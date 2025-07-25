// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "gl_ics.h"

#if ((CFG_SUPPORT_ICS == 1) || (CFG_SUPPORT_PHY_ICS == 1))

/* global variable of ics log */
static struct ics_dev *gIcsDev[CFG_MAX_WLAN_DEVICES];
#if CFG_SUPPORT_MULTI_CARD
/* global variable of ics log name */
static uint8_t aucIcsDevName[CFG_MAX_WLAN_DEVICES][20];
#endif

u_int8_t ics_get_onoff(struct GLUE_INFO *prGlueInfo)
{
	struct ICS_LOG_CACHE *prLogCache;

	if (!prGlueInfo || prGlueInfo->u4DevNum >= CFG_MAX_WLAN_DEVICES ||
		!gIcsDev[prGlueInfo->u4DevNum]) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return FALSE;
	}

	prLogCache = &gIcsDev[prGlueInfo->u4DevNum]->rIcsLogCache;
	return prLogCache->fgOnOff;
}

static u_int8_t ics_set_onoff(struct GLUE_INFO *prGlueInfo, int cmd, int value)
{
	struct ICS_LOG_CACHE *prLogCache;

	if (!prGlueInfo || prGlueInfo->u4DevNum >= CFG_MAX_WLAN_DEVICES ||
		!gIcsDev[prGlueInfo->u4DevNum]) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return FALSE;
	}

	if (cmd != ICS_LOG_CMD_ON_OFF && cmd != ICS_LOG_CMD_SET_LEVEL) {
		DBGLOG(ICS, DEBUG, "Unknown cmd [Cmd:Value]=[%d:%d]\n",
					cmd, value);
		return FALSE;
	}

	prLogCache = &gIcsDev[prGlueInfo->u4DevNum]->rIcsLogCache;
	/*
	 * Special code that matches App behavior:
	 * 1. set ics log level
	 * 2. set on/off (if fwlog on, then icslog also get on)
	 */
	if (cmd == ICS_LOG_CMD_ON_OFF) {
		prLogCache->fgOnOff = (value == 1) ? TRUE : FALSE;
		if (prLogCache->ucLevel == ENUM_ICS_LOG_LEVEL_DISABLE) {
			if (prLogCache->fgOnOff == TRUE)
				DBGLOG(ICS, TRACE, "IcsLv is disable!!!\n");
			prLogCache->fgOnOff = FALSE;
		}
	} else if (cmd == ICS_LOG_CMD_SET_LEVEL) {
		prLogCache->ucLevel = value;
		if (prLogCache->ucLevel == ENUM_ICS_LOG_LEVEL_DISABLE) {
			DBGLOG(ICS, TRACE, "IcsLv set to disable.\n");
			prLogCache->fgOnOff = FALSE;
		} else {
			DBGLOG(ICS, TRACE, "IcsLv set to MAC ICS.\n");
			prLogCache->fgOnOff = TRUE;
		}
	}

	DBGLOG(ICS, DEBUG, "[Cmd:Value]=[%d:%d] IcsLog[Lv:OnOff]=[%u:%u]\n",
		cmd, value, prLogCache->ucLevel, prLogCache->fgOnOff);

	return TRUE;
}

