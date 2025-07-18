// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek USB3.1 gen2 xsphy Driver
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "phy-mtk-io.h"

/* u2 phy banks */
#define SSUSB_SIFSLV_MISC		0x000
#define SSUSB_SIFSLV_U2FREQ		0x100
#define SSUSB_SIFSLV_U2PHY_COM	0x300

/* u3 phy shared banks */
#define SSPXTP_SIFSLV_DIG_GLB		0x000
#define SSPXTP_SIFSLV_PHYA_GLB		0x100

/* u3 phy banks */
#define SSPXTP_SIFSLV_DIG_LN_TOP	0x000
#define SSPXTP_SIFSLV_DIG_LN_TX0	0x100
#define SSPXTP_SIFSLV_DIG_LN_RX0	0x200
#define SSPXTP_SIFSLV_DIG_LN_DAIF	0x300
#define SSPXTP_SIFSLV_PHYA_LN		0x400

#define XSP_MISC_REG0		((SSUSB_SIFSLV_MISC) + 0x00)
#define MR0_RG_MISC_TIMER_1US		GENMASK(15, 8)
#define MR0_RG_MISC_STABLE_TIME		GENMASK(23, 16)
#define MR0_RG_MISC_BANDGAP_STABLE_TIME		GENMASK(31, 24)

#define XSP_MISC_REG1		((SSUSB_SIFSLV_MISC) + 0x04)
#define MR1_RG_MISC_PLL_STABLE_SEL	BIT(16)

#define XSP_U2FREQ_FMCR0	((SSUSB_SIFSLV_U2FREQ) + 0x00)
#define P2F_RG_FREQDET_EN	BIT(24)
#define P2F_RG_CYCLECNT		GENMASK(23, 0)

#define XSP_U2FREQ_MMONR0  ((SSUSB_SIFSLV_U2FREQ) + 0x0c)

#define XSP_U2FREQ_FMMONR1	((SSUSB_SIFSLV_U2FREQ) + 0x10)
#define P2F_RG_FRCK_EN		BIT(8)
#define P2F_USB_FM_VALID	BIT(0)

#define XSP_USBPHYACR0	((SSUSB_SIFSLV_U2PHY_COM) + 0x00)
#define P2A0_RG_USB20_CHP_EN BIT(1)
#define P2A0_RG_USB20_TX_PH_ROT_SEL		GENMASK(26, 24)
#define P2A0_RG_INTR_EN	BIT(5)

#define XSP_USBPHYACR1		((SSUSB_SIFSLV_U2PHY_COM) + 0x04)
#define P2A1_RG_VRT_SEL			GENMASK(14, 12)
#define P2A1_RG_VRT_SEL_MASK	(0x7)
#define P2A1_RG_VRT_SEL_OFST	(12)
#define P2A1_RG_TERM_SEL		GENMASK(10, 8)
#define P2A1_RG_TERM_SEL_MASK	(0x7)
#define P2A1_RG_TERM_SEL_OFST	(8)

#define XSP_USBPHYACR2		((SSUSB_SIFSLV_U2PHY_COM) + 0x08)

#define XSP_USBPHYACR4		((SSUSB_SIFSLV_U2PHY_COM) + 0x10)
#define P2A4_RG_USB20_FS_CR		GENMASK(10, 8)

#define XSP_USBPHYACR5		((SSUSB_SIFSLV_U2PHY_COM) + 0x014)
#define P2A5_RG_HSTX_SRCAL_EN	BIT(15)
#define P2A5_RG_HSTX_SRCTRL		GENMASK(14, 12)

#define XSP_USBPHYACR6		((SSUSB_SIFSLV_U2PHY_COM) + 0x018)
#define P2A6_RG_U2_PHY_REV6		GENMASK(31, 30)
#define P2A6_RG_U2_PHY_REV6_VAL(x)	((0x3 & (x)) << 30)
#define P2A6_RG_U2_PHY_REV6_MASK	(0x3)
#define P2A6_RG_U2_PHY_REV6_OFET	(30)
#define P2A6_RG_U2_PHY_REV1		BIT(25)
#define P2A6_RG_BC11_SW_EN	BIT(23)
#define P2A6_RG_OTG_VBUSCMP_EN	BIT(20)
#define P2A6_RG_U2_DISCTH		GENMASK(7, 4)
#define P2A6_RG_U2_DISCTH_MASK	(0xf)
#define P2A6_RG_U2_DISCTH_OFET	(4)
#define P2A6_RG_U2_SQTH			GENMASK(3, 0)
#define P2A6_RG_U2_SQTH_MASK		(0xf)
#define P2A6_RG_U2_SQTH_OFET		(0)


#define XSP_USBPHYACR3		((SSUSB_SIFSLV_U2PHY_COM) + 0x01c)
#define P2A3_RG_USB20_PUPD_BIST_EN	BIT(12)
#define P2A3_RG_USB20_EN_PU_DP		BIT(9)

#define XSP_U2PHYACR4		((SSUSB_SIFSLV_U2PHY_COM) + 0x020)
#define P2A4_RG_USB20_GPIO_CTL		BIT(9)
#define P2A4_USB20_GPIO_MODE		BIT(8)
#define P2A4_U2_GPIO_CTR_MSK (P2A4_RG_USB20_GPIO_CTL | P2A4_USB20_GPIO_MODE)

#define XSP_USBPHYMON0		((SSUSB_SIFSLV_U2PHY_COM) + 0x024)
#define USB20_GPIO_DM_RO		BIT(1)
#define USB20_GPIO_DP_RO		BIT(0)
#define USB20_GPIO_DM_DP_MASK (USB20_GPIO_DM_RO | USB20_GPIO_DP_RO)

#define XSP_USBPHYA_RESERVE	((SSUSB_SIFSLV_U2PHY_COM) + 0x030)
#define P2AR_RG_INTR_CAL		GENMASK(29, 24)
#define P2AR_RG_INTR_CAL_MASK		(0x3f)
#define P2AR_RG_INTR_CAL_OFET		(24)

#define XSP_USBPHYA_RESERVEA	((SSUSB_SIFSLV_U2PHY_COM) + 0x034)
#define P2ARA_RG_TERM_CAL		GENMASK(11, 8)
#define P2ARA_RG_TERM_CAL_MASK          (0xf)
#define P2ARA_RG_TERM_CAL_OFET		(8)

#define XSP_USBPHYA_RESERVEA1   ((SSUSB_SIFSLV_U2PHY_COM) + 0x038)
#define P2ARA_RG_TX_CHIRPK           BIT(8)

#define XSP_U2PHYA_RESERVE0	((SSUSB_SIFSLV_U2PHY_COM) + 0x040)
#define P2A2R0_RG_PLL_FBKSEL         BIT(31)
#define P2A2R0_RG_HSRX_TERM_CAL		GENMASK(19, 16)
#define P2A2R0_RG_HSTX_IMPN		GENMASK(24, 20)
#define P2A2R0_RG_HSRX_VREF_SEL		GENMASK(6, 4)

#define XSP_U2PHYA_RESERVE1	((SSUSB_SIFSLV_U2PHY_COM) + 0x044)
#define P2A2R1_RG_PLL_POSDIV    GENMASK(2, 0)
#define P2A2R1_RG_PLL_REFCLK_SEL        BIT(5)
#define P2A2R1_RG_HSTX_IMPP		GENMASK(12, 8)

#define XSP_U2PHYDCR1		((SSUSB_SIFSLV_U2PHY_COM) + 0x064)
#define P2C_RG_USB20_SW_PLLMODE	GENMASK(19, 18)

#define XSP_U2PHYDTM0		((SSUSB_SIFSLV_U2PHY_COM) + 0x068)
#define P2D_FORCE_UART_EN		BIT(26)
#define P2D_FORCE_DATAIN		BIT(23)
#define P2D_FORCE_DM_PULLDOWN		BIT(21)
#define P2D_FORCE_DP_PULLDOWN		BIT(20)
#define P2D_FORCE_XCVRSEL		BIT(19)
#define P2D_FORCE_SUSPENDM		BIT(18)
#define P2D_FORCE_TERMSEL		BIT(17)
#define P2D_RG_DATAIN			GENMASK(13, 10)
#define P2D_RG_DMPULLDOWN		BIT(7)
#define P2D_RG_DPPULLDOWN		BIT(6)
#define P2D_RG_XCVRSEL			GENMASK(5, 4)
#define P2D_RG_XCVRSEL_VAL(x)		((0x3 & (x)) << 4)
#define P2D_RG_SUSPENDM			BIT(3)
#define P2D_RG_TERMSEL			BIT(2)
#define P2D_DTM0_PART_MASK \
		(P2D_FORCE_DATAIN | P2D_FORCE_DM_PULLDOWN | \
		P2D_FORCE_DP_PULLDOWN | P2D_FORCE_XCVRSEL | \
		P2D_FORCE_SUSPENDM | P2D_FORCE_TERMSEL | \
		P2D_RG_DMPULLDOWN | P2D_RG_DPPULLDOWN | \
		P2D_RG_TERMSEL)


#define XSP_U2PHYDTM1		((SSUSB_SIFSLV_U2PHY_COM) + 0x06C)
#define P2D_RG_UART_EN		BIT(16)
#define P2D_FORCE_IDDIG		BIT(9)
#define P2D_RG_VBUSVALID	BIT(5)
#define P2D_RG_SESSEND		BIT(4)
#define P2D_RG_AVALID		BIT(2)
#define P2D_RG_IDDIG		BIT(1)

#define SSPXTP_DIG_GLB_04		((SSPXTP_SIFSLV_DIG_GLB) + 0x04)

#define SSPXTP_DIG_GLB_28		((SSPXTP_SIFSLV_DIG_GLB) + 0x028)
#define RG_XTP_DAIF_GLB_TXPLL_IR		GENMASK(17, 13)

#define SSPXTP_DIG_GLB_38		((SSPXTP_SIFSLV_DIG_GLB) + 0x038)
#define RG_XTP_DAIF_GLB_SPLL_IR		GENMASK(17, 13)

#define SSPXTP_PHYA_GLB_00		((SSPXTP_SIFSLV_PHYA_GLB) + 0x00)
#define RG_XTP_GLB_BIAS_INTR_CTRL		GENMASK(21, 16)

#define SSPXTP_PHYA_GLB_14		((SSPXTP_SIFSLV_PHYA_GLB) + 0x14)
#define RG_XTP_GLB_BIAS_V2V_VTRIM		GENMASK(30, 27)

#define SSPXTP_DAIG_LN_TOP_04	((SSPXTP_SIFSLV_DIG_LN_TOP) + 0x04)

#define SSPXTP_DAIG_LN_TOP_10	((SSPXTP_SIFSLV_DIG_LN_TOP) + 0x010)

#define SSPXTP_DAIG_LN_TOP_24	((SSPXTP_SIFSLV_DIG_LN_TOP) + 0x024)

#define SSPXTP_DAIG_LN_TOP_80	((SSPXTP_SIFSLV_DIG_LN_TOP) + 0x080)
#define RG_XTP0_RESERVED_0			GENMASK(31, 0)

#define SSPXTP_DAIG_LN_TOP_A0	((SSPXTP_SIFSLV_DIG_LN_TOP) + 0x0a0)
#define RG_XTP0_T2RLB_ERR_CNT			GENMASK(19, 4)
#define RG_XTP0_T2RLB_ERR			BIT(3)
#define RG_XTP0_T2RLB_PASSTH			BIT(2)
#define RG_XTP0_T2RLB_PASS			BIT(1)
#define RG_XTP0_T2RLB_LOCK			BIT(0)

#define SSPXTP_DAIG_LN_RX0_40	((SSPXTP_SIFSLV_DIG_LN_RX0) + 0x040)

#define SSPXTP_DAIG_LN_RX0_48   ((SSPXTP_SIFSLV_DIG_LN_RX0) + 0x048)
#define RG_XTP0_U1_U2_EXIT_EQUAL		BIT(19)

#define SSPXTP_DAIG_LN_RX0_70	((SSPXTP_SIFSLV_DIG_LN_RX0) + 0x070)
#define RG_XTP0_CDR_PPATH_DVN_G2_LTD0		GENMASK(7, 0)
#define RG_XTP0_CDR_PPATH_DVN_G2_LTD1		GENMASK(15, 8)

#define SSPXTP_DAIG_LN_RX0_7C   ((SSPXTP_SIFSLV_DIG_LN_RX0) + 0x07C)
#define RG_XTP0_CDR_STB_GAIN_G2_LTD0		GENMASK(15, 14)

#define SSPXTP_DAIG_LN_DAIF_00	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x00)
#define RG_XTP0_DAIF_FRC_LN_TX_LCTXCM1		BIT(7)
#define RG_XTP0_DAIF_FRC_LN_TX_LCTXC0		BIT(8)
#define RG_XTP0_DAIF_FRC_LN_TX_LCTXCP1		BIT(9)

#define SSPXTP_DAIG_LN_DAIF_04	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x04)
#define RG_XTP0_DAIF_FRC_LN_RX_AEQ_ATT		BIT(17)

