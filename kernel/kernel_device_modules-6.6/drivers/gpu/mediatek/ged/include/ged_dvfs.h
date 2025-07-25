/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_DVFS_H__
#define __GED_DVFS_H__

#include <linux/types.h>
#include "ged_type.h"
#include "ged_log.h"

#define GED_DVFS_UM_CAL      1

#define GED_DVFS_PROBE_TO_UM 1
#define GED_DVFS_PROBE_IN_KM 0

#define GED_NO_UM_SERVICE    -1

#define GED_DVFS_VSYNC_OFFSET_SIGNAL_EVENT 44
#define GED_FPS_CHANGE_SIGNAL_EVENT        45
#define GED_SRV_SUICIDE_EVENT              46
#define GED_LOW_POWER_MODE_SIGNAL_EVENT    47
#define GED_MHL4K_VID_SIGNAL_EVENT         48
#define GED_GAS_SIGNAL_EVENT               49
#define GED_SIGNAL_BOOST_HOST_EVENT        50
#define GED_VILTE_VID_SIGNAL_EVENT         51
#define GED_LOW_LATENCY_MODE_SIGNAL_EVENT  52

/* GED_DVFS_DIFF_THRESHOLD (us) */
#define GED_DVFS_DIFF_THRESHOLD        500

#define GED_DVFS_TIMER_BACKUP          0x5566dead

#define GED_DVFS_LOADING_BASE_COMMIT    0
#define GED_DVFS_FRAME_BASE_COMMIT      1
#define GED_DVFS_FALLBACK_COMMIT        2
#define GED_DVFS_CUSTOM_CEIL_COMMIT     3
#define GED_DVFS_CUSTOM_BOOST_COMMIT    4
#define GED_DVFS_SET_BOTTOM_COMMIT      5
#define GED_DVFS_SET_LIMIT_COMMIT       6
#define GED_DVFS_INPUT_BOOST_COMMIT     7
#define GED_DVFS_DCS_STRESS_COMMIT      8
#define GED_DVFS_EB_DESIRE_COMMIT       9

#define GED_DVFS_COMMIT_TYPE            int

#define GED_DVFS_DEFAULT                0
#define GED_DVFS_LP                     1
#define GED_DVFS_JUST_MAKE              2
#define GED_DVFS_PERFORMANCE            3
#define GED_DVFS_TUNING_MODE            int

#define GED_EVENT_TOUCH            (1 << 0)
#define GED_EVENT_THERMAL          (1 << 1)
#define GED_EVENT_WFD              (1 << 2)
#define GED_EVENT_MHL              (1 << 3)
#define GED_EVENT_GAS              (1 << 4)
#define GED_EVENT_LOW_POWER_MODE   (1 << 5)
#define GED_EVENT_MHL4K_VID        (1 << 6)
#define GED_EVENT_BOOST_HOST       (1 << 7)
#define GED_EVENT_VR               (1 << 8)
#define GED_EVENT_VILTE_VID        (1 << 9)
#define GED_EVENT_LCD              (1 << 10)
#define GED_EVENT_LOW_LATENCY_MODE (1 << 13)
#define GED_EVENT_DHWC             (1 << 14)

#define LB_TIMEOUT_TYPE_REDUCE_MIPS     4
#define LB_TIMEOUT_REDUCE_MIPS          1

//Bring up flag
extern u32 g_is_bringup;

typedef void (*ged_event_change_fp)(void *private_data, int events);

bool mtk_register_ged_event_change(
	const char *name, void (*pfn_callback)(void *private_data, int events),
	void *private_data);
bool mtk_unregister_ged_event_change(const char *name);
void mtk_ged_event_notify(int events);

#define GED_EVENT_FORCE_ON       (1 << 0)
#define GED_EVENT_FORCE_OFF      (1 << 1)
#define GED_EVENT_NOT_SYNC       (1 << 2)

#define GED_VSYNC_OFFSET_NOT_SYNC -2
#define GED_VSYNC_OFFSET_SYNC     -3

#define GED_LB_SCALE_LIMIT 16

struct GED_DVFS_FREQ_DATA {
	unsigned int ui32Idx;
	unsigned long ulFreq;
};

struct GED_DVFS_BW_DATA {
	unsigned int ui32MaxBW;
	unsigned int ui32AvgBW;
};

#define MAX_BW_PROFILE 5

struct GpuUtilization_Ex {
	// unit for util_*: %
	unsigned int util_active;
	unsigned int util_3d;
	unsigned int util_ta;
	unsigned int util_compute;
	unsigned int util_iter;
	unsigned int util_mcu;
	unsigned int util_iter_u_mcu;

	unsigned int util_active_raw;
	unsigned int util_iter_raw;
	unsigned int util_mcu_raw;
	unsigned int util_irq_raw;
	unsigned int util_sc_comp_raw;
	unsigned int util_l2ext_raw;

