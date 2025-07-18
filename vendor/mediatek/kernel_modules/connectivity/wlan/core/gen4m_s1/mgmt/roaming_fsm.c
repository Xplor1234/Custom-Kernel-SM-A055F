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
 ** Id:
 */

/*! \file   "roaming_fsm.c"
 *    \brief  This file defines the FSM for Roaming MODULE.
 *
 *    This file defines the FSM for Roaming MODULE.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#if CFG_SUPPORT_ROAMING
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static uint8_t *apucDebugRoamingState[ROAMING_STATE_NUM] = {
	(uint8_t *) DISP_STRING("IDLE"),
	(uint8_t *) DISP_STRING("DECISION"),
	(uint8_t *) DISP_STRING("DISCOVERY"),
	(uint8_t *) DISP_STRING("ROAM")
};

static uint8_t apucRoamingReasonToLog[ROAMING_REASON_NUM] = {
	1, /* Low RSSI     - map to ROAMING_REASON_POOR_RCPI(0) */
	0, /* Unspecific   - map to ROAMING_REASON_TX_ERR(1) */
	0, /* Unspecific   - map to ROAMING_REASON_RETRY(2) */
	6, /* Idle roaming - map to ROAMING_REASON_IDLE(3) */
	2, /* High CU      - map to ROAMING_REASON_HIGH_CU(4)*/
	3, /* Beacon lost  - map to ROAMING_REASON_BEACON_TIMEOUT(5) */
	3, /* Beacon lost  - map to ROAMING_REASON_BEACON_TIMEOUT_TX_ERR(6) */
	0, /* Unspecific   - map to ROAMING_REASON_INACTIVE(7) */
	4, /* Unspecific   - map to ROAMING_REASON_SAA_FAIL(8) */
	0, /* Unspecific   - map to ROAMING_REASON_UPPER_LAYER_TRIGGER(9) */
	5, /* Unspecific   - map to ROAMING_REASON_BTM(10) */
	0, /* Unspecific   - map to ROAMING_REASON_REASSOC(11) */
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialize the value in ROAMING_FSM_INFO_T for ROAMING FSM operation
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmInit(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	DBGLOG(ROAMING, LOUD,
	       "[%d]->roamingFsmInit(): Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);
	prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);

	/* 4 <1> Initiate FSM */
	prRoamingFsmInfo->fgIsEnableRoaming =
		prConnSettings->fgIsEnableRoaming;
	prRoamingFsmInfo->eCurrentState = ROAMING_STATE_IDLE;
	prRoamingFsmInfo->rRoamingDiscoveryUpdateTime = 0;
	prRoamingFsmInfo->fgDrvRoamingAllow = TRUE;
#if (CFG_TC10_FEATURE == 1)
	LINK_INITIALIZE(&prRoamingFsmInfo->rCandidateApList);
	LINK_INITIALIZE(&prRoamingFsmInfo->rRoamingHistory);
	prRoamingFsmInfo->fgIsGBandCoex = FALSE;
#endif
}				/* end of roamingFsmInit() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Uninitialize the value in AIS_FSM_INFO_T for AIS FSM operation
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmUninit(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;

	DBGLOG(ROAMING, LOUD,
	       "[%d]->roamingFsmUninit(): Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);

	prRoamingFsmInfo->eCurrentState = ROAMING_STATE_IDLE;
#if (CFG_TC10_FEATURE == 1)
	LINK_INITIALIZE(&prRoamingFsmInfo->rCandidateApList);
	roamingClearHistory(prRoamingFsmInfo);
#endif
} /* end of roamingFsmUninit() */

#if (CFG_TC10_FEATURE == 1)
struct CONNECTED_BSS *roamingGetBss(struct ROAMING_INFO *prRoamingFsmInfo,
	struct BSS_DESC *prTarget)
{
	struct LINK *history = &prRoamingFsmInfo->rRoamingHistory;
	struct CONNECTED_BSS *bss;

	if (!prTarget)
		return NULL;
	LINK_FOR_EACH_ENTRY(bss, history, rLinkEntry, struct CONNECTED_BSS)
	{
		if (EQUAL_MAC_ADDR(bss->aucBssid, prTarget->aucBSSID))
			return bss;
	}
	return NULL;
}

uint8_t roamingIsBssInHistory(struct ROAMING_INFO *prRoamingFsmInfo,
	struct BSS_DESC *prTarget)
{
	struct LINK *history = &prRoamingFsmInfo->rRoamingHistory;
	struct CONNECTED_BSS *bss;