#define SSPXTP_DAIG_LN_DAIF_08	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x008)
#define RG_XTP0_DAIF_LN_TX_LCTXCM1		GENMASK(12, 7)
#define RG_XTP0_DAIF_LN_TX_LCTXCM1_MASK		(0x3f)
#define RG_XTP0_DAIF_LN_TX_LCTXCM1_OFST		(7)
#define RG_XTP0_DAIF_LN_TX_LCTXC0		GENMASK(18, 13)
#define RG_XTP0_DAIF_LN_TX_LCTXC0_MASK		(0x3f)
#define RG_XTP0_DAIF_LN_TX_LCTXC0_OFST		(13)
#define RG_XTP0_DAIF_LN_TX_LCTXCP1		GENMASK(24, 19)
#define RG_XTP0_DAIF_LN_TX_LCTXCP1_MASK		(0x3f)
#define RG_XTP0_DAIF_LN_TX_LCTXCP1_OFST		(19)

#define SSPXTP_DAIG_LN_DAIF_14	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x014)
#define RG_XTP0_DAIF_LN_RX_AEQ_ATT		GENMASK(20, 18)

#define SSPXTP_DAIG_LN_DAIF_20	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x020)
#define RG_XTP0_DAIF_LN_G1_RX_SGDT_HF		GENMASK(23, 22)

#define SSPXTP_DAIG_LN_DAIF_2C	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x02C)
#define RG_XTP0_DAIF_LN_G2_RX_SGDT_HF		GENMASK(23, 22)

#define SSPXTP_DAIG_LN_DAIF_34	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x034)
#define RG_SSPXTP0_DAIF_LN_G2_RX_AEQ_EGEQ_RATIO	GENMASK(30, 25)

#define SSPXTP_DAIG_LN_DAIF_38	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x038)
#define RG_SSPXTP0_DAIF_LN_G1_RX_CTLE1_CSEL	GENMASK(24, 21)

#define SSPXTP_DAIG_LN_DAIF_3C  ((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x03C)
#define RG_SSPXTP0_DAIF_LN_G2_RX_CTLE1_CSEL     GENMASK(24, 21)

#define SSPXTP_DAIG_LN_DAIF_44	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x044)
#define RG_XTP0_DAIF_LN_G2_CDR_IIR_GAIN		GENMASK(15, 13)

#define SSPXTP_PHYA_LN_04	((SSPXTP_SIFSLV_PHYA_LN) + 0x04)
#define RG_XTP_LN0_TX_IMPSEL		GENMASK(4, 0)

#define SSPXTP_PHYA_LN_08	((SSPXTP_SIFSLV_PHYA_LN) + 0x08)
#define RG_XTP_LN0_TX_RXDET_HZ		BIT(13)
#define RG_XTP_LN0_RX_CDR_RESERVE		GENMASK(23, 16)

#define SSPXTP_PHYA_LN_0C	((SSPXTP_SIFSLV_PHYA_LN) + 0x0C)
#define RG_XTP_LN0_RX_FE_RESERVE	BIT(31)

#define SSPXTP_PHYA_LN_14	((SSPXTP_SIFSLV_PHYA_LN) + 0x014)
#define RG_XTP_LN0_RX_IMPSEL		GENMASK(3, 0)

#define SSPXTP_PHYA_LN_30	((SSPXTP_SIFSLV_PHYA_LN) + 0x030)
#define RG_XTP_LN0_RX_AEQ_ATT		BIT(14)

#define SSPXTP_PHYA_LN_3C	((SSPXTP_SIFSLV_PHYA_LN) + 0x03C)
#define RG_XTP_LN0_RX_AEQ_CTLE_ERR_TYPE		GENMASK(14, 13)

#define SSPXTP_PHYA_LN_58	((SSPXTP_SIFSLV_PHYA_LN) + 0x058)
#define RX_XTP_LN0_TX_IMPSEL_PMOS		GENMASK(5, 1)
#define RX_XTP_LN0_TX_IMPSEL_NMOS               GENMASK(10, 6)

#define SSPXTP_PHYA_LN_70	((SSPXTP_SIFSLV_PHYA_LN) + 0x070)
#define RX_XTP_LN0_RX_LEQ_RL_CTLE_CAL		GENMASK(17, 13)
#define RX_XTP_LN0_RX_LEQ_RL_VGA_CAL		GENMASK(22, 18)

#define SSPXTP_PHYA_LN_74       ((SSPXTP_SIFSLV_PHYA_LN) + 0x074)
#define RX_XTP_LN0_RX_LEQ_RL_DFE_CAL		GENMASK(4, 0)

#define XSP_REF_CLK		26	/* MHZ */
#define XSP_SLEW_RATE_COEF	17
#define XSP_SR_COEF_DIVISOR	1000
#define XSP_FM_DET_CYCLE_CNT	1024

#define MTK_USB_STR "mtk_usb"
#define U2_PHY_STR "u2_phy"
#define U3_PHY_STR "u3_phy"
#define USB_JTAG_REG "usb_jtag_rg"

#define TERM_SEL_STR "term_sel"
#define VRT_SEL_STR "vrt_sel"
#define PHY_REV6_STR "phy_rev6"
#define DISCTH_STR "discth"
#define RX_SQTH_STR "rx_sqth"
#define INTR_OFS_STR "intr_ofs"
#define TERM_OFS_STR "term_ofs"
#define SIB_STR	"sib"
#define LOOPBACK_STR "loopback_test"
#define TX_LCTXCM1_STR "tx_lctxcm1"
#define TX_LCTXC0_STR "tx_lctxc0"
#define TX_LCTXCP1_STR "tx_lctxcp1"

#define XSP_MODE_UART_STR "usb2uart_mode=1"
#define XSP_MODE_JTAG_STR "usb2jtag_mode=1"

#define TCPC_NORMAL     0
#define TCPC_FLIP       1

#define XSPHY_SUB_CLASS	1
#define RPTR_SUB_CLASS	2

enum mtk_xsphy_mode {
	XSP_MODE_USB = 0,
	XSP_MODE_UART,
	XSP_MODE_JTAG,
};

enum mtk_xsphy_u2_lpm_parameter {
	PHY_PLL_TIMER_COUNT = 0,
	PHY_PLL_STABLE_TIME,
	PHY_PLL_BANDGAP_TIME,
	PHY_PLL_PARA_CNT,
};

enum mtk_xsphy_jtag_version {
	XSP_JTAG_V1 = 1,
	XSP_JTAG_V2,
};

enum mtk_phy_efuse {
	INTR_CAL = 0,
	TERM_CAL,
	IEXT_INTR_CTRL,
	RX_IMPSEL,
	TX_IMPSEL,
	V2V_VTRIM_N,
	BIAS_INTR_CTRL_N,
	TX_IMPSEL_PMOS_N,
	TX_IMPSEL_NMOS_N,
	RX_IMPSEL_N,
	RX_LEQ_RL_CTLE_CAL_N,
	RX_LEQ_RL_VGA_CAL_N,
	RX_LEQ_RL_DFE_CAL_N,
	V2V_VTRIM_F,
	BIAS_INTR_CTRL_F,
	TX_IMPSEL_PMOS_F,
	TX_IMPSEL_NMOS_F,
	RX_IMPSEL_F,
	RX_LEQ_RL_CTLE_CAL_F,
	RX_LEQ_RL_VGA_CAL_F,
	RX_LEQ_RL_DFE_CAL_F,
	HSRX_TERM_CAL,
	HSTX_IMPN,
	HSTX_IMPP,
};

static char *efuse_name[24] = {
	"intr_cal",
	"term_cal",
	"iext_intr_ctrl",
	"rx_impsel",
	"tx_impsel",
	"v2v_vtrim_n",
	"bias_intr_ctrl_n",
	"tx_impsel_pmos_n",
	"tx_impsel_nmos_n",
	"rx_impsel_n",
	"rx_leq_rl_ctle_cal_n",
	"rx_leq_rl_vga_cal_n",
	"rx_leq_rl_dfe_cal_n",
	"v2v_vtrim_f",
	"bias_intr_ctrl_f",
	"tx_impsel_pmos_f",
	"tx_impsel_nmos_f",
	"rx_impsel_f",
	"rx_leq_rl_ctle_cal_f",
	"rx_leq_rl_vga_cal_f",
	"rx_leq_rl_dfe_cal_f",
	"hsrx_term_cal",
	"hstx_impn",
	"hstx_impp",
};

struct xsphy_instance {
	struct phy *phy;
	void __iomem *port_base;
	void __iomem *ippc_base;
	struct clk *ref_clk;	/* reference clock of anolog phy */
	u32 index;
	u32 type;
	/* only for HQA test */
	bool property_ready;
	int efuse_intr;
	int efuse_term_cal;
	int efuse_tx_imp;
	int efuse_rx_imp;
	int intr_ofs;
	int term_ofs;
	int host_intr_ofs;
	int host_term_ofs;
	int pll_fbksel;
	int pll_posdiv;
	/* u2 eye diagram */
	int eye_src;
	int eye_vrt;
	int eye_term;
	int discth;
	int rx_sqth;
	int host_rx_sqth;
	int rev6;
	int hsrx_vref_sel;
	int fs_cr;
	/* u2 eye diagram for host */
	int eye_src_host;
	int eye_vrt_host;
	int eye_term_host;
	int rev6_host;
	/* u2 lpm */
	bool lpm_quirk;
	u32 lpm_para[PHY_PLL_PARA_CNT];
	/* u3 driving */
	int tx_lctxcm1;
	int tx_lctxc0;
	int tx_lctxcp1;
	bool u3_rx_fix;
	bool u3_gen2_hqa;
	/* u3 lpm */
	bool u1u2_exit_equal;
	bool u3gen2_lpm_fix;
	/* refclk source */
	bool refclk_sel;
	/* HWPLL mode setting */
	bool hwpll_mode;
	bool chp_en_disable;
	struct proc_dir_entry *phy_root;
	struct work_struct procfs_work;
	/* misc */
	int orientation;
	bool u2_sw_efuse;
	bool u3_sw_efuse;
	int u3_sw_efuse_normal[8];
	int u3_sw_efuse_flip[8];
	int hsrx_term_cal;
	int hstx_impn;
	int hstx_impp;
};

struct mtk_xsphy {
	struct device *dev;
	void __iomem *glb_base;	/* only shared u3 sif */
	struct xsphy_instance **phys;
	int nphys;
	int num_rptr;
	int src_ref_clk; /* MHZ, reference clock for slew rate calibrate */
	int src_coef;    /* coefficient for slew rate calibrate */
	bool tx_chirpK_disable; /* Disable tx chirpK at normol status */
	bool bc11_switch_disable; /* Force usb control dpdm */
	int sw_ver; /* chip id ver  */
	int u2_procfs_disable; /* disable porc node set from dts */
	struct proc_dir_entry *root;
	struct workqueue_struct *wq;
	struct phy **repeater;
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

static void u2_phy_props_set(struct mtk_xsphy *xsphy,
		struct xsphy_instance *inst);
static void u2_phy_host_props_set(struct mtk_xsphy *xsphy,
		struct xsphy_instance *inst);
static void u3_phy_props_set(struct mtk_xsphy *xsphy,
		struct xsphy_instance *inst);
static int mtk_phy_get_mode(struct mtk_xsphy *xsphy);
static struct proc_dir_entry *usb_root;

static ssize_t proc_sib_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	struct device *dev = &inst->phy->dev;
	char buf[20];
	unsigned int val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (IS_ERR_OR_NULL(inst->ippc_base))
		return -ENODEV;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	/* SSUSB_SIFSLV_IPPC_BASE SSUSB_IP_SW_RST = 0 */
	writel(0x00031000, inst->ippc_base + 0x00);
	/* SSUSB_IP_HOST_PDN = 0 */
	writel(0x00000000, inst->ippc_base + 0x04);
	/* SSUSB_IP_DEV_PDN = 0 */
	writel(0x00000000, inst->ippc_base + 0x08);
	/* SSUSB_IP_PCIE_PDN = 0 */
	writel(0x00000000, inst->ippc_base + 0x0C);
	/* SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
	writel(0x0000000C, inst->ippc_base + 0x30);

	/* SSPXTP_DAIG_LN_TOP_80[3:0]
	 * 0: No U3 owner
	 * 2: U3 owner is AP USB MAC
	 * 4: U3 owner is AP META MAC
	 * 8: U3 owner is MD STP
	 */
	if (val)
		writel(0x00000008, inst->port_base + SSPXTP_DAIG_LN_TOP_80);
	else
		writel(0x00000002, inst->port_base + SSPXTP_DAIG_LN_TOP_80);

	dev_info(dev, "%s, sib=%d reserved0=%x\n",
		__func__, val, readl(inst->port_base + SSPXTP_DAIG_LN_TOP_80));

	return count;
}

static int proc_sib_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int proc_sib_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_sib_show, pde_data(inode));
}

