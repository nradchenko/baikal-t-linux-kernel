/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare XPCS helpers
 *
 * Author: Jose Abreu <Jose.Abreu@synopsys.com>
 */

#include <linux/bits.h>
#include <linux/pcs/pcs-xpcs.h>

/* Vendor regs access */
#define DW_VENDOR			BIT(15)

/* VR_XS_PMA */
#define DW_VR_XS_PMA_GEN5_10G_MPLL_CTRL	0x007a
#define DW_VR_XS_PMA_REF_CLK_SEL_CORE	BIT(13)
#define DW_VR_XS_PMA_GEN5_10G_GEN_CTRL	0x009c
#define DW_VR_XS_PMA_LANE_MODE		GENMASK(3, 0)
#define DW_VR_XS_PMA_LANE_MODE_KX	0x3
#define DW_VR_XS_PMA_LANE_MODE_KX4	0x4
#define DW_VR_XS_PMA_LANE_MODE_KR	0x5
#define DW_VR_XS_PMA_LANE_MODE_SGMII	0x6
#define DW_VR_XS_PMA_LANE_MODE_RXAUI	0x8
#define DW_VR_XS_PMA_LINK_WIDTH		GENMASK(10, 8)
#define DW_VR_XS_PMA_LINK_WIDTH_1	0x0
#define DW_VR_XS_PMA_LINK_WIDTH_2	0x1
#define DW_VR_XS_PMA_LINK_WIDTH_4	0x2
#define DW_VR_XS_PMA_LANE_PWR_OFF	GENMASK(15, 12)

/* VR_XS_PCS */
#define DW_USXGMII_RST			BIT(10)
#define DW_USXGMII_EN			BIT(9)
#define DW_VR_XS_PCS_XAUI_MODE_CTRL	0x0004
#define DW_VR_XS_PCS_RXAUI_MODE		BIT(0)
#define DW_VR_XS_PCS_MRVL_RXAUI		BIT(1)
#define DW_VR_XS_PCS_DIG_STS		0x0010
#define DW_PSEQ_STATE			GENMASK(4, 2)
#define DW_PSEQ_TXRX_STABLE		0x100
#define DW_RXFIFO_ERR			GENMASK(6, 5)

/* SR_MII */
#define DW_USXGMII_FULL			BIT(8)
#define DW_USXGMII_SS_MASK		(BIT(13) | BIT(6) | BIT(5))
#define DW_USXGMII_10000		(BIT(13) | BIT(6))
#define DW_USXGMII_5000			(BIT(13) | BIT(5))
#define DW_USXGMII_2500			(BIT(5))
#define DW_USXGMII_1000			(BIT(6))
#define DW_USXGMII_100			(BIT(13))
#define DW_USXGMII_10			(0)

/* SR_AN */
#define DW_SR_AN_ADV1			0x10
#define DW_SR_AN_ADV2			0x11
#define DW_SR_AN_ADV3			0x12

/* Clause 73 Defines */
/* AN_LP_ABL1 */
#define DW_C73_PAUSE			BIT(10)
#define DW_C73_ASYM_PAUSE		BIT(11)
#define DW_C73_AN_ADV_SF		0x1
/* AN_LP_ABL2 */
#define DW_C73_1000KX			BIT(5)
#define DW_C73_10000KX4			BIT(6)
#define DW_C73_10000KR			BIT(7)
/* AN_LP_ABL3 */
#define DW_C73_2500KX			BIT(0)
#define DW_C73_5000KR			BIT(1)

/* VR_CTRL_MMD */
#define DW_SR_CTRL_MMD_CTRL		0x0009
#define DW_SR_CTRL_AN_MMD_EN		BIT(0)
#define DW_SR_CTRL_PCS_XS_MMD_EN	BIT(1)
#define DW_SR_CTRL_MII_MMD_EN		BIT(2)
#define DW_SR_CTRL_PMA_MMD_EN		BIT(3)

/* Clause 37 Defines */
/* VR MII MMD registers offsets */
#define DW_VR_MII_MMD_CTRL		0x0000
#define DW_VR_MII_DIG_CTRL1		0x8000
#define DW_VR_MII_AN_CTRL		0x8001
#define DW_VR_MII_AN_INTR_STS		0x8002
/* Enable 2.5G Mode */
#define DW_VR_MII_DIG_CTRL1_2G5_EN	BIT(2)
/* EEE Mode Control Register */
#define DW_VR_MII_EEE_MCTRL0		0x8006
#define DW_VR_MII_EEE_MCTRL1		0x800b
#define DW_VR_MII_DIG_CTRL2		0x80e1

