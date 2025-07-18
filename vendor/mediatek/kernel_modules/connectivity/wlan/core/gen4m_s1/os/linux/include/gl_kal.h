/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 ** Id: /os/linux/include/gl_kal.h
 */

/*! \file   gl_kal.h
 *  \brief  Declaration of KAL functions - kal*() which is provided
 *          by GLUE Layer.
 *
 *    Any definitions in this file will be shared among GLUE Layer
 *    and internal Driver Stack.
 */

#ifndef _GL_KAL_H
#define _GL_KAL_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "config.h"
#include "gl_typedef.h"
#include "gl_wext_priv.h"
#include "link.h"
#include "nic/mac.h"
#include "nic/wlan_def.h"
#include "wlan_lib.h"
#include "wlan_oid.h"

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
#include "conn_power_throttling.h"
#endif

#if CFG_ENABLE_BT_OVER_WIFI
#include "nic/bow.h"
#endif

#include "linux/kallsyms.h"
#include "linux/sched.h"
#if KERNEL_VERSION(4, 11, 0) <= CFG80211_VERSION_CODE
#include "linux/sched/types.h"
#endif

#if CFG_SUPPORT_SCAN_CACHE_RESULT
#include "wireless/core.h"
#endif
/* for sched_clock() */
#include <linux/sched/clock.h>

#if DBG
extern int allocatedMemSize;
#endif

extern struct semaphore g_halt_sem;
extern int g_u4HaltFlag;
extern int g_u4WlanInitFlag;

extern struct delayed_work sched_workq;
#if CFG_MODIFY_TX_POWER_BY_BAT_VOLT
extern unsigned int wlan_bat_volt;
extern bool fgIsTxPowerDecreased;
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* Define how many concurrent operation networks. */
#define KAL_BSS_NUM             4

#if CFG_SUPPORT_DUAL_STA
#define KAL_AIS_NUM           2
#else
#define KAL_AIS_NUM           1
#endif

#if CFG_DUAL_P2PLIKE_INTERFACE
#define KAL_P2P_NUM             2
#else
#define KAL_P2P_NUM             1
#endif

#define OID_HDLR_REC_NUM	5

#if CFG_SUPPORT_MULTITHREAD
#define GLUE_FLAG_MAIN_PROCESS \
	(GLUE_FLAG_HALT | GLUE_FLAG_SUB_MOD_MULTICAST | \
	GLUE_FLAG_TX_CMD_DONE | GLUE_FLAG_TXREQ | GLUE_FLAG_TIMEOUT | \
	GLUE_FLAG_FRAME_FILTER | GLUE_FLAG_OID | GLUE_FLAG_RX | \
	GLUE_FLAG_SER_TIMEOUT)

#define GLUE_FLAG_HIF_PROCESS \
	(GLUE_FLAG_HALT | GLUE_FLAG_INT | GLUE_FLAG_HIF_TX | \
	GLUE_FLAG_HIF_TX_CMD | GLUE_FLAG_HIF_FW_OWN | \
	GLUE_FLAG_HIF_PRT_HIF_DBG_INFO | \
	GLUE_FLAG_UPDATE_WMM_QUOTA | \
	GLUE_FLAG_NOTIFY_MD_CRASH | \
	GLUE_FLAG_DRV_INT)

#define GLUE_FLAG_RX_PROCESS (GLUE_FLAG_HALT | GLUE_FLAG_RX_TO_OS)
#else
/* All flags for single thread driver */
#define GLUE_FLAG_TX_PROCESS  0xFFFFFFFF
#endif

#if CFG_SUPPORT_SNIFFER
#define RADIOTAP_FIELD_TSFT			BIT(0)
#define RADIOTAP_FIELD_FLAGS		BIT(1)
#define RADIOTAP_FIELD_RATE			BIT(2)
#define RADIOTAP_FIELD_CHANNEL		BIT(3)
#define RADIOTAP_FIELD_ANT_SIGNAL	BIT(5)
#define RADIOTAP_FIELD_ANT_NOISE	BIT(6)
#define RADIOTAP_FIELD_ANT			BIT(11)
#define RADIOTAP_FIELD_MCS			BIT(19)
#define RADIOTAP_FIELD_AMPDU		BIT(20)
#define RADIOTAP_FIELD_VHT			BIT(21)
#define RADIOTAP_FIELD_VENDOR       BIT(30)

#define RADIOTAP_LEN_VHT			48
#define RADIOTAP_FIELDS_VHT (RADIOTAP_FIELD_TSFT | \
				    RADIOTAP_FIELD_FLAGS | \
				    RADIOTAP_FIELD_RATE | \
				    RADIOTAP_FIELD_CHANNEL | \
				    RADIOTAP_FIELD_ANT_SIGNAL | \
				    RADIOTAP_FIELD_ANT_NOISE | \
				    RADIOTAP_FIELD_ANT | \
				    RADIOTAP_FIELD_AMPDU | \
				    RADIOTAP_FIELD_VHT | \
				    RADIOTAP_FIELD_VENDOR)

#define RADIOTAP_LEN_HT				36
#define RADIOTAP_FIELDS_HT (RADIOTAP_FIELD_TSFT | \
				    RADIOTAP_FIELD_FLAGS | \
				    RADIOTAP_FIELD_RATE | \
				    RADIOTAP_FIELD_CHANNEL | \
				    RADIOTAP_FIELD_ANT_SIGNAL | \
				    RADIOTAP_FIELD_ANT_NOISE | \
				    RADIOTAP_FIELD_ANT | \
				    RADIOTAP_FIELD_MCS | \
				    RADIOTAP_FIELD_AMPDU | \
				    RADIOTAP_FIELD_VENDOR)

#define RADIOTAP_LEN_LEGACY			26
#define RADIOTAP_FIELDS_LEGACY (RADIOTAP_FIELD_TSFT | \
				    RADIOTAP_FIELD_FLAGS | \
				    RADIOTAP_FIELD_RATE | \
				    RADIOTAP_FIELD_CHANNEL | \
				    RADIOTAP_FIELD_ANT_SIGNAL | \
				    RADIOTAP_FIELD_ANT_NOISE | \
				    RADIOTAP_FIELD_ANT | \
				    RADIOTAP_FIELD_VENDOR)
#endif

#define PERF_MON_INIT_BIT       (0)
#define PERF_MON_DISABLE_BIT    (1)
#define PERF_MON_STOP_BIT       (2)
#define PERF_MON_RUNNING_BIT    (3)

#define PERF_MON_UPDATE_INTERVAL (1000)
#define PERF_MON_TP_MAX_THRESHOLD (10)

#define PERF_MON_TP_CONDITION (125000)
#define PERF_MON_COEX_TP_THRESHOLD (100)

/* By wifi.cfg first. If it is not set 1s by default; 100ms on more. */
#define TX_LATENCY_STATS_UPDATE_INTERVAL (0)
#define TX_LATENCY_STATS_CONTINUOUS_FAIL_THREHOLD (10)

#define TX_LATENCY_STATS_MAX_DRIVER_DELAY_L1 (1)
#define TX_LATENCY_STATS_MAX_DRIVER_DELAY_L2 (5)
#define TX_LATENCY_STATS_MAX_DRIVER_DELAY_L3 (10)
#define TX_LATENCY_STATS_MAX_DRIVER_DELAY_L4 (20)

#define TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L1 (10)
#define TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L2 (20)
#define TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L3 (50)
#define TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L4 (80)

#define TX_LATENCY_STATS_MAX_MAC_DELAY_L1 (5)
#define TX_LATENCY_STATS_MAX_MAC_DELAY_L2 (10)
#define TX_LATENCY_STATS_MAX_MAC_DELAY_L3 (20)
#define TX_LATENCY_STATS_MAX_MAC_DELAY_L4 (50)

#define TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L1 (10)
#define TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L2 (20)
#define TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L3 (50)
#define TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L4 (80)


#if CFG_SUPPORT_DATA_STALL
#define REPORT_EVENT_INTERVAL		30
#define EVENT_PER_HIGH_THRESHOLD	80
#define EVENT_TX_LOW_RATE_THRESHOLD	20
#define EVENT_RX_LOW_RATE_THRESHOLD	20
#define TRAFFIC_RHRESHOLD	150
#endif

#if CFG_SUPPORT_SA_LOG
#define WIFI_LOG_MSG_MAX	(512)
#define WIFI_LOG_MSG_BUFFER	(WIFI_LOG_MSG_MAX * 2)
#endif

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
#define PWR_LEVEL_STAT_UPDATE_INTERVAL	60	/* sec */
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_SPIN_LOCK_CATEGORY_E {
	SPIN_LOCK_FSM = 0,

#if CFG_SUPPORT_MULTITHREAD
	SPIN_LOCK_TX_PORT_QUE,
	SPIN_LOCK_TX_CMD_QUE,
	SPIN_LOCK_TX_CMD_DONE_QUE,
	SPIN_LOCK_TC_RESOURCE,
	SPIN_LOCK_RX_TO_OS_QUE,
#endif

	/* FIX ME */
	SPIN_LOCK_RX_QUE,
	SPIN_LOCK_RX_FREE_QUE,
	SPIN_LOCK_TX_QUE,
#if CFG_SUPPORT_TPENHANCE_MODE
	SPIN_LOCK_TXACK_QUE,
#endif /* CFG_SUPPORT_TPENHANCE_MODE */
	SPIN_LOCK_CMD_QUE,
	SPIN_LOCK_TX_RESOURCE,
	SPIN_LOCK_CMD_RESOURCE,
	SPIN_LOCK_QM_TX_QUEUE,
	SPIN_LOCK_CMD_PENDING,
	SPIN_LOCK_CMD_SEQ_NUM,
	SPIN_LOCK_TX_MSDU_INFO_LIST,
	SPIN_LOCK_TXING_MGMT_LIST,
	SPIN_LOCK_TX_SEQ_NUM,
	SPIN_LOCK_TX_COUNT,
	SPIN_LOCK_TXS_COUNT,
	/* end    */
	SPIN_LOCK_TX,
	/* TX/RX Direct : BEGIN */
	SPIN_LOCK_TX_DIRECT,
	SPIN_LOCK_RX_DIRECT,
	SPIN_LOCK_RX_DIRECT_REORDER,
	/* TX/RX Direct : END */
	SPIN_LOCK_IO_REQ,
	SPIN_LOCK_INT,
	SPIN_LOCK_UPDATE_WMM_QUOTA,

	SPIN_LOCK_MGT_BUF,
	SPIN_LOCK_MSG_BUF,
	SPIN_LOCK_STA_REC,

	SPIN_LOCK_MAILBOX,
	SPIN_LOCK_TIMER,

	SPIN_LOCK_BOW_TABLE,

	SPIN_LOCK_EHPI_BUS,	/* only for EHPI */
	SPIN_LOCK_NET_DEV,

	SPIN_LOCK_BSSLIST_FW,
	SPIN_LOCK_BSSLIST_CFG,
#if CFG_SUPPORT_NAN
	SPIN_LOCK_NAN_NEGO_CRB,
	SPIN_LOCK_NAN_NDL_FLOW_CTRL,
#endif
	SPIN_LOCK_NUM
};

