// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   bellwether.c
*    \brief  Internal driver stack will export
*    the required procedures here for GLUE Layer.
*
*    This file contains all routines which are exported
     from MediaTek 802.11 Wireless LAN driver stack to GLUE Layer.
*/

#ifdef BELLWETHER

#include "precomp.h"
#include "bellwether.h"
#include "coda/bellwether/wf_wfdma_host_dma0.h"
#include "coda/bellwether/wf_wfdma_mcu_dma0.h"
#include "coda/bellwether/wf_hif_dmashdl_top.h"
#include "coda/bellwether/wf_pse_top.h"
#include "coda/bellwether/pcie_mac_ireg.h"
#include "coda/bellwether/conn_infra_rgu_on.h"
#include "coda/bellwether/conn_infra_bus_cr_on.h"
#include "coda/bellwether/wf_top_cfg_on.h"
#include "coda/bellwether/wf_mcu_bus_cr.h"
#include "coda/bellwether/conn_host_csr_top.h"
#include "coda/bellwether/conn_infra_cfg.h"
#include "coda/bellwether/conn_infra_cfg_on.h"
#include "hal_dmashdl_bellwether.h"
#include "coda/bellwether/wf_wtblon_top.h"
#include "coda/bellwether/wf_uwtbl_top.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static void bellwether_ConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx);

static uint8_t bellwetherSetRxRingHwAddr(struct RTMP_RX_RING *prRxRing,
		struct BUS_INFO *prBusInfo, uint32_t u4SwRingIdx);

static bool bellwetherWfdmaAllocRxRing(struct GLUE_INFO *prGlueInfo,
		bool fgAllocMem);

static void bellwetherProcessTxInterrupt(
		struct ADAPTER *prAdapter);

static void bellwetherProcessRxInterrupt(
	struct ADAPTER *prAdapter);

static void bellwetherWfdmaManualPrefetch(
	struct GLUE_INFO *prGlueInfo);

static void bellwetherReadIntStatus(struct ADAPTER *prAdapter,
		uint32_t *pu4IntStatus);

static void bellwetherConfigIntMask(struct GLUE_INFO *prGlueInfo,
		u_int8_t enable);

static void bellwetherWpdmaConfig(struct GLUE_INFO *prGlueInfo,
		u_int8_t enable, bool fgResetHif);

static void bellwetherWfdmaRxRingExtCtrl(struct GLUE_INFO *prGlueInfo,
	struct RTMP_RX_RING *rx_ring,
	u_int32_t index);

#if defined(_HIF_PCIE)
static void bellwetherInitPcieInt(struct GLUE_INFO *prGlueInfo);
#endif

#if (CONFIG_ROM_CODE_DOWNLOAD == 1)
static uint32_t bellwetherDownloadRomCode(struct ADAPTER *prAdapter);
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

struct ECO_INFO bellwether_eco_table[] = {
	/* HW version,  ROM version,    Factory version */
	{0x00, 0x00, 0xA, 0x1},	/* E1 */
	{0x00, 0x00, 0x0, 0x0}	/* End of table */
};

uint8_t *apucbellwetherFwName[] = {
	(uint8_t *) CFG_FW_FILENAME "_bellwether_1a",
	(uint8_t *) CFG_FW_FILENAME "_bellwether_sta",
	(uint8_t *) CFG_FW_FILENAME "_bellwether",
	NULL
};

uint8_t *apucBellFwRomName[] = {
	(uint8_t *) "wf_rom_1a.bin",
	(uint8_t *) "wf_rom.bin",
	NULL
};

uint8_t *apucBellFwRomSramName[] = {
	(uint8_t *) "wf_rom_1a_sram.bin",
	(uint8_t *) "wf_rom_sram.bin",
	NULL
};