static const struct  proc_ops proc_sib_fops = {
	.proc_open = proc_sib_open,
	.proc_write = proc_sib_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_loopback_test_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	struct device *dev = &inst->phy->dev;
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev->parent);
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	bool pass = false;

	writel(0x00000009, pbase + SSPXTP_DAIG_LN_TOP_10);

	writel(0x001F1F01, pbase + SSPXTP_DAIG_LN_RX0_40);

	writel(0x40822803, pbase + SSPXTP_DAIG_LN_TOP_04);

	writel(0x00003007, xsphy->glb_base + SSPXTP_DIG_GLB_04);

	udelay(100);

	writel(0x0000300D, xsphy->glb_base + SSPXTP_DIG_GLB_04);

	udelay(200);

	writel(0x287F8000, pbase + SSPXTP_DAIG_LN_TOP_24);

	writel(0x40022803, pbase + SSPXTP_DAIG_LN_TOP_04);

	udelay(100);

	writel(0x001F1F00, pbase + SSPXTP_DAIG_LN_RX0_40);

	writel(0x00008009, pbase + SSPXTP_DAIG_LN_TOP_10);

	mdelay(10);

	tmp = readl(pbase + SSPXTP_DAIG_LN_TOP_A0);

	if ((tmp & RG_XTP0_T2RLB_LOCK) &&
		(tmp & RG_XTP0_T2RLB_PASS) &&
		(tmp & RG_XTP0_T2RLB_PASSTH) &&
		!(tmp & RG_XTP0_T2RLB_ERR) &&
		!(tmp & RG_XTP0_T2RLB_ERR_CNT))
		pass = true;

	dev_info(dev, "%s, t2rlb=0x%x, pass=%d\n", __func__, tmp, pass);

	seq_printf(s, "%d\n", pass);
	return 0;
}

static int proc_loopback_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_loopback_test_show, pde_data(inode));
}

static const struct  proc_ops proc_loopback_test_fops = {
	.proc_open = proc_loopback_test_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_jtag_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 cr4, cr6, cr0, cr2, mon0;

	cr4 = readl(pbase + XSP_U2PHYACR4);
	cr6 = readl(pbase + XSP_USBPHYACR6);
	cr0 = readl(pbase + XSP_USBPHYACR0);
	cr2 = readl(pbase + XSP_USBPHYACR2);
	mon0 = readl(pbase + XSP_USBPHYMON0);

	seq_printf(s, "<0x20, 0x18, 0x00, 0x08, 0x24>=<0x%x, 0x%x, 0x%x, 0x%x, 0x%x>\n",
		cr4, cr6, cr0, cr2, mon0);

	return 0;
}

static int proc_tx_jtag_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_jtag_show, pde_data(inode));
}

static const struct proc_ops proc_jtag_fops = {
	.proc_open = proc_tx_jtag_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};


static int proc_tx_lctxcm1_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_00);
	tmp &= RG_XTP0_DAIF_FRC_LN_TX_LCTXCM1;
	if (!tmp) {
		seq_puts(s, "invalid\n");
		return 0;
	}

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_08);
	tmp >>= RG_XTP0_DAIF_LN_TX_LCTXCM1_OFST;
	tmp &= RG_XTP0_DAIF_LN_TX_LCTXCM1_MASK;

	seq_printf(s, "%d\n", tmp);
	return 0;
}

static int proc_tx_lctxcm1_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_tx_lctxcm1_show, pde_data(inode));
}

static ssize_t proc_tx_lctxcm1_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	inst->tx_lctxcm1 = val;

	mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_00, RG_XTP0_DAIF_FRC_LN_TX_LCTXCM1);

	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_08, RG_XTP0_DAIF_LN_TX_LCTXCM1, val);

	return count;
}

static const struct proc_ops proc_tx_lctxcm1_fops = {
	.proc_open = proc_tx_lctxcm1_open,
	.proc_write = proc_tx_lctxcm1_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};


static int proc_tx_lctxc0_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_00);
	tmp &= RG_XTP0_DAIF_FRC_LN_TX_LCTXC0;
	if (!tmp) {
		seq_puts(s, "invalid\n");
		return 0;
	}

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_08);
	tmp >>= RG_XTP0_DAIF_LN_TX_LCTXC0_OFST;
	tmp &= RG_XTP0_DAIF_LN_TX_LCTXC0_MASK;

	seq_printf(s, "%d\n", tmp);
	return 0;
}

static int proc_tx_lctxc0_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_tx_lctxc0_show, pde_data(inode));
}

static ssize_t proc_tx_lctxc0_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	inst->tx_lctxc0 = val;

	mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_00, RG_XTP0_DAIF_FRC_LN_TX_LCTXC0);

	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_08, RG_XTP0_DAIF_LN_TX_LCTXC0, val);

	return count;
}

static const struct proc_ops proc_tx_lctxc0_fops = {
	.proc_open = proc_tx_lctxc0_open,
	.proc_write = proc_tx_lctxc0_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_tx_lctxcp1_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_00);
	tmp &= RG_XTP0_DAIF_FRC_LN_TX_LCTXCP1;
	if (!tmp) {
		seq_puts(s, "invalid\n");
		return 0;
	}

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_08);
	tmp >>= RG_XTP0_DAIF_LN_TX_LCTXCP1_OFST;
	tmp &= RG_XTP0_DAIF_LN_TX_LCTXCP1_MASK;

	seq_printf(s, "%d\n", tmp);
	return 0;
}

static int proc_tx_lctxcp1_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_tx_lctxcp1_show, pde_data(inode));
}

static ssize_t proc_tx_lctxcp1_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	inst->tx_lctxcp1 = val;

	mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_00, RG_XTP0_DAIF_FRC_LN_TX_LCTXCP1);

	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_08, RG_XTP0_DAIF_LN_TX_LCTXCP1, val);

	return count;
}

static const struct proc_ops proc_tx_lctxcp1_fops = {
	.proc_open = proc_tx_lctxcp1_open,
	.proc_write = proc_tx_lctxcp1_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int u3_phy_procfs_init(struct mtk_xsphy *xsphy,
			struct xsphy_instance *inst)
{
	struct device *dev = &inst->phy->dev;
	struct proc_dir_entry *root = xsphy->root;
	struct proc_dir_entry *phy_root;
	struct proc_dir_entry *file;
	int ret;

	if (!root) {
		dev_info(dev, "proc root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	phy_root = proc_mkdir(U3_PHY_STR, root);
	if (!root) {
		dev_info(dev, "failed to creat dir proc %s\n", U3_PHY_STR);
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(SIB_STR, 0640,
			phy_root, &proc_sib_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", SIB_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(LOOPBACK_STR, 0440,
			phy_root, &proc_loopback_test_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", LOOPBACK_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(TX_LCTXCM1_STR, 0640,
			phy_root, &proc_tx_lctxcm1_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TX_LCTXCM1_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(TX_LCTXC0_STR, 0640,
			phy_root, &proc_tx_lctxc0_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TX_LCTXC0_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(TX_LCTXCP1_STR, 0640,
			phy_root, &proc_tx_lctxcp1_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TX_LCTXCP1_STR);
		ret = -ENOMEM;
		goto err1;
	}

	inst->phy_root = phy_root;
	return 0;
err1:
	proc_remove(phy_root);

err0:
	return ret;
}

static int u3_phy_procfs_exit(struct xsphy_instance *inst)
{
	proc_remove(inst->phy_root);
	return 0;
}

static void cover_val_to_str(u32 val, u8 width, char *str)
{
	int i;

	str[width] = '\0';
	for (i = (width - 1); i >= 0; i--) {
		if (val % 2)
			str[i] = '1';
		else
			str[i] = '0';
		val /= 2;
	}
}

static int proc_term_sel_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYACR1);
	tmp >>= P2A1_RG_TERM_SEL_OFST;
	tmp &= P2A1_RG_TERM_SEL_MASK;

	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "\n%s = %s\n", TERM_SEL_STR, str);
	return 0;
}

static int proc_term_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_term_sel_show, pde_data(inode));
}

static ssize_t proc_term_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	inst->eye_term = val;
	inst->eye_term_host = val;

	mtk_phy_update_field(pbase + XSP_USBPHYACR1, P2A1_RG_TERM_SEL, val);

	return count;
}

static const struct proc_ops proc_term_sel_fops = {
	.proc_open = proc_term_sel_open,
	.proc_write = proc_term_sel_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_vrt_sel_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYACR1);
	tmp >>= P2A1_RG_VRT_SEL_OFST;
	tmp &= P2A1_RG_VRT_SEL_MASK;

	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "\n%s = %s\n", VRT_SEL_STR, str);
	return 0;
}

static int proc_vrt_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_vrt_sel_show, pde_data(inode));
}

static ssize_t proc_vrt_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	inst->eye_vrt = val;
	inst->eye_vrt_host = val;

	mtk_phy_update_field(pbase + XSP_USBPHYACR1, P2A1_RG_VRT_SEL, val);

	return count;
}

static const struct  proc_ops proc_vrt_sel_fops = {
	.proc_open = proc_vrt_sel_open,
	.proc_write = proc_vrt_sel_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_phy_rev6_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp >>= P2A6_RG_U2_PHY_REV6_OFET;
	tmp &= P2A6_RG_U2_PHY_REV6_MASK;

	cover_val_to_str(tmp, 2, str);

	seq_printf(s, "\n%s = %s\n", PHY_REV6_STR, str);
	return 0;
}

static int proc_phy_rev6_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_phy_rev6_show, pde_data(inode));
}

static ssize_t proc_phy_rev6_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	inst->rev6 = val;
	inst->rev6_host = val;

	mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_PHY_REV6, val);

	return count;
}

static const struct proc_ops proc_phy_rev6_fops = {
	.proc_open = proc_phy_rev6_open,
	.proc_write = proc_phy_rev6_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_discth_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp >>= P2A6_RG_U2_DISCTH_OFET;
	tmp &= P2A6_RG_U2_DISCTH_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "\n%s = %s\n", DISCTH_STR, str);
	return 0;
}

static int proc_discth_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_discth_show, pde_data(inode));
}

static ssize_t proc_discth_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	inst->discth = val;

	mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_DISCTH, val);

	return count;
}

static const struct proc_ops proc_discth_fops = {
	.proc_open = proc_discth_open,
	.proc_write = proc_discth_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_rx_sqth_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp >>= P2A6_RG_U2_SQTH_OFET;
	tmp &= P2A6_RG_U2_SQTH_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "\n%s = %s\n", RX_SQTH_STR, str);
	return 0;
}

static int proc_rx_sqth_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_rx_sqth_show, pde_data(inode));
}

static ssize_t proc_rx_sqth_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	inst->rx_sqth = val;
	inst->host_rx_sqth = val;

	mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_SQTH, val);

	return count;
}

static const struct proc_ops proc_rx_sqth_fops = {
	.proc_open = proc_rx_sqth_open,
	.proc_write = proc_rx_sqth_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_intr_ofs_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYA_RESERVE);
	tmp >>= P2AR_RG_INTR_CAL_OFET;
	tmp &= P2AR_RG_INTR_CAL_MASK;

	cover_val_to_str(tmp, 6, str);

	seq_printf(s, "%s = %d\n", INTR_OFS_STR,
		(inst->intr_ofs == -(P2AR_RG_INTR_CAL_MASK + 1)? 0 : inst->intr_ofs));
	seq_printf(s, "%s = %d\n", "host_intr_ofs",
		(inst->host_intr_ofs == -(P2AR_RG_INTR_CAL_MASK + 1)? 0 : inst->host_intr_ofs));
	seq_printf(s, "%s = %d\n", "efuse_intr", inst->efuse_intr);
	seq_printf(s, "%s = %s (%d)\n", "RG intr val", str, tmp);

	return 0;
}

static int proc_intr_ofs_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_intr_ofs_show, pde_data(inode));
}

static ssize_t proc_intr_ofs_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	int val, new_val;

	memset(buf, 0x00, sizeof(buf));
	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	new_val = inst->efuse_intr + val;

	if (new_val < 0 || new_val > P2AR_RG_INTR_CAL_MASK) {
		dev_err(&inst->phy->dev, "efuse_intr(%d) +/- intr_ofs (%d) out of range.\n",
			inst->efuse_intr, val);
		return -EINVAL;
	}

	mtk_phy_update_field(pbase + XSP_USBPHYA_RESERVE, P2AR_RG_INTR_CAL, new_val);

	inst->intr_ofs = val;
	inst->host_intr_ofs = val;

	return count;
}

static const struct proc_ops proc_intr_ofs_fops = {
	.proc_open = proc_intr_ofs_open,
	.proc_write = proc_intr_ofs_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_term_ofs_show(struct seq_file *s, void *unused)
{
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	u32 tmp;
	char str[16];

	tmp = readl(pbase + XSP_USBPHYA_RESERVEA);
	tmp >>= P2ARA_RG_TERM_CAL_OFET;
	tmp &= P2ARA_RG_TERM_CAL_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "%s = %d\n", TERM_OFS_STR,
		(inst->term_ofs == -(P2ARA_RG_TERM_CAL_MASK + 1)? 0 : inst->term_ofs));
	seq_printf(s, "%s = %d\n", "host_term_ofs",
		(inst->host_term_ofs == -(P2ARA_RG_TERM_CAL_MASK + 1)? 0 : inst->host_term_ofs));
	seq_printf(s, "%s = %d\n", "efuse_term_cal", inst->efuse_term_cal);
	seq_printf(s, "%s = %s (%d)\n", "RG term val", str, tmp);

	return 0;
}

