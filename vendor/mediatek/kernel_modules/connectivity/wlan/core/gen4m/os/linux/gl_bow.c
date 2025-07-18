// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   gl_bow.c
*    \brief  Main routines of Linux driver interface for 802.11 PAL (BT 3.0 + HS)
*
*    This file contains the main routines of Linux driver for MediaTek Inc. 802.11
*    Wireless LAN Adapters.
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "precomp.h"
#include <linux/poll.h>
#include "bss.h"

#if CFG_ENABLE_BT_OVER_WIFI

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* @FIXME if there is command/event with payload length > 28 */
#define MAX_BUFFER_SIZE         (64)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

uint32_t g_u4PrevSysTime;
uint32_t g_u4CurrentSysTime;
uint32_t g_arBowRevPalPacketTime[32];

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* forward declarations */
static ssize_t bow_ampc_read(struct file *filp, char __user *buf, size_t size,
		loff_t *ppos);

static ssize_t bow_ampc_write(struct file *filp, const char __user *buf,
		size_t size, loff_t *ppos);

static long bow_ampc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg);

static unsigned int bow_ampc_poll(struct file *filp, poll_table *wait);

static int bow_ampc_open(struct inode *inodep, struct file *filp);

static int bow_ampc_release(struct inode *inodep, struct file *filp);

/* character file operations */
static const struct file_operations bow_ampc_fops = {
	/* .owner              = THIS_MODULE, */
	.read = bow_ampc_read,
	.write = bow_ampc_write,
	.unlocked_ioctl = bow_ampc_ioctl,
	.poll = bow_ampc_poll,
	.open = bow_ampc_open,
	.release = bow_ampc_release,
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Register for character device to communicate with 802.11 PAL
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t glRegisterAmpc(struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	DBGLOG(BOW, DEBUG,
	       "Register for character device to communicate with 802.11 PAL.\n");

	if (prGlueInfo->rBowInfo.fgIsRegistered == TRUE)
		return FALSE;
#if 0
	/* 1. allocate major number dynamically */

	if (alloc_chrdev_region(&(prGlueInfo->rBowInfo.u4DeviceNumber), 0,	/* first minor number */
				1,	/* number */
				GLUE_BOW_DEVICE_NAME) != 0)

		return FALSE;
#endif

#if 1

#if defined(CONFIG_AMPC_CDEV_NUM)
	prGlueInfo->rBowInfo.u4DeviceNumber = MKDEV(CONFIG_AMPC_CDEV_NUM, 0);
#else
	prGlueInfo->rBowInfo.u4DeviceNumber = MKDEV(226, 0);
#endif

	if (register_chrdev_region(prGlueInfo->rBowInfo.u4DeviceNumber, 1,	/* number */
				   GLUE_BOW_DEVICE_NAME) != 0)

		return FALSE;
#endif

	/* 2. spin-lock initialization */
	/* spin_lock_init(&(prGlueInfo->rBowInfo.rSpinLock)); */

	/* 3. initialize kfifo */
#if 0
	prGlueInfo->rBowInfo.prKfifo = kfifo_alloc(GLUE_BOW_KFIFO_DEPTH,
	GFP_KERNEL,
	&(prGlueInfo->rBowInfo.rSpinLock));
#endif
	if ((kfifo_alloc((struct kfifo *)&(prGlueInfo->rBowInfo.rKfifo), GLUE_BOW_KFIFO_DEPTH, GFP_KERNEL)))
		goto fail_kfifo_alloc;

	/* if(prGlueInfo->rBowInfo.prKfifo == NULL) */
	if (&(prGlueInfo->rBowInfo.rKfifo) == NULL)
		goto fail_kfifo_alloc;

	/* 4. initialize cdev */
	cdev_init(&(prGlueInfo->rBowInfo.cdev), &bow_ampc_fops);
	/* prGlueInfo->rBowInfo.cdev.owner = THIS_MODULE; */
	prGlueInfo->rBowInfo.cdev.ops = &bow_ampc_fops;

	/* 5. add character device */
	if (cdev_add(&(prGlueInfo->rBowInfo.cdev), prGlueInfo->rBowInfo.u4DeviceNumber, 1))
		goto fail_cdev_add;

	/* 6. in queue initialization */
	init_waitqueue_head(&(prGlueInfo->rBowInfo.outq));

	/* 7. finish */
	prGlueInfo->rBowInfo.fgIsRegistered = TRUE;
	return TRUE;

fail_cdev_add:
	kfifo_free(&(prGlueInfo->rBowInfo.rKfifo));
/* kfifo_free(prGlueInfo->rBowInfo.prKfifo); */
fail_kfifo_alloc:
	unregister_chrdev_region(prGlueInfo->rBowInfo.u4DeviceNumber, 1);
	return FALSE;
}				/* end of glRegisterAmpc */

/*----------------------------------------------------------------------------*/
/*!
* \brief Unregister character device for communicating with 802.11 PAL
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t glUnregisterAmpc(struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	DBGLOG(BOW, DEBUG,
	       "Unregister character device for communicating with 802.11 PAL.\n");

	if (prGlueInfo->rBowInfo.fgIsRegistered == FALSE)
		return FALSE;

	prGlueInfo->rBowInfo.fgIsRegistered = FALSE;

	/* 1. free netdev if necessary */
#if CFG_BOW_SEPARATE_DATA_PATH
	kalUninitBowDevice(prGlueInfo);
#endif

	/* 2. removal of character device */
	cdev_del(&(prGlueInfo->rBowInfo.cdev));

	/* 3. free kfifo */
/* kfifo_free(prGlueInfo->rBowInfo.prKfifo); */
	kfifo_free(&(prGlueInfo->rBowInfo.rKfifo));
/* prGlueInfo->rBowInfo.prKfifo = NULL; */
/* prGlueInfo->rBowInfo.rKfifo = NULL; */

	/* 4. free device number */
	unregister_chrdev_region(prGlueInfo->rBowInfo.u4DeviceNumber, 1);

	return TRUE;

}				/* end of glUnregisterAmpc */

