// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Baikal-T1 GMAC driver
 *
 * Copyright (C) 2022 BAIKAL ELECTRONICS, JSC
 */
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/stmmac.h>

#include "dwmac1000.h"
#include "dwmac_dma.h"
#include "stmmac.h"
#include "stmmac_platform.h"

/* General Purpose IO */
#define GMAC_GPIO		0x000000e0
#define GMAC_GPIO_GPIS		BIT(0)
#define GMAC_GPIO_GPO		BIT(8)

struct bt1_xgmac {
	struct device *dev;
	struct clk *tx_clk;
};

typedef int (*bt1_xgmac_plat_init)(struct bt1_xgmac *, struct plat_stmmacenet_data *);

static int bt1_xgmac_clks_config(void *bsp_priv, bool enable)
{
	struct bt1_xgmac *btxg = bsp_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(btxg->tx_clk);
		if (ret)
			dev_err(btxg->dev, "Failed to enable Tx clock\n");
	} else {
		clk_disable_unprepare(btxg->tx_clk);
	}

	return ret;
}

static int bt1_gmac_bus_reset(void *bsp_priv)
{
	struct bt1_xgmac *btxg = bsp_priv;
	struct stmmac_priv *priv = netdev_priv(dev_get_drvdata(btxg->dev));

	writel(0, priv->ioaddr + GMAC_GPIO);
	fsleep(priv->mii->reset_delay_us);
	writel(GMAC_GPIO_GPO, priv->ioaddr + GMAC_GPIO);
	if (priv->mii->reset_post_delay_us > 0)
		fsleep(priv->mii->reset_post_delay_us);

	return 0;
}

/* Clean the basic MAC registers up. Note the MAC interrupts are enabled by
 * default after reset. Let's mask them out so not to have any spurious
 * MAC-related IRQ generated during the cleanup procedure.
 */
static void bt1_gmac_core_clean(struct stmmac_priv *priv)
{
	int i;

	writel(0x7FF, priv->ioaddr + GMAC_INT_MASK);
	writel(0, priv->ioaddr + GMAC_CONTROL);
	writel(0, priv->ioaddr + GMAC_FRAME_FILTER);
	writel(0, priv->ioaddr + GMAC_HASH_HIGH);
	writel(0, priv->ioaddr + GMAC_HASH_LOW);
	writel(0, priv->ioaddr + GMAC_FLOW_CTRL);
	writel(0, priv->ioaddr + GMAC_VLAN_TAG);
	writel(0, priv->ioaddr + GMAC_DEBUG);
	writel(0x80000000, priv->ioaddr + GMAC_PMT);
	writel(0, priv->ioaddr + LPI_CTRL_STATUS);
	writel(0x03e80000, priv->ioaddr + LPI_TIMER_CTRL);
	for (i = 0; i < 15; ++i) {
		writel(0x0000ffff, priv->ioaddr + GMAC_ADDR_HIGH(i));
		writel(0xffffffff, priv->ioaddr + GMAC_ADDR_LOW(i));
	}
	writel(0, priv->ioaddr + GMAC_PCS_BASE);
	writel(0, priv->ioaddr + GMAC_RGSMIIIS);
	writel(0x1, priv->ioaddr + GMAC_MMC_CTRL);
	readl(priv->ioaddr + GMAC_INT_STATUS);
	readl(priv->ioaddr + GMAC_PMT);
	readl(priv->ioaddr + LPI_CTRL_STATUS);
}

/* Clean the basic DMA registers up */
static void bt1_gmac_dma_clean(struct stmmac_priv *priv)
{
	writel(0, priv->ioaddr + DMA_INTR_ENA);
	writel(0x00020100, priv->ioaddr + DMA_BUS_MODE);
	writel(0, priv->ioaddr + DMA_RCV_BASE_ADDR);
	writel(0, priv->ioaddr + DMA_TX_BASE_ADDR);
	writel(0x00100000, priv->ioaddr + DMA_CONTROL);
	writel(0x00110001, priv->ioaddr + DMA_AXI_BUS_MODE);
	writel(0x0001FFFF, priv->ioaddr + DMA_STATUS);
}

static int bt1_gmac_swr_reset(void *bsp_priv)
{
	struct bt1_xgmac *btxg = bsp_priv;
	struct stmmac_priv *priv = netdev_priv(dev_get_drvdata(btxg->dev));

	bt1_gmac_core_clean(priv);

	bt1_gmac_dma_clean(priv);

	return 0;
}

