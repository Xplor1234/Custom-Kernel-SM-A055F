/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DRM_MMP_H__
#define __MTK_DRM_MMP_H__

#include "mmprofile.h"
#include "mmprofile_function.h"
#include "mtk_drm_ddp.h"

#define MMP_CRTC_NUM 4

/* if changed, need to update init_drm_mmp_event() */
struct DRM_MMP_Events {
	mmp_event drm;
	mmp_event crtc[MMP_CRTC_NUM];

	/* define for IRQ */
	mmp_event IRQ;
	mmp_event ovl;
	mmp_event ovl0;
	mmp_event ovl1;
	mmp_event ovl0_2l;
	mmp_event ovl1_2l;
	mmp_event ovl2_2l;
	mmp_event ovl3_2l;
	mmp_event rdma;
	mmp_event rdma0;
	mmp_event rdma1;
	mmp_event rdma2;
	mmp_event rdma3;
	mmp_event rdma4;
	mmp_event rdma5;
	mmp_event mdp_rdma;
	mmp_event mdp_rdma0;
	mmp_event mdp_rdma1;
	mmp_event y2r;
	mmp_event wdma;
	mmp_event wdma0;
	mmp_event wdma1;
	mmp_event wdma12;
	mmp_event ufbc_wdma0;
	mmp_event ufbc_wdma1;
	mmp_event dsi;
	mmp_event dsi0;
	mmp_event dsi1;
	mmp_event dsi2;
	mmp_event aal;
	mmp_event aal0;
	mmp_event aal1;
	mmp_event dp_intf0;
	mmp_event ddp;
	mmp_event mutex[DISP_MUTEX_DDP_COUNT];
	mmp_event postmask;
	mmp_event postmask0;
	mmp_event abnormal_irq;
	mmp_event iova_tf;
	mmp_event pmqos;
	mmp_event ostdl;
	mmp_event channel_bw;
	mmp_event hrt_bw;
	mmp_event mutex_lock;
	mmp_event layering;
	mmp_event layering_blob;
	mmp_event dma_alloc;
	mmp_event dma_free;
	mmp_event dma_get;
	mmp_event dma_put;
	mmp_event ion_import_dma;
	mmp_event ion_import_fd;
	mmp_event ion_import_free;
	mmp_event set_mode;
	mmp_event top_clk;
	mmp_event mml_sram;
	mmp_event sram_alloc;
	mmp_event sram_free;
};

/* if changed, need to update init_crtc_mmp_event() */
struct CRTC_MMP_Events {
	mmp_event trig_loop_done;
	mmp_event bwm_loop_done;
	mmp_event enable;
	mmp_event disable;
	mmp_event release_fence;
	mmp_event update_present_fence;
	mmp_event release_present_fence;
	mmp_event present_fence_timestamp_same;
	mmp_event present_fence_timestamp;
	mmp_event update_sf_present_fence;
	mmp_event release_sf_present_fence;
	mmp_event warn_sf_pf_0;
	mmp_event warn_sf_pf_2;
	mmp_event ovl_bw_monitor;
	mmp_event channel_bw;
	mmp_event atomic_delay;
	mmp_event atomic_begin;
	mmp_event atomic_flush;
	mmp_event enable_vblank;
	mmp_event disable_vblank;
	mmp_event esd_check;
	mmp_event esd_recovery;
	mmp_event target_time;
	mmp_event leave_idle;
	mmp_event enter_idle;
	mmp_event idle_async;
	mmp_event idle_async_cb;
	mmp_event frame_cfg;
	mmp_event suspend;
	mmp_event resume;
	mmp_event dsi_suspend;
	mmp_event dsi_resume;
	mmp_event backlight;
	mmp_event backlight_grp;
	mmp_event ddic_send_cmd;
	mmp_event ddic_read_cmd;
	mmp_event path_switch;
	mmp_event user_cmd;
	mmp_event check_trigger;
	mmp_event kick_trigger;
	mmp_event atomic_commit;
	mmp_event pu_ddic_cmd;
	mmp_event pu_final_roi;
	mmp_event user_cmd_cb;
	mmp_event bl_cb;
	mmp_event clk_change;
	mmp_event layerBmpDump;
	mmp_event layer_dump[6];
	mmp_event wbBmpDump;
	mmp_event wb_dump;
	mmp_event cwbBmpDump;
	mmp_event cwb_dump;
	mmp_event discrete;
	mmp_event discrete_fill;
	/*Msync 2.0 mmp start*/
	mmp_event ovl_status_err;
	mmp_event vfp_period;
	mmp_event not_vfp_period;
	mmp_event dsi_state_dbg7;
	mmp_event dsi_dbg7_after_sof;
	mmp_event msync_enable;
	/*Msync 2.0 mmp end*/
	mmp_event mode_switch;
	mmp_event ddp_clk;
	/*AAL mmp mark*/
	mmp_event aal_sof_thread;
	mmp_event aal_dre30_rw;
	mmp_event aal_dre20_rh;
	mmp_event aal_event_ctl;
	/*Gamma mmp mark*/
	mmp_event gamma_ioctl;
	mmp_event gamma_sof;
	mmp_event gamma_backlight;
	mmp_event mml_dbg;
	mmp_event aal_ess20_elvss;
	mmp_event aal_ess20_gamma;
	mmp_event aal_ess20_curve;
	/*Clarity mmp mark*/
	mmp_event clarity_set_regs;
	/*ODDMR mmp mark*/
	mmp_event oddmr_sof_thread;
	mmp_event oddmr_ctl;
	/*dsi underrun irq check*/
	mmp_event dsi_underrun_irq;
	/*pq common ioctl*/
	mmp_event pq_frame_config;
	mmp_event notify_backlight;
	/* csc_bl sync */
	mmp_event csc_bl;
	mmp_event vblank_rec_thread;
	mmp_event bdg_gce_irq;
	/*vdo ltpo*/
	mmp_event dsi_real_sof;
	mmp_event dsi_ltpo_vsync;
	mmp_event arp_te;
	mmp_event leave_vidle;
	mmp_event enter_vidle;
	mmp_event pause_vidle;
	mmp_event set_dirty;
};

