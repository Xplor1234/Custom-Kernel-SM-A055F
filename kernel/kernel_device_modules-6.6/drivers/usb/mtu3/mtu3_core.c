// SPDX-License-Identifier: GPL-2.0
/*
 * mtu3_core.c - hardware access layer and gadget init/exit of
 *                     MediaTek usb3 Dual-Role Controller Driver
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include "mtu3.h"
#include "mtu3_dr.h"
#include "mtu3_debug.h"
#include "mtu3_trace.h"

#include <linux/usb/typec.h>
#include "class.h"

static int ep_fifo_alloc(struct mtu3_ep *mep, u32 seg_size)
{
	struct mtu3_fifo_info *fifo = mep->fifo;
	u32 num_bits = DIV_ROUND_UP(seg_size, MTU3_EP_FIFO_UNIT);
	u32 start_bit;

	/* ensure that @mep->fifo_seg_size is power of two */
	num_bits = roundup_pow_of_two(num_bits);
	if (num_bits > fifo->limit)
		return -EINVAL;

	mep->fifo_seg_size = num_bits * MTU3_EP_FIFO_UNIT;
	num_bits = num_bits * (mep->slot + 1);
	start_bit = bitmap_find_next_zero_area(fifo->bitmap,
			fifo->limit, 0, num_bits, 0);
	if (start_bit >= fifo->limit)
		return -EOVERFLOW;

	bitmap_set(fifo->bitmap, start_bit, num_bits);
	mep->fifo_size = num_bits * MTU3_EP_FIFO_UNIT;
	mep->fifo_addr = fifo->base + MTU3_EP_FIFO_UNIT * start_bit;

	dev_dbg(mep->mtu->dev, "%s fifo:%#x/%#x, start_bit: %d\n",
		__func__, mep->fifo_seg_size, mep->fifo_size, start_bit);

	return mep->fifo_addr;
}

static void ep_fifo_free(struct mtu3_ep *mep)
{
	struct mtu3_fifo_info *fifo = mep->fifo;
	u32 addr = mep->fifo_addr;
	u32 bits = mep->fifo_size / MTU3_EP_FIFO_UNIT;
	u32 start_bit;

	if (unlikely(addr < fifo->base || bits > fifo->limit))
		return;

	start_bit = (addr - fifo->base) / MTU3_EP_FIFO_UNIT;
	bitmap_clear(fifo->bitmap, start_bit, bits);
	mep->fifo_size = 0;
	mep->fifo_seg_size = 0;

	dev_dbg(mep->mtu->dev, "%s size:%#x/%#x, start_bit: %d\n",
		__func__, mep->fifo_seg_size, mep->fifo_size, start_bit);
}

static void mtu3_vbus_draw_work(struct work_struct *data)
{
	struct mtu3 *mtu = container_of(data, struct mtu3, draw_work);
	union power_supply_propval val;
	int ret;

	val.intval = mtu->is_active &&	!(mtu->vbus_draw > USB_SELF_POWER_VBUS_MAX_DRAW);

	if (mtu->is_power_limit != val.intval) {
		ret = power_supply_set_property(mtu->usb_psy,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
		if (!ret)
			mtu->is_power_limit = val.intval;
		else
			dev_info(mtu->dev, "%s set property error:%d\n", __func__, ret);
	}

	dev_info(mtu->dev, "%s %d mA, is_limit %d\n",
		__func__, mtu->vbus_draw, mtu->is_power_limit);
}

int mtu3_gadget_vbus_draw(struct usb_gadget *g, unsigned int mA)
{
	struct mtu3 *mtu = gadget_to_mtu3(g);

	if (!mtu->usb_psy_name)
		goto skip;

	if (!mtu->usb_psy) {
		mtu->usb_psy = power_supply_get_by_name(mtu->usb_psy_name);
		if (!mtu->usb_psy) {
			dev_info(mtu->dev, "couldn't get usb power supply\n");
			goto skip;
		}
	}

	mtu->vbus_draw = mA;
	queue_work(system_power_efficient_wq, &mtu->draw_work);

	return 0;
skip:
	return -EOPNOTSUPP;
}

static struct typec_port *mtu3_get_typec_port(struct mtu3 *mtu)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct device *child;
	struct typec_port *port = NULL;

	np = of_find_node_by_name(NULL, mtu->typec_name);
	if (!np)
		return NULL;

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return NULL;

	child = device_find_child_by_name(&pdev->dev, mtu->typec_port_name);
	if (!child)
		return NULL;

	if (!strcmp(child->type->name, "typec_port"))
		port = to_typec_port(child);

	put_device(child);

	return port;
}

int mtu3_is_usb_pd(struct mtu3 *mtu)
{
	int usb_pd = -EOPNOTSUPP;

	if (!mtu->typec_name || !mtu->typec_port_name)
		goto skip;

	if (!mtu->typec_port) {
		mtu->typec_port = mtu3_get_typec_port(mtu);
		if (!mtu->typec_port) {
			dev_info(mtu->dev, "couldn't get typec port\n");
			goto skip;
		}
	}

	usb_pd = (mtu->typec_port->pwr_opmode == TYPEC_PWR_MODE_PD) ? 1 : 0;
skip:
	return usb_pd;
}

static void mtu3_ss_u1u2_set(struct mtu3 *mtu)
{
	/* enable or disable accept LGO_U1/U2 link command from host */
	if (mtu->u3_lpm) {
		mtu3_setbits(mtu->mac_base, U3D_LINK_POWER_CONTROL,
			SW_U1_ACCEPT_ENABLE | SW_U2_ACCEPT_ENABLE);
	} else {
		mtu3_clrbits(mtu->mac_base, U3D_LINK_POWER_CONTROL,
			SW_U1_ACCEPT_ENABLE | SW_U2_ACCEPT_ENABLE);
		dev_info(mtu->dev, "disable accept_lgo\n");
	}

	/* enable or disable U1 to U2 transition */
	if (mtu->u3_u1gou2)
		mtu3_setbits(mtu->mac_base, U3D_LTSSM_CTRL, U1_GO_U2_EN);
	else
		mtu3_clrbits(mtu->mac_base, U3D_LTSSM_CTRL, U1_GO_U2_EN);
}

/* enable/disable U3D SS function */
static inline void mtu3_ss_func_set(struct mtu3 *mtu, bool enable)
{
	/* update ss u1/u2 setting */
	mtu3_ss_u1u2_set(mtu);

	/* If usb3_en==0, LTSSM will go to SS.Disable state */
	if (enable) {
		/* A60931 new DTB has true type-C connector.
		 * Phy sends vbus_present and starts to swap lane.
		 * It will take about 120ms to swap lane if needed.
		 * Device shall wait lane swap and then enable U3 terminator.
		 */
		if (mtu->ssusb->fpga_phy == A60931_USB_PHY)
			mdelay(180);

		mtu3_setbits(mtu->mac_base, U3D_USB3_CONFIG, USB3_EN);
	} else
		mtu3_clrbits(mtu->mac_base, U3D_USB3_CONFIG, USB3_EN);

	dev_dbg(mtu->dev, "USB3_EN = %d\n", !!enable);
}