void ics_log_event_notification(struct GLUE_INFO *prGlueInfo,
	int cmd, int value, u_int8_t isOid)
{
	struct ADAPTER *prAdapter = NULL;
	struct PARAM_CUSTOM_ICS_SNIFFER_INFO_STRUCT rSniffer = {0};
	uint32_t u4BufLen = 0;
	uint32_t rStatus;
	uint8_t ucBand;

	if (!prGlueInfo || !ics_set_onoff(prGlueInfo, cmd, value))
		return;

	if (kalIsHalted(prGlueInfo)) {
		DBGLOG(ICS, DEBUG, "device not ready return");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter) {
		DBGLOG(INIT, INFO, "prAdapter is NULL return");
		return;
	}

	kalMemZero(&rSniffer,
		sizeof(struct PARAM_CUSTOM_ICS_SNIFFER_INFO_STRUCT));
	rSniffer.ucModule = 2;
	rSniffer.ucAction = ics_get_onoff(prGlueInfo);
	rSniffer.ucCondition[0] = 2;

	/* Enable/Disable ICS for all band */
	for (ucBand = ENUM_BAND_0; ucBand < ENUM_BAND_NUM; ucBand++) {
		rSniffer.ucCondition[1] = ucBand;
		if (isOid == TRUE) {
			rStatus = kalIoctl(prGlueInfo, wlanoidSetIcsSniffer,
				&rSniffer, sizeof(rSniffer), &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(ICS, ERROR,
					"wlanoidSetIcsSniffer Band[%u] failed\n",
					ucBand);
		} else {
			wlanSetIcsSniffer(prAdapter, &rSniffer,
				sizeof(rSniffer), &u4BufLen, FALSE);
		}
	}
}

/* ring related function */
static int ics_ring_init(struct ics_ring *iRing, size_t size)
{
	int ret = 0;
	void *pBuffer = NULL;

	if (unlikely(iRing->ring_base)) {
		DBGLOG(ICS, ERROR, "ics_ring has init?\n");
		ret = -EPERM;
	} else {
		pBuffer = kmalloc(size, GFP_KERNEL);
		if (pBuffer == NULL && size > PAGE_SIZE)
			pBuffer = vmalloc(size);

		if (!pBuffer)
			ret = -ENOMEM;
		else {
			iRing->ring_base = pBuffer;
			iRing->ring_size = size;
			WLAN_RING_INIT(
				iRing->ring_base,
				iRing->ring_size,
				0,
				0,
				&iRing->ring_cache);
		}
	}

	return ret;
}

static ssize_t ics_ring_read(
	struct ics_ring *iRing,
	char __user *buf,
	size_t count)
{
	ssize_t read = 0;
	struct wlan_ring_segment ring_seg;
	struct wlan_ring *ring = &iRing->ring_cache;
	ssize_t left_to_read = 0;
	int ret;

	if (likely(iRing->ring_base)) {
		left_to_read = count < WLAN_RING_SIZE(ring)
				? count : WLAN_RING_SIZE(ring);
		if (WLAN_RING_EMPTY(ring) ||
			!WLAN_RING_READ_PREPARE(left_to_read,
				&ring_seg, ring)) {
			DBGLOG(ICS, TEMP,
				"no data/taken by other reader?\n");
			goto return_fn;
		}

		WLAN_RING_READ_FOR_EACH(left_to_read, ring_seg, ring) {
			ret = copy_to_user(buf + read, ring_seg.ring_pt,
				ring_seg.sz);
			if (ret)
				goto return_fn;
			left_to_read -= ring_seg.sz;
			read += ring_seg.sz;
		}
	} else {
		DBGLOG(ICS, ERROR, "ics_ring not init yet\n");
		read = -EPERM;
	}

return_fn:
	DBGLOG(ICS, TEMP, "[Done] read:%ld left:%ld\n", read,
		left_to_read);
	return read;
}

static ssize_t ics_ring_write(struct ics_ring *iRing, char *buf,
	size_t count)
{
	ssize_t written = 0;
	struct wlan_ring_segment ring_seg;
	struct wlan_ring *ring = &iRing->ring_cache;
	ssize_t left_to_write = count;

	if (likely(iRing->ring_base)) {
		/* no enough buffer to write */
		if (WLAN_RING_WRITE_REMAIN_SIZE(ring) < left_to_write)
			goto skip;

		WLAN_RING_WRITE_FOR_EACH(left_to_write, ring_seg, ring) {
			memcpy(ring_seg.ring_pt, buf + written, ring_seg.sz);
			left_to_write -= ring_seg.sz;
			written += ring_seg.sz;
		}

	} else {
		DBGLOG(ICS, ERROR, "ics_ring not init yet\n");
		written = -EPERM;
	}

skip:
	DBGLOG(ICS, TEMP, "[Done] written:%ld left:%ld\n", written,
		left_to_write);
	return written;
}

static ssize_t ics_ring_get_buf_size(struct ics_ring *iRing)
{
	struct wlan_ring *ring = &iRing->ring_cache;

	if (unlikely(iRing->ring_base == NULL)) {
		DBGLOG(ICS, ERROR, "ics_ring not init yet\n");
		return -EPERM;
	}

	return WLAN_RING_SIZE(ring);
}

static void ics_ring_deinit(struct ics_ring *iRing)
{
	if (likely(iRing->ring_base)) {
		kvfree(iRing->ring_base);
		iRing->ring_base = NULL;
	}
}

static int fw_log_ics_open(struct inode *inode, struct file *file)
{
	struct ics_dev *prIcsDev = NULL;

	if (!inode || !inode->i_cdev) {
		DBGLOG(ICS, ERROR, "inode is NULL\n");
		return 0;
	}

	DBGLOG(ICS, TEMP, "major %d minor %d (pid %d)\n",
		imajor(inode), iminor(inode), current->pid);

	prIcsDev = CONTAINER_OF(inode->i_cdev, struct ics_dev, cdev);

	if (!prIcsDev) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return 0;
	}

	/* set-up private data */
	file->private_data = (void *) prIcsDev;

	return 0;
}

