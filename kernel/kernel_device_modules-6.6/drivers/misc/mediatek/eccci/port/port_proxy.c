// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/icmpv6.h>
#include <linux/of.h>
#include <linux/sched/clock.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/ndisc.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <clocksource/arm_arch_timer.h>

#include "mt-plat/mtk_ccci_common.h"

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_fsm.h"
#include "ccci_modem.h"
#include "ccci_hif.h"
#include "ccci_port.h"
#include "modem_sys.h"
#include "port_proxy.h"
#include "port_udc.h"
#define TAG PORT
#define CCCI_DEV_NAME "ccci"

static struct port_proxy *port_proxyp;

#define CHECK_HIF_ID(hif_id)
#define CHECK_QUEUE_ID(queue_id)

struct ccci_proc_user {
	unsigned int busy;
	int left_len;
	void __iomem *curr_addr;
};

static spinlock_t file_lock;
static struct port_t *port_list[CCCI_MAX_CH_NUM];
int port_md_gen;

static struct port_t *ccci_port_get_port_by_user_id(unsigned int user_id)
{
	if (user_id < CCCI_MAX_CH_NUM)
		return port_list[user_id];

	return NULL;
}

char *ccci_port_get_dev_name(unsigned int rx_user_id)
{
	struct port_t *port = ccci_port_get_port_by_user_id(rx_user_id);

	if (!port)
		return NULL;

	return port->name;
}
EXPORT_SYMBOL(ccci_port_get_dev_name);

int send_new_time_to_new_md(int tz)
{
	struct timespec64 tv;
	unsigned int timeinfo[8];
	char ccci_time[88];
	int ret;
	int index;
	char *name = "ccci_0_202";
	u64 usec = 0;
	u64 sys_counter = 0;

	sys_counter = arch_timer_read_counter();
	ktime_get_real_ts64(&tv);
	timeinfo[0] = tv.tv_sec;
	timeinfo[1] = sizeof(tv.tv_sec) > 4 ? tv.tv_sec >> 32 : 0;
	timeinfo[2] = tz;
	timeinfo[3] = sys_tz.tz_dsttime;
	if (ccci_md_get_support_microsecond_version() == HIRES_TIME_VER) {
		usec = tv.tv_nsec/NSEC_PER_USEC;
		timeinfo[4] = usec;
		timeinfo[5] = usec >> 32;
		timeinfo[6] = sys_counter;
		timeinfo[7] = sys_counter >> 32;
		scnprintf(ccci_time, sizeof(ccci_time),
			  "%010u,%010u,%010u,%010u,%010u,%010u,%010u,%010u",
			  timeinfo[0], timeinfo[1], timeinfo[2], timeinfo[3],
			  timeinfo[4], timeinfo[5], timeinfo[6], timeinfo[7]);
	} else {
		scnprintf(ccci_time, sizeof(ccci_time), "%010u,%010u,%010u,%010u",
			  timeinfo[0], timeinfo[1], timeinfo[2], timeinfo[3]);
	}

	CCCI_NORMAL_LOG(0, CHAR, "CTime update: %s\n", ccci_time);

	index = mtk_ccci_request_port(name);
	ret = mtk_ccci_send_data(index, ccci_time, strlen(ccci_time) + 1);

	return ret;
}

int port_dev_kernel_read(struct port_t *port, char *buf, int size)
{
	int read_done = 0, ret = 0, read_len = 0, md_state;
	struct sk_buff *skb = NULL;
	unsigned long flags = 0;

	md_state = ccci_fsm_get_md_state();
	if (md_state != READY && port->tx_ch != CCCI_FS_TX &&
		port->tx_ch != CCCI_RPC_TX) {
		pr_info_ratelimited(
			"port %s read data fail when md_state = %d\n",
			port->name, md_state);
		return -ENODEV;
	}

READ_START:
	if (skb_queue_empty(&port->rx_skb_list)) {
		spin_lock_irq(&port->rx_wq.lock);
		ret = wait_event_interruptible_locked_irq(port->rx_wq,
			!skb_queue_empty(&port->rx_skb_list));
		spin_unlock_irq(&port->rx_wq.lock);
		if (ret == -ERESTARTSYS) {
			ret = -EINTR;
			goto exit;
		}
	}
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	if (skb_queue_empty(&port->rx_skb_list)) {
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		goto READ_START;
	}

	skb = skb_peek(&port->rx_skb_list);
	if (skb == NULL) {
		ret = -EFAULT;
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		goto exit;
	}

	read_len = skb->len;

	if (size >= read_len) {
		read_done = 1;
		__skb_unlink(skb, &port->rx_skb_list);
		/*
		 * here we only ask for more request when rx list is empty.
		 * no need to be too gready, because
		 * for most of the case, queue will not stop
		 * sending request to port.
		 * actually we just need to ask by ourselves when
		 * we rejected requests before. these
		 * rejected requests will staty in queue's buffer and may
		 * never get a chance to be handled again.
		 */
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);

	} else {
		read_len = size;
	}
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_CH_TRAFFIC)
		port_ch_dump(port, 0, skb->data, read_len);
	/* 3. copy to user */
	memcpy(buf, skb->data, read_len);
	skb_pull(skb, read_len);
	/* 4. free request */
	if (read_done)
		ccci_free_skb(skb);

 exit:
	return ret ? ret : read_len;
}

int mtk_ccci_send_data(int index, const char *buf, int size)
{
	int ret, actual_count, header_len, alloc_size = 0;
	int md_state;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	struct port_t *tx_port = NULL;

	if (size <= 0) {
		CCCI_ERROR_LOG(-1, CHAR, "invalid size %d for port %d\n",
				size, index);
		return -EINVAL;
	}
	ret = find_port_by_channel(index, &tx_port);
	if (ret < 0)
		return -EINVAL;
	if (!tx_port->name) {
		CCCI_ERROR_LOG(-1, CHAR, "port name is null\n");
		return -1;
	}

	md_state = ccci_fsm_get_md_state();
	if (md_state != READY && tx_port->tx_ch != CCCI_FS_TX &&
		tx_port->tx_ch != CCCI_RPC_TX) {
		CCCI_ERROR_LOG(-1, CHAR,
			"port %s send data fail when md_state = %d\n",
			tx_port->name, md_state);
		return -ENODEV;
	}
	header_len = sizeof(struct ccci_header) +
		(tx_port->rx_ch == CCCI_FS_RX ? sizeof(unsigned int) : 0);
	if (tx_port->flags & PORT_F_USER_HEADER) {
		if (size > (CCCI_MTU + header_len)) {
			CCCI_ERROR_LOG(-1, CHAR,
			"size %d is larger than MTU for index %d",
					size, index);
			return -ENOMEM;
		}
		alloc_size = actual_count = size;
	} else {
		actual_count = size > CCCI_MTU ? CCCI_MTU : size;
		alloc_size = actual_count + header_len;
	}
	skb = ccci_alloc_skb(alloc_size, 1, 1);
	if (skb) {
		if (!(tx_port->flags & PORT_F_USER_HEADER)) {
			ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
			ccci_h->data[0] = 0;
			ccci_h->data[1] = actual_count +
					sizeof(struct ccci_header);
			ccci_h->channel = tx_port->tx_ch;
			ccci_h->reserved = 0;
		} else {
			ccci_h = (struct ccci_header *)skb->data;
		}
		memcpy(skb_put(skb, actual_count), buf, actual_count);
		ret = port_send_skb_to_md(tx_port, skb, 1);
		if (ret) {
			CCCI_ERROR_LOG(-1, CHAR,
				"port %s send skb fail ret = %d\n",
					tx_port->name, ret);
			return ret;
		}
		return actual_count;
	}
	CCCI_ERROR_LOG(-1, CHAR,
		"ccci alloc skb for port %s fail",
		tx_port->name);
	return -1;
}
EXPORT_SYMBOL(mtk_ccci_send_data);

