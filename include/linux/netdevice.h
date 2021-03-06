/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Interfaces handler.
 *
 * Version:	@(#)dev.h	1.0.10	08/12/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Donald J. Becker, <becker@cesdis.gsfc.nasa.gov>
 *		Alan Cox, <Alan.Cox@linux.org>
 *		Bjorn Ekwall. <bj0rn@blox.se>
 *              Pekka Riikonen <priikone@poseidon.pspt.fi>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		Moved to /usr/include/linux for NET3
 */
#ifndef _LINUX_NETDEVICE_H
#define _LINUX_NETDEVICE_H

#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#ifdef __KERNEL__
#include <linux/timer.h>
#include <linux/delay.h>
#include <asm/atomic.h>
#include <asm/cache.h>
#include <asm/byteorder.h>

#include <linux/device.h>
#include <linux/percpu.h>
#include <linux/dmaengine.h>
#include <linux/workqueue.h>

#include <net/net_namespace.h>

struct vlan_group;
struct ethtool_ops;
struct netpoll_info;
/* 802.11 specific */
struct wireless_dev;
					/* source back-compat hooks */
#define SET_ETHTOOL_OPS(netdev,ops) \
	( (netdev)->ethtool_ops = (ops) )

#define HAVE_ALLOC_NETDEV		/* feature macro: alloc_xxxdev
					   functions are available. */
#define HAVE_FREE_NETDEV		/* free_netdev() */
#define HAVE_NETDEV_PRIV		/* netdev_priv() */

#define NET_XMIT_SUCCESS	0
#define NET_XMIT_DROP		1	/* skb dropped			*/
#define NET_XMIT_CN		2	/* congestion notification	*/
#define NET_XMIT_POLICED	3	/* skb is shot by police	*/
#define NET_XMIT_BYPASS		4	/* packet does not leave via dequeue;
					   (TC use only - dev_queue_xmit
					   returns this as NET_XMIT_SUCCESS) */

/* Backlog congestion levels */
#define NET_RX_SUCCESS		0   /* keep 'em coming, baby */
#define NET_RX_DROP		1  /* packet dropped */
#define NET_RX_CN_LOW		2   /* storm alert, just in case */
#define NET_RX_CN_MOD		3   /* Storm on its way! */
#define NET_RX_CN_HIGH		4   /* The storm is here */
#define NET_RX_BAD		5  /* packet dropped due to kernel error */

/* NET_XMIT_CN is special. It does not guarantee that this packet is lost. It
 * indicates that the device will soon be dropping packets, or already drops
 * some packets of the same priority; prompting us to send less aggressively. */
#define net_xmit_eval(e)	((e) == NET_XMIT_CN? 0 : (e))
#define net_xmit_errno(e)	((e) != NET_XMIT_CN ? -ENOBUFS : 0)

#endif

#define MAX_ADDR_LEN	32		/* Largest hardware address length */

/* Driver transmit return codes */
#define NETDEV_TX_OK 0		/* driver took care of packet */
#define NETDEV_TX_BUSY 1	/* driver tx path was busy*/
#define NETDEV_TX_LOCKED -1	/* driver tx lock was already taken */

/*
 *	Compute the worst case header length according to the protocols
 *	used.
 */
 
#if !defined(CONFIG_AX25) && !defined(CONFIG_AX25_MODULE) && !defined(CONFIG_TR)
#define LL_MAX_HEADER	32
#else
#if defined(CONFIG_AX25) || defined(CONFIG_AX25_MODULE)
#define LL_MAX_HEADER	96
#else
#define LL_MAX_HEADER	48
#endif
#endif

#if !defined(CONFIG_NET_IPIP) && !defined(CONFIG_NET_IPIP_MODULE) && \
    !defined(CONFIG_NET_IPGRE) &&  !defined(CONFIG_NET_IPGRE_MODULE) && \
    !defined(CONFIG_IPV6_SIT) && !defined(CONFIG_IPV6_SIT_MODULE) && \
    !defined(CONFIG_IPV6_TUNNEL) && !defined(CONFIG_IPV6_TUNNEL_MODULE)
#define MAX_HEADER LL_MAX_HEADER
#else
#define MAX_HEADER (LL_MAX_HEADER + 48)
#endif

struct net_device_subqueue
{
	/* Give a control state for each queue.  This struct may contain
	 * per-queue locks in the future.
	 */
	unsigned long   state;
};

/*
 *	Network device statistics. Akin to the 2.0 ether stats but
 *	with byte counters.
 */
 
struct net_device_stats
{
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received 	*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
	unsigned long	multicast;		/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;		/* receiver ring buff overflow	*/
	unsigned long	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;		/* recv'r fifo overrun		*/
	unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_window_errors;
	
	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};


/* Media selection options. */
enum {
        IF_PORT_UNKNOWN = 0,
        IF_PORT_10BASE2,
        IF_PORT_10BASET,
        IF_PORT_AUI,
        IF_PORT_100BASET,
        IF_PORT_100BASETX,
        IF_PORT_100BASEFX
};

#ifdef __KERNEL__

#include <linux/cache.h>
#include <linux/skbuff.h>

struct neighbour;
struct neigh_parms;
struct sk_buff;

struct netif_rx_stats
{
	unsigned total;
	unsigned dropped;
	unsigned time_squeeze;
	unsigned cpu_collision;
};

DECLARE_PER_CPU(struct netif_rx_stats, netdev_rx_stat);

struct dev_addr_list
{
	struct dev_addr_list	*next;
	u8			da_addr[MAX_ADDR_LEN];
	u8			da_addrlen;
	u8			da_synced;
	int			da_users;
	int			da_gusers;
};

/*
 *	We tag multicasts with these structures.
 */

#define dev_mc_list	dev_addr_list
#define dmi_addr	da_addr
#define dmi_addrlen	da_addrlen
#define dmi_users	da_users
#define dmi_gusers	da_gusers

struct hh_cache
{
	struct hh_cache *hh_next;	/* Next entry			     */
	atomic_t	hh_refcnt;	/* number of users                   */
/*
 * We want hh_output, hh_len, hh_lock and hh_data be a in a separate
 * cache line on SMP.
 * They are mostly read, but hh_refcnt may be changed quite frequently,
 * incurring cache line ping pongs.
 */
	__be16		hh_type ____cacheline_aligned_in_smp;
					/* protocol identifier, f.e ETH_P_IP
                                         *  NOTE:  For VLANs, this will be the
                                         *  encapuslated type. --BLG
                                         */
	u16		hh_len;		/* length of header */
	int		(*hh_output)(struct sk_buff *skb);
	seqlock_t	hh_lock;

	/* cached hardware header; allow for machine alignment needs.        */
#define HH_DATA_MOD	16
#define HH_DATA_OFF(__len) \
	(HH_DATA_MOD - (((__len - 1) & (HH_DATA_MOD - 1)) + 1))
#define HH_DATA_ALIGN(__len) \
	(((__len)+(HH_DATA_MOD-1))&~(HH_DATA_MOD - 1))
	unsigned long	hh_data[HH_DATA_ALIGN(LL_MAX_HEADER) / sizeof(long)];
};

/* Reserve HH_DATA_MOD byte aligned hard_header_len, but at least that much.
 * Alternative is:
 *   dev->hard_header_len ? (dev->hard_header_len +
 *                           (HH_DATA_MOD - 1)) & ~(HH_DATA_MOD - 1) : 0
 *
 * We could use other alignment values, but we must maintain the
 * relationship HH alignment <= LL alignment.
 */
#define LL_RESERVED_SPACE(dev) \
	(((dev)->hard_header_len&~(HH_DATA_MOD - 1)) + HH_DATA_MOD)
#define LL_RESERVED_SPACE_EXTRA(dev,extra) \
	((((dev)->hard_header_len+extra)&~(HH_DATA_MOD - 1)) + HH_DATA_MOD)

struct header_ops {
	int	(*create) (struct sk_buff *skb, struct net_device *dev,
			   unsigned short type, const void *daddr,
			   const void *saddr, unsigned len);
	int	(*parse)(const struct sk_buff *skb, unsigned char *haddr);
	int	(*rebuild)(struct sk_buff *skb);
#define HAVE_HEADER_CACHE
	int	(*cache)(const struct neighbour *neigh, struct hh_cache *hh);
	void	(*cache_update)(struct hh_cache *hh,
				const struct net_device *dev,
				const unsigned char *haddr);
};

/* These flag bits are private to the generic network queueing
 * layer, they may not be explicitly referenced by any other
 * code.
 */

enum netdev_state_t
{
	//由负责管理设备的入口和出口流量的代码所使用
	__LINK_STATE_XOFF=0,
	//设备开启。此标识可以由netif_running检查
	__LINK_STATE_START,
	//设备存在。此标识看起来可能是多余的，但是，要想到可热插拔设备可以暂时删除。
	//当系统进入挂起模式然后再重新继续时，此标识也会被清除然后再取回其值
	//此标识可以由netif_device_present检查
	//此标识的初始化不会触发任何动作，相反，其值会经过缜密的检查，以过滤出非法请求或者取得设备状态

	//当可热插拔设备拔除时，或者当支持电源管理的系统进入挂起模式时，该标识就会清楚
	__LINK_STATE_PRESENT,
	//由负责管理设备的入口和出口流量的代码所使用
	__LINK_STATE_SCHED,
	//没有载波(carrier)。此标识可以由netif_carrier_ok检查
	__LINK_STATE_NOCARRIER,
	//设备的链接状态已变更
	__LINK_STATE_LINKWATCH_PENDING,
	
	__LINK_STATE_DORMANT,
	__LINK_STATE_QDISC_RUNNING,
};


/*
 * This structure holds at boot time configured netdevice settings. They
 * are then used in the device probing. 
 */
struct netdev_boot_setup {
	char name[IFNAMSIZ];//设备名称
	struct ifmap map;
};
#define NETDEV_BOOT_SETUP_MAX 8

extern int __init netdev_boot_setup(char *str);

/*
 * Structure for NAPI scheduling similar to tasklet but with weighting
 */
struct napi_struct 
{
	/* The poll_list must only be managed by the entity which
	 * changes the state of the NAPI_STATE_SCHED bit.  This means
	 * whoever atomically sets that bit can add this napi_struct
	 * to the per-cpu poll_list, and whoever clears that bit
	 * can remove from the list right before clearing the bit.
	 */
	struct list_head	poll_list;

	unsigned long		state;
	int			weight;
	int			(*poll)(struct napi_struct *, int);
#ifdef CONFIG_NETPOLL
	spinlock_t		poll_lock;
	int			poll_owner;
	struct net_device	*dev;
	struct list_head	dev_list;
#endif
};

enum
{
	NAPI_STATE_SCHED,	/* Poll is scheduled */
	NAPI_STATE_DISABLE,	/* Disable pending */
};

extern void FASTCALL(__napi_schedule(struct napi_struct *n));

static inline int napi_disable_pending(struct napi_struct *n)
{
	return test_bit(NAPI_STATE_DISABLE, &n->state);
}

/**
 *	napi_schedule_prep - check if napi can be scheduled
 *	@n: napi context
 *
 * Test if NAPI routine is already running, and if not mark
 * it as running.  This is used as a condition variable
 * insure only one NAPI poll instance runs.  We also make
 * sure there is no pending NAPI disable.
 */
static inline int napi_schedule_prep(struct napi_struct *n)
{
	return !napi_disable_pending(n) &&
		!test_and_set_bit(NAPI_STATE_SCHED, &n->state);
}

/**
 *	napi_schedule - schedule NAPI poll
 *	@n: napi context
 *
 * Schedule NAPI poll routine to be called if it is not already
 * running.
 */
static inline void napi_schedule(struct napi_struct *n)
{
	if (napi_schedule_prep(n))
		__napi_schedule(n);
}

/* Try to reschedule poll. Called by dev->poll() after napi_complete().  */
static inline int napi_reschedule(struct napi_struct *napi)
{
	if (napi_schedule_prep(napi)) {
		__napi_schedule(napi);
		return 1;
	}
	return 0;
}

/**
 *	napi_complete - NAPI processing complete
 *	@n: napi context
 *
 * Mark NAPI processing as complete.
 */
static inline void __napi_complete(struct napi_struct *n)
{
	BUG_ON(!test_bit(NAPI_STATE_SCHED, &n->state));
	list_del(&n->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(NAPI_STATE_SCHED, &n->state);
}

static inline void napi_complete(struct napi_struct *n)
{
	local_irq_disable();
	__napi_complete(n);
	local_irq_enable();
}

/**
 *	napi_disable - prevent NAPI from scheduling
 *	@n: napi context
 *
 * Stop NAPI from being scheduled on this context.
 * Waits till any outstanding processing completes.
 */
static inline void napi_disable(struct napi_struct *n)
{
	set_bit(NAPI_STATE_DISABLE, &n->state);
	while (test_and_set_bit(NAPI_STATE_SCHED, &n->state))
		msleep(1);
	clear_bit(NAPI_STATE_DISABLE, &n->state);
}

/**
 *	napi_enable - enable NAPI scheduling
 *	@n: napi context
 *
 * Resume NAPI from being scheduled on this context.
 * Must be paired with napi_disable.
 */
static inline void napi_enable(struct napi_struct *n)
{
	BUG_ON(!test_bit(NAPI_STATE_SCHED, &n->state));
	smp_mb__before_clear_bit();
	clear_bit(NAPI_STATE_SCHED, &n->state);
}

#ifdef CONFIG_SMP
/**
 *	napi_synchronize - wait until NAPI is not running
 *	@n: napi context
 *
 * Wait until NAPI is done being scheduled on this context.
 * Waits till any outstanding processing completes but
 * does not disable future activations.
 */
static inline void napi_synchronize(const struct napi_struct *n)
{
	while (test_bit(NAPI_STATE_SCHED, &n->state))
		msleep(1);
}
#else
# define napi_synchronize(n)	barrier()
#endif

/*
 *	The DEVICE structure.
 *	Actually, this whole structure is a big mistake.  It mixes I/O
 *	data with strictly "high-level" data, and it has to know about
 *	almost every data structure used in the INET module.
 *
 *	FIXME: cleanup struct net_device such that network protocol info
 *	moves out.
 */
//在linux内核中每种网络设备都用这个数据结构表示，包括软硬件的配置信息。
//存储着特定网络设备的所有信息。每个设备都有一个这种结构，无论是真实设备(如Ethernet NIC)或虚拟设备(如Bonding或VLAN)
//所有设备的net_device结构放在一个由全局变量dev_base所指的全局列表中
struct net_device
{

	/*
	 * This is the first field of the "visible" part of this structure
	 * (i.e. as seen by users in the "Space.c" file).  It is the name
	 * the interface.
	 */
	char			name[IFNAMSIZ];//设备的名称 (例如， eth0)
	/* device name hash chain */
	struct hlist_node	name_hlist;

	/*
	 *	I/O specific fields
	 *	FIXME: Merge these and struct ifmap into one
	 */
	//mem_end、mem_start字段描述设备所用的共享内存，用于设备与内核沟通。
	//其初始化和访问只会在设备驱动程序内进行，较高分层不需要关心这些字段
	unsigned long		mem_end;	/* shared mem end	*/
	unsigned long		mem_start;	/* shared mem start	*/
	//设备自有内存映射到I/O内存的起始地址
	unsigned long		base_addr;	/* device I/O address	*/
	//设备用于与内核对话的中断编号。此值可由多个设备共享。
	//驱动程序使用request_irq函数分配此变量，并且使用free_irq予以释放
	unsigned int		irq;		/* device IRQ number	*/

	/*
	 *	Some hardware also needs these fields, but they are not
	 *	part of the usual set specified in Space.c.
	 */
	//此接口所使用的端口类型
	//当设备驱动程序没有通过配置命令强制选择特定的端口类型时，就会简单地选择默认的端口类型
	//有些情况下，一个设备驱动程序可以处理不同的接口模型，在这些情况下，该接口可以按特定次序尝试所有端口类型二找出其端口类型
	unsigned char		if_port;	/* Selectable AUI, TP,..*/
	//设备所使用的DMA通道(如果有的话)。
	//为了从内核获取和释放DMA通道，文件kernel/dma.c定义了函数request_dma和free_dma。
	//取得DMA通道之后，要予以开启或关闭时，可以使用各种include/asm-architecture文件中所提供的enable_dma和disable_dma。
	//这些函数由ISA设备使用；PCI设备采用了其他的方法因而不需要这些函数。
	//并非所有的设备都可用DMA，因为有些总线不使用DMA。
	unsigned char		dma;		/* DMA channel		*/
	//和其队列规则有关的设备状态
	//由网络队列子系统所使用的一组标识。
	//其索引是enum netdev_state_t(include/linux/netdevice.h)中的常数，而为每位定义的常数类似__LINK_STATE_XOFF。
	//个别位的设置和清除都使用通用函数set_bit和clear_bit，而这两个函数的调用通常是通过一个包裹函数把用到的位的细节隐藏起来。
	unsigned long		state;
	//把每个net_device数据结构链接为全局列表(net->dev_base_head)中的下一个(next)元素。
	struct list_head	dev_list;
#ifdef CONFIG_NETPOLL
	struct list_head	napi_list;
#endif
	
	/* The device initialization function. Called only once. */
	int			(*init)(struct net_device *dev);//用于初始化一个设备(对驱动程序私有数据进行初始化)

	/* ------- Fields preinitialized in Space.c finish here ------- */

	/* Net device features */
	//一组标识位，用于存储一些设备功能。
	//可报告适配卡的功能，以便与CPU通信，可能的特征功能的列表定义在net_device数据结构本身之内
	//此参数由设备驱动程序初始化
	unsigned long		features;
#define NETIF_F_SG		1	/* Scatter/gather IO. */
#define NETIF_F_IP_CSUM		2	/* Can checksum TCP/UDP over IPv4. */
#define NETIF_F_NO_CSUM		4	/* Does not require checksum. F.e. loopack. */
#define NETIF_F_HW_CSUM		8	/* Can checksum all the packets. */
#define NETIF_F_IPV6_CSUM	16	/* Can checksum TCP/UDP over IPV6 */
#define NETIF_F_HIGHDMA		32	/* Can DMA to high memory. */
#define NETIF_F_FRAGLIST	64	/* Scatter/gather IO. */
#define NETIF_F_HW_VLAN_TX	128	/* Transmit VLAN hw acceleration */
#define NETIF_F_HW_VLAN_RX	256	/* Receive VLAN hw acceleration */
#define NETIF_F_HW_VLAN_FILTER	512	/* Receive filtering on VLAN */
#define NETIF_F_VLAN_CHALLENGED	1024	/* Device cannot handle VLAN packets */
#define NETIF_F_GSO		2048	/* Enable software GSO. */
#define NETIF_F_LLTX		4096	/* LockLess TX - deprecated. Please */
					/* do not use LLTX in new drivers */
#define NETIF_F_NETNS_LOCAL	8192	/* Does not change network namespaces */
#define NETIF_F_MULTI_QUEUE	16384	/* Has multiple TX/RX queues */
#define NETIF_F_LRO		32768	/* large receive offload */

	/* Segmentation offload features */
#define NETIF_F_GSO_SHIFT	16
#define NETIF_F_GSO_MASK	0xffff0000
#define NETIF_F_TSO		(SKB_GSO_TCPV4 << NETIF_F_GSO_SHIFT)
#define NETIF_F_UFO		(SKB_GSO_UDP << NETIF_F_GSO_SHIFT)
#define NETIF_F_GSO_ROBUST	(SKB_GSO_DODGY << NETIF_F_GSO_SHIFT)
#define NETIF_F_TSO_ECN		(SKB_GSO_TCP_ECN << NETIF_F_GSO_SHIFT)
#define NETIF_F_TSO6		(SKB_GSO_TCPV6 << NETIF_F_GSO_SHIFT)

	/* List of features with software fallbacks. */
#define NETIF_F_GSO_SOFTWARE	(NETIF_F_TSO | NETIF_F_TSO_ECN | NETIF_F_TSO6)


#define NETIF_F_GEN_CSUM	(NETIF_F_NO_CSUM | NETIF_F_HW_CSUM)
#define NETIF_F_V4_CSUM		(NETIF_F_GEN_CSUM | NETIF_F_IP_CSUM)
#define NETIF_F_V6_CSUM		(NETIF_F_GEN_CSUM | NETIF_F_IPV6_CSUM)
#define NETIF_F_ALL_CSUM	(NETIF_F_V4_CSUM | NETIF_F_V6_CSUM)
	//由软中断之一使用
	struct net_device	*next_sched;

	/* Interface index. Unique device identifier	*/
	int			ifindex;//独一无二的ID，当设备以dev_new_index注册时分派给每个设备
	int			iflink;//主要是由(虚拟)隧道设备使用，可用于标识抵达隧道另一端的真实设备

	//针对一般设备的，设备驱动程序所收集的一些统计数据可以使用用户空间应用程序予以显示，而其他统计数据则严格由内核使用
	struct net_device_stats* (*get_stats)(struct net_device *dev);
	struct net_device_stats	stats;

#ifdef CONFIG_WIRELESS_EXT
	/* List of functions to handle Wireless Extensions (instead of ioctl).
	 * See <net/iw_handler.h> for details. Jean II */
	const struct iw_handler_def *	wireless_handlers;
	/* Instance data managed by the core of Wireless Extensions. */
	struct iw_public_data *	wireless_data;
#endif
	//指向一组函数指针的指针，用于设置或取出不同设备参数的配置
	const struct ethtool_ops *ethtool_ops;

	/* Hardware header description */
	const struct header_ops *header_ops;//由邻居层使用

	/*
	 * This marks the end of the "visible" part of the structure. All
	 * fields hereafter are internal to the system, and may change at
	 * will (read: may be cleaned up at will).
	 */
	//用于存储各种标识的位域
	//字段中的某些位代表网络设备的功能(如IFF_MULTICAST)，而其他位代表状态的改变(如IFF_UP或IFF_RUNNING).
	//include/linux/if.h中有这些标识的完整列表。
	//此设备的驱动程序通常会在初始化期间设置这些功能，而这些状态标识由内核管理，以响应外部事件。
	unsigned int		flags;	/* interface flags (a la BSD)	*/
	//几乎已不再使用了，只是由于兼容性的原因而存在
	unsigned short		gflags;
	//用于存储用户空间不可见的标识。
	//目前而言，此字段由VLAN和Bridge虚拟设备使用。
        unsigned short          priv_flags; /* Like 'flags' but invisible to userspace. */
	unsigned short		padded;	/* How much padding added by alloc_netdev() */

	unsigned char		operstate; /* RFC2863 operstate */
	unsigned char		link_mode; /* mapping policy to operstate */
	//最大传输单元，表示设备能处理的帧的最大尺寸
	unsigned		mtu;	/* interface MTU value		*/
	//设备所属的类型(Ethernet、Frame Relay 等)。
	//include/linux/if_arp.h包含可能类型的完整列表
	unsigned short		type;	/* interface hardware type	*/
	//以字节为单位的设备头的大小。
	//例如，Ethernet报头是14字节。每个设备头的长度定义在该设备的头文件中。
	//例如，对于Ethernet而言，ETH_HLEN就定义在include/linux/if_ether.h中
	unsigned short		hard_header_len;	/* hardware hdr length	*/
	//有些协议允许一组设备群集起来作为单一设备。这些协议包括EQL、Bonding、以及流量控制的TEQL队列规则。
	//群组中的一个设备会被选定为所谓的主设备(master)。这个字段是一个指针，指向群组中主设备的net_device结构。
	//若此接口不是这类群组中的成员之一，则此指针是NULL。
	struct net_device	*master; /* Pointer to master device of a group,
					  * which this device is member of.
					  */

	/* Interface address info. */
	unsigned char		perm_addr[MAX_ADDR_LEN]; /* permanent hw address */
	//设备链路层地址长度，其值依赖于设备的类型。Ethernet地址都是6个字节长。
	unsigned char		addr_len;	/* hardware address length	*/
	//目前在zSeries OSA NIC上由IPv6所用。
	//此字段用于区别可由不同OS同时共享的同一种设备的诸多虚拟实例
	unsigned short          dev_id;		/* for shared network cards */

	struct dev_addr_list	*uc_list;	/* Secondary unicast mac addresses */
	int			uc_count;	/* Number of installed ucasts	*/
	int			uc_promisc;
	//指向此设备的dev_mc_list结构列表表头的指针
	struct dev_addr_list	*mc_list;	/* Multicast mac addresses	*/
	//此设备的多播地址数目，也就是mc_list所指列表的长度
	int			mc_count;	/* Number of installed mcasts	*/
	//计数器，表示一个设备处于混杂模式中。
	//采用计数器而非简单标识的原因在于，多个客户程序可能都会要求混杂模式。
	//因此，进入混杂模式时递增该计数器，而退出该模式时递减该计数器。
	//除非计数器为零，否则该设备不会退出混杂模式
	//当promiscuity为非零时(如通过调用dev_set_promiscuity)，flags的IFF_PROMISC标识位也会设置，并且由配置此接口的函数进行检查
	int			promiscuity;
	//为非0时，引起此设备监听所有的多播地址。allmulti如promiscuity一样，是一个引用计数而不是简单的布尔值。
	//这是因为多台设备(例如，VLAN和绑定设备)可能各自都需要监听所有地址
	//当此变量由0变为非0时，就会调用dev_set_allmulti函数，以指示该端口监听所有的多播地址。当allmulti变为0时，就会发生相反的事情。
	int			allmulti;


	/* Protocol specific pointers */
	//指向特定协议专用的数据结构
	void 			*atalk_ptr;	/* AppleTalk link 	*/
	void			*ip_ptr;	/* IPv4 specific data	*/  
	void                    *dn_ptr;        /* DECnet specific data */
	void                    *ip6_ptr;       /* IPv6 specific data */
	void			*ec_ptr;	/* Econet specific data	*/
	void			*ax25_ptr;	/* AX.25 specific data */
	struct wireless_dev	*ieee80211_ptr;	/* IEEE 802.11 specific data,
						   assign before registering */

/*
 * Cache line mostly used on receive path (including eth_type_trans())
 */
 	//最后一个封包到达的时间(以jiffies测量)。
 	//此刻，不用于任何特殊的目的，但有需要时可以利用
	unsigned long		last_rx;	/* Time of last Rx	*/
	/* Interface address info used in eth_type_trans() */
	//设备链路层地址
	//不要将其与L3或IP地址混淆。此地址的字节长度由addr_len指定。
	unsigned char		dev_addr[MAX_ADDR_LEN];	/* hw address, (before bcast because most packets are unicast) */
	//链路层广播地址
	unsigned char		broadcast[MAX_ADDR_LEN];	/* hw bcast add	*/

/*
 * Cache line mostly used on queue transmit path (qdisc)
 */
	/* device queue lock */
	//流量控制子系统为每个网络设备定义了一个私有的出口队列。queue_lock用于避免对出口队列的并发访问
	spinlock_t		queue_lock ____cacheline_aligned_in_smp;
	//
	//用于管理入口和出口的封包队列 
	struct Qdisc		*qdisc;
	struct Qdisc		*qdisc_sleeping;
	struct list_head	qdisc_list;
	//设备的传送队列的最大长度。
	//其值可以通过sysfs文件系统予以调整(参见sys/class/net/device_name/目录)
	//当内核支持流量控制时，可能不用tx_queue_len(仅有很少的队列规则使用)
	//注意，所有队列长度为0的设备都是虚拟设备，虚拟设备依赖相关联的真实设备区去做所有的队列化工作，
	//(回环设备除外，因为回环设备由内核内部使用，会立刻传递所有流量，因此不需要)
	unsigned long		tx_queue_len;	/* Max frames per queue allowed */

	/* Partially transmitted GSO packet. */
	struct sk_buff		*gso_skb;

	/* ingress path synchronizer */
	//流量控制子系统为每个网络设备定义了一个私有的入口队列。queue_lock用于避免对出口队列的并发访问
	spinlock_t		ingress_lock;
	struct Qdisc		*qdisc_ingress;

/*
 * One part is mostly used on xmit path (device)
 */
	/* hard_start_xmit synchronizer */
	//xmit_lock锁使驱动程序函数hard_start_smit的访问串行化。
	//这意味着，每个CPU一次只能对任何给定的一个设备做一次传输。
	spinlock_t		_xmit_lock ____cacheline_aligned_in_smp;
	/* cpu id of processor entered to hard_start_xmit or -1,
	   if nobody entered there.
	 */
	//xmit_lock_owner是持有该锁(_xmit_lock)的CPU的ID，
	//对单系统而言这个值总为0，而在SMP系统中该锁没有被取走时，其值为-1。
	//当设备驱动程序支持时，也可以做无锁的传输。
	int			xmit_lock_owner;
	void			*priv;	/* pointer to private data	*/
	
	int			(*hard_start_xmit) (struct sk_buff *skb, struct net_device *dev);//用于传输一个帧
	/* These may be needed for future network-power-down code. */
	//最近的一个帧传输启动的时间(以jiffies测量)。
	//设备驱动程序会在传输前设置此值。如在一段给定的时间后传输没有完成，这个字段用于检测适配卡的问题。
	//传输时间过长就意味着有地方出错，在这种情况下，驱动程序通常复位适配卡。
	unsigned long		trans_start;	/* Time (in jiffies) of last Tx	*/

	int			watchdog_timeo; /* used by dev_watchdog() */
	struct timer_list	watchdog_timer;

/*
 * refcnt is a very hot point, so align it on SMP
 */
	/* Number of references to this device */
	//引用计数。此计数器变为零之前，设备无法除名(unregister)
	atomic_t		refcnt ____cacheline_aligned_in_smp;

	/* delayed register/unregister */
	//网络设备的注册和除名是以两步骤进行的。todo_list用于处理第二步骤
	struct list_head	todo_list;
	/* device index hash chain */
	struct hlist_node	index_hlist;

	//lweventlist链表的结点
	struct net_device	*link_watch_next;

	/* register/unregister state machine */
	//设备的注册状态
	enum 
	{
		//当net_device数据结构已分配而且其内容都清为零时，此值代表的就是dev->reg_state中的0
		NETREG_UNINITIALIZED=0,
		//设备已完成注册
		NETREG_REGISTERED,	/* completed register_netdevice */
		//net_device结构已从dev_base_head/dev_name_head/dev_index_head结构中删除掉了
		NETREG_UNREGISTERING,	/* called unregister_netdevice */
		//设备已完全除名(包括删除/sys中的项目)，但是net_device结构还没释放掉
		NETREG_UNREGISTERED,	/* completed unregister todo */
		//所有对net_device结构的引用都已释放。
		//从网络代码的观点看，此数据结构已可释放。然而，由sysfs来决定是否这样做
		NETREG_RELEASED,		/* called free_netdev */
	} reg_state;

	/* Called after device is detached from network. */
	void			(*uninit)(struct net_device *dev);//用于清理一个设备(对驱动程序私有数据做清理工作)
	/* Called after last user reference disappears. */
	void			(*destructor)(struct net_device *dev);//用于销毁一个设备

	/* Pointers to interface service routines.	*/
	int			(*open)(struct net_device *dev);//用于开启一个设备
	int			(*stop)(struct net_device *dev);//用于关闭一个设备
#define HAVE_NETDEV_POLL
#define HAVE_CHANGE_RX_FLAGS
	void			(*change_rx_flags)(struct net_device *dev, int flags);
#define HAVE_SET_RX_MODE
	void			(*set_rx_mode)(struct net_device *dev);
#define HAVE_MULTICAST
	//此方法用于要求设备驱动程序配置设备以监听mc_list中的多播地址
	//通常不会直接调用此方法，而是通过包裹函数，如dev_mc_upload或其无锁版本__dev_mc_upload。
	//当一个设备无法安装多播地址列表时，就只会将其全部开启
	void			(*set_multicast_list)(struct net_device *dev);
#define HAVE_SET_MAC_ADDR  	
	//改变设备的MAC地址。当设备没有提供此功能时(如同Bridge虚拟设备的情况)，就会设置为NULL
	int			(*set_mac_address)(struct net_device *dev, void *addr);
#define HAVE_VALIDATE_ADDR
	int			(*validate_addr)(struct net_device *dev);
#define HAVE_PRIVATE_IOCTL
	//ioctl是系统调用，用于向设备发出命令。调用此方法可以处理一些ioctl命令
	int			(*do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
#define HAVE_SET_CONFIG
	//配置驱动程序参数，如硬件参数irq、io_addr以及if_port。
	//较高分层的参数(如协议地址)是由do_ioctl来处理
	int			(*set_config)(struct net_device *dev, struct ifmap *map);
#define HAVE_CHANGE_MTU
	//改变设备的MTU值。
	//改变此字段对设备驱动程序没有影响，只是强制内核软件接收新的MTU，在据此处理分段。
	int			(*change_mtu)(struct net_device *dev, int new_mtu);

#define HAVE_TX_TIMEOUT
	//在看门狗定时器到期之时调用此方法，用于确认该次传送是否花费了一段很可疑的长时间才完成。
	//只有定义了该方法，看门狗定时器才会启动
	void			(*tx_timeout) (struct net_device *dev);
	//由VLAN功能使用，把设备注册为拥有VLAN标记(VLAN tagging)功能
	void			(*vlan_rx_register)(struct net_device *dev,
						    struct vlan_group *grp);
	//由VLAN功能使用，把VLAN添加至设备
	void			(*vlan_rx_add_vid)(struct net_device *dev,
						   unsigned short vid);
	//由VLAN功能使用，从设备中删除VLAN
	void			(*vlan_rx_kill_vid)(struct net_device *dev,
						    unsigned short vid);

	int			(*neigh_setup)(struct net_device *dev, struct neigh_parms *);
#ifdef CONFIG_NETPOLL
	struct netpoll_info	*npinfo;
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	//由选用的Netpoll功能使用
	void                    (*poll_controller)(struct net_device *dev);
#endif

	/* Network namespace this network device is inside */
	struct net		*nd_net;

	/* bridge stuff */
	struct net_bridge_port	*br_port; //当此设备配置成桥接端口时，表示其所关联的网桥端口
	/* macvlan */
	struct macvlan_port	*macvlan_port;

	/* class/net/name entry */
	struct device		dev;
	/* space for optional statistics and wireless sysfs groups */
	struct attribute_group  *sysfs_groups[3];

	/* rtnetlink link ops */
	const struct rtnl_link_ops *rtnl_link_ops;

	/* The TX queue control structures */
	unsigned int			egress_subqueue_count;
	struct net_device_subqueue	egress_subqueue[1];
};
#define to_net_dev(d) container_of(d, struct net_device, dev)

#define	NETDEV_ALIGN		32
#define	NETDEV_ALIGN_CONST	(NETDEV_ALIGN - 1)

/**
 *	netdev_priv - access network device private data
 *	@dev: network device
 *
 * Get network device private data
 */
static inline void *netdev_priv(const struct net_device *dev)
{
	return dev->priv;
}

/* Set the sysfs physical device reference for the network logical device
 * if set prior to registration will cause a symlink during initialization.
 */
#define SET_NETDEV_DEV(net, pdev)	((net)->dev.parent = (pdev))

/**
 *	netif_napi_add - initialize a napi context
 *	@dev:  network device
 *	@napi: napi context
 *	@poll: polling function
 *	@weight: default weight
 *
 * netif_napi_add() must be used to initialize a napi context prior to calling
 * *any* of the other napi related functions.
 */
static inline void netif_napi_add(struct net_device *dev,
				  struct napi_struct *napi,
				  int (*poll)(struct napi_struct *, int),
				  int weight)
{
	INIT_LIST_HEAD(&napi->poll_list);
	napi->poll = poll;
	napi->weight = weight;
#ifdef CONFIG_NETPOLL
	napi->dev = dev;
	list_add(&napi->dev_list, &dev->napi_list);
	spin_lock_init(&napi->poll_lock);
	napi->poll_owner = -1;
#endif
	set_bit(NAPI_STATE_SCHED, &napi->state);
}

struct packet_type 
{
	__be16			type;	/* This is really htons(ether_type). */
	struct net_device	*dev;	/* NULL is wildcarded here	     */
	int			(*func) (struct sk_buff *,
					 struct net_device *,
					 struct packet_type *,
					 struct net_device *);
	struct sk_buff		*(*gso_segment)(struct sk_buff *skb,
						int features);
	int			(*gso_send_check)(struct sk_buff *skb);
	void			*af_packet_priv;
	struct list_head	list;
};

#include <linux/interrupt.h>
#include <linux/notifier.h>

extern rwlock_t				dev_base_lock;		/* Device list lock */


#define for_each_netdev(net, d)		\
		list_for_each_entry(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_safe(net, d, n)	\
		list_for_each_entry_safe(d, n, &(net)->dev_base_head, dev_list)
#define for_each_netdev_continue(net, d)		\
		list_for_each_entry_continue(d, &(net)->dev_base_head, dev_list)
#define net_device_entry(lh)	list_entry(lh, struct net_device, dev_list)

static inline struct net_device *next_net_device(struct net_device *dev)
{
	struct list_head *lh;
	struct net *net;

	net = dev->nd_net;
	lh = dev->dev_list.next;
	return lh == &net->dev_base_head ? NULL : net_device_entry(lh);
}

static inline struct net_device *first_net_device(struct net *net)
{
	return list_empty(&net->dev_base_head) ? NULL :
		net_device_entry(net->dev_base_head.next);
}

extern int 			netdev_boot_setup_check(struct net_device *dev);
extern unsigned long		netdev_boot_base(const char *prefix, int unit);
extern struct net_device    *dev_getbyhwaddr(struct net *net, unsigned short type, char *hwaddr);
extern struct net_device *dev_getfirstbyhwtype(struct net *net, unsigned short type);
extern struct net_device *__dev_getfirstbyhwtype(struct net *net, unsigned short type);
extern void		dev_add_pack(struct packet_type *pt);
extern void		dev_remove_pack(struct packet_type *pt);
extern void		__dev_remove_pack(struct packet_type *pt);

extern struct net_device	*dev_get_by_flags(struct net *net, unsigned short flags,
						  unsigned short mask);
extern struct net_device	*dev_get_by_name(struct net *net, const char *name);
extern struct net_device	*__dev_get_by_name(struct net *net, const char *name);
extern int		dev_alloc_name(struct net_device *dev, const char *name);
extern int		dev_open(struct net_device *dev);
extern int		dev_close(struct net_device *dev);
extern int		dev_queue_xmit(struct sk_buff *skb);
extern int		register_netdevice(struct net_device *dev);
extern void		unregister_netdevice(struct net_device *dev);
extern void		free_netdev(struct net_device *dev);
extern void		synchronize_net(void);
extern int 		register_netdevice_notifier(struct notifier_block *nb);
extern int		unregister_netdevice_notifier(struct notifier_block *nb);
extern int call_netdevice_notifiers(unsigned long val, struct net_device *dev);
extern struct net_device	*dev_get_by_index(struct net *net, int ifindex);
extern struct net_device	*__dev_get_by_index(struct net *net, int ifindex);
extern int		dev_restart(struct net_device *dev);
#ifdef CONFIG_NETPOLL_TRAP
extern int		netpoll_trap(void);
#endif

static inline int dev_hard_header(struct sk_buff *skb, struct net_device *dev,
				  unsigned short type,
				  const void *daddr, const void *saddr,
				  unsigned len)
{
	if (!dev->header_ops || !dev->header_ops->create)
		return 0;

	return dev->header_ops->create(skb, dev, type, daddr, saddr, len);
}

static inline int dev_parse_header(const struct sk_buff *skb,
				   unsigned char *haddr)
{
	const struct net_device *dev = skb->dev;

	if (!dev->header_ops || !dev->header_ops->parse)
		return 0;
	return dev->header_ops->parse(skb, haddr);
}

typedef int gifconf_func_t(struct net_device * dev, char __user * bufptr, int len);
extern int		register_gifconf(unsigned int family, gifconf_func_t * gifconf);
static inline int unregister_gifconf(unsigned int family)
{
	return register_gifconf(family, NULL);
}

/*
 * Incoming packets are placed on per-cpu queues so that
 * no locking is needed.
 */
struct softnet_data
{
	struct net_device	*output_queue;
	struct sk_buff_head	input_pkt_queue;
	struct list_head	poll_list;
	struct sk_buff		*completion_queue;

	struct napi_struct	backlog;
#ifdef CONFIG_NET_DMA
	struct dma_chan		*net_dma;
#endif
};

DECLARE_PER_CPU(struct softnet_data,softnet_data);

#define HAVE_NETIF_QUEUE

extern void __netif_schedule(struct net_device *dev);

static inline void netif_schedule(struct net_device *dev)
{
	if (!test_bit(__LINK_STATE_XOFF, &dev->state))
		__netif_schedule(dev);
}

/**
 *	netif_start_queue - allow transmit
 *	@dev: network device
 *
 *	Allow upper layers to call the device hard_start_xmit routine.
 */
static inline void netif_start_queue(struct net_device *dev)
{
	clear_bit(__LINK_STATE_XOFF, &dev->state);
}

/**
 *	netif_wake_queue - restart transmit
 *	@dev: network device
 *
 *	Allow upper layers to call the device hard_start_xmit routine.
 *	Used for flow control when transmit resources are available.
 */
//开启设备的出口队列
static inline void netif_wake_queue(struct net_device *dev)
{
#ifdef CONFIG_NETPOLL_TRAP
	if (netpoll_trap()) {
		clear_bit(__LINK_STATE_XOFF, &dev->state);
		return;
	}
#endif
	if (test_and_clear_bit(__LINK_STATE_XOFF, &dev->state))
		__netif_schedule(dev);
}

/**
 *	netif_stop_queue - stop transmitted packets
 *	@dev: network device
 *
 *	Stop upper layers calling the device hard_start_xmit routine.
 *	Used for flow control when transmit resources are unavailable.
 */
//关闭设备出口对列
static inline void netif_stop_queue(struct net_device *dev)
{
	set_bit(__LINK_STATE_XOFF, &dev->state);
}

/**
 *	netif_queue_stopped - test if transmit queue is flowblocked
 *	@dev: network device
 *
 *	Test if transmit queue on device is currently unable to send.
 */
static inline int netif_queue_stopped(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_XOFF, &dev->state);
}

/**
 *	netif_running - test if up
 *	@dev: network device
 *
 *	Test if the device has been brought up.
 */
static inline int netif_running(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_START, &dev->state);
}

/*
 * Routines to manage the subqueues on a device.  We only need start
 * stop, and a check if it's stopped.  All other device management is
 * done at the overall netdevice level.
 * Also test the device if we're multiqueue.
 */

/**
 *	netif_start_subqueue - allow sending packets on subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Start individual transmit queue of a device with multiple transmit queues.
 */
static inline void netif_start_subqueue(struct net_device *dev, u16 queue_index)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	clear_bit(__LINK_STATE_XOFF, &dev->egress_subqueue[queue_index].state);
#endif
}

/**
 *	netif_stop_subqueue - stop sending packets on subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Stop individual transmit queue of a device with multiple transmit queues.
 */
static inline void netif_stop_subqueue(struct net_device *dev, u16 queue_index)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
#ifdef CONFIG_NETPOLL_TRAP
	if (netpoll_trap())
		return;
#endif
	set_bit(__LINK_STATE_XOFF, &dev->egress_subqueue[queue_index].state);
#endif
}

