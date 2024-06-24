// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Authors:
 *   Alexey Malahov <Alexey.Malahov@baikalelectronics.ru>
 *   Serge Semin <Sergey.Semin@baikalelectronics.ru>
 *
 * Baikal-T1 platform initialization
 */
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>
#include <linux/limits.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/swiotlb.h>
#include <linux/sys_soc.h>

#include <asm/bootinfo.h>
#include <asm/cpu-info.h>
#include <asm/io.h>
#include <asm/mips-cm.h>
#include <asm/mips-cpc.h>
#include <asm/mipsregs.h>
#include <asm/pci.h>
#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/setup.h>
#include <asm/smp-ops.h>
#include <asm/smp.h>
#include <asm/time.h>

#include <asm/mach-baikal-t1/memory.h>

static __initdata const void *fdt;

/*
 * The following configuration have been used to synthesize the Baikal-T1
 * MIPS Warroir P5600 core:
 * 1) SI_EVAReset = 0 - boot in legacy (not EVA) memory layout mode after
 *    reset.
 * 2) SI_UseExceptionBase = 0 - core uses legacy BEV mode, which selects
 *    0xBFC00000 to be exception vector by default after reset.
 * 3) SI_ExceptionBase[31:12] = 0xBFC00000 - externally set default exception
 *    SI_ExceptionBasePA[31:29] = 0x0        base address. It is used when
 *                                           CP0.CONFIG5.K = 1.
 * 4) SI_EICPresent = 0 - even though GIC is always attached to the cores,
 *                        this pin is hardwaired to the state of the
 *                        GIC_VX_CTL_EIC bit.
 */

/*
 * Redefine the MIPS CDMM phys base method to be used at the earliest boot
 * stage before DT is parsed.
 */
#ifdef CONFIG_MIPS_EJTAG_FDC_EARLYCON

phys_addr_t mips_cdmm_phys_base(void)
{
	return BT1_P5600_CDMM_BASE;
}

#endif /* CONFIG_MIPS_EJTAG_FDC_EARLYCON */

/*
 * We have to redefine the L2-sync phys base method, since the default
 * region overlaps the Baikal-T1 boot memory following the CM2 GCRs.
 */
phys_addr_t mips_cm_l2sync_phys_base(void)
{
	return BT1_P5600_GCR_L2SYNC_BASE;
}

void __init *plat_get_fdt(void)
{
	const char *str;

	/* Return already found fdt. */
	if (fdt)
		return (void *)fdt;

	/*
	 * Generic method will search for appended, UHI and built-in DTBs.
	 * Some older version of Baikal-T1 bootloader could also pass DTB via
	 * the FW arg3 slot. So check that option too.
	 */
	fdt = get_fdt();
	if (fdt) {
		str = (fw_arg0 == -2) ? "UHI" : "Built-in/Appended";
	} else if (fw_arg3) {
		fdt = phys_to_virt(fw_arg3);
		str = "Legacy position";
	}

	if (!fdt || fdt_check_header(fdt))
		panic("No valid dtb found. Can't continue.");

	pr_info("%s DTB found at %px\n", str, fdt);

	return (void *)fdt;
}

#ifdef CONFIG_RELOCATABLE

void __init plat_fdt_relocated(void *new_location)
{
	fdt = NULL;

	/*
	 * Forget about the way dtb has been passed at the system startup. Use
	 * UHI always.
	 */
	fw_arg0 = -2;
	fw_arg1 = (unsigned long)new_location;
}

#endif /* CONFIG_RELOCATABLE */

void __init prom_init(void)
{
	if (IS_ENABLED(CONFIG_EVA) && (read_c0_config5() & MIPS_CONF5_K))
		pr_info("Enhanced Virtual Addressing (EVA) enabled\n");

	/*
	 * Disable Legacy SYNC transaction performed on the L2/Memory port.
	 * This shall significantly improve the concurrent MMIO access
	 * performance.
	 */
	change_gcr_control(CM_GCR_CONTROL_SYNCDIS, CM_GCR_CONTROL_SYNCDIS);

	plat_get_fdt();
}

void __init plat_mem_setup(void)
{
	memblock_add(BT1_LOMEM_BASE, BT1_LOMEM_SIZE);

#ifdef CONFIG_HIGHMEM
	memblock_add(BT1_HIMEM_BASE, BT1_HIMEM_SIZE);
#endif

#ifdef CONFIG_PCI
	PCIBIOS_MIN_IO = 0x100;
#endif

	__dt_setup_arch((void *)fdt);
}

