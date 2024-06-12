/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Baikal-T1 platform declarations
 */
#ifndef __ASM_MACH_BAIKAL_T1_PLATFORM_H__
#define __ASM_MACH_BAIKAL_T1_PLATFORM_H__

#ifdef CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED

int mips_set_uca_range(phys_addr_t start, phys_addr_t end);

#else /* !CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED */

static inline int mips_set_uca_range(phys_addr_t start, phys_addr_t end)
{
	return 0;
}

#endif /* !CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED */

#endif /* __ASM_MACH_BAIKAL_T1_PLATFORM_H__ */
