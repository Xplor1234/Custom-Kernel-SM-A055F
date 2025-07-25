// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   mddp.c
*    \brief  Main routines for modem direct path handling
*
*    This file contains the support routines of modem direct path operation.
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#if CFG_MTK_MDDP_SUPPORT

#include "gl_os.h"
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 0)
#include "mddp_export.h"
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP == 0 */
#include <linux/signal.h>
#include <linux/sched/signal.h>
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP == 0 */
#include "mddp.h"
#if CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP
#include "cnm_mem.h"
#endif /* CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP */

#if CFG_MTK_CCCI_SUPPORT
#include "ccci_fsm.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MDDP_HIF_NOTIFY_MD_CRASH_2_FW	0
#define MDDP_HIF_MD_FW_OWN		1
#define MDDP_HIF_SER_RECOVERY		2
#define MDDP_HIF_TRIGGER_RESET		3
#if (CFG_PCIE_GEN_SWITCH == 1)
#define MDDP_HIF_GEN_SWITCH_END		4
#endif /* CFG_PCIE_GEN_SWITCH */
#define MDDP_HIF_MD_SER			5

#if KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE
#define MDDP_SUPPORT_NOTIFY_INFO_V1	1
#else
#define MDDP_SUPPORT_NOTIFY_INFO_V1	0
#endif

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
enum ENUM_MDDPW_DRV_INFO_STATUS {
	MDDPW_DRV_INFO_STATUS_ON_START   = 0,
	MDDPW_DRV_INFO_STATUS_ON_END     = 1,
	MDDPW_DRV_INFO_STATUS_OFF_START  = 2,
	MDDPW_DRV_INFO_STATUS_OFF_END    = 3,
	MDDPW_DRV_INFO_STATUS_ON_END_QOS = 4,
};

enum ENUM_MDDPW_MD_INFO {
	MDDPW_MD_INFO_RESET_IND     = 1,
	MDDPW_MD_INFO_DRV_EXCEPTION = 2,
	MDDPW_MD_EVENT_NOTIFY_MD_INFO_EMI = 3,
	MDDPW_MD_EVENT_COMMUNICATION = 4,
	MDDPW_MD_INFO_WIFI_OFF_CONFIRM = 5,
	MDDPW_MD_INFO_DRVOWN_RELEASE = 6,
	MDDPW_MD_INFO_TRIG_SER_REQ = 64,
	MDDPW_MD_INFO_PCIE_MMIO = 128,
	MDDPW_MD_INFO_PCIE_GENSWITCH_START = 129,
	MDDPW_MD_INFO_PCIE_GENSWITCH_END = 130,
	MDDPW_MD_INFO_PCIE_GENSWITCH_BYPASS_START = 131,
	MDDPW_MD_INFO_PCIE_GENSWITCH_BYPASS_END = 132,
};

enum wsvc_drv_info_id {
	WSVC_DRVINFO_NONE                         = 0,
	WSVC_DRVINFO_LOCAL_MAC                    = 1,
	WSVC_DRVINFO_WIFI_ONOFF                   = 2,
	WSVC_DRVINFO_TXD_TEMPLATE                 = 3,
	WSVC_DRVINFO_QUERY_MD_INFO                = 4,
	WSVC_DRVINFO_DRVOWN_TIME_SET              = 5,
	WSVC_DRVINFO_WIFI_UNIFIED_CMD_VER         = 6,
	WSVC_DRVINFO_PCIE_L_LOCK_SUCCESS          = 7,
	WSVC_DRVINFO_PCIE_L_UNLOCK_SUCCESS        = 8,
	WSVC_DRVINFO_CHECK_SER                    = 9,
	WSVC_DRVINFO_INVALID_ID                   = 10,
	MACOD_DRVINFO_TRIG_SER_RSP                = 64,
	MACOD_DRVINFO_TRIG_SER_SUPP               = 65,
	WFPM_DRVINFO_PCIE_MMIO                    = 128,
	WFPM_DRVINFO_PCIE_GENSWITCH_START         = 129,
	WFPM_DRVINFO_PCIE_GENSWITCH_END           = 130,
	WFPM_DRVINFO_PCIE_GENSWITCH_BYPASS_START  = 131,
	WFPM_DRVINFO_PCIE_GENSWITCH_BYPASS_END    = 132,
};

/* MDDPW_MD_INFO_DRV_EXCEPTION */
struct wsvc_md_event_exception_t {
	uint32_t u4RstReason;
	uint32_t u4RstFlag;
	uint32_t u4Line;
	char pucFuncName[64];
};

enum EMUM_MD_NOTIFY_REASON_TYPE_T {
	MD_INFORMATION_DUMP = 1,
	MD_DRV_OWN_FAIL,
	MD_INIT_FAIL,
	MD_STATE_ABNORMAL,
	MD_TX_DATA_TIMEOUT,
	MD_TX_CMD_FAIL,
	MD_L12_DISABLE, /* 7 */
	MD_L12_ENABLE, /* 8 */
	MD_RST_WIFI_OFF, /* 9 */
	MD_SER_NO_RSP, /* 10 */
	MD_ENUM_MAX,
};

enum MD_TRIG_SER_RESULT {
	MD_TRIG_SER_SUCC = 0,
	MD_TRIG_SER_FAIL = 1,
	MD_TRIG_SER_NOT_SUPP = 2,
};

enum MD_TRIG_SER_SUPP_TYPE {
	MD_TRIG_SER_SUPP = 0,
	MD_TRIG_SER_FW_NOT_SUPP = 1,
};

/* MDDPW_MD_EVENT_COMMUNICATION */
struct wsvc_md_event_comm_t {
	uint32_t u4Reason;
	uint32_t u4RstFlag;
	uint32_t u4Line;
	uint32_t dump_payload[32];
	uint8_t  dump_size;
	char pucFuncName[64];
};

struct mddpw_drv_info_entry {
	struct QUE_ENTRY entry;
	struct mddpw_drv_info_t *info;
};

struct macod_trig_ser_rsp_t {
	uint8_t ver;
	uint8_t sn;
	uint8_t rsp;
	uint8_t rsv;
};

struct macod_trig_ser_supp_t {
	uint8_t ver;
	uint8_t supp_type;
	uint8_t rsv[2];
};

struct macod_trig_ser_req_t {
	uint8_t ver;
	uint8_t sn;
	uint8_t rsv[2];
};

struct wsv_check_ser_info_t {
	uint32_t sn;
};

struct mddpw_drv_handle_t gMddpWFunc = {
	.notify_md_info = mddpMdNotifyInfo,
};

struct mddp_drv_conf_t gMddpDrvConf = {
	.app_type = MDDP_APP_TYPE_WH,
};

struct mddp_drv_handle_t gMddpFunc = {
	.change_state = mddpChangeState,
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#define MAC_ADDR_LEN            6

struct mddp_txd_t {
	uint8_t version;
	uint8_t wlan_idx;
	uint8_t sta_idx;
	uint8_t nw_if_name[8];
	uint8_t sta_mode;
	uint8_t bss_id;
	uint8_t wmmset;
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t local_mac[MAC_ADDR_LEN];
	uint8_t txd_length;
	uint8_t txd[];
} __packed;

struct mddp_pcie_bar_info {
	uint8_t version;
	uint8_t reserved[7];
	uint64_t offset;
};

enum BOOTMODE g_wifi_boot_mode = NORMAL_BOOT;
u_int8_t g_fgMddpEnabled = TRUE;
u_int8_t g_fgMddpWifiEnabled = TRUE;
struct MDDP_SETTINGS g_rSettings;
enum ENUM_MDDPW_DRV_INFO_STATUS g_eMddpStatus;
struct mutex rMddpLock;
u_int32_t g_u4CheckSerCnt;
u_int8_t g_fgIsMdCrash;
unsigned long g_ulMddpActionFlag;
u_int32_t g_u4MddpRstFlag;
uint32_t g_u4SerSn;


struct mddpw_net_stat_ext_t stats;
#if CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP
struct wsvc_stat_lls_report_t cur_lls_stats;
struct wsvc_stat_lls_report_t base_lls_stats;
struct wsvc_stat_lls_report_t todo_lls_stats;
u_int8_t isMdResetSinceLastQuery;
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static int32_t wait_for_md_on_complete(void);
static int32_t wait_for_md_off_complete(void);
static void save_mddp_stats(void);
#if CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP
static void save_mddp_lls_stats(void);
#endif
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
static void mddpRdCCCI(struct MDDP_SETTINGS *prSettings, uint32_t *pu4Val);
static void mddpMdStateChangeReleaseDrvOwn(void);
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

static void mddpRdFunc(struct MDDP_SETTINGS *prSettings, uint32_t *pu4Val)
{
	if (!prSettings->u4SyncAddr)
		return;

	wf_ioremap_read(prSettings->u4SyncAddr, pu4Val);
}

static void mddpSetFuncV1(struct MDDP_SETTINGS *prSettings, uint32_t u4Bit)
{
	uint32_t u4Value = 0;

	if (!prSettings->u4SyncAddr || u4Bit == 0)
		return;

	wf_ioremap_read(prSettings->u4SyncAddr, &u4Value);
	u4Value |= u4Bit;
	wf_ioremap_write(prSettings->u4SyncAddr, u4Value);
}

static void mddpClrFuncV1(struct MDDP_SETTINGS *prSettings, uint32_t u4Bit)
{
	uint32_t u4Value = 0;

	if (!prSettings->u4SyncAddr || u4Bit == 0)
		return;

	wf_ioremap_read(prSettings->u4SyncAddr, &u4Value);
	u4Value &= ~u4Bit;
	wf_ioremap_write(prSettings->u4SyncAddr, u4Value);
}

static void mddpSetFuncV2(struct MDDP_SETTINGS *prSettings, uint32_t u4Bit)
{
	if (!prSettings->u4SyncSetAddr || u4Bit == 0)
		return;

	wf_ioremap_write(prSettings->u4SyncSetAddr, u4Bit);
}

static void mddpClrFuncV2(struct MDDP_SETTINGS *prSettings, uint32_t u4Bit)
{
	if (!prSettings->u4SyncClrAddr || u4Bit == 0)
		return;

	wf_ioremap_write(prSettings->u4SyncClrAddr, u4Bit);
}

static void mddpRdFuncSHM(struct MDDP_SETTINGS *prSettings, uint32_t *pu4Val)
{
	struct mddpw_sys_stat_t *prSysStat = NULL;

	if (gMddpWFunc.get_sys_stat) {
		if (gMddpWFunc.get_sys_stat(&prSysStat)) {
			DBGLOG(NIC, ERROR, "get_sys_stat Error.\n");
			goto exit;
		}
		if (prSysStat == NULL) {
			DBGLOG(NIC, ERROR, "prSysStat Null.\n");
			goto exit;
		}
	} else {
		DBGLOG(NIC, ERROR, "get_sys_stat callback NOT exist.\n");
		goto exit;
	}

	*pu4Val = prSysStat->md_stat[0] &
		(prSettings->u4MdOnBit | prSettings->u4MdOffBit);

	if (prSysStat) {
		DBGLOG(QM, TRACE,
			"md_stat: %u, Val: %u\n",
			prSysStat->md_stat[0], *pu4Val);
	}

exit:
	return;
}

static void mddpSetFuncSHM(struct MDDP_SETTINGS *prSettings, uint32_t u4Bit)
{
	uint32_t u4Value = 0;
	struct mddpw_sys_stat_t *prSysStat = NULL;

	if (gMddpWFunc.get_sys_stat) {
		if (gMddpWFunc.get_sys_stat(&prSysStat)) {
			DBGLOG(NIC, ERROR, "get_sys_stat Error.\n");
			goto exit;
		}
		if (prSysStat == NULL) {
			DBGLOG(NIC, ERROR, "prSysStat Null.\n");
			goto exit;
		}
	} else {
		DBGLOG(NIC, ERROR, "get_sys_stat callback NOT exist.\n");
		goto exit;
	}

	if (u4Bit & MD_SHM_AP_STAT_BIT) {
		u4Value = u4Bit & ~MD_SHM_AP_STAT_BIT;
		prSysStat->ap_stat[0] |= u4Value;
	} else
		prSysStat->md_stat[0] |= u4Value;

	if (prSysStat) {
		DBGLOG(QM, TRACE,
			"u4Bit: %u, ap_stat[0]: %u, md_stat[0]: %u val: %u\n",
			u4Bit, prSysStat->ap_stat[0],
			prSysStat->md_stat[0], u4Value);
	}

exit:
	return;
}

static void mddpClrFuncSHM(struct MDDP_SETTINGS *prSettings, uint32_t u4Bit)
{
	uint32_t u4Value = 0;
	struct mddpw_sys_stat_t *prSysStat = NULL;

	if (gMddpWFunc.get_sys_stat) {
		if (gMddpWFunc.get_sys_stat(&prSysStat)) {
			DBGLOG(NIC, ERROR, "get_sys_stat Error.\n");
			goto exit;
		}
		if (prSysStat == NULL) {
			DBGLOG(NIC, ERROR, "prSysStat Null.\n");
			goto exit;
		}
	} else {
		DBGLOG(NIC, ERROR, "get_sys_stat callback NOT exist.\n");
		goto exit;
	}

	if (u4Bit & MD_SHM_AP_STAT_BIT) {
		u4Value = u4Bit & ~MD_SHM_AP_STAT_BIT;
		prSysStat->ap_stat[0] &= ~u4Value;
	} else
		prSysStat->md_stat[0] &= ~u4Value;

	if (prSysStat) {
		DBGLOG(QM, TRACE,
			"u4Bit: %u, ap_stat[%u]: %u, md_stat[%u]: %u\n",
			u4Bit, u4Value, prSysStat->ap_stat[0],
			u4Value, prSysStat->md_stat[0]);
	}

exit:
	return;
}

static int32_t mddpRegisterCb(void)
{
	int32_t ret = 0;

	switch (g_wifi_boot_mode) {
	case NORMAL_BOOT:
		g_fgMddpEnabled = TRUE;
		break;
	default:
		g_fgMddpEnabled = FALSE;
		break;
	}
	gMddpFunc.wifi_handle = &gMddpWFunc;

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	if (g_rSettings.is_port_open) {
		DBGLOG(INIT, ERROR, "port(%d) already opened!\n",
			g_rSettings.i4PortIdx);
		return ret;
	}

	g_rSettings.i4PortIdx = mtk_ccci_open_port(CCCI_PORT_NAME);
	if (g_rSettings.i4PortIdx < 0) {
		DBGLOG(INIT, ERROR, "open ccci port fail!\n");
		return ret;
	}

	g_rSettings.is_port_open = TRUE;
	DBGLOG(INIT, DEBUG, "port idx:%d\n", g_rSettings.i4PortIdx);

	gMddpWFunc.notify_drv_info = mddpDrvNotifyInfo;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	ret = mddp_drv_attach(&gMddpDrvConf, &gMddpFunc);

	DBGLOG(INIT, INFO, "mddp_drv_attach ret: %d, g_fgMddpEnabled: %d\n",
			ret, g_fgMddpEnabled);
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	kalMemZero(&stats, sizeof(struct mddpw_net_stat_ext_t));
#if CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP
	kalMemZero(&cur_lls_stats, sizeof(struct wsvc_stat_lls_report_t));
	kalMemZero(&base_lls_stats, sizeof(struct wsvc_stat_lls_report_t));
	kalMemZero(&todo_lls_stats, sizeof(struct wsvc_stat_lls_report_t));
	isMdResetSinceLastQuery = FALSE;
#endif

	return ret;
}

static void mddpUnregisterCb(void)
{
	DBGLOG(INIT, INFO, "mddp_drv_detach\n");
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	DBGLOG(INIT, INFO, "port idx:%d\n", g_rSettings.i4PortIdx);
	if (!g_rSettings.is_port_open) {
		DBGLOG(INIT, ERROR, "port didn't open!\n");
		return;
	}

	if (mtk_ccci_close_port(g_rSettings.i4PortIdx) < 0)
		DBGLOG(INIT, ERROR, "close ccci port fail!\n");

	g_rSettings.i4PortIdx = -1;
	g_rSettings.is_port_open = FALSE;
	gMddpWFunc.notify_drv_info = NULL;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	mddp_drv_detach(&gMddpDrvConf, &gMddpFunc);
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	gMddpFunc.wifi_handle = NULL;
}

int32_t mddpGetMdStats(struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate;
	struct net_device_stats *prStats;
	struct GLUE_INFO *prGlueInfo;
	struct mddpw_net_stat_ext_t mddpNetStats;
	int32_t ret;
	uint8_t i = 0;

	if (!mddpIsSupportMcifWifi() || !mddpIsSupportMddpWh())
		return 0;

	if (!gMddpWFunc.get_net_stat_ext)
		return 0;

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prDev);
	prStats = &prNetDevPrivate->stats;
	prGlueInfo = prNetDevPrivate->prGlueInfo;