struct DRM_MMP_Events *get_drm_mmp_events(void);
struct CRTC_MMP_Events *get_crtc_mmp_events(unsigned long id);
void drm_mmp_init(void);
int mtk_drm_mmp_ovl_layer(struct mtk_plane_state *state,
			  u32 downSampleX, u32 downSampleY, int global_lye_num);
int mtk_drm_mmp_wdma_buffer(struct drm_crtc *crtc,
	struct drm_framebuffer *wb_fb, u32 downSampleX, u32 downSampleY);
int mtk_drm_mmp_cwb_buffer(struct drm_crtc *crtc,
	struct mtk_cwb_info *cwb_info,
	void *buffer, unsigned int buf_idx);

/* print mmp log for DRM_MMP_Events */
#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
#define DRM_MMP_MARK(event, v1, v2)                                            \
	mmprofile_log_ex(get_drm_mmp_events()->event,                  \
			 MMPROFILE_FLAG_PULSE, v1, v2)

#define DRM_MMP_EVENT_START(event, v1, v2)                                     \
	mmprofile_log_ex(get_drm_mmp_events()->event,                  \
			 MMPROFILE_FLAG_START, v1, v2)

#define DRM_MMP_EVENT_END(event, v1, v2)                                       \
	mmprofile_log_ex(get_drm_mmp_events()->event,                  \
			 MMPROFILE_FLAG_END, v1, v2)

/* print mmp log for CRTC_MMP_Events */
#define CHECK_ID_TYPE(id) \
_Generic((id), \
unsigned int: id, \
int: (id >= 0 ? (unsigned int)id : UINT_MAX), \
unsigned long: id, \
long: (id >= 0 ? (unsigned long)id : ULONG_MAX) \
)

#define CRTC_MMP_MARK(id, event, v1, v2)                                       \
	do {								\
		if (CHECK_ID_TYPE(id) < MMP_CRTC_NUM)                              \
			mmprofile_log_ex(get_crtc_mmp_events(id)->event,       \
					 MMPROFILE_FLAG_PULSE, v1, v2);       \
	} while (0)

#define CRTC_MMP_EVENT_START(id, event, v1, v2)                                \
	do {								\
		if (CHECK_ID_TYPE(id) < MMP_CRTC_NUM)                              \
			mmprofile_log_ex(get_crtc_mmp_events(id)->event,       \
					 MMPROFILE_FLAG_START, v1, v2);       \
	} while (0)

#define CRTC_MMP_EVENT_END(id, event, v1, v2)                                  \
	do {								\
		if (CHECK_ID_TYPE(id) < MMP_CRTC_NUM)                              \
			mmprofile_log_ex(get_crtc_mmp_events(id)->event,       \
					 MMPROFILE_FLAG_END, v1, v2);       \
	} while (0)

#define CRTC_MMP_BITMAP_MARK(id, event, data)                                  \
	do {								\
		if (CHECK_ID_TYPE(id) < MMP_CRTC_NUM)                              \
			mmprofile_log_meta_bitmap(get_crtc_mmp_events(id)->event,  \
					 MMPROFILE_FLAG_PULSE, data);       \
	} while (0)

#define CRTC_MMP_YUV_BITMAP_MARK(id, event, data)                              \
	do {								\
		if (CHECK_ID_TYPE(id) < MMP_CRTC_NUM)                              \
			mmprofile_log_meta_yuv_bitmap(get_crtc_mmp_events(id)->event,  \
					 MMPROFILE_FLAG_PULSE, data);       \
	} while (0)

#define CRTC_MMP_META_MARK(id, event, data)                                    \
	do {								\
		if (CHECK_ID_TYPE(id) < MMP_CRTC_NUM)                              \
			mmprofile_log_meta(get_crtc_mmp_events(id)->event,     \
					 MMPROFILE_FLAG_PULSE, data);       \
	} while (0)

#else
#define DRM_MMP_MARK(event, v1, v2) do { } while (0)
#define DRM_MMP_EVENT_START(event, v1, v2) do { } while (0)
#define DRM_MMP_EVENT_END(event, v1, v2) do { } while (0)
#define CRTC_MMP_MARK(id, event, v1, v2) do { } while (0)
#define CRTC_MMP_EVENT_START(id, event, v1, v2) do { } while (0)
#define CRTC_MMP_EVENT_END(id, event, v1, v2) do { } while (0)
#endif

#endif
