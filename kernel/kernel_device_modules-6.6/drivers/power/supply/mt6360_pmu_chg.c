// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/iio/consumer.h>
#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/linear_range.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/pm.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/regulator/driver.h>
#include <linux/slab.h>
#include <linux/wait.h>

#if IS_ENABLED(CONFIG_MTK_CHARGER)
#include "charger_class.h"
#include "mtk_charger.h"
#endif
#include <tcpm.h>

#include <linux/mfd/mt6360-private.h>

static bool dbg_log_en = true;
module_param(dbg_log_en, bool, 0644);
#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_info(dev, "%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

/* Define this macro if DCD timeout is supported */
#define CONFIG_MT6360_DCDTOUT_SUPPORT

/* Define this macro if detecting apple samsung TA is needed */
#define MT6360_APPLE_SAMSUNG_TA_SUPPORT

#define MT6360_PMU_DEV_INFO		0x300
#define MT6360_PMU_CORE_CTRL2		0x306
#define MT6360_PMU_TM_PAS_CODE1		0x307
#define MT6360_PMU_CHG_CTRL1		0x311
#define MT6360_PMU_CHG_CTRL2		0x312
#define MT6360_PMU_CHG_CTRL3		0x313
#define MT6360_PMU_CHG_CTRL4		0x314
#define MT6360_PMU_CHG_CTRL5		0x315
#define MT6360_PMU_CHG_CTRL6		0x316
#define MT6360_PMU_CHG_CTRL7		0x317
#define MT6360_PMU_CHG_CTRL9		0x319
#define MT6360_PMU_CHG_CTRL10		0x31A
#define MT6360_PMU_CHG_CTRL11		0x31B
#define MT6360_PMU_CHG_CTRL12		0x31C
#define MT6360_PMU_CHG_CTRL13		0x31D
#define MT6360_PMU_CHG_CTRL14		0x31E
#define MT6360_PMU_CHG_CTRL16		0x320
#define MT6360_PMU_CHG_AICC_RESULT	0x321
#define MT6360_PMU_DEVICE_TYPE		0x322
#define MT6360_PMU_USB_STATUS1		0x327
#define MT6360_PMU_DPDM_CTRL2       0x329
#define MT6360_PMU_CHG_CTRL17		0x32B
#define MT6360_PMU_CHG_CTRL18		0x32C
#define MT6360_PMU_CHRDET_CTRL1		0x32D
#define MT6360_PMU_CHG_HIDDEN_CTRL1	0x330
#define MT6360_PMU_CHG_HIDDEN_CTRL2	0x331
#define MT6360_PMU_CHG_STAT		0x34A
#define MT6360_PMU_TYPEC_OTP_CTRL	0x351
#define MT6360_PMU_ADC_BAT_DATA_H	0x352
#define MT6360_PMU_ADC_CONFIG		0x356
#define MT6360_PMU_CHG_CTRL19		0x361
#define MT6360_PMU_FOD_CTRL		0x365
#define MT6360_PMU_CHG_CTRL20		0x366
#define MT6360_PMU_USBID_CTRL1		0x36D
#define MT6360_PMU_USBID_CTRL2		0x36E
#define MT6360_PMU_USBID_CTRL3		0x36F
#define MT6360_PMU_FLED_EN		0x37E
#define MT6360_PMU_CHG_STAT1		0x3E0
#define MT6360_PMU_CHG_STAT2		0x3E1
#define MT6360_PMU_CHG_STAT4		0x3E3
#define MT6360_PMU_CHG_STAT5		0x3E4
#define MT6360_PMU_FOD_STAT		0x3E7

/* MT6360_PMU_DEV_INFO : 0x300 */
#define MT6360_CHIP_REV_MASK		GENMASK(3, 0)
/* MT6360_PMU_CORE_CTRL2 : 0x306 */
#define MT6360_MASK_SHIP_RST_DIS	BIT(0)
/* MT6360_PMU_CHG_CTRL1 : 0x311 */
#define MT6360_FSLP_MASK		BIT(3)
#define MT6360_HZ_EN_MASK		BIT(2)
#define MT6360_OPA_MODE_MASK		BIT(0)
/* MT6360_PMU_CHG_CTRL2 : 0x312 */
#define MT6360_BATDET_DIS_DLY_MASK		BIT(6)
#define MT6360_IINLMTSEL_SHFT		(2)
#define MT6360_IINLMTSEL_MASK		GENMASK(3, 2)
//gujiayin.wt,add shipmode ctrl
#define MT6360_SHIP_MODE_MASK		BIT(7)
#define MT6360_SHIP_DELAY_MASK		BIT(6)
#define MT6360_TE_EN_MASK		BIT(4)
#define MT6360_CFO_EN_MASK		BIT(1)
#define MT6360_CHG_EN_MASK		BIT(0)
/* MT6360_PMU_CHG_CTRL3 : 0x313 */
#define MT6360_AICR_SHFT		(2)
#define MT6360_AICR_MASK		GENMASK(7, 2)
#define MT6360_ILIM_EN_MASK		BIT(0)
/* MT6360_PMU_CHG_CTRL4 : 0x314 */
#define MT6360_VOREG_SHFT		(1)
#define MT6360_VOREG_MASK		GENMASK(7, 1)
/* MT6360_PMU_CHG_CTRL5 : 0x315 */
#define MT6360_MASK_BOOST_VOREG		GENMASK(7, 2)
/* MT6360_PMU_CHG_CTRL6 : 0x316 */
#define MT6360_MIVR_SHFT		(1)
#define MT6360_MIVR_MASK		GENMASK(7, 1)
/* MT6360_PMU_CHG_CTRL7 : 0x317 */
#define MT6360_ICHG_SHFT		(2)
#define MT6360_ICHG_MASK		GENMASK(7, 2)
#define MT6360_EOC_TIMER_MASK	GENMASK(1, 0)

/* MT6360_PMU_CHG_CTRL9 : 0x319 */
#define MT6360_IEOC_SHFT		(4)
#define MT6360_IEOC_MASK		GENMASK(7, 4)
#define MT6360_EOC_EN_MASK		BIT(3)
/* MT6360_PMU_CHG_AICC_RESULT : 0x321 */
#define MT6360_AICC_RESULT_SHFT		(2)
#define MT6360_AICC_RESULT_MASK		GENMASK(7, 2)
/* MT6360_PMU_CHG_CTRL10 : 0x31A */
#define MT6360_OTG_OC_SHFT		(0)
#define MT6360_OTG_OC_MASK		GENMASK(2, 0)
/* MT6360_PMU_CHG_CTRL12 : 0x31C */
#define MT6360_TMR_EN_MASK		BIT(1)
/* MT6360_PMU_CHG_CTRL13 : 0x31D */
#define MT6360_CHG_WDT_EN_MASK		BIT(7)
/* MT6360_PMU_CHG_CTRL14 : 0x31E */
#define MT6360_RG_EN_AICC_MASK		BIT(7)
/* MT6360_PMU_DEVICE_TYPE : 0x322 */
#define MT6360_USBCHGEN_MASK		BIT(7)
#define MT6360_MASK_DCDTOUTEN		BIT(6)
/* MT6360_PMU_USB_STATUS1 : 0x327 */
#define MT6360_USB_STATUS_SHFT		(4)
#define MT6360_USB_STATUS_MASK		GENMASK(6, 4)
/* MT6360_PMU_CHG_CTRL16 : 0x32A */
#define MT6360_AICC_VTH_SHFT		(1)
#define MT6360_AICC_VTH_MASK		GENMASK(7, 1)
/* MT6360_PMU_CHRDET_CTRL1 : 0x32D */
#define MT6360_CHRDETB_VIN_OVP_VTHSEL_SHFT	(0)
#define MT6360_CHRDETB_VIN_OVP_VTHSEL_MASK	GENMASK(3, 0)
/* MT6360_PMU_CHG_CTRL17 : 0x32B */
#define MT6360_EN_PUMPX_MASK		BIT(7)
#define MT6360_PUMPX_20_10_MASK		BIT(6)
#define MT6360_PUMPX_UP_DN_MASK		BIT(5)
#define MT6360_PUMPX_DEC_SHFT		(0)
#define MT6360_PUMPX_DEC_MASK		GENMASK(4, 0)
/* MT6360_PMU_CHG_HIDDEN_CTRL2 : 0x331 */
#define MT6360_EOC_RST_MASK		BIT(7)
#define MT6360_DISCHG_MASK		BIT(2)
/* MT6360_PMU_CHG_STAT : 0x34A */
#define MT6360_CHG_STAT_SHFT		(6)
#define MT6360_CHG_STAT_MASK		GENMASK(7, 6)
#define MT6360_VBAT_LVL_MASK		BIT(5)
#define MT6360_VBAT_TRICKLE_MASK	BIT(4)
#define MT6360_CHG_BATSYSUV_MASK	BIT(1)
/* MT6360_PMU_TYPEC_OTP_CTRL: 0x351 */
#define MT6360_TYPEC_OTP_SWEN_MASK	BIT(2)
#define MT6360_TYPEC_OTP_EN_MASK	BIT(0)
/* MT6360_PMU_ADC_CONFIG : 0x356 */
#define MT6360_ZCV_EN_SHFT		(6)
#define MT6360_ZCV_EN_MASK		BIT(6)
/* MT6360_PMU_CHG_CTRL19 : 0x361 */
#define MT6360_CHG_VIN_OVP_VTHSEL_SHFT	(5)
#define MT6360_CHG_VIN_OVP_VTHSEL_MASK	GENMASK(6, 5)
/* MT6360_PMU_CHG_CTRL20 : 0x366 */
#define MT6360_EN_SDI_MASK		BIT(1)
/* MT6360_PMU_USBID_CTRL1 : 0x36D */
#define MT6360_USBID_EN_MASK		BIT(7)
#define MT6360_ID_RPULSEL_SHFT		5
#define MT6360_ID_RPULSEL_MASK		GENMASK(6, 5)
#define MT6360_ISTDET_SHFT		2
#define MT6360_ISTDET_MASK		GENMASK(4, 2)
/* MT6360_PMU_USBID_CTRL2 : 0x36E */
#define MT6360_IDTD_MASK		GENMASK(7, 5)
#define MT6360_USBID_FLOAT_MASK		BIT(1)
/* MT6360_PMU_FLED_EN : 0x37E */
#define MT6360_STROBE_EN_MASK		BIT(2)
/* MT6360_PMU_CHG_STAT1 : 0x3E0 */
#define MT6360_PWR_RDY_EVT_MASK		BIT(7)
#define MT6360_MIVR_EVT_SHFT		(6)
#define MT6360_MIVR_EVT_MASK		BIT(6)
#define MT6360_CHG_TREG_SHFT		(4)
#define MT6360_CHG_TREG_MASK		BIT(4)

/* MT6360_PMU_CHG_STAT2 : 0x3E1 */
#define MT6360_CHG_VSYSOV_MASK		BIT(5)
/* MT6360_PMU_CHG_STAT4 : 0x3E3 */
#define MT6360_OTPI_MASK		BIT(7)
#define MT6360_CHG_TMRI_MASK		BIT(3)
/* MT6360_PMU_CHG_STAT5 : 0x3E4 */
#define MT6360_WDTMRI_MASK		BIT(3)
/* MT6360_PMU_CHRDET_STAT : 0x3E7 */
#define MT6360_CHRDETB_OVP_MASK		BIT(3)
#define MT6360_CHRDETB_UVPB_MASK	BIT(2)

/* uV */
#define MT6360_VMIVR_MIN	3900000
#define MT6360_VMIVR_MAX	13400000
#define MT6360_VMIVR_STEP	100000
/* uA */
#define MT6360_ICHG_MIN		100000
#define MT6360_ICHG_MAX		5000000
#define MT6360_ICHG_STEP	100000
/* uV */
#define MT6360_VOREG_MIN	3900000
#define MT6360_VOREG_MAX	4710000
#define MT6360_VOREG_STEP	10000
/* uA */
#define MT6360_AICR_MIN		100000
#define MT6360_AICR_MAX		3250000
#define MT6360_AICR_STEP	50000
/* uA */
#define MT6360_IPREC_MIN	100000
#define MT6360_IPREC_MAX	850000
#define MT6360_IPREC_STEP	50000
/* uA */
#define MT6360_IEOC_MIN		100000
#define MT6360_IEOC_MAX		850000
#define MT6360_IEOC_STEP	50000

/* If use EXT_HEALTH property, set to 1 */
#define POWER_SUPPLY_EXT_HEALTH	0

enum {
	MT6360_RANGE_VMIVR,
	MT6360_RANGE_ICHG,
	MT6360_RANGE_VOREG,
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	MT6360_RANGE_VRECHG,
#endif
	MT6360_RANGE_AICR,
	MT6360_RANGE_IPREC,
	MT6360_RANGE_IEOC,
	MT6360_RANGE_IRCMP_R,
	MT6360_RANGE_VCLAMP,
	MT6360_RANGE_PUMPX20,
	MT6360_RANGE_AICC_VTH,
	MT6360_RANGE_MAX,
};

enum {
	PORT_STAT_NOINFO,
	APPLE_0_5A_CHARGER,
	APPLE_1_0A_CHARGER,
	APPLE_2_1A_CHARGER,
	APPLE_2_4A_CHARGER,
	SAMSUNG_CHARGER,
};

#define MT6360_LINEAR_RANGE(idx, _min, _min_sel, _max_sel, _step) \
	[idx] = REGULATOR_LINEAR_RANGE(_min, _min_sel, _max_sel, _step)

static const struct linear_range mt6360_chg_range[MT6360_RANGE_MAX] = {
	MT6360_LINEAR_RANGE(MT6360_RANGE_VMIVR, 3900000, 0, 0x5F, 100000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_ICHG, 100000, 0, 0x1D, 100000), // ALPS07902899 limit ichg as 3A
	MT6360_LINEAR_RANGE(MT6360_RANGE_VOREG, 3900000, 0, 0x51, 10000),
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	MT6360_LINEAR_RANGE(MT6360_RANGE_VRECHG, 100000, 0, 0x03, 50000),
#endif
	MT6360_LINEAR_RANGE(MT6360_RANGE_AICR, 100000, 0, 0x3F, 50000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_IPREC, 100000, 0, 0x0F, 50000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_IEOC, 100000, 0, 0x0F, 50000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_IRCMP_R, 0, 0, 0x07, 25000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_VCLAMP, 0, 0, 0x07, 32000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_PUMPX20, 5500000, 0, 0x1D, 500000),
	MT6360_LINEAR_RANGE(MT6360_RANGE_AICC_VTH, 3900000, 0, 0x51, 10000),
};

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