/* set/clear U3D HS device soft connect */
static inline void mtu3_hs_softconn_set(struct mtu3 *mtu, bool enable)
{
	if (enable) {
		mtu3_setbits(mtu->mac_base, U3D_POWER_MANAGEMENT,
			SOFT_CONN | SUSPENDM_ENABLE);
	} else {
		mtu3_clrbits(mtu->mac_base, U3D_POWER_MANAGEMENT,
			SOFT_CONN | SUSPENDM_ENABLE);
	}
	dev_dbg(mtu->dev, "SOFTCONN = %d\n", !!enable);
}

/* only port0 of U2/U3 supports device mode */
int mtu3_device_enable(struct mtu3 *mtu)
{
	void __iomem *ibase = mtu->ippc_base;
	struct ssusb_mtk *ssusb = mtu->ssusb;
	u32 check_clk = 0;
	u32 u2_ctrl = 0;

	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);

	if (mtu->u3_capable) {
		check_clk = SSUSB_U3_MAC_RST_B_STS;
		mtu3_clrbits(ibase, SSUSB_U3_CTRL(0),
			(SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN |
			SSUSB_U3_PORT_HOST_SEL));
	}

	u2_ctrl = SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN;
	u2_ctrl |= ssusb->keep_ao ? SSUSB_U2_PORT_HOST : SSUSB_U2_PORT_HOST_SEL;
	mtu3_clrbits(ibase, SSUSB_U2_CTRL(0), u2_ctrl);

	if (mtu->ssusb->dr_mode == USB_DR_MODE_OTG) {
		mtu3_setbits(ibase, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_OTG_SEL);
		if (mtu->u3_capable)
			mtu3_setbits(ibase, SSUSB_U3_CTRL(0),
				     SSUSB_U3_PORT_DUAL_MODE);
	}

	/* Switch UTMI data bus to 16 bit. */
	if (mtu->ssusb->fpga_phy == A60931_USB_PHY) {
		dev_dbg(mtu->dev, "MTU3 FPGA Platform, switch UTMI mode.\n");
		mtu3_setbits(ibase, U3D_SSUSB_SYS_CK_CTRL,
				SSUSB_U2_UTMI_DATABUS_16_8);
	}

	if (mtu->ssusb->utmi_width == 16) {
		mtu3_setbits(ibase, U3D_SSUSB_SYS_CK_CTRL, SSUSB_U2_UTMI_DATABUS_16_8);
		if (SSUSB_IP_XHCI_U2_PORT_NUM(
		    mtu3_readl(ibase, U3D_SSUSB_IP_XHCI_CAP)) == 2)
			mtu3_setbits(ibase, U3D_SSUSB_SYS_CK_CTRL,
					SSUSB_U2_UTMI_DATABUS_16_8_1P);
		dev_info(mtu->dev, "U3D_SSUSB_SYS_CK_CTRL - value:0x%x\n",
			mtu3_readl(ibase, U3D_SSUSB_SYS_CK_CTRL));
	}

	return ssusb_check_clocks(mtu->ssusb, check_clk);
}

void mtu3_device_disable(struct mtu3 *mtu)
{
	void __iomem *ibase = mtu->ippc_base;

	if (mtu->u3_capable)
		mtu3_setbits(ibase, SSUSB_U3_CTRL(0),
			(SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN));

	mtu3_setbits(ibase, SSUSB_U2_CTRL(0),
		SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN);

	if (mtu->ssusb->dr_mode == USB_DR_MODE_OTG) {
		mtu3_clrbits(ibase, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_OTG_SEL);
		if (mtu->u3_capable)
			mtu3_clrbits(ibase, SSUSB_U3_CTRL(0),
				     SSUSB_U3_PORT_DUAL_MODE);
	}

	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
}

static void mtu3_dev_power_on(struct mtu3 *mtu)
{
	void __iomem *ibase = mtu->ippc_base;

	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	if (mtu->u3_capable)
		mtu3_clrbits(ibase, SSUSB_U3_CTRL(0), SSUSB_U3_PORT_PDN);

	mtu3_clrbits(ibase, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_PDN);
}

static void mtu3_dev_power_down(struct mtu3 *mtu)
{
	void __iomem *ibase = mtu->ippc_base;

	if (mtu->u3_capable)
		mtu3_setbits(ibase, SSUSB_U3_CTRL(0), SSUSB_U3_PORT_PDN);

	mtu3_setbits(ibase, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_PDN);
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
}

/* reset U3D's device module. */
static void mtu3_device_reset(struct mtu3 *mtu)
{
	void __iomem *ibase = mtu->ippc_base;

	mtu3_setbits(ibase, U3D_SSUSB_DEV_RST_CTRL, SSUSB_DEV_SW_RST);
	udelay(1);
	mtu3_clrbits(ibase, U3D_SSUSB_DEV_RST_CTRL, SSUSB_DEV_SW_RST);
}

static void mtu3_intr_status_clear(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;

	/* Clear EP0 and Tx/Rx EPn interrupts status */
	mtu3_writel(mbase, U3D_EPISR, ~0x0);
	/* Clear U2 USB common interrupts status */
	mtu3_writel(mbase, U3D_COMMON_USB_INTR, ~0x0);
	/* Clear U3 LTSSM interrupts status */
	mtu3_writel(mbase, U3D_LTSSM_INTR, ~0x0);
	/* Clear speed change interrupt status */
	mtu3_writel(mbase, U3D_DEV_LINK_INTR, ~0x0);
	/* Clear QMU interrupt status */
	mtu3_writel(mbase, U3D_QISAR0, ~0x0);
}

/* disable all interrupts */
static void mtu3_intr_disable(struct mtu3 *mtu)
{
	/* Disable level 1 interrupts */
	mtu3_writel(mtu->mac_base, U3D_LV1IECR, ~0x0);
	/* Disable endpoint interrupts */
	mtu3_writel(mtu->mac_base, U3D_EPIECR, ~0x0);
	mtu3_intr_status_clear(mtu);
}

