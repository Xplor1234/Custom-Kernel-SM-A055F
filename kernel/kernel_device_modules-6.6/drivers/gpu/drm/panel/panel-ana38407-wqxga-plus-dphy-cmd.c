// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define FRAME_WIDTH    (2960)
#define HFP            (50)
#define HSA            (100)//(FRAME_WIDTH)
#define HBP            (50)
#define HTOTAL         (FRAME_WIDTH + HFP + HSA + HBP)
#define FRAME_HEIGHT   (1848)
#define VFP            (16)
#define VSA            (FRAME_HEIGHT)
#define VBP            (8)
#define VTOTAL         (FRAME_HEIGHT + VFP + VSA + VBP)
#define FRAME_TOTAL    (HTOTAL * VTOTAL)
#define PLL_CLOCK      (1524/2)
#define VREFRESH_DEF   (120)
#define VREFRESH_60    (60)
#define VREFRESH_90    (90)
#define VREFRESH_30    (30)
#define VREFRESH_24    (24)
#define VREFRESH_10    (10)
#define CLK_DEF_X10    ((FRAME_TOTAL * VREFRESH_DEF) / 100)
#define CLK_60_X10     ((FRAME_TOTAL * VREFRESH_60) / 100)
#define CLK_90_X10     ((FRAME_TOTAL * VREFRESH_90) / 100)
#define CLK_30_X10     ((FRAME_TOTAL * VREFRESH_30) / 100)
#define CLK_24_X10     ((FRAME_TOTAL * VREFRESH_24) / 100)
#define CLK_10_X10     ((FRAME_TOTAL * VREFRESH_10) / 100)
#define CLK_DEF		(((CLK_DEF_X10 % 10) != 0) ?             \
			(CLK_DEF_X10 / 10 + 1) : (CLK_DEF_X10 / 10))
#define CLK_90		(((CLK_90_X10 % 10) != 0) ?              \
			(CLK_90_X10 / 10 + 1) : (CLK_90_X10 / 10))
#define CLK_60		(((CLK_60_X10 % 10) != 0) ?              \
			(CLK_60_X10 / 10 + 1) : (CLK_60_X10 / 10))
#define CLK_30		(((CLK_30_X10 % 10) != 0) ?              \
			(CLK_30_X10 / 10 + 1) : (CLK_30_X10 / 10))
#define CLK_24		(((CLK_24_X10 % 10) != 0) ?              \
			(CLK_24_X10 / 10 + 1) : (CLK_24_X10 / 10))
#define CLK_10		(((CLK_10_X10 % 10) != 0) ?              \
			(CLK_10_X10 / 10 + 1) : (CLK_10_X10 / 10))

static atomic_t current_backlight;
static struct mtk_panel_para_table bl_tb0[] = {
		{3, { 0x51, 0x0f, 0xff}},
	};

static struct mtk_panel_para_table bl_elvss_tb[] = {
		{3, { 0x51, 0x0f, 0xff}},
		{2, { 0x83, 0xff}},
	};

static struct mtk_panel_para_table elvss_tb[] = {
		{2, { 0x83, 0xff}},
	};
#if 0
unsigned int nt37801_cmd_fhd_buf_thresh[14] = {
	 896, 1792, 2688, 3584, 4480,
	5376, 6272, 6720, 7168, 7616,
	7744, 7872, 8000, 8064};
unsigned int nt37801_cmd_fhd_range_min_qp[15] = {
	0, 0, 1, 1, 3, 3,
	3, 3, 3, 4, 5, 5,
	5, 8, 12};
unsigned int nt37801_cmd_fhd_range_max_qp[15] = {
	4, 4, 5, 6, 7, 7,
	7, 8, 9, 10, 10, 11,
	11, 12, 13};
int nt37801_cmd_fhd_range_bpg_ofs[15] = {
	2, 0, 0, -2, -4, -6,
	-8, -8, -8, -10, -10, -10,
	-12, -12, -12};
