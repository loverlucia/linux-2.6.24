#ifndef _LINUX_BLKDEV_H
#define _LINUX_BLKDEV_H

#ifdef CONFIG_BLOCK

#include <linux/sched.h>
#include <linux/major.h>
#include <linux/genhd.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/wait.h>
#include <linux/mempool.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/stringify.h>
#include <linux/bsg.h>

#include <asm/scatterlist.h>

struct scsi_ioctl_command;

struct request_queue;
typedef struct request_queue request_queue_t __deprecated;
struct elevator_queue;
typedef struct elevator_queue elevator_t;
struct request_pm_state;
struct blk_trace;
struct request;
struct sg_io_hdr;

#define BLKDEV_MIN_RQ	4
#define BLKDEV_MAX_RQ	128	/* Default maximum */

/*
 * This is the per-process anticipatory I/O scheduler state.
 */
struct as_io_context {
	spinlock_t lock;

	void (*dtor)(struct as_io_context *aic); /* destructor */
	void (*exit)(struct as_io_context *aic); /* called on task exit */

	unsigned long state;
	atomic_t nr_queued; /* queued reads & sync writes */
	atomic_t nr_dispatched; /* number of requests gone to the drivers */

	/* IO History tracking */
	/* Thinktime */
	unsigned long last_end_request;
	unsigned long ttime_total;
	unsigned long ttime_samples;
	unsigned long ttime_mean;
	/* Layout pattern */
	unsigned int seek_samples;
	sector_t last_request_pos;
	u64 seek_total;
	sector_t seek_mean;
};

struct cfq_queue;
struct cfq_io_context {
	struct rb_node rb_node;
	void *key;

	struct cfq_queue *cfqq[2];

	struct io_context *ioc;

	unsigned long last_end_request;
	sector_t last_request_pos;

	unsigned long ttime_total;
	unsigned long ttime_samples;
	unsigned long ttime_mean;

	unsigned int seek_samples;
	u64 seek_total;
	sector_t seek_mean;

	struct list_head queue_list;

	void (*dtor)(struct io_context *); /* destructor */
	void (*exit)(struct io_context *); /* called on task exit */
};

/*
 * This is the per-process I/O subsystem state.  It is refcounted and
 * kmalloc'ed. Currently all fields are modified in process io context
 * (apart from the atomic refcount), so require no locking.
 */
struct io_context {
	atomic_t refcount;
	struct task_struct *task;

	unsigned int ioprio_changed;

	/*
	 * For request batching
	 */
	unsigned long last_waited; /* Time last woken after wait for request */
	int nr_batch_requests;     /* Number of requests left in the batch */

	struct as_io_context *aic;
	struct rb_root cic_root;
	void *ioc_data;
};

void put_io_context(struct io_context *ioc);
void exit_io_context(void);
struct io_context *get_io_context(gfp_t gfp_flags, int node);
void copy_io_context(struct io_context **pdst, struct io_context **psrc);
void swap_io_context(struct io_context **ioc1, struct io_context **ioc2);

struct request;
typedef void (rq_end_io_fn)(struct request *, int);

struct request_list {
	int count[2];
	int starved[2];
	int elvpriv;
	mempool_t *rq_pool;
	wait_queue_head_t wait[2];
};

/*
 * request command types
 */
//最常见的请求类型是REQ_TYPE_FS，它用于与块设备之间的实际数据传输。
//其余类型的请求用来发送各种类型的命令，请参看具体的设备驱动程序源文件中的注释
enum rq_cmd_type_bits {
	REQ_TYPE_FS		= 1,	/* fs request */ //文件系统请求
	REQ_TYPE_BLOCK_PC,		/* scsi command */ //scsi命令
	REQ_TYPE_SENSE,			/* sense request */ //请求检测，用于scsi/atapi设备
	REQ_TYPE_PM_SUSPEND,		/* suspend request */ //电源管理命令，要求暂停设备
	REQ_TYPE_PM_RESUME,		/* resume request */ //电源管理命令，要求唤醒设备
	REQ_TYPE_PM_SHUTDOWN,		/* shutdown request */ //电源管理命令，要求将设备停机
	REQ_TYPE_FLUSH,			/* flush request */ //刷出请求
	REQ_TYPE_SPECIAL,		/* driver defined type */ //驱动程序定义的请求类型
	REQ_TYPE_LINUX_BLOCK,		/* generic block layer message */ //一般性的块设备层消息
	/*
	 * for ATA/ATAPI devices. this really doesn't belong here, ide should
	 * use REQ_TYPE_SPECIAL and use rq->cmd[0] with the range of driver
	 * private REQ_LB opcodes to differentiate what type of request this is
	 */
	REQ_TYPE_ATA_CMD,
	REQ_TYPE_ATA_TASK,
	REQ_TYPE_ATA_TASKFILE,
	REQ_TYPE_ATA_PC,
};

