// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

// #include <mt-plat/sync_write.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <linux/iommu.h>
#include <soc/mediatek/smi.h>

#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "ccu_drv.h"
#include "ccu_cmn.h"
#include "ccu_reg.h"
#include "ccu_platform_def.h"
#include "ccu_imgsensor.h"
#include "kd_camera_feature.h"/*for IMGSENSOR_SENSOR_IDX*/
#include "ccu_mva.h"
#include "ccu_qos.h"
#include "ccu_ipc.h"
//for mmdvfs
#ifdef CCU_QOS_SUPPORT_ENABLE
#include "mtk-interconnect.h"
#endif
/***************************************************************************
 *
 **************************************************************************/

#define CCU_DEV_NAME            "ccu"
#define CCU1_DEV_NAME           "ccu1"

static int32_t _user_count;
/* [0]: CCU_CLK_TOP_MUX, [1]: MDP_PWR, [2]: CAM_PWR, [3]: CCU_CLK_CAM_CCU */
struct clk *ccu_clk_pwr_ctrl[CCU_CLK_PWR_NUM];

struct ccu_device_s *g_ccu_device;
static struct ccu_power_s power;
static uint32_t ccu_hw_base;

static wait_queue_head_t wait_queue_deque;
static wait_queue_head_t wait_queue_enque;

#include <linux/dma-buf.h>

static uint32_t ccu_version;
static uint32_t clock_num;
static struct mtk_ccu_clk_name MT6833_ccu_clock_name[] = {
	{true, "TOP_CCU_CLK"},
	{true, "CCU_CLK_CAM_CAM"},
	{true, "CCU_CLK_CAM_LARB13"},
	{true, "CCU_CLK_CAM_CCU0"},
	{true, "CCU_CLK_CAM_GALS"},
	{true, "CCU_CLK_CAM_CCU_GALS"},
	{false, ""}};

static struct mtk_ccu_clk_name MT6853_ccu_clock_name[] = {
	{true, "TOP_CCU_CLK"},
	{true, "CCU_CLK_CAM_CAM"},
	{true, "CCU_CLK_CAM_LARB13"},
	{true, "CCU_CLK_CAM_CCU0"},
	{true, "CCU_CLK_CAM_GALS"},
	{true, "CCU_CLK_CAM_CCU_GALS"},
	{false, ""}};

static struct mtk_ccu_clk_name MT6877_ccu_clock_name[] = {
	{true, "TOP_CCU_CLK"},
	{true, "CCU_CLK_CAM_CAM"},
	{true, "CCU_CLK_CAM_LARB13"},
	{true, "CCU_CLK_CAM_CCU0"},
	{true, "CCU_CLK_CAM_GALS"},
	{true, "CCU_CLK_CAM_CCU_GALS"},
	{false, ""}};

static struct mtk_ccu_clk_name MT6893_ccu_clock_name[] = {
	{true, "TOP_CCU_CLK"},
	{true, "CCU_CLK_CAM_CAM"},
	{true, "CCU_CLK_CAM_LARB13"},
	{true, "CCU_CLK_CAM_CCU0"},
	{false, ""}};

static struct mtk_ccu_clk_name clock_name[CCU_CLK_PWR_NUM];

struct ccu_iova_t ccu_iova[CCU_IOVA_BUFFER_MAX];
int iova_buf_count;

// static struct ion_handle
//	*import_buffer_handle[CCU_IMPORT_BUF_NUM];

// #if IS_ENABLED(CONFIG_PM_WAKELOCKS)
// struct wakeup_source ccu_wake_lock;
// #else
// struct wake_lock ccu_wake_lock;
// #endif
/*static int g_bWaitLock;*/

static irqreturn_t ccu_isr_callback_xxx(int rrq,
					void *device_id);

typedef irqreturn_t(*ccu_isr_fp_t) (int, void *);

struct ccu_isr_callback_t {
	ccu_isr_fp_t irq_fp;
	unsigned int int_number;
	char device_name[16];
};

/* int number is got from kernel api */
const struct ccu_isr_callback_t
	ccu_isr_callbacks[CCU_IRQ_NUM_TYPES] = {
	/* The last used be mapping to device node.*/
	/* Must be the same name with that in device node. */
	{ccu_isr_callback_xxx, 0, "ccu2"}
};

static irqreturn_t ccu_isr_callback_xxx(int irq, void *device_id)
{
	LOG_DBG("%s:0x%x\n", __func__, irq);
	return IRQ_HANDLED;
}

static uint32_t get_checksum(uint8_t *data, uint32_t size)
{
	uint32_t i, val, tv, sz, *ptr;

	if (!data)
		return 0;

	sz = size >> 2;
	ptr = (uint32_t *)data;
	for (i = 0, val = 0; i < sz; ++i)
		val += ptr[i];
	for (i = size & ~3, tv = 0; i < size; ++i)
		tv += ((uint32_t)(data[i])) << ((i & 3) << 3);
	val = (val + tv) ^ CCU_MAGIC_CHK;

	return val;
}

#ifdef CCU_QOS_SUPPORT_ENABLE
static struct regulator *_ccu_qos_request;
static u64 *_g_freq_steps;
static u32 _step_size;
#endif
static int ccu_probe(struct platform_device *dev);

static int ccu_remove(struct platform_device *dev);

static int ccu_suspend(struct platform_device *dev,
		       pm_message_t mesg);

static int ccu_resume(struct platform_device *dev);
static int ccu1_probe(struct platform_device *dev);
static int ccu1_remove(struct platform_device *dev);
static int ccu1_suspend(struct platform_device *dev,
		       pm_message_t mesg);
static int ccu1_resume(struct platform_device *dev);
static int32_t _clk_count;
/*-------------------------------------------------------------------------*/
/* CCU Driver: pm operations                                               */
/*-------------------------------------------------------------------------*/
#if IS_ENABLED(CONFIG_PM)
int ccu_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ccu_suspend(pdev, PMSG_SUSPEND);
}

int ccu_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ccu_resume(pdev);
}

/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
int ccu_pm_restore_noirq(struct device *device)
{
#if !IS_ENABLED(CONFIG_OF)
	mt_irq_set_sens(CAM0_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(CAM0_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;
}

int ccu1_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ccu1_suspend(pdev, PMSG_SUSPEND);
}

int ccu1_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ccu1_resume(pdev);
}

/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
int ccu1_pm_restore_noirq(struct device *device)
{
#if !IS_ENABLED(CONFIG_OF)
	// mt_irq_set_sens(CAM0_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	// mt_irq_set_polarity(CAM0_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;
}
#else
#define ccu_pm_suspend NULL
#define ccu_pm_resume  NULL
#define ccu_pm_restore_noirq NULL
#define ccu1_pm_suspend NULL
#define ccu1_pm_resume  NULL
#define ccu1_pm_restore_noirq NULL
#endif

const struct dev_pm_ops ccu_pm_ops = {
	.suspend = ccu_pm_suspend,
	.resume = ccu_pm_resume,
	.freeze = ccu_pm_suspend,
	.thaw = ccu_pm_resume,
	.poweroff = ccu_pm_suspend,
	.restore = ccu_pm_resume,
	.restore_noirq = ccu_pm_restore_noirq,
};

const struct dev_pm_ops ccu1_pm_ops = {
	.suspend = ccu1_pm_suspend,
	.resume = ccu1_pm_resume,
	.freeze = ccu1_pm_suspend,
	.thaw = ccu1_pm_resume,
	.poweroff = ccu1_pm_suspend,
	.restore = ccu1_pm_resume,
	.restore_noirq = ccu1_pm_restore_noirq,
};
/*---------------------------------------------------------------------------*/
/* CCU Driver: Prototype                                                     */
/*---------------------------------------------------------------------------*/

static const struct of_device_id ccu_of_ids[] = {
	{.compatible = "mediatek,ccu",},
	{.compatible = "mediatek,ccu_camsys",},
	{.compatible = "mediatek,n3d_ctl_a",},
	{}
};

static struct platform_driver ccu_driver = {
	.probe = ccu_probe,
	.remove = ccu_remove,
	.suspend = ccu_suspend,
	.resume = ccu_resume,
	.driver = {
		.name = CCU_DEV_NAME,
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = ccu_of_ids,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &ccu_pm_ops,
#endif
	}
};

