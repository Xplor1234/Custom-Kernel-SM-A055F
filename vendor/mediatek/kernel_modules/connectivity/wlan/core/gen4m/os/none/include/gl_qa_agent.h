/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   gl_qa_agent.h
 *    \brief  This file includes private ioctl support.
 */

#ifndef _GL_QA_AGENT_H
#define _GL_QA_AGENT_H

#if CFG_SUPPORT_QA_TOOL

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

/* Trigger Event */
#define CAP_FREE_RUN		0

/* Ring Mode */
#define CAP_RING_MODE_ENABLE	1
#define CAP_RING_MODE_DISABLE	0

/* Capture Bit Width */
#define CAP_96BIT		0
#define CAP_128BIT

/* I/Q Type */
#define CAP_I_TYPE		0
#define CAP_Q_TYPE		1
#define NUM_OF_CAP_TYPE		2

/* ACTION */
#define ACTION_SWITCH_TO_RFTEST 0 /* to switch firmware mode between normal mode
				   * or rf test mode
				   */
#define ACTION_IN_RFTEST        1

#define HQA_CMD_MAGIC_NO 0x18142880
#define HQA_CHIP_ID_6632	0x6632
#define HQA_CHIP_ID_7668	0x7668

/* (4096(Samples/Bank) * 6Banks * 3(IQSamples/Sample) * 32bits)/96bits */
#define MAX_ICAP_IQ_DATA_CNT					(4096 * 8)
#define ICAP_EVENT_DATA_SAMPLE					256


#if CFG_SUPPORT_TX_BF
#define HQA_BF_STR_SIZE 512
#endif

#define HQA_RX_STATISTIC_NUM 66

#ifdef MAX_EEPROM_BUFFER_SIZE
#undef MAX_EEPROM_BUFFER_SIZE
#endif

#ifdef BUFFER_BIN_PAGE_SIZE
#undef BUFFER_BIN_PAGE_SIZE
#endif

#if defined MT7915 || defined MT7961 || defined MT7902
#define MAX_EEPROM_BUFFER_SIZE	0xe00
#define BUFFER_BIN_PAGE_SIZE 0x400
#else
//For Bellwether, Modify from 1200 to 6K and align to linux
#define MAX_EEPROM_BUFFER_SIZE	6144 //6K
#define BUFFER_BIN_PAGE_SIZE 0x400
#endif

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

extern uint8_t uacEEPROMImage[MAX_EEPROM_BUFFER_SIZE];

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

