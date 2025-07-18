/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
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
 */

#include "gcore_drv_common.h"

#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/sched.h>

#if defined(CONFIG_DETECT_FW_RESTART_INIT_EN)\
	||defined(CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF)
#include <linux/hrtimer.h>
#endif
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM
#ifdef CONFIG_DRM
#include <drm/drm_panel.h>
#endif
#endif

#ifdef CONFIG_GCORE_PSY_NOTIFY
#include <linux/power_supply.h>
#endif

#ifdef CONFIG_FB
#include <linux/fb.h>
#endif
#define GTP_TRANS_X(x, tpd_x_res, lcm_x_res)     (((x)*(lcm_x_res))/(tpd_x_res))
#define GTP_TRANS_Y(y, tpd_y_res, lcm_y_res)     (((y)*(lcm_y_res))/(tpd_y_res))
u8 gcore_report_log_cfg = 1;
EXPORT_SYMBOL(gcore_report_log_cfg);

int gc_lcm_name = 0;

struct workqueue_struct *charger_notify_wq_gc;
struct work_struct gc_resume_work;

struct gcore_exp_fn_data fn_data = {
	.initialized = false,
	.fn_inited = false,
};

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD
static ssize_t tperaseflash_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	char *fwname = NULL;
	fwname = kmalloc(len, 256);
	if (fwname == NULL) {
		GTP_ERROR("tpd kmalloc failed");
		return -ENOMEM;
	}
	memset(fwname, 0, len);
	ret = copy_from_user(fwname, buffer, len);
	if (ret) {
		kfree(fwname);
		return -EINVAL;
	}
	if(!strstr(fwname,"Erase_flash")){
		GTP_DEBUG("Can't erase flash");
		goto fail;
	}
	
	if(force_erase_fash()){
		GTP_ERROR("Erase flash fail");
	}
	
fail:
	kfree(fwname);
	return len;

}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_ops_tperaseflash = {
	.proc_write = tperaseflash_store,
};
#else
static const struct file_operations proc_ops_tperaseflash = {
	.owner = THIS_MODULE,
	.write = tperaseflash_store,
};
#endif

#endif 

struct gcore_exp_fn *p_fwu_fn = &fw_update_fn;
struct gcore_exp_fn *p_intf_fn = &fs_interf_fn;
#if GCORE_MP_TEST_ON
struct gcore_exp_fn *p_mp_fn = &mp_test_fn;
#endif

#if GCORE_WDT_RECOVERY_ENABLE
int wdt_contin;
#endif

#if 0

void gcore_new_function_register(struct gcore_exp_fn *exp_fn)
{
	GTP_DEBUG("gcore_new_function_register fn: %d", exp_fn->fn_type);

	if (!fn_data.initialized) {
		INIT_LIST_HEAD(&fn_data.list);
		fn_data.initialized = true;
	}

	list_add_tail(&exp_fn->link, &fn_data.list);

	if (fn_data.fn_inited) {
		if (exp_fn->init != NULL) {
			exp_fn->init(fn_data.gdev);
		}
	}

	return;
}

EXPORT_SYMBOL(gcore_new_function_register);

void gcore_new_function_unregister(struct gcore_exp_fn *exp_fn)
{
	struct gcore_exp_fn *fn = NULL;
	struct gcore_exp_fn *fn_temp = NULL;

	GTP_DEBUG("gcore_new_function_unregister fn: %d", exp_fn->fn_type);

	if (!list_empty(&fn_data.list)) {
		list_for_each_entry_safe(fn, fn_temp, &fn_data.list, link) {
			if ((fn == exp_fn) && (fn->remove != NULL)) {
				fn->remove(fn_data.gdev);
				list_del(&fn->link);
			}
		}
	}
}

EXPORT_SYMBOL(gcore_new_function_unregister);

#endif

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD
struct proc_dir_entry *proc_gcorei2c_dir=NULL;

static int create_tpd_proc_entry(void)
{
	struct proc_dir_entry *tpd_proc_entry = NULL;

	//gdev_fwu->tpd_proc_dir = NULL;
	proc_gcorei2c_dir = proc_mkdir("gcore_i2c", NULL);
	if (proc_gcorei2c_dir == NULL) {
		GTP_ERROR("mkdir /proc/gcore_i2c failed!");
		return -EPERM;
	}
	tpd_proc_entry = proc_create("Erase_flash", 0664,  proc_gcorei2c_dir, &proc_ops_tperaseflash);
	if (tpd_proc_entry == NULL){
		GTP_ERROR("proc_create FW_upgrade failed!");
		remove_proc_entry("gcore_i2c",NULL);
	}

	return 0;
}
#endif

void gcore_irq_enable(struct gcore_dev *gdev)
{
	unsigned long flags;

	spin_lock_irqsave(&gdev->irq_flag_lock, flags);

	if (gdev->irq_flag == 0) {
		gdev->irq_flag = 1;
		spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
		enable_irq(gdev->touch_irq);
	} else if (gdev->irq_flag == 1) {
		spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
		GTP_ERROR("Touch Eint already enabled!");
	} else {
		spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
		GTP_ERROR("Invalid irq_flag %d!", gdev->irq_flag);
	}
	/*GTP_DEBUG("Enable irq_flag=%d",g_touch.irq_flag); */

}

void gcore_irq_disable(struct gcore_dev *gdev)
{
	unsigned long flags;

	spin_lock_irqsave(&gdev->irq_flag_lock, flags);

	if (gdev->irq_flag == 0) {
		spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
		GTP_ERROR("Touch Eint already disable!");
		return;
	}

	gdev->irq_flag = 0;

	spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);

	disable_irq(gdev->touch_irq);
/* disable_irq_nosync(gdev->touch_irq); */

}

#ifndef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
int gcore_get_gpio_info(struct gcore_dev *gdev, struct device_node *node)
{
	//int flag = 0;

	gdev->irq_gpio = of_get_named_gpio(node, "gcore,irq-gpio", 0);
	if (!gpio_is_valid(gdev->irq_gpio)) {
		GTP_ERROR("invalid gcore,irq-gpio:%d", gdev->irq_gpio);
		return -EPERM;
	}

	if (gpio_request(gdev->irq_gpio, "gcore,irq-gpio")) {
		GTP_ERROR("gpio %d request failed!", gdev->irq_gpio);
		return -EPERM;
	}

	gdev->rst_gpio = of_get_named_gpio(node, "gcore,rst-gpio",0);
	if (!gpio_is_valid(gdev->rst_gpio)) {
		GTP_ERROR("invalid gcore,reset-gpio:%d", gdev->rst_gpio);
		goto err_req_gpio;
	}

	if (gpio_request(gdev->rst_gpio, "gcore,rst-gpio")) {
		GTP_ERROR("gpio %d request failed!", gdev->rst_gpio);
		goto err_req_gpio;
	}
	
	GTP_DEBUG("irq_gpio = %d,rst_gpio = %d",gdev->irq_gpio,gdev->rst_gpio);

	return 0;
	
err_req_gpio:
	gpio_free(gdev->irq_gpio);
	return -EPERM;

}

void gcore_free_gpio(struct gcore_dev *gdev)
{
	gpio_free(gdev->irq_gpio);
	gpio_free(gdev->rst_gpio);
}

#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM
#ifdef CONFIG_DRM1
struct drm_panel *gcore_active_panel;

int gcore_ts_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			gcore_active_panel = panel;
			GTP_DEBUG("%s:find\n", __func__);
			return 0;
		}
	}

	GTP_ERROR("%s: not find\n", __func__);
	return -ENODEV;
}

int gcore_ts_check_default_tp(struct device_node *dt, const char *prop)
{
	const char **gcore_active_tp = NULL;
	int count = 0;
	int tmp = 0;
	int score = 0;
	const char *active;
	int ret = 0;
	int i = 0;

	count = of_property_count_strings(dt->parent, prop);
	if (count <= 0 || count > 3)
		return -ENODEV;

	gcore_active_tp = kcalloc(count, sizeof(char *), GFP_KERNEL);
	if (!gcore_active_tp) {
		GTP_ERROR("gcore active alloc failed\n");
		return -ENOMEM;
	}

	ret = of_property_read_string_array(dt->parent, prop, gcore_active_tp, count);
	if (ret < 0) {
		GTP_ERROR("fail to read %s %d\n", prop, ret);
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < count; i++) {
		active = gcore_active_tp[i];
		if (active != NULL) {
			tmp = of_device_is_compatible(dt, active);
			if (tmp > 0)
				score++;
		}
	}

	if (score <= 0) {
		GTP_ERROR("not match this driver\n");
		ret = -ENODEV;
		goto out;
	}
	ret = 0;
out:
	kfree(gcore_active_tp);
	return ret;
}
#endif
#endif

void gcore_get_dts_info(struct gcore_dev *gdev)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	gdev->rst_gpio = GTP_RST_PORT;
	gdev->irq_gpio = GTP_INT_PORT;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	struct device_node *node = NULL;