static void bt1_gmac_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct bt1_xgmac *btxg = bsp_priv;
	unsigned long rate;
	int ret;

	switch (speed) {
	case SPEED_1000:
		rate = 250000000;
		break;
	case SPEED_100:
		rate = 50000000;
		break;
	case SPEED_10:
		rate = 5000000;
		break;
	default:
		dev_err(btxg->dev, "Unsupported speed %u\n", speed);
		return;
	}

	/* The clock must be gated to successfully update the rate */
	clk_disable_unprepare(btxg->tx_clk);

	ret = clk_set_rate(btxg->tx_clk, rate);
	if (ret)
		dev_err(btxg->dev, "Failed to update Tx clock rate %lu\n", rate);

	ret = clk_prepare_enable(btxg->tx_clk);
	if (ret)
		dev_err(btxg->dev, "Failed to re-enable Tx clock\n");
}

static int bt1_gmac_plat_data_init(struct bt1_xgmac *btxg,
				   struct plat_stmmacenet_data *plat)
{
	plat->has_gmac = 1;
	plat->host_dma_width = 32;
	plat->tx_fifo_size = SZ_16K;
	plat->rx_fifo_size = SZ_16K;
	plat->enh_desc = 1; /* cap.enh_desc */
	plat->tx_coe = 1;
	plat->rx_coe = 1;
	plat->pmt = 1;
	plat->unicast_filter_entries = 8;
	plat->multicast_filter_bins = 0;
	plat->clks_config = bt1_xgmac_clks_config;
	plat->bus_reset = bt1_gmac_bus_reset;
	plat->swr_reset = bt1_gmac_swr_reset;
	plat->fix_mac_speed = bt1_gmac_fix_mac_speed;
	plat->mdio_bus_data->needs_reset = true;

	return 0;
}

static int bt1_xgmac_plat_data_init(struct bt1_xgmac *btxg,
				    struct plat_stmmacenet_data *plat)
{
	plat->has_xgmac = 1;
	plat->host_dma_width = 40; /* = Cap */
	plat->tx_fifo_size = SZ_32K; /* = Cap */
	plat->rx_fifo_size = SZ_32K; /* = Cap */
	plat->tx_coe = 1; /* = Cap */
	plat->rx_coe = 1; /* = Cap */
	plat->tso_en = 1; /* & cap.tsoen */
	plat->rss_en = 1; /* & cap.rssen */
	plat->sph_disable = 0; /* Default */
	//plat->pmt = 0; /* cap.pmt_rwk */
	plat->unicast_filter_entries = 8;
	plat->multicast_filter_bins = 64;
	plat->clks_config = bt1_xgmac_clks_config;
	plat->multi_msi_en = true;

	return 0;
}

static int bt1_xgmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat;
	struct stmmac_resources stmmac_res;
	bt1_xgmac_plat_init plat_init;
	struct bt1_xgmac *btxg;
	int ret;

	btxg = devm_kzalloc(&pdev->dev, sizeof(*btxg), GFP_KERNEL);
	if (!btxg)
		return -ENOMEM;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat))
		return dev_err_probe(&pdev->dev, PTR_ERR(plat), "DT config failed\n");

	btxg->dev = &pdev->dev;

	plat->bsp_priv = btxg;

	btxg->tx_clk = devm_clk_get(&pdev->dev, "tx");
	if (IS_ERR(btxg->tx_clk)) {
		ret = dev_err_probe(&pdev->dev, PTR_ERR(btxg->tx_clk),
				    "Failed to get Tx clock\n");
		goto err_remove_config_dt;
	}

	ret = clk_prepare_enable(btxg->tx_clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to pre-enable Tx clock\n");
		goto err_remove_config_dt;
	}

	plat_init = device_get_match_data(&pdev->dev);
	if (plat_init) {
		ret = plat_init(btxg, plat);
		if (ret)
			goto err_disable_tx_clk;
	}

	ret = stmmac_dvr_probe(&pdev->dev, plat, &stmmac_res);
	if (ret)
		goto err_disable_tx_clk;

	return 0;

err_disable_tx_clk:
	clk_disable_unprepare(btxg->tx_clk);

err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat);

	return ret;
}

static const struct of_device_id bt1_xgmac_match[] = {
	{ .compatible = "baikal,bt1-gmac", .data = (void *)bt1_gmac_plat_data_init },
	{ .compatible = "baikal,bt1-xgmac", .data = (void *)bt1_xgmac_plat_data_init },
	{ }
};
MODULE_DEVICE_TABLE(of, bt1_xgmac_match);

static struct platform_driver bt1_xgmac_driver = {
	.probe  = bt1_xgmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "bt1-xgmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = of_match_ptr(bt1_xgmac_match),
	},
};
module_platform_driver(bt1_xgmac_driver);

MODULE_AUTHOR("Serge Semin <Sergey.Semin@baikalelectronics.ru>");
MODULE_DESCRIPTION("Baikal-T1 GMAC/XGMAC glue driver");
MODULE_LICENSE("GPL v2");
