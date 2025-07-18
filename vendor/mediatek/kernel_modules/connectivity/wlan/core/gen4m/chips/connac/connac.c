// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   connac.c
 *  \brief  Internal driver stack will export the required procedures here
 *          for GLUE Layer.
 *
 *  This file contains all routines which are exported from MediaTek 802.11
 *  Wireless LAN driver stack to GLUE Layer.
 */

#ifdef CONNAC

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#include "connac.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
static uint32_t connac_McuInit(struct ADAPTER *prAdapter);
static void connac_McuDeInit(struct ADAPTER *prAdapter);

uint8_t *apucConnacFwName[] = {
	(uint8_t *) CFG_FW_FILENAME "_soc1_0",
	NULL
};

struct ECO_INFO connac_eco_table[] = {
	/* HW version,  ROM version,    Factory version */
	{0x00, 0x00, 0xA, 0x1}, /* E1 */
	{0x00, 0x00, 0x0, 0x0}	/* End of table */
};

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
struct PCIE_CHIP_CR_MAPPING connac_bus2chip_cr_mapping[] = {
	/* chip addr, bus addr, range */
	{0x80000000, 0x00002000, 0x00001000}, /* MCU_CFG */

	{0x50000000, 0x00004000, 0x00004000}, /* CONN_HIF (PDMA) */
	{0x50002000, 0x00005000, 0x00001000}, /* CONN_HIF (Reserved) */
	{0x5000A000, 0x00006000, 0x00001000}, /* CONN_HIF (DMASHDL) */
	{0x000E0000, 0x00007000, 0x00001000}, /* CONN_HIF_ON (HOST_CSR) */

	{0x82060000, 0x00008000, 0x00004000}, /* WF_UMAC_TOP (PLE) */
	{0x82068000, 0x0000C000, 0x00002000}, /* WF_UMAC_TOP (PSE) */
	{0x8206C000, 0x0000E000, 0x00002000}, /* WF_UMAC_TOP (PP) */
	{0x82070000, 0x00010000, 0x00010000}, /* WF_PHY */
	{0x820F0000, 0x00020000, 0x00010000}, /* WF_LMAC_TOP */
	{0x820E0000, 0x00030000, 0x00010000}, /* WF_LMAC_TOP (WF_WTBL) */

	{0x81000000, 0x00050000, 0x00010000}, /* BTSYS_OFF */
	{0x80070000, 0x00060000, 0x00010000}, /* GPSSYS */
	{0x40000000, 0x00070000, 0x00010000}, /* WF_SYSRAM */
	{0x00300000, 0x00080000, 0x00010000}, /* MCU_SYSRAM */

	{0x80010000, 0x000A1000, 0x00001000}, /* CONN_MCU_DMA */
	{0x80030000, 0x000A2000, 0x00001000}, /* CONN_MCU_BTIF0 */
	{0x81030000, 0x000A3000, 0x00001000}, /* CONN_MCU_CFG_ON */
	{0x80050000, 0x000A4000, 0x00001000}, /* CONN_UART_PTA */
	{0x81040000, 0x000A5000, 0x00001000}, /* CONN_MCU_CIRQ */
	{0x81050000, 0x000A6000, 0x00001000}, /* CONN_MCU_GPT */
	{0x81060000, 0x000A7000, 0x00001000}, /* CONN_PTA */
	{0x81080000, 0x000A8000, 0x00001000}, /* CONN_MCU_WDT */
	{0x81090000, 0x000A9000, 0x00001000}, /* CONN_MCU_PDA */
	{0x810A0000, 0x000AA000, 0x00001000}, /* CONN_RDD_AHB_WRAP0 */
	{0x810B0000, 0x000AB000, 0x00001000}, /* BTSYS_ON */
	{0x810C0000, 0x000AC000, 0x00001000}, /* CONN_RBIST_TOP */
	{0x810D0000, 0x000AD000, 0x00001000}, /* CONN_RDD_AHB_WRAP0 */
	{0x820D0000, 0x000AE000, 0x00001000}, /* WFSYS_ON */
	{0x60000000, 0x000AF000, 0x00001000}, /* CONN_MCU_PDA */

