// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   cmm_asic_connac2x.c
*    \brief  Internal driver stack will export the required procedures here for
* GLUE Layer.
*
*    This file contains all routines which are exported from MediaTek 802.11
* Wireless
*    LAN driver stack to GLUE Layer.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

#if (CFG_SUPPORT_CONNAC2X == 1)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#ifdef MT7961
#include "coda/mt7961/wf_wfdma_host_dma0.h"
#else
#include "coda/mt7915/wf_wfdma_host_dma0.h"
#endif
#include "coda/mt7915/wf_wfdma_host_dma1.h"

#include "precomp.h"
#include "wlan_lib.h"
#include "gl_coredump.h"
#include "gl_fw_log.h"
#include "gl_rst.h"

#if (CFG_SUPPORT_CONNINFRA == 1)
#include "connsys_debug_utility.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define CONNAC2X_HIF_DMASHDL_BASE 0x52000000

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

#define USB_ACCESS_RETRY_LIMIT           1

#if defined(_HIF_USB)
#define MAX_POLLING_LOOP 2
uint32_t g_au4UsbPollAddrTbl[] = {
	CONNAC2X_U3D_RX4CSR0,
	CONNAC2X_U3D_RX5CSR0,
	CONNAC2X_U3D_RX6CSR0,
	CONNAC2X_U3D_RX7CSR0,
	CONNAC2X_U3D_RX8CSR0,
	CONNAC2X_U3D_RX9CSR0,
	CONNAC2X_UDMA_WLCFG_0,
	CONNAC2X_UDMA_WL_TX_SCH_ADDR,
	CONNAC2X_UDMA_AR_CMD_FIFO_ADDR,
	WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_EXT2_CSR_TX_DROP_MODE_ADDR,
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HIF_MISC_HIF_BUSY_ADDR,
	CONNAC2X_UDMA_WL_STOP_DP_OUT_ADDR
};
uint32_t g_au4UsbPollMaskTbl[] = {
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_UDMA_WLCFG_0_WL_TX_BUSY_MASK,
	CONNAC2X_UDMA_WL_TX_SCH_MASK,
	CONNAC2X_UDMA_AR_CMD_FIFO_MASK,
	WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_EXT2_CSR_TX_DROP_MODE_MASK,
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HIF_MISC_HIF_BUSY_MASK,
	CONNAC2X_UDMA_WL_STOP_DP_OUT_DROP_MASK
};
uint32_t g_au4UsbPollValueTbl[] = {
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	CONNAC2X_U3D_RX_FIFOEMPTY,
	0,
	CONNAC2X_UDMA_WL_TX_SCH_IDLE,
	CONNAC2X_UDMA_AR_CMD_FIFO_MASK,
	0,
	0,
	0
};
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
void asicConnac2xCapInit(
	struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo;
	struct mt66xx_chip_info *prChipInfo;
	struct BUS_INFO *prBusInfo = NULL;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	prChipInfo->u2HifTxdSize = 0;
	prChipInfo->u2TxInitCmdPort = 0;
	prChipInfo->u2TxFwDlPort = 0;
	prChipInfo->fillHifTxDesc = NULL;
	prChipInfo->u2CmdTxHdrSize = sizeof(struct CONNAC2X_WIFI_CMD);
	prChipInfo->asicFillInitCmdTxd = asicConnac2xFillInitCmdTxd;
	prChipInfo->asicFillCmdTxd = asicConnac2xFillCmdTxd;
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	prChipInfo->u2UniCmdTxHdrSize = sizeof(struct CONNAC2X_WIFI_UNI_CMD);
	prChipInfo->asicFillUniCmdTxd = asicConnac2xFillUniCmdTxd;
#endif
	prChipInfo->u2RxSwPktBitMap = CONNAC2X_RX_STATUS_PKT_TYPE_SW_BITMAP;
	prChipInfo->u2RxSwPktEvent = CONNAC2X_RX_STATUS_PKT_TYPE_SW_EVENT;
	prChipInfo->u2RxSwPktFrame = CONNAC2X_RX_STATUS_PKT_TYPE_SW_FRAME;
	prChipInfo->u4ExtraTxByteCount = 0;
	asicConnac2xInitTxdHook(prChipInfo->prTxDescOps);
	asicConnac2xInitRxdHook(prChipInfo->prRxDescOps);
#if (CFG_SUPPORT_MSP == 1)
	prChipInfo->asicRxProcessRxvforMSP = asicConnac2xRxProcessRxvforMSP;
#endif /* CFG_SUPPORT_MSP == 1 */
	prChipInfo->asicRxGetRcpiValueFromRxv =
			asicConnac2xRxGetRcpiValueFromRxv;
#if (CFG_SUPPORT_PERF_IND == 1)
	prChipInfo->asicRxPerfIndProcessRXV = asicConnac2xRxPerfIndProcessRXV;
#endif
#if (CFG_CHIP_RESET_SUPPORT == 1) && (CFG_WMT_RESET_API_SUPPORT == 0)
	prChipInfo->rst_L0_notify_step2 = conn2_rst_L0_notify_step2;
#endif
#if CFG_SUPPORT_WIFI_SYSDVT
	prAdapter->u2TxTest = TX_TEST_UNLIMITIED;
	prAdapter->u2TxTestCount = 0;
	prAdapter->ucTxTestUP = TX_TEST_UP_UNDEF;
#endif /* CFG_SUPPORT_WIFI_SYSDVT */

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		prAdapter->fgEnShowHETrigger = FALSE;
		heRlmInitHeHtcACtrlOMAndUPH(prAdapter);
	}
#endif

	switch (prGlueInfo->u4InfType) {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	case MT_DEV_INF_PCIE:
	case MT_DEV_INF_AXI:

		prChipInfo->u2TxInitCmdPort =
			TX_RING_CMD; /* Ring17 for CMD */
		prChipInfo->u2TxFwDlPort =
			TX_RING_FWDL; /* Ring16 for FWDL */
		prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD;
		prChipInfo->u4HifDmaShdlBaseAddr = CONNAC2X_HIF_DMASHDL_BASE;

		HAL_MCR_WR(prAdapter,
				CONNAC2X_BN0_IRQ_ENA_ADDR,
				BIT(0));

		break;
#endif /* _HIF_PCIE */
#if defined(_HIF_USB)
	case MT_DEV_INF_USB:
		prChipInfo->u2HifTxdSize = USB_HIF_TXD_LEN;
		prChipInfo->fillHifTxDesc = fillUsbHifTxDesc;
		prChipInfo->u2TxInitCmdPort = USB_DATA_BULK_OUT_EP8;
		prChipInfo->u2TxFwDlPort = USB_DATA_BULK_OUT_EP4;
		if (prChipInfo->is_support_wacpu)
			prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD;
		else
			prChipInfo->ucPacketFormat =
						TXD_PKT_FORMAT_TXD_PAYLOAD;
		prChipInfo->u4ExtraTxByteCount =
				EXTRA_TXD_SIZE_FOR_TX_BYTE_COUNT;
		prChipInfo->u4HifDmaShdlBaseAddr = USB_HIF_DMASHDL_BASE;

#if (CFG_ENABLE_FW_DOWNLOAD == 1)
		prChipInfo->asicEnableFWDownload = asicConnac2xEnableUsbFWDL;
#endif /* CFG_ENABLE_FW_DOWNLOAD == 1 */
		break;
#endif /* _HIF_USB */
#if defined(_HIF_SDIO)
	case MT_DEV_INF_SDIO:
		prChipInfo->u2HifTxdSize = SDIO_HIF_TXD_LEN;
		prChipInfo->fillHifTxDesc = fillSdioHifTxDesc;
		prChipInfo->u4ExtraTxByteCount =
				EXTRA_TXD_SIZE_FOR_TX_BYTE_COUNT;
		prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD_PAYLOAD;
		break;
#endif /* _HIF_SDIO */
	default:
		break;
	}
}

static void asicConnac2xFillInitCmdTxdInfo(
	struct ADAPTER *prAdapter,
	struct WIFI_CMD_INFO *prCmdInfo,
	u_int8_t *pucSeqNum)
{
	struct INIT_HIF_TX_HEADER *prInitHifTxHeader;
	struct HW_MAC_CONNAC2X_TX_DESC *prInitHifTxHeaderPadding;
	uint32_t u4TxdLen = sizeof(struct HW_MAC_CONNAC2X_TX_DESC);

	prInitHifTxHeaderPadding =
		(struct HW_MAC_CONNAC2X_TX_DESC *) (prCmdInfo->pucInfoBuffer);
	prInitHifTxHeader = (struct INIT_HIF_TX_HEADER *)
		(prCmdInfo->pucInfoBuffer + u4TxdLen);

	HAL_MAC_CONNAC2X_TXD_SET_TX_BYTE_COUNT(prInitHifTxHeaderPadding,
		prCmdInfo->u2InfoBufLen);
	if (!prCmdInfo->ucCID)
		HAL_MAC_CONNAC2X_TXD_SET_PKT_FORMAT(prInitHifTxHeaderPadding,
						INIT_PKT_FT_PDA_FWDL)
	else
		HAL_MAC_CONNAC2X_TXD_SET_PKT_FORMAT(prInitHifTxHeaderPadding,
						INIT_PKT_FT_CMD)
	HAL_MAC_CONNAC2X_TXD_SET_HEADER_FORMAT(prInitHifTxHeaderPadding,
						HEADER_FORMAT_COMMAND);
	prInitHifTxHeader->rInitWifiCmd.ucCID = prCmdInfo->ucCID;
	prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = prCmdInfo->ucPktTypeID;
	prInitHifTxHeader->rInitWifiCmd.ucSeqNum =
		nicIncreaseCmdSeqNum(prAdapter);
	prInitHifTxHeader->u2TxByteCount =
		prCmdInfo->u2InfoBufLen - u4TxdLen;

	if (pucSeqNum)
		*pucSeqNum = prInitHifTxHeader->rInitWifiCmd.ucSeqNum;

	DBGLOG(INIT, DEBUG, "TX CMD: ID[0x%02X] SEQ[%u] LEN[%u]\n",
			prInitHifTxHeader->rInitWifiCmd.ucCID,
			prInitHifTxHeader->rInitWifiCmd.ucSeqNum,
			prCmdInfo->u2InfoBufLen);
}


static void asicConnac2xFillCmdTxdInfo(
	struct ADAPTER *prAdapter,
	struct WIFI_CMD_INFO *prCmdInfo,
	u_int8_t *pucSeqNum)
{
	struct CONNAC2X_WIFI_CMD *prWifiCmd;
	uint32_t u4TxdLen = sizeof(struct HW_MAC_CONNAC2X_TX_DESC);

	prWifiCmd = (struct CONNAC2X_WIFI_CMD *)prCmdInfo->pucInfoBuffer;

	HAL_MAC_CONNAC2X_TXD_SET_TX_BYTE_COUNT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prWifiCmd,
		prCmdInfo->u2InfoBufLen);
	HAL_MAC_CONNAC2X_TXD_SET_PKT_FORMAT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prWifiCmd,
		INIT_PKT_FT_CMD);
	HAL_MAC_CONNAC2X_TXD_SET_HEADER_FORMAT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prWifiCmd,
		HEADER_FORMAT_COMMAND);

	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucExtenCID = prCmdInfo->ucExtCID;
	prWifiCmd->ucPktTypeID = prCmdInfo->ucPktTypeID;
	prWifiCmd->ucSetQuery = prCmdInfo->ucSetQuery;
	prWifiCmd->ucSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	prWifiCmd->ucS2DIndex = prCmdInfo->ucS2DIndex;
	prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen - u4TxdLen;

	if (pucSeqNum)
		*pucSeqNum = prWifiCmd->ucSeqNum;

	if (au2DebugModule[DBG_TX_IDX] & DBG_CLASS_TRACE)
		DBGLOG(TX, TRACE,
			"TX CMD: ID[0x%02X] SEQ[%u] SET[%u] LEN[%u]\n",
			prWifiCmd->ucCID, prWifiCmd->ucSeqNum,
			prWifiCmd->ucSetQuery, prCmdInfo->u2InfoBufLen);
	else
		DBGLOG_LIMITED(TX, DEBUG,
			"TX CMD: ID[0x%02X] SEQ[%u] SET[%u] LEN[%u]\n",
			prWifiCmd->ucCID, prWifiCmd->ucSeqNum,
			prWifiCmd->ucSetQuery, prCmdInfo->u2InfoBufLen);
}

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
static void asicConnac2xFillUniCmdTxdInfo(
	struct ADAPTER *prAdapter,
	struct WIFI_UNI_CMD_INFO *prCmdInfo,
	u_int8_t *pucSeqNum)
{
	struct CONNAC2X_WIFI_UNI_CMD *prWifiCmd;
	uint32_t u4TxdLen = sizeof(struct HW_MAC_CONNAC2X_TX_DESC);

	prWifiCmd = (struct CONNAC2X_WIFI_UNI_CMD *)prCmdInfo->pucInfoBuffer;

	HAL_MAC_CONNAC2X_TXD_SET_TX_BYTE_COUNT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prWifiCmd,
		prCmdInfo->u2InfoBufLen);
	HAL_MAC_CONNAC2X_TXD_SET_PKT_FORMAT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prWifiCmd,
		INIT_PKT_FT_CMD);
	HAL_MAC_CONNAC2X_TXD_SET_HEADER_FORMAT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prWifiCmd,
		HEADER_FORMAT_COMMAND);

	prWifiCmd->u2CID = prCmdInfo->u2CID;
	prWifiCmd->ucPktTypeID = prCmdInfo->ucPktTypeID;
	prWifiCmd->ucSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	prWifiCmd->ucS2DIndex = prCmdInfo->ucS2DIndex;
	prWifiCmd->u2Length = prCmdInfo->u2InfoBufLen - u4TxdLen;
	prWifiCmd->ucOption = prCmdInfo->ucOption;

	if (pucSeqNum)
		*pucSeqNum = prWifiCmd->ucSeqNum;

	DBGLOG(TX, TRACE, "TX CMD: ID[0x%04X] SEQ[%u] OPT[0x%02X] LEN[%u]\n",
			prWifiCmd->u2CID, prWifiCmd->ucSeqNum,
			prWifiCmd->ucOption, prCmdInfo->u2InfoBufLen);
}

#endif