/**
 *	netif_subqueue_stopped - test status of subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Check individual transmit queue of a device with multiple transmit queues.
 */
static inline int __netif_subqueue_stopped(const struct net_device *dev,
					 u16 queue_index)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	return test_bit(__LINK_STATE_XOFF,
			&dev->egress_subqueue[queue_index].state);
#else
	return 0;
#endif
}

static inline int netif_subqueue_stopped(const struct net_device *dev,
					 struct sk_buff *skb)
{
	return __netif_subqueue_stopped(dev, skb_get_queue_mapping(skb));
}

/**
 *	netif_wake_subqueue - allow sending packets on subqueue
 *	@dev: network device
 *	@queue_index: sub queue index
 *
 * Resume individual transmit queue of a device with multiple transmit queues.
 */
static inline void netif_wake_subqueue(struct net_device *dev, u16 queue_index)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
#ifdef CONFIG_NETPOLL_TRAP
	if (netpoll_trap())
		return;
#endif
	if (test_and_clear_bit(__LINK_STATE_XOFF,
			       &dev->egress_subqueue[queue_index].state))
		__netif_schedule(dev);
#endif
}

/**
 *	netif_is_multiqueue - test if device has multiple transmit queues
 *	@dev: network device
 *
 * Check if device has multiple transmit queues
 * Always falls if NETDEVICE_MULTIQUEUE is not configured
 */
