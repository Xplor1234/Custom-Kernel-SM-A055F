# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.


subdir-ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccu/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cam_cal/inc \

obj-$(CONFIG_MTK_IMGSENSOR_ISP3_Z) += isp3_z/
obj-$(CONFIG_MTK_IMGSENSOR_ISP3_M) += isp3_m/
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += isp4_t/
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_C) += isp4_c/
obj-$(CONFIG_MTK_IMGSENSOR) += isp6s/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MON) += isp6s_mon/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MOU) += isp6s_mou/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_LAG) += isp6s_lag/

ifeq ($(CONFIG_CONFIG_WING_CAM_CAL),$(filter $(CONFIG_WING_CAM_CAL),y))
ccflags-y += -D__CONFIG_WING_CAM_CAL__
subdir-ccflags-y += -D__CONFIG_WING_CAM_CAL__
endif

ifeq ($(CONFIG_CONFIG_CAM_SC1300CS_LEAK),$(filter $(CONFIG_CAM_SC1300CS_LEAK),y))
ccflags-y += -D__CONFIG_CAM_SC1300CS_LEAK__
subdir-ccflags-y += -D__CONFIG_CAM_SC1300CS_LEAK__
endif

ifeq ($(CONFIG_CONFIG_MTK_S96818_CAMERA),$(filter $(CONFIG_MTK_S96818_CAMERA),y))
ccflags-y += -DCONFIG_MTK_S96818_CAMERA
subdir-ccflags-y += -DCONFIG_MTK_S96818_CAMERA
endif

$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP3_Z = $(CONFIG_MTK_IMGSENSOR_ISP3_Z))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP3_M = $(CONFIG_MTK_IMGSENSOR_ISP3_M))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP4_T = $(CONFIG_MTK_IMGSENSOR_ISP4_T))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP4_C = $(CONFIG_MTK_IMGSENSOR_ISP4_C))
$(info imgsensor CONFIG_MTK_IMGSENSOR = $(CONFIG_MTK_IMGSENSOR))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_MON = $(CONFIG_MTK_IMGSENSOR_ISP6S_MON))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_MOU = $(CONFIG_MTK_IMGSENSOR_ISP6S_MOU))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_LAG = $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG))
