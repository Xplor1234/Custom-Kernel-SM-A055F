/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _P2P_H
#define _P2P_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/* refer to 'Config Methods' in WPS */
#define WPS_CONFIG_USBA                 0x0001
#define WPS_CONFIG_ETHERNET             0x0002
#define WPS_CONFIG_LABEL                0x0004
#define WPS_CONFIG_DISPLAY              0x0008
#define WPS_CONFIG_EXT_NFC              0x0010
#define WPS_CONFIG_INT_NFC              0x0020
#define WPS_CONFIG_NFC                  0x0040
#define WPS_CONFIG_PBC                  0x0080
#define WPS_CONFIG_KEYPAD               0x0100

/* refer to 'Device Password ID' in WPS */
#define WPS_DEV_PASSWORD_ID_PIN         0x0000
#define WPS_DEV_PASSWORD_ID_USER        0x0001
#define WPS_DEV_PASSWORD_ID_MACHINE     0x0002
#define WPS_DEV_PASSWORD_ID_REKEY       0x0003
#define WPS_DEV_PASSWORD_ID_PUSHBUTTON  0x0004
#define WPS_DEV_PASSWORD_ID_REGISTRAR   0x0005

#define P2P_DEVICE_TYPE_NUM         2
#define P2P_DEVICE_NAME_LENGTH      32
#define P2P_NETWORK_NUM             8
#define P2P_MEMBER_NUM              8

/* Device Capability Definition. */
#define P2P_MAXIMUM_NOA_COUNT                       8

#define P2P_MAX_AKM_SUITES 3

#define P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE 51	/* Contains 6 sub-band. */

/* Memory Size Definition. */
#define P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE           768
#define WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE           300

#define P2P_WILDCARD_SSID           "DIRECT-"

/* Device Charactoristic. */
/* 1000 is too short , the deauth would block in the queue */
#define SAP_CHNL_HOLD_TIME_MS			200
#define P2P_CHNL_HOLD_TIME_MS			3000
#define P2P_AP_CHNL_HOLD_TIME_CSA_MS		100
#define P2P_GC_JOIN_CH_GRANT_THRESHOLD		10
#define P2P_GC_JOIN_CH_REQUEST_INTERVAL		4000
#define P2P_DEFAULT_LISTEN_CHANNEL                   1
#define P2P_EXTEND_ROLE_REQ_CHNL_TIME_DIFF_MS 2000

#if (CFG_SUPPORT_DFS_MASTER == 1)
#define P2P_AP_CAC_WEATHER_CHNL_HOLD_TIME_MS (600*1000)
#define P2P_AP_CAC_MIN_CAC_TIME_MS (60*1000)
#define P2P_AP_CAC_TIMER_MARGIN (2000)
#endif

#define P2P_DEAUTH_TIMEOUT_TIME_MS 1000

#define P2P_SAA_RETRY_COUNT     3

#define AP_NONINDOOR_CHANNEL_5G   149
#define AP_DEFAULT_CHANNEL_2G     6
#define AP_DEFAULT_CHANNEL_5G     36
#define AP_DEFAULT_CHANNEL_5GL    36
#define AP_DEFAULT_CHANNEL_5GH     149
#if (CFG_SUPPORT_WIFI_6G == 1)
#define AP_DEFAULT_CHANNEL_6G     5
#define AP_DEFAULT_CHANNEL_6G_1     97
#define AP_DEFAULT_CHANNEL_6G_2     37
#endif
#define P2P_5G_L_H_ISOLATION_WIDTH 160
#define P2P_5G_6G_ISOLATION_WIDTH  190
#define P2P_5G_L_LOWER_BOUND       5170
#define P2P_5G_L_UPPER_BOUND       5330
#define P2P_5G_H_LOWER_BOUND       5490
#define P2P_5G_H_UPPER_BOUND       5895
#define P2P_6G_LOWER_BOUND         5945
#define P2P_6G_UPPER_BOUND         6425
#define AP_A_BAND_CHANNEL_INTERVAL     4

#if (CFG_TX_MGMT_BY_DATA_Q == 1)
#define DEFAULT_P2P_PROBERESP_RETRY_LIMIT (2)
#else
#define DEFAULT_P2P_PROBERESP_RETRY_LIMIT (2)
#endif

#define DEFAULT_P2P_PROBERESP_LIFE_TIME 500

