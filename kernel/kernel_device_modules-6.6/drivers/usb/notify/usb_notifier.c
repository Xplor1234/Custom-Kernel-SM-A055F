// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2023 Samsung Electronics Co. Ltd.
 * Samsung USB members
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define pr_fmt(fmt) "usb_notifier: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/usb/role.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/usb_notify.h>

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif
#if IS_MODULE(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery_common.h>
#elif defined(CONFIG_BATTERY_SAMSUNG)
#include "../../../battery/common/sec_charging_common.h"
#endif
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
#include <linux/combo_redriver/ps5169.h>
#endif

#if IS_ENABLED(CONFIG_EXTCON_MTK_USB)
#include "../../misc/mediatek/extcon/extcon-mtk-usb.h"
#endif

struct usb_notifier_platform_data {
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct	notifier_block pdic_usb_nb;
	int is_host;
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	struct	notifier_block muic_usb_nb;
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct	notifier_block vbus_nb;
#endif
	int	gpio_redriver_en;
	int can_disable_usb;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct delayed_work usb_ldo_work;
	unsigned int usb_ldocontrol;
	unsigned int usb_ldo_onoff;
	bool usb_ldo_off_working;
	const char *hs_vdd;
	const char *ss_vdd;
	const char *dp_vdd;
#endif
	int  host_wake_lock_enable;
	int  device_wake_lock_enable;
};

#ifdef CONFIG_OF
static void of_get_usb_redriver_dt(struct device_node *np,
		struct usb_notifier_platform_data *pdata)
{
	int gpio = 0;

	gpio = of_get_named_gpio(np, "gpios_redriver_en", 0);
	if (!gpio_is_valid(gpio)) {
		pdata->gpio_redriver_en = -1;
		pr_err("%s: usb30_redriver_en: Invalied gpio pins\n", __func__);
	} else
		pdata->gpio_redriver_en = gpio;

	pr_info("%s, gpios_redriver_en %d\n", __func__, gpio);

	pdata->can_disable_usb =
		!(of_property_read_bool(np, "samsung,unsupport-disable-usb"));
	pr_info("%s, can_disable_usb %d\n", __func__, pdata->can_disable_usb);

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	if (of_property_read_string(np, "hs-regulator", &pdata->hs_vdd) < 0) {
		pr_err("%s - get hs_vdd error\n", __func__);
		pdata->hs_vdd = NULL;
	}
	if (of_property_read_string(np, "ss-regulator", &pdata->ss_vdd) < 0) {
		pr_err("%s - get ss_vdd error\n", __func__);
		pdata->ss_vdd = NULL;
	}
	if (of_property_read_string(np, "dp-regulator", &pdata->dp_vdd) < 0) {
		pr_err("%s - get dp_vdd error\n", __func__);
		pdata->dp_vdd = NULL;
	}

	if (of_property_read_u32(np, "usb-ldocontrol", &pdata->usb_ldocontrol))
		pdata->usb_ldocontrol = 0;

	pr_info("%s, usb_ldocontrol %d\n", __func__, pdata->usb_ldocontrol);
#endif
	pdata->host_wake_lock_enable =
		!(of_property_read_bool(np, "disable_host_wakelock"));

	pdata->device_wake_lock_enable =
		!(of_property_read_bool(np, "disable_device_wakelock"));

	pr_info("%s, host_wake_lock_enable %d ,device_wake_lock_enable %d\n", __func__,
		pdata->host_wake_lock_enable, pdata->device_wake_lock_enable);

}

