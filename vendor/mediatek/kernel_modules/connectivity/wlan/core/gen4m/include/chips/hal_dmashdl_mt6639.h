/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   hal_dmashdl_mt6639.h
*    \brief  DMASHDL HAL API for mt6639
*
*    This file contains all routines which are exported
     from MediaTek 802.11 Wireless LAN driver stack to GLUE Layer.
*/

#ifndef _HAL_DMASHDL_MT6639_H
#define _HAL_DMASHDL_MT6639_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* align MT7663 setting first */

#if defined(_HIF_PCIE) || defined(_HIF_AXI)

/* 1: 3rd arbitration makes decision based on group priority in current slot.
 * 0: 3rd arbitration makes decision based on fixed user-defined priority.
 */

#if (CFG_MTK_WIFI_WFDMA_4_TX_DATA_RING == 1)
#define MT6639_DMASHDL_CFG_TX_DATA_2_REFILL_EN          1
#define MT6639_DMASHDL_CFG_TX_DATA_3_REFILL_EN          1
#define MT6639_DMASHDL_CFG_TX_DATA_0_MAX_QUOTA          0x490
#define MT6639_DMASHDL_CFG_TX_DATA_1_MAX_QUOTA          0x490
#define MT6639_DMASHDL_CFG_TX_DATA_2_MAX_QUOTA          0x490
#define MT6639_DMASHDL_CFG_TX_DATA_3_MAX_QUOTA          0x490
#define MT6639_DMASHDL_CFG_TX_DATA_4_MAX_QUOTA          0x490
#define MT6639_DMASHDL_CFG_TX_DATA_5_MAX_QUOTA          0x490
#define MT6639_DMASHDL_CFG_TX_DATA_0_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_1_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_2_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_3_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_4_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_5_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_AC0_IDX              0x0
#define MT6639_DMASHDL_CFG_TX_DATA_AC1_IDX              0x1
#define MT6639_DMASHDL_CFG_TX_DATA_AC2_IDX              0x2
#define MT6639_DMASHDL_CFG_TX_DATA_AC3_IDX              0x3
#define MT6639_DMASHDL_CFG_TX_DATA_PRIO_IDX             0x4
#define MT6639_DMASHDL_CFG_TX_DATA_ALTX_IDX             0x5
#else /* CFG_MTK_WIFI_WFDMA_4_TX_DATA_RING */
#define MT6639_DMASHDL_CFG_TX_DATA_2_REFILL_EN          0
#define MT6639_DMASHDL_CFG_TX_DATA_3_REFILL_EN          0
#define MT6639_DMASHDL_CFG_TX_DATA_0_MAX_QUOTA          0x5E0
#define MT6639_DMASHDL_CFG_TX_DATA_1_MAX_QUOTA          0x5E0
#define MT6639_DMASHDL_CFG_TX_DATA_2_MAX_QUOTA          0x5E0
#define MT6639_DMASHDL_CFG_TX_DATA_3_MAX_QUOTA          0x5E0
#define MT6639_DMASHDL_CFG_TX_DATA_4_MAX_QUOTA          0
#define MT6639_DMASHDL_CFG_TX_DATA_5_MAX_QUOTA          0
#define MT6639_DMASHDL_CFG_TX_DATA_0_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_1_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_2_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_3_MIN_QUOTA          0x10
#define MT6639_DMASHDL_CFG_TX_DATA_4_MIN_QUOTA          0x0
#define MT6639_DMASHDL_CFG_TX_DATA_5_MIN_QUOTA          0x0
#define MT6639_DMASHDL_CFG_TX_DATA_AC0_IDX              0x0
#define MT6639_DMASHDL_CFG_TX_DATA_AC1_IDX              0x1
#define MT6639_DMASHDL_CFG_TX_DATA_AC2_IDX              0x0
#define MT6639_DMASHDL_CFG_TX_DATA_AC3_IDX              0x0
#define MT6639_DMASHDL_CFG_TX_DATA_PRIO_IDX             0x2
#define MT6639_DMASHDL_CFG_TX_DATA_ALTX_IDX             0x3
#endif /* CFG_MTK_WIFI_WFDMA_4_TX_DATA_RING */

