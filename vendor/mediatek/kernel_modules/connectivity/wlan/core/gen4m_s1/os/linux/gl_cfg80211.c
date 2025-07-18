/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: @(#) gl_cfg80211.c@@
 */

/*! \file   gl_cfg80211.c
 *    \brief  Main routines for supporintg MT6620 cfg80211 control interface
 *
 *    This file contains the support routines of Linux driver for MediaTek Inc.
 *    802.11 Wireless LAN Adapters.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "precomp.h"
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>
#include "gl_cfg80211.h"
#include "gl_vendor.h"
#include "gl_p2p_os.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#if CFG_SUPPORT_WAPI
#define KEY_BUF_SIZE	1024
#endif

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
static const uint32_t arBwCfg80211Table[] = {
	RATE_INFO_BW_20,
	RATE_INFO_BW_40,
	RATE_INFO_BW_80,
	RATE_INFO_BW_160
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
 * @brief This routine is responsible for change STA type between
 *        1. Infrastructure Client (Non-AP STA)
 *        2. Ad-Hoc IBSS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_change_iface(struct wiphy *wiphy,
			  struct net_device *ndev, enum nl80211_iftype type,
			  u32 *flags, struct vif_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_OP_MODE rOpMode;
	uint32_t u4BufLen;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	if (type == NL80211_IFTYPE_STATION)
		rOpMode.eOpMode = NET_TYPE_INFRA;
	else if (type == NL80211_IFTYPE_ADHOC)
		rOpMode.eOpMode = NET_TYPE_IBSS;
	else
		return -EINVAL;
	rOpMode.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetInfrastructureMode,
		(void *)&rOpMode, sizeof(struct PARAM_OP_MODE),
		FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set infrastructure mode error:%x\n",
		       rStatus);

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset wpa info */
	prWpaInfo->u4WpaVersion =
		IW_AUTH_WPA_VERSION_DISABLED;
	prWpaInfo->u4KeyMgmt = 0;
	prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prWpaInfo->u4Mfp = RSN_AUTH_MFP_DISABLED;
	prWpaInfo->ucRSNMfpCap = 0;
#endif

	ndev->ieee80211_ptr->iftype = type;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for adding key
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_add_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index, bool pairwise, const u8 *mac_addr,
		     struct key_params *params)
{
	struct PARAM_KEY rKey;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rslt = -EINVAL;
	uint32_t u4BufLen = 0;
	uint8_t tmp1[8], tmp2[8];
	uint8_t ucBssIndex = 0;
	const uint8_t aucBCAddr[] = BC_MAC_ADDR;
	/* const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR; */

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

 #if DBG
	if (mac_addr) {
		DBGLOG(RSN, INFO,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n",
		       key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, INFO, "keyIdx = %d pairwise = %d null mac\n",
		       key_index, pairwise);
	}
	DBGLOG(RSN, INFO, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, INFO, params->key, params->key_len);
#endif

	kalMemZero(&rKey, sizeof(struct PARAM_KEY));

	rKey.u4KeyIndex = key_index;

	if (params->cipher) {
		switch (params->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			rKey.ucCipher = CIPHER_SUITE_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			rKey.ucCipher = CIPHER_SUITE_WEP104;
			break;
#if 0
		case WLAN_CIPHER_SUITE_WEP128:
			rKey.ucCipher = CIPHER_SUITE_WEP128;
			break;
#endif
		case WLAN_CIPHER_SUITE_TKIP:
			rKey.ucCipher = CIPHER_SUITE_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			rKey.ucCipher = CIPHER_SUITE_CCMP;
			break;
		case WLAN_CIPHER_SUITE_GCMP:
			rKey.ucCipher = CIPHER_SUITE_GCMP_128;
			break;
#if 0
		case WLAN_CIPHER_SUITE_CCMP_256:
			rKey.ucCipher = CIPHER_SUITE_CCMP256;
			break;
#endif
		case WLAN_CIPHER_SUITE_SMS4:
			rKey.ucCipher = CIPHER_SUITE_WPI;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			rKey.ucCipher = CIPHER_SUITE_BIP;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			rKey.ucCipher = CIPHER_SUITE_GCMP_256;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			DBGLOG(RSN, INFO,
				"[TODO] Set BIP-GMAC-256, SW should handle it ...\n");
			return 0;
		default:
			ASSERT(FALSE);
		}
	}

	if (pairwise) {
		ASSERT(mac_addr);
		rKey.u4KeyIndex |= BIT(31);
		rKey.u4KeyIndex |= BIT(30);
		COPY_MAC_ADDR(rKey.arBSSID, mac_addr);
	} else {		/* Group key */
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	if (params->key) {
		if (params->key_len > sizeof(rKey.aucKeyMaterial))
			return -EINVAL;

		kalMemCopy(rKey.aucKeyMaterial, params->key,
			   params->key_len);
		if (rKey.ucCipher == CIPHER_SUITE_TKIP) {
			kalMemCopy(tmp1, &params->key[16], 8);
			kalMemCopy(tmp2, &params->key[24], 8);
			kalMemCopy(&rKey.aucKeyMaterial[16], tmp2, 8);
			kalMemCopy(&rKey.aucKeyMaterial[24], tmp1, 8);
		}
	}

	rKey.ucBssIdx = ucBssIndex;

	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = OFFSET_OF(struct PARAM_KEY, aucKeyMaterial)
				+ rKey.u4KeyLength;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, &rKey,
			   rKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting key for specified STA
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_get_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index,
		     bool pairwise,
		     const u8 *mac_addr, void *cookie,
		     void (*callback)(void *cookie, struct key_params *)
		    )
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	/* not implemented */

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for removing key for specified STA
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_del_key(struct wiphy *wiphy,
			 struct net_device *ndev, u8 key_index, bool pairwise,
			 const u8 *mac_addr)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BSS_INFO *prBssInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_REMOVE_KEY rRemoveKey;
	uint32_t u4BufLen = 0;
	int32_t i4Rslt = -EINVAL;
	uint8_t ucBssIndex = 0;
	uint32_t waitRet = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	if (g_u4HaltFlag) {
		DBGLOG_LIMITED(RSN, WARN, "wlan is halt, skip key deletion\n");
		return WLAN_STATUS_FAILURE;
	}

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n",
		       key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE,
			"keyIdx = %d pairwise = %d null mac\n",
		       key_index, pairwise);
	}

	kalMemZero(&rRemoveKey, sizeof(struct PARAM_REMOVE_KEY));
	rRemoveKey.u4KeyIndex = key_index;
	rRemoveKey.u4Length = sizeof(struct PARAM_REMOVE_KEY);
	if (mac_addr) {
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
		rRemoveKey.u4KeyIndex |= BIT(30);
	}

	if (prGlueInfo->prAdapter == NULL)
		return i4Rslt;

	rRemoveKey.ucBssIdx = ucBssIndex;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
		ucBssIndex);
#if CFG_SUPPORT_802_11W
	/* if encrypted deauth frame is in process, pending remove key */
	if (IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex)
		&& prBssInfo->encryptedDeauthIsInProcess == TRUE) {
		waitRet = wait_for_completion_timeout(
				&prBssInfo->rDeauthComp,
				MSEC_TO_JIFFIES(1000));
		if (!waitRet) {
			DBGLOG(RSN, WARN, "timeout\n");
			prBssInfo->encryptedDeauthIsInProcess = FALSE;
		} else
			DBGLOG(RSN, INFO, "complete\n");
	}
#endif
	rStatus = kalIoctl(prGlueInfo, wlanoidSetRemoveKey, &rRemoveKey,
			rRemoveKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG_LIMITED(RSN, WARN, "remove key error:%x\n", rStatus);
	else
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting default key on an interface
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_set_default_key(struct wiphy *wiphy,
		     struct net_device *ndev, u8 key_index, bool unicast,
		     bool multicast)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_DEFAULT_KEY rDefaultKey;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rst = -EINVAL;
	uint32_t u4BufLen = 0;
	u_int8_t fgDef = FALSE, fgMgtDef = FALSE;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* For STA, should wep set the default key !! */
 #if DBG
	DBGLOG(RSN, INFO,
	       "keyIdx = %d unicast = %d multicast = %d\n", key_index,
	       unicast, multicast);
#endif

	rDefaultKey.ucKeyID = key_index;
	rDefaultKey.ucUnicast = unicast;
	rDefaultKey.ucMulticast = multicast;
	if (rDefaultKey.ucUnicast && !rDefaultKey.ucMulticast)
		return WLAN_STATUS_SUCCESS;

	if (rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgDef = TRUE;

	if (!rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgMgtDef = TRUE;

	rDefaultKey.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDefaultKey, &rDefaultKey,
				sizeof(struct PARAM_DEFAULT_KEY),
				FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rst = 0;

	return i4Rst;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting tx rate from LLS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
static uint32_t wlanGetTxRateFromLinkStats(
	IN struct GLUE_INFO *prGlueInfo, IN uint32_t *pu4TxRate,
	IN uint32_t *pu4TxBw, IN uint8_t ucBssIndex)
{
	uint32_t rStatus = WLAN_STATUS_NOT_SUPPORTED;
#if CFG_SUPPORT_LLS
	uint32_t u4MaxTxRate, u4Nss;
	union {
		struct CMD_GET_STATS_LLS cmd;
		struct EVENT_STATS_LLS_TX_RATE_INFO rate_info;
	} query = {0};
	uint32_t u4QueryBufLen;
	uint32_t u4QueryInfoLen;
	struct _STATS_LLS_TX_RATE_INFO targetRateInfo;
	struct HAL_LLS_FW_REPORT *src;

	if (unlikely(ucBssIndex >= BSSID_NUM))
		return WLAN_STATUS_FAILURE;

	/* make sure LLs is supported */
	src = prGlueInfo->prAdapter->pucLinkStatsSrcBufferAddr;
	if (!src) {
		DBGLOG(REQ, TRACE, "EMI mapping not done. LLS not support");
		return WLAN_STATUS_NOT_SUPPORTED;
	}
	if (!prGlueInfo->prAdapter->rWifiVar.fgStatsLlsEn)
		return rStatus;

	kalMemZero(&query, sizeof(query));
	query.cmd.u4Tag = STATS_LLS_TAG_CURRENT_TX_RATE;
	u4QueryBufLen = sizeof(query);
	u4QueryInfoLen = sizeof(query.cmd);

	rStatus = kalIoctl(prGlueInfo,
			wlanQueryLinkStats,
			&query,
			u4QueryBufLen,
			TRUE,
			TRUE,
			TRUE,
			&u4QueryInfoLen);
	DBGLOG(REQ, INFO, "kalIoctl=%x, %u bytes",
				rStatus, u4QueryInfoLen);
	targetRateInfo = query.rate_info.arTxRateInfo[ucBssIndex];
	DBGLOG_HEX(REQ, INFO, &targetRateInfo, u4QueryInfoLen);

	if (unlikely(rStatus != WLAN_STATUS_SUCCESS)) {
		DBGLOG(REQ, INFO, "wlanQueryLinkStats return fail\n");
		return rStatus;
	}
	if (unlikely(u4QueryInfoLen != sizeof(
		struct EVENT_STATS_LLS_TX_RATE_INFO))) {
		DBGLOG(REQ, INFO, "wlanQueryLinkStats return len unexpected\n");
		return WLAN_STATUS_FAILURE;
	}

	if (targetRateInfo.bw >= ARRAY_SIZE(arBwCfg80211Table)) {
		DBGLOG(REQ, WARN, "wrong tx bw!");
		return WLAN_STATUS_FAILURE;
	}

	*pu4TxBw = arBwCfg80211Table[targetRateInfo.bw];
	targetRateInfo.nsts += 1;
	if (targetRateInfo.nsts == 1)
		u4Nss = targetRateInfo.nsts;
	else
		u4Nss = targetRateInfo.stbc ?
			(targetRateInfo.nsts >> 1)
			: targetRateInfo.nsts;

	wlanQueryRateByTable(targetRateInfo.mode,
			targetRateInfo.rate, targetRateInfo.bw, 0,
			u4Nss, pu4TxRate, &u4MaxTxRate);
	DBGLOG(REQ, INFO, "rate=%u mode=%u nss=%u stbc=%u bw=%u linkspeed=%u\n",
		targetRateInfo.rate, targetRateInfo.mode,
		u4Nss, targetRateInfo.stbc,
		*pu4TxBw, *pu4TxRate);

#endif
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting station information such as
 *        RSSI
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_get_station(struct wiphy *wiphy,
			     struct net_device *ndev, const u8 *mac,
			     struct station_info *sinfo)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4BufLen, u4TxRate, u4TxBw, u4RxRate, u4RxBw;
	int32_t i4Rssi = 0;
	struct PARAM_GET_STA_STATISTICS *prGetStaStatistics;
	struct PARAM_LINK_SPEED_EX rLinkSpeed = {0};
	uint32_t u4TotalError;
	uint32_t u4FcsError;
	struct net_device_stats *prDevStats;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	kalMemZero(arBssid, MAC_ADDR_LEN);
	SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check input MAC address */
	/* On Android O, this might be wlan0 address */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)
	    && UNEQUAL_MAC_ADDR(
		    prGlueInfo->prAdapter->rWifiVar.aucMacAddress, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR
		       "] currently connected BSSID["
		       MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill TX/RX rate */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
		return 0;
	}

	DBGLOG(REQ, TRACE, "Call Glue=%p, LinkSpeed=%p, size=%zu, &u4BufLen=%p",
		prGlueInfo, &rLinkSpeed, sizeof(rLinkSpeed), &u4BufLen);
	rStatus = kalIoctlByBssIdx(prGlueInfo,
				   wlanoidQueryLinkSpeedEx, &rLinkSpeed,
				   sizeof(rLinkSpeed), TRUE, FALSE, FALSE,
				   &u4BufLen, ucBssIndex);
	DBGLOG(REQ, TRACE, "kalIoctlByBssIdx()=%u, prGlueInfo=%p, u4BufLen=%u",
		rStatus, prGlueInfo, u4BufLen);



#if defined(CFG_REPORT_MAX_TX_RATE) && (CFG_REPORT_MAX_TX_RATE == 1)
	/*rewrite LinkSpeed with Max LinkSpeed*/
	rStatus = kalIoctlByBssIdx(prGlueInfo,
			       wlanoidQueryMaxLinkSpeed, &rLinkSpeed,
			       sizeof(rLinkSpeed), TRUE, FALSE, FALSE,
			       &u4BufLen, ucBssIndex);
#endif /* CFG_REPORT_MAX_TX_RATE */

	if (rStatus == WLAN_STATUS_SUCCESS &&
		IS_BSS_INDEX_VALID(ucBssIndex)) {
		u4TxRate = rLinkSpeed.rLq[ucBssIndex].u2TxLinkSpeed;
		u4RxRate = rLinkSpeed.rLq[ucBssIndex].u2RxLinkSpeed;
		i4Rssi = rLinkSpeed.rLq[ucBssIndex].cRssi;
		u4RxBw = rLinkSpeed.rLq[ucBssIndex].u4RxBw;
		if (unlikely(u4RxBw >= ARRAY_SIZE(arBwCfg80211Table))) {
			DBGLOG(REQ, WARN, "wrong u4RxBw!");
			u4RxBw = 0;
		}
	}
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
	sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
	sinfo->filled |= BIT(NL80211_STA_INFO_RX_BITRATE);
	sinfo->filled |= BIT(NL80211_STA_INFO_SIGNAL);
#else
	sinfo->filled |= STATION_INFO_TX_BITRATE;
	sinfo->filled |= STATION_INFO_RX_BITRATE;
	sinfo->filled |= STATION_INFO_SIGNAL;
#endif

	if ((rStatus != WLAN_STATUS_SUCCESS) || (u4TxRate == 0)) {
		/* unable to retrieve link speed */
		DBGLOG(REQ, WARN, "last Tx link speed\n");
	} else {
		/* convert from 100bps to 100kbps */
		prGlueInfo->u4TxLinkSpeedCache[ucBssIndex] = u4TxRate / 1000;
	}

	if ((rStatus != WLAN_STATUS_SUCCESS) || (u4RxRate == 0)) {
		/* unable to retrieve link speed */
		DBGLOG(REQ, WARN, "last Rx link speed\n");
	} else {
		/* convert from 100bps to 100kbps */
		prGlueInfo->u4RxLinkSpeedCache[ucBssIndex] = u4RxRate / 1000;
		prGlueInfo->u4RxBwCache[ucBssIndex] =
			arBwCfg80211Table[u4RxBw];
	}

	if (rStatus != WLAN_STATUS_SUCCESS || i4Rssi == 0) {
		DBGLOG(REQ, WARN,
			"Query RSSI failed, use last RSSI %d\n",
			prGlueInfo->i4RssiCache[ucBssIndex]);
		sinfo->signal = prGlueInfo->i4RssiCache[ucBssIndex] ?
			prGlueInfo->i4RssiCache[ucBssIndex] :
			PARAM_WHQL_RSSI_INITIAL_DBM;
	} else if (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM ||
			i4Rssi == PARAM_WHQL_RSSI_MAX_DBM) {
		DBGLOG(REQ, WARN,
			"RSSI abnormal, use last RSSI %d\n",
			prGlueInfo->i4RssiCache[ucBssIndex]);
		sinfo->signal = prGlueInfo->i4RssiCache[ucBssIndex] ?
			prGlueInfo->i4RssiCache[ucBssIndex] : i4Rssi;
	} else {
		sinfo->signal = i4Rssi;	/* dBm */
		prGlueInfo->i4RssiCache[ucBssIndex] = i4Rssi;
	}

	rStatus = wlanGetTxRateFromLinkStats(prGlueInfo, &u4TxRate, &u4TxBw,
			ucBssIndex);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prGlueInfo->u4TxLinkSpeedCache[ucBssIndex] = u4TxRate;
		prGlueInfo->u4TxBwCache[ucBssIndex] = u4TxBw;
	}

	sinfo->txrate.legacy =
		prGlueInfo->u4TxLinkSpeedCache[ucBssIndex];
	sinfo->rxrate.legacy =
		prGlueInfo->u4RxLinkSpeedCache[ucBssIndex];
	sinfo->txrate.bw =
		prGlueInfo->u4TxBwCache[ucBssIndex];
	sinfo->rxrate.bw =
		prGlueInfo->u4RxBwCache[ucBssIndex];

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	if (prDevStats) {
		/* 4. fill RX_PACKETS */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_BYTES64);
#else
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->filled |= NL80211_STA_INFO_RX_BYTES64;
#endif
		sinfo->rx_packets = prDevStats->rx_packets;
		sinfo->rx_bytes = prDevStats->rx_bytes;

		/* 5. fill TX_PACKETS */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_BYTES64);
#else
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->filled |= NL80211_STA_INFO_TX_BYTES64;
#endif
		sinfo->tx_packets = prDevStats->tx_packets;
		sinfo->tx_bytes = prDevStats->tx_bytes;

		/* 6. fill TX_FAILED */
		prGetStaStatistics = &(
			prGlueInfo->prAdapter->rQueryStaStatistics);
		COPY_MAC_ADDR(prGetStaStatistics->aucMacAddr, arBssid);
		prGetStaStatistics->ucReadClear = TRUE;

		rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidQueryStaStatistics,
				prGetStaStatistics,
				sizeof(struct PARAM_GET_STA_STATISTICS),
				TRUE, FALSE, TRUE, &u4BufLen, ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN,
			       "link speed=%u, rssi=%d, unable to retrieve link speed,status=%u\n",
			       sinfo->txrate.legacy, sinfo->signal, rStatus);
		} else {
			u4FcsError = prGetStaStatistics->rMibInfo[0].u4FcsError;
			u4TotalError = prGetStaStatistics->u4TxFailCount +
				       prGetStaStatistics->u4TxLifeTimeoutCount;
			prGlueInfo->u4FcsErrorCache += u4FcsError;
			prDevStats->tx_errors = u4TotalError;
#define TEMP_LOG_TEMPLATE \
	"link speed=%u/%u, bw=%u/%u, rssi=%d, BSSID:[" MACSTR "]," \
	"TxFail=%u, TxTimeOut=%u, TxOK=%u, RxOK=%u, FcsErr=%u\n"
			DBGLOG(REQ, TRACE,
				TEMP_LOG_TEMPLATE,
				sinfo->txrate.legacy, sinfo->rxrate.legacy,
				sinfo->txrate.bw, sinfo->rxrate.bw,
				sinfo->signal,
				MAC2STR(arBssid),
				prGetStaStatistics->u4TxFailCount,
				prGetStaStatistics->u4TxLifeTimeoutCount,
				prDevStats->tx_packets,
				sinfo->rx_packets,
				u4FcsError
			);
#undef TEMP_LOG_TEMPLATE
		}
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_FAILED);
#else
		sinfo->filled |= STATION_INFO_TX_FAILED;
#endif
		sinfo->tx_failed = prDevStats->tx_errors;
#if KERNEL_VERSION(4, 19, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT_ULL(NL80211_STA_INFO_FCS_ERROR_COUNT);
		sinfo->fcs_err_count = prGlueInfo->u4FcsErrorCache;
#endif
	}

	return 0;
}
#else
int mtk_cfg80211_get_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac,
			     struct station_info *sinfo)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4BufLen, u4Rate;
	int32_t i4Rssi;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	struct PARAM_LINK_SPEED_EX rLinkSpeed;
	uint32_t u4TotalError;
	struct net_device_stats *prDevStats;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	kalMemZero(arBssid, MAC_ADDR_LEN);
	SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR
		       "] currently connected BSSID["
		       MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill TX rate */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
#if defined(CFG_REPORT_MAX_TX_RATE) && (CFG_REPORT_MAX_TX_RATE == 1)
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryMaxLinkSpeed,
				&u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE,
				&u4BufLen,
				ucBssIndex);
#else
		DBGLOG(REQ, TRACE, "Call LinkSpeed=%p sizeof=%zu &u4BufLen=%p",
			&rLinkSpeed, sizeof(rLinkSpeed), &u4BufLen);
		rStatus = kalIoctlByBssIdx(prGlueInfo,
					   wlanoidQueryLinkSpeedEx,
					   &rLinkSpeed, sizeof(rLinkSpeed),
					   TRUE, FALSE, FALSE,
					   &u4BufLen, ucBssIndex);
		DBGLOG(REQ, TRACE, "rStatus=%u, prGlueInfo=%p, u4BufLen=%u",
			rStatus, prGlueInfo, u4BufLen);
		if (ucBssIndex < BSSID_NUM)
			u4Rate = rLinkSpeed.rLq[ucBssIndex].u2TxLinkSpeed;
#endif /* CFG_REPORT_MAX_TX_RATE */

		sinfo->filled |= STATION_INFO_TX_BITRATE;

		if ((rStatus != WLAN_STATUS_SUCCESS) || (u4Rate == 0)) {
			/* unable to retrieve link speed */
			DBGLOG(REQ, WARN, "last link speed\n");
			sinfo->txrate.legacy =
				prGlueInfo->u4TxLinkSpeedCache[ucBssIndex];
		} else {
			/* convert from 100bps to 100kbps */
			sinfo->txrate.legacy = u4Rate / 1000;
			prGlueInfo->u4TxLinkSpeedCache[ucBssIndex] =
				u4Rate / 1000;
		}
	}

	/* 3. fill RSSI */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidQueryRssi,
				&rLinkSpeed, sizeof(rLinkSpeed),
				TRUE, FALSE, FALSE,
				&u4BufLen, ucBssIndex);
		if (IS_BSS_INDEX_VALID(ucBssIndex))
			i4Rssi = rLinkSpeed.rLq[ucBssIndex].cRssi;

		sinfo->filled |= STATION_INFO_SIGNAL;

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN,
				"Query RSSI failed, use last RSSI %d\n",
				prGlueInfo->i4RssiCache[ucBssIndex]);
			sinfo->signal = prGlueInfo->i4RssiCache[ucBssIndex] ?
				prGlueInfo->i4RssiCache[ucBssIndex] :
				PARAM_WHQL_RSSI_INITIAL_DBM;
		} else if (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM ||
			i4Rssi == PARAM_WHQL_RSSI_MAX_DBM) {
			DBGLOG(REQ, WARN,
				"RSSI abnormal, use last RSSI %d\n",
				prGlueInfo->i4RssiCache[ucBssIndex]);
			sinfo->signal = prGlueInfo->i4RssiCache[ucBssIndex] ?
				prGlueInfo->i4RssiCache[ucBssIndex] : i4Rssi;
		} else {
			sinfo->signal = i4Rssi;	/* dBm */
			prGlueInfo->i4RssiCache[ucBssIndex] = i4Rssi;
		}
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);
	if (prDevStats) {
		/* 4. fill RX_PACKETS */
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->rx_packets = prDevStats->rx_packets;

		/* 5. fill TX_PACKETS */
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->tx_packets = prDevStats->tx_packets;

		/* 6. fill TX_FAILED */
		kalMemZero(&rQueryStaStatistics,
			   sizeof(rQueryStaStatistics));
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, arBssid);
		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
				   &rQueryStaStatistics,
				   sizeof(rQueryStaStatistics),
				   TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN,
			       "link speed=%u, rssi=%d, unable to get sta statistics: status=%u\n",
			       sinfo->txrate.legacy, sinfo->signal, rStatus);
		} else {
			DBGLOG(REQ, INFO,
			       "link speed=%u, rssi=%d, BSSID=[" MACSTR
			       "], TxFailCount=%d, LifeTimeOut=%d\n",
			       sinfo->txrate.legacy, sinfo->signal,
			       MAC2STR(arBssid),
			       rQueryStaStatistics.u4TxFailCount,
			       rQueryStaStatistics.u4TxLifeTimeoutCount);

			u4TotalError = rQueryStaStatistics.u4TxFailCount +
				       rQueryStaStatistics.u4TxLifeTimeoutCount;
			prDevStats->tx_errors += u4TotalError;
		}
		sinfo->filled |= STATION_INFO_TX_FAILED;
		sinfo->tx_failed = prDevStats->tx_errors;
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to do a scan
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_scan(struct wiphy *wiphy,
		      struct cfg80211_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t i, j = 0, u4BufLen;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;
	uint32_t num_ssid = 0;
	uint32_t old_num_ssid = 0;
	uint32_t u4ValidIdx = 0;
	uint32_t wildcard_flag = 0;
#if (CFG_SUPPORT_QA_TOOL == 1) || (CFG_SUPPORT_LOWLATENCY_MODE == 1)
	struct ADAPTER *prAdapter = NULL;
#endif
	uint8_t ucBssIndex = 0;
	GLUE_SPIN_LOCK_DECLARATION();

	if (kalIsResetting())
		return -EBUSY;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, ERROR, "prGlueInfo is NULL");
		return -EINVAL;
	}

	ucBssIndex = wlanGetBssIdx(request->wdev->netdev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

#if (CFG_SUPPORT_QA_TOOL == 1) || (CFG_SUPPORT_LOWLATENCY_MODE == 1)
	prAdapter = prGlueInfo->prAdapter;
	if (prGlueInfo->prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo->prAdapter is NULL");
		return -EINVAL;
	}
#endif

	if (wlanIsChipAssert(prGlueInfo->prAdapter))
		return -EBUSY;

#if CFG_SUPPORT_QA_TOOL
	if (prAdapter->fgTestMode) {
		DBGLOG(REQ, ERROR,
			"directly return scan done, TestMode running\n");

		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		kalCfg80211ScanDone(request, FALSE);
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		return 0;
	}
	if (prAdapter->rIcapInfo.eIcapState != ICAP_STATE_INIT) {
		DBGLOG(REQ, ERROR, "skip scan, ICAP In State(%d)\n",
			prAdapter->rIcapInfo.eIcapState);
		return -EBUSY;
	}
#endif

	kalScanReqLog(request);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo->prScanRequest != NULL\n");
		return -EBUSY;
	}

