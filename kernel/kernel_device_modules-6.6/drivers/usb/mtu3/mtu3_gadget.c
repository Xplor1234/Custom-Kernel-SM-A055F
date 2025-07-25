// SPDX-License-Identifier: GPL-2.0
/*
 * mtu3_gadget.c - MediaTek usb3 DRD peripheral support
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <linux/usb/composite.h>

#include "mtu3.h"
#include "mtu3_trace.h"

#include "u_fs.h"

/* workaround for f_fs use after free issue */
struct ffs_ep {
	struct usb_ep *ep;
	struct usb_request *req;
	struct usb_endpoint_descriptor	*descs[3];
	u8 num;
	int status;
};

struct ffs_function {
	struct usb_configuration *conf;
	struct usb_gadget *gadget;
	struct ffs_data *ffs;
	struct ffs_ep *eps;
	u8 eps_revmap[16];
	short *interfaces_nums;
	struct usb_function function;
};

static void mtu3_set_u2_lpm(struct mtu3 *mtu, enum mtu3_u2_lpm_mode mode)
{
	if (mtu->u2_lpm_reject == mode)
		return;

	switch (mode) {
	case MTU3_U2_LPM_REJECT:
		mtu3_setbits(mtu->mac_base, U3D_POWER_MANAGEMENT, LPM_MODE_REJECT);
		mtu3_setbits(mtu->mac_base, U3D_POWER_MANAGEMENT, LPM_HRWE);
		break;
	case MTU3_U2_LPM_ACCEPT:
		mtu3_clrbits(mtu->mac_base, U3D_POWER_MANAGEMENT, LPM_HRWE);
		mtu3_clrbits(mtu->mac_base, U3D_POWER_MANAGEMENT, LPM_MODE_REJECT);
		mtu3_setbits(mtu->mac_base, U3D_USB20_MISC_CONTROL, LPM_U3_ACK_EN);
		break;
	default:
		break; /* others are ignored */
	}

	mtu->u2_lpm_reject = mode;
}

static void mtu3_u2_lpm_timer_func(struct timer_list *t)
{
	struct mtu3 *mtu = from_timer(mtu, t, lpm_timer);
	struct mtu3_ep *mep;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&mtu->lock, flags);

	if (!mtu->is_active)
		goto done;

	/* check tx request status */
	for (i = 1; i < mtu->num_eps; i++) {
		mep = mtu->in_eps + i;
		if (!list_empty(&mep->req_list)) {
			mtu3_gadget_u2_lpm_lock(mtu, U2_LPM_LOCK_TIMEOUT);
			goto done;
		}
	}

	/* bus idle now */
	mtu3_set_u2_lpm(mtu, MTU3_U2_LPM_ACCEPT);
done:
	spin_unlock_irqrestore(&mtu->lock, flags);
}

void mtu3_gadget_u2_lpm_lock(struct mtu3 *mtu, unsigned int timeout_ms)
{
	if (!of_device_is_compatible(mtu->dev->of_node, "mediatek,mt6991-mtu3"))
		return;

	mtu3_set_u2_lpm(mtu, MTU3_U2_LPM_REJECT);
	mod_timer(&mtu->lpm_timer, jiffies + msecs_to_jiffies(timeout_ms));
}

static struct usb_function *mtu3_ep_to_func(struct mtu3_ep *mep)
{
	struct usb_ep *ep = &mep->ep;
	struct usb_composite_dev *cdev;
	struct usb_function *f = NULL;
	int addr;

	cdev = get_gadget_data(&mep->mtu->g);
	if (!cdev || !cdev->config)
		return f;

	if (!mep->epnum)
		return f;

	addr = ((ep->address & 0x80) >> 3)
			| (ep->address & 0x0f);

	list_for_each_entry(f, &cdev->config->functions, list) {
		if (test_bit(addr, f->endpoints))
			break;
	}

	return f;
}

static bool mtu3_ffs_state_valid(struct mtu3_ep *mep)
{
	struct usb_function *f;
	struct ffs_function *func;

	f = mtu3_ep_to_func(mep);

	if (!f || strcmp("Function FS Gadget", f->name))
		return true;

	func = container_of(f, struct ffs_function, function);
	if (!func->ffs)
		return false;

	if (!func->ffs->epfiles || func->ffs->state != FFS_ACTIVE) {
		dev_info(mep->mtu->dev, "%s ffs->state!=active\n", __func__);
		return false;
	}

	return true;
}
/* workaround for f_fs use after free issue */