enum ENUM_MUTEX_CATEGORY_E {
	MUTEX_TX_CMD_CLEAR,
	MUTEX_TX_DATA_DONE_QUE,
	MUTEX_DEL_INF,
	MUTEX_CHIP_RST,
	MUTEX_SET_OWN,
	MUTEX_BOOST_CPU,
	MUTEX_NUM
};

/* event for assoc information update */
struct EVENT_ASSOC_INFO {
	uint8_t ucAssocReq;	/* 1 for assoc req, 0 for assoc rsp */
	uint8_t ucReassoc;	/* 0 for assoc, 1 for reassoc */
	uint16_t u2Length;
	uint8_t *pucIe;
};

enum ENUM_KAL_NETWORK_TYPE_INDEX {
	KAL_NETWORK_TYPE_AIS_INDEX = 0,
#if CFG_ENABLE_WIFI_DIRECT
	KAL_NETWORK_TYPE_P2P_INDEX,
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	KAL_NETWORK_TYPE_BOW_INDEX,
#endif
	KAL_NETWORK_TYPE_INDEX_NUM
};

enum ENUM_KAL_MEM_ALLOCATION_TYPE_E {
	PHY_MEM_TYPE,		/* physically continuous */
	VIR_MEM_TYPE,		/* virtually continuous */
	MEM_TYPE_NUM
};

#ifdef CONFIG_ANDROID		/* Defined in Android kernel source */
#if (KERNEL_VERSION(4, 19, 0) <= CFG80211_VERSION_CODE)
#define KAL_WAKE_LOCK_T struct wakeup_source
#elif (KERNEL_VERSION(4, 9, 0) <= CFG80211_VERSION_CODE)
#define KAL_WAKE_LOCK_T struct wakeup_source
#else
#define KAL_WAKE_LOCK_T struct wake_lock
#endif
#else
#define KAL_WAKE_LOCK_T uint32_t
#endif

#if CFG_SUPPORT_AGPS_ASSIST
enum ENUM_MTK_AGPS_ATTR {
	MTK_ATTR_AGPS_INVALID,
	MTK_ATTR_AGPS_CMD,
	MTK_ATTR_AGPS_DATA,
	MTK_ATTR_AGPS_IFINDEX,
	MTK_ATTR_AGPS_IFNAME,
	MTK_ATTR_AGPS_MAX
};

enum ENUM_AGPS_EVENT {
	AGPS_EVENT_WLAN_ON,
	AGPS_EVENT_WLAN_OFF,
	AGPS_EVENT_WLAN_AP_LIST,
};
u_int8_t kalIndicateAgpsNotify(struct ADAPTER *prAdapter,
			       uint8_t cmd,
			       uint8_t *data, uint16_t dataLen);
#endif /* CFG_SUPPORT_AGPS_ASSIST */

#if CFG_SUPPORT_SNIFFER
/* Vendor Namespace
 * Bit Number 30
 * Required Alignment 2 bytes
 */
struct RADIOTAP_FIELD_VENDOR_ {
	uint8_t aucOUI[3];
	uint8_t ucSubNamespace;
	uint16_t u2DataLen;
	uint8_t ucData;
} __KAL_ATTRIB_PACKED__;

struct MONITOR_RADIOTAP {
	/* radiotap header */
	uint8_t ucItVersion;	/* set to 0 */
	uint8_t ucItPad;
	uint16_t u2ItLen;	/* entire length */
	uint32_t u4ItPresent;	/* fields present */

	/* TSFT
	 * Bit Number 0
	 * Required Alignment 8 bytes
	 * Unit microseconds
	 */
	uint64_t u8MacTime;

	/* Flags
	 * Bit Number 1
	 */
	uint8_t ucFlags;

	/* Rate
	 * Bit Number 2
	 * Unit 500 Kbps
	 */
	uint8_t ucRate;

	/* Channel
	 * Bit Number 3
	 * Required Alignment 2 bytes
	 */
	uint16_t u2ChFrequency;
	uint16_t u2ChFlags;

	/* Antenna signal
	 * Bit Number 5
	 * Unit dBm
	 */
	uint8_t ucAntennaSignal;

	/* Antenna noise
	 * Bit Number 6
	 * Unit dBm
	 */
	uint8_t ucAntennaNoise;

	/* Antenna
	 * Bit Number 11
	 * Unit antenna index
	 */
	uint8_t ucAntenna;

	/* MCS
	 * Bit Number 19
	 * Required Alignment 1 byte
	 */
	uint8_t ucMcsKnown;
	uint8_t ucMcsFlags;
	uint8_t ucMcsMcs;

	/* A-MPDU status
	 * Bit Number 20
	 * Required Alignment 4 bytes
	 */
	uint32_t u4AmpduRefNum;
	uint16_t u2AmpduFlags;
	uint8_t ucAmpduDelimiterCRC;
	uint8_t ucAmpduReserved;

	/* VHT
	 * Bit Number 21
	 * Required Alignment 2 bytes
	 */
	uint16_t u2VhtKnown;
	uint8_t ucVhtFlags;
	uint8_t ucVhtBandwidth;
	uint8_t aucVhtMcsNss[4];
	uint8_t ucVhtCoding;
	uint8_t ucVhtGroupId;
	uint16_t u2VhtPartialAid;

	/* extension space */
	uint8_t aucReserve[12];
} __KAL_ATTRIB_PACKED__;
#endif

struct KAL_HALT_CTRL_T {
	struct semaphore lock;
	struct task_struct *owner;
	u_int8_t fgHalt;
	u_int8_t fgHeldByKalIoctl;
	OS_SYSTIME u4HoldStart;
};

struct KAL_THREAD_SCHEDSTATS {
	/* when marked: the profiling start time(ms),
	 * when unmarked: total duration(ms)
	 */
	unsigned long long time;
	/* time spent in exec (sum_exec_runtime) */
	unsigned long long exec;
	/* time spent in run-queue while not being scheduled (wait_sum) */
	unsigned long long runnable;
	/* time spent waiting for I/O (iowait_sum) */
	unsigned long long iowait;
};

#if CFG_SUPPORT_DATA_STALL
enum ENUM_VENDOR_DRIVER_EVENT {
	EVENT_TEST_MODE,
	EVENT_ARP_NO_RESPONSE,
	EVENT_PER_HIGH,
	EVENT_TX_LOW_RATE,
	EVENT_RX_LOW_RATE,
	EVENT_SG_DISABLE,
	EVENT_SG_1T1R,
	EVENT_SG_2T2R,
	EVENT_TX_DUP_OFF = 100,
	EVENT_TX_DUP_ON = 101,
	EVENT_TX_DUP_CERT_CHANGE = 102
};
#endif

enum ENUM_CMD_TX_RESULT {
	CMD_TX_RESULT_SUCCESS,
	CMD_TX_RESULT_FAILED,
	CMD_TX_RESULT_QUEUED,
	CMD_TX_RESULT_NUM
};

typedef int(*PFN_PWR_LEVEL_HANDLER)(struct ADAPTER *, uint8_t);

struct PWR_LEVEL_HANDLER_ELEMENT {
	struct LINK_ENTRY rLinkEntry;
	PFN_PWR_LEVEL_HANDLER prPwrLevelHandler;
};

struct PARAM_CONNECTIVITY_LOG {
	uint8_t id;
	uint8_t len;
	uint8_t msg[];
};

struct BUFFERED_LOG_ENTRY {
	struct LINK_ENTRY rLinkEntry;
	uint8_t fgBuffered;
	uint8_t ucSn;
	uint8_t ucBssIdx;
	uint8_t aucLog[64];
};

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

#define KAL_SET_BIT(bitOffset, value)      set_bit(bitOffset, &value)
#define KAL_CLR_BIT(bitOffset, value)      clear_bit(bitOffset, &value)
#define KAL_TEST_AND_CLEAR_BIT(bitOffset, value)  \
	test_and_clear_bit(bitOffset, &value)
#define KAL_TEST_BIT(bitOffset, value)     test_bit(bitOffset, &value)
#define SUSPEND_FLAG_FOR_WAKEUP_REASON	(0)
#define SUSPEND_FLAG_CLEAR_WHEN_RESUME	(1)
#ifdef CFG_PDMA_SLPPRT_MODE_SUPPORT
#define GLUE_FLAG_WLAN_RESUME	(2)
#define GLUE_FLAG_WLAN_SUSPEND  (3)
#endif

#define KAL_WARN_ON WARN_ON
#define KAL_IS_ERR IS_ERR
#define KAL_MIN min

/*----------------------------------------------------------------------------*/
/* Macros of getting current thread id                                        */
/*----------------------------------------------------------------------------*/
#define KAL_GET_CURRENT_THREAD_ID() (current->pid)
#define KAL_GET_CURRENT_THREAD_NAME() (current->comm)