static int fw_log_ics_release(struct inode *inode, struct file *file)
{
	DBGLOG(ICS, TEMP, "major %d minor %d (pid %d)\n",
		imajor(inode), iminor(inode), current->pid);
	return 0;
}

static ssize_t fw_log_ics_read(struct file *filp, char __user *buf,
	size_t len, loff_t *off)
{
	struct ics_dev *prIcsDev = NULL;
	size_t ret = 0;

	prIcsDev = (struct ics_dev *) (filp->private_data);

	if (!prIcsDev) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return FALSE;
	}

	ret = ics_ring_read(&prIcsDev->iRing, buf, len);
	return ret;
}

static unsigned int fw_log_ics_poll(struct file *filp, poll_table *wait)
{
	struct ics_dev *prIcsDev = NULL;

	prIcsDev = (struct ics_dev *) (filp->private_data);

	if (!prIcsDev) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return 0;
	}

	poll_wait(filp, &prIcsDev->wq, wait);

	if (ics_ring_get_buf_size(&prIcsDev->iRing) > 0)
		return POLLIN|POLLRDNORM;
	return 0;
}

static long fw_log_ics_unlocked_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ics_dev *prIcsDev = NULL;
	int ret = 0;
	int i = 0;

	prIcsDev = (struct ics_dev *) (filp->private_data);

	if (!prIcsDev) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return -EPERM;
	}

#if (CFG_SUPPORT_MULTI_CARD == 0)
	prGlueInfo = wlanGetGlueInfoByNum(i);
#else
	for (i = 0; i < CFG_MAX_WLAN_DEVICES; i++) {
		if (prIcsDev == gIcsDev[i]) {
			prGlueInfo = wlanGetGlueInfoByNum(i);
			break;
		}
	}
#endif

	if (!prGlueInfo) {
		DBGLOG(ICS, ERROR, "prGlueInfo is NULL\n");
		return -EPERM;
	}

	down(&prIcsDev->ioctl_mtx);
	switch (cmd) {
	case ICS_FW_LOG_IOCTL_SET_LEVEL:{
		unsigned int level = (unsigned int) arg;

		DBGLOG(ICS, DEBUG, "ICS_FW_LOG_IOCTL_SET_LEVEL start\n");

		if (prIcsDev->pfFwEventFuncCB) {
			DBGLOG(ICS, DEBUG,
				"ICS_FW_LOG_IOCTL_SET_LEVEL invoke:%d\n",
				(int)level);
			prIcsDev->pfFwEventFuncCB(prGlueInfo,
				ICS_LOG_CMD_SET_LEVEL,
				level, TRUE);
		} else {
			DBGLOG(ICS, ERROR,
				"ICS_FW_LOG_IOCTL_SET_LEVEL invoke failed\n");
		}

		DBGLOG(ICS, DEBUG, "ICS_FW_LOG_IOCTL_SET_LEVEL end\n");
		break;
	}
	case ICS_FW_LOG_IOCTL_ON_OFF:{
		unsigned int log_on_off = (unsigned int) arg;

		DBGLOG(ICS, DEBUG, "ICS_FW_LOG_IOCTL_ON_OFF start\n");

		if (prIcsDev->pfFwEventFuncCB) {
			DBGLOG(ICS, DEBUG,
				"ICS_FW_LOG_IOCTL_ON_OFF invoke:%d\n",
				(int)log_on_off);
			prIcsDev->pfFwEventFuncCB(prGlueInfo,
				ICS_LOG_CMD_ON_OFF, log_on_off, TRUE);
		} else {
			DBGLOG(ICS, ERROR,
				"ICS_FW_LOG_IOCTL_ON_OFF invoke failed\n");
		}

		DBGLOG(ICS, DEBUG, "ICS_FW_LOG_IOCTL_ON_OFF end\n");
		break;
	}
	default:
		ret = -EPERM;
	}
	DBGLOG(ICS, DEBUG, "cmd --> %d, ret=%d\n", cmd, ret);
	up(&prIcsDev->ioctl_mtx);
	return ret;
}

