/* SPDX-License-Identifier: BSD-2-Clause */
//[File]            : top_misc.h
//[Revision time]   : Fri Jul  1 18:50:30 2022
//[Description]     : This file is auto generated by CODA
//[Copyright]       : Copyright (C) 2022 Mediatek Incorportion. All rights reserved.

#ifndef __TOP_MISC_REGS_H__
#define __TOP_MISC_REGS_H__

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif


//****************************************************************************
//
//                     TOP_MISC CR Definitions                     
//
//****************************************************************************

#define TOP_MISC_BASE                                          0x70006000

#define TOP_MISC_GPIO_IES_ADDR                                 (TOP_MISC_BASE + 0x040) // 6040
#define TOP_MISC_PINMUX_MISC1_ADDR                             (TOP_MISC_BASE + 0x080) // 6080
#define TOP_MISC_PINMUX_MISC2_ADDR                             (TOP_MISC_BASE + 0x084) // 6084
#define TOP_MISC_RSRV_ALL0_0_ADDR                              (TOP_MISC_BASE + 0x200) // 6200
#define TOP_MISC_RSRV_ALL0_1_ADDR                              (TOP_MISC_BASE + 0x204) // 6204
#define TOP_MISC_RSRV_ALL0_2_ADDR                              (TOP_MISC_BASE + 0x208) // 6208
#define TOP_MISC_RSRV_ALL1_0_ADDR                              (TOP_MISC_BASE + 0x20C) // 620C
#define TOP_MISC_RSRV_ALL1_1_ADDR                              (TOP_MISC_BASE + 0x210) // 6210
#define TOP_MISC_D2D_PAD_SLP_ADDR                              (TOP_MISC_BASE + 0x214) // 6214
#define TOP_MISC_FORCE_HIF_AXB_MODE_ADDR                       (TOP_MISC_BASE + 0x218) // 6218
#define TOP_MISC_MISC_CTRL_ADDR                                (TOP_MISC_BASE + 0x300) // 6300
#define TOP_MISC_MISC_INTR_ADDR                                (TOP_MISC_BASE + 0x304) // 6304
#define TOP_MISC_TM_JTAG_ADDR                                  (TOP_MISC_BASE + 0x400) // 6400




/* =====================================================================================

  ---GPIO_IES (0x70006000 + 0x040)---

    GPIO_PD1_RESET[30..0]        - (RW) PAD IES pin control register
    HOST_ACK[31]                 - (RW) Host_ack for SDIO

 =====================================================================================*/
#define TOP_MISC_GPIO_IES_HOST_ACK_ADDR                        TOP_MISC_GPIO_IES_ADDR
#define TOP_MISC_GPIO_IES_HOST_ACK_MASK                        0x80000000                // HOST_ACK[31]
#define TOP_MISC_GPIO_IES_HOST_ACK_SHFT                        31
#define TOP_MISC_GPIO_IES_GPIO_PD1_RESET_ADDR                  TOP_MISC_GPIO_IES_ADDR
#define TOP_MISC_GPIO_IES_GPIO_PD1_RESET_MASK                  0x7FFFFFFF                // GPIO_PD1_RESET[30..0]
#define TOP_MISC_GPIO_IES_GPIO_PD1_RESET_SHFT                  0

/* =====================================================================================

  ---PINMUX_MISC1 (0x70006000 + 0x080)---

    RESERVED0[14..0]             - (RO) Reserved bits
    TMBIST_MODE_EN[15]           - (RW) TMBIST_MODE_EN
    SF_TEST_MODE[16]             - (RW) Serial Flash Mode switch 
                                     1 : Digital mode 
                                     0 : SF mode
    rsv_cr_17[17]                - (RW) rsv_cr
    WF_HB_EN_SWITCH[18]          - (RW) WF_HB_EN switch
    EINT_SDIO_PCIE_SW[19]        - (RW) PCIE/SDIO switch
    CFG_2WIRE_JTAG[20]           - (RW) 2WIRE_JTAG(only for WF MCU)
    RESERVED21[31..21]           - (RO) Reserved bits

 =====================================================================================*/