static const struct of_device_id ccu1_of_ids[] = {
	{.compatible = "mediatek,ccu1",},
	{}
};

static struct platform_driver ccu1_driver = {
	.probe = ccu1_probe,
	.remove = ccu1_remove,
	.suspend = ccu1_suspend,
	.resume = ccu1_resume,
	.driver = {
		.name = CCU1_DEV_NAME,
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = ccu1_of_ids,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &ccu1_pm_ops,
#endif
	}
};
/*---------------------------------------------------------------------------*/
/* CCU Driver: file operations                                               */
/*---------------------------------------------------------------------------*/
static int ccu_open(struct inode *inode, struct file *flip);

static int ccu_release(struct inode *inode, struct file *flip);

static long ccu_ioctl(struct file *flip, unsigned int cmd,
		      unsigned long arg);

#if IS_ENABLED(CONFIG_COMPAT)
static long ccu_compat_ioctl(struct file *flip, unsigned int cmd,
			     unsigned long arg);
#endif

static const struct file_operations ccu_fops = {
	.owner = THIS_MODULE,
	.open = ccu_open,
	.release = ccu_release,
	.unlocked_ioctl = ccu_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	/*for 32bit usersapce program doing ioctl, compat_ioctl will be called*/
	.compat_ioctl = ccu_compat_ioctl
#endif
};

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int ccu_num_users;

int ccu_create_user(struct ccu_user_s **user)
{
	struct ccu_user_s *u;

	u = kmalloc(sizeof(vlist_type(struct ccu_user_s)), GFP_ATOMIC);
	if (!u)
		return -ENOMEM;

	mutex_init(&u->data_mutex);
	/*mutex_lock(&u->data_mutex);*/
	u->id = ++ccu_num_users;
	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	u->running = false;
	u->flush = false;
	INIT_LIST_HEAD(&u->enque_ccu_cmd_list);
	INIT_LIST_HEAD(&u->deque_ccu_cmd_list);
	init_waitqueue_head(&u->deque_wait);
	/*mutex_unlock(&u->data_mutex);*/

	mutex_lock(&g_ccu_device->user_mutex);
	list_add_tail(vlist_link(u, struct ccu_user_s),
		&g_ccu_device->user_list);
	mutex_unlock(&g_ccu_device->user_mutex);

	*user = u;
	return 0;
}


int ccu_push_command_to_queue(struct ccu_user_s *user,
			      struct ccu_cmd_s *cmd)
{
	if (!user) {
		LOG_ERR("empty user");
		return -1;
	}

	LOG_DBG("+:%s\n", __func__);

	mutex_lock(&user->data_mutex);
	list_add_tail(vlist_link(cmd, struct ccu_cmd_s),
		&user->enque_ccu_cmd_list);
	mutex_unlock(&user->data_mutex);

	spin_lock(&g_ccu_device->cmd_wait.lock);
	wake_up_locked(&g_ccu_device->cmd_wait);
	spin_unlock(&g_ccu_device->cmd_wait.lock);

	return 0;
}

int ccu_flush_commands_from_queue(struct ccu_user_s *user)
{

	struct list_head *head, *temp;
	struct ccu_cmd_s *cmd;

	mutex_lock(&user->data_mutex);

	if (!user->running && list_empty(&user->enque_ccu_cmd_list)
	    && list_empty(&user->deque_ccu_cmd_list)) {
		mutex_unlock(&user->data_mutex);
		return 0;
	}

	user->flush = true;
	mutex_unlock(&user->data_mutex);

	/* the running command will add to the deque before interrupt */
	wait_event_interruptible(user->deque_wait, !user->running);

	mutex_lock(&user->data_mutex);
	/* push the remaining enque to the deque */
	list_for_each_safe(head, temp, &user->enque_ccu_cmd_list) {
		cmd = vlist_node_of(head, struct ccu_cmd_s);
		cmd->status = CCU_ENG_STATUS_FLUSH;
		list_del_init(vlist_link(cmd, struct ccu_cmd_s));
		list_add_tail(vlist_link(cmd, struct ccu_cmd_s),
			&user->deque_ccu_cmd_list);
	}

	user->flush = false;
	mutex_unlock(&user->data_mutex);
	return 0;
}

int ccu_pop_command_from_queue(struct ccu_user_s *user,
			       struct ccu_cmd_s **rcmd)
{
	int ret;
	struct ccu_cmd_s *cmd;

	/* wait until condition is true */
	ret = wait_event_interruptible_timeout(user->deque_wait,
					!list_empty(&user->deque_ccu_cmd_list),
					msecs_to_jiffies(3 * 1000));

	/* ret == 0, if timeout; ret == -ERESTARTSYS, if signal interrupt */
	if (ret == 0) {
		LOG_ERR("timeout: pop a command! ret=%d\n", ret);
		*rcmd = NULL;
		return -1;
	} else if (ret < 0) {
		LOG_ERR("interrupted by system signal: %d\n", ret);

		if (ret == -ERESTARTSYS)
			LOG_ERR("interrupted as -ERESTARTSYS\n");

		return ret;
	}
	mutex_lock(&user->data_mutex);
	/* This part should not be happened */
	if (list_empty(&user->deque_ccu_cmd_list)) {
		mutex_unlock(&user->data_mutex);
		LOG_ERR("pop a command from empty queue! ret=%d\n", ret);
		*rcmd = NULL;
		return -1;
	};

	/* get first node from deque list */
	cmd = vlist_node_of(user->deque_ccu_cmd_list.next,
			    struct ccu_cmd_s);
	list_del_init(vlist_link(cmd, struct ccu_cmd_s));

	mutex_unlock(&user->data_mutex);

	*rcmd = cmd;
	return 0;
}


int ccu_delete_user(struct ccu_user_s *user)
{

	if (!user) {
		LOG_ERR("delete empty user!\n");
		return -1;
	}
	/* TODO: notify dropeed command to user?*/
	/* ccu_dropped_command_notify(user, command);*/
	ccu_flush_commands_from_queue(user);

	mutex_lock(&g_ccu_device->user_mutex);
	list_del(vlist_link(user, struct ccu_user_s));
	mutex_unlock(&g_ccu_device->user_mutex);

	kfree(user);

	return 0;
}

int ccu_lock_user_mutex(void)
{
	mutex_lock(&g_ccu_device->user_mutex);
	return 0;
}

int ccu_unlock_user_mutex(void)
{
	mutex_unlock(&g_ccu_device->user_mutex);
	return 0;
}

int ccu_lock_ion_client_mutex(void)
{
	mutex_lock(&g_ccu_device->ion_client_mutex);
	return 0;
}

int ccu_unlock_ion_client_mutex(void)
{
	mutex_unlock(&g_ccu_device->ion_client_mutex);
	return 0;
}

/*---------------------------------------------------------------------------*/
/* IOCTL: implementation                                                     */
/*---------------------------------------------------------------------------*/
int ccu_set_power(struct ccu_power_s *power)
{
	return ccu_power(power);
}