int mtk_ccci_read_data(int index, char *buf, size_t count)
{
	int ret = 0;
	struct port_t *rx_port = NULL;

	ret = find_port_by_channel(index, &rx_port);
	if (ret < 0)
		return -EINVAL;

	if (atomic_read(&rx_port->usage_cnt)) {
		ret = port_dev_kernel_read(rx_port, buf, count);
	} else {
		CCCI_ERROR_LOG(-1, CHAR,  "open %s before read\n",
				rx_port->name);
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL(mtk_ccci_read_data);

static inline void proxy_set_critical_user(struct port_proxy *proxy_p,
	int user_id, int enabled)
{
	if (enabled)
		proxy_p->critical_user_active |= (1U << user_id);
	else
		proxy_p->critical_user_active &= ~(1U << user_id);
}

static inline int proxy_get_critical_user(struct port_proxy *proxy_p,
	int user_id)
{
	return ((proxy_p->critical_user_active &
		(1U << user_id)) >> user_id);
}

/****************************************************************************/
/*REGION: default char device operation definition for node,
 * which export node for userspace
 ***************************************************************************/
int port_dev_open(struct inode *inode, struct file *file)
{
	int major = imajor(inode);
	int minor = iminor(inode);
	struct port_t *port;

	port = port_get_by_node(major, minor);
	if (port == NULL) {
		CCCI_ERROR_LOG(0, CHAR, "port_get_by_node fail\n");
		return -1;
	}

	if (port->rx_ch != CCCI_CCB_CTRL && atomic_read(&port->usage_cnt)) {
		CCCI_ERROR_LOG(0, CHAR,
			"port %s open fail with EBUSY\n", port->name);
		return -EBUSY;
	}

	CCCI_NORMAL_LOG(0, CHAR,
		"port %s open with flag %X by %s\n", port->name, file->f_flags,
		current->comm);
	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	nonseekable_open(inode, file);
	port_user_register(port);
	return 0;
}

int port_dev_close(struct inode *inode, struct file *file)
{
	struct port_t *port = file->private_data;
	struct sk_buff *skb = NULL;
	unsigned long flags;
	int clear_cnt = 0;

	/* 0. decrease usage count, so when we ask more,
	 * the packet can be dropped in recv_request
	 */
	atomic_dec(&port->usage_cnt);
	/* 1. purge Rx request list */
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL) {
		ccci_free_skb(skb);
		clear_cnt++;
	}
	port->rx_drop_cnt += clear_cnt;
	/*  flush Rx */
	port_ask_more_req_to_md(port);
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	CCCI_NORMAL_LOG(0, CHAR,
		"port %s close by %s rx_len=%d empty=%d, clear_cnt=%d, drop=%d usagecnt=%d\n",
		port->name, current->comm, port->rx_skb_list.qlen,
		skb_queue_empty(&port->rx_skb_list),
		clear_cnt, port->rx_drop_cnt, atomic_read(&port->usage_cnt));
	ccci_event_log(
		" port %s closed by %s rx_len=%d empty=%d, clear_cnt=%d, drop=%d, usagecnt=%d\n",
		port->name, current->comm,
		port->rx_skb_list.qlen,
		skb_queue_empty(&port->rx_skb_list),
		clear_cnt, port->rx_drop_cnt, atomic_read(&port->usage_cnt));

	port_user_unregister(port);

	return 0;
}

/*
 * add __no_kcan for !skb_queue_empty(&port->rx_skb_list)
 * the conidition parameter in wait_event_interruptible_locked_irq.
 * TODO: remove once design changed,
 * when no need two spinlock for the two parameters.
 */
__no_kcsan
ssize_t port_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct port_t *port = file->private_data;
	struct sk_buff *skb = NULL;
	int ret = 0, read_len = 0, full_req_done = 0;
	unsigned long flags = 0;
	u64 ts_s, ts_1, ts_e;

READ_START:
	/* 1. get incoming request */
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	if (skb_queue_empty(&port->rx_skb_list)) {
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		if (!(file->f_flags & O_NONBLOCK)) {
			spin_lock_irq(&port->rx_wq.lock);
			ret = wait_event_interruptible_locked_irq(port->rx_wq,
				!skb_queue_empty(&port->rx_skb_list));
			spin_unlock_irq(&port->rx_wq.lock);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	} else
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);

	CCCI_DEBUG_LOG(0, CHAR,
		"read on %s for %zu\n", port->name, count);
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	if (skb_queue_empty(&port->rx_skb_list)) {
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		if (!(file->f_flags & O_NONBLOCK)) {
			goto READ_START;
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}

	skb = skb_peek(&port->rx_skb_list);
	if (skb == NULL) {
		ret = -EFAULT;
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		goto exit;
	}

	read_len = skb->len;

	if (count >= read_len) {
		full_req_done = 1;
		__skb_unlink(skb, &port->rx_skb_list);
		/*
		 * here we only ask for more request when rx list is empty.
		 * no need to be too gready, because
		 * for most of the case, queue will not stop
		 * sending request to port.
		 * actually we just need to ask by ourselves when
		 * we rejected requests before. these
		 * rejected requests will staty in queue's buffer and may
		 * never get a chance to be handled again.
		 */
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);
	} else {
		read_len = count;
	}
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_CH_TRAFFIC)
		port_ch_dump(port, 0, skb->data, read_len);
	/* 3. copy to user */

	ts_s = local_clock();
	if (copy_to_user(buf, skb->data, read_len)) {
		CCCI_ERROR_LOG(0, CHAR,
			"read on %s, copy to user failed, %d/%zu\n",
			port->name, read_len, count);
		ret = -EFAULT;
	}
	ts_1 = local_clock();

#ifdef CONFIG_MTK_SRIL_SUPPORT
	if (port->rx_ch == CCCI_RIL_IPC0_RX || port->rx_ch == CCCI_RIL_IPC1_RX) {
		print_hex_dump(KERN_INFO, "3. mif: RX: ",
				DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
	}
#endif
	skb_pull(skb, read_len);
	/* 4. free request */
	if (full_req_done)
		ccci_free_skb(skb);

	ts_e = local_clock();
	if (ts_e - ts_s > 1000000000ULL)
		CCCI_ERROR_LOG(0, CHAR,
			"ts_s: %u; ts_1: %u; ts_e: %u;",
			do_div(ts_s, 1000000),
			do_div(ts_1, 1000000),
			do_div(ts_e, 1000000));


 exit:
#ifdef CONFIG_MTK_SRIL_SUPPORT
	if (ret < 0 && (port->rx_ch == CCCI_RIL_IPC0_RX || port->rx_ch == CCCI_RIL_IPC1_RX))
		CCCI_ERROR_LOG(0, CHAR,
				"RILD failed to read ipc packet, ret = %d, rx_ch = %d\n",
				ret, port->rx_ch);
#endif
	return ret ? ret : read_len;
}

ssize_t port_dev_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	struct port_t *port = file->private_data;
	unsigned char blocking = !(file->f_flags & O_NONBLOCK);
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	size_t actual_count = 0, alloc_size = 0;
	int ret = 0, header_len = 0;
	int md_state;

	if (count == 0)
		return -EINVAL;

	md_state = ccci_fsm_get_md_state();
	if ((md_state == BOOT_WAITING_FOR_HS1
		|| md_state == BOOT_WAITING_FOR_HS2)
		&& port->tx_ch != CCCI_FS_TX && port->tx_ch != CCCI_RPC_TX) {
		CCCI_DEBUG_LOG(0, TAG,
			"port %s ch%d write fail when md_state=%d !!!\n",
			port->name, port->tx_ch, md_state);
		return -ENODEV;
	}

	header_len = sizeof(struct ccci_header) +
		(port->rx_ch == CCCI_FS_RX ? sizeof(unsigned int) : 0);
	if (port->flags & PORT_F_USER_HEADER) {
		if (count > (CCCI_MTU + header_len)) {
			CCCI_ERROR_LOG(0, CHAR,
				"reject packet(size=%zu ), larger than MTU on %s\n",
				count, port->name);
			return -ENOMEM;
		}
		actual_count = count;
		alloc_size = actual_count;
	} else {
		actual_count = count > CCCI_MTU ? CCCI_MTU : count;
		alloc_size = actual_count + header_len;
	}
	skb = ccci_alloc_skb(alloc_size, 1, blocking);
	if (skb) {
		/* 1. for Tx packet, who issued it should know
		 * whether recycle it  or not
		 */
		/* 2. prepare CCCI header, every member of header
		 * should be re-write as request may be re-used
		 */
		if (!(port->flags & PORT_F_USER_HEADER)) {
			ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
			ccci_h->data[0] = 0;
			ccci_h->data[1] =
				actual_count + sizeof(struct ccci_header);
			ccci_h->channel = port->tx_ch;
			ccci_h->reserved = 0;
		} else {
			ccci_h = (struct ccci_header *)skb->data;
		}
		/* 3. get user data */
		ret = copy_from_user(skb_put(skb, actual_count),
				buf, actual_count);
		if (ret)
			goto err_out;
		if (port->flags & PORT_F_USER_HEADER) {
			/* ccci_header provided by user,
			 * For only send ccci_header
			 * without additional data case,
			 * data[0]=CCCI_MAGIC_NUM, data[1]=user_data,
			 * ch=tx_channel,reserved=no_use,
			 * For send ccci_header with additional data case,
			 * data[0]=0, data[1]=data_size, ch=tx_channel,
			 * reserved=user_data
			 */
			if (actual_count == sizeof(struct ccci_header))
				ccci_h->data[0] = CCCI_MAGIC_NUM;
			else
				ccci_h->data[1] = actual_count;
			/* as EEMCS VA will not fill this filed */
			ccci_h->channel = port->tx_ch;
		}

		if (port->flags & PORT_F_CH_TRAFFIC) {
			if (port->flags & PORT_F_USER_HEADER)
				port_ch_dump(port, 1, skb->data, actual_count);
			else
				port_ch_dump(port, 1, skb->data + header_len,
					actual_count);
		}
		if (port->rx_ch == CCCI_IPC_RX) {
			ret = port_ipc_write_check_id(port, skb);
			if (ret < 0)
				goto err_out;
			else
				ccci_h->reserved = ret;	/* Unity ID */
		}
		/* 4. send out */
		ret = port_send_skb_to_md(port, skb, blocking);
		/* do NOT reference request after called this,
		 * modem may have freed it, unless you get -EBUSY
		 */
		if (ret) {
			if (ret == -EBUSY && !blocking)
				ret = -EAGAIN;
			goto err_out;
		}
		return actual_count;

 err_out:
		if ((ret != -ETXTBSY) && (ret != -ENODEV)) {
			CCCI_NORMAL_LOG(0, CHAR,
				"write error done on %s, l=%zu r=%d\n",
				port->name, actual_count, ret);
		}

		ccci_free_skb(skb);
		return ret;
	}
	/* consider this case as non-blocking */
	return -EBUSY;
}

long port_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct port_t *port = file->private_data;
	struct ccci_smem_region *sub_smem = NULL;

	switch (cmd) {
	case CCCI_IOC_SET_HEADER:
		port->flags |= PORT_F_USER_HEADER;
		break;
	case CCCI_IOC_CLR_HEADER:
		port->flags &= ~PORT_F_USER_HEADER;
		break;
	case CCCI_IOC_SMEM_BASE:
		if (port->rx_ch != CCCI_WIFI_RX)
			return -EFAULT;
		sub_smem = ccci_md_get_smem_by_user_id(SMEM_USER_MD_WIFI_PROXY);

		if (sub_smem == NULL)
			return -EFAULT;
		CCCI_NORMAL_LOG(0, TAG, "wifi smem phy =%lx\n",
			(unsigned long)sub_smem->base_ap_view_phy);
		ret = put_user((unsigned int)sub_smem->base_ap_view_phy,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SMEM_LEN:
		if (port->rx_ch != CCCI_WIFI_RX)
			return -EFAULT;
		sub_smem = ccci_md_get_smem_by_user_id(SMEM_USER_MD_WIFI_PROXY);

		if (sub_smem == NULL)
			return -EFAULT;
		sub_smem->size &= ~(PAGE_SIZE - 1);
		CCCI_NORMAL_LOG(0, TAG, "wifi smem size =%lx(%d)\n",
			(unsigned long)sub_smem->size, (int)PAGE_SIZE);
		ret = put_user((unsigned int)sub_smem->size,
				(unsigned int __user *)arg);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret == -1)
		ret = ccci_fsm_ioctl(cmd, arg);

	return ret;
}

