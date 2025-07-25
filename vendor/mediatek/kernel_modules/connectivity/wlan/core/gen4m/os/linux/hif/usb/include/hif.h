/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "hif.h"
*    \brief  Functions for the driver to register bus and setup the IRQ
*
*    Functions for the driver to register bus and setup the IRQ
*/


#ifndef _HIF_H
#define _HIF_H

#include "nic_cmd_event.h"
#include "wlan_typedef.h"

enum ENUM_USB_END_POINT {
	USB_DATA_BULK_OUT_EP4 = 4,
	USB_DATA_BULK_OUT_EP5,
	USB_DATA_BULK_OUT_EP6,
	USB_DATA_BULK_OUT_EP7,
	USB_DATA_BULK_OUT_EP8,
	USB_DATA_BULK_OUT_EP9,

	USB_DATA_BULK_IN_EP4 = 4,
	USB_DATA_BULK_IN_EP5,

};

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#if defined(_HIF_USB)
#define HIF_NAME "USB"
#else
#error "No HIF defined!"
#endif
#define HIF_CR4_FWDL_SECTION_NUM         2
#define HIF_IMG_DL_STATUS_PORT_IDX       0
#define HIF_IST_LOOP_COUNT              (4)
#define HIF_IST_TX_THRESHOLD            (1) /* Min msdu count to trigger Tx during INT polling state */

#ifdef CFG_HIF_NUM_OF_QM_RX_PKT_NUM
#define HIF_NUM_OF_QM_RX_PKT_NUM        (CFG_HIF_NUM_OF_QM_RX_PKT_NUM)
#else
#define HIF_NUM_OF_QM_RX_PKT_NUM        (512)
#endif

#define HIF_TX_BUFF_COUNT_TC0            256
#define HIF_TX_BUFF_COUNT_TC1            256
#define HIF_TX_BUFF_COUNT_TC2            256
#define HIF_TX_BUFF_COUNT_TC3            256
#define HIF_TX_BUFF_COUNT_TC4            256
#define HIF_TX_BUFF_COUNT_TC5            256

#define HIF_TX_RESOURCE_CTRL             0 /* enable/disable TX resource control */
#define HIF_TX_RESOURCE_CTRL_PLE         0 /* enable/disable TX resource control PLE */


#define HIF_TX_PAGE_SIZE_IN_POWER_OF_2   0
#define HIF_TX_PAGE_SIZE                 1	/* in unit of bytes */

#define USB_EVENT_TYPE                  (EVENT_EP_TYPE_UNKONW)

#define USB_CMD_EP_OUT                  (USB_DATA_BULK_OUT_EP8)
#define USB_WDT_EP_IN                   (0x86)
#define USB_EVENT_EP_IN                 (0x85)
#define USB_DATA_EP_IN                  (0x84)

#define HIF_TX_INIT_CMD_PORT             USB_CMD_EP_OUT

#ifdef CFG_USB_REQ_TX_DATA_FFA_CNT
#define USB_REQ_TX_DATA_FFA_CNT         (CFG_USB_REQ_TX_DATA_FFA_CNT)	/* platform specific USB_REQ_TX_DATA_FFA_CNT */
#else
#define USB_REQ_TX_DATA_FFA_CNT         (10)
#endif

#ifdef CFG_USB_REQ_TX_DATA_CNT
#define USB_REQ_TX_DATA_CNT             (CFG_USB_REQ_TX_DATA_CNT)	/* platform specific USB_REQ_TX_DATA_CNT */
#else
#define USB_REQ_TX_DATA_CNT             (2)	/* must be >= 2 */
#endif

#define USB_REQ_TX_CMD_CNT              (CFG_TX_MAX_CMD_PKT_NUM)
#define USB_REQ_RX_EVENT_CNT            (1)
#define USB_REQ_RX_WDT_CNT              (1)
#ifdef CFG_USB_REQ_RX_DATA_CNT
#define USB_REQ_RX_DATA_CNT             (CFG_USB_REQ_RX_DATA_CNT)	/* platform specific USB_REQ_RX_DATA_CNT */
#else
#define USB_REQ_RX_DATA_CNT             (2)
#endif

