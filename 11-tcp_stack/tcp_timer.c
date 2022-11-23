#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	struct tcp_timer *p, *q;
    list_for_each_entry_safe (p, q, &timer_list, list) {
        p->timeout += TCP_TIMER_SCAN_INTERVAL;
        if (p->enable == 1 && p->timeout >= TCP_TIMEWAIT_TIMEOUT) {
            list_delete_entry(&p->list);
            struct tcp_sock *tsk = timewait_to_tcp_sock(p);
            tcp_set_state(tsk, TCP_CLOSED);
			// FIXME: use gui's method?
            tcp_bind_unhash(tsk);
            tcp_unhash(tsk);
        }
    }
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	tsk->timewait.enable = 1;
	tsk->timewait.type = 0;
    tsk->timewait.timeout = 0;
    list_add_tail(&tsk->timewait.list, &timer_list);
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}
