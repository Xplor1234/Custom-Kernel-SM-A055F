// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 MediaTek, Inc.
 *
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 */

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/mfd/mt6323/registers.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6363/core.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6357/core.h>
#include <linux/pinctrl/consumer.h>

/* 6358 pmic define */
#define MT6358_TOPSTATUS			(0x28)
#define MT6358_PSC_TOP_INT_CON0			(0x910)
#define MT6358_TOP_RST_MISC			(0x14c)
#define MT6358_PWRKEY_DEB_MASK			1
#define MT6358_HOMEKEY_DEB_MASK			3
#define MT6358_RG_INT_EN_HOMEKEY_MASK           1
#define MT6358_RG_INT_EN_PWRKEY_MASK            0
#define MT6358_PWRKEY_RST_SHIFT                 9
#define MT6358_HOMEKEY_RST_SHIFT                8
#define MT6358_RST_DU_SHIFT                     12
#define MTK_PMIC_PWRKEY_INDEX			0
#define MTK_PMIC_HOMEKEY_INDEX			1
#define MTK_PMIC_HOMEKEY2_INDEX			2
#define MTK_PMIC_MAX_KEY_COUNT			3
#define MT6397_PWRKEY_RST_SHIFT			6
#define MT6397_HOMEKEY_RST_SHIFT		5
#define MT6397_RST_DU_SHIFT			8
#define MT6359_PWRKEY_RST_SHIFT			9
#define MT6359_HOMEKEY_RST_SHIFT		8
#define MT6359_RST_DU_SHIFT			12
#define MT6363_PWRKEY_RST_SHIFT			2
#define MT6363_HOMEKEY_RST_SHIFT		4
#define MT6363_RST_DU_SHIFT			6
#define PWRKEY_RST_EN				1
#define HOMEKEY_RST_EN				1
#define RST_DU_MASK				3
#define RST_MODE_MASK				3
#define RST_PWRKEY_MODE				0
#define RST_PWRKEY_HOME_MODE			1
#define RST_PWRKEY_HOME2_MODE			2
#define RST_PWRKEY_HOME_HOME2_MODE		3
#define INVALID_VALUE				0
#define MT6357_PWRKEY_RST_SHIFT			9
#define MT6357_HOMEKEY_RST_SHIFT		8
#define MT6357_RST_DU_SHIFT			12
struct mtk_pmic_keys_regs {
	u32 deb_reg;
	u32 deb_mask;
	u32 intsel_reg;
	u32 intsel_mask;
};

#define MTK_PMIC_KEYS_REGS(_deb_reg, _deb_mask,		\
	_intsel_reg, _intsel_mask)			\
{							\
	.deb_reg		= _deb_reg,		\
	.deb_mask		= _deb_mask,		\
	.intsel_reg		= _intsel_reg,		\
	.intsel_mask		= _intsel_mask,		\
}

struct mtk_pmic_regs {
	const struct mtk_pmic_keys_regs keys_regs[MTK_PMIC_MAX_KEY_COUNT];
	bool release_irq;
	u32 pmic_rst_reg;
	u32 pmic_rst_para_reg;
	u32 pwrkey_rst_shift;
	u32 homekey_rst_shift;
	u32 rst_du_shift;
};