#define USB_RX_AGGREGTAION_LIMIT        (32)	/* Unit: K-bytes */
#define USB_RX_AGGREGTAION_TIMEOUT      (100)	/* Unit: us */
#define USB_RX_AGGREGTAION_PKT_LIMIT    (30)

#define USB_TX_CMD_BUF_SIZE             (1600)
#define USB_TX_DATA_BUFF_SIZE           (32*1024)
#define USB_RX_EVENT_BUF_SIZE           (CFG_RX_MAX_PKT_SIZE + 3 + LEN_USB_RX_PADDING_CSO + 4)
#define USB_RX_WDT_BUF_SIZE             (1)
#define MDP_MAX_MSDU_SIZE               (0x680 << 3) /* 13312 Bytes */
#define MAX_RXD_SIZE                    (192)
#define HIF_RX_HEADER_SIZE              (12)
#define USB_RX_DATA_BUF_SIZE            ((MDP_MAX_MSDU_SIZE + MAX_RXD_SIZE +\
					 HIF_RX_HEADER_SIZE + \
					 LEN_USB_RX_PADDING_CSO + 4)+ \
					 min(USB_RX_AGGREGTAION_LIMIT * 1024, \
					     (USB_RX_AGGREGTAION_PKT_LIMIT * \
					      (CFG_RX_MAX_PKT_SIZE + 3 + LEN_USB_RX_PADDING_CSO) + 4)))

#define LEN_USB_UDMA_TX_TERMINATOR      (4)	/*HW design spec */
#ifdef CFG_USB_RX_PADDING_CSO_LEN
#define LEN_USB_RX_PADDING_CSO          (CFG_USB_RX_PADDING_CSO_LEN)
#else
#define LEN_USB_RX_PADDING_CSO          (4)	/*HW design spec */
#endif

#define USB_RX_EVENT_RFB_RSV_CNT        (0)
#define USB_RX_DATA_RFB_RSV_CNT         (4)
#define USB_RX_WDT_RFB_RSV_CNT          (0)

#define DEVICE_VENDOR_REQUEST_IN        (0xc0)
#define DEVICE_VENDOR_REQUEST_IN_CONNAC2       (0xdF)
#define DEVICE_VENDOR_REQUEST_OUT       (0x40)
#define DEVICE_VENDOR_REQUEST_OUT_CONNAC2       (0x5F)
#define VENDOR_TIMEOUT_MS               (1000)
#if CFG_MTK_FPGA_PLATFORM
#define BULK_TIMEOUT_MS                 (3500)
#else
#define BULK_TIMEOUT_MS                 (1500)
#endif
#define INTERRUPT_TIMEOUT_MS            (1000)
#define SW_RFB_RECHECK_MS               (10)
#define SW_RFB_LOG_LIMIT_MS             (5000)
#define DEVICE_VENDOR_REQUEST_UHW_IN    (0xDE)
#define DEVICE_VENDOR_REQUEST_UHW_OUT   (0x5E)

/* Vendor Request */
#define VND_REQ_POWER_ON_WIFI           (0x4)
#define VND_REQ_REG_READ                (0x63)
#define VND_REQ_REG_WRITE               (0x66)
#define VND_REQ_EP5_IN_INFO             (0x67)
#define VND_REQ_FEATURE_SET             (0x91)
#define FEATURE_SET_WVALUE_RESUME       (0x5)
#define FEATURE_SET_WVALUE_SUSPEND      (0x6)
#define VND_REQ_BUF_SIZE                (16)
#define VND_REQ_UHW_READ                (0x01)
#define VND_REQ_UHW_WRITE               (0x02)
#if CFG_DC_USB_WOW_CALLBACK
#define VND_REQ_USB_SHUTDOWN            (0x55)
#endif
/* When vendor requests keep fail over this TH, bypass subsequent vendor
 * requests since chip may not work and reset is required.
 */