void __attribute__ ((weak)) Charger_Detect_Init(void)
{
}

void __attribute__ ((weak)) Charger_Detect_Release(void)
{
}

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

enum mt6360_adc_channel {
	MT6360_ADC_VBUSDIV5,
	MT6360_ADC_VSYS,
	MT6360_ADC_VBAT,
	MT6360_ADC_IBUS,
	MT6360_ADC_IBAT,
	MT6360_ADC_NODUMP,
	MT6360_ADC_TEMP_JC = MT6360_ADC_NODUMP,
	MT6360_ADC_USBID,
	MT6360_ADC_TS,
	MT6360_ADC_MAX,
};

struct mt6360_chg_platform_data {
	u32 ichg;
	u32 aicr;
	u32 mivr;
	u32 cv;
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	u32 vrechg;
#endif
	u32 ieoc;
	u32 safety_timer;
	u32 ircmp_resistor;
	u32 ircmp_vclamp;
	u32 en_te;
	u32 en_wdt;
	u32 post_aicc;
	u32 batoc_notify;
	u32 bc12_sel;
	const char *chg_name;
};

struct mt6360_chg_info {
	struct device *dev;
	struct regmap *regmap;
	struct iio_channel *channels[MT6360_ADC_MAX];
	struct power_supply *psy;
	struct charger_device *chg_dev;
	int hidden_mode_cnt;
	struct mutex hidden_mode_lock;
	struct mutex pe_lock;
	struct mutex aicr_lock;
	struct mutex tchg_lock;
	struct mutex ichg_lock;
	int tchg;
	u32 zcv;
	u32 ichg;
	u32 ichg_dis_chg;
	u8 chip_rev;

	/* boot_mode */
	u32 bootmode;
	u32 boottype;

	/* Charger type detection */
	struct mutex chgdet_lock;
	bool attach;
	bool pwr_rdy;
	bool bc12_en;
	int psy_usb_type;
	atomic_t tcpc_attach;
	bool is_need_bc12;

	/* dts: charger */
	struct power_supply *chg_psy;

	/* type_c_port0 */
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;

	struct work_struct chgdet_work;
	/* power supply */
	struct power_supply_desc psy_desc;

	struct completion aicc_done;
	struct completion pumpx_done;
	atomic_t pe_complete;
	/* mivr */
	atomic_t mivr_cnt;
	wait_queue_head_t mivr_wq;
	struct task_struct *mivr_task;
	/* unfinish pe pattern */
	struct workqueue_struct *pe_wq;
	struct work_struct pe_work;
	u8 ctd_dischg_status;
	/* otg_vbus */
	struct regulator_dev *otg_rdev;

	/* block BC1.2 during hardreset */
	bool is_in_hardreset;
	u32 cable_type;
	struct delayed_work hardreset_work;
	struct delayed_work tcpc_notify_work;
};


/* for recive bat oc notify */
struct mt6360_chg_info *g_mci;

static const u32 mt6360_otg_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

enum mt6360_iinlmtsel {
	MT6360_IINLMTSEL_AICR_3250 = 0,
	MT6360_IINLMTSEL_CHG_TYPE,
	MT6360_IINLMTSEL_AICR,
	MT6360_IINLMTSEL_LOWER_LEVEL,
};

enum mt6360_charging_status {
	MT6360_CHG_STATUS_READY = 0,
	MT6360_CHG_STATUS_PROGRESS,
	MT6360_CHG_STATUS_DONE,
	MT6360_CHG_STATUS_FAULT,
	MT6360_CHG_STATUS_MAX,
};

enum mt6360_usbsw_state {
	MT6360_USBSW_CHG = 0,
	MT6360_USBSW_USB,
};

enum mt6360_pmu_chg_type {
	MT6360_CHG_TYPE_NOVBUS = 0,
	MT6360_CHG_TYPE_UNDER_GOING,
	MT6360_CHG_TYPE_SDP,
	MT6360_CHG_TYPE_SDPNSTD,
	MT6360_CHG_TYPE_DCP,
	MT6360_CHG_TYPE_CDP,
	MT6360_CHG_TYPE_MAX,
};

static enum power_supply_usb_type mt6360_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
};

static const char * const mt6360_adc_chan_list[] = {
	"VBUSDIV5", "VSYS", "VBAT", "IBUS", "IBAT", "TEMP_JC", "USBID", "TS",
};

static const char __maybe_unused *mt6360_chg_status_name[] = {
	"ready", "progress", "done", "fault",
};

static const struct mt6360_chg_platform_data def_platform_data = {
	.ichg = 2000000,		/* uA */
	.aicr = 500000,			/* uA */
	.mivr = 4400000,		/* uV */
	.cv = 4350000,			/* uA */
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	.vrechg = 200000,		/* uV */
#endif
	.ieoc = 250000,			/* uA */
	.safety_timer = 12,		/* hour */
#ifdef CONFIG_MTK_BIF_SUPPORT
	.ircmp_resistor = 0,		/* uohm */
	.ircmp_vclamp = 0,		/* uV */
#else
	.ircmp_resistor = 25000,	/* uohm */
	.ircmp_vclamp = 32000,		/* uV */
#endif
	.en_te = true,
	.en_wdt = true,
	.post_aicc = true,
	.batoc_notify = false,
	.bc12_sel = 0,
	.chg_name = "primary_chg",
};

/* ================== */
/* Internal Functions */
/* ================== */
static u32 mt6360_trans_ichg_sel(u32 uA)
{
	u32 data;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_ICHG],
					 uA, &data);
	return data;
}

static u32 mt6360_trans_aicr_sel(u32 uA)
{
	u32 data;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_AICR],
					 uA, &data);
	return data;
}

static u32 mt6360_trans_mivr_sel(u32 uV)
{
	u32 data;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_VMIVR],
					 uV, &data);
	return data;
}

static u32 mt6360_trans_cv_sel(u32 uV)
{
	u32 data;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_VOREG],
					 uV, &data);
	return data;
}

#if defined(CONFIG_W2_CHARGER_PRIVATE)
static u32 mt6360_trans_vrechg_sel(u32 uA)
{
	u32 data;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_VRECHG],
					 uA, &data);
	return data;
}
#endif

static u32 mt6360_trans_ieoc_sel(u32 uA)
{
	u32 data;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_IEOC],
					 uA, &data);
	return data;
}

static u32 mt6360_trans_safety_timer_sel(u32 hr)
{
	u32 mt6360_st_tbl[] = {4, 6, 8, 10, 12, 14, 16, 20};
	u32 cur_val, next_val;
	int i;

	if (hr < mt6360_st_tbl[0])
		return 0;
	for (i = 0; i < ARRAY_SIZE(mt6360_st_tbl) - 1; i++) {
		cur_val = mt6360_st_tbl[i];
		next_val = mt6360_st_tbl[i+1];
		if (hr >= cur_val && hr < next_val)
			return i;
	}
	return ARRAY_SIZE(mt6360_st_tbl) - 1;
}

static u32 mt6360_trans_ircmp_r_sel(u32 uohm)
{
	u32 data;

	linear_range_get_selector_within(
		&mt6360_chg_range[MT6360_RANGE_IRCMP_R], uohm, &data);
	return data;
}

static u32 mt6360_trans_ircmp_vclamp_sel(u32 uV)
{
	u32 data;

	linear_range_get_selector_within(
		&mt6360_chg_range[MT6360_RANGE_VCLAMP], uV, &data);
	return data;
}

static const u32 mt6360_usbid_rup[] = {
	500000, 75000, 5000, 1000
};

static inline u32 mt6360_trans_usbid_rup(u32 rup)
{
	int i;
	int maxidx = ARRAY_SIZE(mt6360_usbid_rup) - 1;

	if (rup >= mt6360_usbid_rup[0])
		return 0;
	if (rup <= mt6360_usbid_rup[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (rup == mt6360_usbid_rup[i])
			return i;
		if (rup < mt6360_usbid_rup[i] &&
		    rup > mt6360_usbid_rup[i + 1]) {
			if ((mt6360_usbid_rup[i] - rup) <=
			    (rup - mt6360_usbid_rup[i + 1]))
				return i;
			else
				return i + 1;
		}
	}
	return maxidx;
}

static const u32 mt6360_usbid_src_ton[] = {
	400, 1000, 4000, 10000, 40000, 100000, 400000,
};

static inline u32 mt6360_trans_usbid_src_ton(u32 src_ton)
{
	int i;
	int maxidx = ARRAY_SIZE(mt6360_usbid_src_ton) - 1;

	/* There is actually an option, always on, after 400000 */
	if (src_ton == 0)
		return maxidx + 1;
	if (src_ton < mt6360_usbid_src_ton[0])
		return 0;
	if (src_ton > mt6360_usbid_src_ton[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (src_ton == mt6360_usbid_src_ton[i])
			return i;
		if (src_ton > mt6360_usbid_src_ton[i] &&
		    src_ton < mt6360_usbid_src_ton[i + 1]) {
			if ((src_ton - mt6360_usbid_src_ton[i]) <=
			    (mt6360_usbid_src_ton[i + 1] - src_ton))
				return i;
			else
				return i + 1;
		}
	}
	return maxidx;
}

static inline int mt6360_get_ieoc(struct mt6360_chg_info *mci, u32 *uA)
{
	int ret = 0;
	unsigned int regval, value;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL9, &regval);
	if (ret < 0)
		return ret;
	regval = (regval & MT6360_IEOC_MASK) >> MT6360_IEOC_SHFT;
	ret = linear_range_get_value(&mt6360_chg_range[MT6360_RANGE_IEOC],
				     regval, &value);
	if (!ret)
		*uA = value;
	return 0;
}

static inline int mt6360_get_charging_status(
					struct mt6360_chg_info *mci,
					enum mt6360_charging_status *chg_stat)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT, &regval);
	if (ret < 0)
		return ret;
	*chg_stat = (regval & MT6360_CHG_STAT_MASK) >> MT6360_CHG_STAT_SHFT;
	return 0;
}

static inline int mt6360_is_charger_enabled(struct mt6360_chg_info *mci,
					    bool *en)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL2, &regval);
	if (ret < 0)
		return ret;
	*en = (regval & MT6360_CHG_EN_MASK) ? true : false;
	return 0;
}

static inline int mt6360_select_input_current_limit(
		struct mt6360_chg_info *mci, enum mt6360_iinlmtsel sel)
{
	dev_dbg(mci->dev,
		"%s: select input current limit = %d\n", __func__, sel);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL2,
				  MT6360_IINLMTSEL_MASK,
				  sel << MT6360_IINLMTSEL_SHFT);
}

static int mt6360_enable_wdt(struct mt6360_chg_info *mci, bool en)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_dbg(mci->dev, "%s enable wdt, en = %d\n", __func__, en);
	if (!pdata->en_wdt)
		return 0;
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL13,
				  MT6360_CHG_WDT_EN_MASK,
				  en ? 0xff : 0);
}

static inline int mt6360_get_chrdet_ext_stat(struct mt6360_chg_info *mci,
					  bool *pwr_rdy)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_FOD_STAT, &regval);
	if (ret < 0)
		return ret;
	*pwr_rdy = (regval & BIT(4)) ? true : false;
	return 0;
}

static int DPDM_Switch_TO_CHG_upstream(struct mt6360_chg_info *mci,
						bool switch_to_chg)
{
	struct phy *phy;
	int mode = 0;
	int ret;

	mode = switch_to_chg ? PHY_MODE_BC11_SET : PHY_MODE_BC11_CLR;
	phy = phy_get(mci->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_info(mci->dev, "phy_get fail\n");
		return -EINVAL;
	}

	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_info(mci->dev, "phy_set_mode_ext fail\n");

	phy_put(mci->dev, phy);

	return 0;
}

static int mt6360_set_usbsw_state(struct mt6360_chg_info *mci, int state)
{
	dev_info(mci->dev, "%s: state = %d\n", __func__, state);

	/* Switch D+D- to AP/MT6360 */
	if (state == MT6360_USBSW_CHG)
		DPDM_Switch_TO_CHG_upstream(mci, true);
	else
		DPDM_Switch_TO_CHG_upstream(mci, false);

	return 0;
}

#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
static int __maybe_unused mt6360_enable_dcd_tout(
				      struct mt6360_chg_info *mci, bool en)
{
	dev_info(mci->dev, "%s en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
				  MT6360_MASK_DCDTOUTEN, en ? 0xff : 0);
}

static int __maybe_unused mt6360_is_dcd_tout_enable(
				     struct mt6360_chg_info *mci, bool *en)
{
	int ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_DEVICE_TYPE, &regval);
	if (ret < 0) {
		*en = false;
		return ret;
	}
	*en = (regval & MT6360_MASK_DCDTOUTEN ? true : false);
	return 0;
}
#endif

static bool is_usb_rdy(struct device *dev)
{
	struct device_node *node;
	bool ready = true;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		dev_info(dev, "usb ready=%d\n", ready);
	} else
		dev_info(dev, "usb node missing or invalid\n");

	return ready;
}

static int __mt6360_enable_usbchgen(struct mt6360_chg_info *mci, bool en)
{
	int i, ret = 0;
	const int max_wait_cnt = 200;
	bool pwr_rdy = false;
	enum mt6360_usbsw_state usbsw =
				       en && mci->is_need_bc12 ? MT6360_USBSW_CHG : MT6360_USBSW_USB;
#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
	bool dcd_en = false;
#endif /* CONFIG_MT6360_DCDTOUT_SUPPORT */

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);
	if (en) {
#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
		ret = mt6360_is_dcd_tout_enable(mci, &dcd_en);
		if (!dcd_en)
			msleep(180);
#endif /* CONFIG_MT6360_DCDTOUT_SUPPORT */

		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(mci->dev)) {
				dev_info(mci->dev, "%s: USB ready\n",
					__func__);
				msleep(10);
				break;
			}
			dev_info(mci->dev, "%s: CDP block\n", __func__);
			if (IS_ENABLED(CONFIG_TCPC_CLASS)) {
				if (!atomic_read(&mci->tcpc_attach)) {
					dev_info(mci->dev,
						 "%s: plug out\n", __func__);
					return 0;
				}
			} else {
				/* Check vbus */
				ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
				if (ret < 0) {
					dev_dbg(mci->dev,
						"%s: fail, ret = %d\n",
						 __func__, ret);
					return ret;
				}
				dev_info(mci->dev,
					 "%s: pwr_rdy = %d\n", __func__,
					 pwr_rdy);
				if (!pwr_rdy) {
					dev_info(mci->dev,
						 "%s: plug out\n", __func__);
					return ret;
				}
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			dev_dbg(mci->dev, "%s: CDP timeout\n", __func__);
		else
			dev_info(mci->dev, "%s: CDP free\n", __func__);
	}
	mt6360_set_usbsw_state(mci, usbsw);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
				 MT6360_USBCHGEN_MASK, en ? 0xff : 0);
	if (ret >= 0)
		mci->bc12_en = en;
	return ret;
}

