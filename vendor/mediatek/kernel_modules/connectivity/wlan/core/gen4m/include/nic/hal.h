/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "hal.h"
 *  \brief  The declaration of hal functions
 *
 *   N/A
 */


#ifndef _HAL_H
#define _HAL_H

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
#define WIFI_SER_SYNC_TIMER_TIMEOUT_IN_MS	(100)

/**
 * These static compile options have been moved to wifi.cfg controlled by
 * "TRXDescDump" with bitmap settings:
 *   TXP(0x04),        TXDMAD(0x02), TXD(0x01),
 *   RXDSEGMENT(0x40), RXDMAD(0x20), RXD(0x10).
 *
 * #define CFG_DUMP_TXDMAD
 * #define CFG_DUMP_RXDMAD
 * #define CFG_DUMP_TXD
 * #define CFG_DUMP_TXP
 * #define CFG_DUMP_RXD
 * #define CFG_DUMP_RXD_SEGMENT
 */

#define WIFI_SER_L1_RST_DONE_TIMEOUT    100 /* in unit of ms */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ERR_RECOVERY_STATE {
	ERR_RECOV_STOP_IDLE = 0,
	ERR_RECOV_STOP_PDMA0,
	ERR_RECOV_RESET_PDMA0,
	ERR_RECOV_WAIT_MCU_NORMAL,
	ERR_RECOV_STATE_NUM
};

enum ENUM_TX_RING_IDX {
	TX_RING_DATA0 = 0,
	TX_RING_DATA1,
	TX_RING_DATA2,
	TX_RING_DATA3,
	TX_RING_DATA_PRIO,
	TX_RING_DATA_ALTX,
	TX_RING_CMD,
	TX_RING_FWDL,
	TX_RING_WA_CMD,
	TX_RING_MAX,
};

enum ENUM_RX_RING_IDX {
	RX_RING_DATA0 = 0,
	RX_RING_EVT,
	RX_RING_DATA1,
	RX_RING_TXDONE0,
	RX_RING_TXDONE1,
	RX_RING_DATA2,
	RX_RING_TXDONE2,
	RX_RING_WAEVT0,
	RX_RING_WAEVT1,
	RX_RING_DATA3,
	RX_RING_DATA4,
	RX_RING_DATA5,
	RX_RING_EVT1,
	RX_RING_EVT2,
	RX_RING_MAX,
};

#if (CFG_SUPPORT_HOST_OFFLOAD == 1)
#define RX_RRO_DATA 31

enum ENUM_RRO_IND_REASON {
	RRO_STEP_ONE = 0,
	RRO_REPEAT,
	RRO_OLDPKT,
	RRO_WITHIN,
	RRO_SURPASS,
	RRO_SURPASS_BY_BAR,
	RRO_SURPASS_BIG_SN,
	RRO_DISCONNECT,
	RRO_NOT_RRO_PKT,
	RRO_TIMEOUT_STEP_ONE,
	RRO_TIMEOUT_FLUSH_ALL,
	RRO_BUF_RUN_OUT,
	RRO_COUNTER_NUM
};
#endif /* CFG_SUPPORT_HOST_OFFLOAD == 1 */

#if CFG_PCIE_LTR_UPDATE
enum ENUM_PCIE_LTR_STATE {
	PCIE_LTR_STATE_TX_START = 0,
	PCIE_LTR_STATE_TX_END,
	PCIE_LTR_STATE_NUM
};
#endif

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

/* Macros for flag operations for the Adapter structure */
#define HAL_SET_FLAG(_M, _F)             ((_M)->u4HwFlags |= (_F))
#define HAL_CLEAR_FLAG(_M, _F)           ((_M)->u4HwFlags &= ~(_F))
#define HAL_TEST_FLAG(_M, _F)            ((_M)->u4HwFlags & (_F))
#define HAL_TEST_FLAGS(_M, _F)           (((_M)->u4HwFlags & (_F)) == (_F))

#if CFG_MTK_WIFI_SW_EMI_RING
#define HAL_MCR_EMI_RD(_prAdapter, _u4Offset, _pu4Value, _puRet) { \
	struct ADAPTER *_A = _prAdapter; \
	if (_A) { \
		*_puRet = kalDevRegReadByEmi( \
			_A->prGlueInfo, _u4Offset, _pu4Value); \
	} else { \
		*_puRet = FALSE; \
	} \
}

#define HAL_MCR_EMI_RD8(_prAdapter, _u4Offset, _pu4Low, _pu4High, _puRet) { \
	struct ADAPTER *_A = _prAdapter; \
	if (_A) { \
		*_puRet = kalDevRegReadByEmi8( \
			_A->prGlueInfo, _u4Offset, _pu4Low, _pu4High); \
	} else { \
		*_puRet = FALSE; \
	} \
}
#else
#define HAL_MCR_EMI_RD(_prAdapter, _u4Offset, _pu4Value, _puRet) { \
	*_puRet = FALSE; \
}

#define HAL_MCR_EMI_RD8(_prAdapter, _u4Offset, _pu4Value, _puRet) {	\
	*_puRet = FALSE; \
}
#endif /* CFG_MTK_WIFI_SW_EMI_RING */

#if defined(_HIF_SDIO)
#define HAL_MCR_RD(_prAdapter, _u4Offset, _pu4Value) \
do { \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
			ASSERT(0); \
		} \
		if (_u4Offset > 0xFFFF) { \
			if (!_prAdapter->fgMBAccessFail) { \
				if (kalDevRegRead_mac(_prAdapter->prGlueInfo, \
					_u4Offset, _pu4Value) == FALSE) {\
					_prAdapter->fgMBAccessFail = TRUE; \
					DBGLOG(HAL, ERROR, \
					"MailBox access fail! 0x%x:0x%x\n", \
					(uint32_t) (_u4Offset), \
					*((uint32_t *) (_pu4Value))); \
				} \
			} \
		} else { \
			if (kalDevRegRead(_prAdapter->prGlueInfo, \
				_u4Offset, _pu4Value) == FALSE) {\
				DBGLOG(HAL, ERROR, \
				"HAL_MCR_RD (SDIO) access fail! 0x%x: 0x%x\n", \
					(uint32_t) (_u4Offset), \
					*((uint32_t *) (_pu4Value))); \
			} \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_MCR_RD access! 0x%x\n", \
			(uint32_t) (_u4Offset)); \
	} \
} while (0)

#define HAL_RMCR_RD(_RSN, _A, _R, _V)	HAL_MCR_RD(_A, _R, _V)

#define HAL_MCR_WR(_prAdapter, _u4Offset, _u4Value) \
do { \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
			ASSERT(0); \
		} \
		if (_u4Offset > 0xFFFF) { \
			if (!_prAdapter->fgMBAccessFail) { \
				if (kalDevRegWrite_mac(_prAdapter->prGlueInfo, \
					_u4Offset, _u4Value) == FALSE) {\
					_prAdapter->fgMBAccessFail = TRUE; \
					DBGLOG(HAL, ERROR, \
					"MailBox access fail! 0x%x:0x%x\n", \
					(uint32_t) (_u4Offset), \
					(uint32_t) (_u4Value)); \
				} \
			} \
		} else { \
			if (kalDevRegWrite(_prAdapter->prGlueInfo, \
				_u4Offset, _u4Value) == FALSE) {\
				DBGLOG(HAL, ERROR, \
				"HAL_MCR_WR (SDIO) access fail! 0x%x: 0x%x\n", \
					(uint32_t) (_u4Offset), \
					(uint32_t) (_u4Value)); \
			} \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_MCR_WR access! 0x%x: 0x%x\n", \
			(uint32_t) (_u4Offset), (uint32_t) (_u4Value)); \
	} \
} while (0)

#define HAL_PORT_RD(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	/*fgResult = FALSE; */\
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (kalDevPortRead(_prAdapter->prGlueInfo, _u4Port, _u4Len, \
			_pucBuf, _u4ValidBufSize, FALSE) == FALSE) { \
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_PORT_RD access fail! 0x%x\n", \
				(uint32_t) (_u4Port)); \
		} \
		else { \
			/*fgResult = TRUE;*/ } \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_PORT_RD access! 0x%x\n", \
			(uint32_t) (_u4Port)); \
	} \
}

#define HAL_PORT_WR(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	/*fgResult = FALSE; */\
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (kalDevPortWrite(_prAdapter->prGlueInfo, _u4Port, \
			_u4Len, _pucBuf, _u4ValidBufSize) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_PORT_WR access fail! 0x%x\n", \
				(uint32_t) (_u4Port)); \
		} \
		else \
			; /*fgResult = TRUE;*/ \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_PORT_WR access! 0x%x\n", \
			(uint32_t) (_u4Port)); \
	} \
}

