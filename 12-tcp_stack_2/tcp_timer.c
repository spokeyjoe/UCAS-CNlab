#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"
#include "tcp_buffer.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, release time_wait timer that statys for 2*MSL
// and retransmit/release retrans timers 
void tcp_scan_timer_list()
{
	struct tcp_timer *p, *q;
    list_for_each_entry_safe (p, q, &timer_list, list) {
        p->timeout -= TCP_TIMER_SCAN_INTERVAL;
        if (p->enable == 1 && p->timeout <= 0) {
            struct tcp_sock *tsk = timewait_to_tcp_sock(p);
            if (p->type == TIME_WAIT) {
                list_delete_entry(&p->list);
                tcp_set_state(tsk, TCP_CLOSED);
                tcp_bind_unhash(tsk);
                tcp_unhash(tsk);
            } else {
                if (p->retrans_time == MAX_TRANS_ATTEMPT) {
                    list_delete_entry(&p->list);
                    tcp_sock_reset(tsk); 
                } else {
                    p->retrans_time += 1;
                    p->timeout = TCP_RETRANS_INTERVAL_INITIAL * (1 << p->retrans_time);
                    send_buffer_retrans(tsk);
                }
            }
        }
    }
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	tsk->timewait.enable = 1;
	tsk->timewait.type = TIME_WAIT;
    tsk->timewait.timeout = TCP_TIMEWAIT_TIMEOUT;
    list_add_tail(&tsk->timewait.list, &timer_list);
}

/* Set the retransmit timer of a tcp sock. */
void tcp_set_retrans_timer(struct tcp_sock *tsk) {
    tsk->retrans_timer.enable = 1;
    tsk->retrans_timer.type = RETRANS;
    tsk->retrans_timer.timeout = TCP_RETRANS_INTERVAL_INITIAL;
    tsk->retrans_timer.retrans_time = 0;
    list_add_tail(&tsk->retrans_timer.list, &timer_list);
}

/* Reset the retransmit timer of a tcp stock. */
void tcp_reset_retrans_timer(struct tcp_sock *tsk) {
    tsk->retrans_timer.timeout = TCP_RETRANS_INTERVAL_INITIAL;
    tsk->retrans_timer.retrans_time = 0;
}

/* Delete retrans timer of TSK. */
void tcp_delete_retrans_timer(struct tcp_sock *tsk) {
    list_delete_entry(&tsk->retrans_timer.list);
    tsk->retrans_timer.enable = 0;
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