static int mt6360_enable_usbchgen(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;

	mutex_lock(&mci->chgdet_lock);
	ret = __mt6360_enable_usbchgen(mci, en);
	mutex_unlock(&mci->chgdet_lock);
	return ret;
}

#ifdef MT6360_APPLE_SAMSUNG_TA_SUPPORT
static inline int mt6360_pmu_reg_test_bit(struct mt6360_chg_info *mci,
					  unsigned int addr, u8 shift, bool *is_one)
{
	int ret = 0;
	u8 data = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, addr, &regval);

	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = regval & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return 0;
}

static int mt6360_detect_apple_samsung_ta(struct mt6360_chg_info *mci, int *chg_type)
{
	int ret = 0;
	bool dp_0_9v = false, dp_1_5v = false, dp_2_3v = false, dm_2_3v = false;

	dev_info(mci->dev, "%s\n", __func__);

	mt6360_set_usbsw_state(mci, MT6360_USBSW_CHG);

	/* Check DP > 0.9V */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DPDM_CTRL2, 0x0F, 0x03);
	usleep_range(100, 200);

	ret = mt6360_pmu_reg_test_bit(mci, MT6360_PMU_DPDM_CTRL2, 4, &dp_0_9v);
	if (ret < 0)
		goto out;

	if (!dp_0_9v) {
		dev_info(mci->dev, "%s: DP < 0.9V\n", __func__);
		goto out;
	}

	ret = mt6360_pmu_reg_test_bit(mci, MT6360_PMU_DPDM_CTRL2, 5, &dp_1_5v);
	if (ret < 0)
		goto out;

	/* Samsung charger */
	if (!dp_1_5v) {
		dev_info(mci->dev, "%s[SS]: 0.9V < DP < 1.5V\n", __func__);
		*chg_type = SAMSUNG_CHARGER;
		goto out;
	}

	/* Check DP > 2.3 V */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DPDM_CTRL2, 0x0F, 0x0B);
	usleep_range(100, 200);

	ret = mt6360_pmu_reg_test_bit(mci, MT6360_PMU_DPDM_CTRL2, 5, &dp_2_3v);
	if (ret < 0)
		goto out;

	/* Check DM > 2.3V */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DPDM_CTRL2, 0x0F, 0x0F);
	usleep_range(100, 200);

	ret = mt6360_pmu_reg_test_bit(mci, MT6360_PMU_DPDM_CTRL2, 5, &dm_2_3v);
	if (ret < 0)
		goto out;

	/* Apple charger */
	if (!dp_2_3v && !dm_2_3v) {
		dev_info(mci->dev, "%s[AP0_5]: 1.5V < DP < 2.3V && DM < 2.3V\n",
			 __func__);
		*chg_type = APPLE_0_5A_CHARGER;
	} else if (!dp_2_3v && dm_2_3v) {
		dev_info(mci->dev, "%s[AP1_0]: 1.5V < DP < 2.3V && 2.3V < DM\n",
			 __func__);
		*chg_type = APPLE_1_0A_CHARGER;
	} else if (dp_2_3v && !dm_2_3v) {
		dev_info(mci->dev, "%s[AP2_1]: 2.3V < DP && DM < 2.3V\n", __func__);
		*chg_type = APPLE_2_1A_CHARGER;
	} else {
		dev_info(mci->dev, "%s[AP2_4]: 2.3V < DP && 2.3V < DM\n", __func__);
		*chg_type = APPLE_2_4A_CHARGER;
	}
out:
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DPDM_CTRL2, 0x0F, 0x00);
	usleep_range(100, 200);

	mt6360_set_usbsw_state(mci, MT6360_USBSW_USB);
	return ret;
}
#endif /* MT6360_APPLE_SAMSUNG_TA_SUPPORT */

static int mt6360_chgdet_pre_process(struct mt6360_chg_info *mci)
{
	bool attach = false;

	if (IS_ENABLED(CONFIG_TCPC_CLASS))
		attach = atomic_read(&mci->tcpc_attach);
	else
		attach = mci->pwr_rdy;
	if (attach && (mci->bootmode == 5)) {
		/* Skip charger type detection to speed up meta boot.*/
		dev_notice(mci->dev, "%s: force Standard USB Host in meta\n",
			   __func__);
		mci->attach = attach;
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		power_supply_changed(mci->psy);
		return 0;
	}
	return __mt6360_enable_usbchgen(mci, attach);
}

static int mt6360_toggle_usbchgen(struct mt6360_chg_info *mci)
{
	int ret = 0;

	dev_info(mci->dev, "%s\n", __func__);

	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
							MT6360_USBCHGEN_MASK, 0);
	if (ret < 0)
		dev_err(mci->dev, "%s: clr usbchgen fail\n", __func__);

	udelay(100);

	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
							MT6360_USBCHGEN_MASK, 0xff);
	if (ret < 0)
		dev_err(mci->dev, "%s: set usbchgen fail\n", __func__);

	return ret;
}

static void mt6360_in_hardreset_handler(struct work_struct *work)
{
	struct mt6360_chg_info *mci = container_of(work,
					struct mt6360_chg_info, hardreset_work.work);

	dev_info(mci->dev, "%s\n", __func__);
	mci->is_in_hardreset = false;
}

static void mt6360_set_is_in_hardreset(struct mt6360_chg_info *mci, bool in_hardreset)
{
	dev_info(mci->dev, "%s: set %u\n", __func__, in_hardreset);

	if(mci->is_in_hardreset) {
		cancel_delayed_work(&mci->hardreset_work);
		schedule_delayed_work(&mci->hardreset_work, msecs_to_jiffies(10000));
	} else
		cancel_delayed_work(&mci->hardreset_work);

	mci->is_in_hardreset = in_hardreset;
}

static int mt6360_chgdet_post_process(struct mt6360_chg_info *mci)
{
	int ret = 0;
	bool attach = false, inform_psy = true;
	u8 usb_status = MT6360_CHG_TYPE_NOVBUS;
	unsigned int regval;

	if (IS_ENABLED(CONFIG_TCPC_CLASS))
		attach = atomic_read(&mci->tcpc_attach);
	else
		attach = mci->pwr_rdy;
	if (mci->attach == attach) {
		dev_info(mci->dev, "%s: attach(%d) is the same\n",
				    __func__, attach);
		inform_psy = !attach;
		goto out;
	}
	mci->attach = attach;
	dev_info(mci->dev, "%s: attach = %d\n", __func__, attach);
	/* Plug out during BC12 */
	if (!attach) {
		dev_info(mci->dev, "%s: Charger Type: UNKONWN\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	}  else if (mci->is_in_hardreset &&
				(mci->psy_desc.type == POWER_SUPPLY_TYPE_UNKNOWN)) {
		dev_info(mci->dev, "%s: is_in_hardreset\n", __func__);
		mci->attach = false;
		return mt6360_toggle_usbchgen(mci);
	}

	/* Plug in */
	ret = regmap_read(mci->regmap, MT6360_PMU_USB_STATUS1, &regval);
	if (ret < 0)
		goto out;
	usb_status = (regval & MT6360_USB_STATUS_MASK) >>
						MT6360_USB_STATUS_SHFT;
	switch (usb_status) {
	case MT6360_CHG_TYPE_UNDER_GOING:
		dev_info(mci->dev, "%s: under going...\n", __func__);
		return ret;
	case MT6360_CHG_TYPE_SDP:
		dev_info(mci->dev,
			  "%s: Charger Type: STANDARD_HOST\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case MT6360_CHG_TYPE_SDPNSTD:
		dev_info(mci->dev,
			  "%s: Charger Type: NONSTANDARD_CHARGER\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case MT6360_CHG_TYPE_CDP:
		dev_info(mci->dev,
			  "%s: Charger Type: CHARGING_HOST\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case MT6360_CHG_TYPE_DCP:
		dev_info(mci->dev,
			  "%s: Charger Type: STANDARD_CHARGER\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	}
#ifdef MT6360_APPLE_SAMSUNG_TA_SUPPORT
	if (usb_status == MT6360_CHG_TYPE_SDPNSTD) {
		int chg_type = 0;

		dev_info(mci->dev, "%s: call mt6360_detect_apple_samsung_ta()\n", __func__);
		ret = regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
						MT6360_USBCHGEN_MASK, 0);
		if (ret < 0)
			dev_notice(mci->dev, "%s: disable chgdet fail\n", __func__);
		usleep_range(1000, 1100);

		ret = mt6360_detect_apple_samsung_ta(mci, &chg_type);
		if (ret < 0) {
			dev_notice(mci->dev,
				   "%s: detect apple/samsung ta fail(%d)\n",
				   __func__, ret);
		} else {
			switch (chg_type) {
			case SAMSUNG_CHARGER:
			case APPLE_0_5A_CHARGER:
			case APPLE_1_0A_CHARGER:
			case APPLE_2_1A_CHARGER:
			case APPLE_2_4A_CHARGER:
				mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
				break;
			}
		}
	}
#endif /* MT6360_APPLE_SAMSUNG_TA_SUPPORT */
out:
	if (!attach) {
		ret = __mt6360_enable_usbchgen(mci, false);
		if (ret < 0)
			dev_notice(mci->dev, "%s: disable chgdet fail\n",
				   __func__);
	} else if ((mci->psy_desc.type != POWER_SUPPLY_TYPE_USB_DCP)
		&& (mci->is_need_bc12 == true))
		mt6360_set_usbsw_state(mci, MT6360_USBSW_USB);
	if (!inform_psy)
		return ret;
	if (usb_status == MT6360_CHG_TYPE_DCP) {
		msleep(200);
		power_supply_changed(mci->psy);
	} else
		power_supply_changed(mci->psy);
	return ret;
}

static const u32 mt6360_vinovp_list[] = {
	5500000, 6500000, 10500000, 14500000,
};

static int mt6360_select_vinovp(struct mt6360_chg_info *mci, u32 uV)
{
	int i;

	if (uV < mt6360_vinovp_list[0])
		return -EINVAL;
	for (i = 1; i < ARRAY_SIZE(mt6360_vinovp_list); i++) {
		if (uV < mt6360_vinovp_list[i])
			break;
	}
	i--;
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL19,
				  MT6360_CHG_VIN_OVP_VTHSEL_MASK,
				  i << MT6360_CHG_VIN_OVP_VTHSEL_SHFT);
}

static const u32 mt6360_chrdetovp_list[] = {
	600000, 6500000, 7000000, 7500000,
	8500000, 9500000, 10500000, 11500000,
	12500000, 14500000,
};

static int mt6360_select_chrdetovp(struct mt6360_chg_info *mci, u32 uV)
{
	int i;

	if (uV < mt6360_chrdetovp_list[0])
		return -EINVAL;
	for (i = 1; i < ARRAY_SIZE(mt6360_chrdetovp_list); i++) {
		if (uV < mt6360_chrdetovp_list[i])
			break;
	}
	i--;

	return regmap_update_bits(mci->regmap,
					  MT6360_PMU_CHRDET_CTRL1,
					  MT6360_CHRDETB_VIN_OVP_VTHSEL_MASK,
					  i << MT6360_CHRDETB_VIN_OVP_VTHSEL_SHFT);
}

static inline int mt6360_read_zcv(struct mt6360_chg_info *mci)
{
	int ret = 0;
	u8 zcv_data[2] = {0};

	dev_dbg(mci->dev, "%s\n", __func__);
	/* Read ZCV data */
	ret = regmap_bulk_read(mci->regmap, MT6360_PMU_ADC_BAT_DATA_H,
			       zcv_data, 2);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: read zcv data fail\n", __func__);
		return ret;
	}
	mci->zcv = 1250 * (zcv_data[0] * 256 + zcv_data[1]);
	dev_info(mci->dev, "%s: zcv = (0x%02X, 0x%02X, %dmV)\n",
		 __func__, zcv_data[0], zcv_data[1], mci->zcv/1000);
	/* Disable ZCV */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_ADC_CONFIG,
				 MT6360_ZCV_EN_MASK, 0);
	if (ret < 0)
		dev_dbg(mci->dev, "%s: disable zcv fail\n", __func__);
	return ret;
}

static int __mt6360_set_aicr(struct mt6360_chg_info *mci, u32 uA)
{
	int ret = 0;
	u32 data = 0;

	dev_info(mci->dev, "%s: %d\n", __func__, uA);
	/* Disable sys drop improvement for download mode */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL20,
				 MT6360_EN_SDI_MASK, 0);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: disable en_sdi fail\n", __func__);
		return ret;
	}
	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_AICR],
					 uA, &data);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL3,
				  MT6360_AICR_MASK,
				  data << MT6360_AICR_SHFT);
}

static int __mt6360_set_ichg(struct mt6360_chg_info *mci, u32 uA)
{
	int ret = 0;
	u32 data = 0;

	dev_info(mci->dev, "%s: %d\n", __func__, uA);
	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_ICHG],
					 uA, &data);
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL7,
				 MT6360_ICHG_MASK,
				 data << MT6360_ICHG_SHFT);
	if (ret < 0)
		dev_notice(mci->dev, "%s: fail\n", __func__);
	else
		mci->ichg = uA;
	return ret;
}

static int __mt6360_enable(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;
	u32 ichg_ramp_t = 0;

	mt_dbg(mci->dev, "%s: en = %d\n", __func__, en);

	/* Workaround for vsys overshoot */
	mutex_lock(&mci->ichg_lock);
	if (mci->ichg < 500000) {
		dev_info(mci->dev,
			 "%s: ichg < 500mA, bypass vsys wkard\n", __func__);
		goto out;
	}
	if (!en) {
		mci->ichg_dis_chg = mci->ichg;
		ichg_ramp_t = (mci->ichg - 500000) / 50000 * 2;
		/* Set ichg to 500mA */
		ret = regmap_update_bits(mci->regmap,
					 MT6360_PMU_CHG_CTRL7,
					 MT6360_ICHG_MASK,
					 0x04 << MT6360_ICHG_SHFT);
		if (ret < 0) {
			dev_notice(mci->dev,
				   "%s: set ichg fail\n", __func__);
			goto vsys_wkard_fail;
		}
		mdelay(ichg_ramp_t);
	} else {
		if (mci->ichg == mci->ichg_dis_chg) {
			ret = __mt6360_set_ichg(mci, mci->ichg);
			if (ret < 0) {
				dev_notice(mci->dev,
					   "%s: set ichg fail\n", __func__);
				goto out;
			}
		}
	}

out:
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL2,
				 MT6360_CHG_EN_MASK, en ? 0xff : 0);
	if (ret < 0)
		dev_notice(mci->dev, "%s: fail, en = %d\n", __func__, en);
