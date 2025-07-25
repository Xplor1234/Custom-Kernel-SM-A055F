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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/assoc.c#5
 */

/*! \file   "assoc.c"
 *    \brief  This file includes the association-related functions.
 *
 *    This file includes the association-related functions.
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
struct APPEND_VAR_IE_ENTRY txAssocReqIETable[] = {
	{0, assocCalculateConnIELen, assocGenerateConnIE}
	, /* supplicant connect IE including rsn */
#if CFG_SUPPORT_SPEC_MGMT || CFG_SUPPORT_802_11K
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_POWER_CAP),
	 NULL, rlmReqGeneratePowerCapIE}
	,			/* 33 */
#endif
#if CFG_SUPPORT_SPEC_MGMT
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_SUPPORTED_CHANNELS),
	 NULL, rlmReqGenerateSupportedChIE}
	,			/* 36 */
#endif
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP), NULL, rlmReqGenerateHtCapIE}
	,			/* 45 */
#if CFG_SUPPORT_802_11R
	{(ELEM_HDR_LEN + 3), NULL, assocGenerateMDIE}, /* Element ID: 54 */
	{0, rsnCalculateFTIELen, rsnGenerateFTIE}, /* Element ID: 55 */
#endif
#if CFG_SUPPORT_802_11K
	{(ELEM_HDR_LEN + 5), NULL,
	 rrmGenerateRRMEnabledCapIE}, /* Element ID: 70 */
#endif
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP), NULL, rlmReqGenerateExtCapIE}
	,			/* 127 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_WMM_INFO), NULL, mqmGenerateWmmInfoIE}
	,			/* 221 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_RSN + 4), NULL, rsnGenerateRSNIE}
	,			/* 48 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_WPA), NULL, rsnGenerateWPAIE}
	,			/* 221 */
#if CFG_SUPPORT_802_11AC
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_CAP), NULL, rlmReqGenerateVhtCapIE}
	,			/*191 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP_MODE_NOTIFICATION), NULL,
		rlmReqGenerateVhtOpNotificationIE}
	,			/*199 */
#endif
#if (CFG_SUPPORT_802_11AX == 1)
	{(0), heRlmCalculateHeCapIELen, heRlmReqGenerateHeCapIE}
	,			/* 255, EXT 35 */
#if (CFG_SUPPORT_WIFI_6G == 1)
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_HE_6G_CAP), NULL,
	 heRlmReqGenerateHe6gBandCapIE}
	,			/* 255, EXT 59 */
#endif
#endif
#if (CFG_SUPPORT_802_11BE == 1)
	{(0), ehtRlmCalculateCapIELen, ehtRlmReqGenerateCapIE}
	,
#endif
#if CFG_SUPPORT_ASSURANCE
	{0, assocCalculateRoamReasonLen, assocGenerateRoamReason}
	,
#endif
#if CFG_SUPPORT_MTK_SYNERGY
	{(ELEM_HDR_LEN + ELEM_MIN_LEN_MTK_OUI), NULL, rlmGenerateMTKOuiIE}
				/* 221 */
#endif
};

#if CFG_SUPPORT_AAA
struct VERIFY_IE_ENTRY rxAssocReqIETable[] = {
	{ELEM_ID_RESERVED, NULL}	/* 255 */
};

struct APPEND_VAR_IE_ENTRY txAssocRespIETable[] = {
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_ERP), NULL, rlmRspGenerateErpIE}
	,			/* 42 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP), NULL, rlmRspGenerateHtCapIE}
	,			/* 45 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_HT_OP), NULL, rlmRspGenerateHtOpIE}
	,			/* 61 */
#if CFG_ENABLE_WIFI_DIRECT
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_OBSS_SCAN), NULL,
	rlmRspGenerateObssScanIE}
	,			/* 74 */
	{(0), p2pFuncCalculateP2p_IELenForAssocRsp,
	 p2pFuncGenerateP2p_IEForAssocRsp}
	,			/* 221 */
#if (CFG_SUPPORT_WFD)
	{(0), wfdFuncCalculateWfdIELenForAssocRsp,
	 wfdFuncGenerateWfdIEForAssocRsp}
	,			/* 221 */
#endif
#endif
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP), NULL, rlmRspGenerateExtCapIE}
	,			/* 127 */
#if CFG_SUPPORT_802_11AC
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_CAP), NULL, rlmRspGenerateVhtCapIE}
	,			/*191 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP), NULL, rlmRspGenerateVhtOpIE}
	,			/*192 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_OP_MODE_NOTIFICATION), NULL,
	 rlmRspGenerateVhtOpNotificationIE}
	,			/*199 */
#endif
#if CFG_SUPPORT_802_11AX
	{0, heRlmCalculateHeCapIELen, heRlmRspGenerateHeCapIE}
	,			/* 255, EXT 35 */
	{0, heRlmCalculateHeOpIELen, heRlmRspGenerateHeOpIE}
	,			/* 255, EXT 36 */
#if (CFG_SUPPORT_WIFI_6G == 1)
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_HE_6G_CAP), NULL,
	 heRlmReqGenerateHe6gBandCapIE}
	,			/* 255, EXT 59 */
#endif
#endif
#if CFG_SUPPORT_802_11BE
	{0, ehtRlmCalculateCapIELen, ehtRlmRspGenerateCapIE}
	,
	{0, ehtRlmCalculateOpIELen, ehtRlmRspGenerateOpIE}
	,
#endif
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_WMM_PARAM), NULL, mqmGenerateWmmParamIE}
	,			/* 221 */
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_ASSOC_RSP_WSC_IE), NULL,
	 rsnGenerateWSCIEForAssocRsp}
	,			/* 221 */
#if CFG_SUPPORT_MTK_SYNERGY
	{(ELEM_HDR_LEN + ELEM_MIN_LEN_MTK_OUI), NULL, rlmGenerateMTKOuiIE}
	/* 221 */
#endif
	,
#if CFG_SUPPORT_802_11W
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_TIMEOUT_IE), NULL,
	rsnPmfGenerateTimeoutIE}
	/* 56 */
#endif
	,
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_RSN), NULL, rsnGenerateRSNXIE}
	/* 244 */
};
#endif /* CFG_SUPPORT_AAA */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

static uint8_t assocSkipRSNXIe(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec, struct IE_HDR *prIe);
static uint8_t assocSkipWpaIe(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec, struct IE_HDR *prIe);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static struct SKIP_IE_ENTRY g_assocSkipIEs[] = {
	{ELEM_ID_RSN, NULL},
	{ELEM_ID_EXTENDED_CAP, NULL},
	{ELEM_ID_MOBILITY_DOMAIN, NULL},
	{ELEM_ID_RSNX, assocSkipRSNXIe},
	{ELEM_ID_WPA, assocSkipWpaIe},
};

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to compose the Capability Info Field.
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 *
 * @retval Capability Info Field
 */
/*----------------------------------------------------------------------------*/

uint16_t assocBuildCapabilityInfo(IN struct ADAPTER *prAdapter,
				  IN struct STA_RECORD *prStaRec)
{
	uint32_t u4NonHTPhyType;
	uint16_t u2CapInfo;
	struct BSS_INFO *prBssInfo;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	/* Set up our requested capabilities. */
	u2CapInfo = CAP_INFO_ESS;
	u2CapInfo |= CAP_CF_STA_NOT_POLLABLE;
#if CFG_SUPPORT_802_11K
	u2CapInfo |= CAP_INFO_RADIO_MEASUREMENT;
#endif

	if (prStaRec->u2CapInfo & CAP_INFO_PRIVACY)
		u2CapInfo |= CAP_INFO_PRIVACY;

	/* 7.3.1.4 */
	if (prStaRec->fgHasBasicPhyType) {
		u4NonHTPhyType = prStaRec->ucNonHTBasicPhyType;

		if ((rNonHTPhyAttributes
		     [u4NonHTPhyType].fgIsShortPreambleOptionImplemented) &&
		    /* Short Preamble Option Enable is TRUE */
		    ((prAdapter->rWifiVar.ePreambleType == PREAMBLE_TYPE_SHORT)
		     ||
		     ((prAdapter->rWifiVar.ePreambleType == PREAMBLE_TYPE_AUTO)
		      && (prStaRec->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)))) {

			/* Case I: Implemented == TRUE
			 *            and Short Preamble Option Enable == TRUE.
			 * Case II: Implemented == TRUE
			 *            and Short Preamble == AUTO (depends on
			 *          struct BSS_DESC's capability)
			 */
			u2CapInfo |= CAP_INFO_SHORT_PREAMBLE;
		}
#if CFG_SUPPORT_SPEC_MGMT
		/* 802.11h spectrum management is for 5G band, so
		 * now we only enable spectrum management bit for 5G case.
		 * In TGn 5.2.22, spectrum management bit should set to 1
		 * to pass the UCC's check.
		 */
		if (prBssInfo && (prBssInfo->eBand == BAND_5G
#if (CFG_SUPPORT_WIFI_6G == 1)
			|| prBssInfo->eBand == BAND_6G
#endif
		))
			u2CapInfo |= CAP_INFO_SPEC_MGT;
#endif

		if (rNonHTPhyAttributes
		    [u4NonHTPhyType].fgIsShortSlotTimeOptionImplemented
		    && prAdapter->rWifiVar.fgIsShortSlotTimeOptionEnable) {
			u2CapInfo |= CAP_INFO_SHORT_SLOT_TIME;
		}
	}

	DBGLOG(SAA, LOUD,
	       "ASSOC REQ: Compose Capability = 0x%04x for Target BSS [" MACSTR
	       "].\n", u2CapInfo, MAC2STR(prStaRec->aucMacAddr));

	return u2CapInfo;

}				/* end of assocBuildCapabilityInfo() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to compose Common Information Elements for
 *        Association Request Frame.
 *
 * @param[in] prMsduInfo     Pointer to the composed MSDU_INFO_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void assocBuildReAssocReqFrameCommonIEs(
				IN struct ADAPTER *prAdapter,
				IN struct MSDU_INFO *prMsduInfo)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct STA_RECORD *prStaRec;
	uint8_t *pucBuffer;
	uint16_t u2SupportedRateSet;
	uint8_t aucAllSupportedRates[RATE_NUM_SW] = { 0 };
	uint8_t ucAllSupportedRatesLen;
	uint8_t ucSupRatesLen;
	uint8_t ucExtSupRatesLen;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	pucBuffer =
	    (uint8_t *) ((unsigned long)prMsduInfo->prPacket +
			 (unsigned long)prMsduInfo->u2FrameLength);

	prConnSettings =
		aisGetConnSettings(prAdapter, prStaRec->ucBssIndex);

	if (IS_STA_IN_AIS(prStaRec)) {

		/* Fill the SSID element. */
		SSID_IE(pucBuffer)->ucId = ELEM_ID_SSID;

		/* NOTE(Kevin): We copy the SSID from CONNECTION_SETTINGS
		 *   for the case of Passive Scan and
		 *   the target BSS didn't broadcast SSID on its Beacon Frame.
		 */
		 COPY_SSID(SSID_IE(pucBuffer)->aucSSID,
			SSID_IE(pucBuffer)->ucLength,
			prConnSettings->aucSSID,
			prConnSettings->ucSSIDLen);

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
		pucBuffer += IE_SIZE(pucBuffer);

	}
