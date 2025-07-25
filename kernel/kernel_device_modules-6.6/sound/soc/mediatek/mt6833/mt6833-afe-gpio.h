/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6833-afe-gpio.h  --  Mediatek 6833 afe gpio ctrl definition
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yujie Xiao <yujie.xiao@mediatek.com>
 */

#ifndef _MT6833_AFE_GPIO_H_
#define _MT6833_AFE_GPIO_H_

enum mt6833_afe_gpio {
	MT6833_AFE_GPIO_DAT_MISO0_OFF,
	MT6833_AFE_GPIO_DAT_MISO0_ON,
	MT6833_AFE_GPIO_DAT_MISO1_OFF,
	MT6833_AFE_GPIO_DAT_MISO1_ON,
	MT6833_AFE_GPIO_DAT_MISO2_OFF,
	MT6833_AFE_GPIO_DAT_MISO2_ON,
	MT6833_AFE_GPIO_DAT_MOSI_OFF,
	MT6833_AFE_GPIO_DAT_MOSI_ON,
	MT6833_AFE_GPIO_I2S0_OFF,
	MT6833_AFE_GPIO_I2S0_ON,
	MT6833_AFE_GPIO_I2S1_OFF,
	MT6833_AFE_GPIO_I2S1_ON,
	MT6833_AFE_GPIO_I2S2_OFF,
	MT6833_AFE_GPIO_I2S2_ON,
	MT6833_AFE_GPIO_I2S3_OFF,
	MT6833_AFE_GPIO_I2S3_ON,
	MT6833_AFE_GPIO_I2S5_OFF,
	MT6833_AFE_GPIO_I2S5_ON,
	MT6833_AFE_GPIO_VOW_DAT_OFF,
	MT6833_AFE_GPIO_VOW_DAT_ON,
	MT6833_AFE_GPIO_VOW_CLK_OFF,
	MT6833_AFE_GPIO_VOW_CLK_ON,
	/* S96901AA5-740, wangdawen.wt, add, 2024/09/10, add HAC func */
	GPIO_AUD_HAC_HIGH,
	GPIO_AUD_HAC_LOW,
	MT6833_AFE_GPIO_GPIO_NUM
};

struct mtk_base_afe;

int mt6833_afe_gpio_init(struct mtk_base_afe *afe);
/* +S96901AA5-740, wangdawen.wt, add, 2024/09/10, add HAC func */
int mt6833_afe_gpio_hac_Select(int mode);
/* -S96901AA5-740, wangdawen.wt, add, 2024/09/10, add HAC func */
int mt6833_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink);
bool mt6833_afe_gpio_is_prepared(enum mt6833_afe_gpio type);

#endif
