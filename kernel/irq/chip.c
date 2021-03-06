/*
 * linux/kernel/irq/chip.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the core interrupt handling code, for irq-chip
 * based architectures.
 *
 * Detailed information is available in Documentation/DocBook/genericirq
 */

#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include "internals.h"

/**
 *	dynamic_irq_init - initialize a dynamically allocated irq
 *	@irq:	irq number to initialize
 */
void dynamic_irq_init(unsigned int irq)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to initialize invalid IRQ%d\n", irq);
		WARN_ON(1);
		return;
	}

	/* Ensure we don't have left over values from a previous use of this irq */
	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	desc->status = IRQ_DISABLED;
	desc->chip = &no_irq_chip;
	desc->handle_irq = handle_bad_irq;
	desc->depth = 1;
	desc->msi_desc = NULL;
	desc->handler_data = NULL;
	desc->chip_data = NULL;
	desc->action = NULL;
	desc->irq_count = 0;
	desc->irqs_unhandled = 0;
#ifdef CONFIG_SMP
	desc->affinity = CPU_MASK_ALL;
#endif
	spin_unlock_irqrestore(&desc->lock, flags);
}

/**
 *	dynamic_irq_cleanup - cleanup a dynamically allocated irq
 *	@irq:	irq number to initialize
 */
void dynamic_irq_cleanup(unsigned int irq)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to cleanup invalid IRQ%d\n", irq);
		WARN_ON(1);
		return;
	}

	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	if (desc->action) {
		spin_unlock_irqrestore(&desc->lock, flags);
		printk(KERN_ERR "Destroying IRQ%d without calling free_irq\n",
			irq);
		WARN_ON(1);
		return;
	}
	desc->msi_desc = NULL;
	desc->handler_data = NULL;
	desc->chip_data = NULL;
	desc->handle_irq = handle_bad_irq;
	desc->chip = &no_irq_chip;
	spin_unlock_irqrestore(&desc->lock, flags);
}


/**
 *	set_irq_chip - set the irq chip for an irq
 *	@irq:	irq number
 *	@chip:	pointer to irq chip description structure
 */
//将一个IRQ芯片已irq_chip实例的形式关联到某个特定的中断。
//如果没有提供特定于芯片的实现，该函数还将设置默认的处理程序。
//如果chip指针为NULL，将使用通用的"无控制器"irq_chip实例no_irq_chip，该实现只提供了空操作
int set_irq_chip(unsigned int irq, struct irq_chip *chip)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to install chip for IRQ%d\n", irq);
		WARN_ON(1);
		return -EINVAL;
	}

	if (!chip)
		chip = &no_irq_chip;

	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	irq_chip_set_defaults(chip);
	desc->chip = chip;
	spin_unlock_irqrestore(&desc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(set_irq_chip);

/**
 *	set_irq_type - set the irq type for an irq
 *	@irq:	irq number
 *	@type:	interrupt type - see include/linux/interrupt.h
 */
int set_irq_type(unsigned int irq, unsigned int type)
{
	struct irq_desc *desc;
	unsigned long flags;
	int ret = -ENXIO;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to set irq type for IRQ%d\n", irq);
		return -ENODEV;
	}

	desc = irq_desc + irq;
	if (desc->chip->set_type) {
		spin_lock_irqsave(&desc->lock, flags);
		ret = desc->chip->set_type(irq, type);
		spin_unlock_irqrestore(&desc->lock, flags);
	}
	return ret;
}
EXPORT_SYMBOL(set_irq_type);

/**
 *	set_irq_data - set irq type data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to interrupt specific data
 *
 *	Set the hardware irq controller data for an irq
 */
int set_irq_data(unsigned int irq, void *data)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR
		       "Trying to install controller data for IRQ%d\n", irq);
		return -EINVAL;
	}

	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	desc->handler_data = data;
	spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}
EXPORT_SYMBOL(set_irq_data);

/**
 *	set_irq_data - set irq type data for an irq
 *	@irq:	Interrupt number
 *	@entry:	Pointer to MSI descriptor data
 *
 *	Set the hardware irq controller data for an irq
 */
int set_irq_msi(unsigned int irq, struct msi_desc *entry)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR
		       "Trying to install msi data for IRQ%d\n", irq);
		return -EINVAL;
	}
	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	desc->msi_desc = entry;
	if (entry)
		entry->irq = irq;
	spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}

/**
 *	set_irq_chip_data - set irq chip data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to chip specific data
 *
 *	Set the hardware irq chip data for an irq
 */
