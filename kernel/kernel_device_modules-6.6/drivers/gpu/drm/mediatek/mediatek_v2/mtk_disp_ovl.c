// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ratelimit.h>
#include <linux/iommu.h>
#include <arm/arm-smmu-v3/arm-smmu-v3.h>
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_layering_rule_base.h"
#include "mtk_rect.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_ovl.h"
#include "mtk_disp_pmqos.h"
#ifdef IF_ZERO
#include "mtk_iommu_ext.h"
#endif
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "platform/mtk_drm_platform.h"

#include "slbc_ops.h"
#include "../mml/mtk-mml.h"
#include <soc/mediatek/smi.h>
#include "mtk_drm_fb.h"

int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);

#define REG_FLD(width, shift)                                                  \
	((unsigned int)((((width)&0xFF) << 16) | ((shift)&0xFF)))

#define REG_FLD_MSB_LSB(msb, lsb) REG_FLD((msb) - (lsb) + 1, (lsb))

#define REG_FLD_WIDTH(field) ((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) ((unsigned int)((field)&0xFF))

#define REG_FLD_MASK(field)                                                    \
	((unsigned int)((1ULL << REG_FLD_WIDTH(field)) - 1)                    \
	 << REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val)                                                \
	(((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define DISP_REG_OVL_STA (0x000UL)
#define DISP_REG_OVL_INTEN 0x0004
#define INTEN_FLD_REG_CMT_INTEN REG_FLD_MSB_LSB(0, 0)
#define INTEN_FLD_FME_CPL_INTEN REG_FLD_MSB_LSB(1, 1)
#define INTEN_FLD_FME_UND_INTEN REG_FLD_MSB_LSB(2, 2)
#define INTEN_FLD_FME_SWRST_DONE_INTEN REG_FLD_MSB_LSB(3, 3)
#define INTEN_FLD_FME_HWRST_DONE_INTEN REG_FLD_MSB_LSB(4, 4)
#define INTEN_FLD_RDMA0_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(5, 5)
#define INTEN_FLD_RDMA1_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(6, 6)
#define INTEN_FLD_RDMA2_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(7, 7)
#define INTEN_FLD_RDMA3_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(8, 8)
#define INTEN_FLD_RDMA0_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(9, 9)
#define INTEN_FLD_RDMA1_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(10, 10)
#define INTEN_FLD_RDMA2_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(11, 11)
#define INTEN_FLD_RDMA3_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(12, 12)
#define INTEN_FLD_ABNORMAL_SOF REG_FLD_MSB_LSB(13, 13)
#define INTEN_FLD_START_INTEN REG_FLD_MSB_LSB(14, 14)
#define INIEN_ROI_TIMING_0 REG_FLD_MSB_LSB(15, 15)
#define DISP_REG_OVL_INTSTA 0x0008
#define DISP_REG_OVL_EN (0x000CUL)
#define EN_FLD_BLOCK_EXT_ULTRA			REG_FLD_MSB_LSB(18, 18)
#define EN_FLD_BLOCK_EXT_PREULTRA		REG_FLD_MSB_LSB(19, 19)
#define DISP_OVL_READ_WRK_REG			BIT(20)
#define DISP_OVL_BYPASS_SHADOW			BIT(22)
#define DISP_REG_OVL_TRIG (0x010UL)

#define DISP_REG_OVL_RST 0x0014
#define DISP_REG_OVL_ROI_SIZE 0x0020
#define OVL_ROI_TIMING_0 0x740
#define DISP_REG_OVL_DATAPATH_CON	(0x024UL)
#define DISP_OVL_BGCLR_IN_SEL BIT(2)
#define DATAPATH_PQ_OUT_SEL REG_FLD_MSB_LSB(18, 16)
#define DISP_OVL_PQ_OUT_EN REG_FLD_MSB_LSB(19, 19)
#define DISP_OVL_RDMA0_OUT_SEL REG_FLD_MSB_LSB(20, 20)
#define DISP_OVL_RDMA1_OUT_SEL REG_FLD_MSB_LSB(21, 21)
#define DISP_OVL_RDMA2_OUT_SEL REG_FLD_MSB_LSB(22, 22)
#define DISP_OVL_RDMA3_OUT_SEL REG_FLD_MSB_LSB(23, 23)
#define DISP_OVL_PQ_OUT_OPT REG_FLD_MSB_LSB(23, 23)
#define DISP_OVL_OUTPUT_CLAMP BIT(26)
#define DATAPATH_CON_FLD_LAYER_SMI_ID_EN	REG_FLD_MSB_LSB(0, 0)
#define DATAPATH_CON_FLD_GCLAST_EN		REG_FLD_MSB_LSB(24, 24)
#define DATAPATH_CON_FLD_HDR_GCLAST_EN		REG_FLD_MSB_LSB(25, 25)
#define DATAPATH_CON_FLD_OUTPUT_CLAMP		REG_FLD_MSB_LSB(26, 26)

#define DISP_REG_OVL_ROI_BGCLR 0x0028
#define DISP_REG_OVL_SRC_CON 0x002c
#define DISP_OVL_FORCE_CONSTANT_LAYER BIT(4)
#define DISP_OVL_FORCE_RELAY_MODE BIT(8)

#define DISP_REG_OVL_CON(n) (0x0030 + 0x20 * (n))
#define L_CON_FLD_APHA REG_FLD_MSB_LSB(7, 0)
#define L_CON_FLD_AEN REG_FLD_MSB_LSB(8, 8)
#define L_CON_FLD_VIRTICAL_FLIP REG_FLD_MSB_LSB(9, 9)
#define L_CON_FLD_HORI_FLIP REG_FLD_MSB_LSB(10, 10)
#define L_CON_FLD_EXT_MTX_EN REG_FLD_MSB_LSB(11, 11)
#define L_CON_FLD_CFMT REG_FLD_MSB_LSB(15, 12)
#define L_CON_FLD_MTX REG_FLD_MSB_LSB(19, 16)
#define L_CON_FLD_EN_3D REG_FLD_MSB_LSB(20, 20)
#define L_CON_FLD_EN_LANDSCAPE REG_FLD_MSB_LSB(21, 21)
#define L_CON_FLD_EN_R_FIRST REG_FLD_MSB_LSB(22, 22)
#define L_CON_FLD_CLRFMT_MAN REG_FLD_MSB_LSB(23, 23)
#define L_CON_FLD_BTSW REG_FLD_MSB_LSB(24, 24)
#define L_CON_FLD_RGB_SWAP REG_FLD_MSB_LSB(25, 25)
#define L_CON_FLD_MTX_AUTO_DIS REG_FLD_MSB_LSB(26, 26)
#define L_CON_FLD_MTX_EN REG_FLD_MSB_LSB(27, 27)
#define L_CON_FLD_LSRC REG_FLD_MSB_LSB(29, 28)
#define L_CON_FLD_SKEN REG_FLD_MSB_LSB(30, 30)
#define L_CON_FLD_DKEN REG_FLD_MSB_LSB(31, 31)
#define CON_LSRC_COLOR BIT(28)
#define CON_LSRC_UFOD BIT(29)
#define CON_LSRC_PQ (BIT(28) | BIT(29))
#define CON_VERTICAL_FLIP BIT(9)
#define CON_HORI_FLIP BIT(10)

#define DISP_REG_OVL_SRCKEY(n) (0x0034 + 0x20 * (n))
#define DISP_REG_OVL_SRC_SIZE(n) (0x0038 + 0x20 * (n))
#define DISP_REG_OVL_OFFSET(n) (0x003c + 0x20 * (n))
#define DISP_REG_OVL_PITCH_MSB(n) (0x0040 + 0x20 * (n))
#define L_PITCH_MSB_FLD_YUV_TRANS REG_FLD_MSB_LSB(20, 20)
#define L_PITCH_MSB_FLD_2ND_SUBBUF REG_FLD_MSB_LSB(16, 16)
#define L_PITCH_MSB_FLD_SRC_PITCH_MSB REG_FLD_MSB_LSB(3, 0)
#define DISP_REG_OVL_PITCH(n) (0x0044 + 0x20 * (n))
#define DISP_OVL_LAYER_CONST_BLD BIT(28)
#define DISP_REG_OVL_CLIP(n) (0x04CUL + 0x20 * (n))
#define OVL_L_CLIP_FLD_LEFT REG_FLD_MSB_LSB(7, 0)
#define OVL_L_CLIP_FLD_RIGHT REG_FLD_MSB_LSB(15, 8)
#define OVL_L_CLIP_FLD_TOP REG_FLD_MSB_LSB(23, 16)
#define OVL_L_CLIP_FLD_BOTTOM REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_OVL_RDMA_CTRL(n) (0x00c0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_GMC(n) (0x00c8 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_FIFO_CTRL(n) (0x00d0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA0_MEM_GMC_S2 (0x1E0UL)
#define FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES	REG_FLD_MSB_LSB(11, 0)
#define FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG	REG_FLD_MSB_LSB(27, 16)
#define FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA	REG_FLD_MSB_LSB(29, 29)
#define FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES	REG_FLD_MSB_LSB(30, 30)
#define DISP_REG_OVL_RDMA1_MEM_GMC_S2 (0x1E4UL)
#define DISP_REG_OVL_RDMA2_MEM_GMC_S2 (0x1E8UL)
#define DISP_REG_OVL_RDMA3_MEM_GMC_S2 (0x1ECUL)
#define DISP_REG_OVL_RDMA_BURST_CON1	(0x1F4UL)
#define FLD_RDMA_BURST_CON1_BURST16_EN		REG_FLD_MSB_LSB(28, 28)
#define FLD_RDMA_BURST_CON1_DDR_EN		REG_FLD_MSB_LSB(30, 30)
#define FLD_RDMA_BURST_CON1_DDR_ACK_EN		REG_FLD_MSB_LSB(31, 31)

#define DISP_REG_OVL_SYSRAM_CFG(n) (0x0880UL + 0x10 * n)
#define DISP_REG_OVL_SYSRAM_BUF0_ADDR(n) (0x0884UL + 0x10 * n)
#define DISP_REG_OVL_SYSRAM_BUF1_ADDR(n) (0x0888UL + 0x10 * n)

#define DISP_REG_OVL_GDRDY_PRD (0x208UL)

#define DISP_REG_OVL_RDMA0_DBG (0x24CUL)
#define DISP_REG_OVL_RDMA1_DBG (0x250UL)
#define DISP_REG_OVL_RDMA2_DBG (0x254UL)
#define DISP_REG_OVL_RDMA3_DBG (0x258UL)
#define DISP_REG_OVL_L0_CLR(n) (0x25cUL + 0x4 * (n))

#define DISP_REG_OVL_LC_CON (0x280UL)
#define DISP_REG_OVL_LC_SRCKEY (0x284UL)
#define DISP_REG_OVL_LC_SRC_SIZE (0x288UL)
#define DISP_REG_OVL_LC_OFFSET (0x28cUL)
#define DISP_REG_OVL_LC_SRC_SEL (0x290UL)
#define DISP_REG_OVL_BANK_CON (0x29cUL)
#define DISP_REG_OVL_DEBUG_MON_SEL (0x1D4UL)
#define DISP_REG_OVL_RDMA_GREQ_NUM (0x1F8UL)
#define FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM	REG_FLD_MSB_LSB(3, 0)
#define FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM	REG_FLD_MSB_LSB(7, 4)
#define FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM	REG_FLD_MSB_LSB(11, 8)
#define FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM	REG_FLD_MSB_LSB(15, 12)
#define FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM		REG_FLD_MSB_LSB(23, 16)
#define FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT		REG_FLD_MSB_LSB(26, 24)
#define FLD_OVL_RDMA_GREQ_STOP_EN		REG_FLD_MSB_LSB(27, 27)
#define FLD_OVL_RDMA_GREQ_GRP_END_STOP		REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_GREQ_GRP_BRK_STOP		REG_FLD_MSB_LSB(29, 29)
#define FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA	REG_FLD_MSB_LSB(30, 30)
#define FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA	REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_OVL_RDMA_GREQ_URG_NUM (0x1FCUL)
#define FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM	REG_FLD_MSB_LSB(3, 0)
#define FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM	REG_FLD_MSB_LSB(7, 4)
#define FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM	REG_FLD_MSB_LSB(11, 8)
#define FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM	REG_FLD_MSB_LSB(15, 12)
#define FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH	REG_FLD_MSB_LSB(25, 16)
#define FLD_OVL_RDMA_GREQ_ARG_URG_BIAS		REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_GREQ_NUM_SHT_VAL		REG_FLD_MSB_LSB(29, 29)
#define DISP_REG_OVL_DUMMY_REG (0x200UL)
#define DISP_REG_OVL_RDMA_ULTRA_SRC (0x20CUL)
#define FLD_OVL_RDMA_PREULTRA_BUF_SRC		REG_FLD_MSB_LSB(1, 0)
#define FLD_OVL_RDMA_PREULTRA_SMI_SRC		REG_FLD_MSB_LSB(3, 2)
#define FLD_OVL_RDMA_PREULTRA_ROI_END_SRC	REG_FLD_MSB_LSB(5, 4)
#define FLD_OVL_RDMA_PREULTRA_RDMA_SRC		REG_FLD_MSB_LSB(7, 6)
#define FLD_OVL_RDMA_ULTRA_BUF_SRC		REG_FLD_MSB_LSB(9, 8)
#define FLD_OVL_RDMA_ULTRA_SMI_SRC		REG_FLD_MSB_LSB(11, 10)
#define FLD_OVL_RDMA_ULTRA_ROI_END_SRC		REG_FLD_MSB_LSB(13, 12)
#define FLD_OVL_RDMA_ULTRA_RDMA_SRC		REG_FLD_MSB_LSB(15, 14)
#define DISP_OVL_REG_GDRDY_PRD (0x208UL)
#define DISP_REG_OVL_RDMAn_BUF_LOW(layer) (0x210UL + ((layer) << 2))
#define FLD_OVL_RDMA_BUF_LOW_ULTRA_TH		REG_FLD_MSB_LSB(11, 0)
#define FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH	REG_FLD_MSB_LSB(23, 12)
#define DISP_REG_OVL_RDMAn_BUF_HIGH(layer) (0x220UL + ((layer) << 2))
#define FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH REG_FLD_MSB_LSB(23, 12)
#define FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_OVL_SMI_DBG (0x230UL)
#define DISP_REG_OVL_GREQ_LAYER_CNT (0x234UL)
#define DISP_REG_OVL_GDRDY_PRD_NUM (0x238UL)
#define DISP_REG_OVL_FLOW_CTRL_DBG (0x240UL)
#define DISP_REG_OVL_ADDCON_DBG (0x244UL)
#define DISP_REG_OVL_FUNC_DCM0 (0x2a0UL)
#define FLD_OVL_FUNC_DCM0_GOLDEN		REG_FLD_MSB_LSB(3, 3)
#define DISP_REG_OVL_FUNC_DCM1 (0x2a4UL)
#define DISP_REG_OVL_CLRFMT_EXT (0x2D0UL)
#define FLD_Ln_CLRFMT_NB(n) REG_FLD_MSB_LSB((n)*4 + 1, (n)*4)
#define FLD_ELn_CLRFMT_NB(n) REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 16)
#define DISP_REG_OVL_WCG_CFG1 (0x2D8UL)
#define FLD_Ln_IGAMMA_EN(n) REG_FLD_MSB_LSB((n)*4, (n)*4)
#define FLD_Ln_CSC_EN(n) REG_FLD_MSB_LSB((n)*4 + 1, (n)*4 + 1)
#define FLD_Ln_GAMMA_EN(n) REG_FLD_MSB_LSB((n)*4 + 2, (n)*4 + 2)
#define FLD_ELn_IGAMMA_EN(n) REG_FLD_MSB_LSB((n)*4 + 16, (n)*4 + 16)
#define FLD_ELn_CSC_EN(n) REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 17)
#define FLD_ELn_GAMMA_EN(n) REG_FLD_MSB_LSB((n)*4 + 18, (n)*4 + 18)
#define DISP_REG_OVL_WCG_CFG2 (0x2DCUL)
#define FLD_Ln_IGAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 1, (n)*4)
#define FLD_Ln_GAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 3, (n)*4 + 2)
#define FLD_ELn_IGAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 16)
#define FLD_ELn_GAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 19, (n)*4 + 18)
#define DISP_REG_OVL_DATAPATH_EXT_CON (0x324UL)
#define DISP_REG_OVL_EL_CON(n) (0x330UL + 0x20 * (n))
#define DISP_REG_OVL_EL_SRCKEY(n) (0x334UL + 0x20 * (n))
#define DISP_REG_OVL_EL_SRC_SIZE(n) (0x338UL + 0x20 * (n))
#define DISP_REG_OVL_EL_OFFSET(n) (0x33CUL + 0x20 * (n))
#define DISP_REG_OVL_EL_ADDR(module, n) (0xFB0UL + (module)->data->el_addr_offset * (n))
#define DISP_REG_OVL_EL_PITCH_MSB(n) (0x340U + 0x20 * (n))
#define DISP_REG_OVL_EL_PITCH(n) (0x344U + 0x20 * (n))
#define DISP_REG_OVL_EL_TILE(n) (0x348UL + 0x20 * (n))
#define DISP_REG_OVL_EL_CLIP(n) (0x34CUL + 0x20 * (n))
#define DISP_REG_OVL_EL0_CLR(n) (0x390UL + 0x4 * (n))
#define DISP_REG_OVL_ADDR_MT2701 0x0040
#define DISP_REG_OVL_ADDR_MT6779 0x0f40
#define DISP_REG_OVL_ADDR_MT8173 0x0f40
#define DISP_REG_OVL_ADDR(module, n) ((module)->data->addr + 0x20 * (n))
#define DISP_REG_OVL_STASH_CFG0 (0xAE0UL)
#define DISP_REG_OVL_STASH_CFG1 (0xAE4UL)

#define DISP_REG_OVL_PQ_LOOP_CON (0x2E0UL)
#define DISP_OVL_PQ_OUT_SIZE_SEL BIT(0)
#define DISP_REG_OVL_PQ_OUT_SIZE (0x2E4UL)
#define FLD_OVL_PQ_OUT_SRC_W REG_FLD_MSB_LSB(12, 0)
#define FLD_OVL_PQ_OUT_SRC_H REG_FLD_MSB_LSB(28, 16)

/* OVL Bandwidth monitor */
#define DISP_REG_OVL_BURST_MON_CFG (0x97CUL)
#define FLD_OVL_BURST_ACC_EN REG_FLD_MSB_LSB(0, 0)
#define FLD_OVL_BURST_ACC_FBDC REG_FLD_MSB_LSB(4, 4)
#define FLD_OVL_BURST_ACC_WIN_SIZE REG_FLD_MSB_LSB(12, 8)

#define DISP_REG_OVL_ADDR_MSB(n) (0x0f4c + 0x20 * (n))
#define DISP_REG_OVL_EL_ADDR_MSB(n) (0x0fbc + 0x10 * (n))

#define OVL_LAYER_OFFSET (0x20)
#define DISP_REG_OVL_LX_HDR_ADDR(n) (0xF44UL + 0x20 * (n))
#define DISP_REG_OVL_LX_HDR_PITCH(n) (0xF48UL + 0x20 * (n))

#define DISP_REG_OVL_ELX_HDR_ADDR(module, n) \
	((module)->data->el_hdr_addr + \
	(module)->data->el_hdr_addr_offset * (n))

#define DISP_REG_OVL_ELX_HDR_PITCH(module, n) \
	((module)->data->el_hdr_addr + 0x04UL + \
	(module)->data->el_hdr_addr_offset * (n))

#define DISP_REG_OVL_L0_OFFSET (0x03CUL)
#define DISP_REG_OVL_L0_SRC_SIZE (0x038UL)
#define DISP_REG_OVL_L0_PITCH (0x044UL)
#define L_PITCH_FLD_SRC_PITCH REG_FLD_MSB_LSB(15, 0)

#define ADDCON_DBG_FLD_ROI_X REG_FLD_MSB_LSB(12, 0)
#define ADDCON_DBG_FLD_L0_WIN_HIT REG_FLD_MSB_LSB(14, 14)
#define ADDCON_DBG_FLD_L1_WIN_HIT REG_FLD_MSB_LSB(15, 15)
#define ADDCON_DBG_FLD_ROI_Y REG_FLD_MSB_LSB(28, 16)
#define ADDCON_DBG_FLD_L2_WIN_HIT REG_FLD_MSB_LSB(30, 30)
#define ADDCON_DBG_FLD_L3_WIN_HIT REG_FLD_MSB_LSB(31, 31)
#define DATAPATH_CON_FLD_BGCLR_IN_SEL REG_FLD_MSB_LSB(2, 2)
#define DISP_REG_OVL_RDMA0_CTRL (0x0C0UL)
#define RDMA0_CTRL_FLD_RDMA_EN REG_FLD_MSB_LSB(0, 0)
#define RDMA0_CTRL_FLD_RDMA_INTERLACE REG_FLD_MSB_LSB(4, 4)
#define RDMA0_CTRL_FLD_RMDA_FIFO_USED_SZ REG_FLD_MSB_LSB(27, 16)

#define DISP_REG_OVL_RDMA0_MEM_GMC_SETTING (0x0C8UL)
#define FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD REG_FLD_MSB_LSB(9, 0)
#define FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD REG_FLD_MSB_LSB(25, 16)
#define FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD_HIGH_OFS REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD_HIGH_OFS                      \
	REG_FLD_MSB_LSB(31, 31)

#define DISP_REG_OVL_RDMA0_MEM_SLOW_CON (0x0CCUL)
#define DISP_REG_OVL_RDMA0_FIFO_CTRL (0x0D0UL)
#define FLD_OVL_RDMA_FIFO_THRD REG_FLD_MSB_LSB(9, 0)
#define FLD_OVL_RDMA_FIFO_SIZE REG_FLD_MSB_LSB(27, 16)
#define FLD_OVL_RDMA_FIFO_UND_EN REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_OVL_Ln_R2R_PARA(n) (0x500UL + 0x40 * (n))
#define DISP_REG_OVL_ELn_R2R_PARA(n) (0x600UL + 0x40 * (n))
#define DISP_REG_OVL_FBDC_CFG1 (0x804UL)
#define FLD_FBDC_8XE_MODE			REG_FLD_MSB_LSB(24, 24)
#define FLD_FBDC_FILTER_EN			REG_FLD_MSB_LSB(28, 28)
#define FBDC_8XE_MODE BIT(24)
#define FBDC_FILTER_EN BIT(28)

#define OVL_SECURE 0xfc0
#define EXT_SECURE_OFFSET 4

#define OVL_LAYER_DOMAIN 0xfc4
#define OVL_LAYER_EXT_DOMAIN 0xfc8
#define OVL_LAYER_Lx_DOMAIN(id)		REG_FLD_MSB_LSB((4 + 8 * id), (0 + 8 * id))
#define OVL_LAYER_ELx_DOMAIN(id)	REG_FLD_MSB_LSB((4 + 8 * id), (0 + 8 * id))

#define OVL_LAYER_SVP_DOMAIN_INDEX  (4)
#define OVL_RDMA_DEBUG_OFFSET (0x4)

#define OVL_RDMA_MEM_GMC 0x40402020
#define OVL_ROI_BGCLR (0xFF000000)

#define OVL_CON_CLRFMT_MAN BIT(23)
#define OVL_CON_BYTE_SWAP BIT(24)
#define OVL_CON_RGB_SWAP BIT(25)
#define OVL_CON_MTX_JPEG_TO_RGB (4UL << 16)
#define OVL_CON_MTX_BT709_FULL_RANGE_TO_RGB (5UL << 16)
#define OVL_CON_MTX_BT601_TO_RGB (6UL << 16)
#define OVL_CON_MTX_BT709_TO_RGB (7UL << 16)
#define OVL_CON_CLRFMT_RGB (1UL << 12)
#define OVL_CON_CLRFMT_RGBA8888 (2 << 12)
#define OVL_CON_CLRFMT_ARGB8888 (3 << 12)
#define OVL_CON_CLRFMT_DIM (1 << 28)
#define OVL_CON_CLRFMT_RGB565(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? 0UL : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? OVL_CON_CLRFMT_RGB : 0UL)
#define OVL_CON_CLRFMT_UYVY(module) ((module)->data->fmt_uyvy)
#define OVL_CON_CLRFMT_YUYV(module) ((module)->data->fmt_yuyv)
#define OVL_CON_AEN BIT(8)
#define OVL_CON_ALPHA 0xff

#define M4U_PORT_DISP_OVL0_HDR 1
#define M4U_PORT_DISP_OVL0 3
#define M4U_PORT_DISP_OVL0_2L_HDR ((1 << 5) + 0)
#define M4U_PORT_DISP_OVL0_2L ((1 << 5) + 2)

/* AID offset in mmsys config */
#define MT6989_OVL0_2L_AID_SEL	(0xB00UL)
#define MT6989_OVL1_2L_AID_SEL	(0xB20UL)
#define MT6989_OVL2_2L_AID_SEL	(0xB40UL)
#define MT6989_OVL3_2L_AID_SEL	(0xB60UL)
#define MT6989_OVL4_2L_AID_SEL	(0xBA0UL)
#define MT6989_OVL_LAYER_OFFEST	0x4

#define MT6985_OVL0_2L_AID_SEL	(0xB00UL)
#define MT6985_OVL1_2L_AID_SEL	(0xB20UL)
#define MT6985_OVL2_2L_AID_SEL	(0xB40UL)
#define MT6985_OVL3_2L_AID_SEL	(0xB60UL)
#define MT6985_OVL_LAYER_OFFEST	0x4

#define MT6983_OVL0_AID_SEL	(0xB00UL)
#define MT6983_OVL1_2L_AID_SEL	(0xB08UL)
#define MT6983_OVL0_2L_NWCG_AID_SEL (0xB0CUL)
#define MT6983_OVL1_2L_NWCG_AID_SEL (0xB10UL)

#define MT6895_OVL0_AID_SEL	(0xB00UL)
#define MT6895_OVL0_2L_AID_SEL	(0xB04UL)
#define MT6895_OVL1_2L_AID_SEL	(0xB08UL)

#define MT6879_OVL0_AID_SEL	(0xB00UL)
#define MT6879_OVL0_2L_AID_SEL	(0xB04UL)
#define MT6879_OVL0_2L_NWCG_AID_SEL (0xB0CUL)

#define MT6886_OVL0_AID_SEL		(0xC00UL)
#define MT6886_OVL0_2L_AID_SEL		(0xC1CUL)
#define MT6886_OVL1_2L_AID_SEL		(0xC30UL)
#define MT6886_OVL0_2L_NWCG_AID_SEL		(0xB0CUL)
#define MT6886_OVL1_2L_NWCG_AID_SEL		(0xB10UL)

#define SMI_LARB_NON_SEC_CON        0x380
#define DISP_REG_OVL_SMI_2ND_CFG	(0x8F0)

#define MML_SRAM_SHIFT (512*1024)

enum GS_OVL_FLD {
	GS_OVL_RDMA_ULTRA_TH = 0,
	GS_OVL_RDMA_PRE_ULTRA_TH,
	GS_OVL_RDMA_FIFO_THRD,
	GS_OVL_RDMA_FIFO_SIZE,
	GS_OVL_RDMA_ISSUE_REQ_TH,
	GS_OVL_RDMA_ISSUE_REQ_TH_URG,
	GS_OVL_RDMA_REQ_TH_PRE_ULTRA,
	GS_OVL_RDMA_REQ_TH_ULTRA,
	GS_OVL_RDMA_FORCE_REQ_TH,
	GS_OVL_RDMA_GREQ_NUM,     /* whole reg */
	GS_OVL_RDMA_GREQ_URG_NUM, /* whole reg */
	GS_OVL_RDMA_ULTRA_SRC,    /* whole reg */
	GS_OVL_RDMA_ULTRA_LOW_TH,
	GS_OVL_RDMA_PRE_ULTRA_LOW_TH,
	GS_OVL_RDMA_PRE_ULTRA_HIGH_TH,
	GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS,
	GS_OVL_BLOCK_EXT_ULTRA,
	GS_OVL_BLOCK_EXT_PRE_ULTRA,
	GS_OVL_STASH_EN,
	GS_OVL_STASH_CFG,
	GS_OVL_FLD_NUM,
};

#define CSC_COEF_NUM 9

static s32 sRGB_to_DCI_P3[CSC_COEF_NUM] = {
215603,  46541,      0,
8702,   253442,      0,
4478,    18979, 238687};

static s32 DCI_P3_to_sRGB[CSC_COEF_NUM] = {
321111, -58967,      0,
-11025, 273169,      0,
-5148,  -20614, 287906};

static s32 identity[CSC_COEF_NUM] = {
262144,      0,      0,
0,      262144,      0,
0,           0, 262144};


#define DECLARE_MTK_OVL_COLORSPACE(EXPR)                                       \
	EXPR(OVL_SRGB)                                                         \
	EXPR(OVL_P3)                                                           \
	EXPR(OVL_CS_NUM)                                                       \
	EXPR(OVL_CS_UNKNOWN)

enum mtk_ovl_colorspace { DECLARE_MTK_OVL_COLORSPACE(DECLARE_NUM) };

static const char * const mtk_ovl_colorspace_str[] = {
	DECLARE_MTK_OVL_COLORSPACE(DECLARE_STR)};

#define DECLARE_MTK_OVL_TRANSFER(EXPR)                                         \
	EXPR(OVL_GAMMA2_2)                                                     \
	EXPR(OVL_GAMMA2)                                                       \
	EXPR(OVL_LINEAR)                                                       \
	EXPR(OVL_GAMMA_NUM)                                                    \
	EXPR(OVL_GAMMA_UNKNOWN)

enum mtk_ovl_transfer { DECLARE_MTK_OVL_TRANSFER(DECLARE_NUM) };

static const char * const mtk_ovl_transfer_str[] = {
	DECLARE_MTK_OVL_TRANSFER(DECLARE_STR)};

#define MAX_LAYER_NUM 4
struct mtk_ovl_backup_info {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int con;
	dma_addr_t addr;
	unsigned int src_size;
	unsigned int src_pitch;
	unsigned int data_path_con;
};

/**
 * struct mtk_disp_ovl - DISP_OVL driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report vblank events to
 */
struct mtk_disp_ovl {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_ovl_data *data;
	unsigned int underflow_cnt;
	bool ovl_dis;
	int bg_w, bg_h;
	struct clk *fbdc_clk;
	struct mtk_ovl_backup_info backup_info[MAX_LAYER_NUM];
	unsigned int set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_ovl *comp_to_ovl(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ovl, ddp_comp);
}

int mtk_ovl_layer_num(struct mtk_ddp_comp *comp)
{
	if (mtk_ddp_comp_get_type(comp->id) == MTK_OVL_EXDMA)
		return 1;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL1:
		return 4;
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
	case DDP_COMPONENT_OVL4_2L:
	case DDP_COMPONENT_OVL5_2L:
	case DDP_COMPONENT_OVL6_2L:
	case DDP_COMPONENT_OVL7_2L:
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL1_2L_NWCG:
	case DDP_COMPONENT_OVL2_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		return 2;
	default:
		DDPPR_ERR("invalid ovl module=%d\n", comp->id);
		return -1;
	}
	return 0;
}

resource_size_t mtk_ovl_mmsys_mapping_MT6983(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL1_2L_NWCG:
		return priv->config_regs_pa;
	case DDP_COMPONENT_OVL1:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL2_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		return priv->side_config_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6985(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		return priv->ovlsys0_regs_pa;
	case DDP_COMPONENT_OVL4_2L:
	case DDP_COMPONENT_OVL5_2L:
	case DDP_COMPONENT_OVL6_2L:
	case DDP_COMPONENT_OVL7_2L:
		return priv->ovlsys1_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6989(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
		return priv->ovlsys0_regs_pa;
	case DDP_COMPONENT_OVL3_2L:
	case DDP_COMPONENT_OVL4_2L:
	case DDP_COMPONENT_OVL5_2L:
		return priv->ovlsys1_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_sys_mapping_MT6989(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
		return 0;
	case DDP_COMPONENT_OVL3_2L:
	case DDP_COMPONENT_OVL4_2L:
	case DDP_COMPONENT_OVL5_2L:
		return 1;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return -1;
	}
}

unsigned int ovl_comp_frame_done_event_MT6989(struct mtk_ddp_comp *comp)
{
#define CMDQ_EVENT_OVL1_DISP_OVL2_2L_FRAME_DONE 153
#define CMDQ_EVENT_OVL1_DISP_OVL1_2L_FRAME_DONE 154
#define CMDQ_EVENT_OVL1_DISP_OVL0_2L_FRAME_DONE 155

#define CMDQ_EVENT_OVL0_DISP_OVL2_2L_FRAME_DONE 217
#define CMDQ_EVENT_OVL0_DISP_OVL1_2L_FRAME_DONE 218
#define CMDQ_EVENT_OVL0_DISP_OVL0_2L_FRAME_DONE 219

	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL0_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL1_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL1_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL2_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL2_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL3_2L:
		return CMDQ_EVENT_OVL1_DISP_OVL0_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL4_2L:
		return CMDQ_EVENT_OVL1_DISP_OVL1_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL5_2L:
		return CMDQ_EVENT_OVL1_DISP_OVL2_2L_FRAME_DONE;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6899(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		return priv->ovlsys0_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_sys_mapping_MT6899(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		return 0;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return -1;
	}
}

unsigned int ovl_comp_frame_done_event_MT6899(struct mtk_ddp_comp *comp)
{

#define CMDQ_EVENT_OVL0_DISP_OVL3_2L_FRAME_DONE 215
#define CMDQ_EVENT_OVL0_DISP_OVL2_2L_FRAME_DONE 216
#define CMDQ_EVENT_OVL0_DISP_OVL1_2L_FRAME_DONE 217
#define CMDQ_EVENT_OVL0_DISP_OVL0_2L_FRAME_DONE 218

	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL0_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL1_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL1_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL2_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL2_2L_FRAME_DONE;
	case DDP_COMPONENT_OVL3_2L:
		return CMDQ_EVENT_OVL0_DISP_OVL3_2L_FRAME_DONE;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}


resource_size_t mtk_ovl_mmsys_mapping_MT6886(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL0_2L_NWCG:
		return priv->config_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6895(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
		return priv->config_regs_pa;
	case DDP_COMPONENT_OVL1:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		return priv->side_config_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6879(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL0_2L_NWCG:
		return priv->config_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6983(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL1:
		return MT6983_OVL0_AID_SEL;
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL2_2L:
		/* in MT6983 DDP_component_ovl0_2l use ovl1_2l HW*/
		return MT6983_OVL1_2L_AID_SEL;
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL2_2L_NWCG:
		return MT6983_OVL0_2L_NWCG_AID_SEL;
	case DDP_COMPONENT_OVL1_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		return MT6983_OVL1_2L_NWCG_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6985(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL4_2L:
		return MT6985_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL5_2L:
		return MT6985_OVL1_2L_AID_SEL;
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL6_2L:
		return MT6985_OVL2_2L_AID_SEL;
	case DDP_COMPONENT_OVL3_2L:
	case DDP_COMPONENT_OVL7_2L:
		return MT6985_OVL3_2L_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6989(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL3_2L:
		return MT6989_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL4_2L:
		return MT6989_OVL1_2L_AID_SEL;
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL5_2L:
		return MT6989_OVL2_2L_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6899(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
		return MT6989_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL1_2L:
		return MT6989_OVL1_2L_AID_SEL;
	case DDP_COMPONENT_OVL2_2L:
		return MT6989_OVL2_2L_AID_SEL;
	case DDP_COMPONENT_OVL3_2L:
		return MT6989_OVL3_2L_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6886(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
		return MT6886_OVL0_AID_SEL;
	case DDP_COMPONENT_OVL0_2L:
		return MT6886_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL1_2L:
		return MT6886_OVL1_2L_AID_SEL;
	case DDP_COMPONENT_OVL0_2L_NWCG:
		return MT6886_OVL0_2L_NWCG_AID_SEL;
	case DDP_COMPONENT_OVL1_2L_NWCG:
		return MT6886_OVL1_2L_NWCG_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6895(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL1:
		return MT6895_OVL0_AID_SEL;
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL2_2L:
		return MT6895_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL3_2L:
		return MT6895_OVL1_2L_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_aid_sel_MT6879(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
		return MT6879_OVL0_AID_SEL;
	case DDP_COMPONENT_OVL0_2L:
		return MT6879_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL0_2L_NWCG:
		return MT6879_OVL0_2L_NWCG_AID_SEL;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

bool is_right_ovl_comp_MT6985(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		return false;
	case DDP_COMPONENT_OVL4_2L:
	case DDP_COMPONENT_OVL5_2L:
	case DDP_COMPONENT_OVL6_2L:
	case DDP_COMPONENT_OVL7_2L:
		return true;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return false;
	}
}

static unsigned int mtk_ovl_phy_mapping_MT6989(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
		return 0;
	case DDP_COMPONENT_OVL1_2L:
		return 2;
	case DDP_COMPONENT_OVL2_2L:
		return 4;
	case DDP_COMPONENT_OVL3_2L:
		return 6;
	case DDP_COMPONENT_OVL4_2L:
		return 8;
	case DDP_COMPONENT_OVL5_2L:
		return 10;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

static unsigned int mtk_ovl_phy_mapping_MT6899(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
		return 0;
	case DDP_COMPONENT_OVL1_2L:
		return 2;
	case DDP_COMPONENT_OVL2_2L:
		return 4;
	case DDP_COMPONENT_OVL3_2L:
		return 6;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

static void mtk_ovl_update_hrt_usage(struct mtk_drm_crtc *mtk_crtc,
			struct mtk_ddp_comp *comp, struct mtk_plane_state *plane_state)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	unsigned int lye_id = plane_state->comp_state.lye_id;
	unsigned int ext_lye_id = plane_state->comp_state.ext_lye_id;
	int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	unsigned int fmt;
	unsigned int phy_id = 0;

	if (!plane_state->base.fb) {
		DDPINFO("%s ovl:%d,lye:%d,ext:%d, not found fb\n", __func__,
				plane_state->comp_state.comp_id,
				plane_state->comp_state.lye_id,
				plane_state->comp_state.ext_lye_id);
		return;
	}
	DDPINFO("%s ovl:%d,lye:%d,ext:%d,fmt:0x%x, addr:0x%llx,comp:0x%llx\n", __func__,
			plane_state->comp_state.comp_id,
			plane_state->comp_state.lye_id,
			plane_state->comp_state.ext_lye_id,
			plane_state->base.fb->format->format,
			mtk_fb_get_dma(plane_state->base.fb),
			plane_state->prop_val[PLANE_PROP_COMPRESS]);

	fmt = plane_state->base.fb->format->format;

	if (ovl->data->ovl_phy_mapping) {
		phy_id = ovl->data->ovl_phy_mapping(comp);
		if (phy_id + lye_id >= ARRAY_SIZE(mtk_crtc->usage_ovl_fmt)) {
			DDPINFO("%s, invalid layer:%u+%u\n", __func__,
				phy_id, lye_id);
			return;
		}

		if (ext_lye_id == 0) {
			mtk_crtc->usage_ovl_fmt[(phy_id + lye_id)] = mtk_get_format_bpp(fmt);
			mtk_crtc->usage_ovl_compr[(phy_id + lye_id)] =
					plane_state->prop_val[PLANE_PROP_COMPRESS];
		}
	}
}

int mtk_ovl_aid_bit(struct mtk_ddp_comp *comp, bool is_ext, int id)
{
	if (is_ext)
		return mtk_ovl_layer_num(comp) + id;
	else
		return id;
}

static void dump_ovl_layer_trace(struct mtk_drm_crtc *mtk_crtc,
				 struct mtk_ddp_comp *ovl)
{
#ifdef IF_ZERO /* not ready for dummy register method */
#define LEN 1000
	struct cmdq_pkt_buffer *cmdq_buf = NULL;
	u32 offset = 0;
	u32 idx = 0;
	u32 gdrdy_num = 0, layer_en = 0, compress = 0;
	u32 ext_layer_en = 0, ext_layer_compress = 0;

	const int lnr = mtk_ovl_layer_num(ovl);
	int i = 0;
	u32 w = 0, h = 0, size = 0, con = 0, fmt = 0, src = 0;

	struct mtk_drm_private *priv = NULL;
	char msg[LEN];
	int n = 0;

	if (!mtk_crtc)
		return;
	priv = mtk_crtc->base.dev->dev_private;
	if (!(mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_LAYER_REC) &&
	      mtk_crtc->layer_rec_en))
		return;

	if (ovl->id == DDP_COMPONENT_OVL0_2L)
		offset = DISP_SLOT_LAYER_REC_OVL0_2L;
	else if (ovl->id == DDP_COMPONENT_OVL0)
		offset = DISP_SLOT_LAYER_REC_OVL0;
	else
		return;

	cmdq_buf = &mtk_crtc->gce_obj.buf;

	idx = *(u32 *)(cmdq_buf->va_base + DISP_SLOT_TRIG_CNT);

	gdrdy_num = *(u32 *)(cmdq_buf->va_base + offset);
	gdrdy_num <<= 4;
	n = snprintf(msg, LEN, "idx:%u,ovl%s:bw:%u", idx,
		     ovl->id == DDP_COMPONENT_OVL0 ? "0" : "0_2l", gdrdy_num);

	offset += 4;
	layer_en = *(u32 *)(cmdq_buf->va_base + offset);
	layer_en &= 0xf;

	offset += 4;
	compress = *(u32 *)(cmdq_buf->va_base + offset);
	compress = (compress >> 4) & 0xf;

	offset += 4;
	ext_layer_en = *(u32 *)(cmdq_buf->va_base + offset);
	ext_layer_compress = (ext_layer_en >> 4) & 0x7;
	ext_layer_en &= 0x7;

	for (i = 0; i < lnr + 3; i++) {
		if (i < lnr) {
			if (!(layer_en & 0x1)) {
				offset += (0x4 * 2);
				goto next;
			}
		} else {
			if (!(ext_layer_en & 0x1)) {
				offset += (0x4 * 2);
				goto next;
			}
		}

		offset += 0x4;
		con = *(u32 *)(cmdq_buf->va_base + offset);
		fmt = (con >> 12) & 0xf;
		src = (con >> 28) & 0x3;

		offset += 0x4;
		size = *(u32 *)(cmdq_buf->va_base + offset);
		w = size & 0x1fff;
		h = (size >> 16) & 0x1fff;

		if (i < lnr) {
			n += snprintf(msg + n, LEN - n,
				      "|L%d:%dx%d,f:0x%x,c:%d,src:%d",
				      i, w, h, fmt, compress & 0x1, src);
		} else {
			n += snprintf(msg + n, LEN - n,
				      "|L%d:%dx%d,f:0x%x,c:%d,src:%d",
				      i, w, h, fmt, ext_layer_compress & 0x1,
				      src);
		}

next:
		if (i < lnr) {
			layer_en >>= 1;
			compress >>= 1;
		} else {
			ext_layer_en >>= 1;
			ext_layer_compress >>= 1;
		}
	}

	n += snprintf(msg + n, LEN - n, "\n");
	trace_layer_bw(msg);
#endif
}
static irqreturn_t mtk_disp_ovl_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ovl *priv = dev_id;
	struct mtk_ddp_comp *ovl = NULL;
	struct mtk_drm_private *drv_priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int val = 0;
	unsigned int ret = 0;
	static DEFINE_RATELIMIT_STATE(isr_ratelimit, 1 * HZ, 4);

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	ovl = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(ovl))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(ovl) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(ovl->regs + DISP_REG_OVL_INTSTA);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}
	DRM_MMP_MARK(IRQ, ovl->regs_pa, val);

	mtk_crtc = ovl->mtk_crtc;
	if (mtk_crtc)
		drv_priv = mtk_crtc->base.dev->dev_private;

	if (ovl->id == DDP_COMPONENT_OVL0)
		DRM_MMP_MARK(ovl0, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL1)
		DRM_MMP_MARK(ovl1, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL0_2L)
		DRM_MMP_MARK(ovl0_2l, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL1_2L)
		DRM_MMP_MARK(ovl1_2l, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL2_2L)
		DRM_MMP_MARK(ovl2_2l, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL3_2L)
		DRM_MMP_MARK(ovl3_2l, val, 0);

	if (val & 0x1e0)
		DRM_MMP_MARK(abnormal_irq, val, ovl->id);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(ovl), val);

	writel(~val, ovl->regs + DISP_REG_OVL_INTSTA);

	if (val & (1 << 0))
		DDPIRQ("[IRQ] %s: reg commit!\n", mtk_dump_comp_str(ovl));
	if (val & (1 << 1)) {
		DDPIRQ("[IRQ] %s: frame done!\n", mtk_dump_comp_str(ovl));

		dump_ovl_layer_trace(mtk_crtc, ovl);
	}
	if ((ovl->id == DDP_COMPONENT_OVL0_2L) && (val & (1 << 15))) {
		DDPIRQ("[IRQ] %s: OVL target line\n", mtk_dump_comp_str(ovl));
		if (mtk_crtc && mtk_crtc->esd_ctx) {
			if (drv_priv && (drv_priv->data->mmsys_id == MMSYS_MT6985 ||
				drv_priv->data->mmsys_id == MMSYS_MT6897)) {
				unsigned int index = drm_crtc_index(&mtk_crtc->base);

				CRTC_MMP_MARK(index, target_time, ovl->id, 0xffff0001);
				atomic_set(&mtk_crtc->esd_ctx->target_time, 1);
				wake_up_interruptible(&mtk_crtc->esd_ctx->check_task_wq);
			}
		}
	}
	if (val & (1 << 2)) {
		unsigned long long aee_now_ts = sched_clock();

		if (drv_priv && (!atomic_read(&drv_priv->need_recover))) {
			struct mtk_crtc_state *state;

			state = to_mtk_crtc_state(mtk_crtc->base.state);
			if (state) {
				atomic_set(&drv_priv->need_recover, state->lye_state.mml_ir_lye);
				mtk_dprec_snapshot();
			}
		}

		if (__ratelimit(&isr_ratelimit)) {
			unsigned int smi_cnt = 0;

			if (val & (1 << 13))
				smi_cnt = readl(ovl->regs + DISP_REG_OVL_GREQ_LAYER_CNT);
			DDPPR_ERR("[IRQ] %s: frame underflow! %u reqs are smi hang, cnt=%d\n",
				  mtk_dump_comp_str(ovl), smi_cnt, priv->underflow_cnt);
		}
		priv->underflow_cnt++;
		if (mtk_crtc && (mtk_crtc->last_aee_trigger_ts == 0 ||
			(aee_now_ts - mtk_crtc->last_aee_trigger_ts > TIGGER_INTERVAL_S(10)))) {
			mtk_ovl_dump(ovl);
			mtk_ovl_analysis(ovl);
			mtk_crtc->last_aee_trigger_ts = aee_now_ts;
		}
	}
	if (val & (1 << 3))
		DDPIRQ("[IRQ] %s: sw reset done!\n", mtk_dump_comp_str(ovl));

	if (drv_priv && (!mtk_drm_helper_get_opt(drv_priv->helper_opt,
						 MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK))) {
		mtk_crtc_vblank_irq(&mtk_crtc->base);
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put(ovl);

	return ret;
}

#ifdef IF_ZERO
static void mtk_ovl_enable_vblank(struct mtk_ddp_comp *comp,
				  struct drm_crtc *crtc,
				  struct cmdq_pkt *handle)
{
	unsigned int inten;

	writel(0x0, comp->regs + DISP_REG_OVL_INTSTA);
	inten = 0x1E0 | REG_FLD_VAL(INTEN_FLD_ABNORMAL_SOF, 1) |
		REG_FLD_VAL(INTEN_FLD_START_INTEN, 1);
	writel_relaxed(inten, comp->regs + DISP_REG_OVL_INTEN);
}

static void mtk_ovl_disable_vblank(struct mtk_ddp_comp *comp,
				   struct cmdq_pkt *handle)
{
	writel_relaxed(0x0, comp->regs + DISP_REG_OVL_INTEN);
}
#endif

static int mtk_ovl_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params);

static void mtk_ovl_all_layer_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int keep_first_layer)
{
	int i;
	unsigned int phy_layer0_on = 0;

	/* In 6779 we need to set DISP_OVL_FORCE_RELAY_MODE */

	/* To make sure the OVL_SRC_CON register keep the same value
	 * as readl while writing the new value in GCE. This function should
	 * only used in driver probe.
	 */
	if (keep_first_layer)
		phy_layer0_on = readl(comp->regs + DISP_REG_OVL_SRC_CON) & 0x1;
	if (phy_layer0_on)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0x1, ~0);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON,
			       DISP_OVL_FORCE_RELAY_MODE, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON, 0, ~0);

	for (i = phy_layer0_on ? 1 : 0; i < OVL_PHY_LAYER_NR; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(i), 0,
			       ~0);
}

static void mtk_ovl_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int val;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	const struct compress_info *compr_info = ovl->data->compr_info;
	unsigned int value = 0, mask = 0;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	mtk_ovl_io_cmd(comp, handle, IRQ_LEVEL_NORMAL, NULL);

	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_RST, BIT(0), BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_RST, 0x0, BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_INTSTA, 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_EN,
		       0x1, 0x1);

	/* In 6779 we need to set DISP_OVL_FORCE_RELAY_MODE */
	if (compr_info && strncmp(compr_info->name, "PVRIC_V3_1", 10) == 0) {
		val = FBDC_8XE_MODE | FBDC_FILTER_EN;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_FBDC_CFG1, val, val);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_SRC_CON,
		       DISP_OVL_FORCE_RELAY_MODE, DISP_OVL_FORCE_RELAY_MODE);


	SET_VAL_MASK(value, mask, 1, FLD_RDMA_BURST_CON1_BURST16_EN);
	if (priv->data->mmsys_id == MMSYS_MT6989 || priv->data->mmsys_id == MMSYS_MT6899) {
		/* golden setting */
		SET_VAL_MASK(value, mask, 0, FLD_RDMA_BURST_CON1_DDR_EN);
		SET_VAL_MASK(value, mask, 0, FLD_RDMA_BURST_CON1_DDR_ACK_EN);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_RDMA_BURST_CON1,
		       value, mask);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_LAYER_SMI_ID_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_HDR_GCLAST_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_GCLAST_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_OUTPUT_CLAMP);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
		       value, mask);

	if (priv->data->mmsys_id == MMSYS_MT6989 || priv->data->mmsys_id == MMSYS_MT6899) {
		/* golden setting */
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 1, FLD_OVL_FUNC_DCM0_GOLDEN);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_FUNC_DCM0,
			       value, mask);
	}

	/* Enable feedback real BW consumed from OVL */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_GDRDY_PRD,
		0xFFFFFFFF, 0xFFFFFFFF);

	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_INTEN, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_EN,
		       0x0, 0x1);

	mtk_ovl_all_layer_off(comp, handle, 0);

	comp->qos_bw = 0;
	comp->qos_bw_other = 0;
	comp->fbdc_bw = 0;
	comp->hrt_bw = 0;
	comp->hrt_bw_other = 0;
	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_reset(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_RST, BIT(0) | BIT(28), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_RST, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_RST, 0, ~0);
	DDPINFO("%s-\n", __func__);
}

