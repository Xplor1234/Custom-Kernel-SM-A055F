/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "btmtk_main.h"
#include "btmtk_chip_if.h"
#include "btmtk_fw_log.h"
#include "btmtk_queue.h"
#if 0 // Simfex
#include "btmtk_dbg_tp_evt_if.h"
#endif

MODULE_LICENSE("Dual BSD/GPL");

#if (USE_DEVICE_NODE == 1)
#include "btmtk_proj_sp.h"

/*
 *******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define BT_DRIVER_NAME      "mtk_bt_chrdev"
#define BT_DRIVER_NODE_NAME "stpbt"

/*
 *******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define BT_BUFFER_SIZE				(5120)
#define FTRACE_STR_LOG_SIZE			(256)
#define TRIGGER_HW_ERR_EVT_COUNT	(1000)
/*
 *******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum chip_reset_state {
	CHIP_RESET_NONE,
	CHIP_RESET_START,
	CHIP_RESET_END,
	CHIP_RESET_NOTIFIED
};

/*
 *******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*
 *******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static int32_t BT_devs = 1;
static int32_t BT_major = 192;
module_param(BT_major, uint, 0444);
static struct cdev BT_cdev;
static struct class *BT_class;
static struct device *BT_dev;
static uint32_t BT_open_count = 0;

static uint8_t i_buf[BT_BUFFER_SIZE]; /* Input buffer for read */
static uint8_t o_buf[BT_BUFFER_SIZE]; /* Output buffer for write */
static uint8_t ioc_buf[IOCTL_BT_HOST_INTTRX_SIZE];

extern struct btmtk_dev *g_sbdev;
extern void bthost_debug_init(void);
extern void bthost_debug_save(uint32_t id, uint32_t value, char *desc);
static struct semaphore wr_mtx, rd_mtx;
static struct wakeup_source *bt_wakelock;
/* Wait queue for poll and read */
static wait_queue_head_t inq;
static DECLARE_WAIT_QUEUE_HEAD(BT_wq);
static int32_t flag;
static int32_t bt_ftrace_flag;
/*
 * Reset flag for whole chip reset scenario, to indicate reset status:
 *   0 - normal, no whole chip reset occurs
 *   1 - reset start
 *   2 - reset end, have not sent Hardware Error event yet
 *   3 - reset end, already sent Hardware Error event
 */
static uint32_t rstflag = CHIP_RESET_NONE;
static uint8_t HCI_EVT_HW_ERROR[] = {0x04, 0x10, 0x01, 0x00};
static loff_t rd_offset;
static uint32_t hw_err_retry;

/*
 *******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
extern int main_driver_init(void);
extern void main_driver_exit(void);

static int32_t ftrace_print(const uint8_t *str, ...)
{
#if defined (BT_CONFIG_TRACING) && defined (CONFIG_TRACING)
	va_list args;
	uint8_t temp_string[FTRACE_STR_LOG_SIZE];

	if (bt_ftrace_flag) {
		va_start(args, str);
		if (vsnprintf(temp_string, FTRACE_STR_LOG_SIZE, str, args) < 0)
			BTMTK_INFO("%s: vsnprintf error", __func__);
		va_end(args);
		trace_printk("%s\n", temp_string);
	}
#endif
	return 0;
}

static size_t bt_report_hw_error(uint8_t *buf, size_t count, loff_t *f_pos)
{
	size_t bytes_rest = 0, bytes_read = 0;

	BTMTK_DBG("%s start", __func__);
	if (*f_pos == 0)
		BTMTK_INFO("Send Hardware Error event to stack to restart Bluetooth");

	bytes_rest = sizeof(HCI_EVT_HW_ERROR) - *f_pos;
	bytes_read = count < bytes_rest ? count : bytes_rest;
	memcpy(buf, HCI_EVT_HW_ERROR + *f_pos, bytes_read);
	*f_pos += bytes_read;

	return bytes_read;
}

#if 0 // Simfex
static void bt_state_cb(u_int8_t state)
{
	switch (state) {

	case FUNC_ON:
		rstflag = CHIP_RESET_NONE;
		break;
	case RESET_START:
		rstflag = CHIP_RESET_START;
		break;
	case FUNC_OFF:
		if (rstflag != CHIP_RESET_START) {
			rstflag = CHIP_RESET_NONE;
			break;
		}
	case RESET_END:
		rstflag = CHIP_RESET_END;
		rd_offset = 0;
		flag = 1;
		wake_up_interruptible(&inq);
		wake_up(&BT_wq);
		break;
	default:
		break;
	}
}
#endif

static void BT_event_cb(void)
{
	ftrace_print("%s get called", __func__);

	/*
	 * Hold wakelock for 100ms to avoid system enter suspend in such case:
	 *   FW has sent data to host, STP driver received the data and put it
	 *   into BT rx queue, then send sleep command and release wakelock as
	 *   quick sleep mechanism for low power, BT driver will wake up stack
	 *   hci thread stuck in poll or read.
	 *   But before hci thread comes to read data, system enter suspend,
	 *   hci command timeout timer keeps counting during suspend period till
	 *   expires, then the RTC interrupt wakes up system, command timeout
	 *   handler is executed and meanwhile the event is received.
	 *   This will false trigger FW assert and should never happen.
	 */
	__pm_wakeup_event(bt_wakelock, 100);

	/*
	 * Finally, wake up any reader blocked in poll or read
	 */
	flag = 1;
	wake_up_interruptible(&inq);
	wake_up(&BT_wq);
	ftrace_print("%s wake_up triggered", __func__);
}

