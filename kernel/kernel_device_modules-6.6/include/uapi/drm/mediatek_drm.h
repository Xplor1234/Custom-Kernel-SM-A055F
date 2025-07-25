/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _UAPI_MEDIATEK_DRM_H
#define _UAPI_MEDIATEK_DRM_H

#include <drm/drm.h>

#ifndef BIT
#define BIT(nr)                 ((unsigned long)(1) << (nr))
#endif

#define MTK_SUBMIT_NO_IMPLICIT   0x0 /* disable implicit sync */
#define MTK_SUBMIT_IN_FENCE   0x1 /* enable input fence */
#define MTK_SUBMIT_OUT_FENCE  0x2  /* enable output fence */

#define MTK_DRM_PROP_OVERLAP_LAYER_NUM  "OVERLAP_LAYER_NUM"
#define MTK_DRM_PROP_NEXT_BUFF_IDX  "NEXT_BUFF_IDX"
#define MTK_DRM_PROP_PRESENT_FENCE  "PRESENT_FENCE"
#define MAX_DBV_PN 100
#define MAX_FPS_PN 20

struct mml_frame_info;

/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 *	- this size value would be page-aligned internally.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *	- this handle will be set by gem module of kernel side.
 */
struct drm_mtk_gem_create {
	__u64 size;
	__u32 flags;
	__u32 handle;
};

/**
 * A structure for getting buffer offset.
 *
 * @handle: a pointer to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @offset: relatived offset value of the memory region allocated.
 *     - this value should be set by user.
 */
struct drm_mtk_gem_map_off {
	__u32 handle;
	__u32 pad;
	__u64 offset;
};

/**
 * A structure for buffer submit.
 *
 * @type:
 * @session_id:
 * @layer_id:
 * @layer_en:
 * @fb_id:
 * @index:
 * @fence_fd:
 * @interface_index:
 * @interface_fence_fd:
 */
struct drm_mtk_gem_submit {
	__u32 type;
	/* session */
	__u32 session_id;
	/* layer */
	__u32 layer_id;
	__u32 layer_en;
	/* buffer */
	__u32 fb_id;
	/* output */
	__u32 index;
	__s32 fence_fd;
	__u32 interface_index;
	__s32 interface_fence_fd;
	__s32 ion_fd;
};

/**
 * A structure for secure/gem hnd transform.
 *
 * @sec_hnd: handle of secure memory
 * @gem_hnd: handle of gem
 */
struct drm_mtk_sec_gem_hnd {
	__u32 sec_hnd;
	__u32 gem_hnd;
};

/**
 * A structure for session create.
 *
 * @type:
 * @device_id:
 * @mode:
 * @session_id:
 */
struct drm_mtk_session {
	__u32 type;
	/* device */
	__u32 device_id;
	/* mode */
	__u32 mode;
	/* output */
	__u32 session_id;
};

struct msync_level_table {
	unsigned int level_id;
	unsigned int level_fps;
	unsigned int max_fps;
	unsigned int min_fps;
};

struct msync_parameter_table {
	unsigned int msync_max_fps;
	unsigned int msync_min_fps;
	unsigned int msync_level_num;
	struct msync_level_table *level_tb;
};
/* PQ */
#define C_TUN_IDX 19 /* COLOR_TUNING_INDEX */
#define COLOR_TUNING_INDEX 19
#define THSHP_TUNING_INDEX 24
#define THSHP_PARAM_MAX 146 /* TDSHP_3_0 */
#define PARTIAL_Y_INDEX 22
#define GLOBAL_SAT_SIZE 22
#define CONTRAST_SIZE 22
#define BRIGHTNESS_SIZE 22
#define PARTIAL_Y_SIZE 16
#define PQ_HUE_ADJ_PHASE_CNT 4
#define PQ_SAT_ADJ_PHASE_CNT 4
#define PQ_PARTIALS_CONTROL 5
#define PURP_TONE_SIZE 3
#define SKIN_TONE_SIZE 8  /* (-6) */
#define GRASS_TONE_SIZE 6 /* (-2) */
#define SKY_TONE_SIZE 3
#define CCORR_COEF_CNT 4 /* ccorr feature */
#define S_GAIN_BY_Y_CONTROL_CNT 5
#define S_GAIN_BY_Y_HUE_PHASE_CNT 20
#define LSP_CONTROL_CNT 8
#define COLOR_3D_CNT 4 /* color 3D feature */
#define COLOR_3D_WINDOW_CNT 3
#define COLOR_3D_WINDOW_SIZE 45
#define C_3D_CNT 4 /* COLOR_3D_CNT */
#define C_3D_WINDOW_CNT 3
#define C_3D_WINDOW_SIZE 45

enum TONE_ENUM { PURP_TONE = 0, SKIN_TONE = 1, GRASS_TONE = 2, SKY_TONE = 3 };

struct DISP_PQ_WIN_PARAM {
	int split_en;
	int start_x;
	int start_y;
	int end_x;
	int end_y;
};
#define DISP_PQ_WIN_PARAM_T struct DISP_PQ_WIN_PARAM

struct DISP_PQ_MAPPING_PARAM {
	int image;
	int video;
	int camera;
};

#define DISP_PQ_MAPPING_PARAM_T struct DISP_PQ_MAPPING_PARAM

struct MDP_COLOR_CAP {
	unsigned int en;
	unsigned int pos_x;
	unsigned int pos_y;
};

struct MDP_TDSHP_REG {
	unsigned int TDS_GAIN_MID;
	unsigned int TDS_GAIN_HIGH;
	unsigned int TDS_COR_GAIN;
	unsigned int TDS_COR_THR;
	unsigned int TDS_COR_ZERO;
	unsigned int TDS_GAIN;
	unsigned int TDS_COR_VALUE;
};
struct DISPLAY_PQ_T {

