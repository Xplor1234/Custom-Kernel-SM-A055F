// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

static char bl_tb0[] = { 0x51, 0xff };

/*LCM_DEGREE default value*/
#define PROBE_FROM_DTS 0

//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable
#define BYPASSI2C

#define FHD_FRAME_WIDTH    (1080)
#define FHD_HFP            (72)
#define FHD_HSA            (16)
#define FHD_HBP            (80)
#define FHD_HTOTAL         (FHD_FRAME_WIDTH + FHD_HFP + FHD_HSA + FHD_HBP)
#define FHD_FRAME_HEIGHT   (2400)
#define FHD_VFP_120        (100)
#define FHD_VFP_90         (927)
#define FHD_VFP_60         (2620)
#define FHD_VSA            (10)
#define FHD_VBP            (10)
#define FHD_VTOTAL_120     (FHD_FRAME_HEIGHT + FHD_VFP_120 + FHD_VSA + FHD_VBP)
#define FHD_VTOTAL_90      (FHD_FRAME_HEIGHT + FHD_VFP_90 + FHD_VSA + FHD_VBP)
#define FHD_VTOTAL_60      (FHD_FRAME_HEIGHT + FHD_VFP_60 + FHD_VSA + FHD_VBP)
#define FHD_FRAME_TOTAL_120 (FHD_VTOTAL_120 * FHD_HTOTAL)
#define FHD_FRAME_TOTAL_90  (FHD_VTOTAL_90 * FHD_HTOTAL)
#define FHD_FRAME_TOTAL_60  (FHD_VTOTAL_60 * FHD_HTOTAL)
#define FHD_VREFRESH_120   (120)
#define FHD_VREFRESH_90    (90)
#define FHD_VREFRESH_60    (60)
#define FHD_CLK_120_X10    ((FHD_FRAME_TOTAL_120 * FHD_VREFRESH_120) / 100)
#define FHD_CLK_90_X10     ((FHD_FRAME_TOTAL_90 * FHD_VREFRESH_90) / 100)
#define FHD_CLK_60_X10     ((FHD_FRAME_TOTAL_60 * FHD_VREFRESH_60) / 100)
#define FHD_CLK_120		   (((FHD_CLK_120_X10 % 10) != 0) ?             \
			(FHD_CLK_120_X10 / 10 + 1) : (FHD_CLK_120_X10 / 10))
#define FHD_CLK_90		   (((FHD_CLK_90_X10 % 10) != 0) ?             \
				(FHD_CLK_90_X10 / 10 + 1) : (FHD_CLK_90_X10 / 10))
#define FHD_CLK_60		   (((FHD_CLK_60_X10 % 10) != 0) ?             \
				(FHD_CLK_60_X10 / 10 + 1) : (FHD_CLK_60_X10 / 10))

#define HD_FRAME_WIDTH    (720)
#define HD_HFP            (76)
#define HD_HSA            (12)
#define HD_HBP            (80)
#define HD_HTOTAL         (HD_FRAME_WIDTH + HD_HFP + HD_HSA + HD_HBP)
#define HD_FRAME_HEIGHT   (1600)
#define HD_VFP_120        (100)
#define HD_VFP_90         (927)
#define HD_VFP_60         (2620)
#define HD_VSA            (10)
#define HD_VBP            (10)
#define HD_VTOTAL_120     (HD_FRAME_HEIGHT + HD_VFP_120 + HD_VSA + HD_VBP)
#define HD_VTOTAL_90      (HD_FRAME_HEIGHT + HD_VFP_90 + HD_VSA + HD_VBP)
#define HD_VTOTAL_60      (HD_FRAME_HEIGHT + HD_VFP_60 + HD_VSA + HD_VBP)
#define HD_FRAME_TOTAL_120 (HD_VTOTAL_120 * HD_HTOTAL)
#define HD_FRAME_TOTAL_90  (HD_VTOTAL_90 * HD_HTOTAL)
#define HD_FRAME_TOTAL_60  (HD_VTOTAL_60 * HD_HTOTAL)
#define HD_VREFRESH_120   (120)
#define HD_VREFRESH_90    (90)
#define HD_VREFRESH_60    (60)
#define HD_CLK_120_X10    ((HD_FRAME_TOTAL_120 * HD_VREFRESH_120) / 100)
#define HD_CLK_90_X10     ((HD_FRAME_TOTAL_90 * HD_VREFRESH_90) / 100)
#define HD_CLK_60_X10     ((HD_FRAME_TOTAL_60 * HD_VREFRESH_60) / 100)
#define HD_CLK_120		   (((HD_CLK_120_X10 % 10) != 0) ?             \
			(HD_CLK_120_X10 / 10 + 1) : (HD_CLK_120_X10 / 10))
#define HD_CLK_90		   (((HD_CLK_90_X10 % 10) != 0) ?             \
				(HD_CLK_90_X10 / 10 + 1) : (HD_CLK_90_X10 / 10))
#define HD_CLK_60		   (((HD_CLK_60_X10 % 10) != 0) ?             \
				(HD_CLK_60_X10 / 10 + 1) : (HD_CLK_60_X10 / 10))

static enum RES_SWITCH_TYPE res_switch_type = RES_SWITCH_NO_USE;
static int current_fps = 120;

#ifndef BYPASSI2C
/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;


/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
		.compatible = "mediatek,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	pr_debug("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/***********************************/
#endif

static int fake_mode;

struct jdi {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	unsigned int gate_ic;
	unsigned int lcm_degree;

	int error;
};

