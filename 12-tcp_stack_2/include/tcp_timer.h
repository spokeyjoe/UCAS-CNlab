#ifndef __TCP_TIMER_H__
#define __TCP_TIMER_H__

#include "list.h"

#include <stddef.h>

#define TIME_WAIT 0
#define RETRANS 1

struct tcp_timer {
	struct list_head list;
	int type;	    // time-wait: 0		retrans: 1
	int timeout;	// in micro second
	int enable;
	int retrans_time;
};

struct tcp_sock;
#define timewait_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, timewait))

#define retranstimer_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, retrans_timer))
#define TCP_TIMER_SCAN_INTERVAL 100000
#define TCP_MSL			1000000
#define TCP_TIMEWAIT_TIMEOUT	(2 * TCP_MSL)
#define TCP_RETRANS_INTERVAL_INITIAL 200000
#define MAX_TRANS_ATTEMPT 3

// the thread that scans timer_list periodically
void *tcp_timer_thread(void *arg);
// add the timer of tcp sock to timer_list
void tcp_set_timewait_timer(struct tcp_sock *);
void tcp_set_retrans_timer(struct tcp_sock *tsk);
void tcp_reset_retrans_timer(struct tcp_sock *tsk);
void tcp_delete_retrans_timer(struct tcp_sock *tsk);

#endif