/*----------------------------------------------------------------------------*/
/*!
* \brief read handler for character device to communicate with 802.11 PAL
*
* \param[in]
* \return
*           Follows Linux Character Device Interface
*
*/
/*----------------------------------------------------------------------------*/
static ssize_t bow_ampc_read(struct file *filp, char __user *buf, size_t size,
		loff_t *ppos)
{
	uint8_t aucBuffer[MAX_BUFFER_SIZE];
	ssize_t retval;

	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) (filp->private_data);

	ASSERT(prGlueInfo);

	DBGLOG(BOW, DEBUG, "BoW EVENT read.\n");

	if ((prGlueInfo->rBowInfo.fgIsRegistered == FALSE) ||
		test_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag))
		return -EFAULT;
	/* size check */
/* if(kfifo_len(prGlueInfo->rBowInfo.prKfifo) >= size) */
	if (kfifo_len(&(prGlueInfo->rBowInfo.rKfifo)) >= size)
		retval = size;
	else
		retval = kfifo_len(&(prGlueInfo->rBowInfo.rKfifo));
/* retval = kfifo_len(prGlueInfo->rBowInfo.prKfifo); */

/* kfifo_get(prGlueInfo->rBowInfo.prKfifo, aucBuffer, retval); */
/* kfifo_out(prGlueInfo->rBowInfo.prKfifo, aucBuffer, retval); */
	if (!(kfifo_out(&(prGlueInfo->rBowInfo.rKfifo), aucBuffer, retval)))
		retval = -EIO;

	if (copy_to_user(buf, aucBuffer, retval))
		retval = -EIO;

	return retval;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief write handler for character device to communicate with 802.11 PAL
