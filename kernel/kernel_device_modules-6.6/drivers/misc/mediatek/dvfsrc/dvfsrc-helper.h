/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_DEBUG_H
#define __DVFSRC_DEBUG_H

#include <dvfsrc-exp.h>

#define DUMP_BUF_SIZE 4096

struct mtk_dvfsrc;

/* DEBUG_FLAG */
#define DVFSRC_EMI_DUMP_FLAG         (1 << 0)

/* For ceiling ddr gear operation */
enum {
	CEILING_SYSFS,
	CEILING_IDX1,
	CEILING_IDX2,
	CEILING_FORCE_MODE, /*front of CEILING_ITEM_MAX*/
	CEILING_ITEM_MAX,
};

/* opp */
struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
	u32 emi_opp;
	u32 vcore_uv;
	u32 dram_kbps;
	u32 emi_mbps;
};

struct dvfsrc_opp_desc {
	int num_vcore_opp;
	int num_dram_opp;
	int num_emi_opp;
	int num_opp;
	struct dvfsrc_opp *opps;
};

struct dvfsrc_qos_config {
	const int *ipi_pin;
	int (*qos_dvfsrc_init)(struct mtk_dvfsrc *dvfs);
};

struct dvfsrc_opp_data {
	u32 num_opp_desc;
	struct dvfsrc_opp_desc *opps_desc;
	const struct dvfsrc_qos_config *qos;
	void (*setup_opp_table)(struct mtk_dvfsrc *dvfsrc);
};

/* debug */
struct dvfsrc_config {
	u32 ip_version;
	const int *regs;
	const int *spm_regs;
	char *(*dump_reg)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_record)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_spm_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_vmode_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_spm_cmd)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_spm_timer_latch)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_md_floor_table)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	char *(*dump_vcore_avs_zone)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	int (*query_request)(struct mtk_dvfsrc *dvfsrc, u32 id);
	u64 (*query_dvfs_time)(struct mtk_dvfsrc *dvfsrc);
	u32 (*query_opp_count)(struct mtk_dvfsrc *dvfsrc);
	u32 (*query_opp_gear_info)(struct mtk_dvfsrc *dvfsrc, u32 idx);
	void (*set_vcore_avs)(struct mtk_dvfsrc *dvfsrc, u32 enable, u32 bit);
	void (*set_ddr_ceiling)(struct mtk_dvfsrc *dvfsrc, u32 gear, bool force_en);
};

struct dvfsrc_debug_data {
	u32 num_opp_desc;
	u32 version;
	u32 dump_flag;
	u32 emi_opp_req_enmode;
	struct dvfsrc_opp_desc *opps_desc;
	const struct dvfsrc_config *config;
	const struct dvfsrc_qos_config *qos;
	bool spm_stamp_en;
	bool ceiling_support;
	bool qos_mm_mode_en;
	bool mmdvfs_notify;
};

struct mtk_dvfsrc {
	struct device *dev;
/* opp */
	int opp_type;
	int fw_type;
	struct dvfsrc_opp_desc *opp_desc;
	u32 *vopp_uv_tlb;
	u32 *dopp_kbps_tlb;
	u32 *eopp_mbps_tlb;
/* debug */
	void __iomem *regs;
	void __iomem *spm_regs;
	struct icc_path *bw_path;
	struct icc_path *perf_path;
	struct icc_path *hrt_path;
	struct regulator *vcore_power;
	struct regulator *dvfsrc_vcore_power;
	int num_perf;
	u32 *perfs_peak_bw;
	u32 force_opp_idx;
	const struct dvfsrc_debug_data *dvd;
	char *(*dump_info)(struct mtk_dvfsrc *dvfsrc, char *p, u32 size);
	void (*force_opp)(struct mtk_dvfsrc *dvfsrc, u32 opp);
	struct mutex dump_lock;
	struct notifier_block debug_notifier;
	char dump_buf[DUMP_BUF_SIZE];
	u32 vcore_vsel_reg;
	u32 vcore_vsel_mask;
	u32 vcore_vsel_shift;
	u32 vcore_range_step;
	u32 vcore_range_min_uV;
	u32 qos_mode;
	u32 qos_mm_mode;
	u32 vcore_avs_enable;
	u8 ceil_ddr_opp[CEILING_ITEM_MAX];
	bool ceil_ddr_support;
	bool vchk_enable;
	bool force_ddr_en;
	u32 force_ddr_idx;
};

extern int dvfsrc_register_sysfs(struct device *dev);
extern void dvfsrc_unregister_sysfs(struct device *dev);
extern int dvfsrc_register_debugfs(struct device *dev);
extern void dvfsrc_unregister_debugfs(struct device *dev);

extern const struct dvfsrc_config mt6768_dvfsrc_config;
extern const struct dvfsrc_qos_config mt6768_qos_config;
extern const struct dvfsrc_qos_config mt6765_qos_config;
extern const struct dvfsrc_config mt6779_dvfsrc_config;
extern const struct dvfsrc_config mt6873_dvfsrc_config;
extern const struct dvfsrc_config mt6893_dvfsrc_config;
extern const struct dvfsrc_config mt6877_dvfsrc_config;
extern const struct dvfsrc_config mt6983_dvfsrc_config;
extern const struct dvfsrc_config mt6897_dvfsrc_config;
extern const struct dvfsrc_config mt6989_dvfsrc_config;

extern const struct dvfsrc_qos_config mt6761_qos_config;
#endif

