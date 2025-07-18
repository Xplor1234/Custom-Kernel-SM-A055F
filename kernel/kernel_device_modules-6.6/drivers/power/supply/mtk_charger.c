// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include "adapter_class.h"
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include <asm/setup.h>
#include <linux/thermal.h>
#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
#include "../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#include "../../gpu/drm/mediatek/mediatek_v2/mtk_panel_ext.h"
#elif IS_ENABLED(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include "mtk_charger.h"
#include "mtk_battery.h"
#include "afc_charger_intf.h"
//#include "../../misc/mediatek/thermal/inc/mach/mtk_thermal.h"
#if defined (CONFIG_N28_CHARGER_PRIVATE)
#include "pd_policy_manager.h"
extern struct cp_charging g_cp_charging;
extern struct usbpd_pm *__pdpm;
#endif

#if defined (CHARGING_THERMAL_ENABLE) || defined (CHARGING_15W_THERMAL_ENABLE)
static void lcmoff_switch(struct mtk_charger *info, int onoff);
#endif
// gujiayin.wt 20240910 add SDP/DCP/CDP/HVDCP in  88 mode
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static bool otg_state = 0;
int batt_slate_mode = 0;
EXPORT_SYMBOL(batt_slate_mode);
bool batt_store_mode = false;
EXPORT_SYMBOL(batt_store_mode);
const char * const real_charge_type_text[] = {
	[REAL_TYPE_UNKNOWN]		= "Unknown",
	[REAL_TYPE_BATTERY]		= "Battery",
	[REAL_TYPE_UPS]			= "UPS",
	[REAL_TYPE_MAINS]		= "Mains",
	[REAL_TYPE_USB]			= "USB",
	[REAL_TYPE_USB_DCP]		= "USB_DCP",
	[REAL_TYPE_USB_CDP]		= "USB_CDP",
	[REAL_TYPE_USB_ACA]		= "USB_ACA",
	[REAL_TYPE_USB_TYPE_C]		= "USB_C",
	[REAL_TYPE_USB_PD]		= "USB_PD",
	[REAL_TYPE_USB_PD_DRP]		= "USB_PD_DRP",
	[REAL_TYPE_APPLE_BRICK_ID]	= "BrickID",
	[REAL_TYPE_WIRELESS]		= "Wireless",
	[REAL_TYPE_USB_HVDCP]		= "USB_HVDCP",
	[REAL_TYPE_USB_AFC]		= "USB_AFC",
	[REAL_TYPE_USB_FLOAT]		= "USB_FLOAT",
};
bool is_mtk_pd_probe_ok = false;
EXPORT_SYMBOL(is_mtk_pd_probe_ok);
#endif
#if defined (CONFIG_W2_CHARGER_PRIVATE)
bool is_mtk_pe2_probe_ok = false;
EXPORT_SYMBOL(is_mtk_pe2_probe_ok);
extern int wt_set_batt_cycle_fv(void);
#endif
#if defined (CONFIG_N28_CHARGER_PRIVATE)
extern int wt_get_cv_by_cycle(void);
#endif

#ifdef CHARGING_THERMAL_ENABLE
#ifdef CONFIG_W2_CHARGER_PRIVATE
static int thermal_lcd_on_current[TEMP_MAX] = {3000000, 2500000, 2000000, 1000000, 600000, 300000};
static int thermal_lcd_off_current[TEMP_MAX] = {2800000, 2800000, 2350000, 1400000, 500000, 500000};
#elif defined(CONFIG_N28_CHARGER_PRIVATE)
static int thermal_lcd_on_current[TEMP_MAX] = {3000000, 2500000, 2000000, 1000000, 600000, 300000};
static int thermal_lcd_off_current[TEMP_MAX] = {2800000, 2500000, 2000000, 1400000, 500000, 500000};
static int thermal_lcd_off_cp_current[TEMP_MAX] = {5500000, 4000000, 3500000, 2500000, 1500000, 500000};
#endif
#endif

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

#ifdef MODULE
static char __chg_cmdline[COMMAND_LINE_SIZE];
static char *chg_cmdline = __chg_cmdline;

const char *chg_get_cmd(void)
{
	struct device_node *of_chosen = NULL;
	char *bootargs = NULL;

	if (__chg_cmdline[0] != 0)
		return chg_cmdline;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(
					of_chosen, "bootargs", NULL);
		if (!bootargs)
			chr_err("%s: failed to get bootargs\n", __func__);
		else {
			strcpy(__chg_cmdline, bootargs);
			chr_err("%s: bootargs: %s\n", __func__, bootargs);
		}
	} else
		chr_err("%s: failed to get /chosen\n", __func__);

	return chg_cmdline;
}

#else
const char *chg_get_cmd(void)
{
	return saved_command_line;
}
#endif

int chr_get_debug_level(void)
{
	struct power_supply *psy;
	static struct mtk_charger *info;
	int ret;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL)
			ret = CHRLOG_DEBUG_LEVEL;
		else {
			info =
			(struct mtk_charger *)power_supply_get_drvdata(psy);
			if (info == NULL)
				ret = CHRLOG_DEBUG_LEVEL;
			else
				ret = info->log_level;
		}
	} else
		ret = info->log_level;

	return ret;
}
EXPORT_SYMBOL(chr_get_debug_level);

void _wake_up_charger(struct mtk_charger *info)
{
	unsigned long flags;

	if (info == NULL)
		return;
	spin_lock_irqsave(&info->slock, flags);
	if (!info->charger_wakelock->active)
		__pm_stay_awake(info->charger_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	wake_up_interruptible(&info->wait_que);
}

bool is_disable_charger(struct mtk_charger *info)
{
	if (info == NULL)
		return true;

	if (info->disable_charger == true || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	else
		return false;
}

int _mtk_enable_charging(struct mtk_charger *info,
	bool en)
{
	chr_debug("%s en:%d\n", __func__, en);
	if (info->algo.enable_charging != NULL) {
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
		info->disable_charger = !en;
#if defined (CONFIG_N28_CHARGER_PRIVATE)
		if (en == false) {
			//g_cp_charging.cp_chg_status |= CP_STOP;
			g_cp_charging.cp_chg_status |= CP_EXIT;
			g_cp_charging.cp_chg_status |= CP_REENTER;
		} else {
			//g_cp_charging.cp_chg_status &= ~CP_STOP;
			g_cp_charging.cp_chg_status &= ~CP_EXIT;
		}
#endif
#endif
		return info->algo.enable_charging(info, en);
	}
	return false;
}

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static void mtk_inform_psy_dwork_handler(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger,
								psy_update_dwork.work);
	if (info->psy1 != NULL) {
		power_supply_changed(info->psy1);
	}
}

int charger_manager_disable_charging_new(struct mtk_charger *info,
	bool en)
{
	chr_err("%s en:%d\n", __func__, en);
	charger_dev_enable_hz(info->chg1_dev, en);
	_mtk_enable_charging(info, !en);
	info->cmd_discharging = en;
	if (info->psy1 != NULL) {
		cancel_delayed_work(&info->psy_update_dwork);
		schedule_delayed_work(&info->psy_update_dwork, msecs_to_jiffies(500));
	}
	return false;
}
EXPORT_SYMBOL(charger_manager_disable_charging_new);
#endif

int mtk_charger_notifier(struct mtk_charger *info, int event)
{
	return srcu_notifier_call_chain(&info->evt_nh, event, NULL);
}

static void mtk_charger_parse_dt(struct mtk_charger *info,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val = 0;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
#ifdef CHARGING_THERMAL_ENABLE
	int i = 0;
#endif

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		chr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			chr_err("%s: failed to get atag,boot\n", __func__);
		else {
			chr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			info->bootmode = tag->bootmode;
			info->boottype = tag->boottype;
		}
	}

	if (of_property_read_string(np, "algorithm-name",
		&info->algorithm_name) < 0) {
		if (of_property_read_string(np, "algorithm_name",
			&info->algorithm_name) < 0) {
			chr_err("%s: no algorithm_name, use Basic\n", __func__);
			info->algorithm_name = "Basic";
		}
	}

	if (strcmp(info->algorithm_name, "Basic") == 0) {
		chr_err("found Basic\n");
		mtk_basic_charger_init(info);
	} else if (strcmp(info->algorithm_name, "Pulse") == 0) {
		chr_err("found Pulse\n");
		mtk_pulse_charger_init(info);
	}

	info->disable_charger = of_property_read_bool(np, "disable_charger")
		|| of_property_read_bool(np, "disable-charger");
	info->charger_unlimited = of_property_read_bool(np, "charger_unlimited")
		|| of_property_read_bool(np, "charger-unlimited");
	info->atm_enabled = of_property_read_bool(np, "atm_is_enabled")
		|| of_property_read_bool(np, "atm-is-enabled");
	info->enable_sw_safety_timer =
			of_property_read_bool(np, "enable_sw_safety_timer")
			|| of_property_read_bool(np, "enable-sw-safety-timer");
	info->sw_safety_timer_setting = info->enable_sw_safety_timer;
	info->disable_aicl = of_property_read_bool(np, "disable_aicl")
		|| of_property_read_bool(np, "disable-aicl");
	info->alg_new_arbitration = of_property_read_bool(np, "alg_new_arbitration")
		|| of_property_read_bool(np, "alg-new-arbitration");
	info->alg_unchangeable = of_property_read_bool(np, "alg_unchangeable")
		|| of_property_read_bool(np, "alg-unchangeable");

	/* common */

	if (of_property_read_u32(np, "charger_configuration", &val) >= 0)
		info->config = val;
	else if (of_property_read_u32(np, "charger-configuration", &val) >= 0)
		info->config = val;
	else {
		chr_err("use default charger_configuration:%d\n",
			SINGLE_CHARGER);
		info->config = SINGLE_CHARGER;
	}

//+ gujiayin.wt modify 20240918 disable battery temperature protect
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	info->data.battery_cv = BATTERY_FOR_TESTING_CV;
#else
	if (of_property_read_u32(np, "battery_cv", &val) >= 0)
		info->data.battery_cv = val;
	else if (of_property_read_u32(np, "battery-cv", &val) >= 0)
		info->data.battery_cv = val;
	else {
		chr_err("use default BATTERY_CV:%d\n", BATTERY_CV);
		info->data.battery_cv = BATTERY_CV;
	}
#endif
//- gujiayin.wt modify 20240918 disable battery temperature protect

	info->enable_boot_volt =
		of_property_read_bool(np, "enable_boot_volt")
		|| of_property_read_bool(np, "enable-boot-volt");

	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else if (of_property_read_u32(np, "max-charger-voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.max_charger_voltage = V_CHARGER_MAX;
	}
	info->data.max_charger_voltage_setting = info->data.max_charger_voltage;

	if (of_property_read_u32(np, "vbus_sw_ovp_voltage", &val) >= 0)
		info->data.vbus_sw_ovp_voltage = val;
	else if (of_property_read_u32(np, "vbus-sw-ovp-voltage", &val) >= 0)
		info->data.vbus_sw_ovp_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.vbus_sw_ovp_voltage = VBUS_OVP_VOLTAGE;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else if (of_property_read_u32(np, "min-charger-voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		info->data.min_charger_voltage = V_CHARGER_MIN;
	}

	if (of_property_read_u32(np, "enable_vbat_mon", &val) >= 0) {
		info->enable_vbat_mon = val;
		info->enable_vbat_mon_bak = val;
	} else if (of_property_read_u32(np, "enable-vbat-mon", &val) >= 0) {
		info->enable_vbat_mon = val;
		info->enable_vbat_mon_bak = val;
	} else {
		chr_err("use default enable 6pin\n");
		info->enable_vbat_mon = 0;
		info->enable_vbat_mon_bak = 0;
	}
	chr_err("enable_vbat_mon:%d\n", info->enable_vbat_mon);

#ifdef CONFIG_AFC_CHARGER
	/*yuanjian.wt add for AFC ,get afc prop*/
	info->enable_afc = of_property_read_bool(np, "enable_afc");

	/* yuanjian.wt add for AFC */
	if (of_property_read_u32(np, "afc_start_battery_soc", &val) >= 0)
		info->data.afc_start_battery_soc = val;
	else {
		chr_err("use default AFC_START_BATTERY_SOC:%d\n",
			AFC_START_BATTERY_SOC);
		info->data.afc_start_battery_soc = AFC_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "afc_stop_battery_soc", &val) >= 0)
		info->data.afc_stop_battery_soc = val;
	else {
		chr_err("use default AFC_STOP_BATTERY_SOC:%d\n",
			AFC_STOP_BATTERY_SOC);
		info->data.afc_stop_battery_soc = AFC_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "afc_ichg_level_threshold", &val) >= 0)
		info->data.afc_ichg_level_threshold = val;
	else {
		chr_err("use default AFC_ICHG_LEAVE_THRESHOLD:%d\n",
			AFC_ICHG_LEAVE_THRESHOLD);
		info->data.afc_ichg_level_threshold = AFC_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "afc_pre_input_current", &val) >= 0)
		info->data.afc_pre_input_current = val;
	else {
		chr_err("use default afc_charger_input_current:%d\n",
			AFC_PRE_INPUT_CURRENT);
		info->data.afc_pre_input_current = AFC_PRE_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "afc_charger_input_current", &val) >= 0)
		info->data.afc_charger_input_current = val;
	else {
		chr_err("use default afc_charger_input_current:%d\n",
			AFC_CHARGER_INPUT_CURRENT);
		info->data.afc_charger_input_current = AFC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "afc_charger_current", &val) >= 0)
		info->data.afc_charger_current = val;
	else {
		chr_err("use default afc_charger_current:%d\n",
			AFC_CHARGER_CURRENT);
		info->data.afc_charger_current = AFC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "afc_min_charger_voltage", &val) >= 0)
		info->data.afc_min_charger_voltage = val;
	else {
		chr_err("use default afc_min_charger_voltage:%d\n",
			AFC_MIN_CHARGER_VOLTAGE);
		info->data.afc_min_charger_voltage = AFC_MIN_CHARGER_VOLTAGE;
	}

	if (of_property_read_u32(np, "afc_max_charger_voltage", &val) >= 0)
		info->data.afc_max_charger_voltage = val;
	else {
		chr_err("use default afc_min_charger_voltage:%d\n",
			AFC_MAX_CHARGER_VOLTAGE);
		info->data.afc_max_charger_voltage = AFC_MAX_CHARGER_VOLTAGE;
	}

	if (of_property_read_u32(np, "afc_common_charger_input_curr", &val) >= 0)
		info->data.afc_common_charger_input_curr = val;
	else {
		chr_err("use default afc_charger_input_current:%d\n",
			AFC_COMMON_ICL_CURR_MAX);
		info->data.afc_common_charger_input_curr = AFC_COMMON_ICL_CURR_MAX;
	}

	if (of_property_read_u32(np, "afc_common_charger_curr", &val) >= 0)
		info->data.afc_common_charger_curr = val;
	else {
		chr_err("use default afc_charger_current:%d\n",
			CHG_AFC_COMMON_CURR_MAX);
		info->data.afc_common_charger_curr = CHG_AFC_COMMON_CURR_MAX;
	}
#endif
	/* sw jeita */
//+ gujiayin.wt modify 20240918 disable battery temperature protect
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	info->enable_sw_jeita = false;
#else
	info->enable_sw_jeita = of_property_read_bool(np, "enable_sw_jeita")
		|| of_property_read_bool(np, "enable-sw-jeita");
