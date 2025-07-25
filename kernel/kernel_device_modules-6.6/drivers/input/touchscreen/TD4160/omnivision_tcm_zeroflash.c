/*
 * omnivision TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 omnivision Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND omnivision
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL omnivision BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF omnivision WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, omnivision'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/gpio.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include "omnivision_tcm_core.h"

#define USE_OMNIVSION_IMG_FILE 1

#if !USE_OMNIVSION_IMG_FILE
#include "omnivision_firmware_header.h"
#endif
#include "n28_xianxian_module.h"
#include "n28_boe_module.h"

#define fw_size 102400
#define fw_offset 356

#define ENABLE_SYS_ZEROFLASH true

#define FW_IMAGE_NAME "ovt_td4160_fw_boe.img"

#define BOOT_CONFIG_ID "BOOT_CONFIG"

#define F35_APP_CODE_ID "F35_APP_CODE"

#define ROMBOOT_APP_CODE_ID "ROMBOOT_APP_CODE"

#define RESERVED_BYTES 14

#define APP_CONFIG_ID "APP_CONFIG"

#define DISP_CONFIG_ID "DISPLAY"

#define OPEN_SHORT_ID "OPENSHORT"

#define SYSFS_DIR_NAME "zeroflash"

#define IMAGE_FILE_MAGIC_VALUE 0x4818472b

#define FLASH_AREA_MAGIC_VALUE 0x7c05e516

#define PDT_START_ADDR 0x00e9

#define PDT_END_ADDR 0x00ee

#define UBL_FN_NUMBER 0x35

#define F35_CTRL3_OFFSET 18

#define F35_CTRL7_OFFSET 22

#define F35_WRITE_FW_TO_PMEM_COMMAND 4

#define TP_RESET_TO_HDL_DELAY_MS 11

#define DOWNLOAD_RETRY_COUNT 10

char *fw_image_name;

enum f35_error_code {
	SUCCESS = 0,
	UNKNOWN_FLASH_PRESENT,
	MAGIC_NUMBER_NOT_PRESENT,
	INVALID_BLOCK_NUMBER,
	BLOCK_NOT_ERASED,
	NO_FLASH_PRESENT,
	CHECKSUM_FAILURE,
	WRITE_FAILURE,
	INVALID_COMMAND,
	IN_DEBUG_MODE,
	INVALID_HEADER,
	REQUESTING_FIRMWARE,
	INVALID_CONFIGURATION,
	DISABLE_BLOCK_PROTECT_FAILURE,
};

enum config_download {
	HDL_INVALID = 0,
	HDL_TOUCH_CONFIG,
	HDL_DISPLAY_CONFIG,
	HDL_OPEN_SHORT_CONFIG,
};

struct area_descriptor {
	unsigned char magic_value[4];
	unsigned char id_string[16];
	unsigned char flags[4];
	unsigned char flash_addr_words[4];
	unsigned char length[4];
	unsigned char checksum[4];
};

struct block_data {
	const unsigned char *data;
	unsigned int size;
	unsigned int flash_addr;
};

struct image_info {
	unsigned int packrat_number;
	struct block_data boot_config;
	struct block_data app_firmware;
	struct block_data app_config;
	struct block_data disp_config;
	struct block_data open_short_config;
};

struct image_header {
	unsigned char magic_value[4];
	unsigned char num_of_areas[4];
};

struct rmi_f35_query {
	unsigned char version:4;
	unsigned char has_debug_mode:1;
	unsigned char has_data5:1;
	unsigned char has_query1:1;
	unsigned char has_query2:1;
	unsigned char chunk_size;
	unsigned char has_ctrl7:1;
	unsigned char has_host_download:1;
	unsigned char has_spi_master:1;
	unsigned char advanced_recovery_mode:1;
	unsigned char reserved:4;
} __packed;

struct rmi_f35_data {
	unsigned char error_code:5;
	unsigned char recovery_mode_forced:1;
	unsigned char nvm_programmed:1;
	unsigned char in_recovery:1;
} __packed;

struct rmi_pdt_entry {
	unsigned char query_base_addr;
	unsigned char command_base_addr;
	unsigned char control_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count:3;
	unsigned char reserved_1:2;
	unsigned char fn_version:2;
	unsigned char reserved_2:1;
	unsigned char fn_number;
} __packed;

struct rmi_addr {
	unsigned short query_base;
	unsigned short command_base;
	unsigned short control_base;
	unsigned short data_base;
};

struct firmware_status {
	unsigned short invalid_static_config:1;
	unsigned short need_disp_config:1;
	unsigned short need_app_config:1;
	unsigned short hdl_version:4;
	unsigned short need_open_short_config:1;
	unsigned short reserved:8;
} __packed;

struct zeroflash_hcd {
	bool has_hdl;
	bool f35_ready;
	bool has_open_short_config;
	const unsigned char *image;
	unsigned char *buf;
	const struct firmware *fw_entry;
	struct work_struct config_work;
	struct workqueue_struct *workqueue;
	struct kobject *sysfs_dir;
	struct rmi_addr f35_addr;
	struct image_info image_info;
	struct firmware_status fw_status;
	struct ovt_tcm_buffer out;
	struct ovt_tcm_buffer resp;
	struct ovt_tcm_hcd *tcm_hcd;
};

static void zeroflash_do_romboot_firmware_download(void);

DECLARE_COMPLETION(zeroflash_remove_complete);

STORE_PROTOTYPE(zeroflash, hdl)

static struct device_attribute *attrs[] = {
	ATTRIFY(hdl),
};

static struct zeroflash_hcd *zeroflash_hcd;

static int zeroflash_wait_hdl(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;

	msleep(HOST_DOWNLOAD_WAIT_MS);

	if (!atomic_read(&tcm_hcd->host_downloading))
		return 0;

	retval = wait_event_interruptible_timeout(tcm_hcd->hdl_wq,
			!atomic_read(&tcm_hcd->host_downloading),
			msecs_to_jiffies(HOST_DOWNLOAD_TIMEOUT_MS));
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for completion of host download\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		retval = -EIO;
	} else {
		retval = 0;
	}

	return retval;
}

static ssize_t zeroflash_sysfs_hdl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	unsigned int input;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input && (tcm_hcd->in_hdl_mode)) {

		retval = tcm_hcd->reset(tcm_hcd);
		if (retval < 0)
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to trigger the host download by reset\n");

		retval = zeroflash_wait_hdl(tcm_hcd);
		if (retval < 0)
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to wait for completion of host download\n");
#if USE_OMNIVSION_IMG_FILE
		if (zeroflash_hcd->fw_entry) {
			release_firmware(zeroflash_hcd->fw_entry);
			zeroflash_hcd->fw_entry = NULL;
		}
#endif
		zeroflash_hcd->image = NULL;

	} else {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Invalid HDL devices\n");
	}
	return count;
}

static int zeroflash_check_uboot(void)
{
	int retval;
	unsigned char fn_number;
	unsigned int retry = 3;
	struct rmi_f35_query query;
	struct rmi_pdt_entry p_entry;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

re_check:
	retval = ovt_tcm_rmi_read(tcm_hcd,
			PDT_END_ADDR,
			&fn_number,
			sizeof(fn_number));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read RMI function number\n");
		return retval;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"Found F$%02x\n",
			fn_number);

	if (fn_number != UBL_FN_NUMBER) {
		if (retry--)
			goto re_check;

		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to find F$35\n");
		return -ENODEV;
	}

	if (zeroflash_hcd->f35_ready)
		return 0;

	retval = ovt_tcm_rmi_read(tcm_hcd,
			PDT_START_ADDR,
			(unsigned char *)&p_entry,
			sizeof(p_entry));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read PDT entry\n");
		return retval;
	}

	zeroflash_hcd->f35_addr.query_base = p_entry.query_base_addr;
	zeroflash_hcd->f35_addr.command_base = p_entry.command_base_addr;
	zeroflash_hcd->f35_addr.control_base = p_entry.control_base_addr;
	zeroflash_hcd->f35_addr.data_base = p_entry.data_base_addr;

	retval = ovt_tcm_rmi_read(tcm_hcd,
			zeroflash_hcd->f35_addr.query_base,
			(unsigned char *)&query,
			sizeof(query));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read F$35 query\n");
		return retval;
	}

	zeroflash_hcd->f35_ready = true;

	if (query.has_query2 && query.has_ctrl7 && query.has_host_download) {
		zeroflash_hcd->has_hdl = true;
	} else {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Host download not supported\n");
		zeroflash_hcd->has_hdl = false;
		return -ENODEV;
	}

	return 0;
}

static int zeroflash_parse_fw_image(void)
{
	unsigned int idx;
	unsigned int addr;
	unsigned int offset;
	unsigned int length;
	unsigned int checksum;
	unsigned int flash_addr;
	unsigned int magic_value;
	unsigned int num_of_areas;
	struct image_header *header;
	struct image_info *image_info;
	struct area_descriptor *descriptor;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	const unsigned char *image;
	const unsigned char *content;

	image = zeroflash_hcd->image;
	image_info = &zeroflash_hcd->image_info;
	header = (struct image_header *)image;

	magic_value = le4_to_uint(header->magic_value);
	if (magic_value != IMAGE_FILE_MAGIC_VALUE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid image file magic value\n");
		return -EINVAL;
	}

	memset(image_info, 0x00, sizeof(*image_info));

	offset = sizeof(*header);
	num_of_areas = le4_to_uint(header->num_of_areas);

	for (idx = 0; idx < num_of_areas; idx++) {
		addr = le4_to_uint(image + offset);
		descriptor = (struct area_descriptor *)(image + addr);
		offset += 4;

		magic_value = le4_to_uint(descriptor->magic_value);
		if (magic_value != FLASH_AREA_MAGIC_VALUE)
			continue;

		length = le4_to_uint(descriptor->length);
		content = (unsigned char *)descriptor + sizeof(*descriptor);
		flash_addr = le4_to_uint(descriptor->flash_addr_words) * 2;
		checksum = le4_to_uint(descriptor->checksum);

		if (0 == strncmp((char *)descriptor->id_string,
				BOOT_CONFIG_ID, strlen(BOOT_CONFIG_ID))) {

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Boot config checksum error\n");
				return -EINVAL;
			}
			image_info->boot_config.size = length;
			image_info->boot_config.data = content;
			image_info->boot_config.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"Boot config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Boot config flash address = 0x%08x\n",
					flash_addr);
		} else if ((0 == strncmp((char *)descriptor->id_string,
				F35_APP_CODE_ID, strlen(F35_APP_CODE_ID)))) {

			if (tcm_hcd->sensor_type != TYPE_F35) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Improper descriptor, F35_APP_CODE_ID\n");
				return -EINVAL;
			}

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"HDL_F35 firmware checksum error\n");
				return -EINVAL;
			}
			image_info->app_firmware.size = length;
			image_info->app_firmware.data = content;
			image_info->app_firmware.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"HDL_F35 firmware size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"HDL_F35 firmware flash address = 0x%08x\n",
					flash_addr);

		} else if ((0 == strncmp((char *)descriptor->id_string,
				ROMBOOT_APP_CODE_ID,
				strlen(ROMBOOT_APP_CODE_ID)))) {

			if (tcm_hcd->sensor_type != TYPE_ROMBOOT) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Improper descriptor, ROMBOOT_APP_CODE_ID\n");
				return -EINVAL;
			}

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"HDL_ROMBoot firmware checksum error\n");
				return -EINVAL;
			}
			image_info->app_firmware.size = length;
			image_info->app_firmware.data = content;
			image_info->app_firmware.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"HDL_ROMBoot firmware size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"HDL_ROMBoot firmware flash address = 0x%08x\n",
					flash_addr);

		} else if (0 == strncmp((char *)descriptor->id_string,
				APP_CONFIG_ID, strlen(APP_CONFIG_ID))) {

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Application config checksum error\n");
				return -EINVAL;
			}
			image_info->app_config.size = length;
			image_info->app_config.data = content;
			image_info->app_config.flash_addr = flash_addr;
			image_info->packrat_number = le4_to_uint(&content[14]);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application config flash address = 0x%08x\n",
					flash_addr);
		} else if (0 == strncmp((char *)descriptor->id_string,
				DISP_CONFIG_ID, strlen(DISP_CONFIG_ID))) {

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Display config checksum error\n");
				return -EINVAL;
			}
			image_info->disp_config.size = length;
			image_info->disp_config.data = content;
			image_info->disp_config.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"Display config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Display config flash address = 0x%08x\n",
					flash_addr);
		} else if (0 == strncmp((char *)descriptor->id_string,
				OPEN_SHORT_ID, strlen(OPEN_SHORT_ID))) {

			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"open_short config checksum error\n");
				return -EINVAL;
			}
			zeroflash_hcd->has_open_short_config = true;
			image_info->open_short_config.size = length;
			image_info->open_short_config.data = content;
			image_info->open_short_config.flash_addr = flash_addr;
			LOGN(tcm_hcd->pdev->dev.parent,
					"open_short config size = %d\n",
					length);
			LOGN(tcm_hcd->pdev->dev.parent,
					"open_short config flash address = 0x%08x\n",
					flash_addr);
		}
	}

	return 0;
}

static int zeroflash_get_fw_image(void)
{
	int retval;
#if USE_OMNIVSION_IMG_FILE
	int retry_cnt = 2;
#endif
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

#if USE_OMNIVSION_IMG_FILE
	if (zeroflash_hcd->fw_entry != NULL) {
		release_firmware(zeroflash_hcd->fw_entry);
		zeroflash_hcd->fw_entry = NULL;
		zeroflash_hcd->image = NULL;
	}

	if (ovt_lcm_name == 3) {
		fw_image_name = "ovt_td4160_fw.img";
	}else if(ovt_lcm_name == 7) {
		fw_image_name = "ovt_td4160_fw_boe.img";
	}
	LOGN(tcm_hcd->pdev->dev.parent,"fw_image_name=%s\n",fw_image_name);
	LOGN(tcm_hcd->pdev->dev.parent,"lcm_tp_name =%d\n",ovt_lcm_name);

	while(retry_cnt--) {
		retval = request_firmware(&zeroflash_hcd->fw_entry,
				fw_image_name,
				tcm_hcd->pdev->dev.parent);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to request %s, retry_cnt:%d\n",
					fw_image_name, retry_cnt);
			if (retry_cnt == 0) {
				return retval;
			}
			msleep(1000);
		} else {
			break;
		}
	}
	LOGD(tcm_hcd->pdev->dev.parent,
			"Firmware image size = %d\n",
			(unsigned int)zeroflash_hcd->fw_entry->size);

	zeroflash_hcd->image = zeroflash_hcd->fw_entry->data;
#else
	zeroflash_hcd->image = omnivsion_image_array;
#endif

	retval = zeroflash_parse_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse firmware image\n");
#if USE_OMNIVSION_IMG_FILE
		release_firmware(zeroflash_hcd->fw_entry);
		zeroflash_hcd->fw_entry = NULL;
#endif
		zeroflash_hcd->image = NULL;
		return retval;
	}

	return 0;
}

static void zeroflash_download_config(void)
{
	struct firmware_status *fw_status;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	int retval;

	fw_status = &zeroflash_hcd->fw_status;

	if (!fw_status->need_app_config && !fw_status->need_disp_config
			&& !(fw_status->need_open_short_config
			&& zeroflash_hcd->has_open_short_config)
			&& (atomic_read(&tcm_hcd->host_downloading))) {
		atomic_set(&tcm_hcd->host_downloading, 0);
		retval = wait_for_completion_timeout(tcm_hcd->helper.helper_completion,
			msecs_to_jiffies(500));
		if (retval == 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "timeout to wait for helper completion\n");
			return;
		}
		if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
			atomic_set(&tcm_hcd->helper.task,
					HELP_SEND_REINIT_NOTIFICATION);
			queue_work(tcm_hcd->helper.workqueue,
					&tcm_hcd->helper.work);
		}
		return;
	}

	if (atomic_read(&tcm_hcd->host_downloading) && !tcm_hcd->ovt_tcm_driver_removing)
		queue_work(zeroflash_hcd->workqueue,
				&zeroflash_hcd->config_work);

	return;
}

static int zeroflash_download_open_short_config(void)
{
	int retval;
	unsigned char response_code;
	struct image_info *image_info;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading open_short config\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->open_short_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No open_short config in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->open_short_config.size + 2);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for open_short config\n");
		goto unlock_out;
	}

	switch (zeroflash_hcd->fw_status.hdl_version) {
	case 0:
	case 1:
		zeroflash_hcd->out.buf[0] = 1;
		break;
	case 2:
		zeroflash_hcd->out.buf[0] = 2;
		break;
	default:
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid HDL version (%d)\n",
				zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}

	zeroflash_hcd->out.buf[1] = HDL_OPEN_SHORT_CONFIG;

	retval = secure_memcpy(&zeroflash_hcd->out.buf[2],
			zeroflash_hcd->out.buf_size - 2,
			image_info->open_short_config.data,
			image_info->open_short_config.size,
			image_info->open_short_config.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy open_short config data\n");
		goto unlock_out;
	}

	zeroflash_hcd->out.data_length = image_info->open_short_config.size + 2;

	LOCK_BUFFER(zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DOWNLOAD_CONFIG,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length,
			&zeroflash_hcd->resp.buf,
			&zeroflash_hcd->resp.buf_size,
			&zeroflash_hcd->resp.data_length,
			&response_code,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR)
			goto unlock_resp;
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT)
			goto unlock_resp;
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&zeroflash_hcd->fw_status,
			sizeof(zeroflash_hcd->fw_status),
			zeroflash_hcd->resp.buf,
			zeroflash_hcd->resp.buf_size,
			sizeof(zeroflash_hcd->fw_status));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"open_short config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(zeroflash_hcd->out);

	return retval;
}

static int zeroflash_download_disp_config(void)
{
	int retval;
	unsigned char response_code;
	struct image_info *image_info;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading display config\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->disp_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No display config in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->disp_config.size + 2);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for display config\n");
		goto unlock_out;
	}

	switch (zeroflash_hcd->fw_status.hdl_version) {
	case 0:
	case 1:
		zeroflash_hcd->out.buf[0] = 1;
		break;
	case 2:
		zeroflash_hcd->out.buf[0] = 2;
		break;
	default:
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid HDL version (%d)\n",
				zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}

	zeroflash_hcd->out.buf[1] = HDL_DISPLAY_CONFIG;

	retval = secure_memcpy(&zeroflash_hcd->out.buf[2],
			zeroflash_hcd->out.buf_size - 2,
			image_info->disp_config.data,
			image_info->disp_config.size,
			image_info->disp_config.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy display config data\n");
		goto unlock_out;
	}

	zeroflash_hcd->out.data_length = image_info->disp_config.size + 2;

	LOCK_BUFFER(zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DOWNLOAD_CONFIG,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length,
			&zeroflash_hcd->resp.buf,
			&zeroflash_hcd->resp.buf_size,
			&zeroflash_hcd->resp.data_length,
			&response_code,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR)
			goto unlock_resp;
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT)
			goto unlock_resp;
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&zeroflash_hcd->fw_status,
			sizeof(zeroflash_hcd->fw_status),
			zeroflash_hcd->resp.buf,
			zeroflash_hcd->resp.buf_size,
			sizeof(zeroflash_hcd->fw_status));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Display config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(zeroflash_hcd->out);

	return retval;
}

static int zeroflash_download_app_config(void)
{
	int retval;
	unsigned char padding;
	unsigned char response_code;
	struct image_info *image_info;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading application config\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->app_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No application config in image file\n");
		return -EINVAL;
	}

	padding = image_info->app_config.size % 8;
	if (padding)
		padding = 8 - padding;

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->app_config.size + 2 + padding);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for application config\n");
		goto unlock_out;
	}

	switch (zeroflash_hcd->fw_status.hdl_version) {
	case 0:
	case 1:
		zeroflash_hcd->out.buf[0] = 1;
		break;
	case 2:
		zeroflash_hcd->out.buf[0] = 2;
		break;
	default:
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid HDL version (%d)\n",
				zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}

	zeroflash_hcd->out.buf[1] = HDL_TOUCH_CONFIG;

	retval = secure_memcpy(&zeroflash_hcd->out.buf[2],
			zeroflash_hcd->out.buf_size - 2,
			image_info->app_config.data,
			image_info->app_config.size,
			image_info->app_config.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy application config data\n");
		goto unlock_out;
	}

	zeroflash_hcd->out.data_length = image_info->app_config.size + 2;
	zeroflash_hcd->out.data_length += padding;

	LOCK_BUFFER(zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DOWNLOAD_CONFIG,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length,
			&zeroflash_hcd->resp.buf,
			&zeroflash_hcd->resp.buf_size,
			&zeroflash_hcd->resp.data_length,
			&response_code,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR)
			goto unlock_resp;
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT)
			goto unlock_resp;
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&zeroflash_hcd->fw_status,
			sizeof(zeroflash_hcd->fw_status),
			zeroflash_hcd->resp.buf,
			zeroflash_hcd->resp.buf_size,
			sizeof(zeroflash_hcd->fw_status));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Application config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(zeroflash_hcd->out);

	return retval;
}

static void zeroflash_download_config_work(struct work_struct *work)
{
	int retval;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get firmware image\n");
		return;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start of config download\n");

	if (zeroflash_hcd->fw_status.need_app_config) {
		retval = zeroflash_download_app_config();
		if (retval < 0) {
			atomic_set(&tcm_hcd->host_downloading, 0);
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to download application config, abort\n");
			return;
		}
		goto exit;
	}

	if (zeroflash_hcd->fw_status.need_disp_config) {
		retval = zeroflash_download_disp_config();
		if (retval < 0) {
			atomic_set(&tcm_hcd->host_downloading, 0);
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to download display config, abort\n");
			return;
		}
		goto exit;
	}

	if (zeroflash_hcd->fw_status.need_open_short_config &&
			zeroflash_hcd->has_open_short_config) {

		retval = zeroflash_download_open_short_config();
		if (retval < 0) {
			atomic_set(&tcm_hcd->host_downloading, 0);
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to download open_short config, abort\n");
			return;
		}
		goto exit;
	}

exit:
	LOGN(tcm_hcd->pdev->dev.parent,
			"End of config download\n");

    if (tcm_hcd->ovt_tcm_driver_removing == 1) {
		return;
	}

	zeroflash_download_config();

	return;
}

static int zeroflash_download_app_fw(void)
{
	int retval;
	unsigned char command;
	struct image_info *image_info;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
#if TP_RESET_TO_HDL_DELAY_MS
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
#endif

	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading application firmware\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->app_firmware.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No application firmware in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->app_firmware.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for application firmware\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	retval = secure_memcpy(zeroflash_hcd->out.buf,
			zeroflash_hcd->out.buf_size,
			image_info->app_firmware.data,
			image_info->app_firmware.size,
			image_info->app_firmware.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy application firmware data\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	zeroflash_hcd->out.data_length = image_info->app_firmware.size;

	command = F35_WRITE_FW_TO_PMEM_COMMAND;

#if TP_RESET_TO_HDL_DELAY_MS
	if (bdata->tpio_reset_gpio >= 0) {
		gpio_set_value(bdata->tpio_reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->tpio_reset_gpio, !bdata->reset_on_state);
		mdelay(TP_RESET_TO_HDL_DELAY_MS);
	}
#endif

	retval = ovt_tcm_rmi_write(tcm_hcd,
			zeroflash_hcd->f35_addr.control_base + F35_CTRL3_OFFSET,
			&command,
			sizeof(command));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write F$35 command\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	retval = ovt_tcm_rmi_write(tcm_hcd,
			zeroflash_hcd->f35_addr.control_base + F35_CTRL7_OFFSET,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write application firmware data\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(zeroflash_hcd->out);

	LOGN(tcm_hcd->pdev->dev.parent,
			"Application firmware downloaded\n");

	return 0;
}


static void zeroflash_do_f35_firmware_download(void)
{
	int retval;
	struct rmi_f35_data data;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (tcm_hcd->irq_enabled) {
		retval = tcm_hcd->enable_irq(tcm_hcd, false, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable interrupt\n");
		}
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Prepare F35 firmware download\n");

	if (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Incorrect uboot type, exit\n");
		goto exit;
	}
	retval = zeroflash_check_uboot();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to find valid uboot\n");
		goto exit;
	}

	atomic_set(&tcm_hcd->host_downloading, 1);

	retval = ovt_tcm_rmi_read(tcm_hcd,
			zeroflash_hcd->f35_addr.data_base,
			(unsigned char *)&data,
			sizeof(data));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read F$35 data\n");
		goto exit;
	}

	if (data.error_code != REQUESTING_FIRMWARE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Microbootloader error code = 0x%02x\n",
				data.error_code);
		if (data.error_code != CHECKSUM_FAILURE) {
			retval = -EIO;
			goto exit;
		} else {
			retry_count++;
		}
	} else {
		retry_count = 0;
	}

	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get firmware image\n");
		goto exit;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start of firmware download\n");

	/* perform firmware downloading */
	retval = zeroflash_download_app_fw();
	if (retval < 0) {
        
		
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to download application firmware, so reset tp \n");
		goto exit;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"End of firmware download\n");