#ifdef CONFIG_COMPAT
long port_dev_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		CCCI_ERROR_LOG(0, CHAR,
			"dev_char_compat_ioctl(!filp->f_op || !filp->f_op->unlocked_ioctl)\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case CCCI_IOC_PCM_BASE_ADDR:
	case CCCI_IOC_PCM_LEN:
	case CCCI_IOC_ALLOC_MD_LOG_MEM:
	case CCCI_IOC_FORCE_FD:
	case CCCI_IOC_AP_ENG_BUILD:
	case CCCI_IOC_GET_MD_MEM_SIZE:
		CCCI_ERROR_LOG(0, CHAR,
			"dev_char_compat_ioctl deprecated cmd(%d)\n", cmd);
		return 0;
	default:
		return filp->f_op->unlocked_ioctl(filp, cmd,
				(unsigned long)compat_ptr(arg));
	}
}
#endif


int port_dev_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct port_t *port = fp->private_data;
	int len, ret;
	unsigned long pfn;
	struct ccci_smem_region *wifi_smem = NULL;

	if (port->rx_ch != CCCI_WIFI_RX)
		return -EFAULT;

	wifi_smem = ccci_md_get_smem_by_user_id(SMEM_USER_MD_WIFI_PROXY);
	if (wifi_smem == NULL)
		return -EFAULT;
	wifi_smem->size = (wifi_smem->size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
	CCCI_NORMAL_LOG(0, CHAR,
			"remap wifi smem addr:0x%llx len:%d  map-len:%lu\n",
			(unsigned long long)wifi_smem->base_ap_view_phy,
			wifi_smem->size, vma->vm_end - vma->vm_start);
	if ((vma->vm_end - vma->vm_start) > wifi_smem->size) {
		CCCI_ERROR_LOG(0, CHAR,
			"invalid mm size request from %s\n",
			port->name);
		return -EINVAL;
	}

	len = (vma->vm_end - vma->vm_start) < wifi_smem->size ?
		vma->vm_end - vma->vm_start : wifi_smem->size;
	pfn = wifi_smem->base_ap_view_phy;
	pfn >>= PAGE_SHIFT;
	/* ensure that memory does not get swapped to disk */
	vm_flags_set(vma, VM_IO);
	/* ensure non-cacheable */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = remap_pfn_range(vma, vma->vm_start, pfn,
				len, vma->vm_page_prot);
	if (ret) {
		CCCI_ERROR_LOG(0, CHAR,
			"wifi smem remap failed %d/%lx, 0x%llx -> 0x%llx\n",
			ret, pfn,
			(unsigned long long)wifi_smem->base_ap_view_phy,
			(unsigned long long)vma->vm_start);
		return -EAGAIN;
	}

	CCCI_NORMAL_LOG(0, CHAR,
		"wifi smem remap succeed %lx, 0x%llx -> 0x%llx\n", pfn,
		(unsigned long long)wifi_smem->base_ap_view_phy,
		(unsigned long long)vma->vm_start);

	return 0;
}

/**************************************************************************/
/* REGION: port common API implementation,
 * these APIs are valiable for every port
 */
/**************************************************************************/
static inline int port_struct_init(struct port_t *port,
	struct port_proxy *port_p)
{
	INIT_LIST_HEAD(&port->entry);
	INIT_LIST_HEAD(&port->exp_entry);
	INIT_LIST_HEAD(&port->queue_entry);
	skb_queue_head_init(&port->rx_skb_list);
	if (port->tx_ch == CCCI_UDC_TX)
		skb_queue_head_init(&port->rx_skb_list_hp);
	init_waitqueue_head(&port->rx_wq);
	port->tx_busy_count = 0;
	port->rx_busy_count = 0;
	atomic_set(&port->usage_cnt, 0);
	port->port_proxy = port_p;