/*	struct device_node *node = client->dev.of_node;  */

	node = of_find_matching_node(node, tpd_of_match);

/* 20210114: remove customized parameter for dts to common.h */
/*
	if (node)
	{
		of_property_read_u32_array(node, "gcore,tp-resolution",
			gcore_dts_data.tpd_resolution, ARRAY_SIZE(gcore_dts_data.tpd_resolution));

		GTP_DEBUG("tp res x = %d y = %d", gcore_dts_data.tpd_resolution[0],
			gcore_dts_data.tpd_resolution[1]);

		of_property_read_u32(node, "gcore,max-touch-number",
			&gcore_dts_data.touch_max_num);

		GTP_DEBUG("max touch number = %d", gcore_dts_data.touch_max_num);

		of_property_read_u32_array(node, "gcore,lcd-resolution",
			gcore_dts_data.lcm_resolution, ARRAY_SIZE(gcore_dts_data.lcm_resolution));

		GTP_DEBUG("lcd res x = %d y = %d", gcore_dts_data.lcm_resolution[0],
			gcore_dts_data.lcm_resolution[1]);

	}
	else
	{
		GTP_ERROR("can't find touch compatible custom node.");
	}
*/

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM
#ifdef CONFIG_DRM1
	if (gcore_ts_check_dt(node)) {
		if (!gcore_ts_check_default_tp(node, "qcom,spi-touch-active")) {
			GTP_DEBUG("check default tp 1");
/* return -EPROBE_DEFER; */
		} else {
			GTP_DEBUG("check default tp 2");
/* return -ENODEV */
		}
	}
#endif
#endif

	if (gcore_get_gpio_info(gdev, node)) {
		GTP_ERROR("dts get gpio info fail!");
	}
#endif
}

int gcore_input_device_init(struct gcore_dev *gdev)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	int tpd_res_x = tpd_dts_data.tpd_resolution[0];
	int tpd_res_y = tpd_dts_data.tpd_resolution[1];

	gdev->input_device = tpd->dev;

	set_bit(EV_ABS, gdev->input_device->evbit);
	set_bit(EV_KEY, gdev->input_device->evbit);
	set_bit(EV_SYN, gdev->input_device->evbit);
	set_bit(INPUT_PROP_DIRECT, gdev->input_device->propbit);

	input_set_abs_params(gdev->input_device, ABS_MT_POSITION_X, 0, tpd_res_x, 0, 0);
	input_set_abs_params(gdev->input_device, ABS_MT_POSITION_Y, 0, tpd_res_y, 0, 0);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	set_bit(GESTURE_KEY, gdev->input_device->keybit);
#endif

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	if (input_mt_init_slots(gdev->input_device, GTP_MAX_TOUCH, INPUT_MT_DIRECT)) {
		GTP_ERROR("mt slot init fail.");
	}
#else
	/* for single-touch device */
	set_bit(BTN_TOUCH, gdev->input_device->keybit);
#endif

	return 0;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	int tpd_res_x = TOUCH_SCREEN_X_MAX;
	int tpd_res_y = TOUCH_SCREEN_Y_MAX;

	GTP_DEBUG("touch input device init.");

	gdev->input_device = input_allocate_device();
	if (gdev->input_device == NULL) {
		GTP_ERROR("fail to allocate touch input device.");
		return -EPERM;
	}

	gdev->input_device->name = GTP_DRIVER_NAME;
	set_bit(EV_ABS, gdev->input_device->evbit);
	set_bit(EV_KEY, gdev->input_device->evbit);
	set_bit(EV_SYN, gdev->input_device->evbit);
	set_bit(INPUT_PROP_DIRECT, gdev->input_device->propbit);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	set_bit(GESTURE_KEY, gdev->input_device->keybit);
#endif

	input_set_abs_params(gdev->input_device, ABS_MT_POSITION_X, 0, tpd_res_x, 0, 0);
	input_set_abs_params(gdev->input_device, ABS_MT_POSITION_Y, 0, tpd_res_y, 0, 0);

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	if (input_mt_init_slots(gdev->input_device, GTP_MAX_TOUCH, 0)) {
		GTP_ERROR("mt slot init fail.");
	}
#else
	/* for single-touch device */
	set_bit(BTN_TOUCH, gdev->input_device->keybit);
#endif

	input_set_drvdata(gdev->input_device, gdev);

	if (input_register_device(gdev->input_device)) {
		GTP_ERROR("input register device fail.");
		goto err_free_mem;
	}

	return 0;
	
err_free_mem:
	input_free_device(gdev->input_device);
	return -EPERM;
#endif
}

void gcore_input_device_deinit(struct gcore_dev *gdev)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	return;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	input_unregister_device(gdev->input_device);
#endif
}