static int proc_term_ofs_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_term_ofs_show, pde_data(inode));
}

static ssize_t proc_term_ofs_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct xsphy_instance *inst = s->private;
	void __iomem *pbase = inst->port_base;
	char buf[20];
	int val, new_val;

	memset(buf, 0x00, sizeof(buf));
	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	new_val = inst->efuse_term_cal + val;

	if (new_val < 0 || new_val > P2ARA_RG_TERM_CAL_MASK) {
		dev_err(&inst->phy->dev, "efuse_term_cal(%d) +/- term_ofs (%d) out of range.\n",
			inst->efuse_term_cal, val);
		return -EINVAL;
	}

	mtk_phy_update_field(pbase + XSP_USBPHYA_RESERVEA, P2ARA_RG_TERM_CAL, new_val);

	inst->term_ofs = val;
	inst->host_term_ofs = val;

	return count;
}

static const struct proc_ops proc_term_ofs_fops = {
	.proc_open = proc_term_ofs_open,
	.proc_write = proc_term_ofs_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int u2_phy_procfs_init(struct mtk_xsphy *xsphy,
			struct xsphy_instance *inst)
{
	struct device *dev = &inst->phy->dev;
	struct proc_dir_entry *root = xsphy->root;
	struct proc_dir_entry *phy_root;
	struct proc_dir_entry *file;
	int ret;

	if (!root) {
		dev_info(dev, "proc root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	phy_root = proc_mkdir(U2_PHY_STR, root);
	if (!root) {
		dev_info(dev, "failed to creat dir proc %s\n", U2_PHY_STR);
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(TERM_SEL_STR, 0640,
			phy_root, &proc_term_sel_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TERM_SEL_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(VRT_SEL_STR, 0640,
			phy_root, &proc_vrt_sel_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", VRT_SEL_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PHY_REV6_STR, 0640,
			phy_root, &proc_phy_rev6_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PHY_REV6_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(DISCTH_STR, 0640,
			phy_root, &proc_discth_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", DISCTH_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(RX_SQTH_STR, 0640,
			phy_root, &proc_rx_sqth_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", RX_SQTH_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(INTR_OFS_STR, 0640,
			phy_root, &proc_intr_ofs_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", INTR_OFS_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(TERM_OFS_STR, 0640,
			phy_root, &proc_term_ofs_fops, inst);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TERM_OFS_STR);
		ret = -ENOMEM;
		goto err1;
	}

	if (mtk_phy_get_mode(xsphy) == XSP_MODE_JTAG) {
		file = proc_create_data(USB_JTAG_REG, 0640,
			phy_root, &proc_jtag_fops, inst);
		if (!file) {
			dev_info(dev, "failed to creat proc file: %s\n", USB_JTAG_REG);
			ret = -ENOMEM;
			goto err1;
		}
	}

	inst->phy_root = phy_root;
	return 0;
err1:
	proc_remove(phy_root);

err0:
	return ret;
}

static int u2_phy_procfs_exit(struct xsphy_instance *inst)
{
	proc_remove(inst->phy_root);
	return 0;
}

static void mtk_xsphy_procfs_init_worker(struct work_struct *data)
{
	struct xsphy_instance *inst = container_of(data,
		struct xsphy_instance, procfs_work);
	struct device *dev = &inst->phy->dev;
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev->parent);

	if (!usb_root) {
		usb_root = proc_mkdir(MTK_USB_STR, NULL);
		if (!usb_root) {
			dev_info(xsphy->dev, "failed to create usb_root\n");
			return;
		}
	}

	if (xsphy->u2_procfs_disable && inst->type == PHY_TYPE_USB2)
		return;

	if (!xsphy->root) {
		xsphy->root = proc_mkdir(dev->parent->of_node->name, usb_root);
		if (!xsphy->root) {
			dev_info(xsphy->dev, "failed to create xphy root\n");
			return;
		}
	}

	if (inst->type == PHY_TYPE_USB2)
		u2_phy_procfs_init(xsphy, inst);

	if (inst->type == PHY_TYPE_USB3)
		u3_phy_procfs_init(xsphy, inst);
}

static int mtk_xsphy_procfs_exit(struct mtk_xsphy *xsphy)
{
	proc_remove(xsphy->root);
	return 0;
}

static int mtk_xsphy_get_chipid(void)
{
	struct device_node *dn = of_find_node_by_path("/chosen");
	struct tag_chipid *chipid;
	int sw_ver = 0;

	if (!dn)
		dn = of_find_node_by_path("/chosen@0");
	if (dn) {
		chipid = (struct tag_chipid *) of_get_property(dn,"atag,chipid", NULL);
		if (!chipid)
			return 0;
		sw_ver = (int)chipid->sw_ver;
	}
	return sw_ver;
}

static void u2_phy_sw_efsue_set(struct mtk_xsphy *xsphy,
				struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;

	if (!inst->u2_sw_efuse)
		return;

	if (inst->hsrx_term_cal)
		mtk_phy_update_field(pbase + XSP_U2PHYA_RESERVE0,
			P2A2R0_RG_HSRX_TERM_CAL, inst->hsrx_term_cal);

	if (inst->hstx_impn)
		mtk_phy_update_field(pbase + XSP_U2PHYA_RESERVE0,
			P2A2R0_RG_HSTX_IMPN, inst->hstx_impn);

	if (inst->hstx_impp)
		mtk_phy_update_field(pbase + XSP_U2PHYA_RESERVE1,
			P2A2R1_RG_HSTX_IMPP, inst->hstx_impp);

}

static void u3_phy_sw_efsue_set(struct mtk_xsphy *xsphy,
				struct xsphy_instance *inst)
{

	void __iomem *pbase = inst->port_base;

	if (!inst->u3_sw_efuse)
		return;

	/* RG_XTP_LN0_RX_FE_RESERVE  Bit31 = 0x1*/
	mtk_phy_set_bits(pbase + SSPXTP_PHYA_LN_0C, RG_XTP_LN0_RX_FE_RESERVE);

	/* RG_SSPXTP0_DAIF_LN_G1_RX_CTLE1_CSEL 4'b1101 */
	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_38, RG_SSPXTP0_DAIF_LN_G1_RX_CTLE1_CSEL, 0xd);

	/* RG_SSPXTP0_DAIF_LN_G2_RX_CTLE1_CSEL 4'b1101 */
	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_3C, RG_SSPXTP0_DAIF_LN_G2_RX_CTLE1_CSEL, 0xd);

	/* RG_XTP_LN0_RX_AEQ_CTLE_ERR_TYPE 2'b01 */
	mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_3C, RG_XTP_LN0_RX_AEQ_CTLE_ERR_TYPE, 0x1);

	/* RG_SSPXTP0_DAIF_LN_G2_RX_AEQ_EGEQ_RATIO 6'b010000 */
	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_34, RG_SSPXTP0_DAIF_LN_G2_RX_AEQ_EGEQ_RATIO, 0x10);

	switch (inst->orientation) {
	case TCPC_NORMAL:
		if (inst->u3_sw_efuse_normal[0])
			mtk_phy_update_field(xsphy->glb_base + SSPXTP_PHYA_GLB_14,
				RG_XTP_GLB_BIAS_V2V_VTRIM, inst->u3_sw_efuse_normal[0]);

		if (inst->u3_sw_efuse_normal[1])
			mtk_phy_update_field(xsphy->glb_base + SSPXTP_PHYA_GLB_00,
				RG_XTP_GLB_BIAS_INTR_CTRL, inst->u3_sw_efuse_normal[1]);

		if (inst->u3_sw_efuse_normal[2])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_58,
				RX_XTP_LN0_TX_IMPSEL_PMOS, inst->u3_sw_efuse_normal[2]);

		if (inst->u3_sw_efuse_normal[3])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_58,
				RX_XTP_LN0_TX_IMPSEL_NMOS, inst->u3_sw_efuse_normal[3]);

		if (inst->u3_sw_efuse_normal[4])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_14,
				RG_XTP_LN0_RX_IMPSEL, inst->u3_sw_efuse_normal[4]);

		if (inst->u3_sw_efuse_normal[5])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_70,
				RX_XTP_LN0_RX_LEQ_RL_CTLE_CAL, inst->u3_sw_efuse_normal[5]);

		if (inst->u3_sw_efuse_normal[6])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_70,
				RX_XTP_LN0_RX_LEQ_RL_VGA_CAL, inst->u3_sw_efuse_normal[6]);

		if (inst->u3_sw_efuse_normal[7])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_74,
				RX_XTP_LN0_RX_LEQ_RL_DFE_CAL, inst->u3_sw_efuse_normal[7]);
		break;
	case TCPC_FLIP:
		if (inst->u3_sw_efuse_flip[0])
			mtk_phy_update_field(xsphy->glb_base + SSPXTP_PHYA_GLB_14,
				RG_XTP_GLB_BIAS_V2V_VTRIM, inst->u3_sw_efuse_flip[0]);

		if (inst->u3_sw_efuse_flip[1])
			mtk_phy_update_field(xsphy->glb_base + SSPXTP_PHYA_GLB_00,
				RG_XTP_GLB_BIAS_INTR_CTRL, inst->u3_sw_efuse_flip[1]);

		if (inst->u3_sw_efuse_flip[2])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_58,
				RX_XTP_LN0_TX_IMPSEL_PMOS, inst->u3_sw_efuse_flip[2]);

		if (inst->u3_sw_efuse_flip[3])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_58,
				RX_XTP_LN0_TX_IMPSEL_NMOS, inst->u3_sw_efuse_flip[3]);

		if (inst->u3_sw_efuse_flip[4])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_14,
				RG_XTP_LN0_RX_IMPSEL, inst->u3_sw_efuse_flip[4]);

		if (inst->u3_sw_efuse_flip[5])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_70,
				RX_XTP_LN0_RX_LEQ_RL_CTLE_CAL, inst->u3_sw_efuse_flip[5]);

		if (inst->u3_sw_efuse_flip[6])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_70,
				RX_XTP_LN0_RX_LEQ_RL_VGA_CAL, inst->u3_sw_efuse_flip[6]);

		if (inst->u3_sw_efuse_flip[7])
			mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_74,
				RX_XTP_LN0_RX_LEQ_RL_DFE_CAL, inst->u3_sw_efuse_flip[7]);
		break;
	default:
		return;
	}

}

static void u3_phy_instance_power_on(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	struct device_node *np = xsphy->dev->of_node;

	/* clear hz mode */
	mtk_phy_clear_bits(pbase + SSPXTP_PHYA_LN_08, RG_XTP_LN0_TX_RXDET_HZ);

	/* DA_XTP_GLB_TXPLL_IR[4:0], 5'b00100 */
	mtk_phy_update_field(xsphy->glb_base + SSPXTP_DIG_GLB_28, RG_XTP_DAIF_GLB_TXPLL_IR, 0x4);

	/* DA_XTP_GLB_SPLL_IR[4:0], 5'b00100 */
	mtk_phy_update_field(xsphy->glb_base + SSPXTP_DIG_GLB_38, RG_XTP_DAIF_GLB_SPLL_IR, 0x4);

	/* DA_XTP_LN0_RX_SGDT_HF[1:0], 2'b10 */
	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_20, RG_XTP0_DAIF_LN_G1_RX_SGDT_HF, 0x2);

	mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_2C, RG_XTP0_DAIF_LN_G2_RX_SGDT_HF, 0x2);

	/* Deprecated since MT6989 */
	if (!inst->u3_rx_fix) {
		/* DA_XTP_LN0_RX_AEQ_OFORCE[10], 1'b1 */
		mtk_phy_set_bits(pbase + SSPXTP_PHYA_LN_30, RG_XTP_LN0_RX_AEQ_ATT);

		/* rg_sspxtp0_datf_ln_rx_aeq_att[2:0], 3'b111 */
		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_14, RG_XTP0_DAIF_LN_RX_AEQ_ATT, 0x7);

		/* rg_sspxtp0_datf_frc_ln_rx_aeq_att, 1'b1 */
		mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_04, RG_XTP0_DAIF_FRC_LN_RX_AEQ_ATT);
	} else
		dev_info(xsphy->dev, "%s apply u3_rx_fix.\n", __func__);

	if (inst->u3_gen2_hqa) {
		dev_info(xsphy->dev, "%s apply u3_gen2_hqa.\n", __func__);
		/* RG_XTP0_CDR_PPATH_DVN_G2_LTD1[15:8], 8'b10000100 */
		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_RX0_70, RG_XTP0_CDR_PPATH_DVN_G2_LTD1, 0x84);

		/* RG_XTP0_DAIF_LN_G2_CDR_IIR_GAIN[15:13], 3'b010 */
		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_44, RG_XTP0_DAIF_LN_G2_CDR_IIR_GAIN, 0x2);

		/* RG_XTP_LN0_RX_AEQ_CTLE_ERR_TYPE[14:13], 2'b01 */
		mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_3C, RG_XTP_LN0_RX_AEQ_CTLE_ERR_TYPE, 0x1);
	}

	/* Ponsot */
	if (of_device_is_compatible(np, "mediatek,mt6897-xsphy")) {
		dev_info(xsphy->dev, "%s set RG_XTP_LN0_RX_CDR_RESERVE\n", __func__);
		mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_08, RG_XTP_LN0_RX_CDR_RESERVE, 0x8);
	}

	if (inst->u1u2_exit_equal) {
		dev_info(xsphy->dev, "%s apply u3 lpm u1u2_exit_equal.\n", __func__);
		/* set U1 exit equal to U2 exit */
		mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_RX0_48, RG_XTP0_U1_U2_EXIT_EQUAL);
	}

	if (inst->u3gen2_lpm_fix) {
		dev_info(xsphy->dev, "%s apply u3gen2 lpm fix.\n", __func__);
		/* CDR DVN LTD0 [7:0] = 0x28 */
		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_RX0_70, RG_XTP0_CDR_PPATH_DVN_G2_LTD0, 0x28);
		/* CDR STB LTD0 [15:14] = 0x2 */
		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_RX0_7C, RG_XTP0_CDR_STB_GAIN_G2_LTD0, 0x2);
		/* CDR IIR GAIN [15:13] = 0x2*/
		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_44, RG_XTP0_DAIF_LN_G2_CDR_IIR_GAIN, 0x2);
	}

	dev_info(xsphy->dev, "%s(%d)\n", __func__, inst->index);

}

