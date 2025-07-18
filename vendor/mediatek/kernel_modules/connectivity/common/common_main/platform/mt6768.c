/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[WMT-CONSYS-HW]"

#define REGION_CONN	26

#define DOMAIN_AP	0
#define DOMAIN_CONN	2

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <connectivity_build_in_adapter.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include <linux/platform_device.h>
#include "connsys_debug_utility.h"
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#include "fw_log_wmt.h"
#endif
#include "osal_typedef.h"
#include "mt6768.h"
#include "mtk_wcn_consys_hw.h"
#include "wmt_ic.h"
#include "wmt_lib.h"
#include "wmt_plat.h"
#include "stp_dbg.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
#include "soc/mediatek/emi.h"
#endif
#else
#if IS_ENABLED(CONFIG_MTK_EMI)
#include <mt_emi_api.h>
#endif
#endif

#if CONSYS_PMIC_CTRL_ENABLE
#include <linux/regulator/consumer.h>
#if (COMMON_KERNEL_PMIC_SUPPORT)
#include <linux/mfd/mt6397/core.h>
#include <linux/regmap.h>
#else
#include <upmu_common.h>
#endif
#endif

#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

#include <linux/of_reserved_mem.h>

#if (!COMMON_KERNEL_CLK_SUPPORT)
#include <mtk_clkbuf_ctl.h>
#endif

#ifdef CONFIG_MTK_DEVAPC_DRIVER
#include <devapc_public.h>
#define WMT_DEVAPC_CALLBACK
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static INT32 consys_clock_buffer_ctrl(MTK_WCN_BOOL enable);
static VOID consys_hw_reset_bit_set(MTK_WCN_BOOL enable);
static VOID consys_hw_spm_clk_gating_enable(VOID);
static INT32 consys_hw_power_ctrl(MTK_WCN_BOOL enable);
static INT32 consys_ahb_clock_ctrl(MTK_WCN_BOOL enable);
static INT32 polling_consys_chipid(VOID);
static VOID consys_acr_reg_setting(VOID);
static VOID consys_afe_reg_setting(VOID);
static INT32 consys_hw_vcn18_ctrl(MTK_WCN_BOOL enable);
static VOID consys_vcn28_hw_mode_ctrl(UINT32 enable);
static INT32 consys_hw_vcn28_ctrl(UINT32 enable);
static INT32 consys_hw_wifi_vcn33_ctrl(UINT32 enable);
static INT32 consys_hw_bt_vcn33_ctrl(UINT32 enable);
static UINT32 consys_soc_chipid_get(VOID);
static INT32 consys_emi_mpu_set_region_protection(VOID);
static UINT32 consys_emi_set_remapping_reg(VOID);
static INT32 bt_wifi_share_v33_spin_lock_init(VOID);
static INT32 consys_clk_get_from_dts(struct platform_device *pdev);
static INT32 consys_pmic_get_from_dts(struct platform_device *pdev);
static INT32 consys_read_irq_info_from_dts(struct platform_device *pdev, PINT32 irq_num, PUINT32 irq_flag);
static INT32 consys_read_reg_from_dts(struct platform_device *pdev);
static UINT32 consys_read_cpupcr(VOID);
static INT32 consys_poll_cpupcr_dump(UINT32 times, UINT32 sleep_ms);
static VOID force_trigger_assert_debug_pin(VOID);
static INT32 consys_co_clock_type(VOID);
static P_CONSYS_EMI_ADDR_INFO consys_soc_get_emi_phy_add(VOID);
static VOID consys_set_if_pinmux(MTK_WCN_BOOL enable);
static INT32 consys_dl_rom_patch(UINT32 ip_ver, UINT32 fw_ver);
static VOID consys_set_dl_rom_patch_flag(INT32 flag);
static INT32 consys_dedicated_log_path_init(struct platform_device *pdev);
static VOID consys_dedicated_log_path_deinit(VOID);
static INT32 consys_check_reg_readable(VOID);
static INT32 consys_emi_coredump_remapping(UINT8 __iomem **addr, UINT32 enable);
static INT32 consys_reset_emi_coredump(UINT8 __iomem *addr);
static VOID consys_ic_clock_fail_dump(VOID);
static INT32 consys_is_connsys_reg(UINT32 addr);
static INT32 consys_is_host_csr(SIZE_T addr);
static INT32 consys_dump_osc_state(P_CONSYS_STATE state);
static VOID consys_set_pdma_axi_rready_force_high(UINT32 enable);
static INT32 consys_calibration_backup_restore_support(VOID);
#ifdef WMT_DEVAPC_CALLBACK
static VOID consys_devapc_violation_cb(VOID);
static VOID consyc_register_devapc_cb(VOID);
#endif
static INT32 consys_is_ant_swap_enable_by_hwid(INT32 pin_num);
static UINT64 consys_get_options(VOID);

static INT32 dump_conn_mcu_pc_log_wrapper(VOID);
static INT32 consys_cmd_tx_timeout_dump(VOID);
static INT32 consys_cmd_rx_timeout_dump(VOID);
static INT32 consys_coredump_timeout_dump(VOID);
static INT32 consys_assert_timeout_dump(VOID);
static INT32 consys_before_chip_reset_dump(VOID);

static INT32 consys_jtag_set_for_mcu(VOID);
static UINT32 consys_jtag_flag_ctrl(UINT32 enable);

#if (COMMON_KERNEL_PMIC_SUPPORT)
static INT32 consys_pmic_register_device(VOID);
static int consys_pmic_mt6358_probe(struct platform_device *pdev);
#endif
#if (COMMON_KERNEL_CLK_SUPPORT)
static MTK_WCN_BOOL consys_need_store_pdev(VOID);
static UINT32 consys_store_pdev(struct platform_device *pdev);
#endif


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
#if CONSYS_BT_WIFI_SHARE_V33
struct bt_wifi_v33_status gBtWifiV33;
#endif

#if (COMMON_KERNEL_CLK_SUPPORT)
static struct platform_device *connsys_pdev;
#else
/* CCF part */
static struct clk *clk_scp_conn_main;	/*ctrl conn_power_on/off */
#endif

/* PMIC part */
#if CONSYS_PMIC_CTRL_ENABLE
static struct regulator *reg_VCN18;
static struct regulator *reg_VCN28;
static struct regulator *reg_VCN33_BT;
static struct regulator *reg_VCN33_WIFI;
#if (COMMON_KERNEL_PMIC_SUPPORT)
struct regmap *g_regmap_mt6358;
#endif
#endif

extern int g_mapped_reg_table_sz_mt6768;
extern REG_MAP_ADDR g_mapped_reg_table_mt6768[];

static EMI_CTRL_STATE_OFFSET mtk_wcn_emi_state_off = {
	.emi_apmem_ctrl_state = EXP_APMEM_CTRL_STATE,
	.emi_apmem_ctrl_host_sync_state = EXP_APMEM_CTRL_HOST_SYNC_STATE,
	.emi_apmem_ctrl_host_sync_num = EXP_APMEM_CTRL_HOST_SYNC_NUM,
	.emi_apmem_ctrl_chip_sync_state = EXP_APMEM_CTRL_CHIP_SYNC_STATE,
	.emi_apmem_ctrl_chip_sync_num = EXP_APMEM_CTRL_CHIP_SYNC_NUM,
	.emi_apmem_ctrl_chip_sync_addr = EXP_APMEM_CTRL_CHIP_SYNC_ADDR,
	.emi_apmem_ctrl_chip_sync_len = EXP_APMEM_CTRL_CHIP_SYNC_LEN,
	.emi_apmem_ctrl_chip_print_buff_start = EXP_APMEM_CTRL_CHIP_PRINT_BUFF_START,
	.emi_apmem_ctrl_chip_print_buff_len = EXP_APMEM_CTRL_CHIP_PRINT_BUFF_LEN,
	.emi_apmem_ctrl_chip_print_buff_idx = EXP_APMEM_CTRL_CHIP_PRINT_BUFF_IDX,
	.emi_apmem_ctrl_chip_int_status = EXP_APMEM_CTRL_CHIP_INT_STATUS,
	.emi_apmem_ctrl_chip_paded_dump_end = EXP_APMEM_CTRL_CHIP_PAGED_DUMP_END,
	.emi_apmem_ctrl_host_outband_assert_w1 = EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1,
	.emi_apmem_ctrl_chip_page_dump_num = EXP_APMEM_CTRL_CHIP_PAGE_DUMP_NUM,
	.emi_apmem_ctrl_assert_flag = EXP_APMEM_CTRL_ASSERT_FLAG,
};

static CONSYS_EMI_ADDR_INFO mtk_wcn_emi_addr_info = {
	.emi_phy_addr = CONSYS_EMI_FW_PHY_BASE,
	.paged_trace_off = CONSYS_EMI_PAGED_TRACE_OFFSET,
	.paged_dump_off = CONSYS_EMI_PAGED_DUMP_OFFSET,
	.full_dump_off = CONSYS_EMI_FULL_DUMP_OFFSET,
	.emi_remap_offset = CONSYS_EMI_MAPPING_OFFSET,
	.p_ecso = &mtk_wcn_emi_state_off,
	.emi_core_dump_offset = CONSYS_EMI_COREDUMP_OFFSET,
	.pda_dl_patch_flag = 1,
	.emi_met_size = (32*KBYTE),
	.emi_met_data_offset = CONSYS_EMI_MET_DATA_OFFSET,
};