static int of_usb_notifier_dt(struct device *dev,
		struct usb_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	of_get_usb_redriver_dt(np, pdata);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static void usb_hs_regulator_onoff(struct usb_notifier_platform_data *pdata,
				   unsigned int onoff)
{
	struct regulator *avdd33_usb;
	int ret;

	if (!pdata->hs_vdd) {
		pr_err("%s hs_vdd is null\n", __func__);
		return;
	}

	avdd33_usb = regulator_get(NULL, pdata->hs_vdd);
	if (IS_ERR(avdd33_usb) || avdd33_usb == NULL) {
		pr_err("%s - avdd33_usb regulator_get fail\n", __func__);
		return;
	}

	if (onoff) {
		ret = regulator_enable(avdd33_usb);
		if (ret) {
			pr_err("%s - enable avdd33_usb ldo enable failed, ret=%d\n",
				__func__, ret);
		}
	} else {
		ret = regulator_disable(avdd33_usb);
		if (ret) {
			pr_err("%s - enable avdd33_usb ldo disable failed, ret=%d\n",
				__func__, ret);
		}
	}

	regulator_put(avdd33_usb);
}

static void usb_ss_regulator_onoff(struct usb_notifier_platform_data *pdata,
				   bool onoff)
{
	struct regulator *vdd085_usb;
	int ret;

	if (!pdata->ss_vdd) {
		pr_err("%s ss_vdd is null\n", __func__);
		return;
	}

	vdd085_usb = regulator_get(NULL, pdata->ss_vdd);
	if (IS_ERR(vdd085_usb) || vdd085_usb == NULL) {
		pr_err("%s - vdd085_usb regulator_get fail\n", __func__);
		return;
	}

	if (onoff) {
		ret = regulator_enable(vdd085_usb);
		if (ret) {
			pr_err("%s - enable vdd085_usb ldo enable failed, ret=%d\n",
				__func__, ret);
		}
	} else {
		ret = regulator_disable(vdd085_usb);
		if (ret) {
			pr_err("%s - enable vdd085_usb ldo disable failed, ret=%d\n",
				__func__, ret);
		}
	}

	regulator_put(vdd085_usb);
}

static void usb_dp_regulator_onoff(struct usb_notifier_platform_data *pdata,
				   unsigned int onoff)
{
	struct regulator *vdd3p3_dp;
	int ret;

	if (!pdata->dp_vdd) {
		pr_err("%s dp_vdd is null\n", __func__);
		return;
	}

	vdd3p3_dp = regulator_get(NULL, pdata->dp_vdd);
	if (IS_ERR(vdd3p3_dp) || vdd3p3_dp == NULL) {
		pr_err("%s - dp_3p3 regulator_get fail\n", __func__);
		return;
	}

	if (onoff) {
		ret = regulator_enable(vdd3p3_dp);
		if (ret) {
			pr_err("%s - enable dp_3p3 ldo enable failed, ret=%d\n",
				__func__, ret);
		}
	} else {
		ret = regulator_disable(vdd3p3_dp);
		if (ret) {
			pr_err("%s - enable dp_3p3 ldo disable failed, ret=%d\n",
				__func__, ret);
		}
	}

	regulator_put(vdd3p3_dp);
}

static void usb_regulator_onoff(void *data, unsigned int onoff)
{
	struct usb_notifier_platform_data *pdata =
		(struct usb_notifier_platform_data *)(data);
	pr_info("usb: %s - Turn %s (ldocontrol=%d, usb_ldo_off_working=%d)\n", __func__,
		onoff ? "on":"off", pdata->usb_ldocontrol, pdata->usb_ldo_off_working);

	if (pdata->usb_ldocontrol) {
		if (onoff) {
			if (!pdata->usb_ldo_onoff) {
				if (pdata->usb_ldo_off_working) {
					cancel_delayed_work_sync(&pdata->usb_ldo_work);
					pdata->usb_ldo_off_working = false;
				} else {
					usb_ss_regulator_onoff(pdata, onoff);
					usb_dp_regulator_onoff(pdata, onoff);
					usb_hs_regulator_onoff(pdata, onoff);
				}
			} else {
				pr_err("%s already on\n", __func__);
			}
		} else {
			if (pdata->usb_ldo_onoff) {
				pdata->usb_ldo_off_working = true;
				schedule_delayed_work(&pdata->usb_ldo_work, msecs_to_jiffies(2000));
			} else {
				pr_err("%s already off\n", __func__);
			}
		}
		pdata->usb_ldo_onoff = onoff;
	} else {
		pr_err("%s: can't control\n", __func__);
	}
}

static void usb_ldo_off_control(struct work_struct *work)
{
	struct usb_notifier_platform_data *pdata = container_of(work,
						 struct usb_notifier_platform_data,
						 usb_ldo_work.work);

	pr_info("usb: %s - working=%d\n", __func__, pdata->usb_ldo_off_working);
	if (pdata->usb_ldo_off_working) {
		pdata->usb_ldo_off_working = false;
		usb_hs_regulator_onoff(pdata, false);
		usb_dp_regulator_onoff(pdata, false);
		usb_ss_regulator_onoff(pdata, false);
	}
}
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static int pdic_usb_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;
	struct otg_notify *o_notify = get_otg_notify();
	struct usb_notifier_platform_data *pdata =
		container_of(nb, struct usb_notifier_platform_data, pdic_usb_nb);

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB)
		return 0;

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		pr_info("%s: Turn On Host(DFP)\n", __func__);
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
		ps5169_config(USB_ONLY_MODE, 1);
