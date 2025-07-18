// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_16K 0x4000
#if defined(__CONFIG_WING_CAM_CAL__)
extern unsigned int Common_read_region_bywing(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size);
#endif

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	#if defined(__CONFIG_WING_CAM_CAL__)
	{W2S5KJN1REARTRULY_SENSOR_ID, 0xA0, Common_read_region_bywing ,12679},
	{W2S5KJN1REARST_SENSOR_ID, 0xA0, Common_read_region_bywing ,12679},
	{W2HI5022QREARTXD_SENSOR_ID, 0xA0, Common_read_region_bywing ,12679},
	{W2SC1300MCSFRONTST_SENSOR_ID, 0xA0, Common_read_region_bywing},
	{W2SC1300MCSFRONTTXD_SENSOR_ID, 0xA0, Common_read_region_bywing},
	{W2HI1336FRONTTRULY_SENSOR_ID, 0x40, Common_read_region_bywing},
	{W2SC202CSMICROLH_SENSOR_ID, 0xA4, Common_read_region_bywing},
	{W2GC02M1MICROCXT_SENSOR_ID, 0xA4, Common_read_region_bywing},
	{W2BF2253LMICROSJ_SENSOR_ID, 0xA4, Common_read_region_bywing},
	#endif
	#if 0
	{HI1339_SENSOR_ID, 0xB0, Common_read_region},
	{OV13B10LZ_SENSOR_ID, 0xB0, Common_read_region},
	{GC5035_SENSOR_ID,  0x7E, Common_read_region},
	{HI1339SUBOFILM_SENSOR_ID, 0xA2, Common_read_region},
	{HI1339SUBTXD_SENSOR_ID, 0xA2, Common_read_region},
	{OV48B_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX766_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{IMX766DUAL_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC8054_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID, 0xA0, Common_read_region},
	{IMX481_SENSOR_ID, 0xA2, Common_read_region},
	{GC02M0_SENSOR_ID, 0xA8, Common_read_region},
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{IMX319_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3M5SX_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX686_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KGD1SP_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	/*  ADD before this line */
	#endif
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