void gcore_touch_down(struct input_dev *dev, s32 x, s32 y, u8 id)
{
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	input_mt_slot(dev, id);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
	input_report_abs(dev, ABS_MT_POSITION_X, x);
	input_report_abs(dev, ABS_MT_POSITION_Y, y);
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	input_report_abs(dev, ABS_MT_PRESSURE, 100);
#endif

#else
	input_report_key(dev, BTN_TOUCH, 1);	/* for single-touch device */
	input_report_abs(dev, ABS_MT_POSITION_X, x);
	input_report_abs(dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(dev);
#endif
}

void gcore_touch_up(struct input_dev *dev, u8 id)
{
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	input_mt_slot(dev, id);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
#else
	input_report_key(dev, BTN_TOUCH, 0);	/* for single-touch device */
	input_mt_sync(dev);
#endif
}

void gcore_touch_release_all_point(struct input_dev *dev)
{
	int i = 0;

	GTP_DEBUG("gcore touch release all point");

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	for (i = 0; i < GTP_MAX_TOUCH; i++) {
		gcore_touch_up(dev, i);
	}

	input_report_key(dev, BTN_TOUCH, 0);
	input_report_key(dev, BTN_TOOL_FINGER, 0);
#else
	gcore_touch_up(dev, 0);
#endif

	input_sync(dev);
}

void gcore_touch_release_all_point255(struct input_dev *dev)
{
	int i = 0;

	GTP_DEBUG("gcore touch release all point255");
	
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	for (i = 0; i < 256; i++)
	{
		gcore_touch_up(dev, i);
	}

	input_report_key(dev, BTN_TOUCH, 0);
	input_report_key(dev, BTN_TOOL_FINGER, 0);
#else
	gcore_touch_up(dev, 0);
#endif

	input_sync(dev);
}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
void set_gcore_gesture_en(int en)
{
	GTP_DEBUG("set en = %d", en);
	if (en) {
		fn_data.gdev->gesture_wakeup_en = true;
		gcore_fw_event_notify(FW_GESTURE_ENABLE);
	} else {
		fn_data.gdev->gesture_wakeup_en = false;
		gcore_fw_event_notify(FW_GESTURE_DISABLE);
	}
}

void gcore_gesture_event_handler(struct input_dev *dev, int id)
{

	if ((!id) || (id >= GESTURE_MAX)) {
		return;
	}

	GTP_DEBUG("gesture id = %d", id);

	switch (id) {
	case GESTURE_DOUBLE_CLICK:
		GTP_DEBUG("double click gesture event");
		input_report_key(dev, GESTURE_KEY, 1);
		input_sync(dev);
		input_report_key(dev, GESTURE_KEY, 0);
		input_sync(dev);
		break;

	case GESTURE_UP:
		break;

	case GESTURE_DOWN:
		break;

	case GESTURE_LEFT:
		break;

	case GESTURE_RIGHT:
		break;

	case GESTURE_C:
		GTP_DEBUG("double click gesture event");
		input_report_key(dev, KEY_F24, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F24, 0);
		input_sync(dev);
		break;

	case GESTURE_E:
		break;

	case GESTURE_M:
		break;

	case GESTURE_N:
		break;

	case GESTURE_O:
		break;

	case GESTURE_S:
		break;

	case GESTURE_V:
		break;

	case GESTURE_W:
		break;

	case GESTURE_Z:
		break;

	case GESTURE_PALM:
		GTP_DEBUG("palm gesture event");
		break;
	
	default:
		break;
	}

}
#endif


static u8 Cal8bitsChecksum(u8 *array, u16 length)
{
	s32 s32Checksum = 0;
	u16 i;

	for (i = 0; i < length; i++) {
		s32Checksum += array[i];
	}

	return (u8) (((~s32Checksum) + 1) & 0xFF);
}

int gcore_hostdownload_esd_protect(struct gcore_dev *gdev)
{
	u8 *coor_data = &gdev->touch_data[0];
	int sum = 0;
	int i = 0;

	if (coor_data[0] == 0xB1) {
		/* santiy check coor data follow 0 */
		for (i = 1; i < 64; i++) {
			sum += coor_data[i];
		}

		if (!sum) {
#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
#ifdef CONFIG_MTK_PROXIMITY_TP_SCREEN_ON_ALSPS
        if(fn_data.gdev->PS_Enale == true){
            ps_report_interrupt_data(10);
            GTP_DEBUG("Wake up from proximity");
        }
#else
        if(fn_data.gdev->PS_Enale == true){
            GTP_DEBUG("Wake up from proximity");
			input_report_abs(gdev_intf->input_ps_device, ABS_DISTANCE, 1);
            input_sync(gdev_intf->input_ps_device);
        }
#endif
#endif
			if (!gdev->ts_stat) {
				GTP_DEBUG("watchdog:B1, goto hostdownload!");
				queue_delayed_work(gdev->fwu_workqueue, &gdev->fwu_work,
						   msecs_to_jiffies(100));
			}

			return -EPERM;
		}
	}

	return 0;
}

#if GCORE_WDT_RECOVERY_ENABLE
static u8 wdt_valid_values[] = { 0xFF,0xFF,0x72,0x02,0x00,0x01,0xFF,0xFF };
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
static u8 CB_check_value[] = { 0xFF,0xFF,0xCB,0x00,0x00,0xCB,0xFF,0xFF };
static u8 fw_rcCB_value[] = { 0xFF,0xFF,0x00,0xCB,0xCB,0x00,0xFF,0xFF };
#endif

#ifdef CONFIG_DETECT_FW_RESTART_INIT_EN
//static u8 fw_init_values[] = { 0xFF,0xFF,0x33,0x28,0x04,0x01,0xFF,0xFF };
static u8 fw_init_values[] = { 0xB3,0xFF,0xFF,0x33,0x28,0x04,0x01,0xFF,0xFF };

static int fw_init_count = 0;
static int fw_init_notcount_flag = -1;
static u8 fw_status = 0;

int get_fw_init_count(void){
	return fw_init_count;
}
void set_fw_init_count(int count){
	fw_init_count = count;
}
int get_fw_init_notcount_flag(void){
	return fw_init_notcount_flag;
}
void set_fw_init_notcount_flag(int flag){
	GTP_DEBUG("flag:%d.", flag);
	fw_init_notcount_flag = flag;
}
u8 get_fw_status(void){
	GTP_DEBUG("%x.", fw_status);
	return fw_status;
}


struct hrtimer gcore_timer;
static enum hrtimer_restart gcore_timer_handler(struct hrtimer *timer){
	set_fw_init_notcount_flag(0);
	return HRTIMER_NORESTART;
}

void fw_init_count_timer_init(struct gcore_dev *gdev){

	hrtimer_init(&gcore_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gcore_timer.function = gcore_timer_handler;
	set_fw_init_notcount_flag(0);
	GTP_DEBUG("gcore_timer init success!");
}
void fw_init_count_timer_start(int flag){
	if(get_fw_init_notcount_flag() >= 0){
		set_fw_init_notcount_flag(flag);
		hrtimer_start(&gcore_timer, ms_to_ktime(10), HRTIMER_MODE_REL);
	}else{
		GTP_DEBUG("gcore_timer null, not init yet!");
	}
}
void fw_init_count_timer_exit(void){
	hrtimer_cancel(&gcore_timer);
}

#endif

#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
bool gcore_tpd_proximity_flag_off = 0;
bool gcore_tpd_proximity_flag_off_backup = 0;
bool gcore_get_proximity_flag_off(void)
{
	GTP_DEBUG("proximity_flag_off_backup:%d", gcore_tpd_proximity_flag_off_backup);
	
	return gcore_tpd_proximity_flag_off_backup;
}
EXPORT_SYMBOL(gcore_get_proximity_flag_off);

// proximity use notifier to lcm:
#include <linux/notifier.h>
struct atomic_notifier_head gcore_proximity_notifier;
ATOMIC_NOTIFIER_HEAD(gcore_proximity_notifier);
EXPORT_SYMBOL_GPL(gcore_proximity_notifier);

int gcore_proximity_reg_notifier(struct notifier_block *nb){
	return atomic_notifier_chain_register(&gcore_proximity_notifier, nb);
}
EXPORT_SYMBOL_GPL(gcore_proximity_reg_notifier);
int gcore_proximity_unreg_notifier(struct notifier_block *nb){
	return atomic_notifier_chain_unregister(&gcore_proximity_notifier, nb);
}
EXPORT_SYMBOL_GPL(gcore_proximity_unreg_notifier);

void call_proximity_notifier(void){
	GTP_DEBUG(" entry.");
	atomic_notifier_call_chain(&gcore_proximity_notifier, 0, NULL);
}

// client notifier callback start:
int gcore_proximity_notifier_callback(struct notifier_block *nb, 
	unsigned long event, void *data){
	//printk(KERN_ERR "GTP gcore_proximity_notifier_callback start.\n");
	GTP_DEBUG(" entry.");
	return 0;
}
static struct notifier_block gcore_notifier_block = {
	.notifier_call = gcore_proximity_notifier_callback,
};
extern int gcore_proximity_reg_notifier(struct notifier_block *nb);
extern int gcore_proximity_unreg_notifier(struct notifier_block *nb);

void register_notifier_callback(void){
	gcore_proximity_reg_notifier(&gcore_notifier_block);
}
void unregister_notifier_callback(void){
	gcore_proximity_unreg_notifier(&gcore_notifier_block);
}
// client notifier callback end.

extern struct gcore_dev *gdev_intf;

bool gcore_proximity_flag_suspend_change = 1;
bool set_proximity_flag_suspend_change(int change){
	gcore_proximity_flag_suspend_change = change;
	GTP_DEBUG("set suspend_change = %d.", change);
	return change;
}
void gcore_proxi_timer_start(void);
void gcore_proximity_report_to_application(void){
	if(gcore_tpd_proximity_flag_off != gcore_tpd_proximity_flag_off_backup
		&& gcore_proximity_flag_suspend_change ){
		GTP_DEBUG("[proximity] p-sensor state:%s", gcore_tpd_proximity_flag_off ? "NEAR" : "AWAY");
		gcore_tpd_proximity_flag_off_backup = gcore_tpd_proximity_flag_off;
		
#ifdef CONFIG_MTK_PROXIMITY_TP_SCREEN_ON_ALSPS
		if(gcore_tpd_proximity_flag_off){
			gcore_proxi_timer_start();
			call_proximity_notifier();
			ps_report_interrupt_data(0);
		}else{
			ps_report_interrupt_data(10);
		}
#else
		if(gcore_tpd_proximity_flag_off){
			gcore_proxi_timer_start();
			call_proximity_notifier();
			input_report_abs(gdev_intf->input_ps_device, ABS_DISTANCE, 0);
			GTP_DEBUG("input_report ABS_DISTANCE 0.");
		}else{
			input_report_abs(gdev_intf->input_ps_device, ABS_DISTANCE, 1);
			GTP_DEBUG("input_report ABS_DISTANCE 1.");
		}
		input_sync(gdev_intf->input_ps_device);
#endif
	}
}

struct hrtimer gcore_proxi_timer;
static enum hrtimer_restart gcore_proxi_timer_handler(struct hrtimer *timer){
	set_proximity_flag_suspend_change(1);
	gcore_proximity_report_to_application();
	return HRTIMER_NORESTART;
}
void gcore_proxi_timer_init(void){
	set_proximity_flag_suspend_change(1);
	hrtimer_init(&gcore_proxi_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gcore_proxi_timer.function = gcore_proxi_timer_handler;
	GTP_DEBUG("gcore_timer init success!");
}
void gcore_proxi_timer_start(void){
	set_proximity_flag_suspend_change(0);
	hrtimer_start(&gcore_proxi_timer, ms_to_ktime(400), HRTIMER_MODE_REL);
	GTP_DEBUG("gcore_timer start!");
}
void gcore_proxi_timer_exit(void){
	hrtimer_cancel(&gcore_proxi_timer);
}

#endif


s32 gcore_touch_event_handler(struct gcore_dev *gdev)
{
	u8 *coor_data = NULL;
	s32 i = 0;
	u8 id = 0;
	u8 status = 0;
	bool touch_end = true;
	u8 checksum = 0;
	u16 touchdata = 0;
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	static unsigned long prev_touch;
#endif
	unsigned long curr_touch = 0;
	s32 input_x = 0;
	s32 input_y = 0;
	int data_size = DEMO_DATA_SIZE;
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	int tpd_res_x = tpd_dts_data.tpd_resolution[0];
	int tpd_res_y = tpd_dts_data.tpd_resolution[1];
	int lcm_res_x = tpd_dts_data.tpd_resolution[0];
	int lcm_res_y = tpd_dts_data.tpd_resolution[1];
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	int tpd_res_x = TOUCH_SCREEN_X_MAX;
	int tpd_res_y = TOUCH_SCREEN_Y_MAX;
	int lcm_res_x = TOUCH_SCREEN_X_MAX;
	int lcm_res_y = TOUCH_SCREEN_Y_MAX;
#endif

	

#ifdef CONFIG_ENABLE_FW_RAWDATA
	if (gdev->fw_mode == DEMO) {
		data_size = DEMO_DATA_SIZE;
	} else if (gdev->fw_mode == RAWDATA) {
		data_size = g_rawdata_row * g_rawdata_col * 2;
	} else if (gdev->fw_mode == DEMO_RAWDATA) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
		data_size = 2048;	/* mtk platform spi transfer len request */
#else
		data_size = DEMO_DATA_SIZE + (g_rawdata_row * g_rawdata_col * 2);
#endif
	} else if (gdev->fw_mode == DEMO_RAWDATA_DEBUG) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
		data_size = 2048;	/* mtk platform spi transfer len request */
#else
		data_size = DEMO_DATA_SIZE + ((g_rawdata_row+1)*(g_rawdata_col+1) * 2)  ;	
#endif	
	}
	else if (gdev->fw_mode == FW_DEBUG) {
		data_size = 175;
		//data_size = gdev->fw_packet_len;
			
	}else {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
		data_size = 2048;	/* mtk platform spi transfer len request */
#else
		data_size = DEMO_DATA_SIZE + (g_rawdata_row * g_rawdata_col * 2);
#endif
	}
#endif
	if (data_size > DEMO_RAWDATA_MAX_SIZE) {
		GTP_ERROR("touch data read length exceed MAX_SIZE.");
		return -EPERM;	
	}
	
	if (gcore_bus_read(gdev->touch_data, data_size)) {
		GTP_ERROR("touch data read error.");
		return -EPERM;
	}

	
#ifdef CONFIG_ENABLE_FW_RAWDATA
	//GTP_DEBUG("usr_read:%d. fw_mode:%d.", gdev->usr_read, gdev->fw_mode);
	if (gdev->usr_read) {
		gdev->data_ready = true;
		wake_up_interruptible(&gdev->usr_wait);

		if (gdev->fw_mode == RAWDATA) {
#if GCORE_WDT_RECOVERY_ENABLE
            mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work, \
				msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
#endif
			return 0;
		}
	}
#endif

	
//	printk("<tian> data_size = %d\n",data_size);
	if(gdev->fw_mode == DEMO_RAWDATA_DEBUG){
		for(i=1152+65;i<1319;i+=2){
			touchdata = (gdev->touch_data[i+1] << 8) | gdev->touch_data[i];
			printk("<tian> touch_data = %d,touch_data[%d+1]=%d,touch_data[%d]=%d\n",touchdata,i,gdev->touch_data[i+1],i,gdev->touch_data[i]);
		}
	}


	coor_data = &gdev->touch_data[0];

	if (demolog_enable) {
		GTP_DEBUG("demolog dbg is on!");
#if GCORE_MP_TEST_ON
		dump_demo_data_to_csv_file(coor_data, 10, 6);
#endif
	}
	
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
		if (!memcmp(coor_data, CB_check_value, sizeof(CB_check_value))) {
			gdev->CB_ckstat = true;
			gdev->cb_count = 0;
			GTP_DEBUG("Start receive and save CB value");
			if (gcore_bus_read(gdev->touch_data, CB_SIZE)) {
				GTP_ERROR("touch data read error.");
				return -EPERM;
			}
			gdev->touch_data += 8;
			if (gdev->CB_value == NULL) {
				GTP_ERROR("CB_value is NULL.");
				return 0;
			}
			memcpy(gdev->CB_value,gdev->touch_data, CB_SIZE);
			GTP_REPORT("CB_value[0]: %d,CB_value[1]: %d",gdev->CB_value[0],gdev->CB_value[1]);
			gdev->CB_value[0] = 0xCB;
			gdev->CB_value[1] = 0x01;
			return 0; /* receive and save CB value */
		}
		/* When TP resumed,Checks whether the fw has received CB value */
		if (!memcmp(coor_data, fw_rcCB_value, sizeof(fw_rcCB_value))) {
			gdev->CB_ckstat = true;
			/* gdev->CB_dl = false;*/
			gdev->cb_count = 0;
			GTP_DEBUG("CB value download complete interrupt detected");
			return 0; /* FW had received CB value */
		}
#endif


#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	if(gdev->PS_Enale == true){
		if(gdev->TEL_State){
			if(check_notify_event(coor_data[62],gdev->fw_event)){
				GTP_DEBUG("TEL:Notify fw event interrupt detected");
				mod_delayed_work(gdev->fwu_workqueue, &gdev->event_work, \
					msecs_to_jiffies(36));
				notify_contin = 0;
				gdev->notify_state = false;
				gdev->TEL_State = false;
			}
		}

		if(coor_data[62] & 0x01){
			//GTP_REPORT("TEL: On The Phone Now");
			if(coor_data[62] == 0x81){
				gdev->tel_screen_off = true; 
				gcore_tpd_proximity_flag_off = true; 
				GTP_INFO("TEL: Proximity tp turn off Screen,data[62]=0x%x",coor_data[62]);
			}else if(coor_data[62] == 0x01){
				gdev->tel_screen_off = false;
				gcore_tpd_proximity_flag_off = false; 
				GTP_INFO("TEL: Keep Screen On!data[62]=0x%x",coor_data[62]);
			}else {
				GTP_INFO("TEL: default data[62]=0x%x ",coor_data[62]);
			}
			gcore_proximity_report_to_application();
		}
	}


#endif
		if(gdev->notify_state){
			if(gdev->fw_event == FW_TEL_CALL || gdev->fw_event == FW_TEL_HANDUP 
				|| gdev->fw_event == FW_HANDSFREE_ON || gdev->fw_event == FW_HANDSFREE_OFF){
				status = coor_data[62];
			}else{
				status = coor_data[63];
			}
			if(check_notify_event(status, gdev->fw_event)){
				GTP_DEBUG("Notify fw event interrupt detected");
				mod_delayed_work(gdev->fwu_workqueue, &gdev->event_work, \
					msecs_to_jiffies(36));
				notify_contin = 0;
				gdev->notify_state = false;
			}
		}

#if GCORE_WDT_RECOVERY_ENABLE
	if (!gdev->ts_stat) {
		
		if (!memcmp(coor_data, wdt_valid_values, sizeof(wdt_valid_values))) {
			GTP_REPORT("WDT interrupt detected");
			mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work, \
				msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
			wdt_contin = 0;
			return 0; /* WDT interrupt detected */
		}
	}
#endif
#ifdef CONFIG_DETECT_FW_RESTART_INIT_EN
	if (!memcmp(coor_data, fw_init_values, sizeof(fw_init_values))) {
		if(get_fw_init_notcount_flag() == 0){
			fw_init_count++;
		}else{
			set_fw_init_notcount_flag(0);
		}
		GTP_DEBUG("fw init. count:%d.", fw_init_count);
		return 0;
	}
	fw_status = coor_data[60];
#endif

#if defined(CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD) \
		&& defined(CONFIG_GCORE_HOSTDOWNLOAD_ESD_PROTECT)
	/* firmware watchdog, rehostdownload */
	if (gcore_hostdownload_esd_protect(gdev)) {
		return 0;
	}
#endif

	checksum = Cal8bitsChecksum(coor_data, DEMO_DATA_SIZE - 1);
	if (checksum != coor_data[DEMO_DATA_SIZE - 1]) {
		GTP_ERROR("checksum error! read:%x cal:%x", \
			coor_data[DEMO_DATA_SIZE - 1], checksum);
#if 1
		for (i = 1; i <= 10; i++) {
			GTP_DEBUG("demo %d:%x %x %x %x %x %x", i, coor_data[6*i-6], \
				coor_data[6*i-5], coor_data[6*i-4], coor_data[6*i-3], \
				coor_data[6*i-2], coor_data[6*i-1]);
		}
		GTP_DEBUG("demo:%x %x %x %x", coor_data[60], coor_data[61], \
			coor_data[62], coor_data[63]);
#endif
		return -EPERM;
	}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if(fn_data.gdev->gesture_wakeup_en){
		gcore_gesture_event_handler(gdev->input_device, (coor_data[61] >> 3));
	}
#endif

#if GCORE_WDT_RECOVERY_ENABLE
	if (!gdev->ts_stat) {
		//GTP_DEBUG("mod delay work: wdt_wor");
		mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work, \
		  msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
		wdt_contin = 0; 		
	}
#endif


	for (i = 1; i <= GTP_MAX_TOUCH; i++) {
		GTP_REPORT("i:%d data:%x %x %x %x %x %x", i, coor_data[0], coor_data[1],
			  coor_data[2], coor_data[3], coor_data[4], coor_data[5]);
		
		if ((coor_data[0] == 0xFF) || (coor_data[0] == 0x00)) {
			coor_data += 6;
			continue;
		}

		touch_end = false;
		id = coor_data[0] >> 3;
		status = coor_data[0] & 0x03;
		input_x = ((coor_data[1] << 4) | (coor_data[3] >> 4));
		input_y = ((coor_data[2] << 4) | (coor_data[3] & 0x0F));

		GTP_REPORT("id:%d (x=%d,y=%d)", id, input_x, input_y);
		GTP_REPORT("tpd_x=%d tpd_y=%d lcm_x=%d lcm_y=%d", tpd_res_x, tpd_res_y, lcm_res_x, lcm_res_y);
		if (id > GTP_MAX_TOUCH) {
			coor_data += 6;
			continue;
		}

		input_x = GTP_TRANS_X(input_x, tpd_res_x, lcm_res_x);
		input_y = GTP_TRANS_Y(input_y, tpd_res_y, lcm_res_y);

		set_bit(id, &curr_touch);
		if(gdev->ts_stat == TS_SUSPEND){
			GTP_REPORT("TP enter Suspend now!");
		}else{
			gcore_touch_down(gdev->input_device, input_x, input_y, id - 1);
		}

		coor_data += 6;

	}

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	for (i = 1; i <= GTP_MAX_TOUCH; i++) {
		if (!test_bit(i, &curr_touch) && test_bit(i, &prev_touch)) {
			gcore_touch_up(gdev->input_device, i - 1);
		}

		if (test_bit(i, &curr_touch)) {
			set_bit(i, &prev_touch);
		} else {
			clear_bit(i, &prev_touch);
		}

		clear_bit(i, &curr_touch);
	}

	input_report_key(gdev->input_device, BTN_TOUCH, !touch_end);
#else
	if (touch_end) {
		gcore_touch_up(gdev->input_device, 0);
	}
#endif

	input_sync(gdev->input_device);

	return 0;
}