	LINK_FOR_EACH_ENTRY(bss, history, rLinkEntry, struct CONNECTED_BSS)
	{
		if (EQUAL_MAC_ADDR(bss->aucBssid, prTarget->aucBSSID))
			return TRUE;
	}
	return FALSE;
}

void roamingAddBssToHistory(struct ROAMING_INFO *prRoamingFsmInfo,
	struct BSS_DESC *prTarget) {
	struct LINK *history = &prRoamingFsmInfo->rRoamingHistory;
	struct CONNECTED_BSS *bss;

	if (!prTarget || roamingIsBssInHistory(prRoamingFsmInfo, prTarget))
		return;

	bss = kalMemAlloc(sizeof(struct CONNECTED_BSS), VIR_MEM_TYPE);
	if (!bss) {
		DBGLOG(ROAMING, WARN, "no resource for " MACSTR "\n",
			MAC2STR(prTarget->aucBSSID));
		return;
	}
	kalMemZero(bss, sizeof(struct CONNECTED_BSS));

	COPY_MAC_ADDR(bss->aucBssid, prTarget->aucBSSID);
	LINK_INSERT_TAIL(history, &bss->rLinkEntry);
}

void roamingClearHistory(struct ROAMING_INFO *prRoamingFsmInfo) {
	struct LINK *history = &prRoamingFsmInfo->rRoamingHistory;
	struct CONNECTED_BSS *bss;

	while (!LINK_IS_EMPTY(history)) {
		LINK_REMOVE_HEAD(history, bss, struct CONNECTED_BSS *);
		kalMemFree(bss, VIR_MEM_TYPE, sizeof(struct CONNECTED_BSS));
	}
	LINK_INITIALIZE(&prRoamingFsmInfo->rRoamingHistory);
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief Send commands to firmware
 *
 * @param [IN P_ADAPTER_T]       prAdapter
 *        [IN P_ROAMING_PARAM_T] prParam
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmSendCmd(IN struct ADAPTER *prAdapter,
	IN struct CMD_ROAMING_TRANSIT *prTransit)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	uint32_t rStatus;
	uint8_t ucBssIndex = prTransit->ucBssidx;

	DBGLOG(ROAMING, LOUD,
	       "[%d]->roamingFsmSendCmd(): Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_ROAMING_TRANSIT,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(struct CMD_ROAMING_TRANSIT),
				      /* u4SetQueryInfoLen */
				      (uint8_t *)prTransit, /* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
				     );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */
}				/* end of roamingFsmSendCmd() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Update the recent time when ScanDone occurred
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmScanResultsUpdate(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	DBGLOG(ROAMING, LOUD,
		"[%d]->roamingFsmScanResultsUpdate(): Current Time = %d\n",
		ucBssIndex, kalGetTimeTick());

	/* try driver roaming */
	if (scanCheckNeedDriverRoaming(prAdapter, ucBssIndex)) {
		struct ROAMING_INFO *roam;

		DBGLOG(ROAMING, INFO, "Request driver roaming");
		roam = aisGetRoamingInfo(prAdapter, ucBssIndex);
		roam->eReason = ROAMING_REASON_INACTIVE;
		aisFsmRemoveRoamingRequest(prAdapter, ucBssIndex);
		aisFsmInsertRequest(prAdapter,
			AIS_REQUEST_ROAMING_CONNECT, ucBssIndex);
	}
}				/* end of roamingFsmScanResultsUpdate() */

/*----------------------------------------------------------------------------*/
/*
 * @brief Check if need to do scan for roaming
 *
 * @param[out] fgIsNeedScan Set to TRUE if need to scan since
 *		there is roaming candidate in current scan result
 *		or skip roaming times > limit times
 * @return
 */
