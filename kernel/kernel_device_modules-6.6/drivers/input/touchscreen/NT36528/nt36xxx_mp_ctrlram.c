/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 60119 $
 * $Date: 2020-04-10 16:07:02 +0800 (週五, 10 四月 2020) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "nt36xxx.h"
#include "nt36xxx_mp_ctrlram.h"

#if NVT_TOUCH_MP

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define MP_MODE_CC 0x41
#define FREQ_HOP_DISABLE 0x66
#define FREQ_HOP_ENABLE 0x65


#define SHORT_TEST_CSV_FILE "/data/vendor/fac_sources/ShortTest.csv"
#define OPEN_TEST_CSV_FILE "/data/vendor/fac_sources/OpenTest.csv"
#define FW_RAWDATA_CSV_FILE "/data/vendor/fac_sources/FWMutualTest.csv"
#define FW_CC_CSV_FILE "/data/vendor/fac_sources/FWCCTest.csv"
#define NOISE_TEST_CSV_FILE "/data/vendor/fac_sources/NoiseTest.csv"
#if LPWG_TEST
#define LPWG_RAWDATA_CSV_FILE "/data/local/tmp/LPWGRawdataTest.csv"
#define LPWG_NOISE_CSV_FILE "/data/local/tmp/LPWGNoiseTest.csv"
#endif
#if (FDM_TEST && LPWG_TEST)
#define FDM_FW_RAWDATA_CSV_FILE "/data/local/tmp/FWFDMMutualTest.csv"
#define FDM_NOISE_TEST_CSV_FILE "/data/local/tmp/FDMNoiseTest.csv"
#endif

#define nvt_mp_seq_printf(m, fmt, args...) do {	\
	seq_printf(m, fmt, ##args);	\
	if (!nvt_mp_test_result_printed)	\
		printk(fmt, ##args);	\
} while (0)

#define TOUCHSCREEN_FOLDER                   "touchscreen"
#define CTP_OPENSHORT_TEST_FILE            "ctp_openshort_test"
static uint8_t *RecordResult_Short = NULL;
static uint8_t *RecordResult_Open = NULL;
static uint8_t *RecordResult_FWMutual = NULL;
static uint8_t *RecordResult_FW_CC = NULL;
static uint8_t *RecordResult_FW_DiffMax = NULL;
static uint8_t *RecordResult_FW_DiffMin = NULL;

#if (FDM_TEST && LPWG_TEST)
static uint8_t *RecordResult_FDM_FWRawdata = NULL;
static uint8_t *RecordResult_FDM_FW_DiffMax = NULL;
static uint8_t *RecordResult_FDM_FW_DiffMin = NULL;
#endif

static int32_t TestResult_Short = 0;
static int32_t TestResult_Open = 0;
static int32_t TestResult_FW_Rawdata = 0;
static int32_t TestResult_FWMutual = 0;
static int32_t TestResult_FW_CC = 0;
static int32_t TestResult_Noise = 0;
static int32_t TestResult_FW_DiffMax = 0;
static int32_t TestResult_FW_DiffMin = 0;

#if (FDM_TEST && LPWG_TEST)
static int32_t TestResult_FDM_FWRawdata = 0;
static int32_t TestResult_FDM_Noise = 0;
static int32_t TestResult_FDM_FW_DiffMax = 0;
static int32_t TestResult_FDM_FW_DiffMin = 0;
#endif

static int32_t *RawData_Short = NULL;
static int32_t *RawData_Open = NULL;
static int32_t *RawData_Diff = NULL;
static int32_t *RawData_Diff_Min = NULL;
static int32_t *RawData_Diff_Max = NULL;
static int32_t *RawData_FWMutual = NULL;
static int32_t *RawData_FW_CC = NULL;

#if (FDM_TEST && LPWG_TEST)
static int32_t *RawData_FDM_Diff = NULL;
static int32_t *RawData_FDM_Diff_Min = NULL;
static int32_t *RawData_FDM_Diff_Max = NULL;
static int32_t *RawData_FDM_FWRawdata = NULL;
#endif

//static struct proc_dir_entry *NVT_proc_selftest_entry = NULL;
static struct proc_dir_entry *ctp_device_proc = NULL;
static struct proc_dir_entry *openshort_proc = NULL;
#if LPWG_TEST
static struct proc_dir_entry *NVT_proc_lpwg_selftest_entry = NULL;
#endif
static int8_t nvt_mp_test_result_printed = 0;
static uint8_t fw_ver = 0;

extern void nvt_change_mode(uint8_t mode);
extern uint8_t nvt_get_fw_pipe(void);
extern void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr);
extern void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num);
int32_t nvt_mp_parse_dt(struct device_node *root, const char *node_compatible);
#if LPWG_TEST
int32_t nvt_mp_parse_lpwg_dt(struct device_node *root, const char *node_compatible);
#endif

/*******************************************************
Description:
	Novatek touchscreen allocate buffer for mp selftest.

return:
	Executive outcomes. 0---succeed. -12---Out of memory
*******************************************************/
static int nvt_mp_buffer_init(void)
{
	size_t RecordResult_BufSize = IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE;
	size_t RawData_BufSize = (IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * sizeof(int32_t);

#if (FDM_TEST && LPWG_TEST)
		size_t RecordResult_FDM_BufSize = (IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * FDM_X_Channel;
		size_t RawData_FDM_BufSize =  (IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * FDM_X_Channel * sizeof(int32_t);
#endif

	RecordResult_Short = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Short) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_Short failed!\n", __func__);
		return -ENOMEM;
	}

	RecordResult_Open = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Open) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_Open failed!\n", __func__);
		return -ENOMEM;
	}

	RecordResult_FWMutual = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FWMutual) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FWMutual failed!\n", __func__);
		return -ENOMEM;
	}

	RecordResult_FW_CC = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_CC) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FW_CC failed!\n", __func__);
		return -ENOMEM;
	}

	RecordResult_FW_DiffMax = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMax) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FW_DiffMax failed!\n", __func__);
		return -ENOMEM;
	}

	RecordResult_FW_DiffMin = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMin) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FW_DiffMin failed!\n", __func__);
		return -ENOMEM;
	}

#if (FDM_TEST && LPWG_TEST)
		RecordResult_FDM_FWRawdata = (uint8_t *)kzalloc(RecordResult_FDM_BufSize, GFP_KERNEL);
		if (!RecordResult_FDM_FWRawdata) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FDM_FWRawdata failed!\n", __func__);
			return -ENOMEM;
		}

		RecordResult_FDM_FW_DiffMax = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_FDM_FW_DiffMax) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FDM_FW_DiffMax failed!\n", __func__);
			return -ENOMEM;
		}

		RecordResult_FDM_FW_DiffMin = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_FDM_FW_DiffMin) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RecordResult_FDM_FW_DiffMin failed!\n", __func__);
			return -ENOMEM;
		}
