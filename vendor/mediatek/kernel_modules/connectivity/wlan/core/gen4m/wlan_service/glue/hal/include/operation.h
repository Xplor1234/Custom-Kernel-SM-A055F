/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __OPERATION_H__
#define __OPERATION_H__

#include "test_mac.h"
#include "net_adaption.h"

extern int8_t g_hqa_frame_ctrl;

#define INC_RING_INDEX1(_idx, _RingSize)		\
{ \
	(_idx) = (_idx+1) % (_RingSize); \
}

/*****************************************************************************
 *	Enum value definition
 *****************************************************************************/
/* gen4m object id list */
enum op_wlan_oid {
	OP_WLAN_OID_SET_TEST_MODE_START = 0,
	OP_WLAN_OID_SET_TEST_MODE_ABORT = 1,
	OP_WLAN_OID_RFTEST_SET_AUTO_TEST = 2,
	OP_WLAN_OID_QUERY_RX_STATISTICS = 3,
	OP_WLAN_OID_SET_TEST_ICAP_MODE = 4,
	OP_WLAN_OID_RFTEST_QUERY_AUTO_TEST  = 5,
	OP_WLAN_OID_SET_MCR_WRITE = 6,
	OP_WLAN_OID_QUERY_MCR_READ = 7,
	OP_WLAN_OID_GET_RECAL_COUNT = 8,
	OP_WLAN_OID_GET_RECAL_CONTENT = 9,
	OP_WLAN_OID_GET_ANTSWAP_CAPBILITY = 10,
	OP_WLAN_OID_SET_TEST_ICAP_START = 11,
	OP_WLAN_OID_SET_TEST_ICAP_ABORT = 12,
	OP_WLAN_OID_SET_TEST_ICAP_STATUS = 13,
	OP_WLAN_OID_GET_TEST_ICAP_MAX_DATA_LEN = 14,
	OP_WLAN_OID_GET_TEST_ICAP_DATA = 15,
	OP_WLAN_OID_RESET_RECAL_COUNT = 16,
	OP_WLAN_OID_GET_CAPABILITY = 17,
	OP_WLAN_OID_LIST_MODE = 18,
	OP_WLAN_OID_SET_TEST_RDD_START = 19,
	OP_WLAN_OID_SET_TEST_RDD_STOP = 20,
	OP_WLAN_OID_GET_RDD_CNT = 21,
	OP_WLAN_OID_GET_RDD_CONTENT = 22,
	OP_WLAN_OID_SET_LOG_ONFF = 23,
	OP_WLAN_OID_GET_EFUSE_FREE_BLOCK = 25,
	OP_WLAN_OID_EPRM_READ = 26,
	OP_WLAN_OID_EPRM_WRITE = 27,
	OP_WLAN_OID_NUM
};

enum ENUM_ANT_NUM {
	ANT_WF0 = 0,
	ANT_WF1 = 1,
	MAX_ANT_NUM
};

enum ENUM_M_BAND_NUM {
	M_BAND_0 = 0,
	M_BAND_1 = 1,
	M_BAND_NUM
};

/*****************************************************************************
 *	Structure definition
 *****************************************************************************/
struct param_rdd_log_struct {
	u_int32 band_idx;
	u_int32 log_size;
	u_int32 log_ctrl;
};

struct param_mtk_wifi_test_struct {
	u_int32 func_idx;
	u_int32 func_data;
};

#if (CFG_SUPPORT_CONNAC3X == 0) && (CFG_SUPPORT_CONNAC5X == 0)
struct param_custom_access_rx_stat {
	u_int32 seq_num;
	u_int32 total_num;
};

#else
struct param_custom_access_rx_stat {
	u_int16 seq_num;
	u_int8  band_idx;
	/* bit[0] in event structure will tell new / old firmware format */
	u_int8  data;
	u_int32 total_num;
};
#endif

struct param_custom_mcr_rw_struct {
	u_int32 mcr_offset;
	u_int32 mcr_data;
};

struct test_txpwr_cfg {
	u_int32 ant_idx;
	u_int32 txpwr;
	u_int32 channel;
	u_int32 band_idx;
	u_int32 ch_band;
};