#define HAL_BYTE_WR(_prAdapter, _u4Port, _ucBuf) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (kalDevWriteWithSdioCmd52(_prAdapter->prGlueInfo, \
				_u4Port, _ucBuf) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_BYTE_WR access fail! 0x%x\n", \
				(uint32_t)(_u4Port)); \
		} \
		else { \
			/* Todo:: Nothing*/ \
		} \
	} \
	else { \
		DBGLOG(HAL, WARN, "ignore HAL_BYTE_WR access! 0x%x\n", \
			(uint32_t) (_u4Port)); \
	} \
}

#define HAL_DRIVER_OWN_BY_SDIO_CMD52(_prAdapter, _pfgDriverIsOwnReady) \
{ \
	uint8_t ucBuf = BIT(1); \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (kalDevReadAfterWriteWithSdioCmd52(_prAdapter->prGlueInfo, \
				MCR_WHLPCR_BYTE1, &ucBuf, 1) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, \
			"kalDevReadAfterWriteWithSdioCmd52 access fail!\n"); \
		} \
		else { \
			*_pfgDriverIsOwnReady = (ucBuf & BIT(0)) \
				? TRUE : FALSE; \
		} \
	} else { \
		DBGLOG(HAL, WARN, \
			"ignore HAL_DRIVER_OWN_BY_SDIO_CMD52 access!\n"); \
	} \
}

#else /* #if defined(_HIF_SDIO) */
#define L1_REMAP_OFFSET_MASK (0xffff)
#define GET_L1_REMAP_OFFSET(p) (((p) & L1_REMAP_OFFSET_MASK))
#define L1_REMAP_BASE_MASK (0xffff << 16)
#define GET_L1_REMAP_BASE(p) (((p) & L1_REMAP_BASE_MASK) >> 16)

#define CONN_INFRA_ON_ADDR_START	0x20000000
#define CONN_INFRA_ON_ADDR_END		0x20ffffff
#define CONN_INFRA_PHY_ADDR_START	0x18000000
#define CONN_INFRA_PHY_ADDR_END		0x183fffff
#define WFSYS_PHY_ADDR_START		0x18400000
#define WFSYS_PHY_ADDR_END		0x187fffff
#define BGFSYS_PHY_ADDR_START		0x18800000
#define BGFSYS_PHY_ADDR_END		0x18bfffff
#define CBTOP1_PHY_ADDR_START		0x70000000
#define CBTOP1_PHY_ADDR_END		0x77ffffff
#define CBTOP2_PHY_ADDR_START		0xf0000000
#define CBTOP2_PHY_ADDR_END		0xffffffff

#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
#define WF_MCUSYS_VDNR_GEN_BUS_U_DEBUG_CTRL_START 0x810f0000
#define WF_MCUSYS_VDNR_GEN_BUS_U_DEBUG_CTRL_END 0x810f043c
#define CONN_MCU_BUS_CR_START 0x830c1000
#define CONN_MCU_BUS_CR_END 0x830c10ff
#define CONN_MCU_BUS_CR_BASE_ADDR	0x830c0000
#define CONN_MCU_CONFG_CFG_DBG1_ADDR_START	0x88000000
#define CONN_MCU_CONFG_CFG_DBG1_ADDR_END	0x8800000f
#define AP2WF_FIXED_REMAP_OFFSET	(0x88000000 - 0x7c4f0000)
#define AP2WF_DYNAMIC_REMAP_NO_1	0x7c400124
#define AP2WF_DYNAMIC_REMAP_NO_1_BASE_ADDR	0x7c700000
#endif

#define CONN_INFRA_MCU_ADDR_START	0x7c000000
#define CONN_INFRA_MCU_ADDR_END		0x7cffffff
#define CONN_INFRA_MCU_TO_PHY_ADDR_OFFSET \
	(CONN_INFRA_MCU_ADDR_START - CONN_INFRA_PHY_ADDR_START)
#define CONN_INFRA_ON_ADDR_OFFSET \
	(CONN_INFRA_ON_ADDR_START - CONN_INFRA_PHY_ADDR_START)

#define IS_CONN_INFRA_ON_ADDR(_reg) \
	((_reg) >= CONN_INFRA_ON_ADDR_START && (_reg) \
		<= CONN_INFRA_ON_ADDR_END)
#define IS_CONN_INFRA_PHY_ADDR(_reg) \
	((_reg) >= CONN_INFRA_PHY_ADDR_START && (_reg) \
		<= CONN_INFRA_PHY_ADDR_END)
#define IS_WFSYS_PHY_ADDR(_reg) \
	((_reg) >= WFSYS_PHY_ADDR_START && (_reg) <= WFSYS_PHY_ADDR_END)
#define IS_BGFSYS_PHY_ADDR(_reg) \
	((_reg) >= BGFSYS_PHY_ADDR_START && (_reg) <= BGFSYS_PHY_ADDR_END)
#define IS_CBTOP_PHY_ADDR(_reg) \
	(((_reg) >= CBTOP1_PHY_ADDR_START && (_reg) <= CBTOP1_PHY_ADDR_END) || \
	((_reg) >= CBTOP2_PHY_ADDR_START && (_reg) <= CBTOP2_PHY_ADDR_END))
#define IS_PHY_ADDR(_reg) \
	(IS_CONN_INFRA_PHY_ADDR(_reg) || IS_WFSYS_PHY_ADDR(_reg) \
		|| IS_BGFSYS_PHY_ADDR(_reg) || IS_CBTOP_PHY_ADDR(_reg))

#define IS_CONN_INFRA_MCU_ADDR(_reg) \
	((_reg) >= CONN_INFRA_MCU_ADDR_START && (_reg) \
			<= CONN_INFRA_MCU_ADDR_END)

#if (CFG_MTK_WIFI_CONNV3_SUPPORT == 1)
#define IS_WF_MCUSYS_VDNR_ADDR(_reg) \
	((_reg) >= WF_MCUSYS_VDNR_GEN_BUS_U_DEBUG_CTRL_START && (_reg) \
			<= WF_MCUSYS_VDNR_GEN_BUS_U_DEBUG_CTRL_END)
#define IS_CONN_MCU_BUS_CR_ADDR(_reg) \
	((_reg) >= CONN_MCU_BUS_CR_START && (_reg) \
			<= CONN_MCU_BUS_CR_END)
#define IS_CONN_MCU_CONFG_CFG_DBG1_ADDR(_reg) \
	((_reg) >= CONN_MCU_CONFG_CFG_DBG1_ADDR_START && (_reg) \
			<= CONN_MCU_CONFG_CFG_DBG1_ADDR_END)
#endif

#if (CFG_NEW_HIF_DEV_REG_IF == 1)
#define HAL_RMCR_RD(_RSN, _A, _R, _V) \
{ \
	struct ADAPTER *_AD = _A; \
	if (_AD == NULL) { \
		kalDevRegRead( \
			HIF_DEV_REG_##_RSN, NULL, _R, _V); \
	} else { \
		if (_AD->rAcpiState == ACPI_STATE_D3) \
			ASSERT(0); \
		kalDevRegRead( \
			HIF_DEV_REG_##_RSN, _AD->prGlueInfo, _R, _V); \
	} \
}

#define HAL_RMCR_RD_RANGE(_RSN, _A, _R, _B, _S, _RET) \
{ \
	struct ADAPTER *_AD = _A; \
	if (_AD == NULL) { \
		_RET = kalDevRegReadRange( \
			HIF_DEV_REG_##_RSN, NULL, _R, _B, _S); \
	} else { \
		if (_AD->rAcpiState == ACPI_STATE_D3) \
			ASSERT(0); \
		_RET = kalDevRegReadRange( \
			HIF_DEV_REG_##_RSN, _AD->prGlueInfo, \
			_R, _B, _S); \
	} \
	_RET; \
}
#else
#define HAL_MCR_RD(_A, _R, _V) \
{ \
	struct ADAPTER *_AD = _A; \
	if (_AD == NULL) { \
		kalDevRegRead(NULL, _R, _V); \
	} else { \
		if (_AD->rAcpiState == ACPI_STATE_D3) \
			ASSERT(0); \
		kalDevRegRead(_AD->prGlueInfo, _R, _V); \
	} \
}

#define HAL_MCR_RD_RANGE(_A, _R, _B, _S, _RET) \
{ \
	struct ADAPTER *_AD = _A; \
	if (_AD == NULL) { \
		_RET = kalDevRegReadRange(NULL, _R, _B, _S); \
	} else { \
		if (_AD->rAcpiState == ACPI_STATE_D3) \
			ASSERT(0); \
		_RET = kalDevRegReadRange(_AD->prGlueInfo, _R, _B, _S); \
	} \
	_RET; \
}

#define HAL_RMCR_RD(_RSN, _A, _R, _V)	HAL_MCR_RD(_A, _R, _V)
#define HAL_RMCR_RD_RANGE(_RSN, _A, _R, _B, _S, _RET) \
	HAL_MCR_RD_RANGE(_A, _R, _B, _S, _RET)
#endif /* CFG_NEW_HIF_DEV_REG_IF */

#define HAL_MCR_WR(_prAdapter, _u4Offset, _u4Value) \
{ \
	struct ADAPTER *_AD = _prAdapter; \
	if (_AD == NULL) { \
		kalDevRegWrite(NULL, _u4Offset, _u4Value); \
	} else { \
		if (_AD->rAcpiState == ACPI_STATE_D3) {	\
			ASSERT(0); \
		} \
		kalDevRegWrite(_AD->prGlueInfo, _u4Offset, _u4Value); \
	} \
}

#define HAL_PORT_RD(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevPortRead(_prAdapter->prGlueInfo, _u4Port, \
		_u4Len, _pucBuf, _u4ValidBufSize, FALSE); \
}

#define HAL_PORT_WR(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevPortWrite(_prAdapter->prGlueInfo, _u4Port, \
		_u4Len, _pucBuf, _u4ValidBufSize); \
}

