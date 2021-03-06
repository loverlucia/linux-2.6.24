/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Implementation of the Transmission Control Protocol(TCP).
 *
 * Version:	$Id: tcp_timer.c,v 1.88 2002/02/01 22:01:04 davem Exp $
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Charles Hedrick, <hedrick@klinzhai.rutgers.edu>
 *		Linus Torvalds, <torvalds@cs.helsinki.fi>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Matthew Dillon, <dillon@apollo.west.oic.com>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Jorge Cwik, <jorge@laser.satlink.net>
 */

#include <linux/module.h>
#include <net/tcp.h>
//the number of retries allowed to retransmit a SYN
//segment after which we should give up.
int sysctl_tcp_syn_retries __read_mostly = TCP_SYN_RETRIES;
int sysctl_tcp_synack_retries __read_mostly = TCP_SYNACK_RETRIES;
//在TCP保活打开的情况下，最后一次数据交换到TCP发送第一个保活探测消息的时间，即允许的持续空闲时间。默认值为7200s(2h)
int sysctl_tcp_keepalive_time __read_mostly = TCP_KEEPALIVE_TIME;
//TCP发送保活探测消息以确定连接是否断开的次数。默认为9次
//注意:只有设置了SO_KEEPALIVE套接字选项后才会发送保活探测消息
int sysctl_tcp_keepalive_probes __read_mostly = TCP_KEEPALIVE_PROBES;
//保活探测消息的发送频率。默认为75s
//发送频率sysctl_tcp_keepalive_probes乘以发送次数sysctl_tcp_keepalive_probes，就得到了从开始探测直到放弃探测确定连接断开的时间，大约为11min
int sysctl_tcp_keepalive_intvl __read_mostly = TCP_KEEPALIVE_INTVL;
//当重传次数超过此值时，可能遇到了黑洞，因此要检测PMTU是否有效。并清除缓存在传输控制块中的目的路由缓存项，在下次重传时会重新进行路由选择。
//默认值3(次)，根据RTO的值大约在3s--8min
//the maximum number of retries after which we need to check
//if the intermediate router has failed. If the number of retransmits exceeds this value,
//route - specific negative_advice routine is called (dst→ops→negative_advice()) from
//dst_negative_advice(). In the case of Ipv4, this is ipv4_negative_advice(), which sets
//sk→dst to NULL in case the route has become obsolete or the destination has
//expired. rt_check_expire() is run as a periodic timer for routing entries cached with
//the kernel to check old not - in - use entries.
int sysctl_tcp_retries1 __read_mostly = TCP_RETR1;
//持续定时器周期性发送TCP段或超时重传时，在确定断开TCP连接之前，最多尝试的次数。
//默认值15(次)，根据RTO的值大约相当于13-30min。RFC1122规定，该值必须大于100s
//the maximum number of retries the segment should be
//retransmitted after which we should give up on the connection.
int sysctl_tcp_retries2 __read_mostly = TCP_RETR2;
//For an orphaned socket (that is detached from the process context but exists
//to do some cleanup work), we have some more hard rules for number of retries.
//The maximum number of retries for an orphaned socket is sysctl_tcp_orphan_retries.
//Still we need to kill an orphaned socket in two cases even if it has not exhausted
//its retries (check tcp_out_of_resources()):
//1. Total number of orphaned sockets has exceeded the system - wide maximum
//allowed number (sysctl_tcp_max_orphans).
//2. There is acute memory pressure (tcp_memory_allocated >
//sysctl_tcp_mem[2]).
int sysctl_tcp_orphan_retries __read_mostly;

static void tcp_write_timer(unsigned long);
static void tcp_delack_timer(unsigned long);
static void tcp_keepalive_timer (unsigned long data);

void tcp_init_xmit_timers(struct sock *sk)
{
	inet_csk_init_xmit_timers(sk, &tcp_write_timer, &tcp_delack_timer, &tcp_keepalive_timer);
}

EXPORT_SYMBOL(tcp_init_xmit_timers);

static void tcp_write_err(struct sock *sk)
{
	sk->sk_err = sk->sk_err_soft ? : ETIMEDOUT;
	sk->sk_error_report(sk);

	tcp_done(sk);
	NET_INC_STATS_BH(LINUX_MIB_TCPABORTONTIMEOUT);
}

/* Do not allow orphaned sockets to eat all our resources.
 * This is direct violation of TCP specs, but it is required
 * to prevent DoS attacks. It is called when a retransmission timeout
 * or zero probe timeout occurs on orphaned socket.
 *
 * Criteria is still not confirmed experimentally and may change.
 * We kill the socket, if:
 * 1. If number of orphaned sockets exceeds an administratively configured
 *    limit.
 * 2. If we have strong memory pressure.
 */