WMT_CONSYS_IC_OPS consys_ic_ops_mt6768 = {
	.consys_ic_clock_buffer_ctrl = consys_clock_buffer_ctrl,
	.consys_ic_hw_reset_bit_set = consys_hw_reset_bit_set,
	.consys_ic_hw_spm_clk_gating_enable = consys_hw_spm_clk_gating_enable,
	.consys_ic_hw_power_ctrl = consys_hw_power_ctrl,
	.consys_ic_ahb_clock_ctrl = consys_ahb_clock_ctrl,
	.polling_consys_ic_chipid = polling_consys_chipid,
	.consys_ic_acr_reg_setting = consys_acr_reg_setting,
	.consys_ic_afe_reg_setting = consys_afe_reg_setting,
	.consys_ic_hw_vcn18_ctrl = consys_hw_vcn18_ctrl,
	.consys_ic_vcn28_hw_mode_ctrl = consys_vcn28_hw_mode_ctrl,
	.consys_ic_hw_vcn28_ctrl = consys_hw_vcn28_ctrl,
	.consys_ic_hw_wifi_vcn33_ctrl = consys_hw_wifi_vcn33_ctrl,
	.consys_ic_hw_bt_vcn33_ctrl = consys_hw_bt_vcn33_ctrl,
	.consys_ic_soc_chipid_get = consys_soc_chipid_get,
	.consys_ic_emi_mpu_set_region_protection = consys_emi_mpu_set_region_protection,
	.consys_ic_emi_set_remapping_reg = consys_emi_set_remapping_reg,
	.ic_bt_wifi_share_v33_spin_lock_init = bt_wifi_share_v33_spin_lock_init,
	.consys_ic_clk_get_from_dts = consys_clk_get_from_dts,
	.consys_ic_pmic_get_from_dts = consys_pmic_get_from_dts,
	.consys_ic_read_irq_info_from_dts = consys_read_irq_info_from_dts,
	.consys_ic_read_reg_from_dts = consys_read_reg_from_dts,
	.consys_ic_read_cpupcr = consys_read_cpupcr,
	.consys_ic_poll_cpupcr_dump = consys_poll_cpupcr_dump,
	.ic_force_trigger_assert_debug_pin = force_trigger_assert_debug_pin,
	.consys_ic_co_clock_type = consys_co_clock_type,
	.consys_ic_soc_get_emi_phy_add = consys_soc_get_emi_phy_add,
	.consys_ic_set_if_pinmux = consys_set_if_pinmux,
	.consys_ic_set_dl_rom_patch_flag = consys_set_dl_rom_patch_flag,
	.consys_ic_dedicated_log_path_init = consys_dedicated_log_path_init,
	.consys_ic_dedicated_log_path_deinit = consys_dedicated_log_path_deinit,
	.consys_ic_check_reg_readable = consys_check_reg_readable,
	.consys_ic_emi_coredump_remapping = consys_emi_coredump_remapping,
	.consys_ic_reset_emi_coredump = consys_reset_emi_coredump,
	.consys_ic_clock_fail_dump = consys_ic_clock_fail_dump,
	.consys_ic_is_connsys_reg = consys_is_connsys_reg,
	.consys_ic_is_host_csr = consys_is_host_csr,
	.consys_ic_dump_osc_state = consys_dump_osc_state,
	.consys_ic_set_pdma_axi_rready_force_high = consys_set_pdma_axi_rready_force_high,
	.consys_ic_calibration_backup_restore = consys_calibration_backup_restore_support,
#ifdef WMT_DEVAPC_CALLBACK
	.consys_ic_register_devapc_cb = consyc_register_devapc_cb,
#endif
	.consys_ic_is_ant_swap_enable_by_hwid = consys_is_ant_swap_enable_by_hwid,
	.consys_ic_get_options = consys_get_options,

	/* debug dump */
	.consys_ic_cmd_tx_timeout_dump = consys_cmd_tx_timeout_dump,
	.consys_ic_cmd_rx_timeout_dump = consys_cmd_rx_timeout_dump,
	.consys_ic_coredump_timeout_dump = consys_coredump_timeout_dump,
	.consys_ic_assert_timeout_dump = consys_assert_timeout_dump,
	.consys_ic_before_chip_reset_dump = consys_before_chip_reset_dump,

	.consys_ic_pc_log_dump = dump_conn_mcu_pc_log_wrapper,

	.consys_ic_jtag_set_for_mcu = consys_jtag_set_for_mcu,
	.consys_ic_jtag_flag_ctrl = consys_jtag_flag_ctrl,

	.consys_ic_get_debug_reg_ary_size = &g_mapped_reg_table_sz_mt6768,
	.consys_ic_get_debug_reg_ary = g_mapped_reg_table_mt6768,
#if (COMMON_KERNEL_PMIC_SUPPORT)
	.consys_ic_pmic_register_device = consys_pmic_register_device,
#endif
#if (COMMON_KERNEL_CLK_SUPPORT)
	.consys_ic_need_store_pdev = consys_need_store_pdev,
	.consys_ic_store_pdev = consys_store_pdev,
#endif
};

static const struct connlog_emi_config connsys_fw_log_parameter = {
	.emi_offset = 0x36500,
	.emi_size_total = (192*1024),/* 192KB */
	.emi_size_mcu = (16*1024),
	.emi_size_wifi = (64*1024),
	.emi_size_bt = (64*1024),
	.emi_size_gps = (32*1024),
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static INT32 rom_patch_dl_flag = 1;
static UINT32 gJtagCtrl;

#ifdef WMT_DEVAPC_CALLBACK
static struct devapc_vio_callbacks devapc_handle = {
	.id = INFRA_SUBSYS_CONN,
	.debug_dump = consys_devapc_violation_cb,
};
#endif

#if (COMMON_KERNEL_PMIC_SUPPORT)
#ifdef CONFIG_OF
const struct of_device_id consys_pmic_mt6358_of_ids[] = {
	{.compatible = "mediatek,mt6358-consys",},
	{}
};
#endif

static struct platform_driver consys_pmic_mt6358_dev_drv = {
	.probe = consys_pmic_mt6358_probe,
	.driver = {
		.name = "mt6358-consys",
#ifdef CONFIG_OF
		.of_match_table = consys_pmic_mt6358_of_ids,
#endif
		},
};
#endif

#if CONSYS_ENALBE_SET_JTAG
#define JTAG_ADDR1_BASE 0x10005000
#define JTAG_ADDR2_BASE 0x10002800
#define JTAG_ADDR3_BASE 0x10002600
#define AP2CONN_JTAG_2WIRE_OFFSET 0xF00
#endif

static INT32 consys_jtag_set_for_mcu(VOID)
{
	INT32 ret = 0;
#if CONSYS_ENALBE_SET_JTAG
	UINT32 tmp = 0;
	PVOID addr = 0;
	PVOID remap_addr1 = 0;
	PVOID remap_addr2 = 0;
	PVOID remap_addr3 = 0;

	if (gJtagCtrl) {
		WMT_PLAT_PR_INFO("WCN jtag set for mcu start...\n");

		switch (gJtagCtrl) {
		case 1:
			remap_addr1 = ioremap(JTAG_ADDR1_BASE, 0x1000);
			if (remap_addr1 == 0) {
				WMT_PLAT_PR_ERR("remap jtag_addr1 fail!\n");
				ret = -1;
				goto error;
			}

			remap_addr2 = ioremap(JTAG_ADDR2_BASE, 0x100);
			if (remap_addr2 == 0) {
				WMT_PLAT_PR_ERR("remap jtag_addr2 fail!\n");
				ret = -1;
				goto error;
			}

			remap_addr3 = ioremap(JTAG_ADDR3_BASE, 0x100);
			if (remap_addr3 == 0) {
				WMT_PLAT_PR_ERR("remap jtag_addr3 fail!\n");
				ret =  -1;
				goto error;
			}
			/* 7-wire jtag pinmux setting*/
			/* PAD AUX Function Selection */
			addr = remap_addr1 + 0x310;
			tmp = readl(addr);
			tmp = tmp & 0xfffff00f;
			tmp = tmp | 0x440;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			addr = remap_addr1 + 0x3c0;
			tmp = readl(addr);
			tmp = tmp & 0xf00ff00f;
			tmp = tmp | 0x4400440;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			addr = remap_addr1 + 0x370;
			tmp = readl(addr);
			tmp = tmp & 0xfffff0ff;
			tmp = tmp | 0x400;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			/* PAD Driving Selection */
			addr = remap_addr2;
			tmp = readl(addr);
			tmp = tmp & 0xffe381ff;
			tmp = tmp | 0x1c7e00;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			addr = remap_addr2 + 0x10;
			tmp = readl(addr);
			tmp = tmp & 0xfffc7fff;
			tmp = tmp | 0x38000;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			addr = remap_addr3 + 0x10;
			tmp = readl(addr);
			tmp = tmp & 0xf8ffffff;
			tmp = tmp | 0x7000000;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			/* PAD PULL Selection */
			addr = remap_addr2 + 0x60;
			tmp = readl(addr);
			tmp = tmp & 0xfffdfd87;
			tmp = tmp | 0x20278;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			addr = remap_addr3 + 0x40;
			tmp = readl(addr);
			tmp = tmp & 0xffbfffff;
			tmp = tmp | 0x400000;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);
			break;
		case 2:
			remap_addr1 = ioremap(JTAG_ADDR1_BASE, 0x1000);
			if (remap_addr1 == 0) {
				WMT_PLAT_PR_ERR("remap jtag_addr1 fail!\n");
				ret = -1;
				goto error;
			}

			remap_addr2 = ioremap(0x10002400, 0x100);
			if (remap_addr2 == 0) {
				WMT_PLAT_PR_ERR("remap jtag_addr2 fail!\n");
				ret = -1;
				goto error;
			}

			/* 2-wire jtag pinmux setting*/
			addr = remap_addr1 + 0x450;
			tmp = readl(addr);
			tmp = tmp & 0xff0fffff;
			tmp = tmp | 0x400000;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			addr = remap_addr1 + 0x430;
			tmp = readl(addr);
			tmp = tmp & 0xfff0ffff;
			tmp = tmp | 0x40000;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			/* Driving Selection */

			addr = remap_addr2;
			tmp = readl(addr);
			tmp = tmp & 0xffff99ff;
			tmp = tmp | 0x6600;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);

			/* PULL Selection */
			addr = remap_addr2 + 0x10;
			tmp = readl(addr);
			tmp = tmp & 0xfffff6ff;
			tmp = tmp | 0x900;
			writel(tmp, addr);
			tmp = readl(addr);
			WMT_PLAT_PR_INFO("(RegAddr, RegVal):(0x%p, 0x%x)\n", addr, tmp);
			break;
		default:
			WMT_PLAT_PR_INFO("unsupported options!\n");
		}

	}

