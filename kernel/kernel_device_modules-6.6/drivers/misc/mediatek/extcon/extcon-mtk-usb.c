// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/extcon-provider.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>
#include <linux/usb/role.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>

#include "extcon-mtk-usb.h"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "tcpm.h"
#endif

#if defined(CONFIG_W2_CHARGER_PRIVATE)
#include "charger_class.h"
#endif

#if IS_ENABLED(CONFIG_WT_PROJECT_S96818AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96818BA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96902AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901WA1)
#include <linux/input/tp_notifier.h>
#endif

#if defined (CONFIG_N28_CHARGER_PRIVATE)
extern bool is_sd7601_chg_probe_ok;
extern bool is_upm6910_chg_probe_ok;
#endif
#if defined (CONFIG_W2_CHARGER_PRIVATE)
extern bool is_tcpc_mt6360_probe_ok;
#endif

#if IS_ENABLED(CONFIG_USB_NOTIFIER)
#include <linux/usb_notify.h>

static struct mtk_extcon_info *g_extcon;
#endif

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static void mtk_usb_extcon_update_role(struct work_struct *work)
{
	struct usb_role_info *role = container_of(to_delayed_work(work),
					struct usb_role_info, dwork);
	struct mtk_extcon_info *extcon = role->extcon;
	unsigned int cur_dr, new_dr;

	cur_dr = extcon->c_role;
	new_dr = role->d_role;

	extcon->c_role = new_dr;

	dev_info(extcon->dev, "cur_dr(%d) new_dr(%d)\n", cur_dr, new_dr);

	/* none -> device */
	if (cur_dr == USB_ROLE_NONE &&
			new_dr == USB_ROLE_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
	/* none -> host */
	} else if (cur_dr == USB_ROLE_NONE &&
			new_dr == USB_ROLE_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, true);
	/* device -> none */
	} else if (cur_dr == USB_ROLE_DEVICE &&
			new_dr == USB_ROLE_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
	/* host -> none */
	} else if (cur_dr == USB_ROLE_HOST &&
			new_dr == USB_ROLE_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
	/* device -> host */
	} else if (cur_dr == USB_ROLE_DEVICE &&
			new_dr == USB_ROLE_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
		extcon_set_state_sync(extcon->edev,	EXTCON_USB_HOST, true);
	/* host -> device */
	} else if (cur_dr == USB_ROLE_HOST &&
			new_dr == USB_ROLE_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
		extcon_set_state_sync(extcon->edev,	EXTCON_USB, true);
	}

	/* usb role switch */
	if (extcon->role_sw)
		usb_role_switch_set_role(extcon->role_sw, new_dr);

	kfree(role);
}

#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
void register_set_peripheral (int (*f)(bool));
void register_set_host (int (*f)(bool));

#if IS_ENABLED(CONFIG_USB_NOTIFIER)
int mtk_usb_notify_set_mode(int role)
{
	struct mtk_extcon_info *extcon = g_extcon;
	struct usb_role_info *role_info;

	if (!extcon) {
		pr_info("%s: g_extcon is NULL\n", __func__);
		return -ENODEV;
	}

	pr_info("%s: role %d\n", __func__, role);
	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_ATOMIC);
	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, mtk_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);
	pr_info("%s: role %d--\n", __func__, role);
	return 0;
}
#endif

int set_host(bool enable){
	if(enable) {
		mtk_usb_notify_set_mode(USB_ROLE_HOST);
	}else {
		mtk_usb_notify_set_mode(USB_ROLE_NONE);
	}
	return 0;
}

int set_peripheral(bool enable){
	if(enable) {
		mtk_usb_notify_set_mode(USB_ROLE_DEVICE);
	}else {
		mtk_usb_notify_set_mode(USB_ROLE_NONE);
	}
	return 0;
}
#endif


