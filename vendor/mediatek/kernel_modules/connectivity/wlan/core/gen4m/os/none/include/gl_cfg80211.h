/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   gl_cfg80211.h
 *    \brief  This file is for Portable Driver linux cfg80211 support.
 */


#ifndef _GL_CFG80211_H
#define _GL_CFG80211_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_os.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#ifdef CONFIG_NL80211_TESTMODE
#define NL80211_DRIVER_TESTMODE_VERSION 2
#endif

#define NL80211_SCAN_FLAG_LOW_SPAN (1 << 8)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

#ifdef CONFIG_NL80211_TESTMODE
#if CFG_SUPPORT_NFC_BEAM_PLUS

struct NL80211_DRIVER_SET_NFC_PARAMS {
	struct NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	uint32_t NFC_Enable;

};

#endif



struct NL80211_DRIVER_GET_STA_STATISTICS_PARAMS {
	struct NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	uint32_t u4Version;
	uint32_t u4Flag;
	uint8_t aucMacAddr[MAC_ADDR_LEN];
};

enum _ENUM_TESTMODE_LINK_DETECTION_ATTR {
	NL80211_TESTMODE_LINK_INVALID = 0,
	NL80211_TESTMODE_LINK_TX_FAIL_CNT,
	NL80211_TESTMODE_LINK_TX_RETRY_CNT,
	NL80211_TESTMODE_LINK_TX_MULTI_RETRY_CNT,
	NL80211_TESTMODE_LINK_ACK_FAIL_CNT,
	NL80211_TESTMODE_LINK_FCS_ERR_CNT,
	NL80211_TESTMODE_LINK_TX_CNT,
	NL80211_TESTMODE_LINK_RX_CNT,
	NL80211_TESTMODE_LINK_RST_REASON,
	NL80211_TESTMODE_LINK_RST_TIME,
	NL80211_TESTMODE_LINK_ROAM_FAIL_TIMES,
	NL80211_TESTMODE_LINK_ROAM_FAIL_TIME,
	NL80211_TESTMODE_LINK_TX_DONE_DELAY_IS_ARP,
	NL80211_TESTMODE_LINK_ARRIVE_DRV_TICK,
	NL80211_TESTMODE_LINK_ENQUE_TICK,
	NL80211_TESTMODE_LINK_DEQUE_TICK,
	NL80211_TESTMODE_LINK_LEAVE_DRV_TICK,
	NL80211_TESTMODE_LINK_CURR_TICK,
	NL80211_TESTMODE_LINK_CURR_TIME,

	NL80211_TESTMODE_LINK_DETECT_NUM
};

enum ENUM_TESTMODE_STA_STATISTICS_ATTR {
	NL80211_TESTMODE_STA_STATISTICS_INVALID = 0,
	NL80211_TESTMODE_STA_STATISTICS_VERSION,
	NL80211_TESTMODE_STA_STATISTICS_MAC,
	NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE,
	NL80211_TESTMODE_STA_STATISTICS_FLAG,

	NL80211_TESTMODE_STA_STATISTICS_PER,
	NL80211_TESTMODE_STA_STATISTICS_RSSI,
	NL80211_TESTMODE_STA_STATISTICS_PHY_MODE,
	NL80211_TESTMODE_STA_STATISTICS_TX_RATE,

	NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT,
	NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT,

	NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME,
	NL80211_TESTMODE_STA_STATISTICS_MAX_PROCESS_TIME,
	NL80211_TESTMODE_STA_STATISTICS_AVG_HIF_PROCESS_TIME,
	NL80211_TESTMODE_STA_STATISTICS_MAX_HIF_PROCESS_TIME,

	NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT,
	NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT,
	NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME,

	NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
	NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,

	NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
	NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,

	/*
	 * how many packages TX during statistics interval
	 */
	NL80211_TESTMODE_STA_STATISTICS_ENQUEUE,

	/*
	 * how many packages this TX during statistics interval
	 */
	NL80211_TESTMODE_STA_STATISTICS_STA_ENQUEUE,

	/*
	 * how many packages dequeue during statistics interval
	 */
	NL80211_TESTMODE_STA_STATISTICS_DEQUEUE,

	/*
	 * how many packages this sta dequeue during statistics interval
	 */
	NL80211_TESTMODE_STA_STATISTICS_STA_DEQUEUE,