#if CFG_SUPPORT_LOWLATENCY_MODE
	if (!prGlueInfo->prAdapter->fgEnCfg80211Scan
	    && MEDIA_STATE_CONNECTED
	    == kalGetMediaStateIndicated(prGlueInfo, ucBssIndex)) {
		DBGLOG(REQ, INFO,
		       "mtk_cfg80211_scan LowLatency reject scan\n");
		return -EBUSY;
	}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#if CFG_SUPPORT_SCAN_CACHE_RESULT
	prGlueInfo->scanCache.prGlueInfo = prGlueInfo;
	prGlueInfo->scanCache.prRequest = request;
	prGlueInfo->scanCache.n_channels = (uint32_t) request->n_channels;
	prGlueInfo->scanCache.ucBssIndex = ucBssIndex;
	prGlueInfo->scanCache.u4Flags = request->flags;
	if (isScanCacheDone(&prGlueInfo->scanCache) == TRUE)
		return 0;
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

	prScanRequest = kalMemAlloc(sizeof(struct PARAM_SCAN_REQUEST_ADV),
			VIR_MEM_TYPE);
	if (prScanRequest == NULL) {
		DBGLOG(REQ, ERROR, "alloc scan request fail\n");
		return -ENOMEM;

	}
	kalMemZero(prScanRequest,
		   sizeof(struct PARAM_SCAN_REQUEST_ADV));

	if (request->n_ssids == 0) {
		prScanRequest->u4SsidNum = 0;
		prScanRequest->ucScanType = SCAN_TYPE_PASSIVE_SCAN;
	} else if ((request->ssids) && (request->n_ssids > 0)
		   && (request->n_ssids <= (SCN_SSID_MAX_NUM + 1))) {
		num_ssid = (uint32_t)request->n_ssids;
		old_num_ssid = (uint32_t)request->n_ssids;
		u4ValidIdx = 0;
		for (i = 0; i < request->n_ssids; i++) {
			if ((request->ssids[i].ssid[0] == 0)
			    || (request->ssids[i].ssid_len == 0)) {
				/* remove if this is a wildcard scan */
				num_ssid--;
				wildcard_flag |= (1 << i);
				DBGLOG(REQ, STATE, "i=%d, wildcard scan\n", i);
				continue;
			}
			COPY_SSID(prScanRequest->rSsid[u4ValidIdx].aucSsid,
				prScanRequest->rSsid[u4ValidIdx].u4SsidLen,
				request->ssids[i].ssid,
				request->ssids[i].ssid_len);
			if (prScanRequest->rSsid[u4ValidIdx].u4SsidLen >
				ELEM_MAX_LEN_SSID) {
				prScanRequest->rSsid[u4ValidIdx].u4SsidLen =
				ELEM_MAX_LEN_SSID;
			}
			DBGLOG(REQ, STATE,
			       "i=%d, u4ValidIdx=%d, Ssid=%s, SsidLen=%d\n",
			       i, u4ValidIdx,
			       HIDE(prScanRequest->rSsid[u4ValidIdx].aucSsid),
			       prScanRequest->rSsid[u4ValidIdx].u4SsidLen);

			u4ValidIdx++;
			if (u4ValidIdx == SCN_SSID_MAX_NUM) {
				DBGLOG(REQ, STATE, "SCN_SSID_MAX_NUM\n");
				break;
			}
		}
		/* real SSID number to firmware */
		prScanRequest->u4SsidNum = u4ValidIdx;
		prScanRequest->ucScanType = SCAN_TYPE_ACTIVE_SCAN;
	} else {
		DBGLOG(REQ, ERROR, "request->n_ssids:%d\n",
		       request->n_ssids);
		kalMemFree(prScanRequest,
			   sizeof(struct PARAM_SCAN_REQUEST_ADV), VIR_MEM_TYPE);
		return -EINVAL;
	}

	/* 6G only need to scan PSC channel, transform channel list first*/
	for (i = 0; i < request->n_channels; i++) {
		uint32_t u4channel =
		nicFreq2ChannelNum(request->channels[i]->center_freq *
								1000);
		if (u4channel == 0) {
			DBGLOG(REQ, WARN, "Wrong Channel[%d] freq=%u\n",
			       i, request->channels[i]->center_freq);
			continue;
		}
		prScanRequest->arChannel[j].ucChannelNum = u4channel;
		switch ((request->channels[i])->band) {
		case KAL_BAND_2GHZ:
			prScanRequest->arChannel[j].eBand = BAND_2G4;
			break;
		case KAL_BAND_5GHZ:
			prScanRequest->arChannel[j].eBand = BAND_5G;
			break;
#if (CFG_SUPPORT_WIFI_6G == 1)
		case KAL_BAND_6GHZ:
			/* find out 6G PSC channel */
			if (((u4channel - 5) % 16) != 0)
				continue;

			prScanRequest->arChannel[j].eBand = BAND_6G;
			break;
#endif
		default:
			DBGLOG(REQ, WARN, "UNKNOWN Band %d(chnl=%u)\n",
			       request->channels[i]->band,
			       u4channel);
			prScanRequest->arChannel[j].eBand = BAND_NULL;
			break;
		}
		j++;
	}
	prScanRequest->u4ChannelNum = j;

	/* Check if channel list > MAX support number */
	if (prScanRequest->u4ChannelNum > MAXIMUM_OPERATION_CHANNEL_LIST) {
		prScanRequest->u4ChannelNum = 0;
		DBGLOG(REQ, INFO,
		       "Channel list (%u->%u) exceed maximum support.\n",
		       request->n_channels,
		       prScanRequest->u4ChannelNum);
	}

	if (kalScanParseRandomMac(request->wdev->netdev,
		request, prScanRequest->aucRandomMac)) {
		prScanRequest->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	}

	if (request->ie_len > 0) {
		prScanRequest->u4IELength = request->ie_len;
		prScanRequest->pucIE = (uint8_t *) (request->ie);
	}

#define TEMP_LOG_TEMPLATE "n_ssid=(%u->%u) n_channel(%u==>%u) " \
	"wildcard=0x%X flag=0x%x random_mac=" MACSTR "\n"
	DBGLOG(REQ, INFO, TEMP_LOG_TEMPLATE,
		request->n_ssids, num_ssid, request->n_channels,
		prScanRequest->u4ChannelNum, wildcard_flag,
		request->flags,
		MAC2STR(prScanRequest->aucRandomMac));
#undef TEMP_LOG_TEMPLATE

	prScanRequest->ucBssIndex = ucBssIndex;
	prScanRequest->u4Flags = request->flags;
	prGlueInfo->prScanRequest = request;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetBssidListScanAdv,
			   prScanRequest, sizeof(struct PARAM_SCAN_REQUEST_ADV),
			   FALSE, FALSE, FALSE, &u4BufLen);

	kalMemFree(prScanRequest,
		   sizeof(struct PARAM_SCAN_REQUEST_ADV), VIR_MEM_TYPE);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		prGlueInfo->prScanRequest = NULL;
		DBGLOG(REQ, WARN, "scan error:%x\n", rStatus);
		return -EINVAL;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for abort an ongoing scan. The driver
 *        shall indicate the status of the scan through cfg80211_scan_done()
 *
 * @param wiphy - pointer of wireless hardware description
 *        wdev - pointer of  wireless device state
 *
 */
/*----------------------------------------------------------------------------*/
void mtk_cfg80211_abort_scan(struct wiphy *wiphy,
			     struct wireless_dev *wdev)
{
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return;

	scanlog_dbg(LOG_SCAN_ABORT_REQ_K2D, INFO, "mtk_cfg80211_abort_scan\n");

	rStatus = kalIoctlByBssIdx(prGlueInfo,
			   wlanoidAbortScan,
			   NULL, 1, FALSE, FALSE, TRUE, &u4SetInfoLen,
			   ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, ERROR, "wlanoidAbortScan fail 0x%x\n", rStatus);
}

int wlanParseAkmSuites(uint32_t *au4AkmSuites, uint32_t u4AkmSuitesCount,
	uint32_t u4WpaVersion, enum ENUM_PARAM_AUTH_MODE *prAuthMode,
	uint32_t *pu4AkmSuite, struct IEEE_802_11_MIB *prMib)
{
	enum ENUM_PARAM_AUTH_MODE eOriAuthMode = *prAuthMode;
	uint8_t i, j;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY *prEntry;

	for (i = 0;
#ifdef CFG80211_MAX_NUM_AKM_SUITES
		i < u4AkmSuitesCount && i < CFG80211_MAX_NUM_AKM_SUITES; i++) {
#else
		i < u4AkmSuitesCount && i < NL80211_MAX_NR_AKM_SUITES; i++) {
#endif
		uint32_t u4AkmSuite = 0;
		enum ENUM_PARAM_AUTH_MODE eAuthMode;

		if (u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
			switch (au4AkmSuites[i]) {
			case WLAN_AKM_SUITE_OSEN:
				u4AkmSuite = RSN_AKM_SUITE_OSEN;
				break;
			default:
				break;
			}
		} else if (u4WpaVersion == IW_AUTH_WPA_VERSION_WPA ||
			u4WpaVersion == IW_AUTH_WPA_VERSION_WPA2) {
			switch (au4AkmSuites[i]) {
			case WLAN_AKM_SUITE_8021X:
				if (u4WpaVersion == IW_AUTH_WPA_VERSION_WPA)
					u4AkmSuite = WPA_AKM_SUITE_802_1X;
				else
					u4AkmSuite = RSN_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				if (u4WpaVersion == IW_AUTH_WPA_VERSION_WPA)
					u4AkmSuite = WPA_AKM_SUITE_PSK;
				else
					u4AkmSuite = RSN_AKM_SUITE_PSK;
				break;
#if CFG_SUPPORT_802_11R
			case WLAN_AKM_SUITE_FT_8021X:
				u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
				break;
			case WLAN_AKM_SUITE_FT_PSK:
				u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
				break;
			case WLAN_AKM_SUITE_FT_OVER_SAE:
				u4AkmSuite = RSN_AKM_SUITE_FT_OVER_SAE;
				break;
			case WLAN_AKM_SUITE_FT_802_1X_SHA384_UNRESTRICTED:
				u4AkmSuite =
				    RSN_AKM_SUITE_FT_802_1X_SHA384_UNRESTRICTED;
				break;
			case WLAN_AKM_SUITE_FT_SAE_EXT_KEY:
				u4AkmSuite = RSN_AKM_SUITE_FT_SAE_EXT_KEY;
				break;
#endif
#if CFG_SUPPORT_802_11W
			/* Notice:: Need kernel patch!! */
			case WLAN_AKM_SUITE_8021X_SHA256:
				u4AkmSuite = RSN_AKM_SUITE_802_1X_SHA256;
				break;
			case WLAN_AKM_SUITE_PSK_SHA256:
				u4AkmSuite = RSN_AKM_SUITE_PSK_SHA256;
				break;
#endif
#if CFG_SUPPORT_PASSPOINT
			case WLAN_AKM_SUITE_OSEN:
				u4AkmSuite = RSN_AKM_SUITE_OSEN;
				break;
#endif
			case WLAN_AKM_SUITE_SAE:
				u4AkmSuite = RSN_AKM_SUITE_SAE;
				break;
			case WLAN_AKM_SUITE_SAE_EXT_KEY:
				u4AkmSuite = RSN_AKM_SUITE_SAE_EXT_KEY;
				break;
			case WLAN_AKM_SUITE_OWE:
				u4AkmSuite = RSN_AKM_SUITE_OWE;
				break;
#if CFG_SUPPORT_DPP
			case WLAN_AKM_SUITE_DPP:
				u4AkmSuite = RSN_AKM_SUITE_DPP;
				break;
#endif
			case WLAN_AKM_SUITE_8021X_SUITE_B_192:
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;
			default:
				DBGLOG(REQ, WARN, "invalid Akm Suite (0x%x)\n",
				       au4AkmSuites[i]);
				return -EINVAL;
			}
		}

		eAuthMode = rsnKeyMgmtToAuthMode(
			eOriAuthMode, u4WpaVersion, u4AkmSuite);

		/* Enable the specific AKM suite only. */
		for (j = 0; j < MAX_NUM_SUPPORTED_AKM_SUITES; j++) {
			prEntry =
			    &prMib->dot11RSNAConfigAuthenticationSuitesTable[j];

			if (prEntry->dot11RSNAConfigAuthenticationSuite !=
				u4AkmSuite)
				continue;

			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
				TRUE;

			prMib->dot11RSNAConfigAkm |=
				rsnKeyMgmtToBit(u4AkmSuite);
		}

		if (i == 0) {
			*prAuthMode = eAuthMode;
			*pu4AkmSuite = u4AkmSuite;
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to connect to
 *        the ESS with the specified parameters
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_connect(struct wiphy *wiphy,
			 struct net_device *ndev,
			 struct cfg80211_connect_params *sme)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	enum ENUM_WEP_STATUS eEncStatus;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	uint32_t cipher;
	struct PARAM_CONNECT rNewSsid;
	struct PARAM_OP_MODE rOpMode;
	uint32_t u4AkmSuite = 0;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif
	struct GL_WPA_INFO *prWpaInfo;
	struct IEEE_802_11_MIB *prMib;
#if CFG_SUPPORT_PASSPOINT
	struct HS20_INFO *prHS20Info;
#endif
	uint8_t ucBssIndex = 0;
	char log[256] = {0};
	uint8_t aucSSID[ELEM_MAX_LEN_SSID+1];

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "[wlan%d] mtk_cfg80211_connect %p %zu\n",
		ucBssIndex, sme->ie, sme->ie_len);

	prConnSettings =
		aisGetConnSettings(prGlueInfo->prAdapter,
		ucBssIndex);
	if (prConnSettings->eOPMode >
	    NET_TYPE_AUTO_SWITCH)
		rOpMode.eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		rOpMode.eOpMode = prConnSettings->eOPMode;
	rOpMode.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetInfrastructureMode,
		(void *)&rOpMode, sizeof(struct PARAM_OP_MODE),
		FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO,
		       "wlanoidSetInfrastructureMode fail 0x%x\n", rStatus);
		return -EFAULT;
	}

	/* after set operation mode, key table are cleared */

#if CFG_SUPPORT_REPLAY_DETECTION
	/* reset Detect replay information */
	prDetRplyInfo = aisGetDetRplyInfo(prGlueInfo->prAdapter,
		ucBssIndex);
	kalMemZero(prDetRplyInfo, sizeof(struct GL_DETECT_REPLAY_INFO));
#endif

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);
	/* <1> Reset WPA info */
	prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prWpaInfo->u4KeyMgmt = 0;
	prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
	prWpaInfo->fgPrivacyInvoke = FALSE;
#if CFG_SUPPORT_802_11W
	prWpaInfo->u4Mfp = RSN_AUTH_MFP_DISABLED;
	prWpaInfo->ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
	prWpaInfo->u4CipherGroupMgmt = RSN_CIPHER_SUITE_BIP_CMAC_128;
#endif
	aisInitializeConnectionRsnInfo(prGlueInfo->prAdapter, ucBssIndex);
	prMib = aisGetMib(prGlueInfo->prAdapter, ucBssIndex);

	if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_1)
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_WPA;
	else if ((sme->crypto.wpa_versions & NL80211_WPA_VERSION_2)
		|| (sme->crypto.wpa_versions & NL80211_WPA_VERSION_3))
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_WPA2;
	else
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

	DBGLOG(REQ, INFO,
	       "sme->auth_type=%x, sme->crypto.wpa_versions=%x",
		sme->auth_type,	sme->crypto.wpa_versions);

	switch (sme->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		eAuthMode = AUTH_MODE_OPEN;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_SHARED_KEY;
		eAuthMode = AUTH_MODE_SHARED;
		break;
	case NL80211_AUTHTYPE_FT:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_FT;
		eAuthMode = AUTH_MODE_OPEN;
		break;
	case NL80211_AUTHTYPE_SAE:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_SAE;
		/* To prevent FWKs asks connect without AKM Suite */
		eAuthMode = AUTH_MODE_WPA3_SAE;
		u4AkmSuite = RSN_AKM_SUITE_SAE;
		break;
	default:
		/* NL80211 only set the Tx wep key while connect */
		if (sme->key_len != 0) {
			prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM |
					       IW_AUTH_ALG_SHARED_KEY;
			eAuthMode = AUTH_MODE_AUTO_SWITCH;
		} else {
			prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
			eAuthMode = AUTH_MODE_OPEN;
		}
		break;
	}

	if (sme->crypto.n_akm_suites) {
		DBGLOG(REQ, INFO, "n_akm_suites=%x, akm_suites=%x",
			sme->crypto.n_akm_suites,
			sme->crypto.akm_suites[0]);
		if (wlanParseAkmSuites(sme->crypto.akm_suites,
			sme->crypto.n_akm_suites, prWpaInfo->u4WpaVersion,
			&eAuthMode, &u4AkmSuite, prMib) < 0) {
			return -EINVAL;
		}
	}

	if (sme->crypto.n_ciphers_pairwise) {
		DBGLOG(RSN, INFO, "cipher pairwise (0x%x)\n",
		       sme->crypto.ciphers_pairwise[0]);
		prMib->dot11RSNAConfigPairwiseCipher =
			SWAP32(sme->crypto.ciphers_pairwise[0]);
		switch (sme->crypto.ciphers_pairwise[0]) {
		case WLAN_CIPHER_SUITE_WEP40:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_TKIP;
#if (CFG_TC10_FEATURE == 1)
			if (eAuthMode == AUTH_MODE_WPA_PSK ||
			    eAuthMode == AUTH_MODE_WPA2_PSK)
				prWpaInfo->u4CipherPairwise |=
					IW_AUTH_CIPHER_CCMP;
#endif
			break;
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_CCMP;
#if (CFG_TC10_FEATURE == 1)
			if (eAuthMode == AUTH_MODE_WPA_PSK ||
			    eAuthMode == AUTH_MODE_WPA2_PSK)
				prWpaInfo->u4CipherPairwise |=
					IW_AUTH_CIPHER_TKIP;
#endif
			break;
#if CFG_SUPPORT_WAPI
		case WLAN_CIPHER_SUITE_SMS4:
			break;
#endif
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		case WLAN_CIPHER_SUITE_GCMP_256:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_GCMP256;
			break;
#endif
		case WLAN_CIPHER_SUITE_GCMP:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_GCMP128;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			DBGLOG(REQ, INFO, "WLAN_CIPHER_SUITE_NO_GROUP_ADDR\n");
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher pairwise (%d)\n",
			       sme->crypto.ciphers_pairwise[0]);
			return -EINVAL;
		}
	}

	if (sme->crypto.cipher_group) {
		DBGLOG(RSN, INFO, "cipher group (0x%x)\n",
		       sme->crypto.cipher_group);
		prMib->dot11RSNAConfigGroupCipher =
			SWAP32(sme->crypto.cipher_group);
		switch (sme->crypto.cipher_group) {
		case WLAN_CIPHER_SUITE_WEP40:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_TKIP;
#if (CFG_TC10_FEATURE == 1)
			if (eAuthMode == AUTH_MODE_WPA_PSK ||
			    eAuthMode == AUTH_MODE_WPA2_PSK)
				prWpaInfo->u4CipherGroup |=
					IW_AUTH_CIPHER_CCMP;
#endif
			break;
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_CCMP;
#if (CFG_TC10_FEATURE == 1)
			if (eAuthMode == AUTH_MODE_WPA_PSK ||
			    eAuthMode == AUTH_MODE_WPA2_PSK)
				prWpaInfo->u4CipherGroup |=
					IW_AUTH_CIPHER_TKIP;
#endif
			break;
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		case WLAN_CIPHER_SUITE_GCMP_256:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_GCMP256;
			break;
#endif
		case WLAN_CIPHER_SUITE_GCMP:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_GCMP128;
			break;
#if CFG_SUPPORT_WAPI
		case WLAN_CIPHER_SUITE_SMS4:
			break;
#endif
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher group (%d)\n",
			       sme->crypto.cipher_group);
			return -EINVAL;
		}
	}

	DBGLOG(REQ, INFO,
		"u4WpaVersion=%d, u4AuthAlg=%d, eAuthMode=%d, u4AkmSuite=0x%x, u4CipherGroup=0x%x, u4CipherPairwise=0x%x\n",
		prWpaInfo->u4WpaVersion, prWpaInfo->u4AuthAlg,
		eAuthMode, u4AkmSuite, prWpaInfo->u4CipherGroup,
		prWpaInfo->u4CipherPairwise);

	prWpaInfo->fgPrivacyInvoke = sme->privacy;
	prConnSettings->fgWpsActive = FALSE;

#if CFG_SUPPORT_PASSPOINT
	prHS20Info = aisGetHS20Info(prGlueInfo->prAdapter,
		ucBssIndex);
	prHS20Info->fgConnectHS20AP = FALSE;
#endif /* CFG_SUPPORT_PASSPOINT */

	prConnSettings->non_wfa_vendor_ie_len = 0;
	if (sme->ie && sme->ie_len > 0) {
		uint32_t rStatus;
		uint32_t u4BufLen;
		uint8_t *prDesiredIE = NULL;
		uint8_t *pucIEStart = (uint8_t *)sme->ie;
#if CFG_SUPPORT_WAPI
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetWapiAssocInfo,
				pucIEStart, sme->ie_len, FALSE, FALSE, FALSE,
				&u4BufLen,
				ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, TRACE,
				"[wapi] wapi not support due to set wapi assoc info error:%x\n",
				rStatus);
#endif
#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE(pucIEStart, sme->ie_len,
					  (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctlByBssIdx(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE),
					   FALSE, FALSE, TRUE, &u4BufLen,
					   ucBssIndex);
		}
#endif /* CFG_SUPPORT_PASSPOINT */
		if (wextSrchDesiredWPAIE(pucIEStart, sme->ie_len, ELEM_ID_RSN,
					 (uint8_t **) &prDesiredIE)) {
			struct RSN_INFO rRsnInfo;

			if (rsnParseRsnIE(prGlueInfo->prAdapter,
			    (struct RSN_INFO_ELEM *)prDesiredIE, &rRsnInfo)) {
				prWpaInfo->u4CipherGroupMgmt =
					rRsnInfo.u4GroupMgmtCipherSuite;
				DBGLOG(RSN, INFO,
					"RSN: group mgmt cipher suite 0x%x\n",
					prWpaInfo->u4CipherGroupMgmt);
#if CFG_SUPPORT_802_11W
				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
					prWpaInfo->ucRSNMfpCap =
							RSN_AUTH_MFP_OPTIONAL;
					if (rRsnInfo.u2RsnCap &
					    ELEM_WPA_CAP_MFPR)
						prWpaInfo->
						ucRSNMfpCap =
							RSN_AUTH_MFP_REQUIRED;
				} else
					prWpaInfo->ucRSNMfpCap =
							RSN_AUTH_MFP_DISABLED;
#endif
			}
		}
		if (wextSrchDesiredWPAIE(pucIEStart, sme->ie_len, ELEM_ID_RSNX,
					 (uint8_t **) &prDesiredIE)) {
			struct RSNX_INFO rRsnxeInfo;

			if (rsnParseRsnxIE(prGlueInfo->prAdapter,
				(struct RSNX_INFO_ELEM *)prDesiredIE,
					&rRsnxeInfo)) {
				prWpaInfo->u2RSNXCap = rRsnxeInfo.u2Cap;
				if (prWpaInfo->u2RSNXCap &
					BIT(WLAN_RSNX_CAPAB_SAE_H2E)) {
					DBGLOG(RSN, INFO,
						"SAE-H2E is supported, RSNX ie: 0x%x\n",
						prWpaInfo->u2RSNXCap);
				}
			}
		}
	}

	/* Fill WPA info - mfp setting */
	/* Must put after paring RSNE from upper layer
	* for prWpaInfo->ucRSNMfpCap assignment
	*/
#if CFG_SUPPORT_802_11W
	switch (sme->mfp) {
	case NL80211_MFP_NO:
		prWpaInfo->u4Mfp = RSN_AUTH_MFP_DISABLED;
		/* Change Mfp parameter from DISABLED to OPTIONAL
		* if upper layer set MFPC = 1 in RSNE
		* since upper layer can't bring MFP OPTIONAL information
		* to driver by sme->mfp
		*/
		if (prWpaInfo->ucRSNMfpCap == RSN_AUTH_MFP_OPTIONAL)
			prWpaInfo->u4Mfp = RSN_AUTH_MFP_OPTIONAL;
		else if (prWpaInfo->ucRSNMfpCap ==
					RSN_AUTH_MFP_REQUIRED)
			DBGLOG(REQ, WARN,
				"mfp parameter(DISABLED) conflict with mfp cap(REQUIRED)\n");
		break;
	case NL80211_MFP_REQUIRED:
		prWpaInfo->u4Mfp = RSN_AUTH_MFP_REQUIRED;
		break;
	default:
		prWpaInfo->u4Mfp = RSN_AUTH_MFP_DISABLED;
		break;
	}
	DBGLOG(REQ, INFO, "MFP=%d\n", prWpaInfo->u4Mfp);
#endif

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetAuthMode, &eAuthMode,
			sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen,
			ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set auth mode error:%x\n", rStatus);

	cipher = prWpaInfo->u4CipherGroup | prWpaInfo->u4CipherPairwise;

	if (1 /* prWpaInfo->fgPrivacyInvoke */) {
		if (cipher & (IW_AUTH_CIPHER_GCMP256 |
			      IW_AUTH_CIPHER_GCMP128)) {
			eEncStatus = ENUM_ENCRYPTION4_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_CCMP) {
			eEncStatus = ENUM_ENCRYPTION3_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_TKIP) {
			eEncStatus = ENUM_ENCRYPTION2_ENABLED;
		} else if (cipher & (IW_AUTH_CIPHER_WEP104 |
				     IW_AUTH_CIPHER_WEP40)) {
			eEncStatus = ENUM_ENCRYPTION1_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_NONE) {
			if (prWpaInfo->fgPrivacyInvoke)
				eEncStatus = ENUM_ENCRYPTION1_ENABLED;
			else
				eEncStatus = ENUM_ENCRYPTION_DISABLED;
		} else {
			eEncStatus = ENUM_ENCRYPTION_DISABLED;
		}
	} else {
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
	}

	rStatus = kalIoctlByBssIdx(prGlueInfo,
			wlanoidSetEncryptionStatus, &eEncStatus,
			sizeof(eEncStatus), FALSE, FALSE, FALSE, &u4BufLen,
			ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set encryption mode error:%x\n",
		       rStatus);

	if (sme->key_len != 0
	    && prWpaInfo->u4WpaVersion ==
	    IW_AUTH_WPA_VERSION_DISABLED) {
		/* NL80211 only set the Tx wep key while connect, the max 4 wep
		 * key set prior via add key cmd
		 */
		uint8_t wepBuf[48];
		struct PARAM_WEP *prWepKey = (struct PARAM_WEP *) wepBuf;

		kalMemZero(prWepKey, sizeof(struct PARAM_WEP));
		prWepKey->u4Length = OFFSET_OF(struct PARAM_WEP,
					       aucKeyMaterial) + sme->key_len;
		prWepKey->u4KeyLength = (uint32_t) sme->key_len;
		prWepKey->u4KeyIndex = (uint32_t) sme->key_idx;
		prWepKey->u4KeyIndex |= IS_TRANSMIT_KEY;
		if (prWepKey->u4KeyLength > MAX_KEY_LEN) {
			DBGLOG(REQ, WARN, "Too long key length (%u)\n",
			       prWepKey->u4KeyLength);
			return -EINVAL;
		}
		kalMemCopy(prWepKey->aucKeyMaterial, sme->key,
			   prWepKey->u4KeyLength);

		rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidAbortScan,
				NULL, 1, FALSE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, ERROR, "wlanoidAbortScan fail 0x%x\n",
				rStatus);

		rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidSetAddWep, prWepKey,
				prWepKey->u4Length,
				FALSE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "wlanoidSetAddWep fail 0x%x\n",
				rStatus);
			return -EFAULT;
		}
	}

	/* Avoid dangling pointer, set defatul all zero */
	kalMemZero(&rNewSsid, sizeof(rNewSsid));
	rNewSsid.u4CenterFreq = sme->channel ?
				sme->channel->center_freq : 0;
	rNewSsid.pucBssid = (uint8_t *)sme->bssid;
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
	rNewSsid.pucBssidHint = (uint8_t *)sme->bssid_hint;