/*
 * For request of type REQ_TYPE_LINUX_BLOCK, rq->cmd[0] is the opcode being
 * sent down (similar to how REQ_TYPE_BLOCK_PC means that ->cmd[] holds a
 * SCSI cdb.
 *
 * 0x00 -> 0x3f are driver private, to be used for whatever purpose they need,
 * typically to differentiate REQ_TYPE_SPECIAL requests.
 *
 */
enum {
	/*
	 * just examples for now
	 */
	REQ_LB_OP_EJECT	= 0x40,		/* eject request */
	REQ_LB_OP_FLUSH = 0x41,		/* flush device */
};

/*
 * request type modified bits. first three bits match BIO_RW* bits, important
 */
//__REQ_RW特别重要，因为它指出了数据传输的方向。如果该比特位置位，则将数据写入设备；否则，从设备读取数据。
//剩余的比特位用于发送特殊的与设备相关的命令，
//可以设置屏障(如果设备在请求序列中遇到一个屏障，那么在执行任何其他操作之前，必须将所有待决请求全部处理完毕)，或传输控制码。
enum rq_flag_bits {
	__REQ_RW,		/* not set, read. set, write */ //未置位，读请求；置位，写请求
	__REQ_FAILFAST,		/* no low level driver retries */ //底层驱动程序不进行重试
	__REQ_SORTED,		/* elevator knows about this request */ //该请求由I/O调度器使用
	__REQ_SOFTBARRIER,	/* may not be passed by ioscheduler */ //不能由I/O调度器传递
	__REQ_HARDBARRIER,	/* may not be passed by drive either */
	__REQ_FUA,		/* forced unit access */
	__REQ_NOMERGE,		/* don't touch this for merging */
	__REQ_STARTED,		/* drive already may have started this one */
	__REQ_DONTPREP,		/* don't call prep for this one */
	__REQ_QUEUED,		/* uses queueing */
	__REQ_ELVPRIV,		/* elevator private data attached */
	__REQ_FAILED,		/* set if the request failed */
	__REQ_QUIET,		/* don't worry about errors */
	__REQ_PREEMPT,		/* set for "ide_preempt" requests */
	__REQ_ORDERED_COLOR,	/* is before or after barrier */
	__REQ_RW_SYNC,		/* request is sync (O_DIRECT) */
	__REQ_ALLOCED,		/* request came from our alloc pool */
	__REQ_RW_META,		/* metadata io request */
	__REQ_NR_BITS,		/* stops here */
};

#define REQ_RW		(1 << __REQ_RW)
#define REQ_FAILFAST	(1 << __REQ_FAILFAST)
#define REQ_SORTED	(1 << __REQ_SORTED)
#define REQ_SOFTBARRIER	(1 << __REQ_SOFTBARRIER)
#define REQ_HARDBARRIER	(1 << __REQ_HARDBARRIER)
#define REQ_FUA		(1 << __REQ_FUA)
#define REQ_NOMERGE	(1 << __REQ_NOMERGE)
#define REQ_STARTED	(1 << __REQ_STARTED)
#define REQ_DONTPREP	(1 << __REQ_DONTPREP)
#define REQ_QUEUED	(1 << __REQ_QUEUED)
#define REQ_ELVPRIV	(1 << __REQ_ELVPRIV)
#define REQ_FAILED	(1 << __REQ_FAILED)
#define REQ_QUIET	(1 << __REQ_QUIET)
#define REQ_PREEMPT	(1 << __REQ_PREEMPT)
#define REQ_ORDERED_COLOR	(1 << __REQ_ORDERED_COLOR)
#define REQ_RW_SYNC	(1 << __REQ_RW_SYNC)
#define REQ_ALLOCED	(1 << __REQ_ALLOCED)
#define REQ_RW_META	(1 << __REQ_RW_META)

#define BLK_MAX_CDB	16

/*
 * try to put the fields that are referenced together in the same cacheline
 */
//内核提供数据结构request以描述发送给块设备的请求
struct request {
	//请求需要保存在请求队列上。这种队列使用双链表实现，queuelist提供了所需的链表元素。(这只对异步完成请求有必要，通常不需要该链表)
	struct list_head queuelist;
	//在一个请求完成后，即所有需要的I/O操作都已经执行完毕，可以将其排到完成链表上，此时使用donelist作为链表元素
	struct list_head donelist;

	//指向该请求所属的请求队列(如果有的话)
	struct request_queue *q;
	//包含用于请求的一组通用标志
	unsigned int cmd_flags;
	//请求的类型
	enum rq_cmd_type_bits cmd_type;

	/* Maintain bio traversal state for part by part I/O submission.
	 * hard_* are block layer internals, no driver should touch them!
	 */
	//hard_sector、hard_cur_sectors和hard_nr_sectors与结构中没有hard_前缀的对应成员语义相同，但涉及的是实际硬件而非虚拟设备。
	//通常两组变量的值相同，但在使用RAID或逻辑卷管理器(Logical Volume Manager)时可能会有差别，
	//因为这些机制实际上是将几个物理设备合并为一个虚拟设备
	
