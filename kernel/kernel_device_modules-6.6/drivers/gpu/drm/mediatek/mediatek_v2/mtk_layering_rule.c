// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
/*
#if defined(CONFIG_MTK_DRAMC)
#include "mtk_dramc.h"
#endif
*/
#include "mtk_layering_rule.h"
#include "mtk_log.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_graphics_base.h"

#include <soc/mediatek/mmqos.h>

static unsigned int sp_hrt_idx[MAX_CRTC];
static struct layering_rule_ops l_rule_ops;
static struct layering_rule_info_t l_rule_info;

static DEFINE_MUTEX(hrt_table_lock);

/* To backup for primary display drm_mtk_layer_config */
static struct drm_mtk_layer_config *g_input_config;

static int emi_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{100, 300, 500, 600},
};

static int larb_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{100, 300, 500, 600},
};

/**
 * The layer mapping table define ovl layer dispatch rule for both
 * primary and secondary display.Each table has 16 elements which
 * represent the layer mapping rule by the number of input layers.
 */
static uint16_t layer_mapping_table[HRT_TB_NUM] = {
	0x0003, 0x007E, 0x007A, 0x0001
};
static uint16_t layer_mapping_table_mt6985[HRT_TB_NUM] = {
	0x0003, 0x007E, 0x007A, 0x0001
}; //0x0006:one OVL_2L, 0x007E:three OVL_2L
static uint16_t layer_mapping_table_vds_switch[HRT_TB_NUM] = {
	0x0003, 0x0078, 0x0078, 0x0078
};
static uint16_t layer_mapping_table_dual_exdma[HRT_TB_NUM] = {
	0x0003, 0x0078, 0x0070, 0x0001
};

/**
 * The larb mapping table represent the relation between LARB and OVL.
 */
static uint16_t larb_mapping_table[HRT_TB_NUM] = {
	0x0001, 0x0010, 0x0010, 0x0001
};
static uint16_t larb_mapping_tb_vds_switch[HRT_TB_NUM] = {
	0x0001, 0x0010, 0x0010, 0x0001
};

/**
 * The OVL mapping table is used to get the OVL index of correcponding layer.
 * The bit value 1 means the position of the last layer in OVL engine.
 */
static uint16_t ovl_mapping_table[HRT_TB_NUM] = {
	0x0002, 0x0045, 0x0045, 0x0001
};
static uint16_t ovl_mapping_table_mt6985[HRT_TB_NUM] = {
	0x0002, 0x0055, 0x0055, 0x0001
}; //0x0005:one OVL_2L, 0x0055:three OVL_2L
static uint16_t ovl_mapping_tb_vds_switch[HRT_TB_NUM] = {
	0x0002, 0x0045, 0x0045, 0x0045
};
static uint16_t ovl_mapping_table_dual_exdma[HRT_TB_NUM] = {
	0x0002, 0x0079, 0x0079, 0x0001
};

#define GET_SYS_STATE(sys_state)                                               \
	((l_rule_info.hrt_sys_state >> sys_state) & 0x1)

static void layering_rule_scenario_decision(struct drm_device *dev,
	const enum SCN_FACTOR scn_decision_flag, const unsigned int scale_num)
{
/*TODO: need MMP support*/
#ifdef IF_ZERO
	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_START,
			 l_rule_info.addon_scn[0], l_rule_info.layer_tb_idx |
			 (l_rule_info.bound_tb_idx << 16));
#endif
	l_rule_info.primary_fps = 60;
	l_rule_info.bound_tb_idx = HRT_BOUND_TYPE_LP4;

	if (scn_decision_flag & SCN_MML_SRAM_ONLY)
		l_rule_info.addon_scn[HRT_PRIMARY] = MML_SRAM_ONLY;
	else if (scn_decision_flag & SCN_MML)
		l_rule_info.addon_scn[HRT_PRIMARY] = MML_RSZ;
	else if (scn_decision_flag & SCN_NEED_GAME_PQ)
		l_rule_info.addon_scn[HRT_PRIMARY] = GAME_PQ;
	else if (scn_decision_flag & SCN_NEED_VP_PQ)
		l_rule_info.addon_scn[HRT_PRIMARY] = VP_PQ;
	else if (scale_num == 1)
		l_rule_info.addon_scn[HRT_PRIMARY] = ONE_SCALING;
	else if (scale_num == 2)
		l_rule_info.addon_scn[HRT_PRIMARY] = TWO_SCALING;
	else
		l_rule_info.addon_scn[HRT_PRIMARY] = NONE;

	if (scn_decision_flag & SCN_TRIPLE_DISP) {
		l_rule_info.addon_scn[HRT_SECONDARY] = TRIPLE_DISP;
		l_rule_info.addon_scn[HRT_THIRD] = TRIPLE_DISP;
		l_rule_info.addon_scn[HRT_FOURTH] = TRIPLE_DISP;
	} else {
		l_rule_info.addon_scn[HRT_SECONDARY] = NONE;
		l_rule_info.addon_scn[HRT_THIRD] = NONE;
		l_rule_info.addon_scn[HRT_FOURTH] = NONE;
	}

	if ((scn_decision_flag & SCN_IDLE) && !(scn_decision_flag & SCN_CLEAR))
		l_rule_info.addon_scn[HRT_PRIMARY] = NONE;

/*TODO: need MMP support*/
#ifdef IF_ZERO
	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_END,
			 l_rule_info.addon_scn[0], l_rule_info.layer_tb_idx |
			 (l_rule_info.bound_tb_idx << 16));
