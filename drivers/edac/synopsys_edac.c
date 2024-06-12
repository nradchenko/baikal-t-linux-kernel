// SPDX-License-Identifier: GPL-2.0-only
/*
 * Synopsys DW uMCTL2 DDR ECC Driver
 * This driver is based on ppc4xx_edac.c drivers
 *
 * Copyright (C) 2012 - 2014 Xilinx, Inc.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/edac.h>
#include <linux/fs.h>
#include <linux/log2.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/pfn.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/spinlock.h>
#include <linux/units.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "edac_module.h"

/* Number of channels per memory controller */
#define SNPS_EDAC_NR_CHANS		1

#define SNPS_EDAC_MSG_SIZE		256

#define SNPS_EDAC_MOD_STRING		"snps_edac"
#define SNPS_EDAC_MOD_VER		"1"

/* DDR capabilities */
#define SNPS_CAP_ECC_SCRUB		BIT(0)
#define SNPS_CAP_ECC_SCRUBBER		BIT(1)
#define SNPS_CAP_ZYNQMP			BIT(31)

/* Synopsys uMCTL2 DDR controller registers that are relevant to ECC */

/* DDRC Master 0 Register */
#define DDR_MSTR_OFST			0x0

/* ECC Configuration Registers */
#define ECC_CFG0_OFST			0x70
#define ECC_CFG1_OFST			0x74

/* ECC Status Register */
#define ECC_STAT_OFST			0x78

/* ECC Clear Register */
#define ECC_CLR_OFST			0x7C

/* ECC Error count Register */
#define ECC_ERRCNT_OFST			0x80

/* ECC Corrected Error Address Register */
#define ECC_CEADDR0_OFST		0x84
#define ECC_CEADDR1_OFST		0x88

/* ECC Syndrome Registers */
#define ECC_CSYND0_OFST			0x8C
#define ECC_CSYND1_OFST			0x90
#define ECC_CSYND2_OFST			0x94

/* ECC Bit Mask0 Address Register */
#define ECC_BITMASK0_OFST		0x98
#define ECC_BITMASK1_OFST		0x9C
#define ECC_BITMASK2_OFST		0xA0

/* ECC UnCorrected Error Address Register */
#define ECC_UEADDR0_OFST		0xA4
#define ECC_UEADDR1_OFST		0xA8

/* ECC Syndrome Registers */
#define ECC_UESYND0_OFST		0xAC
#define ECC_UESYND1_OFST		0xB0
#define ECC_UESYND2_OFST		0xB4

/* ECC Poison Address Reg */
#define ECC_POISON0_OFST		0xB8
#define ECC_POISON1_OFST		0xBC

/* DDR CRC/Parity Registers */
#define DDR_CRCPARCTL0_OFST		0xC0
#define DDR_CRCPARCTL1_OFST		0xC4
#define DDR_CRCPARCTL2_OFST		0xC8
#define DDR_CRCPARSTAT_OFST		0xCC

/* DDR Address Map Registers */
#define DDR_ADDRMAP0_OFST		0x200

/* DDR Software Control Register */
#define DDR_SWCTL			0x320

/* ECC Poison Pattern Registers */
#define ECC_POISONPAT0_OFST		0x37C
#define ECC_POISONPAT1_OFST		0x380
#define ECC_POISONPAT2_OFST		0x384

/* DDR SAR Registers */
#define DDR_SARBASE0_OFST		0xF04
#define DDR_SARSIZE0_OFST		0xF08

/* ECC Scrubber Registers */
#define ECC_SBRCTL_OFST			0xF24
#define ECC_SBRSTAT_OFST		0xF28
#define ECC_SBRWDATA0_OFST		0xF2C
#define ECC_SBRWDATA1_OFST		0xF30

/* ZynqMP DDR QOS Registers */
#define ZYNQMP_DDR_QOS_IRQ_STAT_OFST	0x20200
#define ZYNQMP_DDR_QOS_IRQ_EN_OFST	0x20208
#define ZYNQMP_DDR_QOS_IRQ_DB_OFST	0x2020C

/* DDR Master register definitions */
#define DDR_MSTR_DEV_CFG_MASK		GENMASK(31, 30)
#define DDR_MSTR_DEV_X4			0
#define DDR_MSTR_DEV_X8			1
#define DDR_MSTR_DEV_X16		2
#define DDR_MSTR_DEV_X32		3
#define DDR_MSTR_ACT_RANKS_MASK		GENMASK(27, 24)
#define DDR_MSTR_FREQ_RATIO11		BIT(22)
#define DDR_MSTR_BURST_RDWR		GENMASK(19, 16)
#define DDR_MSTR_BUSWIDTH_MASK		GENMASK(13, 12)
#define DDR_MSTR_MEM_MASK		GENMASK(5, 0)
#define DDR_MSTR_MEM_LPDDR4		BIT(5)
#define DDR_MSTR_MEM_DDR4		BIT(4)
#define DDR_MSTR_MEM_LPDDR3		BIT(3)
#define DDR_MSTR_MEM_LPDDR2		BIT(2)
#define DDR_MSTR_MEM_LPDDR		BIT(1)
#define DDR_MSTR_MEM_DDR3		BIT(0)
#define DDR_MSTR_MEM_DDR2		0

/* ECC CFG0 register definitions */
#define ECC_CFG0_DIS_SCRUB		BIT(4)
#define ECC_CFG0_MODE_MASK		GENMASK(2, 0)

/* ECC CFG1 register definitions */
#define ECC_CFG1_POISON_BIT		BIT(1)
#define ECC_CFG1_POISON_EN		BIT(0)

/* ECC status register definitions */
#define ECC_STAT_UE_MASK		GENMASK(23, 16)
#define ECC_STAT_CE_MASK		GENMASK(15, 8)
#define ECC_STAT_BITNUM_MASK		GENMASK(6, 0)

/* ECC control/clear register definitions */
#define ECC_CTRL_CLR_CE_ERR		BIT(0)
#define ECC_CTRL_CLR_UE_ERR		BIT(1)
#define ECC_CTRL_CLR_CE_ERRCNT		BIT(2)
#define ECC_CTRL_CLR_UE_ERRCNT		BIT(3)
#define ECC_CTRL_EN_CE_IRQ		BIT(8)
#define ECC_CTRL_EN_UE_IRQ		BIT(9)

/* ECC error count register definitions */
#define ECC_ERRCNT_UECNT_MASK		GENMASK(31, 16)
#define ECC_ERRCNT_CECNT_MASK		GENMASK(15, 0)

/* ECC Corrected Error register definitions */
#define ECC_CEADDR0_RANK_MASK		GENMASK(27, 24)
#define ECC_CEADDR0_ROW_MASK		GENMASK(17, 0)
#define ECC_CEADDR1_BANKGRP_MASK	GENMASK(25, 24)
#define ECC_CEADDR1_BANK_MASK		GENMASK(23, 16)
#define ECC_CEADDR1_COL_MASK		GENMASK(11, 0)

/* DDR CRC/Parity register definitions */
#define DDR_CRCPARCTL0_CLR_ALRT_ERRCNT	BIT(2)
#define DDR_CRCPARCTL0_CLR_ALRT_ERR	BIT(1)
#define DDR_CRCPARCTL0_EN_ALRT_IRQ	BIT(0)
#define DDR_CRCPARSTAT_ALRT_ERR		BIT(16)
#define DDR_CRCPARSTAT_ALRT_CNT_MASK	GENMASK(15, 0)

/* ECC Poison register definitions */
#define ECC_POISON0_RANK_MASK		GENMASK(27, 24)
#define ECC_POISON0_COL_MASK		GENMASK(11, 0)
#define ECC_POISON1_BANKGRP_MASK	GENMASK(29, 28)
#define ECC_POISON1_BANK_MASK		GENMASK(26, 24)
#define ECC_POISON1_ROW_MASK		GENMASK(17, 0)

/* DDRC address mapping parameters */
#define DDR_ADDRMAP_NREGS		12

#define DDR_MAX_HIF_WIDTH		60
#define DDR_MAX_ROW_WIDTH		18
#define DDR_MAX_COL_WIDTH		14
#define DDR_MAX_BANK_WIDTH		3
#define DDR_MAX_BANKGRP_WIDTH		2
#define DDR_MAX_RANK_WIDTH		2

#define DDR_ADDRMAP_B0_M15		GENMASK(3, 0)
#define DDR_ADDRMAP_B8_M15		GENMASK(11, 8)
#define DDR_ADDRMAP_B16_M15		GENMASK(19, 16)
#define DDR_ADDRMAP_B24_M15		GENMASK(27, 24)

#define DDR_ADDRMAP_B0_M31		GENMASK(4, 0)
#define DDR_ADDRMAP_B8_M31		GENMASK(12, 8)
#define DDR_ADDRMAP_B16_M31		GENMASK(20, 16)
#define DDR_ADDRMAP_B24_M31		GENMASK(28, 24)

#define DDR_ADDRMAP_UNUSED		((u8)-1)
#define DDR_ADDRMAP_MAX_15		DDR_ADDRMAP_B0_M15
#define DDR_ADDRMAP_MAX_31		DDR_ADDRMAP_B0_M31

#define ROW_B0_BASE			6
#define ROW_B1_BASE			7
#define ROW_B2_BASE			8
#define ROW_B3_BASE			9
#define ROW_B4_BASE			10
#define ROW_B5_BASE			11
#define ROW_B6_BASE			12
#define ROW_B7_BASE			13
#define ROW_B8_BASE			14
#define ROW_B9_BASE			15
#define ROW_B10_BASE			16
#define ROW_B11_BASE			17
#define ROW_B12_BASE			18
#define ROW_B13_BASE			19
#define ROW_B14_BASE			20
#define ROW_B15_BASE			21
#define ROW_B16_BASE			22
#define ROW_B17_BASE			23

#define COL_B2_BASE			2
#define COL_B3_BASE			3
#define COL_B4_BASE			4
#define COL_B5_BASE			5
#define COL_B6_BASE			6
#define COL_B7_BASE			7
#define COL_B8_BASE			8
#define COL_B9_BASE			9
#define COL_B10_BASE			10
#define COL_B11_BASE			11
#define COL_B12_BASE			12
#define COL_B13_BASE			13

#define BANK_B0_BASE			2
#define BANK_B1_BASE			3
#define BANK_B2_BASE			4

#define BANKGRP_B0_BASE			2
#define BANKGRP_B1_BASE			3

#define RANK_B0_BASE			6
#define RANK_B1_BASE			7

/* DDRC System Address parameters */
#define DDR_MAX_NSAR			4
#define DDR_MIN_SARSIZE			SZ_256M

/* ECC Scrubber registers definitions */
#define ECC_SBRCTL_SCRUB_INTERVAL	GENMASK(20, 8)
#define ECC_SBRCTL_INTERVAL_STEP	512
#define ECC_SBRCTL_INTERVAL_MIN		0
#define ECC_SBRCTL_INTERVAL_SAFE	1
#define ECC_SBRCTL_INTERVAL_MAX		FIELD_MAX(ECC_SBRCTL_SCRUB_INTERVAL)
#define ECC_SBRCTL_SCRUB_BURST		GENMASK(6, 4)
#define ECC_SBRCTL_SCRUB_MODE_WR	BIT(2)
#define ECC_SBRCTL_SCRUB_EN		BIT(0)
#define ECC_SBRSTAT_SCRUB_DONE		BIT(1)
#define ECC_SBRSTAT_SCRUB_BUSY		BIT(0)

/* ZynqMP DDR QOS Interrupt register definitions */
#define ZYNQMP_DDR_QOS_UE_MASK		BIT(2)
#define ZYNQMP_DDR_QOS_CE_MASK		BIT(1)

/**
 * enum snps_dq_width - SDRAM DQ bus width (ECC capable).
 * @SNPS_DQ_32:	32-bit memory data width.
 * @SNPS_DQ_64:	64-bit memory data width.
 */
enum snps_dq_width {
	SNPS_DQ_32 = 2,
	SNPS_DQ_64 = 3,
};

/**
 * enum snps_dq_mode - SDRAM DQ bus mode.
 * @SNPS_DQ_FULL:	Full DQ bus width.
 * @SNPS_DQ_HALF:	Half DQ bus width.
 * @SNPS_DQ_QRTR:	Quarter DQ bus width.
 */