#define jdi_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define jdi_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct jdi *panel_to_jdi(struct drm_panel *panel)
{
	return container_of(panel, struct jdi, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int jdi_dcs_read(struct jdi *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void jdi_panel_get_data(struct jdi *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = jdi_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void jdi_dcs_write(struct jdi *ctx, const void *data, size_t len)
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

static void jdi_panel_init(struct jdi *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X10);
	//REGR 0XFE,0X10
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB0, 0X00);
	//DSC ON && set PPS
	jdi_dcs_write_seq_static(ctx, 0XC0, 0X03);//JDI VESA
	jdi_dcs_write_seq_static(ctx, 0XC1, 0X89, 0X28, 0X00, 0X08, 0X00, 0XAA,
				0X02, 0X0E, 0X00, 0X2B, 0X00, 0X07, 0X0D, 0XB7, 0X0C, 0XB7);
	jdi_dcs_write_seq_static(ctx, 0XC2, 0X1B, 0XA0);
	jdi_dcs_write_seq_static(ctx, 0XE9, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XFF, 0X20);
	//REGR 0XFE,0X20
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X66);
	jdi_dcs_write_seq_static(ctx, 0X06, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X07, 0X38);
	jdi_dcs_write_seq_static(ctx, 0X18, 0X66);
	jdi_dcs_write_seq_static(ctx, 0X1B, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X2F, 0X83);
	jdi_dcs_write_seq_static(ctx, 0X69, 0X91);
	jdi_dcs_write_seq_static(ctx, 0X95, 0XD1);
	jdi_dcs_write_seq_static(ctx, 0X96, 0XD1);
	jdi_dcs_write_seq_static(ctx, 0XF2, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF3, 0X64);
	jdi_dcs_write_seq_static(ctx, 0XF4, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF5, 0X64);
	jdi_dcs_write_seq_static(ctx, 0XF6, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF7, 0X64);
	jdi_dcs_write_seq_static(ctx, 0XF8, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF9, 0X64);

	jdi_dcs_write_seq_static(ctx, 0x89, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8A, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8D, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8E, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8F, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x91, 0x15);//VCOM

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X23);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X00, 0X80);
	jdi_dcs_write_seq_static(ctx, 0x04, 0x05);
	jdi_dcs_write_seq_static(ctx, 0x05, 0x2d);
	jdi_dcs_write_seq_static(ctx, 0x06, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x07, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x08, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x09, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x11, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x12, 0x95);
	jdi_dcs_write_seq_static(ctx, 0x15, 0x68);
	jdi_dcs_write_seq_static(ctx, 0x16, 0x0B);
	jdi_dcs_write_seq_static(ctx, 0xA0, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x30, 0xFF);
	jdi_dcs_write_seq_static(ctx, 0x31, 0xF0);
	jdi_dcs_write_seq_static(ctx, 0x32, 0xEB);
	jdi_dcs_write_seq_static(ctx, 0x33, 0xE5);
	jdi_dcs_write_seq_static(ctx, 0x34, 0xDD);
	jdi_dcs_write_seq_static(ctx, 0x35, 0xDA);
	jdi_dcs_write_seq_static(ctx, 0x36, 0xD5);
	jdi_dcs_write_seq_static(ctx, 0x37, 0xD0);
	jdi_dcs_write_seq_static(ctx, 0x38, 0xCE);
	jdi_dcs_write_seq_static(ctx, 0x39, 0xCD);
	jdi_dcs_write_seq_static(ctx, 0x3A, 0xCD);
	jdi_dcs_write_seq_static(ctx, 0x3B, 0xCD);
	jdi_dcs_write_seq_static(ctx, 0x3D, 0xCB);
	jdi_dcs_write_seq_static(ctx, 0x3F, 0xCB);
	jdi_dcs_write_seq_static(ctx, 0x40, 0xC6);
	jdi_dcs_write_seq_static(ctx, 0x41, 0xBF);
	jdi_dcs_write_seq_static(ctx, 0x45, 0xFF);
	jdi_dcs_write_seq_static(ctx, 0x46, 0xF0);
	jdi_dcs_write_seq_static(ctx, 0x47, 0xE8);
	jdi_dcs_write_seq_static(ctx, 0x48, 0xCE);
	jdi_dcs_write_seq_static(ctx, 0x49, 0xBC);
	jdi_dcs_write_seq_static(ctx, 0x4A, 0xB8);
	jdi_dcs_write_seq_static(ctx, 0x4B, 0xB5);
	jdi_dcs_write_seq_static(ctx, 0x4C, 0xB0);
	jdi_dcs_write_seq_static(ctx, 0x4D, 0xA8);
	jdi_dcs_write_seq_static(ctx, 0x4E, 0xA0);
	jdi_dcs_write_seq_static(ctx, 0x4F, 0x9B);
	jdi_dcs_write_seq_static(ctx, 0x50, 0x98);
	jdi_dcs_write_seq_static(ctx, 0x51, 0x98);
	jdi_dcs_write_seq_static(ctx, 0x52, 0x88);
	jdi_dcs_write_seq_static(ctx, 0x53, 0x80);
	jdi_dcs_write_seq_static(ctx, 0x54, 0x7F);
	jdi_dcs_write_seq_static(ctx, 0x58, 0xFF);
	jdi_dcs_write_seq_static(ctx, 0x59, 0xF6);
	jdi_dcs_write_seq_static(ctx, 0x5A, 0xED);
	jdi_dcs_write_seq_static(ctx, 0x5B, 0xE6);
	jdi_dcs_write_seq_static(ctx, 0x5C, 0xDF);
	jdi_dcs_write_seq_static(ctx, 0x5D, 0xD8);
	jdi_dcs_write_seq_static(ctx, 0x5E, 0xD3);
	jdi_dcs_write_seq_static(ctx, 0x5F, 0xCE);
	jdi_dcs_write_seq_static(ctx, 0x60, 0xC9);
	jdi_dcs_write_seq_static(ctx, 0x61, 0xC4);
	jdi_dcs_write_seq_static(ctx, 0x62, 0xC1);
	jdi_dcs_write_seq_static(ctx, 0x63, 0xBE);
	jdi_dcs_write_seq_static(ctx, 0x64, 0xBB);
	jdi_dcs_write_seq_static(ctx, 0x65, 0xB8);
	jdi_dcs_write_seq_static(ctx, 0x66, 0xB6);
	jdi_dcs_write_seq_static(ctx, 0x67, 0xB5);
	jdi_dcs_write_seq_static(ctx, 0XFF, 0X24);
	//REGR 0XFE,0X24
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X0F);
	jdi_dcs_write_seq_static(ctx, 0X03, 0X0C);
	jdi_dcs_write_seq_static(ctx, 0X05, 0X1D);
	jdi_dcs_write_seq_static(ctx, 0X08, 0X2F);
	jdi_dcs_write_seq_static(ctx, 0X09, 0X2E);
	jdi_dcs_write_seq_static(ctx, 0X0A, 0X2D);
	jdi_dcs_write_seq_static(ctx, 0X0B, 0X2C);
	jdi_dcs_write_seq_static(ctx, 0X11, 0X17);
	jdi_dcs_write_seq_static(ctx, 0X12, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X13, 0X15);
	jdi_dcs_write_seq_static(ctx, 0X15, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X16, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X17, 0X18);
	jdi_dcs_write_seq_static(ctx, 0X1B, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X1D, 0X1D);
	jdi_dcs_write_seq_static(ctx, 0X20, 0X2F);
	jdi_dcs_write_seq_static(ctx, 0X21, 0X2E);
	jdi_dcs_write_seq_static(ctx, 0X22, 0X2D);
	jdi_dcs_write_seq_static(ctx, 0X23, 0X2C);
	jdi_dcs_write_seq_static(ctx, 0X29, 0X17);
	jdi_dcs_write_seq_static(ctx, 0X2A, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X2B, 0X15);
	jdi_dcs_write_seq_static(ctx, 0X2F, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X30, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X31, 0X18);
	jdi_dcs_write_seq_static(ctx, 0X32, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X34, 0X10);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X36, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X37, 0X20);
	jdi_dcs_write_seq_static(ctx, 0X4D, 0X19);
	jdi_dcs_write_seq_static(ctx, 0X4E, 0X45);
	jdi_dcs_write_seq_static(ctx, 0X4F, 0X45);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X45);
	jdi_dcs_write_seq_static(ctx, 0X71, 0X30);
	jdi_dcs_write_seq_static(ctx, 0X79, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X7A, 0X82);
	jdi_dcs_write_seq_static(ctx, 0X7B, 0X94);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X80, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X81, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X82, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X31);
	jdi_dcs_write_seq_static(ctx, 0X85, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X86, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X87, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X90, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X92, 0X31);
	jdi_dcs_write_seq_static(ctx, 0X93, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X94, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X95, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0XF4);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XA0, 0X14);
	jdi_dcs_write_seq_static(ctx, 0XA2, 0X14);
	jdi_dcs_write_seq_static(ctx, 0XA3, 0X02);
	jdi_dcs_write_seq_static(ctx, 0XA4, 0X04);
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X04);
	jdi_dcs_write_seq_static(ctx, 0XC6, 0XC0);
	jdi_dcs_write_seq_static(ctx, 0XC9, 0X00);
	jdi_dcs_write_seq_static(ctx, 0XD9, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XE9, 0X02);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X25);
	//REGR 0XFE,0X25
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X18, 0X22);
	jdi_dcs_write_seq_static(ctx, 0X19, 0XE4);
	jdi_dcs_write_seq_static(ctx, 0X21, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X66, 0XD8);
	jdi_dcs_write_seq_static(ctx, 0X68, 0X50);
	jdi_dcs_write_seq_static(ctx, 0X69, 0X10);
	jdi_dcs_write_seq_static(ctx, 0X6B, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X6D, 0X0D);
	jdi_dcs_write_seq_static(ctx, 0X6E, 0X48);
	jdi_dcs_write_seq_static(ctx, 0X72, 0X41);
	jdi_dcs_write_seq_static(ctx, 0X73, 0X4A);
	jdi_dcs_write_seq_static(ctx, 0X74, 0XD0);
	jdi_dcs_write_seq_static(ctx, 0X77, 0X62);
	jdi_dcs_write_seq_static(ctx, 0X79, 0X7F);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X1D);
	jdi_dcs_write_seq_static(ctx, 0X7F, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X80, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X0D);
	jdi_dcs_write_seq_static(ctx, 0XCF, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XD6, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XD7, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XEF, 0X20);
	jdi_dcs_write_seq_static(ctx, 0XF0, 0X84);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X26);
	//REGR 0XFE,0X26
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X15, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X81, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X83, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X85, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X86, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X87, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X88, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X8A, 0X1A);
	jdi_dcs_write_seq_static(ctx, 0X8B, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X8C, 0X24);
	jdi_dcs_write_seq_static(ctx, 0X8E, 0X42);
	jdi_dcs_write_seq_static(ctx, 0X8F, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X90, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X91, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X9A, 0X81);
	jdi_dcs_write_seq_static(ctx, 0X9B, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X9E, 0X00);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X27);
	//REGR 0XFE,0X27
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X60);
	jdi_dcs_write_seq_static(ctx, 0X20, 0X81);
	jdi_dcs_write_seq_static(ctx, 0X21, 0XE7);
	jdi_dcs_write_seq_static(ctx, 0X25, 0X82);
	jdi_dcs_write_seq_static(ctx, 0X26, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X6E, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X6F, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X70, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X71, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X72, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X75, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X76, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X77, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X09);
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X5F);
	jdi_dcs_write_seq_static(ctx, 0X80, 0X23);
	jdi_dcs_write_seq_static(ctx, 0X82, 0X09);
	jdi_dcs_write_seq_static(ctx, 0X83, 0X5F);
	jdi_dcs_write_seq_static(ctx, 0X88, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X89, 0X10);
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X10);
	jdi_dcs_write_seq_static(ctx, 0XA6, 0X23);
	jdi_dcs_write_seq_static(ctx, 0XA7, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB6, 0X40);
	jdi_dcs_write_seq_static(ctx, 0XE3, 0X02);
	jdi_dcs_write_seq_static(ctx, 0XE4, 0XDA);
	jdi_dcs_write_seq_static(ctx, 0XE5, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XE6, 0X6D);
	jdi_dcs_write_seq_static(ctx, 0XE9, 0X03);
	jdi_dcs_write_seq_static(ctx, 0XEA, 0X2F);
	jdi_dcs_write_seq_static(ctx, 0XEB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XEC, 0X98);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X2A);
	//REGR 0XFE,0X2A
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X00, 0X91);
	jdi_dcs_write_seq_static(ctx, 0X03, 0X20);
	jdi_dcs_write_seq_static(ctx, 0X07, 0X52);
	jdi_dcs_write_seq_static(ctx, 0X0A, 0X70);
	jdi_dcs_write_seq_static(ctx, 0X0D, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X0E, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X11, 0XF0);
	jdi_dcs_write_seq_static(ctx, 0X15, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X16, 0XB6);
	jdi_dcs_write_seq_static(ctx, 0X19, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X1A, 0X8A);
	jdi_dcs_write_seq_static(ctx, 0X1B, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X1D, 0X36);
	jdi_dcs_write_seq_static(ctx, 0X1E, 0X4F);
	jdi_dcs_write_seq_static(ctx, 0X1F, 0X4F);
	jdi_dcs_write_seq_static(ctx, 0X20, 0X4F);
	jdi_dcs_write_seq_static(ctx, 0X28, 0XEC);
	jdi_dcs_write_seq_static(ctx, 0X29, 0X0C);
	jdi_dcs_write_seq_static(ctx, 0X2A, 0X05);
	jdi_dcs_write_seq_static(ctx, 0X2D, 0X06);
	jdi_dcs_write_seq_static(ctx, 0X2F, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X30, 0X4A);
	jdi_dcs_write_seq_static(ctx, 0X33, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X34, 0XEE);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X30);
	jdi_dcs_write_seq_static(ctx, 0X36, 0X06);
	jdi_dcs_write_seq_static(ctx, 0X37, 0XE9);
	jdi_dcs_write_seq_static(ctx, 0X38, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X39, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X3A, 0X4A);
	jdi_dcs_write_seq_static(ctx, 0X46, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X47, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X4A, 0XF0);
	jdi_dcs_write_seq_static(ctx, 0X4E, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X4F, 0X9B);
	jdi_dcs_write_seq_static(ctx, 0X52, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X6F);
	jdi_dcs_write_seq_static(ctx, 0X54, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X56, 0X36);
	jdi_dcs_write_seq_static(ctx, 0X57, 0X7E);
	jdi_dcs_write_seq_static(ctx, 0X58, 0X7E);
	jdi_dcs_write_seq_static(ctx, 0X59, 0X7E);
	jdi_dcs_write_seq_static(ctx, 0X60, 0X80);
	jdi_dcs_write_seq_static(ctx, 0X61, 0XC7);
	jdi_dcs_write_seq_static(ctx, 0X62, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X63, 0XF3);
	jdi_dcs_write_seq_static(ctx, 0X64, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X65, 0X05);
	jdi_dcs_write_seq_static(ctx, 0X66, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X67, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X68, 0X8A);
	jdi_dcs_write_seq_static(ctx, 0X6A, 0X0F);
	jdi_dcs_write_seq_static(ctx, 0X6B, 0XC9);
	jdi_dcs_write_seq_static(ctx, 0X6C, 0X20);
	jdi_dcs_write_seq_static(ctx, 0X6D, 0XE3);
	jdi_dcs_write_seq_static(ctx, 0X6E, 0XC6);
	jdi_dcs_write_seq_static(ctx, 0X6F, 0X22);
	jdi_dcs_write_seq_static(ctx, 0X70, 0XE1);
	jdi_dcs_write_seq_static(ctx, 0X71, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X7A, 0X07);
	jdi_dcs_write_seq_static(ctx, 0X7B, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X7F, 0X2C);
	jdi_dcs_write_seq_static(ctx, 0X83, 0X0F);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X12);
	jdi_dcs_write_seq_static(ctx, 0X87, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X88, 0XE6);
	jdi_dcs_write_seq_static(ctx, 0X89, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X8B, 0X36);
	jdi_dcs_write_seq_static(ctx, 0X8C, 0X3A);
	jdi_dcs_write_seq_static(ctx, 0X8D, 0X3C);
	jdi_dcs_write_seq_static(ctx, 0X8E, 0X3A);
	jdi_dcs_write_seq_static(ctx, 0X95, 0X80);
	jdi_dcs_write_seq_static(ctx, 0X96, 0XFD);
	jdi_dcs_write_seq_static(ctx, 0X97, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X98, 0X32);
	jdi_dcs_write_seq_static(ctx, 0X99, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X9A, 0X08);
	jdi_dcs_write_seq_static(ctx, 0X9B, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X4C);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0XB1);
	jdi_dcs_write_seq_static(ctx, 0X9F, 0X75);
	jdi_dcs_write_seq_static(ctx, 0XA0, 0XFF);
	jdi_dcs_write_seq_static(ctx, 0XA2, 0X42);
	jdi_dcs_write_seq_static(ctx, 0XA3, 0X6F);
	jdi_dcs_write_seq_static(ctx, 0XA4, 0XF9);
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X47);
	jdi_dcs_write_seq_static(ctx, 0XA6, 0X6A);
	jdi_dcs_write_seq_static(ctx, 0XA7, 0X4C);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X2C);
	//REGR 0XFE,0X2C
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X00, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X02, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X03, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X04, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X05, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X0D, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X0E, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X16, 0X1B);
	jdi_dcs_write_seq_static(ctx, 0X17, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X18, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X19, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X2A, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X4D, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X4E, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X4F, 0X2E);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X54, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X55, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X56, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X58, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X59, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X61, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X62, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X6A, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X6B, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X6C, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X6D, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X0F);
	jdi_dcs_write_seq_static(ctx, 0X9E, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X9F, 0X00);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X20);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB0, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x49, 0x00, 0x6B, 0x00,
		0x85, 0x00, 0x9C, 0x00, 0xB1, 0x00, 0xC4);
	jdi_dcs_write_seq_static(ctx, 0XB1, 0x00, 0xD1, 0x01, 0x07, 0x01, 0x30, 0x01, 0x6E, 0x01,
		0x9E, 0x01, 0xE5, 0x02, 0x1E, 0x02, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0XB2, 0x02, 0x56, 0x02, 0x96, 0x02, 0xBF, 0x02, 0xF4, 0x03,
		0x16, 0x03, 0x41, 0x03, 0x51, 0x03, 0x5F);
	jdi_dcs_write_seq_static(ctx, 0XB3, 0x03, 0x6E, 0x03, 0x82, 0x03, 0x98, 0x03, 0xAC, 0x03,
		0xCC, 0x03, 0xD8, 0x00, 0x00);
	jdi_dcs_write_seq_static(ctx, 0XB4, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x49, 0x00, 0x69, 0x00,
		0x84, 0x00, 0x9B, 0x00, 0xAF, 0x00, 0xC1);
	jdi_dcs_write_seq_static(ctx, 0XB5, 0x00, 0xD2, 0x01, 0x07, 0x01, 0x30, 0x01, 0x6E, 0x01,
		0x9D, 0x01, 0xE5, 0x02, 0x1F, 0x02, 0x20);
	jdi_dcs_write_seq_static(ctx, 0XB6, 0x02, 0x57, 0x02, 0x96, 0x02, 0xBF, 0x02, 0xF3, 0x03,
		0x16, 0x03, 0x3F, 0x03, 0x4F, 0x03, 0x5D);
	jdi_dcs_write_seq_static(ctx, 0XB7, 0x03, 0x6D, 0x03, 0x81, 0x03, 0x98, 0x03, 0xAC, 0x03,
		0xCC, 0x03, 0xD8, 0x00, 0x00);
	jdi_dcs_write_seq_static(ctx, 0XB8, 0x00, 0x00, 0x00, 0x20, 0x00, 0x48, 0x00, 0x6A, 0x00,
		0x86, 0x00, 0x9F, 0x00, 0xB5, 0x00, 0xC6);
	jdi_dcs_write_seq_static(ctx, 0XB9, 0x00, 0xD8, 0x01, 0x0D, 0x01, 0x36, 0x01, 0x73, 0x01,
		0xA1, 0x01, 0xE8, 0x02, 0x21, 0x02, 0x22);
	jdi_dcs_write_seq_static(ctx, 0XBA, 0x02, 0x58, 0x02, 0x98, 0x02, 0xC1, 0x02, 0xF7, 0x03,
		0x1B, 0x03, 0x41, 0x03, 0x54, 0x03, 0x66);
	jdi_dcs_write_seq_static(ctx, 0XBB, 0x03, 0x6E, 0x03, 0x82, 0x03, 0x98, 0x03, 0xAC, 0x03,
		0xD0, 0x03, 0xD8, 0x00, 0x00);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X21);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB0, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x49, 0x00, 0x6B, 0x00,
		0x85, 0x00, 0x9C, 0x00, 0xB1, 0x00, 0xC4);
	jdi_dcs_write_seq_static(ctx, 0XB1, 0x00, 0xD1, 0x01, 0x07, 0x01, 0x30, 0x01, 0x6E, 0x01,
		0x9E, 0x01, 0xE5, 0x02, 0x1E, 0x02, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0XB2, 0x02, 0x56, 0x02, 0x96, 0x02, 0xBF, 0x02, 0xF4, 0x03,
		0x16, 0x03, 0x41, 0x03, 0x51, 0x03, 0x5F);
	jdi_dcs_write_seq_static(ctx, 0XB3, 0x03, 0x6E, 0x03, 0x82, 0x03, 0x98, 0x03, 0xAC, 0x03,
		0xCC, 0x03, 0xD8, 0x00, 0x00);
	jdi_dcs_write_seq_static(ctx, 0XB4, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x49, 0x00, 0x69, 0x00,
		0x84, 0x00, 0x9B, 0x00, 0xAF, 0x00, 0xC1);
	jdi_dcs_write_seq_static(ctx, 0XB5, 0x00, 0xD2, 0x01, 0x07, 0x01, 0x30, 0x01, 0x6E, 0x01,
		0x9D, 0x01, 0xE5, 0x02, 0x1F, 0x02, 0x20);
	jdi_dcs_write_seq_static(ctx, 0XB6, 0x02, 0x57, 0x02, 0x96, 0x02, 0xBF, 0x02, 0xF3, 0x03,
		0x16, 0x03, 0x3F, 0x03, 0x4F, 0x03, 0x5D);
	jdi_dcs_write_seq_static(ctx, 0XB7, 0x03, 0x6D, 0x03, 0x81, 0x03, 0x98, 0x03, 0xAC, 0x03,
		0xCC, 0x03, 0xD8, 0x00, 0x00);
	jdi_dcs_write_seq_static(ctx, 0XB8, 0x00, 0x00, 0x00, 0x20, 0x00, 0x48, 0x00, 0x6A, 0x00,
		0x86, 0x00, 0x9F, 0x00, 0xB5, 0x00, 0xC6);
	jdi_dcs_write_seq_static(ctx, 0XB9, 0x00, 0xD8, 0x01, 0x0D, 0x01, 0x36, 0x01, 0x73, 0x01,
		0xA1, 0x01, 0xE8, 0x02, 0x21, 0x02, 0x22);
	jdi_dcs_write_seq_static(ctx, 0XBA, 0x02, 0x58, 0x02, 0x98, 0x02, 0xC1, 0x02, 0xF7, 0x03,
		0x1B, 0x03, 0x41, 0x03, 0x54, 0x03, 0x66);
	jdi_dcs_write_seq_static(ctx, 0XBB, 0x03, 0x6E, 0x03, 0x82, 0x03, 0x98, 0x03, 0xAC, 0x03,
		0xD0, 0x03, 0xD8, 0x00, 0x00);
	jdi_dcs_write_seq_static(ctx, 0XFF, 0X2B);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB7, 0X06);
	jdi_dcs_write_seq_static(ctx, 0XB8, 0X03);
	jdi_dcs_write_seq_static(ctx, 0XC0, 0X03);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XE0);
	//REGR 0XFE,0XE0
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X82);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XF0);
	//REGR 0XFE,0XF0
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X5A, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X1C, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X33, 0X01);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XD0);
	//REGR 0XFE,0XD0
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X22);
	jdi_dcs_write_seq_static(ctx, 0X54, 0X02);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XC0);
	//CCMON
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X11);
	//CCMOFF
	//CCMRUN
	jdi_dcs_write_seq_static(ctx, 0XFF, 0X10);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X01);//TE Enable
	jdi_dcs_write_seq_static(ctx, 0X51, 0XFF);//Write_Display_Brightness
	jdi_dcs_write_seq_static(ctx, 0X53, 0X0C);//Write_CTRL_Display
	jdi_dcs_write_seq_static(ctx, 0X55, 0X00);//Write CABC

	jdi_dcs_write_seq_static(ctx, 0x11);
	jdi_dcs_write_seq_static(ctx, 0x29);

	jdi_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);

	pr_info("%s-\n", __func__);
}