#endif
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;
	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
//	char bl_tb[] = {0x51, 0x0f, 0xff};
//	unsigned int level = 0;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);

	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    // TODO : TCON_RDY Check

/* #1. CMD: POWER ON PRE SETTING */
	/* 1. Sleep Out(11h) */
	lcm_dcs_write_seq_static(ctx, 0x11);
//-------------------------------------
	/* 2. CMD: TCON_INTR_SETTING */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x02);   /* 0x00(Active High), 0x02(Active Low) */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x0F, 0x00, 0x00, 0x00, 0x14, 0x46, 0x81);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x13);   /* 0x00(Active High), 0x02(Active Low) */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x0F, 0x00, 0x00, 0x00, 0x08, 0xCF, 0x81);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x05);   /* 0x00(Active High), 0x02(Active Low) */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x0F, 0x00, 0x00, 0x00, 0x09, 0xCD, 0x81);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5);

	/* 3. CMD: TE_ON */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* 4. TSP_SYNC_SETTING */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x0B, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0xCC);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/*: 5. DSC SETTING */
#if 0
	/* W=2800, H=1752, Slice_W=1400, Slice_H=12*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x06, 0xD8, 0x0A, 0xF0,
		0x00, 0x49, 0x05, 0x78, 0x05, 0x78, 0x02, 0x00, 0x03, 0xBD,
		0x00, 0x20, 0x0A, 0x4A, 0x00, 0x13, 0x00, 0x0C, 0x01, 0x56,
		0x00, 0x88, 0x18, 0x00, 0x10, 0xD0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54,
		0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02,
		0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA,
		0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
#else
	/* Slice width 1400, height 73 / 2 Slice mode */
	/* DSC v1.1 PPS Setting */
	/* Compression Enable */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	/* PPS */
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x07, 0x38, 0x0B, 0x90,
		0x00, 0x4D, 0x05, 0xC8, 0x05, 0xC8, 0x02, 0x00, 0x03, 0xE5,
		0x00, 0x20, 0x0B, 0x07, 0x00, 0x14, 0x00, 0x0C, 0x01, 0x44,
		0x00, 0x7A, 0x18, 0x00, 0x10, 0xD0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54,
		0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02,
		0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA,
		0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
			);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
#endif /* 0 */

	/* 6. DIA SETTING */
#if 0
	lcm_dcs_write_seq_static(ctx, 0x91, 0x02); // If DIA ON
#else
	lcm_dcs_write_seq_static(ctx, 0x91, 0x00);  // If DIA OFF
#endif

	/* 7. BRIGNTNESS SETTING */
	/* 7-1. VRR_SETTING : 120hz */
#if 1
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x00);   /* 120hz */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x010, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x80, 0x00, 0x00, 0x00);     /* TSP Vsync for 120hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
#else
	/* 7-1. VRR_SETTING : 60hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x10); /* 60hz */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x010, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0xAA, 0xAA, 0xAA, 0xAA);     /* TSP Vsync for 120hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
#endif

	/* 7-2. BRIGHTNESS DIMMING SETTING */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	//---------------------------
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);  /* IF MODE NORMAL AND NORMAL TRANSITION THEN */
	//lcm_dcs_write_seq_static(ctx, 0x53, 0x28);  /* IF MODE NORMAL AND SMOOTH TRANSITION THEN */
	//lcm_dcs_write_seq_static(ctx, 0x53, 0xE0);  /* IF MODE HBM AND NORMAL TRANSITION THEN */
	//lcm_dcs_write_seq_static(ctx, 0x53, 0xE8);  /* IF MODE HBM AND SMOOTH TRANSITION THEN */
	//---------------------------
	lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0x0F);  /* 0x07 0x0F: 420 nit */
	//lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x0A);    /* 0x00 0x0A: 2 nit */
	//lcm_dcs_write_seq_static(ctx, 0x51, 0x05, 0x55);    /* 0x05 0x55: 500 nit */
	//lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);    /* 0x07 0xFFF: 650 nit @ OPR 10% (HBM) */
	/* UPDATE BRIGHTNESS*/
	//---------------------------
	/* 7-2-1. HBM_FlatZ_SETTING */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x0A, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0xE5, 0x84, 0xFF, 0x1D, 0x00,
		0x00, 0xE6, 0xCC, 0x00, 0x68, 0x46); /* HBM IRC MODE : Flat Gamma Z */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

    /* 8. SP SETTING */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x68, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x01);   /* 0x01(SP Setting ON), 0x00(SP setting OFF) */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Delay 50ms */
	msleep(50);

	/* 9. {VRR_SETTING} 120hz */