static irqreturn_t tpd_event_handler(int irq, void *dev_id)
{
	struct gcore_dev *gdev = (struct gcore_dev *)dev_id;

	if (mutex_is_locked(&gdev->transfer_lock)) {
		GTP_DEBUG("touch is locked, ignore");
		return IRQ_HANDLED;
	}

	mutex_lock(&gdev->transfer_lock);
	/* don't reset before "if (tpd_halt..."  */
	GTP_REPORT("tpd_event_handler enter.");

	if (gcore_touch_event_handler(gdev)) {
		GTP_ERROR("touch event handler error.");
	}

	mutex_unlock(&gdev->transfer_lock);

	return IRQ_HANDLED;
}

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	struct gcore_dev *gdev = (struct gcore_dev *)dev_id;
	struct gcore_exp_fn *exp_fn = NULL;
	struct gcore_exp_fn *exp_fn_temp = NULL;
	u8 found = 0;

/* GTP_DEBUG("tpd_eint_interrupt_handler."); */

	if (!list_empty(&fn_data.list)) {
		list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link) {
			if (exp_fn->wait_int == true) {
				exp_fn->event_flag = true;
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		gdev->tpd_flag = 1;
		return IRQ_WAKE_THREAD;
	} else {
		wake_up_interruptible(&gdev->wait);
		return IRQ_HANDLED;
	}

}

