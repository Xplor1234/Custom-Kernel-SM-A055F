/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "tdls.h"
 *    \brief This file contains the internal used in TDLS modules
 *	 for MediaTek Inc. 802.11 Wireless LAN Adapters.
 */


#ifndef _TDLS_H
#define _TDLS_H

#if CFG_SUPPORT_TDLS

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#define TDLS_CFG_CMD_TEST

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/* assign station record idx for the packet */
#define TDLSEX_STA_REC_IDX_GET(__prAdapter__, __MsduInfo__)		\
{									\
	struct STA_RECORD *__StaRec__;					\
	__MsduInfo__->ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;		\
	__StaRec__ = cnmGetStaRecByAddress(__prAdapter__,		\
			(uint8_t) NETWORK_TYPE_AIS_INDEX,		\
			__MsduInfo__->aucEthDestAddr);			\
	if ((__StaRec__ != NULL) && (IS_DLS_STA(__StaRec__)))		\
		__MsduInfo__->ucStaRecIndex = __StaRec__->ucIndex;	\
}

/* fill wiphy flag */
#define TDLSEX_WIPHY_FLAGS_INIT(__fgFlag__)				\
{									\
	__fgFlag__ |= (WIPHY_FLAG_SUPPORTS_TDLS |			\
			WIPHY_FLAG_TDLS_EXTERNAL_SETUP);		\
}

#define LR_TDLS_FME_FIELD_FILL(__Len)	\
{					\
	pPkt += __Len;			\
	u4PktLen += __Len;		\
}

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/* Status code */
#define TDLS_STATUS				uint32_t

#define TDLS_STATUS_SUCCESS			WLAN_STATUS_SUCCESS
#define TDLS_STATUS_FAIL			WLAN_STATUS_FAILURE
#define TDLS_STATUS_INVALID_LENGTH		WLAN_STATUS_INVALID_LENGTH
#define TDLS_STATUS_RESOURCES			WLAN_STATUS_RESOURCES
#define TDLS_STATUS_PENDING			WLAN_STATUS_PENDING

#define TDLS_FME_MAC_ADDR_LEN			6
#define TDLS_EX_CAP_PEER_UAPSD			BIT(0)
#define TDLS_EX_CAP_CHAN_SWITCH			BIT(1)
#define TDLS_EX_CAP_TDLS			BIT(2)
#define TDLS_CMD_PEER_UPDATE_EXT_CAP_MAXLEN	5
#define TDLS_CMD_PEER_UPDATE_SUP_RATE_MAX	50
#define TDLS_CMD_PEER_UPDATE_SUP_CHAN_MAX	50
#define TDLS_SEC_BUF_LENGTH			600

#define MAXNUM_TDLS_PEER			4

/* command */
enum TDLS_CMD_ID {
	TDLS_CMD_TEST_TX_FRAME = 0x00,
	TDLS_CMD_TEST_RCV_FRAME,
	TDLS_CMD_TEST_PEER_ADD,
	TDLS_CMD_TEST_PEER_UPDATE,
	TDLS_CMD_TEST_DATA_FRAME,
	TDLS_CMD_TEST_RCV_NULL
};

/* protocol */
#define TDLS_FRM_PROT_TYPE			0x890d

/* payload specific type in the LLC/SNAP header */
#define TDLS_FRM_PAYLOAD_TYPE			2

#define TDLS_FRM_CATEGORY			12

enum TDLS_FRM_ACTION_ID {
	TDLS_FRM_ACTION_SETUP_REQ = 0x00,
	TDLS_FRM_ACTION_SETUP_RSP,
	TDLS_FRM_ACTION_CONFIRM,
	TDLS_FRM_ACTION_TEARDOWN,
	TDLS_FRM_ACTION_PTI,
	TDLS_FRM_ACTION_CHAN_SWITCH_REQ,
	TDLS_FRM_ACTION_CHAN_SWITCH_RSP,
	TDLS_FRM_ACTION_PEER_PSM_REQ,
	TDLS_FRM_ACTION_PEER_PSM_RSP,
	TDLS_FRM_ACTION_PTI_RSP,
	TDLS_FRM_ACTION_DISCOVERY_REQ,
	TDLS_FRM_ACTION_DISCOVERY_RSP = 0x0e,
	TDLS_FRM_ACTION_EVENT_TEAR_DOWN_TO_SUPPLICANT = 0x30
};

