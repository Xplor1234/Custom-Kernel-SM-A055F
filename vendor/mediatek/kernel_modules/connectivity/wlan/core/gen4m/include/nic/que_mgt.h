/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "que_mgt.h"
 *  \brief  TX/RX queues management header file
 *
 *  The main tasks of queue management include TC-based HIF TX flow control,
 *  adaptive TC quota adjustment, HIF TX grant scheduling, Power-Save
 *  forwarding control, RX packet reordering, and RX BA agreement management.
 */


#ifndef _QUE_MGT_H
#define _QUE_MGT_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
extern uint8_t g_arTdlsLink[MAXNUM_TDLS_PEER];
extern const uint8_t *apucACI2Str[4];
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/* Queue Manager Features */
/* 1: Indicate the last TX packet to the FW for each burst */
#define QM_BURST_END_INFO_ENABLED       0
/* 1: To fairly share TX resource among active STAs */
#define QM_FORWARDING_FAIRNESS          1
#if defined(_HIF_SDIO)
/* 1: To adaptively adjust resource for each TC */
#define QM_ADAPTIVE_TC_RESOURCE_CTRL    1
/* 1: To fast adjust resource for EMPTY TC (assigned resource is 0) */
#define QM_FAST_TC_RESOURCE_CTRL        1
#else
/* 1: To adaptively adjust resource for each TC */
#define QM_ADAPTIVE_TC_RESOURCE_CTRL    0
/* 1: To fast adjust resource for EMPTY TC (assigned resource is 0) */
#define QM_FAST_TC_RESOURCE_CTRL        0
#endif
/* 1: To print TC resource adjustment results */
#define QM_PRINT_TC_RESOURCE_CTRL       0
/* 1: If pkt with SSN is missing, auto advance the RX reordering window */
#define QM_RX_WIN_SSN_AUTO_ADVANCING    1
/* 1: Indicate the packets falling behind to OS
 * before the frame with SSN is received
 */
#define QM_RX_INIT_FALL_BEHIND_PASS     1
/* 1: Count times of TC resource empty happened */
#define QM_TC_RESOURCE_EMPTY_COUNTER    1

/* Parameters */
/* p: Update queue lengths when p TX packets are enqueued */
#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
#define QM_INIT_TIME_TO_UPDATE_QUE_LEN  128
#else
#define QM_INIT_TIME_TO_UPDATE_QUE_LEN  256
#endif
/* s: Adjust the TC resource every s updates of queue lengths  */
#define QM_INIT_TIME_TO_ADJUST_TC_RSC   2
/* Factor for Que Len averaging */
#define QM_QUE_LEN_MOVING_AVE_FACTOR    3

#define QM_MIN_RESERVED_TC0_RESOURCE    0
#define QM_MIN_RESERVED_TC1_RESOURCE    1
#define QM_MIN_RESERVED_TC2_RESOURCE    0
#define QM_MIN_RESERVED_TC3_RESOURCE    0
/* Resource for TC4 is not adjustable */
#define QM_MIN_RESERVED_TC4_RESOURCE    2
#define QM_MIN_RESERVED_TC5_RESOURCE    0

#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
#define QM_MIN_RESERVED_TC6_RESOURCE    1
#define QM_MIN_RESERVED_TC7_RESOURCE    0
#define QM_MIN_RESERVED_TC8_RESOURCE    0
#define QM_MIN_RESERVED_TC9_RESOURCE    0
#define QM_MIN_RESERVED_TC10_RESOURCE   1
#define QM_MIN_RESERVED_TC11_RESOURCE   0
#define QM_MIN_RESERVED_TC12_RESOURCE   0
#define QM_MIN_RESERVED_TC13_RESOURCE   0
#endif

#define QM_GUARANTEED_TC0_RESOURCE      4
#define QM_GUARANTEED_TC1_RESOURCE      4
#define QM_GUARANTEED_TC2_RESOURCE      9
#define QM_GUARANTEED_TC3_RESOURCE      11
/* Resource for TC4 is not adjustable */
#define QM_GUARANTEED_TC4_RESOURCE      2
#define QM_GUARANTEED_TC5_RESOURCE      4

#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
#define QM_GUARANTEED_TC6_RESOURCE      4
#define QM_GUARANTEED_TC7_RESOURCE      9
#define QM_GUARANTEED_TC8_RESOURCE      11
#define QM_GUARANTEED_TC9_RESOURCE      4
#define QM_GUARANTEED_TC10_RESOURCE     4
#define QM_GUARANTEED_TC11_RESOURCE     9
#define QM_GUARANTEED_TC12_RESOURCE     11
#define QM_GUARANTEED_TC13_RESOURCE     11
#endif

#define QM_EXTRA_RESERVED_RESOURCE_WHEN_BUSY    0

#define QM_AVERAGE_TC_RESOURCE          6

#define QM_ACTIVE_TC_NUM                    TC_NUM

#define QM_MGMT_QUEUED_THRESHOLD            6

#define QM_MGMT_QUEUED_TIMEOUT              1000	/* ms */

#define QM_TEST_MODE                        0
#define QM_TEST_TRIGGER_TX_COUNT            50
#define QM_TEST_STA_REC_DETERMINATION       0
#define QM_TEST_STA_REC_DEACTIVATION        0
#define QM_TEST_FAIR_FORWARDING             0

#define QM_DEBUG_COUNTER                    0

/* Per-STA Queues: [0] AC0, [1] AC1, [2] AC2, [3] AC3 */
/* Per-Type Queues: [0] BMCAST */
#define NUM_OF_PER_STA_TX_QUEUES    4
#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
/* Each wmm set has its own BCM_IDX tx queue */
#define NUM_OF_PER_TYPE_TX_QUEUES   MAX_BSSID_NUM
#else
#define NUM_OF_PER_TYPE_TX_QUEUES   1
#endif


/* TX Queue Index */
/* Per-Type */
#define TX_QUEUE_INDEX_BMCAST       0
#define TX_QUEUE_INDEX_NO_STA_REC   0

/* Per-STA */
#define TX_QUEUE_INDEX_AC0          0
#define TX_QUEUE_INDEX_AC1          1
#define TX_QUEUE_INDEX_AC2          2
#define TX_QUEUE_INDEX_AC3          3
#define TX_QUEUE_INDEX_NON_QOS      TX_QUEUE_INDEX_AC1

#define QM_DEFAULT_USER_PRIORITY    0

#define QM_STA_FORWARD_COUNT_UNLIMITED      0xFFFFFFFF
/* Pending Forwarding Frame Threshold:
 *
 *   A conservative estimated value, to reserve enough free MSDU resource for
 *   OS packet, rather than full consumed by the pending forwarding frame.
 *
 *   Indeed, even if network subqueue is not stopped when no MSDU resource, the
 *   new arriving skb will be queued in prGlueInfo->rTxQueue and not be dropped.
 */