#define VND_REQ_FAIL_TH                 (0x3)

#define USB_TX_CMD_QUEUE_MASK           (BITS(2, 4))   /* For H2CDMA Tx CMD mapping */

#define USB_DBDC1_TC                    (TC_NUM)/* for DBDC1 */
#define USB_TC_NUM                      (TC_NUM + 1)/* for DBDC1 */
#define USB_TX_EPOUT_NUM                (5)
#define USB_DMASHDL_DATA_GROUP_NUM      (5)

#define HIF_EXTRA_IO_BUFFER_SIZE        (0)

#define HIF_TX_COALESCING_BUFFER_SIZE   (USB_TX_CMD_BUF_SIZE)
#define HIF_RX_COALESCING_BUFFER_SIZE   (USB_RX_EVENT_BUF_SIZE)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* host interface's private data structure, which is attached to os glue
** layer info structure.
 */

enum usb_state {
	USB_STATE_WIFI_OFF, /* Hif power off wifi */
	USB_STATE_LINK_DOWN,
	USB_STATE_PRE_SUSPEND,
	USB_STATE_PRE_SUSPEND_FAIL,
	USB_STATE_SUSPEND,
	USB_STATE_PRE_RESUME,
	USB_STATE_TRX_FORBID,
	USB_STATE_LINK_UP
};

enum usb_submit_type {
	SUBMIT_TYPE_TX_CMD,
	SUBMIT_TYPE_TX_DATA,
	SUBMIT_TYPE_RX_EVENT,
	SUBMIT_TYPE_RX_DATA,
	SUBMIT_TYPE_RX_WDT
};

enum EVENT_EP_TYPE {
	EVENT_EP_TYPE_UNKONW,
	EVENT_EP_TYPE_BULK,
	EVENT_EP_TYPE_INTR,
	EVENT_EP_TYPE_DATA_EP
};

enum ENUM_SUSPEND_VERSION {
	SUSPEND_V1 = 1,
	SUSPEND_V2
};

struct BUF_CTRL {
	uint8_t *pucBuf;
	uint32_t u4BufSize;
	uint32_t u4WrIdx;
	uint32_t u4ReadSize;
};

struct GL_HIF_INFO {
	struct usb_interface *intf;
	struct usb_device *udev;

	struct GLUE_INFO *prGlueInfo;
	enum usb_state state;
	/* Use stateSyncCtrl to determine if it's allowed to send synchronous
	 * usb control such as usb_control_msg, usb_bulk_msg, etc.
	 * On the other hand, use state to determine if it's allowed to send
	 * asynchronous usb control such as usb_submit_urb.
	 */
	enum usb_state stateSyncCtrl;

	spinlock_t rTxDataQLock;
	spinlock_t rTxCmdQLock;
	spinlock_t rRxEventQLock;
	spinlock_t rRxDataQLock;
#if CFG_CHIP_RESET_SUPPORT
	spinlock_t rRxWdtQLock;
#endif
	spinlock_t rStateLock;