#if 1
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x00);   /* 120hz */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x010, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x80, 0x00, 0x00, 0x00);    /* TSP Vsync for 120hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
#else
	/* 9. {VRR_SETTING} 60hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x10);  /* 60hz */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x010, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0xAA, 0xAA, 0xAA, 0xAA);    /* TSP Vsync for 120hz */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
#endif

	/* #2. CMD: POWER ON POST SETTING */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x29);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	udelay(1000);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
						"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
							__func__, PTR_ERR(ctx->bias_pos));
	} else {
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
	} else {
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}

	udelay(1000);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = CLK_DEF,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

static const struct drm_display_mode mode_90 = {
	.clock = CLK_90,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

static const struct drm_display_mode mode_60 = {
	.clock = CLK_60,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

static const struct drm_display_mode mode_30 = {
	.clock = CLK_30,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

static const struct drm_display_mode mode_24 = {
	.clock = CLK_24,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

static const struct drm_display_mode mode_10 = {
	.clock = CLK_10,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

enum SWITCH_MODE_DELAY switch_mode_delay_table[DISPLAY_MODE_NUM][DISPLAY_MODE_NUM] = {
	/*DISPLAY_MODE_0 ... DISPLAY_MODE_11*/
	//mode switch, TE really switch at (N + x)th TE, the x means delay_x
	//from [row] fps to [column] fps
	{DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_1,
		DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_1,},// mode_0
	{DELAY_2, DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1,
		DELAY_2, DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1,},// mode_1
	{DELAY_0, DELAY_2, DELAY_0, DELAY_1, DELAY_1, DELAY_1,
		DELAY_0, DELAY_2, DELAY_0, DELAY_1, DELAY_1, DELAY_1,},// mode_2
	{DELAY_0, DELAY_2, DELAY_1, DELAY_0, DELAY_1, DELAY_1,
		DELAY_0, DELAY_2, DELAY_1, DELAY_0, DELAY_1, DELAY_1,},// mode_3
	{DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_0, DELAY_1,
		DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_0, DELAY_1,},// mode_4
	{DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_0,
		DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_0,},// mode_5

	{DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_1,
		DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_1,},// mode_6
	{DELAY_2, DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1,
		DELAY_2, DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1,},// mode_7
	{DELAY_0, DELAY_2, DELAY_0, DELAY_1, DELAY_1, DELAY_1,
		DELAY_0, DELAY_2, DELAY_0, DELAY_1, DELAY_1, DELAY_1,},// mode_8
	{DELAY_0, DELAY_2, DELAY_1, DELAY_0, DELAY_1, DELAY_1,
		DELAY_0, DELAY_2, DELAY_1, DELAY_0, DELAY_1, DELAY_1,},// mode_9
	{DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_0, DELAY_1,
		DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_0, DELAY_1,},// mode_10
	{DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_0,
		DELAY_0, DELAY_2, DELAY_1, DELAY_1, DELAY_1, DELAY_0,},// mode_11
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x0F, 0xff};
	unsigned int scaled_level;

	scaled_level = (level * 4095) / 255;
	bl_tb[1] = (scaled_level >> 8) & 0x0F;
	bl_tb[2] = scaled_level & 0xFF;

//	bl_tb[1] = (level >> 8) & 0xF;
//	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	atomic_set(&current_backlight, level);
	return 0;
}


static int lcm_set_bl_elvss_cmdq(void *dsi, dcs_grp_write_gce cb, void *handle,
		struct mtk_bl_ext_config *bl_ext_config)
{
	int pulses;

	if (!cb)
		return -1;

	pulses = bl_ext_config->elvss_pn;

	if ((bl_ext_config->cfg_flag & (0x1<<SET_BACKLIGHT_LEVEL)) &&
			(bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN))) {
		pr_info("%s backlight = -%d\n", __func__, bl_ext_config->backlight_level);
		bl_elvss_tb[0].para_list[1] = (bl_ext_config->backlight_level >> 8) & 0xf;
		bl_elvss_tb[0].para_list[2] = (bl_ext_config->backlight_level) & 0xFF;
		pr_info("%s elvss = -%d\n", __func__, pulses);
		bl_elvss_tb[1].para_list[1] = (u8)((1<<7)|pulses);
		atomic_set(&current_backlight, bl_ext_config->backlight_level);
		cb(dsi, handle, bl_elvss_tb, ARRAY_SIZE(bl_elvss_tb));
	} else if ((bl_ext_config->cfg_flag & (0x1<<SET_BACKLIGHT_LEVEL))) {
		pr_info("%s backlight = -%d\n", __func__, bl_ext_config->backlight_level);
		bl_tb0[0].para_list[1] = (bl_ext_config->backlight_level >> 8) & 0xf;
		bl_tb0[0].para_list[2] = (bl_ext_config->backlight_level) & 0xFF;
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		atomic_set(&current_backlight, bl_ext_config->backlight_level);
	} else if ((bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN))) {

		pr_info("%s elvss = -%d\n", __func__, pulses);
		elvss_tb[0].para_list[1] = (u8)((1<<7)|pulses);
		cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));

	}

	return 0;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = PLL_CLOCK,
	.lcm_degree = 270,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = 77,
		.slice_width = (FRAME_WIDTH/2),
		.chunk_size = 1480,
		.xmit_delay = 512,
		.dec_delay = 997,
		.scale_value = 32,
		.increment_interval = 2823,
		.decrement_interval = 20,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 324,
		.slice_bpg_offset = 122,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
/*
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37801_cmd_fhd_buf_thresh,
			.range_min_qp = nt37801_cmd_fhd_range_min_qp,
			.range_max_qp = nt37801_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37801_cmd_fhd_range_bpg_ofs,
			},
*/
		},
	.data_rate = PLL_CLOCK * 2,
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = PLL_CLOCK + 1,
	},
	.dyn_fps = {
		.vact_timing_fps = 120,
	},
	.real_te_duration = 8333,
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = PLL_CLOCK,
	.lcm_degree = 270,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = 73,
		.slice_width = (FRAME_WIDTH/2),
		.chunk_size = 1400,
		.xmit_delay = 512,
		.dec_delay = 957,
		.scale_value = 32,
		.increment_interval = 2634,
		.decrement_interval = 19,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 342,
		.slice_bpg_offset = 136,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
