// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "rsn.c"
 *  \brief  This file including the 802.11i, wpa and wpa2(rsn) related function.
 *
 *    This file provided the macros and functions library to support
 *	the wpa/rsn ie parsing, cipher and AKM check to help the AP seleced
 *	deciding, tkip mic error handler and rsn PMKID support.
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

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

uint8_t rsnKeyMgmtSae(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_SAE ||
	       akm == RSN_AKM_SUITE_SAE_EXT_KEY ||
	       akm == RSN_AKM_SUITE_FT_OVER_SAE ||
	       akm == RSN_AKM_SUITE_FT_SAE_EXT_KEY;
}

uint8_t rsnKeyMgmtEhtSae(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_SAE_EXT_KEY ||
	       akm == RSN_AKM_SUITE_FT_SAE_EXT_KEY;
}

uint8_t rsnKeyMgmtFT(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_FT_PSK ||
	       akm == RSN_AKM_SUITE_FT_PSK_SHA384 ||
	       akm == RSN_AKM_SUITE_FT_802_1X ||
	       akm == RSN_AKM_SUITE_FT_OVER_SAE ||
	       akm == RSN_AKM_SUITE_FT_802_1X_SHA384 ||
	       akm == RSN_AKM_SUITE_FT_802_1X_SHA384_UNRESTRICTED ||
	       akm == RSN_AKM_SUITE_FT_SAE_EXT_KEY ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA256 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA384;
}

uint8_t rsnKeyMgmtFils(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_FILS_SHA256 ||
	       akm == RSN_AKM_SUITE_FILS_SHA384 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA256 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA384;
}

uint32_t rsnKeyMgmtToAuthMode(enum ENUM_PARAM_AUTH_MODE eOriAuthMode,
	uint32_t version, uint32_t akm)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode = eOriAuthMode;

	if (version == IW_AUTH_WPA_VERSION_DISABLED) {
		switch (akm) {
		case RSN_AKM_SUITE_OSEN:
			eAuthMode = AUTH_MODE_WPA_OSEN;
			break;
		default:
			break;
		}
	} else if (version == IW_AUTH_WPA_VERSION_WPA ||
		   version == IW_AUTH_WPA_VERSION_WPA2) {
		switch (akm) {
		case WPA_AKM_SUITE_802_1X:
			eAuthMode = AUTH_MODE_WPA;
			break;
		case WPA_AKM_SUITE_PSK:
			eAuthMode = AUTH_MODE_WPA_PSK;
			break;
		case RSN_AKM_SUITE_802_1X:
			eAuthMode = AUTH_MODE_WPA2;
			break;
		case RSN_AKM_SUITE_PSK:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			break;
#if CFG_SUPPORT_802_11R
		case RSN_AKM_SUITE_FT_802_1X:
		case RSN_AKM_SUITE_FT_802_1X_SHA384_UNRESTRICTED:
			eAuthMode = AUTH_MODE_WPA2_FT;
			break;
		case RSN_AKM_SUITE_FT_PSK:
			eAuthMode = AUTH_MODE_WPA2_FT_PSK;
			break;
		case RSN_AKM_SUITE_FT_OVER_SAE:
		case RSN_AKM_SUITE_FT_SAE_EXT_KEY:
			eAuthMode = AUTH_MODE_WPA3_SAE;
			break;
#endif
#if CFG_SUPPORT_802_11W
		case RSN_AKM_SUITE_802_1X_SHA256:
			eAuthMode = AUTH_MODE_WPA2;
			break;
		case RSN_AKM_SUITE_PSK_SHA256:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			break;
#endif
#if (CFG_SUPPORT_FILS_SK_OFFLOAD == 1)
		case RSN_AKM_SUITE_FILS_SHA256:
		case RSN_AKM_SUITE_FILS_SHA384:
			eAuthMode = AUTH_MODE_FILS;
			break;
#endif /* CFG_SUPPORT_FILS_SK_OFFLOAD */
#if CFG_SUPPORT_PASSPOINT
		case RSN_AKM_SUITE_OSEN:
			eAuthMode = AUTH_MODE_WPA_OSEN;
			break;
#endif
		case RSN_AKM_SUITE_SAE:
		case RSN_AKM_SUITE_SAE_EXT_KEY:
			eAuthMode = AUTH_MODE_WPA3_SAE;
			break;
		case RSN_AKM_SUITE_OWE:
			eAuthMode = AUTH_MODE_WPA3_OWE;
			break;
#if CFG_SUPPORT_DPP
		case RSN_AKM_SUITE_DPP:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			break;
#endif
		case RSN_AKM_SUITE_8021X_SUITE_B_192:
			eAuthMode = AUTH_MODE_WPA2;
			break;
		default:
			DBGLOG(REQ, WARN, "invalid Akm Suite (0x%x)\n", akm);
			break;
		}
	} else {
		DBGLOG(REQ, WARN, "invalid wpa version (0x%x)\n", version);
	}

	return eAuthMode;
}

uint32_t rsnCipherToBit(uint32_t cipher)
{
#define WPACIPHER2BIT(x) \
do { \
	case (WPA_CIPHER_SUITE_##x): return (WPA_CIPHER_SUITE_##x##_BIT); \
} while (FALSE)

#define RSNCIPHER2BIT(x) \
do { \
	case (RSN_CIPHER_SUITE_##x): return (RSN_CIPHER_SUITE_##x##_BIT); \
} while (FALSE)

	/* sync with dot11RSNAConfigPairwiseCiphersTable */
	switch (cipher) {
	WPACIPHER2BIT(WEP40);
	WPACIPHER2BIT(TKIP);
	WPACIPHER2BIT(CCMP);
	WPACIPHER2BIT(WEP104);
	RSNCIPHER2BIT(WEP40);
	RSNCIPHER2BIT(TKIP);
	RSNCIPHER2BIT(CCMP);
	RSNCIPHER2BIT(WEP104);
	RSNCIPHER2BIT(GROUP_NOT_USED);
	RSNCIPHER2BIT(GCMP_256);
	RSNCIPHER2BIT(GCMP);
	default:
		DBGLOG(REQ, WARN, "invalid Cipher (0x%x)\n", cipher);
		return 0;
	}
}

uint32_t rsnKeyMgmtToBit(uint32_t akm)
{
#define WPAAKM2BIT(x) \
do { \
	case (WPA_AKM_SUITE_##x): return (WPA_AKM_SUITE_##x##_BIT); \
} while (FALSE)
#define RSNAKM2BIT(x) \
do { \
	case (RSN_AKM_SUITE_##x): return (RSN_AKM_SUITE_##x##_BIT); \
} while (FALSE)

	/* sync with dot11RSNAConfigAuthenticationSuitesTable */
	switch (akm) {
	WPAAKM2BIT(NONE);
	WPAAKM2BIT(802_1X);
	WPAAKM2BIT(PSK);
	RSNAKM2BIT(NONE);
	RSNAKM2BIT(802_1X);
	RSNAKM2BIT(PSK);
	RSNAKM2BIT(FT_802_1X);
	RSNAKM2BIT(FT_PSK);
	RSNAKM2BIT(OSEN);
	RSNAKM2BIT(SAE);
	RSNAKM2BIT(OWE);
	RSNAKM2BIT(DPP);
	RSNAKM2BIT(8021X_SUITE_B_192);
	RSNAKM2BIT(SAE_EXT_KEY);
	RSNAKM2BIT(802_1X_SHA256);
	RSNAKM2BIT(PSK_SHA256);
	RSNAKM2BIT(FT_OVER_SAE);
	RSNAKM2BIT(FT_SAE_EXT_KEY);
	default:
		DBGLOG(REQ, WARN, "invalid Akm Suite (0x%x)\n", akm);
		return 0;
	}
}

uint8_t rsnApOverload(uint16_t status, uint16_t reason)
{
	switch (status) {
	case STATUS_CODE_UNSPECIFIED_FAILURE:
	case STATUS_CODE_ASSOC_DENIED_AP_OVERLOAD:
	case STATUS_CODE_ASSOC_DENIED_BANDWIDTH:
	case STATUS_CODE_ASSOC_DENIED_OUTSIDE_STANDARD:
	case STATUS_CODE_AUTH_TIMEOUT:
	case STATUS_CODE_ASSOC_TIMEOUT:
		return TRUE;
	}
	switch (reason) {
	case REASON_CODE_DISASSOC_LACK_OF_BANDWIDTH:
	case REASON_CODE_DISASSOC_AP_OVERLOAD:
		return TRUE;
	}
	return FALSE;
}

uint8_t rsnApInvalidPMK(uint16_t status, uint16_t reason,
	enum ENUM_PARAM_AUTH_MODE AuthMode)
{
	switch (status) {
	case STATUS_INVALID_PMKID:
	case STATUS_CODE_INVALID_INFO_ELEMENT:
	case STATUS_CODE_R0KH_UNREACHABLE:
	case STATUS_CODE_AUTH_TIMEOUT:
	case STATUS_CODE_ASSOC_PREV_AUTH_INVALID:
		return TRUE;
	/* For IoT AP, don't use PMKID */
	case STATUS_CODE_ASSOC_DENIED_AP_OVERLOAD:
		if (AuthMode == AUTH_MODE_WPA3_OWE ||
		    AuthMode == AUTH_MODE_WPA3_SAE)
			return TRUE;
		break;
	}

	switch (reason) {
	case REASON_CODE_PREV_AUTH_INVALID:
	case REASON_CODE_INVALID_PMKID:
		return TRUE;
	}
	return FALSE;
}

uint8_t rsnIsFilsAuthAlg(uint8_t alg)
{
	return alg == AUTH_ALGORITHM_NUM_FILS_SK ||
	       alg == AUTH_ALGORITHM_NUM_FILS_SK_PFS ||
	       alg == AUTH_ALGORITHM_NUM_FILS_PK;
}

uint8_t rsnIsKeyMgmtSha256(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_PSK_SHA256 ||
	       akm == RSN_AKM_SUITE_802_1X_SHA256 ||
	       akm == RSN_AKM_SUITE_SAE ||
	       akm == RSN_AKM_SUITE_FT_OVER_SAE ||
	       akm == RSN_AKM_SUITE_OSEN ||
	       akm == RSN_AKM_SUITE_8021X_SUITE_B ||
	       akm == RSN_AKM_SUITE_FILS_SHA256 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA256;
}

uint8_t rsnIsKeyMgmtSha384(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_8021X_SUITE_B_192 ||
	       akm == RSN_AKM_SUITE_FT_802_1X_SHA384 ||
	       akm == RSN_AKM_SUITE_FILS_SHA384 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA384;
}

uint8_t rsnIsKeyMgmtIeee8021x(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_802_1X ||
	       akm == RSN_AKM_SUITE_FT_802_1X ||
	       akm == RSN_AKM_SUITE_FT_802_1X_SHA384 ||
	       akm == RSN_AKM_SUITE_FT_802_1X_SHA384_UNRESTRICTED ||
	       akm == RSN_AKM_SUITE_OSEN ||
	       akm == RSN_AKM_SUITE_802_1X_SHA256 ||
	       akm == RSN_AKM_SUITE_8021X_SHA384 ||
	       akm == RSN_AKM_SUITE_8021X_SUITE_B ||
	       akm == RSN_AKM_SUITE_8021X_SUITE_B_192 ||
	       akm == RSN_AKM_SUITE_FILS_SHA256 ||
	       akm == RSN_AKM_SUITE_FILS_SHA384 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA256 ||
	       akm == RSN_AKM_SUITE_FT_FILS_SHA384;
}

uint8_t rsnIsKeyMgmtEhtIeee8021x(uint32_t akm)
{
	return akm == RSN_AKM_SUITE_FT_802_1X ||
	       akm == RSN_AKM_SUITE_802_1X_SHA256;
}

uint8_t rsnKekLen(uint32_t akmp, uint16_t pmk_len)
{
	switch (akmp) {
	case RSN_AKM_SUITE_FILS_SHA384:
	case RSN_AKM_SUITE_FT_FILS_SHA384:
		return 64;
	case RSN_AKM_SUITE_8021X_SUITE_B_192:
	case RSN_AKM_SUITE_FILS_SHA256:
	case RSN_AKM_SUITE_FT_FILS_SHA256:
	case RSN_AKM_SUITE_FT_802_1X_SHA384:
		return 32;
	case RSN_AKM_SUITE_DPP:
		return pmk_len <= 32 ? 16 : 32;
	case RSN_AKM_SUITE_OWE:
		return pmk_len <= 32 ? 16 : 32;
	case RSN_AKM_SUITE_SAE_EXT_KEY:
	case RSN_AKM_SUITE_FT_SAE_EXT_KEY:
		return pmk_len <= 32 ? 16 : 32;
	default:
		return 16;
	}
}

uint8_t rsnCipherKeyLen(uint32_t cipher)
{
	switch (cipher) {
	case RSN_CIPHER_SUITE_GCMP_256:
	case RSN_CIPHER_SUITE_CCMP_256:
	case RSN_CIPHER_SUITE_BIP_GMAC_256:
	case RSN_CIPHER_SUITE_BIP_CMAC_256:
		return 32;
	case RSN_CIPHER_SUITE_CCMP:
	case RSN_CIPHER_SUITE_GCMP:
	case RSN_CIPHER_SUITE_AES_128_CMAC:
	case RSN_CIPHER_SUITE_BIP_GMAC_128:
		return 16;
	case RSN_CIPHER_SUITE_TKIP:
		return 32;
	}

	return 0;
}

u_int8_t rsnParseRsnxIE(struct ADAPTER *prAdapter,
		uint8_t *pucInfoElem, struct RSNX_INFO *prRsnxeInfo)
{
	uint8_t *cp;
	uint16_t u2Cap = 0, u2Left;

	if (IE_ID(pucInfoElem) == ELEM_ID_VENDOR &&
	    IE_LEN(pucInfoElem) >= 4 + 1) {
		cp = pucInfoElem + 2 + 4;
		u2Left = IE_LEN(pucInfoElem) - 4;
	} else if (IE_ID(pucInfoElem) == ELEM_ID_RSNX &&
		   IE_LEN(pucInfoElem) >= 1) {
		cp = pucInfoElem + 2;
		u2Left = IE_LEN(pucInfoElem);
	} else {
		DBGLOG(RSN, WARN, "IEID=%d IELEN=%d too short\n",
		       IE_ID(pucInfoElem), IE_LEN(pucInfoElem));
		return FALSE;
	}

	if (u2Left >= 1)
		u2Cap = *cp;
	if (u2Left >= 2)
		u2Cap = WLAN_GET_LE16(cp);

	prRsnxeInfo->u2Cap = u2Cap;

	DBGLOG(RSN, LOUD, "parse RSNXE cap: 0x%x\n",
		prRsnxeInfo->u2Cap);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to parse RSN IE.
 *
 * \param[in]  prInfoElem Pointer to the RSN IE
 * \param[out] prRsnInfo Pointer to the BSSDescription structure to store the
 *                  RSN information from the given RSN IE
 *
 * \retval TRUE - Succeeded
 * \retval FALSE - Failed
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnParseRsnIE(struct ADAPTER *prAdapter,
		       uint8_t *pucInfoElem,
		       struct RSN_INFO *prRsnInfo)
{
	uint8_t i, j;
	uint16_t u2RemainRsnIeLen;
	uint16_t u2Version = VERSION_RSN;
	uint16_t ucOuiType = 0;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = RSN_CIPHER_SUITE_CCMP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint16_t u2PmkidCount = 0;
	uint32_t u4GroupMgmtSuite = RSN_CIPHER_SUITE_BIP_CMAC_128;
	uint32_t u4PairwiseCipher = RSN_CIPHER_SUITE_CCMP;
	uint32_t u4AuthSuite = RSN_AKM_SUITE_802_1X;
	uint8_t *cp;

	/* Verify the length/version of the RSN IE. */
	if (IE_ID(pucInfoElem) == ELEM_ID_VENDOR &&
	    IE_LEN(pucInfoElem) >= 4 + 2 &&
	    (WLAN_GET_BE32(&pucInfoElem[2]) == VENDOR_IE_TYPE_RSNO ||
	     WLAN_GET_BE32(&pucInfoElem[2]) == VENDOR_IE_TYPE_RSNO2) &&
	     WLAN_GET_LE16(&pucInfoElem[2 + 4]) == u2Version) {
		cp = pucInfoElem + 2 + 4 + 2;
		u2RemainRsnIeLen = IE_LEN(pucInfoElem) - 4 - 2;

		if (WLAN_GET_BE32(&pucInfoElem[2]) == VENDOR_IE_TYPE_RSNO)
			ucOuiType = VENDOR_OUI_TYPE_RSNO;
		else
			ucOuiType = VENDOR_OUI_TYPE_RSNO2;
	} else if (IE_ID(pucInfoElem) == WLAN_EID_RSN &&
		   IE_LEN(pucInfoElem) >= 2 &&
		   WLAN_GET_LE16(&pucInfoElem[2]) == u2Version) {
		cp = pucInfoElem + 2 + 2;
		u2RemainRsnIeLen = IE_LEN(pucInfoElem) - 2;
	} else {
		DBGDUMP_MEM8(RSN, WARN, "Unsupported RSN IE\n",
			pucInfoElem, IE_SIZE(pucInfoElem));
		return FALSE;
	}

	do {
		if (u2RemainRsnIeLen == 0)
			break;

		/* Parse the Group Key Cipher Suite field. */
		if (u2RemainRsnIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in group cipher suite (IE len: %d)\n",
			       IE_LEN(pucInfoElem));
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u2RemainRsnIeLen -= 4;

		if (u2RemainRsnIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u2RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite count (IE len: %d)\n",
			       IE_LEN(pucInfoElem));
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u2RemainRsnIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		if (u2PairSuiteCount == 0 || u2PairSuiteCount * 4 > 255 ||
		    u2RemainRsnIeLen < u2PairSuiteCount * 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %d, count=%d)\n",
			       IE_LEN(pucInfoElem), u2PairSuiteCount);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += u2PairSuiteCount * 4;
		u2RemainRsnIeLen -= u2PairSuiteCount * 4;

		if (u2RemainRsnIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher
		 * Suite Count field.
		 */
		if (u2RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %d)\n",
			       IE_LEN(pucInfoElem));
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u2RemainRsnIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher
		 * Suite List field.
		 */
		if (u2AuthSuiteCount == 0 || u2AuthSuiteCount * 4 > 255 ||
		    u2RemainRsnIeLen < u2AuthSuiteCount * 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %d, count=%d)\n",
			       IE_LEN(pucInfoElem), u2AuthSuiteCount);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += u2AuthSuiteCount * 4;
		u2RemainRsnIeLen -= u2AuthSuiteCount * 4;

		if (u2RemainRsnIeLen == 0)
			break;

		/* Parse the RSN u2Capabilities field. */
		if (u2RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in RSN capabilities (IE len: %d)\n",
			       IE_LEN(pucInfoElem));
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
		cp += 2;
		u2RemainRsnIeLen -= 2;

		if (u2RemainRsnIeLen == 0)
			break;

		if (u2RemainRsnIeLen < 2) {
			DBGLOG(RSN, TRACE,
				"Fail to parse PMKID count in RSN iE\n");
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PmkidCount);
		cp += 2;
		u2RemainRsnIeLen -= 2;

		if (u2PmkidCount > 4) {
			DBGLOG(RSN, TRACE,
				"Bad RSN IE due to PMKID count(%d)\n",
				u2PmkidCount);
			return FALSE;
		}

		if (u2PmkidCount > 0 && u2RemainRsnIeLen < 16 * u2PmkidCount) {
			DBGLOG(RSN, TRACE,
				"Fail to parse PMKID in RSN iE, count: %d\n",
				u2PmkidCount);
			return FALSE;
		}

		if (u2PmkidCount > 0) {
			kalMemCopy(prRsnInfo->aucPmkid, cp, IW_PMKID_LEN);
			cp += IW_PMKID_LEN;
			u2RemainRsnIeLen -= IW_PMKID_LEN;
		}

		if (u2RemainRsnIeLen == 0)
			break;

		/* Parse the Group Management Cipher Suite field */
		if (u2RemainRsnIeLen < 4) {
			DBGLOG(RSN, TRACE,
				"Fail to parse group mgmt cipher suite in RSN IE\n");
			break;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupMgmtSuite);
		cp += 4;
		u2RemainRsnIeLen -= 4;

		if (u2RemainRsnIeLen == 0)
			break;

	} while (FALSE);

	/* Save the RSN information for the BSS. */
	prRsnInfo->ucElemId = IE_ID(pucInfoElem);

	prRsnInfo->u2Version = u2Version;

	prRsnInfo->ucOuiType = ucOuiType;

	prRsnInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, LOUD,
	       "RSN: version %d, group key cipher suite 0x%x\n",
	       u2Version, SWAP32(u4GroupSuite));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		for (i = 0, j = 0; i < u2PairSuiteCount &&
			j < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++) {

			WLAN_GET_FIELD_32(pucPairSuite,
					  &u4PairwiseCipher);
			pucPairSuite += 4;

			if (rsnCipherToBit(u4PairwiseCipher) == 0)
				continue;

			prRsnInfo->au4PairwiseKeyCipherSuite[j++] =
				u4PairwiseCipher;

			DBGLOG(RSN, LOUD,
			   "RSN: pairwise key cipher suite [%d]: 0x%x\n",
			   j - 1, SWAP32(u4PairwiseCipher));
		}

		prRsnInfo->u4PairwiseKeyCipherSuiteCount = j;
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present. Use the default chipher suite for RSN: CCMP.
		 */
		prRsnInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prRsnInfo->au4PairwiseKeyCipherSuite[0] = RSN_CIPHER_SUITE_CCMP;

		DBGLOG(RSN, LOUD,
			"RSN: pairwise key cipher suite: 0x%x (default)\n",
			SWAP32(prRsnInfo->au4PairwiseKeyCipherSuite[0]));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and
		 * key management suites is present.
		 */
		for (i = 0, j = 0; i < u2AuthSuiteCount &&
			j < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {

			WLAN_GET_FIELD_32(pucAuthSuite,
					  &u4AuthSuite);
			pucAuthSuite += 4;

			if (rsnKeyMgmtToBit(u4AuthSuite) == 0)
				continue;

			prRsnInfo->au4AuthKeyMgtSuite[j++] =
				u4AuthSuite;

			DBGLOG(RSN, LOUD,
			   "RSN: AKM suite [%d]:: 0x%x\n",
			   j - 1, SWAP32(u4AuthSuite));
		}

		prRsnInfo->u4AuthKeyMgtSuiteCount = j;
	} else {
		/* The information about the authentication and
		 * key management suites is not present.
		 * Use the default AKM suite for RSN.
		 */
		prRsnInfo->u4AuthKeyMgtSuiteCount = 1;
		prRsnInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD, "RSN: AKM suite: 0x%x (default)\n",
			SWAP32(prRsnInfo->au4AuthKeyMgtSuite[0]));
	}

	if (prRsnInfo->u4AuthKeyMgtSuiteCount == 0) {
		DBGLOG(RSN, WARN,
			"Fail to parse AKM in RSN IE\n");
	}

	if (prRsnInfo->u4PairwiseKeyCipherSuiteCount == 0) {
		DBGLOG(RSN, WARN,
			"Fail to parse cipher in RSN IE\n");
	}

	prRsnInfo->u2RsnCap = u2Cap;
	prRsnInfo->fgRsnCapPresent = TRUE;
	prRsnInfo->u2PmkidCount = u2PmkidCount;
	DBGLOG(RSN, LOUD, "RSN cap: 0x%04x, PMKID count: %d\n",
		prRsnInfo->u2RsnCap, prRsnInfo->u2PmkidCount);

	prRsnInfo->u4GroupMgmtCipherSuite = u4GroupMgmtSuite;
	DBGLOG(RSN, LOUD,
		"RSN: group mgmt cipher suite 0x%x\n",
		SWAP32(u4GroupMgmtSuite));

	return TRUE;
}				/* rsnParseRsnIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to parse WPA IE.
 *
 * \param[in]  prInfoElem Pointer to the WPA IE.
 * \param[out] prWpaInfo Pointer to the BSSDescription structure to store the
 *                       WPA information from the given WPA IE.
 *
 * \retval TRUE Succeeded.
 * \retval FALSE Failed.
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnParseWpaIE(struct ADAPTER *prAdapter,
		       struct WPA_INFO_ELEM *prInfoElem,
		       struct RSN_INFO *prWpaInfo)
{
	uint32_t i;
	int32_t u4RemainWpaIeLen;
	uint16_t u2Version;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = WPA_CIPHER_SUITE_TKIP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;
	u_int8_t fgCapPresent = FALSE;

	/* Verify the length of the WPA IE. */
	if (prInfoElem->ucLength < 6) {
		DBGLOG(RSN, TRACE, "WPA IE length too short (length=%d)\n",
		       prInfoElem->ucLength);
		return FALSE;
	}

	/* Check WPA version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != VERSION_WPA) {
		DBGLOG(RSN, TRACE, "Unsupported WPA IE version: %d\n",
		       u2Version);
		return FALSE;
	}

	cp = (uint8_t *) &prInfoElem->u4GroupKeyCipherSuite;
	u4RemainWpaIeLen = (int32_t) prInfoElem->ucLength - 6;

	do {
		if (u4RemainWpaIeLen == 0)
			break;

		/* WPA_OUI      : 4
		 *  Version      : 2
		 *  GroupSuite   : 4
		 *  PairwiseCount: 2
		 *  PairwiseSuite: 4 * pairSuiteCount
		 *  AuthCount    : 2
		 *  AuthSuite    : 4 * authSuiteCount
		 *  Cap          : 2
		 */

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainWpaIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainWpaIeLen -= 4;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainWpaIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (int32_t) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite
		 * Count field.
		 */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite
		 * List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainWpaIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (int32_t) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the WPA u2Capabilities field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in WPA capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		fgCapPresent = TRUE;
		WLAN_GET_FIELD_16(cp, &u2Cap);
		u4RemainWpaIeLen -= 2;
	} while (FALSE);

	/* Save the WPA information for the BSS. */

	prWpaInfo->ucElemId = ELEM_ID_WPA;

	prWpaInfo->u2Version = u2Version;

	prWpaInfo->ucOuiType = VENDOR_OUI_TYPE_WPA;

	prWpaInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, LOUD, "WPA: version %d, group key cipher suite 0x%x\n",
		u2Version, SWAP32(u4GroupSuite));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prWpaInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
					  &prWpaInfo->au4PairwiseKeyCipherSuite
					  [i]);
			pucPairSuite += 4;

			DBGLOG(RSN, LOUD,
			   "WPA: pairwise key cipher suite [%d]: 0x%x\n", i,
			   SWAP32(prWpaInfo->au4PairwiseKeyCipherSuite[i]));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present.
		 * Use the default chipher suite for WPA: TKIP.
		 */
		prWpaInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prWpaInfo->au4PairwiseKeyCipherSuite[0] = WPA_CIPHER_SUITE_TKIP;

		DBGLOG(RSN, LOUD,
			"WPA: pairwise key cipher suite: 0x%x (default)\n",
			SWAP32(prWpaInfo->au4PairwiseKeyCipherSuite[0]));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and
		 * key management suites is present.
		 */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prWpaInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prWpaInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, LOUD,
			       "WPA: AKM suite [%d]: 0x%x\n", i,
			       SWAP32(prWpaInfo->au4AuthKeyMgtSuite[i]));
		}
	} else {
		/* The information about the authentication
		 * and key management suites is not present.
		 * Use the default AKM suite for WPA.
		 */
		prWpaInfo->u4AuthKeyMgtSuiteCount = 1;
		prWpaInfo->au4AuthKeyMgtSuite[0] = WPA_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD,
		       "WPA: AKM suite: 0x%x (default)\n",
		       SWAP32(prWpaInfo->au4AuthKeyMgtSuite[0]));
	}

	if (fgCapPresent) {
		prWpaInfo->fgRsnCapPresent = TRUE;
		prWpaInfo->u2RsnCap = u2Cap;
		DBGLOG(RSN, LOUD, "WPA: RSN cap: 0x%04x\n",
		       prWpaInfo->u2RsnCap);
	} else {
		prWpaInfo->fgRsnCapPresent = FALSE;
		prWpaInfo->u2RsnCap = 0;
	}

	return TRUE;
}				/* rsnParseWpaIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to search the desired pairwise
 *        cipher suite from the MIB Pairwise Cipher Suite
 *        configuration table.
 *
 * \param[in] u4Cipher The desired pairwise cipher suite to be searched
 * \param[out] pu4Index Pointer to the index of the desired pairwise cipher in
 *                      the table
 *
 * \retval TRUE - The desired pairwise cipher suite is found in the table.
 * \retval FALSE - The desired pairwise cipher suite is not found in the
 *                 table.
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnSearchSupportedCipher(struct ADAPTER *prAdapter,
				  uint32_t u4Cipher, uint8_t ucBssIndex)
{
	struct IEEE_802_11_MIB *prMib;

	prMib = aisGetMib(prAdapter, ucBssIndex);

	return !!(rsnCipherToBit(u4Cipher) &
		prMib->dot11RSNAConfigPairwiseCipher);
} /* rsnSearchSupportedCipher */