#endif
//- gujiayin.wt modify 20240918 disable battery temperature protect

	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-above-t4-cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
			JEITA_TEMP_ABOVE_T4_CV);
		info->data.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t3-to-t4-cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
			JEITA_TEMP_T3_TO_T4_CV);
		info->data.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t2-to-t3-cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
			JEITA_TEMP_T2_TO_T3_CV);
		info->data.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t1-to-t2-cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CV:%d\n",
			JEITA_TEMP_T1_TO_T2_CV);
		info->data.jeita_temp_t1_to_t2_cv = JEITA_TEMP_T1_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t0-to-t1-cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
			JEITA_TEMP_T0_TO_T1_CV);
		info->data.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	if (of_property_read_u32(np, "jeita-temp-below-t0-cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
			JEITA_TEMP_BELOW_T0_CV);
		info->data.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_above_t4_cc", &val) >= 0)
		info->data.jeita_temp_above_t4_cc = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CC:%d\n",
			JEITA_TEMP_ABOVE_T4_CC);
		info->data.jeita_temp_above_t4_cc = JEITA_TEMP_ABOVE_T4_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cc", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CC:%d\n",
			JEITA_TEMP_T3_TO_T4_CC);
		info->data.jeita_temp_t3_to_t4_cc = JEITA_TEMP_T3_TO_T4_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cc", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CC:%d\n",
			JEITA_TEMP_T2_TO_T3_CC);
		info->data.jeita_temp_t2_to_t3_cc = JEITA_TEMP_T2_TO_T3_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cc", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CC:%d\n",
			JEITA_TEMP_T1_TO_T2_CC);
		info->data.jeita_temp_t1_to_t2_cc = JEITA_TEMP_T1_TO_T2_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cc", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CC:%d\n",
			JEITA_TEMP_T0_TO_T1_CC);
		info->data.jeita_temp_t0_to_t1_cc = JEITA_TEMP_T0_TO_T1_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cc", &val) >= 0)
		info->data.jeita_temp_below_t0_cc = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CC:%d\n",
			JEITA_TEMP_BELOW_T0_CC);
		info->data.jeita_temp_below_t0_cc= JEITA_TEMP_BELOW_T0_CC;
	}

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else if (of_property_read_u32(np, "temp-t4-thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else {
		chr_err("use default TEMP_T4_THRES:%d\n",
			TEMP_T4_THRES);
		info->data.temp_t4_thres = TEMP_T4_THRES;
	}

	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t4-thres-minus-x-degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T4_THRES_MINUS_X_DEGREE);
		info->data.temp_t4_thres_minus_x_degree =
					TEMP_T4_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else if (of_property_read_u32(np, "temp-t3-thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else {
		chr_err("use default TEMP_T3_THRES:%d\n",
			TEMP_T3_THRES);
		info->data.temp_t3_thres = TEMP_T3_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t3-thres-minus-x-degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T3_THRES_MINUS_X_DEGREE);
		info->data.temp_t3_thres_minus_x_degree =
					TEMP_T3_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else if (of_property_read_u32(np, "temp-t2-thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else {
		chr_err("use default TEMP_T2_THRES:%d\n",
			TEMP_T2_THRES);
		info->data.temp_t2_thres = TEMP_T2_THRES;
	}

	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t2-thres-plus-x-degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T2_THRES_PLUS_X_DEGREE);
		info->data.temp_t2_thres_plus_x_degree =
					TEMP_T2_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else if (of_property_read_u32(np, "temp-t1-thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else {
		chr_err("use default TEMP_T1_THRES:%d\n",
			TEMP_T1_THRES);
		info->data.temp_t1_thres = TEMP_T1_THRES;
	}

	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t1-thres-plus-x-degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T1_THRES_PLUS_X_DEGREE);
		info->data.temp_t1_thres_plus_x_degree =
					TEMP_T1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else if (of_property_read_u32(np, "temp-t0-thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else {
		chr_err("use default TEMP_T0_THRES:%d\n",
			TEMP_T0_THRES);
		info->data.temp_t0_thres = TEMP_T0_THRES;
	}

	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t0-thres-plus-x-degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T0_THRES_PLUS_X_DEGREE);
		info->data.temp_t0_thres_plus_x_degree =
					TEMP_T0_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else if (of_property_read_u32(np, "temp-neg-10-thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else {
		chr_err("use default TEMP_NEG_10_THRES:%d\n",
			TEMP_NEG_10_THRES);
		info->data.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}

	/* battery temperature protection */
	info->thermal.sm = BAT_TEMP_NORMAL;
	info->thermal.enable_min_charge_temp =
		of_property_read_bool(np, "enable_min_charge_temp")
		|| of_property_read_bool(np, "enable-min-charge-temp");

	if (of_property_read_u32(np, "min_charge_temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else if (of_property_read_u32(np, "min-charge-temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else {
		chr_err("use default MIN_CHARGE_TEMP:%d\n",
			MIN_CHARGE_TEMP);
		info->thermal.min_charge_temp = MIN_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "min_charge_temp_plus_x_degree", &val)
		>= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else if (of_property_read_u32(np, "min-charge-temp-plus-x-degree", &val)
		>= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else {
		chr_err("use default MIN_CHARGE_TEMP_PLUS_X_DEGREE:%d\n",
			MIN_CHARGE_TEMP_PLUS_X_DEGREE);
		info->thermal.min_charge_temp_plus_x_degree =
					MIN_CHARGE_TEMP_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "max_charge_temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else if (of_property_read_u32(np, "max-charge-temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else {
		chr_err("use default MAX_CHARGE_TEMP:%d\n",
			MAX_CHARGE_TEMP);
		info->thermal.max_charge_temp = MAX_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "max_charge_temp_minus_x_degree", &val)
		>= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else if (of_property_read_u32(np, "max-charge-temp-minus-x-degree", &val)
		>= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else {
		chr_err("use default MAX_CHARGE_TEMP_MINUS_X_DEGREE:%d\n",
			MAX_CHARGE_TEMP_MINUS_X_DEGREE);
		info->thermal.max_charge_temp_minus_x_degree =
					MAX_CHARGE_TEMP_MINUS_X_DEGREE;
	}

	/* charging current */
	if (of_property_read_u32(np, "usb_charger_current", &val) >= 0)
		info->data.usb_charger_current = val;
	else if (of_property_read_u32(np, "usb-charger-current", &val) >= 0)
		info->data.usb_charger_current = val;
	else {
		chr_err("use default USB_CHARGER_CURRENT:%d\n",
			USB_CHARGER_CURRENT);
		info->data.usb_charger_current = USB_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_current", &val) >= 0)
		info->data.ac_charger_current = val;
	if (of_property_read_u32(np, "ac-charger-current", &val) >= 0)
		info->data.ac_charger_current = val;
	else {
		chr_err("use default AC_CHARGER_CURRENT:%d\n",
			AC_CHARGER_CURRENT);
		info->data.ac_charger_current = AC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_input_current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else if (of_property_read_u32(np, "ac-charger-input-current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else {
		chr_err("use default AC_CHARGER_INPUT_CURRENT:%d\n",
			AC_CHARGER_INPUT_CURRENT);
		info->data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "charging_host_charger_current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else if (of_property_read_u32(np, "charging-host-charger-current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
					CHARGING_HOST_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "pe2_charger_input_current", &val) >= 0)
		info->data.pe2_charger_input_current = val;
	if (of_property_read_u32(np, "pe2_charger_current", &val) >= 0)
		info->data.pe2_charger_current = val;

	/* dynamic mivr */
	info->enable_dynamic_mivr =
			of_property_read_bool(np, "enable_dynamic_mivr")
			|| of_property_read_bool(np, "enable-dynamic-mivr");

	if (of_property_read_u32(np, "min_charger_voltage_1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else if (of_property_read_u32(np, "min-charger-voltage-1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else {
		chr_err("use default V_CHARGER_MIN_1: %d\n", V_CHARGER_MIN_1);
		info->data.min_charger_voltage_1 = V_CHARGER_MIN_1;
	}

	if (of_property_read_u32(np, "min_charger_voltage_2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else if (of_property_read_u32(np, "min-charger-voltage-2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else {
		chr_err("use default V_CHARGER_MIN_2: %d\n", V_CHARGER_MIN_2);
		info->data.min_charger_voltage_2 = V_CHARGER_MIN_2;
	}

	if (of_property_read_u32(np, "max_dmivr_charger_current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else if (of_property_read_u32(np, "max-dmivr-charger-current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else {
		chr_err("use default MAX_DMIVR_CHARGER_CURRENT: %d\n",
			MAX_DMIVR_CHARGER_CURRENT);
		info->data.max_dmivr_charger_current =
					MAX_DMIVR_CHARGER_CURRENT;
	}
	/* fast charging algo support indicator */
	info->enable_fast_charging_indicator =
			of_property_read_bool(np, "enable_fast_charging_indicator")
			|| of_property_read_bool(np, "enable-fast-charging-indicator");

	/*	adapter priority */
	if (of_property_read_u32(np, "adapter-priority", &val)>= 0)
		info->setting.adapter_priority = val;

	/*	dual parallel battery*/
	np = of_parse_phandle(dev->of_node, "current-selector", 0);
	if (np) {
		info->cs_gpio_index = of_get_named_gpio(dev->of_node, "cs-gpios", 0);
		if (of_property_read_string(np, "cs-name",
			&info->curr_select_name) < 0) {
			chr_err("%s: no cs-name\n", __func__);
			info->curr_select_name = "NULL";
		}
		info->cs_with_gauge =
			of_property_read_bool(np, "cs-gauge");
		chr_err("%s: %d\n", __func__, info->cs_with_gauge);
		if (of_property_read_u32(np, "comp-resist", &val) >= 0)
			info->comp_resist = val;
		else
			info->comp_resist = 25;
	} else {
		chr_err("%s: failed to get current_selector\n", __func__);
		info->cs_hw_disable = true;
		info->curr_select_name = "NULL";
	}

#ifdef CHARGING_THERMAL_ENABLE
	//common charge
	//lcm off
	info->data.ap_temp_t3_thres = AP_TEMP_T3_THRES;
	info->data.ap_temp_t3_thres_minus_x_degree = AP_TEMP_T3_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t2_thres = AP_TEMP_T2_THRES;
	info->data.ap_temp_t2_thres_minus_x_degree = AP_TEMP_T2_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t1_thres = AP_TEMP_T1_THRES;
	info->data.ap_temp_t1_thres_minus_x_degree = AP_TEMP_T1_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t0_thres = AP_TEMP_T0_THRES;
	info->data.ap_temp_t0_thres_minus_x_degree = AP_TEMP_T0_THRES_MINUS_X_DEGREE;
	//lcm on
	info->data.ap_temp_lcmon_t4 = AP_TEMP_LCMON_T4;
	info->data.ap_temp_lcmon_t4_anti_shake = AP_TEMP_LCMON_T4_ANTI_SHAKE;
	info->data.ap_temp_lcmon_t3 = AP_TEMP_LCMON_T3;
	info->data.ap_temp_lcmon_t3_anti_shake = AP_TEMP_LCMON_T3_ANTI_SHAKE;
	info->data.ap_temp_lcmon_t2 = AP_TEMP_LCMON_T2;
	info->data.ap_temp_lcmon_t2_anti_shake = AP_TEMP_LCMON_T2_ANTI_SHAKE;
	info->data.ap_temp_lcmon_t1 = AP_TEMP_LCMON_T1;
	info->data.ap_temp_lcmon_t1_anti_shake = AP_TEMP_LCMON_T1_ANTI_SHAKE;
	info->data.ap_temp_lcmon_t0 = AP_TEMP_LCMON_T0;
	info->data.ap_temp_lcmon_t0_anti_shake = AP_TEMP_LCMON_T0_ANTI_SHAKE;

	for (i = 0; i < TEMP_MAX; i++) {
		info->data.lcd_on_ibat[i] = thermal_lcd_on_current[i];
		chr_err("%s: lcd_on_ibat[%d]=%d\n", __func__, i, thermal_lcd_on_current[i]);
	}

	for (i = 0; i < TEMP_MAX; i++) {
		info->data.lcd_off_ibat[i] = thermal_lcd_off_current[i];
		chr_err("%s: lcd_off_ibat[%d]=%d\n", __func__, i, thermal_lcd_off_current[i]);
	}
#ifdef CONFIG_N28_CHARGER_PRIVATE
	//fast charge
	//lcm off
	info->data.ap_temp_t4_cp_thres = AP_TEMP_T4_CP_THRES;
	info->data.ap_temp_t4_cp_thres_minus_x_degree = AP_TEMP_T4_CP_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t3_cp_thres = AP_TEMP_T3_CP_THRES;
	info->data.ap_temp_t3_cp_thres_minus_x_degree = AP_TEMP_T3_CP_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t2_cp_thres = AP_TEMP_T2_CP_THRES;
	info->data.ap_temp_t2_cp_thres_minus_x_degree = AP_TEMP_T2_CP_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t1_cp_thres = AP_TEMP_T1_CP_THRES;
	info->data.ap_temp_t1_cp_thres_minus_x_degree = AP_TEMP_T1_CP_THRES_MINUS_X_DEGREE;
	info->data.ap_temp_t0_cp_thres = AP_TEMP_T0_CP_THRES;
	info->data.ap_temp_t0_cp_thres_minus_x_degree = AP_TEMP_T0_CP_THRES_MINUS_X_DEGREE;

	for (i = 0; i < TEMP_MAX; i++) {
		info->data.lcd_off_cp_ibat[i] = thermal_lcd_off_cp_current[i];
		chr_err("%s: lcd_off_cp_ibat[%d]=%d\n", __func__, i, thermal_lcd_off_cp_current[i]);
	}
#endif
#endif
}

static void mtk_charger_start_timer(struct mtk_charger *info)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&info->charger_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n", __func__);
		return;
	}

	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	end_time.tv_sec = time_now.tv_sec + info->polling_interval;
	end_time.tv_nsec = time_now.tv_nsec + 0;
	info->endtime = end_time;
	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	chr_err("%s: alarm timer start:%d, %lld %ld\n", __func__, ret,
		info->endtime.tv_sec, info->endtime.tv_nsec);
	alarm_start(&info->charger_timer, ktime);
}

static void check_battery_exist(struct mtk_charger *info)
{
	unsigned int i = 0;
	int count = 0;
	//int boot_mode = get_boot_mode();

	if (is_disable_charger(info))
		return;

	for (i = 0; i < 3; i++) {
		if (is_battery_exist(info) == false) {
			count++;
			chr_debug("%s: %d\n", __func__, count);
		}
	}

#ifdef FIXME
	if (count >= 3) {
		if (boot_mode == META_BOOT || boot_mode == ADVMETA_BOOT ||
		    boot_mode == ATE_FACTORY_BOOT)
			chr_info("boot_mode = %d, bypass battery check\n",
				boot_mode);
		else {
			chr_err("battery doesn't exist, shutdown\n");
			orderly_poweroff(true);
		}
	}
#endif
}

static void check_dynamic_mivr(struct mtk_charger *info)
{
	int i = 0, ret = 0;
#if !defined (CONFIG_N28_CHARGER_PRIVATE)
	int vbat = 0;
#endif
	bool is_fast_charge = false;
	struct chg_alg_device *alg = NULL;

	if (!info->enable_dynamic_mivr)
		return;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_fast_charge = true;
			break;
		}
	}

#ifdef CONFIG_AFC_CHARGER
	if ((info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) || afc_get_is_connect(info))
		is_fast_charge = true;
#else
	if (info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30)
		is_fast_charge = true;
#endif

#ifdef CONFIG_N28_CHARGER_PRIVATE
	if (adapter_is_support_pd_pps(info) && !(g_cp_charging.cp_chg_status & CP_EXIT)
		&& !(g_cp_charging.cp_chg_status & CP_DONE))
		is_fast_charge = true;
	//else
		//is_fast_charge = false;
#endif

	if (!is_fast_charge) {
#if !defined (CONFIG_N28_CHARGER_PRIVATE)
		vbat = get_battery_voltage(info);
		if (vbat < info->data.min_charger_voltage_2 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_2);
		else if (vbat < info->data.min_charger_voltage_1 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_1);
		else
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage);
#else
		charger_dev_set_mivr(info->chg1_dev,
			info->data.min_charger_voltage);
#endif
	}
}

/* sw jeita */
void sw_jeita_state_machine_init(struct mtk_charger *info)
{
	struct sw_jeita_data *sw_jeita;

	if (info->enable_sw_jeita == true) {
		sw_jeita = &info->sw_jeita;
		info->battery_temp = get_battery_temperature(info);

		if (info->battery_temp >= info->data.temp_t4_thres)
			sw_jeita->sm = TEMP_ABOVE_T4;
		else if (info->battery_temp > info->data.temp_t3_thres)
			sw_jeita->sm = TEMP_T3_TO_T4;
		else if (info->battery_temp >= info->data.temp_t2_thres)
			sw_jeita->sm = TEMP_T2_TO_T3;
		else if (info->battery_temp >= info->data.temp_t1_thres)
			sw_jeita->sm = TEMP_T1_TO_T2;
		else if (info->battery_temp >= info->data.temp_t0_thres)
			sw_jeita->sm = TEMP_T0_TO_T1;
		else
			sw_jeita->sm = TEMP_BELOW_T0;

		chr_err("[SW_JEITA] tmp:%d sm:%d\n",
			info->battery_temp, sw_jeita->sm);
	}
}

/* sw jeita */
void do_sw_jeita_state_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *sw_jeita;
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	int cycle_fv;
#endif

	sw_jeita = &info->sw_jeita;
	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;

	/* JEITA battery temp Standard */
	if (info->battery_temp >= info->data.temp_t4_thres) {
		chr_err("[SW_JEITA] Battery Over high Temperature(%d) !!\n",
			info->data.temp_t4_thres);

		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (info->battery_temp >= info->data.temp_t3_thres) {
		/* control 45 degree to normal behavior */
		if ((sw_jeita->sm == TEMP_ABOVE_T4)
		    && (info->battery_temp
			>= info->data.temp_t4_thres_minus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t4_thres_minus_x_degree,
				info->data.temp_t4_thres);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t3_thres,
				info->data.temp_t4_thres);

			sw_jeita->sm = TEMP_T3_TO_T4;
		}
	} else if (info->battery_temp >= info->data.temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4)
		     && (info->battery_temp
			 >= info->data.temp_t3_thres_minus_x_degree))
		    || ((sw_jeita->sm == TEMP_T1_TO_T2)
			&& (info->battery_temp
			    <= info->data.temp_t2_thres_plus_x_degree))) {
			chr_err("[SW_JEITA] Battery Temperature not recovery to normal temperature charging mode yet!!\n");
		} else {
			chr_err("[SW_JEITA] Battery Normal Temperature between %d and %d !!\n",
				info->data.temp_t2_thres,
				info->data.temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
		}
	} else if (info->battery_temp >= info->data.temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1
		     || sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t1_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T0_TO_T1) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
					info->data.temp_t1_thres,
					info->data.temp_t1_thres_plus_x_degree);
			}
			#if 0
			if (sw_jeita->sm == TEMP_BELOW_T0) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
					info->data.temp_t1_thres,
					info->data.temp_t1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
			#endif
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t1_thres,
				info->data.temp_t2_thres);

			sw_jeita->sm = TEMP_T1_TO_T2;
		}
	} else if (info->battery_temp >= info->data.temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t0_thres_plus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t0_thres,
				info->data.temp_t0_thres_plus_x_degree);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t0_thres,
				info->data.temp_t1_thres);

			sw_jeita->sm = TEMP_T0_TO_T1;
		}
	} else {
		chr_err("[SW_JEITA] Battery below low Temperature(%d) !!\n",
			info->data.temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (sw_jeita->sm == TEMP_ABOVE_T4) {
		sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
		sw_jeita->cc = info->data.jeita_temp_above_t4_cc;
	}
	else if (sw_jeita->sm == TEMP_T3_TO_T4) {
		sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
		sw_jeita->cc = info->data.jeita_temp_t3_to_t4_cc;
	}
	else if (sw_jeita->sm == TEMP_T2_TO_T3) {
		sw_jeita->cv = info->data.jeita_temp_t2_to_t3_cv;
		sw_jeita->cc = info->data.jeita_temp_t2_to_t3_cc;
	}
	else if (sw_jeita->sm == TEMP_T1_TO_T2) {
		sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
		sw_jeita->cc = info->data.jeita_temp_t1_to_t2_cc;
	}
	else if (sw_jeita->sm == TEMP_T0_TO_T1) {
		sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
		sw_jeita->cc = info->data.jeita_temp_t0_to_t1_cc;
	}
	else if (sw_jeita->sm == TEMP_BELOW_T0) {
		sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
		sw_jeita->cc = info->data.jeita_temp_below_t0_cc;
	}
	else {
		sw_jeita->cv = info->data.battery_cv;
		sw_jeita->cc = info->data.usb_charger_current;
	}

	if (info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		if (sw_jeita->sm == TEMP_T3_TO_T4) {
			sw_jeita->cc = APDO_CHARGER_T3_TO_T4_CURRENT;
		}
		else if (sw_jeita->sm == TEMP_T2_TO_T3) {
			sw_jeita->cc = APDO_CHARGER_T2_TO_T3_CURRENT;
		}
	} else if ((info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30)
		|| afc_get_is_connect(info)) {
		if ((sw_jeita->sm == TEMP_T3_TO_T4) || (sw_jeita->sm == TEMP_T2_TO_T3)) {
			sw_jeita->cc = FIX_PDO_CHARGER_CURRENT;
		}
	}

#if defined (CONFIG_W2_CHARGER_PRIVATE)
	cycle_fv = wt_set_batt_cycle_fv();
	if ((cycle_fv != 0) && (sw_jeita->cv > cycle_fv)) {
		sw_jeita->cv = cycle_fv;
	}
#endif
#if defined (CONFIG_N28_CHARGER_PRIVATE)
	cycle_fv = wt_get_cv_by_cycle();
	if ((cycle_fv != 0) && (sw_jeita->cv > cycle_fv)) {
		sw_jeita->cv = cycle_fv;
	}
	g_cp_charging.cp_jeita_cc = sw_jeita->cc;
	g_cp_charging.cp_jeita_cv = sw_jeita->cv;
#endif
	chr_err("[SW_JEITA]preState:%d newState:%d tmp:%d cv:%d cc:%d ta_type:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
		sw_jeita->cv, sw_jeita->cc, info->ta_type);
}

static int mtk_chgstat_notify(struct mtk_charger *info)
{
	int ret = 0;
	char *env[2] = { "CHGSTAT=1", NULL };

	chr_err("%s: 0x%x\n", __func__, info->notify_code);
	ret = kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, env);
	if (ret)
		chr_err("%s: kobject_uevent_fail, ret=%d", __func__, ret);

	return ret;
}

static void mtk_charger_set_algo_log_level(struct mtk_charger *info, int level)
{
	struct chg_alg_device *alg;
	int i = 0, ret = 0;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_set_prop(alg, ALG_LOG_LEVEL, level);
		if (ret < 0)
			chr_err("%s: set ALG_LOG_LEVEL fail, ret =%d", __func__, ret);
	}
}

static ssize_t sw_jeita_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t sw_jeita_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_sw_jeita = false;
		else
			pinfo->enable_sw_jeita = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_jeita);
/* sw jeita end*/

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static ssize_t show_ap_thermal(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->ap_temp);
	return sprintf(buf, "%d\n", pinfo->ap_temp);
}

static ssize_t store_ap_thermal(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		pinfo->ap_temp = temp;
	} else {
		chr_err("%s: format error!\n", __func__);
	}
	chr_err("%s: %d\n", __func__, pinfo->ap_temp);
	return size;
}

static DEVICE_ATTR(ap_thermal, 0664, show_ap_thermal,
		   store_ap_thermal);
#endif

static ssize_t sw_ovp_threshold_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->data.max_charger_voltage);
	return sprintf(buf, "%d\n", pinfo->data.max_charger_voltage);
}

static ssize_t sw_ovp_threshold_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0)
			pinfo->data.max_charger_voltage = pinfo->data.vbus_sw_ovp_voltage;
		else
			pinfo->data.max_charger_voltage = temp;
		chr_err("%s: %d\n", __func__, pinfo->data.max_charger_voltage);

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_ovp_threshold);

static ssize_t chr_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->chr_type);
	return sprintf(buf, "%d\n", pinfo->chr_type);
}

static ssize_t chr_type_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0)
		pinfo->chr_type = temp;
	else
		chr_err("%s: format error!\n", __func__);

	return size;
}

static DEVICE_ATTR_RW(chr_type);

static ssize_t ta_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	char *ta_type_name = "None";
	int ta_type = MTK_CAP_TYPE_UNKNOWN;

	ta_type = adapter_dev_get_property(pinfo->select_adapter, CAP_TYPE);
	switch (ta_type) {
	case MTK_CAP_TYPE_UNKNOWN:
		ta_type_name = "None";
		break;
	case MTK_PD:
		ta_type_name = "PD";
		break;
	case MTK_UFCS:
		ta_type_name = "UFCS";
		break;
	case MTK_PD_APDO:
		ta_type_name = "PD with PPS";
		break;
	}
	chr_err("%s: %d\n", __func__, ta_type);
	return sprintf(buf, "%s\n", ta_type_name);
}