	//指定了数据传输的起始扇区
	sector_t sector;		/* next sector to submit */ 
	sector_t hard_sector;		/* next sector to complete */ 
	//指定了当前请求还需要传输的扇区数目
	unsigned long nr_sectors;	/* no. of sectors left to submit */ 
	unsigned long hard_nr_sectors;	/* no. of sectors left to complete */
	/* no. of sectors left to submit in the current segment */
	//表明了当前请求在当前段中还需要传输的扇区数目
	unsigned int current_nr_sectors; 

	/* no. of sectors left to complete in the current segment */
	unsigned int hard_cur_sectors; 

	//标识传输尚未完成的当前BIO实例。
	struct bio *bio;
	//指向最后一个BIO实例，因为一个请求中可使用多个BIO
	struct bio *biotail;

	struct hlist_node hash;	/* merge hash */
	/*
	 * The rb_node is only used inside the io scheduler, requests
	 * are pruned when moved to the dispatch queue. So let the
	 * completion_data share space with the rb_node.
	 */
	union {
		struct rb_node rb_node;	/* sort/lookup */
		void *completion_data;
	};

	/*
	 * two pointers are available for the IO schedulers, if they need
	 * more they have to dynamically allocate it.
	 */
	//elevator_private和elevator_private2是两个指向私有数据的指针。
	//可以通过当前处理请求的I/O调度器(传统上称为电梯，elevator)设置。
	void *elevator_private;
	void *elevator_private2;

	struct gendisk *rq_disk;
	unsigned long start_time;
	
	//在使用分散-聚集I/O操作时，nr_phys_segments和nr_hw_segments
	//分别指定了请求中段的数目和经过I/O MMU可能的重排序后之后的段的数目。
	/* Number of scatter-gather DMA addr+len pairs after
	 * physical address coalescing is performed.
	 */
	unsigned short nr_phys_segments;

	/* Number of scatter-gather addr+len pairs after
	 * physical and DMA remapping hardware coalescing is performed.
	 * This is the number of scatter-gather entries the driver
	 * will actually have to deal with after DMA mapping is done.
	 */
	unsigned short nr_hw_segments;

	unsigned short ioprio;

	void *special;
	char *buffer;

	int tag;
	int errors;

	int ref_count;

	/*
	 * when request is used as a packet command carrier
	 */
	//请求可用于向设备传送控制命令，更正式的将，请求可以用作数据包命令载体(packet command carrier)。
	//想要的命令在cmd数组中列出
	unsigned int cmd_len;
	unsigned char cmd[BLK_MAX_CDB];

	unsigned int data_len;
	unsigned int sense_len;
	void *data;
	void *sense;

	unsigned int timeout;
	int retries;

	/*
	 * completion callback.
	 */
	rq_end_io_fn *end_io;
	void *end_io_data;

	/* for bidi */
	struct request *next_rq;
};

/*
 * State information carried for REQ_TYPE_PM_SUSPEND and REQ_TYPE_PM_RESUME
 * requests. Some step values could eventually be made generic.
 */
struct request_pm_state
{
	/* PM state machine step value, currently driver specific */
	int	pm_step;
	/* requested PM state value (S1, S2, S3, S4, ...) */
	u32	pm_state;
	void*	data;		/* for driver use */
};

#include <linux/elevator.h>

typedef void (request_fn_proc) (struct request_queue *q);
typedef int (make_request_fn) (struct request_queue *q, struct bio *bio);
typedef int (prep_rq_fn) (struct request_queue *, struct request *);
typedef void (unplug_fn) (struct request_queue *);

struct bio_vec;
typedef int (merge_bvec_fn) (struct request_queue *, struct bio *, struct bio_vec *);
typedef void (prepare_flush_fn) (struct request_queue *, struct request *);
typedef void (softirq_done_fn)(struct request *);

enum blk_queue_state {
	Queue_down,
	Queue_up,
};

struct blk_queue_tag {
	struct request **tag_index;	/* map of busy tags */
	unsigned long *tag_map;		/* bit map of free/busy tags */
	int busy;			/* current depth */
	int max_depth;			/* what we will send to device */
	int real_max_depth;		/* what the array can hold */
	atomic_t refcnt;		/* map can be shared */
};

struct request_queue
{
	/*
	 * Together with queue_head for cacheline sharing //与queue_head一同用于缓存行共享
	 */
	//一个表头，用于构建一个I/O请求的双链表。
	//链表每个元素的数据类型都是request，代表向块设备读取数据的一个请求。
	//内核会重排该链表以取得更好的I/O性能(提供了几个算法来执行I/O调度器的任务)
	struct list_head	queue_head;
	struct request		*last_merge;
	//因为有各种方法可以重排请求，elevator成员以函数指针的形式将所需的函数群集起来。
	elevator_t		*elevator;

	/*
	 * the queue request freelist, one for reads and one for writes //队列中空闲请求的列表
	 */
	//用作request实例的缓存，还提供了两个计数器，用于记录可用的空闲输入和输出请求的数目
	struct request_list	rq;