#if defined(_HIF_PCIE)
struct PCIE_CHIP_CR_MAPPING bellwether_bus2chip_cr_mapping[] = {
	/* chip addr, bus addr, range */
	{0x830c0000, 0x00000, 0x1000}, /* WF_MCU_BUS_CR_REMAP */
	{0x54000000, 0x02000, 0x1000},  /* WFDMA PCIE0 MCU DMA0 */
	{0x55000000, 0x03000, 0x1000},  /* WFDMA PCIE0 MCU DMA1 */
	{0x56000000, 0x04000, 0x1000},  /* WFDMA reserved */
	{0x57000000, 0x05000, 0x1000},  /* WFDMA MCU wrap CR */
	{0x58000000, 0x06000, 0x1000},  /* WFDMA PCIE1 MCU DMA0 (MEM_DMA) */
	{0x59000000, 0x07000, 0x1000},  /* WFDMA PCIE1 MCU DMA1 */
	{0x820c0000, 0x08000, 0x4000},  /* WF_UMAC_TOP (PLE) */
	{0x820c8000, 0x0c000, 0x2000},  /* WF_UMAC_TOP (PSE) */
	{0x820cc000, 0x0e000, 0x2000},  /* WF_UMAC_TOP (PP) */
	{0x74030000, 0x10000, 0x1000},  /* PCIe MAC */
	{0x820e0000, 0x20000, 0x0400},  /* WF_LMAC_TOP BN0 (WF_CFG) */
	{0x820e1000, 0x20400, 0x0200},  /* WF_LMAC_TOP BN0 (WF_TRB) */
	{0x820e2000, 0x20800, 0x0400},  /* WF_LMAC_TOP BN0 (WF_AGG) */
	{0x820e3000, 0x20c00, 0x0400},  /* WF_LMAC_TOP BN0 (WF_ARB) */
	{0x820e4000, 0x21000, 0x0400},  /* WF_LMAC_TOP BN0 (WF_TMAC) */
	{0x820e5000, 0x21400, 0x0800},  /* WF_LMAC_TOP BN0 (WF_RMAC) */
	{0x820ce000, 0x21c00, 0x0200},  /* WF_LMAC_TOP (WF_SEC) */
	{0x820e7000, 0x21e00, 0x0200},  /* WF_LMAC_TOP BN0 (WF_DMA) */
	{0x820cf000, 0x22000, 0x1000},  /* WF_LMAC_TOP (WF_PF) */
	{0x820e9000, 0x23400, 0x0200},  /* WF_LMAC_TOP BN0 (WF_WTBLOFF) */
	{0x820ea000, 0x24000, 0x0200},  /* WF_LMAC_TOP BN0 (WF_ETBF) */
	{0x820eb000, 0x24200, 0x0400},  /* WF_LMAC_TOP BN0 (WF_LPON) */
	{0x820ec000, 0x24600, 0x0200},  /* WF_LMAC_TOP BN0 (WF_INT) */
	{0x820ed000, 0x24800, 0x0800},  /* WF_LMAC_TOP BN0 (WF_MIB) */
	{0x820ca000, 0x26000, 0x2000},  /* WF_LMAC_TOP BN0 (WF_MUCOP) */
	{0x820d0000, 0x30000, 0x10000}, /* WF_LMAC_TOP (WF_WTBLON) */
	{0x40000000, 0x70000, 0x10000}, /* WF_UMAC_SYSRAM */
	{0x00400000, 0x80000, 0x10000}, /* WF_MCU_SYSRAM */
	{0x00410000, 0x90000, 0x10000}, /* WF_MCU_SYSRAM (configure register) */
	{0x820f0000, 0xa0000, 0x0400},  /* WF_LMAC_TOP BN1 (WF_CFG) */
	{0x820f1000, 0xa0600, 0x0200},  /* WF_LMAC_TOP BN1 (WF_TRB) */
	{0x820f2000, 0xa0800, 0x0400},  /* WF_LMAC_TOP BN1 (WF_AGG) */
	{0x820f3000, 0xa0c00, 0x0400},  /* WF_LMAC_TOP BN1 (WF_ARB) */
	{0x820f4000, 0xa1000, 0x0400},  /* WF_LMAC_TOP BN1 (WF_TMAC) */
	{0x820f5000, 0xa1400, 0x0800},  /* WF_LMAC_TOP BN1 (WF_RMAC) */
	{0x820f7000, 0xa1e00, 0x0200},  /* WF_LMAC_TOP BN1 (WF_DMA) */
	{0x820f9000, 0xa3400, 0x0200},  /* WF_LMAC_TOP BN1 (WF_WTBLOFF) */
	{0x820fa000, 0xa4000, 0x0200},  /* WF_LMAC_TOP BN1 (WF_ETBF) */
	{0x820fb000, 0xa4200, 0x0400},  /* WF_LMAC_TOP BN1 (WF_LPON) */
	{0x820fc000, 0xa4600, 0x0200},  /* WF_LMAC_TOP BN1 (WF_INT) */
	{0x820fd000, 0xa4800, 0x0800},  /* WF_LMAC_TOP BN1 (WF_MIB) */
	{0x820cc000, 0xa5000, 0x2000},  /* WF_LMAC_TOP BN1 (WF_MUCOP) */
	{0x820c4000, 0xa8000, 0x4000},  /* WF_LMAC_TOP BN1 (WF_MUCOP) */
	{0x820b0000, 0xae000, 0x1000},  /* [APB2] WFSYS_ON */
	{0x80020000, 0xb0000, 0x10000}, /* WF_TOP_MISC_OFF */
	{0x81020000, 0xc0000, 0x10000}, /* WF_TOP_MISC_ON */
	{0x7c020000, 0xd0000, 0x10000}, /* CONN_INFRA, wfdma */
	{0x7c500000, BELLWETHER_PCIE2AP_REMAP_BASE_ADDR,
		0x2000000}, /* remap */
	{0x7c060000, 0xe0000, 0x10000}, /* CONN_INFRA, conn_host_csr_top */
	{0x7c000000, 0xf0000, 0x10000}, /* CONN_INFRA */
	{0x0, 0x0, 0x0} /* End */
};
#endif

struct pcie2ap_remap bellwether_pcie2ap_remap = {
	.reg_base = CONN_INFRA_BUS_CR_ON_CONN_INFRA_PCIE2AP_REMAP_WF__5__4_cr_pcie2ap_public_remapping_wf_5_ADDR,
	.reg_mask = CONN_INFRA_BUS_CR_ON_CONN_INFRA_PCIE2AP_REMAP_WF__5__4_cr_pcie2ap_public_remapping_wf_5_MASK,
	.reg_shift = CONN_INFRA_BUS_CR_ON_CONN_INFRA_PCIE2AP_REMAP_WF__5__4_cr_pcie2ap_public_remapping_wf_5_SHFT,
	.base_addr = BELLWETHER_PCIE2AP_REMAP_BASE_ADDR
};

struct ap2wf_remap bellwether_ap2wf_remap = {
	.reg_base = WF_MCU_BUS_CR_AP2WF_REMAP_1_R_AP2WF_PUBLIC_REMAPPING_0_START_ADDRESS_ADDR,
	.reg_mask = WF_MCU_BUS_CR_AP2WF_REMAP_1_R_AP2WF_PUBLIC_REMAPPING_0_START_ADDRESS_MASK,
	.reg_shift = WF_MCU_BUS_CR_AP2WF_REMAP_1_R_AP2WF_PUBLIC_REMAPPING_0_START_ADDRESS_SHFT,
	.base_addr = BELLWETHER_REMAP_BASE_ADDR
};

struct PCIE_CHIP_CR_REMAPPING bellwether_bus2chip_cr_remap = {
	.pcie2ap = &bellwether_pcie2ap_remap,
	.ap2wf = &bellwether_ap2wf_remap,
};

struct wfdma_group_info bellwether_wfmda_host_tx_group[] = {
	{"P0T0:AP DATA0", WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL0_ADDR, true},
	{"P0T1:AP DATA1", WF_WFDMA_HOST_DMA0_WPDMA_TX_RING1_CTRL0_ADDR, true},
	{"P0T2:AP DATA2", WF_WFDMA_HOST_DMA0_WPDMA_TX_RING2_CTRL0_ADDR, true},
	{"P0T3:AP DATA3", WF_WFDMA_HOST_DMA0_WPDMA_TX_RING3_CTRL0_ADDR, true},
	{"P0T17:AP CMD", WF_WFDMA_HOST_DMA0_WPDMA_TX_RING17_CTRL0_ADDR, true},
	{"P0T16:FWDL", WF_WFDMA_HOST_DMA0_WPDMA_TX_RING16_CTRL0_ADDR, true},
};

