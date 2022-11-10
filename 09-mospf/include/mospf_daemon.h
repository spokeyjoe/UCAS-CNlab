#ifndef __MOSPF_DAEMON_H__
#define __MOSPF_DAEMON_H__

#include "base.h"
#include "types.h"
#include "list.h"

void mospf_init();
void mospf_run();
void handle_mospf_packet(iface_info_t *iface, char *packet, int len);
void send_mospf_lsu();
void calculate_rtable();
void dijkstra(int src, int n, int *visited, int *dist, int *prev, int graph[n][n]);
int rid2index(u32 rid, int *rid_table, int num_nodes);

#endif