static int ccu_open(struct inode *inode, struct file *flip)
{
	int ret = 0;
	struct ccu_user_s *user;
	struct CcuMemHandle handle = {0};

	mutex_lock(&g_ccu_device->dev_mutex);

	LOG_INF_MUST("%s pid:%d tid:%d cnt:%d+\n",
		__func__, current->pid, current->tgid, _user_count);

	ccu_create_user(&user);
	if (IS_ERR_OR_NULL(user)) {
		LOG_ERR("fail to create user\n");
		mutex_unlock(&g_ccu_device->dev_mutex);
		return -ENOMEM;
	}

	flip->private_data = user;

	_user_count++;

	if (_user_count > 1) {
		LOG_INF_MUST("%s clean legacy data flow-\n", __func__);
		ccu_force_powerdown();

		handle.meminfo.cached = 0;
		ccu_deallocate_mem(g_ccu_device, &handle);
		handle.meminfo.cached = 1;
		ccu_deallocate_mem(g_ccu_device, &handle);
	}

	// _clk_count = 0;
	iova_buf_count = 0;

	// ccu_ion_init();

	// for (int i = 0; i < CCU_IMPORT_BUF_NUM; i++)
	//	import_buffer_handle[i] =
	//		(struct ion_handle *)CCU_IMPORT_BUF_UNDEF;

	mutex_unlock(&g_ccu_device->dev_mutex);


	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long ccu_compat_ioctl(struct file *flip,
	unsigned int cmd, unsigned long arg)
{
	/*<<<<<<<<<< debug 32/64 compat check*/
	struct compat_ccu_power_s __user *ptr_power32;

	compat_uptr_t uptr_Addr32;
	compat_uint_t uint_Data32;

	int err;
	int i;
	/*>>>>>>>>>> debug 32/64 compat check*/

	int ret = 0;
	struct ccu_user_s *user = flip->private_data;

	LOG_DBG("+, cmd: %d\n", cmd);

	switch (cmd) {
	case CCU_IOCTL_SET_POWER:
	{
		mutex_lock(&g_ccu_device->dev_mutex);

		LOG_DBG("CCU_IOCTL_SET_POWER+\n");

		/*<<<<<<<<<< debug 32/64 compat check*/
		LOG_DBG("[IOCTL_DBG] struct ccu_power_s size: %zu\n",
			sizeof(struct ccu_power_s));
		LOG_DBG("[IOCTL_DBG] struct ccu_working_buffer_s size: %zu\n",
			sizeof(struct ccu_working_buffer_s));
		LOG_DBG("[IOCTL_DBG] arg: %p\n", (void *)arg);
		LOG_DBG("[IOCTL_DBG] long size: %zu\n", sizeof(long));
		LOG_DBG("[IOCTL_DBG] long long: %zu\n", sizeof(long long));
		LOG_DBG("[IOCTL_DBG] char *size: %zu\n", sizeof(char *));

		ptr_power32 = compat_ptr(arg);
		/* No compat_alloc_user_space() in K66. */

		LOG_DBG("[IOCTL_DBG] (void *)arg: %p\n", (void *)arg);
		LOG_DBG("[IOCTL_DBG] ptr_power32: %p\n", ptr_power32);
		LOG_DBG("[IOCTL_DBG] power: %p\n", &power);
		LOG_DBG("[IOCTL_DBG] *ptr_power32 size: %zu\n",
			sizeof(*ptr_power32));
		LOG_DBG("[IOCTL_DBG] power64 size: %zu\n",
			sizeof(power));

		err = 0;
		err |= get_user(uint_Data32, &(ptr_power32->bON));
		power.bON = uint_Data32;

		for (i = 0; i < MAX_LOG_BUF_NUM; i++) {
			err |= get_user(uptr_Addr32,
					(&ptr_power32->workBuf.va_log[i]));
			power.workBuf.va_log[i] = (char *)(uint64_t)uptr_Addr32;
			err |= get_user(uint_Data32,
					(&ptr_power32->workBuf.mva_log[i]));
			power.workBuf.mva_log[i] = uint_Data32;
		}

		LOG_DBG("[IOCTL_DBG] power.bON: %d\n",
			power.bON);
		LOG_DBG("[IOCTL_DBG] power.workBuf.va_log[0]: %p\n",
			power.workBuf.va_log[0]);
		LOG_DBG("[IOCTL_DBG] power.workBuf.va_log[1]: %p\n",
			power.workBuf.va_log[1]);
		LOG_DBG("[IOCTL_DBG] power.workBuf.mva_log[0]: %x\n",
			power.workBuf.mva_log[0]);
		LOG_DBG("[IOCTL_DBG] power.workBuf.mva_log[1]: %x\n",
			power.workBuf.mva_log[1]);


		/*>>>>>>>>>> debug 32/64 compat check*/

		if (err)
			LOG_DBG_MUST("[IOCTL_DBG] err: %d\n", err);
		else
			ret = ccu_set_power(&power);

		mutex_unlock(&g_ccu_device->dev_mutex);

		LOG_DBG("CCU_IOCTL_SET_POWER-");
		break;
	}
	default:
		ret = flip->f_op->unlocked_ioctl(flip, cmd, arg);
		break;
	}

	if (ret != 0) {
		LOG_ERR("fail, cmd(%d), pid(%d),\n",
			cmd, user->open_pid);
		LOG_ERR("(process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
	}

	return ret;
}
#endif

static int ccu_alloc_command(struct ccu_cmd_s **rcmd)
{
	struct ccu_cmd_s *cmd;

	cmd = kzalloc(sizeof(vlist_type(struct ccu_cmd_s)), GFP_KERNEL);
	if (cmd == NULL) {
		LOG_ERR("%s(), node=0x%p\n", __func__, cmd);
		return -ENOMEM;
	}

	*rcmd = cmd;

	return 0;
}


static int ccu_free_command(struct ccu_cmd_s *cmd)
{
	kfree(cmd);
	return 0;
}

int ccu_clock_enable(void)
{
	int ret = 0;
	int i = 0;

	LOG_INF_MUST("%s, 6 clks, 2 pwr. %d\n", __func__, _clk_count);

	mutex_lock(&g_ccu_device->clk_mutex);

	++_clk_count;

	if (_clk_count > 1) {
		mutex_unlock(&g_ccu_device->clk_mutex);
		return ret;
	}

	/* TODO: still need smi_dev?*/
	ret = pm_runtime_resume_and_get(g_ccu_device->smi_dev);

	if (ret) {
		LOG_ERR("mtk_smi_larb_get fail.\n");
		--_clk_count;
		mutex_unlock(&g_ccu_device->clk_mutex);
		return ret;
	}

	ret = pm_runtime_get_sync(g_ccu_device->dev);

	if (ret) {
		pm_runtime_put_sync(g_ccu_device->smi_dev);
		LOG_ERR("pm_runtime_get_sync fail.\n");
		--_clk_count;
		mutex_unlock(&g_ccu_device->clk_mutex);
		return ret;
	}

#ifdef CCU_QOS_SUPPORT_ENABLE
	ccu_qos_init(g_ccu_device);
#endif

#ifndef CCU_LDVT
	for (i = 0; (i < clock_num) && (i < CCU_CLK_PWR_NUM); ++i) {
		ret = clk_prepare_enable(ccu_clk_pwr_ctrl[i]);
		if (ret) {
			LOG_ERR("failed to enable CCU clocks #%d %s\n",
				i, clock_name[i].name);
			goto ERROR;
		}
	}

	mutex_unlock(&g_ccu_device->clk_mutex);
	return ret;

ERROR:
#ifdef CCU_QOS_SUPPORT_ENABLE
	ccu_qos_uninit(g_ccu_device);
#endif
	for (--i; i >= 0 ; --i)
		clk_disable_unprepare(ccu_clk_pwr_ctrl[i]);

	pm_runtime_put_sync(g_ccu_device->dev);
	pm_runtime_put_sync(g_ccu_device->smi_dev);

	--_clk_count;
#endif
	mutex_unlock(&g_ccu_device->clk_mutex);
	return ret;
}

void ccu_clock_disable(void)
{
	int i = 0;

	LOG_DBG_MUST("%s. %d\n", __func__, _clk_count);

	mutex_lock(&g_ccu_device->clk_mutex);
#ifndef CCU_LDVT
	if (_clk_count == 1) {
		for (i = 0; (i < clock_num) && (i < CCU_CLK_PWR_NUM); ++i)
			clk_disable_unprepare(ccu_clk_pwr_ctrl[i]);

		pm_runtime_put_sync(g_ccu_device->dev);
		pm_runtime_put_sync(g_ccu_device->smi_dev);
#ifdef CCU_QOS_SUPPORT_ENABLE
		ccu_qos_uninit(g_ccu_device);
#endif
	}
#endif
	--_clk_count;

	mutex_unlock(&g_ccu_device->clk_mutex);
}

static long ccu_ioctl(struct file *flip, unsigned int cmd,
		      unsigned long arg)
{
	int ret = 0;
	int i = 0;
	int powert_stat;
	struct CCU_WAIT_IRQ_STRUCT IrqInfo;
	struct ccu_user_s *user = flip->private_data;
	enum CCU_BIN_TYPE type;
	struct ccu_run_s ccu_run_info;

	if ((cmd != CCU_IOCTL_WAIT_IRQ) && (cmd != CCU_IOCTL_WAIT_AF_IRQ))
		mutex_lock(&g_ccu_device->dev_mutex);

	LOG_DBG("%s+, cmd:%d\n", __func__, cmd);

	if ((cmd != CCU_IOCTL_SET_POWER) && (cmd != CCU_IOCTL_FLUSH_LOG) &&
		(cmd != CCU_IOCTL_WAIT_IRQ) && (cmd != CCU_IOCTL_IMPORT_MEM) &&
		(cmd != CCU_IOCTL_GET_IOVA) && (cmd != CCU_IOCTL_ALLOC_MEM) &&
		(cmd != CCU_IOCTL_DEALLOC_MEM) &&
		(cmd != CCU_IOCTL_LOAD_CCU_BIN)) {
		powert_stat = ccu_query_power_status();
		if (powert_stat == 0) {
			LOG_WARN("ccuk: ioctl without powered on\n");
			if (cmd != CCU_IOCTL_WAIT_AF_IRQ)
				mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
	}

	switch (cmd) {

	case CCU_IOCTL_SET_POWER:
	{
		LOG_DBG("ccuk: ioctl set powerk+\n");
		ret = copy_from_user(
			&power, (void *)arg, sizeof(struct ccu_power_s));
		if (ret != 0) {
			LOG_ERR(
			"[SET_POWER] copy_from_user failed, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
		ret = ccu_set_power(&power);
		LOG_DBG("ccuk: ioctl set powerk-\n");
		break;
	}

	case CCU_IOCTL_SET_RUN_INPUT:
	{
		ret = copy_from_user(&ccu_run_info,
			(void *)arg, sizeof(struct ccu_run_s));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_SET_RUN_INPUT copy_from_user failed: %d\n",
			ret);
			break;
		}

		ret = ccu_run(&ccu_run_info);
		break;
	}

	case CCU_IOCTL_ENQUE_COMMAND:
	{
		struct ccu_cmd_s *cmd = 0;

		/*allocate ccu_cmd_st_list instead of ccu_cmd_st*/
		ret = ccu_alloc_command(&cmd);
		if (ret != 0) {
			LOG_ERR("[ENQUE_COMMAND] ccu_alloc_command, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return ret;
		}
		ret = copy_from_user(
			cmd, (void *)arg, sizeof(struct ccu_cmd_s));
		if (ret != 0) {
			LOG_ERR(
			"[ENQUE_COMMAND] copy_from_user failed, ret=%d\n", ret);
			ret = ccu_free_command(cmd);
			if (ret != 0)
				LOG_ERR("[ENQUE_COMMAND] free command user, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}

		ret = ccu_push_command_to_queue(user, cmd);
		if (ret != 0) {
			LOG_ERR("[ENQUE_COMMAND] push command, ret=%d\n", ret);
			ret = ccu_free_command(cmd);
			if (ret != 0)
				LOG_ERR("[ENQUE_COMMAND] free command, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}

		break;
	}

	case CCU_IOCTL_DEQUE_COMMAND:
	{
		struct ccu_cmd_s *cmd = 0;

		ret = ccu_pop_command_from_queue(user, &cmd);
		if (ret != 0) {
			LOG_ERR(
			"[DEQUE_COMMAND] pop command failed, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
		ret = copy_to_user((void *)arg, cmd, sizeof(struct ccu_cmd_s));
		if (ret != 0) {
			LOG_ERR(
			"[DEQUE_COMMAND] copy_to_user failed, ret=%d\n", ret);
			ret = ccu_free_command(cmd);
			if (ret != 0)
				LOG_ERR("[DEQUE_COMMAND] free command user, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
		ret = ccu_free_command(cmd);
		if (ret != 0) {
			LOG_ERR(
			"[DEQUE_COMMAND] free command, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}

		break;
	}

	case CCU_IOCTL_FLUSH_COMMAND:
	{
		ret = ccu_flush_commands_from_queue(user);
		if (ret != 0) {
			LOG_ERR(
			"[FLUSH_COMMAND] flush command failed, ret=%d\n", ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}

		break;
	}

	case CCU_IOCTL_IPC_SEND_CMD:
	{
		struct ccu_control_info msg;
		uint32_t *indata = NULL;
		uint32_t *outdata = NULL;

		indata = kzalloc(CCU_IPC_IBUF_CAPACITY, GFP_KERNEL);
		if (!indata) {
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -ENOMEM;
		}
		outdata = kzalloc(CCU_IPC_OBUF_CAPACITY, GFP_KERNEL);
		if (!outdata) {
			kfree(indata);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -ENOMEM;
		}
		ret = copy_from_user(&msg,
			(void *)arg, sizeof(struct ccu_control_info));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_IPC_SEND_CMD copy_from_user failed: %d\n",
			ret);
			kfree(indata);
			kfree(outdata);
			break;
		}
		if (msg.inDataSize > CCU_IPC_IBUF_CAPACITY) {
			LOG_ERR(
			"CCU_IOCTL_IPC_SEND_CMD copy_from_user 2 oversize\n");
			ret = -EINVAL;
			kfree(indata);
			kfree(outdata);
			break;
		}
		ret = copy_from_user(indata,
			msg.inDataPtr, msg.inDataSize);
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_IPC_SEND_CMD copy_from_user 2 failed: %d\n",
			ret);
			kfree(indata);
			kfree(outdata);
			break;
		}
		ret = ccuControl(
		msg.feature_type,
		(enum IMGSENSOR_SENSOR_IDX)msg.sensor_idx,
		msg.msg_id, indata, msg.inDataSize, outdata, msg.outDataSize);
		if (msg.outDataSize > CCU_IPC_OBUF_CAPACITY) {
			LOG_ERR(
			"CCU_IOCTL_IPC_SEND_CMD copy_to_user oversize\n");
			ret = -EINVAL;
			kfree(indata);
			kfree(outdata);
			break;
		}
		ret = copy_to_user((void *)msg.outDataPtr, outdata, msg.outDataSize);
		kfree(indata);
		kfree(outdata);
		break;
	}

	case CCU_IOCTL_WAIT_IRQ:
	{
		if (copy_from_user(&IrqInfo,
			(void *)arg, sizeof(struct CCU_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.Type >= CCU_IRQ_TYPE_AMOUNT)
				|| (IrqInfo.Type < 0)) {
				ret = -EFAULT;
				LOG_ERR("invalid type(%d)\n", IrqInfo.Type);
				goto EXIT;
			}

			LOG_DBG(
			"IRQ type(%d), userKey(%d), timeout(%d), sttype(%d), st(%d)\n",
				IrqInfo.Type,
				IrqInfo.EventInfo.UserKey,
				IrqInfo.EventInfo.Timeout,
				IrqInfo.EventInfo.St_type,
				IrqInfo.EventInfo.Status);

			ret = ccu_waitirq(&IrqInfo);

			if (copy_to_user(
				(void *)arg,
				&IrqInfo,
				sizeof(struct CCU_WAIT_IRQ_STRUCT)) != 0) {
				LOG_ERR("copy_to_user failed\n");
				ret = -EFAULT;
			}
		} else {
			LOG_ERR("copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	}

	case CCU_IOCTL_WAIT_AF_IRQ:
	{
		if (copy_from_user(
			&IrqInfo,
			(void *)arg,
			sizeof(struct CCU_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.Type >= CCU_IRQ_TYPE_AMOUNT)
				|| (IrqInfo.Type < 0)) {
				ret = -EFAULT;
				LOG_ERR("invalid type(%d)\n", IrqInfo.Type);
				goto EXIT;
			}

			LOG_DBG(
			"AFIRQ type(%d), userKey(%d), timeout(%d), sttype(%d), st(%d)\n",
				IrqInfo.Type,
				IrqInfo.EventInfo.UserKey,
				IrqInfo.EventInfo.Timeout,
				IrqInfo.EventInfo.St_type,
				IrqInfo.EventInfo.Status);

			if ((IrqInfo.bDumpReg >=
					IMGSENSOR_SENSOR_IDX_MIN_NUM) &&
				(IrqInfo.bDumpReg <
					IMGSENSOR_SENSOR_IDX_MAX_NUM)) {
				ret = ccu_AFwaitirq(
					&IrqInfo, IrqInfo.bDumpReg);
			} else {
				LOG_DBG_MUST(
					"unknown sensorIdx(%d)(CCU_IOCTL_WAIT_AF_IRQ)\n",
					IrqInfo.bDumpReg);
				ret = -EFAULT;
				goto EXIT;
			}

			if (copy_to_user((void *)arg,
				&IrqInfo, sizeof(struct CCU_WAIT_IRQ_STRUCT))
			    != 0) {
				LOG_ERR("copy_to_user failed\n");
				ret = -EFAULT;
			}
		} else {
			LOG_ERR("copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	}
	case CCU_IOCTL_SEND_CMD:
	{	/*--todo: not used for now, remove it*/
		struct ccu_cmd_s cmd;

		ret = copy_from_user(
			&cmd, (void *)arg, sizeof(struct ccu_cmd_s));

		if (ret != 0) {
			LOG_ERR(
			"[CCU_IOCTL_SEND_CMD] copy_from_user failed, ret=%d\n",
			ret);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
		ccu_send_command(&cmd);
		break;
	}

	case CCU_IOCTL_FLUSH_LOG:
	{
		ccu_flushLog(0, NULL);
		break;
	}

	case CCU_IOCTL_UPDATE_QOS_REQUEST:
	{
		uint32_t ccu_bw[3];

		ccu_qos_update_req(g_ccu_device, &ccu_bw[0]);

		ret = copy_to_user((void *)arg, &ccu_bw,
			sizeof(uint32_t) * 3);
		break;
	}

	case CCU_IOCTL_UPDATE_CAM_FREQ_REQUEST:
	{
		uint32_t freq_level;
#ifdef CCU_QOS_SUPPORT_ENABLE
		struct dev_pm_opp *opp;
		int volt;
		unsigned long freq = 0;
#endif

		ret = copy_from_user(&freq_level,
			(void *)arg, sizeof(uint32_t));

		LOG_DBG_MUST("request freq level: %d(%d)\n", freq_level, _step_size);
		if (freq_level >= _step_size) {
			ret = -EINVAL;
			break;
		}

#ifdef CCU_QOS_SUPPORT_ENABLE
		if (freq_level == CCU_REQ_CAM_FREQ_NONE) {
			volt = 0;
		} else {
			freq = _g_freq_steps[freq_level];
			opp = dev_pm_opp_find_freq_ceil(g_ccu_device->dev,
				&freq);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);
		}
		ret = regulator_set_voltage(_ccu_qos_request, volt, INT_MAX);

		//use pm_qos_request to get
		//current freq setting
		LOG_DBG_MUST("current freq: %d\n", volt);
#endif
		break;
	}

	case CCU_IOCTL_GET_CURRENT_FPS:
	{
		int32_t current_fps_list[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {0};

		ccu_get_current_fps(current_fps_list);

		ret = copy_to_user((void *)arg, &current_fps_list,
			sizeof(int32_t) * IMGSENSOR_SENSOR_IDX_MAX_NUM);

		break;
	}

	case CCU_IOCTL_GET_SENSOR_NAME:
	{
		#define SENSOR_NAME_MAX_LEN 32

		char *sensor_names[5];

		ccu_get_sensor_name(sensor_names);

		if (sensor_names[0] != NULL) {
			ret = copy_to_user((char *)arg,
				sensor_names[0], strlen(sensor_names[0])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 1 failed: %d\n", ret);
				break;
			}
		}

		if (sensor_names[1] != NULL) {
			ret = copy_to_user(((char *)arg+SENSOR_NAME_MAX_LEN),
				sensor_names[1], strlen(sensor_names[1])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 2 failed: %d\n", ret);
				break;
			}
		}

		if (sensor_names[2] != NULL) {
			ret = copy_to_user(((char *)arg+SENSOR_NAME_MAX_LEN*2),
				sensor_names[2], strlen(sensor_names[2])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 3 failed: %d\n", ret);
				break;
			}
		}

		if (sensor_names[3] != NULL) {
			ret = copy_to_user(((char *)arg+SENSOR_NAME_MAX_LEN*3),
				sensor_names[3], strlen(sensor_names[3])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 4 failed: %d\n", ret);
				break;
			}
		}

		if (sensor_names[4] != NULL) {
			ret = copy_to_user(((char *)arg+SENSOR_NAME_MAX_LEN*4),
				sensor_names[4], strlen(sensor_names[4])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 4 failed: %d\n", ret);
				break;
			}
		}
		#undef SENSOR_NAME_MAX_LEN
		break;
	}

	case CCU_READ_STRUCT_SIZE:
	{
		uint32_t structCnt;
		uint32_t *structSizes;

		ret = copy_from_user(&structCnt,
			(void *)arg, sizeof(uint32_t));
		if (ret != 0) {
			LOG_ERR(
			"CCU_READ_STRUCT_SIZE copy_from_user failed: %d\n",
			ret);
			break;
		}
		structSizes = kzalloc(sizeof(uint32_t)*structCnt, GFP_KERNEL);
		if (!structSizes) {
			LOG_ERR(
			"CCU_READ_STRUCT_SIZE alloc failed\n");
			break;
		}
		ccu_read_struct_size(structSizes, structCnt);
		ret = copy_to_user((char *)arg,
			structSizes, sizeof(uint32_t)*structCnt);
		if (ret != 0) {
			LOG_ERR(
			"CCU_READ_STRUCT_SIZE copy_to_user failed: %d\n", ret);
		}
		kfree(structSizes);
		break;
	}

	case CCU_IOCTL_PRINT_REG:
	{
		uint32_t *Reg;

		Reg = kzalloc(sizeof(uint8_t)*
			(CCU_HW_DUMP_SIZE+CCU_DMEM_SIZE+CCU_PMEM_SIZE),
			GFP_KERNEL);
		if (!Reg) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_REG alloc failed\n");
			break;
		}
		ccu_print_reg(Reg);
		ret = copy_to_user((char *)arg,
			Reg, sizeof(uint8_t)*
			(CCU_HW_DUMP_SIZE+CCU_DMEM_SIZE+CCU_PMEM_SIZE));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_REG copy_to_user failed: %d\n", ret);
		}
		kfree(Reg);
		break;
	}

	case CCU_IOCTL_PRINT_SRAM_LOG:
	{
		char *sram_log;

		sram_log = kzalloc(sizeof(char)*
			(CCU_LOG_SIZE*2+CCU_ISR_LOG_SIZE),
			GFP_KERNEL);
		if (!sram_log) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_SRAM_LOG alloc failed\n");
			break;
		}
		ccu_print_sram_log(sram_log);
		ret = copy_to_user((char *)arg,
			sram_log, sizeof(char)*
			(CCU_LOG_SIZE*2+CCU_ISR_LOG_SIZE));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_SRAM_LOG copy_to_user failed: %d\n", ret);
		}
		kfree(sram_log);
		break;
	}

	case CCU_IOCTL_IMPORT_MEM:
	{
		// struct ion_handle *handle;
		struct import_mem_s import_mem;

		ret = copy_from_user(&import_mem,
			(void *)arg, sizeof(struct import_mem_s));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_IMPORT_MEM copy_to_user failed: %d\n",
			ret);
			break;
		}

		for (i = 0; i < CCU_IMPORT_BUF_NUM; i++) {
			if (import_mem.memID[i] == CCU_IMPORT_BUF_UNDEF) {
				LOG_INF_MUST("imported buffer count: %d\n", i);
				break;
			}

			// handle = ccu_ion_import_handle(
			//	import_mem.memID[i]);

			// if (IS_ERR(handle)) {
			//	ret = -EFAULT;
			//	LOG_ERR(
			//	"CCU ccu_ion_import_handle failed: %d\n", ret);
			//	break;
			// }

			// import_buffer_handle[i] = handle;
		}

		break;
	}

	case CCU_IOCTL_GET_IOVA:
	{
		int fd = 0;
		int ret = 0;
		dma_addr_t dma_addr;
		struct dma_buf *buf;

		if (iova_buf_count >= CCU_IOVA_BUFFER_MAX) {
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}

		ret = copy_from_user(&fd,
			(void *)arg, sizeof(int));
		LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] copy_from_user fd=%d\n", fd);

		buf = dma_buf_get(fd);
		if (IS_ERR(buf)) {
			LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] dma_buf_get failed, ret=%llx fd=%d\n", (uint64_t)buf, fd);
			mutex_unlock(&g_ccu_device->dev_mutex);
			return false;
		}
		LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] dma_buf_get va=%llx\n", (uint64_t)buf);
		ccu_iova[iova_buf_count].dma_buf = buf;
		ccu_iova[iova_buf_count].attach =
			dma_buf_attach(ccu_iova[iova_buf_count].dma_buf, g_ccu_device->dev1);
		if (IS_ERR(ccu_iova[iova_buf_count].attach)) {
			LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] dma_buf_attach failed, attach=%llx va=%llx\n",
			 (uint64_t)ccu_iova[iova_buf_count].attach, (uint64_t)ccu_iova[iova_buf_count].dma_buf);
			goto err_attach;
		}
		LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] dma_buf_attach va=%llx\n",
			(uint64_t)ccu_iova[iova_buf_count].attach);
		ccu_iova[iova_buf_count].sgt =
			dma_buf_map_attachment_unlocked(ccu_iova[iova_buf_count].attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(ccu_iova[iova_buf_count].sgt)) {
			LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] dma_buf_map_attachment failed, sgt=%llx va=%d\n",
			 (uint64_t)ccu_iova[iova_buf_count].sgt, DMA_BIDIRECTIONAL);
			goto err_map;
		}
		LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] dma_buf_map_attachment va=%llx\n",
			(uint64_t)ccu_iova[iova_buf_count].sgt);

		/* return true;*/


		dma_addr = sg_dma_address(ccu_iova[iova_buf_count].sgt->sgl);
		iova_buf_count++;
		LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] sg_dma_address, ret=%llu\n", dma_addr);

		ret = copy_to_user((void *)arg, &dma_addr, sizeof(dma_addr_t));
		if (ret != 0) {
			LOG_ERR(
			"[CCU_IOCTL_GET_IOVA] copy_to_user failed, ret=%d\n", ret);
			goto err_copy_out;
		}

		break;

err_copy_out:
		dma_buf_unmap_attachment(ccu_iova[iova_buf_count].attach,
			ccu_iova[iova_buf_count].sgt, DMA_BIDIRECTIONAL);

err_map:
		dma_buf_detach(ccu_iova[iova_buf_count].dma_buf, ccu_iova[iova_buf_count].attach);

err_attach:
		dma_buf_put(ccu_iova[iova_buf_count].dma_buf);
		memset(&(ccu_iova[iova_buf_count]), 0,
			sizeof(struct ccu_iova_t));
		mutex_unlock(&g_ccu_device->dev_mutex);
		return -EFAULT;
	}

	case CCU_IOCTL_LOAD_CCU_BIN:
	{
		ret = copy_from_user(&type,
			(void *)arg, sizeof(enum CCU_BIN_TYPE));
		LOG_INF_MUST("load ccu bin %d\n", type);
		powert_stat = ccu_query_power_status();
		if (type == 0 && powert_stat == 0) {
			LOG_WARN("ccuk: ioctl without powered on\n");
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
		ret = ccu_load_bin(g_ccu_device, type);

		break;
	}

	case CCU_IOCTL_ALLOC_MEM:
	{
		struct CcuMemHandle handle;
		uint32_t cs;

		ret = copy_from_user(&(handle.meminfo),
			(void *)arg, sizeof(struct CcuMemInfo));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_ALLOC_MEM copy_to_user failed: %d\n",
			ret);
			break;
		}
		cs = get_checksum((uint8_t *)&(handle.meminfo.shareFd),
			sizeof(struct CcuMemInfo) - sizeof(unsigned int));
		if (cs != handle.meminfo.chksum) {
			LOG_ERR(
			"CCU_IOCTL_ALLOC_MEM checksum failed: %x\n",
			cs);
			ret = -EINVAL;
			break;
		}

		ret = ccu_allocate_mem(g_ccu_device, &handle, handle.meminfo.size,
			handle.meminfo.cached);
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_ALLOC_MEM ccu_allocate_mem failed: %d\n",
			ret);
			break;
		}
		LOG_INF("CCU_IOCTL_ALLOC_MEM size(%d) cached(%d) OK\n",
			handle.meminfo.size, handle.meminfo.cached);
		break;
	}

	case CCU_IOCTL_DEALLOC_MEM:
	{
		struct CcuMemHandle handle;
		uint32_t cs;

		ret = copy_from_user(&(handle.meminfo),
			(void *)arg, sizeof(struct CcuMemInfo));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_ALLOC_MEM copy_to_user failed: %d\n",
			ret);
			break;
		}

		cs = get_checksum((uint8_t *)&(handle.meminfo.shareFd),
				sizeof(struct CcuMemInfo) - sizeof(unsigned int));
		if (cs != handle.meminfo.chksum) {
			ret = -EINVAL;
			break;
		}

		ret = ccu_deallocate_mem(g_ccu_device, &handle);
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_DEALLOC_MEM ccu_deallocate_mem failed: %d\n",
			ret);
			break;
		}
		ret = copy_to_user((void *)arg, &handle.meminfo, sizeof(struct CcuMemInfo));
		break;
	}

	default:
		LOG_WARN("ioctl:No such command!\n");
		ret = -EINVAL;
		break;
	}