#ifdef CONFIG_COMPAT
static long fw_log_ics_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	struct ics_dev *prIcsDev = NULL;
	long ret = 0;
	int32_t wait_cnt = 0;

	prIcsDev = (struct ics_dev *) (filp->private_data);

	if (!prIcsDev) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return -EPERM;
	}

	DBGLOG(ICS, DEBUG, "COMPAT cmd --> %d\n", cmd);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	while (wait_cnt < ICS_WAIT_READY_MAX_CNT) {
		if (prIcsDev->pfFwEventFuncCB)
			break;
		DBGLOG_LIMITED(ICS, ERROR,
			"Wi-Fi driver is not ready for 2s\n");
		msleep(ICS_WAIT_READY_SLEEP_TIME);
		wait_cnt++;
	}
	fw_log_ics_unlocked_ioctl(filp, cmd, arg);

	return ret;
}
#endif /* CONFIG_COMPAT */

const struct file_operations fw_log_ics_fops = {
	.open = fw_log_ics_open,
	.release = fw_log_ics_release,
	.read = fw_log_ics_read,
	.poll = fw_log_ics_poll,
	.unlocked_ioctl = fw_log_ics_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fw_log_ics_compat_ioctl,
#endif /* CONFIG_COMPAT */
};

void wifi_ics_event_func_register(ics_fwlog_event_func_cb func)
{
	uint8_t i = 0;

	for (i = 0; i < CFG_MAX_WLAN_DEVICES; i++) {
		if (!gIcsDev[i]) {
			DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
			continue;
		}

		DBGLOG(ICS, DEBUG, "wifi_ics_event_func_register %p\n", func);
		gIcsDev[i]->pfFwEventFuncCB = func;
	}
}

ssize_t wifi_ics_fwlog_write(struct GLUE_INFO *prGlueInfo,
	char *buf, size_t count)
{
	struct ics_dev *prIcsDev = NULL;
	ssize_t ret = 0;

	if (!prGlueInfo || prGlueInfo->u4DevNum >= CFG_MAX_WLAN_DEVICES ||
		!gIcsDev[prGlueInfo->u4DevNum]) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return -EPERM;
	}
	prIcsDev = gIcsDev[prGlueInfo->u4DevNum];

	ret = ics_ring_write(&prIcsDev->iRing, buf, count);
	if (ret > 0)
		wake_up_interruptible(&prIcsDev->wq);

	return ret;
}

