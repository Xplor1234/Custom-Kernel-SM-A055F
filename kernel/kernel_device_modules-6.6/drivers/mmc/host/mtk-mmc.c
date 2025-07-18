// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/tracepoint.h>

#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>

#include "../core/card.h"
#include "../core/queue.h"
#include "../core/mmc_ops.h"
#include "../core/core.h"
#include "cqhci.h"
#include "mtk-mmc.h"
#include "mtk-mmc-dbg.h"
#include "rpmb-mtk.h"
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/mtk_blocktag.h>

#if IS_ENABLED(CONFIG_SEC_MMC_FEATURE)
#include "mmc-sec-feature.h"
#endif

/*+dunweichao.wt,2024.8.24,add proc file for sdcard slot detect*/
#include <linux/proc_fs.h>
#include <linux/gpio.h>
/*-dunweichao.wt,2024.8.24,add proc file for sdcard slot detect*/
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
#include "mtk-mmc-swcqhci.h"
#endif

#define PAD_DS_TUNE_DLY_SEL       (0x1 << 0)	/* RW */
/* EMMC50_PAD_DS_TUNE mask */
#define PAD_DS_DLY_SEL		(0x1 << 16)	/* RW */
#define PAD_DS_DLY1		(0x1f << 10)	/* RW */
#define PAD_DS_DLY3		(0x1f << 0)	/* RW */
#define PAD_DELAY_MAX	32 /* PAD delay cells */

#define MSDC_RESET_HW_TIMEOUT 100000

#define MSDC_GPIO_2 "msdc_gpio=2"

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq);

#define BOOTDEV_EMMC 1

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_DEBUG)
static void msdc_gpio_of_parse(struct msdc_host *host);
#endif

static int msdc_get_gpio_version(void)
{
	struct device_node *of_chosen = NULL;
	char *bootargs = NULL;
	int msdc_gpio = 1;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(of_chosen,
			"bootargs", NULL);

		if (bootargs && strstr(bootargs, MSDC_GPIO_2))
			msdc_gpio = 2;
		else
			msdc_gpio = 1;
		of_node_put(of_chosen);
	}
	pr_info("msdc_gpio %d\n", msdc_gpio);
	return msdc_gpio;
}

static const struct mtk_mmc_compatible mt8135_compat = {
	.clk_div_bits = 8,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE,
	.async_fifo = false,
	.data_tune = false,
	.busy_check = false,
	.stop_clk_set = {
		.enable = 0,
	},
	.enhance_rx = false,
	.support_64g = false,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt8173_compat = {
	.clk_div_bits = 8,
	.recheck_sdio_irq = true,
	.hs400_tune = true,
	.pad_tune_reg = MSDC_PAD_TUNE,
	.async_fifo = false,
	.data_tune = false,
	.busy_check = false,
	.stop_clk_set = {
		.enable = 0,
	},
	.enhance_rx = false,
	.support_64g = false,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt8183_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt8195_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt2701_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = false,
	.stop_clk_set = {
		.enable = 0,
	},
	.enhance_rx = false,
	.support_64g = false,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt2712_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt7622_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = false,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt8516_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt7620_compat = {
	.clk_div_bits = 8,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE,
	.async_fifo = false,
	.data_tune = false,
	.busy_check = false,
	.stop_clk_set = {
		.enable = 0,
	},
	.enhance_rx = false,
	.use_internal_cd = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6779_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6761_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.set_crypto_enable_in_sw = true,
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6768_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6877_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6833_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6781_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6853_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6765_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_V1,
	},
	.set_crypto_enable_in_sw = true,
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible common_v2_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = false,
	},
	.new_tx_ver = 0,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6985_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 1,
		.pop_cnt = 2,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = false,
	},
	.new_tx_ver = MSDC_NEW_TX_V1,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6886_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 1,
		.pop_cnt = 2,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = false,
	},
	.new_tx_ver = MSDC_NEW_TX_V1,
	.new_rx_ver = 0,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6897_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = false,
	},
	.new_tx_ver = MSDC_NEW_TX_V1,
	.new_rx_ver = MSDC_NEW_RX_V1,
	.infra_check = {
		.enable = false,
	},
};

static const struct mtk_mmc_compatible mt6989_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 3,
		.pop_cnt = 8,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = false,
	},
	.new_tx_ver = MSDC_NEW_TX_V1,
	.new_rx_ver = MSDC_NEW_RX_V1,
	.infra_check = {
		.enable = true,
		.infra_ack_bit = BIT(14),
		.infra_ack_paddr = 0x1c001104,
	},
};

static const struct mtk_mmc_compatible mt6991_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 1,
		.pop_cnt = 2,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = true,
		.set_type = MSDC_CLK_SET_MULTI,
	},
	.new_tx_ver = MSDC_NEW_TX_V2,
	.new_rx_ver = MSDC_NEW_RX_V1,
	.infra_check = {
		.enable = true,
		.infra_ack_bit = BIT(14),
		.infra_ack_paddr = 0x1c004104,
	},
};

static const struct mtk_mmc_compatible mt6899_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_set = {
		.enable = 1,
		.stop_cnt = 1,
		.pop_cnt = 2,
	},
	.enhance_rx = true,
	.support_64g = true,
	.clock_set = {
		.need_gate_cg = false,
	},
	.new_tx_ver = MSDC_NEW_TX_V2,
	.new_rx_ver = MSDC_NEW_RX_V1,
	.infra_check = {
		.enable = true,
		.infra_ack_bit = BIT(14),
		.infra_ack_paddr = 0x1c001104,
	},
};

static const struct of_device_id msdc_of_ids[] = {
	{ .compatible = "mediatek,mt8135-mmc", .data = &mt8135_compat},
	{ .compatible = "mediatek,mt8173-mmc", .data = &mt8173_compat},
	{ .compatible = "mediatek,mt8183-mmc", .data = &mt8183_compat},
	{ .compatible = "mediatek,mt8195-mmc", .data = &mt8195_compat},
	{ .compatible = "mediatek,mt2701-mmc", .data = &mt2701_compat},
	{ .compatible = "mediatek,mt2712-mmc", .data = &mt2712_compat},
	{ .compatible = "mediatek,mt7622-mmc", .data = &mt7622_compat},
	{ .compatible = "mediatek,mt8516-mmc", .data = &mt8516_compat},
	{ .compatible = "mediatek,mt7620-mmc", .data = &mt7620_compat},
	{ .compatible = "mediatek,mt6779-mmc", .data = &mt6779_compat},
	{ .compatible = "mediatek,mt6761-mmc", .data = &mt6761_compat},
	{ .compatible = "mediatek,mt6768-mmc", .data = &mt6768_compat},
	{ .compatible = "mediatek,mt6877-mmc", .data = &mt6877_compat},
	{ .compatible = "mediatek,common-mmc-v2", .data = &common_v2_compat},
	{ .compatible = "mediatek,mt6985-mmc", .data = &mt6985_compat},
	{ .compatible = "mediatek,mt6886-mmc", .data = &mt6886_compat},
	{ .compatible = "mediatek,mt6897-mmc", .data = &mt6897_compat},
	{ .compatible = "mediatek,mt6989-mmc", .data = &mt6989_compat},
	{ .compatible = "mediatek,mt6991-mmc", .data = &mt6991_compat},
	{ .compatible = "mediatek,mt6833-mmc", .data = &mt6833_compat},
	{ .compatible = "mediatek,mt6853-mmc", .data = &mt6853_compat},
	{ .compatible = "mediatek,mt6781-mmc", .data = &mt6781_compat},
	{ .compatible = "mediatek,mt6765-mmc", .data = &mt6765_compat},
	{ .compatible = "mediatek,mt6899-mmc", .data = &mt6899_compat},
	{}
};
MODULE_DEVICE_TABLE(of, msdc_of_ids);

static const u32 msdc_ints_err = MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO |
									MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
static bool ra_fixed;
static int  rw_times;

static void sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val |= bs;
	writel(val, reg);
}

static void sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val &= ~bs;
	writel(val, reg);
}

static void sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = readl(reg);

	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	writel(tv, reg);
}

static void sdr_get_field(void __iomem *reg, u32 field, u32 *val)
{
	unsigned int tv = readl(reg);

	*val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

static void msdc_reset_hw(struct msdc_host *host)
{
	u32 val;
	int ret;

	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	ret = readl_poll_timeout_atomic(host->base + MSDC_DMA_CTRL, val,
			!(val & MSDC_DMA_CTRL_STOP), 1, MSDC_RESET_HW_TIMEOUT);
	if (ret) {
		dev_info(host->dev, "[%s %d]timeout dump\n", __func__, __LINE__);
		msdc_dump_info(NULL, 0, NULL, host);
	}

	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_RST);
	ret = readl_poll_timeout_atomic(host->base + MSDC_CFG, val,
			!(val & MSDC_CFG_RST), 1, MSDC_RESET_HW_TIMEOUT);
	if (ret) {
		dev_info(host->dev, "[%s %d]timeout dump\n", __func__, __LINE__);
		msdc_dump_info(NULL, 0, NULL, host);
	}

	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	ret = readl_poll_timeout_atomic(host->base + MSDC_DMA_CTRL, val,
			!(val & MSDC_DMA_CTRL_STOP), 1, MSDC_RESET_HW_TIMEOUT);
	if (ret) {
		dev_info(host->dev, "[%s %d]timeout dump\n", __func__, __LINE__);
		msdc_dump_info(NULL, 0, NULL, host);
	}

	sdr_set_bits(host->base + MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	ret = readl_poll_timeout_atomic(host->base + MSDC_FIFOCS, val,
			!(val & MSDC_FIFOCS_CLR), 1, MSDC_RESET_HW_TIMEOUT);
	if (ret) {
		dev_info(host->dev, "[%s %d]timeout dump\n", __func__, __LINE__);
		msdc_dump_info(NULL, 0, NULL, host);
	}

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

extern void gpio_dump_regs_range(int start, int end);

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd);
static void __msdc_enable_sdio_irq(struct msdc_host *host, int enb);
static int msdc_install_tracepoints(struct msdc_host *host);
static void msdc_uninstall_tracepoints(void);

static const u32 cmd_ints_mask = MSDC_INTEN_CMDRDY | MSDC_INTEN_RSPCRCERR |
			MSDC_INTEN_CMDTMO | MSDC_INTEN_ACMDRDY |
			MSDC_INTEN_ACMDCRCERR | MSDC_INTEN_ACMDTMO;
static const u32 data_ints_mask = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO |
			MSDC_INTEN_DATCRCERR | MSDC_INTEN_DMA_BDCSERR |
			MSDC_INTEN_DMA_GPDCSERR | MSDC_INTEN_DMA_PROTECT;

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xff - (u8) sum;
}

static inline void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
	unsigned int j, dma_len;
	dma_addr_t dma_address;
	u32 dma_ctrl;
	struct scatterlist *sg;
	struct mt_gpdma_desc *gpd;
	struct mt_bdma_desc *bd;

	sg = data->sg;

	gpd = dma->gpd;
	bd = dma->bd;

	/* modify gpd */
	gpd->gpd_info |= GPDMA_DESC_HWO;
	gpd->gpd_info |= GPDMA_DESC_BDP;
	/* need to clear first. use these bits to calc checksum */
	gpd->gpd_info &= ~GPDMA_DESC_CHECKSUM;
	gpd->gpd_info |= msdc_dma_calcs((u8 *) gpd, 16) << 8;

	/* modify bd */
	for_each_sg(data->sg, sg, data->sg_count, j) {
		dma_address = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);

		/* init bd */
		bd[j].bd_info &= ~BDMA_DESC_BLKPAD;
		bd[j].bd_info &= ~BDMA_DESC_DWPAD;
		bd[j].ptr = lower_32_bits(dma_address);
		if (host->dev_comp->support_64g) {
			bd[j].bd_info &= ~BDMA_DESC_PTR_H4;
			bd[j].bd_info |= (upper_32_bits(dma_address) & 0xf)
					 << 28;
		}

		if (host->dev_comp->support_64g) {
			bd[j].bd_data_len &= ~BDMA_DESC_BUFLEN_EXT;
			bd[j].bd_data_len |= (dma_len & BDMA_DESC_BUFLEN_EXT);
		} else {
			bd[j].bd_data_len &= ~BDMA_DESC_BUFLEN;
			bd[j].bd_data_len |= (dma_len & BDMA_DESC_BUFLEN);
		}

		if (j == data->sg_count - 1) /* the last bd */
			bd[j].bd_info |= BDMA_DESC_EOL;
		else
			bd[j].bd_info &= ~BDMA_DESC_EOL;

		/* checksume need to clear first */
		bd[j].bd_info &= ~BDMA_DESC_CHECKSUM;
		bd[j].bd_info |= msdc_dma_calcs((u8 *)(&bd[j]), 16) << 8;
	}

	sdr_set_field(host->base + MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, 1);
	dma_ctrl = readl_relaxed(host->base + MSDC_DMA_CTRL);
	dma_ctrl &= ~(MSDC_DMA_CTRL_BRUSTSZ | MSDC_DMA_CTRL_MODE);
	dma_ctrl |= (MSDC_BURST_64B << 12 | 1 << 8);
	writel_relaxed(dma_ctrl, host->base + MSDC_DMA_CTRL);
	if (host->dev_comp->support_64g)
		sdr_set_field(host->base + DMA_SA_H4BIT, DMA_ADDR_HIGH_4BIT,
			      upper_32_bits(dma->gpd_addr) & 0xf);
	writel(lower_32_bits(dma->gpd_addr), host->base + MSDC_DMA_SA);
}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_data *data)
{
	if (!(data->host_cookie & MSDC_PREPARE_FLAG)) {
		data->host_cookie |= MSDC_PREPARE_FLAG;
		data->sg_count = dma_map_sg(host->dev, data->sg, data->sg_len,
					    mmc_get_dma_dir(data));
	}
}

static void msdc_unprepare_data(struct msdc_host *host, struct mmc_data *data)
{
	if (data->host_cookie & MSDC_ASYNC_FLAG)
		return;

	if (data->host_cookie & MSDC_PREPARE_FLAG) {
		dma_unmap_sg(host->dev, data->sg, data->sg_len,
			     mmc_get_dma_dir(data));
		data->host_cookie &= ~MSDC_PREPARE_FLAG;
	}
}

static u64 msdc_timeout_cal(struct msdc_host *host, u64 ns, u64 clks)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u64 timeout, clk_ns;
	u32 mode = 0;

	if (mmc->actual_clock == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000ULL;
		do_div(clk_ns, mmc->actual_clock);
		timeout = ns + clk_ns - 1;
		do_div(timeout, clk_ns);
		timeout += clks;
		/* in 1048576 sclk cycle unit */
		timeout = DIV_ROUND_UP(timeout, (0x1 << 20));
		if (host->dev_comp->clk_div_bits == 8)
			sdr_get_field(host->base + MSDC_CFG,
				      MSDC_CFG_CKMOD, &mode);
		else
			sdr_get_field(host->base + MSDC_CFG,
				      MSDC_CFG_CKMOD_EXTRA, &mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
	}
	return timeout;
}

/* clock control primitives */
static void msdc_set_timeout(struct msdc_host *host, u64 ns, u64 clks)
{
	u64 timeout;

	host->timeout_ns = ns;
	host->timeout_clks = clks;

	timeout = msdc_timeout_cal(host, ns, clks);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC,
		      (u32)(timeout > 255 ? 255 : timeout));
}

static void msdc_set_busy_timeout(struct msdc_host *host, u64 ns, u64 clks)
{
	u64 timeout;

	timeout = msdc_timeout_cal(host, ns, clks);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_WRDTOC,
		      (u32)(timeout > 8191 ? 8191 : timeout));
}

static void msdc_gate_clock(struct msdc_host *host)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	clk_bulk_disable_unprepare(MSDC_NR_CLOCKS, host->bulk_clks);
	clk_disable_unprepare(host->src_clk_cg);
	clk_disable_unprepare(host->crypto_cg);
	clk_disable_unprepare(host->src_clk);
	clk_disable_unprepare(host->macro_clk);
	clk_disable_unprepare(host->bus_clk);
	clk_disable_unprepare(host->h_clk);
	clk_disable_unprepare(host->crypto_clk);
#endif
}

static int msdc_ungate_clock(struct msdc_host *host)
{
	u32 val;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int ret;

	clk_prepare_enable(host->crypto_clk);
	clk_prepare_enable(host->h_clk);
	clk_prepare_enable(host->bus_clk);
	clk_prepare_enable(host->macro_clk);
	clk_prepare_enable(host->src_clk);
	clk_prepare_enable(host->crypto_cg);
	clk_prepare_enable(host->src_clk_cg);
	ret = clk_bulk_prepare_enable(MSDC_NR_CLOCKS, host->bulk_clks);
	if (ret) {
		dev_info(host->dev, "Cannot enable pclk/axi/ahb clock gates\n");
		return ret;
	}
#endif

	return readl_poll_timeout(host->base + MSDC_CFG, val,
				  (val & MSDC_CFG_CKSTB), 1, 20000);
}

static void msdc_new_tx_rx_setting_v2(struct msdc_host *host,
	unsigned char timing)
{
	switch (timing) {
	case MMC_TIMING_LEGACY:
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_UHS_SDR12:
	case MMC_TIMING_UHS_SDR25:
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_DSCLK_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_LATCH_MUX_SEL);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     LOOP_EN_SEL_CLK);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_HS400_CMD_LOOP_MUX_SEL);
		break;
	case MMC_TIMING_UHS_SDR50:
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_MMC_HS200:
	case MMC_TIMING_MMC_HS400:
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_DSCLK_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_LATCH_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     LOOP_EN_SEL_CLK);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_HS400_CMD_LOOP_MUX_SEL);
		break;
	default:
		break;
	}
}

static void msdc_new_tx_rx_setting_v1(struct msdc_host *host,
	unsigned char timing)
{
	switch (timing) {
	case MMC_TIMING_LEGACY:
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_UHS_SDR12:
	case MMC_TIMING_UHS_SDR25:
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_DSCLK_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_LATCH_MUX_SEL);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     LOOP_EN_SEL_CLK);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_HS400_CMD_LOOP_MUX_SEL);
		break;
	case MMC_TIMING_UHS_SDR50:
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_MMC_HS200:
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_DSCLK_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_LATCH_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     LOOP_EN_SEL_CLK);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_HS400_CMD_LOOP_MUX_SEL);
		break;
	case MMC_TIMING_MMC_HS400:
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_DSCLK_MUX_SEL);
		sdr_clr_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_LOOP_LATCH_MUX_SEL);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     LOOP_EN_SEL_CLK);
		sdr_set_bits(host->top_base + LOOP_TEST_CONTROL,
			     TEST_HS400_CMD_LOOP_MUX_SEL);
		break;
	default:
		break;
	}
}

