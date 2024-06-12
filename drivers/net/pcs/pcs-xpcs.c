// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare XPCS helpers
 *
 * Author: Jose Abreu <Jose.Abreu@synopsys.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fwnode.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/pcs/pcs-xpcs.h>
#include <linux/phy.h>
#include <linux/phylink.h>
#include <linux/property.h>

#include "pcs-xpcs.h"

#define phylink_pcs_to_xpcs(pl_pcs) \
	container_of((pl_pcs), struct dw_xpcs, pcs)

static const int xpcs_xgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_usxgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_10gkr_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_10gbaser_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseCR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseLR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseER_Full_BIT,
	/* Speed-compatible modes for the link setup in the external PHYs */
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	/* ETHTOOL_LINK_MODE_10000baseCX4_Full_BIT, */
	/* ETHTOOL_LINK_MODE_10000baseLX4_Full_BIT, */
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_10gbasex_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	/* ETHTOOL_LINK_MODE_10000baseCX4_Full_BIT, */
	/* ETHTOOL_LINK_MODE_10000baseLX4_Full_BIT, */
	/* Speed-compatible modes for the link setup in the external PHYs */
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseCR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseLR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseER_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_xlgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_25000baseCR_Full_BIT,
	ETHTOOL_LINK_MODE_25000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_25000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT,
	ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseCR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT,
	ETHTOOL_LINK_MODE_50000baseDR_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT,
	ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_sgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_10baseT_Half_BIT,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT_Half_BIT,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_1000basex_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_2500basex_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const phy_interface_t xpcs_xgmii_interfaces[] = {
	PHY_INTERFACE_MODE_XGMII,
};

static const phy_interface_t xpcs_usxgmii_interfaces[] = {
	PHY_INTERFACE_MODE_USXGMII,
};

static const phy_interface_t xpcs_10gkr_interfaces[] = {
	PHY_INTERFACE_MODE_10GKR,
};

static const phy_interface_t xpcs_10gbaser_interfaces[] = {
	PHY_INTERFACE_MODE_10GBASER,
};

static const phy_interface_t xpcs_10gbasex_interfaces[] = {
	PHY_INTERFACE_MODE_10GBASEX,
	PHY_INTERFACE_MODE_XAUI,
};

static const phy_interface_t xpcs_xlgmii_interfaces[] = {
	PHY_INTERFACE_MODE_XLGMII,
};

static const phy_interface_t xpcs_sgmii_interfaces[] = {
	PHY_INTERFACE_MODE_SGMII,
};

static const phy_interface_t xpcs_1000basex_interfaces[] = {
	PHY_INTERFACE_MODE_1000BASEX,
};

static const phy_interface_t xpcs_2500basex_interfaces[] = {
	PHY_INTERFACE_MODE_2500BASEX,
};

enum {
	DW_XPCS_XGMII,
	DW_XPCS_USXGMII,
	DW_XPCS_10GKR,
	DW_XPCS_10GBASER,
	DW_XPCS_10GBASEX,
	DW_XPCS_XLGMII,
	DW_XPCS_SGMII,
	DW_XPCS_1000BASEX,
	DW_XPCS_2500BASEX,
	DW_XPCS_INTERFACE_MAX,
};

struct xpcs_compat {
	const int *supported;
	const phy_interface_t *interface;
	int num_interfaces;
	int an_mode;
	int (*pma_config)(struct dw_xpcs *xpcs);
};

struct xpcs_id {
	u32 id;
	u32 mask;
	const struct xpcs_compat *compat;
};

static const struct xpcs_compat *xpcs_find_compat(const struct xpcs_id *id,
						  phy_interface_t interface)
{
	int i, j;

	for (i = 0; i < DW_XPCS_INTERFACE_MAX; i++) {
		const struct xpcs_compat *compat = &id->compat[i];

		for (j = 0; j < compat->num_interfaces; j++)
			if (compat->interface[j] == interface)
				return compat;
	}

	return NULL;
}

int xpcs_get_an_mode(struct dw_xpcs *xpcs, phy_interface_t interface)
{
	const struct xpcs_compat *compat;

	compat = xpcs_find_compat(xpcs->id, interface);
	if (!compat)
		return -ENODEV;

	return compat->an_mode;
}
EXPORT_SYMBOL_GPL(xpcs_get_an_mode);

static bool __xpcs_linkmode_supported(const struct xpcs_compat *compat,
				      enum ethtool_link_mode_bit_indices linkmode)
{
	int i;

	for (i = 0; compat->supported[i] != __ETHTOOL_LINK_MODE_MASK_NBITS; i++)
		if (compat->supported[i] == linkmode)
			return true;

	return false;
}