enum snps_dq_mode {
	SNPS_DQ_FULL = 0,
	SNPS_DQ_HALF = 1,
	SNPS_DQ_QRTR = 2,
};

/**
 * enum snps_burst_length - HIF/SDRAM burst transactions length.
 * @SNPS_DDR_BL2:	Burst length 2xSDRAM-words.
 * @SNPS_DDR_BL4:	Burst length 4xSDRAM-words.
 * @SNPS_DDR_BL8:	Burst length 8xSDRAM-words.
 * @SNPS_DDR_BL16:	Burst length 16xSDRAM-words.
 */
enum snps_burst_length {
	SNPS_DDR_BL2 = 2,
	SNPS_DDR_BL4 = 4,
	SNPS_DDR_BL8 = 8,
	SNPS_DDR_BL16 = 16,
};

/**
 * enum snps_freq_ratio - HIF:SDRAM frequency ratio mode.
 * @SNPS_FREQ_RATIO11:	1:1 frequency mode.
 * @SNPS_FREQ_RATIO12:	1:2 frequency mode.
 */
enum snps_freq_ratio {
	SNPS_FREQ_RATIO11 = 1,
	SNPS_FREQ_RATIO12 = 2,
};

/**
 * enum snps_ecc_mode - ECC mode.
 * @SNPS_ECC_DISABLED:	ECC is disabled/unavailable.
 * @SNPS_ECC_SECDED:	SEC/DED over 1 beat ECC (SideBand/Inline).
 * @SNPS_ECC_ADVX4X8:	Advanced ECC X4/X8 (SideBand).
 */
enum snps_ecc_mode {
	SNPS_ECC_DISABLED = 0,
	SNPS_ECC_SECDED = 4,
	SNPS_ECC_ADVX4X8 = 5,
};

/**
 * enum snps_ref_clk - DW uMCTL2 DDR controller clocks.
 * @SNPS_CSR_CLK:	CSR/APB interface clock.
 * @SNPS_AXI_CLK:	AXI (AHB) Port reference clock.
 * @SNPS_CORE_CLK:	DDR controller (including DFI) clock. SDRAM clock
 *			matches runs with this freq in 1:1 ratio mode and
 *			with twice of this freq in case of 1:2 ratio mode.
 * @SNPS_SBR_CLK:	Scrubber port reference clock (synchronous to
 *			the core clock).
 * @SNPS_MAX_NCLK:	Total number of clocks.
 */
enum snps_ref_clk {
	SNPS_CSR_CLK,
	SNPS_AXI_CLK,
	SNPS_CORE_CLK,
	SNPS_SBR_CLK,
	SNPS_MAX_NCLK
};

/**
 * struct snps_ddrc_info - DDR controller platform parameters.
 * @caps:		DDR controller capabilities.
 * @sdram_mode:		Current SDRAM mode selected.
 * @dev_cfg:		Current memory device config (if applicable).
 * @dq_width:		Memory data bus width (width of the DQ signals
 *			connected to SDRAM chips).
 * @dq_mode:		Proportion of the DQ bus utilized to access SDRAM.
 * @sdram_burst_len:	SDRAM burst transaction length.
 * @hif_burst_len:	HIF burst transaction length (Host Interface).
 * @freq_ratio:		HIF/SDRAM frequency ratio mode.
 * @ecc_mode:		ECC mode enabled for the DDR controller (SEC/DED, etc).
 * @ranks:		Number of ranks enabled to access DIMM (1, 2 or 4).
 */
struct snps_ddrc_info {
	unsigned int caps;
	enum mem_type sdram_mode;
	enum dev_type dev_cfg;
	enum snps_dq_width dq_width;
	enum snps_dq_mode dq_mode;
	enum snps_burst_length sdram_burst_len;
	enum snps_burst_length hif_burst_len;
	enum snps_freq_ratio freq_ratio;
	enum snps_ecc_mode ecc_mode;
	unsigned int ranks;
};

/**
 * struct snps_sys_app_map - System/Application mapping table.
 * @nsar:	Number of SARs enabled on the controller (max 4).
 * @minsize:	Minimal block size (from 256MB to 32GB).
 * @sar.base:	SAR base address aligned to minsize.
 * @sar.size:	SAR size aligned to minsize.
 * @sar.ofst:	SAR address offset.
 */
struct snps_sys_app_map {
	u8 nsar;
	u64 minsize;
	struct {
		u64 base;
		u64 size;
		u64 ofst;
	} sar[DDR_MAX_NSAR];
};

/**
 * struct snps_hif_sdram_map - HIF/SDRAM mapping table.
 * @row:	HIF bit offsets used as row address bits.
 * @col:	HIF bit offsets used as column address bits.
 * @bank:	HIF bit offsets used as bank address bits.
 * @bankgrp:	HIF bit offsets used as bank group address bits.
 * @rank:	HIF bit offsets used as rank address bits.
 *
 * For example, row[0] = 6 means row bit #0 is encoded by the HIF
 * address bit #6 and vice-versa.
 */
struct snps_hif_sdram_map {
	u8 row[DDR_MAX_ROW_WIDTH];
	u8 col[DDR_MAX_COL_WIDTH];
	u8 bank[DDR_MAX_BANK_WIDTH];
	u8 bankgrp[DDR_MAX_BANKGRP_WIDTH];
	u8 rank[DDR_MAX_RANK_WIDTH];
};

/**
 * struct snps_sdram_addr - SDRAM address.
 * @row:	Row number.
 * @col:	Column number.
 * @bank:	Bank number.
 * @bankgrp:	Bank group number.
 * @rank:	Rank number.
 */
struct snps_sdram_addr {
	u16 row;
	u16 col;
	u8 bank;
	u8 bankgrp;
	u8 rank;
};

/**
 * struct snps_ecc_error_info - ECC error log information.
 * @ecnt:	Number of detected errors.
 * @syndrome:	Error syndrome.
 * @sdram:	SDRAM address.
 * @syndrome:	Error syndrome.
 * @bitpos:	Bit position.
 * @data:	Data causing the error.
 * @ecc:	Data ECC.
 */
struct snps_ecc_error_info {
	u16 ecnt;
	struct snps_sdram_addr sdram;
	u32 syndrome;
	u32 bitpos;
	u64 data;
	u32 ecc;
};

/**
 * struct snps_edac_priv - DDR memory controller private data.
 * @info:		DDR controller config info.
 * @sys_app_map:	Sys/App mapping table.
 * @hif_sdram_map:	HIF/SDRAM mapping table.
 * @pdev:		Platform device.
 * @baseaddr:		Base address of the DDR controller.
 * @reglock:		Concurrent CSRs access lock.
 * @clks:		Controller reference clocks.
 * @message:		Buffer for framing the event specific info.
 */
struct snps_edac_priv {
	struct snps_ddrc_info info;
	struct snps_sys_app_map sys_app_map;
	struct snps_hif_sdram_map hif_sdram_map;
	struct platform_device *pdev;
	void __iomem *baseaddr;
	spinlock_t reglock;
	struct clk_bulk_data clks[SNPS_MAX_NCLK];
	char message[SNPS_EDAC_MSG_SIZE];
};

/**
 * snps_map_sys_to_app - Map System address to Application address.
 * @priv:	DDR memory controller private instance data.
 * @sys:	System address (source).
 * @app:	Application address (destination).
 *
 * System address space is used to define disjoint memory regions
 * mapped then to the contiguous application memory space:
 *
 *  System Address Space (SAR) <-> Application Address Space
 *        +------+                        +------+
 *        | SAR0 |----------------------->| Reg0 |
 *        +------+       -offset          +------+
 *        | ...  |           +----------->| Reg1 |
 *        +------+           |            +------+
 *        | SAR1 |-----------+            | ...  |
 *        +------+
 *        | ...  |
 *
 * The translation is done by applying the corresponding SAR offset
 * to the inbound system address. Note according to the hardware reference
 * manual the same mapping is applied to the addresses up to the next
 * SAR base address irrespective to the region size.
 */
static void snps_map_sys_to_app(struct snps_edac_priv *priv,
				dma_addr_t sys, u64 *app)
{
	struct snps_sys_app_map *map = &priv->sys_app_map;
	u64 ofst;
	int i;

	ofst = 0;
	for (i = 0; i < map->nsar; i++) {
		if (sys < map->sar[i].base)
			break;

		ofst = map->sar[i].ofst;
	}

	*app = sys - ofst;
}

/**
 * snps_map_sys_to_app - Map Application address to System address.
 * @priv:	DDR memory controller private instance data.
 * @app:	Application address (source).
 * @sys:	System address (destination).
 *
 * Backward App-to-sys translation is easier because the application address
 * space is contiguous. So we just need to add the offset corresponding
 * to the region the passed address belongs to. Note the later offset is applied
 * to all the addresses above the last available region.
 */
static void snps_map_app_to_sys(struct snps_edac_priv *priv,
				u64 app, dma_addr_t *sys)
{
	struct snps_sys_app_map *map = &priv->sys_app_map;
	u64 ofst, size;
	int i;

	ofst = 0;
	for (i = 0, size = 0; i < map->nsar; i++) {
		ofst = map->sar[i].ofst;
		size += map->sar[i].size;
		if (app < size)
			break;
	}

	*sys = app + ofst;
}

/**
 * snps_map_app_to_hif - Map Application address to HIF address.
 * @priv:	DDR memory controller private instance data.
 * @app:	Application address (source).
 * @hif:	HIF address (destination).
 *
 * HIF address is used to perform the DQ bus width aligned burst transactions.
 * So in order to perform the Application-to-HIF address translation we just
 * need to discard the SDRAM-word bits of the Application address.
 */
static void snps_map_app_to_hif(struct snps_edac_priv *priv,
				u64 app, u64 *hif)
{
	*hif = app >> priv->info.dq_width;
}

/**
 * snps_map_hif_to_app - Map HIF address to Application address.
 * @priv:	DDR memory controller private instance data.
 * @hif:	HIF address (source).
 * @app:	Application address (destination).
 *
 * Backward HIF-to-App translation is just the opposite DQ-width-based
 * shift operation.
 */
static void snps_map_hif_to_app(struct snps_edac_priv *priv,
				u64 hif, u64 *app)
{
	*app = hif << priv->info.dq_width;
}

/**
 * snps_map_hif_to_sdram - Map HIF address to SDRAM address.
 * @priv:	DDR memory controller private instance data.
 * @hif:	HIF address (source).
 * @sdram:	SDRAM address (destination).
 *
 * HIF-SDRAM address mapping is configured with the ADDRMAPx registers, Based
 * on the CSRs value the HIF address bits are mapped to the corresponding bits
 * in the SDRAM rank/bank/column/row. If an SDRAM address bit is unused (there
 * is no any HIF address bit corresponding to it) it will be set to zero. Using
 * this fact we can freely set the output SDRAM address with zeros and walk
 * over the set HIF address bits only. Similarly the unmapped HIF address bits
 * are just ignored.
 */
static void snps_map_hif_to_sdram(struct snps_edac_priv *priv,
				  u64 hif, struct snps_sdram_addr *sdram)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	int i;

	sdram->row = 0;
	for (i = 0; i < DDR_MAX_ROW_WIDTH; i++) {
		if (map->row[i] != DDR_ADDRMAP_UNUSED && hif & BIT(map->row[i]))
			sdram->row |= BIT(i);
	}

	sdram->col = 0;
	for (i = 0; i < DDR_MAX_COL_WIDTH; i++) {
		if (map->col[i] != DDR_ADDRMAP_UNUSED && hif & BIT(map->col[i]))
			sdram->col |= BIT(i);
	}

	sdram->bank = 0;
	for (i = 0; i < DDR_MAX_BANK_WIDTH; i++) {
		if (map->bank[i] != DDR_ADDRMAP_UNUSED && hif & BIT(map->bank[i]))
			sdram->bank |= BIT(i);
	}

	sdram->bankgrp = 0;
	for (i = 0; i < DDR_MAX_BANKGRP_WIDTH; i++) {
		if (map->bankgrp[i] != DDR_ADDRMAP_UNUSED && hif & BIT(map->bankgrp[i]))
			sdram->bankgrp |= BIT(i);
	}

	sdram->rank = 0;
	for (i = 0; i < DDR_MAX_RANK_WIDTH; i++) {
		if (map->rank[i] != DDR_ADDRMAP_UNUSED && hif & BIT(map->rank[i]))
			sdram->rank |= BIT(i);
	}
}