/* 7.3.2.62 Link Identifier element */
#define ELEM_ID_LINK_IDENTIFIER						101

__KAL_ATTRIB_PACKED_FRONT__
struct IE_LINK_IDENTIFIER {
	uint8_t ucId;
	uint8_t ucLength;
	uint8_t aBSSID[6];
	uint8_t aInitiator[6];
	uint8_t aResponder[6];
} __KAL_ATTRIB_PACKED__;

#define TDLS_LINK_IDENTIFIER_IE(__ie__)	((struct IE_LINK_IDENTIFIER *)(__ie__))

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

struct STATION_PRARAMETERS {
	const u8 *supported_rates;
	struct net_device *vlan;
	u32 sta_flags_mask, sta_flags_set;
	u32 sta_modify_mask;
	int listen_interval;
	u16 aid;
	u8 supported_rates_len;
	u8 plink_action;
	u8 plink_state;
	u8 uapsd_queues;
	u8 max_sp;
	/* enum nl80211_mesh_power_mode local_pm; */
	u16 capability;
	const u8 *ext_capab;
	u8 ext_capab_len;
};

/* test command use */
struct PARAM_CUSTOM_TDLS_CMD_STRUCT {

	uint8_t ucFmeType;	/* TDLS_FRM_ACTION_ID */

	uint8_t ucToken;
	uint8_t ucCap;

	/* bit0: TDLS, bit1: Peer U-APSD Buffer, bit2: Channel Switching */

	uint8_t ucExCap;

	uint8_t arSupRate[4];
	uint8_t arSupChan[4];

	uint32_t u4Timeout;

	uint8_t arRspAddr[TDLS_FME_MAC_ADDR_LEN];
	uint8_t arBssid[TDLS_FME_MAC_ADDR_LEN];

	/* Linux Kernel-3.10 */

	/* struct */
	struct STATION_PRARAMETERS rPeerInfo;

};

enum ENUM_TDLS_LINK_OPER {
	TDLS_DISCOVERY_REQ,
	TDLS_SETUP,
	TDLS_TEARDOWN,
	TDLS_ENABLE_LINK,
	TDLS_DISABLE_LINK
};

struct TDLS_CMD_LINK_OPER {

	uint8_t aucPeerMac[6];
	enum ENUM_TDLS_LINK_OPER oper;
	uint8_t ucBssIdx;
};

struct TDLS_CMD_LINK_MGT {

	uint8_t aucPeer[6];
	uint8_t ucActionCode;
	uint8_t ucDialogToken;
	uint16_t u2StatusCode;
	uint32_t u4SecBufLen;
	uint8_t aucSecBuf[TDLS_SEC_BUF_LENGTH];
	uint8_t ucBssIdx;
};

struct TDLS_CMD_PEER_ADD {

	uint8_t aucPeerMac[6];
	enum ENUM_STA_TYPE eStaType;
};

struct TDLS_CMD_PEER_UPDATE_HT_CAP_MCS_INFO {
	uint8_t arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
	uint16_t u2RxHighest;
	uint8_t ucTxParams;
	uint8_t Reserved[3];
};

struct TDLS_CMD_PEER_UPDATE_VHT_CAP_MCS_INFO {
	uint8_t arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
};

struct TDLS_CMD_PEER_UPDATE_HT_CAP {
	uint16_t u2CapInfo;
	uint8_t ucAmpduParamsInfo;

	/* 16 bytes MCS information */
	struct TDLS_CMD_PEER_UPDATE_HT_CAP_MCS_INFO rMCS;

	uint16_t u2ExtHtCapInfo;
	uint32_t u4TxBfCapInfo;
	uint8_t ucAntennaSelInfo;
};