static int jdi_disable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int jdi_unprepare(struct drm_panel *panel)
{

	struct jdi *ctx = panel_to_jdi(panel);

	if (!ctx->prepared)
		return 0;

	jdi_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	jdi_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(150);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	if (ctx->gate_ic == 0) {
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
#if IS_ENABLED(CONFIG_RT4831A_I2C)
	else if (ctx->gate_ic == 4831) {
		_gate_ic_i2c_panel_bias_enable(0);
		_gate_ic_Power_off();
	}
#endif
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int jdi_prepare(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
	if (ctx->gate_ic == 0) {
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	}
#if IS_ENABLED(CONFIG_RT4831A_I2C)
	else if (ctx->gate_ic == 4831) {
		_gate_ic_Power_on();
		_gate_ic_i2c_panel_bias_enable(1);
	}
#endif
#ifndef BYPASSI2C
	_lcm_i2c_write_bytes(0x0, 0xf);
	_lcm_i2c_write_bytes(0x1, 0xf);
#endif
	jdi_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		jdi_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	jdi_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int jdi_enable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

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
	.clock = FHD_CLK_60,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 80,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + FHD_VFP_60,//VFP
	.vsync_end = 2400 + FHD_VFP_60 + 10,//VSA
	.vtotal = 2400 + FHD_VFP_60 + 10 + 10,//VBP
};

static const struct drm_display_mode performance_mode_90hz = {
	.clock = FHD_CLK_90,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 80,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + FHD_VFP_90,//VFP
	.vsync_end = 2400 + FHD_VFP_90 + 10,//VSA
	.vtotal = 2400 + FHD_VFP_90 + 10 + 10,//VBP
};

static const struct drm_display_mode performance_mode_120hz = {
	.clock = FHD_CLK_120,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 80,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + FHD_VFP_120,//VFP
	.vsync_end = 2400 + FHD_VFP_120 + 10,//VSA
	.vtotal = 2400 + FHD_VFP_120 + 10 + 10,//VBP
};

static const struct drm_display_mode hd_mode_60 = {
	.clock = HD_CLK_60,
	.hdisplay = HD_FRAME_WIDTH,
	.hsync_start = HD_FRAME_WIDTH + HD_HFP,
	.hsync_end = HD_FRAME_WIDTH + HD_HFP + HD_HSA,
	.htotal = HD_FRAME_WIDTH + HD_HFP + HD_HSA + HD_HBP,
	.vdisplay = HD_FRAME_HEIGHT,
	.vsync_start = HD_FRAME_HEIGHT + HD_VFP_60,
	.vsync_end = HD_FRAME_HEIGHT + HD_VFP_60 + HD_VSA,
	.vtotal = HD_FRAME_HEIGHT + HD_VFP_60 + HD_VSA + HD_VBP,
};

static const struct drm_display_mode hd_mode_90 = {
	.clock = HD_CLK_90,
	.hdisplay = HD_FRAME_WIDTH,
	.hsync_start = HD_FRAME_WIDTH + HD_HFP,
	.hsync_end = HD_FRAME_WIDTH + HD_HFP + HD_HSA,
	.htotal = HD_FRAME_WIDTH + HD_HFP + HD_HSA + HD_HBP,
	.vdisplay = HD_FRAME_HEIGHT,
	.vsync_start = HD_FRAME_HEIGHT + HD_VFP_90,
	.vsync_end = HD_FRAME_HEIGHT + HD_VFP_90 + HD_VSA,
	.vtotal = HD_FRAME_HEIGHT + HD_VFP_90 + HD_VSA + HD_VBP,
};

static const struct drm_display_mode hd_mode_120 = {
	.clock = HD_CLK_120,
	.hdisplay = HD_FRAME_WIDTH,
	.hsync_start = HD_FRAME_WIDTH + HD_HFP,
	.hsync_end = HD_FRAME_WIDTH + HD_HFP + HD_HSA,
	.htotal = HD_FRAME_WIDTH + HD_HFP + HD_HSA + HD_HBP,
	.vdisplay = HD_FRAME_HEIGHT,
	.vsync_start = HD_FRAME_HEIGHT + HD_VFP_120,
	.vsync_end = HD_FRAME_HEIGHT + HD_VFP_120 + HD_VSA,
	.vtotal = HD_FRAME_HEIGHT + HD_VFP_120 + HD_VSA + HD_VBP,
};

static const struct drm_display_mode performance_mode_30hz = {
	.clock = 185253,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 80,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 2528,//VFP
	.vsync_end = 2400 + 2528 + 10,//VSA
	.vtotal = 2400 + 2528 + 10 + 10,//VBP
};

static const struct drm_display_mode performance_mode_24hz = {
	.clock = 74101,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 80,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 60,//VFP
	.vsync_end = 2400 + 60 + 10,//VSA
	.vtotal = 2400 + 60 + 10 + 10,//VBP
};

static const struct drm_display_mode performance_mode_10hz = {
	.clock = 61751,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 80,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 2528,//VFP
	.vsync_end = 2400 + 2528 + 10,//VSA
	.vtotal = 2400 + 2528 + 10 + 10,//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 551,
	.vfp_low_power = 4180,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_degree = PROBE_FROM_DTS,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 1,
	.lane_swap_en = 1,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_param_load_mode = 0, //0: default flow; 1: key param only; 2: full control
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1102,
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 556,
		.vfp_lp_dyn = 4178,
		.hfp = 76,
		.vfp = 2590,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 551,
	.vfp_low_power = 2578,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_degree = PROBE_FROM_DTS,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 1,
	.lane_swap_en = 1,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_param_load_mode = 0, //0: default flow; 1: key param only; 2: full control
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,//20
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1102,
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 556,
		.vfp_lp_dyn = 2578,
		.hfp = 76,
		.vfp = 940,
	},
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 551,
	.vfp_low_power = 2578,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_degree = PROBE_FROM_DTS,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 1,
	.lane_swap_en = 1,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_param_load_mode = 0, //0: default flow; 1: key param only; 2: full control
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1102,
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 556,
		.vfp_lp_dyn = 2578,
		.hfp = 76,
		.vfp = 116,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int jdi_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{

	if (level > 255)
		level = 255;
	pr_info("%s backlight = -%d\n", __func__, level);
	bl_tb0[1] = (u8)level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

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

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	dst_fps = m ? drm_mode_vrefresh(m) : -EINVAL;

	if (dst_fps == 60) {
		ext_params.skip_vblank = 0;
		ext->params = &ext_params;
	} else if (dst_fps == 90)
		ext->params = &ext_params_90hz;
	else if (dst_fps == 120) {
		ext_params_120hz.skip_vblank = 0;
		ext->params = &ext_params_120hz;
	} else if (dst_fps == 30) {
		ext_params.skip_vblank = 2;
		ext->params = &ext_params;
	} else if (dst_fps == 24) {
		ext_params_120hz.skip_vblank = 5;
		ext->params = &ext_params_120hz;
	} else if (dst_fps == 10) {
		ext_params.skip_vblank = 6;
		ext->params = &ext_params;
	} else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = drm_mode_vrefresh(m);

	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m) == 120)
		*ext_param = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == 90)
		*ext_param = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 60)
		*ext_param = &ext_params;
	else if (drm_mode_vrefresh(m) == 30)
		*ext_param = &ext_params;
	else if (drm_mode_vrefresh(m) == 24)
		*ext_param = &ext_params;
	else if (drm_mode_vrefresh(m) == 10)
		*ext_param = &ext_params;
	else
		ret = 1;

	if (!ret)
		current_fps = drm_mode_vrefresh(m);

	return ret;
}