int set_irq_chip_data(unsigned int irq, void *data)
{
	struct irq_desc *desc = irq_desc + irq;
	unsigned long flags;

	if (irq >= NR_IRQS || !desc->chip) {
		printk(KERN_ERR "BUG: bad set_irq_chip_data(IRQ#%d)\n", irq);
		return -EINVAL;
	}

	spin_lock_irqsave(&desc->lock, flags);
	desc->chip_data = data;
	spin_unlock_irqrestore(&desc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(set_irq_chip_data);

/*
 * default enable function
 */
static void default_enable(unsigned int irq)
{
	struct irq_desc *desc = irq_desc + irq;

	desc->chip->unmask(irq);
	desc->status &= ~IRQ_MASKED;
}

/*
 * default disable function
 */
static void default_disable(unsigned int irq)
{
}

/*
 * default startup function
 */
static unsigned int default_startup(unsigned int irq)
{
	irq_desc[irq].chip->enable(irq);

	return 0;
}

/*
 * Fixup enable/disable function pointers
 */
void irq_chip_set_defaults(struct irq_chip *chip)
{
	if (!chip->enable)
		chip->enable = default_enable;
	if (!chip->disable)
		chip->disable = default_disable;
	if (!chip->startup)
		chip->startup = default_startup;
	if (!chip->shutdown)
		chip->shutdown = chip->disable;
	if (!chip->name)
		chip->name = chip->typename;
	if (!chip->end)
		chip->end = dummy_irq_chip.end;
}

static inline void mask_ack_irq(struct irq_desc *desc, int irq)
{
	if (desc->chip->mask_ack)
		desc->chip->mask_ack(irq);
	else 
	{
		desc->chip->mask(irq);
		desc->chip->ack(irq);
	}
}

/**
 *	handle_simple_irq - Simple and software-decoded IRQs.
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Simple interrupts are either sent from a demultiplexing interrupt
 *	handler or come from hardware, where no interrupt hardware control
 *	is necessary.
 *
 *	Note: The caller is expected to handle the ack, clear, mask and
 *	unmask issues if necessary.
 */
//非常简单，根本不需要电流控制的中断由handle_simple_irq管理。
//如果调用者想要自行处理电流，也可以使用该函数。
void fastcall handle_simple_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irqaction *action;
	irqreturn_t action_ret;
	const unsigned int cpu = smp_processor_id();

	spin_lock(&desc->lock);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out_unlock;
	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_cpu(cpu).irqs[irq]++;

	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	desc->status |= IRQ_INPROGRESS;
	spin_unlock(&desc->lock);

	action_ret = handle_IRQ_event(irq, action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	spin_lock(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;
out_unlock:
	spin_unlock(&desc->lock);
}

/**
 *	handle_level_irq - Level type irq handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Level type interrupts are active as long as the hardware line has
 *	the active level. This may require to mask the interrupt and unmask
 *	it after the associated handler has acknowledged the device, so the
 *	interrupt line is back to inactive.
 */
//默认的电平触发电流处理程序。
//注意，电平触发中断在处理时必须屏蔽，因此需要完成的第一件事就是调用mask_ack_irq。
//在多处理器系统上，可能发生竞态条件，尽管IRQ已经在另一个CPU上处理，
//但仍然在当前CPU上调用了handle_level_irq。这可以通过检查IRQ_INPROGRESS标志来判断，
//这种情况下，IRQ已经在另一个CPU上处理，因而在当前CPU上可以立即放弃处理。
void fastcall handle_level_irq(unsigned int irq, struct irq_desc *desc)
{
	unsigned int cpu = smp_processor_id();
	struct irqaction *action;
	irqreturn_t action_ret;

	spin_lock(&desc->lock);
	mask_ack_irq(desc, irq);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out_unlock;
	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_cpu(cpu).irqs[irq]++;

	/*
	 * If its disabled or no action available
	 * keep it masked and get out of here
	 */
	action = desc->action;
	//没有对该IRQ注册处理程序或设置了IRQ_DISABLED，则立即放弃处理。
	//尽管被禁用，有问题的硬件仍然可能发出IRQ，但可以忽略。
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	desc->status |= IRQ_INPROGRESS;
	spin_unlock(&desc->lock);

	action_ret = handle_IRQ_event(irq, action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	spin_lock(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;
	//解除对IRQ的屏蔽。但内核需要考虑到ISR可能禁用中断的情况，
	//在这种情况下，ISR仍然保持屏蔽状态。否则，使用特定于芯片的函数chip->umask解除屏蔽
	if (!(desc->status & IRQ_DISABLED) && desc->chip->unmask)
		desc->chip->unmask(irq);
out_unlock:
	spin_unlock(&desc->lock);
}

/**
 *	handle_fasteoi_irq - irq handler for transparent controllers
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Only a single callback will be issued to the chip: an ->eoi()
 *	call when the interrupt has been serviced. This enables support
 *	for modern forms of interrupt handlers, which handle the flow
 *	details in hardware, transparently.
 */
//现代IRQ硬件只需要极少的电流处理工作。
//只需在IRQ处理结束之后调用一个芯片相关的函数chip->eoi。
//此类型的默认处理程序是handle_fasteoi_irq
//它基本上等同于handle_level_irq，除了只需在最后与控制器芯片交互。
void fastcall handle_fasteoi_irq(unsigned int irq, struct irq_desc *desc)
{
	unsigned int cpu = smp_processor_id();
	struct irqaction *action;
	irqreturn_t action_ret;

	spin_lock(&desc->lock);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out;

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_cpu(cpu).irqs[irq]++;

	/*
	 * If its disabled or no action available
	 * then mask it and get out of here:
	 */
	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
	{
		desc->status |= IRQ_PENDING;
		if (desc->chip->mask)
			desc->chip->mask(irq);
		goto out;
	}

	desc->status |= IRQ_INPROGRESS;
	desc->status &= ~IRQ_PENDING;
	spin_unlock(&desc->lock);

	action_ret = handle_IRQ_event(irq, action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	spin_lock(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;
out:
	desc->chip->eoi(irq);

	spin_unlock(&desc->lock);
}

/**
 *	handle_edge_irq - edge type IRQ handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Interrupt occures on the falling and/or rising edge of a hardware
 *	signal. The occurence is latched into the irq controller hardware
 *	and must be acked in order to be reenabled. After the ack another
 *	interrupt can happen on the same source even before the first one
 *	is handled by the assosiacted event handler. If this happens it
 *	might be necessary to disable (mask) the interrupt depending on the
 *	controller hardware. This requires to reenable the interrupt inside
 *	of the loop which handles the interrupts which have arrived while
 *	the handler was running. If all pending interrupts are handled, the
 *	loop is left.
 */
//默认的边沿触发电流处理程序。
//在处理边沿触发的IRQ时无需屏蔽，这与电平触发IRQ是相反的。
//这对SMP系统有一个重要的含义: 当在一个CPU上处理一个IRQ时，
//另一个同样编号的IRQ可以出现在另一个CPU上，称为第二个CPU。
//这意味着，当电流处理程序在由第一个IRQ触发的CPU上运行时，还可能被再次调用。
//但为什么应该由两个CPU同时运行同一个IRQ处理程序呢� 内核想要避免这种情况
//处理程序只应该在一个CPU上运行。handle_edge_irq的开始部分必须处理这种情况。
//如果设置了IRQ_INPROGRESS标志，则该IRQ在另一个CPU上已经处于处理过程中。
//通过设置IRQ_PENDING标志，内核能够记录还有另一个IRQ需要在稍后处理。
//在屏蔽该IRQ并通过mask_ack_irq向控制器发送一个确认后，处理过程可以放弃。
//因而第二个CPU可以恢复正常的工作，而第一个CPU将在稍后处理该IRQ.
//注意，如果IRQ被禁用，或没有可用的ISR处理程序，都会放弃处理。
//(有缺陷的硬件可能在IRQ禁用的情况下仍然生成IRQ，内核需要考虑到这种情况)
void fastcall handle_edge_irq(unsigned int irq, struct irq_desc *desc)
{
	const unsigned int cpu = smp_processor_id();

	spin_lock(&desc->lock);

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);

	/*
	 * If we're currently running this IRQ, or its disabled,
	 * we shouldn't process the IRQ. Mark it pending, handle
	 * the necessary masking and go out
	 */
	//没有处理程序，IRQ在处理中或IRQ禁用
	if (unlikely((desc->status & (IRQ_INPROGRESS | IRQ_DISABLED)) || !desc->action)) 
	{
		desc->status |= (IRQ_PENDING | IRQ_MASKED);
		mask_ack_irq(desc, irq);
		goto out_unlock;
	}

	kstat_cpu(cpu).irqs[irq]++;

	/* Start handling the irq */
	desc->chip->ack(irq);

	/* Mark the IRQ currently in progress.*/
	//内核设置IRQ_INPROGRESS标志。
	//这表示IRQ正在处理过程中，可用于避免同一处理程序在多个CPU上执行
	desc->status |= IRQ_INPROGRESS;

	do 
	{
		struct irqaction *action = desc->action;
		irqreturn_t action_ret;

		if (unlikely(!action))
		{
			desc->chip->mask(irq);
			goto out_unlock;
		}

		/*
		 * When another irq arrived while we were handling 
		 * one, we could have masked the irq.
		 * Renable it, if it was not disabled in meantime.
		 * 如果在处理irq时有另一个irq到达，那么当时可能屏蔽了该irq。
		 * 解除对irq的屏蔽，如果它在此期间没有被禁用的话。
		 */
		if (unlikely((desc->status &(IRQ_PENDING | IRQ_MASKED | IRQ_DISABLED)) 
				== (IRQ_PENDING | IRQ_MASKED))) 
		{
			desc->chip->unmask(irq);
			desc->status &= ~IRQ_MASKED;
		}

		desc->status &= ~IRQ_PENDING;
		spin_unlock(&desc->lock);
		//通过调用handle_IRQ_event激活高层ISR处理程序
		action_ret = handle_IRQ_event(irq, action);
		if (!noirqdebug)
			note_interrupt(irq, desc, action_ret);
		spin_lock(&desc->lock);

	}//只要IRQ_PENDING任然置位，而且IRQ没有禁用，就一直迭代下去
	while ((desc->status & (IRQ_PENDING | IRQ_DISABLED)) == IRQ_PENDING);

	desc->status &= ~IRQ_INPROGRESS;
out_unlock:
	spin_unlock(&desc->lock);
}

/**
 *	handle_percpu_IRQ - Per CPU local irq handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Per CPU interrupts on SMP machines without locking requirements
 */
//各CPU IRQ，即IRQ只能发送到多处理器系统的一个特定的CPU，由handle_percpu_irq处理。
//该函数在接收之后确认IRQ，在处理之后调用eoi例程。
//其实现非常简单，因为不需要锁，根据定义代码只能在一个CPU上运行。
void fastcall handle_percpu_irq(unsigned int irq, struct irq_desc *desc)
{
	irqreturn_t action_ret;

	kstat_this_cpu.irqs[irq]++;

	if (desc->chip->ack)
		desc->chip->ack(irq);

	action_ret = handle_IRQ_event(irq, desc->action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	if (desc->chip->eoi)
		desc->chip->eoi(irq);
}

void
__set_irq_handler(unsigned int irq, irq_flow_handler_t handle, int is_chained,
		  const char *name)
{
	struct irq_desc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR
		       "Trying to install type control for IRQ%d\n", irq);
		return;
	}

	desc = irq_desc + irq;

	if (!handle)
		handle = handle_bad_irq;
	else if (desc->chip == &no_irq_chip) {
		printk(KERN_WARNING "Trying to install %sinterrupt handler "
		       "for IRQ%d\n", is_chained ? "chained " : "", irq);
		/*
		 * Some ARM implementations install a handler for really dumb
		 * interrupt hardware without setting an irq_chip. This worked
		 * with the ARM no_irq_chip but the check in setup_irq would
		 * prevent us to setup the interrupt at all. Switch it to
		 * dummy_irq_chip for easy transition.
		 */
		desc->chip = &dummy_irq_chip;
	}

	spin_lock_irqsave(&desc->lock, flags);

	/* Uninstall? */
	if (handle == handle_bad_irq) {
		if (desc->chip != &no_irq_chip)
			mask_ack_irq(desc, irq);
		desc->status |= IRQ_DISABLED;
		desc->depth = 1;
	}
	desc->handle_irq = handle;
	desc->name = name;

	if (handle != handle_bad_irq && is_chained) {
		desc->status &= ~IRQ_DISABLED;
		desc->status |= IRQ_NOREQUEST | IRQ_NOPROBE;
		desc->depth = 0;
		desc->chip->unmask(irq);
	}
	spin_unlock_irqrestore(&desc->lock, flags);
}

void
set_irq_chip_and_handler(unsigned int irq, struct irq_chip *chip,
			 irq_flow_handler_t handle)
{
	set_irq_chip(irq, chip);
	__set_irq_handler(irq, handle, 0, NULL);
}

void
set_irq_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name)
{
	set_irq_chip(irq, chip);
	__set_irq_handler(irq, handle, 0, name);
}
