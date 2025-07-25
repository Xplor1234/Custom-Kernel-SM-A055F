// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_helper.h"
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_dp.h"

#define DP_EN							0x0000
	#define DP_CONTROLLER_EN				BIT(0)
	#define CON_FLD_DP_EN					BIT(0)
#define DP_RST							0x0004
	#define CON_FLD_DP_RST_SEL				BIT(16)
	#define CON_FLD_DP_RST					BIT(0)
#define DP_INTEN						0x0008
	#define INT_TARGET_LINE_EN				BIT(3)
	#define INT_UNDERFLOW_EN				BIT(2)
	#define INT_VDE_EN						BIT(1)
	#define INT_VSYNC_EN					BIT(0)
#define DP_INTSTA						0x000C
	#define INTSTA_TARGET_LINE				BIT(3)
	#define INTSTA_UNDERFLOW				BIT(2)
	#define INTSTA_VDE						BIT(1)
	#define INTSTA_VSYNC					BIT(0)
#define DP_CON							0x0010
	#define CON_FLD_DP_INTL_EN				BIT(2)
	#define CON_FLD_DP_BG_EN				BIT(0)
#define DP_OUTPUT_SETTING				0x0014
	#define RB_SWAP							BIT(0)
#define DP_SIZE							0x0018
#define DP_TGEN_HWIDTH					0x0020
#define DP_TGEN_HPORCH					0x0024
#define DP_TGEN_VWIDTH					0x0028
#define DP_TGEN_VPORCH					0x002C
#define DP_BG_HCNTL						0x0030
#define DP_BG_VCNTL						0x0034
#define DP_BG_COLOR						0x0038
#define DP_FIFO_CTL						0x003C
#define DP_STATUS						0x0040
	#define DP_BUSY							BIT(24)
#define DP_DCM							0x004C
#define DP_DUMMY						0x0050
#define DP_YUV							0x0054
	#define YUV422_EN						BIT(2)
	#define YUV_EN							BIT(0)
#define DP_TGEN_VWIDTH_LEVEN			0x0068
#define DP_TGEN_VPORCH_LEVEN			0x006C
#define DP_TGEN_VWIDTH_RODD				0x0070
#define DP_TGEN_VPORCH_RODD				0x0074
#define DP_TGEN_VWIDTH_REVEN			0x0078
#define DP_TGEN_VPORCH_REVEN			0x007C
#define DP_MUTEX_VSYNC_SETTING			0x00E0
    #define MUTEX_VSYNC_SEL					BIT(16)
#define DP_SHEUDO_REG_UPDATE			0x00E4
#define DP_INTERNAL_DCM_DIS				0x00E8
#define DP_TARGET_LINE					0x00F0
#define DP_CHKSUM_EN					0x0100
#define DP_CHKSUM0						0x0104
#define DP_CHKSUM1						0x0108
#define DP_CHKSUM2						0x010C
#define DP_CHKSUM3						0x0110
#define DP_CHKSUM4						0x0114
#define DP_CHKSUM5						0x0118
#define DP_CHKSUM6						0x011C
#define DP_CHKSUM7						0x0120
#define DP_BUF_CON0						0x0210
    #define BUF_BUF_EN						BIT(0)
    #define BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK	BIT(4)
#define DP_BUF_CON1						0x0214
#define DP_BUF_RW_TIMES					0x0220
#define DP_BUF_SODI_HIGH				0x0224
#define DP_BUF_SODI_LOW					0x0228
#define DP_BUF_PREULTRA_HIGH			0x0234
#define DP_BUF_PREULTRA_LOW				0x0238
#define DP_BUF_ULTRA_HIGH				0x023C
#define DP_BUF_ULTRA_LOW				0x0240
#define DP_BUF_URGENT_HIGH				0x0244
#define DP_BUF_URGENT_LOW				0x0248
#define DP_BUF_VDE						0x024C
    #define BUF_VDE_BLOCK_URGENT			BIT(0)
    #define BUF_NON_VDE_FORCE_PREULTRA		BIT(1)
    #define BUF_VDE_BLOCK_ULTRA				BIT(2)
#define DP_SW_NP_SEL					0x0250
#define DP_PATTERN_CTRL0				0x0F00
	#define DP_PATTERN_COLOR_BAR			BIT(6)
    #define DP_PATTERN_EN					BIT(0)
#define DP_PATTERN_CTRL1				0x0F04