#endif
		send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		pdata->is_host = 1;
		break;
	case USB_STATUS_NOTIFY_ATTACH_UFP:
		pr_info("%s: Turn On Device(UFP)\n", __func__);
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
		ps5169_config(USB_ONLY_MODE, 0);
#endif
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		if (pdata->is_host) {
			pr_info("%s: Turn Off Host(DFP)\n", __func__);
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
			pdata->is_host = 0;
		} else {
			pr_info("%s: Turn Off Device(UFP)\n", __func__);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		}
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
		ps5169_config(CLEAR_STATE, 0);
#endif
		break;
	default:
		pr_info("%s: unsupported DRP type : %d.\n", __func__, usb_status.drp);
		break;
	}
	return 0;
}
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
static int muic_usb_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct otg_notify *o_notify = get_otg_notify();
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_ATTACH_TYPEDEF *p_noti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;

	pr_info("%s action=%lu, attached_dev=%d\n",
		__func__, action, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_USB_CABLE, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_USB_CABLE, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		send_otg_notify(o_notify, NOTIFY_EVENT_USB_CABLE, 0);
		break;
	}
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	pr_info("%s action=%lu, attached_dev=%d\n",
		__func__, action, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			send_otg_notify(o_notify, NOTIFY_EVENT_USB_CABLE, 0);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			send_otg_notify(o_notify, NOTIFY_EVENT_USB_CABLE, 1);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		} else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_HMT_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HMT, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HMT, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			pr_info("%s - USB_HOST_TEST_DETACHED\n", __func__);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			pr_info("%s - USB_HOST_TEST_ATTACHED\n", __func__);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_SMARTDOCK_TA, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_SMARTDOCK_TA, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify
				(o_notify, NOTIFY_EVENT_SMARTDOCK_USB, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify
				(o_notify, NOTIFY_EVENT_SMARTDOCK_USB, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_AUDIODOCK, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_AUDIODOCK, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_MMDOCK, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_MMDOCK, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_LANHUB, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_LANHUB, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_GAMEPAD_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_GAMEPAD, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_GAMEPAD, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		send_otg_notify(o_notify, NOTIFY_EVENT_USB_CABLE, 0);
		break;
	}
#endif

	return 0;
}
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
static int vbus_handle_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *)data;
	struct otg_notify *o_notify;

	o_notify = get_otg_notify();

	pr_info("%s cmd=%lu, vbus_type=%s\n",
		__func__, cmd, vbus_type == STATUS_VBUS_HIGH ? "HIGH" : "LOW");

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER, 1);
		break;
	case STATUS_VBUS_LOW:
		send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER, 0);
		break;
	default:
		break;
	}
	return 0;
}
#endif

static int otg_accessory_power(bool enable)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	u8 on = (u8)!!enable;
	union power_supply_propval val;

	pr_info("otg accessory power = %d\n", on);

	val.intval = enable;
	psy_do_property("otg", set,
			POWER_SUPPLY_PROP_ONLINE, val);
#endif
	return 0;
}