static const struct mtk_pmic_regs mt6397_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6397_CHRSTATUS,
		0x8, MT6397_INT_RSV, 0x10),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6397_OCSTATUS2,
		0x10, MT6397_INT_RSV, 0x8),
	.release_irq = false,
	.pmic_rst_reg = MT6397_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6397_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6397_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6397_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6323_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6323_CHRSTATUS,
		0x2, MT6323_INT_MISC_CON, 0x10),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6323_CHRSTATUS,
		0x4, MT6323_INT_MISC_CON, 0x8),
	.release_irq = false,
	.pmic_rst_reg = MT6323_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6397_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6397_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6397_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6359p_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(INVALID_VALUE,
		INVALID_VALUE, MT6359P_PSC_TOP_INT_CON0, 0x1),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(INVALID_VALUE,
		INVALID_VALUE, MT6359P_PSC_TOP_INT_CON0, 0x2),
	.release_irq = true,
	.pmic_rst_reg = MT6359P_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6359_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6359_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6359_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6363_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6363_TOPSTATUS,
		0x1, MT6363_PSC_TOP_INT_CON0, 0x0),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6363_TOPSTATUS,
		0x3, MT6363_PSC_TOP_INT_CON0, 0x12),
	.keys_regs[MTK_PMIC_HOMEKEY2_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6363_TOPSTATUS,
		0x4, MT6363_PSC_TOP_INT_CON0, 0x24),
	.release_irq = true,
	.pmic_rst_reg = MT6363_STRUP_CON11,
	.pmic_rst_para_reg = MT6363_STRUP_CON12,
	.pwrkey_rst_shift = MT6363_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6363_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6363_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6357_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6357_TOPSTATUS,
		MT6357_PWRKEY_DEB_MASK,
		MT6357_PSC_TOP_INT_CON0,
		MT6357_RG_INT_EN_PWRKEY_MASK),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6357_TOPSTATUS,
		MT6357_HOMEKEY_DEB_MASK,
		MT6357_PSC_TOP_INT_CON0,
		MT6357_RG_INT_EN_HOMEKEY_MASK),
	.release_irq = true,
	.pmic_rst_reg = MT6357_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6357_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6357_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6357_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6358_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6358_TOPSTATUS,
		MT6358_PWRKEY_DEB_MASK,
		MT6358_PSC_TOP_INT_CON0,
		MT6358_RG_INT_EN_PWRKEY_MASK),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6358_TOPSTATUS,
		MT6358_HOMEKEY_DEB_MASK,
		MT6358_PSC_TOP_INT_CON0,
		MT6358_RG_INT_EN_HOMEKEY_MASK),
	.release_irq = true,
	.pmic_rst_reg = MT6358_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6358_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6358_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6358_RST_DU_SHIFT,
};
struct mtk_pmic_keys_info {
	struct mtk_pmic_keys *keys;
	const struct mtk_pmic_keys_regs *regs;
	unsigned int keycode;
	int irq;
	int release_irq_num;
	struct wakeup_source *suspend_lock;
};

struct mtk_pmic_keys {
	struct input_dev *input_dev;
	struct device *dev;
	struct regmap *regmap;
	struct mtk_pmic_keys_info keys[MTK_PMIC_MAX_KEY_COUNT];
	struct pinctrl *pinctrl;
	struct pinctrl_state *kpcol0_pins_mode;
};

enum mtk_pmic_keys_lp_mode {
	LP_DISABLE,
	LP_ONEKEY,
	LP_TWOKEY_HOMEKEY,
	LP_TWOKEY_HOMEKEY2,
};

static struct platform_device *ktf_pmic_pdev;
static struct mtk_pmic_keys *ktf_pmic_key;
static void mtk_pmic_keys_lp_reset_setup(struct mtk_pmic_keys *keys,
		const struct mtk_pmic_regs *pmic_regs)
{
	int ret;
	u32 long_press_mode, long_press_debounce;
	u32 pmic_rst_reg = pmic_regs->pmic_rst_reg;
	u32 pmic_rst_para_reg = pmic_regs->pmic_rst_para_reg;
	u32 pwrkey_rst_shift =
		PWRKEY_RST_EN << pmic_regs->pwrkey_rst_shift;
	u32 homekey_rst_shift =
		RST_MODE_MASK << pmic_regs->homekey_rst_shift;

	if (pmic_rst_para_reg == INVALID_VALUE) {
		pmic_rst_para_reg = pmic_rst_reg;
		homekey_rst_shift = HOMEKEY_RST_EN << pmic_regs->homekey_rst_shift;
	}

	ret = of_property_read_u32(keys->dev->of_node,
		"power-off-time-sec", &long_press_debounce);
	if (ret)
		long_press_debounce = 0;

	ret = regmap_update_bits(keys->regmap, pmic_rst_para_reg,
			   RST_DU_MASK << pmic_regs->rst_du_shift,
			   long_press_debounce << pmic_regs->rst_du_shift);
	if (ret < 0) {
		dev_dbg(keys->dev,
			"regmap_update_bits fail: %d\n", ret);
	}

	ret = of_property_read_u32(keys->dev->of_node,
		"mediatek,long-press-mode", &long_press_mode);
	if (ret)
		long_press_mode = LP_DISABLE;

	switch (long_press_mode) {
	case LP_ONEKEY:
		ret = regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   pwrkey_rst_shift);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_ONEKEY: %d\n", ret);
		}
		ret = regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   RST_PWRKEY_MODE);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_ONEKEY: %d\n", ret);
		}
		break;
	case LP_TWOKEY_HOMEKEY:
		ret = regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   pwrkey_rst_shift);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_TWOKEY_HOMEKEY: %d\n", ret);
		}
		ret = regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   RST_PWRKEY_HOME_MODE << pmic_regs->homekey_rst_shift);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_TWOKEY_HOMEKEY: %d\n", ret);
		}
		break;
	case LP_TWOKEY_HOMEKEY2:
		ret = regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   pwrkey_rst_shift);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_TWOKEY_HOMEKEY2: %d\n", ret);
		}
		ret = regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   RST_PWRKEY_HOME2_MODE << pmic_regs->homekey_rst_shift);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_TWOKEY_HOMEKEY2: %d\n", ret);
		}
		break;
	case LP_DISABLE:
		ret = regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   0);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_DISABLE: %d\n", ret);
		}
		ret = regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   RST_PWRKEY_HOME_HOME2_MODE << pmic_regs->homekey_rst_shift);
		if (ret < 0) {
			dev_dbg(keys->dev,
				"regmap_update_bits fail LP_DISABLE: %d\n", ret);
		}
		break;
	default:
		break;
	}
}