	//用于向队列添加新请求的标准接口
	//在内核期望驱动程序执行某些工作时(如从底层设备读取数据，或向设备写入数据)，内核会自动调用该函数。
	//按内核的术语，该函数也称之为策略例程(strategy routine)
	request_fn_proc		*request_fn;
	//创建新请求。内核对该函数的标准实现向请求链表添加请求。
	//如果链表中有足够多的请求，则会调用特定于驱动程序的request_fn函数，以处理所有这些请求。
	//内核允许设备驱动程序定义自身的make_request_fn函数，因为某些设备(例如RAM磁盘)不使用队列，
	//这可能是由于按任意顺序访问数据都不会影响性能，也可能是由于驱动程序比内核更了解如何处理请求，
	//因而使用内核的标准方法不会带来好处(例如卷管理器)。但这种惯例还是比较罕见的。
	make_request_fn		*make_request_fn;
	//一个请求预备函数。
	//大多数驱动程序不使用该函数，会将对应的指针设置为NULL。
	//如果实现了该函数，它会产生所需的硬件命令，用于在发送实际的请求之前预备一个请求。
	//辅助函数blk_queue_prep_rq会设置给定队列的prep_rq_fn
	prep_rq_fn		*prep_rq_fn;
	//用于拔出一个块设备时调用。
	//插入的设备不会执行请求，而是将请求收集起来，在拔出时执行。
	//巧妙地使用该方法，能够提高块设备层的性能。
	unplug_fn		*unplug_fn;
	//确定是否允许向一个现存的请求增加更多数据。
	//由于请求队列的长度通常是固定的，限制了其中请求的数目，一次内核可使用这种机制来避免可能的问题。
	//但更专门的驱动程序，特别是复合设备的驱动，队列长度的限制可能不同，因此需要提供该函数。
	//内核提供了辅助例程blk_queue_merge_bvec来设置队列的merge_bvec_fn。
	merge_bvec_fn		*merge_bvec_fn;
	//在预备刷出队列时，即一次性执行所有待决请求之前，会调用prepare_flush_fn。
	//在该方法中，设备可以进行必要的清理。
	//辅助函数blk_queue_ordered可以用来向请求队列设置特定的方法。
	prepare_flush_fn	*prepare_flush_fn;
	//对于大的请求来说，完成请求，即完成所有I/O，可能是一个耗时的过程。
	//内核添加了使用软中断SoftIRQ异步完成请求的特性。
	//可以通过调用blk_complete_request要求异步完成请求，
	//softirq_done_fn在这种情况下用作回调函数，通知驱动程序请求已经完成。
	softirq_done_fn		*softirq_done_fn;

	/*
	 * Dispatch queue sorting
	 */
	sector_t		end_sector;
	struct request		*boundary_rq;

	/*
	 * Auto-unplugging state //自动拔出特性涉及的状态值
	 */
	//请求队列可以在系统超负荷时插入。
	//接下来新的请求都会处于未处理状态，直至队列"拔出"，该特性称之为队列插入(queue plugging)。
	//以unplug_为前缀的各个成员用于实现一种定时器机制，在一定时间间隔后自动"拔出"队列。
	//unplug_fn负责实际的拔出操作
	struct timer_list	unplug_timer;
	int			unplug_thresh;	/* After this many requests */ //累积请求数目的阈值
	unsigned long		unplug_delay;	/* After this many jiffies */ //累积时间的阈值，按jiffies计算
	struct work_struct	unplug_work;

	struct backing_dev_info	backing_dev_info;

	/*
	 * The queue owner gets to use this for whatever they like.
	 * ll_rw_blk doesn't touch it.
	 */
	void			*queuedata;

	/*
	 * queue needs bounce pages for pages above this limit //如果页帧号大于该值，则使用弹性缓冲区
	 */
	unsigned long		bounce_pfn;
	gfp_t			bounce_gfp;

	/*
	 * various queue flags, see QUEUE_* below
	 */
	//借助标志来控制队列的内部状态
	unsigned long		queue_flags;

	/*
	 * protects queue structures from reentrancy. ->__queue_lock should
	 * _never_ be used directly, it is queue private. always use
	 * ->queue_lock.
	 */
	spinlock_t		__queue_lock;
	spinlock_t		*queue_lock;

	/*
	 * queue kobject
	 */
	struct kobject kobj;

	/*
	 * queue settings //队列设置
	 */
	unsigned long		nr_requests;	/* Max # of requests */ //请求的最大数目
	unsigned int		nr_congestion_on;
	unsigned int		nr_congestion_off;
	unsigned int		nr_batching;
	//指定设备在单个请求中可以处理的扇区的最大数目。长度单位是局体设备的扇区长度(hardsect_size)
	unsigned int		max_sectors;
	//与max_phys_segments相同，但考虑了(可能的)I/O MMU所进行的重新映射。
	//该成员指定了驱动程序可以传递到设备的地址/长度对的最大数目
	unsigned int		max_hw_sectors;
	//指定用于运输不连续数据的分散-聚集请求中，不连续的段的最大数目
	unsigned short		max_phys_segments;
	unsigned short		max_hw_segments;
	//指定了设备的物理扇区长度，该值通常是512。只有少数非常新的设备使用不同的设置
	unsigned short		hardsect_size;
	unsigned int		max_segment_size;

