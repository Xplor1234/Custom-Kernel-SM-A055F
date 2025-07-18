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
 * Id: @(#)
 */

/*! \file   "scan.h"
 *    \brief
 *
 */


#ifndef _SCAN_H
#define _SCAN_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/*! Maximum buffer size of SCAN list */
#define SCN_MAX_BUFFER_SIZE \
	(CFG_MAX_NUM_BSS_LIST * ALIGN_4(sizeof(struct BSS_DESC)))

/* Remove SCAN result except the connected one. */
#define SCN_RM_POLICY_EXCLUDE_CONNECTED		BIT(0)

/* Remove the timeout one */
#define SCN_RM_POLICY_TIMEOUT			BIT(1)

/* Remove the oldest one with hidden ssid */
#define SCN_RM_POLICY_OLDEST_HIDDEN		BIT(2)

/* If there are more than half BSS which has the same ssid as connection
 * setting, remove the weakest one from them Else remove the weakest one.
 */
#define SCN_RM_POLICY_SMART_WEAKEST		BIT(3)

/* Remove entire SCAN result */
#define SCN_RM_POLICY_ENTIRE			BIT(4)

/* Remove SCAN result except the specific one. */
#define SCN_RM_POLICY_EXCLUDE_SPECIFIC_SSID	BIT(5)

/* This is used by POLICY SMART WEAKEST, If exceed this value, remove weakest
 * struct BSS_DESC with same SSID first in large network.
 */
#define SCN_BSS_DESC_SAME_SSID_THRESHOLD	20

#define SCN_BSS_DESC_STALE_SEC			20 /* Scan Request Timeout */

/* For WFD scan need about 15s. */
#define SCN_BSS_DESC_STALE_SEC_WFD		30

#define SCN_PROBE_DELAY_MSEC			0

#define SCN_ADHOC_BSS_DESC_TIMEOUT_SEC		5 /* Second. */
#if CFG_ENABLE_WIFI_DIRECT
#if CFG_SUPPORT_WFD
 /* Second. For WFD scan timeout. */
#define SCN_ADHOC_BSS_DESC_TIMEOUT_SEC_WFD	20
#endif
#endif

#define SCAN_DONE_DIFFERENCE			3

/* Full2Partial */
/* Define a full scan as scan channel number larger than this number */
#define SCAN_FULL2PARTIAL_CHANNEL_NUM           (25)
#if (CFG_SUPPORT_WIFI_6G == 1)
#define SCAN_CHANNEL_BITMAP_ARRAY_LEN           (8 + 8)
#else
#define SCAN_CHANNEL_BITMAP_ARRAY_LEN           (8)
#endif
#define BITS_OF_UINT                            (32)
#define BITS_OF_BYTE                            (8)

/* dwell time setting, should align FW setting */
#define SCAN_CHANNEL_DWELL_TIME_MIN_MSEC         (42)
#define SCAN_SPLIT_PACKETS_THRESHOLD		(30)
#define SCAN_CHANNEL_DWELL_TIME_LISTEN_MIN_MSEC	 (8)
/* dwell time for beacon timeout scan */
#define SCAN_BEACON_TIMEOUT_DWELL_TIME_MSEC      (100)

/* dwell time setting, reduce APP trigger scan dwell time to 20 */
#define SCAN_CHANNEL_MIN_DWELL_TIME_MSEC_APP	(20)
#define SCAN_CHANNEL_DWELL_TIME_MSEC_APP	(40)

/* dwell time setting for OCE certification */
#define SCAN_CHANNEL_DWELL_TIME_OCE         (42 + 28)


/*----------------------------------------------------------------------------*/
/* MSG_SCN_SCAN_REQ                                                           */
/*----------------------------------------------------------------------------*/
#define SCAN_REQ_SSID_WILDCARD			BIT(0)
#define SCAN_REQ_SSID_P2P_WILDCARD		BIT(1)
#define SCAN_REQ_SSID_SPECIFIED						\
	BIT(2) /* two probe req will be sent, wildcard and specified */
#define SCAN_REQ_SSID_SPECIFIED_ONLY					\
	BIT(3) /* only a specified ssid probe request will be sent */

/*----------------------------------------------------------------------------*/
/* Support Multiple SSID SCAN                                                 */
/*----------------------------------------------------------------------------*/
#define SCN_SSID_MAX_NUM			CFG_SCAN_SSID_MAX_NUM
#define SCN_SSID_MATCH_MAX_NUM			CFG_SCAN_SSID_MATCH_MAX_NUM

#if CFG_SUPPORT_AGPS_ASSIST
#define SCN_AGPS_AP_LIST_MAX_NUM		32
#endif

#define SCN_BSS_JOIN_FAIL_CNT_RESET_SEC		15
#define SCN_BSS_JOIN_FAIL_RESET_STEP		2

#if CFG_SUPPORT_BATCH_SCAN
/*----------------------------------------------------------------------------*/
/* SCAN_BATCH_REQ                                                             */
/*----------------------------------------------------------------------------*/
#define SCAN_BATCH_REQ_START			BIT(0)
#define SCAN_BATCH_REQ_STOP			BIT(1)
#define SCAN_BATCH_REQ_RESULT			BIT(2)
#endif

/* Support AP Setection */
#define SCN_BSS_JOIN_FAIL_THRESOLD          4

#define SCN_CTRL_SCAN_CHANNEL_LISTEN_TIME_ENABLE	BIT(1)
#define SCN_CTRL_IGNORE_AIS_FIX_CHANNEL			BIT(1)
#define SCN_CTRL_ENABLE					BIT(0)

#define SCN_CTRL_DEFAULT_SCAN_CTRL		SCN_CTRL_IGNORE_AIS_FIX_CHANNEL

#define SCN_SCAN_DONE_PRINT_BUFFER_LENGTH	350
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_SCAN_TYPE {
	SCAN_TYPE_PASSIVE_SCAN = 0,
	SCAN_TYPE_ACTIVE_SCAN,
	SCAN_TYPE_NUM
};

enum ENUM_SCAN_STATE {
	SCAN_STATE_IDLE = 0,
	SCAN_STATE_SCANNING,
	SCAN_STATE_NUM
};

enum ENUM_FW_SCAN_STATE {
	FW_SCAN_STATE_IDLE = 0,			/* 0 */
	FW_SCAN_STATE_SCAN_START,		/* 1 */
	FW_SCAN_STATE_REQ_CHANNEL,		/* 2 */
	FW_SCAN_STATE_SET_CHANNEL,		/* 3 */
	FW_SCAN_STATE_DELAYED_ACTIVE_PROB_REQ,	/* 4 */
	FW_SCAN_STATE_ACTIVE_PROB_REQ,		/* 5 */
	FW_SCAN_STATE_LISTEN,			/* 6 */
	FW_SCAN_STATE_SCAN_DONE,		/* 7 */
	FW_SCAN_STATE_NLO_START,		/* 8 */
	FW_SCAN_STATE_NLO_HIT_CHECK,		/* 9 */
	FW_SCAN_STATE_NLO_STOP,			/* 10 */
	FW_SCAN_STATE_BATCH_START,		/* 11 */
	FW_SCAN_STATE_BATCH_CHECK,		/* 12 */
	FW_SCAN_STATE_BATCH_STOP,		/* 13 */
	FW_SCAN_STATE_NUM			/* 14 */
};

enum ENUM_SCAN_CHANNEL {
	SCAN_CHANNEL_FULL = 0,
	SCAN_CHANNEL_2G4 = 1,
	SCAN_CHANNEL_5G = 2,
	SCAN_CHANNEL_P2P_SOCIAL = 3,
	SCAN_CHANNEL_SPECIFIED = 4,
	SCAN_CHANNEL_5G_NO_DFS = 5,
	SCAN_CHANNEL_5G_DFS_ONLY = 6,
	SCAN_CHANNEL_FULL_NO_DFS = 7,
	SCAN_CHANNEL_6G = 8,
	SCAN_CHANNEL_NUM
};

struct MSG_SCN_FSM {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint32_t u4Dummy;
};

enum ENUM_SCHED_SCAN_ACT {
	SCHED_SCAN_ACT_ENABLE = 0,
	SCHED_SCAN_ACT_DISABLE,
};

#define SCAN_LOG_PREFIX_MAX_LEN		(16)
#define SCAN_LOG_MSG_MAX_LEN		(500)
#define SCAN_LOG_BUFF_SIZE		(200)
#define SCAN_LOG_DYN_ALLOC_MEM		(0)

enum ENUM_SCAN_LOG_PREFIX {
	/* Scan */
	LOG_SCAN_REQ_K2D = 0,		/* 0 */
	LOG_SCAN_REQ_D2F,
	LOG_SCAN_RESULT_F2D,
	LOG_SCAN_RESULT_D2K,
	LOG_SCAN_DONE_F2D,
	LOG_SCAN_DONE_D2K,		/* 5 */

	/* Sched scan */
	LOG_SCHED_SCAN_REQ_START_K2D,
	LOG_SCHED_SCAN_REQ_START_D2F,
	LOG_SCHED_SCAN_REQ_STOP_K2D,
	LOG_SCHED_SCAN_REQ_STOP_D2F,
	LOG_SCHED_SCAN_DONE_F2D,	/* 10 */
	LOG_SCHED_SCAN_DONE_D2K,

	/* Scan abort */
	LOG_SCAN_ABORT_REQ_K2D,
	LOG_SCAN_ABORT_REQ_D2F,
	LOG_SCAN_ABORT_DONE_D2K,

	/* Driver only */
	LOG_SCAN_D2D,

	/* Last one */
	LOG_SCAN_MAX
};

/* IEEE Std 802.11 2020 Table 9-283 */
enum ESP_TRAFFIC_AC {
	ESP_AC_BK = 0,
	ESP_AC_BE = 1,
	ESP_AC_VI = 2,
	ESP_AC_VO = 3,
	ESP_AC_NUM = 4,
};

/*----------------------------------------------------------------------------*/
/* BSS Descriptors                                                            */
/*----------------------------------------------------------------------------*/
struct BSS_DESC {
	struct LINK_ENTRY rLinkEntry;
	/* Support AP Selection*/
	struct LINK_ENTRY rLinkEntryEss[KAL_AIS_NUM];
	struct LINK_ENTRY rLinkEntryEss1[KAL_AIS_NUM];

	uint8_t aucBSSID[MAC_ADDR_LEN];

	/* For IBSS, the SrcAddr is different from BSSID */
	uint8_t aucSrcAddr[MAC_ADDR_LEN];

	/* If we are going to connect to this BSS (JOIN or ROAMING to another
	 * BSS), don't remove this record from BSS List.
	 * Is a Bitmap, Bit0: BSS0, Bit1: Bss1
	 */
	u_int8_t fgIsConnecting;

	/* If we have connected to this BSS (NORMAL_TR), don't removed
	 * this record from BSS list.
	 * Is a Bitmap, Bit0: BSS0, Bit1: Bss1
	 */
	u_int8_t fgIsConnected;

	/* If we are in beacon timeout procedure, don't removed
	 * this record from BSS list to keep its channel. And we should not
	 * take the BSS as connection candidate for AP selection.
	 */
	u_int8_t fgIsInBTO;

	/* When this flag is TRUE, means the SSID of this
	 * BSS is not known yet.
	 */
	u_int8_t fgIsHiddenSSID;

	uint8_t ucSSIDLen;
	uint8_t aucSSID[ELEM_MAX_LEN_SSID];

	OS_SYSTIME rUpdateTime;

	enum ENUM_BSS_TYPE eBSSType;

	uint16_t u2CapInfo;

	uint16_t u2BeaconInterval;
	uint16_t u2ATIMWindow;

	uint16_t u2OperationalRateSet;
	uint16_t u2BSSBasicRateSet;
	u_int8_t fgIsUnknownBssBasicRate;

	u_int8_t fgIsERPPresent;
	u_int8_t fgIsHTPresent;
	u_int8_t fgIsVHTPresent;
#if (CFG_SUPPORT_802_11AX == 1)
	u_int8_t fgIsHEPresent;
	uint8_t ucHePhyCapInfo[HE_PHY_CAP_BYTE_NUM];
#if (CFG_SUPPORT_WIFI_6G == 1)
	u_int8_t fgIsHE6GPresent;
	u_int8_t fgIsCoHostedBssPresent;
#endif
#endif
#if (CFG_SUPPORT_802_11BE == 1)
	u_int8_t fgIsEHTPresent;
#endif

#if (CFG_SUPPORT_802_11V_MBSSID == 1)
	/* Max BSSID indicator. Range from 1 to 8.
	* 0 means MBSSID function is disabled
	*/
	u_int8_t ucMaxBSSIDIndicator;
	/* MBSSID index which DUT connected for this BSS.
	* 0 means DUT connect to transmitted BSSID
	*/
	u_int8_t ucMBSSIDIndex;
#endif

	uint8_t ucPhyTypeSet;	/* Available PHY Type Set of this BSS */

	/* record from bcn or probe response */
	uint8_t ucVhtCapNumSoundingDimensions;

	uint8_t ucChannelNum;

	/* Record bandwidth for association process. Some AP will
	 * send association resp by 40MHz BW
	 */
	enum ENUM_CHNL_EXT eSco;

	enum ENUM_CHANNEL_WIDTH eChannelWidth;	/* VHT, HE operation ie */
	uint8_t ucCenterFreqS1;
	uint8_t ucCenterFreqS2;
	uint8_t ucCenterFreqS3;
	enum ENUM_BAND eBand;

	uint8_t ucDTIMPeriod;
	u_int8_t fgTIMPresent;

	/* This BSS's TimeStamp is larger than us(TCL == 1 in RX_STATUS_T) */
	u_int8_t fgIsLargerTSF;

	uint8_t ucRCPI;

	/* A flag to indicate this BSS's WMM capability */
	uint8_t ucWmmFlag;

	/*! \brief The srbiter Search State will matched the scan result,
	 *   and saved the selected cipher and akm, and report the score,
	 *   for arbiter join state, join module will carry this target BSS
	 *   to rsn generate ie function, for gen wpa/rsn ie
	 */
	uint32_t u4RsnSelectedGroupCipher;
	uint32_t u4RsnSelectedPairwiseCipher;
	uint32_t u4RsnSelectedGroupMgmtCipher;
	uint32_t u4RsnSelectedAKMSuite;
	uint32_t u4RsnSelectedProto;
	uint32_t u4RsnSelectedPmf;
	enum ENUM_PARAM_AUTH_MODE eRsnSelectedAuthMode;

	uint16_t u2RsnCap;
	uint16_t u2RsnxCap;

	struct RSN_INFO rRSNInfo;
	struct RSN_INFO rWPAInfo;
	struct RSNX_INFO rRSNXInfo;
#if 1	/* CFG_SUPPORT_WAPI */
	struct WAPI_INFO rIEWAPI;
	u_int8_t fgIEWAPI;
#endif
	u_int8_t fgIERSN;
	u_int8_t fgIEWPA;
	u_int8_t fgIEOsen;
	u_int8_t fgIERSNX;

	/*! \brief RSN parameters selected for connection */
	/*! \brief The Select score for final AP selection,
	 *  0, no sec, 1,2,3 group cipher is WEP, TKIP, CCMP
	 */
	uint8_t ucEncLevel;

#if CFG_ENABLE_WIFI_DIRECT
	u_int8_t fgIsP2PPresent;
	u_int8_t fgIsP2PReport;	/* TRUE: report to upper layer */
	struct P2P_DEVICE_DESC *prP2pDesc;

	/* For IBSS, the SrcAddr is different from BSSID */
	uint8_t aucIntendIfAddr[MAC_ADDR_LEN];

#if 0 /* TODO: Remove this */
	/* Device Capability Attribute. (P2P_DEV_CAPABILITY_XXXX) */
	uint8_t ucDevCapabilityBitmap;

	/* Group Capability Attribute. (P2P_GROUP_CAPABILITY_XXXX) */
	uint8_t ucGroupCapabilityBitmap;
#endif

	struct LINK rP2pDeviceList;

/* P_LINK_T prP2pDeviceList; */

	/* For
	 *    1. P2P Capability.
	 *    2. P2P Device ID. ( in aucSrcAddr[] )
	 *    3. NOA   (TODO:)
	 *    4. Extend Listen Timing. (Probe Rsp)  (TODO:)
	 *    5. P2P Device Info. (Probe Rsp)
	 *    6. P2P Group Info. (Probe Rsp)
	 */
#endif

	/* the beacon doesn't advertise the FT AKM but will
	 * use FT when supported clients connect
	 */
	uint8_t ucIsAdaptive11r;

	/* for Cistco CCX AP that supports single PMK
	 * don't use PMKID thus always use auth SAE
	 */
	uint8_t ucIsCiscoCCXIePresent;

	/* The received IE length exceed the maximum IE buffer size */
	u_int8_t fgIsIEOverflow;

	uint16_t u2RawLength;		/* The byte count of aucRawBuf[] */
	uint16_t u2IELength;		/* The byte count of aucIEBuf[] */

	/* Place u8TimeStamp before aucIEBuf[1] to force DW align */
	union ULARGE_INTEGER u8TimeStamp;

	uint8_t aucRawBuf[CFG_RAW_BUFFER_SIZE];
	uint8_t aucIEBuf[CFG_IE_BUFFER_SIZE];
	uint16_t u2JoinStatus;
	OS_SYSTIME rJoinFailTime;

	/* Support AP Selection */
	struct AIS_BLACKLIST_ITEM *prBlack;

#if CFG_SUPPORT_802_11K
	struct NEIGHBOR_AP *prNeighbor;
	uint8_t fgQueriedCandidates;
#endif
#if CFG_SUPPORT_MBO
	uint8_t fgIsDisallowed;
	uint8_t fgExistEspIE;
	uint32_t u4EspInfo[ESP_AC_NUM];
#endif
	uint16_t u2StaCnt;
	uint16_t u2AvaliableAC; /* Available Admission Capacity */
	uint8_t ucJoinFailureCount;
	uint8_t ucChnlUtilization;
	uint8_t ucSNR;
	u_int8_t fgSeenProbeResp;
	u_int8_t fgExistBssLoadIE;
	u_int8_t fgMultiAnttenaAndSTBC;
	uint32_t u4UpdateIdx;
	uint8_t fgIotApActionValid;
	uint8_t ucIotApAct;
#if CFG_SUPPORT_RSN_SCORE
	u_int8_t fgIsRSNSuitableBss;
#endif
	uint32_t u4ApSelectionScore;
	uint32_t u4EstimatedTputByCu;
	uint32_t u4EstimatedTputByEsp;
	uint16_t u2MaximumMpdu;
	uint8_t fgIsRWMValid;
	uint16_t u2ReducedWanMetrics;
	uint8_t ucATF;
	uint8_t ucBaSize;
	uint16_t u2AMsduByte;
	uint8_t ucPpduDuration;
	uint32_t u4RssiFactor;
	uint32_t u4CUFactor;
	/* end Support AP Selection */
	int8_t cPowerLimit;
	uint8_t aucRrmCap[5];
#if CFG_SUPPORT_HE_ER
	uint8_t fgIsERSUDisable;
	uint8_t ucDCMMaxConRx;
#endif
};

struct SCAN_PARAM {	/* Used by SCAN FSM */
	/* Active or Passive */
	enum ENUM_SCAN_TYPE eScanType;

	/* Network Type */
	uint8_t ucBssIndex;

	/* Specified SSID Type */
	uint8_t ucSSIDType;
	uint8_t ucSSIDNum;
	uint8_t ucShortSSIDNum;

	/* Length of Specified SSID */
	uint8_t ucSpecifiedSSIDLen[SCN_SSID_MAX_NUM];

	/* Specified SSID */
	uint8_t aucSpecifiedSSID[SCN_SSID_MAX_NUM][ELEM_MAX_LEN_SSID];

#if CFG_ENABLE_WIFI_DIRECT
	u_int8_t fgFindSpecificDev;	/* P2P: Discovery Protocol */
	uint8_t aucDiscoverDevAddr[MAC_ADDR_LEN];
	u_int8_t fgIsDevType;
	struct P2P_DEVICE_TYPE rDiscoverDevType;

	/* TODO: Find Specific Device Type. */
#endif	/* CFG_ENABLE_WIFI_DIRECT */

	uint16_t u2ChannelDwellTime;
	uint16_t u2ChannelMinDwellTime;
	uint16_t u2TimeoutValue;
	uint16_t u2OpChStayTime;	/* ms unit */
	uint8_t ucDfsChDwellTime;	/* ms unit */
	uint8_t ucPerScanChCnt;

	uint8_t aucBSSID[CFG_SCAN_OOB_MAX_NUM][MAC_ADDR_LEN];

	enum ENUM_MSG_ID eMsgId;
	u_int8_t fgIsScanV2;

	/* Run time flags */
	uint16_t u2ProbeDelayTime;

	/* channel information */
	enum ENUM_SCAN_CHANNEL eScanChannel;
	uint8_t ucChannelListNum;
	struct RF_CHANNEL_INFO arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];

	/* random mac */
	uint8_t ucScnFuncMask;
	uint8_t aucRandomMac[MAC_ADDR_LEN];

	/* Feedback information */
	uint8_t ucSeqNum;

	/* For 6G OOB discovery*/
	uint8_t ucBssidMatchCh[CFG_SCAN_OOB_MAX_NUM];
	uint8_t ucBssidMatchSsidInd[CFG_SCAN_OOB_MAX_NUM];

	/* Information Element */
	uint16_t u2IELen;
	uint8_t aucIE[MAX_IE_LENGTH];
};

