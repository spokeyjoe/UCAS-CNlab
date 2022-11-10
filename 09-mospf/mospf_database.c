#include "mospf_database.h"
#include "ip.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

struct list_head mospf_db;

void init_mospf_db()
{
	init_list_head(&mospf_db);
}

void print_mospf_db() {
    mospf_db_entry_t *ptr;
    printf("----------------------------------------\n"
           "mOSPF database entries:\n"
           "RID\tNetwork\tMask\tNeighbor\n"
           "----------------------------------------\n");
    list_for_each_entry(ptr, &mospf_db, list) {
        for (int i = 0; i < ptr->nadv; ++i) {
            printf(IP_FMT"\t"IP_FMT"\t"IP_FMT"\t"IP_FMT"\n",
                HOST_IP_FMT_STR(ptr->rid),
                HOST_IP_FMT_STR(ptr->array[i].network),
                HOST_IP_FMT_STR(ptr->array[i].mask),
                HOST_IP_FMT_STR(ptr->array[i].rid));
        }
    }
    printf("----------------------------------------\n");
}