struct wfdma_group_info bellwether_wfmda_host_rx_group[] = {
	{"P0R4:AP DATA0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING4_CTRL0_ADDR, true},
	{"P0R0:AP EVENT", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL0_ADDR, true},
	{"P0R5:AP DATA1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING5_CTRL0_ADDR, true},
	{"P0R6:AP TDONE0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING6_CTRL0_ADDR, true},
	{"P0R7:AP TDONE1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING7_CTRL0_ADDR, true},
	{"P0R8:AP DATA2", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING8_CTRL0_ADDR, true},
	{"P0R9:AP TDONE2", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING9_CTRL0_ADDR, true},
};

struct wfdma_group_info bellwether_wfmda_wm_tx_group[] = {
	{"P0T0:AP EVENT", WF_WFDMA_MCU_DMA0_WPDMA_TX_RING0_CTRL0_ADDR},
	{"P0T4:DATA", WF_WFDMA_MCU_DMA0_WPDMA_TX_RING4_CTRL0_ADDR},
};

struct wfdma_group_info bellwether_wfmda_wm_rx_group[] = {
	{"P0R0:FWDL", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING0_CTRL0_ADDR},
	{"P0R1:AP CMD", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING1_CTRL0_ADDR},
	{"P0R5:DATA", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING5_CTRL0_ADDR},
	{"P0R6:TXDONE", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING6_CTRL0_ADDR},
	{"P0R7:RPT", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING7_CTRL0_ADDR},
};

struct pse_group_info bellwether_pse_group[] = {
	{"HIF0(TX data)", WF_PSE_TOP_PG_HIF0_GROUP_ADDR,
		WF_PSE_TOP_HIF0_PG_INFO_ADDR},
	{"HIF1(Talos CMD)", WF_PSE_TOP_PG_HIF1_GROUP_ADDR,
		WF_PSE_TOP_HIF1_PG_INFO_ADDR},
	{"CPU(I/O r/w)",  WF_PSE_TOP_PG_CPU_GROUP_ADDR,
		WF_PSE_TOP_CPU_PG_INFO_ADDR},
	{"PLE(host report)",  WF_PSE_TOP_PG_PLE_GROUP_ADDR,
		WF_PSE_TOP_PLE_PG_INFO_ADDR},
	{"PLE1(SPL report)", WF_PSE_TOP_PG_PLE1_GROUP_ADDR,
		WF_PSE_TOP_PLE1_PG_INFO_ADDR},
	{"LMAC0(RX data)", WF_PSE_TOP_PG_LMAC0_GROUP_ADDR,
			WF_PSE_TOP_LMAC0_PG_INFO_ADDR},
	{"LMAC1(RX_VEC)", WF_PSE_TOP_PG_LMAC1_GROUP_ADDR,
			WF_PSE_TOP_LMAC1_PG_INFO_ADDR},
	{"LMAC2(TXS)", WF_PSE_TOP_PG_LMAC2_GROUP_ADDR,
			WF_PSE_TOP_LMAC2_PG_INFO_ADDR},
	{"LMAC3(TXCMD/RXRPT)", WF_PSE_TOP_PG_LMAC3_GROUP_ADDR,
			WF_PSE_TOP_LMAC3_PG_INFO_ADDR},
	{"MDP",  WF_PSE_TOP_PG_MDP_GROUP_ADDR,
			WF_PSE_TOP_MDP_PG_INFO_ADDR},
};

struct BUS_INFO bellwether_bus_info = {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	.top_cfg_base = BELLWETHER_TOP_CFG_BASE,

	/* host_dma0 for TXP */
	.host_dma0_base = WF_WFDMA_HOST_DMA0_BASE,
	.host_int_status_addr = WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR,

	.host_int_txdone_bits =
	(
#if (CFG_SUPPORT_DISABLE_DATA_DDONE_INTR == 0)
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_0_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_1_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_2_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_3_MASK |
#endif /* CFG_SUPPORT_DISABLE_DATA_DDONE_INTR == 0 */
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_16_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_17_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_subsys_int_sts_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_mcu2host_sw_int_sts_MASK),
	.host_int_rxdone_bits = 
	(WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_0_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_4_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_5_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_6_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_7_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_8_MASK |
	 WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_9_MASK),

	.host_tx_ring_base = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL0_ADDR,
	.host_tx_ring_ext_ctrl_base =
		WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR,
	.host_tx_ring_cidx_addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL2_ADDR,
	.host_tx_ring_didx_addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL3_ADDR,
	.host_tx_ring_cnt_addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL1_ADDR,

	.host_rx_ring_base = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL0_ADDR,
	.host_rx_ring_ext_ctrl_base =
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_EXT_CTRL_ADDR,
	.host_rx_ring_cidx_addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL2_ADDR,
	.host_rx_ring_didx_addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL3_ADDR,
	.host_rx_ring_cnt_addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL1_ADDR,

	.bus2chip = bellwether_bus2chip_cr_mapping,
	.bus2chip_remap = &bellwether_bus2chip_cr_remap,
	.max_static_map_addr = 0x00100000,

	.tx_ring_fwdl_idx = CONNAC3X_FWDL_TX_RING_IDX,
	.tx_ring_cmd_idx = CONNAC3X_CMD_TX_RING_IDX,
	.tx_ring0_data_idx = 0,
	.tx_ring1_data_idx = 1,
	.tx_prio_data_idx = 2,
	.tx_altx_data_idx = 3,
	.rx_data_ring_num = 3,
	.rx_evt_ring_num = 4,
#if CFG_SUPPORT_RX_PAGE_POOL
	.rx_data_ring_size = 3072,
#else
	.rx_data_ring_size = 1024,
#endif
	.rx_evt_ring_size = 128,
	.rx_data_ring_prealloc_size = 1024,
	.fw_own_clear_addr = CONNAC3X_BN0_IRQ_STAT_ADDR,
	.fw_own_clear_bit = PCIE_LPCR_FW_CLR_OWN,
	.fgCheckDriverOwnInt = FALSE,
	.u4DmaMask = 32,
	.wfmda_host_tx_group = bellwether_wfmda_host_tx_group,
	.wfmda_host_tx_group_len = ARRAY_SIZE(bellwether_wfmda_host_tx_group),
	.wfmda_host_rx_group = bellwether_wfmda_host_rx_group,
	.wfmda_host_rx_group_len = ARRAY_SIZE(bellwether_wfmda_host_rx_group),
	.wfmda_wm_tx_group = bellwether_wfmda_wm_tx_group,
	.wfmda_wm_tx_group_len = ARRAY_SIZE(bellwether_wfmda_wm_tx_group),
	.wfmda_wm_rx_group = bellwether_wfmda_wm_rx_group,
	.wfmda_wm_rx_group_len = ARRAY_SIZE(bellwether_wfmda_wm_rx_group),
	.prDmashdlCfg = &rBellwetherDmashdlCfg,
#if (DBG_DISABLE_ALL_INFO == 0)
	.prPleTopCr = &rBellwetherPleTopCr,
	.prPseTopCr = &rBellwetherPseTopCr,
	.prPpTopCr = &rBellwetherPpTopCr,
#endif
	.prPseGroup = bellwether_pse_group,
	.u4PseGroupLen = ARRAY_SIZE(bellwether_pse_group),
	.pdmaSetup = bellwetherWpdmaConfig,
	.enableInterrupt = asicConnac3xEnablePlatformIRQ,
	.disableInterrupt = asicConnac3xDisablePlatformIRQ,
#if defined(_HIF_PCIE)
	.initPcieInt = bellwetherInitPcieInt,
#endif
	.processTxInterrupt = bellwetherProcessTxInterrupt,
	.processRxInterrupt = bellwetherProcessRxInterrupt,
	.tx_ring_ext_ctrl = asicConnac3xWfdmaTxRingExtCtrl,
	.rx_ring_ext_ctrl = bellwetherWfdmaRxRingExtCtrl,
	/* null wfdmaManualPrefetch if want to disable manual mode */
	.wfdmaManualPrefetch = bellwetherWfdmaManualPrefetch,
	.lowPowerOwnRead = asicConnac3xLowPowerOwnRead,
	.lowPowerOwnSet = asicConnac3xLowPowerOwnSet,
	.lowPowerOwnClear = asicConnac3xLowPowerOwnClear,
	.wakeUpWiFi = asicWakeUpWiFi,
	.processSoftwareInterrupt = asicConnac3xProcessSoftwareInterrupt,
	.softwareInterruptMcu = asicConnac3xSoftwareInterruptMcu,
	.hifRst = asicConnac3xHifRst,
	.devReadIntStatus = bellwetherReadIntStatus,
	.DmaShdlInit = bellwetherDmashdlInit,
	.setRxRingHwAddr = bellwetherSetRxRingHwAddr,
	.wfdmaAllocRxRing = bellwetherWfdmaAllocRxRing,
#endif /*_HIF_PCIE || _HIF_AXI */
};

#if CFG_ENABLE_FW_DOWNLOAD
struct FWDL_OPS_T bellwether_fw_dl_ops = {
	.constructFirmwarePrio = bellwether_ConstructFirmwarePrio,
	.constructPatchName = NULL,
	.downloadPatch = NULL,
	.downloadFirmware = wlanConnacFormatDownload,
	.downloadByDynMemMap = NULL,
	.getFwInfo = wlanGetConnacFwInfo,
	.getFwDlInfo = asicGetFwDlInfo,
#if (CONFIG_ROM_CODE_DOWNLOAD == 1)
	.mcu_init = bellwetherDownloadRomCode,
#endif
};
#endif /* CFG_ENABLE_FW_DOWNLOAD */

struct TX_DESC_OPS_T bellwether_TxDescOps = {
	.fillNicAppend = fillNicTxDescAppend,
	.fillHifAppend = fillTxDescAppendByHostV2,
	.fillTxByteCount = fillConnac3xTxDescTxByteCount,
};

struct RX_DESC_OPS_T bellwether_RxDescOps = {};

#if (DBG_DISABLE_ALL_INFO == 0)
struct CHIP_DBG_OPS bellwether_DebugOps = {
	.showPdmaInfo = connac3x_show_wfdma_info,
	.showPseInfo = connac3x_show_pse_info,
	.showPleInfo = connac3x_show_ple_info,
	.showTxdInfo = connac3x_show_txd_Info,
	.showWtblInfo = connac3x_show_wtbl_info,
	.showUmacWtblInfo = connac3x_show_umac_wtbl_info,
	.get_rssi_from_wtbl = connac3x_get_rssi_from_wtbl,
	.showCsrInfo = NULL,
	.showDmaschInfo = connac3x_show_dmashdl_info,
	.showHifInfo = NULL,
	.printHifDbgInfo = NULL,
	.show_rx_rate_info = connac3x_show_rx_rate_info,
	.show_rx_rssi_info = connac3x_show_rx_rssi_info,
	.show_stat_info = connac3x_show_stat_info,
	.get_tx_info_from_txv = connac3x_get_tx_info_from_txv,
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	.show_mld_info = connac3x_show_mld_info,
#endif
	.show_wfdma_dbg_probe_info = bellwether_show_wfdma_dbg_probe_info,
	.show_wfdma_wrapper_info = bellwether_show_wfdma_wrapper_info,
};
#endif /* DBG_DISABLE_ALL_INFO */

struct mt66xx_chip_info mt66xx_chip_info_bellwether = {
	.bus_info = &bellwether_bus_info,
#if CFG_ENABLE_FW_DOWNLOAD
	.fw_dl_ops = &bellwether_fw_dl_ops,
#endif /* CFG_ENABLE_FW_DOWNLOAD */
	.prTxDescOps = &bellwether_TxDescOps,
	.prRxDescOps = &bellwether_RxDescOps,
#if (DBG_DISABLE_ALL_INFO == 0)
	.prDebugOps = &bellwether_DebugOps,
#endif
	.chip_id = BELLWETHER_CHIP_ID,
	.should_verify_chip_id = FALSE,
	.sw_sync0 = CONNAC3X_CONN_CFG_ON_CONN_ON_MISC_ADDR,
	.sw_ready_bits = WIFI_FUNC_NO_CR4_READY_BITS,
	.sw_ready_bit_offset =
		Connac3x_CONN_CFG_ON_CONN_ON_MISC_DRV_FM_STAT_SYNC_SHFT,
	.is_support_cr4 = FALSE,
	.is_support_wacpu = FALSE,
	.sw_sync_emi_info = NULL,
	.txd_append_size = BELLWETHER_TX_DESC_APPEND_LENGTH,
	.rxd_size = BELLWETHER_RX_DESC_LENGTH,
	.init_evt_rxd_size = BELLWETHER_RX_INIT_DESC_LENGTH,
	.pse_header_length = CONNAC3X_NIC_TX_PSE_HEADER_LENGTH,
	.init_event_size = CONNAC3X_RX_INIT_EVENT_LENGTH,
	.eco_info = bellwether_eco_table,
	.isNicCapV1 = FALSE,
	.top_hcr = CONNAC3X_TOP_HCR,
	.top_hvr = CONNAC3X_TOP_HVR,
	.top_fvr = CONNAC3X_TOP_FVR,
#if (CFG_SUPPORT_802_11AX == 1)
	.arb_ac_mode_addr = BELLWETHER_ARB_AC_MODE_ADDR,
#endif
	.asicCapInit = asicConnac3xCapInit,
#if CFG_ENABLE_FW_DOWNLOAD
	.asicEnableFWDownload = NULL,
#endif /* CFG_ENABLE_FW_DOWNLOAD */
	.downloadBufferBin = wlanConnac3XDownloadBufferBin,
	.is_support_hw_amsdu = TRUE,
	.is_support_asic_lp = TRUE,
	.asicWfdmaReInit = asicConnac3xWfdmaReInit,
	.asicWfdmaReInit_handshakeInit = asicConnac3xWfdmaDummyCrWrite,
	.group5_size = sizeof(struct HW_MAC_RX_STS_GROUP_5),
	.u4LmacWtblDUAddr = CONNAC3X_WIFI_LWTBL_BASE,
	.u4UmacWtblDUAddr = CONNAC3X_WIFI_UWTBL_BASE,
	.isSupportMddpAOR = false,
	.u4HostWfdmaBaseAddr = WF_WFDMA_HOST_DMA0_BASE,
	.u4HostWfdmaWrapBaseAddr = 0x7c027000,
	.u4McuWfdmaBaseAddr = WF_WFDMA_MCU_DMA0_BASE,
	.u4DmaShdlBaseAddr = WF_HIF_DMASHDL_TOP_BASE,
	.cmd_max_pkt_size = CFG_TX_MAX_PKT_SIZE, /* size 1600 */
	.chip_capability = BIT(CHIP_CAPA_FW_LOG_TIME_SYNC),

	.u4MinTxLen = 2,
};

struct mt66xx_hif_driver_data mt66xx_driver_data_bellwether = {
	.chip_info = &mt66xx_chip_info_bellwether,
};

static void bellwether_ConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx)
{
	int ret = 0;
	uint8_t ucIdx = 0;

	for (ucIdx = 0; apucbellwetherFwName[ucIdx]; ucIdx++) {
		if ((*pucNameIdx + 3) >= ucMaxNameIdx) {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
				"kalFirmwareImageMapping >> file name array is not enough.\n");
			ASSERT(0);
			continue;
		}

		/* Type 1. WIFI_RAM_CODE_bellwether.bin */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s.bin",
				apucbellwetherFwName[ucIdx]);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);
	}
}

