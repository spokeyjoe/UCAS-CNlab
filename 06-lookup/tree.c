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


