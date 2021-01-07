#ifndef _LINUX_IRQDESC_H
#define _LINUX_IRQDESC_H

/*
 * Core internal functions to deal with irq descriptors
 */

struct irq_affinity_notify;
struct proc_dir_entry;
struct module;
struct irq_desc;
struct irq_domain;
struct pt_regs;

/**
 * struct irq_desc - interrupt descriptor
 * @irq_common_data:	per irq and chip data passed down to chip functions
 * @kstat_irqs:		irq stats per cpu
 * @handle_irq:		highlevel irq-events handler
 * @preflow_handler:	handler called before the flow handler (currently used by sparc)
 * @action:		the irq action chain
 * @status:		status information
 * @core_internal_state__do_not_mess_with_it: core internal status information
 * @depth:		disable-depth, for nested irq_disable() calls
 * @wake_depth:		enable depth, for multiple irq_set_irq_wake() callers
 * @irq_count:		stats field to detect stalled irqs
 * @last_unhandled:	aging timer for unhandled count
 * @irqs_unhandled:	stats field for spurious unhandled interrupts
 * @threads_handled:	stats field for deferred spurious detection of threaded handlers
 * @threads_handled_last: comparator field for deferred spurious detection of theraded handlers
 * @lock:		locking for SMP
 * @affinity_hint:	hint to user space for preferred irq affinity
 * @affinity_notify:	context for notification of affinity changes
 * @pending_mask:	pending rebalanced interrupts
 * @threads_oneshot:	bitfield to handle shared oneshot threads
 * @threads_active:	number of irqaction threads currently running
 * @wait_for_threads:	wait queue for sync_irq to wait for threaded handlers
 * @nr_actions:		number of installed actions on this descriptor
 * @no_suspend_depth:	number of irqactions on a irq descriptor with
 *			IRQF_NO_SUSPEND set
 * @force_resume_depth:	number of irqactions on a irq descriptor with
 *			IRQF_FORCE_RESUME set
 * @dir:		/proc/irq/ procfs entry
 * @name:		flow handler name for /proc/interrupts output
 */
/**
 * 中断描述符
 */
struct irq_desc {
	struct irq_common_data	irq_common_data;
	//中断控制器相关的数据
	struct irq_data		irq_data;
	//中断在每个CPU上的执行状态
	unsigned int __percpu	*kstat_irqs;
	/**
	 * 高级irq事件事件
	 * 即高级事件处理回调
	 * 如边缘、电平两种方式的中断有不同的处理方式。
	 */
	irq_flow_handler_t	handle_irq;
#ifdef CONFIG_IRQ_PREFLOW_FASTEOI
	//arm上未用
	irq_preflow_handler_t	preflow_handler;
#endif
	//注册的irq回调链表
	struct irqaction	*action;	/* IRQ action list */
	//和irq_common_data中的状态有什么区别?
	unsigned int		status_use_accessors;
	//内部状态，别动
	unsigned int		core_internal_state__do_not_mess_with_it;
	//禁止该中断的次数
	unsigned int		depth;		/* nested irq disables */
	//唤醒该中断的次数，用于电源管理
	unsigned int		wake_depth;	/* nested wake enables */
	//发生的中断次数，用于检测硬件故障。
	unsigned int		irq_count;	/* For detecting broken IRQs */
	//上次未处理该中断的时间。
	unsigned long		last_unhandled;	/* Aging timer for unhandled count */
	//未处理的中断次数
	unsigned int		irqs_unhandled;
	//???
	atomic_t		threads_handled;
	int			threads_handled_last;
	//保护该数据结构的自旋锁
	raw_spinlock_t		lock;
	//在哪些核上面打开此中断
	struct cpumask		*percpu_enabled;
#ifdef CONFIG_SMP
	const struct cpumask	*affinity_hint;
	struct irq_affinity_notify *affinity_notify;
#ifdef CONFIG_GENERIC_PENDING_IRQ
	cpumask_var_t		pending_mask;
#endif
#endif
	unsigned long		threads_oneshot;
	//正在运行该中断irqaction的线程数量。
	atomic_t		threads_active;
	//该中断sync等待队列
	wait_queue_head_t       wait_for_threads;
#ifdef CONFIG_PM_SLEEP
	unsigned int		nr_actions;
	unsigned int		no_suspend_depth;
	unsigned int		cond_suspend_depth;
	unsigned int		force_resume_depth;
#endif
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*dir;
#endif
	int			parent_irq;
	struct module		*owner;
	//proc中的名称
	const char		*name;
} ____cacheline_internodealigned_in_smp;