struct test_ch_cfg {
	u_int8 ctrl_ch;
	u_int8 ctrl_ch2;
	u_int8 central_ch;
	u_int8 bw;
	u_int8 tx_strm;
	u_int8 rx_path;
	boolean scan;
	boolean dfs_check;
	u_int8 band_idx;
	u_int8 ch_band;
	u_int32 out_band_freq;
};

/* Test rbist status for hqa command usage*/
struct GNU_PACKED hqa_rbist_cap_start {
	u_int32 trig;
	u_int32 ring_cap_en;
	u_int32 trig_event;
	u_int32 cap_node;
	u_int32 cap_len;
	u_int32 cap_stop_cycle;
	u_int32 mac_trig_event;
	u_int32 src_addr_lsb;
	u_int32 src_addr_msb;
	u_int32 band_idx;
	u_int32 bw;
	u_int32 en_bit_width; /* 0:32bit, 1:96bit, 2:128bit */
	u_int32 arch; /* 0:on-chip, 1:on-the-fly */
	u_int32 phy_idx;
	u_int32 emi_start_addr;
	u_int32 emi_end_addr;
	u_int32 emi_msb_addr;
	u_int32 cap_src;
	u_int32 resv[2];
};

struct test_struct_ext {
	u_int32 func_idx;
	union {
		u_int32 func_data;
		u_int32 cal_dump;
		struct hqa_rbist_cap_start icap_info;
	} data;
};

struct test_rdd_params {
	u_int32 rdd_idx;
	u_int32 rdd_sel;
};

struct test_rdd_dump_params {
	u_int32 rdd_cnt;
	u_int32 rdd_dw_num;
};

/* Pulse size * num of pulse = 8 * 32 for one event*/
#define TEST_RDD_LOG_SIZE 8
struct test_rdd_log {
	u_int32 prefix;
	u_int32 cnt;
	u_int8 by_pass;
	u_int8 buffer[TEST_RDD_LOG_SIZE];
};

struct test_log_dump_entry {
	u_int32 log_type;
	u_int8 un_dumped;
	struct test_rdd_log rdd;
};

struct test_log_dump_cb {
	u_int8 overwritable; // UINT8 overwritable;
	u_int8 is_dumping; // UINT8 is_dumping;
	u_int8 is_overwritten; // UINT8 is_overwritten;
	s_int32 idx; // INT32 idx;
	s_int32 len; // INT32 len;
	u_int32 recal_curr_type; // UINT32 recal_curr_type;
	struct test_log_dump_entry *entry;
};

/*****************************************************************************
 *	Function declaration
 *****************************************************************************/
s_int32 mt_op_set_tr_mac(
	struct test_wlan_info *winfos,
	s_int32 op_type, boolean enable, u_char band_idx);
s_int32 mt_op_set_tx_stream(
	struct test_wlan_info *winfos,
	u_int32 stream_nums, u_char band_idx);