#endif
}

/* A OVL supports at most 1 yuv layers */
static void filter_by_yuv_layers(struct drm_device *dev, struct drm_mtk_layering_info *disp_info)
{
	unsigned int disp_idx = 0, i = 0;
	struct drm_mtk_layer_config *info;
	unsigned int yuv_gpu_cnt;
	unsigned int yuv_layer_gpu[MAX_PHY_OVL_CNT];
	int yuv_layer_ovl = -1;
	struct mtk_drm_private *priv = dev->dev_private;

	for (disp_idx = 0 ; disp_idx < HRT_DISP_TYPE_NUM ; disp_idx++) {
		yuv_layer_ovl = -1;
		yuv_gpu_cnt = 0;

		/* cal gpu_layer_cnt & yuv_layer_cnt */
		for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
			info = &(disp_info->input_config[disp_idx][i]);
			if (mtk_is_gles_layer(disp_info, disp_idx, i))
				continue;

			/* display not support RGBA1010102 & DRM_FORMAT_ABGR16161616F) */
			if ((priv->data->mmsys_id == MMSYS_MT6765
				|| priv->data->mmsys_id == MMSYS_MT6761)
					&& (info->src_fmt == DRM_FORMAT_RGBA1010102
						|| info->src_fmt == DRM_FORMAT_ABGR16161616F)) {
				mtk_rollback_layer_to_GPU(disp_info, disp_idx, i);
				continue;
			}

			if (mtk_is_yuv(info->src_fmt)) {
				if (info->secure == 1 &&
				    yuv_layer_ovl < 0) {
					yuv_layer_ovl = i;
				} else if (yuv_gpu_cnt < MAX_PHY_OVL_CNT) {
					yuv_layer_gpu[yuv_gpu_cnt] = i;
					yuv_gpu_cnt++;
				} else {
					DDPPR_ERR("%s: yuv_gpu_cnt %d over MAX_PHY_OVL_CNT\n",
						__func__, yuv_gpu_cnt);
					return;
				}
			}
		}

		if (yuv_gpu_cnt == 0)
			continue;

		if (yuv_layer_ovl >= 0) {
			//if have sec layer, rollback the others to gpu
			for (i = 0; i < yuv_gpu_cnt; i++)
				mtk_rollback_layer_to_GPU(disp_info,
					disp_idx, yuv_layer_gpu[i]);
		} else {
			/* keep the 1st normal yuv layer,
			 * rollback the others to gpu
			 */
			for (i = 1; i < yuv_gpu_cnt; i++)
				mtk_rollback_layer_to_GPU(disp_info,
					disp_idx, yuv_layer_gpu[i]);
		}
	}
}

static void filter_2nd_display(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i = 0, j = 0;

	for (i = HRT_SECONDARY; i < HRT_DISP_TYPE_NUM; i++) {
		unsigned int max_layer_cnt = SECONDARY_OVL_LAYER_NUM;
		unsigned int layer_cnt = 0;

		if (is_triple_disp(disp_info) && i == HRT_SECONDARY)
			max_layer_cnt = 1;
		for (j = 0; j < disp_info->layer_num[i]; j++) {
			if (mtk_is_gles_layer(disp_info, i, j))
				continue;

			layer_cnt++;
			if (disp_info->layer_num[i] <= SECONDARY_OVL_LAYER_NUM &&
					layer_cnt > max_layer_cnt)
				mtk_rollback_layer_to_GPU(disp_info, i, j);
			else if (disp_info->layer_num[i] > SECONDARY_OVL_LAYER_NUM &&
					layer_cnt >= max_layer_cnt)
				mtk_rollback_layer_to_GPU(disp_info, i, j);
		}
	}
}

static bool is_ovl_wcg(enum mtk_drm_dataspace ds)
{
	bool ret = false;

	switch (ds) {
	case MTK_DRM_DATASPACE_V0_SCRGB:
	case MTK_DRM_DATASPACE_V0_SCRGB_LINEAR:
	case MTK_DRM_DATASPACE_DISPLAY_P3:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool is_ovl_standard(struct drm_device *dev, enum mtk_drm_dataspace ds)
{
	struct mtk_drm_private *priv = dev->dev_private;
	enum mtk_drm_dataspace std = ds & MTK_DRM_DATASPACE_STANDARD_MASK;
	bool ret = false;

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) &&
	    is_ovl_wcg(ds))
		return ret;

	switch (std) {
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
	case MTK_DRM_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		ret = false;
		break;
	default:
		ret = true;
		break;
	}
	return ret;
}

static void filter_by_wcg(struct drm_device *dev,
			  struct drm_mtk_layering_info *disp_info)
{
	unsigned int j;
	struct drm_mtk_layer_config *c;
	unsigned int disp_idx = 0;
	unsigned int condition;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;

	for (; disp_idx < HRT_DISP_TYPE_NUM ; disp_idx++) {
		for (j = 0; j < disp_info->layer_num[disp_idx]; j++) {
			c = &disp_info->input_config[disp_idx][j];
			/* TODO: check disp WCG cap */
			condition = (disp_idx == 0) || !is_ovl_wcg(c->dataspace);
			if (condition &&
			    (is_ovl_standard(dev, c->dataspace) ||
			     mtk_has_layer_cap(c, MTK_MDP_HDR_LAYER)))
				continue;

			mtk_rollback_layer_to_GPU(disp_info, disp_idx, j);
		}
		if (get_layering_opt(LYE_OPT_SPHRT))
			break;
	}
}

