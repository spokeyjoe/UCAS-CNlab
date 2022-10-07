#include "tree.h"


/* Look up IP in trie T, return port number. */
int trie_lookup(trie *t, uint32_t ip) {
    trie *ptr = t;
    int port_candidate = -1;
    for (int i = 0; i < 32; i += 1) {
        int bit = get_bit(ip, i);
        ptr = (bit == 1) ? ptr->one_child : ptr->zero_child;

        // end of match
        if (ptr == NULL) {
            break;
        }

        // check if current node stores valid port number
        if (ptr->valid == 1) {
            port_candidate = ptr->port;
        }
    }
    return port_candidate;
}

/* Add IP/PREFIX/PORT into trie T. */
void trie_insert(trie *t, uint32_t ip, int prefix_len, int port) {
    trie *ptr = t;
    for (int i = 0; i < prefix_len; i += 1) {
        int bit = get_bit(ip, i);
        trie *nxt = (bit == 1) ? ptr->one_child : ptr->zero_child;

        // add new node
        if (nxt == NULL) {
            nxt = (trie *)malloc(sizeof(trie));
            nxt->port = 0;
            nxt->valid = 0;         // intermediate nodes has invalid port number by default
            nxt->zero_child = NULL;
            nxt->one_child = NULL;
            if (bit == 1) {
                ptr->one_child = nxt;
            } else {
                ptr->zero_child = nxt;
            }
            ptr = nxt;

            // last bit: add entry
            if (i == prefix_len - 1) {
                ptr->port = port;
                ptr->valid = 1;
            }
        } else {      // keep searching
            ptr = nxt;

            if (i == prefix_len - 1) {
                if (ptr->valid == 1) {
                    printf("ERROR: entry already exists.\n");
                    return;
                } else {
                    ptr->valid = 1;
                    ptr->port = port;
                }
            }
        }
    }
}