#if 0
struct PARAM_RX_STAT {
	uint32_t MacFCSErr;		/* Y 0x820F_D014 */
	uint32_t MacMdrdy;		/* Y 0x820F_D030 */
	uint32_t FCSErr_CCK;		/* Y 0x8207_021C [15:00] */
	uint32_t FCSErr_OFDM;		/* Y 0x8207_021C [31:16] */
	uint32_t CCK_PD;		/* Y 0x8207_020C [15:00] */
	uint32_t OFDM_PD;		/* Y 0x8207_020C [15:00] */
	uint32_t CCK_SIG_Err;		/* Y 0x8207_0210 [31:16] */
	uint32_t CCK_SFD_Err;		/* Y 0x8207_0210 [15:00] */
	uint32_t OFDM_SIG_Err;		/* Y 0x8207_0214 [31:16] */
	uint32_t OFDM_TAG_Err;		/* Y 0x8207_0214 [15:00] */
	uint32_t WB_RSSSI0;		/* Y 0x8207_21A8 [23:16] */
	uint32_t IB_RSSSI0;		/* Y 0x8207_21A8 [31:24] */
	uint32_t WB_RSSSI1;		/* Y 0x8207_21A8 [07:00] */
	uint32_t IB_RSSSI1;		/* Y 0x8207_21A8 [15:08] */
	uint32_t PhyMdrdyCCK;		/* Y 0x8207_0220 [15:00] */
	uint32_t PhyMdrdyOFDM;		/* Y 0x8207_0220 [31:16] */
	uint32_t DriverRxCount;		/* Y FW Counter Band0 */
	uint32_t RCPI0;			/* Y RXV4 [07:00] */
	uint32_t RCPI1;			/* Y RXV4 [15:08] */
	uint32_t FreqOffsetFromRX;	/* Y RXV5 MISC1[24:00] OFDM:[11:00]
					 *   CCK:[10:00]
					 */
	uint32_t RSSI0;			/* N */
	uint32_t RSSI1;			/* N */
	uint32_t rx_fifo_full;		/* N */
	uint32_t RxLenMismatch;		/* N */
	uint32_t MacFCSErr_band1;	/* Y 0x820F_D214 */
	uint32_t MacMdrdy_band1;	/* Y 0x820F_D230 */
	/* Y RXV3 [23:16] (must set 0x8207066C[1:0] = 0x0 ~ 0x3) */
	uint32_t FAGC_IB_RSSSI[4];
	/* Y RXV3 [31:24] (must set 0x8207066C[1:0] = 0x0 ~ 0x3) */
	uint32_t FAGC_WB_RSSSI[4];
	/* Y 0x8207_21A8 [31:24] [15:08] 0x8207_29A8 [31:24] [15:08] */
	uint32_t Inst_IB_RSSSI[4];
	/* Y 0x8207_21A8 [23:16] [07:00] 0x8207_29A8 [23:16] [07:00] */
	uint32_t Inst_WB_RSSSI[4];
	uint32_t ACIHitLow;		/* Y 0x8207_21B0 [18] */
	uint32_t ACIHitHigh;		/* Y 0x8207_29B0 [18] */
	uint32_t DriverRxCount1;	/* Y FW Counter Band1 */
	uint32_t RCPI2;			/* Y RXV4 [23:16] */
	uint32_t RCPI3;			/* Y RXV4 [31:24] */
	uint32_t RSSI2;			/* N */
	uint32_t RSSI3;			/* N */
	uint32_t SNR0;			/* Y RXV5 (MISC1 >> 19) - 16 */
	uint32_t SNR1;			/* N */
	uint32_t SNR2;			/* N */
	uint32_t SNR3;			/* N */
	uint32_t rx_fifo_full_band1;	/* N */
	uint32_t RxLenMismatch_band1;	/* N */
	uint32_t CCK_PD_band1;		/* Y 0x8207_040C [15:00] */
	uint32_t OFDM_PD_band1;		/* Y 0x8207_040C [31:16] */
	uint32_t CCK_SIG_Err_band1;	/* Y 0x8207_0410 [31:16] */
	uint32_t CCK_SFD_Err_band1;	/* Y 0x8207_0410 [15:00] */
	uint32_t OFDM_SIG_Err_band1;	/* Y 0x8207_0414 [31:16] */
	uint32_t OFDM_TAG_Err_band1;	/* Y 0x8207_0414 [15:00] */
	uint32_t PhyMdrdyCCK_band1;	/* Y 0x8207_0420 [15:00] */
	uint32_t PhyMdrdyOFDM_band1;	/* Y 0x8207_0420 [31:16] */
	uint32_t CCK_FCS_Err_band1;	/* Y 0x8207_041C [15:00] */
	uint32_t OFDM_FCS_Err_band1;	/* Y 0x8207_041C [31:16] */
	uint32_t MuPktCount;		/* Y MT_ATEUpdateRxStatistic
					 *   RXV1_2ND_CYCLE->GroupId
					 */
};
#else
#if (CFG_SUPPORT_CONNAC3X == 0) && (CFG_SUPPORT_CONNAC5X == 0)
struct PARAM_RX_STAT {
	uint32_t MAC_FCS_Err;	/* b0 */
	uint32_t MAC_Mdrdy;	/* b0 */
	uint32_t FCSErr_CCK;
	uint32_t FCSErr_OFDM;
	uint32_t CCK_PD;
	uint32_t OFDM_PD;
	uint32_t CCK_SIG_Err;
	uint32_t CCK_SFD_Err;
	uint32_t OFDM_SIG_Err;
	uint32_t OFDM_TAG_Err;
	uint32_t WB_RSSI0;
	uint32_t IB_RSSI0;
	uint32_t WB_RSSI1;
	uint32_t IB_RSSI1;
	uint32_t PhyMdrdyCCK;
	uint32_t PhyMdrdyOFDM;
	uint32_t DriverRxCount;
	uint32_t RCPI0;
	uint32_t RCPI1;
	uint32_t FreqOffsetFromRX;
	uint32_t RSSI0;
	uint32_t RSSI1;		/* insert new member here */
	uint32_t OutOfResource;	/* MT7615 begin here */
	uint32_t LengthMismatchCount_B0;
	uint32_t MAC_FCS_Err1;	/* b1 */
	uint32_t MAC_Mdrdy1;	/* b1 */
	uint32_t FAGCRssiIBR0;
	uint32_t FAGCRssiIBR1;
	uint32_t FAGCRssiIBR2;
	uint32_t FAGCRssiIBR3;
	uint32_t FAGCRssiWBR0;
	uint32_t FAGCRssiWBR1;
	uint32_t FAGCRssiWBR2;
	uint32_t FAGCRssiWBR3;

