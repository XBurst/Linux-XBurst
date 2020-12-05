// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic XBurst SoCs specific cache code.
 * Copyright (c) 2020 周琰杰 (Zhou Yanjie) <zhouyanjie@wanyeetech.com>
 */

#include <linux/cpu_pm.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/smp.h>

#include <asm/bootinfo.h>
#include <asm/cache.h>
#include <asm/cacheops.h>
#include <asm/cpu.h>
#include <asm/mmu_context.h>
#include <asm/r4kcache.h>
#include <asm/traps.h>

static unsigned long dcache_size __read_mostly;
static unsigned long icache_size __read_mostly;
static unsigned long scache_size __read_mostly;

struct flush_icache_range_args {
	unsigned long start;
	unsigned long end;
	bool user;
};

static void (*ingenic_blast_dcache)(void);
static void (*ingenic_blast_icache)(void);
static void (*ingenic_blast_scache)(void);

static void (*ingenic_blast_scache_range)(unsigned long start, unsigned long end);

static void ingenic_cache_noop(void) {}
static void ingenic_cache_noop0(void) {printk("-------0\n"); while(1);}
static void ingenic_cache_noop1(void) {printk("-------1\n"); while(1);}
static void ingenic_cache_noop2(void) {printk("-------2\n"); while(1);}
static void ingenic_cache_noop7(void) {printk("-------7\n"); while(1);}

static void ingenic_blast_pcache_setup(void)
{
	ingenic_blast_dcache = blast_dcache32;
	ingenic_blast_icache = blast_icache32;
}

static void ingenic_blast_scache_setup(void)
{
	unsigned long sc_lsize = cpu_scache_line_size();

	if (scache_size == 0) {
		ingenic_blast_scache = (void *)ingenic_cache_noop;
		ingenic_blast_scache_range = (void *)ingenic_cache_noop;
	} else if (sc_lsize == 32) {
		ingenic_blast_scache = blast_scache32;
		ingenic_blast_scache_range = blast_scache_range;
	} else if (sc_lsize == 64) {
		ingenic_blast_scache = blast_scache64;
		ingenic_blast_scache_range = blast_scache_range;
	} else if (sc_lsize == 128) {
		ingenic_blast_scache = blast_scache128;
		ingenic_blast_scache_range = blast_scache_range;
	}
}

static inline bool ingenic_op_needs_ipi(void)
{
	/* cpu_foreign_map[] undeclared when !CONFIG_SMP */
#ifdef CONFIG_SMP
	return !cpumask_empty(&cpu_foreign_map[0]);
#else
	return false;
#endif
}

static inline void local_ingenic_flush_cache_all(void *args)
{
	ingenic_blast_dcache();
	ingenic_blast_icache();
}

static void ingenic_flush_cache_all(void)
{
	preempt_disable();

	if (ingenic_op_needs_ipi())
		smp_call_function_many(&cpu_foreign_map[smp_processor_id()],
				local_ingenic_flush_cache_all, NULL, 1);

	local_ingenic_flush_cache_all(NULL);

	preempt_enable();
}

static void ingenic_flush_cache_page(struct vm_area_struct *vma, unsigned long page, unsigned long pfn)
{
	int exec = vma->vm_flags & VM_EXEC;
	pte_t *ptep;

	preempt_disable();

	if (!cpu_context(smp_processor_id(), vma->vm_mm))
		goto out;

	page &= PAGE_MASK;
	ptep = pte_offset_kernel(pmd_off(vma->vm_mm, page), page);

	if (!(pte_present(*ptep)))
		goto out;

	if ((vma->vm_mm == current->active_mm) && (pte_val(*ptep) & _PAGE_VALID)) {
		if (exec) {
			blast_dcache32_page(page);
			blast_icache32_page(page);
		}

		goto out;
	}

	if (exec) {
		blast_dcache32_page_indexed(page);
		blast_icache32_page_indexed(page);
	}

out:
	preempt_enable();
}