static void msdc_new_tx_rx_setting(struct msdc_host *host, unsigned char timing)
{
	switch (host->dev_comp->new_tx_ver) {
	case MSDC_NEW_TX_V1:
		msdc_new_tx_rx_setting_v1(host, timing);
		break;
	case MSDC_NEW_TX_V2:
		msdc_new_tx_rx_setting_v2(host, timing);
		break;
	default:
		break;
	}
}

static int msdc_prepare_set_mclk(struct msdc_host *host, bool gate)
{
	int ret = 0;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	if (host->dev_comp->clock_set.need_gate_cg == false)
		return 0;

	if (host->dev_comp->clock_set.need_gate_cg &&
		!support_clk_set(host->dev_comp->clock_set.set_type))
		return 1;

	switch (host->dev_comp->clock_set.set_type) {
	case MSDC_CLK_SET_V1:
V1:
		if (gate) {
			/*
			 * As src_clk/HCLK use the same bit to gate/ungate,
			 * So if want to only gate src_clk, need gate its parent(mux).
			 */
			if (host->src_clk_cg)
				clk_disable_unprepare(host->src_clk_cg);
			else
				clk_disable_unprepare(clk_get_parent(host->src_clk));
		} else {
			if (host->src_clk_cg)
				ret = clk_prepare_enable(host->src_clk_cg);
			else
				ret = clk_prepare_enable(clk_get_parent(host->src_clk));
		}
		break;
	case MSDC_CLK_SET_V2:
		/* Due to bus protect, gate/ungate mux replace cg */
		if (gate) {
			clk_disable_unprepare(host->src_clk);
			if (host->src_clk_cg)
				clk_disable_unprepare(host->src_clk);
		} else {
			ret = clk_prepare_enable(host->src_clk);
			if (host->src_clk_cg && ret == 0)
				ret = clk_prepare_enable(host->src_clk);
		}
		break;
	case MSDC_CLK_SET_MULTI:
		if (host->sw_ver == CHIP_VER_E1)
			ret = 0;
		else if (host->sw_ver == CHIP_VER_E2)
			goto V1;
		else
			ret = 2;
		break;
	default:
		break;
	}
#endif
	return ret;
}

static void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	u32 tune_reg = host->dev_comp->pad_tune_reg;
	u32 val;
	int timing_changed = 0, ret;

	if (!hz) {
		dev_dbg(host->dev, "set mclk to 0\n");
		host->mclk = 0;
		mmc->actual_clock = 0;
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
		return;
	}

	if (host->timing != timing)
		timing_changed  = 1;
	flags = readl(host->base + MSDC_INTEN);
	sdr_clr_bits(host->base + MSDC_INTEN, flags);
	if (host->dev_comp->clk_div_bits == 8)
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_HS400_CK_MODE);
	else
		sdr_clr_bits(host->base + MSDC_CFG,
			     MSDC_CFG_HS400_CK_MODE_EXTRA);
	if (timing == MMC_TIMING_UHS_DDR50 ||
	    timing == MMC_TIMING_MMC_DDR52 ||
	    timing == MMC_TIMING_MMC_HS400) {
		if (timing == MMC_TIMING_MMC_HS400)
			mode = 0x3;
		else
			mode = 0x2; /* ddr mode and use divisor */

		if (hz >= (host->src_clk_freq >> 2)) {
			div = 0; /* mean div = 1/4 */
			sclk = host->src_clk_freq >> 2; /* sclk = clk / 4 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) / (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
			div = (div >> 1);
		}

		if (timing == MMC_TIMING_MMC_HS400 &&
		    hz >= (host->src_clk_freq >> 1)) {
			if (host->dev_comp->clk_div_bits == 8)
				sdr_set_bits(host->base + MSDC_CFG,
					     MSDC_CFG_HS400_CK_MODE);
			else
				sdr_set_bits(host->base + MSDC_CFG,
					     MSDC_CFG_HS400_CK_MODE_EXTRA);
			sclk = host->src_clk_freq >> 1;
			div = 0; /* div is ignore when bit18 is set */
		}
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	} else if (hz >= host->src_clk_freq) {
		mode = 0x1; /* no divisor */
		div = 0;
		sclk = host->src_clk_freq;
#endif
	} else {
		mode = 0x0; /* use divisor */
		if (hz >= (host->src_clk_freq >> 1)) {
			div = 0; /* mean div = 1/2 */
			sclk = host->src_clk_freq >> 1; /* sclk = clk / 2 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) / (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
		}
	}
	sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);

	ret = msdc_prepare_set_mclk(host, true);
	if (ret)
		dev_info(host->dev, "[gate] msdc prepare set clk err: %d\n", ret);
	if (host->dev_comp->clk_div_bits == 8)
		sdr_set_field(host->base + MSDC_CFG,
			      MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			      (mode << 8) | div);
	else
		sdr_set_field(host->base + MSDC_CFG,
			      MSDC_CFG_CKMOD_EXTRA | MSDC_CFG_CKDIV_EXTRA,
			      (mode << 12) | div);

	ret = msdc_prepare_set_mclk(host, false);
	if (ret)
		dev_info(host->dev, "[ungate] msdc prepare set clk err: %d\n", ret);
	readl_poll_timeout(host->base + MSDC_CFG, val, (val & MSDC_CFG_CKSTB), 0, 0);
	if (host->mclk == 0 && (mmc->caps2 & MMC_CAP2_NO_MMC)
		&& mmc->ios.signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
		usleep_range(1000, 1500);
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
	}
	mmc->actual_clock = sclk;
	host->mclk = hz;
	host->timing = timing;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);
	sdr_set_bits(host->base + MSDC_INTEN, flags);

	/*
	 * mmc_select_hs400() will drop to 50Mhz and High speed mode,
	 * tune result of hs200/200Mhz is not suitable for 50Mhz
	 */
	if (mmc->actual_clock <= 52000000) {
		writel(host->def_tune_para.iocon, host->base + MSDC_IOCON);
		if (host->top_base) {
			writel(host->def_tune_para.emmc_top_control,
			       host->top_base + EMMC_TOP_CONTROL);
			writel(host->def_tune_para.emmc_top_cmd,
			       host->top_base + EMMC_TOP_CMD);
		} else {
			writel(host->def_tune_para.pad_tune,
			       host->base + tune_reg);
		}
	} else {
		writel(host->saved_tune_para.iocon, host->base + MSDC_IOCON);
		writel(host->saved_tune_para.pad_cmd_tune,
		       host->base + PAD_CMD_TUNE);
		if (host->top_base) {
			writel(host->saved_tune_para.emmc_top_control,
			       host->top_base + EMMC_TOP_CONTROL);
			writel(host->saved_tune_para.emmc_top_cmd,
			       host->top_base + EMMC_TOP_CMD);
		} else {
			writel(host->saved_tune_para.pad_tune,
			       host->base + tune_reg);
		}
	}

	if (timing == MMC_TIMING_MMC_HS400 &&
	    host->dev_comp->hs400_tune)
		sdr_set_field(host->base + tune_reg,
			      MSDC_PAD_TUNE_CMDRRDLY,
			      host->hs400_cmd_int_delay);
	if (timing_changed && (support_new_tx(host->dev_comp->new_tx_ver)
		|| support_new_rx(host->dev_comp->new_rx_ver)))
		msdc_new_tx_rx_setting(host, timing);

	dev_info(host->dev, "sclk: %d, timing: %d\n", mmc->actual_clock,
		timing);
}

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;

	host->use_cmd_intr = false;
	switch (mmc_resp_type(cmd)) {
		/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = 0x1;
		break;
	case MMC_RSP_R1B:
		resp = 0x7;
		host->use_cmd_intr = true;
		break;
	case MMC_RSP_R2:
		resp = 0x2;
		break;
	case MMC_RSP_R3:
		resp = 0x3;
		break;
	case MMC_RSP_NONE:
	default:
		resp = 0x0;
		break;
	}

	return resp;
}

static inline u32 msdc_cmd_prepare_raw_cmd(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 | opcode
	 */
	u32 opcode = cmd->opcode;
	u32 resp = msdc_cmd_find_resp(host, mrq, cmd);
	u32 rawcmd = (opcode & 0x3f) | ((resp & 0x7) << 7);
	u32 blksz = (readl(host->base + SDC_CMD) >> 16) & 0xFFF;

	host->cmd_rsp = resp;

	if ((opcode == SD_IO_RW_DIRECT && cmd->flags == (unsigned int) -1) ||
	    opcode == MMC_STOP_TRANSMISSION)
		rawcmd |= (0x1 << 14);
	else if (opcode == SD_SWITCH_VOLTAGE)
		rawcmd |= (0x1 << 30);
	else if (opcode == SD_APP_SEND_SCR ||
		 opcode == SD_APP_SEND_NUM_WR_BLKS ||
		 (opcode == SD_SWITCH && mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == SD_APP_SD_STATUS && mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == MMC_SEND_EXT_CSD && mmc_cmd_type(cmd) == MMC_CMD_ADTC))
		rawcmd |= (0x1 << 11);

	if (cmd->data) {
		struct mmc_data *data = cmd->data;

		if (mmc_op_multi(opcode)) {
			if (mmc_card_mmc(mmc->card) && mrq->sbc &&
			    !(mrq->sbc->arg & 0xFFFF0000))
				rawcmd |= 0x2 << 28; /* AutoCMD23 */
		}

		rawcmd |= ((data->blksz & 0xFFF) << 16);
		if (data->flags & MMC_DATA_WRITE)
			rawcmd |= (0x1 << 13);
		if (data->blocks > 1)
			rawcmd |= (0x2 << 11);
		else
			rawcmd |= (0x1 << 11);
		/* Always use dma mode */
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_PIO);

		if (host->timeout_ns != data->timeout_ns ||
		    host->timeout_clks != data->timeout_clks)
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);

		writel(data->blocks, host->base + SDC_BLK_NUM);
	} else
		rawcmd |= blksz << 16;

	return rawcmd;
}

static void msdc_start_data(struct msdc_host *host, struct mmc_request *mrq,
			    struct mmc_command *cmd, struct mmc_data *data)
{
	bool read;

	WARN_ON(host->data);
	host->data = data;
	read = data->flags & MMC_DATA_READ;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	msdc_dma_setup(host, &host->dma, data);
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);
	udelay(1);
	sdr_set_bits(host->base + MSDC_INTEN, data_ints_mask);
	dev_dbg(host->dev, "DMA start\n");
	dev_dbg(host->dev, "%s: cmd=%d DMA data: %d blocks; read=%d\n",
			__func__, cmd->opcode, data->blocks, read);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
	if (cmd->opcode == MMC_EXECUTE_READ_TASK ||
		cmd->opcode == MMC_EXECUTE_WRITE_TASK)
		msdc_request_done(host, mrq);
#endif
}

static int msdc_auto_cmd_done(struct msdc_host *host, int events,
		struct mmc_command *cmd)
{
	u32 *rsp = cmd->resp;

	rsp[0] = readl(host->base + SDC_ACMD_RESP);

	if (events & MSDC_INT_ACMDRDY) {
		cmd->error = 0;
		host->need_tune = false;
	} else {
		msdc_reset_hw(host);
		if (events & MSDC_INT_ACMDCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_STOP_EIO;
		} else if (events & MSDC_INT_ACMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
		}
		host->need_tune = true;
		dev_err(host->dev,
			"%s: AUTO_CMD=%d arg=0x%X; rsp 0x%X; cmd_error=%d; "
			"host_error= 0x%X\n",
			__func__, cmd->opcode, cmd->arg, rsp[0], cmd->error,
			host->error);
	}
	return cmd->error;
}

/*
 * msdc_recheck_sdio_irq - recheck whether the SDIO irq is lost
 *
 * Host controller may lost interrupt in some special case.
 * Add SDIO irq recheck mechanism to make sure all interrupts
 * can be processed immediately
 */
static void msdc_recheck_sdio_irq(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u32 reg_int, reg_inten, reg_ps;

	if (mmc->caps & MMC_CAP_SDIO_IRQ) {
		reg_inten = readl(host->base + MSDC_INTEN);
		if (reg_inten & MSDC_INTEN_SDIOIRQ) {
			reg_int = readl(host->base + MSDC_INT);
			reg_ps = readl(host->base + MSDC_PS);
			if (!(reg_int & MSDC_INT_SDIOIRQ ||
			      reg_ps & MSDC_PS_DATA1)) {
				__msdc_enable_sdio_irq(host, 0);
				sdio_signal_irq(mmc);
			}
		}
	}
}

static void msdc_track_cmd_data(struct msdc_host *host,
				struct mmc_command *cmd, struct mmc_data *data)
{
	if (host->error)
		dev_dbg(host->dev, "%s: cmd=%d arg=%08X; host->error=0x%08X\n",
			__func__, cmd->opcode, cmd->arg, host->error);
}

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;


	/*
	 * No need check the return value of cancel_delayed_work, as only ONE
	 * path will go here!
	 */
	cancel_delayed_work(&host->req_timeout);

	spin_lock_irqsave(&host->lock, flags);
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	msdc_track_cmd_data(host, mrq->cmd, mrq->data);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
	if (mrq->cmd && mrq->cmd->opcode != MMC_EXECUTE_READ_TASK &&
		mrq->cmd->opcode != MMC_EXECUTE_WRITE_TASK) {
#endif
		if (mrq->data)
			msdc_unprepare_data(host, mrq->data);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
		}
#endif

	if (host->error)
		msdc_reset_hw(host);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	if (mrq->data && mrq->cmd->opcode != MMC_EXECUTE_READ_TASK
		&& mrq->cmd->opcode != MMC_EXECUTE_WRITE_TASK) {
		mmc_mtk_biolog_transfer_req_compl(mmc_from_priv(host), 0, 0);
		mmc_mtk_biolog_check(mmc_from_priv(host), 0);
	}
#endif

#if IS_ENABLED(CONFIG_SEC_MMC_FEATURE)
	mmc_sd_sec_check_req_err(host, mrq);
#endif

	mmc_request_done(mmc_from_priv(host), mrq);
	if (host->dev_comp->recheck_sdio_irq)
		msdc_recheck_sdio_irq(host);

}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	unsigned long flags;
	u32 *rsp;
	struct mmc_host *mmc = mmc_from_priv(host);

	if (mrq->sbc && cmd == mrq->cmd &&
	    (events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
				   | MSDC_INT_ACMDTMO)))
		msdc_auto_cmd_done(host, events, mrq->sbc);

	sbc_error = mrq->sbc && mrq->sbc->error;

	if (!sbc_error && !(events & (MSDC_INT_CMDRDY
					| MSDC_INT_RSPCRCERR
					| MSDC_INT_CMDTMO)))
		return done;

	spin_lock_irqsave(&host->lock, flags);
	done = !host->cmd;
	host->cmd = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;
	rsp = cmd->resp;

	sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			rsp[0] = readl(host->base + SDC_RESP3);
			rsp[1] = readl(host->base + SDC_RESP2);
			rsp[2] = readl(host->base + SDC_RESP1);
			rsp[3] = readl(host->base + SDC_RESP0);
		} else {
			rsp[0] = readl(host->base + SDC_RESP0);
		}
	}

	if (!sbc_error && !(events & MSDC_INT_CMDRDY)) {
		if (events & MSDC_INT_CMDTMO ||
		    (cmd->opcode != MMC_SEND_TUNING_BLOCK &&
		     cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200 &&
		     !host->hs400_tuning))
			/*
			 * should not clear fifo/interrupt as the tune data
			 * may have already come when cmd19/cmd21 gets response
			 * CRC error.
			 */
			msdc_reset_hw(host);
		if (events & MSDC_INT_RSPCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_CMD_EIO;
			host->need_tune = true;

			if (!(mmc->retune_crc_disable)
					&& cmd->opcode != MMC_SEND_TUNING_BLOCK
					&& cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200
					&& cmd->opcode != MMC_SEND_STATUS) {
				dev_info(host->dev, "need retune since cmd%d crc error\n", cmd->opcode);
				if (mmc->ios.timing == MMC_TIMING_MMC_HS200)
					host->is_skip_hs200_tune = 0;
				mmc_retune_needed(mmc);
			}
			bitmap_set(host->err_bag.err_bitmap,0,1);
			host->err_bag.cmd_opcode = cmd->opcode;
			host->err_bag.cmd_arg = cmd->arg;
			host->err_bag.cmd_rsp = rsp[0];
		} else if (events & MSDC_INT_CMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_CMD_TMO;
			bitmap_set(host->err_bag.err_bitmap,1,1);
			host->err_bag.cmd_opcode = cmd->opcode;
			host->err_bag.cmd_arg = cmd->arg;
			host->err_bag.cmd_rsp = rsp[0];
		}
	} else {
		host->need_tune = false;
		host->retune_times = 0;
	}

	if (cmd->error)
		host->need_tune = true;

	if (cmd->opcode == MMC_CMDQ_TASK_MGMT && host->id == MSDC_EMMC) {
		/* if resp is incorrect for cmd48, return a error to reset MMC device */
		if	(cmd->resp[0] != 0x0900)
			cmd->error = -EIO;
		dev_info(host->dev, "%s: cmd=48, error=%d, resp=0x%08X\n",
			__func__, cmd->error, cmd->resp[0]);
	}

	msdc_cmd_next(host, mrq, cmd);
	return true;
}

/* It is the core layer's responsibility to ensure card status
 * is correct before issue a request. but host design do below
 * checks recommended.
 */
static inline bool msdc_cmd_is_ready(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 val;
	int ret;

	/* The max busy time we can endure is 20ms */
	ret = readl_poll_timeout_atomic(host->base + SDC_STS, val,
					!(val & SDC_STS_CMDBUSY), 1, 20000);
	if (ret) {
		dev_err(host->dev, "CMD bus busy detected\n");
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}

	if (mmc_resp_type(cmd) == MMC_RSP_R1B || cmd->data) {
		/* R1B or with data, should check SDCBUSY */
		ret = readl_poll_timeout_atomic(host->base + SDC_STS, val,
						!(val & SDC_STS_SDCBUSY), 1, 20000);
		if (ret) {
			dev_err(host->dev, "Controller busy detected\n");
			msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
			return false;
		}
	}
	return true;
}