/*----------------------------------------------------------------------------*/
static u_int8_t roamingFsmIsNeedScan(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct AIS_SPECIFIC_BSS_INFO *asbi = NULL;
	struct LINK *prEssLink = NULL;
	u_int8_t fgIsNeedScan = TRUE;

	asbi = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	if (asbi == NULL) {
		DBGLOG(ROAMING, WARN, "ais specific bss info is NULL\n");
		return TRUE;
	}

	prEssLink = &asbi->rCurEssLink;

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	/*
	 * Start skip roaming scan mechanism if only one ESSID AP
	 */
	if (prEssLink->u4NumElem == 1) {
		struct BSS_DESC *prBssDesc;

		/* Get current BssDesc */
		prBssDesc = aisGetTargetBssDesc(prAdapter, ucBssIndex);
		if (prBssDesc) {
			DBGLOG(ROAMING, INFO,
				"roamingFsmSteps: RCPI:%d RoamSkipTimes:%d\n",
				prBssDesc->ucRCPI, asbi->ucRoamSkipTimes);
			if (prBssDesc->ucRCPI > 90) {
				/* Set parameters related to Good Area */
				asbi->ucRoamSkipTimes = 3;
				asbi->fgGoodRcpiArea = TRUE;
				asbi->fgPoorRcpiArea = FALSE;
			} else {
				if (asbi->fgGoodRcpiArea) {
					asbi->ucRoamSkipTimes--;
				} else if (prBssDesc->ucRCPI > 67) {
					/*Set parameters related to Poor Area*/
					if (!asbi->fgPoorRcpiArea) {
						asbi->ucRoamSkipTimes = 2;
						asbi->fgPoorRcpiArea = TRUE;
						asbi->fgGoodRcpiArea = FALSE;
					} else {
						asbi->ucRoamSkipTimes--;
					}
				} else {
					asbi->fgPoorRcpiArea = FALSE;
					asbi->fgGoodRcpiArea = FALSE;
					asbi->ucRoamSkipTimes--;
				}
			}

			if (asbi->ucRoamSkipTimes == 0) {
				asbi->ucRoamSkipTimes = 3;
				asbi->fgPoorRcpiArea = FALSE;
				asbi->fgGoodRcpiArea = FALSE;
				DBGLOG(ROAMING, INFO, "Need Scan\n");
			} else {
				struct CMD_ROAMING_SKIP_ONE_AP cmd = {0};

				cmd.fgIsRoamingSkipOneAP = 1;

				wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_ROAMING_SKIP,
				    TRUE,
				    FALSE,
				    FALSE, NULL, NULL,
				    sizeof(struct CMD_ROAMING_SKIP_ONE_AP),
				    (uint8_t *)&cmd, NULL, 0);

				fgIsNeedScan = FALSE;
			}
		} else {
			DBGLOG(ROAMING, WARN, "Target BssDesc is NULL\n");
		}
	}