static DEVICE_ATTR_RO(ta_type);


static ssize_t Pump_Express_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0, i = 0;
	bool is_ta_detected = false;
	struct mtk_charger *pinfo = dev->driver_data;
	struct chg_alg_device *alg = NULL;

	if (!pinfo) {
		chr_err("%s: pinfo is null\n", __func__);
		return sprintf(buf, "%d\n", is_ta_detected);
	}

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = pinfo->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_ta_detected = true;
			break;
		}
	}
	chr_err("%s: idx = %d, detect = %d\n", __func__, i, is_ta_detected);
	return sprintf(buf, "%d\n", is_ta_detected);
}

static DEVICE_ATTR_RO(Pump_Express);

static ssize_t Charging_mode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0, i = 0;
	char *alg_name = "normal";
	bool is_ta_detected = false;
	struct mtk_charger *pinfo = dev->driver_data;
	struct chg_alg_device *alg = NULL;

	if (!pinfo) {
		chr_err("%s: pinfo is null\n", __func__);
		return sprintf(buf, "%d\n", is_ta_detected);
	}

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = pinfo->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_ta_detected = true;
			break;
		}
	}
	if (alg == NULL)
		return sprintf(buf, "%s\n", alg_name);

	switch (alg->alg_id) {
	case PE_ID:
		alg_name = "PE";
		break;
	case PE2_ID:
		alg_name = "PE2";
		break;
	case PDC_ID:
		alg_name = "PDC";
		break;
	case PE4_ID:
		alg_name = "PE4";
		break;
	case PE5_ID:
		alg_name = "P5";
		break;
	case PE5P_ID:
		alg_name = "P5P";
		break;
	}
	chr_err("%s: charging_mode: %s\n", __func__, alg_name);
	return sprintf(buf, "%s\n", alg_name);
}

static DEVICE_ATTR_RO(Charging_mode);

static ssize_t High_voltage_chg_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: hv_charging = %d\n", __func__, pinfo->enable_hv_charging);
	return sprintf(buf, "%d\n", pinfo->enable_hv_charging);
}

static DEVICE_ATTR_RO(High_voltage_chg_enable);

static ssize_t Rust_detect_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: Rust detect = %d\n", __func__, pinfo->record_water_detected);
	return sprintf(buf, "%d\n", pinfo->record_water_detected);
}

static DEVICE_ATTR_RO(Rust_detect);

static ssize_t Thermal_throttle_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	struct charger_data *chg_data = &(pinfo->chg_data[CHG1_SETTING]);

	return sprintf(buf, "%d\n", chg_data->thermal_throttle_record);
}

static DEVICE_ATTR_RO(Thermal_throttle);

static ssize_t fast_chg_indicator_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->fast_charging_indicator);
	return sprintf(buf, "%d\n", pinfo->fast_charging_indicator);
}

static ssize_t fast_chg_indicator_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->fast_charging_indicator = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if ((pinfo->fast_charging_indicator > 0) &&
	    (pinfo->bootmode == 8 || pinfo->bootmode == 9)) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(fast_chg_indicator);

static ssize_t alg_new_arbitration_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->alg_new_arbitration);
	return sprintf(buf, "%d\n", pinfo->alg_new_arbitration);
}

static ssize_t alg_new_arbitration_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->alg_new_arbitration = temp;
	else
		chr_err("%s: format error!\n", __func__);

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(alg_new_arbitration);

static ssize_t alg_unchangeable_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->alg_unchangeable);
	return sprintf(buf, "%d\n", pinfo->alg_unchangeable);
}

static ssize_t alg_unchangeable_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->alg_unchangeable = temp;
	else
		chr_err("%s: format error!\n", __func__);

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(alg_unchangeable);

static ssize_t enable_meta_current_limit_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_meta_current_limit);
	return sprintf(buf, "%d\n", pinfo->enable_meta_current_limit);
}

static ssize_t enable_meta_current_limit_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->enable_meta_current_limit = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if (pinfo->enable_meta_current_limit > 0) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(enable_meta_current_limit);

static ssize_t cs_heatlim_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->cs_heatlim);
	return sprintf(buf, "%d\n", pinfo->cs_heatlim);
}

static ssize_t cs_heatlim_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->cs_heatlim = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if (pinfo->cs_heatlim > 0) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(cs_heatlim);

static ssize_t cs_para_mode_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->cs_para_mode);
	return sprintf(buf, "%d\n", pinfo->cs_para_mode);
}

static ssize_t cs_para_mode_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->cs_para_mode = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if (pinfo->cs_para_mode > 0) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(cs_para_mode);

static ssize_t vbat_mon_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_vbat_mon);
	return sprintf(buf, "%d\n", pinfo->enable_vbat_mon);
}

static ssize_t vbat_mon_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_vbat_mon = false;
		else
			pinfo->enable_vbat_mon = true;
	} else {
		chr_err("%s: format error!\n", __func__);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(vbat_mon);

static ssize_t ADC_Charger_Voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int vbus = get_vbus(pinfo); /* mV */

	chr_err("%s: %d\n", __func__, vbus);
	return sprintf(buf, "%d\n", vbus);
}

static DEVICE_ATTR_RO(ADC_Charger_Voltage);

static ssize_t ADC_Charging_Current_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int ibat = get_battery_current(pinfo); /* mA */

	chr_err("%s: %d\n", __func__, ibat);
	return sprintf(buf, "%d\n", ibat);
}

static DEVICE_ATTR_RO(ADC_Charging_Current);

static ssize_t input_current_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int aicr = 0;

	aicr = pinfo->chg_data[CHG1_SETTING].thermal_input_current_limit;
	chr_err("%s: %d\n", __func__, aicr);
	return sprintf(buf, "%d\n", aicr);
}

static ssize_t input_current_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	struct charger_data *chg_data;
	signed int temp;

	chg_data = &pinfo->chg_data[CHG1_SETTING];
	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0)
			chg_data->thermal_input_current_limit = 0;
		else
			chg_data->thermal_input_current_limit = temp;
	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(input_current);

static ssize_t charger_log_level_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->log_level);
	return sprintf(buf, "%d\n", pinfo->log_level);
}

static ssize_t charger_log_level_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0) {
			chr_err("%s: val is invalid: %d\n", __func__, temp);
			temp = 0;
		}
		pinfo->log_level = temp;
		chr_err("%s: log_level=%d\n", __func__, pinfo->log_level);

		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
		_wake_up_charger(pinfo);

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(charger_log_level);

static ssize_t BatteryNotify_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_info("%s: 0x%x\n", __func__, pinfo->notify_code);

	return sprintf(buf, "%u\n", pinfo->notify_code);
}

static ssize_t BatteryNotify_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret = 0;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 16, &reg);
		if (ret < 0) {
			chr_err("%s: failed, ret = %d\n", __func__, ret);
			return ret;
		}
		pinfo->notify_code = reg;
		chr_info("%s: store code=0x%x\n", __func__, pinfo->notify_code);
		mtk_chgstat_notify(pinfo);
	}
	return size;
}

static DEVICE_ATTR_RW(BatteryNotify);

/* procfs */
static int mtk_chg_set_cv_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d\n", pinfo->data.battery_cv);
	return 0;
}

static int mtk_chg_set_cv_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_set_cv_show, pde_data(node));
}

static ssize_t mtk_chg_set_cv_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int cv = 0;
	struct mtk_charger *info = pde_data(file_inode(file));
	struct power_supply *psy = NULL;
	union  power_supply_propval dynamic_cv;

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &cv);
	if (ret == 0) {
		if (cv >= BATTERY_CV) {
			info->data.battery_cv = BATTERY_CV;
			chr_info("%s: adjust charge voltage %dV too high, use default cv\n",
				  __func__, cv);
		} else {
			info->data.battery_cv = cv;
			chr_info("%s: adjust charge voltage = %dV\n", __func__, cv);
		}
		psy = power_supply_get_by_name("battery");
		if (!IS_ERR_OR_NULL(psy)) {
			dynamic_cv.intval = info->data.battery_cv;
			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &dynamic_cv);
			if (ret < 0)
				chr_err("set gauge cv fail\n");
		}
		return count;
	}

	chr_err("%s: bad argument\n", __func__);
	return count;
}

static const struct proc_ops mtk_chg_set_cv_fops = {
	.proc_open = mtk_chg_set_cv_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_set_cv_write,
};

static int mtk_chg_current_cmd_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d %d\n", pinfo->usb_unlimited, pinfo->cmd_discharging);
	return 0;
}

static int mtk_chg_current_cmd_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_current_cmd_show, pde_data(node));
}

static ssize_t mtk_chg_current_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32] = {0};
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct mtk_charger *info = pde_data(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) == 2) {
		info->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev,
					EVENT_DISCHARGE, 0);
		} else if (cmd_discharging == 0) {
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev,
					EVENT_RECHARGE, 0);
		}

		chr_info("%s: current_unlimited=%d, cmd_discharging=%d\n",
			__func__, current_unlimited, cmd_discharging);
		return count;
	}

	chr_err("bad argument, echo [usb_unlimited] [disable] > current_cmd\n");
	return count;
}

static const struct proc_ops mtk_chg_current_cmd_fops = {
	.proc_open = mtk_chg_current_cmd_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_current_cmd_write,
};

static int mtk_chg_en_power_path_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool power_path_en = true;

	charger_dev_is_powerpath_enabled(pinfo->chg1_dev, &power_path_en);
	seq_printf(m, "%d\n", power_path_en);

	return 0;
}

static int mtk_chg_en_power_path_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_power_path_show, pde_data(node));
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct mtk_charger *info = pde_data(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_powerpath(info->chg1_dev, enable);
		chr_info("%s: enable power path = %d\n", __func__, enable);
		return count;
	}

	chr_err("bad argument, echo [enable] > en_power_path\n");
	return count;
}

static const struct proc_ops mtk_chg_en_power_path_fops = {
	.proc_open = mtk_chg_en_power_path_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_power_path_write,
};

static int mtk_chg_en_safety_timer_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool safety_timer_en = false;

	charger_dev_is_safety_timer_enabled(pinfo->chg1_dev, &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);

	return 0;
}

static int mtk_chg_en_safety_timer_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_safety_timer_show, pde_data(node));
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct mtk_charger *info = pde_data(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_safety_timer(info->chg1_dev, enable);
		info->safety_timer_cmd = (int)enable;
		chr_info("%s: enable safety timer = %d\n", __func__, enable);

		/* SW safety timer */
		if (info->sw_safety_timer_setting == true) {
			if (enable)
				info->enable_sw_safety_timer = true;
			else
				info->enable_sw_safety_timer = false;
		}

		return count;
	}

	chr_err("bad argument, echo [enable] > en_safety_timer\n");
	return count;
}

static const struct proc_ops mtk_chg_en_safety_timer_fops = {
	.proc_open = mtk_chg_en_safety_timer_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_safety_timer_write,
};

int sc_get_sys_time(void)
{
	struct rtc_time tm_android = {0};
	struct timespec64 tv_android = {0};
	int timep = 0;

	ktime_get_real_ts64(&tv_android);
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	timep = tm_android.tm_sec + tm_android.tm_min * 60 + tm_android.tm_hour * 3600;

	return timep;
}

int sc_get_left_time(int s, int e, int now)
{
	if (e >= s) {
		if (now >= s && now < e)
			return e-now;
	} else {
		if (now >= s)
			return 86400 - now + e;
		else if (now < e)
			return e-now;
	}
	return 0;
}

char *sc_solToStr(int s)
{
	switch (s) {
	case SC_IGNORE:
		return "ignore";
	case SC_KEEP:
		return "keep";
	case SC_DISABLE:
		return "disable";
	case SC_REDUCE:
		return "reduce";
	default:
		return "none";
	}
}

int smart_charging(struct mtk_charger *info)
{
	int time_to_target = 0;
	int time_to_full_default_current = -1;
	int time_to_full_default_current_limit = -1;
	int ret_value = SC_KEEP;
	int sc_real_time = sc_get_sys_time();
	int sc_left_time = sc_get_left_time(info->sc.start_time, info->sc.end_time, sc_real_time);
	int sc_battery_percentage = get_uisoc(info) * 100;
	int sc_charger_current = get_battery_current(info);

	time_to_target = sc_left_time - info->sc.left_time_for_cv;

	if (info->sc.enable == false || sc_left_time <= 0
		|| sc_left_time < info->sc.left_time_for_cv
		|| (sc_charger_current <= 0 && info->sc.last_solution != SC_DISABLE))
		ret_value = SC_IGNORE;
	else {
		if (sc_battery_percentage > info->sc.target_percentage * 100) {
			if (time_to_target > 0)
				ret_value = SC_DISABLE;
		} else {
			if (sc_charger_current != 0)
				time_to_full_default_current =
					info->sc.battery_size * 3600 / 10000 *
					(10000 - sc_battery_percentage)
						/ sc_charger_current;
			else
				time_to_full_default_current =
					info->sc.battery_size * 3600 / 10000 *
					(10000 - sc_battery_percentage);
			chr_err("sc1: %d %d %d %d %d\n",
				time_to_full_default_current,
				info->sc.battery_size,
				sc_battery_percentage,
				sc_charger_current,
				info->sc.current_limit);

			if (time_to_full_default_current < time_to_target &&
				info->sc.current_limit != -1 &&
				sc_charger_current > info->sc.current_limit) {
				time_to_full_default_current_limit =
					info->sc.battery_size / 10000 *
					(10000 - sc_battery_percentage)
					/ info->sc.current_limit;

				chr_err("sc2: %d %d %d %d\n",
					time_to_full_default_current_limit,
					info->sc.battery_size,
					sc_battery_percentage,
					info->sc.current_limit);

				if (time_to_full_default_current_limit < time_to_target &&
					sc_charger_current > info->sc.current_limit)
					ret_value = SC_REDUCE;
			}
		}
	}
	info->sc.last_solution = ret_value;
	if (info->sc.last_solution == SC_DISABLE)
		info->sc.disable_charger = true;
	else
		info->sc.disable_charger = false;
	chr_err("[sc]disable_charger: %d\n", info->sc.disable_charger);
	chr_err("[sc1]en:%d t:%d,%d,%d,%d t:%d,%d,%d,%d c:%d,%d ibus:%d uisoc: %d,%d s:%d ans:%s\n",
		info->sc.enable, info->sc.start_time, info->sc.end_time,
		sc_real_time, sc_left_time, info->sc.left_time_for_cv,
		time_to_target, time_to_full_default_current, time_to_full_default_current_limit,
		sc_charger_current, info->sc.current_limit,
		get_ibus(info), get_uisoc(info), info->sc.target_percentage,
		info->sc.battery_size, sc_solToStr(info->sc.last_solution));

	return ret_value;
}

//N28 thermal bring up
#if 0
void sc_select_charging_current(struct mtk_charger *info, struct charger_data *pdata)
{
	if (info->bootmode == 4 || info->bootmode == 1
		|| info->bootmode == 8 || info->bootmode == 9) {
		info->sc.sc_ibat = -1;	/* not normal boot */
		return;
	}
	info->sc.solution = info->sc.last_solution;
	chr_debug("debug: %d, %d, %d\n", info->bootmode,
		info->sc.disable_in_this_plug, info->sc.solution);
	if (info->sc.disable_in_this_plug == false) {
		chr_debug("sck: %d %d %d %d %d\n",
			info->sc.pre_ibat,
			info->sc.sc_ibat,
			pdata->charging_current_limit,
			pdata->thermal_charging_current_limit,
			info->sc.solution);
		if (info->sc.pre_ibat == -1 || info->sc.solution == SC_IGNORE
			|| info->sc.solution == SC_DISABLE) {
			info->sc.sc_ibat = -1;
		} else {
			if (info->sc.pre_ibat == pdata->charging_current_limit
				&& info->sc.solution == SC_REDUCE
				&& ((pdata->charging_current_limit - 100000) >= 500000)) {
				if (info->sc.sc_ibat == -1)
					info->sc.sc_ibat = pdata->charging_current_limit - 100000;

				else {
					if (info->sc.sc_ibat - 100000 >= 500000)
						info->sc.sc_ibat = info->sc.sc_ibat - 100000;
					else
						info->sc.sc_ibat = 500000;
				}
			}
		}
	}
	info->sc.pre_ibat = pdata->charging_current_limit;

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
		info->sc.disable_in_this_plug = true;
	} else if ((info->sc.solution == SC_REDUCE || info->sc.solution == SC_KEEP)
		&& info->sc.sc_ibat <
		pdata->charging_current_limit &&
		info->sc.disable_in_this_plug == false) {
		pdata->charging_current_limit = info->sc.sc_ibat;
	}
}
#endif

void sc_init(struct smartcharging *sc)
{
	sc->enable = false;
	sc->battery_size = 3000;
	sc->start_time = 0;
	sc->end_time = 80000;
	sc->current_limit = 2000;
	sc->target_percentage = 80;
	sc->left_time_for_cv = 3600;
	sc->pre_ibat = -1;
}

static ssize_t enable_sc_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[enable smartcharging] : %d\n",
	info->sc.enable);

	return sprintf(buf, "%d\n", info->sc.enable);
}

static ssize_t enable_sc_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[enable smartcharging] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val == 0)
			info->sc.enable = false;
		else
			info->sc.enable = true;

		chr_err(
			"[enable smartcharging]enable smartcharging=%d\n",
			info->sc.enable);
	}
	return size;
}
static DEVICE_ATTR_RW(enable_sc);

static ssize_t sc_stime_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging stime] : %d\n",
	info->sc.start_time);

	return sprintf(buf, "%d\n", info->sc.start_time);
}

static ssize_t sc_stime_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging stime] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging stime] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.start_time = (int)val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			info->sc.start_time);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_stime);

static ssize_t sc_etime_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging etime] : %d\n",
	info->sc.end_time);

	return sprintf(buf, "%d\n", info->sc.end_time);
}

static ssize_t sc_etime_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging etime] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging etime] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.end_time = (int)val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			info->sc.end_time);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_etime);

static ssize_t sc_tuisoc_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging target uisoc] : %d\n",
	info->sc.target_percentage);

	return sprintf(buf, "%d\n", info->sc.target_percentage);
}

static ssize_t sc_tuisoc_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging tuisoc] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging tuisoc] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.target_percentage = (int)val;

		chr_err(
			"[smartcharging stime]tuisoc=%d\n",
			info->sc.target_percentage);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_tuisoc);

static ssize_t sc_ibat_limit_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging ibat limit] : %d\n",
	info->sc.current_limit);

	return sprintf(buf, "%d\n", info->sc.current_limit);
}

static ssize_t sc_ibat_limit_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging ibat limit] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging ibat limit] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.current_limit = (int)val;

		chr_err(
			"[smartcharging ibat limit]=%d\n",
			info->sc.current_limit);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_ibat_limit);

static ssize_t enable_power_path_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;
	bool power_path_en = true;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	charger_dev_is_powerpath_enabled(info->chg1_dev, &power_path_en);
	return sprintf(buf, "%d\n", power_path_en);
}

static ssize_t enable_power_path_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	bool enable = true;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		ret = kstrtoul(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val == 0)
			enable = false;
		else
			enable = true;

		charger_dev_enable_powerpath(info->chg1_dev, enable);
		info->cmd_pp = enable;
		chr_err("%s: enable power path = %d\n", __func__, enable);
	}

	return size;
}
static DEVICE_ATTR_RW(enable_power_path);

int mtk_chg_enable_vbus_ovp(bool enable)
{
	static struct mtk_charger *pinfo;
	int ret = 0;
	u32 sw_ovp = 0;
	struct power_supply *psy;

	if (pinfo == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			chr_err("[%s]psy is not rdy\n", __func__);
			return -1;
		}

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("[%s]mtk_gauge is not rdy\n", __func__);
			return -1;
		}
	}

	if (enable)
		sw_ovp = pinfo->data.max_charger_voltage_setting;
	else
		sw_ovp = pinfo->data.vbus_sw_ovp_voltage;

	/* Enable/Disable SW OVP status */
	pinfo->data.max_charger_voltage = sw_ovp;

	disable_hw_ovp(pinfo, enable);

	chr_err("[%s] en:%d ovp:%d\n",
			    __func__, enable, sw_ovp);
	return ret;
}
EXPORT_SYMBOL(mtk_chg_enable_vbus_ovp);