struct TDLS_CMD_PEER_UPDATE_VHT_CAP {
	uint16_t u2CapInfo;
	/* 16 bytes MCS information */
	struct TDLS_CMD_PEER_UPDATE_VHT_CAP_MCS_INFO rVMCS;

};

struct TDLS_CMD_PEER_UPDATE {

	uint8_t aucPeerMac[6];

	uint8_t aucSupChan[TDLS_CMD_PEER_UPDATE_SUP_CHAN_MAX];

	uint16_t u2StatusCode;

	uint8_t aucSupRate[TDLS_CMD_PEER_UPDATE_SUP_RATE_MAX];
	uint16_t u2SupRateLen;

	uint8_t UapsdBitmap;
	uint8_t UapsdMaxSp;	/* MAX_SP */

	uint16_t u2Capability;

	uint8_t aucExtCap[TDLS_CMD_PEER_UPDATE_EXT_CAP_MAXLEN];
	uint16_t u2ExtCapLen;

	struct TDLS_CMD_PEER_UPDATE_HT_CAP rHtCap;
	struct TDLS_CMD_PEER_UPDATE_VHT_CAP rVHtCap;

	u_int8_t fgIsSupHt;
	enum ENUM_STA_TYPE eStaType;

};

/* Command to TDLS core module */
enum TDLS_CMD_CORE_ID {
	TDLS_CORE_CMD_TEST_NULL_RCV = 0x00
};

struct TDLS_CMD_CORE_TEST_NULL_RCV {

	uint32_t u4PM;
};

struct TDLS_CMD_CORE {

	uint32_t u4Command;

	uint8_t aucPeerMac[6];

#define TDLS_CMD_CORE_RESERVED_SIZE					50
	union {
		struct TDLS_CMD_CORE_TEST_NULL_RCV rCmdNullRcv;
		uint8_t Reserved[TDLS_CMD_CORE_RESERVED_SIZE];
	} Content;
};

struct TDLS_EVENT {
	uint32_t u4HostId;
	uint32_t u4SubId;
	uint32_t u4StaIdx;
};

enum TDLS_EVENT_HOST_ID {
	TDLS_HOST_EVENT_TEAR_DOWN = 0x00,
	TDLS_HOST_EVENT_TX_DONE
};

enum TDLS_EVENT_HOST_SUBID_TEAR_DOWN {
	TDLS_HOST_EVENT_TD_PTI_TIMEOUT = 0x00,
	TDLS_HOST_EVENT_TD_AGE_TIMEOUT,
	TDLS_HOST_EVENT_TD_PTI_SEND_FAIL,
	TDLS_HOST_EVENT_TD_PTI_SEND_MAX_FAIL,
	TDLS_HOST_EVENT_TD_WRONG_NETWORK_IDX,
	TDLS_HOST_EVENT_TD_NON_STATE3,
	TDLS_HOST_EVENT_TD_LOST_TEAR_DOWN
};

enum TDLS_REASON_CODE {
	TDLS_REASON_CODE_NONE        =  0,