static void _store_bg_roi(struct mtk_ddp_comp *comp, int h, int w)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	ovl->bg_h = h;
	ovl->bg_w = w;
}

static void _get_bg_roi(struct mtk_ddp_comp *comp, int *h, int *w)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (ovl->set_partial_update != 1)
		*h = ovl->bg_h;
	else
		*h = ovl->roi_height;

	*w = ovl->bg_w;
}

static int mtk_ovl_golden_setting(struct mtk_ddp_comp *comp,
				  struct mtk_ddp_config *cfg,
				  struct cmdq_pkt *handle);

static void mtk_ovl_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width, height;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned long crtc_idx = (unsigned long)drm_crtc_index(crtc);
	int fps;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (comp->mtk_crtc->is_dual_pipe) {
		if (cfg->tile_overhead.is_support) {
			if (ovl->data->is_right_ovl_comp && ovl->data->is_right_ovl_comp(comp))
				width = cfg->tile_overhead.right_in_width;
			else
				width = cfg->tile_overhead.left_in_width;
		} else
			width = cfg->w / 2;
		if (drm_crtc_index(crtc) == 2 && (width % 2)) {
			if (ovl->data->is_right_ovl_comp && ovl->data->is_right_ovl_comp(comp))
				width += 1;
			else
				width -= 1;
		}
	} else
		width = cfg->w;

	if (ovl->set_partial_update != 1)
		height = cfg->h;
	else
		height = ovl->roi_height;

	if (cfg->w != 0 && cfg->h != 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_OVL_ROI_SIZE,
				   height << 16 | width, ~0);

		_store_bg_roi(comp, height, width);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_ROI_BGCLR, OVL_ROI_BGCLR,
		       ~0);
	//enable sram dbg reg:0x900~0x934
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_TRIG, 0x1000, 0x1000);


	mtk_ddp_write(comp, (height * 9) / 10,
		OVL_ROI_TIMING_0, handle);

	DDPINFO("%s -> %u\n", __func__, (height * 9) / 10);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR) &&
		(crtc_idx == 0)) {
		unsigned int bw_monitor_config;

		/****************************************************************/
		/*BURST_ACC_FBDC: 1/0:fbdc size/actual BW(fbdc+sBCH)            */
		/*BURST_ACC_EN: 1: enable bw monitor 0: disable                 */
		/*BURST_ACC_WIN_SIZE: check below table                         */
		/*Scenario | 4AFBC line times(us) | Best fit to 200us MD window */
		/*FHD+@60  | 22.0                 | 198(9w)                     */
		/*FHD+@120 | 11.0                 | 198(18w)                    */
		/*WQHD+@60 | 16.5                 | 198(12w)                    */
		/*WQHD+@120| 8.26                 | 198(24w)                    */
		/****************************************************************/
		bw_monitor_config = REG_FLD_VAL(FLD_OVL_BURST_ACC_EN, 1);
		bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_FBDC, 0);

		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
			fps =
				mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
		else
			fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);

		if (crtc->state->adjusted_mode.hdisplay <= 1080) {
			if (fps == 30) {
				bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_WIN_SIZE, 4);
				ovl_win_size = 5;
			} else if (fps <= 60) {
				bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_WIN_SIZE, 8);
				ovl_win_size = 9;
			} else {
				bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_WIN_SIZE, 17);
				ovl_win_size = 18;
			}
		} else {
			if (fps == 30) {
				bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_WIN_SIZE, 4);
				ovl_win_size = 5;
			} else if (fps <= 60) {
				bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_WIN_SIZE, 11);
				ovl_win_size = 12;
			} else {
				bw_monitor_config |= REG_FLD_VAL(FLD_OVL_BURST_ACC_WIN_SIZE, 23);
				ovl_win_size = 24;
			}
		}
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_BURST_MON_CFG, bw_monitor_config, ~0);
	}
	mtk_ovl_golden_setting(comp, cfg, handle);
}