static bool can_be_compress(struct drm_device *dev, uint32_t format)
{
	struct mtk_drm_private *priv = dev->dev_private;

	if (mtk_is_yuv(format) || (format == DRM_FORMAT_RGB565 &&
			!priv->data->can_compress_rgb565))
		return 0;

	return 1;
}

static void filter_by_fbdc(struct drm_device *dev,
			struct drm_mtk_layering_info *disp_info)
{
	unsigned int i, j;
	struct drm_mtk_layer_config *c;
	struct mtk_drm_private *priv = dev->dev_private;

	/* primary: check fmt */
	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		if (!c->compress)
			continue;

		if ((can_be_compress(dev, c->src_fmt) == 0) ||
			(priv && disp_info->disp_idx > HRT_PRIMARY &&
			mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_SPHRT)))
			mtk_rollback_compress_layer_to_GPU(disp_info,
							   HRT_PRIMARY, i);
	}

	/* secondary: rollback all */
	for (i = HRT_SECONDARY; i < HRT_DISP_TYPE_NUM; i++)
		for (j = 0; j < disp_info->layer_num[i]; j++) {
			c = &(disp_info->input_config[i][j]);

			if (!c->compress ||
				mtk_is_gles_layer(disp_info, i, j))
				continue;

			/* if the layer is already gles layer,
			 * do not set NO_FBDC to reduce BW access
			 */
			mtk_rollback_compress_layer_to_GPU(disp_info, i, j);
		}
}

static bool filter_by_hw_limitation(struct drm_device *dev,
				    struct drm_mtk_layering_info *disp_info)
{
	bool flag = false;

	filter_by_wcg(dev, disp_info);

	filter_by_yuv_layers(dev, disp_info);

	/* Is this nessasary? */
	filter_2nd_display(disp_info);

	return flag;
}

static uint16_t get_mapping_table(struct drm_device *dev, int disp_idx, int disp_list,
				  enum DISP_HW_MAPPING_TB_TYPE tb_type,
				  int param);
static int layering_get_valid_hrt(struct drm_crtc *crtc, struct drm_mtk_layering_info *disp_info);

static void copy_hrt_bound_table(struct drm_mtk_layering_info *disp_info,
			int is_larb, int *hrt_table, struct drm_device *dev)
{
	int valid_num, ovl_bound, i;
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_crtc *crtc;
	unsigned int disp_idx = 0;

	/* Not used in 6779 */
	if (is_larb)
		return;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;

	crtc = priv->crtc[disp_idx];

	/* update table if hrt bw is enabled */
	mutex_lock(&hrt_table_lock);
	valid_num = layering_get_valid_hrt(crtc, disp_info);
	ovl_bound = mtk_get_phy_layer_limit(
		get_mapping_table(dev, 0, disp_info->disp_list, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT));
	valid_num = min(valid_num, ovl_bound * 100);

	/* if emi eff in display, need change the largest bound */
	if (priv->data->need_emi_eff)
		valid_num = (valid_num * 10000) / default_emi_eff;

	for (i = 0; i < HRT_LEVEL_NUM; i++)
		emi_bound_table[l_rule_info.bound_tb_idx][i] = valid_num;
	mutex_unlock(&hrt_table_lock);

	for (i = 0; i < HRT_LEVEL_NUM; i++)
		hrt_table[i] = emi_bound_table[l_rule_info.bound_tb_idx][i];
}