#define TOP_MISC_PINMUX_MISC1_CFG_2WIRE_JTAG_ADDR              TOP_MISC_PINMUX_MISC1_ADDR
#define TOP_MISC_PINMUX_MISC1_CFG_2WIRE_JTAG_MASK              0x00100000                // CFG_2WIRE_JTAG[20]
#define TOP_MISC_PINMUX_MISC1_CFG_2WIRE_JTAG_SHFT              20
#define TOP_MISC_PINMUX_MISC1_EINT_SDIO_PCIE_SW_ADDR           TOP_MISC_PINMUX_MISC1_ADDR
#define TOP_MISC_PINMUX_MISC1_EINT_SDIO_PCIE_SW_MASK           0x00080000                // EINT_SDIO_PCIE_SW[19]
#define TOP_MISC_PINMUX_MISC1_EINT_SDIO_PCIE_SW_SHFT           19
#define TOP_MISC_PINMUX_MISC1_WF_HB_EN_SWITCH_ADDR             TOP_MISC_PINMUX_MISC1_ADDR
#define TOP_MISC_PINMUX_MISC1_WF_HB_EN_SWITCH_MASK             0x00040000                // WF_HB_EN_SWITCH[18]
#define TOP_MISC_PINMUX_MISC1_WF_HB_EN_SWITCH_SHFT             18
#define TOP_MISC_PINMUX_MISC1_rsv_cr_17_ADDR                   TOP_MISC_PINMUX_MISC1_ADDR
#define TOP_MISC_PINMUX_MISC1_rsv_cr_17_MASK                   0x00020000                // rsv_cr_17[17]
#define TOP_MISC_PINMUX_MISC1_rsv_cr_17_SHFT                   17
#define TOP_MISC_PINMUX_MISC1_SF_TEST_MODE_ADDR                TOP_MISC_PINMUX_MISC1_ADDR
#define TOP_MISC_PINMUX_MISC1_SF_TEST_MODE_MASK                0x00010000                // SF_TEST_MODE[16]
#define TOP_MISC_PINMUX_MISC1_SF_TEST_MODE_SHFT                16
#define TOP_MISC_PINMUX_MISC1_TMBIST_MODE_EN_ADDR              TOP_MISC_PINMUX_MISC1_ADDR
#define TOP_MISC_PINMUX_MISC1_TMBIST_MODE_EN_MASK              0x00008000                // TMBIST_MODE_EN[15]
#define TOP_MISC_PINMUX_MISC1_TMBIST_MODE_EN_SHFT              15

/* =====================================================================================

  ---PINMUX_MISC2 (0x70006000 + 0x084)---

    RESERVED0[28..0]             - (RO) Reserved bits
    CFG_BT1_2WIRE_JTAG[29]       - (RW) BT1_2WIRE_JTAG
    CFG_BT0_2WIRE_JTAG[30]       - (RW) BT0_2WIRE_JTAG
    GPIO5_LOCK[31]               - (RW) Lock GPIO5 to GPIO output mode (only can be set once)

 =====================================================================================*/
#define TOP_MISC_PINMUX_MISC2_GPIO5_LOCK_ADDR                  TOP_MISC_PINMUX_MISC2_ADDR
#define TOP_MISC_PINMUX_MISC2_GPIO5_LOCK_MASK                  0x80000000                // GPIO5_LOCK[31]
#define TOP_MISC_PINMUX_MISC2_GPIO5_LOCK_SHFT                  31
#define TOP_MISC_PINMUX_MISC2_CFG_BT0_2WIRE_JTAG_ADDR          TOP_MISC_PINMUX_MISC2_ADDR
#define TOP_MISC_PINMUX_MISC2_CFG_BT0_2WIRE_JTAG_MASK          0x40000000                // CFG_BT0_2WIRE_JTAG[30]
#define TOP_MISC_PINMUX_MISC2_CFG_BT0_2WIRE_JTAG_SHFT          30
#define TOP_MISC_PINMUX_MISC2_CFG_BT1_2WIRE_JTAG_ADDR          TOP_MISC_PINMUX_MISC2_ADDR
#define TOP_MISC_PINMUX_MISC2_CFG_BT1_2WIRE_JTAG_MASK          0x20000000                // CFG_BT1_2WIRE_JTAG[29]
#define TOP_MISC_PINMUX_MISC2_CFG_BT1_2WIRE_JTAG_SHFT          29

