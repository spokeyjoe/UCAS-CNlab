#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

trie t;
ad_trie_head ad_t;
super_trie_head super_t;

// return an array of ip represented by an unsigned integer, size is TEST_SIZE
uint32_t* read_test_data(const char* lookup_file){
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    uint32_t *ip_array= (uint32_t *)malloc(TEST_SIZE * sizeof(uint32_t));

    fp = fopen(lookup_file, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    int i = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        // split line into 4 integer
        int addr1 = atoi(strtok(line, "."));
        int addr2 = atoi(strtok(NULL, "."));
        int addr3 = atoi(strtok(NULL, "."));
        int addr4 = atoi(strtok(NULL, "."));

        // shift & catonate
        ip_array[i++] = (uint32_t)((addr1 << 24) | (addr2 << 16) | (addr3 << 8) | (addr4));
    }

    fclose(fp);
    if (line) {
        free(line);
    }
    return ip_array;
}

// Constructing an trie-tree to lookup according to `forward_file`
void create_tree(const char* forward_file){
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(forward_file, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    uint32_t ip;
    int prefix;
    int port;
    while ((read = getline(&line, &len, fp)) != -1) {
        // split line into IP/prefix/port
        char *ip_string = strtok(line, " ");
        prefix = atoi(strtok(NULL, " "));
        port = atoi(strtok(NULL, " "));

        // split ip string into 4 integer
        int addr1 = atoi(strtok(ip_string, "."));
        int addr2 = atoi(strtok(NULL, "."));
        int addr3 = atoi(strtok(NULL, "."));
        int addr4 = atoi(strtok(NULL, "."));

        // shift & catonate
        ip = (uint32_t)((addr1 << 24) | (addr2 << 16) | (addr3 << 8) | (addr4)); 
        trie_insert(&t, ip, prefix, port);
    }
}

// Look up the ports of ip in file `lookup_file` using the basic tree
uint32_t *lookup_tree(uint32_t* ip_vec){
    uint32_t *port_list = (uint32_t *)malloc(TEST_SIZE * sizeof(uint32_t));
    for (int i = 0; i < TEST_SIZE; i += 1) {
        port_list[i] = (uint32_t)trie_lookup(&t, ip_vec[i]);
    }
    return port_list;
}

// Constructing an advanced trie-tree to lookup according to `forwardingtable_filename`
void create_tree_advance(const char* forward_file){
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(forward_file, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    uint32_t ip;
    int prefix;
    int port;
    while ((read = getline(&line, &len, fp)) != -1) {
        // split line into IP/prefix/port
        char *ip_string = strtok(line, " ");
        prefix = atoi(strtok(NULL, " "));
        port = atoi(strtok(NULL, " "));

        // split ip string into 4 integer
        int addr1 = atoi(strtok(ip_string, "."));
        int addr2 = atoi(strtok(NULL, "."));
        int addr3 = atoi(strtok(NULL, "."));
        int addr4 = atoi(strtok(NULL, "."));

        // shift & catonate
        ip = (uint32_t)((addr1 << 24) | (addr2 << 16) | (addr3 << 8) | (addr4)); 
        super_trie_insert(&super_t, ip, prefix, port);
        // ad_trie_insert(&ad_t, ip, prefix, port);
    }
}

// Look up the ports of ip in file `lookup_file` using the advanced tree
uint32_t *lookup_tree_advance(uint32_t* ip_vec){
    uint32_t *port_list = (uint32_t *)malloc(TEST_SIZE * sizeof(uint32_t));
    for (int i = 0; i < TEST_SIZE; i += 1) {
        port_list[i] = (uint32_t)super_trie_lookup(&super_t, ip_vec[i]);
        //port_list[i] = (uint32_t)ad_trie_lookup(&ad_t, ip_vec[i]);
    }
    return port_list;
}


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

/* Look up IP in super trie T, return port number. */
int super_trie_lookup(super_trie_head *t, uint32_t ip) {
    super_trie *ptr = t->children[get_8bit(ip)];
    if (ptr == NULL) {
        return -1;
    }
    int port_candidate = ptr->port;

    // scan from 8-31 bit (2-7 4-bit chunk)
    for (int i = 2; i < 8; i += 1) {
        int bits4 = get_4bit(ip, i);
        ptr = ptr->children[bits4];

        if (ptr == NULL) {
            break;
        }
        if (ptr->port != -1) {
            port_candidate = ptr->port;
        }
    }
    return port_candidate;
}

/* Add IP/PREFIX/PORT into super trie T. */
void super_trie_insert(super_trie_head *t, uint32_t ip, int prefix_len, int port) {
    super_trie *nxt = t->children[get_8bit(ip)];
    if (nxt == NULL) {
        nxt = (super_trie *)malloc(sizeof(super_trie));
        for (int i = 0; i < 16; i += 1) {
            nxt->children[i] = NULL;
        }
        nxt->port = prefix_len == 8 ? port : -1;
        t->children[get_8bit(ip)] = nxt;
    }
    super_trie *ptr = nxt;
    // scan 2-7 4-bit chunk
    for (int i = 2; i < prefix_len / 4; i += 1) {
        int bits4 = get_4bit(ip, i);
        nxt = ptr->children[bits4];
        if (nxt == NULL) {
            nxt = (super_trie *)malloc(sizeof(super_trie));
            for (int i = 0; i < 16; i += 1) {
                nxt->children[i] = NULL;
            }
            nxt->port = -1;
            ptr->children[bits4] = nxt;
        }
        // store/overwrite port number anyway
        if ((i == prefix_len / 4 - 1) && (prefix_len % 4 == 0)) {
            nxt->port = port;
            return;
        }
        ptr = nxt;
            
    }
    // remaining 1-3 bits: add for all possible children nodes
    int last_4bits = get_4bit(ip, prefix_len / 4);
    // length of invalid bits in last_4bits
    int tail_len = 4 - prefix_len % 4;
    int index_children_l = (last_4bits >> tail_len) << tail_len;
    int index_children_r = index_children_l + (1 << tail_len) - 1;
    for (int i = index_children_l; i < index_children_r + 1; i += 1) {
        nxt = ptr->children[i];
        if (nxt == NULL) {
            nxt = (super_trie *)malloc(sizeof(super_trie));
            ptr->children[i] = nxt;
        }
        nxt->port = port;
    }
}