static int tpd_irq_registration(struct gcore_dev *gdev)
{
	int ret = 0;
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	struct device_node *node = NULL;
#endif

	GTP_DEBUG("Device Tree Tpd_irq_registration!");

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		gdev->touch_irq = irq_of_parse_and_map(node, 0);
		GTP_DEBUG("Device touch_irq = %d!", gdev->touch_irq);
	} else {
		GTP_ERROR("tpd request_irq can not find touch eint device node!.");
		ret = -1;
	}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	gdev->touch_irq = gpio_to_irq(gdev->irq_gpio);

#endif

	ret = devm_request_threaded_irq(&gdev->bus_device->dev, gdev->touch_irq,
					tpd_eint_interrupt_handler,
					tpd_event_handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "TOUCH_PANEL-eint", gdev);
	if (ret > 0) {
		ret = -1;
		GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	}

	return ret;
}

void gcore_gpio_rst_output(int rst, int level)
{

	GTP_DEBUG("gpio rst = %d output level = %d", rst, level);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	if (level) {
		tpd_gpio_output(rst, 1);
	} else {
		tpd_gpio_output(rst, 0);
	}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
	|| defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	if (level) {
		gpio_direction_output(rst, 1);
	} else {
		gpio_direction_output(rst, 0);
	}

#endif

}

int gcore_probe_device(void)
{
#if defined(CONFIG_ENABLE_CHIP_TYPE_GC7371)
	u16 chip_type = 0x7371;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7271)
	u16 chip_type = 0x7271;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202)
	u16 chip_type = 0x7202;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7372)
	u16 chip_type = 0x7372;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7302)
	u16 chip_type = 0x7302;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202H)
	u16 chip_type = 0x7203;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7272)
	u16 chip_type = 0x7272;
#endif
	u8 buf[2] = { 0 };
	u16 id = 0;
	int retries = 0;
	int ret = 0;

	while (retries++ < 3) {
		gcore_idm_read_id(buf, sizeof(buf));

		id = (buf[0] << 8) | buf[1];

		//if (id == chip_type) {
		if ((id != 0)&&((id & 0xff)!=0xff)) {
			GTP_DEBUG("GC%04x chip id match success,chip_type=%04x",id,chip_type);
			ret = 0;
			break;
		} else {
			GTP_ERROR("GC%04x chip id match fail! retries:%d", id, retries);
			ret = -1;
		}

		usleep_range(1000, 1100);
	}

	return ret;
}

#if GCORE_WDT_RECOVERY_ENABLE
void gcore_wdt_recovery_works(struct work_struct *work)
{
	struct gcore_dev *gdev = fn_data.gdev;

	if(gdev->fw_update_state == true){
		GTP_DEBUG("burn bin now");
		return ;
	}
	wdt_contin++;
#if GCORE_MP_TEST_ON
	mp_wait_int_set_fail();	
#endif

	if (!gdev->ts_stat) {
		GTP_ERROR("WDT timeout recovery ts:%d,wdtime:%d", gdev->ts_stat, wdt_contin);
		if (wdt_contin < 2) {
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	gcore_request_firmware_update_work(NULL);
#else
#if defined(CONFIG_ENABLE_CHIP_TYPE_GC7371) \
	|| defined(CONFIG_ENABLE_CHIP_TYPE_GC7272)
	gcore_reset();
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7271) \
	|| defined(CONFIG_ENABLE_CHIP_TYPE_GC7202) \
	|| defined(CONFIG_ENABLE_CHIP_TYPE_GC7372) \
	|| defined(CONFIG_ENABLE_CHIP_TYPE_GC7302) \
	|| defined(CONFIG_ENABLE_CHIP_TYPE_GC7202H)
	if(gcore_upgrade_soft_reset()) 
	{
		GTP_ERROR("hostdownload soft reset fail.");
		gcore_tp_esd_fail_count++;
		return;
	}
						
#endif
#endif
		} else {
#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
#ifdef CONFIG_MTK_PROXIMITY_TP_SCREEN_ON_ALSPS
        if(fn_data.gdev->PS_Enale == true){
            ps_report_interrupt_data(10);
            GTP_DEBUG("Wake up from proximity");
        }
#else
        if(fn_data.gdev->PS_Enale == true){
            GTP_DEBUG("Wake up from proximity");
			input_report_abs(gdev_intf->input_ps_device, ABS_DISTANCE, 1);
            input_sync(gdev_intf->input_ps_device);
        }
#endif
#endif
			gcore_trigger_esd_recovery();
			wdt_contin = 0;
		}
	}
#if CONFIG_GCORE_RESUME_EVENT_NOTIFY
	queue_delayed_work(fn_data.gdev->gtp_workqueue, &fn_data.gdev->resume_notify_work, msecs_to_jiffies(200));
#endif	
}
#endif

#ifdef CONFIG_GCORE_PSY_NOTIFY

static int gcore_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{

	struct power_supply *psy_usb = NULL;
	struct power_supply *psy_ac = NULL;
	struct gcore_dev *gdev = fn_data.gdev;
	
	psy_usb = power_supply_get_by_name("usb");
	if (!psy_usb) {
		GTP_DEBUG("Couldn't get usb psy");
		return -EINVAL;
	}

	psy_ac = power_supply_get_by_name("ac");
	if (!psy_ac) {
		GTP_DEBUG("Couldn't get ac psy");
		return -EINVAL;
	}

	GTP_DEBUG("USB device state change");

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}
	
	GTP_DEBUG("usb1");
	printk("%s %d name is %s\n",__func__,__LINE__,psy_usb->desc->name);
	printk("%s %d name is %s\n",__func__,__LINE__,psy_ac->desc->name);
	if ((strcmp(psy_usb->desc->name, "usb") == 0)
		|| (strcmp(psy_ac->desc->name, "ac") == 0)) {
	GTP_DEBUG("usb2");
		queue_delayed_work(gdev->gtp_workqueue, &gdev->charger_work, msecs_to_jiffies(500));
	}

	return NOTIFY_DONE;


	
}
static int gcore_earphone_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	int plug_flag = 0;
	struct gcore_dev *gdev = fn_data.gdev;
	plug_flag = (int)event;

	GTP_DEBUG("event id =%d\n", plug_flag);
    if (!fn_data.gdev) {
		GTP_DEBUG("fn_data is NULL\n");
        return NOTIFY_DONE;
    }

	if((plug_flag == 2) && (plug_flag != gdev->earphone_mode)){
		gdev->earphone_mode = plug_flag;
		gcore_extern_notify_event(10);
	}else if((plug_flag == 3) && (plug_flag != gdev->earphone_mode)){
		gdev->earphone_mode = plug_flag;
		gcore_extern_notify_event(10);
	}else{
		GTP_DEBUG("Invalid parameter");
	}
    return NOTIFY_DONE;
}