/* enable system global interrupt */
static void mtu3_intr_enable(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;
	u32 value;

	/*Enable level 1 interrupts (BMU, QMU, MAC3, DMA, MAC2, EPCTL) */
	value = BMU_INTR | QMU_INTR | MAC3_INTR | MAC2_INTR | EP_CTRL_INTR;
	mtu3_writel(mbase, U3D_LV1IESR, value);

	/* Enable U2 common USB interrupts */
	value = SUSPEND_INTR | RESUME_INTR | RESET_INTR | LPM_RESUME_INTR;
	mtu3_writel(mbase, U3D_COMMON_USB_INTR_ENABLE, value);

	if (mtu->u3_capable) {
		/* Enable U3 LTSSM interrupts */
		value = HOT_RST_INTR | WARM_RST_INTR |
			ENTER_U3_INTR | EXIT_U3_INTR;
		mtu3_writel(mbase, U3D_LTSSM_INTR_ENABLE, value);
	}

	/* Enable QMU interrupts. */
	value = TXQ_CSERR_INT | TXQ_LENERR_INT | RXQ_CSERR_INT |
			RXQ_LENERR_INT | RXQ_ZLPERR_INT;
	mtu3_writel(mbase, U3D_QIESR1, value);

	/* Enable speed change interrupt */
	mtu3_writel(mbase, U3D_DEV_LINK_INTR_ENABLE, SSUSB_DEV_SPEED_CHG_INTR);
}