#define HAL_BYTE_WR(_prAdapter, _u4Port, _ucBuf) \
{ \
	HAL_MCR_WR(_prAdapter, _u4Port, (uint32_t)_ucBuf); \
}

#endif /* #if defined(_HIF_SDIO) */

#if (CFG_NEW_HIF_DEV_REG_IF == 1)
#define HAL_MCR_WR_FIELD(_R, _A, _O, _u4FieldVal, _ucShft, _u4Mask) \
{ \
	uint32_t u4CrValue = 0; \
	HAL_RMCR_RD(_R, _A, _O, &u4CrValue);	\
	u4CrValue &= (~_u4Mask); \
	u4CrValue |= ((_u4FieldVal << _ucShft) & _u4Mask); \
	HAL_MCR_WR(_A, _O, u4CrValue); \
}

#define HAL_MCR_RD_FIELD(_R, _A, _O, _ucShft, _u4Mask, pu4Val) \
{ \
	HAL_RMCR_RD(_R, _A, _O, pu4Val); \
	*pu4Val = ((*pu4Val & _u4Mask) >> _ucShft); \
}
#else
#define HAL_MCR_WR_FIELD(_A, _O, _u4FieldVal, _ucShft, _u4Mask) \
{ \
	uint32_t u4CrValue = 0; \
	HAL_MCR_RD(_A, _O, &u4CrValue);	\
	u4CrValue &= (~_u4Mask); \
	u4CrValue |= ((_u4FieldVal << _ucShft) & _u4Mask); \
	HAL_MCR_WR(_A, _O, u4CrValue); \
}

#define HAL_MCR_RD_FIELD(_A, _O, _ucShft, _u4Mask, pu4Val) \
{ \
	HAL_MCR_RD(_A, _O, pu4Val); \
	*pu4Val = ((*pu4Val & _u4Mask) >> _ucShft); \
}
#endif /* CFG_NEW_HIF_DEV_REG_IF */

#define HAL_WRITE_TX_DATA(_prAdapter, _prMsduInfo) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevWriteData(_prAdapter->prGlueInfo, _prMsduInfo); \
}

#define HAL_KICK_TX_DATA(_prAdapter) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevKickData(_prAdapter->prGlueInfo); \
}

#define HAL_WRITE_TX_CMD_SMART_SEQ(_prAdapter, _prCmdInfo, _ucTC) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevWriteCmd(_prAdapter->prGlueInfo, _prCmdInfo, _ucTC); \
}

#define HAL_WRITE_TX_CMD(_prAdapter, _prCmdInfo, _ucTC) \
{ \
	enum ENUM_CMD_TX_RESULT ret; \
	KAL_SPIN_LOCK_DECLARATION(); \
	\
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING); \
	ret = kalDevWriteCmd(_prAdapter->prGlueInfo, _prCmdInfo, _ucTC); \
	if (ret == CMD_TX_RESULT_SUCCESS) { \
		if (_prCmdInfo && _prCmdInfo->pfHifTxCmdDoneCb) \
			_prCmdInfo->pfHifTxCmdDoneCb(_prAdapter, _prCmdInfo); \
	} \
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING); \
}

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
#define HAL_READ_RX_PORT(prAdapter, u4PortId, u4Len, pvBuf, _u4ValidBufSize) \
{ \
	ASSERT(u4PortId < 2); \
	HAL_PORT_RD(prAdapter, \
		u4PortId, \
		u4Len, \
		pvBuf, \
		_u4ValidBufSize/*temp!!*//*4Kbyte*/) \
}

#define HAL_WRITE_TX_PORT(_prAdapter, _u4PortId, _u4Len, _pucBuf, _u4BufSize) \
{ \
	HAL_PORT_WR(_prAdapter, \
		_u4PortId, \
		_u4Len, \
		_pucBuf, \
		_u4BufSize/*temp!!*//*4KByte*/) \
}

#define HAL_MCR_RD_AND_WAIT(_pAdapter, _offset, _pReadValue, \
	_waitCondition, _waitDelay, _waitCount, _status)

#define HAL_MCR_WR_AND_WAIT(_pAdapter, _offset, _writeValue, \
	_busyMask, _waitDelay, _waitCount, _status)

#define HAL_GET_CHIP_ID_VER(_prAdapter, pu2ChipId, pu2Version) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(_prAdapter, HIF_SYS_REV, &u4Value); \
	*pu2ChipId = ((u4Value & PCIE_HIF_SYS_PROJ) >> 16); \
	*pu2Version = (u4Value & PCIE_HIF_SYS_REV); \
}

#if (CFG_MTK_WIFI_ON_READ_BY_CFG_SPACE == 1) && defined(_HIF_PCIE)
#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	uint32_t u4Value = 0; \
	glReadPcieCfgSpace(PCIE_CFGSPACE_BASE_OFFSET, &u4Value); \
	*_pfgResult = (((u4Value >> PCIE_CFGSPACE_FW_STATUS_SYNC_SHIFT) \
		       & PCIE_CFGSPACE_FW_STATUS_SYNC_MASK) \
		       == (PCIE_CFGSPACE_FW_STATUS_SYNC_MASK)) ? TRUE : FALSE; \
} while (0)
#else
#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	uint32_t u4Value = 0; \
	u_int8_t fgRet = FALSE; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_EMI_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value, &fgRet); \
	if (!fgRet) \
		HAL_RMCR_RD(ONOFF_READ, _prAdapter, \
			       prChipInfo->sw_sync0, &u4Value);	\
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) \
	     == (_checkItem << prChipInfo->sw_ready_bit_offset)) \
		*_pfgResult = TRUE; \
} while (0)
#endif /* CFG_MTK_WIFI_EN_SW_EMI_READ */

