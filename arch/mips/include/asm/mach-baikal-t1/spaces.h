/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Baikal-T1 SoC Memory Spaces
 */
#ifndef __ASM_MACH_BAIKAL_T1_SPACES_H__
#define __ASM_MACH_BAIKAL_T1_SPACES_H__

#define PCI_IOBASE		mips_io_port_base
#define PCI_IOSIZE		SZ_64K
#define IO_SPACE_LIMIT		(PCI_IOSIZE - 1)

#define pci_remap_iospace pci_remap_iospace

#include <asm/mach-generic/spaces.h>

#endif /* __ASM_MACH_BAIKAL_T1_SPACES_H__ */
