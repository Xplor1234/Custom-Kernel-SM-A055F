// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/of_device.h>

#include <video/videomode.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_simple_kms_helper.h>

#include "../mtk_dump.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_drm_drv.h"

#include "mtk_drm_dp_intf_regs.h"
#include "mtk_drm_dp_common.h"

extern struct mtk_dp *g_mtk_dp;

#define YUV422_PRIORITY		0x0

enum mtk_dpi_out_bit_num {
	MTK_DPI_OUT_BIT_NUM_8BITS,
	MTK_DPI_OUT_BIT_NUM_10BITS,
	MTK_DPI_OUT_BIT_NUM_12BITS,
	MTK_DPI_OUT_BIT_NUM_16BITS
};

enum mtk_dpi_out_yc_map {
	MTK_DPI_OUT_YC_MAP_RGB,
	MTK_DPI_OUT_YC_MAP_CYCY,
	MTK_DPI_OUT_YC_MAP_YCYC,
	MTK_DPI_OUT_YC_MAP_CY,
	MTK_DPI_OUT_YC_MAP_YC
};

enum mtk_dpi_out_channel_swap {
	MTK_DPI_OUT_CHANNEL_SWAP_RGB,
	MTK_DPI_OUT_CHANNEL_SWAP_GBR,
	MTK_DPI_OUT_CHANNEL_SWAP_BRG,
	MTK_DPI_OUT_CHANNEL_SWAP_RBG,
	MTK_DPI_OUT_CHANNEL_SWAP_GRB,
	MTK_DPI_OUT_CHANNEL_SWAP_BGR
};

enum mtk_dpi_out_color_format {
	MTK_DPI_COLOR_FORMAT_RGB,
	MTK_DPI_COLOR_FORMAT_YCBCR_422
};

enum TVDPLL_CLK {
	TCK_26M = 0,
	TVDPLL_D2 = 1,
	TVDPLL_D4 = 2,
	TVDPLL_D8 = 3,
	TVDPLL_D16 = 4,
	TVDPLL_PLL = 5,
};

struct mtk_dpi {
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_connector *connector;
	struct mtk_ddp_comp ddp_comp;
	void __iomem *regs;
	struct device *dev;
	struct device *mmsys_dev;
	struct clk *engine_clk;
	struct clk *pixel_clk;
	struct clk *tvd_clk;
	struct clk *hf_fmm_ck;
	struct clk *hf_fdp_ck;
	struct clk *pclk;
	struct clk *pclk_src[6];
	int irq;
	struct drm_display_mode mode;
	const struct mtk_dpi_conf *conf;
	enum mtk_dpi_out_color_format color_format;
	enum mtk_dpi_out_yc_map yc_map;
	enum mtk_dpi_out_bit_num bit_num;
	enum mtk_dpi_out_channel_swap channel_swap;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_gpio;
	struct pinctrl_state *pins_dpi;
	u32 output_fmt;
	int refcount;
	unsigned long long dp_intf_bw;
	bool suspend;
};

#define MAX_DPI		2
static struct mtk_dpi *g_mtk_dpi[MAX_DPI];

static inline struct mtk_dpi *bridge_to_dpi(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dpi, bridge);
}

enum mtk_dpi_polarity {
	MTK_DPI_POLARITY_RISING,
	MTK_DPI_POLARITY_FALLING,
};

struct mtk_dpi_polarities {
	enum mtk_dpi_polarity de_pol;
	enum mtk_dpi_polarity ck_pol;
	enum mtk_dpi_polarity hsync_pol;
	enum mtk_dpi_polarity vsync_pol;
};

struct mtk_dpi_sync_param {
	u32 sync_width;
	u32 front_porch;
	u32 back_porch;
	bool shift_half_line;
};

struct mtk_dpi_yc_limit {
	u16 y_top;
	u16 y_bottom;
	u16 c_top;
	u16 c_bottom;
};

/**
 * struct mtk_dpi_conf - Configuration of mediatek dpi.
 * @cal_factor: Callback function to calculate factor value.
 * @reg_h_fre_con: Register address of frequency control.
 * @max_clock_khz: Max clock frequency supported for this SoCs in khz units.
 * @edge_sel_en: Enable of edge selection.
 * @output_fmts: Array of supported output formats.
 * @num_output_fmts: Quantity of supported output formats.
 * @is_ck_de_pol: Support CK/DE polarity.
 * @swap_input_support: Support input swap function.
 * @support_direct_pin: IP supports direct connection to dpi panels.
 * @input_2pixel: Input pixel of dp_intf is 2 pixel per round, so enable this
 *		  config to enable this feature.
 * @dimension_mask: Mask used for HWIDTH, HPORCH, VSYNC_WIDTH and VSYNC_PORCH
 *		    (no shift).
 * @hvsize_mask: Mask of HSIZE and VSIZE mask (no shift).
 * @channel_swap_shift: Shift value of channel swap.
 * @yuv422_en_bit: Enable bit of yuv422.
 * @csc_enable_bit: Enable bit of CSC.
 * @pixels_per_iter: Quantity of transferred pixels per iteration.
 * @edge_cfg_in_mmsys: If the edge configuration for DPI's output needs to be set in MMSYS.
 */
struct mtk_dpi_conf {
	unsigned int (*cal_factor)(int clock);
	u32 reg_h_fre_con;
	u32 max_clock_khz;
	bool edge_sel_en;
	const u32 *output_fmts;
	u32 num_output_fmts;
	bool is_ck_de_pol;
	bool swap_input_support;
	bool support_direct_pin;
	bool input_2pixel;
	u32 dimension_mask;
	u32 hvsize_mask;
	u32 channel_swap_shift;
	u32 yuv422_en_bit;
	u32 csc_enable_bit;
	u32 pixels_per_iter;
	bool edge_cfg_in_mmsys;
	bool no_next_bridge;
};

static void mtk_dpi_mask(struct mtk_dpi *dpi, u32 offset, u32 val, u32 mask)
{
	u32 tmp = readl(dpi->regs + offset) & ~mask;

	tmp |= (val & mask);
	writel(tmp, dpi->regs + offset);
}

static void mtk_dpi_write(struct mtk_dpi *dpi, u32 offset, u32 val)
{
	writel(val, dpi->regs + offset);
}

static void mtk_dpi_sw_reset(struct mtk_dpi *dpi, bool reset)
{
	mtk_dpi_mask(dpi, DPI_RET, reset ? RST : 0, RST);
}

#define DP_PATTERN_CTRL0				0x0F00
#define DP_PATTERN_COLOR_BAR			BIT(6)
#define DP_PATTERN_EN					BIT(0)

static void mtk_dpi_enable(struct mtk_dpi *dpi)
{
	dev_info(dpi->dev, "enable dpi go");

	mtk_dpi_mask(dpi, DPI_INTSTA, 0, 0xf);

	mtk_dpi_mask(dpi, DPI_INTEN, INT_VSYNC_EN, (INT_UNDERFLOW_EN |
						   INT_VDE_EN |
						   INT_VSYNC_EN));
	mtk_dpi_mask(dpi, DPI_EN, EN, EN);
	//mtk_dpi_mask(dpi, DP_PATTERN_CTRL0, DP_PATTERN_EN, DP_PATTERN_EN);
	//mtk_dpi_mask(dpi, DP_PATTERN_CTRL0, DP_PATTERN_COLOR_BAR, DP_PATTERN_COLOR_BAR);
}

static void mtk_dpi_disable(struct mtk_dpi *dpi)
{
	mtk_dpi_mask(dpi, DPI_EN, 0, EN);
}