#define MT6639_DMASHDL_SLOT_ARBITER_EN                 (0)
#define MT6639_DMASHDL_PKT_PLE_MAX_PAGE                (0x1)
#define MT6639_DMASHDL_PKT_PSE_MAX_PAGE                (0x18)
#define MT6639_DMASHDL_GROUP_0_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_1_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_2_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_3_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_4_REFILL_EN               \
	(MT6639_DMASHDL_CFG_TX_DATA_2_REFILL_EN)
#define MT6639_DMASHDL_GROUP_5_REFILL_EN               \
	(MT6639_DMASHDL_CFG_TX_DATA_3_REFILL_EN)
#define MT6639_DMASHDL_GROUP_6_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_7_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_8_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_9_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_10_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_11_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_12_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_13_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_14_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_15_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_0_MAX_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_0_MAX_QUOTA)
#define MT6639_DMASHDL_GROUP_1_MAX_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_1_MAX_QUOTA)
#define MT6639_DMASHDL_GROUP_2_MAX_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_2_MAX_QUOTA)
#define MT6639_DMASHDL_GROUP_3_MAX_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_3_MAX_QUOTA)
#define MT6639_DMASHDL_GROUP_4_MAX_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_4_MAX_QUOTA)
#define MT6639_DMASHDL_GROUP_5_MAX_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_5_MAX_QUOTA)
#define MT6639_DMASHDL_GROUP_6_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_7_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_8_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_9_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_10_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_11_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_12_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_13_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_14_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_15_MAX_QUOTA              (0x30)
#define MT6639_DMASHDL_GROUP_0_MIN_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_0_MIN_QUOTA)
#define MT6639_DMASHDL_GROUP_1_MIN_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_1_MIN_QUOTA)
#define MT6639_DMASHDL_GROUP_2_MIN_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_2_MIN_QUOTA)
#define MT6639_DMASHDL_GROUP_3_MIN_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_3_MIN_QUOTA)
#define MT6639_DMASHDL_GROUP_4_MIN_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_4_MIN_QUOTA)
#define MT6639_DMASHDL_GROUP_5_MIN_QUOTA               \
	(MT6639_DMASHDL_CFG_TX_DATA_5_MIN_QUOTA)
#define MT6639_DMASHDL_GROUP_6_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_7_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_8_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_9_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_10_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_11_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_12_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_13_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_14_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_15_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_QUEUE_0_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC0_IDX)   /* LMAC AC00 */
#define MT6639_DMASHDL_QUEUE_1_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC0_IDX)   /* LMAC AC01 */
#define MT6639_DMASHDL_QUEUE_2_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC0_IDX)   /* LMAC AC02 */
#define MT6639_DMASHDL_QUEUE_3_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_PRIO_IDX)   /* LMAC AC03 */
#define MT6639_DMASHDL_QUEUE_4_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC1_IDX)   /* LMAC AC10 */
#define MT6639_DMASHDL_QUEUE_5_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC1_IDX)   /* LMAC AC11 */
#define MT6639_DMASHDL_QUEUE_6_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC1_IDX)   /* LMAC AC12 */
#define MT6639_DMASHDL_QUEUE_7_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_PRIO_IDX)   /* LMAC AC13 */
#define MT6639_DMASHDL_QUEUE_8_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC2_IDX)   /* LMAC AC20 */
#define MT6639_DMASHDL_QUEUE_9_TO_GROUP                \
	(MT6639_DMASHDL_CFG_TX_DATA_AC2_IDX)   /* LMAC AC21 */
#define MT6639_DMASHDL_QUEUE_10_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_AC2_IDX)   /* LMAC AC22 */
#define MT6639_DMASHDL_QUEUE_11_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_PRIO_IDX)   /* LMAC AC23 */
#define MT6639_DMASHDL_QUEUE_12_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_AC3_IDX)   /* LMAC AC30 */
#define MT6639_DMASHDL_QUEUE_13_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_AC3_IDX)   /* LMAC AC31 */
#define MT6639_DMASHDL_QUEUE_14_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_AC3_IDX)   /* LMAC AC32 */
#define MT6639_DMASHDL_QUEUE_15_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_PRIO_IDX)   /* LMAC AC33 */
#define MT6639_DMASHDL_QUEUE_16_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_ALTX_IDX)   /* ALTX */
#define MT6639_DMASHDL_QUEUE_17_TO_GROUP               (0x0)   /* BMC */
#define MT6639_DMASHDL_QUEUE_18_TO_GROUP               (0x0)   /* BCN */
#define MT6639_DMASHDL_QUEUE_19_TO_GROUP               (0x0)   /* HW Reserved */
#define MT6639_DMASHDL_QUEUE_20_TO_GROUP               \
	(MT6639_DMASHDL_CFG_TX_DATA_ALTX_IDX)   /* TGID=1 ALTX */
