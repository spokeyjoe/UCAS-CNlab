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
    int port;                        // port number
    int valid;                       // 1 -> node stores valid port number
    struct trie *zero_child;         // next bit 0 -> zero_child
    struct trie *one_child;          // next bit 1 -> one_child
} trie;

/* Advanced trie node */
typedef struct ad_trie {
    int port;
    int valid;
    struct ad_trie *children[4];
    struct ad_trie *odd_children[2];
} ad_trie;


/* Super trie node */
typedef struct super_trie {
    int port;
    int valid;
    struct super_trie *children[16];
} super_trie;

/* Advanced trie head */
typedef struct ad_trie_head {
    struct ad_trie *children[128];     // first 6-bit for fast lookup
} ad_trie_head;

/* Super trie head */
typedef struct super_trie_head {
    struct super_trie *children[256];  // first 8-bit for fast lookup
} super_trie_head;


void create_tree(const char*);
uint32_t *lookup_tree(uint32_t *);
void create_tree_advance();
uint32_t *lookup_tree_advance(uint32_t *);

uint32_t* read_test_data(const char* lookup_file);

int trie_lookup(trie *t, uint32_t ip);
void trie_insert(trie *t, uint32_t ip, int prefix_len, int port);
int ad_trie_lookup(ad_trie_head *t, uint32_t ip);
void ad_trie_insert(ad_trie_head *t, uint32_t ip, int prefix_len, int port);
void super_trie_insert(super_trie_head *t, uint32_t ip, int prefix_len, int port);
int super_trie_lookup(super_trie_head *t, uint32_t ip);


#endif
