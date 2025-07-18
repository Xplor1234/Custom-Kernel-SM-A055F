/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 52752 $
 * $Date: 2019-11-06 18:05:46 +0800 (週三, 06 十一月 2019) $
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

#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/vmalloc.h>
//#include <linux/spu-verify.h>

#include "nt36xxx.h"
#include "nvt_ts_fw.h"
#include <linux/ktime.h>

#if BOOT_UPDATE_FIRMWARE

#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)

#define NVT_DUMP_PARTITION      (0)
#define NVT_DUMP_PARTITION_LEN  (1024)
#define NVT_DUMP_PARTITION_PATH "/data/local/tmp"

#define FW_BIN_SEC_INFO		(fw_need_write_size - 16)
#define FW_BIN_IC_ID		(FW_BIN_SEC_INFO + 2)
#define FW_BIN_PROJECT_ID	(FW_BIN_SEC_INFO + 3)
#define FW_BIN_PANEL_ID		(FW_BIN_SEC_INFO + 4)
#define FW_BIN_VERSION		(FW_BIN_SEC_INFO + 5)

static ktime_t start, end;
const struct firmware *fw_entry = NULL;
#if LOAD_TP_FW_FROM_H
const struct firmware *fw_entry_mp = NULL;
struct firmware *request_fw_headfile = NULL;
#endif
static size_t fw_need_write_size = 0;
static uint8_t *fwbuf = NULL;
//UMS
struct firmware tmp_fw_entry;
u8 *fw_data;
long fw_size;

struct nvt_ts_bin_map {
	char name[12];
	uint32_t BIN_addr;
	uint32_t SRAM_addr;
	uint32_t size;
	uint32_t crc;
};

static struct nvt_ts_bin_map *bin_map;
#if SEC_TOUCH_CMD
int nvt_sec_mp_parse_dt(struct nvt_ts_data *ts, const char *node_compatible);
#endif

