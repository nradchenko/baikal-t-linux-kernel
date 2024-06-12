/*
 * AMD 10Gb Ethernet driver
 *
 * This file is available to you under your choice of the following two
 * licenses:
 *
 * License 1: GPLv2
 *
 * Copyright (c) 2014-2016 Advanced Micro Devices, Inc.
 *
 * This file is free software; you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * License 2: Modified BSD
 *
 * Copyright (c) 2014-2016 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/clk.h>
#include <linux/property.h>
#include <linux/acpi.h>
#include <linux/mdio.h>

#include "xgbe.h"
#include "xgbe-common.h"

#ifdef CONFIG_ACPI
static const struct acpi_device_id xgbe_acpi_match[];

static struct xgbe_version_data *xgbe_acpi_vdata(struct xgbe_prv_data *pdata)
{
	const struct acpi_device_id *id;

	id = acpi_match_device(xgbe_acpi_match, pdata->dev);

	return id ? (struct xgbe_version_data *)id->driver_data : NULL;
}

static int xgbe_acpi_support(struct xgbe_prv_data *pdata)
{
	struct device *dev = pdata->dev;
	u32 property;
	int ret;

	/* Obtain the system clock setting */
	ret = device_property_read_u32(dev, XGBE_ACPI_DMA_FREQ, &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s property\n",
			XGBE_ACPI_DMA_FREQ);
		return ret;
	}
	pdata->sysclk_rate = property;

	/* Obtain the PTP clock setting */
	ret = device_property_read_u32(dev, XGBE_ACPI_PTP_FREQ, &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s property\n",
			XGBE_ACPI_PTP_FREQ);
		return ret;
	}
	pdata->ptpclk_rate = property;

	return 0;
}
#else   /* CONFIG_ACPI */
static struct xgbe_version_data *xgbe_acpi_vdata(struct xgbe_prv_data *pdata)
{
	return NULL;
}

static int xgbe_acpi_support(struct xgbe_prv_data *pdata)
{
	return -EINVAL;
}
#endif  /* CONFIG_ACPI */

#ifdef CONFIG_OF
static const struct of_device_id xgbe_of_match[];

static struct xgbe_version_data *xgbe_of_vdata(struct xgbe_prv_data *pdata)
{
	const struct of_device_id *id;

	id = of_match_device(xgbe_of_match, pdata->dev);

	return id ? (struct xgbe_version_data *)id->data : NULL;
}

static int xgbe_of_support(struct xgbe_prv_data *pdata)
{
	struct device *dev = pdata->dev;

	/* Obtain the system clock setting */
	pdata->sysclk = devm_clk_get(dev, XGBE_DMA_CLOCK);
	if (IS_ERR(pdata->sysclk)) {
		dev_err(dev, "dma devm_clk_get failed\n");
		return PTR_ERR(pdata->sysclk);
	}
	pdata->sysclk_rate = clk_get_rate(pdata->sysclk);

	/* Obtain the PTP clock setting */
	pdata->ptpclk = devm_clk_get(dev, XGBE_PTP_CLOCK);
	if (IS_ERR(pdata->ptpclk)) {
		dev_err(dev, "ptp devm_clk_get failed\n");
		return PTR_ERR(pdata->ptpclk);
	}
	pdata->ptpclk_rate = clk_get_rate(pdata->ptpclk);

	return 0;
}

static struct platform_device *xgbe_of_get_phy_pdev(struct xgbe_prv_data *pdata)
{
	struct device *dev = pdata->dev;
	struct device_node *phy_node;
	struct platform_device *phy_pdev;

	phy_node = of_parse_phandle(dev->of_node, "phy-handle", 0);
	if (phy_node) {
		/* Old style device tree:
		 *   The XGBE and PHY resources are separate
		 */
		phy_pdev = of_find_device_by_node(phy_node);
		of_node_put(phy_node);
	} else {
		/* New style device tree:
		 *   The XGBE and PHY resources are grouped together with
		 *   the PHY resources listed last
		 */
		get_device(dev);
		phy_pdev = pdata->platdev;
	}

	return phy_pdev;
}
#else   /* CONFIG_OF */
static struct xgbe_version_data *xgbe_of_vdata(struct xgbe_prv_data *pdata)
{
	return NULL;
}

static int xgbe_of_support(struct xgbe_prv_data *pdata)
{
	return -EINVAL;
}

static struct platform_device *xgbe_of_get_phy_pdev(struct xgbe_prv_data *pdata)
{
	return NULL;
}
#endif  /* CONFIG_OF */