void asicConnac2xFillInitCmdTxd(
	struct ADAPTER *prAdapter,
	struct WIFI_CMD_INFO *prCmdInfo,
	u_int16_t *pu2BufInfoLen,
	u_int8_t *pucSeqNum, void **pCmdBuf)
{
	struct INIT_HIF_TX_HEADER *prInitHifTxHeader;
	uint32_t u4TxdLen = sizeof(struct HW_MAC_CONNAC2X_TX_DESC);

	/* We don't need to append TXD while sending fw frames. */
	if (!prCmdInfo->ucCID && pCmdBuf)
		*pCmdBuf = prCmdInfo->pucInfoBuffer;
	else {
		prInitHifTxHeader = (struct INIT_HIF_TX_HEADER *)
			(prCmdInfo->pucInfoBuffer + u4TxdLen);
		asicConnac2xFillInitCmdTxdInfo(
			prAdapter,
			prCmdInfo,
			pucSeqNum);
		if (pCmdBuf)
			*pCmdBuf =
				&prInitHifTxHeader->rInitWifiCmd.aucBuffer[0];
	}
}
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
void asicConnac2xWfdmaRecord(
	struct ADAPTER *prAdapter)
{
#if defined(_HIF_AXI)
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	struct RTMP_TX_RING *prTxRing;
	u_int32_t u4Idx = 0;

	for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
		prTxRing = &prHifInfo->TxRing[u4Idx];
		if (!prTxRing->hw_cnt_addr)
			continue;
		HAL_MCR_RD(prAdapter, prTxRing->hw_cidx_addr,
			   &prTxRing->TxCpuIdxRec);
		HAL_MCR_RD(prAdapter, prTxRing->hw_didx_addr,
			   &prTxRing->TxDmaIdxRec);
	}
#endif
}

void asicConnac2xWfdmaChkIdxMisMatch(
	u_int32_t u4Idx, struct RTMP_TX_RING *prTxRing)
{
	if ((prTxRing->TxCpuIdx != prTxRing->TxCpuIdxRec) ||
		(prTxRing->TxDmaIdx != prTxRing->TxDmaIdxRec)) {
		DBGLOG(HAL, ERROR,
			"P[%u] Idx not match cidx[%u], cidxRec[%u], didx[%u], didxRec[%u]",
			u4Idx,
			prTxRing->TxCpuIdx, prTxRing->TxCpuIdxRec,
			prTxRing->TxDmaIdx, prTxRing->TxDmaIdxRec);
	}
}
#endif
void asicConnac2xWfdmaDummyCrRead(
	struct ADAPTER *prAdapter,
	u_int8_t *pfgResult)
{
#if defined(_HIF_AXI)
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	struct RTMP_TX_RING *prTxRing;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;
	u_int32_t u4Idx = 0;
#endif
	u_int32_t u4RegValue = 0;

	HAL_MCR_RD(prAdapter,
		CONNAC2X_WFDMA_DUMMY_CR,
		&u4RegValue);

#if defined(_HIF_AXI)
	*pfgResult = TRUE;
	for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
		prTxRing = &prHifInfo->TxRing[u4Idx];
		if (!prTxRing->hw_cnt_addr)
			continue;
		HAL_MCR_RD(prAdapter, prTxRing->hw_cidx_addr,
			   &prTxRing->TxCpuIdx);
		HAL_MCR_RD(prAdapter, prTxRing->hw_didx_addr,
			   &prTxRing->TxDmaIdx);
		if (prTxRing->TxCpuIdx != 0 ||
			prTxRing->TxDmaIdx != 0) {
			*pfgResult = FALSE;

			if (prBusInfo->checkIdxMismatch)
				prBusInfo->checkIdxMismatch(u4Idx, prTxRing);
		}
	}

	if (*pfgResult) {
		DBGLOG(HAL, DEBUG,
			"CpuIdx == 0, DmdIdx == 0, DmyCr[0x%08x]",
			u4RegValue);
	}
#else /* !_HIF_AXI */
	*pfgResult = (u4RegValue & CONNAC2X_WFDMA_NEED_REINIT_BIT) == 0 ?
		TRUE : FALSE;
#endif /* !_HIF_AXI */
}

void asicConnac2xWfdmaDummyCrWrite(
	struct ADAPTER *prAdapter)
{
	u_int32_t u4RegValue = 0;

	HAL_MCR_RD(prAdapter,
		CONNAC2X_WFDMA_DUMMY_CR,
		&u4RegValue);
	u4RegValue |= CONNAC2X_WFDMA_NEED_REINIT_BIT;

	HAL_MCR_WR(prAdapter,
		CONNAC2X_WFDMA_DUMMY_CR,
		u4RegValue);
}

u_int8_t asicConnac2xWfdmaIsNeedReInit(
	struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	u_int8_t fgNeedReInit;

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->asicWfdmaReInit == NULL)
		return FALSE;

	/* for bus hang debug purpose */
	if (prAdapter->chip_info->checkbusNoAck)
		prAdapter->chip_info->checkbusNoAck((void *) prAdapter, TRUE);

	asicConnac2xWfdmaDummyCrRead(prAdapter, &fgNeedReInit);

	return fgNeedReInit;
}

static void asicConnac2xWfdmaReInitImpl(struct ADAPTER *prAdapter)
{
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
#if CFG_MTK_WIFI_WFDMA_BK_RS
	{
		struct BUS_INFO *prBusInfo;
#if CFG_MTK_WIFI_SW_WFDMA
		struct SW_WFDMA_INFO *prSwWfdmaInfo;
#endif /* CFG_MTK_WIFI_SW_WFDMA */
		struct GL_HIF_INFO *prHifInfo;
		uint32_t u4Idx;

		prBusInfo = prAdapter->chip_info->bus_info;
#if CFG_MTK_WIFI_SW_WFDMA
		prSwWfdmaInfo = &prBusInfo->rSwWfdmaInfo;
#endif /* CFG_MTK_WIFI_SW_WFDMA */

		DBGLOG(INIT, TRACE,
			"WFDMA reinit after bk/sr(deep sleep)\n");
		prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
		for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
#if CFG_MTK_WIFI_SW_WFDMA
			/* Swwfdma should not reset txring */
			if (prSwWfdmaInfo->fgIsEnSwWfdma &&
			    u4Idx == prSwWfdmaInfo->u4PortIdx)
				continue;
#endif /* CFG_MTK_WIFI_SW_WFDMA */

			prHifInfo->TxRing[u4Idx].TxSwUsedIdx = 0;
			prHifInfo->TxRing[u4Idx].u4UsedCnt = 0;
			prHifInfo->TxRing[u4Idx].TxCpuIdx = 0;
		}

		if (halWpdmaGetRxDmaDoneCnt(prAdapter->prGlueInfo,
					    RX_RING_EVT)) {
			KAL_SET_BIT(RX_RING_EVT,
				prAdapter->ulNoMoreRfb);
		}
	}
#else /* CFG_MTK_WIFI_WFDMA_BK_RS */
	DBGLOG(INIT, DEBUG, "WFDMA reinit due to deep sleep\n");
	halWpdmaInitRing(prAdapter->prGlueInfo, true);
#endif /* CFG_MTK_WIFI_WFDMA_BK_RS */
#elif defined(_HIF_USB)
	{
		struct mt66xx_chip_info *prChipInfo = NULL;

		prChipInfo = prAdapter->chip_info;
		if (prChipInfo->is_support_asic_lp &&
		    prChipInfo->asicUsbInit)
			prChipInfo->asicUsbInit(prAdapter, prChipInfo);

		if (prChipInfo->is_support_wacpu) {
			/* command packet forward to TX ring 17 (WMCPU) or
			 *	  TX ring 20 (WACPU)
			 */
			asicConnac2xEnableUsbCmdTxRing(prAdapter,
				CONNAC2X_USB_CMDPKT2WA);
		}
	}
#endif
}

void asicConnac2xWfdmaReInit(
	struct ADAPTER *prAdapter)
{
	u_int8_t fgResult;

	/*WFDMA re-init flow after chip deep sleep*/
	asicConnac2xWfdmaDummyCrRead(prAdapter, &fgResult);
	if (fgResult) {
		asicConnac2xWfdmaReInitImpl(prAdapter);
		asicConnac2xWfdmaDummyCrWrite(prAdapter);
	}
}

void asicConnac2xFillCmdTxd(
	struct ADAPTER *prAdapter,
	struct WIFI_CMD_INFO *prCmdInfo,
	u_int8_t *pucSeqNum,
	void **pCmdBuf)
{
	struct CONNAC2X_WIFI_CMD *prWifiCmd;

	/* 2. Setup common CMD Info Packet */
	prWifiCmd = (struct CONNAC2X_WIFI_CMD *)prCmdInfo->pucInfoBuffer;
	asicConnac2xFillCmdTxdInfo(prAdapter, prCmdInfo, pucSeqNum);
	if (pCmdBuf)
		*pCmdBuf = &prWifiCmd->aucBuffer[0];
}

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
void asicConnac2xFillUniCmdTxd(
	struct ADAPTER *prAdapter,
	struct WIFI_UNI_CMD_INFO *prCmdInfo,
	u_int8_t *pucSeqNum,
	void **pCmdBuf)
{
	struct CONNAC2X_WIFI_UNI_CMD *prWifiCmd;

	/* 2. Setup common CMD Info Packet */
	prWifiCmd = (struct CONNAC2X_WIFI_UNI_CMD *)prCmdInfo->pucInfoBuffer;
	asicConnac2xFillUniCmdTxdInfo(prAdapter, prCmdInfo, pucSeqNum);
	if (pCmdBuf)
		*pCmdBuf = &prWifiCmd->aucBuffer[0];
}
#endif

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
uint32_t asicConnac2xWfdmaCfgAddrGet(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t ucDmaIdx)
{
	struct BUS_INFO *prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (ucDmaIdx == 0)
		return CONNAC2X_WPDMA_GLO_CFG(prBusInfo->host_dma0_base);
	else
		return CONNAC2X_WPDMA_GLO_CFG(prBusInfo->host_dma1_base);
}

uint32_t asicConnac2xWfdmaIntRstDtxPtrAddrGet(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t ucDmaIdx)
{
	struct BUS_INFO *prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (ucDmaIdx == 0)
		return CONNAC2X_WPDMA_RST_DTX_PTR(prBusInfo->host_dma0_base);
	else
		return CONNAC2X_WPDMA_RST_DTX_PTR(prBusInfo->host_dma1_base);
}

uint32_t asicConnac2xWfdmaIntRstDrxPtrAddrGet(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t ucDmaIdx)
{
	struct BUS_INFO *prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (ucDmaIdx == 0)
		return CONNAC2X_WPDMA_RST_DRX_PTR(prBusInfo->host_dma0_base);
	else
		return CONNAC2X_WPDMA_RST_DRX_PTR(prBusInfo->host_dma1_base);
}

uint32_t asicConnac2xWfdmaHifRstAddrGet(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t ucDmaIdx)
{
	struct BUS_INFO *prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (ucDmaIdx == 0)
		return CONNAC2X_WPDMA_HIF_RST(prBusInfo->host_dma0_base);
	else
		return CONNAC2X_WPDMA_HIF_RST(prBusInfo->host_dma1_base);
}

void asicConnac2xWfdmaControl(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t ucDmaIdx,
	u_int8_t enable)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	union WPDMA_GLO_CFG_STRUCT GloCfg;
	uint32_t u4DmaCfgCr;
	uint32_t u4DmaRstDtxPtrCr;
	uint32_t u4DmaRstDrxPtrCr;

	ASSERT(ucDmaIdx < CONNAC2X_MAX_WFDMA_COUNT);
	u4DmaCfgCr = asicConnac2xWfdmaCfgAddrGet(prGlueInfo, ucDmaIdx);
	u4DmaRstDtxPtrCr =
		asicConnac2xWfdmaIntRstDtxPtrAddrGet(prGlueInfo, ucDmaIdx);
	u4DmaRstDrxPtrCr =
		asicConnac2xWfdmaIntRstDrxPtrAddrGet(prGlueInfo, ucDmaIdx);

	HAL_MCR_RD(prAdapter, u4DmaCfgCr, &GloCfg.word);
	if (enable == TRUE) {
		GloCfg.field_conn2x.pdma_bt_size = 3;
		GloCfg.field_conn2x.tx_wb_ddone = 1;
		GloCfg.field_conn2x.fifo_little_endian = 1;
		GloCfg.field_conn2x.clk_gate_dis = 1;
		GloCfg.field_conn2x.omit_tx_info = 1;
		if (ucDmaIdx == 1)
			GloCfg.field_conn2x.omit_rx_info = 1;
		GloCfg.field_conn2x.csr_disp_base_ptr_chain_en = 1;
		GloCfg.field_conn2x.omit_rx_info_pfet2 = 1;
	} else {
		GloCfg.field_conn2x.tx_dma_en = 0;
		GloCfg.field_conn2x.rx_dma_en = 0;
		GloCfg.field_conn2x.csr_disp_base_ptr_chain_en = 0;
		GloCfg.field_conn2x.omit_tx_info = 0;
		GloCfg.field_conn2x.omit_rx_info = 0;
		GloCfg.field_conn2x.omit_rx_info_pfet2 = 0;
	}
	HAL_MCR_WR(prAdapter, u4DmaCfgCr, GloCfg.word);

	if (!enable) {
		asicConnac2xWfdmaWaitIdle(prGlueInfo, ucDmaIdx, 100, 1000);
		/* remove Reset DMA Index */
	}
}

void asicConnac2xWfdmaStop(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t enable)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	union WPDMA_GLO_CFG_STRUCT GloCfg;
	uint32_t u4DmaCfgCr;
	uint32_t idx;
	struct mt66xx_chip_info *chip_info = prAdapter->chip_info;

	for (idx = 0; idx < CONNAC2X_MAX_WFDMA_COUNT; idx++) {
		if (!chip_info->is_support_wfdma1 && idx)
			break;
		u4DmaCfgCr = asicConnac2xWfdmaCfgAddrGet(prGlueInfo, idx);
		HAL_MCR_RD(prAdapter, u4DmaCfgCr, &GloCfg.word);

		if (enable == TRUE) {
			GloCfg.field_conn2x.tx_dma_en = 0;
			GloCfg.field_conn2x.rx_dma_en = 0;
		} else {
			GloCfg.field_conn2x.tx_dma_en = 1;
			GloCfg.field_conn2x.rx_dma_en = 1;
		}

		HAL_MCR_WR(prAdapter, u4DmaCfgCr, GloCfg.word);
	}

}

