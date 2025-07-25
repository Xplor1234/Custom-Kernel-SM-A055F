/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef LINUX_POWER_CHARGER_CLASS_H
#define LINUX_POWER_CHARGER_CLASS_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>

enum adc_channel {
	ADC_CHANNEL_VBUS,
	ADC_CHANNEL_VSYS,
	ADC_CHANNEL_VBAT,
	ADC_CHANNEL_IBUS,
	ADC_CHANNEL_IBAT,
	ADC_CHANNEL_TEMP_JC,
	ADC_CHANNEL_USBID,
	ADC_CHANNEL_TS,
	ADC_CHANNEL_TBAT,
	ADC_CHANNEL_VOUT,
};

struct charger_properties {
	const char *alias_name;
};

/* Data of notifier from charger device */
struct chgdev_notify {
	bool vbusov_stat;
	bool dpdmov_stat;
};

/* charger_dev notify */
enum {
	CHARGER_DEV_NOTIFY_VBUS_OVP,
	CHARGER_DEV_NOTIFY_BAT_OVP,
	CHARGER_DEV_NOTIFY_EOC,
	CHARGER_DEV_NOTIFY_RECHG,
	CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT,
	CHARGER_DEV_NOTIFY_VBATOVP_ALARM,
	CHARGER_DEV_NOTIFY_VBUSOVP_ALARM,
	CHARGER_DEV_NOTIFY_IBATOCP,
	CHARGER_DEV_NOTIFY_IBUSOCP,
	CHARGER_DEV_NOTIFY_IBUSUCP_FALL,
	CHARGER_DEV_NOTIFY_VOUTOVP,
	CHARGER_DEV_NOTIFY_VDROVP,
	CHARGER_DEV_NOTIFY_BATPRO_DONE,
	CHARGER_DEV_NOTIFY_DPDM_OVP,
};

struct charger_device {
	struct charger_properties props;
	struct chgdev_notify noti;
	const struct charger_ops *ops;
	struct mutex ops_lock;
	struct device dev;
	struct srcu_notifier_head evt_nh;
	void	*driver_data;
	bool is_polling_mode;
};

enum charger_property {
	CHARGER_PROP_BLEED_DISCHARGE,
};

union charger_propval {
	int intval;
	const char *strval;
};

struct charger_ops {
	int (*suspend)(struct charger_device *dev, pm_message_t state);
	int (*resume)(struct charger_device *dev);

	/* cable plug in/out */
	int (*plug_in)(struct charger_device *dev);
	int (*plug_out)(struct charger_device *dev);

	/* enable/disable charger */
	int (*enable)(struct charger_device *dev, bool en);
	int (*is_enabled)(struct charger_device *dev, bool *en);

	/* enable/disable chip */
	int (*enable_chip)(struct charger_device *dev, bool en);
	int (*is_chip_enabled)(struct charger_device *dev, bool *en);

	/* get/set charging current*/
	int (*get_charging_current)(struct charger_device *dev, u32 *uA);
	int (*set_charging_current)(struct charger_device *dev, u32 uA);
	int (*get_min_charging_current)(struct charger_device *dev, u32 *uA);

	/* set cv */
	int (*set_constant_voltage)(struct charger_device *dev, u32 uV);
	int (*get_constant_voltage)(struct charger_device *dev, u32 *uV);

	/* set rechg */
	int (*set_recharge)(struct charger_device *dev, u32 uV);

	int (*check_cs_temp)(struct charger_device *dev);
	int (*cs_status_control)(struct charger_device *chg_dev, bool enable);
	int (*cs_enable_lowpower)(struct charger_device *chg_dev, bool enable);

	/* set input_current */
	int (*get_input_current)(struct charger_device *dev, u32 *uA);
	int (*set_input_current)(struct charger_device *dev, u32 uA);
	int (*get_min_input_current)(struct charger_device *dev, u32 *uA);

	/* set boot volt times */
	int (*set_boot_volt_times)(struct charger_device *dev, u32 val);

	/* set termination current */
	int (*get_eoc_current)(struct charger_device *dev, u32 *uA);
	int (*set_eoc_current)(struct charger_device *dev, u32 uA);
	int (*set_enable_term)(struct charger_device *dev, bool enable);

	/* kick wdt */
	int (*kick_wdt)(struct charger_device *dev);