	port->rx_wakelock = wakeup_source_register(NULL, port->name);
	if (!port->rx_wakelock) {
		CCCI_ERROR_LOG(0, TAG,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	return 0;
}

static void port_dump_net(struct port_t *port, int dir,
	void *msg_buf)
{
	int type = 0;
	u64 ts_nsec;
	unsigned long rem_nsec;
	struct sk_buff *skb = (struct sk_buff *)msg_buf;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct icmp6hdr *icmp6h = NULL;

	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	if (skb->protocol == htons(ETH_P_IP)) {
		iph = ip_hdr(skb);
		CCCI_HISTORY_LOG(0, TAG,
			"[%5lu.%06lu]net(%d):%d,%d,(id:%x,src:%pI4,dst:%pI4)\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			dir, port->flags, port->rx_ch, iph->id,
			&iph->saddr, &iph->daddr);
	}
	if (skb->protocol == htons(ETH_P_IPV6)) {
		ip6h = ipv6_hdr(skb);
		if (ip6h->nexthdr == NEXTHDR_ICMP) {
			icmp6h = icmp6_hdr(skb);
			type = icmp6h->icmp6_type;
		}
		CCCI_HISTORY_LOG(0, TAG,
		"[%5lu.%06lu]net(%d):%d,%d,(src:%pI6,dst:%pI6,len:%d,type:%d)\n",
		(unsigned long)ts_nsec, rem_nsec / 1000,
		dir, port->flags, port->rx_ch,
		&ip6h->saddr, &ip6h->daddr, skb->len, type);
	}
}

static void port_dump_string(struct port_t *port, int dir,
	void *msg_buf, int len)
{
#define DUMP_BUF_SIZE 32
	unsigned char *char_ptr = (unsigned char *)msg_buf;
	char buf[DUMP_BUF_SIZE];
	int i, j;
	u64 ts_nsec;
	unsigned long rem_nsec;
	char *replace_str = NULL;
	int ret = 0;

	for (i = 0, j = 0; i < len && i < DUMP_BUF_SIZE &&
		j + 4 < DUMP_BUF_SIZE; i++) {
		if (((char_ptr[i] >= 32) && (char_ptr[i] <= 126))) {
			buf[j] = char_ptr[i];
			j += 1;
		} else if (char_ptr[i] == '\r' ||
			char_ptr[i] == '\n' ||
			char_ptr[i] == '\t') {
			switch (char_ptr[i]) {
			case '\r':
				replace_str = "\\r";
				break;
			case '\n':
				replace_str = "\\n";
				break;
			case '\t':
				replace_str = "\\t";
				break;
			default:
				replace_str = "";
				break;
			}
			ret = snprintf(buf+j, DUMP_BUF_SIZE - j,
				"%s", replace_str);
			j += 2;
		} else {
			ret = snprintf(buf+j, DUMP_BUF_SIZE - j,
				"[%02X]", char_ptr[i]);
			j += 4;
		}
		if (ret < 0) {
			CCCI_ERROR_LOG(0, TAG,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			break;
		}
	}
	buf[j] = '\0';
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	if (dir == 0)
		CCCI_HISTORY_LOG(0, TAG,
			"[%5lu.%06lu]C:%d,%d(%d,%d,%d) %s: %d<%s\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->rx_ch,
			port->rx_skb_list.qlen,
			atomic_read(&port->rx_pkg_cnt),
			port->rx_drop_cnt, "R", len, buf);
	else
		CCCI_HISTORY_LOG(0, TAG,
			"[%5lu.%06lu]C:%d,%d(%d) %s: %d>%s\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->tx_ch,
			port->tx_pkg_cnt, "W", len, buf);
}
static void port_dump_raw_data(struct port_t *port, int dir,
	void *msg_buf, int len)
{
#define DUMP_RAW_DATA_SIZE 16
	unsigned int *curr_p = (unsigned int *)msg_buf;
	unsigned char *curr_ch_p = NULL;
	int _16_fix_num;
	int tail_num;
	char buf[16];
	int i, j;
	int dump_size;
	u64 ts_nsec;
	unsigned long rem_nsec;

	if (curr_p == NULL) {
		CCCI_HISTORY_LOG(0, TAG, "start_addr <NULL>\n");
		return;
	}
	if (len == 0) {
		CCCI_HISTORY_LOG(0, TAG, "len [0]\n");
		return;
	}
	if (port->rx_ch == CCCI_FS_RX)
		curr_p++;

	dump_size = len > DUMP_RAW_DATA_SIZE ? DUMP_RAW_DATA_SIZE : len;
	_16_fix_num = dump_size / 16;
	tail_num = dump_size % 16;

	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);

	if (dir == 0)
		CCCI_HISTORY_LOG(0, TAG,
			"[%5lu.%06lu]C:%d,%d(%d,%d,%d) %s: %d<",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->rx_ch,
			port->rx_skb_list.qlen,
			atomic_read(&port->rx_pkg_cnt),
			port->rx_drop_cnt, "R", len);
	else
		CCCI_HISTORY_LOG(0, TAG,
			"[%5lu.%06lu]C:%d,%d(%d) %s: %d>",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			port->flags, port->tx_ch,
			port->tx_pkg_cnt, "W", len);
	/* Fix section */
	for (i = 0; i < _16_fix_num; i++) {
		CCCI_HISTORY_LOG(0, TAG,
			"%03X: %08X %08X %08X %08X\n",
			i * 16, *curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3));
		curr_p += 4;
	}

	/* Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		CCCI_HISTORY_LOG(0, TAG,
			"%03X: %08X %08X %08X %08X\n",
			i * 16, *curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3));
	}
}

/**
 * port_get_queue_no - port select queue number
 * @port: port abstract structure
 * @dir: data transmission direction
 * @priority_level: modem hardware queue priority level
 * priority_level = -1 --> use port_cfg default config for ctrl path
 * priority_level = PRIORITY_0 --> lowest priority use port_cfg default
 * priority_level = PRIORITY_1 --> medium priority use MD_HW_MEDIUM_Q(Q2)
 * priority_level = PRIORITY_2 --> highest priority use MD_HW_HIGH_Q(Q1)
 */
static inline int port_get_queue_no(struct port_t *port, enum DIRECTION dir,
			 int priority_level)
{
	int md_state;

	md_state = ccci_fsm_get_md_state();
	if (dir == OUT) {
		if (priority_level == PRIORITY_2)
			return (md_state == EXCEPTION ? port->txq_exp_index :
				MD_HW_HIGH_Q);
		if (priority_level == PRIORITY_1)
			return (md_state == EXCEPTION ? port->txq_exp_index :
				MD_HW_MEDIUM_Q);
		if (priority_level == PRIORITY_0)
			return (md_state == EXCEPTION ? port->txq_exp_index :
				port->txq_index);
		return (md_state == EXCEPTION ? port->txq_exp_index :
			port->txq_index);
	} else if (dir == IN)
		return (md_state == EXCEPTION ? port->rxq_exp_index :
			port->rxq_index);
	else
		return -1;
}

static inline int port_adjust_skb(struct port_t *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = NULL;

	ccci_h = (struct ccci_header *)skb->data;
	/* header provide by user */
	if (port->flags & PORT_F_USER_HEADER) {
		/* CCCI_MON_CH should fall in here,
		 * as header must be send to md_init
		 */
		if (ccci_h->data[0] == CCCI_MAGIC_NUM) {
			if (unlikely(skb->len > sizeof(struct ccci_header))) {
				CCCI_ERROR_LOG(0, TAG,
					"recv unexpected data for %s, skb->len=%d\n",
					port->name, skb->len);
				skb_trim(skb, sizeof(struct ccci_header));
			}
		}
	} else {
		/* remove CCCI header */
		skb_pull(skb, sizeof(struct ccci_header));
	}

	return 0;
}

/*
 *This API is common API for port to resceive skb form modem or HIF,
 *
 */
int port_recv_skb(struct port_t *port, struct sk_buff *skb)
{
	unsigned long flags;
	struct ccci_header ccci_h;
	unsigned long rem_nsec;
	u64 ts_nsec;

	memset(&ccci_h, 0, sizeof(struct ccci_header));
	memcpy((void *)(&ccci_h), skb->data, sizeof(struct ccci_header));

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	CCCI_DEBUG_LOG(0, TAG,
		"recv on %s, len=%d\n", port->name,
		port->rx_skb_list.qlen);
	if (port->rx_skb_list.qlen < port->rx_length_th) {
		port->flags &= ~PORT_F_RX_FULLED;
		if (port->flags & PORT_F_ADJUST_HEADER)
			port_adjust_skb(port, skb);
		if (ccci_h.channel == CCCI_STATUS_RX)
			port->skb_handler(port, skb);
		else  {
			__skb_queue_tail(&port->rx_skb_list, skb);
			if (ccci_h.channel == CCCI_SYSTEM_RX) {
				ts_nsec = sched_clock();
				rem_nsec = do_div(ts_nsec, 1000000000);
				CCCI_HISTORY_LOG(0, TAG,
					"[%5lu.%06lu]sysmsg+%08x %08x %04x\n",
					(unsigned long)ts_nsec, rem_nsec / 1000,
					ccci_h.data[1], ccci_h.reserved,
					ccci_h.seq_num);
			}
		}

#ifdef CONFIG_MTK_SRIL_SUPPORT
		if (ccci_h.channel == CCCI_RIL_IPC0_RX
			|| ccci_h.channel == CCCI_RIL_IPC1_RX) {
			print_hex_dump(KERN_INFO, "2. mif: RX: ",
				DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
		}
#endif
		atomic_inc(&port->rx_pkg_cnt);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		//print log after unlock
		if (ccci_h.channel == CCCI_STATUS_RX) {
			CCCI_NORMAL_LOG(0, TAG,
				"received MD status response 0x%x\n", *(((u32 *)skb->data) + 2));
			ccci_free_skb(skb);
		}
		__pm_wakeup_event(port->rx_wakelock, jiffies_to_msecs(HZ/2));
		spin_lock_irqsave(&port->rx_wq.lock, flags);
		wake_up_all_locked(&port->rx_wq);
		spin_unlock_irqrestore(&port->rx_wq.lock, flags);

		return 0;
	}
	port->flags |= PORT_F_RX_FULLED;
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_ALLOW_DROP) {
		CCCI_NORMAL_LOG(0, TAG,
			"port %s Rx full, drop packet\n",
			port->name);
		goto drop;
	} else {
#ifdef CONFIG_MTK_SRIL_SUPPORT
		__pm_wakeup_event(port->rx_wakelock, jiffies_to_msecs(HZ/2));
		spin_lock_irqsave(&port->rx_wq.lock, flags);
		wake_up_all_locked(&port->rx_wq);
		spin_unlock_irqrestore(&port->rx_wq.lock, flags);
#endif
		return -CCCI_ERR_PORT_RX_FULL;
	}
 drop:
	/* only return drop and caller do drop */
	CCCI_NORMAL_LOG(0, TAG,
		"drop on %s, len=%d\n", port->name,
		port->rx_skb_list.qlen);
	port->rx_drop_cnt++;
	return -CCCI_ERR_DROP_PACKET;
}

int port_kthread_handler(void *arg)
{
	struct port_t *port = arg;
	/* struct sched_param param = { .sched_priority = 1 }; */
	struct sk_buff *skb = NULL;
	unsigned long flags;
	int ret = 0;

	CCCI_DEBUG_LOG(0, TAG,
		"port %s's thread running\n", port->name);

	while (1) {
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq,
					!skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(0, TAG, "read on %s\n", port->name);
		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		/* 2. process port skb */
		if (port->skb_handler)
			port->skb_handler(port, skb);
	}
	return 0;
}

/*
 * This API is called by port,
 * which wants to dump message as
 * ascii string or raw binary format,
 */
void port_ch_dump(struct port_t *port, int dir, void *msg_buf, int len)
{
	if (port->flags & PORT_F_DUMP_RAW_DATA)
		port_dump_raw_data(port, dir, msg_buf, len);
	else if (port->flags & PORT_F_NET_DUMP)
		port_dump_net(port, dir, msg_buf);
	else
		port_dump_string(port, dir, msg_buf, len);
}


int port_ask_more_req_to_md(struct port_t *port)
{
	int ret = -1;
	int rx_qno = port_get_queue_no(port, IN, -1);

	if (port->flags & PORT_F_RX_FULLED)
		ret = ccci_hif_ask_more_request(port->hif_id, rx_qno);
	return ret;
}

int port_write_room_to_md(struct port_t *port)
{
	int ret = -1;
	int tx_qno;

	tx_qno = port_get_queue_no(port, OUT, -1);
	if (port->flags & PORT_F_RX_FULLED)
		ret = ccci_hif_write_room(port->hif_id, tx_qno);
	return ret;
}

int port_user_register(struct port_t *port)
{
	int rx_ch = port->rx_ch;

	if (rx_ch == CCCI_FS_RX)
		proxy_set_critical_user(port_proxyp, CRIT_USR_FS, 1);
#ifndef CONFIG_MTK_SRIL_SUPPORT
	if (rx_ch == CCCI_UART2_RX)
		proxy_set_critical_user(port_proxyp, CRIT_USR_MUXD, 1);
#endif
	if (rx_ch == CCCI_MD_LOG_RX || (rx_ch == CCCI_SMEM_CH &&
		strcmp(port->name, "ccci_ccb_dhl") == 0))
		proxy_set_critical_user(port_proxyp, CRIT_USR_MDLOG, 1);
	if (rx_ch == CCCI_UART1_RX)
		proxy_set_critical_user(port_proxyp, CRIT_USR_META, 1);

	return 0;
}
#ifdef CONFIG_MTK_SRIL_SUPPORT
EXPORT_SYMBOL(port_user_register);
#endif

int port_user_unregister(struct port_t *port)
{
	int rx_ch = port->rx_ch;

	if (rx_ch == CCCI_FS_RX)
		proxy_set_critical_user(port_proxyp, CRIT_USR_FS, 0);
	if (rx_ch == CCCI_UART2_RX)
		proxy_set_critical_user(port_proxyp, CRIT_USR_MUXD, 0);
	if (rx_ch == CCCI_MD_LOG_RX || (rx_ch == CCCI_SMEM_CH &&
		strcmp(port->name, "ccci_ccb_dhl") == 0))
		proxy_set_critical_user(port_proxyp, CRIT_USR_MDLOG, 0);
	if (rx_ch == CCCI_UART1_RX)
		proxy_set_critical_user(port_proxyp, CRIT_USR_META, 0);

	CCCI_NORMAL_LOG(0, TAG, "critical user check: 0x%x\n",
		port_proxyp->critical_user_active);
	ccci_event_log("critical user check: 0x%x\n",
		port_proxyp->critical_user_active);
	return 0;
}

int port_send_skb_to_md(struct port_t *port, struct sk_buff *skb, int blocking)
{
	int tx_qno = 0;
	int ret = 0;
	int md_state;
	md_state = ccci_fsm_get_md_state();

	if ((md_state == BOOT_WAITING_FOR_HS1 ||
		md_state == BOOT_WAITING_FOR_HS2)
		&& port->tx_ch != CCCI_FS_TX && port->tx_ch != CCCI_RPC_TX) {
		CCCI_ERROR_LOG(0, TAG,
			"port %s ch%d write fail when md_state=%d\n",
			port->name, port->tx_ch, md_state);
		return -ENODEV;
	}
	if (md_state == EXCEPTION
		&& port->tx_ch != CCCI_MD_LOG_TX
		&& port->tx_ch != CCCI_UART1_TX
	    && port->tx_ch != CCCI_FS_TX)
		return -ETXTBSY;
	if (md_state == GATED
			|| md_state == WAITING_TO_STOP
			|| md_state == INVALID)
		return -ENODEV;

	tx_qno = port_get_queue_no(port, OUT, -1);
	ret = ccci_hif_send_skb(port->hif_id, tx_qno, skb,
			port->skb_from_pool, blocking);
	if (ret == 0)
		port->tx_pkg_cnt++;
	return ret;
}
/****************************************************************************/
/*REGION: port_proxy class method implementation*/
/****************************************************************************/
static inline int proxy_check_critical_user(struct port_proxy *proxy_p)
{
	int ret = 1;

	if (proxy_get_critical_user(proxy_p, CRIT_USR_MUXD) == 0) {
		if (ccci_get_boot_mode_from_dts() == META_BOOT_ID) {
			if (proxy_get_critical_user(proxy_p,
				CRIT_USR_META) == 0) {
				CCCI_NORMAL_LOG(0, TAG,
				"ready to reset MD in META mode\n");
				ret = 0;
				goto __EXIT_FUN__;
			}
			/* this should never happen */
			CCCI_ERROR_LOG(0, TAG,
				"DHL ctrl is still open in META mode\n");
		} else {
			if (proxy_get_critical_user(proxy_p,
				CRIT_USR_MDLOG) == 0 &&
				proxy_get_critical_user(proxy_p,
				CRIT_USR_MDLOG_CTRL) == 0) {
				CCCI_NORMAL_LOG(0, TAG,
					"ready to reset MD in normal mode\n");
				ret = 0;
				goto __EXIT_FUN__;
			}
		}
	}
__EXIT_FUN__:
	return ret;
}


static inline void proxy_setup_channel_mapping(struct port_proxy *proxy_p)
{
	int i, hif;
	struct port_t *port = NULL;

	/* Init RX_CH=>port list mapping */
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++)
		INIT_LIST_HEAD(&proxy_p->rx_ch_ports[i]);
	/* Init queue_id=>port list mapping per HIF*/
	for (hif = 0; hif < ARRAY_SIZE(proxy_p->queue_ports); hif++) {
		for (i = 0; i < ARRAY_SIZE(proxy_p->queue_ports[hif]); i++)
			INIT_LIST_HEAD(&proxy_p->queue_ports[hif][i]);
	}
	/*Init EE_ports list*/
	INIT_LIST_HEAD(&proxy_p->exp_ports);
	/*setup port mapping*/
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (port->rx_ch < CCCI_MAX_CH_NUM)
			port_list[port->rx_ch] = port;
		else {
			CCCI_ERROR_LOG(0, TAG,
				"%s:%s rx_ch=%d error\n",
				__func__, port->name, port->rx_ch);
			continue;
		}
		if (port->tx_ch < CCCI_MAX_CH_NUM)
			port_list[port->tx_ch] = port;
		else {
			CCCI_ERROR_LOG(0, TAG,
				"%s:%s tx_ch=%d error\n",
				__func__, port->name, port->tx_ch);
			continue;
		}
		/*setup RX_CH=>port list mapping*/
		list_add_tail(&port->entry, &proxy_p->rx_ch_ports[port->rx_ch]);

		/* skip no data transmission port,
		 * such as CCCI_DUMMY_CH type port
		 */
		if (port->txq_index == 0xFF || port->rxq_index == 0xFF)
			continue;
		/*setup QUEUE_ID=>port list mapping*/
		list_add_tail(&port->queue_entry,
			&proxy_p->queue_ports[port->hif_id][port->rxq_index]);
		/*if port configure exception queue index, setup ee_ports*/
		if ((port->txq_exp_index & 0xF0) == 0 ||
			(port->rxq_exp_index & 0xF0) == 0)
			list_add_tail(&port->exp_entry, &proxy_p->exp_ports);
	}
	/*Dump RX_CH=> port list mapping for debugging*/
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++) {
		list_for_each_entry(port, &proxy_p->rx_ch_ports[i], entry) {
			CCCI_DEBUG_LOG(0, TAG,
				"CH%d ports:%s(%d/%d)\n",
				i, port->name, port->rx_ch, port->tx_ch);
		}
	}
	/*Dump Queue_ID=> port list mapping for debugging*/
	for (hif = 0; hif < ARRAY_SIZE(proxy_p->queue_ports); hif++) {
		for (i = 0; i < ARRAY_SIZE(proxy_p->queue_ports[hif]); i++) {
			list_for_each_entry(port,
				&proxy_p->queue_ports[hif][i], queue_entry) {
				CCCI_DEBUG_LOG(0, TAG,
					"HIF%d, Q%d, ports:%s(%d/%d)\n",
					hif, i, port->name, port->rx_ch,
					port->tx_ch);
			}
		}
	}
	/*Dump exp Queue_ID=> port list mapping for debugging*/
	list_for_each_entry(port, &proxy_p->exp_ports, exp_entry) {
		CCCI_DEBUG_LOG(0, TAG,
			"EXP: HIF%d, ports:%s(%d/%d) q(%d/%d),ee_q(%d/%d)\n",
			port->hif_id, port->name, port->rx_ch, port->tx_ch,
			port->rxq_index, port->txq_index,
			port->rxq_exp_index, port->txq_exp_index);
	}
}