static unsigned int BT_poll(struct file *filp, poll_table *wait)
{
	uint32_t mask = 0;

	//BTMTK_DBG("%s : rstflag[%d]", __func__, rstflag);
	//bt_dbg_tp_evt(TP_ACT_POLL, 0, 0, NULL);
	if ((!btmtk_rx_data_valid() && rstflag == CHIP_RESET_NONE) ||
	    (rstflag == CHIP_RESET_START) || (rstflag == CHIP_RESET_NOTIFIED)) {
		/*
		 * BT RX queue is empty, or whole chip reset start, or already sent Hardware Error event
		 * for whole chip reset end, add to wait queue.
		 */
		poll_wait(filp, &inq, wait);
		/*
		 * Check if condition changes before poll_wait return, in case of
		 * wake_up_interruptible is called before add_wait_queue, otherwise,
		 * do_poll will get into sleep and never be waken up until timeout.
		 */
		if (!((!btmtk_rx_data_valid() && rstflag == CHIP_RESET_NONE) ||
		      (rstflag == CHIP_RESET_START) || (rstflag == CHIP_RESET_NOTIFIED)))
			mask |= POLLIN | POLLRDNORM;	/* Readable */
	} else {
		/* BT RX queue has valid data, or whole chip reset end, have not sent Hardware Error event yet */
		mask |= POLLIN | POLLRDNORM;	/* Readable */
	}

	/* Do we need condition here? */
	mask |= POLLOUT | POLLWRNORM;	/* Writable */
	ftrace_print("%s: return mask = 0x%04x", __func__, mask);

	return mask;
}

static ssize_t __bt_write(uint8_t *buf, size_t count, uint32_t flags)
{
	int32_t retval = 0;

#if 0 // Simfex
	bt_dbg_tp_evt(TP_ACT_WR_IN, 0, count, buf);
#endif
	retval = btmtk_send_data(g_sbdev->hdev, buf, count);

	if (retval < 0)
		BTMTK_ERR("%s: bt_core_send_data failed, retval %d", __func__, retval);
	else if (retval == 0) {
		/*
		 * TX queue cannot be digested in time and no space is available for write.
		 *
		 * If nonblocking mode, return -EAGAIN to let user retry,
		 * native program should not call write with no delay.
		 */
		if (flags & O_NONBLOCK) {
			BTMTK_WARN_LIMITTED("%s: Non-blocking write, no space is available!", __func__);
			retval = -EAGAIN;
		} else {
			/* TODO: blocking write case */
		}
	} else
		BTMTK_DBG("%s: Write bytes %d/%zd", __func__, retval, count);

	return retval;
}