/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_SPIN_LOCK_DECLARATION()             unsigned long __ulFlags

#define KAL_ACQUIRE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	kalAcquireSpinLock(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory, &__ulFlags)

#define KAL_RELEASE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	kalReleaseSpinLock(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory, __ulFlags)

/*----------------------------------------------------------------------------*/
/* Macros of MUTEX operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_ACQUIRE_MUTEX(_prAdapter, _rLockCategory)   \
	kalAcquireMutex(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory)

#define KAL_RELEASE_MUTEX(_prAdapter, _rLockCategory)   \
	kalReleaseMutex(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory)

/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define KAL_GET_PKT_QUEUE_ENTRY(_p)             GLUE_GET_PKT_QUEUE_ENTRY(_p)
#define KAL_GET_PKT_DESCRIPTOR(_prQueueEntry)  \
	GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)
#define KAL_GET_PKT_TID(_p)                     GLUE_GET_PKT_TID(_p)
#define KAL_GET_PKT_IS1X(_p)                    GLUE_GET_PKT_IS1X(_p)
#define KAL_GET_PKT_HEADER_LEN(_p)              GLUE_GET_PKT_HEADER_LEN(_p)
#define KAL_GET_PKT_PAYLOAD_LEN(_p)             GLUE_GET_PKT_PAYLOAD_LEN(_p)
#define KAL_GET_PKT_ARRIVAL_TIME(_p)            GLUE_GET_PKT_ARRIVAL_TIME(_p)

/*----------------------------------------------------------------------------*/
/* Macros for kernel related defines                      */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
#define IEEE80211_CHAN_PASSIVE_FLAG	IEEE80211_CHAN_PASSIVE_SCAN
#define IEEE80211_CHAN_PASSIVE_STR		"PASSIVE"
#else
#define IEEE80211_CHAN_PASSIVE_FLAG	IEEE80211_CHAN_NO_IR
#define IEEE80211_CHAN_PASSIVE_STR		"NO_IR"
#endif

#if KERNEL_VERSION(4, 7, 0) <= CFG80211_VERSION_CODE
/**
 * enum nl80211_band - Frequency band
 * @NL80211_BAND_2GHZ: 2.4 GHz ISM band
 * @NL80211_BAND_5GHZ: around 5 GHz band (4.9 - 5.7 GHz)
 * @NL80211_BAND_60GHZ: around 60 GHz band (58.32 - 64.80 GHz)
 * @NUM_NL80211_BANDS: number of bands, avoid using this in userspace
 *	 since newer kernel versions may support more bands
 */
#define KAL_BAND_2GHZ NL80211_BAND_2GHZ
#define KAL_BAND_5GHZ NL80211_BAND_5GHZ
#if (CFG_SUPPORT_WIFI_6G == 1)
#define KAL_BAND_6GHZ NL80211_BAND_6GHZ
#endif
#define KAL_NUM_BANDS NUM_NL80211_BANDS
#else
#define KAL_BAND_2GHZ IEEE80211_BAND_2GHZ
#define KAL_BAND_5GHZ IEEE80211_BAND_5GHZ
#define KAL_NUM_BANDS IEEE80211_NUM_BANDS
#endif

/**
 * enum nl80211_reg_rule_flags - regulatory rule flags
 * @NL80211_RRF_NO_OFDM: OFDM modulation not allowed
 * @NL80211_RRF_AUTO_BW: maximum available bandwidth should be calculated
 *  base on contiguous rules and wider channels will be allowed to cross
 *  multiple contiguous/overlapping frequency ranges.
 * @NL80211_RRF_DFS: DFS support is required to be used
 */
#define KAL_RRF_NO_OFDM NL80211_RRF_NO_OFDM
#define KAL_RRF_DFS     NL80211_RRF_DFS
#if KERNEL_VERSION(3, 15, 0) > CFG80211_VERSION_CODE
#define KAL_RRF_AUTO_BW 0
#else
#define KAL_RRF_AUTO_BW NL80211_RRF_AUTO_BW
#endif

/**
 * kalCfg80211ScanDone - abstraction of cfg80211_scan_done
 *
 * @request: the corresponding scan request (sanity checked by callers!)
 * @aborted: set to true if the scan was aborted for any reason,
 *	userspace will be notified of that
 *
 * Since linux-4.8.y the 2nd parameter is changed from bool to
 * struct cfg80211_scan_info, but we don't use all fields yet.
 */
#if KERNEL_VERSION(4, 8, 0) <= CFG80211_VERSION_CODE
static inline void kalCfg80211ScanDone(struct cfg80211_scan_request *request,
				       bool aborted)
{
	struct cfg80211_scan_info info = {.aborted = aborted };

	cfg80211_scan_done(request, &info);
}
#else
static inline void kalCfg80211ScanDone(struct cfg80211_scan_request *request,
				       bool aborted)
{
	cfg80211_scan_done(request, aborted);
}
#endif

/**
 * kalCfg80211VendorEventAlloc - abstraction of cfg80211_vendor_event_alloc
 * cfg80211_vendor_event_alloc - allocate vendor-specific event skb
 * @wiphy: the wiphy
 * @event_idx: index of the vendor event in the wiphy's vendor_events
 * @approxlen: an upper bound of the length of the data that will
 *	be put into the skb
 * @gfp: allocation flags
 *
 * This function allocates and pre-fills an skb for an event on the
 * vendor-specific multicast group.
 *
 * When done filling the skb, call cfg80211_vendor_event() with the
 * skb to send the event.
 *
 * Return: An allocated and pre-filled skb. %NULL if any errors happen.
 *
 * Since linux-4.1.0 the 2nd parameter is added struct wireless_dev
 */

#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
static inline struct sk_buff *
kalCfg80211VendorEventAlloc(struct wiphy *wiphy, struct wireless_dev *wdev,
				 int approxlen, int event_idx, gfp_t gfp)
{
	return cfg80211_vendor_event_alloc(wiphy, wdev,
					approxlen, event_idx, gfp);
}
#else
static inline struct sk_buff *
kalCfg80211VendorEventAlloc(struct wiphy *wiphy, struct wireless_dev *wdev,
				 int approxlen, int event_idx, gfp_t gfp)
{
	return cfg80211_vendor_event_alloc(wiphy,
					approxlen, event_idx, gfp);
}
#endif

static inline void kalCfg80211VendorEvent(void *pvPacket)
{
	struct sk_buff *pkt = (struct sk_buff *)pvPacket;

	return cfg80211_vendor_event(pkt, GFP_KERNEL);
}

/* Consider on some Android platform, using request_firmware_direct()
 * may cause system failed to load firmware. So we still use
 * request_firmware().
 */
#define REQUEST_FIRMWARE(_fw, _name, _dev) \
	request_firmware(_fw, _name, _dev)

/*----------------------------------------------------------------------------*/
/* Macros of wake_lock operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
/* CONFIG_ANDROID is defined in Android kernel source */
#if (KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE)
#if (KERNEL_VERSION(4, 14, 149) <= LINUX_VERSION_CODE)
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
	_prWakeLock = wakeup_source_register(NULL, _pcName);

#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
{ \
	wakeup_source_unregister(_prWakeLock); \
	_prWakeLock = NULL; \
}
#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
{ \
	_prWakeLock = kalMemAlloc(sizeof(KAL_WAKE_LOCK_T), \
		VIR_MEM_TYPE); \
	if (!_prWakeLock) { \
		DBGLOG(HAL, ERROR, \
			"KAL_WAKE_LOCK_INIT init fail!\n"); \
	} \
	else { \
		wakeup_source_init(_prWakeLock, _pcName); \
	} \
}

#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
{ \
	if (_prWakeLock) { \
		wakeup_source_trash(_prWakeLock); \
		_prWakeLock = NULL; \
	} \
}
#endif
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock) \
{ \
	if (_prWakeLock) { \
		__pm_stay_awake(_prWakeLock); \
	} \
}

#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout) \
{ \
	if (_prWakeLock) { \
		__pm_wakeup_event(_prWakeLock, JIFFIES_TO_MSEC(_u4Timeout)); \
	} \
}

#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock) \
{ \
	if (_prWakeLock) { \
		__pm_relax(_prWakeLock); \
	} \
}

#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock) \
	((_prWakeLock) && ((_prWakeLock)->active))

#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
{ \
	_prWakeLock = kalMemAlloc(sizeof(KAL_WAKE_LOCK_T), \
		VIR_MEM_TYPE); \
	if (!_prWakeLock) { \
		DBGLOG(HAL, ERROR, \
			"KAL_WAKE_LOCK_INIT init fail!\n"); \
	} \
	else { \
		wake_lock_init(_prWakeLock, WAKE_LOCK_SUSPEND, _pcName); \
	} \
}

#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
{ \
	if (_prWakeLock) { \
		wake_lock_destroy(_prWakeLock); \
	} \
}

#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock) \
{ \
	if (_prWakeLock) { \
		wake_lock(_prWakeLock); \
	} \
}

#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout) \
{ \
	if (_prWakeLock) { \
		wake_lock_timeout(_prWakeLock, _u4Timeout); \
	} \
}

#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock) \
{ \
	if (_prWakeLock) { \
		wake_unlock(_prWakeLock); \
	} \
}

#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock) \
		((_prWakeLock) && wake_lock_active(_prWakeLock))
#endif

#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName)
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout)
#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock)
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief Cache memory allocation
 *
 * \param[in] u4Size Required memory size.
 * \param[in] eMemType  Memory allocation type
 *
 * \return Pointer to allocated memory
 *         or NULL
 */