#if (CFG_MTK_WIFI_SUPPORT_SW_SYNC_BY_EMI == 1)
#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	uint32_t u4Value = _checkItem; \
	int32_t i4Ret = -1; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
					\
	if (!prChipInfo->sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].isValid) { \
		DBGLOG(INIT, ERROR, \
		"WiFi Off EMI is invalid, offset:[0x%08x].\n", \
		prChipInfo->sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].offset);\
		break; \
	} \
	i4Ret = emi_mem_read(prChipInfo, \
			prChipInfo->\
			sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].offset, \
			&u4Value, \
			sizeof(u4Value)); \
	if (i4Ret != 0) {\
		DBGLOG(INIT, ERROR, \
		"Read WiFi off EMI offset:[0x%08x] failed.\n", \
		prChipInfo->\
		sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].offset); \
	} else if (u4Value == prChipInfo->wifi_off_magic_num) { \
		*_pfgResult = TRUE; \
	} \
} while (0)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	struct BUS_INFO *prBusInfo = NULL; \
	uint32_t u4Value = 0; \
	int32_t i4Ret = -1; \
	if (!_prAdapter->chip_info || !_prAdapter->chip_info->bus_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	prBusInfo = prChipInfo->bus_info; \
	\
	if (!prChipInfo->sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].isValid) { \
		HAL_RMCR_RD(ONOFF_READ, _prAdapter, \
		       prChipInfo->sw_sync0, &_u4Result); \
	} else { \
		i4Ret = emi_mem_read(prChipInfo, \
			prChipInfo->\
			sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].offset, \
			&_u4Result, \
			sizeof(_u4Result)); \
		if (i4Ret != 0) \
			DBGLOG(INIT, ERROR, \
			"Read EMI offset: [0x%08x] failed.\n", \
			prChipInfo->\
			sw_sync_emi_info[SW_SYNC_ON_OFF_TAG].offset); \
	} \
	if (prBusInfo->getMailboxStatus) {	\
		prBusInfo->getMailboxStatus(_prAdapter, &u4Value);	\
		DBGLOG(INIT, DEBUG, "Mailbox: 0x%x\n", u4Value); \
	} \
} while (0)
#else /* CFG_MTK_WIFI_SUPPORT_SW_SYNC_BY_EMI == 0 */
#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult); \
	*_pfgResult = !*_pfgResult; \
} while (0)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	struct BUS_INFO *prBusInfo = NULL; \
	uint32_t u4Value = 0; \
	if (!_prAdapter->chip_info || !_prAdapter->chip_info->bus_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	prBusInfo = prChipInfo->bus_info; \
	HAL_RMCR_RD(ONOFF_READ, _prAdapter, \
		       prChipInfo->sw_sync0, &_u4Result);	\
	if (prBusInfo->getMailboxStatus) {	\
		prBusInfo->getMailboxStatus(_prAdapter, &u4Value);	\
		DBGLOG(INIT, DEBUG, "Mailbox: 0x%x\n", u4Value); \
	} \
} while (0)
#endif /* CFG_MTK_WIFI_SUPPORT_SW_SYNC_BY_EMI */

#define HAL_INTR_DISABLE(_prAdapter)

#define HAL_INTR_ENABLE(_prAdapter)

#define HAL_INTR_ENABLE_AND_LP_OWN_SET(_prAdapter)

/* Polling interrupt status bit due to CFG_PCIE_LPCR_HOST
 * status bit not work when chip sleep
 */
#define HAL_LP_OWN_RD(_prAdapter, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	struct BUS_INFO *prBusInfo = NULL; \
	if (!_prAdapter->chip_info || !_prAdapter->chip_info->bus_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	prBusInfo = prChipInfo->bus_info; \
	prBusInfo->lowPowerOwnRead(_prAdapter, _pfgResult); \
} while (0)

#define HAL_LP_OWN_SET(_prAdapter, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	struct BUS_INFO *prBusInfo = NULL; \
	if (!_prAdapter->chip_info || !_prAdapter->chip_info->bus_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	prBusInfo = prChipInfo->bus_info; \
	prBusInfo->lowPowerOwnSet(_prAdapter, _pfgResult); \
} while (0)

#define HAL_LP_OWN_CLR(_prAdapter, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	struct BUS_INFO *prBusInfo = NULL; \
	if (!_prAdapter->chip_info || !_prAdapter->chip_info->bus_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	prBusInfo = prChipInfo->bus_info; \
	prBusInfo->lowPowerOwnClear(_prAdapter, _pfgResult); \
} while (0)

#define HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE(_prAdapter, pu4AbnormalReason)

#define HAL_DISABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_ENABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_CFG_MAX_HIF_RX_LEN_NUM(_prAdapter, _ucNumOfRxLen)

#define HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter)

#define HAL_SET_INTR_STATUS_WRITE_1_CLEAR(prAdapter)

#define HAL_READ_INTR_STATUS(prAdapter, length, pvBuf)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount)

#define HAL_READ_RX_LENGTH(prAdapter, pu2Rx0Len, pu2Rx1Len)

#define HAL_GET_INTR_STATUS_FROM_ENHANCE_MODE_STRUCT(pvBuf, u2Len, pu4Status)

#define HAL_GET_TX_STATUS_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu4BufOut, u4LenBufOut)

#define HAL_GET_RX_LENGTH_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu2Rx0Num, au2Rx0Len, pu2Rx1Num, au2Rx1Len)

#define HAL_GET_MAILBOX_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu4Mailbox0, pu4Mailbox1)

#define HAL_IS_TX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & (WPDMA_TX_DONE_INT0 | \
	WPDMA_TX_DONE_INT1 | WPDMA_TX_DONE_INT2 | WPDMA_TX_DONE_INT3 | \
	WPDMA_TX_DONE_INT15)) ? TRUE : FALSE)

#define HAL_IS_RX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & (WPDMA_RX_DONE_INT0 | WPDMA_RX_DONE_INT1)) \
	? TRUE : FALSE)

#define HAL_IS_ABNORMAL_INTR(u4IntrStatus)

#define HAL_IS_FW_OWNBACK_INTR(u4IntrStatus)

#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data)

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data)

#define HAL_SET_MAILBOX_READ_CLEAR(prAdapter, fgEnableReadClear) \
{ \
	prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear = fgEnableReadClear;\
}

#define HAL_GET_MAILBOX_READ_CLEAR(prAdapter) \
	(prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear)

#define HAL_READ_INT_STATUS(_prAdapter, _pu4IntStatus) \
{ \
	struct BUS_INFO *prBusInfo; \
	struct GL_HIF_INFO *prHifInfo; \
	prBusInfo = _prAdapter->chip_info->bus_info; \
	prHifInfo = &_prAdapter->prGlueInfo->rHifInfo; \
	if (prBusInfo->devReadIntStatus) \
		prBusInfo->devReadIntStatus(_prAdapter, _pu4IntStatus); \
	else \
		kalDevReadIntStatus(_prAdapter, _pu4IntStatus);\
	if (_prAdapter->ulNoMoreRfb != 0) \
		*_pu4IntStatus |= WHISR_RX0_DONE_INT; \
	if (!prHifInfo->fgIsBackupIntSta) { \
		prHifInfo->fgIsBackupIntSta = true; \
		prHifInfo->u4WakeupIntSta = prHifInfo->u4IntStatus; \
	} \
}

#define HAL_HIF_INIT(prAdapter)

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable) \
{ \
	halEnableFWDownload(_prAdapter, _fgEnable);	\
}

#define HAL_WAKE_UP_WIFI(_prAdapter) \
{ \
	halWakeUpWiFi(_prAdapter);\
}

#define HAL_CLEAR_DUMMY_REQ(_prAdapter) \
{ \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	uint32_t u4Value = 0; \
	if (!_prAdapter->chip_info) {\
		ASSERT(0); \
	} else {\
		prChipInfo = _prAdapter->chip_info; \
		HAL_RMCR_RD(LPOWN_READ, _prAdapter, \
			       prChipInfo->sw_sync0, &u4Value); \
		u4Value &= ~(WIFI_FUNC_DUMMY_REQ << \
			prChipInfo->sw_ready_bit_offset);\
		HAL_MCR_WR(prAdapter, prChipInfo->sw_sync0, u4Value);\
	} \
}

#define HAL_IS_TX_DIRECT(_prAdapter) \
	((CFG_TX_DIRECT) ? TRUE : FALSE)

#define HAL_IS_RX_DIRECT(_prAdapter) \
	((CFG_RX_DIRECT) ? TRUE : FALSE)

#define HAL_UHW_RD(_prAdapter, _u4Offset, _pu4Value, _pucSts)

#define HAL_UHW_WR(_prAdapter, _u4Offset, _u4Value, _pucSts)

#if !CFG_WMT_RESET_API_SUPPORT
#define HAL_CANCEL_TX_RX(_prAdapter) nicSerStopTxRx(_prAdapter)

#define HAL_RESUME_TX_RX(_prAdapter) nicSerStartTxRx(_prAdapter)
#else
#define HAL_CANCEL_TX_RX(_prAdapter)

#define HAL_RESUME_TX_RX(_prAdapter)
#endif

#define HAL_TOGGLE_WFSYS_RST(_prAdapter)    \
	halToggleWfsysRst(_prAdapter)
#endif

#if defined(_HIF_SDIO)
#define HAL_READ_RX_PORT(prAdapter, u4PortId, u4Len, pvBuf, _u4ValidBufSize) \
{ \
	ASSERT(u4PortId < 2); \
	HAL_PORT_RD(prAdapter, \
		((u4PortId == 0) ? MCR_WRDR0 : MCR_WRDR1), \
		u4Len, \
		pvBuf, \
		_u4ValidBufSize/*temp!!*//*4Kbyte*/) \
}

#define HAL_WRITE_TX_PORT(_prAdapter, _u4PortId, _u4Len, _pucBuf, _u4BufSize) \
{ \
	if ((_u4BufSize - _u4Len) >= sizeof(uint32_t)) { \
		/* fill with single dword of zero as TX-aggregation end */ \
		*(uint32_t *) (&((_pucBuf)[ALIGN_4(_u4Len)])) = 0; \
	} \
	HAL_PORT_WR(_prAdapter, \
		MCR_WTDR1, \
		_u4Len, \
		_pucBuf, \
		_u4BufSize/*temp!!*//*4KByte*/) \
}