EXIT:
	if (ret != 0) {
		LOG_ERR(
		"fail, cmd(%d), cmd_nr(%d), pid(%d),",
			cmd, _IOC_NR(cmd), user->open_pid);
		LOG_ERR(
		"fail, (process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
	}

	if ((cmd != CCU_IOCTL_WAIT_IRQ) && (cmd != CCU_IOCTL_WAIT_AF_IRQ))
		mutex_unlock(&g_ccu_device->dev_mutex);

	return ret;
}

static int ccu_release(struct inode *inode, struct file *flip)
{
	struct ccu_user_s *user = flip->private_data;
	struct CcuMemHandle handle = {0};
	int i = 0;

	mutex_lock(&g_ccu_device->dev_mutex);

	LOG_INF_MUST("%s pid:%d tid:%d cnt:%d+\n",
		__func__, user->open_pid, user->open_tgid, _user_count);

	ccu_delete_user(user);
	_user_count--;

	if (_user_count > 0) {
		LOG_INF_MUST("%s bypass release flow-", __func__);
		mutex_unlock(&g_ccu_device->dev_mutex);
		return 0;
	}

	ccu_force_powerdown();
	for (i = 0; i < CCU_IOVA_BUFFER_MAX; i++) {
		if (ccu_iova[i].dma_buf) {
			dma_buf_unmap_attachment_unlocked(ccu_iova[i].attach, ccu_iova[i].sgt,
							DMA_BIDIRECTIONAL);
			dma_buf_detach(ccu_iova[i].dma_buf, ccu_iova[i].attach);
			dma_buf_put(ccu_iova[i].dma_buf);
			memset(&(ccu_iova[i]), 0, sizeof(struct ccu_iova_t));
		}
	}

	handle.meminfo.cached = 0;
	ccu_deallocate_mem(g_ccu_device, &handle);
	handle.meminfo.cached = 1;
	ccu_deallocate_mem(g_ccu_device, &handle);

	iova_buf_count = 0;
	// for (i = 0; i < CCU_IMPORT_BUF_NUM; i++) {
	//	if (import_buffer_handle[i] ==
	//		(struct ion_handle *)CCU_IMPORT_BUF_UNDEF) {
	//		LOG_INF_MUST("freed buffer count: %d\n", i);
	//		break;
	//	}

	//	/*can't in spin_lock*/
	//	ccu_ion_free_import_handle(import_buffer_handle[i]);
	// }

	// ccu_ion_uninit();

	LOG_INF_MUST("%s -\n", __func__);

	mutex_unlock(&g_ccu_device->dev_mutex);

	return 0;
}