#define MT6639_DMASHDL_QUEUE_21_TO_GROUP               (0x0)   /* TGID=1 BMC  */
#define MT6639_DMASHDL_QUEUE_22_TO_GROUP               (0x0)   /* TGID=1 BCN  */
#define MT6639_DMASHDL_QUEUE_23_TO_GROUP               (0x0)   /* HW Reserved */
#define MT6639_DMASHDL_QUEUE_24_TO_GROUP               (0x0)   /* NAF */
#define MT6639_DMASHDL_QUEUE_25_TO_GROUP               (0x0)   /* NBCN */
#define MT6639_DMASHDL_QUEUE_26_TO_GROUP               (0x0)   /* FIXFID */
#define MT6639_DMASHDL_QUEUE_27_TO_GROUP               (0x0)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_28_TO_GROUP               (0x0)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_29_TO_GROUP               (0x0)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_30_TO_GROUP               (0x0)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_31_TO_GROUP               (0x0)   /* Reserved */
#define MT6639_DMASHDL_PRIORITY0_GROUP                 (0x0)
#define MT6639_DMASHDL_PRIORITY1_GROUP                 (0x1)
#define MT6639_DMASHDL_PRIORITY2_GROUP                 (0x2)
#define MT6639_DMASHDL_PRIORITY3_GROUP                 (0x3)
#define MT6639_DMASHDL_PRIORITY4_GROUP                 (0x4)
#define MT6639_DMASHDL_PRIORITY5_GROUP                 (0x5)
#define MT6639_DMASHDL_PRIORITY6_GROUP                 (0x6)
#define MT6639_DMASHDL_PRIORITY7_GROUP                 (0x7)
#define MT6639_DMASHDL_PRIORITY8_GROUP                 (0x8)
#define MT6639_DMASHDL_PRIORITY9_GROUP                 (0x9)
#define MT6639_DMASHDL_PRIORITY10_GROUP                (0xA)
#define MT6639_DMASHDL_PRIORITY11_GROUP                (0xB)
#define MT6639_DMASHDL_PRIORITY12_GROUP                (0xC)
#define MT6639_DMASHDL_PRIORITY13_GROUP                (0xD)
#define MT6639_DMASHDL_PRIORITY14_GROUP                (0xE)
#define MT6639_DMASHDL_PRIORITY15_GROUP                (0xF)
/* 5 rings are used (4 data + cmd) */
#define MT6639_DMASHDL_HIF_ACK_CNT_TH                  (0x5)
/* Ring 0/1/2/3/15 are used */
#define MT6639_DMASHDL_HIF_GUP_ACT_MAP                 (0x800F)

#elif defined(_HIF_USB)

/* 1: 3rd arbitration makes decision based on group priority in current slot.
 * 0: 3rd arbitration makes decision based on fixed user-defined priority.
 */
