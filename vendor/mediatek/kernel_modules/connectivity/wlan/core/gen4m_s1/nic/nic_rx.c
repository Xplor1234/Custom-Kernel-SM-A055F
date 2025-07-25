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
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_rx.c#5
 */

/*! \file   nic_rx.c
 *    \brief  Functions that provide many rx-related functions
 *
 *    This file includes the functions used to process RFB and dispatch RFBs to
 *    the appropriate related rx functions for protocols.
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
#include "que_mgt.h"
#include "wnm.h"
#if CFG_SUPPORT_NAN
#include "nan_data_engine.h"
#endif

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

#if CFG_MGMT_FRAME_HANDLING
static PROCESS_RX_MGT_FUNCTION
apfnProcessRxMgtFrame[MAX_NUM_OF_FC_SUBTYPES] = {
#if CFG_SUPPORT_AAA
	aaaFsmRunEventRxAssoc,	/* subtype 0000: Association request */
#else
	NULL,			/* subtype 0000: Association request */
#endif /* CFG_SUPPORT_AAA */
	saaFsmRunEventRxAssoc,	/* subtype 0001: Association response */
#if CFG_SUPPORT_AAA
	aaaFsmRunEventRxAssoc,	/* subtype 0010: Reassociation request */
#else
	NULL,			/* subtype 0010: Reassociation request */
#endif /* CFG_SUPPORT_AAA */
	saaFsmRunEventRxAssoc,	/* subtype 0011: Reassociation response */
#if CFG_SUPPORT_ADHOC || CFG_ENABLE_WIFI_DIRECT
	bssProcessProbeRequest,	/* subtype 0100: Probe request */
#else
	NULL,			/* subtype 0100: Probe request */
#endif /* CFG_SUPPORT_ADHOC */
	scanProcessBeaconAndProbeResp,	/* subtype 0101: Probe response */
	NULL,			/* subtype 0110: reserved */
	NULL,			/* subtype 0111: reserved */
	scanProcessBeaconAndProbeResp,	/* subtype 1000: Beacon */
	NULL,			/* subtype 1001: ATIM */
	saaFsmRunEventRxDisassoc,	/* subtype 1010: Disassociation */
	authCheckRxAuthFrameTransSeq,	/* subtype 1011: Authentication */
	saaFsmRunEventRxDeauth,	/* subtype 1100: Deauthentication */
	nicRxProcessActionFrame,	/* subtype 1101: Action */
	NULL,			/* subtype 1110: reserved */
	NULL			/* subtype 1111: reserved */
};
#endif

static struct RX_EVENT_HANDLER arEventTable[] = {
	{EVENT_ID_RX_ADDBA,	qmHandleEventRxAddBa},
	{EVENT_ID_DBDC_SWITCH_DONE, cnmDbdcEventHwSwitchDone},
	{EVENT_ID_RX_DELBA,	qmHandleEventRxDelBa},
	{EVENT_ID_LINK_QUALITY, nicEventLinkQuality},
	{EVENT_ID_LAYER_0_EXT_MAGIC_NUM, nicEventLayer0ExtMagic},
	{EVENT_ID_MIC_ERR_INFO,	nicEventMicErrorInfo},
	{EVENT_ID_SCAN_DONE, nicEventScanDone},
	{EVENT_ID_SCHED_SCAN_DONE, nicEventSchedScanDone},
	{EVENT_ID_TX_DONE, nicTxProcessTxDoneEvent},
	{EVENT_ID_SLEEPY_INFO, nicEventSleepyNotify},
#if CFG_ENABLE_BT_OVER_WIFI
	{EVENT_ID_BT_OVER_WIFI, nicEventBtOverWifi},
#endif
	{EVENT_ID_STATISTICS, nicEventStatistics},
	{EVENT_ID_TPUT_INFO, nicEventTputFactorHandler},
	{EVENT_ID_WTBL_INFO, nicEventWlanInfo},
	{EVENT_ID_MIB_INFO, nicEventMibInfo},
	{EVENT_ID_CH_PRIVILEGE, cnmChMngrHandleChEvent},
	{EVENT_ID_BSS_ABSENCE_PRESENCE, qmHandleEventBssAbsencePresence},
	{EVENT_ID_STA_CHANGE_PS_MODE, qmHandleEventStaChangePsMode},
	{EVENT_ID_STA_UPDATE_FREE_QUOTA, qmHandleEventStaUpdateFreeQuota},
	{EVENT_ID_BSS_BEACON_TIMEOUT, nicEventBeaconTimeout},
	{EVENT_ID_UPDATE_NOA_PARAMS, nicEventUpdateNoaParams},
	{EVENT_ID_STA_AGING_TIMEOUT, nicEventStaAgingTimeout},
	{EVENT_ID_AP_OBSS_STATUS, nicEventApObssStatus},
	{EVENT_ID_ROAMING_STATUS, nicEventRoamingStatus},
	{EVENT_ID_SEND_DEAUTH, nicEventSendDeauth},
	{EVENT_ID_UPDATE_RDD_STATUS, nicEventUpdateRddStatus},
	{EVENT_ID_UPDATE_BWCS_STATUS, nicEventUpdateBwcsStatus},
	{EVENT_ID_UPDATE_BCM_DEBUG, nicEventUpdateBcmDebug},
	{EVENT_ID_ADD_PKEY_DONE, nicEventAddPkeyDone},
	{EVENT_ID_DEBUG_MSG, nicEventDebugMsg},
	{EVENT_ID_TDLS, nicEventTdls},
#if (CFG_SUPPORT_HE_ER == 1)
	{EVENT_ID_BSS_ER_TX_MODE, bssProcessErTxModeEvent},
#endif

#if CFG_SUPPORT_LLS
	{EVENT_ID_STATS_LLS, nicEventStatsLinkStats},
#endif
	{EVENT_ID_RSSI_MONITOR, nicEventRssiMonitor},
	{EVENT_ID_DUMP_MEM, nicEventDumpMem},
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
	{EVENT_ID_CAL_ALL_DONE, nicEventCalAllDone},
#endif
#if CFG_SUPPORT_BAR_DELAY_INDICATION
	{EVENT_ID_RXM_DELAY_BAR, nicEventHandleDelayBar},
#endif
	{EVENT_ID_HIF_CTRL, nicEventHifCtrl},
	{EVENT_ID_RDD_SEND_PULSE, nicEventRddSendPulse},
#if (CFG_SUPPORT_DFS_MASTER == 1)
	{EVENT_ID_RDD_REPORT, cnmRadarDetectEvent},
	{EVENT_ID_CSA_DONE, cnmCsaDoneEvent},
#if CFG_SUPPORT_IDC_CH_SWITCH
	{EVENT_ID_LTE_IDC_REPORT, cnmIdcDetectHandler},
#endif
#endif
	{EVENT_ID_UPDATE_COEX_PHYRATE, nicEventUpdateCoexPhyrate},
	{EVENT_ID_UPDATE_COEX_STATUS, nicEventUpdateCoexStatus},
	{EVENT_ID_TX_ADDBA, qmHandleEventTxAddBa},
	{EVENT_ID_GET_CNM, nicEventCnmInfo},
#if CFG_SUPPORT_SMART_GEAR
	{EVENT_ID_SG_STATUS, cnmEventSGStatus},
#endif
	{EVENT_ID_OPMODE_CHANGE, cnmOpmodeEventHandler},
#if CFG_SUPPORT_LOWLATENCY_MODE
	{EVENT_ID_LOW_LATENCY_INFO, nicEventUpdateLowLatencyInfoStatus},
#endif
#if CFG_FAST_PATH_SUPPORT
	{EVENT_ID_FAST_PATH, fpEventHandler},
#endif
#if CFG_SUPPORT_NAN
	{ EVENT_ID_NAN_EXT_EVENT, nicNanEventDispatcher},
#endif
	{EVENT_ID_REPORT_U_EVENT, nicEventReportUEvent},
#if (CFG_SUPPORT_PKT_OFLD == 1)
	{EVENT_ID_PKT_OFLD, nicEventPktOfld},
#endif
};

static const struct ACTION_FRAME_SIZE_MAP arActionFrameReservedLen[] = {
	{(uint16_t)(CATEGORY_QOS_ACTION | ACTION_QOS_MAP_CONFIGURE << 8),
	 sizeof(struct _ACTION_QOS_MAP_CONFIGURE_FRAME)},
	{(uint16_t)(CATEGORY_PUBLIC_ACTION | ACTION_PUBLIC_20_40_COEXIST << 8),
	 OFFSET_OF(struct ACTION_20_40_COEXIST_FRAME, rChnlReport)},
	{(uint16_t)
	 (CATEGORY_PUBLIC_ACTION | ACTION_PUBLIC_VENDOR_SPECIFIC << 8),
	 sizeof(struct WLAN_PUBLIC_VENDOR_ACTION_FRAME)},
	{(uint16_t)(CATEGORY_HT_ACTION | ACTION_HT_NOTIFY_CHANNEL_WIDTH << 8),
	 sizeof(struct ACTION_NOTIFY_CHNL_WIDTH_FRAME)},
	{(uint16_t)(CATEGORY_HT_ACTION | ACTION_HT_SM_POWER_SAVE << 8),
	 sizeof(struct ACTION_SM_POWER_SAVE_FRAME)},
	{(uint16_t)(CATEGORY_SA_QUERY_ACTION | ACTION_SA_QUERY_REQUEST << 8),
	 sizeof(struct ACTION_SA_QUERY_FRAME)},
	{(uint16_t)
	 (CATEGORY_WNM_ACTION | ACTION_WNM_TIMING_MEASUREMENT_REQUEST << 8),
	 sizeof(struct ACTION_WNM_TIMING_MEAS_REQ_FRAME)},
	{(uint16_t)(CATEGORY_SPEC_MGT | ACTION_MEASUREMENT_REQ << 8),
	 sizeof(struct ACTION_SM_REQ_FRAME)},
	{(uint16_t)(CATEGORY_SPEC_MGT | ACTION_MEASUREMENT_REPORT << 8),
	 sizeof(struct ACTION_SM_REQ_FRAME)},
	{(uint16_t)(CATEGORY_SPEC_MGT | ACTION_TPC_REQ << 8),
	 sizeof(struct ACTION_SM_REQ_FRAME)},
	{(uint16_t)(CATEGORY_SPEC_MGT | ACTION_TPC_REPORT << 8),
	 sizeof(struct ACTION_SM_REQ_FRAME)},
	{(uint16_t)(CATEGORY_SPEC_MGT | ACTION_CHNL_SWITCH << 8),
	 sizeof(struct ACTION_SM_REQ_FRAME)},
	{(uint16_t)
	 (CATEGORY_VHT_ACTION | ACTION_OPERATING_MODE_NOTIFICATION << 8),
	 sizeof(struct ACTION_OP_MODE_NOTIFICATION_FRAME)},
#if (CFG_SUPPORT_TWT == 1)
	{(uint16_t)(CATEGORY_S1G_ACTION | ACTION_S1G_TWT_SETUP << 8),
	 sizeof(struct _ACTION_TWT_SETUP_FRAME)},
	{(uint16_t)(CATEGORY_S1G_ACTION | ACTION_S1G_TWT_TEARDOWN << 8),
	 sizeof(struct _ACTION_TWT_TEARDOWN_FRAME)},
	{(uint16_t)(CATEGORY_S1G_ACTION | ACTION_S1G_TWT_INFORMATION << 8),
	 sizeof(struct _ACTION_TWT_INFO_FRAME)},
#endif
	{(uint16_t)(CATEGORY_RM_ACTION | RM_ACTION_RM_REQUEST << 8),
	 sizeof(struct ACTION_RM_REQ_FRAME)},
	{(uint16_t)(CATEGORY_RM_ACTION | RM_ACTION_REIGHBOR_RESPONSE << 8),
	 sizeof(struct ACTION_NEIGHBOR_REPORT_FRAME)},
	{(uint16_t)(CATEGORY_WME_MGT_NOTIFICATION | ACTION_ADDTS_RSP << 8),
	 sizeof(struct WMM_ACTION_TSPEC_FRAME)},
	{(uint16_t)(CATEGORY_WME_MGT_NOTIFICATION | ACTION_DELTS << 8),
	 sizeof(struct WMM_ACTION_TSPEC_FRAME)},
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
 * @brief Initialize the RFBs
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxInitialize(IN struct ADAPTER *prAdapter)
{
	struct RX_CTRL *prRxCtrl;
	uint8_t *pucMemHandle;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	uint32_t i;

	DEBUGFUNC("nicRxInitialize");

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;

	/* 4 <0> Clear allocated memory. */
	kalMemZero((void *) prRxCtrl->pucRxCached,
		   prRxCtrl->u4RxCachedSize);

	/* 4 <1> Initialize the RFB lists */
	QUEUE_INITIALIZE(&prRxCtrl->rFreeSwRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rReceivedRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rIndicatedRfbList);

	pucMemHandle = prRxCtrl->pucRxCached;
	for (i = CFG_RX_MAX_PKT_NUM; i != 0; i--) {
		prSwRfb = (struct SW_RFB *) pucMemHandle;

		if (nicRxSetupRFB(prAdapter, prSwRfb)) {
			DBGLOG(RX, ERROR,
			       "nicRxInitialize failed: Cannot allocate packet buffer for SwRfb!\n");
			return;
		}
		nicRxReturnRFB(prAdapter, prSwRfb);

		pucMemHandle += ALIGN_4(sizeof(struct SW_RFB));
	}

	if (prRxCtrl->rFreeSwRfbList.u4NumElem !=
	    CFG_RX_MAX_PKT_NUM)
		ASSERT_NOMEM();
	/* Check if the memory allocation consist with this
	 * initialization function
	 */
	ASSERT((uint32_t) (pucMemHandle - prRxCtrl->pucRxCached) ==
	       prRxCtrl->u4RxCachedSize);

	/* 4 <2> Clear all RX counters */
	RX_RESET_ALL_CNTS(prRxCtrl);

	prRxCtrl->pucRxCoalescingBufPtr =
		prAdapter->pucCoalescingBufCached;

#if CFG_HIF_STATISTICS
	prRxCtrl->u4TotalRxAccessNum = 0;
	prRxCtrl->u4TotalRxPacketNum = 0;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
	prRxCtrl->u4QueuedCnt = 0;
	prRxCtrl->u4DequeuedCnt = 0;
#endif

}				/* end of nicRxInitialize() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Uninitialize the RFBs
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxUninitialize(IN struct ADAPTER *prAdapter)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	nicRxFlush(prAdapter);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rReceivedRfbList, prSwRfb,
				  struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo,
				prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
		} else {
			break;
		}
	} while (TRUE);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb,
				  struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo,
				prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
		} else {
			break;
		}
	} while (TRUE);

}				/* end of nicRxUninitialize() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Fill RFB
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb   specify the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxFillRFB(IN struct ADAPTER *prAdapter,
		  IN OUT struct SW_RFB *prSwRfb)
{
	struct RX_DESC_OPS_T *prRxDescOps = prAdapter->chip_info->prRxDescOps;

	if (prRxDescOps->nic_rxd_fill_rfb)
		prRxDescOps->nic_rxd_fill_rfb(prAdapter, prSwRfb);
	else
		DBGLOG(RX, ERROR,
			"%s:: no nic_rxd_fill_rfb??\n",
			__func__);
}

/**
 * nicRxProcessRxv() - function to parse RXV for rate information
 * @prAdapter: pointer to adapter
 * @prSwRfb: RFB of received frame
 *
 * If parsed data will be saved in
 * prAdapter->arStaRec[prSwRfb->ucStaRecIdx].u4RxV[*], then can be used
 * for calling wlanGetRxRate().
 */
void nicRxProcessRxv(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb)
{
#if (CFG_SUPPORT_MSP == 1)
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prAdapter->chip_info;

	if (!prChipInfo || !prChipInfo->asicRxProcessRxvforMSP)
		return;

	prChipInfo->asicRxProcessRxvforMSP(prAdapter, prSwRfb);
#endif /* CFG_SUPPORT_MSP == 1 */
}

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
/*----------------------------------------------------------------------------*/
/*!
 * @brief Fill checksum status in RFB
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 * @param u4TcpUdpIpCksStatus specify the Checksum status
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxFillChksumStatus(IN struct ADAPTER *prAdapter,
			   IN OUT struct SW_RFB *prSwRfb)
{
	struct RX_CSO_REPORT_T *rReport;
	uint32_t u4TcpUdpIpCksStatus;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	u4TcpUdpIpCksStatus = prSwRfb->u4TcpUdpIpCksStatus;
	rReport = (struct RX_CSO_REPORT_T *) &u4TcpUdpIpCksStatus;
	DBGLOG_LIMITED(RX, LOUD,
	       "RX_IPV4_STATUS=%d, RX_TCP_STATUS=%d, RX_UDP_STATUS=%d\n",
	       rReport->u4IpV4CksStatus, rReport->u4TcpCksStatus,
	       rReport->u4UdpCksStatus);
	DBGLOG_LIMITED(RX, LOUD,
		"RX_IPV4_TYPE=%d, RX_IPV6_TYPE=%d, RX_TCP_TYPE=%d, RX_UDP_TYPE=%d\n",
	  rReport->u4IpV4CksType, rReport->u4IpV6CksType,
	  rReport->u4TcpCksType, rReport->u4UdpCksType);

	if (prAdapter->u4CSUMFlags != CSUM_NOT_SUPPORTED) {
		if (u4TcpUdpIpCksStatus &
		    RX_CS_TYPE_IPv4) {	/* IPv4 packet */
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
			if (u4TcpUdpIpCksStatus &
			    RX_CS_STATUS_IP) {	/* IP packet csum failed */
				prSwRfb->aeCSUM[CSUM_TYPE_IPV4] =
					CSUM_RES_FAILED;
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_IPV4] =
					CSUM_RES_SUCCESS;
			}

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {
				/* TCP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus &
				    RX_CS_STATUS_TCP) {
				  /* TCP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] =
						CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] =
						CSUM_RES_SUCCESS;
				}
			} else if (u4TcpUdpIpCksStatus &
				   RX_CS_TYPE_UDP) {	/* UDP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus &
				    RX_CS_STATUS_UDP) {
				  /* UDP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] =
						CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] =
						CSUM_RES_SUCCESS;
				}
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
			}
		} else if (u4TcpUdpIpCksStatus &
			   RX_CS_TYPE_IPv6) {	/* IPv6 packet */
			prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_SUCCESS;

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {
				/* TCP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus &
				    RX_CS_STATUS_TCP) {
				  /* TCP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] =
						CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] =
						CSUM_RES_SUCCESS;
				}
			} else if (u4TcpUdpIpCksStatus &
				   RX_CS_TYPE_UDP) {	/* UDP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus &
				    RX_CS_STATUS_UDP) {
				  /* UDP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] =
						CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] =
						CSUM_RES_SUCCESS;
				}
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
			}
		} else {
			prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
		}
	}

}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*----------------------------------------------------------------------------*/
/*!
 * \brief nicRxClearFrag() is used to clean all fragments in the fragment cache.
 *
 * \param[in] prAdapter       pointer to the Adapter handler
 * \param[in] prStaRec        The fragment cache is stored under station record.
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxClearFrag(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec)
{
	int i, j;
	struct FRAG_INFO *prFragInfo;

	for (i = 0; i < TID_NUM; i++) {
		for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
			prFragInfo = &prStaRec->rFragInfo[i][j];

			if (prFragInfo->pr1stFrag) {
				nicRxReturnRFB(prAdapter,
					prFragInfo->pr1stFrag);
				prFragInfo->pr1stFrag = (struct SW_RFB *)NULL;
			}
		}
	}

	DBGLOG(RX, INFO, "%s\n", __func__);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief rxDefragMPDU() is used to defragment the incoming packets.
 *
 * \param[in] prSWRfb        The RFB which is being processed.
 * \param[in] UINT_16     u2FrameCtrl
 *
 * \retval NOT NULL  Receive the last fragment data
 * \retval NULL      Receive the fragment packet which is not the last
 */
/*----------------------------------------------------------------------------*/
struct SW_RFB *nicRxDefragMPDU(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSWRfb, OUT struct QUE *prReturnedQue)
{

	struct SW_RFB *prOutputSwRfb = (struct SW_RFB *) NULL;
	struct RX_CTRL *prRxCtrl;
	struct FRAG_INFO *prFragInfo;
	uint32_t i = 0, j;
	uint16_t u2SeqCtrl, u2FrameCtrl;
	uint16_t u2SeqNo;
	uint8_t ucFragNo;
	u_int8_t fgFirst = FALSE;
	u_int8_t fgLast = FALSE;
	OS_SYSTIME rCurrentTime;
	struct WLAN_MAC_HEADER *prWlanHeader = NULL;
	struct WLAN_MAC_HEADER_QOS *prWlanHeaderQos = NULL;
	struct WLAN_MAC_HEADER_A4_QOS *prWlanHeaderA4Qos = NULL;
	void *prRxStatus = NULL;
	struct HW_MAC_RX_STS_GROUP_4 *prRxStatusGroup4 = NULL;
	struct STA_RECORD *prStaRec;
	uint8_t ucTid = 0;
#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
	uint8_t ucSecMode = CIPHER_SUITE_NONE;
	uint64_t u8PN;
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */

	DEBUGFUNC("nicRx: rxmDefragMPDU\n");

	ASSERT(prSWRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	prStaRec = prSWRfb->prStaRec;

	prRxStatus = prSWRfb->prRxStatus;
	ASSERT(prRxStatus);

	if (prSWRfb->fgHdrTran == FALSE) {
		prWlanHeader = (struct WLAN_MAC_HEADER *)
					prSWRfb->pvHeader;
		prWlanHeaderQos = (struct WLAN_MAC_HEADER_QOS *)
					prSWRfb->pvHeader;
		prWlanHeaderA4Qos = (struct WLAN_MAC_HEADER_A4_QOS *)
					prSWRfb->pvHeader;
		u2FrameCtrl = prWlanHeader->u2FrameCtrl;
		if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl)) {
			if (RXM_IS_FROM_DS_TO_DS(u2FrameCtrl)) {
				ucTid = (prWlanHeaderA4Qos->
					u2QosCtrl & MASK_QC_TID);
			} else {
				ucTid = (prWlanHeaderQos->
					u2QosCtrl & MASK_QC_TID);
			}
		} else
			ucTid = TID_NUM;
	} else {
		prRxStatusGroup4 = prSWRfb->prRxStatusGroup4;
		prSWRfb->u2SequenceControl = HAL_RX_STATUS_GET_SEQFrag_NUM(
						     prRxStatusGroup4);
		u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(
				      prRxStatusGroup4);
		if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl))
			ucTid = prSWRfb->ucTid;
		else
			ucTid = TID_NUM;
	}
	u2SeqCtrl = prSWRfb->u2SequenceControl;
	u2SeqNo = u2SeqCtrl >> MASK_SC_SEQ_NUM_OFFSET;
	ucFragNo = (uint8_t) (u2SeqCtrl & MASK_SC_FRAG_NUM);
	prSWRfb->u2FrameCtrl = u2FrameCtrl;
	prSWRfb->ucTid = ucTid;

	if (!(u2FrameCtrl & MASK_FC_MORE_FRAG)) {
		/* The last fragment frame */
		if (ucFragNo) {
			DBGLOG(RX, LOUD,
			       "FC %04x M %04x SQ %04x\n", u2FrameCtrl,
			       (uint16_t) (u2FrameCtrl & MASK_FC_MORE_FRAG),
			       u2SeqCtrl);
			fgLast = TRUE;
		}
		/* Non-fragment frame */
		else
			return prSWRfb;
	}
	/* The fragment frame except the last one */
	else {
		if (ucFragNo == 0) {
			DBGLOG(RX, LOUD,
			       "FC %04x M %04x SQ %04x\n", u2FrameCtrl,
			       (uint16_t) (u2FrameCtrl & MASK_FC_MORE_FRAG),
			       u2SeqCtrl);
			fgFirst = TRUE;
		} else {
			DBGLOG(RX, LOUD,
			       "FC %04x M %04x SQ %04x\n", u2FrameCtrl,
			       (uint16_t) (u2FrameCtrl & MASK_FC_MORE_FRAG),
			       u2SeqCtrl);
		}
	}

	GET_CURRENT_SYSTIME(&rCurrentTime);

	/* check cipher suite to set if we need to get PN */
