// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_util.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_mode.h>
#include <drm/drm_connector.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "../mediatek/mediatek_v2/mtk_disp_recovery.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

//#define ENABLE_MTK_LCD_DTS_CHECK

#define REGFLAG_DELAY 0
#define ENABLE 1
#define DISABLE 0

#define S6E8FC5_CMD_120HZ 1

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *dvdd_en_gpio;
	struct gpio_desc *lcd_3p0_en_gpio;

	bool prepared;
	bool enabled;

	int error;
};

struct LCM_setting_table {
	unsigned char count;
	unsigned char para_list[129];
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
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static struct regulator *disp_bias_vcamio;

static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_vcamio = regulator_get(NULL, "vcamio");
	if (IS_ERR(disp_bias_vcamio)) { /* handle return value */
		ret = PTR_ERR(disp_bias_vcamio);
		pr_info("get vcamio fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;

	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_vcamio, 1800000, 1800000);
	if (ret < 0)
		pr_info("set voltage disp_bias_vcamio fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_vcamio);
	if (ret < 0)
		pr_info("enable regulator disp_bias_vcamio fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_vcamio);
	if (ret < 0)
		pr_info("disable regulator disp_bias_vcamio fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static struct LCM_setting_table init_setting_cmd[] = {
	{3, {0x9F, 0xA5, 0xA5}},

	/* Sleep Out */
	{1, {0x11} },
	{REGFLAG_DELAY, {110}}, /* Delay 110ms */

	/* 4 Operating Setting */
	/* 4.1 Common Setting */
	/* 4.1.1 TE(Vsync) ON/OFF */
	{3, {0xF0, 0x5A, 0x5A}},
	{2, {0x35, 0x00}}, /* TE ON */
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.2 PAGE ADDRESS SET */
	{5, {0x2A, 0x00, 0x00, 0x04, 0x37}},
	{5, {0x2B, 0x00, 0x00, 0x09, 0x23}},

	/* 4.1.3 FFC SET */
	{3, {0xF0, 0x5A, 0x5A}},
	{3, {0xFC, 0x5A, 0x5A}},
	//TBD
	{2, {0xF7, 0x0F}},
	{3, {0xF0, 0xA5, 0xA5}},
	{3, {0xFC, 0xA5, 0xA5}},

	/* 4.1.4 ERR_FG Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	//TBD
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.5 PCD Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	//TBD
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.6 ACL Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	//TBD
	{2, {0xF7, 0x0F}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.7 DSC Setting */
//	{2, {0x07, 0x01}},
	{2, {0x9D, 0x01}},
	{89, {0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x24,
				0x04, 0x38, 0x00, 0x1E, 0x02, 0x1C, 0x02, 0x1C,
				0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x02, 0xE3,
				0x00, 0x07, 0x00, 0x0C, 0x03, 0x50, 0x03, 0x64,
				0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
				0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
				0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
				0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
				0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
				0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
				0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4}},
	/* 4.1.8 ASWIRE Pulse Off Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x0A, 0xB5}},
	{2, {0xB5, 0x00}},
	{2, {0xF7, 0x0F}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.2 Brightness Setting */
	/* 4.2.1 Max & Dimming */
	{3, {0xF0, 0x5A, 0x5A}},
#ifdef S6E8FC5_CMD_120HZ
	{3, {0x60, 0x00, 0x00}},  //0x00 : 120Hz, 0x08 : 60Hz
#else
	{3, {0x60, 0x08, 0x00}},  //0x00 : 120Hz, 0x08 : 60Hz
#endif
	{4, {0xB0, 0x00, 0x0F, 0x66}},
	{2, {0x66, 0x10}},
	{2, {0x53, 0x20}},
	{3, {0x51, 0x03, 0xFF}}, //500nit
	{2, {0xF7, 0x0F}},
	{3, {0xF0, 0xA5, 0xA5}},


	/* 4.2.2 HBM & Interpolation Dimming 8 00~420nit) */
	{3, {0xF0, 0x5A, 0x5A}},


#ifdef S6E8FC5_CMD_120HZ
	{3, {0x60, 0x00, 0x00}}, //0x00 : 120Hz, 0x08 : 60Hz
#else
	{3, {0x60, 0x08, 0x00}}, //0x00 : 120Hz, 0x08 : 60Hz
#endif
	{4, {0xB0, 0x00, 0x0F, 0x66}},
	{2, {0x66, 0x10}},
	{2, {0x53, 0xE0}},
	{3, {0x51, 0x03, 0xFF}},
	{2, {0xF7, 0x0F}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.2.3 ACL ON/OFF */
	{3, {0xF0, 0x5A, 0x5A}},
	{2, {0x55, 0x00}},
	{3, {0xF0, 0xA5, 0xA5}},

	{REGFLAG_DELAY, {70}}, /* Delay 70ms */
	{1, {0x29}}, /* Display On */
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	int i;
	int size;

	for (i = 0; i < count; i++) {
		size = table[i].count;

		switch (size) {
		case REGFLAG_DELAY:
			mdelay(table[i].para_list[0]);
			break;
		default:
			lcm_dcs_write(ctx, table[i].para_list, table[i].count);
			break;
		}
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s\n", __func__);
	push_table(ctx, init_setting_cmd,
			sizeof(init_setting_cmd) / sizeof(struct LCM_setting_table));
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->enabled = false;
	pr_info("[%s][s6e8fc5] finished\n", __func__);

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	mdelay(25);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(130);

	ctx->error = 0;
	ctx->prepared = false;

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd-3p0-en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->lcd_3p0_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	//VDDI: VCAMIO OFF
	lcm_panel_bias_disable();
	pr_info("[%s][s6e8fc5] finished\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);

	if (ctx->prepared)
		return 0;

	//VDDI: VCAMIO ON
	lcm_panel_bias_enable();

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd-3p0-en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->lcd_3p0_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return 0;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(1);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

	pr_info("[%s][s6e8fc5] finished\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->enabled = true;

	pr_info("[%s][s6e8fc5] finished\n", __func__);

	return 0;
}

#define HFP (250)//(70)
#define HSA (70)//(20)
#define HBP (110)
#define VFP (12)
#define VSA (2)
#define VBP (16)
#define VAC (2340)
#define HAC (1080)

static struct drm_display_mode default_mode = {
//	.clock = 355999,// Pixel Clock = vtotal(2464) * htotal * vrefresh(90) / 1000;
	.clock = 214722,// Pixel Clock = vtotal(2464) * htotal * vrefresh(90) / 1000;
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
//	.vrefresh = 120,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
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
	if (ret < 0)
		pr_info("%s error\n", __func__);

	pr_info("[%s]ATA read data %x %x %x\n", __func__, data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;
	pr_info("[%s][s6e8fc5] ATA expect read data is %x %x %x\n",
		__func__, id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	unsigned int mapping_level;
	char enable_cmd1[] = {0xf0, 0x5a, 0x5a};
//	char enable_cmd2[] = {0xfc, 0x5a, 0x5a};
	char freq_setting[] = {0x60, 0x00, 0x00}; /* 0x00:120Hz, 0x08:60Hz */
	char global_para[] = {0xb0, 0x00, 0x0f, 0x66};
	char sync_control[] = {0x66, 0x10}; /* 0x10:Normal transition, 0x30:Smooth transition */
	char WRCTRLD[] = {0x53, 0x20}; /* 0x20:Normal transition, 0x28:Smooth transition */
	char bl_tb0[] = {0x51, 0x03, 0xff};
	char ltps_cmd[] = {0xf7, 0x0f};
	char disable_cmd1[] = {0xf0, 0xa5, 0xa5};

	if(level > 255 )
		level = 255;

	mapping_level = level * 1023 / 255;
	bl_tb0[1] = ((mapping_level >> 8) & 0xf);
	bl_tb0[2] = (mapping_level & 0xff);

	pr_info("[%s][s6e8fc5] bl_tb0[0] = 0x%x, bl_tb0[1] = 0x%x, level = %d(0x%x)\n",
		__func__, bl_tb0[1], bl_tb0[2], level, mapping_level);

	if (!cb)
		return -1;

	cb(dsi, handle, enable_cmd1, ARRAY_SIZE(enable_cmd1));
	cb(dsi, handle, freq_setting, ARRAY_SIZE(freq_setting));
	cb(dsi, handle, global_para, ARRAY_SIZE(global_para));
	cb(dsi, handle, sync_control, ARRAY_SIZE(sync_control));
	cb(dsi, handle, WRCTRLD, ARRAY_SIZE(WRCTRLD));
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	cb(dsi, handle, ltps_cmd, ARRAY_SIZE(ltps_cmd));
	cb(dsi, handle, disable_cmd1, ARRAY_SIZE(disable_cmd1));

	return 0;
}

static struct mtk_panel_params ext_params = {
//	.pll_clk = 210,
	//.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},

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
		.pic_height = 2340,
		.pic_width = 1080,
		.slice_height = 30,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 739,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 848,
		.slice_bpg_offset = 868,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.data_rate = 806,//795,//898,
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
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

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		pr_info("failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 70;				// Physical width
	connector->display_info.height_mm = 140;				// Physical height

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
	struct lcm *ctx;
	int ret;

#ifdef ENABLE_MTK_LCD_DTS_CHECK
	struct device_node *lcd_comp;

	lcd_comp = of_find_compatible_node(NULL, NULL,
	"s6e8fc5,fhdp,cmd");
	if (!lcd_comp) {
		pr_info("[%s][s6e8fc5] panel compatible doesn't match\n", __func__);
		return -1;
	}
#endif

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_LPM |
					MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd_3p0_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return PTR_ERR(ctx->lcd_3p0_en_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("[%s][s6e8fc5] probe success\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "s6e8fc5,fhdp,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-s6e8fc5-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Leo Yun <leo.yun@mediatek.com>");
MODULE_DESCRIPTION("s6e8fc5 fhdp LCD Panel Driver");
MODULE_LICENSE("GPL v2");
