// SPDX-License-Identifier: GPL-2.0
/*
 * Synaptics TouchCom touchscreen driver
 *
 * Copyright (C) 2017-2020 Synaptics Incorporated. All rights reserved.
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
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

/**
 * @file syna_tcm2_sysfs.c
 *
 * This file implements cdev and ioctl interface in the reference driver.
 */

#include <linux/string.h>

#include "syna_tcm2.h"
#include "tcm/synaptics_touchcom_core_dev.h"
#include "tcm/synaptics_touchcom_func_base.h"
#include "tcm/synaptics_touchcom_func_touch.h"
#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_TESTING
#include "syna_tcm2_testing.h"
#endif


/* #define ENABLE_PID_TASK */

#define SIG_ATTN (46)

/* structure for IOCTLs
 */
struct syna_ioctl_data {
	unsigned int data_length;
	unsigned int buf_size;
	unsigned char __user *buf;
};

#if defined(CONFIG_COMPAT) && defined(HAVE_COMPAT_IOCTL)
struct syna_tcm_ioctl_data_compat {
	unsigned int data_length;
	unsigned int buf_size;
	compat_uptr_t __user *buf;
};
#endif

/* defines the IOCTLs supported
 */
#define IOCTL_MAGIC 's'

/* Previous IOCTLs in early driver */
#define OLD_RESET_ID		(0x00)
#define OLD_SET_IRQ_MODE_ID	(0x01)
#define OLD_SET_RAW_MODE_ID	(0x02)
#define OLD_CONCURRENT_ID	(0x03)

#define IOCTL_OLD_RESET \
	_IO(IOCTL_MAGIC, OLD_RESET_ID)
#define IOCTL_OLD_SET_IRQ_MODE \
	_IOW(IOCTL_MAGIC, OLD_SET_IRQ_MODE_ID, int)
#define IOCTL_OLD_SET_RAW_MODE \
	_IOW(IOCTL_MAGIC, OLD_SET_RAW_MODE_ID, int)
#define IOCTL_OLD_CONCURRENT \
	_IOW(IOCTL_MAGIC, OLD_CONCURRENT_ID, int)

/* Standard IOCTLs in TCM2 driver */
#define STD_IOCTL_BEGIN		    (0x10)
#define STD_SET_PID_ID		    (0x11)
#define STD_ENABLE_IRQ_ID	    (0x12)
#define STD_RAW_READ_ID		    (0x13)
#define STD_RAW_WRITE_ID	    (0x14)
#define STD_GET_FRAME_ID	    (0x15)
#define STD_SEND_MESSAGE_ID     (0x16)
#define STD_SET_REPORTS_ID      (0x17)
#define STD_CHECK_FRAMES_ID     (0x18)
#define STD_CLEAN_OUT_FRAMES_ID (0x19)

#define IOCTL_STD_IOCTL_BEGIN \
	_IOR(IOCTL_MAGIC, STD_IOCTL_BEGIN)
#define IOCTL_STD_SET_PID \
	_IOW(IOCTL_MAGIC, STD_SET_PID_ID, struct syna_ioctl_data *)
#define IOCTL_STD_ENABLE_IRQ \
	_IOW(IOCTL_MAGIC, STD_ENABLE_IRQ_ID, struct syna_ioctl_data *)
#define IOCTL_STD_RAW_READ \
	_IOR(IOCTL_MAGIC, STD_RAW_READ_ID, struct syna_ioctl_data *)
#define IOCTL_STD_RAW_WRITE \
	_IOW(IOCTL_MAGIC, STD_RAW_WRITE_ID, struct syna_ioctl_data *)
#define IOCTL_STD_GET_FRAME \
	_IOWR(IOCTL_MAGIC, STD_GET_FRAME_ID, struct syna_ioctl_data *)
#define IOCTL_STD_SEND_MESSAGE \
	_IOWR(IOCTL_MAGIC, STD_SEND_MESSAGE_ID, struct syna_ioctl_data *)
#define IOCTL_STD_SET_REPORT_TYPES \
	_IOW(IOCTL_MAGIC, STD_SET_REPORTS_ID, struct syna_ioctl_data *)
#define IOCTL_STD_CHECK_FRAMES \
	_IOWR(IOCTL_MAGIC, STD_CHECK_FRAMES_ID, struct syna_ioctl_data *)
#define IOCTL_STD_CLEAN_OUT_FRAMES \
	_IOWR(IOCTL_MAGIC, STD_CLEAN_OUT_FRAMES_ID, struct syna_ioctl_data *)

/* g_sysfs_dir represents the root directory of sysfs nodes being created
 */
static struct kobject *g_sysfs_dir;

/* g_extif_mutex is used to protect the access from the userspace application
 */
static syna_pal_mutex_t g_extif_mutex;

/* g_cdev_buf is a temporary buffer storing the data from userspace
 */
static struct tcm_buffer g_cdev_cbuf;

/* g_fifo_queue_mutex is used to protect the access from
 * the userspace application
 */
static syna_pal_mutex_t g_fifo_queue_mutex;

/* The g_sysfs_io_polling_interval is used to set the polling interval
 * for syna_tcm_send_command from syna_cdev_ioctl_send_message.
 * It will set to the mode SYSFS_FULL_INTERRUPT for using the full
 * interrupt mode. The way to update this variable is through the
 * syna_cdev_ioctl_enable_irq.
 */
unsigned int g_sysfs_io_polling_interval;

/* 3 sec buffer for 300 FPS
 * consider touch report and one the other reports may be co-enabled
 * at the same time, give a little buffer here
 */
#define FIFO_QUEUE_MAX_FRAMES		(1200)
#define SEND_MESSAGE_HEADER_LENGTH	(3)

/* I/O mode */
#define SYSFS_DISABLED_INTERRUPT		(0)
#define SYSFS_GENERIC_INTERRUPT			(1)
#define SYSFS_FULL_INTERRUPT			(FORCE_ATTN_DRIVEN)

/*Define a data structure that contains a list_head*/
struct fifo_queue {
	struct list_head next;
	unsigned char *fifo_data;
	unsigned int data_length;
	struct timeval timestamp;
};