void asicConnac2xWpdmaConfig(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t enable,
	bool fgResetHif)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	union WPDMA_GLO_CFG_STRUCT GloCfg[CONNAC2X_MAX_WFDMA_COUNT] = {0};
	uint32_t u4DmaCfgCr;
	uint32_t idx, u4DmaNum = 1;
	struct mt66xx_chip_info *chip_info = prAdapter->chip_info;

	if (chip_info->is_support_wfdma1)
		u4DmaNum++;

	for (idx = 0; idx < u4DmaNum; idx++) {
		asicConnac2xWfdmaControl(prGlueInfo, idx, enable);
		u4DmaCfgCr = asicConnac2xWfdmaCfgAddrGet(prGlueInfo, idx);
		HAL_MCR_RD(prAdapter, u4DmaCfgCr, &GloCfg[idx].word);
	}

	if (enable) {
		for (idx = 0; idx < u4DmaNum; idx++) {
			u4DmaCfgCr =
				asicConnac2xWfdmaCfgAddrGet(prGlueInfo, idx);
			GloCfg[idx].field_conn2x.tx_dma_en = 1;
			GloCfg[idx].field_conn2x.rx_dma_en = 1;
			HAL_MCR_WR(prAdapter, u4DmaCfgCr, GloCfg[idx].word);
		}
	}
}

u_int8_t asicConnac2xWfdmaWaitIdle(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t index,
	uint32_t round,
	uint32_t wait_us)
{
	uint32_t i = 0;
	uint32_t u4RegAddr = 0;
	union WPDMA_GLO_CFG_STRUCT GloCfg = {0};
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;

	if (index == 0 || index == 1)
		u4RegAddr = asicConnac2xWfdmaCfgAddrGet(prGlueInfo, index);
	else {
		DBGLOG(HAL, ERROR, "Unknown wfdma index(=%d)\n", index);
		return FALSE;
	}

	do {
		HAL_MCR_RD(prAdapter, u4RegAddr, &GloCfg.word);
		if ((GloCfg.field_conn2x.tx_dma_busy == 0) &&
		    (GloCfg.field_conn2x.rx_dma_busy == 0)) {
			DBGLOG(HAL, TRACE, "==>  DMAIdle, GloCfg=0x%x\n",
			       GloCfg.word);
			return TRUE;
		}
		kalUdelay(wait_us);
	} while ((i++) < round);

	DBGLOG(HAL, DEBUG, "==>  DMABusy, GloCfg=0x%x\n", GloCfg.word);

	return FALSE;
}

void asicConnac2xWfdmaTxRingBasePtrExtCtrl(
	struct GLUE_INFO *prGlueInfo,
	struct RTMP_TX_RING *tx_ring,
	u_int32_t index)
{
	struct BUS_INFO *prBusInfo;
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	uint32_t phy_addr_ext = 0;
	u_int32_t u4RegValue = 0;

	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (prBusInfo->u4DmaMask <= 32)
		return;

	phy_addr_ext = (((uint64_t)tx_ring->Cell[0].AllocPa >>
			DMA_BITS_OFFSET) & DMA_HIGHER_4BITS_MASK) << 16;

	HAL_MCR_RD(prAdapter, tx_ring->hw_cnt_addr,
			&u4RegValue);

	phy_addr_ext |= u4RegValue;
	DBGLOG(HAL, TRACE, "phy_addr_ext=0x%x\n", phy_addr_ext);

	HAL_MCR_WR(prAdapter, tx_ring->hw_cnt_addr,
			phy_addr_ext);
}

void asicConnac2xWfdmaRxRingBasePtrExtCtrl(
	struct GLUE_INFO *prGlueInfo,
	struct RTMP_RX_RING *rx_ring,
	u_int32_t index)
{
	struct BUS_INFO *prBusInfo;
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	uint32_t phy_addr_ext = 0;
	u_int32_t u4RegValue = 0;

	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (prBusInfo->u4DmaMask <= 32)
		return;

	phy_addr_ext = (((uint64_t)rx_ring->Cell[0].AllocPa >>
			DMA_BITS_OFFSET) & DMA_HIGHER_4BITS_MASK) << 16;

	HAL_MCR_RD(prAdapter, rx_ring->hw_cnt_addr,
			&u4RegValue);

	phy_addr_ext |= u4RegValue;
	DBGLOG(HAL, TRACE, "phy_addr_ext=0x%x\n", phy_addr_ext);

	HAL_MCR_WR(prAdapter, rx_ring->hw_cnt_addr,
			phy_addr_ext);
}

u_int8_t asicConnac2xWfdmaPollingAllIdle(
	struct GLUE_INFO *prGlueInfo)
{
	u_int8_t ucDmaIdx;

	for (ucDmaIdx = 0; ucDmaIdx < CONNAC2X_MAX_WFDMA_COUNT; ucDmaIdx++) {
		if (asicConnac2xWfdmaWaitIdle(prGlueInfo, ucDmaIdx, 100, 1000)
			== FALSE) {
			DBGLOG(HAL, WARN,
				"Polling PDMA idle Timeout!!DmaIdx:%d\n",
				ucDmaIdx);
			return FALSE;
		}
	}

	return TRUE;
}

void asicConnac2xWfdmaTxRingExtCtrl(
	struct GLUE_INFO *prGlueInfo,
	struct RTMP_TX_RING *tx_ring,
	u_int32_t index)
{
	struct BUS_INFO *prBusInfo;
	uint32_t ext_offset = 0;
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prGlueInfo->prAdapter->chip_info;
	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (index == TX_RING_CMD)
		ext_offset = prBusInfo->tx_ring_cmd_idx * 4;
	else if (index == TX_RING_FWDL)
		ext_offset = prBusInfo->tx_ring_fwdl_idx * 4;
	else if (prChipInfo->is_support_wacpu) {
		if (index == TX_RING_DATA0)
			ext_offset = prBusInfo->tx_ring0_data_idx * 4;
		if (index == TX_RING_DATA1)
			ext_offset = prBusInfo->tx_ring1_data_idx * 4;
		if (index == TX_RING_WA_CMD)
			ext_offset = prBusInfo->tx_ring_wa_cmd_idx * 4;
	} else
		ext_offset = index * 4;

	tx_ring->hw_desc_base_ext =
		prBusInfo->host_tx_ring_ext_ctrl_base + ext_offset;
	HAL_MCR_WR(prAdapter, tx_ring->hw_desc_base_ext,
		   CONNAC2X_TX_RING_DISP_MAX_CNT);

	asicConnac2xWfdmaTxRingBasePtrExtCtrl(prGlueInfo,
		tx_ring, index);
}

void asicConnac2xWfdmaRxRingExtCtrl(
	struct GLUE_INFO *prGlueInfo,
	struct RTMP_RX_RING *rx_ring,
	u_int32_t index)
{
	struct ADAPTER *prAdapter;
	struct mt66xx_chip_info *prChipInfo;
	struct BUS_INFO *prBusInfo;
	uint32_t ext_offset;
	bool fgIsWfdma1 = false;

	prAdapter = prGlueInfo->prAdapter;
	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	fgIsWfdma1 = prChipInfo->is_support_wfdma1 ||
		prChipInfo->is_support_wacpu;

	switch (index) {
	case RX_RING_EVT:
		ext_offset = 0;
		break;
	case RX_RING_DATA0:
		ext_offset = fgIsWfdma1 ? 0 : (index + 2) * 4;
		break;
	case RX_RING_DATA1:
	case RX_RING_TXDONE0:
	case RX_RING_TXDONE1:
		ext_offset = fgIsWfdma1 ? (index - 1) * 4 : (index + 1) * 4;
		break;
	case RX_RING_WAEVT0:
	case RX_RING_WAEVT1:
		ext_offset = (index - 4) * 4;
		break;
	default:
		DBGLOG(RX, ERROR, "Error index=%d\n", index);
		return;
	}

	rx_ring->hw_desc_base_ext = fgIsWfdma1 ?
		prBusInfo->host_wfdma1_rx_ring_ext_ctrl_base + ext_offset :
		prBusInfo->host_rx_ring_ext_ctrl_base + ext_offset;

	HAL_MCR_WR(prAdapter, rx_ring->hw_desc_base_ext,
		   CONNAC2X_RX_RING_DISP_MAX_CNT);

	asicConnac2xWfdmaRxRingBasePtrExtCtrl(prGlueInfo,
		rx_ring, index);
}

void asicConnac2xWfdmaManualPrefetch(
	struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	u_int32_t val = 0;

	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &val);
	/* disable prefetch offset calculation auto-mode */
	val &=
	~WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN_MASK;
	HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, val);

	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA1_WPDMA_GLO_CFG_ADDR, &val);
	/* disable prefetch offset calculation auto-mode */
	val &=
	~WF_WFDMA_HOST_DMA1_WPDMA_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN_MASK;
	HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA1_WPDMA_GLO_CFG_ADDR, val);


	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_EXT_CTRL_ADDR, 0x00000004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING1_EXT_CTRL_ADDR, 0x00400004);

	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING0_EXT_CTRL_ADDR, 0x00800004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING1_EXT_CTRL_ADDR, 0x00c00004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING2_EXT_CTRL_ADDR, 0x01000004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING3_EXT_CTRL_ADDR, 0x01400004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING4_EXT_CTRL_ADDR, 0x01800004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING5_EXT_CTRL_ADDR, 0x01c00004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING6_EXT_CTRL_ADDR, 0x02000004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING7_EXT_CTRL_ADDR, 0x02400004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING16_EXT_CTRL_ADDR, 0x02800004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING17_EXT_CTRL_ADDR, 0x02c00004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING18_EXT_CTRL_ADDR, 0x03000004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING19_EXT_CTRL_ADDR, 0x03400004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING20_EXT_CTRL_ADDR, 0x03800004);

	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_RX_RING0_EXT_CTRL_ADDR, 0x03c00004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_RX_RING1_EXT_CTRL_ADDR, 0x04000004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_RX_RING2_EXT_CTRL_ADDR, 0x04400004);

	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING2_EXT_CTRL_ADDR, 0x04800004);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING3_EXT_CTRL_ADDR, 0x04c00004);

	/* reset dma idx */
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RST_DTX_PTR_ADDR, 0xFFFFFFFF);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA1_WPDMA_RST_DTX_PTR_ADDR, 0xFFFFFFFF);
}

void asicConnac2xEnablePlatformIRQ(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(prAdapter);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	enable_irq(prHifInfo->u4IrqId);
}

void asicConnac2xDisablePlatformIRQ(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(prAdapter);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	disable_irq_nosync(prHifInfo->u4IrqId);
}

#if defined(_HIF_AXI)
void asicConnac2xDisablePlatformSwIRQ(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(prAdapter);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	disable_irq_nosync(prHifInfo->u4IrqId_1);
}
#endif

void asicConnac2xEnableExtInterrupt(
	struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	union WPDMA_INT_MASK IntMask;
	uint32_t u4HostWpdamBase = 0;

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->is_support_wfdma1)
		u4HostWpdamBase = CONNAC2X_HOST_WPDMA_1_BASE;
	else
		u4HostWpdamBase = CONNAC2X_HOST_WPDMA_0_BASE;

	IntMask.word = 0;
	IntMask.field_conn2x_ext.wfdma0_rx_done_0 = 1;
	IntMask.field_conn2x_ext.wfdma0_rx_done_1 = 1;
	IntMask.field_conn2x_ext.wfdma0_rx_done_2 = 1;
	IntMask.field_conn2x_ext.wfdma0_rx_done_3 = 1;
	IntMask.field_conn2x_ext.wfdma1_rx_done_0 = 1;
	IntMask.field_conn2x_ext.wfdma1_rx_done_1 = 1;
	IntMask.field_conn2x_ext.wfdma1_rx_done_2 = 1;

	IntMask.field_conn2x_ext.wfdma1_tx_done_0 = 1;
	/*IntMask.field_conn2x_ext.wfdma1_tx_done_1 = 1;*/
	IntMask.field_conn2x_ext.wfdma1_tx_done_16 = 1;
	IntMask.field_conn2x_ext.wfdma1_tx_done_17 = 1;
	IntMask.field_conn2x_ext.wfdma1_tx_done_18 = 1;
	IntMask.field_conn2x_ext.wfdma1_tx_done_19 = 1;
	IntMask.field_conn2x_ext.wfdma1_tx_done_20 = 1;

	IntMask.field_conn2x_ext.wfdma0_rx_coherent = 0;
	IntMask.field_conn2x_ext.wfdma0_tx_coherent = 0;
	IntMask.field_conn2x_ext.wfdma1_rx_coherent = 0;
	IntMask.field_conn2x_ext.wfdma1_tx_coherent = 0;

	IntMask.field_conn2x_ext.wfdma1_mcu2host_sw_int_en = 1;

	HAL_MCR_WR(prAdapter,
		CONNAC2X_WPDMA_EXT_INT_MASK(CONNAC2X_HOST_EXT_CONN_HIF_WRAP),
		IntMask.word);
	HAL_MCR_RD(prAdapter,
		CONNAC2X_WPDMA_EXT_INT_MASK(CONNAC2X_HOST_EXT_CONN_HIF_WRAP),
		&IntMask.word);
	DBGLOG(HAL, TRACE, "%s [0x%08x]\n", __func__, IntMask.word);

	if (prAdapter->chip_info->is_support_wfdma1) {
		/* WFDMA issue workaround :
		 * enable TX RX priority interrupt sel 7c025298/7c02529c
		 * to force writeback clear int_stat
		*/
		HAL_MCR_WR(prAdapter,
			(CONNAC2X_HOST_WPDMA_1_BASE + 0x29c), 0x000f00ff);
		HAL_MCR_WR(prAdapter,
			(CONNAC2X_HOST_WPDMA_1_BASE + 0x298), 0xf);
	}

	if (prChipInfo->is_support_asic_lp)
		HAL_MCR_WR_FIELD(prAdapter,
				 CONNAC2X_WPDMA_MCU2HOST_SW_INT_MASK
				 (u4HostWpdamBase),
				 BITS(0, 15),
				 0,
				 BITS(0, 15));
}	/* end of nicEnableInterrupt() */

void asicConnac2xDisableExtInterrupt(
	struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	union WPDMA_INT_MASK IntMask;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	IntMask.word = 0;

	HAL_MCR_WR(prAdapter,
		CONNAC2X_WPDMA_EXT_INT_MASK(CONNAC2X_HOST_EXT_CONN_HIF_WRAP),
		IntMask.word);
	HAL_MCR_RD(prAdapter,
		CONNAC2X_WPDMA_EXT_INT_MASK(CONNAC2X_HOST_EXT_CONN_HIF_WRAP),
		&IntMask.word);

	DBGLOG(HAL, TRACE, "%s\n", __func__);

}

