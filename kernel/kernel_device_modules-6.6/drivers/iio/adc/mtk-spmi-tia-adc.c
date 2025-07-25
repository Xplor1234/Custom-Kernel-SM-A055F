// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/mfd/mt6368/registers.h>
#include <linux/mfd/mt6373/registers.h>
#include <linux/mfd/mt6377/registers.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/syscore_ops.h>

#include <dt-bindings/iio/mt635x-auxadc.h>

#define AUXADC_DEF_AVG_NUM		32
#define VOLT_FULL			1840

#define AUXADC_TIA_VALID_BIT 15
#define AUXADC_RC_OFFSET    16
#define AUXADC_RC_MASK      (GENMASK(17, 16))

#define get_tia_rc_sel(val, offset, mask) (((val) & (mask)) >> (offset))
#define is_adc_data_valid(val, bit)       (((val) & BIT(bit)) != 0)
#define get_adc_data(val, bit)            ((val) & GENMASK(bit, 0))

#define DT_CHANNEL_CONVERT(val)		((val) & 0xFF)
#define DT_PURES_CONVERT(val)		(((val) & 0xFF00) >> 8)

struct pmic_tia_adc_device {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	struct iio_chan_spec *iio_chans;
	unsigned int nchannels;
	const struct auxadc_info *info;
	struct regulator *isink_load;
};

/*
 * @ch_name:	HW channel name
 * @res:	ADC resolution
 * @r_ratio:	resistance ratio, represented by r_ratio[0] / r_ratio[1]
 * @avg_num:	sampling times of AUXADC measurments then average it
 * @regs:	request and data output registers for this channel
 */
struct auxadc_channels {
	enum iio_chan_type type;
	long info_mask;
	/* AUXADC channel attribute */
	const char *ch_name;
	unsigned char res;
	unsigned char r_ratio[2];
	unsigned short avg_num;
	void __iomem *tia_auxadc_reg;
};

#define TIA_AUXADC_CH0	0
#define TIA_AUXADC_CH1  1
#define TIA_AUXADC_CH2	2
#define TIA_AUXADC_CHAN_MAX 3

#define TIA_AUXADC_CHANNEL(_ch_name, _res)	\
	[TIA_AUXADC_##_ch_name] = {				\
		.type = IIO_VOLTAGE,			\
		.info_mask = BIT(IIO_CHAN_INFO_RAW) |		\
			     BIT(IIO_CHAN_INFO_PROCESSED),	\
		.ch_name = __stringify(_ch_name),	\
		.res = _res,				\
	}

/*
 * The array represents all possible AUXADC channels found
 * in the supported PMICs.
 */
static struct auxadc_channels auxadc_chans[] = {
	TIA_AUXADC_CHANNEL(CH0, 14),
	TIA_AUXADC_CHANNEL(CH1, 14),
	TIA_AUXADC_CHANNEL(CH2, 14),
};

#define TIA_PU_R_100K_TYPE_0  (0)    // 100K
#define TIA_PU_R_30K_TYPE_1  (1)    // 30K
#define TIA_PU_R_400K_TYPE_2  (2)    // 400K

#define TIA_PU_R_30K    (30000)
#define TIA_PU_R_100K   (100000)
#define TIA_PU_R_400K   (400000)

#define TIA_DEFAULT_PULLUP_V    (184000)

unsigned int tia2_rc_sel_to_value(unsigned int sel)
{
	unsigned int resistance;

	switch (sel) {
	case 1:
		resistance = TIA_PU_R_30K; /* 30K */
		break;
	case 2:
		resistance = TIA_PU_R_400K; /* 400K */
		break;
	case 0:
	default:
		resistance = TIA_PU_R_100K; /* 100K */
		break;
	}

	return resistance;
}

unsigned long long adc2volt(unsigned int adc_raw)
{
	return ((unsigned long long)adc_raw * TIA_DEFAULT_PULLUP_V) >> 15;
}

