# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/w2gc02m1microcxt_mipi_raw_Sensor.o \

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MON), m y))
imgsensor_isp6s_mon-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MOU), m y))
imgsensor_isp6s_mou-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG), m y))
imgsensor_isp6s_lag-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