static int tcp_out_of_resources(struct sock *sk, int do_reset)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int orphans = atomic_read(&tcp_orphan_count);

	/* If peer does not open window for long time, or did not transmit
	 * anything for long time, penalize it. */
	if ((s32)(tcp_time_stamp - tp->lsndtime) > 2*TCP_RTO_MAX || !do_reset)
		orphans <<= 1;

	/* If some dubious ICMP arrived, penalize even more. */
	if (sk->sk_err_soft)
		orphans <<= 1;

	if (tcp_too_many_orphans(sk, orphans)) {
		if (net_ratelimit())
			printk(KERN_INFO "Out of socket memory\n");

		/* Catch exceptional cases, when connection requires reset.
		 *      1. Last segment was sent recently. */
		if ((s32)(tcp_time_stamp - tp->lsndtime) <= TCP_TIMEWAIT_LEN ||
		    /*  2. Window is closed. */
		    (!tp->snd_wnd && !tp->packets_out))
			do_reset = 1;
		if (do_reset)
			tcp_send_active_reset(sk, GFP_ATOMIC);
		tcp_done(sk);
		NET_INC_STATS_BH(LINUX_MIB_TCPABORTONMEMORY);
		return 1;
	}
	return 0;
}

/* Calculate maximal number or retries on an orphaned socket. */
static int tcp_orphan_retries(struct sock *sk, int alive)
{
	int retries = sysctl_tcp_orphan_retries; /* May be zero. */

	/* We know from an ICMP that something is wrong. */
	if (sk->sk_err_soft && !alive)
		retries = 0;

	/* However, if socket sent something recently, select some safe
	 * number of retries. 8 corresponds to >100 seconds with minimal
	 * RTO of 200msec. */
	if (retries == 0 && alive)
		retries = 8;
	return retries;
}

/* A write timeout has occurred. Process the after effects. */
//判断我们重传的次数是否超过了重传的最大次数
//返回值: 1--超过  0--未超过
static int tcp_write_timeout(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int retry_until;
	int mss;

	//SYN分节的重传
	if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))	
	{
		if (icsk->icsk_retransmits)
			dst_negative_advice(&sk->sk_dst_cache);
		retry_until = icsk->icsk_syn_retries ? : sysctl_tcp_syn_retries;
	}
	//数据的重传
	else 
	{
		if (icsk->icsk_retransmits >= sysctl_tcp_retries1)
		{
			/* Black hole detection */
			if (sysctl_tcp_mtu_probing)
			{
				if (!icsk->icsk_mtup.enabled) 
				{
					icsk->icsk_mtup.enabled = 1;
					tcp_sync_mss(sk, icsk->icsk_pmtu_cookie);
				} 
				else 
				{
					mss = min(sysctl_tcp_base_mss, tcp_mtu_to_mss(sk, icsk->icsk_mtup.search_low)/2);
					mss = max(mss, 68 - tp->tcp_header_len);
					icsk->icsk_mtup.search_low = tcp_mss_to_mtu(sk, mss);
					tcp_sync_mss(sk, icsk->icsk_pmtu_cookie);
				}
			}

			dst_negative_advice(&sk->sk_dst_cache);
		}

		retry_until = sysctl_tcp_retries2;

		//处理是孤立的socket的情况
		if (sock_flag(sk, SOCK_DEAD)) 
		{
			const int alive = (icsk->icsk_rto < TCP_RTO_MAX);

			retry_until = tcp_orphan_retries(sk, alive);

			if (tcp_out_of_resources(sk, alive || icsk->icsk_retransmits < retry_until))
				return 1;
		}
	}

	if (icsk->icsk_retransmits >= retry_until)
	{
		/* Has it gone just too far? */
		tcp_write_err(sk);
		return 1;
	}
	return 0;
}