#define MT6639_DMASHDL_SLOT_ARBITER_EN                 (0)
#define MT6639_DMASHDL_PKT_PLE_MAX_PAGE                (0x1)
#define MT6639_DMASHDL_PKT_PSE_MAX_PAGE                (0x18)
#define MT6639_DMASHDL_GROUP_0_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_1_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_2_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_3_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_4_REFILL_EN               (1)
#define MT6639_DMASHDL_GROUP_5_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_6_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_7_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_8_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_9_REFILL_EN               (0)
#define MT6639_DMASHDL_GROUP_10_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_11_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_12_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_13_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_14_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_15_REFILL_EN              (0)
#define MT6639_DMASHDL_GROUP_0_MAX_QUOTA               (0xFFF)
#define MT6639_DMASHDL_GROUP_1_MAX_QUOTA               (0xFFF)
#define MT6639_DMASHDL_GROUP_2_MAX_QUOTA               (0xFFF)
#define MT6639_DMASHDL_GROUP_3_MAX_QUOTA               (0xFFF)
#define MT6639_DMASHDL_GROUP_4_MAX_QUOTA               (0xFFF)
#define MT6639_DMASHDL_GROUP_5_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_6_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_7_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_8_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_9_MAX_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_10_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_11_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_12_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_13_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_14_MAX_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_15_MAX_QUOTA              (0x40)
#define MT6639_DMASHDL_GROUP_0_MIN_QUOTA               (0x3)
#define MT6639_DMASHDL_GROUP_1_MIN_QUOTA               (0x3)
#define MT6639_DMASHDL_GROUP_2_MIN_QUOTA               (0x3)
#define MT6639_DMASHDL_GROUP_3_MIN_QUOTA               (0x3)
#define MT6639_DMASHDL_GROUP_4_MIN_QUOTA               (0x3)
#define MT6639_DMASHDL_GROUP_5_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_6_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_7_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_8_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_9_MIN_QUOTA               (0x0)
#define MT6639_DMASHDL_GROUP_10_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_11_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_12_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_13_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_14_MIN_QUOTA              (0x0)
#define MT6639_DMASHDL_GROUP_15_MIN_QUOTA              (0x40)
#define MT6639_DMASHDL_QUEUE_0_TO_GROUP                (0x0)   /* LMAC AC00 */
#define MT6639_DMASHDL_QUEUE_1_TO_GROUP                (0x1)   /* LMAC AC01 */
#define MT6639_DMASHDL_QUEUE_2_TO_GROUP                (0x2)   /* LMAC AC02 */
#define MT6639_DMASHDL_QUEUE_3_TO_GROUP                (0x3)   /* LMAC AC03 */
#define MT6639_DMASHDL_QUEUE_4_TO_GROUP                (0x4)   /* LMAC AC10 */
#define MT6639_DMASHDL_QUEUE_5_TO_GROUP                (0x4)   /* LMAC AC11 */
#define MT6639_DMASHDL_QUEUE_6_TO_GROUP                (0x4)   /* LMAC AC12 */
#define MT6639_DMASHDL_QUEUE_7_TO_GROUP                (0x4)   /* LMAC AC13 */
#define MT6639_DMASHDL_QUEUE_8_TO_GROUP                (0x4)   /* LMAC AC20 */
#define MT6639_DMASHDL_QUEUE_9_TO_GROUP                (0x4)   /* LMAC AC21 */
#define MT6639_DMASHDL_QUEUE_10_TO_GROUP               (0x4)   /* LMAC AC22 */
#define MT6639_DMASHDL_QUEUE_11_TO_GROUP               (0x4)   /* LMAC AC23 */
#define MT6639_DMASHDL_QUEUE_12_TO_GROUP               (0x4)   /* LMAC AC30 */
#define MT6639_DMASHDL_QUEUE_13_TO_GROUP               (0x4)   /* LMAC AC31 */
#define MT6639_DMASHDL_QUEUE_14_TO_GROUP               (0x4)   /* LMAC AC32 */
#define MT6639_DMASHDL_QUEUE_15_TO_GROUP               (0x4)   /* LMAC AC33 */
#define MT6639_DMASHDL_QUEUE_16_TO_GROUP               (0x3)   /* ALTX0 */
						/* (q_idex=16, tgid=0) */
#define MT6639_DMASHDL_QUEUE_17_TO_GROUP               (0x0)   /* BMC */
#define MT6639_DMASHDL_QUEUE_18_TO_GROUP               (0x0)   /* BCN */
#define MT6639_DMASHDL_QUEUE_19_TO_GROUP               (0x1)   /* HW Reserved */
#define MT6639_DMASHDL_QUEUE_20_TO_GROUP               (0x3)   /* ALTX1 */
						/* (q_idex=16, tgid=1) */
