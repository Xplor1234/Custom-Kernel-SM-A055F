// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_basic_charger.c
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
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include "mtk_charger.h"

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			info->setting.cv = info->sw_jeita.cv;
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
}

static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;
	int cap_type;

	rp = adapter_dev_get_property(info->adapter_dev[PD], TYPEC_RP_LEVEL);
	cap_type = adapter_dev_get_property(info->adapter_dev[PD], CAP_TYPE);
	if (cap_type == TA_DETECT_FAIL &&
			rp != 500 &&
			info->chr_type != POWER_SUPPLY_TYPE_USB &&
			info->chr_type != POWER_SUPPLY_TYPE_USB_CDP)
		return true;

	return false;
}

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

#ifdef CONFIG_N28_CHARGER_PRIVATE
	int type;

	type = adapter_dev_get_property(info->select_adapter, CAP_TYPE);
	if (type == MTK_PD_APDO)
		info->fast_charging_indicator = PE5_ID;
	else if(type == MTK_PD)
		info->fast_charging_indicator = PDC_ID;
	else
		info->fast_charging_indicator = 0;
#endif
#ifdef CONFIG_W2_CHARGER_PRIVATE
	int type;

	type = adapter_dev_get_property(info->select_adapter, CAP_TYPE);
	if ((type == MTK_PD_APDO) || (type == MTK_PD))
		info->fast_charging_indicator = PDC_ID;
	else
		info->fast_charging_indicator = 0;
#endif
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;
#ifdef CONFIG_W2_CHARGER_PRIVATE
		if (alg->alg_id == PE2_ID)
			info->fast_charging_indicator = PE2_ID;
#endif
		if (info->enable_fast_charging_indicator &&
		    ((alg->alg_id & info->fast_charging_indicator) == 0))
			continue;

		chg_alg_set_current_limit(alg, &info->setting);
		state = chg_alg_is_algo_ready(alg);
		chr_debug("%s %s ret:%s, prtocol_state:%d\n",
			__func__, dev_name(&alg->dev),
			chg_alg_state_to_str(state), info->protocol_state);

		if (state == ALG_READY || state == ALG_RUNNING) {
			ret = true;
			break;
		}
	}
	return ret;
}

#ifdef CONFIG_AFC_CHARGER
#if 0
bool is_pd_adapter(struct mtk_charger *info)
{
	int ta_type = MTK_CAP_TYPE_UNKNOWN;

	ta_type = adapter_dev_get_property(info->select_adapter, CAP_TYPE);
	if (ta_type == MTK_PD_APDO || ta_type == MTK_PD)
		return true;

	return false;
}
#else
bool is_pd_adapter(struct mtk_charger *info)
{
	if (info->ta_type == MTK_PD_CONNECT_PE_READY_SNK
			|| info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30
			|| info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		return true;
	}

	return false;
}
#endif
EXPORT_SYMBOL(is_pd_adapter);
#endif

#ifndef CONFIG_MTK_DISABLE_TEMP_PROTECT
#ifdef CHARGING_THERMAL_ENABLE
extern void ap_thermal_machine(struct mtk_charger *info);
#endif
#endif
static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2, *pdata_dvchg;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;
	int ta_type = MTK_CAP_TYPE_UNKNOWN;

	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
	pdata_dvchg = &info->chg_data[DVCHG1_SETTING];
	// pdata_dvchg2 = &info->chg_data[DVCHG2_SETTING];
	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		is_basic = true;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		is_basic = true;
		goto done;
	}

	if (((info->bootmode == 1) ||
	    (info->bootmode == 5)) && info->enable_meta_current_limit != 0) {
		pdata->input_current_limit = 200000; // 200mA
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		goto done;
	}
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	/*store_mode charger current limit*/
	if (batt_store_mode) {
		pdata->charging_current_limit = 100000;
		chr_err("store mode icl = %d", pdata->charging_current_limit);
		is_basic = true;
		goto done;
	}