/**
 * syna_sysfs_info_show()
 *
 * Attribute to show the device and driver information to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_sysfs_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;
	struct tcm_dev *tcm_dev;
	int i;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);
	tcm_dev = tcm->tcm_dev;

	syna_pal_mutex_lock(&g_extif_mutex);

	count = 0;

	if (!tcm->is_connected) {
		retval = snprintf(buf, PAGE_SIZE - count,
				"Device is NOT connected\n");
		goto exit;
	}


	retval = snprintf(buf, PAGE_SIZE - count,
			"TouchComm version:  %d\n", tcm_dev->id_info.version);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Driver version:     %d.%d.%d\n",
			(unsigned char)(SYNAPTICS_TCM_DRIVER_VERSION >> 8),
			(unsigned char)SYNAPTICS_TCM_DRIVER_VERSION,
			(unsigned char)SYNAPTICS_TCM_DRIVER_SUBVER);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Core lib version:   %d.%02d\n",
			(unsigned char)(SYNA_TCM_CORE_LIB_VERSION >> 8),
			(unsigned char)SYNA_TCM_CORE_LIB_VERSION);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	switch (tcm_dev->id_info.mode) {
	case MODE_APPLICATION_FIRMWARE:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Application Firmware, 0x%02x\n",
				tcm_dev->id_info.mode);
		if (retval < 0)
			goto exit;
		break;
	case MODE_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Bootloader, 0x%02x\n",
				tcm_dev->id_info.mode);
		if (retval < 0)
			goto exit;
		break;
	case MODE_ROMBOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Rom Bootloader, 0x%02x\n",
				tcm_dev->id_info.mode);
		if (retval < 0)
			goto exit;
		break;
	default:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Mode 0x%02x\n",
				tcm_dev->id_info.mode);
		if (retval < 0)
			goto exit;
		break;
	}
	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Part number:        ");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = syna_pal_mem_cpy(buf,
			PAGE_SIZE - count,
			tcm_dev->id_info.part_number,
			sizeof(tcm_dev->id_info.part_number),
			sizeof(tcm_dev->id_info.part_number));
	if (retval < 0) {
		LOGE("Fail to copy part number string\n");
		goto exit;
	}
	buf += sizeof(tcm_dev->id_info.part_number);
	count += sizeof(tcm_dev->id_info.part_number);

	retval = snprintf(buf, PAGE_SIZE - count, "\n");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Packrat number:     %d\n\n", tcm_dev->packrat_number);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	if (tcm_dev->id_info.mode != MODE_APPLICATION_FIRMWARE) {
		retval = count;
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE - count, "Config ID:          ");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	for (i = 0; i < MAX_SIZE_CONFIG_ID; i++) {
		retval = snprintf(buf, PAGE_SIZE - count,
			"0x%2x ", tcm_dev->config_id[i]);
		if (retval < 0)
			goto exit;
		buf += retval;
		count += retval;
	}

	retval = snprintf(buf, PAGE_SIZE - count, "\n");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"Max X & Y:          %d, %d\n", tcm_dev->max_x, tcm_dev->max_y);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"Num of objects:     %d\n", tcm_dev->max_objects);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"Num of cols & rows: %d, %d\n", tcm_dev->cols, tcm_dev->rows);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = count;

exit:
	syna_pal_mutex_unlock(&g_extif_mutex);
	return retval;
}

static struct kobj_attribute kobj_attr_info =
	__ATTR(info, 0444, syna_sysfs_info_show, NULL);

/**
 * syna_sysfs_irq_en_store()
 *
 * Attribute to disable/enable the irq
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [ in] buf:   string buffer input
 *    [ in] count: size of buffer input
 *
 * @return
 *    on success, return count; otherwise, return error code
 */
static ssize_t syna_sysfs_irq_en_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	if (!tcm->hw_if->ops_enable_irq)
		return 0;

	if (!tcm->is_connected) {
		LOGW("Device is NOT connected\n");
		retval = count;
		goto exit;
	}

	syna_pal_mutex_lock(&g_extif_mutex);

	/* disable the interrupt line */
	if (input == 0) {
		retval = tcm->hw_if->ops_enable_irq(tcm->hw_if, false);
		if (retval < 0) {
			LOGE("Fail to disable interrupt\n");
			goto exit;
		}
	} else if (input == 1) {
	/* enable the interrupt line */
		retval = tcm->hw_if->ops_enable_irq(tcm->hw_if, true);
		if (retval < 0) {
			LOGE("Fail to enable interrupt\n");
			goto exit;
		}
	} else {
		retval = -EINVAL;
		goto exit;
	}

	retval = count;

exit:
	syna_pal_mutex_unlock(&g_extif_mutex);
	return retval;
}

static struct kobj_attribute kobj_attr_irq_en =
	__ATTR(irq_en, 0220, NULL, syna_sysfs_irq_en_store);

/**
 * syna_sysfs_reset_store()
 *
 * Attribute to issue a reset.
 * "1" for a sw reset; "2" for a hardware reset
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [ in] buf:   string buffer input
 *    [ in] count: size of buffer input
 *
 * @return
 *    on success, return count; otherwise, return error code
 */
static ssize_t syna_sysfs_reset_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	if (!tcm->is_connected) {
		LOGW("Device is NOT connected\n");
		retval = count;
		goto exit;
	}

	syna_pal_mutex_lock(&g_extif_mutex);

	if (input == 1) {
		retval = syna_tcm_reset(tcm->tcm_dev);
		if (retval < 0) {
			LOGE("Fail to do reset\n");
			goto exit;
		}
	} else if (input == 2) {
		if (!tcm->hw_if->ops_hw_reset)
			goto exit;

		tcm->hw_if->ops_hw_reset(tcm->hw_if);
	} else {
		goto exit;
	}

	/* check the fw setup in case the settings is changed */
	if (IS_APP_FW_MODE(tcm->tcm_dev->dev_mode)) {
		retval = tcm->dev_set_up_app_fw(tcm);
		if (retval < 0) {
			LOGE("Fail to set up app fw\n");
			goto exit;
		}
	}

	retval = count;

exit:
	syna_pal_mutex_unlock(&g_extif_mutex);
	return retval;
}

static struct kobj_attribute kobj_attr_reset =
	__ATTR(reset, 0220, NULL, syna_sysfs_reset_store);

/**
 * syna_sysfs_enable_store()
 *
 * Attribute to enable/disable the input device
 * "1"to  enable input device; "0" to disable
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [ in] buf:   string buffer input
 *    [ in] count: size of buffer input
 *
 * @return
 *    on success, return count; otherwise, return error code
 */
