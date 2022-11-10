#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"
#include "rtable.h"

#include "ip.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
    pthread_mutex_init(&mospf_lock, NULL);

    instance->area_id = 0;
    // get the ip address of the first interface
    iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
    instance->router_id = iface->ip;
    instance->sequence_num = 0;
    instance->lsuint = MOSPF_DEFAULT_LSUINT;

    iface = NULL;
    list_for_each_entry(iface, &instance->iface_list, list) {
        iface->helloint = MOSPF_DEFAULT_HELLOINT;
        init_list_head(&iface->nbr_list);
    }

    init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);

void mospf_run()
{
    pthread_t hello, lsu, nbr, db;
    pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
    pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
    pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
    pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
    while (1) {
        // log(DEBUG, "Sending mOSPF Hello...");
        pthread_mutex_lock(&mospf_lock);
        iface_info_t *p;
        list_for_each_entry(p, &instance->iface_list, list) {
            int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
            char *packet_mospf_hello = malloc(len);
            memset(packet_mospf_hello, 0, len);

            struct ether_header *ehdr = (struct ether_header *)packet_mospf_hello;
            struct iphdr *ihdr = (struct iphdr *)((char *)ehdr + ETHER_HDR_SIZE);
            struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_BASE_HDR_SIZE);
            struct mospf_hello *mhello = (struct mospf_hello *)((char *)mhdr + MOSPF_HDR_SIZE);

            ehdr->ether_type = htons(ETH_P_IP);
            memcpy(ehdr->ether_shost, p->mac, ETH_ALEN);
            ehdr->ether_dhost[0] = 0x01;
            ehdr->ether_dhost[1] = 0x00;
            ehdr->ether_dhost[2] = 0x5e;
            ehdr->ether_dhost[3] = 0x00;
            ehdr->ether_dhost[4] = 0x00;
            ehdr->ether_dhost[5] = 0x05;

            ip_init_hdr(ihdr, p->ip, MOSPF_ALLSPFRouters, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
            ihdr->checksum = ip_checksum(ihdr);

            mospf_init_hdr(mhdr, MOSPF_TYPE_HELLO, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, instance->router_id, 0);
            mospf_init_hello(mhello, p->mask);
            mhdr->checksum = mospf_checksum(mhdr);

            iface_send_packet(p, packet_mospf_hello, len);
        }
        pthread_mutex_unlock(&mospf_lock);
        sleep(MOSPF_DEFAULT_HELLOINT);
    }
}

void *checking_nbr_thread(void *param)
{
    while (1) {
        pthread_mutex_lock(&mospf_lock);

        iface_info_t *p;
        mospf_nbr_t *n, *m;    // neighbor entry
        list_for_each_entry(p, &instance->iface_list, list) {
            list_for_each_entry_safe(n, m, &p->nbr_list, list) {
                n->alive += 1;
                if (n->alive >= 3 * p->helloint) {
                    list_delete_entry(&n->list);
                    free(n);
                    p->num_nbr -= 1;
                    send_mospf_lsu();
                }

            }
        }

        pthread_mutex_unlock(&mospf_lock);
        sleep(1);
    }
}

void *checking_database_thread(void *param)
{
    while (1) {
        pthread_mutex_lock(&mospf_lock);

        mospf_db_entry_t *p, *q;
        list_for_each_entry_safe(p, q, &mospf_db, list) {
            p->alive += 1;
            if (p->alive > MOSPF_DATABASE_TIMEOUT) {
                list_delete_entry(&p->list);
                free(p);
            }
        }
        
		// print_mospf_db();
		calculate_rtable();
		print_rtable();

        pthread_mutex_unlock(&mospf_lock);
        sleep(1);
    }
}

