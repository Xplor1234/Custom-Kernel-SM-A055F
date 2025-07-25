// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
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

/* version V1 sub-banks offset base address */
/* banks shared by multiple phys */
#define SSUSB_SIFSLV_V1_SPLLC		0x000	/* shared by u3 phys */
#define SSUSB_SIFSLV_V1_U2FREQ		0x100	/* shared by u2 phys */
#define SSUSB_SIFSLV_V1_CHIP		0x300	/* shared by u3 phys */
/* u2 phy bank */
#define SSUSB_SIFSLV_V1_U2PHY_COM	0x000
/* u3/pcie/sata phy banks */
#define SSUSB_SIFSLV_V1_U3PHYD		0x000
#define SSUSB_SIFSLV_V1_U3PHYA		0x200

/* version V2/V3 sub-banks offset base address */
/* V3: U2FREQ is not used anymore, but reserved */
/* u2 phy banks */
#define SSUSB_SIFSLV_V2_MISC		0x000
#define SSUSB_SIFSLV_V2_U2FREQ		0x100
#define SSUSB_SIFSLV_V2_U2PHY_COM	0x300
/* u3/pcie/sata phy banks */
#define SSUSB_SIFSLV_V2_SPLLC		0x000
#define SSUSB_SIFSLV_V2_CHIP		0x100
#define SSUSB_SIFSLV_V2_U3PHYD		0x200
#define SSUSB_SIFSLV_V2_U3PHYA		0x400

#define U3P_MISC_REG1		0x04
#define MR1_EFUSE_AUTO_LOAD_DIS		BIT(6)

#define U3P_USBPHYACR0		0x000
#define PA0_RG_U2PLL_FORCE_ON		BIT(15)
#define PA0_USB20_PLL_PREDIV		GENMASK(7, 6)
#define PA0_RG_USB20_INTR_EN		BIT(5)
#define PA0_RG_USB20_BGR_DIV		GENMASK(3, 2)
#define PA0_RG_USB20_BGR_DIV_MASK	(0x3)
#define PA0_RG_USB20_BGR_DIV_OFET	(2)

#define U3P_USBPHYACR1		0x004
#define PA1_RG_INTR_CAL		GENMASK(23, 19)
#define PA1_RG_VRT_SEL			GENMASK(14, 12)
#define PA1_RG_VRT_SEL_MASK	(0x7)
#define PA1_RG_VRT_SEL_OFST	(12)
#define PA1_RG_TERM_SEL		GENMASK(10, 8)
#define PA1_RG_TERM_SEL_MASK	(0x7)
#define PA1_RG_TERM_SEL_OFST	(8)

#define U3P_USBPHYACR2		0x008
#define PA2_RG_U2PLL_BW			GENMASK(21, 19)
#define PA2_RG_SIF_U2PLL_FORCE_EN	BIT(18)
#define PA2_RG_USB20_PLL_BW		GENMASK(21, 19)
#define PA2_RG_USB20_PLL_BW_MASK	(0x7)
#define PA2_RG_USB20_PLL_BW_OFET	(19)

#define PA2_RG_U2_CLKREF_REV	BIT(3)
#define PA2_RG_U2_CLKREF_REV_1		GENMASK(12, 11)

#define U3P_USBPHYACR5		0x014
#define PA5_RG_U2_HSTX_SRCAL_EN	BIT(15)
#define PA5_RG_U2_HSTX_SRCTRL		GENMASK(14, 12)
#define PA5_RG_U2_HS_100U_U3_EN	BIT(11)

#define U3P_USBPHYACR6		0x018
#define PA6_RG_U2_PRE_EMP		GENMASK(31, 30)
#define PA6_RG_U2_PHY_REV6		GENMASK(31, 30)
#define PA6_RG_U2_PHY_REV6_VAL(x)	((0x3 & (x)) << 30)
#define PA6_RG_U2_PHY_REV6_MASK	(0x3)
#define PA6_RG_U2_PHY_REV6_OFET	(30)
#define PA6_RG_U2_PHY_REV4		BIT(28)
#define PA6_RG_U2_PHY_REV4_MASK	(0x1)
#define PA6_RG_U2_PHY_REV4_OFET	(28)
#define PA6_RG_U2_PHY_REV1		BIT(25)
#define PA6_RG_U2_BC11_SW_EN		BIT(23)
#define PA6_RG_U2_OTG_VBUSCMP_EN	BIT(20)
#define PA6_RG_U2_DISCTH		GENMASK(7, 4)
#define PA6_RG_U2_DISCTH_MASK	(0xf)
#define PA6_RG_U2_DISCTH_OFET	(4)
#define PA6_RG_U2_SQTH		GENMASK(3, 0)
#define PA6_RG_U2_SQTH_MASK	(0xf)
#define PA6_RG_U2_SQTH_OFET	(0)

#define U3P_U2PHYACR3		0x01c
#define P2C_RG_USB20_PUPD_BIST_EN	BIT(12)
#define P2C_RG_USB20_EN_PU_DP		BIT(9)

#define U3P_U2PHYACR4		0x020
#define P2C_RG_USB20_GPIO_CTL		BIT(9)
#define P2C_USB20_GPIO_MODE		BIT(8)
#define P2C_U2_GPIO_CTR_MSK	(P2C_RG_USB20_GPIO_CTL | P2C_USB20_GPIO_MODE)

#define U3P_U2PHYA_RESV		0x030
#define P2R_RG_U2PLL_FBDIV_26M		0x1bb13b
#define P2R_RG_U2PLL_FBDIV_48M		0x3c0000

#define U3P_U2PHYA_RESV1	0x044
#define P2R_RG_U2PLL_REFCLK_SEL	BIT(5)
#define P2R_RG_U2PLL_FRA_EN		BIT(3)

#define U3D_U2PHYDCR0		0x060
#define P2C_RG_SIF_U2PLL_FORCE_ON	BIT(24)

#define U3P_U2PHYDTM0		0x068
#define P2C_FORCE_UART_EN		BIT(26)
#define P2C_FORCE_DATAIN		BIT(23)
#define P2C_FORCE_DM_PULLDOWN		BIT(21)
#define P2C_FORCE_DP_PULLDOWN		BIT(20)
#define P2C_FORCE_XCVRSEL		BIT(19)
#define P2C_FORCE_SUSPENDM		BIT(18)
#define P2C_FORCE_TERMSEL		BIT(17)
#define P2C_RG_DATAIN			GENMASK(13, 10)
#define P2C_RG_DMPULLDOWN		BIT(7)
#define P2C_RG_DPPULLDOWN		BIT(6)
#define P2C_RG_XCVRSEL			GENMASK(5, 4)
#define P2C_RG_XCVRSEL_VAL(x)		((0x3 & (x)) << 4)
#define P2C_RG_SUSPENDM			BIT(3)
#define P2C_RG_TERMSEL			BIT(2)
#define P2C_DTM0_PART_MASK \
		(P2C_FORCE_DATAIN | P2C_FORCE_DM_PULLDOWN | \
		P2C_FORCE_DP_PULLDOWN | P2C_FORCE_XCVRSEL | \
		P2C_FORCE_SUSPENDM | P2C_FORCE_TERMSEL | \
		P2C_RG_DMPULLDOWN | P2C_RG_DPPULLDOWN | \
		P2C_RG_TERMSEL)

#define P2C_DTM0_PART_MASK2 \
		(P2C_FORCE_DM_PULLDOWN | P2C_FORCE_DP_PULLDOWN | \
		P2C_FORCE_XCVRSEL | P2C_FORCE_SUSPENDM | \
		P2C_FORCE_TERMSEL | P2C_RG_DMPULLDOWN | \
		P2C_RG_DPPULLDOWN | P2C_RG_TERMSEL)

#define U3P_U2PHYDTM1		0x06C
#define P2C_RG_UART_EN			BIT(16)
#define P2C_FORCE_VBUSVALID		BIT(13)
#define P2C_FORCE_SESSEND		BIT(12)
#define P2C_FORCE_BVALID		BIT(11)
#define P2C_FORCE_AVALID		BIT(10)
#define P2C_FORCE_IDDIG		BIT(9)
#define P2C_FORCE_IDPULLUP		BIT(8)
#define P2C_RG_VBUSVALID		BIT(5)
#define P2C_RG_SESSEND			BIT(4)
#define P2C_RG_BVALID			BIT(3)
#define P2C_RG_AVALID			BIT(2)
#define P2C_RG_IDDIG			BIT(1)
#define P2C_RG_RG_IDPULLUP		BIT(0)

#define U3P_U2PHYBC12C		0x080
#define P2C_RG_CHGDT_EN		BIT(0)

#define U3P_U3_CHIP_GPIO_CTLD		0x0c
#define P3C_REG_IP_SW_RST		BIT(31)
#define P3C_MCU_BUS_CK_GATE_EN		BIT(30)
#define P3C_FORCE_IP_SW_RST		BIT(29)

#define U3P_U3_CHIP_GPIO_CTLE		0x10
#define P3C_RG_SWRST_U3_PHYD		BIT(25)
#define P3C_RG_SWRST_U3_PHYD_FORCE_EN	BIT(24)

#define U3P_U3_PHYA_REG0	0x000
#define P3A_RG_IEXT_INTR		GENMASK(15, 10)
#define P3A_RG_CLKDRV_OFF		GENMASK(3, 2)

#define U3P_U3_PHYA_REG1	0x004
#define P3A_RG_CLKDRV_AMP		GENMASK(31, 29)
#define RG_SSUSB_VA_ON			BIT(29)

#define U3P_U3_PHYA_REG6	0x018
#define P3A_RG_TX_EIDLE_CM		GENMASK(31, 28)

#define U3P_U3_PHYA_REG9	0x024
#define P3A_RG_RX_DAC_MUX		GENMASK(5, 1)

#define U3P_U3_PHYA_DA_REG0	0x100
#define P3A_RG_XTAL_EXT_PE2H		GENMASK(17, 16)
#define P3A_RG_XTAL_EXT_PE1H		GENMASK(13, 12)
#define P3A_RG_XTAL_EXT_EN_U3		GENMASK(11, 10)

#define U3P_U3_PHYA_DA_REG4	0x108
#define P3A_RG_PLL_DIVEN_PE2H		GENMASK(21, 19)
#define P3A_RG_PLL_BC_PE2H		GENMASK(7, 6)