vsys_wkard_fail:
	mutex_unlock(&mci->ichg_lock);
	return ret;
}

static int __mt6360_is_enabled(struct mt6360_chg_info *mci, bool *en)
{
	int ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL2, &regval);
	if (ret < 0)
		return ret;
	*en = regval & MT6360_CHG_EN_MASK ? true : false;
	return 0;
}

static int mt6360_enable_pump_express(struct mt6360_chg_info *mci,
				      bool pe20)
{
	long timeout, pe_timeout = pe20 ? 1400 : 2800;
	int ret = 0;

	dev_info(mci->dev, "%s\n", __func__);
	ret = __mt6360_set_aicr(mci, 800000);
	if (ret < 0)
		return ret;
	mutex_lock(&mci->ichg_lock);
	ret = __mt6360_set_ichg(mci, 2000000);
	mutex_unlock(&mci->ichg_lock);
	if (ret < 0)
		return ret;
	ret = __mt6360_enable(mci, true);
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_EN_PUMPX_MASK, 0);
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_EN_PUMPX_MASK, 0xff);
	if (ret < 0)
		return ret;
	reinit_completion(&mci->pumpx_done);
	atomic_set(&mci->pe_complete, 1);
	timeout = wait_for_completion_interruptible_timeout(
			       &mci->pumpx_done, msecs_to_jiffies(pe_timeout));
	if (timeout == 0)
		ret = -ETIMEDOUT;
	else if (timeout < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0)
		dev_dbg(mci->dev,
			"%s: wait pumpx timeout, ret = %d\n", __func__, ret);
	return ret;
}

static int __mt6360_set_pep20_current_pattern(struct mt6360_chg_info *mci,
					    u32 uV)
{
	int ret = 0;
	u32 data = 0;

	dev_dbg(mci->dev, "%s: pep2.0 = %d\n", __func__, uV);
	mutex_lock(&mci->pe_lock);
	linear_range_get_selector_within(
		&mt6360_chg_range[MT6360_RANGE_PUMPX20], uV, &data);
	/* Set to PE2.0 */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_PUMPX_20_10_MASK, 0xff);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: enable pumpx 20 fail\n", __func__);
		goto out;
	}
	/* Set Voltage */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL17,
				 MT6360_PUMPX_DEC_MASK,
				 data << MT6360_PUMPX_DEC_SHFT);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set pumpx voltage fail\n", __func__);
		goto out;
	}
	ret = mt6360_enable_pump_express(mci, true);
out:
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static int __mt6360_get_adc(struct mt6360_chg_info *mci,
			    enum mt6360_adc_channel channel, int *min, int *max)
{
	int ret = 0;

	ret = iio_read_channel_processed(mci->channels[channel], min);
	if (ret < 0) {
		dev_info(mci->dev, "%s: fail(%d)\n", __func__, ret);
		return ret;
	}
	*max = *min;
	return 0;
}

static int __mt6360_enable_chg_type_det(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);

	if (!IS_ENABLED(CONFIG_TCPC_CLASS) || pdata->bc12_sel != 0)
		return ret;

	mutex_lock(&mci->chgdet_lock);
	if (atomic_read(&mci->tcpc_attach) == en) {
		dev_info(mci->dev, "%s attach(%d) is the same\n",
			 __func__, atomic_read(&mci->tcpc_attach));
		goto out;
	}
	atomic_set(&mci->tcpc_attach, en);
	ret = (en ? mt6360_chgdet_pre_process :
		    mt6360_chgdet_post_process)(mci);

	if (!en)
		mt6360_set_is_in_hardreset(mci, false);
out:
	mutex_unlock(&mci->chgdet_lock);
	return ret;
}

static int __mt6360_enable_otg(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	ret = mt6360_enable_wdt(mci, en ? true : false);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set wdt fail, en = %d\n", __func__, en);
		return ret;
	}
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL1,
				  MT6360_OPA_MODE_MASK, en ? 0xff : 0);
}

/* ================== */
/* External Functions */
/* ================== */
#if IS_ENABLED(CONFIG_MTK_CHARGER)
static int mt6360_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	mutex_lock(&mci->ichg_lock);
	ret = __mt6360_set_ichg(mci, uA);
	mutex_unlock(&mci->ichg_lock);
	return ret;
}

static int mt6360_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval, value;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL7, &regval);
	if (ret < 0)
		return ret;
	regval = (regval & MT6360_ICHG_MASK) >> MT6360_ICHG_SHFT;
	ret = linear_range_get_value(&mt6360_chg_range[MT6360_RANGE_ICHG],
				     regval, &value);
	if (!ret)
		*uA = value;
	return 0;
}

static int mt6360_enable_hidden_mode(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	static const u8 pascode[] = { 0x69, 0x96, 0x63, 0x72, };
	int ret = 0;

	mutex_lock(&mci->hidden_mode_lock);
	if (en) {
		if (mci->hidden_mode_cnt == 0) {
			ret = regmap_bulk_write(mci->regmap,
					   MT6360_PMU_TM_PAS_CODE1, pascode, 4);
			if (ret < 0)
				goto err;
		}
		mci->hidden_mode_cnt++;
	} else {
		if (mci->hidden_mode_cnt == 1)
			ret = regmap_write(mci->regmap,
					   MT6360_PMU_TM_PAS_CODE1, 0x00);
		mci->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	mt_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	goto out;
err:
	dev_dbg(mci->dev, "%s failed, en = %d\n", __func__, en);
out:
	mutex_unlock(&mci->hidden_mode_lock);
	return ret;
}

static int mt6360_enable(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_enable(mci, en);
}

static int mt6360_is_enabled(struct charger_device *chg_dev, bool *en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_is_enabled(mci, en);
}

static int mt6360_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 300000;
	return 0;
}

static int mt6360_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 data = 0;

	dev_dbg(mci->dev, "%s: cv = %d\n", __func__, uV);
	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_VOREG],
					 uV, &data);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL4,
				  MT6360_VOREG_MASK,
				  data << MT6360_VOREG_SHFT);
}

static int mt6360_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval, value;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL4, &regval);
	if (ret < 0)
		return ret;
	regval = (regval & MT6360_VOREG_MASK) >> MT6360_VOREG_SHFT;
	ret = linear_range_get_value(&mt6360_chg_range[MT6360_RANGE_VOREG],
				     regval, &value);
	if (!ret)
		*uV = value;
	return 0;
}

static int mt6360_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_set_aicr(mci, uA);
}

static int mt6360_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval, value;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL3, &regval);
	if (ret < 0)
		return ret;
	regval = (regval & MT6360_AICR_MASK) >> MT6360_AICR_SHFT;
	ret = linear_range_get_value(&mt6360_chg_range[MT6360_RANGE_AICR],
				     regval, &value);
	if (!ret)
		*uA = value;
	return 0;
}

static int mt6360_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int mt6360_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 data = 0;

	dev_dbg(mci->dev, "%s: ieoc = %d\n", __func__, uA);
	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_IEOC],
					 uA, &data);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL9,
				  MT6360_IEOC_MASK,
				  data << MT6360_IEOC_SHFT);
}

static int mt6360_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 aicc_vth = 0, data = 0;
	u8 aicc_vth_sel = 0;
	int ret = 0;

	mt_dbg(mci->dev, "%s: mivr = %d\n", __func__, uV);
	if (uV < 3900000 || uV > 13400000) {
		dev_dbg(mci->dev,
			"%s: unsuitable mivr val(%d)\n", __func__, uV);
		return -EINVAL;
	}

	aicc_vth = uV + 200000;
	linear_range_get_selector_within(
		&mt6360_chg_range[MT6360_RANGE_AICC_VTH], aicc_vth, &data);
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL16,
				 MT6360_AICC_VTH_MASK,
				 aicc_vth_sel << MT6360_AICC_VTH_SHFT);
	if (ret < 0)
		return ret;

	linear_range_get_selector_within(&mt6360_chg_range[MT6360_RANGE_VMIVR],
					 uV, &data);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL6,
				  MT6360_MIVR_MASK,
				  data << MT6360_MIVR_SHFT);
}

static inline int mt6360_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval, value;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL6, &regval);
	if (ret < 0)
		return ret;
	regval = (regval & MT6360_MIVR_MASK) >> MT6360_MIVR_SHFT;
	ret = linear_range_get_value(&mt6360_chg_range[MT6360_RANGE_VMIVR],
				     regval, &value);
	if (!ret)
		*uV = value;
	return 0;
}

static int mt6360_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	*in_loop = (regval & MT6360_MIVR_EVT_MASK) >> MT6360_MIVR_EVT_SHFT;
	return 0;
}

static int mt6360_enable_te(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
				  MT6360_TE_EN_MASK, en ? 0xff : 0);
}

static int mt6360_set_pep_current_pattern(struct charger_device *chg_dev,
					  bool is_inc)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s: pe1.0 pump up = %d\n", __func__, is_inc);

	mutex_lock(&mci->pe_lock);
	/* Set to PE1.0 */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_PUMPX_20_10_MASK, 0);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: enable pumpx 10 fail\n", __func__);
		goto out;
	}

	/* Set Pump Up/Down */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL17,
				 MT6360_PUMPX_UP_DN_MASK,
				 is_inc ? 0xff : 0);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set pumpx up/down fail\n", __func__);
		goto out;
	}
	ret = mt6360_enable_pump_express(mci, false);
out:
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static int mt6360_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
	return 0;
}

static int mt6360_set_pep20_current_pattern(struct charger_device *chg_dev,
					    u32 uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_set_pep20_current_pattern(mci, uV);
}

static int mt6360_reset_ta(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_set_mivr(chg_dev, 4600000);
	if (ret < 0)
		return ret;
	ret = mt6360_select_input_current_limit(mci, MT6360_IINLMTSEL_AICR);
	if (ret < 0)
		return ret;
	ret = mt6360_set_aicr(chg_dev, 100000);
	if (ret < 0)
		return ret;
	msleep(250);
	return mt6360_set_aicr(chg_dev, 500000);
}

static int mt6360_enable_cable_drop_comp(struct charger_device *chg_dev,
					 bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);
	if (en)
		return ret;

	/* Set to PE2.0 */
	mutex_lock(&mci->pe_lock);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_PUMPX_20_10_MASK, 0xff);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: enable pumpx 20 fail\n", __func__);
		goto out;
	}
	/* Disable cable drop compensation */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL17,
				 MT6360_PUMPX_DEC_MASK,
				 0x1F << MT6360_PUMPX_DEC_SHFT);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set pumpx voltage fail\n", __func__);
		goto out;
	}
	ret = mt6360_enable_pump_express(mci, true);
out:
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static inline int mt6360_get_aicc(struct mt6360_chg_info *mci,
				  u32 *aicc_val)
{
	u8 aicc_sel = 0;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_AICC_RESULT, &regval);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: read aicc result fail\n", __func__);
		return ret;
	}
	aicc_sel = (regval & MT6360_AICC_RESULT_MASK) >>
						     MT6360_AICC_RESULT_SHFT;
	*aicc_val = (aicc_sel * 50000) + 100000;
	return 0;
}

static inline int mt6360_post_aicc_measure(struct charger_device *chg_dev,
					   u32 start, u32 stop, u32 step,
					   u32 *measure)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int cur, ret;
	unsigned int regval;

	mt_dbg(mci->dev,
		"%s: post_aicc = (%d, %d, %d)\n", __func__, start, stop, step);
	/* ALPS06847901 adjust mt6360_post_aicc_measure condition */
	for (cur = start; cur < (stop); cur += step) {
		/* set_aicr to cur */
		ret = mt6360_set_aicr(chg_dev, cur + step);
		if (ret < 0)
			return ret;
		usleep_range(150, 200);
		ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
		if (ret < 0)
			return ret;
		/* read mivr stat */
		if (regval & MT6360_MIVR_EVT_MASK)
			break;
	}
	*measure = cur;
	return 0;
}

static int mt6360_run_aicc(struct charger_device *chg_dev, u32 *uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	u32 aicc_val = 0, aicr_val;
	long timeout;
	bool mivr_stat = false;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: read mivr stat fail\n", __func__);
		return ret;
	}
	mivr_stat = (regval & MT6360_MIVR_EVT_MASK) ? true : false;
	if (!mivr_stat) {
		dev_dbg(mci->dev, "%s: mivr stat not act\n", __func__);
		return ret;
	}

	mutex_lock(&mci->pe_lock);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
				 MT6360_RG_EN_AICC_MASK, 0xff);
	if (ret < 0)
		goto out;
	/* Clear AICC measurement IRQ */
	reinit_completion(&mci->aicc_done);
	timeout = wait_for_completion_interruptible_timeout(
				   &mci->aicc_done, msecs_to_jiffies(3000));
	if (timeout == 0)
		ret = -ETIMEDOUT;
	else if (timeout < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0) {
		dev_dbg(mci->dev,
			"%s: wait AICC time out, ret = %d\n", __func__, ret);
		goto out;
	}
	/* get aicc_result */
	ret = mt6360_get_aicc(mci, &aicc_val);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: get aicc result fail\n", __func__);
		goto out;
	}

	if (!pdata->post_aicc)
		goto skip_post_aicc;

	dev_info(mci->dev, "%s: aicc pre val = %d\n", __func__, aicc_val);
	ret = mt6360_get_aicr(chg_dev, &aicr_val);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: get aicr fail\n", __func__);
		goto out;
	}
	ret = mt6360_set_aicr(chg_dev, aicc_val);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set aicr fail\n", __func__);
		goto out;
	}
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
				 MT6360_RG_EN_AICC_MASK, 0);
	if (ret < 0)
		goto out;
	/* always start/end aicc_val/aicc_val+100mA */
	/* ALPS06847901 adjust mt6360_post_aicc_measure condition */
	ret = mt6360_post_aicc_measure(chg_dev, aicc_val,
				       aicc_val + 100000, 50000, &aicc_val);
	if (ret < 0)
		goto out;
	ret = mt6360_set_aicr(chg_dev, aicr_val);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set aicr fail\n", __func__);
		goto out;
	}
	dev_info(mci->dev, "%s: aicc post val = %d\n", __func__, aicc_val);
skip_post_aicc:
	*uA = aicc_val;
out:
	/* Clear EN_AICC */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
				 MT6360_RG_EN_AICC_MASK, 0);
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static int mt6360_enable_power_path(struct charger_device *chg_dev,
					    bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL1,
				  MT6360_FSLP_MASK, en ? 0 : 0xff);
}

