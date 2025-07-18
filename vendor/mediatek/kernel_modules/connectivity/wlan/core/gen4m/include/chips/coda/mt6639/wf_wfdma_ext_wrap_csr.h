/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __WF_WFDMA_EXT_WRAP_CSR_REGS_H__
#define __WF_WFDMA_EXT_WRAP_CSR_REGS_H__

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WF_WFDMA_EXT_WRAP_CSR_BASE \
	(0x18027000 + CONN_INFRA_REMAPPING_OFFSET)

#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0x2C)
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0x30)
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MD_INT_LUMP_SEL \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0xAC)
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HIF_PERF_MAVG_DIV_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0xC0)
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0xE8)
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0XF0)
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0XF4)
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0XF8)
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR \
	(WF_WFDMA_EXT_WRAP_CSR_BASE + 0XFC)


#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie1_md_msi_num_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie1_md_msi_num_MASK \
	0x000C0000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie1_md_msi_num_SHFT \
	18
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie0_md_msi_num_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie0_md_msi_num_MASK \
	0x00030000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie0_md_msi_num_SHFT \
	16
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_conn2ap_irq_mode_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_conn2ap_irq_mode_MASK \
	0x00008000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_conn2ap_irq_mode_SHFT \
	15
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie_dly_rx_int_en_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie_dly_rx_int_en_MASK \
	0x00000200
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie_dly_rx_int_en_SHFT \
	9
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_usb_cmdpkt_dst_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_usb_cmdpkt_dst_MASK \
	0x00000080
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_usb_cmdpkt_dst_SHFT \
	7
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_usb_rxevt_ep4_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_usb_rxevt_ep4_MASK \
	0x00000040
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_usb_rxevt_ep4_SHFT \
	6
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie1_msi_num_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie1_msi_num_MASK \
	0x00000030
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie1_msi_num_SHFT \
	4
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie0_msi_num_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie0_msi_num_MASK \
	0x0000000C
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pcie0_msi_num_SHFT \
	2
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_host_wed_en_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_host_wed_en_MASK \
	0x00000002
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_host_wed_en_SHFT \
	1
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pdma_per_band_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pdma_per_band_MASK \
	0x00000001
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pdma_per_band_SHFT \
	0

#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_WM_EVT_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_WM_EVT_RING_IDX_MASK \
	0xF0000000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_WM_EVT_RING_IDX_SHFT \
	28
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_WA_EVT_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_WA_EVT_RING_IDX_MASK \
	0x0F000000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_WA_EVT_RING_IDX_SHFT \
	24
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_WM_EVT_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_WM_EVT_RING_IDX_MASK \
	0x00F00000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_WM_EVT_RING_IDX_SHFT \
	20
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_WA_EVT_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_WA_EVT_RING_IDX_MASK \
	0x000F0000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_WA_EVT_RING_IDX_SHFT \
	16
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_BN1_TX_FREE_DONE_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_BN1_TX_FREE_DONE_RING_IDX_MASK \
	0x0000F000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_BN1_TX_FREE_DONE_RING_IDX_SHFT \
	12
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_BN0_TX_FREE_DONE_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_BN0_TX_FREE_DONE_RING_IDX_MASK \
	0x00000F00
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_MD_BN0_TX_FREE_DONE_RING_IDX_SHFT \
	8
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN1_TX_FREE_DONE_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN1_TX_FREE_DONE_RING_IDX_MASK \
	0x000000F0
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN1_TX_FREE_DONE_RING_IDX_SHFT \
	4
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN0_TX_FREE_DONE_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN0_TX_FREE_DONE_RING_IDX_MASK \
	0x0000000F
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN0_TX_FREE_DONE_RING_IDX_SHFT \
	0

#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN1_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN1_TX_RING_IDX_MAX_MASK \
	0xF0000000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN1_TX_RING_IDX_MAX_SHFT \
	28
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN1_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN1_TX_RING_IDX_MIN_MASK \
	0x0F000000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN1_TX_RING_IDX_MIN_SHFT \
	24
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN0_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN0_TX_RING_IDX_MAX_MASK \
	0x00F00000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN0_TX_RING_IDX_MAX_SHFT \
	20
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN0_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN0_TX_RING_IDX_MIN_MASK \
	0x000F0000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_MD_BN0_TX_RING_IDX_MIN_SHFT \
	16
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MAX_MASK \
	0x0000F000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MAX_SHFT \
	12
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MIN_MASK \
	0x00000F00
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MIN_SHFT \
	8
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MAX_MASK \
	0x000000F0
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MAX_SHFT \
	4
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MIN_MASK \
	0x0000000F
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MIN_SHFT \
	0