void rsnDumpSupportedCipher(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	uint8_t i;
	struct DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY *prEntry;
	struct IEEE_802_11_MIB *prMib;

	prMib = aisGetMib(prAdapter, ucBssIndex);

	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++) {
		prEntry = &prMib->dot11RSNAConfigPairwiseCiphersTable[i];
		if (prEntry->dot11RSNAConfigPairwiseCipherEnabled)
			DBGLOG(RSN, WARN, "Support cipher=0x%x\n",
				SWAP32(prEntry->dot11RSNAConfigPairwiseCipher));
#if 0
		else
			DBGLOG(RSN, WARN, "Unsupport cipher=0x%x\n",
				SWAP32(prEntry->dot11RSNAConfigPairwiseCipher));
#endif
	}

	DBGLOG(RSN, WARN, "Support cipher bmap=0x%x\n",
		prMib->dot11RSNAConfigPairwiseCipher);
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief Whether BSS RSN is matched from upper layer set.
 *
 * \param[in] prAdapter Pointer to the Adapter structure, BSS RSN Information
 *
 * \retval BOOLEAN
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnIsSuitableBSS(struct ADAPTER *prAdapter,
			  struct BSS_DESC *prBss,
			  struct RSN_INFO *prBssRsnInfo,
			  uint8_t ucBssIndex)
{
	uint32_t i, c, k;

	/* check group */
	k = prBssRsnInfo->u4GroupKeyCipherSuite;
	if (!rsnSearchSupportedCipher(prAdapter, k, ucBssIndex)) {
		DBGLOG(RSN, WARN, "Break by GroupKey=0x%x\n", SWAP32(k));
		rsnDumpSupportedCipher(prAdapter, ucBssIndex);
		return FALSE;
	}

	/* check pairwise */
	c = prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
	if (c == 0) {
		DBGLOG(RSN, WARN, "No PTK found\n");
		return FALSE;
	}

	for (i = 0; i < c; i++) {
		k = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
		if (rsnSearchSupportedCipher(prAdapter, k, ucBssIndex)) {
			break;
		} else if (i == c - 1) {
			DBGLOG(RSN, WARN, "Break by PairwisKey=0x%x\n",
				SWAP32(k));
			rsnDumpSupportedCipher(prAdapter, ucBssIndex);
			return FALSE;
		}
	}

	/* skip akm check for adative 11r */
	if (prBss->ucIsAdaptive11r && rsnSearchFTSuite(prAdapter, ucBssIndex))
		return TRUE;

	/* check akm */
	c = prBssRsnInfo->u4AuthKeyMgtSuiteCount;
	if (c == 0) {
		DBGLOG(RSN, WARN, "No AuthKey found\n");
		return FALSE;
	}

	for (i = 0; i < c; i++) {
		k = prBssRsnInfo->au4AuthKeyMgtSuite[i];
		if (rsnSearchAKMSuite(prAdapter, k, ucBssIndex)) {
			break;
		} else if (i == c - 1) {
			DBGLOG(RSN, WARN, "Break by AuthKey=0x%x\n",
				SWAP32(k));
			rsnDumpSupportedAKMSuite(prAdapter, ucBssIndex);
			return FALSE;
		}
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to search the desired
 *        authentication and key management (AKM) suite from the
 *        MIB Authentication and Key Management Suites table.
 *
 * \param[in]  u4AkmSuite The desired AKM suite to be searched
 * \param[out] pu4Index   Pointer to the index of the desired AKM suite in the
 *                        table
 *
 * \retval TRUE  The desired AKM suite is found in the table.
 * \retval FALSE The desired AKM suite is not found in the table.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnSearchAKMSuite(struct ADAPTER *prAdapter,
			   uint32_t u4AkmSuite, uint8_t ucBssIndex)
{
	struct IEEE_802_11_MIB *prMib;

	prMib = aisGetMib(prAdapter, ucBssIndex);

	return !!(rsnKeyMgmtToBit(u4AkmSuite) & prMib->dot11RSNAConfigAkm);
} /* rsnSearchAKMSuite */

void rsnDumpSupportedAKMSuite(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	uint8_t i;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY *prEntry;
	struct IEEE_802_11_MIB *prMib;

	prMib = aisGetMib(prAdapter, ucBssIndex);

	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prMib->dot11RSNAConfigAuthenticationSuitesTable[i];
		if (prEntry->dot11RSNAConfigAuthenticationSuiteEnabled)
			DBGLOG(RSN, WARN, "Support akm=0x%x\n",
			   SWAP32(prEntry->dot11RSNAConfigAuthenticationSuite));
#if 0
		else
			DBGLOG(RSN, WARN, "Unsupport akm=0x%x\n",
			   SWAP32(prEntry->dot11RSNAConfigAuthenticationSuite));
#endif
	}

	DBGLOG(RSN, WARN, "Support akm bmap=0x%x\n", prMib->dot11RSNAConfigAkm);
}

uint8_t rsnSearchFTSuite(struct ADAPTER *ad, uint8_t bssidx)
{
	return rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_PSK, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_PSK_SHA384, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_802_1X, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_OVER_SAE, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_802_1X_SHA384, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_802_1X_SHA384_UNRESTRICTED,
								bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_SAE_EXT_KEY, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_FILS_SHA256, bssidx) ||
	    rsnSearchAKMSuite(ad, RSN_AKM_SUITE_FT_FILS_SHA384, bssidx);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief refer to wpa_supplicant wpa_key_mgmt_wpa
 */

uint8_t rsnAuthModeRsn(enum ENUM_PARAM_AUTH_MODE eAuthMode)
{
	return eAuthMode == AUTH_MODE_WPA2 ||
	       eAuthMode == AUTH_MODE_WPA2_PSK ||
	       eAuthMode == AUTH_MODE_WPA2_FT_PSK ||
	       eAuthMode == AUTH_MODE_WPA2_FT ||
	       eAuthMode == AUTH_MODE_WPA3_SAE ||
	       eAuthMode == AUTH_MODE_WPA3_OWE ||
	       eAuthMode == AUTH_MODE_FILS;
}

uint8_t rsnIsKeyMgmtForWpa3(struct ADAPTER *ad,
	uint32_t u4AkmSuite,
	uint8_t bssidx,
	struct BSS_DESC *prBss,
	uint8_t fgCheckH2E)
{
	struct GL_WPA_INFO *prWpaInfo;
	u_int8_t fgIsOWE, fgIsSAE, fgIsSAEH2E;

	prWpaInfo = aisGetWpaInfo(ad, bssidx);
	fgIsOWE = u4AkmSuite == RSN_AKM_SUITE_OWE;
	fgIsSAE = rsnKeyMgmtSae(u4AkmSuite);

	fgIsSAEH2E = fgIsSAE &&
		(prWpaInfo->u2RSNXCap & BIT(WLAN_RSNX_CAPAB_SAE_H2E)) &&
		((prBss->fgIERSNX &&
			prBss->u2RsnxCap & BIT(WLAN_RSNX_CAPAB_SAE_H2E))
#if (CFG_SUPPORT_RSNO == 1)
		 || (prBss->fgIERSNXO &&
			prBss->u2RsnxoCap & BIT(WLAN_RSNX_CAPAB_SAE_H2E))
#endif /* CFG_SUPPORT_RSNO */
		);

	if (fgCheckH2E)
		return (fgIsOWE || fgIsSAEH2E);
	else
		return (fgIsOWE || fgIsSAE);
}

uint8_t rsnIsKeyMgmtFor6g(struct ADAPTER *ad,
	uint32_t u4AkmSuite,
	uint8_t bssidx,
	struct BSS_DESC *prBss)
{
	if (rsnIsKeyMgmtIeee8021x(u4AkmSuite)) {
		DBGLOG(RSN, INFO,
			"AKM %d is allowed for 6G enterprise AP\n",
			u4AkmSuite);
		return TRUE;
	}

#if WLAN_INCLUDE_SYS
#if (CFG_SUPPORT_WIFI_6G == 1)
	if (ad->fgEnRfTestMode) {
		DBGLOG(RSN, INFO,
			"RF test mode for 6G: all security type is allowed\n");
		return TRUE;
	}

	if (ad->fgEn6eSafeMode) {
		DBGLOG(RSN, INFO,
			"safe mode for 6G: all security type is allowed\n");
		return TRUE;
	}
#endif
#endif

	if (rsnIsKeyMgmtForWpa3(ad, u4AkmSuite, bssidx, prBss, TRUE))
		return TRUE;

	return FALSE;
}

uint8_t rsnIsKeyMgmtForEht(struct ADAPTER *ad,
	struct BSS_DESC *prBss, uint8_t bssidx)
{
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	/* FC cert: only AKM3/5 is allowed for EHT WPA3 enterprise;
	 * only AKM24/25 is allowed for EHT WPA3 personal
	 */
	if (prWifiVar->fgEhtAkmCheck) {
		DBGLOG(RSN, TRACE,
			"EHT AKM check: limited akm are allowed, akm=0x%04x\n",
			prBss->u4RsnSelectedAKMSuite);
		if (rsnIsKeyMgmtEhtIeee8021x(prBss->u4RsnSelectedAKMSuite))
			return TRUE;

		if (rsnKeyMgmtEhtSae(prBss->u4RsnSelectedAKMSuite) &&
			rsnIsKeyMgmtForWpa3(ad, prBss->u4RsnSelectedAKMSuite,
#if (CFG_WIFI_EHT_H2E_CHK == 1)
				bssidx, prBss, TRUE))
#else
				bssidx, prBss, FALSE))
#endif
			return TRUE;
	} else {
	/* No strict EHT AKM check for IoT */
		if (rsnIsKeyMgmtIeee8021x(prBss->u4RsnSelectedAKMSuite))
			return TRUE;
		if (rsnIsKeyMgmtForWpa3(ad, prBss->u4RsnSelectedAKMSuite,
#if (CFG_WIFI_EHT_H2E_CHK == 1)
				bssidx, prBss, TRUE))
#else
				bssidx, prBss, FALSE))
#endif
			return TRUE;
	}

	return FALSE;
}