void asicConnac2xProcessTxInterrupt(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	union WPDMA_INT_STA_STRUCT rIntrStatus;

	rIntrStatus = (union WPDMA_INT_STA_STRUCT)prHifInfo->u4IntStatus;
	if (rIntrStatus.field_conn2x_ext.wfdma1_tx_done_16)
		halWpdmaProcessCmdDmaDone(prAdapter->prGlueInfo,
			TX_RING_FWDL);

	if (rIntrStatus.field_conn2x_ext.wfdma1_tx_done_17)
		halWpdmaProcessCmdDmaDone(prAdapter->prGlueInfo,
			TX_RING_CMD);

	if (rIntrStatus.field_conn2x_ext.wfdma1_tx_done_20)
		halWpdmaProcessCmdDmaDone(prAdapter->prGlueInfo,
			TX_RING_WA_CMD);

	if (rIntrStatus.field_conn2x_ext.wfdma1_tx_done_18) {
		halWpdmaProcessDataDmaDone(prAdapter->prGlueInfo,
			TX_RING_DATA0);
#if CFG_SUPPORT_MULTITHREAD
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
#endif
	}

	if (rIntrStatus.field_conn2x_ext.wfdma1_tx_done_19) {
		halWpdmaProcessDataDmaDone(prAdapter->prGlueInfo,
			TX_RING_DATA1);
#if CFG_SUPPORT_MULTITHREAD
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
#endif
	}
}

void asicConnac2xLowPowerOwnRead(
	struct ADAPTER *prAdapter,
	u_int8_t *pfgResult)
{
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->is_support_asic_lp) {
		u_int32_t u4RegValue = 0;

		HAL_MCR_RD(prAdapter,
				CONNAC2X_BN0_LPCTL_ADDR,
				&u4RegValue);
		*pfgResult = (u4RegValue &
				PCIE_LPCR_AP_HOST_OWNER_STATE_SYNC)
				== 0 ? TRUE : FALSE;
	} else
		*pfgResult = TRUE;
}

void asicConnac2xLowPowerOwnSet(
	struct ADAPTER *prAdapter,
	u_int8_t *pfgResult)
{
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->is_support_asic_lp) {
		u_int32_t u4RegValue = 0;

		HAL_MCR_WR(prAdapter,
				CONNAC2X_BN0_LPCTL_ADDR,
				PCIE_LPCR_HOST_SET_OWN);
		HAL_MCR_RD(prAdapter,
				CONNAC2X_BN0_LPCTL_ADDR,
				&u4RegValue);
		*pfgResult = (u4RegValue &
			PCIE_LPCR_AP_HOST_OWNER_STATE_SYNC) == 0x4;
	} else
		*pfgResult = TRUE;
}

void asicConnac2xLowPowerOwnClear(
	struct ADAPTER *prAdapter,
	u_int8_t *pfgResult)
{
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->is_support_asic_lp) {
		u_int32_t u4RegValue = 0;

		HAL_MCR_WR(prAdapter,
			CONNAC2X_BN0_LPCTL_ADDR,
			PCIE_LPCR_HOST_CLR_OWN);
		HAL_MCR_RD(prAdapter,
			CONNAC2X_BN0_LPCTL_ADDR,
			&u4RegValue);
		*pfgResult = (u4RegValue &
				PCIE_LPCR_AP_HOST_OWNER_STATE_SYNC) == 0;
	} else
		*pfgResult = TRUE;
}