	TDLS_REASON_CODE_UNREACHABLE = 25,
	TDLS_REASON_CODE_UNSPECIFIED = 26,

	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_UNKNOWN = 0x80,          /*128*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_WIFI_OFF = 0x81,         /*129*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_ROAMING = 0x82,          /*130*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_PTI_TIMEOUT = 0x83,      /*131*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_AGE_TIMEOUT = 0x84,      /*132*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_REKEY = 0x85,            /*133*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_PTI_SEND_FAIL = 0x86,    /*134*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_PTI_SEND_MAX_FAIL = 0x87,/*135*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_WRONG_NETWORK_IDX = 0x88,/*136*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_NON_STATE3 = 0x89,       /*137*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_TX_QUOTA_EMPTY = 0x8a,   /*138*/
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_LOST_TEAR_DOWN = 0x8b    /*139*/
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

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
uint32_t TdlsFrameGeneralIeAppend(struct ADAPTER *prAdapter,
				  struct STA_RECORD *prStaRec, uint8_t *pPkt);

uint32_t			/* TDLS_STATUS */

TdlsDataFrameSend_TearDown(struct ADAPTER *prAdapter,
			   struct STA_RECORD *prStaRec,
			   uint8_t *pPeerMac,
			   uint8_t ucActionCode,
			   uint8_t ucDialogToken, uint16_t u2StatusCode,
			   uint8_t *pAppendIe, uint32_t AppendIeLen);

uint32_t			/* TDLS_STATUS */

TdlsDataFrameSend_CONFIRM(struct ADAPTER *prAdapter,
			  struct STA_RECORD *prStaRec,
			  uint8_t *pPeerMac,
			  uint8_t ucActionCode,
			  uint8_t ucDialogToken, uint16_t u2StatusCode,
			  uint8_t *pAppendIe, uint32_t AppendIeLen);

uint32_t			/* TDLS_STATUS */

TdlsDataFrameSend_SETUP_REQ(struct ADAPTER *prAdapter,
			    struct STA_RECORD *prStaRec,
			    uint8_t *pPeerMac,
			    uint8_t ucActionCode,
			    uint8_t ucDialogToken, uint16_t u2StatusCode,
			    uint8_t *pAppendIe, uint32_t AppendIeLen);

uint32_t			/* TDLS_STATUS */

TdlsDataFrameSend_DISCOVERY_REQ(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				uint8_t *pPeerMac,
				uint8_t ucActionCode,
				uint8_t ucDialogToken, uint16_t u2StatusCode,
				uint8_t *pAppendIe, uint32_t AppendIeLen);

uint32_t			/* TDLS_STATUS */

TdlsDataFrameSend_SETUP_RSP(struct ADAPTER *prAdapter,
			    struct STA_RECORD *prStaRec,
			    uint8_t *pPeerMac,
			    uint8_t ucActionCode,
			    uint8_t ucDialogToken, uint16_t u2StatusCode,
			    uint8_t *pAppendIe, uint32_t AppendIeLen);

uint32_t			/* TDLS_STATUS */

TdlsDataFrameSend_DISCOVERY_RSP(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				uint8_t *pPeerMac,
				uint8_t ucActionCode,
				uint8_t ucDialogToken, uint16_t u2StatusCode,
				uint8_t *pAppendIe, uint32_t AppendIeLen);

uint32_t TdlsexLinkOper(struct ADAPTER *prAdapter,
			void *pvSetBuffer, uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen);

uint32_t TdlsexLinkMgt(struct ADAPTER *prAdapter,
		       void *pvSetBuffer, uint32_t u4SetBufferLen,
		       uint32_t *pu4SetInfoLen);

void TdlsexEventHandle(struct GLUE_INFO *prGlueInfo,
		       uint8_t *prInBuf, uint32_t u4InBufLen);

void TdlsEventTearDown(struct GLUE_INFO *prGlueInfo,
		       uint8_t *prInBuf, uint32_t u4InBufLen);

void TdlsBssExtCapParse(struct STA_RECORD *prStaRec,
			uint8_t *pucIE);

uint32_t
TdlsSendChSwControlCmd(struct ADAPTER *prAdapter,
		       void *pvSetBuffer, uint32_t u4SetBufferLen,
		       uint32_t *pu4SetInfoLen);

void TdlsHandleTxDoneStatus(struct ADAPTER *prAdapter,
			struct MSDU_INFO *prMsduInfo,
			enum ENUM_TX_RESULT_CODE rTxDoneStatus);

uint8_t TdlsEnabled(struct ADAPTER *pAd);
uint8_t TdlsValid(struct ADAPTER *pAd, uint8_t bss);
uint8_t TdlsAllowed(struct ADAPTER *pAd, uint8_t bss);
uint8_t TdlsNeedAdjustBw(struct ADAPTER *pAd, uint8_t bss);
uint8_t TdlsAdjustBw(
	struct ADAPTER *pAd,
	struct STA_RECORD *sta,
	uint8_t bss,
	uint8_t bw);
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* CFG_SUPPORT_TDLS */

#endif /* _TDLS_H */