#define xpcs_linkmode_supported(compat, mode) \
	__xpcs_linkmode_supported(compat, ETHTOOL_LINK_MODE_ ## mode ## _BIT)

int xpcs_read(struct dw_xpcs *xpcs, int dev, u32 reg)
{
	struct mii_bus *bus = xpcs->mdiodev->bus;
	int addr = xpcs->mdiodev->addr;

	return mdiobus_c45_read(bus, addr, dev, reg);
}

int xpcs_write(struct dw_xpcs *xpcs, int dev, u32 reg, u16 val)
{
	struct mii_bus *bus = xpcs->mdiodev->bus;
	int addr = xpcs->mdiodev->addr;

	return mdiobus_c45_write(bus, addr, dev, reg, val);
}

int xpcs_modify(struct dw_xpcs *xpcs, int dev, u32 reg, u16 mask, u16 set)
{
	u32 reg_addr = mdiobus_c45_addr(dev, reg);

	return mdiodev_modify(xpcs->mdiodev, reg_addr, mask, set);
}

int xpcs_modify_changed(struct dw_xpcs *xpcs, int dev, u32 reg, u16 mask, u16 set)
{
	u32 reg_addr = mdiobus_c45_addr(dev, reg);

	return mdiodev_modify_changed(xpcs->mdiodev, reg_addr, mask, set);
}

int xpcs_read_vendor(struct dw_xpcs *xpcs, int dev, u32 reg)
{
	return xpcs_read(xpcs, dev, DW_VENDOR | reg);
}

int xpcs_write_vendor(struct dw_xpcs *xpcs, int dev, int reg, u16 val)
{
	return xpcs_write(xpcs, dev, DW_VENDOR | reg, val);
}

static int xpcs_read_vpcs(struct dw_xpcs *xpcs, int reg)
{
	return xpcs_read_vendor(xpcs, MDIO_MMD_PCS, reg);
}

static int xpcs_write_vpcs(struct dw_xpcs *xpcs, int reg, u16 val)
{
	return xpcs_write_vendor(xpcs, MDIO_MMD_PCS, reg, val);
}

int xpcs_poll_val(struct dw_xpcs *xpcs, int dev, int reg, u16 mask, u16 val)
{
	/* Poll until value is detected (50ms per retry == 0.6 sec) */
	unsigned int retries = 12;
	int ret;

	do {
		msleep(50);
		ret = xpcs_read(xpcs, dev, reg);
		if (ret < 0)
			return ret;
	} while ((ret & mask) != val && --retries);

	return ((ret & mask) != val) ? -ETIMEDOUT : 0;
}

static int __xpcs_soft_reset(struct dw_xpcs *xpcs, bool vendor)
{
	int ret, dev, reg;

	if (xpcs->mmd_ctrl & DW_SR_CTRL_MII_MMD_EN)
		dev = MDIO_MMD_VEND2;
	else if (xpcs->mmd_ctrl & DW_SR_CTRL_PCS_XS_MMD_EN)
		dev = MDIO_MMD_PCS;
	else if (xpcs->mmd_ctrl & DW_SR_CTRL_PMA_MMD_EN)
		dev = MDIO_MMD_PMAPMD;
	else
		return -EINVAL;

	reg = MDIO_CTRL1;
	if (vendor)
		reg |= DW_VENDOR;

	ret = xpcs_modify(xpcs, dev, reg, MDIO_CTRL1_RESET, MDIO_CTRL1_RESET);
	if (ret < 0)
		return ret;

	return xpcs_poll_val(xpcs, dev, reg, MDIO_CTRL1_RESET, 0);
}

int xpcs_soft_reset(struct dw_xpcs *xpcs)
{
	return __xpcs_soft_reset(xpcs, false);
}

int xpcs_vendor_reset(struct dw_xpcs *xpcs)
{
	return __xpcs_soft_reset(xpcs, true);
}

#define xpcs_warn(__xpcs, __state, __args...) \
({ \
	if ((__state)->link) \
		dev_warn(&(__xpcs)->mdiodev->dev, ##__args); \
})

static int xpcs_read_fault_c73(struct dw_xpcs *xpcs,
			       struct phylink_link_state *state,
			       u16 pcs_stat1)
{
	int ret;

	if (pcs_stat1 & MDIO_STAT1_FAULT) {
		xpcs_warn(xpcs, state, "Link fault condition detected!\n");
		return -EFAULT;
	}

	ret = xpcs_read(xpcs, MDIO_MMD_PCS, MDIO_STAT2);
	if (ret < 0)
		return ret;

	if (ret & MDIO_STAT2_RXFAULT)
		xpcs_warn(xpcs, state, "Receiver fault detected!\n");
	if (ret & MDIO_STAT2_TXFAULT)
		xpcs_warn(xpcs, state, "Transmitter fault detected!\n");

	ret = xpcs_read_vendor(xpcs, MDIO_MMD_PCS, DW_VR_XS_PCS_DIG_STS);
	if (ret < 0)
		return ret;

	if (ret & DW_RXFIFO_ERR) {
		xpcs_warn(xpcs, state, "FIFO fault condition detected!\n");
		return -EFAULT;
	}

	ret = xpcs_read(xpcs, MDIO_MMD_PCS, MDIO_PCS_10GBRT_STAT1);
	if (ret < 0)
		return ret;

	if (!(ret & MDIO_PCS_10GBRT_STAT1_BLKLK))
		xpcs_warn(xpcs, state, "Link is not locked!\n");

	ret = xpcs_read(xpcs, MDIO_MMD_PCS, MDIO_PCS_10GBRT_STAT2);
	if (ret < 0)
		return ret;

	if (ret & MDIO_PCS_10GBRT_STAT2_ERR) {
		xpcs_warn(xpcs, state, "Link has errors!\n");
		return -EFAULT;
	}

	return 0;
}

static void xpcs_config_xgmii(struct dw_xpcs *xpcs, int speed)
{
	/* TODO add some XGMII-related configs ...*/
}

static void xpcs_config_usxgmii(struct dw_xpcs *xpcs, int speed)
{
	int ret, speed_sel;

	switch (speed) {
	case SPEED_10:
		speed_sel = DW_USXGMII_10;
		break;
	case SPEED_100:
		speed_sel = DW_USXGMII_100;
		break;
	case SPEED_1000:
		speed_sel = DW_USXGMII_1000;
		break;
	case SPEED_2500:
		speed_sel = DW_USXGMII_2500;
		break;
	case SPEED_5000:
		speed_sel = DW_USXGMII_5000;
		break;
	case SPEED_10000:
		speed_sel = DW_USXGMII_10000;
		break;
	default:
		/* Nothing to do here */
		return;
	}

	ret = xpcs_read_vpcs(xpcs, MDIO_CTRL1);
	if (ret < 0)
		goto out;

	ret = xpcs_write_vpcs(xpcs, MDIO_CTRL1, ret | DW_USXGMII_EN);
	if (ret < 0)
		goto out;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1);
	if (ret < 0)
		goto out;

	ret &= ~DW_USXGMII_SS_MASK;
	ret |= speed_sel | DW_USXGMII_FULL;

	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1, ret);
	if (ret < 0)
		goto out;

	ret = xpcs_read_vpcs(xpcs, MDIO_CTRL1);
	if (ret < 0)
		goto out;

	ret = xpcs_write_vpcs(xpcs, MDIO_CTRL1, ret | DW_USXGMII_RST);
	if (ret < 0)
		goto out;

	return;

out:
	pr_err("%s: XPCS access returned %pe\n", __func__, ERR_PTR(ret));
}

static int _xpcs_config_aneg_c73(struct dw_xpcs *xpcs,
				 const struct xpcs_compat *compat)
{
	int ret, adv;

	/* By default, in USXGMII mode XPCS operates at 10G baud and
	 * replicates data to achieve lower speeds. Hereby, in this
	 * default configuration we need to advertise all supported
	 * modes and not only the ones we want to use.
	 */

	/* SR_AN_ADV3 */
	adv = 0;
	if (xpcs_linkmode_supported(compat, 2500baseX_Full))
		adv |= DW_C73_2500KX;

	/* TODO: 5000baseKR */

	ret = xpcs_write(xpcs, MDIO_MMD_AN, DW_SR_AN_ADV3, adv);
	if (ret < 0)
		return ret;

	/* SR_AN_ADV2 */
	adv = 0;
	if (xpcs_linkmode_supported(compat, 1000baseKX_Full))
		adv |= DW_C73_1000KX;
	if (xpcs_linkmode_supported(compat, 10000baseKX4_Full))
		adv |= DW_C73_10000KX4;
	if (xpcs_linkmode_supported(compat, 10000baseKR_Full))
		adv |= DW_C73_10000KR;

	ret = xpcs_write(xpcs, MDIO_MMD_AN, DW_SR_AN_ADV2, adv);
	if (ret < 0)
		return ret;

	/* SR_AN_ADV1 */
	adv = DW_C73_AN_ADV_SF;
	if (xpcs_linkmode_supported(compat, Pause))
		adv |= DW_C73_PAUSE;
	if (xpcs_linkmode_supported(compat, Asym_Pause))
		adv |= DW_C73_ASYM_PAUSE;

	return xpcs_write(xpcs, MDIO_MMD_AN, DW_SR_AN_ADV1, adv);
}