static ssize_t BT_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	ssize_t retval = 0;
	size_t count = iov_iter_count(from);

	ftrace_print("%s get called, count %zd", __func__, count);
	down(&wr_mtx);

	if (rstflag != CHIP_RESET_NONE) {
		BTMTK_ERR("whole chip reset occurs! rstflag=%d", rstflag);
		retval = -EIO;
		goto OUT;
	}

	if (count > 0) {
		if (count > BT_BUFFER_SIZE) {
			BTMTK_WARN("Shorten write count from %zd to %d", count, BT_BUFFER_SIZE);
			count = BT_BUFFER_SIZE;
		}

		if (copy_from_iter(o_buf, count, from) != count) {
			retval = -EFAULT;
			goto OUT;
		}

		retval = __bt_write(o_buf, count, iocb->ki_filp->f_flags);
	}

OUT:
	up(&wr_mtx);
	return retval;
}

static ssize_t BT_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;

	ftrace_print("%s get called, count %zd", __func__, count);
	down(&wr_mtx);

	BTMTK_DBG("count %zd pos %lld", count, *f_pos);

	if (rstflag != CHIP_RESET_NONE) {
		BTMTK_ERR("whole chip reset occurs! rstflag=%d", rstflag);
		retval = -EIO;
		goto OUT;
	}

	if (count > 0) {
		if (count > BT_BUFFER_SIZE) {
			BTMTK_WARN("Shorten write count from %zd to %d", count, BT_BUFFER_SIZE);
			count = BT_BUFFER_SIZE;
		}

		if (copy_from_user(o_buf, buf, count)) {
			retval = -EFAULT;
			goto OUT;
		}

		retval = __bt_write(o_buf, count, filp->f_flags);
	}

OUT:
	up(&wr_mtx);
	return retval;
}

static ssize_t BT_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;

#if 0 // Simfex
	bt_dbg_tp_evt(TP_ACT_RD_IN, 0, count, NULL);
#endif
	ftrace_print("%s get called, count %zd", __func__, count);
	down(&rd_mtx);

	if (rstflag != CHIP_RESET_NONE) {
		BTMTK_DBG("%s: rstflag != CHIP_RESET_NONE", __func__);
		while (rstflag != CHIP_RESET_END && rstflag != CHIP_RESET_NONE) {
			/*
			 * If nonblocking mode, return -EIO directly.
			 * O_NONBLOCK is specified during open().
			 */
			if (filp->f_flags & O_NONBLOCK) {
				BTMTK_ERR_LIMITTED("Non-blocking read, whole chip reset occurs! rstflag=%d", rstflag);
				retval = -EIO;
				goto OUT;
			}

			wait_event(BT_wq, flag != 0);
			flag = 0;
		}
		/*
		 * Reset end, send Hardware Error event to stack only once.
		 * To avoid high frequency read from stack before process is killed, set rstflag to 3
		 * to block poll and read after Hardware Error event is sent.
		 */
		retval = bt_report_hw_error(i_buf, count, &rd_offset);
		if (rd_offset == sizeof(HCI_EVT_HW_ERROR)) {
			rd_offset = 0;
			rstflag = CHIP_RESET_NOTIFIED;
		}

		if (copy_to_user(buf, i_buf, retval)) {
			retval = -EFAULT;
			if (rstflag == CHIP_RESET_NOTIFIED)
				rstflag = CHIP_RESET_END;
		}

		goto OUT;
	}

	if (count > BT_BUFFER_SIZE) {
		BTMTK_WARN("Shorten read count from %zd to %d", count, BT_BUFFER_SIZE);
		count = BT_BUFFER_SIZE;
	}

	do {
		retval = btmtk_receive_data(g_sbdev->hdev, i_buf, count);
		if (retval < 0) {
			BTMTK_ERR("bt_core_receive_data failed, retval %zd", retval);
			goto OUT;
		} else if (retval == 0) { /* Got nothing, wait for RX queue's signal */
			/*
			 * If nonblocking mode, return -EAGAIN to let user retry.
			 * O_NONBLOCK is specified during open().
			 */
			if (filp->f_flags & O_NONBLOCK) {
				BTMTK_ERR_LIMITTED("Non-blocking read, no data is available!");
				retval = -EAGAIN;
				if (hw_err_retry++ > TRIGGER_HW_ERR_EVT_COUNT) {
					BTMTK_ERR("%s: hw_err_retry[%d] > %d", __func__, hw_err_retry
								, TRIGGER_HW_ERR_EVT_COUNT);
					retval = bt_report_hw_error(i_buf, count, &rd_offset);
					if (rd_offset == sizeof(HCI_EVT_HW_ERROR))
						rd_offset = 0;

					if (copy_to_user(buf, i_buf, retval))
						retval = -EFAULT;
				}
				goto OUT;
			}

			wait_event(BT_wq, flag != 0);
			flag = 0;
		} else { /* Got something from RX queue */
#if 0 // Simfex
			bt_dbg_tp_evt(TP_ACT_RD_OUT, 0, retval, i_buf);
#endif
			break;
		}
	} while (btmtk_rx_data_valid() && rstflag == CHIP_RESET_NONE);

	if (retval == 0) {
		if (rstflag != CHIP_RESET_END) { /* Should never happen */
			WARN(1, "Blocking read is waken up in unexpected case, rstflag=%d", rstflag);
			retval = -EIO;
			goto OUT;
		} else { /* Reset end, send Hardware Error event only once */
			retval = bt_report_hw_error(i_buf, count, &rd_offset);
			if (rd_offset == sizeof(HCI_EVT_HW_ERROR)) {
				rd_offset = 0;
				rstflag = CHIP_RESET_NOTIFIED;
			}
		}
	}

	if (copy_to_user(buf, i_buf, retval)) {
		hw_err_retry = 0;
		retval = -EFAULT;
		if (rstflag == CHIP_RESET_NOTIFIED)
			rstflag = CHIP_RESET_END;
	}