/* return false if vbus is over max_charger_voltage */
static bool mtk_chg_check_vbus(struct mtk_charger *info)
{
	int vchr = 0;

	vchr = get_vbus(info) * 1000;
	if (vchr > info->data.max_charger_voltage) {
		chr_err("%s: vbus(%d mV) > %d mV\n", __func__, vchr / 1000,
			info->data.max_charger_voltage / 1000);
		return false;
	}
	return true;
}

static void mtk_battery_notify_VCharger_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	int vchr = 0;

	vchr = get_vbus(info) * 1000; /* uV */
	if (vchr < info->data.max_charger_voltage)
		info->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		info->notify_code |= CHG_VBUS_OV_STATUS;
		chr_err("[BATTERY] charger_vol(%d mV) > %d mV\n",
			vchr / 1000, info->data.max_charger_voltage / 1000);
		mtk_chgstat_notify(info);
	}
#endif
}

static void mtk_battery_notify_VBatTemp_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)
	if (info->battery_temp >= info->thermal.max_charge_temp) {
		info->notify_code |= CHG_BAT_OT_STATUS;
		chr_err("[BATTERY] bat_temp(%d) out of range(too high)\n",
			info->battery_temp);
		mtk_chgstat_notify(info);
	} else {
		info->notify_code &= ~CHG_BAT_OT_STATUS;
	}

	if (info->enable_sw_jeita == true) {
		if (info->battery_temp < info->data.temp_neg_10_thres) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if (info->battery_temp < info->thermal.min_charge_temp) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
#endif
	}
#endif
}

static void mtk_battery_notify_VChargerDPDM_check(struct mtk_charger *info)
{
	if (!info->dpdmov_stat)
		info->notify_code &= ~CHG_DPDM_OV_STATUS;
	else {
		info->notify_code |= CHG_DPDM_OV_STATUS;
		chr_err("[BATTERY] DP/DM over voltage!\n");
	}
	if (info->dpdmov_stat != info->lst_dpdmov_stat) {
		mtk_chgstat_notify(info);
		info->lst_dpdmov_stat = info->dpdmov_stat;
	}
}

static void mtk_battery_notify_UI_test(struct mtk_charger *info)
{
	switch (info->notify_test_mode) {
	case 1:
		info->notify_code = CHG_VBUS_OV_STATUS;
		chr_debug("[%s] CASE_0001_VCHARGER\n", __func__);
		break;
	case 2:
		info->notify_code = CHG_BAT_OT_STATUS;
		chr_debug("[%s] CASE_0002_VBATTEMP\n", __func__);
		break;
	case 3:
		info->notify_code = CHG_OC_STATUS;
		chr_debug("[%s] CASE_0003_ICHARGING\n", __func__);
		break;
	case 4:
		info->notify_code = CHG_BAT_OV_STATUS;
		chr_debug("[%s] CASE_0004_VBAT\n", __func__);
		break;
	case 5:
		info->notify_code = CHG_ST_TMO_STATUS;
		chr_debug("[%s] CASE_0005_TOTAL_CHARGINGTIME\n", __func__);
		break;
	case 6:
		info->notify_code = CHG_BAT_LT_STATUS;
		chr_debug("[%s] CASE6: VBATTEMP_LOW\n", __func__);
		break;
	case 7:
		info->notify_code = CHG_TYPEC_WD_STATUS;
		chr_debug("[%s] CASE7: Moisture Detection\n", __func__);
		break;
	default:
		chr_debug("[%s] Unknown BN_TestMode Code: %x\n",
			__func__, info->notify_test_mode);
	}
	mtk_chgstat_notify(info);
}

static void mtk_battery_notify_check(struct mtk_charger *info)
{
	if (info->notify_test_mode == 0x0000) {
		mtk_battery_notify_VCharger_check(info);
		mtk_battery_notify_VBatTemp_check(info);
		mtk_battery_notify_VChargerDPDM_check(info);
	} else {
		mtk_battery_notify_UI_test(info);
	}
}

static void mtk_chg_get_tchg(struct mtk_charger *info)
{
	int ret;
	int tchg_min = -127, tchg_max = -127;
	struct charger_data *pdata;

	pdata = &info->chg_data[CHG1_SETTING];
	ret = charger_dev_get_temperature(info->chg1_dev, &tchg_min, &tchg_max);
	if (ret < 0) {
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
	} else {
		pdata->junction_temp_min = tchg_min;
		pdata->junction_temp_max = tchg_max;
	}

	if (info->chg2_dev) {
		pdata = &info->chg_data[CHG2_SETTING];
		ret = charger_dev_get_temperature(info->chg2_dev,
			&tchg_min, &tchg_max);

		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->dvchg1_dev) {
		pdata = &info->chg_data[DVCHG1_SETTING];
		ret = charger_dev_get_adc(info->dvchg1_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->dvchg2_dev) {
		pdata = &info->chg_data[DVCHG2_SETTING];
		ret = charger_dev_get_adc(info->dvchg2_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->hvdvchg1_dev) {
		pdata = &info->chg_data[HVDVCHG1_SETTING];
		ret = charger_dev_get_adc(info->hvdvchg1_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->hvdvchg2_dev) {
		pdata = &info->chg_data[HVDVCHG2_SETTING];
		ret = charger_dev_get_adc(info->hvdvchg2_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}
}

static int mtkts_bts_get_hw_temp(void)
{
	struct thermal_zone_device *tz;
	int temp = 0;
	tz = thermal_zone_get_zone_by_name("mtktsAP");

	if(!IS_ERR(tz)) {
		thermal_zone_get_temp(tz, &temp);
	} else {
		temp = 30000;
		pr_err("read mtktsAP fail\n");
	}
	return temp;
}

#ifdef CHARGING_THERMAL_ENABLE
int get_lcm_off_thermal_level(int ap_temp, struct mtk_charger *info, struct sw_jeita_data *thermal_data)
{
	if (ap_temp >= info->data.ap_temp_t3_thres) { //46
		chr_err("[AP_THERMAL] AP Over %d Temperature!!\n", info->data.ap_temp_t3_thres);

		thermal_data->sm = TEMP_T3_TO_T4; //above T3
	} else if (ap_temp >= info->data.ap_temp_t2_thres) { //42
		if ((thermal_data->sm >= TEMP_T3_TO_T4)
			&& (ap_temp >= info->data.ap_temp_t3_thres_minus_x_degree)) {
			chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
				info->data.ap_temp_t3_thres_minus_x_degree,
				info->data.ap_temp_t3_thres);
			thermal_data->sm = TEMP_T3_TO_T4;
		} else {
			chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t2_thres,
				info->data.ap_temp_t3_thres_minus_x_degree);

			thermal_data->sm = TEMP_T2_TO_T3;
		}
	} else if (ap_temp >= info->data.ap_temp_t1_thres) { //38
		if ((thermal_data->sm >= TEMP_T2_TO_T3)
			&& (ap_temp >= info->data.ap_temp_t2_thres_minus_x_degree)){
			chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
				info->data.ap_temp_t2_thres_minus_x_degree,
				info->data.ap_temp_t2_thres);
			thermal_data->sm = TEMP_T2_TO_T3;
		} else {
			chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t1_thres,
				info->data.ap_temp_t2_thres_minus_x_degree);
			thermal_data->sm = TEMP_T1_TO_T2;
		}
	} else if (ap_temp >= info->data.ap_temp_t0_thres) { //36
		if ((thermal_data->sm >= TEMP_T1_TO_T2)
			&& (ap_temp >= info->data.ap_temp_t1_thres_minus_x_degree)){
			chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
				info->data.ap_temp_t1_thres_minus_x_degree,
				info->data.ap_temp_t1_thres);
			thermal_data->sm = TEMP_T1_TO_T2;
		} else {
			chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t0_thres,
				info->data.ap_temp_t1_thres_minus_x_degree);
			thermal_data->sm = TEMP_T0_TO_T1;
		}
	} else {
		if ((thermal_data->sm >= TEMP_T0_TO_T1)
			&& (ap_temp >= info->data.ap_temp_t0_thres_minus_x_degree)) {
			chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t0_thres_minus_x_degree,
				info->data.ap_temp_t0_thres);
			thermal_data->sm = TEMP_T0_TO_T1;
		} else {
			chr_err("[AP_THERMAL] AP below Temperature(%d) !!\n",
				info->data.ap_temp_t0_thres);
			thermal_data->sm = TEMP_BELOW_T0;
		}
	}

	return thermal_data->sm;
}

#ifdef CONFIG_N28_CHARGER_PRIVATE
int get_lcm_off_thermal_level_of_cp(int ap_temp, struct mtk_charger *info, struct sw_jeita_data *thermal_data)
{
	if (ap_temp >= info->data.ap_temp_t4_cp_thres) { //48
		chr_err("[AP_THERMAL_CP] AP Over %d Temperature!!\n", info->data.ap_temp_t4_cp_thres);

		thermal_data->sm = TEMP_ABOVE_T4;
	} else if (ap_temp >= info->data.ap_temp_t3_cp_thres) { //45
		if ((thermal_data->sm == TEMP_ABOVE_T4)
			&& (ap_temp >= info->data.ap_temp_t4_cp_thres_minus_x_degree)) {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d!!\n",
			info->data.ap_temp_t4_cp_thres_minus_x_degree,
			info->data.ap_temp_t4_cp_thres);
		} else {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d !!\n",
			info->data.ap_temp_t3_cp_thres,
			info->data.ap_temp_t4_cp_thres_minus_x_degree);

			thermal_data->sm = TEMP_T3_TO_T4;
		}
	} else if (ap_temp >= info->data.ap_temp_t2_cp_thres) { //39
		if ((thermal_data->sm >= TEMP_T3_TO_T4)
			&& (ap_temp >= info->data.ap_temp_t3_cp_thres_minus_x_degree)) {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d!!\n",
				info->data.ap_temp_t3_cp_thres_minus_x_degree,
				info->data.ap_temp_t3_thres);
			thermal_data->sm = TEMP_T3_TO_T4;
		} else {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t2_cp_thres,
				info->data.ap_temp_t3_cp_thres_minus_x_degree);

			thermal_data->sm = TEMP_T2_TO_T3;
		}
	} else if (ap_temp >= info->data.ap_temp_t1_cp_thres) { //37
		if ((thermal_data->sm >= TEMP_T2_TO_T3)
			&& (ap_temp >= info->data.ap_temp_t2_cp_thres_minus_x_degree)){
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d!!\n",
				info->data.ap_temp_t2_cp_thres_minus_x_degree,
				info->data.ap_temp_t2_thres);
			thermal_data->sm = TEMP_T2_TO_T3;
		} else {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t1_cp_thres,
				info->data.ap_temp_t2_cp_thres_minus_x_degree);
			thermal_data->sm = TEMP_T1_TO_T2;
		}
	} else if (ap_temp >= info->data.ap_temp_t0_cp_thres) { //35
		if ((thermal_data->sm >= TEMP_T1_TO_T2)
			&& (ap_temp >= info->data.ap_temp_t1_cp_thres_minus_x_degree)){
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d!!\n",
				info->data.ap_temp_t1_cp_thres_minus_x_degree,
				info->data.ap_temp_t1_thres);
			thermal_data->sm = TEMP_T1_TO_T2;
		} else {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t0_cp_thres,
				info->data.ap_temp_t1_cp_thres_minus_x_degree);
			thermal_data->sm = TEMP_T0_TO_T1;
		}
	} else {
		if ((thermal_data->sm >= TEMP_T0_TO_T1)
			&& (ap_temp >= info->data.ap_temp_t0_cp_thres_minus_x_degree)) {
			chr_err("[AP_THERMAL_CP] AP Temperature between %d and %d !!\n",
				info->data.ap_temp_t0_cp_thres_minus_x_degree,
				info->data.ap_temp_t0_cp_thres);
			thermal_data->sm = TEMP_T0_TO_T1;
		} else {
			chr_err("[AP_THERMAL_CP] AP below Temperature(%d) !!\n",
				info->data.ap_temp_t0_cp_thres);
			thermal_data->sm = TEMP_BELOW_T0;
		}
	}

	return thermal_data->sm;
}
#endif

void ap_thermal_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *thermal_data;
	struct charger_data *pdata = &info->chg_data[CHG1_SETTING];
	int ap_temp;
	u32 boot_mode = info->bootmode;

	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if((8 == boot_mode) || (9 == boot_mode)) {
		lcmoff_switch(info, 0);
		chr_err("[AP_THERMAL]lcmoff_switch off!!\n");
	}

	if(info->ap_temp != 0xffff) {
		ap_temp = info->ap_temp;
	} else {
		ap_temp = mtkts_bts_get_hw_temp();
		ap_temp = ap_temp / 1000;
	}

	chr_err("[AP_THERMAL]: lcmonff=%d, ap_temp=%d\n", info->lcmoff, ap_temp);

	thermal_data = &info->ap_thermal_data;
	thermal_data->pre_sm = thermal_data->sm;

	if (info->lcmoff == 1) {
#ifdef CONFIG_N28_CHARGER_PRIVATE
	if (adapter_is_support_pd_pps(info) && !(g_cp_charging.cp_chg_status & CP_EXIT))
		thermal_data->sm = get_lcm_off_thermal_level_of_cp(ap_temp, info, thermal_data);
	else
		thermal_data->sm = get_lcm_off_thermal_level(ap_temp, info, thermal_data);
#else //including W2
	thermal_data->sm = get_lcm_off_thermal_level(ap_temp, info, thermal_data);
#endif
	} else {
		if (ap_temp >= info->data.ap_temp_lcmon_t4) {  //45
			chr_err("[AP_THERMAL] AP Over %d Temperature!!\n", info->data.ap_temp_lcmon_t4);

			thermal_data->sm = TEMP_ABOVE_T4; //above T4
		} else if (ap_temp >= info->data.ap_temp_lcmon_t3) { //43
			if ((thermal_data->sm == TEMP_ABOVE_T4)
				&& (ap_temp >= info->data.ap_temp_lcmon_t4_anti_shake)) {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
					info->data.ap_temp_lcmon_t4_anti_shake,
					info->data.ap_temp_lcmon_t4);
			} else {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_lcmon_t3,
					info->data.ap_temp_lcmon_t4_anti_shake);

				thermal_data->sm = TEMP_T3_TO_T4;
			}
		} else if (ap_temp >= info->data.ap_temp_lcmon_t2) { //42
			if ((thermal_data->sm >= TEMP_T3_TO_T4)
				&& (ap_temp >= info->data.ap_temp_lcmon_t3_anti_shake)) {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
					info->data.ap_temp_lcmon_t3_anti_shake,
					info->data.ap_temp_lcmon_t3);
				thermal_data->sm = TEMP_T3_TO_T4;
			} else {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_lcmon_t2,
					info->data.ap_temp_lcmon_t3_anti_shake);

				thermal_data->sm = TEMP_T2_TO_T3;
			}
		} else if (ap_temp >= info->data.ap_temp_lcmon_t1) { //41
			if ((thermal_data->sm >= TEMP_T2_TO_T3)
				&& (ap_temp >= info->data.ap_temp_lcmon_t2_anti_shake)){
				chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
					info->data.ap_temp_lcmon_t2_anti_shake,
					info->data.ap_temp_lcmon_t2);
				thermal_data->sm = TEMP_T2_TO_T3;
			} else {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_lcmon_t1,
					info->data.ap_temp_lcmon_t2_anti_shake);
				thermal_data->sm = TEMP_T1_TO_T2;
			}
		} else if (ap_temp >= info->data.ap_temp_lcmon_t0) { //39
			if ((thermal_data->sm >= TEMP_T1_TO_T2)
				&& (ap_temp >= info->data.ap_temp_lcmon_t1_anti_shake)){
				chr_err("[AP_THERMAL] AP Temperature between %d and %d!!\n",
					info->data.ap_temp_lcmon_t1_anti_shake,
					info->data.ap_temp_lcmon_t1);
				thermal_data->sm = TEMP_T1_TO_T2;
			} else {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_lcmon_t0,
					info->data.ap_temp_lcmon_t1_anti_shake);
				thermal_data->sm = TEMP_T0_TO_T1;
			}
		} else {
			if ((thermal_data->sm >= TEMP_T0_TO_T1)
				&& (ap_temp >= info->data.ap_temp_lcmon_t0_anti_shake)) {
				chr_err("[AP_THERMAL] AP Temperature between %d and %d !!\n",
					info->data.ap_temp_lcmon_t0_anti_shake,
					info->data.ap_temp_lcmon_t0);
				thermal_data->sm = TEMP_T0_TO_T1;
			} else {
				chr_err("[AP_THERMAL] AP below Temperature(%d) !!\n",
					info->data.ap_temp_lcmon_t0);
				thermal_data->sm = TEMP_BELOW_T0;
			}
		}
	}
	if(thermal_data->sm >= TEMP_MAX) {
		thermal_data->sm = TEMP_MAX - 1;
	}

#if defined (CONFIG_N28_CHARGER_PRIVATE)
	if (info->lcmoff == 1) {
		if (adapter_is_support_pd_pps(info) && !(g_cp_charging.cp_chg_status & CP_EXIT))
			thermal_data->cc = info->data.lcd_off_cp_ibat[thermal_data->sm];
		else
			thermal_data->cc = info->data.lcd_off_ibat[thermal_data->sm];
	} else {
		thermal_data->cc = info->data.lcd_on_ibat[thermal_data->sm];
		switch(thermal_data->sm) {
		case TEMP_T1_TO_T2:
		case TEMP_T0_TO_T1:
		case TEMP_BELOW_T0:
			if (info->battery_temp > TEMP_T1_THRES_PLUS_X_DEGREE &&
				info->enable_hv_charging == true) {
				g_cp_charging.cp_chg_status &= ~CP_EXIT;
			} else {
				g_cp_charging.cp_chg_status |= CP_EXIT;
				g_cp_charging.cp_chg_status |= CP_REENTER;
			}
			break;
		case TEMP_T2_TO_T3:
		case TEMP_T3_TO_T4:
		case TEMP_ABOVE_T4:
		default:
			g_cp_charging.cp_chg_status |= CP_EXIT;
			g_cp_charging.cp_chg_status |= CP_REENTER;
			break;
		}
	}
	g_cp_charging.cp_therm_cc = thermal_data->cc;

	if (adapter_is_support_pd_pps(info)) {
		if (info->lcmoff == 1) {
			chr_err("enable_hv_charging=%d, ap_temp=%d, battery_temp=%d \n",
				info->enable_hv_charging, ap_temp, info->battery_temp);
			if (info->enable_hv_charging == true && //set hv_disable
				ap_temp <= AP_TEMP_T4_CP_THRES && //AP temp higher than 48 degree
				info->battery_temp > TEMP_T1_THRES_PLUS_X_DEGREE) { //batt temp lower than 5 degree
				g_cp_charging.cp_chg_status &= ~CP_EXIT;
			} else {
				g_cp_charging.cp_chg_status |= CP_EXIT;
				g_cp_charging.cp_chg_status |= CP_REENTER;
				chr_err("lcd is off temp higher than 48 exit CP charging\n");
			}
		}
		chr_err("CP_REENTER=%d,CP_DONE=%d,CP_EXIT=%d\n", g_cp_charging.cp_chg_status & CP_EXIT,
			g_cp_charging.cp_chg_status & CP_DONE, g_cp_charging.cp_chg_status & CP_REENTER);

		if (!info->disable_quick_charge
			&& !(g_cp_charging.cp_chg_status & CP_EXIT)
			&& !(g_cp_charging.cp_chg_status & CP_DONE)) {
			if (g_cp_charging.cp_chg_status & CP_REENTER) {
				g_cp_charging.cp_chg_status &= ~CP_REENTER;
				if (__pdpm != NULL) {
					cancel_work_sync(&__pdpm->usb_psy_change_work);
					schedule_work(&__pdpm->usb_psy_change_work);
					chr_err("CP Reenter!\n");
				}
			}
			pdata->input_current_limit = 100000; /* 100mA */
			if (info->is_enable_term) {
				chr_err("disable buck charge term\n");
				charger_dev_set_enable_term(info->chg1_dev, false);
				info->is_enable_term = false;
			}
		}

		if (!info->is_enable_term) {
			if ((g_cp_charging.cp_chg_status & CP_EXIT)
				|| (g_cp_charging.cp_chg_status & CP_DONE)
				|| info->disable_quick_charge) {
				chr_err("enable buck charge term\n");
				charger_dev_set_enable_term(info->chg1_dev, true);
				info->is_enable_term = true;
			}
		}
	}