#endif

	if (cnmP2pIsActive(prAdapter))
		fgIsNeedScan = FALSE;

	return fgIsNeedScan;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief The Core FSM engine of ROAMING for AIS Infra.
 *
 * @param [IN P_ADAPTER_T]          prAdapter
 *        [IN ENUM_ROAMING_STATE_T] eNextState Enum value of next AIS STATE
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void roamingFsmSteps(IN struct ADAPTER *prAdapter,
	IN enum ENUM_ROAMING_STATE eNextState,
	IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE ePreviousState;
	u_int8_t fgIsTransition = (u_int8_t) FALSE;
	u_int32_t u4ScnResultsTimeout = prAdapter->rWifiVar.u4DiscoverTimeout;

	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	do {

		/* Do entering Next State */
		DBGLOG(ROAMING, STATE,
		       "[ROAMING%d] TRANSITION: [%s] -> [%s]\n",
		       ucBssIndex,
		       apucDebugRoamingState[prRoamingFsmInfo->eCurrentState],
		       apucDebugRoamingState[eNextState]);

		/* NOTE(Kevin): This is the only place to
		 *    change the eCurrentState(except initial)
		 */
		ePreviousState = prRoamingFsmInfo->eCurrentState;
		prRoamingFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (u_int8_t) FALSE;

		/* Do tasks of the State that we just entered */
		switch (prRoamingFsmInfo->eCurrentState) {
		/* NOTE(Kevin): we don't have to rearrange the sequence of
		 *   following switch case. Instead I would like to use a common
		 *   lookup table of array of function pointer
		 *   to speed up state search.
		 */
		case ROAMING_STATE_IDLE:
			break;
		case ROAMING_STATE_DECISION:
#if CFG_SUPPORT_DRIVER_ROAMING
			GET_CURRENT_SYSTIME(
				&prRoamingFsmInfo->rRoamingLastDecisionTime);
#endif
			prRoamingFsmInfo->eReason = ROAMING_REASON_POOR_RCPI;
			break;

		case ROAMING_STATE_DISCOVERY: {
			OS_SYSTIME rCurrentTime;
			u_int8_t fgIsNeedScan = FALSE;

#if CFG_SUPPORT_NCHO
			if (prAdapter->rNchoInfo.fgNCHOEnabled == TRUE)
				u4ScnResultsTimeout = 0;
#endif

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
			      prRoamingFsmInfo->rRoamingDiscoveryUpdateTime,
			      SEC_TO_SYSTIME(u4ScnResultsTimeout))) {
				DBGLOG(ROAMING, LOUD,
					"roamingFsmSteps: DiscoveryUpdateTime Timeout\n");

				fgIsNeedScan = roamingFsmIsNeedScan(prAdapter,
								ucBssIndex);
			}
			aisFsmRunEventRoamingDiscovery(
				prAdapter, fgIsNeedScan, ucBssIndex);
		}
		break;
		case ROAMING_STATE_ROAM:
			break;

		default:
			ASSERT(0); /* Make sure we have handle all STATEs */
		}
	} while (fgIsTransition);

	return;

}				/* end of roamingFsmSteps() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Decision state after join completion
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventStart(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct BSS_INFO *prAisBssInfo;
	struct CMD_ROAMING_TRANSIT rTransit;
#if (CFG_TC10_FEATURE == 1)
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#endif

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);
	kalMemZero(&rTransit, sizeof(struct CMD_ROAMING_TRANSIT));

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	prAisBssInfo = aisGetAisBssInfo(prAdapter,
		ucBssIndex);
	if (prAisBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;

	DBGLOG(ROAMING, EVENT,
	       "[%d] EVENT-ROAMING START: Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as DECISION, DISCOVERY -> DECISION */
	if (!(prRoamingFsmInfo->eCurrentState == ROAMING_STATE_IDLE
	      || prRoamingFsmInfo->eCurrentState == ROAMING_STATE_ROAM))
		return;

	eNextState = ROAMING_STATE_DECISION;
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_START;
		rTransit.u2Data = prAisBssInfo->ucBssIndex;
		rTransit.ucBssidx = ucBssIndex;
		roamingFsmSendCmd(prAdapter,
			(struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState, ucBssIndex);
	}

#if (CFG_TC10_FEATURE == 1)
	/* Dump configurations */
#define TEMP_LOG_TEMPLATE \
	"[Roam][Common]MinRoamDelta:%u Delta:%u [Idle]Delta:%u " \
	"[BeaconLost/Emergency]MinRssi:%d [BTM]Delta:%u " \
	"[AP Scoring]RssiWeight:%u CUWeight:%u " \
	"Band1-Rssi-Factor-Val-Score(1/2/3/4):%d/%d/%d/%d-%u/%u/%u/%u " \
	"Band2-Rssi-Factor-Val-Score(1/2/3/4):%d/%d/%d/%d-%u/%u/%u/%u " \
	"Band1-CU-Factor-Val-Score(1/2):%d/%d-%u/%u " \
	"Band2-CU-Factor-Val-Score(1/2):%d/%d-%u/%u " \

	DBGLOG(ROAMING, EVENT, TEMP_LOG_TEMPLATE,
	       prWifiVar->ucRCMinRoamDelta, prWifiVar->ucRCDelta,
	       prWifiVar->ucRIDelta, prWifiVar->cRBMinRssi,
	       prWifiVar->ucRBTMDelta, prWifiVar->ucRssiWeight,
	       prWifiVar->ucCUWeight, prWifiVar->cB1RssiFactorVal1,
	       prWifiVar->cB1RssiFactorVal2, prWifiVar->cB1RssiFactorVal3,
	       prWifiVar->cB1RssiFactorVal4, prWifiVar->ucB1RssiFactorScore1,
	       prWifiVar->ucB1RssiFactorScore2, prWifiVar->ucB1RssiFactorScore3,
	       prWifiVar->ucB1RssiFactorScore4, prWifiVar->cB2RssiFactorVal1,
	       prWifiVar->cB2RssiFactorVal2, prWifiVar->cB2RssiFactorVal3,
	       prWifiVar->cB2RssiFactorVal4, prWifiVar->ucB2RssiFactorScore1,
	       prWifiVar->ucB2RssiFactorScore2, prWifiVar->ucB2RssiFactorScore3,
	       prWifiVar->ucB2RssiFactorScore4, prWifiVar->ucB1CUFactorVal1,
	       prWifiVar->ucB1CUFactorVal2, prWifiVar->ucB1CUFactorScore1,
	       prWifiVar->ucB1CUFactorScore2, prWifiVar->ucB2CUFactorVal1,
	       prWifiVar->ucB2CUFactorVal2, prWifiVar->ucB2CUFactorScore1,
	       prWifiVar->ucB2CUFactorScore2);