#define MT6639_DMASHDL_QUEUE_21_TO_GROUP               (0x1)   /* HW Reserved */
#define MT6639_DMASHDL_QUEUE_22_TO_GROUP               (0x1)   /* HW Reserved */
#define MT6639_DMASHDL_QUEUE_23_TO_GROUP               (0x1)   /* HW Reserved */
#define MT6639_DMASHDL_QUEUE_24_TO_GROUP               (0x0)   /* NAF */
#define MT6639_DMASHDL_QUEUE_25_TO_GROUP               (0x0)   /* NBCN */
#define MT6639_DMASHDL_QUEUE_26_TO_GROUP               (0x0)   /* FIXFID */
#define MT6639_DMASHDL_QUEUE_27_TO_GROUP               (0x1)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_28_TO_GROUP               (0x1)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_29_TO_GROUP               (0x1)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_30_TO_GROUP               (0x1)   /* Reserved */
#define MT6639_DMASHDL_QUEUE_31_TO_GROUP               (0x1)   /* Reserved */
#define MT6639_DMASHDL_PRIORITY0_GROUP                 (0x3)
#define MT6639_DMASHDL_PRIORITY1_GROUP                 (0x2)
#define MT6639_DMASHDL_PRIORITY2_GROUP                 (0x1)
#define MT6639_DMASHDL_PRIORITY3_GROUP                 (0x0)
#define MT6639_DMASHDL_PRIORITY4_GROUP                 (0x4)
#define MT6639_DMASHDL_PRIORITY5_GROUP                 (0x5)
#define MT6639_DMASHDL_PRIORITY6_GROUP                 (0x6)
#define MT6639_DMASHDL_PRIORITY7_GROUP                 (0x7)
#define MT6639_DMASHDL_PRIORITY8_GROUP                 (0x8)
#define MT6639_DMASHDL_PRIORITY9_GROUP                 (0x9)
#define MT6639_DMASHDL_PRIORITY10_GROUP                (0xA)
#define MT6639_DMASHDL_PRIORITY11_GROUP                (0xB)
#define MT6639_DMASHDL_PRIORITY12_GROUP                (0xC)
#define MT6639_DMASHDL_PRIORITY13_GROUP                (0xD)
#define MT6639_DMASHDL_PRIORITY14_GROUP                (0xE)
#define MT6639_DMASHDL_PRIORITY15_GROUP                (0xF)
#if (CFG_WIFI_FWDL_UMAC_RESERVE_SIZE_PARA == 128)
/* PSE quota 169*/
#define MT6639_DMASHDL_DBDC_5G_MAX_QUOTA               (0x38)
#define MT6639_DMASHDL_DBDC_2G_MAX_QUOTA               (0x1C)
#else
/* PSE quota 212*/
#define MT6639_DMASHDL_DBDC_5G_MAX_QUOTA               (0x46)
#define MT6639_DMASHDL_DBDC_2G_MAX_QUOTA               (0x24)
#endif

#define MT6639_DMASHDL_HIF_ACK_CNT_TH                  (0x6)
#define MT6639_DMASHDL_HIF_GUP_ACT_MAP                 (0x801F)

#endif /* defined(_HIF_PCIE) || defined(_HIF_AXI) */

enum ENUM_CONCURRENT_TYPE {
	CONCURRENT_6G_6G_AND_2G = 0,
	CONCURRENT_2G_6G_AND_2G,
	CONCURRENT_5G_5G_AND_2G,
	CONCURRENT_2G_5G_AND_2G,
	CONCURRENT_6G_6G_AND_5G,
	CONCURRENT_5G_6G_AND_5G,
	CONCURRENT_SAME_BAND,
	CONCURRENT_TYPE_NUM
};
/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
extern struct DMASHDL_CFG rMt6639DmashdlCfg;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

void mt6639DmashdlInit(struct ADAPTER *prAdapter);

#if CFG_SUPPORT_WED_PROXY
void mt6639DmashdlWedQueueMappingUpdate(struct ADAPTER *prAdapter);
#endif

#if defined(_HIF_USB)
uint32_t mt6639UpdateDmashdlQuota(struct ADAPTER *prAdapter,
			uint8_t ucWmmIndex, uint32_t u4MaxQuota);

uint32_t mt6639dmashdlQuotaDecision(struct ADAPTER *prAdapter,
			uint8_t ucWmmIndex);
#endif
#endif /* _HAL_DMASHDL_MT6639_H */