#endif
	rNewSsid.pucSsid = (uint8_t *)sme->ssid;
	rNewSsid.u4SsidLen = sme->ssid_len;
	rNewSsid.pucIEs = (uint8_t *)sme->ie;
	rNewSsid.u4IesLen = sme->ie_len;
	rNewSsid.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetConnect,
			   (void *)&rNewSsid, sizeof(struct PARAM_CONNECT),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set SSID:%x\n", rStatus);
		return -EINVAL;
	}

	kalMemZero(aucSSID, ELEM_MAX_LEN_SSID+1);
	COPY_SSID(aucSSID, sme->ssid_len,
		sme->ssid, sme->ssid_len);
	kalSprintf(log, "[CONN] CONNECTING ssid=\"%s\"", aucSSID);
	if (sme->bssid)
		kalSprintf(log + strlen(log), " bssid=" RPTMACSTR,
			RPTMAC2STR(sme->bssid));
	if (sme->bssid_hint)
		kalSprintf(log + strlen(log), " bssid_hint=" RPTMACSTR,
			RPTMAC2STR(sme->bssid_hint));
	if (sme->channel && sme->channel->center_freq)
		kalSprintf(log + strlen(log), " freq=%d",
			sme->channel->center_freq);
	if (sme->channel_hint && sme->channel_hint->center_freq)
		kalSprintf(log + strlen(log), " freq_hint=%d",
			sme->channel_hint->center_freq);

	kalSprintf(log + strlen(log),
		" pairwise=0x%x group=0x%x akm=0x%x auth_type=%x btcoex=%d",
		sme->crypto.ciphers_pairwise[0],
		sme->crypto.cipher_group,
		sme->crypto.akm_suites[0],
		sme->auth_type,
		(aisGetAisBssInfo(prGlueInfo->prAdapter,
		ucBssIndex)->eCoexMode == COEX_TDD_MODE));
	kalReportWifiLog(prGlueInfo->prAdapter, ucBssIndex, log);

	return 0;
}
#if CFG_SUPPORT_WPA3
int mtk_cfg80211_external_auth(struct wiphy *wiphy,
			 struct net_device *ndev,
			 struct cfg80211_external_auth_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4BufLen;
	struct PARAM_EXTERNAL_AUTH auth;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo)
		DBGLOG(REQ, WARN,
		       "SAE-confirm failed with invalid prGlueInfo\n");

	COPY_MAC_ADDR(auth.bssid, params->bssid);
	auth.status = params->status;
	auth.ucBssIdx = wlanGetBssIdx(ndev);
	rStatus = kalIoctl(prGlueInfo, wlanoidExternalAuthDone, (void *)&auth,
			   sizeof(auth), FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "SAE-confirm failed with: %d\n", rStatus);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to disconnect from
 *        currently connected ESS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_disconnect(struct wiphy *wiphy,
			    struct net_device *ndev, u16 reason_code)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetDisassociate, NULL,
			   0, FALSE, FALSE, TRUE, &u4BufLen,
			   ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to join an IBSS group
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_join_ibss(struct wiphy *wiphy,
			   struct net_device *ndev,
			   struct cfg80211_ibss_params *params)
{
	struct PARAM_SSID rNewSsid;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4ChnlFreq;	/* Store channel or frequency information */
	uint32_t u4BufLen = 0, u4SsidLen = 0;
	uint32_t rStatus;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* set channel */
	if (params->channel_fixed) {
		u4ChnlFreq = params->chandef.center_freq1;

		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetFrequency,
				&u4ChnlFreq, sizeof(u4ChnlFreq),
				FALSE, FALSE, FALSE, &u4BufLen,
				ucBssIndex);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	/* set SSID */
	if (params->ssid_len > PARAM_MAX_LEN_SSID)
		u4SsidLen = PARAM_MAX_LEN_SSID;
	else
		u4SsidLen = params->ssid_len;

	kalMemCopy(rNewSsid.aucSsid, params->ssid,
		   u4SsidLen);
	rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidSetSsid, (void *)&rNewSsid,
				sizeof(struct PARAM_SSID),
				FALSE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set SSID:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to leave from IBSS group
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_leave_ibss(struct wiphy *wiphy,
			    struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetDisassociate, NULL,
			   0, FALSE, FALSE, TRUE, &u4BufLen,
			   ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to configure
 *        WLAN power managemenet
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_power_mgmt(struct wiphy *wiphy,
			struct net_device *ndev, bool enabled, int timeout)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_POWER_MODE_ rPowerMode;
	uint8_t ucBssIndex = 0;
	struct BSS_INFO *prBssInfo;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo)
		return -EFAULT;

	if (prGlueInfo->prAdapter->fgEnDbgPowerMode) {
		DBGLOG(REQ, WARN,
			"Force power mode enabled, ignore: %d\n", enabled);
		return 0;
	}

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
		ucBssIndex);
	if (!prBssInfo)
		return -EINVAL;

	DBGLOG(REQ, INFO, "%d: enabled=%d, timeout=%d, fgTIMPresend=%d\n",
	       ucBssIndex, enabled, timeout,
	       prBssInfo->fgTIMPresent);

	if (enabled) {
		if (prBssInfo->eConnectionState
			== MEDIA_STATE_CONNECTED &&
		    !prBssInfo->fgTIMPresent)
			return -EFAULT;

		if (timeout == -1)
			rPowerMode.ePowerMode = Param_PowerModeFast_PSP;
		else
			rPowerMode.ePowerMode = Param_PowerModeMAX_PSP;
	} else {
		rPowerMode.ePowerMode = Param_PowerModeCAM;
	}

	rPowerMode.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSet802dot11PowerSaveProfile,
			   &rPowerMode, sizeof(struct PARAM_POWER_MODE_),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set_power_mgmt error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cache
 *        a PMKID for a BSSID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_pmksa(struct wiphy *wiphy,
		   struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_PMKID pmkid = {0};
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "mtk_cfg80211_set_pmksa " MACSTR " pmk\n",
		MAC2STR(pmksa->bssid));

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	COPY_MAC_ADDR(pmkid.arBSSID, pmksa->bssid);
	kalMemCopy(pmkid.arPMKID, pmksa->pmkid, IW_PMKID_LEN);
	pmkid.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, &pmkid,
			   sizeof(struct PARAM_PMKID),
			   FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to remove
 *        a cached PMKID for a BSSID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_del_pmksa(struct wiphy *wiphy,
			struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_PMKID pmkid;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "mtk_cfg80211_del_pmksa " MACSTR "\n",
		MAC2STR(pmksa->bssid));

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	COPY_MAC_ADDR(pmkid.arBSSID, pmksa->bssid);
	kalMemCopy(pmkid.arPMKID, pmksa->pmkid, IW_PMKID_LEN);
	pmkid.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidDelPmkid, &pmkid,
			   sizeof(struct PARAM_PMKID),
			   FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to flush
 *        all cached PMKID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_flush_pmksa(struct wiphy *wiphy,
			     struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidFlushPmkid, NULL, 0,
			   FALSE, FALSE, FALSE, &u4BufLen,
			   ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "flush pmkid error:%x\n", rStatus);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting the rekey data
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_rekey_data(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_gtk_rekey_data *data)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen;
	struct PARAM_GTK_REKEY_DATA *prGtkData;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rslt = -EINVAL;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = 0;
	struct BSS_INFO *prBssInfo;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(dev);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, ucBssIndex);
	if (!prBssInfo)
		return -EINVAL;

	prGtkData =
		(struct PARAM_GTK_REKEY_DATA *) kalMemAlloc(sizeof(
				struct PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);

	if (!prGtkData)
		return WLAN_STATUS_SUCCESS;

	DBGLOG(RSN, INFO, "ucBssIndex = %d, size(%d)\n",
		ucBssIndex,
		(uint32_t) sizeof(struct cfg80211_gtk_rekey_data));

	DBGLOG(RSN, TRACE, "kek\n");
	DBGLOG_MEM8(RSN, TRACE, (uint8_t *)data->kek,
		    NL80211_KEK_LEN);
	DBGLOG(RSN, TRACE, "kck\n");
	DBGLOG_MEM8(RSN, TRACE, (uint8_t *)data->kck,
		    NL80211_KCK_LEN);
	DBGLOG(RSN, TRACE, "replay count\n");
	DBGLOG_MEM8(RSN, TRACE, (uint8_t *)data->replay_ctr,
		    NL80211_REPLAY_CTR_LEN);


#if 0
	kalMemCopy(prGtkData, data, sizeof(*data));
#else
	kalMemCopy(prGtkData->aucKek, data->kek, NL80211_KEK_LEN);
	kalMemCopy(prGtkData->aucKck, data->kck, NL80211_KCK_LEN);
	kalMemCopy(prGtkData->aucReplayCtr, data->replay_ctr,
		   NL80211_REPLAY_CTR_LEN);
#endif

	prGtkData->ucBssIndex = ucBssIndex;

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	prGtkData->u4Proto = NL80211_WPA_VERSION_2;
	if (prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_WPA)
		prGtkData->u4Proto = NL80211_WPA_VERSION_1;

	if (GET_SELECTOR_TYPE(prBssInfo->u4RsnSelectedPairwiseCipher) ==
			    CIPHER_SUITE_TKIP)
		prGtkData->u4PairwiseCipher = BIT(3);
	else if (GET_SELECTOR_TYPE(prBssInfo->u4RsnSelectedPairwiseCipher) ==
			    CIPHER_SUITE_CCMP)
		prGtkData->u4PairwiseCipher = BIT(4);
	else {
		kalMemFree(prGtkData, VIR_MEM_TYPE,
			   sizeof(struct PARAM_GTK_REKEY_DATA));
		return 0;
	}

	if (GET_SELECTOR_TYPE(prBssInfo->u4RsnSelectedGroupCipher) ==
			    CIPHER_SUITE_TKIP)
		prGtkData->u4GroupCipher = BIT(3);
	else if (GET_SELECTOR_TYPE(prBssInfo->u4RsnSelectedGroupCipher) ==
			    CIPHER_SUITE_CCMP)
		prGtkData->u4GroupCipher = BIT(4);
	else {
		kalMemFree(prGtkData, VIR_MEM_TYPE,
			   sizeof(struct PARAM_GTK_REKEY_DATA));
		return 0;
	}

	prGtkData->u4KeyMgmt = prBssInfo->u4RsnSelectedAKMSuite;
	prGtkData->u4MgmtGroupCipher = 0;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetGtkRekeyData, prGtkData,
				sizeof(struct PARAM_GTK_REKEY_DATA),
				FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "set GTK rekey data error:%x\n",
		       rStatus);
	else
		i4Rslt = 0;

	kalMemFree(prGtkData, VIR_MEM_TYPE,
		   sizeof(struct PARAM_GTK_REKEY_DATA));

	return i4Rslt;
}

void mtk_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
				      IN struct wireless_dev *wdev,
				      IN u16 frame_type,
				      IN bool reg)
{
#if 0
	struct MSG_P2P_MGMT_FRAME_REGISTER *prMgmtFrameRegister =
		(struct MSG_P2P_MGMT_FRAME_REGISTER *) NULL;
#endif
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;

	do {

		DBGLOG(INIT, TRACE, "mtk_cfg80211_mgmt_frame_register\n");

		WIPHY_PRIV(wiphy, prGlueInfo);

		switch (frame_type) {
		case MAC_FRAME_PROBE_REQ:
			if (reg) {
				prGlueInfo->u4OsMgmtFrameFilter |=
					PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(INIT, TRACE,
					"Open packet filer probe request\n");
			} else {
				prGlueInfo->u4OsMgmtFrameFilter &=
					~PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(INIT, TRACE,
					"Close packet filer probe request\n");
			}
			break;
		case MAC_FRAME_ACTION:
			if (reg) {
				prGlueInfo->u4OsMgmtFrameFilter |=
					PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(INIT, TRACE,
					"Open packet filer action frame.\n");
			} else {
				prGlueInfo->u4OsMgmtFrameFilter &=
					~PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(INIT, TRACE,
					"Close packet filer action frame.\n");
			}
			break;
		default:
			DBGLOG(INIT, TRACE,
				"Ask frog to add code for mgmt:%x\n",
				frame_type);
			break;
		}

		if (prGlueInfo->prAdapter != NULL) {

			set_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT,
				&prGlueInfo->ulFlag);

			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);

			if (in_interrupt())
				DBGLOG(INIT, TRACE,
						"It is in interrupt level\n");
		}
#if 0

		prMgmtFrameRegister =
			(struct MSG_P2P_MGMT_FRAME_REGISTER *) cnmMemAlloc(
				prGlueInfo->prAdapter, RAM_TYPE_MSG,
				sizeof(struct MSG_P2P_MGMT_FRAME_REGISTER));

		if (prMgmtFrameRegister == NULL) {
			ASSERT(FALSE);
			break;
		}

		prMgmtFrameRegister->rMsgHdr.eMsgId =
			MID_MNY_P2P_MGMT_FRAME_REGISTER;

		prMgmtFrameRegister->u2FrameType = frame_type;
		prMgmtFrameRegister->fgIsRegister = reg;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
			    (struct MSG_HDR *) prMgmtFrameRegister,
			    MSG_SEND_METHOD_BUF);

#endif

	} while (FALSE);

}				/* mtk_cfg80211_mgmt_frame_register */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to stay on a
 *        specified channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_remain_on_channel(struct wiphy *wiphy,
		   struct wireless_dev *wdev,
		   struct ieee80211_channel *chan, unsigned int duration,
		   u64 *cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Rslt = -EINVAL;
	struct MSG_REMAIN_ON_CHANNEL *prMsgChnlReq =
		(struct MSG_REMAIN_ON_CHANNEL *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		    || (chan == NULL)
		    || (cookie == NULL)) {
			break;
		}

		WIPHY_PRIV(wiphy, prGlueInfo);
		ASSERT(prGlueInfo);

		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;

		*cookie = prGlueInfo->u8Cookie++;

		prMsgChnlReq = cnmMemAlloc(prGlueInfo->prAdapter,
			   RAM_TYPE_MSG, sizeof(struct MSG_REMAIN_ON_CHANNEL));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgChnlReq->rMsgHdr.eMsgId =
			MID_MNY_AIS_REMAIN_ON_CHANNEL;
		prMsgChnlReq->u8Cookie = *cookie;
		prMsgChnlReq->u4DurationMs = duration;
		prMsgChnlReq->eReqType = CH_REQ_TYPE_ROC;
		prMsgChnlReq->ucChannelNum = nicFreq2ChannelNum(
				chan->center_freq * 1000);

		switch (chan->band) {
		case KAL_BAND_2GHZ:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		case KAL_BAND_5GHZ:
			prMsgChnlReq->eBand = BAND_5G;
			break;
#if (CFG_SUPPORT_WIFI_6G == 1)
		case KAL_BAND_6GHZ:
			prMsgChnlReq->eBand = BAND_6G;
			break;
#endif
		default:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		}

		prMsgChnlReq->eSco = CHNL_EXT_SCN;

		prMsgChnlReq->ucBssIdx = ucBssIndex;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgChnlReq, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cancel staying
 *        on a specified channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_cancel_remain_on_channel(
	struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Rslt = -EINVAL;
	struct MSG_CANCEL_REMAIN_ON_CHANNEL *prMsgChnlAbort =
		(struct MSG_CANCEL_REMAIN_ON_CHANNEL *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		   ) {
			break;
		}

		WIPHY_PRIV(wiphy, prGlueInfo);
		ASSERT(prGlueInfo);

		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;

		prMsgChnlAbort =
			cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
			    sizeof(struct MSG_CANCEL_REMAIN_ON_CHANNEL));

		if (prMsgChnlAbort == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgChnlAbort->rMsgHdr.eMsgId =
			MID_MNY_AIS_CANCEL_REMAIN_ON_CHANNEL;
		prMsgChnlAbort->u8Cookie = cookie;

		prMsgChnlAbort->ucBssIdx = ucBssIndex;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgChnlAbort, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

int _mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
		struct wireless_dev *wdev, struct ieee80211_channel *chan,
		bool offchan, unsigned int wait, const u8 *buf, size_t len,
		bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;
	int32_t i4Rslt = -EINVAL;
	struct MSG_MGMT_TX_REQUEST *prMsgTxReq =
			(struct MSG_MGMT_TX_REQUEST *) NULL;
	struct MSDU_INFO *prMgmtFrame = (struct MSDU_INFO *) NULL;
	uint8_t *pucFrameBuf = (uint8_t *) NULL;
	uint64_t *pu8GlCookie = (uint64_t *) NULL;

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (cookie == NULL))
			break;

		WIPHY_PRIV(wiphy, prGlueInfo);
		ASSERT(prGlueInfo);

		*cookie = prGlueInfo->u8Cookie++;

		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_MGMT_TX_REQUEST));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		if (offchan) {
			prMsgTxReq->fgIsOffChannel = TRUE;

			kalChannelFormatSwitch(NULL, chan,
					&prMsgTxReq->rChannelInfo);
			kalChannelScoSwitch(NL80211_CHAN_NO_HT,
					&prMsgTxReq->eChnlExt);
		} else {
			prMsgTxReq->fgIsOffChannel = FALSE;
		}

		if (wait)
			prMsgTxReq->u4Duration = wait;
		else
			prMsgTxReq->u4Duration = 0;

		if (no_cck)
			prMsgTxReq->fgNoneCckRate = TRUE;
		else
			prMsgTxReq->fgNoneCckRate = FALSE;

		if (dont_wait_for_ack)
			prMsgTxReq->fgIsWaitRsp = FALSE;
		else
			prMsgTxReq->fgIsWaitRsp = TRUE;

		prMgmtFrame = cnmMgtPktAlloc(prGlueInfo->prAdapter,
				(int32_t) (len + sizeof(uint64_t)
				+ MAC_TX_RESERVED_FIELD));
		prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
		if (prMsgTxReq->prMgmtMsduInfo == NULL) {
			/* ASSERT(FALSE); */
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgTxReq->u8Cookie = *cookie;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_AIS_MGMT_TX;
		prMsgTxReq->ucBssIdx = wlanGetBssIdx(wdev->netdev);

		pucFrameBuf =
			(uint8_t *)
			((unsigned long) prMgmtFrame->prPacket
			+ MAC_TX_RESERVED_FIELD);
		pu8GlCookie =
			(uint64_t *)
			((unsigned long) prMgmtFrame->prPacket
			+ (unsigned long) len
			+ MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, buf, len);

		*pu8GlCookie = *cookie;

		prMgmtFrame->u2FrameLength = len;
		prMgmtFrame->ucBssIndex = wlanGetBssIdx(wdev->netdev);

#define TEMP_LOG_TEMPLATE "bssIdx: %d, band: %d, chan: %d, offchan: %d, " \
		"wait: %d, len: %d, no_cck: %d, dont_wait_for_ack: %d, " \
		"cookie: 0x%llx\n"
		DBGLOG(P2P, INFO, TEMP_LOG_TEMPLATE,
				prMsgTxReq->ucBssIdx,
				prMsgTxReq->rChannelInfo.eBand,
				prMsgTxReq->rChannelInfo.ucChannelNum,
				prMsgTxReq->fgIsOffChannel,
				prMsgTxReq->u4Duration,
				prMsgTxReq->prMgmtMsduInfo->u2FrameLength,
				prMsgTxReq->fgNoneCckRate,
				prMsgTxReq->fgIsWaitRsp,
				prMsgTxReq->u8Cookie);
#undef TEMP_LOG_TEMPLATE

		mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgTxReq,
			MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	if ((i4Rslt != 0) && (prMsgTxReq != NULL)) {
		if (prMsgTxReq->prMgmtMsduInfo != NULL)
			cnmMgtPktFree(prGlueInfo->prAdapter,
				prMsgTxReq->prMgmtMsduInfo);

		cnmMemFree(prGlueInfo->prAdapter, prMsgTxReq);
	}

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to send a management frame
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			struct cfg80211_mgmt_tx_params *params,
			u64 *cookie)
{
	if (params == NULL)
		return -EINVAL;

	return _mtk_cfg80211_mgmt_tx(wiphy, wdev, params->chan,
			params->offchan, params->wait, params->buf, params->len,
			params->no_cck, params->dont_wait_for_ack, cookie);
}
#else
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
		struct wireless_dev *wdev, struct ieee80211_channel *channel,
		bool offchan, unsigned int wait, const u8 *buf, size_t len,
		bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	return _mtk_cfg80211_mgmt_tx(wiphy, wdev, channel, offchan, wait, buf,
			len, no_cck, dont_wait_for_ack, cookie);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cancel the wait time
 *        from transmitting a management frame on another channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
		struct wireless_dev *wdev, u64 cookie)
{
	int32_t i4Rslt = -EINVAL;
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;
	struct MSG_CANCEL_TX_WAIT_REQUEST *prMsgCancelTxWait =
			(struct MSG_CANCEL_TX_WAIT_REQUEST *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		ASSERT(wiphy);

		WIPHY_PRIV(wiphy, prGlueInfo);
		ASSERT(prGlueInfo);

		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;

		DBGLOG(P2P, INFO, "cookie: 0x%llx, ucBssIndex = %d\n",
			cookie, ucBssIndex);


		prMsgCancelTxWait = cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_CANCEL_TX_WAIT_REQUEST));

		if (prMsgCancelTxWait == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgCancelTxWait->rMsgHdr.eMsgId =
				MID_MNY_AIS_MGMT_TX_CANCEL_WAIT;
		prMsgCancelTxWait->u8Cookie = cookie;
		prMsgCancelTxWait->ucBssIdx = ucBssIndex;

		mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgCancelTxWait,
			MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

#ifdef CONFIG_NL80211_TESTMODE
#if CFG_SUPPORT_WAPI
int mtk_cfg80211_testmode_set_key_ext(IN struct wiphy
				      *wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct NL80211_DRIVER_SET_KEY_EXTS *prParams =
		(struct NL80211_DRIVER_SET_KEY_EXTS *) NULL;
	struct iw_encode_exts *prIWEncExt = (struct iw_encode_exts
					     *)NULL;
	uint32_t rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	uint32_t u4BufLen = 0;
	const uint8_t aucBCAddr[] = BC_MAC_ADDR;
	uint8_t ucBssIndex = 0;

	struct PARAM_KEY *prWpiKey;
	uint8_t *keyStructBuf;

	ASSERT(wiphy);
	WIPHY_PRIV(wiphy, prGlueInfo);

	if (len < sizeof(struct NL80211_DRIVER_SET_KEY_EXTS)) {
		DBGLOG(REQ, ERROR, "len [%d] is invalid!\n", len);
		return -EINVAL;
	}
	if (data == NULL || len == 0) {
		DBGLOG(INIT, TRACE, "%s data or len is invalid\n", __func__);
		return -EINVAL;
	}

	keyStructBuf = kalMemAlloc(KEY_BUF_SIZE, VIR_MEM_TYPE);
	if (keyStructBuf == NULL) {
		DBGLOG(REQ, ERROR, "alloc key buffer fail\n");
		return -ENOMEM;
	}
	kalMemSet(keyStructBuf, 0, KEY_BUF_SIZE);
	prWpiKey = (struct PARAM_KEY *) keyStructBuf;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		ucBssIndex = 0;

	prParams = (struct NL80211_DRIVER_SET_KEY_EXTS *) data;
	prIWEncExt = (struct iw_encode_exts *)&prParams->ext;

	if (prIWEncExt->alg == IW_ENCODE_ALG_SMS4) {
		/* KeyID */
		prWpiKey->u4KeyIndex = prParams->key_index;
		prWpiKey->u4KeyIndex--;
		if (prWpiKey->u4KeyIndex > 1) {
			/* key id is out of range */
			/* printk(KERN_INFO "[wapi] add key error:
			 * key_id invalid %d\n", prWpiKey->ucKeyID);
			 */
			fgIsValid = -EINVAL;
			goto freeBuf;
		}

		if (prIWEncExt->key_len != 32) {
			/* key length not valid */
			/* printk(KERN_INFO "[wapi] add key error:
			 * key_len invalid %d\n", prIWEncExt->key_len);
			 */
			fgIsValid = -EINVAL;
			goto freeBuf;
		}
		prWpiKey->u4KeyLength = prIWEncExt->key_len;

		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_SET_TX_KEY &&
		    !(prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY)) {
			/* WAI seems set the STA group key with
			 * IW_ENCODE_EXT_SET_TX_KEY !!!!
			 * Ignore the group case
			 */
			prWpiKey->u4KeyIndex |= BIT(30);
			prWpiKey->u4KeyIndex |= BIT(31);
			/* BSSID */
			memcpy(prWpiKey->arBSSID, prIWEncExt->addr, 6);
		} else {
			COPY_MAC_ADDR(prWpiKey->arBSSID, aucBCAddr);
		}

		/* PN */
		/* memcpy(prWpiKey->rKeyRSC, prIWEncExt->tx_seq,
		 * IW_ENCODE_SEQ_MAX_SIZE * 2);
		 */

		memcpy(prWpiKey->aucKeyMaterial, prIWEncExt->key, 32);

		prWpiKey->u4Length = sizeof(struct PARAM_KEY);
		prWpiKey->ucBssIdx = ucBssIndex;
		prWpiKey->ucCipher = CIPHER_SUITE_WPI;

		rstatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, prWpiKey,
				sizeof(struct PARAM_KEY),
				FALSE, FALSE, TRUE, &u4BufLen);

		if (rstatus != WLAN_STATUS_SUCCESS) {
			/* printk(KERN_INFO "[wapi] add key error:%x\n",
			 * rStatus);
			 */
			fgIsValid = -EFAULT;
		}

	}

freeBuf:
	if (keyStructBuf)
		kalMemFree(keyStructBuf, VIR_MEM_TYPE, KEY_BUF_SIZE);
	return fgIsValid;
}
#endif