#else //including W2
	if (info->lcmoff == 1) {
		thermal_data->cc = info->data.lcd_off_ibat[thermal_data->sm];
	} else {
		thermal_data->cc = info->data.lcd_on_ibat[thermal_data->sm];
	}
#endif

	chr_err("[AP_THERMAL]lcd is %s!! preState:%d newState:%d tmp:%d cc:%d\n",
			(info->lcmoff == 1) ? "off" : "on", thermal_data->pre_sm,
			thermal_data->sm, ap_temp, thermal_data->cc);
}
EXPORT_SYMBOL_GPL(ap_thermal_machine);
#endif

#ifdef CHARGING_15W_THERMAL_ENABLE
void adjust_15w_ap_thermal_current(struct mtk_charger *info)
{
	u32 boot_mode = info->bootmode;
	struct charger_data *pdata = &info->chg_data[CHG1_SETTING];
	int ta_type = MTK_CAP_TYPE_UNKNOWN;

	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if((8 == boot_mode) || (9 == boot_mode)) {
		lcmoff_switch(info, 1);
		chr_err("[AP_THERMAL]lcmoff_switch on!!\n");
	}

	ta_type = adapter_dev_get_property(info->select_adapter, CAP_TYPE);
	if ((info->lcmoff == 1)
		&& ((ta_type == MTK_PD)
		|| afc_get_is_connect(info))) {
		if (atomic_read(&info->thermal_current_update)) {
			atomic_set(&info->thermal_current_update, 0);
			if (pdata->thermal_charging_current_limit >=
				THERMAL_CURRENT_LCMOFF_T0) {
				pdata->thermal_charging_current_limit =
					THERMAL_15W_CURRENT_LCMOFF_T0;
			} else if (pdata->thermal_charging_current_limit >=
				THERMAL_CURRENT_LCMOFF_T3) {
				pdata->thermal_charging_current_limit =
					THERMAL_15W_CURRENT_LCMOFF_T3;
			} else if (pdata->thermal_charging_current_limit >=
				THERMAL_CURRENT_LCMOFF_T4) {
				pdata->thermal_charging_current_limit =
					THERMAL_15W_CURRENT_LCMOFF_T4;
			}
		} else {
			//do nothing, maintain thermal_charging_current_limit
		}
	} else {
		pdata->thermal_charging_current_limit =
			info->old_thermal_charging_current_limit;
	}

	chr_err("ADJUST_15W_THERMAL:lcd is %s, oldcc:%d newcc:%d update:%d\n",
			(info->lcmoff == 1) ? "off" : "on",
			info->old_thermal_charging_current_limit,
			pdata->thermal_charging_current_limit,
			atomic_read(&info->thermal_current_update));
}
#endif

static void charger_check_status(struct mtk_charger *info)
{
	bool charging = true;
	bool chg_dev_chgen = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;
	int uisoc = 0;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN)
		return;

	temperature = info->battery_temp;
	thermal = &info->thermal;
	uisoc = get_uisoc(info);

	info->setting.vbat_mon_en = true;
	if (info->enable_sw_jeita == true || info->enable_vbat_mon != true ||
	    info->batpro_done == true)
		info->setting.vbat_mon_en = false;

	if (info->enable_sw_jeita == true) {
		do_sw_jeita_state_machine(info);
		if (info->sw_jeita.charging == false) {
			charging = false;
			goto stop_charging;
		}
	} else {

		if (thermal->enable_min_charge_temp) {
			if (temperature < thermal->min_charge_temp) {
				chr_err("Battery Under Temperature or NTC fail %d %d\n",
					temperature, thermal->min_charge_temp);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (temperature >=
				    thermal->min_charge_temp_plus_x_degree) {
					chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
					thermal->min_charge_temp,
					temperature,
					thermal->min_charge_temp_plus_x_degree);
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}

		if (temperature >= thermal->max_charge_temp) {
			chr_err("Battery over Temperature or NTC fail %d %d\n",
				temperature, thermal->max_charge_temp);
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (temperature
			    < thermal->max_charge_temp_minus_x_degree) {
				chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
				thermal->max_charge_temp,
				temperature,
				thermal->max_charge_temp_minus_x_degree);
				thermal->sm = BAT_TEMP_NORMAL;
			} else {
				charging = false;
				goto stop_charging;
			}
		}
	}
#ifdef CHARGING_15W_THERMAL_ENABLE
	adjust_15w_ap_thermal_current(info);
#endif
	mtk_chg_get_tchg(info);

	if (!mtk_chg_check_vbus(info)) {
		charging = false;
		goto stop_charging;
	}

	if (info->cmd_discharging)
		charging = false;
	if (info->safety_timeout)
		charging = false;
	if (info->vbusov_stat)
		charging = false;
	if (info->dpdmov_stat)
		charging = false;
	if (info->sc.disable_charger == true)
		charging = false;
stop_charging:
	mtk_battery_notify_check(info);

	if (charging && uisoc < 80 && info->batpro_done == true) {
		info->setting.vbat_mon_en = true;
		info->batpro_done = false;
		info->stop_6pin_re_en = false;
	}

	chr_err("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d c:%d s:%d ov:%d %d sc:%d %d %d saf_cmd:%d bat_mon:%d %d\n",
		temperature, info->enable_sw_jeita, info->sw_jeita.sm,
		info->sw_jeita.cv, info->sw_jeita.charging, thermal->sm,
		charging, info->cmd_discharging, info->safety_timeout,
		info->vbusov_stat, info->dpdmov_stat, info->sc.disable_charger,
		info->can_charging, charging, info->safety_timer_cmd,
		info->enable_vbat_mon, info->batpro_done);

	charger_dev_is_enabled(info->chg1_dev, &chg_dev_chgen);

	if (charging != info->can_charging)
		_mtk_enable_charging(info, charging);
	else if (charging == false && chg_dev_chgen == true)
		_mtk_enable_charging(info, charging);

	info->can_charging = charging;
}

static bool charger_init_algo(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int idx = 0;
	int ret = 0;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("%s, Found primary charger\n", __func__);
	else {
		chr_err("%s, *** Error : can't find primary charger ***\n"
			, __func__);
		return false;
	}

	chr_err("%s, start current_selector init flow: %s\n", __func__, info->curr_select_name);
	if (strcmp(info->curr_select_name, "current_selector_master") == 0)
		info->cschg1_dev = get_charger_by_name("current_selector_master");

	if (info->cschg1_dev) {
		chr_err("%s, Found main current selector charger\n", __func__);
		ret = charger_cs_init_setting(info->cschg1_dev);
		if (ret < 0) {
			chr_err("%s, failed to init cs, close cs function\n", __func__);
			info->cs_hw_disable = true;
		} else {
			ret = charger_dev_set_constant_voltage(info->cschg1_dev, 4350);
		if (ret < 0)
			chr_err("%s: failed to set cs1 cv to: 4350mV.\n", __func__);
		ret = charger_dev_set_charging_current(info->cschg1_dev, AC_CS_NORMAL_CC);
		if (ret < 0)
			chr_err("%s: failed to set cs1 cc to: %d mA.\n", __func__, AC_CS_NORMAL_CC);
		}
		info->cs_cc_now = AC_CS_NORMAL_CC;
	} else {
		chr_err("%s, *** Warning : can't find main current selector charger ***\n"
			, __func__);
	}

	alg = get_chg_alg_by_name("pe5p");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe5p fail\n");
	else {
		chr_err("get pe5p success\n");
		alg->config = info->config;
		alg->alg_id = PE5P_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("hvbp");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get hvbp fail\n");
	else {
		chr_err("get hvbp success\n");
		alg->config = info->config;
		alg->alg_id = HVBP_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe5");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe5 fail\n");
	else {
		chr_err("get pe5 success\n");
		alg->config = info->config;
		alg->alg_id = PE5_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe45");
	if (alg == NULL) {
		chr_err("cannot get pe45\n");
		alg = get_chg_alg_by_name("pe4");
		info->alg[idx] = alg;
		if (alg == NULL)
			chr_err("cannot get pe4\n");
		else {
			chr_err("get pe4 success\n");
			alg->config = info->config;
			alg->alg_id = PE4_ID;
			chg_alg_init_algo(alg);
			register_chg_alg_notifier(alg, &info->chg_alg_nb);
		}
	} else {
		info->alg[idx] = alg;
		chr_err("get pe45 success\n");
		alg->config = info->config;
		alg->alg_id = PE4_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}

	idx++;

	alg = get_chg_alg_by_name("pd");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pd fail\n");
	else {
		chr_err("get pd success\n");
		alg->config = info->config;
		alg->alg_id = PDC_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe2");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe2 fail\n");
	else {
		chr_err("get pe2 success\n");
		alg->config = info->config;
		alg->alg_id = PE2_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe fail\n");
	else {
		chr_err("get pe success\n");
		alg->config = info->config;
		alg->alg_id = PE_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}

	chr_err("config is %d\n", info->config);
	if (info->config == DUAL_CHARGERS_IN_SERIES) {
		info->chg2_dev = get_charger_by_name("secondary_chg");
		if (info->chg2_dev)
			chr_err("Found secondary charger\n");
		else {
			chr_err("*** Error : can't find secondary charger ***\n");
			return false;
		}
	} else if (info->config == DIVIDER_CHARGER ||
		   info->config == DUAL_DIVIDER_CHARGERS) {
		info->dvchg1_dev = get_charger_by_name("primary_dvchg");
		if (info->dvchg1_dev)
			chr_err("Found primary divider charger\n");
		else {
			chr_err("*** Error : can't find primary divider charger ***\n");
			return false;
		}
		if (info->config == DUAL_DIVIDER_CHARGERS) {
			info->dvchg2_dev =
				get_charger_by_name("secondary_dvchg");
			if (info->dvchg2_dev)
				chr_err("Found secondary divider charger\n");
			else {
				chr_err("*** Error : can't find secondary divider charger ***\n");
				return false;
			}
		}
	} else if (info->config == HVDIVIDER_CHARGER ||
		   info->config == DUAL_HVDIVIDER_CHARGERS) {
		info->hvdvchg1_dev = get_charger_by_name("hvdiv2_chg1");
		if (info->hvdvchg1_dev)
			chr_err("Found primary hvdivider charger\n");
		else {
			chr_err("*** Error : can't find primary hvdivider charger ***\n");
			return false;
		}
		if (info->config == DUAL_HVDIVIDER_CHARGERS) {
			info->hvdvchg2_dev = get_charger_by_name("hvdiv2_chg2");
			if (info->hvdvchg2_dev)
				chr_err("Found secondary hvdivider charger\n");
			else {
				chr_err("*** Error : can't find secondary hvdivider charger ***\n");
				return false;
			}
		}
	}

	chr_err("register chg1 notifier %d %d\n",
		info->chg1_dev != NULL, info->algo.do_event != NULL);
	if (info->chg1_dev != NULL && info->algo.do_event != NULL) {
		chr_err("register chg1 notifier done\n");
		info->chg1_nb.notifier_call = info->algo.do_event;
		register_charger_device_notifier(info->chg1_dev,
						&info->chg1_nb);
		charger_dev_set_drvdata(info->chg1_dev, info);
	}

	chr_err("register dvchg chg1 notifier %d %d\n",
		info->dvchg1_dev != NULL, info->algo.do_dvchg1_event != NULL);
	if (info->dvchg1_dev != NULL && info->algo.do_dvchg1_event != NULL) {
		chr_err("register dvchg chg1 notifier done\n");
		info->dvchg1_nb.notifier_call = info->algo.do_dvchg1_event;
		register_charger_device_notifier(info->dvchg1_dev,
						&info->dvchg1_nb);
		charger_dev_set_drvdata(info->dvchg1_dev, info);
	}

	chr_err("register dvchg chg2 notifier %d %d\n",
		info->dvchg2_dev != NULL, info->algo.do_dvchg2_event != NULL);
	if (info->dvchg2_dev != NULL && info->algo.do_dvchg2_event != NULL) {
		chr_err("register dvchg chg2 notifier done\n");
		info->dvchg2_nb.notifier_call = info->algo.do_dvchg2_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
		charger_dev_set_drvdata(info->dvchg2_dev, info);
	}

	chr_err("register hvdvchg chg1 notifier %d %d\n",
		info->hvdvchg1_dev != NULL,
		info->algo.do_hvdvchg1_event != NULL);
	if (info->hvdvchg1_dev != NULL &&
	    info->algo.do_hvdvchg1_event != NULL) {
		chr_err("register hvdvchg chg1 notifier done\n");
		info->hvdvchg1_nb.notifier_call = info->algo.do_hvdvchg1_event;
		register_charger_device_notifier(info->hvdvchg1_dev,
						 &info->hvdvchg1_nb);
		charger_dev_set_drvdata(info->hvdvchg1_dev, info);
	}

	chr_err("register hvdvchg chg2 notifier %d %d\n",
		info->hvdvchg2_dev != NULL,
		info->algo.do_hvdvchg2_event != NULL);
	if (info->hvdvchg2_dev != NULL &&
	    info->algo.do_hvdvchg2_event != NULL) {
		chr_err("register hvdvchg chg2 notifier done\n");
		info->hvdvchg2_nb.notifier_call = info->algo.do_hvdvchg2_event;
		register_charger_device_notifier(info->hvdvchg2_dev,
						 &info->hvdvchg2_nb);
		charger_dev_set_drvdata(info->hvdvchg2_dev, info);
	}

	return true;
}

static int mtk_charger_force_disable_power_path(struct mtk_charger *info,
	int idx, bool disable);
static int mtk_charger_plug_out(struct mtk_charger *info)
{
	struct charger_data *pdata1 = &info->chg_data[CHG1_SETTING];
	struct charger_data *pdata2 = &info->chg_data[CHG2_SETTING];
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;

	chr_err("%s\n", __func__);
#if defined (CONFIG_N28_CHARGER_PRIVATE)
	info->can_charging = false;
	info->disable_charger = true;
	g_cp_charging.cp_chg_status = 0;
#endif
	info->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->charger_thread_polling = false;
	info->dpdmov_stat = false;
	info->lst_dpdmov_stat = false;
	info->is_soc_100_in_charging = false;
	info->is_basic_discharge = false;

	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;

	notify.evt = EVT_PLUG_OUT;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
		chg_alg_plugout_reset(alg);
	}

#ifdef CONFIG_AFC_CHARGER
	afc_plugout_reset(info);
#endif
	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
	charger_dev_plug_out(info->chg1_dev);
	mtk_charger_force_disable_power_path(info, CHG1_SETTING, true);

	atomic_set(&info->batt_full_discharge, 0);

	if (info->enable_vbat_mon)
		charger_dev_enable_6pin_battery_charging(info->chg1_dev, false);

	mtk_adapter_protocol_init(info);
	return 0;
}

static int mtk_charger_plug_in(struct mtk_charger *info,
				int chr_type)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i, vbat;

	chr_debug("%s\n",
		__func__);

	info->chr_type = chr_type;
	info->usb_type = get_usb_type(info);
	info->charger_thread_polling = true;

	info->can_charging = true;
	info->disable_charger = false;
	//info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;
	info->old_cv = 0;
	info->stop_6pin_re_en = false;
	info->batpro_done = false;
	smart_charging(info);
	chr_err("mtk_is_charger_on plug in, type:%d\n", chr_type);

	vbat = get_battery_voltage(info);

	notify.evt = EVT_PLUG_IN;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
		chg_alg_set_prop(alg, ALG_REF_VBAT, vbat);
	}

	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	info->sc.disable_in_this_plug = false;

	charger_dev_plug_in(info->chg1_dev);
	mtk_charger_force_disable_power_path(info, CHG1_SETTING, false);

	return 0;
}

static bool mtk_is_charger_on(struct mtk_charger *info)
{
	int chr_type;

	chr_type = get_charger_type(info);
	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		if (info->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) {
			mtk_charger_plug_out(info);
			if (batt_slate_mode == 2) //Bug773947,churui1.wt,set mode 2 for batt_slate_mode node
				batt_slate_mode = 0;
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	} else {
		if (info->chr_type != chr_type)
			mtk_charger_plug_in(info, chr_type);

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info, chr_type);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	}

	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
		return false;

	return true;
}

static void charger_send_kpoc_uevent(struct mtk_charger *info)
{
	static bool first_time = true;
	ktime_t ktime_now;

	if (first_time) {
		info->uevent_time_check = ktime_get();
		first_time = false;
	} else {
		ktime_now = ktime_get();
		if ((ktime_ms_delta(ktime_now, info->uevent_time_check) / 1000) >= 60) {
			mtk_chgstat_notify(info);
			info->uevent_time_check = ktime_now;
		}
	}
}

static void kpoc_power_off_check(struct mtk_charger *info)
{
	unsigned int boot_mode = info->bootmode;
	int vbus = 0;
	int counter = 0;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	return;
#endif
	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus >= 0 && vbus < 2500 && !mtk_is_charger_on(info) &&
			!info->ta_hardreset && !is_charger_exist(info)) {
			chr_err("Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n", vbus);
			while (1) {
				if (counter >= 20000) {
					chr_err("%s, wait too long\n", __func__);
					kernel_power_off();
					break;
				}
				if (info->is_suspend == false) {
					chr_err("%s, not in suspend, shutdown\n", __func__);
					kernel_power_off();
					break;
				} else {
					chr_err("%s, suspend! cannot shutdown\n", __func__);
					msleep(20);
				}
				counter++;
			}
		}
		charger_send_kpoc_uevent(info);
	}
}

static void charger_status_check(struct mtk_charger *info)
{
	union power_supply_propval online = {0}, status = {0};
	struct power_supply *chg_psy = NULL;
	int ret = 0;
	bool charging = true;


	chg_psy = power_supply_get_by_name("primary_chg");

	if (IS_ERR_OR_NULL(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);
		if (ret < 0)
			chr_debug("%s: %d\n", __func__, ret);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);
		if (ret < 0)
			chr_debug("%s: %d\n", __func__, ret);
		if (!online.intval)
			charging = false;
		else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
				charging = false;
		}
	}
	if (charging != info->is_charging)
		power_supply_changed(info->psy1);
	info->is_charging = charging;
}


static char *dump_charger_type(int chg_type, int usb_type)
{
	switch (chg_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return "none";
	case POWER_SUPPLY_TYPE_USB:
		if (usb_type == POWER_SUPPLY_USB_TYPE_SDP)
			return "usb";
		else
			return "nonstd";
	case POWER_SUPPLY_TYPE_USB_CDP:
		return "usb-h";
	case POWER_SUPPLY_TYPE_USB_DCP:
		return "std";
	//case POWER_SUPPLY_TYPE_USB_FLOAT:
	//	return "nonstd";
	default:
		return "unknown";
	}
}

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
/*BLOCKING_NOTIFIER_HEAD*/
static BLOCKING_NOTIFIER_HEAD(charger_notifier);

int register_mtk_battery_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(register_mtk_battery_notifier);