	if (!prGlueInfo || (prGlueInfo->u4ReadyFlag == 0) ||
			!prGlueInfo->prAdapter)
		return 0;

	if (!prNetDevPrivate->ucMddpSupport)
		return 0;

	ret = gMddpWFunc.get_net_stat_ext(&mddpNetStats);
	if (ret != 0) {
		DBGLOG(INIT, ERROR, "get_net_stat fail, ret: %d.\n", ret);
		return 0;
	}
	for (i = 0; i < NW_IF_NUM_MAX; i++) {
		struct mddpw_net_stat_elem_ext_t *element, *prev;

		element = &mddpNetStats.ifs[0][i];
		prev = &stats.ifs[0][i];
		if (kalStrnCmp(element->nw_if_name, prDev->name,
				IFNAMSIZ) != 0)
			continue;

		prStats->rx_packets +=
			element->rx_packets + prev->rx_packets;
		prStats->tx_packets +=
			element->tx_packets + prev->tx_packets;
		prStats->rx_bytes +=
			element->rx_bytes + prev->rx_bytes;
		prStats->tx_bytes +=
			element->tx_bytes + prev->tx_bytes;
		prStats->rx_errors +=
			element->rx_errors + prev->rx_errors;
		prStats->tx_errors +=
			element->tx_errors + prev->tx_errors;
		prStats->rx_dropped +=
			element->rx_dropped + prev->rx_dropped;
		prStats->tx_dropped +=
			element->tx_dropped + prev->tx_dropped;
	}

	return 0;
}

#if CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP
int32_t mddpGetMdLlsStats(struct ADAPTER *prAdapter)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate;
	struct GLUE_INFO *prGlueInfo;
	struct STA_RECORD *prStaRec;
	struct net_device *prDev;
	uint8_t i, j, k, l;
	int32_t ret;
	uint32_t bss_num = MDDP_LLS_BSS_NUM_V1;

	if (!mddpIsSupportMcifWifi() || !mddpIsSupportMddpWh()) {
		DBGLOG(INIT, ERROR, "mddp is not supported.\n");
		return 0;
	}

	if (!prAdapter) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL\n");
		return 0;
	}

	prDev = prAdapter->prGlueInfo->prDevHandler;
	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prDev);
	prGlueInfo = prNetDevPrivate->prGlueInfo;

	if (!prGlueInfo || (prGlueInfo->u4ReadyFlag == 0) ||
			!prGlueInfo->prAdapter) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL\n");
		return 0;
	}

	if (!prNetDevPrivate->ucMddpSupport) {
		DBGLOG(INIT, ERROR,
			"prNetDevPrivate->ucMddpSupport is not supported\n");
		return 0;
	}

	if (!gMddpWFunc.get_lls_stat) {
		DBGLOG(INIT, ERROR,
			"gMddpWFunc.get_lls_stat is not supported\n");
		return 0;
	}

	kalMemZero(&cur_lls_stats, sizeof(struct wsvc_stat_lls_report_t));
	ret = gMddpWFunc.get_lls_stat(&cur_lls_stats);
	if (ret != 0) {
		DBGLOG(INIT, ERROR, "get_lls_stat fail, ret: %d.\n", ret);
		return 0;
	}

	if (cur_lls_stats.version == 0) {
		DBGLOG(INIT, ERROR, "MD is resetting.\n");
		return 0;
	} else if (cur_lls_stats.version == 1)
		bss_num = MDDP_LLS_BSS_NUM_V1;
	else if (cur_lls_stats.version == 2)
		bss_num = MDDP_LLS_BSS_NUM_V2;
	DBGLOG(INIT, DEBUG, "cur_lls_stats version: %u\n",
		cur_lls_stats.version);

	for (i = 0; i < bss_num; ++i) {
		for (j = 0; j < AC_NUM; ++j) {
			prAdapter->aprBssInfo[i]->u4RxMpduAc[j] +=
				isMdResetSinceLastQuery ?
				(cur_lls_stats.wmm_ac_stat_rx_mpdu[i][j] +
				todo_lls_stats.wmm_ac_stat_rx_mpdu[i][j]) :
				(cur_lls_stats.wmm_ac_stat_rx_mpdu[i][j] -
				base_lls_stats.wmm_ac_stat_rx_mpdu[i][j]);
			base_lls_stats.wmm_ac_stat_rx_mpdu[i][j] =
				cur_lls_stats.wmm_ac_stat_rx_mpdu[i][j];
		}
	}

	for (i = 0; i < STA_NUM; ++i) {
		struct rate_stat_rx_mpdu_t *cur, *base, *todo;

		cur = &cur_lls_stats.rate_stat_rx_mpdu[i];
		base = &base_lls_stats.rate_stat_rx_mpdu[i];
		todo = &todo_lls_stats.rate_stat_rx_mpdu[i];
		prStaRec = &prAdapter->arStaRec[i];

		for (k = 0; k < STATS_LLS_MAX_OFDM_BW_NUM; ++k) {
			for (l = 0; l < STATS_LLS_OFDM_NUM; ++l) {
				prStaRec->u4RxMpduOFDM[0][k][l] +=
					isMdResetSinceLastQuery ?
					(cur->u4RxMpduOFDM[0][k][l] +
					todo->u4RxMpduOFDM[0][k][l]) :
					(cur->u4RxMpduOFDM[0][k][l] -
					base->u4RxMpduOFDM[0][k][l]);
				base->u4RxMpduOFDM[0][k][l] =
					cur->u4RxMpduOFDM[0][k][l];
			}
		}

		for (k = 0; k < STATS_LLS_MAX_CCK_BW_NUM; ++k) {
			for (l = 0; l < STATS_LLS_CCK_NUM; ++l) {
				prStaRec->u4RxMpduCCK[0][k][l] +=
					isMdResetSinceLastQuery ?
					(cur->u4RxMpduCCK[0][k][l] +
					todo->u4RxMpduCCK[0][k][l]) :
					(cur->u4RxMpduCCK[0][k][l] -
					base->u4RxMpduCCK[0][k][l]);
				base->u4RxMpduCCK[0][k][l] =
					cur->u4RxMpduCCK[0][k][l];
			}
		}

		for (k = 0; k < STATS_LLS_MAX_HT_BW_NUM; ++k) {
			for (l = 0; l < STATS_LLS_HT_NUM; ++l) {
				prStaRec->u4RxMpduHT[0][k][l] +=
					isMdResetSinceLastQuery ?
					(cur->u4RxMpduHT[0][k][l] +
					todo->u4RxMpduHT[0][k][l]) :
					(cur->u4RxMpduHT[0][k][l] -
					base->u4RxMpduHT[0][k][l]);
				base->u4RxMpduHT[0][k][l] =
					cur->u4RxMpduHT[0][k][l];
			}
		}

		for (j = 0; j < STATS_LLS_MAX_NSS_NUM; ++j) {
			for (k = 0; k < STATS_LLS_MAX_VHT_BW_NUM; ++k) {
				for (l = 0; l < STATS_LLS_VHT_NUM; ++l) {
					prStaRec->u4RxMpduVHT[j][k][l] +=
						isMdResetSinceLastQuery ?
						(cur->u4RxMpduVHT[j][k][l] +
						todo->u4RxMpduVHT[j][k][l]) :
						(cur->u4RxMpduVHT[j][k][l] -
						base->u4RxMpduVHT[j][k][l]);
					base->u4RxMpduVHT[j][k][l] =
						cur->u4RxMpduVHT[j][k][l];
				}
			}
		}

		for (j = 0; j < STATS_LLS_MAX_NSS_NUM; ++j) {
			for (k = 0; k < STATS_LLS_MAX_HE_BW_NUM; ++k) {
				for (l = 0; l < STATS_LLS_HE_NUM; ++l) {
					prStaRec->u4RxMpduHE[j][k][l] +=
						isMdResetSinceLastQuery ?
						(cur->u4RxMpduHE[j][k][l] +
						todo->u4RxMpduHE[j][k][l]) :
						(cur->u4RxMpduHE[j][k][l] -
						base->u4RxMpduHE[j][k][l]);
					base->u4RxMpduHE[j][k][l] =
						cur->u4RxMpduHE[j][k][l];
				}
			}
		}

		for (j = 0; j < STATS_LLS_MAX_NSS_NUM; ++j) {
			for (k = 0; k < STATS_LLS_MAX_EHT_BW_NUM; ++k) {
				for (l = 0; l < STATS_LLS_EHT_NUM; ++l) {
					prStaRec->u4RxMpduEHT[j][k][l] +=
						isMdResetSinceLastQuery ?
						(cur->u4RxMpduEHT[j][k][l] +
						todo->u4RxMpduEHT[j][k][l]) :
						(cur->u4RxMpduEHT[j][k][l] -
						base->u4RxMpduEHT[j][k][l]);
					base->u4RxMpduEHT[j][k][l] =
						cur->u4RxMpduEHT[j][k][l];
				}
			}
		}
	}

	isMdResetSinceLastQuery = FALSE;
	kalMemZero(&todo_lls_stats,
		sizeof(struct wsvc_stat_lls_report_t));

	return 0;
}

static void save_mddp_lls_stats(void)
{
	uint8_t i, j, k, l;
	int32_t ret;
	uint32_t bss_num = MDDP_LLS_BSS_NUM_V1;

	if (!mddpIsSupportMcifWifi() || !mddpIsSupportMddpWh()) {
		DBGLOG(INIT, ERROR, "mddp is not supported.\n");
		return;
	}

	if (!gMddpWFunc.get_lls_stat) {
		DBGLOG(INIT, ERROR,
			"gMddpWFunc.get_lls_stat is not supported.\n");
		return;
	}

	kalMemZero(&cur_lls_stats, sizeof(struct wsvc_stat_lls_report_t));
	ret = gMddpWFunc.get_lls_stat(&cur_lls_stats);
	if (ret != 0) {
		DBGLOG(INIT, ERROR, "get_lls_stat fail, ret: %d.\n", ret);
		return;
	}

	if (cur_lls_stats.version == 0) {
		DBGLOG(INIT, ERROR, "MD is resetting.\n");
		return;
	} else if (cur_lls_stats.version == 1)
		bss_num = MDDP_LLS_BSS_NUM_V1;
	else if (cur_lls_stats.version == 2)
		bss_num = MDDP_LLS_BSS_NUM_V2;

	for (i = 0; i < bss_num; ++i) {
		for (j = 0; j < AC_NUM; ++j) {
			todo_lls_stats.wmm_ac_stat_rx_mpdu[i][j] +=
				isMdResetSinceLastQuery ?
				cur_lls_stats.wmm_ac_stat_rx_mpdu[i][j] :
				(cur_lls_stats.wmm_ac_stat_rx_mpdu[i][j] -
				base_lls_stats.wmm_ac_stat_rx_mpdu[i][j]);
		}
	}

	for (i = 0; i < STA_NUM; ++i) {
		struct rate_stat_rx_mpdu_t *cur, *base, *todo;

		cur = &cur_lls_stats.rate_stat_rx_mpdu[i];
		base = &base_lls_stats.rate_stat_rx_mpdu[i];
		todo = &todo_lls_stats.rate_stat_rx_mpdu[i];

		for (k = 0; k < STATS_LLS_MAX_OFDM_BW_NUM; ++k) {
			for (l = 0; l < STATS_LLS_OFDM_NUM; ++l) {
				todo->u4RxMpduOFDM[0][k][l] +=
					isMdResetSinceLastQuery ?
					cur->u4RxMpduOFDM[0][k][l] :
					(cur->u4RxMpduOFDM[0][k][l] -
					base->u4RxMpduOFDM[0][k][l]);
			}
		}

		for (k = 0; k < STATS_LLS_MAX_CCK_BW_NUM; ++k) {
			for (l = 0; l < STATS_LLS_CCK_NUM; ++l) {
				todo->u4RxMpduCCK[0][k][l] +=
					isMdResetSinceLastQuery ?
					cur->u4RxMpduCCK[0][k][l] :
					(cur->u4RxMpduCCK[0][k][l] -
					base->u4RxMpduCCK[0][k][l]);
			}
		}

		for (k = 0; k < STATS_LLS_MAX_HT_BW_NUM; ++k) {
			for (l = 0; l < STATS_LLS_HT_NUM; ++l) {
				todo->u4RxMpduHT[0][k][l] +=
					isMdResetSinceLastQuery ?
					cur->u4RxMpduHT[0][k][l] :
					(cur->u4RxMpduHT[0][k][l] -
					base->u4RxMpduHT[0][k][l]);
			}
		}

		for (j = 0; j < STATS_LLS_MAX_NSS_NUM; ++j) {
			for (k = 0; k < STATS_LLS_MAX_VHT_BW_NUM; ++k) {
				for (l = 0; l < STATS_LLS_VHT_NUM; ++l) {
					todo->u4RxMpduVHT[j][k][l] +=
						isMdResetSinceLastQuery ?
						cur->u4RxMpduVHT[j][k][l] :
						(cur->u4RxMpduVHT[j][k][l] -
						base->u4RxMpduVHT[j][k][l]);
				}
			}
		}

		for (j = 0; j < STATS_LLS_MAX_NSS_NUM; ++j) {
			for (k = 0; k < STATS_LLS_MAX_HE_BW_NUM; ++k) {
				for (l = 0; l < STATS_LLS_HE_NUM; ++l) {
					todo->u4RxMpduHE[j][k][l] +=
						isMdResetSinceLastQuery ?
						cur->u4RxMpduHE[j][k][l] :
						(cur->u4RxMpduHE[j][k][l] -
						base->u4RxMpduHE[j][k][l]);
				}
			}
		}

		for (j = 0; j < STATS_LLS_MAX_NSS_NUM; ++j) {
			for (k = 0; k < STATS_LLS_MAX_EHT_BW_NUM; ++k) {
				for (l = 0; l < STATS_LLS_EHT_NUM; ++l) {
					todo->u4RxMpduEHT[j][k][l] +=
						isMdResetSinceLastQuery ?
						cur->u4RxMpduEHT[j][k][l] :
						(cur->u4RxMpduEHT[j][k][l] -
						base->u4RxMpduEHT[j][k][l]);
				}
			}
		}
	}

	DBGLOG(INIT, INFO, "save mddp lls stats done.\n");
}
#endif

static bool mddpIsSsnSent(struct ADAPTER *prAdapter,
			  uint8_t *prReorderBuf, uint16_t u2SSN)
{
	uint8_t ucSent = 0;
	uint16_t u2Idx = u2SSN / 8;
	uint8_t ucBit = u2SSN % 8;

	ucSent = (prReorderBuf[u2Idx] & BIT(ucBit)) != 0;
	prReorderBuf[u2Idx] &= ~(BIT(ucBit));

	return ucSent;
}

static void mddpGetRxReorderBuffer(struct ADAPTER *prAdapter,
				   struct SW_RFB *prSwRfb,
				   struct mddpw_md_virtual_buf_t **prMdBuf,
				   struct mddpw_ap_virtual_buf_t **prApBuf)
{
	struct mddpw_md_reorder_sync_table_t *prMdTable = NULL;
	struct mddpw_ap_reorder_sync_table_t *prApTable = NULL;
	uint8_t ucStaRecIdx = prSwRfb->ucStaRecIdx;
	uint8_t ucTid = prSwRfb->ucTid;
	int32_t u4Idx = 0;

	if (gMddpWFunc.get_md_rx_reorder_buf &&
	    gMddpWFunc.get_ap_rx_reorder_buf) {
		if (!gMddpWFunc.get_md_rx_reorder_buf(&prMdTable) &&
		    !gMddpWFunc.get_ap_rx_reorder_buf(&prApTable)) {
			u4Idx = prMdTable->reorder_info[ucStaRecIdx].buf_idx;
			*prMdBuf = &prMdTable->virtual_buf[u4Idx][ucTid];
			*prApBuf = &prApTable->virtual_buf[u4Idx][ucTid];
		}
	}
}