static ssize_t syna_sysfs_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	unsigned int input;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;
	bool support_ear_detect;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);
	support_ear_detect = tcm->hw_if->bdata.support_ear_detect;

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	if (input == 1) {
		if (tcm->lpwg_enabled || support_ear_detect) {
			retval = tcm->dev_set_normal_sensing(tcm);
			if (retval < 0)
				return retval;
		}
	} else if (input == 0) {
		if (tcm->lpwg_enabled || support_ear_detect) {
			retval = tcm->dev_set_lowpwr_sensing(tcm);
			if (retval < 0)
				return retval;
		}
	} else {
		LOGE("Un-Supported value: %d\n", input);
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute kobj_attr_enable =
	__ATTR(enable, 0220, NULL, syna_sysfs_enable_store);

/**
 * syna_sysfs_ownership_show()
 *
 * Attribute to show whether the device is connected
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_sysfs_ownership_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", (tcm->is_connected)?"1":"0");
}

static struct kobj_attribute kobj_attr_ownership =
	__ATTR(ownership, 0444, syna_sysfs_ownership_show, NULL);

/**
 * syna_sysfs_check_touch_count_show()
 *
 * Attribute to show the current touch_count for debugging
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_sysfs_check_touch_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	return snprintf(buf, PAGE_SIZE, "touch count: %d\n", tcm->touch_count);
}

static struct kobj_attribute kobj_attr_check_touch_count =
	__ATTR(check_touch_count, 0444, syna_sysfs_check_touch_count_show, NULL);


/**
 * declaration of sysfs attributes
 */
static struct attribute *attrs[] = {
	&kobj_attr_info.attr,
	&kobj_attr_irq_en.attr,
	&kobj_attr_reset.attr,
	&kobj_attr_enable.attr,
	&kobj_attr_ownership.attr,
	&kobj_attr_check_touch_count.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

/**
 * syna_sysfs_create_dir()
 *
 * Create a directory and register it with sysfs.
 * Then, create all defined sysfs files.
 *
 * @param
 *    [ in] tcm:  the driver handle
 *    [ in] pdev: an instance of platform device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_sysfs_create_dir(struct syna_tcm *tcm,
		struct platform_device *pdev)
{
	int retval = 0;

	g_sysfs_dir = kobject_create_and_add("sysfs",
			&pdev->dev.kobj);
	if (!g_sysfs_dir) {
		LOGE("Fail to create sysfs directory\n");
		return -EINVAL;
	}

	tcm->sysfs_dir = g_sysfs_dir;

	retval = sysfs_create_group(g_sysfs_dir, &attr_group);
	if (retval < 0) {
		LOGE("Fail to create sysfs group\n");

		kobject_put(tcm->sysfs_dir);
		return retval;
	}

#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_TESTING
	retval = syna_testing_create_dir(tcm, g_sysfs_dir);
	if (retval < 0) {
		LOGE("Fail to create testing sysfs\n");

		sysfs_remove_group(tcm->sysfs_dir, &attr_group);
		kobject_put(tcm->sysfs_dir);
		return retval;
	}
#endif
#if 0
	retval = syna_sec_fn_create_dir(tcm, g_sysfs_dir);
	if (retval < 0) {
		LOGE("Fail to create sec testing sysfs\n");

		sysfs_remove_group(tcm->sysfs_dir, &attr_group);
		kobject_put(tcm->sysfs_dir);
		return retval;
	}
#endif
	return 0;
}
/**
 * syna_sysfs_remove_dir()
 *
 * Remove the allocate sysfs directory
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
void syna_sysfs_remove_dir(struct syna_tcm *tcm)
{
	if (!tcm) {
		LOGE("Invalid tcm device handle\n");
		return;
	}

	if (tcm->sysfs_dir) {
#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_TESTING
		syna_testing_remove_dir();
#endif

		sysfs_remove_group(tcm->sysfs_dir, &attr_group);

		kobject_put(tcm->sysfs_dir);
	}

}
#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
/**
 * syna_cdev_insert_fifo()
 *
 * Insert/Push the data to the queue.
 *
 * This function is called by syna_cdev_update_report_queue(),
 * where the event data will be placed as the below format in byte
 * and use this function to store the data in queue.
 *     [0        ] : status / report code
 *     [1 :   2  ] : length of data frame
 *     [3 : N + 3] : N bytes of data payload
 *
 * @param
 *    [ in] tcm:      the driver handle
 *    [ in] buf_ptr:  points to a data going to push
 *    [ in] length:   data length
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_insert_fifo(struct syna_tcm *tcm,
		unsigned char *buf_ptr, unsigned int length)
{
	int retval = 0;
	struct fifo_queue *pfifo_data;
	struct fifo_queue *pfifo_data_temp;
	static int pre_remaining_frames = -1;

	syna_pal_mutex_lock(&g_fifo_queue_mutex);

	/* check queue buffer limit */
	if (tcm->fifo_remaining_frame >= FIFO_QUEUE_MAX_FRAMES) {
		if (tcm->fifo_remaining_frame != pre_remaining_frames)
			LOGI("Reached %d and drop FIFO first frame\n",
				tcm->fifo_remaining_frame);

		pfifo_data_temp = list_first_entry(&tcm->frame_fifo_queue,
						struct fifo_queue, next);

		list_del(&pfifo_data_temp->next);
		kfree(pfifo_data_temp->fifo_data);
		kfree(pfifo_data_temp);
		pre_remaining_frames = tcm->fifo_remaining_frame;
		tcm->fifo_remaining_frame--;
	} else if (pre_remaining_frames >= FIFO_QUEUE_MAX_FRAMES) {
		LOGI("Reached limit, dropped oldest frame, remaining:%d\n",
			tcm->fifo_remaining_frame);
		pre_remaining_frames = tcm->fifo_remaining_frame;
	}

	pfifo_data = kmalloc(sizeof(*pfifo_data), GFP_KERNEL);
	if (!(pfifo_data)) {
		LOGE("Failed to allocate memory\n");
		LOGE("Allocation size = %zu\n", (sizeof(*pfifo_data)));
		retval = -ENOMEM;
		goto exit;
	}

	pfifo_data->fifo_data = kmalloc(length, GFP_KERNEL);
	if (!(pfifo_data->fifo_data)) {
		LOGE("Failed to allocate memory, size = %d\n", length);
		retval = -ENOMEM;
		goto exit;
	}

	pfifo_data->data_length = length;

	memcpy((void *)pfifo_data->fifo_data, (void *)buf_ptr, length);
	do_gettimeofday(&(pfifo_data->timestamp));
	/* append the data to the tail for FIFO queueing */
	list_add_tail(&pfifo_data->next, &tcm->frame_fifo_queue);
	tcm->fifo_remaining_frame++;
	retval = 0;

exit:
	syna_pal_mutex_unlock(&g_fifo_queue_mutex);
	return retval;
}
#endif
/**
 * syna_cdev_ioctl_check_frame()
 *
 * Check the queuing status and wait for the data if it's empty.
 *
 * @param
 *    [ in] tcm:       the driver handle
 *    [ in/out] ubuf_ptr:  points to a memory space from userspace
 *    [ in] buf_size:  size of given space
 *    [ in] data_size: timeout value for queue waiting
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_check_frame(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int data_size)
{
	int retval = 0;
	int result = 0;
	unsigned int timeout = 0;
	unsigned int frames = 0;
	unsigned char data[4] = {0};

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if (buf_size < 4 || data_size < 4) {
		LOGE("Invalid sync data size, buf_size: %u\n", buf_size);
		retval = -EINVAL;
		goto exit;
	}

	result = copy_from_user(data, ubuf_ptr,
			sizeof(data));
	if (result) {
		LOGE("Fail to copy data from user space\n");
		retval = -EIO;
		goto exit;
	}

	/* Parese the waiting duration length */
	timeout = syna_pal_le4_to_uint(&data[0]);
	LOGD("Time out: %d\n", timeout);

	if (list_empty(&tcm->frame_fifo_queue)) {
		LOGD("The queue is empty, wait for the frames\n");
		result = wait_event_interruptible_timeout(tcm->wait_frame,
				(tcm->fifo_remaining_frame > 0),
				msecs_to_jiffies(timeout));
		if (result == 0) {
			LOGD("Queue waiting timed out after %dms\n", timeout);
			retval = -ETIMEDOUT;
			goto exit;
		}
		LOGD("Data queued\n");
		retval = data_size;
	} else {
		LOGD("Queue is not empty\n");
		retval = data_size;
	}

exit:
	if (retval != -EIO) {
		frames = tcm->fifo_remaining_frame;
		data[0] = (unsigned char)(frames & 0xff);
		data[1] = (unsigned char)((frames >> 8) & 0xff);
		data[2] = (unsigned char)((frames >> 16) & 0xff);
		data[3] = (unsigned char)((frames >> 24) & 0xff);
		result = copy_to_user((void *)ubuf_ptr,
				data, sizeof(data));
		if (result) {
			LOGE("Fail to copy data to user space\n");
			retval = -EIO;
		}
	}

	return retval;
}

/**
 * syna_cdev_clean_queue()
 *
 * Clean the data queue.
 * All data in the queue will be cleaned up in every time of device
 * open and close.
 *
 * @param
 *    [ in] tcm:       the driver handle
 *
 * @return
 *    void.
 */
