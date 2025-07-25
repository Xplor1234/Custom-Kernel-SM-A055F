// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if (CFG_SUPPORT_NAN == 1)

#include "precomp.h"
#include "nan_base.h"
#include "nan_data_engine.h"

/* for action frame subtypes */
/* #include "nan_rxm.h" */
/* #include "nan_sec.h" */

/* for NAN schedule hooks */
#include "nanScheduler.h"
#include "nan_sec.h"

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

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
static struct _APPEND_SUB_ATTR_ENTRY_T txServInfoSSITable[] = {
	/*    ATTR-ID	     fp for calc-var-len        fp for attr appending */
	{ NAN_ATTR_NDPE_SERVINFO_SUB_ATTR_TRANSPORT_PORT,
	  nanDataEngineNDPEPORTAttrLength, nanDataEngineNDPEPORTAttrAppend },
	{ NAN_ATTR_NDPE_SERVINFO_SUB_ATTR_PROTOCOL,
	  nanDataEngineNDPEProtocolAttrLength,
	  nanDataEngineNDPEProtocolAttrAppend },
	{ NAN_ATTR_NDPE_SERVINFO_SUB_ATTR_SPECINFO,
	  nanDataEngineNDPESpecAttrLength, nanDataEngineNDPESpecAttrAppend },
};

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static uint32_t nanNdpBufferNanAttrLists(struct ADAPTER *prAdapter,
					 uint8_t *pucNanAttrList,
					 uint16_t u2NanAttrListLength,
					 struct _NAN_NDP_INSTANCE_T *prNDP);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function manages to copy whole NAN attribute lists
 *           to a dynamically allocated buffer, as NAN-SEC may take use of
 *           it for integrity check
 *
 * \param[in] WLAN_STATUS_SUCCESS
 *            WLAN_STATUS_RESOURCES
 *
 * \return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_RESOURCES
 */
/*----------------------------------------------------------------------------*/
unsigned char
nanGetFeatureIsSigma(struct ADAPTER *prAdapter)
{
	return prAdapter->rWifiVar.fgNanIsSigma ||
		(prAdapter->rWifiVar.u4SwTestMode ==
			ENUM_SW_TEST_MODE_SIGMA_NAN);
}

static uint32_t
nanNdpBufferNanAttrLists(struct ADAPTER *prAdapter,
			 uint8_t *pucNanAttrList,
			 uint16_t u2NanAttrListLength,
			 struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prNDP) {
		DBGLOG(NAN, ERROR, "[%s] prNDP error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (prNDP->u2AttrListLength) {
		cnmMemFree(prAdapter, prNDP->pucAttrList);
		prNDP->u2AttrListLength = 0;
		prNDP->pucAttrList = NULL;
	}

	if (u2NanAttrListLength == 0)
		rStatus = WLAN_STATUS_SUCCESS;

	else {
		prNDP->pucAttrList = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
						 (uint32_t)u2NanAttrListLength);

		if (prNDP->pucAttrList) {
			/* allocation successful */
			prNDP->u2AttrListLength = u2NanAttrListLength;
			kalMemCopy(prNDP->pucAttrList, pucNanAttrList,
				   prNDP->u2AttrListLength);

			rStatus = WLAN_STATUS_SUCCESS;
		} else {
			/* insufficient size for allocation */
			DBGLOG(NAN, WARN,
			       "NAN Data Engine: NAN Attr allocation failure (%d)\n",
			       u2NanAttrListLength);

			/* @TODO: more error handling */
			prNDP->u2AttrListLength = 0;

			rStatus = WLAN_STATUS_RESOURCES;
		}
	}

	return rStatus;
}

static const char *attr_str(uint8_t attr)
{
	static const char * const nan_attr_string[
		NAN_ATTR_ID_TRANSMIT_POWER_ENVELOPE + 1] = {
		[NAN_ATTR_ID_MASTER_INDICATION] = "Master Indication",
		[NAN_ATTR_ID_CLUSTER] = "Cluster",
		[NAN_ATTR_ID_SERVICE_ID_LIST] = "Service ID List",
		[NAN_ATTR_ID_SERVICE_DESCRIPTOR] = "Service Descriptor",
		[NAN_ATTR_ID_NAN_CONNECTION_CAPABILITY] =
			"NAN Connection Capability",
		[NAN_ATTR_ID_WLAN_INFRASTRUCTURE] =
			"WLAN Infrastructure attribute",
		[NAN_ATTR_ID_P2P_OPERATION] = "P2P Operation",
		[NAN_ATTR_ID_IBSS] = "IBSS attribute",
		[NAN_ATTR_ID_MESH] = "Mesh attribute",
		[NAN_ATTR_ID_FSD] = "Further NAN Service Discovery",
		[NAN_ATTR_ID_FURTHER_AVAILABILITY_MAP] =
			"Further Availability Map",
		[NAN_ATTR_ID_COUNTRY_CODE] = "Country Code",
		[NAN_ATTR_ID_RANGING] = "Ranging",
		/* Cluster Discovery is intended to be carried in any Beacon
		 * frame other than a NAN Beacon and Probe Response frames
		 */
		[NAN_ATTR_ID_CLUSTER_DISCOVERY] = "Cluster Discovery",
		[NAN_ATTR_ID_SDEA] = "Service Descriptor Extended (SDEA)",
		[NAN_ATTR_ID_DEVICE_CAPABILITY] = "Device Capability",
		[NAN_ATTR_ID_NDP] = "NDP",
		[NAN_ATTR_ID_NMSG] = "NMSG",
		[NAN_ATTR_ID_NAN_AVAILABILITY] = "NAN Availability",
		[NAN_ATTR_ID_NDC] = "NDC",
		[NAN_ATTR_ID_NDL] = "NDL",
		[NAN_ATTR_ID_NDL_QOS] = "NDL QoS",
		[NAN_ATTR_ID_MULTICAST_SCHEDULE] = "Multicast Schedule",
		[NAN_ATTR_ID_UNALIGNED_SCHEDULE] = "Unaligned Schedule",
		[NAN_ATTR_ID_PAGING_UNICAST] = "Paging - Unicast",
		[NAN_ATTR_ID_PAGING_MULTICAST] = "Paging - Multicast",
		[NAN_ATTR_ID_RANGING_INFORMATION] = "Ranging Information",
		[NAN_ATTR_ID_RANGING_SETUP] = "Ranging Setup",
		[NAN_ATTR_ID_FTM_RANGING_REPORT] = "FTM Ranging Report",
		[NAN_ATTR_ID_ELEMENT_CONTAINER] = "Element Container",
		[NAN_ATTR_ID_EXT_WLAN_INFRASTRUCTURE] =
			"Ext WLAN Infrastructure",
		[NAN_ATTR_ID_EXT_P2P_OPERATION] = "Ext P2P Operation",
		[NAN_ATTR_ID_EXT_IBSS] = "Ext IBSS",
		[NAN_ATTR_ID_EXT_MESH] = "Ext Mesh",
		[NAN_ATTR_ID_CIPHER_SUITE_INFO] = "Cipher Suite Info (CSIA)",
		[NAN_ATTR_ID_SECURITY_CONTEXT_INFO] =
			"Security Context Info (SCIA)",
		[NAN_ATTR_ID_SHARED_KEY_DESCRIPTOR] =
			"Shared-Key Descriptor",
		[NAN_ATTR_ID_MULTICAST_SCHEDULE_CHANGE] =
			"Multicast Schedule Change",
		[NAN_ATTR_ID_MULTICAST_SCHEDULE_OWNER_CHANGE] =
			"Multicast Schedule Owner Change",
		[NAN_ATTR_ID_PUBLIC_AVAILABILITY] = "Public Availability",
		[NAN_ATTR_ID_SUBSCRIBE_SERVICE_ID_LIST] =
			"Subscriber Service ID List",
		[NAN_ATTR_ID_NDP_EXTENSION] = "NDP Extension",
		[NAN_ATTR_ID_DEVICE_CAPABILITY_EXT] =
			"Device Capability Extension",
		[NAN_ATTR_ID_NAN_IDENTITY_RESOLUTION] =
			"NAN Identity Resolution",
		[NAN_ATTR_ID_NAN_PAIRING_BOOTSTRAPPING] =
			"NAN Pairing Bootstrapping",
		[NAN_ATTR_ID_S3] = "S3",
		[NAN_ATTR_ID_TRANSMIT_POWER_ENVELOPE] =
			"Transmit Power Envelope",
	};

	if (attr == NAN_ATTR_ID_VENDOR_SPECIFIC)
		return "Vendor Specific";
	else if (attr <= NAN_ATTR_ID_TRANSMIT_POWER_ENVELOPE &&
		 nan_attr_string[attr])
		return nan_attr_string[attr];
	else
		return "";
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief        This utility function search through whole attr body list
 *               for specific Attribute ID
 *
 * \param[in]    pucAttrList
 *       [in]    u2Length
 *       [in]    ucAttrId
 *
 * \return   NULL: not found
 *           Non-NULL: pointer to specific ID
 *
 *   @TODO: this function may be better putting into nan_common.c or NAN RXM
 */
/*----------------------------------------------------------------------------*/
struct _NAN_ATTR_HDR_T *
nanRetrieveAttrById(uint8_t *pucAttrList, uint16_t u2Length,
		    uint8_t ucTargetAttrId) {
	struct _NAN_ATTR_HDR_T *prTargetAttr = NULL;
	uint8_t *pucPtr = pucAttrList;
	uint8_t ucAttrId;
	uint16_t u2RemainSize = u2Length;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!pucAttrList) {
		DBGLOG(NAN, ERROR, "pucAttrList error\n");
		return NULL;
	}

	if (u2Length <= 0) {
		DBGLOG(NAN, ERROR, "u2Length invalid\n");
		return NULL;
	}

	while (u2RemainSize >= OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody)) {

		ucAttrId = NAN_GET_U8(pucPtr);
		u2Length = NAN_GET_U16(pucPtr + 1);
		DBGLOG(NAN, DEBUG, "ucAttrId = %d (%s)\n", ucAttrId,
				attr_str(ucAttrId));

		if (ucAttrId == ucTargetAttrId) {
			prTargetAttr = (struct _NAN_ATTR_HDR_T *)pucPtr;
			/* break; */
		} else {
			pucPtr += (OFFSET_OF(struct _NAN_ATTR_HDR_T,
					     aucAttrBody) +
				   u2Length);

			if (u2RemainSize >
			    (uint16_t)(OFFSET_OF(struct _NAN_ATTR_HDR_T,
						aucAttrBody) +
				      u2Length)) {
				u2RemainSize -= (uint16_t)(
					OFFSET_OF(struct _NAN_ATTR_HDR_T,
						  aucAttrBody) +
					u2Length);
			} else {
				/* incomplete buffer */
				u2RemainSize = 0;
			}
		}

		if (ucAttrId == ucTargetAttrId)
			break;
	}

	return prTargetAttr;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief        Utility function to compose NAF header
 *
 * \param[in]
 *
 * \return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t nanDataEngineComposeNAFHeader(struct ADAPTER *prAdapter,
				       struct MSDU_INFO *prMsduInfo,
				       enum _NAN_ACTION_T eAction,
				       uint8_t *pucLocalMacAddr,
				       uint8_t *pucPeerMacAddr,
				       struct STA_RECORD *prStaRec)
{
	struct _NAN_ACTION_FRAME_T *prNAF = NULL;
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	prNAF = prMsduInfo->prPacket;

	/* MAC header */
	WLAN_SET_FIELD_16(&(prNAF->u2FrameCtrl), MAC_FRAME_ACTION);

