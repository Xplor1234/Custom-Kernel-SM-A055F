#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

MTK_CUSTOM_PATH := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_CUSTOM_SRC_PATH := $(CAM_CAL_SRC_PATH)/custom
COMMON_VERSION := v2

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_CUSTOM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION)

# Common code
obj-$(CONFIG_MTK_CAM_CAL) += camera_eeprom.o
camera_eeprom-objs := ../common/$(COMMON_VERSION)/eeprom_driver.o
camera_eeprom-objs += ../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o
camera_eeprom-objs += ../common/$(COMMON_VERSION)/eeprom_utils.o

# Platform code
camera_eeprom-objs += eeprom_i2c_custom_driver.o
camera_eeprom-objs += cam_cal_list.o

ifeq ($(CONFIG_CONFIG_WING_CAM_CAL),$(filter $(CONFIG_WING_CAM_CAL),y))
camera_eeprom-objs += eeprom_wing_custom.o
ccflags-y += -D__CONFIG_WING_CAM_CAL__
subdir-ccflags-y += -D__CONFIG_WING_CAM_CAL__
endif