#endif

	RawData_Short = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Short) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_Short failed!\n", __func__);
		return -ENOMEM;
	}

	RawData_Open = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Open) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_Open failed!\n", __func__);
		return -ENOMEM;
	}

	RawData_Diff = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_Diff failed!\n", __func__);
		return -ENOMEM;
	}

	RawData_Diff_Min = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Min) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_Diff_Min failed!\n", __func__);
		return -ENOMEM;
	}

	RawData_Diff_Max = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Max) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_Diff_Max failed!\n", __func__);
		return -ENOMEM;
	}

	RawData_FWMutual = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FWMutual) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_FWMutual failed!\n", __func__);
		return -ENOMEM;
	}

	RawData_FW_CC = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FW_CC) {
		input_err(true, &ts->client->dev, "%s: kzalloc for RawData_FW_CC failed!\n", __func__);
		return -ENOMEM;
	}

#if (FDM_TEST && LPWG_TEST)
		RawData_FDM_Diff = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
		if (!RawData_FDM_Diff) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RawData_FDM_Diff failed!\n", __func__);
			return -ENOMEM;
		}

		RawData_FDM_Diff_Min = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
		if (!RawData_FDM_Diff_Min) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RawData_FDM_Diff_Min failed!\n", __func__);
			return -ENOMEM;
		}

		RawData_FDM_Diff_Max = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
		if (!RawData_FDM_Diff_Max) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RawData_FDM_Diff_Max failed!\n", __func__);
			return -ENOMEM;
		}

		RawData_FDM_FWRawdata = (int32_t *)kzalloc(RawData_FDM_BufSize, GFP_KERNEL);
		if (!RawData_FDM_FWRawdata) {
			input_err(true, &ts->client->dev, "%s: kzalloc for RawData_FDM_FWRawdata failed!\n", __func__);
			return -ENOMEM;
		}
#endif
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen free buffer for mp selftest.

return:
	n.a.
*******************************************************/
static void nvt_mp_buffer_deinit(void)
{
	if (RecordResult_Short) {
		kfree(RecordResult_Short);
		RecordResult_Short = NULL;
	}

	if (RecordResult_Open) {
		kfree(RecordResult_Open);
		RecordResult_Open = NULL;
	}

	if (RecordResult_FWMutual) {
		kfree(RecordResult_FWMutual);
		RecordResult_FWMutual = NULL;
	}

	if (RecordResult_FW_CC) {
		kfree(RecordResult_FW_CC);
		RecordResult_FW_CC = NULL;
	}

	if (RecordResult_FW_DiffMax) {
		kfree(RecordResult_FW_DiffMax);
		RecordResult_FW_DiffMax = NULL;
	}

	if (RecordResult_FW_DiffMin) {
		kfree(RecordResult_FW_DiffMin);
		RecordResult_FW_DiffMin = NULL;
	}

	if (RawData_Short) {
		kfree(RawData_Short);
		RawData_Short = NULL;
	}

	if (RawData_Open) {
		kfree(RawData_Open);
		RawData_Open = NULL;
	}

	if (RawData_Diff) {
		kfree(RawData_Diff);
		RawData_Diff = NULL;
	}

	if (RawData_Diff_Min) {
		kfree(RawData_Diff_Min);
		RawData_Diff_Min = NULL;
	}

	if (RawData_Diff_Max) {
		kfree(RawData_Diff_Max);
		RawData_Diff_Max = NULL;
	}

	if (RawData_FWMutual) {
		kfree(RawData_FWMutual);
		RawData_FWMutual = NULL;
	}

	if (RawData_FW_CC) {
		kfree(RawData_FW_CC);
		RawData_FW_CC = NULL;
	}
}

static void nvt_print_data_log_in_one_line(int32_t *data, int32_t data_num)
{
	char *tmp_log = NULL;
	int32_t i = 0;

	tmp_log = (char *)kzalloc(data_num * 7 + 1, GFP_KERNEL);
	if (!tmp_log) {
		input_err(true, &ts->client->dev, "%s: kzalloc for tmp_log failed!\n", __func__);
		return;
	}

	for (i = 0; i < data_num; i++) {
		sprintf(tmp_log + i * 7, "%5d, ", data[i]);
	}
	tmp_log[data_num * 7] = '\0';
	printk("[sec_input] %s", tmp_log);
	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}

static void nvt_print_result_log_in_one_line(uint8_t *result, int32_t result_num)
{
	char *tmp_log = NULL;
	int32_t i = 0;

	tmp_log = (char *)kzalloc(result_num * 6 + 1, GFP_KERNEL);
	if (!tmp_log) {
		input_err(true, &ts->client->dev, "%s: kzalloc for tmp_log failed!\n", __func__);
		return;
	}

	for (i = 0; i < result_num; i++) {
		sprintf(tmp_log + i * 6, "0x%02X, ", result[i]);
	}
	tmp_log[result_num * 6] = '\0';
	printk("[sec_input] %s", tmp_log);
	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}