void asicConnac2xProcessSoftwareInterrupt(
	struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_HIF_INFO *prHifInfo;
	struct ERR_RECOVERY_CTRL_T *prErrRecoveryCtrl;
	uint32_t u4Status = 0;
	uint32_t u4HostWpdamBase = 0;

	if (prAdapter->prGlueInfo == NULL) {
		DBGLOG(HAL, ERROR, "prGlueInfo is NULL\n");
		return;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;
	prErrRecoveryCtrl = &prHifInfo->rErrRecoveryCtl;

	if (prAdapter->chip_info->is_support_wfdma1)
		u4HostWpdamBase = CONNAC2X_HOST_WPDMA_1_BASE;
	else
		u4HostWpdamBase = CONNAC2X_HOST_WPDMA_0_BASE;

	HAL_MCR_RD(prAdapter,
		CONNAC2X_WPDMA_MCU2HOST_SW_INT_STA(u4HostWpdamBase),
		&u4Status);

	prErrRecoveryCtrl->u4BackupStatus = u4Status;
	if (u4Status & ERROR_DETECT_MASK) {
		prErrRecoveryCtrl->u4Status = u4Status;
		kalDevRegWrite(prGlueInfo,
			CONNAC2X_WPDMA_MCU2HOST_SW_INT_STA(u4HostWpdamBase),
			u4Status);
		halHwRecoveryFromError(prAdapter);
	} else {
		kalDevRegWrite(prGlueInfo,
			CONNAC2X_WPDMA_MCU2HOST_SW_INT_STA(u4HostWpdamBase),
			u4Status);
		DBGLOG(HAL, TRACE, "undefined SER status[0x%x].\n", u4Status);
	}
}


void asicConnac2xSoftwareInterruptMcu(
	struct ADAPTER *prAdapter, u_int32_t intrBitMask)
{
	struct GLUE_INFO *prGlueInfo;
	uint32_t u4McuWpdamBase = 0;

	if (prAdapter == NULL || prAdapter->prGlueInfo == NULL) {
		DBGLOG(HAL, ERROR, "prAdapter or prGlueInfo is NULL\n");
		return;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	if (prAdapter->chip_info->is_support_wfdma1)
		u4McuWpdamBase = CONNAC2X_MCU_WPDMA_1_BASE;
	else
		u4McuWpdamBase = CONNAC2X_MCU_WPDMA_0_BASE;
	kalDevRegWrite(prGlueInfo,
		CONNAC2X_WPDMA_HOST2MCU_SW_INT_SET(u4McuWpdamBase),
		intrBitMask);
}


void asicConnac2xHifRst(
	struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4HifRstCr;

	u4HifRstCr = asicConnac2xWfdmaHifRstAddrGet(prGlueInfo, 0);
	/* Reset dmashdl and wpdma */
	kalDevRegWrite(prGlueInfo, u4HifRstCr, 0x00000000);
	kalDevRegWrite(prGlueInfo, u4HifRstCr, 0x00000030);

	u4HifRstCr = asicConnac2xWfdmaHifRstAddrGet(prGlueInfo, 1);
	/* Reset dmashdl and wpdma */
	kalDevRegWrite(prGlueInfo, u4HifRstCr, 0x00000000);
	kalDevRegWrite(prGlueInfo, u4HifRstCr, 0x00000030);
}

void asicConnac2xReadExtIntStatus(
	struct ADAPTER *prAdapter,
	uint32_t *pu4IntStatus)
{
	uint32_t u4RegValue = 0;
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	*pu4IntStatus = 0;

	HAL_MCR_RD(prAdapter,
		CONNAC2X_WPDMA_EXT_INT_STA(
			prBusInfo->host_ext_conn_hif_wrap_base),
		&u4RegValue);

	if (HAL_IS_CONNAC2X_EXT_RX_DONE_INTR(u4RegValue,
		prBusInfo->host_int_rxdone_bits))
		*pu4IntStatus |= WHISR_RX0_DONE_INT;

	if (HAL_IS_CONNAC2X_EXT_TX_DONE_INTR(u4RegValue,
		prBusInfo->host_int_txdone_bits))
		*pu4IntStatus |= WHISR_TX_DONE_INT;

	if (u4RegValue & CONNAC_MCU_SW_INT)
		*pu4IntStatus |= WHISR_D2H_SW_INT;

	prHifInfo->u4IntStatus = u4RegValue;

	/* clear interrupt */
	HAL_MCR_WR(prAdapter,
		CONNAC2X_WPDMA_EXT_INT_STA(
			prBusInfo->host_ext_conn_hif_wrap_base),
		u4RegValue);
}

void asicConnac2xProcessRxInterrupt(
	struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	union WPDMA_INT_STA_STRUCT rIntrStatus;

	rIntrStatus = (union WPDMA_INT_STA_STRUCT)prHifInfo->u4IntStatus;
	if (rIntrStatus.field_conn2x_ext.wfdma1_rx_done_0)
		halRxReceiveRFBs(prAdapter, RX_RING_EVT, FALSE);
	if (rIntrStatus.field_conn2x_ext.wfdma1_rx_done_1)
		halRxReceiveRFBs(prAdapter, RX_RING_WAEVT0, FALSE);
	if (rIntrStatus.field_conn2x_ext.wfdma1_rx_done_2)
		halRxReceiveRFBs(prAdapter, RX_RING_WAEVT1, FALSE);
	if (rIntrStatus.field_conn2x_ext.wfdma0_rx_done_0)
		halRxReceiveRFBs(prAdapter, RX_RING_DATA0, TRUE);
	if (rIntrStatus.field_conn2x_ext.wfdma0_rx_done_1)
		halRxReceiveRFBs(prAdapter, RX_RING_DATA1, TRUE);
	if (rIntrStatus.field_conn2x_ext.wfdma0_rx_done_2)
		halRxReceiveRFBs(prAdapter, RX_RING_TXDONE0, TRUE);
	if (rIntrStatus.field_conn2x_ext.wfdma0_rx_done_3)
		halRxReceiveRFBs(prAdapter, RX_RING_TXDONE1, TRUE);
}
#endif /* _HIF_PCIE */

#if defined(_HIF_USB)
/*
 * tx_ring config
 * 1.1tx_ring_ext_ctrl
 * 7c025600[31:0]: 0x00800004 (ring 0 BASE_PTR & max_cnt for EP4)
 * 7c025604[31:0]: 0x00c00004 (ring 1 BASE_PTR & max_cnt for EP5)
 * 7c025608[31:0]: 0x01000004 (ring 2 BASE_PTR & max_cnt for EP6)
 * 7c02560c[31:0]: 0x01400004 (ring 3 BASE_PTR & max_cnt for EP7)
 * 7c025610[31:0]: 0x01800004 (ring 4 BASE_PTR & max_cnt for EP9)
 * 7c025640[31:0]: 0x02800004 (ring 16 BASE_PTR & max_cnt for EP4/FWDL)
 * 7c025644[31:0]: 0x02c00004 (ring 17 BASE_PTR & max_cnt for EP8/WMCPU)
 * 7c025650[31:0]: 0x03800004 (ring 20 BASE_PTR & max_cnt for EP8/WACPU)
 *
 * WFDMA_GLO_CFG Setting
 * 2.1 WFDMA_GLO_CFG: 7c025208[28][27]=2'b11;
 * 2.2 WFDMA_GLO_CFG: 7c025208[20]=1'b1;
 * 2.3 WFDMA_GLO_CFG: 7c025208[9]=1'b1;
 *
 * 3.	trx_dma_en:
 * 3.1 WFDMA_GLO_CFG: 7c025208[2][0]=1'b1;
 */
void asicConnac2xWfdmaInitForUSB(
	struct ADAPTER *prAdapter,
	struct mt66xx_chip_info *prChipInfo)
{
	struct BUS_INFO *prBusInfo;
	uint32_t idx;
	uint32_t u4WfdmaAddr, u4WfdmaCr;

	prBusInfo = prChipInfo->bus_info;

	if (prChipInfo->is_support_wfdma1) {
		u4WfdmaAddr =
		CONNAC2X_TX_RING_EXT_CTRL_BASE(CONNAC2X_HOST_WPDMA_1_BASE);
	} else {
		u4WfdmaAddr =
		CONNAC2X_TX_RING_EXT_CTRL_BASE(CONNAC2X_HOST_WPDMA_0_BASE);
	}
	/*
	 * HOST_DMA1_WPDMA_TX_RING0_EXT_CTRL ~ HOST_DMA1_WPDMA_TX_RING4_EXT_CTRL
	 */
	for (idx = 0; idx < USB_TX_EPOUT_NUM; idx++) {
		HAL_MCR_RD(prAdapter, u4WfdmaAddr + (idx*4), &u4WfdmaCr);
		u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_MAX_CNT_MASK;
		u4WfdmaCr |= CONNAC2X_TX_RING_DISP_MAX_CNT;
		u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_BASE_PTR_MASK;
		u4WfdmaCr |= (0x008 + 0x4 * idx)<<20;
		HAL_MCR_WR(prAdapter, u4WfdmaAddr + (idx*4), u4WfdmaCr);
	}

	/* HOST_DMA1_WPDMA_TX_RING16_EXT_CTRL_ADDR */
	HAL_MCR_RD(prAdapter, u4WfdmaAddr + 0x40, &u4WfdmaCr);
	u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_MAX_CNT_MASK;
	u4WfdmaCr |= CONNAC2X_TX_RING_DISP_MAX_CNT;
	u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_BASE_PTR_MASK;
	u4WfdmaCr |= 0x02800000;
	HAL_MCR_WR(prAdapter, u4WfdmaAddr + 0x40, u4WfdmaCr);

	/* HOST_DMA1_WPDMA_TX_RING17_EXT_CTRL_ADDR */
	HAL_MCR_RD(prAdapter, u4WfdmaAddr + 0x44, &u4WfdmaCr);
	u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_MAX_CNT_MASK;
	u4WfdmaCr |= CONNAC2X_TX_RING_DISP_MAX_CNT;
	u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_BASE_PTR_MASK;
	u4WfdmaCr |= 0x02c00000;
	HAL_MCR_WR(prAdapter, u4WfdmaAddr + 0x44, u4WfdmaCr);


	if (prChipInfo->is_support_wacpu) {
		/* HOST_DMA1_WPDMA_TX_RING20_EXT_CTRL_ADDR */
		HAL_MCR_RD(prAdapter, u4WfdmaAddr + 0x50, &u4WfdmaCr);
		u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_MAX_CNT_MASK;
		u4WfdmaCr |= CONNAC2X_TX_RING_DISP_MAX_CNT;
		u4WfdmaCr &= ~CONNAC2X_WFDMA_DISP_BASE_PTR_MASK;
		u4WfdmaCr |= 0x03800000;
		HAL_MCR_WR(prAdapter, u4WfdmaAddr + 0x50, u4WfdmaCr);
	}

	if (prChipInfo->is_support_wfdma1) {
		u4WfdmaAddr =
			CONNAC2X_WPDMA_GLO_CFG(CONNAC2X_HOST_WPDMA_1_BASE);
		HAL_MCR_RD(prAdapter, u4WfdmaAddr, &u4WfdmaCr);
		u4WfdmaCr |=
			(CONNAC2X_WPDMA1_GLO_CFG_OMIT_TX_INFO |
			 CONNAC2X_WPDMA1_GLO_CFG_OMIT_RX_INFO |
			 CONNAC2X_WPDMA1_GLO_CFG_FW_DWLD_Bypass_dmashdl |
			 CONNAC2X_WPDMA1_GLO_CFG_RX_DMA_EN |
			 CONNAC2X_WPDMA1_GLO_CFG_TX_DMA_EN);
		HAL_MCR_WR(prAdapter, u4WfdmaAddr, u4WfdmaCr);

		/* Enable WFDMA0 RX for receiving data frame */
		u4WfdmaAddr =
			CONNAC2X_WPDMA_GLO_CFG(CONNAC2X_HOST_WPDMA_0_BASE);
		HAL_MCR_RD(prAdapter, u4WfdmaAddr, &u4WfdmaCr);
		u4WfdmaCr |=
			(CONNAC2X_WPDMA1_GLO_CFG_RX_DMA_EN);
		HAL_MCR_WR(prAdapter, u4WfdmaAddr, u4WfdmaCr);
	} else {
		u4WfdmaAddr =
			CONNAC2X_WPDMA_GLO_CFG(CONNAC2X_HOST_WPDMA_0_BASE);
		HAL_MCR_RD(prAdapter, u4WfdmaAddr, &u4WfdmaCr);
		u4WfdmaCr &= ~(CONNAC2X_WPDMA1_GLO_CFG_OMIT_RX_INFO);
		u4WfdmaCr |=
			(CONNAC2X_WPDMA1_GLO_CFG_OMIT_TX_INFO |
			 CONNAC2X_WPDMA1_GLO_CFG_OMIT_RX_INFO_PFET2 |
			 CONNAC2X_WPDMA1_GLO_CFG_FW_DWLD_Bypass_dmashdl |
			 CONNAC2X_WPDMA1_GLO_CFG_RX_DMA_EN |
			 CONNAC2X_WPDMA1_GLO_CFG_TX_DMA_EN);
		HAL_MCR_WR(prAdapter, u4WfdmaAddr, u4WfdmaCr);

	}

	prChipInfo->is_support_dma_shdl = wlanCfgGetUint32(prAdapter,
				    "DmaShdlEnable",
				    FEATURE_ENABLED, FEATURE_DEBUG_ONLY);
	if (!prChipInfo->is_support_dma_shdl) {
		/*
		 *	To disable 0x7C0252B0[6] DMASHDL
		 */
		if (prChipInfo->is_support_wfdma1) {
			u4WfdmaAddr = CONNAC2X_WPDMA_GLO_CFG_EXT0(
					CONNAC2X_HOST_WPDMA_1_BASE);
		} else {
			u4WfdmaAddr = CONNAC2X_WPDMA_GLO_CFG_EXT0(
					CONNAC2X_HOST_WPDMA_0_BASE);
		}
		HAL_MCR_RD(prAdapter, u4WfdmaAddr, &u4WfdmaCr);
		u4WfdmaCr &= ~CONNAC2X_WPDMA1_GLO_CFG_EXT0_TX_DMASHDL_EN;
		HAL_MCR_WR(prAdapter, u4WfdmaAddr, u4WfdmaCr);

		/*
		 *	[28]DMASHDL_BYPASS
		 *	DMASHDL host ask and quota control function bypass
		 *	0: Disable
		 *	1: Enable
		 */
		u4WfdmaAddr = CONNAC2X_HOST_DMASHDL_SW_CONTROL(
					CONNAC2X_HOST_DMASHDL);
		HAL_MCR_RD(prAdapter, u4WfdmaAddr, &u4WfdmaCr);
		u4WfdmaCr |= CONNAC2X_HIF_DMASHDL_BYPASS_EN;
		HAL_MCR_WR(prAdapter, u4WfdmaAddr, u4WfdmaCr);
	}

	if (prChipInfo->asicUsbInit_ic_specific)
		prChipInfo->asicUsbInit_ic_specific(prAdapter, prChipInfo);

	if ((prChipInfo->asicWfdmaReInit)
	    && (prChipInfo->asicWfdmaReInit_handshakeInit))
		prChipInfo->asicWfdmaReInit_handshakeInit(prAdapter);
}

void asicConnac2xUsbRxEvtEP4Setting(
	struct ADAPTER *prAdapter,
	u_int8_t fgEnable)
{
	struct GLUE_INFO *prGlueInfo;
	struct BUS_INFO *prBusInfo;
	uint32_t u4Value = 0;
	uint32_t u4WfdmaValue = 0;
	uint32_t i = 0;
	uint32_t dma_base;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prBusInfo = prAdapter->chip_info->bus_info;
	if (prAdapter->chip_info->is_support_wfdma1)
		dma_base = CONNAC2X_HOST_WPDMA_1_BASE;
	else
		dma_base = CONNAC2X_HOST_WPDMA_0_BASE;
	HAL_MCR_RD(prAdapter, CONNAC2X_WFDMA_HOST_CONFIG_ADDR, &u4WfdmaValue);
	if (fgEnable)
		u4WfdmaValue |= CONNAC2X_WFDMA_HOST_CONFIG_USB_RXEVT_EP4_EN;
	else
		u4WfdmaValue &= ~CONNAC2X_WFDMA_HOST_CONFIG_USB_RXEVT_EP4_EN;
	do {
		HAL_MCR_RD(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
			&u4Value);
		if ((u4Value & CONNAC2X_WPDMA1_GLO_CFG_RX_DMA_BUSY) == 0) {
			u4Value &= ~CONNAC2X_WPDMA1_GLO_CFG_RX_DMA_EN;
			HAL_MCR_WR(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
				u4Value);
			break;
		}
		kalUdelay(1000);
	} while ((i++) < 100);
	if (i > 100)
		DBGLOG(HAL, ERROR, "WFDMA1 RX keep busy....\n");
	else {
		HAL_MCR_WR(prAdapter,
				CONNAC2X_WFDMA_HOST_CONFIG_ADDR,
				u4WfdmaValue);
		HAL_MCR_RD(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
			&u4Value);
		u4Value |= CONNAC2X_WPDMA1_GLO_CFG_RX_DMA_EN;
		HAL_MCR_WR(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
			u4Value);
	}
}


uint8_t asicConnac2xUsbEventEpDetected(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BUS_INFO *prBusInfo = NULL;
	int32_t ret = 0;
	uint8_t ucRetryCount = 0;
	u_int8_t ucEp5Disable = FALSE;

	ASSERT(FALSE == 0);
	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;
	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	if (prHifInfo->fgEventEpDetected == FALSE) {
		prHifInfo->fgEventEpDetected = TRUE;
		do {
			ret = mtk_usb_vendor_request(prGlueInfo, 0,
				prBusInfo->u4device_vender_request_in,
				VND_REQ_EP5_IN_INFO,
				0, 0, &ucEp5Disable,
				sizeof(ucEp5Disable));
			if (ret || ucRetryCount)
				DBGLOG(HAL, ERROR,
				       "usb_control_msg() status: %x retry: %u\n",
				       (unsigned int)ret, ucRetryCount);
			ucRetryCount++;
			if (ucRetryCount > USB_ACCESS_RETRY_LIMIT)
				break;
		} while (ret);

		if (ret) {
			kalSendAeeWarning(HIF_USB_ERR_TITLE_STR,
				HIF_USB_ERR_DESC_STR
				"USB() reports error: %x retry: %u",
				ret, ucRetryCount);
			DBGLOG(HAL, ERROR,
			  "usb_readl() reports error: %x retry: %u\n", ret,
			  ucRetryCount);
		} else {
			DBGLOG(HAL, DEBUG,
				"%s: Get ucEp5Disable = %d\n", __func__,
			  ucEp5Disable);
			if (ucEp5Disable)
				prHifInfo->eEventEpType = EVENT_EP_TYPE_DATA_EP;
		}

		if (prHifInfo->eEventEpType == EVENT_EP_TYPE_DATA_EP)
			asicConnac2xUsbRxEvtEP4Setting(prAdapter, TRUE);
		else
			asicConnac2xUsbRxEvtEP4Setting(prAdapter, FALSE);
	}

	if (prHifInfo->eEventEpType == EVENT_EP_TYPE_DATA_EP)
		return USB_DATA_EP_IN;
	else
		return USB_EVENT_EP_IN;
}

void asicConnac2xEnableUsbCmdTxRing(
	struct ADAPTER *prAdapter,
	u_int8_t ucDstRing)
{
	struct GLUE_INFO *prGlueInfo;
	struct BUS_INFO *prBusInfo;
	uint32_t u4Value = 0;
	uint32_t u4WfdmaValue = 0;
	uint32_t i = 0;
	uint32_t dma_base;
	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prBusInfo = prAdapter->chip_info->bus_info;
	if (prAdapter->chip_info->is_support_wfdma1)
		dma_base = CONNAC2X_HOST_WPDMA_1_BASE;
	else
		dma_base = CONNAC2X_HOST_WPDMA_0_BASE;

	HAL_MCR_RD(prAdapter, CONNAC2X_WFDMA_HOST_CONFIG_ADDR, &u4WfdmaValue);
	u4WfdmaValue &= ~CONNAC2X_WFDMA_HOST_CONFIG_USB_CMDPKT_DST_MASK;
	u4WfdmaValue |= CONNAC2X_WFDMA_HOST_CONFIG_USB_CMDPKT_DST(ucDstRing);
	do {
		HAL_MCR_RD(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
			&u4Value);
		if ((u4Value & CONNAC2X_WPDMA1_GLO_CFG_TX_DMA_BUSY) == 0) {
			u4Value &= ~CONNAC2X_WPDMA1_GLO_CFG_TX_DMA_EN;
			HAL_MCR_WR(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
				u4Value);
			break;
		}
		kalUdelay(1000);
	} while ((i++) < 100);
	if (i > 100)
		DBGLOG(HAL, ERROR, "WFDMA1 TX keep busy....\n");
	else {
		HAL_MCR_WR(prAdapter,
				CONNAC2X_WFDMA_HOST_CONFIG_ADDR,
				u4WfdmaValue);
		HAL_MCR_RD(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
			&u4Value);
		u4Value |= CONNAC2X_WPDMA1_GLO_CFG_TX_DMA_EN;
		HAL_MCR_WR(prAdapter, CONNAC2X_WPDMA_GLO_CFG(dma_base),
			u4Value);
	}
}

#if CFG_ENABLE_FW_DOWNLOAD
void asicConnac2xEnableUsbFWDL(
	struct ADAPTER *prAdapter,
	u_int8_t fgEnable)
{
	struct GLUE_INFO *prGlueInfo;
	struct BUS_INFO *prBusInfo;
	struct mt66xx_chip_info *prChipInfo;

	uint32_t u4Value = 0;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	HAL_MCR_RD(prAdapter, prBusInfo->u4UdmaTxQsel, &u4Value);
	if (fgEnable) {
		u4Value |= FW_DL_EN;
	} else {
		u4Value &= ~FW_DL_EN;
	}

	HAL_MCR_WR(prAdapter, prBusInfo->u4UdmaTxQsel, u4Value);

	if (prChipInfo->is_support_wacpu) {
		/* command packet forward to TX ring 17 (WMCPU) or
		 *    TX ring 20 (WACPU)
		 */
		asicConnac2xEnableUsbCmdTxRing(prAdapter,
			fgEnable ? CONNAC2X_USB_CMDPKT2WM :
				   CONNAC2X_USB_CMDPKT2WA);
	}
}
#endif /* CFG_ENABLE_FW_DOWNLOAD */
u_int8_t asicConnac2xUsbResume(struct ADAPTER *prAdapter,
			struct GLUE_INFO *prGlueInfo)
{
	uint8_t count = 0;
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct BUS_INFO *prBusInfo;
	uint32_t u4Value, u4Idx, u4Loop;
	uint32_t u4PollingFail = FALSE;
	struct CHIP_DBG_OPS *prDbgOps;
	uint32_t u4SerEvnt;
	uint32_t u4Tick;

	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;
	prDbgOps = prChipInfo->prDebugOps;

#if 0 /* enable it if need to do bug fixing by vender request */
	/* NOTE: USB bus may not really do suspend and resume*/
	ret = usb_control_msg(prGlueInfo->rHifInfo.udev,
			  usb_sndctrlpipe(prGlueInfo->rHifInfo.udev, 0),
			  VND_REQ_FEATURE_SET,
			  prBusInfo->u4device_vender_request_out,
			  FEATURE_SET_WVALUE_RESUME, 0, NULL, 0,
			  VENDOR_TIMEOUT_MS);
	if (ret)
		DBGLOG(HAL, ERROR,
		"VendorRequest FeatureSetResume ERROR: %x\n",
		(unsigned int)ret);
#endif

	glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_PRE_RESUME);

	u4SerEvnt = halSerGetMcuEvent(prAdapter, FALSE);
	u4Tick = kalGetTimeTick();
	while (u4SerEvnt & ERROR_DETECT_SER_TRIGGER_IN_SUSPEND) {
		if (u4SerEvnt & ERROR_DETECT_SER_DONE_IN_SUSPEND) {
			halSerGetMcuEvent(prAdapter, TRUE);
			break;
		}

		kalMsleep(10);

		if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4Tick,
			    MSEC_TO_SYSTIME(WIFI_SER_L1_RST_DONE_TIMEOUT))) {

			DBGLOG(HAL, ERROR,
			       "[SER] suspend L1 reset done timeout\n");
			break;
		}
		u4SerEvnt = halSerGetMcuEvent(prAdapter, FALSE);
	}

	if (u4SerEvnt & ERROR_DETECT_SER_TRIGGER_IN_SUSPEND) {
		DBGLOG(INIT, DEBUG,
		       "L1 reset happens in suspend\n");

		/* Gather the following init flow from
		 * - halSerSyncTimerHandler()
		 * - asicConnac2xWfdmaReInit()
		 */

		if (prChipInfo->asicUsbInit)
			prChipInfo->asicUsbInit(prAdapter, prChipInfo);

		if (prBusInfo->DmaShdlReInit)
			prBusInfo->DmaShdlReInit(prAdapter);

#if (CFG_SUPPORT_ADHOC) || (CFG_ENABLE_WIFI_DIRECT)
		nicSerReInitBeaconFrame(prAdapter);
#endif
		/* It's surprising that the toggle bit or sequence
		 * number of USB endpoints in some USB hosts cannot be
		 * reset by kernel API usb_reset_endpoint(). In order to
		 * prevent this IOT, we tend to do not reset toggle bit
		 * and sequence number on both device and host in some
		 * project like MT7961. In MT7961, we can choose that
		 * endpoints reset excludes toggle bit and sequence
		 * number through asicUsbEpctlRstOpt().
		 */
		if (!prBusInfo->asicUsbEpctlRstOpt)
			halSerHifReset(prAdapter);


		if (prChipInfo->is_support_wacpu) {
			/* command packet forward to TX ring 17 (WMCPU) or
			 *	  TX ring 20 (WACPU)
			 */
			asicConnac2xEnableUsbCmdTxRing(prAdapter,
				CONNAC2X_USB_CMDPKT2WA);
		}
	} else if (asicConnac2xWfdmaIsNeedReInit(prAdapter)) {
		DBGLOG(INIT, DEBUG,
		       "Deep sleep happens in suspend\n");

		/* reinit USB because LP could clear WFDMA's CR */
		if (prChipInfo->asicWfdmaReInit)
			prChipInfo->asicWfdmaReInit(prAdapter);
	}


	for (u4Loop = 0; u4Loop < MAX_POLLING_LOOP; u4Loop++) {
		for (u4Idx = 0; u4Idx < ARRAY_SIZE(g_au4UsbPollAddrTbl);
			u4Idx++) {
			HAL_MCR_RD(prAdapter,
				g_au4UsbPollAddrTbl[u4Idx], &u4Value);
			if ((u4Value & g_au4UsbPollMaskTbl[u4Idx])
				!= g_au4UsbPollValueTbl[u4Idx]) {
				DBGLOG(HAL, ERROR,
					"Polling [0x%08x] VALUE [0x%08x]\n",
					g_au4UsbPollAddrTbl[u4Idx], u4Value);
				u4PollingFail = TRUE;
			}
		}
		if (u4PollingFail == TRUE) {
			if (u4Loop == (MAX_POLLING_LOOP - 1)) {
				if (prDbgOps && prDbgOps->showPdmaInfo)
					prDbgOps->showPdmaInfo(prAdapter);
			} else {
				u4PollingFail = FALSE;
				msleep(100);
			}
		} else
			break;
	}

	/* To trigger CR4 path */
	wlanSendDummyCmd(prAdapter, FALSE);
	halEnableInterrupt(prAdapter);

	/* using inband cmd to inform FW instead of vendor request */
	/* All Resume operations move to FW */
	halUSBPreResumeCmd(prAdapter);

	while (prGlueInfo->rHifInfo.state != USB_STATE_LINK_UP) {
		if (count > 50) {
			DBGLOG(HAL, ERROR, "pre_resume timeout\n");

			GL_DEFAULT_RESET_TRIGGER(prAdapter,
						 RST_CMD_EVT_FAIL);

			break;
		}
		msleep(20);
		count++;
	}

	DBGLOG(HAL, STATE, "pre_resume event check(count %d)\n", count);

	wlanResumePmHandle(prGlueInfo);

	return TRUE;
}