static void mtk_ovl_layer_on(struct mtk_ddp_comp *comp, unsigned int idx,
			     unsigned int ext_idx, struct cmdq_pkt *handle)
{
	unsigned int con;

	if (ext_idx != LYE_NORMAL) {
		unsigned int con_mask;

		con_mask =
			BIT(ext_idx - 1) | (0xFFFF << ((ext_idx - 1) * 4 + 16));
		con = BIT(ext_idx - 1) | (idx << ((ext_idx - 1) * 4 + 16));
		cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0x1, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
			       con, con_mask);
		return;
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0x1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_SRC_CON, BIT(idx),
		       BIT(idx));
}

static void mtk_ovl_layer_off(struct mtk_ddp_comp *comp, unsigned int idx,
			      unsigned int ext_idx, struct cmdq_pkt *handle)
{
	u32 wcg_mask = 0, wcg_value = 0, sel_value = 0, sel_mask = 0;

	if (ext_idx != LYE_NORMAL) {
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_IGAMMA_EN(ext_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_GAMMA_EN(ext_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_CSC_EN(ext_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, 0,
			     FLD_ELn_IGAMMA_SEL(ext_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, 0,
			     FLD_ELn_GAMMA_SEL(ext_idx - 1));
	} else {
		SET_VAL_MASK(wcg_value, wcg_mask, 0, FLD_Ln_IGAMMA_EN(idx));
		SET_VAL_MASK(wcg_value, wcg_mask, 0, FLD_Ln_GAMMA_EN(idx));
		SET_VAL_MASK(wcg_value, wcg_mask, 0, FLD_Ln_CSC_EN(idx));
		SET_VAL_MASK(sel_value, sel_mask, 0, FLD_Ln_IGAMMA_SEL(idx));
		SET_VAL_MASK(sel_value, sel_mask, 0, FLD_Ln_GAMMA_SEL(idx));
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG1, wcg_value,
		       wcg_mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG2, sel_value,
		       sel_mask);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG1, wcg_value,
		       wcg_mask);

	if (ext_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON, 0,
			       BIT(ext_idx - 1) | BIT(ext_idx + 3));
	else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON, 0,
			       BIT(idx + 4));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0,
			       BIT(idx));
		/* TODO: only disable RDMA with valid lye_blob information */
		//cmdq_pkt_write(handle, comp->cmdq_base,
		//	       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0,
		//	       ~0);
	}
}

static unsigned int ovl_fmt_convert(struct mtk_disp_ovl *ovl, unsigned int fmt,
				    uint64_t modifier, unsigned int compress)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return OVL_CON_CLRFMT_RGB565(ovl) | (compress ? OVL_CON_BYTE_SWAP : 0UL);
	case DRM_FORMAT_BGR565:
		return (unsigned int)OVL_CON_CLRFMT_RGB565(ovl) |
		       OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGB888:
		return OVL_CON_CLRFMT_RGB888(ovl);
	case DRM_FORMAT_BGR888:
		return (unsigned int)OVL_CON_CLRFMT_RGB888(ovl) |
		       OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
				OVL_CON_BYTE_SWAP | OVL_CON_RGB_SWAP;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP |
			       OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_Y410:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
			       OVL_CON_RGB_SWAP;
		else
			return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_UYVY:
		return OVL_CON_CLRFMT_UYVY(ovl);
	case DRM_FORMAT_YUYV:
		return OVL_CON_CLRFMT_YUYV(ovl);
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
			       OVL_CON_RGB_SWAP;
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_ABGR16161616F:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
			       OVL_CON_RGB_SWAP;
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_C8:
		return OVL_CON_CLRFMT_DIM | OVL_CON_CLRFMT_RGB888(ovl);
	}
}

static const char *mtk_ovl_get_transfer_str(enum mtk_ovl_transfer transfer)
{
	if (transfer < 0) {
		DDPPR_ERR("%s: Invalid ovl transfer:%d\n", __func__, transfer);
		transfer = 0;
	}

	return mtk_ovl_transfer_str[transfer];
}

static const char *
mtk_ovl_get_colorspace_str(enum mtk_ovl_colorspace colorspace)
{
	return mtk_ovl_colorspace_str[colorspace];
}

static enum mtk_ovl_colorspace mtk_ovl_map_cs(enum mtk_drm_dataspace ds)
{
	enum mtk_ovl_colorspace cs = OVL_SRGB;

	switch (ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3:
		cs = OVL_P3;
		break;
	case MTK_DRM_DATASPACE_STANDARD_ADOBE_RGB:
		DDPPR_ERR("%s: ovl get cs ADOBE_RGB\n", __func__);
		fallthrough;
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
		fallthrough;
	case MTK_DRM_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		DDPPR_ERR("%s: ovl does not support BT2020\n", __func__);
		fallthrough;
	default:
		cs = OVL_SRGB;
		break;
	}

	return cs;
}

static enum mtk_ovl_transfer mtk_ovl_map_transfer(enum mtk_drm_dataspace ds)
{
	enum mtk_ovl_transfer xfr = OVL_GAMMA_UNKNOWN;

	switch (ds & MTK_DRM_DATASPACE_TRANSFER_MASK) {
	case MTK_DRM_DATASPACE_TRANSFER_LINEAR:
		xfr = OVL_LINEAR;
		break;
	case MTK_DRM_DATASPACE_TRANSFER_GAMMA2_6:
		fallthrough;
	case MTK_DRM_DATASPACE_TRANSFER_GAMMA2_8:
		DDPINFO("%s: ovl does not support gamma 2.6/2.8, use gamma 2.2\n", __func__);
		fallthrough;
	case MTK_DRM_DATASPACE_TRANSFER_ST2084:
		fallthrough;
	case MTK_DRM_DATASPACE_TRANSFER_HLG:
		DDPINFO("%s: HDR transfer\n", __func__);
		fallthrough;
	default:
		xfr = OVL_GAMMA2_2;
		break;
	}

	return xfr;
}

static int mtk_ovl_do_transfer(unsigned int idx,
			       enum mtk_drm_dataspace plane_ds,
			       enum mtk_drm_dataspace lcm_ds, bool *gamma_en,
			       bool *igamma_en, u32 *gamma_sel, u32 *igamma_sel)
{
	enum mtk_ovl_transfer xfr_in = OVL_GAMMA2_2, xfr_out = OVL_GAMMA2_2;
	enum mtk_ovl_colorspace cs_in = OVL_CS_UNKNOWN, cs_out = OVL_CS_UNKNOWN;
	bool en = false;

	xfr_in = mtk_ovl_map_transfer(plane_ds);
	xfr_out = mtk_ovl_map_transfer(lcm_ds);
	cs_in = mtk_ovl_map_cs(plane_ds);
	cs_out = mtk_ovl_map_cs(lcm_ds);

	DDPDBG("%s+ idx:%d transfer:%s->%s\n", __func__, idx,
	       mtk_ovl_get_transfer_str(xfr_in),
	       mtk_ovl_get_transfer_str(xfr_out));

	en = (xfr_in != OVL_LINEAR);
	if (en) {
		*igamma_en = true;
		*igamma_sel = xfr_in;
	} else
		*igamma_en = false;

	en = xfr_out != OVL_LINEAR;
	if (en) {
		*gamma_en = true;
		*gamma_sel = xfr_out;
	} else
		*gamma_en = false;

	return 0;
}

static unsigned int mtk_crtc_WCG_by_color_mode(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = NULL;

	if (crtc && crtc->dev)
		priv = crtc->dev->dev_private;
	if (priv && mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG_BY_COLOR_MODE))
		return 1;
	DDPDBG("WCG by color mode off\n");
	return 0;
}

static unsigned int mtk_crtc_get_color_mode(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int ret = 0;

	if (crtc == NULL)
		return ret;

	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	if (mtk_crtc_state)
		ret = mtk_crtc_state->prop_val[CRTC_PROP_WCG_BY_COLOR_MODE];

	return ret;
}

static s32 *mtk_get_ovl_csc(enum mtk_ovl_colorspace in,
			    enum mtk_ovl_colorspace out, struct drm_crtc *crtc)
{
	static s32 *ovl_csc[OVL_CS_NUM][OVL_CS_NUM];
	static unsigned int inited = 0xffffffff;
	unsigned int i, j;
	int ovl_in = (in < OVL_CS_NUM) ? in : 0;
	int ovl_out = (out < OVL_CS_NUM) ? out : 0;

	if (out < 0) {
		DDPPR_ERR("%s: Invalid ovl colorspace in:%d\n", __func__, out);
		out = 0;
	}

	if (in < 0) {
		DDPPR_ERR("%s: Invalid ovl colorspace in:%d\n", __func__, in);
		in = 0;
	}


	if (mtk_crtc_WCG_by_color_mode(crtc)) {
		/* WCG by color mode */
		unsigned int color_mode = mtk_crtc_get_color_mode(crtc);

		if (inited == color_mode)
			goto done;

		for (i = 0; i < OVL_CS_NUM; i++)
			for (j = 0; j < OVL_CS_NUM; j++)
				ovl_csc[i][j] = identity;

		switch (color_mode) {
		case HAL_COLOR_MODE_DISPLAY_P3:
		case HAL_COLOR_MODE_DCI_P3:
			DDPDBG("WCG by color mode[%d], P3 mode\n", color_mode);
			ovl_csc[OVL_SRGB][OVL_SRGB] = sRGB_to_DCI_P3;
			ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_DCI_P3;
			break;
		case HAL_COLOR_MODE_SRGB:
			DDPDBG("WCG by color mode[%d], SRGB mode\n", color_mode);
			ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_sRGB;
			ovl_csc[OVL_P3][OVL_P3] = DCI_P3_to_sRGB;
			break;
		case HAL_COLOR_MODE_NATIVE:
		default:
			DDPDBG("WCG by color mode[%d], NATIVE mode\n", color_mode);
			/* default: HAL_COLOR_MODE_NATIVE */
			/* csc do nothing */
			break;
		}
		inited = color_mode;
	} else {
		if (inited == 1)
			goto done;

		for (i = 0; i < OVL_CS_NUM; i++)
			for (j = 0; j < OVL_CS_NUM; j++)
				ovl_csc[i][j] = identity;

		DDPDBG("original WCG mode\n");
		ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_DCI_P3;
		ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_sRGB;
		inited = 1;
	}

done:
	return ovl_csc[ovl_in][ovl_out];
}

static int mtk_ovl_do_csc(unsigned int idx, enum mtk_drm_dataspace plane_ds,
			  enum mtk_drm_dataspace lcm_ds, bool *csc_wcg_en,
			  s32 **csc, struct drm_crtc *crtc)
{
	enum mtk_ovl_colorspace in = OVL_SRGB, out = OVL_SRGB;
	bool en = false;

	in = mtk_ovl_map_cs(plane_ds);
	out = mtk_ovl_map_cs(lcm_ds);

	DDPDBG("%s+ idx:%d csc:%s->%s\n", __func__, idx,
	       mtk_ovl_get_colorspace_str(in), mtk_ovl_get_colorspace_str(out));
	if (mtk_crtc_WCG_by_color_mode(crtc))
		en = 1;
	else
		en = in != out;

	if (en)
		*csc_wcg_en = true;
	else
		*csc_wcg_en = false;

	if (!en)
		return 0;
	if (!csc) {
		DDPPR_ERR("%s+ invalid csc\n", __func__);
		return 0;
	}

	*csc = mtk_get_ovl_csc(in, out, crtc);
	if (!(*csc)) {
		DDPPR_ERR("%s+ idx:%d no ovl csc %s to %s, disable csc\n",
			  __func__, idx, mtk_ovl_get_colorspace_str(in),
			  mtk_ovl_get_colorspace_str(out));
		*csc_wcg_en = false;
	}

	return 0;
}

static enum mtk_drm_dataspace
mtk_ovl_map_lcm_color_mode(enum mtk_drm_color_mode cm)
{
	enum mtk_drm_dataspace ds = MTK_DRM_DATASPACE_SRGB;

	switch (cm) {
	case MTK_DRM_COLOR_MODE_DISPLAY_P3:
		ds = MTK_DRM_DATASPACE_DISPLAY_P3;
		break;
	default:
		ds = MTK_DRM_DATASPACE_SRGB;
		break;
	}

	return ds;
}

/* WCG must on first                 */
/* customization condition, from HWC */
/* return default is 0 ==> WCG on    */
static unsigned int mtk_crtc_dynamic_WCG_off(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int ret = 0;

	if (crtc == NULL)
		return ret;

	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	if (mtk_crtc_state)
		ret = mtk_crtc_state->prop_val[CRTC_PROP_DYNAMIC_WCG_OFF];

	return ret;
}

/* we want WCG first ==> bri ==> CT                              */
/* for combination:                                              */
/* (CT 4x4 matrix) x (bri 4x4 matrix) x (WCG 4x4 matrix) x (RGB) */
void mtk_ovl_csc_combination(bool csc_wcg_en, unsigned int ocfbn, s32 *csc,
	struct mtk_crtc_ovl_csc_config *occ, s32 *csc_final)
{
	unsigned int i, j;
	long long csc_final_temp[16] = {0};

	/* debug log */
	if (csc) {
		for (i = 0; i < 3; i++)
			DDPDBG("WCG<%d> = <%d, %d, %d>\n", i, *(csc + i * 3 + 0),
				*(csc + i * 3 + 1), *(csc + i * 3 + 2));
	}
	for (i = 0; i < 4; i++)
		DDPDBG("brightness<%d> = <%d, %d, %d, %d>\n", i,
			occ->setbrightness[i * 4 + 0], occ->setbrightness[i * 4 + 1],
			occ->setbrightness[i * 4 + 2], occ->setbrightness[i * 4 + 3]);
	for (i = 0; i < 4; i++)
		DDPDBG("setcolortransform<%d> = <%d, %d, %d, %d>\n", i,
			occ->setcolortransform[i * 4 + 0], occ->setcolortransform[i * 4 + 1],
			occ->setcolortransform[i * 4 + 2], occ->setcolortransform[i * 4 + 3]);

	if (csc_wcg_en && csc) {
		/* WCG csc */
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				*(csc_final + i * 4 + j) = *(csc + i * 3 + j);
	} else {
		/* no WCG, set relay matrix */
		*(csc_final + 0 * 4 + 0) = 1 << ocfbn;
		*(csc_final + 1 * 4 + 1) = 1 << ocfbn;
		*(csc_final + 2 * 4 + 2) = 1 << ocfbn;
	}
	/* for offset csc_combination */
	*(csc_final + 15) = 1 << ocfbn;

	/* debug log */
	for (i = 0; i < 4; i++)
		DDPDBG("csc_final_WCG<%d> = <%d, %d, %d, %d>\n", i, csc_final[i * 4 + 0],
			csc_final[i * 4 + 1], csc_final[i * 4 + 2], csc_final[i * 4 + 3]);

	if (occ) {
		/* brightness para */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++) {
				csc_final_temp[i * 4 + j] =
				(long long)(occ->setbrightness[i * 4 + 0]) *
					(long long)*(csc_final + 0 * 4 + j) +
				(long long)(occ->setbrightness[i * 4 + 1]) *
					(long long)*(csc_final + 1 * 4 + j) +
				(long long)(occ->setbrightness[i * 4 + 2]) *
					(long long)*(csc_final + 2 * 4 + j) +
				(long long)(occ->setbrightness[i * 4 + 3]) *
					(long long)*(csc_final + 3 * 4 + j);
			}
		/* copy 4x4 back */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				if (csc_final_temp[i * 4 + j] < 0)
					csc_final_temp[i * 4 + j] =
						(csc_final_temp[i * 4 + j] >> ocfbn) |
						(0xffffffffffffffff << (64 - ocfbn));
				else
					csc_final_temp[i * 4 + j] =
						csc_final_temp[i * 4 + j] >> ocfbn;
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				*(csc_final + i * 4 + j) = csc_final_temp[i * 4 + j];

		/* debug log */
		for (i = 0; i < 4; i++)
			DDPDBG("csc_final_bri<%d> = <%d, %d, %d, %d>\n", i,
				csc_final[i * 4 + 0], csc_final[i * 4 + 1],
				csc_final[i * 4 + 2], csc_final[i * 4 + 3]);

		/* CT para */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++) {
				csc_final_temp[i * 4 + j] =
				(long long)(occ->setcolortransform[i * 4 + 0]) *
					(long long)*(csc_final + 0 * 4 + j) +
				(long long)(occ->setcolortransform[i * 4 + 1]) *
					(long long)*(csc_final + 1 * 4 + j) +
				(long long)(occ->setcolortransform[i * 4 + 2]) *
					(long long)*(csc_final + 2 * 4 + j) +
				(long long)(occ->setcolortransform[i * 4 + 3]) *
					(long long)*(csc_final + 3 * 4 + j);
			}
		/* copy 4x4 back */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				if (csc_final_temp[i * 4 + j] < 0)
					csc_final_temp[i * 4 + j] =
						(csc_final_temp[i * 4 + j] >> ocfbn) |
						(0xffffffffffffffff << (64 - ocfbn));
				else
					csc_final_temp[i * 4 + j] =
						csc_final_temp[i * 4 + j] >> ocfbn;
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				*(csc_final + i * 4 + j) = csc_final_temp[i * 4 + j];

		/* debug log */
		for (i = 0; i < 4; i++)
			DDPDBG("csc_final_CT<%d> = <%d, %d, %d, %d>\n", i,
				csc_final[i * 4 + 0], csc_final[i * 4 + 1],
				csc_final[i * 4 + 2], csc_final[i * 4 + 3]);
	}
}

void mtk_ovl_swap(int *a, int *b)
{
	int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void mtk_ovl_transpose(int *matrix, int size)
{
	int i, j;

	for (i = 0; i < size; i++)
		for (j = 0; j < i; j++)
			if (i != j)
				mtk_ovl_swap(matrix + i * size + j, matrix + j * size + i);
}

void mtk_ovl_get_ovl_csc_data(struct drm_crtc *crtc,
	struct mtk_plane_state *mtk_plane_state, int *csc_bc_en,
	struct mtk_crtc_ovl_csc_config *occ)
{
	int blob_id;
	struct drm_property_blob *blob1 = NULL;
	struct drm_property_blob *blob2 = NULL;

	/* get brightness 4x4 by blob_id */
	blob_id = mtk_plane_state->prop_val[PLANE_PROP_OVL_CSC_SET_BRIGHTNESS];
	if (blob_id) {
		blob1 = drm_property_lookup_blob(crtc->dev, blob_id);
		if (blob1 && blob1->data && blob1->length ==
			(sizeof(int) * 16)/* 4x4 matrix */) {
			memcpy(occ->setbrightness, blob1->data, blob1->length);
			mtk_ovl_transpose(occ->setbrightness, 4);
			*csc_bc_en = 1;
		} else {
			DDPINFO("Cannot get ovl_csc_config: SET_BRIGHTNESS, blob: %d!\n",
										blob_id);
			if (blob1)
				DDPINFO("size of blob1->data is %lu\n", blob1->length);
		}

		if (blob1)
			drm_property_blob_put(blob1);
	}

	/* get colortransform 4x4 by blob_id */
	blob_id = mtk_plane_state->prop_val[PLANE_PROP_OVL_CSC_SET_COLORTRANSFORM];
	if (blob_id) {
		blob2 = drm_property_lookup_blob(crtc->dev, blob_id);
		if (blob2 && blob2->data && blob2->length ==
			(sizeof(int) * 16)/* 4x4 matrix */) {
			memcpy(occ->setcolortransform, blob2->data, blob2->length);
			mtk_ovl_transpose(occ->setcolortransform, 4);
			*csc_bc_en = 1;
		} else {
			DDPINFO("Cannot get ovl_csc_config: SET_COLORTRANSFORM, blob: %d!\n",
										blob_id);
			if (blob2)
				DDPINFO("size of blob2->data is %lu\n", blob2->length);
		}
		if (blob2)
			drm_property_blob_put(blob2);
	}
}

static int mtk_ovl_color_manage(struct mtk_ddp_comp *comp, unsigned int idx,
			struct mtk_plane_state *state, struct cmdq_pkt *handle)
{
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	struct mtk_plane_pending_state *pending = &state->pending;
	struct drm_crtc *crtc = state->crtc;
	struct mtk_drm_private *priv;
	bool gamma_en = false, igamma_en = false, csc_wcg_en = false;
	u32 gamma_sel = 0, igamma_sel = 0;
	s32 *csc = NULL;
	u32 wcg_mask = 0, wcg_value = 0, sel_mask = 0, sel_value = 0, reg = 0;
	enum mtk_drm_color_mode lcm_cm;
	enum mtk_drm_dataspace lcm_ds = 0, plane_ds = 0;
	struct mtk_panel_params *params;
	int i;
	int csc_bc_en = 0; /* csc setbright & setcolortransform enable */
	int csc_bc_support = 0; /* csc setbright & setcolortransform support or not */
	unsigned int ocfbn = 0;
	struct mtk_crtc_ovl_csc_config occ = {0};
	s32 csc_final[16] = {0}; /* 4x4 matrix */
	static s32 csc_tmp;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (!crtc)
		goto done;

	/* init */
	if (comp && comp->mtk_crtc) /* get ocfbn */
		ocfbn = comp->mtk_crtc->crtc_caps.ovl_csc_bit_number;
	if (ocfbn > 0)
		csc_bc_support = 1;
	for (i = 0; i < 4; i++) { /* set matrix identity */
		occ.setbrightness[i * 4 + i] = 1 << ocfbn;
		occ.setcolortransform[i * 4 + i] = 1 << ocfbn;
		csc_final[i * 4 + i] = 1 << ocfbn;
	}

	/* get brightness/layercolortransform csc */
	if (csc_bc_support)
		mtk_ovl_get_ovl_csc_data(crtc, state, &csc_bc_en, &occ);

	priv = crtc->dev->dev_private;
	if ((mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) &&
		(mtk_crtc_dynamic_WCG_off(crtc) == 0) &&
		pending->enable) ||	/* WCG condition */
	    csc_bc_en) {	/* ovl csc condition */
		params = mtk_drm_get_lcm_ext_params(crtc);
		if (params)
			lcm_cm = params->lcm_color_mode;
		else
			lcm_cm = MTK_DRM_COLOR_MODE_NATIVE;

		lcm_ds = mtk_ovl_map_lcm_color_mode(lcm_cm);
		plane_ds =
			(enum mtk_drm_dataspace)pending->prop_val[PLANE_PROP_DATASPACE];
		DDPDBG("%s+ idx:%d ds:0x%08x->0x%08x\n", __func__, idx, plane_ds,
		       lcm_ds);

		mtk_ovl_do_transfer(idx, plane_ds, lcm_ds, &gamma_en, &igamma_en,
				    &gamma_sel, &igamma_sel);

		/* get WCG CSC */
		if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) &&
			(mtk_crtc_dynamic_WCG_off(crtc) == 0) &&
			pending->enable)
			mtk_ovl_do_csc(idx, plane_ds, lcm_ds, &csc_wcg_en, &csc, crtc);
	}
	DDPDBG("%s, g, ig, gs, igs <%d><%d><%d><%d>\n",
		__func__, gamma_en, igamma_en, gamma_sel, igamma_sel);

	DDPDBG("%s, csc_wcg_en, csc_bc_en <%d><%d>\n",
		__func__, csc_wcg_en, csc_bc_en);

	/* csc combination */
	if (csc_wcg_en || csc_bc_en)
		mtk_ovl_csc_combination(csc_wcg_en, ocfbn,
			csc, &occ, csc_final);