static int mt6360_is_power_path_enabled(struct charger_device *chg_dev,
						bool *en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL1, &regval);
	if (ret < 0)
		return ret;
	*en = (regval & MT6360_FSLP_MASK) ? false : true;
	return 0;
}

static int mt6360_enable_safety_timer(struct charger_device *chg_dev,
					      bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL12,
				  MT6360_TMR_EN_MASK, en ? 0xff : 0);
}

static int mt6360_is_safety_timer_enabled(
				struct charger_device *chg_dev, bool *en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL12, &regval);
	if (ret < 0)
		return ret;
	*en = (regval & MT6360_TMR_EN_MASK) ? true : false;
	return 0;
}

static int mt6360_enable_hz(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s: en = %d\n", __func__, en);

	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL1,
				  MT6360_HZ_EN_MASK, en ? 0xff : 0);
}

static const u32 otg_oc_table[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000,
};

static int mt6360_set_otg_current_limit(struct charger_device *chg_dev,
						u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int i;

	/* Set higher OC threshold protect */
	for (i = 0; i < ARRAY_SIZE(otg_oc_table); i++) {
		if (uA <= otg_oc_table[i])
			break;
	}
	dev_dbg(mci->dev,
		"%s: select oc threshold = %d\n", __func__, otg_oc_table[i]);

	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL10,
				  MT6360_OTG_OC_MASK,
				  i << MT6360_OTG_OC_SHFT);
}

static int mt6360_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

//+wangchunhua2.wt, add, 20240912, disable hz in otg mode
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	if(en){
		mt6360_enable_hz(chg_dev,0);
	}
#endif
//-wangchunhua2.wt, add, 20240912, disable hz in otg mode

	return __mt6360_enable_otg(mci, en);
}

static int mt6360_enable_discharge(struct charger_device *chg_dev,
					   bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int i, ret = 0;
	const int dischg_retry_cnt = 3;
	bool is_dischg;
	unsigned int regval;

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	ret = mt6360_enable_hidden_mode(mci->chg_dev, true);
	if (ret < 0)
		return ret;
	/* Set bit2 of reg[0x31] to 1/0 to enable/disable discharging */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_HIDDEN_CTRL2,
				 MT6360_DISCHG_MASK, en ? 0xff : 0);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: fail, en = %d\n", __func__, en);
		goto out;
	}

	if (!en) {
		for (i = 0; i < dischg_retry_cnt; i++) {
			ret = regmap_read(mci->regmap,
					  MT6360_PMU_CHG_HIDDEN_CTRL2, &regval);
			is_dischg = (regval & MT6360_DISCHG_MASK) ?
								   true : false;
			if (!is_dischg)
				break;
			ret = regmap_update_bits(mci->regmap,
						MT6360_PMU_CHG_HIDDEN_CTRL2,
						MT6360_DISCHG_MASK, 0);
			if (ret < 0) {
				dev_dbg(mci->dev,
					"%s: disable dischg failed\n",
					__func__);
				goto out;
			}
		}
		if (i == dischg_retry_cnt) {
			dev_dbg(mci->dev, "%s: dischg failed\n", __func__);
			ret = -EINVAL;
		}
	}
out:
	mt6360_enable_hidden_mode(mci->chg_dev, false);
	return ret;
}

static int mt6360_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_enable_chg_type_det(mci, en);
}

static int mt6360_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
			  int *min, int *max)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	enum mt6360_adc_channel channel;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		channel = MT6360_ADC_VBUSDIV5;
		break;
	case ADC_CHANNEL_VSYS:
		channel = MT6360_ADC_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		channel = MT6360_ADC_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		channel = MT6360_ADC_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		channel = MT6360_ADC_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		channel = MT6360_ADC_TEMP_JC;
		break;
	case ADC_CHANNEL_USBID:
		channel = MT6360_ADC_USBID;
		break;
	case ADC_CHANNEL_TS:
		channel = MT6360_ADC_TS;
		break;
	default:
		return -ENOTSUPP;
	}
	return __mt6360_get_adc(mci, channel, min, max);
}

static int mt6360_get_vsys(struct charger_device *chg_dev, u32 *vsys)
{
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_VSYS, vsys, vsys);
}

static int mt6360_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_VBUS, vbus, vbus);
}

static int mt6360_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_IBUS, ibus, ibus);
}

static int mt6360_get_ibat(struct charger_device *chg_dev, u32 *ibat)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_IBAT, ibat, ibat);
}

static int mt6360_get_tchg(struct charger_device *chg_dev,
				   int *tchg_min, int *tchg_max)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int temp_jc = 0, ret = 0, retry_cnt = 3;

	mt_dbg(mci->dev, "%s\n", __func__);
	/* temp abnormal Workaround */
	do {
		ret = mt6360_get_adc(chg_dev, ADC_CHANNEL_TEMP_JC,
				     &temp_jc, &temp_jc);
		if (ret < 0) {
			dev_dbg(mci->dev,
				"%s: failed, ret = %d\n", __func__, ret);
			return ret;
		}
	} while (temp_jc >= 120 && (retry_cnt--) > 0);
	mutex_lock(&mci->tchg_lock);
	if (temp_jc >= 120)
		temp_jc = mci->tchg;
	else
		mci->tchg = temp_jc;
	mutex_unlock(&mci->tchg_lock);
	*tchg_min = *tchg_max = temp_jc;
	dev_info(mci->dev, "%s: tchg = %d\n", __func__, temp_jc);
	return 0;
}

static int mt6360_kick_wdt(struct charger_device *chg_dev)
{
	unsigned int regval;
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_dbg(mci->dev, "%s\n", __func__);
	return regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL1, &regval);
}

static int mt6360_safety_check(struct charger_device *chg_dev, u32 polling_ieoc)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret, ibat = 0;
	static int eoc_cnt;

	mt_dbg(mci->dev, "%s\n", __func__);
	ret = iio_read_channel_processed(mci->channels[MT6360_ADC_IBAT],
					 &ibat);
	if (ret < 0)
		dev_dbg(mci->dev, "%s: failed, ret = %d\n", __func__, ret);

	if (ibat <= polling_ieoc)
		eoc_cnt++;
	else
		eoc_cnt = 0;
	/* If ibat is less than polling_ieoc for 3 times, trigger EOC event */
	if (eoc_cnt == 3) {
		dev_info(mci->dev, "%s: polling_ieoc = %d, ibat = %d\n",
			 __func__, polling_ieoc, ibat);
		charger_dev_notify(mci->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		eoc_cnt = 0;
	}
	return ret;
}

static int mt6360_reset_eoc_state(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);

	ret = mt6360_enable_hidden_mode(mci->chg_dev, true);
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_HIDDEN_CTRL1,
				 MT6360_EOC_RST_MASK, 0xff);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: set failed, ret = %d\n", __func__, ret);
		goto out;
	}
	udelay(100);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_HIDDEN_CTRL1,
				 MT6360_EOC_RST_MASK, 0);
	if (ret < 0) {
		dev_dbg(mci->dev,
			"%s: clear failed, ret = %d\n", __func__, ret);
		goto out;
	}
out:
	mt6360_enable_hidden_mode(mci->chg_dev, false);
	return ret;
}

static int mt6360_is_charging_done(struct charger_device *chg_dev,
					   bool *done)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	enum mt6360_charging_status chg_stat;
	int ret = 0;

	mt_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_get_charging_status(mci, &chg_stat);
	if (ret < 0)
		return ret;
	*done = (chg_stat == MT6360_CHG_STATUS_DONE) ? true : false;
	return 0;
}

static int mt6360_get_zcv(struct charger_device *chg_dev, u32 *uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s: zcv = %dmV\n", __func__, mci->zcv / 1000);
	*uV = mci->zcv;
	return 0;
}

static int mt6360_dump_registers(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int i, ret = 0;
	int adc_vals[MT6360_ADC_MAX];
	u32 ichg = 0, aicr = 0, mivr = 0, cv = 0, ieoc = 0;
	enum mt6360_charging_status chg_stat = MT6360_CHG_STATUS_READY;
	bool chg_en = false;
	u8 chg_stat1 = 0, chg_ctrl[2] = {0};
	unsigned int regval;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_get_ichg(chg_dev, &ichg);
	ret |= mt6360_get_aicr(chg_dev, &aicr);
	ret |= mt6360_get_mivr(chg_dev, &mivr);
	ret |= mt6360_get_cv(chg_dev, &cv);
	ret |= mt6360_get_ieoc(mci, &ieoc);
	ret |= mt6360_get_charging_status(mci, &chg_stat);
	ret |= mt6360_is_charger_enabled(mci, &chg_en);
	if (ret < 0) {
		dev_notice(mci->dev, "%s: parse chg setting fail\n", __func__);
		return ret;
	}
	for (i = 0; i < MT6360_ADC_MAX; i++) {
		/* Skip unnecessary channel */
		if (i >= MT6360_ADC_NODUMP)
			break;
		ret = iio_read_channel_processed(mci->channels[i],
						 &adc_vals[i]);
		if (ret < 0) {
			dev_dbg(mci->dev,
				"%s: read [%s] adc fail(%d)\n",
				__func__, mt6360_adc_chan_list[i], ret);
			return ret;
		}
	}
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	chg_stat1 = regval;

	ret = regmap_bulk_read(mci->regmap, MT6360_PMU_CHG_CTRL1, chg_ctrl, 2);
	if (ret < 0)
		return ret;
	dev_info(mci->dev,
		 "%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV, IEOC = %dmA, CV = %dmV\n",
		 __func__, ichg / 1000, aicr / 1000, mivr / 1000, ieoc / 1000,
		 cv / 1000);
	dev_info(mci->dev,
		 "%s: VBUS = %dmV, IBUS = %dmA, VSYS = %dmV, VBAT = %dmV, IBAT = %dmA\n",
		 __func__,
		 adc_vals[MT6360_ADC_VBUSDIV5] / 1000,
		 adc_vals[MT6360_ADC_IBUS] / 1000,
		 adc_vals[MT6360_ADC_VSYS] / 1000,
		 adc_vals[MT6360_ADC_VBAT] / 1000,
		 adc_vals[MT6360_ADC_IBAT] / 1000);
	dev_info(mci->dev, "%s: CHG_EN = %d, CHG_STATUS = %s, CHG_STAT1 = 0x%02X\n",
		 __func__, chg_en, mt6360_chg_status_name[chg_stat], chg_stat1);
	dev_info(mci->dev, "%s: CHG_CTRL1 = 0x%02X, CHG_CTRL2 = 0x%02X\n",
		 __func__, chg_ctrl[0], chg_ctrl[1]);
	return 0;
}

//+gujiayin.wt,add shipmode ctrl begin
static int mt6360_set_shipmode(struct charger_device *chg_dev,bool en)
{
	/* Set to ship mode */
	int ret = 0;
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	if (en) {
		ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
						MT6360_SHIP_MODE_MASK, en);
		if (ret < 0) {
			dev_err(mci->dev, "%s:set shipmode fail\n", __func__);
		}
		dev_err(mci->dev, "%s:set shipmode succ\n", __func__);
	}
	return 0;
}

static int mt6360_set_shipmode_delay(struct charger_device *chg_dev,bool en)
{
	/* Set to ship mode delay*/
	int ret = 0;
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	if (en) {
		ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
						MT6360_SHIP_DELAY_MASK, en);
		if (ret < 0) {
			dev_err(mci->dev, "%s:set shipmode delay fail\n", __func__);
		}
		dev_err(mci->dev, "%s:set shipmode delay succ\n", __func__);
	}
	return 0;
}
//-gujiayin,wt,add shipmode ctrl end

static int mt6360_do_event(struct charger_device *chg_dev, u32 event,
				   u32 args)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(mci->psy);
		break;
	default:
		break;
	}

	return 0;
}

static int mt6360_plug_in(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_enable_wdt(mci, true);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: en wdt failed\n", __func__);
		return ret;
	}
	return mt6360_enable_te(chg_dev, true);
}

static int mt6360_plug_out(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_enable_wdt(mci, false);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: disable wdt failed\n", __func__);
		return ret;
	}
	return mt6360_enable_te(chg_dev, false);
}

static int mt6360_enable_usbid(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL1,
				  MT6360_USBID_EN_MASK, en ? 0xff : 0);
}

static int mt6360_set_usbid_rup(struct charger_device *chg_dev, u32 rup)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 data = mt6360_trans_usbid_rup(rup);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL1,
				  MT6360_ID_RPULSEL_MASK,
				  data << MT6360_ID_RPULSEL_SHFT);
}

static int mt6360_set_usbid_src_ton(struct charger_device *chg_dev, u32 src_ton)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 data = mt6360_trans_usbid_src_ton(src_ton);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL1,
				  MT6360_ISTDET_MASK,
				  data << MT6360_ISTDET_SHFT);
}

static int mt6360_enable_usbid_floating(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL2,
				  MT6360_USBID_FLOAT_MASK, en ? 0xff : 0);
}

static int mt6360_enable_force_typec_otp(struct charger_device *chg_dev,
					 bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return regmap_update_bits(mci->regmap, MT6360_PMU_TYPEC_OTP_CTRL,
				  MT6360_TYPEC_OTP_SWEN_MASK, en ? 0xff : 0);
}

static int mt6360_get_ctd_dischg_status(struct charger_device *chg_dev,
					u8 *status)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	*status = mci->ctd_dischg_status;
	return 0;
}

/* ALPS09479827 Charger: add charger class APIs of MT6360 for factory */
static int mt6360_set_eoc_timer(struct charger_device *chg_dev,
					unsigned int time)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	if (time > 3)
		time = 3;
	dev_info(mci->dev, "%s: set eoc time: %d\n", __func__, time);
	return regmap_update_bits(mci->regmap,
					MT6360_PMU_CHG_CTRL7,
					MT6360_EOC_TIMER_MASK,
					time << 0);
}

static int mt6360_enable_eoc(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s: eoc en: %d\n", __func__, en);

	return regmap_update_bits(mci->regmap,
					MT6360_PMU_CHG_CTRL9,
					MT6360_EOC_EN_MASK,
					en ? 0xff: 0);
}

static int mt6360_set_iinlmtsel(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);

	if (en)
		return regmap_update_bits(mci->regmap,
					MT6360_PMU_CHG_CTRL2,
					MT6360_IINLMTSEL_MASK,
					MT6360_IINLMTSEL_AICR <<
							MT6360_IINLMTSEL_SHFT);
	else
		return regmap_update_bits(mci->regmap,
					MT6360_PMU_CHG_CTRL2,
					MT6360_IINLMTSEL_MASK,
					MT6360_IINLMTSEL_AICR_3250 <<
							MT6360_IINLMTSEL_SHFT);
}

