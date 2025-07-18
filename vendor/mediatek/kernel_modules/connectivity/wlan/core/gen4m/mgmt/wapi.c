// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "wapi.c"
 *    \brief  This file including the WAPI related function.
 *
 *    This file provided the macros and functions library support
 *    the wapi ie parsing, cipher and AKM check to help the AP seleced deciding
 */

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

#include "precomp.h"
#if CFG_SUPPORT_WAPI

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to parse WAPI IE.
 *
 * \param[in]  prInfoElem Pointer to the RSN IE
 * \param[out] prRsnInfo Pointer to the BSSDescription structure to store the
 *                  WAPI information from the given WAPI IE
 *
 * \retval TRUE - Succeeded
 * \retval FALSE - Failed
 */
/*----------------------------------------------------------------------------*/
u_int8_t wapiParseWapiIE(struct WAPI_INFO_ELEM *prInfoElem,
			 struct WAPI_INFO *prWapiInfo)
{
	uint32_t i;
	int32_t u4RemainWapiIeLen;
	uint16_t u2Version;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = WAPI_CIPHER_SUITE_WPI;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;

	/* Verify the length of the WAPI IE. */
	if (prInfoElem->ucLength < 6) {
		DBGLOG(SEC, TRACE, "WAPI IE length too short (length=%d)\n",
		       prInfoElem->ucLength);
		return FALSE;
	}

	/* Check WAPI version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != 1) {
		DBGLOG(SEC, TRACE, "Unsupported WAPI IE version: %d\n",
		       u2Version);
		return FALSE;
	}

	cp = (uint8_t *) &prInfoElem->u2AKMSuiteCount;
	u4RemainWapiIeLen = (int32_t) prInfoElem->ucLength - 2;

	do {
		if (u4RemainWapiIeLen == 0)
			break;

		/*
		 *  AuthCount    : 2
		 *  AuthSuite    : 4 * authSuiteCount
		 *  PairwiseCount: 2
		 *  PairwiseSuite: 4 * pairSuiteCount
		 *  GroupSuite   : 4
		 *  Cap          : 2
		 */

		/* Parse the Authentication
		 *   and Key Management Cipher Suite Count field.
		 */
		if (u4RemainWapiIeLen < 2) {
			DBGLOG(SEC, TRACE,
				"Fail to parse WAPI IE in auth & key mgt suite count (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainWapiIeLen -= 2;

		/* Parse the Authentication
		 *   and Key Management Cipher Suite List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainWapiIeLen < (int32_t) i) {
			DBGLOG(SEC, TRACE,
				"Fail to parse WAPI IE in auth & key mgt suite list (IE len: %d)\n",
				prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainWapiIeLen -= (int32_t) i;

		if (u4RemainWapiIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainWapiIeLen < 2) {
			DBGLOG(SEC, TRACE,
			       "Fail to parse WAPI IE in pairwise cipher suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainWapiIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainWapiIeLen < (int32_t) i) {
			DBGLOG(SEC, TRACE,
			       "Fail to parse WAPI IE in pairwise cipher suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainWapiIeLen -= (int32_t) i;

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainWapiIeLen < 4) {
			DBGLOG(SEC, TRACE,
			       "Fail to parse WAPI IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainWapiIeLen -= 4;

		/* Parse the WAPI u2Capabilities field. */
		if (u4RemainWapiIeLen < 2) {
			DBGLOG(SEC, TRACE,
			       "Fail to parse WAPI IE in WAPI capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
		u4RemainWapiIeLen -= 2;

		/* Todo:: BKID support */
	} while (FALSE);

	/* Save the WAPI information for the BSS. */

	prWapiInfo->ucElemId = ELEM_ID_WAPI;

	prWapiInfo->u2Version = u2Version;

	prWapiInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(SEC, LOUD,
	       "WAPI: version %d, group key cipher suite %02x-%02x-%02x-%02x\n",
	       u2Version, (uint8_t) (u4GroupSuite & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupSuite >> 24) & 0x000000FF));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_WAPI_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_WAPI_CIPHER_SUITES;

		prWapiInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
					  &prWapiInfo->au4PairwiseKeyCipherSuite
					  [i]);
			pucPairSuite += 4;

			DBGLOG(SEC, LOUD,
			       "WAPI: pairwise key cipher suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t) (prWapiInfo->au4PairwiseKeyCipherSuite
					  [i] & 0x000000FF),
			       (uint8_t) ((prWapiInfo->au4PairwiseKeyCipherSuite
					   [i] >> 8) & 0x000000FF),
			       (uint8_t) ((prWapiInfo->au4PairwiseKeyCipherSuite
					   [i] >> 16) & 0x000000FF),
			       (uint8_t) ((prWapiInfo->au4PairwiseKeyCipherSuite
					   [i] >> 24) & 0x000000FF));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 *   is not present.
		 *  Use the default chipher suite for WAPI: WPI.
		 */
		prWapiInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prWapiInfo->au4PairwiseKeyCipherSuite[0] =
		    WAPI_CIPHER_SUITE_WPI;