void asicConnac2xUdmaRxFlush(
	struct ADAPTER *prAdapter,
	uint8_t bEnable)
{
	struct BUS_INFO *prBusInfo;
	uint32_t u4Value;

	prBusInfo = prAdapter->chip_info->bus_info;

	HAL_MCR_RD(prAdapter, prBusInfo->u4UdmaWlCfg_0_Addr,
		   &u4Value);
	if (bEnable)
		u4Value |= UDMA_WLCFG_0_RX_FLUSH_MASK;
	else
		u4Value &= ~UDMA_WLCFG_0_RX_FLUSH_MASK;
	HAL_MCR_WR(prAdapter, prBusInfo->u4UdmaWlCfg_0_Addr,
		   u4Value);
}

uint16_t asicConnac2xUsbRxByteCount(
	struct ADAPTER *prAdapter,
	struct BUS_INFO *prBusInfo,
	uint8_t *pRXD)
{

	uint16_t u2RxByteCount;
	uint8_t ucPacketType;

	ucPacketType = HAL_MAC_CONNAC2X_RX_STATUS_GET_PKT_TYPE(
		(struct HW_MAC_CONNAC2X_RX_DESC *)pRXD);
	u2RxByteCount = HAL_MAC_CONNAC2X_RX_STATUS_GET_RX_BYTE_CNT(
		(struct HW_MAC_CONNAC2X_RX_DESC *)pRXD);

	/* According to Barry's rule, it can be summarized as below formula:
	 * 1. Event packet (including WIFI packet sent by MCU)
	   -> RX padding for 4B alignment
	 * 2. WIFI packet from UMAC
	 * -> RX padding for 8B alignment first,
				then extra 4B padding
	 */
	if (ucPacketType == RX_PKT_TYPE_RX_DATA)
		u2RxByteCount = ALIGN_8(u2RxByteCount)
			+ LEN_USB_RX_PADDING_CSO;
	else
		u2RxByteCount = ALIGN_4(u2RxByteCount);

	return u2RxByteCount;
}

#endif /* _HIF_USB */

void fillConnac2xTxDescTxByteCount(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	void *prTxDesc)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4TxByteCount = NIC_TX_DESC_LONG_FORMAT_LENGTH;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(prTxDesc);

	prChipInfo = prAdapter->chip_info;
	u4TxByteCount += nicTxGetFrameLength(prMsduInfo);

	if (prMsduInfo->ucPacketType == TX_PACKET_TYPE_DATA)
		u4TxByteCount += prChipInfo->u4ExtraTxByteCount;

	/* Calculate Tx byte count */
	HAL_MAC_CONNAC2X_TXD_SET_TX_BYTE_COUNT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prTxDesc, u4TxByteCount);
}

void fillConnac2xTxDescAppendWithWaCpu(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	uint8_t *prTxDescBuffer)
{
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;

	/* Fill TxD append */
	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *)
			   prTxDescBuffer;
	kalMemZero(prHwTxDescAppend, prChipInfo->txd_append_size);
	prHwTxDescAppend->CR4_APPEND.u2PktFlags =
		HIF_PKT_FLAGS_CT_INFO_APPLY_TXD;
	prHwTxDescAppend->CR4_APPEND.ucBssIndex =
		prMsduInfo->ucBssIndex;
	prHwTxDescAppend->CR4_APPEND.ucWtblIndex =
		prMsduInfo->ucWlanIndex;
}

void fillConnac2xTxDescAppendByWaCpu(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	uint16_t u4MsduId,
	dma_addr_t rDmaAddr,
	uint32_t u4Idx,
	u_int8_t fgIsLast,
	uint8_t *pucBuffer)
{
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;

	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *)
		(pucBuffer + NIC_TX_DESC_LONG_FORMAT_LENGTH);
	prHwTxDescAppend->CR4_APPEND.u2MsduToken = u4MsduId;
	prHwTxDescAppend->CR4_APPEND.ucBufNum = u4Idx + 1;
	prHwTxDescAppend->CR4_APPEND.au4BufPtr[u4Idx] = rDmaAddr;
	prHwTxDescAppend->CR4_APPEND.au2BufLen[u4Idx] =
		prMsduInfo->u2FrameLength;
}

void fillTxDescTxByteCountWithWaCpu(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	void *prTxDesc)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4TxByteCount = NIC_TX_DESC_LONG_FORMAT_LENGTH;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(prTxDesc);

	prChipInfo = prAdapter->chip_info;
	u4TxByteCount += prMsduInfo->u2FrameLength;

	if (prMsduInfo->ucPacketType == TX_PACKET_TYPE_DATA)
		u4TxByteCount += prChipInfo->txd_append_size;

	/* Calculate Tx byte count */
	HAL_MAC_CONNAC2X_TXD_SET_TX_BYTE_COUNT(
		(struct HW_MAC_CONNAC2X_TX_DESC *)prTxDesc, u4TxByteCount);
}

void asicConnac2xInitTxdHook(
	struct TX_DESC_OPS_T *prTxDescOps)
{
	ASSERT(prTxDescOps);
	prTxDescOps->nic_txd_long_format_op = nic_txd_v2_long_format_op;
	prTxDescOps->nic_txd_tid_op = nic_txd_v2_tid_op;
	prTxDescOps->nic_txd_queue_idx_op = nic_txd_v2_queue_idx_op;
#if (CFG_TCP_IP_CHKSUM_OFFLOAD == 1)
	prTxDescOps->nic_txd_chksum_op = nic_txd_v2_chksum_op;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD == 1 */
	prTxDescOps->nic_txd_header_format_op = nic_txd_v2_header_format_op;
	prTxDescOps->nic_txd_fill_by_pkt_option = nic_txd_v2_fill_by_pkt_option;
	prTxDescOps->nic_txd_compose = nic_txd_v2_compose;
	prTxDescOps->nic_txd_set_pkt_fixed_rate_option_full =
		nic_txd_v2_set_pkt_fixed_rate_option_full;
	prTxDescOps->nic_txd_set_pkt_fixed_rate_option =
		nic_txd_v2_set_pkt_fixed_rate_option;
	prTxDescOps->nic_txd_set_hw_amsdu_template =
		nic_txd_v2_set_hw_amsdu_template;
	prTxDescOps->u2TxdFrNstsMask = CONNAC2X_TX_DESC_FIXDE_RATE_NSTS_MASK;
	prTxDescOps->ucTxdFrNstsOffset =
		CONNAC2X_TX_DESC_FIXDE_RATE_NSTS_OFFSET;
	prTxDescOps->u2TxdFrStbcMask = CONNAC2X_TX_DESC_FIXDE_RATE_STBC;
}

void asicConnac2xInitRxdHook(
	struct RX_DESC_OPS_T *prRxDescOps)
{
	ASSERT(prRxDescOps);
	prRxDescOps->nic_rxd_get_rx_byte_count = nic_rxd_v2_get_rx_byte_count;
	prRxDescOps->nic_rxd_get_pkt_type = nic_rxd_v2_get_packet_type;
	prRxDescOps->nic_rxd_get_wlan_idx = nic_rxd_v2_get_wlan_idx;
	prRxDescOps->nic_rxd_get_sec_mode = nic_rxd_v2_get_sec_mode;
	prRxDescOps->nic_rxd_get_sw_class_error_bit =
		nic_rxd_v2_get_sw_class_error_bit;
	prRxDescOps->nic_rxd_get_ch_num = nic_rxd_v2_get_ch_num;
	prRxDescOps->nic_rxd_get_rf_band = nic_rxd_v2_get_rf_band;
	prRxDescOps->nic_rxd_get_tcl = nic_rxd_v2_get_tcl;
	prRxDescOps->nic_rxd_get_ofld = nic_rxd_v2_get_ofld;
	prRxDescOps->nic_rxd_get_HdrTrans = nic_rxd_v2_get_HdrTrans;
	prRxDescOps->nic_rxd_fill_rfb = nic_rxd_v2_fill_rfb;
	prRxDescOps->nic_rxd_sanity_check = nic_rxd_v2_sanity_check;
	prRxDescOps->nic_rxd_check_wakeup_reason =
		nic_rxd_v2_check_wakeup_reason;
#ifdef CFG_SUPPORT_SNIFFER_RADIOTAP
	prRxDescOps->nic_rxd_fill_radiotap = nic_rxd_v2_fill_radiotap;
#endif
}



#if (CFG_SUPPORT_MSP == 1)
void asicConnac2xRxProcessRxvforMSP(struct ADAPTER *prAdapter,
	  struct SW_RFB *prRetSwRfb)
{
	struct HW_MAC_RX_STS_GROUP_3_V2 *prGroup3;
	uint32_t *prRxV = NULL; /* pointer to destination buffer to store RxV */

	if (prRetSwRfb->ucStaRecIdx >= CFG_STA_REC_NUM) {
		DBGLOG(RX, LOUD,
		"prRetSwRfb->ucStaRecIdx(%d) >= CFG_STA_REC_NUM(%d)\n",
			prRetSwRfb->ucStaRecIdx, CFG_STA_REC_NUM);
		return;
	}

	if (prRetSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) {
		prRxV = prAdapter->arStaRec[prRetSwRfb->ucStaRecIdx].au4RxV;
		kalMemZero(prRxV, sizeof(uint32_t) * RXV_NUM);

		prGroup3 = prRetSwRfb->prRxStatusGroup3;

		/* P-RXV0[0:31] in RXD Group3 */
		prRxV[0] = CONNAC2X_HAL_RX_VECTOR_GET_RX_VECTOR(prGroup3, 0);

		/* P-RXV0[32:63] in RXD Group3 */
		prRxV[1] = CONNAC2X_HAL_RX_VECTOR_GET_RX_VECTOR(prGroup3, 1);

		nicRxProcessRxvLinkStats(prAdapter, prRetSwRfb, prRxV);
	}
}
#endif /* CFG_SUPPORT_MSP == 1 */

uint8_t asicConnac2xRxGetRcpiValueFromRxv(
	uint8_t ucRcpiMode,
	struct SW_RFB *prSwRfb)
{
	uint8_t ucRcpi0, ucRcpi1, ucRcpi2, ucRcpi3;
	uint8_t ucRcpiValue = 0;
	struct HW_MAC_RX_STS_GROUP_3_V2 *prGroup3 = NULL;

	ASSERT(prSwRfb);

	if (ucRcpiMode >= RCPI_MODE_NUM) {
		DBGLOG(RX, WARN,
		       "Rcpi Mode = %d is invalid for getting uint8_t value from RXV\n",
		       ucRcpiMode);
		return 0;
	}

	if ((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) == 0) {
		DBGLOG(RX, WARN, "%s, RXD group 3 is not valid\n", __func__);
		return 0;
	}

	prGroup3 = (struct HW_MAC_RX_STS_GROUP_3_V2 *)
				prSwRfb->prRxStatusGroup3;

	ucRcpi0 = CONNAC2X_HAL_RX_VECTOR_GET_RCPI0_V2(prGroup3);
	ucRcpi1 = CONNAC2X_HAL_RX_VECTOR_GET_RCPI1_V2(prGroup3);
	ucRcpi2 = CONNAC2X_HAL_RX_VECTOR_GET_RCPI2_V2(prGroup3);
	ucRcpi3 = CONNAC2X_HAL_RX_VECTOR_GET_RCPI3_V2(prGroup3);