void rsnMatchCipherSuite(struct RSN_INFO *prBssRsnInfo,
	enum ENUM_WEP_STATUS eEncStatus, uint32_t *pu4PairwiseCipher,
	uint32_t *pu4GroupCipher)
{
	int32_t i;
	uint32_t u4PairwiseCipher = 0;
	uint32_t u4GroupCipher = 0;
	uint32_t u4Cipher;

	/* Select pairwise/group ciphers */
	switch (eEncStatus) {
	case ENUM_ENCRYPTION4_ENABLED: {
		u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			i++) {
			u4Cipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
			/* TODO: WTBL cipher filed cannot
			* 1-1 mapping to spec cipher suite number
			*/
			if (u4Cipher == RSN_CIPHER_SUITE_GCMP_256 ||
			    u4Cipher == RSN_CIPHER_SUITE_GCMP)
				u4PairwiseCipher = u4Cipher;
		}
		if (u4PairwiseCipher != 0)
			break;
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			i++) {
			u4Cipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
			if (GET_SELECTOR_TYPE(u4Cipher) == CIPHER_SUITE_CCMP)
				u4PairwiseCipher = u4Cipher;
		}
	}
		break;

	case ENUM_ENCRYPTION3_ENABLED: {
		u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			i++) {
			u4Cipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
			if (GET_SELECTOR_TYPE(u4Cipher) == CIPHER_SUITE_CCMP)
				u4PairwiseCipher = u4Cipher;

		}
		if (u4PairwiseCipher != 0)
			break;
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			i++) {
			u4Cipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
			if (GET_SELECTOR_TYPE(u4Cipher) == CIPHER_SUITE_TKIP)
				u4PairwiseCipher = u4Cipher;
		}
	}
		break;


	case ENUM_ENCRYPTION2_ENABLED: {
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			i++) {
			u4Cipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
			if (GET_SELECTOR_TYPE(u4Cipher) == CIPHER_SUITE_TKIP)
				u4PairwiseCipher = u4Cipher;
		}
		if (GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite) ==
		    CIPHER_SUITE_CCMP)
			DBGLOG(RSN, TRACE, "Cannot join CCMP BSS\n");
		else
			u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
	}
		break;

	case ENUM_ENCRYPTION1_ENABLED: {
		for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
			i++) {
			u4Cipher = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
			if (GET_SELECTOR_TYPE(u4Cipher) == CIPHER_SUITE_WEP40 ||
			    GET_SELECTOR_TYPE(u4Cipher) == CIPHER_SUITE_WEP104)
				u4PairwiseCipher = u4Cipher;
		}
		if (GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite) ==
		    CIPHER_SUITE_CCMP ||
		    GET_SELECTOR_TYPE(prBssRsnInfo->u4GroupKeyCipherSuite) ==
		    CIPHER_SUITE_TKIP)
			DBGLOG(RSN, TRACE,
				"Cannot join CCMP/TKIP BSS\n");
		else
			u4GroupCipher =	prBssRsnInfo->u4GroupKeyCipherSuite;
	}
		break;

	default:
		break;
	}


	*pu4PairwiseCipher = u4PairwiseCipher;
	*pu4GroupCipher = u4GroupCipher;
}

void rsnAllowCrossAkm(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	uint8_t i;
	struct IEEE_802_11_MIB *prMib;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY *prEntry;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	struct GL_WPA_INFO *prWpaInfo;

	eAuthMode = aisGetAuthMode(prAdapter, ucBssIndex);
	prMib = aisGetMib(prAdapter, ucBssIndex);
	prWpaInfo = aisGetWpaInfo(prAdapter, ucBssIndex);

	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.fgDisCrossPmf) &&
	    kalGetRsnIeMfpCap(prAdapter->prGlueInfo, ucBssIndex)
		== RSN_AUTH_MFP_REQUIRED) {
		prWpaInfo->ucRSNMfpCap = RSN_AUTH_MFP_OPTIONAL;
		DBGLOG(RSN, INFO,
			"Change RSN IE cap from MFPR to MFPC for cross akm roaming\n");
	}

#if (CFG_TC10_FEATURE == 1)
	if (eAuthMode == AUTH_MODE_WPA_PSK ||
	    eAuthMode == AUTH_MODE_WPA2_PSK) {
		/* support cross wpa/wpa2 psk */
		for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
			prEntry =
			    &prMib->dot11RSNAConfigAuthenticationSuitesTable[i];

			if (prEntry->dot11RSNAConfigAuthenticationSuite !=
				WPA_AKM_SUITE_PSK &&
			    prEntry->dot11RSNAConfigAuthenticationSuite !=
				RSN_AKM_SUITE_PSK)
				continue;

			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
				TRUE;

			prMib->dot11RSNAConfigAkm |=
				rsnKeyMgmtToBit(
				prEntry->dot11RSNAConfigAuthenticationSuite);
		}
	}
#endif

	if (eAuthMode == AUTH_MODE_WPA2_PSK ||
	    eAuthMode == AUTH_MODE_WPA3_SAE) {
		/* support cross wpa2/sae/sae-ext */
		for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
			prEntry =
			    &prMib->dot11RSNAConfigAuthenticationSuitesTable[i];

			if (prEntry->dot11RSNAConfigAuthenticationSuite !=
				RSN_AKM_SUITE_PSK &&
				prEntry->dot11RSNAConfigAuthenticationSuite !=
				RSN_AKM_SUITE_SAE &&
				prEntry->dot11RSNAConfigAuthenticationSuite !=
				RSN_AKM_SUITE_SAE_EXT_KEY)
				continue;

			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
				TRUE;

			prMib->dot11RSNAConfigAkm |=
				rsnKeyMgmtToBit(
				prEntry->dot11RSNAConfigAuthenticationSuite);
		}
	}
}

#if (CFG_SUPPORT_RSNO == 1)
u_int8_t rsnSupportRSNOverride(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo) && prAdapter->rWifiVar.fgEnStaRSNO)
		return TRUE;

	return FALSE;
}
#endif /* CFG_SUPPORT_RSNO */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to perform RSNA or TSN policy
 *        selection for a given BSS.
 *
 * \param[in]  prBss Pointer to the BSS description
 *
 * \retval TRUE - The RSNA/TSN policy selection for the given BSS is
 *                successful. The selected pairwise and group cipher suites
 *                are returned in the BSS description.
 * \retval FALSE - The RSNA/TSN policy selection for the given BSS is failed.
 *                 The driver shall not attempt to join the given BSS.
 *
 * \note The Encrypt status matched score will save to bss for final ap select.
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnPerformPolicySelection(
		struct ADAPTER *prAdapter, struct BSS_DESC *prBss,
		uint8_t ucBssIndex)
{
	int32_t i;
	u_int8_t fgSuiteSupported;
	uint32_t u4PairwiseCipher = 0;
	uint32_t u4GroupCipher = 0;
	uint32_t u4GroupMgmtCipher = RSN_CIPHER_SUITE_BIP_CMAC_128;
	uint32_t u4AkmSuite = 0;
	uint8_t u4MgmtProtection = 0;
	struct RSN_INFO *prBssRsnInfo = NULL;
	uint8_t fgTryRsn = TRUE, fgTryWpa = TRUE;
	uint8_t fgTryRsno2 = FALSE, fgTryRsno = FALSE;
	uint8_t fgIsMLO = FALSE, fgIsApRsnOveride = FALSE;
#if CFG_SUPPORT_WPS
	u_int8_t fgIsWpsActive = (u_int8_t) FALSE;
#endif
	enum ENUM_PARAM_AUTH_MODE eAuthMode, eNewAuthMode;
	enum ENUM_PARAM_OP_MODE eOPMode;
	enum ENUM_WEP_STATUS eEncStatus;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct GL_WPA_INFO *prWpaInfo;
	struct IEEE_802_11_MIB *prMib;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prWpaInfo = aisGetWpaInfo(prAdapter, ucBssIndex);

	prBss->u4RsnSelectedPairwiseCipher = 0;
	prBss->u4RsnSelectedGroupCipher = 0;
	prBss->u4RsnSelectedAKMSuite = 0;
	prBss->eRsnSelectedAuthMode = 0;
#if (CFG_SUPPORT_RSNO == 1)
	prBss->eRsnSelectedRSNOverride = RSN_OVERRIDE_NOT_USED;
#endif /* CFG_SUPPORT_RSNO */

	eAuthMode = aisGetAuthMode(prAdapter, ucBssIndex);
	eOPMode = aisGetOPMode(prAdapter, ucBssIndex);
	eEncStatus = aisGetEncStatus(prAdapter, ucBssIndex);
	prMib = aisGetMib(prAdapter, ucBssIndex);
	u4PairwiseCipher = prMib->dot11RSNAConfigPairwiseCipher;
	u4GroupCipher = prMib->dot11RSNAConfigGroupCipher;

#if (CFG_SUPPORT_RSNO == 1)
	if (rsnSupportRSNOverride(prAdapter, ucBssIndex)) {
		if (prBss->fgIERSNO) {
			fgIsApRsnOveride = TRUE;
			fgTryRsno = TRUE;
		}
		if (prBss->fgIERSNO2) {
			fgIsApRsnOveride = TRUE;
			fgTryRsno2 = TRUE;
		}

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		if (prBss->rMlInfo.fgValid) {
			fgIsMLO = TRUE;
		}

		/* MLO cases for RSN overriding are required to use RSNE
		 * Override 2 element and RSNXE Override element together.
		 */
		if (fgTryRsno2) {
			if (fgIsMLO && prBss->fgIERSNO2 && !prBss->fgIERSNXO) {
				DBGLOG(RSN, INFO, "BSS " MACSTR
				   " advertises RSNO2 without RSNXO - ignore RSNO2 for MLO",
				   MAC2STR(prBss->aucBSSID));
				fgTryRsno2 = FALSE;
			}
		}
#endif /* CFG_SUPPORT_802_11BE_MLO */
	}
#endif /* CFG_SUPPORT_RSNO */

	DBGLOG(RSN, TRACE, "BSS["MACSTR
		"] AuthMode [%d], EncStatus [%d], Flag[wpa=%d rsn=%d rsno=%d rsno2=%d aprsno=%d mlo=%d]",
		MAC2STR(prBss->aucBSSID), eAuthMode, eEncStatus,
		fgTryWpa, fgTryRsn, fgTryRsno, fgTryRsno2,
		fgIsApRsnOveride, fgIsMLO);

#if CFG_SUPPORT_WPS
	fgIsWpsActive = aisGetConnSettings(prAdapter, ucBssIndex)->fgWpsActive;
	/* CR1640, disable the AP select privacy check */
	if (fgIsWpsActive && eAuthMode < AUTH_MODE_WPA &&
	    eOPMode == NET_TYPE_INFRA) {
		DBGLOG(RSN, INFO, "-- Skip the Protected BSS check\n");
		goto selected;
	}
#endif

	/* Protection is not required in this BSS. */
	if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) == 0) {
		if (secEnabledInAis(prAdapter, ucBssIndex) == FALSE)
			goto selected;

		DBGLOG(RSN, INFO, "-- Protected BSS but No need\n");
		return FALSE;
	}

	/* Protection is required in this BSS. */
	if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) != 0) {
		if (secEnabledInAis(prAdapter,
			ucBssIndex) == FALSE) {
			DBGLOG(RSN, INFO, "-- Protected BSS\n");
			return FALSE;
		}
	}

#if CFG_SUPPORT_WAPI
	if (aisGetWapiMode(prAdapter, ucBssIndex)) {
		if (!wapiPerformPolicySelection(prAdapter, prBss, ucBssIndex)) {
			DBGLOG(RSN, WARN, MACSTR " wapi policy select fail.\n",
				MAC2STR(prBss->aucBSSID));
			return FALSE;
		}
	}
#endif

try_again:
	DBGLOG(RSN, LOUD,
		"try rsn, wpa=%d rsn=%d rsno=%d rsno2=%d\n",
			fgTryWpa, fgTryRsn, fgTryRsno, fgTryRsno2);

	if (eAuthMode == AUTH_MODE_WPA ||
	    eAuthMode == AUTH_MODE_WPA_PSK ||
	    eAuthMode == AUTH_MODE_WPA_NONE) {
		if (fgTryWpa && prBss->fgIEWPA) {
			prBssRsnInfo = &prBss->rWPAInfo;
			fgTryWpa = FALSE;
		} else if (eAuthMode == AUTH_MODE_WPA_PSK &&
			   fgTryRsn && prBss->fgIERSN) {
			prBssRsnInfo = &prBss->rRSNInfo;
			fgTryRsn = FALSE;
		} else {
			DBGLOG(RSN, INFO,
			       "WPA does not exist. try wpa=%d rsn=%d rsno=%d rsno2=%d\n",
			       fgTryWpa, fgTryRsn, fgTryRsno, fgTryRsno2);
			return FALSE;
		}
	} else if (rsnAuthModeRsn(eAuthMode)) {
		if (0) {
#if (CFG_SUPPORT_RSNO == 1)
		} else if (fgTryRsno2 && prBss->fgIERSNO2) {
			prBssRsnInfo = &prBss->rRSNO2Info;
			fgTryRsno2 = FALSE;
		} else if (fgTryRsno && prBss->fgIERSNO) {
			prBssRsnInfo = &prBss->rRSNOInfo;
			fgTryRsno = FALSE;
#endif /* CFG_SUPPORT_RSNO */
		} else if (fgTryRsn && prBss->fgIERSN) {
			prBssRsnInfo = &prBss->rRSNInfo;
			fgTryRsn = FALSE;
		} else if (eAuthMode == AUTH_MODE_WPA2_PSK &&
			   fgTryWpa && prBss->fgIEWPA) {
			prBssRsnInfo = &prBss->rWPAInfo;
			fgTryWpa = FALSE;
		} else {
			DBGLOG(RSN, INFO,
			       "RSNE does not exist. try wpa=%d rsn=%d rsno=%d rsno2=%d\n",
			       fgTryWpa, fgTryRsn, fgTryRsno, fgTryRsno2);
			return FALSE;
		}
#if CFG_SUPPORT_PASSPOINT
	} else if (eAuthMode == AUTH_MODE_WPA_OSEN) {
		if (prBss->fgIERSN) {
			if (fgTryRsn) {
				prBssRsnInfo = &prBss->rRSNInfo;
				fgTryRsn = FALSE;
			} else {
				DBGLOG(RSN, INFO,
				     "OSEN RSNE does not exist. try wpa=%d rsn=%d rsno=%d rsno2=%d\n",
				     fgTryWpa, fgTryRsn, fgTryRsno, fgTryRsno2);
				return FALSE;
			}
		} else {
			goto selected;
		}
#endif
	} else if (eEncStatus == ENUM_ENCRYPTION1_ENABLED ||
		   eEncStatus == ENUM_ENCRYPTION2_ENABLED) {
		/* If driver is configured to use WEP/TKIP, use this BSS.*/
		goto selected;
	} else {
		DBGLOG(RSN, INFO, "unknown\n");
		return FALSE;
	}

	if (!rsnIsSuitableBSS(prAdapter, prBss, prBssRsnInfo, ucBssIndex)) {
		DBGLOG(RSN, LOUD,
			"not suitable BSS, try again! wpa=%d rsn=%d rsno=%d rsno2=%d\n",
			fgTryWpa, fgTryRsn, fgTryRsno, fgTryRsno2);
		goto try_again;
	}

	if (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount == 1 &&
	    GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[0]) ==
	    CIPHER_SUITE_NONE) {
		/* Since the pairwise cipher use the same cipher suite
		 * as the group cipher in the BSS, we check the group cipher
		 * suite against the current encryption status.
		 */
		fgSuiteSupported = FALSE;

		switch (prBssRsnInfo->u4GroupKeyCipherSuite) {
		case RSN_CIPHER_SUITE_GCMP_256:
		case RSN_CIPHER_SUITE_GCMP:
			if (eEncStatus == ENUM_ENCRYPTION4_ENABLED)
				fgSuiteSupported = TRUE;
			break;
		case WPA_CIPHER_SUITE_CCMP:
		case RSN_CIPHER_SUITE_CCMP:
			if (eEncStatus == ENUM_ENCRYPTION4_ENABLED ||
			    eEncStatus == ENUM_ENCRYPTION3_ENABLED)
				fgSuiteSupported = TRUE;
			break;

		case WPA_CIPHER_SUITE_TKIP:
		case RSN_CIPHER_SUITE_TKIP:
			if (eEncStatus == ENUM_ENCRYPTION3_ENABLED ||
			    eEncStatus == ENUM_ENCRYPTION2_ENABLED)
				fgSuiteSupported = TRUE;
			break;

		case WPA_CIPHER_SUITE_WEP40:
		case WPA_CIPHER_SUITE_WEP104:
			if (eEncStatus == ENUM_ENCRYPTION3_ENABLED ||
			    eEncStatus == ENUM_ENCRYPTION2_ENABLED ||
			    eEncStatus == ENUM_ENCRYPTION1_ENABLED)
				fgSuiteSupported = TRUE;
			break;
		}

		if (fgSuiteSupported) {
			u4PairwiseCipher = WPA_CIPHER_SUITE_NONE;
			u4GroupCipher = prBssRsnInfo->u4GroupKeyCipherSuite;
		}
#if DBG
		else {
			DBGLOG(RSN, TRACE,
			       "Inproper encryption status %d for group-key-only BSS\n",
			       eEncStatus);
		}
#endif
	} else {
		fgSuiteSupported = FALSE;

		DBGLOG(RSN, TRACE,
		       "eEncStatus %d %d 0x%08x\n",
		       eEncStatus,
		       prBssRsnInfo->u4PairwiseKeyCipherSuiteCount,
		       prBssRsnInfo->au4PairwiseKeyCipherSuite[0]);

		rsnMatchCipherSuite(prBssRsnInfo, eEncStatus,
			&u4PairwiseCipher, &u4GroupCipher);
	}

	/* Exception handler */
	/* If we cannot find proper pairwise and group cipher suites
	 * to join the BSS, do not check the supported AKM suites.
	 */
	if (u4PairwiseCipher == 0 || u4GroupCipher == 0) {
		DBGLOG(RSN, INFO,
		       "Failed to select pairwise/group cipher (0x%08x/0x%08x)\n",
		       u4PairwiseCipher, u4GroupCipher);
		rsnDumpSupportedCipher(prAdapter, ucBssIndex);
		return FALSE;
	}
#if CFG_ENABLE_WIFI_DIRECT
	if ((prAdapter->fgIsP2PRegistered) &&
		IS_BSS_INDEX_P2P(prAdapter, ucBssIndex)) {
		if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
		    u4GroupCipher != RSN_CIPHER_SUITE_CCMP
		    || u4AkmSuite != RSN_AKM_SUITE_PSK) {
			DBGLOG(RSN, INFO,
			       "Failed to select pairwise/group cipher for P2P network (0x%08x/0x%08x)\n",
			       u4PairwiseCipher, u4GroupCipher);
			return FALSE;
		}
	}
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex) &&
	    GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex)->eNetworkType == NETWORK_TYPE_BOW) {
		if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
		    u4GroupCipher != RSN_CIPHER_SUITE_CCMP
		    || u4AkmSuite != RSN_AKM_SUITE_PSK) {
			DBGLOG(RSN, INFO,
			       "Failed to select pairwise/group cipher for BT over Wi-Fi network (0x%08x/0x%08x)\n",
			       u4PairwiseCipher, u4GroupCipher);
			return FALSE;
		}
	}