static inline int netif_is_multiqueue(const struct net_device *dev)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	return (!!(NETIF_F_MULTI_QUEUE & dev->features));
#else
	return 0;
#endif
}

/* Use this variant when it is known for sure that it
 * is executing from interrupt context.
 */
extern void dev_kfree_skb_irq(struct sk_buff *skb);

/* Use this variant in places where it could be invoked
 * either from interrupt or non-interrupt context.
 */
extern void dev_kfree_skb_any(struct sk_buff *skb);

#define HAVE_NETIF_RX 1
extern int		netif_rx(struct sk_buff *skb);
extern int		netif_rx_ni(struct sk_buff *skb);
#define HAVE_NETIF_RECEIVE_SKB 1
extern int		netif_receive_skb(struct sk_buff *skb);
extern int		dev_valid_name(const char *name);
extern int		dev_ioctl(struct net *net, unsigned int cmd, void __user *);
extern int		dev_ethtool(struct net *net, struct ifreq *);
extern unsigned		dev_get_flags(const struct net_device *);
extern int		dev_change_flags(struct net_device *, unsigned);
extern int		dev_change_name(struct net_device *, char *);
extern int		dev_change_net_namespace(struct net_device *,
						 struct net *, const char *);
extern int		dev_set_mtu(struct net_device *, int);
extern int		dev_set_mac_address(struct net_device *,
					    struct sockaddr *);