#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
	if (prSWRfb->ucSecMode == CIPHER_SUITE_TKIP
		|| prSWRfb->ucSecMode == CIPHER_SUITE_TKIP_WO_MIC
		|| prSWRfb->ucSecMode == CIPHER_SUITE_CCMP
		|| prSWRfb->ucSecMode == CIPHER_SUITE_CCMP_W_CCX
		|| prSWRfb->ucSecMode == CIPHER_SUITE_CCMP_256
		|| prSWRfb->ucSecMode == CIPHER_SUITE_GCMP_128
		|| prSWRfb->ucSecMode == CIPHER_SUITE_GCMP_256) {
		ucSecMode = prSWRfb->ucSecMode;
		if (!qmRxPNtoU64(prSWRfb->prRxStatusGroup1->aucPN,
			CCMPTSCPNNUM, &u8PN)) {
			DBGLOG(QM, ERROR, "PN2U64 failed\n");
			/* should not enter here, just fallback */
			ucSecMode = CIPHER_SUITE_NONE;
		}
	}
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */


	for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
		prFragInfo = &prStaRec->rFragInfo[prSWRfb->ucTid][j];
		if (prFragInfo->pr1stFrag) {
			/* I. If the receive timer for the MSDU or MMPDU that
			 * is stored in the fragments queue exceeds
			 * dot11MaxReceiveLifetime, we discard the uncompleted
			 * fragments.
			 * II. If we didn't receive the last MPDU for a period,
			 * we use this function for remove frames.
			 */
			if (CHECK_FOR_EXPIRATION(rCurrentTime,
				prFragInfo->rReceiveLifetimeLimit)) {
				prFragInfo->pr1stFrag->eDst =
					RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
					(struct QUE_ENTRY *)
					prFragInfo->pr1stFrag);

				prFragInfo->pr1stFrag = (struct SW_RFB *) NULL;
			}
		}
	}

	for (i = 0; i < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; i++) {
		prFragInfo = &prStaRec->rFragInfo[prSWRfb->ucTid][i];

		if (fgFirst) {	/* looking for timed-out frag buffer */

			if (prFragInfo->pr1stFrag == (struct SW_RFB *)
			    NULL)	/* find a free frag buffer */
				break;
		} else {
			/* looking for a buffer with desired next seqctrl */

			if (prFragInfo->pr1stFrag == (struct SW_RFB *) NULL)
				continue;

			if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl)) {
				if (RXM_IS_QOS_DATA_FRAME(
					prFragInfo->pr1stFrag->u2FrameCtrl)) {
					if (u2SeqNo == prFragInfo->u2SeqNo
#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
					    && ucSecMode
						== prFragInfo->ucSecMode
#else
					    && ucFragNo
						== prFragInfo->ucNextFragNo
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */
					   )
						break;
				}
			} else {
				if (!RXM_IS_QOS_DATA_FRAME(
					prFragInfo->pr1stFrag->u2FrameCtrl)) {
					if (u2SeqNo == prFragInfo->u2SeqNo
#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
					    && ucSecMode
						== prFragInfo->ucSecMode
#else
					    && ucFragNo
						== prFragInfo->ucNextFragNo
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */
					   )
						break;
				}
			}
		}
	}

	if (i >= MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS) {

		/* Can't find a proper struct FRAG_INFO.
		 * I. 1st Fragment MPDU, all of the FragInfo are exhausted
		 * II. 2nd ~ (n-1)th Fragment MPDU, can't find the right
		 * FragInfo for defragment.
		 * Because we won't process fragment frame outside this
		 * function, so we should free it right away.
		 */
		nicRxReturnRFB(prAdapter, prSWRfb);

		return (struct SW_RFB *) NULL;
	}

#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
	if (prFragInfo->pr1stFrag != (struct SW_RFB *) NULL) {
		/* check if the FragNo is cont. */
		if (ucFragNo != prFragInfo->ucNextFragNo
			|| ((ucSecMode != CIPHER_SUITE_NONE)
				&& (u8PN != prFragInfo->u8NextPN))
			) {
			DBGLOG(RX, INFO, "non-cont FragNo or PN, drop it.");

			DBGLOG(RX, INFO,
				"SN:%04x NxFragN:%02x FragN:%02x\n",
				prFragInfo->u2SeqNo,
				prFragInfo->ucNextFragNo,
				ucFragNo);

			if (ucSecMode != CIPHER_SUITE_NONE)
				DBGLOG(RX, INFO,
					"SN:%04x NxPN:%016x PN:%016x\n",
					prFragInfo->u2SeqNo,
					prFragInfo->u8NextPN,
					u8PN);

			/* discard fragments if FragNo is non-cont. */
			nicRxReturnRFB(prAdapter, prFragInfo->pr1stFrag);
			prFragInfo->pr1stFrag = (struct SW_RFB *) NULL;

			nicRxReturnRFB(prAdapter, prSWRfb);
			return (struct SW_RFB *) NULL;
		}
	}
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */

	ASSERT(prFragInfo);

	/* retrieve Rx payload */
	prSWRfb->pucPayload = (uint8_t *) ((
		(unsigned long) prSWRfb->pvHeader) +
		prSWRfb->u2HeaderLen);
	prSWRfb->u2PayloadLength =
		(uint16_t) (prSWRfb->u2RxByteCount - ((
		unsigned long) prSWRfb->pucPayload -
		(unsigned long) prRxStatus));

	if (fgFirst) {
		DBGLOG(RX, LOUD, "rxDefragMPDU first\n");

		SET_EXPIRATION_TIME(prFragInfo->rReceiveLifetimeLimit,
			TU_TO_SYSTIME(
			DOT11_RECEIVE_LIFETIME_TU_DEFAULT));

		prFragInfo->pr1stFrag = prSWRfb;

		prFragInfo->pucNextFragStart =
			(uint8_t *) prSWRfb->pucRecvBuff +
			prSWRfb->u2RxByteCount;

		prFragInfo->u2SeqNo = u2SeqNo;
		prFragInfo->ucNextFragNo = ucFragNo + 1; /* should be 1 */

#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
		prFragInfo->ucSecMode = ucSecMode;
		if (prFragInfo->ucSecMode != CIPHER_SUITE_NONE)
			prFragInfo->u8NextPN = u8PN + 1;
		else
			prFragInfo->u8NextPN = 0;
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */

		DBGLOG(RX, LOUD,
			"First: SeqCtrl:%04x, SN:%04x, NxFragN = %02x\n",
			u2SeqCtrl, prFragInfo->u2SeqNo,
			prFragInfo->ucNextFragNo);

		/* prSWRfb->fgFragmented = TRUE; */
		/* whsu: todo for checksum */
	} else {
		prFragInfo->pr1stFrag->u2RxByteCount +=
			prSWRfb->u2PayloadLength;

		if (prFragInfo->pr1stFrag->u2RxByteCount >
		    CFG_RX_MAX_PKT_SIZE) {

			prFragInfo->pr1stFrag->eDst = RX_PKT_DESTINATION_NULL;
			QUEUE_INSERT_TAIL(prReturnedQue,
				(struct QUE_ENTRY *)
				prFragInfo->pr1stFrag);

			prFragInfo->pr1stFrag = (struct SW_RFB *) NULL;

			nicRxReturnRFB(prAdapter, prSWRfb);
			DBGLOG(RX, LOUD,
				"Defrag: dropped due length > CFG_RX_MAX_PKT_SIZE\n");
		} else {
			kalMemCopy(prFragInfo->pucNextFragStart,
				prSWRfb->pucPayload,
				prSWRfb->u2PayloadLength);
			/* [6630] update rx byte count and packet length */
			prFragInfo->pr1stFrag->u2PacketLen +=
				prSWRfb->u2PayloadLength;
			prFragInfo->pr1stFrag->u2PayloadLength +=
				prSWRfb->u2PayloadLength;

			if (fgLast) {	/* The last one, free the buffer */
				DBGLOG(RX, LOUD, "Defrag: finished\n");

				prOutputSwRfb = prFragInfo->pr1stFrag;

				prFragInfo->pr1stFrag = (struct SW_RFB *) NULL;
			} else {
				DBGLOG(RX, LOUD, "Defrag: mid fraged\n");

				prFragInfo->pucNextFragStart +=
					prSWRfb->u2PayloadLength;

				prFragInfo->ucNextFragNo++;

#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
				if (prFragInfo->ucSecMode
					!= CIPHER_SUITE_NONE)
					prFragInfo->u8NextPN++;
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */

			}

			nicRxReturnRFB(prAdapter, prSWRfb);
		}
	}

	/* DBGLOG_MEM8(RXM, INFO, */
	/* prFragInfo->pr1stFrag->pucPayload, */
	/* prFragInfo->pr1stFrag->u2PayloadLength); */

	return prOutputSwRfb;
}				/* end of rxmDefragMPDU() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Do duplicate detection
 *
 * @param prSwRfb Pointer to the RX packet
 *
 * @return TRUE: a duplicate, FALSE: not a duplicate
 */
/*----------------------------------------------------------------------------*/
u_int8_t nicRxIsDuplicateFrame(IN OUT struct SW_RFB
			       *prSwRfb)
{

	/* Non-QoS Unicast Data or Unicast MMPDU: SC Cache #4;
	 *   QoS Unicast Data: SC Cache #0~3;
	 *   Broadcast/Multicast: RetryBit == 0
	 */
	uint32_t u4SeqCtrlCacheIdx;
	uint16_t u2SequenceControl, u2FrameCtrl;
	u_int8_t fgIsDuplicate = FALSE, fgIsAmsduSubframe = FALSE;
	struct WLAN_MAC_HEADER *prWlanHeader = NULL;
	void *prRxStatus = NULL;
	struct HW_MAC_RX_STS_GROUP_4 *prRxStatusGroup4 = NULL;

	DEBUGFUNC("nicRx: Enter rxmIsDuplicateFrame()\n");

	ASSERT(prSwRfb);

	/* Situations in which the STC_REC is missing include:
	 *   (1) Probe Request (2) (Re)Association Request
	 *   (3) IBSS data frames (4) Probe Response
	 */
	if (!prSwRfb->prStaRec)
		return FALSE;

	prRxStatus = prSwRfb->prRxStatus;
	ASSERT(prRxStatus);

	fgIsAmsduSubframe = prSwRfb->ucPayloadFormat;
	if (prSwRfb->fgHdrTran == FALSE) {
		prWlanHeader = (struct WLAN_MAC_HEADER *) prSwRfb->pvHeader;
		u2SequenceControl = prSwRfb->u2SequenceControl;
		u2FrameCtrl = prWlanHeader->u2FrameCtrl;
	} else {
		prRxStatusGroup4 = prSwRfb->prRxStatusGroup4;
		u2SequenceControl = HAL_RX_STATUS_GET_SEQFrag_NUM(
					    prRxStatusGroup4);
		u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(
				      prRxStatusGroup4);
	}
	prSwRfb->u2SequenceControl = u2SequenceControl;

	/* Case 1: Unicast QoS data */
	if (RXM_IS_QOS_DATA_FRAME(
		    u2FrameCtrl)) {
		/* WLAN header shall exist when doing duplicate detection */
		if (prSwRfb->ucTid < CFG_RX_MAX_BA_TID_NUM &&
			prSwRfb->prStaRec->
			aprRxReorderParamRefTbl[prSwRfb->ucTid]) {

			/* QoS data with an RX BA agreement
			 *  Case 1: The packet is not an AMPDU subframe,
			 *          so the RetryBit may be set to 1 (TBC).
			 *  Case 2: The RX BA agreement was just established.
			 *          Some enqueued packets may not be sent with
			 *          aggregation.
			 */

			DBGLOG(RX, LOUD, "RX: SC=0x%X (BA Entry present)\n",
			       u2SequenceControl);

			/* Update the SN cache in order to ensure the
			 * correctness of duplicate removal in case the
			 * BA agreement is deleted
			 */
			prSwRfb->prStaRec->au2CachedSeqCtrl[prSwRfb->ucTid] =
				u2SequenceControl;

			/* debug */
#if 0
			DBGLOG(RXM, LOUD,
			       "RXM: SC= 0x%X (Cache[%d] updated) with BA\n",
			       u2SequenceControl, prSwRfb->ucTID);

			if (g_prMqm->arRxBaTable[
				prSwRfb->prStaRec->aucRxBaTable[prSwRfb->ucTID]]
				.ucStatus == BA_ENTRY_STATUS_DELETING) {
				DBGLOG(RXM, LOUD,
					"RXM: SC= 0x%X (Cache[%d] updated) with DELETING BA\n",
				  u2SequenceControl, prSwRfb->ucTID);
			}
#endif

			/* HW scoreboard shall take care Case 1.
			 * Let the layer layer handle Case 2.
			 */
			return FALSE;	/* Not a duplicate */
		}

		if (prSwRfb->prStaRec->ucDesiredPhyTypeSet &
		    (PHY_TYPE_BIT_HT | PHY_TYPE_BIT_VHT)) {
			u4SeqCtrlCacheIdx = prSwRfb->ucTid;
#if (CFG_SUPPORT_802_11AX == 1)
			} else if (prSwRfb->prStaRec->ucDesiredPhyTypeSet &
				   PHY_TYPE_BIT_HE) {
				u4SeqCtrlCacheIdx = prSwRfb->ucTid;
#endif
#if (CFG_SUPPORT_802_11BE == 1)
			} else if (prSwRfb->prStaRec->ucDesiredPhyTypeSet &
				   PHY_TYPE_BIT_EHT) {
				u4SeqCtrlCacheIdx = prSwRfb->ucTid;
#endif
		} else {
			if (prSwRfb->ucTid < 8) {	/* UP = 0~7 */
				u4SeqCtrlCacheIdx = aucTid2ACI[prSwRfb->ucTid];
			} else {
				DBGLOG(RX, WARN,
				       "RXM: (Warning) Unknown QoS Data with TID=%d\n",
				       prSwRfb->ucTid);
				/* Ignore duplicate frame check */
				return FALSE;
			}
		}
	}
	/* Case 2: Unicast non-QoS data or MMPDUs */
	else
		u4SeqCtrlCacheIdx = TID_NUM;

	/* If this is a retransmission */
	if (u2FrameCtrl & MASK_FC_RETRY) {
		if (u2SequenceControl !=
		    prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx]) {
			prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx] =
				u2SequenceControl;
			if (fgIsAmsduSubframe ==
					RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU)
				prSwRfb->prStaRec->
					afgIsIgnoreAmsduDuplicate[
					u4SeqCtrlCacheIdx] = TRUE;
			DBGLOG(RX, LOUD, "RXM: SC= 0x%x (Cache[%u] updated)\n",
			       u2SequenceControl, u4SeqCtrlCacheIdx);
		} else {
			/* A duplicate. */
			if (prSwRfb->prStaRec->
				afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx]) {
				if (fgIsAmsduSubframe ==
					RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU)
					prSwRfb->prStaRec->
					afgIsIgnoreAmsduDuplicate[
					u4SeqCtrlCacheIdx] = FALSE;
			} else {
				fgIsDuplicate = TRUE;
				DBGLOG(RX, LOUD,
					"RXM: SC= 0x%x (Cache[%u] duplicate)\n",
				  u2SequenceControl, u4SeqCtrlCacheIdx);
			}
		}
	}

	/* Not a retransmission */
	else {

		prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx] =
			u2SequenceControl;
		prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx]
			= FALSE;

		DBGLOG(RX, LOUD, "RXM: SC= 0x%x (Cache[%u] updated)\n",
		       u2SequenceControl, u4SeqCtrlCacheIdx);
	}

	return fgIsDuplicate;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process packet doesn't need to do buffer reordering
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessPktWithoutReorder(IN struct ADAPTER
				   *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	struct TX_CTRL *prTxCtrl;
	u_int8_t fgIsRetained = FALSE;
	uint32_t u4CurrentRxBufferCount;
	/* P_STA_RECORD_T prStaRec = (P_STA_RECORD_T)NULL; */

	DEBUGFUNC("nicRxProcessPktWithoutReorder");
	/* DBGLOG(RX, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	u4CurrentRxBufferCount = prRxCtrl->rFreeSwRfbList.u4NumElem;
	/* QM USED = $A, AVAILABLE COUNT = $B, INDICATED TO OS = $C
	 * TOTAL = $A + $B + $C
	 *
	 * Case #1 (Retain)
	 * -------------------------------------------------------
	 * $A + $B < THRESHOLD := $A + $B + $C < THRESHOLD + $C
	 * := $TOTAL - THRESHOLD < $C
	 * => $C used too much, retain
	 *
	 * Case #2 (Non-Retain)
	 * -------------------------------------------------------
	 * $A + $B > THRESHOLD := $A + $B + $C > THRESHOLD + $C
	 * := $TOTAL - THRESHOLD > $C
	 * => still available for $C to use
	 *
	 */

#if defined(LINUX)
	fgIsRetained = FALSE;
#else
	fgIsRetained = (((u4CurrentRxBufferCount +
			  qmGetRxReorderQueuedBufferCount(prAdapter) +
			  prTxCtrl->i4PendingFwdFrameCount) <
			 CFG_RX_RETAINED_PKT_THRESHOLD) ? TRUE : FALSE);
#endif

	/* DBGLOG(RX, INFO, ("fgIsRetained = %d\n", fgIsRetained)); */
#if CFG_ENABLE_PER_STA_STATISTICS
	if (prSwRfb->prStaRec
	    && (prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable >
		0))
		prSwRfb->prStaRec->u4TotalRxPktsNumber++;
#endif
	if (kalProcessRxPacket(prAdapter->prGlueInfo,
			       prSwRfb->pvPacket,
			       prSwRfb->pvHeader,
			       (uint32_t) prSwRfb->u2PacketLen, fgIsRetained,
			       prSwRfb->aeCSUM) != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, ERROR,
		       "kalProcessRxPacket return value != WLAN_STATUS_SUCCESS\n");

		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}

#if CFG_SUPPORT_MULTITHREAD
	if (HAL_IS_RX_DIRECT(prAdapter)) {
		kalRxIndicateOnePkt(prAdapter->prGlueInfo,
			(void *) GLUE_GET_PKT_DESCRIPTOR(
				GLUE_GET_PKT_QUEUE_ENTRY(prSwRfb->pvPacket)));
		if (fgIsRetained)
			RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT, 1);
	} else {
		KAL_SPIN_LOCK_DECLARATION();

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
		QUEUE_INSERT_TAIL(&(prAdapter->rRxQueue),
				  (struct QUE_ENTRY *) GLUE_GET_PKT_QUEUE_ENTRY(
					  prSwRfb->pvPacket));
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

		prRxCtrl->ucNumIndPacket++;
		kalSetTxEvent2Rx(prAdapter->prGlueInfo);
	}
#else
	prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] =
		prSwRfb->pvPacket;
	prRxCtrl->ucNumIndPacket++;
#endif

#ifndef LINUX
	if (fgIsRetained) {
		prRxCtrl->apvRetainedPacket[prRxCtrl->ucNumRetainedPacket] =
			prSwRfb->pvPacket;
		prRxCtrl->ucNumRetainedPacket++;
	} else