#endif

	/* Verify if selected pairwisse cipher is supported */
	fgSuiteSupported = rsnSearchSupportedCipher(prAdapter,
		u4PairwiseCipher, ucBssIndex);

	/* Verify if selected group cipher is supported */
	if (fgSuiteSupported)
		fgSuiteSupported = rsnSearchSupportedCipher(prAdapter,
			u4GroupCipher, ucBssIndex);

	if (!fgSuiteSupported) {
		DBGLOG(RSN, INFO,
		       "Failed to support selected pairwise/group cipher (0x%08x/0x%08x)\n",
		       u4PairwiseCipher, u4GroupCipher);
		return FALSE;
	}

	/* Select AKM */
	/* If the driver cannot support any authentication suites advertised in
	 *  the given BSS, we fail to perform RSNA policy selection.
	 */
	/* Attempt to find any overlapping supported AKM suite. */
	if (eAuthMode ==  AUTH_MODE_WPA2_FT_PSK &&
	    rsnSearchAKMSuite(prAdapter, RSN_AKM_SUITE_FT_PSK, ucBssIndex)) {
		u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
	} else if (eAuthMode == AUTH_MODE_WPA2_FT &&
	  rsnSearchAKMSuite(prAdapter, RSN_AKM_SUITE_FT_802_1X, ucBssIndex)) {
		u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
	} else {
		for (i = (prBssRsnInfo->u4AuthKeyMgtSuiteCount - 1); i >= 0;
		     i--) {
			if (rsnSearchAKMSuite(prAdapter,
				prBssRsnInfo->au4AuthKeyMgtSuite[i],
				ucBssIndex)) {
				u4AkmSuite =
					prBssRsnInfo->au4AuthKeyMgtSuite[i];
				break;
			}
		}
	}

	if (u4AkmSuite == 0) {
		DBGLOG(RSN, TRACE, "Cannot support any AKM suites\n");
		return FALSE;
	}

	DBGLOG(RSN, TRACE,
		"Selected pairwise/group cipher: 0x%x/0x%x, AKM suite: 0x%x\n",
		SWAP32(u4PairwiseCipher), SWAP32(u4GroupCipher),
		SWAP32(u4AkmSuite));

#if CFG_SUPPORT_802_11W
	/* check pmf only when rsn ie is selected */
	if (prBssRsnInfo->ucElemId == ELEM_ID_RSN ||
	    prBssRsnInfo->ucOuiType == VENDOR_OUI_TYPE_RSNO ||
	    prBssRsnInfo->ucOuiType == VENDOR_OUI_TYPE_RSNO2) {
		if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo,
			ucBssIndex) == RSN_AUTH_MFP_REQUIRED) {
			if (!prBssRsnInfo->fgRsnCapPresent) {
				DBGLOG(RSN, INFO,
				       "[MFP] Skip RSN IE, No MFP Required Capability.\n");
				return FALSE;
			} else if (!(prBssRsnInfo->u2RsnCap &
					ELEM_WPA_CAP_MFPC)) {
				DBGLOG(RSN, INFO,
				       "[MFP] Skip RSN IE, No MFP Required\n");
				return FALSE;
			}
			u4MgmtProtection =
				ELEM_WPA_CAP_MFPR | ELEM_WPA_CAP_MFPC;
		} else if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo,
			ucBssIndex) == RSN_AUTH_MFP_OPTIONAL) {
			if (prAdapter->rWifiVar.u4SwTestMode ==
				/* PMF Cert. should disallow MFPR if OPTIONAL */
				ENUM_SW_TEST_MODE_SIGMA_PMF)
				u4MgmtProtection = prBssRsnInfo->u2RsnCap &
					ELEM_WPA_CAP_MFPC;
			else
				u4MgmtProtection = prBssRsnInfo->u2RsnCap &
					(ELEM_WPA_CAP_MFPR | ELEM_WPA_CAP_MFPC);
		} else {
			if ((prBssRsnInfo->fgRsnCapPresent) &&
			(prBssRsnInfo->u2RsnCap & ELEM_WPA_CAP_MFPR)) {
				DBGLOG(RSN, INFO,
				       "[MFP] Skip RSN IE, No MFP Required Capability\n");
				return FALSE;
			}
		}

		if (u4MgmtProtection  &&
		    prBssRsnInfo->u4GroupMgmtCipherSuite !=
			RSN_CIPHER_SUITE_BIP_CMAC_128 &&
		    prBssRsnInfo->u4GroupMgmtCipherSuite !=
			RSN_CIPHER_SUITE_BIP_GMAC_256 &&
		    prBssRsnInfo->u4GroupMgmtCipherSuite !=
			prWpaInfo->u4CipherGroupMgmt) {
			DBGLOG(RSN, INFO,
			       "[MFP] Skip RSN IE, No matched GropMgmtCipher expected 0x%x but 0x%x\n",
			       SWAP32(prWpaInfo->u4CipherGroupMgmt),
			       SWAP32(prBssRsnInfo->u4GroupMgmtCipherSuite));
			return FALSE;
		}

		u4GroupMgmtCipher = prBssRsnInfo->u4GroupMgmtCipherSuite;

		DBGLOG(RSN, TRACE,
		       "[MFP] MFP setting=%d, Cap=%d, CapPresent=%d, MgmtProtection = 0x%x, GroupMgmtCipher = 0x%x\n",
		       kalGetRsnIeMfpCap(prAdapter->prGlueInfo, ucBssIndex),
		       prBssRsnInfo->u2RsnCap,
		       prBssRsnInfo->fgRsnCapPresent,
		       u4MgmtProtection, SWAP32(u4GroupMgmtCipher));
	}
#endif

selected:
	eNewAuthMode = rsnKeyMgmtToAuthMode(eAuthMode,
		prWpaInfo->u4WpaVersion, u4AkmSuite);
	if (eNewAuthMode != eAuthMode) {
		DBGLOG(RSN, INFO, "Change AuthMode %d -> %d\n",
			eAuthMode, eNewAuthMode);
		eAuthMode = eNewAuthMode;
	}

#if (CFG_SUPPORT_WIFI_6G == 1)
	if (prBss->eBand == BAND_6G) {
		if (!rsnIsKeyMgmtFor6g(prAdapter,
				u4AkmSuite, ucBssIndex, prBss)) {
#if CFG_SUPPORT_WPA3_LOG
			wpa3Log6gPolicyFail(prAdapter,
				ucBssIndex,
				eAuthMode);
#endif
			DBGLOG(RSN, INFO,
				"Invalid 6g Auth Mode[%d], Akm[%d], only OWE & SAE H2E is allowed\n",
				eAuthMode, u4AkmSuite);
#if (CFG_WLAN_CONNAC3_DEV == 0)
			return FALSE;
#endif
		}
	}
#endif

#if (CFG_SUPPORT_FILS_SK_OFFLOAD == 1)
	if (rsnKeyMgmtFils(u4AkmSuite) && !prBss->ucIsFilsSkSupport) {
		DBGLOG(RSN, INFO, "FILS: fils share key not support\n");
		return FALSE;
	}
#endif /* CFG_SUPPORT_FILS_SK_OFFLOAD */

	prBss->u4RsnSelectedPairwiseCipher = u4PairwiseCipher;
	prBss->u4RsnSelectedGroupCipher = u4GroupCipher;
	prBss->u4RsnSelectedGroupMgmtCipher = u4GroupMgmtCipher;
	prBss->u4RsnSelectedAKMSuite = u4AkmSuite;
	prBss->eRsnSelectedAuthMode = eAuthMode;
	prBss->u4RsnSelectedPmf = u4MgmtProtection;

	if (prBss->u4RsnSelectedProto != IW_AUTH_WPA_VERSION_DISABLED)
		prBss->u4RsnSelectedProto = rsnAuthModeRsn(eAuthMode) ?
			IW_AUTH_WPA_VERSION_WPA2 : IW_AUTH_WPA_VERSION_WPA;

#if (CFG_SUPPORT_RSNO == 1)
	if (fgIsApRsnOveride && prBssRsnInfo) {
		if (prBssRsnInfo->ucElemId == ELEM_ID_RSN)
			prBss->eRsnSelectedRSNOverride =
				RSN_OVERRIDE_RSNE;
		else if (prBssRsnInfo->ucOuiType == VENDOR_OUI_TYPE_RSNO)
			prBss->eRsnSelectedRSNOverride =
				RSN_OVERRIDE_RSNE_OVERRIDE;
		else if (prBssRsnInfo->ucOuiType == VENDOR_OUI_TYPE_RSNO2)
			prBss->eRsnSelectedRSNOverride =
				RSN_OVERRIDE_RSNE_OVERRIDE_2;
	}
#endif /* CFG_SUPPORT_RSNO */

	return TRUE;
}				/* rsnPerformPolicySelection */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate WPA IE for beacon frame.
 *
 * \param[in] pucIeStartAddr Pointer to put the generated WPA IE.
 *
 * \return The append WPA-None IE length
 * \note
 *      Called by: JOIN module, compose beacon IE
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateWpaNoneIE(struct ADAPTER *prAdapter,
			  struct MSDU_INFO *prMsduInfo)
{
	struct WPA_INFO_ELEM *prWpaIE;
	uint32_t u4Suite;
	uint16_t u2SuiteCount;
	uint8_t *cp, *cp2;
	uint8_t ucExpendedLen = 0;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex))
		return;

	if (aisGetAuthMode(prAdapter, ucBssIndex) != AUTH_MODE_WPA_NONE)
		return;

	pucBuffer = (uint8_t *) ((uintptr_t)
				 prMsduInfo->prPacket + (uintptr_t)
				 prMsduInfo->u2FrameLength);
	prWpaIE = (struct WPA_INFO_ELEM *)(pucBuffer);

	/* Start to construct a WPA IE. */
	/* Fill the Element ID field. */
	prWpaIE->ucElemId = ELEM_ID_WPA;

	/* Fill the OUI and OUI Type fields. */
	prWpaIE->aucOui[0] = 0x00;
	prWpaIE->aucOui[1] = 0x50;
	prWpaIE->aucOui[2] = 0xF2;
	prWpaIE->ucOuiType = VENDOR_OUI_TYPE_WPA;

	/* Fill the Version field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2Version, 1);	/* version 1 */
	ucExpendedLen = 6;

	/* Fill the Pairwise Key Cipher Suite List field. */
	u2SuiteCount = 0;
	cp = (uint8_t *) &prWpaIE->aucPairwiseKeyCipherSuite1[0];

	if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_CCMP, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_CCMP;
	else if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_TKIP, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_TKIP;
	else if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_WEP104, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_WEP104;
	else if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_WEP40, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_WEP40;
	else
		u4Suite = WPA_CIPHER_SUITE_TKIP;

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;

	cp = pucBuffer + sizeof(struct WPA_INFO_ELEM);

	/* Fill the Group Key Cipher Suite field as
	 * the same in pair-wise key.
	 */
	WLAN_SET_FIELD_32(&prWpaIE->u4GroupKeyCipherSuite, u4Suite);
	ucExpendedLen += 4;

	/* Fill the Pairwise Key Cipher Suite Count field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2PairwiseKeyCipherSuiteCount,
			  u2SuiteCount);
	ucExpendedLen += 2;

	cp2 = cp;

	/* Fill the Authentication and Key Management Suite
	 * List field.
	 */
	u2SuiteCount = 0;
	cp += 2;

	if (rsnSearchAKMSuite(prAdapter,
		WPA_AKM_SUITE_802_1X, ucBssIndex))
		u4Suite = WPA_AKM_SUITE_802_1X;
	else if (rsnSearchAKMSuite(prAdapter,
		WPA_AKM_SUITE_PSK, ucBssIndex))
		u4Suite = WPA_AKM_SUITE_PSK;
	else
		u4Suite = WPA_AKM_SUITE_NONE;

	/* This shall be the only available value for current implementation */
	ASSERT(u4Suite == WPA_AKM_SUITE_NONE);

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;
	cp += 4;

	/* Fill the Authentication and Key Management Suite Count field. */
	WLAN_SET_FIELD_16(cp2, u2SuiteCount);
	ucExpendedLen += 2;

	/* Fill the Length field. */
	prWpaIE->ucLength = (uint8_t) ucExpendedLen;

	/* Increment the total IE length for the Element ID
	 * and Length fields.
	 */
	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);

}				/* rsnGenerateWpaNoneIE */

#if CFG_ENABLE_WIFI_DIRECT
uint32_t _addWPAIE_impl(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prAdapter->rWifiVar.fgReuseRSNIE)
		return FALSE;

	if (!prBssInfo)
		return FALSE;

	/* AP only */
	if (!IS_BSS_AP(prAdapter, prBssInfo))
		return FALSE;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.
		prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&
		(prP2pSpecBssInfo->u2WpaIeLen != 0)) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucWpaIeBuffer,
			prP2pSpecBssInfo->u2WpaIeLen);
		prMsduInfo->u2FrameLength += prP2pSpecBssInfo->u2WpaIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant WPA IE content w/o update\n");

		return TRUE;
	}

	return FALSE;
}


uint32_t _addRSNIE_impl(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prAdapter->rWifiVar.fgReuseRSNIE)
		return FALSE;

	if (!prBssInfo)
		return FALSE;

	/* AP only */
	if (!IS_BSS_AP(prAdapter, prBssInfo))
		return FALSE;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.
		prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&
		(prP2pSpecBssInfo->u2RsnIeLen != 0)) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucRsnIeBuffer,
			prP2pSpecBssInfo->u2RsnIeLen);
		prMsduInfo->u2FrameLength += prP2pSpecBssInfo->u2RsnIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant RSN IE content w/o update\n");

		return TRUE;
	}

	return FALSE;
}

#endif

static uint8_t rsnIsOsenAuthModeWithRSN(struct ADAPTER *prAdapter,
					uint8_t ucBssIndex)
{
	if (aisGetAuthMode(prAdapter, ucBssIndex) == AUTH_MODE_WPA_OSEN &&
		aisGetConnSettings(prAdapter, ucBssIndex)->fgAuthOsenWithRSN)
		return TRUE;

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate WPA IE for
 *        associate request frame.
 *
 * \param[in]  prCurrentBss     The Selected BSS description
 *
 * \retval The append WPA IE length
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateWPAIE(struct ADAPTER *prAdapter,
		      struct MSDU_INFO *prMsduInfo)
{
	uint8_t *cp;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
#if CFG_ENABLE_WIFI_DIRECT
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;
#endif

	pucBuffer = (uint8_t *) ((uintptr_t)
				 prMsduInfo->prPacket + (uintptr_t)
				 prMsduInfo->u2FrameLength);
	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
#if CFG_ENABLE_WIFI_DIRECT
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar.
			prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */
	if (_addWPAIE_impl(prAdapter, prMsduInfo))
		return;
#endif

	if ((IS_BSS_AIS(prBssInfo) &&
	    (aisGetAuthMode(prAdapter, ucBssIndex) == AUTH_MODE_WPA ||
	     aisGetAuthMode(prAdapter, ucBssIndex) == AUTH_MODE_WPA_PSK))
#if CFG_ENABLE_WIFI_DIRECT
	    || (prAdapter->fgIsP2PRegistered && IS_BSS_P2P(prBssInfo) &&
		 kalP2PGetTkipCipher(prAdapter->prGlueInfo,
				 (uint8_t) prBssInfo->u4PrivateData))
#endif
	) {
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered && prP2pSpecificBssInfo
		    && (prP2pSpecificBssInfo->u2WpaIeLen != 0)) {
			kalMemCopy(pucBuffer,
				prP2pSpecificBssInfo->aucWpaIeBuffer,
				prP2pSpecificBssInfo->u2WpaIeLen);
			prMsduInfo->u2FrameLength +=
			    prP2pSpecificBssInfo->u2WpaIeLen;
			return;
		}
#endif
		/* Construct a WPA IE for association request frame. */
		WPA_IE(pucBuffer)->ucElemId = ELEM_ID_WPA;
		WPA_IE(pucBuffer)->ucLength = ELEM_ID_WPA_LEN_FIXED;
		WPA_IE(pucBuffer)->aucOui[0] = 0x00;
		WPA_IE(pucBuffer)->aucOui[1] = 0x50;
		WPA_IE(pucBuffer)->aucOui[2] = 0xF2;
		WPA_IE(pucBuffer)->ucOuiType = VENDOR_OUI_TYPE_WPA;
		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->u2Version, 1);

#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && IS_BSS_INDEX_P2P(prAdapter, ucBssIndex)) {
			WLAN_SET_FIELD_32(
				&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
				WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(
				&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
				prBssInfo->
					u4RsnSelectedGroupCipher);

		cp = (uint8_t *) &
		    WPA_IE(pucBuffer)->aucPairwiseKeyCipherSuite1[0];

		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->
			u2PairwiseKeyCipherSuiteCount, 1);
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
			&& IS_BSS_INDEX_P2P(prAdapter, ucBssIndex)) {
			WLAN_SET_FIELD_32(cp, WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prBssInfo
						->u4RsnSelectedPairwiseCipher);

		cp = pucBuffer + sizeof(struct WPA_INFO_ELEM);

		WLAN_SET_FIELD_16(cp, 1);
		cp += 2;
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && IS_BSS_INDEX_P2P(prAdapter, ucBssIndex)) {
			WLAN_SET_FIELD_32(cp, WPA_AKM_SUITE_PSK);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prBssInfo
						->u4RsnSelectedAKMSuite);
		cp += 4;

		WPA_IE(pucBuffer)->ucLength = ELEM_ID_WPA_LEN_FIXED;

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}

}				/* rsnGenerateWPAIE */

void rsnGenerateRSNIEImpl(struct ADAPTER *prAdapter,
		      struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex = prMsduInfo->ucBssIndex;
	uint8_t *pucBuffer;
	uint8_t *cp;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo)
		return;

	pucBuffer = (uint8_t *) ((uintptr_t)
				 prMsduInfo->prPacket + (uintptr_t)
				 prMsduInfo->u2FrameLength);

	/* Construct a RSN IE for association request frame. */
	RSN_IE(pucBuffer)->ucElemId = ELEM_ID_RSN;
	RSN_IE(pucBuffer)->ucLength = ELEM_ID_RSN_LEN_FIXED;
	/* Version */
	WLAN_SET_FIELD_16(&RSN_IE(pucBuffer)->u2Version, 1);
	WLAN_SET_FIELD_32(&RSN_IE(pucBuffer)->u4GroupKeyCipherSuite,
			prBssInfo->u4RsnSelectedGroupCipher);
			/* Group key suite */
	cp = (uint8_t *) &RSN_IE(pucBuffer)->aucPairwiseKeyCipherSuite1[0];
	WLAN_SET_FIELD_16(&RSN_IE(pucBuffer)->u2PairwiseKeyCipherSuiteCount, 1);
	WLAN_SET_FIELD_32(cp, prBssInfo->u4RsnSelectedPairwiseCipher);

	cp += 4;

	if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P &&
	    rsnKeyMgmtSae(prBssInfo->u4RsnSelectedAKMSuite)) {
#if CFG_ENABLE_WIFI_DIRECT
		struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo =
			prAdapter->rWifiVar.prP2pSpecificBssInfo
			[prBssInfo->u4PrivateData];
		uint8_t i = 0;

		/* AKM suite count */
		WLAN_SET_FIELD_16(cp,
			prP2pSpecBssInfo->u4KeyMgtSuiteCount);
		cp += 2;

		/* AKM suite */
		for (i = 0;
			i < prP2pSpecBssInfo->u4KeyMgtSuiteCount;
			i++) {
			DBGLOG(RSN, TRACE, "KeyMgtSuite 0x%04x\n",
				prP2pSpecBssInfo->au4KeyMgtSuite[i]);
			WLAN_SET_FIELD_32(cp,
				prP2pSpecBssInfo->au4KeyMgtSuite[i]);
			cp += 4;
		}

		RSN_IE(pucBuffer)->ucLength +=
			(prP2pSpecBssInfo->u4KeyMgtSuiteCount - 1) * 4;
#endif
	} else {
		WLAN_SET_FIELD_16(cp, 1);	/* AKM suite count */
		cp += 2;
		/* AKM suite */
		WLAN_SET_FIELD_32(cp, prBssInfo->u4RsnSelectedAKMSuite);
		DBGLOG(RSN, LOUD, "RSN KeyMgtSuite 0x%04x\n",
				prBssInfo->u4RsnSelectedAKMSuite);
		cp += 4;
	}

	/* Capabilities */
	WLAN_SET_FIELD_16(cp, prBssInfo->u2RsnSelectedCapInfo);
	DBGLOG(RSN, TRACE, "Gen RSN IE=%x\n", prBssInfo->u2RsnSelectedCapInfo);