#endif

	if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
				pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		/* NONSTANDARD_CHARGER */
		pdata->input_current_limit =
			info->data.usb_charger_current;
		pdata->charging_current_limit =
			info->data.usb_charger_current;
		is_basic = true;
	} else {
		/*chr_type && usb_type cannot match above, set 500mA*/
		pdata->input_current_limit =
				info->data.usb_charger_current;
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	}
#ifdef CONFIG_AFC_CHARGER
	if (!is_pd_adapter(info)) {
		if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
			if (afc_get_is_connect(info)) {
				if (info->disable_quick_charge) {
					afc_9v_to_5v(info);
				} else {
					afc_5v_to_9v(info);
				}
			} else {
				afc_check_charger(info);
			}
		}
	}

	afc_set_charging_current(info,
				&pdata->charging_current_limit,
				&pdata->input_current_limit);

	if (!afc_get_is_connect(info) && support_fast_charging(info)) {
#else
	if (support_fast_charging(info)) {
#endif
		is_basic = false;
#ifdef CONFIG_W2_CHARGER_PRIVATE
		if (get_chg_alg_by_name("pe2") == NULL) {
			chr_err("cannot get pe2\n");
		} else {
			if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP && info->fast_charging_indicator == PE2_ID
				&& chg_alg_is_algo_running(get_chg_alg_by_name("pe2"))) {
				pdata->input_current_limit = info->data.pe2_charger_input_current;
				pdata->charging_current_limit = info->data.pe2_charger_current;
			}
		}
#endif
	}
	else {
		is_basic = true;
		/* AICL */
		if (!info->disable_aicl)
			charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
#ifdef CONFIG_AFC_CHARGER
		chr_err("type-C: afc_connect=%d,typec_adapter=%d\n",
			afc_get_is_connect(info), is_typec_adapter(info));
		if (!afc_get_is_connect(info) && is_typec_adapter(info)) {
#else
		if (is_typec_adapter(info)) {
#endif
			if (adapter_dev_get_property(info->adapter_dev[PD]
			, TYPEC_RP_LEVEL)
				== 3000) {
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
			} else if (adapter_dev_get_property(info->adapter_dev[PD],
				TYPEC_RP_LEVEL) == 1500) {
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
			} else {
				chr_err("type-C: inquire rp error\n");
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}

			chr_err("type-C:%d current:%d\n",
				info->ta_status[PD],
				adapter_dev_get_property(info->adapter_dev[PD],
					TYPEC_RP_LEVEL));
		}
	}

	ta_type = adapter_dev_get_property(info->select_adapter, CAP_TYPE);
	if ((ta_type == MTK_PD_APDO) || (ta_type == MTK_PD &&
		(info->chr_type != POWER_SUPPLY_TYPE_USB &&
		info->usb_type != POWER_SUPPLY_USB_TYPE_SDP))) {
		pdata->input_current_limit = FIX_PDO_INPUT_CURRENT;
		pdata->charging_current_limit = FIX_PDO_CHARGER_CURRENT;
		chr_err("type-C: ta type is PD\n");
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB)
			chr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (info->sw_jeita.sm != TEMP_T2_TO_T3) {
				pdata->charging_current_limit = info->sw_jeita.cc;
			}
		}
	}

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
/* set current for 15w pd and pps when disable quick charge*/
	if (info->disable_quick_charge && is_pd_adapter(info)) {
		pdata->input_current_limit = FIX_PDO_COMMON_INPUT_CURRENT;
		//if (info->sw_jeita.cc <= info->data.ac_charger_current) {
			//pdata->charging_current_limit = info->sw_jeita.cc;
		//} else {
			pdata->charging_current_limit = info->data.ac_charger_current;
		//}
	}