#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_PCIE1_BN1_TX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_PCIE1_BN1_TX_RING_IDX_MASK \
	0x00F00000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_PCIE1_BN1_TX_RING_IDX_SHFT \
	20
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_FWDL_TX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_FWDL_TX_RING_IDX_MASK \
	0x000F0000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_FWDL_TX_RING_IDX_SHFT \
	16
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_MD_WA_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_MD_WA_CMD_RING_IDX_MASK \
	0x0000F000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_MD_WA_CMD_RING_IDX_SHFT \
	12
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_MD_WM_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_MD_WM_CMD_RING_IDX_MASK \
	0x00000F00
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_MD_WM_CMD_RING_IDX_SHFT \
	8
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_AP_WA_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_AP_WA_CMD_RING_IDX_MASK \
	0x000000F0
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_AP_WA_CMD_RING_IDX_SHFT \
	4
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_AP_WM_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_AP_WM_CMD_RING_IDX_MASK \
	0x0000000F
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_AP_WM_CMD_RING_IDX_SHFT \
	0

#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_RX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_RX_RING_IDX_MASK \
	0xF0000000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_RX_RING_IDX_SHFT \
	28
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_RX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_RX_RING_IDX_MASK \
	0x0F000000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_RX_RING_IDX_SHFT \
	24
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_RX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_RX_RING_IDX_MASK \
	0x00F00000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_RX_RING_IDX_SHFT \
	20
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_RX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_RX_RING_IDX_MASK \
	0x000F0000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_RX_RING_IDX_SHFT \
	16
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_PCIE1_BN1_TX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_PCIE1_BN1_TX_RING_IDX_MASK \
	0x00002000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_PCIE1_BN1_TX_RING_IDX_SHFT \
	13
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_FWDL_TX_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_FWDL_TX_RING_IDX_MASK \
	0x00001000
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_FWDL_TX_RING_IDX_SHFT \
	12
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_WA_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_WA_CMD_RING_IDX_MASK \
	0x00000800
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_WA_CMD_RING_IDX_SHFT \
	11
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_WM_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_WM_CMD_RING_IDX_MASK \
	0x00000400
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_WM_CMD_RING_IDX_SHFT \
	10
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_WA_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_WA_CMD_RING_IDX_MASK \
	0x00000200
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_WA_CMD_RING_IDX_SHFT \
	9
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_WM_CMD_RING_IDX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_WM_CMD_RING_IDX_MASK \
	0x00000100
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_WM_CMD_RING_IDX_SHFT \
	8
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_TX_RING_IDX_MAX_MASK \
	0x00000080
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_TX_RING_IDX_MAX_SHFT \
	7
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_TX_RING_IDX_MIN_MASK \
	0x00000040
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN1_TX_RING_IDX_MIN_SHFT \
	6
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_TX_RING_IDX_MAX_MASK \
	0x00000020
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_TX_RING_IDX_MAX_SHFT \
	5
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_TX_RING_IDX_MIN_MASK \
	0x00000010
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_MD_BN0_TX_RING_IDX_MIN_SHFT \
	4
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MAX_MASK \
	0x00000008
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MAX_SHFT \
	3
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MIN_MASK \
	0x00000004
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MIN_SHFT \
	2
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MAX_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MAX_MASK \
	0x00000002
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MAX_SHFT \
	1
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MIN_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MIN_MASK \
	0x00000001
#define WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MIN_SHFT \
	0

#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_msi_deassert_tmr_ticks_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_msi_deassert_tmr_ticks_MASK \
	0xFF000000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_msi_deassert_tmr_ticks_SHFT 24
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_msi_deassert_tmr_en_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_msi_deassert_tmr_en_MASK \
	0x00800000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_msi_deassert_tmr_en_SHFT 23
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie1_msi_ack_mode_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie1_msi_ack_mode_MASK \
	0x00070000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie1_msi_ack_mode_SHFT 16
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie0_md_msi_ack_mode_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie0_md_msi_ack_mode_MASK \
	0x00007F00
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie0_md_msi_ack_mode_SHFT 8
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie0_msi_ack_mode_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie0_msi_ack_mode_MASK \
	0x0000007F
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_MSI_CONFIG_pcie0_msi_ack_mode_SHFT 0

#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA3_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA3_MASK 0x40000000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA3_SHFT 30
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR3_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR3_MASK 0x20000000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR3_SHFT 29
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX3_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX3_MASK 0x1F000000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX3_SHFT 24
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA2_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA2_MASK 0x00400000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA2_SHFT 22
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR2_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR2_MASK 0x00200000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR2_SHFT 21
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX2_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX2_MASK 0x001F0000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX2_SHFT 16
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA1_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA1_MASK 0x00004000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA1_SHFT 14
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR1_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR1_MASK 0x00002000
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR1_SHFT 13
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX1_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX1_MASK 0x00001F00
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX1_SHFT 8
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA0_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA0_MASK 0x00000040
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_ENA0_SHFT 6
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR0_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR0_MASK 0x00000020
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_FUNC_DIR0_SHFT 5
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX0_ADDR \
	WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_ADDR
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX0_MASK 0x0000001F
#define WF_WFDMA_EXT_WRAP_CSR_WFDMA_DLY_IDX_CFG_0_DLY_RING_IDX0_SHFT 0

#ifdef __cplusplus
}
#endif

#endif