static int mt6360_en_ilim(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s enable ilim, en = %d\n", __func__, en);

	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL3,
				  MT6360_ILIM_EN_MASK, en ? 0xff : 0);
}

static int mt6360_en_wdt(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_info(mci->dev, "%s enable wdt, en = %d\n", __func__, en);

	if (!pdata->en_wdt)
		return 0;
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL13,
				  MT6360_CHG_WDT_EN_MASK,
				  en ? 0xff : 0);
}

static int mt6360_enable_aicc(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s enable aicc, en = %d\n", __func__, en);

	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL14,
				  MT6360_RG_EN_AICC_MASK,
				  en ? 0xff : 0);
}

static int mt6360_enable_ship_mode(struct charger_device *chg_dev, bool battfet)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret;

	dev_info(mci->dev, "%s , battfet dis dly = %d\n", __func__, battfet);

	/* disable shipping mode rst */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CORE_CTRL2,
				 MT6360_MASK_SHIP_RST_DIS, 0xff);
	if (ret < 0) {
		dev_dbg(mci->dev,
			"%s: fail to disable ship reset\n", __func__);
		goto out;
	}

	/* set ship delay */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CORE_CTRL2,
				 MT6360_BATDET_DIS_DLY_MASK, battfet ? 0xff : 0);
	if (ret < 0) {
		dev_dbg(mci->dev,
			"%s: fail to set ship delay\n", __func__);
		goto out;
	}

	/* enter shipping mode and disable cfo_en/chg_en */
	ret = regmap_write(mci->regmap, MT6360_PMU_CHG_CTRL2, 0x80);
	if (ret < 0)
		dev_dbg(mci->dev,
			"%s: fail to enter shipping mode\n", __func__);
out:
	return ret;
}

/* ALPS05319729 Charger: add two charger class APIs of MT6360 */
static int mt6360_get_health(struct charger_device *chg_dev, int *health)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	*health = POWER_SUPPLY_HEALTH_GOOD;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT2, &regval);
	if (ret < 0)
		return ret;
#if POWER_SUPPLY_EXT_HEALTH
	if (regval & MT6360_CHG_VSYSOV_MASK)
		*health = POWER_SUPPLY_EXT_HEALTH_VSYS_OVP;
#endif

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT4, &regval);
	if (ret < 0)
		return ret;
	if (regval & MT6360_OTPI_MASK)
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
	if (regval & MT6360_CHG_TMRI_MASK)
		*health = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT4, &regval);
	if (ret < 0)
		return ret;
	if (regval & MT6360_WDTMRI_MASK)
		*health = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;

	ret = regmap_read(mci->regmap, MT6360_PMU_FOD_STAT, &regval);
	if (ret < 0)
		return ret;
	if (regval & MT6360_CHRDETB_OVP_MASK)
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
#if POWER_SUPPLY_EXT_HEALTH
	if (!(regval & MT6360_CHRDETB_UVPB_MASK))
		*health = POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE;
#endif

	return 0;
}

static int mt6360_get_charge_type(struct charger_device *chg_dev, int *type)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	u32 aicc = 0;
	unsigned int regval;
	enum mt6360_charging_status chg_stat = MT6360_CHG_STATUS_READY;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT, &regval);
	if (ret < 0)
		return ret;
	chg_stat = (regval & MT6360_CHG_STAT_MASK) >> MT6360_CHG_STAT_SHFT;
	if (chg_stat != MT6360_CHG_STATUS_PROGRESS) {
		*type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		return 0;
	}
	if ((regval & MT6360_VBAT_TRICKLE_MASK) ||
	    !(regval & MT6360_VBAT_LVL_MASK)) {
		*type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		return 0;
	}

	ret = mt6360_run_aicc(chg_dev, &aicc);
	if (ret < 0)
		return ret;
	if (aicc > 0 && aicc <= 400000) // ALPS06452406 slow charging over 400mA
		*type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else
		*type = POWER_SUPPLY_CHARGE_TYPE_FAST;

	return 0;
}

static const struct charger_ops mt6360_chg_ops = {
	/* cable plug in/out */
	.plug_in = mt6360_plug_in,
	.plug_out = mt6360_plug_out,
	/* enable */
	.enable = mt6360_enable,
	.is_enabled = mt6360_is_enabled,
	/* charging current */
	.set_charging_current = mt6360_set_ichg,
	.get_charging_current = mt6360_get_ichg,
	.get_min_charging_current = mt6360_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = mt6360_set_cv,
	.get_constant_voltage = mt6360_get_cv,
	/* charging input current */
	.set_input_current = mt6360_set_aicr,
	.get_input_current = mt6360_get_aicr,
	.get_min_input_current = mt6360_get_min_aicr,
	/* set termination current */
	.set_eoc_current = mt6360_set_ieoc,
	/* charging mivr */
	.set_mivr = mt6360_set_mivr,
	.get_mivr = mt6360_get_mivr,
	.get_mivr_state = mt6360_get_mivr_state,
	/* charing termination */
	.enable_termination = mt6360_enable_te,
	/* PE+/PE+20 */
	.send_ta_current_pattern = mt6360_set_pep_current_pattern,
	.set_pe20_efficiency_table = mt6360_set_pep20_efficiency_table,
	.send_ta20_current_pattern = mt6360_set_pep20_current_pattern,
	.reset_ta = mt6360_reset_ta,
	.enable_cable_drop_comp = mt6360_enable_cable_drop_comp,
	.run_aicl = mt6360_run_aicc,
	/* Power path */
	.enable_powerpath = mt6360_enable_power_path,
	.is_powerpath_enabled = mt6360_is_power_path_enabled,
	/* safety timer */
	.enable_safety_timer = mt6360_enable_safety_timer,
	.is_safety_timer_enabled = mt6360_is_safety_timer_enabled,
	/* OTG */
	.enable_otg = mt6360_enable_otg,
	.set_boost_current_limit = mt6360_set_otg_current_limit,
	.enable_discharge = mt6360_enable_discharge,
	/* Charger type detection */
	.enable_chg_type_det = mt6360_enable_chg_type_det,
	/* ADC */
	.get_adc = mt6360_get_adc,
	.get_vsys_adc = mt6360_get_vsys,
	.get_vbus_adc = mt6360_get_vbus,
	.get_ibus_adc = mt6360_get_ibus,
	.get_ibat_adc = mt6360_get_ibat,
	.get_tchg_adc = mt6360_get_tchg,
	/* kick wdt */
	.kick_wdt = mt6360_kick_wdt,
	/* misc */
	.safety_check = mt6360_safety_check,
	.reset_eoc_state = mt6360_reset_eoc_state,
	.is_charging_done = mt6360_is_charging_done,
	.get_zcv = mt6360_get_zcv,
	.dump_registers = mt6360_dump_registers,
	.enable_hz = mt6360_enable_hz,
	/* event */
	.event = mt6360_do_event,
	//gujiayin.wt,add shipmode ctrl
	/* ship mode */
	.set_shipmode = mt6360_set_shipmode,
	.set_shipmode_delay = mt6360_set_shipmode_delay,
	/* TypeC */
	.enable_usbid = mt6360_enable_usbid,
	.set_usbid_rup = mt6360_set_usbid_rup,
	.set_usbid_src_ton = mt6360_set_usbid_src_ton,
	.enable_usbid_floating = mt6360_enable_usbid_floating,
	.enable_force_typec_otp = mt6360_enable_force_typec_otp,
	.get_ctd_dischg_status = mt6360_get_ctd_dischg_status,
	.enable_hidden_mode = mt6360_enable_hidden_mode,

	.get_health = mt6360_get_health,
	.get_charge_type = mt6360_get_charge_type,

	.set_eoc_timer = mt6360_set_eoc_timer,
	.enable_eoc = mt6360_enable_eoc,
	.enable_ship_mode = mt6360_enable_ship_mode,
	.enable_aicc = mt6360_enable_aicc,
	.en_wdt = mt6360_en_wdt,
	.en_ilim = mt6360_en_ilim,
	.set_iinlmtsel = mt6360_set_iinlmtsel,
};

static const struct charger_properties mt6360_chg_props = {
	.alias_name = "mt6360_chg",
};
#endif /* CONFIG_MTK_CHARGER */

static irqreturn_t mt6360_pmu_chg_treg_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	if ((regval & MT6360_CHG_TREG_MASK) >> MT6360_CHG_TREG_SHFT)
		dev_dbg(mci->dev,
			"%s: thermal regulation loop is active\n", __func__);
	return IRQ_HANDLED;
}

static void mt6360_pmu_chg_irq_enable(const char *name, int en);
static irqreturn_t mt6360_pmu_chg_mivr_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	mt_dbg(mci->dev, "%s\n", __func__);
	mt6360_pmu_chg_irq_enable("chg_mivr_evt", 0);
	atomic_inc(&mci->mivr_cnt);
	wake_up(&mci->mivr_wq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_pwr_rdy_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	bool pwr_rdy = false;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	pwr_rdy = (regval & MT6360_PWR_RDY_EVT_MASK);
	dev_info(mci->dev, "%s: pwr_rdy = %d\n", __func__, pwr_rdy);

	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_batsysuv_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vsysuv_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vsysov_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vbatov_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vbusov_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	struct chgdev_notify *noti = &(mci->chg_dev->noti);
	bool vbusov_stat = false;
	int ret = 0;
	unsigned int regval;

	dev_warn(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT2, &regval);
	if (ret < 0)
		goto out;
	vbusov_stat = (regval & BIT(7));
	noti->vbusov_stat = vbusov_stat;
	dev_info(mci->dev, "%s: stat = %d\n", __func__, vbusov_stat);
out:
#else
	dev_info(mci->dev, "%s\n", __func__);
#endif /* CONFIG_MTK_CHARGER */
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_wd_pmu_det_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_info(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_wd_pmu_done_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_info(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_tmri_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	int ret = 0;
	unsigned int regval;

	dev_notice(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT4, &regval);
	if (ret < 0)
		return IRQ_HANDLED;
	dev_info(mci->dev, "%s: chg_stat4 = 0x%02x\n", __func__, ret);
	if (!(regval & MT6360_CHG_TMRI_MASK))
		return IRQ_HANDLED;

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	charger_dev_notify(mci->chg_dev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
#endif /* CONFIG_MTK_CHARGER */
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_aiccmeasl_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	complete(&mci->aicc_done);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_wdtmri_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	int ret;
	unsigned int regval;

	dev_warn(mci->dev, "%s\n", __func__);
	/* Any I2C R/W can kick watchdog timer */
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL1, &regval);
	if (ret < 0)
		dev_dbg(mci->dev, "%s: kick wdt failed\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_rechgi_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	power_supply_changed(mci->psy);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_termi_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_ieoci_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	bool ieoc_stat = false;
	int ret = 0;
	unsigned int regval;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT4, &regval);
	if (ret < 0)
		goto out;
	ieoc_stat = (regval & BIT(7));
	if (!ieoc_stat)
		goto out;
	power_supply_changed(mci->psy);
out:
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_pumpx_donei_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_info(mci->dev, "%s\n", __func__);
	atomic_set(&mci->pe_complete, 0);
	complete(&mci->pumpx_done);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_attach_i_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_dbg(mci->dev, "%s\n", __func__);

	if (pdata->bc12_sel != 0)
		return IRQ_HANDLED;

	mutex_lock(&mci->chgdet_lock);
	if (!mci->bc12_en) {
		dev_dbg(mci->dev, "%s: bc12 disabled, ignore irq\n",
			__func__);
		goto out;
	}
	mt6360_chgdet_post_process(mci);
out:
	mutex_unlock(&mci->chgdet_lock);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_hvdcp_det_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_dcdti_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chrdet_ext_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	bool pwr_rdy = false;

	ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
	dev_info(mci->dev, "%s: pwr_rdy = %d\n", __func__, pwr_rdy);
	if (ret < 0)
		goto out;
	if (mci->pwr_rdy == pwr_rdy)
		goto out;
	mci->pwr_rdy = pwr_rdy;
	if (!IS_ENABLED(CONFIG_TCPC_CLASS) || pdata->bc12_sel != 0) {
		mutex_lock(&mci->chgdet_lock);
		(pwr_rdy ? mt6360_chgdet_pre_process :
			   mt6360_chgdet_post_process)(mci);
		mutex_unlock(&mci->chgdet_lock);
	}
	if (atomic_read(&mci->pe_complete) && pwr_rdy == true &&
	    mci->chip_rev <= 0x02) {
		dev_info(mci->dev, "%s: re-trigger pe20 pattern\n", __func__);
		queue_work(mci->pe_wq, &mci->pe_work);
	}
out:
	return IRQ_HANDLED;
}

static struct mt6360_pmu_irq_desc mt6360_pmu_chg_irq_desc[] = {
	{ "chg_treg_evt", mt6360_pmu_chg_treg_evt_handler },
	{ "chg_mivr_evt", mt6360_pmu_chg_mivr_evt_handler },
	{ "pwr_rdy_evt", mt6360_pmu_pwr_rdy_evt_handler },
	{ "chg_batsysuv_evt", mt6360_pmu_chg_batsysuv_evt_handler },
	{ "chg_vsysuv_evt", mt6360_pmu_chg_vsysuv_evt_handler },
	{ "chg_vsysov_evt", mt6360_pmu_chg_vsysov_evt_handler },
	{ "chg_vbatov_evt", mt6360_pmu_chg_vbatov_evt_handler },
	{ "chg_vbusov_evt", mt6360_pmu_chg_vbusov_evt_handler },
	{ "wd_pmu_det", mt6360_pmu_wd_pmu_det_handler },
	{ "wd_pmu_done", mt6360_pmu_wd_pmu_done_handler },
	{ "chg_tmri", mt6360_pmu_chg_tmri_handler },
	{ "chg_aiccmeasl", mt6360_pmu_chg_aiccmeasl_handler },
	{ "wdtmri", mt6360_pmu_wdtmri_handler },
	{ "chg_rechgi", mt6360_pmu_chg_rechgi_handler },
	{ "chg_termi", mt6360_pmu_chg_termi_handler },
	{ "chg_ieoci", mt6360_pmu_chg_ieoci_handler },
	{ "pumpx_donei", mt6360_pmu_pumpx_donei_handler },
	{ "attach_i", mt6360_pmu_attach_i_handler },
	{ "hvdcp_det", mt6360_pmu_hvdcp_det_handler },
	{ "dcdti", mt6360_pmu_dcdti_handler },
	{ "chrdet_ext_evt", mt6360_pmu_chrdet_ext_evt_handler },
};

static void mt6360_pmu_chg_irq_enable(const char *name, int en)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i = 0;

	if (unlikely(!name))
		return;
	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_chg_irq_desc); i++) {
		irq_desc = mt6360_pmu_chg_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		if (!strcmp(irq_desc->name, name)) {
			if (en)
				enable_irq(irq_desc->irq);
			else
				disable_irq_nosync(irq_desc->irq);
			break;
		}
	}
}