#define DEFAULT_P2P_CSA_TIMEOUT_MS	7000
#define P2P_MAX_AID_VALUE	2007

#define DEFAULT_MAX_CHANNEL_SWITCH_TIME_TU	1000
#define P2P_MAX_PROBE_RESP_LEN			768

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

#define p2pChangeMediaState(_prAdapter, _prP2pBssInfo, _eNewMediaState) \
	(_prP2pBssInfo->eConnectionState = (_eNewMediaState))

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */
/* if driver need wait for a longger time when do p2p connection */
enum ENUM_P2P_CONNECT_STATE {
	P2P_CNN_NORMAL = 0,
	P2P_CNN_GO_NEG_REQ,
	P2P_CNN_GO_NEG_RESP,
	P2P_CNN_GO_NEG_CONF,
	P2P_CNN_INVITATION_REQ,
	P2P_CNN_INVITATION_RESP,
	P2P_CNN_DEV_DISC_REQ,
	P2P_CNN_DEV_DISC_RESP,
	P2P_CNN_PROV_DISC_REQ,
	P2P_CNN_PROV_DISC_RESP
};

struct P2P_DEV_INFO {
	uint32_t u4DeviceNum;
	enum ENUM_P2P_CONNECT_STATE eConnState;
	struct EVENT_P2P_DEV_DISCOVER_RESULT
		arP2pDiscoverResult[CFG_MAX_NUM_BSS_LIST];
	uint8_t *pucCurrIePtr;
	/* A common pool for IE of all scan results. */
	uint8_t aucCommIePool[CFG_MAX_COMMON_IE_BUF_LEN];
	uint8_t ucExtendChanFlag;
	struct MSDU_INFO *prWaitTxDoneMsdu;
};

enum ENUM_P2P_PEER_TYPE {
	ENUM_P2P_PEER_GROUP,
	ENUM_P2P_PEER_DEVICE,
	ENUM_P2P_PEER_NUM
};

struct P2P_DEVICE_INFO {
	uint8_t aucDevAddr[PARAM_MAC_ADDR_LEN];
	uint8_t aucIfAddr[PARAM_MAC_ADDR_LEN];
	uint8_t ucDevCapabilityBitmap;
	int32_t i4ConfigMethod;
	uint8_t aucPrimaryDeviceType[8];
	uint8_t aucSecondaryDeviceType[8];
	uint8_t aucDeviceName[P2P_DEVICE_NAME_LENGTH];
};

struct P2P_GROUP_INFO {
	struct PARAM_SSID rGroupID;
	struct P2P_DEVICE_INFO rGroupOwnerInfo;
	uint8_t ucMemberNum;
	struct P2P_DEVICE_INFO arMemberInfo[P2P_MEMBER_NUM];
};

struct P2P_NETWORK_INFO {
	enum ENUM_P2P_PEER_TYPE eNodeType;

	union {
		struct P2P_GROUP_INFO rGroupInfo;
		struct P2P_DEVICE_INFO rDeviceInfo;
	} node;
};

struct P2P_NETWORK_LIST {
	uint8_t ucNetworkNum;
	struct P2P_NETWORK_INFO rP2PNetworkInfo[P2P_NETWORK_NUM];
};

struct P2P_DISCONNECT_INFO {
	uint8_t ucRole;
	uint8_t ucRsv[3];
};

struct P2P_SSID_STRUCT {
	uint8_t aucSsid[32];
	uint8_t ucSsidLen;
};

enum ENUM_SCAN_REASON {
	SCAN_REASON_UNKNOWN = 0,
	SCAN_REASON_CONNECT,
	SCAN_REASON_STARTAP,
	SCAN_REASON_ACS,
	SCAN_REASON_NUM,
};

struct P2P_SCAN_REQ_INFO {
	enum ENUM_SCAN_TYPE eScanType;
	enum ENUM_SCAN_CHANNEL eChannelSet;
	uint16_t u2PassiveDewellTime;
	uint8_t ucSeqNumOfScnMsg;
	u_int8_t fgIsAbort;
	u_int8_t fgIsScanRequest;
	uint8_t ucNumChannelList;
	struct RF_CHANNEL_INFO
		arScanChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	uint32_t u4BufLength;
	uint8_t aucIEBuf[MAX_IE_LENGTH];
	uint8_t ucSsidNum;
	uint8_t aucBSSID[MAC_ADDR_LEN];
	enum ENUM_SCAN_REASON eScanReason;
	/* Currently we can only take one SSID scan request */
	struct P2P_SSID_STRUCT arSsidStruct[CFG_SCAN_SSID_MAX_NUM];
};