/**
 * snps_map_sdram_to_hif - Map SDRAM address to HIF address.
 * @priv:	DDR memory controller private instance data.
 * @sdram:	SDRAM address (source).
 * @hif:	HIF address (destination).
 *
 * SDRAM-HIF address mapping is similar to the HIF-SDRAM mapping procedure, but
 * we'll traverse each SDRAM rank/bank/column/row bit.
 *
 * Note the unmapped bits of the SDRAM address components will be just
 * ignored. So make sure the source address is valid.
 */
static void snps_map_sdram_to_hif(struct snps_edac_priv *priv,
				  struct snps_sdram_addr *sdram, u64 *hif)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	unsigned long addr;
	int i;

	*hif = 0;

	addr = sdram->row;
	for_each_set_bit(i, &addr, DDR_MAX_ROW_WIDTH) {
		if (map->row[i] != DDR_ADDRMAP_UNUSED)
			*hif |= BIT_ULL(map->row[i]);
	}

	addr = sdram->col;
	for_each_set_bit(i, &addr, DDR_MAX_COL_WIDTH) {
		if (map->col[i] != DDR_ADDRMAP_UNUSED)
			*hif |= BIT_ULL(map->col[i]);
	}

	addr = sdram->bank;
	for_each_set_bit(i, &addr, DDR_MAX_BANK_WIDTH) {
		if (map->bank[i] != DDR_ADDRMAP_UNUSED)
			*hif |= BIT_ULL(map->bank[i]);
	}

	addr = sdram->bankgrp;
	for_each_set_bit(i, &addr, DDR_MAX_BANKGRP_WIDTH) {
		if (map->bankgrp[i] != DDR_ADDRMAP_UNUSED)
			*hif |= BIT_ULL(map->bankgrp[i]);
	}

	addr = sdram->rank;
	for_each_set_bit(i, &addr, DDR_MAX_RANK_WIDTH) {
		if (map->rank[i] != DDR_ADDRMAP_UNUSED)
			*hif |= BIT_ULL(map->rank[i]);
	}
}

/**
 * snps_map_sys_to_sdram - Map System address to SDRAM address.
 * @priv:	DDR memory controller private instance data.
 * @sys:	System address (source).
 * @sdram:	SDRAM address (destination).
 *
 * Perform a full mapping of the system address (detected on the controller
 * ports) to the SDRAM address tuple row/column/bank/etc.
 */
static void snps_map_sys_to_sdram(struct snps_edac_priv *priv,
				  dma_addr_t sys, struct snps_sdram_addr *sdram)
{
	u64 app, hif;

	snps_map_sys_to_app(priv, sys, &app);

	snps_map_app_to_hif(priv, app, &hif);

	snps_map_hif_to_sdram(priv, hif, sdram);
}

/**
 * snps_map_sdram_to_sys - Map SDRAM address to SDRAM address.
 * @priv:	DDR memory controller private instance data.
 * @sys:	System address (source).
 * @sdram:	SDRAM address (destination).
 *
 * Perform a full mapping of the SDRAM address (row/column/bank/etc) to
 * the system address specific to the controller system bus ports.
 */
static void snps_map_sdram_to_sys(struct snps_edac_priv *priv,
				  struct snps_sdram_addr *sdram, dma_addr_t *sys)
{
	u64 app, hif;

	snps_map_sdram_to_hif(priv, sdram, &hif);

	snps_map_hif_to_app(priv, hif, &app);

	snps_map_app_to_sys(priv, app, sys);
}

/**
 * snps_get_bitpos - Get DQ-bus corrected bit position.
 * @syndrome:	Error syndrome.
 * @dq_width:	Controller DQ-bus width.
 *
 * Return: actual corrected DQ-bus bit position starting from 0.
 */
static inline u32 snps_get_bitpos(u32 syndrome, enum snps_dq_width dq_width)
{
	/* ecc[0] bit */
	if (syndrome == 0)
		return BITS_PER_BYTE << dq_width;

	/* ecc[1:x] bit */
	if (is_power_of_2(syndrome))
		return (BITS_PER_BYTE << dq_width) + ilog2(syndrome) + 1;

	/* data[0:y] bit */
	return syndrome - ilog2(syndrome) - 2;
}

/**
 * snps_ce_irq_handler - Corrected error interrupt handler.
 * @irq:        IRQ number.
 * @dev_id:     Device ID.
 *
 * Return: IRQ_NONE, if interrupt not set or IRQ_HANDLED otherwise.
 */
static irqreturn_t snps_ce_irq_handler(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct snps_edac_priv *priv = mci->pvt_info;
	struct snps_ecc_error_info einfo;
	unsigned long flags;
	u32 qosval, regval;
	dma_addr_t sys;

	/* Make sure IRQ is caused by a corrected ECC error */
	if (priv->info.caps & SNPS_CAP_ZYNQMP) {
		qosval = readl(priv->baseaddr + ZYNQMP_DDR_QOS_IRQ_STAT_OFST);
		if (!(qosval & ZYNQMP_DDR_QOS_CE_MASK))
			return IRQ_NONE;

		qosval &= ZYNQMP_DDR_QOS_CE_MASK;
	}

	regval = readl(priv->baseaddr + ECC_STAT_OFST);
	if (!FIELD_GET(ECC_STAT_CE_MASK, regval))
		return IRQ_NONE;

	/* Read error info like syndrome, bit position, SDRAM address, data */
	einfo.syndrome = FIELD_GET(ECC_STAT_BITNUM_MASK, regval);

	einfo.bitpos = snps_get_bitpos(einfo.syndrome, priv->info.dq_width);

	regval = readl(priv->baseaddr + ECC_ERRCNT_OFST);
	einfo.ecnt = FIELD_GET(ECC_ERRCNT_CECNT_MASK, regval);

	regval = readl(priv->baseaddr + ECC_CEADDR0_OFST);
	einfo.sdram.rank = FIELD_GET(ECC_CEADDR0_RANK_MASK, regval);
	einfo.sdram.row = FIELD_GET(ECC_CEADDR0_ROW_MASK, regval);

	regval = readl(priv->baseaddr + ECC_CEADDR1_OFST);
	einfo.sdram.bankgrp = FIELD_GET(ECC_CEADDR1_BANKGRP_MASK, regval);
	einfo.sdram.bank = FIELD_GET(ECC_CEADDR1_BANK_MASK, regval);
	einfo.sdram.col = FIELD_GET(ECC_CEADDR1_COL_MASK, regval);

	einfo.data = readl(priv->baseaddr + ECC_CSYND0_OFST);
	if (priv->info.dq_width == SNPS_DQ_64)
		einfo.data |= (u64)readl(priv->baseaddr + ECC_CSYND1_OFST) << 32;

	einfo.ecc = readl(priv->baseaddr + ECC_CSYND2_OFST);

	/* Report the detected errors with the corresponding sys address */
	snps_map_sdram_to_sys(priv, &einfo.sdram, &sys);

	snprintf(priv->message, SNPS_EDAC_MSG_SIZE,
		 "Row %hu Col %hu Bank %hhu Bank Group %hhu Rank %hhu Bit %d Data 0x%08llx:0x%02x",
		 einfo.sdram.row, einfo.sdram.col, einfo.sdram.bank,
		 einfo.sdram.bankgrp, einfo.sdram.rank,
		 einfo.bitpos, einfo.data, einfo.ecc);

	edac_mc_handle_error(HW_EVENT_ERR_CORRECTED, mci, einfo.ecnt,
			     PHYS_PFN(sys), offset_in_page(sys),
			     einfo.syndrome, einfo.sdram.rank, 0, -1,
			     priv->message, "");

	/* Make sure the CE IRQ status is cleared */
	spin_lock_irqsave(&priv->reglock, flags);

	regval = readl(priv->baseaddr + ECC_CLR_OFST) |
		 ECC_CTRL_CLR_CE_ERR | ECC_CTRL_CLR_CE_ERRCNT;
	writel(regval, priv->baseaddr + ECC_CLR_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);

	if (priv->info.caps & SNPS_CAP_ZYNQMP)
		writel(qosval, priv->baseaddr + ZYNQMP_DDR_QOS_IRQ_STAT_OFST);

	return IRQ_HANDLED;
}

/**
 * snps_ue_irq_handler - Uncorrected error interrupt handler.
 * @irq:        IRQ number.
 * @dev_id:     Device ID.
 *
 * Return: IRQ_NONE, if interrupt not set or IRQ_HANDLED otherwise.
 */
static irqreturn_t snps_ue_irq_handler(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct snps_edac_priv *priv = mci->pvt_info;
	struct snps_ecc_error_info einfo;
	unsigned long flags;
	u32 qosval, regval;
	dma_addr_t sys;

	/* Make sure IRQ is caused by an uncorrected ECC error */
	if (priv->info.caps & SNPS_CAP_ZYNQMP) {
		qosval = readl(priv->baseaddr + ZYNQMP_DDR_QOS_IRQ_STAT_OFST);
		if (!(regval & ZYNQMP_DDR_QOS_UE_MASK))
			return IRQ_NONE;

		qosval &= ZYNQMP_DDR_QOS_UE_MASK;
	}

	regval = readl(priv->baseaddr + ECC_STAT_OFST);
	if (!FIELD_GET(ECC_STAT_UE_MASK, regval))
		return IRQ_NONE;

	/* Read error info like SDRAM address, data and syndrome */
	regval = readl(priv->baseaddr + ECC_ERRCNT_OFST);
	einfo.ecnt = FIELD_GET(ECC_ERRCNT_UECNT_MASK, regval);

	regval = readl(priv->baseaddr + ECC_UEADDR0_OFST);
	einfo.sdram.rank = FIELD_GET(ECC_CEADDR0_RANK_MASK, regval);
	einfo.sdram.row = FIELD_GET(ECC_CEADDR0_ROW_MASK, regval);

	regval = readl(priv->baseaddr + ECC_UEADDR1_OFST);
	einfo.sdram.bankgrp = FIELD_GET(ECC_CEADDR1_BANKGRP_MASK, regval);
	einfo.sdram.bank = FIELD_GET(ECC_CEADDR1_BANK_MASK, regval);
	einfo.sdram.col = FIELD_GET(ECC_CEADDR1_COL_MASK, regval);

	einfo.data = readl(priv->baseaddr + ECC_UESYND0_OFST);
	if (priv->info.dq_width == SNPS_DQ_64)
		einfo.data |= (u64)readl(priv->baseaddr + ECC_UESYND1_OFST) << 32;

	einfo.ecc = readl(priv->baseaddr + ECC_UESYND2_OFST);

	/* Report the detected errors with the corresponding sys address */
	snps_map_sdram_to_sys(priv, &einfo.sdram, &sys);

	snprintf(priv->message, SNPS_EDAC_MSG_SIZE,
		 "Row %hu Col %hu Bank %hhu Bank Group %hhu Rank %hhu Data 0x%08llx:0x%02x",
		 einfo.sdram.row, einfo.sdram.col, einfo.sdram.bank,
		 einfo.sdram.bankgrp, einfo.sdram.rank,
		 einfo.data, einfo.ecc);

	edac_mc_handle_error(HW_EVENT_ERR_UNCORRECTED, mci, einfo.ecnt,
			     PHYS_PFN(sys), offset_in_page(sys),
			     0, einfo.sdram.rank, 0, -1,
			     priv->message, "");

	/* Make sure the UE IRQ status is cleared */
	spin_lock_irqsave(&priv->reglock, flags);

	regval = readl(priv->baseaddr + ECC_CLR_OFST) |
		 ECC_CTRL_CLR_UE_ERR | ECC_CTRL_CLR_UE_ERRCNT;
	writel(regval, priv->baseaddr + ECC_CLR_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);

	if (priv->info.caps & SNPS_CAP_ZYNQMP)
		writel(qosval, priv->baseaddr + ZYNQMP_DDR_QOS_IRQ_STAT_OFST);

	return IRQ_HANDLED;
}