#define U3P_U3_PHYA_DA_REG5	0x10c
#define P3A_RG_PLL_BR_PE2H		GENMASK(29, 28)
#define P3A_RG_PLL_IC_PE2H		GENMASK(15, 12)

#define U3P_U3_PHYA_DA_REG6	0x110
#define P3A_RG_PLL_IR_PE2H		GENMASK(19, 16)

#define U3P_U3_PHYA_DA_REG7	0x114
#define P3A_RG_PLL_BP_PE2H		GENMASK(19, 16)

#define U3P_U3_PHYA_DA_REG20	0x13c
#define P3A_RG_PLL_DELTA1_PE2H		GENMASK(31, 16)

#define U3P_U3_PHYA_DA_REG25	0x148
#define P3A_RG_PLL_DELTA_PE2H		GENMASK(15, 0)

#define U3P_U3_PHYD_MIX0		0x000

#define U3P_U3_PHYD_LFPS1		0x00c
#define P3D_RG_FWAKE_TH		GENMASK(21, 16)

#define U3P_U3_PHYD_IMPCAL0		0x010
#define P3D_RG_FORCE_TX_IMPEL		BIT(31)
#define P3D_RG_TX_IMPEL			GENMASK(28, 24)

#define U3P_U3_PHYD_IMPCAL1		0x014
#define P3D_RG_FORCE_RX_IMPEL		BIT(31)
#define P3D_RG_RX_IMPEL			GENMASK(28, 24)

#define U3P_U3_PHYD_RX0			0x02c

#define U3P_U3_PHYD_T2RLB		0x030

#define U3P_U3_PHYD_PIPE0		0x040

#define U3P_U3_PHYD_RSV			0x054
#define P3D_RG_EFUSE_AUTO_LOAD_DIS	BIT(12)

#define U3P_U3_PHYD_CDR1		0x05c
#define P3D_RG_CDR_BIR_LTD1		GENMASK(28, 24)
#define P3D_RG_CDR_BIR_LTD0		GENMASK(12, 8)

#define U3P_U3_PHYD_EQ_EYE3		0xdc
#define P3D_RG_EQ_LEQ_SHIFT		GENMASK(26, 24)

#define U3P_U3_PHYD_RXDET1		0x128
#define P3D_RG_RXDET_STB2_SET		GENMASK(17, 9)

#define U3P_U3_PHYD_RXDET2		0x12c
#define P3D_RG_RXDET_STB2_SET_P3	GENMASK(8, 0)

#define U3P_SPLLC_XTALCTL3		0x018
#define XC3_RG_U3_XTAL_RX_PWD		BIT(9)
#define XC3_RG_U3_FRC_XTAL_RX_PWD	BIT(8)

#define U3P_U2FREQ_FMCR0	0x00
#define P2F_RG_MONCLK_SEL	GENMASK(27, 26)
#define P2F_RG_FREQDET_EN	BIT(24)
#define P2F_RG_CYCLECNT		GENMASK(23, 0)

#define U3P_U2FREQ_VALUE	0x0c

#define U3P_U2FREQ_FMMONR1	0x10
#define P2F_USB_FM_VALID	BIT(0)
#define P2F_RG_FRCK_EN		BIT(8)

#define U3P_REF_CLK		26	/* MHZ */
#define U3P_SLEW_RATE_COEF	28
#define U3P_SR_COEF_DIVISOR	1000
#define U3P_FM_DET_CYCLE_CNT	1024

/* SATA register setting */
#define PHYD_CTRL_SIGNAL_MODE4		0x1c
/* CDR Charge Pump P-path current adjustment */
#define RG_CDR_BICLTD1_GEN1_MSK		GENMASK(23, 20)
#define RG_CDR_BICLTD0_GEN1_MSK		GENMASK(11, 8)

#define PHYD_DESIGN_OPTION2		0x24
/* Symbol lock count selection */
#define RG_LOCK_CNT_SEL_MSK		GENMASK(5, 4)

#define PHYD_DESIGN_OPTION9	0x40
/* COMWAK GAP width window */
#define RG_TG_MAX_MSK		GENMASK(20, 16)
/* COMINIT GAP width window */
#define RG_T2_MAX_MSK		GENMASK(13, 8)
/* COMWAK GAP width window */
#define RG_TG_MIN_MSK		GENMASK(7, 5)
/* COMINIT GAP width window */
#define RG_T2_MIN_MSK		GENMASK(4, 0)

#define ANA_RG_CTRL_SIGNAL1		0x4c
/* TX driver tail current control for 0dB de-empahsis mdoe for Gen1 speed */
#define RG_IDRV_0DB_GEN1_MSK		GENMASK(13, 8)

#define ANA_RG_CTRL_SIGNAL4		0x58
#define RG_CDR_BICLTR_GEN1_MSK		GENMASK(23, 20)
/* Loop filter R1 resistance adjustment for Gen1 speed */
#define RG_CDR_BR_GEN2_MSK		GENMASK(10, 8)

#define ANA_RG_CTRL_SIGNAL6		0x60
/* I-path capacitance adjustment for Gen1 */
#define RG_CDR_BC_GEN1_MSK		GENMASK(28, 24)
#define RG_CDR_BIRLTR_GEN1_MSK		GENMASK(4, 0)

#define ANA_EQ_EYE_CTRL_SIGNAL1		0x6c
/* RX Gen1 LEQ tuning step */
#define RG_EQ_DLEQ_LFI_GEN1_MSK		GENMASK(11, 8)

#define ANA_EQ_EYE_CTRL_SIGNAL4		0xd8
#define RG_CDR_BIRLTD0_GEN1_MSK		GENMASK(20, 16)

#define ANA_EQ_EYE_CTRL_SIGNAL5		0xdc
#define RG_CDR_BIRLTD0_GEN3_MSK		GENMASK(4, 0)

/* PHY switch between pcie/usb3/sgmii/sata */
#define USB_PHY_SWITCH_CTRL	0x0
#define RG_PHY_SW_TYPE		GENMASK(3, 0)
#define RG_PHY_SW_PCIE		0x0
#define RG_PHY_SW_USB3		0x1
#define RG_PHY_SW_SGMII		0x2
#define RG_PHY_SW_SATA		0x3

#define TPHY_CLKS_CNT	2

#define TERM_SEL_STR "term_sel"
#define VRT_SEL_STR "vrt_sel"
#define PHY_REV4_STR "phy_rev4"
#define PHY_REV6_STR "phy_rev6"
#define DISCTH_STR "discth"
#define RX_SQTH_STR "rx_sqth"
#define SIB_STR	"sib"
#define LOOPBACK_STR "loopback_test"

#define PHY_MODE_UART "usb2uart_mode=1"
#define PHY_MODE_JTAG "usb2jtag_mode=1"

enum mtk_phy_version {
	MTK_PHY_V1 = 1,
	MTK_PHY_V2,
	MTK_PHY_V3,
};

enum mtk_phy_jtag_version {
	MTK_PHY_JTAG_V1 = 1,
	MTK_PHY_JTAG_V2,
};

enum mtk_phy_efuse {
	INTR_CAL = 0,
	IEXT_INTR_CTRL,
	RX_IMPSEL,
	TX_IMPSEL,
};

static char *efuse_name[4] = {
	"intr_cal",
	"iext_intr_ctrl",
	"rx_impsel",
	"tx_impsel",
};

struct mtk_phy_pdata {
	/* avoid RX sensitivity level degradation only for mt8173 */
	bool avoid_rx_sen_degradation;
	/*
	 * workaround only for mt8195, HW fix it for others of V3,
	 * u2phy should use integer mode instead of fractional mode of
	 * 48M PLL, fix it by switching PLL to 26M from default 48M
	 */
	bool sw_pll_48m_to_26m;
	/*
	 * Some SoCs (e.g. mt8195) drop a bit when use auto load efuse,
	 * support sw way, also support it for v2/v3 optionally.
	 */
	bool sw_efuse_supported;
	enum mtk_phy_version version;
};

struct u2phy_banks {
	void __iomem *misc;
	void __iomem *fmreg;
	void __iomem *com;
};

struct u3phy_banks {
	void __iomem *spllc;
	void __iomem *chip;
	void __iomem *phyd; /* include u3phyd_bank2 */
	void __iomem *phya; /* include u3phya_da */
};

struct mtk_phy_instance {
	struct phy *phy;
	void __iomem *port_base;
	void __iomem *ippc_base;
	union {
		struct u2phy_banks u2_banks;
		struct u3phy_banks u3_banks;
	};
	struct clk_bulk_data clks[TPHY_CLKS_CNT];
	u32 index;
	u32 type;
	struct regmap *type_sw;
	u32 type_sw_reg;
	u32 type_sw_index;
	u32 efuse_sw_en;
	u32 efuse_intr;
	u32 efuse_tx_imp;
	u32 efuse_rx_imp;
	int eye_src;
	int eye_vrt;
	int eye_term;
	int intr;
	int discth;
	int pre_emphasis;
	int rx_sqth;
	int rev4;
	int rev6;
	int pll_bw;
	int bgr_div;
	int fsrxlvl;
	int eq_leq_shift;
	bool bc12_en;
	/* u2 eye diagram for host */
	int eye_src_host;
	int eye_vrt_host;
	int eye_term_host;
	int rev6_host;
	int discth_host;
	struct proc_dir_entry *phy_root;
	bool set_efuse_v1;
	bool usb_special_phy_settings;
};

struct mtk_tphy {
	struct device *dev;
	void __iomem *sif_base;	/* only shared sif */
	const struct mtk_phy_pdata *pdata;
	struct mtk_phy_instance **phys;
	int nphys;
	int src_ref_clk; /* MHZ, reference clock for slew rate calibrate */
	int src_coef; /* coefficient for slew rate calibrate */
	struct proc_dir_entry *root;
};

static void u2_phy_props_set(struct mtk_tphy *tphy,
		struct mtk_phy_instance *instance);

static void u2_phy_host_props_set(struct mtk_tphy *tphy,
		struct mtk_phy_instance *instance);