static const struct of_device_id mtk_dp_intf_driver_dt_match[];
/**
 * struct mtk_dp_intf - DP_INTF driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_dp_intf {
	struct mtk_ddp_comp	 ddp_comp;
	struct device *dev;
	struct mtk_dp_intf_driver_data *driver_data;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_bridge *bridge;
	void __iomem *regs;
	struct clk *hf_fmm_ck;
	struct clk *hf_fdp_ck;
	struct clk *pclk;
	struct clk *vcore_pclk;
	struct clk *pclk_src[6];
	int irq;
	struct drm_display_mode mode;
	int enable;
	int res;
};

struct mtk_dp_intf_resolution_cfg {
	unsigned int clksrc;
	unsigned int con1;
	unsigned int clk;
};

enum TVDPLL_CLK {
	TCK_26M = 0,
	TVDPLL_D2 = 1,
	TVDPLL_D4 = 2,
	TVDPLL_D8 = 3,
	TVDPLL_D16 = 4,
};

enum MT6897_TVDPLL_CLK {
	MT6897_TCK_26M = 0,
	MT6897_TVDPLL_D4 = 1,
	MT6897_TVDPLL_D8 = 2,
	MT6897_TVDPLL_D16 = 3,
};

enum MT6989_TVDPLL_CLK {
	MT6989_TCK_26M = 0,
	MT6989_TVDPLL_D16 = 1,
	MT6989_TVDPLL_D8 = 2,
	MT6989_TVDPLL_D4 = 3,
	MT6989_TVDPLL_D2 = 4,
};

enum MT6991_TVDPLL_CLK {
	MT6991_TCK_26M = 0,
	MT6991_TVDPLL_D16 = 1,
	MT6991_TVDPLL_D8 = 2,
	MT6991_TVDPLL_D4 = 3,
	MT6991_TVDPLL_D2 = 4,
	MT6991_TVDPLL_PLL = 5,
};

enum MT6899_TVDPLL_CLK {
	MT6899_TCK_26M = 0,
	MT6899_TVDPLL_D16 = 1,
	MT6899_TVDPLL_D8 = 2,
	MT6899_TVDPLL_D4 = 3,
	MT6899_TVDPLL_D2 = 4,
};

static const struct mtk_dp_intf_resolution_cfg mt6895_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x840F81F8
				},
	[SINK_800_600] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_720] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8416DFB4
				},
	[SINK_1280_960] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_1024] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1920_1080] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x8216D89D
				},
	[SINK_1080_2460] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x821AC941
				},
	[SINK_1920_1200] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x8217B645
				},
	[SINK_1920_1440] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_2560_1440] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_2560_1600] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_3840_2160_30] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_3840_2160] = {
					.clksrc = TVDPLL_D2,
					.con1 = 0x8316D89D
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = 0,
					.con1 = 0
				},
};

static const struct mtk_dp_intf_resolution_cfg mt6983_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x840F81F8
				},
	[SINK_800_600] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_720] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8416DFB4
				},
	[SINK_1280_960] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_1024] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1920_1080] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x8216D89D
				},
	[SINK_1920_1080_120_RB] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x82160000
				},
	[SINK_1920_1080_120] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_1080_2460] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x821AC941
				},
	[SINK_1920_1200] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x8217B645
				},
	[SINK_1920_1440] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_2560_1440] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x821293B1
				},
	[SINK_2560_1600] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8214A762
				},
	[SINK_3840_2160_30] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_3840_2160] = {
					.clksrc = TVDPLL_D4,
					.con1 = 0x830F93B1
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = 0,
					.con1 = 0
				},
};

static const struct mtk_dp_intf_resolution_cfg mt6985_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x840F81F8
				},
	[SINK_800_600] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_720] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8416DFB4
				},
	[SINK_1280_960] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_1024] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1920_1080] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x8216D89D
				},
	[SINK_1920_1080_120] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_1080_2460] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x821AC941
				},
	[SINK_1920_1200] = {
					.clksrc = TVDPLL_D16,
					.con1 = 0x8217B645
				},
	[SINK_1920_1440] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_2560_1440] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x821293B1
				},
	[SINK_2560_1600] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8214A762
				},
	[SINK_3840_2160_30] = {
					.clksrc = TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_3840_2160] = {
					.clksrc = TVDPLL_D4,
					.con1 = 0x8216D89D
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = 0,
					.con1 = 0
				},
};

static const struct mtk_dp_intf_resolution_cfg mt6897_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = MT6897_TVDPLL_D16,
					.con1 = 0x840F81F8
				},
	[SINK_800_600] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_720] = {
					.clksrc = MT6897_TVDPLL_D8,
					.con1 = 0x8416DFB4
				},
	[SINK_1280_960] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_1024] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1920_1080] = {
					.clksrc = MT6897_TVDPLL_D16,
					.con1 = 0x8216D89D
				},
	[SINK_1920_1080_120] = {
					.clksrc = MT6897_TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_1080_2460] = {
					.clksrc = MT6897_TVDPLL_D16,
					.con1 = 0x821AC941
				},
	[SINK_1920_1200] = {
					.clksrc = MT6897_TVDPLL_D16,
					.con1 = 0x8217B645
				},
	[SINK_1920_1440] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_2560_1440] = {
					.clksrc = MT6897_TVDPLL_D8,
					.con1 = 0x821293B1
				},
	[SINK_2560_1600] = {
					.clksrc = MT6897_TVDPLL_D8,
					.con1 = 0x8214A762
				},
	[SINK_3840_2160_30] = {
					.clksrc = MT6897_TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_3840_2160] = {
					.clksrc = MT6897_TVDPLL_D4,
					.con1 = 0x8216D89D
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = 0,
					.con1 = 0
				},
};

static const struct mtk_dp_intf_resolution_cfg mt6989_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = MT6989_TVDPLL_D16,
					.con1 = 0x840F81F8
				},
	[SINK_800_600] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_720] = {
					.clksrc = MT6989_TVDPLL_D8,
					.con1 = 0x8416DFB4
				},
	[SINK_1280_960] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1280_1024] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_1920_1080] = {
					.clksrc = MT6989_TVDPLL_D16,
					.con1 = 0x8216D89D
				},
	[SINK_1920_1080_120] = {
					.clksrc = MT6989_TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_1080_2460] = {
					.clksrc = MT6989_TVDPLL_D16,
					.con1 = 0x821AC941
				},
	[SINK_1920_1200] = {
					.clksrc = MT6989_TVDPLL_D16,
					.con1 = 0x8217B645
				},
	[SINK_1920_1440] = {
					.clksrc = 0,
					.con1 = 0
				},
	[SINK_2560_1440] = {
					.clksrc = MT6989_TVDPLL_D8,
					.con1 = 0x821293B1
				},
	[SINK_2560_1600] = {
					.clksrc = MT6989_TVDPLL_D8,
					.con1 = 0x8214A762
				},
	[SINK_3840_2160_30] = {
					.clksrc = MT6989_TVDPLL_D8,
					.con1 = 0x8216D89D
				},
	[SINK_3840_2160] = {
					.clksrc = MT6989_TVDPLL_D4,
					.con1 = 0x8216D89D
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = 0,
					.con1 = 0
				},
};

static const struct mtk_dp_intf_resolution_cfg mt6991_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = MT6991_TVDPLL_D16,
					.con1 = 0x840F81F8,
					.clk = 37125
				},
	[SINK_800_600] = {
					.clksrc = MT6991_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_1280_720] = {
					.clksrc = MT6991_TVDPLL_D8,
					.con1 = 0x8416DFB4,
					.clk = 74250
				},
	[SINK_1280_960] = {
					.clksrc = MT6991_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_1280_1024] = {
					.clksrc = MT6991_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_1920_1080] = {
					.clksrc = MT6991_TVDPLL_D16,
					.con1 = 0x8216D89D,
					.clk = 37125
				},
	[SINK_1920_1080_120] = {
					.clksrc = MT6991_TVDPLL_D8,
					.con1 = 0x8216D89D,
					.clk = 74250
				},
	[SINK_1080_2460] = {
					.clksrc = MT6991_TVDPLL_D16,
					.con1 = 0x821AC941,
					.clk = 37125
				},
	[SINK_1920_1200] = {
					.clksrc = MT6991_TVDPLL_D16,
					.con1 = 0x8217B645,
					.clk = 37125
				},
	[SINK_1920_1440] = {
					.clksrc = MT6991_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_2560_1440] = {
					.clksrc = MT6991_TVDPLL_D8,
					.con1 = 0x821293B1,
					.clk = 74250
				},
	[SINK_2560_1600] = {
					.clksrc = MT6991_TVDPLL_D8,
					.con1 = 0x8214A762,
					.clk = 74250
				},
	[SINK_3840_2160_30] = {
					.clksrc = MT6991_TVDPLL_D8,
					.con1 = 0x8216D89D,
					.clk = 74250
				},
	[SINK_3840_2160] = {
					.clksrc = MT6991_TVDPLL_D4,
					.con1 = 0x8216D89D,
					.clk = 148500
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = MT6991_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
};

static const struct mtk_dp_intf_resolution_cfg mt6899_resolution_cfg[SINK_MAX] = {
	[SINK_640_480] = {
					.clksrc = MT6899_TVDPLL_D16,
					.con1 = 0x840F81F8,
					.clk = 37125
				},
	[SINK_800_600] = {
					.clksrc = MT6899_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_1280_720] = {
					.clksrc = MT6899_TVDPLL_D8,
					.con1 = 0x8416DFB4,
					.clk = 74250
				},
	[SINK_1280_960] = {
					.clksrc = MT6899_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_1280_1024] = {
					.clksrc = MT6899_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_1920_1080] = {
					.clksrc = MT6899_TVDPLL_D16,
					.con1 = 0x8216D89D,
					.clk = 37125
				},
	[SINK_1920_1080_120] = {
					.clksrc = MT6899_TVDPLL_D8,
					.con1 = 0x8216D89D,
					.clk = 74250
				},
	[SINK_1080_2460] = {
					.clksrc = MT6899_TVDPLL_D16,
					.con1 = 0x821AC941,
					.clk = 37125
				},
	[SINK_1920_1200] = {
					.clksrc = MT6899_TVDPLL_D16,
					.con1 = 0x8217B645,
					.clk = 37125
				},
	[SINK_1920_1440] = {
					.clksrc = MT6899_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
	[SINK_2560_1440] = {
					.clksrc = MT6899_TVDPLL_D8,
					.con1 = 0x821293B1,
					.clk = 74250
				},
	[SINK_2560_1600] = {
					.clksrc = MT6899_TVDPLL_D8,
					.con1 = 0x8214A762,
					.clk = 74250
				},
	[SINK_3840_2160_30] = {
					.clksrc = MT6899_TVDPLL_D8,
					.con1 = 0x8216D89D,
					.clk = 74250
				},
	[SINK_3840_2160] = {
					.clksrc = MT6899_TVDPLL_D4,
					.con1 = 0x8216D89D,
					.clk = 148500
				}, //htotal = 1500  //con1 = 0x83109D89; //htotal = 1600
	[SINK_7680_4320] = {
					.clksrc = MT6899_TCK_26M,
					.con1 = 0,
					.clk = 26000
				},
};

struct mtk_dp_intf_video_clock {
	char	compatible[128];
	const struct mtk_dp_intf_resolution_cfg *resolution_cfg;
	unsigned int con0_reg;
	unsigned int con1_reg;
};

static const struct mtk_dp_intf_video_clock mt6895_dp_intf_video_clock = {
	.compatible = "mediatek,mt6895-apmixedsys",
	.resolution_cfg = mt6895_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

static const struct mtk_dp_intf_video_clock mt6983_dp_intf_video_clock = {
	.compatible = "mediatek,mt6983-apmixedsys",
	.resolution_cfg = mt6983_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

static const struct mtk_dp_intf_video_clock mt6985_dp_intf_video_clock = {
	.compatible = "mediatek,mt6985-apmixedsys",
	.resolution_cfg = mt6985_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

static const struct mtk_dp_intf_video_clock mt6897_dp_intf_video_clock = {
	.compatible = "mediatek,mt6897-apmixedsys",
	.resolution_cfg = mt6897_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

static const struct mtk_dp_intf_video_clock mt6989_dp_intf_video_clock = {
	.compatible = "mediatek,mt6989-apmixedsys",
	.resolution_cfg = mt6989_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

static const struct mtk_dp_intf_video_clock mt6991_dp_intf_video_clock = {
	.compatible = "mediatek,mt6991-apmixedsys",
	.resolution_cfg = mt6991_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

static const struct mtk_dp_intf_video_clock mt6899_dp_intf_video_clock = {
	.compatible = "mediatek,mt6899-apmixedsys",
	.resolution_cfg = mt6899_resolution_cfg,
	.con0_reg = 0x248,
	.con1_reg = 0x24C
};

struct mtk_dp_intf_driver_data {
	const u32 reg_cmdq_ofs;
	const u8 np_sel;
	s32 (*poll_for_idle)(struct mtk_dp_intf *dp_intf,
		struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	void (*get_pll_clk)(struct mtk_dp_intf *dp_intf);
	const struct mtk_dp_intf_video_clock *video_clock_cfg;
};

#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();  /*make sure register access in order */ \
	} while (0)