/******************************************************************************
 *
 *****************************************************************************/
static dev_t ccu_devt;
static struct cdev *ccu_chardev;
static struct class *ccu_class;
static int ccu_num_devs;

static inline void ccu_unreg_chardev(void)
{
	/* Release char driver */
	if (ccu_chardev != NULL) {
		cdev_del(ccu_chardev);
		ccu_chardev = NULL;
	}
	unregister_chrdev_region(ccu_devt, 1);
}

static inline int ccu_reg_chardev(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&ccu_devt, 0, 1, CCU_DEV_NAME);
	if ((ret) < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}
	/* Allocate driver */
	ccu_chardev = cdev_alloc();
	if (ccu_chardev == NULL) {
		LOG_ERR("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto EXIT;
	}

	/* Attatch file operation. */
	cdev_init(ccu_chardev, &ccu_fops);

	ccu_chardev->owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(ccu_chardev, ccu_devt, 1);
	if ((ret) < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", ret);
		goto EXIT;
	}

EXIT:
	if (ret < 0)
		ccu_unreg_chardev();

	return ret;
}

/*****************************************************************************
 * platform_driver
 ****************************************************************************/

static int ccu_read_platform_info_from_dt(struct device_node
		*node)
{
	uint32_t reg[4] = {0, 0, 0, 0};
	int ret = 0;

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0)
		LOG_ERR("of_property_read_u32_array ERR : %d\n", ret);

	ccu_hw_base = reg[1];

	ret = of_property_read_u32(node, "ccu_version", &ccu_version);
	if (ret < 0)
		ccu_version = CCU_VER_MT6833;

	LOG_DBG("ccu mt%u read dt property ccu_hw_base = %x\n",
		ccu_version, ccu_hw_base);

	return ret;
}

