# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := isp4_t
MTK_CUSTOM_PATH := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_PLATFORM_SRC_PATH := $(CAM_CAL_SRC_PATH)/$(MTK_PLATFORM)
COMMON_VERSION := v1
MTK_PLATFORM_KO := camera_eeprom_$(MTK_PLATFORM)

CAM_CAL_OBJ := \
	../common/$(COMMON_VERSION)/eeprom_driver.o \
	../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o \
	eeprom_i2c_custom_driver.o \
	cam_cal_list.o \
	eeprom_i2c_dev.o \

ifeq ($(CONFIG_CONFIG_WING_CAM_CAL),$(filter $(CONFIG_WING_CAM_CAL),y))
CAM_CAL_OBJ += ../custom/eeprom_wing_custom.o
ccflags-y += -D__CONFIG_WING_CAM_CAL__
subdir-ccflags-y += -D__CONFIG_WING_CAM_CAL__
ccflags-y += -D__CONFIG_WING_CAM_CAL__V1__
subdir-ccflags-y += -I$(CAM_CAL_SRC_PATH)/common/v2
subdir-ccflags-y += -I$(CAM_CAL_SRC_PATH)/custom
endif

ifeq ($(CONFIG_MTK_CAM_CAL_ISP4_T), m)
obj-$(CONFIG_MTK_CAM_CAL_ISP4_T) += $(MTK_PLATFORM_KO).o
$(eval $(MTK_PLATFORM_KO)-objs += $(CAM_CAL_OBJ))
else ifeq ($(CONFIG_MTK_CAM_CAL_ISP4_T), y)
obj-$(CONFIG_MTK_CAM_CAL_ISP4_T) += $(CAM_CAL_OBJ)
endif

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_PLATFORM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION) \
                    -I$(srctree)/drivers/i2c/busses/