struct SCHED_SCAN_PARAM {	/* Used by SCAN FSM */
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;              /* Network Type */
	u_int8_t fgStopAfterIndication;  /* always FALSE */
	uint8_t ucMatchSSIDNum;          /* Match SSID */
	struct BSS_DESC *aprPendingBssDescToInd[SCN_SSID_MATCH_MAX_NUM];
};

struct SCAN_LOG_ELEM_BSS {
	struct LINK_ENTRY rLinkEntry;

	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint16_t u2SeqCtrl;
};

struct SCAN_LOG_CACHE {
	struct LINK rBSSListFW;
	struct LINK rBSSListCFG;

	struct SCAN_LOG_ELEM_BSS arBSSListBufFW[SCAN_LOG_BUFF_SIZE];
	struct SCAN_LOG_ELEM_BSS arBSSListBufCFG[SCAN_LOG_BUFF_SIZE];
};

struct SCAN_INFO {
	/* Store the STATE variable of SCAN FSM */
	enum ENUM_SCAN_STATE eCurrentState;

	OS_SYSTIME rLastScanCompletedTime;

	struct SCAN_PARAM rScanParam;
	struct SCHED_SCAN_PARAM rSchedScanParam;

	uint32_t u4NumOfBssDesc;

	uint8_t aucScanBuffer[SCN_MAX_BUFFER_SIZE];