/* Calculate rtable using mospf database. */
void calculate_rtable()
{
    int num_nodes = 1;
    mospf_db_entry_t *n;
    list_for_each_entry(n, &mospf_db, list) {
        num_nodes += 1;
    }

    // create rid table
    int rid_table[num_nodes];
    rid_table[0] = instance->router_id;
    int p = 0;
    list_for_each_entry(n, &mospf_db, list) {
        p += 1;
        rid_table[p] = n->rid;
    }
    
    // initialize graph
    int graph[num_nodes][num_nodes];
    memset(graph, 0, sizeof(graph));
    list_for_each_entry(n, &mospf_db, list) {
        for (int i = 0; i < n->nadv; i += 1) {
			if (n->array[i].rid != 0) {
				int index_n = rid2index(n->rid, rid_table, num_nodes);
				int index_m = rid2index(n->array[i].rid, rid_table, num_nodes);
				// if node is not in the database, ignore it
				if (index_n == -1 || index_m == -1) {
					// log(DEBUG, "rid not found");
					break;
				}
            	graph[index_n][index_m] = 1;
				graph[index_m][index_n] = 1;
			}
        }
    }
    iface_info_t *iface;
    list_for_each_entry(iface, &instance->iface_list, list) {
        mospf_nbr_t *nbr;
        list_for_each_entry(nbr, &iface->nbr_list, list) {
            graph[0][rid2index(nbr->nbr_id, rid_table, num_nodes)] = 1;
        }
    }

	// dijkstra
    int dist[num_nodes];
    int visited[num_nodes];
    int prev[num_nodes];
    dijkstra(0, num_nodes, visited, dist, prev, graph);

	// update router table
	clear_rtable();
	load_rtable_from_kernel();

	list_for_each_entry(n, &mospf_db, list) {
		for (int i = 0; i < n->nadv; i += 1) {
			struct mospf_lsa *nbr = &n->array[i];
			u32 network = nbr->network;
			u32 mask = nbr->mask;

			// lookup routing table
			rt_entry_t *r;
			int entry_exist = 0;
			list_for_each_entry(r, &rtable, list) {
				if ((r->dest & r->mask) == (network & mask)) {
					entry_exist = 1;
					break;
				}
			}
			if (entry_exist == 1) {
				continue;
			}

			// add routing entry to this network
			int prev_idx = rid2index(nbr->rid == 0? n->rid : nbr->rid, rid_table, num_nodes);
			if (prev_idx == -1) {
				continue;
			}
			while(dist[prev_idx] > 1 && prev[prev_idx] != -1) {
				prev_idx = prev[prev_idx];
			}

			u32 nxt_hop = rid_table[prev_idx];

			// find port to next hop
			int found = 0;
			mospf_nbr_t *nb;
			list_for_each_entry(iface, &instance->iface_list, list) {
				list_for_each_entry(nb, &iface->nbr_list, list) {
					if (nb->nbr_id == nxt_hop) {
						found = 1;
						break;
					}
				}
				if (found) {
					break;
				}
			}
			if (found == 0) {
				continue;
			}

			rt_entry_t *new_entry = new_rt_entry(network, mask, nb->nbr_ip, iface);
			add_rt_entry(new_entry);
		}
	}
}

void dijkstra(int src, int n, int *visited, int *dist, int *prev, int graph[n][n]) {
    for (int i = 0; i < n; i += 1) {
        visited[i] = 0;
        dist[i] = INT32_MAX;
        prev[i] = -1;
    }
    dist[src] = 0;

    for (int i = 0; i < n; i += 1) {
        int closest;
        int min_dist = INT32_MAX;
        for (int j = 0; j < n; j += 1) {
            if (visited[j] == 0 && dist[j] < min_dist) {
                min_dist = dist[j];
                closest = j;
            }
        }

        visited[closest] = 1;
        for (int k = 0; k < n; k += 1) {
            if (visited[k] == 0 && graph[closest][k] > 0 && dist[closest] + graph[closest][k] < dist[k]) {
                dist[k] = dist[closest] + graph[closest][k];
                prev[k] = closest;
            }
        }
    }
}