void unre_mtk_battery_notifier(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(unre_mtk_battery_notifier);

void set_batt_slate_mode(int *en)
{
	blocking_notifier_call_chain(&charger_notifier, 0, en);
}
EXPORT_SYMBOL_GPL(set_batt_slate_mode);

int batt_slate_mode_control(struct notifier_block *nb, unsigned long event, void *v)
{
	struct mtk_charger *info = container_of(nb, struct mtk_charger, charger_nb);
	static int slate_mode_status = 0;

	if (info == NULL) {
		chr_err("[%s]mtk_charge info is NULL\n", __func__);
		return NOTIFY_DONE;
	}

	if (batt_slate_mode && (info->disable_charger == false) && batt_slate_mode != 3) {
		slate_mode_status |= 2;
		charger_dev_enable_hz(info->chg1_dev, 1);
		_mtk_enable_charging(info, 0);
		//mtk_charger_plug_out(info);
		if (info->psy1 != NULL) {
			power_supply_changed(info->psy1);
		}
		chr_err("batt_slate_mode_control:disable charging\n");
	} else if((slate_mode_status & 2) && info->disable_charger && (batt_slate_mode == 0)) {
		slate_mode_status &= ~2;
		charger_dev_enable_hz(info->chg1_dev, 0);
		_mtk_enable_charging(info, 1);
		//mtk_charger_plug_in(info, info->chr_type);
		if (info->psy1 != NULL) {
			power_supply_changed(info->psy1);
		}
		chr_err("batt_slate_mode_control:enable charging\n");
	} else {
		chr_err("%s:%d.do no thing\n", __func__,info->disable_charger);
	}

	return NOTIFY_DONE;
}

static void ato_charger_limit_soc(struct mtk_charger *info, int min, int max)
{
	int limit_soc;

	limit_soc = get_uisoc(info);
	chr_err("limit_soc:%d,disable_chg=%d\n",limit_soc, info->disable_charger);

	if ((limit_soc >= max) && (info->disable_charger == false)) {
		charger_dev_enable_hz(info->chg1_dev, 1);
		_mtk_enable_charging(info, 0);
		chr_err("ato_charger_limit_soc:disable charging\n");
	}

	if ((limit_soc <= min) && info->disable_charger && (batt_slate_mode == 0)) {
		charger_dev_enable_hz(info->chg1_dev, 0);
		_mtk_enable_charging(info, 1);
		chr_err("ato_charger_limit_soc:enable charging\n");
	}
}
#endif

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
int wt_batt_full_capacity_check_for_cp(void)
{
	int uisoc = 0;
	bool is_charger_on = false;
	int batt_full_capacity = 100;
	int batt_mode = 0;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL){
		chr_err("%s Couldn't getdrvdata\n", __func__);
		return -EINVAL;
	}

	batt_mode = info->batt_full_capacity;
	if ((batt_mode == POWER_SUPPLY_CAPACITY_85_OPTION)
		|| (batt_mode == POWER_SUPPLY_CAPACITY_85_OFFCHARGING)) {
		batt_full_capacity = 85;
	} else if ((batt_mode == POWER_SUPPLY_CAPACITY_90_OPTION)
		|| (batt_mode == POWER_SUPPLY_CAPACITY_90_OFFCHARGING)) {
		batt_full_capacity = 90;
	} else if ((batt_mode == POWER_SUPPLY_CAPACITY_95_OPTION)
		|| (batt_mode == POWER_SUPPLY_CAPACITY_95_OFFCHARGING)) {
		batt_full_capacity = 95;
	} else if (batt_mode > POWER_SUPPLY_CAPACITY_100) {
		batt_full_capacity = 80;
	}

	uisoc = get_uisoc(info);
	is_charger_on = mtk_is_charger_on(info);
	chr_err("batt_full_capacity %d,%d,%d\n",is_charger_on,uisoc,batt_full_capacity);
	if (batt_full_capacity != 100) {
		if ((uisoc >= batt_full_capacity)
			&& (is_charger_on == true)) {
			chr_err("%s disable charging\n", __func__);
			return -1;
		}
		return 0;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(wt_batt_full_capacity_check_for_cp);

void wt_batt_full_capacity_check(struct mtk_charger *info)
{
	int uisoc = 0;
	bool is_charger_on = false;
	//struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	int batt_full_capacity = 100;
	static int old_batt_full_capacity = 100;
	int batt_mode = info->batt_full_capacity;
	static int old_uisoc = 0;
	int input_current_limit = USB_CHARGER_CURRENT;

	if ((batt_mode == POWER_SUPPLY_CAPACITY_85_OPTION)
		|| (batt_mode == POWER_SUPPLY_CAPACITY_85_OFFCHARGING)) {
		batt_full_capacity = 85;
	} else if ((batt_mode == POWER_SUPPLY_CAPACITY_90_OPTION)
		|| (batt_mode == POWER_SUPPLY_CAPACITY_90_OFFCHARGING)) {
		batt_full_capacity = 90;
	} else if ((batt_mode == POWER_SUPPLY_CAPACITY_95_OPTION)
		|| (batt_mode == POWER_SUPPLY_CAPACITY_95_OFFCHARGING)) {
		batt_full_capacity = 95;
	} else if (batt_mode > POWER_SUPPLY_CAPACITY_100) {
		batt_full_capacity = 80;
	}

	uisoc = get_uisoc(info);
	is_charger_on = mtk_is_charger_on(info);

	//+P240803-01757, liwei19@wt, modify 20240807, Update notification after charging status change
	if (batt_full_capacity != 100) {
		if (!atomic_read(&info->batt_full_discharge)
			&& (uisoc >= batt_full_capacity)
			&& (is_charger_on == true)) {
			if(batt_mode == POWER_SUPPLY_CAPACITY_80_HIGHSOC)
			{
				charger_dev_enable_hz(info->chg1_dev, 1);
				pr_err("80_HIGHSOC set hiz charger_dev_enable_hz = 1\n");
			} else {
				charger_dev_enable_hz(info->chg1_dev, 0);
				pr_err("NOT 80_HIGHSOC not set hiz, hz = 0\n");
			}

			_mtk_enable_charging(info, 0);
			atomic_set(&info->batt_full_discharge, 1);
			if (info->chr_type == POWER_SUPPLY_TYPE_USB) {
				input_current_limit = USB_CHARGER_CURRENT;
			} else {
				input_current_limit = CHARGING_PROTECT_INPUT_CURRENT;
			}
			charger_dev_set_input_current(info->chg1_dev, input_current_limit);
			if (info->psy1 != NULL) {
				power_supply_changed(info->psy1);
			}
			chr_err("batt_full_capacity_check:disable charging\n");
		} else if (atomic_read(&info->batt_full_discharge)
			&& ((uisoc <= (batt_full_capacity - 2))
			|| ((old_batt_full_capacity < batt_full_capacity)
			&& (uisoc <= (batt_full_capacity - 1))))
			&& (is_charger_on == true)
			&& (info->sw_jeita.charging == true)) {
			if(batt_mode == POWER_SUPPLY_CAPACITY_80_HIGHSOC)
			{
				charger_dev_enable_hz(info->chg1_dev, 0);
				pr_err("80_HIGHSOC exit hiz, hz = 0\n");
			} else {
				charger_dev_enable_hz(info->chg1_dev, 0);
				pr_err("NOT 80_HIGHSOC not set hiz, hz = 0\n");
			}

			_mtk_enable_charging(info, 1);
			atomic_set(&info->batt_full_discharge, 0);
			if (info->psy1 != NULL) {
				power_supply_changed(info->psy1);
			}
			chr_err("batt_full_capacity_check:enable charging\n");
		}
	} else {
	 	if ((is_charger_on == true) && (info->sw_jeita.charging == true) && (info->disable_charger == true) &&
			(batt_slate_mode == 0) && (batt_store_mode == 0) && (batt_mode == POWER_SUPPLY_CAPACITY_100)
			&& (!info->cmd_discharging)
			&& (atomic_read(&info->batt_full_discharge) || (info->is_basic_discharge
			&& (info->batt_soc_rechg == 0)))) {
			//liwei19@wt, modify 20240829, Resolve stop_charge not working
			charger_dev_enable_hz(info->chg1_dev, 0);
			_mtk_enable_charging(info, 1);
			atomic_set(&info->batt_full_discharge, 0);
			info->is_basic_discharge = false;
			if (info->psy1 != NULL) {
				power_supply_changed(info->psy1);
			}
			chr_err("FULL_CAP relieve 100!!!\n");
		}
	}

	if ((info->batt_soc_rechg == 1) && (batt_mode == POWER_SUPPLY_CAPACITY_100) && (batt_full_capacity == 100)) {
		if ((old_uisoc == 100) && (uisoc == 99)
			&& is_charger_on && info->is_soc_100_in_charging) {
			_mtk_enable_charging(info, 0);
			info->is_basic_discharge = true;
		}

		if ((uisoc <= 95)
			&& (info->is_chg_done
			|| info->is_soc_100_in_charging)) {
			//charger_dev_enable_hz(info->chg1_dev, 1);
			_mtk_enable_charging(info, 0);
			info->is_chg_done = false;
			mdelay(20);
			charger_dev_enable_hz(info->chg1_dev, 0);
			_mtk_enable_charging(info, 1);
			mdelay(50);
			info->is_soc_100_in_charging = false;
			info->is_basic_discharge = false;
			atomic_set(&info->batt_full_discharge, 0);
			if (info->psy1 != NULL) {
				power_supply_changed(info->psy1);
			}
		}

		old_uisoc = uisoc;
	} else {
		old_uisoc = 0;
	}
	old_batt_full_capacity = batt_full_capacity;
	//-P240803-01757, liwei19@wt, modify 20240807, Update notification after charging status change
	chr_err("batt_soc_rechg = %d,batt_mode = %d,chg_done = %d,uisoc = %d\n",
		info->batt_soc_rechg, batt_mode, info->is_chg_done, uisoc);
	chr_err("100_in_charging = %d,is_charger_on = %d, basic_discharge=%d\n",
		info->is_soc_100_in_charging, is_charger_on, info->is_basic_discharge);
}
#endif

static int charger_routine_thread(void *arg)
{
	struct mtk_charger *info = arg;
	unsigned long flags;
	unsigned int init_times = 3;
	static bool is_module_init_done;
	bool is_charger_on;
	int ret;
	int vbat_min = 0;
	int vbat_max = 0;
	int cs_vbat, cs_ibat;
	u32 chg_cv = 0;
	int ap_temp = 0;

	while (1) {
		ret = wait_event_interruptible(info->wait_que,
			(info->charger_thread_timeout == true));
		if (ret < 0) {
			chr_err("%s: wait event been interrupted(%d)\n", __func__, ret);
			continue;
		}

		while (is_module_init_done == false) {
			if (charger_init_algo(info) == true) {
				is_module_init_done = true;
				if (info->charger_unlimited) {
					info->enable_sw_safety_timer = false;
					charger_dev_enable_safety_timer(info->chg1_dev, false);
				}
			}
			else {
				if (init_times > 0) {
					chr_err("retry to init charger\n");
					init_times = init_times - 1;
					msleep(10000);
				} else {
					chr_err("holding to init charger\n");
					msleep(60000);
				}
			}
		}

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		info->charger_thread_timeout = false;

		info->battery_temp = get_battery_temperature(info);
		ret = charger_dev_get_adc(info->chg1_dev,
			ADC_CHANNEL_VBAT, &vbat_min, &vbat_max);
		ret = charger_dev_get_constant_voltage(info->chg1_dev, &chg_cv);
		ap_temp = mtkts_bts_get_hw_temp();
		ap_temp = ap_temp / 1000;

		if (vbat_min != 0)
			vbat_min = vbat_min / 1000;

		/* get data from chgIC first, cs adc is backup */
		get_cs_side_battery_voltage(info, &cs_vbat);
		get_cs_side_battery_current(info, &cs_ibat);

		is_charger_on = mtk_is_charger_on(info);
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
		if (is_charger_on && batt_store_mode) {
			ato_charger_limit_soc(info, 60, 70);
			charger_dev_set_charging_current(info->chg1_dev, 100000);
			chr_err("store_mode soc control, uisoc: %d\n", get_uisoc(info));
		}

		if (is_charger_on)
			batt_slate_mode_control(&info->charger_nb,0,NULL);
#endif
		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);
		mtk_check_ta_status(info);
		kpoc_power_off_check(info);

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
		if (is_charger_on)
			wt_batt_full_capacity_check(info);
#endif

		if (!info->cs_hw_disable)
			chr_err("Vbat=%d vbat2=%d vbats=%d vbus:%d ibus:%d I=%d I2=%d T=%d uisoc:%d type:%s>%s idx:%d ta_stat:%d swchg_ibat:%d cv:%d cmd_pp:%d\n",
				get_battery_voltage(info),
				cs_vbat,
				vbat_min,
				get_vbus(info),
				get_ibus(info),
				get_battery_current(info),
				cs_ibat,
				info->battery_temp,
				get_uisoc(info),
				dump_charger_type(info->chr_type, info->usb_type),
				dump_charger_type(get_charger_type(info), get_usb_type(info)), info->select_adapter_idx,
				info->ta_status[info->select_adapter_idx], get_ibat(info), chg_cv, info->cmd_pp);
		else
			chr_err("Vbat=%d vbats=%d vbus:%d ibus:%d I=%d T=%d uisoc:%d type:%s>%s idx:%d ta_stat:%d swchg_ibat:%d cv:%d cmd_pp:%d ap_temp:%d\n",
				get_battery_voltage(info),
				vbat_min,
				get_vbus(info),
				get_ibus(info),
				get_battery_current(info),
				info->battery_temp,
				get_uisoc(info),
				dump_charger_type(info->chr_type, info->usb_type),
				dump_charger_type(get_charger_type(info), get_usb_type(info)), info->select_adapter_idx,
				info->ta_status[info->select_adapter_idx], get_ibat(info), chg_cv, info->cmd_pp, ap_temp);

		if (is_disable_charger(info) == false &&
			is_charger_on == true &&
			info->can_charging == true) {
			if (info->algo.do_algorithm)
				info->algo.do_algorithm(info);
			charger_status_check(info);
		} else {
			chr_debug("disable charging %d %d %d\n",
			    is_disable_charger(info), is_charger_on, info->can_charging);
		}
		if (info->bootmode != 1 && info->bootmode != 2 && info->bootmode != 4
			&& info->bootmode != 8 && info->bootmode != 9)
			smart_charging(info);
		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		chr_debug("%s end , %d\n",
			__func__, info->charger_thread_timeout);
		mutex_unlock(&info->charger_lock);

		if (info->enable_boot_volt &&
			ktime_get_seconds() > RESET_BOOT_VOLT_TIME &&
			!info->reset_boot_volt_times) {
			ret = charger_dev_set_boot_volt_times(info->chg1_dev, 0);
			if (ret < 0)
				chr_err("reset boot_battery_voltage times fails %d\n", ret);
			else {
				info->reset_boot_volt_times = 1;
				chr_err("reset boot_battery_voltage times\n");
			}
		}
	}

	return 0;
}


#ifdef CONFIG_PM
static int charger_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	ktime_t ktime_now;
	struct timespec64 now;
	struct mtk_charger *info;

	info = container_of(notifier,
		struct mtk_charger, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		info->is_suspend = true;
		chr_debug("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		info->is_suspend = false;
		chr_debug("%s: enter PM_POST_SUSPEND\n", __func__);
		ktime_now = ktime_get_boottime();
		now = ktime_to_timespec64(ktime_now);

		if (timespec64_compare(&now, &info->endtime) >= 0 &&
			info->endtime.tv_sec != 0 &&
			info->endtime.tv_nsec != 0) {
			chr_err("%s: alarm timeout, wake up charger\n",
				__func__);
			__pm_relax(info->charger_wakelock);
			info->endtime.tv_sec = 0;
			info->endtime.tv_nsec = 0;
			_wake_up_charger(info);
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

static enum alarmtimer_restart
	mtk_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct mtk_charger *info =
	container_of(alarm, struct mtk_charger, charger_timer);

	if (info->is_suspend == false) {
		_wake_up_charger(info);
	} else {
		__pm_stay_awake(info->charger_wakelock);
	}

	return ALARMTIMER_NORESTART;
}

static void mtk_charger_init_timer(struct mtk_charger *info)
{
	alarm_init(&info->charger_timer, ALARM_BOOTTIME,
			mtk_charger_alarm_timer_func);
	mtk_charger_start_timer(info);

}

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
#if defined (CONFIG_W2_CHARGER_PRIVATE)
static int cc_ori_from_ctd = 0;
void set_cc_orientation(int cc_ori)
{
	cc_ori_from_ctd = cc_ori;
	printk("cc_ori_from_ctd = %d\n", cc_ori_from_ctd);
}
EXPORT_SYMBOL(set_cc_orientation);
#else
extern int tcpci_typec_cc_orientation;
#endif
static ssize_t typec_cc_orientation_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

#if defined (CONFIG_W2_CHARGER_PRIVATE)
	pinfo->typec_cc_orientation = cc_ori_from_ctd;
#else
	pinfo->typec_cc_orientation = tcpci_typec_cc_orientation;
#endif

	chr_err("%s: %d\n", __func__, pinfo->typec_cc_orientation);
	return sprintf(buf, "%d\n", pinfo->typec_cc_orientation);
}

static ssize_t typec_cc_orientation_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0)
		pinfo->typec_cc_orientation = temp;
	else
		chr_err("%s: format error!\n", __func__);
	return size;
}

static DEVICE_ATTR_RW(typec_cc_orientation);

static ssize_t pd_flag_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

	switch (pinfo->ta_type) {
	case MTK_PD_CONNECT_PE_READY_SNK_PD30:
	case MTK_PD_CONNECT_PE_READY_SNK_APDO:
		pinfo->pd_flag = true;
		break;
	case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
	case MTK_PD_CONNECT_NONE:
	case MTK_PD_CONNECT_PE_READY_SNK:
		pinfo->pd_flag = false;
		break;
	}
	chr_err("%s:ta_type=%d,pd_flag=%d\n", __func__, pinfo->ta_type, pinfo->pd_flag);
	return sprintf(buf, "%d\n", pinfo->pd_flag);
}

static ssize_t pd_flag_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->pd_flag = false;
		else
			pinfo->pd_flag = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}
static DEVICE_ATTR_RW(pd_flag);

static ssize_t afc_flag_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

	if (afc_get_is_connect(pinfo) == true)
		pinfo->afc_flag = true;
	else
		pinfo->afc_flag = false;

	chr_err("%s: %d\n", __func__, pinfo->afc_flag);
	return sprintf(buf, "%d\n", pinfo->afc_flag);
}

static ssize_t afc_flag_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->afc_flag = false;
		else
			pinfo->afc_flag = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(afc_flag);

// gujiayin.wt 20240910 add SDP/DCP/CDP/HVDCP in  88 mode
static ssize_t real_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);

	if (pinfo == NULL) {
		chr_err("[%s]mtk_charge info is NULL\n", __func__);
		return -1;
	}

	if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		pinfo->usb_type = get_usb_type(pinfo);
		pinfo->real_charge_type = REAL_TYPE_USB;
	} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pinfo->real_charge_type = REAL_TYPE_USB_CDP;
	} else if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK
			|| pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30
			|| pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		pinfo->real_charge_type = REAL_TYPE_USB_HVDCP;
	} else if (afc_get_is_connect(pinfo)) {
		pinfo->real_charge_type = REAL_TYPE_USB_HVDCP;
	} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pinfo->real_charge_type = REAL_TYPE_USB_DCP;
	} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		pinfo->real_charge_type = REAL_TYPE_USB_FLOAT;
	} else {
		pinfo->real_charge_type = REAL_TYPE_UNKNOWN;
	}

	chr_err("%s: %d\n", __func__, pinfo->real_charge_type);
	return sprintf(buf, "%s\n", real_charge_type_text[pinfo->real_charge_type]);
}
static DEVICE_ATTR_RO(real_type);

// gujiayin.wt 20240910 add shipmode
bool factory_shipmode = false;
EXPORT_SYMBOL(factory_shipmode);
static ssize_t shipmode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *info = power_supply_get_drvdata(psy);

	info->shipmode = true;
	factory_shipmode = info->shipmode;
	chr_err("factory_shipmode = %d,%d",info->shipmode,factory_shipmode);

	return sprintf(buf, "%d\n",info->shipmode);
}
static DEVICE_ATTR_RO(shipmode);

static ssize_t show_online(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	if(otg_state == 1){
		chr_err("%s: otg_online:%s\n", __func__, "ON");
		return sprintf(buf, "%s\n", "ON");
	}else{
		chr_err("%s: otg_onlie:%s\n", __func__, "OFF");
		return sprintf(buf, "%s\n", "OFF");
	}
}

static ssize_t store_online(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger *pinfo = power_supply_get_drvdata(psy);
	unsigned int temp = 0;
	struct power_supply *chg_psy = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(pinfo)) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(pinfo)) {
		pinfo->chg1_dev = get_charger_by_name("primary_chg");
		if (IS_ERR_OR_NULL(pinfo->chg1_dev)) {
			pr_err("[%s]get primary charger device failed\n",__func__);
			return -EINVAL;
		}
	}

	if (kstrtouint(buf, 10, &temp) == 0) {
		if (temp == 0){
			_mtk_enable_charging(pinfo, true);
			charger_dev_enable_otg(pinfo->chg1_dev, false);
			otg_state = 0;
			pr_err("[%s] off val = %d\n",__func__,otg_state);
		} else {
			_mtk_enable_charging(pinfo, false);
			charger_dev_enable_otg(pinfo->chg1_dev, true);
			otg_state = 1;
			pr_err("[%s] on val = %d\n",__func__,otg_state);
		}
	} else {
		chr_err("%s: format error!\n", __func__);
	}

	return size;
}
static DEVICE_ATTR(online, 0664, show_online, store_online);