		DBGLOG(SEC, LOUD,
		       "WAPI: pairwise key cipher suite: %02x-%02x-%02x-%02x (default)\n",
		       (uint8_t) (prWapiInfo->au4PairwiseKeyCipherSuite[0] &
				  0x000000FF),
		       (uint8_t) ((prWapiInfo->au4PairwiseKeyCipherSuite[0] >>
				   8) & 0x000000FF),
		       (uint8_t) ((prWapiInfo->au4PairwiseKeyCipherSuite[0] >>
				   16) & 0x000000FF),
		       (uint8_t) ((prWapiInfo->au4PairwiseKeyCipherSuite[0] >>
				   24) & 0x000000FF));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and
		 *   key management suites is present.
		 */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_WAPI_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_WAPI_AKM_SUITES;

		prWapiInfo->u4AuthKeyMgtSuiteCount =
		    (uint32_t) u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prWapiInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(SEC, LOUD,
			       "WAPI: AKM suite [%d]: %02x-%02x-%02x-%02x\n",
			       (uint8_t) i,
			       (uint8_t) (prWapiInfo->au4AuthKeyMgtSuite[i] &
					  0x000000FF),
			       (uint8_t) ((prWapiInfo->au4AuthKeyMgtSuite[i] >>
					   8) & 0x000000FF),
			       (uint8_t) ((prWapiInfo->au4AuthKeyMgtSuite[i] >>
					   16) & 0x000000FF),
			       (uint8_t) ((prWapiInfo->au4AuthKeyMgtSuite[i] >>
					   24) & 0x000000FF));
		}
	} else {
		/* The information about the authentication and
		 *   key management suites is not present.
		 * Use the default AKM suite for WAPI.
		 */
		prWapiInfo->u4AuthKeyMgtSuiteCount = 1;
		prWapiInfo->au4AuthKeyMgtSuite[0] = WAPI_AKM_SUITE_802_1X;

		DBGLOG(SEC, LOUD,
		       "WAPI: AKM suite: %02x-%02x-%02x-%02x (default)\n",
		       (uint8_t) (prWapiInfo->au4AuthKeyMgtSuite[0] &
				  0x000000FF),
		       (uint8_t) ((prWapiInfo->au4AuthKeyMgtSuite[0] >> 8) &
				  0x000000FF),
		       (uint8_t) ((prWapiInfo->au4AuthKeyMgtSuite[0] >> 16) &
				  0x000000FF),
		       (uint8_t) ((prWapiInfo->au4AuthKeyMgtSuite[0] >> 24) &
				  0x000000FF));
	}

	prWapiInfo->u2WapiCap = u2Cap;
	DBGLOG(SEC, LOUD, "WAPI: cap: 0x%04x\n", prWapiInfo->u2WapiCap);

	return TRUE;
}				/* wapiParseWapiIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to perform WAPI policy selection for
 *        a given BSS.
 *
 * \param[in]  prAdapter Pointer to the adapter object data area.
 * \param[in]  prBss Pointer to the BSS description
 *
 * \retval TRUE - The WAPI policy selection for the given BSS is
 *                successful. The selected pairwise and group cipher suites
 *                are returned in the BSS description.
 * \retval FALSE - The WAPI policy selection for the given BSS is failed.
 *                 The driver shall not attempt to join the given BSS.
 *
 * \note The Encrypt status matched score will save to bss for final ap select.
 */
