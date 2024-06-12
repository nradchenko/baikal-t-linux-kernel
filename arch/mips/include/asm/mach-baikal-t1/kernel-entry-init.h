/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Baikal-T1 platform low-level initialization
 */
#ifndef __ASM_MACH_BAIKAL_T1_KERNEL_ENTRY_INIT_H__
#define __ASM_MACH_BAIKAL_T1_KERNEL_ENTRY_INIT_H__

#include <asm/regdef.h>
#include <asm/mipsregs.h>

	/*
	 * Prepare segments for EVA boot:
	 *
	 * This is in case the processor boots in legacy configuration
	 * (SI_EVAReset is de-asserted and CONFIG5.K == 0)
	 *
	 * =========================== Mappings ===============================
	 * CFG Virtual memory        Physical memory        CCA Mapping
	 *  5  0x00000000 0x3fffffff 0x80000000 0xBffffffff K0   MUSUK (kuseg)
	 *  4  0x40000000 0x7fffffff 0xC0000000 0xfffffffff K0   MUSUK (kuseg)
	 *                           Flat 2GB physical mem
	 *
	 *  3  0x80000000 0x9fffffff 0x00000000 0x1ffffffff K0   MUSUK (kseg0)
	 *  2  0xa0000000 0xbf000000 0x00000000 0x1ffffffff UC   MUSUK (kseg1)
	 *  1  0xc0000000 0xdfffffff -                      K0    MK   (kseg2)
	 *  0  0xe0000000 0xffffffff -                      K0    MK   (kseg3)
	 * where UC = 2 Uncached non-coherent,
	 * WB = 3 Cacheable, non-coherent, write-back, write allocate,
	 * CWBE = 4 Cacheable, coherent, write-back, write-allocate, read
	 *          misses request Exclusive,
	 * CWB = 5 Cacheable, coherent, write-back, write-allocate, read misses
	 *	   request Shared,
	 * UCA = 7 Uncached Accelerated, non-coherent.
	 * UK = 0 Kernel-only unmapped region,
	 * MK = 1 Kernel-only mapped region,
	 * MSK = 2 Supervisor and kernel mapped region,
	 * MUSK = 3 User, supervisor and kernel mapped region,
	 * MUSUK = 4 Used to implement a fully-mapped flat address space in
	 *           user and supervisor modes, with unmapped regions which
	 *           appear in kernel mode,
	 * USK = 5 Supervisor and kernel unmapped region,
	 * UUSK = 7 Unrestricted unmapped region.
	 *
	 * Note K0 = 2 by default on MIPS Warrior P5600.
	 *
	 * Lowmem is expanded to 2GB.
	 *
	 * The following code uses the t0, t1, t2 and ra registers without
	 * previously preserving them.
	 *
	 */
	.macro	platform_eva_init

	.set	push
	.set	reorder
	/*
	 * Get Config.K0 value and use it to program
	 * the segmentation registers
	 */
	mfc0    t1, CP0_CONFIG
	andi	t1, 0x7 /* CCA */
	move	t2, t1
	ins	t2, t1, 16, 3
	/* SegCtl0 */
	li      t0, ((MIPS_SEGCFG_MK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	or	t0, t2
	mtc0	t0, CP0_SEGCTL0

	/* SegCtl1 */
	li      t0, ((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |	\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(2 << MIPS_SEGCFG_C_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	ins	t0, t1, 16, 3
	mtc0	t0, CP0_SEGCTL1

	/* SegCtl2 */
	li	t0, ((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |	\
		(6 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |		\
		(4 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	or	t0, t2
	mtc0	t0, CP0_SEGCTL2

	jal	mips_ihb
	mfc0    t0, CP0_CONFIG5
	li      t2, MIPS_CONF5_K      /* K bit */
	or      t0, t0, t2
	mtc0    t0, CP0_CONFIG5
	sync
	jal	mips_ihb
	nop

	.set	pop
	.endm

	/*
	 * Prepare segments for LEGACY boot:
	 *
	 * =========================== Mappings ==============================
	 * CFG Virtual memory         Physical memory        CCA Mapping
	 *  5  0x00000000 0x3fffffff  -                      CWB  MUSK (kuseg)
	 *  4  0x40000000 0x7fffffff  -                      CWB  MUSK (kuseg)
	 *  3  0x80000000 0x9fffffff  0x00000000 0x1ffffffff CWB   UK  (kseg0)
	 *  2  0xa0000000 0xbf000000  0x00000000 0x1ffffffff  2    UK  (kseg1)
	 *  1  0xc0000000 0xdfffffff  -                      CWB   MSK (kseg2)
	 *  0  0xe0000000 0xffffffff  -                      CWB   MK  (kseg3)
	 *
	 * The following code uses the t0, t1, t2 and ra registers without
	 * previously preserving them.
	 *
	 */
	.macro	platform_legacy_init

	.set	push
	.set	reorder

	/*
	 * Directly use cacheable, coherent, write-back, write-allocate, read
	 * misses request shared attribute (CWB).
	 */
	li      t1, 0x5
	move	t2, t1
	ins	t2, t1, 16, 3
	/* SegCtl0 */
	li      t0, ((MIPS_SEGCFG_MK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT)) |				\
		(((MIPS_SEGCFG_MSK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT)) << 16)
	or	t0, t2
	mtc0	t0, CP0_SEGCTL0

	/* SegCtl1 */
	li      t0, ((MIPS_SEGCFG_UK << MIPS_SEGCFG_AM_SHIFT) |	\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(2 << MIPS_SEGCFG_C_SHIFT)) |				\
		(((MIPS_SEGCFG_UK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT)) << 16)
	ins	t0, t1, 16, 3
	mtc0	t0, CP0_SEGCTL1

	/* SegCtl2 */
	li	t0, ((MIPS_SEGCFG_MUSK << MIPS_SEGCFG_AM_SHIFT) |	\
		(6 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MUSK << MIPS_SEGCFG_AM_SHIFT) |		\
		(4 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	or	t0, t2
	mtc0	t0, CP0_SEGCTL2

	jal	mips_ihb
	nop

	mfc0    t0, CP0_CONFIG, 5
	li      t2, MIPS_CONF5_K      /* K bit */
	or      t0, t0, t2
	mtc0    t0, CP0_CONFIG, 5
	sync
	jal	mips_ihb
	nop

	.set	pop
	.endm

	/*
	 * Baikal-T1 engineering chip had problems when the next features
	 * were enabled.
	 */
	.macro	platform_errata_jr_ls_fix

	.set	push
	.set	reorder

	jal	mips_ihb
	nop

	/* Disable load/store bonding. */
	mfc0    t0, CP0_CONFIG, 6
	lui     t1, (MIPS_CONF6_DLSB >> 16)
	or      t0, t0, t1
	/* Disable all JR prediction except JR $31. */
	ori     t0, t0, MIPS_CONF6_JRCD
	mtc0    t0, CP0_CONFIG, 6
	sync
	jal	mips_ihb
	nop

	/* Disable all JR $31 prediction through return prediction stack. */
	mfc0    t0, CP0_CONFIG, 7
	ori     t0, t0, MIPS_CONF7_RPS
	mtc0    t0, CP0_CONFIG, 7
	sync
	jal	mips_ihb
	nop

	.set	pop
	.endm

	/*
	 * Setup Baikal-T1 platform specific setups of the memory segments
	 * layout. In case if the kernel is built for engineering version
	 * of the chip some errata must be fixed.
	 */
	.macro	kernel_entry_setup

	sync
	ehb

#ifdef CONFIG_EVA
	platform_eva_init
#else
	platform_legacy_init
#endif

#ifdef CONFIG_BT1_ERRATA_JR_LS_BUG
	platform_errata_jr_ls_fix
#endif

	.endm

	/*
	 * Do SMP slave processor setup necessary before we can safely execute
	 * C code.
	 */
	.macro	smp_slave_setup
	sync
	ehb

#ifdef CONFIG_EVA
	platform_eva_init
#else
	platform_legacy_init
#endif

#ifdef CONFIG_BT1_ERRATA_JR_LS_BUG
	platform_errata_jr_ls_fix
#endif

	.endm
#endif /* __ASM_MACH_BAIKAL_T1_KERNEL_ENTRY_INIT_H__ */
