/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

struct mtk_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;
	struct usb_role_switch *role_sw;
	unsigned int c_role; /* current data role */
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	unsigned int last_dr_event; /* Last data role event */
#endif
	struct workqueue_struct *extcon_wq;
	struct regulator *vbus;
	unsigned int vbus_vol;
	unsigned int vbus_cur;
	bool vbus_on;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	struct delayed_work wq_psy;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc_dev;
	struct notifier_block tcpc_nb;
#endif
	bool bypss_typec_sink;
	/* id/vbus gpio */
	struct gpio_desc *id_gpiod;
	struct gpio_desc *vbus_gpiod;
	int id_irq;
	int vbus_irq;
	struct delayed_work wq_detcable;
	unsigned int vbus_limit_cur;
	bool vbus_cur_inlimit;
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	struct charger_device *chg_dev;
#endif
};

struct usb_role_info {
	struct mtk_extcon_info *extcon;
	struct delayed_work dwork;
	unsigned int d_role; /* desire data role */
};

enum {
	DUAL_PROP_MODE_UFP = 0,
	DUAL_PROP_MODE_DFP,
	DUAL_PROP_MODE_NONE,
};

enum {
	DUAL_PROP_PR_SRC = 0,
	DUAL_PROP_PR_SNK,
	DUAL_PROP_PR_NONE,
};

#define USB_GPIO_DEB_US	(2000)
#define USB_GPIO_IRQ_FLAG   \
	(IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT)
