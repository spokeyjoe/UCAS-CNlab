#include "tcp_buffer.h"
#include "log.h"

/* Add an entry for PACKET in send buffer. */
void send_buffer_add(struct tcp_sock *tsk, char *packet, int len) {
    send_buffer_entry_t *entry = (send_buffer_entry_t *)malloc(sizeof(send_buffer_entry_t));
    memset(entry, 0, sizeof(send_buffer_entry_t));
    entry->packet = (char *)malloc(len);
    memcpy(entry->packet, packet, len);
    entry->len = len;
    list_add_tail(&entry->list, &tsk->send_buf);
}

/* Remove all packets acknowledged by ACK (seq_end <= ack). */
void send_buffer_clear_ack(struct tcp_sock *tsk, u32 ack) {
    send_buffer_entry_t *p, *q;
    list_for_each_entry_safe(p, q, &tsk->send_buf, list) {
        struct tcphdr *tcp = packet_to_tcp_hdr(p->packet);
        u32 seq = ntohl(tcp->seq);
        if (less_than_32b(seq, ack)) {
            list_delete_entry(&p->list);
            free(p->packet);
            free(p);
        }
    }
}

/* Retransmit first packet in send buffer of TSK. */
void send_buffer_retrans(struct tcp_sock *tsk) {
    if (list_empty(&tsk->send_buf)) {
        log(DEBUG, "no packet in send buffer to resend.")
        return;
    }
    send_buffer_entry_t *entry = list_entry(tsk->send_buf.next, send_buffer_entry_t, list);

    // alloc mem, we need to modify the packet and resend
    char *packet = (char *)malloc(entry->len);
    memcpy(packet, entry->packet, entry->len);
    struct iphdr *ip = packet_to_ip_hdr(packet);
    struct tcphdr *tcp = packet_to_tcp_hdr(packet);

    tcp->ack = htonl(tsk->rcv_nxt);
    tcp->checksum = tcp_checksum(ip, tcp);
    ip->checksum = ip_checksum(ip);
    int tcp_data_len = ntohs(ip->tot_len) - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE;
    tsk->snd_wnd -= tcp_data_len;
    log(DEBUG, "retrans seq: %u\n", ntohl(tcp->seq));
    ip_send_packet(packet, entry->len); 
}

/* Add packet to ofo buffer. */
void ofo_buf_add(struct tcp_sock *tsk, struct tcp_cb *cb) {
    // allocate entry
    rcv_ofo_buf_entry_t *entry = (rcv_ofo_buf_entry_t *)malloc(sizeof(rcv_ofo_buf_entry_t));
    entry->seq = cb->seq;
    entry->len = cb->pl_len;
    entry->data = (char *)malloc(cb->pl_len);
    memcpy(entry->data, cb->payload, cb->pl_len);

    if (list_empty(&tsk->rcv_ofo_buf)) {
        list_add_tail(&entry->list, &tsk->rcv_ofo_buf);
    }
    // not empty: find slot
    rcv_ofo_buf_entry_t *p;
    list_for_each_entry(p, &tsk->rcv_ofo_buf, list) {
        if (less_than_32b(entry->seq, p->seq)) {
            break;
        }
    }
    list_add_head(&entry->list, &p->list);
}

/* Check packets in ofo buffer,
write all packets that can be acked to rcv_buffer. */
void ofo_buffer_check_ack(struct tcp_sock *tsk) {
    rcv_ofo_buf_entry_t *p, *q;
    list_for_each_entry_safe(p, q, &tsk->rcv_ofo_buf, list) {
        if (tsk->rcv_nxt == p->seq) {
            write_ring_buffer_safe(tsk, p->data, p->len);
            log(DEBUG, "write ofo data to ring buffer, seq: %u, len: %d", p->seq, p->len);
            tsk->rcv_nxt += p->len;
            list_delete_entry(&p->list);
            free(p->data);
            free(p);
        } else if (less_than_32b(tsk->rcv_nxt, p->seq)) {
            break;
        }
    }
}