static inline void __local_ingenic_flush_icache_range(unsigned long start,
						     unsigned long end, bool user)
{
	if (end - start >= dcache_size)
		ingenic_blast_dcache();
	else if (user)
		protected_blast_dcache_range(start, end);
	else
		blast_dcache_range(start, end);

	if (end - start > icache_size)
		ingenic_blast_icache();
	else if (user)
		protected_blast_icache_range(start, end);
	else
		blast_icache_range(start, end);
}

static inline void local_ingenic_flush_icache_range(unsigned long start,
						     unsigned long end)
{
	__local_ingenic_flush_icache_range(start, end, false);
}

static inline void local_ingenic_flush_icache_user_range(unsigned long start,
						     unsigned long end)
{
	__local_ingenic_flush_icache_range(start, end, true);
}

static inline void local_ingenic_flush_icache_range_ipi(void *args)
{
	struct flush_icache_range_args *fir_args = args;
	unsigned long start = fir_args->start;
	unsigned long end = fir_args->end;
	bool user = fir_args->user;

	__local_ingenic_flush_icache_range(start, end, user);
}

static void __ingenic_flush_icache_range(unsigned long start, unsigned long end, bool user)
{
	struct flush_icache_range_args args;
	unsigned long size, cache_size;

	args.start = start;
	args.end = end;
	args.user = user;

	preempt_disable();

	if ((current_cpu_type() == CPU_XBURST2) && ingenic_op_needs_ipi()) {
		size = end - start;
		cache_size = icache_size;
		size *= 2;
		cache_size += dcache_size;

		if (size <= cache_size) {
			if (user) {
				protected_blast_dcache_range(start, end);
				protected_blast_icache_range(start, end);
			} else {
				blast_dcache_range(start, end);
				blast_icache_range(start, end);
			}

			goto out;
		}
	}

	if (ingenic_op_needs_ipi())
		smp_call_function_many(&cpu_foreign_map[smp_processor_id()],
				local_ingenic_flush_icache_range_ipi, &args, 1);

	local_ingenic_flush_icache_range_ipi(&args);

out:
	preempt_enable();
}

static void ingenic_flush_icache_range(unsigned long start, unsigned long end)
{
	return __ingenic_flush_icache_range(start, end, false);
}

static void ingenic_flush_icache_user_range(unsigned long start, unsigned long end)
{
	return __ingenic_flush_icache_range(start, end, true);
}

static void ingenic_dma_cache_wback_inv(unsigned long address, unsigned long size)
{
	if (WARN_ON(size == 0))
		return;

	preempt_disable();

	if (!ingenic_op_needs_ipi() && size >= dcache_size)
		ingenic_blast_dcache();
	else
		blast_dcache_range(address, address + size);

	preempt_enable();

	if (size >= scache_size)
		ingenic_blast_scache();
	else
		ingenic_blast_scache_range(address, address + size);

	__sync();
}

static void xburst_dma_cache_inv(unsigned long address, unsigned long size)
{
	if (WARN_ON(size == 0))
		return;

	preempt_disable();
	write_c0_ingenic_errctl(XBURST_ERRCTL_WST_EN);

	if (!ingenic_op_needs_ipi() && size >= dcache_size)
		ingenic_blast_dcache();
	else
		blast_inv_dcache_range(address, address + size);

	write_c0_ingenic_errctl(XBURST_ERRCTL_WST_DIS);
	preempt_enable();

	__sync();
}

static void xburst2_dma_cache_inv(unsigned long address, unsigned long size)
{
	unsigned long lsize = cpu_scache_line_size();
	unsigned long almask = ~(lsize - 1);

	if (WARN_ON(size == 0))
		return;

	preempt_disable();

	if (!ingenic_op_needs_ipi() && size >= dcache_size)
		ingenic_blast_dcache();
	else
		blast_inv_dcache_range(address, address + size);

	preempt_enable();

	if (size >= scache_size) {
		ingenic_blast_scache();
	} else {
		cache_op(Hit_Writeback_Inv_SD, address & almask);
		cache_op(Hit_Writeback_Inv_SD, (address + size - 1) & almask);
		blast_inv_scache_range(address, address + size);
	}

	__sync();
}