	struct LINK rBSSDescList;

	struct LINK rFreeBSSDescList;

	struct LINK rPendingMsgList;

	/* Sparse Channel Detection */
	u_int8_t fgIsSparseChannelValid;
	struct RF_CHANNEL_INFO rSparseChannel;

	/* Sched scan state tracking */
	u_int8_t fgSchedScanning;

	/* Full2Partial */
	OS_SYSTIME u4LastFullScanTime;
	u_int8_t fgIsScanForFull2Partial;
	u_int8_t ucFull2PartialSeq;
	uint32_t au4ChannelBitMap[SCAN_CHANNEL_BITMAP_ARRAY_LEN];

	/*channel idle count # Mike */
	uint8_t		ucSparseChannelArrayValidNum;
	uint8_t		aucReserved[3];
	uint8_t		aucChannelNum[64];
	uint16_t	au2ChannelIdleTime[64];
	/* Mdrdy Count in each Channel  */
	uint8_t		aucChannelMDRDYCnt[64];
	/* Beacon and Probe Response Count in each Channel */
	uint8_t		aucChannelBAndPCnt[64];
	uint16_t	au2ChannelScanTime[64];
	/* eBand infor for differing the 2g4/6g */
	enum ENUM_BAND aeChannelBand[64];

	/* Support AP Selection */
	uint32_t u4ScanUpdateIdx;
	/* Scan log cache */
	struct SCAN_LOG_CACHE rScanLogCache;

#if CFG_SUPPORT_SCAN_NO_AP_RECOVERY
	uint8_t		ucScnZeroMdrdyTimes;
	uint8_t		ucScnZeroMdrdySerCnt;
	uint8_t		ucScnZeroMdrdySubsysResetCnt;
	uint8_t		ucScnTimeoutTimes;
	uint8_t		ucScnTimeoutSubsysResetCnt;
#endif
	/*Skip DFS channel scan or not */
	u_int8_t	fgSkipDFS;