/* =====================================================================================

  ---TOP_MISC_RSRV_ALL0_0 (0x70006000 + 0x200)---

    TOP_MISC_RSRV_ALL0_0[31..0]  - (RW) Reserved CR with default value 0 for TOP misc

 =====================================================================================*/
#define TOP_MISC_RSRV_ALL0_0_TOP_MISC_RSRV_ALL0_0_ADDR         TOP_MISC_RSRV_ALL0_0_ADDR
#define TOP_MISC_RSRV_ALL0_0_TOP_MISC_RSRV_ALL0_0_MASK         0xFFFFFFFF                // TOP_MISC_RSRV_ALL0_0[31..0]
#define TOP_MISC_RSRV_ALL0_0_TOP_MISC_RSRV_ALL0_0_SHFT         0

/* =====================================================================================

  ---TOP_MISC_RSRV_ALL0_1 (0x70006000 + 0x204)---

    TOP_MISC_RSRV_ALL0_1[31..0]  - (RW) Reserved CR with default value 0 for TOP misc

 =====================================================================================*/
#define TOP_MISC_RSRV_ALL0_1_TOP_MISC_RSRV_ALL0_1_ADDR         TOP_MISC_RSRV_ALL0_1_ADDR
#define TOP_MISC_RSRV_ALL0_1_TOP_MISC_RSRV_ALL0_1_MASK         0xFFFFFFFF                // TOP_MISC_RSRV_ALL0_1[31..0]
#define TOP_MISC_RSRV_ALL0_1_TOP_MISC_RSRV_ALL0_1_SHFT         0

/* =====================================================================================

  ---TOP_MISC_RSRV_ALL0_2 (0x70006000 + 0x208)---

    TOP_MISC_RSRV_ALL0_2[31..0]  - (RW) Reserved CR with default value 0 for TOP misc

 =====================================================================================*/
#define TOP_MISC_RSRV_ALL0_2_TOP_MISC_RSRV_ALL0_2_ADDR         TOP_MISC_RSRV_ALL0_2_ADDR
#define TOP_MISC_RSRV_ALL0_2_TOP_MISC_RSRV_ALL0_2_MASK         0xFFFFFFFF                // TOP_MISC_RSRV_ALL0_2[31..0]
#define TOP_MISC_RSRV_ALL0_2_TOP_MISC_RSRV_ALL0_2_SHFT         0

/* =====================================================================================

  ---TOP_MISC_RSRV_ALL1_0 (0x70006000 + 0x20C)---

    TOP_MISC_RSRV_ALL1_0[31..0]  - (RW) Reserved CR with default value 1 for TOP misc

 =====================================================================================*/
#define TOP_MISC_RSRV_ALL1_0_TOP_MISC_RSRV_ALL1_0_ADDR         TOP_MISC_RSRV_ALL1_0_ADDR
#define TOP_MISC_RSRV_ALL1_0_TOP_MISC_RSRV_ALL1_0_MASK         0xFFFFFFFF                // TOP_MISC_RSRV_ALL1_0[31..0]
#define TOP_MISC_RSRV_ALL1_0_TOP_MISC_RSRV_ALL1_0_SHFT         0

/* =====================================================================================

  ---TOP_MISC_RSRV_ALL1_1 (0x70006000 + 0x210)---

    TOP_MISC_RSRV_ALL1_1[31..0]  - (RW) Reserved CR with default value 1 for TOP misc

 =====================================================================================*/