enum RES_SWITCH_TYPE mtk_get_res_switch_type(void)
{
	pr_info("res_switch_type: %d\n", res_switch_type);
	return res_switch_type;
}

#define REAL_MODE_NUM           (3)
int mtk_scaling_mode_mapping(int mode_idx)
{
	return (mode_idx % REAL_MODE_NUM);
}

static void mode_switch_to_120(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	pr_info("%s\n", __func__);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x22);//120hz
	jdi_dcs_write_seq_static(ctx, 0xFF, 0x10);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	//cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

}

static void mode_switch_to_90(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	pr_info("%s\n", __func__);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x20);//90hz
	jdi_dcs_write_seq_static(ctx, 0xFF, 0x10);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);

}

static void mode_switch_to_60(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	pr_info("%s\n", __func__);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x21);
	jdi_dcs_write_seq_static(ctx, 0xFF, 0x10);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	dst_fps = m ? drm_mode_vrefresh(m) : -EINVAL;

	if (dst_fps == 60) { /* 60 switch to 120 */
		mode_switch_to_60(panel);
	} else if (dst_fps == 90) { /* 1200 switch to 60 */
		mode_switch_to_90(panel);
	} else if (dst_fps == 120) { /* 1200 switch to 60 */
		mode_switch_to_120(panel);
	} else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct jdi *ctx = panel_to_jdi(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int mtk_set_value(int value)
{
	if (value == 1)
		fake_mode = 1;
	else
		fake_mode = 0;

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = jdi_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch = mode_switch,
	.ata_check = panel_ata_check,
	.set_value = mtk_set_value,
	.get_res_switch_type = mtk_get_res_switch_type,
	.scaling_mode_mapping = mtk_scaling_mode_mapping,
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

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int jdi_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
	struct drm_display_mode *mode4;
	struct drm_display_mode *mode5;
	struct drm_display_mode *mode6;
	struct drm_display_mode *mode7;
	struct drm_display_mode *mode8;
	struct drm_display_mode *mode9;

	mode = drm_mode_duplicate(connector->dev, &performance_mode_120hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_120hz.hdisplay, performance_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode_90hz);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_90hz.hdisplay, performance_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	if (fake_mode == 1) {
		mode4 = drm_mode_duplicate(connector->dev, &performance_mode_30hz);
		if (!mode4) {
			dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				 performance_mode_30hz.hdisplay, performance_mode_30hz.vdisplay,
				 drm_mode_vrefresh(&performance_mode_30hz));
			return -ENOMEM;
		}

		drm_mode_set_name(mode4);
		mode4->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode4);

		mode5 = drm_mode_duplicate(connector->dev, &performance_mode_24hz);
		if (!mode5) {
			dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				 performance_mode_24hz.hdisplay, performance_mode_24hz.vdisplay,
				 drm_mode_vrefresh(&performance_mode_24hz));
			return -ENOMEM;
		}

		drm_mode_set_name(mode5);
		mode5->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode5);

		mode6 = drm_mode_duplicate(connector->dev, &performance_mode_10hz);
		if (!mode6) {
			dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				 performance_mode_10hz.hdisplay, performance_mode_10hz.vdisplay,
				 drm_mode_vrefresh(&performance_mode_10hz));
			return -ENOMEM;
		}

		drm_mode_set_name(mode6);
		mode6->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode6);
	}

	if (res_switch_type == RES_SWITCH_ON_AP) {
		mode7 = drm_mode_duplicate(connector->dev, &hd_mode_60);
		if (!mode7) {
			dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				hd_mode_60.hdisplay, hd_mode_60.vdisplay,
				drm_mode_vrefresh(&hd_mode_60));
			return -ENOMEM;
		}

		drm_mode_set_name(mode7);
		mode7->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode7);

		mode8 = drm_mode_duplicate(connector->dev, &hd_mode_90);
		if (!mode8) {
			dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				hd_mode_90.hdisplay, hd_mode_90.vdisplay,
				drm_mode_vrefresh(&hd_mode_90));
			return -ENOMEM;
		}

		drm_mode_set_name(mode8);
		mode8->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode8);

		mode9 = drm_mode_duplicate(connector->dev, &hd_mode_120);
		if (!mode9) {
			dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				hd_mode_120.hdisplay, hd_mode_120.vdisplay,
				drm_mode_vrefresh(&hd_mode_120));
			return -ENOMEM;
		}

		drm_mode_set_name(mode9);
		mode9->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode9);
	}

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs jdi_drm_funcs = {
	.disable = jdi_disable,
	.unprepare = jdi_unprepare,
	.prepare = jdi_prepare,
	.enable = jdi_enable,
	.get_modes = jdi_get_modes,
};

