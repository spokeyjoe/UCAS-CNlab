#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	struct iphdr *in_ip_hdr = packet_to_ip_hdr(in_pkt);
	char *out_pkt;
	int out_len = 0;
	struct iphdr *out_ip_hdr;
	struct icmphdr *out_icmp_hdr;

	if (type == ICMP_ECHOREPLY) {
		out_len = len - IP_HDR_SIZE(in_ip_hdr) + IP_BASE_HDR_SIZE;
	} else {
		out_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE
		        + IP_HDR_SIZE(in_ip_hdr) + ICMP_COPIED_DATA_LEN;
	}

	out_pkt = malloc(out_len);
	out_ip_hdr = packet_to_ip_hdr(out_pkt);
	ip_init_hdr(out_ip_hdr, 0, ntohl(in_ip_hdr->saddr), out_len - ETHER_HDR_SIZE, IPPROTO_ICMP);
	out_icmp_hdr = (struct icmphdr*)(IP_DATA(out_ip_hdr));
    out_icmp_hdr->type = type;
	out_icmp_hdr->code = code;

	char *out_body = (char *)out_icmp_hdr + ICMP_HDR_SIZE;
	if (type == ICMP_ECHOREPLY) {
		int offset = ETHER_HDR_SIZE + IP_HDR_SIZE(in_ip_hdr) + 4;
		memcpy(out_body - 4, in_pkt + offset, len - offset);
	} else {
		memset(&out_icmp_hdr->u, 0, 4);
		memcpy(out_body, in_ip_hdr, IP_HDR_SIZE(in_ip_hdr) + ICMP_COPIED_DATA_LEN);
	}

	out_icmp_hdr->checksum = icmp_checksum(out_icmp_hdr, out_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);

	ip_send_packet(out_pkt, out_len);
}