	/* Support beacon report */
	uint8_t fgWipsBcnReport;
#if CFG_SUPPORT_SCAN_NO_AP_RECOVERY
	uint8_t		ucScnZeroChannelCnt;
	uint8_t		ucScnZeroChSubsysResetCnt;
#endif

};

/* Incoming Mailbox Messages */
struct MSG_SCN_SCAN_REQ {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;
	enum ENUM_SCAN_TYPE eScanType;

	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	uint8_t ucSSIDType;

	uint8_t ucSSIDLength;
	uint8_t aucSSID[PARAM_MAX_LEN_SSID];
	uint16_t u2ChannelDwellTime;	/* ms unit */
	uint16_t u2TimeoutValue;	/* ms unit */
	enum ENUM_SCAN_CHANNEL eScanChannel;
	uint8_t ucChannelListNum;
	struct RF_CHANNEL_INFO arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	uint16_t u2IELen;
	uint8_t aucIE[MAX_IE_LENGTH];
};

struct MSG_SCN_SCAN_REQ_V2 {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;
	enum ENUM_SCAN_TYPE eScanType;

	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	uint8_t ucSSIDType;

	uint8_t ucSSIDNum;
	struct PARAM_SSID *prSsid;
	uint16_t u2ProbeDelay;
	uint16_t u2ChannelDwellTime;	/* In TU. 1024us. */
	uint16_t u2ChannelMinDwellTime;	/* In TU. 1024us. */
	uint16_t u2TimeoutValue;	/* ms unit */
	uint16_t u2OpChStayTime;	/* ms unit */
	uint8_t ucDfsChDwellTime;	/* ms unit */
	uint8_t ucPerScanChCnt;

