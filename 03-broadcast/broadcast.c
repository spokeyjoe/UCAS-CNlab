#include "base.h"
#include <stdio.h>

extern ustack_t *instance;

// the memory of ``packet'' will be free'd in handle_packet().
void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	iface_info_t *ptr;
	list_for_each_entry(ptr, &instance->iface_list, list) {
		if (ptr != iface) {
			iface_send_packet(ptr, packet, len);
		}
	}
}