int rid2index(u32 rid, int *rid_table, int num_nodes) {
    for (int i = 0; i < num_nodes; i += 1) {
        if (rid_table[i] == rid) {
            return i;
        }
    }
	return -1;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
    // log(DEBUG, "Receive mOSPF Hello!");
    struct iphdr       *ihdr   = packet_to_ip_hdr(packet);
    struct mospf_hdr   *mhdr   = (struct mospf_hdr   *)((char *)ihdr + IP_HDR_SIZE(ihdr));
    struct mospf_hello *mhello = (struct mospf_hello *)((char *)mhdr + MOSPF_HDR_SIZE);
    u32 rid  = ntohl(mhdr->rid);
    u32 ip   = ntohl(ihdr->saddr);
    u32 mask = ntohl(mhello->mask);

    pthread_mutex_lock(&mospf_lock);
    
    mospf_nbr_t *n;
    list_for_each_entry(n, &iface->nbr_list, list) {
        if (n->nbr_id == rid) {
            n->alive = 0;
            pthread_mutex_unlock(&mospf_lock);
            return;
        }
    }
    
    mospf_nbr_t *new_entry = malloc(sizeof(mospf_nbr_t));
    new_entry->nbr_id = rid;
    new_entry->nbr_ip = ip;
    new_entry->nbr_mask = mask;
    list_add_tail(&new_entry->list, &iface->nbr_list);
    iface->num_nbr += 1;

    send_mospf_lsu();

    pthread_mutex_unlock(&mospf_lock);
}

void *sending_mospf_lsu_thread(void *param)
{
    while (1) {
        pthread_mutex_lock(&mospf_lock);
        send_mospf_lsu();
        pthread_mutex_unlock(&mospf_lock);
        sleep(instance->lsuint);
    }
}

