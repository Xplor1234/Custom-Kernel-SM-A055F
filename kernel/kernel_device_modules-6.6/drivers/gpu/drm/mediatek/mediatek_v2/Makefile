subdir-ccflags-y += -Wno-error

CFLAGS_mtk_layering_rule_base.o := -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2

mediatek-drm-y := mtk_drm_drv.o \
		  mtk_disp_tdshp.o \
		  mtk_disp_color.o \
		  mtk_disp_ccorr.o \
		  mtk_disp_c3d.o   \
		  mtk_disp_gamma.o \
		  mtk_disp_aal.o \
		  mtk_disp_dither.o \
		  mtk_disp_chist.o \
		  mtk_disp_ovl.o \
		  mtk_disp_rdma.o \
		  mtk_disp_mdp_rdma.o \
		  mtk_disp_rsz.o \
		  mtk_disp_mdp_rsz.o \
		  mtk_disp_wdma.o \
		  mtk_drm_crtc.o \
		  mtk_drm_ddp.o \
		  mtk_drm_ddp_addon.o \
		  mtk_drm_ddp_comp.o \
		  mtk_drm_fb.o \
		  mtk_drm_gem.o \
		  mtk_drm_plane.o \
		  mtk_dsi.o \
		  mtk_mipi_tx.o \
		  mtk_writeback.o \
		  mtk_fence.o \
		  mtk_drm_session.o \
		  mtk_dump.o \
		  mtk_debug.o \
		  mtk_layering_rule.o \
		  mtk_layering_rule_base.o \
		  mtk_rect.o \
		  mtk_drm_mmp.o \
		  mtk_drm_trace.o \
		  mtk_drm_helper.o \
		  mtk_drm_lowpower.o \
		  mtk_disp_postmask.o \
		  mtk_drm_assert.o \
		  mtk_drm_fbconsole.o \
		  mtk_disp_recovery.o \
		  mtk_disp_pmqos.o \
		  mtk_disp_dsc.o \
		  mtk_disp_cm.o \
		  mtk_disp_spr.o \
		  mtk_disp_postalign.o \
		  mtk_disp_oddmr/mtk_disp_oddmr_parse_data.o \
		  mtk_disp_oddmr/mtk_disp_oddmr.o \
		  mtk_drm_arr.o \
		  mtk_disp_merge.o \
		  mtk_dmdp_aal.o \
		  mtk_fbconfig_kdebug.o \
		  mtk_disp_y2r0.o \
		  mtk_disp_dlo_async.o \
		  mtk_disp_dli_async.o \
		  mtk_disp_inlinerotate.o \
		  mtk_mmlsys_bypass_mode.o \
		  mtk_lease.o \
		  platform/mtk_drm_6835.o \
		  mtk_disp_pq_helper.o \
		  mtk_disp_vidle.o \
		  mtk_disp_r2y0.o \
		  mtk_disp_ovl_exdma.o \
		  mtk_disp_ovl_blender.o \
		  mtk_disp_ovl_outproc.o \
		  mtk_disp_vdisp_ao.o \
		  mtk_disp_splitter.o \
		  mtk_notify.o \
		  mtk_cust.o \

#ccflags-y += -DDRM_CMDQ_DISABLE
#mediatek-drm-y += mtk_cmdq_dummy.o

ifeq ($(CONFIG_DRM_MEDIATEK_AUTO_YCT), m)
mediatek-drm-y += mtk_drm_auto/mtk_drm_ddp_comp_auto.o \
	mtk_drm_auto/mtk_drm_crtc_auto.o
endif

ifeq ($(CONFIG_DRM_MEDIATEK_DEBUG_FS),y)
mediatek-drm-y += mtk_drm_debugfs.o
endif

ifeq ($(strip $(MGK_INTERNAL)),true)
ccflags-y += -DDISP_STASH_ENABLE
ccflags-y += -DDRM_PARTIAL_UPDATE
endif

ifeq ($(CONFIG_DRM_MEDIATEK_DPTX_AUTO),y)
mediatek-drm-y += mtk_drm_dp/mtk_drm_dp_intf.o \
	mtk_drm_dp/mtk_drm_dp.o \
	mtk_drm_dp/mtk_drm_dp_mst_drv.o \
	mtk_drm_dp/mtk_drm_dp_mst.o \
	mtk_drm_dp/mtk_drm_dp_debug.o