static int set_online(int event, int state)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval val;
#endif
	struct device_node *np_charger = NULL;
	char *charger_name;

	if (event == NOTIFY_EVENT_SMTD_EXT_CURRENT)
		pr_info("request smartdock charging current = %s\n",
			state ? "1000mA" : "1700mA");
	else if (event == NOTIFY_EVENT_MMD_EXT_CURRENT)
		pr_info("request mmdock charging current = %s\n",
			state ? "900mA" : "1400mA");

	np_charger = of_find_node_by_name(NULL, "battery");
	if (!np_charger) {
		pr_err("%s: failed to get the battery device node\n", __func__);
		return 0;
	}

	if (!of_property_read_string(np_charger, "battery,charger_name",
				(char const **)&charger_name)) {
		pr_info("%s: charger_name = %s\n", __func__,
				charger_name);
	} else {
		pr_err("%s: failed to get the charger name\n",
							 __func__);
		return 0;
	}

	/* for KNOX DT charging */
	pr_info("Knox Desktop connection state = %s\n", state ? "Connected" : "Disconnected");
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (state)
		val.intval = SEC_BATTERY_CABLE_SMART_NOTG;
	else
		val.intval = SEC_BATTERY_CABLE_NONE;

	psy_do_property("battery", set,
			POWER_SUPPLY_PROP_ONLINE, val);
#endif

	return 0;
}

/*
static int mtk_set_host(bool enable)
{
	pr_info("%s enable %d +\n", __func__, enable);

	if (enable)
		mtk_usb_notify_set_mode(USB_ROLE_HOST);
	else
		mtk_usb_notify_set_mode(USB_ROLE_NONE);

	pr_info("%s -\n", __func__);
	return 0;
}

static int mtk_set_peripheral(bool enable)
{
	pr_info("%s enable %d +\n", __func__, enable);

	if (enable)
		mtk_usb_notify_set_mode(USB_ROLE_DEVICE);
	else
		mtk_usb_notify_set_mode(USB_ROLE_NONE);

	pr_info("%s -\n", __func__);
	return 0;
}
*/

static int usb_set_chg_current(int state)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval val;
#endif
	struct device_node *np_charger = NULL;
	char *charger_name;

	np_charger = of_find_node_by_name(NULL, "battery");
	if (!np_charger) {
		pr_err("%s: failed to get the battery device node\n", __func__);
		return 0;
	}

	if (!of_property_read_string(np_charger, "battery,charger_name",
				(char const **)&charger_name)) {
		pr_info("%s: charger_name = %s\n", __func__,
				charger_name);
	} else {
		pr_err("%s: failed to get the charger name\n",  __func__);
		return 0;
	}

	/* current setting */
	pr_info("usb : charging current set = %d\n", state);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	switch (state) {
	case NOTIFY_USB_SUSPENDED:
		val.intval = USB_CURRENT_SUSPENDED;
		break;
	case NOTIFY_USB_UNCONFIGURED:
		val.intval = USB_CURRENT_UNCONFIGURED;
		break;
	case NOTIFY_USB_CONFIGURED:
		val.intval = USB_CURRENT_HIGH_SPEED;
		break;
	default:
		val.intval = USB_CURRENT_HIGH_SPEED;
		break;
	}
	psy_do_property("battery", set,
			POWER_SUPPLY_EXT_PROP_USB_CONFIGURE, val);
#endif

	return 0;
}
#if defined(CONFIG_USB_HW_PARAM)
static int is_skip_list(int index)
{
	int ret = 0;

	switch (index) {
	case USB_CCIC_VR_USE_COUNT:
	case USB_HOST_CLASS_COMM_COUNT:
	case USB_HOST_CLASS_PHYSICAL_COUNT:
	case USB_HOST_CLASS_IMAGE_COUNT:
	case USB_HOST_CLASS_PRINTER_COUNT:
	case USB_HOST_CLASS_CDC_COUNT:
	case USB_HOST_CLASS_CSCID_COUNT:
	case USB_HOST_CLASS_CONTENT_COUNT:
	case USB_HOST_CLASS_VIDEO_COUNT:
	case USB_HOST_CLASS_WIRELESS_COUNT:
	case USB_HOST_CLASS_MISC_COUNT:
	case USB_HOST_CLASS_APP_COUNT:
	case USB_HOST_CLASS_VENDOR_COUNT:
	case USB_CCIC_FWUP_ERROR_COUNT:
	case USB_CCIC_VERSION:
		ret = 1;
		break;
	default:
		break;
	}

	return ret;
}
#endif

