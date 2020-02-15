/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  Copyright (C) 2013, Paul Burton <paul.burton@imgtec.com>
 *  JZ4780 SMP definitions
 */

#ifndef __MIPS_ASM_MACH_JZ4740_SMP_H__
#define __MIPS_ASM_MACH_JZ4740_SMP_H__

#define read_c0_corectrl()		__read_32bit_c0_register($12, 2)
#define write_c0_corectrl(val)		__write_32bit_c0_register($12, 2, val)

#define read_c0_corestatus()		__read_32bit_c0_register($12, 3)
#define write_c0_corestatus(val)	__write_32bit_c0_register($12, 3, val)

#define read_c0_reim()			__read_32bit_c0_register($12, 4)
#define write_c0_reim(val)		__write_32bit_c0_register($12, 4, val)

#define read_c0_mailbox0()		__read_32bit_c0_register($20, 0)
#define write_c0_mailbox0(val)		__write_32bit_c0_register($20, 0, val)

#define read_c0_mailbox1()		__read_32bit_c0_register($20, 1)
#define write_c0_mailbox1(val)		__write_32bit_c0_register($20, 1, val)

#define read_c0_mailbox2()		__read_32bit_c0_register($20, 2)
#define write_c0_mailbox2(val)		__write_32bit_c0_register($20, 2, val)

#define read_c0_mailbox3()		__read_32bit_c0_register($20, 3)
#define write_c0_mailbox3(val)		__write_32bit_c0_register($20, 3, val)

#define smp_clr_pending(mask) do {		\
		unsigned int stat;		\
		stat = read_c0_corestatus();	\
		stat &= ~((mask) & 0xff);	\
		write_c0_corestatus(stat);	\
	} while (0)

/*
 * Core Control register
 */
#define CORECTRL_SLEEP1M_SHIFT	17
#define CORECTRL_SLEEP1M	(_ULCAST_(0x1) << CORECTRL_SLEEP1M_SHIFT)
#define CORECTRL_SLEEP0M_SHIFT	16
#define CORECTRL_SLEEP0M	(_ULCAST_(0x1) << CORECTRL_SLEEP0M_SHIFT)
#define CORECTRL_RPC1_SHIFT	9
#define CORECTRL_RPC1		(_ULCAST_(0x1) << CORECTRL_RPC1_SHIFT)
#define CORECTRL_RPC0_SHIFT	8
#define CORECTRL_RPC0		(_ULCAST_(0x1) << CORECTRL_RPC0_SHIFT)
#define CORECTRL_SWRST1_SHIFT	1
#define CORECTRL_SWRST1		(_ULCAST_(0x1) << CORECTRL_SWRST1_SHIFT)
#define CORECTRL_SWRST0_SHIFT	0
#define CORECTRL_SWRST0		(_ULCAST_(0x1) << CORECTRL_SWRST0_SHIFT)

/*
 * Core Status register
 */
#define CORESTATUS_SLEEP1_SHIFT	17
#define CORESTATUS_SLEEP1	(_ULCAST_(0x1) << CORESTATUS_SLEEP1_SHIFT)
#define CORESTATUS_SLEEP0_SHIFT	16
#define CORESTATUS_SLEEP0	(_ULCAST_(0x1) << CORESTATUS_SLEEP0_SHIFT)
#define CORESTATUS_IRQ1P_SHIFT	9
#define CORESTATUS_IRQ1P	(_ULCAST_(0x1) << CORESTATUS_IRQ1P_SHIFT)
#define CORESTATUS_IRQ0P_SHIFT	8
#define CORESTATUS_IRQ0P	(_ULCAST_(0x1) << CORESTATUS_IRQ8P_SHIFT)
#define CORESTATUS_MIRQ1P_SHIFT	1
#define CORESTATUS_MIRQ1P	(_ULCAST_(0x1) << CORESTATUS_MIRQ1P_SHIFT)
#define CORESTATUS_MIRQ0P_SHIFT	0
#define CORESTATUS_MIRQ0P	(_ULCAST_(0x1) << CORESTATUS_MIRQ0P_SHIFT)

/*
 * Reset Entry & IRQ Mask register
 */
#define REIM_ENTRY_SHIFT	16
#define REIM_ENTRY		(_ULCAST_(0xffff) << REIM_ENTRY_SHIFT)
#define REIM_IRQ1M_SHIFT	9
#define REIM_IRQ1M		(_ULCAST_(0x1) << REIM_IRQ1M_SHIFT)
#define REIM_IRQ0M_SHIFT	8
#define REIM_IRQ0M		(_ULCAST_(0x1) << REIM_IRQ0M_SHIFT)
#define REIM_MBOXIRQ1M_SHIFT	1
#define REIM_MBOXIRQ1M		(_ULCAST_(0x1) << REIM_MBOXIRQ1M_SHIFT)
#define REIM_MBOXIRQ0M_SHIFT	0
#define REIM_MBOXIRQ0M		(_ULCAST_(0x1) << REIM_MBOXIRQ0M_SHIFT)

extern void jz4780_smp_init(void);
extern void jz4780_smp_wait_irqoff(void);
extern void jz4780_secondary_cpu_entry(void);

#endif /* __MIPS_ASM_MACH_JZ4740_SMP_H__ */