	void *prTxCmdReqHead;
	void *arTxDataFfaReqHead;
	void *arTxDataReqHead[USB_TC_NUM];
	void *prRxEventReqHead;
	void *prRxDataReqHead;
#if CFG_CHIP_RESET_SUPPORT
	void *prRxWdtReqHead;
#endif
	struct list_head rTxCmdFreeQ;
	spinlock_t rTxCmdFreeQLock;
	struct list_head rTxCmdSendingQ;
	spinlock_t rTxCmdSendingQLock;
	struct list_head rTxDataFfaQ;
	uint32_t u4AggRsvSize[USB_TC_NUM];
	struct list_head rTxDataFreeQ[USB_TC_NUM];
	struct usb_anchor rTxDataAnchor[USB_TC_NUM];
	/*spinlock_t rTxDataFreeQLock;*/
	struct list_head rRxEventFreeQ;
	/*spinlock_t rRxEventFreeQLock;*/
	struct usb_anchor rRxEventAnchor;
	struct list_head rRxDataFreeQ;
	/*spinlock_t rRxDataFreeQLock;*/
	struct usb_anchor rRxDataAnchor;
	struct list_head rRxEventCompleteQ;
	/*spinlock_t rRxEventCompleteQLock;*/
	struct list_head rRxDataCompleteQ;
	/*spinlock_t rRxDataCompleteQLock;*/
	struct list_head rTxCmdCompleteQ;
	struct list_head rTxDataCompleteQ;
#if CFG_CHIP_RESET_SUPPORT
	struct list_head rRxWdtFreeQ;
	struct usb_anchor rRxWdtAnchor;
	struct list_head rRxWdtCompleteQ;
#endif

	struct BUF_CTRL rTxCmdBufCtrl[USB_REQ_TX_CMD_CNT];
	struct BUF_CTRL rTxDataFfaBufCtrl[USB_REQ_TX_DATA_FFA_CNT];
	struct BUF_CTRL rTxDataBufCtrl[USB_TC_NUM][USB_REQ_TX_DATA_CNT];
	struct BUF_CTRL rRxEventBufCtrl[USB_REQ_RX_EVENT_CNT];
	struct BUF_CTRL rRxDataBufCtrl[USB_REQ_RX_DATA_CNT];
#if CFG_CHIP_RESET_SUPPORT
	struct BUF_CTRL rRxWdtBufCtrl[USB_REQ_RX_WDT_CNT];
#endif

	struct mutex vendor_req_sem;
	void *vendor_req_buf;
	u_int32_t vendor_req_buf_sz;
	u_int8_t fgIntReadClear;
	u_int8_t fgMbxReadClear;
	u_int8_t fgEventEpDetected;
#if CFG_DC_USB_WOW_CALLBACK
	u_int8_t fgUsbShutdown;
#endif
	enum EVENT_EP_TYPE eEventEpType;
};

struct USB_REQ {
	struct list_head list;
	struct urb *prUrb;
	struct BUF_CTRL *prBufCtrl;
	struct GL_HIF_INFO *prHifInfo;
	void *prPriv;
	struct QUE rSendingDataMsduInfoList;
};

struct BUS_INFO {
	const uint32_t u4UdmaWlCfg_0_Addr;
	const uint32_t u4UdmaWlCfg_1_Addr;
	const uint32_t u4UdmaTxQsel;
	const uint32_t u4UdmaConnInfraStatusSelAddr;
	const uint32_t u4UdmaConnInfraStatusAddr;
	const uint32_t u4device_vender_request_in;
	const uint32_t u4device_vender_request_out;
	const uint32_t u4usb_tx_cmd_queue_mask;
	uint32_t u4UdmaWlCfg_0;
	uint32_t u4UdmaTxTimeout; /* UDMA Tx time out limit, unit: us */
	uint32_t u4SuspendVer;
	struct DMASHDL_CFG *prDmashdlCfg;
	/* Is support USB Interrupt IN Endpoint for WDT? */
	u_int8_t fgIsSupportWdtEp;
	/* If vendor request is fail, then increment this field.
	 * Otherwise, reset this field to zero.
	 */
	uint8_t ucVndReqToMcuFailCnt;