static int mtk_usb_extcon_set_role(struct mtk_extcon_info *extcon,
						unsigned int role)
{
	struct usb_role_info *role_info;

#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	struct otg_notify *o_notify = get_otg_notify();
	unsigned int cur_dr = extcon->c_role;
	
	extcon->last_dr_event = role;
	
	dev_info(extcon->dev, "curent role (%d), new role (%d)\n", cur_dr, role);
	
	if (o_notify){
		if (role == USB_ROLE_DEVICE) {
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		} else if (role == USB_ROLE_HOST) {
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		} else if (role == USB_ROLE_NONE) {
			if (cur_dr == USB_ROLE_DEVICE) {
				send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
			} else {
				send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
			}
		}
	}
	return 0;
#endif

	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_ATOMIC);
	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, mtk_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);

	return 0;
}

static bool usb_is_online(struct mtk_extcon_info *extcon)
{
	union power_supply_propval pval;
	union power_supply_propval tval;
	int ret;

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return false;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &tval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get usb type\n");
		return false;
	}

	dev_info(extcon->dev, "online=%d, type=%d\n", pval.intval, tval.intval);


#if defined (CONFIG_N28_CHARGER_PRIVATE)
	if (tval.intval == POWER_SUPPLY_TYPE_USB || tval.intval == POWER_SUPPLY_TYPE_USB_CDP)
#else
	if (pval.intval && (tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP))
#endif
		return true;
	else
		return false;
}

static void mtk_usb_extcon_psy_detector(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
		struct mtk_extcon_info, wq_psy);

	/* Workaround for PR_SWAP, IF tcpc_dev, then do not switch role. */
	/* Since we will set USB to none when type-c plug out */
	#if IS_ENABLED(CONFIG_TCPC_CLASS)
	if (extcon->tcpc_dev) {
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
                if (usb_is_online(extcon) && extcon->c_role == USB_ROLE_NONE
                        && extcon->last_dr_event == USB_ROLE_NONE)
                        mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
#else
                if (usb_is_online(extcon) && extcon->c_role == USB_ROLE_NONE)
                        mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
#endif
	} else {
	#endif
		if (usb_is_online(extcon))
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		else
			mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	}
#endif
}

static int mtk_usb_extcon_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct mtk_extcon_info *extcon = container_of(nb,
					struct mtk_extcon_info, psy_nb);

	if (event != PSY_EVENT_PROP_CHANGED || psy != extcon->usb_psy)
		return NOTIFY_DONE;

	queue_delayed_work(system_power_efficient_wq, &extcon->wq_psy, 0);

	return NOTIFY_DONE;
}

static int mtk_usb_extcon_psy_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;

	if (!of_property_read_bool(dev->of_node, "charger")) {
		ret = -EINVAL;
		goto fail;
	}

	extcon->usb_psy = devm_power_supply_get_by_phandle(dev, "charger");
	if (IS_ERR_OR_NULL(extcon->usb_psy)) {
		/* try to get by name */
		extcon->usb_psy = power_supply_get_by_name("primary_chg");
		if (IS_ERR_OR_NULL(extcon->usb_psy)) {
			dev_err(dev, "fail to get usb_psy\n");
			extcon->usb_psy = NULL;
			ret = -EINVAL;
			goto fail;
		}
	}

	INIT_DELAYED_WORK(&extcon->wq_psy, mtk_usb_extcon_psy_detector);

	extcon->psy_nb.notifier_call = mtk_usb_extcon_psy_notifier;
	ret = power_supply_reg_notifier(&extcon->psy_nb);
	if (ret)
		dev_err(dev, "fail to register notifer\n");
fail:
	return ret;
}

static int mtk_usb_extcon_set_vbus(struct mtk_extcon_info *extcon,
							bool is_on)
{
	struct regulator *vbus = extcon->vbus;
	struct device *dev = extcon->dev;
	int ret;

	/* vbus is optional */
	if (!vbus || extcon->vbus_on == is_on)
		return 0;

	dev_info(dev, "vbus turn %s\n", is_on ? "on" : "off");

	if (is_on) {
		if (extcon->vbus_vol) {
			ret = regulator_set_voltage(vbus,
					extcon->vbus_vol, extcon->vbus_vol);
			if (ret) {
				dev_err(dev, "vbus regulator set voltage failed\n");
				return ret;
			}
		}

		if (extcon->vbus_cur) {
			ret = regulator_set_current_limit(vbus,
					extcon->vbus_cur, extcon->vbus_cur);
			if (ret) {
				dev_err(dev, "vbus regulator set current failed\n");
				return ret;
			}
		}

		ret = regulator_enable(vbus);
		if (ret) {
			dev_info(dev, "vbus regulator enable failed\n");
			return ret;
		}
	} else {
		regulator_disable(vbus);
		/* Restore to default state */
		extcon->vbus_cur_inlimit = 0;
	}

	extcon->vbus_on = is_on;

	return 0;
}