else
mediatek-drm-y += mtk_dp_intf.o \
	mtk_dp.o \
	mtk_dp_hal.o \
	mtk_dp_debug.o
endif

#for 6382 bridge
mediatek-drm-y += mtk_disp_bdg.o

ifeq (yy, $(CONFIG_MTK_TEE_GP_SUPPORT)$(CONFIG_TRUSTONIC_TEE_SUPPORT))
obj-y += ca/
ccflags-y += -DDPTX_HDCP_ENABLE
obj-y += mtk_dp_hdcp1x.o \
	mtk_dp_hdcp2.o
endif

#ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),m)
#obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += ca/
#ccflags-y += -DDPTX_HDCP_ENABLE
#mediatek-drm-y += mtk_dp_hdcp1x.o \
#	mtk_dp_hdcp2.o
#endif

ifeq ($(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT), m)
mediatek-drm-y += mtk_drm_edp/mtk_dvo.o
mtk-drm-edp-y += mtk_drm_edp/mtk_drm_edp_phy.o
mtk-drm-edp-y += mtk_drm_edp/mtk_drm_edp.o
obj-$(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT) += mtk-drm-edp.o
#ccflags-y += -DEDPTX_DEBUG
#ccflags-y += -DEDPTX_ANDROID_SUPPORT
endif

ifneq ($(findstring $(CONFIG_DRM_MEDIATEK_CEC), y m),)
mediatek-drm-y += mtk_cec.o
endif

ifneq ($(findstring $(CONFIG_DRM_MEDIATEK_HDMI), y m),)
mediatek-drm-y += mtk_dpi.o

mediatek-drm-hdmi-objs := mtk_hdmi.o \
	mtk_hdmi_ddc.o \
	mtk_mt8195_hdmi_phy.o \
	mtk_hdmi_ca.o \
	mtk_hdmi_hdr.o \
	mtk_hdmi_edid.o \
	mtk_hdmi_debug.o

ifneq ($(findstring $(CONFIG_DRM_MEDIATEK_HDMI_HDCP), y m),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmirx/
mediatek-drm-hdmi-objs += mtk_hdmi_hdcp.o
endif

endif

obj-$(CONFIG_DRM_MEDIATEK_HDMI) += mediatek-drm-hdmi.o

mediatek-drm-$(CONFIG_DRM_FBDEV_EMULATION) += mtk_drm_fbdev.o

obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mediatek-drm.o
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_sync.o

obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_panel_ext.o
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_disp_notify.o

ifneq ($(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT), y)
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_aod_scp.o
endif

ifneq ($(filter y m, $(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)),)
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_disp_sec.o
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/			\
	     -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mminfra/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spi_slave_drv/          \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/bridge/ 	\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/src/	\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include/     \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/			\
             -I$(DEVICE_MODULES_PATH)/drivers/iommu/				\
             -I$(DEVICE_MODULES_PATH)/drivers/devfreq/				\
             -I$(srctree)/drivers/gpu/drm/				\
             -I$(srctree)/drivers/gpu/drm/display/				\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/				\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/$(MTK_PLATFORM) \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/        \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/		\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/        \
             -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common/ \
             -I$(srctree)/drivers/gpu/drm/mediatek \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/ \
             -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps/ \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mml \
             -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3 \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
	     -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek \

ifneq ($(filter y m, $(CONFIG_MTK_MME_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mme
endif

ifneq ($(filter y m, $(CONFIG_MTK_MDP)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdp/
ccflags-y += -DSHARE_WROT_SRAM
endif

ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox

ifneq ($(filter y m, $(CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW)),)
ccflags-y += -DMTK_LCM_PHYSICAL_ROTATION_HW
endif

#ccflags-y += -DDRM_MMPATH
#ccflags-y += -DNOT_SUPPORT_CABC_HW
ccflags-y += -DDISP_MDP_COLOR_ON
#ccflags-y += -DCCORR_SUPPORT

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