#define QM_FWD_PKT_QUE_THRESHOLD            (CFG_TX_MAX_PKT_NUM >> 1)

/* 1 WMM-related */
/* WMM FLAGS */
#define WMM_FLAG_SUPPORT_WMM                BIT(0)
#define WMM_FLAG_SUPPORT_WMMSA              BIT(1)
#define WMM_FLAG_AC_PARAM_PRESENT           BIT(2)
#define WMM_FLAG_SUPPORT_UAPSD              BIT(3)

/* WMM Admission Control Mandatory FLAGS */
#define ACM_FLAG_ADM_NOT_REQUIRED           0
#define ACM_FLAG_ADM_GRANTED                BIT(0)
#define ACM_FLAG_ADM_REQUIRED               BIT(1)

/* WMM Power Saving FLAGS */
#define AC_FLAG_TRIGGER_ENABLED             BIT(1)
#define AC_FLAG_DELIVERY_ENABLED            BIT(2)

/* WMM-2.2.1 WMM Information Element */
#define ELEM_MAX_LEN_WMM_INFO               7

/* WMM-2.2.2 WMM Parameter Element */
#define ELEM_MAX_LEN_WMM_PARAM              24

/* WMM-2.2.1 WMM QoS Info field */
#define WMM_QOS_INFO_PARAM_SET_CNT          BITS(0, 3)	/* Sent by AP */
#define WMM_QOS_INFO_UAPSD                  BIT(7)

#define WMM_QOS_INFO_VO_UAPSD               BIT(0)	/* Sent by non-AP STA */
#define WMM_QOS_INFO_VI_UAPSD               BIT(1)
#define WMM_QOS_INFO_BK_UAPSD               BIT(2)
#define WMM_QOS_INFO_BE_UAPSD               BIT(3)
#define WMM_QOS_INFO_MAX_SP_LEN_MASK        BITS(5, 6)
#define WMM_QOS_INFO_MAX_SP_ALL             0
#define WMM_QOS_INFO_MAX_SP_2               BIT(5)
#define WMM_QOS_INFO_MAX_SP_4               BIT(6)
#define WMM_QOS_INFO_MAX_SP_6               BITS(5, 6)

/* -- definitions for Max SP length field */
#define WMM_MAX_SP_LENGTH_ALL               0
#define WMM_MAX_SP_LENGTH_2                 2
#define WMM_MAX_SP_LENGTH_4                 4
#define WMM_MAX_SP_LENGTH_6                 6

/* WMM-2.2.2 WMM ACI/AIFSN field */
/* -- subfields in the ACI/AIFSN field */
#define WMM_ACIAIFSN_AIFSN                  BITS(0, 3)
#define WMM_ACIAIFSN_ACM                    BIT(4)
#define WMM_ACIAIFSN_ACI                    BITS(5, 6)
#define WMM_ACIAIFSN_ACI_OFFSET             5

/* -- definitions for ACI field */
#define WMM_ACI_AC_BE                       0
#define WMM_ACI_AC_BK                       BIT(5)
#define WMM_ACI_AC_VI                       BIT(6)
#define WMM_ACI_AC_VO                       BITS(5, 6)

#define WMM_ACI(_AC)                        (_AC << WMM_ACIAIFSN_ACI_OFFSET)

/* -- definitions for ECWmin/ECWmax field */
#define WMM_ECW_WMIN_MASK                   BITS(0, 3)
#define WMM_ECW_WMAX_MASK                   BITS(4, 7)
#define WMM_ECW_WMAX_OFFSET                 4

#define TXM_DEFAULT_FLUSH_QUEUE_GUARD_TIME  0	/* Unit: 64 us */

#define QM_RX_BA_ENTRY_MISS_TIMEOUT_MS		(200)
#if CFG_SUPPORT_IOT_AP_BLOCKLIST
#define QM_RX_BA_ENTRY_IOTAP_MISS_TIMEOUT_MS	(100)
#endif /* CFG_SUPPORT_IOT_AP_BLOCKLIST */
#if CFG_SUPPORT_LOWLATENCY_MODE
#define QM_RX_BA_ENTRY_MISS_TIMEOUT_MS_SHORT	(50)
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

/* BW80 NSS1 rate: MCS9 433 Mbps */
#define QM_DEQUE_PERCENT_VHT80_NSS1	23
/* BW40 NSS1 Max rate: 200 Mbps */
#define QM_DEQUE_PERCENT_VHT40_NSS1	10
/* BW20 NSS1 Max rate: 86.7Mbps */
#define QM_DEQUE_PERCENT_VHT20_NSS1	5

/* BW40 NSS1 Max rate: 150 Mbps (MCS9 200Mbps)*/
#define QM_DEQUE_PERCENT_HT40_NSS1	10
/* BW20 NSS1 Max rate: 72.2Mbps (MCS8 86.7Mbps)*/
#define QM_DEQUE_PERCENT_HT20_NSS1	5

#if (CFG_TC10_FEATURE == 1)
#define QM_ABS_PRES_LOG_BUF_SIZE	512
#define QA_ABS_PRES_LOG_MAX_COUNT	25
#endif

#define QM_ABSENCE_DETECT_INTERVAL      1000 /* Unit: ms */
#define QM_ABSENCE_DETECT_TIMEOUT      10000 /* Unit: ms */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

enum {
	QM_DBG_CNT_00 = 0,
	QM_DBG_CNT_01,
	QM_DBG_CNT_02,
	QM_DBG_CNT_03,
	QM_DBG_CNT_04,
	QM_DBG_CNT_05,
	QM_DBG_CNT_06,
	QM_DBG_CNT_07,
	QM_DBG_CNT_08,
	QM_DBG_CNT_09,
	QM_DBG_CNT_10,
	QM_DBG_CNT_11,
	QM_DBG_CNT_12,
	QM_DBG_CNT_13,
	QM_DBG_CNT_14,
	QM_DBG_CNT_15,
	QM_DBG_CNT_16,
	QM_DBG_CNT_17,
	QM_DBG_CNT_18,
	QM_DBG_CNT_19,
	QM_DBG_CNT_20,
	QM_DBG_CNT_21,
	QM_DBG_CNT_22,
	QM_DBG_CNT_23,
	QM_DBG_CNT_24,
	QM_DBG_CNT_25,
	QM_DBG_CNT_26,
	QM_DBG_CNT_27,
	QM_DBG_CNT_28,
	QM_DBG_CNT_29,
	QM_DBG_CNT_30,
	QM_DBG_CNT_31,
	QM_DBG_CNT_NUM
};