*
* \param[in]
* \return
*           Follows Linux Character Device Interface
*
*/
/*----------------------------------------------------------------------------*/
static ssize_t bow_ampc_write(struct file *filp, const char __user *buf,
		size_t size, loff_t *ppos)
{
	uint8_t i;

	uint8_t aucBuffer[MAX_BUFFER_SIZE];
	struct BT_OVER_WIFI_COMMAND *prCmd;
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) (filp->private_data);
	ASSERT(prGlueInfo);

	if ((prGlueInfo->rBowInfo.fgIsRegistered == FALSE) ||
		test_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag))
		return -EFAULT;

	if (size > MAX_BUFFER_SIZE)
		return -EINVAL;
	else if (copy_from_user(aucBuffer, buf, size))
		return -EIO;

	DBGLOG(BOW, EVENT, "AMP driver CMD buffer size : %d.\n", size);

	for (i = 0; i < MAX_BUFFER_SIZE; i++)
		DBGLOG(BOW, EVENT, "AMP write content : 0x%x.\n", aucBuffer[i]);

	DBGLOG(BOW, EVENT, "BoW CMD write.\n");

	prCmd = (struct BT_OVER_WIFI_COMMAND *) aucBuffer;

	DBGLOG(BOW, EVENT, "AMP write content payload length : %d.\n", prCmd->rHeader.u2PayloadLength);

	DBGLOG(BOW, EVENT, "AMP write content header length : %d.\n", sizeof(struct BT_OVER_WIFI_COMMAND_HEADER));

	/* size check */
	if (prCmd->rHeader.u2PayloadLength + sizeof(struct BT_OVER_WIFI_COMMAND_HEADER) != size) {
		DBGLOG(BOW, EVENT, "Wrong CMD total length.\n");

		return -EINVAL;
	}

	if (wlanbowHandleCommand(prGlueInfo->prAdapter, prCmd) == WLAN_STATUS_SUCCESS)
		return size;
	else
		return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief ioctl handler for character device to communicate with 802.11 PAL
*
* \param[in]
* \return
*           Follows Linux Character Device Interface
*
*/
/*----------------------------------------------------------------------------*/
static long bow_ampc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int err = 0;
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) (filp->private_data);

	ASSERT(prGlueInfo);

	if ((prGlueInfo->rBowInfo.fgIsRegistered == FALSE) ||
		test_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag))
		return -EFAULT;
	/* permission check */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* no ioctl is implemented yet */
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief ioctl handler for character device to communicate with 802.11 PAL
*
* \param[in]
* \return
*           Follows Linux Character Device Interface
*
*/
/*----------------------------------------------------------------------------*/
static unsigned int bow_ampc_poll(struct file *filp, poll_table *wait)
{
	unsigned int retval;
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) (filp->private_data);

	ASSERT(prGlueInfo);

	if ((prGlueInfo->rBowInfo.fgIsRegistered == FALSE) ||
		test_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag))
		return -EFAULT;

	poll_wait(filp, &prGlueInfo->rBowInfo.outq, wait);

	retval = (POLLOUT | POLLWRNORM);	/* always accepts incoming command packets */

/* DBGLOG(BOW, EVENT, ("bow_ampc_pol, POLLOUT | POLLWRNORM, %x\n", retval)); */

/* if(kfifo_len(prGlueInfo->rBowInfo.prKfifo) > 0) */
	if (kfifo_len(&(prGlueInfo->rBowInfo.rKfifo)) > 0) {
		retval |= (POLLIN | POLLRDNORM);

/* DBGLOG(BOW, EVENT, ("bow_ampc_pol, POLLIN | POLLRDNORM, %x\n", retval)); */

	}

	return retval;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief open handler for character device to communicate with 802.11 PAL