done:
	if (ext_lye_idx != LYE_NORMAL) {
		SET_VAL_MASK(wcg_value, wcg_mask, igamma_en,
			     FLD_ELn_IGAMMA_EN(ext_lye_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, gamma_en,
			     FLD_ELn_GAMMA_EN(ext_lye_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, ((csc_wcg_en || csc_bc_en) ? 1 : 0),
			     FLD_ELn_CSC_EN(ext_lye_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, igamma_sel,
			     FLD_ELn_IGAMMA_SEL(ext_lye_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, gamma_sel,
			     FLD_ELn_GAMMA_SEL(ext_lye_idx - 1));
	} else {
		SET_VAL_MASK(wcg_value, wcg_mask, igamma_en,
			     FLD_Ln_IGAMMA_EN(lye_idx));
		SET_VAL_MASK(wcg_value, wcg_mask, gamma_en,
			     FLD_Ln_GAMMA_EN(lye_idx));
		SET_VAL_MASK(wcg_value, wcg_mask, ((csc_wcg_en || csc_bc_en) ? 1 : 0),
			     FLD_Ln_CSC_EN(lye_idx));
		SET_VAL_MASK(sel_value, sel_mask, igamma_sel,
			     FLD_Ln_IGAMMA_SEL(lye_idx));
		SET_VAL_MASK(sel_value, sel_mask, gamma_sel,
			     FLD_Ln_GAMMA_SEL(lye_idx));
	}

	DDPDBG("%s, lye_idx%d,ext_lye_idx%d,csc_wcg_en%d,ovl_csc_en%d,wcg_value0x%x,sel_value0x%x\n",
		__func__, lye_idx, ext_lye_idx, csc_wcg_en, csc_bc_en, wcg_value, sel_value);
	DDPDBG("%s, WCG Dymanic off = %d, WCG by color mode[%d][%d]\n", __func__,
		mtk_crtc_dynamic_WCG_off(crtc),
		mtk_crtc_WCG_by_color_mode(crtc),
		mtk_crtc_get_color_mode(crtc));

	/* enable, gamma, igamma */
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG1, wcg_value,
		       wcg_mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG2, sel_value,
		       sel_mask);

	if (ext_lye_idx != LYE_NORMAL)
		reg = DISP_REG_OVL_ELn_R2R_PARA(ext_lye_idx - 1);
	else
		reg = DISP_REG_OVL_Ln_R2R_PARA(lye_idx);

	/* 3x3 write reg */
	for (i = 0; i < CSC_COEF_NUM; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + reg + 4 * i, csc_final[((i / 3) * 4) + i % 3], ~0);

	/* offset write reg */
	/* offset in coda is s12.6 for u8, but value from HWC is s5.18 and normalize value */
	for (i = 0; i < 3; i++)
		if (csc_final[i * 4 + 3] < 0)
			csc_final[i * 4 + 3] = csc_final[i * 4 + 3] >> 4 | 0xf0000000;
		else
			csc_final[i * 4 + 3] = csc_final[i * 4 + 3] >> 4;

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + reg + 4 * 12, csc_final[3], ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + reg + 4 * 13, csc_final[7], ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + reg + 4 * 14, csc_final[11], ~0);

	if (csc_tmp != csc_final[0]) {
		csc_tmp = csc_final[0];
		CRTC_MMP_MARK(0, csc_bl, csc_tmp, 0);
	}

	return 0;
}

static int mtk_ovl_yuv_matrix_convert(enum mtk_drm_dataspace plane_ds)
{
	int ret = 0;

	//P3 Must align AOSP use BT601 FULL to do Y2R convert
	switch (plane_ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_BT601_625:
	case MTK_DRM_DATASPACE_STANDARD_BT601_625_UNADJUSTED:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525_UNADJUSTED:
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3:
		switch (plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) {
		case MTK_DRM_DATASPACE_RANGE_UNSPECIFIED:
		case MTK_DRM_DATASPACE_RANGE_LIMITED:
			ret = OVL_CON_MTX_BT601_TO_RGB;
			break;
		default:
			ret = OVL_CON_MTX_JPEG_TO_RGB;
			break;
		}
		break;
	case MTK_DRM_DATASPACE_STANDARD_BT709:
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
	case MTK_DRM_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		switch (plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) {
		case MTK_DRM_DATASPACE_RANGE_FULL:
			ret = OVL_CON_MTX_BT709_FULL_RANGE_TO_RGB;
			break;
		default:
			ret = OVL_CON_MTX_BT709_TO_RGB;
			break;
		}
		break;

	case 0:
		switch (plane_ds & 0xffff) {
		case MTK_DRM_DATASPACE_JFIF:
		case MTK_DRM_DATASPACE_BT601_625:
		case MTK_DRM_DATASPACE_BT601_525:
			ret = OVL_CON_MTX_BT601_TO_RGB;
			break;

		case MTK_DRM_DATASPACE_SRGB_LINEAR:
		case MTK_DRM_DATASPACE_SRGB:
		case MTK_DRM_DATASPACE_BT709:
			ret = OVL_CON_MTX_BT709_TO_RGB;
			break;
		}
	}

	if (ret)
		return ret;

	return OVL_CON_MTX_BT601_TO_RGB;
}

static void write_phy_layer_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_ADDR(ovl, id),
		       addr, ~0);

	if (ovl->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ADDR_MSB(id),
			       DO_SHIFT_RIGHT(addr, 32), 0xf);
}

static void write_ext_layer_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_ADDR(ovl, id),
			addr, ~0);

	if (ovl->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_ADDR_MSB(id),
			        DO_SHIFT_RIGHT(addr, 32), 0xf);
}

static void write_phy_layer_hdr_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_LX_HDR_ADDR(id),
			addr, ~0);

	if (ovl->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ADDR_MSB(id),
			       (DO_SHIFT_RIGHT(addr, 32) << 8), 0xf00);
}

static void write_ext_layer_hdr_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_ELX_HDR_ADDR(ovl, id),
			addr, ~0);

	if (ovl->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_ADDR_MSB(id),
			       (addr >> 24), 0xf00);
}

static void write_sec_phy_layer_addr_cmdq(struct mtk_ddp_comp *comp,
			struct cmdq_pkt *handle, u32 id, dma_addr_t addr,
			unsigned int offset, unsigned int buf_size)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (disp_mtee_cb.cb != NULL)
		disp_mtee_cb.cb(DISP_SEC_ENABLE, 0, NULL, handle, comp, 0,
				comp->regs_pa + DISP_REG_OVL_ADDR(ovl, id),
				addr, offset, buf_size);
}

static void write_sec_ext_layer_addr_cmdq(struct mtk_ddp_comp *comp,
			struct cmdq_pkt *handle, u32 id, dma_addr_t addr,
			unsigned int offset, unsigned int buf_size)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (disp_mtee_cb.cb != NULL)
		disp_mtee_cb.cb(DISP_SEC_ENABLE, 0, NULL, handle, comp, 0,
				comp->regs_pa + DISP_REG_OVL_EL_ADDR(ovl, id),
				addr, offset, buf_size);
}

static void write_sec_phy_layer_hdr_addr_cmdq(struct mtk_ddp_comp *comp,
			struct cmdq_pkt *handle, u32 id, dma_addr_t addr,
			unsigned int offset, unsigned int buf_size)
{
	//struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (disp_mtee_cb.cb != NULL)
		disp_mtee_cb.cb(DISP_SEC_ENABLE, 0, NULL, handle, comp, 0,
			comp->regs_pa + DISP_REG_OVL_LX_HDR_ADDR(id),
			addr, offset, buf_size);
}

static void write_sec_ext_layer_hdr_addr_cmdq(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, u32 id, dma_addr_t addr,
				unsigned int offset, unsigned int buf_size)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (disp_mtee_cb.cb != NULL)
		disp_mtee_cb.cb(DISP_SEC_ENABLE, 0, NULL, handle, comp, 0,
			comp->regs_pa + DISP_REG_OVL_ELX_HDR_ADDR(ovl, id),
			addr, offset, buf_size);
}

static void set_sec_phy_layer_dom_cmdq(struct mtk_ddp_comp *comp,
					struct cmdq_pkt *handle, u32 id)
{
	u32 domain_val = 0, domain_mask = 0;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6768 || priv->data->mmsys_id == MMSYS_MT6765) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				BIT(id), BIT(id));
	} else {
		SET_VAL_MASK(domain_val, domain_mask,
				 OVL_LAYER_SVP_DOMAIN_INDEX, OVL_LAYER_Lx_DOMAIN(id));
		cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_LAYER_DOMAIN,
					domain_val, domain_mask);
		DDPINFO("%s:%d,L%dSet dom(0x%llx,0x%x,0x%x)\n",
					__func__, __LINE__, id,
					comp->regs_pa + OVL_LAYER_DOMAIN,
					domain_val, domain_mask);
	}
}

static void set_sec_ext_layer_dom_cmdq(struct mtk_ddp_comp *comp,
					struct cmdq_pkt *handle, u32 id)
{
	u32 domain_val = 0, domain_mask = 0;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6768 || priv->data->mmsys_id == MMSYS_MT6765) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				BIT(id + EXT_SECURE_OFFSET),
				BIT(id + EXT_SECURE_OFFSET));
	} else {
		SET_VAL_MASK(domain_val, domain_mask,
				OVL_LAYER_SVP_DOMAIN_INDEX, OVL_LAYER_ELx_DOMAIN(id));
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_LAYER_EXT_DOMAIN,
				domain_val, domain_mask);
	}
}

static void clr_sec_phy_layer_dom_cmdq(struct mtk_ddp_comp *comp,
					struct cmdq_pkt *handle, u32 id)
{
	u32 domain_val = 0, domain_mask = 0;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6768 || priv->data->mmsys_id == MMSYS_MT6765) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(id));
	} else {
		SET_VAL_MASK(domain_val, domain_mask,
				0, OVL_LAYER_Lx_DOMAIN(id));
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_LAYER_DOMAIN,
				domain_val, domain_mask);
		DDPINFO("%s:%d,L%d clr dom(0x%llx,0x%x,0x%x)\n",
			__func__, __LINE__, id,
			comp->regs_pa + OVL_LAYER_DOMAIN,
			domain_val, domain_mask);
	}
}

static void clr_sec_ext_layer_dom_cmdq(struct mtk_ddp_comp *comp,
					struct cmdq_pkt *handle, u32 id)
{
	u32 domain_val = 0, domain_mask = 0;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6768 || priv->data->mmsys_id == MMSYS_MT6765) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(id + EXT_SECURE_OFFSET));
	} else {
		SET_VAL_MASK(domain_val, domain_mask,
				0, OVL_LAYER_ELx_DOMAIN(id));
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_LAYER_EXT_DOMAIN,
				domain_val, domain_mask);
		DDPINFO("%s:%d,L%d clr dom(0x%llx,0x%x,0x%x)\n",
			__func__, __LINE__, id,
			comp->regs_pa + OVL_LAYER_EXT_DOMAIN,
			domain_val, domain_mask);
	}
}

/* config addr, pitch, src_size */
static void _ovl_common_config(struct mtk_ddp_comp *comp, unsigned int idx,
			       struct mtk_plane_state *state,
			       struct cmdq_pkt *handle)
{
	struct mtk_plane_pending_state *pending = NULL;
	unsigned int fmt = 0;
	unsigned int pitch = 0;
	unsigned int pitch_msb = 0;
	unsigned int dst_h = 0;
	unsigned int dst_w = 0;
	unsigned int src_x = 0;
	unsigned int src_y = 0;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int src_size = 0;
	unsigned int offset = 0;
	unsigned int clip = 0;
	unsigned int buf_size = 0;
	int rotate = 0;
	struct mtk_disp_ovl *ovl = NULL;
	struct mtk_panel_params *params = NULL;
	unsigned int aid_sel_offset = 0;
	resource_size_t mmsys_reg = 0;
	int sec_bit;

	if (!comp || !state) {
		DDPPR_ERR("%s:%d NULL Pointer\n", __func__, __LINE__);
		return;
	}
	pending = &state->pending;
	fmt = pending->format;
	pitch = pending->pitch & 0xffff;
	pitch_msb = ((pending->pitch >> 16) & 0xf);
	dst_h = pending->height;
	dst_w = pending->width;
	src_x = pending->src_x;
	src_y = pending->src_y;

	ovl = comp_to_ovl(comp);

	if (fmt == DRM_FORMAT_YUYV || fmt == DRM_FORMAT_YVYU ||
	    fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_VYUY) {
		if (src_x % 2) {
			src_x -= 1;
			dst_w += 1;
			clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT, 1);
		}
		if ((src_x + dst_w) % 2) {
			dst_w += 1;
			clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT, 1);
		}
	}

	src_size = (dst_h << 16) | dst_w;

	if (!comp->mtk_crtc) {
		DDPPR_ERR("%s:%d NULL Pointer\n", __func__, __LINE__);
		return;
	}

	params = mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
	if (params && params->rotate == MTK_PANEL_ROTATE_180)
		if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
			rotate = 1;
#ifdef MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif
	if (rotate)
		offset = (src_x + dst_w) * mtk_drm_format_plane_cpp(fmt, 0) +
			 (src_y + dst_h - 1) * pitch - 1;
	else
		offset = src_x * mtk_drm_format_plane_cpp(fmt, 0) + src_y * pitch;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	buf_size = (dst_h - 1) * pending->pitch +
		dst_w * mtk_drm_format_plane_cpp(fmt, 0);

	if (ovl->data->mmsys_mapping)
		mmsys_reg = ovl->data->mmsys_mapping(comp);

	if (ovl->data->aid_sel_mapping)
		aid_sel_offset = ovl->data->aid_sel_mapping(comp);

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_PITCH_MSB(id),
			pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_PITCH(id),
			pitch, ~0);

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_aid_bit(comp, true, id);
			if (ovl->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		} else {
			/*legacy secure flow, for mt6789*/
			if (comp->mtk_crtc && comp->mtk_crtc->sec_on) {
				if (state->pending.is_sec && pending->addr) {
					write_sec_ext_layer_addr_cmdq(comp, handle, id,
								pending->addr, offset, buf_size);
					set_sec_ext_layer_dom_cmdq(comp, handle, id);
					goto legacy_sec1;
				} else
					clr_sec_ext_layer_dom_cmdq(comp, handle, id);
			} else
				clr_sec_ext_layer_dom_cmdq(comp, handle, id);
		}

		write_ext_layer_addr_cmdq(comp, handle, id, pending->addr + offset);
legacy_sec1:
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_SRC_SIZE(id),
			src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_CLIP(id), clip,
			~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH_MSB(lye_idx),
			pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH(lye_idx),
			pitch, ~0);

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_aid_bit(comp, false, lye_idx);
			if (ovl->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		} else  {
			if (comp->mtk_crtc->sec_on) {
				DDPMSG("enter svp2----\n");
				if (state->pending.is_sec && pending->addr && (dst_h >= 1)) {
					write_sec_phy_layer_addr_cmdq(comp, handle, lye_idx,
								pending->addr, offset, buf_size);
					set_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
					goto legacy_sec2;
				} else
					clr_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
			} else
				clr_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
		}

		if (pending->mml_mode == MML_MODE_RACING) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_SYSRAM_CFG(lye_idx), 1,
				~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_SYSRAM_BUF0_ADDR(lye_idx),
				pending->addr + offset, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_SYSRAM_BUF1_ADDR(lye_idx),
				pending->addr + offset + MML_SRAM_SHIFT,
				~0);

			/* setting SMI for read SRAM */
			if (comp->larb_cons)
				cmdq_pkt_write(handle, comp->cmdq_base, comp->larb_cons[lye_idx],
					       GENMASK(19, 16), GENMASK(19, 16));
			else
				DDPPR_ERR("%s: comp %d larb_cons is null\n", __func__, comp->id);
		}

		write_phy_layer_addr_cmdq(comp, handle, lye_idx, pending->addr + offset);

legacy_sec2:
		if (ovl->data->support_pq_selfloop && (pending->pq_loop_type == 2)) {
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				       DISP_REG_OVL_PQ_OUT_SIZE, src_size, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				       DISP_REG_OVL_SRC_SIZE(lye_idx), pending->dst_roi, ~0);
		} else
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				       DISP_REG_OVL_SRC_SIZE(lye_idx), src_size, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_CLIP(lye_idx), clip,
			~0);
	}
}

static void mtk_ovl_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	int rotate = 0;
	unsigned int fmt = pending->format;
	unsigned int offset;
	unsigned int con;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int alpha;
	unsigned int alpha_con = 1;
	unsigned int value = 0, mask = 0, fmt_ex = 0;
	unsigned long long temp_bw, temp_peak_bw;
	unsigned int dim_color;
	struct mtk_panel_params *params = NULL;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int frame_idx;
	unsigned long alloc_id = state->prop_val[PLANE_PROP_BUFFER_ALLOC_ID];
	unsigned int avg_ratio = 0;
	struct mtk_drm_private *priv;
	unsigned long crtc_idx;
	int i = 0;
	unsigned int disp_reg_ovl_pitch = 0;
	unsigned int pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	unsigned int modifier = 0;

	/* OVL comp might not attach to CRTC in layer_config(), need to check */
	if (unlikely(!comp->mtk_crtc)) {
		DDPPR_ERR("%s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	frame_idx = mtk_crtc_state->prop_val[CRTC_PROP_OVL_DSI_SEQ];
	priv = crtc->dev->dev_private;
	crtc_idx = (unsigned long)drm_crtc_index(crtc);

	/* handle dim layer for compression flag & color dim*/
	if (fmt == DRM_FORMAT_C8) {
		pending->prop_val[PLANE_PROP_COMPRESS] = 0;
		dim_color = (unsigned int)pending->prop_val[PLANE_PROP_DIM_COLOR];
	} else {
		dim_color = 0xff000000;
	}

	/* Replace drm_framebuffer members that are not used in mtk_ddp_comp(OVL), */
	/* Replace with the drm_framebuffer member in drm_plane_state, */
	/* Because the drm_framebuffer in drm_plane_state can record the sec_id. */
	comp->fb = state->base.fb;
	/* handle buffer de-compression */
	if (ovl->data->compr_info && ovl->data->compr_info->l_config) {
		if (ovl->data->compr_info->l_config(comp,
			    idx, state, handle)) {
			DDPPR_ERR("wrong fbdc input config\n");
			return;
		}
	} else {
		/* Config common register which would be different according
		 * with
		 * this layer is compressed or not, i.e.: addr, pitch...
		 */
		_ovl_common_config(comp, idx, state, handle);
	}

	if (comp->mtk_crtc)
		params = mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
	if (params && params->rotate == MTK_PANEL_ROTATE_180)
		if (drm_crtc_index(crtc) == 0)
			rotate = 1;
#ifdef MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif
	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (fmt == DRM_FORMAT_RGB332) {
		pending->enable = false;
		DDPINFO("%s: DRM_FORMAT_RGB332 not support, so skip it\n", __func__);
	}
	if (ovl->ovl_dis == true && pending->enable == true) {
		if ((priv->data->mmsys_id == MMSYS_MT6985 ||
			priv->data->mmsys_id == MMSYS_MT6989 ||
			priv->data->mmsys_id == MMSYS_MT6899) &&
			mtk_crtc_is_frame_trigger_mode(crtc))
			pending->enable = false;

		DDPPR_ERR("%s, %s, idx:%d, lye_idx:%d, ext_idx:%d, en:%d\n",
			__func__, mtk_dump_comp_str_id(comp->id), idx, lye_idx,
			ext_lye_idx, pending->enable);
		ovl->ovl_dis = false;
	}

	if (!pending->enable)
		mtk_ovl_layer_off(comp, lye_idx, ext_lye_idx, handle);

	mtk_ovl_color_manage(comp, idx, state, handle);

	alpha = 0xFF & (state->base.alpha >> 8);

	DDPINFO("Blending: state->base.alpha =0x%x, alpha = 0x%x\n", state->base.alpha, alpha);
	if (state->base.fb) {
		if (state->base.fb->format->has_alpha) {
			pixel_blend_mode = state->base.pixel_blend_mode;
			DDPINFO("Blending:real alpha exist X mode(%x)\n", fmt);
		}
		DDPINFO("Blending: has_alpha %d pixel_blend_mode=0x%x\n",
			state->base.fb->format->has_alpha, state->base.pixel_blend_mode);
	}

	if (pixel_blend_mode == DRM_MODE_BLEND_PREMULTI)
		modifier |= MTK_FMT_PREMULTIPLIED;

	con = ovl_fmt_convert(ovl, fmt, modifier,
			pending->prop_val[PLANE_PROP_COMPRESS]);
	con |= (alpha_con << 8) | alpha;

	if (fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_YUYV) {
		unsigned int prop = (unsigned int)pending->prop_val[PLANE_PROP_DATASPACE];

		con |= mtk_ovl_yuv_matrix_convert((enum mtk_drm_dataspace)prop);
	} else if (fmt == DRM_FORMAT_Y410) {
		DDPDBG("%s: DRM_FORMAT_Y410, dataspace set as BT601_FULL\n", __func__);
		con |= mtk_ovl_yuv_matrix_convert(MTK_DRM_DATASPACE_V0_JFIF);
	}

	if (!pending->addr && pending->pq_loop_type == 0)
		con |= CON_LSRC_COLOR;
	else if (ovl->data->support_pq_selfloop && (pending->pq_loop_type != 0))
		con |= CON_LSRC_PQ;

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_bg_roi(comp, &bg_h, &bg_w);
		offset = ((bg_h - pending->height - pending->dst_y) << 16) +
			 (bg_w - pending->width - pending->dst_x);
		DDPINFO("bg(%d,%d) (%d,%d,%dx%d)\n", bg_w, bg_h, pending->dst_x,
			pending->dst_y, pending->width, pending->height);
		con |= (CON_HORI_FLIP + CON_VERTICAL_FLIP);
	} else {
		offset = pending->offset;
	}

	if (fmt == DRM_FORMAT_ABGR2101010 || fmt == DRM_FORMAT_XBGR2101010)
		fmt_ex = 1;
	else if (fmt == DRM_FORMAT_ABGR16161616F)
		fmt_ex = 3;

	if ((pending->mml_mode == MML_MODE_RACING) && mtk_crtc->is_force_mml_scen &&
	    !ovl->data->support_pq_selfloop &&
	    (comp->id == DDP_COMPONENT_OVL0_2L || comp->id == DDP_COMPONENT_OVL1_2L ||
	     comp->id == DDP_COMPONENT_OVL2_2L || comp->id == DDP_COMPONENT_OVL3_2L)) {
		con |= REG_FLD_VAL(L_CON_FLD_MTX_AUTO_DIS, 1);
		con |= REG_FLD_VAL(L_CON_FLD_MTX_EN, 0);
		/* if pq selfloop is supported, use Y2R inside OVL instead */
	}

	if (fmt == DRM_FORMAT_Y410) {
		DDPDBG("%s: DRM_FORMAT_Y410, enable OVL Y2R\n", __func__);
		con |= REG_FLD_VAL(L_CON_FLD_MTX_AUTO_DIS, 1);
		con |= REG_FLD_VAL(L_CON_FLD_MTX_EN, 1);
		/* if format is DRM_FORMAT_Y410, enable Y2R inside OVL */
	}

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		SET_VAL_MASK(value, mask, fmt_ex, FLD_ELn_CLRFMT_NB(id));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CLRFMT_EXT, value,
			       mask);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_CON(id), con,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_OFFSET(id),
			       offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL0_CLR(id),
			       dim_color, ~0);

		disp_reg_ovl_pitch = DISP_REG_OVL_EL_PITCH(id);

		/* ext layer is the same as attached phy layer */
		if (!IS_ERR_OR_NULL(comp->qos_req_other) &&
			priv->data->mmsys_id != MMSYS_MT6989 &&
			priv->data->mmsys_id != MMSYS_MT6899) {
			int val = (lye_idx % 2);

			cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SMI_2ND_CFG,
			       (val << (id + 4)), (1 << (id + 4)));
		}
	} else {
		SET_VAL_MASK(value, mask, fmt_ex, FLD_Ln_CLRFMT_NB(lye_idx));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CLRFMT_EXT, value,
			       mask);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CON(lye_idx), con, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_OFFSET(lye_idx), offset, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_L0_CLR(lye_idx),
			       dim_color, ~0);

		disp_reg_ovl_pitch = DISP_REG_OVL_PITCH(lye_idx);

		/*
		 * layer0 --> larb0, layer1 --> larb1
		 * layer2 --> larb0, layer3 --> larb1
		 */
		if (!IS_ERR_OR_NULL(comp->qos_req_other) &&
			priv->data->mmsys_id != MMSYS_MT6989 &&
			priv->data->mmsys_id != MMSYS_MT6899) {
			int val = (lye_idx % 2);

			cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SMI_2ND_CFG,
			       (val << lye_idx), (1 << lye_idx));
		}
	}

	if (priv->data->mmsys_id == MMSYS_MT6989) {
		if (pending->enable) {//enable and not ext layer
			if (ext_lye_idx == 0)
				mtk_crtc->usage_ovl_fmt[(ovl->data->ovl_phy_mapping(comp) + lye_idx)] =
					mtk_get_format_bpp(fmt);
		} else {
			mtk_crtc->usage_ovl_fmt[(ovl->data->ovl_phy_mapping(comp) + lye_idx)] = 0;
		}
	}

