# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MTK_PLATFORM := isp4_t
MTK_SENSOR_PROJECT :=
COMMON_VERSION := v1
IMGSENSOR_DRIVER_PATH  := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/src
MTK_PLATFORM_KO := imgsensor_$(MTK_PLATFORM)
IMGSENSOR_DRIVER_OBJ := \
	../common/$(COMMON_VERSION)/imgsensor.o \
	../common/$(COMMON_VERSION)/imgsensor_hw.o \
	../common/$(COMMON_VERSION)/imgsensor_i2c.o \
	../common/$(COMMON_VERSION)/imgsensor_legacy.o \
	../common/$(COMMON_VERSION)/imgsensor_proc.o \
	../common/$(COMMON_VERSION)/imgsensor_sensor_list.o \
	imgsensor_clk.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), m)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(MTK_PLATFORM_KO).o
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_DRIVER_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(IMGSENSOR_DRIVER_OBJ)
endif

subdir-ccflags-y += \
    -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/ \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/ \
	-DNO_I2C_MTK -DNO_CLK_METER -DIMGSENSOR_USE_RPM -DIMGSENSOR_DFS_CTRL_ENABLE -DSENINF_USE_WAKE_LOCK

#For isp4_t ref only


ifeq ($(CONFIG_CAMEREA_TB8786P2), m)
subdir-ccflags-y += -DIMGSENSOR_TB8786P2
else
subdir-ccflags-y += -DIMGSENSOR_ISP4_T_REF
endif


define add_subdrv
$(eval subdrv-rpath = $1)
$(eval include $(srctree)/$(src)/$1/Makefile)
endef

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/$(1)),)
unknown_drv += $(1)
else
common_drv += $(1)
endif
else
platform_drv += $(1)
endif
else
project_drv += $(1)
endif
endef

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

CUSTOM_KERNEL_KNOWN_DRV := $(filter-out $(unknown_drv),$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)))
CUSTOM_KERNEL_FLAGS := $(addprefix -D, $(shell echo $(CUSTOM_KERNEL_KNOWN_DRV) | tr a-z A-Z))
subdir-ccflags-y += $(CUSTOM_KERNEL_FLAGS)

ifneq ($(unknown_drv),)
$(info unknown_drv: $(unknown_drv))
endif

ifneq ($(common_drv),)
$(info common_drv : $(common_drv))
$(foreach drv,$(common_drv),$(eval $(call add_subdrv,../common/$(COMMON_VERSION)/$(drv))))
endif

ifneq ($(platform_drv),)
$(info platform_drv : $(platform_drv))
$(foreach drv,$(platform_drv),$(eval $(call add_subdrv,$(drv))))
endif

ifneq ($(project_drv),)
$(info project_drv : $(project_drv))
$(foreach drv, $(project_drv), $(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/$(drv))))
endif

ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
$(eval $(call add_subdrv,camera_hw))

ifneq ($(project_drv),)
$(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
endif

else
$(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/camera_hw
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
endif

ifeq ($(CONFIG_CONFIG_WING_CAM_CAL),$(filter $(CONFIG_WING_CAM_CAL),y))
ccflags-y += -D__CONFIG_WING_CAM_CAL__
subdir-ccflags-y += -D__CONFIG_WING_CAM_CAL__
endif
