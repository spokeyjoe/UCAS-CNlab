#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <pthread.h>
#include <stdlib.h>
#include "https-server.h"



int main() {
    pthread_t pthrd;
    int https_port = HTTPS_PORT;
    int http_port = HTTP_PORT;

    while (1) {
        if (pthread_create(&pthrd, NULL, listen_on_port, (void *)&http_port) != 0) {
            perror("Fail to create thread");
            exit(1);
        }
        printf("Creating a new thread...\n");
        listen_on_port((void*)&https_port);
        pthread_detach(pthrd);
    }
}

/* Start a socket and listen on port PORT. */
void* listen_on_port(void* port_num) {
    // init SSL Library
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	// enable TLS method
	const SSL_METHOD *method = TLS_server_method();
	SSL_CTX *ctx = SSL_CTX_new(method);

	// load certificate and private key
	if (SSL_CTX_use_certificate_file(ctx, "./keys/cnlab.cert", SSL_FILETYPE_PEM) <= 0) {
		perror("load cert failed");
		exit(1);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, "./keys/cnlab.prikey", SSL_FILETYPE_PEM) <= 0) {
		perror("load prikey failed");
		exit(1);
	}

    int port = *((int*)port_num);

    // socket addr
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    struct sockaddr_in caddr;
    bzero(&caddr, sizeof(caddr));

    socklen_t socklen;
    
    // init socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Fail to open socket");
        exit(1);
    }
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("`setsockopt` failed");
        exit(1);
    }

    
    // bind & listen
    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Binding failed");
        exit(1);
    }
    printf("Binding succeed on port %d!\n", port);
    listen(sock, 128);
    printf("Listening on port %d...\n", port);
    while (1) {
        int csock = accept(sock, (struct sockaddr*)&caddr, &socklen);
        if (csock < 0) {
            printf("Error (port %d)", port);
            perror("Accept failed");
            exit(1);
        }
        printf("Connection accepted! (port %d)\n", port);
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, csock);
        handle_https_request(ssl, port);
    }
}

/* Read request from ssl descriptor, generate response and send. */
void handle_https_request(SSL* ssl, int port) {
    char request_buf[LEN_REQUEST_BUF];
    int response_len;
    int request_len;

    char method[20];
    char filepath[100];
    char version[20];

    FILE* fp = NULL;

    if (SSL_accept(ssl) == -1){
		perror("SSL_accept failed\n");
		exit(1);
	}
    else {
        printf("SSL_accept succeed! (port %d)\n", port);
        request_len = SSL_read(ssl, request_buf, LEN_REQUEST_BUF);
		if (request_len < 0) {
			perror("SSL_read failed\n");
			exit(1);
		}
        printf("Request receieved! (port %d)\n[DEBUG] Request length: %d\n", port, request_len);

        if (port == HTTP_PORT) {
            // handle request from 80
        }

        sscanf(request_buf, "%s /%s %s", method, filepath, version);

        // TODO: Parse other fields..

        printf("Getting file: %s\n", filepath);

        // 200 OK
        if ((fp = fopen(filepath, "r")) != NULL) {
            printf("[DEBUG] Got file!\n");
            int num_chars = count_file_chars(fp);
            char response_buf[LEN_RESPONSE_BUF_HEADER + num_chars];
            response_len = get_response(response_buf, OK, num_chars, NULL);
            fread(response_buf + response_len, sizeof(char), num_chars, fp);
            response_len += num_chars;
            for (int i = 0; i < response_len; i += 1) {
                printf("%c", response_buf[i]);
            }
            SSL_write(ssl, response_buf, response_len);
        } else {
            printf("[DEBUG] File not found!\n");
            char response_buf[LEN_RESPONSE_BUF_HEADER];
            response_len = get_response(response_buf, NOT_FOUND, 0, NULL);
            SSL_write(ssl, response_buf, response_len);
        }
    }
    int sock = SSL_get_fd(ssl);
    SSL_free(ssl);
    close(sock);
}

/* Count number of chars in the file FP. */
int count_file_chars(FILE* fp) {
    int cnt = 0;
    if (fp == NULL) {
        return -1;
    }
    fseek(fp, 0, SEEK_SET);
    while (fgetc(fp) != EOF) {
        cnt += 1;
    }
    fseek(fp, 0, SEEK_SET);
    return cnt;
}

/* Generate response according to status code CODE 
   and length of content LENGTH. 
   
   For request from 80 port, we generate new https url from HTTP_URL. */
int get_response(char response_buf[], int code, int length, char* http_url) {
    int len = 0;
    len += sprintf(response_buf, "HTTP/1.1 %d %s\r\n", code, code2message(code));
    if (code == 200) {
        len += sprintf(response_buf + len, "Content-length: %d\r\n", length);
    } else if (code == 301) {
        char new_url[100];
        strcpy(new_url, "https:");
        len += sprintf(response_buf + len, "Location: %s\r\n", strcat(new_url, http_url));
    }
    return len;
}