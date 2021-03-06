#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

/*
 * Please do not include this file in generic code.  There is currently
 * no requirement for any architecture to implement anything held
 * within this file.
 *
 * Thanks. --rmk
 */

#include <linux/smp.h>

#ifndef CONFIG_S390

#include <linux/linkage.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/irqreturn.h>
#include <linux/errno.h>

#include <asm/irq.h>
#include <asm/ptrace.h>
#include <asm/irq_regs.h>

struct irq_desc;
//irq -- IRQ编号
//desc -- 一个指向负责该中断的irq_handler实例的指针
typedef	void fastcall (*irq_flow_handler_t)(unsigned int irq, struct irq_desc *desc);


/*
 * IRQ line status.
 *
 * Bits 0-7 are reserved for the IRQF_* bits in linux/interrupt.h
 *
 * IRQ types
 */
#define IRQ_TYPE_NONE		0x00000000	/* Default, unspecified type */
#define IRQ_TYPE_EDGE_RISING	0x00000001	/* Edge rising type */
#define IRQ_TYPE_EDGE_FALLING	0x00000002	/* Edge falling type */
#define IRQ_TYPE_EDGE_BOTH (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH	0x00000004	/* Level high type */
#define IRQ_TYPE_LEVEL_LOW	0x00000008	/* Level low type */
#define IRQ_TYPE_SENSE_MASK	0x0000000f	/* Mask of the above */
#define IRQ_TYPE_PROBE		0x00000010	/* Probing in progress */

/* Internal flags */
//在IRQ处理程序执行期间，状态设置为IRQ_INPROGRESS。与IRQ_DISABLED类似，这会阻止其余的内核代码执行该处理程序
#define IRQ_INPROGRESS		0x00000100	/* IRQ handler active - do not enter! */
//用于表示被设备驱动程序禁用的IRQ电路。该标志通知内核不要进入处理程序
#define IRQ_DISABLED		0x00000200	/* IRQ disabled - do not enter! */
//在CPU注意到一个中断但尚未执行对应的处理程序时，IRQ_PENDING标志置位
#define IRQ_PENDING		0x00000400	/* IRQ pending - replay on enable */
//意味着该IRQ已经禁用，但此前尚有一个未确认的中断
#define IRQ_REPLAY		0x00000800	/* IRQ has been replayed but not acked yet */
//用于IRQ自动检测和配置
#define IRQ_AUTODETECT		0x00001000	/* IRQ is being autodetected */
//用于IRQ自动检测和配置
#define IRQ_WAITING		0x00002000	/* IRQ not yet seen - for autodetection */
//用于Alpha和PowerPC系统，用于区分电平触发和边沿触发的IRQ
#define IRQ_LEVEL		0x00004000	/* IRQ level triggered */
//为正确处理发生在中断处理期间的中断，需要IRQ_MASKED标志
#define IRQ_MASKED		0x00008000	/* IRQ masked - shouldn't be seen again */
//在某个IRQ只能发生在一个CPU上时，将设置IRQ_PER_CPU标志位。
//(在SMP系统中，该标志使几个用于防止并发访问的保护机制变得多余)
#define IRQ_PER_CPU		0x00010000	/* IRQ is per CPU */
#define IRQ_NOPROBE		0x00020000	/* IRQ is not valid for probing */
//如果当前IRQ可以由多个设备共享，不是专属于某一设备，则设置IRQ_NOREQUEST标志
#define IRQ_NOREQUEST		0x00040000	/* IRQ cannot be requested */
#define IRQ_NOAUTOEN		0x00080000	/* IRQ will not be enabled on request irq */
#define IRQ_WAKEUP		0x00100000	/* IRQ triggers system wakeup */
#define IRQ_MOVE_PENDING	0x00200000	/* need to re-target IRQ destination */
#define IRQ_NO_BALANCING	0x00400000	/* IRQ is excluded from balancing */