extern int		dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);

extern int		netdev_budget;

/* Called by rtnetlink.c:rtnl_unlock() */
extern void netdev_run_todo(void);

/**
 *	dev_put - release reference to device
 *	@dev: network device
 *
 * Release reference to device to allow it to be freed.
 */
static inline void dev_put(struct net_device *dev)
{
	atomic_dec(&dev->refcnt);
}

/**
 *	dev_hold - get reference to device
 *	@dev: network device
 *
 * Hold reference to device to keep it from being freed.
 */
static inline void dev_hold(struct net_device *dev)
{
	atomic_inc(&dev->refcnt);
}

/* Carrier loss detection, dial on demand. The functions netif_carrier_on
 * and _off may be called from IRQ context, but it is caller
 * who is responsible for serialization of these calls.
 *
 * The name carrier is inappropriate, these functions should really be
 * called netif_lowerlayer_*() because they represent the state of any
 * kind of lower layer not just hardware media.
 */

extern void linkwatch_fire_event(struct net_device *dev);

/**
 *	netif_carrier_ok - test if carrier present
 *	@dev: network device
 *
 * Check if carrier is present on device
 */
static inline int netif_carrier_ok(const struct net_device *dev)
{
	return !test_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

extern void __netdev_watchdog_up(struct net_device *dev);

extern void netif_carrier_on(struct net_device *dev);

extern void netif_carrier_off(struct net_device *dev);

/**
 *	netif_dormant_on - mark device as dormant.
 *	@dev: network device
 *
 * Mark device as dormant (as per RFC2863).
 *
 * The dormant state indicates that the relevant interface is not
 * actually in a condition to pass packets (i.e., it is not 'up') but is
 * in a "pending" state, waiting for some external event.  For "on-
 * demand" interfaces, this new state identifies the situation where the
 * interface is waiting for events to place it in the up state.
 *
 */
static inline void netif_dormant_on(struct net_device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_DORMANT, &dev->state))
		linkwatch_fire_event(dev);
}