error:
	if (remap_addr1)
		iounmap(remap_addr1);
	if (remap_addr2)
		iounmap(remap_addr2);
	if (remap_addr3)
		iounmap(remap_addr3);
#endif
	return ret;
}

static UINT32 consys_jtag_flag_ctrl(UINT32 enable)
{
	WMT_PLAT_PR_INFO("%s jtag set for MCU\n", enable ? "enable" : "disable");
	gJtagCtrl = enable;
	return 0;
}

#if (COMMON_KERNEL_PMIC_SUPPORT)
static INT32 consys_pmic_register_device(VOID)
{
	int ret;

	ret = platform_driver_register(&consys_pmic_mt6358_dev_drv);
	if (ret)
		WMT_PLAT_PR_INFO("WMT pmic mt6358 driver registered failed(%d)\n", ret);
	else
		WMT_PLAT_PR_INFO("%s mt6358 ok.\n", __func__);

	return 0;
}
#endif

static INT32 consys_clk_get_from_dts(struct platform_device *pdev)
{
#if (!COMMON_KERNEL_CLK_SUPPORT)
	clk_scp_conn_main = devm_clk_get(&pdev->dev, "conn");
	if (IS_ERR(clk_scp_conn_main)) {
		WMT_PLAT_PR_INFO("[CCF]cannot get clk_scp_conn_main clock.\n");
		return PTR_ERR(clk_scp_conn_main);
	}
	WMT_PLAT_PR_DBG("[CCF]clk_scp_conn_main=%p\n", clk_scp_conn_main);
#endif
	return 0;
}

#if CONSYS_PMIC_CTRL_ENABLE
#if (COMMON_KERNEL_PMIC_SUPPORT)
static int consys_pmic_mt6358_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);

	if (!mt6397) {
		WMT_PLAT_PR_INFO("%s mt6397 is NULL\n", __func__);
		return -1;
	}

	g_regmap_mt6358 = mt6397->regmap;
	if (!g_regmap_mt6358) {
		WMT_PLAT_PR_INFO("%s mt6397->regmap is NULL\n", __func__);
		return -1;
	}
	WMT_PLAT_PR_INFO("%s get regmap_mt6358 success!!\n", __func__);
	return 0;
}
#endif
#endif

static INT32 consys_pmic_get_from_dts(struct platform_device *pdev)
{
#if CONSYS_PMIC_CTRL_ENABLE
	reg_VCN18 = regulator_get(&pdev->dev, "vcn18");
	if (!reg_VCN18)
		WMT_PLAT_PR_ERR("Regulator_get VCN_1V8 fail\n");
	reg_VCN28 = regulator_get(&pdev->dev, "vcn28");
	if (!reg_VCN28)
		WMT_PLAT_PR_ERR("Regulator_get VCN_2V8 fail\n");
	reg_VCN33_BT = regulator_get(&pdev->dev, "vcn33_bt");
	if (!reg_VCN33_BT)
		WMT_PLAT_PR_ERR("Regulator_get VCN33_BT fail\n");
	reg_VCN33_WIFI = regulator_get(&pdev->dev, "vcn33_wifi");
	if (!reg_VCN33_WIFI)
		WMT_PLAT_PR_ERR("Regulator_get VCN33_WIFI fail\n");
#endif
	return 0;
}

static INT32 consys_co_clock_type(VOID)
{
	return 0;
}

static INT32 consys_clock_buffer_ctrl(MTK_WCN_BOOL enable)
{
#if (!COMMON_KERNEL_CLK_SUPPORT)
	if (enable)
		KERNEL_clk_buf_ctrl(CLK_BUF_CONN, true);	/*open XO_WCN*/
	else
		KERNEL_clk_buf_ctrl(CLK_BUF_CONN, false);	/*close XO_WCN*/
#endif
	return 0;
}

static VOID consys_set_if_pinmux(MTK_WCN_BOOL enable)
{
	UINT8 *consys_if_pinmux_reg_base = NULL;
	UINT8 *consys_if_driving_reg_base = NULL;

	/* Switch D die pinmux for connecting A die */
	consys_if_pinmux_reg_base = ioremap(CONSYS_IF_PINMUX_REG_BASE, 0x1000);
	if (!consys_if_pinmux_reg_base) {
		WMT_PLAT_PR_ERR("consys_if_pinmux_reg_base(%x) ioremap fail\n", CONSYS_IF_PINMUX_REG_BASE);
		return;
	}

	if (enable) {
		CONSYS_REG_WRITE(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_01_OFFSET,
				(CONSYS_REG_READ(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_01_OFFSET) &
				CONSYS_IF_PINMUX_01_MASK) | CONSYS_IF_PINMUX_01_VALUE);
		CONSYS_REG_WRITE(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_02_OFFSET,
				(CONSYS_REG_READ(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_02_OFFSET) &
				CONSYS_IF_PINMUX_02_MASK) | CONSYS_IF_PINMUX_02_VALUE);

		consys_if_driving_reg_base = ioremap(CONSYS_IF_DRV_PINMUX_REG_BASE, 0x10);
		if (!consys_if_driving_reg_base) {
			iounmap(consys_if_pinmux_reg_base);
			WMT_PLAT_PR_INFO("consys_if_driving_reg_base(%x) ioremap fail\n",
				CONSYS_IF_DRV_PINMUX_REG_BASE);
			return;
		}
		CONSYS_REG_WRITE(consys_if_driving_reg_base,
				(CONSYS_REG_READ(consys_if_driving_reg_base) & CONSYS_IF_DRV_PINMUX_MASK) |
				CONSYS_IF_DRV_PINMUX_VALUE);

		iounmap(consys_if_driving_reg_base);

	} else {
		CONSYS_REG_WRITE(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_01_OFFSET,
				CONSYS_REG_READ(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_01_OFFSET) &
				CONSYS_IF_PINMUX_01_MASK);
		CONSYS_REG_WRITE(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_02_OFFSET,
				CONSYS_REG_READ(consys_if_pinmux_reg_base + CONSYS_IF_PINMUX_02_OFFSET) &
				CONSYS_IF_PINMUX_02_MASK);
	}

	if (consys_if_pinmux_reg_base)
		iounmap(consys_if_pinmux_reg_base);
}

static VOID consys_hw_reset_bit_set(MTK_WCN_BOOL enable)
{
	UINT32 consys_ver_id = 0;
	UINT32 cnt = 0;

	if (enable) {
		/*3.assert CONNSYS CPU SW reset  0x10007018 "[12]=1'b1  [31:24]=8'h88 (key)" */
		CONSYS_REG_WRITE((conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET),
				CONSYS_REG_READ(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET) |
				CONSYS_CPU_SW_RST_BIT | CONSYS_CPU_SW_RST_CTRL_KEY);
	} else {
		/*16.deassert CONNSYS CPU SW reset 0x10007018 "[12]=1'b0 [31:24] =8'h88 (key)" */
		CONSYS_REG_WRITE(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET,
				(CONSYS_REG_READ(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET) &
				 ~CONSYS_CPU_SW_RST_BIT) | CONSYS_CPU_SW_RST_CTRL_KEY);

		consys_ver_id = CONSYS_REG_READ(conn_reg.mcu_base + 0x600);
		while (consys_ver_id != 0x1D1E) {
			if (cnt > 10)
				break;
			consys_ver_id = CONSYS_REG_READ(conn_reg.mcu_base + 0x600);
			WMT_PLAT_PR_INFO("0x18002600(0x%x)\n", consys_ver_id);
			WMT_PLAT_PR_INFO("0x1800216c(0x%x)\n",
					CONSYS_REG_READ(conn_reg.mcu_base + 0x16c));
			WMT_PLAT_PR_INFO("0x18007104(0x%x)\n",
					CONSYS_REG_READ(conn_reg.mcu_conn_hif_on_base + CONSYS_CPUPCR_OFFSET));
			msleep(20);
			cnt++;
		}
	}
}

static VOID consys_hw_spm_clk_gating_enable(VOID)
{
}