void mtu3_req_complete(struct mtu3_ep *mep,
		     struct usb_request *req, int status)
__releases(mep->mtu->lock)
__acquires(mep->mtu->lock)
{
	struct mtu3_request *mreq = to_mtu3_request(req);
	struct mtu3 *mtu = mreq->mtu;

	list_del(&mreq->list);
	if (req->status == -EINPROGRESS)
		req->status = status;

	trace_mtu3_req_complete(mreq);

	/* ep0 makes use of PIO, needn't unmap it */
	if (mep->epnum)
		usb_gadget_unmap_request(&mtu->g, req, mep->is_in);

	dev_dbg(mtu->dev, "%s complete req: %p, sts %d, %d/%d\n",
		mep->name, req, req->status, req->actual, req->length);

	spin_unlock(&mtu->lock);
	usb_gadget_giveback_request(&mep->ep, req);
	spin_lock(&mtu->lock);
}

static void nuke(struct mtu3_ep *mep, const int status)
{
	struct mtu3_request *mreq = NULL;

	if (list_empty(&mep->req_list))
		return;

	dev_dbg(mep->mtu->dev, "abort %s's req: sts %d\n", mep->name, status);

	/* exclude EP0 */
	if (mep->epnum)
		mtu3_qmu_flush(mep);

	while (!list_empty(&mep->req_list)) {
		mreq = list_first_entry(&mep->req_list,
					struct mtu3_request, list);
		mtu3_req_complete(mep, &mreq->request, status);
	}
}

static int mtu3_ep_enable(struct mtu3_ep *mep)
{
	const struct usb_endpoint_descriptor *desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc;
	struct mtu3 *mtu = mep->mtu;
	u32 interval = 0;
	u32 mult = 0;
	u32 burst = 0;
	int ret;

	desc = mep->desc;
	comp_desc = mep->comp_desc;
	mep->type = usb_endpoint_type(desc);
	mep->maxp = usb_endpoint_maxp(desc);

	switch (mtu->g.speed) {
	case USB_SPEED_SUPER:
	case USB_SPEED_SUPER_PLUS:
		if (usb_endpoint_xfer_int(desc) ||
				usb_endpoint_xfer_isoc(desc)) {
			interval = desc->bInterval;
			interval = clamp_val(interval, 1, 16);
			if (usb_endpoint_xfer_isoc(desc) && comp_desc)
				mult = comp_desc->bmAttributes;
		}
		if (comp_desc)
			burst = comp_desc->bMaxBurst;

		break;
	case USB_SPEED_HIGH:
		if (usb_endpoint_xfer_isoc(desc) ||
				usb_endpoint_xfer_int(desc)) {
			interval = desc->bInterval;
			interval = clamp_val(interval, 1, 16);
			mult = usb_endpoint_maxp_mult(desc) - 1;
		}
		break;
	case USB_SPEED_FULL:
		if (usb_endpoint_xfer_isoc(desc))
			interval = clamp_val(desc->bInterval, 1, 16);
		else if (usb_endpoint_xfer_int(desc))
			interval = clamp_val(desc->bInterval, 1, 255);

		break;
	default:
		break; /*others are ignored */
	}

	mep->ep.maxpacket = mep->maxp;
	mep->ep.desc = desc;
	mep->ep.comp_desc = comp_desc;

	/* slot mainly affects bulk/isoc transfer, so ignore int */
	mep->slot = usb_endpoint_xfer_int(desc) ? 0 : mtu->slot;

	if (mep->slot) {
		switch (mtu->ep_slot_mode) {
		case MTU3_EP_SLOT_MAX:
			if (mtu->g.speed >= USB_SPEED_SUPER)
				mep->slot = MTU3_U3_IP_SLOT_MAX;
			else if (!mtu->u3_capable)
				mep->slot = MTU3_U2_IP_SLOT_MAX;
			break;
		/* reserve ep slot for multi-ep function */
		case MTU3_EP_SLOT_MIN:
			if (mtu->g.speed >= USB_SPEED_SUPER || !mtu->u3_capable)
				mep->slot = 0;
			break;
		default:
			break;
		}
	}

	dev_info(mtu->dev, "%s %s maxp:%d interval:%d burst:%d slot:%d\n",
		__func__, mep->name, mep->maxp,	interval, burst, mep->slot);

	ret = mtu3_config_ep(mtu, mep, interval, burst, mult);
	if (ret < 0)
		return ret;

	ret = mtu3_gpd_ring_alloc(mep);
	if (ret < 0) {
		mtu3_deconfig_ep(mtu, mep);
		return ret;
	}

	mtu3_qmu_start(mep);

	return 0;
}