#if CFG_SUPPORT_802_11W

	if (IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		struct BSS_DESC *prBssDesc =
			aisGetTargetBssDesc(prAdapter, ucBssIndex);

		if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo,
			ucBssIndex) != RSN_AUTH_MFP_DISABLED && prBssDesc) {
			WLAN_SET_FIELD_16(cp,
				prBssDesc->u4RsnSelectedPmf);
				/* Capabilities */
			DBGLOG(RSN, TRACE,
				"RSN_AUTH_MFP - 0x%x\n",
				prBssDesc->u4RsnSelectedPmf);
		} else {
			DBGLOG(RSN, TRACE,
				"!RSN_AUTH_MFP - No MFPC!\n");
		}
	} else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P &&
		   prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
		/* AP PMF */
		/* for AP mode, keep origin RSN IE content w/o update */
	}
#endif
	cp += 2;

	/* Fill PMKID and Group Management Cipher for AIS */
	if (IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo =
			aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
		struct GL_WPA_INFO *prWpaInfo;
		struct STA_RECORD *prStaRec;
		struct PMKID_ENTRY *entry = NULL;

		prStaRec = cnmGetStaRecByIndex(prAdapter,
					prMsduInfo->ucStaRecIndex);

		prWpaInfo = aisGetWpaInfo(prAdapter, ucBssIndex);

		entry = aisSearchPmkidEntry(prAdapter, prStaRec, ucBssIndex);

		/* Fill PMKID Count and List field */
		if (entry) {
			uint8_t *pmk = entry->rBssidInfo.arPMKID;

			RSN_IE(pucBuffer)->ucLength = 38;
			/* Fill PMKID Count field */
			WLAN_SET_FIELD_16(cp, 1);
			cp += 2;
			DBGLOG(RSN, INFO, "BSSID " MACSTR
				"use PMKID " PMKSTR "\n",
				MAC2STR(entry->rBssidInfo.arBSSID),
				pmk[0], pmk[1], pmk[2], pmk[3], pmk[4],
				pmk[5], pmk[6], pmk[7], pmk[8], pmk[9],
				pmk[10], pmk[11], pmk[12] + pmk[13],
				pmk[14], pmk[15]);
			/* Fill PMKID List field */
			kalMemCopy(cp, entry->rBssidInfo.arPMKID,
				IW_PMKID_LEN);
			cp += IW_PMKID_LEN;
		}
#if CFG_SUPPORT_802_11W
		else {
			/* Follow supplicant flow to
			 * fill PMKID Count field = 0 only when
			 * Group Management Cipher field
			 * need to be filled
			 */
			if (prAisSpecBssInfo->fgMgmtProtection) {
				WLAN_SET_FIELD_16(cp, 0);
				cp += 2;
				RSN_IE(pucBuffer)->ucLength += 2;
			}
		}

		/* Fill Group Management Cipher field */
		if (prAisSpecBssInfo->fgMgmtProtection) {
			WLAN_SET_FIELD_32(cp,
			prBssInfo->u4RsnSelectedGroupMgmtCipher);
			cp += 4;
			RSN_IE(pucBuffer)->ucLength += 4;
		}
#endif
	}

	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate RSN IE for
 *        associate request frame.
 *
 * \param[in]  prMsduInfo     The Selected BSS description
 *
 * \retval The append RSN IE length
 *
 * \note
 *      Called by: AIS module, P2P module, BOW module Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateRSNIE(struct ADAPTER *prAdapter,
		      struct MSDU_INFO *prMsduInfo)
{
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;

	/* Todo:: network id */
	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	/* For FT, we reuse the RSN Element composed in userspace */
	if (authAddRSNIE_impl(prAdapter, prMsduInfo, AIS_FT_R1)) {
		DBGLOG(RSN, TRACE, "RSN IE: authAddRSNIE return\n");
		return;
	}


#if CFG_ENABLE_WIFI_DIRECT
	if (_addRSNIE_impl(prAdapter, prMsduInfo)) {
		DBGLOG(RSN, TRACE, "RSN IE: _addRSNIE return\n");
		return;
	}

	if (prAdapter->fgIsP2PRegistered &&
	    IS_BSS_INDEX_P2P(prAdapter, ucBssIndex) &&
	    kalP2PGetCcmpCipher(prAdapter->prGlueInfo,
			(uint8_t) prBssInfo->u4PrivateData))
		goto add_rsne;
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	if (IS_BSS_BOW(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)))
		goto add_rsne;
#endif

	if (IS_BSS_INDEX_AIS(prAdapter, ucBssIndex) &&
	     (rsnAuthModeRsn(aisGetAuthMode(prAdapter, ucBssIndex)) ||
	      rsnIsOsenAuthModeWithRSN(prAdapter, ucBssIndex)))
		goto add_rsne;

	return;
add_rsne:
	rsnGenerateRSNIEImpl(prAdapter, prMsduInfo);
}				/* rsnGenerateRSNIE */

#if (CFG_SUPPORT_RSNO == 1)
void rsnGenerateRsnSelectionIE(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer, *cp;
	uint8_t ucBssIndex = prMsduInfo->ucBssIndex;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;

	if (!rsnSupportRSNOverride(prAdapter, ucBssIndex))
		return;

	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	cp = pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;

	if (prAisSpecificBssInfo->eRsnSelectedRSNOverride !=
	    RSN_OVERRIDE_NOT_USED) {
		*cp++ = ELEM_ID_VENDOR;
		*cp++ = 4 + 1;
		WLAN_PUT_BE32(cp, VENDOR_IE_TYPE_RSN_SEL);
		cp += 4;

		switch (prAisSpecificBssInfo->eRsnSelectedRSNOverride) {
		case RSN_OVERRIDE_RSNE:
			*cp++ = RSN_SELECTION_RSNE;
			break;
		case RSN_OVERRIDE_RSNE_OVERRIDE:
			*cp++ = RSN_SELECTION_RSNE_OVERRIDE;
			break;
		case RSN_OVERRIDE_RSNE_OVERRIDE_2:
			*cp++ = RSN_SELECTION_RSNE_OVERRIDE_2;
			break;
		default:
			DBGLOG(RSN, WARN, "wrong rsn override=%d\n",
				prAisSpecificBssInfo->eRsnSelectedRSNOverride);
			*cp++ = 0xfe;
			break;
		}

		prMsduInfo->u2FrameLength += cp - pucBuffer;

		DBGDUMP_MEM8(RSN, INFO,  "RSN Selection",
			pucBuffer, IE_SIZE(pucBuffer));
	}
}
#endif /* CFG_SUPPORT_RSNO */

#if CFG_SUPPORT_AAA
void rsnGenerateRSNXIE(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.
		prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&
		(prP2pSpecBssInfo->u2RsnxIeLen != 0)) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucRsnxIeBuffer,
			prP2pSpecBssInfo->u2RsnxIeLen);
		prMsduInfo->u2FrameLength +=
			prP2pSpecBssInfo->u2RsnxIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant RSNXIE content w/o update\n");
	}
}

void rsnGenerateOWEIE(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.
		prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&
		(prP2pSpecBssInfo->u2OweIeLen != 0)) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucOweIeBuffer,
			prP2pSpecBssInfo->u2OweIeLen);
		prMsduInfo->u2FrameLength +=
			prP2pSpecBssInfo->u2OweIeLen;

		DBGLOG(RSN, INFO,
			"[OWE] Keep supplicant OWEIE content w/o update\n");
	}
}

#if (CFG_SUPPORT_RSNO == 1)
void rsnGenerateRSNOIE(struct ADAPTER *prAdapter,
			struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.prP2pSpecificBssInfo[
			prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo && prP2pSpecBssInfo->u2RsnoIeLen != 0) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucRsnoIeBuffer,
			prP2pSpecBssInfo->u2RsnoIeLen);
		prMsduInfo->u2FrameLength +=
			prP2pSpecBssInfo->u2RsnoIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant RSNOIE content w/o update\n");
	}
}

void rsnGenerateRSNO2IE(struct ADAPTER *prAdapter,
			struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.prP2pSpecificBssInfo[
			prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo && prP2pSpecBssInfo->u2Rsno2IeLen != 0) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucRsno2IeBuffer,
			prP2pSpecBssInfo->u2Rsno2IeLen);
		prMsduInfo->u2FrameLength +=
			prP2pSpecBssInfo->u2Rsno2IeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant RSNO2IE content w/o update\n");
	}
}

void rsnGenerateRSNXOIE(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo)
		return;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.prP2pSpecificBssInfo[
			prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&	prP2pSpecBssInfo->u2RsnxoIeLen != 0) {
		uint8_t *pucBuffer =
			(uint8_t *) ((uintptr_t)
			prMsduInfo->prPacket + (uintptr_t)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucRsnxoIeBuffer,
			prP2pSpecBssInfo->u2RsnxoIeLen);
		prMsduInfo->u2FrameLength +=
			prP2pSpecBssInfo->u2RsnxoIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant RSNXOIE content w/o update\n");
	}
}
#endif /* CFG_SUPPORT_RSNO */
#endif /* CFG_SUPPORT_AAA*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parse the given IE buffer and check if it is WFA IE and return Type
 *	and SubType for further process.
 *
 * \param[in] pucBuf             Pointer to buffer of WFA Information Element.
 * \param[out] pucOuiType        Pointer to the storage of OUI Type.
 * \param[out] pu2SubTypeVersion Pointer to the storage of OUI SubType
 *					and Version.
 * \retval TRUE  Parse IE ok
 * \retval FALSE Parse IE fail
 */
/*----------------------------------------------------------------------------*/
u_int8_t
rsnParseCheckForWFAInfoElem(struct ADAPTER *prAdapter,
			    uint8_t *pucBuf, uint8_t *pucOuiType,
			    uint16_t *pu2SubTypeVersion)
{
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;
	struct IE_WFA *prWfaIE;

	prWfaIE = (struct IE_WFA *)pucBuf;
	do {
		if (IE_LEN(pucBuf) <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE) {
			break;
		} else if (prWfaIE->aucOui[0] != aucWfaOui[0] ||
			   prWfaIE->aucOui[1] != aucWfaOui[1]
			   || prWfaIE->aucOui[2] != aucWfaOui[2]) {
			break;
		}

		*pucOuiType = prWfaIE->ucOuiType;
		WLAN_GET_FIELD_16(&prWfaIE->aucOuiSubTypeVersion[0],
				  pu2SubTypeVersion);

		return TRUE;
	} while (FALSE);

	return FALSE;

}				/* end of rsnParseCheckForWFAInfoElem() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parse the given IE buffer and check if it is WFA IE and return Type
 *	and SubType for further process.
 *
 * \param[in] pucBuf             Pointer to buffer of WFA Information Element.
 * \param[out] pucOuiType        Pointer to the storage of OUI Type.
 *					and Version.
 * \retval TRUE  Parse IE ok
 * \retval FALSE Parse IE fail
 */
/*----------------------------------------------------------------------------*/
u_int8_t
rsnParseCheckForWFASpecificElem(struct ADAPTER *prAdapter,
			    uint8_t *pucBuf, uint8_t *pucOuiType)
{
	struct IE_WFA_SPECIFIC *prWfaIE = (struct IE_WFA_SPECIFIC *)pucBuf;

	if (IE_LEN(pucBuf) < 4 ||
	    WLAN_GET_BE24(prWfaIE->aucOui) != VENDOR_IE_WFA_SPECIFIC_OUI)
		return FALSE;

	*pucOuiType = prWfaIE->ucOuiType;

	return TRUE;
} /* end of rsnParseCheckForWFASpecificElem() */

#if CFG_SUPPORT_AAA
static u_int8_t rsnParserCheckForPmkid(struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct STA_RECORD *prStaRec,
	struct RSN_INFO *prRsnInfo)
{
	struct PMKID_ENTRY *entry;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_BSS_INFO *prMldBss = mldBssGetByBss(prAdapter, prBssInfo);
#endif

	if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.fgSapCheckPmkidInDriver) ||
	    !rsnKeyMgmtSae(prBssInfo->u4RsnSelectedAKMSuite) ||
	    prRsnInfo->u2PmkidCount <= 0)
		return TRUE;

	entry = rsnSearchPmkidEntry(prAdapter,
		cnmStaRecAuthAddr(prAdapter, prStaRec),
		prStaRec->ucBssIndex);
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	if (!entry && IS_MLD_BSSINFO_MULTI(prMldBss)) {
		struct LINK *prBssList;
		struct BSS_INFO *prTempBss;

		prBssList = &prMldBss->rBssList;
		LINK_FOR_EACH_ENTRY(prTempBss, prBssList, rLinkEntryMld,
				    struct BSS_INFO) {
			entry = rsnSearchPmkidEntry(prAdapter,
				cnmStaRecAuthAddr(prAdapter, prStaRec),
				prTempBss->ucBssIndex);
			if (entry)
				break;
		}
	}
#endif

	DBGLOG(RSN, LOUD,
		"Parse PMKID " PMKSTR " from " MACSTR "\n",
		prRsnInfo->aucPmkid[0], prRsnInfo->aucPmkid[1],
		prRsnInfo->aucPmkid[2], prRsnInfo->aucPmkid[3],
		prRsnInfo->aucPmkid[4], prRsnInfo->aucPmkid[5],
		prRsnInfo->aucPmkid[6], prRsnInfo->aucPmkid[7],
		prRsnInfo->aucPmkid[8], prRsnInfo->aucPmkid[9],
		prRsnInfo->aucPmkid[10], prRsnInfo->aucPmkid[11],
		prRsnInfo->aucPmkid[12] + prRsnInfo->aucPmkid[13],
		prRsnInfo->aucPmkid[14], prRsnInfo->aucPmkid[15],
		MAC2STR(prStaRec->aucMacAddr));

	if (!entry) {
		DBGLOG(RSN, WARN,
			"RSN with no PMKID, bss=%d, sta addr="MACSTR"\n",
			prStaRec->ucBssIndex,
			MAC2STR(prStaRec->aucMacAddr));
#if (CFG_SUPPORT_802_11BE_MLO == 1)
		DBGLOG(RSN, WARN,
			"mlo=%d, sta mld addr=" MACSTR "\n",
			mldIsMultiLinkFormed(prAdapter, prStaRec),
			MAC2STR(prStaRec->aucMldAddr));
#endif
		return FALSE;
	} else if (kalMemCmp(prRsnInfo->aucPmkid, entry->rBssidInfo.arPMKID,
			     IW_PMKID_LEN) != 0) {
		DBGLOG(RSN, WARN, "RSN with invalid PMKID\n");
		return FALSE;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parse the given IE buffer and check if it is RSN IE with CCMP PSK
 *
 * \param[in] prAdapter             Pointer to Adapter
 * \param[in] prSwRfb               Pointer to the rx buffer
 * \param[in] pIE                   Pointer rthe buffer of Information Element.
 * \param[out] prStatusCode     Pointer to the return status code.

 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnParserCheckForRSNCCMPPSK(struct ADAPTER *prAdapter,
				 struct RSN_INFO_ELEM *prIe,
				 struct STA_RECORD *prStaRec,
				 uint16_t *pu2StatusCode)
{

	struct RSN_INFO rRsnIe;
	struct BSS_INFO *prBssInfo;
	uint8_t i;
	uint16_t statusCode;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prStaRec->ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return;
	}
	*pu2StatusCode = STATUS_CODE_INVALID_INFO_ELEMENT;
	kalMemZero(&rRsnIe, sizeof(struct RSN_INFO));

	if (rsnParseRsnIE(prAdapter, (uint8_t *)prIe, &rRsnIe)) {
		/* When softap's conf support both TKIP&CCMP,
		 * the Group Cipher Suite would be TKIP
		 * If we check the Group Cipher Suite == CCMP
		 * about peer's Asso Req
		 * The connection would be fail
		 * due to STATUS_CODE_INVALID_GROUP_CIPHER
		 */
		if (!prAdapter->rWifiVar.fgReuseRSNIE) {
			if ((rRsnIe.u4PairwiseKeyCipherSuiteCount != 1)
			    || (rRsnIe.au4PairwiseKeyCipherSuite[0] !=
				RSN_CIPHER_SUITE_CCMP)) {
				*pu2StatusCode =
					STATUS_CODE_INVALID_PAIRWISE_CIPHER;
				return;
			}
			if (rRsnIe.u4GroupKeyCipherSuite !=
					RSN_CIPHER_SUITE_CCMP) {
				*pu2StatusCode =
					STATUS_CODE_INVALID_GROUP_CIPHER;
				return;
			}
		}

		if ((rRsnIe.u4AuthKeyMgtSuiteCount != 1)
			|| ((rRsnIe.au4AuthKeyMgtSuite[0] != RSN_AKM_SUITE_PSK)
#if CFG_SUPPORT_SOFTAP_WPA3
			&& (!rsnKeyMgmtSae(rRsnIe.au4AuthKeyMgtSuite[0]))
#endif
			&& (rRsnIe.au4AuthKeyMgtSuite[0] != RSN_AKM_SUITE_OWE)
			)) {
			DBGLOG(RSN, WARN, "RSN with invalid AKMP\n");
			*pu2StatusCode = STATUS_CODE_INVALID_AKMP;
			return;
		}

		if (rsnParserCheckForPmkid(prAdapter, prBssInfo, prStaRec,
					   &rRsnIe) == FALSE) {
			*pu2StatusCode = STATUS_INVALID_PMKID;
			return;
		}

		DBGLOG(RSN, TRACE, "RSN with CCMP-PSK\n");
		*pu2StatusCode = WLAN_STATUS_SUCCESS;

#if CFG_SUPPORT_802_11W
		/* AP PMF */
		/* 1st check: if already PMF connection, reject assoc req:
		 * error 30 ASSOC_REJECTED_TEMPORARILY
		 */

		if (prBssInfo->u4RsnSelectedAKMSuite ==
			RSN_AKM_SUITE_OWE)
			DBGLOG(RSN, INFO, "[OWE] Ignore PMF check\n");
		else if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			DBGLOG(AAA, INFO,
				"Drop RxAssoc\n");
			*pu2StatusCode = STATUS_CODE_ASSOC_REJECTED_TEMPORARILY;
			return;
		}

		/* if RSN capability not exist, just return */
		if (!rRsnIe.fgRsnCapPresent) {
			*pu2StatusCode = WLAN_STATUS_SUCCESS;
			return;
		}

		prStaRec->rPmfCfg.fgMfpc = (rRsnIe.u2RsnCap &
					    ELEM_WPA_CAP_MFPC) ? 1 : 0;
		prStaRec->rPmfCfg.fgMfpr = (rRsnIe.u2RsnCap &
					    ELEM_WPA_CAP_MFPR) ? 1 : 0;

		prStaRec->rPmfCfg.fgSaeRequireMfp = FALSE;

		for (i = 0; i < rRsnIe.u4AuthKeyMgtSuiteCount; i++) {
			if ((rRsnIe.au4AuthKeyMgtSuite[i] ==
			     RSN_AKM_SUITE_802_1X_SHA256) ||
			    (rRsnIe.au4AuthKeyMgtSuite[i] ==
			     RSN_AKM_SUITE_PSK_SHA256)) {
				DBGLOG(RSN, INFO, "STA SHA256 support\n");
				prStaRec->rPmfCfg.fgSha256 = TRUE;
				break;
			} else if (rsnKeyMgmtSae(
					rRsnIe.au4AuthKeyMgtSuite[i])) {
				DBGLOG(RSN, INFO, "STA SAE support\n");
				prStaRec->rPmfCfg.fgSaeRequireMfp = TRUE;
				break;
			}
		}

		DBGLOG(RSN, INFO,
		       "STA Assoc req mfpc:%d, mfpr:%d, sha256:%d, bssIndex:%d, applyPmf:%d\n",
		       prStaRec->rPmfCfg.fgMfpc, prStaRec->rPmfCfg.fgMfpr,
		       prStaRec->rPmfCfg.fgSha256, prStaRec->ucBssIndex,
		       prStaRec->rPmfCfg.fgApplyPmf);

		/* if PMF validation fail, return success as legacy association
		 */
		statusCode = rsnPmfCapableValidation(prAdapter, prBssInfo,
						     prStaRec);
		*pu2StatusCode = statusCode;
#endif
	}

}
#endif