exit:
	if (retval < 0) {
        gpio_set_value(bdata->reset_gpio, 0);
        msleep(5);
        gpio_set_value(bdata->reset_gpio, 1);        
        msleep(5);
		retry_count++;
    }

	if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT) {
		//retval = tcm_hcd->enable_irq(tcm_hcd, false, true);
		retval = tcm_hcd->enable_irq(tcm_hcd, true, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable interrupt\n");
		}

		LOGD(tcm_hcd->pdev->dev.parent,
				"Interrupt is disabled\n");
	} else {
		retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable interrupt\n");
		}
	}

	return;
}

static void zeroflash_do_romboot_firmware_download(void)
{
	bool get_fw_h = false;
	int retval;
	unsigned char *resp_buf = NULL;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int data_size_blocks;
	unsigned int image_size;
	struct ovt_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Prepare ROMBOOT firmware download\n");

	atomic_set(&tcm_hcd->host_downloading, 1);
	resp_buf = NULL;
	resp_buf_size = 0;

	if (!tcm_hcd->irq_enabled) {
		retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable interrupt\n");
		}
	}

	pm_stay_awake(&tcm_hcd->pdev->dev);

	if (tcm_hcd->id_info.mode != MODE_ROMBOOTLOADER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Not in romboot mode\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		goto exit;
	}

	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to request romboot.img\n");
		get_fw_h = 1;
	}

	if (get_fw_h) {
		image_size = fw_size;
	}else{
		image_size = (unsigned int)zeroflash_hcd->image_info.app_firmware.size;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"image_size = %d\n",
			image_size);

	data_size_blocks = image_size / 16;

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_size + RESERVED_BYTES);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for application firmware\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		goto exit;
	}
        if (get_fw_h) {
			zeroflash_hcd->out.buf[0] = 0x1;
			if (ovt_lcm_name == 3){
				retval = secure_memcpy(&zeroflash_hcd->out.buf[RESERVED_BYTES],
					fw_size,
					xinxian_module_fw + fw_offset,
					fw_size,
					fw_size);
			}else{
				retval = secure_memcpy(&zeroflash_hcd->out.buf[RESERVED_BYTES],
					fw_size,
					boe_module_fw + fw_offset,
					fw_size,
					fw_size);
			}
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to copy application firmware data\n");
					UNLOCK_BUFFER(zeroflash_hcd->out);
					goto exit;
                }
		}else{
			zeroflash_hcd->out.buf[0] = zeroflash_hcd->image_info.app_firmware.size >> 16;

			retval = secure_memcpy(&zeroflash_hcd->out.buf[RESERVED_BYTES],
					zeroflash_hcd->image_info.app_firmware.size,
					zeroflash_hcd->image_info.app_firmware.data,
					zeroflash_hcd->image_info.app_firmware.size,
					zeroflash_hcd->image_info.app_firmware.size);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to copy application firmware data\n");
				UNLOCK_BUFFER(zeroflash_hcd->out);
				goto exit;
			}
		}
	LOGD(tcm_hcd->pdev->dev.parent,
			"data_size_blocks: %d\n",
			data_size_blocks);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ROMBOOT_DOWNLOAD,
			zeroflash_hcd->out.buf,
			image_size + RESERVED_BYTES,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			20);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command ROMBOOT DOWNLOAD");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		if (tcm_hcd->status_report_code != REPORT_IDENTIFY) {
			gpio_set_value(bdata->reset_gpio, 0);
			msleep(5);
			gpio_set_value(bdata->reset_gpio, 1);
			msleep(5);
		}
		goto exit;
	}
	UNLOCK_BUFFER(zeroflash_hcd->out);

	retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_BOOTLOADER);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to switch to bootloader");
		if (tcm_hcd->status_report_code != REPORT_IDENTIFY) {
			gpio_set_value(bdata->reset_gpio, 0);
			msleep(5);
			gpio_set_value(bdata->reset_gpio, 1);
			msleep(5);
		}
		goto exit;
	}

