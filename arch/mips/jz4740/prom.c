// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *  JZ4740 SoC prom code
 */

#include <linux/init.h>

#include <asm/bootinfo.h>
#include <asm/fw/fw.h>
#include <asm/mach-jz4740/jz4780-smp.h>

void __init prom_init(void)
{
	fw_init_cmdline();
#if defined(CONFIG_MACH_JZ4780) && defined(CONFIG_SMP)
	jz4780_smp_init();
#endif
}

void __init prom_free_prom_memory(void)
{
}