static void u3_phy_instance_power_off(struct mtk_xsphy *xsphy,
				      struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;

	/* enable hz mode */
	mtk_phy_set_bits(pbase + SSPXTP_PHYA_LN_08, RG_XTP_LN0_TX_RXDET_HZ);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, inst->index);
}

static void u2_phy_slew_rate_calibrate(struct mtk_xsphy *xsphy,
					struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	int calib_val;
	int fm_out;
	u32 tmp;

	/* use force value */
	if (inst->eye_src)
		return;

	/* enable USB ring oscillator */
	mtk_phy_set_bits(pbase + XSP_USBPHYACR5, P2A5_RG_HSTX_SRCAL_EN);
	udelay(1);	/* wait clock stable */

	/* enable free run clock */
	mtk_phy_set_bits(pbase + XSP_U2FREQ_FMMONR1, P2F_RG_FRCK_EN);

	/* set cycle count as 1024 */
	mtk_phy_update_field(pbase + XSP_U2FREQ_FMCR0, P2F_RG_CYCLECNT,
			     XSP_FM_DET_CYCLE_CNT);

	/* enable frequency meter */
	mtk_phy_set_bits(pbase + XSP_U2FREQ_FMCR0, P2F_RG_FREQDET_EN);

	/* ignore return value */
	readl_poll_timeout(pbase + XSP_U2FREQ_FMMONR1, tmp,
			   (tmp & P2F_USB_FM_VALID), 10, 200);

	fm_out = readl(pbase + XSP_U2FREQ_MMONR0);

	/* disable frequency meter */
	mtk_phy_clear_bits(pbase + XSP_U2FREQ_FMCR0, P2F_RG_FREQDET_EN);

	/* disable free run clock */
	mtk_phy_clear_bits(pbase + XSP_U2FREQ_FMMONR1, P2F_RG_FRCK_EN);

	if (fm_out) {
		/* (1024 / FM_OUT) x reference clock frequency x coefficient */
		tmp = xsphy->src_ref_clk * xsphy->src_coef;
		tmp = (tmp * XSP_FM_DET_CYCLE_CNT) / fm_out;
		calib_val = DIV_ROUND_CLOSEST(tmp, XSP_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		calib_val = 3;
	}
	dev_dbg(xsphy->dev, "phy.%d, fm_out:%d, calib:%d (clk:%d, coef:%d)\n",
		inst->index, fm_out, calib_val,
		xsphy->src_ref_clk, xsphy->src_coef);

	/* set HS slew rate */
	mtk_phy_update_field(pbase + XSP_USBPHYACR5, P2A5_RG_HSTX_SRCTRL, calib_val);

	/* disable USB ring oscillator */
	mtk_phy_clear_bits(pbase + XSP_USBPHYACR5, P2A5_RG_HSTX_SRCAL_EN);
}

static void u2_phy_lpm_pll_set(struct mtk_xsphy *xsphy,
	struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 index = inst->index;

	if (inst->hwpll_mode)
		return;

	if (!inst->lpm_quirk)
		return;

	/* enable SW PLL mode */
	mtk_phy_update_field(pbase + XSP_U2PHYDCR1, P2C_RG_USB20_SW_PLLMODE, 0x1);

	/* enable HW count PLL time */
	mtk_phy_set_bits(pbase + XSP_MISC_REG1, MR1_RG_MISC_PLL_STABLE_SEL);

	/* set 1us time count value */
	mtk_phy_update_field(pbase + XSP_MISC_REG0, MR0_RG_MISC_TIMER_1US,
			    inst->lpm_para[PHY_PLL_TIMER_COUNT]);

	/*set pll stable time*/
	mtk_phy_update_field(pbase + XSP_MISC_REG0, MR0_RG_MISC_STABLE_TIME,
			    inst->lpm_para[PHY_PLL_STABLE_TIME]);

	/* set pll bandgap stable time */
	mtk_phy_update_field(pbase + XSP_MISC_REG0, MR0_RG_MISC_BANDGAP_STABLE_TIME,
			    inst->lpm_para[PHY_PLL_BANDGAP_TIME]);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_init(struct mtk_xsphy *xsphy,
				 struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	/* read u2 intr default value from register if dts/efuse no proper config */
	if (inst->efuse_intr == -EINVAL) {
		tmp = readl(pbase + XSP_USBPHYA_RESERVE);
		tmp >>= P2AR_RG_INTR_CAL_OFET;
		inst->efuse_intr = tmp & P2AR_RG_INTR_CAL_MASK;
	}

	/* read u2 term_cal default value from register if dts/efuse no proper config */
	if (inst->efuse_term_cal == -EINVAL) {
		tmp = readl(pbase + XSP_USBPHYA_RESERVEA);
		tmp >>= P2ARA_RG_TERM_CAL_OFET;
		inst->efuse_term_cal = tmp & P2ARA_RG_TERM_CAL_MASK;
	}

	/* DP/DM BC1.1 path Disable */
	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6, P2A6_RG_BC11_SW_EN);

	mtk_phy_set_bits(pbase + XSP_USBPHYACR0, P2A0_RG_INTR_EN);
}

static void u2_phy_instance_power_on(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 index = inst->index;

	if (inst->refclk_sel) {
		mtk_phy_set_bits(pbase + XSP_U2PHYA_RESERVE1,
					P2A2R1_RG_PLL_REFCLK_SEL);
		udelay(250);
	}

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, P2D_FORCE_SUSPENDM);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_RG_SUSPENDM);

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, P2D_RG_SUSPENDM);

	udelay(30);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_FORCE_SUSPENDM);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_RG_SUSPENDM);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_FORCE_UART_EN);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM1, P2D_RG_UART_EN);

	mtk_phy_clear_bits(pbase + XSP_U2PHYACR4, P2A4_U2_GPIO_CTR_MSK);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_FORCE_SUSPENDM);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0,
			   (P2D_RG_XCVRSEL | P2D_RG_DATAIN | P2D_DTM0_PART_MASK));

	if (inst->hwpll_mode)
		mtk_phy_clear_bits(pbase + XSP_U2PHYDCR1, P2C_RG_USB20_SW_PLLMODE);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6, P2A6_RG_BC11_SW_EN);

	mtk_phy_set_bits(pbase + XSP_USBPHYACR6, P2A6_RG_OTG_VBUSCMP_EN);

	mtk_phy_update_bits(pbase + XSP_U2PHYDTM1,
			    P2D_RG_VBUSVALID | P2D_RG_AVALID | P2D_RG_SESSEND,
			    P2D_RG_VBUSVALID | P2D_RG_AVALID);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR0, P2A0_RG_USB20_TX_PH_ROT_SEL);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6,
			   (P2A6_RG_U2_PHY_REV6 | P2A6_RG_U2_PHY_REV1));
	mtk_phy_set_bits(pbase + XSP_USBPHYACR6, P2A6_RG_U2_PHY_REV6_VAL(1));

	udelay(800);

	u2_phy_lpm_pll_set(xsphy, inst);

	if (inst->chp_en_disable) {
		if (!xsphy->sw_ver)
			mtk_phy_clear_bits(pbase + XSP_USBPHYACR0, P2A0_RG_USB20_CHP_EN);
	}

	dev_info(xsphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_off(struct mtk_xsphy *xsphy,
				      struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 index = inst->index;
	enum phy_mode mode = inst->phy->attrs.mode;

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_FORCE_UART_EN);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM1, P2D_RG_UART_EN);

	mtk_phy_clear_bits(pbase + XSP_U2PHYACR4, P2A4_U2_GPIO_CTR_MSK);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6, P2A6_RG_BC11_SW_EN);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6, P2A6_RG_OTG_VBUSCMP_EN);

	mtk_phy_update_bits(pbase + XSP_U2PHYDTM1,
			    P2D_RG_VBUSVALID | P2D_RG_AVALID | P2D_RG_SESSEND,
			    P2D_RG_SESSEND);

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, (P2D_RG_SUSPENDM | P2D_FORCE_SUSPENDM));

	mdelay(2);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_RG_DATAIN);
	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, (P2D_RG_XCVRSEL_VAL(1) | P2D_DTM0_PART_MASK));

	mtk_phy_set_bits(pbase + XSP_USBPHYACR6, P2A6_RG_U2_PHY_REV6_VAL(1));

	if (mode == PHY_MODE_INVALID)
		mtk_phy_set_bits(pbase + XSP_USBPHYACR6, P2A6_RG_U2_PHY_REV1);

	udelay(800);

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, P2D_RG_SUSPENDM);

	udelay(1);

	/* set BC11_SW_EN to enter L2 power off mode */
	if (mode == PHY_MODE_INVALID)
		mtk_phy_set_bits(inst->port_base + XSP_USBPHYACR6, P2A6_RG_BC11_SW_EN);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_set_mode(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst,
				     enum phy_mode mode,
				     int submode)
{
	u32 tmp;
	int i = 0;

	dev_info(xsphy->dev, "%s mode(%d), submode(%d)\n", __func__,
		mode, submode);

	if (!submode) {
		tmp = readl(inst->port_base + XSP_U2PHYDTM1);
		switch (mode) {
		case PHY_MODE_USB_DEVICE:
			u2_phy_props_set(xsphy, inst);
			tmp |= P2D_FORCE_IDDIG | P2D_RG_IDDIG;
			break;
		case PHY_MODE_USB_HOST:
			u2_phy_host_props_set(xsphy, inst);
			tmp |= P2D_FORCE_IDDIG;
			tmp &= ~P2D_RG_IDDIG;
			break;
		case PHY_MODE_USB_OTG:
			tmp &= ~(P2D_FORCE_IDDIG | P2D_RG_IDDIG);
			break;
		default:
			return;
		}
		writel(tmp, inst->port_base + XSP_U2PHYDTM1);
	} else {
		switch (submode) {
		case PHY_MODE_BC11_SW_SET:
			if (xsphy->bc11_switch_disable)
				break;
			mtk_phy_set_bits(inst->port_base + XSP_USBPHYACR6,
					 P2A6_RG_BC11_SW_EN);
			break;
		case PHY_MODE_BC11_SW_CLR:
			if (xsphy->bc11_switch_disable)
				break;
			/* dont' need to switch back to usb when phy off */
			if (inst->phy->power_count > 0)
				mtk_phy_clear_bits(inst->port_base + XSP_USBPHYACR6,
						   P2A6_RG_BC11_SW_EN);
			break;
		case PHY_MODE_DPDMPULLDOWN_SET:
			mtk_phy_set_bits(inst->port_base + XSP_U2PHYDTM0,
					 (P2D_RG_DPPULLDOWN | P2D_RG_DMPULLDOWN));

			mtk_phy_clear_bits(inst->port_base + XSP_USBPHYACR6,
					   P2A6_RG_U2_PHY_REV1);

			mtk_phy_set_bits(inst->port_base + XSP_USBPHYACR6,
					 P2A6_RG_BC11_SW_EN);
			break;
		case PHY_MODE_DPDMPULLDOWN_CLR:
			mtk_phy_clear_bits(inst->port_base + XSP_U2PHYDTM0,
					   (P2D_RG_DPPULLDOWN | P2D_RG_DMPULLDOWN));

			mtk_phy_set_bits(inst->port_base + XSP_USBPHYACR6,
					 P2A6_RG_U2_PHY_REV1);

			mtk_phy_clear_bits(inst->port_base + XSP_USBPHYACR6,
					   P2A6_RG_BC11_SW_EN);
			break;
		case PHY_MODE_DPPULLUP_SET:
			/* eUSB2 rptr to pullup DP */
			for (i = 0; i < xsphy->num_rptr; i++) {
				if (!IS_ERR_OR_NULL(xsphy->repeater[i]))
					phy_set_mode_ext(xsphy->repeater[i],
						PHY_MODE_USB_DEVICE, PHY_MODE_DPPULLUP_SET);
			}
			mtk_phy_set_bits(inst->port_base + XSP_USBPHYACR3,
					(P2A3_RG_USB20_PUPD_BIST_EN |
					P2A3_RG_USB20_EN_PU_DP));
			break;
		case PHY_MODE_DPPULLUP_CLR:
			/* eUSB2 rptr to pulldown DP */
			for (i = 0; i < xsphy->num_rptr; i++) {
				if (!IS_ERR_OR_NULL(xsphy->repeater[i]))
					phy_set_mode_ext(xsphy->repeater[i],
						PHY_MODE_USB_DEVICE, PHY_MODE_DPPULLUP_CLR);
			}
			mtk_phy_clear_bits(inst->port_base + XSP_USBPHYACR3,
					(P2A3_RG_USB20_PUPD_BIST_EN |
					P2A3_RG_USB20_EN_PU_DP));
			break;
		default:
			return;
		}
	}
}