static struct otg_notify musb_mtk_notify = {
	.vbus_drive	= otg_accessory_power,
//	.set_host = mtk_set_host,
//	.set_peripheral	= mtk_set_peripheral,
	.vbus_detect_gpio = -1,
	.is_wakelock = 1,
	.booting_delay_sec = 12,
#if !IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	.auto_drive_vbus = NOTIFY_OP_POST,
#endif
	.disable_control = 1,
	.device_check_sec = 3,
	.set_battcall = set_online,
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	.set_ldo_onoff = usb_regulator_onoff,
#endif
	.set_chg_current = usb_set_chg_current,
	.pre_peri_delay_us = 6,
#if defined(CONFIG_USB_OTG_WHITELIST_FOR_MDM)
	.sec_whitelist_enable = 0,
#endif
#if defined(CONFIG_USB_HW_PARAM)
	.is_skip_list = is_skip_list,
#endif
};

void register_set_peripheral(int (*mtk_set_peripheral)(bool enable)){
	musb_mtk_notify.set_peripheral = mtk_set_peripheral;
}
EXPORT_SYMBOL(register_set_peripheral);

void register_set_host(int (*mtk_set_host)(bool enable)){
	musb_mtk_notify.set_host = mtk_set_host;
}
EXPORT_SYMBOL(register_set_host);

static int usb_notifier_probe(struct platform_device *pdev)
{
	struct usb_notifier_platform_data *pdata = NULL;
	int ret = 0;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct usb_notifier_platform_data), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = of_usb_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			return ret;
		}

		pdev->dev.platform_data = pdata;
	} else
		pdata = pdev->dev.platform_data;

	musb_mtk_notify.redriver_en_gpio = pdata->gpio_redriver_en;
	musb_mtk_notify.disable_control = pdata->can_disable_usb;
	musb_mtk_notify.is_host_wakelock = pdata->host_wake_lock_enable;
	musb_mtk_notify.is_wakelock = pdata->device_wake_lock_enable;
	set_otg_notify(&musb_mtk_notify);
	set_notify_data(&musb_mtk_notify, pdata);

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdata->usb_ldo_onoff = 0;
	INIT_DELAYED_WORK(&pdata->usb_ldo_work,
		  usb_ldo_off_control);
	pdata->is_host = 0;
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_register(&pdata->pdic_usb_nb, pdic_usb_handle_notification,
					MANAGER_NOTIFY_PDIC_USB);
#elif IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_notifier_register(&pdata->pdic_usb_nb, pdic_usb_handle_notification,
				   PDIC_NOTIFY_DEV_USB);
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&pdata->muic_usb_nb, muic_usb_handle_notification,
			       MUIC_NOTIFY_DEV_USB);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&pdata->vbus_nb, vbus_handle_notification,
			       VBUS_NOTIFY_DEV_MANAGER);
#endif

	dev_info(&pdev->dev, "usb notifier probe\n");
	return 0;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) || IS_ENABLED(CONFIG_MUIC_NOTIFIER) || IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct usb_notifier_platform_data *pdata = dev_get_platdata(&pdev->dev);
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_unregister(&pdata->pdic_usb_nb);
#else
	pdic_notifier_unregister(&pdata->pdic_usb_nb);
#endif
#elif IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_notifier_unregister(&pdata->muic_usb_nb);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&pdata->vbus_nb);
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id usb_notifier_dt_ids[] = {
	{ .compatible = "samsung,usb-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_notifier_dt_ids);
#endif

static struct platform_driver usb_notifier_driver = {
	.probe		= usb_notifier_probe,
	.remove		= usb_notifier_remove,
	.driver		= {
		.name	= "usb_notifier",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(usb_notifier_dt_ids),
#endif
	},
};

static int __init usb_notifier_init(void)
{
	return platform_driver_register(&usb_notifier_driver);
}

static void __exit usb_notifier_exit(void)
{
	platform_driver_unregister(&usb_notifier_driver);
}

late_initcall(usb_notifier_init);
module_exit(usb_notifier_exit);

MODULE_AUTHOR("Samsung USB Team");
MODULE_DESCRIPTION("USB notifier");
MODULE_LICENSE("GPL");