static uint8_t bellwetherSetRxRingHwAddr(struct RTMP_RX_RING *prRxRing,
		struct BUS_INFO *prBusInfo, uint32_t u4SwRingIdx)
{
	uint32_t offset = 0;

	/*
	 * RX_RING_EVT           (RX_Ring0) - WM Event
	 * RX_RING_DATA0          (RX_Ring4) - Band0 Rx Data
	 * RX_RING_DATA1         (RX_Ring5) - Band1 Rx Data
	 * RX_RING_TXDONE0       (RX_Ring6) - Band0 Tx Free Done Event
	 * RX_RING_TXDONE1       (RX_Ring7) - Band1 Tx Free Done Event
	 * RX_RING_DATA2         (RX_Ring8) - Band2 Rx Data
	 * RX_RING_TXDONE2       (RX_Ring9) - Band2 Tx Free Done Event
	*/
	switch (u4SwRingIdx) {
	case RX_RING_EVT:
		offset = 0;
		break;
	case RX_RING_DATA0:
		offset = 4;
		break;
	case RX_RING_DATA1:
		offset = 5;
		break;
	case RX_RING_DATA2:
		offset = 8;
		break;
	case RX_RING_TXDONE0:
		offset = 6;
		break;
	case RX_RING_TXDONE1:
		offset = 7;
		break;
	case RX_RING_TXDONE2:
		offset = 9;
		break;
	default:
		return FALSE;
	}

