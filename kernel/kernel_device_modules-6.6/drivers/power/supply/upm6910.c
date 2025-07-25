/*
 * UPM6910D battery charging driver
 *
 * Copyright (C) 2018 Unisemipower
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[upm6910]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/iio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/hardware_info.h>
//#include <mt-plat/v1/charger_type.h>
#include <linux/power_supply.h>
#include "charger_class.h"
#include <linux/phy/phy.h>//#include <mt-plat/upmu_common.h>
#include <linux/usb/gadget.h>
#include <linux/platform_device.h>
#include "../../misc/mediatek/usb/usb20/musb_core.h"

//#include "mtk_charger_intf.h"
#include "upm6910.h"
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include "mtk_charger.h"

#define POWER_DETECT_ICHG		1020
#define POWER_DETECT_ICL		2000
#define POWER_DETECT_VINDPM		4500
#define UPM6910_CV_VOLTAGE		4448
#define SGM41543D_ITERM_CURRENT		300

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

enum sgm_hiz_state {
	SGM_EXIT_HIZ = 0,
	SGM_ENTER_HIZ,
	SGM_NO_HIZ,
};

enum upm6910_usbsw {
	USBSW_CHG,
	USBSW_USB,
};

/*+S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
#if defined (CONFIG_N28_CHARGER_PRIVATE)
extern bool is_upm6910;
#else
bool is_upm6910 = false;
#endif
/*-S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/

bool is_upm6910_chg_probe_ok = false;
EXPORT_SYMBOL(is_upm6910_chg_probe_ok);

enum {
	PN_UPM6910D,
};

enum upm6910_part_no {
	SGM41513D = 0x01,
	UPM6910D = 0x02,
};

enum upm6910_bc12 {
	UPM6910_NOT_NEED_BC12,
	UPM6910_NEED_BC12,
	UPM6910_NOT_RETRY_BC12,
};
static int pn_data[] = {
	[PN_UPM6910D] = 0x02,
};

/*
static char *pn_str[] = {
	[PN_UPM6910D] = "upm6910d",
};
*/

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
	5, 10, 15, 20, 30, 40, 50, 60,
	80, 100, 120, 140, 160, 180, 200, 240
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
	5, 10, 15, 20, 30, 40, 50, 60,
	80, 100, 120, 140, 160, 180, 200, 240
};

static struct charger_device *s_chg_dev_otg;

static enum power_supply_property  upm6910_charger_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, //cv
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, //vbus current
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,	//cc
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	//POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
};

static enum power_supply_usb_type upm6910_charger_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_DCP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_C,
    POWER_SUPPLY_USB_TYPE_PD,
    POWER_SUPPLY_USB_TYPE_PD_DRP,
    POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
};

static char * upm6910_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

struct upm6910 {
	struct device *dev;
	struct i2c_client *client;

	enum upm6910_part_no part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	int status;
	int irq;
	u32 intr_gpio;

#ifdef WT_COMPILE_FACTORY_VERSION
	int probe_flag;
#endif
	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool vbus_gd;
	bool otg_flag;

	bool bc12_retry_done;
	bool is_need_retry_bc12;
	bool is_need_bc12;
	bool float_flag;
	bool is_usb_compatible_ok;
	struct upm6910_platform_data *platform_data;
	struct charger_device *chg_dev;
	struct regulator_dev *otg_rdev;

	struct power_supply *psy;
	struct power_supply *chg_psy;
	struct power_supply *bat_psy;
	struct power_supply *usb_psy;
	struct power_supply *otg_psy;
	struct power_supply_desc psy_desc;

	struct device_node *usb_node;
	struct device_node *boot_node;
	struct platform_device  *usb_pdev;
	struct musb *musb;
	int bootmode;
	int boottype;
	struct delayed_work psy_dwork;
	struct delayed_work prob_dwork;
	struct delayed_work wt_float_recheck_work;
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus start */
	struct iio_channel	**pmic_ext_iio_chans;
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus end */
	int psy_usb_type;
	int chg_type;
	int old_mivr;
	int chg_old_status;
	int hiz_state;
	int boost_current;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};
static int upm6910_charging(struct charger_device *chg_dev, bool enable);

static int  upm6910_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_ONLINE:
	//case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return 1;
	default:
		return 0;
	}
}


static const struct charger_properties upm6910_chg_props = {
	.alias_name = "upm6910",
};

static int __upm6910_read_reg(struct upm6910 *upm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(upm->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __upm6910_write_reg(struct upm6910 *upm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(upm->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int upm6910_read_byte(struct upm6910 *upm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6910_read_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6910_write_byte(struct upm6910 *upm, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6910_write_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sgm415xx_read_read(struct upm6910 *upm, u8 reg, u8 *data)
{
	int ret = 0;
	int i;
	u8 r_data[3];

	for (i = 0; i < 3; i++) {
		ret |= __upm6910_read_reg(upm, reg, &r_data[i]);
	}
	if (r_data[0] == r_data[1] || r_data[0] == r_data[2])
		*data = r_data[0];
	else
		*data = r_data[1];
	//pr_err("sgm415xx_read_read: update bits try read reg: r_data[0]=%d, r_data[1]=%d, r_data[2]=%d\n",
	//							r_data[0], r_data[1], r_data[2]);
	return ret;
}

static int upm6910_update_bits(struct upm6910 *upm, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&upm->i2c_rw_lock);

	if (upm->part_no == SGM41513D) {
		ret = sgm415xx_read_read(upm, reg, &tmp);
	} else {
		ret = __upm6910_read_reg(upm, reg, &tmp);
	}
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __upm6910_write_reg(upm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&upm->i2c_rw_lock);
	return ret;
}

static int upm6910_enable_otg(struct upm6910 *upm)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	if (upm->part_no == SGM41513D) {
		return upm6910_write_byte(upm, UPM6910_REG_01, 0x2a);
	} else {
		return upm6910_update_bits(upm, UPM6910_REG_01, REG01_OTG_CONFIG_MASK,
				   val);
	}

}

static int upm6910_disable_otg(struct upm6910 *upm)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	if (upm->part_no == SGM41513D) {
		return upm6910_write_byte(upm, UPM6910_REG_01, 0x0a);
	} else {
		return upm6910_update_bits(upm, UPM6910_REG_01, REG01_OTG_CONFIG_MASK,
				   val);
	}

}

static int upm6910_enable_charger(struct upm6910 *upm)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	if (upm->part_no == SGM41513D) {
		ret = upm6910_write_byte(upm, UPM6910_REG_01, 0x1a);
	} else {
		ret =
			upm6910_update_bits(upm, UPM6910_REG_01, REG01_CHG_CONFIG_MASK, val);
	}

	return ret;
}

static int upm6910_disable_charger(struct upm6910 *upm)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	if (upm->part_no == SGM41513D) {
		ret = upm6910_write_byte(upm, UPM6910_REG_01, 0x0a);
	} else {
		ret =
			upm6910_update_bits(upm, UPM6910_REG_01, REG01_CHG_CONFIG_MASK, val);
	}
	return ret;
}

#define sgm41513d_ICHRG_I_MIN_uA			0
#define sgm41513d_ICHRG_I_MAX_uA			3000
int sgm41513d_set_chargecurrent(struct upm6910 *upm, int curr)
{
	u8 ichg;

	if (curr < sgm41513d_ICHRG_I_MIN_uA)
		curr = sgm41513d_ICHRG_I_MIN_uA;
	else if ( curr > sgm41513d_ICHRG_I_MAX_uA)
		curr = sgm41513d_ICHRG_I_MAX_uA;

    if (curr <= 40)
		ichg = curr / 5;
	else if (curr <= 110)
		ichg = 0x08 + (curr -40) / 10;
	else if (curr <= 270)
		ichg = 0x0F + (curr -110) / 20;
	else if (curr <= 540)
		ichg = 0x17 + (curr -270) / 30;
	else if (curr <= 1500)
		ichg = 0x20 + (curr -540) / 60;
	else if (curr <= 2940)
		ichg = 0x30 + (curr -1500) / 120;
	else
		ichg = 0x3d;
    pr_err("sgm41513d_set_ichrg= 0x%x\n", ichg);
	//return upm6910_update_bits(upm, UPM6910_REG_02, REG02_ICHG_MASK,
				  // ichg);
	return upm6910_write_byte(upm, UPM6910_REG_02, (ichg & 0x3f) | (upm->boost_current << 7));
}

int upm6910_set_chargecurrent(struct upm6910 *upm, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;

	return upm6910_update_bits(upm, UPM6910_REG_02, REG02_ICHG_MASK,
				   ichg << REG02_ICHG_SHIFT);
}

int upm6910_set_term_current(struct upm6910 *upm, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return upm6910_update_bits(upm, UPM6910_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_term_current);

/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus start */
struct iio_channel ** upm6910_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}

static bool upm6910_is_pmic_chan_valid(struct upm6910 *upm,
		enum pmic_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!upm->pmic_ext_iio_chans) {
		iio_list = upm6910_get_ext_channels(upm->dev, pmic_iio_chan_name,
		ARRAY_SIZE(pmic_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(upm->dev, "Failed to get channels, %d\n",
					rc);
				upm->pmic_ext_iio_chans = NULL;
			}
			return false;
		}
		upm->pmic_ext_iio_chans = iio_list;
	}

	return true;
}