static void syna_cdev_clean_queue(struct syna_tcm *tcm)
{
	struct fifo_queue *pfifo_data;

	syna_pal_mutex_lock(&g_fifo_queue_mutex);

	while (!list_empty(&tcm->frame_fifo_queue)) {
		pfifo_data = list_first_entry(&tcm->frame_fifo_queue,
				struct fifo_queue, next);
		list_del(&pfifo_data->next);
		kfree(pfifo_data->fifo_data);
		kfree(pfifo_data);
		if (tcm->fifo_remaining_frame != 0)
			tcm->fifo_remaining_frame--;
	}

	LOGD("Queue cleaned, frame: %d\n", tcm->fifo_remaining_frame);

	syna_pal_mutex_unlock(&g_fifo_queue_mutex);
}
/**
 * syna_cdev_ioctl_get_frame()
 *
 * Read the data from the queue and return to userspace if data is
 * copied or the specified timeout is expired.
 *
 * Please be noted that the retried data is formatted as follows.
 *     [0        ] : status / report code
 *     [1 :   2  ] : length of data frame
 *     [3 : N + 3] : N bytes of data payload
 *
 * @param
 *    [ in] tcm:           the driver handle
 *    [in/out] ubuf_ptr:   points to a memory space from userspace
 *    [ in] buf_size:      size of given space
 *    [out] frame_size:    frame size returned
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_get_frame(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int *frame_size)
{
	int retval = 0;
	int timeout = 0;
	unsigned char timeout_data[4] = {0};
	struct fifo_queue *pfifo_data;

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if (buf_size < 4) {
		LOGE("Invalid sync data size, buf_size:%d\n", buf_size);
		retval = -EINVAL;
		goto exit;
	}

#if !defined(ENABLE_EXTERNAL_FRAME_PROCESS)
	LOGE("ENABLE_EXTERNAL_FRAME_PROCESS is not enabled\n");
	return -EINVAL;
#endif

	retval = copy_from_user(timeout_data, ubuf_ptr, sizeof(timeout_data));
	if (retval) {
		LOGE("Fail to copy data from user space, size:%d\n", retval);
		retval = -EIO;
		goto exit;
	}

	/* get the waiting duration */
	timeout = syna_pal_le4_to_uint(&timeout_data[0]);
	LOGD("Wait time: %dms\n", timeout);

	if (list_empty(&tcm->frame_fifo_queue)) {
		LOGD("The queue is empty, wait for the frame\n");
		retval = wait_event_interruptible_timeout(tcm->wait_frame,
				(tcm->fifo_remaining_frame > 0),
				msecs_to_jiffies(timeout));
		if (retval == 0) {
			LOGD("Queue waiting timed out after %dms\n", timeout);
			retval = -ETIMEDOUT;
			*frame_size = 0;
			goto exit;
		}
		LOGD("Data queued\n");
	}

    /* confirm the queue status */
	if (list_empty(&tcm->frame_fifo_queue)) {
		LOGD("Is queue empty? The remaining frame = %d\n",
			tcm->fifo_remaining_frame);
		retval = -ENODATA;
		goto exit;
	}

	syna_pal_mutex_lock(&g_fifo_queue_mutex);

	pfifo_data = list_first_entry(&tcm->frame_fifo_queue,
			struct fifo_queue, next);

	LOGD("Pop data from the queue, data length = %d\n",
		pfifo_data->data_length);

	if (buf_size >= pfifo_data->data_length) {
		retval = copy_to_user((void *)ubuf_ptr,
				pfifo_data->fifo_data,
				pfifo_data->data_length);
		if (retval) {
			LOGE("Fail to copy data to user space, size:%d\n",
				retval);
			retval = -EIO;
		}

		*frame_size = pfifo_data->data_length;

	} else {
		LOGE("No enough space for data copy, buf_size:%d data:%d\n",
			buf_size, pfifo_data->data_length);
		retval = copy_to_user((void *)ubuf_ptr,
				pfifo_data->fifo_data,
				buf_size);
		retval = -EOVERFLOW;
	}

	LOGD("From FIFO: (0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
		pfifo_data->fifo_data[0], pfifo_data->fifo_data[1],
		pfifo_data->fifo_data[2], pfifo_data->fifo_data[3]);

	list_del(&pfifo_data->next);
	kfree(pfifo_data->fifo_data);
	kfree(pfifo_data);
	if (tcm->fifo_remaining_frame != 0)
		tcm->fifo_remaining_frame--;

	syna_pal_mutex_unlock(&g_fifo_queue_mutex);

	if (retval >= 0)
		retval = pfifo_data->data_length;

exit:
	return retval;
}

/**
 * syna_cdev_ioctl_set_reports()
 *
 * Assign the report types for queuing. The enabled reports will be queued
 * into the FIFO queue.
 *
 * @param
 *    [ in] tcm:            the driver handle
 *    [ in] ubuf_ptr:       points to a memory space from userspace
 *    [ in] buf_size:       size of given space
 *    [ in] report_size:    report types data size
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_set_reports(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int report_size)
{
	int retval = 0;
	unsigned char data[REPORT_TYPES] = {0};
	unsigned int reports = 0;
	unsigned int report_set = 0;

	if (buf_size < sizeof(data)) {
		LOGE("Invalid sync data size, buf_size:%d, expected:%d\n",
			buf_size, (unsigned int)sizeof(data));
		return -EINVAL;
	}

#if !defined(ENABLE_EXTERNAL_FRAME_PROCESS)
	LOGE("ENABLE_EXTERNAL_FRAME_PROCESS is not enabled\n");
	return -EINVAL;
#endif

	if (report_size == 0) {
		LOGE("Invalid written size\n");
		return -EINVAL;
	}

	retval = copy_from_user(data, ubuf_ptr, report_size);
	if (retval) {
		LOGE("Fail to copy data from user space, size:%d\n", retval);
		retval = -EIO;
		goto exit;
	}

	retval = syna_pal_mem_cpy(tcm->report_to_queue, REPORT_TYPES,
			data, sizeof(data), REPORT_TYPES);
	for (reports = 0 ; reports < REPORT_TYPES ; reports++) {
		if (tcm->report_to_queue[reports] == EFP_ENABLE) {
			report_set++;
			LOGD("Set report 0x%02x for queue\n", reports);
		}
	}

	LOGI("Forward %d types of reports to the Queue.\n", report_set);

	retval = report_set;

exit:
	return retval;
}
/**
 * syna_cdev_ioctl_send_message()
 *
 * Send the command/message from userspace.
 *
 * For updating the g_sysfs_io_polling_interval, it need to be configured
 * by syna_cdev_ioctl_enable_irq from userspace.
 *
 * @param
 *    [ in] tcm:           the driver handle
 *    [ in/out] ubuf_ptr:  points to a memory space from userspace
 *    [ in] buf_size:      size of given space
 *    [ in/out] msg_size:  size of message
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_send_message(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int *msg_size)
{
	int retval = 0;
	unsigned char *data = NULL;
	unsigned char resp_code = 0;
	unsigned int payload_length = 0;
	unsigned int delay_ms_resp = 0;
	struct tcm_buffer resp_data_buf;

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if (buf_size < SEND_MESSAGE_HEADER_LENGTH) {
		LOGE("Invalid sync data size, buf_size:%d\n", buf_size);
		return -EINVAL;
	}

	if (*msg_size == 0) {
		LOGE("Invalid message length, msg size: 0\n");
		return -EINVAL;
	}

	syna_tcm_buf_lock(&g_cdev_cbuf);

	retval = syna_tcm_buf_alloc(&g_cdev_cbuf, buf_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for g_cdev_cbuf, size: %d\n",
			buf_size);
		goto exit;
	}

	data = g_cdev_cbuf.buf;

	retval = copy_from_user(data, ubuf_ptr, *msg_size);
	if (retval) {
		LOGE("Fail to copy data from user space, size:%d\n", *msg_size);
		retval = -EIO;
		goto exit;
	}

	payload_length = syna_pal_le2_to_uint(&data[1]);
	LOGD("Command = 0x%02x, payload length = %d\n",
		data[0], payload_length);

	/* init a buffer for the response data */
	syna_tcm_buf_init(&resp_data_buf);

	if (g_sysfs_io_polling_interval > 0)
		delay_ms_resp = g_sysfs_io_polling_interval;

	retval = syna_tcm_send_command(tcm->tcm_dev,
			data[0],
			&data[3],
			payload_length,
			&resp_code,
			&resp_data_buf,
			delay_ms_resp);
	if (retval) {
		LOGE("Fail to send command:%d\n", retval);
		retval = -EIO;
		goto exit;
	}

	syna_pal_mem_set(data, 0, buf_size);
	/* status code */
	data[0] = resp_code;
	/* the length for response data */
	data[1] = (unsigned char)(resp_data_buf.data_length & 0xff);
	data[2] = (unsigned char)((resp_data_buf.data_length >> 8) & 0xff);
	/* response data */
	if (resp_data_buf.data_length > 0) {
		retval = syna_pal_mem_cpy(&g_cdev_cbuf.buf[3],
			(g_cdev_cbuf.buf_size - SEND_MESSAGE_HEADER_LENGTH),
			resp_data_buf.buf,
			resp_data_buf.buf_size,
			resp_data_buf.data_length);
		if (retval < 0) {
			LOGE("Fail to copy resp data\n");
			retval = -EIO;
			goto exit;
		}
	}