	u_int8_t (*asicUsbSuspend)(
		struct ADAPTER *prAdapter,
		struct GLUE_INFO *prGlueInfo);
	u_int8_t (*asicUsbResume)(
		struct ADAPTER *prAdapter,
		struct GLUE_INFO *prGlueInfo);
	uint8_t (*asicUsbEventEpDetected)(struct ADAPTER *prAdapter);
	uint16_t (*asicUsbRxByteCount)(struct ADAPTER *prAdapter,
		struct BUS_INFO *prBusInfo,
		uint8_t *pRXD);
	/* Do DMASDHL init when WIFISYS is initialized at probe, L0.5 reset,
	 * etc.
	 */
	void (*DmaShdlInit)(struct ADAPTER *prAdapter);
	/* Although DMASHDL was init, we need to reinit it again due to falcon
	 * L1 reset, etc. Take MT7961 as example. The difference between
	 * mt7961DmashdlInit and mt7961DmashdlReInit is that we don't init CRs
	 * such as refill, min_quota, max_quota in mt7961DmashdlReInit, which
	 * are backup and restored in fw. The reason why some DMASHDL CRs are
	 * reinit by driver and some by fw is
	 *     1. Some DMASHDL CRs shall be inited before fw releases UMAC reset
	 *        in L1 procedure. Then, these CRs are backup and restored by fw
	 *     2. However, the backup and restore of each DMASHDL CR in fw needs
	 *        wm DLM space. So, we save DLM space by reinit the remaining
	 *        DMASHDL CRs in driver.
	 */
	void (*DmaShdlReInit)(struct ADAPTER *prAdapter);
	void (*processAbnormalInterrupt)(struct ADAPTER *prAdapter);
	void (*asicUdmaRxFlush)(struct ADAPTER *prAdapter, uint8_t bEnable);

	uint32_t (*updateTxRingMaxQuota)(struct ADAPTER *prAdapter,
		uint8_t ucWmmIndex, uint32_t u4MaxQuota);
	u_int8_t (*asicUsbEpctlRstOpt)(struct ADAPTER *prAdapter,
				       u_int8_t fgIsRstScopeIncludeToggleBit);
};

/* USB_REQ_T prPriv field for TxData */
#define FFA_MASK                        BIT(7)           /* Indicate if this UsbReq is from FFA queue. */
#define TC_MASK                         BITS(0, 6)       /* Indicate which TC this UsbReq belongs to */

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define USB_TRANS_MSDU_TC(_prMsduInfo) \
	((_prMsduInfo)->ucWmmQueSet ? USB_DBDC1_TC : (_prMsduInfo)->ucTC)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove);

void glUnregisterBus(remove_card pfRemove);

void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie);

void glClearHifInfo(struct GLUE_INFO *prGlueInfo);

void glResetHifInfo(struct GLUE_INFO *prGlueInfo);

u_int8_t glBusInit(void *pvData);

void glBusRelease(void *pData);

int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie);

void glBusFreeIrq(void *pvData, void *pvCookie);

void glSetPowerState(struct GLUE_INFO *prGlueInfo, uint32_t ePowerMode);

void glUdmaTxRxEnable(struct GLUE_INFO *prGlueInfo, u_int8_t enable);

void glUdmaRxAggEnable(struct GLUE_INFO *prGlueInfo, u_int8_t enable);

int32_t mtk_usb_vendor_request(struct GLUE_INFO *prGlueInfo,
		uint8_t uEndpointAddress, uint8_t RequestType,
	    uint8_t Request, uint16_t Value, uint16_t Index,
	    void *TransferBuffer, uint32_t TransferBufferLength);
#if CFG_DC_USB_WOW_CALLBACK
void mtk_usb_shutdown_vnd_cmd(struct GLUE_INFO *prGlueInfo);
#endif

void glUsbEnqueueReq(struct GL_HIF_INFO *prHifInfo, struct list_head *prHead, struct USB_REQ *prUsbReq,
		     spinlock_t *prLock, u_int8_t fgHead);
struct USB_REQ *glUsbDequeueReq(struct GL_HIF_INFO *prHifInfo, struct list_head *prHead, spinlock_t *prLock);
u_int8_t glUsbBorrowFfaReq(struct GL_HIF_INFO *prHifInfo, uint8_t ucTc);

void glUsbSetState(struct GL_HIF_INFO *prHifInfo, enum usb_state state);

void glUsbSetStateSyncCtrl(struct GL_HIF_INFO *prHifInfo, enum usb_state state);