	/*
	 * how many TC[0-3] resource back from firmware during
	 * statistics interval
	 */
	NL80211_TESTMODE_STA_STATISTICS_RB_ARRAY,
	NL80211_TESTMODE_STA_STATISTICS_NO_TC_ARRAY,
	NL80211_TESTMODE_STA_STATISTICS_USED_TC_PGCT_ARRAY,
	NL80211_TESTMODE_STA_STATISTICS_WANTED_TC_PGCT_ARRAY,

	NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_CNT,
	NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_PASS_CNT,
	NL80211_TESTMODE_STA_STATISTICS_IRQ_TASK_CNT,
	NL80211_TESTMODE_STA_STATISTICS_IRQ_AB_CNT,
	NL80211_TESTMODE_STA_STATISTICS_IRQ_SW_CNT,
	NL80211_TESTMODE_STA_STATISTICS_IRQ_TX_CNT,
	NL80211_TESTMODE_STA_STATISTICS_IRQ_RX_CNT,

	NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,

	NL80211_TESTMODE_STA_STATISTICS_NUM
};
#endif
/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/* cfg80211 hooks */
#if 0
int
mtk_cfg80211_change_iface(struct wiphy *wiphy,
			  struct net_device *ndev, enum nl80211_iftype type,
			  u32 *flags, struct vif_params *params);

int
mtk_cfg80211_add_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index, bool pairwise, const u8 *mac_addr,
		     struct key_params *params);

int
mtk_cfg80211_get_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index,
		     bool pairwise,
		     const u8 *mac_addr, void *cookie,
		     void (*callback)(void *cookie, struct key_params *));

int
mtk_cfg80211_del_key(struct wiphy *wiphy, struct net_device *ndev, u8 key_index,
					bool pairwise, const u8 *mac_addr);

int
mtk_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *ndev,
				u8 key_index, bool unicast, bool multicast);

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_get_station(struct wiphy *wiphy,
			     struct net_device *ndev, const u8 *mac,
			     struct station_info *sinfo);
#else
int mtk_cfg80211_get_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac,
			     struct station_info *sinfo);
#endif

int mtk_cfg80211_scan(struct wiphy *wiphy,
		      struct cfg80211_scan_request *request);

void mtk_cfg80211_abort_scan(struct wiphy *wiphy,
			     struct wireless_dev *wdev);

int mtk_cfg80211_connect(struct wiphy *wiphy,
			 struct net_device *ndev,
			 struct cfg80211_connect_params *sme);

int mtk_cfg80211_disconnect(struct wiphy *wiphy,
			    struct net_device *ndev, u16 reason_code);

int mtk_cfg80211_join_ibss(struct wiphy *wiphy,
			   struct net_device *ndev,
			   struct cfg80211_ibss_params *params);

int mtk_cfg80211_leave_ibss(struct wiphy *wiphy,
			    struct net_device *ndev);

int mtk_cfg80211_set_power_mgmt(struct wiphy *wiphy,
			struct net_device *ndev, bool enabled, int timeout);

int mtk_cfg80211_set_pmksa(struct wiphy *wiphy,
			struct net_device *ndev, struct cfg80211_pmksa *pmksa);

int mtk_cfg80211_del_pmksa(struct wiphy *wiphy,
			struct net_device *ndev, struct cfg80211_pmksa *pmksa);

int mtk_cfg80211_flush_pmksa(struct wiphy *wiphy,
			     struct net_device *ndev);

int mtk_cfg80211_set_rekey_data(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_gtk_rekey_data *data);

int mtk_cfg80211_remain_on_channel(struct wiphy *wiphy,
		struct wireless_dev *wdev, struct ieee80211_channel *chan,
		unsigned int duration, u64 *cookie);

int mtk_cfg80211_cancel_remain_on_channel(
	struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie);
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 struct cfg80211_mgmt_tx_params *params,
			 u64 *cookie);
#else
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 struct ieee80211_channel *channel, bool offscan,
			 unsigned int wait,
			 const u8 *buf, size_t len, bool no_cck,
			 bool dont_wait_for_ack, u64 *cookie);
#endif

void mtk_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
		struct wireless_dev *wdev, u16 frame_type, bool reg);

int mtk_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				     struct wireless_dev *wdev, u64 cookie);

#ifdef CONFIG_NL80211_TESTMODE
int
mtk_cfg80211_testmode_get_sta_statistics(struct wiphy
		*wiphy,
		void *data, int len, struct GLUE_INFO *prGlueInfo);

int mtk_cfg80211_testmode_get_scan_done(struct wiphy
					*wiphy, void *data, int len,
					struct GLUE_INFO *prGlueInfo);

#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      void *data, int len);
#else
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy,
			      void *data, int len);
#endif