exit:

	pm_relax(&tcm_hcd->pdev->dev);

	kfree(resp_buf);

	return;
}

static int zeroflash_init(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval = 0;
	int idx;

	zeroflash_hcd = NULL;
	if (!(tcm_hcd->in_hdl_mode))
		return 0;

	zeroflash_hcd = kzalloc(sizeof(*zeroflash_hcd), GFP_KERNEL);
	if (!zeroflash_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for zeroflash_hcd\n");
		return -ENOMEM;
	}

	zeroflash_hcd->tcm_hcd = tcm_hcd;
	zeroflash_hcd->image = NULL;
	zeroflash_hcd->has_hdl = false;
	zeroflash_hcd->f35_ready = false;
	zeroflash_hcd->has_open_short_config = false;

	INIT_BUFFER(zeroflash_hcd->out, false);
	INIT_BUFFER(zeroflash_hcd->resp, false);

	zeroflash_hcd->workqueue =
			create_singlethread_workqueue("ovt_tcm_zeroflash");
	INIT_WORK(&zeroflash_hcd->config_work,
			zeroflash_download_config_work);

	if (ENABLE_SYS_ZEROFLASH == false)
		goto init_finished;

	zeroflash_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!zeroflash_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		return -EINVAL;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		//retval = sysfs_create_file(zeroflash_hcd->sysfs_dir,
		retval = sysfs_create_file(&tcm_hcd->pdev->dev.kobj,	//default path
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
		}
	}