#define _LAYER_CONFIG_FMT \
	"%s %s idx:%d lye_idx:%d ext_idx:%d en:%d fmt:0x%x " \
	"addr:0x%lx compr:%d con:0x%x offset:0x%x lye_cap:%x mml:%d\n"
	DDPINFO(_LAYER_CONFIG_FMT, __func__,
		mtk_dump_comp_str_id(comp->id), idx, lye_idx, ext_lye_idx,
		pending->enable, pending->format, (unsigned long)pending->addr,
		(unsigned int)pending->prop_val[PLANE_PROP_COMPRESS], con, offset,
		state->comp_state.layer_caps & (MTK_DISP_RSZ_LAYER | DISP_MML_CAPS_MASK),
		pending->mml_mode);

	DDPINFO("alpha= 0x%x, con=0x%x, blend = 0x%x, reg_ovl_pitch=0x%x\n",
		alpha,
		alpha_con,
		pixel_blend_mode,
		disp_reg_ovl_pitch);

	if (pixel_blend_mode == DRM_MODE_BLEND_PIXEL_NONE)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + disp_reg_ovl_pitch,
			DISP_OVL_LAYER_CONST_BLD,
			DISP_OVL_LAYER_CONST_BLD);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + disp_reg_ovl_pitch,
			0,
			DISP_OVL_LAYER_CONST_BLD);

	if (pending->enable) {
		u32 vrefresh;
		u32 ratio_tmp = 0;
		unsigned int vact = 0;
		unsigned int vtotal = 0;
		struct mtk_ddp_comp *output_comp;
		struct drm_display_mode *mode = NULL;

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
			vrefresh =
				mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
		else
			vrefresh = drm_mode_vrefresh(&crtc->state->adjusted_mode);

		/* query display mode anyway, would use it even in CMD mode */
		if (output_comp &&
		    ((output_comp->id == DDP_COMPONENT_DSI0) ||
		     (output_comp->id == DDP_COMPONENT_DSI1) ||
		     (output_comp->id == DDP_COMPONENT_DSI2)))
			mtk_ddp_comp_io_cmd(output_comp, NULL,
				DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
		if (mode && !(mtk_dsi_is_cmd_mode(output_comp))) {
			vtotal = mode->vtotal;
			vact = mode->vdisplay;
			ratio_tmp = vtotal * 100 / vact;
		} else
			ratio_tmp = 125;

		DDPDBG("%s, vrefresh=%d, ratio_tmp=%d\n",
			__func__, vrefresh, ratio_tmp);
		DDPDBG("%s, vtotal=%d, vact=%d\n",
			__func__, vtotal, vact);

		mtk_ovl_layer_on(comp, lye_idx, ext_lye_idx, handle);

		/*constant color :non RDMA source*/
		/* TODO: cause RPO abnormal */
//		if (!pending->addr)
//			cmdq_pkt_write(handle, comp->cmdq_base,
//		       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0x0, ~0);
		/* TODO: consider FBDC */
		/* SRT BW (one layer) =
		 * layer_w * layer_h * bpp * vrefresh * max fps blanking_ratio
		 * Sum_SRT(all layer) *= 1.33
		 */
		/* use full frame size's peak BW request bus capability, because tiny region layer */
		/* peak BW should be the same with full frame */
		temp_bw = (unsigned long long)pending->width * pending->height;
		temp_bw *= mtk_get_format_bpp(fmt);

		if (crtc->state)
			temp_peak_bw = (unsigned long long)crtc->state->adjusted_mode.vdisplay *
				crtc->state->adjusted_mode.hdisplay;
		else
			temp_peak_bw = (unsigned long long)pending->width * pending->height;
		temp_peak_bw *= mtk_get_format_bpp(fmt);

		/* COMPRESS ratio */
		if (pending->prop_val[PLANE_PROP_COMPRESS]) {
			temp_bw *= 7;
			do_div(temp_bw, 10);
		}
		do_div(temp_bw, 1000);
		temp_bw *= ratio_tmp;
		do_div(temp_bw, 100);
		temp_bw = temp_bw * vrefresh;
		do_div(temp_bw, 1000);

		do_div(temp_peak_bw, 1000);
		temp_peak_bw *= ratio_tmp;
		do_div(temp_peak_bw, 100);
		temp_peak_bw = temp_peak_bw * vrefresh;
		do_div(temp_peak_bw, 1000);

		DDPDBG("comp %d lye %u bw %llu peak %llu vtotal:%d vact:%d\n",
			comp->id, lye_idx, temp_bw, temp_peak_bw, vtotal, vact);

		if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR) &&
			(crtc_idx == 0) && (pending->prop_val[PLANE_PROP_COMPRESS]) &&
			(priv->data->mmsys_id != MMSYS_MT6989) &&
			((state->comp_state.layer_caps & MTK_HWC_UNCHANGED_LAYER) ||
			(state->comp_state.layer_caps & MTK_HWC_INACTIVE_LAYER) ||
			(state->comp_state.layer_caps & MTK_HWC_UNCHANGED_FBT_LAYER))) {
			uint64_t key = 0;
			int fbt_layer_id = -1;
			unsigned long long temp_bw_old = temp_bw;

			fbt_layer_id = mtk_crtc->fbt_layer_id;
			/* if layer is fbt layer need find fbt layer ratio to cal bw */
			if (idx == fbt_layer_id) {
				key = frame_idx - BWM_GPUC_TUNING_FRAME;
				for (i = 0; i < MAX_FRAME_RATIO_NUMBER; i++) {
					if ((key == fbt_layer_compress_ratio_tb[i].key_value) &&
						(fbt_layer_compress_ratio_tb[i].average_ratio
						!= 0) &&
						(fbt_layer_compress_ratio_tb[i].average_ratio
						<= 1000)) {
						avg_ratio =
						fbt_layer_compress_ratio_tb[i].average_ratio;
						temp_bw = temp_bw * avg_ratio;
						do_div(temp_bw, 1000);
						/* After BWM no need *0.7,So need to make up for it */
						temp_bw *= 10;
						do_div(temp_bw, 7);
						break;
					}
				}
			} else {
				int have_get_ratio = 0;

				key = frame_idx + alloc_id - BWM_GPUC_TUNING_FRAME;
				for (i = 0; i < MAX_LAYER_RATIO_NUMBER; i++) {
					if ((alloc_id ==
						unchanged_compress_ratio_table[i].key_value) &&
						(unchanged_compress_ratio_table[i].average_ratio
						!= 0) &&
						(unchanged_compress_ratio_table[i].average_ratio
						<= 1000)) {
						avg_ratio =
						unchanged_compress_ratio_table[i].average_ratio;
						temp_bw = temp_bw * avg_ratio;
						do_div(temp_bw, 1000);
						have_get_ratio = 1;
						/* After BWM no need *0.7,So need to make up for it */
						temp_bw *= 10;
						do_div(temp_bw, 7);
						break;
					}
				}
				for (i = 0; i < MAX_FRAME_RATIO_NUMBER*MAX_LAYER_RATIO_NUMBER;
					i++) {
					if (have_get_ratio)
						break;

					if ((key == normal_layer_compress_ratio_tb[i].key_value) &&
						(normal_layer_compress_ratio_tb[i].average_ratio
						!= 0) &&
						(normal_layer_compress_ratio_tb[i].average_ratio
						<= 1000)) {
						avg_ratio =
						normal_layer_compress_ratio_tb[i].average_ratio;
						temp_bw = temp_bw * avg_ratio;
						do_div(temp_bw, 1000);
						/* After BWM no need *0.7,So need to make up for it */
						temp_bw *= 10;
						do_div(temp_bw, 7);
						break;
					}
				}
			}

			/* Due to low ratio, bw will be 0 */
			/* We want the min value to be 1 */
			if (temp_bw <= 0)
				temp_bw = 1;

			DDPDBG_BWM("BWM:frame idx:%u alloc id:%lu key:%llu lye_idx:%u bw:%llu(%llu)\n",
					frame_idx, alloc_id, key, idx, temp_bw, temp_bw_old);
		}

		if (pending->mml_mode != MML_MODE_RACING) {
#ifdef IF_ZERO
			if (pending->prop_val[PLANE_PROP_COMPRESS])
				comp->fbdc_bw += temp_bw;
			else
				comp->qos_bw += temp_bw;
#else
			/* so far only report one qos BW, no need to separate FBDC or normal BW */
			if (!IS_ERR(comp->qos_req_other)) {
				if (lye_idx % 2) {
					comp->qos_bw_other += temp_bw;
					comp->hrt_bw_other = temp_peak_bw;
				} else {
					comp->qos_bw += temp_bw;
					comp->hrt_bw = temp_peak_bw;
				}
			} else {
				comp->qos_bw += temp_bw;
				comp->hrt_bw = temp_peak_bw;
			}
#endif
		}
	}
}

static bool compr_l_config_PVRIC_V3_1(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle)
{
	/* input config */
	struct mtk_plane_pending_state *pending = &state->pending;
	dma_addr_t addr = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int vpitch = (unsigned int)pending->prop_val[PLANE_PROP_VPITCH];
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x = pending->src_x, src_y = pending->src_y;
	unsigned int src_w = pending->width, src_h = pending->height;
	unsigned int fmt = pending->format;
	unsigned int Bpp = mtk_drm_format_plane_cpp(fmt, 0);
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int compress = (unsigned int)pending->prop_val[PLANE_PROP_COMPRESS];
	int rotate = 0;

	/* variable to do calculation */
	unsigned int tile_w = 16, tile_h = 4;
	unsigned int tile_body_size = tile_w * tile_h * Bpp;
	unsigned int src_x_align, src_y_align;
	unsigned int src_w_align, src_h_align;
	unsigned int header_offset, tile_offset;
	dma_addr_t buf_addr;
	unsigned int src_buf_tile_num = 0;
	unsigned int buf_size = 0;
	unsigned int buf_total_size = 0;
	struct mtk_panel_params *params = NULL;

	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	unsigned int aid_sel_offset = 0;
	resource_size_t mmsys_reg = 0;
	int sec_bit;

	/* variable to config into register */
	unsigned int lx_fbdc_en;
	dma_addr_t lx_addr, lx_hdr_addr;
	unsigned int lx_pitch, lx_hdr_pitch;
	unsigned int lx_clip, lx_src_size;

	if (comp->mtk_crtc)
		params = mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
	if (params && params->rotate == MTK_PANEL_ROTATE_180)
		if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
			rotate = 1;
#ifdef MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif
	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	/* 1. cal & set OVL_LX_FBDC_EN */
	lx_fbdc_en = (compress != 0);
	if (ext_lye_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
			       lx_fbdc_en << (ext_lye_idx + 3),
			       BIT(ext_lye_idx + 3));
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			       lx_fbdc_en << (lye_idx + 4), BIT(lye_idx + 4));

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_SYSRAM_CFG(lye_idx), 0,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_SYSRAM_BUF0_ADDR(lye_idx),
		0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_SYSRAM_BUF1_ADDR(lye_idx),
		0, ~0);

	if (comp->id == DDP_COMPONENT_OVL0_2L || comp->id == DDP_COMPONENT_OVL1_2L) {
		// setting SMI for read SRAM
		cmdq_pkt_write(handle, comp->cmdq_base,
			(resource_size_t)(0x14021000) + SMI_LARB_NON_SEC_CON + 4*9,
			0x00000000, GENMASK(19, 16));
	}
	/* if no compress, do common config and return */
	if (compress == 0) {
		_ovl_common_config(comp, idx, state, handle);
		return 0;
	}

	/* 2. pre-calculation */
	if (fmt == DRM_FORMAT_RGB888 || fmt == DRM_FORMAT_BGR888) {
		pitch = (4 * pitch / 3);
		Bpp = 4;
	}

	src_buf_tile_num = ALIGN_TO(pitch / 4, tile_w) *
			ALIGN_TO(vpitch, tile_h);
	src_buf_tile_num /= (tile_w * tile_h);
	header_offset = (src_buf_tile_num + 255) / 256 * 128;
	buf_addr = addr + header_offset;

	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;
	src_y_align = (src_y / tile_h) * tile_h;
	src_h_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - src_y_align;

	if (rotate)
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			      (pitch / tile_w / 4) *
				      (src_y_align + src_h_align - tile_h) /
				      tile_h;
	else
		tile_offset = src_x_align / tile_w +
			      (pitch / tile_w / 4) * src_y_align / tile_h;

	/* 3. cal OVL_LX_ADDR * OVL_LX_PITCH */
	lx_addr = buf_addr + tile_offset * 256;
	lx_pitch = pitch * tile_h;

	/* 4. cal OVL_LX_HDR_ADDR, OVL_LX_HDR_PITCH */
	lx_hdr_addr = buf_addr - (tile_offset / 2) - 1;
	lx_hdr_pitch = (pitch / tile_w / 8) |
		       (((pitch / tile_w / 4) % 2) << 16) |
		       (((tile_offset + 1) % 2) << 20);

	/* 5. calculate OVL_LX_SRC_SIZE */
	lx_src_size = (src_h_align << 16) | src_w_align;

	/* 6. calculate OVL_LX_CLIP */
	lx_clip = 0;
	if (rotate) {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
					       src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
					       src_x_align + src_w_align -
						       src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
					       src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
					       src_y_align + src_h_align -
						       src_y - src_h);
	} else {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
					       src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
					       src_x_align + src_w_align -
						       src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
					       src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
					       src_y_align + src_h_align -
						       src_y - src_h);
	}

	/* 7. config register */
	buf_size = (dst_h - 1) * pending->pitch +
		dst_w * mtk_drm_format_plane_cpp(fmt, 0);
	buf_total_size = header_offset + src_buf_tile_num * tile_body_size;

	if (ovl->data->mmsys_mapping)
		mmsys_reg = ovl->data->mmsys_mapping(comp);

	if (ovl->data->aid_sel_mapping)
		aid_sel_offset = ovl->data->aid_sel_mapping(comp);

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_aid_bit(comp, true, id);
			if (ovl->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else if(comp->mtk_crtc->sec_on) {
				if (state->pending.is_sec && pending->addr) {
					DDPMSG("enter svp----\n");
					write_sec_ext_layer_addr_cmdq(comp, handle, id,
							addr, 0, buf_size);
					set_sec_ext_layer_dom_cmdq(comp, handle, id);
					goto legacy_sec1;
				} else
					clr_sec_ext_layer_dom_cmdq(comp, handle, id);
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		}

		write_ext_layer_addr_cmdq(comp, handle, id, lx_addr);
legacy_sec1:
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_EL_PITCH(id),
			       lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_SRC_SIZE(id),
			       lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_EL_CLIP(id),
			       lx_clip, ~0);

		write_ext_layer_hdr_addr_cmdq(comp, handle, id, lx_hdr_addr);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ELX_HDR_PITCH(ovl, id),
			       lx_hdr_pitch, ~0);
	} else {
		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_aid_bit(comp, false, lye_idx);
			if (ovl->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		} else {
			if (comp->mtk_crtc->sec_on) {
				if (state->pending.is_sec && pending->addr) {
					write_sec_phy_layer_addr_cmdq(comp, handle, lye_idx,
								addr, 0, buf_size);
					set_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
					goto legacy_sec2;
				} else
					clr_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
			} else
				clr_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
		}

		write_phy_layer_addr_cmdq(comp, handle, lye_idx, lx_addr);
legacy_sec2:
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_PITCH(lye_idx),
			       lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_SIZE(lye_idx),
			       lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CLIP(lye_idx),
			       lx_clip, ~0);
		write_phy_layer_hdr_addr_cmdq(comp, handle, lye_idx,
				      lx_hdr_addr);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_LX_HDR_PITCH(lye_idx),
			       lx_hdr_pitch, ~0);
	}

	return 0;
}

bool compr_l_config_AFBC_V1_2(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle)
{
	/* input config */
	struct mtk_plane_pending_state *pending = &state->pending;
	dma_addr_t addr = pending->addr;
	bool enable = pending->enable;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int vpitch = (unsigned int)pending->prop_val[PLANE_PROP_VPITCH];
	unsigned int src_x = pending->src_x, src_y = pending->src_y;
	unsigned int src_w = pending->width, src_h = pending->height;
	unsigned int fmt = pending->format;
	unsigned int Bpp = mtk_drm_format_plane_cpp(fmt, 0);
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int compress = (unsigned int)pending->prop_val[PLANE_PROP_COMPRESS];
	int rotate = 0;

	/* variable to do calculation */
	unsigned int tile_w = AFBC_V1_2_TILE_W;
	unsigned int tile_h = AFBC_V1_2_TILE_H;
	unsigned int tile_body_size = tile_w * tile_h * Bpp;
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x_align, src_w_align;
	unsigned int src_y_align, src_y_half_align;
	unsigned int src_y_end_align, src_y_end_half_align;
	unsigned int src_h_align, src_h_half_align = 0;
	unsigned int header_offset, tile_offset;
	dma_addr_t buf_addr;
	unsigned int src_buf_tile_num = 0;
	unsigned int buf_size = 0;
	unsigned int buf_total_size = 0;

	/* variable to config into register */
	unsigned int lx_fbdc_en;
	dma_addr_t lx_addr, lx_hdr_addr;
	unsigned int lx_pitch, lx_hdr_pitch;
	unsigned int lx_clip, lx_src_size;
	unsigned int lx_2nd_subbuf = 0;
	unsigned int lx_pitch_msb = 0;

	struct mtk_panel_params *params = NULL;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	unsigned int aid_sel_offset = 0;
	resource_size_t mmsys_reg = 0;
	int sec_bit;

	DDPDBG("%s:%d, addr:0x%lx, pitch:%d, vpitch:%d\n",
		__func__, __LINE__, (unsigned long)addr,
		pitch, vpitch);
	DDPDBG("src:(%d,%d,%d,%d), fmt:%d, Bpp:%d, compress:%d\n",
		src_x, src_y,
		src_w, src_h,
		fmt, Bpp,
		compress);

	if (comp->mtk_crtc)
		params = mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
	if (params && params->rotate == MTK_PANEL_ROTATE_180)
		if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
			rotate = 1;
#ifdef MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif
	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	/* 1. cal & set OVL_LX_FBDC_EN */
	lx_fbdc_en = (compress != 0);
	if (ext_lye_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
			lx_fbdc_en << (ext_lye_idx + 3),
			BIT(ext_lye_idx + 3));
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			lx_fbdc_en << (lye_idx + 4), BIT(lye_idx + 4));

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_SYSRAM_CFG(lye_idx), 0,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_SYSRAM_BUF0_ADDR(lye_idx),
		0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_SYSRAM_BUF1_ADDR(lye_idx),
		0, ~0);

	/* setting SMI for read DRAM */
	if (comp->larb_cons)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->larb_cons[lye_idx], 0, GENMASK(19, 16));

	/* if no compress, do common config and return */
	if (compress == 0 || (pending->mml_mode == MML_MODE_RACING) ||
			     (pending->mml_mode == MML_MODE_DIRECT_LINK)) {
		_ovl_common_config(comp, idx, state, handle);
		return 0;
	}

	/* 2. pre-calculation */
	if (Bpp == 0) {
		DDPPR_ERR("%s fail, no Bpp info\n", __func__);
		return 0;
	}
	src_buf_tile_num = ALIGN_TO(pitch / Bpp, tile_w) *
	    ALIGN_TO(vpitch, tile_h);
	src_buf_tile_num /= (tile_w * tile_h);
	header_offset = ALIGN_TO(
		src_buf_tile_num * AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES,
		AFBC_V1_2_HEADER_ALIGN_BYTES);
	buf_addr = addr + header_offset;

	/* calculate for alignment */
	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;

	/* src_y_half_align, src_y_end_half_align,
	 * the start y offset and  stop y offset if half tile align
	 * such as 0 and 3, then the src_h_align is 4
	 */
	src_y_align = (src_y / tile_h) * tile_h;
	src_y_end_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - 1;
	src_h_align = src_y_end_align - src_y_align + 1;

	src_y_half_align = (src_y / (tile_h >> 1)) * (tile_h >> 1);
	src_y_end_half_align =
		(1 + (src_y + src_h - 1) / (tile_h >> 1)) * (tile_h >> 1) - 1;
	src_h_half_align = src_y_end_half_align - src_y_half_align + 1;

	if (rotate) {
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			(pitch / tile_w / Bpp) *
			(src_y_align + src_h_align - tile_h) /
			tile_h;
		if (src_y_end_align == src_y_end_half_align)
			lx_2nd_subbuf = 1;
	} else {
		tile_offset = src_x_align / tile_w +
			(pitch / tile_w / Bpp) * src_y_align / tile_h;
		if (src_y_align != src_y_half_align)
			lx_2nd_subbuf = 1;
	}

	/* 3. cal OVL_LX_ADDR * OVL_LX_PITCH */
	lx_addr = buf_addr + (dma_addr_t) tile_offset * (dma_addr_t) tile_body_size;
	lx_pitch = ((pitch * tile_h) & 0xFFFF);
	lx_pitch_msb = (REG_FLD_VAL((L_PITCH_MSB_FLD_YUV_TRANS), (1)) |
		REG_FLD_VAL((L_PITCH_MSB_FLD_2ND_SUBBUF), (lx_2nd_subbuf)) |
		REG_FLD_VAL((L_PITCH_MSB_FLD_SRC_PITCH_MSB),
		((pitch * tile_h) >> 16) & 0xF));

	/* 4. cal OVL_LX_HDR_ADDR, OVL_LX_HDR_PITCH */
	lx_hdr_addr = addr + tile_offset *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;
	lx_hdr_pitch = pitch / tile_w / Bpp *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;

	/* 5. calculate OVL_LX_SRC_SIZE, RGB565 use layout 4, src_h needs align to tile_h*/
	if (fmt != DRM_FORMAT_RGB565 && fmt != DRM_FORMAT_BGR565) {
		src_h_align = src_h_half_align;
		src_y_align = src_y_half_align;
	}
	lx_src_size = (src_h_align << 16) | src_w_align;

	/* 6. calculate OVL_LX_CLIP */
	lx_clip = 0;
	if (rotate) {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
				src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
				src_y_align + src_h_align -
				src_y - src_h);
	} else {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
				src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
				src_y_align + src_h_align -
				src_y - src_h);
	}

	/* 7. config register */
	buf_size = (dst_h - 1) * pitch + dst_w * Bpp;
	buf_total_size = header_offset + src_buf_tile_num * tile_body_size;

	if (ovl->data->mmsys_mapping)
		mmsys_reg = ovl->data->mmsys_mapping(comp);

	if (ovl->data->aid_sel_mapping)
		aid_sel_offset = ovl->data->aid_sel_mapping(comp);

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_aid_bit(comp, true, id);
			if (ovl->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		} else {
			if (comp->mtk_crtc->sec_on) {
				u32 addr_offset;

				addr_offset = header_offset + tile_offset * tile_body_size;
				if (state->pending.is_sec && pending->addr) {
					write_sec_ext_layer_addr_cmdq(comp, handle, id,
							addr, addr_offset, buf_size);

					addr_offset = tile_offset *
					AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;
					write_sec_ext_layer_hdr_addr_cmdq(comp, handle, id,
								addr, addr_offset, buf_size);

					set_sec_ext_layer_dom_cmdq(comp, handle, id);
					goto legacy_sec1;
				} else
					clr_sec_ext_layer_dom_cmdq(comp, handle, id);
			} else
				clr_sec_ext_layer_dom_cmdq(comp, handle, id);
		}

		write_ext_layer_addr_cmdq(comp, handle, id, lx_addr);
		write_ext_layer_hdr_addr_cmdq(comp, handle, id, lx_hdr_addr);
legacy_sec1:
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_PITCH_MSB(id),
			lx_pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_PITCH(id),
			lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_SRC_SIZE(id),
			lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_CLIP(id),
			lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_ELX_HDR_PITCH(ovl, id),
			lx_hdr_pitch, ~0);
	} else {
		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_aid_bit(comp, false, lye_idx);
			if (ovl->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		} else {
			if (comp->mtk_crtc->sec_on) {
				u32 addr_offset;

				addr_offset = header_offset + tile_offset * tile_body_size;
				if (state->pending.is_sec && pending->addr) {
					write_sec_phy_layer_addr_cmdq(comp, handle, lye_idx, addr,
						addr_offset, buf_size);

					addr_offset = tile_offset *
					AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;
					write_sec_phy_layer_hdr_addr_cmdq(comp, handle, lye_idx,
								addr, addr_offset, buf_size);

					set_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
					goto legacy_sec2;
				} else
					clr_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
			} else
				clr_sec_phy_layer_dom_cmdq(comp, handle, lye_idx);
		}

		write_phy_layer_addr_cmdq(comp, handle, lye_idx, lx_addr);
		write_phy_layer_hdr_addr_cmdq(comp, handle, lye_idx,
					lx_hdr_addr);
legacy_sec2:
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH_MSB(lye_idx),
			lx_pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH(lye_idx),
			lx_pitch, 0xffff);

		if (ovl->data->support_pq_selfloop && (pending->pq_loop_type == 2)) {
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				       DISP_REG_OVL_PQ_OUT_SIZE, lx_src_size, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				       DISP_REG_OVL_SRC_SIZE(lye_idx), pending->dst_roi, ~0);
		} else
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DISP_REG_OVL_SRC_SIZE(lye_idx), lx_src_size,
				       ~0);

		if (ovl->ovl_dis == false && enable == 1 && compress == 1 &&
			((lx_src_size&0xffff) * (lx_src_size>>16) == 0)) {
			DDPPR_ERR("%s, %s, idx=%d, en=%d, size:0x%08x\n", __func__,
				mtk_dump_comp_str_id(comp->id), lye_idx, enable, lx_src_size);
			ovl->ovl_dis = true;
		}
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_CLIP(lye_idx),
			lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_LX_HDR_PITCH(lye_idx),
			lx_hdr_pitch, ~0);
	}

	return 0;
}

static int _ovl_UFOd_in(struct mtk_ddp_comp *comp, int connect,
			struct cmdq_pkt *handle)
{
	unsigned int value = 0, mask = 0;

	if (!connect) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0, BIT(4));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_LC_CON, 0, ~0);
		return 0;
	}

	SET_VAL_MASK(value, mask, 2, L_CON_FLD_LSRC);
	SET_VAL_MASK(value, mask, 0, L_CON_FLD_AEN);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_LC_CON, value, mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_LC_SRC_SEL, 0, 0x7);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0x10, 0x10);

	return 0;
}

static void
mtk_ovl_addon_rsz_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			 enum mtk_ddp_comp_id next, struct mtk_rect rsz_src_roi,
			 struct mtk_rect rsz_dst_roi, struct cmdq_pkt *handle)
{
	struct mtk_drm_private *priv = NULL;

	if (!comp || !comp->mtk_crtc || !comp->mtk_crtc->base.dev) {
		DDPPR_ERR("%s:%d NULL Pointer\n", __func__, __LINE__);
		return;
	}
	priv = comp->mtk_crtc->base.dev->dev_private;
	if (!priv) {
		DDPPR_ERR("%s:%d NULL Pointer\n", __func__, __LINE__);
		return;
	}