static ssize_t proc_sib_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mtk_phy_instance *instance = s->private;
	struct device *dev = &instance->phy->dev;
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	char buf[20];
	unsigned int val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (IS_ERR_OR_NULL(instance->ippc_base))
		return -ENODEV;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	/* SSUSB_SIFSLV_IPPC_BASE SSUSB_IP_SW_RST = 0 */
	writel(0x00031000, instance->ippc_base + 0x00);
	/* SSUSB_IP_HOST_PDN = 0 */
	writel(0x00000000, instance->ippc_base + 0x04);
	/* SSUSB_IP_DEV_PDN = 0 */
	writel(0x00000000, instance->ippc_base + 0x08);
	/* SSUSB_IP_PCIE_PDN = 0 */
	writel(0x00000000, instance->ippc_base + 0x0C);
	/* SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
	writel(0x0000000C, instance->ippc_base + 0x30);

	/*
	 * USBMAC mode is 0x62910002 (bit 1)
	 * MDSIB  mode is 0x62910008 (bit 3)
	 * 0x0629 just likes a signature. Can't be removed.
	 */
	if (val)
		writel(0x62910008, u3_banks->chip);
	else
		writel(0x62910002, u3_banks->chip);

	dev_info(dev, "%s, sib=%d\n", __func__, val);
	return count;
}

static int proc_sib_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct device *dev = &instance->phy->dev;
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	unsigned int val;
	u32 tmp;

	tmp = readl(u3_banks->chip);

	if (tmp == 0x62910008)
		val = 1;
	else
		val = 0;

	dev_info(dev, "%s, sib=%d\n", __func__, val);
	seq_printf(s, "%d\n", val);
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

/*
 * loopback_test: default test pattern
 *   readl(U3D_PHYD_PIPE0) &
 *     ~(0x01<<30)) | 0x01<<30,
 *     ~(0x01<<28)) | 0x00<<28,
 *     ~(0x03<<26)) | 0x01<<26,
 *     ~(0x03<<24)) | 0x00<<24,
 *     ~(0x01<<22)) | 0x00<<22,
 *     ~(0x01<<21)) | 0x00<<21,
 *     ~(0x01<<20)) | 0x01<<20.
 */
#define U3P_U3_PHYD_PIPE0_CLR_PATTERN	0x5f700000
#define U3P_U3_PHYD_PIPE0_SET_PATTERN	0x44100000

static int proc_loopback_test_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct device *dev = &instance->phy->dev;
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	int r_pipe0, r_rx0, r_mix0, r_t2rlb;
	bool ret = false;
	u32 tmp;

	r_mix0 = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	r_rx0 = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	r_t2rlb = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	r_pipe0 = readl(u3_banks->phyd + U3P_U3_PHYD_PIPE0);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_PIPE0);
	tmp &= ~(U3P_U3_PHYD_PIPE0_CLR_PATTERN);
	tmp |= U3P_U3_PHYD_PIPE0_SET_PATTERN;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_PIPE0);

	mdelay(10);

	/* T2R loop back disable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01 << 15);
	tmp |= 0x00 << 15;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	mdelay(10);

	/* TSEQ lock detect threshold */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x07 << 24);
	tmp |= 0x07 << 24;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	/* set default TSEQ polarity check value = 1 */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x01 << 28);
	tmp |= 0x01 << 28;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	/* TSEQ polarity check enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x01 << 29);
	tmp |= 0x01 << 29;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	/* TSEQ decoder enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x01 << 30);
	tmp |= 0x01 << 30;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	mdelay(10);

	/* set T2R loop back TSEQ length (x 16us) */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	tmp &= ~(0xff << 0);
	tmp |= 0xf0 << 0;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	/* set T2R loop back BDAT reset period (x 16us) */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	tmp &= ~(0x0f << 12);
	tmp |= 0x0f << 12;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	/* T2R loop back pattern select */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	tmp &= ~(0x03 << 8);
	tmp |= 0x00 << 8;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	mdelay(10);

	/* T2R loop back serial mode */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01 << 13);
	tmp |= 0x01 << 13;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	/* T2R loop back parallel mode = 0 */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01 << 12);
	tmp |= 0x00 << 12;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	/* T2R loop back mode enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01 << 11);
	tmp |= 0x01 << 11;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	/* T2R loop back enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01 << 15);
	tmp |= 0x01 << 15;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);
	mdelay(100);

	dev_info(dev, "%s, U3 loop back started\n", __func__);

	/* check result */
	tmp = readl(u3_banks->phyd + 0x0b4);

	/* verbose dump */
	dev_info(dev, "rb back           /SIB/  : 0x%x\n", tmp);
	dev_info(dev, "rb t2rlb_lock  : %d\n", (tmp >> 2) & 0x01);
	dev_info(dev, "rb t2rlb_pass  : %d\n", (tmp >> 3) & 0x01);
	dev_info(dev, "rb t2rlb_passth: %d\n", (tmp >> 4) & 0x01);

	/* return result */
	tmp &= 0x0E;
	if (tmp == 0x0E)
		ret = true;
	else
		ret = false;

	/* restore settings */
	writel(r_rx0, u3_banks->phyd + U3P_U3_PHYD_RX0);
	writel(r_pipe0, u3_banks->phyd + U3P_U3_PHYD_PIPE0);
	writel(r_mix0, u3_banks->phyd + U3P_U3_PHYD_MIX0);
	writel(r_t2rlb, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	dev_info(dev, "%s, loopback_test=0x%x\n", __func__, tmp);

	seq_printf(s,  "%d\n", ret);
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

static int u3_phy_procfs_init(struct mtk_tphy *tphy,
			struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	struct proc_dir_entry *root = tphy->root;
	struct proc_dir_entry *phy_root;
	struct proc_dir_entry *file;
	int ret;

	if (!root) {
		dev_info(dev, "phy proc root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	phy_root = proc_mkdir("u3_phy", root);
	if (!root) {
		dev_info(dev, "failed to creat dir proc u3_phy\n");
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(SIB_STR, 0644,
			phy_root, &proc_sib_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", SIB_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(LOOPBACK_STR, 0444,
			phy_root, &proc_loopback_test_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", LOOPBACK_STR);
		ret = -ENOMEM;
		goto err1;
	}

	instance->phy_root = phy_root;
	return 0;
err1:
	proc_remove(phy_root);

err0:
	return ret;
}

static int u3_phy_procfs_exit(struct mtk_phy_instance *instance)
{
	proc_remove(instance->phy_root);
	return 0;
}

static int proc_term_sel_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR1);
	tmp >>= PA1_RG_TERM_SEL_OFST;
	tmp &= PA1_RG_TERM_SEL_MASK;

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_TERM_SEL, val);

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR1);
	tmp >>= PA1_RG_VRT_SEL_OFST;
	tmp &= PA1_RG_VRT_SEL_MASK;

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_VRT_SEL, val);

	return count;
}

static const struct  proc_ops proc_vrt_sel_fops = {
	.proc_open = proc_vrt_sel_open,
	.proc_write = proc_vrt_sel_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_phy_rev4_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR6);
	tmp >>= PA6_RG_U2_PHY_REV4_OFET;
	tmp &= PA6_RG_U2_PHY_REV4_MASK;

	cover_val_to_str(tmp, 1, str);

	seq_printf(s, "\n%s = %s\n", PHY_REV4_STR, str);
	return 0;
}

static int proc_phy_rev4_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_phy_rev4_show, pde_data(inode));
}

static ssize_t proc_phy_rev4_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV4, val);

	return count;
}

static const struct proc_ops proc_phy_rev4_fops = {
	.proc_open = proc_phy_rev4_open,
	.proc_write = proc_phy_rev4_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_phy_rev6_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR6);
	tmp >>= PA6_RG_U2_PHY_REV6_OFET;
	tmp &= PA6_RG_U2_PHY_REV6_MASK;

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV6, val);

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR6);
	tmp >>= PA6_RG_U2_DISCTH_OFET;
	tmp &= PA6_RG_U2_DISCTH_MASK;

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_DISCTH, val);

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR6);
	tmp >>= PA6_RG_U2_SQTH_OFET;
	tmp &= PA6_RG_U2_SQTH_MASK;

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
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_SQTH, val);

	return count;
}