int
mtk_cfg80211_testmode_get_sta_statistics(IN struct wiphy
		*wiphy, IN void *data, IN int len,
		IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen;
	uint32_t u4LinkScore;
	uint32_t u4TotalError;
	uint32_t u4TxExceedThresholdCount;
	uint32_t u4TxTotalCount;

	struct NL80211_DRIVER_GET_STA_STATISTICS_PARAMS *prParams =
			NULL;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	if (data && len)
		prParams = (struct NL80211_DRIVER_GET_STA_STATISTICS_PARAMS
			    *) data;

	if (prParams == NULL) {
		DBGLOG(QM, ERROR, "prParams is NULL, data=%p, len=%d\n",
		       data, len);
		return -EINVAL;
	} /* else if (prParams->aucMacAddr == NULL) {
		DBGLOG(QM, ERROR,
		       "prParams->aucMacAddr is NULL, data=%p, len=%d\n",
		       data, len);
		return -EINVAL;
	}
	*/

	skb = cfg80211_testmode_alloc_reply_skb(wiphy,
				sizeof(struct PARAM_GET_STA_STATISTICS) + 1);
	if (!skb) {
		DBGLOG(QM, ERROR, "allocate skb failed:%x\n", rStatus);
		return -ENOMEM;
	}

	kalMemZero(&rQueryStaStatistics,
		   sizeof(rQueryStaStatistics));
	COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr,
		      prParams->aucMacAddr);
	rQueryStaStatistics.ucReadClear = TRUE;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStaStatistics,
			   &rQueryStaStatistics, sizeof(rQueryStaStatistics),
			   TRUE, FALSE, TRUE, &u4BufLen);

	/* Calcute Link Score */
	u4TxExceedThresholdCount =
		rQueryStaStatistics.u4TxExceedThresholdCount;
	u4TxTotalCount = rQueryStaStatistics.u4TxTotalCount;
	u4TotalError = rQueryStaStatistics.u4TxFailCount +
		       rQueryStaStatistics.u4TxLifeTimeoutCount;

	/* u4LinkScore 10~100 , ExceedThreshold ratio 0~90 only
	 * u4LinkScore 0~9    , Drop packet ratio 0~9 and all packets exceed
	 * threshold
	 */
	if (u4TxTotalCount) {
		if (u4TxExceedThresholdCount <= u4TxTotalCount)
			u4LinkScore = (90 - ((u4TxExceedThresholdCount * 90)
							/ u4TxTotalCount));
		else
			u4LinkScore = 0;
	} else {
		u4LinkScore = 90;
	}

	u4LinkScore += 10;

	if (u4LinkScore == 10) {
		if (u4TotalError <= u4TxTotalCount)
			u4LinkScore = (10 - ((u4TotalError * 10)
							/ u4TxTotalCount));
		else
			u4LinkScore = 0;

	}

	if (u4LinkScore > 100)
		u4LinkScore = 100;
	{
		u8 __tmp = 0;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_INVALID, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u8 __tmp = NL80211_DRIVER_TESTMODE_VERSION;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_VERSION, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN,
	    prParams->aucMacAddr) < 0))
		goto nla_put_failure;
	{
		u32 __tmp = u4LinkScore;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.u4Flag;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_FLAG, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4EnqueueCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_ENQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4DequeueCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_DEQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4EnqueueStaCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_STA_ENQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4DequeueStaCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_STA_DEQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.IsrCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrPassCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_PASS_CNT,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.TaskIsrCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_TASK_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrAbnormalCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_AB_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrSoftWareCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_SW_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrTxCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_TX_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrRxCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_RX_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	/* FW part STA link status */
	{
		u8 __tmp = rQueryStaStatistics.ucPer;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_PER, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u8 __tmp = rQueryStaStatistics.ucRcpi;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_RSSI, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4PhyMode;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_PHY_MODE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u16 __tmp = rQueryStaStatistics.u2LinkSpeed;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_TX_RATE, sizeof(u16),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxFailCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxLifeTimeoutCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageAirTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Driver part link status */
	{
		u32 __tmp = rQueryStaStatistics.u4TxTotalCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxExceedThresholdCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageProcessTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxMaxTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_MAX_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageHifTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_AVG_HIF_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxMaxHifTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_MAX_HIF_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Network counter */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceEmptyCount),
	    rQueryStaStatistics.au4TcResourceEmptyCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_NO_TC_ARRAY,
	    sizeof(rQueryStaStatistics.au4DequeueNoTcResource),
	    rQueryStaStatistics.au4DequeueNoTcResource) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_RB_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceBackCount),
	    rQueryStaStatistics.au4TcResourceBackCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_USED_TC_PGCT_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceUsedPageCount),
	    rQueryStaStatistics.au4TcResourceUsedPageCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_WANTED_TC_PGCT_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceWantedPageCount),
	    rQueryStaStatistics.au4TcResourceWantedPageCount) < 0))
		goto nla_put_failure;

	/* Sta queue length */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcQueLen),
	    rQueryStaStatistics.au4TcQueLen) < 0))
		goto nla_put_failure;

	/* Global QM counter */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcAverageQueLen),
	    rQueryStaStatistics.au4TcAverageQueLen) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcCurrentQueLen),
	    rQueryStaStatistics.au4TcCurrentQueLen) < 0))
		goto nla_put_failure;

	/* Reserved field */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,
	    sizeof(rQueryStaStatistics.au4Reserved),
	    rQueryStaStatistics.au4Reserved) < 0))
		goto nla_put_failure;

	return cfg80211_testmode_reply(skb);

nla_put_failure:
	/* nal_put_skb_fail */
	kfree_skb(skb);
	return -EFAULT;
}

int
mtk_cfg80211_testmode_get_link_detection(IN struct wiphy
		*wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len,
		IN struct GLUE_INFO *prGlueInfo)
{

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Status = -EINVAL;
	uint32_t u4BufLen;
	uint8_t u1buf = 0;
	uint32_t i = 0;
	uint32_t arBugReport[sizeof(struct EVENT_BUG_REPORT)];
	struct PARAM_802_11_STATISTICS_STRUCT rStatistics;
	struct EVENT_BUG_REPORT *prBugReport;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	prBugReport = (struct EVENT_BUG_REPORT *) kalMemAlloc(
			      sizeof(struct EVENT_BUG_REPORT), VIR_MEM_TYPE);
	if (!prBugReport) {
		DBGLOG(QM, TRACE, "%s allocate prBugReport failed\n",
		       __func__);
		return -ENOMEM;
	}
	skb = cfg80211_testmode_alloc_reply_skb(wiphy,
			sizeof(struct PARAM_802_11_STATISTICS_STRUCT) +
			sizeof(struct EVENT_BUG_REPORT) + 1);

	if (!skb) {
		kalMemFree(prBugReport, VIR_MEM_TYPE,
			   sizeof(struct EVENT_BUG_REPORT));
		DBGLOG(QM, TRACE, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	kalMemZero(&rStatistics, sizeof(rStatistics));
	kalMemZero(prBugReport, sizeof(struct EVENT_BUG_REPORT));
	kalMemZero(arBugReport, sizeof(struct EVENT_BUG_REPORT));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStatistics,
			   &rStatistics, sizeof(rStatistics),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "query statistics error:%x\n", rStatus);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryBugReport,
			   prBugReport, sizeof(struct EVENT_BUG_REPORT),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "query statistics error:%x\n", rStatus);

	kalMemCopy(arBugReport, prBugReport,
		   sizeof(struct EVENT_BUG_REPORT));

	rStatistics.u4RstReason = glGetRstReason();
	rStatistics.u8RstTime = u8ResetTime;
	rStatistics.u4RoamFailCnt = prGlueInfo->u4RoamFailCnt;
	rStatistics.u8RoamFailTime = prGlueInfo->u8RoamFailTime;
	rStatistics.u2TxDoneDelayIsARP =
		prGlueInfo->fgTxDoneDelayIsARP;
	rStatistics.u4ArriveDrvTick = prGlueInfo->u4ArriveDrvTick;
	rStatistics.u4EnQueTick = prGlueInfo->u4EnQueTick;
	rStatistics.u4DeQueTick = prGlueInfo->u4DeQueTick;
	rStatistics.u4LeaveDrvTick = prGlueInfo->u4LeaveDrvTick;
	rStatistics.u4CurrTick = prGlueInfo->u4CurrTick;
	rStatistics.u8CurrTime = prGlueInfo->u8CurrTime;

	if (!NLA_PUT_U8(skb, NL80211_TESTMODE_LINK_INVALID, &u1buf))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_FAIL_CNT,
			 &rStatistics.rFailedCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_RETRY_CNT,
			 &rStatistics.rRetryCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb,
			 NL80211_TESTMODE_LINK_TX_MULTI_RETRY_CNT,
			 &rStatistics.rMultipleRetryCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_ACK_FAIL_CNT,
			 &rStatistics.rACKFailureCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_FCS_ERR_CNT,
			 &rStatistics.rFCSErrorCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_CNT,
			 &rStatistics.rTransmittedFragmentCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_RX_CNT,
			 &rStatistics.rReceivedFragmentCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_RST_REASON,
			 &rStatistics.u4RstReason))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_RST_TIME,
			 &rStatistics.u8RstTime))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_ROAM_FAIL_TIMES,
			 &rStatistics.u4RoamFailCnt))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_ROAM_FAIL_TIME,
			 &rStatistics.u8RoamFailTime))
		goto nla_put_failure;

	if (!NLA_PUT_U8(skb,
			NL80211_TESTMODE_LINK_TX_DONE_DELAY_IS_ARP,
			&rStatistics.u2TxDoneDelayIsARP))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_ARRIVE_DRV_TICK,
			 &rStatistics.u4ArriveDrvTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_ENQUE_TICK,
			 &rStatistics.u4EnQueTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_DEQUE_TICK,
			 &rStatistics.u4DeQueTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_LEAVE_DRV_TICK,
			 &rStatistics.u4LeaveDrvTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_CURR_TICK,
			 &rStatistics.u4CurrTick))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_CURR_TIME,
			 &rStatistics.u8CurrTime))
		goto nla_put_failure;

	for (i = 0;
	     i < sizeof(struct EVENT_BUG_REPORT) / sizeof(uint32_t);
	     i++) {
		if (!NLA_PUT_U32(skb, i + NL80211_TESTMODE_LINK_DETECT_NUM,
				 &arBugReport[i]))
			goto nla_put_failure;
	}

	i4Status = cfg80211_testmode_reply(skb);
	kalMemFree(prBugReport, VIR_MEM_TYPE,
		   sizeof(struct EVENT_BUG_REPORT));
	return i4Status;

nla_put_failure:
	/* nal_put_skb_fail */
	kfree_skb(skb);
	kalMemFree(prBugReport, VIR_MEM_TYPE,
		   sizeof(struct EVENT_BUG_REPORT));
	return -EFAULT;
}

int mtk_cfg80211_testmode_sw_cmd(IN struct wiphy *wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct NL80211_DRIVER_SW_CMD_PARAMS *prParams =
		(struct NL80211_DRIVER_SW_CMD_PARAMS *) NULL;
	uint32_t rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	uint32_t u4SetInfoLen = 0;

	ASSERT(wiphy);

	WIPHY_PRIV(wiphy, prGlueInfo);

#if 0
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (len < sizeof(struct NL80211_DRIVER_SW_CMD_PARAMS)) {
		DBGLOG(REQ, ERROR, "len [%d] is invalid!\n", len);
		return -EINVAL;
	}

	if (data && len)
		prParams = (struct NL80211_DRIVER_SW_CMD_PARAMS *) data;

	if (prParams) {
		if (prParams->set == 1) {
			rstatus = kalIoctl(prGlueInfo,
				   (PFN_OID_HANDLER_FUNC) wlanoidSetSwCtrlWrite,
				   &prParams->adr, (uint32_t) 8,
				   FALSE, FALSE, TRUE, &u4SetInfoLen);
		}
	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

static int mtk_wlan_cfg_testmode_cmd(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
				     void *data, int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct NL80211_DRIVER_TEST_MODE_PARAMS *prParams = NULL;
	int32_t i4Status;

	ASSERT(wiphy);

	if (len < sizeof(struct NL80211_DRIVER_TEST_MODE_PARAMS)) {
		DBGLOG(REQ, ERROR, "len [%d] is invalid!\n", len);
		return -EINVAL;
	}
	if (!data || !len) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_cmd null data\n");
		return -EINVAL;
	}

	if (!wiphy) {
		DBGLOG(REQ, ERROR,
		       "mtk_cfg80211_testmode_cmd null wiphy\n");
		return -EINVAL;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	prParams = (struct NL80211_DRIVER_TEST_MODE_PARAMS *)data;

	/* Clear the version byte */
	prParams->index = prParams->index & ~BITS(24, 31);
	DBGLOG(INIT, TRACE, "params index=%x\n", prParams->index);

	switch (prParams->index) {
	case TESTMODE_CMD_ID_SW_CMD:	/* SW cmd */
		i4Status = mtk_cfg80211_testmode_sw_cmd(wiphy,
				wdev, data, len);
		break;
	case TESTMODE_CMD_ID_WAPI:	/* WAPI */
#if CFG_SUPPORT_WAPI
		i4Status = mtk_cfg80211_testmode_set_key_ext(wiphy,
				wdev, data, len);
#endif
		break;
	case 0x10:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy,
				data, len, prGlueInfo);
		break;
	case 0x20:
		i4Status = mtk_cfg80211_testmode_get_link_detection(wiphy,
				wdev, data, len, prGlueInfo);
		break;
	case TESTMODE_CMD_ID_STR_CMD:
		i4Status = mtk_cfg80211_process_str_cmd(wiphy,
				wdev, data, len);
		break;

	default:
		i4Status = -EINVAL;
		break;
	}

	if (i4Status != 0)
		DBGLOG(REQ, TRACE, "prParams->index=%d, status=%d\n",
		       prParams->index, i4Status);

	return i4Status;
}

#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      void *data, int len)
{
	ASSERT(wdev);
	return mtk_wlan_cfg_testmode_cmd(wiphy, wdev, data, len);
}
#else
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy,
			      void *data, int len)
{
	return mtk_wlan_cfg_testmode_cmd(wiphy, NULL, data, len);
}
#endif
#endif

#if CFG_SUPPORT_SCHED_SCAN
int mtk_cfg80211_sched_scan_start(IN struct wiphy *wiphy,
			  IN struct net_device *ndev,
			  IN struct cfg80211_sched_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t i, j = 0, u4BufLen;
	struct PARAM_SCHED_SCAN_REQUEST *prSchedScanRequest;
	uint32_t num = 0;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	if (likely(request)) {
		scanlog_dbg(LOG_SCHED_SCAN_REQ_START_K2D, INFO, "ssid(%d)match(%d)ch(%u)f(%u)rssi(%d)\n",
		       request->n_ssids, request->n_match_sets,
		       request->n_channels, request->flags,
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
		       request->min_rssi_thold);
#else
		       request->rssi_thold);
#endif
	} else
		scanlog_dbg(LOG_SCHED_SCAN_REQ_START_K2D, INFO, "--> %s()\n",
			__func__);

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	if (prGlueInfo->prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo->prAdapter is NULL");
		return -EINVAL;
	}

#if CFG_SUPPORT_LOWLATENCY_MODE
	if (!prGlueInfo->prAdapter->fgEnCfg80211Scan
	    && MEDIA_STATE_CONNECTED
	    == kalGetMediaStateIndicated(prGlueInfo, ucBssIndex)) {
		DBGLOG(REQ, INFO,
		       "sched_scan_start LowLatency reject scan\n");
		return -EBUSY;
	}
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

	if (prGlueInfo->prSchedScanRequest != NULL) {
		DBGLOG(SCN, ERROR,
		       "GlueInfo->prSchedScanRequest != NULL\n");
		return -EBUSY;
	} else if (request == NULL) {
		DBGLOG(SCN, ERROR, "request == NULL\n");
		return -EINVAL;
	} else if (!request->n_match_sets) {
		/* invalid scheduled scan request */
		DBGLOG(SCN, ERROR,
		       "No match sets. No need to do sched scan\n");
		return -EINVAL;
	} else if (request->n_match_sets >
		   CFG_SCAN_SSID_MATCH_MAX_NUM) {
		DBGLOG(SCN, WARN, "request->n_match_sets(%d) > %d\n",
		       request->n_match_sets,
		       CFG_SCAN_SSID_MATCH_MAX_NUM);
		return -EINVAL;
	} else if (request->n_ssids >
		   CFG_SCAN_HIDDEN_SSID_MAX_NUM) {
		DBGLOG(SCN, WARN, "request->n_ssids(%d) > %d\n",
		       request->n_ssids, CFG_SCAN_HIDDEN_SSID_MAX_NUM);
		return -EINVAL;
	}

	prSchedScanRequest = (struct PARAM_SCHED_SCAN_REQUEST *)
		     kalMemAlloc(sizeof(struct PARAM_SCHED_SCAN_REQUEST),
								 VIR_MEM_TYPE);
	if (prSchedScanRequest == NULL) {
		DBGLOG(SCN, ERROR, "prSchedScanRequest kalMemAlloc fail\n");
		return -ENOMEM;
	}
	kalMemZero(prSchedScanRequest,
		   sizeof(struct PARAM_SCHED_SCAN_REQUEST));

	/* passed in the probe_reqs in active scans */
	if (request->ssids) {
		for (i = 0; i < request->n_ssids; i++) {
			DBGLOG(SCN, TRACE, "ssids : (%d)[%s]\n",
			       i, request->ssids[i].ssid);
			/* driver ignored the null ssid */
			if (request->ssids[i].ssid_len == 0
			    || request->ssids[i].ssid[0] == 0)
				DBGLOG(SCN, TRACE, "ignore null ssid(%d)\n", i);
			else {
				struct PARAM_SSID *prSsid;

				prSsid = &(prSchedScanRequest->arSsid[num]);
				COPY_SSID(prSsid->aucSsid, prSsid->u4SsidLen,
					  request->ssids[i].ssid,
					  request->ssids[i].ssid_len);
				num++;
			}
		}
	}
	prSchedScanRequest->u4SsidNum = num;
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
	prSchedScanRequest->i4MinRssiThold =
		request->min_rssi_thold;
#else
	prSchedScanRequest->i4MinRssiThold = request->rssi_thold;
#endif

	num = 0;
	if (request->match_sets) {
		for (i = 0; i < request->n_match_sets; i++) {
			DBGLOG(SCN, TRACE, "match : (%d)[%s]\n", i,
			       request->match_sets[i].ssid.ssid);
			/* driver ignored the null ssid */
			if (request->match_sets[i].ssid.ssid_len == 0
			    || request->match_sets[i].ssid.ssid[0] == 0)
				DBGLOG(SCN, TRACE, "ignore null ssid(%d)\n", i);
			else {
				struct PARAM_SSID *prSsid =
					&(prSchedScanRequest->arMatchSsid[num]);

				COPY_SSID(prSsid->aucSsid,
					  prSsid->u4SsidLen,
					  request->match_sets[i].ssid.ssid,
					  request->match_sets[i].ssid.ssid_len);
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
				prSchedScanRequest->ai4RssiThold[i] =
					request->match_sets[i].rssi_thold;
#else
				prSchedScanRequest->ai4RssiThold[i] =
					request->rssi_thold;
#endif
				num++;
			}
		}
	}
	prSchedScanRequest->u4MatchSsidNum = num;

	if (kalSchedScanParseRandomMac(ndev, request,
		prSchedScanRequest->aucRandomMac,
		prSchedScanRequest->aucRandomMacMask)) {
		prSchedScanRequest->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	}

	prSchedScanRequest->u4IELength = request->ie_len;
	if (request->ie_len > 0) {
		prSchedScanRequest->pucIE =
			kalMemAlloc(request->ie_len, VIR_MEM_TYPE);
		if (prSchedScanRequest->pucIE == NULL) {
			DBGLOG(SCN, ERROR, "pucIE kalMemAlloc fail\n");
		} else {
			kalMemZero(prSchedScanRequest->pucIE, request->ie_len);
			kalMemCopy(prSchedScanRequest->pucIE,
				   (uint8_t *)request->ie, request->ie_len);
		}
	}

#if KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE
	prSchedScanRequest->u2ScanInterval =
		(uint16_t) (request->scan_plans->interval);
#else
	prSchedScanRequest->u2ScanInterval = (uint16_t) (
			request->interval);
#endif

	/* 6G only need to scan PSC channel, transform channel list first*/
	for (i = 0; i < request->n_channels; i++) {
		uint32_t u4channel =
			nicFreq2ChannelNum(request->channels[i]->center_freq *
									1000);
		if (u4channel == 0) {
			DBGLOG(REQ, WARN, "Wrong Channel[%d] freq=%u\n",
			       i, request->channels[i]->center_freq);
			continue;
		}
		prSchedScanRequest->aucChannel[j].ucChannelNum = u4channel;
		switch ((request->channels[i])->band) {
		case KAL_BAND_2GHZ:
			prSchedScanRequest->aucChannel[j].ucBand = BAND_2G4;
			break;
		case KAL_BAND_5GHZ:
			prSchedScanRequest->aucChannel[j].ucBand = BAND_5G;
			break;
#if (CFG_SUPPORT_WIFI_6G == 1)
		case KAL_BAND_6GHZ:
			/* find out 6G PSC channel */
			if (((u4channel - 5) % 16) != 0)
				continue;

			prSchedScanRequest->aucChannel[j].ucBand = BAND_6G;
			break;
#endif
		default:
			DBGLOG(REQ, WARN, "UNKNOWN Band %d(chnl=%u)\n",
			       request->channels[i]->band, u4channel);
			prSchedScanRequest->aucChannel[j].ucBand = BAND_NULL;
			break;
		}
		j++;
	}
	prSchedScanRequest->ucChnlNum = j;

	prSchedScanRequest->ucBssIndex = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetStartSchedScan,
			   prSchedScanRequest,
			   sizeof(struct PARAM_SCHED_SCAN_REQUEST),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "scheduled scan error:%x\n", rStatus);
		kalMemFree(prSchedScanRequest->pucIE,
			VIR_MEM_TYPE, request->ie_len);
		kalMemFree(prSchedScanRequest,
			VIR_MEM_TYPE, sizeof(struct PARAM_SCHED_SCAN_REQUEST));
		return -EINVAL;
	}

	/* prSchedScanRequest is owned by oid now, don't free it */

	return 0;
}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy,
				 IN struct net_device *ndev,
				 IN u64 reqid)
#else
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy,
				 IN struct net_device *ndev)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	scanlog_dbg(LOG_SCHED_SCAN_REQ_STOP_K2D, INFO, "--> %s()\n", __func__);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prSchedScanRequest == NULL)
		return -EPERM; /* Operation not permitted */

	rStatus = kalIoctl(prGlueInfo, wlanoidSetStopSchedScan,
			   NULL, 0,
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_FAILURE) {
		DBGLOG(REQ, WARN, "scheduled scan error in IoCtl:%x\n",
		       rStatus);
		return 0;
	} else if (rStatus == WLAN_STATUS_RESOURCES) {
		DBGLOG(REQ, WARN, "scheduled scan error in Driver:%x\n",
		       rStatus);
		return -EINVAL;
	}

	return 0;
}
#endif /* CFG_SUPPORT_SCHED_SCAN */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for handling association request
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_assoc(struct wiphy *wiphy,
	       struct net_device *ndev, struct cfg80211_assoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
#if CFG_SUPPORT_PASSPOINT
	uint8_t *prDesiredIE = NULL;
#endif /* CFG_SUPPORT_PASSPOINT */
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	kalMemZero(arBssid, MAC_ADDR_LEN);
	SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, req->bss->bssid)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR
		       "] currently connected BSSID["
		       MACSTR "]\n",
		       MAC2STR(req->bss->bssid), MAC2STR(arBssid));
		return -ENOENT;
	}

	if (req->ie && req->ie_len > 0) {
#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE((uint8_t *) req->ie, req->ie_len,
					  (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctlByBssIdx(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE),
					   FALSE, FALSE, TRUE, &u4BufLen,
					   ucBssIndex);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, TRACE,
				 * ("[HS20] set HS20 assoc info error:%x\n",
				 * rStatus));
				 */
			}
		}
#endif /* CFG_SUPPORT_PASSPOINT */
	}

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetBssid,
			(void *)req->bss->bssid, MAC_ADDR_LEN,
			FALSE, FALSE, TRUE, &u4BufLen,
			ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set BSSID:%x\n", rStatus);
		return -EINVAL;
	}

	return 0;
}

#if CFG_SUPPORT_NFC_BEAM_PLUS

int mtk_cfg80211_testmode_get_scan_done(IN struct wiphy
					*wiphy, IN void *data, IN int len,
					IN struct GLUE_INFO *prGlueInfo)
{
	int32_t i4Status = -EINVAL;

#ifdef CONFIG_NL80211_TESTMODE
#define NL80211_TESTMODE_P2P_SCANDONE_INVALID 0
#define NL80211_TESTMODE_P2P_SCANDONE_STATUS 1

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t READY_TO_BEAM = 0;

	struct sk_buff *skb = NULL;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	skb = cfg80211_testmode_alloc_reply_skb(wiphy,
						sizeof(uint32_t));

	/* READY_TO_BEAM =
	 * (UINT_32)(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsGOInitialDone)
	 * &(!prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsScanRequest);
	 */
	READY_TO_BEAM = 1;
	/* DBGLOG(QM, TRACE,
	 * "NFC:GOInitialDone[%d] and P2PScanning[%d]\n",
	 * prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsGOInitialDone,
	 * prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsScanRequest));
	 */

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__,
		       rStatus);
		return -ENOMEM;
	}
	{
		u8 __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_INVALID,
		    sizeof(u8), &__tmp) < 0)) {
			kfree_skb(skb);
			return -EINVAL;
		}
	}
	{
		u32 __tmp = READY_TO_BEAM;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_STATUS,
		    sizeof(u32), &__tmp) < 0)) {
			kfree_skb(skb);
			return -EINVAL;
		}
	}

	i4Status = cfg80211_testmode_reply(skb);
#else
	DBGLOG(QM, WARN, "CONFIG_NL80211_TESTMODE not enabled\n");
#endif
	return i4Status;
}

#endif