/**
 * snps_dfi_irq_handler - DFI CRC/Parity error interrupt handler.
 * @irq:        IRQ number.
 * @dev_id:     Device ID.
 *
 * Return: IRQ_NONE, if interrupt not set or IRQ_HANDLED otherwise.
 */
static irqreturn_t snps_dfi_irq_handler(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct snps_edac_priv *priv = mci->pvt_info;
	unsigned long flags;
	u32 regval;
	u16 ecnt;

	/* Make sure IRQ is caused by an DFI alert error */
	regval = readl(priv->baseaddr + DDR_CRCPARSTAT_OFST);
	if (!(regval & DDR_CRCPARSTAT_ALRT_ERR))
		return IRQ_NONE;

	/* Just a number of CRC/Parity errors is available */
	ecnt = FIELD_GET(DDR_CRCPARSTAT_ALRT_CNT_MASK, regval);

	/* Report the detected errors with just the custom message */
	snprintf(priv->message, SNPS_EDAC_MSG_SIZE,
		 "DFI CRC/Parity error detected on dfi_alert_n");

	edac_mc_handle_error(HW_EVENT_ERR_FATAL, mci, ecnt,
			     0, 0, 0, 0, 0, -1, priv->message, "");

	/* Make sure the DFI alert IRQ status is cleared */
	spin_lock_irqsave(&priv->reglock, flags);

	regval = readl(priv->baseaddr + DDR_CRCPARCTL0_OFST) |
		 DDR_CRCPARCTL0_CLR_ALRT_ERR | DDR_CRCPARCTL0_CLR_ALRT_ERRCNT;
	writel(regval, priv->baseaddr + DDR_CRCPARCTL0_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);

	return IRQ_HANDLED;
}

/**
 * snps_sbr_irq_handler - Scrubber Done interrupt handler.
 * @irq:        IRQ number.
 * @dev_id:     Device ID.
 *
 * It just checks whether the IRQ has been caused by the Scrubber Done event
 * and disables the back-to-back scrubbing by falling back to the smallest
 * delay between the Scrubber read commands.
 *
 * Return: IRQ_NONE, if interrupt not set or IRQ_HANDLED otherwise.
 */
static irqreturn_t snps_sbr_irq_handler(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct snps_edac_priv *priv = mci->pvt_info;
	unsigned long flags;
	u32 regval, en;

	/* Make sure IRQ is caused by the Scrubber Done event */
	regval = readl(priv->baseaddr + ECC_SBRSTAT_OFST);
	if (!(regval & ECC_SBRSTAT_SCRUB_DONE))
		return IRQ_NONE;

	spin_lock_irqsave(&priv->reglock, flags);

	regval = readl(priv->baseaddr + ECC_SBRCTL_OFST);
	en = regval & ECC_SBRCTL_SCRUB_EN;
	writel(regval & ~en, priv->baseaddr + ECC_SBRCTL_OFST);

	regval = FIELD_PREP(ECC_SBRCTL_SCRUB_INTERVAL, ECC_SBRCTL_INTERVAL_SAFE);
	writel(regval, priv->baseaddr + ECC_SBRCTL_OFST);

	writel(regval | en, priv->baseaddr + ECC_SBRCTL_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);

	edac_mc_printk(mci, KERN_WARNING, "Back-to-back scrubbing disabled\n");

	return IRQ_HANDLED;
}

/**
 * snps_com_irq_handler - Interrupt IRQ signal handler.
 * @irq:        IRQ number.
 * @dev_id:     Device ID.
 *
 * Return: IRQ_NONE, if interrupts not set or IRQ_HANDLED otherwise.
 */
static irqreturn_t snps_com_irq_handler(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;
	struct snps_edac_priv *priv = mci->pvt_info;
	irqreturn_t rc = IRQ_NONE;

	rc |= snps_ce_irq_handler(irq, dev_id);

	rc |= snps_ue_irq_handler(irq, dev_id);

	rc |= snps_dfi_irq_handler(irq, dev_id);

	if (priv->info.caps & SNPS_CAP_ECC_SCRUBBER)
		rc |= snps_sbr_irq_handler(irq, dev_id);

	return rc;
}

static void snps_enable_irq(struct snps_edac_priv *priv)
{
	unsigned long flags;

	/* Enable UE/CE Interrupts */
	if (priv->info.caps & SNPS_CAP_ZYNQMP) {
		writel(ZYNQMP_DDR_QOS_UE_MASK | ZYNQMP_DDR_QOS_CE_MASK,
		       priv->baseaddr + ZYNQMP_DDR_QOS_IRQ_EN_OFST);

		return;
	}

	spin_lock_irqsave(&priv->reglock, flags);

	/*
	 * IRQs Enable/Disable flags have been available since v3.10a.
	 * This is noop for the older controllers.
	 */
	writel(ECC_CTRL_EN_CE_IRQ | ECC_CTRL_EN_UE_IRQ,
	       priv->baseaddr + ECC_CLR_OFST);

	/*
	 * CRC/Parity interrupts control has been available since v2.10a.
	 * This is noop for the older controllers.
	 */
	writel(DDR_CRCPARCTL0_EN_ALRT_IRQ,
	       priv->baseaddr + DDR_CRCPARCTL0_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);
}

static void snps_disable_irq(struct snps_edac_priv *priv)
{
	unsigned long flags;

	/* Disable UE/CE Interrupts */
	if (priv->info.caps & SNPS_CAP_ZYNQMP) {
		writel(ZYNQMP_DDR_QOS_UE_MASK | ZYNQMP_DDR_QOS_CE_MASK,
		       priv->baseaddr + ZYNQMP_DDR_QOS_IRQ_DB_OFST);

		return;
	}

	spin_lock_irqsave(&priv->reglock, flags);

	writel(0, priv->baseaddr + ECC_CLR_OFST);
	writel(0, priv->baseaddr + DDR_CRCPARCTL0_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);
}

/**
 * snps_get_sdram_bw - Get SDRAM bandwidth.
 * @priv:	DDR memory controller private instance data.
 *
 * The SDRAM interface bandwidth is calculated based on the DDRC Core clock rate
 * and the DW uMCTL2 IP-core parameters like DQ-bus width and mode and
 * Core/SDRAM clocks frequency ratio. Note it returns the theoretical bandwidth
 * which in reality is hardly possible to reach.
 *
 * Return: SDRAM bandwidth or zero if no Core clock specified.
 */
static u64 snps_get_sdram_bw(struct snps_edac_priv *priv)
{
	unsigned long rate;

	/*
	 * Depending on the ratio mode the SDRAM clock either matches the Core
	 * clock or runs with the twice its frequency.
	 */
	rate = clk_get_rate(priv->clks[SNPS_CORE_CLK].clk);
	rate *= priv->info.freq_ratio;

	/*
	 * Scale up by 2 since it's DDR (Double Data Rate) and subtract the
	 * DQ-mode since in non-Full mode only a part of the DQ-bus is utilised
	 * on each SDRAM clock edge.
	 */
	return (2U << (priv->info.dq_width - priv->info.dq_mode)) * (u64)rate;
}

/**
 * snps_get_scrub_bw - Get Scrubber bandwidth.
 * @priv:	DDR memory controller private instance data.
 * @interval:	Scrub interval.
 *
 * DW uMCTL2 DDRC Scrubber performs periodical progressive burst reads (RMW if
 * ECC CE is detected) commands from the whole memory space. The read commands
 * can be delayed by means of the SBRCTL.scrub_interval field. The Scrubber
 * cycles look as follows:
 *
 * |-HIF-burst-read-|-------delay-------|-HIF-burst-read-|------- etc
 *
 * Tb = Bl*[DQ]/Bw[RAM], Td = 512*interval/Fc - periods of the HIF-burst-read
 * and delay stages, where
 * Bl - HIF burst length, [DQ] - Full DQ-bus width, Bw[RAM] - SDRAM bandwidth,
 * Fc - Core clock frequency (Scrubber and Core clocks are synchronous).
 *
 * After some simple calculations the expressions above can be used to get the
 * next Scrubber bandwidth formulae:
 *
 * Bw[Sbr] = Bw[RAM] / (1 + F * interval), where
 * F = 2 * 512 * Fr * Fc * [DQ]e - interval scale factor with
 * Fr - HIF/SDRAM clock frequency ratio (1 or 2), [DQ]e - DQ-bus width mode.
 *
 * Return: Scrubber bandwidth or zero if no Core clock specified.
 */
static u64 snps_get_scrub_bw(struct snps_edac_priv *priv, u32 interval)
{
	unsigned long fac;
	u64 bw_ram;

	fac = (2 * ECC_SBRCTL_INTERVAL_STEP * priv->info.freq_ratio) /
	      (priv->info.hif_burst_len * (1UL << priv->info.dq_mode));

	bw_ram = snps_get_sdram_bw(priv);

	return div_u64(bw_ram, 1 + fac * interval);
}

/**
 * snps_get_scrub_interval - Get Scrubber delay interval.
 * @priv:	DDR memory controller private instance data.
 * @bw:		Scrubber bandwidth.
 *
 * Similarly to the Scrubber bandwidth the interval formulae can be inferred
 * from the same expressions:
 *
 * interval = (Bw[RAM] - Bw[Sbr]) / (F * Bw[Sbr])
 *
 * Return: Scrubber delay interval or zero if no Core clock specified.
 */
static u32 snps_get_scrub_interval(struct snps_edac_priv *priv, u32 bw)
{
	unsigned long fac;
	u64 bw_ram;

	fac = (2 * priv->info.freq_ratio * ECC_SBRCTL_INTERVAL_STEP) /
	      (priv->info.hif_burst_len * (1UL << priv->info.dq_mode));

	bw_ram = snps_get_sdram_bw(priv);

	/* Divide twice so not to cause the integer overflow in (fac * bw) */
	return div_u64(div_u64(bw_ram - bw, bw), fac);
}

/**
 * snps_set_sdram_scrub_rate - Set the Scrubber bandwidth.
 * @mci:	EDAC memory controller instance.
 * @bw:		Bandwidth.
 *
 * It calculates the delay between the Scrubber read commands based on the
 * specified bandwidth and the Core clock rate. If the Core clock is unavailable
 * the passed bandwidth will be directly used as the interval value.
 *
 * Note the method warns about the back-to-back scrubbing since it may
 * significantly degrade the system performance. This mode is supposed to be
 * used for a single SDRAM scrubbing pass only. So it will be turned off in the
 * Scrubber Done IRQ handler.
 *
 * Return: Actually set bandwidth (interval-based approximated bandwidth if the
 * Core clock is unavailable) or zero if the Scrubber was disabled.
 */
static int snps_set_sdram_scrub_rate(struct mem_ctl_info *mci, u32 bw)
{
	struct snps_edac_priv *priv = mci->pvt_info;
	u32 regval, interval;
	unsigned long flags;
	u64 bw_min, bw_max;

	/* Don't bother with the calculations just disable and return. */
	if (!bw) {
		spin_lock_irqsave(&priv->reglock, flags);

		regval = readl(priv->baseaddr + ECC_SBRCTL_OFST);
		regval &= ~ECC_SBRCTL_SCRUB_EN;
		writel(regval, priv->baseaddr + ECC_SBRCTL_OFST);

		spin_unlock_irqrestore(&priv->reglock, flags);

		return 0;
	}

	/* If no Core clock specified fallback to the direct interval setup. */
	bw_max = snps_get_scrub_bw(priv, ECC_SBRCTL_INTERVAL_MIN);
	if (bw_max) {
		bw_min = snps_get_scrub_bw(priv, ECC_SBRCTL_INTERVAL_MAX);
		bw = clamp_t(u64, bw, bw_min, bw_max);

		interval = snps_get_scrub_interval(priv, bw);
	} else {
		bw = clamp_val(bw, ECC_SBRCTL_INTERVAL_MIN, ECC_SBRCTL_INTERVAL_MAX);

		interval = ECC_SBRCTL_INTERVAL_MAX - bw;
	}

	/*
	 * SBRCTL.scrub_en bitfield must be accessed separately from the other
	 * CSR bitfields. It means the flag must be set/cleared with no updates
	 * to the rest of the fields.
	 */
	spin_lock_irqsave(&priv->reglock, flags);

	regval = FIELD_PREP(ECC_SBRCTL_SCRUB_INTERVAL, interval);
	writel(regval, priv->baseaddr + ECC_SBRCTL_OFST);

	writel(regval | ECC_SBRCTL_SCRUB_EN, priv->baseaddr + ECC_SBRCTL_OFST);

	spin_unlock_irqrestore(&priv->reglock, flags);

	if (!interval)
		edac_mc_printk(mci, KERN_WARNING, "Back-to-back scrubbing enabled\n");

	if (!bw_max)
		return interval ? bw : INT_MAX;

	return snps_get_scrub_bw(priv, interval);
}