static void tcp_delack_timer(unsigned long data)
{
	struct sock *sk = (struct sock*)data;
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		/* Try again later. */
		icsk->icsk_ack.blocked = 1;
		NET_INC_STATS_BH(LINUX_MIB_DELAYEDACKLOCKED);
		sk_reset_timer(sk, &icsk->icsk_delack_timer, jiffies + TCP_DELACK_MIN);
		goto out_unlock;
	}

	sk_stream_mem_reclaim(sk);

	if (sk->sk_state == TCP_CLOSE || !(icsk->icsk_ack.pending & ICSK_ACK_TIMER))
		goto out;

	if (time_after(icsk->icsk_ack.timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_delack_timer, icsk->icsk_ack.timeout);
		goto out;
	}
	icsk->icsk_ack.pending &= ~ICSK_ACK_TIMER;

	if (!skb_queue_empty(&tp->ucopy.prequeue))
	{
		struct sk_buff *skb;

		NET_INC_STATS_BH(LINUX_MIB_TCPSCHEDULERFAILED);

		while ((skb = __skb_dequeue(&tp->ucopy.prequeue)) != NULL)
			sk->sk_backlog_rcv(sk, skb);

		tp->ucopy.memory = 0;
	}

	if (inet_csk_ack_scheduled(sk)) {
		if (!icsk->icsk_ack.pingpong) {
			/* Delayed ACK missed: inflate ATO. */
			icsk->icsk_ack.ato = min(icsk->icsk_ack.ato << 1, icsk->icsk_rto);
		} else {
			/* Delayed ACK missed: leave pingpong mode and
			 * deflate ATO.
			 */
			icsk->icsk_ack.pingpong = 0;
			icsk->icsk_ack.ato      = TCP_ATO_MIN;
		}
		tcp_send_ack(sk);
		NET_INC_STATS_BH(LINUX_MIB_DELAYEDACKS);
	}
	TCP_CHECK_TIMER(sk);

out:
	if (tcp_memory_pressure)
		sk_stream_mem_reclaim(sk);
out_unlock:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static void tcp_probe_timer(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int max_probes;

	if (tp->packets_out || !tcp_send_head(sk)) {
		icsk->icsk_probes_out = 0;
		return;
	}

	/* *WARNING* RFC 1122 forbids this
	 *
	 * It doesn't AFAIK, because we kill the retransmit timer -AK
	 *
	 * FIXME: We ought not to do it, Solaris 2.5 actually has fixing
	 * this behaviour in Solaris down as a bug fix. [AC]
	 *
	 * Let me to explain. icsk_probes_out is zeroed by incoming ACKs
	 * even if they advertise zero window. Hence, connection is killed only
	 * if we received no ACKs for normal connection timeout. It is not killed
	 * only because window stays zero for some time, window may be zero
	 * until armageddon and even later. We are in full accordance
	 * with RFCs, only probe timer combines both retransmission timeout
	 * and probe timeout in one bottle.				--ANK
	 */
	max_probes = sysctl_tcp_retries2;

	if (sock_flag(sk, SOCK_DEAD)) {
		const int alive = ((icsk->icsk_rto << icsk->icsk_backoff) < TCP_RTO_MAX);

		max_probes = tcp_orphan_retries(sk, alive);

		if (tcp_out_of_resources(sk, alive || icsk->icsk_probes_out <= max_probes))
			return;
	}

	if (icsk->icsk_probes_out > max_probes) {
		tcp_write_err(sk);
	} else {
		/* Only send another probe if we didn't close things up. */
		tcp_send_probe0(sk);
	}
}

/*
 *	The TCP retransmit timer.
 */
 
