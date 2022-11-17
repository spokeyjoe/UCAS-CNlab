#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	u32 saddr = ntohl(ip_hdr->saddr);
	u32 daddr = ntohl(ip_hdr->daddr);
	rt_entry_t *s_entry = longest_prefix_match(saddr);
	rt_entry_t *d_entry = longest_prefix_match(daddr);

	if (s_entry->iface->index == nat.internal_iface->index &&
	    d_entry->iface->index == nat.external_iface->index) {
		return DIR_OUT;
	} else if (s_entry->iface->index == nat.external_iface->index &&
               daddr == nat.external_iface->ip) {
		return DIR_IN;
	} else {
		return DIR_INVALID;
	}
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	struct tcphdr *tcp_hdr = packet_to_tcp_hdr(packet);
	u32 saddr = ntohl(ip_hdr->saddr);
	u32 daddr = ntohl(ip_hdr->daddr);
	u32 raddr = dir == DIR_IN ? saddr : daddr;
	u16 sport = ntohs(tcp_hdr->sport);
	u16 dport = ntohs(tcp_hdr->dport);
	u16 rport = dir == DIR_IN ? sport : dport;

	char *rkey = malloc(sizeof(raddr) + sizeof(rport));
	memcpy(rkey, &raddr, sizeof(raddr));
	memcpy(rkey + sizeof(raddr), &rport, sizeof(rport));
	u8 idx = hash8(rkey, sizeof(raddr) + sizeof(rport));
	free(rkey);

    struct nat_mapping *p;
	pthread_mutex_lock(&nat.lock);
	time_t cur = time(NULL);

    // existing NAT mapping
	list_for_each_entry(p, &nat.nat_mapping_list[idx], list) {
		if (raddr == p->remote_ip && rport == p->remote_port) {
			if (dir == DIR_IN) {
				if (daddr != p->external_ip || dport != p->external_port) {
					continue;
				}
				ip_hdr->daddr = htonl(p->internal_ip);
				tcp_hdr->dport = htons(p->internal_port);
				p->conn.external_fin = tcp_hdr->flags & TCP_FIN ? TCP_FIN : 0;
				p->conn.external_seq_end = tcp_seq_end(ip_hdr, tcp_hdr);
				p->conn.external_ack = tcp_hdr->flags & TCP_ACK ? tcp_hdr->ack : p->conn.external_ack;
			} else {
				if (saddr != p->internal_ip || sport != p->internal_port) {
					continue;
				}
				ip_hdr->saddr = htonl(p->external_ip);
				tcp_hdr->sport = htons(p->external_port);
				p->conn.internal_fin = tcp_hdr->flags & TCP_FIN ? TCP_FIN : 0;
				p->conn.internal_seq_end = tcp_seq_end(ip_hdr, tcp_hdr);
				p->conn.internal_ack = tcp_hdr->flags & TCP_ACK ? tcp_hdr->ack : p->conn.internal_ack;
			}
			p->update_time = cur;
			tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
			ip_hdr->checksum = ip_checksum(ip_hdr);
			ip_send_packet(packet, len);

			pthread_mutex_unlock(&nat.lock);
			return;
		}
	}

	// invalid: not SYN
	if ((tcp_hdr->flags & TCP_SYN) == 0) {
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		pthread_mutex_unlock(&nat.lock);
		return;
	}

	// build new connection
	if (dir == DIR_OUT) {
		u16 new_port = assign_external_port();
		if (new_port == 0) {
			// port run out
			pthread_mutex_unlock(&nat.lock);
			icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
			free(packet);
			return;
		}
		struct nat_mapping *entry = malloc(sizeof(struct nat_mapping));
		list_add_tail(&entry->list, &nat.nat_mapping_list[idx]);
		entry->remote_ip = raddr;
		entry->remote_port = rport;
		entry->internal_ip = saddr;
		entry->internal_port = sport;
		entry->external_ip = nat.external_iface->ip;
		entry->external_port = new_port;
		entry->update_time = cur;

		entry->conn.internal_fin = tcp_hdr->flags & TCP_FIN ? TCP_FIN : 0;
		entry->conn.internal_seq_end = tcp_seq_end(ip_hdr, tcp_hdr);
		entry->conn.internal_ack = tcp_hdr->flags & TCP_ACK ? tcp_hdr->ack : 0;

		ip_hdr->saddr = htonl(entry->external_ip);
		tcp_hdr->sport = htons(entry->external_port);
		tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
		ip_hdr->checksum = ip_checksum(ip_hdr);
		ip_send_packet(packet, len);

		pthread_mutex_unlock(&nat.lock);
		return;
	} else {
		struct dnat_rule *r;
		list_for_each_entry(r, &nat.rules, list) {
			if (daddr == r->external_ip && dport == r->external_port) {
				struct nat_mapping *entry = malloc(sizeof(struct nat_mapping));
				list_add_tail(&entry->list, &nat.nat_mapping_list[idx]);
				entry->remote_ip = raddr;
				entry->remote_port = rport;
				entry->internal_ip = r->internal_ip;
				entry->internal_port = r->internal_port;
				entry->external_ip = r->external_ip;
				entry->external_port = r->external_port;

				entry->conn.external_fin = tcp_hdr->flags & TCP_FIN ? TCP_FIN : 0;
                entry->conn.external_seq_end = tcp_seq_end(ip_hdr, tcp_hdr);
                entry->conn.external_ack = tcp_hdr->flags & TCP_ACK ? tcp_hdr->ack : 0;

                ip_hdr->daddr = htonl(r->internal_ip);
				logaddr("internal_ip", r->internal_ip);
                tcp_hdr->dport = htons(r->internal_port);
                tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
                ip_hdr->checksum = ip_checksum(ip_hdr);
                ip_send_packet(packet, len);

                pthread_mutex_unlock(&nat.lock);
                return;
			}
		}
	}
}