*
* \param[in]
* \return
*           Follows Linux Character Device Interface
*
*/
/*----------------------------------------------------------------------------*/
static int bow_ampc_open(struct inode *inodep, struct file *filp)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_BOW_INFO *prBowInfo;

	TRACE_FUNC(BOW, DEBUG, "in %s\n");

	prBowInfo = CONTAINER_OF(inodep->i_cdev, struct GL_BOW_INFO, cdev);
	ASSERT(prBowInfo);

	prGlueInfo = CONTAINER_OF(prBowInfo, struct GLUE_INFO, rBowInfo);
	ASSERT(prGlueInfo);

	/* set-up private data */
	filp->private_data = prGlueInfo;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief close handler for character device to communicate with 802.11 PAL
*
* \param[in]
* \return
*           Follows Linux Character Device Interface
*
*/
/*----------------------------------------------------------------------------*/
static int bow_ampc_release(struct inode *inodep, struct file *filp)
{
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) (filp->private_data);

	TRACE_FUNC(BOW, DEBUG, "in %s\n");

	ASSERT(prGlueInfo);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to indicate event for Bluetooth over Wi-Fi
*
* \param[in]
*           prGlueInfo
*           prEvent
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
void kalIndicateBOWEvent(struct GLUE_INFO *prGlueInfo,
		struct BT_OVER_WIFI_EVENT *prEvent)
{
	size_t u4AvailSize, u4EventSize;

	ASSERT(prGlueInfo);
	ASSERT(prEvent);

	/* check device */
	if ((prGlueInfo->rBowInfo.fgIsRegistered == FALSE) ||
		test_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag))
		return;
#if 0
	u4AvailSize = GLUE_BOW_KFIFO_DEPTH - kfifo_len(prGlueInfo->rBowInfo.prKfifo);
#endif
	u4AvailSize = GLUE_BOW_KFIFO_DEPTH - kfifo_len(&(prGlueInfo->rBowInfo.rKfifo));

	u4EventSize = prEvent->rHeader.u2PayloadLength + sizeof(struct BT_OVER_WIFI_EVENT_HEADER);

	/* check kfifo availability */
	if (u4AvailSize < u4EventSize) {
		DBGLOG(BOW, EVENT, "[bow] no space for event: %d/%d\n", u4EventSize, u4AvailSize);
		return;
	}
	/* queue into kfifo */
/* kfifo_put(prGlueInfo->rBowInfo.prKfifo, (PUINT_8)prEvent, u4EventSize); */
/* kfifo_in(prGlueInfo->rBowInfo.prKfifo, (PUINT_8)prEvent, u4EventSize); */
	kfifo_in(&(prGlueInfo->rBowInfo.rKfifo), (uint8_t *) prEvent, u4EventSize);
	wake_up_interruptible(&(prGlueInfo->rBowInfo.outq));

}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi state from glue layer
*
* \param[in]
*           prGlueInfo
*           rPeerAddr
* \return
*           ENUM_BOW_DEVICE_STATE
*/
/*----------------------------------------------------------------------------*/
enum ENUM_BOW_DEVICE_STATE kalGetBowState(struct GLUE_INFO *prGlueInfo,
		uint8_t aucPeerAddress[6])
{
	uint8_t i;

	ASSERT(prGlueInfo);

	DBGLOG(BOW, EVENT, "kalGetBowState.\n");

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (EQUAL_MAC_ADDR(prGlueInfo->rBowInfo.arPeerAddr, aucPeerAddress) == 0) {
			DBGLOG(BOW, EVENT,
			       "kalGetBowState, aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n", i,
			       aucPeerAddress[0], aucPeerAddress[1], aucPeerAddress[2],
			       aucPeerAddress[3], aucPeerAddress[4], aucPeerAddress[5]);

			DBGLOG(BOW, EVENT,
			       "kalGetBowState, prGlueInfo->rBowInfo.aeState %x, %x.\n", i,
			       prGlueInfo->rBowInfo.aeState[i]);

			return prGlueInfo->rBowInfo.aeState[i];
		}
	}

	return BOW_DEVICE_STATE_DISCONNECTED;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to set Bluetooth-over-Wi-Fi state in glue layer