	COPY_MAC_ADDR(prNAF->aucDestAddr, pucPeerMacAddr);
	COPY_MAC_ADDR(prNAF->aucSrcAddr, pucLocalMacAddr);
	COPY_MAC_ADDR(prNAF->aucClusterID,
		      nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_BAND0)
			      ->aucClusterId);

	prNAF->u2SeqCtrl = 0;

	/* action frame body */
	if (prStaRec && prStaRec->fgTransmitKeyExist == TRUE) {
		DBGLOG(NAN, DEBUG,
			"CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION\n");
		prNAF->ucCategory = CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION;
	} else {
		DBGLOG(NAN, DEBUG,
			"CATEGORY_PUBLIC_ACTION\n");
		prNAF->ucCategory = CATEGORY_PUBLIC_ACTION;
	}

	prNAF->ucAction = ACTION_PUBLIC_VENDOR_SPECIFIC;
	kalMemCopy(prNAF->aucOUI, aucOui, VENDOR_OUI_LEN);
	prNAF->ucOUItype = VENDOR_OUI_TYPE_NAN_NAF;
	prNAF->ucOUISubtype = (uint8_t)eAction;

	/* Append attr beginning from here */
	prMsduInfo->u2FrameLength =
		OFFSET_OF(struct _NAN_ACTION_FRAME_T, aucInfoContent);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief    RX Path - NDP attribute handler
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdpAttrUpdateNdp(struct ADAPTER *prAdapter, enum _NAN_ACTION_T eNanAction,
		    struct _NAN_ATTR_NDP_T *prAttrNDP,
		    struct _NAN_NDL_INSTANCE_T *prNDL,
		    struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2NDPSpecificInfoLen;
	uint8_t *pucNDPSpecificInfo;
	uint8_t *pucPivot;
	uint16_t u2CountLen;

	uint8_t *pu1TmpAddr;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prAttrNDP) {
		DBGLOG(NAN, ERROR, "[%s] prAttrNDP error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDP) {
		DBGLOG(NAN, ERROR, "[%s] prNDP error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 1. move pivot to optional field */
	pucPivot = &(prAttrNDP->ucPublishID);
	u2CountLen = OFFSET_OF(struct _NAN_ATTR_NDP_T, ucPublishID) -
		     OFFSET_OF(struct _NAN_ATTR_NDP_T, ucDialogToken);

	/* 1.1 length field check */
	if (prAttrNDP->u2Length < u2CountLen)
		return WLAN_STATUS_FAILURE;

	/* 2. parse mandatory fields */
	if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST) {
		/* check Type-Status field */
		if (prAttrNDP->ucType != NAN_ATTR_NDP_TYPE_REQUEST ||
		    prAttrNDP->ucStatus != NAN_ATTR_NDP_STATUS_CONTINUED) {
			DBGLOG(NAN, WARN,
			       "NAF Data Request: Unexpected NDP Type=%u Status=%u\n",
			       prAttrNDP->ucType, prAttrNDP->ucStatus);

			return WLAN_STATUS_FAILURE;
		}

		/* Update Initiator NDI */
		kalMemCopy(prNDP->aucPeerNDIAddr, prAttrNDP->aucInitiatorNDI,
			   MAC_ADDR_LEN);
		DBGLOG(NAN, DEBUG, "[%s] Update aucPeerNDIAddr:" MACSTR
				  " from aucInitiatorNDI\n",
		       __func__, MAC2STR(prNDP->aucPeerNDIAddr));

		/* NAN_CHK_PNT log message */
		nanLogNdiInfo(prNDP);

		/* NDP needs to load parameters through NDP attributes */
		prNDP->ucDialogToken = prAttrNDP->ucDialogToken;

		if (prAttrNDP->ucNDPControl &
		    NAN_ATTR_NDP_CTRL_CONFIRM_REQUIRED)
			prNDP->fgConfirmRequired = TRUE;

		else
			prNDP->fgConfirmRequired = FALSE;

		if (prAttrNDP->ucNDPControl &
		    NAN_ATTR_NDP_CTRL_SECURITY_PRESENT)
			prNDP->fgSecurityRequired = TRUE;

		else
			prNDP->fgSecurityRequired = FALSE;

	} else if (eNanAction == NAN_ACTION_DATA_PATH_RESPONSE) {
		/* check Type-Status field */
		if (prAttrNDP->ucType != NAN_ATTR_NDP_TYPE_RESPONSE) {
			DBGLOG(NAN, WARN,
			       "NAF Data Response: Unexpected NDP Type=%u Status=%u\n",
			       prAttrNDP->ucType, prAttrNDP->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucDialogToken != prNDP->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF Data Response: DialogToken Mismatch (%d:%d)\n",
			       prAttrNDP->ucDialogToken, prNDP->ucDialogToken);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucStatus ==
			   NAN_ATTR_NDP_STATUS_REJECTED) {
			DBGLOG(NAN, WARN,
			       "NAF Data Response: Being REJECTed\n");
			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucStatus == NAN_ATTR_NDP_STATUS_CONTINUED)
			prNDP->fgConfirmRequired = TRUE;

		/* Update responder NDI: dynamic parsing optional field */
		if (prAttrNDP->ucNDPControl &
		    NAN_ATTR_NDP_CTRL_PUBLISHID_PRESENT)
			pu1TmpAddr = &prAttrNDP->aucResponderNDI[0];
		else
			pu1TmpAddr = &prAttrNDP->ucPublishID;

		if (prAttrNDP->ucNDPControl &
		    NAN_ATTR_NDP_CTRL_RESP_NDI_PRESENT) {
			kalMemCopy(prNDP->aucPeerNDIAddr, pu1TmpAddr,
				   MAC_ADDR_LEN);
			if (prNDP->fgSecurityRequired)
				nanSecUpdatePeerNDI(prNDP, pu1TmpAddr);

			DBGLOG(NAN, DEBUG, "[%s] Update aucPeerNDIAddr:" MACSTR
					  " from aucResponderNDI\n",
			       __func__, MAC2STR(prNDP->aucPeerNDIAddr));

			/* NAN_CHK_PNT log message */
			nanLogNdiInfo(prNDP);
		} else {
			TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
		}

	} else if (eNanAction == NAN_ACTION_DATA_PATH_CONFIRM) {
		if (prAttrNDP->ucType != NAN_ATTR_NDP_TYPE_CONFIRM) {
			DBGLOG(NAN, WARN,
			       "NAF Data Confirm: Unexpected NDP Type=%u Status=%u\n",
			       prAttrNDP->ucType, prAttrNDP->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucDialogToken != prNDP->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF Data Confirm: DialogToken Mismatch (%d:%d)\n",
			       prAttrNDP->ucDialogToken, prNDP->ucDialogToken);
			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucStatus ==
			   NAN_ATTR_NDP_STATUS_REJECTED) {
			DBGLOG(NAN, WARN, "NAF Data Confirm: Being REJECTed\n");
			return WLAN_STATUS_FAILURE;
		}

	} else if (eNanAction == NAN_ACTION_DATA_PATH_KEY_INSTALLMENT) {
		if (prAttrNDP->ucType != NAN_ATTR_NDP_TYPE_SEC_INSTALL) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: Unexpected NDP Type=%u Status=%u\n",
			       prAttrNDP->ucType, prAttrNDP->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucDialogToken != prNDP->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: DialogToken Mismatch (%d:%d)\n",
			       prAttrNDP->ucDialogToken, prNDP->ucDialogToken);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDP->ucStatus ==
			   NAN_ATTR_NDP_STATUS_REJECTED) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: Being REJECTed\n");
			return WLAN_STATUS_FAILURE;
		}

	} else if (eNanAction == NAN_ACTION_DATA_PATH_TERMINATION) {
		if (prAttrNDP->ucType != NAN_ATTR_NDP_TYPE_TERMINATE) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: Unexpected NDP Type=%u Status=%u\n",
			       prAttrNDP->ucType, prAttrNDP->ucStatus);
			return WLAN_STATUS_FAILURE;
		} else /* no need to do more checks */
			return WLAN_STATUS_SUCCESS;
	} else {
		/* unexpected NDP attribute - ignore */
		return WLAN_STATUS_SUCCESS;
	}

	/* parameter checking - Initiator NDI */
	if ((prNDP->eNDPRole == NAN_PROTOCOL_RESPONDER &&
	     UNEQUAL_MAC_ADDR(prAttrNDP->aucInitiatorNDI,
			      prNDP->aucPeerNDIAddr)) ||
	    (prNDP->eNDPRole == NAN_PROTOCOL_INITIATOR &&
	     UNEQUAL_MAC_ADDR(prAttrNDP->aucInitiatorNDI,
			      prNDP->aucLocalNDIAddr))) {
		/* reject by sending NAF */
		prNDP->fgRejectPending = TRUE;
		prNDP->ucNDPSetupStatus = NAN_ATTR_NDP_STATUS_REJECTED;
		prNDP->ucReasonCode = NAN_REASON_CODE_INVALID_PARAMS;

		return WLAN_STATUS_FAILURE;
	}

	/* parsing optional fields through pucPivot */
	if (prAttrNDP->ucNDPControl & NAN_ATTR_NDP_CTRL_PUBLISHID_PRESENT) {
		if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST)
			prNDP->ucPublishId = NAN_GET_U8(pucPivot);
		else {
			DBGLOG(NAN, WARN,
			       "NAF Data Path [%d]: Should not carry Publish ID\n",
			       (uint8_t)eNanAction);
		}

		pucPivot += 1;
		u2CountLen += 1;
	}

	if (prAttrNDP->ucNDPControl & NAN_ATTR_NDP_CTRL_RESP_NDI_PRESENT) {
		if (eNanAction == NAN_ACTION_DATA_PATH_RESPONSE)
			nanDataUpdateNdpPeerNDI(prAdapter, prNDP, pucPivot);

		pucPivot += MAC_ADDR_LEN;
		u2CountLen += MAC_ADDR_LEN;
	}

	/* buffer SpecificInfo, later indicating host through DataIndication
	 * event
	 */
	if (prAttrNDP->ucNDPControl & NAN_ATTR_NDP_CTRL_SPECIFIC_INFO_PRESENT) {
		u2NDPSpecificInfoLen = prAttrNDP->u2Length - u2CountLen;
		pucNDPSpecificInfo = pucPivot;

		nanDataEngineUpdateAppInfo(
			prAdapter, prNDP, NAN_SERVICE_PROTOCOL_TYPE_GENERIC,
			u2NDPSpecificInfoLen, pucNDPSpecificInfo);
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief    RX Path - NDPE attribute handler
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t nanNdpeAttrUpdateNdp(struct ADAPTER *prAdapter,
		     enum _NAN_ACTION_T eNanAction,
		     struct _NAN_ATTR_NDPE_T *prAttrNDPE,
		     struct _NAN_NDL_INSTANCE_T *prNDL,
		     struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint16_t u2ContentLen;
	uint8_t *pucPivot;
	uint16_t u2CountLen;
	struct _NAN_ATTR_NDPE_GENERAL_TLV_T *prTLV;
	struct _NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T *prIPV6TLV = NULL;
	struct _NAN_ATTR_NDPE_SVC_INFO_TLV_T *prAppInfoTLV = NULL;
	struct _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T *prWFAAppInfoTLV = NULL;
	uint8_t *pu1TmpAddr;
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prAttrNDPE) {
		DBGLOG(NAN, ERROR, "[%s] prAttrNDPE error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDP) {
		DBGLOG(NAN, ERROR, "[%s] prNDP error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 1. move pivot to optional field */
	pucPivot = &(prAttrNDPE->ucPublishID);
	u2CountLen = OFFSET_OF(struct _NAN_ATTR_NDPE_T, ucPublishID) -
		     OFFSET_OF(struct _NAN_ATTR_NDPE_T, ucDialogToken);

	/* 1.1 length field check */
	if (prAttrNDPE->u2Length < u2CountLen)
		return WLAN_STATUS_FAILURE;

	/* 2. parse mandatory fields */
	if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST) {
		/* check Type-Status field */
		if (prAttrNDPE->ucType != NAN_ATTR_NDPE_TYPE_REQUEST ||
		    prAttrNDPE->ucStatus != NAN_ATTR_NDPE_STATUS_CONTINUED) {
			DBGLOG(NAN, WARN,
			       "NAF Data Request: Unexpected NDPE Type=%u Status=%u\n",
			       prAttrNDPE->ucType, prAttrNDPE->ucStatus);

			return WLAN_STATUS_FAILURE;
		}

		/* NDP needs to load parameters through NDPE attributes */
		prNDP->ucDialogToken = prAttrNDPE->ucDialogToken;

		/* Update Initiator NDI */
		kalMemCopy(prNDP->aucPeerNDIAddr, prAttrNDPE->aucInitiatorNDI,
			   MAC_ADDR_LEN);
		DBGLOG(NAN, DEBUG, "[%s] Update aucPeerNDIAddr:" MACSTR
				  " from aucInitiatorNDI\n",
		       __func__, MAC2STR(prNDP->aucPeerNDIAddr));

		/* NAN_CHK_PNT log message */
		nanLogNdiInfo(prNDP);

		if (prAttrNDPE->ucNDPEControl &
		    NAN_ATTR_NDPE_CTRL_CONFIRM_REQUIRED)
			prNDP->fgConfirmRequired = TRUE;

		else
			prNDP->fgConfirmRequired = FALSE;

		if (prAttrNDPE->ucNDPEControl &
		    NAN_ATTR_NDPE_CTRL_SECURITY_PRESENT)
			prNDP->fgSecurityRequired = TRUE;

		else
			prNDP->fgSecurityRequired = FALSE;

	} else if (eNanAction == NAN_ACTION_DATA_PATH_RESPONSE) {
		/* check Type-Status field */
		if (prAttrNDPE->ucType != NAN_ATTR_NDPE_TYPE_RESPONSE) {
			DBGLOG(NAN, WARN,
			       "NAF Data Response: Unexpected NDPE Type=%u Status=%u\n",
			       prAttrNDPE->ucType, prAttrNDPE->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDPE->ucDialogToken != prNDP->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF Data Response: DialogToken Mismatch (%d:%d)\n",
			       prAttrNDPE->ucDialogToken, prNDP->ucDialogToken);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDPE->ucStatus ==
			   NAN_ATTR_NDPE_STATUS_REJECTED) {
			DBGLOG(NAN, WARN,
			       "NDP Response: Being REJECTed, reason = %d\n",
			       prAttrNDPE->ucReasonCode);
			return WLAN_STATUS_FAILURE;
		}

		else if (prAttrNDPE->ucStatus == NAN_ATTR_NDPE_STATUS_CONTINUED)
			prNDP->fgConfirmRequired = TRUE;

		/* Update responder NDI: dynamic parsing optional field */
		if (prAttrNDPE->ucNDPEControl &
		    NAN_ATTR_NDP_CTRL_PUBLISHID_PRESENT)
			pu1TmpAddr = &prAttrNDPE->aucResponderNDI[0];
		else
			pu1TmpAddr = &prAttrNDPE->ucPublishID;

		if (prAttrNDPE->ucNDPEControl &
		    NAN_ATTR_NDP_CTRL_RESP_NDI_PRESENT) {
			kalMemCopy(prNDP->aucPeerNDIAddr, pu1TmpAddr,
				   MAC_ADDR_LEN);
			if (prNDP->fgSecurityRequired)
				nanSecUpdatePeerNDI(prNDP, pu1TmpAddr);

			DBGLOG(NAN, DEBUG, "[%s] Update aucPeerNDIAddr:" MACSTR
					  " from aucResponderNDI\n",
			       __func__, MAC2STR(prNDP->aucPeerNDIAddr));

			/* NAN_CHK_PNT log message */
			nanLogNdiInfo(prNDP);
		} else {
			TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
		}

	} else if (eNanAction == NAN_ACTION_DATA_PATH_CONFIRM) {
		if (prAttrNDPE->ucType != NAN_ATTR_NDPE_TYPE_CONFIRM) {
			DBGLOG(NAN, WARN,
			       "NAF Data Confirm: Unexpected NDPE Type=%u Status=%u\n",
			       prAttrNDPE->ucType, prAttrNDPE->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDPE->ucDialogToken != prNDP->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF Data Confirm: DialogToken Mismatch (%d:%d)\n",
			       prAttrNDPE->ucDialogToken, prNDP->ucDialogToken);
			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDPE->ucStatus ==
			   NAN_ATTR_NDPE_STATUS_REJECTED) {
			DBGLOG(NAN, WARN, "NAF Data Confirm: Being REJECTed\n");
			return WLAN_STATUS_FAILURE;
		}

	} else if (eNanAction == NAN_ACTION_DATA_PATH_KEY_INSTALLMENT) {
		if (prAttrNDPE->ucType != NAN_ATTR_NDPE_TYPE_SEC_INSTALL) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: Unexpected NDPE Type=%u Status=%u\n",
			       prAttrNDPE->ucType, prAttrNDPE->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDPE->ucDialogToken != prNDP->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: DialogToken Mismatch (%d:%d)\n",
			       prAttrNDPE->ucDialogToken, prNDP->ucDialogToken);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDPE->ucStatus ==
			   NAN_ATTR_NDPE_STATUS_REJECTED) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: Being REJECTed\n");
			return WLAN_STATUS_FAILURE;
		}

	} else if (eNanAction == NAN_ACTION_DATA_PATH_TERMINATION) {
		if (prAttrNDPE->ucType != NAN_ATTR_NDPE_TYPE_TERMINATE) {
			DBGLOG(NAN, WARN,
			       "NAF Data SecInstall: Unexpected NDPE Type=%u Status=%u\n",
			       prAttrNDPE->ucType, prAttrNDPE->ucStatus);
			return WLAN_STATUS_FAILURE;
		} else /* no need to do more checks */
			return WLAN_STATUS_SUCCESS;
	} else {
		/* unexpected NDPE attribute - ignore */
		return WLAN_STATUS_SUCCESS;
	}

	/* 2.1 parameter checking - Initiator NDI */
	if ((prNDP->eNDPRole == NAN_PROTOCOL_RESPONDER &&
	     UNEQUAL_MAC_ADDR(prAttrNDPE->aucInitiatorNDI,
			      prNDP->aucPeerNDIAddr)) ||
	    (prNDP->eNDPRole == NAN_PROTOCOL_INITIATOR &&
	     UNEQUAL_MAC_ADDR(prAttrNDPE->aucInitiatorNDI,
			      prNDP->aucLocalNDIAddr))) {
		/* reject by sending NAF */
		prNDP->fgRejectPending = TRUE;
		prNDP->ucNDPSetupStatus = NAN_ATTR_NDPE_STATUS_REJECTED;
		prNDP->ucReasonCode = NAN_REASON_CODE_INVALID_PARAMS;

		return WLAN_STATUS_FAILURE;
	}

	/* 3 parsing optional fields through pucPivot */
	if (prAttrNDPE->ucNDPEControl & NAN_ATTR_NDPE_CTRL_PUBLISHID_PRESENT) {
		if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST)
			prNDP->ucPublishId = NAN_GET_U8(pucPivot);
		else {
			DBGLOG(NAN, WARN,
			       "NAF Data Path [%d]: Should not carry Publish ID\n",
			       (uint8_t)eNanAction);
		}

		pucPivot += 1;
		u2CountLen += 1;
	}

	if (prAttrNDPE->ucNDPEControl &
	    NAN_ATTR_NDPE_CTRL_RESP_NDI_PRESENT) {
		if (eNanAction == NAN_ACTION_DATA_PATH_RESPONSE)
			nanDataUpdateNdpPeerNDI(prAdapter, prNDP, pucPivot);

		pucPivot += MAC_ADDR_LEN;
		u2CountLen += MAC_ADDR_LEN;
	}

	DBGLOG(NAN, DEBUG, "[%s] NDPE TLV len = %d\n", __func__,
		prAttrNDPE->u2Length - u2CountLen);
	dumpMemory8(pucPivot, prAttrNDPE->u2Length - u2CountLen);

	if (!nanGetFeatureIsSigma(prAdapter) &&
		prAttrNDPE->u2Length > u2CountLen) {
		if (prNDP->pucPeerAppInfo != NULL)
			cnmMemFree(prAdapter, prNDP->pucPeerAppInfo);
		prNDP->pucPeerAppInfo = cnmMemAlloc(
			prAdapter, RAM_TYPE_BUF,
			prAttrNDPE->u2Length - u2CountLen);
		if (prNDP->pucPeerAppInfo != NULL) {
			kalMemCopy(prNDP->pucPeerAppInfo,
			   pucPivot, prAttrNDPE->u2Length - u2CountLen);
			prNDP->u2PeerAppInfoLen =
				prAttrNDPE->u2Length - u2CountLen;
		}
	}

	/* 3.2 TLV parsing if there is still field */
	while (prAttrNDPE->u2Length > u2CountLen) {
		prTLV = (struct _NAN_ATTR_NDPE_GENERAL_TLV_T *)pucPivot;

		/* remained buffer length check */
		if (prAttrNDPE->u2Length - u2CountLen <
		    OFFSET_OF(struct _NAN_ATTR_NDPE_GENERAL_TLV_T, aucValue) +
			    prTLV->u2Length)
			break;

		switch (prTLV->ucType) {
		case NAN_ATTR_NDPE_TLV_TYPE_IPV6_LINK_LOCAL:
			prIPV6TLV =
				(struct _NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T *)
					prTLV;
			DBGLOG(NAN, DEBUG, "IPV6 link local\n");
			/* Update from the other side */
			if (prIPV6TLV->u2Length == 8) {
				prNDP->fgCarryIPV6 = TRUE;
				if (prNDP->fgIsInitiator == TRUE)
					kalMemCopy(prNDP->aucRspInterfaceId,
						   prIPV6TLV->aucInterfaceId,
						   IPV6MACLEN);
				else
					kalMemCopy(prNDP->aucInterfaceId,
						   prIPV6TLV->aucInterfaceId,
						   IPV6MACLEN);
			}

			break;
#ifdef NAN_UNUSED
		case NAN_ATTR_NDPE_TLV_TYPE_SERVICE_INFO:
			prAppInfoTLV =
			    (struct _NAN_ATTR_NDPE_SERVICE_INFO_TLV_T *) prTLV;

			if (prAppInfoTLV->u2Length > VENDOR_OUI_LEN) {
				if (kalMemCmp(prAppInfoTLV->aucOui, aucOui,
				VENDOR_OUI_LEN) == 0) {
					prWFAAppInfoTLV =
				    (struct _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T *)
				    prTLV;

					u2NDPSpecificInfoLen =
						prWFAAppInfoTLV->u2Length -
						(OFFSET_OF(struct
				 _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T, aucBody) -
						OFFSET_OF(struct
				 _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T, aucOui));

					nanDataEngineUpdateAppInfo(prAdapter,
						prNDP,
						prWFAAppInfoTLV
						->ucServiceProtocolType,
						u2NDPSpecificInfoLen,
						prWFAAppInfoTLV->aucBody);
				} else
					nanDataEngineUpdateOtherAppInfo(
						prAdapter,
						prNDP, prAppInfoTLV);
			}

			break;
#else
		case NAN_ATTR_NDPE_TLV_TYPE_SERVICE_INFO:
			prAppInfoTLV =
				(struct _NAN_ATTR_NDPE_SVC_INFO_TLV_T *)
					prTLV;
			DBGLOG(NAN, DEBUG,
			       "NAN_ATTR_NDPE_TLV_TYPE_SERVICE_INFO\n");
			if (prAppInfoTLV->u2Length > VENDOR_OUI_LEN) {
				if (kalMemCmp(prAppInfoTLV->aucOui, aucOui,
					      VENDOR_OUI_LEN) == 0) {
					prWFAAppInfoTLV =
					    (struct
					     _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T
					    *)prTLV;

					u2ContentLen =
					    prWFAAppInfoTLV->u2Length -
					    (OFFSET_OF(struct
					    _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T,
					    aucBody));

					nanDataEngineUpdateSSI(
						prAdapter, prNDP,
						prWFAAppInfoTLV
							->ucServiceProtocolType,
						u2ContentLen,
						prWFAAppInfoTLV->aucBody);
				} else
					nanDataEngineUpdateOtherAppInfo(
						prAdapter, prNDP, prAppInfoTLV);
			}
			break;
#endif
		default:
			break;
		}

		u2CountLen += OFFSET_OF(struct _NAN_ATTR_NDPE_GENERAL_TLV_T,
					aucValue) +
			      prTLV->u2Length;
		pucPivot += OFFSET_OF(struct _NAN_ATTR_NDPE_GENERAL_TLV_T,
				      aucValue) +
			    prTLV->u2Length;
	}

	return WLAN_STATUS_SUCCESS;
}

#if (CFG_SUPPORT_802_11AX == 1)
void nanNdpeAttrVendorSpecificHandler(struct ADAPTER *prAdapter,
		struct _NAN_ATTR_VENDOR_SPECIFIC_T *prAttrVendorSpecific,
		struct _NAN_NDL_INSTANCE_T *prNDL)
{
	uint8_t aucSsOui[] = {0x00, 0x00, 0xF0}, ucHeCapLen = 0;
	struct BSS_INFO *prBssInfo;

	prBssInfo = prAdapter->aprBssInfo[nanGetSpecificBssInfo(
#if (CFG_SUPPORT_NAN_DBDC == 1)
			prAdapter, NAN_BSS_INDEX_BAND1)->ucBssIndex];
#else
			prAdapter, NAN_BSS_INDEX_BAND0)->ucBssIndex];
#endif

	DBGLOG(NAN, DEBUG, "Receive NAN vendor specific IE\n");
	DBGLOG_MEM8(NAN, DEBUG, prAttrVendorSpecific,
		prAttrVendorSpecific->u2Length + ELEM_HDR_LEN);

#if (CFG_SUPPORT_NAN_11BE == 1)
	/* If Bit1 = 1 (EHT supported) and we did not set EHT CAP yet */
	if ((kalMemCmp(prAttrVendorSpecific->aucOui, aucSsOui, 3) &&
		prAttrVendorSpecific->ucVendorSpecificOuiType == 0x41) &&
		nanIsEhtEnable(prAdapter) &&
		!(prNDL->ucPhyTypeSet & PHY_TYPE_BIT_EHT)) {
		uint8_t ucEhtCapLen = 0;

		DBGLOG(NAN, DEBUG,
			"NAN peer supports EHT due to VendorSpecific IE\n");

		ucEhtCapLen = ehtRlmNANFillCapIE(
					prAdapter, prBssInfo,
					prNDL->aucIeEhtCap);

		if (ucEhtCapLen)
			prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_EHT;
	}