static unsigned int convert_adc_data(struct device *dev, unsigned int raw_data, unsigned int pullup_r)
{
	unsigned long long v_in, cal_pullup_r, convert_v_in;
	unsigned int convert_raw;

	v_in = adc2volt(raw_data);
	if(v_in == TIA_DEFAULT_PULLUP_V) {
		dev_info(dev, "[TIA_TEST] v_in=%llu ===== fail.\n", v_in);
		return -EINVAL;
	}

	cal_pullup_r = (v_in * pullup_r) /(TIA_DEFAULT_PULLUP_V - v_in);
	convert_v_in = (TIA_DEFAULT_PULLUP_V * cal_pullup_r) / (TIA_PU_R_100K + cal_pullup_r);
	convert_raw = (unsigned int) (convert_v_in * (32768) /TIA_DEFAULT_PULLUP_V);

	dev_dbg_ratelimited(dev, "[TIA_TEST_A] raw_data=%d/%d/%llu/%llu/%llu/%d\n",
		raw_data, pullup_r, v_in, cal_pullup_r, convert_v_in, convert_raw);

	return convert_raw;
}

static int pmic_adc_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int *val, int *val2, long mask)
{
	struct pmic_tia_adc_device *tia_adc_dev = iio_priv(indio_dev);
	unsigned int r_type = 0, reg_val = 0, temp_auxadc = 0, auxadc_out = 0;
	bool is_valid;
	int ret = 0;

	mutex_lock(&tia_adc_dev->lock);
	switch (chan->channel) {
	case TIA_AUXADC_CH0:
	case TIA_AUXADC_CH1:
	case TIA_AUXADC_CH2:
		if(unlikely(auxadc_chans[chan->channel].tia_auxadc_reg))
			reg_val = readl(auxadc_chans[chan->channel].tia_auxadc_reg);
		is_valid = is_adc_data_valid(reg_val, AUXADC_TIA_VALID_BIT);
		if(!is_valid)
			ret = -EAGAIN;
		temp_auxadc = get_adc_data(reg_val, (AUXADC_TIA_VALID_BIT-1));

		r_type = get_tia_rc_sel(reg_val, AUXADC_RC_OFFSET, AUXADC_RC_MASK);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	switch (r_type) {
	case TIA_PU_R_100K_TYPE_0:
		auxadc_out = temp_auxadc;
		break;
	case TIA_PU_R_30K_TYPE_1:
	case TIA_PU_R_400K_TYPE_2:
		auxadc_out = convert_adc_data(tia_adc_dev->dev, temp_auxadc, tia2_rc_sel_to_value(r_type));
		break;
	default:
		ret = -EINVAL;
		break;
	}

	dev_dbg_ratelimited(tia_adc_dev->dev, "[TIA_TEST_1][CH:%d] tia_auxadc_reg = %p, r_type(16:17)=%d\n",
			chan->channel, auxadc_chans[chan->channel].tia_auxadc_reg, r_type);
	dev_dbg_ratelimited(tia_adc_dev->dev, "[TIA_TEST_2][CH:%d] r_type=%d, raw=%d/%d, v_in=%llu/%llu\n",
			chan->channel, r_type, temp_auxadc, auxadc_out, adc2volt(temp_auxadc), adc2volt(auxadc_out));

	mutex_unlock(&tia_adc_dev->lock);

	if (ret < 0)
		goto err;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		*val = adc2volt(auxadc_out);
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_RAW:
		*val = auxadc_out;
		ret = IIO_VAL_INT;
		break;
	default:
		return -EINVAL;
	}
err:
	dev_dbg_ratelimited(tia_adc_dev->dev, "[TIA_TEST][CH:%d] : r_type=%d, temp_raw=%d, con_raw=%d, v_in=%llu\n",
		chan->channel, r_type, temp_auxadc, auxadc_out, adc2volt(auxadc_out));
	return ret;
}

static const struct iio_info pmic_adc_info = {
	.read_raw = &pmic_adc_read_raw,
};

static int auxadc_get_data_from_dt(struct platform_device *pdev, struct pmic_tia_adc_device *tia_adc_dev,
				   struct iio_chan_spec *iio_chan,
				   struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int channel = 0;
	unsigned int value = 0;
	int ret;

	ret = of_property_read_u32(node, "channel", &channel);
	if (ret) {
		dev_info(tia_adc_dev->dev, "invalid channel in node:%s\n",
			   node->name);
		return ret;
	}

	if (channel >= TIA_AUXADC_CHAN_MAX) {
		dev_info(tia_adc_dev->dev, "invalid channel number %d in node:%s\n",
			   channel, node->name);
		return -EINVAL;
	}

	if (channel >= ARRAY_SIZE(auxadc_chans)) {
		dev_info(tia_adc_dev->dev, "channel number %d in node:%s not exists\n",
			   channel, node->name);
		return -EINVAL;
	}
	iio_chan->channel = channel;
	iio_chan->datasheet_name = auxadc_chans[channel].ch_name;
	iio_chan->info_mask_separate = auxadc_chans[channel].info_mask;
	iio_chan->type = auxadc_chans[channel].type;
	iio_chan->extend_name = node->name;

	auxadc_chan = &auxadc_chans[channel];
	ret = of_property_read_u32(node, "avg-num", &value);
	if (!ret)
		auxadc_chan->avg_num = value;
	else
		auxadc_chan->avg_num = AUXADC_DEF_AVG_NUM;

	return 0;
}