#if CFG_ENABLE_WIFI_DIRECT
	else if ((prAdapter->fgIsP2PRegistered) && (IS_STA_IN_P2P(prStaRec)))
		pucBuffer =
		    p2pBuildReAssocReqFrameCommonIEs(prAdapter, prMsduInfo,
						     pucBuffer);

#endif
#if CFG_ENABLE_BT_OVER_WIFI
	else if (IS_STA_BOW_TYPE(prStaRec)) {

		SSID_IE(pucBuffer)->ucId = ELEM_ID_SSID;

		/* NOTE(Kevin): We copy the SSID from CONNECTION_SETTINGS
		 *   for the case of Passive Scan and
		 *   the target BSS didn't broadcast SSID on its Beacon Frame.
		 */
		COPY_SSID(SSID_IE(pucBuffer)->aucSSID,
			  SSID_IE(pucBuffer)->ucLength,
			  prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen);

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
		pucBuffer += IE_SIZE(pucBuffer);
	}
#endif

	/* NOTE(Kevin 2008/12/19): 16.3.6.3 MLME-ASSOCIATE.indication -
	 * SupportedRates - The set of data rates that are supported by the STA
	 * that is requesting association.
	 * Original(Portable Driver): Only send the Rates that we'll support.
	 * New: Send the Phy Rates if the result of following
	 *      & operation == NULL.
	 */
	/* rateGetDataRatesFromRateSet((prBssDesc->u2OperationalRateSet & */
	/* rPhyAttributes[prBssDesc->ePhyType].u2SupportedRateSet), */

	if (prStaRec->ucDesiredPhyTypeSet) {
		uint32_t u4NonHTPhyType;

		u4NonHTPhyType = prStaRec->ucNonHTBasicPhyType;

		u2SupportedRateSet = (prStaRec->u2OperationalRateSet &
				      rNonHTPhyAttributes
				      [u4NonHTPhyType].u2SupportedRateSet);

		if (!u2SupportedRateSet) {
			DBGLOG(SAA, INFO,
			       "RateSet NonHTPhyType=%d OperationalRateSet=%d\n",
			       u4NonHTPhyType, prStaRec->u2OperationalRateSet);
			u2SupportedRateSet =
			    rNonHTPhyAttributes
			    [u4NonHTPhyType].u2SupportedRateSet;
		}

		/* TODO(Kevin):
		 * For P2P, we shouldn't send support rate set
		 * which contains 11b rate
		 */

		rateGetDataRatesFromRateSet(u2SupportedRateSet, 0,
					    aucAllSupportedRates,
					    &ucAllSupportedRatesLen);

		ucSupRatesLen =
		    ((ucAllSupportedRatesLen >
		      ELEM_MAX_LEN_SUP_RATES) ? ELEM_MAX_LEN_SUP_RATES :
		     ucAllSupportedRatesLen);

		ucExtSupRatesLen = ucAllSupportedRatesLen - ucSupRatesLen;

		/* Fill the Supported Rates element. */
		if (ucSupRatesLen) {
			SUP_RATES_IE(pucBuffer)->ucId = ELEM_ID_SUP_RATES;
			SUP_RATES_IE(pucBuffer)->ucLength = ucSupRatesLen;
			kalMemCopy(SUP_RATES_IE(pucBuffer)->aucSupportedRates,
				   aucAllSupportedRates, ucSupRatesLen);

			prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
			pucBuffer += IE_SIZE(pucBuffer);
		}

		/* Fill the Extended Supported Rates element. */
		if (ucExtSupRatesLen) {

			EXT_SUP_RATES_IE(pucBuffer)->ucId =
			    ELEM_ID_EXTENDED_SUP_RATES;
			EXT_SUP_RATES_IE(pucBuffer)->ucLength =
			    ucExtSupRatesLen;

			kalMemCopy(EXT_SUP_RATES_IE
				   (pucBuffer)->aucExtSupportedRates,
				   &aucAllSupportedRates[ucSupRatesLen],
				   ucExtSupRatesLen);

			prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
			pucBuffer += IE_SIZE(pucBuffer);
		}
	}
}			/* end of assocBuildReAssocReqFrameCommonIEs() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the (Re)Association Request frame header
 *        and its fixed fields
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucMACAddress          Given Our MAC Address.
 * @param[in out] pu2PayloadLen      Return the length of the composed
 *                                   fixed fields
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void
assocComposeReAssocReqFrameHeaderAndFF(IN struct ADAPTER *prAdapter,
				       IN struct STA_RECORD *prStaRec,
				       IN uint8_t *pucBuffer,
				       IN uint8_t aucMACAddress[],
				       IN OUT uint16_t *pu2PayloadLen)
{
	struct WLAN_ASSOC_REQ_FRAME *prAssocFrame;
	u_int8_t fgIsReAssoc;

	uint16_t u2FrameCtrl;
	uint16_t u2CapInfo;
	uint16_t u2ListenInterval;

	prAssocFrame = (struct WLAN_ASSOC_REQ_FRAME *)pucBuffer;
	fgIsReAssoc = prStaRec->fgIsReAssoc;

	/* 4 <1> Compose the frame header of the (Re)Association
	 *       Request frame.
	 */
	/* Fill the Frame Control field. */
	if (fgIsReAssoc)
		u2FrameCtrl = MAC_FRAME_REASSOC_REQ;
	else
		u2FrameCtrl = MAC_FRAME_ASSOC_REQ;

	WLAN_SET_FIELD_16(&prAssocFrame->u2FrameCtrl, u2FrameCtrl);

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prAssocFrame->aucDestAddr, prStaRec->aucMacAddr);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prAssocFrame->aucSrcAddr, aucMACAddress);

	/* Fill the BSSID field with Target BSSID. */
	COPY_MAC_ADDR(prAssocFrame->aucBSSID, prStaRec->aucMacAddr);

	/* Clear the SEQ/FRAG_NO field(HW won't overide the FRAG_NO,
	 * so we need to clear it).
	 */
	prAssocFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's common fixed field part of
	 *       the (Re)Association Request  frame.
	 */
	u2CapInfo = assocBuildCapabilityInfo(prAdapter, prStaRec);

	/* Fill the Capability Information field. */
	WLAN_SET_FIELD_16(&prAssocFrame->u2CapInfo, u2CapInfo);

	/* Calculate the listen interval for the maximum power mode. Currently,
	 * we set it to the value 2 times DTIM period.
	 */
	if (prStaRec->ucDTIMPeriod) {
		u2ListenInterval =
		    prStaRec->ucDTIMPeriod *
		    DEFAULT_LISTEN_INTERVAL_BY_DTIM_PERIOD;
	} else {
		DBGLOG(SAA, TRACE, "Use default listen interval\n");
		u2ListenInterval = DEFAULT_LISTEN_INTERVAL;
	}
	prStaRec->u2ListenInterval = u2ListenInterval;

	/* Fill the Listen Interval field. */
	WLAN_SET_FIELD_16(&prAssocFrame->u2ListenInterval, u2ListenInterval);

	/* 4 <3> Compose the Current AP Address field for ReAssociation
	 *       Request frame.
	 */
	/* Fill the Current AP Address field. */
	if (prStaRec->fgIsReAssoc) {
		if (IS_STA_IN_AIS(prStaRec)) {

			struct BSS_INFO *prAisBssInfo =
				aisGetAisBssInfo(prAdapter,
				prStaRec->ucBssIndex);
			struct WLAN_REASSOC_REQ_FRAME *prReAssocFrame =
			    (struct WLAN_REASSOC_REQ_FRAME *)prAssocFrame;

			COPY_MAC_ADDR(prReAssocFrame->aucCurrentAPAddr,
				      prAisBssInfo->aucBSSID);
		} else {
			ASSERT(0);
			/* We don't support ReAssociation for other network */
		}

		*pu2PayloadLen =
		    (CAP_INFO_FIELD_LEN + LISTEN_INTERVAL_FIELD_LEN +
		     CURR_AP_ADDR_FIELD_LEN);
	} else {
		*pu2PayloadLen =
		    (CAP_INFO_FIELD_LEN + LISTEN_INTERVAL_FIELD_LEN);
	}
}				/* end of assocComposeReAssocReqFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the (Re)Association Request frame
 *
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 */
/*----------------------------------------------------------------------------*/
uint32_t assocSendReAssocReqFrame(IN struct ADAPTER *prAdapter,
				  IN struct STA_RECORD *prStaRec)
{
	struct MSDU_INFO *prMsduInfo;
	struct BSS_INFO *prBssInfo;

	uint16_t u2PayloadLen;
	uint16_t u2EstimatedFrameLen;
	uint16_t u2EstimatedExtraIELen;
	u_int8_t fgIsReAssoc;
	uint32_t i;
	uint16_t txAssocReqIENums;

	/* 4 <1> Allocate a PKT_INFO_T for Authentication Frame */
	fgIsReAssoc = prStaRec->fgIsReAssoc;

	if (prStaRec->ucBssIndex >= MAX_BSS_INDEX)
		return WLAN_STATUS_INVALID_DATA;

	/* Init with MGMT Header Length + Length of Fixed Fields
	 *   + Common IE Length
	 */
	if (fgIsReAssoc) {
		u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
		    WLAN_MAC_MGMT_HEADER_LEN +
		    CAP_INFO_FIELD_LEN +
		    LISTEN_INTERVAL_FIELD_LEN +
		    CURR_AP_ADDR_FIELD_LEN +
		    (ELEM_HDR_LEN + ELEM_MAX_LEN_SSID) +
		    (ELEM_HDR_LEN + ELEM_MAX_LEN_SUP_RATES) + (ELEM_HDR_LEN +
				       (RATE_NUM_SW - ELEM_MAX_LEN_SUP_RATES));
	} else {
		u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
		    WLAN_MAC_MGMT_HEADER_LEN +
		    CAP_INFO_FIELD_LEN +
		    LISTEN_INTERVAL_FIELD_LEN +
		    (ELEM_HDR_LEN + ELEM_MAX_LEN_SSID) +
		    (ELEM_HDR_LEN + ELEM_MAX_LEN_SUP_RATES) + (ELEM_HDR_LEN +
				       (RATE_NUM_SW - ELEM_MAX_LEN_SUP_RATES));
	}

	/* + Extra IE Length */
	u2EstimatedExtraIELen = 0;
	txAssocReqIENums = sizeof(txAssocReqIETable) /
			   sizeof(struct APPEND_VAR_IE_ENTRY);

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211 && CFG_ENABLE_WIFI_DIRECT
	if (IS_STA_IN_P2P(prStaRec)) {
		if ((prAdapter->fgIsP2PRegistered)) {
			u2EstimatedExtraIELen =
			    p2pCalculate_IEForAssocReq(prAdapter,
						       prStaRec->ucBssIndex,
						       prStaRec);
		} else {
			DBGLOG(P2P, TRACE, "Function Linker Lost.\n");
			ASSERT(FALSE);
		}
	} else {
		for (i = 0; i < txAssocReqIENums; i++) {
			if (txAssocReqIETable[i].u2EstimatedFixedIELen != 0) {
				u2EstimatedExtraIELen +=
				    txAssocReqIETable[i].u2EstimatedFixedIELen;
			} else {
				u2EstimatedExtraIELen += (uint16_t)
				    txAssocReqIETable
				    [i].pfnCalculateVariableIELen(prAdapter,
					  prStaRec->ucBssIndex, prStaRec);
			}
		}
		/* Calculate non-wfa vendor specific ie len */
		u2EstimatedExtraIELen +=
			assoc_get_nonwfa_vend_ie_len(prAdapter,
			prStaRec->ucBssIndex);
	}
#else
	for (i = 0; i < txAssocReqIENums; i++) {
		if (txAssocReqIETable[i].u2EstimatedFixedIELen != 0) {
			u2EstimatedExtraIELen +=
			    txAssocReqIETable[i].u2EstimatedFixedIELen;
		} else {
			u2EstimatedExtraIELen += (uint16_t)
			    txAssocReqIETable[i].pfnCalculateVariableIELen
			    (prAdapter, prStaRec->ucBssIndex, prStaRec);
		}
	}
	/* Calculate non-wfa vendor specific ie len */
	u2EstimatedExtraIELen += assoc_get_nonwfa_vend_ie_len(prAdapter,
		prStaRec->ucBssIndex);
#endif
	u2EstimatedFrameLen += u2EstimatedExtraIELen;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN,
		       "No PKT_INFO_T for sending (Re)Assoc Request.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose (Re)Association Request frame header and fixed fields
	 *       in MSDU_INfO_T.
	 */
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	/* Compose Header and Fixed Field */
	assocComposeReAssocReqFrameHeaderAndFF(prAdapter,
					       prStaRec,
					       (uint8_t
						*) ((unsigned
						     long)(prMsduInfo->prPacket)
						    + MAC_TX_RESERVED_FIELD),
					       prBssInfo->aucOwnMacAddr,
					       &u2PayloadLen);

	/* 4 <3> Update information of MSDU_INFO_T */
	nicTxSetPktLifeTime(prMsduInfo, 100);
	nicTxSetPktRetryLimit(prMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);
	nicTxSetForceRts(prMsduInfo, TRUE);

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen,
		     saaFsmRunEventTxDone, MSDU_RATE_MODE_AUTO);

	/* 4 <4> Compose the frame body's IEs of the (Re)Association Request
	 * frame.
	 */
	assocBuildReAssocReqFrameCommonIEs(prAdapter, prMsduInfo);

	/* 4 <5> Compose IEs in MSDU_INFO_T */
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211 && CFG_ENABLE_WIFI_DIRECT
	if (IS_STA_IN_P2P(prStaRec)) {
		if ((prAdapter->fgIsP2PRegistered)) {
			p2pGenerate_IEForAssocReq(prAdapter, prMsduInfo);
		} else {
			DBGLOG(P2P, TRACE, "Function Linker Lost.\n");
			ASSERT(FALSE);
		}
	} else {
		/* Append IE */
		for (i = 0; i < txAssocReqIENums; i++) {
			if (txAssocReqIETable[i].pfnAppendIE)
				txAssocReqIETable[i].pfnAppendIE(prAdapter,
								 prMsduInfo);
			}
		/* Append non-wfa vendor specific ies for AIS mode */
		assoc_build_nonwfa_vend_ie(prAdapter, prMsduInfo);
	}
