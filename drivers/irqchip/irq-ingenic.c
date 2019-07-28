// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2009-2010, Lars-Peter Clausen <lars@metafoo.de>
 *  Ingenic XBurst platform IRQ support
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqchip/ingenic.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/mach-jz4740/irq.h>

struct ingenic_intc_data {
	void __iomem *base;
	unsigned num_chips;
};

#define JZ_REG_INTC_STATUS	0x00
#define JZ_REG_INTC_MASK	0x04
#define JZ_REG_INTC_SET_MASK	0x08
#define JZ_REG_INTC_CLEAR_MASK	0x0c
#define JZ_REG_INTC_PENDING	0x10
#define CHIP_SIZE		0x20

static void ingenic_chained_handle_irq(struct irq_desc *desc)
{
	struct ingenic_intc_data *intc = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	bool have_irq = false;
	uint32_t pending;
	unsigned i;

	chained_irq_enter(chip, desc);
	for (i = 0; i < intc->num_chips; i++) {
		pending = readl(intc->base + (i * CHIP_SIZE) +
				JZ_REG_INTC_PENDING);
		if (!pending)
			continue;

		have_irq = true;
		while (pending) {
			int bit = __fls(pending);

			generic_handle_irq(bit + (i * 32) + JZ4740_IRQ_BASE);
			pending &= ~BIT(bit);
		}
	}

	if (!have_irq)
		spurious_interrupt();

	chained_irq_exit(chip, desc);
}

static void ingenic_intc_irq_set_mask(struct irq_chip_generic *gc,
						uint32_t mask)
{
	struct irq_chip_regs *regs = &gc->chip_types->regs;

	writel(mask, gc->reg_base + regs->enable);
	writel(~mask, gc->reg_base + regs->disable);
}

void ingenic_intc_irq_suspend(struct irq_data *data)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(data);
	ingenic_intc_irq_set_mask(gc, gc->wake_active);
}

void ingenic_intc_irq_resume(struct irq_data *data)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(data);
	ingenic_intc_irq_set_mask(gc, gc->mask_cache);
}

static int __init ingenic_intc_of_init(struct device_node *node,
				       unsigned num_chips)
{
	struct ingenic_intc_data *intc;
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;
	struct irq_domain *domain;
	int parent_irq, err = 0;
	unsigned i;

	intc = kzalloc(sizeof(*intc), GFP_KERNEL);
	if (!intc) {
		err = -ENOMEM;
		goto out_err;
	}

	parent_irq = irq_of_parse_and_map(node, 0);
	if (!parent_irq) {
		err = -EINVAL;
		goto out_free;
	}

	err = irq_set_handler_data(parent_irq, intc);
	if (err)
		goto out_unmap_irq;

	intc->num_chips = num_chips;
	intc->base = of_iomap(node, 0);
	if (!intc->base) {
		err = -ENODEV;
		goto out_unmap_irq;
	}

	for (i = 0; i < num_chips; i++) {
		/* Mask all irqs */
		writel(0xffffffff, intc->base + (i * CHIP_SIZE) +
		       JZ_REG_INTC_SET_MASK);

		gc = irq_alloc_generic_chip("INTC", 1,
					    JZ4740_IRQ_BASE + (i * 32),
					    intc->base + (i * CHIP_SIZE),
					    handle_level_irq);

		gc->wake_enabled = IRQ_MSK(32);

		ct = gc->chip_types;
		ct->regs.enable = JZ_REG_INTC_CLEAR_MASK;
		ct->regs.disable = JZ_REG_INTC_SET_MASK;
		ct->chip.irq_unmask = irq_gc_unmask_enable_reg;
		ct->chip.irq_mask = irq_gc_mask_disable_reg;
		ct->chip.irq_mask_ack = irq_gc_mask_disable_reg;
		ct->chip.irq_set_wake = irq_gc_set_wake;
		ct->chip.irq_suspend = ingenic_intc_irq_suspend;
		ct->chip.irq_resume = ingenic_intc_irq_resume;

		irq_setup_generic_chip(gc, IRQ_MSK(32), 0, 0,
				       IRQ_NOPROBE | IRQ_LEVEL);
	}

	domain = irq_domain_add_legacy(node, num_chips * 32, JZ4740_IRQ_BASE, 0,
				       &irq_domain_simple_ops, NULL);
	if (!domain)
		pr_warn("unable to register IRQ domain\n");

	irq_set_chained_handler_and_data(parent_irq,
					ingenic_chained_handle_irq, intc);
	return 0;

out_unmap_irq:
	irq_dispose_mapping(parent_irq);
out_free:
	kfree(intc);
out_err:
	return err;
}

static int __init intc_1chip_of_init(struct device_node *node,
				     struct device_node *parent)
{
	return ingenic_intc_of_init(node, 1);
}
IRQCHIP_DECLARE(jz4740_intc, "ingenic,jz4740-intc", intc_1chip_of_init);
IRQCHIP_DECLARE(jz4725b_intc, "ingenic,jz4725b-intc", intc_1chip_of_init);

static int __init intc_2chip_of_init(struct device_node *node,
	struct device_node *parent)
{
	return ingenic_intc_of_init(node, 2);
}
IRQCHIP_DECLARE(jz4760_intc, "ingenic,jz4760-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(jz4760b_intc, "ingenic,jz4760b-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(jz4770_intc, "ingenic,jz4770-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(jz4775_intc, "ingenic,jz4775-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(jz4780_intc, "ingenic,jz4780-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(x1000_intc, "ingenic,x1000-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(x1000e_intc, "ingenic,x1000e-intc", intc_2chip_of_init);
IRQCHIP_DECLARE(x1500_intc, "ingenic,x1500-intc", intc_2chip_of_init);