#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
    /* It's for queuing the data when user is polling the command
     * response for the selected responses. The response will not be
     * queued if the user doesn't set the report/response types through
     * syna_cdev_ioctl_set_reports.
     */
	if (delay_ms_resp != FORCE_ATTN_DRIVEN) {
		if (tcm->report_to_queue[resp_code] == EFP_ENABLE) {
			syna_cdev_update_report_queue(tcm, resp_code,
				&resp_data_buf);
		}
	}
#endif

	if (buf_size < resp_data_buf.data_length) {
		LOGE("No enough space for data copy, buf_size:%d data:%d\n",
			buf_size, resp_data_buf.data_length);
		retval = -EOVERFLOW;
		goto exit;
	}

	retval = copy_to_user((void *)ubuf_ptr,
			data, resp_data_buf.data_length);
	if (retval) {
		LOGE("Fail to copy data to user space\n");
		retval = -EIO;
		goto exit;
	}

	*msg_size = resp_data_buf.data_length + 3;
	retval = *msg_size;

exit:
	syna_tcm_buf_unlock(&g_cdev_cbuf);

	syna_tcm_buf_release(&resp_data_buf);

	return retval;
}

/**
 * syna_cdev_ioctl_enable_irq()
 *
 * Enable or disable the irq via IOCTL.
 *
 * The 4 bytes unsigned int parameter from the buffer:
 *    0:        disable the irq.
 *    1:        enable the irq and set the g_sysfs_io_polling_interval
 *              to default.
 *    2~(~0)-1: the polling interval for response reading from
 *              syna_cdev_ioctl_send_message.
 *    (~0):     To use the fill isr for reading the response from
 *              syna_cdev_ioctl_send_message.
 *
 * @param
 *    [ in] tcm:       the driver handle
 *    [ in] ubuf_ptr:  points to a memory space from userspace
 *    [ in] buf_size:  size of given space
 *    [ in] data_size: size of actual data
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_enable_irq(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int data_size)
{
	int retval = 0;
	unsigned int en = 0;
	unsigned char *data = NULL;

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if ((buf_size < 4) || (data_size < 4)) {
		LOGE("Invalid sync data size, buf_size:%d, data_size:%d\n",
		    buf_size, data_size);
		return -EINVAL;
	}

	if (!tcm->hw_if->ops_enable_irq) {
		LOGW("Not support to enable irq\n");
		return 0;
	}

	syna_tcm_buf_lock(&g_cdev_cbuf);

	retval = syna_tcm_buf_alloc(&g_cdev_cbuf, buf_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for g_cdev_buf, size: %d\n",
			buf_size);
		goto exit;
	}

	data = g_cdev_cbuf.buf;

	retval = copy_from_user(data, ubuf_ptr, buf_size);
	if (retval) {
		LOGE("Fail to copy data from user space, size:%d\n", retval);
		retval = -EIO;
		goto exit;
	}

	en = syna_pal_le4_to_uint(&data[0]);

	LOGD("Prepare to %s irq\n", (en)?"enable":"disable");

	switch (en) {
	case SYSFS_DISABLED_INTERRUPT:
		retval = tcm->hw_if->ops_enable_irq(tcm->hw_if, false);
		if (retval < 0) {
			LOGE("Fail to disable interrupt\n");
			goto exit;
		}
		g_sysfs_io_polling_interval = 0;
		LOGI("IRQ is disabled\n");
		break;
	default:
		if (en == SYSFS_FULL_INTERRUPT)
			g_sysfs_io_polling_interval = SYSFS_FULL_INTERRUPT;
		else if (en == SYSFS_GENERIC_INTERRUPT)
			g_sysfs_io_polling_interval = 0;
		else
			g_sysfs_io_polling_interval = en;

		retval = tcm->hw_if->ops_enable_irq(tcm->hw_if, true);
		if (retval < 0) {
			LOGE("Fail to enable interrupt\n");
			goto exit;
		}
		LOGI("IRQ is enabled\n");
		break;
	}

exit:
	syna_tcm_buf_unlock(&g_cdev_cbuf);

	return retval;
}
/**
 * syna_cdev_ioctl_store_pid()
 *
 * Save PID through IOCTL interface
 *
 * @param
 *    [ in] tcm:       the driver handle
 *    [ in] ubuf_ptr:  points to a memory space from userspace
 *    [ in] buf_size:  size of given space
 *    [ in] data_size: size of actual data
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_store_pid(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int data_size)
{
	int retval = 0;
	unsigned char *data = NULL;

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if (buf_size < 4) {
		LOGE("Invalid sync data size, buf_size:%d\n", buf_size);
		return -EINVAL;
	}

	if (data_size < 4) {
		LOGE("Invalid data_size\n");
		return -EINVAL;
	}

	syna_tcm_buf_lock(&g_cdev_cbuf);

	retval = syna_tcm_buf_alloc(&g_cdev_cbuf, buf_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for g_cdev_buf, size: %d\n",
			buf_size);
		goto exit;
	}

	data = g_cdev_cbuf.buf;

	retval = copy_from_user(data, ubuf_ptr, data_size);
	if (retval) {
		LOGE("Fail to copy data from user space, size:%d\n", retval);
		retval = -EIO;
		goto exit;
	}

	tcm->proc_pid = syna_pal_le4_to_uint(&data[0]);

	LOGD("PID: %d\n", (unsigned int)tcm->proc_pid);
#ifdef ENABLE_PID_TASK
	if (tcm->proc_pid) {
		tcm->proc_task = pid_task(
				find_vpid(tcm->proc_pid),
				PIDTYPE_PID);
		if (!tcm->proc_task) {
			LOGE("Fail to locate task, pid: %d\n",
				(unsigned int)tcm->proc_pid);
			retval = -EINVAL;
			goto exit;
		}
	}
#endif
exit:
	syna_tcm_buf_unlock(&g_cdev_cbuf);

	return retval;
}
/**
 * syna_cdev_ioctl_raw_read()
 *
 * Read the data from device directly without routing to command wrapper
 * interface.
 *
 * @param
 *    [ in] tcm:         the driver handle
 *    [in/out] ubuf_ptr: ubuf_ptr: points to a memory space from userspace
 *    [ in] buf_size:    size of given space
 *    [ in] rd_size:     reading size
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_raw_read(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int rd_size)
{
	int retval = 0;
	unsigned char *data = NULL;

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if ((buf_size < 0) || (rd_size > buf_size)) {
		LOGE("Invalid sync data size, buf_size:%d, rd_size:%d\n",
			buf_size, rd_size);
		return -EINVAL;
	}

	if (rd_size == 0) {
		LOGE("The read length is 0\n");
		return 0;
	}

	syna_tcm_buf_lock(&g_cdev_cbuf);

	retval = syna_tcm_buf_alloc(&g_cdev_cbuf, rd_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for g_cdev_cbuf, size: %d\n",
			rd_size);
		goto exit;
	}

	data = g_cdev_cbuf.buf;

	retval = syna_tcm_read(tcm->tcm_dev,
			data,
			rd_size);
	if (retval < 0) {
		LOGE("Fail to read raw data, size: %d\n", rd_size);
		goto exit;
	}

	if (copy_to_user((void *)ubuf_ptr, data, rd_size)) {
		LOGE("Fail to copy data to user space\n");
		retval = -EINVAL;
		goto exit;
	}

	retval = rd_size;

exit:
	syna_tcm_buf_unlock(&g_cdev_cbuf);

	return retval;
}
/**
 * syna_cdev_ioctl_raw_write()
 *
 * Write the given data to device directly without routing to command wrapper
 * interface.
 *
 * @param
 *    [ in] tcm:      the driver handle
 *    [ in] ubuf_ptr: points to a memory space from userspace
 *    [ in] buf_size: size of given space
 *    [ in] wr_size:  size to write
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_raw_write(struct syna_tcm *tcm,
		const unsigned char *ubuf_ptr, unsigned int buf_size,
		unsigned int wr_size)
{
	int retval = 0;
	unsigned char *data = NULL;

	if (!tcm->is_connected) {
		LOGE("Not connected\n");
		return _EINVAL;
	}

	if ((buf_size < 0) || (wr_size > buf_size)) {
		LOGE("Invalid sync data size, buf_size:%d, wr_size:%d\n",
			buf_size, wr_size);
		return -EINVAL;
	}

	if (wr_size == 0) {
		LOGE("Invalid written size\n");
		return -EINVAL;
	}

	syna_tcm_buf_lock(&g_cdev_cbuf);

	retval = syna_tcm_buf_alloc(&g_cdev_cbuf, wr_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for g_cdev_cbuf, size: %d\n",
			wr_size);
		goto exit;
	}

	data = g_cdev_cbuf.buf;

	retval = copy_from_user(data, ubuf_ptr, wr_size);
	if (retval) {
		LOGE("Fail to copy data from user space, size:%d\n", retval);
		retval = -EIO;
		goto exit;
	}

	LOGD("Header: (0x%02x, 0x%02x, 0x%02x)\n",
		data[0], data[1], data[2]);

	retval = syna_tcm_write(tcm->tcm_dev,
			data,
			wr_size);
	if (retval < 0) {
		LOGE("Fail to write raw data, size: %d\n", wr_size);
		goto exit;
	}

	retval = wr_size;

exit:
	syna_tcm_buf_unlock(&g_cdev_cbuf);

	return retval;
}

/**
 * syna_cdev_ioctl_dispatch()
 *
 * Dispatch the IOCTLs operation based on the given code
 *
 * @param
 *    [ in] tcm:       the driver handle
 *    [ in] code:      code for the target operation
 *    [ in] ubuf_ptr:  points to a memory space from userspace
 *    [ in] ubuf_size: size of given space
 *    [ in] wr_size:   written data size
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_dispatch(struct syna_tcm *tcm,
		unsigned int code, const unsigned char *ubuf_ptr,
		unsigned int ubuf_size, unsigned int *data_size)
{
	int retval = 0;

	switch (code) {
	case STD_SET_PID_ID:
		retval = syna_cdev_ioctl_store_pid(tcm,
				ubuf_ptr, ubuf_size, *data_size);
		break;
	case STD_ENABLE_IRQ_ID:
		retval = syna_cdev_ioctl_enable_irq(tcm,
				ubuf_ptr, ubuf_size, *data_size);
		break;
	case STD_RAW_WRITE_ID:
		retval = syna_cdev_ioctl_raw_write(tcm,
				ubuf_ptr, ubuf_size, *data_size);
		break;
	case STD_RAW_READ_ID:
		retval = syna_cdev_ioctl_raw_read(tcm,
				ubuf_ptr, ubuf_size, *data_size);
		break;
	case STD_GET_FRAME_ID:
		retval = syna_cdev_ioctl_get_frame(tcm,
				ubuf_ptr, ubuf_size, data_size);
		break;
	case STD_SEND_MESSAGE_ID:
		retval = syna_cdev_ioctl_send_message(tcm,
				ubuf_ptr, ubuf_size, data_size);
		break;
	case STD_SET_REPORTS_ID:
		retval = syna_cdev_ioctl_set_reports(tcm,
				ubuf_ptr, ubuf_size, *data_size);
		break;
	case STD_CHECK_FRAMES_ID:
		retval = syna_cdev_ioctl_check_frame(tcm,
				ubuf_ptr, ubuf_size, *data_size);
		break;
	case STD_CLEAN_OUT_FRAMES_ID:
		LOGD("STD_CLEAN_OUT_FRAMES_ID called\n");
		syna_cdev_clean_queue(tcm);
		retval = 0;
		break;
	default:
		LOGE("Unknown ioctl code: 0x%x\n", code);
		return -ENOTTY;
	}

	return retval;
}
/**
 * syna_cdev_ioctl_old_dispatch()
 *
 * Dispatch the old IOCTLs operation based on the given code
 *
 * @param
 *    [ in] tcm:      the driver handle
 *    [ in] code:     code for the target operation
 *    [ in] arg:      argument passed from user-space
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_ioctl_old_dispatch(struct syna_tcm *tcm,
		unsigned int code, unsigned long arg)
{
	int retval = 0;

	switch (code) {
	case OLD_RESET_ID:
		retval = syna_tcm_reset(tcm->tcm_dev);
		if (retval < 0) {
			LOGE("Fail to do reset\n");
			break;
		}

		retval = tcm->dev_set_up_app_fw(tcm);
		if (retval < 0) {
			LOGE("Fail to set up app fw\n");
			break;
		}

		break;
	case OLD_SET_IRQ_MODE_ID:
		if (!tcm->hw_if->ops_enable_irq) {
			retval = -EINVAL;
			break;
		}

		if (arg == 0)
			retval = tcm->hw_if->ops_enable_irq(tcm->hw_if,
					false);
		else if (arg == 1)
			retval = tcm->hw_if->ops_enable_irq(tcm->hw_if,
					true);
		break;
	case OLD_SET_RAW_MODE_ID:
		if (arg == 0)
			tcm->is_attn_redirecting = false;
		else if (arg == 1)
			tcm->is_attn_redirecting = true;

		break;
	case OLD_CONCURRENT_ID:
		retval = 0;
		break;

	default:
		LOGE("Unknown ioctl code: 0x%x\n", code);
		retval = -ENOTTY;
		break;
	}

	return retval;
}

/**
 * syna_cdev_ioctls()
 *
 * Used to implements the IOCTL operations
 *
 * @param
 *    [ in] filp: represents the file descriptor
 *    [ in] cmd:  command code sent from userspace
 *    [ in] arg:  arguments sent from userspace
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
#ifdef HAVE_UNLOCKED_IOCTL
static long syna_cdev_ioctls(struct file *filp, unsigned int cmd,
		unsigned long arg)
#else
static int syna_cdev_ioctls(struct inode *inp, struct file *filp,
		unsigned int cmd, unsigned long arg)
#endif
{
	int retval = 0;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;
	struct syna_ioctl_data ioc_data;
	unsigned char *ptr = NULL;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	syna_pal_mutex_lock(&g_extif_mutex);

	retval = 0;

	/* handle the old IOCTLs */
	if ((_IOC_NR(cmd)) < STD_IOCTL_BEGIN) {
		retval = syna_cdev_ioctl_old_dispatch(tcm,
			(unsigned int)_IOC_NR(cmd), arg);

		goto exit;
	} else if ((_IOC_NR(cmd)) == STD_IOCTL_BEGIN) {
		retval = 1;
		goto exit;
	}

	retval = copy_from_user(&ioc_data,
			(void __user *) arg,
			sizeof(struct syna_ioctl_data));
	if (retval) {
		LOGE("Fail to copy ioctl_data from user space, size:%d\n",
			retval);
		retval = -EINVAL;
		goto exit;
	}

	ptr = ioc_data.buf;

	retval = syna_cdev_ioctl_dispatch(tcm,
			(unsigned int)_IOC_NR(cmd),
			(const unsigned char *)ptr,
			ioc_data.buf_size,
			&ioc_data.data_length);
	if (retval < 0)
		goto exit;

	retval = copy_to_user((void __user *) arg,
			&ioc_data,
			sizeof(struct syna_ioctl_data));
	if (retval) {
		LOGE("Fail to update ioctl_data to user space, size:%d\n",
			retval);
		retval = -EIO;
		goto exit;
	}