	halSetRxRingHwAddr(prRxRing, prBusInfo, offset);

	return TRUE;
}

static bool bellwetherWfdmaAllocRxRing(struct GLUE_INFO *prGlueInfo,
		bool fgAllocMem)
{
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;

	/* Band1 Data Rx path */
	if (!halWpdmaAllocRxRing(prGlueInfo,
			RX_RING_DATA1, prHifInfo->u4RxDataRingSize,
			RXD_SIZE, CFG_RX_MAX_MPDU_SIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[2] fail\n");
		return false;
	}
	if (!halWpdmaAllocRxRing(prGlueInfo,
			RX_RING_DATA2, prHifInfo->u4RxDataRingSize,
			RXD_SIZE, CFG_RX_MAX_MPDU_SIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[5] fail\n");
		return false;
	}
	/* Band0 Tx Free Done Event */
	if (!halWpdmaAllocRxRing(prGlueInfo,
			RX_RING_TXDONE0, prHifInfo->u4RxEvtRingSize,
			RXD_SIZE, RX_BUFFER_AGGRESIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[3] fail\n");
		return false;
	}
	if (!halWpdmaAllocRxRing(prGlueInfo,
			RX_RING_TXDONE1, prHifInfo->u4RxEvtRingSize,
			RXD_SIZE, RX_BUFFER_AGGRESIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[4] fail\n");
		return false;
	}
	if (!halWpdmaAllocRxRing(prGlueInfo,
			RX_RING_TXDONE2, prHifInfo->u4RxEvtRingSize,
			RXD_SIZE, RX_BUFFER_AGGRESIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[6] fail\n");
		return false;
	}
	return true;
}

static void bellwetherProcessTxInterrupt(
		struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	uint32_t u4Sta = prHifInfo->u4IntStatus;

	if (u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_16_MASK)
		halWpdmaProcessCmdDmaDone(
			prAdapter->prGlueInfo, TX_RING_FWDL);

	if (u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_17_MASK)
		halWpdmaProcessCmdDmaDone(
			prAdapter->prGlueInfo, TX_RING_CMD);

#if (CFG_SUPPORT_DISABLE_DATA_DDONE_INTR == 0)
	if (u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_0_MASK) {
		halWpdmaProcessDataDmaDone(
			prAdapter->prGlueInfo, TX_RING_DATA0);
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
	}

	if (u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_1_MASK) {
		halWpdmaProcessDataDmaDone(
			prAdapter->prGlueInfo, TX_RING_DATA1);
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
	}

	if (u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_2_MASK) {
		halWpdmaProcessDataDmaDone(
			prAdapter->prGlueInfo, TX_RING_DATA_PRIO);
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
	}

	if (u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_tx_done_int_sts_3_MASK) {
		halWpdmaProcessDataDmaDone(
			prAdapter->prGlueInfo, TX_RING_DATA_ALTX);
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
	}
#endif /* CFG_SUPPORT_DISABLE_DATA_DDONE_INTR == 0 */
}

static void bellwetherProcessRxInterrupt(
	struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	uint32_t u4Sta = prHifInfo->u4IntStatus;

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_4_MASK) ||
	    (KAL_TEST_BIT(RX_RING_DATA0, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_DATA0, TRUE);

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_5_MASK) ||
	    (KAL_TEST_BIT(RX_RING_DATA1, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_DATA1, TRUE);

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_8_MASK) ||
	    (KAL_TEST_BIT(RX_RING_DATA2, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_DATA2, TRUE);

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_0_MASK) ||
	    (KAL_TEST_BIT(RX_RING_EVT, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_EVT, FALSE);

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_6_MASK) ||
	    (KAL_TEST_BIT(RX_RING_TXDONE0, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_TXDONE0, FALSE);

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_7_MASK) ||
	    (KAL_TEST_BIT(RX_RING_TXDONE1, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_TXDONE1, FALSE);

	if ((u4Sta & WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_9_MASK) ||
	    (KAL_TEST_BIT(RX_RING_TXDONE2, prAdapter->ulNoMoreRfb)))
		halRxReceiveRFBs(prAdapter, RX_RING_TXDONE2, FALSE);
}

static void bellwetherWfdmaManualPrefetch(
	struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	u_int32_t val = 0;
	uint32_t u4WrVal = 0x00000004, u4Addr = 0;

	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &val);
	/* disable prefetch offset calculation auto-mode */
	val &=
	~WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN_MASK;
	HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, val);

	/* Rx ring */
	for (u4Addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_EXT_CTRL_ADDR;
	     u4Addr <= WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_EXT_CTRL_ADDR;
	     u4Addr += 0x4) {
		HAL_MCR_WR(prAdapter, u4Addr, u4WrVal);
		u4WrVal += 0x00400000;
	}

	for (u4Addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING4_EXT_CTRL_ADDR;
	     u4Addr <= WF_WFDMA_HOST_DMA0_WPDMA_RX_RING9_EXT_CTRL_ADDR;
	     u4Addr += 0x4) {
		HAL_MCR_WR(prAdapter, u4Addr, u4WrVal);
		u4WrVal += 0x00400000;
	}

	/* Tx ring */
	for (u4Addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR;
	     u4Addr <= WF_WFDMA_HOST_DMA0_WPDMA_TX_RING3_EXT_CTRL_ADDR;
	     u4Addr += 0x4) {
		HAL_MCR_WR(prAdapter, u4Addr, u4WrVal);
		u4WrVal += 0x00400000;
	}

	for (u4Addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING16_EXT_CTRL_ADDR;
	     u4Addr <= WF_WFDMA_HOST_DMA0_WPDMA_TX_RING17_EXT_CTRL_ADDR;
	     u4Addr += 0x4) {
		HAL_MCR_WR(prAdapter, u4Addr, u4WrVal);
		u4WrVal += 0x00400000;
	}

	/* reset dma TRX idx */
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RST_DTX_PTR_ADDR, 0xFFFFFFFF);
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_WPDMA_RST_DRX_PTR_ADDR, 0xFFFFFFFF);
}

static void bellwetherReadIntStatus(struct ADAPTER *prAdapter,
		uint32_t *pu4IntStatus)
{
	uint32_t u4RegValue = 0, u4WrValue = 0;
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	*pu4IntStatus = 0;

	HAL_MCR_RD(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, &u4RegValue);

	if (HAL_IS_CONNAC3X_EXT_RX_DONE_INTR(
		    u4RegValue, prBusInfo->host_int_rxdone_bits)) {
		*pu4IntStatus |= WHISR_RX0_DONE_INT;
		u4WrValue |= (u4RegValue & prBusInfo->host_int_rxdone_bits);
	}

	if (HAL_IS_CONNAC3X_EXT_TX_DONE_INTR(
		    u4RegValue, prBusInfo->host_int_txdone_bits)) {
		*pu4IntStatus |= WHISR_TX_DONE_INT;
		u4WrValue |= (u4RegValue & prBusInfo->host_int_txdone_bits);
	}

	if (u4RegValue & CONNAC_MCU_SW_INT) {
		*pu4IntStatus |= WHISR_D2H_SW_INT;
		u4WrValue |= (u4RegValue & CONNAC_MCU_SW_INT);
	}

	prHifInfo->u4IntStatus = u4RegValue;

	/* clear interrupt */
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, u4WrValue);
}