/*----------------------------------------------------------------------------*/
#if DBG
#define kalMemAlloc(u4Size, eMemType) ({    \
	void *pvAddr; \
	if (eMemType == PHY_MEM_TYPE) { \
		if (in_interrupt()) \
			pvAddr = kmalloc(u4Size, GFP_ATOMIC);   \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
	} \
	else { \
		if (u4Size > PAGE_SIZE) \
			pvAddr = vmalloc(u4Size);   \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
	} \
	if (pvAddr) {   \
		allocatedMemSize += u4Size;   \
		DBGLOG(INIT, INFO, "0x%p(%ld) allocated (%s:%s)\n", \
		    pvAddr, (uint32_t)u4Size, __FILE__, __func__);  \
	}   \
	pvAddr; \
})
#else
#define kalMemAlloc(u4Size, eMemType) ({    \
	void *pvAddr; \
	if (eMemType == PHY_MEM_TYPE) { \
		if (in_interrupt()) \
			pvAddr = kmalloc(u4Size, GFP_ATOMIC);   \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
	} \
	else { \
		if (u4Size > PAGE_SIZE) \
			pvAddr = vmalloc(u4Size);   \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
	} \
	if (!pvAddr) \
		ASSERT_NOMEM(); \
	pvAddr; \
})
#endif

#define kalMemZAlloc(u4Size, eMemType) ({    \
	void *pvAddr; \
	pvAddr = kalMemAlloc(u4Size, eMemType); \
	if (pvAddr) \
		kalMemSet(pvAddr, 0, u4Size); \
	pvAddr; \
})

/*----------------------------------------------------------------------------*/
/*!
 * \brief Free allocated cache memory
 *
 * \param[in] pvAddr Required memory size.
 * \param[in] eMemType  Memory allocation type
 * \param[in] u4Size Allocated memory size.
 *
 * \return -
 */
/*----------------------------------------------------------------------------*/
#if DBG
#define kalMemFree(pvAddr, eMemType, u4Size)  \
{   \
	if (pvAddr) {   \
		allocatedMemSize -= u4Size; \
		DBGLOG(INIT, INFO, "0x%p(%ld) freed (%s:%s)\n", \
			pvAddr, (uint32_t)u4Size, __FILE__, __func__);  \
	}   \
	kvfree(pvAddr); \
}
#else
#define kalMemFree(pvAddr, eMemType, u4Size)  \
{   \
	kvfree(pvAddr); \
}
#endif

#define kalUdelay(u4USec)                           udelay(u4USec)
#define kalMdelay(u4MSec)                           mdelay(u4MSec)
#define kalMsleep(u4MSec)                           msleep(u4MSec)
#define kalUsleep(u4USec) \
{ \
	if (u4USec > 10000) \
		msleep(u4USec / 1000); \
	else \
		usleep_range(u4USec, u4USec + 50); \
}
#define kalUsleep_range(u4MinUSec, u4MaxUSec)  \
	usleep_range(u4MinUSec, u4MaxUSec)

/* Copy memory from user space to kernel space */
#define kalMemCopyFromUser(_pvTo, _pvFrom, _u4N)  \
	copy_from_user(_pvTo, _pvFrom, _u4N)

/* Copy memory from kernel space to user space */
#define kalMemCopyToUser(_pvTo, _pvFrom, _u4N)  \
	copy_to_user(_pvTo, _pvFrom, _u4N)

/* Copy memory block with specific size */
#define kalMemCopy(pvDst, pvSrc, u4Size)  \
	memcpy(pvDst, pvSrc, u4Size)

/* Set memory block with specific pattern */
#define kalMemSet(pvAddr, ucPattern, u4Size)  \
	memset(pvAddr, ucPattern, u4Size)

/* Copy memory block from IO memory */
#define kalMemCopyFromIo(pvDst, pvSrc, u4Size)  \
	memcpy_fromio(pvDst, pvSrc, u4Size)

/* Copy memory block to IO memory */
#define kalMemCopyToIo(pvDst, pvSrc, u4Size)  \
	memcpy_toio(pvDst, pvSrc, u4Size)

/* Set memory block to IO memory */
#define kalMemSetIo(pvAddr, ucPattern, u4Size)  \
	memset_io(pvAddr, ucPattern, u4Size)

/* Compare two memory block with specific length.
 * Return zero if they are the same.
 */
#define kalMemCmp(pvAddr1, pvAddr2, u4Size)  \
	memcmp(pvAddr1, pvAddr2, u4Size)

/* Zero specific memory block */
#define kalMemZero(pvAddr, u4Size)  \
	memset(pvAddr, 0, u4Size)

/* Move memory block with specific size */
#define kalMemMove(pvDst, pvSrc, u4Size)  \
	memmove(pvDst, pvSrc, u4Size)

#if KERNEL_VERSION(4, 0, 0) <= LINUX_VERSION_CODE
#define strnicmp(s1, s2, n)                         strncasecmp(s1, s2, n)
#endif