	if (prev == DDP_COMPONENT_RSZ0 ||
		prev == DDP_COMPONENT_RSZ1 ||
		prev == DDP_COMPONENT_Y2R0 ||
		prev == DDP_COMPONENT_Y2R1) {
		int lc_x = rsz_dst_roi.x, lc_y = rsz_dst_roi.y;
		int lc_w = rsz_dst_roi.width, lc_h = rsz_dst_roi.height;
		struct mtk_panel_params *params = NULL;

		if (comp->mtk_crtc)
			params = mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
		if (params && params->rotate == MTK_PANEL_ROTATE_180) {
			int bg_w, bg_h;

			_get_bg_roi(comp, &bg_h, &bg_w);
			lc_y = bg_h - lc_h - lc_y;
			lc_x = bg_w - lc_w - lc_x;
		}

		_ovl_UFOd_in(comp, 1, handle);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_LC_OFFSET,
			       ((lc_y << 16) | lc_x), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_LC_SRC_SIZE,
			       ((lc_h << 16) | lc_w), ~0);
	} else
		_ovl_UFOd_in(comp, 0, handle);

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761 ||
		priv->data->mmsys_id == MMSYS_MT6877 ||
		priv->data->mmsys_id == MMSYS_MT6833 ||
		priv->data->mmsys_id == MMSYS_MT6885 ||
		priv->data->mmsys_id == MMSYS_MT6853 ||
		priv->data->mmsys_id == MMSYS_MT6781) {
		if (prev == DDP_COMPONENT_OVL0 || prev == DDP_COMPONENT_OVL0_2L ||
			prev == DDP_COMPONENT_OVL1 || prev == DDP_COMPONENT_OVL1_2L ||
			prev == DDP_COMPONENT_OVL2_2L || prev == DDP_COMPONENT_OVL3_2L ||
			prev == DDP_COMPONENT_OVL4_2L || prev == DDP_COMPONENT_OVL5_2L ||
			prev == DDP_COMPONENT_OVL6_2L)
			cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
					DISP_OVL_BGCLR_IN_SEL, DISP_OVL_BGCLR_IN_SEL);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_DATAPATH_CON, 0,
			DISP_OVL_BGCLR_IN_SEL);
	}
	if (prev == -1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_ROI_SIZE,
				rsz_src_roi.height << 16 | rsz_src_roi.width,
				~0);
		_store_bg_roi(comp, rsz_src_roi.height, rsz_src_roi.width);
	}
	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6877 ||
		priv->data->mmsys_id == MMSYS_MT6833 ||
		priv->data->mmsys_id == MMSYS_MT6761 ||
		priv->data->mmsys_id == MMSYS_MT6853 ||
		priv->data->mmsys_id == MMSYS_MT6781){
		struct mtk_ddp_comp *comp_ovl0 = priv->ddp_comp[DDP_COMPONENT_OVL0];
		static char init_ovl0 = true;

		/* Since the OVL was swapped after LK to kernel, */
		/* the OVL needs to be reinitialized. */
		if (init_ovl0) {
			/* vol0 layer0 off */
			mtk_ovl_layer_off(comp_ovl0, 0, 0, handle);
			init_ovl0 = false;
		}
	}
}

static void mtk_ovl_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	if ((addon_config->config_type.module == DISP_RSZ ||
		addon_config->config_type.module == DISP_RSZ_v2 ||
		addon_config->config_type.module == DISP_RSZ_v3 ||
		addon_config->config_type.module == DISP_RSZ_v4 ||
		addon_config->config_type.module == DISP_RSZ_v5 ||
		addon_config->config_type.module == DISP_RSZ_v6) &&
		addon_config->config_type.type == ADDON_BETWEEN) {
		struct mtk_addon_rsz_config *config =
			&addon_config->addon_rsz_config;

		mtk_ovl_addon_rsz_config(comp, prev, next, config->rsz_src_roi,
					 config->rsz_dst_roi, handle);
	}

	if ((addon_config->config_type.module == DISP_MML_IR_PQ ||
		addon_config->config_type.module == DISP_MML_IR_PQ_1 ||
		addon_config->config_type.module == DISP_MML_IR_PQ_v3) &&
		(addon_config->config_type.type == ADDON_CONNECT ||
		addon_config->config_type.type == ADDON_DISCONNECT)) {
		struct mtk_addon_mml_config *config = &addon_config->addon_mml_config;
		struct mtk_rect src, dst;

		src = config->mml_src_roi[config->pipe];
		dst = config->mml_dst_roi[config->pipe];

		/* this rsz means enlarge/narrow, not component */
		mtk_ovl_addon_rsz_config(comp, prev, next, src, dst, handle);
	}
}

static void mtk_ovl_config_begin(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle, const u32 idx)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	u32 value = 0, mask = 0;

	if (!ovl->data->support_pq_selfloop)
		return;

	if (!comp->mtk_crtc)
		return;

	/* no need connect to OVL PQ_LOOP or PQ_OUT path for external display so far */
	if (comp->mtk_crtc->base.index != 0)
		return;

	if (idx != 0) {
		value |= DISP_OVL_BGCLR_IN_SEL;
		mask |= DISP_OVL_BGCLR_IN_SEL;
	}

	SET_VAL_MASK(value, mask, 0, DISP_OVL_PQ_OUT_EN);
	SET_VAL_MASK(value, mask, 0, DISP_OVL_PQ_OUT_OPT);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			value, mask);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_OVL_PQ_LOOP_CON,
			0, DISP_OVL_PQ_OUT_SIZE_SEL);
}

static void mtk_ovl_connect(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_id prev,
			    enum mtk_ddp_comp_id next)
{
	/* prev == DDP_COMPONENT_ID_MAX imply this OVL is the first comp in current path */
	/* OVL might be connected from VIRTUAL comp like DLI in other dispsys */
	if (mtk_ddp_comp_get_type(prev) != MTK_DISP_OVL &&
			mtk_ddp_comp_get_type(prev) !=  MTK_DISP_VIRTUAL &&
			mtk_ddp_comp_get_type(prev) !=  MTK_DISP_DLI_ASYNC &&
			prev != DDP_COMPONENT_ID_MAX)
		return;
	else if (prev == DDP_COMPONENT_OVL0 || prev == DDP_COMPONENT_OVL0_2L ||
		prev == DDP_COMPONENT_OVL1 || prev == DDP_COMPONENT_OVL1_2L ||
		prev == DDP_COMPONENT_OVL2_2L || prev == DDP_COMPONENT_OVL3_2L ||
		prev == DDP_COMPONENT_OVL4_2L || prev == DDP_COMPONENT_OVL5_2L ||
		prev == DDP_COMPONENT_OVL6_2L ||
		mtk_ddp_comp_get_type(prev) ==  MTK_DISP_VIRTUAL ||
		mtk_ddp_comp_get_type(prev) ==  MTK_DISP_DLI_ASYNC) {
		if (handle == NULL)
			mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_DATAPATH_CON,
				       DISP_OVL_BGCLR_IN_SEL,
				       DISP_OVL_BGCLR_IN_SEL);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
				       DISP_OVL_BGCLR_IN_SEL,
				       DISP_OVL_BGCLR_IN_SEL);
	} else {
		if (handle == NULL)
			mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_DATAPATH_CON,
				       0, DISP_OVL_BGCLR_IN_SEL);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
				       0, DISP_OVL_BGCLR_IN_SEL);
	}
}

void mtk_ovl_cal_golden_setting(struct mtk_ddp_config *cfg,
				struct mtk_ddp_comp *comp, unsigned int *gs)
{
	bool is_dc = cfg->p_golden_setting_context->is_dc;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	const struct mtk_disp_ovl_data *data = ovl->data;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	DDPDBG("%s,is_dc:%d\n", __func__, is_dc);

	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	gs[GS_OVL_RDMA_ULTRA_TH] = 0x3ff;
	gs[GS_OVL_RDMA_PRE_ULTRA_TH] = (!is_dc) ? 0x3ff : data->preultra_th_dc;

	/* OVL_RDMA_FIFO_CTRL */
	gs[GS_OVL_RDMA_FIFO_THRD] = 0;
	gs[GS_OVL_RDMA_FIFO_SIZE] = data->fifo_size;

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	gs[GS_OVL_RDMA_ISSUE_REQ_TH] = (!is_dc) ? data->issue_req_th_dl :
				data->issue_req_th_dc;
	gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] = (!is_dc) ? data->issue_req_th_urg_dl :
				data->issue_req_th_urg_dc;
	gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] = 0;
	gs[GS_OVL_RDMA_REQ_TH_ULTRA] = 1;
	gs[GS_OVL_RDMA_FORCE_REQ_TH] = 0;

	/* OVL_RDMA_GREQ_NUM */
	gs[GS_OVL_RDMA_GREQ_NUM] = (!is_dc) ? (0xF1FF0000 | data->greq_num_dl)
						: 0xF1FF0000;

	/* OVL_RDMA_GREQURG_NUM */
	gs[GS_OVL_RDMA_GREQ_URG_NUM] = (!is_dc) ? data->greq_num_dl : 0x0;

	/* OVL_RDMA_ULTRA_SRC */
	gs[GS_OVL_RDMA_ULTRA_SRC] = (!is_dc) ? 0x8040 : 0xA040;

	/* OVL_RDMA_BUF_LOW_TH */
	gs[GS_OVL_RDMA_ULTRA_LOW_TH] = 0;
	if (priv && priv->data->mmsys_id == MMSYS_MT6877)
		gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] = (!is_dc) ?
					0 : (gs[GS_OVL_RDMA_FIFO_SIZE] / 12);
	else
		gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] = (!is_dc) ?
					0 : (gs[GS_OVL_RDMA_FIFO_SIZE] / 8);
	/* OVL_RDMA_BUF_HIGH_TH */
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] = (!is_dc) ?
				0 : (gs[GS_OVL_RDMA_FIFO_SIZE] * 6 / 8);
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] = 1;

	/* OVL_EN */
	gs[GS_OVL_BLOCK_EXT_ULTRA] = (!is_dc) ? 0 : 1;
	gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] = (!is_dc) ? 0 : 1;

	if (data->stash_en)
		gs[GS_OVL_STASH_EN] = data->stash_en;
	else
		gs[GS_OVL_STASH_EN] = 0;

	gs[GS_OVL_STASH_CFG] = (data->stash_cfg) ? (data->stash_cfg) : 0;

}

static int mtk_ovl_golden_setting(struct mtk_ddp_comp *comp,
				  struct mtk_ddp_config *cfg,
				  struct cmdq_pkt *handle)
{
	unsigned long baddr = comp->regs_pa;
	unsigned int regval;
	unsigned int gs[GS_OVL_FLD_NUM];
	int i, layer_num;
	unsigned long Lx_base;

	layer_num = mtk_ovl_layer_num(comp);

	/* calculate ovl golden setting */
	mtk_ovl_cal_golden_setting(cfg, comp, gs);

	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	regval =
		gs[GS_OVL_RDMA_ULTRA_TH] + (gs[GS_OVL_RDMA_PRE_ULTRA_TH] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       Lx_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING,
			       regval, ~0);
	}

	/* OVL_RDMA_FIFO_CTRL */
	regval = gs[GS_OVL_RDMA_FIFO_THRD] + (gs[GS_OVL_RDMA_FIFO_SIZE] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       Lx_base + DISP_REG_OVL_RDMA0_FIFO_CTRL, regval,
			       ~0);
	}

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	regval = gs[GS_OVL_RDMA_ISSUE_REQ_TH] +
		 (gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] << 16) +
		 (gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] << 28) +
		 (gs[GS_OVL_RDMA_REQ_TH_ULTRA] << 29) +
		 (gs[GS_OVL_RDMA_FORCE_REQ_TH] << 30);
	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + DISP_REG_OVL_RDMA0_MEM_GMC_S2 + i * 4,
			       regval, ~0);

	/* DISP_REG_OVL_RDMA_GREQ_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_NUM];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_RDMA_GREQ_NUM, regval, ~0);

	/* DISP_REG_OVL_RDMA_GREQ_URG_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_URG_NUM];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_RDMA_GREQ_URG_NUM, regval, ~0);

	/* DISP_REG_OVL_RDMA_ULTRA_SRC */
	regval = gs[GS_OVL_RDMA_ULTRA_SRC];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_RDMA_ULTRA_SRC, regval, ~0);

	/* DISP_REG_OVL_RDMAn_BUF_LOW */
	regval = gs[GS_OVL_RDMA_ULTRA_LOW_TH] +
		 (gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] << 12);

	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + DISP_REG_OVL_RDMAn_BUF_LOW(i), regval,
			       ~0);

	/* DISP_REG_OVL_RDMAn_BUF_HIGH */
	regval = (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] << 12) +
		 (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] << 31);

	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + DISP_REG_OVL_RDMAn_BUF_HIGH(i), regval,
			       ~0);

	/* OVL_EN */
	regval = (gs[GS_OVL_BLOCK_EXT_ULTRA] << 18) +
		 (gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] << 19);
	cmdq_pkt_write(handle, comp->cmdq_base, baddr + DISP_REG_OVL_EN,
		       regval, 0x3 << 18);

	/* OVL_STASH_EN */
	regval = gs[GS_OVL_STASH_EN];
	if (regval) {
		cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_STASH_CFG0, regval, ~0);
	}

	regval = gs[GS_OVL_STASH_CFG];
	if (regval) {
		cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_STASH_CFG1, regval, ~0);
	}

	return 0;
}

static dma_addr_t read_phy_layer_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	dma_addr_t layer_addr = 0;

	if (ovl->data->is_support_34bits) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_ADDR_MSB(id));
		layer_addr = DO_SHIFT_LEFT((layer_addr & 0xf), 32);
	}

	layer_addr += readl(comp->regs + DISP_REG_OVL_ADDR(ovl, id));

	return layer_addr;
}

static dma_addr_t read_ext_layer_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	dma_addr_t layer_addr = 0;

	if (ovl->data->is_support_34bits) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_EL_ADDR_MSB(id));
		layer_addr = DO_SHIFT_LEFT((layer_addr & 0xf), 32);
	}

	layer_addr += readl(comp->regs + DISP_REG_OVL_EL_ADDR(ovl, id));

	return layer_addr;
}

static dma_addr_t read_phy_layer_hdr_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	dma_addr_t layer_addr = 0;

	if (ovl->data->is_support_34bits) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_ADDR_MSB(id));
		layer_addr = ((layer_addr & 0xf00) << 24);
	}

	layer_addr += readl(comp->regs + DISP_REG_OVL_LX_HDR_ADDR(id));

	return layer_addr;
}

static dma_addr_t read_ext_layer_hdr_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	dma_addr_t layer_addr = 0;

	if (ovl->data->is_support_34bits) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_EL_ADDR_MSB(id));
		layer_addr += ((layer_addr & 0xf00) << 24);
	}

	layer_addr += readl(comp->regs + DISP_REG_OVL_ELX_HDR_ADDR(ovl, id));

	return layer_addr;
}

static int mtk_ovl_replace_bootup_mva(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, void *params,
				      struct mtk_ddp_fb_info *fb_info)
{
	unsigned int src_on = readl(comp->regs + DISP_REG_OVL_SRC_CON);
	dma_addr_t layer_addr, layer_mva;
	struct iommu_domain *domain;
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_display_mode *mode = NULL;
	unsigned int bw;
	int ret = 0;

	if (unlikely(!(comp && comp->mtk_crtc))) {
		DDPPR_ERR("%s invalid comp or mtk_crtc\n", __func__);
		return -EINVAL;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;
	if (crtc->state) {
		mode = &crtc->state->adjusted_mode;
		bw = _layering_get_frame_bw(crtc, mode);
	}
	if (src_on & 0x1) {
		layer_addr = read_phy_layer_addr(comp, 0);
		if (priv->data->mmsys_id == MMSYS_MT6989 &&
			comp->id == DDP_COMPONENT_OVL0_2L) {
			DDPMSG("%s, replace mva same as pa %pad\n", __func__, &layer_addr);
			domain = iommu_get_domain_for_dev(comp->dev);
			if (domain == NULL) {
				DDPPR_ERR("%s, iommu_get_domain fail\n", __func__);
				return -1;
			}
			ret = iommu_map(domain, layer_addr, layer_addr,
				ROUNDUP(fb_info->size, PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE, GFP_KERNEL);
			write_phy_layer_addr_cmdq(comp, handle, 0, layer_addr);
		} else {
			layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_gem->dma_addr;
			write_phy_layer_addr_cmdq(comp, handle, 0, layer_mva);
		}
		if (mode)
			comp->qos_bw = bw;
	}

	if (src_on & 0x2) {
		layer_addr = read_phy_layer_addr(comp, 1);
		if (priv->data->mmsys_id == MMSYS_MT6989 &&
			comp->id == DDP_COMPONENT_OVL0_2L) {
			DDPMSG("%s, replace mva same as pa %pad\n", __func__, &layer_addr);
			write_phy_layer_addr_cmdq(comp, handle, 1, layer_addr);
		} else {
			layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_gem->dma_addr;
			write_phy_layer_addr_cmdq(comp, handle, 1, layer_mva);
		}
		if (mode) {
			if (!IS_ERR(comp->qos_req_other))
				comp->qos_bw_other = bw;
			else
				comp->qos_bw = bw;
		}
	}

	if (src_on & 0x4) {
		layer_addr = read_phy_layer_addr(comp, 2);
		layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_gem->dma_addr;
		write_phy_layer_addr_cmdq(comp, handle, 2, layer_mva);
		if (mode)
			comp->qos_bw = bw;
	}

	if (src_on & 0x8) {
		layer_addr = read_phy_layer_addr(comp, 3);
		layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_gem->dma_addr;
		write_phy_layer_addr_cmdq(comp, handle, 3, layer_mva);
		if (mode) {
			if (!IS_ERR(comp->qos_req_other))
				comp->qos_bw_other = bw;
			else
				comp->qos_bw = bw;
		}
	}

	return 0;
}

static void mtk_ovl_backup_info_cmp(struct mtk_ddp_comp *comp, bool *compare)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	void __iomem *baddr = comp->regs, *Lx_base = NULL;
	int i = 0;
	unsigned int src_on = readl(DISP_REG_OVL_SRC_CON + baddr);
	struct mtk_ovl_backup_info cur_info[MAX_LAYER_NUM];

	memset(cur_info, 0, sizeof(cur_info));
	for (i = 0; i < mtk_ovl_layer_num(comp); i++) {
		unsigned int val = 0;

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		cur_info[i].layer = i;
		cur_info[i].layer_en = src_on & (0x1 << i);
		if (!cur_info[i].layer_en) {
			DDPDBG("%s:layer%d,en %d,size 0x%x,addr 0x%lx\n",
			       __func__, i, cur_info[i].layer_en,
			       cur_info[i].src_size, (unsigned long)cur_info[i].addr);
			continue;
		}

		cur_info[i].con = readl(DISP_REG_OVL_CON(i) + baddr);
		cur_info[i].addr = read_phy_layer_addr(comp, i);
		cur_info[i].src_size =
			readl(DISP_REG_OVL_L0_SRC_SIZE + Lx_base);

		val = readl(DISP_REG_OVL_L0_PITCH + Lx_base);
		cur_info[i].src_pitch =
			REG_FLD_VAL_GET(L_PITCH_FLD_SRC_PITCH, val);

		val = readl(DISP_REG_OVL_DATAPATH_CON + Lx_base);
		cur_info[i].data_path_con =
			readl(DISP_REG_OVL_DATAPATH_CON + Lx_base);

		DDPDBG("%s:layer%d,en %d,size 0x%x, addr 0x%lx\n", __func__, i,
		       cur_info[i].layer_en, cur_info[i].src_size,
		       (unsigned long)cur_info[i].addr);
		if (memcmp(&cur_info[i], &ovl->backup_info[i],
			   sizeof(struct mtk_ovl_backup_info)) != 0)
			*compare = true;
	}
	memcpy(ovl->backup_info, cur_info,
	       sizeof(struct mtk_ovl_backup_info) * MAX_LAYER_NUM);
}

static int mtk_ovl_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;
	struct mtk_drm_private *priv;

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPINFO("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), io_cmd);
		return -INVALID;
	}

	priv = comp->mtk_crtc->base.dev->dev_private;
	switch (io_cmd) {
	case MTK_IO_CMD_OVL_GOLDEN_SETTING: {
		struct mtk_ddp_config *cfg;

		cfg = (struct mtk_ddp_config *)params;
		mtk_ovl_golden_setting(comp, cfg, handle);
		break;
	}
	case OVL_ALL_LAYER_OFF:
	{
		int *keep_first_layer = params;

		mtk_ovl_all_layer_off(comp, handle, *keep_first_layer);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_RDMA0_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA1_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA2_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA3_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_ABNORMAL_SOF, 1) |
			REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_START_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTSTA, 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTEN, inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_NORMAL: {
		unsigned int inten;
		struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

		if (priv->data->mmsys_id == MMSYS_MT6985 ||
			priv->data->mmsys_id == MMSYS_MT6897)
			inten = REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1) |
					REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1) |
					REG_FLD_VAL(INIEN_ROI_TIMING_0, 1);
		else if (priv->data->mmsys_id == MMSYS_MT6989 ||
				priv->data->mmsys_id == MMSYS_MT6899)
			inten = REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1) |
					REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1);
		else
			inten = REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTSTA, 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTEN, inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_REG_CMT_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_START_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTEN, 0, inten);
		break;
	}
	case PMQOS_SET_BW: {
#ifdef IF_ZERO /* not ready for dummy register method */
		struct mtk_drm_crtc *mtk_crtc;
		struct cmdq_pkt_buffer *cmdq_buf;
		u32 ovl_bw, slot_num;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		mtk_crtc = comp->mtk_crtc;
		cmdq_buf = &(mtk_crtc->gce_obj.buf);

		/* process FBDC */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					    DISP_BW_FBDC_MODE);
		ovl_bw = *(unsigned int *)(cmdq_buf->va_base +
					    DISP_SLOT_PMQOS_BW(slot_num));

		__mtk_disp_set_module_srt(comp->fbdc_qos_req, comp->id, ovl_bw, 0,
					    DISP_BW_FBDC_MODE, priv->data->real_srt_ostdl);

		/* process normal */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					    DISP_BW_NORMAL_MODE);
		ovl_bw = *(unsigned int *)(cmdq_buf->va_base +
					    DISP_SLOT_PMQOS_BW(slot_num));

		__mtk_disp_set_module_srt(comp->qos_req, comp->id, ovl_bw, 0,
					    DISP_BW_NORMAL_MODE, priv->data->real_srt_ostdl);