static ssize_t store_mode_show(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	chr_err("%s:batt_store_mode is %d\n", __func__, batt_store_mode);
	return scnprintf(buf, PAGE_SIZE, "%d\n", batt_store_mode);
}

static ssize_t store_mode_store(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int val;

	if (kstrtoint(buf, 10, &val) == 0) {
		if (val == 0)
			batt_store_mode = false;
		else
			batt_store_mode = true;
	} else if (sscanf(buf, "%10d\n", &val) == 1) {
		if (val == 0)
			batt_store_mode = false;
		else
			batt_store_mode = true;
	} else
		chr_err("%s: format error!\n", __func__);

	return size;
}
static DEVICE_ATTR_RW(store_mode);

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
static ssize_t show_batt_soc_rechg(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = power_supply_get_by_name("mtk-master-charger");
	struct mtk_charger *info = NULL;

	if (psy == NULL || IS_ERR(psy)) {
		chr_err("%s Couldn't get psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL)
		return -EINVAL;

	chr_err("%s: batt_soc_rechg=%d\n", __func__, info->batt_soc_rechg);
	return scnprintf(buf, PAGE_SIZE, "%d\n", info->batt_soc_rechg);
}

static ssize_t store_batt_soc_rechg(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = power_supply_get_by_name("mtk-master-charger");
	struct mtk_charger *info = NULL;
	int val;

	if (psy == NULL || IS_ERR(psy)) {
		chr_err("%s Couldn't get psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL)
		return -EINVAL;

	if (kstrtoint(buf, 10, &val) == 0) {
		info->batt_soc_rechg = val;
	} else if (sscanf(buf, "%10d\n", &val) == 1) {
		info->batt_soc_rechg = val;
	}
	_wake_up_charger(info);

	return size;
}
static DEVICE_ATTR(batt_soc_rechg, 0664, show_batt_soc_rechg,store_batt_soc_rechg);

static ssize_t show_batt_full_capacity(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = power_supply_get_by_name("mtk-master-charger");
	struct mtk_charger *info = NULL;

	if (psy == NULL || IS_ERR(psy)) {
		chr_err("%s Couldn't get psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL)
		return -EINVAL;

	if ((info->batt_full_capacity < ARRAY_SIZE(POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT))
		&& (info->batt_full_capacity >= 0)) {
		chr_err("%s: batt_full_capacity=%d, %s\n", __func__, info->batt_full_capacity,
			POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT[info->batt_full_capacity]);
		return sprintf(buf, "%s\n", POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT[info->batt_full_capacity]);
	}
	chr_err("%s: batt_full_capacity=%d\n", __func__, info->batt_full_capacity);
	return scnprintf(buf, PAGE_SIZE, "%d\n", info->batt_full_capacity);
}

static ssize_t store_batt_full_capacity(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = power_supply_get_by_name("mtk-master-charger");
	struct mtk_charger *info = NULL;

	if (psy == NULL || IS_ERR(psy)) {
		chr_err("%s Couldn't get psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL)
		return -EINVAL;

	info->batt_full_capacity = __sysfs_match_string(POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT,
						   ARRAY_SIZE(POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT), buf);

	if (info->batt_full_capacity == -EINVAL) {
		chr_err("%s: match batt_full_mode fail, keep default measure\n", __func__);
		info->batt_full_capacity = POWER_SUPPLY_NO_ONEUI_CHG;
	}
	_wake_up_charger(info);

	if (info->psy1 != NULL) {
		power_supply_changed(info->psy1);
	}
	chr_err("%s: batt_full_capacity=%d\n", __func__, info->batt_full_capacity);

	return size;
}
static DEVICE_ATTR(batt_full_capacity, 0664, show_batt_full_capacity, store_batt_full_capacity);
#endif

int mtk_charger_create_third_files(struct device dev)
{
	int ret = 0;

	ret = device_create_file(&dev, &dev_attr_typec_cc_orientation);
	if (ret)
		goto _out;

	ret = device_create_file(&dev, &dev_attr_afc_flag);
	if (ret)
		goto _out;

	ret = device_create_file(&dev, &dev_attr_pd_flag);
	if (ret)
		goto _out;

	ret = device_create_file(&dev, &dev_attr_real_type);
	if (ret)
		goto _out;

	return 0;
_out:
	return ret;
}

int notify_adapter_event(struct notifier_block *notifier, unsigned long evt, void *val);
static void late_init_work(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger,
								late_init_work.work);
	struct power_supply *bat_psy = NULL;
	int ret = 0, i = 0;
	struct power_supply *chg_psy = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (IS_ERR_OR_NULL(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return;
	} else {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		if (info == NULL) {
			chr_err("%s info is NULL\n", __func__);
			return;
		}
		bat_psy = info->bat_psy;
		if (bat_psy == NULL || IS_ERR(bat_psy)) {
			chr_err("%s retry to get bat_psy\n", __func__);
			bat_psy = power_supply_get_by_name("battery");
			info->bat_psy = bat_psy;
			goto out;
		}
	}

	ret = device_create_file(&info->bat_psy->dev, &dev_attr_shipmode);
	if (ret)
		chr_err("%s: mtk create battery files failed\n", __func__);

	ret = device_create_file(&info->bat_psy->dev, &dev_attr_store_mode);
	if (ret)
		chr_err("%s: mtk create battery files failed\n", __func__);

	for (i = 0; i < MAX_TA_IDX; i++) {
		if (!info->adapter_dev[i]) {
			chr_err("%s: retry to find %s\n", __func__, adapter_type_names[i]);
			info->adapter_dev[i] = get_adapter_by_name(adapter_type_names[i]);
			info->ta_nb[i].nb.notifier_call = notify_adapter_event;
			info->ta_nb[i].info = info;
			register_adapter_device_notifier(info->adapter_dev[i], &(info->ta_nb[i].nb));
		} else {
			chr_err("%s: %s is already found,do nothing\n", __func__, adapter_type_names[i]);
		}
	}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	ret = device_create_file(&(info->bat_psy->dev), &dev_attr_batt_soc_rechg);
	if (ret)
		chr_err("%s: create batt_soc_rechg file node failed\n", __func__);

	ret = device_create_file(&(info->bat_psy->dev), &dev_attr_batt_full_capacity);
	if (ret)
		chr_err("%s: create batt_full_capacity file node failed\n", __func__);
#endif

	chr_err("%s: mtk create battery files sucessfully\n", __func__);

	return;
out:
	schedule_delayed_work(&info->late_init_work,msecs_to_jiffies(100));
}
#endif

static int mtk_charger_setup_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *battery_dir = NULL, *entry = NULL;
	struct mtk_charger *info = platform_get_drvdata(pdev);

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_jeita);
	if (ret)
		goto _out;

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	ret = device_create_file(&(pdev->dev), &dev_attr_ap_thermal);
	if (ret)
		goto _out;
#endif

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_ovp_threshold);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_chr_type);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_enable_meta_current_limit);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_cs_heatlim);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_cs_para_mode);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_fast_chg_indicator);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Charging_mode);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_ta_type);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_High_voltage_chg_enable);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Rust_detect);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Thermal_throttle);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_alg_new_arbitration);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_alg_unchangeable);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_vbat_mon);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Pump_Express);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charger_Voltage);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charging_Current);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_input_current);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_charger_log_level);
	if (ret)
		goto _out;

	/* Battery warning */
	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;

	/* sysfs node */
	ret = device_create_file(&(pdev->dev), &dev_attr_enable_sc);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_stime);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_etime);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_tuisoc);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_ibat_limit);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_enable_power_path);
	if (ret)
		goto _out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chr_err("%s: mkdir /proc/mtk_battery_cmd failed\n", __func__);
		return -ENOMEM;
	}

	entry = proc_create_data("current_cmd", 0644, battery_dir,
			&mtk_chg_current_cmd_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_power_path", 0644, battery_dir,
			&mtk_chg_en_power_path_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_safety_timer", 0644, battery_dir,
			&mtk_chg_en_safety_timer_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("set_cv", 0644, battery_dir,
			&mtk_chg_set_cv_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}

	return 0;

fail_procfs:
	remove_proc_subtree("mtk_battery_cmd", NULL);
_out:
	return ret;
}

void mtk_charger_get_atm_mode(struct mtk_charger *info)
{
	char atm_str[64] = {0};
	char *ptr = NULL, *ptr_e = NULL;
	char keyword[] = "androidboot.atm=";
	int size = 0;

	ptr = strstr(chg_get_cmd(), keyword);
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");
		if (ptr_e == 0)
			goto end;

		size = ptr_e - (ptr + strlen(keyword));
		if (size <= 0)
			goto end;
		strncpy(atm_str, ptr + strlen(keyword), size);
		atm_str[size] = '\0';
		chr_err("%s: atm_str: %s\n", __func__, atm_str);

		if (!strncmp(atm_str, "enable", strlen("enable")))
			info->atm_enabled = true;
	}
end:
	chr_err("%s: atm_enabled = %d\n", __func__, info->atm_enabled);
}

static int psy_charger_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	case POWER_SUPPLY_PROP_TYPE:
		return 1;
	default:
		return 0;
	}
}

static const enum power_supply_usb_type charger_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
};

static const enum power_supply_property charger_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_VOLTAGE_BOOT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_NOW,
};

static int psy_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;
	int ret = 0, chg_vbat = 0, vbat_max = 0, idx = 0;
	struct chg_alg_device *alg = NULL;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	chr_debug("%s psp:%d\n", __func__, psp);

	if (info->psy1 == psy) {
		chg = info->chg1_dev;
		idx = CHG1_SETTING;
	} else if (info->psy2 == psy) {
		chg = info->chg2_dev;
		idx = CHG2_SETTING;
	} else if (info->psy_dvchg1 == psy) {
		chg = info->dvchg1_dev;
		idx = DVCHG1_SETTING;
	} else if (info->psy_dvchg2 == psy) {
		chg = info->dvchg2_dev;
		idx = DVCHG2_SETTING;
	} else if (info->psy_hvdvchg1 == psy) {
		chg = info->hvdvchg1_dev;
		idx = HVDVCHG1_SETTING;
	} else if (info->psy_hvdvchg2 == psy) {
		chg = info->hvdvchg2_dev;
		idx = HVDVCHG2_SETTING;
	} else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (idx == DVCHG1_SETTING || idx == DVCHG2_SETTING ||
		    idx == HVDVCHG1_SETTING || idx == HVDVCHG2_SETTING) {
			val->intval = false;
			alg = get_chg_alg_by_name("pe5");
			if (alg == NULL)
				chr_err("get pe5 fail\n");
			else {
				ret = chg_alg_is_algo_ready(alg);
				if (ret == ALG_RUNNING)
					val->intval = true;
			}
			break;
		}

		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (chg != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = info->chg_data[idx].junction_temp_max * 10;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_charger_charging_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = get_charger_input_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = get_charger_zcv(info, chg);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		chr_debug("not yet\n");
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = charger_dev_get_adc(info->chg1_dev,
			ADC_CHANNEL_VBAT, &chg_vbat, &vbat_max);
		val->intval = chg_vbat;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* S96818AA3 gujiayin.wt,modify,2024/08/21 add psy node start */
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	int chr_type;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	chr_err("%s psp:%d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		chr_type = get_charger_type(info);
		/* Force to 1 in all charger type */
		if (chr_type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
			val->intval = 1;
		/* Reset to 0 if charger type is USB */
		if ((chr_type == POWER_SUPPLY_TYPE_USB) ||
			(chr_type == POWER_SUPPLY_TYPE_USB_CDP))
			val->intval = 0;

		chr_err("%s chr_type:%d val:%d\n", __func__, chr_type, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	int chr_type;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	chr_err("%s psp:%d, chr_type=%d\n", __func__, psp, info->chr_type);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		chr_type = get_charger_type(info);
		if ((chr_type == POWER_SUPPLY_TYPE_USB) ||
			(chr_type == POWER_SUPPLY_TYPE_USB_CDP))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property mt_otg_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
};

static int mt_otg_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	default:
		pr_err("%sdefault:\n",__func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt_otg_set_property(struct power_supply *psy,
	enum power_supply_property psp,
	const union power_supply_propval *val)
{
	int ret = 0;
	/*
	struct mtk_charger *info;
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(info)) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	*/

	switch (psp) {
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt_otg_props_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	default:
		break;
	}

	return 0;
}
#endif
/* S96818AA3 gujiayin.wt,modify,2024/08/21 add psy node end */

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static enum power_supply_property hv_disable_properties[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int hv_disable_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	default:
		return 0;
	}
}

static int hv_disable_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	chr_debug("%s psp:%d\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->disable_quick_charge;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int hv_disable_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct chg_alg_notify notify;
	struct power_supply *bat_psy = power_supply_get_by_name("battery");

	notify.evt = EVT_MAX;
	notify.value = 0;

	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: failed to get info\n", __func__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->disable_quick_charge = true;
		else
			info->disable_quick_charge = false;

		#if defined (CONFIG_N28_CHARGER_PRIVATE)
		g_cp_charging.disable_quick_charge = info->disable_quick_charge;
		#endif
		chr_err("%s: disable quick charge\n", __func__);
		if(!IS_ERR_OR_NULL(bat_psy)) {
			power_supply_changed(bat_psy);
		}

		break;
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}
#endif

static int mtk_charger_enable_power_path(struct mtk_charger *info,
	int idx, bool en)
{
	int ret = 0;
	bool is_en = true;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);
	info->enable_pp[idx] = en;

	if (info->force_disable_pp[idx])
		goto out;

	ret = charger_dev_is_powerpath_enabled(chg_dev, &is_en);
	if (ret < 0) {
		chr_err("%s: get is power path enabled failed\n", __func__);
		goto out;
	}
	if (is_en == en) {
		chr_err("%s: power path is already en = %d\n", __func__, is_en);
		goto out;
	}

	pr_info("%s: enable power path = %d\n", __func__, en);
	ret = charger_dev_enable_powerpath(chg_dev, en);
out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

static int mtk_charger_force_disable_power_path(struct mtk_charger *info,
	int idx, bool disable)
{
	int ret = 0;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);

	if (disable == info->force_disable_pp[idx])
		goto out;

	info->force_disable_pp[idx] = disable;
	ret = charger_dev_enable_powerpath(chg_dev,
		info->force_disable_pp[idx] ? false : info->enable_pp[idx]);
out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

static int mtk_charger_set_constant_voltage(struct mtk_charger *info,
	int idx, u32 uV)
{
	int ret = 0;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	ret = charger_dev_set_constant_voltage(chg_dev, uV);
	if (ret < 0)
		chr_err("%s: set constant voltage failed\n", __func__);

	return ret;
}

static int psy_charger_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger *info;
	int idx;

	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: failed to get info\n", __func__);
		return -EINVAL;
	}

	if (info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 == psy)
		idx = CHG2_SETTING;
	else if (info->psy_dvchg1 == psy)
		idx = DVCHG1_SETTING;
	else if (info->psy_dvchg2 == psy)
		idx = DVCHG2_SETTING;
	else if (info->psy_hvdvchg1 == psy)
		idx = HVDVCHG1_SETTING;
	else if (info->psy_hvdvchg2 == psy)
		idx = HVDVCHG2_SETTING;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->enable_hv_charging = true;
		else
			info->enable_hv_charging = false;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		info->chg_data[idx].thermal_charging_current_limit =
			val->intval;
#ifdef CHARGING_15W_THERMAL_ENABLE
			info->old_thermal_charging_current_limit = val->intval;
			atomic_set(&info->thermal_current_update, 1);
#endif
		chr_err("%s: thermal_charging_current_limit=%d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		//info->chg_data[idx].thermal_input_current_limit =
		//	val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (val->intval > 0)
			mtk_charger_enable_power_path(info, idx, false);
		else
			mtk_charger_enable_power_path(info, idx, true);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		if (val->intval > 0)
			mtk_charger_force_disable_power_path(info, idx, true);
		else
			mtk_charger_force_disable_power_path(info, idx, false);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		info->ta_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		info->data.battery_cv = val->intval;
		info->stop_6pin_re_en = false;
		mtk_charger_set_constant_voltage(info, idx, info->data.battery_cv);
		break;
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}

static void mtk_charger_external_power_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop = {0};
	union power_supply_propval prop2 = {0};
	union power_supply_propval vbat0 = {0};
	struct power_supply *chg_psy = NULL;
	int ret = 0;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		pr_notice("%s: failed to get info\n", __func__);
		return;
	}
	chg_psy = info->chg_psy;

	if (IS_ERR_OR_NULL(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);

		chg_psy = power_supply_get_by_name("primary_chg");

		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (ret < 0)
			chr_debug("%s: %d\n", __func__, ret);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop2);
		if (ret < 0)
			chr_debug("%s: %d\n", __func__, ret);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);
		if (ret < 0)
			chr_debug("%s: %d\n", __func__, ret);
	}

	if (info->vbat0_flag != vbat0.intval) {
		if (vbat0.intval) {
			info->enable_vbat_mon = false;
			charger_dev_enable_6pin_battery_charging(info->chg1_dev, false);
		} else
			info->enable_vbat_mon = info->enable_vbat_mon_bak;

		info->vbat0_flag = vbat0.intval;
	}

	pr_notice("%s event, name:%s online:%d type:%d vbus:%d\n", __func__,
		psy->desc->name, prop.intval, prop2.intval,
		get_vbus(info));

	_wake_up_charger(info);
}

int notify_adapter_event(struct notifier_block *notifier,
			unsigned long evt, void *val)
{
	struct mtk_charger *pinfo = NULL;
	u32 boot_mode = 0;
	bool report_psy = true;
	int index = 0;
	struct info_notifier_block *ta_nb;

	ta_nb = container_of(notifier, struct info_notifier_block, nb);
	pinfo = ta_nb->info;
	pinfo->ta_hardreset = false;
	index = ta_nb - pinfo->ta_nb;
	chr_err("%s %lu, %d\n", __func__, evt, index);
	boot_mode = pinfo->bootmode;

	switch (evt) {
	case TA_DETACH:
		mutex_lock(&pinfo->ta_lock);
		chr_err("TA Notify Detach\n");
		pinfo->ta_status[index] = TA_DETACH;
		mutex_unlock(&pinfo->ta_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_DETACH, 0);
		atomic_set(&pinfo->batt_full_discharge, 0);
#if defined (CONFIG_N28_CHARGER_PRIVATE)
		charger_dev_set_enable_term(pinfo->chg1_dev, true);
		pinfo->is_enable_term = true;
#endif
		_wake_up_charger(pinfo);
		/* reset PE40 */
		break;

	case TA_ATTACH:
		mutex_lock(&pinfo->ta_lock);
		chr_err("TA Notify Attach\n");
		pinfo->ta_status[index] = TA_ATTACH;
		mutex_unlock(&pinfo->ta_lock);
		_wake_up_charger(pinfo);
		/* reset PE40 */
		break;

	case TA_DETECT_FAIL:
		mutex_lock(&pinfo->ta_lock);
		chr_err("TA Notify Detect Fail\n");
		pinfo->ta_status[index] = TA_DETECT_FAIL;
		mutex_unlock(&pinfo->ta_lock);
		_wake_up_charger(pinfo);
		/* reset PE50 */
		break;

	case TA_HARD_RESET:
		mutex_lock(&pinfo->ta_lock);
		chr_err("TA Notify Hard Reset\n");
		pinfo->ta_status[index] = TA_HARD_RESET;
		pinfo->ta_hardreset = true;
		mutex_unlock(&pinfo->ta_lock);
		_wake_up_charger(pinfo);
		/* PD is ready */
		break;

	case TA_SOFT_RESET:
		mutex_lock(&pinfo->ta_lock);
		chr_err("TA Notify Soft Reset\n");
		pinfo->ta_status[index] = TA_SOFT_RESET;
		mutex_unlock(&pinfo->ta_lock);
		_wake_up_charger(pinfo);
		/* PD30 is ready */
		break;
	case MTK_TYPEC_WD_STATUS:
		chr_err("wd status = %d\n", *(bool *)val);
		pinfo->water_detected = *(bool *)val;
		if (pinfo->water_detected == true) {
			pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
			pinfo->record_water_detected = true;
			if (boot_mode == 8 || boot_mode == 9)
				pinfo->enable_hv_charging = false;
		} else {
			pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
			if (boot_mode == 8 || boot_mode == 9)
				pinfo->enable_hv_charging = true;
		}
		mtk_chgstat_notify(pinfo);
		report_psy = boot_mode == 8 || boot_mode == 9;
		break;
	}
	chr_debug("%s: evt: pd:%d, ufcs:%d\n", __func__,
	pinfo->ta_status[PD], pinfo->ta_status[UFCS]);

	if (report_psy)
		power_supply_changed(pinfo->psy1);
	return NOTIFY_DONE;
}