static int gcore_init_charger_notifier(void)
{
	int ret = 0;
	struct gcore_dev *gdev = fn_data.gdev;

	GTP_DEBUG("Init Charger notifier");

	gdev->charger_notifier.notifier_call = gcore_charger_notify_call;
	ret = power_supply_reg_notifier(&gdev->charger_notifier);

	gdev->earphone_mode = 2;
	gdev->earphone_notifier.notifier_call = gcore_earphone_notify_call;
	ret = usb_register_notifier_chain_for_tp(&gdev->earphone_notifier);
	return ret;
}


static bool gcore_get_charger_ststus(void)
{
	struct power_supply *psy_usb = NULL;
	struct power_supply *psy_ac = NULL;
	union power_supply_propval prop_usb;
	union power_supply_propval prop_ac;
	bool status = false;
	int ret = 0;

	psy_usb = power_supply_get_by_name("usb");
	if (!psy_usb) {
		GTP_ERROR("Couldn't get usb psy");
	}
	
	psy_ac = power_supply_get_by_name("ac");
	if (!psy_ac) {
		GTP_ERROR("Couldn't get ac psy");
	}

	if (!strcmp(psy_usb->desc->name, "usb")) {
		ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_ONLINE, &prop_usb);
		if (ret < 0) {
			GTP_ERROR("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d", ret);
		}
	}

	if (!strcmp(psy_ac->desc->name, "ac")) {
		ret = power_supply_get_property(psy_ac, POWER_SUPPLY_PROP_ONLINE, &prop_ac);
		if (ret < 0) {
			GTP_ERROR("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d", ret);
		}
	}

	if ((prop_usb.intval == POWER_SUPPLY_STATUS_CHARGING) ||((prop_ac.intval == POWER_SUPPLY_STATUS_CHARGING))||
		(prop_usb.intval == POWER_SUPPLY_STATUS_FULL)||((prop_ac.intval == POWER_SUPPLY_STATUS_FULL))) {
		status = true;
	} else {
		status = false;
	}
	GTP_DEBUG("charger status:%d", status);
	return status;
}


static void gcore_work_charger_detect_work(struct work_struct *work)
{
	struct gcore_dev *gdev = fn_data.gdev;
	bool charger_mode_old = gdev->charger_mode;

	gdev->charger_mode = gcore_get_charger_ststus();
	if ((gdev->charger_mode != charger_mode_old)) {
		if (gdev->charger_mode){
			GTP_DEBUG("Charger plug");
			if(gdev->ts_stat == TS_SUSPEND){
				gcore_modify_fw_event_cmd(FW_CHARGER_PLUG);
			}else{
				gcore_fw_event_notify(FW_CHARGER_PLUG);
			}
		}
		else{
			GTP_DEBUG("Charger unplug");
			if(gdev->ts_stat == TS_SUSPEND){
				gcore_modify_fw_event_cmd(FW_CHARGER_UNPLUG);
			}else{
				gcore_fw_event_notify(FW_CHARGER_UNPLUG);
			}

		}

	}
}



#endif

void gc_resume_workqueue_callback(struct work_struct *work)
{
	GTP_DEBUG("gcoregc_resume_workqueue_callback start\n");
	gcore_resume();

}

s32 gcore_init(struct gcore_dev *gdev)
{
	int data_size = DEMO_DATA_SIZE;

	GTP_DEBUG("gcore touch init.");

	gdev->touch_irq = 0;
	gdev->irq_flag = 1;

	spin_lock_init(&gdev->irq_flag_lock);
	mutex_init(&gdev->transfer_lock);
    mutex_init(&gdev->fwupdate_lock);
	mutex_init(&gdev->ITOTEST_lock);
	init_waitqueue_head(&gdev->wait);
#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	#ifdef CONFIG_PM_WAKELOCKS
		gdev->prx_wake_lock_ps = wakeup_source_register(NULL, "prx_wake_lock");
	#else
		wake_lock_init(&gdev->prx_wake_lock_ps, WAKE_LOCK_SUSPEND, "prx_wake_lock");
	#endif
	gcore_proxi_timer_init();
	register_notifier_callback();
#endif
	fn_data.gdev = gdev;
	fn_data.fn_inited = true;
	if (!fn_data.initialized) {
		INIT_LIST_HEAD(&fn_data.list);
		fn_data.initialized = true;
	}

	gdev->rst_output = gcore_gpio_rst_output;
	gdev->irq_enable = gcore_irq_enable;
	gdev->irq_disable = gcore_irq_disable;

#ifdef CONFIG_ENABLE_FW_RAWDATA
	gdev->fw_mode = DEMO;
	init_waitqueue_head(&gdev->usr_wait);
	gdev->usr_read = false;
	gdev->data_ready = false;

	data_size = DEMO_RAWDATA_MAX_SIZE;
#endif

	gdev->touch_data = kzalloc(data_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(gdev->touch_data)) {
		GTP_ERROR("Allocate touch data mem fail!");
		return -ENOMEM;
	}
/* gcore_gpio_rst_output(gdev->rst_gpio, 1); */

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	tpd_load_status = 1;
#endif

	charger_notify_wq_gc = create_singlethread_workqueue("gdev_charger_wq");
	gdev->fwu_workqueue = create_singlethread_workqueue("fwu_workqueue");
	gdev->gtp_workqueue = create_singlethread_workqueue("gtp workqueque");
	INIT_WORK(&gc_resume_work, gc_resume_workqueue_callback);
	INIT_DELAYED_WORK(&gdev->fwu_work, gcore_request_firmware_update_work);
#if GCORE_WDT_RECOVERY_ENABLE
	INIT_DELAYED_WORK(&gdev->wdt_work, gcore_wdt_recovery_works);
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
		INIT_DELAYED_WORK(&gdev->cb_work, gcore_CB_value_download_work);
#endif

#ifdef CONFIG_GCORE_PSY_NOTIFY
		INIT_DELAYED_WORK(&gdev->charger_work, gcore_work_charger_detect_work);
		gcore_init_charger_notifier();
//		queue_delayed_work(gdev->gtp_workqueue, &gdev->charger_work, msecs_to_jiffies(500));
	
#endif


	INIT_DELAYED_WORK(&gdev->resume_notify_work,gcore_resume_event_notify_work);

//tian add notify fw event 
	INIT_DELAYED_WORK(&gdev->event_work, gcore_fw_event_notify_work);

	gdev->ts_stat = TS_NORMAL;

	return 0;
}
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
static void *tx = NULL;
static void *rx = NULL;
#endif
void gcore_deinit(struct gcore_dev *gdev)
{
	struct gcore_exp_fn *exp_fn = NULL;
	struct gcore_exp_fn *exp_fn_temp = NULL;
	GTP_DEBUG("enter gcore deinit");
#if GCORE_WDT_RECOVERY_ENABLE
	gdev->ts_stat = 1;
	cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
	cancel_delayed_work_sync(&fn_data.gdev->cb_work);
	if(gdev->CB_value)
		kfree(gdev->CB_value);
#endif

	cancel_delayed_work_sync(&fn_data.gdev->fwu_work);
	cancel_delayed_work_sync(&fn_data.gdev->event_work);
	gdev->irq_disable(gdev);

	if (charger_notify_wq_gc) {
		flush_workqueue(charger_notify_wq_gc );
		destroy_workqueue(charger_notify_wq_gc );
	}
	if (!list_empty(&fn_data.list)) {
		list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link) {
			if (exp_fn->remove != NULL) {
				exp_fn->remove(gdev);
			}
		}
	}
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	destroy_workqueue(gdev->fwu_workqueue);
#endif

#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
	if(tx)
		kfree(tx);
	if(rx)
		kfree(rx);
#endif
	kfree(gdev->touch_data);

#ifndef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	gcore_free_gpio(gdev);
#endif

#ifdef CONFIG_DETECT_FW_RESTART_INIT_EN
	fw_init_count_timer_exit();
#endif
#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	gcore_proxi_timer_exit();
	unregister_notifier_callback();
#endif

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#ifdef CONFIG_FB
	fb_unregister_client(&gdev->fb_notifier);