#define DISP_REG_SET(handle, reg32, val) \
	do { \
		if (handle == NULL) { \
			mt_reg_sync_writel(val, (unsigned long *)(reg32));\
		} \
	} while (0)

#ifdef IF_ZERO
#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = __raw_readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			mt_reg_sync_writel(regval, (reg32));  \
		} \
	} while (0)
#else
#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			writel(regval, (reg32));  \
		} \
	} while (0)

#endif

static void __iomem	*clk_apmixed_base;
static int irq_intsa;
static int irq_vdesa;
static int irq_underflowsa;
static int irq_tl;
static unsigned long long dp_intf_bw;
static struct mtk_dp_intf *g_dp_intf;

static inline struct mtk_dp_intf *comp_to_dp_intf(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dp_intf, ddp_comp);
}

static inline struct mtk_dp_intf *encoder_to_dp_intf(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dp_intf, encoder);
}

static inline struct mtk_dp_intf *connector_to_dp_intf(struct drm_connector *c)
{
	return container_of(c, struct mtk_dp_intf, conn);
}

static void mtk_dp_intf_mask(struct mtk_dp_intf *dp_intf, u32 offset,
	u32 mask, u32 data)
{
	u32 temp = readl(dp_intf->regs + offset);

	writel((temp & ~mask) | (data & mask), dp_intf->regs + offset);
}

void dp_intf_dump_reg(void)
{
	u32 i, val[4], reg;

	for (i = 0x0; i < 0x100; i += 16) {
		reg = i;
		val[0] = readl(g_dp_intf->regs + reg);
		val[1] = readl(g_dp_intf->regs + reg + 4);
		val[2] = readl(g_dp_intf->regs + reg + 8);
		val[3] = readl(g_dp_intf->regs + reg + 12);
		DPTXMSG("dp_intf reg[0x%x] = 0x%x 0x%x 0x%x 0x%x\n",
			reg, val[0], val[1], val[2], val[3]);
	}
}

static void mtk_dp_intf_destroy_conn_enc(struct mtk_dp_intf *dp_intf)
{
	drm_encoder_cleanup(&dp_intf->encoder);
	/* Skip connector cleanup if creation was delegated to the bridge */
	if (dp_intf->conn.dev)
		drm_connector_cleanup(&dp_intf->conn);
}

static void mtk_dp_intf_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);

	irq_intsa = 0;
	irq_vdesa = 0;
	irq_underflowsa = 0;
	irq_tl = 0;
	dp_intf_bw = 0;

	mtk_dp_intf_mask(dp_intf, DP_INTSTA, 0xf, 0);
	mtk_ddp_write_mask(comp, 1,
		DP_RST, CON_FLD_DP_RST, handle);
	mtk_ddp_write_mask(comp, 0,
		DP_RST, CON_FLD_DP_RST, handle);
#ifdef IF_ZERO
	mtk_ddp_write_mask(comp,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN),
			DP_INTEN,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN), handle);
#else
	mtk_ddp_write_mask(comp,
			INT_VSYNC_EN,
			DP_INTEN,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN), handle);

#endif
	//mtk_ddp_write_mask(comp, DP_PATTERN_EN,
	//	DP_PATTERN_CTRL0, DP_PATTERN_EN, handle);
	//mtk_ddp_write_mask(comp, DP_PATTERN_COLOR_BAR,
	//	DP_PATTERN_CTRL0, DP_PATTERN_COLOR_BAR, handle);

	mtk_ddp_write_mask(comp, DP_CONTROLLER_EN,
		DP_EN, DP_CONTROLLER_EN, handle);

	dp_intf->enable = 1;
	DPTXMSG("%s, dp_intf_start:0x%x!\n",
		mtk_dump_comp_str(comp), readl(baddr + DP_EN));
	dp_intf_dump_reg();
}

static void mtk_dp_intf_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	//mtk_dp_video_trigger(video_mute<<16 | 0);
	irq_intsa = 0;
	irq_vdesa = 0;
	irq_underflowsa = 0;
	irq_tl = 0;
	dp_intf_bw = 0;

	DPTXMSG("%s, stop\n", mtk_dump_comp_str(comp));
}

static void mtk_dp_intf_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_intf *dp_intf = NULL;
	int ret;

	DPTXFUNC();
	mtk_dp_poweron();

	dp_intf = comp_to_dp_intf(comp);

	/* Enable dp intf clk */
	if (dp_intf != NULL) {
		ret = clk_prepare_enable(dp_intf->hf_fmm_ck);
		if (ret < 0)
			DPTXERR("%s Failed to enable hf_fmm_ck clock: %d\n",
				__func__, ret);
		ret = clk_prepare_enable(dp_intf->hf_fdp_ck);
		if (ret < 0)
			DPTXERR("%s Failed to enable hf_fdp_ck clock: %d\n",
				__func__, ret);
		//ret = clk_prepare_enable(dp_intf->pclk);
		//if (ret < 0)
		//	DPTXERR("%s Failed to enable pclk clock: %d\n",
		//		__func__, ret);
		DPTXMSG("%s:succesed enable dp_intf clock\n", __func__);
	} else
		DPTXERR("Failed to enable dp_intf clock\n");
}