OUT:
	up(&rd_mtx);
	return retval;
}

int _ioctl_copy_evt_to_buf(uint8_t *buf, int len)
{
	BTMTK_INFO("%s", __func__);
	memset(ioc_buf, 0x00, sizeof(ioc_buf));
	ioc_buf[0] = 0x04; // evt packet type
	memcpy(ioc_buf + 1, buf, len); // copy evt to ioctl buffer
	BTMTK_INFO_RAW(ioc_buf, len + 1, "%s: len[%d] RX: ", __func__, len + 1);
	return 0;
}

static long BT_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int32_t retval = 0;

	BTMTK_DBG("%s: cmd[0x%08x]", __func__, cmd);
	memset(ioc_buf, 0x00, sizeof(ioc_buf));
	switch (cmd) {
	case COMBO_IOCTL_BT_HOST_DEBUG:
		/*
		 * input: arg(buf_size = 32): id[0:3], value[4:7], desc[8:31]
		 * output: none
		 */
		if (copy_from_user(ioc_buf, (uint8_t __user *)arg, IOCTL_BT_HOST_DEBUG_BUF_SIZE))
			retval = -EFAULT;
		else {
			uint32_t *pint32 = (uint32_t *)&ioc_buf[0];

			BTMTK_INFO("%s: id[%x], value[0x%08x], desc[%s]", __func__, pint32[0], pint32[1], &ioc_buf[8]);
			bthost_debug_save(pint32[0], pint32[1], (char *)&ioc_buf[8]);
		}
		break;
	case COMBO_IOCTL_BT_INTTRX:
		/*
		 * input: arg(buf_size = 128): hci cmd raw data
		 * output: arg(buf_size = 128): hci evt raw data
		 */
		if (copy_from_user(ioc_buf, (uint8_t __user *)arg, IOCTL_BT_HOST_INTTRX_SIZE))
			retval = -EFAULT;
		else {
			/* DynamicAdjustTxPower function */
			if (ioc_buf[0] == 0x01 && ioc_buf[1] == 0x2D && ioc_buf[2] == 0xFC) {
				BTMTK_INFO_RAW(ioc_buf, ioc_buf[3] + 4, "%s: len[%d] TX: ", __func__, ioc_buf[3] + 4);
				if (ioc_buf[4] == HCI_CMD_DY_ADJ_PWR_QUERY)
					btmtk_query_tx_power(g_sbdev, _ioctl_copy_evt_to_buf);
				else
					btmtk_set_tx_power(g_sbdev, ioc_buf[5], _ioctl_copy_evt_to_buf);
				if (copy_to_user((uint8_t __user *)arg, ioc_buf, IOCTL_BT_HOST_INTTRX_SIZE))
					retval = -EFAULT;
			} else if (ioc_buf[0] == 0x01 && ioc_buf[1] == 0x64 && ioc_buf[2] == 0xFD) {
				BTMTK_INFO_RAW(ioc_buf, ioc_buf[6] + 7, "%s: len[%d] TX: ", __func__, ioc_buf[6] + 7);
				if (ioc_buf[6] > 240) {
					BTMTK_ERR("eids buffer is large than expected");
					retval = -EFAULT;
				} else
					btmtk_send_finder_eids(ioc_buf, ioc_buf[6] + 7);
			} else
				retval = -EFAULT;
		}
		break;
	case COMBO_IOCTL_BT_FINDER_MODE:
		if (copy_from_user(ioc_buf, (uint8_t __user *)arg, 1))
			retval = -EFAULT;

		BTMTK_INFO("Finder's switch fucntion [%d], g_sbdev = %p", ioc_buf[0], g_sbdev);
		atomic_set(&g_sbdev->poweroff_finder_mode, ioc_buf[0]);
		break;
	default:
		break;
	}

	return retval;
}