#endif

	/* Not SS OUI or OUI type not 0x33
	 * (VENDOR_COMPATIBILITY_OUI_TYPE declared in SS), skip
	 */
	if (kalMemCmp(prAttrVendorSpecific->aucOui, aucSsOui, 3) ||
		prAttrVendorSpecific->ucVendorSpecificOuiType != 0x33)
		return;

	/* If Bit0 = 1 (HE supported) and we did not set HE CAP yet */
	if ((prAttrVendorSpecific->aucVendorSpecificOuiData[0]
		& BIT(0) == 1) &&
		!(prNDL->ucPhyTypeSet & PHY_TYPE_BIT_HE)) {
		DBGLOG(NAN, DEBUG,
			"NAN peer supports HE due to VendorSpecific IE\n");

		ucHeCapLen = heRlmFillNANHECapIE(
					prAdapter, prBssInfo,
					prNDL->aucIeHeCap);

		if (ucHeCapLen)
			prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_HE;
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief        Attribute Parser for
 *               Schedule Request/Response/Confirm/Update Notification
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdlParseAttributes(struct ADAPTER *prAdapter,
		enum _NAN_ACTION_T eNanAction,
		uint8_t *pucNanAttrList, uint16_t u2NanAttrListLength,
		struct _NAN_NDL_INSTANCE_T *prNDL) {
	uint8_t *pucOffset, *pucEnd;
	struct _NAN_ATTR_HDR_T *prNanAttr;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!pucNanAttrList) {
		DBGLOG(NAN, ERROR, "[%s] pucNanAttrList error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 2. process through "buffered" attributes */
	pucOffset = pucNanAttrList;
	pucEnd = pucOffset + u2NanAttrListLength;

	while (pucOffset < pucEnd && rStatus == WLAN_STATUS_SUCCESS) {
		if (pucEnd - pucOffset <
		    OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody)) {
			/* insufficient length */
			break;
		}

		/* buffer pucAttr for later type-casting purposes */
		prNanAttr = (struct _NAN_ATTR_HDR_T *)pucOffset;
		if (pucEnd - pucOffset <
		    OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
			    prNanAttr->u2Length) {
			/* insufficient length */
			break;
		}

		/* move to next Attr */
		pucOffset += (OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
			      prNanAttr->u2Length);

		/* Parsing attributes */
		switch (prNanAttr->ucAttrId) {
		case NAN_ATTR_ID_DEVICE_CAPABILITY:
			rStatus = nanDeviceCapabilityAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_DEVICE_CAPABILITY_T *)
					prNanAttr,
				prNDL);
			break;

		case NAN_ATTR_ID_NAN_AVAILABILITY:
			rStatus = nanAvailabilityAttrHandler(prAdapter,
					eNanAction,
					(struct _NAN_ATTR_NAN_AVAILABILITY_T *)
					prNanAttr, prNDL, NULL);

			if (rStatus == WLAN_STATUS_PENDING) {
				prNDL->fgNeedRespondCounter = TRUE;
				DBGLOG(NAN, DEBUG,
				       "prNDL %u n=%u will respond counter\n",
				       prNDL->ucIndex, prNDL->ucNDPNum);
				rStatus = WLAN_STATUS_SUCCESS;
			}
			break;

		case NAN_ATTR_ID_NDC:
			rStatus = nanNDCAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_NDC_T *)prNanAttr, prNDL);
			break;

		case NAN_ATTR_ID_NDL:
			rStatus = nanNdlAttrUpdateNdl(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_NDL_T *)prNanAttr, prNDL);
			break;

		case NAN_ATTR_ID_NDL_QOS:
			rStatus = nanNdlQosAttrUpdateNdl(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_NDL_QOS_T *)prNanAttr, prNDL);
			break;

		case NAN_ATTR_ID_ELEMENT_CONTAINER:
			rStatus = nanElemContainerAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_ELEMENT_CONTAINER_T *)
					prNanAttr,
				prNDL);
			break;

		case NAN_ATTR_ID_UNALIGNED_SCHEDULE:
			rStatus = nanUnalignedAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_UNALIGNED_SCHEDULE_T *)
					prNanAttr,
				prNDL);
			break;

		default:
			break;
		}
	}

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, DEBUG, "Parse 0x%x attribute error\n",
		       prNanAttr->ucAttrId);
		prNDL->fgRejectPending = TRUE;
		prNDL->ucNDLSetupCurrentStatus = NAN_ATTR_NDL_STATUS_REJECTED;
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t nanNdlAttrUpdateNdl(struct ADAPTER *prAdapter,
		enum _NAN_ACTION_T eNanAction,
		struct _NAN_ATTR_NDL_T *prAttrNDL,
		struct _NAN_NDL_INSTANCE_T *prNDL)
{
	uint16_t u2NDLImmutableScheduleLen;
	uint8_t *pucNDLImmutableSchedule;
	uint8_t *pucPivot;
	uint16_t u2CountLen;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prAttrNDL) {
		DBGLOG(NAN, ERROR, "[%s] prAttrNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 1. move pivot to optional field */
	pucPivot = &(prAttrNDL->ucNDLPeerID);
	u2CountLen = OFFSET_OF(struct _NAN_ATTR_NDL_T, ucNDLPeerID) -
		     OFFSET_OF(struct _NAN_ATTR_NDL_T, ucDialogToken);

	/* 1.1 length field check */
	if (prAttrNDL->u2Length < u2CountLen)
		return WLAN_STATUS_FAILURE;

	/* 2. parse mandatory fields */
	if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST ||
	    eNanAction == NAN_ACTION_SCHEDULE_REQUEST) {
		/* check Type-Status field */
		if (prAttrNDL->ucType != NAN_ATTR_NDL_TYPE_REQUEST ||
		    prAttrNDL->ucStatus != NAN_ATTR_NDL_STATUS_CONTINUED) {
			DBGLOG(NAN, WARN,
			       "NAF(%d): Unexpected NDL Type=%u Status=%u\n",
			       eNanAction,
			       prAttrNDL->ucType, prAttrNDL->ucStatus);

			return WLAN_STATUS_FAILURE;
		}

		/* NDL needs to load parameters through NDL attributes */
		prNDL->ucDialogToken = prAttrNDL->ucDialogToken;

		/* TODO: check conflict with "existing" NDL */

		/* TODO: set to CONITNUED/ACCEPTED/REJECTED upon
		 * scheduler result
		 */
		prNDL->ucNDLSetupCurrentStatus = NAN_ATTR_NDL_STATUS_CONTINUED;

	} else if (eNanAction == NAN_ACTION_DATA_PATH_RESPONSE ||
		   eNanAction == NAN_ACTION_SCHEDULE_RESPONSE) {
		prNDL->fgIsCounter = FALSE;

		/* check Type-Status field */
		if (prAttrNDL->ucType != NAN_ATTR_NDL_TYPE_RESPONSE) {
			DBGLOG(NAN, WARN,
			       "NAF(%d): Unexpected NDL Type=%u Status=%u\n",
			       eNanAction,
			       prAttrNDL->ucType, prAttrNDL->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDL->ucStatus == NAN_ATTR_NDL_STATUS_REJECTED)
			return WLAN_STATUS_FAILURE;
		else if (prAttrNDL->ucStatus == NAN_ATTR_NDL_STATUS_CONTINUED) {
			if (prNDL->prOperatingNDP)
				prNDL->prOperatingNDP->fgConfirmRequired = TRUE;
			prNDL->fgIsCounter = TRUE;
		}

		if (prAttrNDL->ucDialogToken != prNDL->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF(%d): NDLDialogToken Mismatch (%d:%d)\n",
			       eNanAction, prAttrNDL->ucDialogToken,
			       prNDL->ucDialogToken);

			prNDL->ucDialogToken = prAttrNDL->ucDialogToken;
		}
		/* TODO: check if matching current operating parameters */

		/* TODO: set to ACCEPTED or REJECTED upon scheduler result */
		prNDL->ucNDLSetupCurrentStatus = NAN_ATTR_NDL_STATUS_ACCEPTED;
	} else if (eNanAction == NAN_ACTION_DATA_PATH_CONFIRM ||
		   eNanAction == NAN_ACTION_SCHEDULE_CONFIRM) {
		if (prAttrNDL->ucType != NAN_ATTR_NDL_TYPE_CONFIRM) {
			DBGLOG(NAN, WARN,
			       "NAF(%d): Unexpected NDL Type=%u Status=%u\n",
			       eNanAction,
			       prAttrNDL->ucType, prAttrNDL->ucStatus);

			return WLAN_STATUS_FAILURE;
		} else if (prAttrNDL->ucStatus == NAN_ATTR_NDL_STATUS_REJECTED)
			return WLAN_STATUS_FAILURE;
		else if (prAttrNDL->ucStatus == NAN_ATTR_NDL_STATUS_ACCEPTED) {
			prNDL->ucNDLSetupCurrentStatus =
				NAN_ATTR_NDL_STATUS_ACCEPTED;
		} else {
			/* TODO: check if matching current
			 * operating parameters
			 */

			/* TODO: inform scheduler for final result */
		}

		if (prAttrNDL->ucDialogToken != prNDL->ucDialogToken) {
			DBGLOG(NAN, WARN,
			       "NAF(%d): NDLDialogToken Mismatch (%d:%d)\n",
			       eNanAction, prAttrNDL->ucDialogToken,
			       prNDL->ucDialogToken);

			prNDL->ucDialogToken = prAttrNDL->ucDialogToken;
		}
	} else if (eNanAction == NAN_ACTION_SCHEDULE_UPDATE_NOTIFICATION) {
		/* TODO: check if matching current operating parameters */

		/* TODO: inform scheduler for schedule result */
	} else {
		/* unexpected NDL attribute - ignore */
		return WLAN_STATUS_SUCCESS;
	}

	/* parsing optional fields through pucPivot */
	if (prAttrNDL->b1PeerIdPresent) {
		if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST ||
		    eNanAction == NAN_ACTION_SCHEDULE_REQUEST)
			prNDL->ucPeerID = NAN_GET_U8(pucPivot);

		pucPivot += 1;
		u2CountLen += 1;
	}

	if (prAttrNDL->b1MaxIdlePeriodPresent) {
		if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST ||
		    eNanAction == NAN_ACTION_SCHEDULE_REQUEST)
			prNDL->u2MaximumIdlePeriod = NAN_GET_U16(pucPivot);

		pucPivot += 2;
		u2CountLen += 2;
	}

	if (prAttrNDL->b1ImmutableSchedPresent) {
		u2NDLImmutableScheduleLen = prAttrNDL->u2Length - u2CountLen;
		pucNDLImmutableSchedule = pucPivot;

		rStatus = nanSchedPeerUpdateImmuNdlScheduleList(
			prAdapter, prNDL->aucPeerMacAddr,
			(struct _NAN_SCHEDULE_ENTRY_T *)pucNDLImmutableSchedule,
			(uint32_t)u2NDLImmutableScheduleLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			prNDL->ucReasonCode =
				NAN_REASON_CODE_IMMUTABLE_UNACCEPTABLE;
			prNDL->ucNDLSetupCurrentStatus =
				NAN_ATTR_NDL_STATUS_REJECTED;
		}
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdlQosAttrUpdateNdl(struct ADAPTER *prAdapter,
		enum _NAN_ACTION_T eNanAction,
		struct _NAN_ATTR_NDL_QOS_T *prNdlQosAttr,
		struct _NAN_NDL_INSTANCE_T *prNDL) {
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prNdlQosAttr) {
		DBGLOG(NAN, ERROR, "[%s] prNdlQosAttr error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (eNanAction == NAN_ACTION_DATA_PATH_REQUEST ||
	    eNanAction == NAN_ACTION_DATA_PATH_RESPONSE ||
	    eNanAction ==
		    NAN_ACTION_DATA_PATH_CONFIRM /* only for NDL counter */
	    || eNanAction == NAN_ACTION_SCHEDULE_REQUEST ||
	    eNanAction == NAN_ACTION_SCHEDULE_RESPONSE ||
	    eNanAction == NAN_ACTION_SCHEDULE_CONFIRM ||
	    eNanAction == NAN_ACTION_SCHEDULE_UPDATE_NOTIFICATION) {
		prNDL->ucMinimumTimeSlot = prNdlQosAttr->ucMinTimeSlot;
		prNDL->u2MaximumLatency = prNdlQosAttr->u2MaxLatency;

		rStatus = nanSchedPeerUpdateQosAttr(prAdapter,
						    prNDL->aucPeerMacAddr,
						    (uint8_t *)prNdlQosAttr);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			prNDL->ucReasonCode = NAN_REASON_CODE_QOS_UNACCEPTABLE;
			prNDL->ucNDLSetupCurrentStatus =
				NAN_ATTR_NDL_STATUS_REJECTED;
		}
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanDeviceCapabilityAttrHandler(struct ADAPTER *prAdapter,
	       enum _NAN_ACTION_T eNanAction,
	       struct _NAN_ATTR_DEVICE_CAPABILITY_T *prDeviceCapabilityAttr,
	       struct _NAN_NDL_INSTANCE_T *prNDL)
{
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	return nanSchedPeerUpdateDevCapabilityAttr(
		prAdapter, prNDL->aucPeerMacAddr,
		(uint8_t *)prDeviceCapabilityAttr);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t nanAvailabilityAttrHandler(struct ADAPTER *prAdapter,
			enum _NAN_ACTION_T eNanAction,
			struct _NAN_ATTR_NAN_AVAILABILITY_T *prAvailabilityAttr,
			struct _NAN_NDL_INSTANCE_T *prNDL,
			struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint32_t rStatus;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = nanSchedPeerUpdateAvailabilityAttr(prAdapter,
		prNDL->aucPeerMacAddr, (uint8_t *)prAvailabilityAttr, prNDP);

	if (rStatus == WLAN_STATUS_PENDING) {
		DBGLOG(NAN, DEBUG, "Counter: fill committed with potential\n");
		prNDL->ucNDLSetupCurrentStatus = NAN_ATTR_NDL_STATUS_CONTINUED;
	} else if (rStatus != WLAN_STATUS_SUCCESS) {
		prNDL->ucReasonCode = NAN_REASON_CODE_INVALID_AVAILABILITY;
		prNDL->ucNDLSetupCurrentStatus = NAN_ATTR_NDL_STATUS_REJECTED;
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNDCAttrHandler(struct ADAPTER *prAdapter,
		enum _NAN_ACTION_T eNanAction,
		struct _NAN_ATTR_NDC_T *prNDCAttr,
		struct _NAN_NDL_INSTANCE_T *prNDL) {
	uint32_t rStatus;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = nanSchedPeerUpdateNdcAttr(prAdapter, prNDL->aucPeerMacAddr,
					    (uint8_t *)prNDCAttr);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		prNDL->ucReasonCode = NAN_REASON_CODE_INVALID_AVAILABILITY;
		prNDL->ucNDLSetupCurrentStatus = NAN_ATTR_NDL_STATUS_REJECTED;
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanElemContainerAttrHandler(
	struct ADAPTER *prAdapter, enum _NAN_ACTION_T eNanAction,
	struct _NAN_ATTR_ELEMENT_CONTAINER_T *prElemContainerAttr,
	struct _NAN_NDL_INSTANCE_T *prNDL) {
	uint8_t *pucIE;
	uint16_t u2AttrLength, u2IELength, u2Offset;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prElemContainerAttr) {
		DBGLOG(NAN, ERROR,
			"[%s] prElemContainerAttr error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	u2AttrLength = prElemContainerAttr->u2Length;
	u2IELength = u2AttrLength - 1;
	u2Offset = 0;
	pucIE = &(prElemContainerAttr->aucElements[0]);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_HT_CAP:
			prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_HT;
			if (IE_SIZE(pucIE) <= sizeof(struct IE_HT_CAP))
				COPY_IE(&(prNDL->rIeHtCap), pucIE);
			/* TODO: match with local capabilities
			 * for STA-REC params
			 */

			break;

		case ELEM_ID_VHT_CAP:
			if (prWifiVar->fgEnNanVHT == FALSE)
				break;
			prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_VHT;
			if (IE_SIZE(pucIE) <= sizeof(struct IE_VHT_CAP))
				COPY_IE(&(prNDL->rIeVhtCap), pucIE);
			/* TODO: match with local capabilities
			 * for STA-REC params
			 */

			break;


		case ELEM_ID_RESERVED:
#if (CFG_SUPPORT_802_11AX == 1)
			if (IE_ID_EXT(pucIE) == ELEM_EXT_ID_HE_CAP) {
				prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_HE;
				if (IE_SIZE(pucIE) > sizeof(prNDL->aucIeHeCap))
					DBGLOG(NAN, WARN,
						"HE_CAP IE_LEN (%d) exceed MAX LEN(%zu)!\n",
						IE_LEN(pucIE),
						sizeof(prNDL->aucIeHeCap));
				else
					COPY_IE(&(prNDL->aucIeHeCap), pucIE);
				/* TODO: match with local capabilities
				 * for STA-REC params
				 */
			}
#endif
#if (CFG_SUPPORT_NAN_11BE == 1)
			if ((IE_ID_EXT(pucIE) == ELEM_EXT_ID_EHT_CAPS) &&
				nanIsEhtEnable(prAdapter)) {
				DBGLOG(NAN, TRACE, "EHT IE_ID_EXT = %d\n",
					IE_ID_EXT(pucIE));
				prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_EHT;
				if (IE_SIZE(pucIE) >
					sizeof(prNDL->aucIeEhtCap))
					DBGLOG(NAN, WARN,
						"EHT_CAP (%d) exceed MAX LEN(%zu)!\n",
						IE_LEN(pucIE),
						sizeof(prNDL->aucIeEhtCap));
				else
					COPY_IE(&(prNDL->aucIeEhtCap), pucIE);
				/* TODO: match with local capabilities
				 * for STA-REC params
				 */
			}
#endif
			break;


		default:
			break;
		}
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanUnalignedAttrHandler(struct ADAPTER *prAdapter,
			enum _NAN_ACTION_T eNanAction,
			struct _NAN_ATTR_UNALIGNED_SCHEDULE_T *prUnalignedAttr,
			struct _NAN_NDL_INSTANCE_T *prNDL) {
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	rStatus = nanSchedPeerUpdateUawAttr(prAdapter, prNDL->aucPeerMacAddr,
					    (uint8_t *)prUnalignedAttr);

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanCipherSuiteAttrHandler(
	struct ADAPTER *prAdapter, enum _NAN_ACTION_T eNanAction,
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T *prCipherSuiteAttr,
	struct _NAN_NDL_INSTANCE_T *prNDL, struct _NAN_NDP_INSTANCE_T *prNDP) {
	struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCipherSuite;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	/* Suppose only 1 cipher during NDP creation */
	prCipherSuite = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
				prCipherSuiteAttr->aucCipherSuiteList;
	prNDP->u1RmtCipher = prCipherSuite->ucCipherSuiteID;
	prNDP->u1RmtCipherPId = prCipherSuite->ucPublishID;

	DBGLOG(NAN, DEBUG, "[%s] u1RmtCipher:%d, u1RmtCipherPId:%d\n", __func__,
	       prNDP->u1RmtCipher, prNDP->u1RmtCipherPId);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanSecContextAttrHandler(
	struct ADAPTER *prAdapter, enum _NAN_ACTION_T eNanAction,
	struct _NAN_ATTR_SECURITY_CONTEXT_INFO_T *prSecContextAttr,
	struct _NAN_NDL_INSTANCE_T *prNDL, struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucSecContextList;
	struct _NAN_SECURITY_CONTEXT_ID_T *prSecContext;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	/* Suppose only 1 unicast SCID during NDP creation */
	pucSecContextList = prSecContextAttr->aucSecurityContextIDList;
	prSecContext = (struct _NAN_SECURITY_CONTEXT_ID_T *)pucSecContextList;

	kalMemCopy(prNDP->au1RmtScid, prSecContext->aucSecurityContextID,
		   NAN_SCID_DEFAULT_LEN);
	prNDP->u1RmtScidPId = prSecContext->ucPublishID;

	DBGLOG(NAN, DEBUG, "[%s] u1RmtScidPId:%d\n", __func__,
	       prNDP->u1RmtScidPId);
	dumpMemory8(prNDP->au1RmtScid, NAN_SCID_DEFAULT_LEN);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return   WLAN_STATUS_SUCCESS: Correctly matched and/or updated
 *           WLAN_STATUS_FAILURE: Mismatch and/or parameter not acceptable
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanSharedKeyAttrHandler(
	struct ADAPTER *prAdapter, enum _NAN_ACTION_T eNanAction,
	struct _NAN_ATTR_SHARED_KEY_DESCRIPTOR_T *prAttrSharedKeyDescriptor,
	struct _NAN_NDL_INSTANCE_T *prNDL, struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2FullAttrLen;
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */

#if (ENABLE_NDP_UT_LOG == 1)
	DBGLOG(NAN, DEBUG, "[%s] Enter, u2RxSkdAttrOffset:%d\n", __func__,
	       prNDP->u2RxSkdAttrOffset);
#endif

	if (!prAttrSharedKeyDescriptor) {
		DBGLOG(NAN, ERROR,
			"[%s] prAttrSharedKeyDescriptor error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDP) {
		DBGLOG(NAN, ERROR, "[%s] prNDP error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	u2FullAttrLen = OFFSET_OF(struct _NAN_ATTR_SHARED_KEY_DESCRIPTOR_T,
				  ucPublishID) +
			prAttrSharedKeyDescriptor->u2Length;

	prNDP->u2KdeLen = u2FullAttrLen;

	/* prNDP->pucKdeInfo = (P_UINT_8) prAttrSharedKeyDescriptor; */
	prNDP->pucKdeInfo = prNDP->pucRxMsgBuf + NAN_CATEGORY_HDR_OFFSET +
			    prNDP->u2RxSkdAttrOffset;

	/* rStatus = nanSecRxKdeAttr(prNDP, eNanAction-4,
	 *		prNDP->u2KdeLen, prNDP->pucKdeInfo,
	 *		prNDP->u2AttrListLength, prNDP->pucAttrList);
	 */
	/* rStatus = nanSecRxKdeAttr(prNDP, NAN_ACTION_TO_MSG(eNanAction),
	 *		prNDP->u2KdeLen, prNDP->pucKdeInfo,
	 *		prNDP->u2RxMsgLen, prNDP->pucRxMsgBuf);
	 */

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief        Attribute Parser for
 *               Data Path Request/Response/Confirm/key_Install/Termination
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdpParseAttributes(struct ADAPTER *prAdapter,
		enum _NAN_ACTION_T eNanAction,
		uint8_t *pucNanAttrList, uint16_t u2NanAttrListLength,
		struct _NAN_NDL_INSTANCE_T *prNDL,
		struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucOffset, *pucEnd;
	struct _NAN_ATTR_HDR_T *prNanAttr;
	/* struct _NAN_ATTR_SHARED_KEY_DESCRIPTOR_T*
	 *		prAttrSharedKeyDescriptor;
	 * UINT_16 u2NDPSpecificInfoLen;
	 * PUINT_8 pucNDPSpecificInfo;
	 * UINT_16 u2NDLImmutableScheduleLen;
	 * PUINT_8 pucNDLImmutableSchedule;
	 */
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	unsigned char fgExistNDPE = FALSE;
	unsigned char fgExistNDP = FALSE;
	unsigned char fgExistSKD = FALSE;
	unsigned char fgExistCSID = FALSE;
	unsigned char fgExistSCID = FALSE;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!pucNanAttrList) {
		DBGLOG(NAN, ERROR, "[%s] pucNanAttrList error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDP) {
		DBGLOG(NAN, ERROR, "[%s] prNDP error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 1. always backup whole pucNanAttrList first
	 * for NAN-SEC & Data Response purposes
	 */
	if (nanNdpBufferNanAttrLists(prAdapter, pucNanAttrList,
				     u2NanAttrListLength,
				     prNDP) != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

	/* 2. process through "buffered" attributes */
	pucOffset = prNDP->pucAttrList;
	pucEnd = pucOffset + prNDP->u2AttrListLength;

	while (pucOffset < pucEnd && rStatus == WLAN_STATUS_SUCCESS) {
		if (pucEnd - pucOffset <
		    OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody)) {
			/* insufficient length */
			break;
		}

		/* buffer pucAttr for later type-casting purposes */
		prNanAttr = (struct _NAN_ATTR_HDR_T *)pucOffset;
		if (pucEnd - pucOffset <
		    OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
			    prNanAttr->u2Length) {
			/* insufficient length */
			break;
		}

		DBGLOG(NAN, TRACE,
		       "prNanAttr->ucAttrId=%u, prNanAttr->u2Length=%u\n",
		       prNanAttr->ucAttrId, prNanAttr->u2Length);
		/* move to next Attr */
		pucOffset += (OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
			      prNanAttr->u2Length);

		/* Parsing attributes */
		switch (prNanAttr->ucAttrId) {
		case NAN_ATTR_ID_NDP:
			fgExistNDP = TRUE;
			DBGLOG(NAN, ERROR, "[%s] NDP exist, fgExistNDPE= %d\n",
				__func__, fgExistNDPE);

			/* only parse NDP when support NDPE is turned off,
			 * or NDPE is not there
			 */
			if (fgExistNDPE == FALSE)
				rStatus = nanNdpAttrUpdateNdp(
					prAdapter, eNanAction,
					(struct _NAN_ATTR_NDP_T *)prNanAttr,
					prNDL, prNDP);
			break;

		case NAN_ATTR_ID_DEVICE_CAPABILITY:
			rStatus = nanDeviceCapabilityAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_DEVICE_CAPABILITY_T *)
					prNanAttr,
				prNDL);
			break;

		case NAN_ATTR_ID_NAN_AVAILABILITY:
			rStatus = nanAvailabilityAttrHandler(prAdapter,
					eNanAction,
					(struct _NAN_ATTR_NAN_AVAILABILITY_T *)
					prNanAttr, prNDL, prNDP);

			if (rStatus == WLAN_STATUS_PENDING) {
				prNDL->fgNeedRespondCounter = TRUE;
				DBGLOG(NAN, DEBUG,
				       "prNDL %u n=%u will respond counter\n",
				       prNDL->ucIndex, prNDL->ucNDPNum);
				rStatus = WLAN_STATUS_SUCCESS;
			}
			break;

		case NAN_ATTR_ID_NDC:
			rStatus = nanNDCAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_NDC_T *)prNanAttr, prNDL);
			break;

		case NAN_ATTR_ID_NDL:
			rStatus = nanNdlAttrUpdateNdl(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_NDL_T *)prNanAttr, prNDL);
			break;

		case NAN_ATTR_ID_NDL_QOS:
			rStatus = nanNdlQosAttrUpdateNdl(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_NDL_QOS_T *)prNanAttr, prNDL);

			break;

		case NAN_ATTR_ID_ELEMENT_CONTAINER:
			rStatus = nanElemContainerAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_ELEMENT_CONTAINER_T *)
					prNanAttr,
				prNDL);
			break;

		case NAN_ATTR_ID_UNALIGNED_SCHEDULE:
			rStatus = nanUnalignedAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_UNALIGNED_SCHEDULE_T *)
					prNanAttr,
				prNDL);
			break;

		case NAN_ATTR_ID_CIPHER_SUITE_INFO:
			rStatus = nanCipherSuiteAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
					prNanAttr,
				prNDL, prNDP);
			fgExistCSID = TRUE;
			break;

		case NAN_ATTR_ID_SECURITY_CONTEXT_INFO:
			rStatus = nanSecContextAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_SECURITY_CONTEXT_INFO_T *)
					prNanAttr,
				prNDL, prNDP);
			fgExistSCID = TRUE;
			break;

		case NAN_ATTR_ID_SHARED_KEY_DESCRIPTOR:
			prNDP->u2RxSkdAttrOffset =
				(uint8_t *)prNanAttr - prNDP->pucAttrList;
			rStatus = nanSharedKeyAttrHandler(
				prAdapter, eNanAction,
				(struct _NAN_ATTR_SHARED_KEY_DESCRIPTOR_T *)
					prNanAttr,
				prNDL, prNDP);
			fgExistSKD = TRUE;
			break;
		case NAN_ATTR_ID_NDP_EXTENSION:
			fgExistNDPE = TRUE;
			/* only parse NDPE if option is turned on */

			DBGLOG(NAN, DEBUG, "[%s] NDPE exist, fgExistNDP = %d\n",
			       __func__, fgExistNDP);
			rStatus = nanNdpeAttrUpdateNdp(prAdapter, eNanAction,
				(struct _NAN_ATTR_NDPE_T *)prNanAttr,
				prNDL, prNDP);
			break;
#if (CFG_SUPPORT_802_11AX == 1)
		case NAN_ATTR_ID_VENDOR_SPECIFIC:
			nanNdpeAttrVendorSpecificHandler(prAdapter,
				(struct _NAN_ATTR_VENDOR_SPECIFIC_T *)prNanAttr,
				prNDL);
			break;
#endif
		default:
			break;
		}
	}

	if (fgExistSKD) {
		rStatus = nanSecRxKdeAttr(prNDP, NAN_ACTION_TO_MSG(eNanAction),
					  prNDP->u2KdeLen, prNDP->pucKdeInfo,
					  prNDP->u2RxMsgLen,
					  prNDP->pucRxMsgBuf);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			switch (eNanAction) {
			case NAN_ACTION_DATA_PATH_REQUEST:
				nanLogFailRxReqStr("key_mismatch");
				break;
			case NAN_ACTION_DATA_PATH_RESPONSE:
				nanLogFailRxRespStr("key_mismatch");
				break;
			case NAN_ACTION_DATA_PATH_CONFIRM:
				nanLogFailRxConfmStr("key_mismatch");
				break;
			case NAN_ACTION_DATA_PATH_KEY_INSTALLMENT:
				nanLogFailRxKeyInstlStr("key_mismatch");
				break;
			default:
				break;
			}
		}
	}

	prNDP->fgSupportNDPE =
		nanGetFeaturePeerNDPE(prAdapter, prNDL->aucPeerMacAddr);

	if (fgExistCSID) {
		if (prNDP->u1RmtCipherPId != prNDP->ucPublishId)
			rStatus = WLAN_STATUS_FAILURE;
		else
			prNDP->ucCipherType = prNDP->u1RmtCipher;
	}

	if (fgExistSCID) {
		if (prNDP->u1RmtScidPId != prNDP->ucPublishId)
			rStatus = WLAN_STATUS_FAILURE;
		else
			kalMemCopy(prNDP->au1Scid, prNDP->au1RmtScid,
				   NAN_SCID_DEFAULT_LEN);
	}

	if (rStatus != WLAN_STATUS_SUCCESS) {
		if (prNDP->ucNDPSetupStatus == NAN_ATTR_NDP_STATUS_CONTINUED ||
		    prNDP->ucNDPSetupStatus == NAN_ATTR_NDP_STATUS_ACCEPTED) {
			if (prNDL->ucNDLSetupCurrentStatus ==
			    NAN_ATTR_NDL_STATUS_REJECTED) {

				if (prNDP->fgRejectPending == FALSE)
					prNDP->fgRejectPending = TRUE;

				prNDP->ucNDPSetupStatus =
					NAN_ATTR_NDP_STATUS_REJECTED;
				prNDP->ucReasonCode =
					NAN_REASON_CODE_NDL_UNACCEPTABLE;
			}
		} else if (prNDL->ucNDLSetupCurrentStatus ==
				   NAN_ATTR_NDL_STATUS_CONTINUED ||
			   prNDL->ucNDLSetupCurrentStatus ==
				   NAN_ATTR_NDL_STATUS_ACCEPTED) {
			if (prNDP->ucNDPSetupStatus ==
			    NAN_ATTR_NDP_STATUS_REJECTED) {
				prNDL->ucNDLSetupCurrentStatus =
					NAN_ATTR_NDL_STATUS_REJECTED;
				prNDL->ucReasonCode =
					NAN_REASON_CODE_NDP_REJECTED;
			}
		}
	}

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineNDPAttrLength(struct ADAPTER *prAdapter,
			   struct _NAN_NDL_INSTANCE_T *prNDL,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2AttrLength;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	/* Schedule Req/Resp/Confirm/UpdateNotification condition */
	if (prNDL != NULL && prNDP == NULL)
		return 0;

	u2AttrLength = OFFSET_OF(struct _NAN_ATTR_NDP_T, ucPublishID);

	if (prNDP != NULL) {
		if (nanDataEngineNDPECheck(prAdapter, prNDP->fgSupportNDPE)) {
			DBGLOG(NAN, DEBUG, "[%s] NDPE instead\n",
				__func__);
			return 0;
		}

		u2AttrLength += prNDP->u2AppInfoLen;

		switch (prNDP->eCurrentNDPProtocolState) {
		case NDP_INITIATOR_TX_DP_REQUEST:
			u2AttrLength++; /* for Publish ID */
			return u2AttrLength;

		case NDP_INITIATOR_TX_DP_CONFIRM:
			/* refer to Table.28 - don't carry NDP
			 * for NDL Counter Reject
			 * avoid prNDL == NULL for Coverity Check
			 */
			if (prNDL != NULL && prNDL->ucNDLSetupCurrentStatus ==
			    NAN_ATTR_NDL_STATUS_REJECTED)
				return 0;
			return u2AttrLength;

		case NDP_RESPONDER_TX_DP_RESPONSE:
			if (prNDP->ucNDPSetupStatus ==
				    NAN_ATTR_NDP_STATUS_CONTINUED ||
			    prNDP->ucNDPSetupStatus ==
				    NAN_ATTR_NDP_STATUS_ACCEPTED)
				u2AttrLength +=
					MAC_ADDR_LEN; /* for Responder NDI */

			return u2AttrLength;

		case NDP_RESPONDER_TX_DP_SECURITY_INSTALL:
			return u2AttrLength;

		case NDP_TX_DP_TERMINATION:
			return u2AttrLength;

		default:
			return 0;
		}
	} else
		return u2AttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDP ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNDPAttrAppend(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo,
			   struct _NAN_NDL_INSTANCE_T *prNDL,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2AttrLength;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "[%s] prMsduInfo error\n", __func__);
		return;
	}

	u2AttrLength = nanDataEngineNDPAttrLength(prAdapter, prNDL, prNDP);

	if (u2AttrLength != 0) {
		nanDataEngineNDPAttrAppendImpl(prAdapter, prMsduInfo, prNDL,
					       prNDP, NULL, 0, 0);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDP ATTR Implementation
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNDPAttrAppendImpl(struct ADAPTER *prAdapter,
	       struct MSDU_INFO *prMsduInfo,
	       struct _NAN_NDL_INSTANCE_T *prNDL,
	       struct _NAN_NDP_INSTANCE_T *prNDP,
	       struct _NAN_ATTR_NDP_T *prPeerAttrNDP,
	       uint8_t ucTypeStatus, uint8_t ucReasonCode)
{
	struct _NAN_ATTR_NDP_T *prAttrNDP = NULL;
	uint16_t u2AttrLength;
	uint8_t *pucOffset;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "[%s] prMsduInfo error\n", __func__);
		return;
	}

	if (prNDP == NULL && prPeerAttrNDP == NULL) {
		DBGLOG(NAN, ERROR,
		       "%s(): unexpected condition: invalid parameters\n",
		       __func__);
		return;
	}

	prAttrNDP = (struct _NAN_ATTR_NDP_T *)((uint8_t *)prMsduInfo->prPacket +
					       prMsduInfo->u2FrameLength);

	u2AttrLength = nanDataEngineNDPAttrLength(prAdapter, prNDL, prNDP);

	if (u2AttrLength != 0) {
		prAttrNDP->ucAttrId = NAN_ATTR_ID_NDP;
		prAttrNDP->u2Length =
			u2AttrLength -
			OFFSET_OF(struct _NAN_ATTR_NDP_T, ucDialogToken);

		if (prNDP == NULL) {
			prAttrNDP->ucDialogToken = prPeerAttrNDP->ucDialogToken;
			prAttrNDP->ucTypeStatus = ucTypeStatus;
			prAttrNDP->ucReasonCode = ucReasonCode;
			COPY_MAC_ADDR(prAttrNDP->aucInitiatorNDI,
				      prPeerAttrNDP->aucInitiatorNDI);
			prAttrNDP->ucNDPID = prPeerAttrNDP->ucNDPID;
			prAttrNDP->ucNDPControl = 0;
		} else {
			prAttrNDP->ucDialogToken = prNDP->ucDialogToken;
			prAttrNDP->ucTypeStatus = prNDP->ucTxNextTypeStatus;
			prAttrNDP->ucReasonCode = prNDP->ucReasonCode;

			if (prNDP->eNDPRole == NAN_PROTOCOL_INITIATOR)
				COPY_MAC_ADDR(prAttrNDP->aucInitiatorNDI,
					      prNDP->aucLocalNDIAddr);

			else
				COPY_MAC_ADDR(prAttrNDP->aucInitiatorNDI,
					      prNDP->aucPeerNDIAddr);

			prAttrNDP->ucNDPID = prNDP->ucNDPID;

			prAttrNDP->ucNDPControl = 0;
			if (prNDP->fgConfirmRequired)
				prAttrNDP->ucNDPControl |=
					NAN_ATTR_NDP_CTRL_CONFIRM_REQUIRED;
			if (prNDP->fgSecurityRequired)
				prAttrNDP->ucNDPControl |=
					NAN_ATTR_NDP_CTRL_SECURITY_PRESENT;
			if (prNDP->eCurrentNDPProtocolState ==
			    NDP_INITIATOR_TX_DP_REQUEST)
				prAttrNDP->ucNDPControl |=
					NAN_ATTR_NDP_CTRL_PUBLISHID_PRESENT;
			if (prNDP->eCurrentNDPProtocolState ==
			    NDP_RESPONDER_TX_DP_RESPONSE) {
				if (prNDP->ucNDPSetupStatus ==
				    NAN_ATTR_NDP_STATUS_CONTINUED ||
				    prNDP->ucNDPSetupStatus ==
					NAN_ATTR_NDP_STATUS_ACCEPTED) {
					prAttrNDP->ucNDPControl |=
					    NAN_ATTR_NDP_CTRL_RESP_NDI_PRESENT;
				}
			}
			if (prNDP->u2AppInfoLen > 0)
				prAttrNDP->ucNDPControl |=
					NAN_ATTR_NDP_CTRL_SPECIFIC_INFO_PRESENT;

			/* start to fill option field */
			pucOffset = &(prAttrNDP->ucPublishID);
			if (prAttrNDP->ucNDPControl &
			    NAN_ATTR_NDP_CTRL_PUBLISHID_PRESENT) {
				*pucOffset = prNDP->ucPublishId;
				pucOffset++;
			}

			if (prAttrNDP->ucNDPControl &
			    NAN_ATTR_NDP_CTRL_RESP_NDI_PRESENT) {
				if (prNDP->eNDPRole == NAN_PROTOCOL_INITIATOR)
					COPY_MAC_ADDR(pucOffset,
						      prNDP->aucPeerNDIAddr);

				else
					COPY_MAC_ADDR(pucOffset,
						      prNDP->aucLocalNDIAddr);
				pucOffset += MAC_ADDR_LEN;
			}

			if (prAttrNDP->ucNDPControl &
			    NAN_ATTR_NDP_CTRL_SPECIFIC_INFO_PRESENT) {
				kalMemCopy(pucOffset, prNDP->pucAppInfo,
					   prNDP->u2AppInfoLen);
				pucOffset += prNDP->u2AppInfoLen;
			}
		}

		/* update payload length */
		prMsduInfo->u2FrameLength += u2AttrLength;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDL ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineNDLAttrLength(struct ADAPTER *prAdapter,
			   struct _NAN_NDL_INSTANCE_T *prNDL,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	unsigned char fgGenerateNDL = FALSE;
	uint16_t u2AttrLen = 0;

	uint8_t *pucScheduleList = NULL;
	uint32_t u4ScheduleListLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL == NULL)
		u2AttrLen = OFFSET_OF(struct _NAN_ATTR_NDL_T, ucNDLPeerID);

	else if (prNDP == NULL) {
		/* SCHEDULE REQUEST/RESPONSE/CONFIRM conditions */
		switch (prNDL->eCurrentNDLMgmtState) {
		case NDL_INITIATOR_TX_SCHEDULE_REQUEST:
		case NDL_RESPONDER_TX_SCHEDULE_RESPONSE:
		case NDL_INITIATOR_TX_SCHEDULE_CONFIRM:
			fgGenerateNDL = TRUE;
			break;

		default:
			break;
		}
	} else {
		switch (prNDP->eCurrentNDPProtocolState) {
		case NDP_INITIATOR_TX_DP_REQUEST:
			fgGenerateNDL = TRUE;
			break;

		case NDP_INITIATOR_TX_DP_CONFIRM:
			if (prNDP->fgConfirmRequired == TRUE)
				fgGenerateNDL = FALSE;

			if (prNDL->ucNDLSetupCurrentStatus ==
				 NAN_ATTR_NDL_STATUS_ACCEPTED &&
				 prNDL->fgIsCounter)
				fgGenerateNDL = TRUE;
			else if (prNDL->ucNDLSetupCurrentStatus ==
				 NAN_ATTR_NDL_STATUS_CONTINUED) {
				/* NDL Counter */
				fgGenerateNDL = TRUE;
			} else
				fgGenerateNDL = FALSE;

			/* Sigma 5.3.2 must pass with NDL attr */
			if (nanGetFeatureIsSigma(prAdapter))
				fgGenerateNDL = TRUE;

			break;
		case NDP_RESPONDER_TX_DP_RESPONSE:
			fgGenerateNDL = TRUE;
			break;

		default:
			break;
		}
#ifdef NAN_UNUSED
		if (prNDL->fgScheduleEstablished == TRUE)
			fgGenerateNDL = FALSE;
#endif
	}

	if (fgGenerateNDL == TRUE) {
		u2AttrLen = OFFSET_OF(struct _NAN_ATTR_NDL_T, ucNDLPeerID);

		if (prNDL->fgPagingRequired)
			u2AttrLen++; /* NDL Peer ID */

		if (prNDL->u2MaximumIdlePeriod > 0)
			u2AttrLen += 2; /* Max Idle Period */

		/* Immutable Schedule Entry List */
		nanSchedNegoGetImmuNdlScheduleList(prAdapter, &pucScheduleList,
							&u4ScheduleListLength);

		if (u4ScheduleListLength > 0) {
			prNDL->fgCarryImmutableSchedule = TRUE;
			u2AttrLen += u4ScheduleListLength;
		} else
			prNDL->fgCarryImmutableSchedule = FALSE;
	}

	return u2AttrLen;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDL ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNDLAttrAppend(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo,
			   struct _NAN_NDL_INSTANCE_T *prNDL,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2AttrLength;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "[%s] prMsduInfo error\n", __func__);
		return;
	}

	u2AttrLength = nanDataEngineNDLAttrLength(prAdapter, prNDL, prNDP);

	if (u2AttrLength != 0) {
		nanDataEngineNDLAttrAppendImpl(prAdapter, prMsduInfo, prNDL,
					       NULL, 0, 0);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDL ATTR Implementation
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNDLAttrAppendImpl(struct ADAPTER *prAdapter,
	       struct MSDU_INFO *prMsduInfo,
	       struct _NAN_NDL_INSTANCE_T *prNDL,
	       struct _NAN_ATTR_NDL_T *prPeerAttrNDL,
	       uint8_t ucTypeStatus, uint8_t ucReasonCode)
{
	struct _NAN_ATTR_NDL_T *prAttrNDL = NULL;
	struct _NAN_NDP_INSTANCE_T *prNDP = NULL;
	uint16_t u2AttrLength;
	uint8_t *pucOffset;

	uint8_t *pucScheduleList = NULL;
	uint32_t u4ScheduleListLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "[%s] prMsduInfo error\n", __func__);
		return;
	}

	if (prNDL == NULL && prPeerAttrNDL == NULL) {
		DBGLOG(NAN, ERROR,
		       "%s(): unexpected condition: invalid parameters\n",
		       __func__);
		return;
	}

	prAttrNDL = (struct _NAN_ATTR_NDL_T *)((uint8_t *)prMsduInfo->prPacket +
					       prMsduInfo->u2FrameLength);

	u2AttrLength = nanDataEngineNDLAttrLength(
		prAdapter, prNDL, prNDL == NULL ? NULL : prNDL->prOperatingNDP);

	if (u2AttrLength != 0) {
		prAttrNDL->ucAttrId = NAN_ATTR_ID_NDL;
		prAttrNDL->u2Length =
			u2AttrLength -
			OFFSET_OF(struct _NAN_ATTR_NDL_T, ucDialogToken);

		if (prNDL == NULL) {
			prAttrNDL->ucDialogToken = prPeerAttrNDL->ucDialogToken;
			prAttrNDL->ucTypeStatus = ucTypeStatus;
			prAttrNDL->ucReasonCode = ucReasonCode;
			prAttrNDL->ucNDLControl = 0;
		} else {
			prNDP = prNDL->prOperatingNDP;
			if (prNDP)
				prNDL->ucDialogToken = prNDP->ucDialogToken;
			prAttrNDL->ucDialogToken = prNDL->ucDialogToken;

			if (prNDP == NULL) {
				switch (prNDL->eCurrentNDLMgmtState) {
				case NDL_INITIATOR_TX_SCHEDULE_REQUEST:
					prAttrNDL->ucType =
						NAN_ATTR_NDL_TYPE_REQUEST;
					prAttrNDL->ucStatus =
						NAN_ATTR_NDL_STATUS_CONTINUED;
					break;

				case NDL_RESPONDER_TX_SCHEDULE_RESPONSE:
					/* check & set CONTINUED Per session */
					prNDL->ucNDLSetupCurrentStatus =
						prNDL->fgNeedRespondCounter ?
						NAN_ATTR_NDL_STATUS_CONTINUED :
						prNDL->ucNDLSetupCurrentStatus;

					prNDL->fgNeedRespondCounter = FALSE;

					DBGLOG(NAN, DEBUG,
					       "prNDL %u n=%u, ucNDLSetupCurrentStatus=%u\n",
					       prNDL->ucIndex, prNDL->ucNDPNum,
					       prNDL->ucNDLSetupCurrentStatus);

					prAttrNDL->ucType =
						NAN_ATTR_NDL_TYPE_RESPONSE;
					prAttrNDL->ucStatus =
						prNDL->ucNDLSetupCurrentStatus;
					break;

				case NDL_INITIATOR_TX_SCHEDULE_CONFIRM:
					prAttrNDL->ucType =
						NAN_ATTR_NDL_TYPE_CONFIRM;
					prAttrNDL->ucStatus =
						prNDL->ucNDLSetupCurrentStatus;
					break;

				default:
					DBGLOG(NAN, ERROR,
					       "%s(): unexpected condition: invalid NDL state[%d]\n",
					       __func__,
					       prNDL->eCurrentNDLMgmtState);
					prAttrNDL->ucTypeStatus = ucTypeStatus;
					break;
				}
			} else {
				switch (prNDP->eCurrentNDPProtocolState) {
				case NDP_INITIATOR_TX_DP_REQUEST:
					prAttrNDL->ucType =
						NAN_ATTR_NDL_TYPE_REQUEST;
					prAttrNDL->ucStatus =
						NAN_ATTR_NDL_STATUS_CONTINUED;
					break;

				case NDP_RESPONDER_TX_DP_RESPONSE:
					/* check & set CONTINUED Per session */
					prNDL->ucNDLSetupCurrentStatus =
						prNDL->fgNeedRespondCounter ?
						NAN_ATTR_NDL_STATUS_CONTINUED :
						prNDL->ucNDLSetupCurrentStatus;

					prNDL->fgNeedRespondCounter = FALSE;

					DBGLOG(NAN, DEBUG,
					       "prNDL %u n=%u, ucNDLSetupCurrentStatus=%u\n",
					       prNDL->ucIndex, prNDL->ucNDPNum,
					       prNDL->ucNDLSetupCurrentStatus);

					prAttrNDL->ucType =
						NAN_ATTR_NDL_TYPE_RESPONSE;
					prAttrNDL->ucStatus =
						prNDL->ucNDLSetupCurrentStatus;
					break;

				case NDP_INITIATOR_TX_DP_CONFIRM:
					prAttrNDL->ucType =
						NAN_ATTR_NDL_TYPE_CONFIRM;
					prAttrNDL->ucStatus =
						prNDL->ucNDLSetupCurrentStatus;
					break;

				default:
					DBGLOG(NAN, ERROR,
					       "%s(): unexpected condition: invalid NDP state[%d]\n",
					       __func__,
					       prNDP->eCurrentNDPProtocolState);
					prAttrNDL->ucTypeStatus = ucTypeStatus;
					break;
				}
			}

			prAttrNDL->ucReasonCode = prNDL->ucReasonCode;
			prAttrNDL->ucNDLControl = 0;

			if (prNDL->fgPagingRequired)
				prAttrNDL->b1PeerIdPresent = 1;

			/* Immutable Schedule Entry List */
			if (nanDataEngineNdcAttrLength(prAdapter, prNDL,
						       prNDP) > 0)
				prAttrNDL->b1NdcAttrPresent = 1;

			if (nanDataEngineNdlQosAttrLength(prAdapter, prNDL,
							  prNDP) > 0)
				prAttrNDL->b1NdlQosAttrPresent = 1;

			if (prNDL->u2MaximumIdlePeriod > 0)
				prAttrNDL->b1MaxIdlePeriodPresent = 1;

			if (prNDL->fgCarryImmutableSchedule == TRUE)
				prAttrNDL->b1ImmutableSchedPresent = 1;

			if (prNDP != NULL) {
				prAttrNDL->b2NdlSetupReason =
				      NAN_ATTR_NDL_CTRL_NDL_SETUP_NDP;
			} else {
				prAttrNDL->b2NdlSetupReason =
				      NAN_ATTR_NDL_CTRL_NDL_SETUP_FSD_USING_GAS;
			}

			/* start to fill option field */
			pucOffset = &(prAttrNDL->ucNDLPeerID);

			if (prAttrNDL->b1PeerIdPresent) {
				*pucOffset = prNDL->ucPeerID;
				pucOffset++;
			}

			if (prAttrNDL->b1MaxIdlePeriodPresent) {
				*((uint16_t *)pucOffset) =
					prNDL->u2MaximumIdlePeriod;
				pucOffset += sizeof(uint16_t);
			}

			/* Immutable Schedule Entry List */
			if (prAttrNDL->b1ImmutableSchedPresent) {
				nanSchedNegoGetImmuNdlScheduleList(
					prAdapter, &pucScheduleList,
					&u4ScheduleListLength);
				if (u4ScheduleListLength != 0) {
					kalMemCopy(pucOffset, pucScheduleList,
						   u4ScheduleListLength);
					pucOffset += u4ScheduleListLength;
				}
			}
		}

		/* update payload length */
		prMsduInfo->u2FrameLength += u2AttrLength;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Element Container ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineElemContainerAttrLength(struct ADAPTER *prAdapter,
				     struct _NAN_NDL_INSTANCE_T *prNDL,
				     struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2AttrLength;
#if (CFG_SUPPORT_802_11AX == 1)
	struct BSS_INFO *prBssInfo;
	uint8_t ucHeCapLen, ucHeOpLen;
#if (CFG_SUPPORT_NAN_11BE == 1)
	uint8_t ucEhtCapLen, ucEhtOpLen;
	uint8_t aucElements[100];
#else
	uint8_t aucElements[50];
#endif

	prBssInfo = prAdapter->aprBssInfo[nanGetSpecificBssInfo(
#if (CFG_SUPPORT_NAN_DBDC == 1)
				prAdapter, NAN_BSS_INDEX_BAND1)->ucBssIndex];
#else
				prAdapter, NAN_BSS_INDEX_BAND0)->ucBssIndex];
#endif
#endif

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	u2AttrLength =
		OFFSET_OF(struct _NAN_ATTR_ELEMENT_CONTAINER_T, aucElements);
	u2AttrLength += (ELEM_HDR_LEN + ELEM_MAX_LEN_HT_CAP);
#if CFG_SUPPORT_802_11AC
	u2AttrLength += ELEM_HDR_LEN + ELEM_MAX_LEN_VHT_CAP;
#endif
#if (CFG_SUPPORT_802_11AX == 1)
	DBGLOG(NAN, DEBUG, "Calculate HE IE\n");
	ucHeCapLen = heRlmFillNANHECapIE(
		prAdapter, prBssInfo, aucElements);

	u2AttrLength += ucHeCapLen;
	DBGLOG(NAN, DEBUG, "Len HE Cap IE (%d)\n", ucHeCapLen);

	ucHeOpLen = heRlmFillNANHeOpIE(
		prAdapter, prBssInfo, aucElements);

	u2AttrLength += ucHeOpLen;
	DBGLOG(NAN, DEBUG, "Len HE OP IE (%d)\n", ucHeOpLen);
#endif
#if (CFG_SUPPORT_NAN_11BE == 1)
	if (nanIsEhtEnable(prAdapter)) {
		DBGLOG(NAN, DEBUG, "Calculate EHT IE\n");
		ucEhtCapLen = ehtRlmNANFillCapIE(
			prAdapter, prBssInfo, aucElements);

		u2AttrLength += ucEhtCapLen;
		DBGLOG(NAN, DEBUG, "Len EHT Cap IE (%d)\n", ucEhtCapLen);

		ucEhtOpLen = ehtRlmNANFillOpIE(
			prAdapter, prBssInfo, aucElements);

		u2AttrLength += ucEhtOpLen;
		DBGLOG(NAN, DEBUG, "Len EHT OP IE (%d)\n", ucEhtOpLen);
	}
#endif

	if ((prNDP == NULL) || (prNDL == NULL)) {
		DBGLOG(NAN, DEBUG, "[%s] prNDP/prNDL error\n", __func__);
		return 0;
	}

	switch (prNDP->eCurrentNDPProtocolState) {
	case NDP_INITIATOR_TX_DP_REQUEST:
		return u2AttrLength;

	case NDP_RESPONDER_TX_DP_RESPONSE:
		return u2AttrLength;
#ifdef NAN_UNUSED
		if (prNDL->fgScheduleEstablished == FALSE) {
			if (prNDL->ucNDLSetupCurrentStatus ==
				    NAN_ATTR_NDL_STATUS_ACCEPTED ||
			    prNDL->ucNDLSetupCurrentStatus ==
				    NAN_ATTR_NDL_STATUS_CONTINUED)
				return u2AttrLength;
			else
				return 0;
		} else {
			if (prNDP->ucNDPSetupStatus ==
			    NAN_ATTR_NDP_STATUS_ACCEPTED)
				return u2AttrLength;
			else
				return 0;
		}
#endif
	default:
		return 0;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Element Container ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineElemContainerAttrAppend(struct ADAPTER *prAdapter,
				     struct MSDU_INFO *prMsduInfo,
				     struct _NAN_NDL_INSTANCE_T *prNDL,
				     struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucECAttr = NULL;
	uint16_t u2ECAttrLength = 0;
	struct BSS_INFO *prBssInfo;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (nanDataEngineElemContainerAttrLength(prAdapter, prNDL, prNDP) == 0)
		return;

	prBssInfo = prAdapter->aprBssInfo[nanGetSpecificBssInfo(
#if (CFG_SUPPORT_NAN_DBDC == 1)
			prAdapter, NAN_BSS_INDEX_BAND1)->ucBssIndex];
#else
			prAdapter, NAN_BSS_INDEX_BAND0)->ucBssIndex];
#endif
	if (prNDP->eCurrentNDPProtocolState == NDP_INITIATOR_TX_DP_REQUEST)
		nanDataEngineGetECAttrImpl(prAdapter, &pucECAttr,
					   &u2ECAttrLength, prBssInfo, NULL);
	else
		nanDataEngineGetECAttrImpl(prAdapter, &pucECAttr,
					   &u2ECAttrLength, prBssInfo, prNDL);
	if ((pucECAttr != NULL) && (u2ECAttrLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucECAttr, u2ECAttrLength);
		prMsduInfo->u2FrameLength += u2ECAttrLength;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Device Capability
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineDevCapAttrLength(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucDevCapAttr = NULL;
	uint32_t u4DevCapAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	nanSchedGetDevCapabilityAttr(prAdapter, &pucDevCapAttr,
				     &u4DevCapAttrLength);

	return u4DevCapAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Device Capability
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineDevCapAttrAppend(struct ADAPTER *prAdapter,
			      struct MSDU_INFO *prMsduInfo,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucDevCapAttr = NULL;
	uint32_t u4DevCapAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return;

	nanSchedGetDevCapabilityAttr(prAdapter, &pucDevCapAttr,
				     &u4DevCapAttrLength);

	if ((pucDevCapAttr != NULL) && (u4DevCapAttrLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucDevCapAttr, u4DevCapAttrLength);
		prMsduInfo->u2FrameLength += u4DevCapAttrLength;
	}
}

#if (CFG_SUPPORT_NAN_6G == 1)
/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Device Capability
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineDevCapExtAttrLength(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucDevCapExtAttr = NULL;
	uint32_t u4DevCapExtAttrLength = 0;

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	if (!prAdapter->rWifiVar.ucNanEnable6g)
		return 0;

	nanSchedGetDevCapabilityExtAttr(prAdapter, &pucDevCapExtAttr,
				     &u4DevCapExtAttrLength);

	return u4DevCapExtAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Device Capability
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineDevCapExtAttrAppend(struct ADAPTER *prAdapter,
			      struct MSDU_INFO *prMsduInfo,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucDevCapExtAttr = NULL;
	uint32_t u4DevCapExtAttrLength = 0;

	if ((prNDL == NULL) && (prNDP == NULL))
		return;

	nanSchedGetDevCapabilityExtAttr(prAdapter, &pucDevCapExtAttr,
				     &u4DevCapExtAttrLength);

	if ((pucDevCapExtAttr != NULL) && (u4DevCapExtAttrLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucDevCapExtAttr, u4DevCapExtAttrLength);
		prMsduInfo->u2FrameLength += u4DevCapExtAttrLength;
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NAN Availability
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineNanAvailAttrLength(struct ADAPTER *prAdapter,
				struct _NAN_NDL_INSTANCE_T *prNDL,
				struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucAvailabilityAttr = NULL;
	uint32_t u4AvailabilityAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL == NULL && prNDP == NULL)
		return 0;

	nanSchedGetAvailabilityAttr(prAdapter, prNDL, &pucAvailabilityAttr,
				    &u4AvailabilityAttrLength);

	return u4AvailabilityAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NAN Availability
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNanAvailAttrAppend(struct ADAPTER *prAdapter,
				struct MSDU_INFO *prMsduInfo,
				struct _NAN_NDL_INSTANCE_T *prNDL,
				struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucAvailabilityAttr = NULL;
	uint32_t u4AvailabilityAttrLength = 0;
	struct _NAN_PEER_SCH_DESC_T *prPeerSchDesc;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL == NULL)
		return;

	prPeerSchDesc = nanSchedSearchPeerSchDescByNmi(prAdapter,
						       prNDL->aucPeerMacAddr);
	if (prPeerSchDesc == NULL)
		return;

	DBGLOG(NAN, DEBUG, "Found prPeerSchDesc for " MACSTR "\n",
	       MAC2STR(prNDL->aucPeerMacAddr));

	nanSchedGetAvailabilityAttr(prAdapter, prNDL, &pucAvailabilityAttr,
				    &u4AvailabilityAttrLength);

	/* MERGE_POTENTIAL */
	prPeerSchDesc->u4MergedCommittedChannel = 0;
	kalMemZero(prPeerSchDesc->aucPotMergedBitmap, TYPICAL_BITMAP_LENGTH);

	if ((pucAvailabilityAttr != NULL) && (u4AvailabilityAttrLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucAvailabilityAttr, u4AvailabilityAttrLength);
		prMsduInfo->u2FrameLength += u4AvailabilityAttrLength;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN CHECK NEED TO USE NDPE
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
unsigned char
nanDataEngineNDPECheck(struct ADAPTER *prAdapter,
		unsigned char fgPeerNDPE) {
	if (fgPeerNDPE == TRUE && nanGetFeatureNDPE(prAdapter) == TRUE) {
		DBGLOG(NAN, DEBUG, "Need to support NDPE\n");
		return TRUE;
	}
	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDC
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineNdcAttrLength(struct ADAPTER *prAdapter,
			   struct _NAN_NDL_INSTANCE_T *prNDL,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucNdcAttr = NULL;
	uint32_t u4NdcAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	/* Sigma 5.3.2 must pass with NDC attr
	 * not to carry for NDP Status [ACCEPTED] with Data Path Confirm
	 *  - Table 28
	 * * avoid prNDL == NULL for Coverity Check
	 */
	if (prNDP != NULL &&
	    prNDP->eCurrentNDPProtocolState == NDP_INITIATOR_TX_DP_CONFIRM &&
	    prNDP->fgConfirmRequired == TRUE &&
	    prNDP->ucNDPSetupStatus == NAN_ATTR_NDP_STATUS_ACCEPTED &&
	    prNDL != NULL &&
	    prNDL->fgIsCounter == FALSE) {
		return 0;
	}

	nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucNdcAttr,
						&u4NdcAttrLength);

	return u4NdcAttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDC
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNdcAttrAppend(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo,
			   struct _NAN_NDL_INSTANCE_T *prNDL,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucNdcAttr = NULL;
	uint32_t u4NdcAttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL == NULL && prNDP == NULL)
		return;

	nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucNdcAttr,
						&u4NdcAttrLength);

	if (pucNdcAttr != NULL && u4NdcAttrLength != 0) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucNdcAttr, u4NdcAttrLength);
		prMsduInfo->u2FrameLength += u4NdcAttrLength;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDL QoS
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineNdlQosAttrLength(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucQosAttr = NULL;
	uint32_t u4QosLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	/* not to carry for NDP Status [ACCEPTED] with Data Path Confirm
	 * - Table 28
	 */
	if (prNDP != NULL &&
	    prNDP->eCurrentNDPProtocolState == NDP_INITIATOR_TX_DP_CONFIRM &&
	    prNDP->fgConfirmRequired == TRUE &&
	    prNDP->ucNDPSetupStatus == NAN_ATTR_NDP_STATUS_ACCEPTED) {
		return 0;
	}
	nanSchedNegoGetQosAttr(prAdapter, &pucQosAttr, &u4QosLength);

	return u4QosLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NDL QoS
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNdlQosAttrAppend(struct ADAPTER *prAdapter,
			      struct MSDU_INFO *prMsduInfo,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucQosAttr = NULL;
	uint32_t u4QosLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return;

	nanSchedNegoGetQosAttr(prAdapter, &pucQosAttr, &u4QosLength);

	if ((pucQosAttr != NULL) && (u4QosLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucQosAttr, u4QosLength);
		prMsduInfo->u2FrameLength += u4QosLength;
	}

	return; /* TODO */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Unaligned Schedule
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineUnalignedAttrLength(struct ADAPTER *prAdapter,
				 struct _NAN_NDL_INSTANCE_T *prNDL,
				 struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucUnalignedScheduleAttr = NULL;
	uint32_t u4UnalignedScheduleLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return 0;

	/* avoid prNDL == NULL for Coverity Check */
	if (prNDP != NULL &&
	    prNDP->eCurrentNDPProtocolState == NDP_INITIATOR_TX_DP_CONFIRM &&
	    prNDP->fgConfirmRequired == TRUE &&
	    prNDP->ucNDPSetupStatus == NAN_ATTR_NDP_STATUS_ACCEPTED &&
	    prNDL != NULL &&
	    prNDL->fgIsCounter == FALSE) {
		return 0;
	}

	nanSchedGetUnalignedScheduleAttr(prAdapter, &pucUnalignedScheduleAttr,
					 &u4UnalignedScheduleLength);

	return u4UnalignedScheduleLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Unaligned Schedule
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineUnalignedAttrAppend(struct ADAPTER *prAdapter,
				 struct MSDU_INFO *prMsduInfo,
				 struct _NAN_NDL_INSTANCE_T *prNDL,
				 struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pucUnalignedScheduleAttr = NULL;
	uint32_t u4UnalignedScheduleLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) && (prNDP == NULL))
		return;

	nanSchedGetUnalignedScheduleAttr(prAdapter, &pucUnalignedScheduleAttr,
					 &u4UnalignedScheduleLength);

	if ((pucUnalignedScheduleAttr != NULL) &&
	    (u4UnalignedScheduleLength != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pucUnalignedScheduleAttr, u4UnalignedScheduleLength);
		prMsduInfo->u2FrameLength += u4UnalignedScheduleLength;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - Cipher Suite Information
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineCipherSuiteAttrLength(struct ADAPTER *prAdapter,
				   struct _NAN_NDL_INSTANCE_T *prNDL,
				   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pu1CsidAttrBuf = NULL;
	uint32_t u4CsidAttrLen = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return 0;

	if (prNDP->fgSecurityRequired == FALSE)
		return 0;

	if ((prNDP->eCurrentNDPProtocolState != NDP_INITIATOR_TX_DP_REQUEST) &&
	    (prNDP->eCurrentNDPProtocolState != NDP_RESPONDER_TX_DP_RESPONSE))
		return 0;

	nanSecGetNdpCsidAttr(prNDP, &u4CsidAttrLen, &pu1CsidAttrBuf);

	return u4CsidAttrLen;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - Cipher Suite Information
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineCipherSuiteAttrAppend(struct ADAPTER *prAdapter,
				   struct MSDU_INFO *prMsduInfo,
				   struct _NAN_NDL_INSTANCE_T *prNDL,
				   struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pu1CsidAttrBuf = NULL;
	uint32_t u4CsidAttrLen = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return;

	nanSecGetNdpCsidAttr(prNDP, &u4CsidAttrLen, &pu1CsidAttrBuf);

	if ((pu1CsidAttrBuf != NULL) && (u4CsidAttrLen != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pu1CsidAttrBuf, u4CsidAttrLen);
		prMsduInfo->u2FrameLength += u4CsidAttrLen;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief         NAN Attribute Length Estimation - Security Context Information
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineSecContextAttrLength(struct ADAPTER *prAdapter,
				  struct _NAN_NDL_INSTANCE_T *prNDL,
				  struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pu1ScidAttrBuf = NULL;
	uint32_t u4ScidAttrLen = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return 0;

	if (prNDP->fgSecurityRequired == FALSE)
		return 0;

	if ((prNDP->eCurrentNDPProtocolState != NDP_INITIATOR_TX_DP_REQUEST) &&
	    (prNDP->eCurrentNDPProtocolState != NDP_RESPONDER_TX_DP_RESPONSE))
		return 0;

	nanSecGetNdpScidAttr(prNDP, &u4ScidAttrLen, &pu1ScidAttrBuf);

	return u4ScidAttrLen;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief        NAN Attribute Length Generation - Security Context Information
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineSecContextAttrAppend(struct ADAPTER *prAdapter,
				  struct MSDU_INFO *prMsduInfo,
				  struct _NAN_NDL_INSTANCE_T *prNDL,
				  struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint8_t *pu1ScidAttrBuf = NULL;
	uint32_t u4ScidAttrLen = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return;

	nanSecGetNdpScidAttr(prNDP, &u4ScidAttrLen, &pu1ScidAttrBuf);

	if ((pu1ScidAttrBuf != NULL) && (u4ScidAttrLen != 0)) {
		kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
				   prMsduInfo->u2FrameLength,
			   pu1ScidAttrBuf, u4ScidAttrLen);
		prMsduInfo->u2FrameLength += u4ScidAttrLen;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NAN Shared Key Descriptor
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineSharedKeyAttrLength(struct ADAPTER *prAdapter,
				 struct _NAN_NDL_INSTANCE_T *prNDL,
				 struct _NAN_NDP_INSTANCE_T *prNDP) {
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return 0;

	if (prNDP->fgSecurityRequired == FALSE)
		return 0;

	return nanSecCalKdeAttrLenFunc(prNDP);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Generation - NAN Shared Key Descriptor
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineSharedKeyAttrAppend(struct ADAPTER *prAdapter,
				 struct MSDU_INFO *prMsduInfo,
				 struct _NAN_NDL_INSTANCE_T *prNDL,
				 struct _NAN_NDP_INSTANCE_T *prNDP) {
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return;

	nanSecAppendKdeAttrFunc(prNDP, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP Extension
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t
nanDataEngineNDPESpecAttrLength(struct ADAPTER *prAdapter,
				struct _NAN_NDL_INSTANCE_T *prNDL,
				struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2AttrLength = 0;
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL != NULL && prNDP == NULL)
		return 0;
	if (prNDP->u2AppInfoLen > 0) {
		u2AttrLength = OFFSET_OF(struct _NAN_ATTR_NDPE_GENERAL_TLV_T,
					 aucValue);
		u2AttrLength += prNDP->u2AppInfoLen;
	}

	return u2AttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Generation - NDPE ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNDPESpecAttrAppend(struct ADAPTER *prAdapter,
		uint8_t *pucOffset,
		struct _NAN_NDL_INSTANCE_T *prNDL,
		struct _NAN_NDP_INSTANCE_T *prNDP) {
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDP != NULL) {
		if (prNDP->pucAppInfo != NULL) {
			struct _NAN_ATTR_NDPE_GENERAL_TLV_T *TLV;

			TLV = (struct _NAN_ATTR_NDPE_GENERAL_TLV_T *)pucOffset;
			TLV->ucType = NAN_ATTR_NDPE_SERVINFO_SUB_ATTR_SPECINFO;
			TLV->u2Length = prNDP->u2AppInfoLen;
			kalMemCopy(TLV->aucValue, prNDP->pucAppInfo,
				   prNDP->u2AppInfoLen);
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP Extension
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t nanDataEngineNDPEProtocolAttrLength(struct ADAPTER *prAdapter,
					     struct _NAN_NDL_INSTANCE_T *prNDL,
					     struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint16_t u2AttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL != NULL && prNDP == NULL)
		return 0;
	if (prNDP->ucProtocolType == 0xFF)
		return 0;
	if (prNDP->eCurrentNDPProtocolState == NDP_RESPONDER_TX_DP_RESPONSE) {
		u2AttrLength = OFFSET_OF(struct _NAN_ATTR_NDPE_GENERAL_TLV_T,
					 aucValue);
		u2AttrLength += sizeof(uint8_t);
	}
	return u2AttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Generation - NDPE ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void nanDataEngineNDPEProtocolAttrAppend(struct ADAPTER *prAdapter,
					 uint8_t *pucOffset,
					 struct _NAN_NDL_INSTANCE_T *prNDL,
					 struct _NAN_NDP_INSTANCE_T *prNDP)
{

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDP != NULL &&
	    prNDP->eCurrentNDPProtocolState == NDP_RESPONDER_TX_DP_RESPONSE) {
		struct _NAN_ATTR_NDPE_GENERAL_TLV_T *TLV;

		TLV = (struct _NAN_ATTR_NDPE_GENERAL_TLV_T *)pucOffset;
		TLV->ucType = NAN_ATTR_NDPE_SERVINFO_SUB_ATTR_PROTOCOL;
		TLV->u2Length = sizeof(prNDP->ucProtocolType);
		kalMemCopy(TLV->aucValue, &prNDP->ucProtocolType,
			   sizeof(prNDP->ucProtocolType));
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP Extension
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t nanDataEngineNDPEPORTAttrLength(struct ADAPTER *prAdapter,
					 struct _NAN_NDL_INSTANCE_T *prNDL,
					 struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint16_t u2AttrLength = 0;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prNDL != NULL && prNDP == NULL)
		return 0;
	if (prNDP->u2PortNum == 0)
		return 0;
	if (prNDP->eCurrentNDPProtocolState == NDP_RESPONDER_TX_DP_RESPONSE) {
		u2AttrLength = OFFSET_OF(struct _NAN_ATTR_NDPE_GENERAL_TLV_T,
					 aucValue);
		u2AttrLength += sizeof(uint16_t);
	}
	return u2AttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Generation - NDPE ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void nanDataEngineNDPEPORTAttrAppend(struct ADAPTER *prAdapter,
				     uint8_t *pucOffset,
				     struct _NAN_NDL_INSTANCE_T *prNDL,
				     struct _NAN_NDP_INSTANCE_T *prNDP)
{
	struct _NAN_ATTR_NDPE_GENERAL_TLV_T *TLV;
#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	TLV = (struct _NAN_ATTR_NDPE_GENERAL_TLV_T *)pucOffset;
	if (prNDP != NULL &&
	    prNDP->eCurrentNDPProtocolState == NDP_RESPONDER_TX_DP_RESPONSE) {
		TLV->ucType = NAN_ATTR_NDPE_SERVINFO_SUB_ATTR_TRANSPORT_PORT;
		TLV->u2Length = sizeof(prNDP->u2PortNum);
		kalMemCopy(TLV->aucValue, &prNDP->u2PortNum,
			   sizeof(prNDP->u2PortNum));
	}
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute : Service Info check
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
unsigned char
nanDataEngineServiceInfoCheck(struct ADAPTER *prAdapter,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	if (prNDP->u2PortNum == 0 && prNDP->ucProtocolType == 0xFF &&
	    prNDP->u2AppInfoLen == 0)
		return FALSE;
	return TRUE;
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Length Estimation - NDP Extension
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint16_t nanDataEngineNDPEAttrLength(struct ADAPTER *prAdapter,
				     struct _NAN_NDL_INSTANCE_T *prNDL,
				     struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint16_t u2AttrLength;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return 0;
	}

	/* Schedule Req/Resp/Confirm/UpdateNotification condition */
	if (prNDL != NULL && prNDP == NULL)
		return 0;

	u2AttrLength = OFFSET_OF(struct _NAN_ATTR_NDPE_T, ucPublishID);

	if (prNDP != NULL) {
		if (nanDataEngineNDPECheck(prAdapter, prNDP->fgSupportNDPE) ==
		    FALSE) {
			DBGLOG(NAN, DEBUG,
				"[%s] Do not carry NDPE Attr. fgSupportNDPE %d nanGetFeatureNDPE %d\n",
				 __func__,
				prNDP->fgSupportNDPE,
				nanGetFeatureNDPE(prAdapter));
			return 0;
		}

		DBGLOG(NAN, DEBUG,
		   "[%s] Append AppInfoLen = %d, fgIpv6 = %d\n",
		   __func__,
		   prNDP->u2AppInfoLen,
		   prNDP->fgCarryIPV6);

		if (prNDP->fgCarryIPV6 == TRUE)
			u2AttrLength += sizeof(
			struct _NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T);

		if (!nanGetFeatureIsSigma(prAdapter)) {
			if (prNDP->u2AppInfoLen > 0)
				u2AttrLength += prNDP->u2AppInfoLen;
		} else {
#ifdef NAN_UNUSED
			if (prNDP->u2AppInfoLen > 0)
				u2AttrLength +=
					OFFSET_OF(
					struct
					_NAN_ATTR_NDPE_WFA_SERVICE_INFO_TLV_T,
					aucBody)
					+ prNDP->u2AppInfoLen;

			if (prNDP->u2OtherAppInfoLen > 0)
				u2AttrLength +=
					OFFSET_OF(
					struct
					_NAN_ATTR_NDPE_SERVICE_INFO_TLV_T,
					aucBody)
					+ prNDP->u2OtherAppInfoLen;
#else
		if ((prNDP->eCurrentNDPProtocolState ==
			NDP_INITIATOR_TX_DP_REQUEST ||
			prNDP->eCurrentNDPProtocolState ==
			NDP_RESPONDER_TX_DP_RESPONSE) &&
			nanDataEngineServiceInfoCheck(prAdapter,
			prNDP) == TRUE) {
			uint8_t i = 0;

			u2AttrLength += OFFSET_OF(
			struct
			_NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T,
			aucBody);
			for (i = 0;
				i <
				sizeof(txServInfoSSITable) /
				sizeof(struct
				_APPEND_ATTR_ENTRY_T);
				 i++) {
				if (txServInfoSSITable[i]
					.pfnCalculateVariableAttrLen) {
					u2AttrLength +=
						txServInfoSSITable[i]
						.pfnCalculateVariableAttrLen(
						prAdapter,
						prNDL,
						prNDP);
				}
			}
		}
#endif
		}
		switch (prNDP->eCurrentNDPProtocolState) {
		case NDP_INITIATOR_TX_DP_REQUEST:
			u2AttrLength++; /* for Publish ID */
			return u2AttrLength;

		case NDP_INITIATOR_TX_DP_CONFIRM:
			/* refer to Table.28 - don't carry NDP for NDL
			 * Counter Reject
			 */
			if (prNDL->ucNDLSetupCurrentStatus ==
			    NAN_ATTR_NDL_STATUS_REJECTED)
				return 0;
			return u2AttrLength;

		case NDP_RESPONDER_TX_DP_RESPONSE:
			if (prNDP->ucNDPSetupStatus ==
				    NAN_ATTR_NDP_STATUS_CONTINUED ||
			    prNDP->ucNDPSetupStatus ==
				    NAN_ATTR_NDP_STATUS_ACCEPTED) {
				u2AttrLength +=
					MAC_ADDR_LEN; /* for Responder NDI */
			}
			return u2AttrLength;

		case NDP_RESPONDER_TX_DP_SECURITY_INSTALL:
			return u2AttrLength;

		case NDP_TX_DP_TERMINATION:
			return u2AttrLength;

		default:
			return 0;
		}
	} else
		return u2AttrLength;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Generation - NDPE ATTR
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void
nanDataEngineNDPEAttrAppend(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo,
			    struct _NAN_NDL_INSTANCE_T *prNDL,
			    struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint16_t u2AttrLength;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "[%s] prMsduInfo error\n", __func__);
		return;
	}

	u2AttrLength = nanDataEngineNDPEAttrLength(prAdapter, prNDL, prNDP);

	if (u2AttrLength != 0) {
		nanDataEngineNDPEAttrAppendImpl(prAdapter, prMsduInfo, prNDL,
						prNDP, NULL, 0, 0);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief            NAN Attribute Generation - NDPE ATTR Implementation
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
void nanDataEngineNDPEAttrAppendImpl(struct ADAPTER *prAdapter,
				struct MSDU_INFO *prMsduInfo,
				struct _NAN_NDL_INSTANCE_T *prNDL,
				struct _NAN_NDP_INSTANCE_T *prNDP,
				struct _NAN_ATTR_NDPE_T *prPeerAttrNDPE,
				uint8_t ucTypeStatus,
				uint8_t ucReasonCode)
{
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;
	struct _NAN_ATTR_NDPE_T *prAttrNDPE = NULL;
	struct _NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T *prIPV6TLV = NULL;
	struct _NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T *prWFAAppInfoTLV = NULL;

	uint16_t u2AttrLength;
	uint8_t *pucOffset;

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if (prMsduInfo == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prMsduInfo error\n", __func__);
		return;
	}

	if (prNDP == NULL && prPeerAttrNDPE == NULL) {
		DBGLOG(NAN, ERROR,
		       "%s(): unexpected condition: invalid parameters\n",
		       __func__);
		return;
	}

	prAttrNDPE =
		(struct _NAN_ATTR_NDPE_T *)((uint8_t *)prMsduInfo->prPacket +
					    prMsduInfo->u2FrameLength);

	u2AttrLength = nanDataEngineNDPEAttrLength(prAdapter, prNDL, prNDP);

	if (u2AttrLength == 0)
		return;

	prAttrNDPE->ucAttrId = NAN_ATTR_ID_NDP_EXTENSION;
	prAttrNDPE->u2Length =
		u2AttrLength -
		OFFSET_OF(struct _NAN_ATTR_NDPE_T, ucDialogToken);

	if (prNDP == NULL) {
		prAttrNDPE->ucDialogToken =
			prPeerAttrNDPE->ucDialogToken;
		prAttrNDPE->ucTypeStatus = ucTypeStatus;
		prAttrNDPE->ucReasonCode = ucReasonCode;
		COPY_MAC_ADDR(prAttrNDPE->aucInitiatorNDI,
			      prPeerAttrNDPE->aucInitiatorNDI);
		prAttrNDPE->ucNDPID = prPeerAttrNDPE->ucNDPID;
		prAttrNDPE->ucNDPEControl = 0;
	} else {
		prAttrNDPE->ucDialogToken = prNDP->ucDialogToken;
		prAttrNDPE->ucTypeStatus = prNDP->ucTxNextTypeStatus;
		prAttrNDPE->ucReasonCode = prNDP->ucReasonCode;

		if (prNDP->eNDPRole == NAN_PROTOCOL_INITIATOR)
			COPY_MAC_ADDR(prAttrNDPE->aucInitiatorNDI,
				      prNDP->aucLocalNDIAddr);

		else
			COPY_MAC_ADDR(prAttrNDPE->aucInitiatorNDI,
				      prNDP->aucPeerNDIAddr);

		prAttrNDPE->ucNDPID = prNDP->ucNDPID;

		prAttrNDPE->ucNDPEControl = 0;
		if (prNDP->fgConfirmRequired)
			prAttrNDPE->ucNDPEControl |=
				NAN_ATTR_NDPE_CTRL_CONFIRM_REQUIRED;
		if (prNDP->fgSecurityRequired)
			prAttrNDPE->ucNDPEControl |=
				NAN_ATTR_NDPE_CTRL_SECURITY_PRESENT;
		if (prNDP->eCurrentNDPProtocolState ==
		    NDP_INITIATOR_TX_DP_REQUEST)
			prAttrNDPE->ucNDPEControl |=
				NAN_ATTR_NDPE_CTRL_PUBLISHID_PRESENT;
		if (prNDP->eCurrentNDPProtocolState ==
		    NDP_RESPONDER_TX_DP_RESPONSE) {
			if (prNDP->ucNDPSetupStatus ==
				    NAN_ATTR_NDP_STATUS_CONTINUED ||
			    prNDP->ucNDPSetupStatus ==
				NAN_ATTR_NDP_STATUS_ACCEPTED) {
				prAttrNDPE->ucNDPEControl |=
				    NAN_ATTR_NDPE_CTRL_RESP_NDI_PRESENT;
			}
		}

		/* start to fill option field */
		pucOffset = &(prAttrNDPE->ucPublishID);
		if (prAttrNDPE->ucNDPEControl &
		    NAN_ATTR_NDPE_CTRL_PUBLISHID_PRESENT) {
			*pucOffset = prNDP->ucPublishId;
			pucOffset++;
		}

		if (prAttrNDPE->ucNDPEControl &
		    NAN_ATTR_NDPE_CTRL_RESP_NDI_PRESENT) {
			if (prNDP->eNDPRole == NAN_PROTOCOL_INITIATOR)
				COPY_MAC_ADDR(pucOffset,
					      prNDP->aucPeerNDIAddr);

			else
				COPY_MAC_ADDR(pucOffset,
					      prNDP->aucLocalNDIAddr);
			pucOffset += MAC_ADDR_LEN;
		}

		if (prNDP->fgCarryIPV6 == TRUE) {
			prIPV6TLV =
				(struct
				 _NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T *)
					pucOffset;

			/* filling */
			prIPV6TLV->ucType =
				NAN_ATTR_NDPE_TLV_TYPE_IPV6_LINK_LOCAL;
			prIPV6TLV->u2Length =
			    sizeof(struct
				_NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T) -
				OFFSET_OF(struct
				_NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T,
				aucInterfaceId);
			if (prNDP->fgIsInitiator == TRUE)
				kalMemCopy(prIPV6TLV->aucInterfaceId,
					   prNDP->aucInterfaceId, 8);
			else
				kalMemCopy(prIPV6TLV->aucInterfaceId,
					   prNDP->aucRspInterfaceId, 8);
			/* move to next */
			pucOffset += sizeof(
				struct
				_NAN_ATTR_NDPE_IPV6_LINK_LOCAL_TLV_T);
		}

		if ((prNDP->eCurrentNDPProtocolState ==
			     NDP_INITIATOR_TX_DP_REQUEST ||
		     prNDP->eCurrentNDPProtocolState ==
			     NDP_RESPONDER_TX_DP_RESPONSE ||
			     prNDP->eCurrentNDPProtocolState ==
			     NDP_RESPONDER_TX_DP_SECURITY_INSTALL) &&
		    nanDataEngineServiceInfoCheck(prAdapter, prNDP) ==
			    TRUE) {

			if (nanGetFeatureIsSigma(prAdapter)) {
				uint8_t i = 0;
				uint16_t u2OffSet = 0, u2SubAttrLen = 0,
					u2TotalSubAttrLen = 0;

				prWFAAppInfoTLV =
				(struct
				_NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T
				*)pucOffset;

				prWFAAppInfoTLV->ucType =
				NAN_ATTR_NDPE_TLV_TYPE_SERVICE_INFO;

				kalMemCopy(prWFAAppInfoTLV->aucOui,
					aucOui,
					VENDOR_OUI_LEN);
				prWFAAppInfoTLV->ucServiceProtocolType =
				NAN_SERVICE_PROTOCOL_TYPE_GENERIC;

				/* move to next */
				pucOffset += OFFSET_OF(
				struct
				_NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T,
				aucBody);
				/* fill NAN attributes */
				for (i = 0;
				     i < sizeof(txServInfoSSITable) /
						sizeof(struct
						_APPEND_ATTR_ENTRY_T);
					i++) {
					u2SubAttrLen =
					txServInfoSSITable[i]
					.pfnCalculateVariableAttrLen(
						prAdapter,
						prNDL, prNDP);
					if (u2SubAttrLen != 0 &&
						txServInfoSSITable[i]
						.pfnAppendAttr) {
						txServInfoSSITable[i]
						.pfnAppendAttr(
							prAdapter,
							pucOffset +
							u2OffSet,
							prNDL, prNDP);
						u2OffSet +=
							u2SubAttrLen;
						u2TotalSubAttrLen +=
							u2SubAttrLen;
					}
				}

				prWFAAppInfoTLV->u2Length =
				OFFSET_OF(struct
					_NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T,
					aucBody) +
					u2TotalSubAttrLen -
					OFFSET_OF(struct
					_NAN_ATTR_NDPE_WFA_SVC_INFO_TLV_T,
					aucOui);
			} else {
				if (prNDP->u2AppInfoLen > 0)
					/* AppInfo: portNum,
					 * protocolType
					 */
					kalMemCopy(pucOffset,
						prNDP->pucAppInfo,
						prNDP->u2AppInfoLen);
			}
#ifdef NAN_UNUSED /* OtherAppinfo */
			if (prNDP->u2OtherAppInfoLen > 0) {
				prOtherAppInfoTLV =
				(struct
				_NAN_ATTR_NDPE_SERVICE_INFO_TLV_T *)
				pucOffset;

				prOtherAppInfoTLV->ucType =
					NAN_ATTR_NDPE_TLV_TYPE_SERVICE_INFO;
					prOtherAppInfoTLV->u2Length =
					OFFSET_OF(
					struct
					_NAN_ATTR_NDPE_SERVICE_INFO_TLV_T,
					aucBody)
					+ prNDP->u2OtherAppInfoLen
					- OFFSET_OF(struct
					_NAN_ATTR_NDPE_SERVICE_INFO_TLV_T,
					aucOui);
				kalMemCopy(prOtherAppInfoTLV->aucOui,
					prNDP->aucOtherAppInfoOui,
					VENDOR_OUI_LEN);
				kalMemCopy(prOtherAppInfoTLV->aucBody,
					prNDP->pucOtherAppInfo,
					prNDP->u2OtherAppInfoLen);

				/* move to next */
				pucOffset +=
					OFFSET_OF(
					struct
					_NAN_ATTR_NDPE_SERVICE_INFO_TLV_T,
					aucBody) +
					prNDP->u2OtherAppInfoLen;
			}
#endif
			}
		}
#if (ENABLE_NDP_UT_LOG == 1)
	DBGLOG(NAN, DEBUG, "NAN NDPE ROW DATA\n");
	dumpMemory8((uint8_t *)prMsduInfo->prPacket +
			    prMsduInfo->u2FrameLength,
		    u2AttrLength);
	DBGLOG(NAN, DEBUG, "NAN NDPE ROW DATA END\n");
#endif
	/* update payload length */
	prMsduInfo->u2FrameLength += u2AttrLength;
}

uint32_t
nanDataEngineSetupStaRec(struct ADAPTER *prAdapter,
			 struct _NAN_NDL_INSTANCE_T *prNDL,
			 struct STA_RECORD *prStaRec) {
	uint8_t ucPeerBW;
	uint32_t u4PeerNSS;
	struct IE_HT_CAP *prHtCap;
#if CFG_SUPPORT_802_11AC
	struct IE_VHT_CAP *prVhtCap;
	uint8_t ucVhtCapMcsOwnNotSupportOffset;
#endif
#if (CFG_SUPPORT_802_11AX == 1)
	uint8_t *prHeCap;
#endif
#if (CFG_SUPPORT_NAN_11BE == 1)
	uint8_t *prEhtCap;
#endif

	struct WIFI_VAR *prWifiVar;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *)NULL;

	if ((prStaRec == NULL) || (prNDL == NULL))
		return WLAN_STATUS_FAILURE;

	prWifiVar = &prAdapter->rWifiVar;
	prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	if (nanGetPeerDevCapability(prAdapter,
		ENUM_NAN_DEVCAP_RX_ANT_NUM,
		prNDL->aucPeerMacAddr, NAN_INVALID_MAP_ID,
		&u4PeerNSS) == WLAN_STATUS_SUCCESS) {
		ucPeerBW = nanGetPeerMinBw(prAdapter,
			prNDL->aucPeerMacAddr, prBssInfo->eBand);
		DBGLOG(NAN, DEBUG,
			"[%s] PeerBW %d , PeerNSS %d\n", __func__,
			ucPeerBW, u4PeerNSS);
	} else {
		ucPeerBW = 20;
		/* whsu */
		u4PeerNSS = prBssInfo->ucOpRxNss;

		DBGLOG(NAN, DEBUG, "[%s] Use Default PeerBW %d , PeerNSS %d\n",
		       __func__, ucPeerBW, u4PeerNSS);
	}

	prStaRec->fgHasBasicPhyType = TRUE;
	prStaRec->ucPhyTypeSet = prNDL->ucPhyTypeSet | PHY_TYPE_SET_802_11A |
				 PHY_TYPE_SET_802_11G;
	prStaRec->ucDesiredPhyTypeSet =
		prStaRec->ucPhyTypeSet &
		prAdapter->rWifiVar.ucAvailablePhyTypeSet;

	prStaRec->u2BSSBasicRateSet = prBssInfo->u2BSSBasicRateSet;
	prStaRec->ucNonHTBasicPhyType = PHY_TYPE_ERP_INDEX;
	prStaRec->u2DesiredNonHTRateSet = prBssInfo->u2OperationalRateSet;

	nicTxUpdateStaRecDefaultRate(prAdapter, prStaRec);

	/* fill HT Capabilities */
	prHtCap = &(prNDL->rIeHtCap);

	prStaRec->ucMcsSet = prHtCap->rSupMcsSet.aucRxMcsBitmask[0];
	prStaRec->fgSupMcs32 =
		(prHtCap->rSupMcsSet.aucRxMcsBitmask[32 / 8] & BIT(0)) ? TRUE
								       : FALSE;
	kalMemCopy(
		prStaRec->aucRxMcsBitmask, prHtCap->rSupMcsSet.aucRxMcsBitmask,
		sizeof(prStaRec->aucRxMcsBitmask));
		/* SUP_MCS_RX_BITMASK_OCTET_NUM */
	prStaRec->u2RxHighestSupportedRate =
		prHtCap->rSupMcsSet.u2RxHighestSupportedRate;
	prStaRec->u4TxRateInfo = prHtCap->rSupMcsSet.u4TxRateInfo;
	prStaRec->u2HtCapInfo = prHtCap->u2HtCapInfo;

	/* Set LDPC Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;
	else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_LDPC_CAP;

	/* Set STBC Tx capability */
	if (rlmCheckTxStbc(prAdapter, prStaRec->ucBssIndex,
		FEATURE_FORCE_ENABLED))
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_RX_STBC;
	else if (rlmCheckTxStbc(prAdapter, prStaRec->ucBssIndex,
		FEATURE_DISABLED))
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_RX_STBC;

	/* Set Short GI Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_20M;
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_40M;
	} else if (IS_FEATURE_DISABLED(prWifiVar->ucTxShortGI)) {
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_20M;
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_40M;
	}

	/* Set HT Greenfield Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxGf))
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
	else if (IS_FEATURE_DISABLED(prWifiVar->ucTxGf))
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;

	prStaRec->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL;
	prStaRec->u2HtExtendedCap =
		(HT_EXT_CAP_DEFAULT_VAL &
		 (~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE)));
	prStaRec->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL;
	prStaRec->ucAselCap = ASEL_CAP_DEFAULT_VAL;

#if CFG_SUPPORT_802_11AC
	/* fill VHT Capabilities */
	if ((prNDL->ucPhyTypeSet & PHY_TYPE_BIT_VHT)) {
		DBGLOG(NAN, DEBUG, "NAN peer supports VHT\n");
		prVhtCap = &(prNDL->rIeVhtCap);

		prStaRec->u4VhtCapInfo = prVhtCap->u4VhtCapInfo;
		if (prAdapter->rWifiVar.uc5GBandwidthMode == NAN_CHNL_BW_160) {
			ucPeerBW = 160;
			prStaRec->u4VhtCapInfo |=
				VHT_CAP_INFO_MAX_SUP_CHANNEL_WIDTH_SET_160;
			if (IS_FEATURE_ENABLED
				(prAdapter->rWifiVar.ucRxShortGI)) {
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_SHORT_GI_80;
				prStaRec->u4VhtCapInfo |=
					VHT_CAP_INFO_SHORT_GI_160_80P80;
			} else {
				prStaRec->u4VhtCapInfo &=
					(~VHT_CAP_INFO_SHORT_GI_80);
				prStaRec->u4VhtCapInfo &=
					(~VHT_CAP_INFO_SHORT_GI_160_80P80);
			}
		}
		/* Set Tx LDPC capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;
		else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_LDPC;

		/* Set Tx STBC capability */
		if (rlmCheckTxStbc(prAdapter, prStaRec->ucBssIndex,
			FEATURE_FORCE_ENABLED))
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_STBC_MASK;
		else if (rlmCheckTxStbc(prAdapter, prStaRec->ucBssIndex,
			FEATURE_DISABLED))
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_STBC_MASK;

		/* Set Tx TXOP PS capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxopPsTx))
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_VHT_TXOP_PS;
		else if (IS_FEATURE_DISABLED(prWifiVar->ucTxopPsTx))
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_VHT_TXOP_PS;

		/* Set Tx Short GI capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;
			prStaRec->u4VhtCapInfo |=
				VHT_CAP_INFO_SHORT_GI_160_80P80;
		} else if (IS_FEATURE_DISABLED(prWifiVar->ucTxShortGI)) {
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_SHORT_GI_80;
			prStaRec->u4VhtCapInfo &=
				~VHT_CAP_INFO_SHORT_GI_160_80P80;
		}

		/* Set Vht Rx Mcs Map upon peer's capability
		 * and our capability
		 */
		prStaRec->u2VhtRxMcsMap =
			prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap;
		if (wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) < 8) {
			ucVhtCapMcsOwnNotSupportOffset =
				wlanGetSupportNss(prAdapter,
						  prStaRec->ucBssIndex) *
				2;
			/* Mark Rx Mcs Map which we don't support */
			prStaRec->u2VhtRxMcsMap |=
				BITS(ucVhtCapMcsOwnNotSupportOffset, 15);
		}
		if (prStaRec->u2VhtRxMcsMap !=
		    prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap)
			DBGLOG(NAN, DEBUG,
			       "Change VhtRxMcsMap from 0x%x to 0x%x due to our Nss setting\n",
			       prVhtCap->rVhtSupportedMcsSet.u2RxMcsMap,
			       prStaRec->u2VhtRxMcsMap);
		prStaRec->u2VhtRxHighestSupportedDataRate =
			prVhtCap->rVhtSupportedMcsSet
				.u2RxHighestSupportedDataRate;

		prStaRec->u2VhtTxMcsMap =
			prVhtCap->rVhtSupportedMcsSet.u2TxMcsMap;
		prStaRec->u2VhtTxHighestSupportedDataRate =
			prVhtCap->rVhtSupportedMcsSet
				.u2TxHighestSupportedDataRate;

		prStaRec->ucVhtOpMode = 0;
		switch (ucPeerBW) {
		case 20:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_20;
			break;
		case 40:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_40;
			break;
		case 80:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
			break;
		case 160:
			prStaRec->ucVhtOpMode |=
				VHT_OP_MODE_CHANNEL_WIDTH_160_80P80;
			break;
		default:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
			break;
		}
		prStaRec->ucVhtOpMode |=
			((u4PeerNSS - 1) << VHT_OP_MODE_RX_NSS_OFFSET) &
			VHT_OP_MODE_RX_NSS;
	}
#endif

#if (CFG_SUPPORT_802_11AX == 1)
		/* fill HE Capabilities */
		if ((prNDL->ucPhyTypeSet & PHY_TYPE_BIT_HE)) {
			DBGLOG(NAN, DEBUG, "NAN peer supports HE\n");
			prHeCap = prNDL->aucIeHeCap;
			heRlmRecHeCapInfo(prAdapter, prStaRec, prHeCap);
		}
#endif
#if (CFG_SUPPORT_NAN_11BE == 1)
		/* fill EHT Capabilities */
		if ((prNDL->ucPhyTypeSet & PHY_TYPE_BIT_EHT)) {
			DBGLOG(NAN, DEBUG, "NAN peer supports EHT\n");
			prEhtCap = prNDL->aucIeEhtCap;
			ehtRlmRecCapInfo(prAdapter, prStaRec,
				prEhtCap);
		}
#endif

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief         NAN STA-REC Allocate & Activate
 *
 * \param[in]
 *
 * \return Status WLAN_STATUS_SUCCESS
 *                WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanDataEngineAllocStaRec(struct ADAPTER *prAdapter,
			 struct _NAN_NDL_INSTANCE_T *prNDL,
			 uint8_t ucBssIndex, uint8_t *pucPeerAddr,
			 uint8_t ucRcpi, struct STA_RECORD **pprStaRec) {
	struct BSS_INFO *prBssInfo;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (!prNDL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (pprStaRec == NULL))
		return WLAN_STATUS_FAILURE;

	if (*pprStaRec == NULL) {
		*pprStaRec = cnmStaRecAlloc(prAdapter, STA_TYPE_NAN, ucBssIndex,
					    pucPeerAddr);
		if (*pprStaRec == NULL) {
			DBGLOG(NAN, ERROR, "%s(): STA-REC allocation failure\n",
			       __func__);
			return WLAN_STATUS_FAILURE;
		}
		/* Fixme: support QoS data
		 * (*pprStaRec)->fgIsQoS = TRUE
		 */

		cnmStaRecChangeState(prAdapter, *pprStaRec, STA_STATE_1);
		prBssInfo = prAdapter->aprBssInfo[(*pprStaRec)->ucBssIndex];
		DBGLOG(NAN, DEBUG,
		       "[%s] Update STA_REC: ADDR=%02x:%02x:%02x:%02x:%02x:%02x\n",
		       __func__, (*pprStaRec)->aucMacAddr[0],
		       (*pprStaRec)->aucMacAddr[1], (*pprStaRec)->aucMacAddr[2],
		       (*pprStaRec)->aucMacAddr[3], (*pprStaRec)->aucMacAddr[4],
		       (*pprStaRec)->aucMacAddr[5]);
		DBGLOG(NAN, DEBUG,
		       "[%s] Update STA_REC: Idx:%d, WtblIdx:%d, BssIdx:%d, staType:%d\n",
		       __func__, (*pprStaRec)->ucIndex,
		       (*pprStaRec)->ucWlanIndex,
		       ucBssIndex,
		       (*pprStaRec)->eStaType);
		DBGLOG(NAN, DEBUG,
		       "[%s] BSS OwnMac=%02x:%02x:%02x:%02x:%02x:%02x\n",
		       __func__, prBssInfo->aucOwnMacAddr[0],
		       prBssInfo->aucOwnMacAddr[1], prBssInfo->aucOwnMacAddr[2],
		       prBssInfo->aucOwnMacAddr[3], prBssInfo->aucOwnMacAddr[4],
		       prBssInfo->aucOwnMacAddr[5]);

		nanDataEngineSetupStaRec(prAdapter, prNDL, *pprStaRec);

		(*pprStaRec)->ucRCPI = ucRcpi;

#if (CFG_SUPPORT_NAN_11BE_MLO == 1)
		nanMldStaRecRegister(prAdapter,
			*pprStaRec,
			nanGetLinkIndexbyBand(prAdapter, prBssInfo->eBand));
#endif

		atomic_set(&((*pprStaRec)->NanRefCount), 1);
	} else {
		atomic_inc(&((*pprStaRec)->NanRefCount));
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Get STA_RECORD_T by Peer MAC Address(Usually TA)
 *        & check the Network Type.
 *
 * @param[in] pucPeerMacAddr      Given Peer MAC Address.
 *
 * @retval   Pointer to STA_RECORD_T, if found. NULL, if not found
 */
/*----------------------------------------------------------------------------*/
struct STA_RECORD *
nanGetStaRecByNDI(struct ADAPTER *prAdapter, uint8_t *pucPeerMacAddr) {
	struct STA_RECORD *prStaRec;
	uint16_t i;
	struct BSS_INFO *prBssInfo;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return NULL;
	}

	if (!pucPeerMacAddr)
		return NULL;
	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];
		if (prStaRec->fgIsInUse &&
		    prStaRec->ucBssIndex < MAX_BSSID_NUM) {
			prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
			if (prBssInfo->eNetworkType == NETWORK_TYPE_NAN &&
			    EQUAL_MAC_ADDR(prStaRec->aucMacAddr,
					   pucPeerMacAddr)) {
				break;
			}
		}
	}

	return (i < CFG_STA_REC_NUM) ? prStaRec : NULL;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief         NAN STA-REC Deactivate & Free
 *
 * \param[in]
 *
 * \return Status WLAN_STATUS_SUCCESS
 *                WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanDataEngineFreeStaRec(struct ADAPTER *prAdapter,
			struct _NAN_NDL_INSTANCE_T *prNDL,
			struct STA_RECORD **pprStaRec) {

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter error\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

#if (ENABLE_NDP_UT_LOG == 1)
	TRACE_FUNC(NAN, DEBUG, "[%s] Enter\n");
#endif

	if ((prNDL == NULL) || (pprStaRec == NULL))
		return WLAN_STATUS_FAILURE;

	if (atomic_dec_return(&((*pprStaRec)->NanRefCount)) == 0) {
		nanSecResetTk(*pprStaRec);
		cnmStaRecChangeState(prAdapter, *pprStaRec, STA_STATE_1);
		cnmStaRecFree(prAdapter, *pprStaRec);
		*pprStaRec = NULL;
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDataEngineEnrollNMIContext(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint32_t u4Idx;
	uint32_t u4NdpCxtIdx = NAN_MAX_SUPPORT_NDP_CXT_NUM;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;
	struct _NAN_NDP_INSTANCE_T *prTargetNdpSA;
	struct _NAN_NDP_INSTANCE_T *prNdpTmp;
	unsigned char fgSecurityRequired;
	uint8_t ucBssIndex;
	uint8_t *pucLocalNMI;
	uint8_t *pucPeerNMI;
	enum ENUM_BAND eBand;
	struct STA_RECORD *prNanStaRec = NULL;

#if !CFG_NAN_PMF_PATCH
	return WLAN_STATUS_SUCCESS;
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return WLAN_STATUS_FAILURE;

	pucLocalNMI = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	pucPeerNMI = prNDL->aucPeerMacAddr;

	if ((kalMemCmp(pucLocalNMI, prNDP->aucLocalNDIAddr, MAC_ADDR_LEN) ==
	     0) &&
	    (kalMemCmp(pucPeerNMI, prNDP->aucPeerNDIAddr, MAC_ADDR_LEN) == 0))
		return WLAN_STATUS_SUCCESS;

	u4NdpCxtIdx = NAN_MAX_SUPPORT_NDP_CXT_NUM;
	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4Idx];
		if (prNdpCxt->fgValid == FALSE) {
			if (u4NdpCxtIdx == NAN_MAX_SUPPORT_NDP_CXT_NUM)
				u4NdpCxtIdx = u4Idx;
			continue;
		}

		if ((kalMemCmp(prNdpCxt->aucLocalNDIAddr, pucLocalNMI,
			       MAC_ADDR_LEN) == 0) &&
		    (kalMemCmp(prNdpCxt->aucPeerNDIAddr, pucPeerNMI,
			       MAC_ADDR_LEN) == 0)) {

			u4NdpCxtIdx = u4Idx;
			break;
		}
	}
	if (u4NdpCxtIdx == NAN_MAX_SUPPORT_NDP_CXT_NUM)
		return WLAN_STATUS_FAILURE;

	prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
	if (prNdpCxt->fgValid == FALSE) {
		kalMemZero((uint8_t *)prNdpCxt,
			   sizeof(struct _NAN_NDP_CONTEXT_T));

		kalMemCopy(prNdpCxt->aucLocalNDIAddr, pucLocalNMI,
			   MAC_ADDR_LEN);
		kalMemCopy(prNdpCxt->aucPeerNDIAddr, pucPeerNMI, MAC_ADDR_LEN);

		nanResetStaRec(prNdpCxt);

		DBGLOG(NAN, DEBUG, "Allocate NDP Cxt %d\n", u4NdpCxtIdx);
		DBGLOG(NAN, DEBUG, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       pucLocalNMI[0], pucLocalNMI[1], pucLocalNMI[2],
		       pucLocalNMI[3], pucLocalNMI[4], pucLocalNMI[5]);
		DBGLOG(NAN, DEBUG, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
		       pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);

		prNdpCxt->ucId = (uint8_t)u4NdpCxtIdx;
		prNdpCxt->fgValid = TRUE;
	}

	for (u4Idx = 0; u4Idx < prNdpCxt->ucNumEnrollee; u4Idx++) {
		if (prNdpCxt->aprEnrollNdp[u4Idx] == prNDP)
			break;
	}
	if (u4Idx == prNdpCxt->ucNumEnrollee) {

		if (prNdpCxt->ucNumEnrollee > NAN_MAX_SUPPORT_NDP_NUM) {
			DBGLOG(NAN, ERROR,
				"[%s] Exceed NAN_MAX_SUPPORT_NDP_NUM\n",
				__func__);
			return WLAN_STATUS_FAILURE;
		}

		prNdpCxt->aprEnrollNdp[prNdpCxt->ucNumEnrollee] = prNDP;
		prNdpCxt->ucNumEnrollee++;
	} else {
		for (; u4Idx < (prNdpCxt->ucNumEnrollee - 1); u4Idx++)
			prNdpCxt->aprEnrollNdp[u4Idx] =
				prNdpCxt->aprEnrollNdp[u4Idx + 1];
		prNdpCxt->aprEnrollNdp[u4Idx] = prNDP;
	}

	/* sorting NDP by SA */
	for (u4Idx = (prNdpCxt->ucNumEnrollee - 1); u4Idx >= 1; u4Idx--) {
		if (nanSecCompareSA(prAdapter,
				    prNdpCxt->aprEnrollNdp[u4Idx - 1],
				    prNdpCxt->aprEnrollNdp[u4Idx]) > 0)
			break;

		prNdpTmp = prNdpCxt->aprEnrollNdp[u4Idx - 1];
		prNdpCxt->aprEnrollNdp[u4Idx - 1] =
			prNdpCxt->aprEnrollNdp[u4Idx];
		prNdpCxt->aprEnrollNdp[u4Idx] = prNdpTmp;
	}
	prTargetNdpSA = prNdpCxt->aprEnrollNdp[0];
	fgSecurityRequired = prTargetNdpSA->fgSecurityRequired;

	eBand = nanSchedGetSchRecBandByMac(prAdapter, pucPeerNMI);
	ucBssIndex = nanGetBssIdxbyBand(prAdapter, eBand);
	nanSetPreferLinkStaRec(prAdapter, prNdpCxt, ucBssIndex);

	if (nanDataEngineAllocStaRec(prAdapter, prNDL, ucBssIndex, pucPeerNMI,
		prNDP->ucRCPI, &prNdpCxt->prNanPreferStaRec) !=
	    WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

	prNanStaRec = nanGetPreferLinkStaRec(prAdapter, prNdpCxt);
	/* update SA with strongest security */
	if (!prNanStaRec) {
		DBGLOG(NAN, ERROR, "[%s] prNanStaRec error\n", __func__);
		return WLAN_STATUS_FAILURE;
	}

	/* always set to state 3 for data path operation */
	cnmStaRecChangeState(prAdapter, prNanStaRec, STA_STATE_3);

	/* Notify scheduler */
	nanSchedCmdMapStaRecord(prAdapter, prNDL->aucPeerMacAddr,
				NAN_BSS_INDEX_BAND0,
				prNanStaRec->ucIndex,
				prNdpCxt->ucId,
				prNanStaRec->ucWlanIndex,
				prNDP->aucPeerNDIAddr);

	if (fgSecurityRequired == FALSE)
		nanSecResetTk(prNanStaRec);
	else
		nanSecInstallTk(prTargetNdpSA, prNanStaRec);

	nicTxFreeDescTemplate(prAdapter, prNanStaRec);
	nicTxGenerateDescTemplate(prAdapter, prNanStaRec);

#if (ENABLE_NDP_UT_LOG == 1)
	DBGLOG(NAN, DEBUG, "[%s] Summary\n", __func__);
	DBGLOG(NAN, DEBUG,
	       "Peer NMI=> %02x:%02x:%02x:%02x:%02x:%02x, NdpNum:%d\n",
	       prNDL->aucPeerMacAddr[0], prNDL->aucPeerMacAddr[1],
	       prNDL->aucPeerMacAddr[2], prNDL->aucPeerMacAddr[3],
	       prNDL->aucPeerMacAddr[4], prNDL->aucPeerMacAddr[5],
	       prNDL->ucNDPNum);
	for (u4NdpCxtIdx = 0; u4NdpCxtIdx < NAN_MAX_SUPPORT_NDP_CXT_NUM;
	     u4NdpCxtIdx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		nanDumpStaRec(prNdpCxt);

		DBGLOG(NAN, DEBUG, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucLocalNDIAddr[0],
		       prNdpCxt->aucLocalNDIAddr[1],
		       prNdpCxt->aucLocalNDIAddr[2],
		       prNdpCxt->aucLocalNDIAddr[3],
		       prNdpCxt->aucLocalNDIAddr[4],
		       prNdpCxt->aucLocalNDIAddr[5]);
		DBGLOG(NAN, DEBUG, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucPeerNDIAddr[0], prNdpCxt->aucPeerNDIAddr[1],
		       prNdpCxt->aucPeerNDIAddr[2], prNdpCxt->aucPeerNDIAddr[3],
		       prNdpCxt->aucPeerNDIAddr[4],
		       prNdpCxt->aucPeerNDIAddr[5]);

		for (u4Idx = 0; u4Idx < prNdpCxt->ucNumEnrollee; u4Idx++) {
			DBGLOG(NAN, DEBUG, "NDP#%d, Sec:%d, NDPID:%d\n", u4Idx,
			       prNdpCxt->aprEnrollNdp[u4Idx]
				       ->fgSecurityRequired,
			       prNdpCxt->aprEnrollNdp[u4Idx]->ucNDPID);
		}
	}
#endif

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDataEngineUnrollNMIContext(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint32_t u4Idx;
	uint32_t u4NdpCxtIdx;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;
	struct _NAN_NDP_INSTANCE_T *prTargetNdpSA;
	uint8_t *pucLocalNMI;
	uint8_t *pucPeerNMI;
	enum NAN_BSS_ROLE_INDEX eRole = NAN_BSS_INDEX_BAND0;
	struct STA_RECORD *prNanStaRec = NULL;

#if !CFG_NAN_PMF_PATCH
	return WLAN_STATUS_SUCCESS;
#endif

	if ((prNDL == NULL) || (prNDP == NULL))
		return WLAN_STATUS_FAILURE;

	pucLocalNMI = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	pucPeerNMI = prNDL->aucPeerMacAddr;

	if ((kalMemCmp(pucLocalNMI, prNDP->aucLocalNDIAddr, MAC_ADDR_LEN) ==
	     0) &&
	    (kalMemCmp(pucPeerNMI, prNDP->aucPeerNDIAddr, MAC_ADDR_LEN) == 0))
		return WLAN_STATUS_SUCCESS;

	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4Idx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		if ((kalMemCmp(prNdpCxt->aucLocalNDIAddr, pucLocalNMI,
			       MAC_ADDR_LEN) == 0) &&
		    (kalMemCmp(prNdpCxt->aucPeerNDIAddr, pucPeerNMI,
			       MAC_ADDR_LEN) == 0))
			break;
	}
	if (u4Idx >= NAN_MAX_SUPPORT_NDP_CXT_NUM)
		return WLAN_STATUS_FAILURE;
	u4NdpCxtIdx = u4Idx;

	prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_NUM; u4Idx++) {
		if (prNdpCxt->aprEnrollNdp[u4Idx] == prNDP)
			break;
	}
	if (u4Idx == NAN_MAX_SUPPORT_NDP_NUM)
		return WLAN_STATUS_FAILURE;

	/* update SA with strongest security */
	prTargetNdpSA = NULL;
	prNanStaRec = nanGetPreferLinkStaRec(prAdapter, prNdpCxt);
	if (prNdpCxt->ucNumEnrollee > 1) {
		if (u4Idx == 0) {
			prTargetNdpSA = prNdpCxt->aprEnrollNdp[1];

			if (!prNanStaRec) {
				DBGLOG(NAN, ERROR,
					"[%s] prNanStaRec error\n", __func__);
				return WLAN_STATUS_FAILURE;
			}

			if (prTargetNdpSA->fgSecurityRequired == FALSE)
				nanSecResetTk(prNanStaRec);
			else if (prAdapter->rWifiVar.fgNanUnrollInstallTk)
				nanSecInstallTk(prTargetNdpSA,
						prNanStaRec);
		} else {
			prTargetNdpSA = prNdpCxt->aprEnrollNdp[0];
		}
	}

	/* free STA-REC for deactivating data path operation
	 * STARec only establish when NDP setup success (NORMAL_TR)
	 *  in NDP case
	 */
	if (prNDP->fgNDPEstablish == TRUE)
		nanDataEngineFreeStaRec(prAdapter, prNDL,
					&prNanStaRec);

	if (prNanStaRec == NULL) {
		nanSchedCmdMapStaRecord(prAdapter, prNDL->aucPeerMacAddr, eRole,
					STA_REC_INDEX_NOT_FOUND,
					prNdpCxt->ucId,
					STA_REC_INDEX_NOT_FOUND,
					prNDP->aucPeerNDIAddr);
	} else {
		nanSchedCmdMapStaRecord(prAdapter, prNDL->aucPeerMacAddr, eRole,
					prNanStaRec->ucIndex,
					prNdpCxt->ucId,
					prNanStaRec->ucWlanIndex,
					prNDP->aucPeerNDIAddr);
	}

	for (; u4Idx < (NAN_MAX_SUPPORT_NDP_NUM - 1); u4Idx++)
		prNdpCxt->aprEnrollNdp[u4Idx] =
			prNdpCxt->aprEnrollNdp[u4Idx + 1];
	prNdpCxt->ucNumEnrollee--;
	if (prNdpCxt->ucNumEnrollee == 0) {
		if (prNanStaRec &&
		    atomic_read(&(prNanStaRec->NanRefCount)) > 0) {
			DBGLOG(NAN, WARN, "%s(): STA-REC RefCount:%d\n",
			       __func__,
			       atomic_read(
				       &(prNanStaRec->NanRefCount)));
		}

		prNdpCxt->fgValid = FALSE;
	} else {
		if (prNanStaRec) {
			nicTxFreeDescTemplate(prAdapter, prNanStaRec);
			nicTxGenerateDescTemplate(prAdapter,
				prNanStaRec);
		} else {
			DBGLOG(NAN, WARN,
				"[%s] prNanStaRec = NULL\n", __func__);
		}
	}

#if (ENABLE_NDP_UT_LOG == 1)
	DBGLOG(NAN, DEBUG, "[%s] Summary\n", __func__);
	DBGLOG(NAN, DEBUG,
	       "Peer NMI=> %02x:%02x:%02x:%02x:%02x:%02x, NdpNum:%d\n",
	       prNDL->aucPeerMacAddr[0], prNDL->aucPeerMacAddr[1],
	       prNDL->aucPeerMacAddr[2], prNDL->aucPeerMacAddr[3],
	       prNDL->aucPeerMacAddr[4], prNDL->aucPeerMacAddr[5],
	       prNDL->ucNDPNum);
	for (u4NdpCxtIdx = 0; u4NdpCxtIdx < NAN_MAX_SUPPORT_NDP_CXT_NUM;
	     u4NdpCxtIdx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		nanDumpStaRec(prNdpCxt);

		DBGLOG(NAN, DEBUG, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucLocalNDIAddr[0],
		       prNdpCxt->aucLocalNDIAddr[1],
		       prNdpCxt->aucLocalNDIAddr[2],
		       prNdpCxt->aucLocalNDIAddr[3],
		       prNdpCxt->aucLocalNDIAddr[4],
		       prNdpCxt->aucLocalNDIAddr[5]);
		DBGLOG(NAN, DEBUG, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucPeerNDIAddr[0], prNdpCxt->aucPeerNDIAddr[1],
		       prNdpCxt->aucPeerNDIAddr[2], prNdpCxt->aucPeerNDIAddr[3],
		       prNdpCxt->aucPeerNDIAddr[4],
		       prNdpCxt->aucPeerNDIAddr[5]);

		for (u4Idx = 0; u4Idx < prNdpCxt->ucNumEnrollee; u4Idx++) {
			DBGLOG(NAN, DEBUG, "NDP#%d, Sec:%d, NDPID:%d\n", u4Idx,
			       prNdpCxt->aprEnrollNdp[u4Idx]
				       ->fgSecurityRequired,
			       prNdpCxt->aprEnrollNdp[u4Idx]->ucNDPID);
		}
	}
#endif

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDataEngineEnrollNDPContext(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint32_t u4Idx;
	uint32_t u4NdpCxtIdx = NAN_MAX_SUPPORT_NDP_CXT_NUM;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;
	struct _NAN_NDP_INSTANCE_T *prTargetNdpSA;
	struct _NAN_NDP_INSTANCE_T *prNdpTmp;
	enum NAN_BSS_ROLE_INDEX eRole = NAN_BSS_INDEX_BAND0;
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;
	unsigned char fgSecurityRequired;
	uint8_t ucBssIndex;
	enum ENUM_BAND eBand;
	uint32_t i = 0;
	struct STA_RECORD *prNanStaRec = NULL;

	if ((prNDL == NULL) || (prNDP == NULL))
		return WLAN_STATUS_FAILURE;

	u4NdpCxtIdx = NAN_MAX_SUPPORT_NDP_CXT_NUM;
	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4Idx];
		if (prNdpCxt->fgValid == FALSE) {
			if (u4NdpCxtIdx == NAN_MAX_SUPPORT_NDP_CXT_NUM)
				u4NdpCxtIdx = u4Idx;
			continue;
		}

		if ((kalMemCmp(prNdpCxt->aucLocalNDIAddr,
			       prNDP->aucLocalNDIAddr, MAC_ADDR_LEN) == 0) &&
		    (kalMemCmp(prNdpCxt->aucPeerNDIAddr, prNDP->aucPeerNDIAddr,
			       MAC_ADDR_LEN) == 0)) {

			u4NdpCxtIdx = u4Idx;
			break;
		}
	}
	if (u4NdpCxtIdx == NAN_MAX_SUPPORT_NDP_CXT_NUM)
		return WLAN_STATUS_FAILURE;

	prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
	if (prNdpCxt->fgValid == FALSE) {
		kalMemZero((uint8_t *)prNdpCxt,
			   sizeof(struct _NAN_NDP_CONTEXT_T));

		kalMemCopy(prNdpCxt->aucLocalNDIAddr, prNDP->aucLocalNDIAddr,
			   MAC_ADDR_LEN);
		kalMemCopy(prNdpCxt->aucPeerNDIAddr, prNDP->aucPeerNDIAddr,
			   MAC_ADDR_LEN);

		nanResetStaRec(prNdpCxt);

		DBGLOG(NAN, DEBUG, "Allocate NDP Cxt %d\n", u4NdpCxtIdx);
		DBGLOG(NAN, DEBUG, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNDP->aucLocalNDIAddr[0], prNDP->aucLocalNDIAddr[1],
		       prNDP->aucLocalNDIAddr[2], prNDP->aucLocalNDIAddr[3],
		       prNDP->aucLocalNDIAddr[4], prNDP->aucLocalNDIAddr[5]);
		DBGLOG(NAN, DEBUG, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNDP->aucPeerNDIAddr[0], prNDP->aucPeerNDIAddr[1],
		       prNDP->aucPeerNDIAddr[2], prNDP->aucPeerNDIAddr[3],
		       prNDP->aucPeerNDIAddr[4], prNDP->aucPeerNDIAddr[5]);

		prNdpCxt->ucId = (uint8_t)u4NdpCxtIdx;
		prNdpCxt->fgValid = TRUE;
	}

	for (u4Idx = 0; u4Idx < prNdpCxt->ucNumEnrollee; u4Idx++) {
		if (prNdpCxt->aprEnrollNdp[u4Idx] == prNDP)
			break;
	}
	if (u4Idx == prNdpCxt->ucNumEnrollee) {
		if (prNdpCxt->ucNumEnrollee > NAN_MAX_SUPPORT_NDP_NUM) {
			DBGLOG(NAN, ERROR,
				"[%s] Exceed NAN_MAX_SUPPORT_NDP_NUM\n",
				__func__);
			return WLAN_STATUS_FAILURE;
		}

		prNdpCxt->aprEnrollNdp[prNdpCxt->ucNumEnrollee] = prNDP;
		prNdpCxt->ucNumEnrollee++;
	} else {
		for (; u4Idx < (prNdpCxt->ucNumEnrollee - 1); u4Idx++)
			prNdpCxt->aprEnrollNdp[u4Idx] =
				prNdpCxt->aprEnrollNdp[u4Idx + 1];
		prNdpCxt->aprEnrollNdp[u4Idx] = prNDP;
	}
	prNDP->prContext = prNdpCxt;

	/* sorting NDP by SA */
	for (u4Idx = (prNdpCxt->ucNumEnrollee - 1); u4Idx >= 1; u4Idx--) {
		if (nanSecCompareSA(prAdapter,
				    prNdpCxt->aprEnrollNdp[u4Idx - 1],
				    prNdpCxt->aprEnrollNdp[u4Idx]) > 0)
			break;

		prNdpTmp = prNdpCxt->aprEnrollNdp[u4Idx - 1];
		prNdpCxt->aprEnrollNdp[u4Idx - 1] =
			prNdpCxt->aprEnrollNdp[u4Idx];
		prNdpCxt->aprEnrollNdp[u4Idx] = prNdpTmp;
	}
	prTargetNdpSA = prNdpCxt->aprEnrollNdp[0];
	fgSecurityRequired = prTargetNdpSA->fgSecurityRequired;

	eBand = nanSchedGetSchRecBandByMac(prAdapter, prNDP->aucPeerNDIAddr);
	if (eBand == BAND_NULL) {
		/* Workaround: use NMI address to find peerSchRec,
		 * if search by NDI fail
		 */
		eBand = nanSchedGetSchRecBandByMac(prAdapter,
				prNDL->aucPeerMacAddr);
		DBGLOG(NAN, WARN,
			"Search peerSchRec fail, use NMI, band:%d\n", eBand);
	}
	ucBssIndex = nanGetBssIdxbyBand(prAdapter, eBand);

	for (i = 0;
		i < prAdapter->rWifiVar.ucNanMldLinkMax;
		i++) {
		prNanStaRec = nanGetLinkStaRec(prNdpCxt, i);

#if (CFG_SUPPORT_NAN_11BE_MLO == 1)
		if (prAdapter->rWifiVar.ucNanMldLinkMax > 1)
			ucBssIndex = nanGetBssIdxbyLink(prAdapter, i);
#endif

		if (nanDataEngineAllocStaRec(
			prAdapter,
			prNDL,
			ucBssIndex,
			prNDP->aucPeerNDIAddr,
			prNDP->ucRCPI,
			&prNanStaRec) !=
		    WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;

		/* update SA with strongest security */
		if (!prNanStaRec) {
			DBGLOG(NAN, ERROR,
				"prNanStaRec error\n");
			return WLAN_STATUS_FAILURE;
		}

		nanSetLinkStaRec(prNdpCxt, prNanStaRec, i);
	}

	for (i = 0;
		i < prAdapter->rWifiVar.ucNanMldLinkMax;
		i++) {
		prNanStaRec = nanGetLinkStaRec(prNdpCxt, i);
		if (!prNanStaRec)
			continue;

		/* always set to state 3 for data path operation */
		cnmStaRecChangeState(prAdapter,
			prNanStaRec,
			STA_STATE_3);

		if (fgSecurityRequired == FALSE)
			nanSecResetTk(prNanStaRec);
		else
			nanSecInstallTk(prTargetNdpSA, prNanStaRec);

		nicTxFreeDescTemplate(prAdapter, prNanStaRec);
		nicTxGenerateDescTemplate(prAdapter, prNanStaRec);

		/* Notify scheduler */
		if (i && (eBand == BAND_2G4)) {
			DBGLOG(NAN, INFO,
				"Prefer sta is 2G, skip map\n");
			continue;
		}

		nanSchedCmdMapStaRecord(
			prAdapter,
			prNDL->aucPeerMacAddr,
			nanGetRoleIndexbyLink(i),
			prNanStaRec->ucIndex,
			prNDP->prContext->ucId,
			prNanStaRec->ucWlanIndex,
			prNDP->aucPeerNDIAddr);
	}

	nanSetPreferLinkStaRec(prAdapter,
		prNdpCxt,
		nanGetBssIdxbyBand(prAdapter, eBand));

	prDataPathInfo = &(prAdapter->rDataPathInfo);
	if (atomic_inc_return(&(prDataPathInfo->NetDevRefCount[eRole])) == 1) {
		/* netif_carrier_on */
		kalNanIndicateStatusAndComplete(prAdapter->prGlueInfo,
						WLAN_STATUS_MEDIA_CONNECT,
						eRole);
	}

	nanDataEngineEnrollNMIContext(prAdapter, prNDL, prNDP);

#if (ENABLE_NDP_UT_LOG == 1)
	DBGLOG(NAN, DEBUG, "[%s] Summary\n", __func__);
	DBGLOG(NAN, DEBUG,
	       "Peer NMI=> %02x:%02x:%02x:%02x:%02x:%02x, NdpNum:%d\n",
	       prNDL->aucPeerMacAddr[0], prNDL->aucPeerMacAddr[1],
	       prNDL->aucPeerMacAddr[2], prNDL->aucPeerMacAddr[3],
	       prNDL->aucPeerMacAddr[4], prNDL->aucPeerMacAddr[5],
	       prNDL->ucNDPNum);
	for (u4NdpCxtIdx = 0; u4NdpCxtIdx < NAN_MAX_SUPPORT_NDP_CXT_NUM;
	     u4NdpCxtIdx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		nanDumpStaRec(prNdpCxt);

		DBGLOG(NAN, DEBUG, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucLocalNDIAddr[0],
		       prNdpCxt->aucLocalNDIAddr[1],
		       prNdpCxt->aucLocalNDIAddr[2],
		       prNdpCxt->aucLocalNDIAddr[3],
		       prNdpCxt->aucLocalNDIAddr[4],
		       prNdpCxt->aucLocalNDIAddr[5]);
		DBGLOG(NAN, DEBUG, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucPeerNDIAddr[0], prNdpCxt->aucPeerNDIAddr[1],
		       prNdpCxt->aucPeerNDIAddr[2], prNdpCxt->aucPeerNDIAddr[3],
		       prNdpCxt->aucPeerNDIAddr[4],
		       prNdpCxt->aucPeerNDIAddr[5]);

		for (u4Idx = 0; u4Idx < prNdpCxt->ucNumEnrollee; u4Idx++) {
			DBGLOG(NAN, DEBUG, "NDP#%d, Sec:%d, NDPID:%d\n", u4Idx,
			       prNdpCxt->aprEnrollNdp[u4Idx]
				       ->fgSecurityRequired,
			       prNdpCxt->aprEnrollNdp[u4Idx]->ucNDPID);
		}
	}
#endif

	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanDataEngineUnrollNDPContext(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL,
			      struct _NAN_NDP_INSTANCE_T *prNDP) {
	uint32_t u4Idx;
	uint32_t u4NdpCxtIdx;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;
	struct _NAN_NDP_INSTANCE_T *prTargetNdpSA;
	enum NAN_BSS_ROLE_INDEX eRole = NAN_BSS_INDEX_BAND0;
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;
	uint32_t i = 0;
	struct STA_RECORD *prNanStaRec = NULL;

	if ((prNDL == NULL) || (prNDP == NULL))
		return WLAN_STATUS_FAILURE;

	if (prNDP->prContext == NULL)
		return WLAN_STATUS_FAILURE;

	nanDataEngineUnrollNMIContext(prAdapter, prNDL, prNDP);

	prNdpCxt = prNDP->prContext;
	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_NUM; u4Idx++) {
		if (prNdpCxt->aprEnrollNdp[u4Idx] == prNDP)
			break;
	}
	if (u4Idx == NAN_MAX_SUPPORT_NDP_NUM)
		return WLAN_STATUS_FAILURE;

	/* update SA with strongest security */
	prTargetNdpSA = NULL;
	if (prNdpCxt->ucNumEnrollee > 1) {
		if (u4Idx == 0) {
			prTargetNdpSA = prNdpCxt->aprEnrollNdp[1];
			if (nanLinkResetTk(prAdapter, prNdpCxt)) {
				DBGLOG(NAN, ERROR,
					"prNanStaRec error\n");
				return WLAN_STATUS_FAILURE;
			}
		} else {
			prTargetNdpSA = prNdpCxt->aprEnrollNdp[0];
		}
	}

	/* free STA-REC for deactivating data path operation
	 * STARec only establish when NDP setup success (NORMAL_TR)
	 *  in NDP case
	 */
	for (i = 0;
		i < prAdapter->rWifiVar.ucNanMldLinkMax;
		i++) {
		prNanStaRec = nanGetLinkStaRec(prNdpCxt, i);
		if (!prNanStaRec)
			continue;

		if (prNDP->fgNDPEstablish == TRUE)
			nanDataEngineFreeStaRec(
				prAdapter,
				prNDL,
				&prNanStaRec);

		if (prNdpCxt->ucNumEnrollee > 1)
			goto skip_map_starecord;

		if (prNanStaRec == NULL) {
			nanSchedCmdMapStaRecord(
				prAdapter,
				prNDL->aucPeerMacAddr,
				nanGetRoleIndexbyLink(i),
				STA_REC_INDEX_NOT_FOUND,
				prNdpCxt->ucId,
				STA_REC_INDEX_NOT_FOUND,
				prNDP->aucPeerNDIAddr);
		} else {
			nanSchedCmdMapStaRecord(
				prAdapter,
				prNDL->aucPeerMacAddr,
				nanGetRoleIndexbyLink(i),
				prNanStaRec->ucIndex,
				prNdpCxt->ucId,
				prNanStaRec->ucWlanIndex,
				prNDP->aucPeerNDIAddr);
		}
	}

skip_map_starecord:

	prDataPathInfo = &(prAdapter->rDataPathInfo);
	if (atomic_dec_return(&(prDataPathInfo->NetDevRefCount[eRole])) == 0) {
		/* netif_carrier_off */
		kalNanIndicateStatusAndComplete(prAdapter->prGlueInfo,
						WLAN_STATUS_MEDIA_DISCONNECT,
						eRole);
	}

	for (; u4Idx < (NAN_MAX_SUPPORT_NDP_NUM - 1); u4Idx++)
		prNdpCxt->aprEnrollNdp[u4Idx] =
			prNdpCxt->aprEnrollNdp[u4Idx + 1];
	prNdpCxt->ucNumEnrollee--;
	if (prNdpCxt->ucNumEnrollee == 0) {
		for (i = 0;
			i < prAdapter->rWifiVar.ucNanMldLinkMax;
			i++) {
			prNanStaRec = nanGetLinkStaRec(prNdpCxt, i);

			if (prNanStaRec &&
			    atomic_read(&(prNanStaRec->NanRefCount)) > 0) {
				DBGLOG(NAN, WARN,
					"STA-REC[%d] RefCount:%d\n", i,
				       atomic_read(
					       &(prNanStaRec->NanRefCount)));
			}
		}
		prNdpCxt->fgValid = FALSE;
	} else {
		for (i = 0;
			i < prAdapter->rWifiVar.ucNanMldLinkMax;
			i++) {
			prNanStaRec = nanGetLinkStaRec(prNdpCxt, i);
			if (prNanStaRec) {
				nicTxFreeDescTemplate(
					prAdapter,
					prNanStaRec);
				nicTxGenerateDescTemplate(
					prAdapter,
					prNanStaRec);
			} else {
				DBGLOG(NAN, WARN,
					"prNanStaRec[%d] = NULL\n", i);
			}
		}
	}
	prNDP->prContext = NULL;

#if (ENABLE_NDP_UT_LOG == 1)
	DBGLOG(NAN, DEBUG, "[%s] Summary\n", __func__);
	DBGLOG(NAN, DEBUG,
	       "Peer NMI=> %02x:%02x:%02x:%02x:%02x:%02x, NdpNum:%d\n",
	       prNDL->aucPeerMacAddr[0], prNDL->aucPeerMacAddr[1],
	       prNDL->aucPeerMacAddr[2], prNDL->aucPeerMacAddr[3],
	       prNDL->aucPeerMacAddr[4], prNDL->aucPeerMacAddr[5],
	       prNDL->ucNDPNum);
	for (u4NdpCxtIdx = 0; u4NdpCxtIdx < NAN_MAX_SUPPORT_NDP_CXT_NUM;
	     u4NdpCxtIdx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4NdpCxtIdx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		nanDumpStaRec(prNdpCxt);

		DBGLOG(NAN, DEBUG, "Local=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucLocalNDIAddr[0],
		       prNdpCxt->aucLocalNDIAddr[1],
		       prNdpCxt->aucLocalNDIAddr[2],
		       prNdpCxt->aucLocalNDIAddr[3],
		       prNdpCxt->aucLocalNDIAddr[4],
		       prNdpCxt->aucLocalNDIAddr[5]);
		DBGLOG(NAN, DEBUG, "Peer=> %02x:%02x:%02x:%02x:%02x:%02x\n",
		       prNdpCxt->aucPeerNDIAddr[0], prNdpCxt->aucPeerNDIAddr[1],
		       prNdpCxt->aucPeerNDIAddr[2], prNdpCxt->aucPeerNDIAddr[3],
		       prNdpCxt->aucPeerNDIAddr[4],
		       prNdpCxt->aucPeerNDIAddr[5]);

		for (u4Idx = 0; u4Idx < prNdpCxt->ucNumEnrollee; u4Idx++) {
			DBGLOG(NAN, DEBUG, "NDP#%d, Sec:%d, NDPID:%d\n", u4Idx,
			       prNdpCxt->aprEnrollNdp[u4Idx]
				       ->fgSecurityRequired,
			       prNdpCxt->aprEnrollNdp[u4Idx]->ucNDPID);
		}
	}
#endif

	return WLAN_STATUS_SUCCESS;
}

struct STA_RECORD *nanDataEngineSearchNDPContext(struct ADAPTER *prAdapter,
					struct _NAN_NDL_INSTANCE_T *prNDL,
					uint8_t *pucLocalAddr,
					uint8_t *pucPeerAddr)
{
	uint32_t u4Idx;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;

	if (prNDL == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return NULL;
	}

	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4Idx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		if (EQUAL_MAC_ADDR(prNdpCxt->aucLocalNDIAddr, pucLocalAddr) &&
		    EQUAL_MAC_ADDR(prNdpCxt->aucPeerNDIAddr, pucPeerAddr))
			return nanGetPreferLinkStaRec(prAdapter, prNdpCxt);
	}

	return NULL;
}

struct _NAN_NDP_CONTEXT_T *
nanDataEngineSearchFirstNDP(struct ADAPTER *prAdapter,
	      struct _NAN_NDL_INSTANCE_T *prNDL,
	      uint8_t *pucLocalAddr, uint8_t *pucPeerAddr)
{
	uint32_t u4Idx;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;

	if (prNDL == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL error\n", __func__);
		return NULL;
	}

	for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
		prNdpCxt = &prNDL->arNdpCxt[u4Idx];
		if (prNdpCxt->fgValid == FALSE)
			continue;

		DBGLOG(NAN, ERROR,
			"[%s] Local[" MACSTR "], Peer[" MACSTR "]\n",
			__func__,
			MAC2STR(prNdpCxt->aucLocalNDIAddr),
			MAC2STR(prNdpCxt->aucPeerNDIAddr));

		if ((kalMemCmp(prNdpCxt->aucLocalNDIAddr, pucLocalAddr,
			       MAC_ADDR_LEN) == 0) &&
		    (kalMemCmp(prNdpCxt->aucPeerNDIAddr, pucPeerAddr,
			       MAC_ADDR_LEN) != 0))
			return prNdpCxt;
	}

	return NULL;
}

#endif /* CFG_SUPPORT_NAN */
