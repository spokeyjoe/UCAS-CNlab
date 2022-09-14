#include "base.h"
#include <stdio.h>

// XXX ifaces are stored in instace->iface_list
extern ustack_t *instance;

extern void iface_send_packet(iface_info_t *iface, const char *packet, int len);

void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	iface_info_t *node = NULL;
	list_for_each_entry(node, &instance->iface_list, list) {
		if (node->index != iface->index) {
			iface_send_packet(node, packet, len);
		}
	}
}