/**
 * snps_get_sdram_scrub_rate - Get the Scrubber bandwidth.
 * @mci:	EDAC memory controller instance.
 *
 * Return: Scrubber bandwidth (interval-based approximated bandwidth if the
 * Core clock is unavailable) or zero if the Scrubber was disabled.
 */
static int snps_get_sdram_scrub_rate(struct mem_ctl_info *mci)
{
	struct snps_edac_priv *priv = mci->pvt_info;
	u32 regval;
	u64 bw;

	regval = readl(priv->baseaddr + ECC_SBRCTL_OFST);
	if (!(regval & ECC_SBRCTL_SCRUB_EN))
		return 0;

	regval = FIELD_GET(ECC_SBRCTL_SCRUB_INTERVAL, regval);

	bw = snps_get_scrub_bw(priv, regval);
	if (!bw)
		return regval ? ECC_SBRCTL_INTERVAL_MAX - regval : INT_MAX;

	return bw;
}

/**
 * snps_create_data - Create private data.
 * @pdev:	platform device.
 *
 * Return: Private data instance or negative errno.
 */
static struct snps_edac_priv *snps_create_data(struct platform_device *pdev)
{
	struct snps_edac_priv *priv;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	priv->baseaddr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->baseaddr))
		return ERR_CAST(priv->baseaddr);

	priv->pdev = pdev;
	spin_lock_init(&priv->reglock);

	return priv;
}

/**
 * snps_get_res - Get platform device resources.
 * @priv:	DDR memory controller private instance data.
 *
 * It's supposed to request all the controller resources available for the
 * particular platform and enable all the required for the driver normal
 * work. Note only the CSR and Scrubber clocks are supposed to be switched
 * on/off by the driver.
 *
 * Return: negative errno if failed to get the resources, otherwise - zero.
 */
static int snps_get_res(struct snps_edac_priv *priv)
{
	const char * const ids[] = {
		[SNPS_CSR_CLK] = "pclk",
		[SNPS_AXI_CLK] = "aclk",
		[SNPS_CORE_CLK] = "core",
		[SNPS_SBR_CLK] = "sbr",
	};
	int i, rc;

	for (i = 0; i < SNPS_MAX_NCLK; i++)
		priv->clks[i].id = ids[i];

	rc = devm_clk_bulk_get_optional(&priv->pdev->dev, SNPS_MAX_NCLK,
					priv->clks);
	if (rc) {
		edac_printk(KERN_INFO, EDAC_MC, "Failed to get ref clocks\n");
		return rc;
	}

	/*
	 * Don't touch the Core and AXI clocks since they are critical for the
	 * stable system functioning and are supposed to have been enabled
	 * anyway.
	 */
	rc = clk_prepare_enable(priv->clks[SNPS_CSR_CLK].clk);
	if (rc) {
		edac_printk(KERN_INFO, EDAC_MC, "Couldn't enable CSR clock\n");
		return rc;
	}

	rc = clk_prepare_enable(priv->clks[SNPS_SBR_CLK].clk);
	if (rc) {
		edac_printk(KERN_INFO, EDAC_MC, "Couldn't enable Scrubber clock\n");
		goto err_disable_pclk;
	}

	return 0;

err_disable_pclk:
	clk_disable_unprepare(priv->clks[SNPS_CSR_CLK].clk);

	return rc;
}

/**
 * snps_put_res - Put platform device resources.
 * @priv:	DDR memory controller private instance data.
 */
static void snps_put_res(struct snps_edac_priv *priv)
{
	clk_disable_unprepare(priv->clks[SNPS_SBR_CLK].clk);

	clk_disable_unprepare(priv->clks[SNPS_CSR_CLK].clk);
}

/*
 * zynqmp_init_plat - ZynqMP-specific platform initialization.
 * @priv:	DDR memory controller private data.
 *
 * Return: always zero.
 */
static int zynqmp_init_plat(struct snps_edac_priv *priv)
{
	priv->info.caps |= SNPS_CAP_ZYNQMP;
	priv->info.dq_width = SNPS_DQ_64;

	return 0;
}

/*
 * bt1_init_plat - Baikal-T1-specific platform initialization.
 * @priv:	DDR memory controller private data.
 *
 * Return: always zero.
 */
static int bt1_init_plat(struct snps_edac_priv *priv)
{
	priv->info.dq_width = SNPS_DQ_32;
	priv->info.hif_burst_len = SNPS_DDR_BL8;
	priv->sys_app_map.minsize = SZ_256M;

	return 0;
}

/**
 * snps_get_dtype - Return the controller memory width.
 * @mstr:	Master CSR value.
 *
 * Get the EDAC device type width appropriate for the current controller
 * configuration.
 *
 * Return: a device type width enumeration.
 */
static inline enum dev_type snps_get_dtype(u32 mstr)
{
	if (!(mstr & DDR_MSTR_MEM_DDR4))
		return DEV_UNKNOWN;

	switch (FIELD_GET(DDR_MSTR_DEV_CFG_MASK, mstr)) {
	case DDR_MSTR_DEV_X4:
		return DEV_X4;
	case DDR_MSTR_DEV_X8:
		return DEV_X8;
	case DDR_MSTR_DEV_X16:
		return DEV_X16;
	case DDR_MSTR_DEV_X32:
		return DEV_X32;
	}

	return DEV_UNKNOWN;
}

/**
 * snps_get_mtype - Returns controller memory type.
 * @mstr:	Master CSR value.
 *
 * Get the EDAC memory type appropriate for the current controller
 * configuration.
 *
 * Return: a memory type enumeration.
 */
static inline enum mem_type snps_get_mtype(u32 mstr)
{
	switch (FIELD_GET(DDR_MSTR_MEM_MASK, mstr)) {
	case DDR_MSTR_MEM_DDR2:
		return MEM_DDR2;
	case DDR_MSTR_MEM_DDR3:
		return MEM_DDR3;
	case DDR_MSTR_MEM_LPDDR:
		return MEM_LPDDR;
	case DDR_MSTR_MEM_LPDDR2:
		return MEM_LPDDR2;
	case DDR_MSTR_MEM_LPDDR3:
		return MEM_LPDDR3;
	case DDR_MSTR_MEM_DDR4:
		return MEM_DDR4;
	case DDR_MSTR_MEM_LPDDR4:
		return MEM_LPDDR4;
	}

	return MEM_RESERVED;
}

/**
 * snps_get_ddrc_info - Get the DDR controller config data.
 * @priv:	DDR memory controller private data.
 *
 * Return: negative errno if no ECC detected, otherwise - zero.
 */
static int snps_get_ddrc_info(struct snps_edac_priv *priv)
{
	int (*init_plat)(struct snps_edac_priv *priv);
	u32 regval;

	/* Before getting the DDRC parameters make sure ECC is enabled */
	regval = readl(priv->baseaddr + ECC_CFG0_OFST);

	priv->info.ecc_mode = FIELD_GET(ECC_CFG0_MODE_MASK, regval);
	if (priv->info.ecc_mode != SNPS_ECC_SECDED) {
		edac_printk(KERN_INFO, EDAC_MC, "SEC/DED ECC not enabled\n");
		return -ENXIO;
	}

	/* Assume HW-src scrub is always available if it isn't disabled */
	if (!(regval & ECC_CFG0_DIS_SCRUB))
		priv->info.caps |= SNPS_CAP_ECC_SCRUB;

	/* Auto-detect the scrubber by writing to the SBRWDATA0 CSR */
	regval = readl(priv->baseaddr + ECC_SBRWDATA0_OFST);
	writel(~regval, priv->baseaddr + ECC_SBRWDATA0_OFST);
	if (regval != readl(priv->baseaddr + ECC_SBRWDATA0_OFST)) {
		priv->info.caps |= SNPS_CAP_ECC_SCRUBBER;
		writel(regval, priv->baseaddr + ECC_SBRWDATA0_OFST);
	}

	/* Auto-detect the basic HIF/SDRAM bus parameters */
	regval = readl(priv->baseaddr + DDR_MSTR_OFST);

	priv->info.sdram_mode = snps_get_mtype(regval);
	priv->info.dev_cfg = snps_get_dtype(regval);

	priv->info.dq_mode = FIELD_GET(DDR_MSTR_BUSWIDTH_MASK, regval);

	/*
	 * Assume HIF burst length matches the SDRAM burst length since it's
	 * not auto-detectable
	 */
	priv->info.sdram_burst_len = FIELD_GET(DDR_MSTR_BURST_RDWR, regval) << 1;
	priv->info.hif_burst_len = priv->info.sdram_burst_len;

	/* Retrieve the current HIF/SDRAM frequency ratio: 1:1 vs 1:2 */
	priv->info.freq_ratio = !(regval & DDR_MSTR_FREQ_RATIO11) + 1;

	/* Activated ranks field: set bit corresponds to populated rank */
	priv->info.ranks = FIELD_GET(DDR_MSTR_ACT_RANKS_MASK, regval);
	priv->info.ranks = hweight_long(priv->info.ranks);

	/* Auto-detect the DQ bus width by using the ECC-poison pattern CSR */
	writel(0, priv->baseaddr + DDR_SWCTL);

	/*
	 * If poison pattern [32:64] is changeable then DQ is 64-bit wide.
	 * Note the feature has been available since IP-core v2.51a.
	 */
	regval = readl(priv->baseaddr + ECC_POISONPAT1_OFST);
	writel(~regval, priv->baseaddr + ECC_POISONPAT1_OFST);
	if (regval != readl(priv->baseaddr + ECC_POISONPAT1_OFST)) {
		priv->info.dq_width = SNPS_DQ_64;
		writel(regval, priv->baseaddr + ECC_POISONPAT1_OFST);
	} else {
		priv->info.dq_width = SNPS_DQ_32;
	}

	writel(1, priv->baseaddr + DDR_SWCTL);

	/* Apply platform setups after all the configs auto-detection */
	init_plat = device_get_match_data(&priv->pdev->dev);

	return init_plat ? init_plat(priv) : 0;
}

/**
 * snps_get_sys_app_map - Get System/Application address map.
 * @priv:	DDR memory controller private instance data.
 * @sarregs:	Array with SAR registers value.
 *
 * System address regions are defined by the SARBASEn and SARSIZEn registers.
 * Controller reference manual requires the base addresses and sizes creating
 * a set of ascending non-overlapped regions in order to have a linear
 * application address space. Doing otherwise causes unpredictable results.
 */
static void snps_get_sys_app_map(struct snps_edac_priv *priv, u32 *sarregs)
{
	struct snps_sys_app_map *map = &priv->sys_app_map;
	int i, ofst;

	/*
	 * SARs are supposed to be initialized in the ascending non-overlapped
	 * order: base[i - 1] < base[i] < etc. If that rule is broken for a SAR
	 * it's considered as no more SARs have been enabled, so the detection
	 * procedure will halt. Having the very first SAR with zero base
	 * address only makes sense if there is a consequent SAR.
	 */
	for (i = 0, ofst = 0; i < DDR_MAX_NSAR; i++) {
		map->sar[i].base = sarregs[2 * i] * map->minsize;
		if (map->sar[i].base)
			map->nsar = i + 1;
		else if (i && map->sar[i].base <= map->sar[i - 1].base)
			break;

		map->sar[i].size = (sarregs[2 * i + 1] + 1) * map->minsize;
		map->sar[i].ofst = map->sar[i].base - ofst;
		ofst += map->sar[i].size;
	}

	/*
	 * SAR block size isn't auto-detectable. If one isn't specified for the
	 * platform there is a good chance to have invalid mapping of the
	 * detected SARs. So proceed with 1:1 mapping then.
	 */
	if (!map->minsize && map->nsar) {
		edac_printk(KERN_WARNING, EDAC_MC,
			    "No block size specified. Discard SARs mapping\n");
		map->nsar = 0;
	}
}