#endif
//+ gujiayin.wt modify 20240918 disable battery temperature protect
#ifndef CONFIG_MTK_DISABLE_TEMP_PROTECT
	//sc_select_charging_current(info, pdata);
#ifdef CHARGING_THERMAL_ENABLE
	ap_thermal_machine(info);
	pdata->thermal_charging_current_limit = info->ap_thermal_data.cc;
	pdata->thermal_input_current_limit = -1;
#endif
	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <=
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		} else {
			info->setting.charging_current_limit1 =
					pdata->charging_current_limit;
		}
		pdata->thermal_throttle_record = true;
	} else {
		if (info->enable_sw_jeita && (info->sw_jeita.sm != TEMP_T2_TO_T3)) {
			info->setting.charging_current_limit1 = pdata->charging_current_limit;
		} else {
			info->setting.charging_current_limit1 = pdata->thermal_charging_current_limit;
		}
	}

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <=
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
		pdata->thermal_throttle_record = true;
	} else
		info->setting.input_current_limit1 = -1;

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <=
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	} else
		info->setting.charging_current_limit2 = pdata2->thermal_charging_current_limit;

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <=
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	} else
		info->setting.input_current_limit2 = -1;
#endif
//- gujiayin.wt modify 20240918 disable battery temperature protect

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1
		&& !info->charger_unlimited
		&& !info->disable_aicl) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
//+ gujiayin.wt modify 20240918 disable battery temperature protect
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	info->setting.input_current_limit_dvchg1 = -1;
#else
	if (pdata->thermal_charging_current_limit != -1) {
		pdata_dvchg->thermal_input_current_limit = pdata->thermal_charging_current_limit / 2;
	} else {
		pdata_dvchg->thermal_input_current_limit = -1;
	}
	info->setting.input_current_limit_dvchg1 =
		pdata_dvchg->thermal_input_current_limit;
#endif
//- gujiayin.wt modify 20240918 disable battery temperature protect

done:

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -EOPNOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -EOPNOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	/* For TC_018, pleasae don't modify the format */
	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d dvchg1:%d sc:%d %d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		_uA_to_mA(pdata_dvchg->thermal_input_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->ta_status[info->select_adapter_idx],
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}

static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false;
	bool cs_chg_done = false;
	int i;
	int ret, ret2, ret3;
	int val = 0;
	int lst_rnd_alg_idx = info->lst_rnd_alg_idx;
	int vbat = 0, vbat_cs = 0, ibat_cs = 0;
	int cs_ir_cmp = 0;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	int uisoc = 0;
#endif

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	chr_err("%s batt_soc_rechg:%d,batt_full_capacity:%d,chg_done:%d\n", __func__, info->batt_soc_rechg,info->batt_full_capacity,chg_done);
	if ((info->batt_soc_rechg == 1) && (info->batt_full_capacity == 1)) {
		charger_dev_set_recharge(info->chg1_dev, 200000);
		uisoc = get_uisoc(info);
		if (uisoc > 95 && chg_done) {
			info->is_chg_done = chg_done;
			//chr_err("It's basic mode. Soc is greater than 95,  then it does not rechg!\n");
			return 0;
		}
		else if (uisoc <= 95 && chg_done)
			chg_done = 0;
	} else {
		charger_dev_set_recharge(info->chg1_dev, 100000);
	}