static INT32 consys_hw_power_ctrl(MTK_WCN_BOOL enable)
{
#if CONSYS_PWR_ON_OFF_API_AVAILABLE
	INT32 iRet = 0;
#else
	INT32 value = 0;
	INT32 i = 0;
#endif

	if (enable) {
#if CONSYS_PWR_ON_OFF_API_AVAILABLE
#if (COMMON_KERNEL_CLK_SUPPORT)
		iRet = pm_runtime_get_sync(&connsys_pdev->dev);
		if (iRet)
			WMT_PLAT_PR_INFO("pm_runtime_get_sync() fail(%d)\n", iRet);
		else
			WMT_PLAT_PR_INFO("pm_runtime_get_sync() CONSYS ok\n");

		iRet = device_init_wakeup(&connsys_pdev->dev, true);
		if (iRet)
			WMT_PLAT_PR_INFO("device_init_wakeup(true) fail.\n");
		else
			WMT_PLAT_PR_INFO("device_init_wakeup(true) CONSYS ok\n");
#else
		iRet = clk_prepare_enable(clk_scp_conn_main);
		if (iRet) {
			WMT_PLAT_PR_INFO("clk_prepare_enable(clk_scp_conn_main) fail(%d)\n", iRet);
			return iRet;
		}
		WMT_PLAT_PR_DBG("clk_prepare_enable(clk_scp_conn_main) ok\n");
#endif
#else
		/* turn on SPM clock gating enable PWRON_CONFG_EN 0x10006000 32'h0b160001 */
		CONSYS_REG_WRITE((conn_reg.spm_base + CONSYS_PWRON_CONFG_EN_OFFSET),
				 (CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWRON_CONFG_EN_OFFSET) &
				 0x0000FFFF) | CONSYS_PWRON_CONFG_EN_VALUE);

		/*write assert "conn_top_on" primary part power on, set "connsys_on_domain_pwr_on"=1 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) |
				 CONSYS_SPM_PWR_ON_BIT);
		/*read check "conn_top_on" primary part power status, check "connsys_on_domain_pwr_ack"=1 */
		value = CONSYS_PWR_ON_ACK_BIT &
			CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_OFFSET);
		while (value == 0)
			value = CONSYS_PWR_ON_ACK_BIT &
				CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_OFFSET);
		/*write assert "conn_top_on" secondary part power on, set "connsys_on_domain_pwr_on_s"=1 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) |
				 CONSYS_SPM_PWR_ON_S_BIT);
		/*read check "conn_top_on" secondary part power status, check "connsys_on_domain_pwr_ack_s"=1 */
		value = CONSYS_PWR_ON_ACK_S_BIT &
			CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_S_OFFSET);
		while (value == 0)
			value = CONSYS_PWR_ON_ACK_S_BIT &
				CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_S_OFFSET);
		/*write turn on AP-to-CONNSYS bus clock, set "conn_clk_dis"=0 (apply this for bus
		 *clock toggling)
		 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) &
				 ~CONSYS_CLK_CTRL_BIT);
		/*wait 1us*/
		udelay(1);
		/*de-assert "conn_top_on" isolation, set "connsys_iso_en"=0 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) &
				 ~CONSYS_SPM_PWR_ISO_S_BIT);
		/*read check "conn_top_off" primary part power status, check
		 *"connsys_off_domain_pwr_ack"=1
		 */
		value = CONSYS_TOP2_PWR_ON_ACK_BIT &
			CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_OFFSET);
		while (value == 0)
			value = CONSYS_TOP2_PWR_ON_ACK_BIT &
				CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_OFFSET);
		/*read check "conn_top_off" secondary part power status, check
		 *"connsys_off_domain_pwr_ack_s"=1
		 */
		value = CONSYS_TOP2_PWR_ON_ACK_S_BIT &
			CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_S_OFFSET);
		while (value == 0)
			value = CONSYS_TOP2_PWR_ON_ACK_S_BIT &
				CONSYS_REG_READ(conn_reg.spm_base + CONSYS_PWR_CONN_ACK_S_OFFSET);

		/*de-assert CONNSYS S/W reset, set "ap_sw_rst_b"=1 */
		CONSYS_REG_WRITE(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET,
				(CONSYS_REG_READ(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET) &
				 ~CONSYS_SW_RST_BIT) | CONSYS_CPU_SW_RST_CTRL_KEY);

		/*Turn off AHB bus sleep protect (AP2CONN AHB Bus protect) (apply this for INFRA AHB
		 *bus accessing when CONNSYS had been turned on)
		 */
		/*disable AHBAXI BUS protect 10001220 [13][14] */
		CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_EN_OFFSET,
				 CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_EN_OFFSET) &
				 ~CONSYS_PROT_MASK);
		value = ~CONSYS_PROT_MASK &
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_STA_OFFSET);
		i = 0;
		while (value == 0 || i > 10) {
			value = ~CONSYS_PROT_MASK &
				CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_STA_OFFSET);
			i++;
		}
		/*Disable AXI TX bus sleep protect (CONN2AP AXI Tx Bus protect) (disable sleep
		 * protection when CONNSYS had been turned on)
		 * Note : Should turn off AXI Tx sleep protection after AXI Rx sleep protection has
		 * been turn off.
		 */
		CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_EN_OFFSET,
				 CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_EN_OFFSET) &
				 ~CONSYS_TX_PROT_MASK);
		value = ~CONSYS_TX_PROT_MASK &
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_STA_OFFSET);
		i = 0;
		while (value == 0 || i > 10) {
			value = ~CONSYS_TX_PROT_MASK &
				CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_STA_OFFSET);
			i++;
		}
		/*SPM apsrc/ddr_en hardware mask disable & wait cycle setting*/
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_SPM_APSRC_OFFSET, CONSYS_SPM_APSRC_VALUE);
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_SPM_DDR_EN_OFFSET, CONSYS_SPM_DDR_EN_VALUE);
#endif /* CONSYS_PWR_ON_OFF_API_AVAILABLE */
	} else {
#if CONSYS_PWR_ON_OFF_API_AVAILABLE
#if (COMMON_KERNEL_CLK_SUPPORT)
		iRet = device_init_wakeup(&connsys_pdev->dev, false);
		if (iRet)
			WMT_PLAT_PR_INFO("device_init_wakeup(false) fail.\n");
		else
			WMT_PLAT_PR_INFO("device_init_wakeup(false) CONSYS ok\n");

		iRet = pm_runtime_put_sync(&connsys_pdev->dev);
		if (iRet)
			WMT_PLAT_PR_INFO("pm_runtime_put_sync() fail.\n");
		else
			WMT_PLAT_PR_INFO("pm_runtime_put_sync() CONSYS ok\n");
#else
		clk_disable_unprepare(clk_scp_conn_main);
		WMT_PLAT_PR_DBG("clk_disable_unprepare(clk_scp_conn_main) calling\n");
#endif
#else
		CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_EN_OFFSET,
				 CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_EN_OFFSET) &
				 CONSYS_PROT_MASK);
		value = CONSYS_PROT_MASK &
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_STA_OFFSET);
		i = 0;
		while (value == CONSYS_PROT_MASK || i > 10) {
			value = CONSYS_PROT_MASK &
				CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AHBAXI_PROT_STA_OFFSET);
			i++;
		}
		CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_EN_OFFSET,
				 CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_EN_OFFSET) &
				 CONSYS_TX_PROT_MASK);
		value = CONSYS_TX_PROT_MASK &
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_STA_OFFSET);
		i = 0;
		while (value == CONSYS_TX_PROT_MASK || i > 10) {
			value = CONSYS_TX_PROT_MASK &
				CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AXI_TX_PROT_STA_OFFSET);
			i++;
		}

		/*release connsys ISO, conn_top1_iso_en=1  0x1000632C [1]  1'b1 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) |
				 CONSYS_SPM_PWR_ISO_S_BIT);
		/*de-assert CONNSYS S/W reset, set "ap_sw_rst_b"=1 */
		CONSYS_REG_WRITE(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET,
				(CONSYS_REG_READ(conn_reg.ap_rgu_base + CONSYS_CPU_SW_RST_OFFSET) &
				 CONSYS_SW_RST_BIT) | CONSYS_CPU_SW_RST_CTRL_KEY);
		/*write conn_clk_dis=1, disable connsys clock  0x1000632C [4]  1'b1 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) |
				 CONSYS_CLK_CTRL_BIT);
		/*wait 1us      */
		udelay(1);
		/*write conn_top1_pwr_on=0, power off conn_top1 0x1000632C [3:2] 2'b00 */
		CONSYS_REG_WRITE(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET,
				 CONSYS_REG_READ(conn_reg.spm_base + CONSYS_TOP1_PWR_CTRL_OFFSET) &
				 ~(CONSYS_SPM_PWR_ON_BIT | CONSYS_SPM_PWR_ON_S_BIT));
#endif /* CONSYS_PWR_ON_OFF_API_AVAILABLE */
	}

#if CONSYS_PWR_ON_OFF_API_AVAILABLE
	return iRet;
#else
	return 0;
#endif
}

static INT32 consys_ahb_clock_ctrl(MTK_WCN_BOOL enable)
{
	return 0;
}