static int xpcs_config_aneg_c73(struct dw_xpcs *xpcs,
				const struct xpcs_compat *compat)
{
	int ret;

	ret = _xpcs_config_aneg_c73(xpcs, compat);
	if (ret < 0)
		return ret;

	ret = xpcs_read(xpcs, MDIO_MMD_AN, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret |= MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART;

	return xpcs_write(xpcs, MDIO_MMD_AN, MDIO_CTRL1, ret);
}

static int xpcs_aneg_done_c73(struct dw_xpcs *xpcs,
			      struct phylink_link_state *state,
			      const struct xpcs_compat *compat, u16 an_stat1)
{
	int ret;

	if (an_stat1 & MDIO_AN_STAT1_COMPLETE) {
		ret = xpcs_read(xpcs, MDIO_MMD_AN, MDIO_AN_LPA);
		if (ret < 0)
			return ret;

		/* Check if Aneg outcome is valid */
		if (!(ret & DW_C73_AN_ADV_SF)) {
			xpcs_config_aneg_c73(xpcs, compat);
			return 0;
		}

		return 1;
	}

	return 0;
}

static int xpcs_read_lpa_c73(struct dw_xpcs *xpcs,
			     struct phylink_link_state *state, u16 an_stat1)
{
	u16 lpa[3];
	int i, ret;

	if (!(an_stat1 & MDIO_AN_STAT1_LPABLE)) {
		phylink_clear(state->lp_advertising, Autoneg);
		return 0;
	}

	phylink_set(state->lp_advertising, Autoneg);

	/* Read Clause 73 link partner advertisement */
	for (i = ARRAY_SIZE(lpa); --i >= 0; ) {
		ret = xpcs_read(xpcs, MDIO_MMD_AN, MDIO_AN_LPA + i);
		if (ret < 0)
			return ret;

		lpa[i] = ret;
	}

	mii_c73_mod_linkmode(state->lp_advertising, lpa);

	return 0;
}

static int xpcs_get_max_xlgmii_speed(struct dw_xpcs *xpcs,
				     struct phylink_link_state *state)
{
	unsigned long *adv = state->advertising;
	int speed = SPEED_UNKNOWN;
	int bit;

	for_each_set_bit(bit, adv, __ETHTOOL_LINK_MODE_MASK_NBITS) {
		int new_speed = SPEED_UNKNOWN;

		switch (bit) {
		case ETHTOOL_LINK_MODE_25000baseCR_Full_BIT:
		case ETHTOOL_LINK_MODE_25000baseKR_Full_BIT:
		case ETHTOOL_LINK_MODE_25000baseSR_Full_BIT:
			new_speed = SPEED_25000;
			break;
		case ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT:
		case ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT:
		case ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT:
		case ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT:
			new_speed = SPEED_40000;
			break;
		case ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseKR_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseSR_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseCR_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT:
		case ETHTOOL_LINK_MODE_50000baseDR_Full_BIT:
			new_speed = SPEED_50000;
			break;
		case ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT:
		case ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT:
			new_speed = SPEED_100000;
			break;
		default:
			continue;
		}

		if (new_speed > speed)
			speed = new_speed;
	}

	return speed;
}

static void xpcs_resolve_pma(struct dw_xpcs *xpcs,
			     struct phylink_link_state *state)
{
	state->pause = MLO_PAUSE_TX | MLO_PAUSE_RX;
	state->duplex = DUPLEX_FULL;

	switch (state->interface) {
	case PHY_INTERFACE_MODE_XLGMII:
		state->speed = xpcs_get_max_xlgmii_speed(xpcs, state);
		break;
	default:
		state->speed = SPEED_UNKNOWN;
		break;
	}
}

static int xpcs_validate(struct phylink_pcs *pcs, unsigned long *supported,
			 const struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(xpcs_supported) = { 0, };
	const struct xpcs_compat *compat;
	struct dw_xpcs *xpcs;
	int i;

	xpcs = phylink_pcs_to_xpcs(pcs);
	compat = xpcs_find_compat(xpcs->id, state->interface);
	if (!compat)
		return -EINVAL;

	/* Populate the supported link modes for this PHY interface type.
	 * FIXME: what about the port modes and autoneg bit? This masks
	 * all those away.
	 */
	for (i = 0; compat->supported[i] != __ETHTOOL_LINK_MODE_MASK_NBITS; i++)
		set_bit(compat->supported[i], xpcs_supported);

	linkmode_and(supported, supported, xpcs_supported);

	return 0;
}

void xpcs_get_interfaces(struct dw_xpcs *xpcs, unsigned long *interfaces)
{
	int i, j;

	for (i = 0; i < DW_XPCS_INTERFACE_MAX; i++) {
		const struct xpcs_compat *compat = &xpcs->id->compat[i];

		for (j = 0; j < compat->num_interfaces; j++)
			__set_bit(compat->interface[j], interfaces);
	}
}
EXPORT_SYMBOL_GPL(xpcs_get_interfaces);

int xpcs_config_eee(struct dw_xpcs *xpcs, int mult_fact_100ns, int enable)
{
	int ret;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_EEE_MCTRL0);
	if (ret < 0)
		return ret;

	if (enable) {
	/* Enable EEE */
		ret = DW_VR_MII_EEE_LTX_EN | DW_VR_MII_EEE_LRX_EN |
		      DW_VR_MII_EEE_TX_QUIET_EN | DW_VR_MII_EEE_RX_QUIET_EN |
		      DW_VR_MII_EEE_TX_EN_CTRL | DW_VR_MII_EEE_RX_EN_CTRL |
		      mult_fact_100ns << DW_VR_MII_EEE_MULT_FACT_100NS_SHIFT;
	} else {
		ret &= ~(DW_VR_MII_EEE_LTX_EN | DW_VR_MII_EEE_LRX_EN |
		       DW_VR_MII_EEE_TX_QUIET_EN | DW_VR_MII_EEE_RX_QUIET_EN |
		       DW_VR_MII_EEE_TX_EN_CTRL | DW_VR_MII_EEE_RX_EN_CTRL |
		       DW_VR_MII_EEE_MULT_FACT_100NS);
	}

	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_EEE_MCTRL0, ret);
	if (ret < 0)
		return ret;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_EEE_MCTRL1);
	if (ret < 0)
		return ret;

	if (enable)
		ret |= DW_VR_MII_EEE_TRN_LPI;
	else
		ret &= ~DW_VR_MII_EEE_TRN_LPI;

	return xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_EEE_MCTRL1, ret);
}
EXPORT_SYMBOL_GPL(xpcs_config_eee);