/* The macro to read the given MCR several times to check if the wait
 *  condition come true.
 */
#define HAL_MCR_RD_AND_WAIT(_pAdapter, _offset, _pReadValue, \
	_waitCondition, _waitDelay, _waitCount, _status) \
	{ \
		uint32_t count = 0; \
		(_status) = FALSE; \
		for (count = 0; count < (_waitCount); count++) { \
			HAL_MCR_RD((_pAdapter), (_offset), (_pReadValue)); \
			if ((_waitCondition)) { \
				(_status) = TRUE; \
				break; \
			} \
			kalUdelay((_waitDelay)); \
		} \
	}

/* The macro to write 1 to a R/S bit and read it several times to check if the
 *  command is done
 */
#define HAL_MCR_WR_AND_WAIT(_pAdapter, _offset, _writeValue, _busyMask, \
	_waitDelay, _waitCount, _status) \
	{ \
		uint32_t u4Temp = 0; \
		uint32_t u4Count = _waitCount; \
		(_status) = FALSE; \
		HAL_MCR_WR((_pAdapter), (_offset), (_writeValue)); \
		do { \
			kalUdelay((_waitDelay)); \
			HAL_MCR_RD((_pAdapter), (_offset), &u4Temp); \
			if (!(u4Temp & (_busyMask))) { \
				(_status) = TRUE; \
				break; \
			} \
			u4Count--; \
		} while (u4Count); \
	}

#define HAL_GET_CHIP_ID_VER(_prAdapter, pu2ChipId, pu2Version) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WCIR, \
		&u4Value); \
	*pu2ChipId = (uint16_t)(u4Value & WCIR_CHIP_ID); \
	*pu2Version = (uint16_t)(u4Value & WCIR_REVISION_ID) >> 16; \
}

#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	uint32_t u4Value = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WCIR, \
		&u4Value); \
	if (u4Value & WCIR_WLAN_READY) { \
		*_pfgResult = TRUE; \
	} \
} while (0)

#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	uint32_t u4Value = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(_prAdapter, MCR_WCIR, &u4Value); \
	if ((u4Value & WCIR_WLAN_READY) == 0) { \
		*_pfgResult = TRUE; \
	} \
	halPrintMailbox(_prAdapter);\
	halPollDbgCr(_prAdapter, LP_DBGCR_POLL_ROUND); \
} while (0)

#if (defined(CFG_SDIO_MAILBOX_EXTENSION) && (CFG_SDIO_MAILBOX_EXTENSION == 1))
#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
	halGetMailbox(_prAdapter, ENUM_SDIO_MAILBOX_STATUS, &_u4Result)
#else
#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
	halGetMailbox(_prAdapter, 0, &_u4Result)
#endif

#define HAL_INTR_DISABLE(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		WHLPCR_INT_EN_CLR)

#define HAL_INTR_ENABLE(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		WHLPCR_INT_EN_SET)

#define HAL_INTR_ENABLE_AND_LP_OWN_SET(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		(WHLPCR_INT_EN_SET | WHLPCR_FW_OWN_REQ_SET))

#define HAL_LP_OWN_RD(_prAdapter, _pfgResult) \
{ \
	uint32_t u4RegValue = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
	if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
		*_pfgResult = TRUE; \
	} \
}

#define HAL_LP_OWN_SET(_prAdapter, _pfgResult) \
{ \
	uint32_t u4RegValue = 0; \
	*_pfgResult = FALSE; \
	if (kalIsRstPreventFwOwn() == FALSE) { \
		HAL_MCR_WR(_prAdapter, \
			MCR_WHLPCR, \
			WHLPCR_FW_OWN_REQ_SET); \
		HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
		if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
			*_pfgResult = TRUE; \
		} \
	} else \
		DBGLOG(INIT, DEBUG, "[SER][L0.5]skip set fw own\n"); \
}

#define HAL_LP_OWN_CLR(_prAdapter, _pfgResult) \
{ \
	uint32_t u4RegValue = 0; \
	*_pfgResult = FALSE; \
	/* Software get LP ownership */ \
	HAL_MCR_WR(_prAdapter, \
			MCR_WHLPCR, \
			WHLPCR_FW_OWN_REQ_CLR); \
	HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
	if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
		*_pfgResult = TRUE; \
	} \
}

#if (CFG_SUPPORT_SDIO_DB_DELAY == 1)

#define HAL_LP_DB_DELAY_CLEAR(_prAdapter, _pfgResult) \
{ \
	if (_pfgResult != NULL) { \
		uint32_t u4RegValue = 0; \
		*_pfgResult = TRUE; \
		HAL_MCR_RD(_prAdapter, \
			MCR_WHLPCR, \
			&u4RegValue); \
		if ((u4RegValue & WHLPCR_REG_DB_DELAY_CNT_ENABLE) != 0) { \
			u4RegValue &= WHLPCR_FORCE_DRV_OWN; \
			HAL_MCR_WR(_prAdapter, \
				MCR_WHLPCR, \
				u4RegValue); \
			HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
			if ((u4RegValue \
				& WHLPCR_REG_DB_DELAY_CNT_ENABLE) != 0) { \
				*_pfgResult = FALSE; \
			} \
		} \
	} \
}

#define HAL_LP_DB_DELAY_SET(_prAdapter, _pfgResult) \
{ \
	if (_pfgResult != NULL) { \
		uint32_t u4RegValue = 0; \
		*_pfgResult = FALSE; \
		HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
		if ((u4RegValue & WHLPCR_REG_DB_DELAY_CNT_ENABLE) != 0) { \
			*_pfgResult = TRUE; \
		} else { \
			u4RegValue &= WHLPCR_FORCE_DRV_OWN; \
			u4RegValue |= WHLPCR_REG_DB_DELAY_CNT_ENABLE; \
			u4RegValue |= (WHLPCR_REG_DB_DELAY_CNT_0x60 << \
					WHLPCR_REG_DB_DELAY_CNT_SHIFT); \
			HAL_MCR_WR(_prAdapter, \
				MCR_WHLPCR, \
				u4RegValue); \
			HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
			if ((u4RegValue \
				& WHLPCR_REG_DB_DELAY_CNT_ENABLE) != 0) { \
				*_pfgResult = TRUE; \
			} \
		} \
	} \
}

#endif

#if (CFG_SUPPORT_SDIO_FORCE_DRV_OWN == 1)
#define HAL_LP_FORCE_DRV_OWN_SET(_prAdapter, _pfgResult) \
{ \
	uint32_t u4RegValue = 0; \
	*_pfgResult = FALSE; \
	/* Software set LP force driver own*/ \
	HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
	if ((u4RegValue & \
		(WHLPCR_FORCE_DRV_OWN | WHLPCR_IS_DRIVER_OWN)) != 0) { \
		*_pfgResult = TRUE; \
	} else { \
		u4RegValue &= (WHLPCR_REG_DB_DELAY_CNT_MASK | \
				WHLPCR_REG_DB_DELAY_CNT_ENABLE); \
		u4RegValue |= WHLPCR_FORCE_DRV_OWN; \
		HAL_MCR_WR(_prAdapter, MCR_WHLPCR, u4RegValue); \
		HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
		if (((u4RegValue & WHLPCR_FORCE_DRV_OWN) != 0) && \
			((u4RegValue & WHLPCR_IS_DRIVER_OWN) == 0)) { \
			*_pfgResult = TRUE; \
		} \
	} \
}

#define HAL_LP_FORCE_DRV_OWN_CLR(_prAdapter, _pfgResult) \
{ \
	uint32_t u4RegValue = 0; \
	*_pfgResult = TRUE; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHLPCR, \
		&u4RegValue); \
	if ((u4RegValue & WHLPCR_FORCE_DRV_OWN) != 0) { \
		u4RegValue &= (WHLPCR_REG_DB_DELAY_CNT_MASK | \
				WHLPCR_REG_DB_DELAY_CNT_ENABLE); \
		HAL_MCR_WR(_prAdapter, MCR_WHLPCR, u4RegValue); \
		HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
		if ((u4RegValue & WHLPCR_FORCE_DRV_OWN) != 0) { \
			*_pfgResult = FALSE; \
		} \
	} \
}
#endif

#define HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE(_prAdapter, pu4AbnormalReason) \
{ \
	HAL_MCR_RD(_prAdapter, \
		MCR_WASR, \
		pu4AbnormalReason); \
}

