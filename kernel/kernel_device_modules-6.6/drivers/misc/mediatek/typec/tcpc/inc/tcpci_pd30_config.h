/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __LINUX_TCPC_PD30_CONFIG_H
#define __LINUX_TCPC_PD30_CONFIG_H

/*
 * If DUT send a PD command immediately after Policy Engine is ready,
 * it may interrupt the compliance test process and getting a failed result.
 * even you make sure the CC state is SinkTxNG or SinkTxOK.
 *
 * SRC_FLOW_DELAY_STARTUP: For Ellisys
 * SNK_FLOW_DELAY_STARTUP: For MQP
 */

#define CONFIG_USB_PD_REV30_SRC_FLOW_DELAY_STARTUP	0
#define CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP	0

/* PD30 Common Feature */

#define CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL	1
#define CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE	1

#define CONFIG_USB_PD_REV30_BAT_CAP_LOCAL	1
#define CONFIG_USB_PD_REV30_BAT_CAP_REMOTE	1

#define CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL	1
#define CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE	1

#define CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL	1
#define CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE	1

#define CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL	1
#define CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE	1

#define CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL	1
#define CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE	1

#define CONFIG_USB_PD_REV30_ALERT_LOCAL		1
#define CONFIG_USB_PD_REV30_ALERT_REMOTE	1

#define CONFIG_USB_PD_REV30_STATUS_LOCAL	1
#define CONFIG_USB_PD_REV30_STATUS_REMOTE	1

#define CONFIG_USB_PD_REV30_CHUNKING_BY_PE	0

#define CONFIG_USB_PD_REV30_PPS_SINK		1
#define CONFIG_USB_PD_REV30_PPS_SOURCE		0

#if CONFIG_USB_PD_REV30_STATUS_LOCAL
#define CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP	1
#else
#define CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP	0
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */

#if CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
#define CONFIG_USB_PD_REV30_BAT_INFO	1
#else
#define CONFIG_USB_PD_REV30_BAT_INFO	0
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */

#if CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
#undef CONFIG_USB_PD_REV30_BAT_INFO
#define CONFIG_USB_PD_REV30_BAT_INFO	1
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */

#if CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
#define CONFIG_USB_PD_REV30_COUNTRY_AUTHORITY	1
#else
#define CONFIG_USB_PD_REV30_COUNTRY_AUTHORITY	0
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */

#if CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
#undef CONFIG_USB_PD_REV30_COUNTRY_AUTHORITY
#define CONFIG_USB_PD_REV30_COUNTRY_AUTHORITY	1
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */

#if CONFIG_USB_PD_REV30_ALERT_LOCAL
#define CONFIG_USB_PD_DPM_AUTO_SEND_ALERT	0
#else
#define CONFIG_USB_PD_DPM_AUTO_SEND_ALERT	0
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

#if CONFIG_USB_PD_REV30_ALERT_REMOTE
#define CONFIG_USB_PD_DPM_AUTO_GET_STATUS	1
#else
#define CONFIG_USB_PD_DPM_AUTO_GET_STATUS	0
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

#define CONFIG_MTK_HANDLE_PPS_TIMEOUT	1

#endif /* __LINUX_TCPC_PD30_CONFIG_H */