int mtk_cfg80211_testmode_sw_cmd(struct wiphy *wiphy,
				 void *data, int len);

#if CFG_SUPPORT_PASSPOINT
int mtk_cfg80211_testmode_hs20_cmd(struct wiphy *wiphy,
				   void *data, int len);
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_WAPI
int mtk_cfg80211_testmode_set_key_ext(struct wiphy
				      *wiphy, void *data, int len);
#endif
#if CFG_SUPPORT_NFC_BEAM_PLUS
int mtk_cfg80211_testmode_get_scan_done(struct wiphy *wiphy,
		void *data, int len, struct GLUE_INFO *prGlueInfo);
#endif
#else
/* IGNORE KERNEL DEPENCY ERRORS */
/* #error "Please ENABLE kernel config (CONFIG_NL80211_TESTMODE) to support
 * Wi-Fi Direct"
 */
#endif

#if CFG_SUPPORT_SCHED_SCAN
int
mtk_cfg80211_sched_scan_start(struct wiphy *wiphy,
			      struct net_device *ndev,
			      struct cfg80211_sched_scan_request *request);

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_sched_scan_stop(struct wiphy *wiphy,
				 struct net_device *ndev,
				 u64 reqid);
#else
int mtk_cfg80211_sched_scan_stop(struct wiphy *wiphy,
				 struct net_device *ndev);
#endif
#endif /* CFG_SUPPORT_SCHED_SCAN */

int mtk_cfg80211_assoc(struct wiphy *wiphy,
		       struct net_device *ndev,
		       struct cfg80211_assoc_request *req);

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_change_station(struct wiphy *wiphy,
			    struct net_device *ndev,
			    const u8 *mac, struct station_parameters *params);

int mtk_cfg80211_add_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     const u8 *mac, struct station_parameters *params);

#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     struct station_del_parameters *params);
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     const u8 *mac);
#endif
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
			   struct net_device *dev,
			   const u8 *peer, u8 action_code, u8 dialog_token,
			   u16 status_code, u32 peer_capability,
			   bool initiator, const u8 *buf, size_t len);
#else
int mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
			   struct net_device *dev,
			   const u8 *peer, u8 action_code, u8 dialog_token,
			   u16 status_code, u32 peer_capability,
			   const u8 *buf, size_t len);
#endif

int mtk_cfg80211_tdls_oper(struct wiphy *wiphy,
			   struct net_device *dev,
			   const u8 *peer, enum nl80211_tdls_operation oper);
#else
int
mtk_cfg80211_change_station(struct wiphy *wiphy,
			    struct net_device *ndev, u8 *mac,
			    struct station_parameters *params);

int mtk_cfg80211_add_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac,
			     struct station_parameters *params);

int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac);

int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       u8 *peer, u8 action_code,
		       u8 dialog_token, u16 status_code,
		       const u8 *buf, size_t len);

int mtk_cfg80211_tdls_oper(struct wiphy *wiphy,
			   struct net_device *dev, u8 *peer,
			   enum nl80211_tdls_operation oper);
#endif

int32_t mtk_cfg80211_process_str_cmd(struct GLUE_INFO
				     *prGlueInfo, uint8_t *cmd, int32_t len);

void mtk_reg_notify(struct wiphy *pWiphy,
		    struct regulatory_request *pRequest);
void cfg80211_regd_set_wiphy(struct wiphy *pWiphy);

int mtk_cfg80211_suspend(struct wiphy *wiphy,
			 struct cfg80211_wowlan *wow);

int mtk_cfg80211_resume(struct wiphy *wiphy);

/* cfg80211 wrapper hooks */
#if CFG_ENABLE_UNIFY_WIPHY
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       unsigned char name_assign_type,
				       enum nl80211_iftype type,
				       struct vif_params *params);
#elif KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       unsigned char name_assign_type,
				       enum nl80211_iftype type,
				       u32 *flags,
				       struct vif_params *params);
#else
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       enum nl80211_iftype type, u32 *flags,
				       struct vif_params *params);
#endif
int mtk_cfg_del_iface(struct wiphy *wiphy,
		      struct wireless_dev *wdev);
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_change_iface(struct wiphy *wiphy,
			 struct net_device *ndev,
			 enum nl80211_iftype type,
			 struct vif_params *params);
#else
int mtk_cfg_change_iface(struct wiphy *wiphy,
			 struct net_device *ndev,
			 enum nl80211_iftype type, u32 *flags,
			 struct vif_params *params);
#endif