#endif

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)
#ifdef TP_RESUME_BY_FB_NOTIFIER // CONFIG_FB
	fb_unregister_client(&gdev->fb_notifier);
#endif
#if defined(CONFIG_DRM)
	mtk_disp_notifier_unregister(&gdev->drm_notifier);
#endif
#endif
#ifdef CONFIG_GCORE_PSY_NOTIFY
	usb_unregister_notifier_chain_for_tp(&gdev->earphone_notifier);
#endif
	gcore_input_device_deinit(gdev);

	kfree(gdev);
	
}

int gcore_touch_probe(struct gcore_dev *gdev)
{
#if 0
	struct gcore_exp_fn *exp_fn = NULL;
	struct gcore_exp_fn *exp_fn_temp = NULL;
#endif
#if defined(CONFIG_DRM)
	int ret = 0;
#endif
	GTP_DEBUG("tpd_registration start.");

	GTP_DEBUG("tp drv ver:%s", TOUCH_DRIVER_RELEASE_VERISON);
	
	gcore_get_dts_info(gdev);

	gcore_input_device_init(gdev);

	if (gcore_init(gdev)) {
		GTP_ERROR("gcore init fail!");
	}
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	tpd_gpio_as_int(GTP_INT_PORT);
#endif

	msleep(50);

	/* EINT device tree, default EINT enable */
	tpd_irq_registration(gdev);

	list_add_tail(&p_fwu_fn->link, &fn_data.list);
	if (p_fwu_fn->init != NULL) {
		p_fwu_fn->init(fn_data.gdev);
	}

	list_add_tail(&p_intf_fn->link, &fn_data.list);
	if (p_intf_fn->init != NULL) {
		p_intf_fn->init(fn_data.gdev);
	}
#if GCORE_MP_TEST_ON
	list_add_tail(&p_mp_fn->link, &fn_data.list);
	if (p_mp_fn->init != NULL) {
		p_mp_fn->init(fn_data.gdev);
	}
#endif
#ifdef CONFIG_DETECT_FW_RESTART_INIT_EN
	fw_init_count_timer_init(gdev);
#endif
#if 0
	if (!list_empty(&fn_data.list)) {
		list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link) {
			if (exp_fn->init != NULL) {
				exp_fn->init(gdev);
			}
		}
	}
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM
#ifdef TP_RESUME_BY_FB_NOTIFIER // CONFIG_FB
	GTP_DEBUG("Init notifier_fb struct");
	gdev->fb_notifier.notifier_call = gcore_ts_fb_notifier_callback;
	if (fb_register_client(&gdev->fb_notifier)) {
		GTP_ERROR("[FB]Unable to register fb_notifier.");
	}
#else
	if (gcore_sysfs_add_device(&gdev->bus_device->dev)) {
		GTP_ERROR("gcore sysfs add device fail!");
	}	
#endif
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM
#if defined(CONFIG_DRM)
	GTP_DEBUG("Init notifier_drm struct");
	gdev->drm_notifier.notifier_call = gcore_ts_drm_notifier_callback;
	ret = mtk_disp_notifier_register("gc_ts_notifier", &gdev->drm_notifier);
	if (ret < 0) {
		GTP_ERROR("register notifier failed!");
	}
#elif defined(CONFIG_FB)

	GTP_DEBUG("Init notifier_fb struct");
	gdev->fb_notifier.notifier_call = gcore_ts_fb_notifier_callback;
	if (fb_register_client(&gdev->fb_notifier)) {
		GTP_ERROR("[FB]Unable to register fb_notifier.");
	}
#endif
#endif
	proc_node_init();

	return 0;
}

/*-------------------------------------------------*/
/*  Touch Driver I2C and SPI Function              */
/*-------------------------------------------------*/
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)

/*
 * I2C address
 * GC7202:0x26(0x78 unuse)
 * GC7271:0x78(0x26 unuse)
 */

struct i2c_client *gcore_i2c_client;

#define GTP_I2C_ADDRESS      0xF0	/* slave address 0x78 << 1 */
static const struct i2c_device_id tpd_i2c_id[] = { {GTP_DRIVER_NAME, 0}, {} };
static unsigned short force[] = { 0, GTP_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

s32 gcore_i2c_read(u8 *buffer, s32 len)
{
	s32 data_length = len;
	s32 ret = 0;

	struct i2c_msg msgs[1] = {
		{
		 .addr = gcore_i2c_client->addr,
		 .flags = I2C_M_RD,
		 .buf = buffer,
		 .len = data_length,
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM
		 .scl_rate = 400000,
#endif
		 },
	};

/*
* According to i2c-mtk.c __mt_i2c_transfer(),mtk i2c always use DMA mode.
* Mtk i2c will copy the buffer to DMA memory automatic. so our buffer needn't
* allocate DMA memory.i2c max transfer size is 4K.
*/
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	if (len > 4096) {
		GTP_ERROR("mtk i2c transfer size limit 4k!");
		return -EPERM;
	}
#endif

	ret = i2c_transfer(gcore_i2c_client->adapter, msgs, 1);
	if (ret != 1) {
		GTP_ERROR("I2C Transfer error!");
		return -EPERM;
	}

	return 0;

}

s32 gcore_i2c_write(u8 *buffer, s32 len)
{
	s32 ret = 0;

	struct i2c_msg msgs = {
		.flags = 0,
		.addr = gcore_i2c_client->addr,
		.len = len,
		.buf = buffer,
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM
		.scl_rate = 100000,
#endif
	};

/*
* According to i2c-mtk.c __mt_i2c_transfer(),mtk i2c always use DMA mode.
* Mtk i2c will copy the buffer to DMA memory automatic. so our buffer needn't
* allocate DMA memory.i2c max transfer size is 4K.
*/
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
	if (len > 4096) {
		GTP_ERROR("mtk i2c transfer size limit 4k!");
		return -EPERM;
	}
#endif

	ret = i2c_transfer(gcore_i2c_client->adapter, &msgs, 1);
	if (ret != 1) {
		GTP_ERROR("I2C Transfer error!");
		return -EPERM;
	}

	return 0;

}

static s32 gcore_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct gcore_dev *touch_dev = NULL;

	GTP_DEBUG("tpd_i2c_probe start.");

	gcore_i2c_client = client;
	GTP_DEBUG("gcore_i2c_client->addr:0x%x", gcore_i2c_client->addr);
	if(gcore_i2c_client->addr != 0x26){
		 gcore_i2c_client->addr = 0x26;
		 GTP_DEBUG("gcore_i2c_client->addr:0x%x", gcore_i2c_client->addr);
	}

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD
	GTP_DEBUG("create proc enpty");
	if(create_tpd_proc_entry()){
		GTP_ERROR("proc node init failed");
	}
#endif

	/* GC7371 use rst pin and need init before use */
#ifndef CONFIG_ENABLE_CHIP_TYPE_GC7371
	if (gcore_probe_device()) {
		GTP_ERROR("gcore probe device fail!");
		return -EPERM;
	}
#endif

	touch_dev = kzalloc(sizeof(struct gcore_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(touch_dev)) {
		GTP_ERROR("allocate touch_dev mem fail!");
		return -ENOMEM;
	}

	touch_dev->bus_device = client;

	i2c_set_clientdata(client, touch_dev);
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD
	touch_dev->tpd_proc_dir = proc_gcorei2c_dir;
#endif
	if (gcore_touch_probe(touch_dev)) {
		GTP_ERROR("touch registration fail!");
		return -EPERM;
	}

	return 0;
}

static int gcore_i2c_remove(struct i2c_client *client)
{
	struct gcore_dev *gdev = i2c_get_clientdata(client);
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD
	remove_proc_entry("Erase_flash",gdev->tpd_proc_dir);
#endif
	gcore_deinit(gdev);

	return 0;
}

static int gcore_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strlcpy(info->type, "mtk-tpd", sizeof(info->type));
	return 0;
}

static struct i2c_driver tpd_i2c_driver = {
	.probe = gcore_i2c_probe,
	.remove = gcore_i2c_remove,
	.detect = gcore_i2c_detect,
	.driver.name = GTP_DRIVER_NAME,
	.driver = {
		   .name = GTP_DRIVER_NAME,
		   .of_match_table = tpd_of_match,
		   },
	.id_table = tpd_i2c_id,
	.address_list = (const unsigned short *)forces,
};

#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)

struct spi_device *gcore_spi_slave;

/*
* mtk spi use DMA mode when transfer length over 32 bytes.
* out buffer should allocate DMA memory over 32 bytes.
* TODO:put the allocate memory in init or probe will be better.
*/