#undef TEMP_LOG_TEMPLATE
#endif
}				/* end of roamingFsmRunEventStart() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Discovery state when deciding to find a candidate
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventDiscovery(IN struct ADAPTER *prAdapter,
	IN struct CMD_ROAMING_TRANSIT *prTransit)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	uint8_t ucBssIndex = prTransit->ucBssidx;

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT,
	       "[%d] EVENT-ROAMING DISCOVERY: Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	/* DECISION -> DISCOVERY */
	/* Errors as IDLE, DISCOVERY, ROAM -> DISCOVERY */
	if (prRoamingFsmInfo->eCurrentState !=
	    ROAMING_STATE_DECISION)
		return;

	eNextState = ROAMING_STATE_DISCOVERY;
	/* DECISION -> DISCOVERY */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		struct BSS_INFO *prAisBssInfo;
		struct BSS_DESC *prBssDesc;
		struct BSS_DESC *prBssDescTarget;
		uint8_t arBssid[PARAM_MAC_ADDR_LEN];
		struct PARAM_SSID rSsid;
		struct AIS_FSM_INFO *prAisFsmInfo;
		struct CONNECTION_SETTINGS *prConnSettings;

		kalMemZero(&rSsid, sizeof(struct PARAM_SSID));
		prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
		prConnSettings =
			aisGetConnSettings(prAdapter, ucBssIndex);

		/* sync. rcpi with firmware */
		prAisBssInfo =
			&(prAdapter->rWifiVar.arBssInfoPool[NETWORK_TYPE_AIS]);
		prBssDesc = prAisFsmInfo->prTargetBssDesc;
		if (prBssDesc) {
			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
				  prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
			COPY_MAC_ADDR(arBssid, prBssDesc->aucBSSID);
		} else  {
			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
				  prConnSettings->aucSSID,
				  prConnSettings->ucSSIDLen);
			COPY_MAC_ADDR(arBssid, prConnSettings->aucBSSID);
		}

		prRoamingFsmInfo->ucRcpi = (uint8_t)(prTransit->u2Data & 0xff);
		prRoamingFsmInfo->ucThreshold =	prTransit->u2RcpiLowThreshold;

		prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
				arBssid, TRUE, &rSsid);
		if (prBssDesc) {
			prBssDesc->ucRCPI = prRoamingFsmInfo->ucRcpi;
			DBGLOG(ROAMING, INFO, "RCPI %u(%d)\n",
			     prBssDesc->ucRCPI, RCPI_TO_dBm(prBssDesc->ucRCPI));
		}

		prBssDescTarget = aisGetTargetBssDesc(prAdapter, ucBssIndex);
		if (prBssDescTarget && prBssDescTarget != prBssDesc) {
			prBssDescTarget->ucRCPI = prRoamingFsmInfo->ucRcpi;
			DBGLOG(ROAMING, WARN, "update target bss\n");
		}

		/* Save roaming reason code and PER value for AP selection */
		prRoamingFsmInfo->eReason = prTransit->eReason;
		if (prTransit->eReason == ROAMING_REASON_TX_ERR) {
			prRoamingFsmInfo->ucPER =
				(prTransit->u2Data >> 8) & 0xff;
			DBGLOG(ROAMING, INFO, "ucPER %u\n",
				prRoamingFsmInfo->ucPER);
		} else {
			prRoamingFsmInfo->ucPER = 0;
		}

#if CFG_SUPPORT_NCHO
		if (prRoamingFsmInfo->eReason == ROAMING_REASON_RETRY)
			DBGLOG(ROAMING, INFO,
				"NCHO enable=%d,trigger=%d,delta=%d,period=%d\n",
				prAdapter->rNchoInfo.fgNCHOEnabled,
				prAdapter->rNchoInfo.i4RoamTrigger,
				prAdapter->rNchoInfo.i4RoamDelta,
				prAdapter->rNchoInfo.u4RoamScanPeriod);