/*
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37801_cmd_fhd_buf_thresh,
			.range_min_qp = nt37801_cmd_fhd_range_min_qp,
			.range_max_qp = nt37801_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37801_cmd_fhd_range_bpg_ofs,
			},
*/
		},
	.data_rate = PLL_CLOCK * 2,
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = PLL_CLOCK + 1,
	},
	.dyn_fps = {
		.vact_timing_fps = 120,
	},
	.real_te_duration = 11111,
	.mode_switch_delay = 2,
};

static struct mtk_panel_params ext_params_60hz = {
	.pll_clk = PLL_CLOCK,
	.lcm_degree = 270,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = 73,
		.slice_width = (FRAME_WIDTH/2),
		.chunk_size = 1400,
		.xmit_delay = 512,
		.dec_delay = 957,
		.scale_value = 32,
		.increment_interval = 2634,
		.decrement_interval = 19,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 342,
		.slice_bpg_offset = 136,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
/*
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37801_cmd_fhd_buf_thresh,
			.range_min_qp = nt37801_cmd_fhd_range_min_qp,
			.range_max_qp = nt37801_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37801_cmd_fhd_range_bpg_ofs,
			},
*/
		},
	.data_rate = PLL_CLOCK * 2,
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = PLL_CLOCK + 1,
	},
	.dyn_fps = {
		.vact_timing_fps = 120,
	},
	.real_te_duration = 8333,
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