static ssize_t vbus_limit_cur_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;

	if (!vbus)
		return sprintf(buf, "0");
	else
		return sprintf(buf, "%d\n", extcon->vbus_cur_inlimit);
}

static ssize_t vbus_limit_cur_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;
	int ret, in_limit = 0;
	unsigned int vbus_cur = 0;
	/* Check whether we have vbus instance */
	if (!vbus) {
		dev_info(dev, "No vbus instance\n");
		return -EOPNOTSUPP;
	}
	if ((kstrtoint(buf, 10, &in_limit) != 0) || (in_limit != 1  && in_limit != 0)) {
		dev_info(dev, "Invalid input\n");
		return -EINVAL;
	}

	extcon->vbus_cur_inlimit = (in_limit == 1);
	/* Only operate while vbus is on */
	if (extcon->vbus_on) {
		if (extcon->vbus_cur_inlimit && extcon->vbus_limit_cur)
			vbus_cur = extcon->vbus_limit_cur;
		else if (!extcon->vbus_cur_inlimit && extcon->vbus_cur)
			vbus_cur = extcon->vbus_cur;
	}
	if (vbus_cur) {
		ret = regulator_set_current_limit(vbus, vbus_cur, vbus_cur);
		if (ret) {
			dev_info(dev, "vbus regulator set current failed\n");
			return -EIO;
		}
	} else
		dev_info(dev, "Do not change current\n");
	return count;
}

static DEVICE_ATTR_RW(vbus_limit_cur);

static ssize_t vbus_switch_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;

	if (!vbus)
		return sprintf(buf, "0");
	else
		return sprintf(buf, "%d\n", extcon->vbus_on);
}

static ssize_t vbus_switch_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;
	int is_on = 0;

	/* Check whether we have vbus instance */
	if (!vbus) {
		dev_info(dev, "No vbus instance\n");
		return -EOPNOTSUPP;
	}
	if ((kstrtoint(buf, 10, &is_on) != 0) || (is_on != 1  && is_on != 0)) {
		dev_info(dev, "Invalid input\n");
		return -EINVAL;
	}

	mtk_usb_extcon_set_vbus(extcon, is_on);

	return count;
}

static DEVICE_ATTR_RW(vbus_switch);

static int mtk_usb_extcon_vbus_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;

	if (!of_property_read_bool(dev->of_node, "vbus-supply")) {
		ret = -EINVAL;
		goto fail;
	}

	extcon->vbus =  devm_regulator_get_exclusive(dev, "vbus");
	if (IS_ERR(extcon->vbus)) {
		/* try to get by name */
		extcon->vbus =  devm_regulator_get_exclusive(dev, "usb-otg-vbus");
		if (IS_ERR(extcon->vbus)) {
			dev_err(dev, "failed to get vbus\n");
			ret = PTR_ERR(extcon->vbus);
			extcon->vbus = NULL;
			goto fail;
		}
	}

	/* sync vbus state */
	extcon->vbus_on = regulator_is_enabled(extcon->vbus);
	dev_info(dev, "vbus is %s\n", extcon->vbus_on ? "on" : "off");

	if (!of_property_read_u32(dev->of_node, "vbus-voltage",
				&extcon->vbus_vol))
		dev_info(dev, "vbus-voltage=%d", extcon->vbus_vol);

	if (!of_property_read_u32(dev->of_node, "vbus-current",
				&extcon->vbus_cur))
		dev_info(dev, "vbus-current=%d", extcon->vbus_cur);
	if (!of_property_read_u32(dev->of_node, "vbus-limit-current",
				&extcon->vbus_limit_cur)) {
		dev_info(dev, "vbus limited current=%d", extcon->vbus_limit_cur);
		extcon->vbus_cur_inlimit = 0;
		ret = device_create_file(dev, &dev_attr_vbus_limit_cur);
		if (ret)
			dev_info(dev, "failed to create vbus currnet limit control node\n");
	}

	ret = device_create_file(dev, &dev_attr_vbus_switch);
	if (ret)
		dev_info(dev, "failed to create vbus switch node\n");

