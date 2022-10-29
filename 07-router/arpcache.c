#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "icmp.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	pthread_mutex_lock(&arpcache.lock);
	for (int i = 0; i < MAX_ARP_SIZE; i += 1) {
		struct arp_cache_entry entry = arpcache.entries[i];
		if (entry.valid != 0 && entry.ip4 == ip4) {
			memcpy(mac, entry.mac, ETH_ALEN);
			pthread_mutex_unlock(&arpcache.lock);
			return 1;
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
	return 0;
}

// append the packet to arpcache
//
// Lookup in the list which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	struct cached_pkt *new_packet = malloc(sizeof(struct cached_pkt));
	new_packet->packet = packet;
	new_packet->len = len;

	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *p, *q;
	list_for_each_entry_safe(p, q, &arpcache.req_list, list) {
		// if such request exists, add new packet to packets tail
		if (p->ip4 == ip4 && p->iface == iface) {
			list_add_tail(&new_packet->list, &p->cached_packets);
			pthread_mutex_unlock(&arpcache.lock);
			return;
		}
	}

	// if no such request, create a new entry
	p = malloc(sizeof(struct arp_req));
	p->iface = iface;
	p->ip4 = ip4;
	p->retries = 0;
	init_list_head(&p->cached_packets);
	list_add_tail(&new_packet->list, &p->cached_packets); 
	list_add_tail(&p->list, &arpcache.req_list);

	// send request
	arp_send_request(iface, ip4);
	p->sent = time(NULL);

	pthread_mutex_unlock(&arpcache.lock);
}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	pthread_mutex_lock(&arpcache.lock);

    struct arp_cache_entry *replaced_entry = NULL;
	for (int i = 0; i < MAX_ARP_SIZE; i += 1) {
		if (arpcache.entries[i].valid == 0) {
			replaced_entry = arpcache.entries + i;
			break;
		}
	}
	if (replaced_entry == NULL) {
		replaced_entry = arpcache.entries + (time(NULL) % MAX_ARP_SIZE);
	}

	replaced_entry->valid = 1;
	replaced_entry->ip4   = ip4;
	replaced_entry->added = time(NULL);
	memcpy(replaced_entry->mac, mac, ETH_ALEN);

	// check cached arp request
	struct arp_req *p, *q;
	list_for_each_entry_safe(p, q, &arpcache.req_list, list) {
		// request waiting for this mapping
		if (p->ip4 == ip4) {
			struct cached_pkt *p_pkt, *q_pkt;
			list_for_each_entry_safe(p_pkt, q_pkt, &p->cached_packets, list) {
				struct ether_header *eth_hdr = (struct ether_header*)(p_pkt->packet);
				memcpy(eth_hdr->ether_dhost, mac, ETH_ALEN);
				iface_send_packet(p->iface, p_pkt->packet, p_pkt->len);
				list_delete_entry(&p_pkt->list);
				free(p_pkt);
			}
			list_delete_entry(&p->list);
			free(p);
		}
	}

	pthread_mutex_unlock(&arpcache.lock);
}

// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
    // the unreachable pending packets QAQ
	struct list_head unreachables;
	init_list_head(&unreachables);

	while (1) {
		sleep(1);

		pthread_mutex_lock(&arpcache.lock);

		time_t cur = time(NULL);
		for (int i = 0; i < MAX_ARP_SIZE; i += 1) {
			struct arp_cache_entry entry = arpcache.entries[i];
			if (entry.valid == 1 && cur - entry.added > ARP_ENTRY_TIMEOUT) {
				entry.valid = 0;
			}
		}

		struct arp_req *p, *q;
		list_for_each_entry_safe(p, q, &arpcache.req_list, list) {
			if (p->retries < ARP_REQUEST_MAX_RETRIES) {
				if (cur - p->sent >= 1) {
					// log(DEBUG, "retry sending ARP request for entry, ip4=%x\n", p->ip4);
					p->retries += 1;
					p->sent = cur;
					arp_send_request(p->iface, p->ip4);
				}
			} else {
				// add to unreachable list
				list_delete_entry(&p->list);
				list_add_tail(&p->list, &unreachables);
			}
		}

		pthread_mutex_unlock(&arpcache.lock);

		list_for_each_entry_safe(p, q, &unreachables, list) {
			struct cached_pkt *p_pkt, *q_pkt;
			list_for_each_entry_safe(p_pkt, q_pkt, &p->cached_packets, list) {
				icmp_send_packet(p_pkt->packet, p_pkt->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
				list_delete_entry(&p_pkt->list);
				free(p_pkt->packet);
				free(p_pkt);
			}
			list_delete_entry(&p->list);
			free(p);
		}
	}

	return NULL;
}