static void u3_phy_instance_set_mode(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst,
				     enum phy_mode mode,
				     int submode)
{
	dev_info(xsphy->dev, "%s mode(%d), submode(%d)\n", __func__,
		mode, submode);

	if (!submode) {
		switch (mode) {
		case PHY_MODE_USB_DEVICE:
			break;
		case PHY_MODE_USB_HOST:
			break;
		case PHY_MODE_USB_OTG:
			break;
		default:
			return;
		}
	} else {
		switch (submode) {
		case PHY_MODE_NORMAL:
			inst->orientation = TCPC_NORMAL;
			break;
		case PHY_MODE_FLIP:
			inst->orientation = TCPC_FLIP;
			break;
		default:
			return;
		}
	}
}

static u32 phy_get_efuse_value(struct xsphy_instance *inst,
			     enum mtk_phy_efuse type)
{
	struct device *dev = &inst->phy->dev;
	struct device_node *np = dev->of_node;
	u32 val, mask;
	int index = 0, ret = 0;

	index = of_property_match_string(np,
			"nvmem-cell-names", efuse_name[type]);
	if (index < 0)
		goto no_efuse;

	ret = of_property_read_u32_index(np, "nvmem-cell-masks",
			index, &mask);
	if (ret)
		goto no_efuse;

	ret = nvmem_cell_read_u32(dev, efuse_name[type], &val);
	if (ret)
		goto no_efuse;

	if (!val || !mask)
		goto no_efuse;

	val = (val & mask) >> (ffs(mask) - 1);
	dev_dbg(dev, "%s, %s=0x%x\n", __func__, efuse_name[type], val);

	return val;

no_efuse:
	return 0;
}

static void phy_parse_efuse_property(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	u32 val = 0;

	switch (inst->type) {
	case PHY_TYPE_USB2:
		val = phy_get_efuse_value(inst, INTR_CAL);
		if (val)
			inst->efuse_intr = val;

		val = phy_get_efuse_value(inst, TERM_CAL);
		if (val)
			inst->efuse_term_cal = val;

		/* u2 sw efuse */
		if (!inst->u2_sw_efuse)
			break;

		val = phy_get_efuse_value(inst, HSRX_TERM_CAL);
		if (val)
			inst->hsrx_term_cal = val;

		val = phy_get_efuse_value(inst, HSTX_IMPN);
		if (val)
			inst->hstx_impn = val;

		val = phy_get_efuse_value(inst, HSTX_IMPP);
		if (val)
			inst->hstx_impp = val;

		break;
	case PHY_TYPE_USB3:
		val = phy_get_efuse_value(inst, IEXT_INTR_CTRL);
		if (val)
			inst->efuse_intr = val;

		val = phy_get_efuse_value(inst, RX_IMPSEL);
		if (val)
			inst->efuse_rx_imp = val;

		val = phy_get_efuse_value(inst, TX_IMPSEL);
		if (val)
			inst->efuse_tx_imp = val;

		/* u3 sw efuse side: normal */
		if (!inst->u3_sw_efuse)
			break;

		val = phy_get_efuse_value(inst, V2V_VTRIM_N);
		if (val)
			inst->u3_sw_efuse_normal[0] = val;

		val = phy_get_efuse_value(inst, BIAS_INTR_CTRL_N);
		if (val)
			inst->u3_sw_efuse_normal[1] = val;

		val = phy_get_efuse_value(inst, TX_IMPSEL_PMOS_N);
		if (val)
			inst->u3_sw_efuse_normal[2] = val;

		val = phy_get_efuse_value(inst, TX_IMPSEL_NMOS_N);
		if (val)
			inst->u3_sw_efuse_normal[3] = val;

		val = phy_get_efuse_value(inst, RX_IMPSEL_N);
		if (val)
			inst->u3_sw_efuse_normal[4] = val;

		val = phy_get_efuse_value(inst, RX_LEQ_RL_CTLE_CAL_N);
		if (val)
			inst->u3_sw_efuse_normal[5] = val;

		val = phy_get_efuse_value(inst, RX_LEQ_RL_VGA_CAL_N);
		if (val)
			inst->u3_sw_efuse_normal[6] = val;

		val = phy_get_efuse_value(inst, RX_LEQ_RL_DFE_CAL_N);
		if (val)
			inst->u3_sw_efuse_normal[7] = val;

		/* u3 sw efuse side: flip */
		val = phy_get_efuse_value(inst, V2V_VTRIM_F);
		if (val)
			inst->u3_sw_efuse_flip[0] = val;

		val = phy_get_efuse_value(inst, BIAS_INTR_CTRL_F);
		if (val)
			inst->u3_sw_efuse_flip[1] = val;

		val = phy_get_efuse_value(inst, TX_IMPSEL_PMOS_F);
		if (val)
			inst->u3_sw_efuse_flip[2] = val;

		val = phy_get_efuse_value(inst, TX_IMPSEL_NMOS_F);
		if (val)
			inst->u3_sw_efuse_flip[3] = val;

		val = phy_get_efuse_value(inst, RX_IMPSEL_F);
		if (val)
			inst->u3_sw_efuse_flip[4] = val;

		val = phy_get_efuse_value(inst, RX_LEQ_RL_CTLE_CAL_F);
		if (val)
			inst->u3_sw_efuse_flip[5] = val;

		val = phy_get_efuse_value(inst, RX_LEQ_RL_VGA_CAL_F);
		if (val)
			inst->u3_sw_efuse_flip[6] = val;

		val = phy_get_efuse_value(inst, RX_LEQ_RL_DFE_CAL_F);
		if (val)
			inst->u3_sw_efuse_flip[7] = val;

		break;
	default:
		return;
	}
}

static void phy_parse_property(struct mtk_xsphy *xsphy,
				struct xsphy_instance *inst)
{
	struct device *dev = &inst->phy->dev;
	const char *ofs_str;

	switch (inst->type) {
	case PHY_TYPE_USB2:
		if (device_property_read_u32(dev, "mediatek,efuse-intr",
					 &inst->efuse_intr) || inst->efuse_intr < 0)
			inst->efuse_intr = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,efuse-term",
					 &inst->efuse_term_cal) || inst->efuse_term_cal < 0)
			inst->efuse_term_cal = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,eye-src",
					 &inst->eye_src) || inst->eye_src < 0)
			inst->eye_src =	-EINVAL;
		if (device_property_read_u32(dev, "mediatek,eye-vrt",
					 &inst->eye_vrt) || inst->eye_vrt < 0)
			inst->eye_vrt = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,eye-term",
					 &inst->eye_term) || inst->eye_term < 0)
			inst->eye_term = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,discth",
					 &inst->discth) || inst->discth < 0)
			inst->discth = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,rx-sqth",
					 &inst->rx_sqth) || inst->rx_sqth < 0)
			inst->rx_sqth = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,host-rx-sqth",
					 &inst->host_rx_sqth) || inst->host_rx_sqth < 0)
			inst->host_rx_sqth = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,rev6",
					 &inst->rev6) || inst->rev6 < 0)
			inst->rev6 = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,hsrx-vref-sel",
					&inst->hsrx_vref_sel) || inst->hsrx_vref_sel < 0)
			inst->hsrx_vref_sel = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,fs-cr",
					&inst->fs_cr) || inst->fs_cr < 0)
			inst->fs_cr = -EINVAL;
		if (device_property_read_string(dev, "mediatek,intr-ofs",
					 &ofs_str) || kstrtoint(ofs_str, 10, &inst->intr_ofs) < 0)
			inst->intr_ofs = -(P2AR_RG_INTR_CAL_MASK + 1);
		if (device_property_read_string(dev, "mediatek,host-intr-ofs",
					 &ofs_str) || kstrtoint(ofs_str, 10, &inst->host_intr_ofs) < 0)
			inst->host_intr_ofs = -(P2AR_RG_INTR_CAL_MASK + 1);
		if (device_property_read_string(dev, "mediatek,term-ofs",
					 &ofs_str) || kstrtoint(ofs_str, 10, &inst->term_ofs) < 0)
			inst->term_ofs = -(P2ARA_RG_TERM_CAL_MASK + 1);
		if (device_property_read_string(dev, "mediatek,host-term-ofs",
					 &ofs_str) || kstrtoint(ofs_str, 10, &inst->host_term_ofs) < 0)
			inst->host_term_ofs = -(P2ARA_RG_TERM_CAL_MASK + 1);
		if (device_property_read_u32(dev, "mediatek,pll-fbksel",
				 &inst->pll_fbksel) || inst->pll_fbksel < 0)
			inst->pll_fbksel = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,pll-posdiv",
				 &inst->pll_posdiv) || inst->pll_posdiv < 0)
			inst->pll_posdiv = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,eye-src-host",
					 &inst->eye_src_host) || inst->eye_src_host < 0)
			inst->eye_src_host = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,eye-vrt-host",
					 &inst->eye_vrt_host) || inst->eye_vrt_host < 0)
			inst->eye_vrt_host = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,eye-term-host",
					 &inst->eye_term_host) || inst->eye_term_host < 0)
			inst->eye_term_host = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,rev6-host",
					&inst->rev6_host) || inst->rev6_host < 0)
			inst->rev6_host = -EINVAL;
		if (!device_property_read_u32_array(dev, "mediatek,lpm-parameter",
			inst->lpm_para, PHY_PLL_PARA_CNT))
			inst->lpm_quirk = true;
		inst->hwpll_mode = device_property_read_bool(dev, "mediatek,hwpll-mode");
		inst->refclk_sel = device_property_read_bool(dev, "mediatek,refclk-sel");
		inst->chp_en_disable = device_property_read_bool(dev, "mediatek,chp-en-disable");
		inst->u2_sw_efuse = device_property_read_bool(dev, "mediatek,u2-sw-efuse");

		dev_dbg(dev, "intr:%d, intr_ofs:%d, host_intr_ofs:%d\n",
			inst->efuse_intr, inst->intr_ofs, inst->host_intr_ofs);
		dev_dbg(dev, "term_cal:%d, term_ofs:%d, host_term_ofs:%d\n",
			inst->efuse_term_cal, inst->term_ofs, inst->host_term_ofs);
		dev_dbg(dev, "src:%d, vrt:%d, term:%d, hsrx_vref_sel:%d\n",
			inst->eye_src, inst->eye_vrt, inst->eye_term, inst->hsrx_vref_sel);
		dev_dbg(dev, "fs_cr:%d\n",
			inst->fs_cr);
		dev_dbg(dev, "src_host:%d, vrt_host:%d, term_host:%d\n",
			inst->eye_src_host, inst->eye_vrt_host,
			inst->eye_term_host);
		dev_dbg(dev, "discth:%d, rx_sqth:%d, host_rx_sqth:%d, rev6:%d, rev6_host:%d\n",
			inst->discth, inst->rx_sqth, inst->host_rx_sqth, inst->rev6,
			inst->rev6_host);
		dev_dbg(dev, "u2-sw-efuse:%d hwpll-mode:%d, refclk-sel:%d, chp-en-disable:%d",
				inst->u2_sw_efuse, inst->hwpll_mode, inst->refclk_sel, inst->chp_en_disable);
		break;
	case PHY_TYPE_USB3:
		if (device_property_read_u32(dev, "mediatek,efuse-intr",
					 &inst->efuse_intr) || inst->efuse_intr < 0)
			inst->efuse_intr = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,efuse-tx-imp",
					 &inst->efuse_tx_imp) || inst->efuse_tx_imp < 0)
			inst->efuse_tx_imp = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,efuse-rx-imp",
					 &inst->efuse_rx_imp) || inst->efuse_rx_imp < 0)
			inst->efuse_rx_imp = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,tx-lctxcm1",
					 &inst->tx_lctxcm1) || inst->tx_lctxcm1 < 0)
			inst->tx_lctxcm1 = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,tx-lctxc0",
					 &inst->tx_lctxc0) || inst->tx_lctxc0 < 0)
			inst->tx_lctxc0 = -EINVAL;
		if (device_property_read_u32(dev, "mediatek,tx-lctxcp1",
					 &inst->tx_lctxcp1) || inst->tx_lctxcp1 < 0)
			inst->tx_lctxcp1 = -EINVAL;
		inst->u3_rx_fix = device_property_read_bool(dev, "mediatek,u3-rx-fix");
		inst->u3_gen2_hqa = device_property_read_bool(dev, "mediatek,u3-gen2-hqa");
		inst->u3_sw_efuse = device_property_read_bool(dev, "mediatek,u3-sw-efuse");
		inst->u1u2_exit_equal = device_property_read_bool(dev, "mediatek,u1u2-exit-equal");
		inst->u3gen2_lpm_fix = device_property_read_bool(dev, "mediatek,u3gen2-lpm-fix");

		dev_dbg(dev, "u3-sw-efuse: %d, intr:%d, tx-imp:%d, rx-imp:%d, u3_rx_fix:%d, u3_gen2_hqa:%d\n",
			inst->u3_sw_efuse, inst->efuse_intr, inst->efuse_tx_imp,
			inst->efuse_rx_imp, inst->u3_rx_fix, inst->u3_gen2_hqa);

		dev_dbg(dev, "u1u2_exit_equal: %d, u3gen2_lpm_fix: %d", inst->u1u2_exit_equal,
			inst->u3gen2_lpm_fix);

		inst->orientation = 0;

		break;
	default:
		dev_err(xsphy->dev, "incompatible phy type\n");
		return;
	}
}