#define HAL_DISABLE_RX_ENHANCE_MODE(_prAdapter) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHCR, \
		u4Value & ~WHCR_RX_ENHANCE_MODE_EN); \
}

#define HAL_ENABLE_RX_ENHANCE_MODE(_prAdapter) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHCR, \
		u4Value | WHCR_RX_ENHANCE_MODE_EN); \
}

#define HAL_CFG_MAX_HIF_RX_LEN_NUM(_prAdapter, _ucNumOfRxLen) \
{ \
	uint32_t u4Value = 0, ucNum; \
	ucNum = ((_ucNumOfRxLen >= HIF_RX_MAX_AGG_NUM) ? 0 : _ucNumOfRxLen); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	u4Value &= ~WHCR_MAX_HIF_RX_LEN_NUM; \
	u4Value |= ((((uint32_t)ucNum) << WHCR_OFFSET_MAX_HIF_RX_LEN_NUM) \
		& WHCR_MAX_HIF_RX_LEN_NUM); \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHCR, \
		u4Value); \
}

#define HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(prAdapter, \
		MCR_WHCR, \
		u4Value & ~WHCR_W_INT_CLR_CTRL); \
	prAdapter->prGlueInfo->rHifInfo.fgIntReadClear = TRUE;\
}

#define HAL_SET_INTR_STATUS_WRITE_1_CLEAR(prAdapter) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(prAdapter, \
		MCR_WHCR, \
		u4Value | WHCR_W_INT_CLR_CTRL); \
	prAdapter->prGlueInfo->rHifInfo.fgIntReadClear = FALSE;\
}

/* Note: enhance mode structure may also carried inside the buffer,
 *	 if the length of the buffer is long enough
 */
#define HAL_READ_INTR_STATUS(prAdapter, length, pvBuf) \
	HAL_PORT_RD(prAdapter, \
		MCR_WHISR, \
		length, \
		pvBuf, \
		length)

/* CR not continuous, so need to change base.
 *	MCR_WTQCR0~7 & MCR_WTQCR8~15 is
 *	continuous.
 */
#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount) \
{ \
	uint32_t *pu4Value = (uint32_t *)au2TxReleaseCount; \
	uint8_t  i = 0; \
	uint32_t u4CR = MCR_WTQCR0; \
	for (i = 0; i < SDIO_TX_RESOURCE_REG_NUM; i++) { \
		if (i == 8) { \
			u4CR = MCR_WTQCR8; \
		} \
		HAL_MCR_RD(_prAdapter, \
			u4CR, \
			&pu4Value[i]); \
		u4CR += 4; \
	} \
}


#define HAL_READ_RX_LENGTH(prAdapter, pu2Rx0Len, pu2Rx1Len) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(prAdapter, \
		MCR_WRPLR, \
		&u4Value); \
	*pu2Rx0Len = (uint16_t)u4Value; \
	*pu2Rx1Len = (uint16_t)(u4Value >> 16); \
}

#define HAL_GET_INTR_STATUS_FROM_ENHANCE_MODE_STRUCT(pvBuf, \
	u2Len, pu4Status) \
{ \
	uint32_t *pu4Buf = (uint32_t *)pvBuf; \
	*pu4Status = pu4Buf[0]; \
}

#define HAL_GET_TX_STATUS_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu4BufOut, u4LenBufOut) \
{ \
	uint32_t *pu4Buf = (uint32_t *)pvInBuf; \
	ASSERT(u4LenBufOut >= 8); \
	pu4BufOut[0] = pu4Buf[1]; \
	pu4BufOut[1] = pu4Buf[2]; \
}

#define HAL_GET_RX_LENGTH_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu2Rx0Num, au2Rx0Len, pu2Rx1Num, au2Rx1Len) \
{ \
	uint32_t *pu4Buf = (uint32_t *)pvInBuf; \
	ASSERT((sizeof(au2Rx0Len) / sizeof(uint16_t)) >= 16); \
	ASSERT((sizeof(au2Rx1Len) / sizeof(uint16_t)) >= 16); \
	*pu2Rx0Num = (uint16_t)pu4Buf[3]; \
	*pu2Rx1Num = (uint16_t)(pu4Buf[3] >> 16); \
	kalMemCopy(au2Rx0Len, &pu4Buf[4], 8); \
	kalMemCopy(au2Rx1Len, &pu4Buf[12], 8); \
}

#define HAL_GET_MAILBOX_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu4Mailbox0, pu4Mailbox1) \
{ \
	uint32_t *pu4Buf = (uint32_t *)pvInBuf; \
	*pu4Mailbox0 = (uint16_t)pu4Buf[21]; \
	*pu4Mailbox1 = (uint16_t)pu4Buf[22]; \
}

#define HAL_IS_TX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & WHISR_TX_DONE_INT) ? TRUE : FALSE)

#define HAL_IS_RX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & (WHISR_RX0_DONE_INT | WHISR_RX1_DONE_INT)) \
	? TRUE : FALSE)

#define HAL_IS_ABNORMAL_INTR(u4IntrStatus) \
	((u4IntrStatus & WHISR_ABNORMAL_INT) ? TRUE : FALSE)

#define HAL_IS_FW_OWNBACK_INTR(u4IntrStatus) \
	((u4IntrStatus & WHISR_FW_OWN_BACK_INT) ? TRUE : FALSE)

#if (defined(CFG_SDIO_MAILBOX_EXTENSION) && (CFG_SDIO_MAILBOX_EXTENSION == 1))
#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data) \
{ \
	halPutMailbox(prAdapter, u4MboxId, u4Data); \
}

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data) \
{ \
	halGetMailbox(prAdapter, u4MboxId, pu4Data); \
}
#else
#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data) \
{ \
	ASSERT(u4MboxId < 2); \
	HAL_MCR_WR(prAdapter, \
		((u4MboxId == 0) ? MCR_H2DSM0R : MCR_H2DSM1R), \
		u4Data); \
}

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data) \
{ \
	ASSERT(u4MboxId < 2); \
	HAL_MCR_RD(prAdapter, \
		((u4MboxId == 0) ? MCR_D2HRM0R : MCR_D2HRM1R), \
		pu4Data); \
}
#endif

#define HAL_SET_MAILBOX_READ_CLEAR(prAdapter, fgEnableReadClear) \
{ \
	uint32_t u4Value = 0; \
	HAL_MCR_RD(prAdapter, MCR_WHCR, &u4Value);\
	HAL_MCR_WR(prAdapter, MCR_WHCR, \
		    (fgEnableReadClear) ? \
			(u4Value | WHCR_RECV_MAILBOX_RD_CLR_EN) : \
			(u4Value & ~WHCR_RECV_MAILBOX_RD_CLR_EN)); \
	prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear = fgEnableReadClear;\
}

#define HAL_GET_MAILBOX_READ_CLEAR(prAdapter) \
	(prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear)

#define HAL_READ_INT_STATUS(prAdapter, _pu4IntStatus) \
{ \
	kalDevReadIntStatus(prAdapter, _pu4IntStatus);\
}

#define HAL_HIF_INIT(_prAdapter) \
{ \
	halDevInit(_prAdapter);\
}

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable)

#define HAL_WAKE_UP_WIFI(_prAdapter) \
{ \
	halWakeUpWiFi(_prAdapter);\
}

#define HAL_IS_TX_DIRECT(_prAdapter) FALSE

#define HAL_IS_RX_DIRECT(_prAdapter) FALSE

#define HAL_UHW_RD(_prAdapter, _u4Offset, _pu4Value, _pucSts)

#define HAL_UHW_WR(_prAdapter, _u4Offset, _u4Value, _pucSts)

#define HAL_CANCEL_TX_RX(_prAdapter) nicSerStopTxRx(_prAdapter)

#define HAL_RESUME_TX_RX(_prAdapter) nicSerStartTxRx(_prAdapter)

#define HAL_TOGGLE_WFSYS_RST(_prAdapter)  halToggleWfsysRst(_prAdapter)
#endif

#if defined(_HIF_USB)
#define HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE(_prAdapter, \
	pu4AbnormalReason)

#define HAL_DISABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_ENABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_CFG_MAX_HIF_RX_LEN_NUM(_prAdapter, _ucNumOfRxLen)

#define HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter)

#define HAL_SET_INTR_STATUS_WRITE_1_CLEAR(prAdapter)

#define HAL_READ_INTR_STATUS(prAdapter, length, pvBuf)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount)

#define HAL_READ_RX_LENGTH(prAdapter, pu2Rx0Len, pu2Rx1Len)

#define HAL_GET_INTR_STATUS_FROM_ENHANCE_MODE_STRUCT(pvBuf, \
	u2Len, pu4Status)

