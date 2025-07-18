/*
 * f_dm.c - generic USB serial function driver
 * modified from f_serial.c and f_diag.c
 * ttyGS*
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/configfs.h>
#include <linux/usb/composite.h>

#include <linux/configfs.h>
#include "../gadget/function/u_serial.h"

#define MAX_INST_NAME_LEN		40
/*  DM_PORT NUM : /dev/ttyGS* port number */
#define DM_PORT_NUM			3
/*
 * This function packages a simple "generic serial" port with no real
 * control mechanisms, just raw data transfer over two bulk endpoints.
 *
 * Because it's not standardized, this isn't as interoperable as the
 * CDC ACM driver.  However, for many purposes it's just as functional
 * if you can arrange appropriate host side drivers.
 */

struct dm_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
};

struct f_dm {
	struct gserial			port;
	u8				data_id;
	u8				port_num;

	struct dm_descs			fs;
	struct dm_descs			hs;
};

static inline struct f_dm *func_to_dm(struct usb_function *f)
{
	return container_of(f, struct f_dm, port.func);
}

/*-------------------------------------------------------------------------*/

/* interface descriptor: */
static struct usb_interface_descriptor dm_interface_desc  = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	0x10,
	.bInterfaceProtocol =	0x01,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor dm_fs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor dm_fs_out_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *dm_fs_function[]  = {
	(struct usb_descriptor_header *) &dm_interface_desc,
	(struct usb_descriptor_header *) &dm_fs_in_desc,
	(struct usb_descriptor_header *) &dm_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor dm_hs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor dm_hs_out_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};
static struct usb_descriptor_header *dm_hs_function[]  = {
	(struct usb_descriptor_header *) &dm_interface_desc,
	(struct usb_descriptor_header *) &dm_hs_in_desc,
	(struct usb_descriptor_header *) &dm_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor dm_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor dm_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor dm_ss_bulk_comp_desc = {
	.bLength =              sizeof dm_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *dm_ss_function[] = {
	(struct usb_descriptor_header *) &dm_interface_desc,
	(struct usb_descriptor_header *) &dm_ss_in_desc,
	(struct usb_descriptor_header *) &dm_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &dm_ss_out_desc,
	(struct usb_descriptor_header *) &dm_ss_bulk_comp_desc,
	NULL,
};


/* string descriptors: */
#define F_DM_IDX	0

static struct usb_string dm_string_defs[] = {
	[F_DM_IDX].s = "Samsung Android DM",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings dm_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		dm_string_defs,
};

static struct usb_gadget_strings *dm_strings[] = {
	&dm_string_table,
	NULL,
};

struct dm_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct f_dm *dm;
	u8 port_num;
};
/*-------------------------------------------------------------------------*/

static int dm_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_dm		*dm = func_to_dm(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int status;

	/* we know alt == 0, so this is an activation or a reset */

	if (dm->port.in->driver_data) {
		DBG(cdev, "reset generic ttyGS%d\n", dm->port_num);
		gserial_disconnect(&dm->port);
	} else {
		DBG(cdev, "activate generic ttyGS%d\n", dm->port_num);
	}
	if (!dm->port.in->desc || !dm->port.out->desc) {
			DBG(cdev, "activate dm ttyGS%d\n", dm->port_num);
			if (config_ep_by_speed(cdev->gadget, f,
					       dm->port.in) ||
			    config_ep_by_speed(cdev->gadget, f,
					       dm->port.out)) {
				dm->port.in->desc = NULL;
				dm->port.out->desc = NULL;
				return -EINVAL;
			}
	}


	status = gserial_connect(&dm->port, dm->port_num);
	printk(KERN_DEBUG "usb: %s run generic_connect(%d)", __func__,
			dm->port_num);

	if (status < 0) {
		printk(KERN_DEBUG "fail to activate generic ttyGS%d\n",
				dm->port_num);

		return status;
	}

	return 0;
}

static void dm_disable(struct usb_function *f)
{
	struct f_dm	*dm = func_to_dm(f);

	printk(KERN_DEBUG "usb: %s generic ttyGS%d deactivated\n", __func__,
			dm->port_num);
	gserial_disconnect(&dm->port);
}

/*-------------------------------------------------------------------------*/

/* serial function driver setup/binding */

static int
dm_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_dm		*dm = func_to_dm(f);
	int			status;
	struct usb_ep		*ep;

	/* maybe allocate device-global string ID */
	if (dm_string_defs[F_DM_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dm_string_defs[F_DM_IDX].id = status;
	}
	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	dm->data_id = status;
	dm_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &dm_fs_in_desc);
	if (!ep)
		goto fail;
	dm->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &dm_fs_out_desc);
	if (!ep)
		goto fail;
	dm->port.out = ep;
	ep->driver_data = cdev;	/* claim */
	printk(KERN_INFO "[%s]   in =0x%pK , out =0x%pK\n", __func__,
				dm->port.in, dm->port.out);

	/* copy descriptors, and track endpoint copies */
	f->fs_descriptors = usb_copy_descriptors(dm_fs_function);




	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		dm_hs_in_desc.bEndpointAddress =
				dm_fs_in_desc.bEndpointAddress;
		dm_hs_out_desc.bEndpointAddress =
				dm_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(dm_hs_function);
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		dm_ss_in_desc.bEndpointAddress =
			dm_fs_in_desc.bEndpointAddress;
		dm_ss_out_desc.bEndpointAddress =
			dm_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors = usb_copy_descriptors(dm_ss_function);
		if (!f->ss_descriptors)
			goto fail;
	}

	printk("usb: [%s] generic ttyGS%d: %s speed IN/%s OUT/%s\n",
			__func__,
			dm->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			dm->port.in->name, dm->port.out->name);
	return 0;