#else
	/* Append IE */
	for (i = 0; i < txAssocReqIENums; i++) {
		if (txAssocReqIETable[i].pfnAppendIE)
			txAssocReqIETable[i].pfnAppendIE(prAdapter, prMsduInfo);
	}
	/* Append non-wfa vendor specific ies for AIS mode */
	assoc_build_nonwfa_vend_ie(prAdapter, prMsduInfo);
#endif

	sortAssocReqIE(prAdapter, prMsduInfo, fgIsReAssoc);

	/* 4 <6> Update the (Re)association request information */
	if (IS_STA_IN_AIS(prStaRec)) {
		struct WLAN_ASSOC_REQ_FRAME *prAssocFrame;

		prAssocFrame =
		    (struct WLAN_ASSOC_REQ_FRAME
		     *)((unsigned long)(prMsduInfo->prPacket) +
			MAC_TX_RESERVED_FIELD);

		kalUpdateReAssocReqInfo(prAdapter->prGlueInfo,
					(uint8_t *) &prAssocFrame->u2CapInfo,
					prMsduInfo->u2FrameLength -
					offsetof(struct WLAN_ASSOC_REQ_FRAME,
						 u2CapInfo), fgIsReAssoc,
					prStaRec->ucBssIndex);
	}
#if CFG_ENABLE_WIFI_DIRECT
	if ((prAdapter->fgIsP2PRegistered) && (IS_STA_IN_P2P(prStaRec))) {
		struct WLAN_ASSOC_REQ_FRAME *prAssocFrame;

		prAssocFrame =
		    (struct WLAN_ASSOC_REQ_FRAME
		     *)((unsigned long)(prMsduInfo->prPacket) +
			MAC_TX_RESERVED_FIELD);

		kalP2PUpdateAssocInfo(prAdapter->prGlueInfo,
				      (uint8_t *) &prAssocFrame->u2CapInfo,
				      prMsduInfo->u2FrameLength -
				      offsetof(struct WLAN_ASSOC_REQ_FRAME,
					       u2CapInfo), fgIsReAssoc,
				      prStaRec->ucBssIndex);
	}
#endif

	/* TODO(Kevin): Also release the unused tail room of the composed MMPDU
	 */

	nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	/* 4 <6> Enqueue the frame to send this (Re)Association request frame.
	 */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of assocSendReAssocReqFrame() */

static uint8_t assocSkipRSNXIe(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec, struct IE_HDR *prIe)
{
	uint8_t ucBssIndex = prStaRec->ucBssIndex;

	if (IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		struct BSS_DESC *prTargetBss =
			aisGetTargetBssDesc(prAdapter, ucBssIndex);

		/* skip rsnxe if target ap doesn't support rsnxe */
		if (prTargetBss && !prTargetBss->fgIERSNX &&
		   (prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_FT ||
		    prTargetBss->eRsnSelectedAuthMode == AUTH_MODE_WPA2_PSK))
			return TRUE;
	}

	return FALSE;
}

static uint8_t assocSkipWpaIe(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec, struct IE_HDR *prIe)
{
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;
	struct WPA_INFO_ELEM *prWpaIE = (struct WPA_INFO_ELEM *) NULL;

	prWpaIE = (struct WPA_INFO_ELEM *) prIe;

	if (prWpaIE->ucLength <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE)
		return FALSE;

	if (kalMemCmp(prWpaIE->aucOui, aucWfaOui, sizeof(aucWfaOui)))
		return FALSE;

	/* skip wpa ie, fill by driver */
	if (prWpaIE->ucOuiType == VENDOR_OUI_TYPE_WPA &&
	    prWpaIE->u2Version == 1)
		return TRUE;

	return FALSE;
}

uint32_t assocCalculateConnIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			     struct STA_RECORD *prStaRec)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex;
	const uint8_t *ie;
	uint16_t u2RetLen = 0;
	uint8_t i;

	ucBssIndex = prStaRec->ucBssIndex;
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	if (IS_STA_IN_AIS(prStaRec) && prConnSettings->assocIeLen > 0) {
		prConnSettings = aisGetConnSettings(prAdapter, ucBssIdx);
		u2RetLen = prConnSettings->assocIeLen;

		for (i = 0; i < ARRAY_SIZE(g_assocSkipIEs); i++) {
			ie = kalFindIeMatchMask(g_assocSkipIEs[i].ucElemID,
				       prConnSettings->pucAssocIEs,
				       prConnSettings->assocIeLen,
				       NULL, 0, 0, NULL);
			/* cut out IE */
			if (ie) {
				if (g_assocSkipIEs[i].pfnSkipIE &&
					!g_assocSkipIEs[i].pfnSkipIE(
					prAdapter, prStaRec,
					(struct IE_HDR *)ie))
					continue;
				if (u2RetLen < IE_SIZE(ie)) {
					DBGLOG(SAA, WARN,
					"IE[%d] size[%d] > rem buf size[%d]!\n",
						IE_ID(ie),
						IE_SIZE(ie),
						u2RetLen);
					continue;
				}
				u2RetLen -= IE_SIZE(ie);
			}
		}
	}

	return u2RetLen;
}

void assocGenerateConnIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct STA_RECORD *prStaRec;
	uint8_t *pucBuffer, *cp;
	const uint8_t *ie;
	uint8_t ucBssIndex;
	uint8_t i;
	uint32_t len, ieLen;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return;

	pucBuffer = (uint8_t *) ((uintptr_t)
				 prMsduInfo->prPacket + (uintptr_t)
				 prMsduInfo->u2FrameLength);

	cp = pucBuffer;
	ucBssIndex = prStaRec->ucBssIndex;
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	if (IS_STA_IN_AIS(prStaRec) && prConnSettings->assocIeLen > 0) {
		kalMemCopy(cp, prConnSettings->pucAssocIEs,
				   prConnSettings->assocIeLen);
		cp += prConnSettings->assocIeLen;

		for (i = 0; i < ARRAY_SIZE(g_assocSkipIEs); i++) {
			ie = kalFindIeMatchMask(g_assocSkipIEs[i].ucElemID,
				       pucBuffer,
				       cp - pucBuffer,
				       NULL, 0, 0, NULL);
			if (ie) {
				if (g_assocSkipIEs[i].pfnSkipIE &&
					!g_assocSkipIEs[i].pfnSkipIE(
					prAdapter, prStaRec,
					(struct IE_HDR *)ie))
					continue;

				ieLen = IE_SIZE(ie);
				if ((uint32_t)(cp - ie) < ieLen) {
					DBGLOG(SAA, WARN,
					"IE[%d] size[%d] > rem buf size[%d]!\n",
						IE_ID(ie),
						ieLen,
						(uint32_t)(cp - ie));
					continue;
				}

				len = cp - ie - ieLen;
				/* copy to the start of IE*/
				cp = (char *) ie;
				/* jump to the end of IE to copy Remaing IEs*/
				kalMemMove(cp, ie + ieLen, len);
				cp += len;
			}
		}
	}
	prMsduInfo->u2FrameLength += cp - pucBuffer;
	DBGLOG_MEM8(SAA, INFO, pucBuffer, cp - pucBuffer);
}