int upm6910_get_vbus_value(struct upm6910 *upm,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!upm6910_is_pmic_chan_valid(upm, VBUS_VOLTAGE)) {
		dev_err(upm->dev,"read vbus_dect channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = upm->pmic_ext_iio_chans[VBUS_VOLTAGE];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(upm->dev,
		"read vbus_dect channel fail, ret=%d\n", ret);
		return ret;
	}
	//*val = temp / 100;
	*val = temp * 369 / 39;
	dev_err(upm->dev,"%s: vbus_volt = %d \n", __func__, *val);

	return ret;
}

static int upm6910_get_vbus(struct charger_device *chg_dev,  u32 *vbus)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int val;
	int ret;

	ret = upm6910_get_vbus_value(upm, &val);
	if (ret < 0) {
		pr_err("upm6910_get_vbus error \n");
		val = 0;
	}
	*vbus = val * 1000;
	pr_err(" upm_get_vbus :%d \n", val);

	return val;
}
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus end */

enum charg_stat upm6910_get_charging_status(struct upm6910 *upm)
{
        enum charg_stat status = CHRG_STAT_NOT_CHARGING;
        int ret = 0;
        u8 reg_val = 0;

        ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
        if (ret) {
                pr_err("read UPM6910_REG_08 failed, ret:%d\n", ret);
                return ret;
        }