fail:
	/* we might as well release our claims on endpoints */
	if (dm->port.out)
		dm->port.out->driver_data = NULL;
	if (dm->port.in)
		dm->port.in->driver_data = NULL;

	printk(KERN_ERR "%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
dm_unbind(struct usb_configuration *c, struct usb_function *f)
{
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->fs_descriptors);
	printk(KERN_DEBUG "usb: %s\n", __func__);
}

/*
 * dm_bind_config - add a generic serial function to a configuration
 * @c: the configuration to support the serial instance
 * @port_num: /dev/ttyGS* port this interface will use
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gserial_setup() with enough ports to
 * handle all the ones it binds.  Caller is also responsible
 * for calling @gserial_cleanup() before module unload.
 */
int dm_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct f_dm	*dm;
	int		status;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string ID */
	if (dm_string_defs[F_DM_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dm_string_defs[F_DM_IDX].id = status;
	}

	/* allocate and initialize one new instance */
	dm = kzalloc(sizeof *dm, GFP_KERNEL);
	if (!dm)
		return -ENOMEM;

	dm->port_num = DM_PORT_NUM;

	dm->port.func.name = "dm";
	dm->port.func.strings = dm_strings;
	dm->port.func.bind = dm_bind;
	dm->port.func.unbind = dm_unbind;
	dm->port.func.set_alt = dm_set_alt;
	dm->port.func.disable = dm_disable;

	status = usb_add_function(c, &dm->port.func);
	if (status)
		kfree(dm);
	return status;
}
static struct dm_instance *to_dm_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct dm_instance,
		func_inst.group);
}
static void dm_attr_release(struct config_item *item)
{
	struct dm_instance *fi_dm = to_dm_instance(item);
	usb_put_function_instance(&fi_dm->func_inst);
}

static struct configfs_item_operations dm_item_ops = {
	.release        = dm_attr_release,
};

static struct config_item_type dm_func_type = {
	.ct_item_ops    = &dm_item_ops,
	.ct_owner       = THIS_MODULE,
};


static struct dm_instance *to_fi_dm(struct usb_function_instance *fi)
{
	return container_of(fi, struct dm_instance, func_inst);
}

static int dm_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct dm_instance *fi_dm;
	char *ptr;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_dm = to_fi_dm(fi);
	fi_dm->name = ptr;

	return 0;
}

static void dm_free_inst(struct usb_function_instance *fi)
{
	struct dm_instance *fi_dm;

	fi_dm = to_fi_dm(fi);
	kfree(fi_dm->name);
	kfree(fi_dm);
}

struct usb_function_instance *alloc_inst_dm(bool dm_config)
{
	struct dm_instance *fi_dm;
	int ret;

	fi_dm = kzalloc(sizeof(*fi_dm), GFP_KERNEL);
	if (!fi_dm)
		return ERR_PTR(-ENOMEM);
	fi_dm->func_inst.set_inst_name = dm_set_inst_name;
	fi_dm->func_inst.free_func_inst = dm_free_inst;

	ret = gserial_alloc_line(&fi_dm->port_num);
	if (ret) {
		kfree(fi_dm);
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_dm->func_inst.group,
					"", &dm_func_type);

	return  &fi_dm->func_inst;
}
EXPORT_SYMBOL_GPL(alloc_inst_dm);

static struct usb_function_instance *dm_alloc_inst(void)
{
		return alloc_inst_dm(true);
}

static void dm_free(struct usb_function *f)
{
	struct f_dm	*dm = func_to_dm(f);

	kfree(dm);
}

struct usb_function *function_alloc_dm(struct usb_function_instance *fi, bool dm_config)
{

	struct dm_instance *fi_dm = to_fi_dm(fi);
	struct f_dm	*dm;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* allocate and initialize one new instance */
	dm = kzalloc(sizeof *dm, GFP_KERNEL);
	if (!dm)
		return ERR_PTR(-ENOMEM);


	dm->port_num = fi_dm->port_num;

	dm->port.func.name = "dm";
	dm->port.func.strings = dm_strings;
	dm->port.func.bind = dm_bind;
	dm->port.func.unbind = dm_unbind;
	dm->port.func.set_alt = dm_set_alt;
	dm->port.func.disable = dm_disable;
	dm->port.func.free_func = dm_free;

	fi_dm->dm = dm;

	return &dm->port.func;
}
EXPORT_SYMBOL_GPL(function_alloc_dm);

static struct usb_function *dm_alloc(struct usb_function_instance *fi)
{
	return function_alloc_dm(fi, true);
}

DECLARE_USB_FUNCTION_INIT(dm, dm_alloc_inst, dm_alloc);
MODULE_LICENSE("GPL");