static void mt6360_pmu_chg_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_chg_irq_desc); i++) {
		irq_desc = mt6360_pmu_chg_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		ret = platform_get_irq_byname(pdev, irq_desc->name);
		if (ret < 0)
			continue;
		irq_desc->irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0)
			dev_dbg(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static int mt6360_toggle_cfo(struct mt6360_chg_info *mci)
{
	int ret = 0;
	u32 data = 0;

	ret = regmap_read(mci->regmap, MT6360_PMU_FLED_EN, &data);
	if (ret < 0)
		return ret;
	if (data & MT6360_STROBE_EN_MASK) {
		dev_notice(mci->dev, "%s: fled in strobe mode\n", __func__);
		goto out;
	}

	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
				 MT6360_CFO_EN_MASK, 0);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: failed to clear cfo_en\n", __func__);
		goto out;
	}
	if (regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
			       MT6360_CFO_EN_MASK, 0xFF) < 0)
		dev_dbg(mci->dev, "%s: failed to set cfo_en\n", __func__);
out:
	return ret;
}

static int mt6360_chg_mivr_task_threadfn(void *data)
{
	struct mt6360_chg_info *mci = data;
	u32 ibus;
	int ret;
	unsigned int regval;

	dev_info(mci->dev, "%s ++\n", __func__);
	while (!kthread_should_stop()) {
		wait_event(mci->mivr_wq, atomic_read(&mci->mivr_cnt) > 0 ||
							 kthread_should_stop());
		mt_dbg(mci->dev, "%s: enter mivr thread\n", __func__);
		if (kthread_should_stop())
			break;
		pm_stay_awake(mci->dev);
		/* check real mivr stat or not */
		ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
		if (ret < 0)
			goto loop_cont;
		if (!(regval & MT6360_MIVR_EVT_MASK)) {
			mt_dbg(mci->dev, "%s: mivr stat not act\n", __func__);
			goto loop_cont;
		}
		/* read ibus adc */
		ret = __mt6360_get_adc(mci, MT6360_ADC_IBUS, &ibus, &ibus);
		if (ret < 0) {
			dev_dbg(mci->dev, "%s: get ibus adc fail\n", __func__);
			goto loop_cont;
		}
		/* if ibus adc value < 100mA), toggle cfo */
		if (ibus < 100000) {
			dev_dbg(mci->dev, "%s: enter toggle cfo\n", __func__);
			ret = mt6360_toggle_cfo(mci);
			if (ret < 0)
				dev_dbg(mci->dev,
					"%s: toggle cfo fail\n", __func__);
		}
loop_cont:
		pm_relax(mci->dev);
		atomic_set(&mci->mivr_cnt, 0);
		mt6360_pmu_chg_irq_enable("chg_mivr_evt", 1);
		msleep(200);
	}
	dev_info(mci->dev, "%s --\n", __func__);
	return 0;
}

static void mt6360_trigger_pep_work_handler(struct work_struct *work)
{
	struct mt6360_chg_info *mci =
		(struct mt6360_chg_info *)container_of(work,
		struct mt6360_chg_info, pe_work);
	int ret = 0;

	ret = __mt6360_set_pep20_current_pattern(mci, 5000000);
	if (ret < 0)
		dev_dbg(mci->dev, "%s: trigger pe20 pattern fail\n",
			__func__);
}

static void mt6360_chgdet_work_handler(struct work_struct *work)
{
	int ret = 0;
	bool pwr_rdy = false;
	struct mt6360_chg_info *mci =
		(struct mt6360_chg_info *)container_of(work,
		struct mt6360_chg_info, chgdet_work);

	mutex_lock(&mci->chgdet_lock);
	/* Check PWR_RDY_STAT */
	ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
	if (ret < 0)
		goto out;
	/* power not good */
	if (!pwr_rdy)
		goto out;
	/* power good */
	mci->pwr_rdy = pwr_rdy;
	/* Turn on USB charger detection */
	ret = __mt6360_enable_usbchgen(mci, true);
	if (ret < 0)
		dev_dbg(mci->dev, "%s: en bc12 fail\n", __func__);
out:
	mutex_unlock(&mci->chgdet_lock);
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(ichg, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL7, 2, 0xFC,
			     mt6360_trans_ichg_sel, 0),
	MT6360_PDATA_VALPROP(aicr, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL3, 2, 0xFC,
			     mt6360_trans_aicr_sel, 0),
	MT6360_PDATA_VALPROP(mivr, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL6, 1, 0xFE,
			     mt6360_trans_mivr_sel, 0),
	MT6360_PDATA_VALPROP(cv, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL4, 1, 0xFE,
			     mt6360_trans_cv_sel, 0),
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	MT6360_PDATA_VALPROP(vrechg, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL11, 0, 0x03,
			     mt6360_trans_vrechg_sel, 0),
#endif
	MT6360_PDATA_VALPROP(ieoc, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL9, 4, 0xF0,
			     mt6360_trans_ieoc_sel, 0),
	MT6360_PDATA_VALPROP(safety_timer, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL12, 5, 0xE0,
			     mt6360_trans_safety_timer_sel, 0),
	MT6360_PDATA_VALPROP(ircmp_resistor, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL18, 3, 0x38,
			     mt6360_trans_ircmp_r_sel, 0),
	MT6360_PDATA_VALPROP(ircmp_vclamp, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL18, 0, 0x07,
			     mt6360_trans_ircmp_vclamp_sel, 0),
};

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(ichg, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(aicr, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(mivr, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(cv, struct mt6360_chg_platform_data),
#if defined(CONFIG_W2_CHARGER_PRIVATE)
	MT6360_DT_VALPROP(vrechg, struct mt6360_chg_platform_data),
#endif
	MT6360_DT_VALPROP(ieoc, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(safety_timer, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(ircmp_resistor, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(ircmp_vclamp, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(en_te, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(en_wdt, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(post_aicc, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(batoc_notify, struct mt6360_chg_platform_data),
};

static int mt6360_enable_ilim(struct mt6360_chg_info *mci, bool en)
{
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL3,
				  MT6360_ILIM_EN_MASK, en ? 0xff : 0);
}

static void mt6360_get_boot_mode(struct mt6360_chg_info *mci)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(mci->dev->of_node, "bootmode", 0);
	if (!boot_node)
		dev_info(mci->dev, "%s: failed to get boot mode phandle\n",
			 __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			dev_info(mci->dev, "%s: failed to get atag,boot\n",
				 __func__);
		else {
			dev_info(mci->dev,
			"%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			mci->bootmode = tag->bootmode;
			mci->boottype = tag->boottype;
		}
	}
}


static int mt6360_chg_init_setting(struct mt6360_chg_info *mci)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	unsigned int regval;

	dev_info(mci->dev, "%s\n", __func__);

	ret = regmap_read(mci->regmap, MT6360_PMU_DEV_INFO, &regval);
	if (ret >= 0)
		mci->chip_rev = regval & MT6360_CHIP_REV_MASK;

	ret = regmap_read(mci->regmap, MT6360_PMU_FOD_STAT, &regval);
	if (ret >= 0)
		mci->ctd_dischg_status = regval & 0xE3;

	if (regmap_update_bits(mci->regmap, MT6360_PMU_FOD_CTRL, 0x40, 0) < 0)
		dev_dbg(mci->dev, "%s: disable fod ctrl fail\n", __func__);

	ret = mt6360_select_input_current_limit(mci, MT6360_IINLMTSEL_AICR);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: select iinlmtsel by aicr fail\n",
			__func__);
		return ret;
	}
	usleep_range(5000, 6000);
	ret = mt6360_enable_ilim(mci, false);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: disable ilim fail\n", __func__);
		return ret;
	}

	mt6360_get_boot_mode(mci);
	if (mci->bootmode == 1 || mci->bootmode == 5) {
		/*1:META_BOOT 5:ADVMETA_BOOT*/
		ret = regmap_update_bits(mci->regmap,
					 MT6360_PMU_CHG_CTRL3,
					 MT6360_AICR_MASK,
					 0x02 << MT6360_AICR_SHFT);
		dev_info(mci->dev, "%s: set aicr to 200mA in meta mode\n",
			__func__);
	}

	ret = mt6360_enable_wdt(mci, false);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: disable wdt fail\n", __func__);
		return ret;
	}

	ret = mt6360_enable_usbchgen(mci, false);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: disable chg type detect fail\n",
			__func__);
		return ret;
	}

	ret = mt6360_select_vinovp(mci, 14500000);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: unlimit vin for pump express\n",
			__func__);
		return ret;
	}

	/* chrdet ovp limit for pump express, can be replaced by option */
	ret = mt6360_select_chrdetovp(mci, 12500000);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: unlimit chrdet for pump express\n",
			__func__);
		return ret;
	}

	if (pdata->en_te) {
		ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
					 MT6360_TE_EN_MASK, 0);
		if (ret < 0) {
			dev_dbg(mci->dev, "%s: disable te fail\n", __func__);
			return ret;
		}
	}

	ret = mt6360_read_zcv(mci);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: read zcv fail\n", __func__);
		return ret;
	}

#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
	ret = mt6360_enable_dcd_tout(mci, false);
	if (ret < 0)
		dev_notice(mci->dev, "%s disable dcd fail\n", __func__);
#endif

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT, &regval);
	if (ret < 0) {
		dev_dbg(mci->dev, "%s: read BATSYSUV fail\n", __func__);
		return ret;
	}
	if (!(ret & MT6360_CHG_BATSYSUV_MASK)) {
		dev_info(mci->dev, "%s: BATSYSUV occurred\n", __func__);
		if (regmap_update_bits(mci->regmap, MT6360_PMU_CHG_STAT,
					 MT6360_CHG_BATSYSUV_MASK, 0xff) < 0)
			dev_dbg(mci->dev,
				"%s: clear BATSYSUV fail\n", __func__);
	}

	/* USBID ID_TD = 32T */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL2,
				 MT6360_IDTD_MASK | MT6360_USBID_FLOAT_MASK,
				 0x62);

	/* Disable TypeC OTP for check EVB version by TS pin */
	return regmap_update_bits(mci->regmap, MT6360_PMU_TYPEC_OTP_CTRL,
				  MT6360_TYPEC_OTP_EN_MASK, 0);
}

static int mt6360_set_shipping_mode(struct mt6360_chg_info *mci)
{
	int ret;

	dev_info(mci->dev, "%s\n", __func__);
	/* disable shipping mode rst */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CORE_CTRL2,
				 MT6360_MASK_SHIP_RST_DIS, 0xff);
	if (ret < 0) {
		dev_dbg(mci->dev,
			"%s: fail to disable ship reset\n", __func__);
		goto out;
	}
	/* enter shipping mode and disable cfo_en/chg_en */
	ret = regmap_write(mci->regmap, MT6360_PMU_CHG_CTRL2, 0x80);
	if (ret < 0)
		dev_dbg(mci->dev,
			"%s: fail to enter shipping mode\n", __func__);
out:
	return ret;
}

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct mt6360_chg_info *mci = dev_get_drvdata(dev);
	int32_t tmp = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &tmp) < 0) {
		dev_notice(dev, "parsing number fail\n");
		return -EINVAL;
	}
	if (tmp != 5526789)
		return -EINVAL;
	ret = mt6360_set_shipping_mode(mci);
	if (ret < 0)
		return ret;
	return count;
}
static const DEVICE_ATTR_WO(shipping_mode);

/* ======================= */
/* MT6360 Power Supply Ops */
/* ======================= */
static int mt6360_charger_get_online(struct mt6360_chg_info *mci,
				     bool *val)
{
	int ret;
	bool pwr_rdy;

	if (IS_ENABLED(CONFIG_TCPC_CLASS)) {
		pwr_rdy = atomic_read(&mci->tcpc_attach);
	} else {
		/*uvp_d_stat=true => vbus_on=1*/
		ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
		if (ret < 0) {
			dev_notice(mci->dev,
				"%s: read uvp_d_stat fail\n", __func__);
			return ret;
		}
	}
	dev_info(mci->dev, "%s: online = %d\n", __func__, pwr_rdy);
	*val = pwr_rdy;
	return 0;
}

static int mt6360_charger_set_online(struct mt6360_chg_info *mci,
				     const union power_supply_propval *val)
{
	return __mt6360_enable_chg_type_det(mci, val->intval);
}

static int mt6360_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct mt6360_chg_info *mci = power_supply_get_drvdata(psy);
	enum mt6360_charging_status chg_stat = MT6360_CHG_STATUS_MAX;
	int ret = 0;
	bool pwr_rdy = false, chg_en = false;

	dev_dbg(mci->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = mt6360_charger_get_online(mci, &pwr_rdy);
		val->intval = pwr_rdy;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = mci->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = mci->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (mci->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = mt6360_charger_get_online(mci, &pwr_rdy);
		ret |= __mt6360_is_enabled(mci, &chg_en);
		ret |= mt6360_get_charging_status(mci, &chg_stat);
		if (ret < 0)
			return ret;
		if (!pwr_rdy) {
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			return ret;
		}
		switch (chg_stat) {
		case MT6360_CHG_STATUS_READY:
			fallthrough;
		case MT6360_CHG_STATUS_PROGRESS:
			if (chg_en)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case MT6360_CHG_STATUS_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		case MT6360_CHG_STATUS_FAULT:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		default:
			ret = -ENODATA;
			break;
		}
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static int mt6360_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct mt6360_chg_info *mci = power_supply_get_drvdata(psy);
	int ret = 0;

	dev_dbg(mci->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = mt6360_charger_set_online(mci, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		mci->is_need_bc12 = !!val->intval;
		dev_err(mci->dev, "get is_need_bc12=%d, psy value=%d\n", mci->is_need_bc12, val->intval);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int mt6360_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property mt6360_charger_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static const struct power_supply_desc mt6360_charger_desc = {
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= mt6360_charger_properties,
	.num_properties		= ARRAY_SIZE(mt6360_charger_properties),
	.get_property		= mt6360_charger_get_property,
	.set_property		= mt6360_charger_set_property,
	.property_is_writeable	= mt6360_charger_property_is_writeable,
	.usb_types		= mt6360_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(mt6360_charger_usb_types),
};

static const struct power_supply_desc mt6360_charger_poweroff_desc = {
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= mt6360_charger_properties,
	.num_properties		= ARRAY_SIZE(mt6360_charger_properties),
	.get_property		= mt6360_charger_get_property,
	.set_property		= mt6360_charger_set_property,
	.property_is_writeable	= mt6360_charger_property_is_writeable,
	.usb_types		= mt6360_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(mt6360_charger_usb_types),
};

static char *mt6360_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};
/*otg_vbus*/
static int mt6360_boost_enable(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);

	return __mt6360_enable_otg(mci, true);
}

static int mt6360_boost_disable(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);

	return __mt6360_enable_otg(mci, false);
}

static int mt6360_boost_is_enabled(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, desc->enable_reg, &regval);

	if (ret < 0)
		return ret;
	return regval & desc->enable_mask ? true : false;
}