enum P2P_CHANNEL_SWITCH_POLICY {
	P2P_CHANNEL_SWITCH_POLICY_SCC,
	P2P_CHANNEL_SWITCH_POLICY_SKIP_DFS,
	P2P_CHANNEL_SWITCH_POLICY_SKIP_DFS_USER,
	P2P_CHANNEL_SWITCH_POLICY_NONE,
};

enum P2P_CONCURRENCY_POLICY {
	P2P_CONCURRENCY_POLICY_REMOVE,
	P2P_CONCURRENCY_POLICY_KEEP,
	P2P_CONCURRENCY_POLICY_REMOVE_IF_STA_MLO,
};

enum P2P_AUTH_POLICY {
	P2P_AUTH_POLICY_NONE = 0,
	P2P_AUTH_POLICY_RESET = 1,
	P2P_AUTH_POLICY_IGNORE = 2,
};

enum P2P_VENDOR_ACS_HW_MODE {
	P2P_VENDOR_ACS_HW_MODE_11B,
	P2P_VENDOR_ACS_HW_MODE_11G,
	P2P_VENDOR_ACS_HW_MODE_11A,
	P2P_VENDOR_ACS_HW_MODE_11AD,
	P2P_VENDOR_ACS_HW_MODE_11ANY
};

enum P2P_FOBIDDEN_REGION_TYPE {
	P2P_FOBIDDEN_REGION_ISOLATION = 0,
	P2P_FOBIDDEN_REGION_ALIASING = 1,
	P2P_FOBIDDEN_REGION_NUM = 2
};

/**
 * @ucVhtSeg0: VHT mode Segment0 center channel
 *	The value is the index of the channel center frequency for
 *	20 MHz, 40 MHz, and 80 MHz channels. The value is the center
 *	frequency index of the primary 80 MHz segment for 160 MHz and
 *	80+80 MHz channels.
 * @ucVhtSeg1: VHT mode Segment1 center channel
 *	The value is zero for 20 MHz, 40 MHz, and 80 MHz channels. The
 *	value is the index of the channel center frequency for 160 MHz
 *	channels and the center frequency index of the secondary 80 MHz
 *	segment for 80+80 MHz channels.
 */
struct P2P_ACS_REQ_INFO {
	uint8_t ucRoleIdx;
	int8_t icLinkId;
	u_int8_t fgIsProcessing;
	u_int8_t fgIsHtEnable;
	u_int8_t fgIsHt40Enable;
	u_int8_t fgIsVhtEnable;
	u_int8_t fgIsEhtEnable;
	enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw;
	enum P2P_VENDOR_ACS_HW_MODE eHwMode;
	u_int8_t fgIsAis;

	/* output only */
	enum ENUM_BAND eBand;
	uint8_t ucPrimaryCh;
	uint8_t ucSecondCh;
	uint8_t ucVhtSeg0;
	uint8_t ucVhtSeg1;
	uint16_t u2PunctBitmap;
	uint32_t au4SafeChnl[ENUM_SAFE_CH_MASK_MAX_NUM];
	uint32_t au4ValidChnl[ENUM_SAFE_CH_MASK_MAX_NUM];
};

struct P2P_CHNL_REQ_INFO {
	struct LINK rP2pChnlReqLink;
	u_int8_t fgIsChannelRequested;
	uint8_t ucSeqNumOfChReq;
	uint64_t u8Cookie;
	uint8_t ucReqChnlNum;
	enum ENUM_BAND eBand;
	enum ENUM_CHNL_EXT eChnlSco;
	uint8_t ucOriChnlNum;
	enum ENUM_CHANNEL_WIDTH eChannelWidth;	/*VHT operation ie */
	uint8_t ucCenterFreqS1;
	uint8_t ucCenterFreqS2;
#if (CFG_SUPPORT_SAP_PUNCTURE == 1)
	uint16_t u2PunctBitmap;
#endif /* CFG_SUPPORT_SAP_PUNCTURE */
	enum ENUM_BAND eOriBand;
	enum ENUM_CHNL_EXT eOriChnlSco;
	uint32_t u4MaxInterval;
	enum ENUM_CH_REQ_TYPE eChnlReqType;
#if CFG_SUPPORT_NFC_BEAM_PLUS
	uint32_t NFC_BEAM;	/*NFC Beam + Indication */
#endif
	uint8_t ucChReqNum;
};