static int ccu_probe(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_OF)

	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct device_node *smi_node;
	struct device_link *link;
	struct platform_device *smi_pdev;
	int ret = 0;
	uint32_t phy_addr;
	uint32_t phy_size;
	uint32_t clki;
	struct device_node *node1;
	phandle ccu1_phandle;
	struct platform_device *pdev1;

	node = pdev->dev.of_node;
	g_ccu_device->dev = &pdev->dev;
	LOG_DBG("probe 0, pdev id = %d name = %s\n", pdev->id,
		pdev->name);

	ccu_read_platform_info_from_dt(node);

#ifdef MTK_CCU_EMULATOR
	/* emulator will fill ccu_base and bin_base */
	/*ccu_init_emulator(g_ccu_device);*/
#else
	/* get register address */
	if ((strcmp("ccu", g_ccu_device->dev->of_node->name) == 0)) {

		/* get physical address of pmem  */
		/* ioremap_wc() has no access 4 bytes alignmen
		 * limitation as of_iomap() does?
		 * https://forums.xilinx.com/xlnx/attachments/
		 * xlnx/ELINUX/11158/1/Linux%20CPU%20to%20PL%20Access.pdf
		 */

		/*remap ccu_base*/
		phy_addr = ccu_hw_base;
		phy_size = 0x1000;
#ifdef CCU_LDVT
		g_ccu_device->ccu_base =
			devm_ioremap_wc(dev, phy_addr, phy_size);
#else
		g_ccu_device->ccu_base =
			devm_ioremap(dev, phy_addr, phy_size);
#endif
		LOG_INF("ccu_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_INF("ccu_base va: 0x%llx\n", (uint64_t)g_ccu_device->ccu_base);

		/*remap dmem_base*/
		phy_addr = CCU_DMEM_BASE;
		phy_size = CCU_DMEM_SIZE;
#ifdef CCU_LDVT
		g_ccu_device->dmem_base =
			devm_ioremap_wc(dev, phy_addr, phy_size);
#else
		g_ccu_device->dmem_base =
			devm_ioremap(dev, phy_addr, phy_size);
#endif
		LOG_INF("dmem_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_INF("dmem_base va: 0x%llx\n", (uint64_t)g_ccu_device->dmem_base);

		phy_addr = CCU_PMEM_BASE;
		phy_size = CCU_PMEM_SIZE;
#ifdef CCU_LDVT
		g_ccu_device->pmem_base =
			devm_ioremap_wc(dev, phy_addr, phy_size);
#else
		g_ccu_device->pmem_base =
			devm_ioremap(dev, phy_addr, phy_size);
#endif
		LOG_DBG_MUST("pmem_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_DBG_MUST("pmem_base va: 0x%llx\n", (uint64_t)g_ccu_device->pmem_base);

		/*remap camsys_base*/
		phy_addr = CCU_CAMSYS_BASE;
		phy_size = CCU_CAMSYS_SIZE;
#ifdef CCU_LDVT
		g_ccu_device->camsys_base =
			devm_ioremap_wc(dev, phy_addr, phy_size);
#else
		g_ccu_device->camsys_base =
			devm_ioremap(dev, phy_addr, phy_size);
#endif
		LOG_INF("camsys_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_INF("camsys_base va: 0x%llx\n", (uint64_t)g_ccu_device->camsys_base);

		/* get Clock control from device tree.  */

		// check platform version
		if (ccu_version == CCU_VER_MT6893)
			memcpy(clock_name, MT6893_ccu_clock_name, sizeof(MT6893_ccu_clock_name));
		else if (ccu_version == CCU_VER_MT6833)
			memcpy(clock_name, MT6833_ccu_clock_name, sizeof(MT6833_ccu_clock_name));
		else if (ccu_version == CCU_VER_MT6853)
			memcpy(clock_name, MT6853_ccu_clock_name, sizeof(MT6853_ccu_clock_name));
		else
			memcpy(clock_name, MT6877_ccu_clock_name, sizeof(MT6877_ccu_clock_name));

		for (clki = 0; clki < CCU_CLK_PWR_NUM; ++clki) {
			if (!clock_name[clki].enable)
				break;
			ccu_clk_pwr_ctrl[clki] = devm_clk_get(g_ccu_device->dev,
				clock_name[clki].name);
			if (IS_ERR(ccu_clk_pwr_ctrl[clki]))
				LOG_ERR("Get %s fail.\n", clock_name[clki].name);
		}

		if (clki >= CCU_CLK_PWR_NUM) {
			LOG_ERR("ccu_clk_pwr_ctrl[] too small(%d).\n", clki);
			return -EINVAL;
		}

		clock_num = clki;
		LOG_DBG("CCU got %d clocks\n", clki);

		pm_runtime_enable(g_ccu_device->dev);

		smi_node = of_parse_phandle(node, "mediatek,larbs", 0);
		if (!smi_node) {
			LOG_DERR(g_ccu_device->dev, "get smi larb from DTS fail!\n");
			return -ENODEV;
		}
		smi_pdev = of_find_device_by_node(smi_node);
		if (WARN_ON(!smi_pdev)) {
			of_node_put(smi_node);
			return -ENODEV;
		}
		of_node_put(smi_node);

		link = device_link_add(&pdev->dev, &smi_pdev->dev,
				DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			LOG_ERR("Unable to link smi larb\n");

		g_ccu_device->smi_dev = &smi_pdev->dev;

#ifdef CCU_QOS_SUPPORT_ENABLE
		g_ccu_device->path_ccuo = of_mtk_icc_get(g_ccu_device->dev, "ccu_o");
		g_ccu_device->path_ccui = of_mtk_icc_get(g_ccu_device->dev, "ccu_i");
		g_ccu_device->path_ccug = of_mtk_icc_get(g_ccu_device->dev, "ccu_g");
#endif

		ret = of_property_read_u32(node, "mediatek,ccu1",
			&ccu1_phandle);
		node1 = of_find_node_by_phandle(ccu1_phandle);
		if (!node1) {
			dev_err(dev, "failed to get ccu1 node handle\n");
			return -ENODEV;
		}

		pdev1 = of_find_device_by_node(node1);
		if (WARN_ON(!pdev1)) {
			dev_err(dev, "failed to get ccu1 pdev\n");
			of_node_put(node1);
			return -ENODEV;
		}

		g_ccu_device->dev1 = &pdev1->dev;
		LOG_INF("ccu1@0x%llx\n", (uint64_t)g_ccu_device->dev1);

		g_ccu_device->irq_num = irq_of_parse_and_map(node, 0);
		LOG_INF_MUST("probe 1, ccu_base: 0x%llx, bin_base: 0x%llx,",
		(uint64_t)g_ccu_device->ccu_base, (uint64_t)g_ccu_device->bin_base);
		LOG_INF_MUST("probe 1, ccu_base:irq_num: %d, pdev: 0x%llx\n",
		g_ccu_device->irq_num, (uint64_t)g_ccu_device->dev);

		if (g_ccu_device->irq_num > 0) {
			/* get IRQ flag from device node */
			unsigned int irq_info[3];

			if (of_property_read_u32_array
			    (node, "interrupts", irq_info, ARRAY_SIZE(irq_info))) {
				LOG_DERR(g_ccu_device->dev, "get irq flags from DTS fail!\n");
				return -ENODEV;
			}
		} else {
			LOG_DERR(g_ccu_device->dev,
				"No IRQ!!: ccu_num_devs=%d, devnode(%s), irq=%d\n",
				ccu_num_devs, g_ccu_device->dev->of_node->name,
				g_ccu_device->irq_num);
		}

		/* Only register char driver in the 1st time */
		if (++ccu_num_devs == 1) {

			/* Register char driver */
			ret = ccu_reg_chardev();
			if (ret) {
				LOG_DERR(g_ccu_device->dev, "register char failed");
				return ret;
			}

			/* Create class register */
			ccu_class = class_create("ccudrv");
			if (IS_ERR(ccu_class)) {
				ret = PTR_ERR(ccu_class);
				LOG_ERR("Unable to create class, err = %d\n", ret);
				goto EXIT;
			}

			dev = device_create(ccu_class, NULL, ccu_devt, NULL, CCU_DEV_NAME);
			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				LOG_DERR(g_ccu_device->dev,
					"Failed to create device: /dev/%s, err = %d",
					CCU_DEV_NAME, ret);
				goto EXIT;
			}

// #if IS_ENABLED(CONFIG_PM_WAKELOCKS)
//			wakeup_source_init(&ccu_wake_lock, "ccu_lock_wakelock");
// #else
//			wake_lock_init(&ccu_wake_lock, WAKE_LOCK_SUSPEND,
//					"ccu_lock_wakelock");
// #endif

			/* enqueue/dequeue control in ihalpipe wrapper */
			init_waitqueue_head(&wait_queue_deque);
			init_waitqueue_head(&wait_queue_enque);

			/*for (i = 0; i < CCU_IRQ_NUM_TYPES; i++) {*/
			/*	tasklet_init(ccu_tasklet[i].pCCU_tkt, */
			/* ccu_tasklet[i].tkt_cb, 0);*/
			/*}*/

			// /*register i2c driver callback*/
			// ret = ccu_i2c_register_driver();
			// if (ret < 0)
			//	goto EXIT;
			// ret = ccu_i2c_set_n3d_base(g_ccu_device->n3d_a_base);
			// if (ret < 0)
			//	goto EXIT;

			// /*allocate dma buffer for i2c*/
			if (dma_set_mask_and_coherent(g_ccu_device->dev, DMA_BIT_MASK(36))) {
				LOG_DERR(g_ccu_device->dev,
					"dma_set_mask_and_coherent return error.");
				goto EXIT;
			}
			// g_ccu_device->i2c_dma_vaddr =
			// dma_alloc_coherent(g_ccu_device->dev, CCU_I2C_DMA_BUF_SIZE,
			//		&g_ccu_device->i2c_dma_paddr, GFP_KERNEL);
			// if (g_ccu_device->i2c_dma_vaddr == NULL) {
			//		LOG_DERR(g_ccu_device->dev,
			//			"dma_alloc_coherent fail\n");
			//		ret = -ENOMEM;
			//		goto EXIT;
			// }

			// g_ccu_device->i2c_dma_mva = 0;

EXIT:
			if (ret < 0)
				ccu_unreg_chardev();
		}

		ccu_init_hw(g_ccu_device);

		LOG_INF_MUST("ccu probe succeed...\n");

	}
#endif /* !MTK_CCU_EMULATOR */
#endif /* CONFIG_OF */

	LOG_DBG("- X. CCU driver probe.\n");

	return ret;
}


static int ccu_remove(struct platform_device *pDev)
{
	/*    struct resource *pRes; */

	/*  */
	LOG_DBG("- E.");

	// /*free i2c dma buffer*/
	// dma_free_coherent(g_ccu_device->dev, CCU_I2C_DMA_BUF_SIZE,
	//		g_ccu_device->i2c_dma_vaddr, g_ccu_device->i2c_dma_paddr);

	/*uninit hw*/
	ccu_uninit_hw(g_ccu_device);

	/* unregister char driver. */
	ccu_unreg_chardev();

	/*ccu_i2c_del_drivers();*/
	// ccu_i2c_delete_driver();

	/* Release IRQ */
	disable_irq(g_ccu_device->irq_num);
	devm_free_irq(g_ccu_device->dev, g_ccu_device->irq_num, (void *)g_ccu_device);

	/* kill tasklet */
	/*for (i = 0; i < CCU_IRQ_NUM_TYPES; i++) {*/
	/*      tasklet_kill(ccu_tasklet[i].p_ccu_tkt);*/
	/*}*/
	pm_runtime_disable(g_ccu_device->dev);
	/*  */
	device_destroy(ccu_class, ccu_devt);
	/*  */
	class_destroy(ccu_class);
	ccu_class = NULL;
	/*  */
	return 0;
}

static int ccu_suspend(struct platform_device *pdev,
		       pm_message_t mesg)
{
	return 0;
}

static int ccu_resume(struct platform_device *pdev)
{
	return 0;
}

static int ccu1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));

	pm_runtime_enable(dev);

	return 0;
}


