// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2013, Paul Burton <paul.burton@imgtec.com>
 *  JZ4780 SMP
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/smp.h>
#include <linux/tick.h>
#include <asm/mach-jz4740/smp.h>
#include <asm/smp-ops.h>

static struct clk *cpu_clock_gates[CONFIG_NR_CPUS] = { NULL };

u32 jz4780_cpu_entry_sp;
u32 jz4780_cpu_entry_gp;

static struct cpumask cpu_running;

static DEFINE_SPINLOCK(smp_lock);

static irqreturn_t mbox_handler(int irq, void *dev_id)
{
	int cpu = smp_processor_id();
	u32 action, status;

	spin_lock(&smp_lock);

	switch (cpu) {
	case 0:
		action = read_c0_mailbox0();
		write_c0_mailbox0(0);
		break;
	case 1:
		action = read_c0_mailbox1();
		write_c0_mailbox1(0);
		break;
	case 2:
		action = read_c0_mailbox2();
		write_c0_mailbox2(0);
		break;
	case 3:
		action = read_c0_mailbox3();
		write_c0_mailbox3(0);
		break;
	default:
		panic("unhandled cpu %d!", cpu);
	}

	/* clear pending mailbox interrupt */
	status = read_c0_corestatus();
	status &= ~(CORESTATUS_MIRQ0P << cpu);
	write_c0_corestatus(status);

	spin_unlock(&smp_lock);

	if (action & SMP_RESCHEDULE_YOURSELF)
		scheduler_ipi();
	if (action & SMP_CALL_FUNCTION)
		generic_smp_call_function_interrupt();

	return IRQ_HANDLED;
}

static void jz4780_smp_setup(void)
{
	u32 addr, reim;
	int cpu;

	reim = read_c0_reim();

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		__cpu_number_map[cpu] = cpu;
		__cpu_logical_map[cpu] = cpu;
		set_cpu_possible(cpu, true);
	}

	/* mask mailbox interrupts for this core */
	reim &= ~REIM_MBOXIRQ0M;
	write_c0_reim(reim);

	/* clear mailboxes & pending mailbox IRQs */
	write_c0_mailbox0(0);
	write_c0_mailbox1(0);
	write_c0_corestatus(0);

	/* set reset entry point */
	addr = KSEG1ADDR((u32)&jz4780_secondary_cpu_entry);
	WARN_ON(addr & ~REIM_ENTRY);
	reim &= ~REIM_ENTRY;
	reim |= addr & REIM_ENTRY;

	/* unmask mailbox interrupts for this core */
	reim |= REIM_MBOXIRQ0M;
	write_c0_reim(reim);
	set_c0_status(STATUSF_IP3);
	irq_enable_hazard();

	cpumask_set_cpu(cpu, &cpu_running);
}

static void jz4780_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *cpu_node;
	unsigned cpu, ctrl;
	int err;

	/* setup the mailbox IRQ */
	err = request_irq(MIPS_CPU_IRQ_BASE + 3, mbox_handler,
			IRQF_PERCPU | IRQF_NO_THREAD, "core mailbox", NULL);
	if (err)
		pr_err("request_irq() on core mailbox failed\n");

	ctrl = read_c0_corectrl();

	for_each_of_cpu_node(cpu_node) {
		cpu = of_cpu_node_to_id(cpu_node);
		if (cpu < 0) {
			pr_err("Failed to read index of %s\n",
			       cpu_node->full_name);
			continue;
		}

		/* use reset entry point from REIM register */
		ctrl |= CORECTRL_RPC0 << cpu;

		cpu_clock_gates[cpu] = of_clk_get(cpu_node, 0);
		if (IS_ERR(cpu_clock_gates[cpu])) {
			cpu_clock_gates[cpu] = NULL;
			continue;
		}

		err = clk_prepare(cpu_clock_gates[cpu]);
		if (err)
			pr_err("Failed to prepare CPU clock gate\n");
	}

	write_c0_corectrl(ctrl);
}

static int jz4780_boot_secondary(int cpu, struct task_struct *idle)
{
	unsigned long flags;
	u32 ctrl;

	spin_lock_irqsave(&smp_lock, flags);

	/* ensure the core is in reset */
	ctrl = read_c0_corectrl();
	ctrl |= CORECTRL_SWRST0 << cpu;
	write_c0_corectrl(ctrl);

	/* ungate core clock */
	if (cpu_clock_gates[cpu])
		clk_enable(cpu_clock_gates[cpu]);

	/* set entry sp/gp register values */
	jz4780_cpu_entry_sp = __KSTK_TOS(idle);
	jz4780_cpu_entry_gp = (u32)task_thread_info(idle);
	smp_wmb();

	/* take the core out of reset */
	ctrl &= ~(CORECTRL_SWRST0 << cpu);
	write_c0_corectrl(ctrl);

	cpumask_set_cpu(cpu, &cpu_running);

	spin_unlock_irqrestore(&smp_lock, flags);

	return 0;
}

static void jz4780_init_secondary(void)
{
}

static void jz4780_smp_finish(void)
{
	u32 reim;

	spin_lock(&smp_lock);

	/* unmask mailbox interrupts for this core */
	reim = read_c0_reim();
	reim |= REIM_MBOXIRQ0M << smp_processor_id();
	write_c0_reim(reim);

	spin_unlock(&smp_lock);

	/* unmask interrupts for this core */
	change_c0_status(ST0_IM, STATUSF_IP3 | STATUSF_IP2 |
			 STATUSF_IP1 | STATUSF_IP0);
	irq_enable_hazard();

	/* force broadcast timer */
	tick_broadcast_force();
}

static void jz4780_send_ipi_single_locked(int cpu, unsigned int action)
{
	u32 mbox;

	switch (cpu) {
	case 0:
		mbox = read_c0_mailbox0();
		write_c0_mailbox0(mbox | action);
		break;
	case 1:
		mbox = read_c0_mailbox1();
		write_c0_mailbox1(mbox | action);
		break;
	default:
		panic("unhandled cpu %d!", cpu);
	}
}

static void jz4780_send_ipi_single(int cpu, unsigned int action)
{
	unsigned long flags;

	spin_lock_irqsave(&smp_lock, flags);
	jz4780_send_ipi_single_locked(cpu, action);
	spin_unlock_irqrestore(&smp_lock, flags);
}

static void jz4780_send_ipi_mask(const struct cpumask *mask,
				 unsigned int action)
{
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&smp_lock, flags);

	for_each_cpu(cpu, mask)
		jz4780_send_ipi_single_locked(cpu, action);

	spin_unlock_irqrestore(&smp_lock, flags);
}

static struct plat_smp_ops jz4780_smp_ops = {
	.send_ipi_single = jz4780_send_ipi_single,
	.send_ipi_mask = jz4780_send_ipi_mask,
	.init_secondary = jz4780_init_secondary,
	.smp_finish = jz4780_smp_finish,
	.boot_secondary = jz4780_boot_secondary,
	.smp_setup = jz4780_smp_setup,
	.prepare_cpus = jz4780_smp_prepare_cpus,
};

void __init jz4780_smp_init(void)
{
	register_smp_ops(&jz4780_smp_ops);
}