fail:
	return ret;
}

#if defined (CONFIG_W2_CHARGER_PRIVATE)
extern void set_cc_orientation(int cc_ori);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static int mtk_extcon_tcpc_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_extcon_info *extcon =
			container_of(nb, struct mtk_extcon_info, tcpc_nb);
	struct device *dev = extcon->dev;
	bool vbus_on;

#if defined(CONFIG_W2_CHARGER_PRIVATE)
	extcon->chg_dev = get_charger_by_name("primary_chg");
#endif
	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		dev_info(dev, "source vbus = %dmv\n",
				 noti->vbus_state.mv);
		vbus_on = (noti->vbus_state.mv) ? true : false;
		mtk_usb_extcon_set_vbus(extcon, vbus_on);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		dev_info(dev, "old_state=%d, new_state=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			dev_info(dev, "Type-C SRC plug in\n");
#if defined (CONFIG_W2_CHARGER_PRIVATE)
			set_cc_orientation(noti->typec_state.polarity + 1);
#endif
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
#if defined(CONFIG_W2_CHARGER_PRIVATE)
			//dev_info(dev, "[Loren]set hz flag true\n");
			//charger_dev_enable_hz(extcon->chg_dev, true);
#endif
#if defined(CONFIG_W2_CHARGER_PRIVATE)
		} else if ((extcon->bypss_typec_sink) &&
#else
		} else if (!(extcon->bypss_typec_sink) &&
#endif
			noti->typec_state.old_state == TYPEC_UNATTACHED &&
			(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			dev_info(dev, "Type-C SINK plug in\n");
#if IS_ENABLED(CONFIG_WT_PROJECT_S96818AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96818BA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96902AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901WA1)
			usb_notifier_call_chain_for_tp(USB_PLUG_IN, NULL);
#else
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
#endif
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			dev_info(dev, "Type-C plug out\n");
#if defined (CONFIG_W2_CHARGER_PRIVATE)
			set_cc_orientation(0);
#endif
			mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
#if defined(CONFIG_W2_CHARGER_PRIVATE)
			dev_info(dev, "[Loren]set hz flag false\n");
			charger_dev_enable_hz(extcon->chg_dev, false);
#endif
#if IS_ENABLED(CONFIG_WT_PROJECT_S96818AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96818BA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96902AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901WA1)
			usb_notifier_call_chain_for_tp(USB_PLUG_OUT, NULL);
#endif
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		dev_info(dev, "%s dr_swap, new role=%d\n",
				__func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_UFP &&
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
				extcon->last_dr_event != USB_ROLE_DEVICE
#else
				extcon->c_role != USB_ROLE_DEVICE
#endif
		) {			
			dev_info(dev, "switch role to device\n");
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP &&
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
				extcon->last_dr_event != USB_ROLE_HOST
#else
				extcon->c_role != USB_ROLE_HOST
#endif
			) {
			dev_info(dev, "switch role to host\n");
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		} else
			dev_info(dev, "wrong condition\n");

		break;
	}

	return NOTIFY_OK;
}

static int mtk_usb_extcon_tcpc_init(struct mtk_extcon_info *extcon)
{
	struct tcpc_device *tcpc_dev;
	struct device_node *np = extcon->dev->of_node;
	const char *tcpc_name;
	int ret;

	ret = of_property_read_string(np, "tcpc", &tcpc_name);
	if (ret < 0)
		return -ENODEV;

	tcpc_dev = tcpc_dev_get_by_name(tcpc_name);
	if (!tcpc_dev) {
		dev_err(extcon->dev, "get tcpc device fail\n");
		return -ENODEV;
	}

	extcon->tcpc_nb.notifier_call = mtk_extcon_tcpc_notifier;
	ret = register_tcp_dev_notifier(tcpc_dev, &extcon->tcpc_nb,
		TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_VBUS |
		TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		dev_err(extcon->dev, "register notifer fail\n");
		return -EINVAL;
	}

	extcon->tcpc_dev = tcpc_dev;

	return 0;
}
#endif