/**
 *	netif_dormant_off - set device as not dormant.
 *	@dev: network device
 *
 * Device is not in dormant state.
 */
static inline void netif_dormant_off(struct net_device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_DORMANT, &dev->state))
		linkwatch_fire_event(dev);
}

/**
 *	netif_dormant - test if carrier present
 *	@dev: network device
 *
 * Check if carrier is present on device
 */
static inline int netif_dormant(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_DORMANT, &dev->state);
}


/**
 *	netif_oper_up - test if device is operational
 *	@dev: network device
 *
 * Check if carrier is operational
 */
static inline int netif_oper_up(const struct net_device *dev) {
	return (dev->operstate == IF_OPER_UP ||
		dev->operstate == IF_OPER_UNKNOWN /* backward compat */);
}

/**
 *	netif_device_present - is device available or removed
 *	@dev: network device
 *
 * Check if device has not been removed from system.
 */
static inline int netif_device_present(struct net_device *dev)
{
	return test_bit(__LINK_STATE_PRESENT, &dev->state);
}

extern void netif_device_detach(struct net_device *dev);

extern void netif_device_attach(struct net_device *dev);

/*
 * Network interface message level settings
 */
#define HAVE_NETIF_MSG 1

enum {
	NETIF_MSG_DRV		= 0x0001,
	NETIF_MSG_PROBE		= 0x0002,
	NETIF_MSG_LINK		= 0x0004,
	NETIF_MSG_TIMER		= 0x0008,
	NETIF_MSG_IFDOWN	= 0x0010,
	NETIF_MSG_IFUP		= 0x0020,
	NETIF_MSG_RX_ERR	= 0x0040,
	NETIF_MSG_TX_ERR	= 0x0080,
	NETIF_MSG_TX_QUEUED	= 0x0100,
	NETIF_MSG_INTR		= 0x0200,
	NETIF_MSG_TX_DONE	= 0x0400,
	NETIF_MSG_RX_STATUS	= 0x0800,
	NETIF_MSG_PKTDATA	= 0x1000,
	NETIF_MSG_HW		= 0x2000,
	NETIF_MSG_WOL		= 0x4000,
};