/**
 * snps_get_hif_row_map - Get HIF/SDRAM-row address map.
 * @priv:	DDR memory controller private instance data.
 * @addrmap:	Array with ADDRMAP registers value.
 *
 * SDRAM-row address is defined by the fields in the ADDRMAP[5-7,9-11]
 * registers. Those fields value indicate the HIF address bits used to encode
 * the DDR row address.
 */
static void snps_get_hif_row_map(struct snps_edac_priv *priv, u32 *addrmap)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	u8 map_row_b2_10;
	int i;

	for (i = 0; i < DDR_MAX_ROW_WIDTH; i++)
		map->row[i] = DDR_ADDRMAP_UNUSED;

	map->row[0] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[5]) + ROW_B0_BASE;
	map->row[1] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[5]) + ROW_B1_BASE;

	map_row_b2_10 = FIELD_GET(DDR_ADDRMAP_B16_M15, addrmap[5]);
	if (map_row_b2_10 != DDR_ADDRMAP_MAX_15) {
		for (i = 2; i < 11; i++)
			map->row[i] = map_row_b2_10 + i + ROW_B0_BASE;
	} else {
		map->row[2] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[9]) + ROW_B2_BASE;
		map->row[3] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[9]) + ROW_B3_BASE;
		map->row[4] = FIELD_GET(DDR_ADDRMAP_B16_M15, addrmap[9]) + ROW_B4_BASE;
		map->row[5] = FIELD_GET(DDR_ADDRMAP_B24_M15, addrmap[9]) + ROW_B5_BASE;
		map->row[6] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[10]) + ROW_B6_BASE;
		map->row[7] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[10]) + ROW_B7_BASE;
		map->row[8] = FIELD_GET(DDR_ADDRMAP_B16_M15, addrmap[10]) + ROW_B8_BASE;
		map->row[9] = FIELD_GET(DDR_ADDRMAP_B24_M15, addrmap[10]) + ROW_B9_BASE;
		map->row[10] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[11]) + ROW_B10_BASE;
	}

	map->row[11] = FIELD_GET(DDR_ADDRMAP_B24_M15, addrmap[5]);
	map->row[11] = map->row[11] == DDR_ADDRMAP_MAX_15 ?
		       DDR_ADDRMAP_UNUSED : map->row[11] + ROW_B11_BASE;

	map->row[12] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[6]);
	map->row[12] = map->row[12] == DDR_ADDRMAP_MAX_15 ?
		       DDR_ADDRMAP_UNUSED : map->row[12] + ROW_B12_BASE;

	map->row[13] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[6]);
	map->row[13] = map->row[13] == DDR_ADDRMAP_MAX_15 ?
		       DDR_ADDRMAP_UNUSED : map->row[13] + ROW_B13_BASE;

	map->row[14] = FIELD_GET(DDR_ADDRMAP_B16_M15, addrmap[6]);
	map->row[14] = map->row[14] == DDR_ADDRMAP_MAX_15 ?
		       DDR_ADDRMAP_UNUSED : map->row[14] + ROW_B14_BASE;

	map->row[15] = FIELD_GET(DDR_ADDRMAP_B24_M15, addrmap[6]);
	map->row[15] = map->row[15] == DDR_ADDRMAP_MAX_15 ?
		       DDR_ADDRMAP_UNUSED : map->row[15] + ROW_B15_BASE;

	if (priv->info.sdram_mode == MEM_DDR4 || priv->info.sdram_mode == MEM_LPDDR4) {
		map->row[16] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[7]);
		map->row[16] = map->row[16] == DDR_ADDRMAP_MAX_15 ?
			       DDR_ADDRMAP_UNUSED : map->row[16] + ROW_B16_BASE;

		map->row[17] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[7]);
		map->row[17] = map->row[17] == DDR_ADDRMAP_MAX_15 ?
			       DDR_ADDRMAP_UNUSED : map->row[17] + ROW_B17_BASE;
	}
}

/**
 * snps_get_hif_col_map - Get HIF/SDRAM-column address map.
 * @priv:	DDR memory controller private instance data.
 * @addrmap:	Array with ADDRMAP registers value.
 *
 * SDRAM-column address is defined by the fields in the ADDRMAP[2-4]
 * registers. Those fields value indicate the HIF address bits used to encode
 * the DDR row address.
 */
static void snps_get_hif_col_map(struct snps_edac_priv *priv, u32 *addrmap)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	int i;

	for (i = 0; i < DDR_MAX_COL_WIDTH; i++)
		map->col[i] = DDR_ADDRMAP_UNUSED;

	map->col[0] = 0;
	map->col[1] = 1;
	map->col[2] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[2]) + COL_B2_BASE;
	map->col[3] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[2]) + COL_B3_BASE;

	map->col[4] = FIELD_GET(DDR_ADDRMAP_B16_M15, addrmap[2]);
	map->col[4] = map->col[4] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[4] + COL_B4_BASE;

	map->col[5] = FIELD_GET(DDR_ADDRMAP_B24_M15, addrmap[2]);
	map->col[5] = map->col[5] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[5] + COL_B5_BASE;

	map->col[6] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[3]);
	map->col[6] = map->col[6] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[6] + COL_B6_BASE;

	map->col[7] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[3]);
	map->col[7] = map->col[7] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[7] + COL_B7_BASE;

	map->col[8] = FIELD_GET(DDR_ADDRMAP_B16_M15, addrmap[3]);
	map->col[8] = map->col[8] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[8] + COL_B8_BASE;

	map->col[9] = FIELD_GET(DDR_ADDRMAP_B24_M15, addrmap[3]);
	map->col[9] = map->col[9] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[9] + COL_B9_BASE;

	map->col[10] = FIELD_GET(DDR_ADDRMAP_B0_M15, addrmap[4]);
	map->col[10] = map->col[10] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[10] + COL_B10_BASE;

	map->col[11] = FIELD_GET(DDR_ADDRMAP_B8_M15, addrmap[4]);
	map->col[11] = map->col[11] == DDR_ADDRMAP_MAX_15 ?
		      DDR_ADDRMAP_UNUSED : map->col[11] + COL_B11_BASE;

	/*
	 * In case of the non-Full DQ bus mode the lowest columns are
	 * unmapped and used by the controller to read the full DQ word
	 * in multiple cycles (col[0] for the Half bus mode, col[0:1] for
	 * the Quarter bus mode).
	 */
	if (priv->info.dq_mode) {
		for (i = 11 + priv->info.dq_mode; i >= priv->info.dq_mode; i--) {
			map->col[i] = map->col[i - priv->info.dq_mode];
			map->col[i - priv->info.dq_mode] = DDR_ADDRMAP_UNUSED;
		}
	}

	/*
	 * Per JEDEC DDR2/3/4/mDDR specification, column address bit 10 is
	 * reserved for indicating auto-precharge, and hence no source
	 * address bit can be mapped to col[10].
	 */
	if (priv->info.sdram_mode == MEM_LPDDR || priv->info.sdram_mode == MEM_DDR2 ||
	    priv->info.sdram_mode == MEM_DDR3 || priv->info.sdram_mode == MEM_DDR4) {
		for (i = 12 + priv->info.dq_mode; i > 10; i--) {
			map->col[i] = map->col[i - 1];
			map->col[i - 1] = DDR_ADDRMAP_UNUSED;
		}
	}

	/*
	 * Per JEDEC specification, column address bit 12 is reserved
	 * for the Burst-chop status, so no source address bit mapping
	 * for col[12] either.
	 */
	map->col[13] = map->col[12];
	map->col[12] = DDR_ADDRMAP_UNUSED;
}

/**
 * snps_get_hif_bank_map - Get HIF/SDRAM-bank address map.
 * @priv:	DDR memory controller private instance data.
 * @addrmap:	Array with ADDRMAP registers value.
 *
 * SDRAM-bank address is defined by the fields in the ADDRMAP[1]
 * register. Those fields value indicate the HIF address bits used to encode
 * the DDR bank address.
 */
static void snps_get_hif_bank_map(struct snps_edac_priv *priv, u32 *addrmap)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	int i;

	for (i = 0; i < DDR_MAX_BANK_WIDTH; i++)
		map->bank[i] = DDR_ADDRMAP_UNUSED;

	map->bank[0] = FIELD_GET(DDR_ADDRMAP_B0_M31, addrmap[1]) + BANK_B0_BASE;
	map->bank[1] = FIELD_GET(DDR_ADDRMAP_B8_M31, addrmap[1]) + BANK_B1_BASE;

	map->bank[2] = FIELD_GET(DDR_ADDRMAP_B16_M31, addrmap[1]);
	map->bank[2] = map->bank[2] == DDR_ADDRMAP_MAX_31 ?
		       DDR_ADDRMAP_UNUSED : map->bank[2] + BANK_B2_BASE;
}

/**
 * snps_get_hif_bankgrp_map - Get HIF/SDRAM-bank group address map.
 * @priv:	DDR memory controller private instance data.
 * @addrmap:	Array with ADDRMAP registers value.
 *
 * SDRAM-bank group address is defined by the fields in the ADDRMAP[8]
 * register. Those fields value indicate the HIF address bits used to encode
 * the DDR bank group address.
 */
static void snps_get_hif_bankgrp_map(struct snps_edac_priv *priv, u32 *addrmap)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	int i;

	for (i = 0; i < DDR_MAX_BANKGRP_WIDTH; i++)
		map->bankgrp[i] = DDR_ADDRMAP_UNUSED;

	/* Bank group signals are available on the DDR4 memory only */
	if (priv->info.sdram_mode != MEM_DDR4)
		return;

	map->bankgrp[0] = FIELD_GET(DDR_ADDRMAP_B0_M31, addrmap[8]) + BANKGRP_B0_BASE;

	map->bankgrp[1] = FIELD_GET(DDR_ADDRMAP_B8_M31, addrmap[8]);
	map->bankgrp[1] = map->bankgrp[1] == DDR_ADDRMAP_MAX_31 ?
			  DDR_ADDRMAP_UNUSED : map->bankgrp[1] + BANKGRP_B1_BASE;
}

/**
 * snps_get_hif_rank_map - Get HIF/SDRAM-rank address map.
 * @priv:	DDR memory controller private instance data.
 * @addrmap:	Array with ADDRMAP registers value.
 *
 * SDRAM-rank address is defined by the fields in the ADDRMAP[0]
 * register. Those fields value indicate the HIF address bits used to encode
 * the DDR rank address.
 */
static void snps_get_hif_rank_map(struct snps_edac_priv *priv, u32 *addrmap)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	int i;

	for (i = 0; i < DDR_MAX_RANK_WIDTH; i++)
		map->rank[i] = DDR_ADDRMAP_UNUSED;

	if (priv->info.ranks > 1) {
		map->rank[0] = FIELD_GET(DDR_ADDRMAP_B0_M31, addrmap[0]);
		map->rank[0] = map->rank[0] == DDR_ADDRMAP_MAX_31 ?
			       DDR_ADDRMAP_UNUSED : map->rank[0] + RANK_B0_BASE;
	}

	if (priv->info.ranks > 2) {
		map->rank[1] = FIELD_GET(DDR_ADDRMAP_B8_M31, addrmap[0]);
		map->rank[1] = map->rank[1] == DDR_ADDRMAP_MAX_31 ?
			       DDR_ADDRMAP_UNUSED : map->rank[1] + RANK_B1_BASE;
	}
}

/**
 * snps_get_addr_map - Get HIF/SDRAM/etc address map from CSRs.
 * @priv:	DDR memory controller private instance data.
 *
 * Parse the controller registers content creating the addresses mapping tables.
 * They will be used for the erroneous and poison addresses encode/decode.
 */
static void snps_get_addr_map(struct snps_edac_priv *priv)
{
	u32 regval[max(DDR_ADDRMAP_NREGS, 2 * DDR_MAX_NSAR)];
	int i;

	for (i = 0; i < 2 * DDR_MAX_NSAR; i++)
		regval[i] = readl(priv->baseaddr + DDR_SARBASE0_OFST + i * 4);

	snps_get_sys_app_map(priv, regval);

	for (i = 0; i < DDR_ADDRMAP_NREGS; i++)
		regval[i] = readl(priv->baseaddr + DDR_ADDRMAP0_OFST + i * 4);

	snps_get_hif_row_map(priv, regval);

	snps_get_hif_col_map(priv, regval);

	snps_get_hif_bank_map(priv, regval);

	snps_get_hif_bankgrp_map(priv, regval);

	snps_get_hif_rank_map(priv, regval);
}