#endif

	if (info->cschg1_dev && info->cs_with_gauge
		&& !info->cs_hw_disable) {
		cs_dev_is_charging_done(info->cschg1_dev, &cs_chg_done);
		if (info->is_cs_chg_done != cs_chg_done) {
			if (cs_chg_done) {
				cs_dev_do_event(info->cschg1_dev, EVENT_FULL, 0);
				chr_err("%s cs side battery full\n", __func__);
			} else {
				cs_dev_do_event(info->cschg1_dev, EVENT_RECHARGE, 0);
				chr_err("%s cs battery recharge\n", __func__);
			}
		}
	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			info->polling_interval = CHARGING_FULL_INTERVAL;
				chr_err("%s main side battery full\n", __func__);
			} else {
				charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
				info->polling_interval = CHARGING_INTERVAL;
				chr_err("%s main side battery recharge\n", __func__);
			}
		}
		if (cs_chg_done && chg_done) {
			info->dual_chg_stat = BOTH_EOC;
			chr_err("%s: close curr selc\n", __func__);
			charger_cs_status_control(info->cschg1_dev, 0);
			info->cs_hw_disable = true;
			chr_err("%s: dual_chg_stat = %d\n", __func__, info->dual_chg_stat);
		} else {
			info->dual_chg_stat = STILL_CHG;
			chr_err("%s: dual_chg_stat = %d\n", __func__, info->dual_chg_stat);
		}
	} else if (info->is_chg_done != chg_done) {
		if (chg_done) {
			// add check cs voltage
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			info->polling_interval = CHARGING_FULL_INTERVAL;
			charger_cs_enable_lowpower(info->cschg1_dev, 1);
			chr_err("%s battery full\n", __func__);
		} else {
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			info->polling_interval = CHARGING_INTERVAL;
			charger_cs_enable_lowpower(info->cschg1_dev, 0);
			chr_err("%s battery recharge\n", __func__);
		}
	}

	/* CS */
	if (info->cschg1_dev && !info->cs_hw_disable) {
		ret = charger_dev_set_constant_voltage(info->cschg1_dev, V_CS_BATTERY_CV);
		if (ret < 0)
			chr_err("%s: failed to set cs1 cv to: %d mV.\n", __func__, V_CS_BATTERY_CV);
		/* get battery info */
		vbat = get_battery_voltage(info);
		get_cs_side_battery_voltage(info, &vbat_cs);
		ret = get_cs_side_battery_current(info, &ibat_cs);

		if (ret == FROM_CS_ADC) // sc adc
			cs_ir_cmp = 25 * ibat_cs;	// 25mohm is sc side's measurement resistance.
		else if (ret == FROM_CHG_IC) // chg ic
			cs_ir_cmp = 0 * ibat_cs;
		if (vbat_cs - vbat > V_BATT_EXTRA_DIFF) {
			if (vbat_cs >= V_CS_BATTERY_CV + cs_ir_cmp) {
				if (ibat_cs > CS_CC_MIN && info->cs_cc_now - 100 > CS_CC_MIN) {
					info->cs_cc_now -= 100;
					charger_dev_set_charging_current(info->cschg1_dev, info->cs_cc_now);
				}
			}
			chr_err("cs_ir_cmp:%d, cs_cc_now:%d\n", cs_ir_cmp, info->cs_cc_now);
		} else
			info->cs_cc_now = AC_CS_NORMAL_CC;
		chr_err("cs_ir_cmp:%d, cs_cc_now:%d\n", cs_ir_cmp, info->cs_cc_now);
		charger_dev_dump_registers(info->cschg1_dev);
		// charger_cs_parallel_mode_setting(info->cschg1_dev, info->cs_para_mode);
		ret = cs_dev_check_cs_temp(info->cschg1_dev);
	}

	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (info->enable_fast_charging_indicator &&
			    ((alg->alg_id & info->fast_charging_indicator) == 0))
				continue;

			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}

			if (info->alg_new_arbitration && info->alg_unchangeable &&
				(lst_rnd_alg_idx > -1)) {
				if (lst_rnd_alg_idx != i)
					continue;
			}

			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s, %d\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret), ret);

			if (ret == (int) ALG_INIT_FAIL || ret == (int) ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == (int) ALG_WAIVER) {
				if (info->alg_new_arbitration)
					continue; /* try next algorithm */
				else {
					is_basic = true;
					break;
				}
			} else if (ret == (int) ALG_TA_CHECKING || ret == (int) ALG_DONE ||
						ret == (int) ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				if (info->alg_new_arbitration && !info->alg_unchangeable &&
					(lst_rnd_alg_idx > -1)) {
					if (lst_rnd_alg_idx != i && lst_rnd_alg_idx < MAX_ALG_NO)
						chg_alg_stop_algo(info->alg[lst_rnd_alg_idx]);
				}
				break;
			} else if (ret == (int) ALG_READY || ret == (int) ALG_RUNNING) {
				is_basic = false;
				if (info->alg_new_arbitration && !info->alg_unchangeable &&
					(lst_rnd_alg_idx > -1)) {
					if (lst_rnd_alg_idx != i && lst_rnd_alg_idx < MAX_ALG_NO)
						chg_alg_stop_algo(info->alg[lst_rnd_alg_idx]);
				}
				chg_alg_start_algo(alg);
				chr_err("%s: %d, %d.\n", __func__, ret, info->cs_hw_disable);
				if (ret == (int) ALG_RUNNING && info->cschg1_dev && !info->cs_hw_disable) {
					ret = charger_dev_set_charging_current(info->cschg1_dev, info->cs_cc_now);
					if (ret < 0)
						chr_err("%s: failed to set cs1 cc to: 1500mA.\n", __func__);
				}
				info->lst_rnd_alg_idx = i;
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true ||
		    pdata->charging_current_limit == 0 ||
		    pdata->input_current_limit == 0) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);
		info->lst_rnd_alg_idx = -1;

		/* CS */
		if (info->cschg1_dev && !info->cs_hw_disable) {
			ret = charger_dev_set_charging_current(info->cschg1_dev, info->cs_cc_now);
			if (ret < 0)
				chr_err("%s: failed to set cs1 cc to: %d mA.\n", __func__, info->cs_cc_now);
		}
		chr_debug("%s:old_cv=%d,cv=%d, vbat_mon_en=%d\n",
			__func__,
			info->old_cv,
			info->setting.cv,
			info->setting.vbat_mon_en);
		if (info->old_cv == 0 || (info->old_cv != info->setting.cv)
		    || info->setting.vbat_mon_en == 0) {
			charger_dev_enable_6pin_battery_charging(
				info->chg1_dev, false);
			charger_dev_set_constant_voltage(info->chg1_dev,
				info->setting.cv);
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1)
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			info->old_cv = info->setting.cv;
		} else {
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1) {
				info->stop_6pin_re_en = 1;
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			}
		}
	}

	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0)
		charger_dev_enable(info->chg1_dev, false);
	else {
		alg = get_chg_alg_by_name("pe5p");
		ret = chg_alg_is_algo_ready(alg);
		alg = get_chg_alg_by_name("pe5");
		ret2 = chg_alg_is_algo_ready(alg);
		alg = get_chg_alg_by_name("hvbp");
		ret3 = chg_alg_is_algo_ready(alg);
		if (!(ret == (int) ALG_READY || ret == (int) ALG_RUNNING) &&
			!(ret2 == (int) ALG_READY || ret2 == (int) ALG_RUNNING) &&
			!(ret3 == (int) ALG_READY || ret3 == (int) ALG_RUNNING))
			charger_dev_enable(info->chg1_dev, true);
	}

	if (info->chg1_dev != NULL) {
		charger_dev_dump_registers(info->chg1_dev);
		charger_dev_kick_wdt(info->chg1_dev);
	}

	if (info->chg2_dev != NULL) {
		charger_dev_dump_registers(info->chg2_dev);
		charger_dev_kick_wdt(info->chg2_dev);
	}

	if (info->bkbstchg_dev != NULL)
		charger_dev_dump_registers(info->bkbstchg_dev);

	return 0;
}