void mtk_dp_intf_unprepare_clk(void)
{
	/* disable dp intf clk */
	if (g_dp_intf != NULL) {
		clk_disable_unprepare(g_dp_intf->pclk);
		DPTXMSG("%s:succesed disable dp_intf and DP sel clock\n", __func__);
	} else
		DPTXERR("Failed to disable dp_intf clock\n");
}
EXPORT_SYMBOL(mtk_dp_intf_unprepare_clk);

static void mtk_dp_intf_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_intf *dp_intf = NULL;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	DPTXFUNC();
	mtk_dp_poweroff();
	udelay(1000);
	mtk_ddp_write_mask(comp, 0x0, DP_EN, DP_CONTROLLER_EN, NULL);
	dp_intf = comp_to_dp_intf(comp);

	/* disable dp intf clk */
	if (dp_intf != NULL) {
		mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
		if(mtk_crtc) {
			priv = mtk_crtc->base.dev->dev_private;
			if(atomic_read(&priv->kernel_pm.status) == KERNEL_SHUTDOWN)
				dptx_shutdown();
		}
		clk_disable_unprepare(dp_intf->hf_fmm_ck);
		clk_disable_unprepare(dp_intf->hf_fdp_ck);
		mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
		priv = mtk_crtc->base.dev->dev_private;
		if (priv->data->mmsys_id != MMSYS_MT6991){
			clk_disable_unprepare(dp_intf->pclk);
			clk_disable_unprepare(dp_intf->vcore_pclk);
		}
		DPTXMSG("%s:succesed disable dp_intf clock\n", __func__);
	} else
		DPTXERR("Failed to disable dp_intf clock\n");
}

void mtk_dp_inf_video_clock(struct mtk_dp_intf *dp_intf)
{
	unsigned int clksrc = TVDPLL_D2;
	unsigned int con1 = 0;
	unsigned int con0_reg;
	unsigned int con1_reg;
	int ret = 0;
	struct device_node *node;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	if (dp_intf == NULL) {
		DPTXERR("%s:input error\n", __func__);
		return;
	}

	if (dp_intf->res >= SINK_MAX || dp_intf->res < 0) {
		DPTXERR("%s:input res error: %d\n", __func__, dp_intf->res);
		dp_intf->res = SINK_1920_1080;
	}

	if (mtk_de_get_clk_debug()) {
		clksrc = mtk_de_get_clksrc();
		con1 = mtk_de_get_con1();
		DPTXMSG("%s:clksrc change: %x, con1 change: %x", __func__,
			dp_intf->driver_data->video_clock_cfg->resolution_cfg[dp_intf->res].clksrc,
			dp_intf->driver_data->video_clock_cfg->resolution_cfg[dp_intf->res].con1);
	} else {
		clksrc = dp_intf->driver_data->video_clock_cfg->resolution_cfg[dp_intf->res].clksrc;
		con1 = dp_intf->driver_data->video_clock_cfg->resolution_cfg[dp_intf->res].con1;
	}
	con0_reg = dp_intf->driver_data->video_clock_cfg->con0_reg;
	con1_reg = dp_intf->driver_data->video_clock_cfg->con1_reg;

	mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id != MMSYS_MT6991) {
		DPTXMSG("%s:clksrc %x,con1 %x,con0_reg %x,con1_reg %x,compatible %s",
		__func__, clksrc, con1, con0_reg, con1_reg,
		dp_intf->driver_data->video_clock_cfg->compatible);
		if (clk_apmixed_base == NULL) {
			node = of_find_compatible_node(NULL, NULL,
				dp_intf->driver_data->video_clock_cfg->compatible);
			if (!node) {
				DPTXERR("dp_intf [CLK_APMIXED] find node failed\n");
				return;
			}
			clk_apmixed_base = of_iomap(node, 0);
			if (clk_apmixed_base == NULL) {
				DPTXERR("dp_intf [CLK_APMIXED] io map failed\n");
				return;
			}
		}
		DPTXMSG("clk_apmixed_base clk_apmixed_base 0x%lx!!!,res %d\n",
			(unsigned long)clk_apmixed_base, dp_intf->res);
		DISP_REG_SET(NULL, clk_apmixed_base + con1_reg, con1);

		/*enable TVDPLL */
		DISP_REG_SET_FIELD(NULL, REG_FLD_MSB_LSB(0, 0),
				clk_apmixed_base + con0_reg, 1);
		ret = clk_prepare_enable(dp_intf->pclk);
		if (ret)
			DPTXMSG("%s clk_prepare_enable dp_intf->pclk: err=%d\n",
				__func__, ret);
	}

	ret = clk_set_parent(dp_intf->pclk, dp_intf->pclk_src[clksrc]);
	if (ret)
		DPTXMSG("%s clk_set_parent dp_intf->pclk: err=%d\n",
			__func__, ret);

	/* dptx vcore clk control */
	if (priv->data->mmsys_id != MMSYS_MT6991) {
		ret = clk_prepare_enable(dp_intf->vcore_pclk);
		ret = clk_set_parent(dp_intf->vcore_pclk, dp_intf->pclk_src[clksrc]);
	}

	DPTXMSG("%s set pclk2 and src %d\n", __func__, clksrc);
}

void mtk_dp_intf_prepare_clk(void)
{
	int ret;

	ret = clk_prepare_enable(g_dp_intf->pclk);
	if (ret < 0)
		DPTXMSG("%s Failed to enable pclk: %d\n",
			__func__, ret);

	ret = clk_set_parent(g_dp_intf->pclk, g_dp_intf->pclk_src[TCK_26M]);
	if (ret < 0)
		DPTXMSG("%s Failed to clk_set_parent: %d\n",
			__func__, ret);

	DPTXMSG("%s dpintf->pclk =  %ld\n",
		__func__, clk_get_rate(g_dp_intf->pclk));
}
EXPORT_SYMBOL(mtk_dp_intf_prepare_clk);

static void mtk_dp_intf_golden_setting(struct mtk_ddp_comp *comp,
					    struct cmdq_pkt *handle)
{
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	unsigned int dp_buf_sodi_high, dp_buf_sodi_low;
	unsigned int dp_buf_preultra_high, dp_buf_preultra_low;
	unsigned int dp_buf_ultra_high, dp_buf_ultra_low;
	unsigned int dp_buf_urgent_high, dp_buf_urgent_low;
	unsigned int mmsys_clk, dp_clk; //{26000, 37125, 74250, 148500, 297000};
	unsigned int twait = 12, twake = 5;
	unsigned int fill_rate, consume_rate;

	if (dp_intf->res >= SINK_MAX || dp_intf->res < 0) {
		DPTXERR("%s:input res error: %d\n", __func__, dp_intf->res);
		dp_intf->res = SINK_1920_1080;
	}

	dp_clk = dp_intf->driver_data->video_clock_cfg->resolution_cfg[dp_intf->res].clk;
	dp_clk = dp_clk > 0 ? dp_clk : 74250;
	mmsys_clk = mtk_drm_get_mmclk(&mtk_crtc->base, __func__) / 1000;
	mmsys_clk = mmsys_clk > 0 ? mmsys_clk : 273000;

	fill_rate = mmsys_clk * 4 / 8;
	consume_rate = dp_clk * 4 / 8;
	DPTXMSG("%s mmsys_clk=%d, dp_intf->res=%d, dp_clk=%d, fill_rate=%d, consume_rate=%d\n",
		__func__, mmsys_clk, dp_intf->res, dp_clk, fill_rate, consume_rate);