	unsigned long		seg_boundary_mask;
	unsigned int		dma_alignment;

	struct blk_queue_tag	*queue_tags;
	struct list_head	tag_busy_list;

	unsigned int		nr_sorted;
	unsigned int		in_flight;

	/*
	 * sg stuff
	 */
	unsigned int		sg_timeout;
	unsigned int		sg_reserved_size;
	int			node;
#ifdef CONFIG_BLK_DEV_IO_TRACE
	struct blk_trace	*blk_trace;
#endif
	/*
	 * reserved for flush operations
	 */
	unsigned int		ordered, next_ordered, ordseq;
	int			orderr, ordcolor;
	struct request		pre_flush_rq, bar_rq, post_flush_rq;
	struct request		*orig_bar_rq;

	struct mutex		sysfs_lock;

#if defined(CONFIG_BLK_DEV_BSG)
	struct bsg_class_device bsg_dev;
#endif
};

#define QUEUE_FLAG_CLUSTER	0	/* cluster several segments into 1 */
#define QUEUE_FLAG_QUEUED	1	/* uses generic tag queueing */
#define QUEUE_FLAG_STOPPED	2	/* queue is stopped */
#define	QUEUE_FLAG_READFULL	3	/* read queue has been filled */
#define QUEUE_FLAG_WRITEFULL	4	/* write queue has been filled */
#define QUEUE_FLAG_DEAD		5	/* queue being torn down */
#define QUEUE_FLAG_REENTER	6	/* Re-entrancy avoidance */
#define QUEUE_FLAG_PLUGGED	7	/* queue is plugged */
#define QUEUE_FLAG_ELVSWITCH	8	/* don't use elevator, just do FIFO */
#define QUEUE_FLAG_BIDI		9	/* queue supports bidi requests */

enum {
	/*
	 * Hardbarrier is supported with one of the following methods.
	 *
	 * NONE		: hardbarrier unsupported
	 * DRAIN	: ordering by draining is enough
	 * DRAIN_FLUSH	: ordering by draining w/ pre and post flushes
	 * DRAIN_FUA	: ordering by draining w/ pre flush and FUA write
	 * TAG		: ordering by tag is enough
	 * TAG_FLUSH	: ordering by tag w/ pre and post flushes
	 * TAG_FUA	: ordering by tag w/ pre flush and FUA write
	 */
	QUEUE_ORDERED_NONE	= 0x00,
	QUEUE_ORDERED_DRAIN	= 0x01,
	QUEUE_ORDERED_TAG	= 0x02,

	QUEUE_ORDERED_PREFLUSH	= 0x10,
	QUEUE_ORDERED_POSTFLUSH	= 0x20,
	QUEUE_ORDERED_FUA	= 0x40,

	QUEUE_ORDERED_DRAIN_FLUSH = QUEUE_ORDERED_DRAIN |
			QUEUE_ORDERED_PREFLUSH | QUEUE_ORDERED_POSTFLUSH,
	QUEUE_ORDERED_DRAIN_FUA	= QUEUE_ORDERED_DRAIN |
			QUEUE_ORDERED_PREFLUSH | QUEUE_ORDERED_FUA,
	QUEUE_ORDERED_TAG_FLUSH	= QUEUE_ORDERED_TAG |
			QUEUE_ORDERED_PREFLUSH | QUEUE_ORDERED_POSTFLUSH,
	QUEUE_ORDERED_TAG_FUA	= QUEUE_ORDERED_TAG |
			QUEUE_ORDERED_PREFLUSH | QUEUE_ORDERED_FUA,

	/*
	 * Ordered operation sequence
	 */
	QUEUE_ORDSEQ_STARTED	= 0x01,	/* flushing in progress */
	QUEUE_ORDSEQ_DRAIN	= 0x02,	/* waiting for the queue to be drained */
	QUEUE_ORDSEQ_PREFLUSH	= 0x04,	/* pre-flushing in progress */
	QUEUE_ORDSEQ_BAR	= 0x08,	/* original barrier req in progress */
	QUEUE_ORDSEQ_POSTFLUSH	= 0x10,	/* post-flushing in progress */
	QUEUE_ORDSEQ_DONE	= 0x20,
};

#define blk_queue_plugged(q)	test_bit(QUEUE_FLAG_PLUGGED, &(q)->queue_flags)
#define blk_queue_tagged(q)	test_bit(QUEUE_FLAG_QUEUED, &(q)->queue_flags)
#define blk_queue_stopped(q)	test_bit(QUEUE_FLAG_STOPPED, &(q)->queue_flags)
#define blk_queue_flushing(q)	((q)->ordseq)

#define blk_fs_request(rq)	((rq)->cmd_type == REQ_TYPE_FS)
#define blk_pc_request(rq)	((rq)->cmd_type == REQ_TYPE_BLOCK_PC)
#define blk_special_request(rq)	((rq)->cmd_type == REQ_TYPE_SPECIAL)
#define blk_sense_request(rq)	((rq)->cmd_type == REQ_TYPE_SENSE)