static void bellwetherConfigIntMask(struct GLUE_INFO *prGlueInfo,
		u_int8_t enable)
{
	uint32_t u4Addr = 0, u4WrVal = 0, u4Val = 0;

	u4Addr = WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR;
	u4WrVal =
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA0_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA4_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA5_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA6_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA7_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA8_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA9_MASK |
#if (CFG_SUPPORT_DISABLE_DATA_DDONE_INTR == 0)
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_TX_DONE_INT_ENA0_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_TX_DONE_INT_ENA1_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_TX_DONE_INT_ENA2_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_TX_DONE_INT_ENA3_MASK |
#endif /* CFG_SUPPORT_DISABLE_DATA_DDONE_INTR == 0 */
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_TX_DONE_INT_ENA16_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_TX_DONE_INT_ENA17_MASK |
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_mcu2host_sw_int_ena_MASK;

	HAL_MCR_WR(prGlueInfo->prAdapter, u4Addr, u4WrVal);

	HAL_MCR_RD(prGlueInfo->prAdapter,
		   WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &u4Val);

	DBGLOG(HAL, TRACE,
	       "HOST_INT_ENA(0x%08x):0x%08x, En:%u, WrVal:0x%08x\n",
	       WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR,
	       u4Val,
	       enable,
	       u4WrVal);
}

static void bellwetherWpdmaConfig(struct GLUE_INFO *prGlueInfo,
		u_int8_t enable, bool fgResetHif)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	union WPDMA_GLO_CFG_STRUCT GloCfg;
	uint32_t u4DmaCfgCr;
	uint32_t idx = 0;

	asicConnac3xWfdmaControl(prGlueInfo, 0, enable);
	u4DmaCfgCr = asicConnac3xWfdmaCfgAddrGet(prGlueInfo, 0);
	HAL_MCR_RD(prAdapter, u4DmaCfgCr, &GloCfg.word);

	bellwetherConfigIntMask(prGlueInfo, enable);

	if (enable) {
		u4DmaCfgCr = asicConnac3xWfdmaCfgAddrGet(prGlueInfo, idx);
		GloCfg.field_conn3x.tx_dma_en = 1;
		GloCfg.field_conn3x.rx_dma_en = 1;
		HAL_MCR_WR(prAdapter, u4DmaCfgCr, GloCfg.word);
	}
}