static int mtu3_ep_disable(struct mtu3_ep *mep)
{
	struct mtu3 *mtu = mep->mtu;

	/* abort all pending requests */
	nuke(mep, -ESHUTDOWN);
	mtu3_qmu_stop(mep);
	mtu3_deconfig_ep(mtu, mep);
	mtu3_gpd_ring_free(mep);

	mep->desc = NULL;
	mep->ep.desc = NULL;
	mep->comp_desc = NULL;
	mep->type = 0;
	mep->flags = 0;

	return 0;
}

static int mtu3_gadget_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct mtu3_ep *mep;
	struct mtu3 *mtu;
	unsigned long flags;
	int ret = -EINVAL;

	if (!ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT) {
		pr_debug("%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (!desc->wMaxPacketSize) {
		pr_debug("%s missing wMaxPacketSize\n", __func__);
		return -EINVAL;
	}
	mep = to_mtu3_ep(ep);
	mtu = mep->mtu;

	/* check ep number and direction against endpoint */
	if (usb_endpoint_num(desc) != mep->epnum)
		return -EINVAL;

	if (!!usb_endpoint_dir_in(desc) ^ !!mep->is_in)
		return -EINVAL;

	dev_dbg(mtu->dev, "%s %s\n", __func__, ep->name);

	if (mep->flags & MTU3_EP_ENABLED) {
		dev_info_once(mtu->dev, "%s is already enabled\n",
				mep->name);
		return 0;
	}

	spin_lock_irqsave(&mtu->lock, flags);
	mep->desc = desc;
	mep->comp_desc = ep->comp_desc;

	trace_mtu3_gadget_ep_enable(mep);
	ret = mtu3_ep_enable(mep);
	if (ret)
		goto error;

	mep->flags = MTU3_EP_ENABLED;
	mtu->active_ep++;

error:
	spin_unlock_irqrestore(&mtu->lock, flags);

	dev_dbg(mtu->dev, "%s active_ep=%d\n", __func__, mtu->active_ep);

	/* workaround for f_fs use after free issue */
	if (!ret && !mtu3_ffs_state_valid(mep))
		ret = -EINVAL;

	return ret;
}

static int mtu3_gadget_ep_disable(struct usb_ep *ep)
{
	struct mtu3_ep *mep = to_mtu3_ep(ep);
	struct mtu3 *mtu = mep->mtu;
	unsigned long flags;

	dev_dbg(mtu->dev, "%s %s\n", __func__, mep->name);
	trace_mtu3_gadget_ep_disable(mep);

	if (!(mep->flags & MTU3_EP_ENABLED)) {
		dev_warn(mtu->dev, "%s is already disabled\n", mep->name);
		return 0;
	}

	spin_lock_irqsave(&mtu->lock, flags);
	mtu3_ep_disable(mep);
	mep->flags = 0;
	mtu->active_ep--;
	spin_unlock_irqrestore(&(mtu->lock), flags);

	dev_dbg(mtu->dev, "%s active_ep=%d, mtu3 is_active=%d\n",
		__func__, mtu->active_ep, mtu->is_active);

	return 0;
}

struct usb_request *mtu3_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	struct mtu3_ep *mep = to_mtu3_ep(ep);
	struct mtu3_request *mreq;

	mreq = kzalloc(sizeof(*mreq), gfp_flags);
	if (!mreq)
		return NULL;

	mreq->request.dma = DMA_ADDR_INVALID;
	mreq->epnum = mep->epnum;
	mreq->mep = mep;
	INIT_LIST_HEAD(&mreq->list);
	trace_mtu3_alloc_request(mreq);

	return &mreq->request;
}