static void u2_phy_props_set(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;

	if (inst->efuse_intr != -EINVAL) {
		int intr_val = inst->efuse_intr + inst->intr_ofs;

		if (inst->intr_ofs < -P2AR_RG_INTR_CAL_MASK ||
			inst->intr_ofs > P2AR_RG_INTR_CAL_MASK ||
			intr_val < 0 || intr_val > P2AR_RG_INTR_CAL_MASK )
			intr_val = inst->efuse_intr;

		mtk_phy_update_field(pbase + XSP_USBPHYA_RESERVE, P2AR_RG_INTR_CAL, intr_val);
	}

	if (inst->efuse_term_cal != -EINVAL) {
		int term_val = inst->efuse_term_cal + inst->term_ofs;

		if (inst->term_ofs < -P2ARA_RG_TERM_CAL_MASK ||
			inst->term_ofs > P2ARA_RG_TERM_CAL_MASK ||
			term_val < 0 || term_val > P2ARA_RG_TERM_CAL_MASK)
			term_val = inst->efuse_term_cal;

		mtk_phy_update_field(pbase + XSP_USBPHYA_RESERVEA, P2ARA_RG_TERM_CAL, term_val);
	}

	if (inst->eye_src != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR5, P2A5_RG_HSTX_SRCTRL,
				     inst->eye_src);

	if (inst->eye_vrt != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR1, P2A1_RG_VRT_SEL,
				     inst->eye_vrt);

	if (inst->eye_term != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR1, P2A1_RG_TERM_SEL,
				     inst->eye_term);

	if (inst->discth != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_DISCTH,
				    inst->discth);

	if (inst->rx_sqth != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_SQTH,
				    inst->rx_sqth);

	if (inst->rev6 != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_PHY_REV6,
				     inst->rev6);

	if (inst->pll_fbksel != -EINVAL)
		mtk_phy_update_field(pbase + XSP_U2PHYA_RESERVE0, P2A2R0_RG_PLL_FBKSEL,
				     inst->pll_fbksel);

	if (inst->pll_posdiv != -EINVAL)
		mtk_phy_update_field(pbase + XSP_U2PHYA_RESERVE1, P2A2R1_RG_PLL_POSDIV,
				     inst->pll_posdiv);

	if (inst->hsrx_vref_sel != -EINVAL) {
		if (!xsphy->sw_ver)
			mtk_phy_update_field(pbase + XSP_U2PHYA_RESERVE0, P2A2R0_RG_HSRX_VREF_SEL,
				     inst->hsrx_vref_sel);
	}

	if (inst->fs_cr != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR4, P2A4_RG_USB20_FS_CR,
				     inst->fs_cr);

}

static void u2_phy_host_props_set(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;

	if (inst->efuse_intr != -EINVAL) {
		int host_intr_val = inst->efuse_intr + inst->host_intr_ofs;

		if (inst->host_intr_ofs < -P2AR_RG_INTR_CAL_MASK ||
			inst->host_intr_ofs > P2AR_RG_INTR_CAL_MASK ||
			host_intr_val < 0 || host_intr_val > P2AR_RG_INTR_CAL_MASK )
			host_intr_val = inst->efuse_intr;

		mtk_phy_update_field(pbase + XSP_USBPHYA_RESERVE, P2AR_RG_INTR_CAL, host_intr_val);
	}

	if (inst->efuse_term_cal != -EINVAL) {
		int host_term_val = inst->efuse_term_cal + inst->host_term_ofs;

		if (inst->host_term_ofs < -P2ARA_RG_TERM_CAL_MASK ||
			inst->host_term_ofs > P2ARA_RG_TERM_CAL_MASK ||
			host_term_val < 0 || host_term_val > P2ARA_RG_TERM_CAL_MASK)
			host_term_val = inst->efuse_term_cal;

		mtk_phy_update_field(pbase + XSP_USBPHYA_RESERVEA, P2ARA_RG_TERM_CAL, host_term_val);
	}

	if (inst->host_rx_sqth != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_SQTH,
				    inst->host_rx_sqth);

	if (inst->eye_src_host != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR5, P2A5_RG_HSTX_SRCTRL,
				     inst->eye_src_host);

	if (inst->eye_vrt_host != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR1, P2A1_RG_VRT_SEL,
				     inst->eye_vrt_host);

	if (inst->eye_term_host != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR1, P2A1_RG_TERM_SEL,
				     inst->eye_term_host);

	if (inst->rev6_host != -EINVAL)
		mtk_phy_update_field(pbase + XSP_USBPHYACR6, P2A6_RG_U2_PHY_REV6,
				    inst->rev6_host);

}

static void u3_phy_props_set(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;

	if (inst->efuse_intr != -EINVAL)
		mtk_phy_update_field(xsphy->glb_base + SSPXTP_PHYA_GLB_00,
				     RG_XTP_GLB_BIAS_INTR_CTRL, inst->efuse_intr);

	if (inst->efuse_tx_imp != -EINVAL)
		mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_04,
				     RG_XTP_LN0_TX_IMPSEL, inst->efuse_tx_imp);

	if (inst->efuse_rx_imp != -EINVAL)
		mtk_phy_update_field(pbase + SSPXTP_PHYA_LN_14,
				     RG_XTP_LN0_RX_IMPSEL, inst->efuse_rx_imp);

	if (inst->tx_lctxcm1 != -EINVAL) {
		mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_00,
				 RG_XTP0_DAIF_FRC_LN_TX_LCTXCM1);

		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_08,
				    RG_XTP0_DAIF_LN_TX_LCTXCM1, inst->tx_lctxcm1);
	}

	if (inst->tx_lctxc0 != -EINVAL) {
		mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_00,
				 RG_XTP0_DAIF_FRC_LN_TX_LCTXC0);

		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_08,
				    RG_XTP0_DAIF_LN_TX_LCTXC0, inst->tx_lctxc0);
	}

	if (inst->tx_lctxcp1 != -EINVAL) {
		mtk_phy_set_bits(pbase + SSPXTP_DAIG_LN_DAIF_00,
				 RG_XTP0_DAIF_FRC_LN_TX_LCTXCP1);

		mtk_phy_update_field(pbase + SSPXTP_DAIG_LN_DAIF_08,
				    RG_XTP0_DAIF_LN_TX_LCTXCP1, inst->tx_lctxcp1);
	}
}

static int mtk_phy_init(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	int ret, i;

	ret = clk_prepare_enable(inst->ref_clk);
	if (ret) {
		dev_err(xsphy->dev, "failed to enable ref_clk\n");
		return ret;
	}

	for (i = 0; i < xsphy->num_rptr; i++) {
		if (!IS_ERR_OR_NULL(xsphy->repeater[i])) {
			lockdep_set_subclass(&xsphy->repeater[i]->mutex, RPTR_SUB_CLASS);
			ret = phy_init(xsphy->repeater[i]);
			/* Might not have all repeater mounted. Only print message if error */
			if (ret)
				dev_info(xsphy->dev, "failed to init repeater %d\n", i);
		}
	}

	switch (inst->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(xsphy, inst);
		u2_phy_props_set(xsphy, inst);
		/* show default u2 driving setting */
		dev_info(xsphy->dev, "device src:%d vrt:%d term:%d rev6:%d\n",
			inst->eye_src, inst->eye_vrt,
			inst->eye_term, inst->rev6);
		dev_info(xsphy->dev, "host src:%d vrt:%d term:%d rev6:%d\n",
			inst->eye_src_host, inst->eye_vrt_host,
			inst->eye_term_host, inst->rev6_host);
		dev_info(xsphy->dev, "u2_intr:%d intr_ofs:%d host_intr_ofs:%d discth:%d\n",
			inst->efuse_intr, inst->intr_ofs, inst->host_intr_ofs, inst->discth);
		dev_info(xsphy->dev, "term_cal:%d term_ofs:%d host_term_ofs:%d\n",
			inst->efuse_term_cal, inst->term_ofs, inst->host_term_ofs);
		dev_info(xsphy->dev, "rx_sqth:%d host_rx_sqth:%d\n", inst->rx_sqth, inst->host_rx_sqth);
		dev_info(xsphy->dev, "pll_fbksel:%d, pll_posdiv: %d\n",
			inst->pll_fbksel, inst->pll_posdiv);
		break;
	case PHY_TYPE_USB3:
		u3_phy_props_set(xsphy, inst);
		/* show default u3 driving setting */
		dev_info(xsphy->dev, "u3_intr:%d, tx-imp:%d, rx-imp:%d\n",
			inst->efuse_intr, inst->efuse_tx_imp,
			inst->efuse_rx_imp);
		dev_info(xsphy->dev,
			"tx-lctxcm1:%d, tx-lctxc0:%d, tx-lctxcp1:%d\n",
			inst->tx_lctxcm1, inst->tx_lctxc0, inst->tx_lctxcp1);
		break;
	default:
		dev_err(xsphy->dev, "incompatible phy type\n");
		clk_disable_unprepare(inst->ref_clk);
		return -EINVAL;
	}

	queue_work(xsphy->wq, &inst->procfs_work);

	return 0;
}

static int mtk_phy_power_on(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	void __iomem *pbase = inst->port_base;
	enum phy_mode mode = inst->phy->attrs.mode;
	int i;

	for (i = 0; i < xsphy->num_rptr; i++) {
		if (!IS_ERR_OR_NULL(xsphy->repeater[i]))
			phy_power_on(xsphy->repeater[i]);
	}

	if (inst->type == PHY_TYPE_USB2) {
		u2_phy_sw_efsue_set(xsphy, inst);
		u2_phy_instance_power_on(xsphy, inst);
		u2_phy_slew_rate_calibrate(xsphy, inst);
		if (mode == PHY_MODE_USB_HOST)
			u2_phy_host_props_set(xsphy, inst);
		else
			u2_phy_props_set(xsphy, inst);
	} else if (inst->type == PHY_TYPE_USB3) {
		u3_phy_sw_efsue_set(xsphy, inst);
		u3_phy_instance_power_on(xsphy, inst);
		u3_phy_props_set(xsphy, inst);
	}

	if (xsphy->tx_chirpK_disable) {
		dev_info(xsphy->dev, "Disable tx_chirpK.\n");
		mtk_phy_set_bits(pbase + XSP_USBPHYA_RESERVEA1, P2ARA_RG_TX_CHIRPK);
	}

	return 0;
}

static int mtk_phy_power_off(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	void __iomem *pbase = inst->port_base;
	int i;

	for (i = 0; i < xsphy->num_rptr; i++) {
		if (!IS_ERR_OR_NULL(xsphy->repeater[i])) {
			phy_power_off(xsphy->repeater[i]);
		}
	}

	if (xsphy->tx_chirpK_disable) {
		dev_info(xsphy->dev, "Enable tx_chirpK.\n");
		mtk_phy_clear_bits(pbase + XSP_USBPHYA_RESERVEA1, P2ARA_RG_TX_CHIRPK);
	}

	if (inst->type == PHY_TYPE_USB2)
		u2_phy_instance_power_off(xsphy, inst);
	else if (inst->type == PHY_TYPE_USB3)
		u3_phy_instance_power_off(xsphy, inst);

	return 0;
}

static int mtk_phy_exit(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	int i;

	if (inst->type == PHY_TYPE_USB2 && !xsphy->u2_procfs_disable)
		u2_phy_procfs_exit(inst);
	else if (inst->type == PHY_TYPE_USB3)
		u3_phy_procfs_exit(inst);

	for (i = 0; i < xsphy->num_rptr; i++) {
		if (!IS_ERR_OR_NULL(xsphy->repeater[i]))
			phy_exit(xsphy->repeater[i]);
	}

	clk_disable_unprepare(inst->ref_clk);
	return 0;
}

static int mtk_phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	int i;

	if (inst->type == PHY_TYPE_USB2) {
		for (i = 0; i < xsphy->num_rptr; i++) {
			if (!IS_ERR_OR_NULL(xsphy->repeater[i]))
				phy_set_mode_ext(xsphy->repeater[i], mode, submode);
		}
		u2_phy_instance_set_mode(xsphy, inst, mode, submode);
	} else if (inst->type == PHY_TYPE_USB3)
		u3_phy_instance_set_mode(xsphy, inst, mode, submode);



	return 0;
}