	unsigned long long delta_time;   // unit: ns
	unsigned int freq;   // unit: kHz
};

enum ged_loading_type {
	GED_ACTIVE,
	GED_3D,
	GED_TA,
	GED_COMPUTE,
	GED_ITER,
	GED_MCU
};

enum ged_apo_legacy {
	GED_APO_LEGACY_INVALID,
	GED_APO_LEGACY_VER1,
	GED_APO_LEGACY_VER2,
	GED_APO_LEGACY_MAX
};

bool ged_dvfs_cal_gpu_utilization_ex(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle,
	struct GpuUtilization_Ex *Util_Ex);


void ged_dvfs_run(unsigned long t, long phase,
	unsigned long ul3DFenceDoneTime, GED_DVFS_COMMIT_TYPE eCommitType);

void ged_dvfs_set_tuning_mode(GED_DVFS_TUNING_MODE eMode);
GED_DVFS_TUNING_MODE ged_dvfs_get_tuning_mode(void);

GED_ERROR ged_dvfs_vsync_offset_event_switch(
	GED_DVFS_VSYNC_OFFSET_SWITCH_CMD eEvent, bool bSwitch);
void ged_dvfs_vsync_offset_level_set(int i32level);
int ged_dvfs_vsync_offset_level_get(void);

unsigned int ged_dvfs_get_gpu_loading(void);
unsigned int ged_dvfs_get_gpu_loading_avg(void);
unsigned int ged_dvfs_get_gpu_blocking(void);
unsigned int ged_dvfs_get_gpu_idle(void);
unsigned int ged_dvfs_get_custom_ceiling_gpu_freq(void);
unsigned int ged_dvfs_get_custom_boost_gpu_freq(void);
struct cmd_info ged_dvfs_get_custom_ceiling_gpu_freq_info(void);
struct cmd_info ged_dvfs_get_custom_boost_gpu_freq_info(void);
ssize_t ged_dvfs_get_custom_ceiling_gpu_freq_info_str(char *buf, int sz, ssize_t pos);
ssize_t ged_dvfs_get_custom_boost_gpu_freq_info_str(char *buf, int sz, ssize_t pos);

unsigned long ged_query_info(GED_INFO eType);

void ged_dvfs_get_gpu_cur_freq(struct GED_DVFS_FREQ_DATA *psData);
void ged_dvfs_get_gpu_pre_freq(struct GED_DVFS_FREQ_DATA *psData);

void ged_dvfs_sw_vsync_query_data(struct GED_DVFS_UM_QUERY_PACK *psQueryData);

void ged_dvfs_boost_gpu_freq(void);

GED_ERROR ged_dvfs_probe(int pid);
GED_ERROR ged_dvfs_um_commit(unsigned long gpu_tar_freq, bool bFallback);

GED_ERROR  ged_dvfs_probe_signal(int signo);

enum ged_gpu_power_state {
	GED_POWER_OFF,
	GED_SLEEP,
	GED_POWER_ON,
};
void ged_dvfs_gpu_clock_switch_notify(enum ged_gpu_power_state power_state);
void ged_gpu_autosuspend_timeout_notify(int autosuspend_timeout_ms);

unsigned int ged_gpu_whitebox_power_test_case(int replace);
unsigned int ged_gpu_whitebox_power_test_support(int support_flag);
unsigned int ged_get_whitebox_power_test_case(void);
unsigned int ged_get_whitebox_power_test_support(void);
unsigned int ged_get_whitebox_power_test_case_clear(void);
void ged_set_whitebox_power_state_store(int first, int second);

//#if IS_ENABLED(CONFIG_MTK_GPU_POWER_ON_OFF_TEST)
unsigned int ged_gpu_power_stress_test_enable(void);
//endif /* MTK_GPU_POWER_ON_OFF_TEST */

//#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
unsigned int ged_gpu_apo_support(void);
unsigned long long ged_get_apo_thr_ns(void);
void ged_set_apo_thr_ns(unsigned long long apo_thr_ns);
unsigned long long ged_get_apo_wakeup_ns(void);
void ged_set_apo_wakeup_ns_nolock(unsigned long long apo_wakeup_ns);
void ged_set_apo_wakeup_ns(unsigned long long apo_wakeup_ns);
unsigned long long ged_get_apo_lp_thr_ns(void);
void ged_set_apo_lp_thr_ns(unsigned long long apo_lp_thr_ns);
int ged_get_apo_hint(void);
int ged_get_apo_force_hint(void);
void ged_set_apo_force_hint(int apo_force_hint);
void ged_set_apo_status(int apo_status);
void ged_set_apo_legacy(enum ged_apo_legacy apo_legacy);
enum ged_apo_legacy ged_get_apo_legacy(void);