	dp_buf_sodi_high = (5940000 - twait * 100 * fill_rate / 1000 - consume_rate) * 30 / 32000;
	dp_buf_sodi_low = (25 + twake) * consume_rate * 30 / 32000;

	dp_buf_preultra_high = 36 * consume_rate * 30 / 32000;
	dp_buf_preultra_low = 35 * consume_rate * 30 / 32000;

	dp_buf_ultra_high = 26 * consume_rate * 30 / 32000;
	dp_buf_ultra_low = 25 * consume_rate * 30 / 32000;

	dp_buf_urgent_high = 12 * consume_rate * 30 / 32000;
	dp_buf_urgent_low = 11 * consume_rate * 30 / 32000;

	DPTXDBG("dp_buf_sodi_high=%d, dp_buf_sodi_low=%d, dp_buf_preultra_high=%d, dp_buf_preultra_low=%d\n",
			dp_buf_sodi_high, dp_buf_sodi_low, dp_buf_preultra_high, dp_buf_preultra_low);

	DPTXDBG("dp_buf_ultra_high=%d, dp_buf_ultra_low=%d dp_buf_urgent_high=%d, dp_buf_urgent_low=%d\n",
			dp_buf_ultra_high, dp_buf_ultra_low, dp_buf_urgent_high, dp_buf_urgent_low);

	mtk_ddp_write_relaxed(comp, dp_buf_sodi_high, DP_BUF_SODI_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_sodi_low, DP_BUF_SODI_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_preultra_high, DP_BUF_PREULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_preultra_low, DP_BUF_PREULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_ultra_high, DP_BUF_ULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_ultra_low, DP_BUF_ULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_urgent_high, DP_BUF_URGENT_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_urgent_low, DP_BUF_URGENT_LOW, handle);

}

static void mtk_dp_intf_golden_setting_mt6899(struct mtk_ddp_comp *comp,
					    struct cmdq_pkt *handle)
{
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	/*mt6899 setting*/
	u32 dp_buf_sodi_high = 2966;
	u32 dp_buf_sodi_low = 2089;
	u32 dp_buf_preultra_high = 2506;
	u32 dp_buf_preultra_low = 2437;
	u32 dp_buf_ultra_high = 1810;
	u32 dp_buf_ultra_low = 1741;
	u32 dp_buf_urgent_high = 836;
	u32 dp_buf_urgent_low = 766;

	DPTXDBG("dp_buf_sodi_high=%d, dp_buf_sodi_low=%d, dp_buf_preultra_high=%d, dp_buf_preultra_low=%d\n",
			dp_buf_sodi_high, dp_buf_sodi_low, dp_buf_preultra_high, dp_buf_preultra_low);

	DPTXDBG("dp_buf_ultra_high=%d, dp_buf_ultra_low=%d dp_buf_urgent_high=%d, dp_buf_urgent_low=%d\n",
			dp_buf_ultra_high, dp_buf_ultra_low, dp_buf_urgent_high, dp_buf_urgent_low);

	mtk_ddp_write_relaxed(comp, dp_buf_sodi_high, DP_BUF_SODI_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_sodi_low, DP_BUF_SODI_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_preultra_high, DP_BUF_PREULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_preultra_low, DP_BUF_PREULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_ultra_high, DP_BUF_ULTRA_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_ultra_low, DP_BUF_ULTRA_LOW, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_urgent_high, DP_BUF_URGENT_HIGH, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_urgent_low, DP_BUF_URGENT_LOW, handle);
}

void mhal_DPTx_VideoClock(bool enable, int resolution)
{
	if (enable) {
		g_dp_intf->res = resolution;
		mtk_dp_inf_video_clock(g_dp_intf);
	} else
		clk_disable_unprepare(g_dp_intf->pclk);
}

static void mtk_dp_intf_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	/*u32 reg_val;*/
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;
	unsigned int hsize = 0, vsize = 0;
	unsigned int hpw = 0;
	unsigned int hfp = 0, hbp = 0;
	unsigned int vpw = 0;
	unsigned int vfp = 0, vbp = 0;
	unsigned int vtotal = 0;
	unsigned int bg_left = 0, bg_right = 0;
	unsigned int bg_top = 0, bg_bot = 0;
	unsigned int rw_times = 0;
	u32 val = 0, line_time;
	u32 dp_vfp_mutex = 0;
	mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;

	DPTXMSG("%s w %d, h, %d, clock %d, fps %d!\n",
			__func__, cfg->w, cfg->h, cfg->clock, cfg->vrefresh);

	hsize = cfg->w;
	vsize = cfg->h;
	if ((cfg->w == 640) && (cfg->h == 480)) {
		dp_intf->res = SINK_640_480;
		hpw = 24;
		hfp = 4;
		hbp = 12;
		vpw = 2;
		vfp = 10;
		vbp = 33;
	} else if ((cfg->w == 1280) && (cfg->h == 720)
	    && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1280_720;
		hpw = 10;
		hfp = 28;
		hbp = 55;
		vpw = 5;
		vfp = 5;
		vbp = 20;
	} else if ((cfg->w == 1920) && (cfg->h == 1080)
		   && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1920_1080;
		hpw = 11;
		hfp = 22;
		hbp = 37;
		vpw = 5;
		vfp = 4;
		vbp = 36;
	} else if ((cfg->w == 1920) && (cfg->h == 1080)
		   && (cfg->vrefresh == 120)) {
		if (cfg->clock == 285500) {
			dp_intf->res = SINK_1920_1080_120_RB;
			hpw = 8;
			hfp = 12;
			hbp = 20;
			vpw = 5;
			vfp = 3;
			vbp = 56;
		} else {
			dp_intf->res = SINK_1920_1080_120;
			hpw = 11;
			hfp = 22;
			hbp = 37;
			vpw = 5;
			vfp = 4;
			vbp = 36;
		}
	} else if ((cfg->w == 1080) && (cfg->h == 2460)
			  && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1080_2460;
		hpw = 8;
		hfp = 8; //30/4
		hbp = 7; //30/4
		vpw = 2;
		vfp = 9;
		vbp = 5;
	} else if ((cfg->w == 1920) && (cfg->h == 1200)
			  && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1920_1200;
		hpw = 8;
		hfp = 12;
		hbp = 20;
		vpw = 6;
		vfp = 3;
		vbp = 26;
	} else if ((cfg->w == 2560) && (cfg->h == 1440)
		   && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_2560_1440;
		hpw = 8;
		hfp = 12;
		hbp = 20;
		vpw = 5;
		vfp = 3;
		vbp = 33;
	} else if ((cfg->w == 2560) && (cfg->h == 1600)
		   && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_2560_1600;
		hpw = 8;
		hfp = 12;
		hbp = 20;
		vpw = 6;
		vfp = 3;
		vbp = 37;
	} else if ((cfg->w == 3840) && (cfg->h == 2160)
		   && (cfg->vrefresh == 30)) {
		dp_intf->res = SINK_3840_2160_30;
		hpw = 22;
		hfp = 44;
		hbp = 74;
		vpw = 10;
		vfp = 8;
		vbp = 72;
	} else if ((cfg->w == 3840) && (cfg->h == 2160)
		   && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_3840_2160;
		hpw = 22;
		hfp = 44;
		hbp = 74;
		vpw = 10;
		vfp = 8;
		vbp = 72;
	} else
		DPTXERR("%s error, w %d, h, %d, fps %d!\n",
			__func__, cfg->w, cfg->h, cfg->vrefresh);

	mtk_dp_inf_video_clock(dp_intf);

	mtk_ddp_write_relaxed(comp, vsize << 16 | hsize,
			DP_SIZE, handle);

	mtk_ddp_write_relaxed(comp, hpw,
			DP_TGEN_HWIDTH, handle);
	mtk_ddp_write_relaxed(comp, hfp << 16 | hbp,
			DP_TGEN_HPORCH, handle);

	mtk_ddp_write_relaxed(comp, vpw,
			DP_TGEN_VWIDTH, handle);
	mtk_ddp_write_relaxed(comp, vfp << 16 | vbp,
			DP_TGEN_VPORCH, handle);

	bg_left  = 0x0;
	bg_right = 0x0;
	mtk_ddp_write_relaxed(comp, bg_left << 16 | bg_right,
			DP_BG_HCNTL, handle);

	bg_top  = 0x0;
	bg_bot  = 0x0;
	mtk_ddp_write_relaxed(comp, bg_top << 16 | bg_bot,
			DP_BG_VCNTL, handle);