/* Used for MAC TX */
enum ENUM_MAC_TX_QUEUE_INDEX {
	MAC_TX_QUEUE_AC0_INDEX = 0,
	MAC_TX_QUEUE_AC1_INDEX,
	MAC_TX_QUEUE_AC2_INDEX,
	MAC_TX_QUEUE_AC3_INDEX,
	MAC_TX_QUEUE_AC4_INDEX,
	MAC_TX_QUEUE_AC5_INDEX,
	MAC_TX_QUEUE_AC6_INDEX,
	MAC_TX_QUEUE_BCN_INDEX,
	MAC_TX_QUEUE_BMC_INDEX,
	MAC_TX_QUEUE_NUM
};

#define SEQ_ADD(_SEQ, _INC) (((_SEQ) + (_INC)) & MAX_SEQ_NO)
#define SEQ_DIFF(_SEQ1, _SEQ2) (((_SEQ2) - (_SEQ1)) & MAX_SEQ_NO)
#define SEQ_INC(_SEQ) ((_SEQ) = ((_SEQ) + 1) & MAX_SEQ_NO)
#define SEQ_DEC(_SEQ) ((_SEQ) = ((_SEQ) - 1) & MAX_SEQ_NO)
#define SEQ_SMALLER(_SN1, _SN2) \
		(((_SN1) - (_SN2) & (MAX_SEQ_NO)) > HALF_SEQ_NO_COUNT)
#define IS_BAR_SSN_VALID(_prBaSsnEntry)  ((_prBaSsnEntry)->u2BarSSNIsValid)
#define CLR_BAR_SSN_VALID(_prBaSsnEntry) ((_prBaSsnEntry)->u2BarSSNIsValid = 0)
#define SET_BAR_SSN_VALID(_prBaSsnEntry) ((_prBaSsnEntry)->u2BarSSNIsValid = 1)

#define IS_FLUSHED_SSN_VALID(_prBaSsnEntry)  \
		((_prBaSsnEntry)->u2FlushedSSNIsValid)
#define CLR_FLUSHED_SSN_VALID(_prBaSsnEntry) \
		((_prBaSsnEntry)->u2FlushedSSNIsValid = 0)
#define SET_FLUSHED_SSN_VALID(_prBaSsnEntry) \
		((_prBaSsnEntry)->u2FlushedSSNIsValid = 1)

struct RX_BA_ENTRY {
	u_int8_t fgIsValid;
	struct QUE rReOrderQue;
	uint16_t u2WinStart;
	uint16_t u2WinEnd;
	uint16_t u2WinSize;

	uint16_t u2BarSSN:12;
	uint16_t u2BarSSNIsValid:1;
	uint16_t u2BarSSNReserved:3;

	uint16_t u2LastRcvdSN;
	uint16_t u2LastFallBehindDropSN;

	/* For identifying the RX BA agreement */
	uint8_t ucStaRecIdx;
	uint8_t ucTid;

	u_int8_t fgIsWaitingForPktWithSsn;

	struct TIMER rReorderBubbleTimer;
	uint16_t u2FirstBubbleSn;
	u_int8_t fgHasBubble;

#if CFG_SUPPORT_RX_AMSDU
	/* RX reorder for one MSDU in AMSDU issue */
	uint16_t u2SeqNo; /* for statistic */
	u_int8_t fgAmsduNeedLastFrame; /* for statistic */
	uint8_t ucLastAmsduSubIdx;
	u_int8_t fgIsAmsduDuplicated;
	u_int8_t fgNoDrop;
	uint32_t u4SNOverlapCount;
#endif
#if CFG_WOW_SUPPORT
	u_int8_t fgFirstSnToWinStart;
#endif

#if CFG_SUPPORT_RX_CACHE_INDEX
	struct SW_RFB *prCacheIndex[HALF_SEQ_NO_COUNT];
	uint16_t u2CacheIndexCount; /* monitor to flush when over threshold */
#endif
	uint16_t u2FlushedSSN:12; /* WinStart before flush */
	uint16_t u2FlushedSSNIsValid:1;
	uint16_t u2FlushedSSNReserved:3;

	u_int8_t fgFlushToHost; /* flush to host (1) or drop (0) */

#if (CFG_SUPPORT_CONNAC3X == 1)
	/* Detecting Peer TX retry over threshold to trigger TX reset */
	uint32_t u4RxRetryCount;
	uint16_t u2LoggedDropHeadSN;
#endif
	uint32_t u4ScrambleReset;
	struct {
		uint32_t u4Count;
		uint16_t u2SSN;
	} rDupDrop;
};

struct RX_BA_QUE_ENTRY {
	struct QUE_ENTRY rQueEntry;
	struct RX_BA_ENTRY *prReorderQueParm;
};

typedef uint32_t(*PFN_DEQUEUE_FUNCTION) (struct ADAPTER *prAdapter,
	struct QUE *prQue, uint8_t ucTC,
	uint32_t u4CurrentQuota,
	uint32_t *prPleCurrentQuota, uint32_t u4TotalQuota);

/* The mailbox message
 * (could be used for Host-To-Device or Device-To-Host Mailbox)
 */
struct MAILBOX_MSG {
	/* [0]: D2HRM0R or H2DRM0R, [1]: D2HRM1R or H2DRM1R */
	uint32_t u4Msg[2];
};

/* Used for adaptively adjusting TC resources */
struct TC_RESOURCE_CTRL {
	/* TC0, TC1, TC2, TC3, TC5 */
	uint32_t au4AverageQueLen[TC_NUM - 1];
};

struct QUE_MGT {	/* Queue Management Control Info */
	/* Per-Type Queues: [0] BMCAST or UNKNOWN-STA packets */
	struct QUE arTxQueue[NUM_OF_PER_TYPE_TX_QUEUES];

#if 0
	/* For TX Scheduling */
	uint8_t arRemainingTxOppt[NUM_OF_PER_STA_TX_QUEUES];
	uint8_t arCurrentTxStaIndex[NUM_OF_PER_STA_TX_QUEUES];

#endif

	/* Reordering Queue Parameters */
	struct RX_BA_ENTRY arRxBaTable[CFG_NUM_OF_RX_BA_AGREEMENTS];

	/* Current number of activated
	 * RX BA agreements <= CFG_NUM_OF_RX_BA_AGREEMENTS
	 */
	uint8_t ucRxBaCount;

#if QM_TEST_MODE
	uint32_t u4PktCount;
	struct ADAPTER *prAdapter;

#if QM_TEST_FAIR_FORWARDING
	uint32_t u4CurrentStaRecIndexToEnqueue;
#endif

#endif

#if QM_FORWARDING_FAIRNESS
	/* The current resource used count
	 * for a STA with respect to a TC index
	 */
#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
	uint32_t au4ResourceUsedCount[TC_NUM];
#else
	uint32_t au4ResourceUsedCount[NUM_OF_PER_STA_TX_QUEUES];
#endif