static const struct proc_ops proc_rx_sqth_fops = {
	.proc_open = proc_rx_sqth_open,
	.proc_write = proc_rx_sqth_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
static int u2_phy_procfs_init(struct mtk_tphy *tphy,
			struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	struct proc_dir_entry *root = tphy->root;
	struct proc_dir_entry *phy_root;
	struct proc_dir_entry *file;
	int ret;

	if (!root) {
		dev_info(dev, "proc root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	phy_root = proc_mkdir("u2_phy", root);
	if (!root) {
		dev_info(dev, "failed to creat dir proc /u2_phy\n");
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(TERM_SEL_STR, 0644,
			phy_root, &proc_term_sel_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TERM_SEL_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(VRT_SEL_STR, 0644,
			phy_root, &proc_vrt_sel_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", VRT_SEL_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PHY_REV4_STR, 0644,
			phy_root, &proc_phy_rev4_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PHY_REV4_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PHY_REV6_STR, 0644,
			phy_root, &proc_phy_rev6_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PHY_REV6_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(DISCTH_STR, 0644,
			phy_root, &proc_discth_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", DISCTH_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(RX_SQTH_STR, 0644,
			phy_root, &proc_rx_sqth_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", RX_SQTH_STR);
		ret = -ENOMEM;
		goto err1;
	}

	instance->phy_root = phy_root;
	return 0;
err1:
	proc_remove(phy_root);

err0:
	return ret;
}

static int u2_phy_procfs_exit(struct mtk_phy_instance *instance)
{
	proc_remove(instance->phy_root);
	return 0;
}

static int mtk_phy_procfs_init(struct mtk_tphy *tphy)
{
	struct proc_dir_entry *root = NULL;

	proc_mkdir("mtk_usb", NULL);

	root = proc_mkdir("mtk_usb/usb-phy0", NULL);
	if (!root) {
		dev_info(tphy->dev, "failed to creat usb-phy0  dir\n");
		return -ENOMEM;
	}

	tphy->root = root;
	return 0;
}

static int mtk_phy_procfs_exit(struct mtk_tphy *tphy)
{
	proc_remove(tphy->root);
	return 0;
}


static void hs_slew_rate_calibrate(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *fmreg = u2_banks->fmreg;
	void __iomem *com = u2_banks->com;
	int calibration_val;
	int fm_out;
	u32 tmp;

	/* HW V3 doesn't support slew rate cal anymore */
	if (tphy->pdata->version == MTK_PHY_V3)
		return;

	/* use force value */
	if (instance->eye_src)
		return;

	/* enable USB ring oscillator */
	mtk_phy_set_bits(com + U3P_USBPHYACR5, PA5_RG_U2_HSTX_SRCAL_EN);
	udelay(1);

	/*enable free run clock */
	mtk_phy_set_bits(fmreg + U3P_U2FREQ_FMMONR1, P2F_RG_FRCK_EN);

	/* set cycle count as 1024, and select u2 channel */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~(P2F_RG_CYCLECNT | P2F_RG_MONCLK_SEL);
	tmp |= FIELD_PREP(P2F_RG_CYCLECNT, U3P_FM_DET_CYCLE_CNT);
	if (tphy->pdata->version == MTK_PHY_V1)
		tmp |= FIELD_PREP(P2F_RG_MONCLK_SEL, instance->index >> 1);

	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* enable frequency meter */
	mtk_phy_set_bits(fmreg + U3P_U2FREQ_FMCR0, P2F_RG_FREQDET_EN);

	/* ignore return value */
	readl_poll_timeout(fmreg + U3P_U2FREQ_FMMONR1, tmp,
			   (tmp & P2F_USB_FM_VALID), 10, 200);

	fm_out = readl(fmreg + U3P_U2FREQ_VALUE);

	/* disable frequency meter */
	mtk_phy_clear_bits(fmreg + U3P_U2FREQ_FMCR0, P2F_RG_FREQDET_EN);

	/*disable free run clock */
	mtk_phy_clear_bits(fmreg + U3P_U2FREQ_FMMONR1, P2F_RG_FRCK_EN);

	if (fm_out) {
		/* ( 1024 / FM_OUT ) x reference clock frequency x coef */
		tmp = tphy->src_ref_clk * tphy->src_coef;
		tmp = (tmp * U3P_FM_DET_CYCLE_CNT) / fm_out;
		calibration_val = DIV_ROUND_CLOSEST(tmp, U3P_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		calibration_val = 4;
	}
	dev_dbg(tphy->dev, "phy:%d, fm_out:%d, calib:%d (clk:%d, coef:%d)\n",
		instance->index, fm_out, calibration_val,
		tphy->src_ref_clk, tphy->src_coef);

	/* set HS slew rate */
	mtk_phy_update_field(com + U3P_USBPHYACR5, PA5_RG_U2_HSTX_SRCTRL,
			     calibration_val);

	/* disable USB ring oscillator */
	mtk_phy_clear_bits(com + U3P_USBPHYACR5, PA5_RG_U2_HSTX_SRCAL_EN);
}

static void u3_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	void __iomem *phya = u3_banks->phya;
	void __iomem *phyd = u3_banks->phyd;

	/* ssusb power on */
	mtk_phy_set_bits(phya + U3P_U3_PHYA_REG1, RG_SSUSB_VA_ON);

	/* gating PCIe Analog XTAL clock */
	mtk_phy_set_bits(u3_banks->spllc + U3P_SPLLC_XTALCTL3,
			 XC3_RG_U3_XTAL_RX_PWD | XC3_RG_U3_FRC_XTAL_RX_PWD);

	/* gating XSQ */
	mtk_phy_update_field(phya + U3P_U3_PHYA_DA_REG0, P3A_RG_XTAL_EXT_EN_U3, 2);

	mtk_phy_update_field(phya + U3P_U3_PHYA_REG9, P3A_RG_RX_DAC_MUX, 4);

	mtk_phy_update_field(phya + U3P_U3_PHYA_REG6, P3A_RG_TX_EIDLE_CM, 0xe);

	mtk_phy_update_bits(u3_banks->phyd + U3P_U3_PHYD_CDR1,
			    P3D_RG_CDR_BIR_LTD0 | P3D_RG_CDR_BIR_LTD1,
			    FIELD_PREP(P3D_RG_CDR_BIR_LTD0, 0xc) |
			    FIELD_PREP(P3D_RG_CDR_BIR_LTD1, 0x3));

	mtk_phy_update_field(phyd + U3P_U3_PHYD_LFPS1, P3D_RG_FWAKE_TH, 0x34);

	mtk_phy_update_field(phyd + U3P_U3_PHYD_RXDET1, P3D_RG_RXDET_STB2_SET, 0x10);

	mtk_phy_update_field(phyd + U3P_U3_PHYD_RXDET2, P3D_RG_RXDET_STB2_SET_P3, 0x10);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void u2_phy_pll_26m_set(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;

	if (!tphy->pdata->sw_pll_48m_to_26m)
		return;

	mtk_phy_update_field(com + U3P_USBPHYACR0, PA0_USB20_PLL_PREDIV, 0);

	mtk_phy_update_field(com + U3P_USBPHYACR2, PA2_RG_U2PLL_BW, 3);

	writel(P2R_RG_U2PLL_FBDIV_26M, com + U3P_U2PHYA_RESV);

	mtk_phy_set_bits(com + U3P_U2PHYA_RESV1,
			 P2R_RG_U2PLL_FRA_EN | P2R_RG_U2PLL_REFCLK_SEL);
}

static void u2_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;

	/* switch to USB function, and enable usb pll */
	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_FORCE_UART_EN | P2C_FORCE_SUSPENDM);

	mtk_phy_update_field(com + U3P_U2PHYDTM0, P2C_RG_XCVRSEL, 1);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_RG_DATAIN);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM1, P2C_RG_UART_EN);

	mtk_phy_set_bits(com + U3P_USBPHYACR0, PA0_RG_USB20_INTR_EN);

	/* disable switch 100uA current to SSUSB */
	mtk_phy_clear_bits(com + U3P_USBPHYACR5, PA5_RG_U2_HS_100U_U3_EN);

	mtk_phy_clear_bits(com + U3P_U2PHYACR4, P2C_U2_GPIO_CTR_MSK);

	if (tphy->pdata->avoid_rx_sen_degradation) {
		if (!index) {
			mtk_phy_set_bits(com + U3P_USBPHYACR2, PA2_RG_SIF_U2PLL_FORCE_EN);

			mtk_phy_clear_bits(com + U3D_U2PHYDCR0, P2C_RG_SIF_U2PLL_FORCE_ON);
		} else {
			mtk_phy_set_bits(com + U3D_U2PHYDCR0, P2C_RG_SIF_U2PLL_FORCE_ON);

			mtk_phy_set_bits(com + U3P_U2PHYDTM0,
					 P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM);
		}
	}

	/* DP/DM BC1.1 path Disable */
	mtk_phy_clear_bits(com + U3P_USBPHYACR6, PA6_RG_U2_BC11_SW_EN);

	mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_SQTH, 2);

	/* Workaround only for mt8195, HW fix it for others (V3) */
	u2_phy_pll_26m_set(tphy, instance);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;

	mtk_phy_set_bits(com + U3P_U2PHYDTM0, P2C_FORCE_SUSPENDM);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_RG_SUSPENDM);

	mtk_phy_set_bits(com + U3P_U2PHYDTM0, P2C_RG_SUSPENDM);

	udelay(30);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_FORCE_SUSPENDM);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_RG_SUSPENDM);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_FORCE_UART_EN);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM1, P2C_RG_UART_EN);

	mtk_phy_clear_bits(com + U3P_U2PHYACR4, P2C_U2_GPIO_CTR_MSK);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_FORCE_SUSPENDM);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0,
			   P2C_RG_XCVRSEL | P2C_RG_DATAIN | P2C_DTM0_PART_MASK);

	mtk_phy_clear_bits(com + U3P_USBPHYACR6, PA6_RG_U2_BC11_SW_EN);

	/* OTG Enable */
	mtk_phy_set_bits(com + U3P_USBPHYACR6, PA6_RG_U2_OTG_VBUSCMP_EN);

	mtk_phy_set_bits(com + U3P_U2PHYDTM1, P2C_RG_VBUSVALID | P2C_RG_AVALID);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM1, P2C_RG_SESSEND);

	mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV6, 1);

	udelay(800);

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		mtk_phy_set_bits(com + U3D_U2PHYDCR0, P2C_RG_SIF_U2PLL_FORCE_ON);

		mtk_phy_set_bits(com + U3P_U2PHYDTM0, P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM);
	}
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	if (instance->usb_special_phy_settings) {
		/* Used by phone products */
		/* HQA Setting */
		if (instance->discth)
			mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_DISCTH,
					instance->discth);
		else
			mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_DISCTH, 0xf);
	}
#endif
	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_FORCE_UART_EN);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM1, P2C_RG_UART_EN);

	mtk_phy_clear_bits(com + U3P_U2PHYACR4, P2C_U2_GPIO_CTR_MSK);

	mtk_phy_clear_bits(com + U3P_USBPHYACR6, PA6_RG_U2_BC11_SW_EN);

	/* OTG Disable */
	mtk_phy_clear_bits(com + U3P_USBPHYACR6, PA6_RG_U2_OTG_VBUSCMP_EN);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM1, P2C_RG_VBUSVALID | P2C_RG_AVALID);

	mtk_phy_set_bits(com + U3P_U2PHYDTM1, P2C_RG_SESSEND);

	mtk_phy_set_bits(com + U3P_U2PHYDTM0,  P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM);

	mdelay(2);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_RG_DATAIN);
	mtk_phy_set_bits(com + U3P_U2PHYDTM0, (P2C_RG_XCVRSEL_VAL(1) | P2C_DTM0_PART_MASK));

	mtk_phy_set_bits(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV6_VAL(1));

	udelay(800);

	mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_RG_SUSPENDM);

	udelay(1);

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM);

		mtk_phy_clear_bits(com + U3D_U2PHYDCR0, P2C_RG_SIF_U2PLL_FORCE_ON);
	}

	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_exit(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		mtk_phy_clear_bits(com + U3D_U2PHYDCR0, P2C_RG_SIF_U2PLL_FORCE_ON);

		mtk_phy_clear_bits(com + U3P_U2PHYDTM0, P2C_FORCE_SUSPENDM);
	}
}