#endif
		prSwRfb->pvPacket = NULL;


	/* Return RFB */
	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN,
		       "Cannot allocate packet buffer for SwRfb!\n");
		if (!timerPendingTimer(
			    &prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN,
				"Start ReturnIndicatedRfb Timer (%u)\n",
			  RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
			cnmTimerStartTimer(prAdapter,
				&prAdapter->rPacketDelaySetupTimer,
				SEC_TO_MSEC(
					RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
		}
	}
	nicRxReturnRFB(prAdapter, prSwRfb);
}

u_int8_t nicRxCheckForwardPktResource(
	IN struct ADAPTER *prAdapter, uint32_t ucTid)
{
	struct TX_CTRL *prTxCtrl;
	uint8_t i, uTxQidx;

	prTxCtrl = &prAdapter->rTxCtrl;
	uTxQidx = aucACI2TxQIdx[aucTid2ACI[ucTid]];

	/* If the resource used more than half, we could control WMM resource
	 * by limit every AC queue.
	 */
	for (i = uTxQidx+1; i < WMM_AC_INDEX_NUM; i++) {
		if (GLUE_GET_REF_CNT(prTxCtrl
			->i4PendingFwdFrameWMMCount[uTxQidx]) >=
			GLUE_GET_REF_CNT(prTxCtrl
			->i4PendingFwdFrameWMMCount[i]) &&
			GLUE_GET_REF_CNT(prTxCtrl
			->i4PendingFwdFrameWMMCount[i]) > 0 &&
			GLUE_GET_REF_CNT(prTxCtrl
			->i4PendingFwdFrameCount) > prAdapter
			->rQM.u4MaxForwardBufferCount)
			return FALSE;
	}
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process forwarding data packet
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessForwardPkt(IN struct ADAPTER *prAdapter,
			    IN struct SW_RFB *prSwRfb)
{
	struct MSDU_INFO *prMsduInfo, *prRetMsduInfoList;
	struct TX_CTRL *prTxCtrl;
	struct RX_CTRL *prRxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessForwardPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	if (prSwRfb->ucTid >= TX_DESC_TID_NUM) {
		DBGLOG_LIMITED(RX, WARN,
		       "Wrong forward packet: tid:%d\n", prSwRfb->ucTid);
		prSwRfb->ucTid = 0;
	}

	if (!nicRxCheckForwardPktResource(prAdapter, prSwRfb->ucTid)) {
		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}

	DBGLOG_LIMITED(RX, TRACE, "to forward packet: %d,%d,%d,%d,%d\n",
		GLUE_GET_REF_CNT(prTxCtrl->i4PendingFwdFrameWMMCount[0]),
		GLUE_GET_REF_CNT(prTxCtrl->i4PendingFwdFrameWMMCount[1]),
		GLUE_GET_REF_CNT(prTxCtrl->i4PendingFwdFrameWMMCount[2]),
		GLUE_GET_REF_CNT(prTxCtrl->i4PendingFwdFrameWMMCount[3]),
		GLUE_GET_REF_CNT(prTxCtrl->i4PendingFwdFrameCount));

	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (prMsduInfo &&
	    kalProcessRxPacket(prAdapter->prGlueInfo,
			       prSwRfb->pvPacket,
			       prSwRfb->pvHeader,
			       (uint32_t) prSwRfb->u2PacketLen,
			       prRxCtrl->rFreeSwRfbList.u4NumElem <
			       CFG_RX_RETAINED_PKT_THRESHOLD ? TRUE : FALSE,
			       prSwRfb->aeCSUM) == WLAN_STATUS_SUCCESS) {

		/* parsing forward frame */
		wlanProcessTxFrame(prAdapter, (void *) (prSwRfb->pvPacket));
		/* pack into MSDU_INFO_T */
		nicTxFillMsduInfo(prAdapter, prMsduInfo,
				  (void *) (prSwRfb->pvPacket));

		prMsduInfo->eSrc = TX_PACKET_FORWARDING;
		prMsduInfo->ucBssIndex = secGetBssIdxByWlanIdx(prAdapter,
					 prSwRfb->ucWlanIdx);
		prMsduInfo->ucUserPriority = prSwRfb->ucTid;

		/* release RX buffer (to rIndicatedRfbList) */
		prSwRfb->pvPacket = NULL;
		nicRxReturnRFB(prAdapter, prSwRfb);

		/* Handle if prMsduInfo out of bss index range*/
		if (prMsduInfo->ucBssIndex > MAX_BSSID_NUM) {
			DBGLOG(QM, INFO,
			    "Invalid bssidx:%u\n", prMsduInfo->ucBssIndex);
			if (prMsduInfo->pfTxDoneHandler != NULL)
				prMsduInfo->pfTxDoneHandler(prAdapter,
						prMsduInfo,
						TX_RESULT_DROPPED_IN_DRIVER);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
			return;
		}

		/* increase forward frame counter */
		GLUE_INC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);

		/* add resource control for WMM forward packet */
		GLUE_INC_REF_CNT(prTxCtrl
			->i4PendingFwdFrameWMMCount[
			aucACI2TxQIdx[aucTid2ACI[prSwRfb->ucTid]]]);

		/* send into TX queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
		prRetMsduInfoList = qmEnqueueTxPackets(prAdapter,
						       prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

		if (prRetMsduInfoList !=
		    NULL) {	/* TX queue refuses queuing the packet */
			nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfoList);
			nicTxReturnMsduInfo(prAdapter, prRetMsduInfoList);
		}
		/* indicate service thread for sending */
		if (prTxCtrl->i4PendingFwdFrameCount > 0)
			kalSetEvent(prAdapter->prGlueInfo);
	} else {		/* no TX resource */
		DBGLOG(QM, INFO, "No Tx MSDU_INFO for forwarding frames\n");
		nicRxReturnRFB(prAdapter, prSwRfb);
		if (prMsduInfo)
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process broadcast data packet for both host and forwarding
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessGOBroadcastPkt(IN struct ADAPTER
				*prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct SW_RFB *prSwRfbDuplicated;
	struct TX_CTRL *prTxCtrl;
	struct RX_CTRL *prRxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessGOBroadcastPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	ASSERT(CFG_NUM_OF_QM_RX_PKT_NUM >= 16);

	if (prRxCtrl->rFreeSwRfbList.u4NumElem
	    >= (CFG_RX_MAX_PKT_NUM - (CFG_NUM_OF_QM_RX_PKT_NUM -
				      16 /* Reserved for others */))) {

		/* 1. Duplicate SW_RFB_T */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList,
				  prSwRfbDuplicated, struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		if (prSwRfbDuplicated) {
			kalMemCopy(prSwRfbDuplicated->pucRecvBuff,
				   prSwRfb->pucRecvBuff,
				   ALIGN_4(prSwRfb->u2RxByteCount +
					   HIF_RX_HW_APPENDED_LEN));

			prSwRfbDuplicated->ucPacketType = RX_PKT_TYPE_RX_DATA;
			prSwRfbDuplicated->ucStaRecIdx = prSwRfb->ucStaRecIdx;
			nicRxFillRFB(prAdapter, prSwRfbDuplicated);
			GLUE_COPY_PRIV_DATA(
				prSwRfbDuplicated->pvPacket,
				prSwRfb->pvPacket);

			/* 2. Modify eDst */
			prSwRfbDuplicated->eDst = RX_PKT_DESTINATION_FORWARD;

			/* 4. Forward */
			nicRxProcessForwardPkt(prAdapter, prSwRfbDuplicated);
		}
	} else {
		DBGLOG(RX, WARN,
		       "Stop to forward BMC packet due to less free Sw Rfb %u\n",
		       prRxCtrl->rFreeSwRfbList.u4NumElem);
	}

	/* 3. Indicate to host */
	prSwRfb->eDst = RX_PKT_DESTINATION_HOST;
	nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);

}

#if CFG_SUPPORT_SNIFFER
void nicRxFillRadiotapMCS(IN OUT struct MONITOR_RADIOTAP
			  *prMonitorRadiotap,
			  IN struct HW_MAC_RX_STS_GROUP_3 *prRxStatusGroup3)
{
	uint8_t ucFrMode;
	uint8_t ucShortGI;
	uint8_t ucRxMode;
	uint8_t ucLDPC;
	uint8_t ucSTBC;
	uint8_t ucNess;

	ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] &
		     RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET);
	/* VHTA1 B0-B1 */
	ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] &
		     RX_VT_SHORT_GI) ? 1 : 0;	/* HT_shortgi */
	ucRxMode = (((prRxStatusGroup3)->u4RxVector[0] &
		     RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET);
	ucLDPC = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_LDPC) ?
		 1 : 0;	/* HT_adcode */
	ucSTBC = (((prRxStatusGroup3)->u4RxVector[0] &
		   RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET);	/* HT_stbc */
	ucNess = (((prRxStatusGroup3)->u4RxVector[0] &
		   RX_VT_NESS_MASK) >> RX_VT_NESS_OFFSET);	/* HT_extltf */

	prMonitorRadiotap->ucMcsKnown =
		(BITS(0, 6) | (((ucNess & BIT(1)) >> 1) << 7));

	prMonitorRadiotap->ucMcsFlags = ((ucFrMode) |
		(ucShortGI << 2) |
		((ucRxMode & BIT(0)) << 3) |
		(ucLDPC << 4) | (ucSTBC << 5) |
		((ucNess & BIT(0)) << 7));
	/* Bit[6:0] for 802.11n, mcs0 ~ mcs7 */
	prMonitorRadiotap->ucMcsMcs = ((
		prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_MASK);
}

void nicRxFillRadiotapVHT(IN OUT struct MONITOR_RADIOTAP
			  *prMonitorRadiotap,
			  IN struct HW_MAC_RX_STS_GROUP_3 *prRxStatusGroup3)
{
	uint8_t ucSTBC;
	uint8_t ucTxopPsNotAllow;
	uint8_t ucShortGI;
	uint8_t ucNsym;
	uint8_t ucLdpcExtraOfdmSym;
	uint8_t ucBeamFormed;
	uint8_t ucFrMode;
	uint8_t ucNsts;
	uint8_t ucMcs;

	prMonitorRadiotap->u2VhtKnown = RADIOTAP_VHT_ALL_KNOWN;
	prMonitorRadiotap->u2VhtKnown &=
		~RADIOTAP_VHT_SHORT_GI_NSYM_KNOWN;

	ucSTBC = (((prRxStatusGroup3)->u4RxVector[0] &
		   RX_VT_STBC_MASK) >>
		  RX_VT_STBC_OFFSET);	/* BIT[7]: VHTA1 B3 */
	ucTxopPsNotAllow = ((prRxStatusGroup3)->u4RxVector[0] &
			    RX_VT_TXOP_PS_NOT_ALLOWED) ? 1 : 0;	/* VHTA1 B22 */
	/*
	 * ucNsym = ((prRxStatusGroup3)->u4RxVector[0]
	 * & RX_VT_SHORT_GI_NSYM) ? 1 : 0;	//VHTA2 B1
	 */
	ucNsym = 0; /* Invalid in MT6632*/
	ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] &
		     RX_VT_SHORT_GI) ? 1 : 0;	/* VHTA2 B0 */
	ucLdpcExtraOfdmSym = ((prRxStatusGroup3)->u4RxVector[0] &
			      RX_VT_LDPC_EXTRA_OFDM_SYM) ? 1 : 0; /* VHTA2 B3 */
	ucBeamFormed = ((prRxStatusGroup3)->u4RxVector[0] &
			RX_VT_BEAMFORMED) ? 1 : 0;	/* VHTA2 B8 */
	prMonitorRadiotap->ucVhtFlags = ((ucSTBC) |
		(ucTxopPsNotAllow << 1) |
		(ucShortGI << 2) | (ucNsym << 3)
		| (ucLdpcExtraOfdmSym << 4)
		| (ucBeamFormed << 5));

	ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] &
		     RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET);
	/* VHTA1 B0-B1 */
	switch (ucFrMode) {
	case RX_VT_FR_MODE_20:
		prMonitorRadiotap->ucVhtBandwidth = 0;
		break;
	case RX_VT_FR_MODE_40:
		prMonitorRadiotap->ucVhtBandwidth = 1;
		break;
	case RX_VT_FR_MODE_80:
		prMonitorRadiotap->ucVhtBandwidth = 4;
		break;
	case RX_VT_FR_MODE_160:
		prMonitorRadiotap->ucVhtBandwidth = 11;
		break;
	default:
		prMonitorRadiotap->ucVhtBandwidth = 0;
	}

	/* Set to 0~7 for 1~8 space time streams */
	ucNsts = (((prRxStatusGroup3)->u4RxVector[1] &
		   RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET) + 1;
	/* VHTA1 B10-B12 */

	/* Bit[3:0] for 802.11ac, mcs0 ~ mcs9 */
	ucMcs = ((prRxStatusGroup3)->u4RxVector[0] &
		 RX_VT_RX_RATE_AC_MASK);

	prMonitorRadiotap->aucVhtMcsNss[0] = ((ucMcs << 4) |
		(ucNsts - ucSTBC));	/* STBC = Nsts - Nss */

	/*
	 * prMonitorRadiotap->ucVhtCoding =
	 *		(((prRxStatusGroup3)->u4RxVector[0]
	 * & RX_VT_CODING_MASK) >> RX_VT_CODING_OFFSET);
	 */
	prMonitorRadiotap->ucVhtCoding = 0; /* Invalid in MT6632*/

	/* VHTA2 B2-B3 */

	prMonitorRadiotap->ucVhtGroupId =
		(((((prRxStatusGroup3)->u4RxVector[1] &
		    RX_VT_GROUPID_1_MASK) >> RX_VT_GROUPID_1_OFFSET) << 2) |
		 (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_GROUPID_0_MASK)
		  >> RX_VT_GROUPID_0_OFFSET));
	/* VHTA1 B4-B9 */
	/* VHTA1 B13-B21 */
	prMonitorRadiotap->u2VhtPartialAid = ((((
			prRxStatusGroup3)->u4RxVector[2]
			& RX_VT_AID_1_MASK) << 4) |
	    (((prRxStatusGroup3)->u4RxVector[1]
				& RX_VT_AID_0_MASK) >>
				RX_VT_AID_0_OFFSET));

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process HIF monitor packet
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessMonitorPacket(IN struct ADAPTER *prAdapter,
			       IN OUT struct SW_RFB *prSwRfb)
{
	struct sk_buff *prSkb = NULL;
	struct RX_CTRL *prRxCtrl;
	void *prRxStatus;
	struct HW_MAC_RX_STS_GROUP_2 *prRxStatusGroup2;
	struct HW_MAC_RX_STS_GROUP_3 *prRxStatusGroup3;
	struct MONITOR_RADIOTAP rMonitorRadiotap;
	struct RADIOTAP_FIELD_VENDOR_ rRadiotapFieldVendor;
	uint8_t *prVendorNsOffset;
	uint32_t u4VendorNsLen;
	uint32_t u4RadiotapLen;
	uint32_t u4ItPresent;
	uint8_t aucMtkOui[] = VENDOR_OUI_MTK;
	uint8_t ucRxRate;
	uint8_t ucRxMode;
	uint8_t ucChanNum = 0;
	uint8_t ucMcs;
	uint8_t ucFrMode;
	uint8_t ucShortGI;
	uint32_t u4PhyRate;
	struct RX_DESC_OPS_T *prRxDescOps;
	enum ENUM_BAND eBand = 0;

#if CFG_SUPPORT_MULTITHREAD
	KAL_SPIN_LOCK_DECLARATION();
#endif

	DEBUGFUNC("nicRxProcessMonitorPacket");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRxDescOps = prAdapter->chip_info->prRxDescOps;

	prRxCtrl = &prAdapter->rRxCtrl;

	nicRxFillRFB(prAdapter, prSwRfb);

	/* can't parse radiotap info if no rx vector */
	if (((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_2)) == 0)
	    || ((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) == 0)) {
		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}

	prRxStatus = prSwRfb->prRxStatus;
	prRxStatusGroup2 = prSwRfb->prRxStatusGroup2;
	prRxStatusGroup3 = prSwRfb->prRxStatusGroup3;

	/* Bit Number 30 Vendor Namespace */
	u4VendorNsLen = sizeof(struct RADIOTAP_FIELD_VENDOR_);
	rRadiotapFieldVendor.aucOUI[0] = aucMtkOui[0];
	rRadiotapFieldVendor.aucOUI[1] = aucMtkOui[1];
	rRadiotapFieldVendor.aucOUI[2] = aucMtkOui[2];
	rRadiotapFieldVendor.ucSubNamespace = 0;
	rRadiotapFieldVendor.u2DataLen = u4VendorNsLen - 6;
	/* VHTA1 B0-B1 */
	rRadiotapFieldVendor.ucData = (((
		prRxStatusGroup3)->u4RxVector[0]
			& RX_VT_FR_MODE_MASK) >>
			RX_VT_FR_MODE_OFFSET);

	ucRxMode = (((prRxStatusGroup3)->u4RxVector[0] &
		     RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET);

	if (ucRxMode == RX_VT_VHT_MODE) {
		u4RadiotapLen = RADIOTAP_LEN_VHT;
		u4ItPresent = RADIOTAP_FIELDS_VHT;
	} else if ((ucRxMode == RX_VT_MIXED_MODE)
		   || (ucRxMode == RX_VT_GREEN_MODE)) {
		u4RadiotapLen = RADIOTAP_LEN_HT;
		u4ItPresent = RADIOTAP_FIELDS_HT;
	} else {
		u4RadiotapLen = RADIOTAP_LEN_LEGACY;
		u4ItPresent = RADIOTAP_FIELDS_LEGACY;
	}

	/* Radiotap Header & Bit Number 30 Vendor Namespace */
	prVendorNsOffset = (uint8_t *) &rMonitorRadiotap +
			   u4RadiotapLen;
	u4RadiotapLen += u4VendorNsLen;
	kalMemSet(&rMonitorRadiotap, 0,
		  sizeof(struct MONITOR_RADIOTAP));
	kalMemCopy(prVendorNsOffset,
		   (uint8_t *) &rRadiotapFieldVendor, u4VendorNsLen);
	rMonitorRadiotap.u2ItLen = cpu_to_le16(u4RadiotapLen);
	rMonitorRadiotap.u4ItPresent = u4ItPresent;

	/* Bit Number 0 TSFT */
	rMonitorRadiotap.u8MacTime =
		(prRxStatusGroup2->u4Timestamp);

	/* Bit Number 1 FLAGS */
	if (prSwRfb->fgIsFrag)
		rMonitorRadiotap.ucFlags |= BIT(3);

	if (prSwRfb->fgIsFCS)
		rMonitorRadiotap.ucFlags |= BIT(6);

	/* Bit Number 2 RATE */
	if ((ucRxMode == RX_VT_LEGACY_CCK)
	    || (ucRxMode == RX_VT_LEGACY_OFDM)) {
		/* Bit[2:0] for Legacy CCK, Bit[3:0] for Legacy OFDM */
		ucRxRate = ((prRxStatusGroup3)->u4RxVector[0] & BITS(0, 3));
		rMonitorRadiotap.ucRate = nicGetHwRateByPhyRate(ucRxRate);
	} else {
		ucMcs = ((prRxStatusGroup3)->u4RxVector[0] &
			 RX_VT_RX_RATE_AC_MASK);
		/* VHTA1 B0-B1 */
		ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] &
			     RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET);
		ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] &
			     RX_VT_SHORT_GI) ? 1 : 0;	/* VHTA2 B0 */

		/* ucRate(500kbs) = u4PhyRate(100kbps) / 5, max ucRate = 0xFF */
		u4PhyRate = nicGetPhyRateByMcsRate(ucMcs, ucFrMode,
						   ucShortGI);
		if (u4PhyRate > 1275)
			rMonitorRadiotap.ucRate = 0xFF;
		else
			rMonitorRadiotap.ucRate = u4PhyRate / 5;
	}

	/* Bit Number 3 CHANNEL */
	if (ucRxMode == RX_VT_LEGACY_CCK)
		rMonitorRadiotap.u2ChFlags |= BIT(5);
	else			/* OFDM */
		rMonitorRadiotap.u2ChFlags |= BIT(6);

	RX_STATUS_GET(prRxDescOps, ucChanNum, get_ch_num, prRxStatus);
	RX_STATUS_GET(prRxDescOps, eBand, get_rf_band, prRxStatus);

	nicRxdChNumTranslate(eBand, &ucChanNum);

	if (eBand == BAND_2G4) {
		rMonitorRadiotap.u2ChFlags |= BIT(7);
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 2407);
	} else if (eBand == BAND_5G) {		/* BAND_5G */
		rMonitorRadiotap.u2ChFlags |= BIT(8);
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 5000);
	}
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (eBand == BAND_6G)		/* BAND_6G */
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 5950);
#endif


	/* Bit Number 5 ANT SIGNAL */
	rMonitorRadiotap.ucAntennaSignal =
		RCPI_TO_dBm(HAL_RX_STATUS_GET_RCPI0(prRxStatusGroup3));

	/* Bit Number 6 ANT NOISE */
	rMonitorRadiotap.ucAntennaNoise = ((((
			prRxStatusGroup3)->u4RxVector[5]
				& RX_VT_NF0_MASK) >> 1) +
				128);

	/* Bit Number 11 ANT, Invalid for MT6632 and MT7615 */
	rMonitorRadiotap.ucAntenna = ((
		prRxStatusGroup3)->u4RxVector[2] & RX_VT_SEL_ANT) ? 1 : 0;

	/* Bit Number 19 MCS */
	if ((u4ItPresent & RADIOTAP_FIELD_MCS))
		nicRxFillRadiotapMCS(&rMonitorRadiotap, prRxStatusGroup3);

	/* Bit Number 20 AMPDU */
	if (prSwRfb->fgIsAmpdu) {
		if (prSwRfb->ucRxvSeqNo)
			++prRxCtrl->u4AmpduRefNum;
		rMonitorRadiotap.u4AmpduRefNum = prRxCtrl->u4AmpduRefNum;
	}

	/* Bit Number 21 VHT */
	if ((u4ItPresent & RADIOTAP_FIELD_VHT))
		nicRxFillRadiotapVHT(&rMonitorRadiotap, prRxStatusGroup3);

	prSwRfb->pvHeader -= u4RadiotapLen;
	kalMemCopy(prSwRfb->pvHeader, &rMonitorRadiotap,
		   u4RadiotapLen);

	prSkb = (struct sk_buff *)(prSwRfb->pvPacket);
	prSkb->data = (unsigned char *)(prSwRfb->pvHeader);

	skb_reset_tail_pointer(prSkb);
	skb_trim(prSkb, 0);
	skb_put(prSkb, (u4RadiotapLen + prSwRfb->u2PacketLen));

#if CFG_SUPPORT_MULTITHREAD
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	QUEUE_INSERT_TAIL(&(prAdapter->rRxQueue),
			  (struct QUE_ENTRY *) GLUE_GET_PKT_QUEUE_ENTRY(
				  prSwRfb->pvPacket));
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

	prRxCtrl->ucNumIndPacket++;
	kalSetTxEvent2Rx(prAdapter->prGlueInfo);
#else
	prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] =
		prSwRfb->pvPacket;
	prRxCtrl->ucNumIndPacket++;
#endif

	prSwRfb->pvPacket = NULL;
	/* Return RFB */
	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN,
		       "Cannot allocate packet buffer for SwRfb!\n");
		if (!timerPendingTimer(
			    &prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN,
				"Start ReturnIndicatedRfb Timer (%u)\n",
			  RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
			cnmTimerStartTimer(prAdapter,
				&prAdapter->rPacketDelaySetupTimer,
				SEC_TO_MSEC(
					RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
		}
	}
	nicRxReturnRFB(prAdapter, prSwRfb);
}
#else
/*----------------------------------------------------------------------------*/
/*!
 * @brief Process HIF monitor packet
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessMonitorPacket(IN struct ADAPTER *prAdapter,
			       IN OUT struct SW_RFB *prSwRfb)
{
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process & Parsing RXV for traffic indicator
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
#if CFG_SUPPORT_PERF_IND
void nicRxPerfIndProcessRXV(IN struct ADAPTER *prAdapter,
			       IN struct SW_RFB *prSwRfb,
			       IN uint8_t ucBssIndex)
{
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prAdapter->chip_info;
	if (!prChipInfo || !prChipInfo->asicRxPerfIndProcessRXV)
		return;

	prChipInfo->asicRxPerfIndProcessRXV(prAdapter, prSwRfb, ucBssIndex);
	/* else { */
		/* print too much, remove for system perfomance */
		/* DBGLOG(RX, ERROR, "%s: no asicRxPerfIndProcessRXV ??\n", */
		/* __func__); */
	/* } */
}
#endif

static void nicRxSendDeauthPacket(IN struct ADAPTER *prAdapter,
		IN uint16_t u2FrameCtrl,
		IN uint8_t *pucSrcAddr,
		IN uint8_t *pucDestAddr,
		IN uint8_t *pucBssid)
{
	struct SW_RFB rSwRfb;
	struct WLAN_MAC_HEADER rWlanHeader;
	uint32_t u4Status;

	if (!prAdapter || !pucSrcAddr || !pucDestAddr || !pucBssid)
		return;

	kalMemZero(&rSwRfb, sizeof(rSwRfb));
	kalMemZero(&rWlanHeader, sizeof(rWlanHeader));

	rWlanHeader.u2FrameCtrl = u2FrameCtrl;
	COPY_MAC_ADDR(rWlanHeader.aucAddr1, pucSrcAddr);
	COPY_MAC_ADDR(rWlanHeader.aucAddr2, pucDestAddr);
	COPY_MAC_ADDR(rWlanHeader.aucAddr3, pucBssid);
	rSwRfb.pvHeader = &rWlanHeader;

	u4Status = authSendDeauthFrame(prAdapter,
		NULL,
		NULL,
		&rSwRfb,
		REASON_CODE_CLASS_3_ERR,
		NULL);
	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(NIC, WARN, "u4Status: %d\n", u4Status);
}