/**
 * snps_get_sdram_size - Calculate SDRAM size.
 * @priv:	DDR memory controller private data.
 *
 * The total size of the attached memory is calculated based on the HIF/SDRAM
 * mapping table. It can be done since the hardware reference manual demands
 * that none two SDRAM bits should be mapped to the same HIF bit and that the
 * unused SDRAM address bits mapping must be disabled.
 *
 * Return: the memory size in bytes.
 */
static u64 snps_get_sdram_size(struct snps_edac_priv *priv)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	u64 size = 0;
	int i;

	for (i = 0; i < DDR_MAX_ROW_WIDTH; i++) {
		if (map->row[i] != DDR_ADDRMAP_UNUSED)
			size++;
	}

	for (i = 0; i < DDR_MAX_COL_WIDTH; i++) {
		if (map->col[i] != DDR_ADDRMAP_UNUSED)
			size++;
	}

	for (i = 0; i < DDR_MAX_BANK_WIDTH; i++) {
		if (map->bank[i] != DDR_ADDRMAP_UNUSED)
			size++;
	}

	for (i = 0; i < DDR_MAX_BANKGRP_WIDTH; i++) {
		if (map->bankgrp[i] != DDR_ADDRMAP_UNUSED)
			size++;
	}

	/* Skip the ranks since the multi-rankness is determined by layer[0] */

	return 1ULL << (size + priv->info.dq_width);
}

/**
 * snps_init_csrows - Initialize the csrow data.
 * @mci:	EDAC memory controller instance.
 *
 * Initialize the chip select rows associated with the EDAC memory
 * controller instance.
 */
static void snps_init_csrows(struct mem_ctl_info *mci)
{
	struct snps_edac_priv *priv = mci->pvt_info;
	struct csrow_info *csi;
	struct dimm_info *dimm;
	u32 row, width;
	u64 size;
	int j;

	/* Actual SDRAM-word width for which ECC is calculated */
	width = 1U << (priv->info.dq_width - priv->info.dq_mode);

	for (row = 0; row < mci->nr_csrows; row++) {
		csi = mci->csrows[row];
		size = snps_get_sdram_size(priv);

		for (j = 0; j < csi->nr_channels; j++) {
			dimm		= csi->channels[j]->dimm;
			dimm->edac_mode	= EDAC_SECDED;
			dimm->mtype	= priv->info.sdram_mode;
			dimm->nr_pages	= PHYS_PFN(size) / csi->nr_channels;
			dimm->grain	= width;
			dimm->dtype	= priv->info.dev_cfg;
		}
	}
}

/**
 * snps_mc_create - Create and initialize MC instance.
 * @priv:	DDR memory controller private data.
 *
 * Allocate the EDAC memory controller descriptor and initialize it
 * using the private data info.
 *
 * Return: MC data instance or negative errno.
 */
static struct mem_ctl_info *snps_mc_create(struct snps_edac_priv *priv)
{
	struct edac_mc_layer layers[2];
	struct mem_ctl_info *mci;

	layers[0].type = EDAC_MC_LAYER_CHIP_SELECT;
	layers[0].size = priv->info.ranks;
	layers[0].is_virt_csrow = true;
	layers[1].type = EDAC_MC_LAYER_CHANNEL;
	layers[1].size = SNPS_EDAC_NR_CHANS;
	layers[1].is_virt_csrow = false;

	mci = edac_mc_alloc(EDAC_AUTO_MC_NUM, ARRAY_SIZE(layers), layers, 0);
	if (!mci) {
		edac_printk(KERN_ERR, EDAC_MC,
			    "Failed memory allocation for mc instance\n");
		return ERR_PTR(-ENOMEM);
	}

	mci->pvt_info = priv;
	mci->pdev = &priv->pdev->dev;
	platform_set_drvdata(priv->pdev, mci);

	/* Initialize controller capabilities and configuration */
	mci->mtype_cap = MEM_FLAG_LPDDR | MEM_FLAG_DDR2 | MEM_FLAG_LPDDR2 |
			 MEM_FLAG_DDR3 | MEM_FLAG_LPDDR3 |
			 MEM_FLAG_DDR4 | MEM_FLAG_LPDDR4;
	mci->edac_ctl_cap = EDAC_FLAG_NONE | EDAC_FLAG_SECDED | EDAC_FLAG_PARITY;
	mci->edac_cap = mci->edac_ctl_cap;

	if (priv->info.caps & SNPS_CAP_ECC_SCRUB) {
		mci->scrub_mode = SCRUB_HW_SRC;
		mci->scrub_cap = SCRUB_FLAG_HW_SRC;
	} else {
		mci->scrub_mode = SCRUB_SW_SRC;
		mci->scrub_cap = SCRUB_FLAG_SW_SRC;
	}

	if (priv->info.caps & SNPS_CAP_ECC_SCRUBBER) {
		mci->scrub_cap |= SCRUB_FLAG_HW_PROG | SCRUB_FLAG_HW_TUN;
		mci->set_sdram_scrub_rate = snps_set_sdram_scrub_rate;
		mci->get_sdram_scrub_rate = snps_get_sdram_scrub_rate;
	}

	mci->ctl_name = "snps_umctl2_ddrc";
	mci->dev_name = SNPS_EDAC_MOD_STRING;
	mci->mod_name = SNPS_EDAC_MOD_VER;

	edac_op_state = EDAC_OPSTATE_INT;

	mci->ctl_page_to_phys = NULL;

	snps_init_csrows(mci);

	return mci;
}

/**
 * snps_mc_free - Free MC instance.
 * @mci:	EDAC memory controller instance.
 *
 * Just revert what was done in the framework of the snps_mc_create().
 *
 * Return: MC data instance or negative errno.
 */
static void snps_mc_free(struct mem_ctl_info *mci)
{
	struct snps_edac_priv *priv = mci->pvt_info;

	platform_set_drvdata(priv->pdev, NULL);

	edac_mc_free(mci);
}

/**
 * snps_request_ind_irq - Request individual DDRC IRQs.
 * @mci:	EDAC memory controller instance.
 *
 * Return: 0 if the IRQs were successfully requested, 1 if the individual IRQs
 * are unavailable, otherwise negative errno.
 */
static int snps_request_ind_irq(struct mem_ctl_info *mci)
{
	struct snps_edac_priv *priv = mci->pvt_info;
	struct device *dev = &priv->pdev->dev;
	int rc, irq;

	irq = platform_get_irq_byname_optional(priv->pdev, "ecc_ce");
	if (irq == -ENXIO)
		return 1;
	if (irq < 0)
		return irq;

	rc = devm_request_irq(dev, irq, snps_ce_irq_handler, 0, "ecc_ce", mci);
	if (rc) {
		edac_printk(KERN_ERR, EDAC_MC, "Failed to request ECC CE IRQ\n");
		return rc;
	}

	irq = platform_get_irq_byname(priv->pdev, "ecc_ue");
	if (irq < 0)
		return irq;

	rc = devm_request_irq(dev, irq, snps_ue_irq_handler, 0, "ecc_ue", mci);
	if (rc) {
		edac_printk(KERN_ERR, EDAC_MC, "Failed to request ECC UE IRQ\n");
		return rc;
	}

	irq = platform_get_irq_byname_optional(priv->pdev, "dfi_e");
	if (irq > 0) {
		rc = devm_request_irq(dev, irq, snps_dfi_irq_handler, 0, "dfi_e", mci);
		if (rc) {
			edac_printk(KERN_ERR, EDAC_MC, "Failed to request DFI IRQ\n");
			return rc;
		}
	}

	irq = platform_get_irq_byname_optional(priv->pdev, "ecc_sbr");
	if (irq > 0) {
		rc = devm_request_irq(dev, irq, snps_sbr_irq_handler, 0, "ecc_sbr", mci);
		if (rc) {
			edac_printk(KERN_ERR, EDAC_MC, "Failed to request Sbr IRQ\n");
			return rc;
		}
	}

	return 0;
}

/**
 * snps_request_com_irq - Request common DDRC IRQ.
 * @mci:	EDAC memory controller instance.
 *
 * It first attempts to get the named IRQ. If failed the method fallbacks
 * to first available one.
 *
 * Return: 0 if the IRQ was successfully requested otherwise negative errno.
 */
static int snps_request_com_irq(struct mem_ctl_info *mci)
{
	struct snps_edac_priv *priv = mci->pvt_info;
	struct device *dev = &priv->pdev->dev;
	int rc, irq;

	irq = platform_get_irq_byname_optional(priv->pdev, "ecc");
	if (irq < 0) {
		irq = platform_get_irq(priv->pdev, 0);
		if (irq < 0)
			return irq;
	}

	rc = devm_request_irq(dev, irq, snps_com_irq_handler, 0, "ecc", mci);
	if (rc) {
		edac_printk(KERN_ERR, EDAC_MC, "Failed to request IRQ\n");
		return rc;
	}

	return 0;
}

/**
 * snps_setup_irq - Request and enable DDRC IRQs.
 * @mci:	EDAC memory controller instance.
 *
 * It first tries to get and request individual IRQs. If failed the method
 * fallbacks to the common IRQ line case. The IRQs will be enabled only if
 * some of these requests have been successful.
 *
 * Return: 0 if IRQs were successfully setup otherwise negative errno.
 */
static int snps_setup_irq(struct mem_ctl_info *mci)
{
	struct snps_edac_priv *priv = mci->pvt_info;
	int rc;

	rc = snps_request_ind_irq(mci);
	if (rc > 0)
		rc = snps_request_com_irq(mci);
	if (rc)
		return rc;

	snps_enable_irq(priv);

	return 0;
}

#ifdef CONFIG_EDAC_DEBUG

#define SNPS_DEBUGFS_FOPS(__name, __read, __write) \
	static const struct file_operations __name = {	\
		.owner = THIS_MODULE,		\
		.open = simple_open,		\
		.read = __read,			\
		.write = __write,		\
	}

#define SNPS_DBGFS_BUF_LEN 128