static void rsnMicErrorSendMsg(struct ADAPTER *prAdapter,
	struct STA_RECORD *prSta, u_int8_t fgFlags)
{
	struct MSG_MIC_ERROR *prMicErrorMsg;

	prMicErrorMsg = (struct MSG_MIC_ERROR *) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, sizeof(struct MSG_MIC_ERROR));
	if (!prMicErrorMsg) {
		DBGLOG(RSN, WARN, "cnmMemAlloc Fail\n");
		return;
	}

	prMicErrorMsg->rMsgHdr.eMsgId = MID_RSN_MIC_FAIL;
	prMicErrorMsg->prStaRec = prSta;
	prMicErrorMsg->fgFlags = fgFlags;

	DBGLOG(RSN, LOUD,
		"Handle Msg eMsgId:%u ucBssidx:%u fgFlags:%u\n",
		prMicErrorMsg->rMsgHdr.eMsgId,
		prMicErrorMsg->prStaRec->ucBssIndex,
		prMicErrorMsg->fgFlags);

	mboxSendMsg(prAdapter, MBOX_ID_0,
		(struct MSG_HDR *) prMicErrorMsg, MSG_SEND_METHOD_BUF);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate an authentication event to NDIS.
 *
 * \param[in] u4Flags Authentication event: \n
 *                     PARAM_AUTH_REQUEST_REAUTH 0x01 \n
 *                     PARAM_AUTH_REQUEST_KEYUPDATE 0x02 \n
 *                     PARAM_AUTH_REQUEST_PAIRWISE_ERROR 0x06 \n
 *                     PARAM_AUTH_REQUEST_GROUP_ERROR 0x0E \n
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void rsnGenMicErrorEvent(struct ADAPTER *prAdapter,
	struct STA_RECORD *prSta, u_int8_t fgFlags)
{
	struct PARAM_INDICATION_EVENT authEvent;
	struct BSS_INFO *prAisBssInfo;
	uint8_t ucBssIndex = 0;

	ucBssIndex = prSta->ucBssIndex;

	prAisBssInfo =
		aisGetAisBssInfo(prAdapter,
		ucBssIndex);

	/* Status type: Authentication Event */
	authEvent.rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

	/* Authentication request */
	authEvent.rAuthReq.u4Length = sizeof(struct PARAM_AUTH_REQUEST);
	COPY_MAC_ADDR(authEvent.rAuthReq.arBssid,
		prAisBssInfo->aucBSSID);
	if (fgFlags == TRUE)
		authEvent.rAuthReq.u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
	else
		authEvent.rAuthReq.u4Flags = PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (void *)&authEvent,
				     sizeof(struct PARAM_INDICATION_EVENT),
				     ucBssIndex);
}				/* rsnGenMicErrorEvent */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to handle TKIP MIC failures.
 *
 * \param[in] adapter_p Pointer to the adapter object data area.
 * \param[in] prSta Pointer to the STA which occur MIC Error
 * \param[in] fgErrorKeyType type of error key
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnTkipHandleMICFailure(struct ADAPTER *prAdapter,
			     struct STA_RECORD *prSta,
			     u_int8_t fgErrorKeyType)
{
	/* UINT_32               u4RsnaCurrentMICFailTime; */
	/* P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo; */

#if 1
	rsnMicErrorSendMsg(prAdapter, prSta, fgErrorKeyType);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO,
	       "Send TKIP MIC error event msg to main thread (type: 0%d)\n",
	       fgErrorKeyType);
#else
	ASSERT(prSta);

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, prSta->ucBssIndex);

	/* Record the MIC error occur time. */
	GET_CURRENT_SYSTIME(&u4RsnaCurrentMICFailTime);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO,
	       "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);

	/* If less than 60 seconds have passed since a previous TKIP MIC
	 * failure, disassociate from the AP and wait for 60 seconds
	 * before (re)associating with the same AP.
	 */
	if (prAisSpecBssInfo->u4RsnaLastMICFailTime != 0 &&
	    !CHECK_FOR_TIMEOUT(u4RsnaCurrentMICFailTime,
			       prAisSpecBssInfo->u4RsnaLastMICFailTime,
			       SEC_TO_SYSTIME(TKIP_COUNTERMEASURE_SEC))) {
		/* If less than 60 seconds expired since last MIC error,
		 * we have to block traffic.
		 */

		DBGLOG(RSN, INFO, "Start blocking traffic!\n");
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);

		secFsmEventStartCounterMeasure(prAdapter, prSta);
	} else {
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);
		DBGLOG(RSN, INFO, "First TKIP MIC error!\n");
	}

	COPY_SYSTIME(prAisSpecBssInfo->u4RsnaLastMICFailTime,
		     u4RsnaCurrentMICFailTime);
#endif
}				/* rsnTkipHandleMICFailure */

void rsnMicErrorHandleMsg(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct MSG_MIC_ERROR *prMicErrorMsg;

	prMicErrorMsg = (struct MSG_MIC_ERROR *)prMsgHdr;

	DBGLOG(RSN, LOUD,
		"Handle Msg eMsgId:%u ucBssidx:%u fgFlags:%u\n",
		prMicErrorMsg->rMsgHdr.eMsgId,
		prMicErrorMsg->prStaRec->ucBssIndex,
		prMicErrorMsg->fgFlags);

	rsnGenMicErrorEvent(prAdapter, prMicErrorMsg->prStaRec,
				prMicErrorMsg->fgFlags);

	cnmMemFree(prAdapter, prMsgHdr);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to search the desired entry in
 *        PMKID cache according to the BSSID
 *
 * \param[in] pucBssid Pointer to the BSSID
 * \param[out] pu4EntryIndex Pointer to place the found entry index
 *
 * \retval TRUE, if found one entry for specified BSSID
 * \retval FALSE, if not found
 */
/*----------------------------------------------------------------------------*/
struct PMKID_ENTRY *rsnSearchPmkidEntry(struct ADAPTER *prAdapter,
					uint8_t *pucBssid,
					uint8_t ucBssIndex)
{
	return rsnSearchPmkidEntryEx(prAdapter, pucBssid,
		NULL, NULL, ucBssIndex);
} /* rsnSearchPmkidEntry */

struct PMKID_ENTRY *rsnSearchPmkidEntryEx(struct ADAPTER *prAdapter,
					uint8_t *pucBssid,
					struct PARAM_SSID *prSsid,
					uint8_t *pucFilsCacheId,
					uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;

	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return NULL;
	}
	cache = &prBssInfo->rPmkidCache;

	LINK_FOR_EACH_ENTRY(entry, cache, rLinkEntry, struct PMKID_ENTRY) {
		if (pucBssid &&
		    EQUAL_MAC_ADDR(entry->rBssidInfo.arBSSID, pucBssid))
			return entry;
	}

	if (prSsid && prSsid->u4SsidLen) {
		LINK_FOR_EACH_ENTRY(entry, cache, rLinkEntry,
				struct PMKID_ENTRY) {
			if (EQUAL_SSID(entry->rBssidInfo.rSsid.aucSsid,
				       entry->rBssidInfo.rSsid.u4SsidLen,
				       prSsid->aucSsid, prSsid->u4SsidLen))
				return entry;
		}
	}

#if (CFG_SUPPORT_FILS_SK_OFFLOAD == 1)
	if (prSsid && prSsid->u4SsidLen && pucFilsCacheId) {
		LINK_FOR_EACH_ENTRY(entry, cache, rLinkEntry,
				struct PMKID_ENTRY) {
			if (!entry->rBssidInfo.fgFilsCacheIdSet)
				continue;
			if (EQUAL_SSID(entry->rBssidInfo.rSsid.aucSsid,
				       entry->rBssidInfo.rSsid.u4SsidLen,
				       prSsid->aucSsid, prSsid->u4SsidLen) &&
			    !kalMemCmp(entry->rBssidInfo.arFilsCacheId,
				       pucFilsCacheId, FILS_CACHE_ID_LEN))
				return entry;
		}
	}
#endif /* CFG_SUPPORT_FILS_SK_OFFLOAD */

	return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to add/update pmkid.
 *
 * \param[in] prPmkid The new pmkid
 *
 * \return status
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnSetPmkid(struct ADAPTER *prAdapter,
		    struct PARAM_PMKID *prPmkid)
{
	struct BSS_INFO *prBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	GLUE_SPIN_LOCK_DECLARATION();

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prPmkid->ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	cache = &prBssInfo->rPmkidCache;

	GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);
	if (prPmkid->fgFilsCacheIdSet)
		entry = rsnSearchPmkidEntryEx(prAdapter, NULL,
			&prPmkid->rSsid, prPmkid->arFilsCacheId,
			prPmkid->ucBssIdx);
	else
		entry = rsnSearchPmkidEntryEx(prAdapter, prPmkid->arBSSID,
			NULL, NULL,
			prPmkid->ucBssIdx);
	if (entry)
		LINK_REMOVE_KNOWN_ENTRY(cache, entry);
	GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);

	if (!entry) {
		entry = kalMemAlloc(sizeof(struct PMKID_ENTRY), VIR_MEM_TYPE);
		if (!entry)
			return -ENOMEM;
	}

	DBGLOG(RSN, INFO,
		"[%d] Set " MACSTR
		", cacheid(set=%d)=0x%2x%2x, total %d, expiration at %d, PMKID"
		PMKSTR "\n",
		prPmkid->ucBssIdx,
		MAC2STR(prPmkid->arBSSID), prPmkid->fgFilsCacheIdSet,
		prPmkid->arFilsCacheId[0], prPmkid->arFilsCacheId[1],
		cache->u4NumElem, prPmkid->u4Expiration,
		prPmkid->arPMKID[0], prPmkid->arPMKID[1], prPmkid->arPMKID[2],
		prPmkid->arPMKID[3], prPmkid->arPMKID[4], prPmkid->arPMKID[5],
		prPmkid->arPMKID[6], prPmkid->arPMKID[7], prPmkid->arPMKID[8],
		prPmkid->arPMKID[9], prPmkid->arPMKID[10], prPmkid->arPMKID[11],
		prPmkid->arPMKID[12] + prPmkid->arPMKID[13],
		prPmkid->arPMKID[14], prPmkid->arPMKID[15]);

	kalMemCopy(&entry->rBssidInfo, prPmkid, sizeof(struct PARAM_PMKID));
	entry->u2StatusCode = STATUS_CODE_SUCCESSFUL;
	entry->u2ReasonCode = REASON_CODE_RESERVED;
	GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);
	LINK_INSERT_TAIL(cache, &entry->rLinkEntry);
	GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);
	return WLAN_STATUS_SUCCESS;
} /* rsnSetPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to del pmkid.
 *
 * \param[in] prPmkid pmkid should be deleted
 *
 * \return status
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnDelPmkid(struct ADAPTER *prAdapter,
		    struct PARAM_PMKID *prPmkid)
{
	struct BSS_INFO *prBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	GLUE_SPIN_LOCK_DECLARATION();

	if (!prPmkid)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(RSN, TRACE, "[%d] Del " MACSTR " pmkid\n",
		prPmkid->ucBssIdx,
		MAC2STR(prPmkid->arBSSID));

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prPmkid->ucBssIdx);
	cache = &prBssInfo->rPmkidCache;
	GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);
	if (prPmkid->fgFilsCacheIdSet)
		entry = rsnSearchPmkidEntryEx(prAdapter, NULL,
			&prPmkid->rSsid, prPmkid->arFilsCacheId,
			prPmkid->ucBssIdx);
	else
		entry = rsnSearchPmkidEntryEx(prAdapter, prPmkid->arBSSID,
			NULL, NULL,
			prPmkid->ucBssIdx);
	if (entry)
		LINK_REMOVE_KNOWN_ENTRY(cache, entry);
	GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);

	if (entry) {
		if (kalMemCmp(prPmkid->arPMKID,
			entry->rBssidInfo.arPMKID, IW_PMKID_LEN)) {
			DBGLOG(RSN, WARN,
				"Del " MACSTR " pmkid but mismatch\n",
				MAC2STR(prPmkid->arBSSID));
		}
		kalMemFree(entry, VIR_MEM_TYPE, sizeof(struct PMKID_ENTRY));
	}

	return WLAN_STATUS_SUCCESS;
} /* rsnDelPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to delete all pmkid.
 *
 * \return status
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnFlushPmkid(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK cache;

	GLUE_SPIN_LOCK_DECLARATION();

	prBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	LINK_INITIALIZE(&cache);

	DBGLOG(RSN, TRACE, "[%d] Flush Pmkid total:%d\n",
		ucBssIndex,
		prBssInfo->rPmkidCache.u4NumElem);

	GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);
	LINK_MERGE_TO_HEAD(&cache, &prBssInfo->rPmkidCache);
	GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_PMKID);

	while (!LINK_IS_EMPTY(&cache)) {
		LINK_REMOVE_HEAD_VAR(&cache, entry, struct PMKID_ENTRY *);
		kalMemFree(entry, VIR_MEM_TYPE, sizeof(struct PMKID_ENTRY));
	}

	return WLAN_STATUS_SUCCESS;
} /* rsnDelPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate an PMKID candidate list
 *        indication to NDIS.
 *
 * \param[in] prAdapter Pointer to the adapter object data area.
 * \param[in] u4Flags PMKID candidate list event:
 *                    PARAM_PMKID_CANDIDATE_PREAUTH_ENABLED 0x01
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnGeneratePmkidIndication(struct ADAPTER *prAdapter,
				struct PARAM_PMKID_CANDIDATE *prCandi,
				uint8_t ucBssIndex)
{
	struct PARAM_INDICATION_EVENT pmkidEvent;

	/* Status type: PMKID Candidatelist Event */
	pmkidEvent.rStatus.eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;
	kalMemCopy(&pmkidEvent.rCandi, prCandi,
		sizeof(struct PARAM_PMKID_CANDIDATE));

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (void *) &pmkidEvent,
				     sizeof(struct PARAM_INDICATION_EVENT),
				     ucBssIndex);
} /* rsnGeneratePmkidIndication */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to chek PMK expiration. If it will expire
 *        within PMK_REFRESH_THRESHOLD_SEC seconds, don't set PMKID in
 *        ASSOC REQ.
 *
 * \retval TRUE, if pmk is going to expire
 * \retval FALSE, otherwise
 */
/*----------------------------------------------------------------------------*/
uint8_t rsnCheckPmkExpiration(struct ADAPTER *prAdapter,
					struct PMKID_ENTRY *targetEntry,
					uint8_t ucBssIndex)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	struct BSS_INFO *prBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;
	uint32_t u4MinExpiration, u4RefreshThreshold;
	OS_SYSTIME now;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return FALSE;
	}
	cache = &prBssInfo->rPmkidCache;

	u4RefreshThreshold = prWifiVar->u4PmkRefreshThreshold;
	u4MinExpiration = targetEntry->rBssidInfo.u4Expiration;
	GET_BOOT_SYSTIME(&now);

	LINK_FOR_EACH_ENTRY(entry, cache, rLinkEntry, struct PMKID_ENTRY) {
		if ((targetEntry->rBssidInfo.u2PMKLen ==
			entry->rBssidInfo.u2PMKLen) &&
		    !kalMemCmp(targetEntry->rBssidInfo.arPMK,
				entry->rBssidInfo.arPMK,
				entry->rBssidInfo.u2PMKLen) &&
		    (entry->rBssidInfo.u4Expiration < u4MinExpiration))
			u4MinExpiration = entry->rBssidInfo.u4Expiration;
	}

	if (CHECK_FOR_EXPIRATION(MSEC_TO_SEC(now) + u4RefreshThreshold,
				u4MinExpiration)) {
		DBGLOG(RSN, INFO,
			"PMK is almost expired, pmk expired time=%d, refresh threshold time=%llu!\n",
			u4MinExpiration,
			MSEC_TO_SEC(now) + u4RefreshThreshold);
		return TRUE;
	}

	return FALSE;
}

#if CFG_SUPPORT_802_11W

/*----------------------------------------------------------------------------*/
/*!
 * \brief to check if the Bip Key installed or not
 *
 * \param[in]
 *           prAdapter
 *
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnCheckBipKeyInstalled(struct ADAPTER
				 *prAdapter, struct STA_RECORD *prStaRec)
{
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;

	/* caution: prStaRec might be null ! */
	if (!prStaRec)
		return FALSE;
	ucBssIndex = prStaRec->ucBssIndex;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null, bssIndex=%d\n",
			ucBssIndex);
		return FALSE;
	}

	if (IS_BSS_AIS(prBssInfo)) {
		return aisGetAisSpecBssInfo(prAdapter,
				ucBssIndex)->fgBipKeyInstalled;
	} else if (IS_BSS_APGO(prBssInfo)) {
		u_int8_t fgApplyPmf = prStaRec->rPmfCfg.fgApplyPmf &&
			prStaRec->fgTransmitKeyExist;

		if (fgApplyPmf)
			DBGLOG(RSN, INFO, "AP-STA PMF capable\n");
		return fgApplyPmf;
	} else
		return FALSE;
}

uint32_t rsnCheckBipGmacKeyInstall(struct ADAPTER
				 *prAdapter, struct STA_RECORD *prStaRec)
{
	uint8_t ucBssIndex;

	/* caution: prStaRec might be null ! */
	if (!prStaRec)
		return FALSE;
	ucBssIndex = prStaRec->ucBssIndex;

	if (IS_BSS_INDEX_AIS(prAdapter, prStaRec->ucBssIndex)) {
		return aisGetAisSpecBssInfo(prAdapter,
				ucBssIndex)->fgBipGmacKeyInstalled;
	} else
		return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to check the Sa query timeout.
 *
 *
 * \note
 *      Called by: AIS module, Handle by Sa Quert timeout
 */
/*----------------------------------------------------------------------------*/
uint8_t rsnCheckSaQueryTimeout(
	struct ADAPTER *prAdapter, uint8_t ucBssIdx)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct BSS_INFO *prAisBssInfo;
	uint32_t now;

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIdx);
	prAisBssInfo =
		aisGetAisBssInfo(prAdapter, ucBssIdx);

	GET_CURRENT_SYSTIME(&now);

	if (CHECK_FOR_TIMEOUT(now, prBssSpecInfo->u4SaQueryStart,
			      TU_TO_MSEC(SA_QUERY_RETRY_TIMEOUT))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		prBssSpecInfo->ucSaQueryTimedOut = 1;
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
		prBssSpecInfo->u4SaQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);

		if (prAisBssInfo == NULL)
			DBGLOG(RSN, ERROR, "prAisBssInfo is NULL");
		else if (prAisBssInfo->eConnectionState ==
					MEDIA_STATE_CONNECTED)
#if (CFG_SUPPORT_CONN_LOG == 1)
			connLogRxDeauthPrint(prAdapter,
				ucBssIdx);