#ifdef CONFIG_IRQ_PER_CPU
# define CHECK_IRQ_PER_CPU(var) ((var) & IRQ_PER_CPU)
# define IRQ_NO_BALANCING_MASK	(IRQ_PER_CPU | IRQ_NO_BALANCING)
#else
# define CHECK_IRQ_PER_CPU(var) 0
# define IRQ_NO_BALANCING_MASK	IRQ_NO_BALANCING
#endif

struct proc_dir_entry;
struct msi_desc;

/**
 * struct irq_chip - hardware interrupt chip descriptor
 *
 * @name:		name for /proc/interrupts
 * @startup:		start up the interrupt (defaults to ->enable if NULL)
 * @shutdown:		shut down the interrupt (defaults to ->disable if NULL)
 * @enable:		enable the interrupt (defaults to chip->unmask if NULL)
 * @disable:		disable the interrupt (defaults to chip->mask if NULL)
 * @ack:		start of a new interrupt
 * @mask:		mask an interrupt source
 * @mask_ack:		ack and mask an interrupt source
 * @unmask:		unmask an interrupt source
 * @eoi:		end of interrupt - chip level
 * @end:		end of interrupt - flow level
 * @set_affinity:	set the CPU affinity on SMP machines
 * @retrigger:		resend an IRQ to the CPU
 * @set_type:		set the flow type (IRQ_TYPE_LEVEL/etc.) of an IRQ
 * @set_wake:		enable/disable power-management wake-on of an IRQ
 *
 * @release:		release function solely used by UML
 * @typename:		obsoleted by name, kept as migration helper
 */
//该类型抽象出了一个IRQ控制器的具体特征，可用于内核的体系机构无关部分。
//它提供的函数用于改变IRQ的状态，这也是它们还负责设置flag的原因
//该结构需要考虑内核中出现的各个IRQ实现的所有特性。
//因而，一个该结构的特定实例，通常只定义所有可能方法的一个子集
struct irq_chip {
	//用于标识硬件控制器。
	//在IA-32系统上可能的值是"XTPIC"和"IO-APIC"，在AMD64系统上大多数情况下也会使用后者。
	//在其他系统上有各种各样的值。
	const char	*name;
	//用于第一次初始化一个IRQ。在大多数情况下，初始化工作仅限于启用该IRQ
	//因而，startup函数实际上就是将工作转给enable。
	unsigned int	(*startup)(unsigned int irq);
	//完全关闭一个中断源。
	//如果不支持该特性，那么这个函数实际上是disable的别名
	void		(*shutdown)(unsigned int irq);
	//激活一个IRQ.
	//换句话说，它执行IRQ由禁用状态到启用状态的转换。
	//为此，必须向I/O内存或I/O端口中硬件相关的位置写入特定于硬件的数值
	void		(*enable)(unsigned int irq);
	//与enable相对应，用于禁用IRQ。
	void		(*disable)(unsigned int irq);
	//ack与中断控制器的硬件密切相关。在某些模型中，IRQ请求的到达
	//(以及在处理器的对应中断)必须显示确认，后续的请求才能进行处理。
	//如果芯片组没有这样的要求，该指针可以指向一个空函数，或NULL指针。
	void		(*ack)(unsigned int irq);
	void		(*mask)(unsigned int irq);
	//确认一个中断，并在接下来屏蔽该中断�
	void		(*mask_ack)(unsigned int irq);
	void		(*unmask)(unsigned int irq);
	//现代的中断控制器不需要内核进行太多的电流控制，控制器几乎可以管理所有事务。
	//在处理中断时需要一个到硬件的回调，由eoi提供，eoi表示end of interrupt，即中断结束
	void		(*eoi)(unsigned int irq);

	//标记中断处理在电流层次的结束。
	//如果一个中断在中断处理期间被禁用，那么该函数负责重新启用此类中断
	void		(*end)(unsigned int irq);
	//在多处理器系统中，可以使用set_affinity指定CPU来处理特定的IRQ.
	//这使得可以将IRQ分配给某些CPU(通常，SMP系统上的IRQ是平局发布到所有处理器的)。
	//该方法在单处理器系统上没用，可以设置为NULL指针
	void		(*set_affinity)(unsigned int irq, cpumask_t dest);
	int		(*retrigger)(unsigned int irq);
	//设置IRQ的电流类型。
	//该方法主要使用在ARM、PowerPC和SuperH机器上，其他系统不需要该方法，可以设置为NULL
	int		(*set_type)(unsigned int irq, unsigned int flow_type);
	int		(*set_wake)(unsigned int irq, unsigned int on);