static void mtk_dpi_config_hsync(struct mtk_dpi *dpi,
				 struct mtk_dpi_sync_param *sync)
{
	mtk_dpi_mask(dpi, DPI_TGEN_HWIDTH, sync->sync_width << HPW,
		     dpi->conf->dimension_mask << HPW);
	mtk_dpi_mask(dpi, DPI_TGEN_HPORCH, sync->back_porch << HBP,
		     dpi->conf->dimension_mask << HBP);
	mtk_dpi_mask(dpi, DPI_TGEN_HPORCH, sync->front_porch << HFP,
		     dpi->conf->dimension_mask << HFP);
}

static void mtk_dpi_config_vsync(struct mtk_dpi *dpi,
				 struct mtk_dpi_sync_param *sync,
				 u32 width_addr, u32 porch_addr)
{
	mtk_dpi_mask(dpi, width_addr,
		     sync->shift_half_line << VSYNC_HALF_LINE_SHIFT,
		     VSYNC_HALF_LINE_MASK);
	mtk_dpi_mask(dpi, width_addr,
		     sync->sync_width << VSYNC_WIDTH_SHIFT,
		     dpi->conf->dimension_mask << VSYNC_WIDTH_SHIFT);
	mtk_dpi_mask(dpi, porch_addr,
		     sync->back_porch << VSYNC_BACK_PORCH_SHIFT,
		     dpi->conf->dimension_mask << VSYNC_BACK_PORCH_SHIFT);
	mtk_dpi_mask(dpi, porch_addr,
		     sync->front_porch << VSYNC_FRONT_PORCH_SHIFT,
		     dpi->conf->dimension_mask << VSYNC_FRONT_PORCH_SHIFT);
}

static void mtk_dpi_config_vsync_lodd(struct mtk_dpi *dpi,
				      struct mtk_dpi_sync_param *sync)
{
	mtk_dpi_config_vsync(dpi, sync, DPI_TGEN_VWIDTH, DPI_TGEN_VPORCH);
}

static void mtk_dpi_config_vsync_leven(struct mtk_dpi *dpi,
				       struct mtk_dpi_sync_param *sync)
{
	mtk_dpi_config_vsync(dpi, sync, DPI_TGEN_VWIDTH_LEVEN,
			     DPI_TGEN_VPORCH_LEVEN);
}

static void mtk_dpi_config_vsync_rodd(struct mtk_dpi *dpi,
				      struct mtk_dpi_sync_param *sync)
{
	mtk_dpi_config_vsync(dpi, sync, DPI_TGEN_VWIDTH_RODD,
			     DPI_TGEN_VPORCH_RODD);
}

static void mtk_dpi_config_vsync_reven(struct mtk_dpi *dpi,
				       struct mtk_dpi_sync_param *sync)
{
	mtk_dpi_config_vsync(dpi, sync, DPI_TGEN_VWIDTH_REVEN,
			     DPI_TGEN_VPORCH_REVEN);
}

static void mtk_dpi_config_pol(struct mtk_dpi *dpi,
			       struct mtk_dpi_polarities *dpi_pol)
{
	unsigned int pol;
	unsigned int mask;

	mask = HSYNC_POL | VSYNC_POL;
	pol = (dpi_pol->hsync_pol == MTK_DPI_POLARITY_RISING ? 0 : HSYNC_POL) |
	      (dpi_pol->vsync_pol == MTK_DPI_POLARITY_RISING ? 0 : VSYNC_POL);
	if (dpi->conf->is_ck_de_pol) {
		mask |= CK_POL | DE_POL;
		pol |= (dpi_pol->ck_pol == MTK_DPI_POLARITY_RISING ?
			0 : CK_POL) |
		       (dpi_pol->de_pol == MTK_DPI_POLARITY_RISING ?
			0 : DE_POL);
	}

	mtk_dpi_mask(dpi, DPI_OUTPUT_SETTING, pol, mask);
}

static void mtk_dpi_config_3d(struct mtk_dpi *dpi, bool en_3d)
{
	mtk_dpi_mask(dpi, DPI_CON, en_3d ? TDFP_EN : 0, TDFP_EN);
}

static void mtk_dpi_config_interface(struct mtk_dpi *dpi, bool inter)
{
	mtk_dpi_mask(dpi, DPI_CON, inter ? INTL_EN : 0, INTL_EN);
}

static void mtk_dpi_config_fb_size(struct mtk_dpi *dpi, u32 width, u32 height)
{
	mtk_dpi_mask(dpi, DPI_SIZE, width << HSIZE,
		     dpi->conf->hvsize_mask << HSIZE);
	mtk_dpi_mask(dpi, DPI_SIZE, height << VSIZE,
		     dpi->conf->hvsize_mask << VSIZE);
}

static void mtk_dpi_config_channel_limit(struct mtk_dpi *dpi)
{
	struct mtk_dpi_yc_limit limit;

	if (drm_default_rgb_quant_range(&dpi->mode) ==
	    HDMI_QUANTIZATION_RANGE_LIMITED) {
		limit.y_bottom = 0x10;
		limit.y_top = 0xfe0;
		limit.c_bottom = 0x10;
		limit.c_top = 0xfe0;
	} else {
		limit.y_bottom = 0;
		limit.y_top = 0xfff;
		limit.c_bottom = 0;
		limit.c_top = 0xfff;
	}

	mtk_dpi_mask(dpi, DPI_Y_LIMIT, limit.y_bottom << Y_LIMINT_BOT,
		     Y_LIMINT_BOT_MASK);
	mtk_dpi_mask(dpi, DPI_Y_LIMIT, limit.y_top << Y_LIMINT_TOP,
		     Y_LIMINT_TOP_MASK);
	mtk_dpi_mask(dpi, DPI_C_LIMIT, limit.c_bottom << C_LIMIT_BOT,
		     C_LIMIT_BOT_MASK);
	mtk_dpi_mask(dpi, DPI_C_LIMIT, limit.c_top << C_LIMIT_TOP,
		     C_LIMIT_TOP_MASK);
}

static void mtk_dpi_config_bit_num(struct mtk_dpi *dpi,
				   enum mtk_dpi_out_bit_num num)
{
	u32 val;

	switch (num) {
	case MTK_DPI_OUT_BIT_NUM_8BITS:
		val = OUT_BIT_8;
		break;
	case MTK_DPI_OUT_BIT_NUM_10BITS:
		val = OUT_BIT_10;
		break;
	case MTK_DPI_OUT_BIT_NUM_12BITS:
		val = OUT_BIT_12;
		break;
	case MTK_DPI_OUT_BIT_NUM_16BITS:
		val = OUT_BIT_16;
		break;
	default:
		val = OUT_BIT_8;
		break;
	}
	mtk_dpi_mask(dpi, DPI_OUTPUT_SETTING, val << OUT_BIT,
		     OUT_BIT_MASK);
}

static void mtk_dpi_config_yc_map(struct mtk_dpi *dpi,
				  enum mtk_dpi_out_yc_map map)
{
	u32 val;

	switch (map) {
	case MTK_DPI_OUT_YC_MAP_RGB:
		val = YC_MAP_RGB;
		break;
	case MTK_DPI_OUT_YC_MAP_CYCY:
		val = YC_MAP_CYCY;
		break;
	case MTK_DPI_OUT_YC_MAP_YCYC:
		val = YC_MAP_YCYC;
		break;
	case MTK_DPI_OUT_YC_MAP_CY:
		val = YC_MAP_CY;
		break;
	case MTK_DPI_OUT_YC_MAP_YC:
		val = YC_MAP_YC;
		break;
	default:
		val = YC_MAP_RGB;
		break;
	}