#endif
		break;
	}
	case PMQOS_SET_HRT_BW: {
		u32 bw_val = *(unsigned int *)params;
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
		unsigned int phy_id = 0, port_bw = bw_val, total_bw = 0, hdr_bw = 0;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, bw_val,
				priv->data->respective_ostdl);
			if (!IS_ERR(comp->hrt_qos_req_other))
				__mtk_disp_set_module_hrt(comp->hrt_qos_req_other, comp->id, bw_val,
					priv->data->respective_ostdl);
			ret = OVL_REQ_HRT;
			break;
		}

		if (ovl->data->ovl_phy_mapping)
			phy_id = ovl->data->ovl_phy_mapping(comp);

		if (phy_id >= MAX_LAYER_NR) {
			DDPINFO("%s, invalid layer:%u\n", __func__, phy_id);
			break;
		}

		if (!IS_ERR_OR_NULL(comp->hrt_qos_req)) {
			port_bw = (bw_val * mtk_crtc->usage_ovl_fmt[phy_id]) >> 2;
			if (comp->last_hrt_bw != port_bw) {
				DDPQOS("%s/%u,layer:%u update:%u->%u compress:%d bw:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id, comp->last_hrt_bw,
					port_bw, mtk_crtc->usage_ovl_compr[phy_id], bw_val);
				__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, port_bw,
					priv->data->respective_ostdl);
				comp->last_hrt_bw = port_bw;
			}
			if (port_bw && mtk_crtc->usage_ovl_compr[phy_id])
				total_bw += port_bw;
		}

		if (!IS_ERR_OR_NULL(comp->hrt_qos_req_other)) {
			if (phy_id + 1 >= MAX_LAYER_NR) {
				DDPINFO("%s,invalid layer:%u\n", __func__, (unsigned int)(phy_id + 1U));
				break;
			}

			port_bw = (bw_val * mtk_crtc->usage_ovl_fmt[phy_id + 1]) >> 2;
			if (comp->last_hrt_bw_other != port_bw) {
				DDPQOS("%s/%u,layer:%u update:%u->%u compress:%d bw:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id, comp->last_hrt_bw_other,
					port_bw, mtk_crtc->usage_ovl_compr[phy_id + 1], bw_val);
				__mtk_disp_set_module_hrt(comp->hrt_qos_req_other, comp->id, port_bw,
					priv->data->respective_ostdl);
				comp->last_hrt_bw_other = port_bw;
			}
			if (port_bw && mtk_crtc->usage_ovl_compr[phy_id + 1])
				total_bw += port_bw;
		}

		if (!IS_ERR_OR_NULL(comp->hdr_qos_req)) {
			if (total_bw)
				hdr_bw = (total_bw > 32) ? total_bw / 32 : 1;

			if (comp->last_hdr_bw != hdr_bw) {
				DDPQOS("%s/%u,layer:%u update hdr:%u->%u total:%u bw:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hdr_bw, hdr_bw, total_bw, bw_val);
				__mtk_disp_set_module_hrt(comp->hdr_qos_req, comp->id, hdr_bw,
					priv->data->respective_ostdl);
				comp->last_hdr_bw = hdr_bw;
			}
		}

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_SET_HRT_BW_DELAY: {
		u32 bw_val = *(unsigned int *)params;
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
		unsigned int phy_id = 0, port_bw = bw_val, total_bw = 0, hdr_bw = 0;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl)
			break;

		if (!handle) {
			DDPPR_ERR("no cmdq handle\n");
			break;
		}

		if (ovl->data->ovl_phy_mapping)
			phy_id = ovl->data->ovl_phy_mapping(comp);

		if (phy_id >= MAX_LAYER_NR) {
			DDPINFO("%s,invalid layer:%u\n", __func__, phy_id);
			break;
		}

		if (!IS_ERR_OR_NULL(comp->hrt_qos_req)) {
			port_bw = (bw_val * mtk_crtc->usage_ovl_fmt[phy_id]) >> 2;
			if (port_bw > comp->last_hrt_bw) {
				DDPQOS("%s/%u,layer:%u fast up:%u->%u compress:%d\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hrt_bw, port_bw, mtk_crtc->usage_ovl_compr[phy_id]);
				__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, port_bw,
					priv->data->respective_ostdl);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_BW_VAL(phy_id)),
					NO_PENDING_HRT, ~0);
			} else if (port_bw < comp->last_hrt_bw) {
				DDPQOS("%s/%u,layer:%u slow down:%u->%u compress:%d\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hrt_bw, port_bw, mtk_crtc->usage_ovl_compr[phy_id]);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_BW_VAL(phy_id)),
					port_bw, ~0);
			}
			comp->last_hrt_bw = port_bw;
			if (port_bw && mtk_crtc->usage_ovl_compr[phy_id])
				total_bw += port_bw;
		}

		if (!IS_ERR_OR_NULL(comp->hrt_qos_req_other)) {
			if (phy_id + 1 >= MAX_LAYER_NR) {
				DDPINFO("%s,invalid layer:%u\n", __func__, (unsigned int)(phy_id + 1U));
				break;
			}

			port_bw = (bw_val * mtk_crtc->usage_ovl_fmt[phy_id + 1]) >> 2;
			if (port_bw > comp->last_hrt_bw_other) {
				DDPQOS("%s/%u,layer:%u fast up:%u->%u compress:%d\n",
					mtk_dump_comp_str_id(comp->id), comp->id, (unsigned int)(phy_id + 1U),
					comp->last_hrt_bw_other, port_bw, mtk_crtc->usage_ovl_compr[phy_id + 1]);
				__mtk_disp_set_module_hrt(comp->hrt_qos_req_other, comp->id, port_bw,
					priv->data->respective_ostdl);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_BW_VAL(phy_id + 1)),
					NO_PENDING_HRT, ~0);
			} else if (port_bw < comp->last_hrt_bw_other) {
				DDPQOS("%s/%u,layer:%u slow down:%u->%u compress:%d\n",
					mtk_dump_comp_str_id(comp->id), comp->id, (unsigned int)(phy_id + 1U),
					comp->last_hrt_bw_other, port_bw, mtk_crtc->usage_ovl_compr[phy_id + 1]);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_BW_VAL(phy_id + 1)),
					port_bw, ~0);
			}
			comp->last_hrt_bw_other = port_bw;
			if (port_bw && mtk_crtc->usage_ovl_compr[phy_id + 1])
				total_bw += port_bw;
		}

		if (!IS_ERR_OR_NULL(comp->hdr_qos_req)) {
			if (total_bw)
				hdr_bw = (total_bw > 32) ? total_bw / 32 : 1;

			if (hdr_bw > comp->last_hdr_bw) {
				DDPQOS("%s/%u,layer:%u hdr fast up:%u->%u total:%u bw:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hdr_bw, hdr_bw, total_bw, bw_val);
				__mtk_disp_set_module_hrt(comp->hdr_qos_req, comp->id, hdr_bw,
					priv->data->respective_ostdl);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HDR_BW_VAL(phy_id)),
					NO_PENDING_HRT, ~0);
			} else if (hdr_bw < comp->last_hdr_bw) {
				DDPQOS("%s/%u,layer:%u hdr slow down:%u->%u total:%u bw:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hdr_bw, hdr_bw, total_bw, bw_val);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HDR_BW_VAL(phy_id)),
					hdr_bw, ~0);
			}
			comp->last_hdr_bw = hdr_bw;
		}

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_SET_HRT_BW_DELAY_POST: {
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
		unsigned int phy_id = 0, port_bw = 0, hdr_bw = 0;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			DDPPR_ERR("respective_ostdl do not set\n");
			break;
		}

		if (ovl->data->ovl_phy_mapping)
			phy_id = ovl->data->ovl_phy_mapping(comp);

		if (phy_id >= MAX_LAYER_NR) {
			DDPINFO("%s,invalid layer:%u\n", __func__, phy_id);
			break;
		}

		if (!IS_ERR_OR_NULL(comp->hrt_qos_req)) {
			port_bw = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_BW_VAL(phy_id));
			if (port_bw != NO_PENDING_HRT && port_bw <= comp->last_hrt_bw) {
				DDPQOS("%s/%u,layer:%u final down:%u slot:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hrt_bw, port_bw);
				__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, comp->last_hrt_bw,
					priv->data->respective_ostdl);
				*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
					DISP_SLOT_CUR_BW_VAL(phy_id)) =	NO_PENDING_HRT;
			}
		}
		if (!IS_ERR_OR_NULL(comp->hrt_qos_req_other)) {
			if (phy_id + 1 >= MAX_LAYER_NR) {
				DDPINFO("%s,invalid layer:%u\n", __func__, (unsigned int)(phy_id + 1U));
				break;
			}

			port_bw = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_BW_VAL(phy_id + 1));
			if (port_bw != NO_PENDING_HRT && port_bw <= comp->last_hrt_bw_other) {
				DDPQOS("%s/%u,layer:%u final down:%u slot:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, (unsigned int)(phy_id + 1U),
					comp->last_hrt_bw_other, port_bw);
				__mtk_disp_set_module_hrt(comp->hrt_qos_req_other, comp->id, comp->last_hrt_bw_other,
					priv->data->respective_ostdl);
				*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
					DISP_SLOT_CUR_BW_VAL(phy_id + 1)) =	NO_PENDING_HRT;
			}
		}

		if (!IS_ERR_OR_NULL(comp->hdr_qos_req)) {
			hdr_bw = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_HDR_BW_VAL(phy_id));
			if (hdr_bw != NO_PENDING_HRT && hdr_bw <= comp->last_hdr_bw) {
				DDPQOS("%s/%u,layer:%u hdr final down:%u slot:%u\n",
					mtk_dump_comp_str_id(comp->id), comp->id, phy_id,
					comp->last_hdr_bw, hdr_bw);
				__mtk_disp_set_module_hrt(comp->hdr_qos_req, comp->id, comp->last_hdr_bw,
					priv->data->respective_ostdl);
				*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
					DISP_SLOT_CUR_HDR_BW_VAL(phy_id)) = NO_PENDING_HRT;
			}
		}

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_UPDATE_BW: {
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		unsigned int force_update = 0; /* force_update repeat last qos BW */
		unsigned int update_pending = 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		mtk_crtc = comp->mtk_crtc;
		crtc = &mtk_crtc->base;

		/* process FBDC */
		/* qos BW only has one port for one device, no need to separate */
		//__mtk_disp_set_module_srt(comp->fbdc_qos_req, comp->id, comp->fbdc_bw, 0,
		//			    DISP_BW_FBDC_MODE, priv->data->real_srt_ostdl);

		if (params) {
			force_update = *(unsigned int *)params;
			/* tricky way use variable force update */
			update_pending = (force_update == DISP_BW_UPDATE_PENDING);
			force_update = (force_update == DISP_BW_FORCE_UPDATE) ? 1 : 0;
		}

		/* update total srt BW: case1:fast up, case2:slow down*/
		if (!force_update && !update_pending) {
			if (comp->qos_bw) {
				DDPDBG("ovl:%u, update total srt bw:%u->%u\n",
					comp->id, mtk_crtc->total_srt,
					(unsigned int)(mtk_crtc->total_srt + comp->qos_bw));
				mtk_crtc->total_srt += comp->qos_bw;
			}
			if (!IS_ERR_OR_NULL(comp->qos_req_other) && comp->qos_bw_other) {
				DDPDBG("ovl:%u, update total srt bw other:%u->%u\n",
					comp->id, mtk_crtc->total_srt,
					(unsigned int)(mtk_crtc->total_srt + comp->qos_bw_other));
				mtk_crtc->total_srt += comp->qos_bw_other;
			}
		}

		/* process normal */
		if (!force_update && comp->last_qos_bw == comp->qos_bw) {
			if (comp->qos_bw)
				DDPDBG("ovl:%u, IGNORE srt bw:%u->%u @%s\n",
					comp->id, comp->last_qos_bw, comp->qos_bw,
					update_pending ? "FINAL DOWN" : "FAST UP");
			if (IS_ERR_OR_NULL(comp->qos_req_other))
				break;
			else if (comp->last_qos_bw_other == comp->qos_bw_other) {
				if (comp->qos_bw_other)
					DDPDBG("ovl:%u, IGNORE srt bw other:%u->%u @%s\n",
						comp->id, comp->last_qos_bw_other, comp->qos_bw_other,
						update_pending ? "FINAL DOWN" : "FAST UP");
				break;
			}
			goto other;
		}

		/* update srt bw: case1:force update, case2:fast up, case3:final down */
		if (force_update ||
			(comp->last_qos_bw < comp->qos_bw) ||
			((comp->last_qos_bw > comp->qos_bw) && update_pending)) {
			__mtk_disp_set_module_srt(comp->qos_req, comp->id, comp->qos_bw, 0,
					DISP_BW_NORMAL_MODE, priv->data->real_srt_ostdl);
			DDPQOS("ovl:%u %s srt bw:%u->%u\n", comp->id,
				force_update ? "FORCE UPDATE" : (update_pending ? "FINAL DOWN" : "FAST UP"),
				comp->last_qos_bw, comp->qos_bw);
			comp->last_qos_bw = comp->qos_bw;
		} else if (comp->last_qos_bw > comp->qos_bw)
			DDPDBG("ovl:%u, SLOW DOWN srt bw:%u->%u\n",
				comp->id, comp->last_qos_bw, comp->qos_bw);

other:
		/* update other srt bw: case1:force update, case2:fast up, case3:final down */
		if (!IS_ERR_OR_NULL(comp->qos_req_other)) {
			if (force_update ||
				(comp->last_qos_bw_other < comp->qos_bw_other) ||
				((comp->last_qos_bw_other > comp->qos_bw_other) && update_pending)) {
				__mtk_disp_set_module_srt(comp->qos_req_other, comp->id, comp->qos_bw_other, 0,
					DISP_BW_NORMAL_MODE, priv->data->real_srt_ostdl);
				DDPQOS("ovl:%u %s srt bw other:%u->%u\n", comp->id,
					force_update ? "FORCE UPDATE" : (update_pending ? "FINAL DOWN" : "FAST UP"),
					comp->last_qos_bw_other, comp->qos_bw_other);
				comp->last_qos_bw_other = comp->qos_bw_other;
			} else if (comp->last_qos_bw_other > comp->qos_bw_other)
				DDPDBG("ovl:%u, SLOW DOWN srt bw other:%u->%u\n",
					comp->id, comp->last_qos_bw_other, comp->qos_bw_other);
		}
		break;
	}
	case OVL_REPLACE_BOOTUP_MVA: {
		struct mtk_ddp_fb_info *fb_info =
			(struct mtk_ddp_fb_info *)params;

		mtk_ovl_replace_bootup_mva(comp, handle, params, fb_info);
		if (priv->data->mmsys_id == MMSYS_MT6989 ||
			priv->data->mmsys_id == MMSYS_MT6899)
			iommu_dev_disable_feature(comp->dev, IOMMU_DEV_FEAT_BYPASS_S1);
		break;
	}
	case BACKUP_INFO_CMP: {
		mtk_ovl_backup_info_cmp(comp, params);
		break;
	}
	case BACKUP_OVL_STATUS: {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		dma_addr_t slot = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_OVL_STATUS);

		cmdq_pkt_mem_move(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_STA,
			slot, CMDQ_THR_SPR_IDX3);
		break;
	}
	case OVL_GET_SOURCE_BPC: {
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

		if (ovl && ovl->data) {
			DDPINFO("%s, source_bpc[%d]\n", __func__, ovl->data->source_bpc);
			return ovl->data->source_bpc;
		}
		break;
	}
	case OVL_GET_SELFLOOP_SUPPORT: {
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

		if (ovl && ovl->data) {
			DDPINFO("%s, support_pq_selfloop[%d]\n", __func__,
				ovl->data->support_pq_selfloop);
			return ovl->data->support_pq_selfloop;
		}
		break;
	}
	case OVL_SET_PQ_OUT: {
		struct mtk_addon_config_type *c = (struct mtk_addon_config_type *)params;
		u32 value = 0, mask = 0;

		if (c->module == OVL_RSZ)
			SET_VAL_MASK(value, mask, 1, DISP_OVL_PQ_OUT_OPT);

		if (c->module != DISP_MML_DL)
			SET_VAL_MASK(value, mask, 1, DISP_OVL_PQ_OUT_EN);

		SET_VAL_MASK(value, mask, c->tgt_layer, DATAPATH_PQ_OUT_SEL);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			       value, mask);

		cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_OVL_PQ_LOOP_CON,
			       DISP_OVL_PQ_OUT_SIZE_SEL, DISP_OVL_PQ_OUT_SIZE_SEL);
		break;
	}
	case OVL_FRAME_DONE_EVENT: {
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

		if (ovl->data->frame_done_event) {
			DDPINFO("%s, %d, comp id:%d, event:%d\n", __func__, __LINE__,
				comp->id, ovl->data->frame_done_event(comp));
			return ovl->data->frame_done_event(comp);
		} else {
			DDPMSG("%s, %d, invalid comp\n", __func__, __LINE__);
			return U16_MAX;
		}

		break;
	}
	case GET_OVL_SYS_NUM: {
		struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

		if (ovl->data->ovlsys_mapping) {
			DDPINFO("%s, %d, comp id:%d, ovlsys:%d\n", __func__, __LINE__,
				comp->id, ovl->data->ovlsys_mapping(comp));
			return ovl->data->ovlsys_mapping(comp);
		} else {
			DDPMSG("%s, %d, invalid comp\n", __func__, __LINE__);
			return -1;
		}

		break;
	}
	case OVL_PHY_USAGE: {
		struct mtk_plane_state *plane_state;
		struct mtk_drm_crtc *mtk_crtc;

		plane_state = (struct mtk_plane_state *)params;
		mtk_crtc = comp->mtk_crtc;

		mtk_ovl_update_hrt_usage(mtk_crtc, comp, plane_state);

		break;
	}
	default:
		break;
	}

	return ret;
}

void mtk_ovl_dump_golden_setting(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned long rg0 = 0, rg1 = 0, rg2 = 0, rg3 = 0, rg4 = 0;
	int i = 0;
	unsigned int value;

	DDPDUMP("-- %s Golden Setting --\n", mtk_dump_comp_str(comp));
	for (i = 0; i < mtk_ovl_layer_num(comp); i++) {
		rg0 = DISP_REG_OVL_RDMA0_MEM_GMC_SETTING
			+ i * OVL_LAYER_OFFSET;
		rg1 = DISP_REG_OVL_RDMA0_FIFO_CTRL + i * OVL_LAYER_OFFSET;
		rg2 = DISP_REG_OVL_RDMA0_MEM_GMC_S2 + i * 0x4;
		rg3 = DISP_REG_OVL_RDMAn_BUF_LOW(i);
		rg4 = DISP_REG_OVL_RDMAn_BUF_HIGH(i);
		DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
			rg0, readl(rg0 + baddr), rg1, readl(rg1 + baddr),
			rg2, readl(rg2 + baddr));
		DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x\n",
			rg3, readl(rg3 + baddr),
			rg4, readl(rg4 + baddr));
	}

	rg0 = DISP_REG_OVL_RDMA_BURST_CON1;
	DDPDUMP("0x%03lx:0x%08x\n", rg0, readl(rg0 + baddr));

	rg0 = DISP_REG_OVL_RDMA_GREQ_NUM;
	rg1 = DISP_REG_OVL_RDMA_GREQ_URG_NUM;
	rg2 = DISP_REG_OVL_RDMA_ULTRA_SRC;
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr),
		rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));

	rg0 = DISP_REG_OVL_EN;
	rg1 = DISP_REG_OVL_DATAPATH_CON;
	rg2 = DISP_REG_OVL_FBDC_CFG1;
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr),
		rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));

	value = readl(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING + baddr);
	DDPDUMP("RDMA0_MEM_GMC_SETTING1\n");
	DDPDUMP("[9:0]:%x [25:16]:%x [28]:%x [31]:%x\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD_HIGH_OFS, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD_HIGH_OFS,
			value));

	value = readl(DISP_REG_OVL_RDMA0_FIFO_CTRL + baddr);
	DDPDUMP("RDMA0_FIFO_CTRL\n");
	DDPDUMP("[9:0]:%u [25:16]:%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_FIFO_THRD, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_FIFO_SIZE, value));

	value = readl(DISP_REG_OVL_RDMA0_MEM_GMC_S2 + baddr);
	DDPDUMP("RDMA0_MEM_GMC_SETTING2\n");
	DDPDUMP("[11:0]:%u [27:16]:%u [28]:%u [29]:%u [30]:%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES, value));

	value = readl(DISP_REG_OVL_RDMA_BURST_CON1 + baddr);
	DDPDUMP("OVL_RDMA_BURST_CON1\n");
	DDPDUMP("[28]:%u[30]:%u[31]:%u\n",
		REG_FLD_VAL_GET(FLD_RDMA_BURST_CON1_BURST16_EN, value),
		REG_FLD_VAL_GET(FLD_RDMA_BURST_CON1_DDR_EN, value),
		REG_FLD_VAL_GET(FLD_RDMA_BURST_CON1_DDR_ACK_EN, value));

	value = readl(DISP_REG_OVL_RDMA_GREQ_NUM + baddr);
	DDPDUMP("RDMA_GREQ_NUM\n");
	DDPDUMP("[3:0]%u [7:4]%u [11:8]%u [15:12]%u [23:16]%x [26:24]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT, value));
	DDPDUMP("[27]%u [28]%u [29]%u [30]%u [31]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_STOP_EN, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_GRP_END_STOP, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_GRP_BRK_STOP, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA, value));

	value = readl(DISP_REG_OVL_RDMA_GREQ_URG_NUM + baddr);
	DDPDUMP("RDMA_GREQ_URG_NUM\n");
	DDPDUMP("[3:0]:%u [7:4]:%u [11:8]:%u [15:12]:%u [25:16]:%u [28]:%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_ARG_URG_BIAS, value));

	value = readl(DISP_REG_OVL_RDMA_ULTRA_SRC + baddr);
	DDPDUMP("RDMA_ULTRA_SRC\n");
	DDPDUMP("[1:0]%u [3:2]%u [5:4]%u [7:6]%u [9:8]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_BUF_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_SMI_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_ROI_END_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_RDMA_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_BUF_SRC, value));
	DDPDUMP("[11:10]%u [13:12]%u [15:14]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_SMI_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_ROI_END_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_RDMA_SRC, value));

	value = readl(DISP_REG_OVL_RDMAn_BUF_LOW(0) + baddr);
	DDPDUMP("RDMA0_BUF_LOW\n");
	DDPDUMP("[11:0]:%x [23:12]:%x\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_LOW_ULTRA_TH, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH, value));

	value = readl(DISP_REG_OVL_RDMAn_BUF_HIGH(0) + baddr);
	DDPDUMP("RDMA0_BUF_HIGH\n");
	DDPDUMP("[23:12]:%x [31]:%x\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS, value));

	value = readl(DISP_REG_OVL_EN + baddr);
	DDPDUMP("OVL_EN\n");
	DDPDUMP("[18]:%x [19]:%x\n",
		REG_FLD_VAL_GET(EN_FLD_BLOCK_EXT_ULTRA, value),
		REG_FLD_VAL_GET(EN_FLD_BLOCK_EXT_PREULTRA, value));

	value = readl(DISP_REG_OVL_DATAPATH_CON + baddr);
	DDPDUMP("DATAPATH_CON\n");
	DDPDUMP("[0]:%u [24]:%u [25]:%u [26]:%u\n",
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_LAYER_SMI_ID_EN, value),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_GCLAST_EN, value),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_HDR_GCLAST_EN, value),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_OUTPUT_CLAMP, value));

	value = readl(DISP_REG_OVL_FBDC_CFG1 + baddr);
	DDPDUMP("OVL_FBDC_CFG1\n");
	DDPDUMP("[24]:%u [28]:%u\n",
		REG_FLD_VAL_GET(FLD_FBDC_8XE_MODE, value),
		REG_FLD_VAL_GET(FLD_FBDC_FILTER_EN, value));

	value = readl(DISP_REG_OVL_STASH_CFG0 + baddr);
	DDPDUMP("OVL_STASH_CFG0:%x\n", value);
	value = readl(DISP_REG_OVL_STASH_CFG1 + baddr);
	DDPDUMP("OVL_STASH_CFG1:%x\n", value);

}

int mtk_ovl_dump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	void __iomem *baddr = comp->regs;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	if (mtk_ddp_comp_helper_get_opt(comp,
					MTK_DRM_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned int i = 0;

		for (i = 0; i < 0xFF0; i += 0x10)
			mtk_serial_dump_reg(baddr, i, 4);

		/* ADDR */
		mtk_cust_dump_reg(baddr, 0xF40, 0xF60, 0xF80, 0xFA0);
		mtk_cust_dump_reg(baddr, 0xFB0, 0xFB4, 0xFB8, -1);

		/* HDR_ADDR, HDR_PITCH */
		mtk_cust_dump_reg(baddr, 0xF44, 0xF48, 0xF64, 0xF68);
		mtk_cust_dump_reg(baddr, 0xF84, 0xF88, 0xFA4, 0xFA8);
		mtk_cust_dump_reg(baddr, 0xFD0, 0xFD4, 0xFD8, 0xFDC);
		mtk_cust_dump_reg(baddr, 0xFE0, 0xFE4, -1, -1);

		/* ADDR MSB*/
		if (ovl->data->is_support_34bits) {
			mtk_cust_dump_reg(baddr, 0xF4C, 0xF6C, 0xF8C, 0xFAC);
			mtk_cust_dump_reg(baddr, 0xFBC, 0xFCC, 0xFDC, -1);
		}
	} else {
		DDPDUMP("0x8f0:0x%x\n", readl(DISP_REG_OVL_SMI_2ND_CFG + baddr));
		DDPDUMP("0x740:0x%x\n", readl(OVL_ROI_TIMING_0 + baddr));
		/* STA, INTEN, INTSTA, EN*/
		mtk_serial_dump_reg(baddr, 0x0, 4);

		/* TRIG, RST */
		mtk_serial_dump_reg(baddr, 0x10, 2);

		/* ROI_SIZE, DATAPATH_CON, OVL_ROI_BGCLR, OVL_SRC_CON */
		mtk_serial_dump_reg(baddr, 0x20, 4);

		/* LX_CON... */
		for (i = 0; i < 4; i++) {
			mtk_serial_dump_reg(baddr, 0x30 + i * 0x20, 4);
			mtk_serial_dump_reg(baddr, 0x40 + i * 0x20, 4);
		}
		mtk_cust_dump_reg(baddr, 0xF40, 0xF60, 0xF80, 0xFA0);

		/* RDMAX_CON... */
		for (i = 0; i < 4; i++) {
			unsigned int off = 0xC0 + 0x20 * i;

			mtk_cust_dump_reg(baddr, off, off + 0x8, off + 0xC,
					  off + 0x10);
		}

		mtk_serial_dump_reg(baddr, 0x200, 4);
		/* debug monitor */
		mtk_serial_dump_reg(baddr, 0x230, 4);
		mtk_serial_dump_reg(baddr, 0x240, 4);
		mtk_serial_dump_reg(baddr, 0x250, 4);
		/* LC_CON */
		mtk_serial_dump_reg(baddr, 0x280, 4);

		mtk_serial_dump_reg(baddr, 0x2a0, 2);

		/* CLRFMT, WCG */
		mtk_serial_dump_reg(baddr, 0x2D0, 4);

		/* PQ LOOP */
		mtk_serial_dump_reg(baddr, 0x2E0, 2);

		/* DATAPATH_EXT_CON */
		mtk_serial_dump_reg(baddr, 0x324, 1);

		/* OVL_ELX_CON */
		for (i = 0; i < 3; i++) {
			mtk_serial_dump_reg(baddr, 0x330 + i * 0x20, 4);
			mtk_serial_dump_reg(baddr, 0x340 + i * 0x20, 4);
		}
		mtk_cust_dump_reg(baddr, 0xFB0, 0xFB4, 0xFB8, -1);

		/* SBCH */
		mtk_serial_dump_reg(baddr, 0x3A0, 3);

		/* WCG */
		for (i = 0 ; i < 4 ; i++) {
			mtk_serial_dump_reg(baddr, 0x500 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x510 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x520 + i * 40, 1);
		}

		for (i = 0 ; i < 3 ; i++) {
			mtk_serial_dump_reg(baddr, 0x600 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x610 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x620 + i * 40, 1);
		}

		/* FBDC */
		mtk_serial_dump_reg(baddr, 0x800, 3);

		/*SYSRAM*/
		mtk_serial_dump_reg(baddr, 0x880, 4);
		mtk_serial_dump_reg(baddr, 0x890, 4);
		mtk_serial_dump_reg(baddr, 0x8a0, 4);
		mtk_serial_dump_reg(baddr, 0x8b0, 4);
		mtk_serial_dump_reg(baddr, 0x900, 4);
		mtk_serial_dump_reg(baddr, 0x910, 4);
		mtk_serial_dump_reg(baddr, 0x920, 4);
		mtk_serial_dump_reg(baddr, 0x930, 4);

		/* BW Monitor */
		mtk_serial_dump_reg(baddr, 0x940, 4);
		mtk_serial_dump_reg(baddr, 0x950, 4);
		mtk_serial_dump_reg(baddr, 0x960, 4);
		mtk_serial_dump_reg(baddr, 0x970, 4);

		for (i = 0; i < 4; i++)
			mtk_serial_dump_reg(baddr, 0xF44 + i * 0x20, 2);
		for (i = 0; i < 3; i++)
			mtk_serial_dump_reg(baddr, 0xFD0 + i * 0x8, 2);

		/* ADDR MSB*/
		if (ovl->data->is_support_34bits) {
			mtk_cust_dump_reg(baddr, 0xF4C, 0xF6C, 0xF8C, 0xFAC);
			mtk_cust_dump_reg(baddr, 0xFBC, 0xFCC, 0xFDC, -1);
		}
	}
	/* For debug MPU violation issue */
	mtk_cust_dump_reg(baddr, 0xFC0, 0xFC4, 0xFC8, -1);

	mtk_ovl_dump_golden_setting(comp);

	return 0;
}

static void ovl_printf_status(unsigned int status)
{
	DDPDUMP("- OVL_FLOW_CONTROL_DEBUG -\n");
	DDPDUMP("addcon_idle:%d,blend_idle:%d\n",
		(status >> 10) & (0x1), (status >> 11) & (0x1));
	DDPDUMP("out_valid:%d,out_ready:%d,out_idle:%d\n",
		(status >> 12) & (0x1), (status >> 13) & (0x1),
		(status >> 15) & (0x1));
	DDPDUMP("rdma_idle3-0:(%d,%d,%d,%d),rst:%d\n", (status >> 16) & (0x1),
		(status >> 17) & (0x1), (status >> 18) & (0x1),
		(status >> 19) & (0x1), (status >> 20) & (0x1));
	DDPDUMP("trig:%d,frame_hwrst_done:%d\n",
		(status >> 21) & (0x1), (status >> 23) & (0x1));
	DDPDUMP("frame_swrst_done:%d,frame_underrun:%d,frame_done:%d\n",
		(status >> 24) & (0x1), (status >> 25) & (0x1),
		(status >> 26) & (0x1));
	DDPDUMP("ovl_running:%d,ovl_start:%d,ovl_clr:%d\n",
		(status >> 27) & (0x1), (status >> 28) & (0x1),
		(status >> 29) & (0x1));
	DDPDUMP("reg_update:%d,ovl_upd_reg:%d\n",
		(status >> 30) & (0x1),
		(status >> 31) & (0x1));

	DDPDUMP("ovl_fms_state:\n");
	switch (status & 0x3ff) {
	case 0x1:
		DDPDUMP("idle\n");
		break;
	case 0x2:
		DDPDUMP("wait_SOF\n");
		break;
	case 0x4:
		DDPDUMP("prepare\n");
		break;
	case 0x8:
		DDPDUMP("reg_update\n");
		break;
	case 0x10:
		DDPDUMP("eng_clr(internal reset)\n");
		break;
	case 0x20:
		DDPDUMP("eng_act(processing)\n");
		break;
	case 0x40:
		DDPDUMP("h_wait_w_rst\n");
		break;
	case 0x80:
		DDPDUMP("s_wait_w_rst\n");
		break;
	case 0x100:
		DDPDUMP("h_w_rst\n");
		break;
	case 0x200:
		DDPDUMP("s_w_rst\n");
		break;
	default:
		DDPDUMP("ovl_fsm_unknown\n");
		break;
	}
}

static void ovl_print_ovl_rdma_status(unsigned int status)
{
	DDPDUMP("warm_rst_cs:%d,layer_greq:%d,out_data:0x%x\n", status & 0x7,
		(status >> 3) & 0x1, (status >> 4) & 0xffffff);
	DDPDUMP("out_ready:%d,out_valid:%d,smi_busy:%d,smi_greq:%d\n",
		(status >> 28) & 0x1, (status >> 29) & 0x1,
		(status >> 30) & 0x1, (status >> 31) & 0x1);
}

static void ovl_dump_layer_info_compress(struct mtk_ddp_comp *comp, int layer,
					 bool is_ext_layer)
{
	unsigned int compr_en = 0, pitch_msb, pitch;
	void __iomem *baddr = comp->regs;
	dma_addr_t addr;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (is_ext_layer) {
		compr_en = DISP_REG_GET_FIELD(
			REG_FLD(1, layer + 4),
			baddr + DISP_REG_OVL_DATAPATH_EXT_CON);
		addr = read_ext_layer_hdr_addr(comp, layer);
		pitch_msb = readl(baddr + DISP_REG_OVL_EL_PITCH_MSB(layer));
		pitch = readl(baddr + DISP_REG_OVL_ELX_HDR_PITCH(ovl, layer));
	} else {
		compr_en =
			DISP_REG_GET_FIELD(REG_FLD(1, layer + 4),
					   baddr + DISP_REG_OVL_DATAPATH_CON);
		addr = read_phy_layer_hdr_addr(comp, layer);
		pitch_msb = readl(baddr + DISP_REG_OVL_PITCH_MSB(layer));
		pitch = readl(baddr + DISP_REG_OVL_LX_HDR_PITCH(layer));
	}

	if (compr_en == 0) {
		DDPDUMP("compr_en:%u\n", compr_en);
		return;
	}

	DDPDUMP("compr_en:%u, pitch_msb:0x%x, hdr_addr:0x%lx, hdr_pitch:0x%x\n",
		compr_en, pitch_msb, (unsigned long)addr, pitch);
}

static void ovl_dump_layer_info(struct mtk_ddp_comp *comp, int layer,
				bool is_ext_layer)
{
	unsigned int con, src_size, offset, pitch, clip;
	/*  enum UNIFIED_COLOR_FMT fmt; */
	void __iomem *baddr = comp->regs;
	void __iomem *Lx_base;
	dma_addr_t addr;
	static const char * const pixel_src[] = { "mem", "color", "ufod", "pq" };

	if (is_ext_layer) {
		Lx_base = baddr + layer * OVL_LAYER_OFFSET;
		Lx_base += (DISP_REG_OVL_EL_CON(0) - DISP_REG_OVL_CON(0));
		addr = read_ext_layer_addr(comp, layer);
	} else {
		Lx_base = baddr + layer * OVL_LAYER_OFFSET;
		addr = read_phy_layer_addr(comp, layer);
	}

	con = readl(DISP_REG_OVL_CON(0) + Lx_base);
	offset = readl(DISP_REG_OVL_L0_OFFSET + Lx_base);
	src_size = readl(DISP_REG_OVL_L0_SRC_SIZE + Lx_base);
	pitch = readl(DISP_REG_OVL_L0_PITCH + Lx_base);
	clip = readl(DISP_REG_OVL_CLIP(0) + Lx_base);

	/* TODO
	 * fmt = display_fmt_reg_to_unified_fmt(
	 * REG_FLD_VAL_GET(L_CON_FLD_CFMT, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_BTSW, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, con));
	 */
	DDPDUMP("%s_L%d:(%u,%u,%ux%u)\n",
		is_ext_layer ? "ext" : "phy", layer, offset & 0xfff,
		(offset >> 16) & 0xfff, src_size & 0xfff,
		(src_size >> 16) & 0xfff);
	DDPDUMP("pitch=%u,addr=0x%lx,source=%s,aen=%u,alpha=%u,cl=0x%x\n",
		pitch & 0xffff,
		(unsigned long)addr, /* unified_color_fmt_name(fmt),*/
		pixel_src[REG_FLD_VAL_GET(L_CON_FLD_LSRC, con)],
		REG_FLD_VAL_GET(L_CON_FLD_AEN, con),
		REG_FLD_VAL_GET(L_CON_FLD_APHA, con),
		clip);
	DDPDUMP("L0_SYSRAM_CFG:0x%x, L0_SYSRAM_BUF0:0x%x, L0_SYSRAM_BUF1:0x%x\n",
			readl(comp->regs + 0x880),
			readl(comp->regs + 0x884),
			readl(comp->regs + 0x888));

	ovl_dump_layer_info_compress(comp, layer, is_ext_layer);
}

void mtk_ovl_cur_pos_dump(struct mtk_ddp_comp *comp)
{
	unsigned int addcon;
	void __iomem *baddr;

	if (!comp || comp->blank_mode)
		return;
	baddr = comp->regs;
	if (!baddr) {
		DDPINFO("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	addcon = readl(DISP_REG_OVL_ADDCON_DBG + baddr);

	DDPMSG("%s cur_pos(%u,%u)\n", mtk_dump_comp_str(comp),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_X, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_Y, addcon));
}

int mtk_ovl_analysis(struct mtk_ddp_comp *comp)
{
	int i = 0;
	void __iomem *Lx_base;
	void __iomem *rdma_offset;
	void __iomem *baddr = comp->regs;
	unsigned int src_con;
	unsigned int ext_con;
	unsigned int addcon;
	unsigned int path_con;
	unsigned int pq_out_size;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	src_con = readl(DISP_REG_OVL_SRC_CON + baddr);
	ext_con = readl(DISP_REG_OVL_DATAPATH_EXT_CON + baddr);
	addcon = readl(DISP_REG_OVL_ADDCON_DBG + baddr);
	path_con = readl(DISP_REG_OVL_DATAPATH_CON + baddr);
	pq_out_size = readl(DISP_REG_OVL_PQ_OUT_SIZE + baddr);

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("ovl_en=%d,layer_en(%d,%d,%d,%d),bg(%dx%d)\n",
		readl(DISP_REG_OVL_EN + baddr) & 0x1, src_con & 0x1,
		(src_con >> 1) & 0x1, (src_con >> 2) & 0x1,
		(src_con >> 3) & 0x1,
		readl(DISP_REG_OVL_ROI_SIZE + baddr) & 0xfff,
		(readl(DISP_REG_OVL_ROI_SIZE + baddr) >> 16) & 0xfff);
	DDPDUMP("ext_layer:layer_en(%d,%d,%d),attach_layer(%d,%d,%d)\n",
		ext_con & 0x1, (ext_con >> 1) & 0x1, (ext_con >> 2) & 0x1,
		(ext_con >> 16) & 0xf, (ext_con >> 20) & 0xf,
		(ext_con >> 24) & 0xf);
	DDPDUMP("cur_pos(%u,%u),layer_hit(%u,%u,%u,%u),bg_mode=%s,sta=0x%x\n",
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_X, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_Y, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L0_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L1_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L2_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L3_WIN_HIT, addcon),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_BGCLR_IN_SEL, path_con) ? "DL" : "const",
		readl(DISP_REG_OVL_STA + baddr));
	DDPDUMP("pq_out_en(%u),sel(%u),size(%u,%u),pq_out_opt(%u)\n",
		REG_FLD_VAL_GET(DISP_OVL_PQ_OUT_EN, path_con),
		REG_FLD_VAL_GET(DATAPATH_PQ_OUT_SEL, path_con),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_X, pq_out_size),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_Y, pq_out_size),
		REG_FLD_VAL_GET(DISP_OVL_PQ_OUT_OPT, path_con));

	/* phy layer */
	for (i = 0; i < mtk_ovl_layer_num(comp); i++) {
		unsigned int rdma_ctrl;

		if (src_con & (0x1 << i))
			ovl_dump_layer_info(comp, i, false);
		else
			DDPDUMP("phy_L%d:disabled\n", i);

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		rdma_ctrl = readl(Lx_base + DISP_REG_OVL_RDMA0_CTRL);
		DDPDUMP("ovl rdma%d status:(en=%d,fifo_used:%d,GMC=0x%x)\n", i,
			REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RDMA_EN, rdma_ctrl),
			REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RMDA_FIFO_USED_SZ,
					rdma_ctrl),
			readl(Lx_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));

		rdma_offset = i * OVL_RDMA_DEBUG_OFFSET + baddr;
		ovl_print_ovl_rdma_status(
			readl(DISP_REG_OVL_RDMA0_DBG + rdma_offset));
	}

	/* ext layer */
	for (i = 0; i < 3; i++) {
		if (ext_con & (0x1 << i))
			ovl_dump_layer_info(comp, i, true);
		else
			DDPDUMP("ext_L%d:disabled\n", i);
	}
	ovl_printf_status(readl(DISP_REG_OVL_FLOW_CTRL_DBG + baddr));

	return 0;
}

