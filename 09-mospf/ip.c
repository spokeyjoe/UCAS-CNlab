#include "ip.h"
#include "icmp.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"

#include "mospf_proto.h"
#include "mospf_daemon.h"

#include "log.h"
#include "ether.h"

#include <stdlib.h>
#include <assert.h>

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	if (daddr == iface->ip) {
		if (ip->protocol == IPPROTO_ICMP) {
			struct icmphdr *icmp = (struct icmphdr *)IP_DATA(ip);
			if (icmp->type == ICMP_ECHOREQUEST) {
				log(DEBUG, "sending ICMP reply");
				icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
			}
		} else if (ip->protocol == IPPROTO_MOSPF) {
			handle_mospf_packet(iface, packet, len);
		}

		free(packet);
	} else if (ip->daddr == htonl(MOSPF_ALLSPFRouters)) {
		assert(ip->protocol == IPPROTO_MOSPF);
		handle_mospf_packet(iface, packet, len);

		free(packet);
	} else {
		ip_forward_packet(daddr, packet, len);
	}

}

void ip_forward_packet(u32 dst, char *packet, int len) {
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	logaddr("DADDR: ", dst);

	rt_entry_t *entry = longest_prefix_match(dst);
	if (entry == NULL) {
		log(DEBUG, "no match in routing table");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
	} else {
		if (--ip_hdr->ttl == 0) {
			icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
		} else {
			u32 nxt_hop = entry->gw == 0 ? dst : entry->gw;
			log(DEBUG, "Next Hop: %d.%d.%d.%d\n", nxt_hop>>24, (nxt_hop<<8)>>24, (nxt_hop<<16)>>24, (nxt_hop<<24)>>24);
			ip_hdr->checksum = ip_checksum(ip_hdr);
			iface_send_packet_by_arp(entry->iface, nxt_hop, packet, len);
		}
	}
}