	uint8_t aucBSSID[MAC_ADDR_LEN];
	enum ENUM_SCAN_CHANNEL eScanChannel;
	uint8_t ucChannelListNum;
	struct RF_CHANNEL_INFO arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	uint8_t ucScnFuncMask;
	uint8_t aucRandomMac[MAC_ADDR_LEN];	/* random mac */

	/* pass from PARAM_SCAN_REQUEST_ADV.aucBssid */
	uint8_t aucExtBssid[CFG_SCAN_OOB_MAX_NUM][MAC_ADDR_LEN];
	uint8_t ucShortSSIDNum;
	/* For 6G OOB discovery*/
	uint8_t ucBssidMatchCh[CFG_SCAN_OOB_MAX_NUM];
	uint8_t ucBssidMatchSsidInd[CFG_SCAN_OOB_MAX_NUM];
	uint16_t u2IELen;
	uint8_t aucIE[MAX_IE_LENGTH];
};

struct MSG_SCN_SCAN_CANCEL {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;
	u_int8_t fgIsChannelExt;
	u_int8_t fgIsOidRequest;
};

/* Outgoing Mailbox Messages */
enum ENUM_SCAN_STATUS {
	SCAN_STATUS_DONE = 0,
	SCAN_STATUS_CANCELLED,
	SCAN_STATUS_FAIL,
	SCAN_STATUS_BUSY,
	SCAN_STATUS_NUM
};