static int xpcs_config_aneg_c37_sgmii(struct dw_xpcs *xpcs, unsigned int mode)
{
	int ret, mdio_ctrl;

	/* For AN for C37 SGMII mode, the settings are :-
	 * 1) VR_MII_MMD_CTRL Bit(12) [AN_ENABLE] = 0b (Disable SGMII AN in case
	      it is already enabled)
	 * 2) VR_MII_AN_CTRL Bit(2:1)[PCS_MODE] = 10b (SGMII AN)
	 * 3) VR_MII_AN_CTRL Bit(3) [TX_CONFIG] = 0b (MAC side SGMII)
	 *    DW xPCS used with DW EQoS MAC is always MAC side SGMII.
	 * 4) VR_MII_DIG_CTRL1 Bit(9) [MAC_AUTO_SW] = 1b (Automatic
	 *    speed/duplex mode change by HW after SGMII AN complete)
	 * 5) VR_MII_MMD_CTRL Bit(12) [AN_ENABLE] = 1b (Enable SGMII AN)
	 *
	 * Note: Since it is MAC side SGMII, there is no need to set
	 *	 SR_MII_AN_ADV. MAC side SGMII receives AN Tx Config from
	 *	 PHY about the link state change after C28 AN is completed
	 *	 between PHY and Link Partner. There is also no need to
	 *	 trigger AN restart for MAC-side SGMII.
	 */
	mdio_ctrl = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL);
	if (mdio_ctrl < 0)
		return mdio_ctrl;

	if (mdio_ctrl & AN_CL37_EN) {
		ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL,
				 mdio_ctrl & ~AN_CL37_EN);
		if (ret < 0)
			return ret;
	}

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~(DW_VR_MII_PCS_MODE_MASK | DW_VR_MII_TX_CONFIG_MASK);
	ret |= (DW_VR_MII_PCS_MODE_C37_SGMII <<
		DW_VR_MII_AN_CTRL_PCS_MODE_SHIFT &
		DW_VR_MII_PCS_MODE_MASK);
	ret |= (DW_VR_MII_TX_CONFIG_MAC_SIDE_SGMII <<
		DW_VR_MII_AN_CTRL_TX_CONFIG_SHIFT &
		DW_VR_MII_TX_CONFIG_MASK);
	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_CTRL, ret);
	if (ret < 0)
		return ret;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_DIG_CTRL1);
	if (ret < 0)
		return ret;

	if (phylink_autoneg_inband(mode))
		ret |= DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW;
	else
		ret &= ~DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW;

	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_DIG_CTRL1, ret);
	if (ret < 0)
		return ret;

	if (phylink_autoneg_inband(mode))
		ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL,
				 mdio_ctrl | AN_CL37_EN);

	return ret;
}

static int xpcs_config_aneg_c37_1000basex(struct dw_xpcs *xpcs, unsigned int mode,
					  const unsigned long *advertising)
{
	phy_interface_t interface = PHY_INTERFACE_MODE_1000BASEX;
	int ret, mdio_ctrl, adv;
	bool changed = 0;

	/* According to Chap 7.12, to set 1000BASE-X C37 AN, AN must
	 * be disabled first:-
	 * 1) VR_MII_MMD_CTRL Bit(12)[AN_ENABLE] = 0b
	 * 2) VR_MII_AN_CTRL Bit(2:1)[PCS_MODE] = 00b (1000BASE-X C37)
	 */
	mdio_ctrl = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL);
	if (mdio_ctrl < 0)
		return mdio_ctrl;

	if (mdio_ctrl & AN_CL37_EN) {
		ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL,
				 mdio_ctrl & ~AN_CL37_EN);
		if (ret < 0)
			return ret;
	}

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~DW_VR_MII_PCS_MODE_MASK;
	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_CTRL, ret);
	if (ret < 0)
		return ret;

	/* Check for advertising changes and update the C45 MII ADV
	 * register accordingly.
	 */
	adv = phylink_mii_c22_pcs_encode_advertisement(interface,
						       advertising);
	if (adv >= 0) {
		ret = xpcs_modify_changed(xpcs, MDIO_MMD_VEND2,
					  MII_ADVERTISE, 0xffff, adv);
		if (ret < 0)
			return ret;

		changed = ret;
	}

	/* Clear CL37 AN complete status */
	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_INTR_STS, 0);
	if (ret < 0)
		return ret;

	if (phylink_autoneg_inband(mode) &&
	    linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, advertising)) {
		ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL,
				 mdio_ctrl | AN_CL37_EN);
		if (ret < 0)
			return ret;
	}

	return changed;
}

static int xpcs_config_10gbaser(struct dw_xpcs *xpcs)
{
	int ret;

	/* Disable Clause 73 AN in anyway */
	ret = xpcs_modify(xpcs, MDIO_MMD_AN, MDIO_CTRL1,
			  MDIO_AN_CTRL1_ENABLE, 0);
	if (ret < 0)
		return ret;

	/* Disable RXAUI if it's enabled by default */
	ret = xpcs_read_vpcs(xpcs, DW_VR_XS_PCS_XAUI_MODE_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~DW_VR_XS_PCS_RXAUI_MODE;
	ret = xpcs_write_vpcs(xpcs, DW_VR_XS_PCS_XAUI_MODE_CTRL, ret);
	if (ret < 0)
		return ret;

	/* Set PCS to 10G speed and 10GBASE-R PCS */
	ret = xpcs_modify(xpcs, MDIO_MMD_PCS, MDIO_CTRL1,
			  MDIO_CTRL1_SPEEDSEL, MDIO_CTRL1_SPEED10G);
	if (ret < 0)
		return ret;

	ret = xpcs_modify(xpcs, MDIO_MMD_PCS, MDIO_CTRL2,
			  MDIO_PCS_CTRL2_TYPE, MDIO_PCS_CTRL2_10GBR);
	if (ret < 0)
		return ret;

	/* Make sure the specified PCS type is perceived by the core */
	return xpcs_vendor_reset(xpcs);
}

static int xpcs_config_10gbasex(struct dw_xpcs *xpcs)
{
	int ret;

	/* Disable Clause 73 AN in anyway */
	ret = xpcs_modify(xpcs, MDIO_MMD_AN, MDIO_CTRL1,
			  MDIO_AN_CTRL1_ENABLE, 0);
	if (ret < 0)
		return ret;

	/* Disable RXAUI if it's enabled by default */
	ret = xpcs_read_vpcs(xpcs, DW_VR_XS_PCS_XAUI_MODE_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~DW_VR_XS_PCS_RXAUI_MODE;
	ret = xpcs_write_vpcs(xpcs, DW_VR_XS_PCS_XAUI_MODE_CTRL, ret);
	if (ret < 0)
		return ret;

	/* Set PCS to 10G speed and 10GBASE-X PCS */
	ret = xpcs_modify(xpcs, MDIO_MMD_PCS, MDIO_CTRL1,
			  MDIO_CTRL1_SPEEDSEL, MDIO_CTRL1_SPEED10G);
	if (ret < 0)
		return ret;

	ret = xpcs_modify(xpcs, MDIO_MMD_PCS, MDIO_CTRL2,
			  MDIO_PCS_CTRL2_TYPE, MDIO_PCS_CTRL2_10GBX);
	if (ret < 0)
		return ret;

	/* Make sure the specified PCS type is perceived by the core */
	return xpcs_vendor_reset(xpcs);
}

static int xpcs_config_2500basex(struct dw_xpcs *xpcs)
{
	int ret;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_DIG_CTRL1);
	if (ret < 0)
		return ret;
	ret |= DW_VR_MII_DIG_CTRL1_2G5_EN;
	ret &= ~DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW;
	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_DIG_CTRL1, ret);
	if (ret < 0)
		return ret;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL);
	if (ret < 0)
		return ret;
	ret &= ~AN_CL37_EN;
	ret |= SGMII_SPEED_SS6;
	ret &= ~SGMII_SPEED_SS13;
	return xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_MMD_CTRL, ret);
}