#ifdef IF_ZERO
	mtk_ddp_write_mask(comp, DSC_UFOE_SEL,
			DISP_REG_DSC_CON, DSC_UFOE_SEL, handle);
	mtk_ddp_write_relaxed(comp,
			(slice_group_width - 1) << 16 | slice_width,
			DISP_REG_DSC_SLICE_W, handle);
	mtk_ddp_write(comp, 0x20000c03, DISP_REG_DSC_PPS6, handle);
#endif

	if (hsize & 0x3)
		rw_times = ((hsize >> 2) + 1) * vsize;
	else
		rw_times = (hsize >> 2) * vsize;

	mtk_ddp_write_relaxed(comp, rw_times,
			DP_BUF_RW_TIMES, handle);
	mtk_ddp_write_mask(comp, BUF_BUF_EN,
			DP_BUF_CON0, BUF_BUF_EN, handle);
	mtk_ddp_write_mask(comp, BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK,
			DP_BUF_CON0, BUF_BUF_FIFO_UNDERFLOW_DONT_BLOCK, handle);
	mtk_ddp_write_relaxed(comp, dp_intf->driver_data->np_sel,
			DP_SW_NP_SEL, handle);
	if (priv->data->mmsys_id == MMSYS_MT6899)
		mtk_dp_intf_golden_setting_mt6899(comp, handle);
	else
		mtk_dp_intf_golden_setting(comp, handle);
	val = BUF_VDE_BLOCK_URGENT | BUF_NON_VDE_FORCE_PREULTRA | BUF_VDE_BLOCK_ULTRA;
	mtk_ddp_write_relaxed(comp, val, DP_BUF_VDE, handle);

	vtotal = vfp + vpw + vbp + cfg->h;
	line_time = 1000000 / (vtotal * cfg->vrefresh);
	val = (200 + line_time - 1) / line_time;
	val = val | MUTEX_VSYNC_SEL;
	mtk_ddp_write_relaxed(comp, val, DP_MUTEX_VSYNC_SETTING, handle);

	DPTXMSG("%s config done\n",
			mtk_dump_comp_str(comp));

	dp_intf->enable = true;
}

