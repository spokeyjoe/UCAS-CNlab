#include "base.h"
#include <stdio.h>

extern ustack_t *instance;

// the memory of ``packet'' will be free'd in handle_packet().
void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	// TODO: broadcast packet 
	iface_info_t *node;
	list_for_each_entry(node, instance -> list, list) {
		if (node != iface) {
			iface_send_packet(node, packet, len);
		}
	}
}