static INT32 polling_consys_chipid(VOID)
{
	INT32 retry = 10;
	UINT32 consys_ver_id = 0;
	UINT32 consys_hw_ver = 0;
	UINT32 consys_fw_ver = 0;
	UINT8 *consys_reg_base = NULL;
	UINT32 value = 0;

	/*12.poll CONNSYS CHIP ID until chipid is returned  0x18070008 */
	while (retry-- > 0) {
		consys_ver_id = CONSYS_REG_READ(conn_reg.mcu_top_misc_off_base + CONSYS_IP_VER_OFFSET);
		if (consys_ver_id == CONSYS_IP_VER_ID) {
			WMT_PLAT_PR_INFO("retry(%d)consys version id(0x%08x)\n", retry, consys_ver_id);
			consys_hw_ver = CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HW_ID_OFFSET);
			WMT_PLAT_PR_INFO("consys HW version id(0x%x)\n", consys_hw_ver & 0xFFFF);
			consys_fw_ver = CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_FW_ID_OFFSET);
			WMT_PLAT_PR_INFO("consys FW version id(0x%x)\n", consys_fw_ver & 0xFFFF);

			if (consys_dl_rom_patch(consys_ver_id, consys_fw_ver) == 0)
				break;
		}
		WMT_PLAT_PR_ERR("Read CONSYS version id(0x%08x)", consys_ver_id);
		msleep(20);
	}

	if (retry <= 0)
		return -1;

	consys_ver_id = CONSYS_REG_READ(conn_reg.mcu_top_misc_off_base + CONSYS_CONF_ID_OFFSET);
	WMT_PLAT_PR_INFO("consys configuration id(0x%x)\n", consys_ver_id & 0xF);
	consys_ver_id = CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HW_ID_OFFSET);
	WMT_PLAT_PR_INFO("consys HW version id(0x%x)\n", consys_ver_id & 0xFFFF);
	consys_ver_id = CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_FW_ID_OFFSET);
	WMT_PLAT_PR_INFO("consys FW version id(0x%x)\n", consys_ver_id & 0xFFFF);

	if (wmt_plat_soc_co_clock_flag_get()) {
		consys_reg_base = ioremap(CONSYS_COCLOCK_STABLE_TIME_BASE, 0x100);
		if (consys_reg_base) {
			/**
			 * 1. set CR "vcore ready stable time" "XO initial stable time" and
			 *    "XO bg stable time" to optimize the wakeup time after sleep
			 * 2. clear "source clock enable ack to XO state" mask
			 */
			value = CONSYS_REG_READ(consys_reg_base);
			value = (value & CONSYS_COCLOCK_STABLE_TIME_MASK) | CONSYS_COCLOCK_STABLE_TIME;
			CONSYS_REG_WRITE(consys_reg_base, value);

			value = CONSYS_REG_READ(consys_reg_base + CONSYS_COCLOCK_ACK_ENABLE_OFFSET);
			value = (value & 0xffff00fe) | 0x600;
			CONSYS_REG_WRITE(consys_reg_base + CONSYS_COCLOCK_ACK_ENABLE_OFFSET, value);
			iounmap(consys_reg_base);
		} else
			WMT_PLAT_PR_ERR("connsys co_clock stable time base(0x%x) ioremap fail!\n",
					  CONSYS_COCLOCK_STABLE_TIME_BASE);
	}

	/* EMI control CR setting(hw mode) */
	CONSYS_CLR_BIT(conn_reg.mcu_base + CONSYS_SW_IRQ_OFFSET, CONSYS_EMI_CTRL_VALUE);

	/* toppose_restore_done rollabck */
	CONSYS_REG_WRITE(conn_reg.mcu_top_misc_on_base + CONSYS_TOPPOSE_RESTORE_OFFSET,
			(CONSYS_REG_READ(conn_reg.mcu_top_misc_on_base + CONSYS_TOPPOSE_RESTORE_OFFSET) &
				CONSYS_TOPPOSE_RESTORE_MASK) | CONSYS_TOPPOSE_RESTORE_VALUE);

	return 0;
}

static VOID consys_acr_reg_setting(VOID)
{
}

static VOID consys_afe_reg_setting(VOID)
{
#if CONSYS_AFE_REG_SETTING
	UINT8 *consys_afe_reg_base = NULL;

	/* update WPLL setting for WPLL issue@LT */
	consys_afe_reg_base = ioremap(CONSYS_AFE_REG_BASE, 0x100);
	if (consys_afe_reg_base) {

		UINT32 value = CONSYS_REG_READ(consys_afe_reg_base + CONSYS_AFE_RG_WBG_PLL_03_OFFSET);

		value = (value & 0xffe7ffff) | 0x80000;
		CONSYS_REG_WRITE(consys_afe_reg_base + CONSYS_AFE_RG_WBG_PLL_03_OFFSET, value);
		iounmap(consys_afe_reg_base);
	} else
		WMT_PLAT_PR_ERR("AFE base(0x%x) ioremap fail!\n", CONSYS_AFE_REG_BASE);
#endif
}

static INT32 consys_hw_vcn18_ctrl(MTK_WCN_BOOL enable)
{
#if CONSYS_PMIC_CTRL_ENABLE
	if (enable) {
		/*need PMIC driver provide new API protocol */
		/*1.AP power on VCN_1V8 LDO (with PMIC_WRAP API) VCN_1V8  */
		/*set vcn18 SW mode*/
#if (COMMON_KERNEL_PMIC_SUPPORT)
		if (g_regmap_mt6358)
			regmap_write(g_regmap_mt6358, PMIC_RG_LDO_VCN18_OP_EN_SET_ADDR, 1);
		else
			WMT_PLAT_PR_INFO("failed to get regmap\n");
#else
		KERNEL_upmu_set_reg_value(MT6358_LDO_VCN18_OP_EN, 0x1);
#endif
		if (reg_VCN18) {
			regulator_set_voltage(reg_VCN18, 1800000, 1800000);
			if (regulator_enable(reg_VCN18))
				WMT_PLAT_PR_ERR("enable VCN18 fail\n");
			else
				WMT_PLAT_PR_DBG("enable VCN18 ok\n");
		}

		if (!(wmt_lib_get_ext_ldo())) {
#if (COMMON_KERNEL_PMIC_SUPPORT)
		if (g_regmap_mt6358)
			regmap_write(g_regmap_mt6358, PMIC_RG_LDO_VCN33_OP_EN_SET_ADDR, 1);
		else
			WMT_PLAT_PR_INFO("failed to get regmap\n");

#else
			KERNEL_upmu_set_reg_value(MT6358_LDO_VCN33_OP_EN, 0x1);
#endif
			if (reg_VCN33_BT) {
				regulator_set_voltage(reg_VCN33_BT, 3300000, 3300000);
				if (regulator_enable(reg_VCN33_BT))
					WMT_PLAT_PR_ERR("WMT do BT PMIC on fail!\n");
			}
		} else
			WMT_PLAT_PR_INFO("VCN33 uses external LDO!\n");
	} else {
		if (!(wmt_lib_get_ext_ldo())) {
			if (reg_VCN33_BT)
				regulator_disable(reg_VCN33_BT);
		}

		/*AP power off MT6358 VCN_1V8 LDO */
		if (reg_VCN18) {
			if (regulator_disable(reg_VCN18))
				WMT_PLAT_PR_ERR("disable VCN_1V8 fail!\n");
			else
				WMT_PLAT_PR_DBG("disable VCN_1V8 ok\n");
		}
	}
#endif
	return 0;
}

static VOID consys_vcn28_hw_mode_ctrl(UINT32 enable)
{
#if CONSYS_PMIC_CTRL_ENABLE
#if (COMMON_KERNEL_PMIC_SUPPORT)
	int val = enable ? 1 : 0;

	if (!g_regmap_mt6358) {
		WMT_PLAT_PR_INFO("failed to get g_regmap_mt6358\n");
		return;
	}

	regmap_update_bits(
		g_regmap_mt6358,
		PMIC_RG_LDO_VCN28_HW0_OP_EN_ADDR,
		PMIC_RG_LDO_VCN28_HW0_OP_EN_MASK << PMIC_RG_LDO_VCN28_HW0_OP_EN_SHIFT,
		val << PMIC_RG_LDO_VCN28_HW0_OP_EN_SHIFT);
	regmap_update_bits(
		g_regmap_mt6358,
		PMIC_RG_LDO_VCN28_HW0_OP_CFG_ADDR,
		PMIC_RG_LDO_VCN28_HW0_OP_CFG_MASK << PMIC_RG_LDO_VCN28_HW0_OP_CFG_SHIFT,
		0 << PMIC_RG_LDO_VCN28_HW0_OP_CFG_SHIFT);
#else
	if (enable) {
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN28_HW0_OP_EN, 1);
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN28_HW0_OP_CFG, 0);
	} else {
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN28_HW0_OP_EN, 0);
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN28_HW0_OP_CFG, 0);
	}
#endif
#endif
}

static INT32 consys_hw_vcn28_ctrl(UINT32 enable)
{
#if CONSYS_PMIC_CTRL_ENABLE
	if (enable) {
		/*in co-clock mode,need to turn on vcn28 when fm on */
		if (reg_VCN28) {
			regulator_set_voltage(reg_VCN28, 2800000, 2800000);
			if (regulator_enable(reg_VCN28))
				WMT_PLAT_PR_ERR("WMT do VCN28 PMIC on fail!\n");
		}
		WMT_PLAT_PR_INFO("turn on vcn28 for fm/gps usage in co-clock mode\n");
	} else {
		/*in co-clock mode,need to turn off vcn28 when fm off */
		if (reg_VCN28)
			regulator_disable(reg_VCN28);
		WMT_PLAT_PR_INFO("turn off vcn28 for fm/gps usage in co-clock mode\n");
	}
#endif
	return 0;
}