static void tcp_retransmit_timer(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	//If no packets are transmitted, just return because we have nothing to retransmit  
	if (!tp->packets_out)
		goto out;

	BUG_TRAP(!tcp_write_queue_empty(sk));

 	//we check if the socket is still alive and not in the SYN_SENT/SYN_RECV state 
 	//and if somehow the send window is closed
	if (!tp->snd_wnd && !sock_flag(sk, SOCK_DEAD) && !((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV)))
	{
		/* Receiver dastardly shrinks window. Our retransmits
		 * become zero probes, but we should not timeout this
		 * connection. If the socket is an orphan, time it out,
		 * we cannot allow such beasts to hang infinitely.
		 */
#ifdef TCP_DEBUG
		if (1)
		{
			struct inet_sock *inet = inet_sk(sk);
			LIMIT_NETDEBUG(KERN_DEBUG "TCP: Treason uncloaked! Peer %u.%u.%u.%u:%u/%u shrinks window %u:%u. Repaired.\n",
			       NIPQUAD(inet->daddr), ntohs(inet->dport), inet->num, tp->snd_una, tp->snd_nxt);
		}
#endif
		//we need to timeout the connection in case we have not received
		//any ACK from the peer for more than TCP_RTO_MAX
		if (tcp_time_stamp - tp->rcv_tstamp > TCP_RTO_MAX)
		{
			tcp_write_err(sk);
			goto out;
		}
		//In case the socket is not timed out, we enter the loss state by entering slow - start (call tcp_enter_loss())
		tcp_enter_loss(sk, 0);
		//retransmit the head of the retransmit queue
		tcp_retransmit_skb(sk, tcp_write_queue_head(sk));
		//then invalidate the destination by calling __sk_dst_reset(). 
		//The reason for finding an alternate route
		//for the connection may be that we are not able to communicate with the peer
		//because of which we may not be able to get window updates.
		__sk_dst_reset(sk);
		//we reset the retransmit timer doubling timeout 
		goto out_reset_timer;
	}

	//判断我们重传需要的次数。如果超过了重传次数，直接跳转到out。 
	//Check if we have actually exhausted all our retries 
	if (tcp_write_timeout(sk))
		goto out;

	//收集一些统计信息
	if (icsk->icsk_retransmits == 0)
	{
		if (icsk->icsk_ca_state == TCP_CA_Disorder || icsk->icsk_ca_state == TCP_CA_Recovery) 
		{
			if (tcp_is_sack(tp))
			{
				if (icsk->icsk_ca_state == TCP_CA_Recovery)
					NET_INC_STATS_BH(LINUX_MIB_TCPSACKRECOVERYFAIL);
				else
					NET_INC_STATS_BH(LINUX_MIB_TCPSACKFAILURES);
			} 
			else 
			{
				if (icsk->icsk_ca_state == TCP_CA_Recovery)
					NET_INC_STATS_BH(LINUX_MIB_TCPRENORECOVERYFAIL);
				else
					NET_INC_STATS_BH(LINUX_MIB_TCPRENOFAILURES);
			}
		} 
		else if (icsk->icsk_ca_state == TCP_CA_Loss)
		{
			NET_INC_STATS_BH(LINUX_MIB_TCPLOSSFAILURES);
		}
		else
		{
			NET_INC_STATS_BH(LINUX_MIB_TCPTIMEOUTS);
		}
	}

	if (tcp_use_frto(sk)) 
	{
		tcp_enter_frto(sk);
	} 
	else
	{
		//we have not exhausted our retries. We need to
		//call tcp_enter_loss() to enter into the slow - start phase
		tcp_enter_loss(sk, 0);
	}

	//we try to retransmit the first segment from the retransmit queue
	if (tcp_retransmit_skb(sk, tcp_write_queue_head(sk)) > 0) 
	{
		/* Retransmission failed because of local congestion,  do not backoff.*/
		//we fail to retransmit here, the reason for failure is local congestion.
		//In this case, we don't back off the retransmit timeout value. We reset the retransmit
		//timer with a minimum timeout value of icsk->icsk_rto and TCP_RESOURCE_PROBE_INTERVAL. 
		//Since we need to probe availability of local resources more frequently
		//than RTO, that is why we want the tcp retransmit timer to expire fast so that we
		//can retransmit the lost segment.
		if (!icsk->icsk_retransmits)
			icsk->icsk_retransmits = 1;
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS, min(icsk->icsk_rto, TCP_RESOURCE_PROBE_INTERVAL), TCP_RTO_MAX);
		goto out;
	}

	/* Increase the timeout each time we retransmit.  Note that
	 * we do not increase the rtt estimate.  rto is initialized
	 * from rtt, but increases here.  Jacobson (SIGCOMM 88) suggests
	 * that doubling rto each time is the least we can get away with.
	 * In KA9Q, Karn uses this for the first few times, and then
	 * goes to quadratic.  netBSD doubles, but only goes up to *64,
	 * and clamps at 1 to 64 sec afterwards.  Note that 120 sec is
	 * defined in the protocol as the maximum possible RTT.  I guess
	 * we'll have to use something other than TCP to talk to the
	 * University of Mars.
	 *
	 * PAWS allows us longer timeouts and large windows, so once
	 * implemented ftp to mars will work nicely. We will have to fix
	 * the 120 second clamps though!
	 */
	///icsk->icsk_backoff主要用在零窗口定时器
	//We increment tp?back_off by one
	// Even though we are not using the value of tp?back_off here, it is
	//required by the zero - window probe timer.
	icsk->icsk_backoff++;
	icsk->icsk_retransmits++;

out_reset_timer:
	//要注意，重传的时候为了防止确认二义性，使用karn算法，也就是定时器退避策略

	 //(RTO can 't exceed beyond TCP_RTO_MAX). get the min of backoffed value of RTO and  TCP_RTO_MAX
	icsk->icsk_rto = min(icsk->icsk_rto << 1, TCP_RTO_MAX);
	//we reset the retransmit timer to expire at the backoffed value of RTO, tp->rto
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS, icsk->icsk_rto, TCP_RTO_MAX);
	//check if the maximum number of retries has exceeded the limit to
	//reset route. If so, we reset the route for the connection so that on next
	//retransmit we are able to find a new route for the connection because the current
	//route may be causing a problem.
	if (icsk->icsk_retransmits > sysctl_tcp_retries1) 
		__sk_dst_reset(sk);