#endif
			saaSendDisconnectMsgHandler(prAdapter,
					prAisBssInfo->prStaRecOfAP,
					prAisBssInfo,
					FRM_DEAUTH);

		return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query timer.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnStartSaQueryTimer(struct ADAPTER *prAdapter,
			  uintptr_t ulParamPtr)
{
	struct BSS_INFO *prBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint16_t u2PayloadLen;
	uint8_t *pucTmp = NULL;
	uint8_t ucTransId[ACTION_SA_QUERY_TR_ID_LEN];
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	prBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prBssSpecInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	DBGLOG(RSN, INFO, "MFP: Start Sa Query\n");

	if (prBssInfo->prStaRecOfAP == NULL) {
		DBGLOG(RSN, INFO, "MFP: unassociated AP!\n");
		return;
	}

	if (!EQUAL_MAC_ADDR(prBssSpecInfo->aucSaQueryBSSID,
			    prBssInfo->aucBSSID)) {
		DBGLOG(RSN, INFO,
			"MFP: Sa Query AP[" MACSTR
			"] associated AP[" MACSTR "]!\n",
			MAC2STR(prBssSpecInfo->aucSaQueryBSSID),
			MAC2STR(prBssInfo->aucBSSID));
		rsnStopSaQuery(prAdapter, ucBssIndex);
		return;
	}

	if (prBssSpecInfo->u4SaQueryCount > 0
	    && rsnCheckSaQueryTimeout(prAdapter,
	    ucBssIndex)) {
		DBGLOG(RSN, INFO, "MFP: u4SaQueryCount count =%d\n",
		       prBssSpecInfo->u4SaQueryCount);
		return;
	}

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((uintptr_t)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_REQUEST;

	if (prBssSpecInfo->u4SaQueryCount == 0)
		GET_CURRENT_SYSTIME(&prBssSpecInfo->u4SaQueryStart);

	if (prBssSpecInfo->u4SaQueryCount) {
		pucTmp = kalMemAlloc(prBssSpecInfo->u4SaQueryCount *
				     ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
		if (!pucTmp) {
			DBGLOG(RSN, INFO,
			       "MFP: Fail to alloc tmp buffer for backup sa query id\n");
			cnmMgtPktFree(prAdapter, prMsduInfo);
			return;
		}
		kalMemCopy(pucTmp, prBssSpecInfo->pucSaQueryTransId,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
	}

	kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
		   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);

	ucTransId[0] = (uint8_t) (kalRandomNumber() & 0xFF);
	ucTransId[1] = (uint8_t) (kalRandomNumber() & 0xFF);

	kalMemCopy(prTxFrame->ucTransId, ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	prBssSpecInfo->u4SaQueryCount++;

	prBssSpecInfo->pucSaQueryTransId =
	    kalMemAlloc(prBssSpecInfo->u4SaQueryCount *
			ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
	if (!prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(pucTmp, VIR_MEM_TYPE,
			   (prBssSpecInfo->u4SaQueryCount - 1) *
			   ACTION_SA_QUERY_TR_ID_LEN);
		DBGLOG(RSN, INFO,
		       "MFP: Fail to alloc buffer for sa query id list\n");
		cnmMgtPktFree(prAdapter, prMsduInfo);
		return;
	}

	if (pucTmp) {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, pucTmp,
			   (prBssSpecInfo->u4SaQueryCount - 1) *
			   ACTION_SA_QUERY_TR_ID_LEN);
		kalMemCopy(
			&prBssSpecInfo->pucSaQueryTransId[
				(prBssSpecInfo->u4SaQueryCount - 1)
				* ACTION_SA_QUERY_TR_ID_LEN],
				ucTransId, ACTION_SA_QUERY_TR_ID_LEN);
		kalMemFree(pucTmp, VIR_MEM_TYPE,
			   (prBssSpecInfo->u4SaQueryCount -
			    1) * ACTION_SA_QUERY_TR_ID_LEN);
	} else {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, ucTransId,
			   ACTION_SA_QUERY_TR_ID_LEN);
	}

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "Set SA Query timer %d (%d Tu)",
	       prBssSpecInfo->u4SaQueryCount, SA_QUERY_TIMEOUT);

	cnmTimerStartTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer,
			   TU_TO_MSEC(SA_QUERY_TIMEOUT));
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnStartSaQuery(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

	prBssSpecInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIdx);

	if (prBssSpecInfo->u4SaQueryCount == 0)
		rsnStartSaQueryTimer(prAdapter, (uintptr_t) ucBssIdx);
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to stop the 802.11w sa query.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnStopSaQuery(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

	prBssSpecInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIdx);

	DBGLOG(RSN, TRACE, "MFP: Stop Sa Query\n");

	cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);

	if (prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
	}

	prBssSpecInfo->u4SaQueryCount = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnSaQueryRequest(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame = NULL;
	uint16_t u2PayloadLen;
	struct STA_RECORD *prStaRec;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint8_t ucBssIndex = secGetBssIdxByRfb(prAdapter,
		prSwRfb);
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;

	prBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	if (!prSwRfb)
		return;

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec) {
		DBGLOG(RSN, WARN,
		       "Received a SA Query from "MACSTR
		       " BssIndex[%d] w/o corresponding staRec\n",
		       ucBssIndex, MAC2STR(prRxFrame->aucSrcAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Request from DA[" MACSTR
	       "] SA[" MACSTR "] BSSID[" MACSTR "]\n",
	       MAC2STR(prRxFrame->aucDestAddr),
	       MAC2STR(prRxFrame->aucSrcAddr),
	       MAC2STR(prRxFrame->aucBSSID));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo,
		ucBssIndex) ==
	    MEDIA_STATE_DISCONNECTED) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Ignore SA Query Request from unassociated STA "
		       MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	if (!rsnCheckCombackBssDesc(prAdapter, prStaRec, ucBssIndex)) {
		DBGLOG(RSN, INFO,
			"Ignore SA Query Request from non-targeted AP "
			MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Sending SA Query Response to " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((uintptr_t)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId,
		   ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	nicTxConfigPktControlFlag(prMsduInfo,
			MSDU_CONTROL_FLAG_FORCE_LINK |
			MSDU_CONTROL_FLAG_DIS_MAT,
			TRUE);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnSaQueryAction(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	uint32_t i;
	uint8_t ucBssIndex = secGetBssIdxByRfb(prAdapter,
		prSwRfb);

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (!prStaRec)
		return;

	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
		       (uintptr_t)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Unexpected SA Query Action %d\n",
		       prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Response from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */

	for (i = 0; i < prBssSpecInfo->u4SaQueryCount; i++) {
		if (kalMemCmp(prBssSpecInfo->pucSaQueryTransId +
			      i * ACTION_SA_QUERY_TR_ID_LEN,
			      prRxFrame->ucTransId,
			      ACTION_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= prBssSpecInfo->u4SaQueryCount) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: No matching SA Query transaction identifier found\n");
		return;
	}

	DBGLOG(RSN, INFO, "Reply to pending SA Query received\n");

	rsnStopSaQuery(prAdapter, ucBssIndex);
}

uint8_t rsnCheckBipGmac(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb) {
	struct STA_RECORD *prStaRec;
	struct WLAN_DEAUTH_FRAME_WITH_MIC *prDeauthMICFrame;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo = NULL;
	uint8_t nounce[12];
	uint8_t *npos;
	uint8_t aad_gmac[48];
	uint8_t *prAddr;

	if (prSwRfb->u2PacketLen < sizeof(struct WLAN_DEAUTH_FRAME_WITH_MIC))
		return FALSE;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec) {
		DBGLOG(RSN, ERROR, "prStaRec is NULL!");
		return FALSE;
	}
	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter,
		prStaRec->ucBssIndex);
	prDeauthMICFrame =
		(struct WLAN_DEAUTH_FRAME_WITH_MIC *) prSwRfb->pvHeader;

	DBGLOG(SAA, INFO,
		"BIP checking for Rx Deauth/Disassoc frame ,DA[" MACSTR
		"] SA[" MACSTR "] BSSID[" MACSTR "] ReasonCode[0x%x]\n",
		MAC2STR(prDeauthMICFrame->aucDestAddr),
		MAC2STR(prDeauthMICFrame->aucSrcAddr),
		MAC2STR(prDeauthMICFrame->aucBSSID),
		prDeauthMICFrame->u2ReasonCode);

	/* BIP part 1: replay protection checking
	 * If IPN in deauth >= saved IPN -> pass
	 * If IPN in deauth < saved IPN -> fail
	 */
	DBGLOG(RSN, INFO, "Dump IPN from deauth/disassoc frame and saved IPN");
	DBGLOG_MEM8(RSN, INFO, prDeauthMICFrame->aucIPN, 6);
	DBGLOG_MEM8(RSN, INFO, prAisSpecBssInfo->aucIPN, 6);
	if (kalMemCmp(prDeauthMICFrame->aucIPN,
		prAisSpecBssInfo->aucIPN, 6) < 0) {
		DBGLOG(RSN, WARN, "replay protection checking failure");
		return FALSE;
	}

	/* BIP part2: MIC content checking */
	/* nounce = A2 + IPN in BE order */
	kalMemCopy(nounce, prDeauthMICFrame->aucSrcAddr, 6);
	npos = nounce + 6;
	*npos++ = prDeauthMICFrame->aucIPN[5];
	*npos++ = prDeauthMICFrame->aucIPN[4];
	*npos++ = prDeauthMICFrame->aucIPN[3];
	*npos++ = prDeauthMICFrame->aucIPN[2];
	*npos++ = prDeauthMICFrame->aucIPN[1];
	*npos++ = prDeauthMICFrame->aucIPN[0];
	DBGLOG_MEM8(RSN, INFO, nounce, 12);

	/* AAD-GMAC */
	/* AAD || Management frame body || MME (MIC field masked to 0) */
	/* copy FC and skip duration */
	kalMemCopy(aad_gmac, &prDeauthMICFrame->u2FrameCtrl, 2);
	/* copy A1,A2,A3 and skip SEQ */
	prAddr = (uint8_t *) prDeauthMICFrame->aucDestAddr;
	kalMemCopy(aad_gmac+2, prAddr, 18);
	/* copy reason code, MIC tag/legnth, keyID, IPN */
	kalMemCopy(aad_gmac+20, &prDeauthMICFrame->u2ReasonCode, 12);
	kalMemSet(aad_gmac+32, 0, 16);
	DBGLOG(RSN, INFO, "Dump AAD for GMAC");
	DBGLOG_MEM8(RSN, INFO, aad_gmac, 48);

	DBGLOG(RSN, INFO, "Dump IGTK for GMAC");
	DBGLOG_MEM8(RSN, INFO, prAisSpecBssInfo->aucIGTK, 32);

	if (aes_gcm_ad_impl(aad_gmac, sizeof(aad_gmac),
		prAisSpecBssInfo->aucIGTK,
		sizeof(prAisSpecBssInfo->aucIGTK),
		nounce, sizeof(nounce),
		prDeauthMICFrame->aucMIC) < 0) {
		DBGLOG(RSN, WARN, "aes_gcm_ad fail");
		return FALSE;
	}

	return TRUE;
}

void rsnUpdateCombackBssDesc(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	uint8_t i;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	if (!prAisFsmInfo || !prAisSpecBssInfo)
		return;

	for (i = 0; i < MLD_LINK_MAX; i++)
		prAisSpecBssInfo->aprTargetComebackBssDesc[i] =
			aisGetLinkBssDesc(prAisFsmInfo, i);
}

uint8_t rsnCheckCombackBssDesc(struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec, uint8_t ucBssIndex)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo = NULL;
	uint8_t i;

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	if (!prAisSpecBssInfo)
		return FALSE;

	for (i = 0; i < MLD_LINK_MAX; i++) {
		if (!prAisSpecBssInfo->aprTargetComebackBssDesc[i])
			continue;

		if (!prStaRec)
			return TRUE;

		if (EQUAL_MAC_ADDR(prStaRec->aucMacAddr,
		    prAisSpecBssInfo->aprTargetComebackBssDesc[i]->aucBSSID))
			return TRUE;
	}

	return FALSE;
}

void rsnResetCombackBssDesc(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{

	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo = NULL;

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	if (prAisSpecBssInfo) {
		kalMemZero(prAisSpecBssInfo->aprTargetComebackBssDesc,
			sizeof(prAisSpecBssInfo->aprTargetComebackBssDesc));
	}
}

#endif

static u_int8_t rsnCheckWpaRsnInfo(struct BSS_INFO *prBss,
				   struct BSS_DESC *prBssDesc,
				   struct RSN_INFO *prWpaRsnInfo)
{
	uint32_t i = 0;

	if (prWpaRsnInfo->u4GroupKeyCipherSuite !=
	    prBss->u4RsnSelectedGroupCipher) {
		DBGLOG(RSN, INFO,
		       "GroupCipherSuite change, old=0x%04x, new=0x%04x\n",
		       prBss->u4RsnSelectedGroupCipher,
		       prWpaRsnInfo->u4GroupKeyCipherSuite);
		return TRUE;
	}

	/* check akm */
	if (prBssDesc->ucIsAdaptive11r &&
	    rsnKeyMgmtFT(prBss->u4RsnSelectedAKMSuite))
		return FALSE;

	for (; i < prWpaRsnInfo->u4AuthKeyMgtSuiteCount; i++)
		if (prBss->u4RsnSelectedAKMSuite ==
		    prWpaRsnInfo->au4AuthKeyMgtSuite[i])
			break;

	if (i == prWpaRsnInfo->u4AuthKeyMgtSuiteCount) {
		DBGLOG(RSN, INFO,
		       "KeyMgmt change, not find 0x%04x in new beacon\n",
		       prBss->u4RsnSelectedAKMSuite);
		return TRUE;
	}

	for (i = 0; i < prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount; i++)
		if (prBss->u4RsnSelectedPairwiseCipher ==
		    prWpaRsnInfo->au4PairwiseKeyCipherSuite[i])
			break;
	if (i == prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount) {
		DBGLOG(RSN, INFO,
		       "Pairwise Cipher change, not find 0x%04x in new beacon\n",
		       prBss->u4RsnSelectedPairwiseCipher);
		return TRUE;
	}

	return FALSE;
}

u_int8_t rsnCheckSecurityModeChanged(
			struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo,
			struct BSS_DESC *prBssDesc)
{
	uint8_t ucBssIdx = 0;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	struct GL_WPA_INFO *prWpaInfo;

	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "Empty prBssInfo\n");
		return FALSE;
	}
	ucBssIdx = prBssInfo->ucBssIndex;
	eAuthMode = aisGetAuthMode(prAdapter, ucBssIdx);
	prWpaInfo = aisGetWpaInfo(prAdapter, ucBssIdx);

	switch (eAuthMode) {
	case AUTH_MODE_OPEN: /* original is open system */
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) &&
		    !prWpaInfo->fgPrivacyInvoke &&
		    !secIsWepBss(prAdapter, prBssInfo)
#if CFG_SUPPORT_WPS2
		    /* Don't check while WPS is in process */
		    && !aisGetConnSettings(prAdapter, ucBssIdx)->fgWpsActive
#endif
		) {
			DBGLOG(RSN, INFO, "security change, open->privacy\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_SHARED:	/* original is WEP */
	case AUTH_MODE_AUTO_SWITCH:
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) == 0) {
			DBGLOG(RSN, INFO, "security change, WEP->open\n");
			return TRUE;
		} else if (prBssDesc->fgIERSN || prBssDesc->fgIEWPA) {
			DBGLOG(RSN, INFO, "security change, WEP->WPA/WPA2\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_WPA:	/*original is WPA */
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA_NONE:
		if (prBssDesc->fgIEWPA)
			return rsnCheckWpaRsnInfo(prBssInfo, prBssDesc,
						&prBssDesc->rWPAInfo);
		DBGLOG(RSN, INFO, "security change, WPA->%s\n",
		       prBssDesc->fgIERSN ? "WPA2" :
		       (prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ?
				"WEP" : "OPEN"));
		return TRUE;
	case AUTH_MODE_WPA2:	/*original is WPA2 */
	case AUTH_MODE_WPA2_PSK:
	case AUTH_MODE_WPA2_FT:
	case AUTH_MODE_WPA2_FT_PSK:
	case AUTH_MODE_WPA3_SAE:
		if (prBssDesc->fgIERSN)
			return rsnCheckWpaRsnInfo(prBssInfo, prBssDesc,
						&prBssDesc->rRSNInfo);
		DBGLOG(RSN, INFO, "security change, WPA2->%s\n",
		       prBssDesc->fgIEWPA ? "WPA" :
		       (prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ?
				"WEP" : "OPEN"));
		return TRUE;
	default:
		DBGLOG(RSN, WARN, "unknowned eAuthMode=%d\n", eAuthMode);
		break;
	}
	return FALSE;
}

#if CFG_SUPPORT_AAA
#define WPS_DEV_OUI_WFA                 0x0050f204
#define ATTR_RESPONSE_TYPE              0x103b

#define ATTR_VERSION                    0x104a
#define ATTR_VENDOR_EXT                 0x1049
#define WPS_VENDOR_ID_WFA               14122

void rsnGenerateWSCIEForAssocRsp(struct ADAPTER *prAdapter,
				 struct MSDU_INFO *prMsduInfo)
{
	struct WIFI_VAR *prWifiVar = NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *)NULL;
	uint16_t u2IELen = 0;
	uint8_t ucType = 3;

	prWifiVar = &(prAdapter->rWifiVar);

	DBGLOG(RSN, TRACE, "WPS: Building WPS IE for (Re)Association Response");
	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	if (!prP2pBssInfo) {
		DBGLOG(RSN, ERROR, "prP2pBssInfo is null\n");
		return;
	}

	if (prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	if (prWifiVar->
		prP2PConnSettings[prP2pBssInfo->u4PrivateData]->fgIsWPSMode) {
		DBGLOG(RSN, INFO, "WPS: Building WPS IE by beacon");
		ucType = 0;
	}

	u2IELen = kalP2PCalWSC_IELen(prAdapter->prGlueInfo, ucType,
				     (uint8_t) prP2pBssInfo->u4PrivateData);

	kalP2PGenWSC_IE(prAdapter->prGlueInfo,
			ucType,
			(uint8_t *) ((uintptr_t) prMsduInfo->prPacket +
				  (uintptr_t) prMsduInfo->u2FrameLength),
			(uint8_t) prP2pBssInfo->u4PrivateData);
	prMsduInfo->u2FrameLength += (uint16_t) kalP2PCalWSC_IELen(
					prAdapter->prGlueInfo, ucType,
					(uint8_t) prP2pBssInfo->u4PrivateData);
}

#endif

#if CFG_SUPPORT_802_11W
/* AP PMF */
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to validate setting if PMF connection capable
 *    or not. If AP MFPC=1, and STA MFPC=1, we let this as PMF connection
 *
 *
 * \return status code
 */
/*----------------------------------------------------------------------------*/
uint16_t rsnPmfCapableValidation(struct ADAPTER
				 *prAdapter, struct BSS_INFO *prBssInfo,
				 struct STA_RECORD *prStaRec)
{
	u_int8_t selfMfpc, selfMfpr, peerMfpc, peerMfpr;

	selfMfpc = prBssInfo->rApPmfCfg.fgMfpc;
	selfMfpr = prBssInfo->rApPmfCfg.fgMfpr;
	peerMfpc = prStaRec->rPmfCfg.fgMfpc;
	peerMfpr = prStaRec->rPmfCfg.fgMfpr;

	DBGLOG(RSN, INFO,
	       "AP mfpc:%d, mfpr:%d / STA mfpc:%d, mfpr:%d\n",
	       selfMfpc, selfMfpr, peerMfpc, peerMfpr);

	if ((selfMfpc == TRUE) && (peerMfpc == FALSE)) {
		if ((selfMfpr == TRUE) && (peerMfpr == FALSE)) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case 4\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}