#define blk_noretry_request(rq)	((rq)->cmd_flags & REQ_FAILFAST)
#define blk_rq_started(rq)	((rq)->cmd_flags & REQ_STARTED)

#define blk_account_rq(rq)	(blk_rq_started(rq) && blk_fs_request(rq))

#define blk_pm_suspend_request(rq)	((rq)->cmd_type == REQ_TYPE_PM_SUSPEND)
#define blk_pm_resume_request(rq)	((rq)->cmd_type == REQ_TYPE_PM_RESUME)
#define blk_pm_request(rq)	\
	(blk_pm_suspend_request(rq) || blk_pm_resume_request(rq))

#define blk_sorted_rq(rq)	((rq)->cmd_flags & REQ_SORTED)
#define blk_barrier_rq(rq)	((rq)->cmd_flags & REQ_HARDBARRIER)
#define blk_fua_rq(rq)		((rq)->cmd_flags & REQ_FUA)
#define blk_bidi_rq(rq)		((rq)->next_rq != NULL)
#define blk_empty_barrier(rq)	(blk_barrier_rq(rq) && blk_fs_request(rq) && !(rq)->hard_nr_sectors)

#define list_entry_rq(ptr)	list_entry((ptr), struct request, queuelist)

#define rq_data_dir(rq)		((rq)->cmd_flags & 1)

/*
 * We regard a request as sync, if it's a READ or a SYNC write.
 */
#define rq_is_sync(rq)		(rq_data_dir((rq)) == READ || (rq)->cmd_flags & REQ_RW_SYNC)
#define rq_is_meta(rq)		((rq)->cmd_flags & REQ_RW_META)

static inline int blk_queue_full(struct request_queue *q, int rw)
{
	if (rw == READ)
		return test_bit(QUEUE_FLAG_READFULL, &q->queue_flags);
	return test_bit(QUEUE_FLAG_WRITEFULL, &q->queue_flags);
}

static inline void blk_set_queue_full(struct request_queue *q, int rw)
{
	if (rw == READ)
		set_bit(QUEUE_FLAG_READFULL, &q->queue_flags);
	else
		set_bit(QUEUE_FLAG_WRITEFULL, &q->queue_flags);
}

static inline void blk_clear_queue_full(struct request_queue *q, int rw)
{
	if (rw == READ)
		clear_bit(QUEUE_FLAG_READFULL, &q->queue_flags);
	else
		clear_bit(QUEUE_FLAG_WRITEFULL, &q->queue_flags);
}


/*
 * mergeable request must not have _NOMERGE or _BARRIER bit set, nor may
 * it already be started by driver.
 */
#define RQ_NOMERGE_FLAGS	\
	(REQ_NOMERGE | REQ_STARTED | REQ_HARDBARRIER | REQ_SOFTBARRIER)
#define rq_mergeable(rq)	\
	(!((rq)->cmd_flags & RQ_NOMERGE_FLAGS) && blk_fs_request((rq)))

/*
 * q->prep_rq_fn return values
 */
#define BLKPREP_OK		0	/* serve it */
#define BLKPREP_KILL		1	/* fatal error, kill */
#define BLKPREP_DEFER		2	/* leave on queue */

extern unsigned long blk_max_low_pfn, blk_max_pfn;

/*
 * standard bounce addresses:
 *
 * BLK_BOUNCE_HIGH	: bounce all highmem pages
 * BLK_BOUNCE_ANY	: don't bounce anything
 * BLK_BOUNCE_ISA	: bounce pages above ISA DMA boundary
 */
#define BLK_BOUNCE_HIGH		((u64)blk_max_low_pfn << PAGE_SHIFT)
#define BLK_BOUNCE_ANY		((u64)blk_max_pfn << PAGE_SHIFT)
#define BLK_BOUNCE_ISA		(ISA_DMA_THRESHOLD)

/*
 * default timeout for SG_IO if none specified
 */
#define BLK_DEFAULT_SG_TIMEOUT	(60 * HZ)

#ifdef CONFIG_BOUNCE
extern int init_emergency_isa_pool(void);
extern void blk_queue_bounce(struct request_queue *q, struct bio **bio);
#else
static inline int init_emergency_isa_pool(void)
{
	return 0;
}
static inline void blk_queue_bounce(struct request_queue *q, struct bio **bio)
{
}
#endif /* CONFIG_MMU */

struct req_iterator {
	int i;
	struct bio *bio;
};

/* This should not be used directly - use rq_for_each_segment */
#define __rq_for_each_bio(_bio, rq)	\
	if ((rq->bio))			\
		for (_bio = (rq)->bio; _bio; _bio = _bio->bi_next)

#define rq_for_each_segment(bvl, _rq, _iter)			\
	__rq_for_each_bio(_iter.bio, _rq)			\
		bio_for_each_segment(bvl, _iter.bio, _iter.i)

#define rq_iter_last(rq, _iter)					\
		(_iter.bio->bi_next == NULL && _iter.i == _iter.bio->bi_vcnt-1)