#define HAL_GET_TX_STATUS_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu4BufOut, u4LenBufOut)

#define HAL_GET_RX_LENGTH_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu2Rx0Num, au2Rx0Len, pu2Rx1Num, au2Rx1Len)

#define HAL_GET_MAILBOX_FROM_ENHANCE_MODE_STRUCT(pvInBuf, \
	pu4Mailbox0, pu4Mailbox1)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount)

#define HAL_IS_TX_DONE_INTR(u4IntrStatus) FALSE

#define HAL_IS_RX_DONE_INTR(u4IntrStatus)

#define HAL_IS_ABNORMAL_INTR(u4IntrStatus)

#define HAL_IS_FW_OWNBACK_INTR(u4IntrStatus)

#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data)

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data) TRUE

#define HAL_SET_MAILBOX_READ_CLEAR(prAdapter, fgEnableReadClear)

#define HAL_GET_MAILBOX_READ_CLEAR(prAdapter) TRUE

#define HAL_WIFI_FUNC_POWER_ON(_prAdapter) \
	mtk_usb_vendor_request(_prAdapter->prGlueInfo, 0, \
		_prAdapter->chip_info->bus_info->u4device_vender_request_out, \
		VND_REQ_POWER_ON_WIFI, 0, 1, NULL, 0)

#define HAL_WIFI_FUNC_CHIP_RESET(_prAdapter) \
	mtk_usb_vendor_request(_prAdapter->prGlueInfo, 0, \
		_prAdapter->chip_info->bus_info->u4device_vender_request_out, \
		VND_REQ_POWER_ON_WIFI, 1, 1, NULL, 0)

#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	uint32_t u4Value = 0; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) \
	     == (_checkItem << prChipInfo->sw_ready_bit_offset)) \
		*_pfgResult = TRUE; \
} while (0)

#define HAL_VDR_PWR_ON_READY_CHECK(_prAdapter, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	uint32_t u4Value = 0; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->vdr_pwr_on, &u4Value); \
	if ((u4Value & prChipInfo->vdr_pwr_on_chk_bit) \
		 == prChipInfo->vdr_pwr_on_chk_bit) \
		*_pfgResult = TRUE; \
} while (0)


#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	uint32_t u4Value = 0; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) == 0) \
		*_pfgResult = TRUE; \
} while (0)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
do { \
	struct mt66xx_chip_info *prChipInfo = NULL; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &_u4Result); \
} while (0)

#define HAL_INTR_DISABLE(_prAdapter)

#define HAL_INTR_ENABLE(_prAdapter)

#define HAL_INTR_ENABLE_AND_LP_OWN_SET(_prAdapter)

#define HAL_READ_INT_STATUS(prAdapter, _pu4IntStatus) \
{ \
	halGetCompleteStatus(prAdapter, _pu4IntStatus); \
}

#define HAL_HIF_INIT(_prAdapter) \
{ \
	halDevInit(_prAdapter);\
}

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable) \
{ \
	halEnableFWDownload(_prAdapter, _fgEnable);\
}

#define HAL_WAKE_UP_WIFI(_prAdapter) \
{ \
	halWakeUpWiFi(_prAdapter);\
}

#define HAL_LP_OWN_RD(_prAdapter, _pfgResult)

#define HAL_LP_OWN_SET(_prAdapter, _pfgResult)

#define HAL_LP_OWN_CLR(_prAdapter, _pfgResult)

#define HAL_WRITE_TX_PORT(_prAdapter, _u4PortId, _u4Len, _pucBuf, _u4BufSize) \
{ \
	HAL_PORT_WR(_prAdapter, \
		_u4PortId, \
		_u4Len, \
		_pucBuf, \
		_u4BufSize/*temp!!*//*4KByte*/) \
}

#define HAL_IS_TX_DIRECT(_prAdapter) \
	((CFG_TX_DIRECT) ? TRUE : FALSE)

#define HAL_IS_RX_DIRECT(_prAdapter) \
	((CFG_RX_DIRECT) ? TRUE : FALSE)

#define HAL_UHW_RD(_prAdapter, _u4Offset, _pu4Value, _pucSts)	\
{ \
	*_pucSts = FALSE;			       \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	*_pucSts = kalDevUhwRegRead(_prAdapter->prGlueInfo, _u4Offset, \
				    _pu4Value); \
}

#define HAL_UHW_WR(_prAdapter, _u4Offset, _u4Value, _pucSts)	\
{ \
	*_pucSts = FALSE;			       \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	*_pucSts = kalDevUhwRegWrite(_prAdapter->prGlueInfo, _u4Offset, \
				     _u4Value); \
}

#define HAL_UHW_WR_FIELD(_prAdapter, _u4Offset, _u4FieldVal, _ucShft, _u4Mask) \
{ \
	uint32_t u4CrValue = 0; \
	kalDevUhwRegRead(_prAdapter->prGlueInfo, _u4Offset, &u4CrValue); \
	u4CrValue &= (~_u4Mask); \
	u4CrValue |= ((_u4FieldVal << _ucShft) & _u4Mask); \
	kalDevUhwRegWrite(_prAdapter->prGlueInfo, _u4Offset, u4CrValue); \
}

#define HAL_UHW_RD_FIELD(_prAdapter, _u4Offset, _ucShft, _u4Mask, pu4Val) \
{ \
	kalDevUhwRegRead(_prAdapter->prGlueInfo, _u4Offset, pu4Val); \
	*pu4Val = ((*pu4Val & _u4Mask) >> _ucShft); \
}

#define HAL_CANCEL_TX_RX(_prAdapter)    \
{ \
	glUsbSetStateSyncCtrl(&_prAdapter->prGlueInfo->rHifInfo, \
			      USB_STATE_TRX_FORBID); \
	nicSerStopTxRx(prAdapter); \
	halCancelTxRx(_prAdapter); \
}

#define HAL_RESUME_TX_RX(_prAdapter)    \
{ \
	glUsbSetStateSyncCtrl(&_prAdapter->prGlueInfo->rHifInfo, \
			      USB_STATE_LINK_UP); \
	nicSerStartTxRx(prAdapter); \
}

#define HAL_TOGGLE_WFSYS_RST(_prAdapter)    \
	halToggleWfsysRst(_prAdapter)
#endif

/*
 * TODO: os-related, should we separate the file just leave API here and
 * do the implementation in os folder (?)
 * followings are the necessary API for build PASS
 */
#ifdef CFG_VIRTUAL_OS
#define HAL_WRITE_TX_PORT(_prAd, _u4PortId, _u4Len, _pucBuf, _u4BufSize) \
	kal_virt_write_tx_port(_prAd, _u4PortId, _u4Len, _pucBuf, _u4BufSize)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
	kal_virt_get_wifi_func_stat(_prAdapter, &_u4Result)

#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
	kal_virt_chk_wifi_func_off(_prAdapter, _checkItem, _pfgResult)

#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
	kal_virt_chk_wifi_func_ready(_prAdapter, _checkItem, _pfgResult)

#define HAL_SET_MAILBOX_READ_CLEAR(_prAdapter, _fgEnableReadClear) \
	kal_virt_set_mailbox_readclear(_prAdapter, _fgEnableReadClear)

#define HAL_SET_INTR_STATUS_READ_CLEAR(_prAdapter) \
	kal_virt_set_int_stat_readclear(_prAdapter)

#define HAL_HIF_INIT(_prAdapter) \
	kal_virt_init_hif(_prAdapter)

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable) \
	kal_virt_enable_fwdl(_prAdapter, _fgEnable)

#define HAL_READ_INT_STATUS(_prAdapter, _pu4IntStatus) \
	kal_virt_get_int_status(_prAdapter, _pu4IntStatus)

#define HAL_IS_TX_DIRECT(_prAdapter) FALSE

#define HAL_IS_RX_DIRECT(_prAdapter) FALSE

#define HAL_UHW_RD(_prAdapter, _u4Offset, _pu4Value, _pfgSts)	\
	kal_virt_uhw_rd(_prAdapter, _u4Offset, _pu4Value, _pfgSts)

#define HAL_UHW_WR(_prAdapter, _u4Offset, _u4Value, _pfgSts)	\
	kal_virt_uhw_wr(_prAdapter, _u4Offset, _u4Value, _pfgSts)

#define HAL_CANCEL_TX_RX(_prAdapter)    \
	kal_virt_cancel_tx_rx(_prAdapter)

#define HAL_RESUME_TX_RX(_prAdapter)    \
	kal_virt_resume_tx_rx(_prAdapter)