	uint32_t InstRssiIBR0;
	uint32_t InstRssiIBR1;
	uint32_t InstRssiIBR2;
	uint32_t InstRssiIBR3;
	uint32_t InstRssiWBR0;
	uint32_t InstRssiWBR1;
	uint32_t InstRssiWBR2;
	uint32_t InstRssiWBR3;
	uint32_t ACIHitLower;
	uint32_t ACIHitUpper;
	uint32_t DriverRxCount1;
	uint32_t RCPI2;
	uint32_t RCPI3;
	uint32_t RSSI2;
	uint32_t RSSI3;
	uint32_t SNR0;
	uint32_t SNR1;
	uint32_t SNR2;
	uint32_t SNR3;
	uint32_t OutOfResource1;
	uint32_t LengthMismatchCount_B1;
	uint32_t CCK_PD_Band1;
	uint32_t OFDM_PD_Band1;
	uint32_t CCK_SIG_Err_Band1;
	uint32_t CCK_SFD_Err_Band1;
	uint32_t OFDM_SIG_Err_Band1;
	uint32_t OFDM_TAG_Err_Band1;
	uint32_t PHY_CCK_MDRDY_Band1;
	uint32_t PHY_OFDM_MDRDY_Band1;
	uint32_t CCK_FCS_Err_Band1;
	uint32_t OFDM_FCS_Err_Band1;
	uint32_t MRURxCount;
	uint32_t SIGMCS;
	uint32_t SINR;
	uint32_t RXVRSSI;
	uint32_t Reserved[184];
	uint32_t PHY_Mdrdy;
	uint32_t Noise_Floor;
	uint32_t AllLengthMismatchCount_B0;
	uint32_t AllLengthMismatchCount_B1;
	uint32_t AllMacMdrdy0;
	uint32_t AllMacMdrdy1;
	uint32_t AllFCSErr0;
	uint32_t AllFCSErr1;
	uint32_t RXOK0;
	uint32_t RXOK1;
	uint32_t PER0;
	uint32_t PER1;
};
#else
struct TESTMODE_RX_STAT_BAND {
	/* mac part */
	uint32_t u4MacRxFcsErrCnt;
	uint32_t u4MacRxLenMisMatch;
	uint32_t u4MacRxFcsOkCnt;
	uint32_t u4Reserved1[2];
	uint32_t u4MacRxMdrdyCnt;

	/* phy part */
	uint32_t u4PhyRxFcsErrCntCck;
	uint32_t u4PhyRxFcsErrCntOfdm;
	uint32_t u4PhyRxPdCck;
	uint32_t u4PhyRxPdOfdm;
	uint32_t u4PhyRxSigErrCck;
	uint32_t u4PhyRxSfdErrCck;
	uint32_t u4PhyRxSigErrOfdm;
	uint32_t u4PhyRxTagErrOfdm;
	uint32_t u4PhyRxMdrdyCntCck;
	uint32_t u4PhyRxMdrdyCntOfdm;
};

struct TESTMODE_RX_STAT_USER {
	uint32_t u4FreqOffsetFromRx;
	uint32_t u4Snr;
	uint32_t u4FcsErrorCnt;
};

struct TESTMODE_RX_STAT_COMM {
	uint32_t u4MacRxFifoFull;
	uint32_t u4Reserved1[2];

	uint32_t u4AciHitLow;
	uint32_t u4AciHitHigh;
};

struct TESTMODE_RX_STAT_RXV {
	uint32_t u4Rcpi;
	uint32_t u4Rssi;
	uint32_t u4Snr;
	uint32_t u4AdcRssi;
};

struct TESTMODE_RX_STAT_RSSI {
	uint32_t u4RssiIb;
	uint32_t u4RssiWb;
	uint32_t u4CcaIdlePwr;
	uint32_t u4Reserved1;
};

struct TESTMODE_RX_STAT_BAND_EXT1 {
	/* mac part */
	uint32_t u4RxU2MMpduCnt;

	/* phy part */
	uint32_t u4PhyRxPdAlr;
	uint32_t u4Reserved[3];
};

struct TESTMODE_RX_STAT_COMM_EXT1 {
	uint32_t u4DrvRxCnt;
	uint32_t u4Sinr;
	uint32_t u4MuRxCnt;
	/* mac part */
	uint32_t u4Reserved0[4];

	/* phy part */
	uint32_t u4EhtSigMcs;
	uint32_t u4Reserved1[3];
};

struct TESTMODE_RX_STAT_USER_EXT1 {
	uint32_t u4NeVarDbAllUser;
	uint32_t u4Reserved1[3];
};

struct PARAM_RX_STAT {
	struct TESTMODE_RX_STAT_BAND rInfoBand[UNI_TM_MAX_BAND_NUM];
	struct TESTMODE_RX_STAT_BAND_EXT1 rInfoBandExt1[UNI_TM_MAX_BAND_NUM];
	struct TESTMODE_RX_STAT_COMM rInfoComm[UNI_TM_MAX_BAND_NUM];
	struct TESTMODE_RX_STAT_COMM_EXT1 rInfoCommExt1[UNI_TM_MAX_BAND_NUM];