#if CFG_SUPPORT_ASSURANCE

uint32_t assocCalculateRoamReasonLen(struct ADAPTER *prAdapter,
		uint8_t ucBssIdx, struct STA_RECORD *prStaRec)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t ucBssIndex = 0;

	ucBssIndex = prStaRec->ucBssIndex;
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	if (IS_STA_IN_AIS(prStaRec) && prStaRec->fgIsReAssoc &&
	    prAisSpecificBssInfo->fgRoamingReasonEnable)
		return sizeof(struct IE_ASSURANCE_ROAMING_REASON);

	return 0;
}

void assocGenerateRoamReason(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	struct STA_RECORD *prStaRec;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prAisBssInfo;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return;

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ucBssIndex = prStaRec->ucBssIndex;
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	if (IS_STA_IN_AIS(prStaRec) && prStaRec->fgIsReAssoc &&
	    prAisSpecificBssInfo->fgRoamingReasonEnable) {
		struct IE_ASSURANCE_ROAMING_REASON *ie =
			(struct IE_ASSURANCE_ROAMING_REASON *) pucBuffer;

		ie->ucId = ELEM_ID_VENDOR;
		ie->ucLength =
		      sizeof(struct IE_ASSURANCE_ROAMING_REASON) - ELEM_HDR_LEN;
		WLAN_SET_FIELD_BE24(ie->aucOui, VENDOR_IE_SAMSUNG_OUI);
		ie->ucOuiType = 0x22;
		ie->ucSubType = 0x04;
		ie->ucVersion = 0x01;
		ie->ucSubTypeReason = 0x00;

		/* 0. unspecified
		 * 1. low rssi
		 * 2. CU
		 * 3. Beacon lost
		 * 4. Deauth/Disassoc
		 * 5. BTM
		 * 6. idle roaming
		 * 7. manual
		 */
		switch (prRoamingFsmInfo->eReason) {
		case ROAMING_REASON_POOR_RCPI:
			ie->ucReason = 0x01;
			break;
		case ROAMING_REASON_BEACON_TIMEOUT:
		case ROAMING_REASON_BEACON_TIMEOUT_TX_ERR:
			ie->ucReason = 0x03;
			break;
		case ROAMING_REASON_SAA_FAIL:
			ie->ucReason = 0x04;
			break;
		case ROAMING_REASON_BTM:
			ie->ucReason = 0x05;
			break;
		case ROAMING_REASON_IDLE:
			ie->ucReason = 0x06;
			break;
		case ROAMING_REASON_INACTIVE:
		case ROAMING_REASON_TX_ERR:
			ie->ucReason = 0x07;
			break;
		default:
			ie->ucReason = 0x05;
		}
		ie->ucSubTypeRcpi = 0x01;
		ie->ucRcpi = prRoamingFsmInfo->ucRcpi;
		ie->ucSubTypeRcpiThreshold = 0x02;
		ie->ucRcpiThreshold = prRoamingFsmInfo->ucThreshold;
		ie->ucSubTypeCuThreshold = 0x03;
		ie->ucCuThreshold = 0;
		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
		DBGLOG_MEM8(SAA, INFO, pucBuffer, IE_SIZE(pucBuffer));
	}
}

#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will strictly check the TX (Re)Association Request
 *        frame for SAA event handling.
 *
 * @param[in] prMsduInfo         Pointer of MSDU_INFO_T
 *
 * @retval WLAN_STATUS_FAILURE   This is not the frame we should handle
 *                               at current state.
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t assocCheckTxReAssocReqFrame(IN struct ADAPTER *prAdapter,
				     IN struct MSDU_INFO *prMsduInfo)
{
	struct WLAN_ASSOC_REQ_FRAME *prAssocReqFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TxFrameCtrl;

	prAssocReqFrame = (struct WLAN_ASSOC_REQ_FRAME *)(prMsduInfo->prPacket);
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	/* WLAN_GET_FIELD_16(&prAssocReqFrame->u2FrameCtrl, &u2TxFrameCtrl) */
	u2TxFrameCtrl = prAssocReqFrame->u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */
	u2TxFrameCtrl &= MASK_FRAME_TYPE;
	if (prStaRec->fgIsReAssoc) {
		if (u2TxFrameCtrl != MAC_FRAME_REASSOC_REQ)
			return WLAN_STATUS_FAILURE;

	} else {
		if (u2TxFrameCtrl != MAC_FRAME_ASSOC_REQ)
			return WLAN_STATUS_FAILURE;

	}

	return WLAN_STATUS_SUCCESS;

}				/* end of assocCheckTxReAssocReqFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will strictly check the TX (Re)Association Response
 *        frame for AAA event handling.
 *
 * @param[in] prMsduInfo         Pointer of MSDU_INFO_T
 *
 * @retval WLAN_STATUS_FAILURE   This is not the frame we should handle
 *                               at current state.
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t assocCheckTxReAssocRespFrame(IN struct ADAPTER *prAdapter,
				      IN struct MSDU_INFO *prMsduInfo)
{
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TxFrameCtrl;

	prAssocRspFrame = (struct WLAN_ASSOC_RSP_FRAME *)(prMsduInfo->prPacket);
	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	/* WLAN_GET_FIELD_16(&prAssocFrame->u2FrameCtrl, &u2TxFrameCtrl) */
	u2TxFrameCtrl = prAssocRspFrame->u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */
	u2TxFrameCtrl &= MASK_FRAME_TYPE;
	if (prStaRec->fgIsReAssoc) {
		if (u2TxFrameCtrl != MAC_FRAME_REASSOC_RSP)
			return WLAN_STATUS_FAILURE;

	} else {
		if (u2TxFrameCtrl != MAC_FRAME_ASSOC_RSP)
			return WLAN_STATUS_FAILURE;

	}

	return WLAN_STATUS_SUCCESS;

}				/* end of assocCheckTxReAssocRespFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will validate the incoming (Re)Association Frame
 *        and take out the status code.
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[out] pu2StatusCode         Pointer to store the Status Code
 *                                   from Authentication.
 *
 * @retval WLAN_STATUS_FAILURE       This is not the frame we should handle
 *                                   at current state.
 * @retval WLAN_STATUS_SUCCESS       This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t
assocCheckRxReAssocRspFrameStatus(IN struct ADAPTER *prAdapter,
				  IN struct SW_RFB *prSwRfb,
				  OUT uint16_t *pu2StatusCode)
{
	struct STA_RECORD *prStaRec;
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame;
	uint16_t u2RxFrameCtrl;
	uint16_t u2RxCapInfo;
	uint16_t u2RxStatusCode;
	uint16_t u2RxAssocId;

	if (!prSwRfb || !pu2StatusCode) {
		DBGLOG(SAA, ERROR, "Invalid parameter, ignore!\n");
		return WLAN_STATUS_FAILURE;
	}

	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
	    (CAP_INFO_FIELD_LEN +
	     STATUS_CODE_FIELD_LEN +
	     AID_FIELD_LEN)) {
		DBGLOG(SAA, WARN, "Invalid AssocRsp length!");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(SAA, LOUD, "prSwRfb->u2PayloadLength = %d\n",
	       prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec) {
		DBGLOG(SAA, ERROR, "Invalid prStaRec, ignore!\n");
		return WLAN_STATUS_INVALID_PACKET;
	}

	if (prStaRec->ucBssIndex >= MAX_BSS_INDEX) {
		DBGLOG(NIC, ERROR, "ucBssIndex out of range!\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <1> locate the (Re)Association Resp Frame. */
	prAssocRspFrame = (struct WLAN_ASSOC_RSP_FRAME *)prSwRfb->pvHeader;

	/* If Association Response's BSSID doesn't match
	 * our target, ignore.
	 */
	if (!EQUAL_MAC_ADDR(prAssocRspFrame->aucBSSID,
		 prStaRec->aucMacAddr)) {
		DBGLOG(SAA, INFO, "Unknown BSSID\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <2> Parse the Header of (Re)Association Resp Frame. */
	/* WLAN_GET_FIELD_16(&prAssocRspFrame->u2FrameCtrl, &u2RxFrameCtrl); */
	u2RxFrameCtrl = prAssocRspFrame->u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */
	u2RxFrameCtrl &= MASK_FRAME_TYPE;
	if (prStaRec->fgIsReAssoc) {
		if (u2RxFrameCtrl != MAC_FRAME_REASSOC_RSP)
			return WLAN_STATUS_FAILURE;

	} else {
		if (u2RxFrameCtrl != MAC_FRAME_ASSOC_RSP)
			return WLAN_STATUS_FAILURE;

	}

	/* 4 <3> Parse the Fixed Fields of (Re)Association Resp Frame Body. */
	/* WLAN_GET_FIELD_16(&prAssocRspFrame->u2CapInfo, &u2RxCapInfo); */
	u2RxCapInfo = prAssocRspFrame->u2CapInfo;
	/* NOTE(Kevin): Optimized for ARM */

	/* WLAN_GET_FIELD_16(&prAssocRspFrame->u2StatusCode, &u2RxStatusCode);
	 */
	u2RxStatusCode = prAssocRspFrame->u2StatusCode;
	/* NOTE(Kevin): Optimized for ARM */

	/* 4 <4> Check CAP_INFO */
	/* NOTE(Kevin): CM suggest to add MGMT workaround for those APs
	 * didn't check the CAP Privacy Bit to overcome a corner case
	 * that the Privacy Bit of our SCAN result didn't consist with
	 * AP's Association Resp.
	 */
	if (u2RxStatusCode == STATUS_CODE_SUCCESSFUL) {
#if CFG_SUPPORT_WAPI
		if (aisGetWapiMode(prAdapter, prStaRec->ucBssIndex)) {
			/* WAPI AP allow the customer use WZC to join mode,
			 * the privacy bit is 0 even at WAI & WAPI_PSK mode,
			 * but the assoc respose set the privacy bit set 1
			 */
			DBGLOG(SEC, TRACE,
			       "Workaround the WAPI AP allow the customer to use WZC to join\n");
		} else
#endif
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered && 1) {
			/* Todo:: Fixed this */
		} else
#endif
		{
		}

#if CFG_STRICT_CHECK_CAPINFO_PRIVACY
		if ((prStaRec->u2CapInfo & CAP_INFO_PRIVACY) ^
			(u2RxCapInfo & CAP_INFO_PRIVACY))
			u2RxStatusCode = STATUS_CODE_CAP_NOT_SUPPORTED;

#endif
	}

	if (u2RxStatusCode == STATUS_CODE_SUCCESSFUL) {
		/* Update the information in the structure used to query and set
		 *  OID_802_11_ASSOCIATION_INFORMATION.
		 */

		kalUpdateReAssocRspInfo(prAdapter->prGlueInfo,
					(uint8_t *) &
					prAssocRspFrame->u2CapInfo,
					prSwRfb->u2PacketLen -
						prSwRfb->u2HeaderLen,
					prStaRec->ucBssIndex);
	}
	/* 4 <5> Update CAP_INFO and ASSOC_ID */
	if (u2RxStatusCode == STATUS_CODE_SUCCESSFUL) {
		prStaRec->u2CapInfo = u2RxCapInfo;

		/* WLAN_GET_FIELD_16(&prAssocRspFrame->u2AssocId,
		 *       &u2RxAssocId);
		 */
		u2RxAssocId = prAssocRspFrame->u2AssocId;
		/* NOTE(Kevin): Optimized for ARM */

		/*  20110715 Workaround for Kingnet 710 AP (Realtek 8186)
		 *   This AP raises the bit 6&7 not bit 14&15 in AID field.
		 *   It cause wrong AID assignment.
		 *   For AID = 2
		 *     Normal case: 0xC002(1100 0000 0000 0010) => 2
		 *     Kingnet 710:  0x00C2(0000 0000 1100 0010) => 194
		 *     workaround: mask bit 6&7 for this AP
		 */
		if ((u2RxAssocId & BIT(6)) && (u2RxAssocId & BIT(7))
		    && !(u2RxAssocId & BITS(8, 15))) {
			prStaRec->u2AssocId = u2RxAssocId & ~BITS(6, 7);
		} else {
			prStaRec->u2AssocId = u2RxAssocId & ~AID_MSB;
#if CFG_SUPPORT_802_11W
			if (prStaRec->eStaType == STA_TYPE_LEGACY_AP) {
				struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

				prBssSpecInfo =
					aisGetAisSpecBssInfo(prAdapter,
					prStaRec->ucBssIndex);
				prBssSpecInfo->ucSaQueryTimedOut = 0;
			}
#endif
		}
	}
#if CFG_SUPPORT_802_11W
	if (u2RxStatusCode == STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED) {
		DBGLOG(SAA, INFO,
		       "AP rejected due the authentication algorithm not support\n");
	} else if (u2RxStatusCode == STATUS_CODE_ASSOC_REJECTED_TEMPORARILY) {
		uint8_t *pucIE, *pucTime;
		uint16_t u2IELength;
		uint16_t u2Offset = 0;

		u2IELength = prSwRfb->u2PacketLen -
		    (uint16_t) OFFSET_OF(struct WLAN_ASSOC_RSP_FRAME,
					 aucInfoElem[0]);

		pucIE = prAssocRspFrame->aucInfoElem;

		if (pucIE != NULL && u2IELength > 0) {
			DBGLOG_MEM8(SAA, TRACE, pucIE, u2IELength);
		} else {
			DBGLOG(SAA, ERROR, "Empty IE\n");
		}

		IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
			if (IE_ID(pucIE) == ELEM_ID_TIMEOUT_INTERVAL
			    && IE_LEN(pucIE) == 5) {
				pucTime = ((struct IE_HDR *)pucIE)->aucInfo;
				if (pucTime[0] ==
				    ACTION_SA_TIMEOUT_ASSOC_COMEBACK) {
					uint32_t tu;

					WLAN_GET_FIELD_32(pucTime + 1, &tu);
					DBGLOG(SAA, INFO,
					       "AP rejected association temporarily;comeback duration %u TU (%u ms)\n",
					       tu, TU_TO_MSEC(tu));
					prStaRec->u4assocComeBackTime = tu;
					if (tu >
					    TX_ASSOCIATION_RETRY_TIMEOUT_TU) {
						DBGLOG(SAA, INFO,
						       "Update timer based on comeback duration: %u TU\n",
						       tu);
						/* ieee80211_reschedule_timer(
						 * wpa_s, ms);
						 */
					} else {
						prStaRec->u4assocComeBackTime =
						TX_ASSOCIATION_RETRY_TIMEOUT_TU;
						DBGLOG(SAA, INFO,
							"Update a minimal comeback duration: %u TU\n",
					       TX_ASSOCIATION_RETRY_TIMEOUT_TU);
					}
				}
			}
		}		/* end of IE_FOR_EACH */
	}