static void mtk_ovl_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(comp->dev);
	int ret;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	struct mtk_drm_private *dev_priv = NULL;

	mtk_ddp_comp_clk_prepare(comp);

	if (priv->fbdc_clk != NULL) {
		ret = clk_prepare_enable(priv->fbdc_clk);
		if (ret)
			DDPPR_ERR("clk prepare enable failed:%s\n",
				mtk_dump_comp_str(comp));
	}

	/* Bypass shadow register and read shadow register */
	if (ovl->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, DISP_OVL_BYPASS_SHADOW,
			DISP_REG_OVL_EN, DISP_OVL_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_REG_OVL_EN, DISP_OVL_BYPASS_SHADOW);

	if (comp->mtk_crtc && comp->mtk_crtc->base.dev) {
		dev_priv = comp->mtk_crtc->base.dev->dev_private;
		if (mtk_drm_helper_get_opt(dev_priv->helper_opt, MTK_DRM_OPT_LAYER_REC))
			writel(0xffffffff, comp->regs + DISP_OVL_REG_GDRDY_PRD);
	}
}

static void mtk_ovl_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(comp->dev);

	if (priv->fbdc_clk != NULL)
		clk_disable_unprepare(priv->fbdc_clk);

	mtk_ddp_comp_clk_unprepare(comp);
}

static void
mtk_ovl_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkt,
		       enum mtk_ddp_comp_trigger_flag flag)
{
	switch (flag) {
	case MTK_TRIG_FLAG_PRE_TRIGGER:
	{
	/* may cause side effect, need check
		if (comp->blank_mode)
			break;
		DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

		cmdq_pkt_write(pkt, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_RST, 0x1, 0x1);
		cmdq_pkt_write(pkt, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_RST, 0x0, 0x1);
	*/
		break;
	}
#ifdef IF_ZERO /* not ready for dummy register method */
	case MTK_TRIG_FLAG_LAYER_REC:
	{
		u32 offset = 0;
		struct cmdq_pkt_buffer *qbuf;

		int i = 0;
		const int lnr = mtk_ovl_layer_num(comp);
		u32 ln_con = 0, ln_size = 0;

		struct mtk_drm_private *priv = NULL;

		if (!comp->mtk_crtc)
			return;

		priv = comp->mtk_crtc->base.dev->dev_private;
		if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC))
			return;

		if (comp->id == DDP_COMPONENT_OVL0_2L)
			offset = DISP_SLOT_LAYER_REC_OVL0_2L;
		else if (comp->id == DDP_COMPONENT_OVL0)
			offset = DISP_SLOT_LAYER_REC_OVL0;
		else
			return;

		qbuf = &comp->mtk_crtc->gce_obj.buf;

		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_GDRDY_PRD_NUM,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_SRC_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		for (i = 0; i < lnr + 3; i++) {
			if (i < lnr) {
				ln_con = DISP_REG_OVL_CON(i);
				ln_size = DISP_REG_OVL_SRC_SIZE(i);
			} else {
				ln_con = DISP_REG_OVL_EL_CON(i - lnr);
				ln_size = DISP_REG_OVL_EL_SRC_SIZE(i - lnr);
			}

			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_con,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_size,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
		}

		if (comp->id == DDP_COMPONENT_OVL0_2L) {
			if (offset >= DISP_SLOT_LAYER_REC_OVL0)
				DDPMSG("%s:error:ovl0_2l:offset overflow:%u\n",
				       __func__, offset);
		} else if (comp->id == DDP_COMPONENT_OVL0) {
			if (offset >= DISP_SLOT_LAYER_REC_END)
				DDPMSG("%s:error:ovl0:offset overflow:%u\n",
				       __func__, offset);
		}

		break;
	}
#endif
	default:
		break;
	}
}

static int mtk_ovl_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, false);
	struct total_tile_overhead_v to_v_info;
	unsigned int overhead_v;

	DDPINFO("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	ovl->set_partial_update = enable;

	to_v_info = mtk_crtc_get_total_overhead_v(comp->mtk_crtc);
	overhead_v = to_v_info.overhead_v;

	if (comp->mtk_crtc->res_switch == RES_SWITCH_ON_AP &&
		comp->mtk_crtc->scaling_ctx.scaling_en)
		ovl->roi_height = to_v_info.in_height;
	else
		ovl->roi_height = partial_roi.height + (overhead_v * 2);

	DDPDBG("%s, %s overhead_v:%d, roi_height:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v, ovl->roi_height);

	if (ovl->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_ROI_SIZE,
				ovl->roi_height << 16, 0x1fff << 16);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_ROI_SIZE,
				full_height << 16, 0x1fff << 16);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_ovl_funcs = {
	.config = mtk_ovl_config,
	.first_cfg = mtk_ovl_config,
	.config_begin = mtk_ovl_config_begin,
	.start = mtk_ovl_start,
	.stop = mtk_ovl_stop,
	.reset = mtk_ovl_reset,
#ifdef IF_ZERO
	.enable_vblank = mtk_ovl_enable_vblank,
	.disable_vblank = mtk_ovl_disable_vblank,
#endif
	.layer_on = mtk_ovl_layer_on,
	.layer_off = mtk_ovl_layer_off,
	.layer_config = mtk_ovl_layer_config,
	.addon_config = mtk_ovl_addon_config,
	.io_cmd = mtk_ovl_io_cmd,
	.prepare = mtk_ovl_prepare,
	.unprepare = mtk_ovl_unprepare,
	.connect = mtk_ovl_connect,
	.config_trigger = mtk_ovl_config_trigger,
	.partial_update = mtk_ovl_set_partial_update,
};

/* TODO: to be refactored */
int drm_ovl_tf_cb(int port, unsigned long mva, void *data)
{
	struct mtk_disp_ovl *ovl = (struct mtk_disp_ovl *)data;

	DDPPR_ERR("%s tf mva: 0x%lx\n", mtk_dump_comp_str(&ovl->ddp_comp), mva);

	mtk_ovl_analysis(&ovl->ddp_comp);
	mtk_ovl_dump(&ovl->ddp_comp);

	return 0;
}

static int mtk_disp_ovl_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;
	unsigned int bg_h, bg_w;
	void __iomem *baddr;
	char buf[50];

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "qos");
		priv->ddp_comp.qos_req = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "qos_other");
		priv->ddp_comp.qos_req_other = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.qos_req_other))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "fbdc_qos");
		priv->ddp_comp.fbdc_qos_req = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hrt_qos");
		priv->ddp_comp.hrt_qos_req = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hrt_qos_other");
		priv->ddp_comp.hrt_qos_req_other = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hrt_qos_req_other))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hdr_qos");
		priv->ddp_comp.hdr_qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hdr_qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));
	}

	baddr = priv->ddp_comp.regs;
	bg_w = readl(DISP_REG_OVL_ROI_SIZE + baddr) & 0xfff,
	bg_h = (readl(DISP_REG_OVL_ROI_SIZE + baddr) >> 16) & 0xfff,
	_store_bg_roi(&priv->ddp_comp, bg_h, bg_w);

	return 0;
}

static void mtk_disp_ovl_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_component_ops = {
	.bind = mtk_disp_ovl_bind, .unbind = mtk_disp_ovl_unbind,
};

static int mtk_disp_ovl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret, len;
	const __be32 *ranges = NULL;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_OVL);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	DDPINFO("%s comp_id:%d\n", __func__, comp_id);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}
	priv->fbdc_clk = of_clk_get(dev->of_node, 1);
	if (IS_ERR(priv->fbdc_clk))
		priv->fbdc_clk = NULL;

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges && priv->data && priv->data->is_support_34bits)
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	writel(0, priv->ddp_comp.regs + DISP_REG_OVL_INTSTA);
	dsb(sy);
	writel(0, priv->ddp_comp.regs + DISP_REG_OVL_INTEN);
	dsb(sy);
	ret = devm_request_irq(dev, irq, mtk_disp_ovl_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			       priv);
	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
				__func__, __LINE__,
				irq, ret, comp_id);
		return ret;
	}

	//mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_ovl_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		//mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
	return ret;
}

static int mtk_disp_ovl_remove(struct platform_device *pdev)
{
	//struct mtk_disp_ovl *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_ovl_component_ops);
	//mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_ovl_data mt2701_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT2701,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = false,
	.fmt_uyvy = 9U << 12,
	.fmt_yuyv = 8U << 12,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6761  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

const struct mtk_disp_ovl_data mt6761_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6761,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x3ff,
	.fifo_size = 192,
	.issue_req_th_dl = 127,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 63,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x7777,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6765  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

const struct mtk_disp_ovl_data mt6765_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6765,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0x3ff,
	.fifo_size = 192,
	.issue_req_th_dl = 127,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 63,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x7777,
	.is_support_34bits = false,
};

static const struct compress_info compr_info_mt6768  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

const struct mtk_disp_ovl_data mt6768_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6768,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x3ff,
	.fifo_size = 192,
	.issue_req_th_dl = 127,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 63,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x7777,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6779  = {
	.name = "PVRIC_V3_1_MTK_1",
	.l_config = &compr_l_config_PVRIC_V3_1,
};

static const struct mtk_disp_ovl_data mt6779_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT6779,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6779,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6885  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6885_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6885,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x15e,
	.fifo_size = 384,
	.issue_req_th_dl = 255,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 127,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x7777,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6877  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6877_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6877,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x5555,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6781  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6781_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6781,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x5555,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6983  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6983_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6983,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x15e,
	.fifo_size = 384,
	.issue_req_th_dl = 255,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 127,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x7777,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6983,
	.aid_per_layer_setting = false,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6983,
	.source_bpc = 10,
};

static const struct compress_info compr_info_mt6985  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6985_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6985,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x3c0,
	.fifo_size = 1024,
	.issue_req_th_dl = 511,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 255,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xFFFF,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6985,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6985,
	.source_bpc = 10,
	.support_pq_selfloop = true, /* pq in out self loop */
	.is_right_ovl_comp = &is_right_ovl_comp_MT6985,
};

static const struct compress_info compr_info_mt6989 = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct compress_info compr_info_mt6899 = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6989_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6989,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0x3c0,
	.fifo_size = 1536,
	.issue_req_th_dl = 511,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 255,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xFFFF,
	.stash_en = 0x73,
	.stash_cfg = 0x10080400,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6989,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6989,
	.source_bpc = 10,
	.support_pq_selfloop = true, /* pq in out self loop */
	//.is_right_ovl_comp = &is_right_ovl_comp_MT6985,
	.frame_done_event = &ovl_comp_frame_done_event_MT6989,
	.ovlsys_mapping = &mtk_ovl_sys_mapping_MT6989,
	.ovl_phy_mapping = &mtk_ovl_phy_mapping_MT6989,
};

static const struct mtk_disp_ovl_data mt6899_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6899,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0x3c0,
	.fifo_size = 1536,
	.issue_req_th_dl = 511,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 255,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xFFFF,
	.stash_en = 0x73,
	.stash_cfg = 0x10080400,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6899,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6899,
	.source_bpc = 10,
	.support_pq_selfloop = true, /* pq in out self loop */
	//.is_right_ovl_comp = &is_right_ovl_comp_MT6985,
	.frame_done_event = &ovl_comp_frame_done_event_MT6899,
	.ovlsys_mapping = &mtk_ovl_sys_mapping_MT6899,
	.ovl_phy_mapping = &mtk_ovl_phy_mapping_MT6899,
};

static const struct compress_info compr_info_mt6897  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6897_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6897,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x3c0,
	.fifo_size = 1024,
	.issue_req_th_dl = 511,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 255,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xFFFF,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6985,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6985,
	.source_bpc = 10,
	.support_pq_selfloop = true, /* pq in out self loop */
	.is_right_ovl_comp = &is_right_ovl_comp_MT6985,
};

static const struct compress_info compr_info_mt6895  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6895_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6895,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0x15e,
	.fifo_size = 384,
	.issue_req_th_dl = 255,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 127,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x7777,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6895,
	.aid_per_layer_setting = false,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6895,
	.source_bpc = 10,
};

static const struct compress_info compr_info_mt6886  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6886_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6886,
	.support_shadow = false,
	.need_bypass_shadow = false,
	/* dc mode setting align mt6895 */
	.preultra_th_dc = 0x15e,
	.fifo_size = 768,
	.issue_req_th_dl = 383,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 191,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0xbbbb,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6886,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6886,
	.source_bpc = 10,
};

static const struct compress_info compr_info_mt6873  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6873_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6873,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x5555,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6853  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6853_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6853,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x5555,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6833  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6833_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6833,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x5555,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6879  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6879_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6879,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0xe0,
	.fifo_size = 576,
	.issue_req_th_dl = 383,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 191,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xbbbb,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6879,
	.aid_per_layer_setting = false,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6879,
	.source_bpc = 8,
};

static const struct compress_info compr_info_mt6855  = {
	.name = "PVRIC_V3_1_MTK_1",
	.l_config = &compr_l_config_PVRIC_V3_1,
};

static const struct mtk_disp_ovl_data mt6855_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6855,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0xe0,
	.fifo_size = 576,
	.issue_req_th_dl = 383,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 191,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xbbbb,
	.is_support_34bits = true,
	.source_bpc = 8,
};

static const struct mtk_disp_ovl_data mt8173_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT8173,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.is_support_34bits = false,
	.source_bpc = 8,
};

static const struct of_device_id mtk_disp_ovl_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-ovl",
	 .data = &mt2701_ovl_driver_data},
	{.compatible = "mediatek,mt6761-disp-ovl",
	 .data = &mt6761_ovl_driver_data},
	{.compatible = "mediatek,mt6765-disp-ovl",
	 .data = &mt6765_ovl_driver_data},
	{.compatible = "mediatek,mt6768-disp-ovl",
	 .data = &mt6768_ovl_driver_data},
	{.compatible = "mediatek,mt6779-disp-ovl",
	 .data = &mt6779_ovl_driver_data},
	{.compatible = "mediatek,mt8173-disp-ovl",
	 .data = &mt8173_ovl_driver_data},
	{.compatible = "mediatek,mt6885-disp-ovl",
	 .data = &mt6885_ovl_driver_data},
	{.compatible = "mediatek,mt6877-disp-ovl",
	 .data = &mt6877_ovl_driver_data},
	{.compatible = "mediatek,mt6781-disp-ovl",
	 .data = &mt6781_ovl_driver_data},
	{.compatible = "mediatek,mt6983-disp-ovl",
	 .data = &mt6983_ovl_driver_data},
	{.compatible = "mediatek,mt6985-disp-ovl",
	 .data = &mt6985_ovl_driver_data},
	{.compatible = "mediatek,mt6989-disp-ovl",
	 .data = &mt6989_ovl_driver_data},
	{.compatible = "mediatek,mt6899-disp-ovl",
	 .data = &mt6899_ovl_driver_data},
	{.compatible = "mediatek,mt6897-disp-ovl",
	 .data = &mt6897_ovl_driver_data},
	{.compatible = "mediatek,mt6895-disp-ovl",
	 .data = &mt6895_ovl_driver_data},
	{.compatible = "mediatek,mt6886-disp-ovl",
	 .data = &mt6886_ovl_driver_data},
	{.compatible = "mediatek,mt6873-disp-ovl",
	 .data = &mt6873_ovl_driver_data},
	{.compatible = "mediatek,mt6853-disp-ovl",
	 .data = &mt6853_ovl_driver_data},
	{.compatible = "mediatek,mt6833-disp-ovl",
	 .data = &mt6833_ovl_driver_data},
	{.compatible = "mediatek,mt6879-disp-ovl",
	 .data = &mt6879_ovl_driver_data},
	{.compatible = "mediatek,mt6855-disp-ovl",
	 .data = &mt6855_ovl_driver_data},
	{.compatible = "mediatek,mt6835-disp-ovl",
	 .data = &mt6835_ovl_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_driver_dt_match);

struct platform_driver mtk_disp_ovl_driver = {
	.probe = mtk_disp_ovl_probe,
	.remove = mtk_disp_ovl_remove,
	.driver = {

			.name = "mediatek-disp-ovl",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ovl_driver_dt_match,
		},
};