#define netif_msg_drv(p)	((p)->msg_enable & NETIF_MSG_DRV)
#define netif_msg_probe(p)	((p)->msg_enable & NETIF_MSG_PROBE)
#define netif_msg_link(p)	((p)->msg_enable & NETIF_MSG_LINK)
#define netif_msg_timer(p)	((p)->msg_enable & NETIF_MSG_TIMER)
#define netif_msg_ifdown(p)	((p)->msg_enable & NETIF_MSG_IFDOWN)
#define netif_msg_ifup(p)	((p)->msg_enable & NETIF_MSG_IFUP)
#define netif_msg_rx_err(p)	((p)->msg_enable & NETIF_MSG_RX_ERR)
#define netif_msg_tx_err(p)	((p)->msg_enable & NETIF_MSG_TX_ERR)
#define netif_msg_tx_queued(p)	((p)->msg_enable & NETIF_MSG_TX_QUEUED)
#define netif_msg_intr(p)	((p)->msg_enable & NETIF_MSG_INTR)
#define netif_msg_tx_done(p)	((p)->msg_enable & NETIF_MSG_TX_DONE)
#define netif_msg_rx_status(p)	((p)->msg_enable & NETIF_MSG_RX_STATUS)
#define netif_msg_pktdata(p)	((p)->msg_enable & NETIF_MSG_PKTDATA)
#define netif_msg_hw(p)		((p)->msg_enable & NETIF_MSG_HW)
#define netif_msg_wol(p)	((p)->msg_enable & NETIF_MSG_WOL)