void mtu3_free_request(struct usb_ep *ep, struct usb_request *req)
{
	struct mtu3_request *mreq = to_mtu3_request(req);
	struct mtu3_request *r = NULL;
	struct mtu3_ep *mep = to_mtu3_ep(ep);
	struct mtu3 *mtu = mep->mtu;
	unsigned long flags;

	spin_lock_irqsave(&mtu->lock, flags);

	if (!mreq || mreq->mep != mep)
		goto error;

	list_for_each_entry(r, &mep->req_list, list) {
		if (r == mreq) {
			list_del(&mreq->list);
			break;
		}
	}
	trace_mtu3_free_request(mreq);
	kfree(mreq);
error:
	spin_unlock_irqrestore(&mtu->lock, flags);
}

static int mtu3_gadget_queue(struct usb_ep *ep,
		struct usb_request *req, gfp_t gfp_flags)
{
	struct mtu3_ep *mep = to_mtu3_ep(ep);
	struct mtu3_request *mreq = to_mtu3_request(req);
	struct mtu3 *mtu = mep->mtu;
	unsigned long flags;
	int ret = 0;
	struct list_head *new = NULL;
	struct list_head *head = NULL;

	if (!req->buf)
		return -ENODATA;

	if (!mreq || mreq->mep != mep)
		return -EINVAL;

	dev_dbg(mtu->dev, "%s %s EP%d(%s), req=%p, maxp=%d, len#%d\n",
		__func__, mep->is_in ? "TX" : "RX", mreq->epnum, ep->name,
		mreq, ep->maxpacket, mreq->request.length);

	if (req->length > GPD_BUF_SIZE ||
	    (mtu->gen2cp && req->length > GPD_BUF_SIZE_EL)) {
		dev_warn(mtu->dev,
			"req length > supported MAX:%d requested:%d\n",
			mtu->gen2cp ? GPD_BUF_SIZE_EL : GPD_BUF_SIZE,
			req->length);
		return -EOPNOTSUPP;
	}

	/* don't queue if the ep is down */
	if (!mep->desc) {
		dev_dbg(mtu->dev, "req=%p queued to %s while it's disabled\n",
			req, ep->name);
		return -ESHUTDOWN;
	}

	mreq->mtu = mtu;
	mreq->request.actual = 0;
	mreq->request.status = -EINPROGRESS;

	ret = usb_gadget_map_request(&mtu->g, req, mep->is_in);
	if (ret) {
		dev_err(mtu->dev, "dma mapping failed\n");
		return ret;
	}

	spin_lock_irqsave(&mtu->lock, flags);

	if (mtu3_prepare_transfer(mep)) {
		usb_gadget_unmap_request(&mtu->g, req, mep->is_in);  // Unmap before returning on error.
		ret = -EAGAIN;
		goto error;
	}

	if (mtu->g.lpm_capable && mtu->g.speed <= USB_SPEED_HIGH) {
		if (mtu->u2_lpm_reject == MTU3_U2_LPM_ACCEPT)
			dev_info(mtu->dev, "%s may need to remote wakeup\n", __func__);
		mtu3_gadget_u2_lpm_lock(mtu, U2_LPM_LOCK_TIMEOUT);
	}

	trace_mtu3_gadget_queue(mreq);
	new = &mreq->list;
	head = &mep->req_list;
	if(new == head->prev) {
		dev_info(mtu->dev, "req double add error,%s %s EP%d(%s), req=%p, maxp=%d, len#%d\n",
			__func__, mep->is_in ? "TX" : "RX", mreq->epnum, ep->name,
			mreq, ep->maxpacket, mreq->request.length);
		ret = -EINVAL;
		goto error;
	}
	list_add_tail(&mreq->list, &mep->req_list);
	mtu3_insert_gpd(mep, mreq);
	mtu3_qmu_resume(mep);

error:
	spin_unlock_irqrestore(&mtu->lock, flags);

	return ret;
}