static int *get_bound_table(enum DISP_HW_MAPPING_TB_TYPE tb_type)
{
	switch (tb_type) {
	case DISP_HW_EMI_BOUND_TB:
		return emi_bound_table[l_rule_info.bound_tb_idx];
	case DISP_HW_LARB_BOUND_TB:
		return larb_bound_table[l_rule_info.bound_tb_idx];
	default:
		break;
	}
	return NULL;
}
uint16_t get_dynamic_mapping_table(struct drm_device *dev, unsigned int disp_idx,
		unsigned int disp_list, unsigned int tb_type, unsigned int hrt_type)
{
	struct mtk_drm_private *priv = dev->dev_private;
	unsigned int temp, temp1 = 0, comp_id_nr, *comp_id_list;
	unsigned int i, j;
	int main_disp_idx = -1;
	uint16_t ret = 0;

	comp_id_nr = mtk_ddp_ovl_resource_list(priv, &comp_id_list);

	for (i = 0; i < MAX_CRTC ; ++i) {
		if (priv->pre_defined_bw[i] == 0xFFFFFFFF) {
			main_disp_idx = i;
			break;
		}
	}
	if (main_disp_idx == -1) {
		DDPPR_ERR("%s can't find main disp, set it to 0\n", __func__);
		main_disp_idx = 0;
	}

	switch (hrt_type) {
	case HRT_TB_TYPE_GENERAL0:
		if (tb_type == DISP_HW_LAYER_TB)
			ret = 0x3;
		else /* DISP_HW_OVL_TB */
			ret = 0x2;
		break;
	case HRT_TB_TYPE_GENERAL1:
		if (tb_type == DISP_HW_LAYER_TB) {
			if (disp_idx == main_disp_idx) {
				temp = priv->ovl_usage[main_disp_idx];

				for (i = 0 ; i < MAX_CRTC ; ++i) {
					if (i == main_disp_idx)
						continue;

					if ((disp_list & BIT(i)) != 0)
						temp = temp & ~priv->ovl_usage[i];
				}
			} else {
				temp = priv->ovl_usage[disp_idx];
			}
			for (i = 0, j = 0 ; i < comp_id_nr ; ++i) {
				if ((temp & BIT(i)) != 0) {
					temp1 |= 0x3 << (j * 2);
					j++;
				}
			}
			temp1 = temp1 << 1;
			ret = temp1;
		} else { /* DISP_HW_OVL_TB */
			if (disp_idx == main_disp_idx) {
				temp = priv->ovl_usage[main_disp_idx];

				for (i = 0 ; i < MAX_CRTC ; ++i) {
					if (i == main_disp_idx)
						continue;

					if ((disp_list & BIT(i)) != 0)
						temp = temp & ~priv->ovl_usage[i];
				}
			} else {
				temp = priv->ovl_usage[disp_idx];
			}

			for (i = 0, j = 0 ; i < comp_id_nr ; ++i) {
				if ((temp & BIT(i)) != 0) {
					temp1 |= 0x1 << (j * 2);
					j++;
				}
			}
			temp1 |= 0x1 << (j * 2);
			ret = temp1;
		}
		break;
	case HRT_TB_TYPE_RPO_L0:
		if (tb_type == DISP_HW_LAYER_TB) {
			if (disp_idx == main_disp_idx) {
				temp = priv->ovl_usage[main_disp_idx];

				for (i = 0 ; i < MAX_CRTC ; ++i) {
					if (i == main_disp_idx)
						continue;

					if ((disp_list & BIT(i)) != 0)
						temp = temp & ~priv->ovl_usage[i];
				}
			} else {
				temp = priv->ovl_usage[disp_idx];
			}

			for (i = 0, j = 0; i < comp_id_nr ; ++i) {
				if ((temp & BIT(i)) != 0) {
					if (i == 0)
						temp1 |= 0x1 << (j * 2);
					else
						temp1 |= 0x3 << (j * 2);
					j++;
				}
			}
			temp1 = temp1 << 1;
			ret =  temp1;
		} else { /* DISP_HW_OVL_TB */
			if (disp_idx == main_disp_idx) {
				temp = priv->ovl_usage[main_disp_idx];

				for (i = 0 ; i < MAX_CRTC ; ++i) {
					if (i == main_disp_idx)
						continue;

					if ((disp_list & BIT(i)) != 0)
						temp = temp & ~priv->ovl_usage[i];
				}
			} else {
				temp = priv->ovl_usage[disp_idx];
			}

			for (i = 0, j = 0 ; i < comp_id_nr ; ++i) {
				if ((temp & BIT(i)) != 0) {
					temp1 |= 0x1 << (j * 2);
					j++;
				}
			}
			temp1 |= 0x1 << (j * 2);
			ret = temp1;
		}
		break;
	case HRT_TB_TYPE_SINGLE_LAYER:
		ret = 0x1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static uint16_t get_mapping_table(struct drm_device *dev, int disp_idx, int disp_list,
				  enum DISP_HW_MAPPING_TB_TYPE tb_type,
				  int param)
{
	uint16_t map = 0;
	uint16_t tmp_map = 0;
	int i;
	int cnt = 0;
	struct drm_crtc *crtc;
	const struct mtk_addon_scenario_data *addon_data = NULL;
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc;

	drm_for_each_crtc(crtc, dev) {
		if (drm_crtc_index(crtc) == disp_idx) {
			addon_data =
				mtk_addon_get_scenario_data(__func__,
					crtc,
					l_rule_info.addon_scn[disp_idx]);
			break;
		}
	}

	mtk_crtc = to_mtk_crtc(crtc);

	if (!addon_data) {
		DDPPR_ERR("disp_idx:%d cannot get addon data\n", disp_idx);
		return 0;
	}

	switch (tb_type) {
	case DISP_HW_OVL_TB:
		if (priv->data->mmsys_id == MMSYS_MT6985 ||
			priv->data->mmsys_id == MMSYS_MT6897 ||
			priv->data->mmsys_id == MMSYS_MT6989 ||
			priv->data->mmsys_id == MMSYS_MT6899)
			if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH))
				map = get_dynamic_mapping_table(dev, disp_idx, disp_list,
						DISP_HW_OVL_TB, addon_data->hrt_type);
			else {
					map = ovl_mapping_table_mt6985[addon_data->hrt_type];
			}
		else if (priv->data->ovl_exdma_rule)
			if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH))
				map = get_dynamic_mapping_table(dev, disp_idx,
						disp_list, DISP_HW_LAYER_TB, addon_data->hrt_type);
			else if (mtk_crtc->path_data->is_exdma_dual_layer)
				map = ovl_mapping_table_dual_exdma[addon_data->hrt_type];
			else
				map = layer_mapping_table_mt6985[addon_data->hrt_type];
		else
			map = ovl_mapping_table[addon_data->hrt_type];
		if (priv->secure_static_path_switch == true ||
			(mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch))
			map = ovl_mapping_tb_vds_switch[addon_data->hrt_type];
		break;
	case DISP_HW_LARB_TB:
		map = larb_mapping_table[addon_data->hrt_type];
		if (priv->secure_static_path_switch == true ||
			(mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch))
			map = larb_mapping_tb_vds_switch[addon_data->hrt_type];
		break;
	case DISP_HW_LAYER_TB:
		if (param <= MAX_PHY_OVL_CNT && param >= 0) {
			if (priv->data->mmsys_id == MMSYS_MT6985 ||
				priv->data->mmsys_id == MMSYS_MT6897 ||
				priv->data->mmsys_id == MMSYS_MT6989 ||
				priv->data->mmsys_id == MMSYS_MT6899)
				if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH))
					tmp_map = get_dynamic_mapping_table(dev, disp_idx,
						disp_list, DISP_HW_LAYER_TB, addon_data->hrt_type);
				else
					tmp_map = layer_mapping_table_mt6985[addon_data->hrt_type];
			else if (mtk_crtc->path_data->is_exdma_dual_layer)
				tmp_map = layer_mapping_table_dual_exdma[addon_data->hrt_type];
			else
				tmp_map = layer_mapping_table[addon_data->hrt_type];
			if (priv->secure_static_path_switch == true ||
				(mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_VDS_PATH_SWITCH) &&
				priv->need_vds_path_switch))
					tmp_map = layer_mapping_table_vds_switch[
						addon_data->hrt_type];

			for (i = 0, map = 0; i < 16; i++) {
				if (cnt == param)
					break;

				if (tmp_map & 0x1) {
					map |= (0x1 << i);
					cnt++;
				}
				tmp_map >>= 1;
			}
		}
		break;
	default:
		break;
	}
	return map;
}