	{0x80020000, 0x000B0000, 0x00010000}, /* CONN_TOP_MISC_OFF */
	{0x81020000, 0x000C0000, 0x00010000}, /* CONN_TOP_MISC_ON */

	{0x0, 0x0, 0x0}
};
#endif /* _HIF_PCIE || _HIF_AXI */

void connacConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx)
{
	uint8_t ucIdx = 0;
	uint8_t aucFlavor[2] = {0};
	int ret = 0;

	kalGetFwFlavorByGlue(prGlueInfo, &aucFlavor[0]);
	for (ucIdx = 0; apucConnacFwName[ucIdx]; ucIdx++) {
		if ((*pucNameIdx + 3) >= ucMaxNameIdx) {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
				"kalFirmwareImageMapping >> file name array is not enough.\n");
			ASSERT(0);
			continue;
		}

		/* Type 1. WIFI_RAM_CODE_soc1_0_1_1.bin */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN,
				"%s_%u%s_1.bin",
				apucConnacFwName[ucIdx],
				CFG_WIFI_IP_SET,
				aucFlavor);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);

		/* Type 2. WIFI_RAM_CODE_soc1_0_1_1 */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN,
				"%s_%u%s_1",
				apucConnacFwName[ucIdx],
				CFG_WIFI_IP_SET,
				aucFlavor);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);

		/* Type 3. WIFI_RAM_CODE_soc1_0 */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s",
				apucConnacFwName[ucIdx]);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);

		/* Type 4. WIFI_RAM_CODE_soc1_0.bin */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s.bin",
				apucConnacFwName[ucIdx]);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);
	}
}

void connacConstructPatchName(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucName, uint8_t *pucNameIdx)
{
	int ret = 0;

	ret = kalSnprintf(apucName[(*pucNameIdx)],
		CFG_FW_NAME_MAX_LEN, "mtsoc1_0_patch_e%x_hdr.bin",
		wlanGetEcoVersion(prGlueInfo->prAdapter));
	if (ret < 0 || ret >= CFG_FW_NAME_MAX_LEN)
		DBGLOG(INIT, ERROR, "kalSnprintf failed, ret: %d\n", ret);
}

struct BUS_INFO connac_bus_info = {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	.top_cfg_base = CONNAC_TOP_CFG_BASE,
	.host_tx_ring_base = MT_TX_RING_BASE,
	.host_tx_ring_ext_ctrl_base = MT_TX_RING_BASE_EXT,
	.host_tx_ring_cidx_addr = MT_TX_RING_CIDX,
	.host_tx_ring_didx_addr = MT_TX_RING_DIDX,
	.host_tx_ring_cnt_addr = MT_TX_RING_CNT,

	.host_rx_ring_base = MT_RX_RING_BASE,
	.host_rx_ring_ext_ctrl_base = MT_RX_RING_BASE_EXT,
	.host_rx_ring_cidx_addr = MT_RX_RING_CIDX,
	.host_rx_ring_didx_addr = MT_RX_RING_DIDX,
	.host_rx_ring_cnt_addr = MT_RX_RING_CNT,
	.bus2chip = connac_bus2chip_cr_mapping,
	.tx_ring_fwdl_idx = 3,
	.tx_ring_cmd_idx = 15,
	.tx_ring0_data_idx = 0,
	.tx_ring1_data_idx = 0, /* no used */
	.rx_data_ring_num = 1,
	.rx_evt_ring_num = 1,
	.rx_data_ring_size = 256,
	.rx_evt_ring_size = 16,
	.rx_data_ring_prealloc_size = 256,
	.fw_own_clear_addr = WPDMA_INT_STA,
	.fw_own_clear_bit = WPDMA_FW_CLR_OWN_INT,
	.max_static_map_addr = 0x00040000,
	.fgCheckDriverOwnInt = FALSE,
	.u4DmaMask = 36,