*
* \param[in]
*           prGlueInfo
*           eBowState
*           rPeerAddr
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalSetBowState(struct GLUE_INFO *prGlueInfo,
		enum ENUM_BOW_DEVICE_STATE eBowState, uint8_t aucPeerAddress[6])
{
	uint8_t i;

	ASSERT(prGlueInfo);
	DBGLOG(BOW, EVENT, "kalSetBowState, aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       aucPeerAddress[0],
	       aucPeerAddress[1], aucPeerAddress[2], aucPeerAddress[3], aucPeerAddress[4], aucPeerAddress[5]);

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (EQUAL_MAC_ADDR(prGlueInfo->rBowInfo.arPeerAddr, aucPeerAddress) == 0) {
			prGlueInfo->rBowInfo.aeState[i] = eBowState;

			DBGLOG(BOW, EVENT,
			       "kalSetBowState, aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n", i,
			       aucPeerAddress[0], aucPeerAddress[1], aucPeerAddress[2],
			       aucPeerAddress[3], aucPeerAddress[4], aucPeerAddress[5]);

			DBGLOG(BOW, EVENT,
			       "kalSetBowState, prGlueInfo->rBowInfo.aeState %x, %x.\n", i,
			       prGlueInfo->rBowInfo.aeState[i]);

			return TRUE;
		}
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi global state
*
* \param[in]
*           prGlueInfo
*
* \return
*           BOW_DEVICE_STATE_DISCONNECTED
*               in case there is no BoW connection or
*               BoW connection under initialization
*
*           BOW_DEVICE_STATE_STARTING
*               in case there is no BoW connection but
*               some BoW connection under initialization
*
*           BOW_DEVICE_STATE_CONNECTED
*               in case there is any BoW connection available
*/
/*----------------------------------------------------------------------------*/
enum ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(struct GLUE_INFO *prGlueInfo)
{
	uint32_t i;

	ASSERT(prGlueInfo);

/* Henry, can reduce this logic to indentify state change */

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (prGlueInfo->rBowInfo.aeState[i] == BOW_DEVICE_STATE_CONNECTED)
			return BOW_DEVICE_STATE_CONNECTED;
	}

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (prGlueInfo->rBowInfo.aeState[i] == BOW_DEVICE_STATE_STARTING)
			return BOW_DEVICE_STATE_STARTING;
	}

	return BOW_DEVICE_STATE_DISCONNECTED;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi operating frequency