int glUsbSubmitUrb(struct GL_HIF_INFO *prHifInfo, struct urb *urb,
			enum usb_submit_type type);

uint32_t halTxUSBSendCmd(struct GLUE_INFO *prGlueInfo, uint8_t ucTc,
		struct CMD_INFO *prCmdInfo);
void halTxUSBSendCmdComplete(struct urb *urb);
void halTxUSBProcessCmdComplete(struct ADAPTER *prAdapter,
		struct USB_REQ *prUsbReq);

uint32_t halTxUSBSendData(struct GLUE_INFO *prGlueInfo,
		struct MSDU_INFO *prMsduInfo);
uint32_t halTxUSBKickData(struct GLUE_INFO *prGlueInfo);
void halTxUSBSendDataComplete(struct urb *urb);
void halTxUSBProcessMsduDone(struct GLUE_INFO *prGlueInfo,
		struct USB_REQ *prUsbReq);
void halTxUSBProcessDataComplete(struct ADAPTER *prAdapter,
		struct USB_REQ *prUsbReq);

uint32_t halRxUSBEnqueueRFB(
	struct ADAPTER *prAdapter,
	uint8_t *pucBuf,
	uint32_t u4Length,
	uint32_t u4MinRfbCnt,
	struct list_head *prCompleteQ);
uint32_t halRxUSBReceiveEvent(struct ADAPTER *prAdapter, u_int8_t fgFillUrb);
void halRxUSBReceiveEventComplete(struct urb *urb);
uint32_t halRxUSBReceiveWdt(struct ADAPTER *prAdapter);
void halRxUSBReceiveWdtComplete(struct urb *urb);
uint32_t halRxUSBReceiveData(struct ADAPTER *prAdapter);
void halRxUSBReceiveDataComplete(struct urb *urb);
void halRxUSBProcessEventDataComplete(struct ADAPTER *prAdapter,
		struct list_head *prCompleteQ, struct list_head *prFreeQ,
		uint32_t u4MinRfbCnt);
void halRxUSBProcessWdtComplete(struct ADAPTER *prAdapter,
				struct list_head *prCompleteQ,
				struct list_head *prFreeQ,
				uint32_t u4MinRfbCnt);

void halUSBPreSuspendCmd(struct ADAPTER *prAdapter);
void halUSBPreResumeCmd(struct ADAPTER *prAdapter);
void halUSBPreSuspendDone(struct ADAPTER *prAdapter, struct CMD_INFO *prCmdInfo,
		uint8_t *pucEventBuf);
void halUSBPreSuspendTimeout(struct ADAPTER *prAdapter,
		struct CMD_INFO *prCmdInfo);

uint32_t halSerGetMcuEvent(struct ADAPTER *prAdapter, u_int8_t fgClear);

void glGetDev(void *ctx, void **dev);
void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev);

struct mt66xx_hif_driver_data *get_platform_driver_data(void);
struct mt66xx_hif_driver_data *get_platform_driver_data_by_dev(void *ctx);

void glGetChipInfo(void **prChipInfo);
void glGetChipInfoByGlue(struct GLUE_INFO *prGlueInfo, void **prChipInfo);

void halGetCompleteStatus(struct ADAPTER *prAdapter, uint32_t *pu4IntStatus);

uint16_t glGetUsbDeviceVendorId(struct usb_device *dev);
uint16_t glGetUsbDeviceProductId(struct usb_device *dev);

int32_t glGetUsbDeviceManufacturerName(struct usb_device *dev, uint8_t *buffer, uint32_t bufLen);
int32_t glGetUsbDeviceProductName(struct usb_device *dev, uint8_t *buffer, uint32_t bufLen);
int32_t glGetUsbDeviceSerialNumber(struct usb_device *dev, uint8_t *buffer, uint32_t bufLen);

static inline int32_t glBusFuncOn(void)
{
	return 0;
}

static inline void glBusFuncOff(void)
{
}

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