void mddpUpdateReorderQueParm(struct ADAPTER *prAdapter,
			      struct RX_BA_ENTRY *prReorderQueParm,
			      struct SW_RFB *prSwRfb)
{
	struct mddpw_md_virtual_buf_t *prMdBuf = NULL;
	struct mddpw_ap_virtual_buf_t *prApBuf = NULL;
	uint16_t u2SSN = prReorderQueParm->u2WinStart, u2Idx;

	mddpGetRxReorderBuffer(prAdapter, prSwRfb, &prMdBuf, &prApBuf);
	if (!prMdBuf || !prApBuf) {
		DBGLOG(QM, ERROR, "Can't get reorder buffer.\n");
		return;
	}

	for (u2Idx = 0; u2Idx < prReorderQueParm->u2WinSize; u2Idx++) {
		if (prReorderQueParm->u2WinStart == prSwRfb->u2SSN ||
		    !mddpIsSsnSent(prAdapter, prMdBuf->virtual_buf, u2SSN))
			break;

		prReorderQueParm->u2WinStart = u2SSN % MAX_SEQ_NO_COUNT;
		prReorderQueParm->u2WinEnd =
			SEQ_ADD(prReorderQueParm->u2WinStart,
				prReorderQueParm->u2WinSize - 1);
		SEQ_INC(u2SSN);
		DBGLOG(QM, TRACE,
			"Update reorder window: SSN: %d, start: %d, end: %d.\n",
			u2SSN,
			prReorderQueParm->u2WinStart,
			prReorderQueParm->u2WinEnd);
	}

	prApBuf->start_idx = prReorderQueParm->u2WinStart;
	prApBuf->end_idx = prReorderQueParm->u2WinEnd;
}

int32_t mddpNotifyDrvInfoQue(struct QUE *prQue)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_entry *prDrvInfoEntry;
	struct mddpw_drv_info_t *prDrvInfo;
	struct QUE_ENTRY *prEntry = NULL;
	uint32_t u4BufSize = 0, u4InfoNum = 0, u4DrvInfoSize;
	uint8_t *buff = NULL, *cur = NULL;
	int32_t ret = 0;

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(NIC, ERROR, "notify_drv_info callback NOT exist.\n");
		ret = -1;
		goto exit;
	}

	u4BufSize = sizeof(struct mddpw_drv_notify_info_t);
	prDrvInfoEntry = QUEUE_GET_HEAD(prQue);
	while (prDrvInfoEntry) {
		if (prDrvInfoEntry->info) {
			u4BufSize += sizeof(struct mddpw_drv_info_t) +
				prDrvInfoEntry->info->info_len;
			u4InfoNum++;
		}
		prDrvInfoEntry = QUEUE_GET_NEXT_ENTRY(prDrvInfoEntry);
	}

	buff = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);
	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "buffer allocation failed.\n");
		ret = -1;
		goto exit;
	}

	prNotifyInfo = (struct mddpw_drv_notify_info_t *)buff;
	prNotifyInfo->version = 0;
	prNotifyInfo->buf_len = u4BufSize -
		sizeof(struct mddpw_drv_notify_info_t);
	prNotifyInfo->info_num = u4InfoNum;

	cur = buff + sizeof(struct mddpw_drv_notify_info_t);
	QUEUE_REMOVE_HEAD(prQue, prEntry, struct QUE_ENTRY *);
	while (prEntry) {
		prDrvInfoEntry = (struct mddpw_drv_info_entry *)prEntry;
		prDrvInfo = prDrvInfoEntry->info;
		if (prDrvInfo) {
			u4DrvInfoSize = sizeof(struct mddpw_drv_info_t) +
				prDrvInfo->info_len;

			kalMemCopy(cur, prDrvInfo, u4DrvInfoSize);
			cur += u4DrvInfoSize;
			kalMemFree(prDrvInfo, VIR_MEM_TYPE, u4DrvInfoSize);
		}
		kalMemFree(prDrvInfoEntry, VIR_MEM_TYPE,
			   sizeof(struct mddpw_drv_info_entry));
		QUEUE_REMOVE_HEAD(prQue, prEntry, struct QUE_ENTRY *);
	}

	ret = gMddpWFunc.notify_drv_info(prNotifyInfo);
	if (ret)
		DBGLOG(INIT, WARN, "notify failed. mddp status:%d\n",
		       g_eMddpStatus);
exit:
	if (buff)
		kalMemFree(buff, VIR_MEM_TYPE, u4BufSize);

	return ret;
}

int32_t mddpNotifyDrvTxd(struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t fgActivate)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	struct mddp_txd_t *prMddpTxd;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct net_device *prNetdev;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_STA_RECORD *prMldSta;
#endif
#if (MDDP_SUPPORT_NOTIFY_INFO_V1 == 1)
	struct mddpw_drv_notify_info_t_v1 *prNotifyInfoV1;
	struct mddpw_drv_info_t_v1 *prDrvInfoV1;
	void *prTemplate;
	u_int8_t fgIsNewFormat = FALSE;
	uint32_t u4TxdNum = 1, u4Idx = 0;
#endif /* MDDP_SUPPORT_NOTIFY_INFO_V1 */
	uint32_t u4BufSize = 0;
	uint8_t *buff = NULL;
	int32_t ret = 0;

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(NIC, ERROR, "notify_drv_info callback NOT exist.\n");
		ret = -1;
		goto exit;
	}
	if (!prStaRec) {
		DBGLOG(NIC, ERROR, "sta NOT valid\n");
		ret = -1;
		goto exit;
	}
	if (prStaRec->ucBssIndex >= MAX_BSSID_NUM) {
		DBGLOG(NIC, ERROR, "sta bssid NOT valid: %d.\n",
				prStaRec->ucBssIndex);
		ret = -1;
		goto exit;
	}
	if (fgActivate && !prStaRec->aprTxDescTemplate[0]) {
		DBGLOG(NIC, INFO,
			"sta[%d]'s TXD NOT generated done, maybe wait.\n",
			prStaRec->ucBssIndex);
		ret = -1;
		goto exit;
	}

	prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
	prNetdev = wlanGetNetDev(prAdapter->prGlueInfo, prStaRec->ucBssIndex);
	if (prNetdev) {
		prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prNetdev);
		if (!prNetDevPrivate->ucMddpSupport) {
			DBGLOG(NIC, TRACE, "mddp not support\n");
			goto exit;
		}
	} else {
		DBGLOG(NIC, INFO, "NetDev is null BssIndex[%d]\n",
		       prStaRec->ucBssIndex);
	}

#if (MDDP_SUPPORT_NOTIFY_INFO_V1 == 1)
	if (mddp_check_subfeature(MF_ID_COMMON, COM_LEN2B)) {
		fgIsNewFormat = TRUE;
		u4TxdNum = 8;
		u4BufSize = (sizeof(struct mddpw_drv_notify_info_t_v1) +
			      sizeof(struct mddpw_drv_info_t_v1) +
			      sizeof(struct mddp_txd_t) +
			      NIC_TX_DESC_LONG_FORMAT_LENGTH * u4TxdNum);
	} else
#endif /* MDDP_SUPPORT_NOTIFY_INFO_V1 */
	{
		u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			      sizeof(struct mddpw_drv_info_t) +
			      sizeof(struct mddp_txd_t) +
			      NIC_TX_DESC_LONG_FORMAT_LENGTH);
	}
	buff = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);
	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "buffer allocation failed.\n");
		ret = -1;
		goto exit;
	}

#if (MDDP_SUPPORT_NOTIFY_INFO_V1 == 1)
	if (fgIsNewFormat) {
		prNotifyInfoV1 = (struct mddpw_drv_notify_info_t_v1 *) buff;
		prNotifyInfoV1->version = 1;
		prNotifyInfoV1->buf_len = sizeof(struct mddpw_drv_info_t_v1) +
			sizeof(struct mddp_txd_t) +
			NIC_TX_DESC_LONG_FORMAT_LENGTH * u4TxdNum;
		prNotifyInfoV1->info_num = 1;
		prDrvInfoV1 = (struct mddpw_drv_info_t_v1 *)
			&(prNotifyInfoV1->buf[0]);
		prDrvInfoV1->info_id = WSVC_DRVINFO_TXD_TEMPLATE;
		prDrvInfoV1->info_len =
			(sizeof(struct mddp_txd_t) +
			NIC_TX_DESC_LONG_FORMAT_LENGTH * u4TxdNum);
		prMddpTxd = (struct mddp_txd_t *) &(prDrvInfoV1->info[0]);
		prMddpTxd->version = 2;
	} else
#endif /* MDDP_SUPPORT_NOTIFY_INFO_V1 */
	{
		prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
		prNotifyInfo->version = 0;
		prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
			sizeof(struct mddp_txd_t) +
			NIC_TX_DESC_LONG_FORMAT_LENGTH;
		prNotifyInfo->info_num = 1;
		prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
		prDrvInfo->info_id = WSVC_DRVINFO_TXD_TEMPLATE;
		prDrvInfo->info_len =
			(sizeof(struct mddp_txd_t) +
			 NIC_TX_DESC_LONG_FORMAT_LENGTH);
		prMddpTxd = (struct mddp_txd_t *) &(prDrvInfo->info[0]);
		prMddpTxd->version = 1;
	}

	prMddpTxd->sta_idx = prStaRec->ucIndex;
	prMddpTxd->wlan_idx = prStaRec->ucWlanIndex;
	prMddpTxd->sta_mode = prStaRec->eStaType;
	prMddpTxd->bss_id = prStaRec->ucBssIndex;
	/* TODO: Create a new msg for DMASHDL BMP */
	prMddpTxd->wmmset = halRingDataSelectByWmmIndex(prAdapter,
		prBssInfo->ucWmmQueSet);
	if (prNetdev) {
		kalMemCopy(prMddpTxd->nw_if_name, prNetdev->name,
			   sizeof(prMddpTxd->nw_if_name));
	} else {
		kalMemZero(prMddpTxd->nw_if_name,
			   sizeof(prMddpTxd->nw_if_name));
	}
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	prMldSta = mldStarecGetByStarec(prAdapter, prStaRec);
#if (CFG_SINGLE_BAND_MLSR_56 == 1)
	if (prMldSta && prMldSta->fgIsSbMlsr)
		prMldSta = NULL;
#endif /* CFG_SINGLE_BAND_MLSR_56*/
	if (prMldSta)
		kalMemCopy(prMddpTxd->aucMacAddr, prMldSta->aucPeerMldAddr,
			   MAC_ADDR_LEN);
	else
#endif /* CFG_SUPPORT_802_11BE_MLO */
		kalMemCopy(prMddpTxd->aucMacAddr, prStaRec->aucMacAddr,
			   MAC_ADDR_LEN);
	kalMemCopy(prMddpTxd->local_mac,
		   prBssInfo->aucOwnMacAddr, MAC_ADDR_LEN);
	if (fgActivate) {
		prMddpTxd->txd_length = NIC_TX_DESC_LONG_FORMAT_LENGTH;
#if (MDDP_SUPPORT_NOTIFY_INFO_V1 == 1)
		for (u4Idx = 0; u4Idx < u4TxdNum; u4Idx++) {
			if (prStaRec->aprTxDescTemplate[u4Idx])
				prTemplate = prStaRec->aprTxDescTemplate[u4Idx];
			else
				prTemplate = prStaRec->aprTxDescTemplate[0];
			kalMemCopy(prMddpTxd->txd +
				   u4Idx * prMddpTxd->txd_length,
				   prTemplate,
				   prMddpTxd->txd_length);
		}
#else
		kalMemCopy(prMddpTxd->txd, prStaRec->aprTxDescTemplate[0],
				prMddpTxd->txd_length);
#endif /* MDDP_SUPPORT_NOTIFY_INFO_V1 */
	} else {
		prMddpTxd->txd_length = 0;
	}

#if (MDDP_SUPPORT_NOTIFY_INFO_V1 == 1)
	if (fgIsNewFormat)
		ret = gMddpWFunc.notify_drv_info_v1(prNotifyInfoV1);
	else
#endif /* MDDP_SUPPORT_NOTIFY_INFO_V1 */
		ret = gMddpWFunc.notify_drv_info(prNotifyInfo);

#define TEMP_LOG_TEMPLATE "ver:%d,idx:%d,w_idx:%d,mod:%d,bss:%d,wmm:%d," \
		"name:%s,act:%d,ret:%d"
	DBGLOG(NIC, INFO, TEMP_LOG_TEMPLATE,
		prMddpTxd->version,
		prMddpTxd->sta_idx,
		prMddpTxd->wlan_idx,
		prMddpTxd->sta_mode,
		prMddpTxd->bss_id,
		prMddpTxd->wmmset,
		prMddpTxd->nw_if_name,
		fgActivate,
		ret);
#undef TEMP_LOG_TEMPLATE

exit:
	if (buff)
		kalMemFree(buff, VIR_MEM_TYPE, u4BufSize);
	return ret;
}

#if defined(_HIF_PCIE)
int32_t mddpNotifyWifiPcieBarInfo(struct QUE *prQue)
{
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct mddpw_drv_info_entry *prDrvInfoEntry;
	struct mddpw_drv_info_t *prDrvInfo;
	struct mddp_pcie_bar_info *prBarInfo;
	uint32_t u4BufSize = 0;
	int32_t feature = 0;

	glGetChipInfo((void **)&prChipInfo);
	if (prChipInfo == NULL) {
		DBGLOG(HAL, ERROR, "prChipInfo in NULL\n");
		return -1;
	}

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		return -1;
	}

	if (gMddpWFunc.get_mddp_feature)
		feature = gMddpWFunc.get_mddp_feature();

	prDrvInfoEntry = kalMemAlloc(
		sizeof(struct mddpw_drv_info_entry), VIR_MEM_TYPE);
	if (prDrvInfoEntry == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate entry.\n");
		return -1;
	}

	u4BufSize = sizeof(struct mddpw_drv_info_t) +
		     sizeof(struct mddp_pcie_bar_info);
	prDrvInfo = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);
	if (prDrvInfo == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate drv info.\n");
		kalMemFree(prDrvInfoEntry, VIR_MEM_TYPE,
			   sizeof(struct mddpw_drv_info_entry));
		return -1;
	}

	prDrvInfoEntry->info = prDrvInfo;
	prDrvInfo->info_id = WFPM_DRVINFO_PCIE_MMIO;
	prDrvInfo->info_len = sizeof(struct mddp_pcie_bar_info);
	prBarInfo = (struct mddp_pcie_bar_info *) &(prDrvInfo->info[0]);
	prBarInfo->version = 0;
	prBarInfo->offset = prChipInfo->u8CsrOffset;

	QUEUE_INSERT_TAIL(prQue, prDrvInfoEntry);

	DBGLOG(INIT, DEBUG, "pcie bar info 0x%llx, feature:0x%x.\n",
	       prBarInfo->offset, feature);

	return 0;
}
#endif /* _HIF_PCIE */

int32_t mddpNotifyTrigSerRspInfo(enum MD_TRIG_SER_RESULT eRst)
{
	struct QUE rQue, *prQue = &rQue;
	struct mddpw_drv_info_entry *prDrvInfoEntry;
	struct mddpw_drv_info_t *prDrvInfo;
	struct macod_trig_ser_rsp_t *prRsp;
	uint32_t u4BufSize = 0;

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		return -1;
	}

	QUEUE_INITIALIZE(prQue);
	prDrvInfoEntry = kalMemAlloc(
		sizeof(struct mddpw_drv_info_entry), VIR_MEM_TYPE);
	if (prDrvInfoEntry == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate entry.\n");
		return -1;
	}

	u4BufSize = sizeof(struct mddpw_drv_info_t) +
		     sizeof(struct macod_trig_ser_rsp_t);
	prDrvInfo = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);
	if (prDrvInfo == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate drv info.\n");
		kalMemFree(prDrvInfoEntry, VIR_MEM_TYPE,
			   sizeof(struct mddpw_drv_info_entry));
		return -1;
	}

	prDrvInfoEntry->info = prDrvInfo;
	prDrvInfo->info_id = MACOD_DRVINFO_TRIG_SER_RSP;
	prDrvInfo->info_len = sizeof(struct macod_trig_ser_rsp_t);
	prRsp = (struct macod_trig_ser_rsp_t *) &(prDrvInfo->info[0]);
	prRsp->ver = 0;
	prRsp->sn = g_u4SerSn;
	prRsp->rsp = eRst;

	DBGLOG(INIT, DEBUG, "SER rsp %u.\n", prRsp->rsp);

	QUEUE_INSERT_TAIL(prQue, prDrvInfoEntry);
	mddpNotifyDrvInfoQue(prQue);

	return 0;
}