void get_switch_mode_delay (enum SWITCH_MODE_DELAY **switch_mode_delay, unsigned int mode_num)
{
	unsigned int i = 0;

	for (i = 0;  i < mode_num; i++) {
		memcpy(switch_mode_delay[i], switch_mode_delay_table[i],
			sizeof(enum SWITCH_MODE_DELAY) * mode_num);
	}
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m) == 120) {
		ext_params.skip_vblank = 0;
		ext_params.vblank_off = false;
		ext->params = &ext_params;
	} else if (drm_mode_vrefresh(m) == 90) {
		ext->params = &ext_params_90hz;
	} else if (drm_mode_vrefresh(m) == 60) {
		ext_params_60hz.skip_vblank = 2;
		ext->params = &ext_params_60hz;
	} else if (drm_mode_vrefresh(m) == 30) {
		ext_params.skip_vblank = 4;
		ext_params.vblank_off = false;
		ext->params = &ext_params;
	} else if (drm_mode_vrefresh(m) == 24) {
		ext_params.skip_vblank = 5;
		ext_params.vblank_off = false;
		ext->params = &ext_params;
	} else if (drm_mode_vrefresh(m) == 10) {
		ext_params.skip_vblank = 12;
		ext_params.vblank_off = true;
		ext->params = &ext_params;
	} else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
}

static void mode_switch_to_90(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
}

static void mode_switch_to_60(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x91, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
}

static void mode_switch_to_30(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x91, 0x03, 0x03, 0x00, 0x01, 0x03, 0x03, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
}

static void mode_switch_to_24(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x91, 0x04, 0x04, 0x00, 0x01, 0x04, 0x04, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
}

static void mode_switch_to_10(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x91, 0x0B, 0x0B, 0x00, 0x01, 0x0B, 0x0B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (stage == BEFORE_DSI_POWERDOWN)
		return ret;

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (drm_mode_vrefresh(m) == 120)
		mode_switch_to_120(panel);
	else if (drm_mode_vrefresh(m) == 90)
		mode_switch_to_90(panel);
	else if (drm_mode_vrefresh(m) == 60)
		mode_switch_to_60(panel);
	else if (drm_mode_vrefresh(m) == 30)
		mode_switch_to_30(panel);
	else if (drm_mode_vrefresh(m) == 24)
		mode_switch_to_24(panel);
	else if (drm_mode_vrefresh(m) == 10)
		mode_switch_to_10(panel);
	else
		ret = 1;

	return ret;
}

static int lcm_update_roi(struct drm_panel *panel,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	int ret = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + w - 1;
	unsigned int y1 = y0 + h - 1;
	unsigned char x0_msb = ((x0 >> 8) & 0xFF);
	unsigned char x0_lsb = (x0 & 0xFF);
	unsigned char x1_msb = ((x1 >> 8) & 0xFF);
	unsigned char x1_lsb = (x1 & 0xFF);
	unsigned char y0_msb = ((y0 >> 8) & 0xFF);
	unsigned char y0_lsb = (y0 & 0xFF);
	unsigned char y1_msb = ((y1 >> 8) & 0xFF);
	unsigned char y1_lsb = (y1 & 0xFF);

	//set TE scan line: display total line - slice height + 8 = 2368
	lcm_dcs_write_seq_static(ctx, 0x44, 0x09, 0x40);

	pr_info("%s (x,y,w,h): (%d,%d,%d,%d)\n", __func__, x, y, w, h);
	lcm_dcs_write_seq(ctx, 0x2A, x0_msb, x0_lsb, x1_msb, x1_lsb);
	lcm_dcs_write_seq(ctx, 0x2B, y0_msb, y0_lsb, y1_msb, y1_lsb);

	return ret;
}