	int (*event)(struct charger_device *dev, u32 event, u32 args);

	/* 6-pin battery */
	int (*enable_6pin_battery_charging)(struct charger_device *dev,
					    bool en);

	/* PE+/PE+2.0 */
	int (*send_ta_current_pattern)(struct charger_device *dev, bool is_inc);
	int (*send_ta20_current_pattern)(struct charger_device *dev, u32 uV);
	int (*reset_ta)(struct charger_device *dev);
	int (*enable_cable_drop_comp)(struct charger_device *dev, bool en);

	int (*set_mivr)(struct charger_device *dev, u32 uV);
	int (*get_mivr)(struct charger_device *dev, u32 *uV);
	int (*get_mivr_state)(struct charger_device *dev, bool *in_loop);

	//gujiayin.wt,add shipmode ctrl
	/* ship mode */
	int (*set_shipmode)(struct charger_device *dev, bool en);
	int (*set_shipmode_delay)(struct charger_device *dev, bool en);
	/* enable/disable powerpath */
	int (*is_powerpath_enabled)(struct charger_device *dev, bool *en);
	int (*enable_powerpath)(struct charger_device *dev, bool en);

	/* enable/disable vbus ovp */
	int (*enable_vbus_ovp)(struct charger_device *dev, bool en);

	/* enable/disable charging safety timer */
	int (*is_safety_timer_enabled)(struct charger_device *dev, bool *en);
	int (*enable_safety_timer)(struct charger_device *dev, bool en);

	/* enable term */
	int (*enable_termination)(struct charger_device *dev, bool en);

	/* direct charging */
	int (*enable_direct_charging)(struct charger_device *dev, bool en);
	int (*kick_direct_charging_wdt)(struct charger_device *dev);
	int (*set_direct_charging_ibusoc)(struct charger_device *dev, u32 uA);
	int (*set_direct_charging_vbusov)(struct charger_device *dev, u32 uV);

	int (*set_ibusocp)(struct charger_device *dev, u32 uA);
	int (*set_vbusovp)(struct charger_device *dev, u32 uV);
	int (*set_ibatocp)(struct charger_device *dev, u32 uA);
	int (*set_vbatovp)(struct charger_device *dev, u32 uV);
	int (*set_vbatovp_alarm)(struct charger_device *dev, u32 uV);
	int (*reset_vbatovp_alarm)(struct charger_device *dev);
	int (*set_vbusovp_alarm)(struct charger_device *dev, u32 uV);
	int (*reset_vbusovp_alarm)(struct charger_device *dev);
	int (*is_vbuslowerr)(struct charger_device *dev, bool *err);
	int (*init_chip)(struct charger_device *dev);
	int (*enable_auto_trans)(struct charger_device *dev, bool en);
	int (*set_auto_trans)(struct charger_device *dev, uint32_t uV, bool en);
	int (*set_operation_mode)(struct charger_device *dev, bool div2);

	/* OTG */
	int (*enable_otg)(struct charger_device *dev, bool en);
	int (*enable_discharge)(struct charger_device *dev, bool en);
	int (*set_boost_current_limit)(struct charger_device *dev, u32 uA);

	/* charger type detection */
	int (*enable_chg_type_det)(struct charger_device *dev, bool en);

	/* run AICL */
	int (*run_aicl)(struct charger_device *dev, u32 *uA);

	/* reset EOC state */
	int (*reset_eoc_state)(struct charger_device *dev);

	int (*safety_check)(struct charger_device *dev, u32 polling_ieoc);

	int (*is_charging_done)(struct charger_device *dev, bool *done);
	int (*set_cs_regVal)(struct charger_device *dev, int setVal);
	int (*set_pe20_efficiency_table)(struct charger_device *dev);
	int (*dump_registers)(struct charger_device *dev);
	int (*dump_init_setting)(struct charger_device *dev);