static int enable_charging(struct mtk_charger *info,
						bool en)
{
	int i;
	struct chg_alg_device *alg;


	chr_err("%s %d\n", __func__, en);

	if (en == false) {
#ifdef CONFIG_AFC_CHARGER
		/*yuanjian.wt ad for AFC*/
		if (afc_get_is_enable(info)) {
			afc_set_is_enable(info, false);
			if (afc_get_is_connect(info))
				afc_leave(info);
		}
#endif
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;
			chg_alg_stop_algo(alg);
		}
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	} else {
#ifdef CONFIG_AFC_CHARGER
		if (!afc_get_is_enable(info)) {
			afc_set_is_enable(info, true);
			afc_set_to_check_chr_type(info, true);
		}
#endif
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}

static int charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	struct mtk_charger *info =
			container_of(nb, struct mtk_charger, chg1_nb);
	struct chgdev_notify *data = v;
	int ret = 0, vbat_min = 0, vbat_max = 0, vbat_cs = 0;
	int i;

	chr_err("%s %lu\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		info->stop_6pin_re_en = 1;
		notify.evt = EVT_FULL;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		if (info->cschg1_dev && info->dual_chg_stat == BOTH_EOC) {	// mt6375_2p version not in mt6379_2p
			ret = charger_dev_get_adc(info->chg1_dev,
				ADC_CHANNEL_VBAT, &vbat_min, &vbat_max);
			if (ret < 0)
				chr_err("%s: failed to get vbat, recharge mode\n", __func__);
			else {
				vbat_min = vbat_min / 1000;
				ret = charger_dev_get_vbat(info->cschg1_dev, &vbat_cs);
				if ( abs( vbat_min - vbat_cs ) <= V_BATT_EXTRA_DIFF
				&& info->cs_hw_disable) {
					chr_err("%s: opening cs\n", __func__);
					charger_cs_status_control(info->cschg1_dev, 1);
					info->cs_hw_disable = false;
					info->dual_chg_stat = STILL_CHG;
				}
			}
		}
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	case CHARGER_DEV_NOTIFY_BATPRO_DONE:
		info->batpro_done = true;
		info->setting.vbat_mon_en = 0;
		notify.evt = EVT_BATPRO_DONE;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}
		pr_info("%s: batpro_done = %d\n", __func__, info->batpro_done);
		break;
	case CHARGER_DEV_NOTIFY_DPDM_OVP:
		info->dpdmov_stat = data->dpdmov_stat;
		pr_info("%s: DPDM ovp = %d\n", __func__, info->dpdmov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int to_alg_notify_evt(unsigned long evt)
{
	switch (evt) {
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		return EVT_VBUSOVP;
	case CHARGER_DEV_NOTIFY_IBUSOCP:
		return EVT_IBUSOCP;
	case CHARGER_DEV_NOTIFY_IBUSUCP_FALL:
		return EVT_IBUSUCP_FALL;
	case CHARGER_DEV_NOTIFY_BAT_OVP:
		return EVT_VBATOVP;
	case CHARGER_DEV_NOTIFY_IBATOCP:
		return EVT_IBATOCP;
	case CHARGER_DEV_NOTIFY_VBATOVP_ALARM:
		return EVT_VBATOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VBUSOVP_ALARM:
		return EVT_VBUSOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VOUTOVP:
		return EVT_VOUTOVP;
	case CHARGER_DEV_NOTIFY_VDROVP:
		return EVT_VDROVP;
	default:
		return -EINVAL;
	}
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg2_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int hvdvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			      void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, hvdvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int hvdvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			      void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, hvdvchg2_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	info->algo.do_event = charger_dev_event;
	info->algo.do_dvchg1_event = dvchg1_dev_event;
	info->algo.do_dvchg2_event = dvchg2_dev_event;
	info->algo.do_hvdvchg1_event = hvdvchg1_dev_event;
	info->algo.do_hvdvchg2_event = hvdvchg2_dev_event;
	info->lst_rnd_alg_idx = -1;
	//info->change_current_setting = mtk_basic_charging_current;
	return 0;
}