static int lcm_update_roi_cmdq(void *dsi, dcs_write_gce cb, void *handle,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	int ret = 0;
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + w - 1;
	unsigned int y1 = y0 + h - 1;
	unsigned char x0_msb = ((x0 >> 8) & 0xFF);
	unsigned char x0_lsb = (x0 & 0xFF);
	unsigned char x1_msb = ((x1 >> 8) & 0xFF);
	unsigned char x1_lsb = (x1 & 0xFF);
	unsigned char y0_msb = ((y0 >> 8) & 0xFF);
	unsigned char y0_lsb = (y0 & 0xFF);
	unsigned char y1_msb = ((y1 >> 8) & 0xFF);
	unsigned char y1_lsb = (y1 & 0xFF);

	//set TE scan line: display total line - slice height + 8 = 2368
	char te_sl[] = { 0x44, 0x09, 0x40};
	char roi_x[] = { 0x2A, x0_msb, x0_lsb, x1_msb, x1_lsb};
	char roi_y[] = { 0x2B, y0_msb, y0_lsb, y1_msb, y1_lsb};

	pr_info("%s (x,y,w,h): (%d,%d,%d,%d)\n", __func__, x, y, w, h);

	if (!cb)
		return -1;

	cb(dsi, handle, te_sl, ARRAY_SIZE(te_sl));
	cb(dsi, handle, roi_x, ARRAY_SIZE(roi_x));
	cb(dsi, handle, roi_y, ARRAY_SIZE(roi_y));

	return ret;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.set_bl_elvss_cmdq = lcm_set_bl_elvss_cmdq,
	/* Not real backlight cmd in AOD, just for QC purpose */
	.set_aod_light_mode = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.lcm_update_roi = lcm_update_roi,
	.lcm_update_roi_cmdq = lcm_update_roi_cmdq,
	.get_switch_mode_delay = get_switch_mode_delay,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode1;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
	struct drm_display_mode *mode4;
	struct drm_display_mode *mode5;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode1 = drm_mode_duplicate(connector->dev, &mode_90);
	if (!mode1)
		return -ENOMEM;

	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);

	mode2 = drm_mode_duplicate(connector->dev, &mode_60);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_60.hdisplay, mode_60.vdisplay,
			drm_mode_vrefresh(&mode_60));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &mode_30);
	if (!mode3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_30.hdisplay, mode_30.vdisplay,
			drm_mode_vrefresh(&mode_30));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	mode4 = drm_mode_duplicate(connector->dev, &mode_24);
	if (!mode4) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_24.hdisplay, mode_24.vdisplay,
			drm_mode_vrefresh(&mode_24));
		return -ENOMEM;
	}

	drm_mode_set_name(mode4);
	mode4->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode4);

	mode5 = drm_mode_duplicate(connector->dev, &mode_10);
	if (!mode5) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_10.hdisplay, mode_10.vdisplay,
			drm_mode_vrefresh(&mode_10));
		return -ENOMEM;
	}

	drm_mode_set_name(mode5);
	mode5->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode5);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
	} else {
		devm_gpiod_put(dev, ctx->bias_pos);
	}

	ctx->prepared = true;
	ctx->enabled = true;
	atomic_set(&current_backlight, 2047);

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx != NULL) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif

}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "anapass,ana38407-wqxga-plus-dphy-cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-ana38407-wqxga-plus-dphy-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Neil Yu <neil.yu@mediatek.com>");
MODULE_DESCRIPTION("ana38407 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");