	unsigned int GLOBAL_SAT[GLOBAL_SAT_SIZE];
	unsigned int CONTRAST[CONTRAST_SIZE];
	unsigned int BRIGHTNESS[BRIGHTNESS_SIZE];
	unsigned int PARTIAL_Y[PARTIAL_Y_INDEX][PARTIAL_Y_SIZE];
	unsigned int PURP_TONE_S[COLOR_TUNING_INDEX]
				[PQ_PARTIALS_CONTROL][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_S[COLOR_TUNING_INDEX]
				[PQ_PARTIALS_CONTROL][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_S[COLOR_TUNING_INDEX]
				[PQ_PARTIALS_CONTROL][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_S[COLOR_TUNING_INDEX]
			       [PQ_PARTIALS_CONTROL][SKY_TONE_SIZE];
	unsigned int PURP_TONE_H[COLOR_TUNING_INDEX][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_H[COLOR_TUNING_INDEX][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_H[COLOR_TUNING_INDEX][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_H[COLOR_TUNING_INDEX][SKY_TONE_SIZE];
	unsigned int CCORR_COEF[CCORR_COEF_CNT][3][3];
	unsigned int S_GAIN_BY_Y[5][S_GAIN_BY_Y_HUE_PHASE_CNT];
	unsigned int S_GAIN_BY_Y_EN;
	unsigned int LSP_EN;
	unsigned int LSP[LSP_CONTROL_CNT];
	unsigned int COLOR_3D[4][COLOR_3D_WINDOW_CNT][COLOR_3D_WINDOW_SIZE];
};
#define DISPLAY_PQ struct DISPLAY_PQ_T

struct DISPLAY_COLOR_REG {
	unsigned int GLOBAL_SAT;
	unsigned int CONTRAST;
	unsigned int BRIGHTNESS;
	unsigned int PARTIAL_Y[PARTIAL_Y_SIZE];
	unsigned int PURP_TONE_S[PQ_PARTIALS_CONTROL][PURP_TONE_SIZE];
	unsigned int SKIN_TONE_S[PQ_PARTIALS_CONTROL][SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_S[PQ_PARTIALS_CONTROL][GRASS_TONE_SIZE];
	unsigned int SKY_TONE_S[PQ_PARTIALS_CONTROL][SKY_TONE_SIZE];
	unsigned int PURP_TONE_H[PURP_TONE_SIZE];
	unsigned int SKIN_TONE_H[SKIN_TONE_SIZE];
	unsigned int GRASS_TONE_H[GRASS_TONE_SIZE];
	unsigned int SKY_TONE_H[SKY_TONE_SIZE];
	unsigned int S_GAIN_BY_Y[S_GAIN_BY_Y_CONTROL_CNT]
				[S_GAIN_BY_Y_HUE_PHASE_CNT];
	unsigned int S_GAIN_BY_Y_EN;
	unsigned int LSP_EN;
	unsigned int COLOR_3D[COLOR_3D_WINDOW_CNT][COLOR_3D_WINDOW_SIZE];
};
#define DISPLAY_COLOR_REG_T struct DISPLAY_COLOR_REG


#define DISP_COLOR_TM_MAX 4
struct DISP_COLOR_TRANSFORM {
	int matrix[DISP_COLOR_TM_MAX][DISP_COLOR_TM_MAX];
};

struct DISPLAY_TDSHP_T {

	unsigned int entry[THSHP_TUNING_INDEX][THSHP_PARAM_MAX];

};
#define DISPLAY_TDSHP struct DISPLAY_TDSHP_T

enum PQ_DS_index_t {
	DS_en = 0,
	iUpSlope,
	iUpThreshold,
	iDownSlope,
	iDownThreshold,
	iISO_en,
	iISO_thr1,
	iISO_thr0,
	iISO_thr3,
	iISO_thr2,
	iISO_IIR_alpha,
	iCorZero_clip2,
	iCorZero_clip1,
	iCorZero_clip0,
	iCorThr_clip2,
	iCorThr_clip1,
	iCorThr_clip0,
	iCorGain_clip2,
	iCorGain_clip1,
	iCorGain_clip0,
	iGain_clip2,
	iGain_clip1,
	iGain_clip0,
	PQ_DS_INDEX_MAX
};

struct DISP_PQ_DS_PARAM {
	int param[PQ_DS_INDEX_MAX];
};
#define DISP_PQ_DS_PARAM_T struct DISP_PQ_DS_PARAM

enum PQ_DC_index_t {
	BlackEffectEnable = 0,
	WhiteEffectEnable,
	StrongBlackEffect,
	StrongWhiteEffect,
	AdaptiveBlackEffect,
	AdaptiveWhiteEffect,
	ScenceChangeOnceEn,
	ScenceChangeControlEn,
	ScenceChangeControl,
	ScenceChangeTh1,
	ScenceChangeTh2,
	ScenceChangeTh3,
	ContentSmooth1,
	ContentSmooth2,
	ContentSmooth3,
	MiddleRegionGain1,
	MiddleRegionGain2,
	BlackRegionGain1,
	BlackRegionGain2,
	BlackRegionRange,
	BlackEffectLevel,
	BlackEffectParam1,
	BlackEffectParam2,
	BlackEffectParam3,
	BlackEffectParam4,
	WhiteRegionGain1,
	WhiteRegionGain2,
	WhiteRegionRange,
	WhiteEffectLevel,
	WhiteEffectParam1,
	WhiteEffectParam2,
	WhiteEffectParam3,
	WhiteEffectParam4,
	ContrastAdjust1,
	ContrastAdjust2,
	DCChangeSpeedLevel,
	ProtectRegionEffect,
	DCChangeSpeedLevel2,
	ProtectRegionWeight,
	DCEnable,
	DarkSceneTh,
	DarkSceneSlope,
	DarkDCGain,
	DarkACGain,
	BinomialTh,
	BinomialSlope,
	BinomialDCGain,
	BinomialACGain,
	BinomialTarRange,
	bIIRCurveDiffSumTh,
	bIIRCurveDiffMaxTh,
	bGlobalPQEn,
	bHistAvoidFlatBgEn,
	PQDC_INDEX_MAX
};
#define PQ_DC_index enum PQ_DC_index_t

struct DISP_PQ_DC_PARAM {
	int param[PQDC_INDEX_MAX];
};

/* OD */
struct DISP_OD_CMD {
	unsigned int size;
	unsigned int type;
	unsigned int ret;
	unsigned long param0;
	unsigned long param1;
	unsigned long param2;
	unsigned long param3;
};


struct DISP_WRITE_REG {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct DISP_READ_REG {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

enum disp_ccorr_id_t {
	DISP_CCORR0 = 0,
	DISP_CCORR1,
	DISP_CCORR2,
	DISP_CCORR3,
	DISP_CCORR_TOTAL
};
enum drm_disp_ccorr_linear_t {
	DRM_DISP_NONLINEAR = 0,
	DRM_DISP_LINEAR,
};

struct DISP_CCORR_COEF_T {
	enum disp_ccorr_id_t hw_id;
	unsigned int coef[3][3];
};

#define DISP_GAMMA_LUT_SIZE 512
#define DISP_GAMMA_12BIT_LUT_SIZE 1024

enum disp_gamma_id_t {
	DISP_GAMMA0 = 0,
	DISP_GAMMA1,
	DISP_GAMMA_TOTAL
};


struct DISP_GAMMA_LUT_T {
	enum disp_gamma_id_t hw_id;
	unsigned int lut[DISP_GAMMA_LUT_SIZE];
};

struct DISP_GAMMA_12BIT_LUT_T {
	enum disp_gamma_id_t hw_id;
	unsigned int lut_0[DISP_GAMMA_12BIT_LUT_SIZE];
	unsigned int lut_1[DISP_GAMMA_12BIT_LUT_SIZE];
};

struct DISP_PQ_PARAM {
	unsigned int u4SHPGain;  /* 0 : min , 9 : max. */
	unsigned int u4SatGain;  /* 0 : min , 9 : max. */
	unsigned int u4PartialY; /* 0 : min , 9 : max. */
	unsigned int u4HueAdj[PQ_HUE_ADJ_PHASE_CNT];
	unsigned int u4SatAdj[PQ_SAT_ADJ_PHASE_CNT];
	unsigned int u4Contrast;   /* 0 : min , 9 : max. */
	unsigned int u4Brightness; /* 0 : min , 9 : max. */
	unsigned int u4Ccorr;      /* 0 : min , 3 : max. ccorr feature */
	unsigned int u4ColorLUT; /* 0 : min , 3 : max.  ccorr feature */
};
#define DISP_PQ_PARAM_T struct DISP_PQ_PARAM

struct DISP_DITHER_PARAM {
	_Bool relay;
	__u32 mode;
};

struct DISP_AAL_TRIG_STATE {
	int curAli;
	int aliThreshold;
	unsigned int dre_frm_trigger;
	unsigned int dre3_en_state;
	unsigned int dre3_krn_flag;
};


#define DRM_MTK_GEM_CREATE		0x00
#define DRM_MTK_GEM_MAP_OFFSET		0x01
#define DRM_MTK_GEM_SUBMIT		0x02
#define DRM_MTK_SESSION_CREATE		0x03
#define DRM_MTK_SESSION_DESTROY		0x04
#define DRM_MTK_LAYERING_RULE           0x05
#define DRM_MTK_CRTC_GETFENCE           0x06
#define DRM_MTK_WAIT_REPAINT            0x07
#define DRM_MTK_GET_DISPLAY_CAPS	0x08
#define DRM_MTK_SET_DDP_MODE   0x09
#define DRM_MTK_GET_SESSION_INFO	0x0A
#define DRM_MTK_SEC_HND_TO_GEM_HND	0x0B
#define DRM_MTK_GET_MASTER_INFO		0x0C
#define DRM_MTK_CRTC_GETSFFENCE         0x0D
#define DRM_MTK_MML_GEM_SUBMIT         0x0E
#define DRM_MTK_SET_MSYNC_PARAMS         0x0F
#define DRM_MTK_GET_MSYNC_PARAMS         0x10
#define DRM_MTK_FACTORY_LCM_AUTO_TEST    0x11
#define DRM_MTK_DRM_SET_LEASE_INFO    0x12
#define DRM_MTK_DRM_GET_LEASE_INFO    0x13
#define DRM_MTK_CRTC_FENCE_REL           0x14
#define DRM_MTK_GET_MODE_EXT_INFO 0x15
#define DRM_MTK_HWVSYNC_ON 0x16
#define DRM_MTK_DUMMY_CMD_ON 0x17

/* PQ */
#define DRM_MTK_GET_LCM_INDEX   0x2C
#define DRM_MTK_SUPPORT_COLOR_TRANSFORM    0x2D
#define DRM_MTK_PQ_PROXY_IOCTL 0x37

#define DRM_MTK_SET_OVL_LAYER	0x4A
#define DRM_MTK_MAP_DMA_BUF	0x4B
#define DRM_MTK_UNMAP_DMA_BUF	0x4C

#define DRM_MTK_HDMI_GET_DEV_INFO	0x3A
#define DRM_MTK_HDMI_AUDIO_ENABLE	0x3B
#define DRM_MTK_HDMI_AUDIO_CONFIG	0x3C
#define DRM_MTK_HDMI_GET_CAPABILITY	0x3D

#define DRM_MTK_DEBUG_LOG			0x3E

/* CHIST */
#define DRM_MTK_GET_CHIST           0x46
#define DRM_MTK_GET_CHIST_CAPS      0x47
#define DRM_MTK_SET_CHIST_CONFIG    0x48

#define DRM_MTK_GET_PQ_CAPS 0x54
#define DRM_MTK_SET_PQ_CAPS 0x55

#define DRM_MTK_ODDMR_LOAD_PARAM 0x57
#define DRM_MTK_ODDMR_CTL 0x58
#define DRM_MTK_GET_PANELS_INFO 0x5a

#define DRM_MTK_KICK_IDLE 0x5b

#define DRM_MTK_PQ_FRAME_CONFIG 0x5c

/* DISP_ESD_CEHCK */
#define DRM_MTK_ESD_STAT_CHK 0x5d

/* DISP to MML */
#define DRM_MTK_MML_CTRL 0x5f

/* The device specific ioctl range is from DRM_COMMAND_BASE(0x40) to DRM_COMMAND_END(0x9f)
 * The index of ioctl which define here must be less then 0x60
 */

/* C3D */
#define DISP_C3D_1DLUT_SIZE 32

struct DISP_C3D_LUT {
	unsigned int lut1d[DISP_C3D_1DLUT_SIZE];
	unsigned long long lut3d;
	int bin_num;
};

enum MTKFB_DISPIF_TYPE {
	DISPIF_TYPE_DBI = 0,
	DISPIF_TYPE_DPI,
	DISPIF_TYPE_DSI,
	DISPIF_TYPE_DPI0,
	DISPIF_TYPE_DPI1,
	DISPIF_TYPE_DSI0,
	DISPIF_TYPE_DSI1,
	HDMI = 7,
	HDMI_SMARTBOOK,
	MHL,
	DISPIF_TYPE_EPD,
	DISPLAYPORT,
	SLIMPORT
};

enum MTKFB_DISPIF_MODE {
	DISPIF_MODE_VIDEO = 0,
	DISPIF_MODE_COMMAND
};

struct mtk_dispif_info {
	unsigned int display_id;
	unsigned int isHwVsyncAvailable;
	enum MTKFB_DISPIF_TYPE displayType;
	unsigned int displayWidth;
	unsigned int displayHeight;
	unsigned int displayFormat;
	enum MTKFB_DISPIF_MODE displayMode;
	unsigned int vsyncFPS;
	unsigned int physicalWidth;
	unsigned int physicalHeight;
	unsigned int isConnected;
	unsigned int lcmOriginalWidth;
	unsigned int lcmOriginalHeight;
};

#define DRM_IOCTL_MTK_SET_DDP_MODE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SET_DDP_MODE, unsigned int)

enum MTK_DRM_SESSION_MODE {
	MTK_DRM_SESSION_INVALID = 0,
	/* single output */
	MTK_DRM_SESSION_DL,

	/* two ouputs */
	MTK_DRM_SESSION_DOUBLE_DL,
	MTK_DRM_SESSION_DC_MIRROR,

	/* three session at same time */
	MTK_DRM_SESSION_TRIPLE_DL,
	MTK_DRM_SESSION_NUM,
};

#define MAX_FRAME_RATIO_NUMBER (10)
#define MAX_LAYER_RATIO_NUMBER (12)
#define MAX_EMI_EFF_LEVEL (16)
#define BWM_GPUC_TUNING_FRAME (5)

enum MTK_LAYERING_CAPS {
	MTK_LAYERING_OVL_ONLY = 0x00000001,
	MTK_MDP_RSZ_LAYER =		0x00000002,
	MTK_DISP_RSZ_LAYER =	0x00000004,
	MTK_MDP_ROT_LAYER =		0x00000008,
	MTK_MDP_HDR_LAYER =		0x00000010,
	MTK_NO_FBDC =			0x00000020,
	MTK_CLIENT_CLEAR_LAYER =	0x00000040,
	MTK_DISP_CLIENT_CLEAR_LAYER =	0x00000080,
	MTK_DMDP_RSZ_LAYER =		0x00000100,
	MTK_MML_OVL_LAYER =	0x00000200,
	MTK_MML_DISP_DIRECT_LINK_LAYER =	0x00000400,
	MTK_MML_DISP_DIRECT_DECOUPLE_LAYER =	0x00000800,
	MTK_MML_DISP_DECOUPLE_LAYER =	0x00001000,
	MTK_MML_DISP_MDP_LAYER =	0x00002000,
	MTK_MML_DISP_NOT_SUPPORT =	0x00004000,
	MTK_HWC_UNCHANGED_LAYER = 0x00008000,
	MTK_HWC_INACTIVE_LAYER = 0x00010000,
	MTK_HWC_UNCHANGED_FBT_LAYER = 0x00020000,
	MTK_DISP_UNCHANGED_RATIO_VALID = 0x00040000,
	MTK_DISP_FBT_RATIO_VALID = 0x00080000,
	MTK_DISP_CLIENT_LAYER = 0x00100000,
	MTK_DISP_SRC_YUV_LAYER = 0x00200000,
	MTK_MML_DISP_DECOUPLE2_LAYER =	0x00400000,
};

enum MTK_DISP_CAPS {
	MTK_GLES_FBT_GET_RATIO = 0x00000001,
	MTK_GLES_FBT_UNCHANGED = 0x00000002,
	MTK_NEED_REPAINT       = 0x00000004,
};

struct drm_mtk_layer_config {
	__u32 ovl_id;
	__u32 src_fmt;
	int dataspace;
	__u32 dst_offset_x, dst_offset_y;
	__u32 dst_width, dst_height;
	int ext_sel_layer;
	__u32 src_offset_x, src_offset_y;
	__u32 src_width, src_height;
	__u32 layer_caps;
	__u32 clip; /* drv internal use */
	__u64 buffer_alloc_id;
	__u8 compress;
	__u8 secure;
};

struct wb_frame_info {
	__u32 fmt;
	__u32 src_width, src_height;
	__u32 dst_width, dst_height;
};
#define LYE_CRTC 4
struct drm_mtk_layering_info {
	struct drm_mtk_layer_config *input_config[LYE_CRTC];
	int disp_mode[LYE_CRTC];
	/* index of crtc display mode including resolution, fps... */
	int disp_mode_idx[LYE_CRTC];
	int layer_num[LYE_CRTC];
	int gles_head[LYE_CRTC];
	int gles_tail[LYE_CRTC];
	__u32 disp_caps[LYE_CRTC];
	__u32 frame_idx[LYE_CRTC];
	int hrt_num;
	__u32 disp_idx;
	__u32 disp_list;
	/* res_idx: SF/HWC selects which resolution to use */
	int res_idx;
	__u32 hrt_weight;
	__u32 hrt_idx;
	struct mml_frame_info *mml_cfg[LYE_CRTC];
	struct wb_frame_info wb_cfg[LYE_CRTC];
	__u32 exec_reserved_time;
};

/**
 * A structure for fence retrival.
 *
 * @crtc_id:
 * @fence_fd:
 * @fence_idx:
 */
struct drm_mtk_fence {
	/* input */
	__u32 crtc_id; /**< Id */

	/* output */
	__s32 fence_fd;
	/* device */
	__u32 fence_idx;
};

enum DRM_REPAINT_TYPE {
	DRM_WAIT_FOR_REPAINT,
	DRM_REPAINT_FOR_ANTI_LATENCY,
	DRM_REPAINT_FOR_SWITCH_DECOUPLE,
	DRM_REPAINT_FOR_SWITCH_DECOUPLE_MIRROR,
	DRM_REPAINT_FOR_IDLE,
	DRM_REPAINT_FOR_MML,
	DRM_REPAINT_TYPE_NUM,
};

enum MTK_DRM_DISP_FEATURE {
	DRM_DISP_FEATURE_HRT = 0x00000001,
	DRM_DISP_FEATURE_DISP_SELF_REFRESH = 0x00000002,
	DRM_DISP_FEATURE_RPO = 0x00000004,
	DRM_DISP_FEATURE_FORCE_DISABLE_AOD = 0x00000008,
	DRM_DISP_FEATURE_OUTPUT_ROTATED = 0x00000010,
	DRM_DISP_FEATURE_THREE_SESSION = 0x00000020,
	DRM_DISP_FEATURE_FBDC = 0x00000040,
	DRM_DISP_FEATURE_SF_PRESENT_FENCE = 0x00000080,
	DRM_DISP_FEATURE_PQ_34_COLOR_MATRIX = 0x00000100,
	/*Msync*/
	DRM_DISP_FEATURE_MSYNC2_0 = 0x00000200,
	DRM_DISP_FEATURE_MML_PRIMARY = 0x00000400,
	DRM_DISP_FEATURE_VIRUTAL_DISPLAY = 0x00000800,
	DRM_DISP_FEATURE_IOMMU = 0x00001000,
	DRM_DISP_FEATURE_OVL_BW_MONITOR = 0x00002000,
	DRM_DISP_FEATURE_GPU_CACHE = 0x00004000,
	DRM_DISP_FEATURE_SPHRT = 0x00008000,
	DRM_DISP_FEATURE_PARTIAL_UPDATE = 0x00010000,
};

enum MTK_DRM_DUMP_POINT {
	MTK_DRM_BEFORE_PQ,
	MTK_DRM_MID,
	MTK_DRM_AFTER_PQ,
	MTK_DRM_MML,
	MTK_DRM_DUMP_POINT_NUM,
};

enum mtk_mmsys_id {
	MMSYS_MT2701 = 0x2701,
	MMSYS_MT2712 = 0x2712,
	MMSYS_MT8173 = 0x8173,
	MMSYS_MT6779 = 0x6779,
	MMSYS_MT6761 = 0x6761,
	MMSYS_MT6765 = 0x6765,
	MMSYS_MT6768 = 0x6768,
	MMSYS_MT6885 = 0x6885,
	MMSYS_MT6983 = 0x6983,
	MMSYS_MT6985 = 0x6985,
	MMSYS_MT6873 = 0x6873,
	MMSYS_MT6853 = 0x6853,
	MMSYS_MT6833 = 0x6833,
	MMSYS_MT6877 = 0x6877,
	MMSYS_MT6781 = 0x6781,
	MMSYS_MT6879 = 0x6879,
	MMSYS_MT6895 = 0x6895,
	MMSYS_MT6886 = 0x6886,
	MMSYS_MT6855 = 0x6855,
	MMSYS_MT6897 = 0x6897,
	MMSYS_MT6835 = 0x6835,
	MMSYS_MT6989 = 0x6989,
	MMSYS_MT6899 = 0x6899,
	MMSYS_MT6991 = 0x6991,
	MMSYS_MAX,
};

#define MTK_DRM_COLOR_FORMAT_A_BIT (1 << MTK_DRM_COLOR_FORMAT_A)
#define MTK_DRM_COLOR_FORMAT_R_BIT (1 << MTK_DRM_COLOR_FORMAT_R)
#define MTK_DRM_COLOR_FORMAT_G_BIT (1 << MTK_DRM_COLOR_FORMAT_G)
#define MTK_DRM_COLOR_FORMAT_B_BIT (1 << MTK_DRM_COLOR_FORMAT_B)
#define MTK_DRM_COLOR_FORMAT_Y_BIT (1 << MTK_DRM_COLOR_FORMAT_Y)
#define MTK_DRM_COLOR_FORMAT_U_BIT (1 << MTK_DRM_COLOR_FORMAT_U)
#define MTK_DRM_COLOR_FORMAT_V_BIT (1 << MTK_DRM_COLOR_FORMAT_V)
#define MTK_DRM_COLOR_FORMAT_S_BIT (1 << MTK_DRM_COLOR_FORMAT_S)
#define MTK_DRM_COLOR_FORMAT_H_BIT (1 << MTK_DRM_COLOR_FORMAT_H)
#define MTK_DRM_COLOR_FORMAT_M_BIT (1 << MTK_DRM_COLOR_FORMAT_M)


#define MTK_DRM_DISP_CHIST_CHANNEL_COUNT 7

enum MTK_DRM_CHIST_COLOR_FORMT {
	MTK_DRM_COLOR_FORMAT_A,
	MTK_DRM_COLOR_FORMAT_R,
	MTK_DRM_COLOR_FORMAT_G,
	MTK_DRM_COLOR_FORMAT_B,
	MTK_DRM_COLOR_FORMAT_Y,
	MTK_DRM_COLOR_FORMAT_U,
	MTK_DRM_COLOR_FORMAT_V,
	MTK_DRM_COLOR_FORMAT_H,
	MTK_DRM_COLOR_FORMAT_S,
	MTK_DRM_COLOR_FORMAT_M,
	MTK_DRM_COLOR_FORMAT_MAX
};

enum MTK_DRM_CHIST_CALLER {
	MTK_DRM_CHIST_CALLER_PQ,
	MTK_DRM_CHIST_CALLER_HWC,
	MTK_DRM_CHIST_CALLER_UNKONW
};

enum MTK_PLANE_MODE {
	MTK_PLANE_HWC,
	MTK_PLANE_SIDEBAND,
};

enum MTK_PANEL_ID {
	MTK_PANEL_DSI0,
	MTK_PANEL_EDP,
	MTK_PANEL_DP0,
	MTK_PANEL_DP1,
	MTK_PANEL_DSI1_0,
	MTK_PANEL_DSI1_1,
	MTK_PANEL_DSI2,
};

enum MTK_FB_USER_TYPE {
	MTK_USER_NORMAL,
	MTK_USER_AVM,
};

struct mtk_drm_layer_info {
	enum MTK_PANEL_ID panel_id;
	unsigned int layer_id;
	unsigned int layer_en;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int crop_w;
	unsigned int crop_h;
	unsigned int tgt_x;
	unsigned int tgt_y;
	unsigned int tgt_w;
	unsigned int tgt_h;
	unsigned int src_format;
	unsigned long long pts;

	void *src_vaddr;
	uint64_t src_mvaddr;
	int dma_fd;
	bool compress;
	enum MTK_FB_USER_TYPE user_type;
};

struct mtk_drm_disp_caps_info {
	unsigned int hw_ver;
	unsigned int disp_feature_flag;
	int lcm_degree; /* for rotate180 */
	unsigned int rsz_in_max[2]; /* for RPO { width, height } */

	/* for WCG */
	int lcm_color_mode;
	unsigned int max_luminance;
	unsigned int average_luminance;
	unsigned int min_luminance;

	/* for color histogram */
	unsigned int color_format;
	unsigned int max_bin;
	unsigned int max_channel;

	/* Msync2.0 */
	unsigned int msync_level_num;
};

enum MTK_CRTC_ABILITY {
	ABILITY_FBDC = BIT(0),
	ABILITY_EXT_LAYER = BIT(1),
	ABILITY_IDLEMGR = BIT(2),
	ABILITY_ESD_CHECK = BIT(3),
	ABILITY_PQ = BIT(4),
	ABILITY_RSZ = BIT(5),
	ABILITY_MML = BIT(6),
	ABILITY_WCG = BIT(7),
	ABILITY_PRE_TE = BIT(8),
	ABILITY_BW_MONITOR = BIT(9),
	ABILITY_CWB = BIT(10),
	ABILITY_MSYNC20 = BIT(11),
	ABILITY_DUAL_DISCRETE = BIT(12),
	ABILITY_PARTIAL_UPDATE = BIT(13),
	ABILITY_PARTIAL_UPDATE_BISO = BIT(14),
	ABILITY_STASH_CMD = BIT(15),
};

struct mtk_drm_wb_caps {
	_Bool support;
	_Bool rsz;
	_Bool rsz_crop;
	unsigned int rsz_out_min_w;
	unsigned int rsz_out_min_h;
	unsigned int rsz_out_max_w;
	unsigned int rsz_out_max_h;
};

struct mtk_drm_conn_caps {
	unsigned int lcm_degree;
	int lcm_color_mode;
};

#define MAX_MODES 30

struct mtk_drm_connector_caps {
	struct mtk_drm_conn_caps conn_caps;
	unsigned int width_after_pq[MAX_MODES];
	unsigned int height_after_pq[MAX_MODES];
};

enum SWITCH_MODE_DELAY {
	DELAY_INVAL = 0,
	DELAY_0,
	DELAY_1,
	DELAY_2,
	DELAY_3,
	DELAY_4,
	DELAY_5,
	DELAY_6,
	DELAY_7,
	DELAY_8,
	DELAY_9,
	DELAY_10,
};

struct mtk_drm_mode_ext_info {
	unsigned int crtc_id;
	unsigned int mode_num;
	unsigned int *total_offset;
	unsigned int *real_te_duration;
	enum SWITCH_MODE_DELAY **switch_mode_delay;
	unsigned int idle_fps;
};

struct mtk_drm_crtc_caps {
	struct mtk_drm_wb_caps wb_caps[MTK_DRM_DUMP_POINT_NUM];
	unsigned int crtc_ability;
	unsigned int ovl_csc_bit_number;
	unsigned int rpo_support_num;
};

#define MTK_PANEL_NUM 5

struct drm_mtk_session_info {
	unsigned int session_id;
	unsigned int vsyncFPS;
	unsigned int physicalWidthUm;
	unsigned int physicalHeightUm;
	unsigned int physical_width;
	unsigned int physical_height;
	unsigned int crop_width[MTK_PANEL_NUM];
	unsigned int crop_height[MTK_PANEL_NUM];
	int rotate;
};

enum drm_disp_ccorr_id_t {
	DRM_DISP_CCORR0 = 0,
	DRM_DISP_CCORR1,
	DRM_DISP_CCORR_TOTAL
};

struct DRM_DISP_CCORR_COEF_T {
	union {
		enum drm_disp_ccorr_id_t hw_id;
		enum drm_disp_ccorr_linear_t linear;
	};
	unsigned int coef[3][3];
	unsigned int offset[3];
	int FinalBacklight;
	int silky_bright_flag;
};

enum drm_disp_gamma_id_t {
	DRM_DISP_GAMMA0 = 0,
	DRM_DISP_GAMMA1,
	DRM_DISP_GAMMA_TOTAL
};

#define DRM_DISP_GAMMA_LUT_SIZE 512

struct DRM_DISP_GAMMA_LUT_T {
	enum drm_disp_gamma_id_t hw_id;
	unsigned int lut[DRM_DISP_GAMMA_LUT_SIZE];
};

struct DRM_DISP_READ_REG {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct DRM_DISP_WRITE_REG {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct DISP_TDSHP_REG {
	/* GROUP : MDP_TDSHP_00 (0x0000) */
	__u32 tdshp_softcoring_gain;         //[MSB:LSB] = [07:00]
	__u32 tdshp_gain_high;               //[MSB:LSB] = [15:08]
	__u32 tdshp_gain_mid;                //[MSB:LSB] = [23:16]
	__u32 tdshp_ink_sel;                 //[MSB:LSB] = [26:24]
	__u32 tdshp_adap_luma_bp;            //[MSB:LSB] = [28:28]
	__u32 tdshp_bypass_high;             //[MSB:LSB] = [29:29]
	__u32 tdshp_bypass_mid;              //[MSB:LSB] = [30:30]
	__u32 tdshp_en;                      //[MSB:LSB] = [31:31]
	/* GROUP : MDP_TDSHP_01 (0x0004) */
	__u32 tdshp_limit_ratio;             //[MSB:LSB] = [03:00]
	__u32 tdshp_gain;                    //[MSB:LSB] = [11:04]
	__u32 tdshp_coring_zero;             //[MSB:LSB] = [23:16]
	__u32 tdshp_coring_thr;              //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_02 (0x0008) */
	__u32 tdshp_coring_value;            //[MSB:LSB] = [15:08]
	__u32 tdshp_bound;                   //[MSB:LSB] = [23:16]
	__u32 tdshp_limit;                   //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_03 (0x000c) */
	__u32 tdshp_sat_proc;                //[MSB:LSB] = [05:00]
	__u32 tdshp_ac_lpf_coe;              //[MSB:LSB] = [11:08]
	__u32 tdshp_clip_thr;                //[MSB:LSB] = [23:16]
	__u32 tdshp_clip_ratio;              //[MSB:LSB] = [28:24]
	__u32 tdshp_clip_en;                 //[MSB:LSB] = [31:31]
	/* GROUP : MDP_TDSHP_05 (0x0014) */
	__u32 tdshp_ylev_p048;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p032;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p016;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p000;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_06 (0x0018) */
	__u32 tdshp_ylev_p112;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p096;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p080;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p064;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_07 (0x001c) */
	__u32 tdshp_ylev_p176;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p160;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p144;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p128;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_08 (0x0020) */
	__u32 tdshp_ylev_p240;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p224;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p208;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p192;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_09 (0x0024) */
	__u32 tdshp_ylev_en;                 //[MSB:LSB] = [14:14]
	__u32 tdshp_ylev_alpha;              //[MSB:LSB] = [21:16]
	__u32 tdshp_ylev_256;                //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_00 (0x0040) */
	__u32 pbc1_radius_r;                 //[MSB:LSB] = [05:00]
	__u32 pbc1_theta_r;                  //[MSB:LSB] = [11:06]
	__u32 pbc1_rslope_1;                 //[MSB:LSB] = [21:12]
	__u32 pbc1_gain;                     //[MSB:LSB] = [29:22]
	__u32 pbc1_lpf_en;                   //[MSB:LSB] = [30:30]
	__u32 pbc1_en;                       //[MSB:LSB] = [31:31]
	/* GROUP : MDP_PBC_01 (0x0044) */
	__u32 pbc1_lpf_gain;                 //[MSB:LSB] = [05:00]
	__u32 pbc1_tslope;                   //[MSB:LSB] = [15:06]
	__u32 pbc1_radius_c;                 //[MSB:LSB] = [23:16]
	__u32 pbc1_theta_c;                  //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_02 (0x0048) */
	__u32 pbc1_edge_slope;               //[MSB:LSB] = [05:00]
	__u32 pbc1_edge_thr;                 //[MSB:LSB] = [13:08]
	__u32 pbc1_edge_en;                  //[MSB:LSB] = [14:14]
	__u32 pbc1_conf_gain;                //[MSB:LSB] = [19:16]
	__u32 pbc1_rslope;                   //[MSB:LSB] = [31:22]
	/* GROUP : MDP_PBC_03 (0x004c) */
	__u32 pbc2_radius_r;                 //[MSB:LSB] = [05:00]
	__u32 pbc2_theta_r;                  //[MSB:LSB] = [11:06]
	__u32 pbc2_rslope_1;                 //[MSB:LSB] = [21:12]
	__u32 pbc2_gain;                     //[MSB:LSB] = [29:22]
	__u32 pbc2_lpf_en;                   //[MSB:LSB] = [30:30]
	__u32 pbc2_en;                       //[MSB:LSB] = [31:31]
	/* GROUP : MDP_PBC_04 (0x0050) */
	__u32 pbc2_lpf_gain;                 //[MSB:LSB] = [05:00]
	__u32 pbc2_tslope;                   //[MSB:LSB] = [15:06]
	__u32 pbc2_radius_c;                 //[MSB:LSB] = [23:16]
	__u32 pbc2_theta_c;                  //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_05 (0x0054) */
	__u32 pbc2_edge_slope;               //[MSB:LSB] = [05:00]
	__u32 pbc2_edge_thr;                 //[MSB:LSB] = [13:08]
	__u32 pbc2_edge_en;                  //[MSB:LSB] = [14:14]
	__u32 pbc2_conf_gain;                //[MSB:LSB] = [19:16]
	__u32 pbc2_rslope;                   //[MSB:LSB] = [31:22]
	/* GROUP : MDP_PBC_06 (0x0058) */
	__u32 pbc3_radius_r;                 //[MSB:LSB] = [05:00]
	__u32 pbc3_theta_r;                  //[MSB:LSB] = [11:06]
	__u32 pbc3_rslope_1;                 //[MSB:LSB] = [21:12]
	__u32 pbc3_gain;                     //[MSB:LSB] = [29:22]
	__u32 pbc3_lpf_en;                   //[MSB:LSB] = [30:30]
	__u32 pbc3_en;                       //[MSB:LSB] = [31:31]
	/* GROUP : MDP_PBC_07 (0x005c) */
	__u32 pbc3_lpf_gain;                 //[MSB:LSB] = [05:00]
	__u32 pbc3_tslope;                   //[MSB:LSB] = [15:06]
	__u32 pbc3_radius_c;                 //[MSB:LSB] = [23:16]
	__u32 pbc3_theta_c;                  //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_08 (0x0060) */
	__u32 pbc3_edge_slope;               //[MSB:LSB] = [05:00]
	__u32 pbc3_edge_thr;                 //[MSB:LSB] = [13:08]
	__u32 pbc3_edge_en;                  //[MSB:LSB] = [14:14]
	__u32 pbc3_conf_gain;                //[MSB:LSB] = [19:16]
	__u32 pbc3_rslope;                   //[MSB:LSB] = [31:22]
	/* GROUP : MDP_TDSHP_10 (0x0320) */
	__u32  tdshp_mid_softlimit_ratio;    //[MSB:LSB] = [03:00]
	__u32  tdshp_mid_coring_zero;        //[MSB:LSB] = [23:16]
	__u32  tdshp_mid_coring_thr;         //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_11 (0x0324) */
	__u32  tdshp_mid_softcoring_gain;    //[MSB:LSB] = [07:00]
	__u32  tdshp_mid_coring_value;       //[MSB:LSB] = [15:08]
	__u32  tdshp_mid_bound;              //[MSB:LSB] = [13:16]
	__u32  tdshp_mid_limit;              //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_12 (0x0328) */
	__u32  tdshp_high_softlimit_ratio;   //[MSB:LSB] = [03:00]
	__u32  tdshp_high_coring_zero;       //[MSB:LSB] = [23:16]
	__u32  tdshp_high_coring_thr;        //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_13 (0x032c) */
	__u32  tdshp_high_softcoring_gain;   //[MSB:LSB] = [07:00]
	__u32  tdshp_high_coring_value;      //[MSB:LSB] = [15:08]
	__u32  tdshp_high_bound;             //[MSB:LSB] = [13:16]
	__u32  tdshp_high_limit;             //[MSB:LSB] = [31:24]
	/* GROUP : MDP_EDF_GAIN_00 (0x0300) */
	__u32  edf_clip_ratio_inc;           //[MSB:LSB] = [02:00]
	__u32  edf_edge_gain;                //[MSB:LSB] = [12:08]
	__u32  edf_detail_gain;              //[MSB:LSB] = [23:16]
	__u32  edf_flat_gain;                //[MSB:LSB] = [28:24]
	__u32  edf_gain_en;                  //[MSB:LSB] = [31:31]
	/* GROUP : MDP_EDF_GAIN_01 (0x0304) */
	__u32  edf_edge_th;                  //[MSB:LSB] = [08:00]
	__u32  edf_detail_fall_th;           //[MSB:LSB] = [17:09]
	__u32  edf_detail_rise_th;           //[MSB:LSB] = [23:18]
	__u32  edf_flat_th;                  //[MSB:LSB] = [30:25]
	/* GROUP : MDP_EDF_GAIN_02 (0x0308) */
	__u32  edf_edge_slope;               //[MSB:LSB] = [04:00]
	__u32  edf_detail_fall_slope;        //[MSB:LSB] = [12:08]
	__u32  edf_detail_rise_slope;        //[MSB:LSB] = [18:16]
	__u32  edf_flat_slope;               //[MSB:LSB] = [26:24]
	/* GROUP : MDP_EDF_GAIN_03 (0x030c) */
	__u32  edf_edge_mono_slope;          //[MSB:LSB] = [03:00]
	__u32  edf_edge_mono_th;             //[MSB:LSB] = [14:08]
	__u32  edf_edge_mag_slope;           //[MSB:LSB] = [20:16]
	__u32  edf_edge_mag_th;              //[MSB:LSB] = [28:24]
	/* GROUP : MDP_EDF_GAIN_05 (0x0314) */
	__u32  edf_bld_wgt_mag;              //[MSB:LSB] = [07:00]
	__u32  edf_bld_wgt_mono;             //[MSB:LSB] = [15:08]
	/* GROUP : MDP_C_BOOST_MAIN (0x00e0) */
	__u32  tdshp_cboost_gain;            //[MSB:LSB] = [07:00]
	__u32  tdshp_cboost_en;              //[MSB:LSB] = [13:13]
	__u32  tdshp_cboost_lmt_l;           //[MSB:LSB] = [23:16]
	__u32  tdshp_cboost_lmt_u;           //[MSB:LSB] = [31:24]
	/* GROUP : MDP_C_BOOST_MAIN_2 (0x00e4) */
	__u32  tdshp_cboost_yoffset;         //[MSB:LSB] = [06:00]
	__u32  tdshp_cboost_yoffset_sel;     //[MSB:LSB] = [17:16]
	__u32  tdshp_cboost_yconst;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_00 (0x0480) */
	__u32 tdshp_post_ylev_p048;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p032;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p016;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p000;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_01 (0x0484) */
	__u32 tdshp_post_ylev_p112;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p096;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p080;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p064;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_02 (0x0488) */
	__u32 tdshp_post_ylev_p176;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p160;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p144;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p128;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_03 (0x048c) */
	__u32 tdshp_post_ylev_p240;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p224;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p208;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p192;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_04 (0x0490) */
	__u32 tdshp_post_ylev_en;            //[MSB:LSB] = [14:14]
	__u32 tdshp_post_ylev_alpha;         //[MSB:LSB] = [21:16]
	__u32 tdshp_post_ylev_256;           //[MSB:LSB] = [31:24]
};

struct DISP_TDSHP_DISPLAY_SIZE {
	int width;
	int height;
	int lcm_width;
	int lcm_height;
};

struct DISP_MDP_AAL_CLARITY_REG {
	/* GROUP : MDP_AAL_CFG_MAIN */
	__u32 aal_round_9bit_out_en;                                   //[MSB:LSB] = [01:01]
	__u32 dre_max_diff_mode;                                       //[MSB:LSB] = [02:02]
	__u32 dre_max_hist_mode;                                       //[MSB:LSB] = [03:03]
	__u32 dre_alg_mode;                                            //[MSB:LSB] = [04:04]
	__u32 dre_output_mode;                                         //[MSB:LSB] = [05:05]
	__u32 aal_tile_mode_en;                                        //[MSB:LSB] = [06:06]

	/* GROUP : MDP_AAL_DRE_BILATERAL */
	__u32 have_bilateral_filter;                                   //[MSB:LSB] = [00:00]
	__u32 bilateral_flt_en;                                        //[MSB:LSB] = [01:01]
	__u32 bilateral_range_flt_slope;                               //[MSB:LSB] = [07:04]
	__u32 dre_bilateral_detect_en;                                 //[MSB:LSB] = [08:08]
	__u32 bilateral_impulse_noise_en;                              //[MSB:LSB] = [09:09]

	/* GROUP : MDP_AAL_DRE_BILATERAL_Blending_00 */
	__u32 dre_bilateral_blending_en;                               //[MSB:LSB] = [00:00]
	__u32 dre_bilateral_blending_wgt;                              //[MSB:LSB] = [08:04]
	__u32 dre_bilateral_blending_wgt_mode;                         //[MSB:LSB] = [10:09]
	__u32 dre_bilateral_activate_blending_wgt_gain;                //[MSB:LSB] = [14:11]
	__u32 dre_bilateral_activate_blending_A;                       //[MSB:LSB] = [18:15]
	__u32 dre_bilateral_activate_blending_B;                       //[MSB:LSB] = [22:19]
	__u32 dre_bilateral_activate_blending_C;                       //[MSB:LSB] = [26:23]
	__u32 dre_bilateral_activate_blending_D;                       //[MSB:LSB] = [30:27]

	/* GROUP : MDP_AAL_DRE_BILATERAL_Blending_01 */
	__u32 dre_bilateral_size_blending_wgt;                         //[MSB:LSB] = [03:00]

	/* GROUP : MDP_AAL_DRE_BILATERAL_CUST_FLT1_00 */
	__u32 bilateral_custom_range_flt1_0_0;                         //[MSB:LSB] = [05:00]
	__u32 bilateral_custom_range_flt1_0_1;                         //[MSB:LSB] = [11:06]
	__u32 bilateral_custom_range_flt1_0_2;                         //[MSB:LSB] = [17:12]
	__u32 bilateral_custom_range_flt1_0_3;                         //[MSB:LSB] = [23:18]
	__u32 bilateral_custom_range_flt1_0_4;                         //[MSB:LSB] = [29:24]

	/* GROUP : MDP_AAL_DRE_BILATERAL_CUST_FLT1_01 */
	__u32 bilateral_custom_range_flt1_1_0;                         //[MSB:LSB] = [05:00]
	__u32 bilateral_custom_range_flt1_1_1;                         //[MSB:LSB] = [11:06]
	__u32 bilateral_custom_range_flt1_1_2;                         //[MSB:LSB] = [17:12]
	__u32 bilateral_custom_range_flt1_1_3;                         //[MSB:LSB] = [23:18]
	__u32 bilateral_custom_range_flt1_1_4;                         //[MSB:LSB] = [29:24]

	/* GROUP : MDP_AAL_DRE_BILATERAL_CUST_FLT1_02 */
	__u32 bilateral_custom_range_flt1_2_0;                         //[MSB:LSB] = [05:00]
	__u32 bilateral_custom_range_flt1_2_1;                         //[MSB:LSB] = [11:06]
	__u32 bilateral_custom_range_flt1_2_2;                         //[MSB:LSB] = [17:12]
	__u32 bilateral_custom_range_flt1_2_3;                         //[MSB:LSB] = [23:18]
	__u32 bilateral_custom_range_flt1_2_4;                         //[MSB:LSB] = [29:24]

	/* GROUP : MDP_AAL_DRE_BILATERAL_CUST_FLT2_00 */
	__u32 bilateral_custom_range_flt2_0_0;                         //[MSB:LSB] = [05:00]
	__u32 bilateral_custom_range_flt2_0_1;                         //[MSB:LSB] = [11:06]
	__u32 bilateral_custom_range_flt2_0_2;                         //[MSB:LSB] = [17:12]
	__u32 bilateral_custom_range_flt2_0_3;                         //[MSB:LSB] = [23:18]
	__u32 bilateral_custom_range_flt2_0_4;                         //[MSB:LSB] = [29:24]

	/* GROUP : MDP_AAL_DRE_BILATERAL_CUST_FLT2_01 */
	__u32 bilateral_custom_range_flt2_1_0;                         //[MSB:LSB] = [05:00]
	__u32 bilateral_custom_range_flt2_1_1;                         //[MSB:LSB] = [11:06]
	__u32 bilateral_custom_range_flt2_1_2;                         //[MSB:LSB] = [17:12]
	__u32 bilateral_custom_range_flt2_1_3;                         //[MSB:LSB] = [23:18]
	__u32 bilateral_custom_range_flt2_1_4;                         //[MSB:LSB] = [29:24]

	/* GROUP : MDP_AAL_DRE_BILATERAL_CUST_FLT2_02 */
	__u32 bilateral_custom_range_flt2_2_0;                         //[MSB:LSB] = [05:00]
	__u32 bilateral_custom_range_flt2_2_1;                         //[MSB:LSB] = [11:06]
	__u32 bilateral_custom_range_flt2_2_2;                         //[MSB:LSB] = [17:12]
	__u32 bilateral_custom_range_flt2_2_3;                         //[MSB:LSB] = [23:18]
	__u32 bilateral_custom_range_flt2_2_4;                         //[MSB:LSB] = [29:24]

	/* GROUP : MDP_AAL_DRE_BILATERAL_FLT_CONFIG */
	__u32 bilateral_range_flt_gain;                                //[MSB:LSB] = [02:00]
	__u32 bilateral_custom_range_flt_gain;                         //[MSB:LSB] = [05:03]
	__u32 bilateral_custom_range_flt_slope;                        //[MSB:LSB] = [09:06]
	__u32 bilateral_contrary_blending_wgt;                         //[MSB:LSB] = [11:10]
	__u32 bilateral_size_blending_wgt;                             //[MSB:LSB] = [18:12]

	/* GROUP : MDP_AAL_DRE_BILATERAL_FREQ_BLENDING */
	__u32 bilateral_contrary_blending_out_wgt;                     //[MSB:LSB] = [04:00]
	__u32 bilateral_size_blending_out_wgt;                         //[MSB:LSB] = [09:05]
	__u32 bilateral_range_flt_out_wgt;                             //[MSB:LSB] = [14:10]
	__u32 bilateral_custom_range_flt1_out_wgt;                     //[MSB:LSB] = [19:15]
	__u32 bilateral_custom_range_flt2_out_wgt;                     //[MSB:LSB] = [24:20]

	/* GROUP : MDP_AAL_DRE_BILATERAL_REGION_PROTECTION */
	__u32 dre_bilateral_blending_region_protection_en;             //[MSB:LSB] = [00:00]
	__u32 dre_bilateral_region_protection_activate_A;              //[MSB:LSB] = [04:01]
	__u32 dre_bilateral_region_protection_activate_B;              //[MSB:LSB] = [12:05]
	__u32 dre_bilateral_region_protection_activate_C;              //[MSB:LSB] = [20:13]
	__u32 dre_bilateral_region_protection_activate_D;              //[MSB:LSB] = [24:21]
	__u32 dre_bilateral_region_protection_input_shift_bit;         //[MSB:LSB] = [27:25]

};

struct DISP_TDSHP_CLARITY_REG {
	// High & Mid Gain
	__u32 tdshp_gain_high;
	__u32 tdshp_gain_mid;

	// Mid-Band Vertical Filter
	__u32 mid_coef_v_custom_range_flt_0_0;
	__u32 mid_coef_v_custom_range_flt_0_1;
	__u32 mid_coef_v_custom_range_flt_0_2;
	__u32 mid_coef_v_custom_range_flt_0_3;
	__u32 mid_coef_v_custom_range_flt_0_4;

	__u32 mid_coef_v_custom_range_flt_1_0;
	__u32 mid_coef_v_custom_range_flt_1_1;
	__u32 mid_coef_v_custom_range_flt_1_2;
	__u32 mid_coef_v_custom_range_flt_1_3;
	__u32 mid_coef_v_custom_range_flt_1_4;

	__u32 mid_coef_v_custom_range_flt_2_0;
	__u32 mid_coef_v_custom_range_flt_2_1;
	__u32 mid_coef_v_custom_range_flt_2_2;
	__u32 mid_coef_v_custom_range_flt_2_3;
	__u32 mid_coef_v_custom_range_flt_2_4;

	// Mid-Band Horizontal Filter
	__u32 mid_coef_h_custom_range_flt_0_0;
	__u32 mid_coef_h_custom_range_flt_0_1;
	__u32 mid_coef_h_custom_range_flt_0_2;
	__u32 mid_coef_h_custom_range_flt_0_3;
	__u32 mid_coef_h_custom_range_flt_0_4;

	__u32 mid_coef_h_custom_range_flt_1_0;
	__u32 mid_coef_h_custom_range_flt_1_1;
	__u32 mid_coef_h_custom_range_flt_1_2;
	__u32 mid_coef_h_custom_range_flt_1_3;
	__u32 mid_coef_h_custom_range_flt_1_4;

	__u32 mid_coef_h_custom_range_flt_2_0;
	__u32 mid_coef_h_custom_range_flt_2_1;
	__u32 mid_coef_h_custom_range_flt_2_2;
	__u32 mid_coef_h_custom_range_flt_2_3;
	__u32 mid_coef_h_custom_range_flt_2_4;

	// High-Band Vertical Filter
	__u32 high_coef_v_custom_range_flt_0_0;
	__u32 high_coef_v_custom_range_flt_0_1;
	__u32 high_coef_v_custom_range_flt_0_2;
	__u32 high_coef_v_custom_range_flt_0_3;
	__u32 high_coef_v_custom_range_flt_0_4;

	__u32 high_coef_v_custom_range_flt_1_0;
	__u32 high_coef_v_custom_range_flt_1_1;
	__u32 high_coef_v_custom_range_flt_1_2;
	__u32 high_coef_v_custom_range_flt_1_3;
	__u32 high_coef_v_custom_range_flt_1_4;

	__u32 high_coef_v_custom_range_flt_2_0;
	__u32 high_coef_v_custom_range_flt_2_1;
	__u32 high_coef_v_custom_range_flt_2_2;
	__u32 high_coef_v_custom_range_flt_2_3;
	__u32 high_coef_v_custom_range_flt_2_4;

	// High-Band Horizontal Filter
	__u32 high_coef_h_custom_range_flt_0_0;
	__u32 high_coef_h_custom_range_flt_0_1;
	__u32 high_coef_h_custom_range_flt_0_2;
	__u32 high_coef_h_custom_range_flt_0_3;
	__u32 high_coef_h_custom_range_flt_0_4;

	__u32 high_coef_h_custom_range_flt_1_0;
	__u32 high_coef_h_custom_range_flt_1_1;
	__u32 high_coef_h_custom_range_flt_1_2;
	__u32 high_coef_h_custom_range_flt_1_3;
	__u32 high_coef_h_custom_range_flt_1_4;

	__u32 high_coef_h_custom_range_flt_2_0;
	__u32 high_coef_h_custom_range_flt_2_1;
	__u32 high_coef_h_custom_range_flt_2_2;
	__u32 high_coef_h_custom_range_flt_2_3;
	__u32 high_coef_h_custom_range_flt_2_4;

	// High-Band Right-Diagonal Filter
	__u32 high_coef_rd_custom_range_flt_0_0;
	__u32 high_coef_rd_custom_range_flt_0_1;
	__u32 high_coef_rd_custom_range_flt_0_2;
	__u32 high_coef_rd_custom_range_flt_0_3;
	__u32 high_coef_rd_custom_range_flt_0_4;

	__u32 high_coef_rd_custom_range_flt_1_0;
	__u32 high_coef_rd_custom_range_flt_1_1;
	__u32 high_coef_rd_custom_range_flt_1_2;
	__u32 high_coef_rd_custom_range_flt_1_3;
	__u32 high_coef_rd_custom_range_flt_1_4;

	__u32 high_coef_rd_custom_range_flt_2_0;
	__u32 high_coef_rd_custom_range_flt_2_1;
	__u32 high_coef_rd_custom_range_flt_2_2;
	__u32 high_coef_rd_custom_range_flt_2_3;
	__u32 high_coef_rd_custom_range_flt_2_4;

	// High-Band Left-Diagonal Filter
	__u32 high_coef_ld_custom_range_flt_0_0;
	__u32 high_coef_ld_custom_range_flt_0_1;
	__u32 high_coef_ld_custom_range_flt_0_2;
	__u32 high_coef_ld_custom_range_flt_0_3;
	__u32 high_coef_ld_custom_range_flt_0_4;

	__u32 high_coef_ld_custom_range_flt_1_0;
	__u32 high_coef_ld_custom_range_flt_1_1;
	__u32 high_coef_ld_custom_range_flt_1_2;
	__u32 high_coef_ld_custom_range_flt_1_3;
	__u32 high_coef_ld_custom_range_flt_1_4;

	__u32 high_coef_ld_custom_range_flt_2_0;
	__u32 high_coef_ld_custom_range_flt_2_1;
	__u32 high_coef_ld_custom_range_flt_2_2;
	__u32 high_coef_ld_custom_range_flt_2_3;
	__u32 high_coef_ld_custom_range_flt_2_4;

	// Active Parameters
	__u32 mid_negative_offset;
	__u32 mid_positive_offset;
	__u32 high_negative_offset;
	__u32 high_positive_offset;

	// Active Parameters Frequency D
	__u32 D_active_parameter_N_gain;
	__u32 D_active_parameter_N_offset;
	__u32 D_active_parameter_P_offset ;
	__u32 D_active_parameter_P_gain;

	// Active Parameters Frequency H
	__u32 High_active_parameter_N_gain;
	__u32 High_active_parameter_N_offset;
	__u32 High_active_parameter_P_offset;
	__u32 High_active_parameter_P_gain;

	// Active Parameters Frequency L
	__u32 L_active_parameter_N_gain;
	__u32 L_active_parameter_N_offset;
	__u32 L_active_parameter_P_offset;
	__u32 L_active_parameter_P_gain;

	// Active Parameters Frequency M
	__u32 Mid_active_parameter_N_gain;
	__u32 Mid_active_parameter_N_offset;
	__u32 Mid_active_parameter_P_offset;
	__u32 Mid_active_parameter_P_gain;

	// Size Parameters
	__u32 SIZE_PARA_SMALL_MEDIUM;
	__u32 SIZE_PARA_MEDIUM_BIG;
	__u32 SIZE_PARA_BIG_HUGE;

	// Final Size Adaptive Weight Huge
	__u32 Mid_size_adaptive_weight_HUGE;
	__u32 Mid_auto_adaptive_weight_HUGE;
	__u32 high_size_adaptive_weight_HUGE;
	__u32 high_auto_adaptive_weight_HUGE;

	// Final Size Adaptive Weight Big
	__u32 Mid_size_adaptive_weight_BIG;
	__u32 Mid_auto_adaptive_weight_BIG;
	__u32 high_size_adaptive_weight_BIG;
	__u32 high_auto_adaptive_weight_BIG;

	// Final Size Adaptive Weight Medium
	__u32 Mid_size_adaptive_weight_MEDIUM;
	__u32 Mid_auto_adaptive_weight_MEDIUM;
	__u32 high_size_adaptive_weight_MEDIUM;
	__u32 high_auto_adaptive_weight_MEDIUM;

	// Final Size Adaptive Weight Small
	__u32 Mid_size_adaptive_weight_SMALL;
	__u32 Mid_auto_adaptive_weight_SMALL;
	__u32 high_size_adaptive_weight_SMALL;
	__u32 high_auto_adaptive_weight_SMALL;

	// Config
	__u32 FREQ_EXTRACT_ENHANCE;
	__u32 FILTER_HIST_EN;

	// Frequency Weighting
	__u32 freq_M_weighting;
	__u32 freq_H_weighting;
	__u32 freq_D_weighting;
	__u32 freq_L_weighting;

	// Frequency Weighting Final
	__u32 freq_M_final_weighting;
	__u32 freq_D_final_weighting;
	__u32 freq_L_final_weighting;
	__u32 freq_WH_final_weighting;


	// Luma Chroma Parameters
	__u32 Luma_adaptive_mode;
	__u32 Chroma_adaptive_mode;

	__u32 SIZE_PARAMETER;
	__u32 SEG_data_length;
	__u32 Luma_shift;
	__u32 Chroma_shift;
	__u32 Regional_en;

	/* GROUP : LUMA_CHROMA_PARAMETER */
	__u32 luma_low_gain;
	__u32 luma_low_index;
	__u32 luma_high_index;
	__u32 luma_high_gain;
	__u32 chroma_low_gain;
	__u32 chroma_low_index;
	__u32 chroma_high_index;
	__u32 chroma_high_gain;




	// Base Black & White edges
	__u32 class_0_positive_gain;
	__u32 class_0_negative_gain;
	__u32 class_1_positive_gain;
	__u32 class_1_negative_gain;
	__u32 class_2_positive_gain;
	__u32 class_2_negative_gain;
};

struct DISP_CLARITY_REG {
	struct DISP_MDP_AAL_CLARITY_REG mdp_aal_clarity_regs;
	struct DISP_TDSHP_CLARITY_REG disp_tdshp_clarity_regs;
};

struct drm_mtk_channel_hist {
	unsigned int channel_id;
	enum MTK_DRM_CHIST_COLOR_FORMT color_format;
	unsigned int hist[256];
	unsigned int bin_count;
};

struct drm_mtk_chist_info {
	unsigned int present_fence;
	unsigned int device_id;
	enum MTK_DRM_CHIST_CALLER caller;
	unsigned int get_channel_count;
	struct drm_mtk_channel_hist channel_hist[MTK_DRM_DISP_CHIST_CHANNEL_COUNT];
	unsigned int lcm_width;
	unsigned int lcm_height;
};

struct drm_mtk_channel_config {
	_Bool enabled;
	enum MTK_DRM_CHIST_COLOR_FORMT color_format;
	unsigned int bin_count;
	unsigned int channel_id;
	unsigned int blk_width;
	unsigned int blk_height;
	unsigned int roi_start_x;
	unsigned int roi_start_y;
	unsigned int roi_end_x;
	unsigned int roi_end_y;
};

struct drm_mtk_chist_caps {
	unsigned int device_id;
	unsigned int support_color;
	unsigned int lcm_width;
	unsigned int lcm_height;
	struct drm_mtk_channel_config chist_config[MTK_DRM_DISP_CHIST_CHANNEL_COUNT];
};

struct drm_mtk_chist_config {
	unsigned int device_id;
	unsigned int lcm_color_mode;
	unsigned int config_channel_count;
	enum MTK_DRM_CHIST_CALLER caller;
	struct drm_mtk_channel_config chist_config[MTK_DRM_DISP_CHIST_CHANNEL_COUNT];
};


struct drm_mtk_ccorr_caps {
	unsigned int ccorr_bit;
	unsigned int ccorr_number;
	unsigned int ccorr_linear;//1st byte:high 4 bit:CCORR1,low 4 bit:CCORR0
};

struct mtk_drm_pq_caps_info {
	struct drm_mtk_ccorr_caps ccorr_caps;
};

enum MTK_DRM_ODDMR_CTL_CMD {
	MTK_DRM_ODDMR_OD_INIT = 0,
	MTK_DRM_ODDMR_OD_ENABLE = 1,
	MTK_DRM_ODDMR_OD_DISABLE = 2,
	MTK_DRM_ODDMR_DMR_INIT = 3,
	MTK_DRM_ODDMR_DMR_ENABLE = 4,
	MTK_DRM_ODDMR_DMR_DISABLE = 5,
	MTK_DRM_ODDMR_LOAD_PARAM = 6,
	MTK_DRM_ODDMR_OD_READ_SW_REG = 7,
	MTK_DRM_ODDMR_OD_WRITE_SW_REG = 8,
	MTK_DRM_ODDMR_OD_USER_GAIN = 9,
	MTK_DRM_ODDMR_REMAP_ENABLE = 10,
	MTK_DRM_ODDMR_REMAP_DISABLE = 11,
	MTK_DRM_ODDMR_REMAP_TARGET = 12,
	MTK_DRM_ODDMR_DBI_NORMAL_COUNTING_CTL = 13,
	MTK_DRM_ODDMR_DBI_IRDROP_ENABLE = 14,
	MTK_DRM_ODDMR_DBI_SET_GAIN_RATIO = 15,
	MTK_DRM_ODDMR_DMR_BINSET_INIT = 16,
	MTK_DRM_ODDMR_DMR_BINSET_CHG = 17,
	MTK_DRM_ODDMR_REG_TUNING_INIT = 19,
	MTK_DRM_ODDMR_REG_TUNING_ENABLE = 20,
};

struct mtk_drm_oddmr_ctl {
	enum MTK_DRM_ODDMR_CTL_CMD cmd;
	__u32 size;
	__u8 *data;
};

struct mtk_drm_oddmr_param {
	__u32 head_id;
	__u32 size;
	__u8 *data;
	__u32 checksum;
};

struct mtk_drm_pq_config_ctl {
	__u32 crtc_id;
	__u8 check_trigger;
	__u8 len;
	void *data;
};

struct mtk_drm_pq_param {
	__u32 cmd;
	__u32 size;
	void *data;
};

struct mtk_drm_pq_proxy_ctl {
	__u32 crtc_id;
	__u32 cmd;
	__u32 size;
	__u32 extra_size;
	void *data;
	void *extra_data;
};

enum mtk_pq_module_type {
	MTK_DISP_PQ_COLOR,
	MTK_DISP_PQ_DITHER,
	MTK_DISP_PQ_CCORR,
	MTK_DISP_PQ_AAL,
	MTK_DISP_PQ_GAMMA,
	MTK_DISP_PQ_CHIST,
	MTK_DISP_PQ_C3D,
	MTK_DISP_PQ_TDSHP,
	MTK_DISP_PQ_DMR,
	MTK_DISP_PQ_DBI,
	MTK_DISP_VIRTUAL_TYPE,
	MTK_DISP_PQ_TYPE_MAX = 65535,
};

enum mtk_pq_frame_cfg_cmd {
	PQ_CMD_INVALID,
	PQ_AAL_EVENTCTL,
	PQ_AAL_INIT_REG,
	PQ_AAL_SET_ESS20_SPECT_PARAM,
	PQ_AAL_SET_PARAM,
	PQ_AAL_INIT_DRE30,
	PQ_AAL_CLARITY_SET_REG,
	PQ_GAMMA_SET_GAMMALUT = 100,
	PQ_GAMMA_SET_12BIT_GAMMALUT,
	PQ_GAMMA_BYPASS_GAMMA,
	PQ_GAMMA_DISABLE_MUL_EN,
	PQ_CHIST_CONFIG = 200,
	PQ_COLOR_MUTEX_CONTROL = 300,
	PQ_COLOR_BYPASS,
	PQ_COLOR_SET_PQINDEX,
	PQ_COLOR_SET_PQPARAM,
	PQ_COLOR_WRITE_REG,
	PQ_COLOR_WRITE_SW_REG,
	PQ_COLOR_SET_COLOR_REG,
	PQ_COLOR_SET_WINDOW,
	PQ_COLOR_DRECOLOR_SET_PARAM,
	PQ_CCORR_EVENTCTL = 400,
	PQ_CCORR_SUPPORT_COLOR_MATRIX,
	PQ_CCORR_SET_CCORR,
	PQ_CCORR_AIBLD_CV_MODE,
	PQ_CCORR_SET_PQ_CAPS,
	PQ_C3D_EVENTCTL = 500,
	PQ_C3D_BYPASS,
	PQ_C3D_SET_LUT,
	PQ_TDSHP_SET_REG = 600,
	PQ_DITHER_SET_DITHER_PARAM = 700,
	PQ_DBI_LOAD_PARAM = 800,
	PQ_DBI_LOAD_TB,
	PQ_DBI_ENABLE,
	PQ_DBI_DISABLE,
	PQ_DBI_REMAP_TARGET,
	PQ_DBI_REMAP_CHG,
	PQ_DBI_REMAP_ENABLE,
	PQ_DBI_REMAP_DISABLE,
	PQ_DBI_SET_GAIN_RATIO,
	PQ_DBI_LOAD_SCP_PARAM,
	PQ_DBI_GET_SCP_LIFECYCLE,
	PQ_VIRTUAL_SET_PROPERTY = 900,
	PQ_VIRTUAL_CHECK_TRIGGER,
	PQ_VIRTUAL_RELAY_ENGINES,
	/* Get cmd begin */
	/* Notice:
	 * Command for getting must be added after the PQ_GET_CMD_START.
	 * Otherwise, the func of 'copy_to_user' will not be called.
	 */
	PQ_GET_CMD_START = 60000,
	PQ_AAL_GET_HIST,
	PQ_AAL_GET_SIZE,
	PQ_AAL_SET_TRIGGER_STATE,
	PQ_AAL_GET_BASE_VOLTAGE,
	PQ_CHIST_GET = 60100,
	PQ_COLOR_READ_REG = 60200,
	PQ_COLOR_READ_SW_REG,
	PQ_CCORR_GET_IRQ = 60300,
	PQ_CCORR_GET_PQ_CAPS,
	PQ_C3D_GET_IRQ = 60400,
	PQ_C3D_GET_IRQ_STATUS,
	PQ_C3D_GET_BIN_NUM,
	PQ_TDSHP_GET_SIZE = 60500,
	PQ_DMR_INIT = 60600,
	PQ_DMR_ENABLE,
	PQ_DMR_DISABLE,
	PQ_DBI_GET_HW_ID,
	PQ_DBI_GET_WIDTH,
	PQ_DBI_GET_HEIGHT,
	PQ_DBI_GET_DBV,
	PQ_DBI_GET_FPS,
	PQ_DBI_GET_SCP,
	PQ_DMR_BINSET_INIT,
	PQ_DMR_BINSET_CHG,
	PQ_ODDMR_REG_TUNING_INIT,
	PQ_ODDMR_REG_TUNING_ENABLE,
	PQ_VIRTUAL_GET_MASTER_INFO = 60700,
	PQ_VIRTUAL_GET_IRQ,
	PQ_VIRTUAL_WAIT_CRTC_READY,
	PQ_VIRTUAL_GET_PIXEL_TYPE_BY_FENCE,
	PQ_CMD_MAX = 65535,
};

enum PQ_FEATURE_BIT_SHIFT {
	PQ_FEATURE_DEFAULT = 0,
	PQ_FEATURE_KRN_OPT_USE_PQ,
	PQ_FEATURE_KRN_HBM,
	PQ_FEATURE_KRN_DOZE,
	PQ_FEATURE_KRN_PU,
	PQ_FEATURE_KRN_SET_COLOR_MATRIX,
	PQ_FEATURE_KRN_FEATURE6,
	PQ_FEATURE_KRN_FEATURE7,
	PQ_FEATURE_KRN_FEATURE8,
	PQ_FEATURE_KRN_FEATURE9,
	PQ_FEATURE_KRN_FEATURE10,
	PQ_FEATURE_KRN_FEATURE11,
	PQ_FEATURE_KRN_FEATURE12,
	PQ_FEATURE_KRN_FEATURE13,
	PQ_FEATURE_KRN_FEATURE14,
	PQ_FEATURE_KRN_FEATURE15,
	PQ_FEATURE_HAL_SET_DITHER_PARAM,
	PQ_FEATURE_HAL_COLOR_DETECT,
	PQ_FEATURE_HAL_PU,
	PQ_FEATURE_HAL_RELAY_DEBUG,
	PQ_FEATURE_HAL_PROPERTY_DEBUG,
	PQ_FEATURE_HAL_FEATURE_SWITCH,
	PQ_FEATURE_HAL_PQ_INIT,
	PQ_FEATURE_HAL_AAL_FUNC_CHG,
	PQ_FEATURE_HAL_FEATURE9,
	PQ_FEATURE_HAL_FEATURE10,
	PQ_FEATURE_HAL_FEATURE11,
	PQ_FEATURE_HAL_FEATURE12,
	PQ_FEATURE_HAL_FEATURE13,
	PQ_FEATURE_HAL_FEATURE14,
	PQ_FEATURE_HAL_FEATURE15,
	PQ_FEATURE_HAL_FEATURE16,
	PQ_FEATURE_FEATURE_NUM_MAX = PQ_FEATURE_HAL_FEATURE16,
};

enum mtk_pq_relay_engine {
	MTK_DISP_PQ_COLOR_RELAY = 0x1,
	MTK_DISP_PQ_CCORR_RELAY = 0x2,
	MTK_DISP_PQ_C3D_RELAY = 0x4,
	MTK_DISP_PQ_TDSHP_RELAY = 0x8,
	MTK_DISP_PQ_AAL_RELAY = 0x10,
	MTK_DISP_PQ_DMDP_AAL_RELAY = 0x20,
	MTK_DISP_PQ_GAMMA_RELAY = 0x40,
	MTK_DISP_PQ_DITHER_RELAY = 0x80,
	MTK_DISP_PQ_CHIST_RELAY = 0x100,
};

struct mtk_pq_relay_enable {
	bool enable;
	bool wait_hw_config_done;
	uint32_t relay_engines;
	enum PQ_FEATURE_BIT_SHIFT caller;
};

enum mtk_pq_aal_eventctl {
	AAL_EVENT_EN = 0x1,
	AAL_EVENT_STOP = 0x2,
	AAL_EVENT_FUNC_ON = 0x4,
	AAL_EVENT_FUNC_OFF = 0x8,
};

#define GET_PANELS_STR_LEN 64
#define MAX_CRTC_CNT 10
enum MTK_PANEL_DSI_MODE {
	MTK_PANEL_DSI_MODE_UNKNOWN = 0,
	MTK_PANEL_DSI_MODE_VDO,
	MTK_PANEL_DSI_MODE_CMD,
};

struct mtk_drm_panels_info {
	int connector_cnt;
	int default_connector_id;
	unsigned int *connector_obj_id;
	unsigned int **possible_crtc;
	char **panel_name;
	unsigned int *panel_id;
	unsigned int *dsi_mode;
};

struct DISP_PANEL_BASE_VOLTAGE {
	unsigned char AnodeBase[MAX_FPS_PN];
	unsigned char AnodeOffset[MAX_DBV_PN];
	unsigned char ELVSSBase[MAX_DBV_PN];
	_Bool flag;
	unsigned int DDICDbvPn;
	unsigned int DDICFpsPn;
};

enum MTK_PANEL_SPR_MODE {
	MTK_PANEL_RGBG_BGRG_TYPE = 0,
	MTK_PANEL_BGRG_RGBG_TYPE,
	MTK_PANEL_RGBRGB_BGRBGR_TYPE,
	MTK_PANEL_BGRBGR_RGBRGB_TYPE,
	MTK_PANEL_RGBRGB_BRGBRG_TYPE,
	MTK_PANEL_BRGBRG_RGBRGB_TYPE,
	MTK_PANEL_EXT_TYPE,
	MTK_PANEL_SPR_OFF_TYPE = 0x1000,
	MTK_PANEL_INVALID_TYPE = 0xFFFF,
};

struct mtk_pixel_type_fence {
	unsigned int fence_idx;
	unsigned int type;
	unsigned int secure;
};

struct mtk_drm_dma_buf {
	__s32 fd;
	__u64 mva;
};

#define DRM_IOCTL_MTK_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_CREATE, struct drm_mtk_gem_create)

#define DRM_IOCTL_MTK_GEM_MAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_MAP_OFFSET, struct drm_mtk_gem_map_off)

#define DRM_IOCTL_MTK_GEM_SUBMIT	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_SUBMIT, struct drm_mtk_gem_submit)

#define DRM_IOCTL_MTK_SESSION_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SESSION_CREATE, struct drm_mtk_session)

#define DRM_IOCTL_MTK_SESSION_DESTROY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SESSION_DESTROY, struct drm_mtk_session)

#define DRM_IOCTL_MTK_LAYERING_RULE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_LAYERING_RULE, struct drm_mtk_layering_info)

#define DRM_IOCTL_MTK_CRTC_GETFENCE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_CRTC_GETFENCE, struct drm_mtk_fence)

#define DRM_IOCTL_MTK_CRTC_FENCE_REL	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_CRTC_FENCE_REL, struct drm_mtk_fence)

#define DRM_IOCTL_MTK_CRTC_GETSFFENCE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_CRTC_GETSFFENCE, struct drm_mtk_fence)

#define DRM_IOCTL_MTK_MML_GEM_SUBMIT	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_MML_GEM_SUBMIT, struct mml_submit)

#define DRM_IOCTL_MTK_SET_MSYNC_PARAMS    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SET_MSYNC_PARAMS, struct msync_parameter_table)

#define DRM_IOCTL_MTK_GET_MSYNC_PARAMS    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_MSYNC_PARAMS, struct msync_parameter_table)

#define DRM_IOCTL_MTK_FACTORY_LCM_AUTO_TEST	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_FACTORY_LCM_AUTO_TEST, int)

#define DRM_IOCTL_MTK_DRM_SET_LEASE_INFO	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_DRM_SET_LEASE_INFO, int)

#define DRM_IOCTL_MTK_DRM_GET_LEASE_INFO	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_DRM_GET_LEASE_INFO, int)


#define DRM_IOCTL_MTK_WAIT_REPAINT	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_WAIT_REPAINT, unsigned int)

#define DRM_IOCTL_MTK_GET_DISPLAY_CAPS	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_DISPLAY_CAPS, struct mtk_drm_disp_caps_info)

#define DRM_IOCTL_MTK_SET_DDP_MODE     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SET_DDP_MODE, unsigned int)

#define DRM_IOCTL_MTK_GET_SESSION_INFO     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_SESSION_INFO, struct drm_mtk_session_info)

#define DRM_IOCTL_MTK_GET_MASTER_INFO     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_MASTER_INFO, int)

#define DRM_IOCTL_MTK_SEC_HND_TO_GEM_HND     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SEC_HND_TO_GEM_HND, struct drm_mtk_sec_gem_hnd)

#define DRM_IOCTL_MTK_GET_LCM_INDEX    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_LCM_INDEX, unsigned int)

#define DRM_IOCTL_MTK_GET_PANELS_INFO   DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_PANELS_INFO, struct mtk_drm_panels_info)

#define DRM_IOCTL_MTK_SUPPORT_COLOR_TRANSFORM     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SUPPORT_COLOR_TRANSFORM, struct DISP_COLOR_TRANSFORM)

#define DRM_IOCTL_MTK_SUPPORT_COLOR_TRANSFORM    DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_SUPPORT_COLOR_TRANSFORM, \
			struct DISP_COLOR_TRANSFORM)

#define DRM_IOCTL_MTK_GET_CHIST     DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_GET_CHIST, struct drm_mtk_chist_info)

#define DRM_IOCTL_MTK_GET_CHIST_CAPS     DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_GET_CHIST_CAPS, struct drm_mtk_chist_caps)

#define DRM_IOCTL_MTK_SET_CHIST_CONFIG     DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_SET_CHIST_CONFIG, struct drm_mtk_chist_config)

#define DRM_IOCTL_MTK_GET_PQ_CAPS DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_GET_PQ_CAPS, struct mtk_drm_pq_caps_info)
#define DRM_IOCTL_MTK_SET_PQ_CAPS    DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_SET_PQ_CAPS, struct mtk_drm_pq_caps_info)

#define DRM_IOCTL_MTK_ODDMR_LOAD_PARAM    DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_ODDMR_LOAD_PARAM, struct mtk_drm_oddmr_param)
#define DRM_IOCTL_MTK_ODDMR_CTL    DRM_IOWR(DRM_COMMAND_BASE + \
				DRM_MTK_ODDMR_CTL, struct mtk_drm_oddmr_ctl)

#define DRM_IOCTL_MTK_KICK_IDLE    DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_KICK_IDLE, unsigned int)

#define DRM_IOCTL_MTK_PQ_FRAME_CONFIG    DRM_IOWR(DRM_COMMAND_BASE + \
				DRM_MTK_PQ_FRAME_CONFIG, struct mtk_drm_pq_config_ctl)

#define DRM_IOCTL_MTK_PQ_PROXY_IOCTL    DRM_IOWR(DRM_COMMAND_BASE + \
				DRM_MTK_PQ_PROXY_IOCTL, struct mtk_drm_pq_proxy_ctl)

#define DRM_IOCTL_MTK_GET_MODE_EXT_INFO DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_GET_MODE_EXT_INFO, struct mtk_drm_mode_ext_info)

#define DRM_IOCTL_MTK_HWVSYNC_ON DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_HWVSYNC_ON, unsigned int)

#define DRM_IOCTL_MTK_DEBUG_LOG     DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_DEBUG_LOG, int)

#define DRM_IOCTL_MTK_ESD_STAT_CHK    DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_ESD_STAT_CHK, unsigned int)

#define DRM_IOCTL_MTK_DUMMY_CMD_ON DRM_IOWR(DRM_COMMAND_BASE + \
			DRM_MTK_DUMMY_CMD_ON, unsigned int)

#define DRM_IOCTL_MTK_SET_OVL_LAYER    DRM_IOWR(DRM_COMMAND_BASE + \
					DRM_MTK_SET_OVL_LAYER, \
					struct mtk_drm_layer_info)

#define DRM_IOCTL_MTK_MAP_DMA_BUF    DRM_IOWR(DRM_COMMAND_BASE + \
					DRM_MTK_MAP_DMA_BUF, \
					struct mtk_drm_dma_buf)

#define DRM_IOCTL_MTK_UNMAP_DMA_BUF    DRM_IOWR(DRM_COMMAND_BASE + \
					DRM_MTK_UNMAP_DMA_BUF, int)

/* AAL IOCTL */
#define AAL_HIST_BIN            33	/* [0..32] */
#define AAL_DRE_POINT_NUM       29
#define AAL_DRE_BLK_NUM			(16)

/* Display Clarity */
#define MDP_AAL_CLARITY_READBACK_NUM (7)
#define DISP_TDSHP_CLARITY_READBACK_NUM (12)

struct DISP_AAL_INITREG {
	/* DRE */
	int dre_map_bypass;
	/* ESS */
	int cabc_gainlmt[33];
	/* DRE 3.0 Reg. */
	int dre_s_lower;
	int dre_s_upper;
	int dre_y_lower;
	int dre_y_upper;
	int dre_h_lower;
	int dre_h_upper;
	int dre_h_slope;
	int dre_s_slope;
	int dre_y_slope;
	int dre_x_alpha_base;
	int dre_x_alpha_shift_bit;
	int dre_y_alpha_base;
	int dre_y_alpha_shift_bit;
	int act_win_x_end;
	int dre_blk_x_num;
	int dre_blk_y_num;
	int dre_blk_height;
	int dre_blk_width;
	int dre_blk_area;
	int dre_blk_area_min;
	int hist_bin_type;
	int dre_flat_length_slope;
	int dre_flat_length_th;
	int act_win_y_start;
	int act_win_y_end;
	int blk_num_x_start;
	int blk_num_x_end;
	int dre0_blk_num_x_start;
	int dre0_blk_num_x_end;
	int dre1_blk_num_x_start;
	int dre1_blk_num_x_end;
	int blk_cnt_x_start;
	int blk_cnt_x_end;
	int blk_num_y_start;
	int blk_num_y_end;
	int blk_cnt_y_start;
	int blk_cnt_y_end;
	int last_tile_x_flag;
	int last_tile_y_flag;
	int act_win_x_start;
	int dre0_blk_cnt_x_start;
	int dre0_blk_cnt_x_end;
	int dre1_blk_cnt_x_start;
	int dre1_blk_cnt_x_end;
	int dre0_act_win_x_start;
	int dre0_act_win_x_end;
	int dre1_act_win_x_start;
	int dre1_act_win_x_end;
	_Bool isdual;
	int width;
	int height;
};

enum rgbSeq {
	gain_r,
	gain_g,
	gain_b,
};

struct DISP_AAL_PARAM {
	int DREGainFltStatus[AAL_DRE_POINT_NUM];
	int cabc_fltgain_force;	/* 10-bit ; [0,1023] */
	int cabc_gainlmt[33];
	int FinalBacklight;	/* 10-bit ; [0,1023] */
	int silky_bright_flag;
	int allowPartial;
	int refreshLatency;	/* DISP_AAL_REFRESH_LATENCY */
	unsigned int silky_gain_range; /* 13bit: [1-8192]; 14bit:[1-16384] */
	unsigned int silky_bright_gain[3];
	unsigned long long dre30_gain;
};

struct DISP_DRE30_INIT {
	/* DRE 3.0 SW */
	unsigned long long dre30_hist_addr;
};

struct DISP_AAL_DISPLAY_SIZE {
	int width;
	int height;
	_Bool isdualpipe;
	int aaloverhead;
};

struct DISP_AAL_HIST {
	unsigned int serviceFlags;
	int backlight;
	int panel_nits;
	int aal0_colorHist;
	int aal1_colorHist;
	unsigned int mdp_aal_ghist_valid;
	unsigned int aal0_maxHist[AAL_HIST_BIN];
	unsigned int aal1_maxHist[AAL_HIST_BIN];
	unsigned int mdp_aal0_maxHist[AAL_HIST_BIN];
	unsigned int mdp_aal1_maxHist[AAL_HIST_BIN];
	int requestPartial;
	unsigned long long dre30_hist;
	unsigned int panel_type;
	int essStrengthIndex;
	int ess_enable;
	int dre_enable;
	unsigned int aal0_yHist[AAL_HIST_BIN];
	unsigned int aal1_yHist[AAL_HIST_BIN];
	unsigned int mdp_aal0_yHist[AAL_HIST_BIN];
	unsigned int mdp_aal1_yHist[AAL_HIST_BIN];
	unsigned int MaxHis_denominator_pipe0[AAL_DRE_BLK_NUM];
	unsigned int MaxHis_denominator_pipe1[AAL_DRE_BLK_NUM];
	int srcWidth;
	int srcHeight;
	unsigned int aal0_clarity[MDP_AAL_CLARITY_READBACK_NUM];
	unsigned int aal1_clarity[MDP_AAL_CLARITY_READBACK_NUM];
	unsigned int tdshp0_clarity[DISP_TDSHP_CLARITY_READBACK_NUM];
	unsigned int tdshp1_clarity[DISP_TDSHP_CLARITY_READBACK_NUM];
	int pipeLineNum;
	unsigned int fps;
};

struct DISP_AAL_ESS20_SPECT_PARAM {
	unsigned int ELVSSPN;
	unsigned int ClarityGain; /* 10-bit ; [0,1023] */
	unsigned int flag;//
};

#define DRECOLOR_SGY_Y_ENTRY 5
#define DRECOLOR_SGY_HUE_NUM 20
#define DRECOLOR_LSP_CNT 8
struct DISP_AAL_DRECOLOR_REG {
	unsigned int sgy_en;
	unsigned int sgy_out_gain[DRECOLOR_SGY_Y_ENTRY][DRECOLOR_SGY_HUE_NUM];
	unsigned int lsp_en;
	unsigned int lsp_out_setting[DRECOLOR_LSP_CNT];
};

struct DISP_AAL_DRECOLOR_PARAM {
	bool drecolor_sel;
	struct DISP_AAL_DRECOLOR_REG drecolor_reg;
};

enum SET_BL_EXT_TYPE {
	SET_BACKLIGHT_LEVEL,
	SET_ELVSS_PN,
	ENABLE_DYN_ELVSS,
	DISABLE_DYN_ELVSS,
};

#define DRM_IOCTL_MTK_HDMI_GET_DEV_INFO     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_HDMI_GET_DEV_INFO, struct mtk_dispif_info)
#define DRM_IOCTL_MTK_HDMI_AUDIO_ENABLE     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_HDMI_AUDIO_ENABLE, unsigned int)

#define DRM_IOCTL_MTK_HDMI_AUDIO_CONFIG     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_HDMI_AUDIO_CONFIG, unsigned int)

#define DRM_IOCTL_MTK_HDMI_GET_CAPABILITY     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_HDMI_GET_CAPABILITY, unsigned int)

struct mtk_drm_mml_caps_info {
	uint32_t mode_caps;
	uint32_t hw_caps;
};

struct mtk_drm_mml_query_hw_support {
	struct mml_frame_info *info;
	bool support;
};

enum mtk_drm_mml_func {
	mtk_drm_mml_func_unknown,
	mtk_drm_mml_func_get_caps,
	mtk_drm_mml_func_query_hw_support,
};

struct mtk_drm_mml_ctrl {
	uint32_t func;
	union {
		struct mtk_drm_mml_caps_info caps;
		struct mtk_drm_mml_query_hw_support query;
	};
};

#define DRM_IOCTL_MTK_MML_CTRL DRM_IOWR(DRM_COMMAND_BASE + \
	DRM_MTK_MML_CTRL, struct mtk_drm_mml_ctrl)

#define MTK_DRM_ADVANCE
#define MTK_DRM_FORMAT_DIM		fourcc_code('D', ' ', '0', '0')
#endif /* _UAPI_MEDIATEK_DRM_H */