int32_t mddpNotifyTrigSerSuppInfo(struct QUE *prQue)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct mddpw_drv_info_entry *prDrvInfoEntry;
	struct mddpw_drv_info_t *prDrvInfo;
	struct macod_trig_ser_supp_t *prSuppInfo;
	uint32_t u4BufSize = 0;

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		return -1;
	}

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return -1;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return -1;
	}

	prDrvInfoEntry = kalMemAlloc(
		sizeof(struct mddpw_drv_info_entry), VIR_MEM_TYPE);
	if (prDrvInfoEntry == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate entry.\n");
		return -1;
	}

	u4BufSize = sizeof(struct mddpw_drv_info_t) +
		     sizeof(struct macod_trig_ser_supp_t);
	prDrvInfo = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);
	if (prDrvInfo == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate drv info.\n");
		kalMemFree(prDrvInfoEntry, VIR_MEM_TYPE,
			   sizeof(struct mddpw_drv_info_entry));
		return -1;
	}

	prDrvInfoEntry->info = prDrvInfo;
	prDrvInfo->info_id = MACOD_DRVINFO_TRIG_SER_SUPP;
	prDrvInfo->info_len = sizeof(struct macod_trig_ser_supp_t);
	prSuppInfo = (struct macod_trig_ser_supp_t *) &(prDrvInfo->info[0]);
	prSuppInfo->ver = 0;
	if (prAdapter->rWifiVar.eEnableSerL1 == FEATURE_OPT_SER_ENABLE)
		prSuppInfo->supp_type = MD_TRIG_SER_SUPP;
	else
		prSuppInfo->supp_type = MD_TRIG_SER_FW_NOT_SUPP;

	QUEUE_INSERT_TAIL(prQue, prDrvInfoEntry);

	DBGLOG(INIT, DEBUG, "SER support type %u.\n", prSuppInfo->supp_type);

	return 0;
}

int32_t mddpNotifyWifiStatusByQue(
	struct QUE *prQue, enum ENUM_MDDPW_DRV_INFO_STATUS status)
{
	struct mddpw_drv_info_entry *prDrvInfoEntry;
	struct mddpw_drv_info_t *prDrvInfo;
	uint32_t u4BufSize = 0;
	int32_t ret = 0, feature = 0;
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	struct mddpw_coex_intf_info_t *prCoexInfo;

	if (GLUE_GET_REF_CNT(g_rSettings.seq) >= COEX_NOTIFY_MAX_SEQ)
		GLUE_SET_REF_CNT(0, g_rSettings.seq);
	GLUE_INC_REF_CNT(g_rSettings.seq);
	GLUE_SET_REF_CNT(0, g_rSettings.md_status);
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

	if (gMddpWFunc.get_mddp_feature)
		feature = gMddpWFunc.get_mddp_feature();

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		ret = -1;
		goto exit;
	}

	prDrvInfoEntry = kalMemAlloc(
		sizeof(struct mddpw_drv_info_entry), VIR_MEM_TYPE);
	if (prDrvInfoEntry == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate entry.\n");
		ret = -1;
		goto exit;
	}

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	u4BufSize = sizeof(struct mddpw_drv_info_t) +
		      sizeof(struct mddpw_coex_intf_info_t);
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	u4BufSize = sizeof(struct mddpw_drv_info_t) + sizeof(bool);
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	prDrvInfo = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);
	if (prDrvInfo == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate drv info.\n");
		kalMemFree(prDrvInfoEntry, VIR_MEM_TYPE,
			   sizeof(struct mddpw_drv_info_entry));
		ret = -1;
		goto exit;
	}

	prDrvInfoEntry->info = prDrvInfo;
	prDrvInfo->info_id = WSVC_DRVINFO_WIFI_ONOFF;
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
		sizeof(struct mddpw_coex_intf_info_t);
	prDrvInfo->info_len = sizeof(struct mddpw_coex_intf_info_t);
	prCoexInfo = (struct mddpw_coex_intf_info_t *)
		&(prDrvInfo->info[0]);
	prCoexInfo->status = status;
	prCoexInfo->ringNum = 0;
	prCoexInfo->seq = GLUE_GET_REF_CNT(g_rSettings.seq);
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	prDrvInfo->info_len = WIFI_ONOFF_NOTIFICATION_LEN;
	prDrvInfo->info[0] = status;
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

	QUEUE_INSERT_TAIL(prQue, prDrvInfoEntry);

	DBGLOG(INIT, DEBUG, "power: %d, ret: %d, feature:%d.\n",
	       status, ret, feature);
	g_eMddpStatus = status;

exit:
	return ret;
}

int32_t mddpNotifyWifiStatus(enum ENUM_MDDPW_DRV_INFO_STATUS status)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	uint32_t u4BufSize = 0;
	uint8_t *buff = NULL;
	int32_t ret = 0, feature = 0;
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	struct mddpw_coex_intf_info_t *prCoexInfo;

	if (GLUE_GET_REF_CNT(g_rSettings.seq) >= COEX_NOTIFY_MAX_SEQ)
		GLUE_SET_REF_CNT(0, g_rSettings.seq);
	GLUE_INC_REF_CNT(g_rSettings.seq);
	GLUE_SET_REF_CNT(0, g_rSettings.md_status);
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

	if (gMddpWFunc.get_mddp_feature)
		feature = gMddpWFunc.get_mddp_feature();

	if (gMddpWFunc.notify_drv_info) {
		int32_t ret;

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
		u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			sizeof(struct mddpw_drv_info_t) +
			sizeof(struct mddpw_coex_intf_info_t));
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
		u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			sizeof(struct mddpw_drv_info_t) + sizeof(bool));
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
		buff = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);

		if (buff == NULL) {
			DBGLOG(NIC, ERROR, "Can't allocate buffer.\n");
			return -1;
		}
		prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
		prNotifyInfo->version = 0;
		prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
				sizeof(bool);
		prNotifyInfo->info_num = 1;
		prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
		prDrvInfo->info_id = WSVC_DRVINFO_WIFI_ONOFF;
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
		prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
				sizeof(struct mddpw_coex_intf_info_t);
		prDrvInfo->info_len = sizeof(struct mddpw_coex_intf_info_t);
		prCoexInfo = (struct mddpw_coex_intf_info_t *)
				&(prDrvInfo->info[0]);
		prCoexInfo->status = status;
		prCoexInfo->ringNum = 0;
		prCoexInfo->seq = GLUE_GET_REF_CNT(g_rSettings.seq);
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
		prDrvInfo->info_len = WIFI_ONOFF_NOTIFICATION_LEN;
		prDrvInfo->info[0] = status;
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

		ret = gMddpWFunc.notify_drv_info(prNotifyInfo);
		DBGLOG(INIT, INFO, "power: %d, ret: %d, feature:%d.\n",
		       status, ret, feature);
		kalMemFree(buff, VIR_MEM_TYPE, u4BufSize);
		g_eMddpStatus = status;
		if (ret)
			DBGLOG(INIT, WARN, "notify failed. mddp status:%d\n",
			       g_eMddpStatus);
	} else {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		ret = -1;
	}

	return ret;
}

int32_t mddpNotifyCheckSer(uint32_t u4Status)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	struct wsv_check_ser_info_t *prSerInfo;
	uint32_t u4BufSize = 0;
	uint8_t *buff = NULL;
	int32_t ret = 0;

	if (!u4Status) {
		ret = -1;
		goto exit;
	}

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		ret = -1;
		goto exit;
	}

	u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
		      sizeof(struct mddpw_drv_info_t) +
		      sizeof(struct wsv_check_ser_info_t));
	buff = kalMemAlloc(u4BufSize, PHY_MEM_TYPE);
	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "Can't allocate buffer.\n");
		ret = -1;
		goto exit;
	}

	prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
	prNotifyInfo->version = 0;
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
		sizeof(struct wsv_check_ser_info_t);
	prNotifyInfo->info_num = 1;
	prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
	prDrvInfo->info_id = WSVC_DRVINFO_CHECK_SER;
	prDrvInfo->info_len = sizeof(struct wsv_check_ser_info_t);
	prSerInfo = (struct wsv_check_ser_info_t *) &(prDrvInfo->info[0]);
	prSerInfo->sn = g_u4CheckSerCnt;
	g_u4CheckSerCnt++;

	ret = gMddpWFunc.notify_drv_info(prNotifyInfo);
	DBGLOG(NIC, INFO, "check ser sn: %u, sta:0x%08x, ret: %d.\n",
	       prSerInfo->sn, u4Status, ret);
	kalMemFree(buff, PHY_MEM_TYPE, u4BufSize);

exit:
	return ret;
}

static bool mddpIsCasanFWload(void)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	bool ret = FALSE;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		goto exit;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		goto exit;
	}

	if (prAdapter->u4CasanLoadType == 1)
		ret = TRUE;

exit:
	return ret;
}

int32_t mddpNotifyDrvOwnTimeoutTime(void)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	int32_t ret = 0;
	uint32_t u4BufSize = 0;
	uint32_t u4DrvOwnTimeoutTime = g_rSettings.u4MdDrvOwnTimeoutTime;
	uint8_t *buff = NULL;

	DBGLOG(INIT, INFO, "Wi-Fi Notify MD Drv Own Timeout time.\n");

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(NIC, ERROR, "notify_drv_info callback NOT exist.\n");
		ret = -1;
		goto exit;
	}

	/* align AP/MD drv own timeout */
	if (mddpIsCasanFWload() == TRUE)
		u4DrvOwnTimeoutTime = LP_OWN_BACK_TOTAL_DELAY_CASAN_MS;

	u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			sizeof(struct mddpw_drv_info_t) + sizeof(uint32_t));

	buff = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);

	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "buffer allocation failed.\n");
		ret = -ENODEV;
		goto exit;
	}

	prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
	prNotifyInfo->version = 0;
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
			sizeof(uint32_t);
	prNotifyInfo->info_num = 1;
	prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
	prDrvInfo->info_id = WSVC_DRVINFO_DRVOWN_TIME_SET;
	prDrvInfo->info_len = sizeof(uint32_t);

	kalMemCopy((uint32_t *) &(prDrvInfo->info[0]), &u4DrvOwnTimeoutTime,
			sizeof(uint32_t));

	ret = gMddpWFunc.notify_drv_info(prNotifyInfo);

exit:
	if (buff)
		kalMemFree(buff, VIR_MEM_TYPE, u4BufSize);

	DBGLOG(INIT, INFO, "ret: %d, timeout: %d.\n",
				   ret, u4DrvOwnTimeoutTime);
	return ret;
}

#if defined(_HIF_PCIE)
#if CFG_SUPPORT_PCIE_ASPM
int32_t mddpNotifyMDPCIeL12Status(uint8_t fgEnable)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	int32_t ret = 0;
	uint32_t u4BufSize = 0;
	uint8_t ucInfoId = WSVC_DRVINFO_PCIE_L_LOCK_SUCCESS;
	uint8_t *buff = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	DBGLOG(INIT, TRACE, "Notify PCIe L1.2 Status %u\n", fgEnable);

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(NIC, ERROR, "notify_drv_info callback NOT exist.\n");
		ret = -1;
		goto exit;
	}

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		goto exit;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		goto exit;
	}

	u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			sizeof(struct mddpw_drv_info_t) + sizeof(uint32_t));

	buff = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);

	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "buffer allocation failed.\n");
		ret = -ENODEV;
		goto exit;
	}

	if (!fgEnable) {
		/* Disable L1ss */
		ucInfoId = WSVC_DRVINFO_PCIE_L_LOCK_SUCCESS;
		GLUE_INC_REF_CNT(prAdapter->u4MddpPCIeL12SeqNum);
	} else
		/* Enable L1ss */
		ucInfoId = WSVC_DRVINFO_PCIE_L_UNLOCK_SUCCESS;

	prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
	prNotifyInfo->version = 0;
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
			sizeof(uint32_t);
	prNotifyInfo->info_num = 1;
	prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
	prDrvInfo->info_id = ucInfoId;
	prDrvInfo->info_len = sizeof(uint32_t);

	kalMemCopy((uint32_t *) &(prDrvInfo->info[0]),
			&prAdapter->u4MddpPCIeL12SeqNum,
			sizeof(uint32_t));

	ret = gMddpWFunc.notify_drv_info(prNotifyInfo);

exit:
	if (buff)
		kalMemFree(buff, VIR_MEM_TYPE, u4BufSize);

	if (prAdapter) {
		DBGLOG(INIT, TRACE, "ret: %d, info_id: %u, u4SeqNum:%u.\n",
			ret, ucInfoId, prAdapter->u4MddpPCIeL12SeqNum);
	}

	return ret;
}
#endif

#if (CFG_PCIE_GEN_SWITCH == 1)
int32_t mddpMdNotifyInfoHandleGenSwitchStart(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint16_t u2genSwitchSeq = 0, u2GenSwitchRsp = 0;
	struct mddpw_drv_info_genswitch *prAckRsp =
		(struct mddpw_drv_info_genswitch *) &(prMdInfo->buf[1]);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (prHifInfo->rErrRecoveryCtl.eErrRecovState !=
			   ERR_RECOV_STOP_IDLE) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE,
			u2genSwitchSeq, u2GenSwitchRsp);
		DBGLOG(INIT, ERROR, "SER on-going\n");
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
		goto end;
	}

	if (prHifInfo->u4GenSwitchState != MDDP_GEN_SWITCH_START_BEGIN_STATE) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE,
			u2genSwitchSeq, u2GenSwitchRsp);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
		goto end;
	}

	if (prMdInfo->buf_len >= 4) {
		u2genSwitchSeq = prAckRsp->u2Seq;
		u2GenSwitchRsp = prAckRsp->u2Result;
	}

	if (u2GenSwitchRsp == 1) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_START_END_STATE,
			u2genSwitchSeq, u2GenSwitchRsp);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_START_END_STATE;
		wlandioStopPcieStatus(prAdapter, PCIE_STOP_TRANSITION_END);
	} else {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE,
			u2genSwitchSeq, u2GenSwitchRsp);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
	}

end:
	del_timer_sync(&prHifInfo->rGenSwitch4MddpTimer);

	return 0;
}

int32_t mddpMdNotifyInfoHandleGenSwitchEnd(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint16_t u2genSwitchSeq = 0, u2GenSwitchRsp = 0;
	struct mddpw_drv_info_genswitch *prAckRsp =
		(struct mddpw_drv_info_genswitch *) &(prMdInfo->buf[1]);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (prMdInfo->buf_len >= 4) {
		u2genSwitchSeq = prAckRsp->u2Seq;
		u2GenSwitchRsp = prAckRsp->u2Result;
	}

	DBGLOG(HAL, INFO,
		"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
		prHifInfo->u4GenSwitchState,
		MDDP_GEN_SWITCH_NORMAL_STATE,
		u2genSwitchSeq, u2GenSwitchRsp);
	prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;

	del_timer_sync(&prHifInfo->rGenSwitch4MddpTimer);

	return 0;
}

int32_t mddpMdNotifyInfoHandleGenSwitchByPassStart(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint16_t u2genSwitchSeq = 0, u2GenSwitchRsp = 0;
	struct mddpw_drv_info_genswitch *prAckRsp =
		(struct mddpw_drv_info_genswitch *) &(prMdInfo->buf[1]);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (prMdInfo->buf_len >= 4) {
		u2genSwitchSeq = prAckRsp->u2Seq;
		u2GenSwitchRsp = prAckRsp->u2Result;
	}

	if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_NORMAL_STATE) {
		/* set to bypass state only when GenSwitch in normal state */
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_BYPASS_STATE,
			u2genSwitchSeq, u2GenSwitchRsp);
		prHifInfo->u4GenSwitchState =
			MDDP_GEN_SWITCH_BYPASS_STATE;
		wlandioStopPcieStatus(prAdapter,
			PCIE_MD_BYPASS_GEN_SWITCH_START);
	} else {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u, ignore md bypass\n",
			prHifInfo->u4GenSwitchState,
			prHifInfo->u4GenSwitchState,
			u2genSwitchSeq, u2GenSwitchRsp);
	}

	return 0;
}