/* VR_MII_DIG_CTRL1 */
#define DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW		BIT(9)

/* VR_MII_DIG_CTRL2 */
#define DW_VR_MII_DIG_CTRL2_TX_POL_INV		BIT(4)
#define DW_VR_MII_DIG_CTRL2_RX_POL_INV		BIT(0)

/* VR_MII_AN_CTRL */
#define DW_VR_MII_AN_CTRL_TX_CONFIG_SHIFT	3
#define DW_VR_MII_TX_CONFIG_MASK		BIT(3)
#define DW_VR_MII_TX_CONFIG_PHY_SIDE_SGMII	0x1
#define DW_VR_MII_TX_CONFIG_MAC_SIDE_SGMII	0x0
#define DW_VR_MII_AN_CTRL_PCS_MODE_SHIFT	1
#define DW_VR_MII_PCS_MODE_MASK			GENMASK(2, 1)
#define DW_VR_MII_PCS_MODE_C37_1000BASEX	0x0
#define DW_VR_MII_PCS_MODE_C37_SGMII		0x2

/* VR_MII_AN_INTR_STS */
#define DW_VR_MII_AN_STS_C37_ANSGM_FD		BIT(1)
#define DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT	2
#define DW_VR_MII_AN_STS_C37_ANSGM_SP		GENMASK(3, 2)
#define DW_VR_MII_C37_ANSGM_SP_10		0x0
#define DW_VR_MII_C37_ANSGM_SP_100		0x1
#define DW_VR_MII_C37_ANSGM_SP_1000		0x2
#define DW_VR_MII_C37_ANSGM_SP_LNKSTS		BIT(4)

/* SR MII MMD Control defines */
#define AN_CL37_EN			BIT(12)	/* Enable Clause 37 auto-nego */
#define SGMII_SPEED_SS13		BIT(13)	/* SGMII speed along with SS6 */
#define SGMII_SPEED_SS6			BIT(6)	/* SGMII speed along with SS13 */

/* VR MII EEE Control 0 defines */
#define DW_VR_MII_EEE_LTX_EN			BIT(0)  /* LPI Tx Enable */
#define DW_VR_MII_EEE_LRX_EN			BIT(1)  /* LPI Rx Enable */
#define DW_VR_MII_EEE_TX_QUIET_EN		BIT(2)  /* Tx Quiet Enable */
#define DW_VR_MII_EEE_RX_QUIET_EN		BIT(3)  /* Rx Quiet Enable */
#define DW_VR_MII_EEE_TX_EN_CTRL		BIT(4)  /* Tx Control Enable */
#define DW_VR_MII_EEE_RX_EN_CTRL		BIT(7)  /* Rx Control Enable */

#define DW_VR_MII_EEE_MULT_FACT_100NS_SHIFT	8
#define DW_VR_MII_EEE_MULT_FACT_100NS		GENMASK(11, 8)

/* VR MII EEE Control 1 defines */
#define DW_VR_MII_EEE_TRN_LPI		BIT(0)	/* Transparent Mode Enable */

#define DW_XPCS_INFO_DECLARE(_name, _did, _pma)			\
	static const struct dw_xpcs_info _name = { .did = _did, .pma = _pma }

int xpcs_read(struct dw_xpcs *xpcs, int dev, u32 reg);
int xpcs_write(struct dw_xpcs *xpcs, int dev, u32 reg, u16 val);
int xpcs_modify(struct dw_xpcs *xpcs, int dev, u32 reg, u16 mask, u16 set);
int xpcs_modify_changed(struct dw_xpcs *xpcs, int dev, u32 reg, u16 mask, u16 set);
int xpcs_read_vendor(struct dw_xpcs *xpcs, int dev, u32 reg);
int xpcs_write_vendor(struct dw_xpcs *xpcs, int dev, int reg, u16 val);
int xpcs_poll_val(struct dw_xpcs *xpcs, int dev, int reg, u16 mask, u16 val);
int xpcs_soft_reset(struct dw_xpcs *xpcs);
int xpcs_vendor_reset(struct dw_xpcs *xpcs);

int xpcs_10gbaser_pma_config(struct dw_xpcs *xpcs);
int xpcs_10gbasex_pma_config(struct dw_xpcs *xpcs);
int nxp_sja1105_sgmii_pma_config(struct dw_xpcs *xpcs);
int nxp_sja1110_sgmii_pma_config(struct dw_xpcs *xpcs);
int nxp_sja1110_2500basex_pma_config(struct dw_xpcs *xpcs);