static int mtu3_gadget_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	struct mtu3_ep *mep = to_mtu3_ep(ep);
	struct mtu3_request *mreq = NULL;
	struct mtu3_request *r = NULL;
	struct mtu3 *mtu = mep->mtu;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&mtu->lock, flags);

	mreq = to_mtu3_request(req);

	if (!mreq) {
		ret = -EINVAL;
		goto done;
	}

	dev_dbg(mtu->dev, "%s : req=%p\n", __func__, req);

	list_for_each_entry(r, &mep->req_list, list) {
		if (r == mreq)
			break;
	}
	if (r != mreq) {
		dev_info(mtu->dev, "req=%p not queued to %s\n", req, ep->name);
		ret = -EINVAL;
		goto done;
	}

	if (mreq->mep != mep) {
		dev_info(mtu->dev, "mreq->mep != mep, not queued.\n");
		ret = -EINVAL;
		goto done;
	}

	trace_mtu3_gadget_dequeue(mreq);

	mtu3_qmu_flush(mep);  /* REVISIT: set BPS ?? */
	mtu3_req_complete(mep, req, -ECONNRESET);
	mtu3_qmu_start(mep);

done:
	spin_unlock_irqrestore(&mtu->lock, flags);

	return ret;
}

/*
 * Set or clear the halt bit of an EP.
 * A halted EP won't TX/RX any data but will queue requests.
 */
static int mtu3_gadget_ep_set_halt(struct usb_ep *ep, int value)
{
	struct mtu3_ep *mep = to_mtu3_ep(ep);
	struct mtu3 *mtu = mep->mtu;
	struct mtu3_request *mreq;
	unsigned long flags;
	int ret = 0;

	dev_dbg(mtu->dev, "%s : %s...", __func__, ep->name);

	spin_lock_irqsave(&mtu->lock, flags);

	if (mep->type == USB_ENDPOINT_XFER_ISOC) {
		ret = -EINVAL;
		goto done;
	}

	mreq = next_request(mep);
	if (value) {
		/*
		 * If there is not request for TX-EP, QMU will not transfer
		 * data to TX-FIFO, so no need check whether TX-FIFO
		 * holds bytes or not here
		 */
		if (mreq) {
			dev_dbg(mtu->dev, "req in progress, cannot halt %s\n",
				ep->name);
			ret = -EAGAIN;
			goto done;
		}
	} else {
		mep->flags &= ~MTU3_EP_WEDGE;
	}

	dev_dbg(mtu->dev, "%s %s stall\n", ep->name, value ? "set" : "clear");

	mtu3_ep_stall_set(mep, value);

done:
	spin_unlock_irqrestore(&mtu->lock, flags);
	trace_mtu3_gadget_ep_set_halt(mep);

	return ret;
}

/* Sets the halt feature with the clear requests ignored */
static int mtu3_gadget_ep_set_wedge(struct usb_ep *ep)
{
	struct mtu3_ep *mep = to_mtu3_ep(ep);

	mep->flags |= MTU3_EP_WEDGE;

	return usb_ep_set_halt(ep);
}

static const struct usb_ep_ops mtu3_ep_ops = {
	.enable = mtu3_gadget_ep_enable,
	.disable = mtu3_gadget_ep_disable,
	.alloc_request = mtu3_alloc_request,
	.free_request = mtu3_free_request,
	.queue = mtu3_gadget_queue,
	.dequeue = mtu3_gadget_dequeue,
	.set_halt = mtu3_gadget_ep_set_halt,
	.set_wedge = mtu3_gadget_ep_set_wedge,
};

static int mtu3_gadget_get_frame(struct usb_gadget *gadget)
{
	struct mtu3 *mtu = gadget_to_mtu3(gadget);

	return (int)mtu3_readl(mtu->mac_base, U3D_USB20_FRAME_NUM);
}

static void function_wake_notif(struct mtu3 *mtu, u8 intf)
{
	mtu3_writel(mtu->mac_base, U3D_DEV_NOTIF_0,
		    TYPE_FUNCTION_WAKE | DEV_NOTIF_VAL_FW(intf));
	mtu3_setbits(mtu->mac_base, U3D_DEV_NOTIF_0, SEND_DEV_NOTIF);
}