static bool msdc_command_resp_polling(struct msdc_host *host,
	struct mmc_request *mrq, struct mmc_command *cmd,
	unsigned long timeout)
{
	bool ret = false;
	unsigned long tmo;
	int events;

retry:
	/* polling */
	tmo = jiffies + timeout;
	while (1) {
		events = readl(host->base + MSDC_INT);
		if ((events & cmd_ints_mask) != 0) {
			/* clear all int flag */
			events &= cmd_ints_mask;
			writel(events, host->base + MSDC_INT);
			break;
		}

		if (time_after(jiffies, tmo) &&
			((readl(host->base + MSDC_INT) & cmd_ints_mask) == 0)) {
			dev_info(host->dev, "[%s]: CMD<%d> polling_for_completion timeout ARG<0x%.8x>\n",
				__func__, cmd->opcode, cmd->arg);
			ret = msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
			goto exit;
		}
	}

	if (cmd)
		ret = msdc_cmd_done(host, events, mrq, cmd);

	/* if only autocmd23 done,
	 * it needs to polling read/write interrupt directly
	 */
	if (!ret && mrq->sbc && cmd == mrq->cmd &&
	    (events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
		| MSDC_INT_ACMDTMO)))
		goto retry;

exit:
	return ret;
}

static void msdc_start_command(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 rawcmd;
	unsigned long flags;

	WARN_ON(host->cmd);
	host->cmd = cmd;
	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	if (!msdc_cmd_is_ready(host, mrq, cmd))
		return;

	if ((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16 ||
	    readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT ||
	    readl(host->base + MSDC_DMA_CFG) & MSDC_DMA_CFG_STS) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);

	spin_lock_irqsave(&host->lock, flags);
	if (host->use_cmd_intr) {
		sdr_set_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
		sdr_set_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	} else {
		sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	}
	spin_unlock_irqrestore(&host->lock, flags);

	writel(cmd->arg, host->base + SDC_ARG);
	writel(rawcmd, host->base + SDC_CMD);
}

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	if ((cmd->error &&
	    !(cmd->error == -EILSEQ &&
	    (cmd->opcode == MMC_SEND_TUNING_BLOCK ||
	    cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200 ||
	    host->hs400_tuning))) || (mrq->sbc && mrq->sbc->error))
		msdc_request_done(host, mrq);
	else if (cmd == mrq->sbc) {
		msdc_start_command(host, mrq, mrq->cmd);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->cmd, CMD_TIMEOUT);
	} else if (!cmd->data)
		msdc_request_done(host, mrq);
	else
		msdc_start_data(host, mrq, cmd, cmd->data);
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
static inline bool msdc_op_cmdq_on_tran(struct mmc_command *cmd)
{
	return cmd->opcode == MMC_QUE_TASK_PARAMS ||
	       cmd->opcode == MMC_QUE_TASK_ADDR ||
		   (cmd->opcode == MMC_SEND_STATUS && cmd->arg & (1 << 15));
}

static unsigned int msdc_cmdq_command_start(struct msdc_host *host,
	struct mmc_command *cmd, unsigned long timeout)
{
	unsigned long flags;
	unsigned long tmo;

	cmd->error = 0;
	tmo = jiffies + timeout;

	while (!msdc_cmd_is_ready(host, host->mrq, cmd)) {
		if (time_after(jiffies, tmo) &&
			!msdc_cmd_is_ready(host, host->mrq, cmd)) {
			dev_info(host->dev, "cmd_busy timeout: before CMD<%d>",
				 cmd->opcode);
			cmd->error = -ETIMEDOUT;
			return cmd->error;
		}
	}

	/* disable cmd interrupts for cq cmd */
	spin_lock_irqsave(&host->lock, flags);
	host->use_cmd_intr = false;
	sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	spin_unlock_irqrestore(&host->lock, flags);

	sdr_set_field(host->base + EMMC51_CFG0, EMMC51_CMDQ_MASK,
			(0x81) | (cmd->opcode << 1));
	writel(cmd->arg, host->base + SDC_ARG);
	writel(0, host->base + SDC_CMD);

	return 0;
}

static unsigned int msdc_cmdq_command_resp_polling(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long timeout)
{
	unsigned long flags;
	u32 events;
	unsigned long tmo;
	u32 event_mask = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;
	u64 rsp_time __maybe_unused;

	/* polling */
	tmo = jiffies + timeout;
	rsp_time = sched_clock();
	while (1) {
		spin_lock_irqsave(&host->lock, flags);
		events = readl(host->base + MSDC_INT);
		if (events & event_mask) {
			/* clear all int flag */
			events &= event_mask;
			writel(events, host->base + MSDC_INT);
			spin_unlock_irqrestore(&host->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&host->lock, flags);

		if (time_after(jiffies, tmo)) {
			spin_lock_irqsave(&host->lock, flags);
			events = readl(host->base + MSDC_INT);
			spin_unlock_irqrestore(&host->lock, flags);
			if (!(events & event_mask)) {
				dev_info(host->dev,
					"[%s]: CMD<%d> polling_for_completion timeout ARG<0x%.8x>",
					__func__, cmd->opcode, cmd->arg);
				cmd->error = -ETIMEDOUT;
			}
			goto out;
		}
	}

	/* command interrupts */
	if (events & event_mask) {
		if (events & MSDC_INT_CMDRDY) {
			cmd->resp[0] = readl(host->base + SDC_RESP0);
		} else if (events & MSDC_INT_RSPCRCERR) {
			cmd->error = -EILSEQ;
			dev_info(host->dev,
				"[%s]: XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>",
				__func__, cmd->opcode, cmd->arg);
		} else if (events & MSDC_INT_CMDTMO) {
			cmd->error = -ETIMEDOUT;
			dev_info(host->dev, "[%s]: XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>",
				__func__, cmd->opcode, cmd->arg);
		}
	}
out:
	sdr_clr_bits(host->base + EMMC51_CFG0, EMMC51_CFG_CMDQEN);
	return cmd->error;
}

#define CMD_CQ_TIMEOUT (HZ * 3)
static void msdc_start_request_cmdq(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (msdc_cmdq_command_start(host, mrq->cmd, CMD_CQ_TIMEOUT))
		goto end;

	if (msdc_cmdq_command_resp_polling(host, mrq->cmd, CMD_CQ_TIMEOUT))
		goto end;
end:
	host->mrq = NULL;
	return;

}
#endif

static bool modify_ra_in_bottom(struct mmc_request *mrq)
{
	struct mmc_queue_req *mqrq;
	struct request *req;
	unsigned long pre_ra_pages;

	mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
	req = blk_mq_rq_from_pdu(mqrq);
	if (!req || !req->q)
		return false;
	if (!req->q->disk || !req->q->disk->bdi)
		return false;

	pre_ra_pages = req->q->disk->bdi->ra_pages;
	pr_info("[ra_in_fix] before fix ra = %lu\n", pre_ra_pages);
	req->q->disk->bdi->ra_pages = RA_FOR_PERF;
	pr_info("[ra_in_fix] after fix ra = %lu  request_queue = %p  disk = %p bdi = %p\n",
				req->q->disk->bdi->ra_pages, req->q, req->q->disk, req->q->disk->bdi);

	if (pre_ra_pages == RA_FOR_PERF)
		return true;
	return false;
}

static bool mmc_op_read(u32 opcode)
{
	return opcode == MMC_READ_SINGLE_BLOCK ||
	       opcode == MMC_READ_MULTIPLE_BLOCK;
}

static bool mmc_op_write(u32 opcode)
{
	return opcode == MMC_WRITE_BLOCK ||
	       opcode == MMC_WRITE_MULTIPLE_BLOCK;
}

static void msdc_start_request_legacy(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (!mmc_card_mmc(mmc->card) ||
	    (mrq->sbc->arg & 0xFFFF0000))) {
		msdc_start_command(host, mrq, mrq->sbc);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->sbc, CMD_TIMEOUT);
	} else {
		msdc_start_command(host, mrq, mrq->cmd);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->cmd, CMD_TIMEOUT);
	}
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->error = 0;
	WARN_ON(host->mrq);
	host->mrq = mrq;

	if (mrq->data)
		msdc_prepare_data(host, mrq->data);

	if (!mrq->host)
		mrq->host = mmc;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	if (mrq->data && mrq->cmd->opcode != MMC_EXECUTE_READ_TASK
		&& mrq->cmd->opcode != MMC_EXECUTE_WRITE_TASK) {
		mmc_mtk_biolog_send_command(0, mrq);
		mmc_mtk_biolog_check(mmc, 1);
	}
#endif
	if (ra_fixed == false) {
		if ((mmc_op_write(mrq->cmd->opcode) || mmc_op_read(mrq->cmd->opcode))
		&& mmc_card_sd(mmc->card) && (rw_times < EARLY_RW)) {
			rw_times++;
			pr_info("[ra_count_r] %d cmd = %d\n", rw_times, mrq->cmd->opcode);
			if (modify_ra_in_bottom(mrq))
				ra_fixed = true;
		}
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
	if (msdc_op_cmdq_on_tran(mrq->cmd))
		msdc_start_request_cmdq(mmc, mrq);
	else
#endif
		msdc_start_request_legacy(mmc, mrq);

}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	msdc_prepare_data(host, data);
	data->host_cookie |= MSDC_ASYNC_FLAG;
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
		int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	if (data->host_cookie) {
		data->host_cookie &= ~MSDC_ASYNC_FLAG;
		msdc_unprepare_data(host, data);
	}
}

static void msdc_data_xfer_next(struct msdc_host *host, struct mmc_request *mrq)
{
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
	if (host->cqhci && host->swcq_host) {
		struct swcq_host *swcq_host = host->swcq_host;

		if (atomic_read(&swcq_host->ongoing_task.id)
			!= MMC_SWCQ_TASK_IDLE) {
			atomic_set(&swcq_host->ongoing_task.done, 1);
			wake_up_interruptible(&swcq_host->wait_dat_trans);
			return;
		}
	}
#endif
	if (mrq && mrq->cmd) {
		if (mmc_op_multi(mrq->cmd->opcode)
			&& mrq->stop && !mrq->stop->error && !mrq->sbc) {
			msdc_start_command(host, mrq, mrq->stop);
			if (!host->use_cmd_intr)
				msdc_command_resp_polling(host, mrq,
					mrq->stop, CMD_TIMEOUT);
		} else
			msdc_request_done(host, mrq);
	}
}

static bool msdc_data_xfer_done(struct msdc_host *host, u32 events,
				struct mmc_request *mrq, struct mmc_data *data)
{
	struct mmc_command *stop;
	unsigned long flags;
	bool done;
	unsigned int check_data = events &
	    (MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO
	     | MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR
	     | MSDC_INT_DMA_PROTECT);
	u32 val;
	int ret;

	spin_lock_irqsave(&host->lock, flags);
	done = !host->data;
	if (check_data)
		host->data = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;
	stop = data->stop;

	if (check_data || (stop && stop->error)) {
		dev_dbg(host->dev, "DMA status: 0x%8X\n",
				readl(host->base + MSDC_DMA_CFG));
		sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP,
				1);

		ret = readl_poll_timeout_atomic(host->base + MSDC_DMA_CTRL, val,
						!(val & MSDC_DMA_CTRL_STOP), 1, 20000);
		if (ret)
			dev_info(host->dev, "DMA stop timed out\n");

		spin_lock_irqsave(&host->lock, flags);
		sdr_clr_bits(host->base + MSDC_INTEN, data_ints_mask);
		spin_unlock_irqrestore(&host->lock, flags);
		dev_dbg(host->dev, "DMA stop\n");

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error)) {
			data->bytes_xfered = data->blocks * data->blksz;
			host->need_tune = false;
			host->retune_times = 0;
		} else {
			dev_dbg(host->dev, "interrupt events: %x\n", events);
			msdc_reset_hw(host);
			host->need_tune = true;
			host->error |= REQ_DAT_ERR;
			data->bytes_xfered = 0;

			if (events & MSDC_INT_DATTMO) {
				data->error = -ETIMEDOUT;
				host->data_timeout_cont++;
				bitmap_set(host->err_bag.err_bitmap,3,1);
				if(data != NULL && data->mrq != NULL && data->mrq->cmd != NULL) {
					host->err_bag.cmd_opcode = data->mrq->cmd->opcode;
					host->err_bag.blocks = data->blocks;
					host->err_bag.bytes_xfered = data->bytes_xfered;
				} else {
					dev_err(host->dev,
					"%s: data->mrq or data->mrq->cmd is NULL\n",
					__func__);
				}
			} else if (events & MSDC_INT_DATCRCERR) {
				host->data_timeout_cont = 0;
				data->error = -EILSEQ;
				bitmap_set(host->err_bag.err_bitmap,2,1);
				if(data != NULL && data->mrq != NULL && data->mrq->cmd != NULL) {
					host->err_bag.cmd_opcode = data->mrq->cmd->opcode;
					host->err_bag.blocks = data->blocks;
					host->err_bag.bytes_xfered = data->bytes_xfered;
				} else {
					dev_err(host->dev,
					"%s: data->mrq or data->mrq->cmd is NULL\n",
					__func__);
				}
			}
		}

		msdc_data_xfer_next(host, mrq);
		done = true;
	}
	return done;
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = readl(host->base + SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	writel(val, host->base + SDC_CFG);
	dev_dbg(host->dev, "Bus Width = %d", width);
}

static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	struct msdc_host *host = mmc_priv(mmc);
	int ret;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_330 &&
		    ios->signal_voltage != MMC_SIGNAL_VOLTAGE_180) {
			dev_err(host->dev, "Unsupported signal voltage!\n");
			return -EINVAL;
		}

		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			dev_dbg(host->dev, "Regulator set error %d (%d)\n",
				ret, ios->signal_voltage);
			return ret;
		} else
			dev_dbg(host->dev, "Regulator set success %d (%d)\n",
				ret, ios->signal_voltage);

		/* Apply different pinctrl settings for different signal voltage */
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
			host->pins_state = PINS_UHS;
			pinctrl_select_state(host->pinctrl, host->pins_uhs);
		} else {
			host->pins_state = PINS_DEFAULT;
			pinctrl_select_state(host->pinctrl, host->pins_default);
		}
	}
#endif
	return 0;
}

static int msdc_card_busy(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 status = readl(host->base + MSDC_PS);

	/* only check if data0 is low */
	return !(status & BIT(16));
}

static void msdc_request_timeout(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
			req_timeout.work);

	/* simulate HW timeout status */
	dev_err(host->dev, "%s: aborting cmd/data/mrq\n", __func__);
	if (host->mrq) {
		dev_err(host->dev, "%s: aborting mrq=%p cmd=%d\n", __func__,
				host->mrq, host->mrq->cmd->opcode);
		if (host->cmd) {
			dev_err(host->dev, "%s: aborting cmd=%d\n",
					__func__, host->cmd->opcode);
			msdc_cmd_done(host, MSDC_INT_CMDTMO, host->mrq,
					host->cmd);
		} else if (host->data) {
			dev_err(host->dev, "%s: abort data: cmd%d; %d blocks\n",
					__func__, host->mrq->cmd->opcode,
					host->data->blocks);
			msdc_data_xfer_done(host, MSDC_INT_DATTMO, host->mrq,
					host->data);
		}
	}
}

static void __msdc_enable_sdio_irq(struct msdc_host *host, int enb)
{
	if (enb) {
		sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
		if (host->dev_comp->recheck_sdio_irq)
			msdc_recheck_sdio_irq(host);
	} else {
		sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	}
}

static void msdc_enable_sdio_irq(struct mmc_host *mmc, int enb)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	spin_lock_irqsave(&host->lock, flags);
	__msdc_enable_sdio_irq(host, enb);
	spin_unlock_irqrestore(&host->lock, flags);

	if (enb)
		pm_runtime_get_noresume(host->dev);
	else
		pm_runtime_put_noidle(host->dev);
}

static irqreturn_t msdc_cmdq_irq(struct msdc_host *host, u32 intsts)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	int cmd_err = 0, dat_err = 0;

	if (intsts & MSDC_INT_RSPCRCERR) {
		cmd_err = -EILSEQ;
		dev_err(host->dev, "%s: CMD CRC ERR", __func__);
	} else if (intsts & MSDC_INT_CMDTMO) {
		cmd_err = -ETIMEDOUT;
		dev_err(host->dev, "%s: CMD TIMEOUT ERR", __func__);
	}

	if (intsts & MSDC_INT_DATCRCERR) {
		dat_err = -EILSEQ;
		dev_err(host->dev, "%s: DATA CRC ERR", __func__);
	} else if (intsts & MSDC_INT_DATTMO) {
		dat_err = -ETIMEDOUT;
		dev_err(host->dev, "%s: DATA TIMEOUT ERR", __func__);
	}

	if (cmd_err || dat_err) {
		writel(msdc_ints_err, host->base + MSDC_INT);
		dev_err(host->dev, "cmd_err = %d, dat_err =%d, intsts = 0x%x",
			cmd_err, dat_err, intsts);
		msdc_dump_info(NULL, 0, NULL, host);
	}

	return cqhci_irq(mmc, 0, cmd_err, dat_err);
}