int32_t mddpMdNotifyInfoHandleGenSwitchByPassEnd(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint16_t u2genSwitchSeq = 0, u2GenSwitchRsp = 0;
	struct mddpw_drv_info_genswitch *prAckRsp =
		(struct mddpw_drv_info_genswitch *) &(prMdInfo->buf[1]);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (prMdInfo->buf_len >= 4) {
		u2genSwitchSeq = prAckRsp->u2Seq;
		u2GenSwitchRsp = prAckRsp->u2Result;
	}

	if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_BYPASS_STATE) {
		/* set to bypass state only when GenSwitch in normal state */
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d] seq: %u, rsp: %u\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE,
			u2genSwitchSeq, u2GenSwitchRsp);
		prHifInfo->u4GenSwitchState =
			MDDP_GEN_SWITCH_NORMAL_STATE;
		wlandioStopPcieStatus(prAdapter,
			PCIE_MD_BYPASS_GEN_SWITCH_END);
	}

	return 0;
}

int32_t mddpNotifyMDGenSwithAction(uint32_t u4Action)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	int32_t ret = 0;
	uint32_t u4BufSize = 0;
	uint32_t u4InfoId = 0;
	uint8_t *buff = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	DBGLOG(INIT, TRACE, "Notify GenSwitch u4Action %u\n", u4Action);

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(NIC, ERROR, "notify_drv_info callback NOT exist.\n");
		ret = -1;
		goto exit;
	}

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		goto exit;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		goto exit;
	}

	u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			sizeof(struct mddpw_drv_info_t) + sizeof(uint32_t));

	buff = kalMemAlloc(u4BufSize, PHY_MEM_TYPE);

	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "buffer allocation failed.\n");
		ret = -ENODEV;
		goto exit;
	}

	if (u4Action == WFPM_DRVINFO_PCIE_GENSWITCH_START) {
		/* GenSwitch Start */
		u4InfoId = WFPM_DRVINFO_PCIE_GENSWITCH_START;
		GLUE_INC_REF_CNT(prAdapter->u2MddpGenSwitchSeqNum);
	} else if (u4Action == WFPM_DRVINFO_PCIE_GENSWITCH_END) {
		/* GenSwitch End */
		u4InfoId = WFPM_DRVINFO_PCIE_GENSWITCH_END;
	} else
		goto exit;

	prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
	prNotifyInfo->version = 0;
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
			sizeof(uint32_t);
	prNotifyInfo->info_num = 1;
	prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
	prDrvInfo->info_id = u4InfoId;
	prDrvInfo->info_len = sizeof(uint32_t);

	kalMemCopy((uint16_t *) &(prDrvInfo->info[0]),
			&prAdapter->u2MddpGenSwitchSeqNum,
			sizeof(uint16_t));

	ret = gMddpWFunc.notify_drv_info(prNotifyInfo);

exit:
	if (buff)
		kalMemFree(buff, PHY_MEM_TYPE, u4BufSize);

	if (prAdapter) {
		DBGLOG(INIT, TRACE, "ret: %d, info_id: %u, SeqNum:%u.\n",
			ret, u4InfoId, prAdapter->u2MddpGenSwitchSeqNum);
	}

	return ret;
}

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
void mddpGenSwitchMsgTimeout(struct timer_list *timer)
#else
void mddpGenSwitchMsgTimeout(unsigned long arg)
#endif
{
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	struct GL_HIF_INFO *prHif =
		from_timer(prHif, timer, rGenSwitch4MddpTimer);
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)prHif->rSerTimerData;
#else
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)arg;
#endif
	struct ADAPTER *prAdapter = NULL;
	struct GL_HIF_INFO *prHifInfo;

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prHifInfo = &prGlueInfo->rHifInfo;

	/* call API to notify Gen Switch module */
	if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_START_BEGIN_STATE) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d], msg timeout\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_START_ACK_TIMEOUT_STATE);
		prHifInfo->u4GenSwitchState =
			MDDP_GEN_SWITCH_START_ACK_TIMEOUT_STATE;
		mddpNotifyMDGenSwitchEnd(prAdapter);
	} else if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_END_STATE) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d], msg timeout\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
	} else {
		DBGLOG(HAL, ERROR, "invalid state [%d], msg timeout\n",
			prHifInfo->u4GenSwitchState);
	}
}

int32_t mddpNotifyMDGenSwitchStart(struct ADAPTER *prAdapter)
{
	int32_t ret = 0;
	struct GL_HIF_INFO *prHifInfo = NULL;
	int32_t md_state = 0;

	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		goto end;
	}

	if (!is_cal_flow_finished()) {
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
		goto end;
	}

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing()) {
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
		goto end;
	}
#endif
	if (get_wifi_process_status()) {
		DBGLOG(REQ, ERROR,
			"Wi-Fi on/off process is ongoing\n");
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
		goto end;
	}
#endif

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (prHifInfo->rErrRecoveryCtl.eErrRecovState !=
			   ERR_RECOV_STOP_IDLE) {
		DBGLOG(INIT, ERROR, "SER on-going\n");
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
		goto end;
	}

	if (!mddpIsSupportMcifWifi()) {
		wlandioStopPcieStatus(prAdapter, PCIE_STOP_TRANSITION_END);
		goto end;
	}

	if (GLUE_GET_REF_CNT(prHifInfo->fgIsDebugSopOnGoing)) {
		DBGLOG(HAL, ERROR, "Debug SOP On-going\n");
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
		goto end;
	}

#if CFG_MTK_CCCI_SUPPORT
	md_state = ccci_fsm_get_md_state();
#endif

	if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_BYPASS_STATE) {
		/* no need to send msg to md in bypass state */
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_BYPASS_STATE);
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
	} else if (prHifInfo->fgMdResetInd == TRUE) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d], reset ind\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_START_END_SKIP_MD_STATE);
		prHifInfo->u4GenSwitchState =
			MDDP_GEN_SWITCH_START_END_SKIP_MD_STATE;
		wlandioStopPcieStatus(prAdapter, PCIE_STOP_TRANSITION_END);
	} else if (md_state == READY && prHifInfo->u4GenSwitchState ==
			MDDP_GEN_SWITCH_NORMAL_STATE) {
		prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_START_BEGIN_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_START_BEGIN_STATE;
		mod_timer(&prHifInfo->rGenSwitch4MddpTimer,
			  jiffies + MDDP_GEN_SWITCH_MSG_TIMEOUT * HZ /
			  MSEC_PER_SEC);
		DBGLOG(HAL, INFO, "Start GenSwitch timer\n");

		ret = mddpNotifyMDGenSwithAction(
			WFPM_DRVINFO_PCIE_GENSWITCH_START);
	} else if (md_state != READY) {
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_START_END_SKIP_MD_STATE);
		prHifInfo->u4GenSwitchState =
			MDDP_GEN_SWITCH_START_END_SKIP_MD_STATE;
		wlandioStopPcieStatus(prAdapter, PCIE_STOP_TRANSITION_END);
	} else {
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			prHifInfo->u4GenSwitchState);
	}
end:
	return ret;
}

int32_t mddpNotifyMDGenSwitchEnd(struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "Invalid drv state.\n");
		return -1;
	}


	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return -1;
	}

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (prHifInfo) {
		if (GLUE_GET_REF_CNT(prHifInfo->fgIsDebugSopOnGoing)) {
			DBGLOG(HAL, ERROR, "Debug SOP On-going\n");
			return -1;
		}
	}

	KAL_SET_BIT(MDDP_HIF_GEN_SWITCH_END, g_ulMddpActionFlag);
	kalSetMddpEvent(prGlueInfo);

	return 0;
}

int32_t __mddpNotifyMDGenSwitchEnd(struct ADAPTER *prAdapter)
{
	int32_t ret = 0;
	struct GL_HIF_INFO *prHifInfo = NULL;
	int32_t md_state = 0;

	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		goto end;
	}

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (!mddpIsSupportMcifWifi())
		goto end;

	if (!is_cal_flow_finished()) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d], cal not finished\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
		goto end;
	}

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing()) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d], pwr on processing\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
		goto end;
	}
#endif
#endif

#if CFG_MTK_CCCI_SUPPORT
	md_state = ccci_fsm_get_md_state();
#endif

	if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_BYPASS_STATE) {
		/* no need to send msg to md in bypass state */
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_BYPASS_STATE);
	} else if (prHifInfo->fgMdResetInd == TRUE) {
		DBGLOG(HAL, INFO,
			"mddp gen switch state [%d]->[%d], reset ind\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
	} else if (md_state == READY && prHifInfo->u4GenSwitchState ==
			MDDP_GEN_SWITCH_START_END_STATE) {
		prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState, MDDP_GEN_SWITCH_END_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_END_STATE;
		mod_timer(&prHifInfo->rGenSwitch4MddpTimer,
			  jiffies + MDDP_GEN_SWITCH_MSG_TIMEOUT * HZ /
			  MSEC_PER_SEC);
		DBGLOG(HAL, INFO, "Start GenSwitch timer\n");

		ret = mddpNotifyMDGenSwithAction(
			WFPM_DRVINFO_PCIE_GENSWITCH_END);
	} else if (md_state == READY && prHifInfo->u4GenSwitchState ==
			MDDP_GEN_SWITCH_START_BEGIN_STATE) {
		prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState, MDDP_GEN_SWITCH_END_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_END_STATE;
		mod_timer(&prHifInfo->rGenSwitch4MddpTimer,
			  jiffies + MDDP_GEN_SWITCH_MSG_TIMEOUT * HZ /
			  MSEC_PER_SEC);
		DBGLOG(HAL, INFO, "Start GenSwitch timer\n");
		ret = mddpNotifyMDGenSwithAction(
			WFPM_DRVINFO_PCIE_GENSWITCH_END);
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
	} else if (md_state == READY && prHifInfo->u4GenSwitchState ==
			MDDP_GEN_SWITCH_START_ACK_TIMEOUT_STATE) {
		prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState, MDDP_GEN_SWITCH_END_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_END_STATE;
		mod_timer(&prHifInfo->rGenSwitch4MddpTimer,
			  jiffies + MDDP_GEN_SWITCH_MSG_TIMEOUT * HZ /
			  MSEC_PER_SEC);
		DBGLOG(HAL, INFO, "Start GenSwitch timer\n");
		ret = mddpNotifyMDGenSwithAction(
			WFPM_DRVINFO_PCIE_GENSWITCH_END);
		wlandioStopPcieStatus(prAdapter, PCIE_MD_REJECT_GEN_SWITCH);
	} else {
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
	}
end:
	return ret;
}

void mddpHandleGenSwitchMdExp(int32_t md_state)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "Invalid drv state.\n");
		return;
	}
	prHifInfo = &prGlueInfo->rHifInfo;
	if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_START_BEGIN_STATE) {
		del_timer_sync(&prHifInfo->rGenSwitch4MddpTimer);
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_START_END_SKIP_MD_STATE);
		prHifInfo->u4GenSwitchState =
			MDDP_GEN_SWITCH_START_END_SKIP_MD_STATE;
		wlandioStopPcieStatus(prGlueInfo->prAdapter,
			PCIE_STOP_TRANSITION_END);
	} else if (prHifInfo->u4GenSwitchState == MDDP_GEN_SWITCH_END_STATE) {
		del_timer_sync(&prHifInfo->rGenSwitch4MddpTimer);
		DBGLOG(HAL, INFO, "mddp gen switch state [%d]->[%d]\n",
			prHifInfo->u4GenSwitchState,
			MDDP_GEN_SWITCH_NORMAL_STATE);
		prHifInfo->u4GenSwitchState = MDDP_GEN_SWITCH_NORMAL_STATE;
	}
}

bool mddpWaitGenSwitchToNormalState(void)
{
	uint32_t u4StartTime, u4CurTime;
	bool fgCompletion = true;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4GenSwithTimeoutTime = MDDP_GEN_SWITCH_MSG_TIMEOUT;
	struct GL_HIF_INFO *prHifInfo = NULL;

	u4StartTime = kalGetTimeTick();
	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return false;
	}

	prHifInfo = &prGlueInfo->rHifInfo;

	do {
		u4CurTime = kalGetTimeTick();
		if (CHECK_FOR_TIMEOUT(u4CurTime, u4StartTime,
				u4GenSwithTimeoutTime)) {
			DBGLOG(INIT, ERROR,
				"wait for Gen Switch done timeout\n");
			fgCompletion = false;
			break;
		}

		kalMsleep(CFG_RESPONSE_POLLING_DELAY);
	} while (prHifInfo->u4GenSwitchState != MDDP_GEN_SWITCH_NORMAL_STATE);

	return fgCompletion;
}

uint32_t mddpGetGenSwitchState(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	return prHifInfo->u4GenSwitchState;
}
#endif /* CFG_PCIE_GEN_SWITCH */
#endif


#ifdef CFG_SUPPORT_UNIFIED_COMMAND
int32_t mddpNotifyMDUnifiedCmdVer(void)
{
	struct mddpw_drv_notify_info_t *prNotifyInfo;
	struct mddpw_drv_info_t *prDrvInfo;
	int32_t ret = 0;
	uint32_t u4BufSize = 0;
	uint32_t u4UnifiedCmdVer = 1;
	uint8_t *buff = NULL;

	DBGLOG(INIT, INFO, "Notify MD Unified Cmd version.\n");

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(NIC, ERROR, "notify_drv_info callback NOT exist.\n");
		ret = -1;
		goto exit;
	}

	u4BufSize = (sizeof(struct mddpw_drv_notify_info_t) +
			sizeof(struct mddpw_drv_info_t) + sizeof(uint32_t));

	buff = kalMemAlloc(u4BufSize, VIR_MEM_TYPE);

	if (buff == NULL) {
		DBGLOG(NIC, ERROR, "buffer allocation failed.\n");
		ret = -ENODEV;
		goto exit;
	}

	prNotifyInfo = (struct mddpw_drv_notify_info_t *) buff;
	prNotifyInfo->version = 0;
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_info_t) +
			sizeof(uint32_t);
	prNotifyInfo->info_num = 1;
	prDrvInfo = (struct mddpw_drv_info_t *) &(prNotifyInfo->buf[0]);
	prDrvInfo->info_id = WSVC_DRVINFO_WIFI_UNIFIED_CMD_VER;
	prDrvInfo->info_len = sizeof(uint32_t);

	kalMemCopy((uint32_t *) &(prDrvInfo->info[0]), &u4UnifiedCmdVer,
			sizeof(uint32_t));

	ret = gMddpWFunc.notify_drv_info(prNotifyInfo);

exit:
	if (buff)
		kalMemFree(buff, VIR_MEM_TYPE, u4BufSize);

	DBGLOG(INIT, INFO, "ret: %d, Ver: %u.\n", ret, u4UnifiedCmdVer);
	return ret;
}
#endif

static void mddpResetGlobalVariable(void)
{
	g_u4CheckSerCnt = 0;
	g_ulMddpActionFlag = 0;
	g_u4MddpRstFlag = 0;
}

void __mddpNotifyWifiOnStart(u_int8_t fgIsForce)
{
	struct QUE rQue, *prQue = &rQue;

	if (!fgIsForce &&
	    g_eMddpStatus != MDDPW_DRV_INFO_STATUS_OFF_END) {
		DBGLOG(NIC, ERROR, "mddp status mismatch[%u]\n", g_eMddpStatus);
		return;
	}

#if CFG_MTK_CCCI_SUPPORT
	mtk_ccci_register_md_state_cb(&mddpMdStateChangedCb);
#endif

	QUEUE_INITIALIZE(prQue);

	mddpNotifyWifiStatusByQue(prQue, MDDPW_DRV_INFO_STATUS_ON_START);
	mddpNotifyTrigSerSuppInfo(prQue);
#if defined(_HIF_PCIE) && (CFG_MTK_SUPPORT_LIGHT_MDDP == 0)
	mddpNotifyWifiPcieBarInfo(prQue);
#endif
	mddpNotifyDrvInfoQue(prQue);
}

void mddpNotifyWifiOnStart(u_int8_t fgIsForce)
{
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	if (!mddpIsSupportCcci())
		return;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	mddpResetGlobalVariable();

	if (!mddpIsSupportMcifWifi())
		return;

	if (!is_cal_flow_finished())
		return;

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing())
		return;