	mtk_dpi_mask(dpi, DPI_OUTPUT_SETTING, val << YC_MAP, YC_MAP_MASK);
}

static void mtk_dpi_config_channel_swap(struct mtk_dpi *dpi,
					enum mtk_dpi_out_channel_swap swap)
{
	u32 val;

	switch (swap) {
	case MTK_DPI_OUT_CHANNEL_SWAP_RGB:
		val = SWAP_RGB;
		break;
	case MTK_DPI_OUT_CHANNEL_SWAP_GBR:
		val = SWAP_GBR;
		break;
	case MTK_DPI_OUT_CHANNEL_SWAP_BRG:
		val = SWAP_BRG;
		break;
	case MTK_DPI_OUT_CHANNEL_SWAP_RBG:
		val = SWAP_RBG;
		break;
	case MTK_DPI_OUT_CHANNEL_SWAP_GRB:
		val = SWAP_GRB;
		break;
	case MTK_DPI_OUT_CHANNEL_SWAP_BGR:
		val = SWAP_BGR;
		break;
	default:
		val = SWAP_RGB;
		break;
	}

	mtk_dpi_mask(dpi, DPI_OUTPUT_SETTING,
		     val << dpi->conf->channel_swap_shift,
		     CH_SWAP_MASK << dpi->conf->channel_swap_shift);
}

static void mtk_dpi_config_yuv422_enable(struct mtk_dpi *dpi, bool enable)
{
	if (enable) {
		mtk_dpi_mask(dpi, DPI_YUV, DPI_YUV_EN, DPI_YUV_EN);
		mtk_dpi_mask(dpi, DPI_YUV, DPI_YUV422_EN, DPI_YUV422_EN);
	} else {
		mtk_dpi_mask(dpi, DPI_YUV, 0, DPI_YUV_EN);
		mtk_dpi_mask(dpi, DPI_YUV, 0, DPI_YUV422_EN);
	}
}

static void mtk_dpi_config_swap_input(struct mtk_dpi *dpi, bool enable)
{
	mtk_dpi_mask(dpi, DPI_CON, enable ? IN_RB_SWAP : 0, IN_RB_SWAP);
}

static void mtk_dpi_config_2n_h_fre(struct mtk_dpi *dpi)
{
	mtk_dpi_mask(dpi, dpi->conf->reg_h_fre_con, H_FRE_2N, H_FRE_2N);
}

static void mtk_dpi_config_disable_edge(struct mtk_dpi *dpi)
{
	if (dpi->conf->edge_sel_en)
		mtk_dpi_mask(dpi, dpi->conf->reg_h_fre_con, 0, EDGE_SEL_EN);
}

static void mtk_dpi_config_color_format(struct mtk_dpi *dpi,
					enum mtk_dpi_out_color_format format)
{
	mtk_dpi_config_channel_swap(dpi, MTK_DPI_OUT_CHANNEL_SWAP_RGB);

	dev_info(dpi->dev, "format:%d", format);

	if (format == MTK_DPI_COLOR_FORMAT_YCBCR_422) {
		mtk_dpi_config_yuv422_enable(dpi, true);

		/*
		 * If height is smaller than 720, we need to use RGB_TO_BT601
		 * to transfer to yuv422. Otherwise, we use RGB_TO_JPEG.
		 */
		mtk_dpi_mask(dpi, DPI_MATRIX_SET, dpi->mode.hdisplay <= 720 ?
			     MATRIX_SEL_RGB_TO_BT601 : MATRIX_SEL_RGB_TO_JPEG,
			     INT_MATRIX_SEL_MASK);
	} else {
		mtk_dpi_config_yuv422_enable(dpi, false);
		if (dpi->conf->swap_input_support)
			mtk_dpi_config_swap_input(dpi, false);
	}
}

static void mtk_dpi_dual_edge(struct mtk_dpi *dpi)
{
	if (dpi->output_fmt == MEDIA_BUS_FMT_RGB888_2X12_LE ||
	    dpi->output_fmt == MEDIA_BUS_FMT_RGB888_2X12_BE) {
		mtk_dpi_mask(dpi, DPI_DDR_SETTING, DDR_EN | DDR_4PHASE,
			     DDR_EN | DDR_4PHASE);
		mtk_dpi_mask(dpi, DPI_OUTPUT_SETTING,
			     dpi->output_fmt == MEDIA_BUS_FMT_RGB888_2X12_LE ?
			     EDGE_SEL : 0, EDGE_SEL);
		//if (dpi->conf->edge_cfg_in_mmsys)
			//mtk_mmsys_ddp_dpi_fmt_config(dpi->mmsys_dev, MTK_DPI_RGB888_DDR_CON);
	} else {
		mtk_dpi_mask(dpi, DPI_DDR_SETTING, DDR_EN | DDR_4PHASE, 0);
		//if (dpi->conf->edge_cfg_in_mmsys)
		//	mtk_mmsys_ddp_dpi_fmt_config(dpi->mmsys_dev, MTK_DPI_RGB888_SDR_CON);
	}
}

static void mtk_dpi_power_off(struct mtk_dpi *dpi)
{
	if (WARN_ON(dpi->refcount == 0))
		return;

	if (--dpi->refcount != 0)
		return;

	mtk_dpi_disable(dpi);

	clk_disable_unprepare(dpi->pclk_src[TVDPLL_PLL]);
	clk_disable_unprepare(dpi->hf_fdp_ck);
	clk_disable_unprepare(dpi->hf_fmm_ck);
}