static int mt6360_boost_set_voltage_sel(struct regulator_dev *rdev,
					unsigned int sel)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int shift = ffs(desc->vsel_mask) - 1;

	return regmap_update_bits(mci->regmap, desc->enable_reg,
				  desc->enable_mask, sel << shift);
}

static int mt6360_boost_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int shift = ffs(desc->vsel_mask) - 1, ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, desc->vsel_reg, &regval);
	if (ret < 0)
		return ret;
	return (regval & desc->vsel_mask) >> shift;
}

static int mt6360_boost_set_current_limit(struct regulator_dev *rdev,
					  int min_uA, int max_uA)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int i, shift = ffs(desc->csel_mask) - 1;

	for (i = 0; i < ARRAY_SIZE(mt6360_otg_oc_threshold); i++) {
		if (min_uA <= mt6360_otg_oc_threshold[i])
			break;
	}
	if (i == ARRAY_SIZE(mt6360_otg_oc_threshold) ||
		mt6360_otg_oc_threshold[i] > max_uA) {
		dev_notice(mci->dev,
			"%s: out of current range\n", __func__);
		return -EINVAL;
	}
	dev_info(mci->dev, "%s: select otg_oc = %d\n",
		 __func__, mt6360_otg_oc_threshold[i]);
	return regmap_update_bits(mci->regmap, desc->csel_reg,
				  desc->csel_mask, i << shift);
}

static int mt6360_boost_get_current_limit(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int shift = ffs(desc->csel_mask) - 1, ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, desc->csel_reg, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & desc->csel_mask) >> shift;
	if (ret >= ARRAY_SIZE(mt6360_otg_oc_threshold))
		return -EINVAL;
	return mt6360_otg_oc_threshold[ret];
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static int pd_tcp_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mt6360_chg_info *mci = container_of(nb,
		struct mt6360_chg_info, pd_nb);

	switch (event) {
	case TCP_NOTIFY_HARD_RESET_STATE:
		mutex_lock(&mci->chgdet_lock);
		if (noti->hreset_state.state >= TCP_HRESET_SIGNAL_SEND
			&& mci->cable_type == TCPC_CABLE_TYPE_C2C)
			mt6360_set_is_in_hardreset(mci, true);
		else
			mt6360_set_is_in_hardreset(mci, false);
		mutex_unlock(&mci->chgdet_lock);

		break;
	case TCP_NOTIFY_PD_STATE:
		dev_info(mci->dev, "%s pd state = %d\n",
			__func__, noti->pd_state.connected);
		if ((noti->pd_state.connected == PD_CONNECT_NONE) ||
			(noti->pd_state.connected == PD_CONNECT_HARD_RESET))
			mt6360_set_is_in_hardreset(mci, false);
		break;
	case TCP_NOTIFY_CABLE_TYPE:
		mci->cable_type = noti->cable_type.type;
		break;
	default:
		break;
	};

	return NOTIFY_OK;
}

static void mt6360_tcpc_notify_handler(struct work_struct *work)
{
	struct mt6360_chg_info *mci = container_of(work,
				struct mt6360_chg_info, tcpc_notify_work.work);
	int ret;

	dev_info(mci->dev, "%s\n", __func__);

	mci->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!mci->tcpc_dev) {
		pr_notice("%s get tcpc device type_c_port0 fail\n", __func__);
		return;
	}

	mci->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(mci->tcpc_dev, &mci->pd_nb,
						TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		pr_notice("%s: register tcpc notifer fail\n", __func__);
}
#endif

static const struct regulator_ops mt6360_chg_otg_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = mt6360_boost_enable,
	.disable = mt6360_boost_disable,
	.is_enabled = mt6360_boost_is_enabled,
	.set_voltage_sel = mt6360_boost_set_voltage_sel,
	.get_voltage_sel = mt6360_boost_get_voltage_sel,
	.set_current_limit = mt6360_boost_set_current_limit,
	.get_current_limit = mt6360_boost_get_current_limit,
};

static const struct regulator_desc mt6360_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &mt6360_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 4425000,
	.uV_step = 25000, /* 25mV per step */
	.n_voltages = 57, /* 4425mV to 5825mV */
	.vsel_reg = MT6360_PMU_CHG_CTRL5,
	.vsel_mask = MT6360_MASK_BOOST_VOREG,
	.enable_reg = MT6360_PMU_CHG_CTRL1,
	.enable_mask = MT6360_OPA_MODE_MASK,
	.csel_reg = MT6360_PMU_CHG_CTRL10,
	.csel_mask = MT6360_OTG_OC_MASK,
};

static int mt6360_pmu_chg_probe(struct platform_device *pdev)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct mt6360_chg_info *mci;
	struct iio_channel *channel;
	struct power_supply_config charger_cfg = {};
	struct regulator_config config = { };
	struct device_node *np = pdev->dev.of_node;
	struct device_node *node;
	int i, ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	if (np) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &def_platform_data, sizeof(*pdata));
		mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
		node = of_parse_phandle(np, "bc12_ref", 0);
		if (!node)
			dev_notice(&pdev->dev,
				"%s: can't parse phandle bc12_ref\n", __func__);
		if (of_property_read_u32(node, "bc12-sel-port0",
					 &pdata->bc12_sel) < 0)
			dev_notice(&pdev->dev,
				   "%s: can't parse bc12_sel\n", __func__);

		pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_dbg(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mci = devm_kzalloc(&pdev->dev, sizeof(*mci), GFP_KERNEL);
	if (!mci)
		return -ENOMEM;
	mci->dev = &pdev->dev;
	mci->hidden_mode_cnt = 0;
	mutex_init(&mci->hidden_mode_lock);
	mutex_init(&mci->pe_lock);
	mutex_init(&mci->aicr_lock);
	mutex_init(&mci->chgdet_lock);
	mutex_init(&mci->tchg_lock);
	mutex_init(&mci->ichg_lock);
	mci->tchg = 0;
	mci->ichg = 2000000;
	mci->ichg_dis_chg = 2000000;
	mci->attach = false;
	mci->is_need_bc12 = true;
	g_mci = mci;
	init_completion(&mci->aicc_done);
	init_completion(&mci->pumpx_done);
	atomic_set(&mci->pe_complete, 0);
	atomic_set(&mci->mivr_cnt, 0);
	atomic_set(&mci->tcpc_attach, 0);
	init_waitqueue_head(&mci->mivr_wq);
	if (!IS_ENABLED(CONFIG_TCPC_CLASS) && pdata->bc12_sel == 0)
		INIT_WORK(&mci->chgdet_work, mt6360_chgdet_work_handler);

	platform_set_drvdata(pdev, mci);

	INIT_DELAYED_WORK(&mci->hardreset_work, mt6360_in_hardreset_handler);

	/* get parent regmap */
	mci->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!mci->regmap) {
		dev_dbg(&pdev->dev, "Fail to get parent regmap\n");
		ret = -ENODEV;
		goto err_mutex_init;
	}
	/* apply platform data */
	ret = mt6360_pdata_apply_helper(mci->regmap, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0) {
		dev_dbg(&pdev->dev, "apply pdata fail\n");
		goto err_mutex_init;
	}
	/* Initial Setting */
	ret = mt6360_chg_init_setting(mci);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "%s: init setting fail\n", __func__);
		goto err_mutex_init;
	}

	/* Get ADC iio channels */
	for (i = 0; i < MT6360_ADC_MAX; i++) {
		channel = devm_iio_channel_get(&pdev->dev,
					       mt6360_adc_chan_list[i]);
		if (IS_ERR(channel)) {
			dev_dbg(&pdev->dev,
				"%s: iio channel get fail\n", __func__);
			ret = PTR_ERR(channel);
			goto err_mutex_init;
		}
		mci->channels[i] = channel;
	}

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	mci->chg_dev = charger_device_register(pdata->chg_name, mci->dev,
						mci, &mt6360_chg_ops,
						&mt6360_chg_props);
	if (IS_ERR(mci->chg_dev)) {
		dev_dbg(mci->dev, "charger device register fail\n");
		ret = PTR_ERR(mci->chg_dev);
		goto err_mutex_init;
	}
#endif /* CONFIG_MTK_CHARGER */

	mci->mivr_task = kthread_run(mt6360_chg_mivr_task_threadfn, mci,
				     "mivr_thread.%s", dev_name(mci->dev));
	ret = PTR_ERR_OR_ZERO(mci->mivr_task);
	if (ret < 0) {
		dev_dbg(mci->dev, "create mivr handling thread fail\n");
		goto err_register_chg_dev;
	}
	ret = device_create_file(mci->dev, &dev_attr_shipping_mode);
	if (ret < 0) {
		dev_notice(&pdev->dev, "create shipping attr fail\n");
		goto err_create_mivr_thread_run;
	}

	mci->pe_wq = create_singlethread_workqueue("pe_pattern");
	if (!mci->pe_wq) {
		dev_dbg(mci->dev, "%s: create pe_pattern work queue fail\n",
			__func__);
		goto err_shipping_mode_attr;
	}
	INIT_WORK(&mci->pe_work, mt6360_trigger_pep_work_handler);

	/* otg regulator */
	config.dev = &pdev->dev;
	config.driver_data = mci;
	mci->otg_rdev = devm_regulator_register(&pdev->dev,
						&mt6360_otg_rdesc, &config);
	if (IS_ERR(mci->otg_rdev)) {
		ret = PTR_ERR(mci->otg_rdev);
		goto err_register_otg;
	}

	if (mci->bootmode == 0) {
	/* power supply register */
		dev_info(&pdev->dev, "%s:Boot charging\n", __func__);
		memcpy(&mci->psy_desc,
			&mt6360_charger_desc, sizeof(mci->psy_desc));
	} else {
		dev_info(&pdev->dev, "%s:KERNEL_POWER_OFF_CHARGING_BOOT\n", __func__);
		memcpy(&mci->psy_desc,
			&mt6360_charger_poweroff_desc, sizeof(mci->psy_desc));
	}
	mci->psy_desc.name = pdata->chg_name;

	charger_cfg.drv_data = mci;
	charger_cfg.of_node = pdev->dev.of_node;
	charger_cfg.supplied_to = mt6360_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(mt6360_charger_supplied_to);
	mci->psy = devm_power_supply_register(&pdev->dev,
					      &mci->psy_desc, &charger_cfg);
	if (IS_ERR(mci->psy)) {
		dev_notice(&pdev->dev, "Fail to register power supply dev\n");
		ret = PTR_ERR(mci->psy);
		goto err_register_psy;
	}

	/* irq register */
	mt6360_pmu_chg_irq_register(pdev);
	device_init_wakeup(&pdev->dev, true);

	/* Schedule work for microB's BC1.2 */
	if (!IS_ENABLED(CONFIG_TCPC_CLASS) && pdata->bc12_sel == 0)
		schedule_work(&mci->chgdet_work);
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	mci->is_in_hardreset = false;
	INIT_DELAYED_WORK(&mci->tcpc_notify_work, mt6360_tcpc_notify_handler);
	schedule_delayed_work(&mci->tcpc_notify_work, msecs_to_jiffies(5000));
#endif

	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
	return 0;
err_register_psy:
err_register_otg:
	destroy_workqueue(mci->pe_wq);
err_shipping_mode_attr:
	device_remove_file(mci->dev, &dev_attr_shipping_mode);
err_create_mivr_thread_run:
	if (mci->mivr_task)
		kthread_stop(mci->mivr_task);
err_register_chg_dev:
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	charger_device_unregister(mci->chg_dev);
#endif /* CONFIG_MTK_CHARGER */
err_mutex_init:
	mutex_destroy(&mci->tchg_lock);
	mutex_destroy(&mci->chgdet_lock);
	mutex_destroy(&mci->aicr_lock);
	mutex_destroy(&mci->pe_lock);
	mutex_destroy(&mci->hidden_mode_lock);
	return -EPROBE_DEFER;
}

static int mt6360_pmu_chg_remove(struct platform_device *pdev)
{
	struct mt6360_chg_info *mci = platform_get_drvdata(pdev);

	dev_dbg(mci->dev, "%s\n", __func__);
	flush_workqueue(mci->pe_wq);
	destroy_workqueue(mci->pe_wq);
	if (mci->mivr_task) {
		atomic_inc(&mci->mivr_cnt);
		wake_up(&mci->mivr_wq);
		kthread_stop(mci->mivr_task);
	}
	device_remove_file(mci->dev, &dev_attr_shipping_mode);
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	charger_device_unregister(mci->chg_dev);
#endif /* CONFIG_MTK_CHARGER */
	mutex_destroy(&mci->tchg_lock);
	mutex_destroy(&mci->chgdet_lock);
	mutex_destroy(&mci->aicr_lock);
	mutex_destroy(&mci->pe_lock);
	mutex_destroy(&mci->hidden_mode_lock);
	return 0;
}

static int __maybe_unused mt6360_pmu_chg_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_chg_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_chg_pm_ops,
			 mt6360_pmu_chg_suspend, mt6360_pmu_chg_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_chg_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_chg", },
	{ .compatible = "mediatek,mt6360-chg", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_chg_of_id);

static const struct platform_device_id mt6360_pmu_chg_id[] = {
	{ "mt6360_pmu_chg", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_chg_id);

static struct platform_driver mt6360_pmu_chg_driver = {
	.driver = {
		.name = "mt6360_pmu_chg",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_chg_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_chg_of_id),
	},
	.probe = mt6360_pmu_chg_probe,
	.remove = mt6360_pmu_chg_remove,
	.id_table = mt6360_pmu_chg_id,
};

static int __init mt6360_pmu_chg_init(void)
{
	return platform_driver_register(&mt6360_pmu_chg_driver);
}
device_initcall_sync(mt6360_pmu_chg_init);

static void __exit mt6360_pmu_chg_exit(void)
{
	platform_driver_unregister(&mt6360_pmu_chg_driver);
}
module_exit(mt6360_pmu_chg_exit);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU CHG Driver");
MODULE_LICENSE("GPL");