void send_mospf_lsu() {
    // collect neighbor list
    int nadv = 0;
    iface_info_t *p;
    list_for_each_entry(p, &instance->iface_list, list) {
        nadv += p->num_nbr == 0 ? 1 : p->num_nbr;
    }
    struct mospf_lsa *lsa = malloc(nadv * MOSPF_LSA_SIZE);
    int i = 0;
    list_for_each_entry(p, &instance->iface_list, list) {
        if (p->num_nbr == 0) {
            lsa[i].rid = htonl(0);
            lsa[i].mask = htonl(p->mask);
            lsa[i].network = htonl(p->ip);
            i += 1;
        } else {
            mospf_nbr_t *n;
            list_for_each_entry(n, &p->nbr_list, list) {
                lsa[i].rid = htonl(n->nbr_id);
                lsa[i].mask = htonl(n->nbr_mask);
                lsa[i].network = htonl(n->nbr_ip);
                i += 1;
            }
        }
    }

    int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + MOSPF_LSA_SIZE * nadv;
    list_for_each_entry(p, &instance->iface_list, list) {
        if (p->num_nbr != 0) {
            mospf_nbr_t *n;
            list_for_each_entry(n, &p->nbr_list, list) {
                char *packet_lsu = malloc(len);
                struct iphdr *ihdr = packet_to_ip_hdr(packet_lsu);
                struct mospf_hdr *mhdr = (struct mospf_hdr*)((char *)ihdr + IP_BASE_HDR_SIZE);
                struct mospf_lsu *mlsu = (struct mospf_lsu*)((char *)mhdr + MOSPF_HDR_SIZE);
                struct mospf_lsa *mlsa = (struct mospf_lsa*)((char *)mlsu + MOSPF_LSU_SIZE);

                memcpy(mlsa, lsa, nadv * MOSPF_LSA_SIZE);

                mospf_init_lsu(mlsu, nadv);

                mospf_init_hdr(mhdr, MOSPF_TYPE_LSU, len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE, instance->router_id, 0);
                mhdr->checksum = mospf_checksum(mhdr);

                ip_init_hdr(ihdr, p->ip, n->nbr_ip, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
                ihdr->checksum = ip_checksum(ihdr);

                ip_send_packet(packet_lsu, len);
            }
        }
    }

    free(lsa);
    instance->sequence_num += 1;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
    // log(DEBUG, "Receive mOSPF LSU!");
    struct iphdr *ihdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
    struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_HDR_SIZE(ihdr));
    struct mospf_lsu *mlsu = (struct mospf_lsu *)((char *)mhdr + MOSPF_HDR_SIZE);
    struct mospf_lsa *mlsa = (struct mospf_lsa *)((char *)mlsu + MOSPF_LSU_SIZE);

    u32 rid = ntohl(mhdr->rid);
    u16 seq = ntohs(mlsu->seq);
    u32 nadv = ntohl(mlsu->nadv);

    pthread_mutex_lock(&mospf_lock);

    // check if copy exists
    mospf_db_entry_t *p, *entry_to_update = NULL;
    list_for_each_entry(p, &mospf_db, list) {
        if (p->rid == rid) {
            if (p->seq >= seq) {
                pthread_mutex_unlock(&mospf_lock);
                return;
            } else {
                entry_to_update = p;
                break;
            }
        }
    }

    // add or update
    if (entry_to_update == NULL) {
        mospf_db_entry_t *new_entry = malloc(sizeof(mospf_db_entry_t));
        new_entry->rid = rid;
        new_entry->alive = 0;
        new_entry->seq = seq;
        new_entry->nadv = nadv;
        new_entry->array = malloc(nadv * MOSPF_LSA_SIZE);
        for (int i = 0; i < nadv; ++i) {
            new_entry->array[i].rid = ntohl(mlsa[i].rid);
            new_entry->array[i].mask = ntohl(mlsa[i].mask);
            new_entry->array[i].network = ntohl(mlsa[i].network);
        }
        list_add_tail(&new_entry->list, &mospf_db);
    } else {
        entry_to_update->alive = 0;
        entry_to_update->seq = seq;
        entry_to_update->nadv = nadv;
        entry_to_update->array = malloc(nadv * MOSPF_LSA_SIZE);
        for (int i = 0; i < nadv; ++i) {
            entry_to_update->array[i].rid = ntohl(mlsa[i].rid);
            entry_to_update->array[i].mask = ntohl(mlsa[i].mask);
            entry_to_update->array[i].network = ntohl(mlsa[i].network);
        }
    }

    // forward
    mlsu->ttl -= 1;
    if (mlsu->ttl > 0) {
        mhdr->checksum = mospf_checksum(mhdr);
        iface_info_t *p;
        list_for_each_entry(p, &instance->iface_list, list) {
            if (p->index != iface->index && p->num_nbr != 0) {
                mospf_nbr_t *n;
                list_for_each_entry(n, &p->nbr_list, list) {
                    char *forward_packet = malloc(len);
                    memcpy(forward_packet, packet, len);
                    struct iphdr *new_ihdr = packet_to_ip_hdr(forward_packet);
                    new_ihdr->saddr = htonl(p->ip);
                    new_ihdr->daddr = htonl(n->nbr_ip);
                    new_ihdr->checksum = ip_checksum(new_ihdr);
                    ip_send_packet(forward_packet, len);
                }
            }
        }
    }

    pthread_mutex_unlock(&mospf_lock);
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
    // log(DEBUG, "Receive mOSPF packet!");
    struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
    
    struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

    if (mospf->version != MOSPF_VERSION) {
        log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
        return ;
    }
    if (mospf->checksum != mospf_checksum(mospf)) {
        log(ERROR, "received mospf packet with incorrect checksum");
        return ;
    }
    if (ntohl(mospf->aid) != instance->area_id) {
        log(ERROR, "received mospf packet with incorrect area id");
        return ;
    }

    switch (mospf->type) {
        case MOSPF_TYPE_HELLO:
            handle_mospf_hello(iface, packet, len);
            break;
        case MOSPF_TYPE_LSU:
            handle_mospf_lsu(iface, packet, len);
            break;
        default:
            log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
            break;
    }
}