void ged_get_gpu_frame_time(int frame_time);
void ged_get_active_time(void);
void ged_get_idle_time(void);
void ged_check_power_duration(void);
unsigned long long ged_get_power_duration(void);
void ged_gpu_apo_init_nolock(void);
void ged_gpu_apo_reset_nolock(void);
void ged_gpu_apo_reset(void);
bool ged_gpu_apo_notify(void);

void ged_get_predict_active_time(void);
void ged_get_predict_idle_time(void);
bool ged_check_predict_power_autosuspend_nolock(void);
bool ged_check_predict_power_autosuspend(void);
void ged_check_predict_power_duration(void);
unsigned long long ged_get_predict_power_duration(void);
void ged_gpu_predict_apo_init_nolock(void);
void ged_gpu_predict_apo_reset_nolock(void);
void ged_gpu_predict_apo_reset(void);
bool ged_gpu_predict_apo_notify(void);

unsigned int ged_get_apo_autosuspend_delay_ms(void);
int ged_get_apo_autosuspend_delay_ref_count(void);
void ged_set_apo_autosuspend_delay_ctrl(int ctrl);
int ged_get_apo_autosuspend_delay_ctrl(void);
int ged_get_apo_autosuspend_delay_target_ref_count(void);
void ged_set_apo_autosuspend_delay_target_ref_count(int apo_autosuspend_delay_target_ref_count);
void ged_set_apo_autosuspend_delay_ms_ref_idletime_nolock(long long idle_time);
void ged_set_apo_autosuspend_delay_ms_ref_idletime(long long idle_time);
void ged_set_apo_autosuspend_delay_ms_nolock(unsigned int apo_autosuspend_delay_ms);
void ged_set_apo_autosuspend_delay_ms(unsigned int apo_autosuspend_delay_ms);
bool ged_gpu_is_heavy(void);

//#endif /* CONFIG_MTK_GPU_APO_SUPPORT */
// for autosuspend_stress
int ged_get_autosuspend_stress(void);
void ged_set_autosuspend_stress(int enable);

GED_ERROR ged_dvfs_system_init(void);
void ged_dvfs_system_exit(void);
unsigned long ged_dvfs_get_last_commit_idx(void);
unsigned long ged_dvfs_get_last_commit_top_idx(void);
unsigned long ged_dvfs_get_last_commit_stack_idx(void);
unsigned long ged_dvfs_get_last_commit_dual_idx(void);
unsigned long ged_dvfs_write_sysram_last_commit_idx(void);
unsigned long ged_dvfs_write_sysram_last_commit_idx_test(int commit_idx);
unsigned long ged_dvfs_write_sysram_last_commit_top_idx(void);
unsigned long ged_dvfs_write_sysram_last_commit_top_idx_test(int commit_idx);
unsigned long ged_dvfs_write_sysram_last_commit_stack_idx(void);
unsigned long ged_dvfs_write_sysram_last_commit_stack_idx_test(int commit_idx);
unsigned long ged_dvfs_write_sysram_last_commit_dual(void);
unsigned long ged_dvfs_write_sysram_last_commit_dual_test(int top_idx, int stack_idx);
void ged_dvfs_set_sysram_last_commit_top_idx(int commit_idx);
void ged_dvfs_set_sysram_last_commit_dual_idx(int top_idx, int stack_idx);
void ged_dvfs_set_sysram_last_commit_stack_idx(int commit_idx);
unsigned int ged_dvfs_write_sysram_protm_enter(void);
unsigned int ged_dvfs_write_sysram_protm_exit(void);
int ged_write_sysram_pwr_hint(int pwr_hint);
int ged_dvfs_update_step_size(int low_step, int med_step, int high_step);
void ged_dvfs_record_soc_timer(u64 soc_timer);


extern void (*ged_kpi_set_gpu_dvfs_hint_fp)(int t_gpu_target,
	int boost_accum_gpu);

extern int (*ged_kpi_gpu_dvfs_fp)(int t_gpu, int t_gpu_target,
	int target_fps_margin, unsigned int force_fallback);
extern void (*ged_kpi_trigger_fb_dvfs_fp)(void);
extern int (*ged_kpi_check_if_fallback_mode_fp)(void);

extern void mtk_gpu_ged_hint(int a, int b);
int ged_dvfs_boost_value(void);
void start_mewtwo_timer(void);
void cancel_mewtwo_timer(void);

extern void (*mtk_dvfs_margin_value_fp)(int i32MarginValue);
extern int (*mtk_get_dvfs_margin_value_fp)(void);
extern int ged_get_dvfs_margin(void);
extern unsigned int ged_get_dvfs_margin_mode(void);