init_finished:
	/* prepare the firmware download process */
	if (tcm_hcd->in_hdl_mode) {
		switch (tcm_hcd->sensor_type) {
		case TYPE_F35:
			zeroflash_do_f35_firmware_download();
			break;
		case TYPE_ROMBOOT:
			zeroflash_do_romboot_firmware_download();
			break;
		default:
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to find valid HDL state (%d)\n",
					 tcm_hcd->sensor_type);
			break;

		}
	}
	return retval;
}

static int zeroflash_remove(struct ovt_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!zeroflash_hcd)
		goto exit;

	if (ENABLE_SYS_ZEROFLASH == true) {

		for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
			sysfs_remove_file(zeroflash_hcd->sysfs_dir,
					&(*attrs[idx]).attr);
		}

		kobject_put(zeroflash_hcd->sysfs_dir);
	}


	cancel_work_sync(&zeroflash_hcd->config_work);
	flush_workqueue(zeroflash_hcd->workqueue);
	destroy_workqueue(zeroflash_hcd->workqueue);

	RELEASE_BUFFER(zeroflash_hcd->resp);
	RELEASE_BUFFER(zeroflash_hcd->out);
#if USE_OMNIVSION_IMG_FILE
	if (zeroflash_hcd->fw_entry)
		release_firmware(zeroflash_hcd->fw_entry);