        status = (reg_val & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;

        return status;
}

int upm6910_set_prechg_current(struct upm6910 *upm, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return upm6910_update_bits(upm, UPM6910_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6910_set_prechg_current);

int sgm41513d_set_term_current(struct upm6910 *upm, int curr)
{
	u8 iterm;

	for(iterm = 1; iterm < 16 && curr >= ITERM_CURRENT_STABLE[iterm]; iterm++)
		;
	iterm--;

	return upm6910_update_bits(upm, UPM6910_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513d_set_term_current);

int sgm41513d_set_prechg_current(struct upm6910 *upm, int curr)
{
	u8 iprechg;

	for(iprechg = 1; iprechg < 16 && curr >= IPRECHG_CURRENT_STABLE[iprechg]; iprechg++)
		;
	iprechg--;

	return upm6910_update_bits(upm, UPM6910_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513d_set_prechg_current);

int upm6910_set_chargevolt(struct upm6910 *upm, int volt)
{
	u8 val;
	int ret = 0;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	if (is_upm6910) {
		if(volt == 4350)
			val = 16;
		else if(volt == 4330)
			val = 15;
		else if(volt == 4310)
			val = 14;
		else if(volt == 4280)
			val = 13;
		else if(volt == 4240)
			val = 12;
		else
			val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
	} else {
		val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
		if (upm->part_no == SGM41513D) {
			int temp = ((volt - REG04_VREG_BASE) % REG04_VREG_LSB);
			int cv = 0;

			if (temp > 4 && temp < 12){ // set  +8mV
				cv = 1;
			} else if (temp >= 12 && temp < 22) { //set CV  +step  - 16mV
				cv = 3;
				val += 1;
			} else if (temp > 22) { //set  Cv - 1step, +8mV
				cv = 2;
				val += 1;
			} else {
				cv = 0;
			}

			if (val == 15)
				cv = 2;
			ret = upm6910_update_bits(upm, SGM41513D_REG_0F, REG0F_VREG_FT_MASK,
							cv << REG0F_VREG_FT_SHIFT);
		}
	}
	ret =  upm6910_update_bits(upm, UPM6910_REG_04, REG04_VREG_MASK,
				   val << REG04_VREG_SHIFT);
	if (ret < 0) {
		pr_err("write UPM6910_REG_04 fail, ret:%d\n", ret);
		return ret;
	}

	return ret;
}

int upm6910_set_rechargeth(struct upm6910 *upm, int rechargeth)
{
	int ret = 0;
	u8 val;

/* +S98901AA1 liangjianfeng wt, modify, 20240906, modify for add sgm41513 adapter */
	if (rechargeth <= 100) {
		val = REG04_VRECHG_100MV << REG04_VRECHG_SHIFT;
	} else {
		val = REG04_VRECHG_200MV << REG04_VRECHG_SHIFT;
	}
/* -S98901AA1 liangjianfeng wt, modify, 20240906, modify for add sgm41513 adapter */

	ret =  upm6910_update_bits(upm, UPM6910_REG_04, REG04_VRECHG_MASK, val);
	if (ret < 0) {
		pr_err("write rechargeth UPM6910_REG_04 fail, ret:%d\n", ret);
		return ret;
	}
	return ret;
}

static int upm6910_set_recharge(struct charger_device *chg_dev, u32 volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("recharge = %d\n", volt);
	return upm6910_set_rechargeth(upm, volt / 1000);
}

int upm6910_set_input_volt_limit(struct upm6910 *upm, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return upm6910_update_bits(upm, UPM6910_REG_06, REG06_VINDPM_MASK,
				   val << REG06_VINDPM_SHIFT);
}

static int sgm41513d_set_input_volt_limit(struct upm6910 *upm, int mv)
{
	u8 volt_val, os_val;
	int ret = 0;

	pr_info("mv:%d\n", mv);
	if (mv < REG06_VINDPM_3P9V ||
	    mv > REG06_VINDPM_12P0V)
		return -EINVAL;

	if (mv < REG06_VINDPM_5P9V) {
		volt_val = (mv - REG06_VINDPM_3P9V) / REG06_VINDPM_LSB;
		os_val = REG0F_VINDPM_OS_3P9V;
	} else if (mv >= REG06_VINDPM_5P9V && mv < REG06_VINDPM_7P5V) {
		volt_val = (mv - REG06_VINDPM_5P9V) / REG06_VINDPM_LSB;
		os_val = REG0F_VINDPM_OS_5P9V;
	} else if (mv >= REG06_VINDPM_7P5V && mv < REG06_VINDPM_10P5V) {
		volt_val = (mv - REG06_VINDPM_7P5V) / REG06_VINDPM_LSB;
		os_val = REG0F_VINDPM_OS_7P5V;
	} else {
		volt_val = (mv - REG06_VINDPM_10P5V) / REG06_VINDPM_LSB;
		os_val = REG0F_VINDPM_OS_10P5V;
	}

	ret =  upm6910_update_bits(upm, UPM6910_REG_06, REG06_VINDPM_MASK,
				   volt_val << REG06_VINDPM_SHIFT);
	if (ret < 0) {
		pr_err("write SGM41513D_REG_06 fail, ret:%d\n", ret);
		return ret;
	}

	ret =  upm6910_update_bits(upm, SGM41513D_REG_0F, REG0F_VINDPM_OS_MASK,
				   os_val << REG0F_VINDPM_OS_SHIFT);
	if (ret < 0) {
		pr_err("write SGM41513D_REG_0F fail, ret:%d\n", ret);
	}

	return ret;
}

static int sgm41513d_get_vindpm_os(struct upm6910 *upm)
{
	u8 reg_val;
	int ret,os_val;

	ret = upm6910_read_byte(upm, SGM41513D_REG_0F, &reg_val);
	if (ret != 0)
		return ret;

	os_val = (reg_val & REG0F_VINDPM_OS_MASK) >> REG0F_VINDPM_OS_SHIFT;
	pr_info("os_val:%d\n", os_val);

	return os_val;
}

static int sgm41513d_get_input_volt_limit(struct upm6910 *upm, int *volt)
{
	u8 volt_val;
	int ret,os_val,vindpm;

	ret = upm6910_read_byte(upm, UPM6910_REG_06, &volt_val);
	if (ret != 0)
		return ret;

	vindpm = (volt_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
	os_val = sgm41513d_get_vindpm_os(upm);
	if (REG0F_VINDPM_OS_3P9V == os_val)
		vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_3P9V;
	else if (REG0F_VINDPM_OS_5P9V == os_val)
		vindpm = vindpm * REG06_VINDPM_LSB +  REG06_VINDPM_5P9V;
	else if (REG0F_VINDPM_OS_7P5V == os_val)
		vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_7P5V;
	else if (REG0F_VINDPM_OS_10P5V == os_val)
		vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_10P5V;
	*volt = vindpm;
	pr_info("vindpm:%d\n", vindpm);

	return ret;
}

static int upm6910_set_ivl(struct charger_device *chg_dev, u32 volt);
#define SGM415xx_VINDPM_MAX  12000000
static int sgm41513d_enable_power_path(struct charger_device *chg_dev, bool en)
{
	int ret = 0,init_data_vlim = 0;
	u32 mivr ;
	static bool old_state = 0;
	static 	u32 old_mivr = 0;

	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	if(IS_ERR_OR_NULL(upm))
		return -ENOMEM;
	if (old_state == en) {
		pr_err("already is set\n");
		return 0;
	}

	if (en) {
		sgm41513d_get_input_volt_limit(upm,&init_data_vlim);
		old_mivr = init_data_vlim * 1000;
		mivr = 	SGM415xx_VINDPM_MAX;
	} else {
		if (old_mivr ==  0){
			sgm41513d_get_input_volt_limit(upm,&init_data_vlim);
			old_mivr = init_data_vlim * 1000;
		}
		mivr = 	old_mivr;
	}

	pr_info("old_mivr=%d,mivr=%d,en=%d\n",old_mivr,mivr,en);
	ret = upm6910_set_ivl(chg_dev,mivr);
	if (ret) {
		dev_err(upm->dev, "%s:power path set failed\n", __func__);
		return ret;
	}
	old_state = en;
	return 0;
}
#if 0
static int upm6910_get_input_volt_limit(struct upm6910 *upm, int *volt)
{
	u8 val;
	int ret;
	int vindpm;

	ret = upm6910_read_byte(upm, UPM6910_REG_06, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_06, val);
	}

	pr_info("vindpm_reg_val:0x%X\n", val);
	vindpm = (val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
	vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
	*volt = vindpm;

	return ret;
}
#endif
static int upm6910_get_input_current_limit(struct upm6910 *upm)
{
	u8 val = 0;
	int ret = 0;
	int iindpm = 0;

	ret = upm6910_read_byte(upm, UPM6910_REG_00, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_00, val);
	}

	pr_info("indpm_reg_val:0x%X\n", val);
	iindpm = (val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
	iindpm = iindpm * REG00_IINLIM_LSB + REG00_IINLIM_BASE;


	return iindpm;
}


int upm6910_set_input_current_limit(struct upm6910 *upm, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	if (upm->part_no == SGM41513D) {
		return upm6910_write_byte(upm, UPM6910_REG_00, val & 0x1f);
	} else {
		return upm6910_update_bits(upm, UPM6910_REG_00, REG00_IINLIM_MASK,
				   val << REG00_IINLIM_SHIFT);
	}
}

int upm6910_set_watchdog_timer(struct upm6910 *upm, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
		       REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return upm6910_update_bits(upm, UPM6910_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(upm6910_set_watchdog_timer);

int upm6910_disable_watchdog_timer(struct upm6910 *upm)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6910_disable_watchdog_timer);

int upm6910_reset_watchdog_timer(struct upm6910 *upm)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	if (upm->part_no == SGM41513D) {
		return upm6910_write_byte(upm, UPM6910_REG_01, val | (upm->charge_enabled << 4) | 0x0a);
	} else {
		return upm6910_update_bits(upm, UPM6910_REG_01, REG01_WDT_RESET_MASK,
				   val);
	}
}
EXPORT_SYMBOL_GPL(upm6910_reset_watchdog_timer);

int upm6910_reset_chip(struct upm6910 *upm)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
	    upm6910_update_bits(upm, UPM6910_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(upm6910_reset_chip);

int upm6910_enter_hiz_mode(struct upm6910 *upm)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(upm6910_enter_hiz_mode);

int upm6910_exit_hiz_mode(struct upm6910 *upm)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(upm6910_exit_hiz_mode);

static int upm6910_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	if (upm->part_no == SGM41513D) {
		upm->hiz_state = en;
		ret = sgm41513d_enable_power_path(chg_dev,en);
	} else {
		if (en)
			ret = upm6910_enter_hiz_mode(upm);
		else
			ret = upm6910_exit_hiz_mode(upm);
	}
	pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}
//EXPORT_SYMBOL_GPL(upm6910_set_hiz_mode);

#if 0
static int upm6910_get_hiz_mode(struct charger_device *chg_dev)
{
	int ret;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	ret = upm6910_read_byte(upm, UPM6910_REG_00, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_00, val);
	}

	ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
	pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

	return ret;
}
//EXPORT_SYMBOL_GPL(upm6910_get_hiz_mode);
#endif

static int upm6910_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    dev_info(upm->dev, "%s event = %d\n", __func__, event);

/* +S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */
    switch (event) {
    case EVENT_FULL:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
        break;
    case EVENT_RECHARGE:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
        break;
    default:
        break;
    }
/* -S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */

    return 0;
}

static int upm6910_enable_term(struct charger_device *chg_dev, bool enable)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	int ret;

	pr_err("%s, enable=%d\n", __func__, enable);
	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
//EXPORT_SYMBOL_GPL(upm6910_enable_term);

int upm6910_set_boost_current(struct upm6910 *upm, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr >= BOOSTI_1200) //max otg current 1.2A
		val = REG02_BOOST_LIM_1P2A;
	upm->boost_current = val;
	return upm6910_update_bits(upm, UPM6910_REG_02, REG02_BOOST_LIM_MASK,
				   val << REG02_BOOST_LIM_SHIFT);
}

int upm6910_set_boost_voltage(struct upm6910 *upm, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5V;


	return upm6910_update_bits(upm, UPM6910_REG_06, REG06_BOOSTV_MASK,
				   val << REG06_BOOSTV_SHIFT);
}
//EXPORT_SYMBOL_GPL(upm6910_set_boost_voltage);

static int upm6910_set_acovp_threshold(struct upm6910 *upm, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14P0V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P5V;

	return upm6910_update_bits(upm, UPM6910_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}
//EXPORT_SYMBOL_GPL(upm6910_set_acovp_threshold);

static int upm6910_set_stat_ctrl(struct upm6910 *upm, int ctrl)
{
	u8 val;

	val = ctrl;
	if (upm->part_no == SGM41513D) {
		//upm6910_write_byte(upm, UPM6910_REG_00, val & 0x7f);
		return 0;
	} else {
		return upm6910_update_bits(upm, UPM6910_REG_00, REG00_STAT_CTRL_MASK,
				   val << REG00_STAT_CTRL_SHIFT);
	}
}

static int upm6910_set_int_mask(struct upm6910 *upm, int mask)
{
	u8 val;

	val = mask;

	return upm6910_update_bits(upm, UPM6910_REG_0A, REG0A_INT_MASK_MASK,
				   val << REG0A_INT_MASK_SHIFT);
}

static int upm6910_force_dpdm(struct upm6910 *upm)
{
	const u8 val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_FORCE_DPDM_MASK,
				val);
}
#if 0
static int upm6910_enable_batfet(struct upm6910 *upm)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_enable_batfet);
#endif

static int upm6910_disable_batfet(struct upm6910 *upm)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_disable_batfet);

static int upm6910_set_batfet_delay(struct upm6910 *upm, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DLY_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_batfet_delay);

static int upm6910_set_batfet_reset(struct upm6910 *upm, uint8_t value)
{
	u8 val;

	if (value == 0)
		val = REG07_BATFET_RST_DISABLE;
	else
		val = REG07_BATFET_RST_ENABLE;

	val <<= REG07_BATFET_RST_EN_SHIFT;
	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_RST_EN_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_batfet_reset);

static int upm6910_enable_safety_timer(struct upm6910 *upm)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_enable_safety_timer);

static int upm6910_disable_safety_timer(struct upm6910 *upm)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_disable_safety_timer);

static int upm6910_enable_hvdcp(struct upm6910 *upm, bool en)
{
	const u8 val = en << REG0C_EN_HVDCP_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_EN_HVDCP_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_enable_hvdcp);

static int upm6910_set_dp(struct upm6910 *upm, int dp_stat)
{
	const u8 val = dp_stat << REG0C_DP_MUX_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_DP_MUX_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_dp);