	/* Currently used only by UML, might disappear one day.*/
#ifdef CONFIG_IRQ_RELEASE_METHOD
	void		(*release)(unsigned int irq, void *dev_id);
#endif
	/*
	 * For compatibility, ->typename is copied into ->name.
	 * Will disappear.
	 */
	const char	*typename;//为了兼容性而使用
};

/**
 * struct irq_desc - interrupt descriptor
 *
 * @handle_irq:		highlevel irq-events handler [if NULL, __do_IRQ()]
 * @chip:		low level interrupt hardware access
 * @msi_desc:		MSI descriptor
 * @handler_data:	per-IRQ data for the irq_chip methods
 * @chip_data:		platform-specific per-chip private data for the chip
 *			methods, to allow shared chip implementations
 * @action:		the irq action chain
 * @status:		status information
 * @depth:		disable-depth, for nested irq_disable() calls
 * @wake_depth:		enable depth, for multiple set_irq_wake() callers
 * @irq_count:		stats field to detect stalled irqs
 * @irqs_unhandled:	stats field for spurious unhandled interrupts
 * @last_unhandled:	aging timer for unhandled count
 * @lock:		locking for SMP
 * @affinity:		IRQ affinity on SMP
 * @cpu:		cpu index useful for balancing
 * @pending_mask:	pending rebalanced interrupts
 * @dir:		/proc/irq/ procfs entry
 * @affinity_entry:	/proc/irq/smp_affinity procfs entry on SMP
 * @name:		flow handler name for /proc/interrupts output
 */
//对于每个IRQ中断线，Linux都用一个irq_desc_t数据结构来描述，我们把它叫做IRQ描述符
struct irq_desc
{
	//电流层ISR由handle_irq提供。
	//每当发生中断时，特定于体系结构的代码都会调用handle_irq。
	//该函数负责使用chip中提供的特定于控制器的方法，进行处理中断所必须的一些底层操作。
	irq_flow_handler_t	handle_irq;
	//电流处理和芯片相关操作被封装在chip中。
	struct irq_chip		*chip;
	struct msi_desc		*msi_desc;
	//可以指向任意数据，该数据可以是特定于IRQ或处理程序的。
	void			*handler_data;
	//指向可能与chip相关的任意数据
	void			*chip_data;
	//提供了一个操作链，需要在中断发生时执行。
	//由中断通知的设备驱动程序，可以将与之相关的处理程序函数放置在此处
	struct irqaction	*action;	/* IRQ action list */
	//描述了IRQ的当前状态。
	//根据status当前值，内核很容易或者某个IRQ的状态，而无须了解底层实现的硬件相关特性。
	//当然只设置对应的标志位是不会产生预期效果的，还必须将新状态通知底层硬件。
	//因而，该标志只能通过特定于控制器的函数设置，这些函数同时还负责将设置信息同步到底层硬件。
	//在很多情况下，这必须使用汇编语言代码，或通过out命令向特定地址写入特定数值。
	unsigned int		status;		/* IRQ status */

	//用于确定IRQ电路是启用的还是禁用的。正值表示禁用，而0表示启用
	//为什么用正值表示禁用IRQ呢� 因为这使得内核能够区分启用和禁用的IRQ电路，
	//以及重复禁用同一中断的情形。这个值相当于一个计数器，内核其余部分的代码
	//每次禁用某个中断，则将对应的计数器加1；每次中断被再次启用，则将计数器减1。
	//当depth归0时，硬件才能再次使用对应的IRQ。这种方法能够支持对嵌套禁用中断的正确处理。
	unsigned int		depth;		/* nested irq disables */
	unsigned int		wake_depth;	/* nested wake enables */
	unsigned int		irq_count;	/* For detecting broken IRQs */
	unsigned int		irqs_unhandled;
	unsigned long		last_unhandled;	/* Aging timer for unhandled count */
	spinlock_t		lock;
#ifdef CONFIG_SMP
	cpumask_t		affinity;
	unsigned int		cpu;
#endif
#if defined(CONFIG_GENERIC_PENDING_IRQ) || defined(CONFIG_IRQBALANCE)
	cpumask_t		pending_mask;
#endif
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*dir;
#endif
	//指定了电流层处理程序的名称，将显示在/proc/interrupts中。
	//对边沿触发中断，通常是是"edge"，对电平触发中断，通常是"level"
	const char		*name;
} ____cacheline_internodealigned_in_smp;