	/* The current serving STA with respect to a TC index */
#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
	uint32_t au4HeadStaRecIndex[TC_NUM];
#else
	uint32_t au4HeadStaRecIndex[NUM_OF_PER_STA_TX_QUEUES];
#endif

	/* The current serving BssInfo */
	uint32_t u4HeadBssInfoIndex;

	/* For TC5 only */
	u_int8_t fgGlobalQFirst;
	uint32_t u4GlobalResourceUsedCount;
#endif

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	uint32_t au4AverageQueLen[TC_NUM];
	uint32_t au4CurrentTcResource[TC_NUM]; /* unit: frame */
	/* The minimum amount of resource no matter busy or idle */
	uint32_t au4MinReservedTcResource[TC_NUM];
	/* The minimum amount of resource when extremely busy */
	uint32_t au4GuaranteedTcResource[TC_NUM];

	uint32_t u4TimeToAdjustTcResource;
	uint32_t u4TimeToUpdateQueLen;

	uint32_t u4QueLenMovingAverage;
	uint32_t u4ExtraReservedTcResource;
	uint32_t u4ResidualTcResource;

	/* Set to TRUE if the last TC adjustment has not been
	 * completely applied (i.e., waiting more TX-Done events
	 */
	/* to align the TC quotas to the TC resource assignment) */
	u_int8_t fgTcResourcePostAnnealing;

#if QM_FAST_TC_RESOURCE_CTRL
	u_int8_t fgTcResourceFastReaction;
#endif

	u_int8_t fgForceReassign;

#endif

#if QM_DEBUG_COUNTER
	uint32_t au4QmDebugCounters[QM_DBG_CNT_NUM];
#endif

	uint32_t u4TxAllowedStaCount;

#if QM_TC_RESOURCE_EMPTY_COUNTER
	uint32_t au4QmTcResourceEmptyCounter[MAX_BSSID_NUM][TC_NUM];
	uint32_t au4DequeueNoTcResourceCounter[TC_NUM];
	/*
	 * how many page count back during statistics intervals
	 */
	uint32_t au4QmTcResourceBackCounter[TC_NUM];
	/*
	 * how many page count used to TX frame during statistics intervals
	 */
	uint32_t au4QmTcUsedPageCounter[TC_NUM];
	uint32_t au4QmTcWantedPageCounter[TC_NUM];

	uint32_t u4EnqueueCounter;
	uint32_t u4DequeueCounter;
#endif

	uint32_t u4MaxForwardBufferCount;

	OS_SYSTIME rLastTxPktDumpTime;
	u_int8_t fgIsTxResrouceControlEn;
};

struct EVENT_TX_ADDBA {
	uint8_t      ucStaRecIdx;
	uint8_t      ucTid;
	uint8_t      ucWinSize;
	/* AMSDU in AMPDU is enabled or not (TID bitmap)*/
	uint8_t      ucAmsduEnBitmap;
	uint16_t     u2SSN;

	/*
	 * AMSDU count/length limits by count *OR* length
	 * Count: MPDU count in an AMSDU shall not exceed ucMaxMpduCount
	 * Length: AMSDU length shall not exceed u4MaxMpduLen
	 */
	uint8_t
	ucMaxMpduCount;     /* Max MPDU count in an AMSDU */
	uint8_t      aucReserved1[1];
	uint32_t     u4MaxMpduLen;       /* Max AMSDU length */

	/*
	 * Note: If length of a packet < u4MinMpduLen then it shall not be
	 * aggregated in an AMSDU
	 */
	uint32_t
	u4MinMpduLen;       /* Min MPDU length to be AMSDU */
	uint8_t      aucReserved2[16];
};

struct EVENT_RX_ADDBA {
	/* Fields not present in the received ADDBA_REQ */
	uint8_t ucStaRecIdx;

	/* Fields that are present in the received ADDBA_REQ */
	uint8_t ucDialogToken;	/* Dialog Token chosen by the sender */
	uint16_t u2BAParameterSet;	/* BA policy, TID, buffer size */
	uint16_t u2BATimeoutValue;
	uint16_t u2BAStartSeqCtrl;	/* SSN */
};

struct EVENT_RX_DELBA {
	/* Fields not present in the received ADDBA_REQ */
	uint8_t ucStaRecIdx;
	uint8_t ucTid;
	uint8_t aucReserved[2];
};

struct EVENT_BSS_ABSENCE_PRESENCE {
	/* Event Body */
	uint8_t ucBssIndex;
	uint8_t ucIsAbsent;
	uint8_t ucBssFreeQuota;
	uint8_t aucReserved[1];
};

struct EVENT_STA_CHANGE_PS_MODE {
	/* Event Body */
	uint8_t ucStaRecIdx;
	uint8_t ucIsInPs;
	uint8_t ucUpdateMode;
	uint8_t ucFreeQuota;
};

/* The free quota is used by PS only now */
/* The event may be used by per STA flow conttrol in general */
struct EVENT_STA_UPDATE_FREE_QUOTA {
	/* Event Body */
	uint8_t ucStaRecIdx;
	uint8_t ucUpdateMode;
	uint8_t ucFreeQuota;
	uint8_t aucReserved[1];
};

struct EVENT_STA_SAP_DSCB_IE {
	/* Event Body */
	uint8_t  ucBssIndex;
	uint8_t  fgIsDscbEnable;
	uint16_t u2DscbBitmap;
	uint8_t  aucReserved[4];
};

struct EVENT_CHECK_REORDER_BUBBLE {
	/* Event Body */
	uint8_t ucStaRecIdx;
	uint8_t ucTid;
};

/* Wi-Fi Multimedia Technical Specification
 * with WMM-Power Save and WMM-Admission Control
 * Version 1.2.0
 */
/* WMM-2.2.1 WMM Information Element */
struct IE_WMM_INFO {
	uint8_t ucId;		/* Element ID */
	uint8_t ucLength;	/* Length */
	uint8_t aucOui[3];	/* OUI */
	uint8_t ucOuiType;	/* OUI Type */
	uint8_t ucOuiSubtype;	/* OUI Subtype */
	uint8_t ucVersion;	/* Version */
	uint8_t ucQosInfo;	/* QoS Info field */
	uint8_t ucDummy[3];	/* Dummy for pack */
};

/* WMM-2.2.2 Figure 9 AC Parameters Record Format
 * 802.11 2020 Figure 9-295 - AC_BE, AC_BK, AC_VI, and AC_VO Parameter Record
 * field format
 */