int xpcs_do_config(struct dw_xpcs *xpcs, phy_interface_t interface,
		   unsigned int mode, const unsigned long *advertising)
{
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(xpcs->id, interface);
	if (!compat)
		return -ENODEV;

	switch (compat->an_mode) {
	case DW_AN_C73:
		if (test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, advertising)) {
			ret = xpcs_config_aneg_c73(xpcs, compat);
			if (ret)
				return ret;
		}
		break;
	case DW_AN_C37_SGMII:
		ret = xpcs_config_aneg_c37_sgmii(xpcs, mode);
		if (ret)
			return ret;
		break;
	case DW_AN_C37_1000BASEX:
		ret = xpcs_config_aneg_c37_1000basex(xpcs, mode,
						     advertising);
		if (ret)
			return ret;
		break;
	case DW_10GBASER:
		ret = xpcs_config_10gbaser(xpcs);
		if (ret)
			return ret;
		break;
	case DW_10GBASEX:
		ret = xpcs_config_10gbasex(xpcs);
		if (ret)
			return ret;
		break;
	case DW_2500BASEX:
		ret = xpcs_config_2500basex(xpcs);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	if (compat->pma_config) {
		ret = compat->pma_config(xpcs);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(xpcs_do_config);

static int xpcs_config(struct phylink_pcs *pcs, unsigned int mode,
		       phy_interface_t interface,
		       const unsigned long *advertising,
		       bool permit_pause_to_mac)
{
	struct dw_xpcs *xpcs = phylink_pcs_to_xpcs(pcs);

	return xpcs_do_config(xpcs, interface, mode, advertising);
}

static int xpcs_get_state_c73(struct dw_xpcs *xpcs,
			      struct phylink_link_state *state,
			      const struct xpcs_compat *compat)
{
	int pcs_stat1;
	int an_stat1;
	int ret;

	/* The link status bit is latching-low, so it is important to
	 * avoid unnecessary re-reads of this register to avoid missing
	 * a link-down event.
	 */
	pcs_stat1 = xpcs_read(xpcs, MDIO_MMD_PCS, MDIO_STAT1);
	if (pcs_stat1 < 0) {
		state->link = false;
		return pcs_stat1;
	}

	/* Link needs to be read first ... */
	state->link = !!(pcs_stat1 & MDIO_STAT1_LSTATUS);

	/* ... and then we check the faults. */
	ret = xpcs_read_fault_c73(xpcs, state, pcs_stat1);
	if (ret) {
		ret = xpcs_soft_reset(xpcs);
		if (ret)
			return ret;

		state->link = 0;

		return xpcs_do_config(xpcs, state->interface, MLO_AN_INBAND, NULL);
	}

	/* There is no point doing anything else if the link is down. */
	if (!state->link)
		return 0;

	if (state->an_enabled) {
		/* The link status bit is latching-low, so it is important to
		 * avoid unnecessary re-reads of this register to avoid missing
		 * a link-down event.
		 */
		an_stat1 = xpcs_read(xpcs, MDIO_MMD_AN, MDIO_STAT1);
		if (an_stat1 < 0) {
			state->link = false;
			return an_stat1;
		}

		state->an_complete = xpcs_aneg_done_c73(xpcs, state, compat,
							an_stat1);
		if (!state->an_complete) {
			state->link = false;
			return 0;
		}

		ret = xpcs_read_lpa_c73(xpcs, state, an_stat1);
		if (ret < 0) {
			state->link = false;
			return ret;
		}

		phylink_resolve_c73(state);
	} else {
		xpcs_resolve_pma(xpcs, state);
	}

	return 0;
}

static int xpcs_get_state_c37_sgmii(struct dw_xpcs *xpcs,
				    struct phylink_link_state *state)
{
	int ret;

	/* Reset link_state */
	state->link = false;
	state->speed = SPEED_UNKNOWN;
	state->duplex = DUPLEX_UNKNOWN;
	state->pause = 0;

	/* For C37 SGMII mode, we check DW_VR_MII_AN_INTR_STS for link
	 * status, speed and duplex.
	 */
	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_INTR_STS);
	if (ret < 0)
		return ret;

	if (ret & DW_VR_MII_C37_ANSGM_SP_LNKSTS) {
		int speed_value;

		state->link = true;

		speed_value = (ret & DW_VR_MII_AN_STS_C37_ANSGM_SP) >>
			      DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT;
		if (speed_value == DW_VR_MII_C37_ANSGM_SP_1000)
			state->speed = SPEED_1000;
		else if (speed_value == DW_VR_MII_C37_ANSGM_SP_100)
			state->speed = SPEED_100;
		else
			state->speed = SPEED_10;

		if (ret & DW_VR_MII_AN_STS_C37_ANSGM_FD)
			state->duplex = DUPLEX_FULL;
		else
			state->duplex = DUPLEX_HALF;
	}

	return 0;
}

static int xpcs_get_state_c37_1000basex(struct dw_xpcs *xpcs,
					struct phylink_link_state *state)
{
	int lpa, bmsr;

	if (state->an_enabled) {
		/* Reset link state */
		state->link = false;

		lpa = xpcs_read(xpcs, MDIO_MMD_VEND2, MII_LPA);
		if (lpa < 0 || lpa & LPA_RFAULT)
			return lpa;

		bmsr = xpcs_read(xpcs, MDIO_MMD_VEND2, MII_BMSR);
		if (bmsr < 0)
			return bmsr;

		phylink_mii_c22_pcs_decode_state(state, bmsr, lpa);
	}

	return 0;
}

static void xpcs_get_state(struct phylink_pcs *pcs,
			   struct phylink_link_state *state)
{
	struct dw_xpcs *xpcs = phylink_pcs_to_xpcs(pcs);
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(xpcs->id, state->interface);
	if (!compat)
		return;

	switch (compat->an_mode) {
	case DW_10GBASER:
		phylink_mii_c45_pcs_get_state(xpcs->mdiodev, state);
		break;
	case DW_AN_C73:
		ret = xpcs_get_state_c73(xpcs, state, compat);
		if (ret) {
			pr_err("xpcs_get_state_c73 returned %pe\n",
			       ERR_PTR(ret));
			return;
		}
		break;
	case DW_AN_C37_SGMII:
		ret = xpcs_get_state_c37_sgmii(xpcs, state);
		if (ret) {
			pr_err("xpcs_get_state_c37_sgmii returned %pe\n",
			       ERR_PTR(ret));
		}
		break;
	case DW_AN_C37_1000BASEX:
		ret = xpcs_get_state_c37_1000basex(xpcs, state);
		if (ret) {
			pr_err("xpcs_get_state_c37_1000basex returned %pe\n",
			       ERR_PTR(ret));
		}
		break;
	default:
		return;
	}
}

static void xpcs_link_up_check(struct dw_xpcs *xpcs, int speed, int duplex)
{
	int ret;

	ret = xpcs_poll_val(xpcs, MDIO_MMD_PCS, MDIO_STAT1,
			    MDIO_STAT1_LSTATUS, MDIO_STAT1_LSTATUS);
	if (ret < 0) {
		dev_err(&xpcs->mdiodev->dev, "link is malfunction\n");
		return;
	}

	if (speed != SPEED_10000)
		dev_warn(&xpcs->mdiodev->dev, "Incompatible speed\n");

	if (duplex != DUPLEX_FULL)
		dev_warn(&xpcs->mdiodev->dev, "Incompatible duplex\n");

	ret = xpcs_read(xpcs, MDIO_MMD_PCS, MDIO_STAT2);
	if (ret < 0) {
		dev_err(&xpcs->mdiodev->dev, "Failed to read PCS status\n");
		return;
	}

	if (ret & MDIO_STAT2_RXFAULT)
		dev_dbg(&xpcs->mdiodev->dev, "Receiver fault detected\n");

	if (ret & MDIO_STAT2_TXFAULT)
		dev_dbg(&xpcs->mdiodev->dev, "Transmitter fault detected\n");
}

static void xpcs_link_up_sgmii(struct dw_xpcs *xpcs, unsigned int mode,
			       int speed, int duplex)
{
	int val, ret;

	if (phylink_autoneg_inband(mode))
		return;

	val = mii_bmcr_encode_fixed(speed, duplex);
	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1, val);
	if (ret)
		pr_err("%s: xpcs_write returned %pe\n", __func__, ERR_PTR(ret));
}