#endif
	kfree(zeroflash_hcd);
	zeroflash_hcd = NULL;

exit:
	complete(&zeroflash_remove_complete);

	return 0;
}

static int zeroflash_syncbox(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *fw_status;

	if (!zeroflash_hcd)
		return 0;

	switch (tcm_hcd->report.id) {
	case REPORT_STATUS:
		fw_status = (unsigned char *)&zeroflash_hcd->fw_status;

		retval = secure_memcpy(fw_status,
				sizeof(zeroflash_hcd->fw_status),
				tcm_hcd->report.buffer.buf,
				tcm_hcd->report.buffer.buf_size,
				sizeof(zeroflash_hcd->fw_status));

		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy firmware status\n");
			return retval;
		}
		zeroflash_download_config();
		break;
	case REPORT_HDL_F35:
		zeroflash_do_f35_firmware_download();
		break;
	case REPORT_HDL_ROMBOOT:
		zeroflash_do_romboot_firmware_download();
		break;

	default:
		break;
	}

	return 0;
}

static int zeroflash_reinit(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!zeroflash_hcd && tcm_hcd->in_hdl_mode) {
		retval = zeroflash_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static struct ovt_tcm_module_cb zeroflash_module = {
	.type = TCM_ZEROFLASH,
	.init = zeroflash_init,
	.remove = zeroflash_remove,
	.syncbox = zeroflash_syncbox,
#ifdef REPORT_NOTIFIER
	.asyncbox = NULL,
#endif
	.reinit = zeroflash_reinit,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

int  zeroflash_module_init(void)
{
	return ovt_tcm_add_module(&zeroflash_module, true);
}

void  zeroflash_module_exit(void)
{
	ovt_tcm_add_module(&zeroflash_module, false);

	wait_for_completion(&zeroflash_remove_complete);

	return;
}

EXPORT_SYMBOL(zeroflash_module_init);
EXPORT_SYMBOL(zeroflash_module_exit);