#define TOP_MISC_RSRV_ALL1_1_TOP_MISC_RSRV_ALL1_1_ADDR         TOP_MISC_RSRV_ALL1_1_ADDR
#define TOP_MISC_RSRV_ALL1_1_TOP_MISC_RSRV_ALL1_1_MASK         0xFFFFFFFF                // TOP_MISC_RSRV_ALL1_1[31..0]
#define TOP_MISC_RSRV_ALL1_1_TOP_MISC_RSRV_ALL1_1_SHFT         0

/* =====================================================================================

  ---D2D_PAD_SLP (0x70006000 + 0x214)---

    D2D_PAD_SLP_CLK_OE[0]        - (RW) D2D_PAD_SLP_CLK_OE
    RESERVED1[31..1]             - (RO) Reserved bits

 =====================================================================================*/
#define TOP_MISC_D2D_PAD_SLP_D2D_PAD_SLP_CLK_OE_ADDR           TOP_MISC_D2D_PAD_SLP_ADDR
#define TOP_MISC_D2D_PAD_SLP_D2D_PAD_SLP_CLK_OE_MASK           0x00000001                // D2D_PAD_SLP_CLK_OE[0]
#define TOP_MISC_D2D_PAD_SLP_D2D_PAD_SLP_CLK_OE_SHFT           0

/* =====================================================================================

  ---FORCE_HIF_AXB_MODE (0x70006000 + 0x218)---

    FORCE_HIF_AXB_MODE[0]        - (RW) FORCE_HIF_AXB_MODE
    RESERVED1[1]                 - (RO) Reserved bits
    HIF_TESTMODE_PROBE_SEL[3..2] - (RW) HIF_TESTMODE_PROBE_SEL
    RESERVED4[31..4]             - (RO) Reserved bits

 =====================================================================================*/
#define TOP_MISC_FORCE_HIF_AXB_MODE_HIF_TESTMODE_PROBE_SEL_ADDR TOP_MISC_FORCE_HIF_AXB_MODE_ADDR
#define TOP_MISC_FORCE_HIF_AXB_MODE_HIF_TESTMODE_PROBE_SEL_MASK 0x0000000C                // HIF_TESTMODE_PROBE_SEL[3..2]
#define TOP_MISC_FORCE_HIF_AXB_MODE_HIF_TESTMODE_PROBE_SEL_SHFT 2
#define TOP_MISC_FORCE_HIF_AXB_MODE_FORCE_HIF_AXB_MODE_ADDR    TOP_MISC_FORCE_HIF_AXB_MODE_ADDR
#define TOP_MISC_FORCE_HIF_AXB_MODE_FORCE_HIF_AXB_MODE_MASK    0x00000001                // FORCE_HIF_AXB_MODE[0]
#define TOP_MISC_FORCE_HIF_AXB_MODE_FORCE_HIF_AXB_MODE_SHFT    0

/* =====================================================================================

  ---MISC_CTRL (0x70006000 + 0x300)---

    RESERVED0[3..0]              - (RO) Reserved bits
    GIO_LB_EN[4]                 - (RW) GPIO LoopBack Enable
                                     1: Connect GPO to GPI directly
                                     0: No loopback
    ZBIPOL[5]                    - (RW) HIF ZB_INT interrupt polarity. The Host interrupt default value is high.
                                     0: Interrupt is HIGH active
                                     1: Interrupt is LOW active
    BIPOL[6]                     - (RW) HIF C_INT interrupt polarity. The Host interrupt default value is high.
                                     0: Interrupt is HIGH active
                                     1: Interrupt is LOW active
    HIPOL[7]                     - (RW) HIF W_INT interrupt polarity. The Host interrupt default value is high.
                                     0: Interrupt is HIGH active
                                     1: Interrupt is LOW active
    AIPOL[8]                     - (RW) All interrupt polarity. The Host interrupt default value is high.
                                     1: Interrupt is HIGH active
                                     0: Interrupt is LOW active
    APRD[9]                      - (RW) All interrupt periodic mode. The period will be 8 RTC periods.
                                     1: Periodic mode
                                     0: Non-period mode
    DMODE[10]                    - (RW) All interrupt hirect mode enable
                                     1: Enable
                                     0: Disable
    RESERVED11[12..11]           - (RO) Reserved bits
    BAEN[13]                     - (RW) BGF interrupt send to host all interrupt enable
                                     1: Enable
                                     0: Disable
    HFCAEN[14]                   - (RW) HIF C_INT interrupt send to host all interrupt enable
                                     1: Enable
                                     0: Disable
    HFWAEN[15]                   - (RW) HIF W_INT interrupt send to host all interrupt enable
                                     1: Enable
                                     0: Disable
    RESERVED16[31..16]           - (RO) Reserved bits

 =====================================================================================*/