static void xpcs_link_up_1000basex(struct dw_xpcs *xpcs, unsigned int mode,
				   int speed, int duplex)
{
	int val, ret;

	if (phylink_autoneg_inband(mode))
		return;

	switch (speed) {
	case SPEED_1000:
		val = BMCR_SPEED1000;
		break;
	case SPEED_100:
	case SPEED_10:
	default:
		pr_err("%s: speed = %d\n", __func__, speed);
		return;
	}

	if (duplex == DUPLEX_FULL)
		val |= BMCR_FULLDPLX;
	else
		pr_err("%s: half duplex not supported\n", __func__);

	ret = xpcs_write(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1, val);
	if (ret)
		pr_err("%s: xpcs_write returned %pe\n", __func__, ERR_PTR(ret));
}

void xpcs_link_up(struct phylink_pcs *pcs, unsigned int mode,
		  phy_interface_t interface, int speed, int duplex)
{
	struct dw_xpcs *xpcs = phylink_pcs_to_xpcs(pcs);

	if (interface == PHY_INTERFACE_MODE_XGMII)
		return xpcs_config_xgmii(xpcs, speed);
	if (interface == PHY_INTERFACE_MODE_USXGMII)
		return xpcs_config_usxgmii(xpcs, speed);
	if (interface == PHY_INTERFACE_MODE_10GBASER)
		return xpcs_link_up_check(xpcs, speed, duplex);
	if (interface == PHY_INTERFACE_MODE_10GBASEX)
		return xpcs_link_up_check(xpcs, speed, duplex);
	if (interface == PHY_INTERFACE_MODE_SGMII)
		return xpcs_link_up_sgmii(xpcs, mode, speed, duplex);
	if (interface == PHY_INTERFACE_MODE_1000BASEX)
		return xpcs_link_up_1000basex(xpcs, mode, speed, duplex);
}
EXPORT_SYMBOL_GPL(xpcs_link_up);

static void xpcs_an_restart(struct phylink_pcs *pcs)
{
	struct dw_xpcs *xpcs = phylink_pcs_to_xpcs(pcs);
	int ret;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1);
	if (ret >= 0) {
		ret |= BMCR_ANRESTART;
		xpcs_write(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1, ret);
	}
}

static u32 xpcs_get_id(struct dw_xpcs *xpcs)
{
	int ret;
	u32 id;

	/* First, search C73 PCS using PCS MMD */
	ret = xpcs_read(xpcs, MDIO_MMD_PCS, MII_PHYSID1);
	if (ret < 0)
		return 0xffffffff;

	id = ret << 16;

	ret = xpcs_read(xpcs, MDIO_MMD_PCS, MII_PHYSID2);
	if (ret < 0)
		return 0xffffffff;

	/* If Device IDs are not all zeros or all ones,
	 * we found C73 AN-type device
	 */
	if ((id | ret) && (id | ret) != 0xffffffff)
		return id | ret;

	/* Next, search C37 PCS using Vendor-Specific MII MMD */
	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, MII_PHYSID1);
	if (ret < 0)
		return 0xffffffff;

	id = ret << 16;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND2, MII_PHYSID2);
	if (ret < 0)
		return 0xffffffff;

	/* If Device IDs are not all zeros, we found C37 AN-type device */
	if (id | ret)
		return id | ret;

	return 0xffffffff;
}