	int (*get_adc)(struct charger_device *dev, enum adc_channel chan,
		       int *min, int *max);
	int (*get_adc_accuracy)(struct charger_device *dev,
				enum adc_channel chan, int *min, int *max);
	int (*get_vbus_adc)(struct charger_device *dev, u32 *vbus);
	int (*get_vbat_adc)(struct charger_device *dev, u32 *vbat);
	int (*get_ibus_adc)(struct charger_device *dev, u32 *ibus);
	int (*get_ibat_adc)(struct charger_device *dev, u32 *ibat);
	int (*get_cs_current)(struct charger_device *chg_dev, int *ibat);
	int (*parallel_mode_setting)(struct charger_device *chg_dev, int mode);
	int (*cs_init_setting)(struct charger_device *chg_dev);
	int (*get_tchg_adc)(struct charger_device *dev, int *tchg_min,
		int *tchg_max);
	int (*get_zcv)(struct charger_device *dev, u32 *uV);

	/* TypeC */
	int (*enable_usbid)(struct charger_device *dev, bool en);
	int (*set_usbid_rup)(struct charger_device *dev, u32 rup);
	int (*set_usbid_src_ton)(struct charger_device *dev, u32 src_ton);
	int (*enable_usbid_floating)(struct charger_device *dev, bool en);
	int (*enable_force_typec_otp)(struct charger_device *dev, bool en);
	int (*enable_hidden_mode)(struct charger_device *dev, bool en);
	int (*get_ctd_dischg_status)(struct charger_device *dev, u8 *status);
	int (*enable_hz)(struct charger_device *dev, bool en);
	int (*set_vac_ovp)(struct charger_device *dev, u32 uV);

	/* ALPS09479827 Charger: add charger class APIs of MT6360 for factory */
	int (*get_vsys_adc)(struct charger_device *dev, u32 *vsys);
	int (*set_eoc_timer)(struct charger_device *dev, unsigned int mins);
	int (*enable_eoc)(struct charger_device *dev, bool en);
	int (*enable_ship_mode)(struct charger_device *dev, bool batdet_dis_dly);
	int (*enable_aicc)(struct charger_device *dev, bool en);
	int (*en_wdt)(struct charger_device *dev, bool en);
	int (*en_ilim)(struct charger_device *dev, bool en);
	int (*set_iinlmtsel)(struct charger_device *dev, bool en);

	int (*set_property)(struct charger_device *dev,
			    enum charger_property prop,
			    union charger_propval *val);
	int (*get_property)(struct charger_device *dev,
			    enum charger_property prop,
			    union charger_propval *val);

	/* misc */
	int (*get_health)(struct charger_device *dev, int *health);
	int (*get_charge_type)(struct charger_device *dev, int *type);
};

static inline void *charger_dev_get_drvdata(
	const struct charger_device *charger_dev)
{
	return charger_dev->driver_data;
}

static inline void charger_dev_set_drvdata(
	struct charger_device *charger_dev, void *data)
{
	charger_dev->driver_data = data;
}

extern struct charger_device *charger_device_register(
	const char *name,
	struct device *parent, void *devdata, const struct charger_ops *ops,
	const struct charger_properties *props);
extern void charger_device_unregister(
	struct charger_device *charger_dev);
extern struct charger_device *get_charger_by_name(
	const char *name);

#define to_charger_device(obj) container_of(obj, struct charger_device, dev)

static inline void *charger_get_data(
	struct charger_device *charger_dev)
{
	return dev_get_drvdata(&charger_dev->dev);
}

extern int charger_dev_enable(struct charger_device *charger_dev, bool en);
extern int charger_dev_is_enabled(struct charger_device *charger_dev, bool *en);
extern int charger_dev_plug_in(struct charger_device *charger_dev);
extern int charger_dev_plug_out(struct charger_device *charger_dev);
extern int charger_dev_set_charging_current(
	struct charger_device *charger_dev, u32 uA);
extern int charger_dev_get_charging_current(
	struct charger_device *charger_dev, u32 *uA);
extern int charger_dev_get_min_charging_current(
	struct charger_device *charger_dev, u32 *uA);
extern int charger_dev_set_input_current(
	struct charger_device *charger_dev, u32 uA);
extern int charger_dev_get_input_current(
	struct charger_device *charger_dev, u32 *uA);
extern int charger_dev_get_min_input_current(
	struct charger_device *charger_dev, u32 *uA);
extern int charger_dev_set_boot_volt_times(
	struct charger_device *charger_dev, u32 val);
extern int charger_dev_set_eoc_current(
	struct charger_device *charger_dev, u32 uA);
extern int charger_dev_get_eoc_current(
	struct charger_device *charger_dev, u32 *uA);