	.pdmaSetup = asicPdmaConfig,
	.updateTxRingMaxQuota = NULL,
	.enableInterrupt = asicEnableInterrupt,
	.disableInterrupt = asicDisableInterrupt,
	.lowPowerOwnRead = asicLowPowerOwnRead,
	.lowPowerOwnSet = asicLowPowerOwnSet,
	.lowPowerOwnClear = asicLowPowerOwnClear,
	.wakeUpWiFi = asicWakeUpWiFi,
	.isValidRegAccess = asicIsValidRegAccess,
	.getMailboxStatus = asicGetMailboxStatus,
	.setDummyReg = asicSetDummyReg,
	.checkDummyReg = asicCheckDummyReg,
	.tx_ring_ext_ctrl = asicPdmaTxRingExtCtrl,
	.rx_ring_ext_ctrl = asicPdmaRxRingExtCtrl,
	.hifRst = NULL,
#if defined(_HIF_PCIE)
	.initPcieInt = NULL,
#endif
	.DmaShdlInit = asicPcieDmaShdlInit,
	.DmaShdlReInit = NULL,
	.setDmaIntMask = asicPdmaIntMaskConfig,
#endif /* _HIF_PCIE || _HIF_AXI */
#if defined(_HIF_USB)
	.u4UdmaWlCfg_0_Addr = CONNAC_UDMA_WLCFG_0,
	.u4UdmaWlCfg_1_Addr = CONNAC_UDMA_WLCFG_1,
	.u4UdmaWlCfg_0 =
		(UDMA_WLCFG_0_TX_EN(1) |
		UDMA_WLCFG_0_RX_EN(1) |
		UDMA_WLCFG_0_RX_MPSZ_PAD0(1) |
		UDMA_WLCFG_0_1US_TIMER_EN(1)),
	.u4device_vender_request_in = DEVICE_VENDOR_REQUEST_IN,
	.u4device_vender_request_out = DEVICE_VENDOR_REQUEST_OUT,
	.asicUsbSuspend = NULL,
	.asicUsbResume = NULL,
	.asicUsbEventEpDetected = NULL,
	.asicUsbRxByteCount = NULL,
	.DmaShdlInit = asicUsbDmaShdlInit,
	.DmaShdlReInit = NULL,
	.asicUdmaRxFlush = NULL,
#if CFG_CHIP_RESET_SUPPORT
	.asicUsbEpctlRstOpt = NULL,
#endif
#endif /* _HIF_USB */
};

struct FWDL_OPS_T connac_fw_dl_ops = {
	.constructFirmwarePrio = connacConstructFirmwarePrio,
	.constructPatchName = connacConstructPatchName,
#if !CFG_MTK_ANDROID_WMT
	.downloadPatch = wlanDownloadPatch,
#endif
	.downloadFirmware = wlanConnacFormatDownload,
	.downloadByDynMemMap = NULL,
	.getFwInfo = wlanGetConnacFwInfo,
	.getFwDlInfo = asicGetFwDlInfo,
	.phyAction = NULL,
	.downloadEMI = wlanDownloadEMISection,
	.mcu_init = connac_McuInit,
	.mcu_deinit = connac_McuDeInit,
};

struct TX_DESC_OPS_T connacTxDescOps = {
	.fillNicAppend = fillNicTxDescAppend,
	.fillHifAppend = fillTxDescAppendByHostV2,
	.fillTxByteCount = fillTxDescTxByteCount,
};

struct RX_DESC_OPS_T connacRxDescOps = {
};

#if CFG_SUPPORT_QA_TOOL
struct ATE_OPS_T connacAteOps = {
	.setICapStart = connacSetICapStart,
	.getICapStatus = connacGetICapStatus,
	.getICapIQData = connacGetICapIQData,
	.getRbistDataDumpEvent = nicExtEventICapIQData,
};
#endif