static unsigned int xgbe_resource_count(struct platform_device *pdev,
					unsigned int type)
{
	unsigned int count;
	int i;

	for (i = 0, count = 0; i < pdev->num_resources; i++) {
		struct resource *res = &pdev->resource[i];

		if (type == resource_type(res))
			count++;
	}

	return count;
}

static struct platform_device *xgbe_get_phy_pdev(struct xgbe_prv_data *pdata)
{
	struct platform_device *phy_pdev;

	if (pdata->use_acpi) {
		get_device(pdata->dev);
		phy_pdev = pdata->platdev;
	} else {
		phy_pdev = xgbe_of_get_phy_pdev(pdata);
	}

	return phy_pdev;
}

static struct xgbe_version_data *xgbe_get_vdata(struct xgbe_prv_data *pdata)
{
	return pdata->use_acpi ? xgbe_acpi_vdata(pdata)
			       : xgbe_of_vdata(pdata);
}

static int xgbe_init_function_plat_amd(struct xgbe_prv_data *pdata)
{
	unsigned int phy_memnum, phy_irqnum, dma_irqnum, dma_irqend;
	struct platform_device *pdev = pdata->platdev;
	struct platform_device *phy_pdev;
	struct device *dev = pdata->dev;
	int ret;

	phy_pdev = xgbe_get_phy_pdev(pdata);
	if (!phy_pdev) {
		dev_err(dev, "unable to obtain phy device\n");
		return -EINVAL;
	}
	pdata->phy_platdev = phy_pdev;
	pdata->phy_dev = &phy_pdev->dev;

	if (pdev == phy_pdev) {
		/* New style device tree or ACPI:
		 *   The XGBE and PHY resources are grouped together with
		 *   the PHY resources listed last
		 */
		phy_memnum = xgbe_resource_count(pdev, IORESOURCE_MEM) - 3;
		phy_irqnum = platform_irq_count(pdev) - 1;
		dma_irqnum = 1;
		dma_irqend = phy_irqnum;
	} else {
		/* Old style device tree:
		 *   The XGBE and PHY resources are separate
		 */
		phy_memnum = 0;
		phy_irqnum = 0;
		dma_irqnum = 1;
		dma_irqend = platform_irq_count(pdev);
	}

	/* Obtain the mmio areas for the device */
	pdata->xgmac_regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pdata->xgmac_regs)) {
		dev_err(dev, "xgmac ioremap failed\n");
		ret = PTR_ERR(pdata->xgmac_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "xgmac_regs = %p\n", pdata->xgmac_regs);

	pdata->xpcs_regs = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(pdata->xpcs_regs)) {
		dev_err(dev, "xpcs ioremap failed\n");
		ret = PTR_ERR(pdata->xpcs_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "xpcs_regs  = %p\n", pdata->xpcs_regs);

	pdata->rxtx_regs = devm_platform_ioremap_resource(phy_pdev,
							  phy_memnum++);
	if (IS_ERR(pdata->rxtx_regs)) {
		dev_err(dev, "rxtx ioremap failed\n");
		ret = PTR_ERR(pdata->rxtx_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "rxtx_regs  = %p\n", pdata->rxtx_regs);

	pdata->sir0_regs = devm_platform_ioremap_resource(phy_pdev,
							  phy_memnum++);
	if (IS_ERR(pdata->sir0_regs)) {
		dev_err(dev, "sir0 ioremap failed\n");
		ret = PTR_ERR(pdata->sir0_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "sir0_regs  = %p\n", pdata->sir0_regs);

	pdata->sir1_regs = devm_platform_ioremap_resource(phy_pdev,
							  phy_memnum++);
	if (IS_ERR(pdata->sir1_regs)) {
		dev_err(dev, "sir1 ioremap failed\n");
		ret = PTR_ERR(pdata->sir1_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "sir1_regs  = %p\n", pdata->sir1_regs);

	/* Check for per channel interrupt support */
	if (device_property_present(dev, XGBE_DMA_IRQS_PROPERTY)) {
		pdata->per_channel_irq = 1;
		pdata->channel_irq_mode = XGBE_IRQ_MODE_EDGE;
	}

	/* Obtain device settings unique to ACPI/OF */
	if (pdata->use_acpi)
		ret = xgbe_acpi_support(pdata);
	else
		ret = xgbe_of_support(pdata);
	if (ret)
		goto err_io;

	/* Always have XGMAC and XPCS (auto-negotiation) interrupts */
	pdata->irq_count = 2;

	/* Get the device interrupt */
	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		goto err_io;
	pdata->dev_irq = ret;

	/* Get the per channel DMA interrupts */
	if (pdata->per_channel_irq) {
		unsigned int i, max = ARRAY_SIZE(pdata->channel_irq);

		for (i = 0; (i < max) && (dma_irqnum < dma_irqend); i++) {
			ret = platform_get_irq(pdata->platdev, dma_irqnum++);
			if (ret < 0)
				goto err_io;

			pdata->channel_irq[i] = ret;
		}

		pdata->channel_irq_count = max;

		pdata->irq_count += max;
	}

	/* Get the auto-negotiation interrupt */
	ret = platform_get_irq(phy_pdev, phy_irqnum++);
	if (ret < 0)
		goto err_io;
	pdata->an_irq = ret;

	return 0;

err_io:
	platform_device_put(phy_pdev);

	return ret;
}

static void xgbe_init_function_disclk_baikal(void *data)
{
	struct xgbe_prv_data *pdata = data;

	clk_disable_unprepare(pdata->sysclk);
}

static int xgbe_init_function_plat_baikal(struct xgbe_prv_data *pdata)
{
	struct platform_device *pdev = pdata->platdev;
	struct device *dev = pdata->dev;
	struct device_node *phy_node;
	struct mdio_device *mdio_dev;
	int ret;

	phy_node = of_parse_phandle(dev->of_node, "phy-handle", 0);
	if (!phy_node) {
		dev_err(dev, "unable to obtain phy node\n");
		return -ENODEV;
	}

	/* Nothing more sophisticated available at the moment... */
	mdio_dev = of_mdio_find_device(phy_node);
	of_node_put(phy_node);
	if (!mdio_dev) {
		dev_err_probe(dev, -EPROBE_DEFER, "unable to obtain mdio device\n");
		return -EPROBE_DEFER;
	}

	pdata->phy_platdev = NULL;
	pdata->phy_dev = &mdio_dev->dev;

	/* Obtain the CSR regions of the device */
	pdata->xgmac_regs = devm_platform_ioremap_resource_byname(pdev, "stmmaceth");
	if (IS_ERR(pdata->xgmac_regs)) {
		dev_err(dev, "xgmac ioremap failed\n");
		ret = PTR_ERR(pdata->xgmac_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "xgmac_regs = %p\n", pdata->xgmac_regs);

	pdata->xpcs_regs = devm_platform_ioremap_resource_byname(pdev, "xpcs");
	if (IS_ERR(pdata->xpcs_regs)) {
		dev_err(dev, "xpcs ioremap failed\n");
		ret = PTR_ERR(pdata->xpcs_regs);
		goto err_io;
	}
	if (netif_msg_probe(pdata))
		dev_dbg(dev, "xpcs_regs  = %p\n", pdata->xpcs_regs);

	/* Obtain the platform clocks setting */
	pdata->apbclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(pdata->apbclk)) {
		dev_err(dev, "apb devm_clk_get failed\n");
		ret = PTR_ERR(pdata->apbclk);
		goto err_io;
	}

	pdata->sysclk = devm_clk_get(dev, "stmmaceth");
	if (IS_ERR(pdata->sysclk)) {
		dev_err(dev, "dma devm_clk_get failed\n");
		ret = PTR_ERR(pdata->sysclk);
		goto err_io;
	}
	pdata->sysclk_rate = clk_get_rate(pdata->sysclk);

	pdata->ptpclk = devm_clk_get(dev, "ptp_ref");
	if (IS_ERR(pdata->ptpclk)) {
		dev_err(dev, "ptp devm_clk_get failed\n");
		ret = PTR_ERR(pdata->ptpclk);
		goto err_io;
	}
	pdata->ptpclk_rate = clk_get_rate(pdata->ptpclk);

	pdata->refclk = devm_clk_get(dev, "tx");
	if (IS_ERR(pdata->refclk)) {
		dev_err(dev, "ref devm_clk_get failed\n");
		ret = PTR_ERR(pdata->refclk);
		goto err_io;
	}

	/* Even though it's claimed that the CSR clock source is different from
	 * the application clock the CSRs are still unavailable until the DMA
	 * clock signal is enabled.
	 */
	ret = clk_prepare_enable(pdata->sysclk);
	if (ret) {
		dev_err(dev, "sys clock enable failed\n");
		goto err_io;
	}

	ret = devm_add_action_or_reset(dev, xgbe_init_function_disclk_baikal, pdata);
	if (ret) {
		dev_err(dev, "sys clock undo registration failed\n");
		goto err_io;
	}

	/* Forget about the per-channel IRQs for now... */
	pdata->per_channel_irq = 0; // 1
	pdata->channel_irq_mode = XGBE_IRQ_MODE_EDGE; // XGBE_IRQ_MODE_LEVEL;

	pdata->irq_count = 1;

	ret = platform_get_irq_byname(pdev, "macirq");
	if (ret < 0)
		goto err_io;
	pdata->dev_irq = ret;
	pdata->an_irq = pdata->dev_irq;

	return 0;

err_io:
	put_device(pdata->phy_dev);

	return ret;
}

static int xgbe_platform_probe(struct platform_device *pdev)
{
	struct xgbe_prv_data *pdata;
	struct device *dev = &pdev->dev;
	const char *phy_mode;
	enum dev_dma_attr attr;
	int ret;

	pdata = xgbe_alloc_pdata(dev);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto err_alloc;
	}

	pdata->platdev = pdev;
	pdata->adev = ACPI_COMPANION(dev);
	platform_set_drvdata(pdev, pdata);

	/* Check if we should use ACPI or DT */
	pdata->use_acpi = dev->of_node ? 0 : 1;

	/* Get the version data */
	pdata->vdata = xgbe_get_vdata(pdata);

	/* Platform-specific resources setup */
	ret = pdata->vdata->init_function_plat_impl(pdata);
	if (ret)
		goto err_plat;

	/* Activate basic clocks */
	ret = clk_prepare_enable(pdata->apbclk);
	if (ret) {
		dev_err(dev, "apb clock enable failed\n");
		goto err_apb;
	}

	ret = clk_prepare_enable(pdata->refclk);
	if (ret) {
		dev_err(dev, "ref clock enable failed\n");
		goto err_ref;
	}

	/* Retrieve the MAC address */
	ret = device_property_read_u8_array(dev, XGBE_MAC_ADDR_PROPERTY,
					    pdata->mac_addr,
					    sizeof(pdata->mac_addr));
	if (ret || !is_valid_ether_addr(pdata->mac_addr)) {
		dev_err(dev, "invalid %s property\n", XGBE_MAC_ADDR_PROPERTY);
		if (!ret)
			ret = -EINVAL;
		goto err_io;
	}

	/* Retrieve the PHY mode - "xgmii", "10gbase-r" or "xaui"/"10gbase-x" */
	ret = device_property_read_string(dev, XGBE_PHY_MODE_PROPERTY,
					  &phy_mode);
	if (ret) {
		dev_err(dev, "failed to read %s property\n", XGBE_PHY_MODE_PROPERTY);
		goto err_io;
	} else if (!strcmp(phy_mode, phy_modes(PHY_INTERFACE_MODE_XGMII))) {
		pdata->phy_mode = PHY_INTERFACE_MODE_XGMII;
	} else if (!strcmp(phy_mode, phy_modes(PHY_INTERFACE_MODE_10GBASER))) {
		pdata->phy_mode = PHY_INTERFACE_MODE_10GBASER;
	} else if (!strcmp(phy_mode, phy_modes(PHY_INTERFACE_MODE_XAUI))) {
		pdata->phy_mode = PHY_INTERFACE_MODE_XAUI;
	} else if (!strcmp(phy_mode, phy_modes(PHY_INTERFACE_MODE_10GBASEX))) {
		pdata->phy_mode = PHY_INTERFACE_MODE_10GBASEX;
	} else {
		ret = -EINVAL;
		dev_err(dev, "invalid %s property\n", XGBE_PHY_MODE_PROPERTY);
		goto err_io;
	}

	/* Set the DMA coherency values */
	attr = device_get_dma_attr(dev);
	if (attr == DEV_DMA_NOT_SUPPORTED) {
		dev_err(dev, "DMA is not supported");
		ret = -ENODEV;
		goto err_io;
	}
	pdata->coherent = (attr == DEV_DMA_COHERENT);
	if (pdata->coherent) {
		pdata->arcr = XGBE_DMA_OS_ARCR;
		pdata->awcr = XGBE_DMA_OS_AWCR;
	} else {
		pdata->arcr = XGBE_DMA_SYS_ARCR;
		pdata->awcr = XGBE_DMA_SYS_AWCR;
	}

	/* Set the maximum fifo amounts */
	pdata->tx_max_fifo_size = pdata->vdata->tx_max_fifo_size;
	pdata->rx_max_fifo_size = pdata->vdata->rx_max_fifo_size;

	/* Set the hardware channel and queue counts */
	xgbe_set_counts(pdata);

	/* Configure the netdev resource */
	ret = xgbe_config_netdev(pdata);
	if (ret)
		goto err_io;

	netdev_notice(pdata->netdev, "net device enabled\n");

	return 0;

err_io:
	clk_disable_unprepare(pdata->refclk);

err_ref:
	clk_disable_unprepare(pdata->apbclk);

err_apb:
	put_device(pdata->phy_dev);

err_plat:
	xgbe_free_pdata(pdata);

err_alloc:
	dev_notice(dev, "net device not enabled\n");

	return ret;
}

static int xgbe_platform_remove(struct platform_device *pdev)
{
	struct xgbe_prv_data *pdata = platform_get_drvdata(pdev);

	xgbe_deconfig_netdev(pdata);

	clk_disable_unprepare(pdata->refclk);

	clk_disable_unprepare(pdata->apbclk);

	put_device(pdata->phy_dev);

	xgbe_free_pdata(pdata);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int xgbe_platform_suspend(struct device *dev)
{
	struct xgbe_prv_data *pdata = dev_get_drvdata(dev);
	struct net_device *netdev = pdata->netdev;
	int ret = 0;

	DBGPR("-->xgbe_suspend\n");

	if (netif_running(netdev))
		ret = xgbe_powerdown(netdev, XGMAC_DRIVER_CONTEXT);

	pdata->lpm_ctrl = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_CTRL1);
	pdata->lpm_ctrl |= MDIO_CTRL1_LPOWER;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_CTRL1, pdata->lpm_ctrl);

	DBGPR("<--xgbe_suspend\n");

	return ret;
}

static int xgbe_platform_resume(struct device *dev)
{
	struct xgbe_prv_data *pdata = dev_get_drvdata(dev);
	struct net_device *netdev = pdata->netdev;
	int ret = 0;

	DBGPR("-->xgbe_resume\n");

	pdata->lpm_ctrl &= ~MDIO_CTRL1_LPOWER;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_CTRL1, pdata->lpm_ctrl);

	if (netif_running(netdev)) {
		ret = xgbe_powerup(netdev, XGMAC_DRIVER_CONTEXT);

		/* Schedule a restart in case the link or phy state changed
		 * while we were powered down.
		 */
		schedule_work(&pdata->restart_work);
	}

	DBGPR("<--xgbe_resume\n");

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct xgbe_version_data xgbe_v1 = {
	.init_function_plat_impl	= xgbe_init_function_plat_amd,
	.init_function_ptrs_phy_impl	= xgbe_init_function_ptrs_phy_v1,
	.xpcs_access			= XGBE_XPCS_ACCESS_V1,
	.tx_max_fifo_size		= 81920,
	.rx_max_fifo_size		= 81920,
	.tx_tstamp_workaround		= 1,
};

static const struct xgbe_version_data xgbe_v3 = {
	.init_function_plat_impl	= xgbe_init_function_plat_baikal,
	.init_function_ptrs_phy_impl	= xgbe_init_function_ptrs_phy_v3,
	.xpcs_access			= XGBE_XPCS_ACCESS_V1,
	.tx_max_fifo_size		= 32768,
	.rx_max_fifo_size		= 32768,
	.blen				= DMA_SBMR_BLEN_16,
	.pbl				= DMA_PBL_256,
	.rd_osr_limit			= 8,
	.wr_osr_limit			= 8,
	.tx_tstamp_workaround		= 1,
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id xgbe_acpi_match[] = {
	{ .id = "AMDI8001",
	  .driver_data = (kernel_ulong_t)&xgbe_v1 },
	{},
};

MODULE_DEVICE_TABLE(acpi, xgbe_acpi_match);
#endif

#ifdef CONFIG_OF
static const struct of_device_id xgbe_of_match[] = {
	{ .compatible = "amd,xgbe-seattle-v1a",
	  .data = &xgbe_v1 },
	{ .compatible = "amd,bt1-xgmac",
	  .data = &xgbe_v3 },
	{},
};

MODULE_DEVICE_TABLE(of, xgbe_of_match);
#endif

static SIMPLE_DEV_PM_OPS(xgbe_platform_pm_ops,
			 xgbe_platform_suspend, xgbe_platform_resume);

static struct platform_driver xgbe_driver = {
	.driver = {
		.name = XGBE_DRV_NAME,
#ifdef CONFIG_ACPI
		.acpi_match_table = xgbe_acpi_match,
#endif
#ifdef CONFIG_OF
		.of_match_table = xgbe_of_match,
#endif
		.pm = &xgbe_platform_pm_ops,
	},
	.probe = xgbe_platform_probe,
	.remove = xgbe_platform_remove,
};

int xgbe_platform_init(void)
{
	return platform_driver_register(&xgbe_driver);
}

void xgbe_platform_exit(void)
{
	platform_driver_unregister(&xgbe_driver);
}