void mtu3_set_speed(struct mtu3 *mtu, enum usb_device_speed speed)
{
	void __iomem *mbase = mtu->mac_base;

	if (speed > mtu->max_speed && !mtu->ssusb->is_host)
		speed = mtu->max_speed;

	switch (speed) {
	case USB_SPEED_FULL:
		/* disable U3 SS function */
		mtu3_clrbits(mbase, U3D_USB3_CONFIG, USB3_EN);
		/* disable HS function */
		mtu3_clrbits(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
		mtu3_clrbits(mtu->ippc_base, SSUSB_U3_CTRL(0),
			     SSUSB_U3_PORT_SSP_SPEED);
		break;
	case USB_SPEED_HIGH:
		mtu3_clrbits(mbase, U3D_USB3_CONFIG, USB3_EN);
		/* HS/FS detected by HW */
		mtu3_setbits(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
		mtu3_clrbits(mtu->ippc_base, SSUSB_U3_CTRL(0),
			     SSUSB_U3_PORT_SSP_SPEED);
		break;
	case USB_SPEED_SUPER:
		mtu3_setbits(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
		mtu3_clrbits(mtu->ippc_base, SSUSB_U3_CTRL(0),
			     SSUSB_U3_PORT_SSP_SPEED);
		break;
	case USB_SPEED_SUPER_PLUS:
		mtu3_setbits(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
		mtu3_setbits(mtu->ippc_base, SSUSB_U3_CTRL(0),
			     SSUSB_U3_PORT_SSP_SPEED);
		break;
	default:
		dev_err(mtu->dev, "invalid speed: %s\n",
			usb_speed_string(speed));
		return;
	}

	mtu->speed = speed;
	dev_info(mtu->dev, "set speed: %s\n", usb_speed_string(speed));
}

/* CSR registers will be reset to default value if port is disabled */
static void mtu3_csr_init(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;

	if (mtu->u3_capable) {
		/* disable LGO_U1/U2 by default */
		mtu3_clrbits(mbase, U3D_LINK_POWER_CONTROL,
				SW_U1_REQUEST_ENABLE | SW_U2_REQUEST_ENABLE);
		/* update ss u1/u2 setting */
		mtu3_ss_u1u2_set(mtu);
		mtu3_setbits(mbase, U3D_MAC_U1_EN_CTRL,
			ACCEPT_BMU_RX_EMPTY_HCK);
		mtu3_setbits(mbase, U3D_MAC_U2_EN_CTRL,
			ACCEPT_BMU_RX_EMPTY_HCK);
		/* device responses to u3_exit from host automatically */
		mtu3_clrbits(mbase, U3D_LTSSM_CTRL, SOFT_U3_EXIT_EN);
		/* automatically build U2 link when U3 detect fail */
		mtu3_setbits(mbase, U3D_USB2_TEST_MODE, U2U3_AUTO_SWITCH);
		/* auto clear SOFT_CONN when clear USB3_EN if work as HS */
		mtu3_setbits(mbase, U3D_U3U2_SWITCH_CTRL, SOFTCON_CLR_AUTO_EN);
	}

	/* delay about 0.1us from detecting reset to send chirp-K */
	mtu3_clrbits(mbase, U3D_LINK_RESET_INFO, WTCHRP_MSK);
	/* enable automatical HWRW from L1 */
	mtu3_setbits(mbase, U3D_POWER_MANAGEMENT, LPM_HRWE);
	mtu3_writel(mbase, U3D_USB2_EPCTL_LPM, L1_EXIT_EP0_CHK);
	mtu3_writel(mbase, U3D_USB2_EPCTL_LPM_FC_CHK, 0);
}

/* reset: u2 - data toggle, u3 - SeqN, flow control status etc */
static void mtu3_ep_reset(struct mtu3_ep *mep)
{
	struct mtu3 *mtu = mep->mtu;
	u32 rst_bit = EP_RST(mep->is_in, mep->epnum);

	mtu3_setbits(mtu->mac_base, U3D_EP_RST, rst_bit);
	mtu3_clrbits(mtu->mac_base, U3D_EP_RST, rst_bit);
}

/* set/clear the stall and toggle bits for non-ep0 */
void mtu3_ep_stall_set(struct mtu3_ep *mep, bool set)
{
	struct mtu3 *mtu = mep->mtu;
	void __iomem *mbase = mtu->mac_base;
	u8 epnum = mep->epnum;
	u32 csr;

	if (mep->is_in) {	/* TX */
		csr = mtu3_readl(mbase, MU3D_EP_TXCR0(epnum)) & TX_W1C_BITS;
		if (set)
			csr |= TX_SENDSTALL;
		else
			csr = (csr & (~TX_SENDSTALL)) | TX_SENTSTALL;
		mtu3_writel(mbase, MU3D_EP_TXCR0(epnum), csr);
	} else {	/* RX */
		csr = mtu3_readl(mbase, MU3D_EP_RXCR0(epnum)) & RX_W1C_BITS;
		if (set)
			csr |= RX_SENDSTALL;
		else
			csr = (csr & (~RX_SENDSTALL)) | RX_SENTSTALL;
		mtu3_writel(mbase, MU3D_EP_RXCR0(epnum), csr);
	}

	if (!set) {
		mtu3_qmu_stop(mep);
		mtu3_ep_reset(mep);
		mep->flags &= ~MTU3_EP_STALL;
		mtu3_qmu_resume(mep);
	} else {
		mep->flags |= MTU3_EP_STALL;
	}

	dev_dbg(mtu->dev, "%s: %s\n", mep->name,
		set ? "SEND STALL" : "CLEAR STALL, with EP RESET");
}

void mtu3_dev_on_off(struct mtu3 *mtu, int is_on)
{
	if (is_on) {
		if (mtu->u3_capable && mtu->speed >= USB_SPEED_SUPER)
			mtu3_ss_func_set(mtu, true);
		else {
			mtu3_ss_func_set(mtu, false);
			mtu3_hs_softconn_set(mtu, true);
		}
	} else {
		mtu3_ss_func_set(mtu, false);
		mtu3_hs_softconn_set(mtu, false);
	}

	dev_info(mtu->dev, "gadget (%s) pullup D%s\n",
		usb_speed_string(mtu->speed), is_on ? "+" : "-");
}

static void mtu3_regs_init(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;

	/* be sure interrupts are disabled before registration of ISR */
	mtu3_intr_disable(mtu);

	mtu3_csr_init(mtu);

	/* U2/U3 detected by HW */
	mtu3_writel(mbase, U3D_DEVICE_CONF, 0);
	/* vbus detected by HW */
	mtu3_clrbits(mbase, U3D_MISC_CTRL, VBUS_FRC_EN | VBUS_ON);
	/* use new QMU format when HW version >= 0x1003 */
	if (mtu->gen2cp)
		mtu3_writel(mbase, U3D_QFCR, ~0x0);

	/* update txdeemph */
	ssusb_set_txdeemph(mtu->ssusb);

	/* update ux exit lfps */
	ssusb_set_ux_exit_lfps(mtu->ssusb);

	/* update polling scdlfps time */
	ssusb_set_polling_scdlfps_time(mtu->ssusb);
}

void mtu3_check_params(struct mtu3 *mtu)
{
	/* device's u3 port (port0) is disabled */
	if (mtu->u3_capable && (mtu->ssusb->u3p_dis_msk & BIT(0)))
		mtu->u3_capable = 0;

	/* check the max_speed parameter */
	switch (mtu->max_speed) {
	case USB_SPEED_FULL:
	case USB_SPEED_HIGH:
	case USB_SPEED_SUPER:
	case USB_SPEED_SUPER_PLUS:
		break;
	default:
		dev_err(mtu->dev, "invalid max_speed: %s\n",
			usb_speed_string(mtu->max_speed));
		fallthrough;
	case USB_SPEED_UNKNOWN:
		/* default as SSP */
		mtu->max_speed = USB_SPEED_SUPER_PLUS;
		break;
	}

	if (!mtu->u3_capable && (mtu->max_speed > USB_SPEED_HIGH))
		mtu->max_speed = USB_SPEED_HIGH;

	mtu->speed = mtu->max_speed;

	dev_info(mtu->dev, "max_speed: %s\n",
		 usb_speed_string(mtu->max_speed));
}

void mtu3_start(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;

	dev_dbg(mtu->dev, "%s devctl 0x%x\n", __func__,
		mtu3_readl(mbase, U3D_DEVICE_CONTROL));

	mtu3_clrbits(mtu->ippc_base, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	if (mtu->u3_capable)
		mtu3_clrbits(mtu->ippc_base, SSUSB_U3_CTRL(0), SSUSB_U3_PORT_PDN);

	mtu3_clrbits(mtu->ippc_base, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_PDN);

	mtu3_regs_init(mtu);
	ssusb_set_force_vbus(mtu->ssusb, true);
	mtu3_check_params(mtu);
	mtu3_set_speed(mtu, mtu->speed);

	/* Initialize the default interrupts */
	mtu3_intr_enable(mtu);
	mtu->is_active = 1;

	if (mtu->softconnect)
		mtu3_dev_on_off(mtu, 1);
	else if (!mtu->is_gadget_ready)
		ssusb_phy_dp_pullup(mtu->ssusb);

	/* set vbus limit*/
	mtu3_gadget_vbus_draw(&mtu->g, USB_SELF_POWER_VBUS_MAX_DRAW);

	ssusb_toggle_vbus(mtu->ssusb);
}

void mtu3_stop(struct mtu3 *mtu)
{
	dev_dbg(mtu->dev, "%s\n", __func__);

	mtu3_intr_disable(mtu);

	if (mtu->softconnect)
		mtu3_dev_on_off(mtu, 0);

	mtu->is_active = 0;
	mtu3_dev_power_down(mtu);
}

static void mtu3_dev_suspend(struct mtu3 *mtu)
{
	if (!mtu->is_active)
		return;

	mtu3_intr_disable(mtu);
	mtu3_dev_power_down(mtu);
}

static void mtu3_dev_resume(struct mtu3 *mtu)
{
	if (!mtu->is_active)
		return;

	mtu3_dev_power_on(mtu);
	mtu3_intr_enable(mtu);
}

/* for non-ep0 */
int mtu3_config_ep(struct mtu3 *mtu, struct mtu3_ep *mep,
			int interval, int burst, int mult)
{
	void __iomem *mbase = mtu->mac_base;
	bool gen2cp = mtu->gen2cp;
	int epnum = mep->epnum;
	u32 csr0, csr1, csr2;
	int fifo_sgsz, fifo_addr;
	int num_pkts;

	fifo_addr = ep_fifo_alloc(mep, mep->maxp);
	if (fifo_addr < 0) {
		dev_err(mtu->dev, "alloc ep fifo failed(%d)\n", mep->maxp);
		return -ENOMEM;
	}
	fifo_sgsz = ilog2(mep->fifo_seg_size);
	dev_dbg(mtu->dev, "%s fifosz: %x(%x/%x)\n", __func__, fifo_sgsz,
		mep->fifo_seg_size, mep->fifo_size);

	if (mep->is_in) {
		csr0 = TX_TXMAXPKTSZ(mep->maxp);
		csr0 |= TX_DMAREQEN;

		num_pkts = (burst + 1) * (mult + 1) - 1;
		csr1 = TX_SS_BURST(burst) | TX_SLOT(mep->slot);
		csr1 |= TX_MAX_PKT(gen2cp, num_pkts) | TX_MULT(gen2cp, mult);

		csr2 = TX_FIFOADDR(fifo_addr >> 4);
		csr2 |= TX_FIFOSEGSIZE(fifo_sgsz);

		switch (mep->type) {
		case USB_ENDPOINT_XFER_BULK:
			csr1 |= TX_TYPE(TYPE_BULK);
			break;
		case USB_ENDPOINT_XFER_ISOC:
			csr1 |= TX_TYPE(TYPE_ISO);
			csr2 |= TX_BINTERVAL(interval);
			break;
		case USB_ENDPOINT_XFER_INT:
			csr1 |= TX_TYPE(TYPE_INT);
			csr2 |= TX_BINTERVAL(interval);
			break;
		}

		/* Enable QMU Done interrupt */
		mtu3_setbits(mbase, U3D_QIESR0, QMU_TX_DONE_INT(epnum));

		mtu3_writel(mbase, MU3D_EP_TXCR0(epnum), csr0);
		mtu3_writel(mbase, MU3D_EP_TXCR1(epnum), csr1);
		mtu3_writel(mbase, MU3D_EP_TXCR2(epnum), csr2);

		dev_dbg(mtu->dev, "U3D_TX%d CSR0:%#x, CSR1:%#x, CSR2:%#x\n",
			epnum, mtu3_readl(mbase, MU3D_EP_TXCR0(epnum)),
			mtu3_readl(mbase, MU3D_EP_TXCR1(epnum)),
			mtu3_readl(mbase, MU3D_EP_TXCR2(epnum)));
	} else {
		csr0 = RX_RXMAXPKTSZ(mep->maxp);
		csr0 |= RX_DMAREQEN;

		num_pkts = (burst + 1) * (mult + 1) - 1;
		csr1 = RX_SS_BURST(burst) | RX_SLOT(mep->slot);
		csr1 |= RX_MAX_PKT(gen2cp, num_pkts) | RX_MULT(gen2cp, mult);

		csr2 = RX_FIFOADDR(fifo_addr >> 4);
		csr2 |= RX_FIFOSEGSIZE(fifo_sgsz);

		switch (mep->type) {
		case USB_ENDPOINT_XFER_BULK:
			csr1 |= RX_TYPE(TYPE_BULK);
			break;
		case USB_ENDPOINT_XFER_ISOC:
			csr1 |= RX_TYPE(TYPE_ISO);
			csr2 |= RX_BINTERVAL(interval);
			break;
		case USB_ENDPOINT_XFER_INT:
			csr1 |= RX_TYPE(TYPE_INT);
			csr2 |= RX_BINTERVAL(interval);
			break;
		}

		/*Enable QMU Done interrupt */
		mtu3_setbits(mbase, U3D_QIESR0, QMU_RX_DONE_INT(epnum));

		mtu3_writel(mbase, MU3D_EP_RXCR0(epnum), csr0);
		mtu3_writel(mbase, MU3D_EP_RXCR1(epnum), csr1);
		mtu3_writel(mbase, MU3D_EP_RXCR2(epnum), csr2);

		dev_dbg(mtu->dev, "U3D_RX%d CSR0:%#x, CSR1:%#x, CSR2:%#x\n",
			epnum, mtu3_readl(mbase, MU3D_EP_RXCR0(epnum)),
			mtu3_readl(mbase, MU3D_EP_RXCR1(epnum)),
			mtu3_readl(mbase, MU3D_EP_RXCR2(epnum)));
	}

	/* L1 Exit Check Enable except ISOC OUT EP*/
	if (!((mep->type == USB_ENDPOINT_XFER_ISOC) && !(mep->is_in)))
		mtu3_setbits(mbase, U3D_USB2_EPCTL_LPM,
				L1_EXIT_EP_CHK(mep->is_in, epnum));

	/* RX initiate L1 exit only when latest transaction is flow controlled */
	if (!(mep->is_in))
		mtu3_setbits(mbase, U3D_USB2_EPCTL_LPM_FC_CHK,
				L1_EXIT_EP_FC_CHK(mep->is_in, epnum));

	dev_dbg(mtu->dev, "csr0:%#x, csr1:%#x, csr2:%#x\n", csr0, csr1, csr2);
	dev_dbg(mtu->dev, "%s: %s, fifo-addr:%#x, fifo-size:%#x(%#x/%#x)\n",
		__func__, mep->name, mep->fifo_addr, mep->fifo_size,
		fifo_sgsz, mep->fifo_seg_size);

	return 0;
}

/* for non-ep0 */
void mtu3_deconfig_ep(struct mtu3 *mtu, struct mtu3_ep *mep)
{
	void __iomem *mbase = mtu->mac_base;
	int epnum = mep->epnum;

	if (mep->is_in) {
		mtu3_writel(mbase, MU3D_EP_TXCR0(epnum), 0);
		mtu3_writel(mbase, MU3D_EP_TXCR1(epnum), 0);
		mtu3_writel(mbase, MU3D_EP_TXCR2(epnum), 0);
		mtu3_setbits(mbase, U3D_QIECR0, QMU_TX_DONE_INT(epnum));
	} else {
		mtu3_writel(mbase, MU3D_EP_RXCR0(epnum), 0);
		mtu3_writel(mbase, MU3D_EP_RXCR1(epnum), 0);
		mtu3_writel(mbase, MU3D_EP_RXCR2(epnum), 0);
		mtu3_setbits(mbase, U3D_QIECR0, QMU_RX_DONE_INT(epnum));
	}

	mtu3_ep_reset(mep);
	ep_fifo_free(mep);

	dev_dbg(mtu->dev, "%s: %s\n", __func__, mep->name);
}

/*
 * Two scenarios:
 * 1. when device IP supports SS, the fifo of EP0, TX EPs, RX EPs
 *	are separated;
 * 2. when supports only HS, the fifo is shared for all EPs, and
 *	the capability registers of @EPNTXFFSZ or @EPNRXFFSZ indicate
 *	the total fifo size of non-ep0, and ep0's is fixed to 64B,
 *	so the total fifo size is 64B + @EPNTXFFSZ;
 *	Due to the first 64B should be reserved for EP0, non-ep0's fifo
 *	starts from offset 64 and are divided into two equal parts for
 *	TX or RX EPs for simplification.
 */
static void get_ep_fifo_config(struct mtu3 *mtu)
{
	struct mtu3_fifo_info *tx_fifo;
	struct mtu3_fifo_info *rx_fifo;
	u32 fifosize;

	if (mtu->separate_fifo) {
		fifosize = mtu3_readl(mtu->mac_base, U3D_CAP_EPNTXFFSZ);
		tx_fifo = &mtu->tx_fifo;
		tx_fifo->base = 0;
		tx_fifo->limit = fifosize / MTU3_EP_FIFO_UNIT;
		bitmap_zero(tx_fifo->bitmap, MTU3_FIFO_BIT_SIZE);

		fifosize = mtu3_readl(mtu->mac_base, U3D_CAP_EPNRXFFSZ);
		rx_fifo = &mtu->rx_fifo;
		rx_fifo->base = 0;
		rx_fifo->limit = fifosize / MTU3_EP_FIFO_UNIT;
		bitmap_zero(rx_fifo->bitmap, MTU3_FIFO_BIT_SIZE);
		mtu->slot = MTU3_U3_IP_SLOT_DEFAULT;
	} else {
		fifosize = mtu3_readl(mtu->mac_base, U3D_CAP_EPNTXFFSZ);
		tx_fifo = &mtu->tx_fifo;
		tx_fifo->base = MTU3_U2_IP_EP0_FIFO_SIZE;
		tx_fifo->limit = (fifosize / MTU3_EP_FIFO_UNIT) >> 1;
		bitmap_zero(tx_fifo->bitmap, MTU3_FIFO_BIT_SIZE);

		rx_fifo = &mtu->rx_fifo;
		rx_fifo->base =
			tx_fifo->base + tx_fifo->limit * MTU3_EP_FIFO_UNIT;
		rx_fifo->limit = tx_fifo->limit;
		bitmap_zero(rx_fifo->bitmap, MTU3_FIFO_BIT_SIZE);
		mtu->slot = MTU3_U2_IP_SLOT_DEFAULT;
	}

	dev_dbg(mtu->dev, "%s, TX: base-%d, limit-%d; RX: base-%d, limit-%d\n",
		__func__, tx_fifo->base, tx_fifo->limit,
		rx_fifo->base, rx_fifo->limit);
}

static void mtu3_ep0_setup(struct mtu3 *mtu)
{
	u32 maxpacket = mtu->g.ep0->maxpacket;
	u32 csr;

	dev_dbg(mtu->dev, "%s maxpacket: %d\n", __func__, maxpacket);

	csr = mtu3_readl(mtu->mac_base, U3D_EP0CSR);
	csr &= ~EP0_MAXPKTSZ_MSK;
	csr |= EP0_MAXPKTSZ(maxpacket);
	csr &= EP0_W1C_BITS;
	mtu3_writel(mtu->mac_base, U3D_EP0CSR, csr);

	/* Enable EP0 interrupt */
	mtu3_writel(mtu->mac_base, U3D_EPIESR, EP0ISR | SETUPENDISR);
}

static int mtu3_mem_alloc(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;
	struct mtu3_ep *ep_array;
	int in_ep_num, out_ep_num;
	u32 cap_epinfo;
	int ret;
	int i;

	cap_epinfo = mtu3_readl(mbase, U3D_CAP_EPINFO);
	in_ep_num = CAP_TX_EP_NUM(cap_epinfo);
	out_ep_num = CAP_RX_EP_NUM(cap_epinfo);

	dev_info(mtu->dev, "fifosz/epnum: Tx=%#x/%d, Rx=%#x/%d\n",
		 mtu3_readl(mbase, U3D_CAP_EPNTXFFSZ), in_ep_num,
		 mtu3_readl(mbase, U3D_CAP_EPNRXFFSZ), out_ep_num);

	/* one for ep0, another is reserved */
	mtu->num_eps = min(in_ep_num, out_ep_num) + 1;
	ep_array = kcalloc(mtu->num_eps * 2, sizeof(*ep_array), GFP_KERNEL);
	if (ep_array == NULL)
		return -ENOMEM;

	mtu->ep_array = ep_array;
	mtu->in_eps = ep_array;
	mtu->out_eps = &ep_array[mtu->num_eps];
	/* ep0 uses in_eps[0], out_eps[0] is reserved */
	mtu->ep0 = mtu->in_eps;
	mtu->ep0->mtu = mtu;
	mtu->ep0->epnum = 0;

	for (i = 1; i < mtu->num_eps; i++) {
		struct mtu3_ep *mep = mtu->in_eps + i;

		mep->fifo = &mtu->tx_fifo;
		mep = mtu->out_eps + i;
		mep->fifo = &mtu->rx_fifo;
	}

	get_ep_fifo_config(mtu);

	ret = mtu3_qmu_init(mtu);
	if (ret)
		kfree(mtu->ep_array);

	return ret;
}

static void mtu3_mem_free(struct mtu3 *mtu)
{
	mtu3_qmu_exit(mtu);
	kfree(mtu->ep_array);
}

static irqreturn_t mtu3_link_isr(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;
	enum usb_device_speed udev_speed;
	u32 maxpkt = 64;
	u32 link;
	u32 speed;

	link = mtu3_readl(mbase, U3D_DEV_LINK_INTR);
	link &= mtu3_readl(mbase, U3D_DEV_LINK_INTR_ENABLE);
	mtu3_writel(mbase, U3D_DEV_LINK_INTR, link); /* W1C */
	dev_dbg(mtu->dev, "=== LINK[%x] ===\n", link);

	if (!(link & SSUSB_DEV_SPEED_CHG_INTR))
		return IRQ_NONE;

	speed = SSUSB_DEV_SPEED(mtu3_readl(mbase, U3D_DEVICE_CONF));

	switch (speed) {
	case MTU3_SPEED_FULL:
		udev_speed = USB_SPEED_FULL;
		/*BESLCK = 4 < BESLCK_U3 = 10 < BESLDCK = 15 */
		mtu3_writel(mbase, U3D_USB20_LPM_PARAMETER, LPM_BESLDCK(0xf)
				| LPM_BESLCK(4) | LPM_BESLCK_U3(0xa));
		mtu3_setbits(mbase, U3D_POWER_MANAGEMENT,
				LPM_BESL_STALL | LPM_BESLD_STALL);
		break;
	case MTU3_SPEED_HIGH:
		udev_speed = USB_SPEED_HIGH;
		/*BESLCK = 4 < BESLCK_U3 = 10 < BESLDCK = 15 */
		mtu3_writel(mbase, U3D_USB20_LPM_PARAMETER, LPM_BESLDCK(0xf)
				| LPM_BESLCK(4) | LPM_BESLCK_U3(0xa));
		mtu3_setbits(mbase, U3D_POWER_MANAGEMENT,
				LPM_BESL_STALL | LPM_BESLD_STALL);
		break;
	case MTU3_SPEED_SUPER:
		udev_speed = USB_SPEED_SUPER;
		maxpkt = 512;
		break;
	case MTU3_SPEED_SUPER_PLUS:
		udev_speed = USB_SPEED_SUPER_PLUS;
		maxpkt = 512;
		break;
	default:
		udev_speed = USB_SPEED_UNKNOWN;
		break;
	}
	dev_dbg(mtu->dev, "%s: %s\n", __func__, usb_speed_string(udev_speed));
	mtu3_dbg_trace(mtu->dev, "link speed %s",
		       usb_speed_string(udev_speed));

	mtu->g.speed = udev_speed;
	mtu->g.ep0->maxpacket = maxpkt;
	mtu->ep0_state = MU3D_EP0_STATE_SETUP;
	mtu->connected = !!(udev_speed != USB_SPEED_UNKNOWN);

	if (udev_speed == USB_SPEED_UNKNOWN) {
		mtu3_gadget_disconnect(mtu);
		pm_runtime_put(mtu->dev);
	} else {
		pm_runtime_get(mtu->dev);
		mtu3_ep0_setup(mtu);

		if (udev_speed >= MTU3_SPEED_SUPER)
			ssusb_phy_dp_pullup(mtu->ssusb);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mtu3_u3_ltssm_isr(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;
	u32 ltssm;

	ltssm = mtu3_readl(mbase, U3D_LTSSM_INTR);
	ltssm &= mtu3_readl(mbase, U3D_LTSSM_INTR_ENABLE);
	mtu3_writel(mbase, U3D_LTSSM_INTR, ltssm); /* W1C */
	dev_dbg(mtu->dev, "=== LTSSM[%x] ===\n", ltssm);
	trace_mtu3_u3_ltssm_isr(ltssm);

	if (ltssm & (HOT_RST_INTR | WARM_RST_INTR))
		mtu3_gadget_reset(mtu);

	if (ltssm & VBUS_FALL_INTR) {
		mtu3_ss_func_set(mtu, false);
		mtu3_gadget_reset(mtu);
	}

	if (ltssm & VBUS_RISE_INTR)
		mtu3_ss_func_set(mtu, true);

	if (ltssm & EXIT_U3_INTR)
		mtu3_gadget_resume(mtu);

	if (ltssm & ENTER_U3_INTR)
		mtu3_gadget_suspend(mtu);

	return IRQ_HANDLED;
}

static irqreturn_t mtu3_u2_common_isr(struct mtu3 *mtu)
{
	void __iomem *mbase = mtu->mac_base;
	u32 u2comm;

	u2comm = mtu3_readl(mbase, U3D_COMMON_USB_INTR);
	u2comm &= mtu3_readl(mbase, U3D_COMMON_USB_INTR_ENABLE);
	mtu3_writel(mbase, U3D_COMMON_USB_INTR, u2comm); /* W1C */
	dev_dbg(mtu->dev, "=== U2COMM[%x] ===\n", u2comm);
	trace_mtu3_u2_common_isr(u2comm);

	if (u2comm & SUSPEND_INTR)
		mtu3_gadget_suspend(mtu);

	if (u2comm & RESUME_INTR)
		mtu3_gadget_resume(mtu);

	if (u2comm & RESET_INTR)
		mtu3_gadget_reset(mtu);

	if (u2comm & LPM_RESUME_INTR)
		mtu3_gadget_u2_lpm_lock(mtu, U2_LPM_LOCK_TIMEOUT);

	return IRQ_HANDLED;
}

static irqreturn_t mtu3_irq(int irq, void *data)
{
	struct mtu3 *mtu = (struct mtu3 *)data;
	unsigned long flags;
	u32 level1;
	u32 tmp;

	spin_lock_irqsave(&mtu->lock, flags);

	/* U3D_LV1ISR is RU */
	level1 = mtu3_readl(mtu->mac_base, U3D_LV1ISR);
	level1 &= mtu3_readl(mtu->mac_base, U3D_LV1IER);

	if (unlikely(!mtu->softconnect) && (level1 & MAC2_INTR)) {
		dev_info(mtu->dev, "%s !softconnect MAC2_INTR\n", __func__);
		tmp = mtu3_readl(mtu->mac_base, U3D_COMMON_USB_INTR);
		tmp &= mtu3_readl(mtu->mac_base, U3D_COMMON_USB_INTR_ENABLE);
		mtu3_writel(mtu->mac_base, U3D_COMMON_USB_INTR, tmp);
		spin_unlock_irqrestore(&mtu->lock, flags);
		return IRQ_HANDLED;
	}

	if (unlikely(!mtu->softconnect) && (level1 & BMU_INTR)) {
		dev_info(mtu->dev, "%s !softconnect BMU_INTR\n", __func__);
		tmp = mtu3_readl(mtu->mac_base, U3D_EPISR);
		tmp &= mtu3_readl(mtu->mac_base, U3D_EPIER);
		mtu3_writel(mtu->mac_base, U3D_EPISR, tmp);
		spin_unlock_irqrestore(&mtu->lock, flags);
		return IRQ_HANDLED;
	}

	if (unlikely(!mtu->softconnect) && (level1 & QMU_INTR)) {
		dev_info(mtu->dev, "%s !softconnect QMU_INTR\n", __func__);
		tmp = mtu3_readl(mtu->mac_base, U3D_QISAR0);
		tmp &= mtu3_readl(mtu->mac_base, U3D_QIER0);
		mtu3_writel(mtu->mac_base, U3D_QISAR0, tmp);
		spin_unlock_irqrestore(&mtu->lock, flags);
		return IRQ_HANDLED;
	}

	if (unlikely(!mtu->softconnect) && (level1 & MAC3_INTR)) {
		dev_info(mtu->dev, "%s !softconnect MAC3_INTR\n", __func__);
		tmp = mtu3_readl(mtu->mac_base, U3D_LTSSM_INTR);
		tmp &= mtu3_readl(mtu->mac_base, U3D_LTSSM_INTR_ENABLE);
		mtu3_writel(mtu->mac_base, U3D_LTSSM_INTR, tmp);
		spin_unlock_irqrestore(&mtu->lock, flags);
		return IRQ_HANDLED;
	}

	if (level1 & EP_CTRL_INTR)
		mtu3_link_isr(mtu);

	if (level1 & MAC2_INTR)
		mtu3_u2_common_isr(mtu);

	if (level1 & MAC3_INTR)
		mtu3_u3_ltssm_isr(mtu);

	if (level1 & BMU_INTR)
		mtu3_ep0_isr(mtu);

	if (level1 & QMU_INTR)
		mtu3_qmu_isr(mtu);

	spin_unlock_irqrestore(&mtu->lock, flags);

	return IRQ_HANDLED;
}

static void mtu3_gadget_set_ready(struct mtu3 *mtu)
{
	struct device_node *np = mtu->dev->of_node;

	dev_info(mtu->dev, "remove cdp-block property\n");

	of_remove_property(np, of_find_property(np, "cdp-block", NULL));
}

static int mtu3_hw_init(struct mtu3 *mtu)
{
	u32 value;
	int ret;

	value = mtu3_readl(mtu->ippc_base, U3D_SSUSB_IP_TRUNK_VERS);
	mtu->hw_version = IP_TRUNK_VERS(value);

	if(of_property_read_bool(mtu->dev->of_node, "mediatek,gen2cp-unsupported")) {
		mtu->gen2cp = 0;
		dev_info(mtu->dev, "force gen2cp to be 0 ");
	} else {
		mtu->gen2cp = !!(mtu->hw_version >= MTU3_TRUNK_VERS_1003);
	}

	value = mtu3_readl(mtu->ippc_base, U3D_SSUSB_IP_DEV_CAP);
	mtu->u3_capable = !!SSUSB_IP_DEV_U3_PORT_NUM(value);
	/* usb3 ip uses separate fifo */
	mtu->separate_fifo = mtu->u3_capable;

	dev_info(mtu->dev, "IP version 0x%x(%s IP)\n", mtu->hw_version,
		mtu->u3_capable ? "U3" : "U2");

	mtu3_check_params(mtu);

	mtu3_device_reset(mtu);

	ret = mtu3_device_enable(mtu);
	if (ret) {
		dev_err(mtu->dev, "device enable failed %d\n", ret);
		return ret;
	}

	ret = mtu3_mem_alloc(mtu);
	if (ret)
		return -ENOMEM;

	mtu3_regs_init(mtu);
	mtu3_gadget_set_ready(mtu);

	return 0;
}

static void mtu3_hw_exit(struct mtu3 *mtu)
{
	mtu3_device_disable(mtu);
	mtu3_mem_free(mtu);
}

/*
 * we set 32-bit DMA mask by default, here check whether the controller
 * supports 36-bit DMA or not, if it does, set 36-bit DMA mask.
 */
static int mtu3_set_dma_mask(struct mtu3 *mtu)
{
	struct device *dev = mtu->dev;
	bool is_36bit = false;
	int ret = 0;
	u32 value;

	value = mtu3_readl(mtu->mac_base, U3D_MISC_CTRL);
	if (value & DMA_ADDR_36BIT) {
		is_36bit = true;
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
		/* If set 36-bit DMA mask fails, fall back to 32-bit DMA mask */
		if (ret) {
			is_36bit = false;
			ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
		}
	}
	dev_info(dev, "dma mask: %s bits\n", is_36bit ? "36" : "32");

	return ret;
}

static void ssusb_get_host_speed_max(struct mtu3 *mtu)
{
	u32 cap_val, host_u3p_num;

	if (mtu == NULL || mtu->ippc_base == NULL)
		return;

	cap_val = mtu3_readl(mtu->ippc_base, U3D_SSUSB_IP_XHCI_CAP);
	host_u3p_num = SSUSB_IP_XHCI_U3_PORT_NUM(cap_val);

	if (host_u3p_num) {
		cap_val = (mtu3_readl(mtu->ippc_base, U3D_SSUSB_IP_MAC_CAP) &
			  SSUSB_IP_MAC_U3_SPEED_CAP_MSK) >> SSUSB_IP_MAC_U3_SPEED_CAP_OFST;
		switch (cap_val) {
		case SSUSB_IP_MAC_U3_SPEED_GEN2X2:
		case SSUSB_IP_MAC_U3_SPEED_GEN2X1:
			mtu->max_speed_host = USB_SPEED_SUPER_PLUS;
			break;
		case SSUSB_IP_MAC_U3_SPEED_GEN1X2:
		case SSUSB_IP_MAC_U3_SPEED_GEN1X1:
			mtu->max_speed_host = USB_SPEED_SUPER;
			break;
		default:
			mtu->max_speed_host = USB_SPEED_HIGH;
			break;
		}
	} else
		mtu->max_speed_host = USB_SPEED_HIGH;
}

int ssusb_gadget_init(struct ssusb_mtk *ssusb)
{
	struct device *dev = ssusb->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct mtu3 *mtu = NULL;
	int ret = -ENOMEM;

	mtu = devm_kzalloc(dev, sizeof(struct mtu3), GFP_KERNEL);
	if (mtu == NULL)
		return -ENOMEM;

	mtu->irq = platform_get_irq_byname_optional(pdev, "device");
	if (mtu->irq < 0) {
		if (mtu->irq == -EPROBE_DEFER)
			return mtu->irq;

		/* for backward compatibility */
		mtu->irq = platform_get_irq(pdev, 0);
		if (mtu->irq < 0)
			return mtu->irq;
	}
	dev_info(dev, "irq %d\n", mtu->irq);

	spin_lock_init(&mtu->lock);
	mtu->dev = dev;
	mtu->ippc_base = ssusb->ippc_base;
	mtu->mac_base = ssusb->mac_base;
	ssusb->u3d = mtu;
	mtu->ssusb = ssusb;
	mtu->max_speed = usb_get_maximum_speed(dev);

	mtu->u3_lpm = !of_property_read_bool(dev->of_node, "usb3-lpm-disable");
	mtu->u3_u1gou2 = !of_property_read_bool(dev->of_node, "usb3-u1gou2-disable");

	dev_dbg(dev, "mac_base=0x%p, ippc_base=0x%p\n",
		mtu->mac_base, mtu->ippc_base);

	/* check usbif compliance property */
	if (device_property_read_string(mtu->dev, "usb-psy-name", &mtu->usb_psy_name) >= 0) {
		dev_info(mtu->dev, "usb psy: %s\n", mtu->usb_psy_name);
		INIT_WORK(&mtu->draw_work, mtu3_vbus_draw_work);
	}

	if (device_property_read_string(mtu->dev, "typec-name", &mtu->typec_name) >= 0)
		dev_info(mtu->dev, "typec: %s\n", mtu->typec_name);

	if (device_property_read_string(mtu->dev, "typec-port-name", &mtu->typec_port_name) >= 0)
		dev_info(mtu->dev, "typec port: %s\n", mtu->typec_port_name);

	ssusb_parse_toggle_vbus(ssusb, dev->of_node);

	ret = mtu3_hw_init(mtu);
	if (ret) {
		dev_err(dev, "mtu3 hw init failed:%d\n", ret);
		return ret;
	}
	if (of_property_read_u32(dev->of_node, "maximum-speed-host", &mtu->max_speed_host) < 0)
		ssusb_get_host_speed_max(mtu);

	dev_info(dev, "max_speed_host: %s\n", usb_speed_string(mtu->max_speed_host));


	ret = mtu3_set_dma_mask(mtu);
	if (ret) {
		dev_err(dev, "mtu3 set dma_mask failed:%d\n", ret);
		goto dma_mask_err;
	}

	ret = devm_request_threaded_irq(dev, mtu->irq, NULL,
			mtu3_irq, IRQF_ONESHOT, dev_name(dev), mtu);

	if (ret) {
		dev_err(dev, "request irq %d failed!\n", mtu->irq);
		goto irq_err;
	}

	/* power down device IP for power saving by default */
	mtu3_stop(mtu);

	ret = mtu3_gadget_setup(mtu);
	if (ret) {
		dev_err(dev, "mtu3 gadget init failed:%d\n", ret);
		goto gadget_err;
	}

	ssusb_dev_debugfs_init(ssusb);

	dev_dbg(dev, " %s() done...\n", __func__);

	return 0;

gadget_err:
	device_init_wakeup(dev, false);

dma_mask_err:
irq_err:
	mtu3_hw_exit(mtu);
	ssusb->u3d = NULL;
	dev_err(dev, " %s() fail...\n", __func__);

	return ret;
}

void ssusb_gadget_exit(struct ssusb_mtk *ssusb)
{
	struct mtu3 *mtu = ssusb->u3d;

	mtu3_gadget_cleanup(mtu);
	device_init_wakeup(ssusb->dev, false);
	mtu3_hw_exit(mtu);
}

bool ssusb_gadget_ip_sleep_check(struct ssusb_mtk *ssusb)
{
	struct mtu3 *mtu = ssusb->u3d;

	/* host only, should wait for ip sleep */
	if (!mtu)
		return true;

	/* device is started and pullup D+, ip can sleep */
	if (mtu->is_active && mtu->softconnect)
		return true;

	/* ip can't sleep if not pullup D+ when support device mode */
	return false;
}

int ssusb_gadget_suspend(struct ssusb_mtk *ssusb, pm_message_t msg)
{
	struct mtu3 *mtu = ssusb->u3d;

	if (!mtu->gadget_driver)
		return 0;

	if (mtu->connected)
		return -EBUSY;

	mtu3_dev_suspend(mtu);
	synchronize_irq(mtu->irq);

	return 0;
}

int ssusb_gadget_resume(struct ssusb_mtk *ssusb, pm_message_t msg)
{
	struct mtu3 *mtu = ssusb->u3d;

	if (!mtu->gadget_driver)
		return 0;

	mtu3_dev_resume(mtu);

	return 0;
}