	/*If Rcpi is not available, set it to zero*/
	if (ucRcpi0 == RCPI_MEASUREMENT_NOT_AVAILABLE)
		ucRcpi0 = RCPI_LOW_BOUND;
	if (ucRcpi1 == RCPI_MEASUREMENT_NOT_AVAILABLE)
		ucRcpi1 = RCPI_LOW_BOUND;

	DBGLOG(RX, TRACE, "RCPI WF0:%d WF1:%d WF2:%d WF3:%d\n",
	       ucRcpi0, ucRcpi1, ucRcpi2, ucRcpi3);

	switch (ucRcpiMode) {
	case RCPI_MODE_WF0:
		ucRcpiValue = ucRcpi0;
		break;

	case RCPI_MODE_WF1:
		ucRcpiValue = ucRcpi1;
		break;

	case RCPI_MODE_WF2:
		ucRcpiValue = ucRcpi2;
		break;

	case RCPI_MODE_WF3:
		ucRcpiValue = ucRcpi3;
		break;

	case RCPI_MODE_AVG: /*Not recommended for CBW80+80*/
		ucRcpiValue = (ucRcpi0 + ucRcpi1) / 2;
		break;

	case RCPI_MODE_MAX:
		ucRcpiValue =
			(ucRcpi0 > ucRcpi1) ? (ucRcpi0) : (ucRcpi1);
		break;

	case RCPI_MODE_MIN:
		ucRcpiValue =
			(ucRcpi0 < ucRcpi1) ? (ucRcpi0) : (ucRcpi1);
		break;

	default:
		break;
	}

	return ucRcpiValue;
}

#if (CFG_SUPPORT_PERF_IND == 1)
void asicConnac2xRxPerfIndProcessRXV(struct ADAPTER *prAdapter,
			       struct SW_RFB *prSwRfb,
			       uint8_t ucBssIndex)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_PERF_IND_INFO *prPerfIndInfo;
	struct HW_MAC_RX_STS_GROUP_3 *prRxStatusGroup3 = NULL;
	uint8_t ucRCPI0 = 0, ucRCPI1 = 0;
	uint32_t u4PhyRate;
	uint16_t u2Rate = 0; /* Unit 500 Kbps */
	struct RxRateInfo rRxRateInfo = {0};
	int status;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prGlueInfo = prAdapter->prGlueInfo;
	status = wlanGetRxRateByBssid(prGlueInfo, ucBssIndex, &u4PhyRate, NULL,
				&rRxRateInfo);
	if (status < 0 || u4PhyRate == 0)
		return;

	if (prSwRfb->prRxStatusGroup3 == NULL)
		return;

	prPerfIndInfo = &prGlueInfo->PerfIndCache;

	if (rRxRateInfo.u4Nss == 1) {
		if (prPerfIndInfo->ucCurRxNss[ucBssIndex] < 0xff)
			prPerfIndInfo->ucCurRxNss[ucBssIndex]++;
	} else if (rRxRateInfo.u4Nss == 2) {
		if (prPerfIndInfo->ucCurRxNss2[ucBssIndex] < 0xff)
			prPerfIndInfo->ucCurRxNss2[ucBssIndex]++;
	}

	/* ucRate(500kbs) = u4PhyRate(100kbps) */
	u2Rate = u4PhyRate / 5;

	/* RCPI */
	prRxStatusGroup3 = prSwRfb->prRxStatusGroup3;
	ucRCPI0 = HAL_RX_STATUS_GET_RCPI0(prRxStatusGroup3);
	ucRCPI1 = HAL_RX_STATUS_GET_RCPI1(prRxStatusGroup3);

	/* Record peak rate to Traffic Indicator*/
	if (u2Rate > prPerfIndInfo->u2CurRxRate[ucBssIndex]) {
		prPerfIndInfo->u2CurRxRate[ucBssIndex] = u2Rate;
		prPerfIndInfo->ucCurRxRCPI0[ucBssIndex] = ucRCPI0;
		prPerfIndInfo->ucCurRxRCPI1[ucBssIndex] = ucRCPI1;
	}

	DBGLOG(SW4, TEMP, "rate=[%u], nss=[%u], cnt_nss1=[%d], cnt_nss2=[%d]\n",
		u2Rate,
		rRxRateInfo.u4Nss,
		prPerfIndInfo->ucCurRxNss[ucBssIndex],
		prPerfIndInfo->ucCurRxNss2[ucBssIndex]);
}
#endif

#if (CFG_CHIP_RESET_SUPPORT == 1) && (CFG_WMT_RESET_API_SUPPORT == 0)
u_int8_t conn2_rst_L0_notify_step2(void)
{
	if (glGetRstReason() == RST_BT_TRIGGER) {
		typedef void (*p_bt_fun_type) (void);
		p_bt_fun_type bt_func;
		char *bt_func_name = "WF_rst_L0_notify_BT_step2";
		void *pvAddr = NULL;

		pvAddr = GLUE_SYMBOL_GET(bt_func_name);
		if (pvAddr) {
			bt_func = (p_bt_fun_type) pvAddr;
			bt_func();
			GLUE_SYMBOL_PUT(bt_func_name);
		} else {
			DBGLOG(INIT, WARN, "[SER][L0] %s does not exist\n",
							bt_func_name);
			return FALSE;
		}
	} else {
		/* if wifi trigger, then wait bt ready notify */
		DBGLOG(INIT, WARN, "[SER][L0] not support..\n");
	}

	return TRUE;
}
#endif

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
#if (CFG_DOWNLOAD_DYN_MEMORY_MAP == 1)
uint32_t downloadImgByDynMemMap(struct ADAPTER *prAdapter,
	uint32_t u4Addr, uint32_t u4Len,
	uint8_t *pucStartPtr, enum ENUM_IMG_DL_IDX_T eDlIdx)
{
	if (eDlIdx != IMG_DL_IDX_PATCH && eDlIdx != IMG_DL_IDX_N9_FW)
		return WLAN_STATUS_NOT_SUPPORTED;

	DBGLOG(INIT, DEBUG, "u4Addr: 0x%x, u4Len: %u, eDlIdx: %u\n",
			u4Addr, u4Len, eDlIdx);

	kalDevRegWriteRange(prAdapter->prGlueInfo,
			    u4Addr,
			    (void *)pucStartPtr,
			    u4Len);

	return WLAN_STATUS_SUCCESS;
}
#endif
#endif

#if defined(_HIF_PCIE) || defined(_HIF_AXI) || defined(_HIF_USB)
void asicConnac2xDmashdlSetPlePktMaxPage(struct ADAPTER *prAdapter,
				      uint16_t u2MaxPage)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	HAL_MCR_RD(prAdapter, prCfg->rPlePacketMaxSize.u4Addr, &u4Val);

	u4Val &= ~prCfg->rPlePacketMaxSize.u4Mask;
	u4Val |= (u2MaxPage << prCfg->rPlePacketMaxSize.u4Shift) &
		prCfg->rPlePacketMaxSize.u4Mask;

	HAL_MCR_WR(prAdapter, prCfg->rPlePacketMaxSize.u4Addr, u4Val);
}

void asicConnac2xDmashdlSetPsePktMaxPage(struct ADAPTER *prAdapter,
				      uint16_t u2MaxPage)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	HAL_MCR_RD(prAdapter, prCfg->rPsePacketMaxSize.u4Addr, &u4Val);

	u4Val &= ~prCfg->rPsePacketMaxSize.u4Mask;
	u4Val |= (u2MaxPage << prCfg->rPsePacketMaxSize.u4Shift) &
		 prCfg->rPsePacketMaxSize.u4Mask;

	HAL_MCR_WR(prAdapter, prCfg->rPsePacketMaxSize.u4Addr, u4Val);
}

void asicConnac2xDmashdlSetPktMaxPage(struct ADAPTER *prAdapter,
					uint16_t u2PleMaxPage,
					uint16_t u2PseMaxPage)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	/* The implementation here tends to reduce IO control and hence no
	 * reuse of asicConnac2xDmashdlSetPlePktMaxPage and
	 * asicConnac2xDmashdlSetPsePktMaxPage is applied.
	 */
	u4Val |= (u2PleMaxPage << prCfg->rPlePacketMaxSize.u4Shift) &
		 prCfg->rPlePacketMaxSize.u4Mask;
	u4Val |= (u2PseMaxPage << prCfg->rPsePacketMaxSize.u4Shift) &
		 prCfg->rPsePacketMaxSize.u4Mask;

	/* The CR address of packet max size setting of Pse and Ple
	 * are the same
	 */
	HAL_MCR_WR(prAdapter, prCfg->rPsePacketMaxSize.u4Addr, u4Val);
}

void asicConnac2xDmashdlGetPktMaxPage(struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Val = 0;
	uint32_t ple_pkt_max_sz;
	uint32_t pse_pkt_max_sz;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	HAL_MCR_RD(prAdapter, prCfg->rPlePacketMaxSize.u4Addr, &u4Val);

	ple_pkt_max_sz = (u4Val & prCfg->rPlePacketMaxSize.u4Mask) >>
		prCfg->rPlePacketMaxSize.u4Shift;
	pse_pkt_max_sz = (u4Val & prCfg->rPsePacketMaxSize.u4Mask) >>
		prCfg->rPsePacketMaxSize.u4Shift;

	DBGLOG(HAL, DEBUG, "DMASHDL PLE_PACKET_MAX_SIZE (0x%08x): 0x%08x\n",
		prCfg->rPlePacketMaxSize.u4Addr, u4Val);
	DBGLOG(HAL, DEBUG, "PLE/PSE packet max size=0x%03x/0x%03x\n",
		ple_pkt_max_sz, pse_pkt_max_sz);

}

void asicConnac2xDmashdlSetRefill(struct ADAPTER *prAdapter, uint8_t ucGroup,
			       u_int8_t fgEnable)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Mask;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	if (ucGroup >= prCfg->u4GroupNum)
		ASSERT(0);

	u4Mask = prCfg->rGroup0RefillDisable.u4Mask << ucGroup;

	HAL_MCR_RD(prAdapter, prCfg->rGroup0RefillDisable.u4Addr, &u4Val);

	if (fgEnable)
		u4Val &= ~u4Mask;
	else
		u4Val |= u4Mask;

	HAL_MCR_WR(prAdapter, prCfg->rGroup0RefillDisable.u4Addr, u4Val);
}

void asicConnac2xDmashdlSetAllRefill(struct ADAPTER *prAdapter,
			       u_int8_t *pfgEnable)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Group;
	uint32_t u4Mask;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	HAL_MCR_RD(prAdapter, prCfg->rGroup0RefillDisable.u4Addr, &u4Val);

	for (u4Group = 0; u4Group < prCfg->u4GroupNum; u4Group++) {
		u4Mask = prCfg->rGroup0RefillDisable.u4Mask << u4Group;

		if (pfgEnable[u4Group])
			u4Val &= ~u4Mask;
		else
			u4Val |= u4Mask;
	}

	HAL_MCR_WR(prAdapter, prCfg->rGroup0RefillDisable.u4Addr, u4Val);
}

void asicConnac2xDmashdlGetRefill(struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	HAL_MCR_RD(prAdapter, prCfg->rGroup0RefillDisable.u4Addr, &u4Val);
	DBGLOG(HAL, DEBUG, "DMASHDL ReFill Control (0x%08x): 0x%08x\n",
		prCfg->rGroup0RefillDisable.u4Addr, u4Val);
}

void asicConnac2xDmashdlSetMaxQuota(struct ADAPTER *prAdapter, uint8_t ucGroup,
				 uint16_t u2MaxQuota)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	if (ucGroup >= prCfg->u4GroupNum)
		ASSERT(0);

	u4Addr = prCfg->rGroup0ControlMaxQuota.u4Addr + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~prCfg->rGroup0ControlMaxQuota.u4Mask;
	u4Val |= (u2MaxQuota << prCfg->rGroup0ControlMaxQuota.u4Shift) &
		prCfg->rGroup0ControlMaxQuota.u4Mask;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void asicConnac2xDmashdlSetMinQuota(struct ADAPTER *prAdapter, uint8_t ucGroup,
				 uint16_t u2MinQuota)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	if (ucGroup >= prCfg->u4GroupNum)
		ASSERT(0);

	u4Addr = prCfg->rGroup0ControlMinQuota.u4Addr + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~prCfg->rGroup0ControlMinQuota.u4Mask;
	u4Val |= (u2MinQuota << prCfg->rGroup0ControlMinQuota.u4Shift) &
		prCfg->rGroup0ControlMinQuota.u4Mask;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void asicConnac2xDmashdlSetAllQuota(struct ADAPTER *prAdapter,
				 uint16_t *pu2MaxQuota, uint16_t *pu2MinQuota)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Group;
	uint32_t u4Addr;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	for (u4Group = 0; u4Group < prCfg->u4GroupNum; u4Group++) {
		u4Addr = prCfg->rGroup0ControlMinQuota.u4Addr + (u4Group << 2);

		u4Val = 0;
		u4Val |= (pu2MaxQuota[u4Group]
			<< prCfg->rGroup0ControlMaxQuota.u4Shift) &
			prCfg->rGroup0ControlMaxQuota.u4Mask;
		u4Val |= (pu2MinQuota[u4Group]
			<< prCfg->rGroup0ControlMinQuota.u4Shift) &
			prCfg->rGroup0ControlMinQuota.u4Mask;

		HAL_MCR_WR(prAdapter, u4Addr, u4Val);
	}
}

void asicConnac2xDmashdlGetGroupControl(struct ADAPTER *prAdapter,
					uint8_t ucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t max_quota;
	uint32_t min_quota;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	u4Addr = prCfg->rGroup0ControlMaxQuota.u4Addr + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	max_quota = GET_DMASHDL_MAX_QUOTA_NUM(u4Val);
	min_quota = GET_DMASHDL_MIN_QUOTA_NUM(u4Val);
	DBGLOG(HAL, DEBUG, "\tDMASHDL Group%d control(0x%08x): 0x%08x\n",
		ucGroup, u4Addr, u4Val);
	DBGLOG(HAL, DEBUG, "\tmax/min quota = 0x%03x/ 0x%03x\n",
		max_quota, min_quota);

}

