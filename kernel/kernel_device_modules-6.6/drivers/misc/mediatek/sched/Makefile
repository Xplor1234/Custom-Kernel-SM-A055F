obj-$(CONFIG_MTK_CPUFREQ_SUGOV_EXT) += cpufreq_sugov_ext.o
cpufreq_sugov_ext-y += sugov/cpufreq_sugov_main.o
ifeq ($(CONFIG_MTK_GEARLESS_SUPPORT),y)
	cpufreq_sugov_ext-y += mtk_energy_model/v3/static_power.o
else
	cpufreq_sugov_ext-y += mtk_energy_model/v1/static_power.o
	ifeq ($(CONFIG_MTK_LEAKAGE_HIT),y)
		subdir-ccflags-y += -DCFG_LKG_HIT
	endif
endif
cpufreq_sugov_ext-y += sugov/nonlinear_opp_cap.o
cpufreq_sugov_ext-y += sugov/dsu_interface.o
cpufreq_sugov_ext-y += sugov/sched_version_ctrl.o
cpufreq_sugov_ext-y += common.o

obj-$(CONFIG_MTK_SCHEDULER) += scheduler.o
scheduler-y += eas/sched_main.o
scheduler-y += eas/sched_sys_common.o
scheduler-y += eas/eas_plus.o
scheduler-y += eas/rt.o
scheduler-y += eas/topology.o
scheduler-y += eas/eas_adpf.o
scheduler-y += common.o
scheduler-y += fair.o
scheduler-$(CONFIG_MTK_CORE_PAUSE)+= eas/core_pause.o
scheduler-$(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)+= eas/rotate.o
scheduler-$(CONFIG_MTK_SCHED_VIP_TASK)+= eas/vip.o
scheduler-$(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)+= eas/flt_init.o
scheduler-$(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)+= eas/flt_api.o
scheduler-$(CONFIG_MTK_SCHED_GROUP_AWARE)+= eas/group.o
scheduler-$(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)+= eas/flt_utility_v2.o
scheduler-$(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)+= eas/flt_utility_v3.o
scheduler-$(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)+= eas/flt_utility_v4.o
scheduler-$(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)+= eas/flt_cal_build_utility.o

subdir-ccflags-y += -I$(srctree)/kernel/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/thermal/mediatek
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/cpufreq/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/upower

obj-$(CONFIG_MTK_CORE_CTL) += mtk_core_ctl.o
mtk_core_ctl-y += core_ctl/sched_avg.o
mtk_core_ctl-y += core_ctl/core_ctl.o