*
* \param[in]
*           prGlueInfo
*
* \return
*           in unit of KHz
*/
/*----------------------------------------------------------------------------*/
uint32_t kalGetBowFreqInKHz(struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rBowInfo.u4FreqInKHz;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi role
*
* \param[in]
*           prGlueInfo
*
* \return
*           0: Responder
*           1: Initiator
*/
/*----------------------------------------------------------------------------*/
uint8_t kalGetBowRole(struct GLUE_INFO *prGlueInfo,
		uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN])
{
	uint32_t i;

	ASSERT(prGlueInfo);

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (EQUAL_MAC_ADDR(prGlueInfo->rBowInfo.arPeerAddr[i], rPeerAddr) == 0)
			return prGlueInfo->rBowInfo.aucRole[i];
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to set Bluetooth-over-Wi-Fi role
*
* \param[in]
*           prGlueInfo
*           ucRole
*                   0: Responder
*                   1: Initiator
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
void kalSetBowRole(struct GLUE_INFO *prGlueInfo, uint8_t ucRole,
		uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN])
{
	uint32_t i;

	ASSERT(prGlueInfo);
	ASSERT(ucRole <= 1);

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (EQUAL_MAC_ADDR(prGlueInfo->rBowInfo.arPeerAddr[i], rPeerAddr) == 0) {
			/* Henry, 0 : Responder, 1 : Initiator */
			prGlueInfo->rBowInfo.aucRole[i] = ucRole;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to get available Bluetooth-over-Wi-Fi physical link number
*
* \param[in]
*           prGlueInfo
* \return
*           UINT_32
*               how many physical links are aviailable
*/
/*----------------------------------------------------------------------------*/
uint8_t kalGetBowAvailablePhysicalLinkCount(struct GLUE_INFO *prGlueInfo)
{
	uint8_t i;
	uint8_t ucLinkCount = 0;

	ASSERT(prGlueInfo);

	for (i = 0; i < CFG_BOW_PHYSICAL_LINK_NUM; i++) {
		if (prGlueInfo->rBowInfo.aeState[i] == BOW_DEVICE_STATE_DISCONNECTED)
			ucLinkCount++;
	}

#if 0
	DBGLOG(BOW, EVENT, "kalGetBowAvailablePhysicalLinkCount, ucLinkCount, %c.\n", ucLinkCount);
#endif

	return ucLinkCount;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This inline function is to extract some packet information for BoW
*
* @param prGlueInfo         Pointer to the glue structure
* @param prNdisPacket       Packet descriptor
* @param pfgIs1X            802.1x packet or not
*
* @retval TRUE      Success to extract information
* @retval FALSE     Fail to extract correct information
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalBowFrameClassifier(struct GLUE_INFO *prGlueInfo, void *prPacket,
		u_int8_t *pfgIs1X)
{
	uint32_t u4PacketLen;
	uint16_t u2EtherTypeLen;
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	uint8_t *aucLookAheadBuf = NULL;
	uint8_t ucEthTypeLenOffset = ETHER_HEADER_LEN - ETHER_TYPE_LEN;
	uint8_t *pucNextProtocol = NULL;
	uint8_t aucLLC[] = ETH_LLC;
	uint8_t aucSnapBtOui[] = ETH_SNAP_BT_SIG_OUI;
	uint8_t ucMinLength = ucEthTypeLenOffset + ETHER_TYPE_LEN + ETH_LLC_LEN + ETH_SNAP_LEN;

	u4PacketLen = prSkb->len;

	if (u4PacketLen < ETHER_HEADER_LEN) {
		DBGLOG(INIT, WARN, "Invalid Ether packet length: %lu\n", u4PacketLen);
		return FALSE;
	}

	aucLookAheadBuf = prSkb->data;

	*pfgIs1X = FALSE;

	/* 4 <0> Obtain Ether Type/Len */
	WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset], &u2EtherTypeLen);

	/* 4 <1> Skip 802.1Q header (VLAN Tagging) */
	if (u2EtherTypeLen == ETH_P_VLAN) {
		ucEthTypeLenOffset += ETH_802_1Q_HEADER_LEN;
		WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset], &u2EtherTypeLen);
	}
	/* 4 <2> Obtain next protocol pointer */
	pucNextProtocol = &aucLookAheadBuf[ucEthTypeLenOffset + ETHER_TYPE_LEN];

	/* 4 <3> Handle ethernet format */
	if (u2EtherTypeLen > ETH_802_3_MAX_LEN) {
		/* Not BoW frame */
		return FALSE;
	}
	/* 4 <4> Check for PAL (BT over Wi-Fi) */
	/* BoW LLC/SNAP header check */
	if (u4PacketLen >= ucMinLength &&
	    !kalMemCmp(pucNextProtocol, aucLLC, ETH_LLC_LEN) &&
	    !kalMemCmp(pucNextProtocol + ETH_LLC_LEN, aucSnapBtOui, ETH_SNAP_OUI_LEN)) {
		uint16_t u2LocalCode;

		WLAN_GET_FIELD_BE16(pucNextProtocol + ETH_LLC_LEN + ETH_SNAP_OUI_LEN, &u2LocalCode);

		if (u2LocalCode == BOW_PROTOCOL_ID_SECURITY_FRAME)
			*pfgIs1X = TRUE;

		return TRUE;
	}

	return FALSE;
}				/* end of kalBoWFrameClassifier() */