static int mtk_dpi_power_on(struct mtk_dpi *dpi)
{
	int ret;

	if (++dpi->refcount != 1)
		return 0;

	ret = clk_prepare_enable(dpi->hf_fmm_ck);
	if (ret) {
		dev_info(dpi->dev, "Failed to enable hf_fmm_ck clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dpi->hf_fdp_ck);
	if (ret) {
		dev_info(dpi->dev, "Failed to enable hf_fdp_ck clock: %d\n", ret);
		goto err_pixel;
	}

	ret = clk_prepare_enable(dpi->pclk_src[TVDPLL_PLL]);
	if (ret) {
		dev_info(dpi->dev, "Failed to enable pclk_src: %d\n", ret);
		goto err_pixel;
	}

	return 0;

err_pixel:
	clk_disable_unprepare(dpi->hf_fmm_ck);
err_refcount:
	dpi->refcount--;
	return ret;
}

static void mtk_dpi_set_golden_setting(struct mtk_dpi *dpi)
{
	unsigned int dp_buf_sodi_high, dp_buf_sodi_low;
	unsigned int dp_buf_preultra_high, dp_buf_preultra_low;
	unsigned int dp_buf_ultra_high, dp_buf_ultra_low;
	unsigned int dp_buf_urgent_high, dp_buf_urgent_low;
	unsigned int mmsys_clk, dp_clk; //{26000, 37125, 74250, 148500, 297000};
	unsigned int twait = 12, twake = 5;
	unsigned int fill_rate, consume_rate;

	dp_clk = dpi->mode.clock / 4;
	dp_clk = dp_clk > 0 ? dp_clk : 74250;
	mmsys_clk = mtk_drm_get_mmclk(&dpi->ddp_comp.mtk_crtc->base, __func__) / 1000;
	mmsys_clk = mmsys_clk > 0 ? mmsys_clk : 273000;

	fill_rate = mmsys_clk * 4 / 8;
	consume_rate = dp_clk * 4 / 8;
	dev_info(dpi->dev, "%s mmsys_clk=%d, dp_clk=%d, fill_rate=%d, consume_rate=%d\n",
		 __func__, mmsys_clk, dp_clk, fill_rate, consume_rate);

	dp_buf_sodi_high = (5940000 - twait * 100 * fill_rate / 1000 - consume_rate) * 30 / 32000;
	dp_buf_sodi_low = (25 + twake) * consume_rate * 30 / 32000;

	dp_buf_preultra_high = 36 * consume_rate * 30 / 32000;
	dp_buf_preultra_low = 35 * consume_rate * 30 / 32000;

	dp_buf_ultra_high = 26 * consume_rate * 30 / 32000;
	dp_buf_ultra_low = 25 * consume_rate * 30 / 32000;

	dp_buf_urgent_high = 12 * consume_rate * 30 / 32000;
	dp_buf_urgent_low = 11 * consume_rate * 30 / 32000;

	dev_info(dpi->dev, "dp_buf_sodi_high=%d, dp_buf_sodi_low=%d, dp_buf_preultra_high=%d, dp_buf_preultra_low=%d\n",
		 dp_buf_sodi_high, dp_buf_sodi_low, dp_buf_preultra_high, dp_buf_preultra_low);

	dev_info(dpi->dev, "dp_buf_ultra_high=%d, dp_buf_ultra_low=%d dp_buf_urgent_high=%d, dp_buf_urgent_low=%d\n",
		 dp_buf_ultra_high, dp_buf_ultra_low, dp_buf_urgent_high, dp_buf_urgent_low);

	mtk_dpi_write(dpi, DPI_BUF_SODI_HIGH, dp_buf_sodi_high);
	mtk_dpi_write(dpi, DPI_BUF_SODI_LOW, dp_buf_sodi_low);

	mtk_dpi_write(dpi, DPI_BUF_PREULTRA_HIGH, dp_buf_preultra_high);
	mtk_dpi_write(dpi, DPI_BUF_PREULTRA_LOW, dp_buf_preultra_low);

	mtk_dpi_write(dpi, DPI_BUF_ULTRA_HIGH, dp_buf_ultra_high);
	mtk_dpi_write(dpi, DPI_BUF_ULTRA_LOW, dp_buf_ultra_low);

	mtk_dpi_write(dpi, DPI_BUF_URGENT_HIGH, dp_buf_urgent_high);
	mtk_dpi_write(dpi, DPI_BUF_URGENT_LOW, dp_buf_urgent_low);

	mtk_dpi_write(dpi, DPI_MUTEX_VSYNC_SETTING, 0x1000F);
}

static int mtk_dpi_set_display_mode(struct mtk_dpi *dpi,
				    struct drm_display_mode *mode)
{
	struct mtk_dpi_polarities dpi_pol;
	struct mtk_dpi_sync_param hsync;
	struct mtk_dpi_sync_param vsync_lodd = { 0 };
	struct mtk_dpi_sync_param vsync_leven = { 0 };
	struct mtk_dpi_sync_param vsync_rodd = { 0 };
	struct mtk_dpi_sync_param vsync_reven = { 0 };
	struct videomode vm = { 0 };
	unsigned long pll_rate;
	unsigned int factor;
	unsigned int clksrc = TCK_26M;
	unsigned int rw_times = 0;
	int ret = 0;

	/* let pll_rate can fix the valid range of tvdpll (1G~2GHz) */
	factor = dpi->conf->cal_factor(mode->clock);
	drm_display_mode_to_videomode(mode, &vm);
	pll_rate = vm.pixelclock * factor;

	dev_dbg(dpi->dev, "Want PLL %lu Hz, pixel clock %lu Hz\n",
		pll_rate, vm.pixelclock);

	vm.pixelclock = mode->clock * 1000;

	if (vm.pixelclock < 70000000)
		clksrc = TVDPLL_D16;
	else if (vm.pixelclock < 200000000)
		clksrc = TVDPLL_D8;
	else
		clksrc = TVDPLL_D4;

	pll_rate = vm.pixelclock * (1 << clksrc);

	dev_info(dpi->dev, "%s pixel %lu clksrc %d pll_rate %lu\n",
		 __func__, vm.pixelclock, clksrc, pll_rate);

	ret = clk_set_rate(dpi->pclk_src[TVDPLL_PLL], pll_rate / 4);
	if (ret) {
		dev_info(dpi->dev, "%s cannot set pclk_src[TVDPLL_PLL]: err=%d\n",
			 __func__, ret);
	}

	ret = clk_set_parent(dpi->pclk, dpi->pclk_src[clksrc]);
	if (ret) {
		dev_info(dpi->dev, "%s clk_set_parent dp_intf->pclk: err=%d\n",
			 __func__, ret);
	}

	dev_info(dpi->dev, "%s dpintf->pclk_src[TVDPLL_PLL] =  %ld\n",
		 __func__, clk_get_rate(dpi->pclk_src[TVDPLL_PLL]));
	dev_info(dpi->dev, "%s dpintf->pclk_src[clksrc] =  %ld\n",
		 __func__, clk_get_rate(dpi->pclk_src[clksrc]));
	dev_info(dpi->dev, "%s dpintf->pclk =  %ld\n",
		 __func__, clk_get_rate(dpi->pclk));
	dev_info(dpi->dev, "%s dpintf->hf_fmm_ck =	%ld\n",
		 __func__, clk_get_rate(dpi->hf_fmm_ck));
	dev_info(dpi->dev, "%s dpintf->hf_fdp_ck =	%ld\n",
		 __func__, clk_get_rate(dpi->hf_fdp_ck));

	dpi_pol.ck_pol = MTK_DPI_POLARITY_FALLING;
	dpi_pol.de_pol = MTK_DPI_POLARITY_RISING;
	dpi_pol.hsync_pol = vm.flags & DISPLAY_FLAGS_HSYNC_HIGH ?
			    MTK_DPI_POLARITY_FALLING : MTK_DPI_POLARITY_RISING;
	dpi_pol.vsync_pol = vm.flags & DISPLAY_FLAGS_VSYNC_HIGH ?
			    MTK_DPI_POLARITY_FALLING : MTK_DPI_POLARITY_RISING;

	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	hsync.sync_width = vm.hsync_len / dpi->conf->pixels_per_iter;
	hsync.back_porch = vm.hback_porch / dpi->conf->pixels_per_iter;
	hsync.front_porch = vm.hfront_porch / dpi->conf->pixels_per_iter;

	hsync.shift_half_line = false;
	vsync_lodd.sync_width = vm.vsync_len;
	vsync_lodd.back_porch = vm.vback_porch;
	vsync_lodd.front_porch = vm.vfront_porch;
	vsync_lodd.shift_half_line = false;

	if (vm.flags & DISPLAY_FLAGS_INTERLACED &&
	    mode->flags & DRM_MODE_FLAG_3D_MASK) {
		vsync_leven = vsync_lodd;
		vsync_rodd = vsync_lodd;
		vsync_reven = vsync_lodd;
		vsync_leven.shift_half_line = true;
		vsync_reven.shift_half_line = true;
	} else if (vm.flags & DISPLAY_FLAGS_INTERLACED &&
		   !(mode->flags & DRM_MODE_FLAG_3D_MASK)) {
		vsync_leven = vsync_lodd;
		vsync_leven.shift_half_line = true;
	} else if (!(vm.flags & DISPLAY_FLAGS_INTERLACED) &&
		   mode->flags & DRM_MODE_FLAG_3D_MASK) {
		vsync_rodd = vsync_lodd;
	}
	mtk_dpi_sw_reset(dpi, true);
	mtk_dpi_config_pol(dpi, &dpi_pol);

	if (vm.hactive & 0x3)
		rw_times = ((vm.hactive >> 2) + 1) * vm.vactive;
	else
		rw_times = (vm.hactive >> 2) * vm.vactive;

	mtk_dpi_mask(dpi, DPI_BUF_RW_TIMES, rw_times, 0xffffffff);
	mtk_dpi_mask(dpi, DPI_BUF_CON0, BUF_BUF_EN, BUF_BUF_EN);
	mtk_dpi_mask(dpi, DPI_BUF_CON0, BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK,
		     BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK);
	mtk_dpi_set_golden_setting(dpi);

	mtk_dpi_config_hsync(dpi, &hsync);
	mtk_dpi_config_vsync_lodd(dpi, &vsync_lodd);
	mtk_dpi_config_vsync_rodd(dpi, &vsync_rodd);
	mtk_dpi_config_vsync_leven(dpi, &vsync_leven);
	mtk_dpi_config_vsync_reven(dpi, &vsync_reven);

	mtk_dpi_config_3d(dpi, !!(mode->flags & DRM_MODE_FLAG_3D_MASK));
	mtk_dpi_config_interface(dpi, !!(vm.flags &
					 DISPLAY_FLAGS_INTERLACED));
	if (vm.flags & DISPLAY_FLAGS_INTERLACED)
		mtk_dpi_config_fb_size(dpi, vm.hactive, vm.vactive >> 1);
	else
		mtk_dpi_config_fb_size(dpi, vm.hactive, vm.vactive);

	mtk_dpi_config_channel_limit(dpi);
	mtk_dpi_config_bit_num(dpi, dpi->bit_num);
	mtk_dpi_config_channel_swap(dpi, dpi->channel_swap);
	mtk_dpi_config_color_format(dpi, dpi->color_format);
	if (dpi->conf->support_direct_pin) {
		mtk_dpi_config_yc_map(dpi, dpi->yc_map);
		mtk_dpi_config_2n_h_fre(dpi);
		mtk_dpi_dual_edge(dpi);
		mtk_dpi_config_disable_edge(dpi);
	}
	if (dpi->conf->input_2pixel) {
		mtk_dpi_mask(dpi, DPI_CONFIG_1TNP, DPI_CONFIG_1T2P,
			     DPI_CONFIG_1TNP_MASK);
	}
	mtk_dpi_sw_reset(dpi, false);

	return 0;
}

static u32 *mtk_dpi_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						      struct drm_bridge_state *bridge_state,
						      struct drm_crtc_state *crtc_state,
						      struct drm_connector_state *conn_state,
						      unsigned int *num_output_fmts)
{
	struct mtk_dpi *dpi = bridge_to_dpi(bridge);
	u32 *output_fmts;

	*num_output_fmts = 0;

	if (!dpi->conf->output_fmts) {
		dev_info(dpi->dev, "output_fmts should not be null\n");
		return NULL;
	}

	output_fmts = kcalloc(dpi->conf->num_output_fmts, sizeof(*output_fmts),
			      GFP_KERNEL);
	if (!output_fmts)
		return NULL;

	*num_output_fmts = dpi->conf->num_output_fmts;

	memcpy(output_fmts, dpi->conf->output_fmts,
	       sizeof(*output_fmts) * dpi->conf->num_output_fmts);

	return output_fmts;
}

static u32 *mtk_dpi_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     u32 output_fmt,
						     unsigned int *num_input_fmts)
{
	u32 *input_fmts;

	*num_input_fmts = 0;

	input_fmts = kcalloc(1, sizeof(*input_fmts),
			     GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	*num_input_fmts = 1;
	input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;

	return input_fmts;
}

static int mtk_dpi_bridge_attach(struct drm_bridge *bridge,
		enum drm_bridge_attach_flags flags)
{
	struct mtk_dpi *dpi = bridge_to_dpi(bridge);

	if (dpi->conf->no_next_bridge)
		return 0;

	return drm_bridge_attach(bridge->encoder, dpi->next_bridge,
			&dpi->bridge, flags);
}

static void mtk_dpi_bridge_mode_set(struct drm_bridge *bridge,
			const struct drm_display_mode *mode,
			const struct drm_display_mode *adjusted_mode)
{
	unsigned int out_bus_format;
	struct drm_bridge_state *bridge_state;
	struct mtk_dpi *dpi = bridge_to_dpi(bridge);

	drm_mode_copy(&dpi->mode, adjusted_mode);
	dev_info(dpi->dev, "dpintf mode set, Htt:%d, Vtt:%d, Hact:%d, Vact:%d, fps:%d\n",
			dpi->mode.htotal, dpi->mode.vtotal,
			dpi->mode.hdisplay, dpi->mode.vdisplay, drm_mode_vrefresh(mode));

	bridge_state = drm_priv_to_bridge_state(bridge->base.state);

	out_bus_format = bridge_state->output_bus_cfg.format;

	if (out_bus_format == MEDIA_BUS_FMT_FIXED)
		if (dpi->conf->num_output_fmts)
			out_bus_format = dpi->conf->output_fmts[0];

	dev_info(dpi->dev, "dpintf, input format 0x%04x, output format 0x%04x\n",
		 bridge_state->input_bus_cfg.format,
		bridge_state->output_bus_cfg.format);

	dpi->output_fmt = out_bus_format;
	dpi->bit_num = MTK_DPI_OUT_BIT_NUM_8BITS;
	dpi->channel_swap = MTK_DPI_OUT_CHANNEL_SWAP_RGB;
	dpi->yc_map = MTK_DPI_OUT_YC_MAP_RGB;
	if (out_bus_format == MEDIA_BUS_FMT_YUYV8_1X16)
		dpi->color_format = MTK_DPI_COLOR_FORMAT_YCBCR_422;
	else
		dpi->color_format = MTK_DPI_COLOR_FORMAT_RGB;
}

static void mtk_dpi_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_dpi *dpi = bridge_to_dpi(bridge);

	mtk_dpi_power_off(dpi);

	if (dpi->pinctrl && dpi->pins_gpio)
		pinctrl_select_state(dpi->pinctrl, dpi->pins_gpio);
}