int IcsInit(void)
{
	struct ics_dev *prIcsDev = NULL;
	uint8_t *prDrvName = NULL;
	int result = 0;
	int err = 0;
	int i = 0;

#if CFG_SUPPORT_MULTI_CARD
	for (i = 0; i < CFG_MAX_WLAN_DEVICES; i++) {
		kalSnprintf(aucIcsDevName[i], sizeof(aucIcsDevName[i]),
			FW_LOG_ICS_DRIVER_NAME "%d", i);
		prDrvName = aucIcsDevName[i];
#else
		prDrvName = FW_LOG_ICS_DRIVER_NAME;
#endif
		gIcsDev[i] = kzalloc(sizeof(struct ics_dev), GFP_KERNEL);

		if (!gIcsDev[i]) {
			result = -ENOMEM;
			goto return_fn;
		}
		prIcsDev = gIcsDev[i];

		prIcsDev->devno = MKDEV(prIcsDev->major, 0);
		result = alloc_chrdev_region(&prIcsDev->devno, 0, 1,
				prDrvName);
		prIcsDev->major = MAJOR(prIcsDev->devno);
		DBGLOG(ICS, DEBUG,
			"alloc_chrdev_region result %d, major %d\n",
			result, prIcsDev->major);

		if (result < 0)
			goto free_dev;

		prIcsDev->driver_class = KAL_CLASS_CREATE(prDrvName);

		if (KAL_IS_ERR(prIcsDev->driver_class)) {
			result = -ENOMEM;
			DBGLOG(ICS, ERROR, "class_create failed %d.\n",
				result);
			goto unregister_chrdev_region;
		}

		prIcsDev->class_dev = device_create(prIcsDev->driver_class,
			NULL, prIcsDev->devno, NULL, prDrvName);

		if (!prIcsDev->class_dev) {
			result = -ENOMEM;
			DBGLOG(ICS, ERROR, "class_device_create failed %d.\n",
				result);
			goto class_destroy;
		}

		err = ics_ring_init(&prIcsDev->iRing, ICS_LOG_SIZE);
		if (err) {
			result = -ENOMEM;
			DBGLOG(ICS, ERROR,
				"Error %d ics_ring_init.\n", err);
			goto device_destroy;
		}

		init_waitqueue_head(&prIcsDev->wq);
		sema_init(&prIcsDev->ioctl_mtx, 1);
		prIcsDev->pfFwEventFuncCB = NULL;

		cdev_init(&prIcsDev->cdev, &fw_log_ics_fops);

		prIcsDev->cdev.owner = THIS_MODULE;
		prIcsDev->cdev.ops = &fw_log_ics_fops;

		err = cdev_add(&prIcsDev->cdev, prIcsDev->devno, 1);
		if (err) {
			result = -ENOMEM;
			DBGLOG(ICS, ERROR,
				"Error %d adding fw_log_ics dev.\n", err);
			goto ics_ring_deinit;
		}
#if CFG_SUPPORT_MULTI_CARD
	}
#endif
	goto return_fn;

ics_ring_deinit:
	ics_ring_deinit(&prIcsDev->iRing);
device_destroy:
	device_destroy(prIcsDev->driver_class, prIcsDev->devno);
class_destroy:
	class_destroy(prIcsDev->driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(prIcsDev->devno, 1);
free_dev:
	kfree(prIcsDev);
return_fn:
	return result;
}

static void IcsDestory(struct ics_dev *prIcsDev)
{
	if (!prIcsDev) {
		DBGLOG(ICS, ERROR, "ICS_DEV is NULL\n");
		return;
	}

	ics_ring_deinit(&prIcsDev->iRing);
	device_destroy(prIcsDev->driver_class, prIcsDev->devno);
	class_destroy(prIcsDev->driver_class);
	cdev_del(&prIcsDev->cdev);
	unregister_chrdev_region(MKDEV(prIcsDev->major, 0), 1);
	DBGLOG(ICS, DEBUG, "unregister_chrdev_region major %d\n",
		prIcsDev->major);
	kfree(prIcsDev);
}

int IcsDeInit(struct GLUE_INFO *prGlueInfo)
{
	uint8_t i = 0;

#if (CFG_SUPPORT_MULTI_CARD == 0)
	IcsDestory(gIcsDev[i]);
#else
	if (prGlueInfo && prGlueInfo->u4DevNum < CFG_MAX_WLAN_DEVICES) {
		IcsDestory(gIcsDev[prGlueInfo->u4DevNum]);
		gIcsDev[prGlueInfo->u4DevNum] = NULL;
		return 0;
	}

	for (i = 0; i < CFG_MAX_WLAN_DEVICES; i++) {
		IcsDestory(gIcsDev[i]);
		gIcsDev[i] = NULL;
	}
#endif /* CFG_SUPPORT_MULTI_CARD */

	return 0;
}

#if CFG_SUPPORT_ICS_TIMER
void IcsTimerInit(struct ADAPTER *prAdapter)
{
	cnmTimerInitTimer(prAdapter,
			&prAdapter->rIcsTimer,
			(PFN_MGMT_TIMEOUT_FUNC) IcsLogTimeout,
			(uintptr_t) NULL);
}

void IcsLogStartWithTimer(struct ADAPTER *prAdapter)
{
	struct WIFI_VAR *prWifiVar;
	static uint32_t u4NextUpdateTime;

	if (!prAdapter || !prAdapter->prGlueInfo)
		return;

	prWifiVar = &prAdapter->rWifiVar;

	if (!prWifiVar->fgTxTimeoutIcsLog)
		return;

	if (TIME_BEFORE(kalGetTimeTick(), u4NextUpdateTime))
		return;

	u4NextUpdateTime = kalGetTimeTick() +
		prWifiVar->u4TxTimeoutIcsLogInterval;

	ics_log_event_notification(prAdapter->prGlueInfo,
		(int)ICS_LOG_CMD_SET_LEVEL,
		ENUM_ICS_LOG_LEVEL_MAC,
		FALSE);
	cnmTimerStartTimer(prAdapter, &prAdapter->rIcsTimer,
		prWifiVar->u4TxTimeoutIcsLogDuration);
	DBGLOG(ICS, INFO, "Enable Txtimeout ICS log for %u ms\n",
		prWifiVar->u4TxTimeoutIcsLogDuration);
}

void IcsLogTimeout(struct ADAPTER *prAdapter, uintptr_t ulParamPtr)
{
	if (!prAdapter || !prAdapter->prGlueInfo)
		return;

	ics_log_event_notification(prAdapter->prGlueInfo,
		(int)ICS_LOG_CMD_SET_LEVEL,
		ENUM_ICS_LOG_LEVEL_DISABLE,
		FALSE);
	ics_log_event_notification(prAdapter->prGlueInfo,
		(int)ICS_LOG_CMD_ON_OFF,
		ENUM_ICS_LOG_LEVEL_DISABLE,
		FALSE);
	DBGLOG(ICS, INFO, "Timeout to disable Txtimeout ICS log\n");
}
#endif /* CFG_SUPPORT_ICS_TIMER */
#endif /* CFG_SUPPORT_ICS */