#if CFG_SUPPORT_TDLS

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for changing a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_change_station(struct wiphy *wiphy,
			    struct net_device *ndev, const u8 *mac,
			    struct station_parameters *params)
{

	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_UPDATE rCmdUpdate;
	uint32_t rStatus;
	uint32_t u4BufLen, u4Temp;
	struct ADAPTER *prAdapter;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex = 0;
#if KERNEL_VERSION(6, 0, 0) <= CFG80211_VERSION_CODE
	struct link_station_parameters *prLinkParams =
		&(params->link_sta_params);
#else
	struct station_parameters *prLinkParams = params;
#endif

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (!prBssInfo || !params || !mac)
		return -EINVAL;

	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_AUTHORIZED))) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetAuthorized,
			(void *) mac, MAC_ADDR_LEN, FALSE,
			FALSE, FALSE, &u4BufLen);
	}

	if (prLinkParams->supported_rates == NULL)
		return 0;

	/* init */
	kalMemZero(&rCmdUpdate, sizeof(rCmdUpdate));
	kalMemCopy(rCmdUpdate.aucPeerMac, mac, 6);

	if (prLinkParams->supported_rates != NULL) {

		u4Temp = prLinkParams->supported_rates_len;
		if (u4Temp > CMD_PEER_UPDATE_SUP_RATE_MAX)
			u4Temp = CMD_PEER_UPDATE_SUP_RATE_MAX;
		kalMemCopy(rCmdUpdate.aucSupRate, prLinkParams->supported_rates,
			   u4Temp);
		rCmdUpdate.u2SupRateLen = u4Temp;
	}

	/*
	 * In supplicant, only recognize WLAN_EID_QOS 46, not 0xDD WMM
	 * So force to support UAPSD here.
	 */
	rCmdUpdate.UapsdBitmap = 0x0F;	/*params->uapsd_queues; */
	rCmdUpdate.UapsdMaxSp = 0;	/*params->max_sp; */

	rCmdUpdate.u2Capability = params->capability;

	if (params->ext_capab != NULL) {

		u4Temp = params->ext_capab_len;
		if (u4Temp > CMD_PEER_UPDATE_EXT_CAP_MAXLEN)
			u4Temp = CMD_PEER_UPDATE_EXT_CAP_MAXLEN;
		kalMemCopy(rCmdUpdate.aucExtCap, params->ext_capab, u4Temp);
		rCmdUpdate.u2ExtCapLen = u4Temp;
	}

	if (prLinkParams->ht_capa != NULL) {

		rCmdUpdate.rHtCap.u2CapInfo = prLinkParams->ht_capa->cap_info;
		rCmdUpdate.rHtCap.ucAmpduParamsInfo =
			prLinkParams->ht_capa->ampdu_params_info;
		rCmdUpdate.rHtCap.u2ExtHtCapInfo =
			prLinkParams->ht_capa->extended_ht_cap_info;
		rCmdUpdate.rHtCap.u4TxBfCapInfo =
			prLinkParams->ht_capa->tx_BF_cap_info;
		rCmdUpdate.rHtCap.ucAntennaSelInfo =
			prLinkParams->ht_capa->antenna_selection_info;
		kalMemCopy(rCmdUpdate.rHtCap.rMCS.arRxMask,
			   prLinkParams->ht_capa->mcs.rx_mask,
			   sizeof(rCmdUpdate.rHtCap.rMCS.arRxMask));

		rCmdUpdate.rHtCap.rMCS.u2RxHighest =
			prLinkParams->ht_capa->mcs.rx_highest;
		rCmdUpdate.rHtCap.rMCS.ucTxParams =
			prLinkParams->ht_capa->mcs.tx_params;
		rCmdUpdate.fgIsSupHt = TRUE;
	}
	/* vht */

	if (prLinkParams->vht_capa != NULL) {
		/* rCmdUpdate.rVHtCap */
		/* rCmdUpdate.rVHtCap */
	}

	/* update a TDLS peer record */
	/* sanity check */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER)))
		rCmdUpdate.eStaType = STA_TYPE_DLS_PEER;
	rCmdUpdate.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, cnmPeerUpdate, &rCmdUpdate,
			   sizeof(struct CMD_PEER_UPDATE), FALSE, FALSE, FALSE,
			   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;
	/* for Ch Sw AP prohibit case */
	if (prBssInfo->fgTdlsIsChSwProhibited) {
		/* disable TDLS ch sw function */

		rStatus = kalIoctl(prGlueInfo,
				   TdlsSendChSwControlCmd,
				   &TdlsSendChSwControlCmd,
				   sizeof(struct CMD_TDLS_CH_SW),
				   FALSE, FALSE, FALSE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);
	}

	return 0;
}
#else
int
mtk_cfg80211_change_station(struct wiphy *wiphy,
			    struct net_device *ndev, u8 *mac,
			    struct station_parameters *params)
{

	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_UPDATE rCmdUpdate;
	uint32_t rStatus;
	uint32_t u4BufLen, u4Temp;
	struct ADAPTER *prAdapter;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (!prBssInfo || !params || !mac)
		return -EINVAL;

	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_AUTHORIZED))) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetAuthorized,
			(void *) mac, MAC_ADDR_LEN, FALSE,
			FALSE, FALSE, &u4BufLen);
	}

	if (params->supported_rates == NULL)
		return 0;

	/* init */
	kalMemZero(&rCmdUpdate, sizeof(rCmdUpdate));
	kalMemCopy(rCmdUpdate.aucPeerMac, mac, 6);

	if (params->supported_rates != NULL) {

		u4Temp = params->supported_rates_len;
		if (u4Temp > CMD_PEER_UPDATE_SUP_RATE_MAX)
			u4Temp = CMD_PEER_UPDATE_SUP_RATE_MAX;
		kalMemCopy(rCmdUpdate.aucSupRate, params->supported_rates,
			   u4Temp);
		rCmdUpdate.u2SupRateLen = u4Temp;
	}

	/*
	 * In supplicant, only recognize WLAN_EID_QOS 46, not 0xDD WMM
	 * So force to support UAPSD here.
	 */
	rCmdUpdate.UapsdBitmap = 0x0F;	/*params->uapsd_queues; */
	rCmdUpdate.UapsdMaxSp = 0;	/*params->max_sp; */

	rCmdUpdate.u2Capability = params->capability;

	if (params->ext_capab != NULL) {

		u4Temp = params->ext_capab_len;
		if (u4Temp > CMD_PEER_UPDATE_EXT_CAP_MAXLEN)
			u4Temp = CMD_PEER_UPDATE_EXT_CAP_MAXLEN;
		kalMemCopy(rCmdUpdate.aucExtCap, params->ext_capab, u4Temp);
		rCmdUpdate.u2ExtCapLen = u4Temp;
	}

	if (params->ht_capa != NULL) {

		rCmdUpdate.rHtCap.u2CapInfo = params->ht_capa->cap_info;
		rCmdUpdate.rHtCap.ucAmpduParamsInfo =
			params->ht_capa->ampdu_params_info;
		rCmdUpdate.rHtCap.u2ExtHtCapInfo =
			params->ht_capa->extended_ht_cap_info;
		rCmdUpdate.rHtCap.u4TxBfCapInfo =
			params->ht_capa->tx_BF_cap_info;
		rCmdUpdate.rHtCap.ucAntennaSelInfo =
			params->ht_capa->antenna_selection_info;
		kalMemCopy(rCmdUpdate.rHtCap.rMCS.arRxMask,
			   params->ht_capa->mcs.rx_mask,
			   sizeof(rCmdUpdate.rHtCap.rMCS.arRxMask));

		rCmdUpdate.rHtCap.rMCS.u2RxHighest =
			params->ht_capa->mcs.rx_highest;
		rCmdUpdate.rHtCap.rMCS.ucTxParams =
			params->ht_capa->mcs.tx_params;
		rCmdUpdate.fgIsSupHt = TRUE;
	}
	/* vht */

	if (params->vht_capa != NULL) {
		/* rCmdUpdate.rVHtCap */
		/* rCmdUpdate.rVHtCap */
	}

	/* update a TDLS peer record */
	/* sanity check */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER)))
		rCmdUpdate.eStaType = STA_TYPE_DLS_PEER;
	rCmdUpdate.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, cnmPeerUpdate, &rCmdUpdate,
			   sizeof(struct CMD_PEER_UPDATE), FALSE, FALSE, FALSE,
			   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	/* for Ch Sw AP prohibit case */
	if (prBssInfo->fgTdlsIsChSwProhibited) {
		/* disable TDLS ch sw function */

		rStatus = kalIoctl(prGlueInfo,
				   TdlsSendChSwControlCmd,
				   &TdlsSendChSwControlCmd,
				   sizeof(struct CMD_TDLS_CH_SW),
				   FALSE, FALSE, FALSE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for adding a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_add_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     const u8 *mac, struct station_parameters *params)
{
	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_ADD rCmdCreate;
	struct ADAPTER *prAdapter;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	/* init */
	kalMemZero(&rCmdCreate, sizeof(rCmdCreate));
	kalMemCopy(rCmdCreate.aucPeerMac, mac, 6);

	/* create a TDLS peer record */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER))) {
		rCmdCreate.eStaType = STA_TYPE_DLS_PEER;
		rCmdCreate.ucBssIdx = ucBssIndex;
		rStatus = kalIoctl(prGlueInfo, cnmPeerAdd, &rCmdCreate,
				   sizeof(struct CMD_PEER_ADD),
				   FALSE, FALSE, FALSE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}

	return 0;
}
#else
int mtk_cfg80211_add_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac,
			     struct station_parameters *params)
{
	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_ADD rCmdCreate;
	struct ADAPTER *prAdapter;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	/* init */
	kalMemZero(&rCmdCreate, sizeof(rCmdCreate));
	kalMemCopy(rCmdCreate.aucPeerMac, mac, 6);

	/* create a TDLS peer record */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER))) {
		rCmdCreate.eStaType = STA_TYPE_DLS_PEER;
		rCmdCreate.ucBssIdx = ucBssIndex;
		rStatus = kalIoctl(prGlueInfo, cnmPeerAdd, &rCmdCreate,
				   sizeof(struct CMD_PEER_ADD),
				   FALSE, FALSE, FALSE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for deleting a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 *
 * @other
 *		must implement if you have add_station().
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     struct station_del_parameters *params)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	const u8 *mac = params->mac ? params->mac : bcast_addr;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prAdapter = prGlueInfo->prAdapter;

	/* For kernel 3.18 modification, we trasfer to local buff to query
	 * sta
	 */
	memset(deleteMac, 0, MAC_ADDR_LEN);
	memcpy(deleteMac, mac, MAC_ADDR_LEN);

	prStaRec = cnmGetStaRecByAddress(prAdapter,
		 (uint8_t) ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev, const u8 *mac)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prAdapter = prGlueInfo->prAdapter;

	/* For kernel 3.18 modification, we trasfer to local buff to query
	 * sta
	 */
	memset(deleteMac, 0, MAC_ADDR_LEN);
	memcpy(deleteMac, mac, MAC_ADDR_LEN);

	prStaRec = cnmGetStaRecByAddress(prAdapter,
		 (uint8_t) ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#endif
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prAdapter = prGlueInfo->prAdapter;

	prStaRec = cnmGetStaRecByAddress(prAdapter,
			 (uint8_t) ucBssIndex, mac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to transmit a TDLS data frame from nl80211.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 * \param[in]
 * \param[in]
 * \param[in] buf includes RSN IE + FT IE + Lifetimeout IE
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       bool initiator, const u8 *buf, size_t len)
{
	struct GLUE_INFO *prGlueInfo;
	struct TDLS_CMD_LINK_MGT rCmdMgt;
	uint32_t u4BufLen;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);

	if  (len > TDLS_SEC_BUF_LENGTH) {
		DBGLOG(REQ, WARN, "%s:len > TDLS_SEC_BUF_LENGTH\n", __func__);
		return -EINVAL;
	}

	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);
	rCmdMgt.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt,
		 sizeof(struct TDLS_CMD_LINK_MGT), FALSE, TRUE, FALSE,
		 &u4BufLen);

	DBGLOG(REQ, INFO, "rStatus: %x", rStatus);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;
	else
		return -EINVAL;
}
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       const u8 *buf, size_t len)
{
	struct GLUE_INFO *prGlueInfo;
	struct TDLS_CMD_LINK_MGT rCmdMgt;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);

	if  (len > TDLS_SEC_BUF_LENGTH) {
		DBGLOG(REQ, WARN, "%s:len > TDLS_SEC_BUF_LENGTH\n", __func__);
		return -EINVAL;
	}

	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);
	rCmdMgt.ucBssIdx = ucBssIndex;
	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt,
		 sizeof(struct TDLS_CMD_LINK_MGT), FALSE, TRUE, FALSE,
		 &u4BufLen);
	return 0;

}

#else
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, const u8 *buf, size_t len)
{
	struct GLUE_INFO *prGlueInfo;
	struct TDLS_CMD_LINK_MGT rCmdMgt;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);

	if (len > TDLS_SEC_BUF_LENGTH) {
		DBGLOG(REQ, WARN,
		       "In mtk_cfg80211_tdls_mgmt , len > TDLS_SEC_BUF_LENGTH, please check\n");
		return -EINVAL;
	}

	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);
	rCmdMgt.ucBssIdx = ucBssIndex;
	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt,
		 sizeof(struct TDLS_CMD_LINK_MGT), FALSE, TRUE, FALSE,
		 &u4BufLen);
	return 0;

}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to hadel TDLS link from nl80211.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 * \param[in]
 * \param[in]
 * \param[in] buf includes RSN IE + FT IE + Lifetimeout IE
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy,
			   struct net_device *dev,
			   const u8 *peer, enum nl80211_tdls_operation oper)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen;
	struct ADAPTER *prAdapter;
	struct TDLS_CMD_LINK_OPER rCmdOper;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(REQ, INFO, "ucBssIndex = %d, oper=%d",
		ucBssIndex, oper);

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper = (enum ENUM_TDLS_LINK_OPER)oper;
	rCmdOper.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper,
			sizeof(struct TDLS_CMD_LINK_OPER), FALSE, FALSE, FALSE,
			&u4BufLen);

	DBGLOG(REQ, INFO, "rStatus: %x", rStatus);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;
	else
		return -EINVAL;
}
#else
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy,
			   struct net_device *dev, u8 *peer,
			   enum nl80211_tdls_operation oper)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen;
	struct ADAPTER *prAdapter;
	struct TDLS_CMD_LINK_OPER rCmdOper;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, INFO, "ucBssIndex = %d, oper=%d",
		ucBssIndex, oper);

	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper = oper;
	rCmdOper.ucBssIdx = ucBssIndex;

	kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper,
			sizeof(struct TDLS_CMD_LINK_OPER), FALSE, FALSE, FALSE,
			&u4BufLen);
	return 0;
}
#endif
#endif

#ifdef CONFIG_NL80211_TESTMODE
#if CFG_SUPPORT_NCHO
/* NCHO related command definition. Setting by supplicant */
#define CMD_NCHO_ROAM_TRIGGER_GET		"GETROAMTRIGGER"
#define CMD_NCHO_ROAM_TRIGGER_SET		"SETROAMTRIGGER"
#define CMD_NCHO_ROAM_DELTA_GET			"GETROAMDELTA"
#define CMD_NCHO_ROAM_DELTA_SET			"SETROAMDELTA"
#define CMD_NCHO_ROAM_SCAN_PERIOD_GET		"GETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_PERIOD_SET		"SETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_GET		"GETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_SET		"SETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_ADD		"ADDROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CONTROL_GET		"GETROAMSCANCONTROL"
#define CMD_NCHO_ROAM_SCAN_CONTROL_SET		"SETROAMSCANCONTROL"
#define CMD_NCHO_MODE_SET			"SETNCHOMODE"
#define CMD_NCHO_MODE_GET			"GETNCHOMODE"
#define CMD_NCHO_ROAM_SCAN_FREQS_GET		"GETROAMSCANFREQUENCIES"
#define CMD_NCHO_ROAM_SCAN_FREQS_SET		"SETROAMSCANFREQUENCIES"
#define CMD_NCHO_ROAM_SCAN_FREQS_ADD		"ADDROAMSCANFREQUENCIES"
#define CMD_NCHO_ROAM_BAND_GET			"GETROAMBAND"
#define CMD_NCHO_ROAM_BAND_SET			"SETROAMBAND"
#define CMD_NCHO_ROAM_SCAN_FREQS_GET_LEGACY	"GETROAMSCANFREQUENCIES_LEGACY"
#define CMD_NCHO_ROAM_SCAN_FREQS_ADD_LEGACY	"ADDROAMSCANFREQUENCIES_LEGACY"

int testmode_set_ncho_roam_trigger(IN struct wiphy *wiphy,
				  IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam trigger cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamTrigger,
				   &i4Param, sizeof(int32_t),
				   FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "NCHO set roam trigger fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam trigger successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_trigger(IN struct wiphy *wiphy,
				  IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamTrigger,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamTrigger fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_NOT_INDICATING;
	}

	i4Param = RCPI_TO_dBm(i4Param);		/* RCPI to DB */
	i4BytesWritten = snprintf(buf, sizeof(buf),
		CMD_NCHO_ROAM_TRIGGER_GET" %d", i4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(INIT, INFO, "NCHO query RoamTrigger is [%s][%s]\n",
		cmdV1Header.buffer, pcCommand);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_delta(IN struct wiphy *wiphy,
				    IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR,
			       "NCHO parse u4Param error %d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam delta cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamDelta,
				   &i4Param, sizeof(int32_t),
				   FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam delta fail 0x%x\n", rStatus);
		else
			DBGLOG(INIT, TRACE, "NCHO set roam delta successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_delta(IN struct wiphy *wiphy,
				    IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamDelta,
			   &i4Param, sizeof(int32_t),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamDelta fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf),
		CMD_NCHO_ROAM_DELTA_GET" %d", i4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is [%d][%s]\n",
		i4Param, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_period(IN struct wiphy *wiphy,
					 IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam period cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnPeriod,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "NCHO set roam period fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE, "NCHO set roam period successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_scn_period(IN struct wiphy *wiphy,
					 IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnPeriod,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamTrigger fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_NOT_INDICATING;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf),
		CMD_NCHO_ROAM_SCAN_PERIOD_GET" %d", i4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(INIT, INFO, "NCHO query Roam Period is [%s][%s]\n",
		cmdV1Header.buffer, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_chnl(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen, IN uint8_t changeMode)
{
	uint32_t u4ChnlInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;
	struct CFG_NCHO_SCAN_CHNL *prExistingRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl =
		&prGlueInfo->prAdapter->rNchoInfo.rAddRoamScnChnl;

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		rRoamScnChnl.ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "NCHO ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "NCHO param mismatch %d\n",
			       u4ChnlInfo);
			return WLAN_STATUS_INVALID_DATA;
		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4ChnlInfo);
			if (i4Ret) {
				while (i != 2) {
					rRoamScnChnl.arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "NCHO parse chnl num error %d\n", i4Ret);
				return -1;
			}
			if (u4ChnlInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "NCHO t = %d, channel value=%d\n",
				       t, u4ChnlInfo);
				if ((u4ChnlInfo >= 1) && (u4ChnlInfo <= 14))
					rRoamScnChnl.arChnlInfoList[t].eBand =
								BAND_2G4;
				else
					rRoamScnChnl.arChnlInfoList[t].eBand =
								BAND_5G;

				rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				t++;
			}
		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			rRoamScnChnl.arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			rRoamScnChnl.arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			rRoamScnChnl.ucChannelListNum++;
		}

		DBGLOG(INIT, TRACE, "NCHO %s roam scan channel cmd\n",
			changeMode ? "set" : "add");
		if (changeMode)
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				FALSE, FALSE, TRUE, &u4SetInfoLen);
		else
			rStatus = kalIoctl(prGlueInfo,
				wlanoidAddNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan channel successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_scn_chnl(IN struct wiphy *wiphy,
				       IN char *pcCommand, IN int i4TotalLen)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	u4ChnlInfo = rRoamScnChnl.ucChannelListNum;
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   CMD_NCHO_ROAM_SCAN_CHANNELS_GET" %u",
				   u4ChnlInfo);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo =
			rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u", u4ChnlInfo);
	}

	DBGLOG(REQ, INFO, "NCHO get scn chl list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_ctrl(IN struct wiphy *wiphy,
				       IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam scan control cmd %d\n",
		       u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnCtrl,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan control fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan control successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_scn_ctrl(IN struct wiphy *wiphy,
				       IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnCtrl,
			   &u4Param, sizeof(uint32_t),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnCtrl fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf),
		CMD_NCHO_ROAM_SCAN_CONTROL_GET" %u", u4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, INFO, "NCHO query ok and ret is [%u][%s]\n",
		u4Param, buf);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_mode(IN struct wiphy *wiphy, IN char *pcCommand,
			IN int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc,
		       apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set enable cmd %d\n",
			       u4Param);
			rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoEnable,
				&u4Param, sizeof(uint32_t),
				FALSE, FALSE, TRUE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR,
				       "NCHO set enable fail 0x%x\n", rStatus);
			else
				DBGLOG(INIT, TRACE,
				       "NCHO set enable successed\n");
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_get_ncho_mode(IN struct wiphy *wiphy, IN char *pcCommand,
			 IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoEnable,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoEnable fail 0x%x\n",
		       rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_INVALID_DATA;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf),
				  CMD_NCHO_MODE_GET" %u", u4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, INFO, "NCHO query ok and ret is [%u][%s]\n",
		u4Param, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_freq(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen, IN uint8_t changeMode)
{
	uint32_t u4ChnlInfo = 0, u4FreqInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CFG_NCHO_SCAN_CHNL *prExistingRoamScnChnl;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl =
		&prGlueInfo->prAdapter->rNchoInfo.rAddRoamScnChnl;

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		rRoamScnChnl.ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "NCHO ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "NCHO param mismatch %d\n",
			       u4ChnlInfo);
			return WLAN_STATUS_INVALID_DATA;
		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4FreqInfo);
			if (i4Ret) {
				while (i != 2) {
					rRoamScnChnl.arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "NCHO parse chnl num error %d\n", i4Ret);
				return -1;
			}
			if (u4FreqInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "NCHO t = %d, Freq=%d\n", t, u4FreqInfo);
				/* Input freq unit should be kHz */
				u4ChnlInfo = nicFreq2ChannelNum(
					u4FreqInfo * 1000);
				if (u4FreqInfo >= 2412 && u4FreqInfo <= 2484)
					rRoamScnChnl.arChnlInfoList[t].eBand =
						BAND_2G4;
				else if (u4FreqInfo >= 5180 &&
					u4FreqInfo <= 5900)
					rRoamScnChnl.arChnlInfoList[t].eBand =
						BAND_5G;
#if (CFG_SUPPORT_WIFI_6G == 1)
				else if (u4FreqInfo >= 5955 &&
					u4FreqInfo <= 7115)
					rRoamScnChnl.arChnlInfoList[t].eBand =
						BAND_6G;
#endif

				rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				rRoamScnChnl.arChnlInfoList[t].u2PriChnlFreq =
								u4FreqInfo;
				t++;
			}
		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			rRoamScnChnl.arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			rRoamScnChnl.arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			rRoamScnChnl.ucChannelListNum++;
		}

		DBGLOG(INIT, TRACE, "NCHO %s roam scan channel cmd\n",
			changeMode ? "set" : "add");
		if (changeMode)
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				FALSE, FALSE, TRUE, &u4SetInfoLen);
		else
			rStatus = kalIoctl(prGlueInfo,
				wlanoidAddNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan channel successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_scn_freq(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	u4ChnlInfo = rRoamScnChnl.ucChannelListNum;
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   "%s %u",
				   apcArgv[0], u4ChnlInfo);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo =
			rRoamScnChnl.arChnlInfoList[i].u2PriChnlFreq;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u", u4ChnlInfo);
	}

	DBGLOG(REQ, INFO, "NCHO get scn freq list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_get_cu(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen, uint8_t ucBssIndex)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BSS_INFO *prAisBssInfo = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;
	struct BSS_DESC *prBssDesc = NULL;
	int32_t i4RetValue = -1, i4BytesWritten = -1;
	char buf[8] = {0};

	WIPHY_PRIV(wiphy, prGlueInfo);
	prAisBssInfo = aisGetAisBssInfo(prGlueInfo->prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter, ucBssIndex);
	prBssDesc = prAisFsmInfo->prTargetBssDesc;

	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED &&
		prBssDesc && prBssDesc->fgExistBssLoadIE)
		i4RetValue = prBssDesc->ucChnlUtilization;

	i4BytesWritten = snprintf(buf, sizeof(buf), "%d", i4RetValue);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, TRACE, "Current CU:%d\n", i4RetValue);

	return mtk_cfg80211_process_str_cmd_reply(wiphy, buf, sizeof(buf));
}

int testmode_get_ncho_roam_band(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamBand,
			   &i4Param, sizeof(int32_t),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamDelta fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf),
		CMD_NCHO_ROAM_BAND_GET" %d", i4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is [%d][%s]\n",
		i4Param, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_band(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;

	WIPHY_PRIV(wiphy, prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc,
		       apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam band:%d\n", u4Param);

			if (u4Param >= NCHO_ROAM_BAND_MAX)
				return WLAN_STATUS_INVALID_DATA;

			rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamBand,
				&u4Param, sizeof(uint32_t),
				FALSE, FALSE, TRUE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR,
				       "NCHO set roam band fail 0x%x\n",
				       rStatus);
			else
				DBGLOG(INIT, TRACE,
				       "NCHO set roam band successed\n");
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set roam band failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}
#endif /* CFG_SUPPORT_NCHO */

#if CFG_SUPPORT_MANIPULATE_TID
int testmode_manipulate_tid(IN struct wiphy *wiphy, IN char *pcCommand,
			 IN int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MANIUPLATE_TID param;

	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc != 4) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n", i4Argc, pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalMemZero(&param, sizeof(struct PARAM_MANIUPLATE_TID));

	if (kalkStrtou8(apcArgv[1], 0, &param.ucMode))
		DBGLOG(REQ, LOUD, "Mode parse %s error\n", apcArgv[1]);

	if (kalkStrtou8(apcArgv[3], 0, &param.ucTid))
		DBGLOG(REQ, LOUD, "Tid parse %s error\n", apcArgv[3]);

	rStatus = kalIoctl(prGlueInfo, wlanoidManipulateTid,
			&param, sizeof(struct PARAM_MANIUPLATE_TID),
			FALSE, FALSE, FALSE, &u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR,
		       "SET_TID fail 0x%x\n", rStatus);
	else
		DBGLOG(INIT, TRACE,
		       "SET_TID successed\n");

	return rStatus;
}
#endif /* CFG_SUPPORT_MANIPULATE_TID */

#if CFG_TC10_FEATURE
int testmode_dump_roaming_history(IN struct wiphy *wiphy,
			IN char *pcCommand, IN int i4TotalLen,
			uint8_t ucBssIndex)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	struct LINK *history = NULL;
	struct CONNECTED_BSS *bss;
	char buf[512] = {0};

	WIPHY_PRIV(wiphy, prGlueInfo);
	prRoamingFsmInfo = aisGetRoamingInfo(prGlueInfo->prAdapter, ucBssIndex);
	history = &prRoamingFsmInfo->rRoamingHistory;

	LINK_FOR_EACH_ENTRY(bss, history, rLinkEntry, struct CONNECTED_BSS)
	{
		kalSprintf(buf + strlen(buf), "bss=" MACSTR "\n", MAC2STR(bss->aucBssid));
	}

	return mtk_cfg80211_process_str_cmd_reply(wiphy, &buf[0], sizeof(buf));
}

int testmode_set_scan_param(IN struct wiphy *wiphy,
			IN char *pcCommand, IN int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_SCAN param;

	WIPHY_PRIV(wiphy, prGlueInfo);

	/*ex: wpa_cli driver SET_DWELL_TIME W X Y Z*/
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc != 5) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n", i4Argc, pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalMemZero(&param, sizeof(struct PARAM_SCAN));

	if (kalkStrtou8(apcArgv[1], 0, &param.ucDfsChDwellTimeMs)) {
		DBGLOG(REQ, LOUD, "DfsDwellTime parse %s err\n", apcArgv[1]);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou16(apcArgv[2], 0, &param.u2OpChStayTimeMs)) {
		DBGLOG(REQ, LOUD, "OpChStayTimeMs parse %s err\n", apcArgv[2]);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou8(apcArgv[3], 0, &param.ucNonDfsChDwellTimeMs)) {
		DBGLOG(REQ, LOUD,
			"NonDfsDwellTimeMs parse %s err\n", apcArgv[3]);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou16(apcArgv[4], 0, &param.u2OpChAwayTimeMs)) {
		DBGLOG(REQ, LOUD, "OpChAwayTimeMs parse %s err\n", apcArgv[4]);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetScanParam,
			&param, sizeof(struct PARAM_SCAN),
			FALSE, FALSE, FALSE, &u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR,
		       "SET_SCAN_PARAM fail 0x%x\n", rStatus);
	else
		DBGLOG(INIT, TRACE,
		       "SET_SCAN_PARAM successed\n");

	return rStatus;
}

int testmode_set_latency_crt_data(IN struct wiphy *wiphy,
			IN char *pcCommand, IN int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0, i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0, u4Mode = 0;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	/*ex: wpa_cli driver SET_LATENCY_CRT_DATA X */
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc != 2) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n", i4Argc, pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Mode);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "Set Latency crt mode parse error %d\n",
			i4Ret);
		return WLAN_STATUS_FAILURE;
	}

	/* Do further scan handling for mode 2 and mode 3,
	 * reset if u4Mode == 0
	 */
	if (u4Mode >= 2 || u4Mode == 0) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetLatencyCrtData,
			&u4Mode, sizeof(uint32_t),
			FALSE, FALSE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
				"SET_CRT_DATA fail 0x%x\n", rStatus);
		else
			DBGLOG(INIT, TRACE,
				"SET_CRT_DATA successed\n");
	}

	return rStatus;
}
#endif

int testmode_add_roam_scn_chnl(
	IN struct wiphy *wiphy, IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4ChnlInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_SCAN_CHNL *prRoamScnChnl;
	struct CFG_SCAN_CHNL *prExistingRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl = &prGlueInfo->prAdapter->rAddRoamScnChnl;

	prRoamScnChnl = kalMemAlloc(sizeof(struct CFG_SCAN_CHNL), VIR_MEM_TYPE);
	if (prRoamScnChnl == NULL) {
		DBGLOG(REQ, ERROR, "alloc roaming scan channel fail\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(prRoamScnChnl, sizeof(struct CFG_SCAN_CHNL));

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4Ret);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;
		}

		prRoamScnChnl->ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "param mismatch %d\n",
			       u4ChnlInfo);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;

		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4ChnlInfo);
			if (i4Ret) {
				while (i != 2) {
					prRoamScnChnl->arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "parse chnl num error %d\n", i4Ret);
				rStatus = WLAN_STATUS_FAILURE;
				goto label_exit;
			}
			if (u4ChnlInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "t = %d, channel value=%d\n",
				       t, u4ChnlInfo);
				if ((u4ChnlInfo >= 1) && (u4ChnlInfo <= 14))
					prRoamScnChnl->arChnlInfoList[t].eBand =
								BAND_2G4;
				else
					prRoamScnChnl->arChnlInfoList[t].eBand =
								BAND_5G;

				prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				t++;
			}
		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			prRoamScnChnl->arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			prRoamScnChnl->arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			prRoamScnChnl->ucChannelListNum++;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidAddRoamScnChnl,
			prRoamScnChnl, sizeof(struct CFG_SCAN_CHNL),
			FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "add roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "add roam scan channel successed\n");
		goto label_exit;
	} else {
		DBGLOG(REQ, ERROR, "add failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
		goto label_exit;
	}

label_exit:
	kalMemFree(prRoamScnChnl, sizeof(struct CFG_SCAN_CHNL), VIR_MEM_TYPE);
	return rStatus;
}

#define CMD_SET_AX_BLACKLIST                    "SET_AX_BLACKLIST"