static void mtk_dpi_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_dpi *dpi = bridge_to_dpi(bridge);

	if (dpi->pinctrl && dpi->pins_dpi)
		pinctrl_select_state(dpi->pinctrl, dpi->pins_dpi);

	mtk_dpi_power_on(dpi);
	mtk_dpi_set_display_mode(dpi, &dpi->mode);
	mtk_dpi_enable(dpi);
}

static enum drm_mode_status
mtk_dpi_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_info *info,
			  const struct drm_display_mode *mode)
{
	struct mtk_dpi *dpi = bridge_to_dpi(bridge);

	if (mode->clock > dpi->conf->max_clock_khz)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static const struct drm_bridge_funcs mtk_dpi_bridge_funcs = {
	.attach = mtk_dpi_bridge_attach,
	.mode_set = mtk_dpi_bridge_mode_set,
	.mode_valid = mtk_dpi_bridge_mode_valid,
	.disable = mtk_dpi_bridge_disable,
	.enable = mtk_dpi_bridge_enable,
	.atomic_get_output_bus_fmts = mtk_dpi_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dpi_bridge_atomic_get_input_bus_fmts,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
};

unsigned int mtk_dpi_encoder_index(struct device *dev)
{
	struct mtk_dpi *dpi = dev_get_drvdata(dev);
	unsigned int encoder_index = drm_encoder_index(&dpi->encoder);

	dev_dbg(dev, "encoder index:%d\n", encoder_index);
	return encoder_index;
}

static int mtk_dpi_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_dpi *dpi = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &dpi->ddp_comp);
	if (ret < 0) {
		dev_info(dev, "Failed to register component %s: %d\n",
			 dev->of_node->full_name, ret);
		return ret;
	}

	dpi->mmsys_dev = priv->mmsys_dev;
	ret = drm_simple_encoder_init(drm_dev, &dpi->encoder,
				      DRM_MODE_ENCODER_TMDS);
	if (ret) {
		dev_info(dev, "Failed to initialize decoder: %d\n", ret);
		return ret;
	}

	dpi->encoder.possible_crtcs = mtk_drm_find_possible_crtc_by_comp(drm_dev, dpi->ddp_comp);

	ret = drm_bridge_attach(&dpi->encoder, &dpi->bridge, NULL,
				DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret)
		goto err_cleanup;

	return 0;