#define HAL_TOGGLE_WFSYS_RST(_prAdapter)    \
	kal_virt_toggle_wfsys_rst(_prAdapter)

#endif
#define INVALID_VERSION 0xFFFF /* used by HW/FW version */
/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

u_int8_t halVerifyChipID(struct ADAPTER *prAdapter);
uint32_t halGetChipHwVer(struct ADAPTER *prAdapter);
uint32_t halGetChipSwVer(struct ADAPTER *prAdapter);

uint32_t halRxWaitResponse(struct ADAPTER *prAdapter,
	uint8_t ucPortIdx, uint8_t *pucRspBuffer,
	uint32_t u4MaxRespBufferLen, uint32_t *pu4Length,
	uint32_t u4WaitingInterval, uint32_t u4TimeoutValue);

void halEnableInterrupt(struct ADAPTER *prAdapter);
void halDisableInterrupt(struct ADAPTER *prAdapter);

#if (CFG_MTK_WIFI_DRV_OWN_DEBUG_MODE == 1)
uint32_t halSetDriverOwn(struct ADAPTER *prAdapter,
	struct DRV_OWN_INFO *prDrvOwnInfo);
void halSetFWOwn(struct ADAPTER *prAdapter,
	u_int8_t fgEnableGlobalInt,
	enum ENUM_DRV_OWN_SRC eDrvOwnSrc);
uint32_t halUpdateDrvOwnInfo(struct ADAPTER *prAdapter,
			     struct DRV_OWN_INFO *prDrvOwnInfo,
			     enum DRV_OWN_INFO_ACTION eAction,
			     uint32_t u4Result,
			     uint8_t **ppucLog);
void halAccessDrvOwnTable(struct ADAPTER *prAdapter,
			enum DRV_OWN_INFO_ACTION eAction);
#else
uint32_t halSetDriverOwn(struct ADAPTER *prAdapter);
void halSetFWOwn(struct ADAPTER *prAdapter,
	u_int8_t fgEnableGlobalInt);
#endif

void halDevInit(struct ADAPTER *prAdapter);
void halEnableFWDownload(struct ADAPTER *prAdapter,
	u_int8_t fgEnable);
void halWakeUpWiFi(struct ADAPTER *prAdapter);
void halTxCancelSendingCmd(struct ADAPTER *prAdapter,
	struct CMD_INFO *prCmdInfo);
void halTxCancelAllSending(struct ADAPTER *prAdapter);
void halCancelTxRx(struct ADAPTER *prAdapter);
uint32_t halToggleWfsysRst(struct ADAPTER *prAdapter);
u_int8_t halTxIsCmdBufEnough(struct ADAPTER *prAdapter);
u_int8_t halTxIsDataBufEnough(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);
void halProcessTxInterrupt(struct ADAPTER *prAdapter);
uint32_t halTxPollingResource(struct ADAPTER *prAdapter,
	uint8_t ucTC);
void halSerHifReset(struct ADAPTER *prAdapter);

void halProcessRxInterrupt(struct ADAPTER *prAdapter);
void halProcessAbnormalInterrupt(struct ADAPTER *prAdapter);
void halProcessSoftwareInterrupt(struct ADAPTER *prAdapter);
/* Hif power off wifi */
uint32_t halHifPowerOffWifi(struct ADAPTER *prAdapter);

bool halHifSwInfoInit(struct ADAPTER *prAdapter);
void halHifSwInfoUnInit(struct GLUE_INFO *prGlueInfo);
void halRxProcessMsduReport(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);
void halMsduReportStats(struct ADAPTER *prAdapter, uint32_t u4Token,
	uint32_t u4MacLatency, uint32_t u4AirLatency, uint32_t u4Stat);
u_int8_t halProcessToken(struct ADAPTER *prAdapter,
	uint32_t u4Token,
	struct QUE *prFreeQueue);
uint32_t halTxGetDataPageCount(struct ADAPTER *prAdapter,
	uint32_t u4FrameLength, u_int8_t fgIncludeDesc);
uint32_t halTxGetCmdPageCount(struct ADAPTER *prAdapter,
	uint32_t u4FrameLength, u_int8_t fgIncludeDesc);
uint32_t halDumpHifStatus(struct ADAPTER *prAdapter,
	uint8_t *pucBuf, uint32_t u4Max);
u_int8_t halIsPendingRx(struct ADAPTER *prAdapter);
uint32_t halGetValidCoalescingBufSize(struct ADAPTER *prAdapter);
uint32_t halAllocateIOBuffer(struct ADAPTER *prAdapter);
uint32_t halReleaseIOBuffer(struct ADAPTER *prAdapter);
void halDeAggRxPktWorker(struct work_struct *work);
void halRxTasklet(uintptr_t data);
void halRxWork(struct GLUE_INFO *prGlueInfo);
void halTxWork(struct GLUE_INFO *prGlueInfo);
void halTxCompleteTasklet(uintptr_t data);
void halPrintHifDbgInfo(struct ADAPTER *prAdapter);
u_int8_t halIsTxResourceControlEn(struct ADAPTER *prAdapter);
void halTxResourceResetHwTQCounter(struct ADAPTER *prAdapter);
uint32_t halGetHifTxDataPageSize(struct ADAPTER *prAdapter);
uint32_t halGetHifTxCmdPageSize(struct ADAPTER *prAdapter);
void halTxReturnFreeResource(struct ADAPTER *prAdapter,
	uint16_t *au2TxDoneCnt);
void halTxGetFreeResource(struct ADAPTER *prAdapter,
	uint16_t *au2TxDoneCnt, uint16_t *au2TxRlsCnt);
void halRestoreTxResource(struct ADAPTER *prAdapter);
void halRestoreTxResource_v1(struct ADAPTER *prAdapter);
void halUpdateTxDonePendingCount_v1(struct ADAPTER *prAdapter,
	u_int8_t isIncr, uint8_t ucTc, uint16_t u2Cnt);
void halUpdateTxDonePendingCount(struct ADAPTER *prAdapter,
	u_int8_t isIncr, uint8_t ucTc, uint32_t u4Len);
void halTxReturnFreeResource_v1(struct ADAPTER *prAdapter,
	uint16_t *au2TxDoneCnt);
uint8_t halTxRingDataSelect(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);
void halUpdateTxMaxQuota(struct ADAPTER *prAdapter);
void halTriggerSwInterrupt(struct ADAPTER *prAdapter, uint32_t u4Bit);
u_int8_t halTxIsBssCntFull(struct ADAPTER *prAdapter, uint8_t ucBssIndex);
void halUpdateBssTokenCnt(struct ADAPTER *prAdapter, uint8_t ucBssIndex);
#if (CFG_TX_HIF_CREDIT_FEATURE == 1)
uint32_t halGetBssTxCredit(struct ADAPTER *prAdapter, uint8_t ucBssIndex);
void halAdjustBssTxCredit(struct ADAPTER *prAdapter, uint8_t ucBssIndex);
u_int8_t halTxIsBssCreditCntFull(uint32_t u4TxCredit);
#endif
#if defined(_HIF_AXI)
void halSetHifIntEvent(struct GLUE_INFO *pr, unsigned long ulBit);
#endif
void halUpdateHifConfig(struct ADAPTER *prAdapter);
void halDumpHifStats(struct ADAPTER *prAdapter);
#if defined(_HIF_USB)
void halSerSyncTimerHandler(struct ADAPTER *prAdapter);
#endif /* defined(_HIF_USB) */
u_int8_t halIsHifStateReady(struct GLUE_INFO *prGlueInfo, uint8_t *pucState);
bool halIsHifStateLinkup(struct ADAPTER *prAdapter);
bool halIsHifStateSuspend(struct ADAPTER *prAdapter);

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
void halRxReceiveRFBs(struct ADAPTER *prAdapter, uint32_t u4Port,
	uint8_t fgRxData);
u_int8_t halWpdmaWaitIdle(struct GLUE_INFO *prGlueInfo,
	int32_t round, int32_t wait_us);
bool halWpdmaAllocRxRing(struct GLUE_INFO *prGlueInfo, uint32_t u4Num,
			 uint32_t u4Size, uint32_t u4DescSize,
			 uint32_t u4BufSize, bool fgAllocMem);
uint8_t halRingDataSelectByWmmIndex(
	struct ADAPTER *prAdapter,
	uint8_t ucWmmIndex);
#endif /* defined(_HIF_PCIE) || defined(_HIF_AXI) */
uint32_t halSetSuspendFlagToFw(struct ADAPTER *prAdapter,
	u_int8_t fgSuspend);
u_int32_t halTxGetFreeCmdCnt(struct ADAPTER *prAdapter);

#endif /* _HAL_H */