extern struct irq_desc irq_desc[NR_IRQS];

/*
 * Migration helpers for obsolete names, they will go away:
 */
#define hw_interrupt_type	irq_chip //为了兼容IRQ子系统的前一版本
typedef struct irq_chip		hw_irq_controller;
#define no_irq_type		no_irq_chip
typedef struct irq_desc		irq_desc_t;

/*
 * Pick up the arch-dependent methods:
 */
#include <asm/hw_irq.h>

extern int setup_irq(unsigned int irq, struct irqaction *new);

#ifdef CONFIG_GENERIC_HARDIRQS

#ifndef handle_dynamic_tick
# define handle_dynamic_tick(a)		do { } while (0)
#endif

#ifdef CONFIG_SMP

#if defined(CONFIG_GENERIC_PENDING_IRQ) || defined(CONFIG_IRQBALANCE)

void set_pending_irq(unsigned int irq, cpumask_t mask);
void move_native_irq(int irq);
void move_masked_irq(int irq);

#else /* CONFIG_GENERIC_PENDING_IRQ || CONFIG_IRQBALANCE */

static inline void move_irq(int irq)
{
}

static inline void move_native_irq(int irq)
{
}

static inline void move_masked_irq(int irq)
{
}

static inline void set_pending_irq(unsigned int irq, cpumask_t mask)
{
}

#endif /* CONFIG_GENERIC_PENDING_IRQ */

extern int irq_set_affinity(unsigned int irq, cpumask_t cpumask);
extern int irq_can_set_affinity(unsigned int irq);

#else /* CONFIG_SMP */

#define move_native_irq(x)
#define move_masked_irq(x)

static inline int irq_set_affinity(unsigned int irq, cpumask_t cpumask)
{
	return -EINVAL;
}

static inline int irq_can_set_affinity(unsigned int irq) { return 0; }

#endif /* CONFIG_SMP */

#ifdef CONFIG_IRQBALANCE
extern void set_balance_irq_affinity(unsigned int irq, cpumask_t mask);
#else
static inline void set_balance_irq_affinity(unsigned int irq, cpumask_t mask)
{
}
#endif

#ifdef CONFIG_AUTO_IRQ_AFFINITY
extern int select_smp_affinity(unsigned int irq);
#else
static inline int select_smp_affinity(unsigned int irq)
{
	return 1;
}
#endif

extern int no_irq_affinity;

static inline int irq_balancing_disabled(unsigned int irq)
{
	return irq_desc[irq].status & IRQ_NO_BALANCING_MASK;
}

/* Handle irq action chains: */
extern int handle_IRQ_event(unsigned int irq, struct irqaction *action);

/*
 * Built-in IRQ handlers for various IRQ types,
 * callable via desc->chip->handle_irq()
 */
extern void fastcall handle_level_irq(unsigned int irq, struct irq_desc *desc);
extern void fastcall handle_fasteoi_irq(unsigned int irq, struct irq_desc *desc);
extern void fastcall handle_edge_irq(unsigned int irq, struct irq_desc *desc);
extern void fastcall handle_simple_irq(unsigned int irq, struct irq_desc *desc);
extern void fastcall handle_percpu_irq(unsigned int irq, struct irq_desc *desc);
extern void fastcall handle_bad_irq(unsigned int irq, struct irq_desc *desc);

/*
 * Monolithic do_IRQ implementation.
 * (is an explicit fastcall, because i386 4KSTACKS calls it from assembly)
 */