int chg_alg_event(struct notifier_block *notifier,
			unsigned long event, void *data)
{
	chr_err("%s: evt:%lu\n", __func__, event);

	return NOTIFY_DONE;
}

static char *mtk_charger_supplied_to[] = {
	"battery"
};

#if defined (CHARGING_THERMAL_ENABLE) || defined (CHARGING_15W_THERMAL_ENABLE)
static void lcmoff_switch(struct mtk_charger *info, int onoff)
{
	chr_err("%s: onoff = %d\n", __func__, onoff);

	/* onoff = 0: LCM OFF */
	/* others: LCM ON */
	if (info) {
		if (onoff) {
			/* deactivate lcmoff policy */
			info->lcmoff = 0;
		} else {
			/* activate lcmoff policy */
			info->lcmoff = 1;
		}
	}
}

static int lcmoff_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *v)
{
    struct mtk_charger *info = container_of(self, struct mtk_charger, fb_notif);

    if (info && v) {
#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
        const unsigned long event_enum[2] = {MTK_DISP_EARLY_EVENT_BLANK, MTK_DISP_EVENT_BLANK};
        const int blank_enum[2] = {MTK_DISP_BLANK_POWERDOWN, MTK_DISP_BLANK_UNBLANK};
        int blank_value = *((int *)v);
#elif IS_ENABLED(CONFIG_FB)
        const unsigned long event_enum[2] = {FB_EARLY_EVENT_BLANK, FB_EVENT_BLANK};
        const int blank_enum[2] = {FB_BLANK_POWERDOWN, FB_BLANK_UNBLANK};
        int blank_value = *((int *)(((struct fb_event *)v)->data));
#endif
        chr_err("notifier,event:%lu,blank:%d", event, blank_value);
        if ((blank_enum[1] == blank_value) && (event_enum[1] == event)) {
            lcmoff_switch(info, 1);
        } else if ((blank_enum[0] == blank_value) && (event_enum[0] == event)) {
			lcmoff_switch(info, 0);
        } else {
            chr_err("notifier,event:%lu,blank:%d, not care", event, blank_value);
        }
    } else {
        chr_err("ts_data/v is null");
        return -EINVAL;
    }

    return 0;
}

static int charge_notifier_callback_init(struct mtk_charger *info)
{
    int ret = 0;

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
    chr_err("init notifier with mtk_disp_notifier_register");
    info->fb_notif.notifier_call = lcmoff_fb_notifier_callback;
    ret = mtk_disp_notifier_register("charge_thermal_notifier", &info->fb_notif);
    if (ret < 0) {
        chr_err("[DRM]mtk_disp_notifier_register fail: %d", ret);
    }

#elif IS_ENABLED(CONFIG_FB)
    chr_err("init notifier with fb_register_client");
    info->fb_notif.notifier_call = lcmoff_fb_notifier_callback;
    ret = fb_register_client(&info->fb_notif);
    if (ret) {
        chr_err("[FB]Unable to register fb_notifier: %d", ret);
    }
#endif

    return ret;
}

static int charge_notifier_callback_exit(struct mtk_charger *info)
{

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
    if (mtk_disp_notifier_unregister(&info->fb_notif))
        chr_err("[DRM]Error occurred while unregistering disp_notifier.");
#elif IS_ENABLED(CONFIG_FB)
    if (fb_unregister_client(&info->fb_notif))
        chr_err("[FB]Error occurred while unregistering fb_notifier.");
#endif

    return 0;
}
#endif

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	int i;
	char *name = NULL;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
#if defined (CONFIG_W2_CHARGER_PRIVATE) || defined (CONFIG_N28_CHARGER_PRIVATE)
	if (is_mtk_pd_probe_ok) {
		pr_err("%s: mtk_pd_probe ok\n", __func__);
	} else {
		pr_err("%s: mtk_pd_probe fail\n", __func__);
		return -EPROBE_DEFER;
	}
#endif
#if defined (CONFIG_W2_CHARGER_PRIVATE)
	if (is_mtk_pe2_probe_ok) {
		pr_err("%s: mtk_pe2_probe ok\n", __func__);
	} else {
		pr_err("%s: mtk_pe2_probe fail\n", __func__);
		return -EPROBE_DEFER;
	}
#endif
	mtk_charger_parse_dt(info, &pdev->dev);

	mutex_init(&info->cable_out_lock);
	mutex_init(&info->charger_lock);
	mutex_init(&info->pd_lock);
	mutex_init(&info->ta_lock);
	for (i = 0; i < CHG2_SETTING + 1; i++) {
		mutex_init(&info->pp_lock[i]);
		info->force_disable_pp[i] = false;
		info->enable_pp[i] = true;
	}
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
		"charger suspend wakelock");
	info->charger_wakelock =
		wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	if (register_pm_notifier(&info->pm_notifier)) {
		chr_err("%s: register pm failed\n", __func__);
		return -ENODEV;
	}
	info->pm_notifier.notifier_call = charger_pm_event;
#endif /* CONFIG_PM */
	srcu_init_notifier_head(&info->evt_nh);
	mtk_charger_setup_files(pdev);
	mtk_charger_get_atm_mode(info);
	sw_jeita_state_machine_init(info);

	for (i = 0; i < CHGS_SETTING_MAX; i++) {
		info->chg_data[i].thermal_charging_current_limit = -1;
		info->chg_data[i].thermal_input_current_limit = -1;
		info->chg_data[i].input_current_limit_by_aicl = -1;
	}
#if defined (CHARGING_15W_THERMAL_ENABLE)
	info->old_thermal_charging_current_limit = -1;
	atomic_set(&info->thermal_current_update, 0);
#endif
	info->enable_hv_charging = true;
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	info->batt_charging_source = SEC_BATTERY_CABLE_NONE;
	info->disable_quick_charge = false;
#endif
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	info->batt_full_capacity = POWER_SUPPLY_NO_ONEUI_CHG;
	info->batt_soc_rechg = 0;
#endif
	info->is_enable_term = true;
	//S96818AA3-769, liwei19.wt add, fixme
	//info->ta_type = MTK_CAP_TYPE_UNKNOWN;
	info->ta_type = MTK_PD_CONNECT_NONE;
	info->typec_cc_orientation = 0;

	atomic_set(&info->batt_full_discharge, 0);

	info->psy_desc1.name = "mtk-master-charger";
	info->psy_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc1.usb_types = charger_psy_usb_types;
	info->psy_desc1.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_desc1.properties = charger_psy_properties;
	info->psy_desc1.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
		mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy_cfg1.supplied_to = mtk_charger_supplied_to;
	info->psy_cfg1.num_supplicants = ARRAY_SIZE(mtk_charger_supplied_to);
	info->psy1 = power_supply_register(&pdev->dev, &info->psy_desc1,
			&info->psy_cfg1);


	info->chg_psy = power_supply_get_by_name("primary_chg");

	if (IS_ERR_OR_NULL(info->chg_psy))
		chr_err("%s: devm power fail to get chg_psy\n", __func__);

#if !IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
	info->bc12_psy = power_supply_get_by_name("primary_chg");
	if (IS_ERR_OR_NULL(info->bc12_psy))
		chr_err("%s: devm power fail to get bc12_psy\n", __func__);
#endif

	info->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev,
		"battmanager");
	if (IS_ERR_OR_NULL(info->bat_psy))
		chr_err("%s: devm power fail to get bat_psy\n", __func__);

	if (IS_ERR(info->psy1))
		chr_err("register psy1 fail:%ld\n",
			PTR_ERR(info->psy1));
/* S96818AA3 gujiayin.wt,modify,2024/08/21 add psy node start*/
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	info->ac_desc.name = "ac";
	info->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	info->ac_desc.properties = mt_ac_properties;
	info->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	info->ac_desc.get_property = mt_ac_get_property;
	info->ac_cfg.drv_data = info;

	info->ac_psy = power_supply_register(&pdev->dev,
			&info->ac_desc, &info->ac_cfg);

	if (IS_ERR(info->ac_psy)) {
		chr_err("%s Failed to register power supply: %ld\n",
			__func__, PTR_ERR(info->ac_psy));
		return PTR_ERR(info->ac_psy);
	}

	info->usb_desc.name = "usb";
	info->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	info->usb_desc.properties = mt_usb_properties;
	info->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	info->usb_desc.get_property = mt_usb_get_property;
	info->usb_cfg.drv_data = info;

	info->usb_psy = power_supply_register(&pdev->dev,
			&info->usb_desc, &info->usb_cfg);

	if (IS_ERR(info->usb_psy)) {
		chr_err("%s Failed to register power supply: %ld\n",
			__func__, PTR_ERR(info->usb_psy));
		return PTR_ERR(info->usb_psy);
	}

	mtk_charger_create_third_files(info->usb_psy->dev);

	info->otg_desc.name = "otg",
	info->otg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	info->otg_desc.properties = mt_otg_properties,
	info->otg_desc.num_properties = ARRAY_SIZE(mt_otg_properties),
	info->otg_desc.get_property = mt_otg_get_property,
	info->otg_desc.set_property = mt_otg_set_property,
	info->otg_desc.property_is_writeable = mt_otg_props_is_writeable,
	info->otg_cfg.drv_data = info;

	info->otg_psy = power_supply_register(&pdev->dev,
			&info->otg_desc, &info->otg_cfg);

	if (IS_ERR(info->otg_psy)) {
		chr_err("%s Failed to register power supply: %ld\n",
			__func__, PTR_ERR(info->otg_psy));
		return PTR_ERR(info->otg_psy);
	}

	device_create_file(&info->otg_psy->dev, &dev_attr_online);
#endif
/* S96818AA3 gujiayin.wt,modify,2024/08/21 add psy node end*/
	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.usb_types = charger_psy_usb_types;
	info->psy_desc2.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
			&info->psy_cfg2);

	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%ld\n",
			PTR_ERR(info->psy2));

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	info->psy_desc3.name = "hv_disable";
	info->psy_desc3.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc3.properties = hv_disable_properties;
	info->psy_desc3.num_properties = ARRAY_SIZE(hv_disable_properties);
	info->psy_desc3.get_property = hv_disable_get_property;
	info->psy_desc3.set_property = hv_disable_set_property;
	info->psy_desc3.property_is_writeable =
			hv_disable_property_is_writeable;
	info->psy_cfg3.drv_data = info;
	info->psy3 = power_supply_register(&pdev->dev, &info->psy_desc3,
			&info->psy_cfg3);

	if (IS_ERR(info->psy3))
		chr_err("register psy3 fail:%ld\n",
			PTR_ERR(info->psy3));
#endif

	info->psy_dvchg_desc1.name = "mtk-mst-div-chg";
	info->psy_dvchg_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_dvchg_desc1.usb_types = charger_psy_usb_types;
	info->psy_dvchg_desc1.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_dvchg_desc1.properties = charger_psy_properties;
	info->psy_dvchg_desc1.num_properties =
		ARRAY_SIZE(charger_psy_properties);
	info->psy_dvchg_desc1.get_property = psy_charger_get_property;
	info->psy_dvchg_desc1.set_property = psy_charger_set_property;
	info->psy_dvchg_desc1.property_is_writeable =
		psy_charger_property_is_writeable;
	info->psy_dvchg_cfg1.drv_data = info;
	info->psy_dvchg1 = power_supply_register(&pdev->dev,
						 &info->psy_dvchg_desc1,
						 &info->psy_dvchg_cfg1);
	if (IS_ERR(info->psy_dvchg1))
		chr_err("register psy dvchg1 fail:%ld\n",
			PTR_ERR(info->psy_dvchg1));

	info->psy_dvchg_desc2.name = "mtk-slv-div-chg";
	info->psy_dvchg_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_dvchg_desc2.usb_types = charger_psy_usb_types;
	info->psy_dvchg_desc2.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_dvchg_desc2.properties = charger_psy_properties;
	info->psy_dvchg_desc2.num_properties =
		ARRAY_SIZE(charger_psy_properties);
	info->psy_dvchg_desc2.get_property = psy_charger_get_property;
	info->psy_dvchg_desc2.set_property = psy_charger_set_property;
	info->psy_dvchg_desc2.property_is_writeable =
		psy_charger_property_is_writeable;
	info->psy_dvchg_cfg2.drv_data = info;
	info->psy_dvchg2 = power_supply_register(&pdev->dev,
						 &info->psy_dvchg_desc2,
						 &info->psy_dvchg_cfg2);
	if (IS_ERR(info->psy_dvchg2))
		chr_err("register psy dvchg2 fail:%ld\n",
			PTR_ERR(info->psy_dvchg2));

	info->psy_hvdvchg_desc1.name = "mtk-mst-hvdiv-chg";
	info->psy_hvdvchg_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_hvdvchg_desc1.usb_types = charger_psy_usb_types;
	info->psy_hvdvchg_desc1.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_hvdvchg_desc1.properties = charger_psy_properties;
	info->psy_hvdvchg_desc1.num_properties =
					     ARRAY_SIZE(charger_psy_properties);
	info->psy_hvdvchg_desc1.get_property = psy_charger_get_property;
	info->psy_hvdvchg_desc1.set_property = psy_charger_set_property;
	info->psy_hvdvchg_desc1.property_is_writeable =
					      psy_charger_property_is_writeable;
	info->psy_hvdvchg_cfg1.drv_data = info;
	info->psy_hvdvchg1 = power_supply_register(&pdev->dev,
						   &info->psy_hvdvchg_desc1,
						   &info->psy_hvdvchg_cfg1);
	if (IS_ERR(info->psy_hvdvchg1))
		chr_err("register psy hvdvchg1 fail:%ld\n",
					PTR_ERR(info->psy_hvdvchg1));

	info->psy_hvdvchg_desc2.name = "mtk-slv-hvdiv-chg";
	info->psy_hvdvchg_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_hvdvchg_desc2.usb_types = charger_psy_usb_types;
	info->psy_hvdvchg_desc2.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_hvdvchg_desc2.properties = charger_psy_properties;
	info->psy_hvdvchg_desc2.num_properties =
					     ARRAY_SIZE(charger_psy_properties);
	info->psy_hvdvchg_desc2.get_property = psy_charger_get_property;
	info->psy_hvdvchg_desc2.set_property = psy_charger_set_property;
	info->psy_hvdvchg_desc2.property_is_writeable =
					      psy_charger_property_is_writeable;
	info->psy_hvdvchg_cfg2.drv_data = info;
	info->psy_hvdvchg2 = power_supply_register(&pdev->dev,
						   &info->psy_hvdvchg_desc2,
						   &info->psy_hvdvchg_cfg2);
	if (IS_ERR(info->psy_hvdvchg2))
		chr_err("register psy hvdvchg2 fail:%ld\n",
					PTR_ERR(info->psy_hvdvchg2));

	info->log_level = CHRLOG_ERROR_LEVEL;
	mtk_adapter_protocol_init(info);

	for (i = 0;i < MAX_TA_IDX;i++) {
		info->adapter_dev[i] =
			get_adapter_by_name(adapter_type_names[i]);
		if (!info->adapter_dev[i])
			chr_err("%s: No %s found\n", __func__, adapter_type_names[i]);
		else {
			info->ta_nb[i].nb.notifier_call = notify_adapter_event;
			info->ta_nb[i].info = info;
			register_adapter_device_notifier(info->adapter_dev[i],
					&(info->ta_nb[i].nb));
		}
	}

	sc_init(&info->sc);
	info->chg_alg_nb.notifier_call = chg_alg_event;

	info->fast_charging_indicator = 0;
	info->enable_meta_current_limit = 1;

	if (strcmp(info->curr_select_name,"NULL")) {
		info->cs_para_mode = 0;
		info->cs_heatlim = 5;
		info->dual_chg_stat = STILL_CHG;
	}

	info->is_charging = false;
	info->safety_timer_cmd = -1;
	info->cmd_pp = -1;
#ifdef CHARGING_THERMAL_ENABLE
	info->ap_temp = 0xffff;
#endif

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	INIT_DELAYED_WORK(&info->psy_update_dwork, mtk_inform_psy_dwork_handler);
	INIT_DELAYED_WORK(&info->late_init_work, late_init_work);
	schedule_delayed_work(&info->late_init_work,msecs_to_jiffies(16*HZ));
	info->charger_nb.notifier_call = batt_slate_mode_control;
	if (register_mtk_battery_notifier(&info->charger_nb)) {
		chr_err("%s: register mtk battery notifier failed\n", __func__);
	}
#endif
	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (info != NULL && info->bootmode != 8 && info->bootmode != 9)
		mtk_charger_force_disable_power_path(info, CHG1_SETTING, true);

	kthread_run(charger_routine_thread, info, "charger_thread");
#if defined (CHARGING_THERMAL_ENABLE) || defined (CHARGING_15W_THERMAL_ENABLE)
	if(charge_notifier_callback_init(info))
		chr_err("charge_notifier_callback_init failed\n");
#endif
#ifdef CONFIG_AFC_CHARGER
	/*yuanjian.wt add for AFC*/
	if (afc_charge_init(info) < 0)
		info->afc.afc_init_ok = false;
#endif

	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
#if defined (CHARGING_THERMAL_ENABLE) || defined (CHARGING_15W_THERMAL_ENABLE)
	struct mtk_charger *info = platform_get_drvdata(dev);

	if(info) {
		charge_notifier_callback_exit(info);
	}
#endif
	return 0;
}

static void mtk_charger_shutdown(struct platform_device *dev)
{
	struct mtk_charger *info = platform_get_drvdata(dev);
	int i;

	printk("%s:%d,%d,enter!!\n", __func__, info->shipmode, factory_shipmode);
	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i] == NULL)
			continue;
		chg_alg_stop_algo(info->alg[i]);
	}
#if defined (CONFIG_N28_CHARGER_PRIVATE)
	if(info->shipmode) {
		printk("shutdown with entering shipmode!\n");
		charger_dev_enable_hz(info->chg1_dev, 1); //set HIZ before set shipmode
		charger_dev_set_shipmode(info->chg1_dev, true);
		charger_dev_set_shipmode_delay(info->chg1_dev, false);
	}else
		printk("##shutdown only!!\n");
#if defined (CONFIG_N28_CHARGER_PRIVATE)
	g_cp_charging.cp_chg_status |= CP_EXIT;
#endif
#endif

#ifdef CONFIG_AFC_CHARGER
	if (afc_get_is_connect(info)) {
		afc_reset_ta_vchr(info);
	}
#endif
#if defined (CONFIG_W2_CHARGER_PRIVATE)
	if(factory_shipmode) {
		printk("w2 shutdown with entering shipmode!\n");
		charger_dev_enable_powerpath(info->chg1_dev, false);
		charger_dev_enable_ship_mode(info->chg1_dev, true);
	}else
		printk("w2 ##shutdown only!!\n");
#endif
}

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "mediatek,charger",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

struct platform_device mtk_charger_device = {
	.name = "charger",
	.id = -1,
};

static struct platform_driver mtk_charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.driver = {
		   .name = "charger",
		   .of_match_table = mtk_charger_of_match,
	},
};

static int __init mtk_charger_init(void)
{
	return platform_driver_register(&mtk_charger_driver);
}
#if IS_BUILTIN(CONFIG_MTK_CHARGER)
late_initcall(mtk_charger_init);
#else
module_init(mtk_charger_init);
#endif
static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&mtk_charger_driver);
}
module_exit(mtk_charger_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
