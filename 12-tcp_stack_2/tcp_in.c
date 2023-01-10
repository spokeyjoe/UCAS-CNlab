#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"
#include "tcp_buffer.h"

#include <stdlib.h>

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
    u16 old_snd_wnd = tsk->snd_wnd;
    tsk->snd_wnd = cb->rwnd;
    if (old_snd_wnd == 0)
        wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
    if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
        tcp_update_window(tsk, cb);
}

#ifndef max
#   define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
    u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
    if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
        return 1;
    }
    else {
        log(ERROR, "received packet with invalid seq, drop it.");
        return 0;
    }
}

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
    if (cb->flags & TCP_RST) {
        tcp_sock_close(tsk);
        return;
    }

    // update snd_una & rcv_nxt
    if (cb->flags & TCP_ACK) {
        tsk->snd_una = cb->ack;
    }

    tsk->rcv_nxt = cb->seq_end;

    // ACK: clear send buffer
    if (cb->flags & TCP_ACK) {
        send_buffer_clear_ack(tsk, cb->ack);
    }

    // state machine

    switch (tsk->state) {
    case TCP_LISTEN:
        if (cb->flags == TCP_SYN) {
            struct tcp_sock *csk = alloc_child_tcp_sock(tsk, cb);
            tcp_send_control_packet(csk, TCP_SYN | TCP_ACK);
            tcp_set_state(csk, TCP_SYN_RECV);
        }
        break;
    case TCP_SYN_SENT:
        if (cb->flags == (TCP_SYN | TCP_ACK)) {
            tcp_update_window_safe(tsk, cb);
            tcp_send_control_packet(tsk, TCP_ACK);
            tcp_set_state(tsk, TCP_ESTABLISHED);
            wake_up(tsk->wait_connect);
            tcp_delete_retrans_timer(tsk);
        }
        break;
    case TCP_SYN_RECV:
        if (cb->flags == TCP_ACK) {
            tcp_sock_accept_enqueue(tsk);
            tcp_set_state(tsk, TCP_ESTABLISHED);
            wake_up(tsk->wait_accept);
            tcp_delete_retrans_timer(tsk);

            // it could be the first data segment
            if (cb->pl_len != 0){
                // out of order, add to ofo buffer
                if (greater_than_32b(tsk->rcv_nxt, cb->seq)) {
                    ofo_buf_add(tsk, cb);
                } else { // in order, write to ring buffer
                    write_ring_buffer_safe(tsk, cb->payload, cb->pl_len);
                    tcp_send_control_packet(tsk, TCP_ACK);

                    // check ofo buffer
                    ofo_buffer_check_ack(tsk);
                }
            }
        }
        break;
    case TCP_ESTABLISHED:
        if (cb->flags & TCP_FIN) {
            tcp_send_control_packet(tsk, TCP_ACK);
            tcp_set_state(tsk, TCP_CLOSE_WAIT);
            if (tsk->wait_empty->sleep) {
                wake_up(tsk->wait_empty);
            }
        }
        
        if (cb->flags & TCP_ACK) {
            // duplicate packet
            if (cb->seq_end == tsk->rcv_nxt) {
                tcp_send_control_packet(tsk, TCP_ACK);
                return;
            }
            tcp_update_window_safe(tsk, cb);

            if (cb->pl_len != 0) {
                // out of order, add to ofo buffer
                if (greater_than_32b(tsk->rcv_nxt, cb->seq)) {
                    ofo_buf_add(tsk, cb);
                } else { // in order, write to ring buffer
                    write_ring_buffer_safe(tsk, cb->payload, cb->pl_len);
                    tcp_send_control_packet(tsk, TCP_ACK);

                    // check ofo buffer
                    ofo_buffer_check_ack(tsk);
                }
            } else { // got ACK from receiver
                // all data are acked
                if (cb->ack == tsk->snd_nxt) {
                    tcp_delete_retrans_timer(tsk);
                } else {
                    tcp_reset_retrans_timer(tsk);
                }

            }
        }
        break;

    case TCP_CLOSE_WAIT:
        if (cb->flags & TCP_FIN) {
            tcp_send_control_packet(tsk, TCP_ACK);
        }
        break;
    case TCP_FIN_WAIT_1:
        if (cb->flags == TCP_ACK) {
            tcp_set_state(tsk, TCP_FIN_WAIT_2);
            tcp_delete_retrans_timer(tsk);
        } else if (cb->flags == (TCP_ACK | TCP_FIN)) {
            tcp_send_control_packet(tsk, TCP_ACK);
            tcp_set_state(tsk, TCP_TIME_WAIT);
            tcp_set_timewait_timer(tsk);
            tcp_delete_retrans_timer(tsk);
        }
        break;
    case TCP_FIN_WAIT_2:
        if (cb->flags & TCP_FIN) {
            tcp_send_control_packet(tsk, TCP_ACK);
            tcp_set_state(tsk, TCP_TIME_WAIT);
            tcp_set_timewait_timer(tsk);
        }
        break;
    case TCP_LAST_ACK:
        if (cb->flags & TCP_ACK) {
            tcp_unhash(tsk);
            tcp_set_state(tsk, TCP_CLOSED);
            tcp_delete_retrans_timer(tsk);
        }
        break;
    case TCP_TIME_WAIT:
        if (cb->flags & TCP_FIN) {
            tcp_send_control_packet(tsk, TCP_ACK);
        }
        break;
    default:
        break;
    }
}

// create child socket & add to listen queue
struct tcp_sock *alloc_child_tcp_sock(struct tcp_sock *tsk, struct tcp_cb *cb) {
    struct tcp_sock *csk = alloc_tcp_sock();
    memcpy((char *)csk, (char *)tsk, sizeof(struct tcp_sock));
    csk->parent = tsk;
    csk->sk_sip = cb->daddr;
    csk->sk_sport = cb->dport;
    csk->sk_dip = cb->saddr;
    csk->sk_dport = cb->sport;
    csk->iss = tcp_new_iss();
    csk->rcv_nxt = cb->seq_end;
    csk->snd_nxt = csk->iss;

    struct sock_addr *sa = malloc(sizeof(struct sock_addr));
    sa->ip = htonl(cb->daddr);
    sa->port = htons(cb->dport);
    tcp_sock_bind(csk, sa);
    tcp_hash(csk);

    list_add_tail(&csk->list, &tsk->listen_queue);

    return csk;
}

/* Wait until rcv_buffer has enough space and write LEN of DATA. */
void write_ring_buffer_safe(struct tcp_sock *tsk, char *data, int len) {
    pthread_mutex_lock(&tsk->rcv_buf_lock);

    while (ring_buffer_free(tsk->rcv_buf) < len) {
        // log(DEBUG, "STATE_MACHINE: ring buffer not enough for receiving packet, go to sleep");
        pthread_mutex_unlock(&tsk->rcv_buf_lock);
        sleep_on(tsk->wait_full);
        pthread_mutex_lock(&tsk->rcv_buf_lock);
        // log(DEBUG, "STATE_MACHINE: ring buffer not full, wake up to see if i can receive packet");
    }

    write_ring_buffer(tsk->rcv_buf, data, len);
    tsk->rcv_wnd = ring_buffer_free(tsk->rcv_buf);
    
    pthread_mutex_unlock(&tsk->rcv_buf_lock);

    if (tsk->wait_empty->sleep) {
        wake_up(tsk->wait_empty);
    }
}