static void u2_phy_instance_set_mode(struct mtk_tphy *tphy,
				     struct mtk_phy_instance *instance,
				     enum phy_mode mode,
				     int submode)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	u32 tmp;

	dev_info(tphy->dev, "%s mode(%d), submode(%d)\n", __func__,
		mode, submode);

	if (!submode) {
		tmp = readl(u2_banks->com + U3P_U2PHYDTM1);
		switch (mode) {
		case PHY_MODE_USB_DEVICE:
#if defined(CONFIG_W2_CHARGER_PRIVATE)
			instance->eye_vrt = 0x7;
			instance->eye_term = 0x5;
			instance->rev6 = 0x3;
#elif defined(CONFIG_N28_CHARGER_PRIVATE)
			instance->eye_vrt = 0x7;
			instance->eye_term = 0x3;
			instance->rev6 = 0x3;
			instance->discth = 0xf;
#endif
			dev_info(tphy->dev, "%s USB Device:eye_vrt(%x), eye_term(%x), eye_rev6(%x), eye_disc(%x)\n", __func__,
				instance->eye_vrt, instance->eye_term, instance->rev6, instance->discth);
			u2_phy_props_set(tphy, instance);
			tmp |= P2C_FORCE_IDDIG | P2C_RG_IDDIG;
			break;
		case PHY_MODE_USB_HOST:
#if defined(CONFIG_W2_CHARGER_PRIVATE)
			instance->eye_vrt_host = 0x7;
			instance->eye_term_host = 0x5;
			instance->rev6_host = 0x3;
			instance->discth_host = 0xf;
#elif defined(CONFIG_N28_CHARGER_PRIVATE)
			instance->eye_vrt_host = 0x7;
			instance->eye_term_host = 0x3;
			instance->rev6_host = 0x3;
			instance->discth_host = 0x9;
#endif
			dev_info(tphy->dev, "%s USB Host:eye_vrt(%x), eye_term(%x), eye_rev6(%x), eye_disc(%x)\n", __func__,
				instance->eye_vrt_host, instance->eye_term_host, instance->rev6_host, instance->discth_host);
			u2_phy_host_props_set(tphy, instance);
			tmp |= P2C_FORCE_IDDIG;
			tmp &= ~P2C_RG_IDDIG;
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
			if (instance->usb_special_phy_settings) {
				/* Used by phone products */
				tmp |= P2C_RG_VBUSVALID | P2C_RG_BVALID | P2C_RG_AVALID;
				tmp &= ~P2C_RG_SESSEND;
			}
#endif
			break;
		case PHY_MODE_USB_OTG:
			tmp &= ~(P2C_FORCE_IDDIG | P2C_RG_IDDIG);
			break;
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		case PHY_MODE_INVALID:
			if (instance->usb_special_phy_settings) {
				/* Used by phone products */
				tmp |= P2C_RG_SESSEND | P2C_RG_RG_IDPULLUP;
				tmp &= ~(P2C_RG_VBUSVALID | P2C_RG_BVALID | P2C_RG_AVALID |
					P2C_RG_IDDIG);
				tmp |= P2C_FORCE_IDDIG;
			} else
				return;
			break;
#endif

		default:
			return;
		}
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		if (instance->usb_special_phy_settings) {
		/* Used by phone products */
			tmp |= P2C_FORCE_VBUSVALID | P2C_FORCE_SESSEND | P2C_FORCE_BVALID |
				P2C_FORCE_AVALID | P2C_FORCE_IDPULLUP;
		}
#endif
		writel(tmp, u2_banks->com + U3P_U2PHYDTM1);
	} else {
		switch (submode) {
		case PHY_MODE_BC11_SW_SET:
			mtk_phy_set_bits(u2_banks->com + U3P_USBPHYACR6,
					 PA6_RG_U2_BC11_SW_EN);
			break;
		case PHY_MODE_BC11_SW_CLR:
			mtk_phy_clear_bits(u2_banks->com + U3P_USBPHYACR6,
					   PA6_RG_U2_BC11_SW_EN);
			break;
		case PHY_MODE_DPDMPULLDOWN_SET:
			mtk_phy_set_bits(u2_banks->com + U3P_U2PHYDTM0,
					 P2C_RG_DPPULLDOWN | P2C_RG_DMPULLDOWN);

			mtk_phy_clear_bits(u2_banks->com + U3P_USBPHYACR6,
					   PA6_RG_U2_PHY_REV1);

			mtk_phy_clear_bits(u2_banks->com + U3P_USBPHYACR6,
					   PA6_RG_U2_BC11_SW_EN);
			break;
		case PHY_MODE_DPDMPULLDOWN_CLR:
			mtk_phy_clear_bits(u2_banks->com + U3P_U2PHYDTM0,
					   (P2C_RG_DPPULLDOWN | P2C_RG_DMPULLDOWN));

			mtk_phy_set_bits(u2_banks->com + U3P_USBPHYACR6,
					 PA6_RG_U2_PHY_REV1);

			mtk_phy_set_bits(u2_banks->com + U3P_USBPHYACR6,
					 PA6_RG_U2_BC11_SW_EN);
			break;
		case PHY_MODE_DPPULLUP_SET:
			mtk_phy_set_bits(u2_banks->com + U3P_U2PHYACR3,
					  P2C_RG_USB20_PUPD_BIST_EN |
					  P2C_RG_USB20_EN_PU_DP);
			break;
		case PHY_MODE_DPPULLUP_CLR:
			mtk_phy_clear_bits(u2_banks->com + U3P_U2PHYACR3,
					   P2C_RG_USB20_PUPD_BIST_EN |
					   P2C_RG_USB20_EN_PU_DP);
			break;
		default:
			return;
		}
	}
}

static void u3_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 index = instance->index;

	mtk_phy_clear_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLD,
			   P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);

	mtk_phy_clear_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLE,
			   P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);

	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u3_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	enum phy_mode mode = instance->phy->attrs.mode;
	u32 index = instance->index;

	if (mode !=  PHY_MODE_USB_HOST) {
		mtk_phy_set_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLD,
			 P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);

		mtk_phy_set_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLE,
			 P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
	}

	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void pcie_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	void __iomem *phya = u3_banks->phya;

	if (tphy->pdata->version != MTK_PHY_V1)
		return;

	mtk_phy_update_bits(phya + U3P_U3_PHYA_DA_REG0,
			    P3A_RG_XTAL_EXT_PE1H | P3A_RG_XTAL_EXT_PE2H,
			    FIELD_PREP(P3A_RG_XTAL_EXT_PE1H, 0x2) |
			    FIELD_PREP(P3A_RG_XTAL_EXT_PE2H, 0x2));

	/* ref clk drive */
	mtk_phy_update_field(phya + U3P_U3_PHYA_REG1, P3A_RG_CLKDRV_AMP, 0x4);

	mtk_phy_update_field(phya + U3P_U3_PHYA_REG0, P3A_RG_CLKDRV_OFF, 0x1);

	/* SSC delta -5000ppm */
	mtk_phy_update_field(phya + U3P_U3_PHYA_DA_REG20, P3A_RG_PLL_DELTA1_PE2H, 0x3c);

	mtk_phy_update_field(phya + U3P_U3_PHYA_DA_REG25, P3A_RG_PLL_DELTA_PE2H, 0x36);

	/* change pll BW 0.6M */
	mtk_phy_update_bits(phya + U3P_U3_PHYA_DA_REG5,
			    P3A_RG_PLL_BR_PE2H | P3A_RG_PLL_IC_PE2H,
			    FIELD_PREP(P3A_RG_PLL_BR_PE2H, 0x1) |
			    FIELD_PREP(P3A_RG_PLL_IC_PE2H, 0x1));

	mtk_phy_update_bits(phya + U3P_U3_PHYA_DA_REG4,
			    P3A_RG_PLL_DIVEN_PE2H | P3A_RG_PLL_BC_PE2H,
			    FIELD_PREP(P3A_RG_PLL_BC_PE2H, 0x3));

	mtk_phy_update_field(phya + U3P_U3_PHYA_DA_REG6, P3A_RG_PLL_IR_PE2H, 0x2);

	mtk_phy_update_field(phya + U3P_U3_PHYA_DA_REG7, P3A_RG_PLL_BP_PE2H, 0xa);

	/* Tx Detect Rx Timing: 10us -> 5us */
	mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_RXDET1,
			     P3D_RG_RXDET_STB2_SET, 0x10);

	mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_RXDET2,
			     P3D_RG_RXDET_STB2_SET_P3, 0x10);

	/* wait for PCIe subsys register to active */
	usleep_range(2500, 3000);
	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void pcie_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;

	mtk_phy_clear_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLD,
			   P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);

	mtk_phy_clear_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLE,
			   P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
}

static void pcie_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)

{
	struct u3phy_banks *bank = &instance->u3_banks;

	mtk_phy_set_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLD,
			 P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);

	mtk_phy_set_bits(bank->chip + U3P_U3_CHIP_GPIO_CTLE,
			 P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
}

static void sata_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	void __iomem *phyd = u3_banks->phyd;

	/* charge current adjustment */
	mtk_phy_update_bits(phyd + ANA_RG_CTRL_SIGNAL6,
			    RG_CDR_BIRLTR_GEN1_MSK | RG_CDR_BC_GEN1_MSK,
			    FIELD_PREP(RG_CDR_BIRLTR_GEN1_MSK, 0x6) |
			    FIELD_PREP(RG_CDR_BC_GEN1_MSK, 0x1a));

	mtk_phy_update_field(phyd + ANA_EQ_EYE_CTRL_SIGNAL4, RG_CDR_BIRLTD0_GEN1_MSK, 0x18);

	mtk_phy_update_field(phyd + ANA_EQ_EYE_CTRL_SIGNAL5, RG_CDR_BIRLTD0_GEN3_MSK, 0x06);

	mtk_phy_update_bits(phyd + ANA_RG_CTRL_SIGNAL4,
			    RG_CDR_BICLTR_GEN1_MSK | RG_CDR_BR_GEN2_MSK,
			    FIELD_PREP(RG_CDR_BICLTR_GEN1_MSK, 0x0c) |
			    FIELD_PREP(RG_CDR_BR_GEN2_MSK, 0x07));

	mtk_phy_update_bits(phyd + PHYD_CTRL_SIGNAL_MODE4,
			    RG_CDR_BICLTD0_GEN1_MSK | RG_CDR_BICLTD1_GEN1_MSK,
			    FIELD_PREP(RG_CDR_BICLTD0_GEN1_MSK, 0x08) |
			    FIELD_PREP(RG_CDR_BICLTD1_GEN1_MSK, 0x02));

	mtk_phy_update_field(phyd + PHYD_DESIGN_OPTION2, RG_LOCK_CNT_SEL_MSK, 0x02);

	mtk_phy_update_bits(phyd + PHYD_DESIGN_OPTION9,
			    RG_T2_MIN_MSK | RG_TG_MIN_MSK,
			    FIELD_PREP(RG_T2_MIN_MSK, 0x12) |
			    FIELD_PREP(RG_TG_MIN_MSK, 0x04));

	mtk_phy_update_bits(phyd + PHYD_DESIGN_OPTION9,
			    RG_T2_MAX_MSK | RG_TG_MAX_MSK,
			    FIELD_PREP(RG_T2_MAX_MSK, 0x31) |
			    FIELD_PREP(RG_TG_MAX_MSK, 0x0e));

	mtk_phy_update_field(phyd + ANA_RG_CTRL_SIGNAL1, RG_IDRV_0DB_GEN1_MSK, 0x20);

	mtk_phy_update_field(phyd + ANA_EQ_EYE_CTRL_SIGNAL1, RG_EQ_DLEQ_LFI_GEN1_MSK, 0x03);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void phy_v1_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = NULL;
		u2_banks->fmreg = tphy->sif_base + SSUSB_SIFSLV_V1_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V1_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = tphy->sif_base + SSUSB_SIFSLV_V1_SPLLC;
		u3_banks->chip = tphy->sif_base + SSUSB_SIFSLV_V1_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V1_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V1_U3PHYA;
		break;
	case PHY_TYPE_SATA:
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V1_U3PHYD;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