static long BT_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return BT_unlocked_ioctl(filp, cmd, arg);
}

static int BT_open(struct inode *inode, struct file *file)
{
	int32_t ret;
	u8 retry = 0;

	/* Turn on BT */
	if (g_sbdev == NULL) {
		BTMTK_ERR("g_sbdev == NULL");
		return -1;
	}

	if (g_sbdev->hdev == NULL) {
		BTMTK_ERR("g_sbdev->hdev == NULL");
		return -1;
	}

	if (BT_open_count > 0) {
		BTMTK_INFO("Driver already openned, reuse openned driver as tunnel");
		BT_open_count++;
		return 0;
	}

	__pm_stay_awake(bt_wakelock);
	BTMTK_INFO("major %d minor %d (pid %d)", imajor(inode), iminor(inode), current->pid);

	/* BT on in reboot conflict with pre-cal flow */
	do {
		ret = -EAGAIN;
		if (btmtk_get_chip_state(g_sbdev) == BTMTK_STATE_DISCONNECT) {
			BTMTK_WARN("%s: uart disconnected", __func__);
			__pm_relax(bt_wakelock);
			return -1;
		}
		/* wait pre-cal done */
		if (g_sbdev->is_pre_cal_done) {
			/* wait uds_work done or remove uds_work from workqueue */
			cancel_work_sync(&g_sbdev->pwr_on_uds_work);
			ret = bt_open(g_sbdev->hdev);
		}
		if (ret) {
			BTMTK_WARN_LIMITTED("%s: ret[%d] retry[%d] is_pre_cal_done[%d]"
							, __func__, ret, retry, g_sbdev->is_pre_cal_done);
			msleep(50);
		}
	} while ((ret == -EIO || ret == -EAGAIN) && retry++ < BT_OPEN_MAX_RETRY);

	if (ret) {
		BTMTK_ERR("BT turn on fail!");
		__pm_relax(bt_wakelock);
		return ret;
	}

	BT_open_count++;
	btmtk_register_rx_event_cb(BT_event_cb);

	bt_ftrace_flag = 1;
	hw_err_retry = 0;
	__pm_relax(bt_wakelock);
	bthost_debug_init();

	BTMTK_INFO("BT turn on OK!");
	return 0;
}

static int BT_close(struct inode *inode, struct file *file)
{
	int32_t ret;

	bt_ftrace_flag = 0;
	//bt_core_unregister_rx_event_cb();

	/* err handle for uart disconnect */
	if (g_sbdev == NULL) {
		BTMTK_ERR("%s: g_sbdev == NULL", __func__);
		return -1;
	}

	if (g_sbdev->hdev == NULL) {
		BTMTK_ERR("%s: g_sbdev->hdev == NULL", __func__);
		/* if bt closed after bt disconnected, would not able to set fops to closed */
		/* Todo: how to excute bt_close flow to free unused data */
		g_sbdev->fops_state = BTMTK_FOPS_STATE_CLOSED;
		return -1;
	}

	if (BT_open_count > 1) {
		BT_open_count--;
		BTMTK_INFO("BT turn off OK!, BT_open_count [%d]", BT_open_count);
		return 0;
	}

	__pm_stay_awake(bt_wakelock);
	BTMTK_INFO("%s: major %d minor %d (pid %d)", __func__, imajor(inode), iminor(inode), current->pid);
	/* wait uds_work done or remove uds_work from workqueue */
	cancel_work_sync(&g_sbdev->pwr_on_uds_work);
	ret = bt_close(g_sbdev->hdev);
	__pm_relax(bt_wakelock);

	bthost_debug_init();

	BT_open_count--;
	if (ret) {
		BTMTK_ERR("BT turn off fail!");
		return ret;
	}

	BTMTK_INFO("BT turn off OK!");
	return 0;
}