#if 0
static int upm6910_set_dm(struct upm6910 *upm, int dm_stat)
{
	const u8 val = dm_stat << REG0C_DM_MUX_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_DM_MUX_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_dm);
#endif
static int upm6910_set_dp_for_afc(struct upm6910 *upm)
{
    int ret;

    ret = upm6910_enable_hvdcp(upm, REG0C_EN_HVDCP_ENABLE);
    if (ret) {
		pr_err("upm6910_enable_hvdcp failed(%d)\n", ret);
	}

    ret = upm6910_set_dp(upm, REG0C_DPDM_OUT_0P6V);
    if (ret) {
		pr_err("upm6910_set_dp failed(%d)\n", ret);
	}

	return ret;
}
//EXPORT_SYMBOL_GPL(upm6910_set_dp_for_afc);

static int sgm41513d_set_dp(struct upm6910 *upm, int dp_stat)
{
	const u8 val = dp_stat << REG0C_DP_MUX_SHIFT;

	return upm6910_update_bits(upm, SGM41513D_REG_0D, REG0C_DP_MUX_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(sgm41513d_set_dp);

static int sgm41513d_recover_dp_for_afc(struct upm6910 *upm)
{
    int ret;

    ret = sgm41513d_set_dp(upm, REG0C_DPDM_OUT_HIZ);
    if (ret) {
		pr_err("sgm41513d_set_dp failed(%d)\n", ret);
	}

	return ret;
}
#if 0
static int sgm41513d_set_dm(struct upm6910 *upm, int dm_stat)
{
	const u8 val = dm_stat << REG0C_DM_MUX_SHIFT;

	return upm6910_update_bits(upm, SGM41513D_REG_0D, REG0C_DM_MUX_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(sgm41513d_set_dm);
#endif
static int sgm41513d_set_dp_for_afc(struct upm6910 *upm)
{
    int ret;

    ret = sgm41513d_set_dp(upm, REG0C_DPDM_OUT_0P6V);
    if (ret) {
		pr_err("sgm41513d_set_dp failed(%d)\n", ret);
	}

	return ret;
}
//EXPORT_SYMBOL_GPL(sgm41513d_set_dp_for_afc);

/*
static int upm6910_dpdm_set_hidden_mode(struct upm6910 *upm)
{
	int ret = 0;
	union power_supply_propval propval;

	if (!upm->bat_psy) {
		upm->bat_psy = power_supply_get_by_name("battery");
		if (!upm->bat_psy) {
			pr_err("%s Couldn't get battery psy\n", __func__);
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(upm->bat_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
	if (ret || propval.intval <= 3450000) {
		pr_err("%s cannot goto hidden mode\n", __func__);
		return -EINVAL;
	}

	ret = upm6910_write_byte(upm, 0xA9, 0x6E);
	if (ret) {
		pr_err("write 0xA9, 0x6E failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xB1, 0x80);
	if (ret) {
		pr_err("write 0xB1, 0x80 failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xB0, 0x21);
	if (ret) {
		pr_err("write 0xB0, 0x21 failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xB1, 0x00);
	if (ret) {
		pr_err("write 0xB1, 0x00 failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xA9, 0x00);
	if (ret) {
		pr_err("write 0xA9, 0x00 failed(%d)\n", ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(upm6910_dpdm_set_hidden_mode);
*/

static int upm6910_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = upm6910_read_byte(upm, UPM6910_REG_02, &reg_val);
	if (!ret) {
		if(upm->part_no == SGM41513D){
			reg_val &= REG02_ICHG_MASK;
			if (reg_val <= 0x8)
				ichg = reg_val * 5;
			else if (reg_val <= 0xF)
				ichg = 40 + (reg_val - 0x8) * 10;
			else if (reg_val <= 0x17)
				ichg = 110 + (reg_val - 0xF) * 20;
			else if (reg_val <= 0x20)
				ichg = 270 + (reg_val - 0x17) * 30;
			else if (reg_val <= 0x30)
				ichg = 540 + (reg_val - 0x20) * 60;
			else if (reg_val <= 0x3C)
				ichg = 1500 + (reg_val - 0x30) * 120;
			else
				ichg = 3000;
			*curr = ichg * 1000;
			pr_err("sgm41513d curr = %d\n", *curr);
		} else {
			ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
			ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
			*curr = ichg * 1000;
		}
	}
	pr_err("%s curr=%d\n", __func__, *curr);
	return ret;
}

static int upm6910_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = upm6910_read_byte(upm, UPM6910_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;
}

static int upm6910_chg_set_usbsw(struct upm6910 *upm,
                enum upm6910_usbsw usbsw)
{
    struct phy *phy;
    int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
                           PHY_MODE_BC11_CLR;

    dev_err(upm->dev, "usbsw=%d\n", usbsw);
    phy = phy_get(upm->dev, "usb2-phy");
    if (IS_ERR_OR_NULL(phy)) {
        dev_err(upm->dev, "failed to get usb2-phy\n");
        return -ENODEV;
    }
    ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
    if (ret)
        dev_err(upm->dev, "failed to set phy ext mode\n");
    phy_put(upm->dev, phy);
    return ret;
}


static struct upm6910_platform_data *upm6910_parse_dt(struct device_node *np,
						      struct upm6910 *upm)
{
	int ret;
	struct upm6910_platform_data *pdata;

	pdata = devm_kzalloc(&upm->client->dev, sizeof(struct upm6910_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &upm->chg_dev_name) < 0) {
		upm->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "upm,eint_name", &upm->eint_name) < 0) {
		upm->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	ret = of_get_named_gpio(np, "intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no upm,intr_gpio(%d)\n",
				      __func__, ret);
	} else
		upm->intr_gpio = ret;

	pr_info("%s intr_gpio = %u\n", __func__, upm->intr_gpio);

	upm->chg_det_enable = of_property_read_bool(np, "upm,upm6910,charge-detect-enable");

	ret = of_property_read_u32(np, "upm,upm6910,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of upm,upm6910,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of upm,upm6910,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of upm,upm6910,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of upm,upm6910,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of upm,upm6910,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of upm,upm6910,precharge-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of upm,upm6910,termination-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of upm,upm6910,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of upm,upm6910,boost-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of upm,upm6910,vac-ovp-threshold\n");
	}

	return pdata;
}
#if 0
static bool is_hiz_mode_trigger_interrupt(struct upm6910 *upm)
{
	bool exist = false;
	int ret = 0, vbus = 0, hiz_en = 0;
	u8 reg_val = 0, vbus_stat = 0;
	union power_supply_propval propval;

	if (!upm->usb_psy) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (!upm->usb_psy) {
			pr_err("%s Couldn't get usb psy\n", __func__);
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(upm->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
	if (!ret) {
		vbus = propval.intval;
	}

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
	if (!ret) {
		vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
		vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	}

	ret = upm6910_read_byte(upm, UPM6910_REG_00, &reg_val);
	if (!ret) {
		hiz_en = !!(reg_val & REG00_ENHIZ_MASK);
	}

	if (vbus > 4000 && vbus_stat != REG08_VBUS_TYPE_OTG && hiz_en) {
		exist = true;
	}

	return exist;
}
#endif
static int upm6910_get_charger_type(struct upm6910 *upm)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat = 0;

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
	if (ret)
		return ret;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	pr_info("[%s] reg08: 0x%02x, float_flag: %d\n", __func__, reg_val, upm->float_flag);

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		upm->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		upm->chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case REG08_VBUS_TYPE_CDP:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		upm->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case REG08_VBUS_TYPE_DCP:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		upm6910_set_input_current_limit(upm, 1500);
		upm6910_set_chargecurrent(upm, 2000);
		if(upm->part_no == SGM41513D)
			ret = sgm41513d_set_dp_for_afc(upm);
		else
			ret = upm6910_set_dp_for_afc(upm);
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
	case REG08_VBUS_TYPE_NON_STD:
		if (upm->float_flag) {
			upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			if (vbus_stat == REG08_VBUS_TYPE_NON_STD) {
				if(upm->part_no == SGM41513D)
					ret = sgm41513d_set_dp_for_afc(upm);
				else
					ret = upm6910_set_dp_for_afc(upm);
			}
		} else {
			upm->float_flag = true;
			upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		}
		upm->chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	default:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	}
	if (upm->is_need_bc12
		&& ((upm->psy_desc.type == POWER_SUPPLY_TYPE_USB)
		|| (upm->psy_desc.type == POWER_SUPPLY_TYPE_USB_CDP))) {
		//Charger_Detect_Release();
		upm6910_chg_set_usbsw(upm, USBSW_USB);
	}

	return 0;
}

static bool is_upm6910_power_good(struct upm6910 *upm)
{
	int ret = 0;
	u8 reg_val = 0;
	bool pg_stat = false;

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
	if (!ret) {
		pg_stat = !!(reg_val & REG08_PG_STAT_MASK);
		if (!pg_stat) {
			pr_err("%s:pg_stat is false, may be plug out charger\n", __func__);
			return false;
		}
	} else {
		pr_err("%s:read pg_stat fail, don't recheck type\n", __func__);
		return false;
	}

	return true;
}

extern int get_usb_state_from_musb_core(void);
static void wtchg_float_charge_type_check(struct work_struct *work)
{
	struct upm6910 *upm = container_of(work, struct upm6910, wt_float_recheck_work.work);
	int ret = 0;
	int usb_state = 0;

	if (!is_upm6910_power_good(upm) || !upm->is_need_bc12 || !upm->is_need_retry_bc12)
		return;

	usb_state = get_usb_state_from_musb_core();
	pr_err("%s: enter,usb_state=%d\n", __func__, usb_state);

	if ((upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_DCP)
		|| (((upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
		|| (upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_CDP))
		&& (usb_state == 0) && (upm->bootmode != 8))
		) {
		upm6910_chg_set_usbsw(upm, USBSW_CHG);
		msleep(50);
		upm6910_force_dpdm(upm);
		msleep(600);

		if (!is_upm6910_power_good(upm)) {
			return;
		}

		ret = upm6910_get_charger_type(upm);
		if (ret)
			pr_err("upm6910 irq handle, get charger type failed\n");
		pr_err("the last type is :%d, bc12 retry secussfully\n",upm->psy_desc.type);

		if (upm->psy) {
			power_supply_changed(upm->psy);
		}
	}

	upm->bc12_retry_done = true;

}

static void upm6910_inform_psy_dwork_handler(struct work_struct *work)
{
	struct upm6910 *upm = container_of(work, struct upm6910,
								psy_dwork.work);

	if (upm->psy) {
		if (upm->bootmode != 8) {
			msleep(150);
		}
		power_supply_changed(upm->psy);
	}

	if (!upm->is_need_bc12) {
		return;
	}
	if ((upm->psy_desc.type == POWER_SUPPLY_TYPE_USB ||upm->psy_desc.type == POWER_SUPPLY_TYPE_USB_CDP)
		&& (!upm->bc12_retry_done)) {
		cancel_delayed_work(&upm->wt_float_recheck_work);
		schedule_delayed_work(&upm->wt_float_recheck_work, msecs_to_jiffies(3000));
	}
}

extern signed int battery_get_vbus(void);
//extern int upm6720_adc_en(bool enable);

static irqreturn_t upm6910_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;

    struct upm6910 *upm = (struct upm6910 *)data;

#ifdef WT_COMPILE_FACTORY_VERSION
	if(upm->probe_flag == 0)
		return IRQ_HANDLED;
#endif

	ret = upm6910_read_byte(upm, UPM6910_REG_0A, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_vbus_gd = upm->vbus_gd;

	upm->vbus_gd = !!(reg_val & REG0A_VBUS_GD_MASK);

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = upm->power_good;
	upm->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	pr_err("%s: vbus_gd=%d, power_good=%d\n", __func__, upm->vbus_gd, upm->power_good);
/*+S96818AA1-1936,zhouxiaopeng2.wt,MODIFY,20230517,optimize interrupt function flow*/
	if (!prev_vbus_gd && upm->vbus_gd) {
		//Charger_Detect_Init();
		if (upm->part_no == SGM41513D) {
			upm->hiz_state = SGM_NO_HIZ;
		}
		if (upm->is_need_bc12) {
			upm6910_chg_set_usbsw(upm, USBSW_CHG);
		}
		upm6910_enable_term(upm->chg_dev, true);
		upm->bc12_retry_done = false;
		upm->is_need_retry_bc12 = true;
		pr_err("adapter/usb inserted\n");
	} else if (prev_vbus_gd && !upm->vbus_gd) {
		if (upm->part_no == SGM41513D) {
			upm->hiz_state = SGM_NO_HIZ;
		}
		ret = upm6910_get_charger_type(upm);
		upm->is_need_bc12 = true;
		upm->float_flag = false;
		cancel_delayed_work(&upm->wt_float_recheck_work);
		//Charger_Detect_Release();
		upm6910_chg_set_usbsw(upm, USBSW_USB);
		//upm6720_adc_en(0);
		if (upm->psy) {
			power_supply_changed(upm->psy);
		}
		pr_err("adapter/usb removed\n");
		upm->chg_old_status = POWER_SUPPLY_STATUS_UNKNOWN;
		return IRQ_HANDLED;
	}

	if (!prev_pg && upm->power_good) {
		ret = upm6910_get_charger_type(upm);
		if (upm->psy_desc.type != POWER_SUPPLY_TYPE_UNKNOWN) {
			upm6910_set_input_current_limit(upm, 500);
			upm6910_set_chargecurrent(upm, 500);
			if (s_chg_dev_otg) {
				upm6910_charging(s_chg_dev_otg, true);
			}
		}
		schedule_delayed_work(&upm->psy_dwork, 0);
	}
/*-S96818AA1-1936,zhouxiaopeng2.wt,MODIFY,20230517,optimize interrupt function flow*/

	return IRQ_HANDLED;
}

static int upm6910_register_interrupt(struct upm6910 *upm)
{
	int ret = 0;
	ret = devm_gpio_request_one(upm->dev, upm->intr_gpio, GPIOF_DIR_IN,
			devm_kasprintf(upm->dev, GFP_KERNEL,
			"upm6910_intr_gpio.%s", dev_name(upm->dev)));
	if (ret < 0) {
		pr_err("%s gpio request fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	upm->client->irq = gpio_to_irq(upm->intr_gpio);
	if (upm->client->irq < 0) {
		pr_err("%s gpio2irq fail(%d)\n",
				      __func__, upm->client->irq);
		return upm->client->irq;
	}
	pr_info("%s irq = %d\n", __func__, upm->client->irq);

	ret = devm_request_threaded_irq(upm->dev, upm->client->irq, NULL,
					upm6910_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"upm_irq", upm);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(upm->irq);

	return 0;
}

static int upm6910_init_device(struct upm6910 *upm)
{
	int ret;

	upm6910_disable_watchdog_timer(upm);

	if (upm->part_no == SGM41513D) {
		sgm41513d_recover_dp_for_afc(upm);
		upm6910_set_chargevolt(upm, UPM6910_CV_VOLTAGE);
		upm6910_set_rechargeth(upm, 100);
	} else {
		upm6910_set_rechargeth(upm, 100);
	}

	ret = upm6910_set_stat_ctrl(upm, upm->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);
	if(upm->part_no == SGM41513D)
		ret = sgm41513d_set_prechg_current(upm, upm->platform_data->iprechg);
	else
		ret = upm6910_set_prechg_current(upm, upm->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);
	if(upm->part_no == SGM41513D)
		ret = sgm41513d_set_term_current(upm, upm->platform_data->iterm);
	else
		ret = upm6910_set_term_current(upm, upm->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = upm6910_set_boost_voltage(upm, upm->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = upm6910_set_boost_current(upm, upm->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = upm6910_set_acovp_threshold(upm, upm->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret = upm6910_set_batfet_reset(upm, 0);
	if (ret)
		pr_err("Failed to set batfet reset disable, ret = %d\n", ret);

	ret = upm6910_set_int_mask(upm,
				   REG0A_IINDPM_INT_MASK |
				   REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");
/*+S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
	ret = upm6910_disable_safety_timer(upm);
	if (ret)
		pr_err("Failed to disable safety timer function\n");
/*-S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
	return 0;
}

static void upm6910_inform_prob_dwork_handler(struct work_struct *work)
{
	struct upm6910 *upm = container_of(work, struct upm6910,
								prob_dwork.work);
	if (upm->is_usb_compatible_ok && !IS_ERR_OR_NULL(upm->usb_node)) {
		upm->usb_pdev = of_find_device_by_node(upm->usb_node);
		of_node_put(upm->usb_node);
	}

	if (!IS_ERR_OR_NULL(upm->usb_pdev)) {
		upm->musb = platform_get_drvdata(upm->usb_pdev);
		if (IS_ERR_OR_NULL(upm->musb)) {
			pr_err("fail to get musb\n");
		} else {
			pr_err("get musb succeffully\n");
		}
		put_device(&upm->usb_pdev->dev);
	} else {
		pr_err("fail to find usb platform device\n");
	}
	//Charger_Detect_Init();
	//if (upm->part_no == SGM41513D)
	upm6910_chg_set_usbsw(upm, USBSW_CHG);
	msleep(50);
	//upm6910_dpdm_set_hidden_mode(upm);
	//msleep(700);
	upm6910_force_dpdm(upm);
	msleep(300);
#ifdef WT_COMPILE_FACTORY_VERSION
	upm->probe_flag = 1;
#endif
	upm6910_irq_handler(upm->irq, (void *) upm);
}

static int upm6910_detect_device(struct upm6910 *upm)
{
	int ret;
	u8 data;

	pr_err("part_no= 0x%x\n", upm->part_no);
	ret = upm6910_read_byte(upm, UPM6910_REG_0B, &data);
	if (!ret) {
		upm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		upm->revision =
		    (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void upm6910_dump_regs(struct upm6910 *upm)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0F; addr++) {
		ret = upm6910_read_byte(upm, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static void sgm41513d_dump_regs(struct upm6910 *upm)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0F; addr++) {
		ret = upm6910_read_byte(upm, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
upm6910_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct upm6910 *upm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	if(upm->part_no == SGM41513D)
		idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sgm41513d Reg");
	else
		idx = snprintf(buf, PAGE_SIZE, "%s:\n", "upm6910 Reg");
	for (addr = 0x0; addr <= 0x0F; addr++) {
		ret = upm6910_read_byte(upm, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
upm6910_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct upm6910 *upm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0F) {
		upm6910_write_byte(upm, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, upm6910_show_registers,
		   upm6910_store_registers);

static struct attribute *upm6910_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group upm6910_attr_group = {
	.attrs = upm6910_attributes,
};

static int upm6910_charging(struct charger_device *chg_dev, bool enable)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val[3];

	if (enable)
		ret = upm6910_enable_charger(upm);
	else
		ret = upm6910_disable_charger(upm);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = upm6910_read_byte(upm, UPM6910_REG_01, &val[0]);
	ret |= upm6910_read_byte(upm, UPM6910_REG_01, &val[1]);

	//pr_err("i2c_read_01: val[0]=%d, val[1]=%d\n", val[0], val[1]);

	if (!ret) {
		if (val[0] != val[1]) {
			ret = upm6910_read_byte(upm, UPM6910_REG_01, &val[0]);
		}
		if (!ret)
			upm->charge_enabled = !!(val[0] & REG01_CHG_CONFIG_MASK);
	}
	return ret;
}

static int upm6910_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = upm6910_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int upm6910_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = upm6910_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int upm6910_dump_register(struct charger_device *chg_dev)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	if(upm->part_no == SGM41513D)
		sgm41513d_dump_regs(upm);
	else
		upm6910_dump_regs(upm);
	return 0;
}

static int upm6910_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	*en = upm->charge_enabled;

	return 0;
}

static int upm6910_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}
	dev_info(upm->dev, "[%s] Can't should run!!!\n", __func__);

	return ret;
}

static int upm6910_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	if(upm->part_no == SGM41513D)
		return sgm41513d_set_chargecurrent(upm, curr / 1000);
	else
		return upm6910_set_chargecurrent(upm, curr / 1000);
}

static int upm6910_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int upm6910_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return upm6910_set_chargevolt(upm, volt / 1000);
}

static int upm6910_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = upm6910_read_byte(upm, UPM6910_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int upm6910_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);
	upm->old_mivr = volt;
	if (upm->part_no == SGM41513D) {
		if ((upm->hiz_state == SGM_ENTER_HIZ)&&(volt != SGM415xx_VINDPM_MAX))
			return 0;
		else
			return sgm41513d_set_input_volt_limit(upm, volt / 1000);
	}
	return upm6910_set_input_volt_limit(upm, volt / 1000);
}

static int upm6910_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	if (upm->old_mivr > 0) {
		*uV = upm->old_mivr;
	}
	pr_err("old_mivr = %d\n", upm->old_mivr);
	return 0;
}

static int upm6910_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return upm6910_set_input_current_limit(upm, curr / 1000);
}

static int upm6910_kick_wdt(struct charger_device *chg_dev)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	return upm6910_reset_watchdog_timer(upm);
}

/* +S96818AA1-1936,zhouxiaopeng2.wt,add,20230803,Modify OTG current path */
extern int upm6720_set_otg_txmode(bool enable);
extern int sp2130_set_otg_txmode(bool enable);

static int upm6910_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	u8 val;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	union power_supply_propval propval;

	if (en) {
/* +S96818AA1-5256, zhouxiaopeng2.wt, MODIFY, 20230614, Cancel hiz mode before setting OTG */
		ret = upm6910_read_byte(upm, UPM6910_REG_00, &val);
		if (!ret && ((val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT)) {
			ret = upm6910_exit_hiz_mode(upm);
		}
		ret = upm6910_enable_otg(upm);
/* -S96818AA1-5256, zhouxiaopeng2.wt, MODIFY, 20230614, Cancel hiz mode before setting OTG */
		upm6720_set_otg_txmode(1);
		sp2130_set_otg_txmode(1);
		upm->otg_flag = 1;
	} else {
		ret = upm6910_disable_otg(upm);
		//if (upm->otg_flag) {
				upm6720_set_otg_txmode(0);
				sp2130_set_otg_txmode(0);
				upm->otg_flag = 0;
		//}
	}

/* +S96818AA1-5256, zhouxiaopeng2.wt, MODIFY, 20230609, add register print information for OTG */
	upm6910_dump_regs(upm);
/* +S96818AA1-5256, zhouxiaopeng2.wt, MODIFY, 20230609, add register print information for OTG */
	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

/* +S96818AA1-2041, churui1.wt, ADD, 20230606, the node obtains OTG information */
	if (!upm->otg_psy)
		upm->otg_psy = power_supply_get_by_name("otg");
	if (!upm->otg_psy) {
		pr_err("get power supply failed\n");
		return -EINVAL;
	}

	propval.intval = en;
	ret = power_supply_set_property(upm->otg_psy, POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0) {
		pr_err("otg_psy type fail(%d)\n", ret);
		return ret;
	}
/*
	propval.intval = en;
	ret = power_supply_set_property(upm->otg_psy, POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0) {
		pr_err("otg_psy online fail(%d)\n", ret);
		return ret;
	}
*/
/* -S96818AA1-2041, churui1.wt, ADD, 20230606, the node obtains OTG information */

	return ret;
}
/* -S96818AA1-1936,zhouxiaopeng2.wt,add,20230803,Modify OTG current path */


static int upm6910_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = upm6910_enable_safety_timer(upm);
	else
		ret = upm6910_disable_safety_timer(upm);

	return ret;
}

static int upm6910_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = upm6910_read_byte(upm, UPM6910_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int upm6910_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = upm6910_set_boost_current(upm, curr / 1000);

	return ret;
}

static int upm6910_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = -1;
	struct upm6910 *upm = NULL;
	u8 reg_val = 0;
	int vbus_stat = 0;

	if (NULL == chg_dev) {
		pr_err("chg_dev is NULL \n");
		return ret;
	}

	upm = dev_get_drvdata(&chg_dev->dev);
	if (NULL == upm) {
		pr_err("upm is NULL \n");
		return ret;
	}

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
	if (ret)
		return -1;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	if (vbus_stat != REG08_VBUS_TYPE_SDP && vbus_stat != REG08_VBUS_TYPE_CDP) {
		pr_err("vbus_stat = %d\n", vbus_stat);
		return -1;
	}

	pr_err("en = %d\n", en);
	if (en) {
		//Charger_Detect_Init();
		if (upm->is_need_bc12) {
			upm6910_chg_set_usbsw(upm, USBSW_CHG);
		}
		ret = upm6910_get_charger_type(upm);
		schedule_delayed_work(&upm->psy_dwork, 0);
	}
	return ret;
}

/* S96818AA3 gujiayin.wt,modify,2024/08/22 otg bring up satrt */
static int upm6910_enable_vbus(struct regulator_dev *rdev)
{
	struct upm6910 *upm = charger_get_data(s_chg_dev_otg);
	int ret = 0;

	pr_notice("%s ente\r\n", __func__);

	if (IS_ERR_OR_NULL(upm->chg_dev)) {
		ret = PTR_ERR(upm->chg_dev);
		return ret;
	}

	upm6910_set_otg(upm->chg_dev, true);
	return ret;
}

static int upm6910_disable_vbus(struct regulator_dev *rdev)
{
	struct upm6910 *upm = charger_get_data(s_chg_dev_otg);
	int ret = 0;

	if (IS_ERR_OR_NULL(upm->chg_dev)) {
		ret = PTR_ERR(upm->chg_dev);
		return ret;
	}

	upm6910_set_otg(upm->chg_dev, false);

	return ret;
}

static int upm6910_is_enabled_vbus(struct regulator_dev *rdev)
{
	struct upm6910 *upm = charger_get_data(s_chg_dev_otg);
	int ret = 0;
	u8 pg_stat = 0;
	u8 val = 0;

	pr_notice("%s enter\n", __func__);

	ret = upm6910_read_byte(upm, UPM6910_REG_01, &pg_stat);
	if(ret)
		pr_notice("%s is enabled vbus fail\n", __func__);
	val = pg_stat & REG01_OTG_CONFIG_MASK;
	//pr_notice("%s, is enable vbus temp:%d,val:%d\n", __func__);

	return val ? 1 : 0;
}

static struct regulator_ops upm6910_vbus_ops = {
	.enable = upm6910_enable_vbus,
	.disable = upm6910_disable_vbus,
	.is_enabled = upm6910_is_enabled_vbus,
};

static const struct regulator_desc upm6910_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &upm6910_vbus_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int upm6910_vbus_regulator_register(struct charger_device *chg_dev)
{
	struct regulator_config config = {};
	struct upm6910 *upm = NULL;
	int ret = 0;

	//otg regulator
	upm = dev_get_drvdata(&chg_dev->dev);
	config.dev = upm->dev;
	config.driver_data = upm;
	upm->otg_rdev =
		devm_regulator_register(upm->dev, &upm6910_otg_rdesc, &config);
	upm->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;

	if (IS_ERR(upm->otg_rdev)) {
		ret = PTR_ERR(upm->otg_rdev);
		pr_info("%s: register otg regulator failed (%d)\n", __func__, ret);
	}

	return ret;
}
/* S96818AA3 gujiayin.wt,modify,2024/08/22 otg bring up end */

extern void upm6720_shipmode_enable_adc(bool enable);
extern int sp2130_shipmode_enable_adc(bool enable);
/* +S96818AA1-2200, churui1.wt, ADD, 20230523, support shipmode function */
static int upm6910_set_shipmode(struct charger_device *chg_dev, bool en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en) {
		upm6720_shipmode_enable_adc(false);
		sp2130_shipmode_enable_adc(false);
		ret = upm6910_disable_batfet(upm);
		if (ret < 0) {
			pr_err("set shipmode failed(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int upm6910_set_shipmode_delay(struct charger_device *chg_dev, bool en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	//if set R07 bit5 as 1, then set bit3 as 0
	ret = upm6910_set_batfet_delay(upm, en);
	if (ret < 0) {
		pr_err("set shipmode delay failed(%d)\n", ret);
		return ret;
	}

	return 0;
}
/* -S96818AA1-2200, churui1.wt, ADD, 20230523, support shipmode function */

static struct charger_ops upm6910_chg_ops = {
	/* Normal charging */
	.plug_in = upm6910_plug_in,
	.plug_out = upm6910_plug_out,
	.dump_registers = upm6910_dump_register,
	.enable = upm6910_charging,
	.is_enabled = upm6910_is_charging_enable,
	.get_charging_current = upm6910_get_ichg,
	.set_charging_current = upm6910_set_ichg,
	.get_input_current = upm6910_get_icl,
	.set_input_current = upm6910_set_icl,
	.get_constant_voltage = upm6910_get_vchg,
	.set_constant_voltage = upm6910_set_vchg,
	.kick_wdt = upm6910_kick_wdt,
	.set_mivr = upm6910_set_ivl,
	.get_mivr = upm6910_get_mivr,
	.is_charging_done = upm6910_is_charging_done,
	.get_min_charging_current = upm6910_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = upm6910_set_safety_timer,
	.is_safety_timer_enabled = upm6910_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = upm6910_set_otg,
	.set_boost_current_limit = upm6910_set_boost_ilmt,
	.enable_discharge = NULL,

	.enable_chg_type_det = upm6910_enable_chg_type_det,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus start */
	.get_vbus_adc = upm6910_get_vbus,
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus end */
	/* Event */
	.event = upm6910_do_event,

	/* ship mode */
	.set_shipmode = upm6910_set_shipmode,
	.set_shipmode_delay = upm6910_set_shipmode_delay,

    /* HIZ */
	.enable_hz = upm6910_set_hiz_mode, // churui1.wt, ADD, 20230519, set the hiz mode

	/* ADC */
	.get_tchg_adc = NULL,

	/* term */
	.set_enable_term = upm6910_enable_term,

	/* rechg */
	.set_recharge = upm6910_set_recharge,
};


static int  upm6910_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct upm6910 *upm= power_supply_get_drvdata(psy);
	int ret = 0;
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus start */
	u8 pg_stat = 0;
	ret = upm6910_read_byte(upm, UPM6910_REG_08, &pg_stat);
	if (!ret)
		pg_stat = !!(pg_stat & REG08_PG_STAT_MASK);
	else
		pg_stat = upm->power_good;
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus end */
	switch(psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus start */
		val->intval = pg_stat;
		pr_err("%s: charge_online=%d\n", __func__, pg_stat);
/* S96818AA3 gujiayin.wt,modify,2024/08/21 get vbus end */
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if ((pg_stat == 1) && (upm->psy_desc.type == POWER_SUPPLY_TYPE_UNKNOWN)) {
			upm6910_get_charger_type(upm);
		}
		val->intval = upm->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = upm->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = upm6910_get_input_current_limit(upm);
		val->intval = (ret < 0) ? 0 : (ret * 1000);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = upm6910_get_charging_status(upm);
		pr_err("original val=%d\n", ret);
		if(ret >= 0) {
			switch (ret) {
				case CHRG_STAT_NOT_CHARGING:
					if (!pg_stat)
						val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
					else
						val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
					break;
				case CHRG_STAT_PRE_CHARGINT:
				case CHRG_STAT_FAST_CHARGING:
					val->intval = POWER_SUPPLY_STATUS_CHARGING;
					break;
				case CHRG_STAT_CHARGING_TERM:
					if (!pg_stat)
						val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
					else
						val->intval = POWER_SUPPLY_STATUS_FULL;
					break;
				default:
					val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
					break;
			}
			ret = 0;
		} else {
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		}
		if(upm->chg_old_status != val->intval) {
			if (upm->psy) {
				power_supply_changed(upm->psy);
				pr_debug("%s:status=%d,%d\n", __func__, upm->chg_old_status, val->intval);
			} else {
				pr_err("%s: cx->chg_psy error!!!\n", __func__);
			}
		}
		upm->chg_old_status = val->intval;
		break;
	default:
		ret = -ENODATA;
		break;
	}

	if (ret < 0){
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}
	return 0;
}


static int  upm6910_charger_set_property(struct power_supply *psy,
					  enum power_supply_property psp,
					  const union power_supply_propval *val)
{
   struct upm6910 *upm = power_supply_get_drvdata(psy);
   int ret = 0;

   switch(psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = upm6910_set_input_current_limit(upm, val->intval / 1000);
		break;
	//case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_AUTHENTIC:
		ret = upm6910_charging(upm->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = upm6910_enable_term(upm->chg_dev, !!val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (UPM6910_NOT_RETRY_BC12 == val->intval) {
			upm->is_need_retry_bc12 = false;
		} else {
			upm->is_need_bc12 = !!val->intval;
		}
		pr_err("is_need_bc12 =%d, is_need_retry_bc12 =%d\n", upm->is_need_bc12, upm->is_need_retry_bc12);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		upm6910_set_otg(upm->chg_dev, false);
		break;
	default:
		ret = -ENODATA;
		break;
	}

   return ret;
}

static const struct power_supply_desc upm6910_charger_desc = {
  .name 		  = "primary_chg",
  .type 		  = POWER_SUPPLY_TYPE_USB,
  .properties	  =  upm6910_charger_properties,
  .num_properties = ARRAY_SIZE(upm6910_charger_properties),
  .get_property   =  upm6910_charger_get_property,
  .set_property   =  upm6910_charger_set_property,
  .property_is_writeable  =  upm6910_charger_property_is_writeable,
  .usb_types	  =  upm6910_charger_usb_types,
  .num_usb_types  = ARRAY_SIZE( upm6910_charger_usb_types),
};


static struct of_device_id upm6910_charger_match_table[] = {
	{
	 .compatible = "upm,upm6910d",
	 .data = &pn_data[PN_UPM6910D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, upm6910_charger_match_table);

static void upm6910_charger_remove(struct i2c_client *client);

static void get_power_off_charging(struct upm6910 *upm)
{
	struct tag_bootmode *tag = NULL;

	if (IS_ERR_OR_NULL(upm->boot_node))
		pr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(upm->boot_node,
							"atag,boot", NULL);
		if (IS_ERR_OR_NULL(tag))
			pr_err("%s: failed to get atag,boot\n", __func__);
		else {
			pr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			upm->bootmode = tag->bootmode;
			upm->boottype = tag->boottype;
			pr_err("%s:bootmode = %d,boottype =%d\n", __func__,upm->bootmode,upm->boottype);

		}
	}

}

static int upm6910_charger_probe(struct i2c_client *client)
{
	struct upm6910 *upm;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config charger_cfg = {};
	const char *compatible;

	int ret = 0;

	upm = devm_kzalloc(&client->dev, sizeof(struct upm6910), GFP_KERNEL);
	if (!upm)
		return -ENOMEM;

	client->addr = 0x6B;
	upm->dev = &client->dev;
	upm->client = client;
#ifdef WT_COMPILE_FACTORY_VERSION
	upm->probe_flag=0;
#endif
	i2c_set_clientdata(client, upm);

	mutex_init(&upm->i2c_rw_lock);
	pr_err("%s:enter!\n", __func__);
	ret = upm6910_detect_device(upm);
	if (ret) {
		pr_err("No upm6910 device found!\n");
		client->addr = 0x1A;
		upm->dev = &client->dev;
		upm->client = client;
		i2c_set_clientdata(client, upm);
		mutex_init(&upm->i2c_rw_lock);
		ret = upm6910_detect_device(upm);
		if (ret) {
			pr_err("No sgm41513 device found!\n");
			return -ENODEV;
		}
/*+S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
	}else{
		is_upm6910 = true;
		printk("charger ic is upm6910\n");
	}
/*-S96818AA1-9230 lijiawei,wt.modify upm6910 safetytimer function logic*/
	match = of_match_node(upm6910_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}
	pr_err("part_no=0x%X,match->data=0x%X,SGM41513D=0x%X,UPM6910D=0x%X\n",upm->part_no,*(int *)match->data,SGM41513D,UPM6910D);
	if ((upm->part_no != *(int *)match->data) &&(upm->part_no != SGM41513D)) {
		//pr_info("part no mismatch, hw:%s, devicetree:%s\n",
		//	pn_str[upm->part_no], pn_str[*(int *) match->data]);
		pr_err("remove driver upm69xx\n");
	        return -ENODEV;
	}

	upm->platform_data = upm6910_parse_dt(node, upm);
	if (!upm->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}
	upm->hiz_state = SGM_NO_HIZ;
	upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	upm->bc12_retry_done = false;
	upm->is_need_bc12 = true;
	upm->is_need_retry_bc12 = true;
	INIT_DELAYED_WORK(&upm->psy_dwork, upm6910_inform_psy_dwork_handler);
	INIT_DELAYED_WORK(&upm->prob_dwork, upm6910_inform_prob_dwork_handler);
	INIT_DELAYED_WORK(&upm->wt_float_recheck_work, wtchg_float_charge_type_check);

	ret = upm6910_init_device(upm);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	upm6910_register_interrupt(upm);

	upm->chg_dev = charger_device_register(upm->chg_dev_name,
					      &client->dev, upm,
					      &upm6910_chg_ops,
					      &upm6910_chg_props);
	if (IS_ERR_OR_NULL(upm->chg_dev)) {
		ret = PTR_ERR(upm->chg_dev);
		return ret;
	}

	/* otg regulator */
	s_chg_dev_otg = upm->chg_dev;
	/* power supply register */
	memcpy(&upm->psy_desc, &upm6910_charger_desc, sizeof(upm->psy_desc));
	charger_cfg.drv_data = upm;
	charger_cfg.of_node = client->dev.of_node;
	charger_cfg.supplied_to = upm6910_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(upm6910_charger_supplied_to);
	upm->psy = devm_power_supply_register(&client->dev,
					&upm->psy_desc, &charger_cfg);
	if (IS_ERR(upm->psy)) {
		pr_err("Fail to register power supply dev\n");
	}

	if (!upm->usb_psy) {
		upm->usb_psy = power_supply_get_by_name("usb");
		if (!upm->usb_psy) {
			pr_err("%s Couldn't get usb psy\n", __func__);
		}
	}

	ret = sysfs_create_group(&upm->dev->kobj, &upm6910_attr_group);
	if (ret)
		dev_err(upm->dev, "failed to register sysfs. err: %d\n", ret);

	if (upm->part_no == SGM41513D)
		mod_delayed_work(system_wq, &upm->prob_dwork,
					msecs_to_jiffies(700));
	else
		mod_delayed_work(system_wq, &upm->prob_dwork,
					msecs_to_jiffies(2*1000));

	upm->otg_flag = 0;

	is_upm6910_chg_probe_ok = true;
	ret = upm6910_vbus_regulator_register(upm->chg_dev);
	if (ret)
		dev_err(upm->dev, "failed to register upm6910_vbus_regulator: %d\n", ret);

	pr_err("upm6910 probe successfully, Part Num:%d, Revision:%d\n!",
	       upm->part_no, upm->revision);
	upm->usb_node =  of_parse_phandle(node,"usb",0);
	upm->is_usb_compatible_ok = false;
	pr_err("usb node address: %p\n",upm->usb_node);

	if (!IS_ERR_OR_NULL(upm->usb_node)) {
		if (of_property_read_string(upm->usb_node, "compatible", &compatible) == 0) {
			if (strcmp(compatible, "mediatek,mt6768-usb20") == 0) {
				pr_err("node compatible ok\n");
				upm->is_usb_compatible_ok = true;
			} else {
				pr_err("node compatible fail\n");
				of_node_put(upm->usb_node);
				//upm->is_usb_compatible_ok = false;
			}
		} else {
			pr_err("fail to parse node compatible \n");
			of_node_put(upm->usb_node);
			//upm->is_usb_compatible_ok = false;
		}
	} else {
		pr_err("fail to parse usb node \n");
	}
	upm->usb_pdev = NULL;

	upm->boot_node = of_parse_phandle(node, "bootmode", 0);
	get_power_off_charging(upm);
	if (upm->part_no == SGM41513D)
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SGM41513D");
	else
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "UPM6910D");
	return 0;
}

static void upm6910_charger_remove(struct i2c_client *client)
{
	struct upm6910 *upm = i2c_get_clientdata(client);

	mutex_destroy(&upm->i2c_rw_lock);

	sysfs_remove_group(&upm->dev->kobj, &upm6910_attr_group);
}

/*+S96818AA1-9473 lijiawei,wt.PVT BA1 GN4 machine can not enter shipmode*/
extern bool factory_shipmode;
static void upm6910_charger_shutdown(struct i2c_client *client)
{
	struct upm6910 *upm = i2c_get_clientdata(client);

	printk("upm6910_charger_shutdown\n");
#ifdef WT_COMPILE_FACTORY_VERSION
	if (factory_shipmode == true) {
		upm6910_set_shipmode(upm->chg_dev,true);
		upm6910_set_shipmode_delay(upm->chg_dev,false);
	}
#else
	if (factory_shipmode == true) {
		upm6910_set_hiz_mode(upm->chg_dev,1);
		upm6910_set_shipmode(upm->chg_dev,true);
		upm6910_set_shipmode_delay(upm->chg_dev,false);
	}
#endif
	upm6910_reset_chip(upm);
}
/*-S96818AA1-9473 lijiawei,wt.PVT BA1 GN4 machine can not enter shipmode*/
static struct i2c_driver upm6910_charger_driver = {
	.driver = {
		   .name = "upm6910-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = upm6910_charger_match_table,
		   },

	.probe = upm6910_charger_probe,
	.remove = upm6910_charger_remove,
	.shutdown = upm6910_charger_shutdown,
};

module_i2c_driver(upm6910_charger_driver);

MODULE_DESCRIPTION("Unisemipower UPM6910D Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Unisemepower");