#ifndef CONFIG_GENERIC_HARDIRQS_NO__DO_IRQ
extern fastcall unsigned int __do_IRQ(unsigned int irq);
#endif

/*
 * Architectures call this to let the generic IRQ layer
 * handle an interrupt. If the descriptor is attached to an
 * irqchip-style controller then we call the ->handle_irq() handler,
 * and it calls __do_IRQ() if it's attached to an irqtype-style controller.
 */
static inline void generic_handle_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_desc + irq;

#ifdef CONFIG_GENERIC_HARDIRQS_NO__DO_IRQ
	desc->handle_irq(irq, desc);
#else
	if (likely(desc->handle_irq))
		desc->handle_irq(irq, desc);
	else
		__do_IRQ(irq);
#endif
}

/* Handling of unhandled and spurious interrupts: */
extern void note_interrupt(unsigned int irq, struct irq_desc *desc,
			   int action_ret);

/* Resending of interrupts :*/
void check_irq_resend(struct irq_desc *desc, unsigned int irq);

/* Enable/disable irq debugging output: */
extern int noirqdebug_setup(char *str);

/* Checks whether the interrupt can be requested by request_irq(): */
extern int can_request_irq(unsigned int irq, unsigned long irqflags);

/* Dummy irq-chip implementations: */
extern struct irq_chip no_irq_chip;
extern struct irq_chip dummy_irq_chip;

extern void
set_irq_chip_and_handler(unsigned int irq, struct irq_chip *chip,
			 irq_flow_handler_t handle);
extern void
set_irq_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name);

extern void
__set_irq_handler(unsigned int irq, irq_flow_handler_t handle, int is_chained,
		  const char *name);

/* caller has locked the irq_desc and both params are valid */
static inline void __set_irq_handler_unlocked(int irq,
					      irq_flow_handler_t handler)
{
	irq_desc[irq].handle_irq = handler;
}

/*
 * Set a highlevel flow handler for a given IRQ:
 */
//为某个给定的IRQ编号设置电流处理程序
static inline void
set_irq_handler(unsigned int irq, irq_flow_handler_t handle)
{
	__set_irq_handler(irq, handle, 0, NULL);
}

/*
 * Set a highlevel chained flow handler for a given IRQ.
 * (a chained handler is automatically enabled and set to
 *  IRQ_NOREQUEST and IRQ_NOPROBE)
 */
//为某个给定的IRQ编号设置电流处理程序，处理程序必须处理共享的中断。
//这会设置irq_desc[irq]->status中的标志位IRQ_NOREQUEST和IRQ_NOPROBE
static inline void
set_irq_chained_handler(unsigned int irq,
			irq_flow_handler_t handle)
{
	__set_irq_handler(irq, handle, 1, NULL);
}

/* Handle dynamic irq creation and destruction */
extern int create_irq(void);
extern void destroy_irq(unsigned int irq);

/* Test to see if a driver has successfully requested an irq */
static inline int irq_has_action(unsigned int irq)
{
	struct irq_desc *desc = irq_desc + irq;
	return desc->action != NULL;
}

/* Dynamic irq helper functions */
extern void dynamic_irq_init(unsigned int irq);
extern void dynamic_irq_cleanup(unsigned int irq);

/* Set/get chip/data for an IRQ: */
extern int set_irq_chip(unsigned int irq, struct irq_chip *chip);
extern int set_irq_data(unsigned int irq, void *data);
extern int set_irq_chip_data(unsigned int irq, void *data);
extern int set_irq_type(unsigned int irq, unsigned int type);
extern int set_irq_msi(unsigned int irq, struct msi_desc *entry);

#define get_irq_chip(irq)	(irq_desc[irq].chip)
#define get_irq_chip_data(irq)	(irq_desc[irq].chip_data)
#define get_irq_data(irq)	(irq_desc[irq].handler_data)
#define get_irq_msi(irq)	(irq_desc[irq].msi_desc)

#endif /* CONFIG_GENERIC_HARDIRQS */

#endif /* !CONFIG_S390 */

#endif /* _LINUX_IRQ_H */