exit:
	syna_pal_mutex_unlock(&g_extif_mutex);

	return retval;
}

#if defined(CONFIG_COMPAT) && defined(HAVE_COMPAT_IOCTL)
/**
 * syna_cdev_compat_ioctls()
 *
 * Used to implements the IOCTL compatible operations
 *
 * @param
 *    [ in] filp: represents the file descriptor
 *    [ in] cmd: command code sent from userspace
 *    [ in] arg: arguments sent from userspace
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static long syna_cdev_compat_ioctls(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;
	struct syna_tcm_ioctl_data_compat ioc_data;
	unsigned char *ptr = NULL;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	syna_pal_mutex_lock(&g_extif_mutex);

	retval = 0;

	/* handle the old IOCTLs */
	if ((_IOC_NR(cmd)) < STD_IOCTL_BEGIN) {
		retval = syna_cdev_ioctl_old_dispatch(tcm,
			(unsigned int)_IOC_NR(cmd), arg);

		goto exit;
	} else if ((_IOC_NR(cmd)) == STD_IOCTL_BEGIN) {
		retval = 1;
		goto exit;
	}

	retval = copy_from_user(&ioc_data,
		(struct syna_tcm_ioctl_data_compat __user *) compat_ptr(arg),
		sizeof(struct syna_tcm_ioctl_data_compat));
	if (retval) {
		LOGE("Fail to copy ioctl_data from user space, size:%d\n",
			retval);
		retval = -EINVAL;
		goto exit;
	}

	ptr = compat_ptr((unsigned long)ioc_data.buf);

	retval = syna_cdev_ioctl_dispatch(tcm,
			(unsigned int)_IOC_NR(cmd),
			(const unsigned char *)ptr,
			ioc_data.buf_size,
			&ioc_data.data_length);
	if (retval < 0)
		goto exit;

	retval = copy_to_user(compat_ptr(arg),
			&ioc_data,
			sizeof(struct syna_tcm_ioctl_data_compat));
	if (retval) {
		LOGE("Fail to update ioctl_data to user space, size:%d\n",
			retval);
		retval = -EIO;
		goto exit;
	}