void mtk_layering_rule_init(struct drm_device *dev)
{
	struct mtk_drm_private *private = dev->dev_private;

	l_rule_info.primary_fps = 60;
	l_rule_info.hrt_idx = 0;
	mtk_register_layering_rule_ops(&l_rule_ops, &l_rule_info);

	mtk_set_layering_opt(
		LYE_OPT_RPO,
		mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_RPO));
	mtk_set_layering_opt(LYE_OPT_EXT_LAYER,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_OVL_EXT_LAYER));
	mtk_set_layering_opt(LYE_OPT_CLEAR_LAYER,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_CLEAR_LAYER));
	mtk_set_layering_opt(LYE_OPT_OVL_BW_MONITOR,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_OVL_BW_MONITOR));
	mtk_set_layering_opt(LYE_OPT_GPU_CACHE,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_GPU_CACHE));
	mtk_set_layering_opt(LYE_OPT_SPHRT,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_SPHRT));
	mtk_set_layering_opt(LYE_OPT_SPDA_OVL_SWITCH,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_SDPA_OVL_SWITCH));

	if (get_layering_opt(LYE_OPT_RPO)) {
		const struct mtk_addon_scenario_data *addon_data;
		const struct mtk_addon_module_data *module_data;
		const struct mtk_addon_path_data *path_data;
		struct mtk_ddp_comp *comp = NULL;
		int i;

		addon_data = mtk_addon_get_scenario_data(__func__, private->crtc[0], ONE_SCALING);
		if (!addon_data || !addon_data->module_num)
			return;

		module_data = &addon_data->module_data[0];
		path_data = mtk_addon_module_get_path(module_data->module);

		if(private->data->mmsys_id != MMSYS_MT6991 &&
			private->data->mmsys_id != MMSYS_MT6899 &&
			private->data->mmsys_id != MMSYS_MT6989) {
			comp = private->ddp_comp[module_data->attach_comp];
			if (!comp) {
				DDPPR_ERR("RPO attached comp is NULL %d\n", module_data->attach_comp);
				return;
			}
			l_rule_info.rpo_scale_num =
			    mtk_ddp_comp_io_cmd(comp, NULL, OVL_GET_SELFLOOP_SUPPORT, NULL) ? 1 : 2;
		} else
			l_rule_info.rpo_scale_num = 1;

		for (i = 0; i < path_data->path_len; i++) {
			if ((mtk_ddp_comp_get_type(path_data->path[i]) == MTK_DISP_RSZ
				&& private->data->mmsys_id != MMSYS_MT6991)
				|| (mtk_ddp_comp_get_type(path_data->path[i]) == MTK_DISP_MDP_RSZ)) {
				comp = private->ddp_comp[path_data->path[i]];
				break;
			}
		}
		if (!comp) {
			DDPPR_ERR("RPO comp is NULL\n");
			return;
		}
		mtk_ddp_comp_io_cmd(comp, NULL, RSZ_GET_TILE_LENGTH, &l_rule_info.rpo_tile_length);
		mtk_ddp_comp_io_cmd(comp, NULL, RSZ_GET_IN_MAX_HEIGHT,
				    &l_rule_info.rpo_in_max_height);
	}
}

static bool _rollback_all_to_GPU_for_idle(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	/* Slghtly modify this function for TUI */

	if (atomic_read(&priv->rollback_all))
		return true;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
				    MTK_DRM_OPT_IDLEMGR_BY_REPAINT) ||
	    !atomic_read(&priv->idle_need_repaint)) {
		atomic_set(&priv->idle_need_repaint, 0);
		return false;
	}

	atomic_set(&priv->idle_need_repaint, 0);

	return true;
}

unsigned long long _layering_get_frame_bw(struct drm_crtc *crtc,
						struct drm_display_mode *mode)
{
	static unsigned long long bw_base;
	static int fps;
	unsigned int vact_fps;
	int width = mode->hdisplay, height = mode->vdisplay;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
		struct mtk_panel_params *params;