#endif

		roamingFsmSteps(prAdapter, eNextState, ucBssIndex);
	}
}				/* end of roamingFsmRunEventDiscovery() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Roam state after Scan Done
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventRoam(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);
	kalMemZero(&rTransit, sizeof(struct CMD_ROAMING_TRANSIT));

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT,
	       "[%d] EVENT-ROAMING ROAM: Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as IDLE, DECISION, ROAM -> ROAM */
	if (prRoamingFsmInfo->eCurrentState !=
	    ROAMING_STATE_DISCOVERY)
		return;

	eNextState = ROAMING_STATE_ROAM;
	/* DISCOVERY -> ROAM */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_ROAM;
		rTransit.ucBssidx = ucBssIndex;
		roamingFsmSendCmd(prAdapter,
			(struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState, ucBssIndex);
	}
}				/* end of roamingFsmRunEventRoam() */

void roamingFsmNotifyEvent(
	IN struct ADAPTER *adapter, IN uint8_t bssIndex, IN uint8_t ucFail,
	IN struct BSS_DESC *prBssDesc)
{
	struct ROAMING_INFO *roam = aisGetRoamingInfo(adapter, bssIndex);
	struct ROAMING_EVENT_INFO *prEventInfo = &roam->rEventInfo;
	struct BSS_INFO *prAisBssInfo = aisGetAisBssInfo(adapter, bssIndex);
	char uevent[300];

	COPY_MAC_ADDR(roam->rEventInfo.aucPrevBssid, prAisBssInfo->aucBSSID);
	COPY_MAC_ADDR(roam->rEventInfo.aucCurrBssid, prBssDesc->aucBSSID);
	roam->rEventInfo.ucPrevChannel = prAisBssInfo->ucPrimaryChannel;
	roam->rEventInfo.ucCurrChannel = prBssDesc->ucChannelNum;
	roam->rEventInfo.ucBw = (uint8_t) prBssDesc->eBand;
	roam->rEventInfo.u2ApLoading = prBssDesc->u2StaCnt;
	roam->rEventInfo.ucSupportStbc = prBssDesc->fgMultiAnttenaAndSTBC;
	roam->rEventInfo.ucSupportStbc = prBssDesc->fgMultiAnttenaAndSTBC;
	roam->rEventInfo.ucPrevRcpi =
		dBm_TO_RCPI(adapter->rLinkQuality.rLq[bssIndex].cRssi);
	roam->rEventInfo.ucCurrRcpi = prBssDesc->ucRCPI;

	kalSnprintf(uevent, sizeof(uevent),
		"roam=Status:%s,BSSID:" MACSTR "/" MACSTR
		",Reason:%d,Chann:%d/%d,RCPI:%d/%d,BW:%d,STBC:%s\n",
		(ucFail == TRUE ? "FAIL" : "SUCCESS"),
		MAC2STR(prEventInfo->aucPrevBssid),
		MAC2STR(prEventInfo->aucCurrBssid), (uint8_t) roam->eReason,
		prEventInfo->ucPrevChannel, prEventInfo->ucCurrChannel,
		prEventInfo->ucPrevRcpi, prEventInfo->ucCurrRcpi,
		prEventInfo->ucBw,
		(prEventInfo->ucSupportStbc == TRUE ? "TRUE" : " FALSE"));
	kalSendUevent(uevent);
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Decision state as being failed to find out any candidate
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventFail(IN struct ADAPTER *prAdapter,
	IN uint8_t ucReason, IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);
	kalMemZero(&rTransit, sizeof(struct CMD_ROAMING_TRANSIT));

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, STATE,
	       "[%d] EVENT-ROAMING FAIL: reason %x Current Time = %d\n",
	       ucBssIndex,
	       ucReason, kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as IDLE, DECISION, DISCOVERY -> DECISION */
	if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_ROAM)
		return;

	eNextState = ROAMING_STATE_DECISION;
	/* ROAM -> DECISION */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_FAIL;
		rTransit.u2Data = (uint16_t) (ucReason & 0xffff);
		rTransit.ucBssidx = ucBssIndex;
		roamingFsmSendCmd(prAdapter,
			(struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState, ucBssIndex);
	}
}				/* end of roamingFsmRunEventFail() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Transit to Idle state as beging aborted by other moduels, AIS
 *
 * @param [IN P_ADAPTER_T] prAdapter
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void roamingFsmRunEventAbort(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_ROAMING_STATE eNextState;
	struct CMD_ROAMING_TRANSIT rTransit;

	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);
	kalMemZero(&rTransit, sizeof(struct CMD_ROAMING_TRANSIT));

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT,
	       "[%d] EVENT-ROAMING ABORT: Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	eNextState = ROAMING_STATE_IDLE;
	/* IDLE, DECISION, DISCOVERY, ROAM -> IDLE */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rTransit.u2Event = ROAMING_EVENT_ABORT;
		rTransit.ucBssidx = ucBssIndex;
		roamingFsmSendCmd(prAdapter,
			(struct CMD_ROAMING_TRANSIT *) &rTransit);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState, ucBssIndex);
	}
}				/* end of roamingFsmRunEventAbort() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process events from firmware
 *
 * @param [IN P_ADAPTER_T]       prAdapter
 *        [IN P_ROAMING_PARAM_T] prParam
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
uint32_t roamingFsmProcessEvent(IN struct ADAPTER *prAdapter,
	IN struct CMD_ROAMING_TRANSIT *prTransit)
{
	uint8_t ucBssIndex = prTransit->ucBssidx;

	if (ucBssIndex >= MAX_BSSID_NUM) {
		DBGLOG(ROAMING, ERROR, "ucBssIndex [%d] out of range!\n",
			ucBssIndex);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(ROAMING, LOUD,
	       "[%d] ROAMING Process Events: Current Time = %d\n",
	       ucBssIndex,
	       kalGetTimeTick());

	if (prTransit->u2Event == ROAMING_EVENT_DISCOVERY) {
		DBGLOG(ROAMING, INFO,
			"ROAMING_EVENT_DISCOVERY Data[%d] RCPI[%d(%d)] PER[%d] Thr[%d(%d)] Reason[%d] Time[%ld]\n",
			prTransit->u2Data,
			(prTransit->u2Data) & 0xff,      /* L[8], RCPI */
			RCPI_TO_dBm((prTransit->u2Data) & 0xff),
			(prTransit->u2Data >> 8) & 0xff, /* H[8], PER */
			prTransit->u2RcpiLowThreshold,
			RCPI_TO_dBm(prTransit->u2RcpiLowThreshold),
			prTransit->eReason,
			prTransit->u4RoamingTriggerTime);
		roamingFsmRunEventDiscovery(prAdapter, prTransit);
	} else if (prTransit->u2Event == ROAMING_EVENT_THRESHOLD_UPDATE) {
		DBGLOG(ROAMING, INFO,
			"ROAMING_EVENT_THRESHOLD_UPDATE RCPI H[%d(%d)] L[%d(%d)]\n",
			prTransit->u2RcpiHighThreshold,
			RCPI_TO_dBm(prTransit->u2RcpiHighThreshold),
			prTransit->u2RcpiLowThreshold,
			RCPI_TO_dBm(prTransit->u2RcpiLowThreshold));
	}

	return WLAN_STATUS_SUCCESS;
}