#endif
	*pu2StatusCode = u2RxStatusCode;

	if (IS_STA_IN_AIS(prStaRec) &&
		prStaRec->eAuthAssocState == SAA_STATE_WAIT_ASSOC2) {
		char log[256] = {0};

		u2RxAssocId = prAssocRspFrame->u2AssocId;
		if ((u2RxAssocId & BIT(6)) && (u2RxAssocId & BIT(7))
		    && !(u2RxAssocId & BITS(8, 15))) {
			u2RxAssocId = u2RxAssocId & ~BITS(6, 7);
		} else {
			u2RxAssocId = u2RxAssocId & ~AID_MSB;
		}
		if (prStaRec->fgIsReAssoc)
			kalSprintf(log, "[CONN] REASSOC");
		else
			kalSprintf(log, "[CONN] ASSOC");

		kalSprintf(log + strlen(log),
			" RESP bssid=" RPTMACSTR
			" sn=%d status=%d assoc_id=%d",
			RPTMAC2STR(prStaRec->aucMacAddr),
			WLAN_GET_SEQ_SEQ(prAssocRspFrame->u2SeqCtrl),
			*pu2StatusCode, u2RxAssocId);
		kalReportWifiLog(prAdapter, prStaRec->ucBssIndex, log);
	}
	return WLAN_STATUS_SUCCESS;

}				/* end of assocCheckRxReAssocRspFrameStatus() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will compose the Disassociation frame
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucMACAddress          Given Our MAC Address.
 * @param[in] u2ReasonCode           The reason code of disassociation
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
assocComposeDisassocFrame(IN struct STA_RECORD *prStaRec,
			  IN uint8_t *pucBuffer, IN uint8_t aucMACAddress[],
			  IN uint16_t u2ReasonCode)
{
	struct WLAN_DISASSOC_FRAME *prDisAssocFrame;
	uint16_t u2FrameCtrl;

	prDisAssocFrame = (struct WLAN_DISASSOC_FRAME *)pucBuffer;

	/* 4 <1> Compose the frame header of the DisAssociation  frame. */
	/* Fill the Frame Control field. */
	u2FrameCtrl = MAC_FRAME_DISASSOC;

	WLAN_SET_FIELD_16(&prDisAssocFrame->u2FrameCtrl, u2FrameCtrl);

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prDisAssocFrame->aucDestAddr, prStaRec->aucMacAddr);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prDisAssocFrame->aucSrcAddr, aucMACAddress);

	/* Fill the BSSID field with Target BSSID. */
	COPY_MAC_ADDR(prDisAssocFrame->aucBSSID, prStaRec->aucMacAddr);

	/* Clear the SEQ/FRAG_NO field(HW won't overide the FRAG_NO,
	 * so we need to clear it).
	 */
	prDisAssocFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's fixed field part of
	 * the Disassociation frame.
	 */
	/* Fill the Reason Code field. */
	WLAN_SET_FIELD_16(&prDisAssocFrame->u2ReasonCode, u2ReasonCode);
}				/* end of assocComposeDisassocFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the Disassociation frame
 *
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 * @param[in] u2ReasonCode  The reason code of disassociation
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 */
/*----------------------------------------------------------------------------*/
uint32_t assocSendDisAssocFrame(IN struct ADAPTER *prAdapter,
				IN struct STA_RECORD *prStaRec,
				IN uint16_t u2ReasonCode)
{
	uint8_t *pucMacAddress;
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2PayloadLen;
	uint16_t u2EstimatedFrameLen;
	/* UINT_32 u4Status = WLAN_STATUS_SUCCESS; */

	DBGLOG(RSN, INFO, "assocSendDisAssocFrame\n");

	/* 4 <1> Allocate a PKT_INFO_T for Disassociation Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields + IE Length */
	u2EstimatedFrameLen =
	    MAC_TX_RESERVED_FIELD + WLAN_MAC_MGMT_HEADER_LEN +
	    REASON_CODE_FIELD_LEN;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN, "No PKT_INFO_T for sending DisAssoc.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose Disassociation  frame header and fixed fields
	 *       in MSDU_INfO_T.
	 */
	pucMacAddress =
	    GET_BSS_INFO_BY_INDEX(prAdapter,
				  prStaRec->ucBssIndex)->aucOwnMacAddr;

	/* Compose Header and Fixed Field */
	assocComposeDisassocFrame(prStaRec,
				  (uint8_t
				   *) ((unsigned long)(prMsduInfo->prPacket) +
				       MAC_TX_RESERVED_FIELD), pucMacAddress,
				  u2ReasonCode);

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		/* PMF certification 4.3.3.1, 4.3.3.2 send unprotected
		 * deauth reason 6/7
		 */
		if (prStaRec->rPmfCfg.fgRxDeauthResp != TRUE) {

			struct WLAN_DISASSOC_FRAME *prDisassocFrame;

			prDisassocFrame =
			    (struct WLAN_DISASSOC_FRAME
			     *)((unsigned long)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);

			prDisassocFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		}
	}
#endif

	u2PayloadLen = REASON_CODE_FIELD_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	nicTxSetPktLifeTime(prMsduInfo, 100);
	nicTxSetPktRetryLimit(prMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);
	nicTxSetForceRts(prMsduInfo, TRUE);

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	/* caution: access prStaRec only if true */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		/* 4.3.3.1 send unprotected deauth reason 6/7 */
		if (prStaRec->rPmfCfg.fgRxDeauthResp != TRUE) {
			DBGLOG(RSN, INFO,
			       "Disassoc Set MSDU_OPT_PROTECTED_FRAME\n");
			nicTxConfigPktOption(prMsduInfo,
					     MSDU_OPT_PROTECTED_FRAME, TRUE);
		}

		prStaRec->rPmfCfg.fgRxDeauthResp = FALSE;
	}