#endif /* CFG_MTK_WIFI_CONNV3_SUPPORT */
#endif /* CFG_MTK_ANDROID_WMT */
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	mutex_lock(&rMddpLock);
	__mddpNotifyWifiOnStart(fgIsForce);
	mutex_unlock(&rMddpLock);
}

int32_t __mddpNotifyWifiOnEnd(u_int8_t fgIsForce)
{
	int32_t ret = 0;

	if (!fgIsForce &&
	    g_eMddpStatus != MDDPW_DRV_INFO_STATUS_ON_START) {
		DBGLOG(NIC, ERROR, "mddp status mismatch[%u]\n", g_eMddpStatus);
		return ret;
	}

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 0)
	/* Notify Driver own timeout time before Wi-Fi on end */
	mddpNotifyDrvOwnTimeoutTime();

	/* Notify MD Unified Cmd version */
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	mddpNotifyMDUnifiedCmdVer();
#endif

	if (g_rSettings.rOps.set)
		g_rSettings.rOps.set(&g_rSettings, g_rSettings.u4WifiOnBit);

	if (g_rSettings.u4MDDPSupportMode == MDDP_SUPPORT_AOP) {
		if (g_rSettings.rOps.clr)
			g_rSettings.rOps.clr(&g_rSettings,
				g_rSettings.u4MdOnBit);
	}
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP == 0 */

#if (CFG_SUPPORT_CONNAC2X == 0 && CFG_SUPPORT_CONNAC3X == 0)
	ret = mddpNotifyWifiStatus(MDDPW_DRV_INFO_STATUS_ON_END);
#else
	ret = mddpNotifyWifiStatus(MDDPW_DRV_INFO_STATUS_ON_END_QOS);
#endif
	if (ret == 0)
		ret = wait_for_md_on_complete();

	return ret;
}

int32_t mddpNotifyWifiOnEnd(u_int8_t fgIsForce)
{
	int32_t ret = 0;

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	if (!mddpIsSupportCcci())
		return ret;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	if (!mddpIsSupportMcifWifi())
		return ret;

	if (!is_cal_flow_finished())
		return ret;

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing())
		return ret;
#endif /* CFG_MTK_WIFI_CONNV3_SUPPORT */
#endif /* CFG_MTK_ANDROID_WMT */
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	mutex_lock(&rMddpLock);
	ret = __mddpNotifyWifiOnEnd(fgIsForce);
	mutex_unlock(&rMddpLock);

	return ret;
}

void __mddpNotifyWifiOffStart(void)
{
	int32_t ret;

	if (g_eMddpStatus != MDDPW_DRV_INFO_STATUS_ON_END &&
	    g_eMddpStatus != MDDPW_DRV_INFO_STATUS_ON_END_QOS) {
		DBGLOG(NIC, ERROR, "mddp status mismatch[%u]\n", g_eMddpStatus);
		return;
	}

	DBGLOG(INIT, INFO, "md off start.\n");
	if (g_rSettings.u4MDDPSupportMode == MDDP_SUPPORT_AOP) {
		if (g_rSettings.rOps.set)
			g_rSettings.rOps.set(&g_rSettings,
				g_rSettings.u4MdOffBit);
	}

	ret = mddpNotifyWifiStatus(MDDPW_DRV_INFO_STATUS_OFF_START);
	if (ret == 0)
		wait_for_md_off_complete();


#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	mddpMdStateChangeReleaseDrvOwn();
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	/* call fw own directly when wifi off */
	mddpSetMDFwOwn();
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
}

void mddpNotifyWifiOffStart(void)
{
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	if (!mddpIsSupportCcci())
		return;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */

#if defined(_HIF_PCIE)
	struct GLUE_INFO *prGlueInfo = NULL;
#endif /* _HIF_PCIE */

	if (!mddpIsSupportMcifWifi())
		return;

	if (!is_cal_flow_finished())
		return;

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing())
		return;
#endif /* CFG_MTK_WIFI_CONNV3_SUPPORT */
#endif /* CFG_MTK_ANDROID_WMT */
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	mutex_lock(&rMddpLock);
	__mddpNotifyWifiOffStart();
	mutex_unlock(&rMddpLock);

#if defined(_HIF_PCIE) && (CFG_MTK_SUPPORT_LIGHT_MDDP == 0)
	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return;
	}

	/* avoid power off process MD SER */
	kalSetMddpEvent(prGlueInfo);
#endif
}

void __mddpNotifyWifiOffEnd(void)
{
	int32_t u4ClrBits = 0;

	if (g_eMddpStatus != MDDPW_DRV_INFO_STATUS_OFF_START) {
		DBGLOG(NIC, ERROR, "mddp status mismatch[%u]\n", g_eMddpStatus);
		return;
	}

	if (g_rSettings.u4MDDPSupportMode == MDDP_SUPPORT_SHM)
		u4ClrBits = g_rSettings.u4WifiOnBit;
	else
		u4ClrBits = g_rSettings.u4WifiOnBit | g_rSettings.u4MdInitBit;

	if (g_rSettings.rOps.clr) {
		g_rSettings.rOps.clr(
			&g_rSettings,
			u4ClrBits);
	}

	mddpNotifyWifiStatus(MDDPW_DRV_INFO_STATUS_OFF_END);
}

void mddpNotifyWifiOffEnd(void)
{
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	if (!mddpIsSupportCcci())
		return;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	if (!mddpIsSupportMcifWifi())
		return;

	if (!is_cal_flow_finished())
		return;

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing())
		return;
#endif /* CFG_MTK_WIFI_CONNV3_SUPPORT */
#endif /* CFG_MTK_ANDROID_WMT */
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

	mutex_lock(&rMddpLock);
	__mddpNotifyWifiOffEnd();
	mutex_unlock(&rMddpLock);
}

void mddpUnregisterMdStateCB(void)
{
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 0)
	if (!mddpIsSupportMcifWifi())
		return;

	if (!is_cal_flow_finished())
		return;

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing())
		return;
#endif /* CFG_MTK_WIFI_CONNV3_SUPPORT */
#endif /* CFG_MTK_ANDROID_WMT */
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP == 0 */

#if CFG_MTK_CCCI_SUPPORT
	mutex_lock(&rMddpLock);
	DBGLOG(INIT, INFO, "unregister mddp ccci cb\n");
	mtk_ccci_register_md_state_cb(NULL);
	mutex_unlock(&rMddpLock);
#endif
}

void mddpNotifyWifiReset(void)
{
	if (!mddpIsSupportMcifWifi())
		return;

	if (g_rSettings.rOps.clr)
		g_rSettings.rOps.clr(&g_rSettings, g_rSettings.u4WifiOnBit);
}

u_int8_t mddpMdNotifyInfoSanityCheck(struct GLUE_INFO *prGlueInfo)
{
	u_int8_t fgHalted = FALSE;

	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return FALSE;
	}

	if (prGlueInfo->prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return FALSE;
	}

	fgHalted = kalIsHalted(prGlueInfo);

	if (fgHalted || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, INFO,
			"Skip update info. to MD, fgHalted: %d, u4ReadyFlag: %d\n",
			fgHalted, prGlueInfo->u4ReadyFlag);
		return FALSE;
	}

	if (!mddpIsSupportMcifWifi()) {
		DBGLOG(INIT, ERROR, "mcif wifi not support.\n");
		return FALSE;
	}

	if (!is_cal_flow_finished()) {
		DBGLOG(INIT, ERROR, "cal flow not finished.\n");
		return FALSE;
	}

#if CFG_MTK_ANDROID_WMT
#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
	if (is_pwr_on_notify_processing()) {
		DBGLOG(INIT, ERROR, "is_pwr_on_notify_processing.\n");
		return FALSE;
	}
#endif
#endif
	return TRUE;
}

int32_t mddpMdNotifyInfoHandleResetInd(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct BSS_INFO *prSapBssInfo = (struct BSS_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	u_int8_t fgIsForceNotify = TRUE;
	uint32_t i;
	int32_t ret = 0;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prHifInfo->fgMdResetInd = TRUE;

	DBGLOG(INIT, INFO, "MD resetting.\n");
	save_mddp_stats();
#if CFG_SUPPORT_LLS && CFG_SUPPORT_LLS_MDDP
	save_mddp_lls_stats();
	isMdResetSinceLastQuery = TRUE;
#endif
#if defined(_HIF_PCIE)
#if (CFG_PCIE_GEN_SWITCH == 1)
	mddpWaitGenSwitchToNormalState();
#endif /* CFG_PCIE_GEN_SWITCH */
#endif
	/* don't notify md when wifi off */
	if (g_eMddpStatus == MDDPW_DRV_INFO_STATUS_OFF_START)
		fgIsForceNotify = FALSE;
	mutex_lock(&rMddpLock);
	__mddpNotifyWifiOnStart(fgIsForceNotify);
	ret = __mddpNotifyWifiOnEnd(fgIsForceNotify);
	mutex_unlock(&rMddpLock);
	if (ret) {
		DBGLOG(INIT, INFO, "mddpNotifyWifiOnEnd failed.\n");
		return 0;
	}

	g_fgIsMdCrash = FALSE;
	prHifInfo->fgMdResetInd = FALSE;

	/* Notify STA's TXD to MD */
	for (i = 0; i < KAL_AIS_NUM; i++) {
		struct BSS_INFO *prAisBssInfo =	aisGetMainLinkBssInfo(
			aisFsmGetInstance(prAdapter, i));

		if (prAisBssInfo && prAisBssInfo->eConnectionState ==
		    MEDIA_STATE_CONNECTED)
			mddpNotifyDrvTxd(prAdapter,
					 prAisBssInfo->prStaRecOfAP,
					 TRUE);
	}
#if CFG_ENABLE_WIFI_DIRECT
	/* Notify SAP clients' TXD to MD */
	prP2pBssInfo = cnmGetSapBssInfo(prAdapter);
	if (prP2pBssInfo) {
		struct LINK *prClientList;
		struct STA_RECORD *prCurrStaRec;

		prClientList = &prP2pBssInfo->rStaRecOfClientList;
		LINK_FOR_EACH_ENTRY(prCurrStaRec, prClientList,
				    rLinkEntry, struct STA_RECORD) {
			if (!prCurrStaRec)
				break;
			mddpNotifyDrvTxd(prAdapter,
					 prCurrStaRec,
					 TRUE);
		}

		prSapBssInfo = cnmGetOtherSapBssInfo(prAdapter,
						     prP2pBssInfo);
		if (prSapBssInfo) {
			struct LINK *prClientList;
			struct STA_RECORD *prCurrStaRec;

			prClientList =
				&prSapBssInfo->rStaRecOfClientList;
			LINK_FOR_EACH_ENTRY(prCurrStaRec, prClientList,
					    rLinkEntry, struct STA_RECORD) {
				if (!prCurrStaRec)
					break;
				mddpNotifyDrvTxd(prAdapter,
						 prCurrStaRec,
						 TRUE);
			}

			prSapBssInfo = cnmGetOtherSapBssInfo(prAdapter,
					prP2pBssInfo);
			if (prSapBssInfo) {
				struct LINK *prClientList;
				struct STA_RECORD *prCurrStaRec;

				prClientList =
					&prSapBssInfo->rStaRecOfClientList;
				LINK_FOR_EACH_ENTRY(prCurrStaRec, prClientList,
						rLinkEntry, struct STA_RECORD) {
					if (!prCurrStaRec)
						break;
					mddpNotifyDrvTxd(prAdapter,
							prCurrStaRec,
							TRUE);
				}
			}
		}
	}
#endif
	return 0;
}

int32_t mddpMdNotifyInfoHandleDrvException(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct wsvc_md_event_exception_t *event;

	if (prMdInfo->buf_len != sizeof(struct wsvc_md_event_exception_t)) {
		DBGLOG(INIT, ERROR,
		       "Invalid args from MD, expect %lu but %u\n",
		       sizeof(struct wsvc_md_event_exception_t),
		       prMdInfo->buf_len);
		return -EINVAL;
	}
	event = (struct wsvc_md_event_exception_t *)
		&(prMdInfo->buf[1]);
	DBGLOG(INIT, WARN, "reason: %d, flag: %d, line: %d, func: %s\n",
	       event->u4RstReason,
	       event->u4RstFlag,
	       event->u4Line,
	       event->pucFuncName);
	mddpTriggerReset(prAdapter, event->u4RstFlag);

	return 0;
}

int32_t mddpMdNotifyInfoHandleCommunication(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GLUE_INFO *prGlueInfo;
	struct BUS_INFO *prBusInfo;
	struct wsvc_md_event_comm_t *event;

	prGlueInfo = prAdapter->prGlueInfo;
	prBusInfo = prAdapter->chip_info->bus_info;

	if (prMdInfo->buf_len != sizeof(struct wsvc_md_event_comm_t)) {
		DBGLOG(INIT, ERROR,
		       "Invalid args from MD, expect %lu but %u\n",
		       sizeof(struct wsvc_md_event_comm_t),
		       prMdInfo->buf_len);
		return -EINVAL;
	}
	event = (struct wsvc_md_event_comm_t *)
		&(prMdInfo->buf[1]);
	if (event->u4Reason == MD_L12_DISABLE ||
	    event->u4Reason == MD_L12_ENABLE) {
		DBGLOG_LIMITED(INIT, WARN,
			       "reason:%d, flag:%d, line:%d, func:%s, bssIdx:%d\n",
			       event->u4Reason,
			       event->u4RstFlag,
			       event->u4Line,
			       event->pucFuncName,
			       event->dump_payload[1]);
	} else {
		DBGLOG(INIT, WARN,
		       "reason:%d, flag:%d, line:%d, func:%s, bssIdx:%d\n",
		       event->u4Reason,
		       event->u4RstFlag,
		       event->u4Line,
		       event->pucFuncName,
		       event->dump_payload[1]);
	}
	if (event->u4Reason == MD_TX_DATA_TIMEOUT) {
		prAdapter->u4HifChkFlag |= HIF_CHK_TX_TIMEOUT;
		prAdapter->u4HifChkFlag |= HIF_CHK_MD_TX_TIMEOUT;
		prAdapter->ucMddpBssIndex =
			(uint8_t) event->dump_payload[1];
		kalSetHifDbgEvent(prGlueInfo);
#if defined(_HIF_PCIE)
#if CFG_SUPPORT_PCIE_ASPM
	} else if (event->u4Reason == MD_L12_DISABLE) {
		/* disable PCIe L1.2, 2 means md config */
		if (prBusInfo->configPcieAspm) {
			prBusInfo->configPcieAspm(prGlueInfo, FALSE, 2);
			mddpNotifyMDPCIeL12Status(0);
		}
	} else if (event->u4Reason == MD_L12_ENABLE) {
		/* enable PCIe L1.2, 2 means md config */
		if (prBusInfo->configPcieAspm) {
			prBusInfo->configPcieAspm(prGlueInfo, TRUE, 2);
			mddpNotifyMDPCIeL12Status(1);
		}
#endif
#endif
	} else if (event->u4Reason == MD_INFORMATION_DUMP ||
		   event->u4Reason == MD_DRV_OWN_FAIL ||
		   event->u4Reason == MD_INIT_FAIL ||
		   event->u4Reason == MD_STATE_ABNORMAL ||
		   event->u4Reason == MD_SER_NO_RSP ||
		   event->u4Reason == MD_TX_CMD_FAIL) {
		mddpTriggerReset(prAdapter, event->u4RstFlag);
	} else {
		DBGLOG(INIT, WARN,
		       "MD event reason undefined reason:%d\n",
		       event->u4Reason);
	}

	return 0;
}

int32_t mddpMdNotifyInfoHandleTrigSerReq(
	struct ADAPTER *prAdapter,
	struct mddpw_md_notify_info_t *prMdInfo)
{
	struct macod_trig_ser_req_t *event;

	if (prMdInfo->buf_len != sizeof(struct macod_trig_ser_req_t)) {
		DBGLOG(INIT, ERROR,
		       "Invalid args from MD, expect %lu but %u\n",
		       sizeof(struct macod_trig_ser_req_t),
		       prMdInfo->buf_len);
		return -EINVAL;
	}
	event = (struct macod_trig_ser_req_t *)&(prMdInfo->buf[1]);
	g_u4SerSn = event->sn;
	if (prAdapter->rWifiVar.eEnableSerL1 == FEATURE_OPT_SER_ENABLE)
		mddpTriggerMdSer(prAdapter);
	else
		mddpNotifyTrigSerRspInfo(MD_TRIG_SER_NOT_SUPP);
	DBGLOG(INIT, DEBUG, "ser req ver: %u, sn: %u\n", event->ver, event->sn);

	return 0;
}

int32_t mddpMdNotifyInfo(struct mddpw_md_notify_info_t *prMdInfo)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t ret = 0;

	DBGLOG(INIT, TRACE, "[%s]MD notify\n", __func__);

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!mddpMdNotifyInfoSanityCheck(prGlueInfo)) {
		ret = -ENODEV;
		goto exit;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (prMdInfo->info_type == MDDPW_MD_INFO_RESET_IND) {
		ret = mddpMdNotifyInfoHandleResetInd(prAdapter, prMdInfo);
	} else if (prMdInfo->info_type == MDDPW_MD_INFO_DRV_EXCEPTION) {
		ret = mddpMdNotifyInfoHandleDrvException(prAdapter, prMdInfo);
	} else if (prMdInfo->info_type == MDDPW_MD_EVENT_COMMUNICATION) {
		ret = mddpMdNotifyInfoHandleCommunication(prAdapter, prMdInfo);
	} else if (prMdInfo->info_type == MDDPW_MD_INFO_TRIG_SER_REQ) {
		ret = mddpMdNotifyInfoHandleTrigSerReq(prAdapter, prMdInfo);
#if defined(_HIF_PCIE)
#if (CFG_PCIE_GEN_SWITCH == 1)
	} else if (prMdInfo->info_type == MDDPW_MD_INFO_PCIE_GENSWITCH_START) {
		mddpMdNotifyInfoHandleGenSwitchStart(prAdapter, prMdInfo);
	} else if (prMdInfo->info_type == MDDPW_MD_INFO_PCIE_GENSWITCH_END) {
		mddpMdNotifyInfoHandleGenSwitchEnd(prAdapter, prMdInfo);
	} else if (prMdInfo->info_type ==
			MDDPW_MD_INFO_PCIE_GENSWITCH_BYPASS_START) {
		mddpMdNotifyInfoHandleGenSwitchByPassStart(prAdapter, prMdInfo);
	} else if (prMdInfo->info_type ==
			MDDPW_MD_INFO_PCIE_GENSWITCH_BYPASS_END) {
		mddpMdNotifyInfoHandleGenSwitchByPassEnd(prAdapter, prMdInfo);
#endif /* CFG_PCIE_GEN_SWITCH */
#endif
	} else {
		DBGLOG(INIT, ERROR, "unknown MD info type: %d\n",
			prMdInfo->info_type);
		ret = -ENODEV;
		goto exit;
	}

exit:
	return ret;
}

