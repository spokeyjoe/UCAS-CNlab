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

/* Look up IP in advanced trie T, return port number. */
int ad_trie_lookup(ad_trie_head *t, uint32_t ip) {
    ad_trie *ptr = t->children[get_6bit(ip)];
    if (ptr == NULL) {
        return -1;
    }
    int port_candidate = -1;
    ad_trie *odd_ptr = NULL;
    ad_trie *even_ptr = NULL;

    // scan from 10-31 bit (5-15 2bits)
    for (int i = 3; i < 16; i += 1) {
        int bit = get_bit(ip, 2*i);
        odd_ptr = ptr->odd_children[bit];
        if (odd_ptr != NULL) {
            port_candidate = odd_ptr->port;
        }

        int bits = get_2bit(ip, i);
        even_ptr = ptr->children[bits];
        if (even_ptr != NULL) {
            if (even_ptr->valid == 1) {
                port_candidate = even_ptr->port;
            }
            ptr = even_ptr;
        } else {
            break;
        }
    }
    return port_candidate;
}

/* Add IP/PREFIX/PORT into advanced trie T. */
void ad_trie_insert(ad_trie_head *t, uint32_t ip, int prefix_len, int port) {
    int bits_6 = get_6bit(ip);
    ad_trie *nxt = t->children[bits_6];
    if (nxt == NULL) {
        nxt = (ad_trie *)malloc(sizeof(ad_trie));
        for (int i = 0; i < 4; i += 1) {
            nxt->children[i] = NULL;
        }
        nxt->odd_children[0] = NULL;
        nxt->odd_children[1] = NULL;
        nxt->port = 0;
        nxt->valid = 0;
        t->children[bits_6] = nxt;
    }
    ad_trie *ptr = nxt;
    for (int i = 3; i < 3 + (prefix_len - 6) / 2; i += 1) {
        int bits = get_2bit(ip, i);
        nxt = ptr->children[bits];

        // add new node
        if (nxt == NULL) {
            nxt = (ad_trie *)malloc(sizeof(ad_trie));
            for (int i = 0; i < 4; i += 1) {
                nxt->children[i] = NULL;
            }
            nxt->odd_children[0] = NULL;
            nxt->odd_children[1] = NULL;
            nxt->port = 0;
            nxt->valid = 0;
            ptr->children[bits] = nxt;
            ptr = nxt;

            // even-length prefix, last 2-bits
            if ((i == (2 + (prefix_len - 6) / 2)) && (prefix_len % 2 == 0)) {
                ptr->port = port;
                ptr->valid = 1;
            }
        } else {
            ptr = nxt;
            if ((i == (2 + (prefix_len - 6) / 2)) && (prefix_len % 2 == 0)) {
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
    // prefix_len is odd
    if ((prefix_len - 6) % 2 == 1) {
        int last_bit = get_bit(ip, prefix_len - 1);
        nxt = ptr->odd_children[last_bit];
        // add new node
        if (nxt == NULL) {
            nxt = (ad_trie *)malloc(sizeof(ad_trie));
            for (int i = 0; i < 4; i += 1) {
                nxt->children[i] = NULL;
            }
            nxt->odd_children[0] = NULL;
            nxt->odd_children[1] = NULL;
            nxt->port = port;
            nxt->valid = 1;
            ptr->odd_children[last_bit] = nxt;
            ptr = nxt;
        }
    }
}