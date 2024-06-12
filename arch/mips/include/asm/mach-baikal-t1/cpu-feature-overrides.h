/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Baikal-T1 core features override
 */
#ifndef __ASM_MACH_BAIKAL_T1_CPU_FEATURE_OVERRIDES_H__
#define __ASM_MACH_BAIKAL_T1_CPU_FEATURE_OVERRIDES_H__

#ifdef CONFIG_BT1_CPU_FEATURE_OVERRIDES

#define cpu_has_tlb		1
/* Don't override FTLB flag otherwise 'noftlb' option won't work. */
/* #define cpu_has_ftlb		1 */
#define cpu_has_tlbinv		1
#define cpu_has_segments	1
#define cpu_has_eva		1
#define cpu_has_htw		1
#define cpu_has_ldpte		0
#define cpu_has_rixiex		1
#define cpu_has_maar		1
#define cpu_has_rw_llb		1

#define cpu_has_3kex		0
#define cpu_has_4kex		1
#define cpu_has_3k_cache	0
#define cpu_has_4k_cache	1
#define cpu_has_tx39_cache	0
#define cpu_has_octeon_cache	0

/* Don't override FPU flags otherwise 'nofpu' option won't work. */
/* #define cpu_has_fpu		1 */
/* #define raw_cpu_has_fpu	1 */
#define cpu_has_32fpr		1

#define cpu_has_counter		1
#define cpu_has_watch		1
#define cpu_has_divec		1
#define cpu_has_vce		0
#define cpu_has_cache_cdex_p	0
#define cpu_has_cache_cdex_s	0
#define cpu_has_prefetch	1
#define cpu_has_mcheck		1
#define cpu_has_ejtag		1
#define cpu_has_llsc		1
#define cpu_has_bp_ghist	0
#define cpu_has_guestctl0ext	1 /* ? */
#define cpu_has_guestctl1	1 /* ? */
#define cpu_has_guestctl2	1 /* ? */
#define cpu_has_guestid		1
#define cpu_has_drg		0
#define cpu_has_mips16		0
#define cpu_has_mips16e2	0
#define cpu_has_mdmx		0
#define cpu_has_mips3d		0
#define cpu_has_smartmips	0

#define cpu_has_rixi		1

#define cpu_has_mmips		0
#define cpu_has_lpa		1
#define cpu_has_mvh		1
#define cpu_has_xpa		1
#define cpu_has_vtag_icache	0
#define cpu_has_dc_aliases	0
#define cpu_has_ic_fills_f_dc	0
#define cpu_has_pindexed_dcache	0
/* Depends on the MIPS_CM/SMP configs. */
/* #define cpu_icache_snoops_remote_store 1 */

/*
 * MIPS P5600 Warrior is based on the MIPS32 Release 2 architecture, which
 * makes it backward compatible with all 32bits early MIPS  architecture
 * releases.
 */
#define cpu_has_mips_1		1
#define cpu_has_mips_2		1
#define cpu_has_mips_3		0
#define cpu_has_mips_4		0
#define cpu_has_mips_5		0
#define cpu_has_mips32r1	1
#define cpu_has_mips32r2	1
#define cpu_has_mips32r5	1
#define cpu_has_mips32r6	0
#define cpu_has_mips64r1	0
#define cpu_has_mips64r2	0
#define cpu_has_mips64r6	0
#define cpu_has_mips_r2_exec_hazard 0

#define cpu_has_clo_clz		1
#define cpu_has_wsbh		1
#define cpu_has_dsp		0
#define cpu_has_dsp2		0
#define cpu_has_dsp3		0
#define cpu_has_loongson_mmi	0
#define cpu_has_loongson_cam	0
#define cpu_has_loongson_ext	0
#define cpu_has_loongson_ext2	0
#define cpu_has_mipsmt		0
#define cpu_has_vp		0
#define cpu_has_userlocal	1

#define cpu_has_nofpuex		0
#define cpu_has_64bits		0
#define cpu_has_64bit_zero_reg	0
#define cpu_has_64bit_gp_regs	0
#define cpu_has_64bit_addresses	0
/*
 * VINT is hardwired to 1 by P5600 design while VEIC as being SI_EICPresent
 * and seeing we always have MIPS GIC available in the chip must have been set
 * to 1. Alas the IP core engineers mistakenly made it to be wired with
 * GIC_VX_CTL_EIC bit. Lets fix it by manually setting the flag to 1.
 */
#define cpu_has_vint		1
#define cpu_has_veic		1
/* Chaches line size is fixed by P5600 design. */
#define cpu_dcache_line_size()	32
#define cpu_icache_line_size()	32
#define cpu_scache_line_size()	32
#define cpu_tcache_line_size()	0
#define cpu_hwrena_impl_bits	0
#define cpu_has_perf_cntr_intr_bit 1
#define cpu_has_vz		1
#define cpu_has_msa		1
#define cpu_has_ufr		1
#define cpu_has_fre		0
#define cpu_has_cdmm		1
#define cpu_has_small_pages	0
#define cpu_has_nan_legacy	0
#define cpu_has_nan_2008	1
#define cpu_has_ebase_wg	1
#define cpu_has_badinstr	1
#define cpu_has_badinstrp	1
#define cpu_has_contextconfig	1
#define cpu_has_perf		1
#define cpu_has_mac2008_only	0
#define cpu_has_mmid		0
#define cpu_has_mm_sysad	0
#define cpu_has_mm_full		1

#endif /* CONFIG_BT1_CPU_FEATURE_OVERRIDES */

#endif /* __ASM_MACH_BAIKAL_T1_CPU_FEATURE_OVERRIDES_H__ */