static void phy_v2_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = instance->port_base + SSUSB_SIFSLV_V2_MISC;
		u2_banks->fmreg = instance->port_base + SSUSB_SIFSLV_V2_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V2_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = instance->port_base + SSUSB_SIFSLV_V2_SPLLC;
		u3_banks->chip = instance->port_base + SSUSB_SIFSLV_V2_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V2_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V2_U3PHYA;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

static void phy_parse_property(struct mtk_tphy *tphy,
				struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;

	if (instance->type != PHY_TYPE_USB2)
		return;

	instance->bc12_en = device_property_read_bool(dev, "mediatek,bc12");
	device_property_read_u32(dev, "mediatek,eye-src",
				 &instance->eye_src);
	device_property_read_u32(dev, "mediatek,eye-vrt",
				 &instance->eye_vrt);
	device_property_read_u32(dev, "mediatek,eye-term",
				 &instance->eye_term);
	device_property_read_u32(dev, "mediatek,intr",
				 &instance->intr);
	device_property_read_u32(dev, "mediatek,discth",
				 &instance->discth);
	device_property_read_u32(dev, "mediatek,pre-emphasis",
				 &instance->pre_emphasis);
	device_property_read_u32(dev, "mediatek,rx-sqth",
				 &instance->rx_sqth);
	device_property_read_u32(dev, "mediatek,rev4",
				 &instance->rev4);
	device_property_read_u32(dev, "mediatek,rev6",
				 &instance->rev6);
	device_property_read_u32(dev, "mediatek,eye-src-host",
				 &instance->eye_src_host);
	device_property_read_u32(dev, "mediatek,eye-vrt-host",
				 &instance->eye_vrt_host);
	device_property_read_u32(dev, "mediatek,eye-term-host",
				 &instance->eye_term_host);
	device_property_read_u32(dev, "mediatek,rev6-host",
				&instance->rev6_host);
	device_property_read_u32(dev, "mediatek,disc-host",
				&instance->discth_host);
	device_property_read_u32(dev, "mediatek,pll-bw",
				&instance->pll_bw);
	device_property_read_u32(dev, "mediatek,bgr-div",
				&instance->bgr_div);
	device_property_read_u32(dev, "mediatek,fsrxlvl",
				&instance->fsrxlvl);
	device_property_read_u32(dev, "mediatek,eq-leq-shift",
				&instance->eq_leq_shift);
	instance->usb_special_phy_settings = device_property_read_bool(dev,
				"mediatek,usb-special-phy-settings");
	dev_dbg(dev, "bc12:%d, src:%d, vrt:%d, term:%d, intr:%d, disc:%d\n",
		instance->bc12_en, instance->eye_src,
		instance->eye_vrt, instance->eye_term,
		instance->intr, instance->discth);
	dev_dbg(dev, "pre-emp:%d\n", instance->pre_emphasis);
	dev_dbg(dev, "rx_sqth:%d\n", instance->rx_sqth);
	dev_dbg(dev, "rev4:%d, rev6:%d\n", instance->rev4, instance->rev6);
	dev_dbg(dev, "pll-bw:%d, bgr-div:%d\n", instance->pll_bw, instance->bgr_div);
	dev_dbg(dev, "fsrxlvl:%d, eq-leq-shift:%d\n", instance->fsrxlvl, instance->eq_leq_shift);
	dev_dbg(dev, "usb-special-phy-settings:%d\n", instance->usb_special_phy_settings);
}

static void u3_phy_props_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	if (instance->eq_leq_shift) {
		mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_EQ_EYE3,
				P3D_RG_EQ_LEQ_SHIFT, instance->eq_leq_shift);
	}
}

static void u2_phy_props_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	struct device *dev = &instance->phy->dev;
	struct device_node *np = dev->parent->of_node;

	dev_info(dev, "bc12:%d, src:%d, vrt:%d, term:%d, intr:%d\n",
		instance->bc12_en, instance->eye_src,
		instance->eye_vrt, instance->eye_term, instance->intr);
	dev_info(dev, "disc:%d rx_sqth:%d\n",
		instance->discth, instance->rx_sqth);
	dev_info(dev, "rev4:%d, rev6:%d\n", instance->rev4, instance->rev6);
	dev_dbg(dev, "pll-bw:%d, bgr-div:%d\n", instance->pll_bw, instance->bgr_div);

	if (instance->bc12_en) /* BC1.2 path Enable */
		mtk_phy_set_bits(com + U3P_U2PHYBC12C, P2C_RG_CHGDT_EN);

	if (tphy->pdata->version < MTK_PHY_V3 && instance->eye_src)
		mtk_phy_update_field(com + U3P_USBPHYACR5, PA5_RG_U2_HSTX_SRCTRL,
				     instance->eye_src);

	if (instance->eye_vrt)
		mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_VRT_SEL,
				     instance->eye_vrt);

	if (instance->eye_term)
		mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_TERM_SEL,
				     instance->eye_term);

	if (instance->intr) {
		if (u2_banks->misc)
			mtk_phy_set_bits(u2_banks->misc + U3P_MISC_REG1,
					 MR1_EFUSE_AUTO_LOAD_DIS);

		mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_INTR_CAL,
				     instance->intr);
	}

	if (instance->discth)
		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_DISCTH,
				     instance->discth);

	if (instance->pre_emphasis)
		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PRE_EMP,
				     instance->pre_emphasis);

	if (instance->rev4)
		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV4,
				     instance->rev4);

	if (instance->rev6)
		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV6,
				     instance->rev6);

	if (instance->pll_bw)
		mtk_phy_update_field(com + U3P_USBPHYACR2, PA2_RG_USB20_PLL_BW,
				     instance->pll_bw);

	if (instance->bgr_div)
		mtk_phy_update_field(com + U3P_USBPHYACR0, PA0_RG_USB20_BGR_DIV,
				    instance->bgr_div);

	if (instance->fsrxlvl) {
		mtk_phy_update_field(com + U3P_USBPHYACR2, PA2_RG_U2_CLKREF_REV,
				instance->fsrxlvl);
	}

	if (instance->rx_sqth) {
		if(of_device_is_compatible(np, "mediatek,mt6781-tphy")) {
			/* for mt6781 only */
			/* USBPHY_CLR32(0x18, (0x1<<28)); */
			mtk_phy_clear_bits(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV4);
		}

		if(of_device_is_compatible(np, "mediatek,mt6781-tphy") ||
				of_device_is_compatible(np, "mediatek,mt6833-tphy")) {
			/* for mt6781 && mt6833 */
			/* USBPHY_CLR32(0x18, (0xf<<0)); */
			mtk_phy_clear_bits(com + U3P_USBPHYACR6, PA6_RG_U2_SQTH);
		}

		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_SQTH,
				     instance->rx_sqth);
	}

	if(of_device_is_compatible(np, "mediatek,mt6853-tphy")) {
		/* for mt6853 only */
		/* u3phywrite32(U3D_USBPHYACR2, 11, (0x3<<11), 0x3); */
		mtk_phy_update_field(com + U3P_USBPHYACR2,
				PA2_RG_U2_CLKREF_REV_1, 0x3);
	}
}

static void u2_phy_host_props_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;

	if (instance->eye_src_host)
		mtk_phy_update_field(com + U3P_USBPHYACR5, PA5_RG_U2_HSTX_SRCTRL,
				instance->eye_src_host);

	if (instance->eye_vrt_host)
		mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_VRT_SEL,
				instance->eye_vrt_host);

	if (instance->eye_term_host)
		mtk_phy_update_field(com + U3P_USBPHYACR1, PA1_RG_TERM_SEL,
				instance->eye_term_host);

	if (instance->rev6_host)
		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_PHY_REV6,
				instance->rev6_host);

	if (instance->discth_host)
		mtk_phy_update_field(com + U3P_USBPHYACR6, PA6_RG_U2_DISCTH,
				instance->discth_host);
}

/* type switch for usb3/pcie/sgmii/sata */
static int phy_type_syscon_get(struct mtk_phy_instance *instance,
			       struct device_node *dn)
{
	struct of_phandle_args args;
	int ret;

	/* type switch function is optional */
	if (!of_property_read_bool(dn, "mediatek,syscon-type"))
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn, "mediatek,syscon-type",
					       2, 0, &args);
	if (ret)
		return ret;

	instance->type_sw_reg = args.args[0];
	instance->type_sw_index = args.args[1] & 0x3; /* <=3 */
	instance->type_sw = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	dev_info(&instance->phy->dev, "type_sw - reg %#x, index %d\n",
		 instance->type_sw_reg, instance->type_sw_index);

	return PTR_ERR_OR_ZERO(instance->type_sw);
}

static int phy_type_set(struct mtk_phy_instance *instance)
{
	int type;
	u32 offset;

	if (!instance->type_sw)
		return 0;

	switch (instance->type) {
	case PHY_TYPE_USB3:
		type = RG_PHY_SW_USB3;
		break;
	case PHY_TYPE_PCIE:
		type = RG_PHY_SW_PCIE;
		break;
	case PHY_TYPE_SGMII:
		type = RG_PHY_SW_SGMII;
		break;
	case PHY_TYPE_SATA:
		type = RG_PHY_SW_SATA;
		break;
	case PHY_TYPE_USB2:
	default:
		return 0;
	}

	offset = instance->type_sw_index * BITS_PER_BYTE;
	regmap_update_bits(instance->type_sw, instance->type_sw_reg,
			   RG_PHY_SW_TYPE << offset, type << offset);

	return 0;
}