static const struct xpcs_compat synopsys_xpcs_compat[DW_XPCS_INTERFACE_MAX] = {
	[DW_XPCS_XGMII] = {
		.supported = xpcs_xgmii_features,
		.interface = xpcs_xgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_xgmii_interfaces),
		.an_mode = DW_AN_C73,
	},
	[DW_XPCS_USXGMII] = {
		.supported = xpcs_usxgmii_features,
		.interface = xpcs_usxgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_usxgmii_interfaces),
		.an_mode = DW_AN_C73,
	},
	[DW_XPCS_10GKR] = {
		.supported = xpcs_10gkr_features,
		.interface = xpcs_10gkr_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gkr_interfaces),
		.an_mode = DW_AN_C73,
	},
	[DW_XPCS_10GBASER] = {
		.supported = xpcs_10gbaser_features,
		.interface = xpcs_10gbaser_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gbaser_interfaces),
		.an_mode = DW_10GBASER,
		.pma_config = xpcs_10gbaser_pma_config,
	},
	[DW_XPCS_10GBASEX] = {
		.supported = xpcs_10gbasex_features,
		.interface = xpcs_10gbasex_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gbasex_interfaces),
		.an_mode = DW_10GBASEX,
		.pma_config = xpcs_10gbasex_pma_config,
	},
	[DW_XPCS_XLGMII] = {
		.supported = xpcs_xlgmii_features,
		.interface = xpcs_xlgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_xlgmii_interfaces),
		.an_mode = DW_AN_C73,
	},
	[DW_XPCS_SGMII] = {
		.supported = xpcs_sgmii_features,
		.interface = xpcs_sgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_sgmii_interfaces),
		.an_mode = DW_AN_C37_SGMII,
	},
	[DW_XPCS_1000BASEX] = {
		.supported = xpcs_1000basex_features,
		.interface = xpcs_1000basex_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_1000basex_interfaces),
		.an_mode = DW_AN_C37_1000BASEX,
	},
	[DW_XPCS_2500BASEX] = {
		.supported = xpcs_2500basex_features,
		.interface = xpcs_2500basex_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_2500basex_features),
		.an_mode = DW_2500BASEX,
	},
};

static const struct xpcs_compat nxp_sja1105_xpcs_compat[DW_XPCS_INTERFACE_MAX] = {
	[DW_XPCS_SGMII] = {
		.supported = xpcs_sgmii_features,
		.interface = xpcs_sgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_sgmii_interfaces),
		.an_mode = DW_AN_C37_SGMII,
		.pma_config = nxp_sja1105_sgmii_pma_config,
	},
};

static const struct xpcs_compat nxp_sja1110_xpcs_compat[DW_XPCS_INTERFACE_MAX] = {
	[DW_XPCS_SGMII] = {
		.supported = xpcs_sgmii_features,
		.interface = xpcs_sgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_sgmii_interfaces),
		.an_mode = DW_AN_C37_SGMII,
		.pma_config = nxp_sja1110_sgmii_pma_config,
	},
	[DW_XPCS_2500BASEX] = {
		.supported = xpcs_2500basex_features,
		.interface = xpcs_2500basex_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_2500basex_interfaces),
		.an_mode = DW_2500BASEX,
		.pma_config = nxp_sja1110_2500basex_pma_config,
	},
};

static const struct xpcs_compat bt1_xpcs_compat[DW_XPCS_INTERFACE_MAX] = {
	[DW_XPCS_XGMII] = {
		.supported = xpcs_xgmii_features,
		.interface = xpcs_xgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_xgmii_interfaces),
		.an_mode = DW_AN_C73,
	},
	[DW_XPCS_10GBASER] = {
		.supported = xpcs_10gbaser_features,
		.interface = xpcs_10gbaser_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gbaser_interfaces),
		.an_mode = DW_10GBASER,
		.pma_config = xpcs_10gbaser_pma_config,
	},
	[DW_XPCS_10GBASEX] = {
		.supported = xpcs_10gbasex_features,
		.interface = xpcs_10gbasex_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gbasex_interfaces),
		.an_mode = DW_10GBASEX,
		.pma_config = xpcs_10gbasex_pma_config,
	},
};

static const struct xpcs_id xpcs_id_list[] = {
	{
		.id = DW_XPCS_ID,
		.mask = DW_XPCS_ID_MASK,
		.compat = synopsys_xpcs_compat,
	}, {
		.id = NXP_SJA1105_XPCS_ID,
		.mask = DW_XPCS_ID_MASK,
		.compat = nxp_sja1105_xpcs_compat,
	}, {
		.id = NXP_SJA1110_XPCS_ID,
		.mask = DW_XPCS_ID_MASK,
		.compat = nxp_sja1110_xpcs_compat,
	}, {
		.id = BT1_XGMAC_XPCS_ID,
		.mask = DW_XPCS_ID_MASK,
		.compat = bt1_xpcs_compat,
	},
};

static const struct phylink_pcs_ops xpcs_phylink_ops = {
	.pcs_validate = xpcs_validate,
	.pcs_config = xpcs_config,
	.pcs_get_state = xpcs_get_state,
	.pcs_an_restart = xpcs_an_restart,
	.pcs_link_up = xpcs_link_up,
};

static struct dw_xpcs *xpcs_create_data(struct mdio_device *mdiodev)
{
	struct dw_xpcs *xpcs;

	xpcs = kzalloc(sizeof(*xpcs), GFP_KERNEL);
	if (!xpcs)
		return ERR_PTR(-ENOMEM);

	xpcs->mdiodev = mdiodev;
	xpcs->pcs.ops = &xpcs_phylink_ops;
	xpcs->pcs.poll = true;

	return xpcs;
}

static void xpcs_free_data(struct dw_xpcs *xpcs)
{
	kfree(xpcs);
}

static int xpcs_init_clks(struct dw_xpcs *xpcs)
{
	static const char *ids[DW_XPCS_NUM_CLKS] = {
		[DW_XPCS_CLK_CORE] = "core",
		[DW_XPCS_CLK_PAD] = "pad",
	};
	struct device *dev = &xpcs->mdiodev->dev;
	int ret, i;

	for (i = 0; i < DW_XPCS_NUM_CLKS; ++i)
		xpcs->clks[i].id = ids[i];

	ret = clk_bulk_get_optional(dev, DW_XPCS_NUM_CLKS, xpcs->clks);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get clocks\n");

	ret = clk_bulk_prepare_enable(DW_XPCS_NUM_CLKS, xpcs->clks);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to enable clocks\n");

	return 0;
}

static void xpcs_clear_clks(struct dw_xpcs *xpcs)
{
	clk_bulk_disable_unprepare(DW_XPCS_NUM_CLKS, xpcs->clks);

	clk_bulk_put(DW_XPCS_NUM_CLKS, xpcs->clks);
}

