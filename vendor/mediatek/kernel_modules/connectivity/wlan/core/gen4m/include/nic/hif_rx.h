/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "hif_rx.h"
 *    \brief  Provide HIF RX Header Information between F/W and Driver
 *
 *    N/A
 */


#ifndef _HIF_RX_H
#define _HIF_RX_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*! HIF_RX_HEADER_T */
/* DW 0, Byte 1 */
#define HIF_RX_HDR_PACKET_TYPE_MASK      BITS(0, 1)

/* DW 1, Byte 0 */
#define HIF_RX_HDR_HEADER_LEN            BITS(2, 7)
#define HIF_RX_HDR_HEADER_LEN_OFFSET     2
#define HIF_RX_HDR_HEADER_OFFSET_MASK    BITS(0, 1)

/* DW 1, Byte 1 */
#define HIF_RX_HDR_80211_HEADER_FORMAT   BIT(0)
#define HIF_RX_HDR_DO_REORDER            BIT(1)
#define HIF_RX_HDR_PAL                   BIT(2)
#define HIF_RX_HDR_TCL                   BIT(3)
#define HIF_RX_HDR_NETWORK_IDX_MASK      BITS(4, 7)
#define HIF_RX_HDR_NETWORK_IDX_OFFSET    4

/* DW 1, Byte 2, 3 */
#define HIF_RX_HDR_SEQ_NO_MASK           BITS(0, 11)
#define HIF_RX_HDR_TID_MASK              BITS(12, 14)
#define HIF_RX_HDR_TID_OFFSET            12
#define HIF_RX_HDR_BAR_FRAME             BIT(15)

#define HIF_RX_HDR_FLAG_AMP_WDS             BIT(0)
#define HIF_RX_HDR_FLAG_802_11_FORMAT       BIT(1)
#define HIF_RX_HDR_FLAG_BAR_FRAME           BIT(2)
#define HIF_RX_HDR_FLAG_DO_REORDERING       BIT(3)
#define HIF_RX_HDR_FLAG_CTRL_WARPPER_FRAME  BIT(4)

#define HIF_RX_HW_APPENDED_LEN              4

/* For DW 2, Byte 3 - ucHwChannelNum */
#define HW_CHNL_NUM_MAX_2G4                 14
#define HW_CHNL_NUM_MAX_4G_5G               (255 - HW_CHNL_NUM_MAX_2G4)
#if (CFG_SUPPORT_WIFI_6G == 1)
#define HW_CHNL_NUM_MAX_5G                  180
#define CHNL_NUM_BASE_6G                    181
#endif

/*******************************************************************************
 *                         D A T A   T Y P E S
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
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static __KAL_INLINE__ void hifDataTypeCheck(void);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/* Kevin: we don't have to call following function to inspect the data
 * structure. It will check automatically while at compile time.
 * We'll need this for porting driver to different RTOS.
 */
static __KAL_INLINE__ void hifDataTypeCheck(void)
{
}

#endif