static inline u32 netif_msg_init(int debug_value, int default_msg_enable_bits)
{
	/* use default */
	if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
		return default_msg_enable_bits;
	if (debug_value == 0)	/* no output */
		return 0;
	/* set low N bits */
	return (1 << debug_value) - 1;
}

/* Test if receive needs to be scheduled but only if up */
//确定设备处于运行，而且设备还没有被添加到网络层的 POLL 处理队列中，
//在调用 netif_rx_schedule之前会调用这个函数。
static inline int netif_rx_schedule_prep(struct net_device *dev, struct napi_struct *napi)
{
	return napi_schedule_prep(napi);
}

/* Add interface to tail of rx poll list. This assumes that _prep has
 * already been called and returned 1.
 */
static inline void __netif_rx_schedule(struct net_device *dev, struct napi_struct *napi)
{
	__napi_schedule(napi);
}

/* Try to reschedule poll. Called by irq handler. */
//这个函数被中断服务程序调用，将设备的 POLL 方法添加到网络层次的 POLL 处理队列中去，
//排队并且准备接收数据包，在使用之前需要调用 netif_rx_reschedule_prep，并且返回的数为 1，
//并且触发一个 NET_RX_SOFTIRQ 的软中断通知网络层接收数据包。
static inline void netif_rx_schedule(struct net_device *dev, struct napi_struct *napi)
{
	if (netif_rx_schedule_prep(dev, napi))
		__netif_rx_schedule(dev, napi);
}