__KAL_ATTRIB_PACKED_FRONT__
struct WMM_AC_PARAM {
	/* WMM-2.2.2 Figure 10 ACI/AIFSN Field
	 * 802.11 2020 Figure 9-296 - ACI/AIFSN field format
	 * BITS(0, 3): AIFSN
	 * BITS(4): ACM
	 * BITS(5, 6): ACI
	 * BITS(7): Reserved
	 */
	uint8_t ucAciAifsn;
	/* WMM-2.2.2 Figure 11 ECWmin/ECWmax field
	 * 802.11 2020 Figure 9-297 - ECWmin/ECWmax field format
	 * BITS(0, 3): ECWmin
	 * BITS(4, 7): ECWmax
	 */
	uint8_t ucEcw;
	uint16_t u2TxopLimit;
} __KAL_ATTRIB_PACKED__;

/* WMM-2.2.2 Figure 8 WMM Parameter Element */
__KAL_ATTRIB_PACKED_FRONT__
struct IE_WMM_PARAM {
	uint8_t ucId;		/* Element ID */
	uint8_t ucLength;	/* Length */

	/* IE Body */
	uint8_t aucOui[3];	/* OUI */
	uint8_t ucOuiType;	/* OUI Type */
	uint8_t ucOuiSubtype;	/* OUI Subtype */
	uint8_t ucVersion;	/* Version */

	/* WMM IE Body */
	uint8_t ucQosInfo;	/* QoS Info field */
	uint8_t ucReserved;

	/* AC Parameters */
	struct WMM_AC_PARAM arAcParam[4];
} __KAL_ATTRIB_PACKED__;

struct IE_WMM_TSPEC {
	uint8_t ucId;		/* Element ID */
	uint8_t ucLength;	/* Length */
	uint8_t aucOui[3];	/* OUI */
	uint8_t ucOuiType;	/* OUI Type */
	uint8_t ucOuiSubtype;	/* OUI Subtype */
	uint8_t ucVersion;	/* Version */
	/* WMM TSPEC body */
	uint8_t aucTsInfo[3];	/* TS Info */
	/* Note: Utilize PARAM_QOS_TSPEC to fill (memory copy) */
	uint8_t aucTspecBodyPart[];
};

struct IE_WMM_HDR {
	uint8_t ucId;		/* Element ID */
	uint8_t ucLength;	/* Length */
	uint8_t aucOui[3];	/* OUI */
	uint8_t ucOuiType;	/* OUI Type */
	uint8_t ucOuiSubtype;	/* OUI Subtype */
	uint8_t ucVersion;	/* Version */
	uint8_t aucBody[];	/* IE body */
};

__KAL_ATTRIB_PACKED_FRONT__
struct AC_QUE_PARMS {
	uint16_t u2CWmin;	/*!< CWmin */
	uint16_t u2CWmax;	/*!< CWmax */
	uint16_t u2TxopLimit;	/*!< TXOP limit */
	uint16_t u2Aifsn;	/*!< AIFSN */
	uint8_t ucGuradTime;	/*!< GuardTime for STOP/FLUSH. */
	uint8_t ucIsACMSet;
} __KAL_ATTRIB_PACKED__;

/* 802.11 2020 9.4.2.28 EDCA Parameter Set element
 * Figure 9-293 - EDCA Parameter Set element format
 */
__KAL_ATTRIB_PACKED_FRONT__
struct IE_EDCA_PARAM_SET {
	uint8_t ucId;
	uint8_t ucLength;
	/* 802.11 2020 Figure 9-100 - QoS Info field format when sent by an AP
	 * BITS(0, 3): EDCA Parameter Set Update Count
	 * BITS(4): Q-Ack
	 * BITS(5): Queue Request
	 * BITS(6): TXOP Request
	 * BITS(7): Reserved
	 */
	uint8_t ucQosInfo;
	/* 802.11 2020 Figure 9-294 - Update EDCA Info field format
	 * BITS(0): Override
	 * BITS(1, 2): PS-Poll ACI
	 * BITS(3, 4): RAW ACI
	 * BITS(5, 6): STA Type
	 * BITS(7): Reserved
	 */
	uint8_t ucUpdateEdcaInfo;
	struct WMM_AC_PARAM arAcParam[4];
} __KAL_ATTRIB_PACKED__;

#if (CFG_SUPPORT_802_11AX == 1)
/* MU EDCA parameters for each AC */
struct _CMD_MU_EDCA_PARAMS_T {
	uint8_t ucECWmin;	/* CWmin */
	uint8_t ucECWmax;	/* CWmax */
	uint8_t ucAifsn;		/* AIFSN */
	uint8_t ucIsACMSet;
	uint8_t ucMUEdcaTimer;
	uint8_t aucPadding[3];
};
#endif

/* WMM ACI (AC index) */
enum ENUM_WMM_ACI {
	WMM_AC_BE_INDEX = 0,
	WMM_AC_BK_INDEX,
	WMM_AC_VI_INDEX,
	WMM_AC_VO_INDEX,
	WMM_AC_INDEX_NUM
};

/* Used for CMD Queue Operation */
enum ENUM_FRAME_ACTION {
	FRAME_ACTION_DROP_PKT = 0,
	FRAME_ACTION_QUEUE_PKT,
	FRAME_ACTION_TX_PKT,
	FRAME_ACTION_NUM
};

enum ENUM_FRAME_TYPE_IN_CMD_Q {
	FRAME_TYPE_802_1X = 0,
	FRAME_TYPE_MMPDU,
	FRAME_TYPE_NUM
};

enum ENUM_FREE_QUOTA_MODET {
	FREE_QUOTA_UPDATE_MODE_INIT = 0,
	FREE_QUOTA_UPDATE_MODE_OVERWRITE,
	FREE_QUOTA_UPDATE_MODE_INCREASE,
	FREE_QUOTA_UPDATE_MODE_DECREASE
};

struct CMD_UPDATE_WMM_PARMS {
	struct AC_QUE_PARMS arACQueParms[WMM_AC_INDEX_NUM];
	uint8_t ucBssIndex;
	uint8_t fgIsQBSS;
	uint8_t ucWmmSet;
	uint8_t aucReserved;
};

#if (CFG_SUPPORT_802_11AX == 1)
struct _CMD_MQM_UPDATE_MU_EDCA_PARMS_T {
	/* DWORD_0 - Common Part */
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body */
	uint8_t ucBssIndex;
	uint8_t fgIsQBSS;
	uint8_t ucWmmSet;
	uint8_t aucPadding1[1];

	struct _CMD_MU_EDCA_PARAMS_T arMUEdcaParams[WMM_AC_INDEX_NUM];
	uint8_t aucPadding[32];
};
#endif

struct CMD_TX_AMPDU {
	u_int8_t fgEnable;
	u_int8_t fgApply;
	uint8_t aucReserved[2];
};