extern int blk_register_queue(struct gendisk *disk);
extern void blk_unregister_queue(struct gendisk *disk);
extern void register_disk(struct gendisk *dev);
extern void generic_make_request(struct bio *bio);
extern void blk_put_request(struct request *);
extern void __blk_put_request(struct request_queue *, struct request *);
extern void blk_end_sync_rq(struct request *rq, int error);
extern struct request *blk_get_request(struct request_queue *, int, gfp_t);
extern void blk_insert_request(struct request_queue *, struct request *, int, void *);
extern void blk_requeue_request(struct request_queue *, struct request *);
extern void blk_plug_device(struct request_queue *);
extern int blk_remove_plug(struct request_queue *);
extern void blk_recount_segments(struct request_queue *, struct bio *);
extern int scsi_cmd_ioctl(struct file *, struct request_queue *,
			  struct gendisk *, unsigned int, void __user *);
extern int sg_scsi_ioctl(struct file *, struct request_queue *,
		struct gendisk *, struct scsi_ioctl_command __user *);

/*
 * Temporary export, until SCSI gets fixed up.
 */
extern int blk_rq_append_bio(struct request_queue *q, struct request *rq,
			     struct bio *bio);

/*
 * A queue has just exitted congestion.  Note this in the global counter of
 * congested queues, and wake up anyone who was waiting for requests to be
 * put back.
 */
static inline void blk_clear_queue_congested(struct request_queue *q, int rw)
{
	clear_bdi_congested(&q->backing_dev_info, rw);
}

/*
 * A queue has just entered congestion.  Flag that in the queue's VM-visible
 * state flags and increment the global gounter of congested queues.
 */
static inline void blk_set_queue_congested(struct request_queue *q, int rw)
{
	set_bdi_congested(&q->backing_dev_info, rw);
}

extern void blk_start_queue(struct request_queue *q);
extern void blk_stop_queue(struct request_queue *q);
extern void blk_sync_queue(struct request_queue *q);
extern void __blk_stop_queue(struct request_queue *q);
extern void blk_run_queue(struct request_queue *);
extern void blk_start_queueing(struct request_queue *);
extern int blk_rq_map_user(struct request_queue *, struct request *, void __user *, unsigned long);
extern int blk_rq_unmap_user(struct bio *);
extern int blk_rq_map_kern(struct request_queue *, struct request *, void *, unsigned int, gfp_t);
extern int blk_rq_map_user_iov(struct request_queue *, struct request *,
			       struct sg_iovec *, int, unsigned int);
extern int blk_execute_rq(struct request_queue *, struct gendisk *,
			  struct request *, int);
extern void blk_execute_rq_nowait(struct request_queue *, struct gendisk *,
				  struct request *, int, rq_end_io_fn *);
extern int blk_verify_command(unsigned char *, int);
extern void blk_unplug(struct request_queue *q);

static inline struct request_queue *bdev_get_queue(struct block_device *bdev)
{
	return bdev->bd_disk->queue;
}

static inline void blk_run_backing_dev(struct backing_dev_info *bdi,
				       struct page *page)
{
	if (bdi && bdi->unplug_io_fn)
		bdi->unplug_io_fn(bdi, page);
}

static inline void blk_run_address_space(struct address_space *mapping)
{
	if (mapping)
		blk_run_backing_dev(mapping->backing_dev_info, NULL);
}

/*
 * end_request() and friends. Must be called with the request queue spinlock
 * acquired. All functions called within end_request() _must_be_ atomic.
 *
 * Several drivers define their own end_request and call
 * end_that_request_first() and end_that_request_last()
 * for parts of the original function. This prevents
 * code duplication in drivers.
 */
extern int end_that_request_first(struct request *, int, int);
extern int end_that_request_chunk(struct request *, int, int);
extern void end_that_request_last(struct request *, int);
extern void end_request(struct request *, int);
extern void end_queued_request(struct request *, int);
extern void end_dequeued_request(struct request *, int);
extern void blk_complete_request(struct request *);

/*
 * end_that_request_first/chunk() takes an uptodate argument. we account
 * any value <= as an io error. 0 means -EIO for compatability reasons,
 * any other < 0 value is the direct error type. An uptodate value of
 * 1 indicates successful io completion
 */
#define end_io_error(uptodate)	(unlikely((uptodate) <= 0))

static inline void blkdev_dequeue_request(struct request *req)
{
	elv_dequeue_request(req->q, req);
}

/*
 * Access functions for manipulating queue properties
 */
extern struct request_queue *blk_init_queue_node(request_fn_proc *rfn,
					spinlock_t *lock, int node_id);