const struct file_operations BT_fops = {
	.open = BT_open,
	.release = BT_close,
	.read = BT_read,
	.write = BT_write,
	.write_iter = BT_write_iter,
	/* .ioctl = BT_ioctl, */
	.unlocked_ioctl = BT_unlocked_ioctl,
	.compat_ioctl = BT_compat_ioctl,
	.poll = BT_poll
};

int BT_init(void)
{
	int32_t alloc_err = 0;
	int32_t cdv_err = 0;
	dev_t dev = MKDEV(BT_major, 0);

	sema_init(&wr_mtx, 1);
	sema_init(&rd_mtx, 1);
	init_waitqueue_head(&inq);

	/* Initialize wake lock for I/O operation */
	bt_wakelock = wakeup_source_register(NULL, "bt_drv_io");

	/* Allocate char device */
	alloc_err = register_chrdev_region(dev, BT_devs, BT_DRIVER_NAME);
	if (alloc_err) {
		BTMTK_ERR("Failed to register device numbers");
		goto alloc_error;
	}

	cdev_init(&BT_cdev, &BT_fops);
	BT_cdev.owner = THIS_MODULE;

	cdv_err = cdev_add(&BT_cdev, dev, BT_devs);
	if (cdv_err)
		goto cdv_error;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	BT_class = class_create(BT_DRIVER_NODE_NAME);
#else
	BT_class = class_create(THIS_MODULE, BT_DRIVER_NODE_NAME);
#endif
	if (IS_ERR(BT_class))
		goto create_node_error;
	BT_dev = device_create(BT_class, NULL, dev, NULL, BT_DRIVER_NODE_NAME);
	if (IS_ERR(BT_dev))
		goto create_node_error;

	BTMTK_INFO("%s driver(major %d) installed", BT_DRIVER_NAME, BT_major);
	return 0;

create_node_error:
	if (BT_class && !IS_ERR(BT_class)) {
		class_destroy(BT_class);
		BT_class = NULL;
	}

	cdev_del(&BT_cdev);

cdv_error:
	unregister_chrdev_region(dev, BT_devs);

alloc_error:
	wakeup_source_unregister(bt_wakelock);
	return -1;
}

void BT_exit(void)
{
	dev_t dev = MKDEV(BT_major, 0);

	if (BT_dev && !IS_ERR(BT_dev)) {
		device_destroy(BT_class, dev);
		BT_dev = NULL;
	}
	if (BT_class && !IS_ERR(BT_class)) {
		class_destroy(BT_class);
		BT_class = NULL;
	}

	cdev_del(&BT_cdev);
	unregister_chrdev_region(dev, BT_devs);

	/* Destroy wake lock */
	wakeup_source_unregister(bt_wakelock);

	BTMTK_INFO("%s driver removed", BT_DRIVER_NAME);
}

inline int btmtk_chardev_init(void)
{
	return BT_init();
}

#else

int BT_init(void)
{
	BTMTK_INFO("%s: not device node, return", __func__);
	return 0;
}

void BT_exit(void)
{
	BTMTK_INFO("%s: not device node, return", __func__);
}

#endif // USE_DEVICE_NODE


#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
/* build-in mode */
int mtk_wcn_stpbt_drv_init(void)
{
	return main_driver_init();
}
EXPORT_SYMBOL(mtk_wcn_stpbt_drv_init);

void mtk_wcn_stpbt_drv_exit(void)
{
	return main_driver_exit();
}
EXPORT_SYMBOL(mtk_wcn_stpbt_drv_exit);
#endif