extern void (*mtk_loading_base_dvfs_step_fp)(int i32MarginValue);
extern int (*mtk_get_loading_base_dvfs_step_fp)(void);

extern void (*mtk_timer_base_dvfs_margin_fp)(int i32MarginValue);
extern int (*mtk_get_timer_base_dvfs_margin_fp)(void);
extern void (*ged_kpi_fastdvfs_update_dcs_fp)(void);
int ged_dvfs_get_tb_dvfs_margin_cur(void);
unsigned int ged_dvfs_get_tb_dvfs_margin_mode(void);
int ged_dvfs_get_margin_value_min_cmd(void);
int ged_dvfs_get_margin_step(void);
void set_api_sync_flag(int flag);
int get_api_sync_flag(void);
void ged_reset_api_sync_ts(void);
unsigned long long ged_get_api_sync_ts(void);
#define LOADING_ACTIVE 0
#define LOADING_MAX_3DTA_COM 1
#define LOADING_MAX_3DTA 2
#define LOADING_3D 3
#define LOADING_ITER 4
#define LOADING_MAX_ITERMCU 5
#define LOADING_UNION_ITERMCU 6


#define WORKLOAD_ACTIVE 0
#define WORKLOAD_3D 3
#define WORKLOAD_ITER 4
#define WORKLOAD_MAX_ITERMCU 5
#define WORKLOAD_UNION_ITERMCU 6

extern void (*mtk_dvfs_loading_mode_fp)(int i32LoadingMode);
extern int (*mtk_get_dvfs_loading_mode_fp)(void);
extern void (*mtk_dvfs_workload_mode_fp)(int i32WorkloadMode);
extern int (*mtk_get_dvfs_workload_mode_fp)(void);
extern void ged_get_gpu_utli_ex(struct GpuUtilization_Ex *util_ex);
#ifndef MAX
#define MAX(x, y)	((x) < (y) ? (y) : (x))
#endif
#ifndef MIN
#define MIN(x, y)      ((x) < (y) ? (x) : (y))
#endif

extern unsigned int g_gpufreq_v2;

extern void (*mtk_set_fastdvfs_mode_fp)(unsigned int u32Mode);
extern void (*mtk_get_fastdvfs_mode_fp)(void *ipi_data);

extern unsigned int g_eb_workload;
extern unsigned int eb_policy_dts_flag;

enum ged_step_size_freq_th {
	GED_STEP_FREQ_LOW,
	GED_STEP_FREQ_MID,
	GED_STEP_FREQ_HIGH,
};

void ged_dvfs_enable_async_ratio(int enableAsync);
void ged_dvfs_enable_lb_async_ratio(int enableAsync);
void ged_dvfs_force_top_oppidx(int idx);
void ged_dvfs_force_stack_oppidx(int idx);
void ged_dvfs_set_async_log_level(unsigned int level);
int ged_dvfs_get_async_ratio_support(void);
int ged_dvfs_get_lb_async_ratio_support(void);
int ged_dvfs_get_top_oppidx(void);
int ged_dvfs_get_stack_oppidx(void);
unsigned int ged_dvfs_get_async_log_level(void);
void ged_dvfs_set_slide_window_size(int size);
void ged_dvfs_set_uncomplete_ts_type(int type);
void ged_dvfs_notify_power_off(void);
void ged_dvfs_set_fallback_tuning(int tuning);
int ged_dvfs_get_fallback_tuning(void);
void ged_dvfs_set_async_perf_model(int version);
int ged_dvfs_get_async_perf_model(void);
void ged_dvfs_set_lb_async_perf_diff(int perfDiffTH);
int ged_dvfs_get_lb_async_perf_diff(void);
void ged_dvfs_write_sysram_virtual_commit_idx(int top_idx, int stack_idx);
int ged_dvfs_get_lb_win_size_cmd(void);

/* MBrain */
//GPU freq part:
#define MAX_OPP_LOGS_COLUMN_DIGITS 64
struct GED_DVFS_OPP_STAT {
	/* unit: ms */
	uint64_t ui64Active;
	uint64_t ui64Idle;
};
void ged_dvfs_reset_opp_cost(int oppsize);
int ged_dvfs_query_opp_cost(struct GED_DVFS_OPP_STAT *psReport,
				int i32NumOpp, bool bStript, u64 *last_ts);
int ged_dvfs_init_opp_cost(void);
int ged_dvfs_get_real_oppfreq_num(void);
extern unsigned int ged_kpi_enabled(void);

//GPU power state part:
int ged_dvfs_query_power_state_time(u64 *off_time, u64 *idle_time, u64 *on_time, u64 *last_ts);

//GPU average loading part:
int ged_dvfs_query_loading(u64 *sum_loading, u64 *sum_delta_time);
/* MBrain end */
#endif