static int mtu3_gadget_wakeup(struct usb_gadget *gadget)
{
	struct mtu3 *mtu = gadget_to_mtu3(gadget);
	unsigned long flags;

	dev_dbg(mtu->dev, "%s\n", __func__);

	/* remote wakeup feature is not enabled by host */
	if (!mtu->may_wakeup)
		return  -EOPNOTSUPP;

	spin_lock_irqsave(&mtu->lock, flags);
	if (mtu->g.speed >= USB_SPEED_SUPER) {
		/*
		 * class driver may do function wakeup even UFP is in U0,
		 * and UX_EXIT only takes effect in U1/U2/U3;
		 */
		mtu3_setbits(mtu->mac_base, U3D_LINK_POWER_CONTROL, UX_EXIT);
		/*
		 * Assume there's only one function on the composite device
		 * and enable remote wake for the first interface.
		 * FIXME if the IAD (interface association descriptor) shows
		 * there is more than one function.
		 */
		function_wake_notif(mtu, 0);
	} else {
		mtu3_setbits(mtu->mac_base, U3D_POWER_MANAGEMENT, RESUME);
		spin_unlock_irqrestore(&mtu->lock, flags);
		usleep_range(10000, 11000);
		spin_lock_irqsave(&mtu->lock, flags);
		mtu3_clrbits(mtu->mac_base, U3D_POWER_MANAGEMENT, RESUME);
	}
	spin_unlock_irqrestore(&mtu->lock, flags);
	return 0;
}

static int mtu3_gadget_set_self_powered(struct usb_gadget *gadget,
		int is_selfpowered)
{
	struct mtu3 *mtu = gadget_to_mtu3(gadget);

	mtu->is_self_powered = !!is_selfpowered;
	return 0;
}

static void mtu3_nuke_all_ep(struct mtu3 *mtu)
{
	int i;

	nuke(mtu->ep0, -ESHUTDOWN);
	for (i = 1; i < mtu->num_eps; i++) {
		nuke(mtu->in_eps + i, -ESHUTDOWN);
		nuke(mtu->out_eps + i, -ESHUTDOWN);
	}
}

static int mtu3_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	struct mtu3 *mtu = gadget_to_mtu3(gadget);
	unsigned long flags;

	dev_dbg(mtu->dev, "%s (%s) for %sactive device\n", __func__,
		is_on ? "on" : "off", mtu->is_active ? "" : "in");

	pm_runtime_get_sync(mtu->dev);

	/* we'd rather not pullup unless the device is active. */
	spin_lock_irqsave(&mtu->lock, flags);

	is_on = !!is_on;
	if (!mtu->is_active) {
		/* save it for mtu3_start() to process the request */
		mtu->softconnect = is_on;
	} else if (is_on != mtu->softconnect) {
		mtu->softconnect = is_on;

		if (!is_on)
			mtu3_nuke_all_ep(mtu);

		mtu3_dev_on_off(mtu, is_on);
	}

	spin_unlock_irqrestore(&mtu->lock, flags);

	if (!mtu->is_gadget_ready && is_on)
		mtu->is_gadget_ready = 1;

	pm_runtime_put(mtu->dev);

	return 0;
}

static int mtu3_gadget_start(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	struct mtu3 *mtu = gadget_to_mtu3(gadget);
	unsigned long flags;

	if (mtu->gadget_driver) {
		dev_err(mtu->dev, "%s is already bound to %s\n",
			mtu->g.name, mtu->gadget_driver->driver.name);
		return -EBUSY;
	}

	dev_dbg(mtu->dev, "bind driver %s\n", driver->function);
	pm_runtime_get_sync(mtu->dev);

	spin_lock_irqsave(&mtu->lock, flags);

	mtu->softconnect = 0;
	mtu->gadget_driver = driver;

	if (mtu->ssusb->dr_mode == USB_DR_MODE_PERIPHERAL)
		mtu3_start(mtu);

	spin_unlock_irqrestore(&mtu->lock, flags);
	pm_runtime_put(mtu->dev);

	return 0;
}