static irqreturn_t msdc_thread_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *) dev_id;
	unsigned int ret = 0, i = 0;
	int len = 0;
	unsigned long flags;
	struct err_info_bag dump_bag;

	if (atomic_read(&host->err_dropped) >= 128) {
		dev_info(host->dev, "Err kfifo has been full for a time and too much log dropped\n");
		atomic_set(&host->err_dropped, 0);
	}

	spin_lock_irqsave(&host->err_info_lock, flags);
	if (!kfifo_is_empty(&host->err_info_bag_ring))
		len = kfifo_len(&host->err_info_bag_ring);
	spin_unlock_irqrestore(&host->err_info_lock, flags);

	for (i = 0; i < len; i++) {
		spin_lock_irqsave(&host->err_info_lock, flags);
		ret = kfifo_out(&host->err_info_bag_ring, &dump_bag, 1);
		spin_unlock_irqrestore(&host->err_info_lock, flags);
		if (ret != 1) {
			dev_info(host->dev, "Err kfifo out is fail\n");
			break;
		}
		if (dump_bag.err_bitmap[0] & (ERR_CMD_CRC | ERR_CMD_TMO)) {
			if (dump_bag.err_bitmap[0] & ERR_CMD_CRC)
				dev_info(host->dev, "At time:%lld cmd_crc happen cmd=%d arg=0x%X; rsp 0x%X;\n",
					dump_bag.irq_timestamp, dump_bag.cmd_opcode, dump_bag.cmd_arg,
					dump_bag.cmd_rsp);
			if (dump_bag.err_bitmap[0] & ERR_CMD_TMO)
				dev_info(host->dev, "At time:%lld cmd_tmo happen cmd=%d arg=0x%X; rsp 0x%X;\n",
					dump_bag.irq_timestamp, dump_bag.cmd_opcode, dump_bag.cmd_arg,
					dump_bag.cmd_rsp);
		}
		if (dump_bag.err_bitmap[0] & (ERR_DAT_CRC | ERR_DAT_TMO)) {
			if (dump_bag.err_bitmap[0] & ERR_DAT_CRC)
				dev_info(host->dev, "At time:%lld dat_crc happen cmd=%d; blocks=%d; xfer_size=%d;\n",
					dump_bag.irq_timestamp, dump_bag.cmd_opcode, dump_bag.blocks,
					dump_bag.bytes_xfered);
			if (dump_bag.err_bitmap[0] & ERR_DAT_TMO)
				dev_info(host->dev, "At time:%lld dat_tmo happen cmd=%d; blocks=%d; xfer_size=%d;\n",
					dump_bag.irq_timestamp, dump_bag.cmd_opcode, dump_bag.blocks,
					dump_bag.bytes_xfered);
		}
		if (dump_bag.err_bitmap[0] & (ERR_MSDC_FIFOCS_CLR_TIMEOUT | ERR_MSDC_DMA_CFG_STS_TIMEOUT)) {
			if (dump_bag.err_bitmap[0] & ERR_MSDC_FIFOCS_CLR_TIMEOUT) {
				dev_info(host->dev, "At time:%lld ERR_MSDC_FIFOCS_CLR_TIMEOUT happen\n",
					dump_bag.irq_timestamp);
				msdc_dump_register_from_buf(host, 0);
			}
			if (dump_bag.err_bitmap[0] & ERR_MSDC_DMA_CFG_STS_TIMEOUT) {
				dev_info(host->dev, "At time:%lld ERR_MSDC_DMA_CFG_STS_TIMEOUT happen\n",
					dump_bag.irq_timestamp);
				msdc_dump_register_from_buf(host, 1);
			}
		}
	}

	return IRQ_HANDLED;
}


static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *) dev_id;
	struct mmc_host *mmc = mmc_from_priv(host);
	unsigned long flags;

	while (true) {
		struct mmc_request *mrq;
		struct mmc_command *cmd;
		struct mmc_data *data;
		u32 events, event_mask;

		spin_lock(&host->lock);
		events = readl(host->base + MSDC_INT);
		event_mask = readl(host->base + MSDC_INTEN);
		if ((events & event_mask) & MSDC_INT_SDIOIRQ)
			__msdc_enable_sdio_irq(host, 0);
		/* clear interrupts */
		writel(events & event_mask, host->base + MSDC_INT);

		mrq = host->mrq;
		cmd = host->cmd;
		data = host->data;
		spin_unlock(&host->lock);

		if ((events & event_mask) & MSDC_INT_SDIOIRQ)
			sdio_signal_irq(mmc);

		if ((events & event_mask) & MSDC_INT_CDSC) {
			if (host->internal_cd)
				mmc_detect_change(mmc, msecs_to_jiffies(20));
			events &= ~MSDC_INT_CDSC;
		}

		if (!(events & (event_mask & ~MSDC_INT_SDIOIRQ)))
			break;

		if ((mmc->caps2 & MMC_CAP2_CQE) &&
		    (events & MSDC_INT_CMDQ)) {
			msdc_cmdq_irq(host, events);
			/* clear interrupts */
			writel(events, host->base + MSDC_INT);
			return IRQ_HANDLED;
		}
#if !IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
		if (!mrq) {
			dev_err(host->dev,
				"%s: MRQ=NULL; events=%08X; event_mask=%08X\n",
				__func__, events, event_mask);
			WARN_ON(1);
			break;
		}
#endif
		dev_dbg(host->dev, "%s: events=%08X\n", __func__, events);

		if (cmd)
			msdc_cmd_done(host, events, mrq, cmd);
		else if (data)
			msdc_data_xfer_done(host, events, mrq, data);
	}

	if (!bitmap_empty(host->err_bag.err_bitmap, ERR_BIT_SIZE)) {
		host->err_bag.irq_timestamp = sched_clock();

		spin_lock_irqsave(&host->err_info_lock, flags);
		if (kfifo_is_full(&host->err_info_bag_ring) && kfifo_out(&host->err_info_bag_ring, &host->err_bag, 1))
			atomic_inc(&host->err_dropped);
		kfifo_in(&host->err_info_bag_ring, &host->err_bag, 1);
		spin_unlock_irqrestore(&host->err_info_lock, flags);

		bitmap_zero(host->err_bag.err_bitmap, ERR_BIT_SIZE);
		return IRQ_WAKE_THREAD;
	}
	return IRQ_HANDLED;
}

static void msdc_init_hw(struct msdc_host *host)
{
	u32 val;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	if (host->reset) {
		reset_control_assert(host->reset);
		usleep_range(10, 50);
		reset_control_deassert(host->reset);
	}

	if (support_new_tx(host->dev_comp->new_tx_ver)) {
		sdr_set_bits(host->base + SDC_ADV_CFG0, SDC_NEW_TX_EN);
		msdc_new_tx_rx_setting(host, host->timing);
	}
	if (support_new_rx(host->dev_comp->new_rx_ver))
		sdr_set_bits(host->base + MSDC_NEW_RX_CFG,
			MSDC_NEW_RX_PATH_SEL);

	/* Configure to MMC/SD mode */
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_MODE);

	/* Reset */
	msdc_reset_hw(host);

	/* Disable and clear all interrupts */
	writel(0, host->base + MSDC_INTEN);
	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);

	/* Configure card detection */
	if (host->internal_cd) {
		sdr_set_field(host->base + MSDC_PS, MSDC_PS_CDDEBOUNCE,
			      DEFAULT_DEBOUNCE);
		sdr_set_bits(host->base + MSDC_PS, MSDC_PS_CDEN);
		sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_CDSC);
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_INSWKUP);
	} else {
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_INSWKUP);
		sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);
		sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_CDSC);
	}

	if (host->top_base) {
		writel(0, host->top_base + EMMC_TOP_CONTROL);
		writel(0, host->top_base + EMMC_TOP_CMD);
	} else {
		writel(0, host->base + tune_reg);
	}
	writel(0, host->base + MSDC_IOCON);
	sdr_set_field(host->base + MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);
	writel(0x403c0046, host->base + MSDC_PATCH_BIT);
	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_CKGEN_MSDC_DLY_SEL, 1);
	writel(0xfffe4089, host->base + MSDC_PATCH_BIT1);
	/*
	 * high speed bus -> EMMC50_CFG2_AXI_SET_LEN = 0xf -> MSDC_PB1_ENABLE_SINGLE_BURST = 0
	 * low speed bus -> EMMC50_CFG2_AXI_SET_LEN = 0 -> MSDC_PB1_ENABLE_SINGLE_BURST = 1
	 */
	sdr_get_field(host->base + EMMC50_CFG2, EMMC50_CFG2_AXI_SET_LEN, &val);
	if (val == 0)
		sdr_set_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_ENABLE_SINGLE_BURST);
	else
		sdr_clr_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_ENABLE_SINGLE_BURST);
	sdr_set_bits(host->base + EMMC50_CFG0, EMMC50_CFG_CFCSTS_SEL);

	if (host->dev_comp->stop_clk_set.enable) {
		sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_STOP_DLY,
			host->dev_comp->stop_clk_set.stop_cnt & 0xF);
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_POP_EN_CNT,
			host->dev_comp->stop_clk_set.pop_cnt & 0xF);
		sdr_clr_bits(host->base + SDC_FIFO_CFG,
			     SDC_FIFO_CFG_WRVALIDSEL);
		sdr_clr_bits(host->base + SDC_FIFO_CFG,
			     SDC_FIFO_CFG_RDVALIDSEL);
	}

	if (host->dev_comp->busy_check)
		sdr_clr_bits(host->base + MSDC_PATCH_BIT1, (1 << 7));

	if (host->dev_comp->async_fifo) {
		sdr_set_field(host->base + MSDC_PATCH_BIT2,
			      MSDC_PB2_RESPWAIT, 3);
		if (host->dev_comp->enhance_rx) {
			if (host->top_base)
				sdr_set_bits(host->top_base + EMMC_TOP_CONTROL,
					     SDC_RX_ENH_EN);
			else
				sdr_set_bits(host->base + SDC_ADV_CFG0,
					     SDC_RX_ENHANCE_EN);
		} else {
			sdr_set_field(host->base + MSDC_PATCH_BIT2,
				      MSDC_PB2_RESPSTSENSEL, 2);
			sdr_set_field(host->base + MSDC_PATCH_BIT2,
				      MSDC_PB2_CRCSTSENSEL, 2);
		}
		/* use async fifo, then no need tune internal delay */
		sdr_clr_bits(host->base + MSDC_PATCH_BIT2,
			     MSDC_PATCH_BIT2_CFGRESP);
		sdr_set_bits(host->base + MSDC_PATCH_BIT2,
			     MSDC_PATCH_BIT2_CFGCRCSTS);
	}

	if (host->dev_comp->support_64g)
		sdr_set_bits(host->base + MSDC_PATCH_BIT2,
			     MSDC_PB2_SUPPORT_64G);

	if (host->dev_comp->data_tune) {
		if (host->top_base) {
			sdr_set_bits(host->top_base + EMMC_TOP_CONTROL,
				     PAD_DAT_RD_RXDLY_SEL);
			sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL,
				     DATA_K_VALUE_SEL);
			sdr_set_bits(host->top_base + EMMC_TOP_CMD,
				     PAD_CMD_RD_RXDLY_SEL);
		} else {
			sdr_set_bits(host->base + tune_reg,
				     MSDC_PAD_TUNE_RD_SEL |
				     MSDC_PAD_TUNE_CMD_SEL);
		}
	} else {
		/* choose clock tune */
		if (host->top_base)
			sdr_set_bits(host->top_base + EMMC_TOP_CONTROL,
				     PAD_RXDLY_SEL);
		else
			sdr_set_bits(host->base + tune_reg,
				     MSDC_PAD_TUNE_RXDLYSEL);
	}

	/* Configure to enable SDIO mode.
	 * it's must otherwise sdio cmd5 failed
	 */
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIO);

	/* Config SDIO device detect interrupt function */
	sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	sdr_set_bits(host->base + SDC_ADV_CFG0, SDC_DAT1_IRQ_TRIGGER);

	/* Configure to default data timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);

	/* default write data / busy timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_WRDTOC, 100);

	host->def_tune_para.iocon = readl(host->base + MSDC_IOCON);
	host->saved_tune_para.iocon = readl(host->base + MSDC_IOCON);
	if (host->top_base) {
		host->def_tune_para.emmc_top_control =
			readl(host->top_base + EMMC_TOP_CONTROL);
		host->def_tune_para.emmc_top_cmd =
			readl(host->top_base + EMMC_TOP_CMD);
		host->saved_tune_para.emmc_top_control =
			readl(host->top_base + EMMC_TOP_CONTROL);
		host->saved_tune_para.emmc_top_cmd =
			readl(host->top_base + EMMC_TOP_CMD);
	} else {
		host->def_tune_para.pad_tune = readl(host->base + tune_reg);
		host->saved_tune_para.pad_tune = readl(host->base + tune_reg);
	}

	host->need_tune = false;
	host->retune_times = 0;

}

static void msdc_deinit_hw(struct msdc_host *host)
{
	u32 val;

	if (host->internal_cd) {
		/* Disabled card-detect */
		sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_INSWKUP);
	}

	/* Disable and clear all interrupts */
	writel(0, host->base + MSDC_INTEN);

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct mt_gpdma_desc *gpd = dma->gpd;
	struct mt_bdma_desc *bd = dma->bd;
	dma_addr_t dma_addr;
	int i;

	memset(gpd, 0, sizeof(struct mt_gpdma_desc) * 2);

	dma_addr = dma->gpd_addr + sizeof(struct mt_gpdma_desc);
	gpd->gpd_info = GPDMA_DESC_BDP; /* hwo, cs, bd pointer */
	/* gpd->next is must set for desc DMA
	 * That's why must alloc 2 gpd structure.
	 */
	gpd->next = lower_32_bits(dma_addr);
	if (host->dev_comp->support_64g)
		gpd->gpd_info |= (upper_32_bits(dma_addr) & 0xf) << 24;

	dma_addr = dma->bd_addr;
	gpd->ptr = lower_32_bits(dma->bd_addr); /* physical address */
	if (host->dev_comp->support_64g)
		gpd->gpd_info |= (upper_32_bits(dma_addr) & 0xf) << 28;

	memset(bd, 0, sizeof(struct mt_bdma_desc) * MAX_BD_NUM);
	for (i = 0; i < (MAX_BD_NUM - 1); i++) {
		dma_addr = dma->bd_addr + sizeof(*bd) * (i + 1);
		bd[i].next = lower_32_bits(dma_addr);
		if (host->dev_comp->support_64g)
			bd[i].bd_info |= (upper_32_bits(dma_addr) & 0xf) << 24;
	}
}

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		msdc_init_hw(host);
		mmc->regulator_enabled = true;
		break;
	case MMC_POWER_ON:
		host->vqmmc_enabled = true;
		break;
	case MMC_POWER_OFF:
		mmc->regulator_enabled = false;
		host->vqmmc_enabled = false;
		break;
	default:
		break;
	}

	if (host->mclk != ios->clock || host->timing != ios->timing)
		msdc_set_mclk(host, ios->timing, ios->clock);
}
#else
static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret;

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc)) {
			msdc_init_hw(host);
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_err(host->dev, "Failed to set vmmc power!\n");
				return;
			} else
				dev_dbg(host->dev, "Success to set vmmc power!\n");
			/* There is a pulse signal in some case,
			 * so add delay to avoid false alarm when vmmc power up
			 */
			mdelay(3);
			ret = devm_regulator_register_notifier(mmc->supply.vmmc,
				&host->sd_oc.nb);
			if (ret) {
				dev_info(host->dev, "Failed to register vmmc nb!\n");
				return;
			} else
				dev_dbg(host->dev, "Success to register vmmc nb!\n");
		}
		if (host->id == MSDC_SD && host->pins_uhs &&
			ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
			host->pins_state = PINS_UHS;
			pinctrl_select_state(host->pinctrl, host->pins_uhs);
		} else if (host->pins_default) {
			host->pins_state = PINS_DEFAULT;
			pinctrl_select_state(host->pinctrl, host->pins_default);
		}
		break;
	case MMC_POWER_ON:
		if (mmc->supply.vqmmc == NULL || IS_ERR(mmc->supply.vqmmc)) {
			dev_info(host->dev, "vqmmc is null, not set!\n");
			break;
		}
		if (!IS_ERR(mmc->supply.vqmmc) && !host->vqmmc_enabled) {
			ret = regulator_enable(mmc->supply.vqmmc);
			if (ret)
				dev_err(host->dev, "Failed to set vqmmc power!\n");
			else {
				host->vqmmc_enabled = true;
				dev_dbg(host->dev, "Success to set vqmmc power!\n");
			}
		}
		if (host->id == MSDC_SD && host->pins_uhs &&
			ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180 ) {
			host->pins_state = PINS_UHS;
			pinctrl_select_state(host->pinctrl, host->pins_uhs);
		} else if (host->pins_default) {
			host->pins_state = PINS_DEFAULT;
			pinctrl_select_state(host->pinctrl, host->pins_default);
		}
		break;
	case MMC_POWER_OFF:
		if (mmc->supply.vmmc == NULL  || IS_ERR(mmc->supply.vmmc)) {
			dev_info(host->dev, "vmmc is null, not set!\n");
			break;
		}
		devm_regulator_unregister_notifier(mmc->supply.vmmc,
			&host->sd_oc.nb);
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}
		if (host->pins_pull_down)
			pinctrl_select_state(host->pinctrl, host->pins_pull_down);
		if (host->id == MSDC_SD) {
			if (host->mclk == 100000) {
				host->block_bad_card = 1;
				gpio_dump_regs_range(33, 33);
				pr_notice("[%s]: msdc%d power off at clk %dhz set block_bad_card = %d, SDcard status = %d\n",
					__func__, host->id, host->mclk,
					host->block_bad_card, mmc_gpio_get_cd(mmc));
			}
		}
		break;
	default:
		break;
	}

	if (host->mclk != ios->clock || host->timing != ios->timing)
		msdc_set_mclk(host, ios->timing, ios->clock);
}
#endif

static u32 test_delay_bit(u64 delay, u32 bit, u32 max_gears)
{
	bit %= max_gears;
	return delay & (1ULL << bit) ? 1 : 0;
}

static int get_delay_len(u64 delay, u32 start_bit, u32 max_gears)
{
	int i;

	for (i = 0; i < (max_gears - start_bit); i++) {
		if (test_delay_bit(delay, start_bit + i, max_gears) == 0)
			return i;
	}
	return max_gears - start_bit;
}

static struct msdc_delay_phase get_best_delay(struct msdc_host *host, u64 delay,
		u32 max_gears, int full_scan)
{
	int start = 0, len = 0;
	int start_final = 0, len_final = 0;
	u8 final_phase = 0xff;
	struct msdc_delay_phase delay_phase = { 0, };

	if (delay == 0) {
		dev_info(host->dev, "phase error: [map:%llx]\n", delay);
		delay_phase.final_phase = final_phase;
		return delay_phase;
	}

	while (start < max_gears) {
		len = get_delay_len(delay, start, max_gears);
		if (len_final < len) {
			start_final = start;
			len_final = len;
		}
		start += len ? len : 1;
		if (!full_scan)
			if (len >= 12 && start_final < 4)
				break;
	}

	/* The rule is that to find the smallest delay cell */
	if (start_final == 0 && !full_scan)
		final_phase = (start_final + len_final / 3) % max_gears;
	else
		final_phase = (start_final + len_final / 2) % max_gears;
	dev_info(host->dev, "phase: [map:%llx] [maxlen:%d] [final:%d]\n",
		 delay, len_final, final_phase);

	delay_phase.maxlen = len_final;
	delay_phase.start = start_final;
	delay_phase.final_phase = final_phase;
	return delay_phase;
}