#endif
	DBGLOG(SAA, INFO, "ucTxSeqNum=%d ucStaRecIndex=%d u2ReasonCode=%d\n",
	       prMsduInfo->ucTxSeqNum, prMsduInfo->ucStaRecIndex, u2ReasonCode);

	/* 4 <4> Enqueue the frame to send this (Re)Association request frame.
	 */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of assocSendDisAssocFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Disassociation
 *        frame if the given BSSID is matched.
 *
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 * @param[in] aucBSSID           Given BSSID
 * @param[out] pu2ReasonCode     Pointer to store the Reason Code from
 *                               Deauthentication.
 *
 * @retval WLAN_STATUS_FAILURE   This is not the frame we should handle
 *                               at current state.
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t
assocProcessRxDisassocFrame(IN struct ADAPTER *prAdapter,
			    IN struct SW_RFB *prSwRfb, IN uint8_t aucBSSID[],
			    OUT uint16_t *pu2ReasonCode)
{
	struct WLAN_DISASSOC_FRAME *prDisassocFrame;
	uint16_t u2RxReasonCode;

	if (!prSwRfb || !aucBSSID || !pu2ReasonCode) {
		DBGLOG(SAA, WARN, "Invalid parameters, ignore pkt!\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <1> locate the Disassociation Frame. */
	prDisassocFrame = (struct WLAN_DISASSOC_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Disassociation Frame. */
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
	    REASON_CODE_FIELD_LEN) {
		DBGLOG(SAA, LOUD, "Invalid DisAssoc packet length!");
		return WLAN_STATUS_FAILURE;
	}

	/* Check if this Disassoc Frame is coming from Target BSSID */
	if (UNEQUAL_MAC_ADDR(prDisassocFrame->aucBSSID, aucBSSID)) {
		DBGLOG(SAA, LOUD,
		       "Ignore Disassoc Frame from other BSS [" MACSTR "]\n",
		       MAC2STR(prDisassocFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <3> Parse the Fixed Fields of Deauthentication Frame Body. */
	WLAN_GET_FIELD_16(&prDisassocFrame->u2ReasonCode, &u2RxReasonCode);
	*pu2ReasonCode = u2RxReasonCode;

	return WLAN_STATUS_SUCCESS;

}				/* end of assocProcessRxDisassocFrame() */

#if CFG_SUPPORT_AAA
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Association Req
 *        frame and return a Status Code.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 * @param[out] pu2StatusCode     Pointer to store the Status Code for carried
 *                               in Association Response.
 *
 * @retval WLAN_STATUS_FAILURE   This is not the frame we should handle
 *                               at current state.
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t assocProcessRxAssocReqFrame(IN struct ADAPTER *prAdapter,
				     IN struct SW_RFB *prSwRfb,
				     OUT uint16_t *pu2StatusCode)
{
	struct WLAN_ASSOC_REQ_FRAME *prAssocReqFrame;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	struct IE_SSID *prIeSsid = (struct IE_SSID *)NULL;
	struct RSN_INFO_ELEM *prIeRsn = (struct RSN_INFO_ELEM *)NULL;
	struct IE_SUPPORTED_RATE_IOT *prIeSupportedRate =
	    (struct IE_SUPPORTED_RATE_IOT *)NULL;
	struct IE_EXT_SUPPORTED_RATE *prIeExtSupportedRate =
	    (struct IE_EXT_SUPPORTED_RATE *)NULL;
	struct WIFI_VAR *prWifiVar = NULL;
	uint8_t *pucIE, *pucIEStart;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;
	uint16_t u2StatusCode = STATUS_CODE_SUCCESSFUL;
	uint16_t u2RxFrameCtrl;
	uint16_t u2BSSBasicRateSet;
	uint8_t ucFixedFieldLength;
	u_int8_t fgIsUnknownBssBasicRate;
	uint32_t i;
	u_int8_t fgIsTKIP = FALSE;
	enum ENUM_BAND eBand = 0;
	struct RX_DESC_OPS_T *prRxDescOps;

	if (!prAdapter || !prSwRfb || !pu2StatusCode) {
		DBGLOG(SAA, WARN, "Invalid parameters, ignore pkt!\n");
		return WLAN_STATUS_FAILURE;
	}

	prWifiVar = &(prAdapter->rWifiVar);
	prRxDescOps = prAdapter->chip_info->prRxDescOps;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (prStaRec == NULL)
		return WLAN_STATUS_FAILURE;

#if CFG_TC10_FEATURE
	prStaRec->ucSupportedBand = 0;
#endif
	/* 4 <1> locate the Association Req Frame. */
	prAssocReqFrame = (struct WLAN_ASSOC_REQ_FRAME *)prSwRfb->pvHeader;

	/* WLAN_GET_FIELD_16(&prAssocReqFrame->u2FrameCtrl,
	 *    &u2RxFrameCtrl);
	 */
	u2RxFrameCtrl = prAssocReqFrame->u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */
	u2RxFrameCtrl &= MASK_FRAME_TYPE;

	/* 4 <2> Parse the Header of Association Req Frame. */
	if (u2RxFrameCtrl == MAC_FRAME_REASSOC_REQ)
		ucFixedFieldLength =
		    CAP_INFO_FIELD_LEN + LISTEN_INTERVAL_FIELD_LEN +
		    CURR_AP_ADDR_FIELD_LEN;
	else
		ucFixedFieldLength =
		    CAP_INFO_FIELD_LEN + LISTEN_INTERVAL_FIELD_LEN;

	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen)
		<= ucFixedFieldLength) {
		/* Length of this (re)association req is invalid, ignore it */
		return WLAN_STATUS_FAILURE;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(SAA, ERROR, "prBssInfo is null\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	/* Check if this Disassoc Frame is coming from Target BSSID */
	if (UNEQUAL_MAC_ADDR(prAssocReqFrame->aucBSSID, prBssInfo->aucBSSID))
		return WLAN_STATUS_FAILURE;	/* Just Ignore this MMPDU */

	if (u2RxFrameCtrl == MAC_FRAME_REASSOC_REQ) {
		prStaRec->fgIsReAssoc = TRUE;

		u2IELength = prSwRfb->u2PacketLen -
		    (uint16_t) OFFSET_OF(struct WLAN_REASSOC_REQ_FRAME,
					 aucInfoElem[0]);

		pucIEStart = pucIE =
		    ((struct WLAN_REASSOC_REQ_FRAME *)(prSwRfb->
						       pvHeader))->aucInfoElem;
	} else {
		prStaRec->fgIsReAssoc = FALSE;

		u2IELength = prSwRfb->u2PacketLen -
		    (uint16_t) OFFSET_OF(struct WLAN_ASSOC_REQ_FRAME,
					 aucInfoElem[0]);

		pucIEStart = pucIE = prAssocReqFrame->aucInfoElem;
	}

	/* 4 <3> Parse the Fixed Fields of Assoc Req Frame Body. */
	prStaRec->u2CapInfo = prAssocReqFrame->u2CapInfo;

#if 0
/* CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_HOTSPOT_PRIVACY_CHECK */
	if (prAdapter->fgIsP2PRegistered && IS_STA_P2P_TYPE(prStaRec)) {
		if (((prStaRec->u2CapInfo & CAP_INFO_PRIVACY)
		     && !kalP2PGetCipher(prAdapter->prGlueInfo))) {
			u2StatusCode = STATUS_CODE_CAP_NOT_SUPPORTED;
			DBGLOG(RSN, TRACE,
			       "STA Assoc req privacy bit check fail\n");
			return WLAN_STATUS_SUCCESS;
		}
	}
#endif

	prStaRec->u2ListenInterval = prAssocReqFrame->u2ListenInterval;
	prStaRec->ucPhyTypeSet = 0;

	/* Might be legacy client or p2p gc. */
	prStaRec->eStaType = STA_TYPE_LEGACY_CLIENT;

	/* 4 <4> Parse the IE of Assoc Req Frame Body. */
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if ((!prIeSsid) &&	/* NOTE(Kevin): Get SSID once */
			    (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID)) {
				prIeSsid = (struct IE_SSID *)pucIE;
			}
			break;

		case ELEM_ID_SUP_RATES:
			if ((!prIeSupportedRate)
			    && (IE_LEN(pucIE) <= RATE_NUM_SW))
				prIeSupportedRate = SUP_RATES_IOT_IE(pucIE);

			break;
#if CFG_TC10_FEATURE
		case ELEM_ID_SUP_OPERATING_CLASS:
			if (IE_LEN(pucIE) >= 2 &&
				IE_LEN(pucIE) <= 256) {
				uint8_t ucIdx;
				uint8_t ucIs2gSupport = 0;
				uint8_t ucIs5gSupport = 0;
				uint8_t ucIs6gSupport = 0;

				for (ucIdx = 0;
					ucIdx < (IE_LEN(pucIE)-1);
					ucIdx++) {
					if (SUP_OPERATING_CLASS_IE(pucIE)->
						ucSup[ucIdx] <= 87)
						ucIs2gSupport = 1;
					else if (SUP_OPERATING_CLASS_IE(pucIE)->
						ucSup[ucIdx] <= 130)
						ucIs5gSupport = 2;
					else if (SUP_OPERATING_CLASS_IE(pucIE)->
						ucSup[ucIdx] <= 179)
						ucIs6gSupport = 4;
					}
				prStaRec->ucSupportedBand =
					ucIs2gSupport + ucIs5gSupport +
					ucIs6gSupport;
			}
			break;
#endif
		case ELEM_ID_EXTENDED_SUP_RATES:
			if (!prIeExtSupportedRate)
				prIeExtSupportedRate = EXT_SUP_RATES_IE(pucIE);
			break;
		case ELEM_ID_HT_CAP:
			prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_HT;
			break;
		case ELEM_ID_VHT_CAP:
			prStaRec->ucPhyTypeSet |= PHY_TYPE_BIT_VHT;
			break;
		case ELEM_ID_RSN:
#if CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_HOTSPOT_PRIVACY_CHECK
			if (prAdapter->fgIsP2PRegistered
			    && IS_STA_IN_P2P(prStaRec)) {
				prIeRsn = RSN_IE(pucIE);
				rsnParserCheckForRSNCCMPPSK(prAdapter, prIeRsn,
							    prStaRec,
							    &u2StatusCode);
				if (u2StatusCode != STATUS_CODE_SUCCESSFUL) {
					*pu2StatusCode = u2StatusCode;
					return WLAN_STATUS_SUCCESS;
				}
			}
#endif
			break;
		case ELEM_ID_VENDOR:
			if (p2pFuncParseCheckForTKIPInfoElem(pucIE))
				fgIsTKIP = TRUE;

#if CFG_ENABLE_WIFI_DIRECT
			{
				if ((prAdapter->fgIsP2PRegistered)) {
					uint8_t ucOuiType = 0;

					p2pFuncParseCheckForP2PInfoElem
					    (prAdapter, pucIE, &ucOuiType);

					if (ucOuiType == VENDOR_OUI_TYPE_P2P) {
						DBGLOG(P2P, TRACE,
						       "Target Client is a P2P group client\n");
						prStaRec->eStaType =
						    STA_TYPE_P2P_GC;
					}
				}
			}
#endif
			break;
		case ELEM_ID_IBSS_PARAM_SET:
			/* Check IBSS parameter set length to avoid
			 * abnormal content
			 */
			if (IE_LEN(pucIE) != ELEM_MAX_LEN_IBSS_PARAMETER_SET) {
				*pu2StatusCode =
				    STATUS_CODE_UNSPECIFIED_FAILURE;
				DBGLOG(SAA, WARN,
				    "Invalid IBSS Parameter IE length!\n");
				return WLAN_STATUS_FAILURE;
			}
			break;
		case ELEM_ID_RESERVED:
#if (CFG_SUPPORT_802_11AX == 1)
			if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_CAP)
				prStaRec->ucPhyTypeSet |= PHY_TYPE_SET_802_11AX;
#endif
#if (CFG_SUPPORT_802_11BE == 1)
			if (IE_ID_EXT(pucIE) == EID_EXT_EHT_CAPS)
				prStaRec->ucPhyTypeSet |= PHY_TYPE_SET_802_11BE;
#endif
			break;

		default:
			for (i = 0;
			     i <
			     (sizeof(rxAssocReqIETable) /
			      sizeof(struct VERIFY_IE_ENTRY)); i++) {

				if (((IE_ID(pucIE)) ==
				     rxAssocReqIETable[i].ucElemID)
				    && (rxAssocReqIETable[i].pfnVarifyIE !=
					NULL)) {
					rxAssocReqIETable[i].pfnVarifyIE
					    (prAdapter, prSwRfb,
					     (struct IE_HDR *)pucIE,
					     &u2StatusCode);

					if (u2StatusCode !=
					    STATUS_CODE_SUCCESSFUL) {
						*pu2StatusCode = u2StatusCode;
						return WLAN_STATUS_SUCCESS;
					}
				}
			}

			break;
		}
	}			/* end of IE_FOR_EACH */

	/*
	 * According to TGn & TGac 4.2.44, AP should not bring HT/VHT Cap IE in
	 * the IE of Assoc resp, if the STA request to use TKIP cipher
	 */
	if (fgIsTKIP && !prWifiVar->ucApAllowHtVhtTkip)
		prStaRec->ucPhyTypeSet &= ~(PHY_TYPE_BIT_VHT | PHY_TYPE_BIT_HT);

	/* parsing for WMM related information (2010/12/21) */
	mqmProcessAssocReq(prAdapter, prSwRfb, pucIEStart, u2IELength);

	do {
		if (prIeSsid) {
			if (UNEQUAL_SSID
			    (prBssInfo->aucSSID, prBssInfo->ucSSIDLen,
			     prIeSsid->aucSSID, prIeSsid->ucLength)) {

				u2StatusCode = STATUS_CODE_UNSPECIFIED_FAILURE;
				break;
			}
		} else {
			u2StatusCode = STATUS_CODE_UNSPECIFIED_FAILURE;
			break;
		}

		prStaRec->u2OperationalRateSet = 0;
		prStaRec->u2BSSBasicRateSet = 0;

		if (!prIeSupportedRate) {
			DBGLOG(SAA, WARN, "Supported Rate not present!\n");
			u2StatusCode =
				STATUS_CODE_ASSOC_DENIED_RATE_NOT_SUPPORTED;
			break;
		}
		/* Ignore any Basic Bit */
		rateGetRateSetFromIEs(prIeSupportedRate,
		    prIeExtSupportedRate,
		    &prStaRec->u2OperationalRateSet,
		    &u2BSSBasicRateSet,
		    &fgIsUnknownBssBasicRate);

		if ((prBssInfo->
			 u2BSSBasicRateSet & prStaRec->u2OperationalRateSet)
			!= prBssInfo->u2BSSBasicRateSet) {
			u2StatusCode =
				STATUS_CODE_ASSOC_DENIED_RATE_NOT_SUPPORTED;
			DBGLOG(SAA, WARN, "Basic rate not supported!\n");
			break;
		}

		/* Accpet the Sta, update BSSBasicRateSet from Bss */
		prStaRec->u2BSSBasicRateSet =
			prBssInfo->u2BSSBasicRateSet;

		prStaRec->u2DesiredNonHTRateSet =
			(prStaRec->u2OperationalRateSet & RATE_SET_ALL_ABG);

		RX_STATUS_GET(
			prRxDescOps,
			eBand,
			get_rf_band,
			prSwRfb->prRxStatus);
		if (eBand == BAND_2G4) {
			if (prStaRec->u2OperationalRateSet &
				RATE_SET_OFDM)
				prStaRec->ucPhyTypeSet |=
					PHY_TYPE_BIT_ERP;
			if (prStaRec->u2OperationalRateSet &
				RATE_SET_HR_DSSS)
				prStaRec->ucPhyTypeSet |=
					PHY_TYPE_BIT_HR_DSSS;
		} else {	/* (BAND_5G == prBssDesc->eBande) */
			if (prStaRec->u2OperationalRateSet &
				RATE_SET_OFDM)
				prStaRec->ucPhyTypeSet |=
					PHY_TYPE_BIT_OFDM;
		}

		/* Update default Tx rate */
		nicTxUpdateStaRecDefaultRate(prAdapter, prStaRec);

#if CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_HOTSPOT_PRIVACY_CHECK
		if (prAdapter->fgIsP2PRegistered && IS_STA_IN_P2P(prStaRec)) {
			if (prIeRsn) {
				if (!kalP2PGetCipher
				    (prAdapter->prGlueInfo,
				     (uint8_t) prBssInfo->u4PrivateData)) {
					u2StatusCode =
					    STATUS_CODE_CIPHER_SUITE_REJECTED;
					break;
				}
			} else {
				/* prStaRec->rSecInfo.fgAllowOnly1x = FALSE; */
				/* if (kalP2PGetCipher(
				 *	prAdapter->prGlueInfo)) {
				 *      // Only Allow 1x
				 *        prStaRec->rSecInfo.fgAllowOnly1x =
				 *		TRUE;
				 *        break;
				 *      }
				 */
			}
		}
#endif

	} while (FALSE);

#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered && IS_STA_IN_P2P(prStaRec)) {
#if 1				/* ICS */
		{
			uint8_t *cp = (uint8_t *) &prAssocReqFrame->u2CapInfo;

			if (prStaRec->fgIsReAssoc)
				cp += 10;
			else
				cp += 4;
			if (prStaRec->pucAssocReqIe) {
				kalMemFree(prStaRec->pucAssocReqIe,
					   VIR_MEM_TYPE,
					   prStaRec->u2AssocReqIeLen);
				prStaRec->pucAssocReqIe = NULL;
			}
			prStaRec->u2AssocReqIeLen = u2IELength;
			if (u2IELength) {
				prStaRec->pucAssocReqIe =
					kalMemAlloc(u2IELength,
						VIR_MEM_TYPE);

				if (prStaRec->pucAssocReqIe) {
					kalMemCopy(prStaRec->pucAssocReqIe,
						cp, u2IELength);
				} else {
					DBGLOG(SAA, LOUD,
						"allocate memory for prStaRec->pucAssocReqIe failed!\n");
					return WLAN_STATUS_RESOURCES;
				}
			}
		}
#endif
		kalP2PUpdateAssocInfo(prAdapter->prGlueInfo,
				      (uint8_t *) &prAssocReqFrame->u2CapInfo,
				      u2IELength +
				      (prStaRec->fgIsReAssoc ? 10 : 4),
				      prStaRec->fgIsReAssoc,
				      prStaRec->ucBssIndex);
	}
#endif

	*pu2StatusCode = u2StatusCode;

	return WLAN_STATUS_SUCCESS;

}				/* end of assocProcessRxAssocReqFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to compose Common Information Elements for
 *        Association Response Frame.
 *
 * @param[in] prMsduInfo     Pointer to the composed MSDU_INFO_T.
 * @param[in] prBssInfo      Pointer to the BSS_INFO_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
assocBuildReAssocRespFrameCommonIEs(IN struct ADAPTER *prAdapter,
				    IN struct MSDU_INFO *prMsduInfo,
				    IN struct BSS_INFO *prBssInfo)
{
	uint8_t *pucBuffer;
	struct STA_RECORD *prStaRec;
	uint8_t ucSupRatesLen;
	uint8_t ucExtSupRatesLen;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	pucBuffer =
	    (uint8_t *) ((unsigned long)prMsduInfo->prPacket +
			 (unsigned long)prMsduInfo->u2FrameLength);

	if (prBssInfo->ucAllSupportedRatesLen > ELEM_MAX_LEN_SUP_RATES) {
		ucSupRatesLen = ELEM_MAX_LEN_SUP_RATES;
		ucExtSupRatesLen =
		    prBssInfo->ucAllSupportedRatesLen - ELEM_MAX_LEN_SUP_RATES;
	} else {
		ucSupRatesLen = prBssInfo->ucAllSupportedRatesLen;
		ucExtSupRatesLen = 0;
	}

	/* Fill the Supported Rates element. */
	if (ucSupRatesLen) {
		SUP_RATES_IE(pucBuffer)->ucId = ELEM_ID_SUP_RATES;
		SUP_RATES_IE(pucBuffer)->ucLength = ucSupRatesLen;
		kalMemCopy(SUP_RATES_IE(pucBuffer)->aucSupportedRates,
			   prBssInfo->aucAllSupportedRates, ucSupRatesLen);

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
		pucBuffer += IE_SIZE(pucBuffer);
	}

	/* Fill the Extended Supported Rates element. */
	if (ucExtSupRatesLen) {

		EXT_SUP_RATES_IE(pucBuffer)->ucId = ELEM_ID_EXTENDED_SUP_RATES;
		EXT_SUP_RATES_IE(pucBuffer)->ucLength = ucExtSupRatesLen;

		kalMemCopy(EXT_SUP_RATES_IE(pucBuffer)->aucExtSupportedRates,
			   &prBssInfo->aucAllSupportedRates[ucSupRatesLen],
			   ucExtSupRatesLen);

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}
}			/* end of assocBuildReAssocRespFrameCommonIEs() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the (Re)Association Response frame
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucBssid               Given BSSID.
 * @param[in] u2CapInfo              Capability Field of current BSS.
 * @param[in out] pu2PayloadLen      Return the length of the composed
 *                                   fixed fields
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
assocComposeReAssocRespFrameHeaderAndFF(IN struct STA_RECORD *prStaRec,
					IN uint8_t *pucBuffer,
					IN uint8_t aucBSSID[],
					IN uint16_t u2CapInfo,
					IN OUT uint16_t *pu2PayloadLen)
{
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame;
	u_int8_t fgIsReAssoc;

	uint16_t u2FrameCtrl;

	prAssocRspFrame = (struct WLAN_ASSOC_RSP_FRAME *)pucBuffer;
	fgIsReAssoc = prStaRec->fgIsReAssoc;

	/* 4 <1> Compose the frame header of the (Re)Association Request frame.
	 */
	/* Fill the Frame Control field. */
	if (fgIsReAssoc)
		u2FrameCtrl = MAC_FRAME_REASSOC_RSP;
	else
		u2FrameCtrl = MAC_FRAME_ASSOC_RSP;

	/* WLAN_SET_FIELD_16(&prAssocFrame->u2FrameCtrl, u2FrameCtrl); */
	prAssocRspFrame->u2FrameCtrl = u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the DA field with Target MAC Address. */
	COPY_MAC_ADDR(prAssocRspFrame->aucDestAddr, prStaRec->aucMacAddr);

	/* Fill the SA field with current BSSID. */
	COPY_MAC_ADDR(prAssocRspFrame->aucSrcAddr, aucBSSID);

	/* Fill the BSSID field with current BSSID. */
	COPY_MAC_ADDR(prAssocRspFrame->aucBSSID, aucBSSID);

	/* Clear the SEQ/FRAG_NO field(HW won't overide the FRAG_NO,
	 * so we need to clear it).
	 */
	prAssocRspFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's common fixed field part of
	 *       the (Re)Association Request  frame.
	 */
	/* Fill the Capability Information field. */
	/* WLAN_SET_FIELD_16(&prAssocFrame->u2CapInfo, u2CapInfo); */
	prAssocRspFrame->u2CapInfo = u2CapInfo;
	/* NOTE(Kevin): Optimized for ARM */

	/* WLAN_SET_FIELD_16(&prAssocFrame->u2StatusCode,
	 *	prStaRec->u2StatusCode);
	 */
	prAssocRspFrame->u2StatusCode = prStaRec->u2StatusCode;
	/* NOTE(Kevin): Optimized for ARM */

	/* WLAN_SET_FIELD_16(&prAssocFrame->u2AssocId,
	 *	((prStaRec->u2AssocId & AID_MASK) | AID_MSB));
	 */
	/* NOTE(Kevin): Optimized for ARM */
	prAssocRspFrame->u2AssocId =
	    ((prStaRec->u2AssocId & AID_MASK) | AID_MSB);

	*pu2PayloadLen =
	    (CAP_INFO_FIELD_LEN + STATUS_CODE_FIELD_LEN + AID_FIELD_LEN);
}		/* end of assocComposeReAssocRespFrameHeaderAndFF() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the (Re)Association Resp frame
 *
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 */
/*----------------------------------------------------------------------------*/
uint32_t assocSendReAssocRespFrame(IN struct ADAPTER *prAdapter,
				   IN struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;

	uint16_t u2PayloadLen;
	uint16_t u2EstimatedFrameLen;
	uint16_t u2EstimatedExtraIELen;
	u_int8_t fgIsReAssoc;
	uint32_t i;

	/* 4 <1> Allocate a PKT_INFO_T for Authentication Frame */
	fgIsReAssoc = prStaRec->fgIsReAssoc;

	/* Init with MGMT Header Length + Length of Fixed Fields
	 * + Common IE Length
	 */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
	    WLAN_MAC_MGMT_HEADER_LEN +
	    CAP_INFO_FIELD_LEN +
	    STATUS_CODE_FIELD_LEN +
	    AID_FIELD_LEN +
	    (ELEM_HDR_LEN + ELEM_MAX_LEN_SUP_RATES) + (ELEM_HDR_LEN +
				       (RATE_NUM_SW - ELEM_MAX_LEN_SUP_RATES));

	/* + Extra IE Length */
	u2EstimatedExtraIELen = 0;

	for (i = 0;
	     i <
	     (uint32_t) sizeof(txAssocRespIETable) /
	     (uint32_t) sizeof(struct APPEND_VAR_IE_ENTRY);
	     i++) {
		if (txAssocRespIETable[i].u2EstimatedFixedIELen != 0) {
			u2EstimatedExtraIELen +=
			    txAssocRespIETable[i].u2EstimatedFixedIELen;
		} else if (txAssocRespIETable[i].pfnCalculateVariableIELen !=
			   NULL) {
			u2EstimatedExtraIELen += (uint16_t)
			    txAssocRespIETable[i].pfnCalculateVariableIELen
			    (prAdapter, prStaRec->ucBssIndex, prStaRec);
		}

	}

	u2EstimatedFrameLen += u2EstimatedExtraIELen;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(AAA, WARN,
		       "No PKT_INFO_T for sending (Re)Assoc Response.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose (Re)Association Request frame header and fixed fields
	 *       in MSDU_INfO_T.
	 */
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	/* Compose Header and Fixed Field */
	assocComposeReAssocRespFrameHeaderAndFF(prStaRec,
			(uint8_t *) ((unsigned long)
			(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD),
			prBssInfo->aucBSSID, prBssInfo->u2CapInfo,
			&u2PayloadLen);

	/* 4 <3> Update information of MSDU_INFO_T */
	nicTxSetPktLifeTime(prMsduInfo, 100);
	nicTxSetPktRetryLimit(prMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);
	nicTxSetForceRts(prMsduInfo, TRUE);

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen,
		     aaaFsmRunEventTxDone, MSDU_RATE_MODE_AUTO);

	/* 4 <4> Compose the frame body's IEs of the (Re)Association Request
	 *       frame.
	 */
	assocBuildReAssocRespFrameCommonIEs(prAdapter, prMsduInfo, prBssInfo);

	/* 4 <5> Compose IEs in MSDU_INFO_T */

	/* Append IE */
	for (i = 0;
	     i <
	     sizeof(txAssocRespIETable) / sizeof(struct APPEND_VAR_IE_ENTRY);
	     i++) {
		if (txAssocRespIETable[i].pfnAppendIE)
			txAssocRespIETable[i].pfnAppendIE(prAdapter,
							  prMsduInfo);

	}

	DBGLOG(AAA, TRACE, "Dump assoc response frame\n");

	if (aucDebugModule[DBG_P2P_IDX] & DBG_CLASS_TRACE) {
		dumpMemory8((uint8_t *) prMsduInfo->prPacket,
			(uint32_t) prMsduInfo->u2FrameLength);
	}

#if CFG_SUPPORT_WFD
	/* TODO put WFD IE in assoc resp if driver will send assoc resp */

#endif

	/* TODO(Kevin): Also release the unused tail room of the composed MMPDU
	 */

	nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	/* 4 <6> Enqueue the frame to send this (Re)Association request frame.
	 */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(SAA, INFO,
			"Send Assoc Resp to " MACSTR ", Seq: %d, status: %d\n",
			MAC2STR(prStaRec->aucMacAddr),
			prMsduInfo->ucTxSeqNum,
			prStaRec->u2StatusCode);

	return WLAN_STATUS_SUCCESS;

}				/* end of assocSendReAssocRespFrame() */

/*-----------------------------------------------------------------------*/
/*!
 * @brief Get the non-wfa vendor ie length that was previously set
 *        by wpa_supplicant for association request frame.
 *
 * @param prAdapter    pointer to driver adapter
 *
 * @retval length of the non-wfa vendor ie
 */
/*-----------------------------------------------------------------------*/
uint16_t assoc_get_nonwfa_vend_ie_len(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *prConnSettings;

	if (!prAdapter || !prAdapter->prGlueInfo)
		return 0;

	prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);
	if (!prConnSettings)
		return 0;

	return prConnSettings->non_wfa_vendor_ie_len;
}