static int auxadc_remap_dt(struct platform_device *pdev,
						struct auxadc_channels *auxadc_chan,
						unsigned int index)
{
	struct resource *res;
	char str[20];
	int ret;

	ret = sprintf(str, "auxadc_tia_t%d", index);
	if (ret < 0) {
		dev_info(&pdev->dev, "%s, sprintf error\n", __func__);
		return ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, str);
	if (unlikely(!res)) {
		dev_info(&pdev->dev, "%s fail, str =%s\n", __func__, str);
		return -EINVAL;
	}

	auxadc_chan->tia_auxadc_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(auxadc_chan->tia_auxadc_reg)) {
		dev_info(&pdev->dev, "%s fail, chan =%d\n", __func__, index);
		return -ENOMEM;
	}
	dev_info(&pdev->dev, "%s, tia_auxadc_reg[chan:%d]=0x%p\n", __func__, index, auxadc_chan->tia_auxadc_reg);

	return 0;
}

static int auxadc_parse_dt(struct platform_device *pdev,
			   struct pmic_tia_adc_device *tia_adc_dev,
			   struct device_node *node)
{
	struct iio_chan_spec *iio_chan;
	struct device_node *child;
	unsigned int index = 0;
	int ret;

	tia_adc_dev->nchannels = of_get_available_child_count(node);
	if (!tia_adc_dev->nchannels)
		return -EINVAL;

	tia_adc_dev->iio_chans = devm_kcalloc(tia_adc_dev->dev, tia_adc_dev->nchannels,
		sizeof(*tia_adc_dev->iio_chans), GFP_KERNEL);
	if (!tia_adc_dev->iio_chans)
		return -ENOMEM;
	iio_chan = tia_adc_dev->iio_chans;

	for_each_available_child_of_node(node, child) {
		ret = auxadc_get_data_from_dt(pdev, tia_adc_dev, iio_chan, child);
		if (ret < 0) {
			of_node_put(child);
			return ret;
		}
		ret = auxadc_remap_dt(pdev, &auxadc_chans[index], index);
		if (ret < 0) {
			of_node_put(child);
			return ret;
		}
		iio_chan->indexed = 1;
		iio_chan->address = index++;
		iio_chan++;
	}

	return 0;
}

static int tia_adc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct pmic_tia_adc_device *tia_adc_dev;
	struct iio_dev *indio_dev;
	int ret;

	dev_info(&pdev->dev, "tia probe start\n");

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*tia_adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	tia_adc_dev = iio_priv(indio_dev);
	tia_adc_dev->dev = &pdev->dev;
	tia_adc_dev->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	mutex_init(&tia_adc_dev->lock);
	tia_adc_dev->info = of_device_get_match_data(&pdev->dev);

	ret = auxadc_parse_dt(pdev, tia_adc_dev, node);
	if (ret) {
		dev_info(&pdev->dev, "auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &pmic_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = tia_adc_dev->iio_chans;
	indio_dev->num_channels = tia_adc_dev->nchannels;

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret < 0) {
		dev_info(&pdev->dev, "failed to register iio device!\n");
		return ret;
	}

	dev_info(&pdev->dev, "tia probe done\n");

	return 0;
}

static const struct of_device_id tia_adc_of_match[] = {
	{ .compatible = "mediatek,tia-auxadc", },
	{ }
};
MODULE_DEVICE_TABLE(of, tia_adc_of_match);

static struct platform_driver tia_adc_driver = {
	.driver = {
		.name = "mtk-spmi-tia-adc",
		.of_match_table = tia_adc_of_match,
	},
	.probe	= tia_adc_probe,
};
module_platform_driver(tia_adc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JOAN CHO <Joan.cho@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SPMI PMIC TIA ADC Driver");