#define TOP_MISC_MISC_CTRL_HFWAEN_ADDR                         TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_HFWAEN_MASK                         0x00008000                // HFWAEN[15]
#define TOP_MISC_MISC_CTRL_HFWAEN_SHFT                         15
#define TOP_MISC_MISC_CTRL_HFCAEN_ADDR                         TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_HFCAEN_MASK                         0x00004000                // HFCAEN[14]
#define TOP_MISC_MISC_CTRL_HFCAEN_SHFT                         14
#define TOP_MISC_MISC_CTRL_BAEN_ADDR                           TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_BAEN_MASK                           0x00002000                // BAEN[13]
#define TOP_MISC_MISC_CTRL_BAEN_SHFT                           13
#define TOP_MISC_MISC_CTRL_DMODE_ADDR                          TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_DMODE_MASK                          0x00000400                // DMODE[10]
#define TOP_MISC_MISC_CTRL_DMODE_SHFT                          10
#define TOP_MISC_MISC_CTRL_APRD_ADDR                           TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_APRD_MASK                           0x00000200                // APRD[9]
#define TOP_MISC_MISC_CTRL_APRD_SHFT                           9
#define TOP_MISC_MISC_CTRL_AIPOL_ADDR                          TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_AIPOL_MASK                          0x00000100                // AIPOL[8]
#define TOP_MISC_MISC_CTRL_AIPOL_SHFT                          8
#define TOP_MISC_MISC_CTRL_HIPOL_ADDR                          TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_HIPOL_MASK                          0x00000080                // HIPOL[7]
#define TOP_MISC_MISC_CTRL_HIPOL_SHFT                          7
#define TOP_MISC_MISC_CTRL_BIPOL_ADDR                          TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_BIPOL_MASK                          0x00000040                // BIPOL[6]
#define TOP_MISC_MISC_CTRL_BIPOL_SHFT                          6
#define TOP_MISC_MISC_CTRL_ZBIPOL_ADDR                         TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_ZBIPOL_MASK                         0x00000020                // ZBIPOL[5]
#define TOP_MISC_MISC_CTRL_ZBIPOL_SHFT                         5
#define TOP_MISC_MISC_CTRL_GIO_LB_EN_ADDR                      TOP_MISC_MISC_CTRL_ADDR
#define TOP_MISC_MISC_CTRL_GIO_LB_EN_MASK                      0x00000010                // GIO_LB_EN[4]
#define TOP_MISC_MISC_CTRL_GIO_LB_EN_SHFT                      4

/* =====================================================================================

  ---MISC_INTR (0x70006000 + 0x304)---

    BGF_FW_INTB[0]               - (RW) FW control BGF_INT_B
    WIFI_FW_INTB[1]              - (RW) FW control WIFI_INT_B
    ZB_FW_INTB[2]                - (RW) FW control ZB_INT_B
    RESERVED3[31..3]             - (RO) Reserved bits

 =====================================================================================*/