static struct port_t *proxy_get_port(struct port_proxy *proxy_p, int minor,
	enum CCCI_CH ch)
{
	int i;
	struct port_t *port = NULL;

	if (proxy_p == NULL)
		return NULL;
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (minor >= 0 && port->minor == minor)
			return port;
		if (ch != CCCI_INVALID_CH_ID &&
			(port->rx_ch == ch || port->tx_ch == ch))
			return port;
	}
	return NULL;
}

/*
 * kernel inject CCCI message to modem.
 */
static inline int proxy_send_msg_to_md(struct port_proxy *proxy_p,
	int ch, unsigned int msg, unsigned int resv, int blocking)
{
	struct port_t *port = NULL;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	int ret = 0;
	int md_state;
	int qno = -1;

	if (!proxy_p) {
		CCCI_ERROR_LOG(0, TAG,
			"proxy_send_msg_to_md: proxy_p is NULL\n");
		return -CCCI_ERR_MD_NOT_READY;
	}

	md_state = ccci_fsm_get_md_state();
	if (md_state != BOOT_WAITING_FOR_HS1 &&
		md_state != BOOT_WAITING_FOR_HS2
		&& md_state != READY && md_state != EXCEPTION)
		return -CCCI_ERR_MD_NOT_READY;
	if (ch == CCCI_SYSTEM_TX && md_state != READY)
		return -CCCI_ERR_MD_NOT_READY;
	if ((msg == CCISM_SHM_INIT || msg == CCISM_SHM_INIT_DONE ||
		msg == C2K_CCISM_SHM_INIT ||
		msg == C2K_CCISM_SHM_INIT_DONE) &&
		md_state != READY) {
		return -CCCI_ERR_MD_NOT_READY;
	}
	if (ch == CCCI_SYSTEM_TX)
		port = proxy_p->sys_port;
	else if (ch == CCCI_CONTROL_TX)
		port = proxy_p->ctl_port;
	else
		port = port_get_by_channel(ch);
	if (port) {
		skb = ccci_alloc_skb(sizeof(struct ccci_header),
				port->skb_from_pool, blocking);
		if (skb) {
			ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
			ccci_h->data[0] = CCCI_MAGIC_NUM;
			ccci_h->data[1] = msg;
			ccci_h->channel = ch;
			ccci_h->reserved = resv;
			qno = port_get_queue_no(port, OUT, -1);
			ret = ccci_hif_send_skb(port->hif_id, qno, skb,
					port->skb_from_pool, blocking);
			if (ret)
				ccci_free_skb(skb);
			return ret;
		} else {
			return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
		}
	}
	return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
}

/*
 * if recv_request returns 0 or -CCCI_ERR_DROP_PACKET,
 * then it's port's duty to free the request, and caller should
 * NOT reference the request any more. but if it returns other error,
 * caller should be responsible to free the request.
 */
static inline int proxy_dispatch_recv_skb(struct port_proxy *proxy_p,
	int hif_id, struct sk_buff *skb, unsigned int flag)
{
	struct ccci_header *ccci_h = NULL;
	struct port_t *port = NULL;
	struct list_head *port_list = NULL;
	int ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
	char matched = 0;
	int md_state = ccci_fsm_get_md_state();
	int channel = CCCI_INVALID_CH_ID;

	if (unlikely(!skb)) {
		ret = -CCCI_ERR_INVALID_PARAM;
		goto err_exit;
	}

	if (flag == NORMAL_DATA) {
		ccci_h = (struct ccci_header *)skb->data;
		channel = ccci_h->channel;
	} else {
		WARN_ON(1);
	}

	if (unlikely(channel >= CCCI_MAX_CH_NUM ||
		channel == CCCI_INVALID_CH_ID)) {
		ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
		goto err_exit;
	}
	if (unlikely(md_state == GATED || md_state == INVALID)) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		goto err_exit;
	}

	port_list = &proxy_p->rx_ch_ports[channel];
	list_for_each_entry(port, port_list, entry) {
		/*
		 * multi-cast is not supported, because one port may
		 * freed or modified this request
		 * before another port can process it. but we still can
		 * use req->state to achive some
		 * kind of multi-cast if needed.
		 */
		matched = (hif_id == port->hif_id) && port->ops &&
			((port->ops->recv_match == NULL) ?
			(channel == port->rx_ch) :
			port->ops->recv_match(port, skb));
		if (matched) {
			if (likely(skb && port->ops && port->ops->recv_skb)) {
				ret = port->ops->recv_skb(port, skb);
			} else {
				CCCI_ERROR_LOG(0, TAG,
					"port->ops->recv_skb is null\n");
				ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
				goto err_exit;
			}
			if (ret == -CCCI_ERR_PORT_RX_FULL)
				port->rx_busy_count++;
			break;
		}
	}

 err_exit:
	if (ret < 0 && ret != -CCCI_ERR_PORT_RX_FULL) {
		if (channel == CCCI_CONTROL_RX)
			CCCI_ERROR_LOG(0, CORE,
				"drop on channel %d, ret %d\n", channel, ret);
		if (skb)
			ccci_free_skb(skb);
		ret = -CCCI_ERR_DROP_PACKET;
	}

	return ret;
}