static INT32 consys_hw_bt_vcn33_ctrl(UINT32 enable)
{
#if 0
#if CONSYS_BT_WIFI_SHARE_V33
	/* spin_lock_irqsave(&gBtWifiV33.lock,gBtWifiV33.flags); */
	if (enable) {
		if (gBtWifiV33.counter == 1) {
			gBtWifiV33.counter++;
			WMT_PLAT_PR_DBG("V33 has been enabled,counter(%d)\n", gBtWifiV33.counter);
		} else if (gBtWifiV33.counter == 2) {
			WMT_PLAT_PR_DBG("V33 has been enabled,counter(%d)\n", gBtWifiV33.counter);
		} else {
#if CONSYS_PMIC_CTRL_ENABLE
			/*do BT PMIC on,depenency PMIC API ready */
			/*switch BT PALDO control from SW mode to HW mode:0x416[5]-->0x1 */
			/* VOL_DEFAULT, VOL_3300, VOL_3400, VOL_3500, VOL_3600 */
			hwPowerOn(MT6351_POWER_LDO_VCN33_BT, VOL_3300 * 1000, "wcn_drv");
			mt6351_upmu_set_rg_vcn33_on_ctrl(1);
#endif
			WMT_PLAT_PR_INFO("WMT do BT/WIFI v3.3 on\n");
			gBtWifiV33.counter++;
		}

	} else {
		if (gBtWifiV33.counter == 1) {
			/*do BT PMIC off */
			/*switch BT PALDO control from HW mode to SW mode:0x416[5]-->0x0 */
#if CONSYS_PMIC_CTRL_ENABLE
			mt6351_upmu_set_rg_vcn33_on_ctrl(0);
			hwPowerDown(MT6351_POWER_LDO_VCN33_BT, "wcn_drv");
#endif
			WMT_PLAT_PR_INFO("WMT do BT/WIFI v3.3 off\n");
			gBtWifiV33.counter--;
		} else if (gBtWifiV33.counter == 2) {
			gBtWifiV33.counter--;
			WMT_PLAT_PR_DBG("V33 no need disabled,counter(%d)\n", gBtWifiV33.counter);
		} else {
			WMT_PLAT_PR_DBG("V33 has been disabled,counter(%d)\n", gBtWifiV33.counter);
		}

	}
	/* spin_unlock_irqrestore(&gBtWifiV33.lock,gBtWifiV33.flags); */
#else
	if (enable) {
		/*do BT PMIC on,depenency PMIC API ready */
		/*switch BT PALDO control from SW mode to HW mode:0x416[5]-->0x1 */
#if CONSYS_PMIC_CTRL_ENABLE
		/* VOL_DEFAULT, VOL_3300, VOL_3400, VOL_3500, VOL_3600 */
		if (reg_VCN33_BT) {
			regulator_set_voltage(reg_VCN33_BT, 3300000, 3300000);
			if (regulator_enable(reg_VCN33_BT))
				WMT_PLAT_PR_ERR("WMT do BT PMIC on fail!\n");
		}

		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_EN, 1);
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_CFG, 0);
#endif
		WMT_PLAT_PR_DBG("WMT do BT PMIC on\n");
	} else {
		/*do BT PMIC off */
		/*switch BT PALDO control from HW mode to SW mode:0x416[5]-->0x0 */
#if CONSYS_PMIC_CTRL_ENABLE
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_EN, 0);
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_CFG, 0);
		if (reg_VCN33_BT)
			regulator_disable(reg_VCN33_BT);
#endif
		WMT_PLAT_PR_DBG("WMT do BT PMIC off\n");
	}
#endif

#endif
	return 0;
}

static INT32 consys_hw_wifi_vcn33_ctrl(UINT32 enable)
{
#if 0
#if CONSYS_BT_WIFI_SHARE_V33
	mtk_wcn_consys_hw_bt_paldo_ctrl(enable);
#else
	if (enable) {
		/*do WIFI PMIC on,depenency PMIC API ready */
		/*switch WIFI PALDO control from SW mode to HW mode:0x418[14]-->0x1 */
#if CONSYS_PMIC_CTRL_ENABLE
		if (reg_VCN33_WIFI) {
			regulator_set_voltage(reg_VCN33_WIFI, 3300000, 3300000);
			if (regulator_enable(reg_VCN33_WIFI))
				WMT_PLAT_PR_ERR("WMT do WIFI PMIC on fail!\n");
		}
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_EN, 1);
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_CFG, 0);
#endif
		WMT_PLAT_PR_DBG("WMT do WIFI PMIC on\n");
	} else {
		/*do WIFI PMIC off */
		/*switch WIFI PALDO control from HW mode to SW mode:0x418[14]-->0x0 */
#if CONSYS_PMIC_CTRL_ENABLE
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_EN, 0);
		KERNEL_pmic_set_register_value(PMIC_RG_LDO_VCN33_HW0_OP_CFG, 0);
		if (reg_VCN33_WIFI)
			regulator_disable(reg_VCN33_WIFI);
#endif
		WMT_PLAT_PR_DBG("WMT do WIFI PMIC off\n");
	}

#endif

#endif
	return 0;
}

static INT32 consys_emi_mpu_set_region_protection(VOID)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
	struct emimpu_region_t region;
	unsigned long long start = gConEmiPhyBase;
	unsigned long long end = gConEmiPhyBase + gConEmiSize - 1;

	mtk_emimpu_init_region(&region, REGION_CONN);
	mtk_emimpu_set_addr(&region, start, end);
	mtk_emimpu_set_apc(&region, DOMAIN_AP, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&region, DOMAIN_CONN, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_protection(&region);
	mtk_emimpu_free_region(&region);

	WMT_PLAT_PR_INFO("setting MPU for EMI share memory\n");
#endif
#else
#if IS_ENABLED(CONFIG_MTK_EMI)
	struct emi_region_info_t region_info;

	/*set MPU for EMI share Memory */
	WMT_PLAT_PR_INFO("setting MPU for EMI share memory\n");

	region_info.start = gConEmiPhyBase;
	region_info.end = gConEmiPhyBase + gConEmiSize - 1;
	region_info.region = 26;
	SET_ACCESS_PERMISSION(region_info.apc, LOCK,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			NO_PROTECTION, FORBIDDEN, NO_PROTECTION);
	emi_mpu_set_protection(&region_info);
#endif
#endif
	return 0;
}

static UINT32 consys_emi_set_remapping_reg(VOID)
{
	UINT32 addrPhy = 0;
	UINT32 value = 0;

	mtk_wcn_emi_addr_info.emi_ap_phy_addr = gConEmiPhyBase;
	mtk_wcn_emi_addr_info.emi_size = gConEmiSize;

	/*consys to ap emi remapping register:10001380, cal remapping address */
	addrPhy = (gConEmiPhyBase >> 24) & 0xFFF;
	value = CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET);
	WMT_PLAT_PR_INFO("before CONSYS_EMI_MAPPING read:0x%zx, value:0x%x\n",
			conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET, value);

	value = value & 0xFFFFF000;
	CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET, value | addrPhy);
	WMT_PLAT_PR_INFO("CONSYS_EMI_MAPPING dump in restore cb(0x%08x)\n",
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET));

	/*Perisys Configuration Registers remapping*/
	addrPhy = ((0x10003000 >> 24) & 0xFFF) << 16;
	value = CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_PERI_MAPPING_OFFSET);
	value = value & 0xF000FFFF;
	CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_EMI_PERI_MAPPING_OFFSET, value | addrPhy);

	WMT_PLAT_PR_INFO("CONSYS_EMI_MAPPING dump in restore cb(0x%08x)\n",
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_PERI_MAPPING_OFFSET));

	mtk_wcn_emi_addr_info.emi_ram_bt_buildtime_offset =
			CONSYS_EMI_RAM_BT_BUILDTIME_OFFSET;
	mtk_wcn_emi_addr_info.emi_ram_wifi_buildtime_offset =
			CONSYS_EMI_RAM_WIFI_BUILDTIME_OFFSET;
	mtk_wcn_emi_addr_info.emi_ram_mcu_buildtime_offset =
			CONSYS_EMI_RAM_MCU_BUILDTIME_OFFSET;
	mtk_wcn_emi_addr_info.emi_patch_mcu_buildtime_offset =
			CONSYS_EMI_PATCH_MCU_BUILDTIME_OFFSET;

	return 0;
}

static INT32 bt_wifi_share_v33_spin_lock_init(VOID)
{
#if CONSYS_BT_WIFI_SHARE_V33
	gBtWifiV33.counter = 0;
	spin_lock_init(&gBtWifiV33.lock);
#endif
	return 0;
}

static INT32 consys_read_irq_info_from_dts(struct platform_device *pdev, PINT32 irq_num, PUINT32 irq_flag)
{
	struct device_node *node;

	node = pdev->dev.of_node;
	if (node) {
		*irq_num = irq_of_parse_and_map(node, 0);
		*irq_flag = irq_get_trigger_type(*irq_num);
		WMT_PLAT_PR_INFO("get irq id(%d) and irq trigger flag(%d) from DT\n", *irq_num,
				   *irq_flag);
	} else {
		WMT_PLAT_PR_ERR("[%s] can't find CONSYS compatible node\n", __func__);
		return -1;
	}

	return 0;
}