static irqreturn_t mtk_pmic_keys_release_irq_handler_thread(
				int irq, void *data)
{
	struct mtk_pmic_keys_info *info = data;

	input_report_key(info->keys->input_dev, info->keycode, 0);
	input_sync(info->keys->input_dev);
	if (info->suspend_lock)
		__pm_relax(info->suspend_lock);
	dev_dbg(info->keys->dev, "release key =%d using PMIC\n",
			info->keycode);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_pmic_keys_irq_handler_thread(int irq, void *data)
{
	struct mtk_pmic_keys_info *info = data;
	u32 key_deb, pressed;
	int ret;

	if (info->release_irq_num > 0) {
		pressed = 1;
	} else {
		ret = regmap_read(info->keys->regmap, info->regs->deb_reg, &key_deb);
		if (ret < 0) {
			dev_dbg(info->keys->dev,
				"regmap_read fail: %d\n", ret);
		}
		key_deb &= info->regs->deb_mask;
		pressed = !key_deb;
	}

	input_report_key(info->keys->input_dev, info->keycode, pressed);
	input_sync(info->keys->input_dev);

	if (pressed && info->suspend_lock)
		__pm_stay_awake(info->suspend_lock);
	else if (info->suspend_lock)
		__pm_relax(info->suspend_lock);
	dev_dbg(info->keys->dev, "(%s) key =%d using PMIC\n",
		 pressed ? "pressed" : "released", info->keycode);

	return IRQ_HANDLED;
}

static int mtk_pmic_key_setup(struct mtk_pmic_keys *keys,
		struct mtk_pmic_keys_info *info)
{
	int ret;

	info->keys = keys;

	ret = regmap_update_bits(keys->regmap, info->regs->intsel_reg,
				 info->regs->intsel_mask,
				 info->regs->intsel_mask);
	if (ret < 0)
		return ret;

	ret = devm_request_threaded_irq(keys->dev, info->irq, NULL,
					mtk_pmic_keys_irq_handler_thread,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"mtk-pmic-keys", info);
	if (ret) {
		dev_err(keys->dev, "Failed to request IRQ: %d: %d\n",
			info->irq, ret);
		return ret;
	}
	if (info->release_irq_num > 0) {
		ret = devm_request_threaded_irq(keys->dev,
				info->release_irq_num,
				NULL, mtk_pmic_keys_release_irq_handler_thread,
				IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				"mtk-pmic-keys", info);
		if (ret) {
			dev_dbg(keys->dev, "Failed to request IRQ: %d: %d\n",
				info->release_irq_num, ret);
			return ret;
		}
	}

	input_set_capability(keys->input_dev, EV_KEY, info->keycode);

	return 0;
}

static int keypad_pinctrl_init(struct mtk_pmic_keys *keys)
{
	int ret = 0;

	keys->pinctrl = devm_pinctrl_get(keys->dev);
	if (IS_ERR(keys->pinctrl)) {
		dev_dbg(keys->dev, "Failed to get keypad pinctrl handler");
		return PTR_ERR(keys->pinctrl);
	}

	keys->kpcol0_pins_mode = pinctrl_lookup_state(keys->pinctrl, "kpcol0_mode");
	if (!IS_ERR(keys->kpcol0_pins_mode)) {
		ret = pinctrl_select_state(keys->pinctrl, keys->kpcol0_pins_mode);
		if (ret) {
			dev_dbg(keys->dev, "failed to switch kpcol0 to gpio mode, ret: %d\n", ret);
			return ret;
		}
	} else {
		dev_dbg(keys->dev, "failed to get pinctrl state: %s\n", "kpcol0_mode");
		return PTR_ERR(keys->kpcol0_pins_mode);
	}

	return ret;
}

#if IS_ENABLED(CONFIG_SEC_DEBUG)
struct mtk_pmic_keys *secdbg_keys;

int mtk_pmic_pwrkey_status(void)
{
	struct mtk_pmic_keys_info *pwrkey;
	const struct mtk_pmic_keys_regs *regs;
	u32 key_deb, pressed;

	if (!secdbg_keys)
		return -EINVAL;

	pwrkey = &secdbg_keys->keys[MTK_PMIC_PWRKEY_INDEX];
	regs = pwrkey->regs;

	regmap_read(secdbg_keys->regmap, regs->deb_reg, &key_deb);
	dev_info(secdbg_keys->dev, "Read register 0x%x and mask 0x%x and value: 0x%x\n",
		 regs->deb_reg, 1 << regs->deb_mask, key_deb);
	key_deb &= 1 << regs->deb_mask;
	pressed = !key_deb;

	dev_info(secdbg_keys->dev, "%s power key\n", pressed ? "pressed" : "released");

	return pressed;
}
EXPORT_SYMBOL(mtk_pmic_pwrkey_status);

int mtk_pmic_homekey_status(void)
{
	struct mtk_pmic_keys_info *homekey;
	const struct mtk_pmic_keys_regs *regs;
	u32 key_deb, pressed;

	if (!secdbg_keys)
		return -EINVAL;

	homekey = &secdbg_keys->keys[MTK_PMIC_HOMEKEY_INDEX];
	regs = homekey->regs;

	regmap_read(secdbg_keys->regmap, regs->deb_reg, &key_deb);
	dev_info(secdbg_keys->dev, "Read register 0x%x and mask 0x%x and value: 0x%x\n",
		 regs->deb_reg, 1 << regs->deb_mask, key_deb);
	key_deb &= 1 << regs->deb_mask;
	pressed = !key_deb;

	dev_info(secdbg_keys->dev, "%s home key\n", pressed ? "pressed" : "released");

	return pressed;
}
EXPORT_SYMBOL(mtk_pmic_homekey_status);
#endif

static int __maybe_unused mtk_pmic_keys_suspend(struct device *dev)
{
	struct mtk_pmic_keys *keys = dev_get_drvdata(dev);
	int index;

	for (index = 0; index < MTK_PMIC_MAX_KEY_COUNT; index++) {
		if (keys->keys[index].suspend_lock)
			enable_irq_wake(keys->keys[index].irq);
	}

	return 0;
}

static int __maybe_unused mtk_pmic_keys_resume(struct device *dev)
{
	struct mtk_pmic_keys *keys = dev_get_drvdata(dev);
	int index;

	for (index = 0; index < MTK_PMIC_MAX_KEY_COUNT; index++) {
		if (keys->keys[index].suspend_lock)
			disable_irq_wake(keys->keys[index].irq);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_pmic_keys_pm_ops, mtk_pmic_keys_suspend,
			mtk_pmic_keys_resume);

static const struct of_device_id of_mtk_pmic_keys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6359p-keys",
		.data = &mt6359p_regs,
	}, {
		.compatible = "mediatek,mt6397-keys",
		.data = &mt6397_regs,
	}, {
		.compatible = "mediatek,mt6323-keys",
		.data = &mt6323_regs,
	}, {
		.compatible = "mediatek,mt6363-keys",
		.data = &mt6363_regs,
	}, {
		.compatible = "mediatek,mt6358-keys",
		.data = &mt6358_regs,
	}, {
		.compatible = "mediatek,mt6357-keys",
		.data = &mt6357_regs,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, of_mtk_pmic_keys_match_tbl);

static int mtk_pmic_keys_probe(struct platform_device *pdev)
{
	int error, index = 0;
	unsigned int keycount;
	unsigned int release_irq_interval;
	struct mt6397_chip *pmic_chip;
	struct device_node *node = pdev->dev.of_node, *child;
	struct mtk_pmic_keys *keys;
	const struct mtk_pmic_regs *mtk_pmic_regs;
	struct input_dev *input_dev;
	const struct of_device_id *of_id =
		of_match_device(of_mtk_pmic_keys_match_tbl, &pdev->dev);

	ktf_pmic_pdev = pdev;
	keys = devm_kzalloc(&pdev->dev, sizeof(*keys), GFP_KERNEL);
	if (!keys)
		return -ENOMEM;

	keys->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!keys->regmap) {
		pmic_chip =  dev_get_drvdata(pdev->dev.parent);
		if (!pmic_chip || !pmic_chip->regmap) {
			dev_info(keys->dev, "failed to get pmic key regmap\n");
			return -ENODEV;
		}

		keys->regmap = pmic_chip->regmap;
	}

	keys->dev = &pdev->dev;
	mtk_pmic_regs = of_id->data;

	keys->input_dev = input_dev = devm_input_allocate_device(keys->dev);
	if (!input_dev) {
		dev_dbg(keys->dev, "input allocate device fail.\n");
		return -ENOMEM;
	}

	input_dev->name = "mtk-pmic-keys";
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0001;

	__set_bit(EV_KEY, input_dev->evbit);
	keycount = of_get_available_child_count(node);
	if(strncmp(of_id->compatible, "mediatek,mt6363-keys", 20) == 0)
		release_irq_interval = 3;
	else
		release_irq_interval = 2;
	ktf_pmic_key = keys;
	if (keycount > MTK_PMIC_MAX_KEY_COUNT) {
		dev_err(keys->dev, "too many keys defined (%d)\n", keycount);
		return -EINVAL;
	}

	for_each_child_of_node(node, child) {
		keys->keys[index].regs = &mtk_pmic_regs->keys_regs[index];

		keys->keys[index].irq = platform_get_irq(pdev, index);
		if (keys->keys[index].irq < 0)
			return keys->keys[index].irq;
		if (mtk_pmic_regs->release_irq) {
			keys->keys[index].release_irq_num = platform_get_irq(
						pdev,
						index + release_irq_interval);
			if (keys->keys[index].release_irq_num < 0)
				return keys->keys[index].release_irq_num;
		}

		error = of_property_read_u32(child,
			"linux,keycodes", &keys->keys[index].keycode);
		if (error) {
			dev_dbg(keys->dev,
				"failed to read key:%d linux,keycode property: %d\n",
				index, error);
			of_node_put(child);
			return error;
		}

		if (of_property_read_bool(child, "wakeup-source"))
			keys->keys[index].suspend_lock =
				wakeup_source_register(NULL, "pwrkey wakelock");

		error = mtk_pmic_key_setup(keys, &keys->keys[index]);
		if (error) {
			of_node_put(child);
			return error;
		}

		if (index == 2) {
			error = keypad_pinctrl_init(keys);
			if (error < 0) {
				dev_dbg(keys->dev, "failed to init keypad gpio\n");
				return error;
			}
		}

		index++;
	}

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev,
			"register input device failed (%d)\n", error);
		return error;
	}

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	secdbg_keys = keys;
#endif


	mtk_pmic_keys_lp_reset_setup(keys, mtk_pmic_regs);

	platform_set_drvdata(pdev, keys);

	return 0;
}

static struct platform_driver pmic_keys_pdrv = {
	.probe = mtk_pmic_keys_probe,
	.driver = {
		   .name = "mtk-pmic-keys",
		   .of_match_table = of_mtk_pmic_keys_match_tbl,
		   .pm = &mtk_pmic_keys_pm_ops,
	},
};

module_platform_driver(pmic_keys_pdrv);

int ktf_mtk_pmic_kpd_test(char *str)
{
	int ret = 0;

	if (!str)
		return -EINVAL;
	if (!ktf_pmic_pdev)
		return -ENODEV;
	if (!ktf_pmic_key)
		return -ENODEV;
	if (!strncmp(str, "pmicsuspend", 11)) {
		mtk_pmic_keys_suspend(ktf_pmic_key->dev);
		ret = mtk_pmic_keys_resume(ktf_pmic_key->dev);
	} else if (!strncmp(str, "pmicprobe", 9)) {
		ret = mtk_pmic_keys_probe(ktf_pmic_pdev);
	} else {
		pr_info("%s is fail", __func__);
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(ktf_mtk_pmic_kpd_test);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("MTK pmic-keys driver v0.1");