		params = mtk_crtc->panel_ext->params;
		if (params->dyn_fps.switch_en == 1 &&
			params->dyn_fps.vact_timing_fps != 0)
			vact_fps = params->dyn_fps.vact_timing_fps;
		else
			vact_fps = drm_mode_vrefresh(mode);
	} else
		vact_fps = drm_mode_vrefresh(mode);
	DDPINFO("%s,vrefresh = %d", __func__, vact_fps);

	if (fps == vact_fps)
		return bw_base;

	fps = vact_fps;

	bw_base = (unsigned long long)width * height * fps * 125 * 4;

	bw_base = DO_COMMON_DIV(bw_base, 100 * 1024 * 1024);

	return bw_base;
}

static int layering_get_valid_hrt(struct drm_crtc *crtc,
		struct drm_mtk_layering_info *disp_info)
{
	unsigned long long dvfs_bw = 0, avail_bw = 0;
	unsigned long long query_bw = 0, cam_bw = 0;
	bool already_query = false;
	unsigned long long tmp = 0;
	struct mtk_ddp_comp *output_comp;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv =
			mtk_crtc->base.dev->dev_private;
	unsigned int disp_idx = drm_crtc_index(crtc);
	int mode_idx = disp_info->disp_mode_idx[0];

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)){
		dvfs_bw = 600;
		goto out_dvfs_bw;
	}

	if (get_layering_opt(LYE_OPT_SPHRT)) {
		if (priv->pre_defined_bw[disp_idx] != 0xffffffff) {
			avail_bw = priv->pre_defined_bw[disp_idx];
		} else {
			unsigned int i, disp_status;

			disp_status = disp_info->disp_list;
			for (i = 0 ; i < MAX_CRTC ; ++i) {
				if (((disp_status >> i) & 0x1) == 0)
					continue;
				if (priv->pre_defined_bw[i] == 0xffffffff)
					continue;

				tmp += priv->pre_defined_bw[i];
			}
			query_bw = mtk_mmqos_get_avail_hrt_bw(HRT_DISP);
			already_query = true;
			avail_bw = query_bw;
		}
	} else {
		query_bw = mtk_mmqos_get_avail_hrt_bw(HRT_DISP);
		already_query = true;
		avail_bw = query_bw;
	}
	if (avail_bw == 0xfffffffffffffffe) {
		DDPPR_ERR("mm_hrt_get_available_hrt_bw=-2\n");
		dvfs_bw = 600;
		goto out_dvfs_bw;
	}
	avail_bw -= tmp;
	dvfs_bw = avail_bw;
	dvfs_bw *= 10000;
	tmp = 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	DDPDBG("%s: %u mode_idx:%d\n", __func__, disp_idx, mode_idx);
	mtk_crtc->mode_idx = mode_idx;
	tmp = mode_idx;
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL,
			GET_FRAME_HRT_BW_BY_MODE, &tmp);

	if (!tmp) {
		/* for avail_bw == 0 case, which imply this display is not HRT,
		 *  return this function to 16 overlap
		 */
		if (avail_bw == 0){
			dvfs_bw = 1600;
			goto out_dvfs_bw;
		}
		DDPPR_ERR("Get frame hrt bw by datarate is zero\n");
		dvfs_bw = 600;
		goto out_dvfs_bw;
	}
	dvfs_bw = DO_COMMON_DIV(dvfs_bw, tmp * 100);

	/* error handling when requested BW is less than 2 layers */
	if (dvfs_bw < 200) {
		// disp_aee_print("avail BW less than 2 layers, BW: %llu\n",
		//	dvfs_bw);
		cam_bw = mtk_mmqos_get_cam_hrt();
		DDPPR_ERR("disp %u avail BW < 2 layers\n", disp_idx);
		DDPPR_ERR("dvfsbw:%llu, availbw:%llu, querybw(%d):%llu, tmp=%llu, cambw=%llu\n",
			dvfs_bw, avail_bw, already_query, query_bw, tmp, cam_bw);
		dvfs_bw = 200;
	}

	DDPINFO("disp %u get avail HRT BW:%llu : %llu %llu\n",
		disp_idx, avail_bw, dvfs_bw, tmp);

out_dvfs_bw:

	if ((priv->data->mmsys_id == MMSYS_MT6991) && (dvfs_bw > 400))
		dvfs_bw = 400;

	return dvfs_bw;
}

void mtk_update_layering_opt_by_disp_opt(enum MTK_DRM_HELPER_OPT opt, int value)
{
	switch (opt) {
	case MTK_DRM_OPT_OVL_EXT_LAYER:
		mtk_set_layering_opt(LYE_OPT_EXT_LAYER, value);
		break;
	case MTK_DRM_OPT_RPO:
		mtk_set_layering_opt(LYE_OPT_RPO, value);
		break;
	case MTK_DRM_OPT_CLEAR_LAYER:
		mtk_set_layering_opt(LYE_OPT_CLEAR_LAYER, value);
		break;
	case MTK_DRM_OPT_OVL_BW_MONITOR:
		mtk_set_layering_opt(LYE_OPT_OVL_BW_MONITOR, value);
		break;
	case MTK_DRM_OPT_GPU_CACHE:
		mtk_set_layering_opt(LYE_OPT_GPU_CACHE, value);
		break;
	case MTK_DRM_OPT_SPHRT:
		mtk_set_layering_opt(LYE_OPT_SPHRT, value);
		break;
	case MTK_DRM_OPT_SDPA_OVL_SWITCH:
		mtk_set_layering_opt(LYE_OPT_SPDA_OVL_SWITCH, value);
		break;
	default:
		break;
	}
}

unsigned int _layering_rule_get_hrt_idx(unsigned int disp_idx)
{
	if (disp_idx >= MAX_CRTC)
		return 0;

	return sp_hrt_idx[disp_idx];
}