int mtk_dp_intf_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = NULL;

	if (IS_ERR_OR_NULL(comp) || IS_ERR_OR_NULL(comp->regs)) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("(0x0000) DP_EN                 =0x%x\n",
			readl(baddr + DP_EN));
	DDPDUMP("(0x0004) DP_RST                =0x%x\n",
			readl(baddr + DP_RST));
	DDPDUMP("(0x0008) DP_INTEN              =0x%x\n",
			readl(baddr + DP_INTEN));
	DDPDUMP("(0x000C) DP_INTSTA             =0x%x\n",
			readl(baddr + DP_INTSTA));
	DDPDUMP("(0x0010) DP_CON                =0x%x\n",
			readl(baddr + DP_CON));
	DDPDUMP("(0x0014) DP_OUTPUT_SETTING     =0x%x\n",
			readl(baddr + DP_OUTPUT_SETTING));
	DDPDUMP("(0x0018) DP_SIZE               =0x%x\n",
			readl(baddr + DP_SIZE));
	DDPDUMP("(0x0020) DP_TGEN_HWIDTH        =0x%x\n",
			readl(baddr + DP_TGEN_HWIDTH));
	DDPDUMP("(0x0024) DP_TGEN_HPORCH        =0x%x\n",
			readl(baddr + DP_TGEN_HPORCH));
	DDPDUMP("(0x0028) DP_TGEN_VWIDTH        =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH));
	DDPDUMP("(0x002C) DP_TGEN_VPORCH        =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH));
	DDPDUMP("(0x0030) DP_BG_HCNTL           =0x%x\n",
			readl(baddr + DP_BG_HCNTL));
	DDPDUMP("(0x0034) DP_BG_VCNTL           =0x%x\n",
			readl(baddr + DP_BG_VCNTL));
	DDPDUMP("(0x0038) DP_BG_COLOR           =0x%x\n",
			readl(baddr + DP_BG_COLOR));
	DDPDUMP("(0x003C) DP_FIFO_CTL           =0x%x\n",
			readl(baddr + DP_FIFO_CTL));
	DDPDUMP("(0x0040) DP_STATUS             =0x%x\n",
			readl(baddr + DP_STATUS));
	DDPDUMP("(0x004C) DP_DCM                =0x%x\n",
			readl(baddr + DP_DCM));
	DDPDUMP("(0x0050) DP_DUMMY              =0x%x\n",
			readl(baddr + DP_DUMMY));
	DDPDUMP("(0x0068) DP_TGEN_VWIDTH_LEVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_LEVEN));
	DDPDUMP("(0x006C) DP_TGEN_VPORCH_LEVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_LEVEN));
	DDPDUMP("(0x0070) DP_TGEN_VWIDTH_RODD   =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_RODD));
	DDPDUMP("(0x0074) DP_TGEN_VPORCH_RODD   =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_RODD));
	DDPDUMP("(0x0078) DP_TGEN_VWIDTH_REVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_REVEN));
	DDPDUMP("(0x007C) DP_TGEN_VPORCH_REVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_REVEN));
	DDPDUMP("(0x00E0) DP_MUTEX_VSYNC_SETTING=0x%x\n",
			readl(baddr + DP_MUTEX_VSYNC_SETTING));
	DDPDUMP("(0x00E4) DP_SHEUDO_REG_UPDATE  =0x%x\n",
			readl(baddr + DP_SHEUDO_REG_UPDATE));
	DDPDUMP("(0x00E8) DP_INTERNAL_DCM_DIS   =0x%x\n",
			readl(baddr + DP_INTERNAL_DCM_DIS));
	DDPDUMP("(0x00F0) DP_TARGET_LINE        =0x%x\n",
			readl(baddr + DP_TARGET_LINE));
	DDPDUMP("(0x0100) DP_CHKSUM_EN          =0x%x\n",
			readl(baddr + DP_CHKSUM_EN));
	DDPDUMP("(0x0104) DP_CHKSUM0            =0x%x\n",
			readl(baddr + DP_CHKSUM0));
	DDPDUMP("(0x0108) DP_CHKSUM1            =0x%x\n",
			readl(baddr + DP_CHKSUM1));
	DDPDUMP("(0x010C) DP_CHKSUM2            =0x%x\n",
			readl(baddr + DP_CHKSUM2));
	DDPDUMP("(0x0110) DP_CHKSUM3            =0x%x\n",
			readl(baddr + DP_CHKSUM3));
	DDPDUMP("(0x0114) DP_CHKSUM4            =0x%x\n",
			readl(baddr + DP_CHKSUM4));
	DDPDUMP("(0x0118) DP_CHKSUM5            =0x%x\n",
			readl(baddr + DP_CHKSUM5));
	DDPDUMP("(0x011C) DP_CHKSUM6            =0x%x\n",
			readl(baddr + DP_CHKSUM6));
	DDPDUMP("(0x0120) DP_CHKSUM7            =0x%x\n",
			readl(baddr + DP_CHKSUM7));
	DDPDUMP("(0x0210) DP_BUF_CON0      =0x%x\n",
			readl(baddr + DP_BUF_CON0));
	DDPDUMP("(0x0214) DP_BUF_CON1      =0x%x\n",
			readl(baddr + DP_BUF_CON1));
	DDPDUMP("(0x0220) DP_BUF_RW_TIMES      =0x%x\n",
			readl(baddr + DP_BUF_RW_TIMES));
	DDPDUMP("(0x0F00) DP_PATTERN_CTRL0      =0x%x\n",
			readl(baddr + DP_PATTERN_CTRL0));
	DDPDUMP("(0x0F04) DP_PATTERN_CTRL1      =0x%x\n",
			readl(baddr + DP_PATTERN_CTRL1));

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
	DDPDUMP("en=%d, rst_sel=%d, rst=%d, bg_en=%d, intl_en=%d\n",
	DISP_REG_GET_FIELD(CON_FLD_DP_EN, baddr + DP_EN),
	DISP_REG_GET_FIELD(CON_FLD_DP_RST_SEL, baddr + DP_RST),
	DISP_REG_GET_FIELD(CON_FLD_DP_RST, baddr + DP_RST),
	DISP_REG_GET_FIELD(CON_FLD_DP_BG_EN, baddr + DP_CON),
	DISP_REG_GET_FIELD(CON_FLD_DP_INTL_EN, baddr + DP_CON));
	DDPDUMP("== End %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	return 0;
}

unsigned long long mtk_dpintf_get_frame_hrt_bw_base(
		struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_intf *dp_intf)
{
	unsigned long long base_bw;
	unsigned int vtotal, htotal;
	int vrefresh;
	u32 bpp = 4;

	/* for the case dpintf not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->base.state))
		return 1;

	htotal = mtk_crtc->base.state->adjusted_mode.htotal;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	base_bw = div_u64((unsigned long long)vtotal * htotal * vrefresh * bpp, 1000000);

	if (dp_intf_bw != base_bw) {
		dp_intf_bw = base_bw;
		DPTXMSG("%s Frame Bw:%llu, htotal:%d, vtotal:%d, vrefresh:%d\n",
			__func__, base_bw, htotal, vtotal, vrefresh);
	}

	return base_bw;
}

static unsigned long long mtk_dpintf_get_frame_hrt_bw_base_by_mode(
		struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_intf *dp_intf)
{
	unsigned long long base_bw;
	unsigned int vtotal, htotal;
	unsigned int vrefresh;
	u32 bpp = 4;

	/* for the case dpintf not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->avail_modes))
		return 1;

	htotal = mtk_crtc->avail_modes->htotal ;
	vtotal = mtk_crtc->avail_modes->vtotal;
	vrefresh = drm_mode_vrefresh(mtk_crtc->avail_modes);
	base_bw = div_u64((unsigned long long)vtotal * htotal * vrefresh * bpp, 1000000);

	if (dp_intf_bw != base_bw) {
		dp_intf_bw = base_bw;
		DPTXMSG("%s Frame Bw:%llu, htotal:%d, vtotal:%d, vrefresh:%d\n",
			__func__, base_bw, htotal, vtotal, vrefresh);
	}

	return base_bw;
}

static int mtk_dpintf_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);

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
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dp_intf_funcs = {
	.config = mtk_dp_intf_config,
	.start = mtk_dp_intf_start,
	.stop = mtk_dp_intf_stop,
	.prepare = mtk_dp_intf_prepare,
	.unprepare = mtk_dp_intf_unprepare,
	.io_cmd = mtk_dpintf_io_cmd,
};

static int mtk_dp_intf_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_dp_intf *dp_intf = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DPTXMSG("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &dp_intf->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	DPTXMSG("%s-\n", __func__);
	return 0;
}

static void mtk_dp_intf_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_dp_intf *dp_intf = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_dp_intf_destroy_conn_enc(dp_intf);
	mtk_ddp_comp_unregister(drm_dev, &dp_intf->ddp_comp);
}

static const struct component_ops mtk_dp_intf_component_ops = {
	.bind = mtk_dp_intf_bind,
	.unbind = mtk_dp_intf_unbind,
};

static int mtk_dp_intf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_intf *dp_intf;
	enum mtk_ddp_comp_id comp_id;
	const struct of_device_id *of_id;
	struct resource *mem;
	int ret;
	struct device_node *node;

	DPTXMSG("%s+\n", __func__);
	dp_intf = devm_kzalloc(dev, sizeof(*dp_intf), GFP_KERNEL);
	if (!dp_intf)
		return -ENOMEM;
	dp_intf->dev = dev;

	of_id = of_match_device(mtk_dp_intf_driver_dt_match, &pdev->dev);
	if (!of_id) {
		dev_err(dev, "DP_intf device match failed\n");
		return -ENODEV;
	}
	dp_intf->driver_data = (struct mtk_dp_intf_driver_data *)of_id->data;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dp_intf->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(dp_intf->regs)) {
		ret = PTR_ERR(dp_intf->regs);
		dev_err(dev, "Failed to ioremap mem resource: %d\n", ret);
		return ret;
	}

	/* Get dp intf clk
	 * Input pixel clock(hf_fmm_ck) frequency needs to be > hf_fdp_ck * 4
	 * Otherwise FIFO will underflow
	 */
	dp_intf->hf_fmm_ck = devm_clk_get(dev, "hf_fmm_ck");
	if (IS_ERR(dp_intf->hf_fmm_ck)) {
		ret = PTR_ERR(dp_intf->hf_fmm_ck);
		dev_err(dev, "Failed to get hf_fmm_ck clock: %d\n", ret);
		return ret;
	}
	dp_intf->hf_fdp_ck = devm_clk_get(dev, "hf_fdp_ck");
	if (IS_ERR(dp_intf->hf_fdp_ck)) {
		ret = PTR_ERR(dp_intf->hf_fdp_ck);
		dev_err(dev, "Failed to get hf_fdp_ck clock: %d\n", ret);
		return ret;
	}

	if (clk_apmixed_base == NULL) {
		node = of_find_compatible_node(NULL, NULL,
			dp_intf->driver_data->video_clock_cfg->compatible);
		if (!node)
			DPTXERR("[CLK_APMIXED] find node failed\n");
		clk_apmixed_base = of_iomap(node, 0);
		if (clk_apmixed_base == NULL)
			DPTXERR("[CLK_APMIXED] io map failed\n");

		DPTXERR("clk_apmixed_base clk_apmixed_base 0x%lx!!!\n",
			(unsigned long)clk_apmixed_base);
	}

	if (dp_intf->driver_data->get_pll_clk)
		dp_intf->driver_data->get_pll_clk(dp_intf);
	else {
		dp_intf->pclk = devm_clk_get(dev, "MUX_DP");
		dp_intf->pclk_src[1] = devm_clk_get(dev, "TVDPLL_D2");
		dp_intf->pclk_src[2] = devm_clk_get(dev, "TVDPLL_D4");
		dp_intf->pclk_src[3] = devm_clk_get(dev, "TVDPLL_D8");
		dp_intf->pclk_src[4] = devm_clk_get(dev, "TVDPLL_D16");
		if (IS_ERR(dp_intf->pclk)
			|| IS_ERR(dp_intf->pclk_src[0])
			|| IS_ERR(dp_intf->pclk_src[1])
			|| IS_ERR(dp_intf->pclk_src[2])
			|| IS_ERR(dp_intf->pclk_src[3])
			|| IS_ERR(dp_intf->pclk_src[4]))
			dev_err(dev, "Failed to get pclk andr src clock !!!\n");
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DP_INTF);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dp_intf->ddp_comp, comp_id,
				&mtk_dp_intf_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	/* Get dp intf irq num and request irq */
	dp_intf->irq = platform_get_irq(pdev, 0);
	dp_intf->res = SINK_MAX;
	if (dp_intf->irq <= 0) {
		dev_err(dev, "Failed to get irq: %d\n", dp_intf->irq);
		return -EINVAL;
	}

	irq_set_status_flags(dp_intf->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dp_intf->irq, dp_intf->driver_data->irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), dp_intf);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mediatek dp intf irq\n");
		ret = -EPROBE_DEFER;
		return ret;
	}

	platform_set_drvdata(pdev, dp_intf);
	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dp_intf_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	g_dp_intf = dp_intf;
	DPTXMSG("%s-\n", __func__);
	return ret;
}

static int mtk_dp_intf_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dp_intf_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static s32 mtk_dp_intf_poll_for_idle(struct mtk_dp_intf *dp_intf,
	struct cmdq_pkt *handle)
{
	return 0;
}

static void mtk_dp_intf_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D2");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[4] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[0])
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3])
		|| IS_ERR(dp_intf->pclk_src[4]))
		dev_err(dp_intf->dev, "Failed to get pclk andr src clock !!!\n");
}

static void mtk_dp_intf_mt6897_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[0])
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		dev_err(dp_intf->dev, "Failed to get pclk andr src clock !!!\n");
}