static INT32 consys_read_reg_from_dts(struct platform_device *pdev)
{
	INT32 iRet = -1;
	struct device_node *node = NULL;

	node = pdev->dev.of_node;
	if (node) {
		/* registers base address */
		conn_reg.mcu_base = (SIZE_T) of_iomap(node, MCU_BASE_INDEX);
		conn_reg.ap_rgu_base = (SIZE_T) of_iomap(node, TOP_RGU_BASE_INDEX);
		conn_reg.topckgen_base = (SIZE_T) of_iomap(node, INFRACFG_AO_BASE_INDEX);
		conn_reg.spm_base = (SIZE_T) of_iomap(node, SPM_BASE_INDEX);
		conn_reg.mcu_top_misc_off_base = (SIZE_T) of_iomap(node, MCU_TOP_MISC_OFF_BASE_INDEX);
		conn_reg.mcu_conn_hif_on_base = (SIZE_T) of_iomap(node, MCU_CONN_HIF_ON_BASE_INDEX);
		conn_reg.mcu_cfg_on_base = (SIZE_T) of_iomap(node, MCU_CFG_ON_BASE_INDEX);
		conn_reg.mcu_cirq_base = (SIZE_T) of_iomap(node, MCU_CIRQ_BASE_INDEX);
		conn_reg.mcu_top_misc_on_base = (SIZE_T) of_iomap(node, MCU_TOP_MISC_ON_BASE_INDEX);
		conn_reg.mcu_conn_hif_pdma_base = (SIZE_T) of_iomap(node, MCU_CONN_HIF_PDMA_BASE_INDEX);

		WMT_PLAT_PR_DBG("Get base mcu(0x%zx), rgu(0x%zx), topckgen(0x%zx), spm(0x%zx)\n",
				conn_reg.mcu_base, conn_reg.ap_rgu_base,
				conn_reg.topckgen_base, conn_reg.spm_base);
		WMT_PLAT_PR_DBG("Get base misc_off(0x%zx), hif_on(0x%zx), cfg_on(0x%zx), misc_on(0x%zx)\n",
				conn_reg.mcu_top_misc_off_base,
				conn_reg.mcu_conn_hif_on_base,
				conn_reg.mcu_cfg_on_base,
				conn_reg.mcu_top_misc_on_base);
		WMT_PLAT_PR_DBG("Get hif_pdma(0x%zx)\n",
				conn_reg.mcu_conn_hif_pdma_base);
	} else {
		WMT_PLAT_PR_ERR("[%s] can't find CONSYS compatible node\n", __func__);
		return iRet;
	}

	return 0;
}

static VOID force_trigger_assert_debug_pin(VOID)
{
	CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_AP2CONN_OSC_EN_OFFSET,
			CONSYS_REG_READ(conn_reg.topckgen_base +
				CONSYS_AP2CONN_OSC_EN_OFFSET) & ~CONSYS_AP2CONN_WAKEUP_BIT);
	WMT_PLAT_PR_INFO("enable:dump CONSYS_AP2CONN_OSC_EN_REG(0x%x)\n",
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AP2CONN_OSC_EN_OFFSET));
	usleep_range(64, 96);
	CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_AP2CONN_OSC_EN_OFFSET,
			CONSYS_REG_READ(conn_reg.topckgen_base +
				CONSYS_AP2CONN_OSC_EN_OFFSET) | CONSYS_AP2CONN_WAKEUP_BIT);
	WMT_PLAT_PR_INFO("disable:dump CONSYS_AP2CONN_OSC_EN_REG(0x%x)\n",
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_AP2CONN_OSC_EN_OFFSET));
}

static UINT32 consys_read_cpupcr(VOID)
{
	if (conn_reg.mcu_conn_hif_on_base == 0)
		return 0;

	return CONSYS_REG_READ(conn_reg.mcu_conn_hif_on_base + CONSYS_CPUPCR_OFFSET);
}

static INT32 consys_poll_cpupcr_dump(UINT32 times, UINT32 sleep_ms)
{
	UINT64 ts;
	ULONG nsec;
	INT32 str_len = 0, i;
	char str[DBG_LOG_STR_SIZE] = {""};
	unsigned int remain = DBG_LOG_STR_SIZE;
	char *p = NULL;

	p = str;
	for (i = 0; i < times; i++) {
		osal_get_local_time(&ts, &nsec);
		str_len = snprintf(p, remain, "%llu.%06lu/0x%08x;", ts, nsec,
								consys_read_cpupcr());
		if (str_len < 0) {
			WMT_PLAT_PR_WARN("%s snprintf fail", __func__);
			continue;
		}
		p += str_len;
		remain -= str_len;

		if (sleep_ms > 0)
			osal_sleep_ms(sleep_ms);
	}
	WMT_PLAT_PR_INFO("TIME/CPUPCR: %s", str);
	return 0;
}

static UINT32 consys_soc_chipid_get(VOID)
{
	return PLATFORM_SOC_CHIP;
}

static P_CONSYS_EMI_ADDR_INFO consys_soc_get_emi_phy_add(VOID)
{
	return &mtk_wcn_emi_addr_info;
}

static INT32 consys_dl_rom_patch(UINT32 ip_ver, UINT32 fw_ver)
{
	if (rom_patch_dl_flag) {
		if (mtk_wcn_soc_rom_patch_dwn(ip_ver, fw_ver) == 0)
			rom_patch_dl_flag = 1;
		else
			return -1;
	}

	return 0;
}

static VOID consys_set_dl_rom_patch_flag(INT32 flag)
{
	rom_patch_dl_flag = flag;
}

static INT32 consys_dedicated_log_path_init(struct platform_device *pdev)
{
	struct device_node *node;
	UINT32 irq_num;
	UINT32 irq_flag;
	INT32 iret = -1;
	struct connlog_irq_config irq_config;

	memset(&irq_config, 0, sizeof(struct connlog_irq_config));
	node = pdev->dev.of_node;
	if (node) {
		irq_num = irq_of_parse_and_map(node, 2);
		irq_flag = irq_get_trigger_type(irq_num);
		WMT_PLAT_PR_INFO("get conn2ap_sw_irq id(%d) and irq trigger flag(%d) from DT\n", irq_num,
				   irq_flag);
	} else {
		WMT_PLAT_PR_ERR("[%s] can't find CONSYS compatible node\n", __func__);
		return iret;
	}

	irq_config.irq_num = irq_num;
	irq_config.irq_flag = irq_flag;
	irq_config.irq_callback = NULL;

	connsys_dedicated_log_path_apsoc_init(gConEmiPhyBase, &connsys_fw_log_parameter, &irq_config);
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	fw_log_wmt_init();
#endif
	return 0;
}

static VOID consys_dedicated_log_path_deinit(VOID)
{
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	fw_log_wmt_deinit();
#endif
	connsys_dedicated_log_path_apsoc_deinit();
}

static INT32 consys_check_reg_readable(VOID)
{
	INT32 can_read = 0;
	UINT32 value = 0;

	if (conn_reg.mcu_cfg_on_base != 0 &&
	    conn_reg.mcu_top_misc_on_base != 0) {
		/*check connsys clock and sleep status*/
		CONSYS_REG_WRITE(conn_reg.mcu_conn_hif_on_base, CONSYS_CLOCK_CHECK_VALUE);
		udelay(1000);
		value = CONSYS_REG_READ(conn_reg.mcu_conn_hif_on_base);
		if ((value & CONSYS_HCLK_CHECK_BIT) &&
		    (value & CONSYS_OSCCLK_CHECK_BIT) &&
		    ((value & CONSYS_SLEEP_CHECK_BIT) == 0))
			can_read = 1;
	}

	if (!can_read)
		WMT_PLAT_PR_ERR("connsys clock check fail 0x18007000(0x%x)\n", value);

	return can_read;
}

static INT32 consys_emi_coredump_remapping(UINT8 __iomem **addr, UINT32 enable)
{
	if (enable) {
		*addr = ioremap(gConEmiPhyBase + CONSYS_EMI_COREDUMP_OFFSET, CONSYS_EMI_MEM_SIZE);
		if (*addr) {
			WMT_PLAT_PR_INFO("COREDUMP EMI mapping OK virtual(0x%p) physical(0x%x)\n",
					   *addr, (UINT32) gConEmiPhyBase + CONSYS_EMI_COREDUMP_OFFSET);
			memset_io(*addr, 0, CONSYS_EMI_MEM_SIZE);
		} else {
			WMT_PLAT_PR_ERR("EMI mapping fail\n");
			return -1;
		}
	} else {
		if (*addr) {
			iounmap(*addr);
			*addr = NULL;
		}
	}
	return 0;
}

static INT32 consys_reset_emi_coredump(UINT8 __iomem *addr)
{
	if (!addr) {
		WMT_PLAT_PR_ERR("get virtual address fail\n");
		return -1;
	}
	WMT_PLAT_PR_INFO("Reset EMI(0xF0068000 ~ 0xF0068400) and (0xF0070400 ~ 0xF0078400)\n");
	/* reset 0xF0068000 ~ 0xF0068400 (1K) */
	memset_io(addr, 0, 0x400);
	/* reset 0xF0070400 ~ 0xF0078400 (32K)  */
	memset_io(addr + CONSYS_EMI_PAGED_DUMP_OFFSET, 0, 0x8000);
	return 0;
}

static INT32 consys_is_connsys_reg(UINT32 addr)
{
	if (addr > 0x18000000 && addr < 0x180FFFFF) {
		if (addr >= 0x18007000 && addr <= 0x18007FFF)
			return 0;

		return 1;
	}

	return 0;
}

static INT32 consys_is_host_csr(SIZE_T addr)
{
	SIZE_T start_offset = 0x000;
	SIZE_T end_offset = 0xFFF;

	if (addr >= (CONN_HIF_ON_BASE_ADDR + start_offset) &&
		addr <= (CONN_HIF_ON_BASE_ADDR + end_offset))
		return 1;

	if (conn_reg.mcu_conn_hif_on_base != 0 &&
		addr >= (conn_reg.mcu_conn_hif_on_base + start_offset) &&
		addr <= (conn_reg.mcu_conn_hif_on_base + end_offset))
		return 1;

	return 0;
}