static void nicRxProcessDropPacket(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb)
{
	struct WLAN_MAC_HEADER *prWlanHeader = NULL;
	uint8_t ucBssIndex = 0;
	uint16_t u2FrameCtrl;

	if (!prAdapter || !prSwRfb)
		return;

	prWlanHeader = (struct WLAN_MAC_HEADER *) prSwRfb->pvHeader;

	if (!prWlanHeader)
		return;

	u2FrameCtrl = prWlanHeader->u2FrameCtrl;
	DBGLOG(RX, TEMP,
		"TA: " MACSTR " RA: " MACSTR " bssid: " MACSTR " fc: 0x%x\n",
		MAC2STR(prWlanHeader->aucAddr2),
		MAC2STR(prWlanHeader->aucAddr1),
		MAC2STR(prWlanHeader->aucAddr3),
		u2FrameCtrl);

	if ((u2FrameCtrl & (MASK_FC_FROM_DS | MASK_FC_TO_DS)) == 0)
		return;

	for (ucBssIndex = 0; ucBssIndex < prAdapter->ucHwBssIdNum;
			ucBssIndex++) {
		struct BSS_INFO *prBssInfo;
		u_int8_t fgSendDeauth = FALSE;

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (!prBssInfo)
			continue;
		if (IS_BSS_NOT_ALIVE(prAdapter, prBssInfo))
			continue;
		switch (prBssInfo->eNetworkType) {
		case NETWORK_TYPE_P2P:
			if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT &&
				EQUAL_MAC_ADDR(prWlanHeader->aucAddr3,
					prBssInfo->aucOwnMacAddr))
				fgSendDeauth = TRUE;
			break;
		default:
			break;
		}

		if (fgSendDeauth)
			nicRxSendDeauthPacket(prAdapter,
				u2FrameCtrl,
				prWlanHeader->aucAddr1,
				prWlanHeader->aucAddr2,
				prWlanHeader->aucAddr3);
	}
}
/* fos_change begin */
#if CFG_SUPPORT_STAT_STATISTICS
void nicRxGetNoiseLevelAndLastRate(IN struct ADAPTER *prAdapter,
			       IN struct SW_RFB *prSwRfb)
{
	struct STA_RECORD *prStaRec;
	uint8_t noise_level = 0;
	uint8_t ucRxRate;
	uint8_t ucRxMode;
	uint8_t ucMcs;
	uint8_t ucFrMode;
	uint8_t ucShortGI;

	if (prAdapter == NULL || prSwRfb == NULL)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (prStaRec == NULL)
		return;

	noise_level = ((prSwRfb->prRxStatusGroup3->u4RxVector[5] &
		RX_VT_NF0_MASK) >> 1);

	if (noise_level == 0) {
		DBGLOG(RX, TRACE, "Invalid noise level\n");
	} else if (prStaRec->ucNoise_avg) {
		prStaRec->ucNoise_avg = (((prStaRec->ucNoise_avg << 3) -
			  prStaRec->ucNoise_avg) >> 3) + (noise_level >> 3);
	} else {
		prStaRec->ucNoise_avg = noise_level;
	}

	DBGLOG(RX, TRACE, "Noise_level avg:%d latest:%d\n",
		prStaRec->ucNoise_avg, noise_level);

	wlanGetRxRateByBssid(prAdapter->prGlueInfo,
				GLUE_GET_PKT_BSS_IDX(prRetSwRfb->pvPacket),
				&prStaRec->u4LastPhyRate, NULL, NULL);
}
#endif /* fos_change end */

void nicRxProcessPacketToHost(struct ADAPTER *prAdapter,
	struct SW_RFB *prRetSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	struct STA_RECORD *prStaRec;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;

	prRxCtrl = &prAdapter->rRxCtrl;
	prStaRec = cnmGetStaRecByIndex(prAdapter,
			prRetSwRfb->ucStaRecIdx);
	if (!prStaRec)
		return;

	/* store it in local variable to prevent timing issue */
	ucBssIndex = prStaRec->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo)
		return;

#if ARP_MONITER_ENABLE
	if (IS_STA_IN_AIS(prStaRec))
		qmHandleRxArpPackets(prAdapter, prRetSwRfb);

	/* STA or GC */
	qmHandleRxDhcpPackets(prAdapter, prRetSwRfb);
#endif

	if (ucBssIndex < MAX_BSSID_NUM)
		GET_BOOT_SYSTIME(&prRxCtrl->u4LastRxTime[ucBssIndex]);

	secCheckRxEapolPacketEncryption(prAdapter, prRetSwRfb, prStaRec);
}

