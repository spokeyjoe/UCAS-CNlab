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
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "https-server.h"


int main() {
    pthread_t pthrd;

    // init SSL Library
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	// enable TLS method
	const SSL_METHOD *method = TLS_server_method();
	SSL_CTX *ctx = SSL_CTX_new(method);

	// load certificate and private key
	if (SSL_CTX_use_certificate_file(ctx, "./keys/cnlab.cert", SSL_FILETYPE_PEM) <= 0) {
		perror("Fail to load certificate");
		exit(1);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, "./keys/cnlab.prikey", SSL_FILETYPE_PEM) <= 0) {
		perror("Fail to load private key");
		exit(1);
	}

    while (1) {
        listen_on_port(HTTP_PORT);
        if (pthread_create(&pthrd, NULL, listen_on_port, HTTPS_PORT) != 0) {
            perror("Fail to create thread");
            exit(1);
        }
        pthread_detach(pthrd);
    }
}

/* Start a socket and listen on port PORT. */
void listen_on_port(int port) {
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

    // socket addr
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    // bind & listen
    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Binding failed");
        exit(1);
    }
    listen(sock, 128);
    printf("Listening on port %d...\n", port);
    while (1) {
        struct sockaddr_in caddr;
        socklen_t len;
        int csock = accept(sock, (struct sockaddr*)&caddr, &len);
        if (csock < 0) {
            perror("Accept failed");
            exit(1);
        }
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, csock);
        handle_https_request(ssl, port);
    }

    close(sock);
    SSL_CTX_free(ctx);

    return 0;
}

void handle_https_request(SSL* ssl, int port) {
    char request_buf[LEN_REQUEST_BUF] = {0};
    char response_buf[LEN_RESPONSE_BUF] = {0};

    char method[20];
    char filename[100];

    int response_len;

    FILE* fp = NULL;

    if (SSL_accept(ssl) == -1){
		perror("SSL_accept failed");
		exit(1);
	} else {
        // read from SSL to request buffer
        int bytes = SSL_read(ssl, request_buf, sizeof(request_buf));
        if (bytes < 0) {
            perror("SSL_read failed");
            exit(1);
        }
        
        if (port == HTTP_PORT) {
            response_len = get_response(response_buf, MOVED_PERMANENTLY, 0, NULL);
            SSL_write(ssl, response_buf, strlen(response_buf));
        }

        // parse request
        char* method = strtok(request_buf, " ");
        char* filepath = strtok(NULL, " ");
        printf("Getting file: %s\n", filepath);

        // 200 OK
        if ((fp = fopen(filepath, "r")) != NULL) {
            num_chars = count_file_chars(fp);
            response_len = get_response(response_buf, OK, num_chars, NULL);
            fread(response_buf + response_len, sizeof(char), num_chars, fp);

            SSL_write(ssl, response_buf, strlen(response_buf));
        } else {
            response_len = get_response(response_buf, NOT_FOUND, 0, NULL);
            SSL_write(ssl, response_buf, strlen(response_buf));
        }
    }
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