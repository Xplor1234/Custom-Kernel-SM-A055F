/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "wlan_p2p.h"
 *    \brief This file contains the declairations of Wi-Fi Direct command
 *	   processing routines for MediaTek Inc. 802.11 Wireless LAN Adapters.
 */


#ifndef _WLAN_P2P_H
#define _WLAN_P2P_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

#if CFG_ENABLE_WIFI_DIRECT
/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/* Service Discovery */
struct PARAM_P2P_SEND_SD_RESPONSE {
	uint8_t rReceiverAddr[PARAM_MAC_ADDR_LEN];
	uint8_t fgNeedTxDoneIndication;
	uint8_t ucChannelNum;
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[];	/*native 802.11 */
};

struct PARAM_P2P_GET_SD_REQUEST {
	uint8_t rTransmitterAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[];	/*native 802.11 */
};

struct PARAM_P2P_GET_SD_REQUEST_EX {
	uint8_t rTransmitterAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2PacketLength;
	/* Channel Number Where SD Request is received. */
	uint8_t ucChannelNum;
	uint8_t ucSeqNum;	/* Get SD Request by sequence number. */
	uint8_t aucPacketContent[];	/*native 802.11 */
};

struct PARAM_P2P_SEND_SD_REQUEST {
	uint8_t rReceiverAddr[PARAM_MAC_ADDR_LEN];
	uint8_t fgNeedTxDoneIndication;
	/* Indicate the Service Discovery Supplicant Version. */
	uint8_t ucVersionNum;
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[];	/*native 802.11 */
};

/* Service Discovery 1.0. */
struct PARAM_P2P_GET_SD_RESPONSE {
	uint8_t rTransmitterAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[];	/*native 802.11 */
};

/* Service Discovery 2.0. */
struct PARAM_P2P_GET_SD_RESPONSE_EX {
	uint8_t rTransmitterAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2PacketLength;
	uint8_t ucSeqNum;	/* Get SD Response by sequence number. */
	uint8_t aucPacketContent[];	/*native 802.11 */
};

struct PARAM_P2P_TERMINATE_SD_PHASE {
	uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN];
};

/*! \brief Key mapping of BSSID */
struct P2P_PARAM_KEY {
	uint32_t u4Length;	/*!< Length of structure */
	uint32_t u4KeyIndex;	/*!< KeyID */
	uint32_t u4KeyLength;	/*!< Key length in bytes */
	uint8_t arBSSID[PARAM_MAC_ADDR_LEN];	/*!< MAC address */
	uint64_t rKeyRSC;
	/* Following add to change the original windows structure */
	uint8_t ucBssIdx;	/* for specific P2P BSS interface. */
	int32_t i4LinkId;
	uint8_t ucCipher;
	uint8_t aucKeyMaterial[32];	/*!< Key content by above setting */
};

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

/*--------------------------------------------------------------*/
/* Routines to handle command                                   */
/*--------------------------------------------------------------*/
/* WLAN_STATUS */
/* wlanoidSetAddP2PKey(IN P_ADAPTER_T prAdapter, */
/* IN PVOID pvSetBuffer,
 * UINT_32 u4SetBufferLen,
 * PUINT_32 pu4SetInfoLen);
 */

/* WLAN_STATUS */
/* wlanoidSetRemoveP2PKey(IN P_ADAPTER_T prAdapter, */
/* IN PVOID pvSetBuffer,
 * UINT_32 u4SetBufferLen,
 * PUINT_32 pu4SetInfoLen);
 */

uint32_t
wlanoidSetNetworkAddress(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

/*--------------------------------------------------------------*/
/* Service Discovery Subroutines                                */
/*--------------------------------------------------------------*/
uint32_t
wlanoidSendP2PSDRequest(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSendP2PSDResponse(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidGetP2PSDRequest(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidGetP2PSDResponse(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *puQueryInfoLen);

uint32_t
wlanoidSetP2PTerminateSDPhase(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen);

#if CFG_SUPPORT_ANTI_PIRACY
uint32_t
wlanoidSetSecCheckRequest(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

/*WLAN_STATUS
 *wlanoidGetSecCheckResponse(IN P_ADAPTER_T prAdapter,
 *		IN PVOID pvQueryBuffer,
 *		UINT_32 u4QueryBufferLen,
 *		PUINT_32 pu4QueryInfoLen);
 */
#endif

uint32_t
wlanoidSetNoaParam(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetOppPsParam(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetUApsdParam(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryP2pPowerSaveProfile(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetP2pPowerSaveProfile(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetP2pSetNetworkAddress(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryP2pVersion(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetP2pSupplicantVersion(struct ADAPTER *prAdapter,
		void *pvSetBuffer,
		uint32_t u4SetBufferLen,
		uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
uint32_t
wlanoidSetP2pWPSmode(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen);
#endif

uint32_t
wlanoidRequestP2pScan(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidAbortP2pScan(struct ADAPTER *prAdapter,
		void *pvQueryBuffer,
		uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen);

/*--------------------------------------------------------------*/
/* Callbacks for event indication                               */
/*--------------------------------------------------------------*/

#endif
#endif /* _WLAN_P2P_H */