/*******************************************************
Description:
	Novatek touchscreen self-test criteria print function.

return:
	n.a.
*******************************************************/
static void nvt_print_lmt_array(int32_t *array, int32_t x_ch, int32_t y_ch)
{
	int32_t j = 0;

	for (j = 0; j < y_ch; j++) {
		nvt_print_data_log_in_one_line(array + j * x_ch, x_ch);
		printk("\n");
	}
#if TOUCH_KEY_NUM > 0
	nvt_print_data_log_in_one_line(array + y_ch * x_ch, Key_Channel);
	printk("\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
}

static void nvt_print_criteria(void)
{
	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---PS_Config_Lmt_Short_Rawdata---
	printk("PS_Config_Lmt_Short_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Short_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_Short_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Short_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_Open_Rawdata---
	printk("PS_Config_Lmt_Open_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_Open_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_Rawdata---
	printk("PS_Config_Lmt_FW_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_CC---
	printk("PS_Config_Lmt_FW_CC_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_CC_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_CC_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_CC_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_Diff---
	printk("PS_Config_Lmt_FW_Diff_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Diff_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_Diff_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Diff_N, X_Channel, Y_Channel);

#if (FDM_TEST && LPWG_TEST)
	//---PS_Config_Lmt_FDM_Rawdata---
	printk("PS_Config_Lmt_FDM_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FDM_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FDM_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FDM_Rawdata_N, X_Channel, Y_Channel);
	//---PS_Config_Lmt_FDM_Diff---
	printk("PS_Config_Lmt_FDM_Diff_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FDM_Diff_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FDM_Diff_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FDM_Diff_N, X_Channel, Y_Channel);
#endif

#if LPWG_TEST
	//---PS_Config_Lmt_LPWG_Rawdata---
	printk("PS_Config_Lmt_LPWG_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_LPWG_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_LPWG_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_LPWG_Rawdata_N, X_Channel, Y_Channel);
	//---PS_Config_Lmt_LPWG_Diff---
	printk("PS_Config_Lmt_LPWG_Diff_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_LPWG_Diff_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_LPWG_Diff_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_LPWG_Diff_N, X_Channel, Y_Channel);
#endif

	input_info(true, &ts->client->dev, "%s: --\n", __func__);
}

static int32_t nvt_save_rawdata_to_csv(int32_t *rawdata, uint8_t x_ch, uint8_t y_ch, const char *file_path, uint32_t offset)
{	
#if 0
	int32_t x = 0;
	int32_t y = 0;
	int32_t iArrayIndex = 0;
	struct file *fp = NULL;
	char *fbufp = NULL;
#ifdef HAVE_VFS_WRITE
	mm_segment_t org_fs;
#endif
	int32_t write_ret = 0;
	uint32_t output_len = 0;
	loff_t pos = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
	int32_t keydata_output_offset = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);
	fbufp = (char *)kzalloc(8192, GFP_KERNEL);
	if (!fbufp) {
		input_err(true, &ts->client->dev, "%s: kzalloc for fbufp failed!\n", __func__);
		return -ENOMEM;
	}

	for (y = 0; y < y_ch; y++) {
		for (x = 0; x < x_ch; x++) {
			iArrayIndex = y * x_ch + x;
			sprintf(fbufp + iArrayIndex * 7 + y * 2, "%5d, ", rawdata[iArrayIndex]);
		}
		nvt_print_data_log_in_one_line(rawdata + y * x_ch, x_ch);
		printk("\n");
		sprintf(fbufp + (iArrayIndex + 1) * 7 + y * 2,"\r\n");
	}
#if TOUCH_KEY_NUM > 0
	keydata_output_offset = y_ch * x_ch * 7 + y_ch * 2;
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		sprintf(fbufp + keydata_output_offset + k * 7, "%5d, ", rawdata[iArrayIndex]);
	}
	nvt_print_data_log_in_one_line(rawdata + y_ch * x_ch, Key_Channel);
	printk("\n");
	sprintf(fbufp + y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7, "\r\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
	
	//org_fs = get_fs();
	//set_fs(KERNEL_DS);
	fp = filp_open(file_path, O_RDWR | O_CREAT, 0644);
	if (fp == NULL || IS_ERR(fp)) {
		input_err(true, &ts->client->dev, "%s: open %s failed\n", __func__, file_path);
		//set_fs(org_fs);
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

#if TOUCH_KEY_NUM > 0
	output_len = y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7 + 2;
#else
	output_len = y_ch * x_ch * 7 + y_ch * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	pos = offset;
#ifdef HAVE_VFS_WRITE
	org_fs = get_fs();
	set_fs(KERNEL_DS);
	write_ret = vfs_write(fp, (char __user *)fbufp, output_len, &pos);
	set_fs(org_fs);
#else
	write_ret = kernel_write(fp, fbufp, output_len, &pos);
#endif /* #ifdef HAVE_VFS_WRITE */
	if (write_ret <= 0) {
		input_err(true, &ts->client->dev, "%s: write %s failed\n", __func__, file_path);
		if (fp) {
			filp_close(fp, NULL);
			fp = NULL;
		}
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}
	if (fbufp) {
		kfree(fbufp);
		fbufp = NULL;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);
#endif
	return 0;
}

static int32_t nvt_polling_hand_shake_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 250;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] == 0xA0) || (buf[1] == 0xA1))
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "polling hand shake status failed, buf[1]=0x%02X\n", buf[1]);

		// Read back 5 bytes from offset EVENT_MAP_HOST_CMD for debug check
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);
		input_err(true, &ts->client->dev, "Read back 5 bytes from offset EVENT_MAP_HOST_CMD: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buf[1], buf[2], buf[3], buf[4], buf[5]);

		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);		
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);

		return -1;
	} else {
		return 0;
	}
}

static int8_t nvt_switch_FreqHopEnDis(uint8_t FreqHopEnDis)
{
	uint8_t buf[8] = {0};
	uint8_t retry = 0;
	int8_t ret = 0;

	input_info(true, &ts->client->dev, "%s  ++\n", __func__);

	for (retry = 0; retry < 20; retry++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		//---switch FreqHopEnDis---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = FreqHopEnDis;
		CTP_SPI_WRITE(ts->client, buf, 2);

		msleep(35);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(retry == 20)) {
		input_err(true, &ts->client->dev, "switch FreqHopEnDis 0x%02X failed, buf[1]=0x%02X\n", FreqHopEnDis, buf[1]);
		ret = -1;
	}

	input_info(true, &ts->client->dev, "%s  --\n", __func__);

	return ret;
}

static int32_t nvt_read_baseline(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("[sec_input] %s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, FW_RAWDATA_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

static int32_t nvt_read_CC(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("[sec_input] %s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, FW_CC_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

static void nvt_enable_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fw_noise(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
	uint32_t rawdata_diff_min_offset = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	frame_num = PS_Config_Diff_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	input_info(true, &ts->client->dev, "%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_noise_collect(frame_num);
	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
			RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
		RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s: RawData_Diff_Max:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Max, X_Channel, Y_Channel, NOISE_TEST_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2 + Key_Channel * 7 + 2;
#else
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	input_info(true, &ts->client->dev, "%s: RawData_Diff_Min:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Min, X_Channel, Y_Channel, NOISE_TEST_CSV_FILE, rawdata_diff_min_offset) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

#if LPWG_TEST
static int32_t nvt_read_lpwg_rawdata(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, LPWG_RAWDATA_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

static int32_t nvt_read_fw_lpwg_noise(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
	uint32_t rawdata_diff_min_offset = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	frame_num = PS_Config_Diff_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	printk("%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_noise_collect(frame_num);
	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
			RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
		RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Diff_Max:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Max, X_Channel, Y_Channel, LPWG_NOISE_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2 + Key_Channel * 7 + 2;
#else
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	printk("%s:RawData_Diff_Min:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Min, X_Channel, Y_Channel, LPWG_NOISE_CSV_FILE, rawdata_diff_min_offset) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}
#endif	/* LPWG_TEST */

#if (FDM_TEST && LPWG_TEST)
static void nvt_enable_fdm_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x4B;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fdm_fw_noise(int32_t *xdata)
{
	uint8_t buf[128] = {0};
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
	uint32_t rawdata_diff_min_offset = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	frame_num = PS_Config_FDM_Noise_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	printk("%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_fdm_noise_collect(frame_num);
	// need wait PS_Config_FDM_Noise_Test_Frame * 8.3ms
	msleep(frame_num * 83 * 2);

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	for (x = 0; x < FDM_X_Channel; x++) {
		//---change xdata index---
		nvt_set_page(ts->mmap->DIFF_PIPE0_ADDR + Y_Channel * FDM_X_Channel * x);

		//---read data---
		buf[0] = (ts->mmap->DIFF_PIPE0_ADDR + Y_Channel * FDM_X_Channel * x) & 0xFF;
		CTP_SPI_READ(ts->client, buf, Y_Channel * 2 + 1);

		for (y = 0; y < Y_Channel; y++) {
			xdata[y * FDM_X_Channel + x] = (uint16_t)(buf[y * FDM_X_Channel + 1] + 256 * buf[y * FDM_X_Channel + 2]);
		}
	}

	for (y = 0; y < Y_Channel; y++) {
		for (x = 0; x < FDM_X_Channel; x++) {
			iArrayIndex = y * FDM_X_Channel + x;
			RawData_FDM_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF) * 4;	//scaling up
			RawData_FDM_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF) * 4;
		}
	}

	//---Leave Test Mode---
	//nvt_change_mode(NORMAL_MODE);	//No return to normal mode. Continuous to get fdm rawdata

	printk("%s:RawData_FDM_Diff_Max:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_FDM_Diff_Max, FDM_X_Channel, Y_Channel,
				FDM_NOISE_TEST_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_diff_min_offset = Y_Channel * FDM_X_Channel * 7 + Y_Channel * 2 + Key_Channel * 7 + 2;
#else
	rawdata_diff_min_offset = Y_Channel * FDM_X_Channel * 7 + Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	printk("%s:RawData_FDM_Diff_Min:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_FDM_Diff_Min, 2, Y_Channel,
				FDM_NOISE_TEST_CSV_FILE, rawdata_diff_min_offset) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

static int32_t nvt_read_fdm_baseline(int32_t *xdata)
{
	uint8_t buf[128] = {0};
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---Enter Test Mode---
	//nvt_change_mode(TEST_MODE_2);

	//if (nvt_check_fw_status()) {
	//	return -EAGAIN;
	//}

	for (x = 0; x < FDM_X_Channel; x++) {
		//---set xdata index---
		nvt_set_page(ts->mmap->DOZE_GM_S1D_SCAN_RAW_ADDR + Y_Channel * FDM_X_Channel * x);

		//---read data---
		buf[0] = (ts->mmap->DOZE_GM_S1D_SCAN_RAW_ADDR + Y_Channel * FDM_X_Channel * x) & 0xFF;
		CTP_SPI_READ(ts->client, buf, Y_Channel * 2 + 1);
		for (y = 0; y < Y_Channel; y++) {
			xdata[y * 2 + x] = (uint16_t)(buf[y * 2 + 1] + 256 * buf[y * 2 + 2]);
		}
	}

	for (y = 0; y < Y_Channel; y++) {
		for (x = 0; x < FDM_X_Channel; x++) {
			iArrayIndex = y * FDM_X_Channel + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, FDM_X_Channel, Y_Channel, FDM_FW_RAWDATA_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}
#endif

static void nvt_enable_open_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable open test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x45;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static void nvt_enable_short_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x43;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fw_open(int32_t *xdata)
{
	uint32_t raw_pipe_addr = 0;
	uint8_t *rawdata_buf = NULL;
	uint32_t x = 0;
	uint32_t y = 0;
	uint8_t buf[128] = {0};
#if TOUCH_KEY_NUM > 0
	uint32_t raw_btn_pipe_addr = 0;
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_open_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_buf = (uint8_t *)kzalloc((IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * 2, GFP_KERNEL);
#else
	rawdata_buf = (uint8_t *)kzalloc(IC_X_CFG_SIZE * IC_Y_CFG_SIZE * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		input_err(true, &ts->client->dev, "%s: kzalloc for rawdata_buf failed!\n", __func__);
		return -ENOMEM;
	}

	if (nvt_get_fw_pipe() == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		//---change xdata index---
		nvt_set_page(raw_pipe_addr + y * IC_X_CFG_SIZE * 2);
		buf[0] = (uint8_t)((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) & 0xFF);
		CTP_SPI_READ(ts->client, buf, IC_X_CFG_SIZE * 2 + 1);
		memcpy(rawdata_buf + y * IC_X_CFG_SIZE * 2, buf + 1, IC_X_CFG_SIZE * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

	//---change xdata index---
	nvt_set_page(raw_btn_pipe_addr);
	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, IC_KEY_CFG_SIZE * 2 + 1);
	memcpy(rawdata_buf + IC_Y_CFG_SIZE * IC_X_CFG_SIZE * 2, buf + 1, IC_KEY_CFG_SIZE * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		for (x = 0; x < IC_X_CFG_SIZE; x++) {
			if ((AIN_Y[y] != 0xFF) && (AIN_X[x] != 0xFF)) {
				xdata[AIN_Y[y] * X_Channel + AIN_X[x]] = (int16_t)((rawdata_buf[(y * IC_X_CFG_SIZE + x) * 2] + 256 * rawdata_buf[(y * IC_X_CFG_SIZE + x) * 2 + 1]));
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF)
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] = (int16_t)(rawdata_buf[(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2] + 256 * rawdata_buf[(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (rawdata_buf) {
		kfree(rawdata_buf);
		rawdata_buf = NULL;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);


	input_info(true, &ts->client->dev, "%s: RawData_Open\n", __func__);
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, OPEN_TEST_CSV_FILE, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: save rawdata to CSV file failed\n", __func__);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

static int32_t nvt_read_fw_short(int32_t *xdata)
{
	uint32_t raw_pipe_addr = 0;
	uint8_t *rawdata_buf = NULL;
	uint32_t x = 0;
	uint32_t y = 0;
	uint8_t buf[128] = {0};
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	uint32_t raw_btn_pipe_addr = 0;
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_short_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_buf = (uint8_t *)kzalloc((X_Channel * Y_Channel + Key_Channel) * 2, GFP_KERNEL);
#else
	rawdata_buf = (uint8_t *)kzalloc(X_Channel * Y_Channel * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		input_err(true, &ts->client->dev, "%s: kzalloc for rawdata_buf failed!\n", __func__);
		return -ENOMEM;
	}

	if (nvt_get_fw_pipe() == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < Y_Channel; y++) {
		//---change xdata index---
		nvt_set_page(raw_pipe_addr + y * X_Channel * 2);
		buf[0] = (uint8_t)((raw_pipe_addr + y * X_Channel * 2) & 0xFF);
		CTP_SPI_READ(ts->client, buf, X_Channel * 2 + 1);
		memcpy(rawdata_buf + y * X_Channel * 2, buf + 1, X_Channel * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

	//---change xdata index---
	nvt_set_page(raw_btn_pipe_addr);
	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, Key_Channel * 2 + 1);
	memcpy(rawdata_buf + Y_Channel * X_Channel * 2, buf + 1, Key_Channel * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < Y_Channel; y++) {
		for (x = 0; x < X_Channel; x++) {
			iArrayIndex = y * X_Channel + x;
			xdata[iArrayIndex] = (int16_t)(rawdata_buf[iArrayIndex * 2] + 256 * rawdata_buf[iArrayIndex * 2 + 1]);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)(rawdata_buf[iArrayIndex * 2] + 256 * rawdata_buf[iArrayIndex * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (rawdata_buf) {
		kfree(rawdata_buf);
		rawdata_buf = NULL;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s: RawData_Short\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, SHORT_TEST_CSV_FILE, 0) < 0) {
		pr_err("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen raw data test for each single point function.

return:
	Executive outcomes. 0---passed. negative---failed.
*******************************************************/
static int32_t RawDataTest_SinglePoint_Sub(int32_t rawdata[], uint8_t RecordResult[], uint8_t x_ch, uint8_t y_ch, int32_t Rawdata_Limit_Postive[], int32_t Rawdata_Limit_Negative[])
{
	int32_t i = 0;
	int32_t j = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */
	int32_t iArrayIndex = 0;
	bool isPass = true;

	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			iArrayIndex = j * x_ch + i;

			RecordResult[iArrayIndex] = 0x00; // default value for PASS

			if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x01;

			if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x02;
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;

		RecordResult[iArrayIndex] = 0x00; // default value for PASS

		if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x01;

		if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x02;
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Check RecordResult---
	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			if (RecordResult[j * x_ch + i] != 0) {
				isPass = false;
				break;
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		if (RecordResult[iArrayIndex] != 0) {
			isPass = false;
			break;
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (isPass == false) {
		return -1; // FAIL
	} else {
		return 0; // PASS
	}
}

/*******************************************************
Description:
	Novatek touchscreen print self-test result function.

return:
	n.a.
*******************************************************/
void print_selftest_result(struct seq_file *m, int32_t TestResult, uint8_t RecordResult[], int32_t rawdata[], uint8_t x_len, uint8_t y_len)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	switch (TestResult) {
		case 0:
			nvt_mp_seq_printf(m, " PASS!\n");
			break;

		case 1:
			nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
			break;

		case -1:
			nvt_mp_seq_printf(m, " FAIL!\n");
			nvt_mp_seq_printf(m, "RecordResult:\n");
			for (i = 0; i < y_len; i++) {
				for (j = 0; j < x_len; j++) {
					iArrayIndex = i * x_len + j;
					seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
				}
				if (!nvt_mp_test_result_printed)
					nvt_print_result_log_in_one_line(RecordResult + i * x_len, x_len);
				nvt_mp_seq_printf(m, "\n");
			}
#if TOUCH_KEY_NUM > 0
			for (k = 0; k < Key_Channel; k++) {
				iArrayIndex = y_len * x_len + k;
				seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
			}
			if (!nvt_mp_test_result_printed)
				nvt_print_result_log_in_one_line(RecordResult + y_len * x_len, Key_Channel);
			nvt_mp_seq_printf(m, "\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
			nvt_mp_seq_printf(m, "ReadData:\n");
			for (i = 0; i < y_len; i++) {
				for (j = 0; j < x_len; j++) {
					iArrayIndex = i * x_len + j;
					seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
				}
				if (!nvt_mp_test_result_printed)
					nvt_print_data_log_in_one_line(rawdata + i * x_len, x_len);
				nvt_mp_seq_printf(m, "\n");
			}
#if TOUCH_KEY_NUM > 0
			for (k = 0; k < Key_Channel; k++) {
				iArrayIndex = y_len * x_len + k;
				seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
			}
			if (!nvt_mp_test_result_printed)
				nvt_print_data_log_in_one_line(rawdata + y_len * x_len, Key_Channel);
			nvt_mp_seq_printf(m, "\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
			break;
	}
	nvt_mp_seq_printf(m, "\n");
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show_selftest(struct seq_file *m, void *v)
{
	if((TestResult_FW_Rawdata == 0 ) && (TestResult_Noise == 0) && (TestResult_Short == 0) && (TestResult_Open == 0)){
		 nvt_mp_seq_printf(m, "%s\n", "result=1"); 
		 input_info(true, &ts->client->dev, "%s: result=1\n", __func__);
		 return 0;
	}else{
		nvt_mp_seq_printf(m, "%s\n", "result=0");
		printk("nvt Test FAIL:FW_Rawdata=%d,Noise=%d,Short=%d,Open=%d\n",TestResult_FW_Rawdata,TestResult_Noise,TestResult_Short,TestResult_Open);
		input_info(true, &ts->client->dev, "%s: result=1\n", __func__);
	}
	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	nvt_mp_seq_printf(m, "FW Version: %d\n\n", fw_ver);

	nvt_mp_seq_printf(m, "Short Test");
	print_selftest_result(m, TestResult_Short, RecordResult_Short, RawData_Short, X_Channel, Y_Channel);

	nvt_mp_seq_printf(m, "Open Test");
	print_selftest_result(m, TestResult_Open, RecordResult_Open, RawData_Open, X_Channel, Y_Channel);

	nvt_mp_seq_printf(m, "FW Rawdata Test");
	if ((TestResult_FW_Rawdata == 0) || (TestResult_FW_Rawdata == 1)) {
		 print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual, RawData_FWMutual, X_Channel, Y_Channel);
	} else { // TestResult_FW_Rawdata is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_FWMutual == -1) {
			nvt_mp_seq_printf(m, "FW Mutual");
			print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual, RawData_FWMutual, X_Channel, Y_Channel);
		}
		if (TestResult_FW_CC == -1) {
			nvt_mp_seq_printf(m, "FW CC");
			print_selftest_result(m, TestResult_FW_CC, RecordResult_FW_CC, RawData_FW_CC, X_Channel, Y_Channel);
		}
	}

	nvt_mp_seq_printf(m, "Noise Test");
	if ((TestResult_Noise == 0) || (TestResult_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, X_Channel, Y_Channel);
	} else { // TestResult_Noise is -1
		nvt_mp_seq_printf(m, " FAIL!\n");

		if (TestResult_FW_DiffMax == -1) {
			nvt_mp_seq_printf(m, "FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, X_Channel, Y_Channel);
		}
		if (TestResult_FW_DiffMin == -1) {
			nvt_mp_seq_printf(m, "FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin, RawData_Diff_Min, X_Channel, Y_Channel);
		}
	}

	nvt_mp_test_result_printed = 1;

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_selftest
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_selftest open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_selftest_open(struct inode *inode, struct file *file)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default

	TestResult_Short = 0;
	TestResult_Open = 0;
	TestResult_FW_Rawdata = 0;
	TestResult_FWMutual = 0;
	TestResult_FW_CC = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	nvt_update_firmware(ts->platdata->firmware_name_mp);

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "get fw info failed!\n");
		return -EAGAIN;
	}

	fw_ver = ts->fw_ver;

	/* Parsing criteria from dts */
	if(of_property_read_bool(np, "novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *	 mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", ts->nvt_pid);

		if (nvt_mp_parse_dt(np, mpcriteria)) {
			//---Download Normal FW---
			nvt_update_firmware(ts->platdata->firmware_name);
			mutex_unlock(&ts->lock);
			input_err(true, &ts->client->dev, "mp parse device tree failed!\n");
			return -EINVAL;
		}
	} else {
		input_info(true, &ts->client->dev, "Not found novatek,mp-support-dt, use default setting\n");
		//---Print Test Criteria---
		nvt_print_criteria();
	}

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "switch frequency hopping disable failed!\n");
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "check fw reset state failed!\n");
		return -EAGAIN;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "clear fw status failed!\n");
		return -EAGAIN;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "check fw status failed!\n");
		return -EAGAIN;
	}

	//---FW Rawdata Test---
	if (nvt_read_baseline(RawData_FWMutual) != 0) {
		TestResult_FWMutual = 1;
	} else {
		TestResult_FWMutual = RawDataTest_SinglePoint_Sub(RawData_FWMutual, RecordResult_FWMutual, X_Channel, Y_Channel,
												PS_Config_Lmt_FW_Rawdata_P, PS_Config_Lmt_FW_Rawdata_N);
	}
	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
	} else {
		TestResult_FW_CC = RawDataTest_SinglePoint_Sub(RawData_FW_CC, RecordResult_FW_CC, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N);
	}

	if ((TestResult_FWMutual == 1) || (TestResult_FW_CC == 1)) {
		TestResult_FW_Rawdata = 1;
	} else {
		if ((TestResult_FWMutual == -1) || (TestResult_FW_CC == -1))
			TestResult_FW_Rawdata = -1;
		else
			TestResult_FW_Rawdata = 0;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Noise Test---
	if (nvt_read_fw_noise(RawData_Diff) != 0) {
		TestResult_Noise = 1;	// 1: ERROR
		TestResult_FW_DiffMax = 1;
		TestResult_FW_DiffMin = 1;
	} else {
		TestResult_FW_DiffMax = RawDataTest_SinglePoint_Sub(RawData_Diff_Max, RecordResult_FW_DiffMax, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		TestResult_FW_DiffMin = RawDataTest_SinglePoint_Sub(RawData_Diff_Min, RecordResult_FW_DiffMin, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;
	}

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short) != 0) {
		TestResult_Short = 1; // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short = RawDataTest_SinglePoint_Sub(RawData_Short, RecordResult_Short, X_Channel, Y_Channel,
										PS_Config_Lmt_Short_Rawdata_P, PS_Config_Lmt_Short_Rawdata_N);
	}

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open) != 0) {
		TestResult_Open = 1;	// 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open = RawDataTest_SinglePoint_Sub(RawData_Open, RecordResult_Open, X_Channel, Y_Channel,
											PS_Config_Lmt_Open_Rawdata_P, PS_Config_Lmt_Open_Rawdata_N);
	}

	//---Download Normal FW---
	nvt_update_firmware(ts->platdata->firmware_name);

	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	nvt_mp_test_result_printed = 0;

	return seq_open(file, &nvt_selftest_seq_ops);
}

static const struct proc_ops nvt_selftest_fops = {
	.proc_open = nvt_selftest_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

#if LPWG_TEST
/*******************************************************
Description:
	Novatek touchscreen lpwg_selftest information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
static int32_t c_show_lpwg_selftest(struct seq_file *m, void *v)
{
	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	nvt_mp_seq_printf(m, "FW Version: %d\n\n", ts->fw_ver);

	nvt_mp_seq_printf(m, "LPWG FW Rawdata Test");
	if ((TestResult_FW_Rawdata == 0) || (TestResult_FW_Rawdata == 1)) {
		print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual,
				RawData_FWMutual, X_Channel, Y_Channel);
	} else { // TestResult_FW_Rawdata is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_FWMutual == -1) {
			nvt_mp_seq_printf(m, "FW Mutual");
			print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual,
					RawData_FWMutual, X_Channel, Y_Channel);
		}
	}

	nvt_mp_seq_printf(m, "LPWG Noise Test");
	if ((TestResult_Noise == 0) || (TestResult_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax,
				RawData_Diff_Max, X_Channel, Y_Channel);
	} else { // TestResult_Noise is -1
		nvt_mp_seq_printf(m, " FAIL!\n");

		if (TestResult_FW_DiffMax == -1) {
			nvt_mp_seq_printf(m, "FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax,
					RawData_Diff_Max, X_Channel, Y_Channel);
		}
		if (TestResult_FW_DiffMin == -1) {
			nvt_mp_seq_printf(m, "FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin,
					RawData_Diff_Min, X_Channel, Y_Channel);
		}
	}

#if (FDM_TEST && LPWG_TEST)
		nvt_mp_seq_printf(m, "FDM FW Rawdata Test");
		if ((TestResult_FDM_FWRawdata == 0) || (TestResult_FDM_FWRawdata == 1)) {
			print_selftest_result(m, TestResult_FDM_FWRawdata,
					RecordResult_FDM_FWRawdata, RawData_FDM_FWRawdata,
					FDM_X_Channel, Y_Channel);
		} else { // TestResult_FDM_FWRawdata is -1
			nvt_mp_seq_printf(m, " FAIL!\n");
			if (TestResult_FDM_FWRawdata == -1) {
				nvt_mp_seq_printf(m, "FDM FW Mutual");
				print_selftest_result(m, TestResult_FDM_FWRawdata,
						RecordResult_FDM_FWRawdata, RawData_FDM_FWRawdata,
						FDM_X_Channel, Y_Channel);
			}
		}

		nvt_mp_seq_printf(m, "FDM Noise Test");
		if ((TestResult_FDM_Noise == 0) || (TestResult_FDM_Noise == 1)) {
			print_selftest_result(m, TestResult_FDM_FW_DiffMax,
					RecordResult_FDM_FW_DiffMax, RawData_FDM_Diff_Max,
					FDM_X_Channel, Y_Channel);
		} else { // TestResult_FDM_Noise is -1
			nvt_mp_seq_printf(m, " FAIL!\n");

			if (TestResult_FDM_FW_DiffMax == -1) {
				nvt_mp_seq_printf(m, "FDM FW Diff Max");
				print_selftest_result(m, TestResult_FDM_FW_DiffMax,
						RecordResult_FDM_FW_DiffMax, RawData_FDM_Diff_Max,
						FDM_X_Channel, Y_Channel);
			}
			if (TestResult_FDM_FW_DiffMin == -1) {
				nvt_mp_seq_printf(m, "FDM FW Diff Min");
				print_selftest_result(m, TestResult_FDM_FW_DiffMin,
						RecordResult_FDM_FW_DiffMin, RawData_FDM_Diff_Min,
						FDM_X_Channel, Y_Channel);
			}
		}
#endif
	nvt_mp_test_result_printed = 1;

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	return 0;
}

const struct seq_operations nvt_lpwg_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_lpwg_selftest
};
/*
static int32_t ddic_resx_gpio = 956; //878+78
static void nvt_ddic_hw_resx(void)
{
	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	gpio_set_value(ddic_resx_gpio, 1);
	mdelay(20);
	gpio_set_value(ddic_resx_gpio, 0);
	mdelay(20);
	gpio_set_value(ddic_resx_gpio, 1);
	mdelay(20);

	input_info(true, &ts->client->dev, "%s: --\n", __func__);
};
*/
/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_lpwg_selftest open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_lpwg_selftest_open(struct inode *inode, struct file *file)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default

	TestResult_FW_Rawdata = 0;
	TestResult_FWMutual = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;
#if (FDM_TEST && LPWG_TEST)
	TestResult_FDM_FWRawdata = 0;
	TestResult_FDM_Noise = 0;
	TestResult_FDM_FW_DiffMax = 0;
	TestResult_FDM_FW_DiffMin = 0;
#endif

	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

//	nvt_ddic_hw_resx();

	//---Download MP FW---
	nvt_update_firmware(ts->platdata->firmware_name_mp);

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: get fw info failed!\n", __func__);
		return -EAGAIN;
	}

	nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD, 0x13);
	msleep(100);

	/* Parsing criteria from dts */
	if(of_property_read_bool(np, "novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *	 mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", ts->nvt_pid);

		if (nvt_mp_parse_lpwg_dt(np, mpcriteria)) {
			//---Download Normal FW---
			// comment below, left host to do a complete reset
			//nvt_update_firmware(ts->platdata->firmware_name);
			mutex_unlock(&ts->lock);
			input_err(true, &ts->client->dev, "%s: mp parse device tree failed!\n", __func__);
			return -EINVAL;
		}
	} else {
		input_info(true, &ts->client->dev, "%s: Not found novatek,mp-support-dt, use default setting\n", __func__);
		//---Print Test Criteria---
		nvt_print_criteria();
	}

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: switch frequency hopping disable failed!\n", __func__);
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: check fw reset state failed!\n", __func__);
		return -EAGAIN;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: clear fw status failed!\n", __func__);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: check fw status failed!\n", __func__);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: get fw info failed!\n", __func__);
		return -EAGAIN;
	}

	//---FW Rawdata Test---
	if (nvt_read_lpwg_rawdata(RawData_FWMutual) != 0) {
		TestResult_FWMutual = 1;
	} else {
		TestResult_FWMutual = RawDataTest_SinglePoint_Sub(RawData_FWMutual, RecordResult_FWMutual,
				X_Channel, Y_Channel,
				PS_Config_Lmt_LPWG_Rawdata_P, PS_Config_Lmt_LPWG_Rawdata_N);
	}

	if (TestResult_FWMutual == 1) {
		TestResult_FW_Rawdata = 1;
	} else if (TestResult_FWMutual == -1) {
		TestResult_FW_Rawdata = -1;
	}
	else {
		TestResult_FW_Rawdata = 0;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Noise Test---
	if (nvt_read_fw_lpwg_noise(RawData_Diff) != 0) {
		TestResult_Noise = 1;	// 1: ERROR
		TestResult_FW_DiffMax = 1;
		TestResult_FW_DiffMin = 1;
	} else {
		TestResult_FW_DiffMax = RawDataTest_SinglePoint_Sub(RawData_Diff_Max, RecordResult_FW_DiffMax,
				X_Channel, Y_Channel,
				PS_Config_Lmt_LPWG_Diff_P, PS_Config_Lmt_LPWG_Diff_N);

		// for carrier sensing system, only positive noise data
		TestResult_FW_DiffMin = RawDataTest_SinglePoint_Sub(RawData_Diff_Min, RecordResult_FW_DiffMin,
				X_Channel, Y_Channel,
				PS_Config_Lmt_LPWG_Diff_P, PS_Config_Lmt_LPWG_Diff_N);

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;
	}

#if (FDM_TEST && LPWG_TEST)
		//---Noise Test---
		if (nvt_read_fdm_fw_noise(RawData_FDM_Diff) != 0) {
			TestResult_FDM_Noise = 1;	// 1: ERROR
			TestResult_FDM_FW_DiffMax = 1;
			TestResult_FDM_FW_DiffMin = 1;
		} else {
			TestResult_FDM_FW_DiffMax = RawDataTest_SinglePoint_Sub(RawData_FDM_Diff_Max, RecordResult_FDM_FW_DiffMax,
					FDM_X_Channel, Y_Channel,
					PS_Config_Lmt_FDM_Diff_P, PS_Config_Lmt_FDM_Diff_N);

			TestResult_FDM_FW_DiffMin = RawDataTest_SinglePoint_Sub(RawData_FDM_Diff_Min, RecordResult_FDM_FW_DiffMin,
					FDM_X_Channel, Y_Channel,
					PS_Config_Lmt_FDM_Diff_P, PS_Config_Lmt_FDM_Diff_N);
		}

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_FDM_Noise = -1;
		else
			TestResult_FDM_Noise = 0;

		//---FDM FW Rawdata Test---
		if(nvt_read_fdm_baseline(RawData_FDM_FWRawdata) != 0) {
			TestResult_FDM_FWRawdata = 1;
		} else {
			TestResult_FDM_FWRawdata = RawDataTest_SinglePoint_Sub(RawData_FDM_FWRawdata, RecordResult_FDM_FWRawdata,
					FDM_X_Channel, Y_Channel,
					PS_Config_Lmt_FDM_Rawdata_P, PS_Config_Lmt_FDM_Rawdata_N);
		}
#endif
	//---Download Normal FW---
	// comment below, left host to do a complete reset
//	nvt_ddic_hw_resx();
//	nvt_update_firmware(ts->platdata->firmware_name);
//	nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD, 0x13);

	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: --\n", __func__);

	nvt_mp_test_result_printed = 0;

	return seq_open(file, &nvt_lpwg_selftest_seq_ops);
}

static const struct proc_ops nvt_lpwg_selftest_fops = {
	.proc_open = nvt_lpwg_selftest_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#endif

#ifdef CONFIG_OF
/*******************************************************
Description:
	Novatek touchscreen parse AIN setting for array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_ain(struct device_node *np, const char *name, uint8_t *array, int32_t size)
{
	struct property *data;
	int32_t len, ret;
	int32_t tmp[40];
	int32_t i;

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len != size)) {
		input_err(true, &ts->client->dev, "%s: error find %s. len=%d\n", __func__, name, len);
		return -1;
	} else {
		input_info(true, &ts->client->dev, "%s: %s. len=%d\n", __func__, name, len);
		ret = of_property_read_u32_array(np, name, tmp, len);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: error reading %s. ret=%d\n", __func__, name, ret);
			return -1;
		}

		for (i = 0; i < len; i++)
			array[i] = tmp[i];

#if NVT_DEBUG
		printk("[NVT-ts] %s = ", name);
		nvt_print_result_log_in_one_line(array, len);
		printk("\n");
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for u32 type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_u32(struct device_node *np, const char *name, int32_t *para)
{
	int32_t ret;

	ret = of_property_read_u32(np, name, para);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: error reading %s. ret=%d\n", __func__, name, ret);
		return -1;
	} else {
#if NVT_DEBUG
		input_info(true, &ts->client->dev, "%s: %s=%d\n", __func__, name, *para);
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_array(struct device_node *np, const char *name, int32_t *array,
		int32_t size)
{
	struct property *data;
	int32_t len, ret;
#if NVT_DEBUG
	int32_t j = 0;
#endif

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len < size)) {
		input_err(true, &ts->client->dev, "%s: error find %s. len=%d\n", __func__, name, len);
		return -1;
	} else {
		input_info(true, &ts->client->dev, "%s: %s. len=%d\n", __func__, name, len);
		ret = of_property_read_u32_array(np, name, array, len);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: error reading %s. ret=%d\n", __func__, name, ret);
			return -1;
		}

#if NVT_DEBUG
		input_info(true, &ts->client->dev, "%s =\n", name);
		for (j = 0; j < Y_Channel; j++) {
			nvt_print_data_log_in_one_line(array + j * X_Channel, X_Channel);
			printk("\n");
		}
#if TOUCH_KEY_NUM > 0
		nvt_print_data_log_in_one_line(array + Y_Channel * X_Channel, Key_Channel);
		printk("\n");
#endif
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse device tree mp function.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_dt(struct device_node *root, const char *node_compatible)
{
	struct device_node *np = root;
	struct device_node *child = NULL;

	input_info(true, &ts->client->dev, "%s: Parse mp criteria for node %s\n", __func__, node_compatible);

	/* find each MP sub-nodes */
	for_each_child_of_node(root, child) {
		/* find the specified node */
		if (of_device_is_compatible(child, node_compatible)) {
			input_info(true, &ts->client->dev, "%s: found child node %s\n", __func__, node_compatible);
			np = child;
			break;
		}
	}
	if (child == NULL) {
		input_err(true, &ts->client->dev, "%s: Not found compatible node %s!\n", __func__, node_compatible);
		return -1;
	}

	/* MP Config*/
	if (nvt_mp_parse_u32(np, "IC_X_CFG_SIZE", &IC_X_CFG_SIZE))
		return -1;

	if (nvt_mp_parse_u32(np, "IC_Y_CFG_SIZE", &IC_Y_CFG_SIZE))
		return -1;

#if TOUCH_KEY_NUM > 0
	if (nvt_mp_parse_u32(np, "IC_KEY_CFG_SIZE", &IC_KEY_CFG_SIZE))
		return -1;
#endif

	if (nvt_mp_parse_u32(np, "X_Channel", &X_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "Y_Channel", &Y_Channel))
		return -1;

	if (nvt_mp_parse_ain(np, "AIN_X", AIN_X, IC_X_CFG_SIZE))
		return -1;

	if (nvt_mp_parse_ain(np, "AIN_Y", AIN_Y, IC_Y_CFG_SIZE))
		return -1;

#if TOUCH_KEY_NUM > 0
	if (nvt_mp_parse_ain(np, "AIN_KEY", AIN_KEY, IC_KEY_CFG_SIZE))
		return -1;
#endif

	/* MP Criteria */
	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_Rawdata_P", PS_Config_Lmt_Short_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_Rawdata_N", PS_Config_Lmt_Short_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Rawdata_P", PS_Config_Lmt_Open_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Rawdata_N", PS_Config_Lmt_Open_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_P", PS_Config_Lmt_FW_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_N", PS_Config_Lmt_FW_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_CC_P", PS_Config_Lmt_FW_CC_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_CC_N", PS_Config_Lmt_FW_CC_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Diff_P", PS_Config_Lmt_FW_Diff_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Diff_N", PS_Config_Lmt_FW_Diff_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "PS_Config_Diff_Test_Frame", &PS_Config_Diff_Test_Frame))
		return -1;

	input_info(true, &ts->client->dev, "%s: Parse mp criteria done!\n", __func__);

	return 0;
}

#if LPWG_TEST
/*******************************************************
Description:
	Novatek touchscreen parse device tree mp lpwg function.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_lpwg_dt(struct device_node *root, const char *node_compatible)
{
	struct device_node *np = root;
	struct device_node *child = NULL;

	input_info(true, &ts->client->dev, "%s: Parse mp lpwg criteria for node %s\n", __func__, node_compatible);

	/* find each MP sub-nodes */
	for_each_child_of_node(root, child) {
		/* find the specified node */
		if (of_device_is_compatible(child, node_compatible)) {
			input_info(true, &ts->client->dev, "%s: found child node %s\n", __func__, node_compatible);
			np = child;
			break;
		}
	}
	if (child == NULL) {
		input_err(true, &ts->client->dev, "%s: Not found compatible node %s!\n", __func__, node_compatible);
		return -1;
	}

	/* MP Config*/
	if (nvt_mp_parse_u32(np, "IC_X_CFG_SIZE", &IC_X_CFG_SIZE))
		return -1;

	if (nvt_mp_parse_u32(np, "IC_Y_CFG_SIZE", &IC_Y_CFG_SIZE))
		return -1;

#if TOUCH_KEY_NUM > 0
	if (nvt_mp_parse_u32(np, "IC_KEY_CFG_SIZE", &IC_KEY_CFG_SIZE))
		return -1;
#endif

	if (nvt_mp_parse_u32(np, "X_Channel", &X_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "Y_Channel", &Y_Channel))
		return -1;

	if (nvt_mp_parse_ain(np, "AIN_X", AIN_X, IC_X_CFG_SIZE))
		return -1;

	if (nvt_mp_parse_ain(np, "AIN_Y", AIN_Y, IC_Y_CFG_SIZE))
		return -1;

#if TOUCH_KEY_NUM > 0
	if (nvt_mp_parse_ain(np, "AIN_KEY", AIN_KEY, IC_KEY_CFG_SIZE))
		return -1;
#endif

	/* MP Criteria */
	if (nvt_mp_parse_array(np, "PS_Config_Lmt_LPWG_Rawdata_P", PS_Config_Lmt_LPWG_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_LPWG_Rawdata_N", PS_Config_Lmt_LPWG_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_LPWG_Diff_P", PS_Config_Lmt_LPWG_Diff_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_LPWG_Diff_N", PS_Config_Lmt_LPWG_Diff_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "PS_Config_Diff_Test_Frame", &PS_Config_Diff_Test_Frame))
		return -1;

#if (FDM_TEST && LPWG_TEST)
	if (nvt_mp_parse_u32(np, "FDM_X_Channel", &FDM_X_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FDM_Rawdata_P", PS_Config_Lmt_FDM_Rawdata_P,
			FDM_X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FDM_Rawdata_N", PS_Config_Lmt_FDM_Rawdata_N,
			FDM_X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "PS_Config_FDM_Noise_Test_Frame", &PS_Config_FDM_Noise_Test_Frame))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FDM_Diff_P", PS_Config_Lmt_FDM_Diff_P,
			FDM_X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FDM_Diff_N", PS_Config_Lmt_FDM_Diff_N,
			FDM_X_Channel * Y_Channel + Key_Channel))
		return -1;
#endif

	input_info(true, &ts->client->dev, "%s: Parse mp lpwg criteria done!\n", __func__);

	return 0;
}
#endif /* LPWG_TEST */
#endif /* #ifdef CONFIG_OF */

/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_mp_proc_init(void)
{
//	NVT_proc_selftest_entry = proc_create("nvt_selftest", 0444, NULL, &nvt_selftest_fops);
	ctp_device_proc = proc_mkdir(TOUCHSCREEN_FOLDER, NULL);
		if(ctp_device_proc == NULL){
				input_err(true, &ts->client->dev, "%s:create parent_proc fail \n", __func__);
		}

    openshort_proc = proc_create(CTP_OPENSHORT_TEST_FILE, 0777, ctp_device_proc, &nvt_selftest_fops);
    if (openshort_proc == NULL) {
		input_err(true, &ts->client->dev, "%s:create openshort_proc entry fail \n", __func__);
	}

#if LPWG_TEST
	NVT_proc_lpwg_selftest_entry =
		proc_create("nvt_lpwg_selftest", 0444, NULL, &nvt_lpwg_selftest_fops);
#endif
/*	if (NVT_proc_selftest_entry == NULL) {
		input_err(true, &ts->client->dev, "%s: create /proc/nvt_selftest Failed!\n", __func__);
		return -1;
*/
#if LPWG_TEST
//	} else
	if (NVT_proc_lpwg_selftest_entry == NULL) {
		input_err(true, &ts->client->dev, "%s: create /proc/nvt_lpwg_selftest Failed!\n", __func__);
		return -1;
#endif
	} else {
		if(nvt_mp_buffer_init()) {
			input_err(true, &ts->client->dev, "%s: Allocate mp memory failed\n", __func__);
			return -1;
		}
		else {
			input_info(true, &ts->client->dev, "%s: create /proc/nvt_selftest Succeeded!\n", __func__);
#if LPWG_TEST
			input_info(true, &ts->client->dev, "%s: create /proc/nvt_lpwg_selftest Succeeded!\n", __func__);
#endif
		}
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_mp_proc_deinit(void)
{
	nvt_mp_buffer_deinit();
/*
	if (NVT_proc_selftest_entry != NULL) {
		remove_proc_entry("nvt_selftest", NULL);
		NVT_proc_selftest_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/nvt_selftest\n", __func__);
	}
*/
	if (ctp_device_proc != NULL) {
		remove_proc_entry("nvt_selftest", NULL);
		ctp_device_proc = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/nvt_selftest\n", __func__);
	}

#if LPWG_TEST
	if (NVT_proc_lpwg_selftest_entry != NULL) {
		remove_proc_entry("nvt_lpwg_selftest", NULL);
		NVT_proc_lpwg_selftest_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/nvt_lpwg_selftest\n", __func__);
	}
#endif
}
#endif /* #if NVT_TOUCH_MP */