struct P2P_CSA_REQ_INFO {
	uint8_t ucBssIdx;
};

/* Glubal Connection Settings. */
struct P2P_CONNECTION_SETTINGS {
	/*UINT_8 ucRfChannelListSize;*/
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE
	/*UINT_8 aucChannelEntriesField[P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE];*/
#endif

	u_int8_t fgIsApMode;
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	u_int8_t fgIsWPSMode;
#endif
};

struct NOA_TIMING {
	u_int8_t fgIsInUse;	/* Indicate if this entry is in use or not */
	uint8_t ucCount;		/* Count */

	uint8_t aucReserved[2];

	uint32_t u4Duration;	/* Duration */
	uint32_t u4Interval;	/* Interval */
	uint32_t u4StartTime;	/* Start Time */
};

struct P2P_FILS_DISCOVERY_INFO {
	u_int8_t fgValid;
	uint32_t u4MinInterval;
	uint32_t u4MaxInterval;
	uint32_t u4Length;
	uint8_t aucIEBuf[MAX_IE_LENGTH];
};

struct P2P_UNSOL_PROBE_RESP_INFO {
	u_int8_t fgValid;
	uint32_t u4Interval;
	uint32_t u4Length;
	uint8_t aucIEBuf[MAX_IE_LENGTH];
};

struct P2P_SPECIFIC_BSS_INFO {
	/* For GO(AP) Mode - Compose TIM IE */
	/*UINT_16 u2SmallestAID;*//* TH3 multiple P2P */
	/*UINT_16 u2LargestAID;*//* TH3 multiple P2P */
	/*UINT_8 ucBitmapCtrl;*//* TH3 multiple P2P */
	/* UINT_8 aucPartialVirtualBitmap[MAX_LEN_TIM_PARTIAL_BMP]; */

	/* For GC/GO OppPS */
	u_int8_t fgEnableOppPS;
	uint16_t u2CTWindow;

	/* For GC/GO NOA */
	uint8_t ucNoAIndex;
	uint8_t ucNoATimingCount;	/* Number of NoA Timing */
	struct NOA_TIMING arNoATiming[P2P_MAXIMUM_NOA_COUNT];

	u_int8_t fgIsNoaAttrExisted;

	/* For P2P Device */
	/* TH3 multiple P2P */	/* Regulatory Class for channel. */
	/*UINT_8 ucRegClass;*/
	/* Linten Channel only on channels 1, 6 and 11 in the 2.4 GHz. */
	/*UINT_8 ucListenChannel;*//* TH3 multiple P2P */

	/* Operating Channel, should be one of channel */
	/* list in p2p connection settings. */
	uint8_t ucPreferredChannel;
	enum ENUM_CHNL_EXT eRfSco;
	enum ENUM_BAND eRfBand;

	/* Extended Listen Timing. */
	uint16_t u2AvailabilityPeriod;
	uint16_t u2AvailabilityInterval;

	uint16_t u2AttributeLen;
	uint8_t aucAttributesCache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	/*UINT_16 u2WscAttributeLen;*//* TH3 multiple P2P */
	/* TH3 multiple P2P */
	/*UINT_8 aucWscAttributesCache[WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];*/

	/*UINT_8 aucGroupID[MAC_ADDR_LEN];*//* TH3 multiple P2P */
	uint16_t u2GroupSsidLen;
	uint8_t aucGroupSsid[ELEM_MAX_LEN_SSID];

	struct PARAM_CUSTOM_NOA_PARAM_STRUCT rNoaParam;
	struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT rOppPsParam;

	uint32_t u4KeyMgtSuiteCount;
	uint32_t au4KeyMgtSuite[P2P_MAX_AKM_SUITES];

	uint16_t u2WpaIeLen;
	uint8_t aucWpaIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_WPA];

	uint16_t u2RsnIeLen;
	uint8_t aucRsnIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RSN];

	uint16_t u2RsnxIeLen;
	uint8_t aucRsnxIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RSN];

#if (CFG_SUPPORT_RSNO == 1)
	uint16_t u2RsnoIeLen;
	uint8_t aucRsnoIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RSN + 4];

	uint16_t u2Rsno2IeLen;
	uint8_t aucRsno2IeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RSN + 4];

	uint16_t u2RsnxoIeLen;
	uint8_t aucRsnxoIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RSN + 4];
#endif /* CFG_SUPPORT_RSNO */

	uint16_t u2OweIeLen;
	uint8_t aucOweIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_WPA];

	uint16_t u2TpeIeLen;
	uint8_t aucTpeIeBuffer[MAX_TPE_IE_LENGTH];

	uint16_t u2RnrIeLen;
	uint8_t aucRnrIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RNR];

	u_int8_t fgIsRddOpchng;
	struct WIFI_EVENT *prRddPostOpchng;
	u_int8_t ucRddBw;
	u_int8_t ucRddCh;
#if CFG_SUPPORT_P2P_ECSA
	u_int8_t fgEcsa;
	u_int8_t ucEcsaBw;
#endif
	struct P2P_FILS_DISCOVERY_INFO rFilsInfo;
	struct P2P_UNSOL_PROBE_RESP_INFO rUnsolProbeInfo;

	/* OWE */
	uint8_t *pucDHIEBuf;
	uint8_t ucDHIELen;

	u_int8_t fgAddPwrConstrIe;

	u_int8_t fgMlIeExist;
	/* For CSA trigger when ch abort */
	u_int8_t fgIsGcEapolDone;

#if (CFG_SUPPORT_SAP_BCN_PROT == 1)
	u_int8_t fgBcnProtEn;
	uint8_t ucBcnKeyIdx;
#endif /* CFG_SUPPORT_SAP_BCN_PROT */

#if (CFG_SUPPORT_SAP_BCN_CRI_UPD == 1)
	u_int8_t fgForceUpdateBpcc;
#endif /* CFG_SUPPORT_SAP_BCN_CRI_UPD */
};

struct P2P_QUEUED_ACTION_FRAME {
	uint8_t ucRoleIdx;
	int32_t u4Freq;
	uint8_t *prHeader;
	uint16_t u2Length;
};

struct P2P_MGMT_TX_REQ_INFO {
	struct LINK rTxReqLink;
	struct MSDU_INFO *prMgmtTxMsdu;
	u_int8_t fgIsWaitRsp;
};

struct P2P_LINK_INFO {
	struct BSS_INFO *prP2pBss;
	struct BSS_DESC *prP2pTargetBssDesc;
	struct STA_RECORD *prP2pTargetStaRec;
};

struct P2P_LISTEN_OFFLOAD_INFO {
	uint8_t ucBssIndex;
	uint32_t u4DevId;
	uint32_t u4flags;
	uint32_t u4Freq;
	uint32_t u4Period;
	uint32_t u4Interval;
	uint32_t u4Count;
	uint8_t aucDevice[MAX_UEVENT_LEN];
	uint32_t u2DevLen;
	uint8_t aucIE[MAX_IE_LENGTH];
	uint16_t u2IELen;
};

struct P2P_CH_SWITCH_CANDIDATE {
	uint8_t ucBssIndex;
	enum ENUM_BAND eRfBand;
	enum ENUM_MBMC_BN eHwBand;
	uint8_t ucChUpperBound;
	uint8_t ucChLowerBound;
};

struct P2P_HW_BAND_UNIT {
	uint8_t ucBssIndex;
	enum ENUM_BAND eRfBand;
	uint8_t ucCh;
};

struct P2P_CH_BW_RANGE {
	enum ENUM_BAND eRfBand;
	uint8_t ucCh;
	uint8_t ucBwBitmap;
	u_int8_t fgIsDfsSupport;
	uint32_t u4CenterFreq[MAX_BW_NUM];
	uint32_t u4UpperBound[MAX_BW_NUM];
	uint32_t u4LowerBound[MAX_BW_NUM];
};

struct P2P_HW_BAND_GROUP {
	enum ENUM_MBMC_BN eHwBand;
	uint8_t ucUnitNum;
	struct P2P_HW_BAND_UNIT
		arP2pHwBandUnit[MAX_BSSID_NUM];
	u_int8_t fgIsMcc;
};

struct P2P_CH_SWITCH_INTERFACE {
	struct P2P_CH_SWITCH_CANDIDATE *prP2pChInterface;
	uint8_t *ucInterfaceLen;
};

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

#endif	/*_P2P_H */
