/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
/*S96818AA3-712,liuzhaofei@wingtech.com,modify,20240924,add the thermal NTC;*/
#define APPLY_PRECISE_BTS_TEMP
#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
/*S96818AA3-712,liuzhaofei@wingtech.com,modify,20240924,add the thermal NTC;*/
#define AUX_IN3_NTC (2)
#define AUX_IN4_NTC (3)

#define BTS_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTS_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define BTSMDPA_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */

/*S96818AA3-712,liuzhaofei@wingtech.com,modify,20240920,add the thermal NTC;*/
#define BTSCHARGER_RAP_PULL_UP_R		390000	/* 100K,pull up resister */
#define BTSCHARGER_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSCHARGER_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSCHARGER_RAP_NTC_TABLE		7

#define BTSCHARGER_RAP_ADC_CHANNEL		AUX_IN3_NTC

#define BTSFLASH_RAP_PULL_UP_R		390000	/* 100K,pull up resister */
#define BTSFLASH_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSFLASH_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSFLASH_RAP_NTC_TABLE		7

#define BTSFLASH_RAP_ADC_CHANNEL		AUX_IN4_NTC
/*S96818AA3-712,liuzhaofei@wingtech.com,modify,20240920,add the thermal NTC;*/
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