exit:
	syna_pal_mutex_unlock(&g_extif_mutex);

	return retval;
}
#endif

/**
 * syna_cdev_llseek()
 *
 * Used to change the current position in a file.
 *
 * @param
 *    [ in] filp:   represents the file descriptor
 *    [ in] off:    the file position
 *    [ in] whence: flag for seeking
 *
 * @return
 *    return negative value always because TouchComm doesn't need.
 */
static loff_t syna_cdev_llseek(struct file *filp,
		loff_t off, int whence)
{
	return -EINVAL;
}
/**
 * syna_cdev_read()
 *
 * Used to read data through the device file.
 * Function will use raw write approach.
 *
 * @param
 *    [ in] filp:  represents the file descriptor
 *    [out] buf:   given buffer from userspace
 *    [ in] count: size of buffer
 *    [ in] f_pos: the file position
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static ssize_t syna_cdev_read(struct file *filp,
		char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (count == 0)
		return 0;

	syna_pal_mutex_lock(&g_extif_mutex);

	retval = syna_cdev_ioctl_raw_read(tcm,
			(const unsigned char *)buf, count, count);
	if (retval != count) {
		LOGE("Invalid read operation, request:%d, return:%d\n",
			(unsigned int)count, retval);
	}

	syna_pal_mutex_unlock(&g_extif_mutex);

	return retval;
}
/**
 * syna_cdev_write()
 *
 * Used to send data to device through the device file.
 * Function will use raw write approach.
 *
 * @param
 *    [ in] filp:  represents the file descriptor
 *    [ in] buf:   given buffer from userspace
 *    [ in] count: size of buffer
 *    [ in] f_pos: the file position
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static ssize_t syna_cdev_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (count == 0)
		return 0;

	syna_pal_mutex_lock(&g_extif_mutex);

	retval = syna_cdev_ioctl_raw_write(tcm,
			(const unsigned char *)buf, count, count);
	if (retval != count) {
		LOGE("Invalid write operation, request:%d, return:%d\n",
			(unsigned int)count, retval);
	}

	syna_pal_mutex_unlock(&g_extif_mutex);

	return retval;
}
/**
 * syna_cdev_open()
 *
 * Invoked when the device file is being open, which should be
 * always the first operation performed on the device file
 *
 * @param
 *    [ in] inp:  represents a file in rootfs
 *    [ in] filp: represents the file descriptor
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_open(struct inode *inp, struct file *filp)
{
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	syna_pal_mutex_lock(&g_extif_mutex);

	if (tcm->char_dev_ref_count != 0) {
		LOGN("cdev already open, %d\n",
			tcm->char_dev_ref_count);
		return -EBUSY;
	}

	tcm->char_dev_ref_count++;

	g_sysfs_io_polling_interval = 0;

#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
	syna_cdev_clean_queue(tcm);
#endif
	syna_pal_mutex_unlock(&g_extif_mutex);

	LOGI("cdev open\n");

	return 0;
}
/**
 * syna_cdev_release()
 *
 * Invoked when the device file is being released
 *
 * @param
 *    [ in] inp:  represents a file in rootfs
 *    [ in] filp: represents the file descriptor
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_cdev_release(struct inode *inp, struct file *filp)
{
	struct device *p_dev;
	struct kobject *p_kobj;
	struct syna_tcm *tcm;

	p_kobj = g_sysfs_dir->parent;
	p_dev = container_of(p_kobj, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	syna_pal_mutex_lock(&g_extif_mutex);

	if (tcm->char_dev_ref_count <= 0) {
		LOGN("cdev already closed, %d\n",
			tcm->char_dev_ref_count);
		return -EBUSY;
	}

	tcm->char_dev_ref_count--;

	tcm->is_attn_redirecting = false;
	syna_pal_mem_set(tcm->report_to_queue, 0, REPORT_TYPES);
#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
	syna_cdev_clean_queue(tcm);
#endif
	syna_pal_mutex_unlock(&g_extif_mutex);

	g_sysfs_io_polling_interval = 0;

	LOGI("cdev close\n");

	return 0;
}

/**
 * Declare the operations of TouchCom device file
 */