static inline void msdc_set_cmd_delay(struct msdc_host *host, u32 value)
{
	u32 pad_dly1, pad_dly2;
	u32 dly1_sel, dly2_sel;

	pad_dly1 = value < 32 ? value : 31;
	pad_dly2 = value < 32 ? 0 : (value - 32);

	dly1_sel = 1;
	dly2_sel = value < 32 ? 0 : 1;

	if (host->top_base) {
		sdr_set_field(host->top_base + EMMC_TOP_CMD, PAD_CMD_RXDLY, pad_dly1);
		sdr_set_field(host->top_base + EMMC_TOP_CMD, PAD_CMD_RXDLY2, pad_dly2);
		sdr_set_field(host->top_base + EMMC_TOP_CMD, PAD_CMD_RD_RXDLY_SEL, dly1_sel);
		sdr_set_field(host->top_base + EMMC_TOP_CMD, PAD_CMD_RD_RXDLY2_SEL, dly2_sel);
	} else {
		sdr_set_field(host->base + MSDC_PAD_TUNE0, MSDC_PAD_TUNE_CMDRDLY, pad_dly1);
		sdr_set_field(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE_CMDRDLY, pad_dly2);
		sdr_set_field(host->base + MSDC_PAD_TUNE0, MSDC_PAD_TUNE_CMD_SEL, dly1_sel);
		sdr_set_field(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE_CMD_SEL, dly2_sel);
	}
}

static inline void msdc_set_data_delay(struct msdc_host *host, u32 value)
{
	u32 pad_dly1, pad_dly2;
	u32 dly1_sel, dly2_sel;

	pad_dly1 = value < 32 ? value : 31;
	pad_dly2 = value < 32 ? 0 : (value - 32);

	dly1_sel = 1;
	dly2_sel = value < 32 ? 0 : 1;

	if (host->top_base) {
		sdr_set_field(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY, pad_dly1);
		sdr_set_field(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2, pad_dly2);
		sdr_set_field(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL, dly1_sel);
		sdr_set_field(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2_SEL, dly2_sel);
	} else {
		sdr_set_field(host->base + MSDC_PAD_TUNE0, MSDC_PAD_TUNE_DATRRDLY, pad_dly1);
		sdr_set_field(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE_DATRRDLY, pad_dly2);
		sdr_set_field(host->base + MSDC_PAD_TUNE0, MSDC_PAD_TUNE_RD_SEL, dly1_sel);
		sdr_set_field(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE_RD_SEL, dly2_sel);
	}
}

static int autok_simple_score64(char *res_str64, u64 result64)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result64 == 0xFFFFFFFFFFFFFFFF) {
		strncpy(res_str64,
	"OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO",
			65);
		return 64;
	}
	if (result64 == 0) {
		/* maybe result is 0 */
		strncpy(res_str64,
	"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
			65);
		return 0;
	}

	/* calc continue zero number */
	while (bit < 64) {
		if (!(result64 & ((u64) (1LL << bit)))) {
			res_str64[bit] = 'X';
			bit++;
			if (old < num)
				old = num;
			num = 0;
			continue;
		}
		res_str64[bit] = 'O';
		bit++;
		num++;
	}
	if (num > old)
		old = num;

	res_str64[64] = '\0';
	return old;
}

static int msdc_tune_response(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u64 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0,};
	struct msdc_delay_phase internal_delay_phase;
	u8 final_delay, final_maxlen;
	u32 internal_delay = 0;
	u32 tune_reg = host->dev_comp->pad_tune_reg;
	int cmd_err = 0;
	int i, j;

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
	    mmc->ios.timing == MMC_TIMING_UHS_SDR104)
		sdr_set_field(host->base + tune_reg,
			      MSDC_PAD_TUNE_CMDRRDLY,
			      host->hs200_cmd_int_delay);

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	for (i = 0 ; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		/*
		 * Using the same parameters, it may sometimes pass the test,
		 * but sometimes it may fail. To make sure the parameters are
		 * more stable, we test each set of parameters 3 times.
		 */
		for (j = 0; j < 3; j++) {
			//mmc_send_tuning(mmc, opcode, &cmd_err);
			if (opcode != MMC_SEND_STATUS)
				mmc_send_tuning(mmc, opcode, &cmd_err);
			if (!cmd_err) {
				rise_delay |= (1ULL << i);
			} else {
				rise_delay &= ~(1ULL << i);
				break;
			}
		}
	}
	final_rise_delay = get_best_delay(host, rise_delay, PAD_DELAY_2CELL_MAX, 0);
	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	for (i = 0; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		/*
		 * Using the same parameters, it may sometimes pass the test,
		 * but sometimes it may fail. To make sure the parameters are
		 * more stable, we test each set of parameters 3 times.
		 */
		for (j = 0; j < 3; j++) {
			mmc_send_tuning(mmc, opcode, &cmd_err);
			if (!cmd_err) {
				fall_delay |= (1ULL << i);
			} else {
				fall_delay &= ~(1ULL << i);
				break;
			}
		}
	}
	final_fall_delay = get_best_delay(host, fall_delay, PAD_DELAY_2CELL_MAX, 0);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_fall_delay.maxlen >= 12 && final_fall_delay.start < 4)
		final_maxlen = final_fall_delay.maxlen;
	if (final_maxlen == final_rise_delay.maxlen) {
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		final_delay = final_rise_delay.final_phase;
	} else {
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		final_delay = final_fall_delay.final_phase;
	}
	msdc_set_cmd_delay(host, final_delay);

	if (host->dev_comp->async_fifo || host->hs200_cmd_int_delay)
		goto skip_internal;

	for (i = 0; i < PAD_DELAY_1CELL_MAX; i++) {
		sdr_set_field(host->base + tune_reg,
			      MSDC_PAD_TUNE_CMDRRDLY, i);
		mmc_send_tuning(mmc, opcode, &cmd_err);
		if (!cmd_err)
			internal_delay |= (1U << i);
	}
	dev_dbg(host->dev, "Final internal delay: 0x%x\n", internal_delay);
	internal_delay_phase = get_best_delay(host, (u64)internal_delay, PAD_DELAY_1CELL_MAX, 0);
	sdr_set_field(host->base + tune_reg, MSDC_PAD_TUNE_CMDRRDLY,
		      internal_delay_phase.final_phase);
skip_internal:
	dev_dbg(host->dev, "Final cmd pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

static int hs400_tune_response(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 cmd_delay = 0;
	struct msdc_delay_phase final_cmd_delay = { 0,};
	u8 final_delay;
	int cmd_err = 0;
	int i, j;

	/* select EMMC50 PAD CMD tune */
	sdr_set_bits(host->base + PAD_CMD_TUNE, BIT(0));
	sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMDTA, 2);

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
	    mmc->ios.timing == MMC_TIMING_UHS_SDR104)
		sdr_set_field(host->base + MSDC_PAD_TUNE,
			      MSDC_PAD_TUNE_CMDRRDLY,
			      host->hs200_cmd_int_delay);

	if (host->hs400_cmd_resp_sel_rising)
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	else
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	for (i = 0 ; i < PAD_DELAY_1CELL_MAX; i++) {
		sdr_set_field(host->base + PAD_CMD_TUNE,
			      PAD_CMD_TUNE_RX_DLY3, i);
		/*
		 * Using the same parameters, it may sometimes pass the test,
		 * but sometimes it may fail. To make sure the parameters are
		 * more stable, we test each set of parameters 3 times.
		 */
		for (j = 0; j < 3; j++) {
			mmc_send_tuning(mmc, opcode, &cmd_err);
			if (!cmd_err) {
				cmd_delay |= (1U << i);
			} else {
				cmd_delay &= ~(1U << i);
				break;
			}
		}
	}
	final_cmd_delay = get_best_delay(host, (u64)cmd_delay, PAD_DELAY_1CELL_MAX, 0);
	sdr_set_field(host->base + PAD_CMD_TUNE, PAD_CMD_TUNE_RX_DLY3,
		      final_cmd_delay.final_phase);
	final_delay = final_cmd_delay.final_phase;

	dev_dbg(host->dev, "Final cmd pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

static int msdc_tune_data(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u64 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0,};
	u8 final_delay, final_maxlen;
	int i, ret;

	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_INT_DAT_LATCH_CK_SEL,
		      host->latch_ck);
	if (!support_new_rx(host->dev_comp->new_rx_ver) &&
		(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
		 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	else
		sdr_clr_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

	if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
		sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
	else
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

	for (i = 0 ; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			rise_delay |= (1ULL << i);
	}
	final_rise_delay = get_best_delay(host, rise_delay, PAD_DELAY_2CELL_MAX, 0);
	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	if (!support_new_rx(host->dev_comp->new_rx_ver) &&
		(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
		 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	else
		sdr_set_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

	if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
		sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
	else
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

	for (i = 0; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			fall_delay |= (1ULL << i);
	}
	final_fall_delay = get_best_delay(host, fall_delay, PAD_DELAY_2CELL_MAX, 0);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_maxlen == final_rise_delay.maxlen) {
		if (!support_new_rx(host->dev_comp->new_rx_ver) &&
			(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
			 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
			sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
		else
			sdr_clr_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

		if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
			sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
		else
			sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

		final_delay = final_rise_delay.final_phase;
	} else {
		if (!support_new_rx(host->dev_comp->new_rx_ver) &&
			(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
			 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
			sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
		else
			sdr_set_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

		if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
			sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
		else
			sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

		final_delay = final_fall_delay.final_phase;
	}
	msdc_set_data_delay(host, final_delay);

	dev_dbg(host->dev, "Final data pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

/*
 * MSDC IP which supports data tune + async fifo can do CMD/DAT tune
 * together, which can save the tuning time.
 */
static int msdc_tune_together(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u64 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0,};
	u8 final_delay, final_maxlen;
	int i, ret;
	char tune_result_str64[65];
	unsigned int score = 0;

	dev_info(host->dev, "[%s]opcode=%d,clock rising edge", __func__, opcode);

	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_INT_DAT_LATCH_CK_SEL,
		      host->latch_ck);

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	if (!support_new_rx(host->dev_comp->new_rx_ver) &&
		(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
		 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	else
		sdr_clr_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

	if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
		sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
	else
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

	for (i = 0 ; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			rise_delay |= (1ULL << i);
	}
	score = autok_simple_score64(tune_result_str64, rise_delay);
	dev_info(host->dev, "[%s]rising %s 0x%llx %d", __func__, tune_result_str64, rise_delay, score);
	final_rise_delay = get_best_delay(host, rise_delay, PAD_DELAY_2CELL_MAX, 0);
	dev_info(host->dev, "[%s]final_rise_delay.start=%d,maxlen=%d,final_phase=%d", __func__,
		final_rise_delay.start, final_rise_delay.maxlen, final_rise_delay.final_phase);

	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	dev_info(host->dev, "[%s]opcode=%d,clock falling edge", __func__, opcode);
	sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	if (!support_new_rx(host->dev_comp->new_rx_ver) &&
		(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
		 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	else
		sdr_set_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

	if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
		sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
	else
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

	for (i = 0; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			fall_delay |= (1ULL << i);
	}
	score = autok_simple_score64(tune_result_str64, fall_delay);
	dev_info(host->dev, "[%s]falling %s 0x%llx %d", __func__, tune_result_str64, fall_delay, score);
	final_fall_delay = get_best_delay(host, fall_delay, PAD_DELAY_2CELL_MAX, 0);
	dev_info(host->dev, "[%s]final_fall_delay.start=%d,maxlen=%d,final_phase=%d", __func__,
		final_fall_delay.start, final_fall_delay.maxlen, final_fall_delay.final_phase);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_maxlen == final_rise_delay.maxlen) {
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		if (!support_new_rx(host->dev_comp->new_rx_ver) &&
			(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
			 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
			sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
		else
			sdr_clr_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

		if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
			sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
		else
			sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

		final_delay = final_rise_delay.final_phase;
		dev_info(host->dev, "[%s]clock rising edge, delay:%d", __func__, final_delay);
	} else {
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		if (!support_new_rx(host->dev_comp->new_rx_ver) &&
			(mmc->ios.timing == MMC_TIMING_UHS_DDR50 ||
			 mmc->ios.timing == MMC_TIMING_MMC_DDR52))
			sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
		else
			sdr_set_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);

		if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
			sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
		else
			sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

		final_delay = final_fall_delay.final_phase;
		dev_info(host->dev, "[%s]clock falling edge, delay:%d", __func__, final_delay);
	}

	msdc_set_cmd_delay(host, final_delay);
	msdc_set_data_delay(host, final_delay);

	dev_info(host->dev, "[%s]Final pad delay: %d\n", __func__, final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

static int msdc_tune_ddr50(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	int cmd_err = 0;
	u64 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay = { 0, }, final_fall_delay = { 0, };
	int ret, i;
	u8 final_delay, final_maxlen;

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	if (support_new_rx(host->dev_comp->new_rx_ver))
		sdr_clr_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);
	else
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);

	if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
		sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
	else
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

	if (host->top_base) {
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2_SEL);
	} else {
		sdr_clr_bits(host->base + MSDC_PAD_TUNE0, MSDC_PAD_TUNE_DATRRDLY);
		sdr_clr_bits(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE_DATRRDLY);
		sdr_clr_bits(host->base + MSDC_PAD_TUNE0, MSDC_PAD_TUNE_RD_SEL);
		sdr_clr_bits(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE_RD_SEL);
	}

	mmc_send_tuning(mmc, opcode, &cmd_err);
	if (cmd_err)
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);

	for (i = 0; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			rise_delay |= (1ULL << i);
	}
	final_rise_delay = get_best_delay(host, rise_delay, PAD_DELAY_2CELL_MAX, 1);
	/* if rising edge has full margin, then do not scan falling edge */
	if (final_rise_delay.maxlen == PAD_DELAY_2CELL_MAX)
		goto skip_fall;

	if (support_new_rx(host->dev_comp->new_rx_ver))
		sdr_set_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);
	else
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);

	if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
		sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
	else
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

	for (i = 0; i < PAD_DELAY_2CELL_MAX; i++) {
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			fall_delay |= (1ULL << i);
	}
	final_fall_delay = get_best_delay(host, fall_delay, PAD_DELAY_2CELL_MAX, 1);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_maxlen == final_rise_delay.maxlen) {
		if (support_new_rx(host->dev_comp->new_rx_ver))
			sdr_clr_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);
		else
			sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);

		if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
			sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
		else
			sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

		final_delay = final_rise_delay.final_phase;
	} else {
		if (support_new_rx(host->dev_comp->new_rx_ver))
			sdr_set_bits(host->base + MSDC_PATCH_BIT, MSDC_PATCH_BIT_RD_DAT_SEL);
		else
			sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);

		if (host->dev_comp->async_fifo || support_new_rx(host->dev_comp->new_rx_ver))
			sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE);
		else
			sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);

		final_delay = final_fall_delay.final_phase;
	}

	msdc_set_data_delay(host, final_delay);

	dev_dbg(host->dev, "Final pad delay: %d\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}


/*--------------------------------------------------------------------------*/
/* SDCard error handler                                                     */
/*--------------------------------------------------------------------------*/
/* if continuous power cycle fail reach the limit */
/* driver will force remove card */
#define MSDC_MAX_POWER_CYCLE_FAIL_CONTINUOUS (3)

/* count of bad sd detecter (or bad sd condition kinds),
 * we can add it here if has other condition
 */
#define BAD_SD_DETECTER_COUNT 1

/* we take it as bad sd when the bad sd condition occurs
 * out of tolerance
 */
u32 bad_sd_tolerance[BAD_SD_DETECTER_COUNT] = {10};

/* bad sd condition occur times
 */
u32 bad_sd_detecter[BAD_SD_DETECTER_COUNT] = {0};

/* bad sd condition occur times will reset to zero by self
 * when reach the forget time (when set to 0, means not
 * reset to 0 by self), unit:s
 */
u32 bad_sd_forget[BAD_SD_DETECTER_COUNT] = {3};

/* the latest occur time of the bad sd condition,
 * unit: clock
 */
unsigned long bad_sd_timer[BAD_SD_DETECTER_COUNT] = {0};

static void msdc_reset_bad_sd_detecter(struct msdc_host *host)
{
	u32 i = 0;

	if (host == NULL) {
		pr_notice("WARN: host is NULL at %s\n", __func__);
		return;
	}

	host->block_bad_card = 0;
	for (i = 0; i < BAD_SD_DETECTER_COUNT; i++)
		bad_sd_detecter[i] = 0;
}

static void msdc_ops_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct msdc_host *host = mmc_priv(mmc);

	/* when the card is inited or re-inited, the flag should be reset to retune hs200 */
	host->is_skip_hs200_tune = 0;
}

void msdc_set_bad_card_and_remove(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);

	if (host->card_inserted) {
		host->block_bad_card = 1;
		host->card_inserted = 0;
	}

	if ((mmc == NULL) || (mmc->card == NULL)) {
		pr_info("WARN: mmc or card is NULL");
		return;
	}

	if (mmc->card) {
		mmc_card_set_removed(mmc->card);
		pr_info("schedule mmc_rescan");
		mmc_detect_change(mmc, msecs_to_jiffies(200));
		if (host->block_bad_card)
			pr_info("remove the bad card, block_bad_card=%d, card_inserted=%d",
				host->block_bad_card, host->card_inserted);
	}
}

static int msdc_get_cd(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	int val;

	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		host->card_inserted = 1;
		goto end;
	} else if (!host->internal_cd) {
		host->card_inserted = mmc_gpio_get_cd(mmc);
	} else {
		val = readl(host->base + MSDC_PS) & MSDC_PS_CDSTS;
		if (mmc->caps2 & MMC_CAP2_CD_ACTIVE_HIGH)
			host->card_inserted = !!val;
		else
			host->card_inserted = !val;
	}

	if (host->block_bad_card)
		host->card_inserted = 0;