static inline void proxy_dispatch_queue_status(struct port_proxy *proxy_p,
	int hif, int qno, int dir, unsigned int state)
{
	struct port_t *port = NULL;
	int match = 0;

	if (hif < 0 || hif >= CCCI_HIF_NUM || qno >= MAX_QUEUE_NUM || qno < 0) {
		CCCI_ERROR_LOG(0, CORE,
			"%s:hif=%d or qno=%d is inval\n", __func__, hif, qno);
		return;
	}
	/*EE then notify EE port*/
	if (unlikely(ccci_fsm_get_md_state()
		== EXCEPTION)) {
		list_for_each_entry(port,
		&proxy_p->exp_ports, exp_entry) {
			match = (port->hif_id == hif);
			if (dir == OUT)
				match =	(qno == port->txq_exp_index);
			else
				match = (qno == port->rxq_exp_index);
			if (match && port->ops && port->ops->queue_state_notify)
				port->ops->queue_state_notify(port, dir, qno, state);
		}
		return;
	}

	list_for_each_entry(port,
		&proxy_p->queue_ports[hif][qno], queue_entry) {
		match = (port->hif_id == hif);
		/* consider network data/ack queue design */
		if (dir == OUT)
			match = qno == port->txq_index
				|| qno == (port->txq_exp_index & 0x0F);
		else
			match = qno == port->rxq_index;
		if (match && port->ops && port->ops->queue_state_notify) {
			port->ops->queue_state_notify(port, dir, qno, state);
		}
	}
}

static inline void proxy_dispatch_md_status(struct port_proxy *proxy_p,
	unsigned int state)
{
	int i;
	struct port_t *port = NULL;

	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if ((state == GATED) && (port->flags &
			PORT_F_CH_TRAFFIC)) {
			atomic_set(&port->rx_pkg_cnt, 0);
			port->rx_drop_cnt = 0;
			port->tx_pkg_cnt = 0;
		}
		if (port->ops && port->ops->md_state_notify)
			port->ops->md_state_notify(port, state);
	}
}

static inline void proxy_dump_status(struct port_proxy *proxy_p)
{
	struct port_t *port = NULL;
	unsigned int port_full_sum = 0;
	unsigned int i, full_len;
	/* the worst is all port full */
	char port_full[352];
	int ret = 0;

	if (!proxy_p || !proxy_p->ports) {
		CCCI_ERROR_LOG(0, TAG, "proxy_p or proxy_p->ports is NULL\n");
		return;
	}

	full_len = sizeof(port_full);
	memset(port_full, 0, full_len);
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (port->flags & PORT_F_RX_FULLED) {
			port_full_sum++;
			ret += scnprintf(port_full + ret, full_len - ret, "%d ",
				port->rx_ch);
			if (ret >= full_len)
				break;
		}
		if (port->tx_busy_count != 0 || port->rx_busy_count != 0) {
			CCCI_REPEAT_LOG(0, TAG, "port %s busy count %d/%d\n", port->name,
				port->tx_busy_count, port->rx_busy_count);
			port->tx_busy_count = 0;
			port->rx_busy_count = 0;
		}
		if (port->ops && port->ops->dump_info)
			port->ops->dump_info(port, 0);
	}
	if (port_full_sum)
		CCCI_ERROR_LOG(0, TAG, "port_full sum = %u, rx_ch: %s\n",
			port_full_sum, port_full);
}

static inline int proxy_register_char_dev(struct port_proxy *proxy_p)
{
	int ret = 0;
	dev_t dev = 0;

	if (proxy_p->major) {
		dev = MKDEV(proxy_p->major, proxy_p->minor_base);
		ret = register_chrdev_region(dev, 120, CCCI_DEV_NAME);
	} else {
		ret = alloc_chrdev_region(&dev, proxy_p->minor_base,
				120, CCCI_DEV_NAME);
		if (ret)
			CCCI_ERROR_LOG(0, CHAR,
				"alloc_chrdev_region fail,ret=%d\n", ret);
		proxy_p->major = MAJOR(dev);
	}
	return ret;
}

static inline void proxy_init_all_ports(struct port_proxy *proxy_p)
{
	int i;
	struct port_t *port = NULL;

	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++)
		INIT_LIST_HEAD(&proxy_p->rx_ch_ports[i]);

	/* init port */
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		port_struct_init(port, proxy_p);
		if (port->tx_ch == CCCI_SYSTEM_TX)
			proxy_p->sys_port = port;
		if (port->tx_ch == CCCI_CONTROL_TX)
			proxy_p->ctl_port = port;
		port->major = proxy_p->major;
		port->minor_base = proxy_p->minor_base;
		if (port->ops && port->ops->init)
			port->ops->init(port);
		spin_lock_init(&port->flag_lock);
	}

	proxy_setup_channel_mapping(proxy_p);
}

static inline void proxy_set_traffic_flag(struct port_proxy *proxy_p,
	unsigned int dump_flag)
{
	int idx;
	struct port_t *port = NULL;

	proxy_p->traffic_dump_flag = dump_flag;
	CCCI_NORMAL_LOG(0, TAG,
			 "%s: 0x%x\n", __func__, proxy_p->traffic_dump_flag);
	for (idx = 0; idx < proxy_p->port_number; idx++) {
		port = proxy_p->ports + idx;
		/*clear traffic & dump flag*/
		port->flags &= ~(PORT_F_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);

		/*set RILD related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_RILD)) {
			if (port->rx_ch == CCCI_UART2_RX ||
				port->rx_ch == CCCI_C2K_AT ||
				port->rx_ch == CCCI_C2K_AT2 ||
				port->rx_ch == CCCI_C2K_AT3 ||
				port->rx_ch == CCCI_C2K_AT4 ||
				port->rx_ch == CCCI_C2K_AT5 ||
				port->rx_ch == CCCI_C2K_AT6 ||
				port->rx_ch == CCCI_C2K_AT7 ||
				port->rx_ch == CCCI_C2K_AT8)
				port->flags |= PORT_F_CH_TRAFFIC;
		}
		/*set AUDIO related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_AUDIO)) {
			if (port->rx_ch == CCCI_PCM_RX)
				port->flags |= (PORT_F_CH_TRAFFIC
					| PORT_F_DUMP_RAW_DATA);
		}
		/*set IMS related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_IMS)) {
			if (port->rx_ch == CCCI_IMSV_DL ||
				port->rx_ch == CCCI_IMSC_DL ||
				port->rx_ch == CCCI_IMSA_DL ||
				port->rx_ch == CCCI_IMSA_DL ||
				port->rx_ch == CCCI_IMSEM_DL)
				port->flags |= (PORT_F_CH_TRAFFIC
					| PORT_F_DUMP_RAW_DATA);
		}
	}
}

static inline struct port_proxy *proxy_alloc(void)
{
	int ret = 0;
	struct port_proxy *proxy_p;

	/* Allocate port_proxy obj and set all member zero */
	proxy_p = kzalloc(sizeof(struct port_proxy), GFP_KERNEL);
	if (proxy_p == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc port_proxy fail\n", __func__);
		return NULL;
	}

	ret = proxy_register_char_dev(proxy_p);
	if (ret)
		goto EXIT_FUN;
	proxy_p->port_number =
		port_get_cfg(&proxy_p->ports);
	if (proxy_p->port_number > 0 && proxy_p->ports)
		proxy_init_all_ports(proxy_p);
	else
		ret = -1;

EXIT_FUN:
	if (ret) {
		kfree(proxy_p);
		proxy_p = NULL;
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get md port config fail,ret=%d\n", __func__, ret);
	}
	return proxy_p;
};

struct port_t *port_get_by_minor(int minor)
{
	return proxy_get_port(port_proxyp, minor,
			CCCI_INVALID_CH_ID);
}
#ifdef CONFIG_MTK_SRIL_SUPPORT
EXPORT_SYMBOL(port_get_by_minor);
#endif
struct port_t *port_get_by_channel(enum CCCI_CH ch)
{
	return proxy_get_port(port_proxyp, -1, ch);
}
struct port_t *port_get_by_node(int major, int minor)
{
	if (port_proxyp && port_proxyp->major == major)
		return proxy_get_port(port_proxyp, minor,
				CCCI_INVALID_CH_ID);

	return NULL;
}
int port_send_msg_to_md(struct port_t *port, unsigned int msg,
	unsigned int resv, int blocking)
{
	int ch = port->tx_ch;

	return proxy_send_msg_to_md(port->port_proxy, ch,
			msg, resv, blocking);
}

/****************************************************************************/
/* Extern API implement for none port related module */
/****************************************************************************/
/*
 * This API is called by ccci_modem,
 * and used to create all ccci port instance for per modem
 */

static int ccci_lp_mem_open(struct inode *inode, struct file *file)
{
	struct ccci_proc_user *proc_user;

	proc_user = kzalloc(sizeof(struct ccci_proc_user), GFP_KERNEL);
	if (!proc_user) {
		CCCI_ERROR_LOG(-1, TAG, "fail to open ccci_lp_mem\n");
		return -1;
	}

	file->private_data = proc_user;
	proc_user->busy = 0;
	nonseekable_open(inode, file);

	return 0;
}

static ssize_t ccci_lp_mem_read(struct file *file, char __user *buf,
				size_t size, loff_t *ppos)
{
		int proc_size = 0, read_len = 0, has_closed = 0;
		unsigned long flags;
		void __iomem *user_start_addr = NULL;
		struct ccci_proc_user *proc_user = file->private_data;
		struct ccci_smem_region *ccci_user_region =
			ccci_md_get_smem_by_user_id(SMEM_USER_LOW_POWER);

		spin_lock_irqsave(&file_lock, flags);
		proc_user = (struct ccci_proc_user *)file->private_data;
		if (proc_user == NULL)
			has_closed = 1;
		else
			proc_user->busy = 1;
		spin_unlock_irqrestore(&file_lock, flags);

		if (has_closed) {
			CCCI_ERROR_LOG(-1, TAG,
				"ccci_lp_proc has been already closed\n");
			return 0;
		}
		if (!ccci_user_region) {
			CCCI_ERROR_LOG(-1, TAG, "not found Low power region\n");
			proc_user->busy = 0;
			return -1;
		}

		user_start_addr = ccci_user_region->base_ap_view_vir;
		proc_size = ccci_user_region->size;
		if (proc_user->left_len)
			read_len = size > proc_user->left_len ?
					proc_user->left_len : size;
		else
			read_len = size > proc_size ? proc_size : size;
		proc_user->curr_addr = proc_user->curr_addr ?
				proc_user->curr_addr : user_start_addr;

		if (proc_user->curr_addr < user_start_addr + proc_size) {
			CCCI_ERROR_LOG(-1, TAG, "copy to user\n");
			if (copy_to_user(buf, proc_user->curr_addr, read_len)) {
				CCCI_ERROR_LOG(-1, TAG,
				"read ccci_lp_mem fail, size %zu\n", size);
				proc_user->busy = 0;
				return -EFAULT;
			}
			proc_user->curr_addr = proc_user->curr_addr + read_len;
			proc_user->left_len = proc_size - read_len;
		} else {
			proc_user->busy = 0;
			return 0;
		}
		proc_user->busy = 0;

		return read_len;
}

static int ccci_lp_mem_close(struct inode *inode, struct file *file)
{
	int need_wait = 0;
	unsigned long flags;
	struct ccci_proc_user *proc_user = file->private_data;

	if (proc_user == NULL)
		return -1;

	do {
		spin_lock_irqsave(&file_lock, flags);
		if (proc_user->busy) {
			need_wait = 1;
		} else {
			need_wait = 0;
			file->private_data = NULL;
		}
		spin_unlock_irqrestore(&file_lock, flags);
		if (need_wait)
			msleep(20);
	} while (need_wait);
	if (proc_user != NULL)
		kfree(proc_user);

	return 0;
}

const struct proc_ops ccci_dbm_ops = {
	.proc_open = ccci_lp_mem_open,
	.proc_read = ccci_lp_mem_read,
	.proc_release = ccci_lp_mem_close,
};

static void ccci_proc_init(void)
{
	struct proc_dir_entry *ccci_dbm_proc;

	ccci_dbm_proc = proc_create("ccci_lp_mem", 0440, NULL, &ccci_dbm_ops);
	if (ccci_dbm_proc == NULL)
		CCCI_ERROR_LOG(-1, TAG, "fail to create ccci dbm proc\n");
	spin_lock_init(&file_lock);
	return;

}

int ccci_port_init(void)
{
	int ret = 0;
	struct device_node *node = NULL;

	ccci_proc_init();

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mddriver");
	if (node)
		ret = of_property_read_u32(node,
			"mediatek,md-generation", &port_md_gen);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, CHAR, "%s:get md_gen from dts fail\n",
			__func__);
		return -1;
	}

	CCCI_NORMAL_LOG(0, TAG, "%s: port_md_gen=%d\n",
		__func__, port_md_gen);

	port_proxyp = proxy_alloc();
	if (port_proxyp == NULL) {
		CCCI_ERROR_LOG(0, TAG, "alloc port_proxy fail\n");
		return -1;
	}
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SPMI_MTK_PMIF)
	/* register callback func for spmi */
	if (register_spmi_md_force_assert == NULL) {
		register_spmi_md_force_assert = exec_ccci_kern_func;
		CCCI_NORMAL_LOG(0, TAG,
			"%s: hook register_spmi_md_force_assert done\n", __func__);
	}