struct CMD_ADDBA_REJECT {
	u_int8_t fgEnable;
	u_int8_t fgApply;
	uint8_t aucReserved[2];
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

#define QM_TX_GET_NEXT_MSDU_INFO(_prMsduInfo) \
	((struct MSDU_INFO *)(((_prMsduInfo)->rQueEntry).prNext))

#define QM_RX_SET_NEXT_SW_RFB(_prSwRfbPreceding, _prSwRfbNext) \
	((((_prSwRfbPreceding)->rQueEntry).prNext) = \
	(struct QUE_ENTRY *)(_prSwRfbNext))

#define QM_RX_GET_NEXT_SW_RFB(_prSwRfb) \
	((struct SW_RFB *)(((_prSwRfb)->rQueEntry).prNext))

#define QM_GET_STA_REC_PTR_FROM_INDEX(_prAdapter, _ucIndex) \
	cnmGetStaRecByIndex(_prAdapter, _ucIndex)

#if 0
#define QM_TX_SET_MSDU_INFO_FOR_DATA_PACKET( \
		_prMsduInfo, \
		_ucTC, \
		_ucPacketType, \
		_ucFormatID, \
		_fgIs802_1x, \
		_fgIs802_11, \
		_u2PalLLH, \
		_u2AclSN, \
		_ucPsForwardingType, \
		_ucPsSessionID \
					) \
	{ \
		ASSERT(_prMsduInfo); \
		(_prMsduInfo)->ucTC = (_ucTC); \
		(_prMsduInfo)->ucPacketType = (_ucPacketType); \
		(_prMsduInfo)->ucFormatID = (_ucFormatID); \
		(_prMsduInfo)->fgIs802_1x = (_fgIs802_1x); \
		(_prMsduInfo)->fgIs802_11 = (_fgIs802_11); \
		(_prMsduInfo)->u2PalLLH = (_u2PalLLH); \
		(_prMsduInfo)->u2AclSN = (_u2AclSN); \
		(_prMsduInfo)->ucPsForwardingType = (_ucPsForwardingType); \
		(_prMsduInfo)->ucPsSessionID = (_ucPsSessionID); \
		(_prMsduInfo)->fgIsBurstEnd = (FALSE); \
	}
#else
#define QM_TX_SET_MSDU_INFO_FOR_DATA_PACKET( \
		_prMsduInfo, \
		_ucTC, \
		_ucPacketType, \
		_ucFormatID, \
		_fgIs802_1x, \
		_fgIs802_11, \
		_ucPsForwardingType \
					) \
	{ \
		ASSERT(_prMsduInfo); \
		(_prMsduInfo)->ucTC = (_ucTC); \
		(_prMsduInfo)->ucPacketType = (_ucPacketType); \
		(_prMsduInfo)->ucFormatID = (_ucFormatID); \
		(_prMsduInfo)->fgIs802_1x = (_fgIs802_1x); \
		(_prMsduInfo)->fgIs802_11 = (_fgIs802_11); \
		(_prMsduInfo)->ucPsForwardingType = (_ucPsForwardingType); \
	}
#endif

#define QM_INIT_STA_REC( \
		prAdapter, \
		_prStaRec, \
		_fgIsValid, \
		_fgIsQoS, \
		_pucMacAddr \
		) \
	{ \
		ASSERT(_prStaRec); \
		(_prStaRec)->fgIsValid = (_fgIsValid); \
		(_prStaRec)->fgIsQoS = (_fgIsQoS); \
		qmSetStaPS(prAdapter, prStaRec, FALSE); \
		(_prStaRec)->ucPsSessionID = 0xFF; \
		COPY_MAC_ADDR((_prStaRec)->aucMacAddr, (_pucMacAddr)); \
	}

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
#define QM_GET_TX_QUEUE_LEN(_prAdapter, _u4QueIdx) \
	(((_prAdapter)->rQM.au4AverageQueLen[(_u4QueIdx)] >> \
	(_prAdapter)->rQM.u4QueLenMovingAverage))
#endif

#define WMM_IE_OUI_TYPE(fp)      (((struct IE_WMM_HDR *)(fp))->ucOuiType)
#define WMM_IE_OUI_SUBTYPE(fp)   (((struct IE_WMM_HDR *)(fp))->ucOuiSubtype)
#define WMM_IE_OUI(fp)           (((struct IE_WMM_HDR *)(fp))->aucOui)

#if QM_DEBUG_COUNTER
#define QM_DBG_CNT_INC(_prQM, _index) \
	{ (_prQM)->au4QmDebugCounters[(_index)]++; }
#else
#define QM_DBG_CNT_INC(_prQM, _index) {}
#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Queue Management and STA_REC Initialization                                */
/*----------------------------------------------------------------------------*/

void qmInit(struct ADAPTER *prAdapter,
	    u_int8_t isTxResrouceControlEn);

#if QM_TEST_MODE
void qmTestCases(struct ADAPTER *prAdapter);
#endif

void qmActivateStaRec(struct ADAPTER *prAdapter,
		      struct STA_RECORD *prStaRec);

void qmDeactivateStaRec(struct ADAPTER *prAdapter,
			struct STA_RECORD *prStaRec);

void qmUpdateStaRec(struct ADAPTER *prAdapter,
		    struct STA_RECORD *prStaRec);

/*----------------------------------------------------------------------------*/
/* TX-Related Queue Management                                                */
/*----------------------------------------------------------------------------*/

struct MSDU_INFO *qmFlushTxQueues(struct ADAPTER
	*prAdapter);

struct MSDU_INFO *qmFlushStaTxQueues(struct ADAPTER
	*prAdapter, uint32_t u4StaRecIdx);

void qmDetermineTxPacketRate(struct ADAPTER *prAdapter,
			struct MSDU_INFO *prMsduInfo);

struct MSDU_INFO *qmEnqueueTxPackets(struct ADAPTER
	*prAdapter, struct MSDU_INFO *prMsduInfoListHead);

struct MSDU_INFO *qmDequeueTxPackets(struct ADAPTER
	*prAdapter, struct TX_TCQ_STATUS *prTcqStatus);

#if CFG_SUPPORT_MULTITHREAD
struct MSDU_INFO *qmDequeueTxPacketsMthread(
	struct ADAPTER *prAdapter,
	struct TX_TCQ_STATUS *prTcqStatus);

u_int8_t
qmAdjustTcQuotasMthread(struct ADAPTER *prAdapter,
			struct TX_TCQ_ADJUST *prTcqAdjust,
			struct TX_TCQ_STATUS *prTcqStatus);
#endif

u_int8_t qmAdjustTcQuotas(struct ADAPTER *prAdapter,
			  struct TX_TCQ_ADJUST *prTcqAdjust,
			  struct TX_TCQ_STATUS *prTcqStatus);

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
void qmReassignTcResource(struct ADAPTER *prAdapter);

void qmUpdateAverageTxQueLen(struct ADAPTER *prAdapter);

void qmDoAdaptiveTcResourceCtrl(struct ADAPTER
				*prAdapter);