end:
	if (host->block_bad_card != 0 ||
		mmc->trigger_card_event != false) {
		if (host->card_inserted == 0 && host->id == MSDC_SD) {
			rw_times = 0;
			ra_fixed = false;
		}
		pr_info(
			"%s:card status:%s block bad card<%d> trigger card event<%d>",
			__func__, host->card_inserted ? "inserted" : "removed",
			host->block_bad_card, mmc->trigger_card_event);
	}

	return host->card_inserted;
}

//int sdcard_hw_reset(struct mmc_host *mmc)
int sdcard_hw_reset(struct mmc_card *card)
{
	struct mmc_host *mmc = card->host;
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;

	host->card_inserted = msdc_get_cd(mmc);

	if (!(host->card_inserted)) {
		pr_notice("card is not inserted!\n");
		msdc_set_bad_card_and_remove(host);
		ret = -1;
		return ret;
	}

	ret = mmc_hw_reset(card);
	if (ret) {
		if (++host->power_cycle_cnt
			> MSDC_MAX_POWER_CYCLE_FAIL_CONTINUOUS)
			msdc_set_bad_card_and_remove(host);
		dev_info(host->dev,
			"power reset (%d) failed, block_bad_card = %d\n",
			host->power_cycle_cnt, host->block_bad_card);
	} else {
		host->power_cycle_cnt = 0;
		dev_info(host->dev, "msdc power reset success\n");
	}

	return ret;
}

int msdc_data_timeout_cont_chk(struct msdc_host *host)
{
	if ((host->id == MSDC_SD) &&
		(host->data_timeout_cont >= MSDC_MAX_DATA_TIMEOUT_CONTINUOUS)) {
		pr_info("force remove bad card, data timeout continuous %d",
			host->data_timeout_cont);
		msdc_set_bad_card_and_remove(host);
		return 1;
	}

	return 0;
}

/* SDcard will change speed mode and power reset
 * UHS card
 *    UHS_SDR104 --> UHS_DDR50 --> UHS_SDR50 --> UHS_SDR25
 * HS card
 *    50MHz --> 25MHz --> 12.5MHz --> 6.25MHz
 */
int sdcard_reset_tuning(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	char *remove_cap;
	int ret = 0;

	if (!mmc->card) {
		pr_info("mmc card = NULL, skip reset tuning\n");
		return -1;
	}

	if (mmc_card_uhs(mmc->card)) {
		if (mmc->card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR104) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR104;
			remove_cap = "UHS_SDR104";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_DDR50) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_DDR50;
			remove_cap = "UHS_DDR50";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_SDR50) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR50;
			remove_cap = "UHS_SDR50";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_SDR25) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR25;
			remove_cap = "UHS_SDR25";
		} else {
			remove_cap = "none";
		}
		dev_info(host->dev, "remove %s mode then reinit card\n", remove_cap);
	} else if (mmc_card_hs(mmc->card)) {
		if (mmc->card->sw_caps.hs_max_dtr >= HIGH_SPEED_MAX_DTR / 4)
			mmc->card->sw_caps.hs_max_dtr /= 2;
		dev_info(host->dev, "set hs speed %dhz then reinit card\n",
			mmc->card->sw_caps.hs_max_dtr);
	} else {
		dev_info(host->dev, "ds card just reinit card\n");
	}

	/* force remove card for continuous data timeout */
	ret = msdc_data_timeout_cont_chk(host);
	if (ret) {
		ret = -1;
		goto done;
	}

	/* power cycle sdcard */
	ret = sdcard_hw_reset(mmc->card);
	if (ret) {
		ret = -1;
		goto done;
	}

done:
	return ret;
}

static int msdc_detect_bad_sd(struct msdc_host *host, u32 condition)
{
	unsigned long time_current = jiffies;
	int ret = 0;

	if (host == NULL) {
		pr_notice("WARN: host is NULL at %s\n", __func__);
		ret = -1;
		goto end;
	}

	if (condition >= BAD_SD_DETECTER_COUNT) {
		pr_notice("msdc1: BAD_SD_DETECTER_COUNT is %d, need check it's definition at %s\n",
			BAD_SD_DETECTER_COUNT, __func__);
		ret = -1;
		goto end;
	}

	if (bad_sd_forget[condition]
		&& time_after(time_current,
		(bad_sd_timer[condition] + bad_sd_forget[condition] * HZ)))
		bad_sd_detecter[condition] = 0;
	bad_sd_timer[condition] = time_current;

	if (++(bad_sd_detecter[condition]) >= bad_sd_tolerance[condition]) {
		msdc_set_bad_card_and_remove(host);
		ret = -1;
	}
	pr_notice("%s:bad_sd_detecter:%d\n", __func__, bad_sd_detecter[condition]);

end:
	return ret;
}

static int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	dev_info(host->dev,"[%s]start,opcode=%d,vcore=%d",
		__func__, opcode,
		host->dvfsrc_vcore_power ?
		regulator_get_voltage(host->dvfsrc_vcore_power) : -1);

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200) {
		if (host->is_skip_hs200_tune) {
			pr_notice("[AUTOK]eMMC Skip HS200 Tune\n");
			goto end;
		}
	}

	if ((!(mmc->caps2 & MMC_CAP2_NO_SD)) && host->need_tune) {
		ret = msdc_detect_bad_sd(host, 0);
		if (ret)
			goto end;
		dev_info(host->dev, "%s error retune %d times\n",
			__func__, ++host->retune_times);
	}

	if (mmc->ios.timing == MMC_TIMING_UHS_DDR50) {
		ret = msdc_tune_ddr50(mmc, opcode);
		goto tune_done;
	}

	if (host->dev_comp->data_tune && host->dev_comp->async_fifo
		&& opcode != MMC_SEND_STATUS) {
		ret = msdc_tune_together(mmc, opcode);
		if (host->hs400_mode) {
			sdr_clr_bits(host->base + MSDC_IOCON,
				     MSDC_IOCON_DSPL | MSDC_IOCON_W_DSPL);
			/* hs400 for new rx, need clr read data edge bit */
			if (support_new_rx(host->dev_comp->new_rx_ver))
				sdr_clr_bits(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL);
			msdc_set_data_delay(host, 0);
		}
		goto tune_done;
	}
	if (host->hs400_mode &&
	    host->dev_comp->hs400_tune)
		ret = hs400_tune_response(mmc, opcode);
	else
		ret = msdc_tune_response(mmc, opcode);
	if (ret == -EIO) {
		dev_err(host->dev, "Tune response fail!\n");
		return ret;
	}
	if (host->hs400_mode == false) {
		ret = msdc_tune_data(mmc, opcode);
		if (ret == -EIO)
			dev_err(host->dev, "Tune data fail!\n");
	}

tune_done:
	host->saved_tune_para.iocon = readl(host->base + MSDC_IOCON);
	host->saved_tune_para.pad_tune = readl(host->base + tune_reg);
	host->saved_tune_para.pad_cmd_tune = readl(host->base + PAD_CMD_TUNE);
	if (host->top_base) {
		host->saved_tune_para.emmc_top_control = readl(host->top_base +
				EMMC_TOP_CONTROL);
		host->saved_tune_para.emmc_top_cmd = readl(host->top_base +
				EMMC_TOP_CMD);
	}

end:
	if (host->retune_times >= 4 && !host->block_bad_card) {
		if (!(mmc->caps2 & MMC_CAP2_NO_SD))
			sdcard_reset_tuning(mmc);
	} else if (!ret) {
		host->autok_vcore = host->dvfsrc_vcore_power ?
			regulator_get_voltage(host->dvfsrc_vcore_power) : -1;
		dev_info(host->dev,"[%s]msdc tune pass,opcode=%d,vcore=%d",
			__func__, opcode, host->autok_vcore);
		host->need_tune = false;
	}

	return ret;
}

static int msdc_prepare_hs400_tuning(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	host->hs400_mode = true;

	if (host->top_base)
		writel(host->hs400_ds_delay,
		       host->top_base + EMMC50_PAD_DS_TUNE);
	else
		writel(host->hs400_ds_delay, host->base + PAD_DS_TUNE);
	/* hs400 mode must set it to 0 */
	sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_CFGCRCSTS);
	/* to improve read performance, set outstanding to 2 */
	sdr_set_field(host->base + EMMC50_CFG3, EMMC50_CFG3_OUTS_WR, 2);

	return 0;
}

static int msdc_execute_hs400_tuning(struct mmc_host *mmc, struct mmc_card *card)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_delay_phase dly1_delay;
	u32 val, result_dly1 = 0;
	u8 *ext_csd;
	int i, ret;
	char tune_result_str64[65];
	unsigned int score = 0;

	dev_info(host->dev,"[%s]start,opcode=%d,vcore=%d",
		__func__, MMC_SEND_EXT_CSD,
		host->dvfsrc_vcore_power ?
		regulator_get_voltage(host->dvfsrc_vcore_power) : -1);

	if (host->top_base) {
		sdr_set_bits(host->top_base + EMMC50_PAD_DS_TUNE,
			     PAD_DS_DLY_SEL);// 1:DS pass through
		if (host->hs400_ds_dly3)
			sdr_set_field(host->top_base + EMMC50_PAD_DS_TUNE,
				      PAD_DS_DLY3, host->hs400_ds_dly3);

	} else {
		sdr_set_bits(host->base + PAD_DS_TUNE, PAD_DS_TUNE_DLY_SEL);
		if (host->hs400_ds_dly3)
			sdr_set_field(host->base + PAD_DS_TUNE,
				      PAD_DS_TUNE_DLY3, host->hs400_ds_dly3);
	}

	host->hs400_tuning = true;
	for (i = 0; i < PAD_DELAY_MAX; i++) {
		if (host->top_base)
			sdr_set_field(host->top_base + EMMC50_PAD_DS_TUNE,
				      PAD_DS_DLY1, i);
		else
			sdr_set_field(host->base + PAD_DS_TUNE,
				      PAD_DS_TUNE_DLY1, i);

		ret = mmc_get_ext_csd(card, &ext_csd);
		if (!ret) {
			result_dly1 |= (1 << i);
			kfree(ext_csd);
		}
	}
	host->hs400_tuning = false;
	score = autok_simple_score64(tune_result_str64, result_dly1);
	dev_info(host->dev, "[%s]%s 0x%x %d", __func__, tune_result_str64, result_dly1, score);

	dly1_delay = get_best_delay(host, result_dly1, PAD_DELAY_MAX, 0);
	dev_info(host->dev, "[%s]dly1_delay.start=%d,maxlen=%d,final_phase=%d\n", __func__,
		dly1_delay.start, dly1_delay.maxlen, dly1_delay.final_phase);
	if (dly1_delay.maxlen == 0) {
		dev_err(host->dev, "Failed to get DLY1 delay!\n");
		goto fail;
	}
	if (host->top_base)
		sdr_set_field(host->top_base + EMMC50_PAD_DS_TUNE,
			      PAD_DS_DLY1, dly1_delay.final_phase);
	else
		sdr_set_field(host->base + PAD_DS_TUNE,
			      PAD_DS_TUNE_DLY1, dly1_delay.final_phase);

	if (host->top_base)
		val = readl(host->top_base + EMMC50_PAD_DS_TUNE);
	else
		val = readl(host->base + PAD_DS_TUNE);

	dev_info(host->dev, "Fianl PAD_DS_TUNE: 0x%x\n", val);
	host->autok_vcore = host->dvfsrc_vcore_power ?
		regulator_get_voltage(host->dvfsrc_vcore_power) : -1;
	dev_info(host->dev,"[%s]msdc tune pass,opcode=%d,vcore=%d",
		__func__, MMC_SEND_EXT_CSD, host->autok_vcore);
	host->is_skip_hs200_tune = 1;
	return 0;

fail:
	dev_err(host->dev, "Failed to tuning DS pin delay!\n");
	return -EIO;
}

static void msdc_hw_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	dev_info(mmc_dev(mmc), "hw reset device\n");
	sdr_set_bits(host->base + EMMC_IOCON, 1);
	mdelay(10);
	sdr_clr_bits(host->base + EMMC_IOCON, 1);
}

static void msdc_ack_sdio_irq(struct mmc_host *mmc)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	spin_lock_irqsave(&host->lock, flags);
	__msdc_enable_sdio_irq(host, 1);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void msdc_ops_card_event(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->power_cycle_cnt = 0;
	host->data_timeout_cont = 0;
	host->is_skip_hs200_tune = 0;
	msdc_reset_bad_sd_detecter(host);
	msdc_get_cd(mmc);

#if IS_ENABLED(CONFIG_SEC_MMC_FEATURE)
	sd_sec_card_event(mmc);
#endif
}

static void msdc_hs400_enhanced_strobe(struct mmc_host *mmc,
				       struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (ios->enhanced_strobe) {
		msdc_prepare_hs400_tuning(mmc, ios);
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_PADCMD_LATCHCK, 1);
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_CMD_RESP_SEL, 1);
		sdr_set_field(host->base + EMMC50_CFG1, EMMC50_CFG1_DS_CFG, 1);

		sdr_clr_bits(host->base + CQHCI_SETTING, CQHCI_RD_CMD_WND_SEL);
		sdr_clr_bits(host->base + CQHCI_SETTING, CQHCI_WR_CMD_WND_SEL);
		sdr_clr_bits(host->base + EMMC51_CFG0, CMDQ_RDAT_CNT);
	} else {
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_PADCMD_LATCHCK, 0);
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_CMD_RESP_SEL, 0);
		sdr_set_field(host->base + EMMC50_CFG1, EMMC50_CFG1_DS_CFG, 0);

		sdr_set_bits(host->base + CQHCI_SETTING, CQHCI_RD_CMD_WND_SEL);
		sdr_set_bits(host->base + CQHCI_SETTING, CQHCI_WR_CMD_WND_SEL);
		sdr_set_field(host->base + EMMC51_CFG0, CMDQ_RDAT_CNT, 0xb4);
	}
}

/* SiP commands */
#define MTK_SIP_MMC_CONTROL	MTK_SIP_SMC_CMD(0x273)
#define MMC_MTK_ATF_VERSION	1
#define MMC_MTK_TFA_VERSION	2
#define MMC_MTK_SIP_CRYPTO_CTRL	BIT(1)
#define MMC_MTK_SIP_INFRA_REQ	BIT(2)
#define MMC_MTK_SIP_INFRA_REQ_RELEASE	BIT(3)
#define MMC_MTK_ATF_SIP_CRYPTO_CTRL	BIT(0)
#define MMC_MTK_TFA_SIP_CRYPTO_CTRL	BIT(1)

/* SMC call wapper function */
#define mmc_mtk_crypto_ctrl(smcc_res) \
	arm_smccc_smc(MTK_SIP_MMC_CONTROL, \
		MMC_MTK_SIP_CRYPTO_CTRL, 0, 0, 0, 0, 0, 0, &smcc_res)

#define mmc_mtk_infra_req(smcc_res, id) \
	arm_smccc_smc(MTK_SIP_MMC_CONTROL, \
		MMC_MTK_SIP_INFRA_REQ, id, 0, 0, 0, 0, 0, &smcc_res)

#define mmc_mtk_infra_req_release(smcc_res, id) \
	arm_smccc_smc(MTK_SIP_MMC_CONTROL, \
		MMC_MTK_SIP_INFRA_REQ_RELEASE, id, 0, 0, 0, 0, 0, &smcc_res)

#define mmc_mtk_crypto_ctrl_atf(smcc_res) \
	arm_smccc_smc(MTK_SIP_MMC_CONTROL, \
		MMC_MTK_ATF_SIP_CRYPTO_CTRL, 4, 1, 0, 0, 0, 0, &smcc_res)

#define mmc_mtk_crypto_ctrl_tfa(smcc_res) \
	arm_smccc_smc(MTK_SIP_MMC_CONTROL, \
		MMC_MTK_TFA_SIP_CRYPTO_CTRL, 0, 0, 0, 0, 0, 0, &smcc_res)

static void mmc_mtk_crypto_enable(struct mmc_host *mmc)
{
	struct arm_smccc_res res = {0};
	struct msdc_host *host = mmc_priv(mmc);

	if (host->tf_ver == MMC_MTK_ATF_VERSION)
		mmc_mtk_crypto_ctrl_atf(res);
	else if (host->tf_ver == MMC_MTK_TFA_VERSION)
		mmc_mtk_crypto_ctrl_tfa(res);
	else
		dev_info(mmc_dev(mmc), "tf version[%d] is not supported\n", host->tf_ver);

	if (res.a0) {
		dev_info(mmc_dev(mmc), "crypto enable failed, err: %lu\n", res.a0);
		mmc->caps2 &= ~MMC_CAP2_CRYPTO;
	}
}

static void msdc_enable_rst_n_func(struct mmc_host *mmc, struct mmc_card *card)
{
	int ret = 0;

	if(!mmc || !card)
		return;

	if ((mmc->caps & MMC_CAP_HW_RESET) && !card->ext_csd.rst_n_function) {
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_RST_N_FUNCTION, 1, 1000);
		if (!ret){
			card->ext_csd.rst_n_function = 1;
			dev_info(mmc_dev(mmc), "%s: set ext_csd.rst_n_function = 1 with success: %d\n",
				__func__, ret);
			}
		else {
			mmc->caps &= ~MMC_CAP_HW_RESET;
			dev_info(mmc_dev(mmc), "%s: set ext_csd.rst_n_function = 1 with fail: %d\n",
				__func__, ret);
		}
	}
}

static void msdc_cqe_enable(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct cqhci_host *cq_host = mmc->cqe_private;

	if (host->dev_comp->set_crypto_enable_in_sw)
		mmc_mtk_crypto_enable(mmc);

	/* enable cmdq irq */
	writel(MSDC_INT_CMDQ, host->base + MSDC_INTEN);
	/* enable busy check */
	sdr_set_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
	/* default write data / busy timeout 20s */
	msdc_set_busy_timeout(host, 20 * 1000000000ULL, 0);
	/* default read data timeout 1s */
	msdc_set_timeout(host, 1000000000ULL, 0);
	cqhci_writel(cq_host, 0x40, CQHCI_SSC1);
	msdc_enable_rst_n_func(mmc, mmc->card);
}

static void msdc_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	struct msdc_host *host = mmc_priv(mmc);
	unsigned int val = 0;

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
	/* disable cmdq irq */
	sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INT_CMDQ);
	/* disable busy check */
	sdr_clr_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);

	if (recovery)
		msdc_reset_hw(host);

}

