/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Baikal-T1 IRQ numbers declaration
 */
#ifndef __ASM_MACH_BAIKAL_T1_IRQ_H__
#define __ASM_MACH_BAIKAL_T1_IRQ_H__

#define NR_IRQS			255
#define MIPS_CPU_IRQ_BASE	0

#define BT1_APB_EHB_IRQ		16
#define BT1_WDT_IRQ		17
#define BT1_GPIO32_IRQ		19
#define BT1_TIMER0_IRQ		24
#define BT1_TIMER1_IRQ		25
#define BT1_TIMER2_IRQ		26
#define BT1_PVT_IRQ		31
#define BT1_I2C1_IRQ		33
#define BT1_I2C2_IRQ		34
#define BT1_SPI1_IRQ		40
#define BT1_SPI2_IRQ		41
#define BT1_UART0_IRQ		48
#define BT1_UART1_IRQ		49
#define BT1_DMAC_IRQ		56
#define BT1_SATA_IRQ		64
#define BT1_USB_IRQ		68
#define BT1_GMAC0_IRQ		72
#define BT1_GMAC1_IRQ		73
#define BT1_XGMAC_IRQ		74
#define BT1_XGMAC_TX0_IRQ	75
#define BT1_XGMAC_TX1_IRQ	76
#define BT1_XGMAC_RX0_IRQ	77
#define BT1_XGMAC_RX1_IRQ	78
#define BT1_XGMAC_XPCS_IRQ	79
#define BT1_PCIE_EDMA_TX0_IRQ	80
#define BT1_PCIE_EDMA_TX1_IRQ	81
#define BT1_PCIE_EDMA_TX2_IRQ	82
#define BT1_PCIE_EDMA_TX3_IRQ	83
#define BT1_PCIE_EDMA_RX0_IRQ	84
#define BT1_PCIE_EDMA_RX1_IRQ	85
#define BT1_PCIE_EDMA_RX2_IRQ	86
#define BT1_PCIE_EDMA_RX3_IRQ	87
#define BT1_PCIE_MSI_IRQ	88
#define BT1_PCIE_AER_IRQ	89
#define BT1_PCIE_PME_IRQ	90
#define BT1_PCIE_HP_IRQ		91
#define BT1_PCIE_BW_IRQ		92
#define BT1_PCIE_L_REQ_IRQ	93
#define BT1_DDR_DFI_E_IRQ	96
#define BT1_DDR_ECC_CE_IRQ	97
#define BT1_DDR_ECC_UE_IRQ	98
#define BT1_DDR_ECC_SBR_IRQ	99
#define BT1_HWA_IRQ		104
#define BT1_AXI_EHB_IRQ		127

#include_next <irq.h>

#endif /* __ASM_MACH_BAIKAL_T1_IRQ_H__ */
