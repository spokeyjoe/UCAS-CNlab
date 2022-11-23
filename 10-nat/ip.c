#include "ip.h"
#include "icmp.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"
#include "nat.h"
#include "log.h"

#include <stdlib.h>

void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	if (daddr == iface->ip && ip->protocol == IPPROTO_ICMP) {
		struct icmphdr *icmp = (struct icmphdr *)IP_DATA(ip);
		if (icmp->type == ICMP_ECHOREQUEST) {
			icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		}

		free(packet);
	}
	else {
		nat_translate_packet(iface, packet, len);
	}
}

void ip_forward_packet(u32 dst, char *packet, int len) {
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	// logaddr("DADDR: ", dst);

	rt_entry_t *entry = longest_prefix_match(dst);
	if (entry == NULL) {
		log(DEBUG, "no match in routing table");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
	} else {
		if (--ip_hdr->ttl == 0) {
			icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
		} else {
			u32 nxt_hop = entry->gw == 0 ? dst : entry->gw;
			ip_hdr->checksum = ip_checksum(ip_hdr);
			iface_send_packet_by_arp(entry->iface, nxt_hop, packet, len);
		}
	}
}