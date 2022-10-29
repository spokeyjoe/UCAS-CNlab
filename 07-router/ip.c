#include "ip.h"
#include "icmp.h"
#include "rtable.h"
#include "arp.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip_hdr->daddr);
	// log(DEBUG, "DADDR: %d.%d.%d.%d", daddr>>24, (daddr<<8)>>24, (daddr<<16)>>24, (daddr<<24)>>24);
	u32 iface_ip = iface->ip;
	// log(DEBUG, "IFACE: %d.%d.%d.%d", iface_ip>>24, (iface_ip<<8)>>24, (iface_ip<<16)>>24, (iface_ip<<24)>>24);


	if (daddr == iface_ip) {
		if (ip_hdr->protocol == IPPROTO_ICMP) {
			// log(DEBUG, "Receive ICMP echo request!");
			struct icmphdr *icmp_hdr = (struct icmphdr *)(IP_DATA(ip_hdr));
			if (icmp_hdr->type == ICMP_ECHOREQUEST) {
				icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
			}
		} else {
			free(packet);
		}
	} else {
		rt_entry_t *entry = longest_prefix_match(daddr);
		if (entry == NULL) {
			icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
		} else {
			if (--ip_hdr->ttl == 0) {
				icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
			} else {
				u32 nxt_hop = entry->gw == 0 ? daddr : entry->gw;
				// log(DEBUG, "Next Hop: %d.%d.%d.%d\n", nxt_hop>>24, (nxt_hop<<8)>>24, (nxt_hop<<16)>>24, (nxt_hop<<24)>>24);
				ip_hdr->checksum = ip_checksum(ip_hdr);
				iface_send_packet_by_arp(entry->iface, nxt_hop, packet, len);
			}
		}
	}

	
}