int32_t mddpChangeState(enum mddp_state_e event, void *buf, uint32_t *buf_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	u_int8_t fgHalted = FALSE;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return 0;
	}

	fgHalted = kalIsHalted(prGlueInfo);
	if (fgHalted || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "fgHalted: %d, u4ReadyFlag: %d\n",
				fgHalted, prGlueInfo->u4ReadyFlag);
		return 0;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return 0;
	}

	switch (event) {
	case MDDP_STATE_ENABLING:
		break;

	case MDDP_STATE_ACTIVATING:
		break;

	case MDDP_STATE_ACTIVATED:
		DBGLOG(INIT, INFO, "Mddp activated.\n");
		prAdapter->fgMddpActivated = true;
		break;

	case MDDP_STATE_DEACTIVATING:
		break;

	case MDDP_STATE_DEACTIVATED:
		DBGLOG(INIT, INFO, "Mddp deactivated.\n");
		prAdapter->fgMddpActivated = false;
		break;

	case MDDP_STATE_DISABLING:
	case MDDP_STATE_UNINIT:
	case MDDP_STATE_CNT:
	case MDDP_STATE_DUMMY:
	default:
		break;
	}

	return 0;

}

static int32_t wait_for_md_off_complete(void)
{
	uint32_t u4Value = 0;
	uint32_t u4StartTime, u4CurTime;
	uint32_t u4MDOffTimeoutTime = MD_ON_OFF_TIMEOUT;
	int32_t ret = 0;

	if (mddpIsCasanFWload() == TRUE)
		u4MDOffTimeoutTime = MD_ON_OFF_TIMEOUT_CASAN;

	u4StartTime = kalGetTimeTick();

	do {
		if (g_rSettings.rOps.rd)
			g_rSettings.rOps.rd(&g_rSettings, &u4Value);

		if ((u4Value & g_rSettings.u4MdOffBit) == 0
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
		&& g_rSettings.recv_seq == GLUE_GET_REF_CNT(g_rSettings.seq)
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
		) {
			DBGLOG(INIT, INFO, "md off end.\n");
			break;
		}

		u4CurTime = kalGetTimeTick();
		if (CHECK_FOR_TIMEOUT(u4CurTime, u4StartTime,
				u4MDOffTimeoutTime)) {
			DBGLOG(INIT, ERROR, "wait for md off timeout\n");
			ret = -1;
			break;
		}

		kalMsleep(CFG_RESPONSE_POLLING_DELAY);
	} while (TRUE);

	return ret;
}

static int32_t wait_for_md_on_complete(void)
{
	uint32_t u4Value = 0;
	uint32_t u4StartTime, u4CurTime;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4MDOnTimeoutTime = MD_ON_OFF_TIMEOUT;
	int32_t ret = 0;

	u4StartTime = kalGetTimeTick();
	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return -1;
	}
	if (mddpIsCasanFWload() == TRUE)
		u4MDOnTimeoutTime = MD_ON_OFF_TIMEOUT_CASAN;

	do {
		if (g_rSettings.rOps.rd)
			g_rSettings.rOps.rd(&g_rSettings, &u4Value);

		if ((u4Value & g_rSettings.u4MdOnBit) > 0) {
			DBGLOG(INIT, INFO, "md on end.\n");
			break;
		} else if (!prGlueInfo->u4ReadyFlag) {
			DBGLOG(INIT, WARN, "Skip waiting due to ready flag.\n");
			ret = -1;
			break;
		}

		u4CurTime = kalGetTimeTick();
		if (CHECK_FOR_TIMEOUT(u4CurTime, u4StartTime,
				u4MDOnTimeoutTime)) {
			DBGLOG(INIT, ERROR, "wait for md on timeout\n");
			ret = -1;
			break;
		}

		kalMsleep(CFG_RESPONSE_POLLING_DELAY);
	} while (TRUE);

	return ret;
}

void setMddpSupportRegister(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
#if (CFG_SUPPORT_CONNAC2X == 0 && CFG_SUPPORT_CONNAC3X == 0)
	uint32_t u4Val = 0;
#endif

	prChipInfo = prAdapter->chip_info;

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	g_rSettings.rOps.rd = mddpRdCCCI;
	g_rSettings.u4MdOnBit = MD_STATUS_ON_SYNC_BIT;
	g_rSettings.u4MdOffBit = MD_STATUS_OFF_SYNC_BIT;
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	if (prChipInfo->isSupportMddpSHM) {
		g_rSettings.rOps.rd = mddpRdFuncSHM;
		g_rSettings.rOps.set = mddpSetFuncSHM;
		g_rSettings.rOps.clr = mddpClrFuncSHM;
		g_rSettings.u4MdInitBit = MD_SHM_MD_INIT_BIT;
		g_rSettings.u4MdOnBit = MD_SHM_MD_ON_BIT;
		g_rSettings.u4MdOffBit = MD_SHM_MD_OFF_BIT;
		g_rSettings.u4WifiOnBit = MD_SHM_WIFI_ON_BIT;
		g_rSettings.u4MDDPSupportMode = MDDP_SUPPORT_SHM;
	} else if (prChipInfo->isSupportMddpAOR) {
		g_rSettings.rOps.rd = mddpRdFunc;
		g_rSettings.rOps.set = mddpSetFuncV2;
		g_rSettings.rOps.clr = mddpClrFuncV2;
		g_rSettings.u4SyncAddr = MD_AOR_RD_CR_ADDR;
		g_rSettings.u4SyncSetAddr = MD_AOR_SET_CR_ADDR;
		g_rSettings.u4SyncClrAddr = MD_AOR_CLR_CR_ADDR;
		g_rSettings.u4MdInitBit = MD_AOR_MD_INIT_BIT;
		g_rSettings.u4MdOnBit = MD_AOR_MD_ON_BIT;
		g_rSettings.u4MdOffBit = MD_AOR_MD_OFF_BIT;
		g_rSettings.u4WifiOnBit = MD_AOR_WIFI_ON_BIT;
		g_rSettings.u4MDDPSupportMode = MDDP_SUPPORT_AOP;
	} else {
		g_rSettings.rOps.rd = mddpRdFunc;
		g_rSettings.rOps.set = mddpSetFuncV1;
		g_rSettings.rOps.clr = mddpClrFuncV1;
		g_rSettings.u4SyncAddr = MD_STATUS_SYNC_CR;
		g_rSettings.u4MdOnBit = MD_STATUS_ON_SYNC_BIT;
		g_rSettings.u4MdOffBit = MD_STATUS_OFF_SYNC_BIT;
		g_rSettings.u4MDDPSupportMode = MDDP_SUPPORT_AOP;
	}
	if (prChipInfo->u4MdDrvOwnTimeoutTime)
		g_rSettings.u4MdDrvOwnTimeoutTime =
			prChipInfo->u4MdDrvOwnTimeoutTime;
	else
		g_rSettings.u4MdDrvOwnTimeoutTime =
			LP_OWN_BACK_TOTAL_DELAY_MD_MS;
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

#if (CFG_SUPPORT_CONNAC2X == 0 && CFG_SUPPORT_CONNAC3X == 0)
	HAL_MCR_RD(prAdapter, MDDP_SUPPORT_CR, &u4Val);
	if (g_fgMddpEnabled)
		u4Val |= MDDP_SUPPORT_CR_BIT;
	else
		u4Val &= ~MDDP_SUPPORT_CR_BIT;
	HAL_MCR_WR(prAdapter, MDDP_SUPPORT_CR, u4Val);
#endif
}

void mddpInit(int bootmode)
{
	/* failed to get bootmode */
	if (bootmode == -1)
		return;

	g_wifi_boot_mode = bootmode;
	g_eMddpStatus = MDDPW_DRV_INFO_STATUS_OFF_END;
	mutex_init(&rMddpLock);
	mddpRegisterCb();
}

void mddpUninit(void)
{
	g_fgMddpEnabled = FALSE;
	mddpUnregisterCb();
}

static void notifyMdCrash2FW(void)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "Invalid drv state.\n");
		return;
	}

	if (g_rSettings.u4MDDPSupportMode == MDDP_SUPPORT_SHM) {
		if (g_rSettings.rOps.clr)
			g_rSettings.rOps.clr(&g_rSettings,
				g_rSettings.u4MdOnBit);
	}

	/*
	 * Set MD FW Own before Notify FW MD crash
	 * Reason: MD cannt set FW own itself when MD crash
	 */
	KAL_SET_BIT(MDDP_HIF_NOTIFY_MD_CRASH_2_FW, g_ulMddpActionFlag);
	KAL_SET_BIT(MDDP_HIF_MD_FW_OWN, g_ulMddpActionFlag);
	kalSetMddpEvent(prGlueInfo);
}

void mddpNotifyMdCrash(struct ADAPTER *prAdapter)
{
	DBGLOG(HAL, DEBUG, "halNotifyMdCrash.\n");
	halTriggerSwInterrupt(prAdapter, MCU_INT_NOTIFY_MD_CRASH);
}

void mddpInHifThread(struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	if (KAL_TEST_AND_CLEAR_BIT(
		    MDDP_HIF_NOTIFY_MD_CRASH_2_FW,
		    g_ulMddpActionFlag))
		mddpNotifyMdCrash(prAdapter);

	if (KAL_TEST_AND_CLEAR_BIT(
		    MDDP_HIF_MD_FW_OWN,
		    g_ulMddpActionFlag))
		mddpSetMDFwOwn();

	if (KAL_TEST_AND_CLEAR_BIT(
		    MDDP_HIF_SER_RECOVERY,
		    g_ulMddpActionFlag) &&
	    prBusInfo->getMdSwIntSta)
		mddpNotifyCheckSer(prBusInfo->getMdSwIntSta(prAdapter));

	if (KAL_TEST_AND_CLEAR_BIT(
		    MDDP_HIF_TRIGGER_RESET,
		    g_ulMddpActionFlag)) {
		glSetRstReason(RST_MDDP_MD_TRIGGER_EXCEPTION);
		GL_USER_DEFINE_RESET_TRIGGER(prAdapter,
			RST_MDDP_MD_TRIGGER_EXCEPTION,
			g_u4MddpRstFlag | RST_FLAG_DO_CORE_DUMP);
	}

	if (KAL_TEST_AND_CLEAR_BIT(
		    MDDP_HIF_MD_SER,
		    g_ulMddpActionFlag)) {
#if (CFG_PCIE_GEN_SWITCH == 1)
		if (prAdapter->fgIsGenSwitchProcessing) {
			mddpNotifyTrigSerRspInfo(MD_TRIG_SER_FAIL);
		} else
#endif /* CFG_PCIE_GEN_SWITCH */
		{
			halSetDrvSer(prAdapter);
			mddpNotifyTrigSerRspInfo(MD_TRIG_SER_SUCC);
		}
	}

#if (CFG_PCIE_GEN_SWITCH == 1)
	if (KAL_TEST_AND_CLEAR_BIT(
		    MDDP_HIF_GEN_SWITCH_END,
		    g_ulMddpActionFlag)) {
		__mddpNotifyMDGenSwitchEnd(prAdapter);
	}
#endif /* CFG_PCIE_GEN_SWITCH */
}

void mddpTriggerMdFwOwnByFw(struct ADAPTER *prAdapter)
{
	if (g_fgIsMdCrash) {
		KAL_SET_BIT(MDDP_HIF_MD_FW_OWN, g_ulMddpActionFlag);
		kalSetMddpEvent(prAdapter->prGlueInfo);
	}
}

void mddpTriggerMdSerRecovery(struct ADAPTER *prAdapter)
{
	KAL_SET_BIT(MDDP_HIF_SER_RECOVERY, g_ulMddpActionFlag);
	kalSetMddpEvent(prAdapter->prGlueInfo);
}

void mddpTriggerReset(struct ADAPTER *prAdapter, uint32_t u4RstFlag)
{
	g_u4MddpRstFlag = u4RstFlag;
	KAL_SET_BIT(MDDP_HIF_TRIGGER_RESET, g_ulMddpActionFlag);
	kalSetMddpEvent(prAdapter->prGlueInfo);
}

void mddpTriggerMdSer(struct ADAPTER *prAdapter)
{
	KAL_SET_BIT(MDDP_HIF_MD_SER, g_ulMddpActionFlag);
	kalSetMddpEvent(prAdapter->prGlueInfo);
}

#if CFG_MTK_CCCI_SUPPORT
void  mddpMdStateChangedCb(enum MD_STATE old_state,
		enum MD_STATE new_state)
{
	DBGLOG(INIT, TRACE, "old_state: %d, new_state: %d.\n",
			old_state, new_state);
#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
	if (new_state == GATED || new_state == EXCEPTION) {
		mddpMdStateChangeReleaseDrvOwn();
		DBGLOG(INIT, DEBUG, "release drv own");
	}
#else /* CFG_MTK_SUPPORT_LIGHT_MDDP */
	switch (new_state) {
	case GATED: /* MD off */
		notifyMdCrash2FW();
#if defined(_HIF_PCIE)
#if (CFG_PCIE_GEN_SWITCH == 1)
		mddpHandleGenSwitchMdExp(GATED);
#endif /* CFG_PCIE_GEN_SWITCH */
#endif
		break;
	case EXCEPTION: /* MD crash */
		notifyMdCrash2FW();
#if defined(_HIF_PCIE)
#if (CFG_PCIE_GEN_SWITCH == 1)
		mddpHandleGenSwitchMdExp(EXCEPTION);
#endif /* CFG_PCIE_GEN_SWITCH */
#endif
		g_fgIsMdCrash = TRUE;
		break;
	default:
		break;
	}
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */
}
#endif /* CFG_MTK_CCCI_SUPPORT */