static int snps_ddrc_info_show(struct seq_file *s, void *data)
{
	struct mem_ctl_info *mci = s->private;
	struct snps_edac_priv *priv = mci->pvt_info;
	unsigned long rate;

	seq_printf(s, "SDRAM: %s\n", edac_mem_types[priv->info.sdram_mode]);

	rate = clk_get_rate(priv->clks[SNPS_CORE_CLK].clk);
	if (rate) {
		rate = rate / HZ_PER_MHZ;
		seq_printf(s, "Clock: Core %luMHz SDRAM %luMHz\n",
			   rate, priv->info.freq_ratio * rate);
	}

	seq_printf(s, "DQ bus: %u/%s\n", (BITS_PER_BYTE << priv->info.dq_width),
		   priv->info.dq_mode == SNPS_DQ_FULL ? "Full" :
		   priv->info.dq_mode == SNPS_DQ_HALF ? "Half" :
		   priv->info.dq_mode == SNPS_DQ_QRTR ? "Quarter" :
		   "Unknown");
	seq_printf(s, "Burst: SDRAM %u HIF %u\n", priv->info.sdram_burst_len,
		   priv->info.hif_burst_len);

	seq_printf(s, "Ranks: %u\n", priv->info.ranks);

	seq_printf(s, "ECC: %s\n",
		   priv->info.ecc_mode == SNPS_ECC_SECDED ? "SEC/DED" :
		   priv->info.ecc_mode == SNPS_ECC_ADVX4X8 ? "Advanced X4/X8" :
		   "Unknown");

	seq_puts(s, "Caps:");
	if (priv->info.caps) {
		if (priv->info.caps & SNPS_CAP_ECC_SCRUB)
			seq_puts(s, " +Scrub");
		if (priv->info.caps & SNPS_CAP_ECC_SCRUBBER)
			seq_puts(s, " +Scrubber");
		if (priv->info.caps & SNPS_CAP_ZYNQMP)
			seq_puts(s, " +ZynqMP");
	} else {
		seq_puts(s, " -");
	}
	seq_putc(s, '\n');

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(snps_ddrc_info);

static int snps_sys_app_map_show(struct seq_file *s, void *data)
{
	struct mem_ctl_info *mci = s->private;
	struct snps_edac_priv *priv = mci->pvt_info;
	struct snps_sys_app_map *map = &priv->sys_app_map;
	u64 size;
	int i;

	if (!map->nsar) {
		seq_puts(s, "No SARs detected\n");
		return 0;
	}

	seq_printf(s, "%9s %-37s %-18s %-37s\n",
		   "", "System address", "Offset", "App address");

	for (i = 0, size = 0; i < map->nsar; i++) {
		seq_printf(s, "Region %d: ", i);
		seq_printf(s, "0x%016llx-0x%016llx ", map->sar[i].base,
			   map->sar[i].base + map->sar[i].size - 1);
		seq_printf(s, "0x%016llx ", map->sar[i].ofst);
		seq_printf(s, "0x%016llx-0x%016llx\n", size,
			   size + map->sar[i].size - 1);
		size += map->sar[i].size;
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(snps_sys_app_map);

static u8 snps_find_sdram_dim(struct snps_edac_priv *priv, u8 hif, char *dim)
{
	struct snps_hif_sdram_map *map = &priv->hif_sdram_map;
	int i;

	for (i = 0; i < DDR_MAX_ROW_WIDTH; i++) {
		if (map->row[i] == hif) {
			*dim = 'r';
			return i;
		}
	}

	for (i = 0; i < DDR_MAX_COL_WIDTH; i++) {
		if (map->col[i] == hif) {
			*dim = 'c';
			return i;
		}
	}

	for (i = 0; i < DDR_MAX_BANK_WIDTH; i++) {
		if (map->bank[i] == hif) {
			*dim = 'b';
			return i;
		}
	}

	for (i = 0; i < DDR_MAX_BANKGRP_WIDTH; i++) {
		if (map->bankgrp[i] == hif) {
			*dim = 'g';
			return i;
		}
	}

	for (i = 0; i < DDR_MAX_RANK_WIDTH; i++) {
		if (map->rank[i] == hif) {
			*dim = 'a';
			return i;
		}
	}

	return DDR_ADDRMAP_UNUSED;
}

static int snps_hif_sdram_map_show(struct seq_file *s, void *data)
{
	struct mem_ctl_info *mci = s->private;
	struct snps_edac_priv *priv = mci->pvt_info;
	char dim, buf[SNPS_DBGFS_BUF_LEN];
	const int line_len = 10;
	u8 bit;
	int i;

	seq_printf(s, "%3s", "");
	for (i = 0; i < line_len; i++)
		seq_printf(s, " %02d ", i);

	for (i = 0; i < DDR_MAX_HIF_WIDTH; i++) {
		if (i % line_len == 0)
			seq_printf(s, "\n%02d ", i);

		bit = snps_find_sdram_dim(priv, i, &dim);

		if (bit != DDR_ADDRMAP_UNUSED)
			scnprintf(buf, SNPS_DBGFS_BUF_LEN, "%c%hhu", dim, bit);
		else
			scnprintf(buf, SNPS_DBGFS_BUF_LEN, "--");

		seq_printf(s, "%3s ", buf);
	}
	seq_putc(s, '\n');

	seq_puts(s, "r - row, c - column, b - bank, g - bank group, a - rank\n");

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(snps_hif_sdram_map);

static ssize_t snps_inject_data_error_read(struct file *filep, char __user *ubuf,
					   size_t size, loff_t *offp)
{
	struct mem_ctl_info *mci = filep->private_data;
	struct snps_edac_priv *priv = mci->pvt_info;
	struct snps_sdram_addr sdram;
	char buf[SNPS_DBGFS_BUF_LEN];
	dma_addr_t sys;
	u32 regval;
	int pos;

	regval = readl(priv->baseaddr + ECC_POISON0_OFST);
	sdram.rank = FIELD_GET(ECC_POISON0_RANK_MASK, regval);
	sdram.col = FIELD_GET(ECC_POISON0_COL_MASK, regval);

	regval = readl(priv->baseaddr + ECC_POISON1_OFST);
	sdram.bankgrp = FIELD_PREP(ECC_POISON1_BANKGRP_MASK, regval);
	sdram.bank = FIELD_PREP(ECC_POISON1_BANK_MASK, regval);
	sdram.row = FIELD_PREP(ECC_POISON1_ROW_MASK, regval);

	snps_map_sdram_to_sys(priv, &sdram, &sys);

	pos = scnprintf(buf, sizeof(buf),
			"%pad: Row %hu Col %hu Bank %hhu Bank Group %hhu Rank %hhu\n",
			&sys, sdram.row, sdram.col, sdram.bank, sdram.bankgrp,
			sdram.rank);

	return simple_read_from_buffer(ubuf, size, offp, buf, pos);
}

static ssize_t snps_inject_data_error_write(struct file *filep, const char __user *ubuf,
					    size_t size, loff_t *offp)
{
	struct mem_ctl_info *mci = filep->private_data;
	struct snps_edac_priv *priv = mci->pvt_info;
	struct snps_sdram_addr sdram;
	u32 regval;
	u64 sys;
	int rc;

	rc = kstrtou64_from_user(ubuf, size, 0, &sys);
	if (rc)
		return rc;

	snps_map_sys_to_sdram(priv, sys, &sdram);

	regval = FIELD_PREP(ECC_POISON0_RANK_MASK, sdram.rank) |
		 FIELD_PREP(ECC_POISON0_COL_MASK, sdram.col);
	writel(regval, priv->baseaddr + ECC_POISON0_OFST);

	regval = FIELD_PREP(ECC_POISON1_BANKGRP_MASK, sdram.bankgrp) |
		 FIELD_PREP(ECC_POISON1_BANK_MASK, sdram.bank) |
		 FIELD_PREP(ECC_POISON1_ROW_MASK, sdram.row);
	writel(regval, priv->baseaddr + ECC_POISON1_OFST);

	return size;
}

SNPS_DEBUGFS_FOPS(snps_inject_data_error, snps_inject_data_error_read,
		  snps_inject_data_error_write);

static ssize_t snps_inject_data_poison_read(struct file *filep, char __user *ubuf,
					    size_t size, loff_t *offp)
{
	struct mem_ctl_info *mci = filep->private_data;
	struct snps_edac_priv *priv = mci->pvt_info;
	char buf[SNPS_DBGFS_BUF_LEN];
	const char *errstr;
	u32 regval;
	int pos;

	regval = readl(priv->baseaddr + ECC_CFG1_OFST);
	if (!(regval & ECC_CFG1_POISON_EN))
		errstr = "Off";
	else if (regval & ECC_CFG1_POISON_BIT)
		errstr = "CE";
	else
		errstr = "UE";

	pos = scnprintf(buf, sizeof(buf), "%s\n", errstr);

	return simple_read_from_buffer(ubuf, size, offp, buf, pos);
}

static ssize_t snps_inject_data_poison_write(struct file *filep, const char __user *ubuf,
					     size_t size, loff_t *offp)
{
	struct mem_ctl_info *mci = filep->private_data;
	struct snps_edac_priv *priv = mci->pvt_info;
	char buf[SNPS_DBGFS_BUF_LEN];
	u32 regval;
	int rc;

	rc = simple_write_to_buffer(buf, sizeof(buf), offp, ubuf, size);
	if (rc < 0)
		return rc;

	writel(0, priv->baseaddr + DDR_SWCTL);

	regval = readl(priv->baseaddr + ECC_CFG1_OFST);
	if (strncmp(buf, "CE", 2) == 0)
		regval |= ECC_CFG1_POISON_BIT | ECC_CFG1_POISON_EN;
	else if (strncmp(buf, "UE", 2) == 0)
		regval = (regval & ~ECC_CFG1_POISON_BIT) | ECC_CFG1_POISON_EN;
	else
		regval &= ~ECC_CFG1_POISON_EN;
	writel(regval, priv->baseaddr + ECC_CFG1_OFST);

	writel(1, priv->baseaddr + DDR_SWCTL);

	return size;
}

SNPS_DEBUGFS_FOPS(snps_inject_data_poison, snps_inject_data_poison_read,
		  snps_inject_data_poison_write);

/**
 * snps_create_debugfs_nodes -	Create DebugFS nodes.
 * @mci:	EDAC memory controller instance.
 *
 * Create DW uMCTL2 EDAC driver DebugFS nodes in the device private
 * DebugFS directory.
 *
 * Return: none.
 */
static void snps_create_debugfs_nodes(struct mem_ctl_info *mci)
{
	edac_debugfs_create_file("ddrc_info", 0400, mci->debugfs, mci,
				 &snps_ddrc_info_fops);

	edac_debugfs_create_file("sys_app_map", 0400, mci->debugfs, mci,
				 &snps_sys_app_map_fops);

	edac_debugfs_create_file("hif_sdram_map", 0400, mci->debugfs, mci,
				 &snps_hif_sdram_map_fops);

	edac_debugfs_create_file("inject_data_error", 0600, mci->debugfs, mci,
				 &snps_inject_data_error);

	edac_debugfs_create_file("inject_data_poison", 0600, mci->debugfs, mci,
				 &snps_inject_data_poison);
}

#else /* !CONFIG_EDAC_DEBUG */

static inline void snps_create_debugfs_nodes(struct mem_ctl_info *mci) {}

#endif /* !CONFIG_EDAC_DEBUG */

/**
 * snps_mc_probe - Check controller and bind driver.
 * @pdev:	platform device.
 *
 * Probe a specific controller instance for binding with the driver.
 *
 * Return: 0 if the controller instance was successfully bound to the
 * driver; otherwise, < 0 on error.
 */
static int snps_mc_probe(struct platform_device *pdev)
{
	struct snps_edac_priv *priv;
	struct mem_ctl_info *mci;
	int rc;

	priv = snps_create_data(pdev);
	if (IS_ERR(priv))
		return PTR_ERR(priv);

	rc = snps_get_res(priv);
	if (rc)
		return rc;

	rc = snps_get_ddrc_info(priv);
	if (rc)
		goto put_res;

	snps_get_addr_map(priv);

	mci = snps_mc_create(priv);
	if (IS_ERR(mci)) {
		rc = PTR_ERR(mci);
		goto put_res;
	}

	rc = snps_setup_irq(mci);
	if (rc)
		goto free_edac_mc;

	rc = edac_mc_add_mc(mci);
	if (rc) {
		edac_printk(KERN_ERR, EDAC_MC,
			    "Failed to register with EDAC core\n");
		goto free_edac_mc;
	}

	snps_create_debugfs_nodes(mci);

	return 0;

free_edac_mc:
	snps_mc_free(mci);

put_res:
	snps_put_res(priv);

	return rc;
}

/**
 * snps_mc_remove - Unbind driver from device.
 * @pdev:	Platform device.
 *
 * Return: Unconditionally 0
 */
static int snps_mc_remove(struct platform_device *pdev)
{
	struct mem_ctl_info *mci = platform_get_drvdata(pdev);
	struct snps_edac_priv *priv = mci->pvt_info;

	snps_disable_irq(priv);

	edac_mc_del_mc(&pdev->dev);

	snps_mc_free(mci);

	snps_put_res(priv);

	return 0;
}

static const struct of_device_id snps_edac_match[] = {
	{ .compatible = "xlnx,zynqmp-ddrc-2.40a", .data = zynqmp_init_plat },
	{ .compatible = "baikal,bt1-ddrc", .data = bt1_init_plat },
	{ .compatible = "snps,ddrc-3.80a" },
	{ }
};
MODULE_DEVICE_TABLE(of, snps_edac_match);

static struct platform_driver snps_edac_mc_driver = {
	.driver = {
		   .name = "snps-edac",
		   .of_match_table = snps_edac_match,
		   },
	.probe = snps_mc_probe,
	.remove = snps_mc_remove,
};
module_platform_driver(snps_edac_mc_driver);

MODULE_AUTHOR("Xilinx Inc");
MODULE_DESCRIPTION("Synopsys uMCTL2 DDR ECC driver");
MODULE_LICENSE("GPL v2");