static void stop_activity(struct mtu3 *mtu)
{
	struct usb_gadget_driver *driver = mtu->gadget_driver;
	int i;

	/* don't disconnect if it's not connected */
	if (mtu->g.speed == USB_SPEED_UNKNOWN)
		driver = NULL;
	else
		mtu->g.speed = USB_SPEED_UNKNOWN;

	/* deactivate the hardware */
	if (mtu->softconnect) {
		mtu->softconnect = 0;
		mtu3_dev_on_off(mtu, 0);
	}

	/*
	 * killing any outstanding requests will quiesce the driver;
	 * then report disconnect
	 */
	nuke(mtu->ep0, -ESHUTDOWN);
	for (i = 1; i < mtu->num_eps; i++) {
		nuke(mtu->in_eps + i, -ESHUTDOWN);
		nuke(mtu->out_eps + i, -ESHUTDOWN);
	}

	if (driver) {
		spin_unlock(&mtu->lock);
		driver->disconnect(&mtu->g);
		spin_lock(&mtu->lock);
	}
}

static int mtu3_gadget_stop(struct usb_gadget *g)
{
	struct mtu3 *mtu = gadget_to_mtu3(g);
	unsigned long flags;

	dev_dbg(mtu->dev, "%s\n", __func__);

	spin_lock_irqsave(&mtu->lock, flags);

	stop_activity(mtu);
	mtu->gadget_driver = NULL;

	if (mtu->ssusb->dr_mode == USB_DR_MODE_PERIPHERAL)
		mtu3_stop(mtu);

	spin_unlock_irqrestore(&mtu->lock, flags);

	synchronize_irq(mtu->irq);
	return 0;
}

static void
mtu3_gadget_set_speed(struct usb_gadget *g, enum usb_device_speed speed)
{
	struct mtu3 *mtu = gadget_to_mtu3(g);
	unsigned long flags;

	dev_dbg(mtu->dev, "%s %s\n", __func__, usb_speed_string(speed));

	spin_lock_irqsave(&mtu->lock, flags);
	mtu->speed = speed;
	spin_unlock_irqrestore(&mtu->lock, flags);
}

static void mtu3_gadget_async_callbacks(struct usb_gadget *g, bool enable)
{
	struct mtu3 *mtu = gadget_to_mtu3(g);
	unsigned long flags;

	spin_lock_irqsave(&mtu->lock, flags);
	mtu->async_callbacks = enable;
	spin_unlock_irqrestore(&mtu->lock, flags);
}

static const struct usb_gadget_ops mtu3_gadget_ops = {
	.get_frame = mtu3_gadget_get_frame,
	.wakeup = mtu3_gadget_wakeup,
	.set_selfpowered = mtu3_gadget_set_self_powered,
	.pullup = mtu3_gadget_pullup,
	.udc_start = mtu3_gadget_start,
	.udc_stop = mtu3_gadget_stop,
	.udc_set_speed = mtu3_gadget_set_speed,
	.udc_async_callbacks = mtu3_gadget_async_callbacks,
	.vbus_draw = mtu3_gadget_vbus_draw,
};

static void mtu3_state_reset(struct mtu3 *mtu)
{
	mtu->address = 0;
	mtu->ep0_state = MU3D_EP0_STATE_SETUP;
	mtu->may_wakeup = 0;
	mtu->u1_enable = 0;
	mtu->u2_enable = 0;
	mtu->delayed_status = false;
	mtu->test_mode = false;
	mtu->u2_lpm_reject = MTU3_U2_LPM_DEFAULT;
}

static void init_hw_ep(struct mtu3 *mtu, struct mtu3_ep *mep,
		u32 epnum, u32 is_in)
{
	mep->epnum = epnum;
	mep->mtu = mtu;
	mep->is_in = is_in;

	INIT_LIST_HEAD(&mep->req_list);

	sprintf(mep->name, "ep%d%s", epnum,
		!epnum ? "" : (is_in ? "in" : "out"));

	mep->ep.name = mep->name;
	INIT_LIST_HEAD(&mep->ep.ep_list);

	/* initialize maxpacket as SS */
	if (!epnum) {
		usb_ep_set_maxpacket_limit(&mep->ep, 512);
		mep->ep.caps.type_control = true;
		mep->ep.ops = &mtu3_ep0_ops;
		mtu->g.ep0 = &mep->ep;
	} else {
		usb_ep_set_maxpacket_limit(&mep->ep, 1024);
		mep->ep.caps.type_iso = true;
		mep->ep.caps.type_bulk = true;
		mep->ep.caps.type_int = true;
		mep->ep.ops = &mtu3_ep_ops;
		list_add_tail(&mep->ep.ep_list, &mtu->g.ep_list);
	}

	dev_dbg(mtu->dev, "%s, name=%s, maxp=%d\n", __func__, mep->ep.name,
		 mep->ep.maxpacket);

	if (!epnum) {
		mep->ep.caps.dir_in = true;
		mep->ep.caps.dir_out = true;
	} else if (is_in) {
		mep->ep.caps.dir_in = true;
	} else {
		mep->ep.caps.dir_out = true;
	}
}