static void mtk_dp_intf_mt6989_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	/* Need to config both vcore and vdisplay */
	dp_intf->vcore_pclk = devm_clk_get(dp_intf->dev, "MUX_VCORE_DP");
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->vcore_pclk)
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		dev_err(dp_intf->dev, "Failed to get pclk andr src clock !!!\n");
}

static void mtk_dp_intf_mt6991_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[MT6991_TCK_26M] = devm_clk_get(dp_intf->dev, "DPI_26M");
	dp_intf->pclk_src[MT6991_TVDPLL_D4] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	dp_intf->pclk_src[MT6991_TVDPLL_D8] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[MT6991_TVDPLL_D16] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[MT6991_TVDPLL_PLL] = devm_clk_get(dp_intf->dev, "DPI_CK");

	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[TCK_26M])
		|| IS_ERR(dp_intf->pclk_src[TVDPLL_D4])
		|| IS_ERR(dp_intf->pclk_src[TVDPLL_D8])
		|| IS_ERR(dp_intf->pclk_src[TVDPLL_D16])
		|| IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_PLL]))
		DPTXERR("Failed to get pclk andr src clock, -%d-%d-%d-%d-%d-%d-\n",
			IS_ERR(dp_intf->pclk),
			IS_ERR(dp_intf->pclk_src[TCK_26M]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_D4]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_D8]),
			IS_ERR(dp_intf->pclk_src[TVDPLL_D16]),
			IS_ERR(dp_intf->pclk_src[MT6991_TVDPLL_PLL]));
}

static void mtk_dp_intf_mt6899_get_pll_clk(struct mtk_dp_intf *dp_intf)
{
	/* Need to config both vcore and vdisplay */
	dp_intf->vcore_pclk = devm_clk_get(dp_intf->dev, "MUX_VCORE_DP");
	dp_intf->pclk = devm_clk_get(dp_intf->dev, "MUX_DP");
	dp_intf->pclk_src[1] = devm_clk_get(dp_intf->dev, "TVDPLL_D16");
	dp_intf->pclk_src[2] = devm_clk_get(dp_intf->dev, "TVDPLL_D8");
	dp_intf->pclk_src[3] = devm_clk_get(dp_intf->dev, "TVDPLL_D4");
	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->vcore_pclk)
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		DPTXERR("Failed to get pclk andr src clock, -%d-%d-%d-%d-%d-\n",
			IS_ERR(dp_intf->pclk),
			IS_ERR(dp_intf->vcore_pclk),
			IS_ERR(dp_intf->pclk_src[1]),
			IS_ERR(dp_intf->pclk_src[2]),
			IS_ERR(dp_intf->pclk_src[3]));
}

static irqreturn_t mtk_dp_intf_irq_status(int irq, void *dev_id)
{
	struct mtk_dp_intf *dp_intf = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv = NULL;
	int dpintf_opt = 0;
	mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;
	dpintf_opt = mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_DPINTF_UNDERFLOW_AEE);

	status = readl(dp_intf->regs + DP_INTSTA);

	DRM_MMP_MARK(dp_intf0, status, 0);

	status &= 0xf;
	if (status) {
		mtk_dp_intf_mask(dp_intf, DP_INTSTA, status, 0);
		if (status & INTSTA_VSYNC) {
			// mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
			mtk_crtc_vblank_irq(&mtk_crtc->base);
			irq_intsa++;
		}

		if (status & INTSTA_VDE)
			irq_vdesa++;

		if (status & INTSTA_UNDERFLOW) {
			DPTXMSG("%s dpintf_underflow!\n", __func__);
			irq_underflowsa++;
		}

		if (status & INTSTA_TARGET_LINE)
			irq_tl++;
	}

	if (irq_intsa == 3)
		mtk_dp_video_trigger(video_unmute << 16 | dp_intf->res);

	if (dpintf_opt && (status & INTSTA_UNDERFLOW) && (irq_underflowsa == 1)) {
#if IS_ENABLED(CONFIG_ARM64)
		DDPAEE("DPINTF underflow 0x%x. TS: 0x%08llx\n",
			status, arch_timer_read_counter());
#else
		DDPAEE("DPINTF underflow 0x%x\n",
			status);
#endif
		mtk_drm_crtc_analysis(&(mtk_crtc->base));
		mtk_drm_crtc_dump(&(mtk_crtc->base));
		mtk_smi_dbg_hang_detect("dpintf underflow");
	}

	return IRQ_HANDLED;
}

static const struct mtk_dp_intf_driver_data mt6885_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6983_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6895_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6895_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6985_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6985_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6897_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 0,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6897_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_mt6897_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6989_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 2,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6989_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_mt6989_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6991_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 2,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6991_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_mt6991_get_pll_clk,
};

static const struct mtk_dp_intf_driver_data mt6899_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.np_sel = 2,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
	.video_clock_cfg = &mt6899_dp_intf_video_clock,
	.get_pll_clk = mtk_dp_intf_mt6899_get_pll_clk,
};

static const struct of_device_id mtk_dp_intf_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-dp-intf",
	.data = &mt6885_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6895-dp-intf",
	.data = &mt6895_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6985-dp-intf",
	.data = &mt6985_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6897-dp-intf",
	.data = &mt6897_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6989-dp-intf",
	.data = &mt6989_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6991-dp-intf",
	.data = &mt6991_dp_intf_driver_data},
	{ .compatible = "mediatek,mt6899-dp-intf",
	.data = &mt6899_dp_intf_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dp_intf_driver_dt_match);

struct platform_driver mtk_dp_intf_driver = {
	.probe = mtk_dp_intf_probe,
	.remove = mtk_dp_intf_remove,
	.driver = {
		.name = "mediatek-dp-intf",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dp_intf_driver_dt_match,
	},
};