static void msdc_cqe_pre_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg |= CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static void msdc_cqe_post_disable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg &= ~CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static const struct mmc_host_ops mt_msdc_ops = {
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.get_ro = mmc_gpio_get_ro,
	.get_cd = msdc_get_cd,
	.hs400_enhanced_strobe = msdc_hs400_enhanced_strobe,
	.enable_sdio_irq = msdc_enable_sdio_irq,
	.ack_sdio_irq = msdc_ack_sdio_irq,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
	.card_busy = msdc_card_busy,
	.execute_tuning = msdc_execute_tuning,
	.prepare_hs400_tuning = msdc_prepare_hs400_tuning,
	.execute_hs400_tuning = msdc_execute_hs400_tuning,
	.card_hw_reset = msdc_hw_reset,
	.card_event = msdc_ops_card_event,
	.init_card = msdc_ops_init_card,
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_CQHCI_DEBUG)
extern void mt_irq_dump_status(unsigned int irq);
void msdc_dumpregs(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	msdc_dump_info(NULL, 0, NULL, host);
	mt_irq_dump_status(host->irq);
}
#endif

static const struct cqhci_host_ops msdc_cmdq_ops = {
	.enable         = msdc_cqe_enable,
	.disable        = msdc_cqe_disable,
	.pre_enable = msdc_cqe_pre_enable,
	.post_disable = msdc_cqe_post_disable,
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_CQHCI_DEBUG)
	.dumpregs = msdc_dumpregs,
#endif
};

static irqreturn_t sdio_eint_irq(int irq, void *dev_id)
{
	unsigned long flags;
	struct msdc_host *host = (struct msdc_host *)dev_id;
	struct mmc_host *mmc = mmc_from_priv(host);

	spin_lock_irqsave(&host->lock, flags);
	if (likely(host->sdio_irq_cnt > 0)) {
		disable_irq_nosync(host->eint_irq);
		disable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt--;
	}
	spin_unlock_irqrestore(&host->lock, flags);

	sdio_signal_irq(mmc);

	return IRQ_HANDLED;
}

static int request_sdio_eint_irq(struct msdc_host *host)
{
	struct gpio_desc *desc;
	int ret = 0;
	int irq;

	desc = devm_gpiod_get_index(host->dev, "eint", 0, GPIOD_IN);
	if (IS_ERR(desc)) {
		pr_info("mmc%d:failed to get sdio eint gpiod\n", host->id);
		return PTR_ERR(desc);
	}

	irq = gpiod_to_irq(desc);
	if (irq >= 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(host->dev, irq,
				NULL, sdio_eint_irq,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"sdio-eint", host);
	} else
		ret = irq;

	host->eint_irq = irq;
	return ret;
}

static int cd_gpio = 0;//dunweichao.wt,2024.8.24,add proc file for sdcard slot detect

static void msdc_of_property_parse(struct platform_device *pdev,
				   struct msdc_host *host)
{
	const char *tf_ver = NULL;

	of_property_read_u32(pdev->dev.of_node, "mediatek,latch-ck",
			     &host->latch_ck);

	of_property_read_u32(pdev->dev.of_node, "hs400-ds-delay",
			     &host->hs400_ds_delay);

	of_property_read_u32(pdev->dev.of_node, "mediatek,hs400-ds-dly3",
			     &host->hs400_ds_dly3);

	of_property_read_u32(pdev->dev.of_node, "mediatek,hs200-cmd-int-delay",
			     &host->hs200_cmd_int_delay);

	of_property_read_u32(pdev->dev.of_node, "mediatek,hs400-cmd-int-delay",
			     &host->hs400_cmd_int_delay);

	of_property_read_u32(pdev->dev.of_node, "host-index", &host->id);

	cd_gpio = of_get_named_gpio(pdev->dev.of_node,"cd-gpios",0);//dunweichao.wt,2024.8.24,add proc file for sdcard slot detect

	host->dvfsrc_vcore_power =
		devm_regulator_get_optional(&pdev->dev, "dvfsrc-vcore");
	if (IS_ERR(host->dvfsrc_vcore_power)) {
		pr_info("mmc%d:failed to get dvfsrc-vcore:%ld\n",
			host->id, PTR_ERR(host->dvfsrc_vcore_power));
		host->dvfsrc_vcore_power = NULL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "req-vcore",
		&host->req_vcore)) {
		pr_info("mmc%d:failed to get req-vcore\n", host->id);
		host->req_vcore = 0;
	} else
		pr_info("mmc%d:req-vcore:%d\n", host->id, host->req_vcore);

	if (of_property_read_u32(pdev->dev.of_node, "ocr-voltage", &host->ocr_volt)) {
		pr_info("mmc%d:failed to get ocr_volt\n", host->id);
		host->ocr_volt = 0;
	} else
		pr_info("mmc%d:ocr-voltage:%d\n", host->id, host->ocr_volt);

	if (of_property_read_bool(pdev->dev.of_node,
				  "mediatek,hs400-cmd-resp-sel-rising"))
		host->hs400_cmd_resp_sel_rising = true;
	else
		host->hs400_cmd_resp_sel_rising = false;

	if (of_property_read_bool(pdev->dev.of_node, "supports-cqe"))
		host->cqhci = true;
	else
		host->cqhci = false;

	if (of_property_read_bool(pdev->dev.of_node, "sdcard-aggressive-pm"))
		host->sdcard_aggressive_pm = true;
	else
		host->sdcard_aggressive_pm = false;


	/* default value is TFA version, indicate tf-a is used */
	host->tf_ver = MMC_MTK_TFA_VERSION;
	if (!of_property_read_string(pdev->dev.of_node, "tf-ver", &tf_ver)) {
		if (!strncmp(tf_ver, "atf", 3))
			host->tf_ver = MMC_MTK_ATF_VERSION;
		else if (!strncmp(tf_ver, "tf-a", 4))
			host->tf_ver = MMC_MTK_TFA_VERSION;
		else
			pr_info("mmc%d: tf version[%s] is supported\n", host->id, tf_ver);
	}
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_DEBUG)
	msdc_gpio_of_parse(host);
#endif
}

static const void *mtk_get_boot_property(struct device_node *np,
	const char *name, int *lenp)
{
	struct device_node *boot_node = NULL;

	boot_node = of_parse_phandle(np, "bootmode", 0);
	if (!boot_node) {
		pr_info("boot_node is NULL\n");
		return NULL;
	}
	return of_get_property(boot_node, name, lenp);
}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static int msdc_of_clock_parse(struct platform_device *pdev,
			       struct msdc_host *host)
{
	int ret = 0;

	host->src_clk = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(host->src_clk))
		return PTR_ERR(host->src_clk);

	host->h_clk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(host->h_clk))
		return PTR_ERR(host->h_clk);

	host->bus_clk = devm_clk_get_optional(&pdev->dev, "bus_clk");
	if (IS_ERR(host->bus_clk))
		host->bus_clk = NULL;

	host->macro_clk = devm_clk_get_optional(&pdev->dev, "macro");
	if (IS_ERR(host->macro_clk))
		host->macro_clk = NULL;

	host->crypto_clk = devm_clk_get_optional(&pdev->dev, "crypto_clk");
	if (IS_ERR(host->crypto_clk)) {
		host->crypto_clk = NULL;
		dev_info(&pdev->dev, "Cannot get crypto clk\n");
	}

	/*source clock control gate is optional clock*/
	host->src_clk_cg = devm_clk_get_optional(&pdev->dev, "source_cg");
	if (IS_ERR(host->src_clk_cg))
		host->src_clk_cg = NULL;

	host->sys_clk_cg = devm_clk_get_optional(&pdev->dev, "sys_cg");
	if (IS_ERR(host->sys_clk_cg))
		host->sys_clk_cg = NULL;

	host->crypto_cg = devm_clk_get_optional(&pdev->dev, "crypto_cg");
	if (IS_ERR(host->crypto_cg))
		host->crypto_cg = NULL;

	/* If present, always enable for this clock gate */
	ret = clk_prepare_enable(host->sys_clk_cg);
	if (ret) {
		dev_info(&pdev->dev, "Cannot enable clock gate\n");
		return ret;
	}

	host->bulk_clks[0].id = "pclk_cg";
	host->bulk_clks[1].id = "axi_cg";
	host->bulk_clks[2].id = "ahb_cg";
	ret = devm_clk_bulk_get_optional(&pdev->dev, MSDC_NR_CLOCKS,
					 host->bulk_clks);
	if (ret) {
		dev_info(&pdev->dev, "Cannot get pclk/axi/ahb clock gates\n");
		return ret;
	}

	return 0;
}
#endif

void msdc_sd_power_off(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);

	if (mmc) {
		pr_notice("VMMC OC,Power Off SD card\n");
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}

		msdc_set_bad_card_and_remove(host);
	}
}

static int msdc_sd_event(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct msdc_host *host =
		container_of(nb, struct msdc_host, sd_oc.nb);

	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
	case REGULATOR_EVENT_FAIL:
		schedule_work(&host->sd_oc.work);
		break;
	default:
		break;
	};

	return NOTIFY_OK;
}

static void sdcard_oc_handler(struct work_struct *work)
{
	struct msdc_host *host =
		container_of(work, struct msdc_host, sd_oc.work);

	msdc_sd_power_off(host);
}

//+dunweichao.wt,2024.4.8,add proc file for sdcard slot detect
static int sim_card_status_show(struct seq_file *m, void *v)
{
    int gpio_value = 0;
    gpio_value = gpio_get_value_cansleep(cd_gpio);
    pr_err("%s: cd_gpio is %d gpio_value is %d\n", __func__,cd_gpio, gpio_value);
    seq_printf(m, "%d\n", gpio_value);
    return 0;
}
static int sim_card_status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sim_card_status_show, NULL);
}
static const struct proc_ops sim_card_status_fops = {
    .proc_open       = sim_card_status_proc_open,
    .proc_read       = seq_read,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
static int sim_card_tray_create_proc(void)
{
    struct proc_dir_entry *status_entry;
    status_entry = proc_create("sd_tray_gpio_value", 0, NULL, &sim_card_status_fops);
    if (!status_entry) {
        return -ENOMEM;
    }
    return 0;
}
static void sim_card_tray_remove_proc(void)
{
    remove_proc_entry("sd_tray_gpio_value", NULL);
}
//-dunweichao.wt,2024.4.8,add proc file for sdcard slot detect

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
static void msdc_swcq_dump(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	msdc_dump_info(NULL, 0, NULL, host);
}

static void  msdc_swcq_err_handle(struct mmc_host *mmc)
{

}

static void msdc_swcq_prepare_tuning(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200)
		host->is_skip_hs200_tune = 0;
}

static void msdc_swcq_cqe_enable(struct mmc_host *mmc, struct mmc_card *card)
{
	struct msdc_host *host = mmc_priv(mmc);

	/* enable busy check */
	sdr_set_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
	/* default write data / busy timeout 20s */
	msdc_set_busy_timeout(host, 20 * 1000000000ULL, 0);
	/* default read data timeout 1s */
	msdc_set_timeout(host, 1000000000ULL, 0);
	/* set the rst pin to enable device reset n function*/
	msdc_enable_rst_n_func(mmc, card);
}

static void msdc_swcq_cqe_disable(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 val;

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
	/* disable busy check */
	sdr_clr_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
}

static const struct swcq_host_ops msdc_swcq_ops = {
	.dump_info = msdc_swcq_dump,
	.err_handle = msdc_swcq_err_handle,
	.prepare_tuning = msdc_swcq_prepare_tuning,
	.enable = msdc_swcq_cqe_enable,
	.disable = msdc_swcq_cqe_disable,
};
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_DEBUG)
static void msdc_gpio_of_parse(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	struct device *dev = mmc->parent;

	if (device_property_read_u32(dev, "dump-gpio-start",
		&host->dump_gpio_start))
		host->dump_gpio_start = 0;
	if (device_property_read_u32(dev, "dump-gpio-end",
		&host->dump_gpio_end))
		host->dump_gpio_end = 0;

	dev_info(dev, "msdc_gpio:%d-%d\n" ,
		host->dump_gpio_start,
		host->dump_gpio_end
		);
}
#endif

static int mtk_msdc_get_chipid(struct msdc_host *host)
{
	struct device_node *node;
	struct mmc_tag_chipid *chip_id;
	int len;

	host->sw_ver = -1;
	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (node) {
		chip_id = (struct mmc_tag_chipid *)of_get_property(node,
								"atag,chipid",
								&len);
		if (!chip_id) {
			dev_info(host->dev, "could not found atag,chipid in chosen\n");
			return -ENODEV;
		}
	} else {
		dev_info(host->dev, "chosen node not found in device tree\n");
		return -ENODEV;
	}

	host->sw_ver = chip_id->sw_ver;
	dev_dbg(host->dev, "chip_id->size: 0x%x\n", chip_id->size);
	dev_dbg(host->dev, "chip_id->hw_code: 0x%x\n", chip_id->hw_code);
	dev_dbg(host->dev, "chip_id->hw_subcode: 0x%x\n", chip_id->hw_subcode);
	dev_dbg(host->dev, "chip_id->hw_ver: 0x%x\n", chip_id->hw_ver);
	dev_dbg(host->dev, "chip_id->sw_ver: 0x%x\n", chip_id->sw_ver);
	return 0;
}

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct tag_bootmode {
		u32 size;
		u32 tag;
		u32 bootmode;
		u32 boottype;
	} *tag = NULL;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	struct resource *res;
#endif
	int ret;
	int msdc_gpio = 1;
	int host_index = -1;

	dev_info(&pdev->dev, "[%s %d]mtk-mmc start\n",__func__,__LINE__);

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}

	/* Add check_boot_type check and return ENODEV if not eMMC boot */
	if (device_property_read_u32(&pdev->dev, "host-index", &host_index) < 0) {
		dev_info(&pdev->dev, "index property is missing\n");
		host_index = -1;
	}

	/* Get boot type from bootmode */
	tag = (struct tag_bootmode *)mtk_get_boot_property(pdev->dev.of_node, "atag,boot", NULL);
	if (!tag)
		dev_info(&pdev->dev, "failed to get atag,boot\n");
	else if ((tag->boottype != BOOTDEV_EMMC) && (host_index == 0)) {
		dev_info(&pdev->dev, "no eMMC boot\n");
		return -ENODEV;
	}

	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	ret = mmc_of_parse(mmc);
	if (ret)
		goto host_free;

	mmc->cqe_recovery_reset_always = 1;

	host->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto host_free;
	}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		host->top_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(host->top_base))
			host->top_base = NULL;
	}

	ret = mmc_regulator_get_supply(mmc);
	if (ret)
		goto host_free;

	ret = msdc_of_clock_parse(pdev, host);
	if (ret)
		goto host_free;
#endif

	host->reset = devm_reset_control_get_optional_exclusive(&pdev->dev,
								"hrst");
	if (IS_ERR(host->reset)) {
		ret = PTR_ERR(host->reset);
		goto host_free;
	}

	/* only eMMC has crypto property */
	if (!(mmc->caps2 & MMC_CAP2_NO_MMC)) {
		if (!IS_ERR_OR_NULL(host->crypto_clk) ||
			!IS_ERR_OR_NULL(host->crypto_cg))
			mmc->caps2 |= MMC_CAP2_CRYPTO;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = -EINVAL;
		goto host_free;
	}

	msdc_of_property_parse(pdev, host);

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		dev_err(&pdev->dev, "Cannot find pinctrl!\n");
		goto host_free;
	}

	msdc_gpio = msdc_get_gpio_version();

	host->pins_default = pinctrl_lookup_state(host->pinctrl,
		(msdc_gpio == 2 && mmc->index == 0) ? "default_v2" : "default");
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		dev_err(&pdev->dev, "Cannot find pinctrl default!\n");
		goto host_free;
	}
	pinctrl_select_state(host->pinctrl, host->pins_default);

	host->pins_uhs = pinctrl_lookup_state(host->pinctrl,
		(msdc_gpio == 2 && mmc->index == 0) ? "state_uhs_v2" : "state_uhs");
	if (IS_ERR(host->pins_uhs)) {
		ret = PTR_ERR(host->pins_uhs);
		dev_err(&pdev->dev, "Cannot find pinctrl uhs!\n");
		goto host_free;
	}

	host->pins_pull_down = pinctrl_lookup_state(host->pinctrl,
		(msdc_gpio == 2 && mmc->index == 0) ? "pull_down_v2" : "pull_down");
	if (IS_ERR(host->pins_pull_down)) {
		ret = PTR_ERR(host->pins_pull_down);
		dev_info(&pdev->dev, "Cannot find pinctrl pull_down!\n");
		host->pins_pull_down = NULL;
	}
#endif

	host->pins_state = PINS_DEFAULT;

	if (host->id == MSDC_SD) {
		host->sd_oc.nb.notifier_call = msdc_sd_event;
		INIT_WORK(&host->sd_oc.work, sdcard_oc_handler);
	}

	host->dev = &pdev->dev;
	host->dev_comp = of_device_get_match_data(&pdev->dev);
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	host->src_clk_freq = clk_get_rate(host->src_clk);
	if (host->ocr_volt != 0)
		mmc->ocr_avail = host->ocr_volt;
#else
	host->src_clk_freq = FPGA_SRC_CLK;
	mmc->ocr_avail = MSDC_OCR_AVAIL;