out:;
}

static void tcp_write_timer(unsigned long data)
{
	struct sock *sk = (struct sock*)data;
	struct inet_connection_sock *icsk = inet_csk(sk);
	int event;
	///首先加锁。
	bh_lock_sock(sk);
	///如果是进程上下文则什么也不做。
	if (sock_owned_by_user(sk))
	{
		/* Try again later */
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer, jiffies + (HZ / 20));
		goto out_unlock;
	}

	if (sk->sk_state == TCP_CLOSE || !icsk->icsk_pending)
		goto out;

	//若定时器未到期，重置为到期时间
	if (time_after(icsk->icsk_timeout, jiffies)) 
	{
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer, icsk->icsk_timeout);
		goto out;
	}

	event = icsk->icsk_pending;
	icsk->icsk_pending = 0;

	switch (event)
	{
	case ICSK_TIME_RETRANS:
		tcp_retransmit_timer(sk);
		break;
	case ICSK_TIME_PROBE0:
		tcp_probe_timer(sk);
		break;
	}
	TCP_CHECK_TIMER(sk);

out:
	sk_stream_mem_reclaim(sk);
out_unlock:
	bh_unlock_sock(sk);
	sock_put(sk);
}

/*
 *	Timer for listening sockets
 */

static void tcp_synack_timer(struct sock *sk)
{
	inet_csk_reqsk_queue_prune(sk, TCP_SYNQ_INTERVAL,
				   TCP_TIMEOUT_INIT, TCP_RTO_MAX);
}

void tcp_set_keepalive(struct sock *sk, int val)
{
	if ((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_LISTEN))
		return;

	if (val && !sock_flag(sk, SOCK_KEEPOPEN))
		inet_csk_reset_keepalive_timer(sk, keepalive_time_when(tcp_sk(sk)));
	else if (!val)
		inet_csk_delete_keepalive_timer(sk);
}


static void tcp_keepalive_timer (unsigned long data)
{
	struct sock *sk = (struct sock *) data;
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	__u32 elapsed;

	/* Only process if socket is not in use. */
	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		/* Try again later. */
		inet_csk_reset_keepalive_timer (sk, HZ/20);
		goto out;
	}

	if (sk->sk_state == TCP_LISTEN) {
		tcp_synack_timer(sk);
		goto out;
	}

	if (sk->sk_state == TCP_FIN_WAIT2 && sock_flag(sk, SOCK_DEAD)) {
		if (tp->linger2 >= 0) {
			const int tmo = tcp_fin_time(sk) - TCP_TIMEWAIT_LEN;

			if (tmo > 0) {
				tcp_time_wait(sk, TCP_FIN_WAIT2, tmo);
				goto out;
			}
		}
		tcp_send_active_reset(sk, GFP_ATOMIC);
		goto death;
	}

	if (!sock_flag(sk, SOCK_KEEPOPEN) || sk->sk_state == TCP_CLOSE)
		goto out;

	elapsed = keepalive_time_when(tp);

	/* It is alive without keepalive 8) */
	if (tp->packets_out || tcp_send_head(sk))
		goto resched;

	elapsed = tcp_time_stamp - tp->rcv_tstamp;

	if (elapsed >= keepalive_time_when(tp)) {
		if ((!tp->keepalive_probes && icsk->icsk_probes_out >= sysctl_tcp_keepalive_probes) ||
		     (tp->keepalive_probes && icsk->icsk_probes_out >= tp->keepalive_probes)) {
			tcp_send_active_reset(sk, GFP_ATOMIC);
			tcp_write_err(sk);
			goto out;
		}
		if (tcp_write_wakeup(sk) <= 0) {
			icsk->icsk_probes_out++;
			elapsed = keepalive_intvl_when(tp);
		} else {
			/* If keepalive was lost due to local congestion,
			 * try harder.
			 */
			elapsed = TCP_RESOURCE_PROBE_INTERVAL;
		}
	} else {
		/* It is tp->rcv_tstamp + keepalive_time_when(tp) */
		elapsed = keepalive_time_when(tp) - elapsed;
	}

	TCP_CHECK_TIMER(sk);
	sk_stream_mem_reclaim(sk);

resched:
	inet_csk_reset_keepalive_timer (sk, elapsed);
	goto out;

death:
	tcp_done(sk);

out:
	bh_unlock_sock(sk);
	sock_put(sk);
}