#if (KERNEL_VERSION(6, 0, 0) <= CFG80211_VERSION_CODE) && \
	(CFG_SUPPORT_802_11BE_MLO == 1)
int mtk_cfg_add_intf_link(struct wiphy *wiphy,
	struct wireless_dev *wdev, unsigned int link_id);

void mtk_cfg_del_intf_link(struct wiphy *wiphy,
	struct wireless_dev *wdev, unsigned int link_id);
#endif

int mtk_cfg_add_key(struct wiphy *wiphy,
		    struct net_device *ndev, u8 key_index,
		    bool pairwise, const u8 *mac_addr,
		    struct key_params *params);
int mtk_cfg_get_key(struct wiphy *wiphy,
		    struct net_device *ndev, u8 key_index,
		    bool pairwise, const u8 *mac_addr, void *cookie,
		    void (*callback)(void *cookie, struct key_params *));
int mtk_cfg_del_key(struct wiphy *wiphy,
		    struct net_device *ndev, u8 key_index,
		    bool pairwise, const u8 *mac_addr);
int mtk_cfg_set_default_key(struct wiphy *wiphy,
			    struct net_device *ndev,
			    u8 key_index, bool unicast, bool multicast);

int mtk_cfg_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *ndev, u8 key_index);

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_get_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac, struct station_info *sinfo);
#else
int mtk_cfg_get_station(struct wiphy *wiphy,
			struct net_device *ndev,
			u8 *mac, struct station_info *sinfo);
#endif

#if CFG_SUPPORT_TDLS
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_change_station(struct wiphy *wiphy,
			   struct net_device *ndev,
			   const u8 *mac, struct station_parameters *params);
#else
int mtk_cfg_change_station(struct wiphy *wiphy,
			   struct net_device *ndev,
			   u8 *mac, struct station_parameters *params);
#endif
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_add_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac, struct station_parameters *params);
#else
int mtk_cfg_add_station(struct wiphy *wiphy,
			struct net_device *ndev,
			u8 *mac, struct station_parameters *params);
#endif
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_oper(struct wiphy *wiphy,
		      struct net_device *ndev,
		      const u8 *peer, enum nl80211_tdls_operation oper);
#else
int mtk_cfg_tdls_oper(struct wiphy *wiphy,
		      struct net_device *ndev,
		      u8 *peer, enum nl80211_tdls_operation oper);
#endif
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *ndev,
		      const u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code,
		      u32 peer_capability, bool initiator, const u8 *buf,
		      size_t len);
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *ndev,
		      const u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code,
		      u32 peer_capability, const u8 *buf, size_t len);
#else
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *ndev,
		      u8 *peer, u8 action_code,
		      u8 dialog_token, u16 status_code,
		      const u8 *buf, size_t len);
#endif
#endif /* CFG_SUPPORT_TDLS */

#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev,
			struct station_del_parameters *params);
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac);
#else
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev, u8 *mac);
#endif
int mtk_cfg_scan(struct wiphy *wiphy,
		 struct cfg80211_scan_request *request);
#if KERNEL_VERSION(4, 5, 0) <= CFG80211_VERSION_CODE
void mtk_cfg_abort_scan(struct wiphy *wiphy,
			struct wireless_dev *wdev);
#endif

#if CFG_SUPPORT_SCHED_SCAN
int mtk_cfg_sched_scan_start(struct wiphy *wiphy,
			     struct net_device *ndev,
			     struct cfg80211_sched_scan_request *request);

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_sched_scan_stop(struct wiphy *wiphy,
			    struct net_device *ndev,
			    u64 reqid);
#else
int mtk_cfg_sched_scan_stop(struct wiphy *wiphy,
			    struct net_device *ndev);
#endif

#endif /* CFG_SUPPORT_SCHED_SCAN */

int mtk_cfg_connect(struct wiphy *wiphy,
		    struct net_device *ndev,
		    struct cfg80211_connect_params *sme);
int mtk_cfg_disconnect(struct wiphy *wiphy,
		       struct net_device *ndev,
		       u16 reason_code);
int mtk_cfg_join_ibss(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_ibss_params *params);
int mtk_cfg_leave_ibss(struct wiphy *wiphy,
		       struct net_device *ndev);
int mtk_cfg_set_power_mgmt(struct wiphy *wiphy,
			   struct net_device *ndev,
			   bool enabled, int timeout);
int mtk_cfg_set_pmksa(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_pmksa *pmksa);
int mtk_cfg_del_pmksa(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_pmksa *pmksa);
int mtk_cfg_flush_pmksa(struct wiphy *wiphy,
			struct net_device *ndev);