#endif

	return 0;
}

/*
 * This API is called by ccci_fsm,
 * and used to dump all ccci port status for debugging
 */
void ccci_port_dump_status(void)
{
	proxy_dump_status(port_proxyp);
}
EXPORT_SYMBOL(ccci_port_dump_status);

static inline void user_broadcast_wrapper(unsigned int state)
{
	int mapped_event = -1;

	switch (state) {
	case GATED:
		mapped_event = MD_STA_EV_STOP;
		break;
	case BOOT_WAITING_FOR_HS1:
		break;
	case BOOT_WAITING_FOR_HS2:
		mapped_event = MD_STA_EV_HS1;
		break;
	case READY:
		mapped_event = MD_STA_EV_READY;
		break;
	case EXCEPTION:
		mapped_event = MD_STA_EV_EXCEPTION;
		break;
	case RESET:
		break;
	case WAITING_TO_STOP:
		break;
	default:
		break;
	}

	if (mapped_event >= 0)
		inject_md_status_event(mapped_event, NULL);
}

/*
 * This API is called by ccci_fsm,
 * and used to dispatch modem status for all ports,
 * which want to know md state transition.
 */
void ccci_port_md_status_notify(unsigned int state)
{
	proxy_dispatch_md_status(port_proxyp, (unsigned int)state);
	user_broadcast_wrapper(state);
}


/*
 * This API is called by HIF,
 * and used to dispatch Queue status for all ports,
 * which is mounted on the hif_id & qno
 */
void ccci_port_queue_status_notify(int hif_id, int qno,
	int dir, unsigned int state)
{
	CHECK_HIF_ID(hif_id);
	CHECK_QUEUE_ID(qno);

	proxy_dispatch_queue_status(port_proxyp, hif_id, qno,
		dir, (unsigned int)state);
}
EXPORT_SYMBOL(ccci_port_queue_status_notify);

/*
 * This API is called by HIF,
 * and used to dispatch RX data for related port
 */
int ccci_port_recv_skb(int hif_id, struct sk_buff *skb,
	unsigned int flag)
{
	return proxy_dispatch_recv_skb(port_proxyp, hif_id, skb, flag);
}
EXPORT_SYMBOL(ccci_port_recv_skb);

/*
 * This API is called by ccci fsm,
 * and used to check whether all critical user exited.
 */
int ccci_port_check_critical_user(void)
{
	return proxy_check_critical_user(port_proxyp);
}

/*
 * This API is called by ccci fsm,
 * and used to check critical user only ccci_fsd exited.
 */
int ccci_port_critical_user_only_fsd(void)
{
	if (!port_proxyp)
		return 0;

	if (port_proxyp->critical_user_active == (1 << CRIT_USR_FS))
		return 1;

	return 0;
}

/*
 * This API is called by ccci fsm,
 * and used to get critical user status.
 */
int ccci_port_get_critical_user(unsigned int user_id)
{
	return proxy_get_critical_user(port_proxyp, user_id);
}

/*
 * This API is called by ccci fsm,
 * and used to send a ccci msg for modem.
 */
int ccci_port_send_msg_to_md(int ch, unsigned int msg,
	unsigned int resv, int blocking)
{
	return proxy_send_msg_to_md(port_proxyp, ch, msg, resv, blocking);
}
EXPORT_SYMBOL(ccci_port_send_msg_to_md);
/*
 * This API is called by ccci fsm,
 * and used to set port traffic flag to catch traffic history on
 * some important channel.
 * port traffic use md_boot_data[MD_CFG_DUMP_FLAG] =
 * 0x6000_000x as port dump flag
 */
void ccci_port_set_traffic_flag(unsigned int dump_flag)
{
	proxy_set_traffic_flag(port_proxyp, dump_flag);
}

#if IS_ENABLED(CONFIG_MTK_ECCCI_C2K_USB)
/* only md3 can usb bypass */
int modem_dtr_set(int on, int low_latency)
{
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int ret = 0;

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_IND_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_IND_MSG & 0xFF;
	c2k_ctl_msg.option = 0;
	if (on)
		c2k_ctl_msg.option |= 0x04;
	else
		c2k_ctl_msg.option &= 0xFB;

	CCCI_NORMAL_LOG(0, TAG, "usb bypass dtr set(%d)(0x%x)\n",
		on, (u32) (*((u32 *)&c2k_ctl_msg)));
	ccci_port_send_msg_to_md(CCCI_SYSTEM_TX, C2K_PPP_LINE_STATUS,
		(u32) (*((u32 *)&c2k_ctl_msg)), 1);

	return ret;
}
EXPORT_SYMBOL(modem_dtr_set);

int modem_dcd_state(void)
{
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int dcd_state = 0, ret = 0;
	struct ccci_per_md *per_md_data = NULL;

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_QUERY_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_QUERY_MSG & 0xFF;
	c2k_ctl_msg.option = 0;

	ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
			C2K_PPP_LINE_STATUS, // md user is: stun wu
			(u32) (*((u32 *)&c2k_ctl_msg)), 1);

	CCCI_NORMAL_LOG(0, TAG,
		"usb bypass query state(0x%x)\n",
		(u32) (*((u32 *)&c2k_ctl_msg)));

	per_md_data = ccci_get_per_md_data();

	if (per_md_data == NULL)
		return -1;
	if (ret == -CCCI_ERR_MD_NOT_READY)
		dcd_state = 0;
	else {
		msleep(20);
		dcd_state = per_md_data->dtr_state;
	}
	return dcd_state;
}
EXPORT_SYMBOL(modem_dcd_state);