err_cleanup:
	drm_encoder_cleanup(&dpi->encoder);
	return ret;
}

static void mtk_dpi_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct mtk_dpi *dpi = dev_get_drvdata(dev);

	drm_encoder_cleanup(&dpi->encoder);
}

static const struct component_ops mtk_dpi_component_ops = {
	.bind = mtk_dpi_bind,
	.unbind = mtk_dpi_unbind,
};

static unsigned int mt8173_calculate_factor(int clock)
{
	if (clock <= 27000)
		return 3 << 4;
	else if (clock <= 84000)
		return 3 << 3;
	else if (clock <= 167000)
		return 3 << 2;
	else
		return 3 << 1;
}

static unsigned int mt2701_calculate_factor(int clock)
{
	if (clock <= 64000)
		return 4;
	else if (clock <= 128000)
		return 2;
	else
		return 1;
}

static unsigned int mt8183_calculate_factor(int clock)
{
	if (clock <= 27000)
		return 8;
	else if (clock <= 167000)
		return 4;
	else
		return 2;
}

static unsigned int mt8195_dpintf_calculate_factor(int clock)
{
	if (clock < 70000)
		return 4;
	else if (clock < 200000)
		return 2;
	else
		return 1;
}

static unsigned int mt8678_dpintf_calculate_factor(int clock)
{
	if (clock < 70000)
		return 4;
	else if (clock < 200000)
		return 2;
	else
		return 1;
}

static const u32 mt8173_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
};

static const u32 mt8183_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_2X12_LE,
	MEDIA_BUS_FMT_RGB888_2X12_BE,
};

static const u32 mt8195_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static const u32 mt8678_output_fmts[] = {
#if YUV422_PRIORITY
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
#else
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
#endif
};

static const struct mtk_dpi_conf mt8173_conf = {
	.cal_factor = mt8173_calculate_factor,
	.reg_h_fre_con = 0xe0,
	.max_clock_khz = 300000,
	.output_fmts = mt8173_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8173_output_fmts),
	.pixels_per_iter = 1,
	.is_ck_de_pol = true,
	.swap_input_support = true,
	.support_direct_pin = true,
	.dimension_mask = HPW_MASK,
	.hvsize_mask = HSIZE_MASK,
	.channel_swap_shift = CH_SWAP,
	.yuv422_en_bit = YUV422_EN,
	.csc_enable_bit = CSC_ENABLE,
};

static const struct mtk_dpi_conf mt2701_conf = {
	.cal_factor = mt2701_calculate_factor,
	.reg_h_fre_con = 0xb0,
	.edge_sel_en = true,
	.max_clock_khz = 150000,
	.output_fmts = mt8173_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8173_output_fmts),
	.pixels_per_iter = 1,
	.is_ck_de_pol = true,
	.swap_input_support = true,
	.support_direct_pin = true,
	.dimension_mask = HPW_MASK,
	.hvsize_mask = HSIZE_MASK,
	.channel_swap_shift = CH_SWAP,
	.yuv422_en_bit = YUV422_EN,
	.csc_enable_bit = CSC_ENABLE,
};

static const struct mtk_dpi_conf mt8183_conf = {
	.cal_factor = mt8183_calculate_factor,
	.reg_h_fre_con = 0xe0,
	.max_clock_khz = 100000,
	.output_fmts = mt8183_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8183_output_fmts),
	.pixels_per_iter = 1,
	.is_ck_de_pol = true,
	.swap_input_support = true,
	.support_direct_pin = true,
	.dimension_mask = HPW_MASK,
	.hvsize_mask = HSIZE_MASK,
	.channel_swap_shift = CH_SWAP,
	.yuv422_en_bit = YUV422_EN,
	.csc_enable_bit = CSC_ENABLE,
};

static const struct mtk_dpi_conf mt8186_conf = {
	.cal_factor = mt8183_calculate_factor,
	.reg_h_fre_con = 0xe0,
	.max_clock_khz = 150000,
	.output_fmts = mt8183_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8183_output_fmts),
	.edge_cfg_in_mmsys = true,
	.pixels_per_iter = 1,
	.is_ck_de_pol = true,
	.swap_input_support = true,
	.support_direct_pin = true,
	.dimension_mask = HPW_MASK,
	.hvsize_mask = HSIZE_MASK,
	.channel_swap_shift = CH_SWAP,
	.yuv422_en_bit = YUV422_EN,
	.csc_enable_bit = CSC_ENABLE,
};

static const struct mtk_dpi_conf mt8192_conf = {
	.cal_factor = mt8183_calculate_factor,
	.reg_h_fre_con = 0xe0,
	.max_clock_khz = 150000,
	.output_fmts = mt8183_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8183_output_fmts),
	.pixels_per_iter = 1,
	.is_ck_de_pol = true,
	.swap_input_support = true,
	.support_direct_pin = true,
	.dimension_mask = HPW_MASK,
	.hvsize_mask = HSIZE_MASK,
	.channel_swap_shift = CH_SWAP,
	.yuv422_en_bit = YUV422_EN,
	.csc_enable_bit = CSC_ENABLE,
};

static const struct mtk_dpi_conf mt8195_dpintf_conf = {
	.cal_factor = mt8195_dpintf_calculate_factor,
	.max_clock_khz = 600000,
	.output_fmts = mt8195_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8195_output_fmts),
	.pixels_per_iter = 4,
	.input_2pixel = true,
	.dimension_mask = DPINTF_HPW_MASK,
	.hvsize_mask = DPINTF_HSIZE_MASK,
	.channel_swap_shift = DPINTF_CH_SWAP,
	.yuv422_en_bit = DPINTF_YUV422_EN,
	.csc_enable_bit = DPINTF_CSC_ENABLE,
};

static const struct mtk_dpi_conf mt8678_dpintf_conf0 = {
	.cal_factor = mt8678_dpintf_calculate_factor,
	.max_clock_khz = 600000,
	.output_fmts = mt8678_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8678_output_fmts),
	.pixels_per_iter = 4,
	.input_2pixel = true,
	.dimension_mask = DPINTF_HPW_MASK,
	.hvsize_mask = DPINTF_HSIZE_MASK,
	.channel_swap_shift = DPINTF_CH_SWAP,
	.yuv422_en_bit = DPINTF_YUV422_EN,
	.csc_enable_bit = DPINTF_CSC_ENABLE,
};