static void mtu3_gadget_init_eps(struct mtu3 *mtu)
{
	u8 epnum;

	/* initialize endpoint list just once */
	INIT_LIST_HEAD(&(mtu->g.ep_list));

	dev_dbg(mtu->dev, "%s num_eps(1 for a pair of tx&rx ep)=%d\n",
		__func__, mtu->num_eps);

	init_hw_ep(mtu, mtu->ep0, 0, 0);
	for (epnum = 1; epnum < mtu->num_eps; epnum++) {
		init_hw_ep(mtu, mtu->in_eps + epnum, epnum, 1);
		init_hw_ep(mtu, mtu->out_eps + epnum, epnum, 0);
	}
}

int mtu3_gadget_setup(struct mtu3 *mtu)
{
	mtu->g.ops = &mtu3_gadget_ops;
	mtu->g.max_speed = mtu->max_speed;
	mtu->g.speed = USB_SPEED_UNKNOWN;
	mtu->g.sg_supported = 0;
	mtu->g.name = MTU3_DRIVER_NAME;
	mtu->g.irq = mtu->irq;
	mtu->g.lpm_capable = mtu->u3_lpm && (mtu->max_speed > USB_SPEED_HIGH);
	mtu->is_active = 0;
	mtu->delayed_status = false;
	mtu->u2_lpm_reject = MTU3_U2_LPM_DEFAULT;

	timer_setup(&mtu->lpm_timer, mtu3_u2_lpm_timer_func, 0);

	mtu3_gadget_init_eps(mtu);

	return usb_add_gadget_udc(mtu->dev, &mtu->g);
}

void mtu3_gadget_cleanup(struct mtu3 *mtu)
{
	usb_del_gadget_udc(&mtu->g);
}

void mtu3_gadget_resume(struct mtu3 *mtu)
{
	dev_info(mtu->dev, "gadget RESUME\n");
	if (mtu->async_callbacks && mtu->gadget_driver &&
			mtu->gadget_driver->resume) {
		spin_unlock(&mtu->lock);
		mtu->gadget_driver->resume(&mtu->g);
		spin_lock(&mtu->lock);
	}
}

/* called when SOF packets stop for 3+ msec or enters U3 */
void mtu3_gadget_suspend(struct mtu3 *mtu)
{
	dev_info(mtu->dev, "gadget SUSPEND\n");
	if (mtu->async_callbacks && mtu->gadget_driver &&
			mtu->gadget_driver->suspend) {
		spin_unlock(&mtu->lock);
		mtu->gadget_driver->suspend(&mtu->g);
		spin_lock(&mtu->lock);
	}
}

/* called when VBUS drops below session threshold, and in other cases */
void mtu3_gadget_disconnect(struct mtu3 *mtu)
{
	struct usb_gadget_driver *driver;

	dev_dbg(mtu->dev, "gadget DISCONNECT\n");
	if (mtu->async_callbacks && mtu->gadget_driver &&
			mtu->gadget_driver->disconnect) {
		driver = mtu->gadget_driver;
		spin_unlock(&mtu->lock);
		/*
		 * avoid kernel panic because mtu3_gadget_stop() assigned NULL
		 * to mtu->gadget_driver.
		 */
		driver->disconnect(&mtu->g);
		spin_lock(&mtu->lock);
	}

	mtu3_state_reset(mtu);
	usb_gadget_set_state(&mtu->g, USB_STATE_NOTATTACHED);
}

void mtu3_gadget_reset(struct mtu3 *mtu)
{
	dev_info(mtu->dev, "gadget RESET\n");

	/* report disconnect, if we didn't flush EP state */
	if (mtu->g.speed != USB_SPEED_UNKNOWN)
		mtu3_gadget_disconnect(mtu);
	else
		mtu3_state_reset(mtu);
}