static int ccu1_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);

	return 0;
}

static int ccu1_suspend(struct platform_device *pdev,
		       pm_message_t mesg)
{
	return 0;
}

static int ccu1_resume(struct platform_device *pdev)
{
	return 0;
}
/******************************************************************************
 *
 *****************************************************************************/
static int __init CCU_INIT(void)
{
#ifdef CCU_QOS_SUPPORT_ENABLE
	int fi = 0;
	unsigned long freq = 0;
	struct dev_pm_opp *opp = NULL;
#endif
	/*struct device_node *node = NULL;*/

	g_ccu_device = kzalloc(sizeof(struct ccu_device_s), GFP_KERNEL);
	if (!g_ccu_device) {
		LOG_ERR("failed to allocate CCU device memory");
		return -ENOMEM;
	}
	/*g_ccu_device = dma_cache_coherent();*/

	INIT_LIST_HEAD(&g_ccu_device->user_list);
	mutex_init(&g_ccu_device->dev_mutex);
	mutex_init(&g_ccu_device->user_mutex);
	mutex_init(&g_ccu_device->clk_mutex);
	mutex_init(&g_ccu_device->ion_client_mutex);
	init_waitqueue_head(&g_ccu_device->cmd_wait);

	LOG_DBG("platform_driver_register start\n");
	if (platform_driver_register(&ccu_driver)) {
		LOG_ERR("failed to register CCU driver");
		return -ENODEV;
	}

	if (platform_driver_register(&ccu1_driver)) {
		LOG_ERR("failed to register CCU1 driver");
		return -ENODEV;
	}

#ifdef CCU_QOS_SUPPORT_ENABLE
	/*Call pm_qos_add_request when initialize module or driver prob*/
	dev_pm_opp_of_add_table(g_ccu_device->dev);
	_ccu_qos_request = devm_regulator_get(g_ccu_device->dev,
		"dvfsrc-vcore");

	/*Call mmdvfs_qos_get_freq_steps to get supported frequency*/
	_step_size = dev_pm_opp_get_opp_count(g_ccu_device->dev);
	_g_freq_steps = kcalloc(_step_size, sizeof(u64), GFP_KERNEL);

	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(g_ccu_device->dev,
		&freq))) {
		_g_freq_steps[fi] = freq;
		freq++;
		fi++;
		dev_pm_opp_put(opp);
	}

	if (fi == 0)
		LOG_ERR("get MMDVFS freq steps failed\n");
#endif

	return 0;
}


static void __exit CCU_EXIT(void)
{
#ifdef CCU_QOS_SUPPORT_ENABLE
	//Call pm_qos_remove_request when
	//de-initialize module or driver remove
	kfree(_g_freq_steps);
#endif
	platform_driver_unregister(&ccu_driver);
	platform_driver_unregister(&ccu1_driver);
	kfree(g_ccu_device);
}


/******************************************************************************
 *
 *****************************************************************************/
module_init(CCU_INIT);
module_exit(CCU_EXIT);
MODULE_DESCRIPTION("MTK CCU Driver");
MODULE_AUTHOR("SW1");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