static const struct mtk_dpi_conf mt8678_dpintf_conf1 = {
	.cal_factor = mt8678_dpintf_calculate_factor,
	.max_clock_khz = 600000,
	.output_fmts = mt8678_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt8678_output_fmts),
	.pixels_per_iter = 4,
	.input_2pixel = true,
	.dimension_mask = DPINTF_HPW_MASK,
	.hvsize_mask = DPINTF_HSIZE_MASK,
	.channel_swap_shift = DPINTF_CH_SWAP,
	.yuv422_en_bit = DPINTF_YUV422_EN,
	.csc_enable_bit = DPINTF_CSC_ENABLE,
	.no_next_bridge = true,
};

static void mt8678_get_clk(struct mtk_dpi *dp_intf)
{
	dev_info(dp_intf->dev, "%s+\n", __func__);

	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[TCK_26M] = devm_clk_get(dp_intf->dev, "DPI_26M");
	dp_intf->pclk_src[TVDPLL_D4] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[TVDPLL_D8] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[TVDPLL_D16] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[TVDPLL_PLL] = devm_clk_get(dp_intf->dev, "DPI_CK");

	if (IS_ERR(dp_intf->pclk) ||
	    IS_ERR(dp_intf->pclk_src[TCK_26M]) ||
		IS_ERR(dp_intf->pclk_src[TVDPLL_D4]) ||
		IS_ERR(dp_intf->pclk_src[TVDPLL_D8]) ||
		IS_ERR(dp_intf->pclk_src[TVDPLL_D16]) ||
		IS_ERR(dp_intf->pclk_src[TVDPLL_PLL]))
		dev_info(dp_intf->dev, "Failed to get pclk andr src clock, -%d-%d-%d-%d-%d-%d-\n",
			 IS_ERR(dp_intf->pclk),
			IS_ERR(dp_intf->pclk_src[TCK_26M]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_D4]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_D8]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_D16]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_PLL]));
}

/*  dp intf api for drm drv start */
int mtk_dp_intf_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = NULL;

	if (IS_ERR_OR_NULL(comp) || IS_ERR_OR_NULL(comp->regs)) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("(0x0000) DPI_EN                 =0x%x\n",
		readl(baddr + DPI_EN));
	DDPDUMP("(0x0F00) DP_PATTERN_CTRL0      =0x%x\n",
		readl(baddr + DP_PATTERN_CTRL0));

	return 0;
}

int mtk_dp_intf_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("en=%d\n", DISP_REG_GET_FIELD(BIT(0), baddr + DPI_EN));
	DDPDUMP("== End %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	return 0;
}

/*  dp intf api for drm drv end */

/*  dp intf ddp comp functions start */
static unsigned long long mtk_dpintf_get_frame_hrt_bw_base
	(struct mtk_drm_crtc *mtk_crtc, struct mtk_dpi *dp_intf)
{
	unsigned long long bw_base;
	int hact;
	int vtotal;
	int vact;
	int vrefresh;
	u32 bpp = 3;

	/* for the case dpintf not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->base.state))
		return 1;

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	bw_base = (unsigned long long)div_u64(vact * hact * vrefresh * 4, 1000);
	bw_base = div_u64(bw_base * vtotal, vact);
	bw_base = div_u64(bw_base, 1000);

	if (dp_intf->dp_intf_bw != bw_base) {
		dp_intf->dp_intf_bw = bw_base;
		DDPMSG("%s Frame Bw:%llu, bpp:%d\n", __func__, bw_base, bpp);
	}
	return bw_base;
}

static unsigned long long mtk_dpintf_get_frame_hrt_bw_base_by_mode
	(struct mtk_drm_crtc *mtk_crtc, struct mtk_dpi *dp_intf)
{
	unsigned long long base_bw;
	unsigned int hact;
	unsigned int vtotal;
	unsigned int vact;
	unsigned int vrefresh;
	u32 bpp = 3;

	/* for the case dpintf not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->avail_modes))
		return 1;

	hact = mtk_crtc->avail_modes->hdisplay;
	vtotal = mtk_crtc->avail_modes->vtotal;
	vact = mtk_crtc->avail_modes->vdisplay;
	vrefresh = drm_mode_vrefresh(mtk_crtc->avail_modes);
	base_bw = (unsigned long long)div_u64(vact * hact * vrefresh * 4, 1000);
	base_bw = div_u64(base_bw * vtotal, vact);
	base_bw = div_u64(base_bw, 1000);
	if (dp_intf->dp_intf_bw != base_bw) {
		dp_intf->dp_intf_bw = base_bw;
		DDPMSG("%s Frame Bw:%llu, bpp:%d\n", __func__, base_bw, bpp);
	}

	return base_bw;
}

static inline struct mtk_dpi *comp_to_dp_intf(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dpi, ddp_comp);
}

static int mtk_dpintf_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			     enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_dpi *dp_intf = comp_to_dp_intf(comp);

	switch (cmd) {
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dpintf_get_frame_hrt_bw_base(mtk_crtc, dp_intf);
	}
		break;
	case GET_FRAME_HRT_BW_BY_MODE:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dpintf_get_frame_hrt_bw_base_by_mode(mtk_crtc, dp_intf);
	}
		break;
	case CONNECTOR_IS_ENABLE:
	{
		unsigned int *connector_enable =
			(unsigned int *)params;
		if (g_mtk_dp) {
			*connector_enable = g_mtk_dp->dp_ready;
			DDPMSG("connect status:%d\n", g_mtk_dp->dp_ready);
		}
	}
		break;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	case SET_CRTC_ID:
	{
		DDPMSG("%s set %s possible crtcs 0x%x\n", __func__,
			mtk_dump_comp_str(comp), *(unsigned int *)params);
		dp_intf->encoder.possible_crtcs = *(unsigned int *)params;
	}
		break;
#endif
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dp_intf_funcs = {
	.io_cmd = mtk_dpintf_io_cmd,
};

static irqreturn_t mtk_dp_intf_irq_status(int irq, void *dev_id)
{
	struct mtk_dpi *dpi = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc = dpi->ddp_comp.mtk_crtc;

	status = readl(dpi->regs + DPI_INTSTA);

	status &= 0xf;
	if (status) {
		mtk_dpi_mask(dpi, DPI_INTSTA, 0, status);
		if (mtk_crtc && (status & INT_VSYNC_STA))
			mtk_crtc_vblank_irq(&mtk_crtc->base);

		if (status & INT_UNDERFLOW_STA)
			DDPDBG("[E]%s dpintf underflow!\n", __func__);
	}

	return IRQ_HANDLED;
}

/*  dp intf ddp comp functions end */

static int mtk_dpi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dpi *dpi;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPMSG("%s %s +\n", __func__, dev_name(dev));
	dpi = devm_kzalloc(dev, sizeof(*dpi), GFP_KERNEL);
	if (!dpi)
		return -ENOMEM;

	dpi->dev = dev;
	dpi->conf = (struct mtk_dpi_conf *)of_device_get_match_data(dev);
	dpi->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	if (!dpi->conf->no_next_bridge) {
		dpi->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 0, 0);
		if (IS_ERR(dpi->next_bridge)) {
			dev_dbg(dev, "Can not find next_bridge");
			return -EPROBE_DEFER;
		}
		dev_info(dev, "Found dp tx bridge node: %pOF\n", dpi->next_bridge->of_node);
	}

	dpi->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dpi->pinctrl)) {
		dpi->pinctrl = NULL;
		dev_dbg(&pdev->dev, "Cannot find pinctrl!\n");
	}
	if (dpi->pinctrl) {
		dpi->pins_gpio = pinctrl_lookup_state(dpi->pinctrl, "sleep");
		if (IS_ERR(dpi->pins_gpio)) {
			dpi->pins_gpio = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl idle!\n");
		}
		if (dpi->pins_gpio)
			pinctrl_select_state(dpi->pinctrl, dpi->pins_gpio);

		dpi->pins_dpi = pinctrl_lookup_state(dpi->pinctrl, "default");
		if (IS_ERR(dpi->pins_dpi)) {
			dpi->pins_dpi = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl active!\n");
		}
	}
	dpi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dpi->regs))
		return -EPROBE_DEFER;

	dpi->hf_fmm_ck = devm_clk_get(dev, "hf_fmm_ck");
	if (IS_ERR(dpi->hf_fmm_ck)) {
		ret = PTR_ERR(dpi->hf_fmm_ck);
		dev_info(dev, "Failed to get hf_fmm_ck clock: %d\n", ret);
		return ret;
	}
	dpi->hf_fdp_ck = devm_clk_get(dev, "hf_fdp_ck");
	if (IS_ERR(dpi->hf_fdp_ck)) {
		ret = PTR_ERR(dpi->hf_fdp_ck);
		dev_info(dev, "Failed to get hf_fdp_ck clock: %d\n", ret);
		return ret;
	}

	mt8678_get_clk(dpi);

	ret = clk_prepare_enable(dpi->pclk);
	if (ret < 0)
		dev_info(dev, "%s Failed to enable pclk:%d\n",
			 __func__, ret);

	ret = clk_set_parent(dpi->pclk, dpi->pclk_src[TCK_26M]);
	if (ret < 0)
		dev_info(dev, "%s Failed to clk_set_parent:%d\n",
			 __func__, ret);
	dev_info(dev, "%s dpintf->pclk:%ld\n",
		 __func__, clk_get_rate(dpi->pclk));

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DP_INTF);
	if ((int)comp_id < 0) {
		dev_info(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dpi->ddp_comp, comp_id, &mtk_dp_intf_funcs);
	if (ret) {
		dev_info(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	dpi->irq = platform_get_irq(pdev, 0);
	if (dpi->irq < 0)
		return dpi->irq;

	irq_set_status_flags(dpi->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dpi->irq, mtk_dp_intf_irq_status,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), dpi);
	if (ret) {
		dev_info(&pdev->dev, "failed to request mediatek dp intf irq\n");
		ret = -EPROBE_DEFER;
		return ret;
	}

	platform_set_drvdata(pdev, dpi);

	dpi->bridge.funcs = &mtk_dpi_bridge_funcs;
	dpi->bridge.of_node = dev->of_node;
	dpi->bridge.type = DRM_MODE_CONNECTOR_DPI;

	ret = devm_drm_bridge_add(dev, &dpi->bridge);
	if (ret)
		return ret;

	ret = component_add(dev, &mtk_dpi_component_ops);
	if (ret) {
		dev_info(dev, "Failed to component add: %d\n", ret);
		return -EPROBE_DEFER;
	}

	if (comp_id == DDP_COMPONENT_DP_INTF0)
		g_mtk_dpi[0] = dpi;
	else if (comp_id == DDP_COMPONENT_DP_INTF1)
		g_mtk_dpi[1] = dpi;

	DDPMSG("%s %s comp %s-\n", __func__, dev_name(dev), mtk_dump_comp_str_id(comp_id));

	return 0;
}

static void mtk_dpi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dpi_component_ops);
}