int ccci_c2k_rawbulk_intercept(int ch_id, unsigned int interception)
{
	int ret = 0;
	struct port_t *port = NULL;
	struct list_head *port_list = NULL;
	char matched = 0;
	int ch_id_tx, ch_id_rx = 0;

	struct ccci_per_md *per_md_data = ccci_get_per_md_data();

	/* USB bypass's channel id offset,
	 * please refer to viatel_rawbulk.h
	 */
	if (ch_id >= FS_CH_C2K)
		ch_id += 2;
	else
		ch_id += 1;

	/*only data and log channel are legal*/
	if (ch_id == DATA_PPP_CH_C2K) {
		ch_id_tx = CCCI_C2K_PPP_TX;
		ch_id_rx = CCCI_C2K_PPP_RX;

	} else if (ch_id == MDLOG_CH_C2K) {
		ch_id_tx = CCCI_MD_LOG_TX;
		ch_id_rx = CCCI_MD_LOG_RX;
	} else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(0, TAG,
			"Err: wrong ch_id(%d) from usb bypass\n", ch_id);
		return ret;
	}

	/*use rx channel to find port*/
	port_list = &port_proxyp->rx_ch_ports[ch_id_rx];
	list_for_each_entry(port, port_list, entry) {
		matched = (ch_id_tx == port->tx_ch);
		if (matched) {
			port->interception = !!interception;
			if (port->interception)
				atomic_inc(&port->usage_cnt);
			else
				atomic_dec(&port->usage_cnt);
			if (ch_id == DATA_PPP_CH_C2K)
				per_md_data->data_usb_bypass = !!interception;
			ret = 0;
			CCCI_NORMAL_LOG(0, TAG,
				"port(%s) ch(%d) interception(%d) set\n",
				port->name, ch_id_tx, interception);
		}
	}
	if (!matched) {
		ret = -ENODEV;
		CCCI_ERROR_LOG(0, TAG,
			"Err: no port found when setting interception(%d,%d)\n",
			ch_id_tx, interception);
	}

	return ret;
}
EXPORT_SYMBOL(ccci_c2k_rawbulk_intercept);

int ccci_c2k_buffer_push(int ch_id, void *buf, int count)
{
	int ret = 0;
	struct port_t *port = NULL;
	struct list_head *port_list = NULL;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	char matched = 0;
	size_t actual_count = 0;
	int ch_id_tx, ch_id_rx = 0;
	/* usb will call this routine in ISR, so we cannot schedule */
	unsigned char blk1 = 0;
	/* non-blocking for all request from USB */
	unsigned char blk2 = 0;

	/* USB bypass's channel id offset, please refer to viatel_rawbulk.h */
	if (ch_id >= FS_CH_C2K)
		ch_id += 2;
	else
		ch_id += 1;

	/* only data and log channel are legal */
	if (ch_id == DATA_PPP_CH_C2K) {
		ch_id_tx = CCCI_C2K_PPP_TX;
		ch_id_rx = CCCI_C2K_PPP_RX;
	} else if (ch_id == MDLOG_CH_C2K) {
		ch_id_tx = CCCI_MD_LOG_TX;
		ch_id_rx = CCCI_MD_LOG_RX;
	} else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(0, TAG,
			"Err: wrong ch_id(%d) from usb bypass\n", ch_id);
		return ret;
	}

	CCCI_NORMAL_LOG(0, TAG,
		"data from usb bypass (ch%d)(%d)\n", ch_id_tx, count);

	actual_count = count > CCCI_MTU ? CCCI_MTU : count;

	port_list = &port_proxyp->rx_ch_ports[ch_id_rx];
	list_for_each_entry(port, port_list, entry) {
		matched = (ch_id_tx == port->tx_ch);
		if (matched) {
			skb = ccci_alloc_skb(actual_count,
					port->skb_from_pool, blk1);
			if (skb) {
				ccci_h = (struct ccci_header *)skb_put(skb,
				sizeof(struct ccci_header));
				ccci_h->data[0] = 0;
				ccci_h->data[1] = actual_count +
				sizeof(struct ccci_header);
				ccci_h->channel = port->tx_ch;
				ccci_h->reserved = 0;

				memcpy(skb_put(skb, actual_count), buf,
				actual_count);

				/*
				 * for md3, ccci_h->channel will probably change
				 * after call send_skb,
				 * because md3's channel mapping.
				 * do NOT reference request after called this,
				 * modem may have freed it, unless you get
				 * -EBUSY
				 */
				ret = port_send_skb_to_md(port, skb, blk2);

				if (ret) {
					if (ret == -EBUSY && !blk2)
						ret = -EAGAIN;
					goto push_err_out;
				}
				return actual_count;
push_err_out:
				ccci_free_skb(skb);
				return ret;
			}
			/* consider this case as non-blocking */
			return -ENOMEM;
		}
	}
	return -ENODEV;
}
EXPORT_SYMBOL(ccci_c2k_buffer_push);

#endif

static void receive_wakeup_src_notify(char *buf, unsigned int len)
{
	int tmp_data = 0;

	if (len == 0) {
		/* before spm add MD_WAKEUP_SOURCE parameter. */
		if (port_md_gen < 6295)
			ccci_hif_set_wakeup_src(CLDMA_HIF_ID, 1);
		else
			ccci_hif_set_wakeup_src(DPMAIF_HIF_ID, 1);

		ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);

		return;
	}

	/* after spm add MD_WAKEUP_SOURCE parameter. */
	if (len > sizeof(tmp_data))
		len = sizeof(tmp_data);
	memcpy((void *)&tmp_data, buf, len);
	switch (tmp_data) {
	case WAKE_SRC_HIF_CCIF0:
		ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
		break;
	case WAKE_SRC_HIF_CLDMA:
		ccci_hif_set_wakeup_src(CLDMA_HIF_ID, 1);
		break;
	case WAKE_SRC_HIF_DPMAIF:
		ccci_hif_set_wakeup_src(DPMAIF_HIF_ID, 1);
		break;
	default:
		break;
	};
}

int exec_ccci_kern_func(unsigned int id, char *buf, unsigned int len)
{
	int ret = 0;
	int tmp_data;

	if (!get_modem_is_enabled()) {
		CCCI_ERROR_LOG(0, CORE,
			"wrong MD ID from %ps for %d\n",
			__builtin_return_address(0), id);
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	}

	CCCI_DEBUG_LOG(0, CORE, "%ps execute function %d\n",
		__builtin_return_address(0), id);
	switch (id) {
	case ID_GET_MD_WAKEUP_SRC:
		receive_wakeup_src_notify(buf, len);
		break;
	case ID_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(0, CORE, "Force MD assert called by %s\n",
			current->comm);
		ret = ccci_md_force_assert(MD_FORCE_ASSERT_BY_USER_TRIGGER, NULL, 0);
		break;
	case ID_SPMI_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(0, CORE, "Force MD assert called by SPMI\n");
		ret = ccci_md_force_assert(MD_FORCE_ASSERT_BY_SPMI_TRIGGER, NULL, 0);
		break;
	case ID_MD_MPU_ASSERT:
		if (buf != NULL && strlen(buf)) {
			CCCI_NORMAL_LOG(0, CORE,
				"Force MD assert(MPU) called by %s\n",
				current->comm);
			ret = ccci_md_force_assert(MD_FORCE_ASSERT_BY_AP_MPU, buf, len);

		} else
			CCCI_NORMAL_LOG(0, CORE,
				"MD MPU API called by %s\n",
				current->comm);
		break;
	case ID_PAUSE_LTE:
		/*
		 * MD booting/flight mode/exception mode: return >0 to DVFS.
		 * MD ready: return 0 if message delivered,
		 * return <0 if get error.
		 * DVFS will call this API with IRQ disabled.
		 */
		if (ccci_fsm_get_md_state() != READY)
			ret = 1;
		else {
			ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
					MD_PAUSE_LTE, *((int *)buf), 1);
			if (ret == -CCCI_ERR_MD_NOT_READY ||
				ret == -CCCI_ERR_HIF_NOT_POWER_ON)
				ret = 1;
		}
		break;
	case ID_GET_MD_STATE:
		ret = ccci_fsm_get_md_state_for_user();
		break;
		/* used for throttling feature - start */
	case ID_THROTTLING_CFG:
		ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
				MD_THROTTLING,
				*((int *)buf), 1);
		break;
		/* used for throttling feature - end */
	case ID_UPDATE_TX_POWER:
		{
			unsigned int msg_id = MD_SW_MD1_TX_POWER;
			unsigned int mode = *((unsigned int *)buf);

			ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
				msg_id, mode, 0);
			if (ret != 0)
				CCCI_ERROR_LOG(0, TAG, "send msg id error %d\n", ret);
#ifdef MTK_TC10_FEATURE_CARKIT
			ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
				MD_CARKIT_STATUS, mode, 0);
			if (ret != 0)
				CCCI_ERROR_LOG(0, TAG, "send carkit status error %d\n", ret);
#endif

		}
		break;
	case ID_DUMP_MD_SLEEP_MODE:
		{
			struct ccci_smem_region *low_pwr =
				ccci_md_get_smem_by_user_id(SMEM_USER_RAW_DBM);

			CCCI_MEM_LOG_TAG(0, TAG, "Dump MD SLP registers\n");
			if (low_pwr == NULL)
				return -1;
			ccci_util_cmpt_mem_dump(CCCI_DUMP_MEM_DUMP,
				low_pwr->base_ap_view_vir, low_pwr->size);
		}
		//ccci_md_dump_info(DUMP_FLAG_SMEM_MDSLP, NULL, 0);
		break;
	case ID_PMIC_INTR:
		ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
				PMIC_INTR_MODEM_BUCK_OC,
				*((int *)buf), 1);
		break;
	case ID_LWA_CONTROL_MSG:
		ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
			LWA_CONTROL_MSG, *((int *)buf), 1);
		break;
	case MD_TX_POWER:
	case MD_RF_MAX_TEMPERATURE_SUB6:
	case MD_RF_ALL_TEMPERATURE_MMW:
		ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX, id, 0, 0);
		break;
	case MD_DISPLAY_DYNAMIC_MIPI:
		fallthrough;
	case MD_NR_BAND_ACTIVATE_INFO:
		tmp_data = 0;
		len = (len < sizeof(tmp_data)) ? len : sizeof(tmp_data);
		memcpy((void *)&tmp_data, buf, len);
		ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX, id, tmp_data, 0);
		break;
	case ID_GET_MD_BOOT_CNT:
		ret = ccci_get_md_boot_count();
		break;
	default:
		ret = -CCCI_ERR_FUNC_ID_ERROR;
		break;
	};
	return ret;
}
EXPORT_SYMBOL(exec_ccci_kern_func);