uint8_t roamingFsmInDecision(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct ROAMING_INFO *roam;
	enum ENUM_PARAM_CONNECTION_POLICY policy;
	struct CONNECTION_SETTINGS *setting;

	roam = aisGetRoamingInfo(prAdapter, ucBssIndex);
	setting = aisGetConnSettings(prAdapter, ucBssIndex);
	policy = setting->eConnectionPolicy;

	return IS_BSS_INDEX_AIS(prAdapter, ucBssIndex) &&
	       roam->fgIsEnableRoaming &&
	       roam->eCurrentState == ROAMING_STATE_DECISION &&
	       policy != CONNECT_BY_BSSID ?
	       TRUE : FALSE;
}

uint8_t roamingFsmIsDiscovering(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo = NULL;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	uint8_t fgIsDiscovering = FALSE;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);

	fgIsDiscovering =
		(prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED &&
		(prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY ||
		prRoamingFsmInfo->eCurrentState == ROAMING_STATE_ROAM)) ||
		aisFsmIsInProcessPostpone(prAdapter, ucBssIndex);

	return fgIsDiscovering;
}

void roamingFsmLogScanStart(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex, IN uint8_t fgIsFullScn,
	IN struct BSS_DESC *prBssDesc)
{
	struct ROAMING_INFO *prRoamInfo;
	uint32_t u4CannelUtilization = 0;
	uint8_t ucIsValidCu = FALSE;
	char aucLog[256] = {0};
	uint8_t ucReason;

	prRoamInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	ucIsValidCu = (prBssDesc && prBssDesc->fgExistBssLoadIE);
	if (ucIsValidCu)
		u4CannelUtilization = prBssDesc->ucChnlUtilization * 100 / 255;

	ucReason = (uint8_t) prRoamInfo->eReason;
	if (ucReason >= ROAMING_REASON_NUM)
		return;

	kalSprintf(aucLog,
		"[ROAM] SCAN_START reason=%d rssi=%d cu=%d full_scan=%d rssi_thres=%d",
		apucRoamingReasonToLog[ucReason],
		RCPI_TO_dBm(prRoamInfo->ucRcpi),
		ucIsValidCu ? u4CannelUtilization : -1,
		fgIsFullScn, RCPI_TO_dBm(prRoamInfo->ucThreshold));

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogScanDone(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamInfo;
	struct SCAN_INFO *prScanInfo;
	struct LINK *prCurEssLink;
	char aucLog[256] = {0};
	char aucScanChannel[200] = {0};
	uint8_t ucIdx = 0, ucPos = 0, ucAvailableLen = 195, ucMaxLen = 195;

	prRoamInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCurEssLink = &(aisGetAisSpecBssInfo(
			prAdapter, ucBssIndex)->rCurEssLink);

	DBGLOG(ROAMING, INFO, "Start to log scan done(%d)",
			prScanInfo->ucSparseChannelArrayValidNum);
	for (ucIdx = 0; (ucIdx < prScanInfo->ucSparseChannelArrayValidNum &&
		ucAvailableLen > 0); ucIdx++) {
		ucPos += kalSnprintf(aucScanChannel + ucPos, ucMaxLen - ucPos,
			"%d ", KHZ_TO_MHZ(nicChannelNum2Freq(
				prScanInfo->aucChannelNum[ucIdx],
				prScanInfo->aeChannelBand[ucIdx])));

		ucAvailableLen = (ucMaxLen > ucPos) ? (ucMaxLen - ucPos) : 0;
		if (!ucAvailableLen)
			aucScanChannel[199] = '\0';
	}

	kalSprintf(aucLog,
		"[ROAM] SCAN_DONE ap_count=%d freq[%d]=%s",
		prCurEssLink->u4NumElem,
		prScanInfo->ucSparseChannelArrayValidNum,
		aucScanChannel);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogSocre(IN struct ADAPTER *prAdapter, uint8_t *prefix,
	IN uint8_t ucBssIndex, struct BSS_DESC *prBssDesc, uint32_t u4Score,
	uint32_t u4Tput)
{
	char aucLog[256] = {0};
	char aucTput[24] = {0};

	if (!prBssDesc)
		return;

	if (u4Tput)
		kalSprintf(aucTput, " tp=%dkbps", u4Tput / 1000);
	kalSprintf(aucLog,
		"[ROAM] %s bssid=" RPTMACSTR
		" freq=%d rssi=%d cu=%d score=%d.%d%s",
		prefix, RPTMAC2STR(prBssDesc->aucBSSID),
		KHZ_TO_MHZ(nicChannelNum2Freq(prBssDesc->ucChannelNum,
			prBssDesc->eBand)), RCPI_TO_dBm(prBssDesc->ucRCPI),
		(prBssDesc->fgExistBssLoadIE ?
			(prBssDesc->ucChnlUtilization * 100 / 255) : -1),
			u4Score / 100, u4Score % 100, aucTput);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogResult(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex, struct BSS_DESC *prSelectedBssDesc)
{
	char aucLog[256] = {0};
	uint8_t fgIsRoam =
		(prSelectedBssDesc && !prSelectedBssDesc->fgIsConnected);
	struct BSS_DESC *prBssDesc = (fgIsRoam ? prSelectedBssDesc :
		aisGetTargetBssDesc(prAdapter, ucBssIndex));

	kalSprintf(aucLog, "[ROAM] RESULT %s bssid=" RPTMACSTR,
		fgIsRoam ? "ROAM" : "NO_ROAM",
		RPTMAC2STR(prBssDesc->aucBSSID));

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogCancel(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex, uint8_t *pucReason)
{
	char aucLog[256] = {0};

	kalSprintf(aucLog, "[ROAM] CANCELED [%s]", pucReason);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}
#endif