static int mtk_phy_get_mode(struct mtk_xsphy *xsphy)
{
	struct device_node *of_chosen;
	struct device_node *np = xsphy->dev->of_node;
	char *bootargs;
	int mode = XSP_MODE_USB;

	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		goto done;

	bootargs = (char *)of_get_property(of_chosen,
			"bootargs", NULL);
	if (!bootargs)
		goto done;

	if (strstr(bootargs, XSP_MODE_UART_STR) &&
			of_find_node_with_property(np, "usb2uart"))
		mode = XSP_MODE_UART;
	else if (strstr(bootargs, XSP_MODE_JTAG_STR) &&
			of_find_node_with_property(np, "usb2jtag"))
		mode = XSP_MODE_JTAG;

done:
	return mode;
}

static int mtk_xsphy_uart_suspend(struct device *dev)
{
	return 0;
}
static int mtk_xsphy_uart_resume(struct device *dev)
{
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev);
	struct xsphy_instance *inst = xsphy->phys[0];
	void __iomem *pbase = inst->port_base;

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6, (0x1 << 23));

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, (0x1 << 3) | (0x1 << 18));

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, (0x3 << 30));

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, (0x1 << 30));

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, (0x1 << 29));

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, (0x1 << 26) | (0x1 << 27) | (0x1 << 28));

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM1, (0x1 << 16) | (0x1 << 17) | (0x1 << 18));

	mtk_phy_set_bits(pbase + XSP_U2PHYACR4, (0x1 << 17));

	mtk_phy_clear_bits(pbase + XSP_U2PHYDTM0, (0x3 << 4));

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0, (0x1 << 2) | (0x1 << 4));

	mtk_phy_set_bits(pbase + XSP_U2PHYDTM0,
		(0x1 << 17) | (0x1 << 19) | (0x1 << 20) | (0x1 << 21) | (0x1 << 23));

	return 0;
}

static int mtk_phy_uart_init(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "uart init\n");

	ret = clk_prepare_enable(inst->ref_clk);
	if (ret) {
		dev_info(xsphy->dev, "failed to enable ref_clk\n");
		return ret;
	}
	udelay(250);

	xsphy->suspend = mtk_xsphy_uart_suspend;
	xsphy->resume = mtk_xsphy_uart_resume;

	return 0;
}

static int mtk_phy_uart_exit(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "uart exit\n");

	clk_disable_unprepare(inst->ref_clk);
	return 0;
}

static int mtk_phy_jtag_init(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	void __iomem *pbase = inst->port_base;
	struct device *dev = &phy->dev;
	struct device_node *np = dev->of_node;
	struct of_phandle_args args;
	struct regmap *reg_base;
	u32 jtag_vers;
	u32 tmp;
	int ret;

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "jtag init\n");

	ret = of_parse_phandle_with_fixed_args(np, "usb2jtag", 1, 0, &args);
	if (ret)
		return ret;

	jtag_vers = args.args[0];
	reg_base = syscon_node_to_regmap(args.np);
	of_node_put(args.np);

	dev_info(xsphy->dev, "base - reg:0x%lx, version:%d\n",
			(unsigned long)reg_base, jtag_vers);

	ret = clk_prepare_enable(inst->ref_clk);
	if (ret) {
		dev_info(xsphy->dev, "failed to enable ref_clk\n");
		return ret;
	}

	mtk_phy_set_bits(pbase + XSP_U2PHYACR4, 0xf300);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR6, ~(0xf67ffff));

	mtk_phy_set_bits(pbase + XSP_USBPHYACR0, 0x1);

	mtk_phy_clear_bits(pbase + XSP_USBPHYACR2, ~(0xfffdffff));

	udelay(100);

	switch (jtag_vers) {
	case XSP_JTAG_V1:
		regmap_read(reg_base, 0xf00, &tmp);
		tmp |= 0x4030;
		regmap_write(reg_base, 0xf00, tmp);
		break;
	case XSP_JTAG_V2:
		regmap_read(reg_base, 0x100, &tmp);
		tmp |= 0x2;
		regmap_write(reg_base, 0x100, tmp);
		break;
	default:
		break;
	}

	queue_work(xsphy->wq, &inst->procfs_work);

	return ret;
}

static int mtk_phy_jtag_exit(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "jtag exit\n");

	clk_disable_unprepare(inst->ref_clk);
	return 0;
}

static struct phy *mtk_phy_xlate(struct device *dev,
				 struct of_phandle_args *args)
{
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev);
	struct xsphy_instance *inst = NULL;
	struct device_node *phy_np = args->np;
	int index;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < xsphy->nphys; index++)
		if (phy_np == xsphy->phys[index]->phy->dev.of_node) {
			inst = xsphy->phys[index];
			break;
		}

	if (!inst) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	inst->type = args->args[0];
	if (!(inst->type == PHY_TYPE_USB2 ||
	      inst->type == PHY_TYPE_USB3)) {
		dev_err(dev, "unsupported phy type: %d\n", inst->type);
		return ERR_PTR(-EINVAL);
	}

	if (!inst->property_ready) {
		phy_parse_property(xsphy, inst);
		phy_parse_efuse_property(xsphy, inst);
		inst->property_ready = true;
	}

	return inst->phy;
}

static const struct phy_ops mtk_xsphy_uart_ops = {
	.init		= mtk_phy_uart_init,
	.exit		= mtk_phy_uart_exit,
	.owner		= THIS_MODULE,
};

static const struct phy_ops mtk_xsphy_jtag_ops = {
	.init		= mtk_phy_jtag_init,
	.exit		= mtk_phy_jtag_exit,
	.owner		= THIS_MODULE,
};

static const struct phy_ops mtk_xsphy_ops = {
	.init		= mtk_phy_init,
	.exit		= mtk_phy_exit,
	.power_on	= mtk_phy_power_on,
	.power_off	= mtk_phy_power_off,
	.set_mode	= mtk_phy_set_mode,
	.owner		= THIS_MODULE,
};

static const struct of_device_id mtk_xsphy_id_table[] = {
	{ .compatible = "mediatek,xsphy", },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_xsphy_id_table);

static int mtk_xsphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct resource *glb_res;
	struct mtk_xsphy *xsphy;
	struct resource res;
	int port, retval, i;

	xsphy = devm_kzalloc(dev, sizeof(*xsphy), GFP_KERNEL);
	if (!xsphy)
		return -ENOMEM;

	xsphy->nphys = of_get_child_count(np);
	xsphy->phys = devm_kcalloc(dev, xsphy->nphys,
				       sizeof(*xsphy->phys), GFP_KERNEL);
	if (!xsphy->phys)
		return -ENOMEM;

	xsphy->dev = dev;

	xsphy->num_rptr = of_count_phandle_with_args(np,
			"phys", "#phy-cells");

	if (xsphy->num_rptr > 0) {
		xsphy->repeater = devm_kcalloc(dev, xsphy->num_rptr,
					sizeof(*xsphy->repeater), GFP_KERNEL);
		if (!xsphy->repeater)
			return -ENOMEM;
	} else {
		xsphy->num_rptr = 0;
	}

	for (i = 0; i < xsphy->num_rptr; i++) {
		xsphy->repeater[i] = devm_of_phy_get_by_index(dev, np, i);
		if (IS_ERR(xsphy->repeater[i]))
			dev_info(dev, "failed to get repeater-%d\n", i);
	}

	platform_set_drvdata(pdev, xsphy);

	glb_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* optional, may not exist if no u3 phys */
	/* and name is not equal to u2_port_base */
	if (glb_res && strcmp(glb_res->name, "u2_port_base") != 0) {
		/* get banks shared by multiple u3 phys */
		xsphy->glb_base = devm_ioremap_resource(dev, glb_res);
		if (IS_ERR(xsphy->glb_base)) {
			dev_err(dev, "failed to remap glb regs\n");
			return PTR_ERR(xsphy->glb_base);
		}
	}

	xsphy->src_ref_clk = XSP_REF_CLK;
	xsphy->src_coef = XSP_SLEW_RATE_COEF;
	/* update parameters of slew rate calibrate if exist */
	device_property_read_u32(dev, "mediatek,src-ref-clk-mhz",
				 &xsphy->src_ref_clk);
	device_property_read_u32(dev, "mediatek,src-coef", &xsphy->src_coef);

	xsphy->tx_chirpK_disable = device_property_read_bool(dev,
				"tx-chirpk-capable");

	dev_info(dev, "tx-chirpK-capable = %d\n", xsphy->tx_chirpK_disable);

	xsphy->bc11_switch_disable = device_property_read_bool(dev,
			"bc11-switch-disable");
	dev_info(dev, "bc11-switch-disable = %d\n", xsphy->bc11_switch_disable);

	xsphy->u2_procfs_disable = device_property_read_bool(dev,
				"u2-procfs-disable");

	dev_info(dev, "u2-procfs-disable = %d\n", xsphy->u2_procfs_disable);

	xsphy->sw_ver = mtk_xsphy_get_chipid();

	dev_info(dev, "xsphy->sw_ver = %d\n", xsphy->sw_ver);

	/* create phy workqueue */
	xsphy->wq = create_singlethread_workqueue("xsphy");
	if (!xsphy->wq)
		return -ENOMEM;

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct xsphy_instance *inst;
		struct phy *phy;
		int mode;

		inst = devm_kzalloc(dev, sizeof(*inst), GFP_KERNEL);
		if (!inst) {
			retval = -ENOMEM;
			goto put_child;
		}

		xsphy->phys[port] = inst;

		INIT_WORK(&inst->procfs_work, mtk_xsphy_procfs_init_worker);

		/* change ops to usb uart or jtage mode */
		mode = mtk_phy_get_mode(xsphy);
		switch (mode) {
		case XSP_MODE_UART:
			phy = devm_phy_create(dev, child_np,
				&mtk_xsphy_uart_ops);
			break;
		case XSP_MODE_JTAG:
			phy = devm_phy_create(dev, child_np,
				&mtk_xsphy_jtag_ops);
			break;
		default:
			phy = devm_phy_create(dev, child_np,
				&mtk_xsphy_ops);
		}

		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			retval = PTR_ERR(phy);
			goto put_child;
		}

		retval = of_address_to_resource(child_np, 0, &res);
		if (retval) {
			dev_err(dev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		inst->port_base = devm_ioremap_resource(&phy->dev, &res);
		if (IS_ERR(inst->port_base)) {
			dev_err(dev, "failed to remap phy regs\n");
			retval = PTR_ERR(inst->port_base);
			goto put_child;
		}

		/* Get optional property ippc address */
		retval = of_address_to_resource(child_np, 1, &res);
		if (retval) {
			dev_info(dev, "failed to get ippc resource(id-%d)\n",
				port);
		} else {
			inst->ippc_base = devm_ioremap(dev, res.start,
				resource_size(&res));
			if (IS_ERR(inst->ippc_base))
				dev_info(dev, "failed to remap ippc regs\n");
			else
				dev_info(dev, "ippc 0x%p\n", inst->ippc_base);

		}

		inst->phy = phy;
		inst->index = port;
		phy_set_drvdata(phy, inst);
		port++;

		inst->ref_clk = devm_clk_get(&phy->dev, "ref");
		if (IS_ERR(inst->ref_clk)) {
			dev_err(dev, "failed to get ref_clk(id-%d)\n", port);
			retval = PTR_ERR(inst->ref_clk);
			goto put_child;
		}

		/* rename phy lock name to avoid possible lock warning. */
		lockdep_set_subclass(&inst->phy->mutex, XSPHY_SUB_CLASS);
	}

	provider = devm_of_phy_provider_register(dev, mtk_phy_xlate);
	return PTR_ERR_OR_ZERO(provider);

put_child:
	of_node_put(child_np);
	return retval;
}

static int __maybe_unused mtk_xsphy_suspend(struct device *dev)
{
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev);

	if (xsphy->suspend)
		return xsphy->suspend(dev);
	return 0;
}
static int __maybe_unused mtk_xsphy_resume(struct device *dev)
{
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev);

	if (xsphy->resume)
		return xsphy->resume(dev);
	return 0;
}
static const struct dev_pm_ops mtk_xsphy_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_xsphy_suspend, mtk_xsphy_resume)
};
#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &mtk_xsphy_pm_ops : NULL)

static int mtk_xsphy_remove(struct platform_device *pdev)
{
	struct mtk_xsphy *xsphy = dev_get_drvdata(&pdev->dev);

	mtk_xsphy_procfs_exit(xsphy);
	return 0;
}

static struct platform_driver mtk_xsphy_driver = {
	.probe		= mtk_xsphy_probe,
	.remove		= mtk_xsphy_remove,
	.driver		= {
		.name	= "mtk-xsphy",
		.pm = DEV_PM_OPS,
		.of_match_table = mtk_xsphy_id_table,
	},
};

module_platform_driver(mtk_xsphy_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_DESCRIPTION("MediaTek USB XS-PHY driver");
MODULE_LICENSE("GPL v2");