u16 assign_external_port() {
	for (u16 port = NAT_PORT_MIN; port <= NAT_PORT_MAX; port += 1) {
		if (nat.assigned_ports[port] == 0) {				
			nat.assigned_ports[port] = 1;
			return port;
		}
	}
	return 0;
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}



// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		pthread_mutex_lock(&nat.lock);
		time_t cur = time(NULL);
		struct nat_mapping *p, *q;
		for (int i = 0; i < HASH_8BITS; i += 1) {
			list_for_each_entry_safe(p, q, &nat.nat_mapping_list[i], list) {
				if ((cur - p->update_time > TCP_ESTABLISHED_TIMEOUT) || is_flow_finished(&p->conn)) {
					nat.assigned_ports[p->external_port] = 0;
					list_delete_entry(&p->list);
					free(p);
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
		sleep(1);
	}
	return NULL;
}

int parse_config(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		log(ERROR, "cannot open %s", filename);
		return 1;
	}

	char line[256];
	char iname[32];

	while (fgets(line, sizeof(line) / sizeof(*line), fp)) {
        char *delim = strchr(line, ':');
        if (delim == NULL) { continue; }

        if (strncmp(line, "internal-iface", delim - line) == 0) {
            sscanf(delim + 1, "%s", iname);
            nat.internal_iface = if_name_to_iface(iname);
        } else if (strncmp(line, "external-iface", delim - line) == 0) {
            sscanf(delim + 1, "%s", iname);
            nat.external_iface = if_name_to_iface(iname);
        } else if (strncmp(line, "dnat-rules", delim - line) == 0) {
            u32 o_ip, i_ip;
            u16 o_port, i_port;

            if (sscanf(delim + 1, IP_FMT":%hu -> "IP_FMT":%hu", 
                    HOST_IP_SCAN_STR(o_ip), &o_port,
                    HOST_IP_SCAN_STR(i_ip), &i_port) < 10) {
                fprintf(stderr, "ERROR: illegal dnat rule format\n");
                return 1;
            }

            struct dnat_rule *rule = malloc(sizeof(*rule));
            rule->external_ip = o_ip;
            rule->internal_ip = i_ip;
            rule->external_port = o_port;
            rule->internal_port = i_port;
            list_add_tail(&rule->list, &nat.rules);
        }
    }

	return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	parse_config(config_file);

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit()
{
	pthread_mutex_lock(&nat.lock);

    for (int i = 0; i < HASH_8BITS; ++i) {
        struct nat_mapping *ptr = NULL, *q = NULL;
        list_for_each_entry_safe (ptr, q, &nat.nat_mapping_list[i], list) {
            list_delete_entry(&ptr->list);
            free(ptr);
        }
    }

    pthread_kill(nat.thread, SIGTERM);

    pthread_mutex_unlock(&nat.lock);
}