int testmode_set_ax_blacklist(IN struct wiphy *wiphy, IN char *pcCommand,
				IN int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_AX_BLACKLIST rBlacklist;
	int32_t i4BytesWritten = -1;
	uint8_t ucType = 0;
	uint8_t i = 0;
	uint8_t aucMacAddr[MAC_ADDR_LEN] = { 0 };
	uint8_t index = 0;
	int32_t i4Ret = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, cmd [%s]\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou8(apcArgv[1], 0, &ucType);
		if (i4BytesWritten)
			DBGLOG(REQ, ERROR, "parse ucType error %d\n",
					i4BytesWritten);


		kalMemZero(&rBlacklist, sizeof(rBlacklist));
		rBlacklist.ucType = ucType;
		rBlacklist.ucCount = (i4Argc - 2);
		for (i = 2; i < i4Argc; i++) {
			DBGLOG(REQ, TRACE,
				"argc %i, cmd [%s]\n", i4Argc, apcArgv[i]);
			i4Ret = wlanHwAddrToBin(apcArgv[i], &aucMacAddr[0]);
			if (i4Ret != 17) {
				DBGLOG(REQ, WARN,
				    "BSSID format is wrong! i4Ret=%d\n", i4Ret);
				continue;
			}

			if (index + MAC_ADDR_LEN >
				sizeof(rBlacklist.aucList)) {
				DBGLOG(REQ, WARN,
				    "Could only set %d BSSID in blacklist!\n",
				    i - 2);
				break;
			}
			COPY_MAC_ADDR(&rBlacklist.aucList[index],
					aucMacAddr);
			index += MAC_ADDR_LEN;
		}
		rStatus = kalIoctl(prGlueInfo, wlanoidSetAxBlocklist,
			(void *)&rBlacklist, sizeof(struct PARAM_AX_BLACKLIST),
			FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);

	} else {
		DBGLOG(REQ, ERROR, "fail invalid data\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_roam_scn_chnl(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen, IN uint8_t ucBssIndex)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   TRUE, TRUE, TRUE, &u4BufLen, ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanoidQueryRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   "GETROAMSCANCHANNELS_LEGACY %u",
				   rRoamScnChnl.ucChannelListNum);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo = rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u", u4ChnlInfo);
	}

	DBGLOG(REQ, INFO, "Get roam scn chl list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_reassoc(IN struct wiphy *wiphy, IN char *pcCommand,
	IN int i4TotalLen, IN uint8_t ucBssIndex, IN uint8_t reassocWithChl,
	IN uint8_t isLegacyCmd)
{
	uint32_t u4InputInfo = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CONNECT rNewSsid;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	uint8_t bssid[MAC_ADDR_LEN] = {0};
	uint8_t ucSSIDLen;
	uint8_t aucSSID[ELEM_MAX_LEN_SSID] = {0};

	DBGLOG(INIT, INFO, "command is %s\n", pcCommand);
	DBGLOG(INIT, INFO, "reassoc with %s (%s mode)\n",
		reassocWithChl ? "channel" : "frequency",
		isLegacyCmd ? "legacy" : "NCHO");
	WIPHY_PRIV(wiphy, prGlueInfo);
	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (!(isLegacyCmd ^ prGlueInfo->prAdapter->rNchoInfo.fgNCHOEnabled)) {
		DBGLOG(REQ, ERROR,
			"Command(%s) doesn't match current mode(%s)\n",
			isLegacyCmd ? "-legacy" : "-NCHO",
			prGlueInfo->prAdapter->rNchoInfo.fgNCHOEnabled ?
			"-NCHO" : "-legacy");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 3) {
		DBGLOG(REQ, TRACE, "argc is %i, cmd is %s, %s\n", i4Argc,
		       apcArgv[1], apcArgv[2]);
		i4Ret = kalkStrtou32(apcArgv[2], 0, &u4InputInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}
		/* copy ssd to local instead of assigning
		 * prConnSettings->aucSSID to rNewSsid.pucSsid because
		 * wlanoidSetConnect will copy ssid from rNewSsid again
		 */
		COPY_SSID(aucSSID, ucSSIDLen,
			prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

		i4Ret = wlanHwAddrToBin(apcArgv[1], bssid);
		if (i4Ret != 17) {
			DBGLOG(REQ, ERROR,
			    "BSSID format is wrong! i4Ret=%d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}
		kalMemZero(&rNewSsid, sizeof(rNewSsid));
		rNewSsid.u4CenterFreq = reassocWithChl ?
			(nicChannelNum2Freq(u4InputInfo, BAND_NULL) / 1000) :
			u4InputInfo;
		rNewSsid.pucBssid = NULL;
		rNewSsid.pucBssidHint = bssid;
		rNewSsid.pucSsid = aucSSID;
		rNewSsid.u4SsidLen = ucSSIDLen;
		rNewSsid.ucBssIdx = ucBssIndex;

		DBGLOG(INIT, INFO,
		       "Reassoc ssid=%s(%d) bssid=" MACSTR " freq=%d\n",
		       rNewSsid.pucSsid, rNewSsid.u4SsidLen,
		       MAC2STR(bssid), rNewSsid.u4CenterFreq);

		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetConnect,
			   (void *)&rNewSsid, sizeof(struct PARAM_CONNECT),
			   FALSE, FALSE, TRUE, &u4SetInfoLen, ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "reassoc fail 0x%x\n", rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "reassoc successed\n");
	} else {
		DBGLOG(REQ, ERROR, "reassoc failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_set_roam_trigger(IN struct wiphy *wiphy,
				  IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		DBGLOG(INIT, TRACE, "set roam trigger cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetRoamTrigger,
				   &i4Param, sizeof(int32_t),
				   FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "set roam trigger fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "set roam trigger successed\n");
	} else {
		DBGLOG(REQ, ERROR, "set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_roam_trigger(IN struct wiphy *wiphy,
				  IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRoamTrigger,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanoidQueryRoamTrigger fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_NOT_INDICATING;
	}

	i4Param = RCPI_TO_dBm(i4Param);		/* RCPI to DB */
	i4BytesWritten = snprintf(buf, sizeof(buf),
		"GETROAMTRIGGER_LEGACY %d", i4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(INIT, INFO, "query RoamTrigger is [%s][%s]\n",
		cmdV1Header.buffer, pcCommand);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_enable_btm(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen, IN uint8_t ucBssIndex)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;

	DBGLOG(INIT, INFO, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		i4Ret = kalkStrtou8(apcArgv[1], 0,
				&prConnSettings->ucBTMEnableMode);
		prConnSettings->ucBTMEnableMode += 1;
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}
	} else {
		DBGLOG(REQ, ERROR, "Set enable BTM failed\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;

}

int testmode_get_roam_scn_freq(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	enum ENUM_BAND eBand;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanoidQueryRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   "GETROAMSCANFREQUENCIES_LEGACY %u",
				   rRoamScnChnl.ucChannelListNum);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo = rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
		eBand = rRoamScnChnl.arChnlInfoList[i].eBand;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u",
				       KHZ_TO_MHZ(nicChannelNum2Freq(
				       u4ChnlInfo, eBand)));
	}

	DBGLOG(REQ, INFO, "Get roam scn freq list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_add_roam_scn_freq(IN struct wiphy *wiphy,
			  IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4ChnlInfo = 0, u4FreqInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_SCAN_CHNL *prRoamScnChnl;
	struct CFG_SCAN_CHNL *prExistingRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl = &prGlueInfo->prAdapter->rAddRoamScnChnl;

	prRoamScnChnl = kalMemAlloc(sizeof(struct CFG_SCAN_CHNL), VIR_MEM_TYPE);
	if (prRoamScnChnl == NULL) {
		DBGLOG(REQ, ERROR, "alloc roaming scan frequency fail\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(prRoamScnChnl, sizeof(struct CFG_SCAN_CHNL));

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, INFO, "argc[%i]\n", i4Argc);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4Ret);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;
		}

		prRoamScnChnl->ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "Frequency count:%d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "param mismatch %d\n", u4ChnlInfo);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;

		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4FreqInfo);
			if (i4Ret) {
				while (i != 2) {
					prRoamScnChnl->arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "parse chnl num error %d\n", i4Ret);
				rStatus = WLAN_STATUS_FAILURE;
				goto label_exit;
			}
			if (u4FreqInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "[%d] freq=%d\n", t, u4FreqInfo);
				/* Input freq unit should be kHz */
				u4ChnlInfo = nicFreq2ChannelNum(
					u4FreqInfo * 1000);
				if (u4FreqInfo >= 2412 && u4FreqInfo <= 2484)
					prRoamScnChnl->arChnlInfoList[t].eBand =
						BAND_2G4;
				else if (u4FreqInfo >= 5180 &&
					u4FreqInfo <= 5900)
					prRoamScnChnl->arChnlInfoList[t].eBand =
						BAND_5G;
#if (CFG_SUPPORT_WIFI_6G == 1)
				else if (u4FreqInfo >= 5955 &&
					u4FreqInfo <= 7115)
					prRoamScnChnl->arChnlInfoList[t].eBand =
						BAND_6G;
#endif

				prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				prRoamScnChnl->arChnlInfoList[t].u2PriChnlFreq =
								u4FreqInfo;
				t++;
			}

		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			prRoamScnChnl->arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			prRoamScnChnl->arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			prRoamScnChnl->ucChannelListNum++;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidAddRoamScnChnl,
			prRoamScnChnl, sizeof(struct CFG_SCAN_CHNL),
			FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "add roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "add roam scan channel successed\n");
		goto label_exit;
	} else {
		DBGLOG(REQ, ERROR, "add failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
		goto label_exit;
	}

label_exit:
	kalMemFree(prRoamScnChnl, sizeof(struct CFG_SCAN_CHNL), VIR_MEM_TYPE);
	return rStatus;
}

int
priv_driver_set_indoor_ch(IN struct wiphy *wiphy,
			  IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t *blockedChs, numBlockedChs = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct REG_INFO *prRegInfo;
	uint32_t u4BufLen;
	int i, count;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR,
		       "Failed to get prGlueInfo from wiphy\n");
		return -EINVAL;
	}
	prAdapter = prGlueInfo->prAdapter;
	prRegInfo = &prGlueInfo->rRegInfo;

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc < 2) {
		DBGLOG(REQ, ERROR,
			"Set indoor ch error input parameter %d\n", i4Argc);
		return rStatus;
	}

	if (kalkStrtou8(apcArgv[1], 0, &numBlockedChs))
		DBGLOG(REQ, LOUD, "parse %s error\n", apcArgv[1]);

	/* Reset domain info to country code default */
	if (numBlockedChs == 0) {
		uint8_t country[2] = {0};

		prRegInfo->eRegChannelListMap = REG_CH_MAP_COUNTRY_CODE;
		country[0] = (prAdapter->rWifiVar.u2CountryCode
				& 0xff00) >> 8;
		country[1] = prAdapter->rWifiVar.u2CountryCode
				& 0x00ff;

		return kalIoctl(prGlueInfo, wlanoidSetCountryCode,
				country, 2, FALSE, FALSE, TRUE, &u4BufLen);
	}

	if (i4Argc != numBlockedChs + 2) {
		DBGLOG(REQ, ERROR, "param mismatch %d\n",
		       numBlockedChs);
		return WLAN_STATUS_INVALID_DATA;
	}

	blockedChs = kalMemAlloc(sizeof(int8_t) * numBlockedChs,
				 VIR_MEM_TYPE);
	if (!blockedChs) {
		DBGLOG(REQ, ERROR, "alloc blockedChs fail!\n");
		return WLAN_STATUS_FAILURE;
	}

	prRegInfo->eRegChannelListMap = REG_CH_MAP_BLOCK_INDOOR;
	for (i = 2, count = 0; i < i4Argc; i++) {
		if (kalkStrtou8(apcArgv[i], 0, &(blockedChs[count])))
			DBGLOG(REQ, LOUD, "err parse ch %s\n", apcArgv[1]);

		if (!rlmIsValidChnl(prAdapter, blockedChs[count], BAND_5G)) {
			DBGLOG(INIT, INFO, "Skip blocked CH: %d\n",
				blockedChs[count]);
			continue;
		}

		count++;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetBlockIndoorChs,
			blockedChs, count, FALSE,
			FALSE, TRUE, &u4BufLen);

	kalMemFree(blockedChs, VIR_MEM_TYPE,
		   sizeof(int8_t) * numBlockedChs);

	return rStatus;
}

int
priv_driver_set_beacon_recv(IN struct wiphy *wiphy, IN uint8_t enable)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t reportBeaconEn;
	uint32_t u4BufLen;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR,
		       "[SWIPS] Failed to get prGlueInfo from wiphy\n");
		return -EINVAL;
	}

	reportBeaconEn = enable;
	return kalIoctl(prGlueInfo, wlanoidSetBeaconRecv,
			&reportBeaconEn, sizeof(uint8_t),
			FALSE, FALSE, TRUE, &u4BufLen);
}

int32_t mtk_cfg80211_process_str_cmd_reply(
	IN struct wiphy *wiphy, IN char *data, IN int len)
{
	struct sk_buff *skb;
	int32_t i4Err = 0;

	skb = cfg80211_testmode_alloc_reply_skb(wiphy, len);

	if (!skb) {
		DBGLOG(REQ, INFO, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	i4Err = nla_put_nohdr(skb, len, data);
	if (i4Err) {
		kfree_skb(skb);
		return i4Err;
	}

	return cfg80211_testmode_reply(skb);
}
#if CFG_SUPPORT_ASSURANCE

int testmode_set_disconnect_ies(IN struct wiphy *wiphy, IN char *pcCommand,
			 IN int i4TotalLen)
{
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint16_t hexLength = 0;
	uint8_t *asciiArray = NULL;
	uint8_t hexArray[CFG_CFG80211_IE_BUF_LEN];
	uint8_t IEs[CFG_CFG80211_IE_BUF_LEN];
	uint16_t ieLength = 0;
	uint16_t i;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, cmd [%s]\n", i4Argc, apcArgv[1]);
		asciiArray = apcArgv[1];
		hexLength = (uint16_t)i4TotalLen - 20;
		if (hexLength > CFG_CFG80211_IE_BUF_LEN)
			hexLength = CFG_CFG80211_IE_BUF_LEN;
		for (i = 0; i < hexLength; i++)
			hexArray[i] = (uint8_t)
			    wlanHexToNum((int8_t)asciiArray[i]);
		ieLength = hexLength/2;
		for (i = 0; i < ieLength; i++)
			IEs[i] = hexArray[i*2] << 4 | hexArray[i*2+1];
		DBGLOG(REQ, INFO, "ielen:%d, hexlen:%d\n", ieLength, hexLength);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisconnectIes,
			IEs, ieLength, FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, ERROR, "fail invalid data\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_roaming_reason_enabled(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidGetRoamingReasonEnable,
			   &u4Param, sizeof(u4Param),
			   TRUE, TRUE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf), "%u", u4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, INFO, "ret is %d\n", u4Param);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_roaming_reason_enabled(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, %s\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "set enable cmd %d\n", u4Param);
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetRoamingReasonEnable,
				&u4Param, sizeof(uint32_t),
				FALSE, FALSE, TRUE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		}
	} else {
		DBGLOG(REQ, ERROR, "set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_get_br_err_reason_enabled(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidGetBrErrReasonEnable,
			   &u4Param, sizeof(u4Param),
			   TRUE, TRUE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, sizeof(buf), "%u", u4Param);
	if (i4BytesWritten < 0 || (uint32_t) i4BytesWritten >= sizeof(buf)) {
		DBGLOG(REQ, ERROR, "snprintf return value error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, INFO, "ret is %d\n", u4Param);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_br_err_reason_enabled(IN struct wiphy *wiphy,
	IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, %s\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "set enable cmd %d\n", u4Param);
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetBrErrReasonEnable,
				&u4Param, sizeof(uint32_t),
				FALSE, FALSE, TRUE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		}
	} else {
		DBGLOG(REQ, ERROR, "set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

#endif

int32_t mtk_cfg80211_inspect_mac_addr(IN char *pcMacAddr)
{
	int32_t i = 0;

	if (pcMacAddr == NULL)
		return -1;

	for (i = 0; i < 17; i++) {
		if ((i % 3 != 2) && (!kalIsXdigit(pcMacAddr[i]))) {
			DBGLOG(REQ, ERROR, "[%c] is not hex digit\n",
			       pcMacAddr[i]);
			return -1;
		}
		if ((i % 3 == 2) && (pcMacAddr[i] != ':')) {
			DBGLOG(REQ, ERROR, "[%c]separate symbol is error\n",
			       pcMacAddr[i]);
			return -1;
		}
	}

	if (pcMacAddr[17] != '\0') {
		DBGLOG(REQ, ERROR, "no null-terminated character\n");
		return -1;
	}

	return 0;
}

int32_t mtk_cfg80211_process_str_cmd(IN struct wiphy *wiphy,
		struct wireless_dev *wdev,
		uint8_t *data, int32_t len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4SetInfoLen = 0;
	uint8_t ucBssIndex = 0;
	struct NL80211_DRIVER_STRING_CMD_PARAMS *param;
	uint8_t *cmd;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if (len <= sizeof(struct NL80211_DRIVER_STRING_CMD_PARAMS)) {
		DBGLOG(REQ, ERROR, "len [%d] is invalid!\n", len);
		return -EINVAL;
	}

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	param = (struct NL80211_DRIVER_STRING_CMD_PARAMS *) data;
	cmd = (uint8_t *) (param + 1);
	len -= sizeof(struct NL80211_DRIVER_STRING_CMD_PARAMS);

	DBGLOG(REQ, INFO, "cmd: %s\n", cmd);

	/* data is a null-terminated string, len also count null character */
	if (strlen(cmd) == 9 &&
			strnicmp(cmd, "tdls-ps ", 8) == 0) {
#if CFG_SUPPORT_TDLS
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidDisableTdlsPs,
				   (void *)(cmd + 8), 1,
				   FALSE, FALSE, TRUE, &u4SetInfoLen);
#else
		DBGLOG(REQ, WARN, "not support tdls\n");
		return -EOPNOTSUPP;
#endif
	} else if (strnicmp(cmd, "GETBSSINFO", 10) == 0) {
#if CFG_TC10_FEATURE
		uint32_t u4Len = 0;
		uint8_t rsp[200];

		kalMemZero(rsp, sizeof(rsp));
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidGetBssInfo,
				   (void *)rsp, u4Len, FALSE, FALSE,
				   FALSE, &u4SetInfoLen);

		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, u4SetInfoLen + 1);
#else
		DBGLOG(REQ, WARN, "not support GETBSSINFO\n");
		return -EOPNOTSUPP;
#endif
	} else if (strnicmp(cmd, "GETSTAINFO", 10) == 0) {
#if CFG_TC10_FEATURE
		uint32_t u4Len = 0;
		uint8_t rsp[200];
		uint8_t aucMacAddr[MAC_ADDR_LEN] = {0};
		int32_t i4Argc = 0, i4Ret = 0;
		int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
		struct BSS_INFO *prBssInfo;
		struct STA_RECORD *prStaRec;

		DBGLOG(REQ, LOUD, "command is %s\n", cmd);
		wlanCfgParseArgument(cmd, &i4Argc, apcArgv);
		DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

		if (i4Argc < 2)
			return -EOPNOTSUPP;

		i4Ret = mtk_cfg80211_inspect_mac_addr(apcArgv[1]);
		if (i4Ret) {
			DBGLOG(REQ, ERROR,
				"inspect mac format error u4Ret=%d\n", i4Ret);
			return -EOPNOTSUPP;
		}

		i4Ret = sscanf(apcArgv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			&aucMacAddr[0], &aucMacAddr[1], &aucMacAddr[2],
			&aucMacAddr[3], &aucMacAddr[4], &aucMacAddr[5]);

		if (i4Ret != MAC_ADDR_LEN) {
			DBGLOG(REQ, ERROR, "sscanf mac format fail u4Ret=%d\n",
				i4Ret);
			return -EOPNOTSUPP;
		}
		prBssInfo = prGlueInfo->prAdapter->aprBssInfo[ucBssIndex];
		if (prBssInfo == NULL) {
			DBGLOG(REQ, WARN, "No hotspot is found\n");
			return -EOPNOTSUPP;
		}

		prStaRec = bssGetClientByMac(prGlueInfo->prAdapter,
			prBssInfo,
			aucMacAddr);
		if (prStaRec == NULL) {
			prGlueInfo->prAdapter->fgSapLastStaRecSet = 0;
		} else {
			prGlueInfo->prAdapter->fgSapLastStaRecSet = 1;
			kalMemCopy(&prGlueInfo->prAdapter->rSapLastStaRec,
				prStaRec, sizeof(struct STA_RECORD));
		}
		kalMemZero(rsp, sizeof(rsp));
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidGetStaInfo,
				   (void *)rsp, u4Len, FALSE, FALSE,
				   FALSE, &u4SetInfoLen);

		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, u4SetInfoLen + 1);
#else
		DBGLOG(REQ, WARN, "not support GETSTAINFO\n");
		return -EOPNOTSUPP;
#endif
	} else if (strnicmp(cmd, "SET_LATENCY_CRT_DATA", 20) == 0) {
#if CFG_TC10_FEATURE
		rStatus = wlanChipConfigWithType(prGlueInfo->prAdapter, cmd, 22,
						CHIP_CONFIG_TYPE_WO_RESPONSE);
		return testmode_set_latency_crt_data(wiphy, cmd, len);
#endif
	} else if (strncasecmp(cmd, "NEIGHBOR-REQUEST", 16) == 0) {
		uint8_t *pucSSID = NULL;
		uint32_t u4SSIDLen = 0;

		if (strlen(cmd) > 22 &&
				(strncasecmp(cmd+16, " SSID=", 6) == 0)) {
			pucSSID = cmd + 22;
			u4SSIDLen = len - 22;
			DBGLOG(REQ, INFO, "cmd=%s, ssid len %u, ssid=%s\n", cmd,
			       u4SSIDLen, HIDE(pucSSID));
		}
		rStatus = kalIoctlByBssIdx(prGlueInfo,
				   wlanoidSendNeighborRequest,
				   (void *)pucSSID, u4SSIDLen, FALSE, FALSE,
				   TRUE, &u4SetInfoLen,
				   ucBssIndex);
	} else if (strncasecmp(cmd, "BSS-TRANSITION-QUERY", 20) == 0) {
		uint8_t *pucReason = NULL;

		if (strlen(cmd) > 28 &&
				(strncasecmp(cmd+20, " reason=", 8) == 0))
			pucReason = cmd + 28;
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSendBTMQuery,
				   (void *)pucReason, 1, FALSE, FALSE, TRUE,
				   &u4SetInfoLen,
				   ucBssIndex);
	} else if (strlen(cmd) == 11 &&
			strnicmp(cmd, "OSHAREMOD ", 10) == 0) {
#if CFG_SUPPORT_OSHARE
		struct OSHARE_MODE_T cmdBuf;
		struct OSHARE_MODE_T *pCmdHeader = NULL;
		struct OSHARE_MODE_SETTING_V1_T *pCmdData = NULL;

		kalMemZero(&cmdBuf, sizeof(cmdBuf));

		pCmdHeader = &cmdBuf;
		pCmdHeader->cmdVersion = OSHARE_MODE_CMD_V1;
		pCmdHeader->cmdType = 1; /*1-set   0-query*/
		pCmdHeader->magicCode = OSHARE_MODE_MAGIC_CODE;
		pCmdHeader->cmdBufferLen = MAX_OSHARE_MODE_LENGTH;

		pCmdData = (struct OSHARE_MODE_SETTING_V1_T *) &
			   (pCmdHeader->buffer[0]);
		pCmdData->osharemode = *(uint8_t *)(cmd + 10) - '0';

		DBGLOG(REQ, INFO, "cmd=%s, osharemode=%u\n", cmd,
		       pCmdData->osharemode);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetOshareMode,
				   &cmdBuf,
				   sizeof(struct OSHARE_MODE_T),
				   FALSE,
				   FALSE,
				   TRUE,
				   &u4SetInfoLen);

		if (rStatus == WLAN_STATUS_SUCCESS)
			prGlueInfo->prAdapter->fgEnOshareMode
				= pCmdData->osharemode;
#else
		DBGLOG(REQ, WARN, "not support OSHAREMOD\n");
		return -EOPNOTSUPP;