#define TOP_MISC_MISC_INTR_ZB_FW_INTB_ADDR                     TOP_MISC_MISC_INTR_ADDR
#define TOP_MISC_MISC_INTR_ZB_FW_INTB_MASK                     0x00000004                // ZB_FW_INTB[2]
#define TOP_MISC_MISC_INTR_ZB_FW_INTB_SHFT                     2
#define TOP_MISC_MISC_INTR_WIFI_FW_INTB_ADDR                   TOP_MISC_MISC_INTR_ADDR
#define TOP_MISC_MISC_INTR_WIFI_FW_INTB_MASK                   0x00000002                // WIFI_FW_INTB[1]
#define TOP_MISC_MISC_INTR_WIFI_FW_INTB_SHFT                   1
#define TOP_MISC_MISC_INTR_BGF_FW_INTB_ADDR                    TOP_MISC_MISC_INTR_ADDR
#define TOP_MISC_MISC_INTR_BGF_FW_INTB_MASK                    0x00000001                // BGF_FW_INTB[0]
#define TOP_MISC_MISC_INTR_BGF_FW_INTB_SHFT                    0

/* =====================================================================================

  ---TM_JTAG (0x70006000 + 0x400)---

    JTAG_TMS[0]                  - (RW) JTAG TMS
    JTAG_TCK[1]                  - (RW) JTAG TCK
    JTAG_TRSTN[2]                - (RW) JTAG TRSTN
    JTAG_TDI[3]                  - (RW) JTAG TDI
    JTAG_TDO[4]                  - (RO) JTAG TDO
    RESERVED5[30..5]             - (RO) Reserved bits
    TMBIST_MODE[31]              - (RW) JTAG mode enable
                                     1'b0 : disable
                                     1'b1 : enable

 =====================================================================================*/
#define TOP_MISC_TM_JTAG_TMBIST_MODE_ADDR                      TOP_MISC_TM_JTAG_ADDR
#define TOP_MISC_TM_JTAG_TMBIST_MODE_MASK                      0x80000000                // TMBIST_MODE[31]
#define TOP_MISC_TM_JTAG_TMBIST_MODE_SHFT                      31
#define TOP_MISC_TM_JTAG_JTAG_TDO_ADDR                         TOP_MISC_TM_JTAG_ADDR
#define TOP_MISC_TM_JTAG_JTAG_TDO_MASK                         0x00000010                // JTAG_TDO[4]
#define TOP_MISC_TM_JTAG_JTAG_TDO_SHFT                         4
#define TOP_MISC_TM_JTAG_JTAG_TDI_ADDR                         TOP_MISC_TM_JTAG_ADDR
#define TOP_MISC_TM_JTAG_JTAG_TDI_MASK                         0x00000008                // JTAG_TDI[3]
#define TOP_MISC_TM_JTAG_JTAG_TDI_SHFT                         3
#define TOP_MISC_TM_JTAG_JTAG_TRSTN_ADDR                       TOP_MISC_TM_JTAG_ADDR
#define TOP_MISC_TM_JTAG_JTAG_TRSTN_MASK                       0x00000004                // JTAG_TRSTN[2]
#define TOP_MISC_TM_JTAG_JTAG_TRSTN_SHFT                       2
#define TOP_MISC_TM_JTAG_JTAG_TCK_ADDR                         TOP_MISC_TM_JTAG_ADDR
#define TOP_MISC_TM_JTAG_JTAG_TCK_MASK                         0x00000002                // JTAG_TCK[1]
#define TOP_MISC_TM_JTAG_JTAG_TCK_SHFT                         1
#define TOP_MISC_TM_JTAG_JTAG_TMS_ADDR                         TOP_MISC_TM_JTAG_ADDR
#define TOP_MISC_TM_JTAG_JTAG_TMS_MASK                         0x00000001                // JTAG_TMS[0]
#define TOP_MISC_TM_JTAG_JTAG_TMS_SHFT                         0

#ifdef __cplusplus
}
#endif

#endif // __TOP_MISC_REGS_H__