static void probe_pcache(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;
	unsigned long config1;

	config1 = read_c0_config1();

	c->icache.linesz = 2 << ((config1 >> 19) & 7);
	c->icache.sets = 32 << (((config1 >> 22) + 1) & 7);
	c->icache.ways = 1 + ((config1 >> 16) & 7);

	icache_size = c->icache.sets * c->icache.ways * c->icache.linesz;
	if (!icache_size)
		panic("Invalid Primary instruction cache size.");

	c->icache.waysize = icache_size / c->icache.ways;
	c->icache.waybit = __ffs(c->icache.waysize);

	c->dcache.linesz = 2 << ((config1 >> 10) & 7);
	c->dcache.sets = 32 << (((config1 >> 13) + 1) & 7);
	c->dcache.ways = 1 + ((config1 >> 7) & 7);

	dcache_size = c->dcache.sets * c->dcache.ways * c->dcache.linesz;
	if (!dcache_size)
		panic("Invalid Primary data cache size.");

	c->dcache.waysize = dcache_size / c->dcache.ways;
	c->dcache.waybit = __ffs(c->dcache.waysize);

	c->options |= MIPS_CPU_PREFETCH;

	switch (mips_machtype) {

	/* Physically indexed icache */
	case MACH_INGENIC_JZ4730:
	case MACH_INGENIC_JZ4740:
		c->icache.flags |= MIPS_CACHE_PINDEX;
		break;

	/* Physically indexed icache and dcache */
	case MACH_INGENIC_JZ4725B:
	case MACH_INGENIC_JZ4755:
	case MACH_INGENIC_JZ4760:
	case MACH_INGENIC_X2000:
	case MACH_INGENIC_X2000E:
		c->icache.flags |= MIPS_CACHE_PINDEX;
		c->dcache.flags |= MIPS_CACHE_PINDEX;
		break;
	}

	pr_info("Primary instruction cache %ldkiB, %s, %d-way, %d sets, linesize %d bytes.\n",
			icache_size >> 10, c->icache.flags & MIPS_CACHE_PINDEX ? "PIPT" : "VIPT",
			c->icache.ways, c->icache.sets, c->icache.linesz);

	pr_info("Primary data cache %ldkiB, %s, %d-way, %d sets, linesize %d bytes.\n",
			dcache_size >> 10, c->dcache.flags & MIPS_CACHE_PINDEX ? "PIPT" : "VIPT",
			c->dcache.ways, c->dcache.sets, c->dcache.linesz);
}

static void probe_scache(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;
	unsigned int config1, config2;

	/* Mark as not present until probe completed */
	c->scache.flags |= MIPS_CACHE_NOT_PRESENT;

	/* Does this Ingenic CPU have a config2 register? */
	config1 = read_c0_config1();
	if (!(config1 & MIPS_CONF_M))
		return;

	config2 = read_c0_config2();

	c->scache.linesz = 2 << ((config2 >> 4) & 0xf);
	c->scache.sets = 64 << ((config2 >> 8) & 0xf);
	c->scache.ways = 1 + ((config2 >> 0) & 0xf);

	switch (mips_machtype) {

	/*
	 * According to config2 it would be 5-ways, but that is
	 * contradicted by all documentation.
	 */
	case MACH_INGENIC_JZ4770:
	case MACH_INGENIC_JZ4775:
		c->scache.ways = 4;
		break;

	/*
	 * According to config2 it would be 8-ways and 256-sets,
	 * but that is contradicted by all documentation.
	 */
	case MACH_INGENIC_JZ4780:
		c->scache.sets = 1024;
		c->scache.ways = 4;
		break;

	/*
	 * According to config2 it would be 5-ways and 512-sets,
	 * but that is contradicted by all documentation.
	 */
	case MACH_INGENIC_X1000:
	case MACH_INGENIC_X1000E:
		c->scache.sets = 256;
		c->scache.ways = 4;
		break;
	}

	scache_size = c->scache.ways * c->scache.sets * c->scache.linesz;
	if (!scache_size)
		return ;

	c->scache.waysize = c->scache.sets * c->scache.linesz;
	c->scache.waybit = __ffs(c->scache.waysize);

	c->scache.flags &= ~MIPS_CACHE_NOT_PRESENT;

	if (mips_machtype >= MACH_INGENIC_X2000)
		c->scache.flags |= MIPS_CACHE_PINDEX;
	else
		write_c0_ingenic_errctl(XBURST_ERRCTL_WST_DIS);

	pr_info("Unified secondary cache %ldkiB, %s, %d-way, %d sets, linesize %d bytes.\n",
			scache_size >> 10, c->scache.flags & MIPS_CACHE_PINDEX ? "PIPT" : "VIPT",
			c->scache.ways, c->scache.sets, c->scache.linesz);
}