void nicRxIndicatePackets(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfbListHead)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prRetSwRfb, *prNextSwRfb;

	prRxCtrl = &prAdapter->rRxCtrl;
	prRetSwRfb = prSwRfbListHead;

	while (prRetSwRfb) {
		/**
		 * Collect RXV information,
		 * prAdapter->arStaRec[i].u4RxV[*] updated.
		 * wlanGetRxRate() can get new rate values
		 */
		nicRxProcessRxv(prAdapter, prRetSwRfb);

/* fos_change begin */
#if CFG_SUPPORT_STAT_STATISTICS
		nicRxGetNoiseLevelAndLastRate(prAdapter, prRetSwRfb);
#endif /* fos_change end */

#if CFG_SUPPORT_PERF_IND
		nicRxPerfIndProcessRXV(prAdapter, prRetSwRfb,
			GLUE_GET_PKT_BSS_IDX(prRetSwRfb->pvPacket));
#endif

		/* save next first */
		prNextSwRfb = (struct SW_RFB *)
			QUEUE_GET_NEXT_ENTRY(
				(struct QUE_ENTRY *)
				prRetSwRfb);

		switch (prRetSwRfb->eDst) {
		case RX_PKT_DESTINATION_HOST:
			nicRxProcessPacketToHost(prAdapter, prRetSwRfb);
#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_SUPPORT_CONNAC2X == 1)
			/* Not handle non-CONNAC2X case */
			if (RXV_AUTODVT_DNABLED(prAdapter) &&
				(prRetSwRfb->ucGroupVLD &
				BIT(RX_GROUP_VLD_3)) &&
				(prRetSwRfb->ucGroupVLD &
				BIT(RX_GROUP_VLD_5))) {
				connac2x_rxv_correct_test(
					prAdapter, prRetSwRfb);
			}
#endif
#endif /* CFG_SUPPORT_WIFI_SYSDVT */
			nicRxProcessPktWithoutReorder(prAdapter, prRetSwRfb);
			break;

		case RX_PKT_DESTINATION_FORWARD:
			nicRxProcessForwardPkt(
				prAdapter, prRetSwRfb);
			break;

		case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
			nicRxProcessGOBroadcastPkt(prAdapter,
				prRetSwRfb);
			break;

		case RX_PKT_DESTINATION_NULL:
			nicRxReturnRFB(prAdapter, prRetSwRfb);
			RX_INC_CNT(prRxCtrl,
				RX_DST_NULL_DROP_COUNT);
			RX_INC_CNT(prRxCtrl,
				RX_DROP_TOTAL_COUNT);
			break;

		default:
			break;
		}
#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4DequeuedCnt++;
#endif
		prRetSwRfb = prNextSwRfb;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process HIF data packet
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessDataPacket(IN struct ADAPTER *prAdapter,
			    IN OUT struct SW_RFB *prSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prRetSwRfb;
	struct HW_MAC_RX_DESC *prRxStatus;

	u_int8_t fgDrop;
	uint8_t ucBssIndex = 0;
	struct mt66xx_chip_info *prChipInfo;
	struct RX_DESC_OPS_T *prRxDescOps;

	DEBUGFUNC("nicRxProcessDataPacket");
	/* DBGLOG(INIT, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	nicRxFillRFB(prAdapter, prSwRfb);

	fgDrop = FALSE;

	prRxCtrl = &prAdapter->rRxCtrl;
	prChipInfo = prAdapter->chip_info;
	prRxDescOps = prChipInfo->prRxDescOps;
	prRxStatus = prSwRfb->prRxStatus;

	/* Check AMPDU_nERR_Bitmap */
	prSwRfb->fgDataFrame = TRUE;
	prSwRfb->fgFragFrame = FALSE;
	prSwRfb->fgReorderBuffer = FALSE;

#if CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION
	prSwRfb->fgIsFirstSubAMSDULLCMS = FALSE;
#endif /* CFG_SUPPORT_FRAG_AGG_ATTACK_DETECTION */

#if CFG_WIFI_SW_CIPHER_MISMATCH
	if (prSwRfb->prStaRec &&
	    prSwRfb->prStaRec->fgTransmitKeyExist &&
	    prSwRfb->prStaRec->ucStaState == STA_STATE_3 &&
	    prSwRfb->fgIsBC == FALSE &&
	    prSwRfb->fgIsMC == FALSE &&
	    !prSwRfb->fgIsCipherMS) {
		uint16_t u2FrameCtrl = 0;

		if (prSwRfb->fgHdrTran == FALSE) {
			u2FrameCtrl = ((struct WLAN_MAC_HEADER *)
				prSwRfb->pvHeader)->u2FrameCtrl;
			prSwRfb->fgIsCipherMS =
				!RXM_IS_PROTECTED_FRAME(u2FrameCtrl);
		} else if (prSwRfb->prRxStatusGroup4) {
			u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(
					      prSwRfb->prRxStatusGroup4);
			prSwRfb->fgIsCipherMS =
				!RXM_IS_PROTECTED_FRAME(u2FrameCtrl);
		}
	}
#endif

	if (prRxDescOps->nic_rxd_sanity_check)
		fgDrop = prRxDescOps->nic_rxd_sanity_check(
			prAdapter, prSwRfb);
	else {
		DBGLOG(RX, ERROR,
			"%s:: no nic_rxd_sanity_check??\n", __func__);
		fgDrop = TRUE;
	}

	if (fgDrop && prRxStatus->ucWlanIdx >= WTBL_SIZE &&
			HAL_RX_STATUS_IS_LLC_MIS(prRxStatus))
		nicRxProcessDropPacket(prAdapter, prSwRfb);

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	if (prAdapter->fgIsSupportCsumOffload && fgDrop == FALSE)
		nicRxFillChksumStatus(prAdapter, prSwRfb);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	/* if(secCheckClassError(prAdapter, prSwRfb, prStaRec) == TRUE && */
	if (prAdapter->fgTestMode == FALSE && fgDrop == FALSE) {
#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4QueuedCnt++;
#endif
		ucBssIndex = secGetBssIdxByWlanIdx(prAdapter,
						   prSwRfb->ucWlanIdx);
		GLUE_SET_PKT_BSS_IDX(prSwRfb->pvPacket, ucBssIndex);
		STATS_RX_PKT_INFO_DISPLAY(prSwRfb, prAdapter, ucBssIndex);

#if CFG_FAST_PATH_SUPPORT
		if (
#if CFG_SUPPORT_LOWLATENCY_MODE
		    prAdapter->fgEnLowLatencyMode &&
#endif
			prAdapter->rWifiVar.ucSupportProtocol != 0)
			mscsHandleRxPacket(prAdapter, prSwRfb);
#endif

		if (IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
			qmCheckRxEAPOLM3(prAdapter, prSwRfb, ucBssIndex);
		}

#if ((CFG_SUPPORT_802_11AX == 1) && (CFG_SUPPORT_WIFI_SYSDVT == 1))
		if (fgEfuseCtrlAxOn == 1) {
		if (prAdapter->fgEnShowHETrigger) {
			uint16_t u2TxFrameCtrl;

			u2TxFrameCtrl = (*(uint8_t *) (prSwRfb->pvHeader) &
				 MASK_FRAME_TYPE);
			if (RXM_IS_TRIGGER_FRAME(u2TxFrameCtrl)) {
				DBGLOG(NIC, STATE,
					"\n%s: HE Trigger --------------\n",
					__func__);
				dumpMemory8((uint8_t *)prSwRfb->prRxStatus,
					prSwRfb->u2RxByteCount);
				DBGLOG(NIC, STATE,
					"%s: HE Trigger end --------------\n",
					__func__);
				nicRxReturnRFB(prAdapter, prSwRfb);
				return;
			}
		}
		}
#endif /* CFG_SUPPORT_802_11AX == 1 */

		prRetSwRfb = qmHandleRxPackets(prAdapter, prSwRfb);
		if (prRetSwRfb != NULL)
			nicRxIndicatePackets(prAdapter, prRetSwRfb);
	} else {
		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(prRxCtrl, RX_CLASS_ERR_DROP_COUNT);
		RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
	}
}

#if 1
void nicRxProcessEventPacket(IN struct ADAPTER *prAdapter,
			     IN OUT struct SW_RFB *prSwRfb)
{
	struct mt66xx_chip_info *prChipInfo;
	struct CMD_INFO *prCmdInfo;
	struct WIFI_EVENT *prEvent;
	uint32_t u4Idx, u4Size;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prChipInfo = prAdapter->chip_info;
	prEvent = (struct WIFI_EVENT *)
			(prSwRfb->pucRecvBuff + prChipInfo->rxd_size);
	if (prEvent->ucEID != EVENT_ID_DEBUG_MSG
	    && prEvent->ucEID != EVENT_ID_ASSERT_DUMP) {
		DBGLOG(NIC, TRACE,
			"RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
			prEvent->ucEID, prEvent->ucSeqNum,
			prEvent->u2PacketLength);
	}
#if (CFG_SUPPORT_STATISTICS == 1)
	wlanWakeLogEvent(prEvent->ucEID);
#endif
	/* Event handler table */
	u4Size = sizeof(arEventTable) / sizeof(struct
					       RX_EVENT_HANDLER);

	for (u4Idx = 0; u4Idx < u4Size; u4Idx++) {
		if (prEvent->ucEID == arEventTable[u4Idx].eEID) {
			arEventTable[u4Idx].pfnHandler(prAdapter, prEvent);

			break;
		}
	}

	/* Event cannot be found in event handler table, use default action */
	if (u4Idx >= u4Size) {
		DBGLOG(RX, INFO, "Not static config event: id=0x%02X, seq=%u",
				prEvent->ucEID, prEvent->ucSeqNum);
		prCmdInfo = nicGetPendingCmdInfo(prAdapter,
						 prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (unlikely(prEvent->ucEID ==
					EVENT_ID_INIT_EVENT_CMD_RESULT) &&
					prCmdInfo->fgIsOid) {
				/*
				 * This event ID will be returned if CMD is not
				 * handled by FW. Here skip invoking
				 * pfCmdDoneHandler since the call back cannot
				 * distinguish the calling conditions.
				 * The callback accessing the event buffer is
				 * dangerous.
				 */
				DBGLOG(RX, INFO, "FW not support cmd 0x%02X",
						prCmdInfo->ucCID);
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo,
						0, WLAN_STATUS_FAILURE);
			} else {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(
						prAdapter, prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(
						prAdapter->prGlueInfo,
						prCmdInfo,
						0,
						WLAN_STATUS_SUCCESS);
			}

			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else {
			DBGLOG_LIMITED(RX, INFO,
				"UNHANDLED RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
			  prEvent->ucEID, prEvent->ucSeqNum,
			  prEvent->u2PacketLength);
		}
	}

	/* Reset Chip NoAck flag */
	if (prAdapter->fgIsChipNoAck) {
		DBGLOG_LIMITED(RX, WARN,
		       "Got response from chip, clear NoAck flag!\n");
		WARN_ON(TRUE);
	}
	prAdapter->ucOidTimeoutCount = 0;
	prAdapter->fgIsChipNoAck = FALSE;

	nicRxReturnRFB(prAdapter, prSwRfb);
}
#else
u_int8_t fgKeepPrintCoreDump = FALSE;
/*----------------------------------------------------------------------------*/
/*!
 * @brief Process HIF event packet
 *
 * @param prAdapter pointer to the Adapter handler
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessEventPacket(IN struct ADAPTER *prAdapter,
			     IN OUT struct SW_RFB *prSwRfb)
{
	struct CMD_INFO *prCmdInfo;
	/* P_MSDU_INFO_T prMsduInfo; */
	struct WIFI_EVENT *prEvent;
	struct GLUE_INFO *prGlueInfo;
	u_int8_t fgIsNewVersion;
	/*#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)*/
	uint32_t u4QueryInfoLen;
	/*#endif*/
	/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
	struct EVENT_ACCESS_EFUSE *prEventEfuseAccess;
	struct EXT_EVENT_EFUSE_FREE_BLOCK *prEventGetFreeBlock;
	/*#endif*/
	DEBUGFUNC("nicRxProcessEventPacket");
	/* DBGLOG(INIT, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	nicRxFillRFB(prAdapter, prSwRfb);
	if (prSwRfb->prRxStatusGroup3 == NULL) {
		DBGLOG(RX, WARN,
		       "rxStatusGroup3 for MGMT frame is NULL, drop this packet, dump RXD and Packet\n");
		DBGLOG_MEM8(RX, WARN, (uint8_t *) prSwRfb->prRxStatus,
			    sizeof(*prSwRfb->prRxStatus));
		if (prSwRfb->pvHeader)
			DBGLOG_MEM8(RX, WARN, (uint8_t *) prSwRfb->pvHeader,
				prSwRfb->u2PacketLen > 32 ?
					32 : prSwRfb->u2PacketLen);
		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DROP_TOTAL_COUNT);
#if 0
#if CFG_CHIP_RESET_SUPPORT
		glSetRstReason(RST_GROUP3_NULL);
		glResetTrigger(prAdapter);
#endif
#endif
		) {
			DBGLOG(INIT, INFO,
				"RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
				prEvent->ucEID, prEvent->ucSeqNum,
				prEvent->u2PacketLength);
		}

		/* Event Handling */
		switch (prEvent->ucEID) {
#if 0				/* It is removed now */
		case EVENT_ID_CMD_RESULT:
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
				prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				struct EVENT_CMD_RESULT *prCmdResult;

				prCmdResult = (struct EVENT_CMD_RESULT *)
					((uint8_t *) prEvent + EVENT_HDR_SIZE);

				/* CMD_RESULT should be only in response to
				 * Set commands
				 */
				ASSERT(prCmdInfo->fgSetQuery == FALSE
				       || prCmdInfo->fgNeedResp == TRUE);

				if (prCmdResult->ucStatus == 0) {
					/* success */
					if (prCmdInfo->pfCmdDoneHandler) {
						prCmdInfo->pfCmdDoneHandler(
							prAdapter, prCmdInfo,
							prEvent->aucBuffer);
					} else if (prCmdInfo->fgIsOid == TRUE) {
						kalOidComplete(
							prAdapter->prGlueInfo,
							prCmdInfo,
							0,
							WLAN_STATUS_SUCCESS);
					}
				} else if (prCmdResult->ucStatus == 1) {
					/* reject */
					if (prCmdInfo->fgIsOid == TRUE)
						kalOidComplete(
							prAdapter->prGlueInfo,
							prCmdInfo,
							0,
							WLAN_STATUS_FAILURE);
				} else if (prCmdResult->ucStatus == 2) {
					/* unknown CMD */
					if (prCmdInfo->fgIsOid == TRUE)
						kalOidComplete(
							prAdapter->prGlueInfo,
							prCmdInfo,
							0,
						WLAN_STATUS_NOT_SUPPORTED
							);
				}
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

			break;
#endif

#if 0
		case EVENT_ID_CONNECTION_STATUS:
			/* OBSELETE */
		{
			struct EVENT_CONNECTION_STATUS *prConnectionStatus;

			prConnectionStatus =
				(struct EVENT_CONNECTION_STATUS *)
					(prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_CONNECTION_STATUS = %d\n",
				 prConnectionStatus->ucMediaStatus);
			if (prConnectionStatus->ucMediaStatus ==
			    MEDIA_STATE_DISCONNECTED) {
				/* disconnected */
				if (kalGetMediaStateIndicated(prGlueInfo) !=
				    MEDIA_STATE_DISCONNECTED) {

					kalIndicateStatusAndComplete(prGlueInfo,
						WLAN_STATUS_MEDIA_DISCONNECT,
						NULL, 0);

					prAdapter->rWlanInfo.u4SysTime =
						kalGetTimeTick();
				}
			} else if (prConnectionStatus->ucMediaStatus ==
				   MEDIA_STATE_CONNECTED) {
				/* connected */
				prAdapter->rWlanInfo.u4SysTime =
					kalGetTimeTick();

				/* fill information for association result */
				prAdapter->rWlanInfo.rCurrBssId.rSsid.
				u4SsidLen = prConnectionStatus->ucSsidLen;
				kalMemCopy(
					prAdapter->rWlanInfo.rCurrBssId.
					rSsid.aucSsid,
					prConnectionStatus->aucSsid,
					prConnectionStatus->ucSsidLen);

				kalMemCopy(
					prAdapter->rWlanInfo.rCurrBssId.
					arMacAddress,
					prConnectionStatus->aucBssid,
					MAC_ADDR_LEN);
				/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.u4Privacy =
					prConnectionStatus->ucEncryptStatus;
				/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.rRssi = 0;
				/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.
				eNetworkTypeInUse = PARAM_NETWORK_TYPE_AUTOMODE;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.
				u4BeaconPeriod
					= prConnectionStatus->u2BeaconPeriod;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.
				u4ATIMWindow
					= prConnectionStatus->u2ATIMWindow;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.
				u4DSConfig
					= prConnectionStatus->u4FreqInKHz;
				prAdapter->rWlanInfo.ucNetworkType =
					prConnectionStatus->ucNetworkType;

				switch (prConnectionStatus->ucInfraMode) {
				case 0:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode
						= NET_TYPE_IBSS;
					break;
				case 1:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode
						= NET_TYPE_INFRA;
					break;
				case 2:
				default:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode
						 = NET_TYPE_AUTO_SWITCH;
					break;
				}
				/* always indicate to OS according to MSDN
				 * (re-association/roaming)
				 */
				kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_CONNECT, NULL, 0);
			}
		}
		break;

		case EVENT_ID_SCAN_RESULT:
			/* OBSELETE */
			break;
#endif

		case EVENT_ID_RX_ADDBA:
			/* The FW indicates that an RX BA agreement
			 * will be established
			 */
			qmHandleEventRxAddBa(prAdapter, prEvent);
			break;

		case EVENT_ID_RX_DELBA:
			/* The FW indicates that an RX BA agreement
			 * has been deleted
			 */
			qmHandleEventRxDelBa(prAdapter, prEvent);
			break;

		case EVENT_ID_CHECK_REORDER_BUBBLE:
			qmHandleEventCheckReorderBubble(prAdapter, prEvent);
			break;

		case EVENT_ID_LINK_QUALITY:
#if CFG_ENABLE_WIFI_DIRECT && CFG_SUPPORT_P2P_RSSI_QUERY
			if (prEvent->u2PacketLen == EVENT_HDR_SIZE + sizeof(
				    struct EVENT_LINK_QUALITY_EX)) {
				struct EVENT_LINK_QUALITY_EX *prLqEx =
					(struct EVENT_LINK_QUALITY_EX *)
						(prEvent->aucBuffer);

				if (prLqEx->ucIsLQ0Rdy)
					nicUpdateLinkQuality(prAdapter, 0,
						(struct EVENT_LINK_QUALITY *)
							prLqEx);
				if (prLqEx->ucIsLQ1Rdy)
					nicUpdateLinkQuality(prAdapter, 1,
						(struct EVENT_LINK_QUALITY *)
							prLqEx);
			} else {
				/* For old FW, P2P may invoke link quality
				 * query, and make driver flag becone TRUE.
				 */
				DBGLOG(P2P, WARN,
				       "Old FW version, not support P2P RSSI query.\n");

				/* Must not use NETWORK_TYPE_P2P_INDEX,
				 * cause the structure is mismatch.
				 */
				nicUpdateLinkQuality(prAdapter, 0,
					(struct EVENT_LINK_QUALITY *)
						(prEvent->aucBuffer));
			}
#else
			/*only support ais query */
			{
				uint8_t ucBssIndex;
				struct BSS_INFO *prBssInfo;

				for (ucBssIndex = 0;
					ucBssIndex < prAdapter->ucHwBssIdNum;
				  ucBssIndex++) {
					prBssInfo =
						prAdapter->aprBssInfo[
						ucBssIndex];

					if ((prBssInfo->eNetworkType ==
						NETWORK_TYPE_AIS)
					    && (prBssInfo->fgIsInUse))
						break;
				}

				/* No hit(bss1 for default ais network) */
				if (ucBssIndex >= prAdapter->ucHwBssIdNum)
					ucBssIndex = 1;

				nicUpdateLinkQuality(prAdapter, ucBssIndex,
					(struct EVENT_LINK_QUALITY *)
						(prEvent->aucBuffer));
			}

#endif

			/* command response handling */
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(
						prAdapter, prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(
						prAdapter->prGlueInfo,
						prCmdInfo,
						0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
#ifndef LINUX
			if (prAdapter->rWlanInfo.eRssiTriggerType ==
			    ENUM_RSSI_TRIGGER_GREATER &&
			    prAdapter->rWlanInfo.rRssiTriggerValue
						>= (int32_t) (
				    prAdapter->rLinkQuality.cRssi)) {
				prAdapter->rWlanInfo.eRssiTriggerType =
					ENUM_RSSI_TRIGGER_TRIGGERED;

				kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
					(void *)
						&(prAdapter->rWlanInfo.
						rRssiTriggerValue),
					sizeof(int32_t));
			} else if (prAdapter->rWlanInfo.eRssiTriggerType ==
				   ENUM_RSSI_TRIGGER_LESS
				   && prAdapter->rWlanInfo.rRssiTriggerValue
						<= (int32_t) (
					   prAdapter->rLinkQuality.cRssi)) {
				prAdapter->rWlanInfo.eRssiTriggerType =
					ENUM_RSSI_TRIGGER_TRIGGERED;

				kalIndicateStatusAndComplete(prGlueInfo,
					WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
					(void *)
					&(prAdapter->rWlanInfo.
					rRssiTriggerValue),
					sizeof(int32_t));
			}
#endif

			break;

		/*#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)*/
		case EVENT_ID_LAYER_0_EXT_MAGIC_NUM:
			if ((prEvent->ucExtenEID) == EXT_EVENT_ID_CMD_RESULT) {

				u4QueryInfoLen =
					sizeof(
					struct PARAM_CUSTOM_EFUSE_BUFFER_MODE);

				prCmdInfo = nicGetPendingCmdInfo(prAdapter,
					prEvent->ucSeqNum);

				if (prCmdInfo != NULL) {
					if ((prCmdInfo->fgIsOid) != 0) {
						kalOidComplete(
							prAdapter->prGlueInfo,
							prCmdInfo,
							u4QueryInfoLen,
							WLAN_STATUS_SUCCESS);
						/* return prCmdInfo */
						cmdBufFreeCmdInfo(
							prAdapter, prCmdInfo);
					}
				}
			}
			/*#if  (CFG_EEPROM_PAGE_ACCESS == 1)*/

			else if ((prEvent->ucExtenEID) ==
				 EXT_EVENT_ID_CMD_EFUSE_ACCESS) {
				u4QueryInfoLen =
					sizeof(
					struct PARAM_CUSTOM_ACCESS_EFUSE);
				prCmdInfo = nicGetPendingCmdInfo(prAdapter,
					prEvent->ucSeqNum);
				prEventEfuseAccess =
					(struct EVENT_ACCESS_EFUSE *)
					(prEvent->aucBuffer);

				/* Efuse block size 16 */
				kalMemCopy(prAdapter->aucEepromVaule,
					   prEventEfuseAccess->aucData, 16);

				if (prCmdInfo != NULL) {
					if ((prCmdInfo->fgIsOid) != 0) {
						kalOidComplete(
							prAdapter->prGlueInfo,
							prCmdInfo,
							u4QueryInfoLen,
							WLAN_STATUS_SUCCESS);
						/* return prCmdInfo */
						cmdBufFreeCmdInfo(
							prAdapter, prCmdInfo);

					}
				}
			}

			else if ((prEvent->ucExtenEID) ==
				 EXT_EVENT_ID_EFUSE_FREE_BLOCK) {
				u4QueryInfoLen = sizeof(struct
					PARAM_CUSTOM_EFUSE_FREE_BLOCK);
				prCmdInfo = nicGetPendingCmdInfo(prAdapter,
					prEvent->ucSeqNum);
				prEventGetFreeBlock =
					(struct EXT_EVENT_EFUSE_FREE_BLOCK *)
					(prEvent->aucBuffer);
				prAdapter->u4FreeBlockNum =
					prEventGetFreeBlock->u2FreeBlockNum;

				if (prCmdInfo != NULL) {
					if ((prCmdInfo->fgIsOid) != 0) {
						kalOidComplete(
							prAdapter->prGlueInfo,
							prCmdInfo,
							u4QueryInfoLen,
							WLAN_STATUS_SUCCESS);
						/* return prCmdInfo */
						cmdBufFreeCmdInfo(prAdapter,
							prCmdInfo);
					}
				}
			}
			/*#endif*/
			break;
		/*#endif*/

		case EVENT_ID_MIC_ERR_INFO: {
			struct EVENT_MIC_ERR_INFO *prMicError;
			/* P_PARAM_AUTH_EVENT_T prAuthEvent; */
			struct STA_RECORD *prStaRec;

			DBGLOG(RSN, EVENT, "EVENT_ID_MIC_ERR_INFO\n");

			prMicError = (struct EVENT_MIC_ERR_INFO *) (
					     prEvent->aucBuffer);
			prStaRec = cnmGetStaRecByAddress(prAdapter,
				prAdapter->prAisBssInfo->ucBssIndex,
				prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
			ASSERT(prStaRec);

			if (prStaRec)
				rsnTkipHandleMICFailure(prAdapter, prStaRec,
					(u_int8_t) prMicError->u4Flags);
			else
				DBGLOG(RSN, INFO, "No STA rec!!\n");
#if 0
			prAuthEvent = (struct PARAM_AUTH_EVENT *)
				      prAdapter->aucIndicationEventBuffer;

			/* Status type: Authentication Event */
			prAuthEvent->rStatus.eStatusType =
				ENUM_STATUS_TYPE_AUTHENTICATION;

			/* Authentication request */
			prAuthEvent->arRequest[0].u4Length = sizeof(
					struct PARAM_AUTH_REQUEST);
			kalMemCopy((void *) prAuthEvent->arRequest[0].arBssid,
				(void *)
				prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
				PARAM_MAC_ADDR_LEN);

			if (prMicError->u4Flags != 0)
				prAuthEvent->arRequest[0].u4Flags =
					PARAM_AUTH_REQUEST_GROUP_ERROR;
			else
				prAuthEvent->arRequest[0].u4Flags =
					PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				(void *) prAuthEvent,
				sizeof(struct PARAM_STATUS_INDICATION) + sizeof(
				struct PARAM_AUTH_REQUEST));
#endif
		}
		break;

#if 0				/* Marked for MT6630 */
		case EVENT_ID_ASSOC_INFO: {
			struct EVENT_ASSOC_INFO *prAssocInfo;

			prAssocInfo = (struct EVENT_ASSOC_INFO *) (
					      prEvent->aucBuffer);

			kalHandleAssocInfo(prAdapter->prGlueInfo, prAssocInfo);
		}
		break;

		case EVENT_ID_802_11_PMKID: {
			struct PARAM_AUTH_EVENT *prAuthEvent;
			uint8_t *cp;
			uint32_t u4LenOfUsedBuffer;

			prAuthEvent = (struct PARAM_AUTH_EVENT *)
				      prAdapter->aucIndicationEventBuffer;

			prAuthEvent->rStatus.eStatusType =
				ENUM_STATUS_TYPE_CANDIDATE_LIST;

			u4LenOfUsedBuffer =
				(uint32_t) (prEvent->u2PacketLength - 8);

			prAuthEvent->arRequest[0].u4Length = u4LenOfUsedBuffer;

			cp = (uint8_t *) &prAuthEvent->arRequest[0];

			/* Status type: PMKID Candidatelist Event */
			kalMemCopy(cp, (struct EVENT_PMKID_CANDIDATE_LIST *) (
					   prEvent->aucBuffer),
				   prEvent->u2PacketLength - 8);

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				(void *) prAuthEvent,
				sizeof(struct PARAM_STATUS_INDICATION) +
				u4LenOfUsedBuffer);
		}
		break;
#endif
		case EVENT_ID_SCAN_DONE:
			fgIsNewVersion = TRUE;
			scnEventScanDone(prAdapter,
				(struct EVENT_SCAN_DONE *) (prEvent->aucBuffer),
				fgIsNewVersion);
			break;

		case EVENT_ID_NLO_DONE:
			DBGLOG(INIT, INFO, "EVENT_ID_NLO_DONE\n");
			scnEventNloDone(prAdapter,
				(struct EVENT_NLO_DONE *) (prEvent->aucBuffer));
#if CFG_SUPPORT_PNO
			prAdapter->prAisBssInfo->fgIsPNOEnable = FALSE;
			if (prAdapter->prAisBssInfo->fgIsNetRequestInActive
			    && prAdapter->prAisBssInfo->fgIsPNOEnable) {
				UNSET_NET_ACTIVE(prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex);
				DBGLOG(INIT, INFO,
				       "INACTIVE  AIS from  ACTIVEto disable PNO\n");
				/* sync with firmware */
				nicDeactivateNetwork(prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex);
			}
#endif
			break;

		case EVENT_ID_TX_DONE:
#if 1
			nicTxProcessTxDoneEvent(prAdapter, prEvent);
#else
			{
				struct EVENT_TX_DONE *prTxDone;

				prTxDone =
					(struct EVENT_TX_DONE *)
					(prEvent->aucBuffer);

				DBGLOG(INIT, INFO,
					"EVENT_ID_TX_DONE WIDX:PID[%u:%u] Status[%u] SN[%u]\n",
				  prTxDone->ucWlanIndex,
				  prTxDone->ucPacketSeq,
				  prTxDone->ucStatus,
				  prTxDone->u2SequenceNumber);

				/* call related TX Done Handler */
				prMsduInfo = nicGetPendingTxMsduInfo(prAdapter,
					prTxDone->ucWlanIndex,
					prTxDone->ucPacketSeq);

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
				DBGLOG(INIT, TRACE,
					"EVENT_ID_TX_DONE u4TimeStamp = %x u2AirDelay = %x\n",
				  prTxDone->au4Reserved1,
				  prTxDone->au4Reserved2);

				wnmReportTimingMeas(prAdapter,
					prMsduInfo->ucStaRecIndex,
					prTxDone->au4Reserved1,
					prTxDone->au4Reserved1 +
						prTxDone->au4Reserved2);
#endif

				if (prMsduInfo) {
					prMsduInfo->pfTxDoneHandler(
						prAdapter, prMsduInfo,
						(enum ENUM_TX_RESULT_CODE)
							(prTxDone->ucStatus));

					if (prMsduInfo->eSrc == TX_PACKET_MGMT)
						cnmMgtPktFree(
							prAdapter, prMsduInfo);
					else
						nicTxReturnMsduInfo(
							prAdapter, prMsduInfo);
				}
			}
#endif
			break;

		case EVENT_ID_SLEEPY_INFO:
#if defined(_HIF_USB)
#else
		{
			struct EVENT_SLEEPY_INFO *prEventSleepyNotify;

			prEventSleepyNotify = (struct EVENT_SLEEPY_INFO *) (
						      prEvent->aucBuffer);

			prAdapter->fgWiFiInSleepyState = (u_int8_t) (
					prEventSleepyNotify->ucSleepyState);

#if CFG_SUPPORT_MULTITHREAD
			if (prEventSleepyNotify->ucSleepyState)
				kalSetFwOwnEvent2Hif(prGlueInfo);
#endif
		}
#endif
		break;
		case EVENT_ID_BT_OVER_WIFI:
#if CFG_ENABLE_BT_OVER_WIFI
		{
			uint8_t aucTmp[sizeof(struct BT_OVER_WIFI_EVENT) +
				sizeof(struct BOW_LINK_DISCONNECTED)];
			struct EVENT_BT_OVER_WIFI *prEventBtOverWifi;
			struct BT_OVER_WIFI_EVENT *prBowEvent;
			struct BOW_LINK_CONNECTED *prBowLinkConnected;
			struct BOW_LINK_DISCONNECTED *prBowLinkDisconnected;

			prEventBtOverWifi = (struct EVENT_BT_OVER_WIFI *) (
						    prEvent->aucBuffer);

			/* construct event header */
			prBowEvent = (struct BT_OVER_WIFI_EVENT *) aucTmp;

			if (prEventBtOverWifi->ucLinkStatus == 0) {
				/* Connection */
				prBowEvent->rHeader.ucEventId =
					BOW_EVENT_ID_LINK_CONNECTED;
				prBowEvent->rHeader.ucSeqNumber = 0;
				prBowEvent->rHeader.u2PayloadLength = sizeof(
						struct BOW_LINK_CONNECTED);

				/* fill event body */
				prBowLinkConnected =
					(struct BOW_LINK_CONNECTED *)
					(prBowEvent->aucPayload);
				prBowLinkConnected->rChannel.ucChannelNum =
					prEventBtOverWifi->ucSelectedChannel;
				kalMemZero(prBowLinkConnected->aucPeerAddress,
					   MAC_ADDR_LEN);	/* @FIXME */

				kalIndicateBOWEvent(
					prAdapter->prGlueInfo, prBowEvent);
			} else {
				/* Disconnection */
				prBowEvent->rHeader.ucEventId =
					BOW_EVENT_ID_LINK_DISCONNECTED;
				prBowEvent->rHeader.ucSeqNumber = 0;
				prBowEvent->rHeader.u2PayloadLength = sizeof(
						struct BOW_LINK_DISCONNECTED);

				/* fill event body */
				prBowLinkDisconnected =
					(struct BOW_LINK_DISCONNECTED *)
						(prBowEvent->aucPayload);
				/* @FIXME */
				prBowLinkDisconnected->ucReason = 0;
				kalMemZero(
					prBowLinkDisconnected->aucPeerAddress,
					MAC_ADDR_LEN);	/* @FIXME */

				kalIndicateBOWEvent(
					prAdapter->prGlueInfo, prBowEvent);
			}
		}
		break;
#endif
		case EVENT_ID_STATISTICS:
			/* buffer statistics for further query */
			prAdapter->fgIsStatValid = TRUE;
			prAdapter->rStatUpdateTime = kalGetTimeTick();
			kalMemCopy(&prAdapter->rStatStruct, prEvent->aucBuffer,
				   sizeof(struct EVENT_STATISTICS));

			/* command response handling */
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(
						prAdapter, prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(prAdapter->prGlueInfo,
						prCmdInfo,
						0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

			break;

#if CFG_SUPPORT_MSP
		case EVENT_ID_WTBL_INFO:
			/* buffer statistics for further query */
			prAdapter->fgIsStatValid = TRUE;
			prAdapter->rStatUpdateTime = kalGetTimeTick();
			kalMemCopy(&prAdapter->rEventWlanInfo,
				prEvent->aucBuffer,
				sizeof(struct EVENT_WLAN_INFO));

			DBGLOG(RSN, INFO, "EVENT_ID_WTBL_INFO");
			/* command response handling */
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(
						prAdapter, prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(prAdapter->prGlueInfo,
						prCmdInfo,
						0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

			break;

		case EVENT_ID_MIB_INFO:
			/* buffer statistics for further query */
			prAdapter->fgIsStatValid = TRUE;
			prAdapter->rStatUpdateTime = kalGetTimeTick();

			DBGLOG(RSN, INFO, "EVENT_ID_MIB_INFO");
			/* command response handling */
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(prAdapter,
						prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(prAdapter->prGlueInfo,
						prCmdInfo,
						0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

			break;
#endif
		case EVENT_ID_CH_PRIVILEGE:
			cnmChMngrHandleChEvent(prAdapter, prEvent);
			break;

		case EVENT_ID_BSS_ABSENCE_PRESENCE:
			qmHandleEventBssAbsencePresence(prAdapter, prEvent);
			break;

		case EVENT_ID_STA_CHANGE_PS_MODE:
			qmHandleEventStaChangePsMode(prAdapter, prEvent);
			break;
#if CFG_ENABLE_WIFI_DIRECT
		case EVENT_ID_STA_UPDATE_FREE_QUOTA:
			qmHandleEventStaUpdateFreeQuota(prAdapter, prEvent);
			break;
#endif
		case EVENT_ID_BSS_BEACON_TIMEOUT:
			DBGLOG(INIT, INFO, "EVENT_ID_BSS_BEACON_TIMEOUT\n");

			if (prAdapter->fgDisBcnLostDetection == FALSE) {
				struct BSS_INFO *prBssInfo =
					(struct BSS_INFO *) NULL;
				struct EVENT_BSS_BEACON_TIMEOUT
					*prEventBssBeaconTimeout;

				prEventBssBeaconTimeout =
					(struct EVENT_BSS_BEACON_TIMEOUT *)
						(prEvent->aucBuffer);

				if (prEventBssBeaconTimeout->ucBssIndex >=
				    prAdapter->ucHwBssIdNum)
					break;

				DBGLOG(INIT, INFO, "Reason code: %d\n",
				       prEventBssBeaconTimeout->ucReasonCode);

				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					prEventBssBeaconTimeout->ucBssIndex);

				if (prEventBssBeaconTimeout->ucBssIndex ==
				    prAdapter->prAisBssInfo->ucBssIndex)
					aisBssBeaconTimeout(prAdapter);
#if CFG_ENABLE_WIFI_DIRECT
				else if (prBssInfo->eNetworkType ==
					NETWORK_TYPE_P2P)
					p2pRoleFsmRunEventBeaconTimeout(
						prAdapter, prBssInfo);
#endif
#if CFG_ENABLE_BT_OVER_WIFI
				else if (GET_BSS_INFO_BY_INDEX(prAdapter,
					prEventBssBeaconTimeout->ucBssIndex)->
					eNetworkType == NETWORK_TYPE_BOW) {
					/* ToDo:: Nothing */
				}
#endif
				else {
					DBGLOG(RX, ERROR,
						"EVENT_ID_BSS_BEACON_TIMEOUT: (ucBssIndex = %d)\n",
					  prEventBssBeaconTimeout->ucBssIndex);
				}
			}

			break;
		case EVENT_ID_UPDATE_NOA_PARAMS:
#if CFG_ENABLE_WIFI_DIRECT
			if (prAdapter->fgIsP2PRegistered) {
				struct EVENT_UPDATE_NOA_PARAMS
					*prEventUpdateNoaParam;

				prEventUpdateNoaParam =
					(struct EVENT_UPDATE_NOA_PARAMS *)
					(prEvent->aucBuffer);

				if (GET_BSS_INFO_BY_INDEX(prAdapter,
						prEventUpdateNoaParam->
						ucBssIndex)->
						eNetworkType ==
						NETWORK_TYPE_P2P) {
					p2pProcessEvent_UpdateNOAParam(
						prAdapter,
						prEventUpdateNoaParam->
						ucBssIndex,
						prEventUpdateNoaParam);
				} else {
					ASSERT(0);
				}
			}
#else
			ASSERT(0);
#endif
			break;

		case EVENT_ID_STA_AGING_TIMEOUT:
#if CFG_ENABLE_WIFI_DIRECT
		{
			if (prAdapter->fgDisStaAgingTimeoutDetection == FALSE) {
				struct EVENT_STA_AGING_TIMEOUT
					*prEventStaAgingTimeout;
				struct STA_RECORD *prStaRec;
				struct BSS_INFO *prBssInfo =
					(struct BSS_INFO *) NULL;

				prEventStaAgingTimeout =
					(struct EVENT_STA_AGING_TIMEOUT *)
					(prEvent->aucBuffer);
				prStaRec = cnmGetStaRecByIndex(prAdapter,
					prEventStaAgingTimeout->ucStaRecIdx);
				if (prStaRec == NULL)
					break;

				DBGLOG(INIT, INFO,
				  "EVENT_ID_STA_AGING_TIMEOUT %u " MACSTR "\n",
				  prEventStaAgingTimeout->ucStaRecIdx,
				  MAC2STR(prStaRec->aucMacAddr));

				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					prStaRec->ucBssIndex);

				bssRemoveClient(prAdapter, prBssInfo, prStaRec);

				/* Call False Auth */
				if (prAdapter->fgIsP2PRegistered) {
					p2pFuncDisconnect(prAdapter,
					prBssInfo, prStaRec, TRUE,
					REASON_CODE_DISASSOC_INACTIVITY);
				}

			}
			/* gDisStaAgingTimeoutDetection */
		}
#endif
		break;

		case EVENT_ID_AP_OBSS_STATUS:
#if CFG_ENABLE_WIFI_DIRECT
			if (prAdapter->fgIsP2PRegistered)
				rlmHandleObssStatusEventPkt(prAdapter,
					(struct EVENT_AP_OBSS_STATUS *)
					prEvent->aucBuffer);
#endif
			break;

		case EVENT_ID_ROAMING_STATUS:
#if CFG_SUPPORT_ROAMING
		{
			struct CMD_ROAMING_TRANSIT *prTransit;

			prTransit = (struct CMD_ROAMING_TRANSIT *) (
					    prEvent->aucBuffer);
			roamingFsmProcessEvent(prAdapter, prTransit);
		}
#endif /* CFG_SUPPORT_ROAMING */
		break;
		case EVENT_ID_SEND_DEAUTH:
#if DBG
		{
			struct WLAN_MAC_HEADER *prWlanMacHeader;

			prWlanMacHeader = (struct WLAN_MAC_HEADER *)
				prEvent->aucBuffer;
			DBGLOG(RX, INFO, "nicRx: aucAddr1: " MACSTR "\n",
			       MAC2STR(prWlanMacHeader->aucAddr1));
			DBGLOG(RX, INFO, "nicRx: aucAddr2: " MACSTR "\n",
			       MAC2STR(prWlanMacHeader->aucAddr2));
		}
#endif
			/* receive packets without StaRec */
		prSwRfb->pvHeader = (struct WLAN_MAC_HEADER *)
			prEvent->aucBuffer;
		if (authSendDeauthFrame(prAdapter,
			NULL,
			NULL,
			prSwRfb,
			REASON_CODE_CLASS_3_ERR,
			(PFN_TX_DONE_HANDLER) NULL) ==
			WLAN_STATUS_SUCCESS) {
			DBGLOG(RX, INFO, "Send Deauth Error\n");
		}
		break;

#if CFG_SUPPORT_RDD_TEST_MODE
		case EVENT_ID_UPDATE_RDD_STATUS: {
			struct EVENT_RDD_STATUS *prEventRddStatus;

			prEventRddStatus = (struct EVENT_RDD_STATUS *) (
						   prEvent->aucBuffer);

			prAdapter->ucRddStatus = prEventRddStatus->ucRddStatus;
		}

		break;
#endif

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	case EVENT_ID_UPDATE_BWCS_STATUS:
		{
			struct PTA_IPC *prEventBwcsStatus;

			prEventBwcsStatus =
				(struct PTA_IPC *) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			DBGLOG(RSN, EVENT, "BCM BWCS Event: %02x%02x%02x%02x\n",
				prEventBwcsStatus->u.aucBTPParams[0],
				prEventBwcsStatus->u.aucBTPParams[1],
				prEventBwcsStatus->u.aucBTPParams[2],
				prEventBwcsStatus->u.aucBTPParams[3]);
#endif

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				WLAN_STATUS_BWCS_UPDATE,
				(void *) prEventBwcsStatus,
				sizeof(struct PTA_IPC));
		}

		break;

		case EVENT_ID_UPDATE_BCM_DEBUG: {
			struct PTA_IPC *prEventBwcsStatus;

			prEventBwcsStatus =
				(struct PTA_IPC *) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			DBGLOG(RSN, EVENT, "BCM FW status: %02x%02x%02x%02x\n",
			       prEventBwcsStatus->u.aucBTPParams[0],
			       prEventBwcsStatus->u.aucBTPParams[1],
			       prEventBwcsStatus->u.aucBTPParams[2],
			       prEventBwcsStatus->u.aucBTPParams[3]);
#endif
		}

		break;
#endif
		case EVENT_ID_ADD_PKEY_DONE: {
			struct EVENT_ADD_KEY_DONE_INFO *prAddKeyDone;
			struct STA_RECORD *prStaRec;

			prAddKeyDone = (struct EVENT_ADD_KEY_DONE_INFO *) (
					       prEvent->aucBuffer);

			DBGLOG(RSN, EVENT,
			       "EVENT_ID_ADD_PKEY_DONE BSSIDX=%d " MACSTR "\n",
			       prAddKeyDone->ucBSSIndex,
			       MAC2STR(prAddKeyDone->aucStaAddr));

			prStaRec = cnmGetStaRecByAddress(prAdapter,
				prAddKeyDone->ucBSSIndex,
				prAddKeyDone->aucStaAddr);

			if (prStaRec) {
				DBGLOG(RSN, EVENT,
					"STA " MACSTR " Add Key Done!!\n",
				  MAC2STR(prStaRec->aucMacAddr));
				prStaRec->fgIsTxKeyReady = TRUE;
				qmUpdateStaRec(prAdapter, prStaRec);
			}
		}
		break;
		case EVENT_ID_ICAP_DONE: {
			struct EVENT_ICAP_STATUS *prEventIcapStatus;
			struct PARAM_CUSTOM_MEM_DUMP_STRUCT rMemDumpInfo;
			uint32_t u4QueryInfo;

			prEventIcapStatus = (struct EVENT_ICAP_STATUS *) (
						    prEvent->aucBuffer);

			rMemDumpInfo.u4Address =
				prEventIcapStatus->u4StartAddress;
			rMemDumpInfo.u4Length =
				prEventIcapStatus->u4IcapSieze;
#if CFG_SUPPORT_QA_TOOL
			rMemDumpInfo.u4IcapContent =
				prEventIcapStatus->u4IcapContent;
#endif

			wlanoidQueryMemDump(prAdapter, &rMemDumpInfo,
					    sizeof(rMemDumpInfo), &u4QueryInfo);

		}

		break;
		case EVENT_ID_DEBUG_MSG: {
			struct EVENT_DEBUG_MSG *prEventDebugMsg;
			uint16_t u2DebugMsgId;
			uint8_t ucMsgType;
			uint8_t ucFlags;
			uint32_t u4Value;
			uint16_t u2MsgSize;
			uint8_t *pucMsg;

			prEventDebugMsg = (struct EVENT_DEBUG_MSG *) (
						  prEvent->aucBuffer);

			u2DebugMsgId = prEventDebugMsg->u2DebugMsgId;
			ucMsgType = prEventDebugMsg->ucMsgType;
			ucFlags = prEventDebugMsg->ucFlags;
			u4Value = prEventDebugMsg->u4Value;
			u2MsgSize = prEventDebugMsg->u2MsgSize;
			pucMsg = prEventDebugMsg->aucMsg;

			DBGLOG(SW4, TRACE,
			  "DEBUG_MSG Id %u Type %u Fg 0x%x Val 0x%x Size %u\n",
				u2DebugMsgId, ucMsgType,
				ucFlags, u4Value, u2MsgSize);

			if (u2MsgSize <= DEBUG_MSG_SIZE_MAX) {
				if (ucMsgType >= DEBUG_MSG_TYPE_END)
					ucMsgType = DEBUG_MSG_TYPE_MEM32;

				if (ucMsgType == DEBUG_MSG_TYPE_ASCII) {
					uint8_t *pucChr;

					pucMsg[u2MsgSize] = '\0';

					/* skip newline */
					pucChr = kalStrChr(pucMsg, '\0');
					if (*(pucChr - 1) == '\n')
						*(pucChr - 1) = '\0';

					DBGLOG(SW4, INFO, "<FW>%s\n", pucMsg);
				} else if (ucMsgType == DEBUG_MSG_TYPE_MEM8) {
					DBGLOG(SW4, INFO, "<FW>Dump MEM8\n");
					DBGLOG_MEM8(SW4, INFO,
						pucMsg, u2MsgSize);
				} else {
					DBGLOG(SW4, INFO, "<FW>Dump MEM32\n");
					DBGLOG_MEM32(SW4, INFO, pucMsg,
						u2MsgSize);
				}
			} /* DEBUG_MSG_SIZE_MAX */
			else
				DBGLOG(SW4, INFO,
					"Debug msg size %u is too large.\n",
				  u2MsgSize);
		}
		break;

#if CFG_SUPPORT_BATCH_SCAN
		case EVENT_ID_BATCH_RESULT:
			DBGLOG(SCN, TRACE, "Got EVENT_ID_BATCH_RESULT");

			/* command response handling */
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(prAdapter,
						prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(prAdapter->prGlueInfo,
						prCmdInfo,
						0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

			break;
#endif /* CFG_SUPPORT_BATCH_SCAN */

#if CFG_SUPPORT_TDLS
		case EVENT_ID_TDLS:

			TdlsexEventHandle(prAdapter->prGlueInfo,
				(uint8_t *) prEvent->aucBuffer,
				(uint32_t) (prEvent->u2PacketLength - 8));
			break;
#endif /* CFG_SUPPORT_TDLS */

		case EVENT_ID_DUMP_MEM:
			DBGLOG(INIT, INFO, "%s: EVENT_ID_DUMP_MEM\n", __func__);

			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				DBGLOG(INIT, INFO, ": ==> 1\n");
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(prAdapter,
						prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(prAdapter->prGlueInfo,
						prCmdInfo,
						0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				/* Burst mode */
				DBGLOG(INIT, INFO, ": ==> 2\n");
				nicEventQueryMemDump(prAdapter,
					prEvent->aucBuffer);
			}
			break;
#if CFG_ASSERT_DUMP
		case EVENT_ID_ASSERT_DUMP:

			if (prEvent->ucS2DIndex == S2D_INDEX_EVENT_N2H) {
				if (!prAdapter->fgN9AssertDumpOngoing) {
					DBGLOG(INIT, INFO,
						"%s: EVENT_ID_ASSERT_DUMP\n",
						__func__);
					DBGLOG(INIT, INFO,
					       "\n[DUMP_N9]====N9 ASSERT_DUMPSTART====\n");
					fgKeepPrintCoreDump = TRUE;
					if (kalOpenCorDumpFile(TRUE) !=
						WLAN_STATUS_SUCCESS)
						DBGLOG(INIT, INFO,
							"kalOpenCorDumpFile fail\n");
					else
						prAdapter->fgN9CorDumpFileOpend
							= TRUE;

					prAdapter->fgN9AssertDumpOngoing = TRUE;
				} else if (prAdapter->fgN9AssertDumpOngoing) {

					if (fgKeepPrintCoreDump)
						DBGLOG(INIT, INFO,
							"[DUMP_N9]%s:\n",
							prEvent->aucBuffer);
					if (!kalStrnCmp(prEvent->aucBuffer,
							";more log added here",
							5) || !kalStrnCmp(
							prEvent->aucBuffer,
							";[core dump start]",
							5))
						fgKeepPrintCoreDump = FALSE;

					if (prAdapter->fgN9CorDumpFileOpend &&
						(kalWriteCorDumpFile(
							prEvent->aucBuffer,
							prEvent->u2PacketLength,
							TRUE) !=
							WLAN_STATUS_SUCCESS)) {
						DBGLOG(INIT, INFO,
								"kalWriteN9CorDumpFile fail\n");
					}
					wlanCorDumpTimerReset(prAdapter, TRUE);
				}
			} else {
				/* prEvent->ucS2DIndex == S2D_INDEX_EVENT_C2H */
				if (!prAdapter->fgCr4AssertDumpOngoing) {
					DBGLOG(INIT, INFO,
						"%s: EVENT_ID_ASSERT_DUMP\n",
						__func__);
					DBGLOG(INIT, INFO,
					       "\n[DUMP_Cr4]====CR4 ASSERT_DUMPSTART====\n");
					fgKeepPrintCoreDump = TRUE;
					if (kalOpenCorDumpFile(FALSE) !=
						WLAN_STATUS_SUCCESS)
						DBGLOG(INIT, INFO,
							"kalOpenCorDumpFile fail\n");
					else
						prAdapter->
						fgCr4CorDumpFileOpend = TRUE;

					prAdapter->fgCr4AssertDumpOngoing =
						TRUE;
				} else if (prAdapter->fgCr4AssertDumpOngoing) {
					if (fgKeepPrintCoreDump)
						DBGLOG(INIT, INFO,
							"[DUMP_CR4]%s:\n",
							prEvent->aucBuffer);
					if (!kalStrnCmp(prEvent->aucBuffer,
							";more log added here",
							5))
						fgKeepPrintCoreDump = FALSE;

					if (prAdapter->fgCr4CorDumpFileOpend &&
						(kalWriteCorDumpFile(
							prEvent->aucBuffer,
							prEvent->u2PacketLength,
							FALSE) !=
						  WLAN_STATUS_SUCCESS)) {
						DBGLOG(INIT, INFO,
							"kalWriteN9CorDumpFile fail\n");
					}
					wlanCorDumpTimerReset(prAdapter, FALSE);
				}
			}
			break;

#endif

		case EVENT_ID_RDD_SEND_PULSE:
			DBGLOG(INIT, INFO, "%s: EVENT_ID_RDD_SEND_PULSE\n",
			       __func__);

			nicEventRddPulseDump(prAdapter, prEvent->aucBuffer);
			break;

		case EVENT_ID_ACCESS_RX_STAT:
		case EVENT_ID_ACCESS_REG:
		case EVENT_ID_NIC_CAPABILITY:
		case EVENT_ID_ACCESS_EEPROM:
		case EVENT_ID_TEST_STATUS:
		default:
			prCmdInfo = nicGetPendingCmdInfo(prAdapter,
							 prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if (prCmdInfo->pfCmdDoneHandler)
					prCmdInfo->pfCmdDoneHandler(prAdapter,
						prCmdInfo,
						prEvent->aucBuffer);
				else if (prCmdInfo->fgIsOid)
					kalOidComplete(prAdapter->prGlueInfo,
						prCmdInfo,
						       0, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

			break;
		}

		/* Reset Chip NoAck flag */
		if (prGlueInfo->prAdapter->fgIsChipNoAck) {
		DBGLOG(INIT, WARN,
		       "Got response from chip, clear NoAck flag!\n");
			WARN_ON(TRUE);
		}
		prGlueInfo->prAdapter->ucOidTimeoutCount = 0;
		prGlueInfo->prAdapter->fgIsChipNoAck = FALSE;

		nicRxReturnRFB(prAdapter, prSwRfb);
	}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief nicRxProcessMgmtPacket is used to dispatch management frames
 *        to corresponding modules
 *
 * @param prAdapter Pointer to the Adapter structure.
 * @param prSWRfb the RFB to receive rx data
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessMgmtPacket(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb)
{
	struct GLUE_INFO *prGlueInfo;
	uint8_t ucSubtype;
#if CFG_SUPPORT_802_11W
	/* BOOL   fgMfgDrop = FALSE; */
#endif
#if CFG_WIFI_SW_CIPHER_MISMATCH
	struct WLAN_MAC_HEADER *prWlanHeader = NULL;
#endif
	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	nicRxFillRFB(prAdapter, prSwRfb);

	if (!prSwRfb->pvHeader || !prSwRfb->pvPacket) {
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_NULL_PACKET_COUNT);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DROP_TOTAL_COUNT);
		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}

#if CFG_WIFI_SW_CIPHER_MISMATCH
	prWlanHeader = (struct WLAN_MAC_HEADER *) prSwRfb->pvHeader;
#endif
	ucSubtype = (*(uint8_t *) (prSwRfb->pvHeader) &
		     MASK_FC_SUBTYPE) >> OFFSET_OF_FC_SUBTYPE;

#if CFG_RX_PKTS_DUMP
	{
		struct WLAN_MAC_MGMT_HEADER *prWlanMgmtHeader;
		uint16_t u2TxFrameCtrl;

		u2TxFrameCtrl = (*(uint8_t *) (prSwRfb->pvHeader) &
				 MASK_FRAME_TYPE);

#if ((CFG_SUPPORT_802_11AX == 1) && (CFG_SUPPORT_WIFI_SYSDVT == 1))
	if (fgEfuseCtrlAxOn == 1) {
		if (RXM_IS_TRIGGER_FRAME(u2TxFrameCtrl)) {
			if (prAdapter->fgEnShowHETrigger) {
				DBGLOG(NIC, STATE,
						"HE Trigger --------------\n");
				dumpMemory8((uint8_t *)prSwRfb->prRxStatus,
					prSwRfb->u2RxByteCount);
				DBGLOG(NIC, STATE,
						"HE Trigger end --------------\n");
			}
			nicRxReturnRFB(prAdapter, prSwRfb);
			return;
		}
	}
#endif /* CFG_SUPPORT_802_11AX == 1 */

		if (prAdapter->rRxCtrl.u4RxPktsDumpTypeMask & BIT(
			    HIF_RX_PKT_TYPE_MANAGEMENT)) {
			if (u2TxFrameCtrl == MAC_FRAME_BEACON
			    || u2TxFrameCtrl == MAC_FRAME_PROBE_RSP) {

				prWlanMgmtHeader =
					(struct WLAN_MAC_MGMT_HEADER *) (
							   prSwRfb->pvHeader);

				DBGLOG(SW4, INFO,
					"QM RX MGT: net %u sta idx %u wlan idx %u ssn %u ptype %u subtype %u 11 %u\n",
				  prSwRfb->prStaRec->ucBssIndex,
				  prSwRfb->ucStaRecIdx,
				  prSwRfb->ucWlanIdx,
				  prWlanMgmtHeader->u2SeqCtrl,
				  /* The new SN of the frame */
				  prSwRfb->ucPacketType, ucSubtype);
				/* HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr))); */

				DBGLOG_MEM8(SW4, TRACE,
					(uint8_t *) prSwRfb->pvHeader,
					prSwRfb->u2PacketLen);
			}
		}
	}
#endif
#if CFG_SUPPORT_802_11W
	if (prSwRfb->fgIcvErr) {
		if (prSwRfb->ucSecMode == CIPHER_SUITE_BIP)
			DBGLOG(RSN, INFO, "[MFP] RX with BIP ICV ERROR\n");
		else
			DBGLOG(RSN, INFO, "[MFP] RX with ICV ERROR\n");

		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DROP_TOTAL_COUNT);
		return;
	}
#endif

#if CFG_WIFI_SW_CIPHER_MISMATCH
	if ((rsnCheckBipKeyInstalled(prAdapter, prSwRfb->prStaRec))
		&& (prSwRfb->prStaRec->ucStaState == STA_STATE_3)
		&& (!(prWlanHeader->u2FrameCtrl & MASK_FC_PROTECTED_FRAME))
		&& (prSwRfb->fgIsBC == FALSE)
		&& (prSwRfb->fgIsMC == FALSE)) {
		prSwRfb->fgIsCipherMS = TRUE;
	}
#endif

	if (prAdapter->fgTestMode == FALSE) {
#if CFG_MGMT_FRAME_HANDLING
		prGlueInfo = prAdapter->prGlueInfo;
		if ((prGlueInfo == NULL) || (prGlueInfo->u4ReadyFlag == 0)) {
			DBGLOG(RX, WARN,
			   "Bypass this mgmt frame without wlanProbe done\n");
		} else if (apfnProcessRxMgtFrame[ucSubtype]) {
			switch (apfnProcessRxMgtFrame[ucSubtype] (prAdapter,
					prSwRfb)) {
			case WLAN_STATUS_PENDING:
				return;
			case WLAN_STATUS_SUCCESS:
			case WLAN_STATUS_FAILURE:
				break;

			default:
				DBGLOG(RX, WARN,
				       "Unexpected MMPDU(0x%02X) returned with abnormal status\n",
				       ucSubtype);
				break;
			}
		}
#endif
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

void nicRxProcessMsduReport(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb)
{
	halRxProcessMsduReport(prAdapter, prSwRfb);

	nicRxReturnRFB(prAdapter, prSwRfb);
}

void nicRxProcessRxReport(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb)
{
	struct HW_MAC_RX_REPORT *prRxRpt;
	uint32_t *prRxv = NULL;
	uint32_t u4RxvOfst, u4Idx;
	uint16_t u2RxByteCntHw, u2RxByteCntSw, u2PRXVCnt;
	uint8_t ucDataType;
	struct SW_RX_RPT_BLK_RXV *prRxRptBlkRxv = NULL;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);

	prRxRpt = (struct HW_MAC_RX_REPORT *)prSwRfb->pucRecvBuff;
	u2RxByteCntHw = RX_RPT_GET_RX_BYTE_COUNT(prRxRpt);
	u2RxByteCntSw = RX_RPT_HDR_LEN + RX_RPT_USER_INFO_LEN;
	u2PRXVCnt = RX_RPT_GET_RXV_PRXV_BYTE_COUNT(prRxRpt);
	u4RxvOfst = (RX_RPT_HDR_LEN + RX_RPT_USER_INFO_LEN
		+ RX_RPT_BLK_HDR_LEN) << 2;

	/* Sanity check */
	if (RX_RPT_GET_RXV_BLK_EXIST(prRxRpt))
		u2RxByteCntSw += RX_RPT_BLK_HDR_LEN;
	if (RX_RPT_GET_RXV_TYPE_CRXV1_VLD(prRxRpt))
		u2RxByteCntSw += RX_RPT_BLK_CRXV1_LEN;
	if (RX_RPT_GET_RXV_TYPE_PRXV1_VLD(prRxRpt))
		u2RxByteCntSw += RX_RPT_BLK_PRXV1_LEN;
	if (RX_RPT_GET_RXV_TYPE_PRXV2_VLD(prRxRpt))
		u2RxByteCntSw += RX_RPT_BLK_PRXV2_LEN;
	if (RX_RPT_GET_RXV_TYPE_CRXV2_VLD(prRxRpt))
		u2RxByteCntSw += RX_RPT_BLK_CRXV2_LEN;

	if (u2RxByteCntHw != (u2RxByteCntSw << 2)) {
		DBGLOG(RX, ERROR, "Expect %d bytes but real %d bytes !!\n",
			(u2RxByteCntSw << 2), u2RxByteCntHw);
		return;
	}

	prSwRfb->ucStaRecIdx = secGetStaIdxByWlanIdx(prAdapter,
		(uint8_t) RX_RPT_GET_WLAN_ID(prRxRpt));

	if (prSwRfb->ucStaRecIdx >= CFG_STA_REC_NUM)
		return;

	/* Only check data frame */
	ucDataType = (uint8_t) RX_RPT_GET_FRAME_TYPE(prRxRpt);
	if (!RX_RPT_IS_DATA_FRAME(ucDataType))
		return;

	prRxRptBlkRxv = (struct SW_RX_RPT_BLK_RXV *)kalMemAlloc(
			sizeof(struct SW_RX_RPT_BLK_RXV), VIR_MEM_TYPE);
	if (!prRxRptBlkRxv) {
		DBGLOG(RX, ERROR, "Allocate prRxRptBlkRxv failed!\n");
		return;
	}

	if (RX_RPT_GET_RXV_BLK_EXIST(prRxRpt)) {
		if (RX_RPT_GET_RXV_TYPE_CRXV1_VLD(prRxRpt)) {
			prRxv = (uint32_t *)((uint8_t *)prRxRpt + u4RxvOfst);
			for (u4Idx = 0; u4Idx < RX_RPT_BLK_CRXV1_LEN; u4Idx++) {
				prRxRptBlkRxv->u4CRxv1[u4Idx] =
					*(prRxv + u4Idx);
			}

			u4RxvOfst += (RX_RPT_BLK_CRXV1_LEN << 2);
		}
		if (RX_RPT_GET_RXV_TYPE_PRXV1_VLD(prRxRpt)) {
			prRxv = (uint32_t *)((uint8_t *)prRxRpt + u4RxvOfst);
			for (u4Idx = 0; u4Idx < RX_RPT_BLK_PRXV1_LEN; u4Idx++)
				prRxRptBlkRxv->u4PRxv1[u4Idx] =
					*(prRxv + u4Idx);

			u4RxvOfst += (RX_RPT_BLK_PRXV1_LEN << 2);
		}
		if (RX_RPT_GET_RXV_TYPE_PRXV2_VLD(prRxRpt)) {
			prRxv = (uint32_t *)((uint8_t *)prRxRpt + u4RxvOfst);
			for (u4Idx = 0; u4Idx < RX_RPT_BLK_PRXV2_LEN; u4Idx++)
				prRxRptBlkRxv->u4PRxv2[u4Idx] =
					*(prRxv + u4Idx);

			u4RxvOfst += (RX_RPT_BLK_PRXV2_LEN << 2);
		}
		if (RX_RPT_GET_RXV_TYPE_CRXV2_VLD(prRxRpt)) {
			prRxv = (uint32_t *)((uint8_t *)prRxRpt + u4RxvOfst);
			for (u4Idx = 0; u4Idx < RX_RPT_BLK_CRXV2_LEN; u4Idx++)
				prRxRptBlkRxv->u4CRxv2[u4Idx] =
					*(prRxv + u4Idx);

			u4RxvOfst += (RX_RPT_BLK_CRXV2_LEN << 2);
		}
	}

	if (prRxRptBlkRxv)
		kalMemFree(prRxRptBlkRxv, VIR_MEM_TYPE,
			sizeof(struct SW_RX_RPT_BLK_RXV));
}

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
static void nicRxCheckWakeupReason(struct ADAPTER *prAdapter,
				   struct SW_RFB *prSwRfb)
{
	struct RX_DESC_OPS_T *prRxDescOps;

	prRxDescOps = prAdapter->chip_info->prRxDescOps;
	if (prRxDescOps->nic_rxd_check_wakeup_reason)
		prRxDescOps->nic_rxd_check_wakeup_reason(prAdapter, prSwRfb);
	else
		DBGLOG(RX, ERROR,
			"%s:: no nic_rxd_check_wakeup_reason??\n",
			__func__);
}
#endif /* CFG_SUPPORT_WAKEUP_REASON_DEBUG */

#if (CFG_SUPPORT_ICS == 1)
static void nicRxProcessIcsLog(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb)
{
	struct ICS_AGG_HEADER *prIcsAggHeader;
	struct ICS_BIN_LOG_HDR *prIcsBinLogHeader;
	void *pvPacket = NULL;
	uint32_t u4Size = 0;
	uint8_t *pucRecvBuff;
	ssize_t ret;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prIcsAggHeader = (struct ICS_AGG_HEADER *)prSwRfb->prRxStatus;
	u4Size = prIcsAggHeader->rxByteCount + sizeof(
			struct ICS_BIN_LOG_HDR);
	pvPacket = kalPacketAlloc(prAdapter->prGlueInfo, u4Size,
			&pucRecvBuff);

	if (pvPacket) {
		/* prepare ICS header */
		prIcsBinLogHeader = (struct ICS_BIN_LOG_HDR *)pucRecvBuff;
		prIcsBinLogHeader->u4MagicNum = ICS_BIN_LOG_MAGIC_NUM;
		prIcsBinLogHeader->u4Timestamp = 0;
		prIcsBinLogHeader->u2MsgID = RX_PKT_TYPE_ICS;
		prIcsBinLogHeader->u2Length = prIcsAggHeader->rxByteCount;

		/* prepare ICS frame */
		kalMemCopy(pucRecvBuff + sizeof(struct ICS_BIN_LOG_HDR),
				prIcsAggHeader,
				prIcsAggHeader->rxByteCount);

		/* write to ring, ret: written */
		ret = wifi_ics_fwlog_write(pucRecvBuff, u4Size);
		if (ret != u4Size)
			DBGLOG_LIMITED(NIC, INFO,
				"dropped written:%d rxByteCount:%d\n",
				ret, prIcsAggHeader->rxByteCount);

		kalPacketFree(prAdapter->prGlueInfo, pvPacket);
	}
}
#endif /* CFG_SUPPORT_ICS */

static void nicRxProcessPacketType(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	struct mt66xx_chip_info *prChipInfo;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;

	prRxCtrl = &prAdapter->rRxCtrl;
	prChipInfo = prAdapter->chip_info;

	switch (prSwRfb->ucPacketType) {
	case RX_PKT_TYPE_RX_DATA:
		if (HAL_IS_RX_DIRECT(prAdapter)
			&& HAL_MON_EN(prAdapter)) {
			spin_lock_bh(&prGlueInfo->rSpinLock[
				SPIN_LOCK_RX_DIRECT]);
			nicRxProcessMonitorPacket(
				prAdapter, prSwRfb);
			spin_unlock_bh(&prGlueInfo->rSpinLock[
				SPIN_LOCK_RX_DIRECT]);
			break;
		} else if (HAL_MON_EN(prAdapter)) {
			nicRxProcessMonitorPacket(
				prAdapter, prSwRfb);
			break;
		}
		if (HAL_IS_RX_DIRECT(prAdapter)) {
			spin_lock_bh(&prGlueInfo->rSpinLock[
				SPIN_LOCK_RX_DIRECT]);
			nicRxProcessDataPacket(
				prAdapter,
				prSwRfb);
			spin_unlock_bh(&prGlueInfo->rSpinLock[
				SPIN_LOCK_RX_DIRECT]);
		} else {
			nicRxProcessDataPacket(
				prAdapter,
				prSwRfb);
		}
		break;

	case RX_PKT_TYPE_SW_DEFINED:
		/* HIF_RX_PKT_TYPE_EVENT */
		if ((NIC_RX_GET_U2_SW_PKT_TYPE(
			prSwRfb->prRxStatus) &
		     prChipInfo->u2RxSwPktBitMap) ==
		    prChipInfo->u2RxSwPktEvent) {
			nicRxProcessEventPacket(
				prAdapter,
				prSwRfb);
		}
		/* case HIF_RX_PKT_TYPE_MANAGEMENT: */
		else if ((NIC_RX_GET_U2_SW_PKT_TYPE(
				prSwRfb->prRxStatus)
			& prChipInfo->u2RxSwPktBitMap)
			== prChipInfo->u2RxSwPktFrame){

			/* OFLD pkts should go data flow
			 * 1: EAPOL
			 * 2: ARP / NS
			 * 3: TDLS
			 */
			RX_STATUS_GET(
				prChipInfo->prRxDescOps,
				prSwRfb->ucOFLD,
				get_ofld,
				prSwRfb->prRxStatus);
			RX_STATUS_GET(
				prChipInfo->prRxDescOps,
				prSwRfb->fgHdrTran,
				get_HdrTrans,
				prSwRfb->prRxStatus);
			if ((prSwRfb->ucOFLD) || (prSwRfb->fgHdrTran)) {
				if (HAL_IS_RX_DIRECT(prAdapter)) {
					spin_lock_bh(&prGlueInfo->rSpinLock[
						SPIN_LOCK_RX_DIRECT]);
					if (HAL_MON_EN(prAdapter))
						nicRxProcessMonitorPacket(
							prAdapter, prSwRfb);
					else
						nicRxProcessDataPacket(
							prAdapter, prSwRfb);
					spin_unlock_bh(&prGlueInfo->rSpinLock[
						SPIN_LOCK_RX_DIRECT]);
				} else {
					if (HAL_MON_EN(prAdapter))
						nicRxProcessMonitorPacket(
							prAdapter, prSwRfb);
					else
						nicRxProcessDataPacket(
							prAdapter, prSwRfb);
				}
			}
			else
				nicRxProcessMgmtPacket(
				prAdapter, prSwRfb);
		} else {
			DBGLOG(RX, ERROR,
				"u2PktTYpe(0x%04X) is OUT OF DEF.!!!\n",
			  NIC_RX_GET_U2_SW_PKT_TYPE(
				prSwRfb->prRxStatus));
			DBGLOG_MEM8(RX, ERROR,
				prSwRfb->pucRecvBuff,
				prSwRfb->u2RxByteCount);

			/*ASSERT(0);*/
			nicRxReturnRFB(prAdapter,
				prSwRfb);
			RX_INC_CNT(prRxCtrl,
				RX_TYPE_ERR_DROP_COUNT);
			RX_INC_CNT(prRxCtrl,
				RX_DROP_TOTAL_COUNT);

		}
		break;

	case RX_PKT_TYPE_MSDU_REPORT:
		nicRxProcessMsduReport(prAdapter,
			prSwRfb);
		break;

	case RX_PKT_TYPE_RX_REPORT:
		nicRxProcessRxReport(prAdapter, prSwRfb);
		nicRxReturnRFB(prAdapter, prSwRfb);
		break;

#if (CFG_SUPPORT_ICS == 1)
	case RX_PKT_TYPE_ICS:
		if ((prAdapter->fgEnTmacICS
			|| prAdapter->fgEnRmacICS) == TRUE)
			nicRxProcessIcsLog(prAdapter, prSwRfb);
		RX_INC_CNT(prRxCtrl, RX_ICS_LOG_COUNT);
		nicRxReturnRFB(prAdapter, prSwRfb);
		break;
#endif /* CFG_SUPPORT_ICS */

	/* case HIF_RX_PKT_TYPE_TX_LOOPBACK: */
	/* case HIF_RX_PKT_TYPE_MANAGEMENT: */
	case RX_PKT_TYPE_TX_STATUS:
	case RX_PKT_TYPE_RX_VECTOR:
	case RX_PKT_TYPE_TM_REPORT:
	default:
		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(prRxCtrl,
			RX_TYPE_ERR_DROP_COUNT);
		RX_INC_CNT(prRxCtrl,
			RX_DROP_TOTAL_COUNT);
		DBGLOG(RX, ERROR, "ucPacketType = %d\n",
		       prSwRfb->ucPacketType);
		break;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief nicProcessRFBs is used to process RFBs in the rReceivedRFBList queue.
 *
 * @param prAdapter Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxProcessRFBs(IN struct ADAPTER *prAdapter)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	struct QUE rTempRfbList;
	struct QUE *prTempRfbList = &rTempRfbList;
	uint32_t u4RxLoopCount, u4Tick;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prRxCtrl->ucNumIndPacket = 0;
	prRxCtrl->ucNumRetainedPacket = 0;
	u4RxLoopCount = prAdapter->rWifiVar.u4TxRxLoopCount;
	u4Tick = kalGetTimeTick();

	QUEUE_INITIALIZE(prTempRfbList);

	while (u4RxLoopCount--) {
		while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rReceivedRfbList)) {

			/* check process RFB timeout */
			if ((kalGetTimeTick() - u4Tick) > RX_PROCESS_TIMEOUT) {
				DBGLOG(RX, WARN,
					"Process RFBs timeout, pending count: %u\n",
					prRxCtrl->rReceivedRfbList.u4NumElem);
				kalSetRxProcessEvent(prAdapter->prGlueInfo);
				break;
			}

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_MOVE_ALL(prTempRfbList,
				&prRxCtrl->rReceivedRfbList);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

			while (QUEUE_IS_NOT_EMPTY(prTempRfbList)) {
				QUEUE_REMOVE_HEAD(prTempRfbList,
					prSwRfb, struct SW_RFB *);

				if (!prSwRfb)
					break;
#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
				if (kalIsWakeupByWlan(prAdapter))
					nicRxCheckWakeupReason(prAdapter,
							       prSwRfb);
#endif

				/* Too many leading tabs -
				 * consider code refactoring
				 */
				nicRxProcessPacketType(prAdapter, prSwRfb);
			}

			if (prRxCtrl->ucNumIndPacket > 0) {
				RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT,
					   prRxCtrl->ucNumRetainedPacket);
#if !CFG_SUPPORT_MULTITHREAD
#if CFG_NATIVE_802_11
				kalRxIndicatePkts(prAdapter->prGlueInfo,
					(uint32_t) prRxCtrl->ucNumIndPacket,
					(uint32_t)
						prRxCtrl->ucNumRetainedPacket);
#else
				kalRxIndicatePkts(prAdapter->prGlueInfo,
				  prRxCtrl->apvIndPacket,
				  (uint32_t) prRxCtrl->ucNumIndPacket);
#endif
#endif
				kalPerMonStart(prAdapter->prGlueInfo);
			}
		}
	}
}				/* end of nicRxProcessRFBs() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Setup a RFB and allocate the os packet to the RFB
 *
 * @param prAdapter      Pointer to the Adapter structure.
 * @param prSwRfb        Pointer to the RFB
 *
 * @retval WLAN_STATUS_SUCCESS
 * @retval WLAN_STATUS_RESOURCES
 */
/*----------------------------------------------------------------------------*/
uint32_t nicRxSetupRFB(IN struct ADAPTER *prAdapter,
		       IN struct SW_RFB *prSwRfb)
{
	void *pvPacket;
	uint8_t *pucRecvBuff;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (!prSwRfb->pvPacket) {
		kalMemZero(prSwRfb, sizeof(struct SW_RFB));
		pvPacket = kalPacketAlloc(prAdapter->prGlueInfo,
					  CFG_RX_MAX_PKT_SIZE, &pucRecvBuff);
		if (pvPacket == NULL)
			return WLAN_STATUS_RESOURCES;

		prSwRfb->pvPacket = pvPacket;
		prSwRfb->pucRecvBuff = (void *) pucRecvBuff;
	} else {
		kalMemZero(((uint8_t *) prSwRfb + OFFSET_OF(struct SW_RFB,
				prRxStatus)),
			   (sizeof(struct SW_RFB) - OFFSET_OF(struct SW_RFB,
					   prRxStatus)));
	}

	prSwRfb->prRxStatus = prSwRfb->pucRecvBuff;

	return WLAN_STATUS_SUCCESS;

}				/* end of nicRxSetupRFB() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called to put a RFB back onto the "RFB with Buffer"
 *        list or "RFB without buffer" list according to pvPacket.
 *
 * @param prAdapter      Pointer to the Adapter structure.
 * @param prSwRfb          Pointer to the RFB
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxReturnRFB(IN struct ADAPTER *prAdapter,
		    IN struct SW_RFB *prSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	struct QUE_ENTRY *prQueEntry;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRxCtrl = &prAdapter->rRxCtrl;
	prQueEntry = &prSwRfb->rQueEntry;

	ASSERT(prQueEntry);

	/* The processing on this RFB is done,
	 * so put it back on the tail of our list
	 */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	if (prSwRfb->pvPacket) {
		/* QUEUE_INSERT_TAIL */
		QUEUE_INSERT_TAIL(&prRxCtrl->rFreeSwRfbList, prQueEntry);
		if (prAdapter->u4NoMoreRfb != 0) {
			DBGLOG_LIMITED(RX, INFO,
				"Free rfb and set IntEvent!!!!!\n");
			kalSetDrvIntEvent(prAdapter->prGlueInfo);
		}
	} else {
		/* QUEUE_INSERT_TAIL */
		QUEUE_INSERT_TAIL(&prRxCtrl->rIndicatedRfbList, prQueEntry);
	}
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	/* Trigger Rx if there are free SwRfb */
	if (halIsPendingRx(prAdapter)
	    && (prRxCtrl->rFreeSwRfbList.u4NumElem > 0))
		kalSetIntEvent(prAdapter->prGlueInfo);
}				/* end of nicRxReturnRFB() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Process rx interrupt. When the rx
 *        Interrupt is asserted, it means there are frames in queue.
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicProcessRxInterrupt(IN struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);
	prAdapter->prGlueInfo->IsrRxCnt++;

	if (halIsHifStateSuspend(prAdapter)) {
		DBGLOG(RX, WARN, "suspend RX INT\n");
	}

	/* SER break point */
	if (nicSerIsRxStop(prAdapter)) {
		/* Skip following Rx handling */
		return;
	}

	halProcessRxInterrupt(prAdapter);

#if CFG_SUPPORT_MULTITHREAD
	kalSetRxProcessEvent(prAdapter->prGlueInfo);
#else
	nicRxProcessRFBs(prAdapter);
#endif

	return;

}				/* end of nicProcessRxInterrupt() */

#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
 * @brief Used to update IP/TCP/UDP checksum statistics of RX Module.
 *
 * @param prAdapter  Pointer to the Adapter structure.
 * @param aeCSUM     The array of checksum result.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxUpdateCSUMStatistics(IN struct ADAPTER *
	prAdapter, IN const enum ENUM_CSUM_RESULT aeCSUM[]) {
	struct RX_CTRL *prRxCtrl;

	ASSERT(prAdapter);
	ASSERT(aeCSUM);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS)
	    || (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)) {
		/* count success num */
		RX_INC_CNT(prRxCtrl, RX_CSUM_IP_SUCCESS_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_FAILED)
		   || (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_FAILED)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_IP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_NONE)
		   && (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_NONE)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	} else {
		ASSERT(0);
	}

	if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_SUCCESS) {
		/* count success num */
		RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_FAILED) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_FAILED_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_SUCCESS) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_FAILED) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_NONE)
		   && (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_NONE)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	} else {
		ASSERT(0);
	}

}				/* end of nicRxUpdateCSUMStatistics() */
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to query current status of RX Module.
 *
 * @param prAdapter      Pointer to the Adapter structure.
 * @param pucBuffer      Pointer to the message buffer.
 * @param pu4Count      Pointer to the buffer of message length count.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxQueryStatus(IN struct ADAPTER *prAdapter,
		      IN uint8_t *pucBuffer, OUT uint32_t *pu4Count)
{
	struct RX_CTRL *prRxCtrl;
	uint8_t *pucCurrBuf = pucBuffer;
	uint32_t u4CurrCount;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);
	ASSERT(pu4Count);