static s32 gcore_spi_setup_xfer(struct spi_transfer *xfer, u32 len)
{

	if (IS_ERR_OR_NULL(tx)) {
		tx = kzalloc(FW_SIZE, GFP_KERNEL | GFP_DMA);
	}
	if (tx == NULL) {
		GTP_ERROR("tx kzalloc fail!");
		return -ENOMEM;
	}
		
	if (IS_ERR_OR_NULL(rx)) {
		rx = kzalloc(FW_SIZE, GFP_KERNEL | GFP_DMA);
	}
	if (rx == NULL) {
		GTP_ERROR("rx kzalloc fail!");
		kfree(tx);
		return -ENOMEM;
	}
	
	xfer->len = len;
	xfer->tx_buf = tx;
	xfer->rx_buf = rx;
	
	return 0;

}

s32 gcore_spi_read(u8 *buffer, s32 len)
{
	struct spi_transfer transfer = { 0, };
	struct spi_message msg;
	int ret = 0;

/* GTP_DEBUG("spi read. len = %d", len); */

#ifdef CONFIG_MTK_LEGACY_PLATFORM
	/* MTK spi.c if transfer > 1024 bytes, the lens must be a multiple of 1024  */
	if ((len > 1024) && (len % 1024)) {
		GTP_ERROR("mtk spi tansfer len must be a multiple of 1024!");
		return -EPERM;
	}
#endif

	spi_message_init(&msg);
	ret = gcore_spi_setup_xfer(&transfer, len);
	if (ret) {
		GTP_ERROR("gcore spi setup xfer fail!");
		return -EPERM;
	}

	memset((void *)transfer.tx_buf,0xff,len);
	
	spi_message_add_tail(&transfer, &msg);
	ret = spi_sync(gcore_spi_slave, &msg);
	if (ret < 0) {
		GTP_ERROR("spi message transfer error!");
	}

	memcpy(buffer, transfer.rx_buf, len);

	return 0;
}

s32 gcore_spi_write(const u8 *buffer, s32 len)
{
	struct spi_transfer transfer = { 0, };
	struct spi_message msg;
	int ret = 0;

#ifdef CONFIG_MTK_LEGACY_PLATFORM
	/* MTK spi.c if transfer > 1024 bytes, the lens must be a multiple of 1024  */
	if ((len > 1024) && (len % 1024)) {
		GTP_ERROR("mtk spi tansfer len must be a multiple of 1024!");
		return -EPERM;
	}
#endif

	spi_message_init(&msg);
	ret = gcore_spi_setup_xfer(&transfer, len);
	if (ret) {
		GTP_ERROR("gcore spi setup xfer fail!");
		return -EPERM;
	}

	memcpy((u8 *) transfer.tx_buf, buffer, len);

	spi_message_add_tail(&transfer, &msg);
	ret = spi_sync(gcore_spi_slave, &msg);
	if (ret < 0) {
		GTP_ERROR("spi message transfer error!");
	}

	return 0;
}

#define SPI_DUMMY  2

s32 gcore_spi_write_then_read(u8 *txbuf, s32 n_tx, u8 *rxbuf, s32 n_rx)
{
	struct spi_transfer transfer = { 0, };
	struct spi_message msg;
	int ret = 0;
	int total = n_tx + n_rx + SPI_DUMMY;

	spi_message_init(&msg);
	ret = gcore_spi_setup_xfer(&transfer, total);
	if (ret) {
		GTP_ERROR("gcore spi setup xfer fail!");
		return -EPERM;
	}

	memcpy((u8 *) transfer.tx_buf, txbuf, n_tx);

	spi_message_add_tail(&transfer, &msg);
	ret = spi_sync(gcore_spi_slave, &msg);
	if (ret < 0) {
		GTP_ERROR("spi message transfer error!");
	}

	memcpy(rxbuf, transfer.rx_buf + n_tx + SPI_DUMMY, n_rx);

	return 0;
}

s32 gcore_spi_write_and_read(u8 *txbuf, s32 n_tx, u8 *rxbuf, s32 n_rx)
{
	struct spi_transfer transfer = { 0, };
	struct spi_message msg;
	int ret = 0;
	int total = n_tx;

	if(n_tx < n_rx)
		total = n_rx;
	spi_message_init(&msg);
	ret = gcore_spi_setup_xfer(&transfer, total);
	if (ret) {
		GTP_ERROR("gcore spi setup xfer fail!");
		return -EPERM;
	}

	memcpy((u8 *) transfer.tx_buf, txbuf, n_tx);

	spi_message_add_tail(&transfer, &msg);
	ret = spi_sync(gcore_spi_slave, &msg);
	if (ret < 0) {
		GTP_ERROR("spi message transfer error!");
	}

	memcpy(rxbuf, transfer.rx_buf, n_rx);

	return 0;
}
void gcore_spi_config(void)
{
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_MTK_LEGACY_PLATFORM
	struct mt_chip_conf *spi_config;

	spi_config = (struct mt_chip_conf *)gcore_spi_slave->controller_data;
	if (!spi_config) {
		pr_err("spi config is error!!!");
	}

	spi_config->cpol = 1;
	spi_config->cpha = 1;
	spi_config->holdtime = 5000;
	spi_config->setuptime = 5000;
	spi_config->high_time = 40;
	spi_config->low_time = 40;

	return;
#endif
#endif

	gcore_spi_slave->mode = SPI_MODE_3;
	gcore_spi_slave->bits_per_word = 8;
	gcore_spi_slave->max_speed_hz = 6000000;
	gcore_spi_slave->chip_select = 0;
	spi_setup(gcore_spi_slave);

}

static s32 gcore_spi_probe(struct spi_device *slave)
{
	struct gcore_dev *touch_dev = NULL;

	GTP_DEBUG("tpd_spi_probe start.");

	gcore_spi_slave = slave;
	gcore_spi_config();

	/* GC7371 use rst pin and need init before use */
#ifndef CONFIG_ENABLE_CHIP_TYPE_GC7371
	if (gcore_probe_device()) {
		GTP_ERROR("gcore probe device fail!");
		return -EPERM;
	}
#endif

	touch_dev = kzalloc(sizeof(struct gcore_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(touch_dev)) {
		GTP_ERROR("allocate touch_dev mem fail!");
		return -ENOMEM;
	}

	touch_dev->bus_device = slave;

	spi_set_drvdata(slave, touch_dev);
	
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
		if (IS_ERR_OR_NULL(touch_dev->CB_value)) {
			touch_dev->CB_value = kzalloc(CB_SIZE, GFP_KERNEL);
		}
		if (IS_ERR_OR_NULL(touch_dev->CB_value)) {
			GTP_ERROR("CB_value mem allocate fail");
		} else {
			memset(touch_dev->CB_value, 0, CB_SIZE);
			touch_dev->CB_value[0]	=	0xCB;
			touch_dev->CB_value[1]	=	0x00;
		}
	
#endif

	if (gcore_touch_probe(touch_dev)) {
		GTP_ERROR("touch registration fail!");
		return -EPERM;
	}

	return 0;
}

static void gcore_spi_remove(struct spi_device *slave)
{
	struct gcore_dev *gdev = spi_get_drvdata(slave);

	gcore_deinit(gdev);

//	return 0;
}

#if 0
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_MTK_LEGACY_PLATFORM
static struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
	       .modalias = GTP_DRIVER_NAME,
	       .bus_num = 1,
	       .chip_select = 1,
	       .mode = SPI_MODE_3,
	       },
};
#endif
#endif
#endif

static struct spi_device_id tpd_spi_id = { GTP_DRIVER_NAME, 0 };

static struct spi_driver tpd_spi_driver = {
	.probe = gcore_spi_probe,
	.id_table = &tpd_spi_id,
	.driver = {
		   .name = GTP_DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = tpd_of_match,
		   },
	.remove = gcore_spi_remove,
};

#endif /* CONFIG_TOUCH_DRIVER_INTERFACE_I2C */

s32 gcore_bus_read(u8 *buffer, s32 len)
{
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
	return gcore_i2c_read(buffer, len);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
	return gcore_spi_read(buffer, len);
#endif

}

EXPORT_SYMBOL(gcore_bus_read);

s32 gcore_bus_write(const u8 *buffer, s32 len)
{
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
	return gcore_i2c_write((u8 *) buffer, len);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
	return gcore_spi_write(buffer, len);
#endif

}

EXPORT_SYMBOL(gcore_bus_write);

int gcore_touch_bus_init(void)
{
	GTP_DEBUG("touch_bus_init start.");

	if(strstr(wt_saved_command_line,"n28_gc7272_dsi_vdo_hdp_txd_sharp")){
		gc_lcm_name = 4;
	}else{
        GTP_DEBUG("match lcm is failed");
		return -1;
	}

#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
	return i2c_add_driver(&tpd_i2c_driver);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)

#if 0
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_MTK_LEGACY_PLATFORM
	spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
#endif
#endif
#endif

	return spi_register_driver(&tpd_spi_driver);
#endif

}

void gcore_touch_bus_exit(void)
{
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
	i2c_del_driver(&tpd_i2c_driver);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
	spi_unregister_driver(&tpd_spi_driver);
#endif

}
