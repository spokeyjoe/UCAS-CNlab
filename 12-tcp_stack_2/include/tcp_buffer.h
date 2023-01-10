#ifndef __TCP_BUFFER_H__
#define __TCP_BUFFER_H__

#include "tcp_sock.h"

// TCP send buffer entry
typedef struct {
    struct list_head list;
    char *packet;
    int len;
} send_buffer_entry_t;

// TCP receive out-of-order buffer entry
typedef struct {
    struct list_head list;
    char *data;
    int len;
    int seq;
} rcv_ofo_buf_entry_t;

void send_buffer_add(struct tcp_sock *tsk, char *packet, int len);
void send_buffer_clear_ack(struct tcp_sock *tsk, u32 ack);
void send_buffer_retrans(struct tcp_sock *tsk);

void ofo_buf_add(struct tcp_sock *tsk, struct tcp_cb *cb);
void ofo_buffer_check_ack(struct tcp_sock *tsk);