static const struct of_device_id mtk_dpi_of_ids[] = {
	{ .compatible = "mediatek,mt2701-dpi", .data = &mt2701_conf },
	{ .compatible = "mediatek,mt8173-dpi", .data = &mt8173_conf },
	{ .compatible = "mediatek,mt8183-dpi", .data = &mt8183_conf },
	{ .compatible = "mediatek,mt8186-dpi", .data = &mt8186_conf },
	{ .compatible = "mediatek,mt8188-dp-intf", .data = &mt8195_dpintf_conf },
	{ .compatible = "mediatek,mt8192-dpi", .data = &mt8192_conf },
	{ .compatible = "mediatek,mt8195-dp-intf", .data = &mt8195_dpintf_conf },
	{ .compatible = "mediatek,disp1_dp_intf0", .data = &mt8678_dpintf_conf0 },
	{ .compatible = "mediatek,disp1_dp_intf1", .data = &mt8678_dpintf_conf1 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_dpi_of_ids);

#ifdef CONFIG_PM_SLEEP
static int mtk_dpi_suspend(struct device *dev)
{
	struct mtk_dpi *mtk_dpi = dev_get_drvdata(dev);

	if (!mtk_dpi) {
		pr_info("[DPI] suspend, dpi not initial\n");
		return 0;
	}

	if (mtk_dpi->suspend) {
		pr_info("[DPI] %s already suspend\n", __func__);
		return 0;
	}

	dev_info(mtk_dpi->dev, "[DPI] suspend +\n");

	clk_disable_unprepare(mtk_dpi->pclk);

	mtk_dpi->suspend = true;

	dev_info(mtk_dpi->dev, "[DPI] suspend -\n");

	return 0;
}

static int mtk_dpi_resume(struct device *dev)
{
	struct mtk_dpi *mtk_dpi = dev_get_drvdata(dev);
	int ret;

	if (!mtk_dpi) {
		pr_info("[DPI] resume, dpi not initial\n");
		return 0;
	}

	if (!mtk_dpi->suspend) {
		pr_info("[DPI] %s already resume\n", __func__);
		return 0;
	}

	dev_info(mtk_dpi->dev, "[DPI] resume +\n");

	ret = clk_prepare_enable(mtk_dpi->pclk);
	if (ret < 0)
		dev_info(mtk_dpi->dev, "%s Failed to enable pclk:%d\n",
			 __func__, ret);

	mtk_dpi->suspend = false;

	dev_info(mtk_dpi->dev, "[DPI] resume -\n");

	return 0;
}

void mtk_drm_dpi_suspend(void)
{
	struct mtk_dpi *mtk_dpi = NULL;
	int i = 0;

	DDPMSG("%s +\n", __func__);

	for (i = 0; i < MAX_DPI; i++) {
		mtk_dpi = g_mtk_dpi[i];

		if (!mtk_dpi || !mtk_dpi->dev) {
			DDPMSG("[E][DPI] %s dp-%d not initial\n", __func__, i);
			continue;
		}

		mtk_dpi_suspend(mtk_dpi->dev);
	}

	DDPMSG("%s -\n", __func__);
}

void mtk_drm_dpi_resume(void)
{
	struct mtk_dpi *mtk_dpi = NULL;
	int i = 0;

	DDPMSG("%s +\n", __func__);

	for (i = 0; i < MAX_DPI; i++) {
		mtk_dpi = g_mtk_dpi[i];

		if (!mtk_dpi || !mtk_dpi->dev) {
			DDPMSG("[E][DPI] %s dp-%d not initial\n", __func__, i);
			continue;
		}

		mtk_dpi_resume(mtk_dpi->dev);
	}

	DDPMSG("%s -\n", __func__);
}
#endif

//static SIMPLE_DEV_PM_OPS(mtk_dpi_pm_ops,
//		mtk_dpi_suspend, mtk_dpi_resume);

struct platform_driver mtk_dp_intf_driver = {
	.probe = mtk_dpi_probe,
	.remove_new = mtk_dpi_remove,
	.driver = {
		.name = "mediatek-dpi",
		.of_match_table = mtk_dpi_of_ids,
//		.pm = &mtk_dpi_pm_ops,
	},
};