extern struct request_queue *blk_init_queue(request_fn_proc *, spinlock_t *);
extern void blk_cleanup_queue(struct request_queue *);
extern void blk_queue_make_request(struct request_queue *, make_request_fn *);
extern void blk_queue_bounce_limit(struct request_queue *, u64);
extern void blk_queue_max_sectors(struct request_queue *, unsigned int);
extern void blk_queue_max_phys_segments(struct request_queue *, unsigned short);
extern void blk_queue_max_hw_segments(struct request_queue *, unsigned short);
extern void blk_queue_max_segment_size(struct request_queue *, unsigned int);
extern void blk_queue_hardsect_size(struct request_queue *, unsigned short);
extern void blk_queue_stack_limits(struct request_queue *t, struct request_queue *b);
extern void blk_queue_segment_boundary(struct request_queue *, unsigned long);
extern void blk_queue_prep_rq(struct request_queue *, prep_rq_fn *pfn);
extern void blk_queue_merge_bvec(struct request_queue *, merge_bvec_fn *);
extern void blk_queue_dma_alignment(struct request_queue *, int);
extern void blk_queue_softirq_done(struct request_queue *, softirq_done_fn *);
extern struct backing_dev_info *blk_get_backing_dev_info(struct block_device *bdev);
extern int blk_queue_ordered(struct request_queue *, unsigned, prepare_flush_fn *);
extern int blk_do_ordered(struct request_queue *, struct request **);
extern unsigned blk_ordered_cur_seq(struct request_queue *);
extern unsigned blk_ordered_req_seq(struct request *);
extern void blk_ordered_complete_seq(struct request_queue *, unsigned, int);

extern int blk_rq_map_sg(struct request_queue *, struct request *, struct scatterlist *);
extern void blk_dump_rq_flags(struct request *, char *);
extern void generic_unplug_device(struct request_queue *);
extern void __generic_unplug_device(struct request_queue *);
extern long nr_blockdev_pages(void);

int blk_get_queue(struct request_queue *);
struct request_queue *blk_alloc_queue(gfp_t);
struct request_queue *blk_alloc_queue_node(gfp_t, int);
extern void blk_put_queue(struct request_queue *);

/*
 * tag stuff
 */
#define blk_queue_tag_depth(q)		((q)->queue_tags->busy)
#define blk_queue_tag_queue(q)		((q)->queue_tags->busy < (q)->queue_tags->max_depth)
#define blk_rq_tagged(rq)		((rq)->cmd_flags & REQ_QUEUED)
extern int blk_queue_start_tag(struct request_queue *, struct request *);
extern struct request *blk_queue_find_tag(struct request_queue *, int);
extern void blk_queue_end_tag(struct request_queue *, struct request *);
extern int blk_queue_init_tags(struct request_queue *, int, struct blk_queue_tag *);
extern void blk_queue_free_tags(struct request_queue *);
extern int blk_queue_resize_tags(struct request_queue *, int);
extern void blk_queue_invalidate_tags(struct request_queue *);
extern struct blk_queue_tag *blk_init_tags(int);
extern void blk_free_tags(struct blk_queue_tag *);

static inline struct request *blk_map_queue_find_tag(struct blk_queue_tag *bqt,
						int tag)
{
	if (unlikely(bqt == NULL || tag >= bqt->real_max_depth))
		return NULL;
	return bqt->tag_index[tag];
}

extern int blkdev_issue_flush(struct block_device *, sector_t *);

#define MAX_PHYS_SEGMENTS 128
#define MAX_HW_SEGMENTS 128
#define SAFE_MAX_SECTORS 255
#define BLK_DEF_MAX_SECTORS 1024

#define MAX_SEGMENT_SIZE	65536

#define blkdev_entry_to_request(entry) list_entry((entry), struct request, queuelist)

static inline int queue_hardsect_size(struct request_queue *q)
{
	int retval = 512;

	if (q && q->hardsect_size)
		retval = q->hardsect_size;

	return retval;
}

static inline int bdev_hardsect_size(struct block_device *bdev)
{
	return queue_hardsect_size(bdev_get_queue(bdev));
}

static inline int queue_dma_alignment(struct request_queue *q)
{
	int retval = 511;

	if (q && q->dma_alignment)
		retval = q->dma_alignment;

	return retval;
}

/* assumes size > 256 */
static inline unsigned int blksize_bits(unsigned int size)
{
	unsigned int bits = 8;
	do 
	{
		bits++;
		size >>= 1;
	} while (size > 256);
	return bits;
}

static inline unsigned int block_size(struct block_device *bdev)
{
	return bdev->bd_block_size;
}

typedef struct {struct page *v;} Sector;

unsigned char *read_dev_sector(struct block_device *, sector_t, Sector *);

static inline void put_dev_sector(Sector p)
{
	page_cache_release(p.v);
}

struct work_struct;
int kblockd_schedule_work(struct work_struct *work);
void kblockd_flush_work(struct work_struct *work);

#define MODULE_ALIAS_BLOCKDEV(major,minor) \
	MODULE_ALIAS("block-major-" __stringify(major) "-" __stringify(minor))
#define MODULE_ALIAS_BLOCKDEV_MAJOR(major) \
	MODULE_ALIAS("block-major-" __stringify(major) "-*")


#else /* CONFIG_BLOCK */
/*
 * stubs for when the block layer is configured out
 */
#define buffer_heads_over_limit 0

static inline long nr_blockdev_pages(void)
{
	return 0;
}

static inline void exit_io_context(void)
{
}

#endif /* CONFIG_BLOCK */

#endif