#endif
	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	if (host->dev_comp->clk_div_bits == 8)
		mmc->f_min = DIV_ROUND_UP(host->src_clk_freq, 4 * 255);
	else
		mmc->f_min = DIV_ROUND_UP(host->src_clk_freq, 4 * 4095);

	if (host->dev_comp->infra_check.enable) {
		host->infra_ack_vaddr =
			ioremap(host->dev_comp->infra_check.infra_ack_paddr, 0x4);
		if (host->infra_ack_vaddr == NULL)
			pr_info("mmc%d infra_ack_paddr ioremap fail\n", host->id);
	}

	if (!(mmc->caps & MMC_CAP_NONREMOVABLE) &&
	    !mmc_can_gpio_cd(mmc) &&
	    host->dev_comp->use_internal_cd) {
		/*
		 * Is removable but no GPIO declared, so
		 * use internal functionality.
		 */
		host->internal_cd = true;
	}

	if (host->sdcard_aggressive_pm)
		mmc->caps |= MMC_CAP_AGGRESSIVE_PM;

	if (mmc->caps & MMC_CAP_SDIO_IRQ)
		mmc->caps2 |= MMC_CAP2_SDIO_IRQ_NOTHREAD;

	mmc->caps |= MMC_CAP_CMD23;

	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_BD_NUM;
	if (host->dev_comp->support_64g)
		mmc->max_seg_size = BDMA_DESC_BUFLEN_EXT;
	else
		mmc->max_seg_size = BDMA_DESC_BUFLEN;
	mmc->max_req_size = 512 * 1024;
	mmc->max_blk_count = mmc->max_req_size / 512;
	if (host->dev_comp->support_64g)
		host->dma_mask = DMA_BIT_MASK(36);
	else
		host->dma_mask = DMA_BIT_MASK(32);
	mmc_dev(mmc)->dma_mask = &host->dma_mask;

	host->timeout_clks = 3 * 1048576;

	if (dma_set_coherent_mask(&pdev->dev, host->dma_mask))
		dev_info(host->dev, "mmc%d dma set mask fail\n", host->id);

	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
				2 * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
				MAX_BD_NUM * sizeof(struct mt_bdma_desc),
				&host->dma.bd_addr, GFP_KERNEL);
	if (!host->dma.gpd || !host->dma.bd) {
		ret = -ENOMEM;
		goto release_mem;
	}
	msdc_init_gpd_bd(host, &host->dma);
	INIT_DELAYED_WORK(&host->req_timeout, msdc_request_timeout);
	spin_lock_init(&host->lock);

	platform_set_drvdata(pdev, mmc);
	ret = msdc_ungate_clock(host);
	if (ret) {
		dev_info(&pdev->dev, "Cannot ungate clocks!\n");
		goto release_mem;
	}
	msdc_init_hw(host);

	if (host->cqhci) {
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
		mmc->caps2 |= MMC_CAP2_CQE;
		host->swcq_host = devm_kzalloc(mmc->parent, sizeof(*host->swcq_host), GFP_KERNEL);
		if (!host->swcq_host) {
			ret = -ENOMEM;
			goto host_free;
		}
		host->swcq_host->ops = &msdc_swcq_ops;
		ret = swcq_init(host->swcq_host, mmc);
		mmc->max_segs = 128;
		if (ret)
			goto host_free;
		goto skip_hwcq;
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_CQHCI)
		mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
		host->cq_host = devm_kzalloc(mmc->parent, sizeof(*host->cq_host), GFP_KERNEL);
		if (!host->cq_host) {
			ret = -ENOMEM;
			goto host_free;
		}
		host->cq_host->caps |= CQHCI_TASK_DESC_SZ_128;
		host->cq_host->quirks |= CQHCI_QUIRK_DIS_BEFORE_NON_CQ_CMD;
		host->cq_host->mmio = host->base + 0x800;
		host->cq_host->ops = &msdc_cmdq_ops;
		ret = cqhci_init(host->cq_host, mmc, true);
		if (ret)
			goto host_free;
		mmc->max_segs = 128;
		/* cqhci 16bit length */
		/* 0 size, means 65536 so we don't have to -1 here */
		mmc->max_seg_size = 64 * 1024;
#endif
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
skip_hwcq:
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_DEBUG)
	spin_lock_init(&host->log_lock);
#endif

	spin_lock_init(&host->err_info_lock);
	ret = devm_request_threaded_irq(&pdev->dev, host->irq, msdc_irq, msdc_thread_irq,
					IRQF_TRIGGER_NONE, pdev->name, host);

	if (ret)
		goto release;

	if (host->dvfsrc_vcore_power && host->req_vcore)
		if (regulator_set_voltage(host->dvfsrc_vcore_power,
			host->req_vcore, INT_MAX))
			dev_info(host->dev, "%s: failed to set vcore to %d\n",
				__func__, host->req_vcore);

	if (host->id == MSDC_SD || host->id == MSDC_EMMC)
		irq_set_affinity_hint(host->irq, get_cpu_mask(3));

	if (host->id == MSDC_SDIO) {
		ret = request_sdio_eint_irq(host);
		if (ret)
			dev_info(host->dev, "failed to register sdio eint irq!\n");
	}


	if (host->dev_comp->clock_set.set_type == MSDC_CLK_SET_MULTI)
		mtk_msdc_get_chipid(host);

	cpu_latency_qos_add_request(&host->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	pm_runtime_set_active(host->dev);
	pm_runtime_set_autosuspend_delay(host->dev, MTK_MMC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(host->dev);
	pm_runtime_enable(host->dev);
	ret = mmc_add_host(mmc);
	//+dunweichao.wt,2024.8.24,add proc file for sdcard slot detect
	if(sim_card_tray_create_proc()) {
		dev_err(&pdev->dev, "creat proc sim_card_status failed\n");
	} else {
		dev_err(&pdev->dev, "creat proc sim_card_status successed\n");
	}
	//-dunweichao.wt,2024.8.24,add proc file for sdcard slot detect

	if (ret)
		goto end;
	bitmap_zero(host->err_bag.err_bitmap, ERR_BIT_SIZE);
	INIT_KFIFO(host->err_info_bag_ring);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_DEBUG)
	ret = mmc_dbg_register(mmc);
#endif

#if IS_ENABLED(CONFIG_RPMB)
	ret = mmc_rpmb_register(mmc);
#endif

#if IS_ENABLED(CONFIG_SEC_MMC_FEATURE)
	if (host->id == MSDC_EMMC)
		mmc_sec_set_features(mmc);
	else if (host->id == MSDC_SD)
		sd_sec_set_features(mmc);
#endif

	msdc_install_tracepoints(host);
	dev_info(&pdev->dev, "[%s %d]ret=%d\n", __func__, __LINE__, ret);
	return 0;
end:
	pm_runtime_disable(host->dev);
	cpu_latency_qos_remove_request(&host->pm_qos_req);
release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);
release_mem:
	if (host->dma.gpd)
		dma_free_coherent(&pdev->dev,
			2 * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	if (host->dma.bd)
		dma_free_coherent(&pdev->dev,
			MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);
host_free:
	if (!IS_ERR_OR_NULL(host->pinctrl))
		devm_pinctrl_put(host->pinctrl);
	if (host->dev_comp->infra_check.enable &&
		host->infra_ack_vaddr != NULL)
		iounmap(host->infra_ack_vaddr);
	mmc_free_host(mmc);
	dev_info(&pdev->dev, "[%s %d]ret=%d\n", __func__, __LINE__, ret);
	if(!strcmp(mmc_hostname(mmc),"mmc1"))
		sim_card_tray_remove_proc(); //dunweichao.wt,2024.8.24,add proc file for sdcard slot detect
	return ret;
}

static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;

	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);

	pm_runtime_get_sync(host->dev);

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(mmc);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);

	cpu_latency_qos_remove_request(&host->pm_qos_req);
	pm_runtime_disable(host->dev);
	pm_runtime_put_noidle(host->dev);
	dma_free_coherent(&pdev->dev,
			2 * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(&pdev->dev, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);
	if (host->dev_comp->infra_check.enable &&
		host->infra_ack_vaddr != NULL)
		iounmap(host->infra_ack_vaddr);

	mmc_free_host(mmc);
	msdc_uninstall_tracepoints();
	mmc_mtk_biolog_exit();

	return 0;
}

static void msdc_save_reg(struct msdc_host *host)
{
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	host->save_para.msdc_cfg = readl(host->base + MSDC_CFG);
	host->save_para.iocon = readl(host->base + MSDC_IOCON);
	host->save_para.sdc_cfg = readl(host->base + SDC_CFG);
	host->save_para.patch_bit0 = readl(host->base + MSDC_PATCH_BIT);
	host->save_para.patch_bit1 = readl(host->base + MSDC_PATCH_BIT1);
	host->save_para.patch_bit2 = readl(host->base + MSDC_PATCH_BIT2);
	host->save_para.pad_ds_tune = readl(host->base + PAD_DS_TUNE);
	host->save_para.pad_cmd_tune = readl(host->base + PAD_CMD_TUNE);
	host->save_para.emmc50_cfg0 = readl(host->base + EMMC50_CFG0);
	host->save_para.emmc50_cfg3 = readl(host->base + EMMC50_CFG3);
	host->save_para.sdc_fifo_cfg = readl(host->base + SDC_FIFO_CFG);
	if (host->top_base) {
		host->save_para.emmc_top_control =
			readl(host->top_base + EMMC_TOP_CONTROL);
		host->save_para.emmc_top_cmd =
			readl(host->top_base + EMMC_TOP_CMD);
		host->save_para.emmc50_pad_ds_tune =
			readl(host->top_base + EMMC50_PAD_DS_TUNE);
		host->save_para.top_loop_test_control =
			readl(host->top_base + LOOP_TEST_CONTROL);
		host->save_para.top_new_rx_cfg =
			readl(host->top_base + MSDC_TOP_NEW_RX_CFG);
	} else {
		host->save_para.pad_tune = readl(host->base + tune_reg);
	}
}

static void msdc_restore_reg(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	if (support_new_tx(host->dev_comp->new_tx_ver)) {
		sdr_clr_bits(host->base + SDC_ADV_CFG0, SDC_NEW_TX_EN);
		sdr_set_bits(host->base + SDC_ADV_CFG0, SDC_NEW_TX_EN);
	}
	if (support_new_rx(host->dev_comp->new_rx_ver)) {
		sdr_clr_bits(host->base + MSDC_NEW_RX_CFG, MSDC_NEW_RX_PATH_SEL);
		sdr_set_bits(host->base + MSDC_NEW_RX_CFG, MSDC_NEW_RX_PATH_SEL);
	}

	writel(host->save_para.msdc_cfg, host->base + MSDC_CFG);
	writel(host->save_para.iocon, host->base + MSDC_IOCON);
	writel(host->save_para.sdc_cfg, host->base + SDC_CFG);
	writel(host->save_para.patch_bit0, host->base + MSDC_PATCH_BIT);
	writel(host->save_para.patch_bit1, host->base + MSDC_PATCH_BIT1);
	writel(host->save_para.patch_bit2, host->base + MSDC_PATCH_BIT2);
	writel(host->save_para.pad_ds_tune, host->base + PAD_DS_TUNE);
	writel(host->save_para.pad_cmd_tune, host->base + PAD_CMD_TUNE);
	writel(host->save_para.emmc50_cfg0, host->base + EMMC50_CFG0);
	writel(host->save_para.emmc50_cfg3, host->base + EMMC50_CFG3);
	writel(host->save_para.sdc_fifo_cfg, host->base + SDC_FIFO_CFG);
	if (host->top_base) {
		writel(host->save_para.emmc_top_control,
		       host->top_base + EMMC_TOP_CONTROL);
		writel(host->save_para.emmc_top_cmd,
		       host->top_base + EMMC_TOP_CMD);
		writel(host->save_para.emmc50_pad_ds_tune,
		       host->top_base + EMMC50_PAD_DS_TUNE);
		writel(host->save_para.top_loop_test_control,
			   host->top_base + LOOP_TEST_CONTROL);
		writel(host->save_para.top_new_rx_cfg,
			   host->top_base + MSDC_TOP_NEW_RX_CFG);
	} else {
		writel(host->save_para.pad_tune, host->base + tune_reg);
	}

	if (sdio_irq_claimed(mmc))
		__msdc_enable_sdio_irq(host, 1);
}

static int infra_req_ack_check(struct msdc_host *host)
{
	struct arm_smccc_res res;

	int cnt = 200;
	u32 val = 0;

	mmc_mtk_infra_req(res, host->id);
	if (res.a0) {
		pr_info("%s: infra request fail, err: %lu\n",
			 __func__, res.a0);
		return 1;
	}

	if (host->infra_ack_vaddr == NULL) {
		pr_info("mmc%d infra_ack_vaddr NULL pointer err\n",
			 host->id);
		return 1;
	}

	while(!val && cnt-- > 0) {
		/* It takes close to 1ms for infra ack ready */
		udelay(10);
		sdr_get_field(host->infra_ack_vaddr,
			host->dev_comp->infra_check.infra_ack_bit, &val);
	}

	if (val == 0)
		pr_info("mmc%d infra ack polling timeout\n", host->id);

	return val ? 0 : 1;
}

static int infra_req_release(struct msdc_host *host)
{
	struct arm_smccc_res res;

	mmc_mtk_infra_req_release(res, host->id);
	if (res.a0) {
		pr_info("%s: infra request release fail, err: %lu\n",
			 __func__, res.a0);
		return 1;
	}

	return 0;
}

static int __maybe_unused msdc_runtime_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);
	u32 val = 0;
	int ret = 0;

	if (mmc->caps2 & MMC_CAP2_CQE) {
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
		if (host->swcq_host)
			ret = 0;
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_CQHCI)
		if (host->cq_host)
			ret = cqhci_suspend(mmc);
#endif
		val = readl(host->base + MSDC_INT);
		writel(val, host->base + MSDC_INT);
		if (ret)
			dev_dbg(host->dev, "%s: %d\n", __func__, ret);
	}

	sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	if (host->sdio_irq_cnt == 0 && host->id == MSDC_SDIO) {
		enable_irq(host->eint_irq);
		enable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt++;
	}

	msdc_save_reg(host);
	msdc_gate_clock(host);

	/* release spm request to avoid infra can not keep off */
	if (host->dev_comp->infra_check.enable)
		if (infra_req_release(host))
			WARN_ON_ONCE(1);

	if (host->dvfsrc_vcore_power && host->req_vcore) {
		if (regulator_set_voltage(host->dvfsrc_vcore_power, 0, INT_MAX))
			dev_info(host->dev, "%s: failed to set vcore to MIN\n", __func__);
		else
			dev_dbg(host->dev, "%s: set vcore %d,get vcore %d\n", __func__,
			0, regulator_get_voltage(host->dvfsrc_vcore_power));
	}

	cpu_latency_qos_update_request(&host->pm_qos_req,
		PM_QOS_DEFAULT_VALUE);


	if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		if (host->pins_pull_down)
			pinctrl_select_state(host->pinctrl, host->pins_pull_down);

	return 0;
}

static int __maybe_unused msdc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);
	int ret;

	if (!(mmc->caps2 & MMC_CAP2_NO_SD)) {
		if (host->pins_state == PINS_DEFAULT && host->pins_default)
			pinctrl_select_state(host->pinctrl, host->pins_default);
		else if (host->pins_state == PINS_UHS && host->pins_uhs)
			pinctrl_select_state(host->pinctrl, host->pins_uhs);
	}

	cpu_latency_qos_update_request(&host->pm_qos_req, 0);
	if (host->dvfsrc_vcore_power && host->req_vcore) {
		if (regulator_set_voltage(host->dvfsrc_vcore_power,
			host->req_vcore, INT_MAX))
			dev_info(host->dev, "%s: failed to set vcore to %d\n",
				__func__, host->req_vcore);
		else
			dev_dbg(host->dev,"%s: set vcore %d,get vcore %d\n", __func__,
				host->req_vcore, regulator_get_voltage(host->dvfsrc_vcore_power));
	}

	/*
	 * ensure infra keep on during msdc ip work.
	 * sw flow: send spm request -> ungating cg -> polling infra ack -> ip work
	 */
	if (host->dev_comp->infra_check.enable) {
		ret = infra_req_ack_check(host);
		if (ret)
			pr_info("mmc%d infra ack polling fail\n", host->id);
	}

	ret = msdc_ungate_clock(host);
	if (ret)
		return ret;

	msdc_restore_reg(host);

	if (host->sdio_irq_cnt > 0 && host->id == MSDC_SDIO) {
		disable_irq_nosync(host->eint_irq);
		disable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt--;
	}
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
	if (host->swcq_host && host->dev_comp->set_crypto_enable_in_sw)
		mmc_mtk_crypto_enable(mmc);
#endif

	return 0;
}

static int __maybe_unused msdc_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);
	u32 val = 0;
	int ret = 0;

	if (mmc->caps2 & MMC_CAP2_CQE) {
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_MTK_SW_CQHCI)
		if (host->swcq_host)
			ret = 0;
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_CQHCI)
		if (host->cq_host)
			ret = cqhci_suspend(mmc);
#endif
		val = readl(host->base + MSDC_INT);
		writel(val, host->base + MSDC_INT);
		if (ret)
			return ret;
	}

	return pm_runtime_force_suspend(dev);
}

static int __maybe_unused msdc_resume(struct device *dev)
{
	return pm_runtime_force_resume(dev);
}

static void msdc_vh_mmc_update_mmc_queue(void *data,
			struct mmc_card *card, struct mmc_queue *mq)
{
	bool cache_enabled;

	if (!mmc_card_mmc(card))
		return;

	cache_enabled = !!test_bit(QUEUE_FLAG_WC, &mq->queue->queue_flags);
	blk_queue_write_cache(mq->queue, cache_enabled, false);
	blk_queue_flag_set(QUEUE_FLAG_SAME_FORCE, mq->queue);
	dev_dbg(mmc_dev(card->host), "disable fua and force complete on same CPU in mmc queue\n");
}

static struct tracepoints_table msdc_interests[] = {
	{
		.name = "android_vh_mmc_update_mmc_queue",
		.func = msdc_vh_mmc_update_mmc_queue
	},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(msdc_interests) / sizeof(struct tracepoints_table); \
	i++)

static void msdc_lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(msdc_interests[i].name, tp->name) == 0)
			msdc_interests[i].tp = tp;
	}
}

static void msdc_uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (msdc_interests[i].init) {
			tracepoint_probe_unregister(msdc_interests[i].tp,
						    msdc_interests[i].func,
						    NULL);
		}
	}
}

static int msdc_install_tracepoints(struct msdc_host *host)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(msdc_lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (msdc_interests[i].tp == NULL) {
			dev_info(host->dev, "Error: tracepoint %s not found\n",
				msdc_interests[i].name);
			continue;
		}

		tracepoint_probe_register(msdc_interests[i].tp,
					  msdc_interests[i].func,
					  NULL);
		msdc_interests[i].init = true;
	}

	return 0;
}

static const struct dev_pm_ops msdc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msdc_suspend, msdc_resume)
	SET_RUNTIME_PM_OPS(msdc_runtime_suspend, msdc_runtime_resume, NULL)
};

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = "mtk-msdc",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = msdc_of_ids,
		.pm = &msdc_dev_pm_ops,
	},
};

module_platform_driver(mt_msdc_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