static void mtk_usb_extcon_detect_cable(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
		struct mtk_extcon_info, wq_detcable);
	enum usb_role role;
	int id, vbus;

	/* check ID and VBUS */
	id = extcon->id_gpiod ?
		gpiod_get_value_cansleep(extcon->id_gpiod) : 1;
	vbus = extcon->vbus_gpiod ?
		gpiod_get_value_cansleep(extcon->vbus_gpiod) : id;

	/* check if vbus detect by charger */
	if (extcon->usb_psy)
		vbus = 0;

	if (!id)
		role = USB_ROLE_HOST;
	else if (vbus)
		role = USB_ROLE_DEVICE;
	else
		role = USB_ROLE_NONE;

	dev_dbg(extcon->dev, "id %d, vbus %d, set role: %s\n",
			id, vbus, usb_role_string(role));

	if (role == USB_ROLE_HOST)
		mtk_usb_extcon_set_vbus(extcon, true);
	else
		mtk_usb_extcon_set_vbus(extcon, false);

	mtk_usb_extcon_set_role(extcon, role);
}

static irqreturn_t mtk_usb_gpio_handle(int irq, void *dev_id)
{
	struct mtk_extcon_info *extcon = dev_id;

	/* issue detection work */
	queue_delayed_work(system_power_efficient_wq, &extcon->wq_detcable, 0);

	return IRQ_HANDLED;
}

static int mtk_usb_extcon_gpio_init(struct mtk_extcon_info *extcon)
{
	struct device *dev = extcon->dev;
	int ret = 0;

	extcon->id_gpiod = devm_gpiod_get_optional(dev, "id", GPIOD_IN);
	if (IS_ERR(extcon->id_gpiod)) {
		dev_info(dev, "get id gpio err\n");
		return PTR_ERR(extcon->id_gpiod);
	}

	extcon->vbus_gpiod = devm_gpiod_get_optional(dev, "vbus", GPIOD_IN);
	if (IS_ERR(extcon->vbus_gpiod)) {
		dev_info(dev, "get vbus gpio err\n");
		return PTR_ERR(extcon->vbus_gpiod);
	}

	if (!extcon->id_gpiod && !extcon->vbus_gpiod) {
		dev_info(dev, "failed to get gpios\n");
		return -ENODEV;
	}

	if (extcon->id_gpiod)
		ret = gpiod_set_debounce(extcon->id_gpiod, USB_GPIO_DEB_US);
	if (extcon->vbus_gpiod)
		ret = gpiod_set_debounce(extcon->vbus_gpiod, USB_GPIO_DEB_US);

	INIT_DELAYED_WORK(&extcon->wq_detcable, mtk_usb_extcon_detect_cable);

	if (extcon->id_gpiod) {
		extcon->id_irq = gpiod_to_irq(extcon->id_gpiod);
		if (extcon->id_irq < 0) {
			dev_info(dev, "failed to get ID IRQ\n");
			return extcon->id_irq;
		}
		ret = devm_request_threaded_irq(dev, extcon->id_irq,
						NULL, mtk_usb_gpio_handle,
						USB_GPIO_IRQ_FLAG,
						dev_name(dev), extcon);
		if (ret < 0) {
			dev_info(dev, "failed to request ID IRQ\n");
			return ret;
		}
	}

	if (extcon->vbus_gpiod) {
		extcon->vbus_irq = gpiod_to_irq(extcon->vbus_gpiod);
		if (extcon->vbus_irq < 0) {
			dev_info(dev, "failed to get VBUS IRQ\n");
			return extcon->vbus_irq;
		}
		ret = devm_request_threaded_irq(dev, extcon->vbus_irq,
						NULL, mtk_usb_gpio_handle,
						USB_GPIO_IRQ_FLAG,
						dev_name(dev), extcon);
		if (ret < 0) {
			dev_info(dev, "failed to request VBUS IRQ\n");
			return ret;
		}
	}

	/* get id/vbus pin value when boot on */
	queue_delayed_work(system_power_efficient_wq,
			   &extcon->wq_detcable,
			   msecs_to_jiffies(10));

	return 0;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#define PROC_FILE_SMT "mtk_typec"
#define FILE_SMT_U2_CC_MODE "smt_u2_cc_mode"

static int usb_cc_smt_procfs_show(struct seq_file *s, void *unused)
{
	struct mtk_extcon_info *extcon = s->private;
	struct device_node *np = extcon->dev->of_node;
	const char *tcpc_name;
	uint8_t cc1, cc2;
	int ret;

	ret = of_property_read_string(np, "tcpc", &tcpc_name);
	if (ret < 0)
		return -ENODEV;

	extcon->tcpc_dev = tcpc_dev_get_by_name(tcpc_name);
	if (!extcon->tcpc_dev)
		return -ENODEV;

	tcpm_inquire_remote_cc(extcon->tcpc_dev, &cc1, &cc2, false);
	dev_info(extcon->dev, "cc1=%d, cc2=%d\n", cc1, cc2);

	if (cc1 == TYPEC_CC_VOLT_OPEN || cc1 == TYPEC_CC_DRP_TOGGLING)
		seq_puts(s, "0\n");
	else if (cc2 == TYPEC_CC_VOLT_OPEN || cc2 == TYPEC_CC_DRP_TOGGLING)
		seq_puts(s, "0\n");
	else
		seq_puts(s, "1\n");

	return 0;
}

static int usb_cc_smt_procfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, usb_cc_smt_procfs_show, pde_data(inode));
}

