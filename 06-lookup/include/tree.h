#ifndef __TREE_H__
#define __TREE_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

// do not change it
#define TEST_SIZE 100000

/* Trie node */
typedef struct trie {
    // uint32_t ip_prefix;       // IP prefix
    // int prefix_len;           // same as level number
    int port;
    trie *zero_child;         // next bit 0 -> zero_child
    trie *one_child;          // next bit 1 -> one_child
} trie;

/* Look up IP in trie T, return port number. */
int trie_lookup(trie *t, uint32_t ip) {
    trie *ptr = t;
    for (int i = 0; i < 32; i += 1) {
        int bit = get_bit(ip, i);
        trie *nxt = (bit == 1) ? t->one_child : t->zero_child;
        if (nxt == NULL) { // end of match 
            return ptr->port ? ptr->port : NULL;
        } else {
            ptr = nxt;
        }
    }
}

/* Add IP/PREFIX/PORT into trie T. */
void trie_insert(trie *t, uint32_t ip, int prefix_len, int port) {
    trie *ptr = t;
    for (int i = 0; i < prefix_len; i += 1) {
        int bit = get_bit(ip, i);
        trie *nxt = (bit == 1) ? t->one_child : t->zero_child;
        if (nxt == NULL) {
            nxt = (trie *)malloc(sizeof(trie));
            nxt->port = NULL;
            nxt->zero_child = NULL;
            nxt->one_child = NULL;
            if (bit == 1) {
                t->one_child = nxt;
            } else {
                t->zero_child = nxt;
            }
            ptr = nxt;

            // last bit: add entry
            if (i == prefix_len) {
                ptr->port = port;
            }
        } else {
            ptr = nxt;
            printf("DEBUG: entry already exists\n");
            if (ptr->port != port) {
                printf("Port being inserted is not consistent with entry already in table\n");
                return;
            }
        }
    }
}




void create_tree(const char*);
uint32_t *lookup_tree(uint32_t *);
void create_tree_advance();
uint32_t *lookup_tree_advance(uint32_t *);

uint32_t* read_test_data(const char* lookup_file);

#endif