static VOID consys_ic_clock_fail_dump(VOID)
{
	INT8 *addr;
	char *buffer, *temp;
	INT32 size = 1024;

	/* make sure buffer size is big enough */
	buffer = osal_malloc(size);
	if (!buffer)
		return;

	temp = buffer;
	temp += sprintf(temp, "CONN_HIF_TOP_MISC=0x%08x CONN_HIF_BUSY_STATUS=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_TOP_MISC),
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_BUSY_STATUS));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_HIF_DBG_IDX, 0x3333);
	temp += sprintf(temp, "Write CONSYS_HIF_DBG_IDX to 0x3333\n");

	temp += sprintf(temp, "CONSYS_HIF_DBG_PROBE=0x%08x CONN_HIF_TOP_MISC=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_DBG_PROBE),
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_TOP_MISC));

	temp += sprintf(temp, "CONN_HIF_BUSY_STATUS=0x%08x CONN_HIF_PDMA_BUSY_STATUS=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_BUSY_STATUS),
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_PDMA_BUSY_STATUS));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_HIF_DBG_IDX, 0x2222);
	temp += sprintf(temp, "Write CONSYS_HIF_DBG_IDX to 0x2222\n");

	temp += sprintf(temp, "CONSYS_HIF_DBG_PROBE=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_DBG_PROBE));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_HIF_DBG_IDX, 0x3333);
	temp += sprintf(temp, "Write CONSYS_HIF_DBG_IDX to 0x3333\n");

	temp += sprintf(temp, "CONSYS_HIF_DBG_PROBE=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_DBG_PROBE));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_HIF_DBG_IDX, 0x4444);
	temp += sprintf(temp, "Write CONSYS_HIF_DBG_IDX to 0x4444\n");

	temp += sprintf(temp, "CONSYS_HIF_DBG_PROBE=0x%08x CONN_MCU_EMI_CONTROL=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_HIF_DBG_PROBE),
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_SW_IRQ_OFFSET));
	temp += sprintf(temp, "EMI_CONTROL_DBG_PROBE=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + EMI_CONTROL_DBG_PROBE));
	temp += sprintf(temp, "CONN_MCU_CLOCK_CONTROL=0x%08x CONN_MCU_BUS_CONTROL=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_CLOCK_CONTROL),
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_BUS_CONTROL));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_DEBUG_SELECT, 0x003e3d00);
	temp += sprintf(temp, "Write CONSYS_DEBUG_SELECT to 0x003e3d00\n");

	temp += sprintf(temp, "CONN_MCU_DEBUG_STATUS=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_DEBUG_STATUS));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_DEBUG_SELECT, 0x00403f00);
	temp += sprintf(temp, "Write CONSYS_DEBUG_SELECT to 0x00403f00\n");

	temp += sprintf(temp, "CONN_MCU_DEBUG_STATUS=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_DEBUG_STATUS));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_DEBUG_SELECT, 0x00424100);
	temp += sprintf(temp, "Write CONSYS_DEBUG_SELECT to 0x00424100\n");

	temp += sprintf(temp, "CONN_MCU_DEBUG_STATUS=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_DEBUG_STATUS));

	CONSYS_REG_WRITE(conn_reg.mcu_base + CONSYS_DEBUG_SELECT, 0x00444300);
	temp += sprintf(temp, "Write CONSYS_DEBUG_SELECT to 0x00444300\n");

	temp += sprintf(temp, "CONN_MCU_DEBUG_STATUS=0x%08x\n",
		CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_DEBUG_STATUS));

	addr = ioremap(0x10001B20, 0x100);
	if (addr) {
	/* 0x1020E804 */
		temp += sprintf(temp, "0x10001B20=0x%08x\n", CONSYS_REG_READ(addr));
		iounmap(addr);
	}
	WMT_PLAT_PR_ERR("%s length = %d", buffer, osal_strlen(buffer));
	osal_free(buffer);
}

/*
 * Before calling this function, should check consys state
 *  ex: consys power on already and reg_readable
 */

static INT32 consys_dump_osc_state(P_CONSYS_STATE state)
{
#if 0
	UINT8 __iomem *addr;
#endif

	CONSYS_REG_WRITE(CONN_CFG_ON_CONN_ON_HOST_MAILBOX_MCU_ADDR, 0x1);
	CONSYS_REG_WRITE(CONN_CFG_ON_CONN_ON_MON_CTL_ADDR, 0x80000001);
	CONSYS_REG_WRITE(CONN_CFG_ON_CONN_ON_DBGSEL_ADDR, 0x3);
	state->lp[0] = (UINT32)CONN_CFG_ON_CONN_ON_MON_FLAG_RECORD_MAPPING_AP_ADDR;
	state->lp[1] = CONSYS_REG_READ(CONN_CFG_ON_CONN_ON_MON_FLAG_RECORD_ADDR);
	WMT_PLAT_PR_INFO("0x%08x: 0x%x\n", state->lp[0], state->lp[1]);
	CONSYS_REG_WRITE(CONN_CFG_ON_CONN_ON_HOST_MAILBOX_MCU_ADDR, 0x0);

#if 0
	addr = ioremap(gConEmiPhyBase + 0x66500, sizeof(struct consys_sw_state));
	if (addr)
		memcpy_fromio(&state->sw_state, addr, sizeof(struct consys_sw_state));
	else
		WMT_PLAT_PR_WARN("ioremap fail\n");
	iounmap(addr);
#endif

	return MTK_WCN_BOOL_TRUE;
}

static VOID consys_set_pdma_axi_rready_force_high(UINT32 enable)
{
	if (enable)
		CONSYS_SET_BIT(conn_reg.mcu_conn_hif_pdma_base + CONSYS_HIF_PDMA_AXI_RREADY,
				CONSYS_PDMA_AXI_RREADY_MASK);
	else if ((CONSYS_REG_READ(conn_reg.mcu_conn_hif_pdma_base + CONSYS_HIF_PDMA_AXI_RREADY) &
		 CONSYS_PDMA_AXI_RREADY_MASK) != 0)
		CONSYS_CLR_BIT(conn_reg.mcu_conn_hif_pdma_base + CONSYS_HIF_PDMA_AXI_RREADY,
			       CONSYS_PDMA_AXI_RREADY_MASK);
}

static INT32 consys_calibration_backup_restore_support(VOID)
{
	return 1;
}

#ifdef WMT_DEVAPC_CALLBACK
static VOID consys_devapc_violation_cb(VOID)
{
	/**
	 * Don't use wmt_lib_trigger_assert() because it will invoke vmalloc and then cause KE since
	 * this callback is supposed to be invoked in DEVAPC exception hanlder.
	 */
	wmt_lib_trigger_assert_keyword_delay(WMTDRV_TYPE_WMT, 46, "DEVAPC Violation");
}

static VOID consyc_register_devapc_cb(VOID)
{
	register_devapc_vio_callback(&devapc_handle);
}
#endif

static INT32 consys_is_ant_swap_enable_by_hwid(INT32 pin_num)
{
	return !connectivity_export_gpio_get_tristate_input(pin_num);
}

#if (COMMON_KERNEL_CLK_SUPPORT)
static MTK_WCN_BOOL consys_need_store_pdev(VOID)
{
	return MTK_WCN_BOOL_TRUE;
}

static UINT32 consys_store_pdev(struct platform_device *pdev)
{
	connsys_pdev = pdev;
	return 0;
}
#endif

static UINT64 consys_get_options(VOID)
{
	UINT64 options = OPT_QUERY_ADIE |
			OPT_WIFI_LTE_COEX |
			OPT_BT_TSSI_FROM_WIFI_CONFIG_NEW_OPID |
			OPT_INIT_COEX_BEFORE_RF_CALIBRATION |
			OPT_COEX_CONFIG_ADJUST |
			OPT_COEX_CONFIG_ADJUST_NEW_FLAG |
			OPT_WIFI_LTE_COEX_TABLE_3 |
			OPT_COEX_EXT_ELNA_GAIN_P1_SUPPORT |
			OPT_NORMAL_PATCH_DWN_3 |
			OPT_PATCH_CHECKSUM;
	return options;
}

INT32 dump_conn_mcu_pc_log_wrapper(VOID)
{
	return dump_conn_mcu_pc_log_mt6768("");
}

static INT32 consys_common_dump(const char *trg_str)
{
	int ret = 0;

	ret += dump_conn_debug_dump_mt6768(trg_str);
	ret += dump_conn_mcu_debug_flag_mt6768(trg_str);
	ret += dump_conn_mcu_apb0_bus_mt6768(trg_str);
	ret += dump_conn_mcu_apb1_bus_mt6768(trg_str);
	ret += dump_conn_mcu_pc_log_mt6768(trg_str);
	ret += dump_conn_cfg_on_debug_signal_mt6768(trg_str);
	ret += dump_conn_cfg_on_register_mt6768(trg_str);
	ret += dump_conn_cmdbt_debug_signal_mt6768(trg_str);
	ret += dump_conn_emi_detect_mt6768(trg_str);
	ret += dump_conn_slp_protect_debug_mt6768(trg_str);
	ret += dump_conn_spm_r13_mt6768(trg_str);
	ret += dump_conn_bus_timeout_debug_mt6768(trg_str);
	ret += dump_conn_ILM_corrupt_issue_debug_mt6768(trg_str);

	return ret;
}

INT32 consys_cmd_tx_timeout_dump(VOID)
{
	return consys_common_dump("tx_timeout");
}

INT32 consys_cmd_rx_timeout_dump(VOID)
{
	return consys_common_dump("rx_timeout");
}

INT32 consys_coredump_timeout_dump(VOID)
{
	return consys_common_dump("coredump_timeout");
}

INT32 consys_assert_timeout_dump(VOID)
{
	return consys_common_dump("assert_timeout");
}

INT32 consys_before_chip_reset_dump(VOID)
{
	return consys_common_dump("before_chip_reset");
}
