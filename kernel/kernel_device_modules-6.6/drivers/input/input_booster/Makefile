subdir-ccflags-y += -Wno-error

obj-$(CONFIG_SEC_INPUT_BOOSTER)	+= input_booster_lkm.o
ifdef CONFIG_SEC_INPUT_BOOSTER_MODE
	input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER)	+= evdev_booster.o input_booster_mode.o
else
	input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER)	+= evdev_booster.o input_booster.o
endif

input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER_QC)	+= input_booster_qc.o
input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER_SLSI)	+= input_booster_lsi.o
input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER_MTK)	+= input_booster_mtk.o

# DDRfreq limit API
ifdef CONFIG_MTK_DVFSRC
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
endif