static int phy_efuse_get(struct mtk_tphy *tphy, struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	int ret = 0;

	instance->set_efuse_v1 = device_property_read_bool(dev,
				"mediatek,set-efuse-v1");
	dev_info(dev, "set-efuse-v1:%d\n", instance->set_efuse_v1);
	/* tphy v1 doesn't support sw efuse, skip it */
	if (!tphy->pdata->sw_efuse_supported || instance->set_efuse_v1) {
		instance->efuse_sw_en = 0;
		return 0;
	}

	/* software efuse is optional */
	instance->efuse_sw_en = device_property_read_bool(dev, "nvmem-cells");
	if (!instance->efuse_sw_en)
		return 0;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		ret = nvmem_cell_read_variable_le_u32(dev, "intr", &instance->efuse_intr);
		if (ret) {
			dev_err(dev, "fail to get u2 intr efuse, %d\n", ret);
			break;
		}

		/* no efuse, ignore it */
		if (!instance->efuse_intr) {
			dev_warn(dev, "no u2 intr efuse, but dts enable it\n");
			instance->efuse_sw_en = 0;
			break;
		}

		dev_dbg(dev, "u2 efuse - intr %x\n", instance->efuse_intr);
		break;

	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		ret = nvmem_cell_read_variable_le_u32(dev, "intr", &instance->efuse_intr);
		if (ret) {
			dev_err(dev, "fail to get u3 intr efuse, %d\n", ret);
			break;
		}

		ret = nvmem_cell_read_variable_le_u32(dev, "rx_imp", &instance->efuse_rx_imp);
		if (ret) {
			dev_err(dev, "fail to get u3 rx_imp efuse, %d\n", ret);
			break;
		}

		ret = nvmem_cell_read_variable_le_u32(dev, "tx_imp", &instance->efuse_tx_imp);
		if (ret) {
			dev_err(dev, "fail to get u3 tx_imp efuse, %d\n", ret);
			break;
		}

		/* no efuse, ignore it */
		if (!instance->efuse_intr &&
		    !instance->efuse_rx_imp &&
		    !instance->efuse_tx_imp) {
			dev_warn(dev, "no u3 intr efuse, but dts enable it\n");
			instance->efuse_sw_en = 0;
			break;
		}

		dev_dbg(dev, "u3 efuse - intr %x, rx_imp %x, tx_imp %x\n",
			instance->efuse_intr, instance->efuse_rx_imp,instance->efuse_tx_imp);
		break;
	default:
		dev_err(dev, "no sw efuse for type %d\n", instance->type);
		ret = -EINVAL;
	}

	return ret;
}

static int phy_efuse_set_v1(struct mtk_phy_instance *instance,
			     enum mtk_phy_efuse type)
{
	struct device *dev = &instance->phy->dev;
	struct device_node *np = dev->of_node;
	struct u2phy_banks *u2_banks;
	struct u3phy_banks *u3_banks;
	u32 val, mask;
	int index = 0, ret = 0;

	index = of_property_match_string(np,
			"nvmem-cell-names", efuse_name[type]);
	if (index < 0)
		return index;

	ret = of_property_read_u32_index(np, "nvmem-cell-masks",
			index, &mask);
	if (ret)
		return ret;

	ret = nvmem_cell_read_u32(dev, efuse_name[type], &val);
	if (ret)
		return ret;

	if (!val || !mask)
		return 0;

	val = (val & mask) >> (ffs(mask) - 1);
	dev_info(dev, "%s, %s=0x%x\n", __func__, efuse_name[type], val);

	switch (type) {
	case INTR_CAL:
		u2_banks = &instance->u2_banks;
		mtk_phy_update_field(u2_banks->com + U3P_USBPHYACR1,
				PA1_RG_INTR_CAL, val);
		break;
	case IEXT_INTR_CTRL:
		u3_banks = &instance->u3_banks;
		mtk_phy_update_field(u3_banks->phya + U3P_U3_PHYA_REG0,
				P3A_RG_IEXT_INTR, val);
		break;
	case RX_IMPSEL:
		u3_banks = &instance->u3_banks;
		mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_IMPCAL1,
				P3D_RG_RX_IMPEL, val);
		break;
	case TX_IMPSEL:
		u3_banks = &instance->u3_banks;
		mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_IMPCAL0,
				P3D_RG_TX_IMPEL, val);
		break;
	default:
		return 0;
	}

	return 0;
}

static void phy_efuse_set_(struct mtk_tphy *tphy,
		struct mtk_phy_instance *instance)
{
	switch (instance->type) {
	case PHY_TYPE_USB2:
		/** u2_phy_efuse_set */
		phy_efuse_set_v1(instance, INTR_CAL);
		break;
	case PHY_TYPE_USB3:
		/** u3_phy_efuse_set */
		phy_efuse_set_v1(instance, IEXT_INTR_CTRL);
		phy_efuse_set_v1(instance, RX_IMPSEL);
		phy_efuse_set_v1(instance, TX_IMPSEL);
		break;
	default:
		break;
	}
}

static void phy_efuse_set(struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	struct mtk_tphy *tphy = dev_get_drvdata(dev->parent);

	if ((tphy->pdata->version == MTK_PHY_V1 && !instance->efuse_sw_en) ||
			instance->set_efuse_v1) {
		phy_efuse_set_(tphy, instance);
		return;
	}

	if (!instance->efuse_sw_en)
		return;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		mtk_phy_set_bits(u2_banks->misc + U3P_MISC_REG1, MR1_EFUSE_AUTO_LOAD_DIS);

		mtk_phy_update_field(u2_banks->com + U3P_USBPHYACR1, PA1_RG_INTR_CAL,
				     instance->efuse_intr);
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		mtk_phy_set_bits(u3_banks->phyd + U3P_U3_PHYD_RSV, P3D_RG_EFUSE_AUTO_LOAD_DIS);

		mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_IMPCAL0, P3D_RG_TX_IMPEL,
				    instance->efuse_tx_imp);
		mtk_phy_set_bits(u3_banks->phyd + U3P_U3_PHYD_IMPCAL0, P3D_RG_FORCE_TX_IMPEL);

		mtk_phy_update_field(u3_banks->phyd + U3P_U3_PHYD_IMPCAL1, P3D_RG_RX_IMPEL,
				    instance->efuse_rx_imp);
		mtk_phy_set_bits(u3_banks->phyd + U3P_U3_PHYD_IMPCAL1, P3D_RG_FORCE_RX_IMPEL);

		mtk_phy_update_field(u3_banks->phya + U3P_U3_PHYA_REG0, P3A_RG_IEXT_INTR,
				    instance->efuse_intr);
		break;
	default:
		dev_warn(dev, "no sw efuse for type %d\n", instance->type);
		break;
	}
}

static int mtk_phy_init(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	ret = clk_bulk_prepare_enable(TPHY_CLKS_CNT, instance->clks);
	if (ret)
		return ret;

	phy_efuse_set(instance);

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(tphy, instance);
		u2_phy_props_set(tphy, instance);
		u2_phy_procfs_init(tphy, instance);
		break;
	case PHY_TYPE_USB3:
		u3_phy_instance_init(tphy, instance);
		u3_phy_procfs_init(tphy, instance);
		break;
	case PHY_TYPE_PCIE:
		pcie_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_SATA:
		sata_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_SGMII:
		/* nothing to do, only used to set type */
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		clk_bulk_disable_unprepare(TPHY_CLKS_CNT, instance->clks);
		return -EINVAL;
	}

	return 0;
}

static int mtk_phy_power_on(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		u2_phy_instance_power_on(tphy, instance);
		hs_slew_rate_calibrate(tphy, instance);
		u2_phy_props_set(tphy, instance);
	} else if (instance->type == PHY_TYPE_USB3) {
		u3_phy_instance_power_on(tphy, instance);
		u3_phy_props_set(tphy, instance);
	} else if (instance->type == PHY_TYPE_PCIE) {
		pcie_phy_instance_power_on(tphy, instance);
	}

	return 0;
}

static int mtk_phy_power_off(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		u2_phy_instance_power_off(tphy, instance);
	else if (instance->type == PHY_TYPE_USB3)
		u3_phy_instance_power_off(tphy, instance);
	else if (instance->type == PHY_TYPE_PCIE)
		pcie_phy_instance_power_off(tphy, instance);

	return 0;
}

static int mtk_phy_exit(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		u2_phy_instance_exit(tphy, instance);
		u2_phy_procfs_exit(instance);
	}
	if (instance->type == PHY_TYPE_USB3)
		u3_phy_procfs_exit(instance);

	clk_bulk_disable_unprepare(TPHY_CLKS_CNT, instance->clks);
	return 0;
}

static int mtk_phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		u2_phy_instance_set_mode(tphy, instance, mode, submode);

	return 0;
}

static bool mtk_phy_uart_mode(struct mtk_tphy *tphy)
{
	struct device_node *of_chosen;
	char *bootargs;
	bool uart_mode = false;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(of_chosen,
			"bootargs", NULL);

		if (bootargs && strstr(bootargs, PHY_MODE_UART))
			uart_mode = true;
	}

	return uart_mode;
}

static int mtk_phy_uart_init(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	if  (instance->type != PHY_TYPE_USB2)
		return 0;

	dev_info(tphy->dev, "uart init\n");

	ret = clk_bulk_prepare_enable(TPHY_CLKS_CNT, instance->clks);
	if (ret) {
		dev_info(tphy->dev, "failed to enable clock\n");
		return ret;
	}
	udelay(250);

	return 0;
}

static int mtk_phy_uart_exit(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if  (instance->type != PHY_TYPE_USB2)
		return 0;

	dev_info(tphy->dev, "uart exit\n");

	clk_bulk_disable_unprepare(TPHY_CLKS_CNT, instance->clks);
	return 0;
}

static bool mtk_phy_jtag_mode(struct mtk_tphy *tphy)
{
	struct device_node *of_chosen;
	char *bootargs;
	bool jtag_mode = false;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(of_chosen,
			"bootargs", NULL);

		if (bootargs && strstr(bootargs, PHY_MODE_JTAG))
			jtag_mode = true;
	}

	return jtag_mode;
}