/* string operation */
#define kalStrCpy(dest, src)               strcpy(dest, src)
#define kalStrnCpy(dest, src, n)           strncpy(dest, src, n)
#define kalStrCmp(ct, cs)                  strcmp(ct, cs)
#define kalStrnCmp(ct, cs, n)              strncmp(ct, cs, n)
#define kalStrChr(s, c)                    strchr(s, c)
#define kalStrrChr(s, c)                   strrchr(s, c)
#define kalStrnChr(s, n, c)                strnchr(s, n, c)
#define kalStrLen(s)                       strlen(s)
#define kalStrnLen(s, b)                   strnlen(s, b)
#define kalStrniCmp(ct, cs, n)             strncasecmp(ct, cs, n)
/* #define kalStrtoul(cp, endp, base)      simple_strtoul(cp, endp, base) */
/* #define kalStrtol(cp, endp, base)       simple_strtol(cp, endp, base) */
#define kalkStrtou8(cp, base, resp)        kstrtou8(cp, base, resp)
#define kalkStrtou16(cp, base, resp)       kstrtou16(cp, base, resp)
#define kalkStrtou32(cp, base, resp)       kstrtou32(cp, base, resp)
#define kalkStrtos32(cp, base, resp)       kstrtos32(cp, base, resp)
#define kalSnprintf(buf, size, fmt, ...)   \
	_kalSnprintf((char *)(buf), (size_t)(size), \
		(const char *)(fmt), ##__VA_ARGS__)
#define kalScnprintf(buf, size, fmt, ...)  \
	scnprintf(buf, size, fmt, ##__VA_ARGS__)
#define kalSprintf(buf, fmt, ...)          \
	_kalSprintf((char *)(buf), (const char *)(fmt), ##__VA_ARGS__)
/* remove for AOSP */
/* #define kalSScanf(buf, fmt, ...)        sscanf(buf, fmt, __VA_ARGS__) */
#define kalStrStr(ct, cs)                  strstr(ct, cs)
#define kalStrSep(s, ct)                   strsep(s, ct)
#define kalStrCat(dest, src)               strcat(dest, src)
#define kalIsXdigit(c)                     isxdigit(c)
#define kalStrtoint(_data, _base, _res) kstrtoint(_data, _base, _res)
#define kalStrtoul(_data, _base, _res) kstrtoul(_data, _base, _res)

int8_t *strtok_r(int8_t *s, const int8_t *delim, int8_t **last);
#define kalStrtokR(_buf, _tok, _saved) \
	strtok_r((int8_t *)_buf, _tok, (int8_t **)_saved)

int8_t atoi(uint8_t ch);
#define kalAtoi(_ch) atoi(_ch)

/* defined for wince sdio driver only */
#if defined(_HIF_SDIO)
#define kalDevSetPowerState(prGlueInfo, ePowerMode) \
	glSetPowerState(prGlueInfo, ePowerMode)
#else
#define kalDevSetPowerState(prGlueInfo, ePowerMode)
#endif

#if CFG_MTK_ANDROID_WMT
#define _kalRequestFirmware request_firmware
#else
#define _kalRequestFirmware request_firmware
#endif

#if !defined(__GCC4_has_attribute___fallthrough__)
#define __GCC4_has_attribute___fallthrough__ 0
#endif

/* clone 'fallthrough' in include/linux/compiler_attributes.h */
#if __has_attribute(__fallthrough__)
#define kal_fallthrough __attribute__((__fallthrough__))
#else
#define kal_fallthrough do {} while (0)  /* fallthrough */
#endif

#define kal_max_t(_type, _v1, _v2) max_t(_type, _v1, _v2)
#define kal_min_t(_type, _v1, _v2) min_t(_type, _v1, _v2)

/*----------------------------------------------------------------------------*/
/*!
 * \brief Notify OS with SendComplete event of the specific packet.
 *        Linux should free packets here.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 * \param[in] status         Status Code for OS upper layer
 *
 * \return -
 */
/*----------------------------------------------------------------------------*/
#define kalSendComplete(prGlueInfo, pvPacket, status)   \
	    kalSendCompleteAndAwakeQueue(prGlueInfo, pvPacket)

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used to locate the starting address of incoming
 *        ethernet frame for skb.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 *
 * \return starting address of ethernet frame buffer.
 */
/*----------------------------------------------------------------------------*/
#define kalQueryBufferPointer(prGlueInfo, pvPacket)     \
	    ((uint8_t *)((struct sk_buff *)pvPacket)->data)

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used to query the length of valid buffer which is
 *        accessible during port read/write.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 *
 * \return starting address of ethernet frame buffer.
 */
/*----------------------------------------------------------------------------*/
#define kalQueryValidBufferLength(prGlueInfo, pvPacket)     \
	    ((uint32_t)((struct sk_buff *)pvPacket)->end -  \
	     (uint32_t)((struct sk_buff *)pvPacket)->data)

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used to copy the entire frame from skb to the
 *        destination address in the input parameter.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 * \param[in] pucDestBuffer  Destination Address
 *
 * \return -
 */
/*----------------------------------------------------------------------------*/
#define kalCopyFrame(prGlueInfo, pvPacket, pucDestBuffer)   \
	do {struct sk_buff *skb = (struct sk_buff *)pvPacket; \
	    memcpy(pucDestBuffer, skb->data, skb->len); } while (0)

#define kalGetTimeTick()                jiffies_to_msecs(jiffies)

#define kalGetTimeTickNs()              sched_clock()

#if CFG_SUPPORT_SA_LOG
#define kalPrintSALogLimited(fmt, ...)					\
({									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
		DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);	\
	ratelimit_set_flags(&_rs, RATELIMIT_MSG_ON_RELEASE);		\
									\
	if (__ratelimit(&_rs))						\
		kalPrintSALog(fmt, ##__VA_ARGS__);			\
})
#endif

#define WLAN_TAG                        "[wlan]"
#if CFG_SUPPORT_SA_LOG
#define kalPrint(_Fmt...) \
	(get_wifi_standalone_log_mode() == 1) \
	? kalPrintSALog(WLAN_TAG _Fmt) \
	: pr_info(WLAN_TAG _Fmt)
#define kalPrintLimited(_Fmt...) \
	(get_wifi_standalone_log_mode() == 1) \
	? kalPrintSALogLimited(WLAN_TAG _Fmt) \
	: pr_info_ratelimited(WLAN_TAG _Fmt)
#else
#define kalPrint(_Fmt...)               pr_info(WLAN_TAG _Fmt)
#define kalPrintLimited(_Fmt...)        pr_info_ratelimited(WLAN_TAG _Fmt)
#endif

#define kalBreakPoint() \
do { \
	WARN_ON(1); \
	panic("Oops"); \
} while (0)

#if CFG_ENABLE_AEE_MSG
#define kalSendAeeException                         aee_kernel_exception
#define kalSendAeeWarning                           aee_kernel_warning
#define kalSendAeeReminding                         aee_kernel_reminding
#else
#define kalSendAeeException(_module, _desc, ...)
#define kalSendAeeWarning(_module, _desc, ...)
#define kalSendAeeReminding(_module, _desc, ...)
#endif

#define PRINTF_ARG(...)      __VA_ARGS__
#define SPRINTF(buf, arg)    {buf += sprintf((char *)(buf), PRINTF_ARG arg); }

#define USEC_TO_SYSTIME(_usec)      ((_usec) / USEC_PER_MSEC)
#define MSEC_TO_SYSTIME(_msec)      (_msec)

#define MSEC_TO_JIFFIES(_msec)      msecs_to_jiffies(_msec)
#define JIFFIES_TO_MSEC(_jiffie)    jiffies_to_msecs(_jiffie)

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
#define get_ds() KERNEL_DS
#define kal_access_ok(type, addr, size) access_ok(addr, size)
#else
#define kal_access_ok(type, addr, size) access_ok(type, addr, size)
#endif
#define KAL_TIME_INTERVAL_DECLARATION()     struct timespec64 __rTs, __rTe
#define KAL_REC_TIME_START()                ktime_get_ts64(&__rTs)
#define KAL_REC_TIME_END()                  ktime_get_ts64(&__rTe)
#define KAL_GET_TIME_INTERVAL() \
	((SEC_TO_USEC(__rTe.tv_sec) + (uint32_t)NSEC_TO_USEC(__rTe.tv_nsec)) - \
	(SEC_TO_USEC(__rTs.tv_sec) + (uint32_t)NSEC_TO_USEC(__rTs.tv_nsec)))
#define KAL_ADD_TIME_INTERVAL(_Interval) \
	{ \
		(_Interval) += KAL_GET_TIME_INTERVAL(); \
	}

#if defined(_HIF_PCIE)
#define KAL_DMA_TO_DEVICE	PCI_DMA_TODEVICE
#define KAL_DMA_FROM_DEVICE	PCI_DMA_FROMDEVICE

#define KAL_DMA_ALLOC_COHERENT(_dev, _size, _handle) \
	pci_alloc_consistent(_dev, _size, _handle)
#define KAL_DMA_FREE_COHERENT(_dev, _size, _addr, _handle) \
	pci_free_consistent(_dev, _size, _addr, _handle)
#define KAL_DMA_MAP_SINGLE(_dev, _ptr, _size, _dir) \
	pci_map_single(_dev, _ptr, _size, _dir)
#define KAL_DMA_UNMAP_SINGLE(_dev, _addr, _size, _dir) \
	pci_unmap_single(_dev, _addr, _size, _dir)
#define KAL_DMA_MAPPING_ERROR(_dev, _addr) \
	pci_dma_mapping_error(_dev, _addr)
#else
#define KAL_DMA_TO_DEVICE	DMA_TO_DEVICE
#define KAL_DMA_FROM_DEVICE	DMA_FROM_DEVICE

#define KAL_DMA_ALLOC_COHERENT(_dev, _size, _handle) \
	dma_alloc_coherent(_dev, _size, _handle, GFP_DMA)
#define KAL_DMA_FREE_COHERENT(_dev, _size, _addr, _handle) \
	dma_free_coherent(_dev, _size, _addr, _handle)
#define KAL_DMA_MAP_SINGLE(_dev, _ptr, _size, _dir) \
	dma_map_single(_dev, _ptr, _size, _dir)
#define KAL_DMA_UNMAP_SINGLE(_dev, _addr, _size, _dir) \
	dma_unmap_single(_dev, _addr, _size, _dir)
#define KAL_DMA_MAPPING_ERROR(_dev, _addr) \
	dma_mapping_error(_dev, _addr)
#endif

#if CFG_SUPPORT_DATA_STALL
#define KAL_REPORT_ERROR_EVENT			kalIndicateDriverEvent
#endif

#if CFG_SUPPORT_BIGDATA_PIP
#define KAL_REPORT_BIGDATA_PIP			kalBigDataPip
#endif

/*----------------------------------------------------------------------------*/
/* Macros of show stack operations for using in Driver Layer                  */
/*----------------------------------------------------------------------------*/
#if CFG_MTK_ANDROID_WMT
#define kal_show_stack(_adapter, _task, _sp) \
	connectivity_export_show_stack(_task, _sp)
#else
#define kal_show_stack(_adapter, _task, _sp)
#endif

/*----------------------------------------------------------------------------*/
/* Macros of systrace operations for using in Driver Layer                    */
/*----------------------------------------------------------------------------*/
#if !CONFIG_WLAN_DRV_BUILD_IN

#define kalTraceBegin(_fmt, ...) \
	tracing_mark_write("B|%d|" _fmt "\n", current->tgid, ##__VA_ARGS__)

#define kalTraceEnd() \
	tracing_mark_write("E|%d\n", current->tgid)

#define kalTraceInt(_value, _fmt, ...) \
	tracing_mark_write("C|%d|" _fmt "|%d\n", \
		current->tgid, ##__VA_ARGS__, _value)

#define kalTraceCall() \
	{ kalTraceBegin("%s", __func__); kalTraceEnd(); }

#define kalTraceEvent(_fmt, ...) \
	{ kalTraceBegin(_fmt, ##__VA_ARGS__); kalTraceEnd(); }

#define __type_is_void(expr) __builtin_types_compatible_p(typeof(expr), void)
#define __expr_zero(expr) __builtin_choose_expr(__type_is_void(expr), 0, (expr))

#define TRACE(_expr, _fmt, ...) \
	__builtin_choose_expr(__type_is_void(_expr), \
	__TRACE_VOID(_expr, _fmt, ##__VA_ARGS__), \
	__TRACE(__expr_zero(_expr), _fmt, ##__VA_ARGS__))

#define __TRACE(_expr, _fmt, ...) \
	({ \
		typeof(_expr) __ret; \
		kalTraceBegin(_fmt, ##__VA_ARGS__); \
		__ret = (_expr); \
		kalTraceEnd(); \
		__ret; \
	})

#define __TRACE_VOID(_expr, _fmt, ...) \
	({ \
		kalTraceBegin(_fmt, ##__VA_ARGS__); \
		(void) (_expr); \
		kalTraceEnd(); \
	})
#else

#define kalTraceBegin(_fmt, ...)
#define kalTraceEnd()
#define kalTraceInt(_value, _fmt, ...)
#define kalTraceCall()
#define kalTraceEvent(_fmt, ...)
#define TRACE(_expr, _fmt, ...) _expr

#endif

#if KERNEL_VERSION(6, 4, 0) <= CFG80211_VERSION_CODE
#define KAL_CLASS_CREATE(__name)	class_create(__name)
#else
#define KAL_CLASS_CREATE(__name)	class_create(THIS_MODULE, __name)
#endif
/*----------------------------------------------------------------------------*/
/* Macros of wiphy operations for using in Driver Layer                       */
/*----------------------------------------------------------------------------*/
#define WIPHY_PRIV(_wiphy, _priv) \
	(_priv = *((struct GLUE_INFO **) wiphy_priv(_wiphy)))

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Routines in gl_kal.c                                                       */
/*----------------------------------------------------------------------------*/
void kalAcquireSpinLock(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_SPIN_LOCK_CATEGORY_E rLockCategory,
			OUT unsigned long *plFlags);

void kalReleaseSpinLock(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_SPIN_LOCK_CATEGORY_E rLockCategory,
			IN unsigned long ulFlags);

void kalUpdateMACAddress(IN struct GLUE_INFO *prGlueInfo,
			 IN uint8_t *pucMacAddr);

void kalAcquireMutex(IN struct GLUE_INFO *prGlueInfo,
		     IN enum ENUM_MUTEX_CATEGORY_E rMutexCategory);

void kalReleaseMutex(IN struct GLUE_INFO *prGlueInfo,
		     IN enum ENUM_MUTEX_CATEGORY_E rMutexCategory);

void kalPacketFree(IN struct GLUE_INFO *prGlueInfo,
		   IN void *pvPacket);

void *kalPacketAlloc(IN struct GLUE_INFO *prGlueInfo,
		     IN uint32_t u4Size,
		     OUT uint8_t **ppucData);

void *kalPacketAllocWithHeadroom(IN struct GLUE_INFO
				 *prGlueInfo,
				 IN uint32_t u4Size, OUT uint8_t **ppucData);

void kalOsTimerInitialize(IN struct GLUE_INFO *prGlueInfo,
			  IN void *prTimerHandler);

u_int8_t kalSetTimer(IN struct GLUE_INFO *prGlueInfo,
		     IN OS_SYSTIME rInterval);

uint32_t
kalProcessRxPacket(IN struct GLUE_INFO *prGlueInfo,
		   IN void *pvPacket,
		   IN uint8_t *pucPacketStart, IN uint32_t u4PacketLen,
		   /* IN PBOOLEAN           pfgIsRetain, */
		   IN u_int8_t fgIsRetain, IN enum ENUM_CSUM_RESULT aeCSUM[]);

uint32_t kalRxIndicatePkts(IN struct GLUE_INFO *prGlueInfo,
			   IN void *apvPkts[],
			   IN uint8_t ucPktNum);

uint32_t kalRxIndicateOnePkt(IN struct GLUE_INFO
			     *prGlueInfo, IN void *pvPkt);

#if CFG_SUPPORT_NAN
int kalIndicateNetlink2User(struct GLUE_INFO *prGlueInfo, void *pvBuf,
			    uint32_t u4BufLen);
void kalCreateUserSock(struct GLUE_INFO *prGlueInfo);
void kalReleaseUserSock(struct GLUE_INFO *prGlueInfo);
void kalNanIndicateStatusAndComplete(struct GLUE_INFO *prGlueInfo,
				     uint32_t eStatus, uint8_t ucRoleIdx);
#endif

void
kalIndicateStatusAndComplete(IN struct GLUE_INFO
			     *prGlueInfo,
			     IN uint32_t eStatus, IN void *pvBuf,
			     IN uint32_t u4BufLen,
			     IN uint8_t ucBssIndex);

void
kalUpdateReAssocReqInfo(IN struct GLUE_INFO *prGlueInfo,
			IN uint8_t *pucFrameBody, IN uint32_t u4FrameBodyLen,
			IN u_int8_t fgReassocRequest,
			IN uint8_t ucBssIndex);

#if CFG_SUPPORT_ASSURANCE
void kalUpdateDeauthInfo(IN struct GLUE_INFO
			 *prGlueInfo,
			 IN uint8_t *pucFrameBody,
			 IN uint32_t u4FrameBodyLen,
			 IN uint8_t ucBssIndex);
#endif

void kalUpdateReAssocRspInfo(IN struct GLUE_INFO
			     *prGlueInfo,
			     IN uint8_t *pucFrameBody,
			     IN uint32_t u4FrameBodyLen,
			     IN uint8_t ucBssIndex);

#if CFG_TX_FRAGMENT
u_int8_t
kalQueryTxPacketHeader(IN struct GLUE_INFO *prGlueInfo,
		       IN void *pvPacket, OUT uint16_t *pu2EtherTypeLen,
		       OUT uint8_t *pucEthDestAddr);
#endif /* CFG_TX_FRAGMENT */

void kalSendCompleteAndAwakeQueue(IN struct GLUE_INFO
				  *prGlueInfo,
				  IN void *pvPacket);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
void kalQueryTxChksumOffloadParam(IN void *pvPacket,
				  OUT uint8_t *pucFlag);

void kalUpdateRxCSUMOffloadParam(IN void *pvPacket,
				 IN enum ENUM_CSUM_RESULT eCSUM[]);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

u_int8_t kalRetrieveNetworkAddress(IN struct GLUE_INFO *prGlueInfo,
				IN OUT uint8_t *prMacAddr);

void
kalReadyOnChannel(IN struct GLUE_INFO *prGlueInfo,
		  IN uint64_t u8Cookie,
		  IN enum ENUM_BAND eBand, IN enum ENUM_CHNL_EXT eSco,
		  IN uint8_t ucChannelNum, IN uint32_t u4DurationMs,
		  IN uint8_t ucBssIndex);

void
kalRemainOnChannelExpired(IN struct GLUE_INFO *prGlueInfo,
			  IN uint64_t u8Cookie, IN enum ENUM_BAND eBand,
			  IN enum ENUM_CHNL_EXT eSco, IN uint8_t ucChannelNum,
			  IN uint8_t ucBssIndex);

#if CFG_SUPPORT_DFS
void
kalIndicateChannelSwitch(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_CHNL_EXT eSco,
			IN uint8_t ucChannelNum, IN enum ENUM_BAND eBand);
#endif

void
kalIndicateMgmtTxStatus(IN struct GLUE_INFO *prGlueInfo,
			IN uint64_t u8Cookie, IN u_int8_t fgIsAck,
			IN uint8_t *pucFrameBuf, IN uint32_t u4FrameLen,
			IN uint8_t ucBssIndex);

void kalIndicateRxMgmtFrame(IN struct ADAPTER *prAdapter,
				IN struct GLUE_INFO *prGlueInfo,
			    IN struct SW_RFB *prSwRfb,
			    IN uint8_t ucBssIndex);

#if CFG_SUPPORT_DATA_STALL
u_int8_t kalIndicateDriverEvent(struct ADAPTER *prAdapter,
				uint32_t event,
				uint16_t dataLen,
				uint8_t ucBssIdx,
				u_int8_t fgForceReport);
#endif

#if CFG_SUPPORT_BIGDATA_PIP
int8_t kalBigDataPip(struct ADAPTER *prAdapter,
					uint8_t *payload,
					uint16_t dataLen);
#endif

#if CFG_SUPPORT_DBDC
int8_t kalIndicateOpModeChange(struct ADAPTER *prAdapter,
					uint8_t ucBssIdx,
					uint8_t ucChannelBw,
					uint8_t ucTxNss,
					uint8_t ucRxNss);
#endif


/*----------------------------------------------------------------------------*/
/* Routines in interface - ehpi/sdio.c                                        */
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegRead(IN struct GLUE_INFO *prGlueInfo,
		       IN uint32_t u4Register,
		       OUT uint32_t *pu4Value);
u_int8_t kalDevRegRead_mac(IN struct GLUE_INFO *prGlueInfo,
			   IN uint32_t u4Register, OUT uint32_t *pu4Value);

u_int8_t kalDevRegWrite(struct GLUE_INFO *prGlueInfo,
			IN uint32_t u4Register,
			IN uint32_t u4Value);
u_int8_t kalDevRegWrite_mac(struct GLUE_INFO *prGlueInfo,
			    IN uint32_t u4Register, IN uint32_t u4Value);

u_int8_t
kalDevPortRead(IN struct GLUE_INFO *prGlueInfo,
	       IN uint16_t u2Port, IN uint32_t u2Len, OUT uint8_t *pucBuf,
	       IN uint32_t u2ValidOutBufSize);

u_int8_t
kalDevPortWrite(struct GLUE_INFO *prGlueInfo,
		IN uint16_t u2Port, IN uint32_t u2Len, IN uint8_t *pucBuf,
		IN uint32_t u2ValidInBufSize);

u_int8_t kalDevWriteData(IN struct GLUE_INFO *prGlueInfo,
			 IN struct MSDU_INFO *prMsduInfo);
enum ENUM_CMD_TX_RESULT kalDevWriteCmd(IN struct GLUE_INFO *prGlueInfo,
			IN struct CMD_INFO *prCmdInfo, IN uint8_t ucTC);
u_int8_t kalDevKickData(IN struct GLUE_INFO *prGlueInfo);
void kalDevReadIntStatus(IN struct ADAPTER *prAdapter,
			 OUT uint32_t *pu4IntStatus);

u_int8_t kalDevWriteWithSdioCmd52(IN struct GLUE_INFO
				  *prGlueInfo,
				  IN uint32_t u4Addr, IN uint8_t ucData);

#if CFG_SUPPORT_EXT_CONFIG
uint32_t kalReadExtCfg(IN struct GLUE_INFO *prGlueInfo);
#endif

u_int8_t
kalQoSFrameClassifierAndPacketInfo(IN struct GLUE_INFO
				   *prGlueInfo,
				   IN void *prPacket,
				   OUT struct TX_PACKET_INFO *prTxPktInfo);

u_int8_t kalGetEthDestAddr(IN struct GLUE_INFO *prGlueInfo,
			   IN void *prPacket,
			   OUT uint8_t *pucEthDestAddr);

void
kalOidComplete(IN struct GLUE_INFO *prGlueInfo,
	       IN struct CMD_INFO *prCmdInfo, IN uint32_t u4SetQueryInfoLen,
	       IN uint32_t rOidStatus);

uint32_t
kalIoctl(IN struct GLUE_INFO *prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN void *pvInfoBuf,
	 IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	 IN u_int8_t fgWaitResp,
	 IN u_int8_t fgCmd, OUT uint32_t *pu4QryInfoLen);

uint32_t
kalIoctlByBssIdx(IN struct GLUE_INFO *prGlueInfo,
	IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	IN void *pvInfoBuf,
	IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	IN u_int8_t fgWaitResp, IN u_int8_t fgCmd,
	OUT uint32_t *pu4QryInfoLen,
	IN uint8_t ucBssIndex);

void SET_IOCTL_BSSIDX(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

uint8_t GET_IOCTL_BSSIDX(
	IN struct ADAPTER *prAdapter);

void kalHandleAssocInfo(IN struct GLUE_INFO *prGlueInfo,
			IN struct EVENT_ASSOC_INFO *prAssocInfo);

#if CFG_ENABLE_FW_DOWNLOAD
void *kalFirmwareImageMapping(IN struct GLUE_INFO
			      *prGlueInfo,
			      OUT void **ppvMapFileBuf,
			      OUT uint32_t *pu4FileLength,
			      IN enum ENUM_IMG_DL_IDX_T eDlIdx);
void kalFirmwareImageUnmapping(IN struct GLUE_INFO
			       *prGlueInfo,
			       IN void *prFwHandle, IN void *pvMapFileBuf);
#endif

#if CFG_CHIP_RESET_SUPPORT
void kalRemoveProbe(IN struct GLUE_INFO *prGlueInfo);
#endif
/*----------------------------------------------------------------------------*/
/* Card Removal Check                                                         */
/*----------------------------------------------------------------------------*/
u_int8_t kalIsCardRemoved(IN struct GLUE_INFO *prGlueInfo);

/*----------------------------------------------------------------------------*/
/* TX                                                                         */
/*----------------------------------------------------------------------------*/
void kalFlushPendingTxPackets(IN struct GLUE_INFO
			      *prGlueInfo);

/*----------------------------------------------------------------------------*/
/* Media State Indication                                                     */
/*----------------------------------------------------------------------------*/
enum ENUM_PARAM_MEDIA_STATE kalGetMediaStateIndicated(
	IN struct GLUE_INFO
	*prGlueInfo, IN uint8_t ucBssIndex);

void kalSetMediaStateIndicated(IN struct GLUE_INFO *prGlueInfo,
		IN enum ENUM_PARAM_MEDIA_STATE eParamMediaStateIndicate,
		IN uint8_t ucBssIndex);

/*----------------------------------------------------------------------------*/
/* OID handling                                                               */
/*----------------------------------------------------------------------------*/
void kalOidCmdClearance(IN struct GLUE_INFO *prGlueInfo);

void kalOidClearance(IN struct GLUE_INFO *prGlueInfo);

void kalEnqueueCommand(IN struct GLUE_INFO *prGlueInfo,
		       IN struct QUE_ENTRY *prQueueEntry);

#if CFG_ENABLE_BT_OVER_WIFI
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi handling                                              */
/*----------------------------------------------------------------------------*/
void kalIndicateBOWEvent(IN struct GLUE_INFO *prGlueInfo,
			 IN struct BT_OVER_WIFI_EVENT *prEvent);

enum ENUM_BOW_DEVICE_STATE kalGetBowState(
	IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

u_int8_t kalSetBowState(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_BOW_DEVICE_STATE eBowState,
			uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

enum ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(
	IN struct GLUE_INFO
	*prGlueInfo);

uint32_t kalGetBowFreqInKHz(IN struct GLUE_INFO
			    *prGlueInfo);

uint8_t kalGetBowRole(IN struct GLUE_INFO *prGlueInfo,
		      IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

void kalSetBowRole(IN struct GLUE_INFO *prGlueInfo,
		   IN uint8_t ucRole,
		   IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

uint8_t kalGetBowAvailablePhysicalLinkCount(
	IN struct GLUE_INFO *prGlueInfo);

#if CFG_BOW_SEPARATE_DATA_PATH
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi Net Device Init/Uninit                                */
/*----------------------------------------------------------------------------*/
u_int8_t kalInitBowDevice(IN struct GLUE_INFO *prGlueInfo,
			  IN const char *prDevName);

u_int8_t kalUninitBowDevice(IN struct GLUE_INFO
			    *prGlueInfo);
#endif /* CFG_BOW_SEPARATE_DATA_PATH */
#endif /* CFG_ENABLE_BT_OVER_WIFI */

/*----------------------------------------------------------------------------*/
/* Security Frame Clearance                                                   */
/*----------------------------------------------------------------------------*/
void kalClearSecurityFrames(IN struct GLUE_INFO
			    *prGlueInfo);

void kalClearSecurityFramesByBssIdx(IN struct GLUE_INFO
				    *prGlueInfo,
				    IN uint8_t ucBssIndex);

void kalSecurityFrameSendComplete(IN struct GLUE_INFO
				  *prGlueInfo,
				  IN void *pvPacket, IN uint32_t rStatus);

/*----------------------------------------------------------------------------*/
/* Management Frame Clearance                                                 */
/*----------------------------------------------------------------------------*/
void kalClearMgmtFrames(IN struct GLUE_INFO *prGlueInfo);

void kalClearMgmtFramesByBssIdx(IN struct GLUE_INFO
				*prGlueInfo,
				IN uint8_t ucBssIndex);

uint32_t kalGetTxPendingFrameCount(IN struct GLUE_INFO
				   *prGlueInfo);

uint32_t kalGetTxPendingCmdCount(IN struct GLUE_INFO
				 *prGlueInfo);

void kalClearCommandQueue(IN struct GLUE_INFO *prGlueInfo);

u_int8_t kalSetTimer(IN struct GLUE_INFO *prGlueInfo,
		     IN uint32_t u4Interval);

u_int8_t kalCancelTimer(IN struct GLUE_INFO *prGlueInfo);

void kalScanDone(IN struct GLUE_INFO *prGlueInfo,
		 IN uint8_t ucBssIndex,
		 IN uint32_t status);

#if CFG_SUPPORT_SCAN_CACHE_RESULT
uint8_t kalUpdateBssTimestamp(IN struct GLUE_INFO *prGlueInfo);
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

uint32_t kalRandomNumber(void);

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
void kalTimeoutHandler(struct timer_list *timer);
#else
void kalTimeoutHandler(unsigned long arg);
#endif

void kalSetEvent(struct GLUE_INFO *pr);

void kalSetSerTimeoutEvent(struct GLUE_INFO *pr);

void kalSetIntEvent(struct GLUE_INFO *pr);

void kalSetDrvIntEvent(struct GLUE_INFO *pr);

void kalSetHifIntEvent(struct GLUE_INFO *pr, unsigned long ulBit);

void kalSetWmmUpdateEvent(struct GLUE_INFO *pr);

void kalSetMdCrashEvent(struct GLUE_INFO *pr);

void kalSetHifDbgEvent(struct GLUE_INFO *pr);

#if CFG_SUPPORT_MULTITHREAD
void kalSetTxEvent2Hif(struct GLUE_INFO *pr);

void kalSetTxEvent2Rx(struct GLUE_INFO *pr);

void kalSetTxCmdEvent2Hif(struct GLUE_INFO *pr);

void kalSetTxCmdDoneEvent(struct GLUE_INFO *pr);

void kalSetRxProcessEvent(struct GLUE_INFO *pr);
#endif
/*----------------------------------------------------------------------------*/
/* NVRAM/Registry Service                                                     */
/*----------------------------------------------------------------------------*/
u_int8_t kalIsConfigurationExist(IN struct GLUE_INFO
				 *prGlueInfo);

struct REG_INFO *kalGetConfiguration(IN struct GLUE_INFO
				     *prGlueInfo);

u_int8_t kalCfgDataRead(IN struct GLUE_INFO *prGlueInfo,
			IN uint32_t u4Offset,
			IN ssize_t len, OUT uint16_t *pu2Data);

u_int8_t kalCfgDataRead16(IN struct GLUE_INFO *prGlueInfo,
			  IN uint32_t u4Offset,
			  OUT uint16_t *pu2Data);

u_int8_t kalCfgDataWrite16(IN struct GLUE_INFO *prGlueInfo,
			   IN uint32_t u4Offset, IN uint16_t u2Data);

u_int8_t kalCfgDataWrite8(IN struct GLUE_INFO *prGlueInfo,
			   IN uint32_t u4Offset, IN uint8_t u2Data);


/*----------------------------------------------------------------------------*/
/* RSSI Updating                                                              */
/*----------------------------------------------------------------------------*/
void
kalUpdateRSSI(IN struct GLUE_INFO *prGlueInfo,
	      IN uint8_t ucBssIndex,
	      IN int8_t cRssi,
	      IN int8_t cLinkQuality);

/*----------------------------------------------------------------------------*/
/* I/O Buffer Pre-allocation                                                  */
/*----------------------------------------------------------------------------*/
u_int8_t kalInitIOBuffer(u_int8_t is_pre_alloc);

void kalUninitIOBuffer(void);

void *kalAllocateIOBuffer(IN uint32_t u4AllocSize);

void kalReleaseIOBuffer(IN void *pvAddr,
			IN uint32_t u4Size);

void
kalGetChannelList(IN struct GLUE_INFO *prGlueInfo,
		  IN enum ENUM_BAND eSpecificBand,
		  IN uint8_t ucMaxChannelNum, IN uint8_t *pucNumOfChannel,
		  IN struct RF_CHANNEL_INFO *paucChannelList);

u_int8_t kalIsAPmode(IN struct GLUE_INFO *prGlueInfo);

#if CFG_SUPPORT_802_11W
/*----------------------------------------------------------------------------*/
/* 802.11W                                                                    */
/*----------------------------------------------------------------------------*/
uint32_t kalGetMfpSetting(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex);
uint8_t kalGetRsnIeMfpCap(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex);
#endif

/*----------------------------------------------------------------------------*/
/* file opetation                                                             */
/*----------------------------------------------------------------------------*/
int32_t kalRequestFirmware(const uint8_t *pucPath,
			   uint8_t *pucData,
			   uint32_t u4Size, uint32_t *pu4ReadSize,
			   struct device *dev);


/*----------------------------------------------------------------------------*/
/* NL80211                                                                    */
/*----------------------------------------------------------------------------*/
void
kalIndicateBssInfo(IN struct GLUE_INFO *prGlueInfo,
		   IN uint8_t *pucFrameBuf, IN uint32_t u4BufLen,
		   IN uint8_t ucChannelNum, IN enum ENUM_BAND eBand,
		   IN int32_t i4SignalStrength);

/*----------------------------------------------------------------------------*/
/* Net device                                                                 */
/*----------------------------------------------------------------------------*/
uint32_t
kalHardStartXmit(struct sk_buff *prSkb,
		 IN struct net_device *prDev,
		 struct GLUE_INFO *prGlueInfo, uint8_t ucBssIndex);

u_int8_t kalIsPairwiseEapolPacket(IN void *prPacket);

u_int8_t
kalGetIPv4Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv4Addrs,
		  OUT uint32_t *pu4NumOfIpv4Addr);

#if IS_ENABLED(CONFIG_IPV6)
u_int8_t
kalGetIPv6Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv6Addrs,
		  OUT uint32_t *pu4NumOfIpv6Addr);
#else
static inline u_int8_t
kalGetIPv6Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv6Addrs,
		  OUT uint32_t *pu4NumOfIpv6Addr) {
	/* Not support IPv6 */
	*pu4NumOfIpv6Addr = 0;
	return 0;
}
#endif /* IS_ENABLED(CONFIG_IPV6) */

void kalSetNetAddressFromInterface(IN struct GLUE_INFO
				   *prGlueInfo,
				   IN struct net_device *prDev,
				   IN u_int8_t fgSet);

uint32_t kalResetStats(IN struct net_device *prDev);

void *kalGetStats(IN struct net_device *prDev);

void kalResetPacket(IN struct GLUE_INFO *prGlueInfo,
		    IN void *prPacket);

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
/*----------------------------------------------------------------------------*/
/* SDIO Read/Write Pattern Support                                            */
/*----------------------------------------------------------------------------*/
u_int8_t kalSetSdioTestPattern(IN struct GLUE_INFO
			       *prGlueInfo,
			       IN u_int8_t fgEn, IN u_int8_t fgRead);
#endif

/*----------------------------------------------------------------------------*/
/* PNO Support                                                                */
/*----------------------------------------------------------------------------*/
void kalSchedScanResults(IN struct GLUE_INFO *prGlueInfo);

void kalSchedScanStopped(IN struct GLUE_INFO *prGlueInfo,
			 u_int8_t fgDriverTriggerd);

void kalSetFwOwnEvent2Hif(struct GLUE_INFO *pr);
#if (CFG_SUPPORT_ICS == 1)
uint32_t kalOpenIcsDumpFile(void);
uint32_t kalWriteIcsDumpFile(uint8_t *pucBuffer, uint16_t u2Size);
#endif /* CFG_SUPPORT_ICS */
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if CFG_WOW_SUPPORT
void kalWowInit(IN struct GLUE_INFO *prGlueInfo);
void kalWowProcess(IN struct GLUE_INFO *prGlueInfo,
		   uint8_t enable);
#endif

int main_thread(void *data);

#if CFG_SUPPORT_MULTITHREAD
int hif_thread(void *data);
int rx_thread(void *data);
#endif
uint64_t kalGetBootTime(void);

int kalMetInitProcfs(IN struct GLUE_INFO *prGlueInfo);
int kalMetRemoveProcfs(void);

uint8_t kalGetEapolKeyType(void *prPacket);

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
u_int8_t kalIsWakeupByWlan(struct ADAPTER *prAdapter);
#endif

int32_t kalHaltLock(uint32_t waitMs);
int32_t kalHaltTryLock(void);
void kalHaltUnlock(void);
void kalSetHalted(u_int8_t fgHalt);
u_int8_t kalIsHalted(void);

void kalFreeTxMsduWorker(struct work_struct *work);
void kalFreeTxMsdu(struct ADAPTER *prAdapter,
		   struct MSDU_INFO *prMsduInfo);

int32_t kalPerMonInit(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonDisable(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonEnable(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonStart(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonStop(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonDestroy(IN struct GLUE_INFO *prGlueInfo);
void kalPerMonHandler(IN struct ADAPTER *prAdapter,
		      unsigned long ulParam);
uint32_t kalPerMonGetInfo(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucBuf,
			  IN uint32_t u4Max);
int32_t kalBoostCpu(IN struct ADAPTER *prAdapter,
		    IN uint32_t u4TarPerfLevel,
		    IN uint32_t u4BoostCpuTh);
int32_t kalCheckTputLoad(IN struct ADAPTER *prAdapter,
			 IN uint32_t u4CurrPerfLevel,
			 IN uint32_t u4TarPerfLevel,
			 IN int32_t i4Pending,
			 IN uint32_t u4Used);
uint32_t kalGetCpuBoostThreshold(void);
uint32_t kalGetEmiMetOffset(void);
void kalSetEmiMetOffset(uint32_t newEmiMetOffset);
void kalSetRpsMap(IN struct GLUE_INFO *glue, IN unsigned long value);
extern int set_task_util_min_pct(pid_t pid, unsigned int min);

#if CFG_MTK_ANDROID_EMI
void kalSetEmiMpuProtection(phys_addr_t emiPhyBase, bool enable);
void kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
			       uint32_t size);
#endif
int32_t kalSetCpuNumFreq(uint32_t u4CoreNum,
			 uint32_t u4Freq);
int32_t kalGetFwFlavor(uint8_t *flavor);
int32_t kalGetFwFlavorByPlat(uint8_t *flavor);
int32_t kalGetConnsysVerId(void);
int32_t kalPerMonSetForceEnableFlag(uint8_t uFlag);
int32_t kalFbNotifierReg(IN struct GLUE_INFO *prGlueInfo);
void kalFbNotifierUnReg(void);

#if KERNEL_VERSION(3, 0, 0) <= LINUX_VERSION_CODE
/* since: 0b5c9db1b11d3175bb42b80663a9f072f801edf5 */
static inline void kal_skb_reset_mac_len(struct sk_buff
		*skb)
{
	skb_reset_mac_len(skb);
}
#else
static inline void kal_skb_reset_mac_len(struct sk_buff
		*skb)
{
	skb->mac_len = skb->network_header - skb->mac_header;
}
#endif

void kalInitDevWakeup(struct ADAPTER *prAdapter, struct device *prDev);

u_int8_t kalIsValidMacAddr(const uint8_t *addr);

u_int8_t kalScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_scan_request *request, uint8_t *pucRandomMac);

u_int8_t kalSchedScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_sched_scan_request *request,
	uint8_t *pucRandomMac, uint8_t *pucRandomMacMask);

void kalScanReqLog(struct cfg80211_scan_request *request);
void kalScanResultLog(struct ADAPTER *prAdapter, struct ieee80211_mgmt *mgmt);
void kalScanLogCacheFlushBSS(struct ADAPTER *prAdapter,
	const uint16_t logBufLen);
int kalMaskMemCmp(const void *cs, const void *ct,
	const void *mask, size_t count);

u_int8_t
kalChannelScoSwitch(IN enum nl80211_channel_type channel_type,
		IN enum ENUM_CHNL_EXT *prChnlSco);

u_int8_t
kalChannelFormatSwitch(IN struct cfg80211_chan_def *channel_def,
		IN struct ieee80211_channel *channel,
		IN struct RF_CHANNEL_INFO *prRfChnlInfo);

#if CFG_SUPPORT_RX_GRO
uint32_t kal_is_skb_gro(struct ADAPTER *prAdapter, uint8_t ucBssIdx);
void kal_gro_flush(struct ADAPTER *prAdapter, struct net_device *prDev);
#endif

void kalRemoveBss(struct GLUE_INFO *prGlueInfo,
	uint8_t aucBSSID[],
	uint8_t ucChannelNum,
	enum ENUM_BAND eBand);

#if CFG_SUPPORT_WPA3
int kalExternalAuthRequest(IN struct ADAPTER *prAdapter,
			   IN uint8_t uBssIndex);
#endif
#if CFG_MODIFY_TX_POWER_BY_BAT_VOLT
int32_t kalBatNotifierReg(IN struct GLUE_INFO *prGlueInfo);
void kalEnableTxPwrBackoffByBattVolt(struct ADAPTER *prAdapter, bool ucEnable);
void kalSetTxPwrBackoffByBattVolt(struct ADAPTER *prAdapter, bool ucEnable);
void kalBatNotifierUnReg(void);
#endif

#if CFG_SUPPORT_NAN
void kalNanHandleVendorEvent(struct ADAPTER *prAdapter, uint8_t *prBuffer);
void kalNanHandlePendingCmd(IN struct ADAPTER *prAdapter, uint8_t *prBuffer);
#endif

int kalWlanUeventInit(void);
void kalWlanUeventDeinit(void);
u_int8_t kalSendUevent(const char *src);

int _kalSnprintf(char *buf, size_t size, const char *fmt, ...);
int _kalSprintf(char *buf, const char *fmt, ...);

/* systrace utilities */
#if !CONFIG_WLAN_DRV_BUILD_IN
void tracing_mark_write(const char *fmt, ...);
#endif

uint32_t kalSetSuspendFlagToEMI(IN struct ADAPTER
	*prAdapter, IN u_int8_t fgSuspend);

#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
extern uint32_t getFWLogOnOff(void);
#endif

void setTimeParameter(
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT *prChipConfigInfo,
	int chipConfigInfoSize, unsigned int second, unsigned int usecond);
uint32_t kalSyncTimeToFW(IN struct ADAPTER *prAdapter,
	IN u_int8_t fgInitCmd, unsigned int second, unsigned int usecond);
void kalSyncTimeToFWByIoctl(void);

void kalUpdateCompHdlrRec(IN struct ADAPTER *prAdapter,
	IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN struct CMD_INFO *prCmdInfo);

#if CFG_SUPPORT_SA_LOG
void kalPrintSALog(const char *fmt, ...);
#endif

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
void kalPwrLevelHdlrRegister(IN struct ADAPTER *prAdapter,
					PFN_PWR_LEVEL_HANDLER hdlr);
void kalPwrLevelHdlrUnregisterAll(IN struct ADAPTER *prAdapter);
void connsysPowerLevelNotify(IN struct ADAPTER *prAdapter);
void connsysPowerTempNotify(IN struct ADAPTER *prAdapter);
void connsysPowerTempUpdate(enum conn_pwr_msg_type status,
					int currentTemp);
uint32_t kalDumpPwrLevel(IN struct ADAPTER *prAdapter);
#endif
/* clone 'fallthrough' in include/linux/compiler_attributes.h */
#if __has_attribute(__fallthrough__)
#define kal_fallthrough __attribute__((__fallthrough__))
#else
#define kal_fallthrough do {} while (0)  /* fallthrough */
#endif

void kalReportWifiLog(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, uint8_t *log);
void kalBufferWifiLog(struct ADAPTER *prAdapter, uint8_t ucBssIndex,
				uint8_t *log, uint8_t ucSn);
struct BUFFERED_LOG_ENTRY *kalGetBufferLog(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, uint8_t ucSn);
void kalRemoveBufferLog(struct ADAPTER *prAdapter,
	struct BUFFERED_LOG_ENTRY *entry);
void kalClearBufferLog(struct ADAPTER *prAdapter);
#endif /* _GL_KAL_H */