#endif
	} else if (strnicmp(cmd, "CMD_EXAMPLE", 11) == 0) {
		char tmp[] = "CMD_RESPONSE";

		return mtk_cfg80211_process_str_cmd_reply(
			wiphy, tmp, sizeof(tmp));

#if CFG_SUPPORT_ASSURANCE
	} else if (strnicmp(cmd, "SET_DISCONNECT_IES ", 19) == 0) {
		rStatus = testmode_set_disconnect_ies(wiphy, cmd, len);
	} else if (strnicmp(cmd, "GET_ROAMING_REASON_ENABLED", 26) == 0) {
		return testmode_get_roaming_reason_enabled(wiphy, cmd, len);
	} else if (strnicmp(cmd, "SET_ROAMING_REASON_ENABLED", 26) == 0) {
		rStatus = testmode_set_roaming_reason_enabled(wiphy, cmd, len);
	} else if (strnicmp(cmd, "GET_BR_ERR_REASON_ENABLED", 25) == 0) {
		return testmode_get_br_err_reason_enabled(wiphy, cmd, len);
	} else if (strnicmp(cmd, "SET_BR_ERR_REASON_ENABLED", 25) == 0) {
		rStatus = testmode_set_br_err_reason_enabled(wiphy, cmd, len);
#endif
	} else if (strnicmp(cmd, "ADDROAMSCANCHANNELS_LEGACY", 26) == 0) {
		rStatus = testmode_add_roam_scn_chnl(wiphy, cmd, len);
	} else if (strnicmp(cmd, "GETROAMSCANCHANNELS_LEGACY", 26) == 0 ||
		   strnicmp(cmd, "GETROAMSCANCHANNLES_LEGACY", 26) == 0) {
		rStatus = testmode_get_roam_scn_chnl(
				wiphy, cmd, len, ucBssIndex);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_FREQS_GET_LEGACY,
			  strlen(CMD_NCHO_ROAM_SCAN_FREQS_GET_LEGACY)) == 0) {
		rStatus = testmode_get_roam_scn_freq(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_FREQS_ADD_LEGACY,
			  strlen(CMD_NCHO_ROAM_SCAN_FREQS_ADD_LEGACY)) == 0) {
		rStatus = testmode_add_roam_scn_freq(wiphy, cmd, len);
	} else if (strnicmp(cmd, "REASSOC_LEGACY", 14) == 0) {
		rStatus = testmode_reassoc(wiphy, cmd, len, ucBssIndex, 1, 1);
	} else if (strnicmp(cmd, "REASSOC_FREQUENCY_LEGACY", 24) == 0) {
		rStatus = testmode_reassoc(wiphy, cmd, len, ucBssIndex, 0, 1);
	} else if (strnicmp(cmd, "REASSOC", 7) == 0) {
		rStatus = testmode_reassoc(wiphy, cmd, len, ucBssIndex, 1, 0);
	} else if (strnicmp(cmd, "REASSOC_FREQUENCY", 17) == 0) {
		rStatus = testmode_reassoc(wiphy, cmd, len, ucBssIndex, 0, 0);
	} else if (strnicmp(cmd, "SETROAMTRIGGER_LEGACY", 21) == 0) {
		rStatus = testmode_set_roam_trigger(wiphy, cmd, len);
	} else if (strnicmp(cmd, "GETROAMTRIGGER_LEGACY", 21) == 0) {
		return testmode_get_roam_trigger(wiphy, cmd, len);
	} else if (strnicmp(cmd, "SET_ENABLE_BTM", 14) == 0) {
		rStatus = testmode_set_enable_btm(wiphy, cmd, len, ucBssIndex);
#if CFG_SUPPORT_NCHO
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_TRIGGER_SET,
			  strlen(CMD_NCHO_ROAM_TRIGGER_SET)) == 0) {
		rStatus = testmode_set_ncho_roam_trigger(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_TRIGGER_GET,
			    strlen(CMD_NCHO_ROAM_TRIGGER_GET)) == 0) {
		return testmode_get_ncho_roam_trigger(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_DELTA_SET,
			    strlen(CMD_NCHO_ROAM_DELTA_SET)) == 0) {
		rStatus = testmode_set_ncho_roam_delta(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_DELTA_GET,
			    strlen(CMD_NCHO_ROAM_DELTA_GET)) == 0) {
		return testmode_get_ncho_roam_delta(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_PERIOD_SET,
			    strlen(CMD_NCHO_ROAM_SCAN_PERIOD_SET)) == 0) {
		rStatus = testmode_set_ncho_roam_scn_period(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_PERIOD_GET,
			    strlen(CMD_NCHO_ROAM_SCAN_PERIOD_GET)) == 0) {
		return testmode_get_ncho_roam_scn_period(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_CHANNELS_SET,
			    strlen(CMD_NCHO_ROAM_SCAN_CHANNELS_SET)) == 0) {
		rStatus = testmode_set_ncho_roam_scn_chnl(wiphy, cmd, len, 1);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_CHANNELS_ADD,
			    strlen(CMD_NCHO_ROAM_SCAN_CHANNELS_ADD)) == 0) {
		rStatus = testmode_set_ncho_roam_scn_chnl(wiphy, cmd, len, 0);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_CHANNELS_GET,
			    strlen(CMD_NCHO_ROAM_SCAN_CHANNELS_GET)) == 0) {
		return testmode_get_ncho_roam_scn_chnl(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_CONTROL_SET,
			    strlen(CMD_NCHO_ROAM_SCAN_CONTROL_SET)) == 0) {
		rStatus = testmode_set_ncho_roam_scn_ctrl(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_CONTROL_GET,
			    strlen(CMD_NCHO_ROAM_SCAN_CONTROL_GET)) == 0) {
		return testmode_get_ncho_roam_scn_ctrl(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_MODE_SET,
			    strlen(CMD_NCHO_MODE_SET)) == 0) {
		rStatus = testmode_set_ncho_mode(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_MODE_GET,
			    strlen(CMD_NCHO_MODE_GET)) == 0) {
		return testmode_get_ncho_mode(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_FREQS_GET,
			    strlen(CMD_NCHO_ROAM_SCAN_FREQS_GET)) == 0) {
		return testmode_get_ncho_roam_scn_freq(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_FREQS_SET,
		    strlen(CMD_NCHO_ROAM_SCAN_FREQS_SET)) == 0) {
		return testmode_set_ncho_roam_scn_freq(wiphy, cmd, len, 1);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_SCAN_FREQS_ADD,
			    strlen(CMD_NCHO_ROAM_SCAN_FREQS_ADD)) == 0) {
		return testmode_set_ncho_roam_scn_freq(wiphy, cmd, len, 0);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_BAND_GET,
			    strlen(CMD_NCHO_ROAM_BAND_GET)) == 0) {
	return testmode_get_ncho_roam_band(wiphy, cmd, len);
	} else if (strnicmp(cmd, CMD_NCHO_ROAM_BAND_SET,
			    strlen(CMD_NCHO_ROAM_BAND_SET)) == 0) {
		return testmode_set_ncho_roam_band(wiphy, cmd, len);
#endif
	} else if (strnicmp(cmd, CMD_SET_AX_BLACKLIST,
			    strlen(CMD_SET_AX_BLACKLIST)) == 0) {
		return testmode_set_ax_blacklist(wiphy, cmd, len);
	} else if (strnicmp(cmd, "SET_INDOOR_CHANNELS ", 20) == 0) {
		rStatus = priv_driver_set_indoor_ch(wiphy, cmd, len);
	} else if (strnicmp(cmd, "BEACON_RECV start", 17) == 0) {
		struct AIS_FSM_INFO *prAisFsmInfo;

		prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter,
						ucBssIndex);
		/* return abort reason if connecting */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_SEARCH ||
		    prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN ||
		    prAisFsmInfo->eCurrentState == AIS_STATE_JOIN) {
			DBGLOG(INIT, WARN,
			       "Dont start SWIPS when connecting\n");
			return mtk_cfg80211_process_str_cmd_reply(
				wiphy, "FAIL_CONNECT_STARTS",
				strlen("FAIL_CONNECT_STARTS") + 1);
		}
		/* return abort reason if connecting */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN ||
		    prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN ||
		    prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
			DBGLOG(INIT, WARN,
			       "Dont start SWIPS when scanning\n");
			return mtk_cfg80211_process_str_cmd_reply(
				wiphy, "FAIL_SCAN_STARTS",
				strlen("FAIL_SCAN_STARTS") + 1);
		}

		rStatus = priv_driver_set_beacon_recv(wiphy, TRUE);
		DBGLOG(REQ, INFO, "rStatus=%d\n", rStatus);
	} else if (strnicmp(cmd, "BEACON_RECV stop", 16) == 0) {
		rStatus = priv_driver_set_beacon_recv(wiphy, FALSE);

#if CFG_SUPPORT_MANIPULATE_TID
	} else if (strnicmp(cmd, "SET_TID ", 8) == 0) {
		return testmode_manipulate_tid(wiphy, cmd, len);
#endif
#if CFG_TC10_FEATURE
	} else if (strnicmp(cmd, "RoamingHistory", 14) == 0) {
		return testmode_dump_roaming_history(wiphy, cmd, len, ucBssIndex);
	} else if (strnicmp(cmd, "SET_DWELL_TIME ", 15) == 0) {
		return testmode_set_scan_param(wiphy, cmd, len);
	} else if (strnicmp(cmd, "GET_CU", 6) == 0) {
		return testmode_get_cu(wiphy, cmd, len, ucBssIndex);
	} else if (strnicmp(cmd, "SET_DEBUG_LEVEL ", 16) == 0) {
		struct CMD_CONNSYS_FW_LOG rFwLogCmd;
		uint32_t u4BufLen;

		kalMemZero(&rFwLogCmd, sizeof(rFwLogCmd));
		rFwLogCmd.fgCmd = FW_LOG_CMD_ON_OFF;
		rFwLogCmd.fgEarlySet = FALSE;

		if (strnicmp(cmd+16, "1", 1) == 0) {
			DBGLOG(REQ, INFO,
				"Set log level to DEFAULT\n");

			rFwLogCmd.fgValue = 0;
			kalIoctl(prGlueInfo,
				connsysFwLogControl,
				&rFwLogCmd,
				sizeof(struct CMD_CONNSYS_FW_LOG),
				FALSE, FALSE, FALSE,
				&u4BufLen);
		} else if (strnicmp(cmd+16, "2", 1) == 0) {
			DBGLOG(REQ, INFO,
				"Set log level to MORE\n");

			rFwLogCmd.fgValue = 1;
			kalIoctl(prGlueInfo,
				connsysFwLogControl,
				&rFwLogCmd,
				sizeof(struct CMD_CONNSYS_FW_LOG),
				FALSE, FALSE, FALSE,
				&u4BufLen);
		} else if (strnicmp(cmd+16, "3", 1) == 0) {
			DBGLOG(REQ, INFO,
				"Set log level to EXTREME\n");

			rFwLogCmd.fgValue = 2;
			kalIoctl(prGlueInfo,
				connsysFwLogControl,
				&rFwLogCmd,
				sizeof(struct CMD_CONNSYS_FW_LOG),
				FALSE, FALSE, FALSE,
				&u4BufLen);
		} else {
			DBGLOG(REQ, WARN, "Invalid log level.\n");
		}
#endif
	} else
		return -EOPNOTSUPP;

	return rStatus;
}

#endif /* CONFIG_NL80211_TESTMODE */

#if (CFG_SUPPORT_SINGLE_SKU == 1)

#if (CFG_BUILT_IN_DRIVER == 1)
/* in kernel-x.x/net/wireless/reg.c */
#else
bool is_world_regdom(const char *alpha2)
{
	if (!alpha2)
		return false;

	return (alpha2[0] == '0') && (alpha2[1] == '0');
}
#endif

enum regd_state regd_state_machine(IN struct regulatory_request *pRequest)
{
	switch (pRequest->initiator) {
	case NL80211_REGDOM_SET_BY_USER:
		DBGLOG(RLM, INFO, "regd_state_machine: SET_BY_USER\n");

		return rlmDomainStateTransition(REGD_STATE_SET_COUNTRY_USER,
						pRequest);

	case NL80211_REGDOM_SET_BY_DRIVER:
		DBGLOG(RLM, INFO, "regd_state_machine: SET_BY_DRIVER\n");

		return rlmDomainStateTransition(
			       REGD_STATE_SET_COUNTRY_DRIVER, pRequest);

	case NL80211_REGDOM_SET_BY_CORE:
		DBGLOG(RLM, INFO,
		       "regd_state_machine: NL80211_REGDOM_SET_BY_CORE\n");

		return rlmDomainStateTransition(REGD_STATE_SET_WW_CORE,
						pRequest);

	case NL80211_REGDOM_SET_BY_COUNTRY_IE:
		DBGLOG(RLM, WARN,
		       "============== WARNING ==============\n");
		DBGLOG(RLM, WARN,
		       "regd_state_machine: SET_BY_COUNTRY_IE\n");
		DBGLOG(RLM, WARN, "Regulatory rule is updated by IE.\n");
		DBGLOG(RLM, WARN,
		       "============== WARNING ==============\n");

		return rlmDomainStateTransition(REGD_STATE_SET_COUNTRY_IE,
						pRequest);

	default:
		return rlmDomainStateTransition(REGD_STATE_INVALID,
						pRequest);
	}
}


void
mtk_apply_custom_regulatory(IN struct wiphy *pWiphy,
			    IN const struct ieee80211_regdomain *pRegdom)
{
	u32 band_idx, ch_idx;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;

	DBGLOG(RLM, INFO, "%s()\n", __func__);

	/* to reset cha->flags*/
	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		sband = pWiphy->bands[band_idx];
		if (!sband)
			continue;

		for (ch_idx = 0; ch_idx < sband->n_channels; ch_idx++) {
			chan = &sband->channels[ch_idx];

			/*reset chan->flags*/
			chan->flags = 0;
		}

	}

	/* update to kernel */
	wiphy_apply_custom_regulatory(pWiphy, pRegdom);
}

void
mtk_reg_notify(IN struct wiphy *pWiphy,
	       IN struct regulatory_request *pRequest)
{
	struct GLUE_INFO *prGlueInfo;
	struct ADAPTER *prAdapter;
	enum regd_state old_state;
	struct wiphy *pBaseWiphy = wlanGetWiphy();

	if (g_u4HaltFlag) {
		DBGLOG(RLM, WARN, "wlan is halt, skip reg callback\n");
		return;
	}

	if (!pWiphy) {
		DBGLOG(RLM, ERROR, "pWiphy = NULL!\n");
		return;
	}

	/*
	 * Awlays use wlan0's base wiphy pointer to update reg notifier.
	 * Because only one reg state machine is handled.
	 */
	if (pBaseWiphy && (pWiphy != pBaseWiphy)) {
		pWiphy = pBaseWiphy;
		DBGLOG(RLM, ERROR, "Use base wiphy to update (p=%p)\n",
			pBaseWiphy);
	}

	old_state = rlmDomainGetCtrlState();

	/*
	 * Magic flow for driver to send inband command after kernel's calling
	 * reg_notifier callback
	 */
	if (!pRequest) {
		/*triggered by our driver in wlan initial process.*/

		if (old_state == REGD_STATE_INIT) {
			if (rlmDomainIsUsingLocalRegDomainDataBase()) {
				DBGLOG(RLM, WARN,
				       "County Code is not assigned. Use default WW.\n");
				goto DOMAIN_SEND_CMD;

			} else {
				DBGLOG(RLM, ERROR,
				       "Invalid REG state happened. state = 0x%x\n",
				       old_state);
				return;
			}
		} else if ((old_state == REGD_STATE_SET_WW_CORE) ||
			   (old_state == REGD_STATE_SET_COUNTRY_USER) ||
			   (old_state == REGD_STATE_SET_COUNTRY_DRIVER)) {
			goto DOMAIN_SEND_CMD;
		} else {
			DBGLOG(RLM, ERROR,
			       "Invalid REG state happened. state = 0x%x\n",
			       old_state);
			return;
		}
	}

	/*
	 * Ignore the CORE's WW setting when using local data base of regulatory
	 * rules
	 */
	if ((pRequest->initiator == NL80211_REGDOM_SET_BY_CORE) &&
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	    (pWiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY))
#else
	    (pWiphy->regulatory_flags & REGULATORY_CUSTOM_REG))
#endif
		return;/*Ignore the CORE's WW setting*/

	/*
	 * State machine transition
	 */
	DBGLOG(RLM, INFO,
	       "request->alpha2=%s, initiator=%x, intersect=%d\n",
	       pRequest->alpha2, pRequest->initiator, pRequest->intersect);

	regd_state_machine(pRequest);

	if (rlmDomainGetCtrlState() == old_state) {
		if (((old_state == REGD_STATE_SET_COUNTRY_USER)
		     || (old_state == REGD_STATE_SET_COUNTRY_DRIVER))
		    && (!(rlmDomainIsSameCountryCode(pRequest->alpha2,
					     sizeof(pRequest->alpha2)))))
			DBGLOG(RLM, INFO, "Set by user to NEW country code\n");
		else
			/* Change to same state or same country, ignore */
			return;
	} else if (rlmDomainIsCtrlStateEqualTo(REGD_STATE_INVALID)) {
		DBGLOG(RLM, ERROR,
		       "\n%s():\n---> WARNING. Transit to invalid state.\n",
		       __func__);
		DBGLOG(RLM, ERROR, "---> WARNING.\n ");
		rlmDomainAssert(0);
	}

	/*
	 * Set country code
	 */
	if (pRequest->initiator != NL80211_REGDOM_SET_BY_DRIVER) {
		rlmDomainSetCountryCode(pRequest->alpha2,
					sizeof(pRequest->alpha2));
	} else {
		/*SET_BY_DRIVER*/

		if (rlmDomainIsEfuseUsed()) {
			if (!rlmDomainIsUsingLocalRegDomainDataBase())
				DBGLOG(RLM, WARN,
				       "[WARNING!!!] Local DB must be used if country code from efuse.\n");
		} else {
			/* iwpriv case */
			if (rlmDomainIsUsingLocalRegDomainDataBase() &&
			    (!rlmDomainIsEfuseUsed())) {
				/*iwpriv set country but local data base*/
				u32 country_code =
						rlmDomainGetTempCountryCode();

				rlmDomainSetCountryCode((char *)&country_code,
							sizeof(country_code));
			} else {
				/*iwpriv set country but query CRDA*/
				rlmDomainSetCountryCode(pRequest->alpha2,
						sizeof(pRequest->alpha2));
			}
		}
	}

	rlmDomainSetDfsRegion(pRequest->dfs_region);


DOMAIN_SEND_CMD:
	DBGLOG(RLM, INFO, "g_mtk_regd_control.alpha2 = 0x%x\n",
	       rlmDomainGetCountryCode());

	/*
	 * Check if using customized regulatory rule
	 */
	if (rlmDomainIsUsingLocalRegDomainDataBase()) {
		const struct ieee80211_regdomain *pRegdom;
		u32 country_code = rlmDomainGetCountryCode();
		char alpha2[4];

		/*fetch regulatory rules from local data base*/
		alpha2[0] = country_code & 0xFF;
		alpha2[1] = (country_code >> 8) & 0xFF;
		alpha2[2] = (country_code >> 16) & 0xFF;
		alpha2[3] = (country_code >> 24) & 0xFF;

		pRegdom = rlmDomainSearchRegdomainFromLocalDataBase(alpha2);
		if (!pRegdom) {
			DBGLOG(RLM, ERROR,
			       "%s(): Error, Cannot find the correct RegDomain. country = %u\n",
			       __func__, rlmDomainGetCountryCode());

			rlmDomainAssert(0);
			return;
		}

		mtk_apply_custom_regulatory(pWiphy, pRegdom);
	}

	/*
	 * Parsing channels
	 */
	rlmDomainParsingChannel(pWiphy); /*real regd update*/

	/*
	 * Check if firmawre support single sku.
	 * no need to send information to FW due to FW is not supported.
	 */
	if (!regd_is_single_sku_en())
		return;

	/*
	 * Always use the wlan GlueInfo as parameter.
	 */
	prGlueInfo = rlmDomainGetGlueInfo();
	if (!prGlueInfo) {
		DBGLOG(RLM, ERROR, "prGlueInfo is NULL!\n");
		return; /*interface is not up yet.*/
	}

	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter) {
		DBGLOG(RLM, ERROR, "prAdapter is NULL!\n");
		return; /*interface is not up yet.*/
	}

	/*
	 * Send commands to firmware
	 */
	prAdapter->rWifiVar.u2CountryCode =
		(uint16_t)rlmDomainGetCountryCode();
	rlmDomainSendCmd(prAdapter);
}

void
cfg80211_regd_set_wiphy(IN struct wiphy *prWiphy)
{
	/*
	 * register callback
	 */
	prWiphy->reg_notifier = mtk_reg_notify;


	/*
	 * clear REGULATORY_CUSTOM_REG flag
	 */
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	/*tells kernel that assign WW as default*/
	prWiphy->flags &= ~(WIPHY_FLAG_CUSTOM_REGULATORY);
#else
	prWiphy->regulatory_flags &= ~(REGULATORY_CUSTOM_REG);

	/*ignore the hint from IE*/
	prWiphy->regulatory_flags |= REGULATORY_COUNTRY_IE_IGNORE;
#endif


	/*
	 * set REGULATORY_CUSTOM_REG flag
	 */
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	/*tells kernel that assign WW as default*/
	prWiphy->flags |= (WIPHY_FLAG_CUSTOM_REGULATORY);
#else
	prWiphy->regulatory_flags |= (REGULATORY_CUSTOM_REG);
#endif
	/* assigned a defautl one */
	if (rlmDomainGetLocalDefaultRegd())
		wiphy_apply_custom_regulatory(prWiphy,
					      rlmDomainGetLocalDefaultRegd());
#endif


	/*
	 * Initialize regd control information
	 */
	rlmDomainResetCtrlInfo(FALSE);
}

#else
void
cfg80211_regd_set_wiphy(IN struct wiphy *prWiphy)
{
}
#endif

int mtk_cfg80211_suspend(struct wiphy *wiphy,
			 struct cfg80211_wowlan *wow)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(REQ, INFO, "mtk_cfg80211_suspend\n");

#if (CFG_SUPPORT_STATISTICS == 1)
	wlanWakeDumpRes();
#endif
	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted() || !wiphy)
		goto end;

	WIPHY_PRIV(wiphy, prGlueInfo);


	if (prGlueInfo && prGlueInfo->prAdapter) {
		set_bit(SUSPEND_FLAG_FOR_WAKEUP_REASON,
			&prGlueInfo->prAdapter->ulSuspendFlag);
		set_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
			&prGlueInfo->prAdapter->ulSuspendFlag);

		if (prGlueInfo->prAdapter->u4HostStatusEmiOffset)
			kalSetSuspendFlagToEMI(prGlueInfo->prAdapter, TRUE);
#ifdef CFG_PDMA_SLPPRT_MODE_SUPPORT
		set_bit(GLUE_FLAG_WLAN_SUSPEND,
			&prGlueInfo->prAdapter->ulSuspendFlag);
#endif
	}
end:
	kalHaltUnlock();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief cfg80211 resume callback, will be invoked in wiphy_resume.
 *
 * @param wiphy: pointer to wiphy
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_resume(struct wiphy *wiphy)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus, u4InfoLen;

	DBGLOG(REQ, INFO, "mtk_cfg80211_resume\n");

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted() || !wiphy)
		goto end;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		goto end;
#ifdef CFG_PDMA_SLPPRT_MODE_SUPPORT
	set_bit(GLUE_FLAG_WLAN_RESUME,
			&prAdapter->ulSuspendFlag);
#endif
	clear_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
		  &prAdapter->ulSuspendFlag);

	rStatus = kalIoctl(prGlueInfo,
			wlanoidIndicateBssInfo,
			(void *) NULL,
			0,
			FALSE, FALSE, FALSE, &u4InfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "ScanResultLog error:%x\n",
		       rStatus);
	if (prGlueInfo->prAdapter->u4HostStatusEmiOffset)
		kalSetSuspendFlagToEMI(prGlueInfo->prAdapter, FALSE);
end:
	kalHaltUnlock();

	return 0;
}

#if CFG_ENABLE_UNIFY_WIPHY
/*----------------------------------------------------------------------------*/
/*!
 * @brief Check the net device is P2P net device (P2P GO/GC, AP), or not.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : the net device
 *
 * @retval 0:  AIS device (STA/IBSS)
 *         1:  P2P GO/GC, AP
 */
/*----------------------------------------------------------------------------*/
int mtk_IsP2PNetDevice(struct GLUE_INFO *prGlueInfo,
		       struct net_device *ndev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
	int iftype = 0;
	int ret = 1;

	if (ndev == NULL) {
		DBGLOG(REQ, WARN, "ndev is NULL\n");
		return -1;
	}

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			  netdev_priv(ndev);
	iftype = ndev->ieee80211_ptr->iftype;

	/* P2P device/GO/GC always return 1 */
	if (prNetDevPrivate->ucIsP2p == TRUE)
		ret = 1;
	else if (iftype == NL80211_IFTYPE_STATION)
		ret = 0;
	else if (iftype == NL80211_IFTYPE_ADHOC)
		ret = 0;
#if CFG_SUPPORT_NAN
	else if (prNetDevPrivate->ucIsNan == TRUE)
		ret = 0;
#endif

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialize the AIS related FSM and data.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : the net device
 *        ucBssIdx   : the AIS BSS index adssigned by the driver (wlanProbe)
 *
 * @retval 0
 *
 */
/*----------------------------------------------------------------------------*/
int mtk_init_sta_role(struct ADAPTER *prAdapter,
		      struct net_device *ndev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNdevPriv = NULL;
	uint8_t ucBssIndex = 0;

	if ((prAdapter == NULL) || (ndev == NULL))
		return -1;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex))
		return -1;

	/* init AIS FSM */
	aisFsmInit(prAdapter, ucBssIndex);

#if CFG_SUPPORT_ROAMING
	/* Roaming Module - intiailization */
	roamingFsmInit(prAdapter, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

	ndev->netdev_ops = wlanGetNdevOps();
	ndev->ieee80211_ptr->iftype = NL80211_IFTYPE_STATION;

	/* set the ndev's ucBssIdx to the AIS BSS index */
	prNdevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		     netdev_priv(ndev);
	prNdevPriv->ucBssIdx = ucBssIndex;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Uninitialize the AIS related FSM and data.
 *
 * @param prAdapter : the driver private data
 *
 * @retval 0
 *
 */
/*----------------------------------------------------------------------------*/
int mtk_uninit_sta_role(struct ADAPTER *prAdapter,
			struct net_device *ndev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNdevPriv = NULL;
	uint8_t ucBssIndex = 0;

	if ((prAdapter == NULL) || (ndev == NULL))
		return -1;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex))
		return -1;

#if CFG_SUPPORT_ROAMING
	/* Roaming Module - unintiailization */
	roamingFsmUninit(prAdapter, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

	/* uninit AIS FSM */
	aisFsmUninit(prAdapter, ucBssIndex);

	/* set the ucBssIdx to the illegal value */
	prNdevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		     netdev_priv(ndev);
	prNdevPriv->ucBssIdx = 0xff;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialize the AP (P2P) related FSM and data.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : net device
 *
 * @retval 0      : success
 *         others : can't alloc and setup the AP FSM & data
 *
 */
/*----------------------------------------------------------------------------*/
int mtk_init_ap_role(struct GLUE_INFO *prGlueInfo,
		     struct net_device *ndev)
{
	uint8_t u4Idx = 0;
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;

	for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
		if (gprP2pRoleWdev[u4Idx] == NULL)
			break;
	}

	if (u4Idx >= KAL_P2P_NUM) {
		DBGLOG(INIT, ERROR, "There is no free gprP2pRoleWdev.\n");
		return -ENOMEM;
	}

	if ((u4Idx == 0) ||
	    (prAdapter == NULL) ||
	    (prAdapter->rP2PNetRegState !=
	     ENUM_NET_REG_STATE_REGISTERED)) {
		DBGLOG(INIT, ERROR,
		       "The wlan0 can't set to AP without p2p0\n");
		/* System will crash, if p2p0 isn't existing. */
		return -EFAULT;
	}

	/* reference from the glRegisterP2P() */
	gprP2pRoleWdev[u4Idx] = ndev->ieee80211_ptr;
	if (glSetupP2P(prGlueInfo, gprP2pRoleWdev[u4Idx], ndev,
		       u4Idx, TRUE)) {
		gprP2pRoleWdev[u4Idx] = NULL;
		return -EFAULT;
	}

	prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum++;

	/* reference from p2pNetRegister() */
	/* The ndev doesn't need register_netdev, only reassign the gPrP2pDev.*/
	gPrP2pDev[u4Idx] = ndev;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Unnitialize the AP (P2P) related FSM and data.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : net device
 *
 * @retval 0      : success
 *         others : can't find the AP information by the ndev
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
mtk_oid_uninit_ap_role(struct ADAPTER *prAdapter, void *pvSetBuffer,
	uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	unsigned char u4Idx = 0;

	if ((prAdapter == NULL) || (pvSetBuffer == NULL)
		|| (pu4SetInfoLen == NULL))
		return WLAN_STATUS_FAILURE;

	/* init */
	*pu4SetInfoLen = sizeof(unsigned char);
	if (u4SetBufferLen < sizeof(unsigned char))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4Idx = *(unsigned char *) pvSetBuffer;

	DBGLOG(INIT, INFO, "ucRoleIdx = %d\n", u4Idx);

	glUnregisterP2P(prAdapter->prGlueInfo, u4Idx);

	gPrP2pDev[u4Idx] = NULL;
	gprP2pRoleWdev[u4Idx] = NULL;

	return 0;

}

int mtk_uninit_ap_role(struct GLUE_INFO *prGlueInfo,
		       struct net_device *ndev)
{
	unsigned char u4Idx;
	uint32_t rStatus;
	uint32_t u4BufLen;

	if (!prGlueInfo) {
		DBGLOG(INIT, WARN, "prGlueInfo is NULL\n");
		return -EINVAL;
	}
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &u4Idx) != 0) {
		DBGLOG(INIT, WARN,
		       "can't find the matched dev to uninit AP\n");
		return -EFAULT;
	}

	rStatus = kalIoctl(prGlueInfo,
				mtk_oid_uninit_ap_role, &u4Idx,
				sizeof(unsigned char),
				FALSE, FALSE, FALSE,
				&u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	return 0;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_start_radar_detection(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_chan_def *chandef,
				  unsigned int cac_time_ms)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_start_radar_detection(wiphy,
						      dev,
						      chandef,
						      cac_time_ms);

}
#else
int mtk_cfg_start_radar_detection(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_chan_def *chandef)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_start_radar_detection(wiphy, dev, chandef);

}

#endif


#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_channel_switch(struct wiphy *wiphy,
			   struct net_device *dev,
			   struct cfg80211_csa_settings *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_channel_switch(wiphy, dev, params);

}
#endif
#endif

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       unsigned char name_assign_type,
				       enum nl80211_iftype type,
				       struct vif_params *params)
#elif KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       unsigned char name_assign_type,
				       enum nl80211_iftype type,
				       u32 *flags,
				       struct vif_params *params)
#else
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       enum nl80211_iftype type,
				       u32 *flags,
				       struct vif_params *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	u32 *flags = NULL;
#endif

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return ERR_PTR(-EFAULT);
	}

	/* TODO: error handele for the non-P2P interface */

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 == 0)
	DBGLOG(REQ, WARN, "P2P is not supported\n");
	return ERR_PTR(-EINVAL);
#else	/* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
	return mtk_p2p_cfg80211_add_iface(wiphy, name,
					  name_assign_type, type,
					  flags, params);
#else	/* KERNEL_VERSION > (4, 1, 0) */
	return mtk_p2p_cfg80211_add_iface(wiphy, name, type, flags,
					  params);
#endif	/* KERNEL_VERSION */
#endif  /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
}