#if defined (CONFIG_N28_CHARGER_PRIVATE)
extern int charger_dev_set_enable_term(
	struct charger_device *chg_dev, bool enable);
#endif
extern int charger_dev_kick_wdt(
	struct charger_device *charger_dev);
extern int charger_dev_set_constant_voltage(
	struct charger_device *charger_dev, u32 uV);
extern int charger_dev_get_constant_voltage(
	struct charger_device *charger_dev, u32 *uV);
extern int charger_dev_set_recharge(
	struct charger_device *charger_dev, u32 uV);
extern int charger_dev_dump_init_setting(
	struct charger_device *charger_dev);
extern int charger_dev_dump_registers(
	struct charger_device *charger_dev);
extern int charger_dev_enable_vbus_ovp(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_set_mivr(
	struct charger_device *charger_dev, u32 uV);
extern int charger_dev_get_mivr(
	struct charger_device *charger_dev, u32 *uV);
extern int charger_dev_get_mivr_state(
	struct charger_device *charger_dev, bool *in_loop);
extern int charger_dev_do_event(
	struct charger_device *charger_dev, u32 event, u32 args);
//gujiayin.wt,add shipmode ctrl
/* ship mode */
extern int charger_dev_set_shipmode(
	struct charger_device *chg_dev, bool en);
extern int charger_dev_set_shipmode_delay(
	struct charger_device *chg_dev, bool en);
extern int cs_dev_do_event(
	struct charger_device *charger_dev, u32 event, u32 args);
extern int charger_dev_enable_6pin_battery_charging(
	struct charger_device *charger_dev, bool en);
extern int cs_dev_check_cs_temp(
	struct charger_device *chg_dev);
extern int charger_dev_is_powerpath_enabled(
	struct charger_device *charger_dev, bool *en);
extern int charger_dev_is_safety_timer_enabled(
	struct charger_device *charger_dev, bool *en);
extern int charger_dev_enable_termination(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_is_charging_done(
	struct charger_device *charger_dev, bool *done);
extern int cs_dev_is_charging_done(
	struct charger_device *charger_dev, bool *done);
extern int charger_dev_enable_powerpath(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_enable_safety_timer(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_enable_auto_trans(
	struct charger_device *chg_dev, bool en);
extern int charger_dev_set_auto_trans(
	struct charger_device *chg_dev, u32 uV, bool en);
extern int charger_dev_set_operation_mode(
	struct charger_device *chg_dev, bool div2);
extern int charger_dev_enable_chg_type_det(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_enable_otg(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_enable_discharge(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_set_boost_current_limit(
	struct charger_device *charger_dev, u32 uA);
extern int charger_dev_get_zcv(
	struct charger_device *charger_dev, u32 *uV);
extern int charger_dev_run_aicl(
	struct charger_device *charger_dev, u32 *uA);
extern int charger_dev_reset_eoc_state(
	struct charger_device *charger_dev);
extern int charger_dev_safety_check(
	struct charger_device *charger_dev, u32 polling_ieoc);
extern int charger_dev_enable_hz(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_set_vac_ovp(
	struct charger_device *charger_dev, u32 uV);

/* PE+/PE+2.0 */
extern int charger_dev_send_ta_current_pattern(
	struct charger_device *charger_dev, bool is_increase);
extern int charger_dev_send_ta20_current_pattern(
	struct charger_device *charger_dev, u32 uV);
extern int charger_dev_reset_ta(
	struct charger_device *charger_dev);
extern int charger_dev_set_pe20_efficiency_table(
	struct charger_device *charger_dev);
extern int charger_dev_enable_cable_drop_comp(
	struct charger_device *charger_dev, bool en);

/* PE 3.0 */
extern int charger_dev_enable_chip(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_is_chip_enabled(
	struct charger_device *charger_dev, bool *en);
extern int charger_dev_enable_direct_charging(
	struct charger_device *charger_dev, bool en);
extern int charger_dev_kick_direct_charging_wdt(
	struct charger_device *charger_dev);
extern int charger_dev_get_adc(struct charger_device *charger_dev,
	enum adc_channel chan, int *min, int *max);
extern int charger_dev_get_adc_accuracy(struct charger_device *charger_dev,
	enum adc_channel chan, int *min, int *max);
/* Prefer use charger_dev_get_adc api */
extern int charger_dev_get_vbus(
	struct charger_device *charger_dev, u32 *vbus);
extern int charger_dev_get_vbat(
	struct charger_device *charger_dev, u32 *vbat);
extern int charger_dev_get_ibus(
	struct charger_device *charger_dev, u32 *ibus);
extern int charger_dev_get_ibat(
	struct charger_device *charger_dev, u32 *ibat);
extern int charger_cs_get_ibat(
	struct charger_device *chg_dev, int *ibat);
extern int charger_cs_parallel_mode_setting(
	struct charger_device *chg_dev, int mode);
extern int charger_cs_init_setting(
	struct charger_device *chg_dev);
extern int charger_cs_status_control(
	struct charger_device *charger_dev, bool enable);
extern int charger_cs_enable_lowpower(
	struct charger_device *charger_dev, bool enable);
extern int charger_dev_get_temperature(
	struct charger_device *charger_dev, int *tchg_min,
		int *tchg_max);
extern int charger_dev_set_direct_charging_ibusoc(
	struct charger_device *charger_dev, u32 ua);
extern int charger_dev_set_direct_charging_vbusov(
	struct charger_device *charger_dev, u32 uv);

extern int charger_dev_set_ibusocp(struct charger_device *chg_dev, u32 uA);
extern int charger_dev_set_vbusovp(struct charger_device *chg_dev, u32 uV);
extern int charger_dev_set_ibatocp(struct charger_device *chg_dev, u32 uA);
extern int charger_dev_set_vbatovp(struct charger_device *chg_dev, u32 uV);
extern int charger_dev_set_vbatovp_alarm(struct charger_device *chg_dev,
					 u32 uV);
extern int charger_dev_reset_vbatovp_alarm(struct charger_device *chg_dev);
extern int charger_dev_set_vbusovp_alarm(struct charger_device *chg_dev,
					 u32 uV);
extern int charger_dev_reset_vbusovp_alarm(struct charger_device *chg_dev);
extern int charger_dev_is_vbuslowerr(struct charger_device *chg_dev, bool *err);
extern int charger_dev_init_chip(struct charger_device *chg_dev);

/* TypeC */
extern int charger_dev_enable_usbid(struct charger_device *dev, bool en);
extern int charger_dev_set_usbid_rup(struct charger_device *dev, u32 rup);
extern int charger_dev_set_usbid_src_ton(struct charger_device *dev,
					 u32 src_ton);
extern int charger_dev_enable_usbid_floating(struct charger_device *dev,
					     bool en);
extern int charger_dev_enable_force_typec_otp(struct charger_device *dev,
					      bool en);
extern int charger_dev_get_ctd_dischg_status(struct charger_device *dev,
					     u8 *status);

extern int charger_dev_set_property(struct charger_device *dev,
				    enum charger_property prop,
				    union charger_propval *val);
extern int charger_dev_get_property(struct charger_device *dev,
				    enum charger_property prop,
				    union charger_propval *val);

/* misc */
extern int charger_dev_get_health(struct charger_device *dev, int *health);
extern int charger_dev_get_charge_type(struct charger_device *dev, int *type);

/* For buck1 FPWM */
extern int charger_dev_enable_hidden_mode(struct charger_device *dev, bool en);

extern int register_charger_device_notifier(
	struct charger_device *charger_dev,
			      struct notifier_block *nb);
extern int unregister_charger_device_notifier(
	struct charger_device *charger_dev,
				struct notifier_block *nb);
extern int charger_dev_notify(
	struct charger_device *charger_dev, int event);

extern int charger_dev_get_vsys(
	struct charger_device *charger_dev, u32 *vsys);

extern int charger_dev_set_eoc_timer(
	struct charger_device *charger_dev, unsigned int mins);

extern int charger_dev_enable_eoc(
	struct charger_device *charger_dev, bool en);

extern int charger_dev_enable_ship_mode(
	struct charger_device *charger_dev, bool batdet_dis_dly);

extern int charger_dev_enable_aicc(
	struct charger_device *charger_dev,bool en);

extern int charger_dev_en_wdt(
	struct charger_device *charger_dev, bool en);

extern int charger_dev_en_ilim(
	struct charger_device *charger_dev, bool en);

extern int charger_dev_set_iinlmtsel(
	struct charger_device *charger_dev,	bool en);

#endif /*LINUX_POWER_CHARGER_CLASS_H*/