void __init device_tree_init(void)
{
	int err;

	unflatten_and_copy_device_tree();

	mips_cpc_probe();

	err = register_cps_smp_ops();
	if (err)
		err = register_up_smp_ops();
}

#ifdef CONFIG_SWIOTLB

void __init plat_swiotlb_setup(void)
{
	phys_addr_t top;

	/*
	 * Skip SWIOTLB initialization since there is no that much memory to
	 * cause the peripherals invalid access.
	 */
	top = memblock_end_of_DRAM();
	if (top <= SIZE_MAX)
		return;

	/*
	 * Override the default SWIOTLB size with the configuration value.
	 * Note a custom size has been passed via the kernel parameter it won't
	 * be overwritten.
	 */
	swiotlb_adjust_size(CONFIG_BT1_SWIOTLB_SIZE * SZ_1M);
	swiotlb_init(true, SWIOTLB_VERBOSE);
}

#endif /* CONFIG_SWIOTLB */

void __init prom_free_prom_memory(void) {}

#define HZ_TO_MHZ(_hz)	(_hz / 1000000)
#define HZ_GET_KHZ(_hz)	((_hz / 1000) % 1000)
void __init plat_time_init(void)
{
	struct device_node *np;
	unsigned long rate;
	struct clk *clk;

	of_clk_init(NULL);

	np = of_get_cpu_node(0, NULL);
	if (!np) {
		pr_err("Failed to get CPU of node\n");
		goto err_timer_probe;
	}

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk)) {
		pr_err("Failed to get CPU clock (%ld)\n", PTR_ERR(clk));
		goto err_timer_probe;
	}

	/* CPU count/compare timer runs at half the CPU frequency. */
	rate = clk_get_rate(clk);
	mips_hpt_frequency = rate / 2;

	pr_info("MIPS CPU frequency: %lu.%03lu MHz\n",
		HZ_TO_MHZ(rate), HZ_GET_KHZ(rate));
	pr_info("MIPS CPU count/compare timer frequency: %u.%03u MHz\n",
		HZ_TO_MHZ(mips_hpt_frequency), HZ_GET_KHZ(mips_hpt_frequency));

	clk_put(clk);

err_timer_probe:
	timer_probe();
}

const char *get_system_type(void)
{
	return "Baikal-T1 SoC";
}

static struct bt1_soc {
	struct soc_device_attribute dev_attr;
	char revision[16];
	char id[16];
} soc;

static int __init soc_setup(void)
{
	unsigned int cpuid = boot_cpu_data.processor_id;
	struct soc_device *soc_dev;
	struct device *parent = NULL;
	int ret = 0;

	soc.dev_attr.machine = mips_get_machine_name();
	soc.dev_attr.family = get_system_type();
	soc.dev_attr.revision = soc.revision;
	soc.dev_attr.soc_id = soc.id;

	snprintf(soc.revision, sizeof(soc.revision) - 1, "%u.%u.%u",
		(cpuid >> 5) & 0x07, (cpuid >> 2) & 0x07, cpuid & 0x03);
	snprintf(soc.id, sizeof(soc.id) - 1, "0x%08X",
		readl(phys_to_virt(BT1_BOOT_CTRL_BASE + BT1_BOOT_CTRL_DRID)));

	soc_dev = soc_device_register(&soc.dev_attr);
	if (IS_ERR(soc_dev)) {
		ret = PTR_ERR(soc_dev);
		goto err_return;
	}

	parent = soc_device_to_device(soc_dev);

err_return:
	return ret;
}
arch_initcall(soc_setup);

int __uncached_access(struct file *file, unsigned long addr)
{
	if (file->f_flags & O_DSYNC)
		return 1;

	return addr >= __pa(high_memory) ||
		((addr >= BT1_MMIO_START) && (addr < BT1_MMIO_END));
}

#ifdef CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED

static phys_addr_t uca_start, uca_end;

pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
			      unsigned long size, pgprot_t vma_prot)
{
	phys_addr_t offset = PFN_PHYS(pfn), end = offset + size;

	if (__uncached_access(file, offset)) {
		if (uca_start && (offset >= uca_start) &&
		    (end <= uca_end))
			return __pgprot((pgprot_val(vma_prot) &
					 ~_CACHE_MASK) |
					_CACHE_UNCACHED_ACCELERATED);
		else
			return pgprot_noncached(vma_prot);
	}
	return vma_prot;
}

int mips_set_uca_range(phys_addr_t start, phys_addr_t end)
{
	if (end <= start || end <= BT1_MMIO_START)
		return -EINVAL;

	uca_start = start;
	uca_end = end;
	return 0;
}

#endif /* !CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED */
