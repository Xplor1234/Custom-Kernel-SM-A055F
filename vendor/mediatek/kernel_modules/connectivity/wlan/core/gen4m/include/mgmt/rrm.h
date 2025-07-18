/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _RRM_H
#define _RRM_H

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

/* Radio Measurement Request Mode definition */
#define RM_REQ_MODE_PARALLEL_BIT                    BIT(0)
#define RM_REQ_MODE_ENABLE_BIT                      BIT(1)
#define RM_REQ_MODE_REQUEST_BIT                     BIT(2)
#define RM_REQ_MODE_REPORT_BIT                      BIT(3)
#define RM_REQ_MODE_DURATION_MANDATORY_BIT          BIT(4)
#define RM_REP_MODE_LATE                            BIT(0)
#define RM_REP_MODE_INCAPABLE                       BIT(1)
#define RM_REP_MODE_REFUSED                         BIT(2)

/* Radio Measurement Report Frame Max Length */
#define RM_REPORT_FRAME_MAX_LENGTH	(1600 - NIC_TX_DESC_AND_PADDING_LENGTH)
/* beacon request mode definition */
#define RM_BCN_REQ_PASSIVE_MODE                     0
#define RM_BCN_REQ_ACTIVE_MODE                      1
#define RM_BCN_REQ_TABLE_MODE                       2
#define RM_BCN_REQ_MODE_MAX                         3

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

enum RM_STATE {
	RM_NO_REQUEST,
	RM_ON_GOING,
	RM_WAITING, /* waiting for rrm scan */
};

enum RM_REQ_PRIORITY {
	RM_PRI_BROADCAST,
	RM_PRI_MULTICAST,
	RM_PRI_UNICAST
};

/* Beacon RM related parameters */
struct BCN_RM_PARAMS {
	enum RM_STATE eState;
	uint8_t token;
	uint8_t lastIndication;
	u8 ssid[ELEM_MAX_LEN_SSID];
	size_t ssidLen;
	enum BEACON_REPORT_DETAIL reportDetail;
	uint8_t *reportIeIds;
	uint8_t reportIeIdsLen;
	uint8_t *reportExtIeIds;
	uint8_t reportExtIeIdsLen;
	uint8_t apChannels[256];
	uint8_t apChannelsLen;
};

/* Channel Load RM related parameters */
struct CHNL_LOAD_RM_PARAMS {
	enum RM_STATE eState;
	uint8_t token;
	uint8_t reportingCondition;
	uint8_t chnlLoadRefValue;
	uint16_t minDwellTime;
};

struct STA_STATS_RM_PARAMS {
	enum RM_STATE eState;
	uint8_t token;
	uint32_t u4OriStatsData[13];
};

struct RM_BEACON_REPORT_PARAMS {
	uint8_t ucChannel;
	uint8_t ucRCPI;
	uint8_t ucRSNI;
	uint8_t ucAntennaID;
	uint8_t ucFrameInfo;
	uint8_t aucBcnFixedField[12];
};

struct RM_MEASURE_REPORT_ENTRY {
	struct LINK_ENTRY rLinkEntry;
	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint16_t u2MeasReportLen;
	uint8_t *pucMeasReport;
};

struct RADIO_MEASUREMENT_REQ_PARAMS {
	/* Remain Request Elements Length, started at prMeasElem. if it is 0,
	 * means RM is done
	 */
	uint16_t u2RemainReqLen;
	uint16_t u2ReqIeBufLen;
	struct IE_MEASUREMENT_REQ *prCurrMeasElem;
	OS_SYSTIME rStartTime;
	uint16_t u2Repetitions;
	uint8_t *pucReqIeBuf;
	enum RM_REQ_PRIORITY ePriority;
	u_int8_t fgRmIsOngoing;
	OS_SYSTIME rScanStartTime;

	struct BCN_RM_PARAMS rBcnRmParam;
	struct CHNL_LOAD_RM_PARAMS rChnlLoadRmParam;
	struct STA_STATS_RM_PARAMS rStaStatsRmParam;
};

struct RADIO_MEASUREMENT_REPORT_PARAMS {
	/* bssidx to response */
	uint8_t ucRspBssIndex;
	/* the total length of Measurement Report elements */
	uint16_t u2ReportFrameLen;
	uint8_t *pucReportFrameBuff;
	/* Variables to collect report */
	struct LINK rReportLink; /* a link to save received report entry */
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
void rrmParamInit(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void rrmParamUninit(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void rrmProcessNeighborReportResonse(struct ADAPTER *prAdapter,
				     struct WLAN_ACTION_FRAME *prAction,
				     struct SW_RFB *prSwRfb);

void rrmTxNeighborReportRequest(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				struct SUB_ELEMENT_LIST *prSubIEs);

void rrmGenerateRRMEnabledCapIE(struct ADAPTER *prAdapter,
				struct MSDU_INFO *prMsduInfo);

void rrmProcessRadioMeasurementRequest(struct ADAPTER *prAdapter,
				       struct SW_RFB *prSwRfb);

void rrmProcessLinkMeasurementRequest(struct ADAPTER *prAdapter,
				      struct WLAN_ACTION_FRAME *prAction);

void rrmFillRrmCapa(uint8_t *pucCapa);

void rrmStartNextMeasurement(struct ADAPTER *prAdapter, u_int8_t fgNewStarted,
	uint8_t ucBssIndex);


u_int8_t rrmFillScanMsg(struct ADAPTER *prAdapter,
			struct MSG_SCN_SCAN_REQ_V2 *prMsg);

void rrmDoBeaconMeasurement(struct ADAPTER *prAdapter, uintptr_t ulParam);

void rrmDoChnlLoadMeasurement(struct ADAPTER *prAdapter, uintptr_t ulParam);

void rrmDoStaStatsMeasurement(struct ADAPTER *prAdapter, uintptr_t ulParam);

void rrmTxNeighborReportRequest(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				struct SUB_ELEMENT_LIST *prSubIEs);

void rrmTxRadioMeasurementReport(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void rrmFreeMeasurementResources(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

enum RM_REQ_PRIORITY rrmGetRmRequestPriority(uint8_t *pucDestAddr);

void rrmRunEventProcessNextRm(struct ADAPTER *prAdapter,
			      struct MSG_HDR *prMsgHdr);

void rrmScheduleNextRm(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void rrmUpdateBssTimeTsf(struct ADAPTER *prAdapter, struct BSS_DESC *prBssDesc);

#if (CFG_SUPPORT_WIFI_6G == 1)
uint8_t rrmCheckIs6GOpClass(uint8_t ucOpClass);
#endif

void rrmCollectBeaconReport(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, uint8_t ucBssIndex);

void rrmCollectChannelLoadReport(struct ADAPTER *prAdapter,
	uint8_t ucChnlUtil, uint8_t ucBssIndex);

void rrmCollectStaStatsReport(struct ADAPTER *prAdapter, uintptr_t ulParam);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _RRM_H */