int _layering_rule_set_hrt_idx(unsigned int disp_idx, unsigned int value)
{
	if (disp_idx >= MAX_CRTC)
		return -1;

	sp_hrt_idx[disp_idx] = value;

	return 0;
}

#define SET_CLIP_R(clip, clip_r) (clip |= ((clip_r & 0xFF) << 0))
#define SET_CLIP_B(clip, clip_b) (clip |= ((clip_b & 0xFF) << 8))
#define SET_CLIP_L(clip, clip_l) (clip |= ((clip_l & 0xFF) << 16))
#define SET_CLIP_T(clip, clip_t) (clip |= ((clip_t & 0xFF) << 24))

#define GET_CLIP_R(clip) ((clip >> 0) & 0xFF)
#define GET_CLIP_B(clip) ((clip >> 8) & 0xFF)
#define GET_CLIP_L(clip) ((clip >> 16) & 0xFF)
#define GET_CLIP_T(clip) ((clip >> 24) & 0xFF)

static void calc_clip_x(struct drm_mtk_layer_config *cfg)
{
	unsigned int tile_w = 16;
	unsigned int src_x_s, src_x_e; /* aligned */
	unsigned int clip_l = 0, clip_r = 0;

	src_x_s = (cfg->src_offset_x) & ~(tile_w - 1);

	src_x_e = (cfg->src_offset_x + cfg->src_width + tile_w - 1) &
		  ~(tile_w - 1);

	clip_l = cfg->src_offset_x - src_x_s;
	clip_r = src_x_e - cfg->src_offset_x - cfg->src_width;

	SET_CLIP_R(cfg->clip, clip_r);
	SET_CLIP_L(cfg->clip, clip_l);
}

static void calc_clip_y(struct drm_mtk_layer_config *cfg)
{
	unsigned int tile_h = 4;
	unsigned int src_y_s, src_y_e; /* aligned */
	unsigned int clip_t = 0, clip_b = 0;

	src_y_s = (cfg->src_offset_y) & ~(tile_h - 1);

	src_y_e = (cfg->src_offset_y + cfg->src_height + tile_h - 1) &
		  ~(tile_h - 1);

	clip_t = cfg->src_offset_y - src_y_s;
	clip_b = src_y_e - cfg->src_offset_y - cfg->src_height;

	SET_CLIP_T(cfg->clip, clip_t);
	SET_CLIP_B(cfg->clip, clip_b);
}

static void backup_input_config(struct drm_mtk_layering_info *disp_info)
{
	unsigned int size = 0;

	mutex_lock(&hrt_table_lock);
	/* free before use */
	if (g_input_config != 0) {
		kfree(g_input_config);
		g_input_config = 0;
	}

	if (disp_info->layer_num[HRT_PRIMARY] <= 0 ||
		disp_info->input_config[HRT_PRIMARY] == NULL) {
		mutex_unlock(&hrt_table_lock);
		return;
	}

	/* memory allocate */
	size = sizeof(struct drm_mtk_layer_config) *
	       disp_info->layer_num[HRT_PRIMARY];
	g_input_config = kzalloc(size, GFP_KERNEL);

	if (g_input_config == 0) {
		DDPPR_ERR("%s: allocate memory fail\n", __func__);
		mutex_unlock(&hrt_table_lock);
		return;
	}

	/* memory copy */
	memcpy(g_input_config, disp_info->input_config[HRT_PRIMARY], size);
	mutex_unlock(&hrt_table_lock);
}

static void fbdc_pre_calculate(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i = 0;
	struct drm_mtk_layer_config *cfg = NULL;

	/* backup g_input_config */
	backup_input_config(disp_info);

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		cfg = &(disp_info->input_config[HRT_PRIMARY][i]);

		cfg->clip = 0;

		if (!cfg->compress)
			continue;

		if (mtk_is_gles_layer(disp_info, HRT_PRIMARY, i))
			continue;

		if (cfg->src_height != cfg->dst_height ||
		    cfg->src_width != cfg->dst_width)
			continue;

		calc_clip_x(cfg);
		calc_clip_y(cfg);
	}
}

static void
fbdc_adjust_layout_for_ext_grouping(struct drm_mtk_layering_info *disp_info)
{
	int i = 0;
	struct drm_mtk_layer_config *c;
	unsigned int dst_offset_x, dst_offset_y;
	unsigned int clip_r, clip_b, clip_l, clip_t;

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		/* skip if not compress, gles, resize */
		if (!c->compress ||
		    mtk_is_gles_layer(disp_info, HRT_PRIMARY, i) ||
		    (c->src_height != c->dst_height) ||
		    (c->src_width != c->dst_width))
			continue;

		dst_offset_x = c->dst_offset_x;
		dst_offset_y = c->dst_offset_y;

		clip_r = GET_CLIP_R(c->clip);
		clip_b = GET_CLIP_B(c->clip);
		clip_l = GET_CLIP_L(c->clip);
		clip_t = GET_CLIP_T(c->clip);

		/* bounary handling */
		if (dst_offset_x < clip_l)
			c->dst_offset_x = 0;
		else
			c->dst_offset_x -= clip_l;
		if (dst_offset_y < clip_t)
			c->dst_offset_y = 0;
		else
			c->dst_offset_y -= clip_t;

		c->dst_width += (clip_r + dst_offset_x - c->dst_offset_x);
		c->dst_height += (clip_b + dst_offset_y - c->dst_offset_y);
	}
}