static const struct  proc_ops usb_cc_smt_procfs_fops = {
	.proc_open = usb_cc_smt_procfs_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int mtk_usb_extcon_procfs_init(struct mtk_extcon_info *extcon)
{
	struct proc_dir_entry *file, *root;
	int ret = 0;

	root = proc_mkdir(PROC_FILE_SMT, NULL);
	if (!root) {
		dev_info(extcon->dev, "fail creating proc dir: %s\n",
			PROC_FILE_SMT);
		ret = -ENOMEM;
		goto error;
	}

	file = proc_create_data(FILE_SMT_U2_CC_MODE, 0400, root,
		&usb_cc_smt_procfs_fops, extcon);
	if (!file) {
		dev_info(extcon->dev, "fail creating proc file: %s\n",
			FILE_SMT_U2_CC_MODE);
		ret = -ENOMEM;
		goto error;
	}

	dev_info(extcon->dev, "success creating proc file: %s\n",
		FILE_SMT_U2_CC_MODE);

error:
	dev_info(extcon->dev, "%s ret:%d\n", __func__, ret);
	return ret;
}
#endif

static int mtk_usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_extcon_info *extcon;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	const char *tcpc_name;
#endif
	int ret;

	extcon = devm_kzalloc(&pdev->dev, sizeof(*extcon), GFP_KERNEL);
	if (!extcon)
		return -ENOMEM;

	extcon->dev = dev;

#if defined (CONFIG_N28_CHARGER_PRIVATE)
	if (is_sd7601_chg_probe_ok || is_upm6910_chg_probe_ok) {
		pr_err("%s: primary_chg probe ok\n", __func__);
	} else {
		pr_err("%s: primary_chg probe ok\n", __func__);
		ret = -EPROBE_DEFER;
		return ret;
	}
#endif
#if defined (CONFIG_W2_CHARGER_PRIVATE)
	if (is_tcpc_mt6360_probe_ok) {
		pr_err("%s: tcpc_mt6360_probe ok\n", __func__);
	} else {
		pr_err("%s: tcpc_mt6360_probe fail\n", __func__);
		return -EPROBE_DEFER;
	}
#endif
	/* extcon */
	extcon->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(extcon->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, extcon->edev);
	if (ret < 0) {
		dev_info(dev, "failed to register extcon device\n");
		return ret;
	}

	/* usb role switch */
	extcon->role_sw = usb_role_switch_get(extcon->dev);
	if (IS_ERR(extcon->role_sw)) {
		dev_err(dev, "failed to get usb role\n");
		return PTR_ERR(extcon->role_sw);
	}

	/* initial usb role */
	if (extcon->role_sw)
		extcon->c_role = USB_ROLE_NONE;

	/* vbus */
	ret = mtk_usb_extcon_vbus_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init vbus\n");

	extcon->bypss_typec_sink =
		of_property_read_bool(dev->of_node,
			"mediatek,bypss-typec-sink");
#if defined (CONFIG_N28_CHARGER_PRIVATE)
	extcon->bypss_typec_sink = 0;
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = of_property_read_string(dev->of_node, "tcpc", &tcpc_name);
	if (of_property_read_bool(dev->of_node, "mediatek,u2") && ret == 0
		&& strcmp(tcpc_name, "type_c_port0") == 0) {
		mtk_usb_extcon_procfs_init(extcon);
	}
#endif

	extcon->extcon_wq = create_singlethread_workqueue("extcon_usb");
	if (!extcon->extcon_wq)
		return -ENOMEM;

	/* power psy */
	ret = mtk_usb_extcon_psy_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init psy\n");

	/* get id/vbus gpio resources */
	ret = mtk_usb_extcon_gpio_init(extcon);
	if (ret < 0)
		dev_info(dev, "failed to init id/vbus pin\n");

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	/* tcpc */
	ret = mtk_usb_extcon_tcpc_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init tcpc\n");
#endif

#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	g_extcon = extcon;
	register_set_peripheral(set_peripheral);
	register_set_host(set_host);
#endif

	platform_set_drvdata(pdev, extcon);

	return 0;
}