struct MSG_SCN_SCAN_DONE {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;
	enum ENUM_SCAN_STATUS eScanStatus;
};

#if CFG_SUPPORT_AGPS_ASSIST
enum AP_PHY_TYPE {
	AGPS_PHY_A,
	AGPS_PHY_B,
	AGPS_PHY_G,
};

struct AGPS_AP_INFO {
	uint8_t aucBSSID[MAC_ADDR_LEN];
	int16_t i2ApRssi;	/* -127..128 */
	uint16_t u2Channel;	/* 0..256 */
	enum AP_PHY_TYPE ePhyType;
};

struct AGPS_AP_LIST {
	uint8_t ucNum;
	struct AGPS_AP_INFO arApInfo[SCN_AGPS_AP_LIST_MAX_NUM];
};
#endif

#if (CFG_SUPPORT_WIFI_RNR == 1)
struct NEIGHBOR_AP_INFO {
	struct LINK_ENTRY rLinkEntry;
	struct PARAM_SCAN_REQUEST_ADV rScanRequest;
	uint8_t aucScanIEBuf[MAX_IE_LENGTH];
};
#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
extern const char aucScanLogPrefix[][SCAN_LOG_PREFIX_MAX_LEN];

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#if DBG_DISABLE_ALL_LOG
#define scanlog_dbg(prefix, _Clz, _Fmt, ...)
#else /* DBG_DISABLE_ALL_LOG */
#define scanlog_dbg(prefix, _Clz, _Fmt, ...) \
	do { \
		if ((aucDebugModule[DBG_SCN_IDX] & \
			DBG_CLASS_##_Clz) == 0) \
			break; \
		LOG_FUNC("[%u]SCANLOG:(SCN " #_Clz ") %s " _Fmt, \
			KAL_GET_CURRENT_THREAD_ID(), \
			aucScanLogPrefix[prefix], ##__VA_ARGS__); \
	} while (0)
#endif /* DBG_DISABLE_ALL_LOG */

#define IS_6G_OP_CLASS(_opClass) \
	((_opClass >= 131) && (_opClass <= 135))

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Routines in scan.c                                                         */
/*----------------------------------------------------------------------------*/
void scnInit(IN struct ADAPTER *prAdapter);

void scnUninit(IN struct ADAPTER *prAdapter);

/* Scan utilities */
uint32_t scanCountBits(IN uint32_t bitMap[], IN uint32_t bitMapSize);

void scanSetRequestChannel(IN struct ADAPTER *prAdapter,
		IN uint32_t u4ScanChannelNum,
		IN struct RF_CHANNEL_INFO arChannel[],
		IN uint32_t u4ScanFlags,
		IN uint8_t fgIsOnlineScan,
		OUT struct MSG_SCN_SCAN_REQ_V2 *prScanReqMsg);

/* BSS-DESC Search */
struct BSS_DESC *scanSearchBssDescByBssid(IN struct ADAPTER *prAdapter,
					  IN uint8_t aucBSSID[]);

struct BSS_DESC *
scanSearchBssDescByBssidAndSsid(IN struct ADAPTER *prAdapter,
				IN uint8_t aucBSSID[],
				IN u_int8_t fgCheckSsid,
				IN struct PARAM_SSID *prSsid);

struct BSS_DESC *scanSearchBssDescByTA(IN struct ADAPTER *prAdapter,
				       IN uint8_t aucSrcAddr[]);

struct BSS_DESC *
scanSearchBssDescByTAAndSsid(IN struct ADAPTER *prAdapter,
			     IN uint8_t aucSrcAddr[],
			     IN u_int8_t fgCheckSsid,
			     IN struct PARAM_SSID *prSsid);

/* BSS-DESC Search - Alternative */
struct BSS_DESC *
scanSearchExistingBssDesc(IN struct ADAPTER *prAdapter,
			  IN enum ENUM_BSS_TYPE eBSSType,
			  IN uint8_t aucBSSID[],
			  IN uint8_t aucSrcAddr[]);

struct BSS_DESC *
scanSearchExistingBssDescWithSsid(IN struct ADAPTER *prAdapter,
				  IN enum ENUM_BSS_TYPE eBSSType,
				  IN uint8_t aucBSSID[],
				  IN uint8_t aucSrcAddr[],
				  IN u_int8_t fgCheckSsid,
				  IN struct PARAM_SSID *prSsid);

/* BSS-DESC Allocation */
struct BSS_DESC *scanAllocateBssDesc(IN struct ADAPTER *prAdapter);

/* BSS-DESC Removal */
void scanRemoveBssDescsByPolicy(IN struct ADAPTER *prAdapter,
				IN uint32_t u4RemovePolicy);

void scanRemoveBssDescByBssid(IN struct ADAPTER *prAdapter,
			      IN uint8_t aucBSSID[]);

void scanRemoveBssDescByBandAndNetwork(
				IN struct ADAPTER *prAdapter,
				IN enum ENUM_BAND eBand,
				IN uint8_t ucBssIndex);

/* BSS-DESC State Change */
void scanRemoveConnFlagOfBssDescByBssid(IN struct ADAPTER *prAdapter,
					IN uint8_t aucBSSID[],
					IN uint8_t ucBssIndex);

/* BSS-DESC Insertion - ALTERNATIVE */
struct BSS_DESC *scanAddToBssDesc(IN struct ADAPTER *prAdapter,
				  IN struct SW_RFB *prSwRfb);

uint32_t scanProcessBeaconAndProbeResp(IN struct ADAPTER *prAdapter,
				       IN struct SW_RFB *prSWRfb);

void
scanBuildProbeReqFrameCommonIEs(IN struct MSDU_INFO *prMsduInfo,
				IN uint8_t *pucDesiredSsid,
				IN uint32_t u4DesiredSsidLen,
				IN uint16_t u2SupportedRateSet);

uint32_t scanSendProbeReqFrames(IN struct ADAPTER *prAdapter,
				IN struct SCAN_PARAM *prScanParam);

void scanUpdateBssDescForSearch(IN struct ADAPTER *prAdapter,
				IN struct BSS_DESC *prBssDesc);

struct BSS_DESC *scanSearchBssDescByPolicy(IN struct ADAPTER *prAdapter,
					   IN uint8_t ucBssIndex);

uint32_t scanAddScanResult(IN struct ADAPTER *prAdapter,
			   IN struct BSS_DESC *prBssDesc,
			   IN struct SW_RFB *prSwRfb);

void scanReportBss2Cfg80211(IN struct ADAPTER *prAdapter,
			    IN enum ENUM_BSS_TYPE eBSSType,
			    IN struct BSS_DESC *SpecificprBssDesc);

bool scnEnableSplitScan(struct ADAPTER *prAdapter,
				uint8_t ucBssIndex,
				struct CMD_SCAN_REQ_V2 *prCmdScanReq);

/*----------------------------------------------------------------------------*/
/* Routines in scan_fsm.c                                                     */
/*----------------------------------------------------------------------------*/
void scnFsmSteps(IN struct ADAPTER *prAdapter,
		 IN enum ENUM_SCAN_STATE eNextState);

/*----------------------------------------------------------------------------*/
/* Command Routines                                                           */
/*----------------------------------------------------------------------------*/
void scnSendScanReq(IN struct ADAPTER *prAdapter);

void scnSendScanReqV2(IN struct ADAPTER *prAdapter);

/*----------------------------------------------------------------------------*/
/* RX Event Handling                                                          */
/*----------------------------------------------------------------------------*/
void scnEventScanDone(IN struct ADAPTER *prAdapter,
		      IN struct EVENT_SCAN_DONE *prScanDone,
		      u_int8_t fgIsNewVersion);

void scnEventSchedScanDone(IN struct ADAPTER *prAdapter,
		     IN struct EVENT_SCHED_SCAN_DONE *prSchedScanDone);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Handling                                                   */
/*----------------------------------------------------------------------------*/
void scnFsmMsgStart(IN struct ADAPTER *prAdapter,
		    IN struct MSG_HDR *prMsgHdr);

void scnFsmMsgAbort(IN struct ADAPTER *prAdapter,
		    IN struct MSG_HDR *prMsgHdr);

void scnFsmHandleScanMsg(IN struct ADAPTER *prAdapter,
			 IN struct MSG_SCN_SCAN_REQ *prScanReqMsg);

void scnFsmHandleScanMsgV2(IN struct ADAPTER *prAdapter,
			   IN struct MSG_SCN_SCAN_REQ_V2 *prScanReqMsg);

void scnFsmRemovePendingMsg(IN struct ADAPTER *prAdapter,
			    IN uint8_t ucSeqNum,
			    IN uint8_t ucBssIndex);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Generation                                                 */
/*----------------------------------------------------------------------------*/
void
scnFsmGenerateScanDoneMsg(IN struct ADAPTER *prAdapter,
			  IN enum ENUM_MSG_ID eMsgId,
			  IN uint8_t ucSeqNum,
			  IN uint8_t ucBssIndex,
			  IN enum ENUM_SCAN_STATUS eScanStatus);

/*----------------------------------------------------------------------------*/
/* Query for sparse channel                                                   */
/*----------------------------------------------------------------------------*/
u_int8_t scnQuerySparseChannel(IN struct ADAPTER *prAdapter,
			       enum ENUM_BAND *prSparseBand,
			       uint8_t *pucSparseChannel);

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
#if CFG_SUPPORT_PASSPOINT
struct BSS_DESC *scanSearchBssDescByBssidAndLatestUpdateTime(
						IN struct ADAPTER *prAdapter,
						IN uint8_t aucBSSID[]);
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_AGPS_ASSIST
void scanReportScanResultToAgps(struct ADAPTER *prAdapter);
#endif

#if CFG_SUPPORT_SCHED_SCAN
u_int8_t scnFsmSchedScanRequest(IN struct ADAPTER *prAdapter,
			IN struct PARAM_SCHED_SCAN_REQUEST *prSchedScanRequest);

u_int8_t scnFsmSchedScanStopRequest(IN struct ADAPTER *prAdapter);

u_int8_t scnFsmSchedScanSetAction(IN struct ADAPTER *prAdapter,
			IN enum ENUM_SCHED_SCAN_ACT ucSchedScanAct);

u_int8_t scnFsmSchedScanSetCmd(IN struct ADAPTER *prAdapter,
			IN struct CMD_SCHED_SCAN_REQ *prSchedScanCmd);

void scnSetSchedScanPlan(IN struct ADAPTER *prAdapter,
			IN struct CMD_SCHED_SCAN_REQ *prSchedScanCmd,
			IN uint16_t u2ScanInterval);

#endif /* CFG_SUPPORT_SCHED_SCAN */

#if CFG_SUPPORT_SCAN_NO_AP_RECOVERY
void scnDoZeroMdrdyRecoveryCheck(IN struct ADAPTER *prAdapter,
			IN struct EVENT_SCAN_DONE *prScanDone,
			IN struct SCAN_INFO *prScanInfo, IN uint8_t ucBssIndex);

void scnDoScanTimeoutRecoveryCheck(IN struct ADAPTER *prAdapter,
			IN uint8_t ucBssIndex);

void scnDoZeroChRecoveryCheck(IN struct ADAPTER *prAdapter,
			IN struct SCAN_INFO *prScanInfo);
#endif

void scanLogEssResult(struct ADAPTER *prAdapter);
void scanInitEssResult(struct ADAPTER *prAdapter);
#if CFG_SUPPORT_SCAN_CACHE_RESULT
/*----------------------------------------------------------------------------*/
/* Routines in scan_cache.c                                                   */
/*----------------------------------------------------------------------------*/
u_int8_t isScanCacheDone(struct GL_SCAN_CACHE_INFO *prScanCache);
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

void scanReqLog(struct CMD_SCAN_REQ_V2 *prCmdScanReq);
void scanResultLog(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb);
void scanLogCacheAddBSS(struct LINK *prList,
	struct SCAN_LOG_ELEM_BSS *prListBuf,
	enum ENUM_SCAN_LOG_PREFIX prefix,
	uint8_t bssId[], uint16_t seq);
void scanLogCacheFlushBSS(struct LINK *prList,
			enum ENUM_SCAN_LOG_PREFIX prefix);
void scanLogCacheFlushAll(struct ADAPTER *prAdapter,
	struct SCAN_LOG_CACHE *prScanLogCache,
	enum ENUM_SCAN_LOG_PREFIX prefix);

void scanRemoveBssDescFromList(IN struct LINK *prBSSDescList,
			       IN struct BSS_DESC *prBssDesc,
			       IN struct ADAPTER *prAdapter);
void scanInsertBssDescToList(IN struct LINK *prBSSDescList,
			     IN struct BSS_DESC *prBssDesc,
			     IN u_int8_t init);
void scanResetBssDesc(IN struct ADAPTER *prAdapter,
		      IN struct BSS_DESC *prBssDesc);

/* Check if VHT IE filled in Epigram IE */
void scanCheckEpigramVhtIE(IN uint8_t *pucBuf, IN struct BSS_DESC *prBssDesc);
void scanParseVHTCapIE(IN uint8_t *pucIE, IN struct BSS_DESC *prBssDesc);
void scanParseVHTOpIE(IN uint8_t *pucIE, IN struct BSS_DESC *prBssDesc);

void scanCheckAdaptive11rIE(IN uint8_t *pucBuf, IN struct BSS_DESC *prBssDesc);
void scanCheckCiscoCCXIE(IN uint8_t *pucBuf, IN struct BSS_DESC *prBssDesc);

void scanHandleOceIE(IN struct SCAN_PARAM *prScanParam,
	IN struct CMD_SCAN_REQ_V2 *prCmdScanReq);

void scnFsmDumpScanDoneInfo(IN struct ADAPTER *prAdapter,
	IN struct EVENT_SCAN_DONE *prScanDone);

#if (CFG_SUPPORT_WIFI_6G == 1)
void scanParseHEOpIE(IN uint8_t *pucIE, IN struct BSS_DESC *prBssDesc,
	IN enum ENUM_BAND eHwBand);
#endif

void scanUpdateSWIPSBcn(IN struct ADAPTER *prAdapter,
			IN struct BSS_DESC *prBss, IN uint8_t ucBssIndex);

void scanAbortBeaconRecv(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex,
			 IN enum SWPIS_ABORT_REASON abortReason);

void updateLinkStatsApRec(struct ADAPTER *prAdapter,
		struct BSS_DESC *prBssDesc);
#endif /* _SCAN_H */