/*-----------------------------------------------------------------------*/
/*!
 * @brief Builds the non-wfa vendor specific ies into association
 *        request frame.
 *
 * @param prAdapter    pointer to driver adapter
 *        prMsduInfo   pointer to the msdu frame body
 *
 * @retval void
 */
/*-----------------------------------------------------------------------*/
void assoc_build_nonwfa_vend_ie(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex = 0;
	uint8_t *ptr = NULL;
	uint16_t len = 0;

	if (!prAdapter || !prMsduInfo)
		return;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);
	if (!prConnSettings)
		return;

	len = prConnSettings->non_wfa_vendor_ie_len;
	if (!len)
		return;

	ptr = (uint8_t *)prMsduInfo->prPacket +
		(uint16_t)prMsduInfo->u2FrameLength;
	kalMemCopy(ptr, prConnSettings->non_wfa_vendor_ie_buf,
			   len);
	prMsduInfo->u2FrameLength += len;
}

void assocGenerateMDIE(IN struct ADAPTER *prAdapter,
		       IN OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint8_t ucBssIndex = prMsduInfo->ucBssIndex;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	struct FT_IES *prFtIEs;
	struct GL_WPA_INFO *prWpaInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t *prMDIE = NULL;

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex))
		return;

	eAuthMode = aisGetAuthMode(prAdapter, ucBssIndex);
	prFtIEs = aisGetFtIe(prAdapter, ucBssIndex, FT_R1);
	prWpaInfo = aisGetWpaInfo(prAdapter, ucBssIndex);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	/* don't include MDIE in assoc request frame if auth mode is not FT
	 * related
	 */
	if (!rsnKeyMgmtFT(prBssInfo->u4RsnSelectedAKMSuite) &&
	    !(eAuthMode == AUTH_MODE_OPEN &&
	      prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED &&
	      prWpaInfo->u4AuthAlg == IW_AUTH_ALG_FT)) { /* Non-RSN FT */
		DBGLOG(SAA, ERROR, "Don't gen MDIE\n");
		return;
	}

	if (!prFtIEs) {
		DBGLOG(SAA, ERROR, "prFtIEs is null\n");
		return;
	}

	if (!prFtIEs->prMDIE) {
		struct BSS_DESC *prBssDesc =
			aisGetTargetBssDesc(prAdapter, ucBssIndex);

		if (prBssDesc)
			prMDIE = (uint8_t *) kalFindIeExtIE(
				ELEM_ID_MOBILITY_DOMAIN, 0,
				prBssDesc->aucIEBuf,
				prBssDesc->u2IELength);
	} else {
		prMDIE = (uint8_t *)prFtIEs->prMDIE;
	}

	if (!prMDIE) {
		DBGLOG(SAA, ERROR, "FT: can't find MDIE\n");
		return;
	}

	prMsduInfo->u2FrameLength += 5; /* IE size for MD IE is fixed, it's 5 */
	kalMemCopy(pucBuffer, prMDIE, 5);
	DBGDUMP_MEM8(SAA, INFO, "FT: Generate MDIE\n", pucBuffer, 5);
}

#endif /* CFG_SUPPORT_AAA */