#define SPRINTF_RX_QSTATUS(arg) \
	{ \
		u4CurrCount = \
			kalScnprintf(pucCurrBuf, *pu4Count, PRINTF_ARG arg); \
		pucCurrBuf += (uint8_t)u4CurrCount; \
		*pu4Count -= u4CurrCount; \
	}


	SPRINTF_RX_QSTATUS(("\n\nRX CTRL STATUS:"));
	SPRINTF_RX_QSTATUS(("\n==============="));
	SPRINTF_RX_QSTATUS(("\nFREE RFB w/i BUF LIST :%9u",
			    prRxCtrl->rFreeSwRfbList.u4NumElem));
	SPRINTF_RX_QSTATUS(("\nFREE RFB w/o BUF LIST :%9u",
			    prRxCtrl->rIndicatedRfbList.u4NumElem));
	SPRINTF_RX_QSTATUS(("\nRECEIVED RFB LIST     :%9u",
			    prRxCtrl->rReceivedRfbList.u4NumElem));

	SPRINTF_RX_QSTATUS(("\n\n"));

	/* *pu4Count = (UINT_32)((UINT_32)pucCurrBuf - (UINT_32)pucBuffer); */

}				/* end of nicRxQueryStatus() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Clear RX related counters
 *
 * @param prAdapter Pointer of Adapter Data Structure
 *
 * @return - (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxClearStatistics(IN struct ADAPTER *prAdapter)
{
	struct RX_CTRL *prRxCtrl;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	RX_RESET_ALL_CNTS(prRxCtrl);

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to query current statistics of RX Module.
 *
 * @param prAdapter      Pointer to the Adapter structure.
 * @param pucBuffer      Pointer to the message buffer.
 * @param pu4Count       Pointer to the buffer of message length count.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxQueryStatistics(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucBuffer, OUT uint32_t *pu4Count)
{
	struct RX_CTRL *prRxCtrl;
	uint8_t *pucCurrBuf = pucBuffer;
	uint32_t u4CurrCount;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);
	ASSERT(pu4Count);

#define SPRINTF_RX_COUNTER(eCounter) \
	{ \
		u4CurrCount = kalScnprintf(pucCurrBuf, *pu4Count, \
			"%-30s : %u\n", #eCounter, \
			(uint32_t)prRxCtrl->au8Statistics[eCounter]); \
		pucCurrBuf += (uint8_t)u4CurrCount; \
		*pu4Count -= u4CurrCount; \
	}

	SPRINTF_RX_COUNTER(RX_MPDU_TOTAL_COUNT);
	SPRINTF_RX_COUNTER(RX_SIZE_ERR_DROP_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_INDICATION_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_RETURNED_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_RETAINED_COUNT);

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	SPRINTF_RX_COUNTER(RX_CSUM_TCP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UDP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_IP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_TCP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UDP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_IP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	SPRINTF_RX_COUNTER(RX_IP_V6_PKT_CCOUNT);
#endif

	/* *pu4Count = (UINT_32)(pucCurrBuf - pucBuffer); */

	nicRxClearStatistics(prAdapter);

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Use to distinguish waiting pre-cal or not
 *
 * @param prAdapter pointer to the Adapter handler
 * @param pucRspBuffer pointer to the Response buffer
 *
 * @retval WLAN_STATUS_SUCCESS: Response packet has been read
 * @retval WLAN_STATUS_FAILURE: Read Response packet timeout or error occurred
 *
 */