void asicConnac2xDmashdlSetQueueMapping(struct ADAPTER *prAdapter,
					uint8_t ucQueue,
					uint8_t ucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr, u4Mask, u4Shft;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	if (ucQueue >= 32)
		ASSERT(0);

	if (ucGroup >= prCfg->u4GroupNum)
		ASSERT(0);

	u4Addr = prCfg->rQueueMapping0Queue0.u4Addr + ((ucQueue >> 3) << 2);
	u4Mask = prCfg->rQueueMapping0Queue0.u4Mask << ((ucQueue % 8) << 2);
	u4Shft = (ucQueue % 8) << 2;

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~u4Mask;
	u4Val |= (ucGroup << u4Shft) & u4Mask;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void asicConnac2xDmashdlSetAllQueueMapping(struct ADAPTER *prAdapter,
					uint8_t *pucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr, u4Mask, u4Shft;
	uint32_t u4Val = 0;
	uint8_t ucQueue;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	/* The implementation here tends to reduce IO control and hence no
	 * reuse of asicConnac2xDmashdlSetQueueMapping is applied.
	 */
	for (ucQueue = 0; ucQueue < prCfg->ucQueueNum; ucQueue++) {
		if (ucQueue % 8 == 0)
			u4Val = 0;

		u4Mask = prCfg->rQueueMapping0Queue0.u4Mask
			<< ((ucQueue % 8) << 2);
		u4Shft = (ucQueue % 8) << 2;

		u4Val |= (pucGroup[ucQueue] << u4Shft) & u4Mask;

		if (ucQueue % 8 == 7) {
			u4Addr = prCfg->rQueueMapping0Queue0.u4Addr
				+ ((ucQueue >> 3) << 2);
			HAL_MCR_WR(prAdapter, u4Addr, u4Val);
		}
	}
}

void asicConnac2xDmashdlSetSlotArbiter(struct ADAPTER *prAdapter,
				    u_int8_t fgEnable)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	HAL_MCR_RD(prAdapter, prCfg->rPageSettingGroupSeqOrderType.u4Addr,
		   &u4Val);

	if (fgEnable)
		u4Val |= prCfg->rPageSettingGroupSeqOrderType.u4Mask;
	else
		u4Val &= ~prCfg->rPageSettingGroupSeqOrderType.u4Mask;

	HAL_MCR_WR(prAdapter, prCfg->rPageSettingGroupSeqOrderType.u4Addr,
		   u4Val);
}

void asicConnac2xDmashdlSetUserDefinedPriority(struct ADAPTER *prAdapter,
					    uint8_t ucPriority, uint8_t ucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr, u4Mask, u4Shft;
	uint32_t u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	ASSERT(ucPriority < 16);
	ASSERT(ucGroup < prCfg->u4GroupNum);

	u4Addr = prCfg->rSchdulerSetting0Priority0Group.u4Addr +
		((ucPriority >> 3) << 2);
	u4Mask = prCfg->rSchdulerSetting0Priority0Group.u4Mask <<
		((ucPriority % 8) << 2);
	u4Shft = (ucPriority % 8) << 2;

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~u4Mask;
	u4Val |= (ucGroup << u4Shft) & u4Mask;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void asicConnac2xDmashdlSetAllUserDefinedPriority(struct ADAPTER *prAdapter,
					       uint8_t *pucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr, u4Mask, u4Shft;
	uint32_t u4Val = 0;
	uint8_t ucPriority;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	/* The implementation here tends to reduce IO control and hence no
	 * reuse of asicConnac2xDmashdlSetUserDefinedPriority is applied.
	 */
	for (ucPriority = 0; ucPriority < prCfg->ucPriorityNum; ucPriority++) {
		if (ucPriority % 8 == 0)
			u4Val = 0;

		u4Mask = prCfg->rSchdulerSetting0Priority0Group.u4Mask
			<< ((ucPriority % 8) << 2);
		u4Shft = (ucPriority % 8) << 2;

		u4Val |= (pucGroup[ucPriority] << u4Shft) & u4Mask;

		if (ucPriority % 8 == 7) {
			u4Addr = prCfg->rSchdulerSetting0Priority0Group.u4Addr
				+ ((ucPriority >> 3) << 2);
			HAL_MCR_WR(prAdapter, u4Addr, u4Val);
		}
	}
}

uint32_t asicConnac2xDmashdlGetRsvCount(struct ADAPTER *prAdapter,
					uint8_t ucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t rsv_cnt = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	u4Addr = prCfg->rStatusRdGp0RsvCnt.u4Addr + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	rsv_cnt = (u4Val & prCfg->rStatusRdGp0RsvCnt.u4Mask) >>
		prCfg->rStatusRdGp0RsvCnt.u4Shift;

	DBGLOG(HAL, DEBUG, "\tDMASHDL Status_RD_GP%d(0x%08x): 0x%08x\n",
		ucGroup, u4Addr, u4Val);
	DBGLOG(HAL, TRACE, "\trsv_cnt = 0x%03x\n", rsv_cnt);
	return rsv_cnt;
}

uint32_t asicConnac2xDmashdlGetSrcCount(struct ADAPTER *prAdapter,
					uint8_t ucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t src_cnt = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	u4Addr = prCfg->rStatusRdGp0SrcCnt.u4Addr + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	src_cnt = (u4Val & prCfg->rStatusRdGp0SrcCnt.u4Mask) >>
		prCfg->rStatusRdGp0SrcCnt.u4Shift;

	DBGLOG(HAL, TRACE, "\tsrc_cnt = 0x%03x\n", src_cnt);
	return src_cnt;
}

void asicConnac2xDmashdlGetPKTCount(struct ADAPTER *prAdapter, uint8_t ucGroup)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t pktin_cnt = 0;
	uint32_t ask_cnt = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	if ((ucGroup & 0x1) == 0)
		u4Addr = prCfg->rRdGroupPktCnt0.u4Addr + (ucGroup << 1);
	else
		u4Addr = prCfg->rRdGroupPktCnt0.u4Addr + ((ucGroup-1) << 1);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);
	DBGLOG(HAL, DEBUG, "\tDMASHDL RD_group_pkt_cnt_%d(0x%08x): 0x%08x\n",
		ucGroup / 2, u4Addr, u4Val);
	if ((ucGroup & 0x1) == 0) {
		pktin_cnt = GET_EVEN_GROUP_PKT_IN_CNT(u4Val);
		ask_cnt = GET_EVEN_GROUP_ASK_CNT(u4Val);
	} else {
		pktin_cnt = GET_ODD_GROUP_PKT_IN_CNT(u4Val);
		ask_cnt = GET_ODD_GROUP_ASK_CNT(u4Val);
	}
	DBGLOG(HAL, DEBUG, "\tpktin_cnt = 0x%02x, ask_cnt = 0x%02x",
		pktin_cnt, ask_cnt);
}

void asicConnac2xDmashdlSetOptionalControl(struct ADAPTER *prAdapter,
		uint16_t u2HifAckCntTh, uint16_t u2HifGupActMap)
{
	struct BUS_INFO *prBusInfo;
	struct DMASHDL_CFG *prCfg;
	uint32_t u4Addr, u4Val = 0;

	prBusInfo = prAdapter->chip_info->bus_info;
	prCfg = prBusInfo->prDmashdlCfg;

	u4Addr = prCfg->rOptionalControlCrHifAckCntTh.u4Addr;

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~prCfg->rOptionalControlCrHifAckCntTh.u4Mask;
	u4Val |= (u2HifAckCntTh <<
		  prCfg->rOptionalControlCrHifAckCntTh.u4Shift);

	u4Val &= ~prCfg->rOptionalControlCrHifGupActMap.u4Mask;
	u4Val |= (u2HifGupActMap <<
		  prCfg->rOptionalControlCrHifGupActMap.u4Shift);

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}
#endif

#if defined(_HIF_AXI)
static void handle_wfsys_reset(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *chip_info = prAdapter->chip_info;
	struct CHIP_DBG_OPS *debug_ops = chip_info != NULL ?
		chip_info->prDebugOps : NULL;

	if (kalIsResetting()) {
#if (CFG_WIFI_COREDUMP_SUPPORT == 1)
		g_Coredump_source = COREDUMP_SOURCE_WF_DRIVER;
		if (!prAdapter->prGlueInfo->u4ReadyFlag)
			glSetIsNeedWaitCoredumpFlag(TRUE);
#endif
		DBGLOG(HAL, ERROR,
			"Wi-Fi Driver trigger, need do complete.\n");
		reset_done_trigger_completion();
	} else {
#if (CFG_WIFI_COREDUMP_SUPPORT == 1)
		g_Coredump_source = COREDUMP_SOURCE_WF_FW;
		if (!prAdapter->prGlueInfo->u4ReadyFlag)
			glSetIsNeedWaitCoredumpFlag(TRUE);
#endif
		DBGLOG(HAL, ERROR,
			"FW trigger assert.\n");

		glSetRstReason(RST_FW_ASSERT);

		glResetUpdateFlag(TRUE);

		if (get_wifi_process_status()) {
#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
			fw_log_handler();
#endif
#if (CFG_WIFI_COREDUMP_SUPPORT == 1)
			wifi_coredump_start(g_Coredump_source, NULL,
				ENUM_COREDUMP_BY_CHIP_RST_LEGACY_MODE, TRUE);
			glSetIsNeedWaitCoredumpFlag(FALSE);
#endif
			if (debug_ops && debug_ops->dumpwfsyscpupcr)
				debug_ops->dumpwfsyscpupcr(prAdapter);

			glResetUpdateFlag(FALSE);
			g_Coredump_source = COREDUMP_SOURCE_NUM;
		} else {
			kalSetRstEvent(TRUE);
		}
	}
}

static void handle_whole_chip_reset(struct ADAPTER *prAdapter)
{
	DBGLOG(HAL, ERROR,
		"FW trigger whole chip reset.\n");

#if (CFG_WIFI_COREDUMP_SUPPORT == 1)
	g_Coredump_source = COREDUMP_SOURCE_WF_FW;
	if (!prAdapter->prGlueInfo->u4ReadyFlag)
		g_IsNeedWaitCoredump = TRUE;
#endif
	glResetUpdateFlag(TRUE);
	g_IsWfsysBusNoAck = TRUE;
	kalSetRstEvent(TRUE);
}

static void handle_sw_wfdma_event(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	struct SW_WFDMA_INFO *prSwWfdmaInfo =
		&prChipInfo->bus_info->rSwWfdmaInfo;

	if (prSwWfdmaInfo->fgIsEnSwWfdma) {
		if (KAL_TEST_BIT(GLUE_FLAG_HALT_BIT,
			prAdapter->prGlueInfo->ulFlag)) {
			DBGLOG(HAL, TRACE,
				"GLUE_FLAG_HALT skip SwWfdma INT\n");
		} else {
			DBGLOG(HAL, TRACE,
				"FW trigger SwWfdma INT.\n");
			halSetHifIntEvent(prAdapter->prGlueInfo,
					  HIF_FLAG_SW_WFDMA_INT_BIT);
		}
	}
}

bool asicConnac2xSwIntHandler(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t status = 0;
	bool ret = true;

	if (!prAdapter)
		return true;

	prChipInfo = prAdapter->chip_info;

	if (!prChipInfo->get_sw_interrupt_status)
		goto exit;

	ret = prChipInfo->get_sw_interrupt_status(prAdapter, &status);
	if (ret == false) {
#if (CFG_WIFI_COREDUMP_SUPPORT == 1)
		g_Coredump_source = COREDUMP_SOURCE_WF_FW;
		if (!prAdapter->prGlueInfo->u4ReadyFlag)
			glSetIsNeedWaitCoredumpFlag(TRUE);
#endif
		DBGLOG(HAL, ERROR, "get_sw_interrupt_status failed\n");
		glResetUpdateFlag(TRUE);
		kalSetRstEvent(TRUE);
		goto exit;
	}

	if (status == 0)
		goto exit;

#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
	if (status & BIT(SW_INT_FW_LOG))
		fw_log_handler();
#endif

	if (status & BIT(SW_INT_SUBSYS_RESET))
		handle_wfsys_reset(prAdapter);

	if (status & BIT(SW_INT_WHOLE_RESET))
		handle_whole_chip_reset(prAdapter);

	/* SW wfdma cmd done interrupt */
	if (status & BIT(SW_INT_SW_WFDMA))
		handle_sw_wfdma_event(prAdapter);

exit:
	return ret;
}
#endif /* _HIF_AXI */

int connsys_power_on(void)
{
	int ret = 0;

#if (CFG_SUPPORT_CONNINFRA == 1)
	ret = conninfra_pwr_on(CONNDRV_TYPE_WIFI);
	if (ret)
		DBGLOG(INIT, ERROR,
			"Conninfra pwr on fail (%d).\n",
			ret);
#endif

	return ret;
}

int connsys_power_done(void)
{
	return 0;
}

void connsys_power_off(void)
{
#if (CFG_SUPPORT_CONNINFRA == 1)
	conninfra_pwr_off(CONNDRV_TYPE_WIFI);
#endif
}

#if CFG_MTK_ANDROID_WMT
void unregister_plat_connsys_cbs(void)
{
#if (CFG_SUPPORT_CONNINFRA == 1)
	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_WIFI);
#endif

	unregister_chrdev_cbs();
}

void register_plat_connsys_cbs(void)
{
#if (CFG_SUPPORT_CONNINFRA == 1)
	struct sub_drv_ops_cb conninfra_wf_cb;
#endif

	register_chrdev_cbs();

#if (CFG_SUPPORT_CONNINFRA == 1)
	kalMemZero(&conninfra_wf_cb, sizeof(struct sub_drv_ops_cb));
	conninfra_wf_cb.rst_cb.pre_whole_chip_rst =
			glRstwlanPreWholeChipReset;
	conninfra_wf_cb.rst_cb.post_whole_chip_rst =
			glRstwlanPostWholeChipReset;
	conninfra_wf_cb.time_change_notify = kalSyncTimeToFWByIoctl;
#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
	/* Register conninfra call back */
	conninfra_wf_cb.pre_cal_cb.pwr_on_cb = wlan_precal_pwron_v1;
	conninfra_wf_cb.pre_cal_cb.do_cal_cb = wlan_precal_docal_v1;
	conninfra_wf_cb.pre_cal_cb.get_cal_result_cb = wlan_precal_get_res;
#endif /* (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1) */

	conninfra_sub_drv_ops_register(CONNDRV_TYPE_WIFI,
		&conninfra_wf_cb);
#endif
}
#endif

#if (CFG_SUPPORT_CONNINFRA == 1)
int32_t fw_log_wifi_irq_handler(void)
{
	return connsys_log_irq_handler(CONN_DEBUG_TYPE_WIFI);
}
#endif

#endif /* CFG_SUPPORT_CONNAC2X == 1 */