#ifdef CONFIG_SPARSE_IRQ
extern void irq_lock_sparse(void);
extern void irq_unlock_sparse(void);
#else
static inline void irq_lock_sparse(void) { }
static inline void irq_unlock_sparse(void) { }
extern struct irq_desc irq_desc[NR_IRQS];
#endif

static inline struct irq_desc *irq_data_to_desc(struct irq_data *data)
{
	return container_of(data->common, struct irq_desc, irq_common_data);
}

static inline unsigned int irq_desc_get_irq(struct irq_desc *desc)
{
	return desc->irq_data.irq;
}

static inline struct irq_data *irq_desc_get_irq_data(struct irq_desc *desc)
{
	return &desc->irq_data;
}

static inline struct irq_chip *irq_desc_get_chip(struct irq_desc *desc)
{
	return desc->irq_data.chip;
}

static inline void *irq_desc_get_chip_data(struct irq_desc *desc)
{
	return desc->irq_data.chip_data;
}

static inline void *irq_desc_get_handler_data(struct irq_desc *desc)
{
	return desc->irq_common_data.handler_data;
}

static inline struct msi_desc *irq_desc_get_msi_desc(struct irq_desc *desc)
{
	return desc->irq_common_data.msi_desc;
}

/*
 * Architectures call this to let the generic IRQ layer
 * handle an interrupt.
 */
static inline void generic_handle_irq_desc(struct irq_desc *desc)
{
	/**
	 * 调用描述符中的回调，例如handle_level_irq.
	 * 通过__irq_set_handler设置回调函数。
	 */
	desc->handle_irq(desc);
}

int generic_handle_irq(unsigned int irq);

#ifdef CONFIG_HANDLE_DOMAIN_IRQ
/*
 * Convert a HW interrupt number to a logical one using a IRQ domain,
 * and handle the result interrupt number. Return -EINVAL if
 * conversion failed. Providing a NULL domain indicates that the
 * conversion has already been done.
 */
int __handle_domain_irq(struct irq_domain *domain, unsigned int hwirq,
			bool lookup, struct pt_regs *regs);

static inline int handle_domain_irq(struct irq_domain *domain,
				    unsigned int hwirq, struct pt_regs *regs)
{
	return __handle_domain_irq(domain, hwirq, true, regs);
}
#endif

/* Test to see if a driver has successfully requested an irq */
static inline int irq_desc_has_action(struct irq_desc *desc)
{
	return desc->action != NULL;
}

static inline int irq_has_action(unsigned int irq)
{
	return irq_desc_has_action(irq_to_desc(irq));
}

/**
 * irq_set_handler_locked - Set irq handler from a locked region
 * @data:	Pointer to the irq_data structure which identifies the irq
 * @handler:	Flow control handler function for this interrupt
 *
 * Sets the handler in the irq descriptor associated to @data.
 *
 * Must be called with irq_desc locked and valid parameters. Typical
 * call site is the irq_set_type() callback.
 */
static inline void irq_set_handler_locked(struct irq_data *data,
					  irq_flow_handler_t handler)
{
	struct irq_desc *desc = irq_data_to_desc(data);

	desc->handle_irq = handler;
}

/**
 * irq_set_chip_handler_name_locked - Set chip, handler and name from a locked region
 * @data:	Pointer to the irq_data structure for which the chip is set
 * @chip:	Pointer to the new irq chip
 * @handler:	Flow control handler function for this interrupt
 * @name:	Name of the interrupt
 *
 * Replace the irq chip at the proper hierarchy level in @data and
 * sets the handler and name in the associated irq descriptor.
 *
 * Must be called with irq_desc locked and valid parameters.
 */
static inline void
irq_set_chip_handler_name_locked(struct irq_data *data, struct irq_chip *chip,
				 irq_flow_handler_t handler, const char *name)
{
	struct irq_desc *desc = irq_data_to_desc(data);

	desc->handle_irq = handler;
	desc->name = name;
	data->chip = chip;
}

static inline int irq_balancing_disabled(unsigned int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	return desc->status_use_accessors & IRQ_NO_BALANCING_MASK;
}

static inline int irq_is_percpu(unsigned int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	return desc->status_use_accessors & IRQ_PER_CPU;
}

static inline void
irq_set_lockdep_class(unsigned int irq, struct lock_class_key *class)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc)
		lockdep_set_class(&desc->lock, class);
}

#ifdef CONFIG_IRQ_PREFLOW_FASTEOI
static inline void
__irq_set_preflow_handler(unsigned int irq, irq_preflow_handler_t handler)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	desc->preflow_handler = handler;
}
#endif

#endif