static int mtk_usb_extcon_remove(struct platform_device *pdev)
{
	return 0;
}

static void mtk_usb_extcon_shutdown(struct platform_device *pdev)
{
	struct mtk_extcon_info *extcon = platform_get_drvdata(pdev);

	dev_info(extcon->dev, "shutdown\n");

	mtk_usb_extcon_set_vbus(extcon, false);
}

static int __maybe_unused mtk_usb_extcon_suspend(struct device *dev)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (extcon->id_gpiod)
			enable_irq_wake(extcon->id_irq);
		if (extcon->vbus_gpiod)
			enable_irq_wake(extcon->vbus_irq);
		return 0;
	}

	if (extcon->id_gpiod)
		disable_irq(extcon->id_irq);
	if (extcon->vbus_gpiod)
		disable_irq(extcon->vbus_irq);

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int __maybe_unused mtk_usb_extcon_resume(struct device *dev)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (extcon->id_gpiod)
			disable_irq_wake(extcon->id_irq);
		if (extcon->vbus_gpiod)
			disable_irq_wake(extcon->vbus_irq);
		return 0;
	}

	pinctrl_pm_select_default_state(dev);

	if (extcon->id_gpiod)
		enable_irq(extcon->id_irq);
	if (extcon->vbus_gpiod)
		enable_irq(extcon->vbus_irq);

	if (extcon->id_gpiod || extcon->vbus_gpiod)
		queue_delayed_work(system_power_efficient_wq,
				   &extcon->wq_detcable, 0);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_usb_extcon_pm_ops,
			mtk_usb_extcon_suspend, mtk_usb_extcon_resume);

static const struct of_device_id mtk_usb_extcon_of_match[] = {
	{ .compatible = "mediatek,extcon-usb", },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_usb_extcon_of_match);

static struct platform_driver mtk_usb_extcon_driver = {
	.probe		= mtk_usb_extcon_probe,
	.remove		= mtk_usb_extcon_remove,
	.shutdown	= mtk_usb_extcon_shutdown,
	.driver		= {
		.name	= "mtk-extcon-usb",
		.pm	= &mtk_usb_extcon_pm_ops,
		.of_match_table = mtk_usb_extcon_of_match,
	},
};

static int __init mtk_usb_extcon_init(void)
{
	return platform_driver_register(&mtk_usb_extcon_driver);
}
late_initcall(mtk_usb_extcon_init);

static void __exit mtk_usb_extcon_exit(void)
{
	platform_driver_unregister(&mtk_usb_extcon_driver);
}
module_exit(mtk_usb_extcon_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Extcon USB Driver");