void qmCheckForFastTcResourceCtrl(struct ADAPTER
				  *prAdapter, uint8_t ucTc);

#endif

void qmDetermineStaRecIndex(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo);

uint32_t qmDequeueTxPacketsFromPerStaQueues(struct ADAPTER
	*prAdapter, struct QUE *prQue, uint8_t ucTC,
	uint32_t
	u4CurrentQuota,
	uint32_t
	*prPleCurrentQuota, uint32_t u4TotalQuota);

void qmDequeueTxPacketsFromPerTypeQueues(struct ADAPTER
	*prAdapter, struct QUE *prQue, uint8_t ucTC,
	uint32_t
	u4CurrentQuota,
	uint32_t
	*prPleCurrentQuota, uint32_t u4TotalQuota);

uint32_t qmDequeueTxPacketsFromGlobalQueue(struct ADAPTER
	*prAdapter, struct QUE *prQue, uint8_t ucTC,
	uint32_t
	u4CurrentQuota,
	uint32_t
	*prPleCurrentQuota, uint32_t u4TotalQuota);

#if CFG_SUPPORT_NAN
void qmUpdateFreeNANQouta(struct ADAPTER *prAdapter,
			  struct EVENT_UPDATE_NAN_TX_STATUS *prTxStatus);
#endif

#if CFG_QUEUE_RX_IF_CONN_NOT_READY
void qmSetStaRecRxAllowed(struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec, u_int8_t fgIsRxAllowed);
#endif /* CFG_QUEUE_RX_IF_CONN_NOT_READY */

void qmSetStaRecTxAllowed(struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec, u_int8_t fgIsTxAllowed);

uint32_t gmGetDequeueQuota(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct BSS_INFO *prBssInfo,
	uint32_t u4TotalQuota
);

/*----------------------------------------------------------------------------*/
/* RX-Related Queue Management                                                */
/*----------------------------------------------------------------------------*/

void qmInitRxQueues(struct ADAPTER *prAdapter);

struct SW_RFB *qmFlushRxQueues(struct ADAPTER
			       *prAdapter);

struct QUE *qmDetermineStaTxQueue(struct ADAPTER
				  *prAdapter, struct MSDU_INFO *prMsduInfo,
				  uint8_t ucActiveTs, uint8_t *pucTC);

void qmSetTxPacketDescTemplate(struct ADAPTER *prAdapter,
			       struct MSDU_INFO *prMsduInfo);

struct SW_RFB *qmHandleRxPackets(struct ADAPTER
				 *prAdapter, struct SW_RFB *prSwRfbListHead);

void qmProcessPktWithReordering(struct ADAPTER
				*prAdapter, struct SW_RFB *prSwRfb,
				struct QUE *prReturnedQue);

void qmProcessBarFrame(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb, struct QUE *prReturnedQue);

void qmHandleRxReorderWinShift(struct ADAPTER *prAdapter,
	uint8_t ucStaRecIdx, uint8_t ucTid, uint16_t u2SSN,
	struct QUE *prReturnedQue);

void qmInsertReorderPkt(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb,
			struct RX_BA_ENTRY *prReorderQueParm,
			struct QUE *prReturnedQue);

void qmInsertFallWithinReorderPkt(struct ADAPTER
				  *prAdapter, struct SW_RFB *prSwRfb,
				  struct RX_BA_ENTRY *prReorderQueParm,
				  struct QUE *prReturnedQue);

void qmInsertFallAheadReorderPkt(struct ADAPTER
				 *prAdapter, struct SW_RFB *prSwRfb,
				 struct RX_BA_ENTRY *prReorderQueParm,
				 struct QUE *prReturnedQue);

void qmPopOutReorderPkt(struct ADAPTER *prAdapter,
	struct RX_BA_ENTRY *prReorderQueParm,
	struct SW_RFB *prSwRfb, struct QUE *prReturnedQue,
	enum ENUM_RX_STATISTIC_COUNTER eRxCounter);

void qmPopOutDueToFallWithin(struct ADAPTER *prAdapter,
			     struct RX_BA_ENTRY *prReorderQueParm,
			     struct QUE *prReturnedQue);

void qmPopOutDueToFallAhead(struct ADAPTER *prAdapter,
			    struct RX_BA_ENTRY *prReorderQueParm,
			    struct QUE *prReturnedQue);

void qmHandleReorderBubbleTimeout(struct ADAPTER *prAdapter,
			uintptr_t ulParamPtr);

void qmHandleEventCheckReorderBubble(struct ADAPTER *prAdapter,
				     struct RX_BA_ENTRY *prReorderQueParm);

void qmHandleMailboxRxMessage(struct MAILBOX_MSG prMailboxRxMsg);

void qmHandleEventTxAddBa(struct ADAPTER *prAdapter,
			  struct WIFI_EVENT *prEvent);

void qmHandleEventRxAddBa(struct ADAPTER *prAdapter,
			  struct WIFI_EVENT *prEvent);

void qmHandleEventRxDelBa(struct ADAPTER *prAdapter,
			  struct WIFI_EVENT *prEvent);

struct RX_BA_ENTRY *qmLookupRxBaEntry(struct ADAPTER
	*prAdapter, uint8_t ucStaRecIdx, uint8_t ucTid);

u_int8_t
qmAddRxBaEntry(struct ADAPTER *prAdapter,
	       uint8_t ucStaRecIdx, uint8_t ucTid,
	       uint16_t u2WinStart, uint16_t
	       u2WinSize);

void qmDelRxBaEntry(struct ADAPTER *prAdapter,
		    uint8_t ucStaRecIdx, uint8_t ucTid,
		    u_int8_t fgFlushToHost);

u_int8_t qmIsIndependentPkt(struct SW_RFB *prSwRfb);

void mqmProcessAssocRsp(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb, const uint8_t *pucIE,
			uint16_t u2IELength);

void mqmProcessBcn(struct ADAPTER *prAdapter,
		   struct SW_RFB *prSwRfb, const uint8_t *pucIE,
		   uint16_t u2IELength);

#if (CFG_SUPPORT_802_11AX == 1)
u_int8_t mqmCompareMUEdcaParameters(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
		struct _IE_MU_EDCA_PARAM_T *prIeMUEdcaParam);

u_int8_t mqmParseMUEdcaParams(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb,
	const uint8_t *pucIE, uint16_t u2IELength, u_int8_t fgForceOverride);
#endif

u_int8_t mqmIsBssEdcaParamsUpdated(struct ADAPTER *prAdapter,
		struct SW_RFB *prSwRfb, const uint8_t *pucIE,
		uint16_t u2IELength, u_int8_t fgForceOverride);

u_int8_t mqmHandleWMMEdcaParams(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
		const uint8_t *pucIE, u_int8_t fgForceOverwrite);