	/* rxv part */
	struct TESTMODE_RX_STAT_RXV rInfoRXV[UNI_TM_MAX_ANT_NUM];

	/* RSSI */
	struct TESTMODE_RX_STAT_RSSI rInfoFagc[UNI_TM_MAX_ANT_NUM];
	struct TESTMODE_RX_STAT_RSSI rInfoInst[UNI_TM_MAX_ANT_NUM];

	/* User */
	struct TESTMODE_RX_STAT_USER rInfoUser[UNI_TM_MAX_USER_NUM];
	struct TESTMODE_RX_STAT_USER_EXT1 rInfoUserExt1[UNI_TM_MAX_USER_NUM];
};

/* do not re-order for FW compatible */
struct TESTMODE_CAP {
	uint8_t version;/*0*/
	uint8_t support_mimo;
	uint8_t support_dbdc;
	uint8_t support_emlsr;
	uint8_t adie_num;
	uint8_t band_num;/*5*/
	uint8_t phy_num;
	uint8_t mimo_band0_supported_band;
	uint8_t dbdc_band0_supported_band;
	uint8_t dbdc_band1_supported_band;
	uint8_t mimo_band1_supported_band;/*10*/

	uint8_t support_tbtc;
	uint8_t tbtc_band2_supported_band;
	uint8_t tbtc_band3_supported_band;

	uint8_t reserved[18];
};

extern struct TESTMODE_CAP g_HqaCap;
#endif
extern struct PARAM_RX_STAT g_HqaRxStat;

#endif

struct HQA_CMD_FRAME {
	uint32_t MagicNo;
	uint16_t Type;
	uint16_t Id;
	uint16_t Length;
	uint16_t Sequence;
	uint8_t Data[4096];
} __KAL_ATTRIB_PACKED__;

/* TODO: os-related, implement with correspoinding structure in OS */
typedef int32_t(*HQA_CMD_HANDLER) (void *prNetDev,
				   void *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame);

struct HQA_CMD_TABLE {
	HQA_CMD_HANDLER *CmdSet;
	uint32_t CmdSetSize;
	uint32_t CmdOffset;
};

#if (CONFIG_WLAN_SERVICE == 1)
struct PARAM_LIST_MODE_STATUS {
	uint16_t    u2Status;
	uint32_t    u4ExtId;
	uint32_t    u4SegNum;
	union {
		uint32_t u4TxStatus[LIST_SEG_MAX];
	} u;
};

extern struct list_mode_event g_HqaListModeStatus;
#endif
/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/* TODO: os-related, implement with correspoinding structure in OS
 * just use linux as function declare prototype
 */
int HQA_CMDHandler(void *prNetDev,
		   void *prIwReqData,
		   struct HQA_CMD_FRAME *HqaCmdFrame);

int priv_qa_agent(void *prNetDev,
		  void *prIwReqInfo,
		  void *prIwReqData, char *pcExtra);

int32_t mt6632SetICapStart(struct GLUE_INFO *prGlueInfo,
			   uint32_t u4Trigger, uint32_t u4RingCapEn,
			   uint32_t u4Event, uint32_t u4Node, uint32_t u4Len,
			   uint32_t u4StopCycle,
			   uint32_t u4BW, uint32_t u4MacTriggerEvent,
			   uint32_t u4SourceAddrLSB,
			   uint32_t u4SourceAddrMSB, uint32_t u4Band);
int32_t mt6632GetICapStatus(struct GLUE_INFO *prGlueInfo);

int32_t connacSetICapStart(struct GLUE_INFO *prGlueInfo,
			   uint32_t u4Trigger, uint32_t u4RingCapEn,
			   uint32_t u4Event, uint32_t u4Node, uint32_t u4Len,
			   uint32_t u4StopCycle,
			   uint32_t u4BW, uint32_t u4MacTriggerEvent,
			   uint32_t u4SourceAddrLSB,
			   uint32_t u4SourceAddrMSB, uint32_t u4Band);
int32_t connacGetICapStatus(struct GLUE_INFO *prGlueInfo);

int32_t commonGetICapIQData(struct GLUE_INFO *prGlueInfo,
			uint8_t *pData, uint32_t u4IQType, uint32_t u4WFNum);
int32_t connacGetICapIQData(struct GLUE_INFO *prGlueInfo,
			uint8_t *pData, uint32_t u4IQType, uint32_t u4WFNum);


#endif /*CFG_SUPPORT_QA_TOOL */
#endif /* _GL_QA_AGENT_H */
