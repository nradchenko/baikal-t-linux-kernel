// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 BAIKAL ELECTRONICS, JSC
 *
 * Authors:
 *   Alexey Malahov <Alexey.Malahov@baikalelectronics.ru>
 *   Serge Semin <Sergey.Semin@baikalelectronics.ru>
 *
 * Baikal-T1 early printk
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/serial_reg.h>

#include <asm/mach-baikal-t1/memory.h>

#define BT1_UART_BASE(_id) \
	(void *)KSEG1ADDR(CONCATENATE(BT1_UART, CONCATENATE(_id, _BASE)))

void prom_putchar(char c)
{
	void __iomem *uart_base = BT1_UART_BASE(CONFIG_BT1_EARLY_UART);
	unsigned int timeout = 50000;
	int status, bits;

	bits = UART_LSR_TEMT | UART_LSR_THRE;

	do {
		status = __raw_readl(uart_base + (UART_LSR << 2));

		if (--timeout == 0)
			break;
	} while ((status & bits) != bits);

	if (timeout)
		__raw_writel(c, uart_base + (UART_TX << 2));
}
