// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Baikal-T1 IRQ initialization
 */
#include <linux/irqchip.h>

#include <asm/mipsregs.h>
#include <asm/mips-gic.h>
#include <asm/irq_cpu.h>
#include <asm/irq.h>

int get_c0_fdc_int(void)
{
	return gic_get_c0_fdc_int();
}

int get_c0_perfcount_int(void)
{
	return gic_get_c0_perfcount_int();
}

unsigned int get_c0_compare_int(void)
{
	return gic_get_c0_compare_int();
}

/*
 * If CP0.Cause.IV == 1 and cpu_has_veic = 1 the next method isn't supposed
 * to be called ever. Otherwise we just handle a vectored interrupt, which was
 * routed to the generic exception vector.
 */
#if !defined(CONFIG_IRQ_MIPS_CPU)

asmlinkage void plat_irq_dispatch(void)
{
	extern unsigned long vi_handlers[];
	unsigned int cause = (read_c0_cause() & CAUSEF_IP) >> CAUSEB_IP2;
	void (*isr)(void) = (void *)vi_handlers[cause];

	if (cause && isr)
		isr();
	else if (cause && !isr)
		panic("Vectored interrupt %u handler is empty\n", cause);
	else
		spurious_interrupt();
}

#endif /* !CONFIG_IRQ_MIPS_CPU */

void __init arch_init_irq(void)
{
	if (!cpu_has_veic)
		mips_cpu_irq_init();

	irqchip_init();
}