static void bellwetherWfdmaRxRingExtCtrl(struct GLUE_INFO *prGlueInfo,
	struct RTMP_RX_RING *rx_ring,
	u_int32_t index)
{
	struct ADAPTER *prAdapter;
	struct mt66xx_chip_info *prChipInfo;
	struct BUS_INFO *prBusInfo;
	uint32_t ext_offset;

	prAdapter = prGlueInfo->prAdapter;
	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	switch (index) {
	case RX_RING_DATA0:
		ext_offset = 4 * 4;
		break;
	case RX_RING_EVT:
		ext_offset = 0 * 4;
		break;
	case RX_RING_DATA1:
		ext_offset = 5 * 4;
		break;
	case RX_RING_TXDONE0:
		ext_offset = 6 * 4;
		break;
	case RX_RING_TXDONE1:
		ext_offset = 7 * 4;
		break;
	case RX_RING_DATA2:
		ext_offset = 8 * 4;
		break;
	case RX_RING_TXDONE2:
		ext_offset = 9 * 4;
		break;
	default:
		DBGLOG(RX, ERROR, "Error index=%d\n", index);
		return;
	}

	rx_ring->hw_desc_base_ext =
		prBusInfo->host_rx_ring_ext_ctrl_base + ext_offset;

	HAL_MCR_WR(prAdapter, rx_ring->hw_desc_base_ext,
		   CONNAC3X_RX_RING_DISP_MAX_CNT);
}

#if defined(_HIF_PCIE)
static void bellwetherInitPcieInt(struct GLUE_INFO *prGlueInfo)
{
	HAL_MCR_WR(prGlueInfo->prAdapter,
		PCIE_MAC_IREG_IMASK_HOST_ADDR,
		PCIE_MAC_IREG_IMASK_HOST_DMA_END_EN_MASK);
}
#endif

static int __load_rom_binary(struct ADAPTER *prAdapter,
	const struct firmware **fw,
	uint8_t **name_table)
{
	const struct firmware *temp;
	uint8_t idx = 0;
	int ret = 0;

	for (idx = 0; name_table[idx]; idx++) {
		ret = request_firmware(&temp,
				       name_table[idx],
				       prAdapter->prGlueInfo->prDev);

		if (ret) {
			DBGLOG(INIT, WARN,
				"Request FW image: %s failed, ret: %d\n",
				name_table[idx], ret);
			continue;
		}

		DBGLOG(INIT, DEBUG,
			"Request FW ROM image: %s done, size: 0x%zx\n",
			name_table[idx],
			temp->size);
		*fw = temp;
		return 0;
	}
	return -EINVAL;
}

static uint32_t __load_rom_code(struct ADAPTER *prAdapter,
	uint8_t **name_table, uint32_t addr)
{
	const struct firmware *fw = NULL;
	uint32_t ret = WLAN_STATUS_SUCCESS;

	if (__load_rom_binary(prAdapter, &fw, name_table))
		return WLAN_STATUS_FAILURE;

	if (!fw->data || !fw->size) {
		DBGLOG(INIT, WARN,
			"fw->data: 0x%p, fw->size: %d\n",
			fw->data, fw->size);
		ret = WLAN_STATUS_INVALID_DATA;
		goto exit;
	}

	kalDevRegWriteRange(prAdapter->prGlueInfo,
		addr, (void *)fw->data, fw->size);

exit:
	release_firmware(fw);
	return ret;
}

#if CFG_MTK_FPGA_PLATFORM
static void __init_pcie_settings(struct ADAPTER *prAdapter)
{
	/*
	 * 0x7C048308 = 0x17400
	 * 0x7C048320 = 0x0
	 */
	HAL_MCR_WR(prAdapter, CONN_BUS_CR_ADDR_MD_SHARED_BASE_ADDR_ADDR, 0x17400);
	HAL_MCR_WR(prAdapter, CONN_BUS_CR_ADDR_CONN2AP_REMAP_BYPASS_ADDR, 0x0);

	/*
	 * 0x74030D00 = 0x10000
	 */
	HAL_MCR_WR(prAdapter, PCIE_MAC_IREG_PCIE_DEBUG_DUMMY_0_ADDR, 0x10000);
}
#endif

static u_int8_t check_mcu_idle(struct ADAPTER *prAdapter)
{
#define MCU_IDLE		0x1D1E

	uint32_t u4Value = 0;

	HAL_MCR_RD(prAdapter, WF_TOP_CFG_ON_ROMCODE_INDEX_ADDR,
		&u4Value);

	return u4Value == MCU_IDLE ? TRUE : FALSE;
}

static uint32_t __polling_wf_mcu_idle(struct ADAPTER *prAdapter)
{
#define MCU_IDLE_POLL_ROUND	100
#define MCU_IDLE_POLL_US	1000

	uint32_t u4Value = 0;
	uint32_t i = 0;
	u_int8_t fgIdle = FALSE;

	do {
		if (check_mcu_idle(prAdapter) == TRUE) {
			fgIdle = TRUE;
			break;
		}

		kalUdelay(MCU_IDLE_POLL_US);
	} while ((i++) < MCU_IDLE_POLL_ROUND);

	DBGLOG(INIT, DEBUG, "u4Value: 0x%x, fgIdle: %d\n", u4Value, fgIdle);

	return (fgIdle == TRUE ? WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE);
}

static uint32_t __polling_wf_task_idle(struct ADAPTER *prAdapter)
{
#define WF_IDLE			0xBE11
#define WF_IDLE_POLL_ROUND	5000
#define WF_IDLE_POLL_US		1000

	uint32_t u4Value = 0;
	uint32_t i = 0;
	u_int8_t fgIdle = FALSE;

	do {
		HAL_MCR_RD(prAdapter, CONN_INFRA_CR_SW_DEF_MCU_ENTER_IDLE_LOOP,
			&u4Value);
		if (u4Value == WF_IDLE) {
			fgIdle = TRUE;
			break;
		}

		kalUdelay(WF_IDLE_POLL_US);
	} while ((i++) < WF_IDLE_POLL_ROUND);

	DBGLOG(INIT, DEBUG, "u4Value: 0x%x, fgIdle: %d\n", u4Value, fgIdle);

	return (fgIdle == TRUE ? WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE);
}