static const struct file_operations device_fops = {
	.owner = THIS_MODULE,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl = syna_cdev_ioctls,
#if defined(CONFIG_COMPAT) && defined(HAVE_COMPAT_IOCTL)
	.compat_ioctl = syna_cdev_compat_ioctls,
#endif
#else
	.ioctl = syna_cdev_ioctls,
#endif
	.llseek = syna_cdev_llseek,
	.read = syna_cdev_read,
	.write = syna_cdev_write,
	.open = syna_cdev_open,
	.release = syna_cdev_release,
};
/**
 * syna_cdev_redirect_attn()
 *
 * Expose the status of ATTN signal to userspace
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
void syna_cdev_redirect_attn(struct syna_tcm *tcm)
{
	if (tcm->proc_pid)
		return;
}
#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
/**
 * syna_cdev_update_report_queue()
 *
 * Push the selected data to the queue.
 *
 * @param
 *    [ in] tcm:         the driver handle
 *    [ in] code:        report type
 *    [ in] pevent_data: report payload
 *
 * @return
 *    none.
 */
void syna_cdev_update_report_queue(struct syna_tcm *tcm,
		unsigned char code, struct tcm_buffer *pevent_data)
{
	int retval;
	unsigned char *frame_buffer = NULL;
	unsigned int frame_length = 0;

	if (pevent_data == NULL) {
		LOGE("Returned, invalid event data pointer\n");
		return;
	}
	frame_length = pevent_data->data_length + 3;
	LOGD("The overall queuing data length = %d\n", frame_length);
	frame_buffer = (unsigned char *)syna_pal_mem_alloc(frame_length,
					sizeof(unsigned char));
	if (!frame_buffer) {
		LOGE("Fail to allocate buffer, size: %d, data_length: %d\n",
			pevent_data->data_length + 3, pevent_data->data_length);
		return;
	}

	frame_buffer[0] = code;
	frame_buffer[1] = (unsigned char)pevent_data->data_length;
	frame_buffer[2] = (unsigned char)(pevent_data->data_length >> 8);

	if (pevent_data->data_length > 0) {
		retval = syna_pal_mem_cpy(&frame_buffer[3],
				(frame_length - 3),
				pevent_data->buf,
				pevent_data->data_length,
				pevent_data->data_length);
		if (retval < 0) {
			LOGE("Fail to copy data to buffer, size: %d\n",
				pevent_data->data_length);
			goto exit;
		}
	}
	retval = syna_cdev_insert_fifo(tcm, frame_buffer, frame_length);
	if (retval < 0) {
		LOGE("Fail to insert data to fifo\n");
		goto exit;
	}

	wake_up_interruptible(&(tcm->wait_frame));

exit:
	syna_pal_mem_free((void *)frame_buffer);
}
#endif
/**
 * syna_cdev_devnode()
 *
 * Provide the declaration of devtmpfs
 *
 * @param
 *    [ in] dev:  an instance of device
 *    [ in] mode: mode of created node
 *
 * @return
 *    the string of devtmpfs
 */
static char *syna_cdev_devnode(struct device *dev, umode_t *mode)
{
	if (!mode)
		return NULL;

	/* S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH */
	*mode = CHAR_DEVICE_MODE;

	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}
/**
 * syna_cdev_create_sysfs()
 *
 * Create a device node and register it with sysfs.
 *
 * @param
 *    [ in] tcm: the driver handle
 *    [ in] pdev: an instance of platform device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_cdev_create_sysfs(struct syna_tcm *tcm,
		struct platform_device *pdev)
{
	int retval = 0;
	struct class *device_class = NULL;
	struct device *device = NULL;
	static int cdev_major_num;

	tcm->device_class = NULL;
	tcm->device = NULL;

	tcm->is_attn_redirecting = false;

	syna_pal_mutex_alloc(&g_extif_mutex);
#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
	syna_pal_mutex_alloc(&g_fifo_queue_mutex);
#endif
	syna_tcm_buf_init(&g_cdev_cbuf);

	if (cdev_major_num) {
		tcm->char_dev_num = MKDEV(cdev_major_num, 0);
		retval = register_chrdev_region(tcm->char_dev_num, 1,
				PLATFORM_DRIVER_NAME);
		if (retval < 0) {
			LOGE("Fail to register char device\n");
			goto err_register_chrdev_region;
		}
	} else {
		retval = alloc_chrdev_region(&tcm->char_dev_num, 0, 1,
				PLATFORM_DRIVER_NAME);
		if (retval < 0) {
			LOGE("Fail to allocate char device\n");
			goto err_alloc_chrdev_region;
		}

		cdev_major_num = MAJOR(tcm->char_dev_num);
	}

	cdev_init(&tcm->char_dev, &device_fops);
	tcm->char_dev.owner = THIS_MODULE;

	retval = cdev_add(&tcm->char_dev, tcm->char_dev_num, 1);
	if (retval < 0) {
		LOGE("Fail to add cdev_add\n");
		goto err_add_chardev;
	}

	device_class = class_create(THIS_MODULE, PLATFORM_DRIVER_NAME);
	if (IS_ERR(device_class)) {
		LOGE("Fail to create device class\n");
		retval = PTR_ERR(device_class);
		goto err_create_class;
	}

	device_class->devnode = syna_cdev_devnode;

	device = device_create(device_class, NULL,
			tcm->char_dev_num, NULL,
			CHAR_DEVICE_NAME"%d", MINOR(tcm->char_dev_num));
	if (IS_ERR(tcm->device)) {
		LOGE("Fail to create character device\n");
		retval = -ENODEV;
		goto err_create_device;
	}

	tcm->device_class = device_class;

	tcm->device = device;

	tcm->char_dev_ref_count = 0;
	tcm->proc_pid = 0;

#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
	INIT_LIST_HEAD(&tcm->frame_fifo_queue);
	init_waitqueue_head(&tcm->wait_frame);
#endif
	syna_pal_mem_set(tcm->report_to_queue, 0, REPORT_TYPES);

	retval = syna_sysfs_create_dir(tcm, pdev);
	if (retval < 0) {
		LOGE("Fail to create sysfs dir\n");
		retval = -ENODEV;
		goto err_create_dir;
	}

	return 0;

err_create_dir:
	device_destroy(device_class, tcm->char_dev_num);
err_create_device:
	class_destroy(device_class);
err_create_class:
	cdev_del(&tcm->char_dev);
err_add_chardev:
	unregister_chrdev_region(tcm->char_dev_num, 1);
err_alloc_chrdev_region:
err_register_chrdev_region:
	return retval;
}
/**
 * syna_cdev_remove_sysfs()
 *
 * Remove the allocate cdev device node and release the resource
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
void syna_cdev_remove_sysfs(struct syna_tcm *tcm)
{
	if (!tcm) {
		LOGE("Invalid tcm driver handle\n");
		return;
	}

	syna_sysfs_remove_dir(tcm);

	syna_pal_mem_set(tcm->report_to_queue, 0, REPORT_TYPES);
	syna_cdev_clean_queue(tcm);
	syna_pal_mutex_free(&g_fifo_queue_mutex);

	tcm->char_dev_ref_count = 0;
	tcm->proc_pid = 0;

	if (tcm->device) {
		device_destroy(tcm->device_class, tcm->char_dev_num);
		class_destroy(tcm->device_class);
		cdev_del(&tcm->char_dev);
		unregister_chrdev_region(tcm->char_dev_num, 1);
	}

	syna_tcm_buf_release(&g_cdev_cbuf);

	syna_pal_mutex_free(&g_extif_mutex);

	tcm->device_class = NULL;

	tcm->device = NULL;
}