u_int8_t mqmHandle80211EdcaParamSet(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
		const uint8_t *pucIE, u_int8_t fgForceOverwrite);

u_int8_t mqmIsEdcaParamsChanged(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
		uint8_t ucQosInfo, struct WMM_AC_PARAM *arAcParam);

void mqmUpdateEdcaParams(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo,
		struct STA_RECORD *prStaRec, uint8_t ucQosInfo,
		struct WMM_AC_PARAM *arAcParam);

void mqmFillAcQueParam(struct WMM_AC_PARAM *prAcParam,
		struct AC_QUE_PARMS *prAcQueParams);

void mqmProcessScanResult(struct ADAPTER *prAdapter,
			  struct BSS_DESC *prScanResult,
			  struct STA_RECORD *prStaRec);

uint32_t mqmFillWmmInfoIE(uint8_t *pucOutBuf,
	u_int8_t fgSupportUAPSD, uint8_t ucBmpDeliveryAC,
	uint8_t ucBmpTriggerAC, uint8_t ucUapsdSp);

uint32_t mqmGenerateWmmInfoIEByStaRec(struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
	uint8_t *
	pucOutBuf);

void mqmGenerateWmmInfoIE(struct ADAPTER *prAdapter,
			  struct MSDU_INFO *prMsduInfo);

void mqmGenerateWmmParamIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

#if CFG_SUPPORT_TDLS

uint32_t mqmGenerateWmmParamIEByParam(struct ADAPTER
	*prAdapter, struct BSS_INFO *prBssInfo, uint8_t *pOutBuf);
#endif

enum ENUM_FRAME_ACTION qmGetFrameAction(struct ADAPTER
	*prAdapter,
	uint8_t ucBssIndex, uint8_t ucStaRecIdx,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_FRAME_TYPE_IN_CMD_Q eFrameType,
	uint16_t u2FrameLength);

void qmHandleEventBssAbsencePresence(struct ADAPTER
				     *prAdapter, struct WIFI_EVENT *prEvent);

#if (CFG_ABSENCE_TIMEOUT_DETECTION == 1)
void qmDetectAbnormalBssAbsence(struct ADAPTER *ad);
#endif /* CFG_ABSENCE_TIMEOUT_DETECTION */

#if CFG_ENABLE_WIFI_DIRECT
void qmHandleEventStaChangePsMode(struct ADAPTER
				  *prAdapter, struct WIFI_EVENT *prEvent);

void mqmProcessAssocReq(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb, uint8_t *pucIE,
			uint16_t u2IELength);

void qmHandleEventStaUpdateFreeQuota(struct ADAPTER
				     *prAdapter, struct WIFI_EVENT *prEvent);

void
qmUpdateFreeQuota(struct ADAPTER *prAdapter,
		  struct STA_RECORD *prStaRec, uint8_t ucUpdateMode,
		  uint8_t ucFreeQuota);
#endif
void qmFreeAllByBssIdx(struct ADAPTER *prAdapter,
		       uint8_t ucBssIndex);

uint32_t qmGetRxReorderQueuedBufferCount(struct ADAPTER
		*prAdapter);

uint32_t qmDumpQueueStatus(struct ADAPTER *prAdapter,
			   uint8_t *pucBuf, uint32_t u4MaxLen);

void addReorderQueParm(struct QUE *prRxBaEntry,
		struct RX_BA_ENTRY *prReorderQueParm,
		struct ADAPTER *prAdapter,
		enum ENUM_SPIN_LOCK_CATEGORY_E lock);

struct RX_BA_ENTRY *getReorderQueParm(struct QUE *prRxBaEntry,
		struct ADAPTER *prAdapter,
		enum ENUM_SPIN_LOCK_CATEGORY_E lock);

void qmFlushTimeoutReorderBubble(struct ADAPTER *prAdapter,
		struct RX_BA_ENTRY *prReorderQueParm);

void qmFlushDeletedBaReorder(struct ADAPTER *prAdapter,
		struct RX_BA_ENTRY *prReorderQueParm);

void qmResetTcControlResource(struct ADAPTER *prAdapter);
void qmAdjustTcQuotaPle(struct ADAPTER *prAdapter,
			struct TX_TCQ_ADJUST *prTcqAdjust,
			struct TX_TCQ_STATUS *prTcqStatus);
struct DHCP_PROTOCOL *qmGetDhcpPkt(uint8_t *pucData, uint16_t u2PacketLen,
	u_int8_t fgFromServer, uint16_t *pDhcpLen);

#if defined(CFG_SUPPORT_REPLAY_DETECTION) || \
	defined(CFG_SUPPORT_FRAG_AGG_VALIDATION)
#define CCMPTSCPNNUM	6
u_int8_t qmRxPNtoU64(uint8_t *pucPN, uint8_t uPNNum,
	uint64_t *pu64Rets);
#endif

#ifdef CFG_SUPPORT_REPLAY_DETECTION
u_int8_t qmHandleRxReplay(struct ADAPTER *prAdapter,
			  struct SW_RFB *prSwRfb);
#endif

#if CFG_SUPPORT_FRAG_AGG_VALIDATION
u_int8_t qmDetectRxInvalidEAPOL(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);
#endif /* CFG_SUPPORT_FRAG_AGG_VALIDATION */

#if CFG_SUPPORT_FRAG_AGG_VALIDATION
u_int8_t qmAmsduValidation(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);
#endif /* CFG_SUPPORT_FRAG_AGG_VALIDATION */

u_int8_t
qmIsNoDropPacket(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb);

void qmMoveStaTxQueue(struct ADAPTER *prAdapter,
	struct STA_RECORD *prSrcStaRec, struct STA_RECORD *prDstStaRec);
void qmHandleDelTspec(struct ADAPTER *prAdapter, struct STA_RECORD *prStaRec,
		      enum ENUM_ACI eAci);
void qmReleaseCHAtFinishedDhcp(struct ADAPTER *prAdapter,
			       uint8_t ucBssIndex);
void qmCheckRxEAPOLM3(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb, uint8_t ucBssIndex);
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if QM_TEST_MODE
extern struct QUE_MGT g_rQM;
#endif
extern const uint8_t aucTid2ACI[TX_DESC_TID_NUM];
extern const uint8_t aucACI2TxQIdx[WMM_AC_INDEX_NUM];
extern const uint8_t arNetwork2TcResource[WMM_AC_INDEX_NUM +
		1][NET_TC_NUM];

#endif /* _QUE_MGT_H */

u_int8_t qmIsStaInPS(struct ADAPTER *prAdapter, struct STA_RECORD *prStaRec);
void qmSetStaPS(struct ADAPTER *prAdapter, struct STA_RECORD *prStaRec,
	u_int8_t fgIsInPS);