struct CHIP_DBG_OPS connac_debug_ops = {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	.showPdmaInfo = halShowPdmaInfo,
	.showPseInfo = halShowPseInfo,
	.showPleInfo = halShowPleInfo,
	.showTxdInfo = halShowTxdInfo,
	.showCsrInfo = halShowHostCsrInfo,
	.showDmaschInfo = halShowDmaschInfo,
	.dumpMacInfo = haldumpMacInfo,
	.dumpTxdInfo = halDumpTxdInfo,
#else
	.showPdmaInfo = NULL,
	.showPseInfo = NULL,
	.showPleInfo = NULL,
	.showTxdInfo = NULL,
	.showCsrInfo = NULL,
	.showDmaschInfo = NULL,
	.dumpMacInfo = NULL,
	.dumpTxdInfo = NULL,
#endif
	.showWtblInfo = NULL,
	.showHifInfo = NULL,
	.printHifDbgInfo = halPrintHifDbgInfo,
	.show_stat_info = halShowStatInfo,
#if CFG_SUPPORT_LINK_QUALITY_MONITOR
	.get_rx_rate_info = connac_get_rx_rate_info,
#endif
	.show_mcu_debug_info = NULL,
};

struct mt66xx_chip_info mt66xx_chip_info_connac = {
	.bus_info = &connac_bus_info,
	.fw_dl_ops = &connac_fw_dl_ops,
	.prTxDescOps = &connacTxDescOps,
	.prRxDescOps = &connacRxDescOps,
#if CFG_SUPPORT_QA_TOOL
	.prAteOps = &connacAteOps,
#endif
	.prDebugOps = &connac_debug_ops,

	.chip_id = CONNAC_CHIP_ID,
	.should_verify_chip_id = FALSE,
	.sw_sync0 = CONNAC_SW_SYNC0,
	.sw_ready_bits = WIFI_FUNC_NO_CR4_READY_BITS,
	.sw_ready_bit_offset = CONNAC_SW_SYNC0_RDY_OFFSET,
	.patch_addr = CONNAC_PATCH_START_ADDR,
	.is_support_cr4 = FALSE,
	.txd_append_size = CONNAC_TX_DESC_APPEND_LENGTH,
	.rxd_size = CONNAC_RX_DESC_LENGTH,
	.init_evt_rxd_size = CONNAC_INIT_EVT_RX_DESC_LENGTH,
	.pse_header_length = NIC_TX_PSE_HEADER_LENGTH,
	.init_event_size = CONNAC_RX_INIT_EVENT_LENGTH,
	.event_hdr_size = CONNAC_RX_EVENT_HDR_LENGTH,
	.eco_info = connac_eco_table,
	.isNicCapV1 = FALSE,
	.is_support_efuse = FALSE,
	.sw_sync_emi_info = NULL,
	/* IP info, should be overwrite by getNicCapabalityV2 */
	.u4ChipIpVersion = CONNAC_CHIP_IP_VERSION,
	.u4ChipIpConfig = CONNAC_CHIP_IP_CONFIG,
	.u2ADieChipVersion = CONNAC_CHIP_ADIE_INFO,
	.asicCapInit = asicCapInit,
	.asicEnableFWDownload = asicEnableFWDownload,
	.asicGetChipID = asicGetChipID,
	.downloadBufferBin = NULL,
	.is_support_hw_amsdu = FALSE,
	.ucMaxSwAmsduNum = 4,
	.ucMaxSwapAntenna = 2,
	.workAround = 0,

	.top_hcr = TOP_HCR,
	.top_hvr = TOP_HVR,
	.top_fvr = TOP_FVR,
	.custom_oid_interface_version = MTK_CUSTOM_OID_INTERFACE_VERSION,
	.em_interface_version = MTK_EM_INTERFACE_VERSION,
#if CFG_MTK_ANDROID_WMT
	.rEmiInfo = {
		.type = EMI_ALLOC_TYPE_WMT,
	},
#endif
};

struct mt66xx_hif_driver_data mt66xx_driver_data_connac = {
	.chip_info = &mt66xx_chip_info_connac,
};

static uint32_t connac_McuInit(struct ADAPTER *prAdapter)
{
	mtk_wcn_consys_hw_wifi_paldo_ctrl(1);
	return WLAN_STATUS_SUCCESS;
}

static void connac_McuDeInit(struct ADAPTER *prAdapter)
{
	mtk_wcn_consys_hw_wifi_paldo_ctrl(0);
}

#endif /* CONNAC */