int mtk_cfg_del_iface(struct wiphy *wiphy,
		      struct wireless_dev *wdev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	/* TODO: error handele for the non-P2P interface */
#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 == 0)
	DBGLOG(REQ, WARN, "P2P is not supported\n");
	return -EINVAL;
#else	/* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
	return mtk_p2p_cfg80211_del_iface(wiphy, wdev);
#endif  /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_change_iface(struct wiphy *wiphy,
			 struct net_device *ndev,
			 enum nl80211_iftype type,
			 struct vif_params *params)
#else
int mtk_cfg_change_iface(struct wiphy *wiphy,
			 struct net_device *ndev,
			 enum nl80211_iftype type, u32 *flags,
			 struct vif_params *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetdevPriv = NULL;
	struct P2P_INFO *prP2pInfo = NULL;
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	u32 *flags = NULL;
#endif

	GLUE_SPIN_LOCK_DECLARATION();

	WIPHY_PRIV(wiphy, prGlueInfo);
	ASSERT(prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (!ndev) {
		DBGLOG(REQ, WARN, "ndev is NULL\n");
		return -EINVAL;
	}

	prNetdevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		       netdev_priv(ndev);

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211)
	/* for p2p0(GO/GC) & ap0(SAP): mtk_p2p_cfg80211_change_iface
	 * for wlan0 (STA/SAP): the following mtk_cfg_change_iface process
	 */
	if (!wlanIsAisDev(ndev)) {
		return mtk_p2p_cfg80211_change_iface(wiphy, ndev, type,
						     flags, params);
	}
#endif /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */

	DBGLOG(P2P, INFO, "ndev=%p, new type=%d\n", ndev, type);

	prAdapter = prGlueInfo->prAdapter;

	if (ndev->ieee80211_ptr->iftype == type) {
		DBGLOG(REQ, INFO, "ndev type is not changed (%d)\n", type);
		return 0;
	}

	netif_carrier_off(ndev);
	/* stop ap will stop all queue, and kalIndicateStatusAndComplete only do
	 * netif_carrier_on. So that, the following STA can't send 4-way M2 to
	 * AP.
	 */
	netif_tx_start_all_queues(ndev);

	/* flush scan */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if ((prGlueInfo->prScanRequest != NULL) &&
	    (prGlueInfo->prScanRequest->wdev == ndev->ieee80211_ptr)) {
		kalCfg80211ScanDone(prGlueInfo->prScanRequest, TRUE);
		prGlueInfo->prScanRequest = NULL;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	/* expect that only AP & STA will be handled here (excluding IBSS) */

	if (type == NL80211_IFTYPE_AP) {
		/* STA mode change to AP mode */
		prP2pInfo = prAdapter->prP2pInfo;

		if (prP2pInfo == NULL) {
			DBGLOG(INIT, ERROR, "prP2pInfo is NULL\n");
			return -EFAULT;
		}

		if (prP2pInfo->u4DeviceNum >= KAL_P2P_NUM) {
			DBGLOG(INIT, ERROR, "resource invalid, u4DeviceNum=%d\n"
			       , prP2pInfo->u4DeviceNum);
			return -EFAULT;
		}

		mtk_uninit_sta_role(prAdapter, ndev);

		if (mtk_init_ap_role(prGlueInfo, ndev) != 0) {
			DBGLOG(INIT, ERROR, "mtk_init_ap_role FAILED\n");

			/* Only AP/P2P resource has the failure case.	*/
			/* So, just re-init AIS.			*/
			mtk_init_sta_role(prAdapter, ndev);
			return -EFAULT;
		}
	} else {
		/* AP mode change to STA mode */
		if (mtk_uninit_ap_role(prGlueInfo, ndev) != 0) {
			DBGLOG(INIT, ERROR, "mtk_uninit_ap_role FAILED\n");
			return -EFAULT;
		}

		mtk_init_sta_role(prAdapter, ndev);

		/* continue the mtk_cfg80211_change_iface() process */
		mtk_cfg80211_change_iface(wiphy, ndev, type, flags, params);
	}

	return 0;
}

#if (KERNEL_VERSION(6, 1, 0) <= CFG80211_VERSION_CODE)
int mtk_cfg_add_key(struct wiphy *wiphy,
		    struct net_device *ndev, int link_id, u8 key_index,
		    bool pairwise, const u8 *mac_addr,
		    struct key_params *params)
#else
int mtk_cfg_add_key(struct wiphy *wiphy,
		    struct net_device *ndev, u8 key_index,
		    bool pairwise, const u8 *mac_addr,
		    struct key_params *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_add_key(wiphy, ndev, key_index,
						pairwise, mac_addr, params);
	}
	/* STA Mode */
	return mtk_cfg80211_add_key(wiphy, ndev, key_index,
				    pairwise,
				    mac_addr, params);
}

#if (KERNEL_VERSION(6, 1, 0) <= CFG80211_VERSION_CODE)
int mtk_cfg_get_key(struct wiphy *wiphy,
		    struct net_device *ndev, int link_id, u8 key_index,
		    bool pairwise, const u8 *mac_addr, void *cookie,
		    void (*callback)(void *cookie, struct key_params *))
#else
int mtk_cfg_get_key(struct wiphy *wiphy,
		    struct net_device *ndev, u8 key_index,
		    bool pairwise, const u8 *mac_addr, void *cookie,
		    void (*callback)(void *cookie, struct key_params *))
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_get_key(wiphy, ndev, key_index,
					pairwise, mac_addr, cookie, callback);
	}
	/* STA Mode */
	return mtk_cfg80211_get_key(wiphy, ndev, key_index,
				    pairwise, mac_addr, cookie, callback);
}

#if (KERNEL_VERSION(6, 1, 0) <= CFG80211_VERSION_CODE)
int mtk_cfg_del_key(struct wiphy *wiphy,
		    struct net_device *ndev, int link_id, u8 key_index,
		    bool pairwise, const u8 *mac_addr)
#else
int mtk_cfg_del_key(struct wiphy *wiphy,
		    struct net_device *ndev, u8 key_index,
		    bool pairwise, const u8 *mac_addr)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_del_key(wiphy, ndev, key_index,
						pairwise, mac_addr);
	}
	/* STA Mode */
	return mtk_cfg80211_del_key(wiphy, ndev, key_index,
				    pairwise, mac_addr);
}

#if (KERNEL_VERSION(6, 1, 0) <= CFG80211_VERSION_CODE)
int mtk_cfg_set_default_key(struct wiphy *wiphy,
			    struct net_device *ndev, int link_id,
			    u8 key_index, bool unicast, bool multicast)
#else
int mtk_cfg_set_default_key(struct wiphy *wiphy,
			    struct net_device *ndev,
			    u8 key_index, bool unicast, bool multicast)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_set_default_key(wiphy, ndev,
						key_index, unicast, multicast);
	}
	/* STA Mode */
	return mtk_cfg80211_set_default_key(wiphy, ndev,
					    key_index, unicast, multicast);
}

#if (KERNEL_VERSION(6, 1, 0) <= CFG80211_VERSION_CODE)
int mtk_cfg_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *ndev, int link_id, u8 key_index)
#else
int mtk_cfg_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *ndev, u8 key_index)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_set_mgmt_key(wiphy, ndev, key_index);
	/* STA Mode */
	DBGLOG(REQ, WARN, "STA don't support this function\n");
	return -EFAULT;
}

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_get_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac, struct station_info *sinfo)
#else
int mtk_cfg_get_station(struct wiphy *wiphy,
			struct net_device *ndev,
			u8 *mac, struct station_info *sinfo)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_get_station(wiphy, ndev, mac,
						    sinfo);
	/* STA Mode */
	return mtk_cfg80211_get_station(wiphy, ndev, mac, sinfo);
}

#if CFG_SUPPORT_TDLS
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_change_station(struct wiphy *wiphy,
			   struct net_device *ndev,
			   const u8 *mac, struct station_parameters *params)
#else
int mtk_cfg_change_station(struct wiphy *wiphy,
			   struct net_device *ndev,
			   u8 *mac, struct station_parameters *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_change_station(
			wiphy, ndev, mac, params);
	}
	/* STA Mode */
	return mtk_cfg80211_change_station(wiphy, ndev, mac,
					   params);
}

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_add_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac, struct station_parameters *params)
#else
int mtk_cfg_add_station(struct wiphy *wiphy,
			struct net_device *ndev,
			u8 *mac, struct station_parameters *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_add_station(wiphy, ndev, mac);
	}
	/* STA Mode */
	return mtk_cfg80211_add_station(wiphy, ndev, mac, params);
}

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_oper(struct wiphy *wiphy,
		      struct net_device *ndev,
		      const u8 *peer, enum nl80211_tdls_operation oper)
#else
int mtk_cfg_tdls_oper(struct wiphy *wiphy,
		      struct net_device *ndev,
		      u8 *peer, enum nl80211_tdls_operation oper)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}
	/* STA Mode */
	return mtk_cfg80211_tdls_oper(wiphy, ndev, peer, oper);
}

#if KERNEL_VERSION(6, 3, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev, const u8 *peer,
		      int link_id, u8 action_code, u8 dialog_token,
		      u16 status_code, u32 peer_capability,
		      bool initiator, const u8 *buf, size_t len)
#elif KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev,
		      const u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code, u32 peer_capability,
		      bool initiator, const u8 *buf, size_t len)
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev,
		      const u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code, u32 peer_capability,
		      const u8 *buf, size_t len)
#else
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev,
		      u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code,
		      const u8 *buf, size_t len)
#endif
{
	struct GLUE_INFO *prGlueInfo;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
			dialog_token, status_code, peer_capability, initiator,
			buf, len);
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
			dialog_token, status_code, peer_capability,
			buf, len);
#else
	return mtk_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
				      dialog_token, status_code,
				      buf, len);
#endif
}
#endif /* CFG_SUPPORT_TDLS */

#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev,
			struct station_del_parameters *params)
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac)
#else
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev, u8 *mac)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
		return mtk_p2p_cfg80211_del_station(wiphy, ndev, params);
#else
		return mtk_p2p_cfg80211_del_station(wiphy, ndev, mac);
#endif
	}
	/* STA Mode */
#if CFG_SUPPORT_TDLS
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_del_station(wiphy, ndev, params);
#else	/* CFG80211_VERSION_CODE > KERNEL_VERSION(3, 19, 0) */
	return mtk_cfg80211_del_station(wiphy, ndev, mac);
#endif	/* CFG80211_VERSION_CODE */
#else	/* CFG_SUPPORT_TDLS == 0 */
	/* AIS only support this function when CFG_SUPPORT_TDLS */
	return -EFAULT;
#endif	/* CFG_SUPPORT_TDLS */
}

int mtk_cfg_scan(struct wiphy *wiphy,
		 struct cfg80211_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo,
			       request->wdev->netdev) > 0)
		return mtk_p2p_cfg80211_scan(wiphy, request);
	/* STA Mode */
	return mtk_cfg80211_scan(wiphy, request);
}

#if KERNEL_VERSION(4, 5, 0) <= CFG80211_VERSION_CODE
void mtk_cfg_abort_scan(struct wiphy *wiphy,
			struct wireless_dev *wdev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0)
		mtk_p2p_cfg80211_abort_scan(wiphy, wdev);
	else	/* STA Mode */
		mtk_cfg80211_abort_scan(wiphy, wdev);
}
#endif

#if CFG_SUPPORT_SCHED_SCAN
int mtk_cfg_sched_scan_start(IN struct wiphy *wiphy,
			     IN struct net_device *ndev,
			     IN struct cfg80211_sched_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_sched_scan_start(wiphy, ndev, request);

}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_sched_scan_stop(IN struct wiphy *wiphy,
			    IN struct net_device *ndev,
			    IN u64 reqid)
#else
int mtk_cfg_sched_scan_stop(IN struct wiphy *wiphy,
			    IN struct net_device *ndev)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return 0;
	}
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_sched_scan_stop(wiphy, ndev, reqid);
#else
	return mtk_cfg80211_sched_scan_stop(wiphy, ndev);
#endif
}
#endif /* CFG_SUPPORT_SCHED_SCAN */

int mtk_cfg_connect(struct wiphy *wiphy,
		    struct net_device *ndev,
		    struct cfg80211_connect_params *sme)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_connect(wiphy, ndev, sme);
	/* STA Mode */
	return mtk_cfg80211_connect(wiphy, ndev, sme);
}

int mtk_cfg_update_connect_params(struct wiphy *wiphy,
		  struct net_device *ndev,
		  struct cfg80211_connect_params *sme,
		  u32 changed){
	uint8_t ucBssIndex = 0;
	uint32_t u4BufLen;
	uint32_t rStatus;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CONNECT rNewSsid;

	if (!(changed & UPDATE_ASSOC_IES))
		return 0;

	if (!(sme->ie && sme->ie_len))
		return 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	ucBssIndex = wlanGetBssIdx(ndev);

	DBGLOG(REQ, INFO, "[wlan%d] update connect %p %zu %d\n",
		ucBssIndex, sme->ie, sme->ie_len, changed);
	rNewSsid.pucIEs = (uint8_t *)sme->ie;
	rNewSsid.u4IesLen = sme->ie_len;
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidUpdateConnect,
		   (void *)&rNewSsid, sizeof(struct PARAM_CONNECT),
		   FALSE, FALSE, TRUE, &u4BufLen, ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "update SSID:%x\n", rStatus);
		return -EINVAL;
	}
	return 0;
}


int mtk_cfg_disconnect(struct wiphy *wiphy,
		       struct net_device *ndev,
		       u16 reason_code)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_disconnect(wiphy, ndev,
						   reason_code);
	/* STA Mode */
	return mtk_cfg80211_disconnect(wiphy, ndev, reason_code);
}

int mtk_cfg_join_ibss(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_ibss_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_join_ibss(wiphy, ndev, params);
	/* STA Mode */
	return mtk_cfg80211_join_ibss(wiphy, ndev, params);
}

int mtk_cfg_leave_ibss(struct wiphy *wiphy,
		       struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_leave_ibss(wiphy, ndev);
	/* STA Mode */
	return mtk_cfg80211_leave_ibss(wiphy, ndev);
}

int mtk_cfg_set_power_mgmt(struct wiphy *wiphy,
			   struct net_device *ndev,
			   bool enabled, int timeout)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_set_power_mgmt(wiphy, ndev,
						       enabled, timeout);
	}
	/* STA Mode */
	return mtk_cfg80211_set_power_mgmt(wiphy, ndev, enabled,
					   timeout);
}

int mtk_cfg_set_pmksa(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_set_pmksa(wiphy, ndev, pmksa);
}

int mtk_cfg_del_pmksa(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_del_pmksa(wiphy, ndev, pmksa);
}

int mtk_cfg_flush_pmksa(struct wiphy *wiphy,
			struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, TRACE, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_flush_pmksa(wiphy, ndev);
}

#if CONFIG_SUPPORT_GTK_REKEY
int mtk_cfg_set_rekey_data(struct wiphy *wiphy,
			   struct net_device *dev,
			   struct cfg80211_gtk_rekey_data *data)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_set_rekey_data(wiphy, dev, data);
}
#endif /* CONFIG_SUPPORT_GTK_REKEY */

int mtk_cfg_suspend(struct wiphy *wiphy,
		    struct cfg80211_wowlan *wow)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!wlanIsDriverReady(prGlueInfo)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return 0;
	}

	/* TODO: AP/P2P do not support this function, should take that case. */
	return mtk_cfg80211_suspend(wiphy, wow);
}

int mtk_cfg_resume(struct wiphy *wiphy)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!wlanIsDriverReady(prGlueInfo)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return 0;
	}

	/* TODO: AP/P2P do not support this function, should take that case. */
	return mtk_cfg80211_resume(wiphy);
}

int mtk_cfg_assoc(struct wiphy *wiphy,
		  struct net_device *ndev,
		  struct cfg80211_assoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}
	/* STA Mode */
	return mtk_cfg80211_assoc(wiphy, ndev, req);
}

int mtk_cfg_remain_on_channel(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      struct ieee80211_channel *chan,
			      unsigned int duration, u64 *cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_remain_on_channel(wiphy, wdev, chan,
				duration, cookie);
	}
	/* STA Mode */
	return mtk_cfg80211_remain_on_channel(wiphy, wdev, chan,
					      duration, cookie);
}

int mtk_cfg_cancel_remain_on_channel(struct wiphy *wiphy,
				     struct wireless_dev *wdev, u64 cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_cancel_remain_on_channel(wiphy,
				wdev,
				cookie);
	}
	/* STA Mode */
	return mtk_cfg80211_cancel_remain_on_channel(wiphy, wdev,
			cookie);
}

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_mgmt_tx(struct wiphy *wiphy,
		    struct wireless_dev *wdev,
		    struct cfg80211_mgmt_tx_params *params, u64 *cookie)
#else
int mtk_cfg_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		struct ieee80211_channel *channel, bool offchan,
		unsigned int wait, const u8 *buf, size_t len, bool no_cck,
		bool dont_wait_for_ack, u64 *cookie)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0)
		return mtk_p2p_cfg80211_mgmt_tx(wiphy, wdev, params,
						cookie);
	/* STA Mode */
	return mtk_cfg80211_mgmt_tx(wiphy, wdev, params, cookie);
#else /* KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE */
	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_mgmt_tx(wiphy, wdev, channel, offchan,
			wait, buf, len, no_cck, dont_wait_for_ack, cookie);
	}
	/* STA Mode */
	return mtk_cfg80211_mgmt_tx(wiphy, wdev, channel, offchan, wait, buf,
			len, no_cck, dont_wait_for_ack, cookie);
#endif
}

void mtk_cfg_mgmt_frame_register(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 u16 frame_type, bool reg)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		mtk_p2p_cfg80211_mgmt_frame_register(wiphy, wdev,
						     frame_type,
						     reg);
	} else {
		mtk_cfg80211_mgmt_frame_register(wiphy, wdev, frame_type,
						 reg);
	}
}

#if KERNEL_VERSION(5, 8, 0) <= CFG80211_VERSION_CODE
void mtk_cfg_mgmt_frame_update(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				struct mgmt_frame_regs *upd)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	u_int8_t fgIsP2pNetDevice = FALSE;
	uint32_t *pu4PacketFilter = NULL;

	if ((wiphy == NULL) || (wdev == NULL) || (upd == NULL)) {
		DBGLOG(INIT, TRACE, "Invalidate params\n");
		return;
	}
	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return;
	}
	fgIsP2pNetDevice = mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev);

	DBGLOG(INIT, TRACE,
		"netdev(0x%p) update management frame filter: 0x%08x\n",
		wdev->netdev, upd->interface_stypes);
	do {
		if (fgIsP2pNetDevice) {
			uint8_t ucRoleIdx = 0;
			struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
				(struct P2P_ROLE_FSM_INFO *) NULL;

			if (prGlueInfo->prP2PInfo[0]->prDevHandler ==
				wdev->netdev) {
				pu4PacketFilter =
					&prGlueInfo->prP2PDevInfo
					->u4OsMgmtFrameFilter;
				/* Reset filters*/
				*pu4PacketFilter = 0;
			} else {
				if (mtk_Netdev_To_RoleIdx(prGlueInfo,
					wdev->netdev, &ucRoleIdx) < 0) {
					DBGLOG(P2P, WARN,
						"wireless dev match fail!\n");
					break;
				}
				/* Non P2P device*/
				ASSERT(ucRoleIdx < KAL_P2P_NUM);
				DBGLOG(P2P, TRACE,
					"Open packet filer RoleIdx %u\n",
					ucRoleIdx);
				prP2pRoleFsmInfo =
					prGlueInfo->prAdapter->rWifiVar
					.aprP2pRoleFsmInfo[ucRoleIdx];
				pu4PacketFilter = &prP2pRoleFsmInfo
					->u4P2pPacketFilter;
				*pu4PacketFilter =
					PARAM_PACKET_FILTER_SUPPORTED;
			}
		} else {
			pu4PacketFilter = &prGlueInfo->u4OsMgmtFrameFilter;
			*pu4PacketFilter = 0;
		}
		if (upd->interface_stypes & MASK_MAC_FRAME_PROBE_REQ)
			*pu4PacketFilter |= PARAM_PACKET_FILTER_PROBE_REQ;

		if (upd->interface_stypes & MASK_MAC_FRAME_ACTION)
			*pu4PacketFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
#if CFG_SUPPORT_SOFTAP_WPA3
		if (upd->interface_stypes & MASK_MAC_FRAME_AUTH)
			*pu4PacketFilter |= PARAM_PACKET_FILTER_AUTH;

		if (upd->interface_stypes & MASK_MAC_FRAME_ASSOC_REQ)
			*pu4PacketFilter |= PARAM_PACKET_FILTER_ASSOC_REQ;
#endif

		set_bit(fgIsP2pNetDevice ?
			GLUE_FLAG_FRAME_FILTER_BIT :
			GLUE_FLAG_FRAME_FILTER_AIS_BIT,
			&prGlueInfo->ulFlag);

		/* wake up main thread */
		wake_up_interruptible(&prGlueInfo->waitq);
	} while (FALSE);
}
#endif

#ifdef CONFIG_NL80211_TESTMODE
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_testmode_cmd(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 void *data, int len)
#else
int mtk_cfg_testmode_cmd(struct wiphy *wiphy, void *data,
			 int len)
#endif
{
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_testmode_cmd(wiphy, wdev, data,
						     len);
	}

	return mtk_cfg80211_testmode_cmd(wiphy, wdev, data, len);
#else
	/* XXX: no information can to check the mtk_IsP2PNetDevice */
	/* return mtk_p2p_cfg80211_testmode_cmd(wiphy, data, len); */
	return mtk_cfg80211_testmode_cmd(wiphy, data, len);
#endif
}
#endif	/* CONFIG_NL80211_TESTMODE */

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0)
int mtk_cfg_change_bss(struct wiphy *wiphy,
		       struct net_device *dev,
		       struct bss_parameters *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_change_bss(wiphy, dev, params);
}

int mtk_cfg_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				u64 cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) <= 0) {
		return mtk_cfg80211_mgmt_tx_cancel_wait(wiphy, wdev, cookie);
	}

	return mtk_p2p_cfg80211_mgmt_tx_cancel_wait(wiphy, wdev,
			cookie);
}

int mtk_cfg_deauth(struct wiphy *wiphy,
		   struct net_device *dev,
		   struct cfg80211_deauth_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_deauth(wiphy, dev, req);
}

int mtk_cfg_disassoc(struct wiphy *wiphy,
		     struct net_device *dev,
		     struct cfg80211_disassoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_disassoc(wiphy, dev, req);
}

int mtk_cfg_start_ap(struct wiphy *wiphy,
		     struct net_device *dev,
		     struct cfg80211_ap_settings *settings)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_start_ap(wiphy, dev, settings);
}

int mtk_cfg_change_beacon(struct wiphy *wiphy,
			  struct net_device *dev,
			  struct cfg80211_beacon_data *info)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_change_beacon(wiphy, dev, info);
}

#if (KERNEL_VERSION(5, 19, 2) <= CFG80211_VERSION_CODE)
int mtk_cfg_stop_ap(struct wiphy *wiphy, struct net_device *dev,
	unsigned int link_id)
#else
int mtk_cfg_stop_ap(struct wiphy *wiphy, struct net_device *dev)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_stop_ap(wiphy, dev);
}

int mtk_cfg_set_wiphy_params(struct wiphy *wiphy,
			     u32 changed)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	/* TODO: AIS not support this function */
	return mtk_p2p_cfg80211_set_wiphy_params(wiphy, changed);
}

#if (KERNEL_VERSION(5, 19, 2) <= CFG80211_VERSION_CODE)
int mtk_cfg_set_bitrate_mask(struct wiphy *wiphy,
			     struct net_device *dev,
			     unsigned int link_id,
			     const u8 *peer,
			     const struct cfg80211_bitrate_mask *mask)
#else
int mtk_cfg_set_bitrate_mask(struct wiphy *wiphy,
			     struct net_device *dev,
			     const u8 *peer,
			     const struct cfg80211_bitrate_mask *mask)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_set_bitrate_mask(wiphy, dev, peer,
			mask);
}

int mtk_cfg_set_txpower(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			enum nl80211_tx_power_setting type, int mbm)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_set_txpower(wiphy, wdev, type, mbm);
}

int mtk_cfg_get_txpower(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			int *dbm)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG_LIMITED(REQ, TRACE, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) <= 0) {
		DBGLOG_LIMITED(REQ, TRACE,
			"STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_get_txpower(wiphy, wdev, dbm);
}
#endif /* (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0) */
#endif	/* CFG_ENABLE_UNIFY_WIPHY */

/*-----------------------------------------------------------------------*/
/*!
 * @brief This function goes through the provided ies buffer, and
 *        collects those non-wfa vendor specific ies into driver's
 *        internal buffer (non_wfa_vendor_ie_buf), to be sent in
 *        AssocReq in AIS mode.
 *        The non-wfa vendor specific ies are those with ie_id = 0xdd
 *        and ouis are different from wfa's oui. (i.e., it could be
 *        customer's vendor ie ...etc.
 *
 * @param prGlueInfo    driver's private glueinfo
 *        ies           ie buffer
 *        len           length of ie
 *
 * @retval length of the non_wfa vendor ie
 */
/*-----------------------------------------------------------------------*/
uint16_t cfg80211_get_non_wfa_vendor_ie(struct GLUE_INFO *prGlueInfo,
	uint8_t *ies, int32_t len, uint8_t ucBssIndex)
{
	const uint8_t *pos = ies, *end = ies+len;
	struct ieee80211_vendor_ie *ie;
	int32_t ie_oui = 0;
	uint16_t *ret_len, max_len;
	uint8_t *w_pos;
	struct CONNECTION_SETTINGS *prConnSettings;

	if (!prGlueInfo || !ies || !len)
		return 0;

	prConnSettings =
		aisGetConnSettings(prGlueInfo->prAdapter,
		ucBssIndex);
	if (!prConnSettings)
		return 0;

	w_pos = prConnSettings->non_wfa_vendor_ie_buf;
	ret_len = &prConnSettings->non_wfa_vendor_ie_len;
	max_len = (uint16_t)sizeof(prConnSettings->non_wfa_vendor_ie_buf);

	while (pos < end) {
		pos = cfg80211_find_ie(WLAN_EID_VENDOR_SPECIFIC, pos,
				       end - pos);
		if (!pos)
			break;

		ie = (struct ieee80211_vendor_ie *)pos;

		/* Make sure we can access ie->len */
		BUILD_BUG_ON(offsetof(struct ieee80211_vendor_ie, len) != 1);

		if (ie->len < sizeof(*ie))
			goto cont;

		ie_oui = ie->oui[0] << 16 | ie->oui[1] << 8 | ie->oui[2];
		/*
		 * If oui is other than: 0x0050f2 & 0x506f9a,
		 * we consider it is non-wfa oui.
		 */
		if (ie_oui != WLAN_OUI_MICROSOFT && ie_oui != WLAN_OUI_WFA) {
			/*
			 * If remaining buf len is capable, we copy
			 * this ie to the buf.
			 */
			if (max_len-(*ret_len) >= ie->len+2) {
				DBGLOG(AIS, TRACE,
					   "vendor ie(len=%d, oui=0x%06x)\n",
					   ie->len, ie_oui);
				memcpy(w_pos, pos, ie->len+2);
				w_pos += (ie->len+2);
				(*ret_len) += ie->len+2;
			} else {
				/* Otherwise we give an error msg
				 * and return.
				 */
				DBGLOG(AIS, ERROR,
					"Insufficient buf for vendor ie, exit!\n");
				break;
			}
		}
cont:
		pos += 2 + ie->len;
	}
	return *ret_len;
}

int mtk_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *dev,
				 struct cfg80211_update_ft_ies_params *ftie)
{
#if CFG_SUPPORT_802_11R
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4InfoBufLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint8_t ucBssIndex = 0;

	if (!wiphy)
		return -1;
	WIPHY_PRIV(wiphy, prGlueInfo);

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidUpdateFtIes, (void *)ftie,
			   sizeof(*ftie), FALSE, FALSE, FALSE, &u4InfoBufLen,
			   ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "FT: update Ft IE failed\n");
#else
	DBGLOG(OID, INFO, "FT: 802.11R is not enabled\n");
#endif

	return 0;
}