/* Try to reschedule poll. Called by dev->poll() after netif_rx_complete().  */
static inline int netif_rx_reschedule(struct net_device *dev,
				      struct napi_struct *napi)
{
	if (napi_schedule_prep(napi)) {
		__netif_rx_schedule(dev, napi);
		return 1;
	}
	return 0;
}

/* same as netif_rx_complete, except that local_irq_save(flags)
 * has already been issued
 */
static inline void __netif_rx_complete(struct net_device *dev,
				       struct napi_struct *napi)
{
	__napi_complete(napi);
}

/* Remove interface from poll list: it must be in the poll list
 * on current cpu. This primitive is called by dev->poll(), when
 * it completes the work. The device cannot be out of poll list at this
 * moment, it is BUG().
 */
//把当前指定的设备从 POLL 队列中清除，通常被设备的 POLL 方法调用，
//注意如果在 POLL 队列处于工作状态的时候是不能把指定设备清除的，否则将会出错。
static inline void netif_rx_complete(struct net_device *dev, struct napi_struct *napi)
{
	unsigned long flags;

	local_irq_save(flags);
	__netif_rx_complete(dev, napi);
	local_irq_restore(flags);
}

/**
 *	netif_tx_lock - grab network device transmit lock
 *	@dev: network device
 *	@cpu: cpu number of lock owner
 *
 * Get network device transmit lock
 */
static inline void __netif_tx_lock(struct net_device *dev, int cpu)
{
	spin_lock(&dev->_xmit_lock);
	dev->xmit_lock_owner = cpu;
}

static inline void netif_tx_lock(struct net_device *dev)
{
	__netif_tx_lock(dev, smp_processor_id());
}

static inline void netif_tx_lock_bh(struct net_device *dev)
{
	spin_lock_bh(&dev->_xmit_lock);
	dev->xmit_lock_owner = smp_processor_id();
}

static inline int netif_tx_trylock(struct net_device *dev)
{
	int ok = spin_trylock(&dev->_xmit_lock);
	if (likely(ok))
		dev->xmit_lock_owner = smp_processor_id();
	return ok;
}

static inline void netif_tx_unlock(struct net_device *dev)
{
	dev->xmit_lock_owner = -1;
	spin_unlock(&dev->_xmit_lock);
}

static inline void netif_tx_unlock_bh(struct net_device *dev)
{
	dev->xmit_lock_owner = -1;
	spin_unlock_bh(&dev->_xmit_lock);
}

#define HARD_TX_LOCK(dev, cpu) {			\
	if ((dev->features & NETIF_F_LLTX) == 0) {	\
		__netif_tx_lock(dev, cpu);			\
	}						\
}

#define HARD_TX_UNLOCK(dev) {				\
	if ((dev->features & NETIF_F_LLTX) == 0) {	\
		netif_tx_unlock(dev);			\
	}						\
}

static inline void netif_tx_disable(struct net_device *dev)
{
	netif_tx_lock_bh(dev);
	netif_stop_queue(dev);
	netif_tx_unlock_bh(dev);
}

/* These functions live elsewhere (drivers/net/net_init.c, but related) */

extern void		ether_setup(struct net_device *dev);

/* Support for loadable net-drivers */
extern struct net_device *alloc_netdev_mq(int sizeof_priv, const char *name,
				       void (*setup)(struct net_device *),
				       unsigned int queue_count);
//参数1 -- 私有数据结构大小
//参数2 -- 设备名称。这可能是一部分名称，再由内核通过某种规则，将其完成，以确保独一无二的设备名称。
//参数3 -- 这个函数用于初始化net_device的部分字段
#define alloc_netdev(sizeof_priv, name, setup) \
	alloc_netdev_mq(sizeof_priv, name, setup, 1)
extern int		register_netdev(struct net_device *dev);
extern void		unregister_netdev(struct net_device *dev);
/* Functions used for secondary unicast and multicast support */
extern void		dev_set_rx_mode(struct net_device *dev);
extern void		__dev_set_rx_mode(struct net_device *dev);
extern int		dev_unicast_delete(struct net_device *dev, void *addr, int alen);
extern int		dev_unicast_add(struct net_device *dev, void *addr, int alen);
extern int 		dev_mc_delete(struct net_device *dev, void *addr, int alen, int all);
extern int		dev_mc_add(struct net_device *dev, void *addr, int alen, int newonly);
extern int		dev_mc_sync(struct net_device *to, struct net_device *from);
extern void		dev_mc_unsync(struct net_device *to, struct net_device *from);
extern int 		__dev_addr_delete(struct dev_addr_list **list, int *count, void *addr, int alen, int all);
extern int		__dev_addr_add(struct dev_addr_list **list, int *count, void *addr, int alen, int newonly);
extern void		dev_set_promiscuity(struct net_device *dev, int inc);
extern void		dev_set_allmulti(struct net_device *dev, int inc);
extern void		netdev_state_change(struct net_device *dev);
extern void		netdev_features_change(struct net_device *dev);
/* Load a device via the kmod */
extern void		dev_load(struct net *net, const char *name);
extern void		dev_mcast_init(void);
extern int		netdev_max_backlog;
extern int		weight_p;
extern int		netdev_set_master(struct net_device *dev, struct net_device *master);
extern int skb_checksum_help(struct sk_buff *skb);
extern struct sk_buff *skb_gso_segment(struct sk_buff *skb, int features);
#ifdef CONFIG_BUG
extern void netdev_rx_csum_fault(struct net_device *dev);
#else
static inline void netdev_rx_csum_fault(struct net_device *dev)
{
}
#endif
/* rx skb timestamps */
extern void		net_enable_timestamp(void);
extern void		net_disable_timestamp(void);

#ifdef CONFIG_PROC_FS
extern void *dev_seq_start(struct seq_file *seq, loff_t *pos);
extern void *dev_seq_next(struct seq_file *seq, void *v, loff_t *pos);
extern void dev_seq_stop(struct seq_file *seq, void *v);
#endif

extern void linkwatch_run_queue(void);

extern int netdev_compute_features(unsigned long all, unsigned long one);

static inline int net_gso_ok(int features, int gso_type)
{
	int feature = gso_type << NETIF_F_GSO_SHIFT;
	return (features & feature) == feature;
}

static inline int skb_gso_ok(struct sk_buff *skb, int features)
{
	return net_gso_ok(features, skb_shinfo(skb)->gso_type);
}

static inline int netif_needs_gso(struct net_device *dev, struct sk_buff *skb)
{
	return skb_is_gso(skb) &&
	       (!skb_gso_ok(skb, dev->features) ||
		unlikely(skb->ip_summed != CHECKSUM_PARTIAL));
}

/* On bonding slaves other than the currently active slave, suppress
 * duplicates except for 802.3ad ETH_P_SLOW, alb non-mcast/bcast, and
 * ARP on active-backup slaves with arp_validate enabled.
 */
static inline int skb_bond_should_drop(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct net_device *master = dev->master;

	if (master &&
	    (dev->priv_flags & IFF_SLAVE_INACTIVE)) {
		if ((dev->priv_flags & IFF_SLAVE_NEEDARP) &&
		    skb->protocol == __constant_htons(ETH_P_ARP))
			return 0;

		if (master->priv_flags & IFF_MASTER_ALB) {
			if (skb->pkt_type != PACKET_BROADCAST &&
			    skb->pkt_type != PACKET_MULTICAST)
				return 0;
		}
		if (master->priv_flags & IFF_MASTER_8023AD &&
		    skb->protocol == __constant_htons(ETH_P_SLOW))
			return 0;

		return 1;
	}
	return 0;
}

#endif /* __KERNEL__ */

#endif	/* _LINUX_DEV_H */