#if CFG_BOW_SEPARATE_DATA_PATH

/* Net Device Hooks */
/*----------------------------------------------------------------------------*/
/*!
 * \brief A function for net_device open (ifup)
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \retval 0     The execution succeeds.
 * \retval < 0   The execution failed.
 */
/*----------------------------------------------------------------------------*/
static int bowOpen(struct net_device *prDev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	/* 2. carrier on & start TX queue */
	netif_carrier_on(prDev);
	netif_tx_start_all_queues(prDev);

	return 0;		/* success */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief A function for net_device stop (ifdown)
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \retval 0     The execution succeeds.
 * \retval < 0   The execution failed.
 */
/*----------------------------------------------------------------------------*/
static int bowStop(struct net_device *prDev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	/* 1. stop TX queue */
	netif_tx_stop_all_queues(prDev);

	/* 2. turn of carrier */
	if (netif_carrier_ok(prDev))
		netif_carrier_off(prDev);

	return 0;
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is TX entry point of NET DEVICE.
 *
 * \param[in] prSkb  Pointer of the sk_buff to be sent
 * \param[in] prDev  Pointer to struct net_device
 *
 * \retval NETDEV_TX_OK - on success.
 * \retval NETDEV_TX_BUSY - on failure, packet will be discarded by upper layer.
 */
/*----------------------------------------------------------------------------*/
static netdev_tx_t bowHardStartXmit(struct sk_buff *prSkb,
		struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t ucBssIndex;
	u_int8_t fgIs1x;

	ASSERT(prSkb);
	ASSERT(prDev);

#if 1
	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(prDev);
	prGlueInfo = prNetDevPrivate->prGlueInfo;
	ucBssIndex = prNetDevPrivate->ucBssIdx;
#else
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));

	ucBssIndex = wlanGetBssIdxByNetInterface(prGlueInfo, NET_DEV_BOW_IDX);
#endif

	kalResetPacket(prGlueInfo, (void *) prSkb);

	/* Discard frames not generated by PAL */
	/* Parsing BOW frame info */
	if (!kalBowFrameClassifier(prGlueInfo, (void *) prSkb, &fgIs1x)) {
		/* Cannot extract packet */
		DBGLOG(BOW, DEBUG, "Invalid BOW frame, skip Tx\n");
		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}

	if (fgIs1x)
		GLUE_SET_PKT_FLAG(prSkb, ENUM_PKT_1X);

	if (kalHardStartXmit(prSkb, prDev, prGlueInfo, ucBssIndex) == WLAN_STATUS_SUCCESS) {
		/* Successfully enqueue to Tx queue */
	}

	/* For Linux, we'll always return OK FLAG, because we'll free this skb by ourself */
	return NETDEV_TX_OK;
}

/* callbacks for netdevice */
static const struct net_device_ops bow_netdev_ops = {
	.ndo_open = bowOpen,
	.ndo_stop = bowStop,
	.ndo_start_xmit = bowHardStartXmit,
};