#if CONFIG_SUPPORT_GTK_REKEY
int mtk_cfg_set_rekey_data(struct wiphy *wiphy,
			   struct net_device *dev,
			   struct cfg80211_gtk_rekey_data *data);
#endif /* CONFIG_SUPPORT_GTK_REKEY */
int mtk_cfg_suspend(struct wiphy *wiphy,
		    struct cfg80211_wowlan *wow);
int mtk_cfg_resume(struct wiphy *wiphy);
int mtk_cfg_assoc(struct wiphy *wiphy,
		  struct net_device *ndev,
		  struct cfg80211_assoc_request *req);
int mtk_cfg_remain_on_channel(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      struct ieee80211_channel *chan,
			      unsigned int duration,
			      u64 *cookie);
int mtk_cfg_cancel_remain_on_channel(struct wiphy *wiphy,
				     struct wireless_dev *wdev, u64 cookie);
uint16_t cfg80211_get_non_wfa_vendor_ie(struct GLUE_INFO
		*prGlueInfo, uint8_t *ies, int32_t len);

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_mgmt_tx(struct wiphy *wiphy,
		    struct wireless_dev *wdev,
		    struct cfg80211_mgmt_tx_params *params, u64 *cookie);
#else
int mtk_cfg_mgmt_tx(struct wiphy *wiphy,
		    struct wireless_dev *wdev,
		    struct ieee80211_channel *channel, bool offscan,
		    unsigned int wait,
		    const u8 *buf, size_t len, bool no_cck,
		    bool dont_wait_for_ack,
		    u64 *cookie);
#endif
void mtk_cfg_mgmt_frame_register(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 u16 frame_type, bool reg);

#ifdef CONFIG_NL80211_TESTMODE
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_testmode_cmd(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 void *data, int len);
#else
int mtk_cfg_testmode_cmd(struct wiphy *wiphy, void *data,
			 int len);
#endif
#endif	/* CONFIG_NL80211_TESTMODE */

#if (CFG_SUPPORT_DFS_MASTER == 1)
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_start_radar_detection(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_chan_def *chandef,
				  unsigned int cac_time_ms);
#else
int mtk_cfg_start_radar_detection(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_chan_def *chandef);
#endif


#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_channel_switch(struct wiphy *wiphy,
			   struct net_device *dev,
			   struct cfg80211_csa_settings *params);
#endif
#endif


#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0)
int mtk_cfg_change_bss(struct wiphy *wiphy,
		       struct net_device *dev,
		       struct bss_parameters *params);
int mtk_cfg_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				u64 cookie);
int mtk_cfg_deauth(struct wiphy *wiphy,
		   struct net_device *dev,
		   struct cfg80211_deauth_request *req);
int mtk_cfg_disassoc(struct wiphy *wiphy,
		     struct net_device *dev,
		     struct cfg80211_disassoc_request *req);
int mtk_cfg_start_ap(struct wiphy *wiphy,
		     struct net_device *dev,
		     struct cfg80211_ap_settings *settings);
int mtk_cfg_change_beacon(struct wiphy *wiphy,
			  struct net_device *dev,
			  struct cfg80211_beacon_data *info);
int mtk_cfg_stop_ap(struct wiphy *wiphy,
		    struct net_device *dev);
int mtk_cfg_set_wiphy_params(struct wiphy *wiphy,
			     u32 changed);
int mtk_cfg_set_bitrate_mask(struct wiphy *wiphy,
			     struct net_device *dev,
			     const u8 *peer,
			     const struct cfg80211_bitrate_mask *mask);
int mtk_cfg_set_txpower(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			enum nl80211_tx_power_setting type, int mbm);
int mtk_cfg_get_txpower(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			int *dbm);
#endif /* (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0) */

#endif	/* CFG_ENABLE_UNIFY_WIPHY */

int mtk_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *dev,
				struct cfg80211_update_ft_ies_params *ftie);
#endif

#ifdef CFG_REMIND_IMPLEMENT
#define mtk_cfg80211_find_ie_match_mask(_eid, _ies, _len, _match,\
	_match_len, _match_offset, _match_mask) \
	((uint8_t *) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__))
#else
const uint8_t *mtk_cfg80211_find_ie_match_mask(uint8_t eid,
				const uint8_t *ies, int len,
				const uint8_t *match,
				int match_len, int match_offset,
				const uint8_t *match_mask);
#endif

#define kalCfgDataWrite8(_prGlueInfo, _u4Offset, _u1Data) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _GL_CFG80211_H */