static int get_below_ext_layer(struct drm_mtk_layering_info *disp_info,
			       int disp_idx, int cur)
{
	struct drm_mtk_layer_config *c, *tmp_c;
	int phy_id = -1, ext_id = -1, l_dst_offset_y = -1, i;

	if (disp_idx < 0)
		return -1;

	c = &(disp_info->input_config[disp_idx][cur]);

	/* search for phy */
	if (c->ext_sel_layer != -1) {
		for (i = cur - 1; i >= 0; i--) {
			tmp_c = &(disp_info->input_config[disp_idx][i]);
			if (tmp_c->ext_sel_layer == -1)
				phy_id = i;
		}
		if (phy_id == -1) /* error handle */
			return -1;
	} else
		phy_id = cur;

	/* traverse the ext layer below cur */
	tmp_c = &(disp_info->input_config[disp_idx][phy_id]);
	if (tmp_c->dst_offset_y > c->dst_offset_y) {
		ext_id = phy_id;
		l_dst_offset_y = tmp_c->dst_offset_y;
	}

	for (i = phy_id + 1; i <= phy_id + 3; i++) {
		/* skip itself */
		if (i == cur)
			continue;

		/* hit max num, stop */
		if (i >= disp_info->layer_num[disp_idx])
			break;

		/* hit gles, stop */
		if (mtk_is_gles_layer(disp_info, disp_idx, i))
			break;

		tmp_c = &(disp_info->input_config[disp_idx][i]);

		/* hit phy layer, stop */
		if (tmp_c->ext_sel_layer == -1)
			break;

		if (tmp_c->dst_offset_y > c->dst_offset_y) {
			if (l_dst_offset_y == -1 ||
			    l_dst_offset_y > tmp_c->dst_offset_y) {
				ext_id = i;
				l_dst_offset_y = tmp_c->dst_offset_y;
			}
		}
	}

	return ext_id;
}

static void
fbdc_adjust_layout_for_overlap_calc(struct drm_mtk_layering_info *disp_info)
{
	int i = 0, ext_id = 0;
	struct drm_mtk_layer_config *c, *ext_c;

	/* adjust dst layout because src clip */
	fbdc_adjust_layout_for_ext_grouping(disp_info);

	/* adjust dst layout because of buffer pre-fetch */
	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		/* skip gles layer */
		if (mtk_is_gles_layer(disp_info, HRT_PRIMARY, i))
			continue;

		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		/* skip resize layer */
		if ((c->src_height != c->dst_height) ||
		    (c->src_width != c->dst_width))
			continue;

		/* if compressed, shift up 4 lines because pre-fetching */
		if (c->compress) {
			if (c->dst_height > 4)
				c->dst_height -= 4;
			else
				c->dst_height = 1;
		}

		/* if there is compressed ext layer below this layer,
		 * add pre-fetch lines behind it
		 */
		ext_id = get_below_ext_layer(disp_info, HRT_PRIMARY, i);

		if (mtk_is_layer_id_valid(disp_info, HRT_PRIMARY, ext_id) ==
		    true) {
			ext_c = &(disp_info->input_config[HRT_PRIMARY][ext_id]);
			if (ext_c->compress)
				c->dst_height += (GET_CLIP_T(ext_c->clip) + 4);
		}
	}
}

static void fbdc_adjust_layout(struct drm_mtk_layering_info *disp_info,
			       enum ADJUST_LAYOUT_PURPOSE p)
{
	if (p == ADJUST_LAYOUT_EXT_GROUPING)
		fbdc_adjust_layout_for_ext_grouping(disp_info);
	else
		fbdc_adjust_layout_for_overlap_calc(disp_info);
}

static void fbdc_restore_layout(struct drm_mtk_layering_info *dst_info,
				enum ADJUST_LAYOUT_PURPOSE p)
{
	int i = 0;
	struct drm_mtk_layer_config *layer_info_s, *layer_info_d;

	if (g_input_config == 0)
		return;

	for (i = 0; i < dst_info->layer_num[HRT_PRIMARY]; i++) {
		layer_info_d = &(dst_info->input_config[HRT_PRIMARY][i]);
		layer_info_s = &(g_input_config[i]);

		layer_info_d->dst_offset_x = layer_info_s->dst_offset_x;
		layer_info_d->dst_offset_y = layer_info_s->dst_offset_y;
		layer_info_d->dst_width = layer_info_s->dst_width;
		layer_info_d->dst_height = layer_info_s->dst_height;
	}
}

static struct layering_rule_ops l_rule_ops = {
	.scenario_decision = layering_rule_scenario_decision,
	.get_bound_table = get_bound_table,
	/* HRT table would change so do not use get_hrt_bound
	 * in layering_rule_base. Instead, copy hrt table before calculation
	 */
	.get_hrt_bound = NULL,
	.copy_hrt_bound_table = copy_hrt_bound_table,
	.get_mapping_table = get_mapping_table,
	.rollback_to_gpu_by_hw_limitation = filter_by_hw_limitation,
	.rollback_all_to_GPU_for_idle = _rollback_all_to_GPU_for_idle,
	.fbdc_pre_calculate = fbdc_pre_calculate,
	.fbdc_adjust_layout = fbdc_adjust_layout,
	.fbdc_restore_layout = fbdc_restore_layout,
	.fbdc_rule = filter_by_fbdc,
	.layering_get_valid_hrt = layering_get_valid_hrt,
};