/*----------------------------------------------------------------------------*/
/*!
* \brief initialize net device for Bluetooth-over-Wi-Fi
*
* \param[in]
*           prGlueInfo
*           prDevName
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalInitBowDevice(struct GLUE_INFO *prGlueInfo, const char *prDevName)
{
	struct ADAPTER *prAdapter;
	struct GL_HIF_INFO *prHif;
	uint8_t rMacAddr[PARAM_MAC_ADDR_LEN];
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->rBowInfo.fgIsRegistered == TRUE);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prHif = &prGlueInfo->rHifInfo;
	ASSERT(prHif);

	prChipInfo = prGlueInfo->prAdapter->chip_info;

	if (prGlueInfo->rBowInfo.fgIsNetRegistered == FALSE) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
		prGlueInfo->rBowInfo.prDevHandler =
			alloc_netdev_mq(sizeof(struct GLUE_INFO *), prDevName,
			NET_NAME_PREDICTABLE, ether_setup, CFG_MAX_TXQ_NUM);
#else
		prGlueInfo->rBowInfo.prDevHandler =
		    alloc_netdev_mq(sizeof(struct GLUE_INFO *), prDevName, ether_setup, CFG_MAX_TXQ_NUM);
#endif
		if (!prGlueInfo->rBowInfo.prDevHandler)
			return FALSE;

		/* 1. setup netdev */
		/* 1.1 Point to shared glue structure */
		/* *((P_GLUE_INFO_T *) netdev_priv(prGlueInfo->rBowInfo.prDevHandler)) = prGlueInfo; */
		prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(prGlueInfo->rBowInfo.prDevHandler);
		prNetDevPriv->prGlueInfo = prGlueInfo;

		/* 1.2 fill hardware address */
		COPY_MAC_ADDR(rMacAddr, prAdapter->rMyMacAddr);
		rMacAddr[0] |= 0x2;	/* change to local administrated address */
		kalMemCopy(prGlueInfo->rBowInfo.prDevHandler->dev_addr, rMacAddr, ETH_ALEN);
		kalMemCopy(prGlueInfo->rBowInfo.prDevHandler->perm_addr,
			   prGlueInfo->rBowInfo.prDevHandler->dev_addr, ETH_ALEN);

		/* 1.3 register callback functions */
		prGlueInfo->rBowInfo.prDevHandler->needed_headroom =
			NIC_TX_DESC_AND_PADDING_LENGTH + prChipInfo->txd_append_size;
		prGlueInfo->rBowInfo.prDevHandler->netdev_ops = &bow_netdev_ops;

#if defined(_HIF_SDIO) && (MTK_WCN_HIF_SDIO == 0)
		SET_NETDEV_DEV(prGlueInfo->rBowInfo.prDevHandler, &(prHif->func->dev));
#endif

		register_netdev(prGlueInfo->rBowInfo.prDevHandler);

		/* 2. net device initialize */
		netif_carrier_off(prGlueInfo->rBowInfo.prDevHandler);
		netif_tx_stop_all_queues(prGlueInfo->rBowInfo.prDevHandler);

		/* 2.1 bind NetDev pointer to NetDev index */
		wlanBindBssIdxToNetInterface(prGlueInfo, bowInit(prAdapter),
					     (void *) prGlueInfo->rBowInfo.prDevHandler);
		prNetDevPriv->ucBssIdx = prAdapter->rWifiVar.rBowFsmInfo.ucBssIndex;
		/* wlanBindNetInterface(prGlueInfo, NET_DEV_BOW_IDX, */
		/* (PVOID)prGlueInfo->rBowInfo.prDevHandler); */

		/* 3. finish */
		prGlueInfo->rBowInfo.fgIsNetRegistered = TRUE;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief uninitialize net device for Bluetooth-over-Wi-Fi
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalUninitBowDevice(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);
	/* ASSERT(prGlueInfo->rBowInfo.fgIsRegistered == TRUE); */

	if (prGlueInfo->rBowInfo.fgIsNetRegistered == TRUE) {

		prGlueInfo->rBowInfo.fgIsNetRegistered = FALSE;

		bowUninit(prAdapter);

		if (netif_carrier_ok(prGlueInfo->rBowInfo.prDevHandler))
			netif_carrier_off(prGlueInfo->rBowInfo.prDevHandler);

		netif_tx_stop_all_queues(prGlueInfo->rBowInfo.prDevHandler);

		/* netdevice unregistration & free */
		unregister_netdev(prGlueInfo->rBowInfo.prDevHandler);
		free_netdev(prGlueInfo->rBowInfo.prDevHandler);
		prGlueInfo->rBowInfo.prDevHandler = NULL;

		return TRUE;

	} else {
		return FALSE;
	}
}

#endif /* CFG_BOW_SEPARATE_DATA_PATH */
#endif /* CFG_ENABLE_BT_OVER_WIFI */