/*----------------------------------------------------------------------------*/
u_int8_t wapiPerformPolicySelection(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBss, uint8_t ucBssIndex)
{
	uint32_t i;
	uint32_t u4PairwiseCipher = 0;
	uint32_t u4GroupCipher = 0;
	uint32_t u4AkmSuite = 0;
	struct WAPI_INFO *prBssWapiInfo;
	struct WLAN_INFO *prWlanInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	/* Notice!!!! WAPI AP not set the privacy bit for WAI
	 * and WAI-PSK at WZC configuration mode
	 */
	prWlanInfo = &prAdapter->rWlanInfo;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	if (prBss->fgIEWAPI) {
		prBssWapiInfo = &prBss->rIEWAPI;
	} else {
		if (prConnSettings->fgWapiMode == FALSE) {
			DBGLOG(SEC, TRACE, "-- No Protected BSS\n");
			return TRUE;
		}
		DBGLOG(SEC, TRACE,
		       "WAPI Information Element does not exist.\n");
		return FALSE;
	}

	/* Select pairwise/group ciphers */
	for (i = 0; i < prBssWapiInfo->u4PairwiseKeyCipherSuiteCount; i++) {
		if (prBssWapiInfo->au4PairwiseKeyCipherSuite[i] ==
		    prConnSettings->u4WapiSelectedPairwiseCipher) {
			u4PairwiseCipher =
			    prBssWapiInfo->au4PairwiseKeyCipherSuite[i];
		}
	}
	if (prBssWapiInfo->u4GroupKeyCipherSuite ==
	    prConnSettings->u4WapiSelectedGroupCipher)
		u4GroupCipher = prBssWapiInfo->u4GroupKeyCipherSuite;

	/* Exception handler */
	/* If we cannot find proper pairwise and group cipher suites to join the
	 *  BSS, do not check the supported AKM suites.
	 */
	if (u4PairwiseCipher == 0 || u4GroupCipher == 0) {
		DBGLOG(SEC, TRACE,
		       "Failed to select pairwise/group cipher (0x%08x/0x%08x)\n",
		       u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}

	/* Select AKM */
	/* If the driver cannot support any authentication suites advertised in
	 *  the given BSS, we fail to perform RSNA policy selection.
	 */
	/* Attempt to find any overlapping supported AKM suite. */
	for (i = 0; i < prBssWapiInfo->u4AuthKeyMgtSuiteCount; i++) {
		if (prBssWapiInfo->au4AuthKeyMgtSuite[i] ==
		    prConnSettings->u4WapiSelectedAKMSuite) {
			u4AkmSuite = prBssWapiInfo->au4AuthKeyMgtSuite[i];
			break;
		}
	}

	if (u4AkmSuite == 0) {
		DBGLOG(SEC, TRACE, "Cannot support any AKM suites\n");
		return FALSE;
	}

	DBGLOG(SEC, TRACE,
	       "Selected pairwise/group cipher: %02x-%02x-%02x-%02x/%02x-%02x-%02x-%02x\n",
	       (uint8_t) (u4PairwiseCipher & 0x000000FF),
	       (uint8_t) ((u4PairwiseCipher >> 8) & 0x000000FF),
	       (uint8_t) ((u4PairwiseCipher >> 16) & 0x000000FF),
	       (uint8_t) ((u4PairwiseCipher >> 24) & 0x000000FF),
	       (uint8_t) (u4GroupCipher & 0x000000FF),
	       (uint8_t) ((u4GroupCipher >> 8) & 0x000000FF),
	       (uint8_t) ((u4GroupCipher >> 16) & 0x000000FF),
	       (uint8_t) ((u4GroupCipher >> 24) & 0x000000FF));

	DBGLOG(SEC, TRACE, "Selected AKM suite: %02x-%02x-%02x-%02x\n",
	       (uint8_t) (u4AkmSuite & 0x000000FF),
	       (uint8_t) ((u4AkmSuite >> 8) & 0x000000FF),
	       (uint8_t) ((u4AkmSuite >> 16) & 0x000000FF),
	       (uint8_t) ((u4AkmSuite >> 24) & 0x000000FF));

	return TRUE;
}				/* wapiPerformPolicySelection */


#endif