		if (peerMfpr == TRUE) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case 7\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}

		if (rsnKeyMgmtSae(prBssInfo->u4RsnSelectedAKMSuite) &&
			prStaRec->rPmfCfg.fgSaeRequireMfp) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case sae_require_mfp\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}
	}

	if ((selfMfpc == TRUE) && (peerMfpc == TRUE)) {
		DBGLOG(RSN, ERROR, "PMF Connection\n");
		prStaRec->rPmfCfg.fgApplyPmf = TRUE;
	}

	return STATUS_CODE_SUCCESSFUL;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate TIMEOUT INTERVAL IE
 *     for association resp.
 *     Add Timeout interval IE (56) when PMF invalid association.
 *
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void rsnPmfGenerateTimeoutIE(struct ADAPTER *prAdapter,
			     struct MSDU_INFO *prMsduInfo)
{
	struct IE_TIMEOUT_INTERVAL *prTimeout;
	struct STA_RECORD *prStaRec = NULL;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	prTimeout = (struct IE_TIMEOUT_INTERVAL *)
	    (((uint8_t *) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* only when PMF connection, and association error code is 30 */
	if ((rsnCheckBipKeyInstalled(prAdapter, prStaRec) == TRUE)
	    &&
	    (prStaRec->u2StatusCode ==
	     STATUS_CODE_ASSOC_REJECTED_TEMPORARILY)) {

		DBGLOG(RSN, INFO, "rsnPmfGenerateTimeoutIE TRUE\n");
		prTimeout->ucId = ELEM_ID_TIMEOUT_INTERVAL;
		prTimeout->ucLength = ELEM_MAX_LEN_TIMEOUT_IE;
		prTimeout->ucType = IE_TIMEOUT_INTERVAL_TYPE_ASSOC_COMEBACK;
		prTimeout->u4Value = 1 << 10;
		prMsduInfo->u2FrameLength += IE_SIZE(prTimeout);
	}
}

#if CFG_ENABLE_WIFI_DIRECT
/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to check the Sa query timeout.
 * check if total retry time is greater than 1000ms
 *
 * \retval 1: retry max timeout. 0: not timeout
 * \note
 *      Called by: AAA module, Handle by Sa Query timeout
 */
/*----------------------------------------------------------------------------*/
uint8_t rsnApCheckSaQueryTimeout(struct ADAPTER *prAdapter,
				 struct STA_RECORD *prStaRec)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	struct BSS_INFO *prBssInfo;
	OS_SYSTIME rCurrentTime;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	if (CHECK_FOR_TIMEOUT(rCurrentTime, prStaRec->rPmfCfg.u4SAQueryStart,
			      TU_TO_MSEC(prWifiVar->u4SaQueryMaxTimeout))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		/* XXX PMF TODO how to report STA REC disconnect?? */
		/* when SAQ retry count timeout, clear this STA */
		prStaRec->rPmfCfg.ucSAQueryTimedOut = 1;
		prStaRec->rPmfCfg.u2TransactionID = 0;
		prStaRec->rPmfCfg.u4SAQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prStaRec->rPmfCfg.rSAQueryTimer);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prStaRec->ucBssIndex);

		if (!prBssInfo) {
			DBGLOG(RSN, ERROR, "prBssInfo is null\n");
			return 1;
		}

		/* refer to p2pRoleFsmRunEventRxDeauthentication */
		if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			if (bssRemoveClient(prAdapter, prBssInfo, prStaRec)) {
				/* Indicate disconnect to Host. */
				p2pFuncDisconnect(prAdapter, prBssInfo,
					prStaRec, FALSE,
					REASON_CODE_DEAUTH_LEAVING_BSS,
					MAC_FRAME_DEAUTH, TRUE);
				/* Deactive BSS if PWR is IDLE and no peer */
				if (IS_NET_PWR_STATE_IDLE(prAdapter,
					prBssInfo->ucBssIndex)
				    &&
				    (bssGetClientCount(prAdapter, prBssInfo)
					== 0)) {
					/* All Peer disconnected !!
					 * Stop BSS now!!
					 */
					p2pFuncStopComplete(prAdapter,
						prBssInfo);
				}
			}
		}

		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query timer.
 *    This routine is triggered every 201ms, and every time enter function,
 *    check max timeout
 *
 * \note
 *      Called by: AAA module, Handle TX SAQ request
 */
/*----------------------------------------------------------------------------*/
void rsnApStartSaQueryTimer(struct ADAPTER *prAdapter,
			    uintptr_t ulParamPtr)
{
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) ulParamPtr;
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint16_t u2PayloadLen;

	DBGLOG(RSN, INFO, "MFP: AP Start Sa Query timer\n");

	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return;
	}

	if (prStaRec->rPmfCfg.u4SAQueryCount > 0 &&
	    rsnApCheckSaQueryTimeout(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO,
		       "MFP: retry max timeout, u4SaQueryCount count =%d\n",
		       prStaRec->rPmfCfg.u4SAQueryCount);
		return;
	}

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((uintptr_t)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_REQUEST;

	if (prStaRec->rPmfCfg.u4SAQueryCount == 0)
		GET_CURRENT_SYSTIME(&prStaRec->rPmfCfg.u4SAQueryStart);

	/* if retry, transcation id ++ */
	if (prStaRec->rPmfCfg.u4SAQueryCount) {
		prStaRec->rPmfCfg.u2TransactionID++;
	} else {
		/* if first SAQ request, random pick transaction id */
		prStaRec->rPmfCfg.u2TransactionID =
		    (uint16_t) (kalRandomNumber() & 0xFFFF);
	}

	DBGLOG(RSN, INFO, "SAQ transaction id:%d\n",
	       prStaRec->rPmfCfg.u2TransactionID);

	/* trnsform U16 to U8 array */
	prTxFrame->ucTransId[0] = ((prStaRec->rPmfCfg.u2TransactionID
				& 0xff00) >> 8);
	prTxFrame->ucTransId[1] = ((prStaRec->rPmfCfg.u2TransactionID
				& 0x00ff) >> 0);

	prStaRec->rPmfCfg.u4SAQueryCount++;

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "AP Set SA Query timer %d (%d Tu)\n",
		prStaRec->rPmfCfg.u4SAQueryCount,
		prAdapter->rWifiVar.u4SaQueryRetryTimeout);

	cnmTimerStartTimer(prAdapter,
		&prStaRec->rPmfCfg.rSAQueryTimer,
		TU_TO_MSEC(prAdapter->rWifiVar.u4SaQueryRetryTimeout));
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w TX SA query.
 *
 *
 * \note
 *      Called by: AAA module, Handle Tx action frame request
 */
/*----------------------------------------------------------------------------*/
void rsnApStartSaQuery(struct ADAPTER *prAdapter,
		       struct STA_RECORD *prStaRec)
{
	if (!prStaRec)
		return;

	DBGLOG(RSN, INFO, "sta=%u %u, count=%u timeout=%u\n",
		prStaRec->ucIndex, prStaRec->ucWlanIndex,
		prStaRec->rPmfCfg.u4SAQueryCount,
		prAdapter->rWifiVar.u4SaQueryMaxTimeout);

	if (prStaRec->rPmfCfg.u4SAQueryCount > 0)
		rsnApCheckSaQueryTimeout(prAdapter, prStaRec);

	if (prStaRec->rPmfCfg.u4SAQueryCount == 0)
		rsnApStartSaQueryTimer(prAdapter, (uintptr_t)prStaRec);
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to stop the 802.11w SA query.
 *
 *
 * \note
 *      Called by: AAA module, stop TX SAQ if receive correct SAQ response
 */
/*----------------------------------------------------------------------------*/
void rsnApStopSaQuery(struct ADAPTER *prAdapter,
		      struct STA_RECORD *prStaRec)
{
	cnmTimerStopTimer(prAdapter, &prStaRec->rPmfCfg.rSAQueryTimer);
	prStaRec->rPmfCfg.u2TransactionID = 0;
	prStaRec->rPmfCfg.u4SAQueryCount = 0;
	prStaRec->rPmfCfg.ucSAQueryTimedOut = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AAA module, Handle Rx action request
 */
/*----------------------------------------------------------------------------*/
void rsnApSaQueryRequest(struct ADAPTER *prAdapter,
			 struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame = NULL;
	uint16_t u2PayloadLen;
	struct STA_RECORD *prStaRec;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;

	if (!prSwRfb)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)		/* Todo:: for not AIS check */
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "prBssInfo is null\n");
		return;
	}

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: AP Received SA Query Request from " MACSTR
	       "\n", MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (!rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: AP Ignore SA Query Request non-PMF STA "
		       MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Sending SA Query Response to " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	/* drop cipher mismatch */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		if (prSwRfb->fgIsCipherMS ||
			prSwRfb->fgIsCipherLenMS) {
			/* if cipher mismatch, or incorrect encrypt,
			 * just drop
			 */
			DBGLOG(RSN, ERROR, "drop SAQ req CM/CLM=1\n");
			return;
		}
	}

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((uintptr_t)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		DBGLOG(RSN, INFO, "AP SAQ resp set FC PF bit\n");
	}
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId,
		   ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AAA module, Handle Rx action request
 */
/*----------------------------------------------------------------------------*/
void rsnApSaQueryAction(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	struct ACTION_SA_QUERY_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2SwapTrID;

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (prStaRec == NULL) {
		DBGLOG(RSN, INFO, "rsnApSaQueryAction: prStaRec is NULL");
		return;
	}

	DBGLOG(RSN, TRACE,
	       "AP PMF SAQ action enter from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));
	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
		       (uintptr_t)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnApSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Unexpected SA Query Action %d\n",
		       prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Response from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */
	/* transform to network byte order */
	u2SwapTrID = HTONS(prStaRec->rPmfCfg.u2TransactionID);
	if (kalMemCmp((uint8_t *) &u2SwapTrID, prRxFrame->ucTransId,
		      ACTION_SA_QUERY_TR_ID_LEN) == 0) {
		DBGLOG(RSN, INFO, "AP Reply to SA Query received\n");
		rsnApStopSaQuery(prAdapter, prStaRec);
	} else {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: AP No matching SA Query transaction identifier found\n");
	}

}

#endif /* CFG_SUPPORT_802_11W */
#endif /* CFG_ENABLE_WIFI_DIRECT */

#if CFG_SUPPORT_PASSPOINT
u_int8_t rsnParseOsenIE(struct ADAPTER *prAdapter,
			struct IE_WFA_OSEN *prInfoElem,
			struct RSN_INFO *prOsenInfo)
{
	uint32_t i;
	int32_t u4RemainRsnIeLen;
	uint16_t u2Version = 0;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = RSN_CIPHER_SUITE_CCMP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;

	cp = ((uint8_t *) prInfoElem) + 6;
	u4RemainRsnIeLen = (int32_t) prInfoElem->ucLength - 4;
	do {
		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainRsnIeLen < 4) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainRsnIeLen -= 4;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in pairwise cipher suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %d, Remain %u, Cnt %d GS %x)\n",
			       prInfoElem->ucLength, u4RemainRsnIeLen,
			       u2PairSuiteCount, u4GroupSuite);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite
		 * Count field.
		 */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite
		 * List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the RSN u2Capabilities field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in RSN capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
	} while (FALSE);

	/* Save the RSN information for the BSS. */
	prOsenInfo->ucElemId = ELEM_ID_VENDOR;

	prOsenInfo->u2Version = 0;

	prOsenInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, TRACE,
	       "RSN: version %d, group key cipher suite 0x%x\n",
	       u2Version, SWAP32(u4GroupSuite));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prOsenInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
				&prOsenInfo->au4PairwiseKeyCipherSuite[i]);
			pucPairSuite += 4;

			DBGLOG(RSN, TRACE,
			  "RSN: pairwise key cipher suite [%d]: 0x%x\n", i,
			  SWAP32(prOsenInfo->au4PairwiseKeyCipherSuite[i]));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present. Use the default chipher suite for RSN: CCMP
		 */

		prOsenInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prOsenInfo->au4PairwiseKeyCipherSuite[0] =
		    RSN_CIPHER_SUITE_CCMP;

		DBGLOG(RSN, WARN,
		       "No Pairwise Cipher Suite found, using default (CCMP)\n");
	}

	if (pucAuthSuite) {
		/* The information about the authentication
		 * and key management suites is present.
		 */

		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prOsenInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prOsenInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, TRACE, "RSN: AKM suite [%d]: 0x%x\n", i,
				SWAP32(prOsenInfo->au4AuthKeyMgtSuite[i]));
		}
	} else {
		/* The information about the authentication and
		 * key management suites is not present.
		 * Use the default AKM suite for RSN.
		 */
		prOsenInfo->u4AuthKeyMgtSuiteCount = 1;
		prOsenInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;

		DBGLOG(RSN, WARN, "No AKM found, using default (802.1X)\n");
	}

	prOsenInfo->u2RsnCap = u2Cap;
#if CFG_SUPPORT_802_11W
	prOsenInfo->fgRsnCapPresent = TRUE;
#endif
	DBGLOG(RSN, TRACE, "RSN cap: 0x%04x\n", prOsenInfo->u2RsnCap);

	return TRUE;
}
#endif /* CFG_SUPPORT_PASSPOINT */

uint32_t rsnCalculateFTIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			     struct STA_RECORD *prStaRec)
{
	/* Use R0 with auth, R1 with assoc */
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx, AIS_FT_R1);

	if (!prFtIEs || !prFtIEs->prFTIE || !prStaRec ||
	    !rsnIsFtOverTheAir(prAdapter, ucBssIdx, prStaRec->ucIndex))
		return 0;
	return IE_SIZE(prFtIEs->prFTIE);
}

void rsnGenerateFTIE(struct ADAPTER *prAdapter,
		     struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint32_t ucFtIeSize = 0;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;
	/* Use R0 with auth, R1 with assoc */
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx, AIS_FT_R1);

	if (!prFtIEs || !prFtIEs->prFTIE ||
	    !rsnIsFtOverTheAir(prAdapter, ucBssIdx, prMsduInfo->ucStaRecIndex))
		return;
	ucFtIeSize = IE_SIZE(prFtIEs->prFTIE);
	prMsduInfo->u2FrameLength += ucFtIeSize;
	kalMemCopy(pucBuffer, prFtIEs->prFTIE, ucFtIeSize);
	DBGDUMP_MEM8(SAA, INFO, "FT: Generate FTIE\n", pucBuffer, ucFtIeSize);
}

u_int8_t rsnIsFtOverTheAir(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
	uint8_t ucStaRecIdx)
{
	struct STA_RECORD *prStaRec;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIdx);
	if (IS_BSS_INDEX_VALID(ucBssIdx) &&
	    IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) &&
	    prStaRec && prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_FT)
		return TRUE;

	return FALSE;
}

void rsnReqDumpWTBL(struct ADAPTER *prAdapter,
	enum ENUM_FW_DUMP eType, uint8_t ucBssIndex)
{
	struct MSG_FW_DUMP *prFwDumpMsg;

	if (!IS_FEATURE_ENABLED(prAdapter->rWifiVar.fgRxIcvErrDbg))
		return;

	if (rsnFwDumpIsLimited(prAdapter))
		return;

	prFwDumpMsg = (struct MSG_FW_DUMP *) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, sizeof(struct MSG_FW_DUMP));

	if (!prFwDumpMsg) {
		DBGLOG(RSN, WARN, "cnmMemAlloc Fail\n");
		return;
	}

	prFwDumpMsg->rMsgHdr.eMsgId = MID_RSN_FW_DUMP;
	prFwDumpMsg->eType = eType;
	prFwDumpMsg->ucBssIndex = ucBssIndex;

	DBGLOG(RSN, TRACE,
		"Request WTBL dump for ICV error!\n");
	DBGLOG(RSN, LOUD,
		"Send Msg eMsgId:%u eType:%u ucBssIndex:%u\n",
		prFwDumpMsg->rMsgHdr.eMsgId,
		prFwDumpMsg->eType,
		prFwDumpMsg->ucBssIndex);
	mboxSendMsg(prAdapter, MBOX_ID_0,
		(struct MSG_HDR *) prFwDumpMsg, MSG_SEND_METHOD_BUF);
}

void rsnTriggerDumpWTBL(struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr)
{
	struct MSG_FW_DUMP *prFwDumpMsg;

	prFwDumpMsg = (struct MSG_FW_DUMP *) prMsgHdr;
	DBGLOG(RSN, LOUD,
		"Handle Msg eMsgId:%u eType:%u ucBssIndex:%u\n",
		prFwDumpMsg->rMsgHdr.eMsgId,
		prFwDumpMsg->eType,
		prFwDumpMsg->ucBssIndex);
	switch (prFwDumpMsg->eType) {
	case FW_DUMP_WTBL:
		rsnDumpWTBL(prAdapter);
		break;
	default:
		DBGLOG(RSN, WARN, "Invalid eType:%u\n",
			prFwDumpMsg->eType);
		break;
	}
	cnmMemFree(prAdapter, prMsgHdr);
}

void rsnDumpWTBL(struct ADAPTER *prAdapter)
{
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};
	uint8_t cmd[30] = {0};
	uint8_t strLen = 0;
	uint32_t strOutLen = 0;

	strLen = kalSnprintf(cmd, sizeof(cmd),
			"icvErrDumpWTBL");
	DBGLOG(RSN, TRACE,
		"Trigger WTBL Dump to FW: %s, strlen=%d", cmd, strLen);
	rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
	rChipConfigInfo.u2MsgSize = strLen;
	kalStrnCpy(rChipConfigInfo.aucCmd, cmd, strLen);
	wlanSetChipConfig(prAdapter, &rChipConfigInfo,
			sizeof(rChipConfigInfo), &strOutLen, FALSE);
}

bool rsnFwDumpIsLimited(struct ADAPTER *prAdapter)
{
	if (CHECK_FOR_TIMEOUT(kalGetTimeTick(),
				prAdapter->rRsnFwDumpTime,
				SEC_TO_SYSTIME(1))) {
		GET_CURRENT_SYSTIME(
			&prAdapter->rRsnFwDumpTime);
		return FALSE;
	}
	return TRUE;
}

#if (CFG_SUPPORT_SAP_BCN_PROT == 1)
uint32_t rsnCalculateMMIELen(struct ADAPTER *prAdapter,
			       uint8_t ucBssIndex, struct STA_RECORD *prStaRec)
{
#define LEN_PMF_MMIE_TRUNCATE		16
#define LEN_PMF_MMIE			24

	struct BSS_INFO *prBssInfo;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;
	uint32_t u4Length = 0;

	if (!prAdapter)
		return 0;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo || !IS_BSS_APGO(prBssInfo))
		return 0;

	prP2pSpecificBssInfo =
		prAdapter->rWifiVar
			.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];
	if (!prP2pSpecificBssInfo || !prP2pSpecificBssInfo->fgBcnProtEn)
		return 0;

	switch (prBssInfo->u4RsnSelectedGroupMgmtCipher) {
	case RSN_CIPHER_SUITE_BIP_CMAC_128:
		u4Length = LEN_PMF_MMIE_TRUNCATE;
		break;
	default:
		u4Length = LEN_PMF_MMIE;
		break;
	}

	return u4Length;
}

void rsnGenerateMMIE(struct ADAPTER *prAdapter,
		     struct MSDU_INFO *prMsduInfo)
{
	struct BSS_INFO *prBssInfo;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;
	struct MMIE_ELEM *prMmie;
	uint32_t u4Length = 0;

	if (!prAdapter)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
	if (!prBssInfo || !IS_BSS_APGO(prBssInfo))
		return;

	prP2pSpecificBssInfo =
		prAdapter->rWifiVar
			.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];
	if (!prP2pSpecificBssInfo || !prP2pSpecificBssInfo->fgBcnProtEn)
		return;

	u4Length = rsnCalculateMMIELen(prAdapter, prMsduInfo->ucBssIndex,
				       NULL);
	if (u4Length == 0)
		return;

	prMmie = (struct MMIE_ELEM *)((uintptr_t)prMsduInfo->prPacket +
				      (uintptr_t)prMsduInfo->u2FrameLength);

	kalMemZero(prMmie, (ELEM_HDR_LEN + u4Length));
	prMmie->ucElemId = ELEM_ID_MMIE;
	prMmie->ucLength = u4Length;
	prMmie->u2KeyId = prP2pSpecificBssInfo->ucBcnKeyIdx;
	/* Left IPN & MIC to be filled by FW/HW */

	prMsduInfo->u2FrameLength += IE_SIZE(prMmie);
}
#endif /* CFG_SUPPORT_SAP_BCN_PROT */