static int jdi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct jdi *ctx;
	struct device_node *backlight;
	unsigned int res_switch;
	unsigned int value;
	unsigned int lcm_degree;
	int ret;
	int probe_ret;
	pr_info("%s+ jdi,nt36672e,vdo,120hz\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct jdi), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(dev->of_node, "res-switch", &res_switch);
	if (ret < 0)
		res_switch = 0;
	else
		res_switch_type = (enum RES_SWITCH_TYPE)res_switch;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;


	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	if (ctx->gate_ic == 0) {
		ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_info(dev, "cannot get bias-gpios 0 %ld\n",
				 PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		devm_gpiod_put(dev, ctx->bias_pos);

		ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_info(dev, "cannot get bias-gpios 1 %ld\n",
				 PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		devm_gpiod_put(dev, ctx->bias_neg);
	}
	ctx->prepared = true;
	ctx->enabled = true;

	if (of_property_read_bool(dsi_node, "init-panel-off")) {
		ctx->prepared = false;
		ctx->enabled = false;
		pr_info("nt36672e,120hz dsi_node:%s set prepared = enabled = false\n",
					dsi_node->full_name);
	}
	drm_panel_init(&ctx->panel, dev, &jdi_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
	probe_ret = of_property_read_u32(dev->of_node, "lcm-degree", &lcm_degree);
	if (probe_ret < 0)
		lcm_degree = 0;
	else
		ext_params.lcm_degree = lcm_degree;
	pr_info("lcm_degree: %d\n", ext_params.lcm_degree);
#endif
	pr_info("%s- jdi,nt36672e,vdo,120hz\n", __func__);

	return ret;
}

static void jdi_remove(struct mipi_dsi_device *dsi)
{
	struct jdi *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

}

static const struct of_device_id jdi_of_match[] = {
	{
	    .compatible = "jdi,nt36672e,vdo,120hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, jdi_of_match);

static struct mipi_dsi_driver jdi_driver = {
	.probe = jdi_probe,
	.remove = jdi_remove,
	.driver = {
		.name = "panel-jdi-nt36672e-vdo-120hz",
		.owner = THIS_MODULE,
		.of_match_table = jdi_of_match,
	},
};

module_mipi_dsi_driver(jdi_driver);

MODULE_AUTHOR("shaohua deng <shaohua.deng@mediatek.com>");
MODULE_DESCRIPTION("JDI NT36672E VDO 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