static int cca = -1;

static int __init cca_setup(char *str)
{
	get_option(&str, &cca);

	return 0;
}

early_param("cca", cca_setup);

static void ingenic_coherency_setup(void)
{
	if (cca < 0 || cca > 7)
		cca = read_c0_config() & CONF_CM_CMASK;
	_page_cachable_default = cca << _CACHE_SHIFT;

	pr_debug("Using cache attribute %d\n", cca);
	change_c0_config(CONF_CM_CMASK, cca);
}

static void ingenic_cache_error_setup(void)
{
	extern char __weak except_vec2_generic;

	set_uncached_handler(0x100, &except_vec2_generic, 0x80);
}

void ingenic_cache_init(void)
{
	extern void build_clear_page(void);
	extern void build_copy_page(void);

	probe_pcache();
	probe_scache();

	ingenic_blast_pcache_setup();
	ingenic_blast_scache_setup();

	__flush_cache_vmap = ingenic_cache_noop0;
	__flush_cache_vunmap = ingenic_cache_noop1;
	__flush_cache_all = ingenic_flush_cache_all;

	__local_flush_icache_user_range = local_ingenic_flush_icache_user_range;
	__flush_icache_user_range = ingenic_flush_icache_user_range;

	__flush_kernel_vmap_range = (void *)ingenic_cache_noop2;

	flush_cache_range = (void *)ingenic_cache_noop;
	flush_cache_page = ingenic_flush_cache_page;
	flush_cache_mm = (void *)ingenic_cache_noop;
	flush_cache_all = ingenic_cache_noop;

	local_flush_icache_range = local_ingenic_flush_icache_range;
	flush_icache_range = ingenic_flush_icache_range;
	flush_icache_all = ingenic_cache_noop;

	local_flush_data_cache_page = (void *)ingenic_cache_noop7;
	flush_data_cache_page = blast_dcache32_page;

	_dma_cache_wback_inv = ingenic_dma_cache_wback_inv;
	_dma_cache_wback = ingenic_dma_cache_wback_inv;
	if (current_cpu_type() == CPU_XBURST)
		_dma_cache_inv = xburst_dma_cache_inv;
	else if (current_cpu_type() == CPU_XBURST2)
		_dma_cache_inv = xburst2_dma_cache_inv;
	else
		panic("Unknown Ingenic CPU type.");

	build_clear_page();
	build_copy_page();

	local_ingenic_flush_cache_all(NULL);

	ingenic_coherency_setup();
	board_cache_error_setup = ingenic_cache_error_setup;
}

static int ingenic_cache_pm_notifier(struct notifier_block *self,
						     unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		ingenic_coherency_setup();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block ingenic_cache_pm_notifier_block = {
	.notifier_call = ingenic_cache_pm_notifier,
};

int __init ingenic_cache_init_pm(void)
{
	return cpu_pm_register_notifier(&ingenic_cache_pm_notifier_block);
}
arch_initcall(ingenic_cache_init_pm);