s_int32 mt_op_set_tx_path(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_set_rx_path(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_set_rx_filter(
	struct test_wlan_info *winfos,
	struct rx_filter_ctrl rx_filter);
s_int32 mt_op_set_clean_persta_txq(
	struct test_wlan_info *winfos,
	boolean sta_pause_enable, u_char omac_idx, u_char band_idx);
s_int32 mt_op_set_cfg_on_off(
	struct test_wlan_info *winfos,
	u_int32 type,
	u_int32 enable,
	u_int32 band_idx,
	u_int32 ch_band);
s_int32 mt_op_log_on_off(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 log_type,
	u_int32 log_ctrl,
	u_int32 log_size);
s_int32 mt_op_set_antenna_port(
	struct test_wlan_info *winfos,
	u_int8 rf_mode_mask, u_int8 rf_port_mask, u_int8 ant_port_mask);
s_int32 mt_op_set_slot_time(
	struct test_wlan_info *winfos,
	u_int8 slot_time, u_int8 sifs_time, u_int8 rifs_time,
	u_int16 eifs_time, u_char band_idx);
s_int32 mt_op_set_power_drop_level(
	struct test_wlan_info *winfos,
	u_int8 pwr_drop_level, u_char band_idx);
s_int32 mt_op_set_rx_filter_pkt_len(
	struct test_wlan_info *winfos,
	u_int8 enable, u_char band_idx, u_int32 rx_pkt_len);
s_int32 mt_op_get_antswap_capability(
	struct test_wlan_info *winfos,
#if (CFG_SUPPORT_CONNAC3X == 1) || (CFG_SUPPORT_CONNAC5X == 1)
	u_char band_idx,
#endif /* (CFG_SUPPORT_CONNAC3X == 1) || (CFG_SUPPORT_CONNAC5X == 1) */
	u_int32 *antswap_support);
s_int32 mt_op_set_antswap(
	struct test_wlan_info *winfos,
	u_int32 ant);
s_int32 mt_op_set_freq_offset(
	struct test_wlan_info *winfos,
	u_int32 freq_offset, u_char band_idx);
s_int32 mt_op_set_phy_counter(
	struct test_wlan_info *winfos,
	s_int32 control, u_char band_idx);
s_int32 mt_op_set_rxv_index(
	struct test_wlan_info *winfos,
	u_int8 group_1, u_int8 group_2, u_char band_idx);
s_int32 mt_op_set_fagc_path(
	struct test_wlan_info *winfos,
	u_int8 path, u_char band_idx);
s_int32 mt_op_set_fw_mode(
	struct test_wlan_info *winfos, u_char fw_mode);
s_int32 mt_op_set_rf_test_mode(
	struct test_wlan_info *winfos,
	u_int32 op_mode, u_int8 icap_len, u_int16 rsp_len);
s_int32 mt_op_start_tx(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_stop_tx(
	struct test_wlan_info *winfos,
	u_char band_idx);
s_int32 mt_op_start_rx(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_stop_rx(
	struct test_wlan_info *winfos,
	u_char band_idx);
s_int32 mt_op_set_channel(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_set_tx_content(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_set_tmr(
	struct test_wlan_info *winfos,
	struct test_tmr_info *tmr_info);
s_int32 mt_op_set_preamble(
	struct test_wlan_info *winfos,
	u_char mode);
s_int32 mt_op_set_rate(
	struct test_wlan_info *winfos,
	u_char mcs);
s_int32 mt_op_set_system_bw(
	struct test_wlan_info *winfos,
	u_char sys_bw);
s_int32 mt_op_set_per_pkt_bw(
	struct test_wlan_info *winfos,
	u_char per_pkt_bw);
s_int32 mt_op_reset_txrx_counter(
	struct test_wlan_info *winfos);
s_int32 mt_op_set_rx_vector_idx(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 group1,
	u_int32 group2);
s_int32 mt_op_set_fagc_rssi_path(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 fagc_path);
s_int32 mt_op_get_rx_stat_leg(
	struct test_wlan_info *winfos,
	struct test_rx_stat_leg *rx_stat);
s_int32 mt_op_dbdc_tx_tone(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_dbdc_tx_tone_pwr(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_dbdc_continuous_tx(
	struct test_wlan_info *winfos,
	u_char band_idx,
	struct test_configuration *configs);
s_int32 mt_op_get_tx_info(
	struct test_wlan_info *winfos,
	struct test_configuration *test_configs_band0,
	struct test_configuration *test_configs_band1,
	struct test_configuration *test_configs_band2);
s_int32 mt_op_get_rx_statistics_all(
	struct test_wlan_info *winfos,
	struct hqa_comm_rx_stat *hqa_rx_stat);
s_int32 mt_op_get_capability(
	struct test_wlan_info *winfos,
	struct test_capability *capability);
s_int32 mt_op_calibration_test_mode(
	struct test_wlan_info *winfos,
	u_char mode);
s_int32 mt_op_set_icap_start(
	struct test_wlan_info *winfos,
	u_int8 *data);
s_int32 mt_op_get_icap_status(
	struct test_wlan_info *winfos,
	s_int32 *icap_stat);
s_int32 mt_op_get_icap_max_data_len(
	struct test_wlan_info *winfos,
	u_long *max_data_len);
s_int32 mt_op_get_icap_data(
	struct test_wlan_info *winfos,
	s_int32 *icap_cnt,
	s_int32 *icap_data,
	u_int32 wf_num,
	u_int32 iq_type);
s_int32 mt_op_do_cal_item(
	struct test_wlan_info *winfos,
	u_int32 item,
	u_char band_idx);
s_int32 mt_op_set_band_mode(
	struct test_wlan_info *winfos,
	struct test_band_state *band_state);
s_int32 mt_op_get_chipid(
	struct test_wlan_info *winfos);
s_int32 mt_op_mps_start(
	struct test_wlan_info *winfos,
	u_char band_idx);
s_int32 mt_op_mps_set_nss(
	struct test_wlan_info *winfos,
	u_int32 len,
	struct test_mps_setting *mps_setting);
s_int32 mt_op_mps_set_per_packet_bw(
	struct test_wlan_info *winfos,
	u_int32 len,
	struct test_mps_setting *mps_setting);
s_int32 mt_op_mps_set_packet_count(
	struct test_wlan_info *winfos,
	u_int32 len,
	struct test_mps_setting *mps_setting);

s_int32 mt_op_mps_set_payload_length(
	struct test_wlan_info *winfos,
	u_int32 len,
	struct test_mps_setting *mps_setting);
s_int32 mt_op_mps_set_power_gain(
	struct test_wlan_info *winfos,
	u_int32 len,
	struct test_mps_setting *mps_setting);
s_int32 mt_op_mps_set_seq_data(
	struct test_wlan_info *winfos,
	u_int32 len,
	struct test_mps_setting *mps_setting);
s_int32 mt_op_set_muru_manual(
	void *virtual_device,
	struct test_wlan_info *winfos,
	struct test_configuration *configs);
s_int32 mt_op_set_tam_arb(
	struct test_wlan_info *winfos,
	u_int8 arb_op_mode);

/* gen4m operation define */
s_int32 mt_op_set_test_mode_start(struct test_wlan_info *winfos);
s_int32 mt_op_set_test_mode_abort(struct test_wlan_info *winfos);

/* For test mac usage */
s_int32 mt_op_backup_and_set_cr(
	struct test_wlan_info *winfos,
	struct test_bk_cr *bks,
	u_char band_idx);
s_int32 mt_op_restore_cr(
	struct test_wlan_info *winfos,
	struct test_bk_cr *bks,
	u_char band_idx);
s_int32 mt_op_set_ampdu_ba_limit(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_int8 agg_limit);
s_int32 mt_op_set_sta_pause_cr(
	struct test_wlan_info *winfos);
s_int32 mt_op_set_ifs_cr(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx);
s_int32 mt_op_write_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs);
s_int32 mt_op_read_bulk_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs);
s_int32 mt_op_read_bulk_rf_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs);
s_int32 mt_op_write_bulk_rf_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs);
s_int32 mt_op_read_bulk_eeprom(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms);
s_int32 mt_op_write_bulk_eeprom(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms);
s_int32 mt_op_get_free_efuse_block(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms);
/* For test phy usage */
s_int32 mt_op_get_tx_pwr(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx,
	u_char channel,
	u_char ant_idx,
	u_int32 *power);
s_int32 mt_op_get_tx_default_pwr(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx,
	u_char channel,
	u_char ant_idx,
	u_int32 *power);
s_int32 mt_op_set_get_pwr_type(
	struct test_wlan_info *winfos,
	u_int32_t powertype);
s_int32 mt_op_set_tx_pwr(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx,
	struct test_txpwr_param *pwr_param);
s_int32 mt_op_set_freq_offset(
	struct test_wlan_info *winfos,
	u_int32 freq_offset, u_char band_idx);
s_int32 mt_op_get_freq_offset(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 *freq_offset);
#if (CFG_SUPPORT_CONNAC3X == 1) || (CFG_SUPPORT_CONNAC5X == 1)
s_int32 mt_op_set_freq_offset_C2(
	struct test_wlan_info *winfos,
	u_int32 freq_offset, u_char band_idx);
s_int32 mt_op_get_freq_offset_C2(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 *freq_offset);
#endif
s_int32 mt_op_get_cfg_on_off(
	struct test_wlan_info *winfos,
	u_int32 type,
	u_int32 band_idx,
	u_int32 ch_band,
	u_int32 *result);
s_int32 mt_op_get_tx_tone_pwr(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 ant_idx,
	u_int32 *power);
s_int32 mt_op_get_recal_cnt(
	struct test_wlan_info *winfos,
	u_int32 *recal_cnt,
	u_int32 *recal_dw_num);
s_int32 mt_op_get_recal_content(
	struct test_wlan_info *winfos,
	u_int32 *content);
s_int32 mt_op_get_rxv_cnt(
	struct test_wlan_info *winfos,
	u_int32 *rxv_cnt,
	u_int32 *rxv_dw_num);
s_int32 mt_op_get_rxv_content(
	struct test_wlan_info *winfos,
	u_int32 dw_cnt,
	u_int32 *content);
s_int32 mt_op_set_cal_bypass(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int32 cal_item);
s_int32 mt_op_set_dpd(
	struct test_wlan_info *winfos,
	u_int32 on_off,
	u_int32 wf_sel);

#if (CFG_SUPPORT_CONNAC3X == 1) || (CFG_SUPPORT_CONNAC5X == 1)
s_int32 mt_op_set_max_pac_ext(
	struct test_wlan_info *winfos,
	u_int32 max_pac_ext);
#endif

s_int32 mt_op_set_tssi(
	struct test_wlan_info *winfos,
	u_int32 on_off,
	u_int32 wf_sel);
s_int32 mt_op_get_thermal_val(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx,
	u_int32 *value);
s_int32 mt_op_set_rdd_test(
	struct test_wlan_info *winfos,
	u_int32 rdd_idx,
	u_int32 rdd_sel,
	u_int32 enable);
s_int32 mt_op_set_off_ch_scan(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx,
	struct test_off_ch_param *off_ch_param);
s_int32 mt_op_get_rdd_cnt(
	struct test_wlan_info *winfos,
	u_int32 *rdd_cnt,
	u_int32 *rdd_dw_num);
s_int32 mt_op_get_rdd_content(
	struct test_wlan_info *winfos,
	u_int32 *content,
	u_int32 *total_cnt);
s_int32 mt_op_hetb_ctrl(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_char ctrl_type,
	u_char enable,
	u_int8 bw,
	u_int8 ltf_gi,
	u_int8 stbc,
	u_int8 pri_ru_idx,
	struct test_ru_info *ru_info);
s_int32 mt_op_set_ru_aid(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_int16 mu_rx_aid);
s_int32 mt_op_set_mutb_spe(
	struct test_wlan_info *winfos,
	u_char band_idx,
	u_char tx_mode,
	u_int8 spe_idx);
s_int32 mt_op_get_rx_stat_band(
	struct test_wlan_info *winfos,
	u_int8 band_idx,
	u_int8 blk_idx,
	struct test_rx_stat_band_info *rx_st_band);
s_int32 mt_op_get_rx_stat_path(
	struct test_wlan_info *winfos,
	u_int8 band_idx,
	u_int8 blk_idx,
	struct test_rx_stat_path_info *rx_st_path);
s_int32 mt_op_get_rx_stat_user(
	struct test_wlan_info *winfos,
	u_int8 band_idx,
	u_int8 blk_idx,
	struct test_rx_stat_user_info *rx_st_user);
s_int32 mt_op_get_rx_stat_comm(
	struct test_wlan_info *winfos,
	u_int8 band_idx,
	u_int8 blk_idx,
	struct test_rx_stat_comm_info *rx_st_comm);
s_int32 mt_op_get_wf_path_comb(
	struct test_wlan_info *winfos,
	u_int8 band_idx,
	boolean dbdc_mode_en,
	u_int8 *path,
	u_int8 *path_len);
s_int32 mt_op_listmode_cmd(
	struct test_wlan_info *winfos,
	u_int8 *para,
	u_int16 para_len,
	u_int32 *rsp_len,
	void *rsp_data);
s_int32 mt_op_set_efem_mode(
	struct test_wlan_info *winfos,
	u_int32 band_idx,
	u_int32 ch_band,
	u_int32 wf_path,
	u_int32 enable,
	u_int32 mode,
	u_int32 level);
s_int32 mt_op_set_tx_gain(
	struct test_wlan_info *winfos,
	u_int32 band_idx,
	u_int32 ch_band,
	u_int32 wf_path,
	u_int32 enable,
	u_int32 gain_type,
	u_int32 value);
s_int32 mt_op_set_etssi_gain(
	struct test_wlan_info *winfos,
	u_int32 band_idx,
	u_int32 ch_band,
	u_int32 wf_path,
	u_int32 enable,
	u_int32 gain_value);
s_int32 mt_op_get_tssi_meas_dbv(
	struct test_wlan_info *winfos,
	u_int32 band_idx,
	u_int32 wf_path,
	u_int32 *dbv_value);
s_int32 mt_op_get_sleep_check(
	struct test_wlan_info *winfos,
	u_int32 action,
	u_int32 *sleep_result);

#endif /* __OPERATION_H__ */