static int xpcs_init_id(struct dw_xpcs *xpcs)
{
	const struct dw_xpcs_info *info;
	int i;

	info = device_get_match_data(&xpcs->mdiodev->dev) ?:
	       dev_get_platdata(&xpcs->mdiodev->dev);
	if (!info) {
		xpcs->info.did = DW_XPCS_ID_NATIVE;
		xpcs->info.pma = DW_XPCS_PMA_UNKNOWN;
	} else {
		xpcs->info = *info;
	}

	if (xpcs->info.did == DW_XPCS_ID_NATIVE)
		xpcs->info.did = xpcs_get_id(xpcs);

	for (i = 0; i < ARRAY_SIZE(xpcs_id_list); i++) {
		const struct xpcs_id *entry = &xpcs_id_list[i];

		if ((xpcs->info.did & entry->mask) != entry->id)
			continue;

		xpcs->id = entry;

		return 0;
	}

	return -ENODEV;
}

static int xpcs_init_iface(struct dw_xpcs *xpcs, phy_interface_t interface)
{
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(xpcs->id, interface);
	if (!compat)
		return -EINVAL;

	ret = xpcs_read(xpcs, MDIO_MMD_VEND1, DW_SR_CTRL_MMD_CTRL);
	if (ret < 0)
		return ret;

	xpcs->mmd_ctrl = ret;

	return xpcs_soft_reset(xpcs);
}

static struct dw_xpcs *xpcs_create(struct mdio_device *mdiodev,
				   phy_interface_t interface)
{
	struct dw_xpcs *xpcs;
	int ret;

	ret = device_attach(&mdiodev->dev);
	if (ret < 0 && ret != -ENODEV)
		return ERR_PTR(ret);

	xpcs = xpcs_create_data(mdiodev);
	if (IS_ERR(xpcs))
		return xpcs;

	ret = xpcs_init_clks(xpcs);
	if (ret)
		goto out_free_data;

	ret = xpcs_init_id(xpcs);
	if (ret)
		goto out_clear_clks;

	ret = xpcs_init_iface(xpcs, interface);
	if (ret)
		goto out_clear_clks;

	return xpcs;

out_clear_clks:
	xpcs_clear_clks(xpcs);

out_free_data:
	xpcs_free_data(xpcs);

	return ERR_PTR(ret);
}

struct dw_xpcs *xpcs_create_bynode(const struct fwnode_handle *fwnode,
				   phy_interface_t interface)
{
	struct fwnode_handle *pcs_node;
	struct mdio_device *mdiodev;
	struct dw_xpcs *xpcs;

	pcs_node = fwnode_find_reference(fwnode, "pcs-handle", 0);
	if (IS_ERR(pcs_node))
		return ERR_CAST(pcs_node);

	mdiodev = fwnode_mdio_find_device(pcs_node);
	fwnode_handle_put(pcs_node);
	if (!mdiodev)
		return ERR_PTR(-ENODEV);

	xpcs = xpcs_create(mdiodev, interface);
	if (IS_ERR(xpcs))
		mdio_device_put(mdiodev);

	return xpcs;
}
EXPORT_SYMBOL_GPL(xpcs_create_bynode);

struct dw_xpcs *xpcs_create_byaddr(struct mii_bus *bus, int addr,
				   phy_interface_t interface)
{
	struct mdio_device *mdiodev;
	struct dw_xpcs *xpcs;

	if (addr >= PHY_MAX_ADDR)
		return ERR_PTR(-EINVAL);

	if (mdiobus_is_registered_device(bus, addr)) {
		mdiodev = bus->mdio_map[addr];
		mdio_device_get(mdiodev);
	} else {
		mdiodev = mdio_device_create(bus, addr);
		if (IS_ERR(mdiodev))
			return ERR_CAST(mdiodev);
	}

	xpcs = xpcs_create(mdiodev, interface);
	if (IS_ERR(xpcs))
		mdio_device_put(mdiodev);

	return xpcs;
}
EXPORT_SYMBOL_GPL(xpcs_create_byaddr);

void xpcs_destroy(struct dw_xpcs *xpcs)
{
	if (!xpcs)
		return;

	mdio_device_put(xpcs->mdiodev);

	xpcs_clear_clks(xpcs);

	xpcs_free_data(xpcs);
}
EXPORT_SYMBOL_GPL(xpcs_destroy);

DW_XPCS_INFO_DECLARE(xpcs_generic, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_UNKNOWN);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen1_3g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN1_3G);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen2_3g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN2_3G);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen2_6g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN2_6G);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen4_3g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN4_3G);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen4_6g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN4_6G);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen5_10g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN5_10G);
DW_XPCS_INFO_DECLARE(xpcs_pma_gen5_12g, DW_XPCS_ID_NATIVE, DW_XPCS_PMA_GEN5_12G);
DW_XPCS_INFO_DECLARE(xpcs_bt1, BT1_XGMAC_XPCS_ID, DW_XPCS_PMA_GEN5_10G);

static const struct of_device_id xpcs_of_ids[] = {
	{ .compatible = "snps,dw-xpcs", .data = &xpcs_generic },
	{ .compatible = "snps,dw-xpcs-gen1-3g", .data = &xpcs_pma_gen1_3g },
	{ .compatible = "snps,dw-xpcs-gen2-3g", .data = &xpcs_pma_gen2_3g },
	{ .compatible = "snps,dw-xpcs-gen2-6g", .data = &xpcs_pma_gen2_6g },
	{ .compatible = "snps,dw-xpcs-gen4-3g", .data = &xpcs_pma_gen4_3g },
	{ .compatible = "snps,dw-xpcs-gen4-6g", .data = &xpcs_pma_gen4_6g },
	{ .compatible = "snps,dw-xpcs-gen5-10g", .data = &xpcs_pma_gen5_10g },
	{ .compatible = "snps,dw-xpcs-gen5-12g", .data = &xpcs_pma_gen5_12g },
	{ .compatible = "baikal,bt1-xpcs", .data = &xpcs_bt1 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, xpcs_of_ids);

static struct mdio_device_id __maybe_unused xpcs_mdio_ids[] = {
	{ DW_XPCS_ID, DW_XPCS_ID_MASK },
	{ NXP_SJA1105_XPCS_ID, DW_XPCS_ID_MASK },
	{ NXP_SJA1110_XPCS_ID, DW_XPCS_ID_MASK },
	{ }
};
MODULE_DEVICE_TABLE(mdio, xpcs_mdio_ids);

static struct mdio_driver xpcs_driver = {
	.mdiodrv.driver = {
		.name = "dwxpcs",
		.of_match_table = xpcs_of_ids,
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	},
};
mdio_module_driver(xpcs_driver);

MODULE_DESCRIPTION("DWC Ethernet XPCS platform driver");
MODULE_AUTHOR("Jose Abreu <Jose.Abreu@synopsys.com>");
MODULE_LICENSE("GPL v2");