/*----------------------------------------------------------------------------*/
inline uint32_t
nicRxWaitResponse(IN struct ADAPTER *prAdapter,
		  IN uint8_t ucPortIdx, OUT uint8_t *pucRspBuffer,
		  IN uint32_t u4MaxRespBufferLen, OUT uint32_t *pu4Length) {
	return nicRxWaitResponseByWaitingInterval(
				prAdapter, ucPortIdx,
				pucRspBuffer, u4MaxRespBufferLen,
				pu4Length, CFG_DEFAULT_SLEEP_WAITING_INTERVAL);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Read the Response data from data port
 *
 * @param prAdapter pointer to the Adapter handler
 * @param pucRspBuffer pointer to the Response buffer
 *
 * @retval WLAN_STATUS_SUCCESS: Response packet has been read
 * @retval WLAN_STATUS_FAILURE: Read Response packet timeout or error occurred
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
nicRxWaitResponseByWaitingInterval(IN struct ADAPTER *prAdapter,
		  IN uint8_t ucPortIdx, OUT uint8_t *pucRspBuffer,
		  IN uint32_t u4MaxRespBufferLen, OUT uint32_t *pu4Length,
		  IN uint32_t u4WaitingInterval) {
	struct mt66xx_chip_info *prChipInfo;
	struct WIFI_EVENT *prEvent;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	u4Status = halRxWaitResponse(prAdapter, ucPortIdx,
					pucRspBuffer, u4MaxRespBufferLen,
					pu4Length, u4WaitingInterval);
	if (u4Status == WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, TRACE,
		       "Dump Response buffer, length = %u\n", *pu4Length);
		DBGLOG_MEM8(RX, TRACE, pucRspBuffer, *pu4Length);

		prEvent = (struct WIFI_EVENT *)
			(pucRspBuffer + prChipInfo->rxd_size);

		DBGLOG(INIT, TRACE,
		       "RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
		       prEvent->ucEID, prEvent->ucSeqNum,
		       prEvent->u2PacketLength);
	} else {
		prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
		halPrintHifDbgInfo(prAdapter);
		DBGLOG(RX, ERROR, "halRxWaitResponse fail!status %X\n",
		       u4Status);
	}

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Set filter to enable Promiscuous Mode
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxEnablePromiscuousMode(IN struct ADAPTER *
				prAdapter) {
	ASSERT(prAdapter);

}				/* end of nicRxEnablePromiscuousMode() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Set filter to disable Promiscuous Mode
 *
 * @param prAdapter  Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void nicRxDisablePromiscuousMode(IN struct ADAPTER *
				 prAdapter) {
	ASSERT(prAdapter);

}				/* end of nicRxDisablePromiscuousMode() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief this function flushes all packets queued in reordering module
 *
 * @param prAdapter              Pointer to the Adapter structure.
 *
 * @retval WLAN_STATUS_SUCCESS   Flushed successfully
 */
/*----------------------------------------------------------------------------*/
uint32_t nicRxFlush(IN struct ADAPTER *prAdapter)
{
	struct SW_RFB *prSwRfb;

	ASSERT(prAdapter);
	prSwRfb = qmFlushRxQueues(prAdapter);
	if (prSwRfb != NULL) {
		do {
			struct SW_RFB *prNextSwRfb;

			/* save next first */
			prNextSwRfb = (struct SW_RFB *) QUEUE_GET_NEXT_ENTRY((
						struct QUE_ENTRY *) prSwRfb);

			/* free */
			nicRxReturnRFB(prAdapter, prSwRfb);

			prSwRfb = prNextSwRfb;
		} while (prSwRfb);
	}

	return WLAN_STATUS_SUCCESS;
}

uint8_t nicIsActionFrameValid(IN struct SW_RFB *prSwRfb)
{
	struct WLAN_ACTION_FRAME *prActFrame;
	uint16_t u2ActionIndex = 0, u2ExpectedLen = 0;
	uint32_t u4Idx, u4Size;

	if (prSwRfb->u2PacketLen < sizeof(struct WLAN_ACTION_FRAME) - 1)
		return FALSE;
	prActFrame = (struct WLAN_ACTION_FRAME *) prSwRfb->pvHeader;

	DBGLOG(RSN, TRACE, "Action frame category=%d action=%d\n",
	       prActFrame->ucCategory, prActFrame->ucAction);

	u2ActionIndex = prActFrame->ucCategory | prActFrame->ucAction << 8;
	u4Size = sizeof(arActionFrameReservedLen) /
		 sizeof(struct ACTION_FRAME_SIZE_MAP);
	for (u4Idx = 0; u4Idx < u4Size; u4Idx++) {
		if (u2ActionIndex == arActionFrameReservedLen[u4Idx].u2Index) {
			u2ExpectedLen = (uint16_t)
				arActionFrameReservedLen[u4Idx].len;
			DBGLOG(RSN, LOUD,
				"Found expected len of incoming action frame:%d\n",
				u2ExpectedLen);
			break;
		}
	}
	if (u2ExpectedLen != 0 && prSwRfb->u2PacketLen < u2ExpectedLen) {
		DBGLOG(RSN, INFO,
			"Received an abnormal action frame: packet len/expected len:%d/%d\n",
			prSwRfb->u2PacketLen, u2ExpectedLen);
		return FALSE;
	}
	return TRUE;
}

#if CFG_SUPPORT_NAN
uint32_t nicRxNANPMFCheck(struct ADAPTER *prAdapter,
		 struct BSS_INFO *prBssInfo, struct SW_RFB *prSwRfb)
{
	struct _NAN_ACTION_FRAME_T *prActionFrame = NULL;

	if (!prSwRfb) {
		DBGLOG(NAN, ERROR, "prSwRfb error!\n");
		return WLAN_STATUS_FAILURE;
	}

	prActionFrame = (struct _NAN_ACTION_FRAME_T *)prSwRfb->pvHeader;

	if (prAdapter->rWifiVar.fgNoPmf)
		return WLAN_STATUS_SUCCESS;

	if (prBssInfo != NULL) {
		if (prBssInfo->eNetworkType == NETWORK_TYPE_NAN) {
			if (prSwRfb->prStaRec->fgIsTxKeyReady == TRUE) {
				/* NAN Todo: Not HW_MAC_RX_DESC here */
#if (defined CFG_SUPPORT_CONNAC3X && CFG_SUPPORT_CONNAC3X == 1)
				if (
				HAL_MAC_CONNAC3X_RX_STATUS_IS_CIPHER_MISMATCH(
				(struct HW_MAC_CONNAC3X_RX_DESC *)prSwRfb
						->prRxStatus) == TRUE) {
#elif (CFG_SUPPORT_CONNAC2X == 1)
				if (HAL_MAC_CONNAC2X_RX_STATUS_IS_CIPHER_MISMATCH(
					(struct HW_MAC_CONNAC2X_RX_DESC *)prSwRfb
							->prRxStatus) == TRUE) {
#else
				if (HAL_RX_STATUS_IS_CIPHER_MISMATCH(
					(struct HW_MAC_RX_DESC *)prSwRfb
							->prRxStatus) == TRUE) {
#endif
					DBGLOG(NAN, INFO,
					       "[PMF] Rx NON-PROTECT NAF, StaIdx:%d, Wtbl:%d\n",
					       prSwRfb->prStaRec->ucIndex,
					       prSwRfb->ucWlanIdx);
					DBGLOG(NAN, INFO,
					       "Src=>%02x:%02x:%02x:%02x:%02x:%02x, OUISubtype:%d\n",
					       prActionFrame->aucSrcAddr[0],
					       prActionFrame->aucSrcAddr[1],
					       prActionFrame->aucSrcAddr[2],
					       prActionFrame->aucSrcAddr[3],
					       prActionFrame->aucSrcAddr[4],
					       prActionFrame->aucSrcAddr[5],
					       prActionFrame->ucOUISubtype);
					return WLAN_STATUS_FAILURE;
				}
			}
		}
	}
	return WLAN_STATUS_SUCCESS;
}

uint32_t nicRxProcessNanPubActionFrame(struct ADAPTER *prAdapter,
			      struct SW_RFB *prSwRfb)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _NAN_ACTION_FRAME_T *prActionFrame = NULL;
	uint8_t ucOuiType;
	uint8_t ucOuiSubtype;
	struct BSS_INFO *prBssInfo = NULL;

	if (!prSwRfb) {
		DBGLOG(NAN, ERROR, "prSwRfb error!\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(NAN, LOUD, "NAN RX ACTION FRAME PROCESSING\n");
	prActionFrame = (struct _NAN_ACTION_FRAME_T *)prSwRfb->pvHeader;
	if (!IS_WFA_SPECIFIC_OUI(prActionFrame->aucOUI))
		return WLAN_STATUS_INVALID_DATA;

	ucOuiType = prActionFrame->ucOUItype;

	if (prSwRfb->prStaRec && (ucOuiType == VENDOR_OUI_TYPE_NAN_NAF)) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(
			prAdapter, prSwRfb->prStaRec->ucBssIndex);
		if (nicRxNANPMFCheck(prAdapter, prBssInfo, prSwRfb) ==
		    WLAN_STATUS_FAILURE)
			return WLAN_STATUS_FAILURE;
	}

	if (ucOuiType == VENDOR_OUI_TYPE_NAN_NAF ||
	    ucOuiType == VENDOR_OUI_TYPE_NAN_SDF) {
		ucOuiSubtype = prActionFrame->ucOUISubtype;
		DBGLOG(NAN, INFO,
		       "Rx NAN Pub Action, StaIdx:%d, Wtbl:%d, Key:%d, OUISubtype:%d\n",
		       prSwRfb->ucStaRecIdx, prSwRfb->ucWlanIdx,
		       (prSwRfb->prStaRec ? prSwRfb->prStaRec->fgIsTxKeyReady
					  : 0),
		       ucOuiSubtype);
		DBGLOG(NAN, INFO, "Src=>%02x:%02x:%02x:%02x:%02x:%02x\n",
		       prActionFrame->aucSrcAddr[0],
		       prActionFrame->aucSrcAddr[1],
		       prActionFrame->aucSrcAddr[2],
		       prActionFrame->aucSrcAddr[3],
		       prActionFrame->aucSrcAddr[4],
		       prActionFrame->aucSrcAddr[5]);
		DBGLOG(NAN, INFO, "Dest=>%02x:%02x:%02x:%02x:%02x:%02x\n",
		       prActionFrame->aucDestAddr[0],
		       prActionFrame->aucDestAddr[1],
		       prActionFrame->aucDestAddr[2],
		       prActionFrame->aucDestAddr[3],
		       prActionFrame->aucDestAddr[4],
		       prActionFrame->aucDestAddr[5]);

		switch (ucOuiSubtype) {
		case NAN_ACTION_RANGING_REQUEST:
			rWlanStatus = nanRangingRequestRx(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_RANGING_RESPONSE:
			rWlanStatus = nanRangingResponseRx(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_RANGING_TERMINATION:
			rWlanStatus =
				nanRangingTerminationRx(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_RANGING_REPORT:
			rWlanStatus = nanRangingReportRx(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_DATA_PATH_REQUEST:
			rWlanStatus =
				nanNdpProcessDataRequest(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_DATA_PATH_RESPONSE:
			rWlanStatus =
				nanNdpProcessDataResponse(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_DATA_PATH_CONFIRM:
			rWlanStatus =
				nanNdpProcessDataConfirm(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_DATA_PATH_KEY_INSTALLMENT:
			rWlanStatus =
				nanNdpProcessDataKeyInstall(prAdapter, prSwRfb);
			break;
		case NAN_ACTION_DATA_PATH_TERMINATION:
			rWlanStatus = nanNdpProcessDataTermination(prAdapter,
								   prSwRfb);
			break;
		case NAN_ACTION_SCHEDULE_REQUEST:
			rWlanStatus = nanNdlProcessScheduleRequest(prAdapter,
								   prSwRfb);
			break;
		case NAN_ACTION_SCHEDULE_RESPONSE:
			rWlanStatus = nanNdlProcessScheduleResponse(prAdapter,
								    prSwRfb);
			break;
		case NAN_ACTION_SCHEDULE_CONFIRM:
			rWlanStatus = nanNdlProcessScheduleConfirm(prAdapter,
								   prSwRfb);
			break;
		case NAN_ACTION_SCHEDULE_UPDATE_NOTIFICATION:
			rWlanStatus = nanNdlProcessScheduleUpdateNotification(
				prAdapter, prSwRfb);
			break;
		default:
			break;
		}
	}
	return rWlanStatus;
}
#endif

static u_int8_t nicIsUnprotectedRobustActionFrame(struct ADAPTER *prAdapter,
						  struct SW_RFB *prSwRfb)
{
#if CFG_SUPPORT_802_11W
	u_int8_t fgRobustAction;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct WLAN_ACTION_FRAME *prActFrame = prSwRfb->pvHeader;
	struct BSS_INFO *prBssInfo = NULL;

	fgRobustAction = secIsRobustActionFrame(prAdapter, prSwRfb->pvHeader);

	if (!fgRobustAction)
		return FALSE;
	if (!prSwRfb->prStaRec)
		return FALSE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					prSwRfb->prStaRec->ucBssIndex);
	if (prBssInfo && prBssInfo->eNetworkType != NETWORK_TYPE_AIS)
		return FALSE;

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter,
					prSwRfb->prStaRec->ucBssIndex);

	if (!prAisSpecBssInfo->fgMgmtProtection)
		return FALSE;

	if (prActFrame->u2FrameCtrl & MASK_FC_PROTECTED_FRAME)
		return FALSE;

#if CFG_WIFI_SW_CIPHER_MISMATCH
	if (!prSwRfb->fgIsCipherMS)
#else
	if (prSwRfb->ucSecMode != CIPHER_SUITE_CCMP)
#endif
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param
 *
 * @retval
 */
/*----------------------------------------------------------------------------*/
uint32_t nicRxProcessActionFrame(IN struct ADAPTER *
				 prAdapter, IN struct SW_RFB *prSwRfb) {
	struct WLAN_ACTION_FRAME *prActFrame;
	struct BSS_INFO *prBssInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	DBGLOG(RSN, TRACE, "[Rx] nicRxProcessActionFrame\n");

	if (!nicIsActionFrameValid(prSwRfb))
		return WLAN_STATUS_INVALID_PACKET;
	prActFrame = (struct WLAN_ACTION_FRAME *) prSwRfb->pvHeader;

	if (nicIsUnprotectedRobustActionFrame(prAdapter, prSwRfb)) {
		DBGLOG(RSN, INFO,
		       "[MFP] Not handle and drop un-protected robust action frame %x %x!!\n",
		       prSwRfb->ucWlanIdx, prSwRfb->ucSecMode);
		return WLAN_STATUS_INVALID_PACKET;
	}

	DBGLOG(RSN, INFO, "[Rx]RobustAction %x %x\n",
	       prSwRfb->ucWlanIdx, prSwRfb->ucSecMode);

	if (prSwRfb->prStaRec)
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prSwRfb->prStaRec->ucBssIndex);

	switch (prActFrame->ucCategory) {
#if CFG_M0VE_BA_TO_DRIVER
	case CATEGORY_BLOCK_ACK_ACTION:
		DBGLOG(RX, WARN,
		       "[Puff][%s] Rx CATEGORY_BLOCK_ACK_ACTION\n", __func__);

		if (prSwRfb->prStaRec)
			mqmHandleBaActionFrame(prAdapter, prSwRfb);

		break;
#endif

	case CATEGORY_QOS_ACTION:
		DBGLOG(RX, INFO, "received dscp action frame: %d\n",
		       __LINE__);
		handleQosMapConf(prAdapter, prSwRfb);
		break;

	case CATEGORY_PUBLIC_ACTION:
		aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			rlmProcessPublicAction(prAdapter, prSwRfb);
			if (prBssInfo)
				p2pFuncValidateRxActionFrame(prAdapter, prSwRfb,
					(prBssInfo->ucBssIndex ==
					prAdapter->ucP2PDevBssIdx),
					(uint8_t) prBssInfo->u4PrivateData);
			else
				p2pFuncValidateRxActionFrame(prAdapter,
					prSwRfb, TRUE, 0);
		}
#endif
#if CFG_SUPPORT_NAN
		if (prAdapter->fgIsNANRegistered)
			nicRxProcessNanPubActionFrame(prAdapter, prSwRfb);
#endif
		break;

	case CATEGORY_HT_ACTION:
		rlmProcessHtAction(prAdapter, prSwRfb);
		break;
	case CATEGORY_VENDOR_SPECIFIC_PROTECTED_ACTION:
#if CFG_FAST_PATH_SUPPORT
		fpProcessVendorSpecProtectedFrame(prAdapter, prSwRfb);
#endif
		aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
		break;

	case CATEGORY_VENDOR_SPECIFIC_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			if (prBssInfo)
				p2pFuncValidateRxActionFrame(prAdapter, prSwRfb,
					(prBssInfo->ucBssIndex ==
					prAdapter->ucP2PDevBssIdx),
					(uint8_t) prBssInfo->u4PrivateData);
			else
				p2pFuncValidateRxActionFrame(prAdapter,
					prSwRfb, TRUE, 0);
		}
#endif
#if CFG_SUPPORT_NCHO
		if (prBssInfo && prBssInfo->eNetworkType == NETWORK_TYPE_AIS) {
			if (prAdapter->rNchoInfo.fgNCHOEnabled == TRUE
			    && prAdapter->rNchoInfo.u4WesMode == TRUE) {
				aisFuncValidateRxActionFrame(prAdapter,
					prSwRfb);
				DBGLOG(INIT, INFO,
				       "NCHO CATEGORY_VENDOR_SPECIFIC_ACTION\n");
			}
		}
#endif
		break;

#if CFG_SUPPORT_802_11W
	case CATEGORY_SA_QUERY_ACTION: {
		struct BSS_INFO *prBssInfo;

		if (prSwRfb->prStaRec) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
				prSwRfb->prStaRec->ucBssIndex);
			ASSERT(prBssInfo);
			if ((prBssInfo->eNetworkType == NETWORK_TYPE_AIS) &&
				aisGetAisSpecBssInfo(prAdapter,
				prSwRfb->prStaRec->ucBssIndex)->
			    fgMgmtProtection /* Use MFP */) {
				/* MFP test plan 5.3.3.4 */
				rsnSaQueryAction(prAdapter, prSwRfb);
			} else if ((prBssInfo->eNetworkType ==
					NETWORK_TYPE_P2P) &&
				  (prBssInfo->eCurrentOPMode ==
					OP_MODE_ACCESS_POINT)) {
				/* AP PMF */
				DBGLOG(RSN, INFO,
					"[Rx] nicRx AP PMF SAQ action\n");
				if (rsnCheckBipKeyInstalled(prAdapter,
						prSwRfb->prStaRec)) {
					/* MFP test plan 4.3.3.4 */
					rsnApSaQueryAction(prAdapter, prSwRfb);
				}
			}
		}
	}
	break;
#endif

	case CATEGORY_WNM_ACTION: {
		if (prSwRfb->prStaRec &&
		    GET_BSS_INFO_BY_INDEX(prAdapter,
					  prSwRfb->prStaRec->ucBssIndex)
				    ->eNetworkType == NETWORK_TYPE_AIS) {
			DBGLOG(RX, INFO, "WNM action frame: %d\n", __LINE__);
			wnmWNMAction(prAdapter, prSwRfb);
		} else
			DBGLOG(RX, INFO, "WNM action frame: %d\n", __LINE__);
	}
	break;

#if CFG_SUPPORT_DFS
	case CATEGORY_SPEC_MGT: {
		if (prAdapter->fgEnable5GBand) {
			DBGLOG(RLM, INFO,
			       "[CSA]nicRxProcessActionFrame\n");
			rlmProcessSpecMgtAction(prAdapter, prSwRfb);
		}
	}
	break;
#endif

#if CFG_SUPPORT_802_11AC
	case CATEGORY_VHT_ACTION:
		rlmProcessVhtAction(prAdapter, prSwRfb);
		break;
#endif

#if (CFG_SUPPORT_TWT == 1)
	case CATEGORY_S1G_ACTION:
		twtProcessS1GAction(prAdapter, prSwRfb);
		break;
#endif

#if CFG_SUPPORT_802_11K
	case CATEGORY_RM_ACTION:
		switch (prActFrame->ucAction) {
		case RM_ACTION_RM_REQUEST:
#if CFG_SUPPORT_RM_BEACON_REPORT_BY_SUPPLICANT
			/* handle RM beacon request by supplicant */
			if (prAdapter->prAisBssInfo &&
					prSwRfb->prStaRec
					&& prSwRfb->prStaRec->ucBssIndex ==
					prAdapter->prAisBssInfo->ucBssIndex)
				aisFuncValidateRxActionFrame(prAdapter,
					prSwRfb);
#else
			rrmProcessRadioMeasurementRequest(prAdapter, prSwRfb);
#endif
			break;

		case RM_ACTION_REIGHBOR_RESPONSE:
			rrmProcessNeighborReportResonse(prAdapter, prActFrame,
							prSwRfb);
			break;
		}
		break;
#endif

	case CATEGORY_WME_MGT_NOTIFICATION:
		wmmParseQosAction(prAdapter, prSwRfb);
		break;

	case CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION:
		aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
#if CFG_SUPPORT_NAN
		if (prAdapter->fgIsNANRegistered)
			nicRxProcessNanPubActionFrame(prAdapter, prSwRfb);
#endif
		break;

	case CATEGORY_ROBUST_AV_STREAMING_ACTION:
#if CFG_FAST_PATH_SUPPORT
		mscsProcessRobustAVStreaming(prAdapter, prSwRfb);
#endif
		aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
		break;

	default:
		break;
	}			/* end of switch case */

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param
 *
 * @retval
 */
/*----------------------------------------------------------------------------*/
uint8_t nicRxGetRcpiValueFromRxv(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucRcpiMode,
	IN struct SW_RFB *prSwRfb)
{
	struct mt66xx_chip_info *prChipInfo;

	prChipInfo = prAdapter->chip_info;
	if (prChipInfo->asicRxGetRcpiValueFromRxv)
		return prChipInfo->asicRxGetRcpiValueFromRxv(
				ucRcpiMode, prSwRfb);
	else {
		DBGLOG(RX, ERROR, "%s: no asicRxGetRcpiValueFromRxv ??\n",
			__func__);
		return 0;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param
 *
 * @retval
 */
/*----------------------------------------------------------------------------*/
int32_t nicRxGetLastRxRssi(struct ADAPTER *prAdapter, IN char *pcCommand,
				 IN int i4TotalLen, IN uint8_t ucWlanIdx)
{
	int32_t i4RSSI0 = 0, i4RSSI1 = 0, i4RSSI2 = 0, i4RSSI3 = 0;
	int32_t i4BytesWritten = 0;
	uint32_t u4RxV3 = 0;
	uint8_t ucStaIdx;
	struct CHIP_DBG_OPS *prChipDbg;

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) ==
	    WLAN_STATUS_SUCCESS) {
		u4RxV3 = prAdapter->arStaRec[ucStaIdx].au4RxV[3];
		DBGLOG(REQ, LOUD, "****** RX Vector3 = 0x%08x ******\n",
		       u4RxV3);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last RX RSSI", " = NOT SUPPORT");
		return i4BytesWritten;
	}

	prChipDbg = prAdapter->chip_info->prDebugOps;

	if (prChipDbg && prChipDbg->show_rx_rssi_info) {
		i4BytesWritten = prChipDbg->show_rx_rssi_info(
				prAdapter,
				pcCommand,
				i4TotalLen,
				ucStaIdx);
		return i4BytesWritten;
	}

	i4RSSI0 = RCPI_TO_dBm((u4RxV3 & RX_VT_RCPI0_MASK) >>
			      RX_VT_RCPI0_OFFSET);
	i4RSSI1 = RCPI_TO_dBm((u4RxV3 & RX_VT_RCPI1_MASK) >>
			      RX_VT_RCPI1_OFFSET);

	if (prAdapter->rWifiVar.ucNSS > 2) {
		i4RSSI2 = RCPI_TO_dBm((u4RxV3 & RX_VT_RCPI2_MASK) >>
				      RX_VT_RCPI2_OFFSET);
		i4RSSI3 = RCPI_TO_dBm((u4RxV3 & RX_VT_RCPI3_MASK) >>
				      RX_VT_RCPI3_OFFSET);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d %d %d %d\n",
			"Last RX Data RSSI", " = ",
			i4RSSI0, i4RSSI1, i4RSSI2, i4RSSI3);
	} else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d %d\n",
			"Last RX Data RSSI", " = ", i4RSSI0, i4RSSI1);

	return i4BytesWritten;
}

/**
 * For MLO, it should read prSwRfb->ucHwBandIdx to match the RX link.
 */
static void updateLinkStatsMpduAc(struct ADAPTER *prAdapter,
				struct SW_RFB *prSwRfb)
{
#if CFG_SUPPORT_LLS
	static const uint8_t Tid2LinkStatsAc[] = {
		STATS_LLS_WIFI_AC_BE,
		STATS_LLS_WIFI_AC_BK,
		STATS_LLS_WIFI_AC_BK,
		STATS_LLS_WIFI_AC_BE,
		STATS_LLS_WIFI_AC_VI,
		STATS_LLS_WIFI_AC_VI,
		STATS_LLS_WIFI_AC_VO,
		STATS_LLS_WIFI_AC_VO,
	};
	uint8_t ac;
	uint8_t ucBssIdx = GLUE_GET_PKT_BSS_IDX(prSwRfb->pvPacket);
	struct BSS_INFO *prBssInfo;

	ac = Tid2LinkStatsAc[(uint8_t)(prSwRfb->ucTid & 0x7U)];
	if (prSwRfb->ucPayloadFormat == RX_PAYLOAD_FORMAT_MSDU ||
	    prSwRfb->ucPayloadFormat == RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
		if (!prBssInfo)
			return;

#if (defined CFG_SUPPORT_802_11BE_MLO && CFG_SUPPORT_802_11BE_MLO == 1)
		if (prBssInfo->eHwBandIdx != prSwRfb->ucHwBandIdx) {
			uint8_t ucHwBandIdx = prSwRfb->ucHwBandIdx;
			uint8_t i;

			/* find the BSS by matching the band index */
			/* TODO: performance? */
			for (i = 0; i < MAX_BSSID_NUM; i++) {
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);
				if (prBssInfo &&
				    prBssInfo->eHwBandIdx == ucHwBandIdx)
					break;
			}

			if (!prBssInfo)
				return;
		}
#endif
		prBssInfo->u4RxMpduAc[ac]++;
	}
#endif
}

void nicRxProcessRxvLinkStats(struct ADAPTER *prAdapter,
			      struct SW_RFB *prRetSwRfb, uint32_t *pu4RxV)
{
#if CFG_SUPPORT_LLS
	struct CHIP_DBG_OPS *prChipDbg;

	prChipDbg = prAdapter->chip_info->prDebugOps;
	if (prChipDbg && prChipDbg->get_rx_link_stats)
		prChipDbg->get_rx_link_stats(prAdapter, prRetSwRfb, pu4RxV);

	updateLinkStatsMpduAc(prAdapter, prRetSwRfb);
#endif
}