static void save_mddp_stats(void)
{
	struct mddpw_net_stat_ext_t temp;
	uint8_t i = 0;
	int32_t ret;

	if (!gMddpWFunc.get_net_stat_ext)
		return;

	ret = gMddpWFunc.get_net_stat_ext(&temp);
	if (ret != 0) {
		DBGLOG(INIT, ERROR, "get_net_stat fail, ret: %d.\n", ret);
		return;
	}

	for (i = 0; i < NW_IF_NUM_MAX; i++) {
		struct mddpw_net_stat_elem_ext_t *element, *curr;

		element = &temp.ifs[0][i];
		curr = &stats.ifs[0][i];

		curr->rx_packets += element->rx_packets;
		curr->tx_packets += element->tx_packets;
		curr->rx_bytes += element->rx_bytes;
		curr->tx_bytes += element->tx_bytes;
		curr->rx_errors += element->rx_errors;
		curr->tx_errors += element->tx_errors;
		curr->rx_dropped += element->rx_dropped;
		curr->tx_dropped += element->tx_dropped;
	}
}

void mddpSetMDFwOwn(void)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	unsigned int u4MdLpctlAddr = 0;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "Invalid drv state.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return;
	}

	prChipInfo = prAdapter->chip_info;

#if defined(_HIF_PCIE)
	if (prChipInfo->bus_info->hwControlVote)
		prChipInfo->bus_info->hwControlVote(prAdapter,
			FALSE, PCIE_VOTE_USER_MDDP);
#endif

#ifdef MD_LPCTL_ADDR
	u4MdLpctlAddr = MD_LPCTL_ADDR;
#endif /* MD_LPCTL_ADDR */
	if (prChipInfo->u4MdLpctlAddr)
		u4MdLpctlAddr = prChipInfo->u4MdLpctlAddr;
	kalDevRegWrite(NULL, u4MdLpctlAddr, MDDP_LPCR_MD_SET_FW_OWN);
	DBGLOG(INIT, INFO, "Set MD Fw Own.\n");

#if defined(_HIF_PCIE)
	if (prChipInfo->bus_info->hwControlVote)
		prChipInfo->bus_info->hwControlVote(prAdapter,
			TRUE, PCIE_VOTE_USER_MDDP);
#endif
}

void mddpEnableMddpSupport(void)
{
	if (!g_fgMddpEnabled)
		mddpRegisterCb();

	g_fgMddpWifiEnabled = TRUE;
}

void mddpDisableMddpSupport(void)
{
	if (gMddpFunc.wifi_handle)
		mddpUnregisterCb();

	g_fgMddpWifiEnabled = FALSE;
}

bool mddpIsSupportMcifWifi(void)
{
	int32_t i4Feature = 0;

	if (!g_fgMddpWifiEnabled)
		return false;

	if (!gMddpWFunc.get_mddp_feature) {
		DBGLOG_LIMITED(INIT, LOUD, "gMddpWFunc not register\n");
		return false;
	}

	if (!g_fgMddpEnabled) {
		DBGLOG_LIMITED(INIT, INFO, "mddp enable: %u.\n",
			       g_fgMddpEnabled);
		return false;
	}

	i4Feature = gMddpWFunc.get_mddp_feature();
	if ((i4Feature & MDDP_FEATURE_MCIF_WIFI) == 0) {
		DBGLOG_LIMITED(INIT, INFO, "feature: %d.\n", i4Feature);
		return false;
	}

	return true;
}

bool mddpIsSupportMddpWh(void)
{
	if (!gMddpWFunc.get_mddp_feature || !g_fgMddpEnabled)
		return false;

	return (gMddpWFunc.get_mddp_feature() & MDDP_FEATURE_MDDP_WH) != 0;
}

#if (CFG_MTK_SUPPORT_LIGHT_MDDP == 1)
static void mddpRdCCCI(struct MDDP_SETTINGS *prSettings, uint32_t *pu4Val)
{
	uint32_t md_status = 0;
	*pu4Val = prSettings->u4MdOnBit | prSettings->u4MdOffBit;

	md_status = GLUE_GET_REF_CNT(prSettings->md_status);
	if (md_status & prSettings->u4MdOffBit)
		*pu4Val &= ~prSettings->u4MdOffBit;

	DBGLOG(INIT, DEBUG, "value:%u md_status:%d\n", *pu4Val, md_status);
}

static int coex_read_data_from_md(int index, char *buf, size_t count)
{
	int ret = 0;
	int retry_cnt = 0;

	do {
		ret = mtk_ccci_read_data(index, buf, count);
		if (ret < 0 || ret > count) {
			retry_cnt++;
			msleep(CHECK_MD_STATUS_TIME);
		} else {
			DBGLOG(INIT, DEBUG,
				"retry count = %d, recv data from MD success\n",
				retry_cnt);
			DBGLOG_MEM32(INIT, DEBUG, buf, ret);
			return ret;
		}
	} while (retry_cnt < CHECK_MD_STATUS_MAX_COUNT);
	DBGLOG(INIT, WARN, "recv data from MD failed\n");
	return STATUS_FAILURE;
}

static int coex_send_data_to_md(int index, char *buf, size_t count)
{
	int ret = 0;
	int retry_cnt = 0;

	do {
		ret = mtk_ccci_write_data(index, buf, count);
		if (ret < 0 || ret > count) {
			retry_cnt++;
			msleep(CHECK_MD_STATUS_TIME);
		} else {
			DBGLOG(INIT, STATE,
				"retry count = %d, send to MD success\n",
				retry_cnt);
			DBGLOG_MEM32(INIT, DEBUG, buf, ret);
			return STATUS_SUCCESS;
		}
	} while (retry_cnt < CHECK_MD_STATUS_MAX_COUNT);
	DBGLOG(INIT, WARN, "send to MD failed\n");
	return STATUS_FAILURE;
}

static void mddpMDDrvOwnReqHdlr(struct mddpw_md_notify_info_t *md_info)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct mddpw_drv_own_info_t *drv_own_info = NULL;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "[MDDP] Invalid drv state.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "[MDDP] prAdapter is NULL.\n");
		return;
	}

	drv_own_info = (struct mddpw_drv_own_info_t *) &(md_info->buf[0]);

	DBGLOG(INIT, ERROR, "[MDDP] device_id:%d, seq_num:%d\n",
		drv_own_info->device_id, drv_own_info->seq_num);

	if (drv_own_info->device_id != 0) {
		DBGLOG(INIT, ERROR, "[MDDP] Unkownn Device.\n");
		return;
	}

	g_rSettings.drv_own_seq = drv_own_info->seq_num;
	g_rSettings.is_resp_drv_own = 1;

	if (g_rSettings.is_drv_own_acquired == TRUE) {
		DBGLOG(INIT, WARN, "[MDDP] Drv own is already acquired.\n");
		return;
	}

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter,
				DRV_OWN_SRC_MD_DRV_OWN_REQ);
	if (prAdapter->fgIsFwOwn == FALSE) {
		DBGLOG(INIT, DEBUG, "[MDDP] Already Drv Owned.\n");
		mddpNotifyDrvOwn(STATUS_SUCCESS);
	}
}

static void mddpMDDrvOwnReleaseHdlr(
		struct mddpw_md_notify_info_t *md_info)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(INIT, ERROR, "Invalid drv state.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return;
	}

	if (g_rSettings.is_drv_own_acquired == FALSE) {
		DBGLOG(INIT, WARN, "[MDDP] Drv own not acquired.\n");
		return;
	}

	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE,
		DRV_OWN_SRC_MD_DRV_OWN_REQ);
	g_rSettings.is_drv_own_acquired = FALSE;
}

static void md_rx_msg_handle(struct MDDP_SETTINGS *pSt,
		struct mdfpm_ctrl_msg_t *msg, unsigned long msg_len)
{
	struct mddpw_md_notify_info_t *md_info;
	struct GLUE_INFO *prGlueInfo = NULL;
	u_int8_t fgIsForceNotify = TRUE;

	DBGLOG(INIT, DEBUG, "[MDDP] => user_id:%d, msg_id:%d\n",
		msg->dest_user_id, msg->msg_id);

	if (msg->dest_user_id != CCCI_USER_ID_COEX) {
		DBGLOG(INIT, ERROR, "unaccepted user_id(%d)!\n",
			msg->dest_user_id);
		return;
	}

	switch (msg->msg_id) {
	case CCCI_MSG_ID_RESET_IND:
		/* don't notify md when wifi off */
		if (g_eMddpStatus == MDDPW_DRV_INFO_STATUS_OFF_START)
			fgIsForceNotify = FALSE;
		DBGLOG(INIT, STATE, "received RESET IND %u\n", fgIsForceNotify);
		mddpNotifyWifiOnStart(fgIsForceNotify);
		WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
		if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
			DBGLOG(INIT, ERROR, "Invalid drv state.\n");
			return;
		}
		mddpNotifyWifiOnEnd(fgIsForceNotify);
		break;
	case CCCI_MSG_ID_MD_NOTIFY:
		DBGLOG(INIT, DEBUG, "received MD NOTIFY\n");
		md_info = (struct mddpw_md_notify_info_t *) msg->buf;
		switch (md_info->info_type) {
		case MD_NOTIFY_INFO_ONOFF:
			kalMemCopy(&pSt->recv_seq, md_info->buf,
				sizeof(pSt->recv_seq));
			DBGLOG(INIT, DEBUG, "get seq = %d from MD\n",
				pSt->recv_seq);
			GLUE_SET_REF_CNT(pSt->u4MdOffBit, pSt->md_status);
			break;
		case MD_NOTIFY_INFO_DRV_OWN_RELEASE:
			mddpMDDrvOwnReleaseHdlr(md_info);
			break;
		default:
			DBGLOG(INIT, WARN, "unsupport info type %d from MD.\n",
				md_info->info_type);
			break;
		}
		break;
	case CCCI_MSG_ID_MD_REQ:
		md_info = (struct mddpw_md_notify_info_t *) msg->buf;
		mddpMDDrvOwnReqHdlr(md_info);
		break;
	default:
		DBGLOG(INIT, WARN, "unsupport RSP MDG_ID[%d] from MD.\n",
			msg->msg_id);
		break;
	}
}

int32_t mddpDrvNotifyInfo(struct mddpw_drv_notify_info_t *prDrvInfo)
{
	struct mdfpm_ctrl_msg_t msg;
	unsigned int header_size = sizeof(msg) - MDFPM_TTY_BUF_SZ;

	msg.dest_user_id = CCCI_USER_ID_COEX;
	msg.msg_id = CCCI_MSG_ID_COEX_NOTIFY;
	msg.buf_len = prDrvInfo->buf_len +
			sizeof(struct mddpw_drv_notify_info_t);
	if (msg.buf_len > MDFPM_TTY_BUF_SZ) {
		DBGLOG(INIT, ERROR, "buf len(%d) error!\n", msg.buf_len);
		return STATUS_FAILURE;
	}

	memcpy(msg.buf, prDrvInfo, msg.buf_len);

	return coex_send_data_to_md(g_rSettings.i4PortIdx, (char *)&msg,
				msg.buf_len + header_size);
}

int md_rx_handler(void *data)
{
	int ret = 0;
	char *msg = NULL;
	enum MD_STATE md_state;
	bool *was_frozen = FALSE;
	unsigned long msg_len = 0;
	struct MDDP_SETTINGS *pSt = data;

	if (!pSt) {
		DBGLOG(INIT, ERROR, "global Setting is NULL!!!\n");
		return STATUS_FAILURE;
	}

	if (!pSt->is_port_open) {
		DBGLOG(INIT, ERROR, "ccci port not open!!!\n");
		return STATUS_FAILURE;
	}

	// register the interrupt signal listened by the thread
	allow_signal(SIGKILL);

	msg = (char *)(&pSt->ctrl_msg);
	msg_len = sizeof(pSt->ctrl_msg);

	do {
		if (signal_pending(current)) {
			DBGLOG(INIT, ERROR,
				"catch SIGKILL, md rx thread exit!\n");
			break;
		}

		if (kthread_should_stop()) {
			DBGLOG(INIT, ERROR, "md rx thread exit!\n");
			break;
		}

		md_state = ccci_fsm_get_md_state(0);
		if (md_state != READY) {
			/* modem not ready */
			msleep(100);
			continue;
		}

		ret = coex_read_data_from_md(pSt->i4PortIdx, msg, msg_len);
		if (ret == STATUS_FAILURE) {
			DBGLOG(INIT, LOUD, "read_data fail !!!\n");
			continue;
		}

		md_rx_msg_handle(pSt, &pSt->ctrl_msg, ret);
	} while (!kthread_freezable_should_stop(was_frozen));
	return STATUS_SUCCESS;
}

bool mddpIsSupportCcci(void)
{
	if (g_rSettings.is_port_open)
		return true;
	DBGLOG(INIT, WARN, "[MDDP] => ccci port not oepn\n");
	return false;
}

void mddpNotifyDrvOwn(uint32_t u4Status)
{
	struct mdfpm_ctrl_msg_t msg;
	struct mddpw_drv_own_t *prNotifyInfo = NULL;
	struct mddpw_drv_own_info_t *prDrvInfo = NULL;
	int32_t ret = 0;

	if (!g_rSettings.is_resp_drv_own)
		return;

	DBGLOG(INIT, DEBUG, "[MDDP] => status:%u seq_num:%d\n",
		u4Status, g_rSettings.drv_own_seq);

	if (!gMddpWFunc.notify_drv_info) {
		DBGLOG(INIT, ERROR, "notify_drv_info is NULL.\n");
		return;
	}

	msg.dest_user_id = CCCI_USER_ID_COEX;
	msg.msg_id = CCCI_MSG_ID_DRV_RSP;
	msg.buf_len = sizeof(struct mddpw_drv_own_t) +
			sizeof(struct mddpw_drv_own_info_t);

	prNotifyInfo = (struct mddpw_drv_own_t *) &msg.buf;
	prNotifyInfo->version = 0;
	prNotifyInfo->resource = 0; /* DRVOWN */
	prNotifyInfo->buf_len = sizeof(struct mddpw_drv_own_info_t);

	prDrvInfo = (struct mddpw_drv_own_info_t *) &(prNotifyInfo->buf[0]);
	prDrvInfo->device_id = 0;
	prDrvInfo->seq_num = g_rSettings.drv_own_seq;
	prDrvInfo->status = u4Status;

	ret = coex_send_data_to_md(g_rSettings.i4PortIdx, (char *)&msg,
			sizeof(msg) + msg.buf_len - MDFPM_TTY_BUF_SZ);

	DBGLOG(INIT, DEBUG,
		"[MDDP] user_id:%d msg_id:%d seq_num:%d status:%d ret:%d.\n",
		msg.dest_user_id, msg.msg_id,
		prDrvInfo->seq_num, u4Status, ret);

	g_rSettings.is_resp_drv_own = 0;
	if (u4Status == STATUS_SUCCESS)
		g_rSettings.is_drv_own_acquired = TRUE;
	else
		DBGLOG(INIT, WARN, "[MDDP] drv own failed.\n");
}

void mddpMdStateChangeReleaseDrvOwn(void)
{
	struct mddpw_md_notify_info_t *md_info = NULL;

	mddpMDDrvOwnReleaseHdlr(md_info);
}

void mddpStartMdRxThread(void)
{
	g_rSettings.notify_md_thread = kthread_run(md_rx_handler,
		(void *) &g_rSettings, "md_rx_task");
	GLUE_SET_REF_CNT(0, g_rSettings.seq);
}

void mddpStopMdRxThread(void)
{
	if (g_rSettings.notify_md_thread) {
		send_sig(SIGKILL,
			g_rSettings.notify_md_thread, SEND_SIG_SRC_KERNEL);
		DBGLOG(INIT, STATE, "send sig to stop md rx thread\n");
		kthread_stop(g_rSettings.notify_md_thread);
		g_rSettings.notify_md_thread = NULL;
	}
}

bool mddpIsMdDrvOwnAcquired(void)
{
	if (g_rSettings.is_drv_own_acquired == FALSE) {
		DBGLOG(INIT, WARN, "[MDDP] Drv own not acquired.\n");
		return FALSE;
	}
	return TRUE;
}
#endif /* CFG_MTK_SUPPORT_LIGHT_MDDP */

#endif /* CFG_MTK_MDDP_SUPPORT */