static uint32_t bellwetherDownloadRomCode(struct ADAPTER *prAdapter)
{
	uint32_t u4PollingCnt = 0;
	uint32_t u4Value = 0;
	uint32_t ret = WLAN_STATUS_SUCCESS;

	/* Force wake up conn_infra */
	HAL_MCR_RD(prAdapter, CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_ADDR, &u4Value);
	u4Value |= CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP_MASK;
	HAL_MCR_WR(prAdapter, CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_ADDR, u4Value);

	/* Polling conn_infra ID */
	u4PollingCnt = 0;
	do {
		HAL_MCR_RD(prAdapter, CONN_INFRA_CFG_IP_VERSION_ADDR, &u4Value);
		if (u4Value == CONNSYS_VERSION_ID)
			break;
		else if (u4PollingCnt > 100) {
			DBGLOG(INIT, DEBUG,
			       "(%d) Polling conninfra id failed, value=0x%x.\n",
			       __LINE__, u4Value);
			ret = WLAN_STATUS_FAILURE;
			goto exit2;
		}
		udelay(1000);
	} while(true);

	/* Polling conn_infra_ready */
	u4PollingCnt = 0;
	do {
		HAL_MCR_RD(prAdapter, CONN_INFRA_CFG_ON_CONN_INFRA_CFG_PWRCTRL1_ADDR, &u4Value);
		if (u4Value & CONN_INFRA_CFG_ON_CONN_INFRA_CFG_PWRCTRL1_CONN_INFRA_RDY_MASK)
			break;
		else if (u4PollingCnt > 100) {
			DBGLOG(INIT, DEBUG,
			       "Polling conninfra ready failed, value=0x%x.\n",
			       u4Value);
			ret = WLAN_STATUS_FAILURE;
			goto exit1;
		}
		udelay(1000);
	} while(true);

	if (check_mcu_idle(prAdapter)) {
		ret = WLAN_STATUS_SUCCESS;
		goto exit1;
	}

	/* Enable wf related slp prot */
	HAL_MCR_RD(prAdapter, CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_CTRL_ADDR, &u4Value);
	u4Value |= CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_CTRL_CFG_CONN_WF_SLP_PROT_SW_EN_MASK;
	HAL_MCR_WR(prAdapter, CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_CTRL_ADDR, u4Value);

	/* Check slp prot rdy */
	u4PollingCnt = 0;
	do {
		HAL_MCR_RD(prAdapter, CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_STATUS_ADDR, &u4Value);
		if ((u4Value & CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_STATUS_WFDMA2CONN_SLP_PROT_RDY_MASK) &&
			(u4Value & CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_STATUS_CONN2WF_SLP_PROT_RDY_MASK) &&
			(u4Value & CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_STATUS_WF2CONN_SLP_PROT_RDY_MASK))
			break;
		else if (u4PollingCnt > 100) {
			DBGLOG(INIT, DEBUG,
			       "Polling slp prot rdy failed, value=0x%x.\n",
			       u4Value);
			ret = WLAN_STATUS_FAILURE;
			goto exit1;
		}
		udelay(1000);
	} while(true);

	/* Reset wfsys */
	HAL_MCR_RD(prAdapter, CONN_INFRA_RGU_ON_WFSYS_SW_RST_B_ADDR, &u4Value);
	u4Value &= ~CONN_INFRA_RGU_ON_WFSYS_SW_RST_B_WFSYS_SW_RST_B_MASK;
	HAL_MCR_WR(prAdapter, CONN_INFRA_RGU_ON_WFSYS_SW_RST_B_ADDR, u4Value);

	/* Wf mcu reset */
	HAL_MCR_RD(prAdapter, CONN_INFRA_RGU_ON_WFSYS_CPU_SW_RST_B_ADDR, &u4Value);
	u4Value &= ~CONN_INFRA_RGU_ON_WFSYS_CPU_SW_RST_B_WFSYS_CPU_SW_RST_B_MASK;
	HAL_MCR_WR(prAdapter, CONN_INFRA_RGU_ON_WFSYS_CPU_SW_RST_B_ADDR, u4Value);

	/* Disable wf related slp prot */
	HAL_MCR_RD(prAdapter, CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_CTRL_ADDR, &u4Value);
	u4Value &= ~CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_CTRL_CFG_CONN_WF_SLP_PROT_SW_EN_MASK;
	HAL_MCR_WR(prAdapter, CONN_INFRA_CFG_ON_CONN_INFRA_WF_SLP_CTRL_ADDR, u4Value);

	/* Release wf reset */
	HAL_MCR_RD(prAdapter, CONN_INFRA_RGU_ON_WFSYS_SW_RST_B_ADDR, &u4Value);
	u4Value |= CONN_INFRA_RGU_ON_WFSYS_SW_RST_B_WFSYS_SW_RST_B_MASK;
	HAL_MCR_WR(prAdapter, CONN_INFRA_RGU_ON_WFSYS_SW_RST_B_ADDR, u4Value);

	/* Polling conn_infra ID */
	u4PollingCnt = 0;
	do {
		HAL_MCR_RD(prAdapter, CONN_INFRA_CFG_IP_VERSION_ADDR, &u4Value);
		if (u4Value == CONNSYS_VERSION_ID)
			break;
		else if (u4PollingCnt > 100) {
			DBGLOG(INIT, DEBUG,
			       "(%d) Polling conninfra id failed, value=0x%x.\n",
			       __LINE__, u4Value);
			ret = WLAN_STATUS_FAILURE;
			goto exit1;
		}
		udelay(1000);
	} while(true);

	ret = __load_rom_code(prAdapter,
		apucBellFwRomName,
		BELLWETHER_FIRMWARE_ROM_ADDR);
	if (ret != WLAN_STATUS_SUCCESS)
		goto exit1;

	ret = __load_rom_code(prAdapter,
		apucBellFwRomSramName,
		BELLWETHER_FIRMWARE_ROM_SRAM_ADDR);
	if (ret != WLAN_STATUS_SUCCESS)
		goto exit1;

#if CFG_MTK_FPGA_PLATFORM
	__init_pcie_settings(prAdapter);
#endif

	HAL_MCR_RD(prAdapter, CONN_INFRA_RGU_ON_WFSYS_CPU_SW_RST_B_ADDR, &u4Value);
	u4Value |= CONN_INFRA_RGU_ON_WFSYS_CPU_SW_RST_B_WFSYS_CPU_SW_RST_B_MASK;
	HAL_MCR_WR(prAdapter, CONN_INFRA_RGU_ON_WFSYS_CPU_SW_RST_B_ADDR, u4Value);

	ret = __polling_wf_mcu_idle(prAdapter);
	if (ret != WLAN_STATUS_SUCCESS)
		goto exit1;

	ret = __polling_wf_task_idle(prAdapter);
	if (ret != WLAN_STATUS_SUCCESS)
		goto exit1;

exit1:
	/* Force wake up conn_infra */
	HAL_MCR_RD(prAdapter, CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_ADDR, &u4Value);
	u4Value &= ~CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP_MASK;
	HAL_MCR_WR(prAdapter, CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_ADDR, u4Value);

exit2:
	DBGLOG(INIT, DEBUG, "ret: 0x%lx\n", ret);
	return ret;
}

#endif  /* bellwether */