static int mtk_phy_jtag_init(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);
	struct device *dev = &phy->dev;
	struct device_node *np = dev->of_node;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct of_phandle_args args;
	struct regmap *reg_base;
	u32 jtag_vers;
	u32 tmp;
	int ret;

	if  (instance->type != PHY_TYPE_USB2)
		return 0;

	dev_info(tphy->dev, "jtag init\n");

	ret = of_parse_phandle_with_fixed_args(np, "usb2jtag", 1, 0, &args);
	if (ret)
		return ret;

	jtag_vers = args.args[0];
	reg_base = syscon_node_to_regmap(args.np);
	of_node_put(args.np);

	dev_info(tphy->dev, "version:%d\n", jtag_vers);

	ret = clk_bulk_prepare_enable(TPHY_CLKS_CNT, instance->clks);
	if (ret) {
		dev_err(tphy->dev, "failed to enable clock\n");
		return ret;
	}
	tmp = readl(u2_banks->com + 0x20);
	tmp |= 0xf300;
	writel(tmp, u2_banks->com + 0x20);

	tmp = readl(u2_banks->com  + 0x18);
	tmp &= 0xff7ffff;
	writel(tmp, u2_banks->com  + 0x18);

	tmp = readl(u2_banks->com);
	tmp |= 0x1;
	writel(tmp, u2_banks->com);

	tmp = readl(u2_banks->com  + 0x08);
	tmp &= 0xfffdffff;
	writel(tmp, u2_banks->com  + 0x08);

	udelay(100);

	switch (jtag_vers) {
	case MTK_PHY_JTAG_V1:
		regmap_read(reg_base, 0xf00, &tmp);
		tmp |= 0x4030;
		regmap_write(reg_base, 0xf00, tmp);
		break;
	case MTK_PHY_JTAG_V2:
		regmap_read(reg_base, 0x100, &tmp);
		tmp |= 0x2;
		regmap_write(reg_base, 0x100, tmp);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_phy_jtag_exit(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if  (instance->type != PHY_TYPE_USB2)
		return 0;

	dev_info(tphy->dev, "jtag exit\n");

	clk_bulk_disable_unprepare(TPHY_CLKS_CNT, instance->clks);
	return 0;
}

static struct phy *mtk_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct mtk_tphy *tphy = dev_get_drvdata(dev);
	struct mtk_phy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;
	int ret;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < tphy->nphys; index++)
		if (phy_np == tphy->phys[index]->phy->dev.of_node) {
			instance = tphy->phys[index];
			break;
		}

	if (!instance) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	instance->type = args->args[0];
	if (!(instance->type == PHY_TYPE_USB2 ||
	      instance->type == PHY_TYPE_USB3 ||
	      instance->type == PHY_TYPE_PCIE ||
	      instance->type == PHY_TYPE_SATA ||
	      instance->type == PHY_TYPE_SGMII)) {
		dev_err(dev, "unsupported device type: %d\n", instance->type);
		return ERR_PTR(-EINVAL);
	}

	switch (tphy->pdata->version) {
	case MTK_PHY_V1:
		phy_v1_banks_init(tphy, instance);
		break;
	case MTK_PHY_V2:
	case MTK_PHY_V3:
		phy_v2_banks_init(tphy, instance);
		break;
	default:
		dev_err(dev, "phy version is not supported\n");
		return ERR_PTR(-EINVAL);
	}

	ret = phy_efuse_get(tphy, instance);
	if (ret)
		return ERR_PTR(ret);

	phy_parse_property(tphy, instance);
	phy_type_set(instance);

	return instance->phy;
}

static const struct phy_ops mtk_phy_uart_ops = {
	.init		= mtk_phy_uart_init,
	.exit		= mtk_phy_uart_exit,
	.owner		= THIS_MODULE,
};

static const struct phy_ops mtk_phy_jtag_ops = {
	.init		= mtk_phy_jtag_init,
	.exit		= mtk_phy_jtag_exit,
	.owner		= THIS_MODULE,
};

static const struct phy_ops mtk_tphy_ops = {
	.init		= mtk_phy_init,
	.exit		= mtk_phy_exit,
	.power_on	= mtk_phy_power_on,
	.power_off	= mtk_phy_power_off,
	.set_mode	= mtk_phy_set_mode,
	.owner		= THIS_MODULE,
};

static const struct mtk_phy_pdata tphy_v1_pdata = {
	.avoid_rx_sen_degradation = false,
	.version = MTK_PHY_V1,
};

static const struct mtk_phy_pdata tphy_v2_pdata = {
	.avoid_rx_sen_degradation = false,
	.sw_efuse_supported = true,
	.version = MTK_PHY_V2,
};

static const struct mtk_phy_pdata tphy_v3_pdata = {
	.sw_efuse_supported = true,
	.version = MTK_PHY_V3,
};

static const struct mtk_phy_pdata mt8173_pdata = {
	.avoid_rx_sen_degradation = true,
	.version = MTK_PHY_V1,
};

static const struct mtk_phy_pdata mt8195_pdata = {
	.sw_pll_48m_to_26m = true,
	.sw_efuse_supported = true,
	.version = MTK_PHY_V3,
};

static const struct of_device_id mtk_tphy_id_table[] = {
	{ .compatible = "mediatek,mt2701-u3phy", .data = &tphy_v1_pdata },
	{ .compatible = "mediatek,mt2712-u3phy", .data = &tphy_v2_pdata },
	{ .compatible = "mediatek,mt8173-u3phy", .data = &mt8173_pdata },
	{ .compatible = "mediatek,mt8195-tphy", .data = &mt8195_pdata },
	{ .compatible = "mediatek,generic-tphy-v1", .data = &tphy_v1_pdata },
	{ .compatible = "mediatek,generic-tphy-v2", .data = &tphy_v2_pdata },
	{ .compatible = "mediatek,generic-tphy-v3", .data = &tphy_v3_pdata },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_tphy_id_table);

static int mtk_tphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct resource *sif_res;
	struct mtk_tphy *tphy;
	struct resource res;
	int port, retval;

	tphy = devm_kzalloc(dev, sizeof(*tphy), GFP_KERNEL);
	if (!tphy)
		return -ENOMEM;

	tphy->pdata = of_device_get_match_data(dev);
	if (!tphy->pdata)
		return -EINVAL;

	tphy->nphys = of_get_child_count(np);
	tphy->phys = devm_kcalloc(dev, tphy->nphys,
				       sizeof(*tphy->phys), GFP_KERNEL);
	if (!tphy->phys)
		return -ENOMEM;

	tphy->dev = dev;
	platform_set_drvdata(pdev, tphy);

	sif_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* SATA phy of V1 needn't it if not shared with PCIe or USB */
	if (sif_res && tphy->pdata->version == MTK_PHY_V1) {
		/* get banks shared by multiple phys */
		tphy->sif_base = devm_ioremap_resource(dev, sif_res);
		if (IS_ERR(tphy->sif_base)) {
			dev_err(dev, "failed to remap sif regs\n");
			return PTR_ERR(tphy->sif_base);
		}
	}

	if (tphy->pdata->version < MTK_PHY_V3) {
		tphy->src_ref_clk = U3P_REF_CLK;
		tphy->src_coef = U3P_SLEW_RATE_COEF;
		/* update parameters of slew rate calibrate if exist */
		device_property_read_u32(dev, "mediatek,src-ref-clk-mhz",
					 &tphy->src_ref_clk);
		device_property_read_u32(dev, "mediatek,src-coef",
					 &tphy->src_coef);
	}

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct mtk_phy_instance *instance;
		struct clk_bulk_data *clks;
		struct device *subdev;
		struct phy *phy;

		instance = devm_kzalloc(dev, sizeof(*instance), GFP_KERNEL);
		if (!instance) {
			retval = -ENOMEM;
			goto put_child;
		}

		tphy->phys[port] = instance;

		phy = devm_phy_create(dev, child_np, &mtk_tphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			retval = PTR_ERR(phy);
			goto put_child;
		}

		subdev = &phy->dev;
		retval = of_address_to_resource(child_np, 0, &res);
		if (retval) {
			dev_err(subdev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		instance->port_base = devm_ioremap_resource(subdev, &res);
		if (IS_ERR(instance->port_base)) {
			retval = PTR_ERR(instance->port_base);
			goto put_child;
		}

		/* Get optional property ippc address */
		retval = of_address_to_resource(child_np, 1, &res);
		if (retval) {
			dev_info(dev, "failed to get ippc resource(id-%d)\n",
				port);
		} else {
			instance->ippc_base = devm_ioremap(dev, res.start,
				resource_size(&res));
			if (IS_ERR(instance->ippc_base))
				dev_info(dev, "failed to remap ippc regs\n");
		}

		instance->phy = phy;
		instance->index = port;
		phy_set_drvdata(phy, instance);
		port++;

		clks = instance->clks;
		clks[0].id = "ref";     /* digital (& analog) clock */
		clks[1].id = "da_ref";  /* analog clock */
		retval = devm_clk_bulk_get_optional(subdev, TPHY_CLKS_CNT, clks);
		if (retval)
			goto put_child;

		retval = phy_type_syscon_get(instance, child_np);
		if (retval)
			goto put_child;

		/* change ops to usb uart or jtage mode */
		if (mtk_phy_uart_mode(tphy))
			phy->ops = &mtk_phy_uart_ops;
		else if (mtk_phy_jtag_mode(tphy))
			phy->ops = &mtk_phy_jtag_ops;
	}
	mtk_phy_procfs_init(tphy);

	provider = devm_of_phy_provider_register(dev, mtk_phy_xlate);

	return PTR_ERR_OR_ZERO(provider);
put_child:
	of_node_put(child_np);
	return retval;
}

static int mtk_tphy_remove(struct platform_device *pdev)
{
	struct mtk_tphy *tphy = dev_get_drvdata(&pdev->dev);

	mtk_phy_procfs_exit(tphy);
	return 0;
}

static struct platform_driver mtk_tphy_driver = {
	.probe		= mtk_tphy_probe,
	.remove		= mtk_tphy_remove,
	.driver		= {
		.name	= "mtk-tphy",
		.of_match_table = mtk_tphy_id_table,
	},
};

module_platform_driver(mtk_tphy_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_DESCRIPTION("MediaTek T-PHY driver");
MODULE_LICENSE("GPL v2");