static int32_t nvt_get_fw_need_write_size(const struct firmware *fw_entry)
{
	int32_t i = 0;
	int32_t total_sectors_to_check = 0;

	total_sectors_to_check = fw_entry->size / FLASH_SECTOR_SIZE;
	/* printk("total_sectors_to_check = %d\n", total_sectors_to_check); */

	for (i = total_sectors_to_check; i > 0; i--) {
		/* printk("current end flag address checked = 0x%X\n", i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN); */
		/* check if there is end flag "NVT" at the end of this sector */
		if (strncmp(&fw_entry->data[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN], "NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
			fw_need_write_size = i * FLASH_SECTOR_SIZE;
			input_info(true, &ts->client->dev, "%s: fw_need_write_size = %zu(0x%zx), NVT end flag\n",
						__func__, fw_need_write_size, fw_need_write_size);
			return 0;
		}

		/* check if there is end flag "MOD" at the end of this sector */
		if (strncmp(&fw_entry->data[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN], "MOD", NVT_FLASH_END_FLAG_LEN) == 0) {
			fw_need_write_size = i * FLASH_SECTOR_SIZE;
			input_info(true, &ts->client->dev, "%s: fw_need_write_size = %zu(0x%zx), MOD end flag\n",
						__func__, fw_need_write_size, fw_need_write_size);
			return 0;
		}
	}

	input_err(true, &ts->client->dev, "%s: end flag \"NVT\" \"MOD\" not found!\n", __func__);
	return -1;
}

/*******************************************************
Description:
	Novatek touchscreen init variable and allocate buffer
for download firmware function.

return:
	n.a.
*******************************************************/
static int32_t nvt_download_init(void)
{
	/* allocate buffer for transfer firmware */
	//input_info(true, &ts->client->dev, "NVT_TRANSFER_LEN = 0x%06X\n", NVT_TRANSFER_LEN);

	if (fwbuf == NULL) {
		fwbuf = (uint8_t *)kzalloc((NVT_TRANSFER_LEN + 1 + DUMMY_BYTES), GFP_KERNEL);
		if(fwbuf == NULL) {
			input_err(true, &ts->client->dev, "%s: kzalloc for fwbuf failed!\n", __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen checksum function. Calculate bin
file checksum for comparison.

return:
	n.a.
*******************************************************/
static uint32_t CheckSum(const u8 *data, size_t len)
{
	uint32_t i = 0;
	uint32_t checksum = 0;

	for (i = 0 ; i < len+1 ; i++)
		checksum += data[i];

	checksum += len;
	checksum = ~checksum +1;

	return checksum;
}

static uint32_t byte_to_word(const uint8_t *data)
{
	return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
}

/*******************************************************
Description:
	Novatek touchscreen parsing bin header function.

return:
	n.a.
*******************************************************/
static uint32_t partition = 0;
static uint8_t ilm_dlm_num = 2;
static int32_t nvt_bin_header_parser(const u8 *fwdata, size_t fwsize)
{
	uint32_t list = 0;
	uint32_t pos = 0x00;
	uint32_t end = 0x00;
	uint8_t info_sec_num = 0;
	uint8_t ovly_sec_num = 0;
	uint8_t ovly_info = 0;

	/* Find the header size */
	end = fwdata[0] + (fwdata[1] << 8) + (fwdata[2] << 16) + (fwdata[3] << 24);
	pos = 0x30;	// info section start at 0x30 offset
	while (pos < end) {
		info_sec_num ++;
		pos += 0x10;	/* each header info is 16 bytes */
	}

	/*
	 * Find the DLM OVLY section
	 * [0:3] Overlay Section Number
	 * [4]   Overlay Info
	 */
	ovly_info = (fwdata[0x28] & 0x10) >> 4;
	ovly_sec_num = (ovly_info) ? (fwdata[0x28] & 0x0F) : 0;

	/*
	 * calculate all partition number
	 * ilm_dlm_num (ILM & DLM) + ovly_sec_num + info_sec_num
	 */
	partition = ilm_dlm_num + ovly_sec_num + info_sec_num;
	input_info(true, &ts->client->dev, "%s: ovly_info = %d, ilm_dlm_num = %d, ovly_sec_num = %d, info_sec_num = %d, partition = %d\n",
					__func__, ovly_info, ilm_dlm_num, ovly_sec_num, info_sec_num, partition);

	/* allocated memory for header info */
	bin_map = (struct nvt_ts_bin_map *)kzalloc((partition+1) * sizeof(struct nvt_ts_bin_map), GFP_KERNEL);
	if(bin_map == NULL) {
		input_err(true, &ts->client->dev, "%s: kzalloc for bin_map failed!\n", __func__);
		return -ENOMEM;
	}

	for (list = 0; list < partition; list++) {
		/*
		 * [1] parsing ILM & DLM header info
		 * BIN_addr : SRAM_addr : size (12-bytes)
		 * crc located at 0x18 & 0x1C
		 */
		if (list < ilm_dlm_num) {
			bin_map[list].BIN_addr = byte_to_word(&fwdata[0 + list*12]);
			bin_map[list].SRAM_addr = byte_to_word(&fwdata[4 + list*12]);
			bin_map[list].size = byte_to_word(&fwdata[8 + list*12]);
			if (ts->hw_crc)
				bin_map[list].crc = byte_to_word(&fwdata[0x18 + list*4]);
			else { //ts->hw_crc
				if ((bin_map[list].BIN_addr + bin_map[list].size) < fwsize)
					bin_map[list].crc = CheckSum(&fwdata[bin_map[list].BIN_addr], bin_map[list].size);
				else {
					input_err(true, &ts->client->dev, "%s: access range (0x%08X to 0x%08X) is larger than bin size!\n",
							__func__, bin_map[list].BIN_addr, bin_map[list].BIN_addr + bin_map[list].size);
					return -EINVAL;
				}
			} //ts->hw_crc
			if (list == 0)
				sprintf(bin_map[list].name, "ILM");
			else if (list == 1)
				sprintf(bin_map[list].name, "DLM");
		}

		/*
		 * [2] parsing others header info
		 * SRAM_addr : size : BIN_addr : crc (16-bytes)
		 */
		if ((list >= ilm_dlm_num) && (list < (ilm_dlm_num + info_sec_num))) {
			/* others partition located at 0x30 offset */
			pos = 0x30 + (0x10 * (list - ilm_dlm_num));

			bin_map[list].SRAM_addr = byte_to_word(&fwdata[pos]);
			bin_map[list].size = byte_to_word(&fwdata[pos+4]);
			bin_map[list].BIN_addr = byte_to_word(&fwdata[pos+8]);
			if (ts->hw_crc)
				bin_map[list].crc = byte_to_word(&fwdata[pos+12]);
			else { //ts->hw_crc
				if ((bin_map[list].BIN_addr + bin_map[list].size) < fwsize)
					bin_map[list].crc = CheckSum(&fwdata[bin_map[list].BIN_addr], bin_map[list].size);
				else {
					input_err(true, &ts->client->dev, "%s: access range (0x%08X to 0x%08X) is larger than bin size!\n",
							__func__, bin_map[list].BIN_addr, bin_map[list].BIN_addr + bin_map[list].size);
					return -EINVAL;
				}
			} //ts->hw_crc
			/* detect header end to protect parser function */
			if ((bin_map[list].BIN_addr == 0) && (bin_map[list].size != 0)) {
				sprintf(bin_map[list].name, "Header");
			} else {
				sprintf(bin_map[list].name, "Info-%d", (list - ilm_dlm_num));
			}
		}

		/*
		 * [3] parsing overlay section header info
		 * SRAM_addr : size : BIN_addr : crc (16-bytes)
		 */
		if (list >= (ilm_dlm_num + info_sec_num)) {
			/* overlay info located at DLM (list = 1) start addr */
			pos = bin_map[1].BIN_addr + (0x10 * (list- ilm_dlm_num - info_sec_num));

			bin_map[list].SRAM_addr = byte_to_word(&fwdata[pos]);
			bin_map[list].size = byte_to_word(&fwdata[pos+4]);
			bin_map[list].BIN_addr = byte_to_word(&fwdata[pos+8]);
			if (ts->hw_crc)
				bin_map[list].crc = byte_to_word(&fwdata[pos+12]);
			else { //ts->hw_crc
				if ((bin_map[list].BIN_addr + bin_map[list].size) < fwsize)
					bin_map[list].crc = CheckSum(&fwdata[bin_map[list].BIN_addr], bin_map[list].size);
				else {
					input_err(true, &ts->client->dev, "%s: access range (0x%08X to 0x%08X) is larger than bin size!\n",
							__func__, bin_map[list].BIN_addr, bin_map[list].BIN_addr + bin_map[list].size);
					return -EINVAL;
				}
			} //ts->hw_crc
			sprintf(bin_map[list].name, "Overlay-%d", (list- ilm_dlm_num - info_sec_num));
		}

		/* BIN size error detect */
		if ((bin_map[list].BIN_addr + bin_map[list].size) > fwsize) {
			input_err(true, &ts->client->dev, "%s: access range (0x%08X to 0x%08X) is larger than bin size!\n",
					__func__, bin_map[list].BIN_addr, bin_map[list].BIN_addr + bin_map[list].size);
			return -EINVAL;
		}

		input_err(true, &ts->client->dev, "%s: [%d][%s] SRAM (0x%08X), SIZE (0x%08X), BIN (0x%08X), CRC (0x%08X)\n",
				__func__, list, bin_map[list].name,
				bin_map[list].SRAM_addr, bin_map[list].size,  bin_map[list].BIN_addr, bin_map[list].crc);
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen release update firmware function.

return:
	n.a.
*******************************************************/
static void update_firmware_release(void)
{
#if LOAD_TP_FW_FROM_H
	if (request_fw_headfile) {
		kfree(request_fw_headfile);
	}

	if (fw_entry) {
		release_firmware(fw_entry);
	}

	request_fw_headfile = NULL;
	fw_entry = NULL;
	fw_entry_mp = NULL;
#else
	if (fw_entry) {		
		release_firmware(fw_entry);	

	}	
	
	fw_entry = NULL;
#endif	
}

/*******************************************************
Description:
	Novatek touchscreen request update firmware function.

return:
	Executive outcomes. 0---succeed. -1,-22---failed.
*******************************************************/
static int32_t update_firmware_request(const char *filename)
{
	uint8_t retry = 0;
	int32_t ret = 0;

	if (NULL == filename) {
		return -ENOENT;
	}

	while (1) {
		input_info(true, &ts->client->dev, "filename is %s\n", filename);

		if(ts->isUMS) {
			input_info(true, &ts->client->dev, "%s ts->isUMS\n", __func__);
			if(tmp_fw_entry.data != NULL) {
				input_info(true, &ts->client->dev, "%s UMS FW present\n", __func__);
				fw_entry = &tmp_fw_entry;
			} else {
				input_info(true, &ts->client->dev, "%s UMS FW NOT present\n", __func__);
				ts->isUMS = false;
			}
		}

		if(!ts->isUMS){
			#if LOAD_TP_FW_FROM_H
			if ((strcmp(filename, BOOT_UPDATE_FIRMWARE_NAME)) &&
				(strcmp(filename, MP_UPDATE_FIRMWARE_NAME))) {
				input_err(true, &ts->client->dev, "filename %s not support\n", filename);
				goto request_fail;
			}

			//LOAD TP FW from headfile
			if (!strcmp(filename, BOOT_UPDATE_FIRMWARE_NAME)) {
				ret = request_firmware(&fw_entry, filename, &ts->client->dev);
				if (ret) {
					input_info(true, &ts->client->dev, "firmware load failed ret=%d\t;request firmware from headfile\n", ret);

					request_fw_headfile = kzalloc(sizeof(struct firmware), GFP_KERNEL);
					if(request_fw_headfile == NULL) {
						input_info(true, &ts->client->dev, "request_fw_headfile kzalloc failed!\n");
						ret = -1;
						goto request_fail;
					}

					request_fw_headfile->size = sizeof(NVT_TS_FW);
					request_fw_headfile->data = NVT_TS_FW;

					fw_entry = request_fw_headfile;
				}else{
				input_info(true, &ts->client->dev, "firmware download succeed\n");
				}
			}

			//LOAD MP FW from file system
			else if (!strcmp(filename, MP_UPDATE_FIRMWARE_NAME)) {
				input_info(true, &ts->client->dev, "request mp firmware\n");
				ret = request_firmware(&fw_entry_mp, filename, &ts->client->dev);
				if (ret) {
					input_info(true, &ts->client->dev, "request mp firmware failed\n");
					goto request_fail;
				} else {
					fw_entry = fw_entry_mp;
				}
			}
			#else
			ret = request_firmware(&fw_entry, filename, &ts->client->dev);
			if (ret) {
				input_err(true, &ts->client->dev, "firmware load failed, ret=%d\n", ret);
				goto request_fail;
			}
			#endif
		}

		// check FW need to write size
		if (nvt_get_fw_need_write_size(fw_entry)) {
			input_err(true, &ts->client->dev, "get fw need to write size fail!\n");
			ret = -EINVAL;
			goto invalid;
		}

		// check if FW version add FW version bar equals 0xFF
		if (*(fw_entry->data + FW_BIN_VER_OFFSET) + *(fw_entry->data + FW_BIN_VER_BAR_OFFSET) != 0xFF) {
			input_err(true, &ts->client->dev, "bin file FW_VER + FW_VER_BAR should be 0xFF!\n");
			input_err(true, &ts->client->dev, "FW_VER=0x%02X, FW_VER_BAR=0x%02X\n", *(fw_entry->data+FW_BIN_VER_OFFSET), *(fw_entry->data+FW_BIN_VER_BAR_OFFSET));
			ret = -ENOEXEC;
			goto invalid;
		}

		/* BIN Header Parser */
		ret = nvt_bin_header_parser(fw_entry->data, fw_entry->size);
		if (ret) {
			input_err(true, &ts->client->dev, "bin header parser failed\n");
			goto invalid;
		} else {
			ts->fw_ver_bin[0] = fw_entry->data[FW_BIN_IC_ID];
			ts->fw_ver_bin[1] = fw_entry->data[FW_BIN_PROJECT_ID];
			ts->fw_ver_bin[2] = fw_entry->data[FW_BIN_PANEL_ID];
			ts->fw_ver_bin[3] = fw_entry->data[FW_BIN_VER_OFFSET];
			ts->fw_ver_bin_bar = fw_entry->data[FW_BIN_VER_BAR_OFFSET];

			input_err(true, &ts->client->dev, "%s: fw_ver_bin = %02X%02X%02X%02X\n", __func__,
				ts->fw_ver_bin[0], ts->fw_ver_bin[1], ts->fw_ver_bin[2], ts->fw_ver_bin[3]);
			break;
		}

invalid:
		if (ts->isUMS) {
			input_info(true, &ts->client->dev, "%s UMS Update NO update_firmware_release\n", __func__);
		} else {
			input_info(true, &ts->client->dev, "%s BUILT-IN Update update_firmware_release\n", __func__);
			update_firmware_release();
		}
		if (!IS_ERR_OR_NULL(bin_map)) {
			kfree(bin_map);
			bin_map = NULL;
		}

request_fail:
		retry++;
		if(unlikely(retry > 2)) {
			input_err(true, &ts->client->dev, "error, retry=%d\n", retry);
			break;
		}
	}

	return ret;
}

#if NVT_DUMP_PARTITION
/*******************************************************
Description:
	Novatek touchscreen dump flash partition function.

return:
	n.a.
*******************************************************/
loff_t file_offset = 0;
static int32_t nvt_read_ram_and_save_file(uint32_t addr, uint16_t len, char *name)
{
	char file[256] = "";
	uint8_t *fbufp = NULL;
	int32_t ret = 0;
	struct file *fp = NULL;
	//mm_segment_t org_fs;

	sprintf(file, "%s/dump_%s.bin", NVT_DUMP_PARTITION_PATH, name);
	input_info(true, &ts->client->dev, "%s: Dump [%s] from 0x%08X to 0x%08X\n",
				__func__, file, addr, addr+len);

	fbufp = (uint8_t *)kzalloc(len+1, GFP_KERNEL);
	if(fbufp == NULL) {
		input_info(true, &ts->client->dev, "%s: kzalloc for fbufp failed!\n", __func__);
		ret = -ENOMEM;
		goto alloc_buf_fail;
	}

	//org_fs = get_fs();
	//set_fs(KERNEL_DS);
	fp = filp_open(file, O_RDWR | O_CREAT, 0644);
	if (fp == NULL || IS_ERR(fp)) {
		ret = -ENOMEM;
		input_info(true, &ts->client->dev, "%s: open file failed\n", __func__);
		goto open_file_fail;
	}

	/* SPI read */
	//---set xdata index to addr---
	nvt_set_page(addr);

	fbufp[0] = addr & 0x7F;	//offset
	CTP_SPI_READ(ts->client, fbufp, len+1);

	/* Write to file */
	ret = vfs_write(fp, (char __user *)fbufp+1, len, &file_offset);
	if (ret != len) {
		input_info(true, &ts->client->dev, "%s: write file failed\n", __func__);
		goto open_file_fail;
	} else {
		ret = 0;
	}

open_file_fail:
	set_fs(org_fs);
	if (!IS_ERR_OR_NULL(fp)) {
		filp_close(fp, NULL);
		fp = NULL;
	}

	if (!IS_ERR_OR_NULL(fbufp)) {
		kfree(fbufp);
		fbufp = NULL;
	}
alloc_buf_fail:

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen nvt_dump_partition function to dump
 each partition for debug.

return:
	n.a.
*******************************************************/
static int32_t nvt_dump_partition(void)
{
	uint32_t list = 0;
	char *name;
	uint32_t SRAM_addr, size;
	uint32_t i = 0;
	uint16_t len = 0;
	int32_t count = 0;
	int32_t ret = 0;

	if (NVT_DUMP_PARTITION_LEN >= sizeof(ts->rbuf)) {
		input_err(true, &ts->client->dev, "%s: dump len %d is larger than buffer size %ld\n",
				__func__, NVT_DUMP_PARTITION_LEN, sizeof(ts->rbuf));
		return -EINVAL;
	} else if (NVT_DUMP_PARTITION_LEN >= NVT_TRANSFER_LEN) {
		input_err(true, &ts->client->dev, "%s: dump len %d is larger than NVT_TRANSFER_LEN\n",
				__func__, NVT_DUMP_PARTITION_LEN);
		return -EINVAL;
	}

	if (bin_map == NULL) {
		input_err(true, &ts->client->dev, "%s: bin_map is NULL\n", __func__);
		return -ENOMEM;
	}

	memset(fwbuf, 0, (NVT_DUMP_PARTITION_LEN+1));

	for (list = 0; list < partition; list++) {
		/* initialize variable */
		SRAM_addr = bin_map[list].SRAM_addr;
		size = bin_map[list].size;
		name = bin_map[list].name;

		/* ignore reserved partition (Reserved Partition size is zero) */
		if (!size)
			continue;
		else
			size = size +1;

		/* write data to SRAM */
		if (size % NVT_DUMP_PARTITION_LEN)
			count = (size / NVT_DUMP_PARTITION_LEN) + 1;
		else
			count = (size / NVT_DUMP_PARTITION_LEN);

		for (i = 0 ; i < count ; i++) {
			len = (size < NVT_DUMP_PARTITION_LEN) ? size : NVT_DUMP_PARTITION_LEN;

			/* dump for debug download firmware */
			ret = nvt_read_ram_and_save_file(SRAM_addr, len, name);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: nvt_read_ram_and_save_file failed, ret = %d\n", __func__, ret);
				goto out;
			}

			SRAM_addr += NVT_DUMP_PARTITION_LEN;
			size -= NVT_DUMP_PARTITION_LEN;
		}

		file_offset = 0;
	}

out:
	return ret;
}
#endif /* NVT_DUMP_PARTITION */

/*******************************************************
Description:
	Novatek touchscreen write data to sram function.

- fwdata   : The buffer is written
- SRAM_addr: The sram destination address
- size     : Number of data bytes in @fwdata being written
- BIN_addr : The transferred data offset of @fwdata

return:
	Executive outcomes. 0---succeed. else---fail.
*******************************************************/
static int32_t nvt_write_sram(const u8 *fwdata,
		uint32_t SRAM_addr, uint32_t size, uint32_t BIN_addr)
{
	int32_t ret = 0;
	uint32_t i = 0;
	uint16_t len = 0;
	int32_t count = 0;

	if (size % NVT_TRANSFER_LEN)
		count = (size / NVT_TRANSFER_LEN) + 1;
	else
		count = (size / NVT_TRANSFER_LEN);

	for (i = 0 ; i < count ; i++) {
		len = (size < NVT_TRANSFER_LEN) ? size : NVT_TRANSFER_LEN;

		//---set xdata index to start address of SRAM---
		ret = nvt_set_page(SRAM_addr);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: set page failed, ret = %d\n", __func__, ret);
			return ret;
		}

		//---write data into SRAM---
		fwbuf[0] = SRAM_addr & 0x7F;	//offset
		memcpy(fwbuf+1, &fwdata[BIN_addr], len);	//payload
		ret = CTP_SPI_WRITE(ts->client, fwbuf, len+1);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: write to sram failed, ret = %d\n", __func__, ret);
			return ret;
		}

		SRAM_addr += NVT_TRANSFER_LEN;
		BIN_addr += NVT_TRANSFER_LEN;
		size -= NVT_TRANSFER_LEN;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen nvt_write_firmware function to write
firmware into each partition.

return:
	n.a.
*******************************************************/
static int32_t nvt_write_firmware(const u8 *fwdata, size_t fwsize)
{
	uint32_t list = 0;
	//char *name = NULL;
	uint32_t BIN_addr, SRAM_addr, size;
	int32_t ret = 0;

	memset(fwbuf, 0, (NVT_TRANSFER_LEN+1));

	for (list = 0; list < partition; list++) {
		/* initialize variable */
		SRAM_addr = bin_map[list].SRAM_addr;
		size = bin_map[list].size;
		BIN_addr = bin_map[list].BIN_addr;
		//name = bin_map[list].name;

//		input_info(true, &ts->client->dev, "[%d][%s] SRAM (0x%08X), SIZE (0x%08X), BIN (0x%08X)\n",
//				list, name, SRAM_addr, size, BIN_addr);

		/* Check data size */
		if ((BIN_addr + size) > fwsize) {
			input_err(true, &ts->client->dev, "%s: access range (0x%08X to 0x%08X) is larger than bin size!\n",
					__func__, BIN_addr, BIN_addr + size);
			ret = -EINVAL;
			goto out;
		}

		/* ignore reserved partition (Reserved Partition size is zero) */
		if (!size)
			continue;
		else
			size = size +1;

		/* write data to SRAM */
		ret = nvt_write_sram(fwdata, SRAM_addr, size, BIN_addr);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: sram program failed, ret = %d\n", __func__, ret);
			goto out;
		}
	}

out:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen check checksum function.
This function will compare file checksum and fw checksum.

return:
	n.a.
*******************************************************/
static int32_t nvt_check_fw_checksum(void)
{
	uint32_t fw_checksum = 0;
	uint32_t len = partition*4;
	uint32_t list = 0;
	int32_t ret = 0;

	memset(fwbuf, 0, (len+1));

	//---set xdata index to checksum---
	nvt_set_page(ts->mmap->R_ILM_CHECKSUM_ADDR);

	/* read checksum */
	fwbuf[0] = (ts->mmap->R_ILM_CHECKSUM_ADDR) & 0x7F;
	ret = CTP_SPI_READ(ts->client, fwbuf, len+1);
	if (ret) {
		input_err(true, &ts->client->dev, "Read fw checksum failed\n");
		return ret;
	}

	/*
	 * Compare each checksum from fw
	 * ILM + DLM + Overlay + Info
	 * ilm_dlm_num (ILM & DLM) + ovly_sec_num + info_sec_num
	 */
	for (list = 0; list < partition; list++) {
		fw_checksum = byte_to_word(&fwbuf[1+list*4]);

		/* ignore reserved partition (Reserved Partition size is zero) */
		if(!bin_map[list].size)
			continue;

		if (bin_map[list].crc != fw_checksum) {
			input_err(true, &ts->client->dev, "[%d] BIN_checksum=0x%08X, FW_checksum=0x%08X\n",
					list, bin_map[list].crc, fw_checksum);
			ret = -EIO;
		}
	}

	return ret;
}

int nvt_get_checksum(struct nvt_ts_data *ts, u8 *csum_result, u8 csum_size)
{
	uint32_t fw_checksum = 0;
	uint32_t len = partition*4;
	uint32_t list = 0;
	int32_t ret = 0;
	u8 temp[BLOCK_64KB_NUM * 4 + 1] = { 0 };

	if ((len > csum_size) && (!ts->hw_crc)) {
		input_err(true, &ts->client->dev, "Buffer size is too short!\n");
		return -EINVAL;
	}

	memset(fwbuf, 0, (len+1));

	//---set xdata index to checksum---
	nvt_set_page(ts->mmap->R_ILM_CHECKSUM_ADDR);

	/* read checksum */
	fwbuf[0] = (ts->mmap->R_ILM_CHECKSUM_ADDR) & 0x7F;
	ret = CTP_SPI_READ(ts->client, fwbuf, len+1);
	if (ret) {
		input_err(true, &ts->client->dev, "Read fw checksum failed\n");
		return ret;
	}

	if (ts->hw_crc) {
		// Since IC w/ HW CRC, we only copy ILM + DLM checksum.
		// Total 8 bytes
		for (list = 0; list < 2; list++) {
			fw_checksum = byte_to_word(&fwbuf[1+list*4]);

			snprintf(temp, sizeof(temp), "%04X", fw_checksum);
			strlcat(csum_result, temp, csum_size);
		}
	} else {
		/*
		 * Copy each checksum from fw
		 * ILM + DLM + Overlay + Info
		 * ilm_dlm_num (ILM & DLM) + ovly_sec_num + info_sec_num
		 */
		for (list = 0; list < partition; list++) {
			fw_checksum = byte_to_word(&fwbuf[1+list*4]);

			/* ignore reserved partition (Reserved Partition size is zero) */
			if(!bin_map[list].size)
				continue;

			snprintf(temp, sizeof(temp), "%04X", fw_checksum);
			strlcat(csum_result, temp, csum_size);
		}
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen set bootload crc reg bank function.
This function will set hw crc reg before enable crc function.

return:
	n.a.
*******************************************************/
static void nvt_set_bld_crc_bank(uint32_t DES_ADDR, uint32_t SRAM_ADDR,
		uint32_t LENGTH_ADDR, uint32_t size,
		uint32_t G_CHECKSUM_ADDR, uint32_t crc)
{
	/* write destination address */
	nvt_set_page(DES_ADDR);
	fwbuf[0] = DES_ADDR & 0x7F;
	fwbuf[1] = (SRAM_ADDR) & 0xFF;
	fwbuf[2] = (SRAM_ADDR >> 8) & 0xFF;
	fwbuf[3] = (SRAM_ADDR >> 16) & 0xFF;
	CTP_SPI_WRITE(ts->client, fwbuf, 4);

	/* write length */
	//nvt_set_page(LENGTH_ADDR);
	fwbuf[0] = LENGTH_ADDR & 0x7F;
	fwbuf[1] = (size) & 0xFF;
	fwbuf[2] = (size >> 8) & 0xFF;
	fwbuf[3] = (size >> 16) & 0x01;
	if (ts->hw_crc == 1) {
		CTP_SPI_WRITE(ts->client, fwbuf, 3);
	} else if (ts->hw_crc > 1) {
		CTP_SPI_WRITE(ts->client, fwbuf, 4);
	}

	/* write golden dlm checksum */
	//nvt_set_page(G_CHECKSUM_ADDR);
	fwbuf[0] = G_CHECKSUM_ADDR & 0x7F;
	fwbuf[1] = (crc) & 0xFF;
	fwbuf[2] = (crc >> 8) & 0xFF;
	fwbuf[3] = (crc >> 16) & 0xFF;
	fwbuf[4] = (crc >> 24) & 0xFF;
	CTP_SPI_WRITE(ts->client, fwbuf, 5);

	return;
}

/*******************************************************
Description:
	Novatek touchscreen set BLD hw crc function.
This function will set ILM and DLM crc information to register.

return:
	n.a.
*******************************************************/
static void nvt_set_bld_hw_crc(void)
{
	/* [0] ILM */
	/* write register bank */
	nvt_set_bld_crc_bank(ts->mmap->ILM_DES_ADDR, bin_map[0].SRAM_addr,
			ts->mmap->ILM_LENGTH_ADDR, bin_map[0].size,
			ts->mmap->G_ILM_CHECKSUM_ADDR, bin_map[0].crc);

	/* [1] DLM */
	/* write register bank */
	nvt_set_bld_crc_bank(ts->mmap->DLM_DES_ADDR, bin_map[1].SRAM_addr,
			ts->mmap->DLM_LENGTH_ADDR, bin_map[1].size,
			ts->mmap->G_DLM_CHECKSUM_ADDR, bin_map[1].crc);
}

/*******************************************************
Description:
	Novatek touchscreen read BLD hw crc info function.
This function will check crc results from register.

return:
	n.a.
*******************************************************/
static void nvt_read_bld_hw_crc(void)
{
	uint8_t buf[8] = {0};
	uint32_t g_crc = 0, r_crc = 0;

	/* CRC Flag */
	nvt_set_page(ts->mmap->BLD_ILM_DLM_CRC_ADDR);
	buf[0] = ts->mmap->BLD_ILM_DLM_CRC_ADDR & 0x7F;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 2);
	input_info(true, &ts->client->dev, "crc_done = %d, ilm_crc_flag = %d, dlm_crc_flag = %d\n",
			(buf[1] >> 2) & 0x01, (buf[1] >> 0) & 0x01, (buf[1] >> 1) & 0x01);

	/* ILM CRC */
	nvt_set_page(ts->mmap->G_ILM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->G_ILM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	CTP_SPI_READ(ts->client, buf, 5);
	g_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	nvt_set_page(ts->mmap->R_ILM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->R_ILM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	CTP_SPI_READ(ts->client, buf, 5);
	r_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	input_info(true, &ts->client->dev, "ilm: bin crc = 0x%08X, golden = 0x%08X, result = 0x%08X\n",
			bin_map[0].crc, g_crc, r_crc);

	/* DLM CRC */
	nvt_set_page(ts->mmap->G_DLM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->G_DLM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	CTP_SPI_READ(ts->client, buf, 5);
	g_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	nvt_set_page(ts->mmap->R_DLM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->R_DLM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	CTP_SPI_READ(ts->client, buf, 5);
	r_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	input_info(true, &ts->client->dev, "dlm: bin crc = 0x%08X, golden = 0x%08X, result = 0x%08X\n",
			bin_map[1].crc, g_crc, r_crc);

	return;
}

/*******************************************************
Description:
	Novatek touchscreen Download_Firmware with HW CRC
function. It's complete download firmware flow.

return:
	Executive outcomes. 0---succeed. else---fail.
*******************************************************/
static int32_t nvt_download_firmware_hw_crc(void)
{
	uint8_t retry = 0;
	int32_t ret = 0;

	start = ktime_get_real() / 1000; //do_gettimeofday(&start);

	while (1) {
		/* bootloader reset to reset MCU */
		nvt_bootloader_reset();

		/* Start to write firmware process */
		ret = nvt_write_firmware(fw_entry->data, fw_entry->size);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Write_Firmware failed. (%d)\n", __func__, ret);
			goto fail;
		}

#if NVT_DUMP_PARTITION
		ret = nvt_dump_partition();
		if (ret) {
			input_err(true, &ts->client->dev, "%s: nvt_dump_partition failed, ret = %d\n", __func__, ret);
		}
#endif

		/* set ilm & dlm reg bank */
		nvt_set_bld_hw_crc();

		/* enable hw bld crc function */
		nvt_bld_crc_enable();

		/* clear fw reset status & enable fw crc check */
		nvt_fw_crc_enable();

		/* Set Boot Ready Bit */
		nvt_boot_ready();

		ret = nvt_check_fw_reset_state(RESET_STATE_INIT);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: nvt_check_fw_reset_state failed. (%d)\n", __func__, ret);
			goto fail;
		} else {
			break;
		}

fail:
		retry++;
		if(unlikely(retry > 2)) {
			input_err(true, &ts->client->dev, "%s: error, retry=%d\n", __func__, retry);
			nvt_read_bld_hw_crc();
			break;
		}
	}

	end = ktime_get_real() / 1000; //do_gettimeofday(&end);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen Download_Firmware function. It's
complete download firmware flow.

return:
	n.a.
*******************************************************/
static int32_t nvt_download_firmware(void)
{
	uint8_t retry = 0;
	int32_t ret = 0;

	start = ktime_get_real() / 1000; //do_gettimeofday(&start);

	while (1) {
		/*
		 * Send eng reset cmd before download FW
		 * Keep TP_RESX low when send eng reset cmd
		 */
#if NVT_TOUCH_SUPPORT_HW_RST
		gpio_set_value(ts->reset_gpio, 0);
		mdelay(1);	//wait 1ms
#endif
		nvt_eng_reset();
#if NVT_TOUCH_SUPPORT_HW_RST
		gpio_set_value(ts->reset_gpio, 1);
		mdelay(10);	//wait tRT2BRST after TP_RST
#endif
		nvt_bootloader_reset();

		/* clear fw reset status */
		nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE, 0x00);

		/* Start to write firmware process */
		ret = nvt_write_firmware(fw_entry->data, fw_entry->size);
		if (ret) {
			input_err(true, &ts->client->dev, "Write_Firmware failed. (%d)\n", ret);
			goto fail;
		}

#if NVT_DUMP_PARTITION
		ret = nvt_dump_partition();
		if (ret) {
			input_err(true, &ts->client->dev, "nvt_dump_partition failed, ret = %d\n", ret);
		}
#endif

		/* Set Boot Ready Bit */
		nvt_boot_ready();

		ret = nvt_check_fw_reset_state(RESET_STATE_INIT);
		if (ret) {
			input_err(true, &ts->client->dev, "nvt_check_fw_reset_state failed. (%d)\n", ret);
			goto fail;
		}

		/* check fw checksum result */
		ret = nvt_check_fw_checksum();
		if (ret) {
			input_err(true, &ts->client->dev, "firmware checksum not match, retry=%d\n", retry);
			goto fail;
		} else {
			break;
		}

fail:
		retry++;
		if(unlikely(retry > 2)) {
			input_err(true, &ts->client->dev, "error, retry=%d\n", retry);
			break;
		}
	}

	end = ktime_get_real() / 1000; //do_gettimeofday(&end);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen update firmware main function.

return:
	n.a.
*******************************************************/
int32_t nvt_update_firmware(const char *firmware_name)
{
	int32_t ret = 0;

	if (firmware_name == NULL || strlen(firmware_name) == 0) {
		ret = -EINVAL;
		input_err(true, &ts->client->dev, "%s : firmware_name is null\n", __func__);
		goto request_firmware_fail;
	}

	// request bin file in "/etc/firmware"
	ret = update_firmware_request(firmware_name);
	if (ret) {
		input_err(true, &ts->client->dev, "update_firmware_request failed. (%d)\n", ret);
		goto request_firmware_fail;
	}

	/* initial buffer and variable */
	ret = nvt_download_init();
	if (ret) {
		input_err(true, &ts->client->dev, "Download Init failed. (%d)\n", ret);
		goto download_fail;
	}

	/* download firmware process */
	if (ts->hw_crc)
		ret = nvt_download_firmware_hw_crc();
	else
		ret = nvt_download_firmware();
	if (ret) {
		input_err(true, &ts->client->dev, "Download Firmware failed. (%d)\n", ret);
		goto download_fail;
	}

	input_info(true, &ts->client->dev, "%s: Update firmware success! <%lld us>\n",
			__func__, (end - start)*1000000L + (end - start));

	/* Get FW Info */
	ret = nvt_get_fw_info();
	if (ret) {
		input_err(true, &ts->client->dev, "nvt_get_fw_info failed. (%d)\n", ret);
	}

download_fail:
	if (!IS_ERR_OR_NULL(bin_map)) {
		kfree(bin_map);
		bin_map = NULL;
	}
	if (ts->isUMS) {
			input_info(true, &ts->client->dev, "%s UMS Update NO update_firmware_release\n", __func__);
	} else {
		input_info(true, &ts->client->dev, "%s BUILT-IN Update update_firmware_release\n", __func__);
		update_firmware_release();
	}
request_firmware_fail:

	return ret;
}
#if 0
int nvt_ts_fw_update_from_ums(struct nvt_ts_data *ts)
{
	struct file *fp;
	//mm_segment_t old_fs;
	//long fw_size, nread;
	long nread;
	int ret = 0;

	//old_fs = get_fs();
	//set_fs(KERNEL_DS);

	mutex_lock(&ts->lock);

	fp = filp_open(NVT_TS_DEFAULT_UMS_FW, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		input_err(true, &ts->client->dev, "%s: failed to open %s\n",
			__func__, NVT_TS_DEFAULT_UMS_FW);
			mutex_unlock(&ts->lock);
			//set_fs(old_fs);
			return PTR_ERR(fp);
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size > 0) {
		//struct firmware tmp_fw_entry;
		//u8 *fw_data;

		if(fw_data != NULL) {
			vfree(fw_data);
		}

		fw_data = vzalloc(fw_size);
		if (!fw_data) {
			input_err(true, &ts->client->dev, "%s: failed to alloc mem\n", __func__);
			ret = -ENOMEM;
			goto out;
		}

		nread = kernel_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		if (nread != fw_size) {
			input_err(true, &ts->client->dev, "%s: failed to read firmware file, nread %ld Bytes\n",
				__func__, nread);
			ret = -EIO;

		} else {
			input_info(true, &ts->client->dev, "%s: start, file path %s, size %ld Bytes\n",
				__func__, NVT_TS_DEFAULT_UMS_FW, fw_size);
			input_info(true, &ts->client->dev, "%s: firmware version %02X\n",
				__func__, fw_data[FW_BIN_VER_OFFSET]);

//			if (ts->client->irq)
//				disable_irq(ts->client->irq);

			tmp_fw_entry.data = fw_data;
			tmp_fw_entry.size = fw_size;

			fw_entry = &tmp_fw_entry;

//			nvt_ts_sw_reset_idle(ts);

//			ret = nvt_ts_update_firmware(ts);

//			ts->fw_entry = NULL;

//			if (ts->client->irq)
//				enable_irq(ts->client->irq);
			// check FW need to write size
			if (nvt_get_fw_need_write_size(fw_entry)) {
				input_err(true, &ts->client->dev, "get fw need to write size fail!\n");
				ret = -EINVAL;
				goto download_fail;
			}

			// check if FW version add FW version bar equals 0xFF
			if (*(fw_entry->data + FW_BIN_VER_OFFSET) + *(fw_entry->data + FW_BIN_VER_BAR_OFFSET) != 0xFF) {
				input_err(true, &ts->client->dev, "bin file FW_VER + FW_VER_BAR should be 0xFF!\n");
				input_err(true, &ts->client->dev, "FW_VER=0x%02X, FW_VER_BAR=0x%02X\n", *(fw_entry->data+FW_BIN_VER_OFFSET), *(fw_entry->data+FW_BIN_VER_BAR_OFFSET));
				ret = -ENOEXEC;
				goto download_fail;
			}

			/* BIN Header Parser */
			ret = nvt_bin_header_parser(fw_entry->data, fw_entry->size);
			if (ret) {
				input_err(true, &ts->client->dev, "bin header parser failed\n");
				goto download_fail;
			} else {
				ts->fw_ver_bin[0] = fw_entry->data[FW_BIN_IC_ID];
				ts->fw_ver_bin[1] = fw_entry->data[FW_BIN_PROJECT_ID];
				ts->fw_ver_bin[2] = fw_entry->data[FW_BIN_PANEL_ID];
				ts->fw_ver_bin[3] = fw_entry->data[FW_BIN_VER_OFFSET];
				ts->fw_ver_bin_bar = fw_entry->data[FW_BIN_VER_BAR_OFFSET];

				input_info(true, &ts->client->dev, "%s: fw_ver_bin = %02X%02X%02X%02X\n", __func__,
					ts->fw_ver_bin[0], ts->fw_ver_bin[1], ts->fw_ver_bin[2], ts->fw_ver_bin[3]);
//				break;
			}

			/* initial buffer and variable */
			ret = nvt_download_init();
			if (ret) {
				input_err(true, &ts->client->dev, "Download Init failed. (%d)\n", ret);
				goto download_fail;
			}

			/* download firmware process */
			if (ts->hw_crc)
				ret = nvt_download_firmware_hw_crc();
			else
				ret = nvt_download_firmware();
			if (ret) {
				input_err(true, &ts->client->dev, "Download Firmware failed. (%d)\n", ret);
				goto download_fail;
			}

			input_info(true, &ts->client->dev, "Update firmware success! <%lld us>\n",
					(end - start)*1000000L + (end - start));

			/* Get FW Info */
			ret = nvt_get_fw_info();
			if (ret) {
				input_err(true, &ts->client->dev, "nvt_get_fw_info failed. (%d)\n", ret);
			}

download_fail:
			if (!IS_ERR_OR_NULL(bin_map)) {
				kfree(bin_map);
				bin_map = NULL;
			}
//			update_firmware_release();
//request_firmware_fail:
//			return ret;
		}
		//vfree(fw_data);
	}

out:
	mutex_unlock(&ts->lock);

	filp_close(fp, NULL);
	//set_fs(old_fs);

	return ret;
}
#endif
int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts)
{
	int ret = 0;
	mutex_lock(&ts->lock);
	ret = nvt_update_firmware(ts->platdata->firmware_name);
	mutex_unlock(&ts->lock);

	return ret;
}

int nvt_ts_fw_update_from_mp_bin(struct nvt_ts_data *ts, bool is_start)
{
	int ret = 0;
	bool saveIsUMS;

	if (is_start) {
		input_info(true, &ts->client->dev, "isUMS = %s\n", ts->isUMS ? "true but load mp fw" : "false");
		saveIsUMS = ts->isUMS ? true : false;	//save isUMS status
		ts->isUMS = false;
		ret = nvt_update_firmware(ts->platdata->firmware_name_mp);
		ts->isUMS = saveIsUMS ? true : false;	//restore isUMS status
	} else {
		ret = nvt_update_firmware(ts->platdata->firmware_name);
	}
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen update firmware when booting
	function.

return:
	n.a.
*******************************************************/
void Boot_Update_Firmware(struct work_struct *work)
{
#if SEC_TOUCH_CMD
	int ret;
#endif
	ts->isUMS = false;

	mutex_lock(&ts->lock);
	nvt_update_firmware(ts->platdata->firmware_name);
	mutex_unlock(&ts->lock);
#if SEC_TOUCH_CMD
	/* Parsing criteria from dts */
	if(of_property_read_bool(ts->client->dev.of_node, "novatek,mp-support-dt")) {
			u8 mpcriteria[32] = { 0 };
			int pid;

			//---set xdata index to EVENT BUF ADDR---
			nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

			//---read project id---
			mpcriteria[0] = EVENT_MAP_PROJECTID;
			CTP_SPI_READ(ts->client, mpcriteria, 3);

			//---set xdata index to EVENT BUF ADDR---
			nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

			pid = (mpcriteria[2] << 8) + mpcriteria[1];

			/*
			* Parsing Criteria by Novatek PID
			* The string rule is "novatek-mp-criteria-<nvt_pid>"
			* nvt_pid is 2 bytes (show hex).
			*
			* Ex. nvt_pid = 500A
			*	  mpcriteria = "novatek-mp-criteria-500A"
			*/
			snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", pid);

			ret = nvt_sec_mp_parse_dt(ts, mpcriteria);
			if (ret) {
					input_err(true, &ts->client->dev, "%s: failed to parse mp device tree\n",
							__func__);
					//return ret;
			}
	}
#endif
}
#endif /* BOOT_UPDATE_FIRMWARE */
