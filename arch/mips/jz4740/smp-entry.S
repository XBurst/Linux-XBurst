/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  Copyright (C) 2013, Paul Burton <paul.burton@imgtec.com>
 *  JZ4780 SMP entry point
 */

#include <asm/addrspace.h>
#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/cacheops.h>
#include <asm/mipsregs.h>

#define CACHE_SIZE (32 * 1024)
#define CACHE_LINESIZE 32

.extern jz4780_cpu_entry_sp
.extern jz4780_cpu_entry_gp

.section .text.smp-entry
.balign 0x10000
.set noreorder
LEAF(jz4780_secondary_cpu_entry)
	mtc0	zero, CP0_CAUSE

	li	t0, ST0_CU0
	mtc0	t0, CP0_STATUS

	/* cache setup */
	li	t0, KSEG0
	ori	t1, t0, CACHE_SIZE
	mtc0	zero, CP0_TAGLO, 0
1:	cache	Index_Store_Tag_I, 0(t0)
	cache	Index_Store_Tag_D, 0(t0)
	bne	t0, t1, 1b
	 addiu	t0, t0, CACHE_LINESIZE

	/* kseg0 cache attribute */
	mfc0	t0, CP0_CONFIG, 0
	ori	t0, t0, CONF_CM_CACHABLE_NONCOHERENT
	mtc0	t0, CP0_CONFIG, 0

	/* pagemask */
	mtc0	zero, CP0_PAGEMASK, 0

	/* retrieve sp */
	la	t0, jz4780_cpu_entry_sp
	lw	sp, 0(t0)

	/* retrieve gp */
	la	t0, jz4780_cpu_entry_gp
	lw	gp, 0(t0)

	/* jump to the kernel in kseg0 */
	la	t0, smp_bootstrap
	jr	t0
	 nop
	END(jz4780_secondary_cpu_entry)
