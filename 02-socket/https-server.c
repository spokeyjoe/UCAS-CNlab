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
#include "https-server.h"


int main() {
    pthread_t pthrd;

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
        handle_https_request(csock, port);
    }
    return 0;
}

void handle_https_request(int sock, int port) {
    char request_buf[LEN_REQUEST_BUF] = {0};
    char response_buf[LEN_RESPONSE_BUF] = {0};

    char method[20];
    char filename[100];

    int response_len;
    int request_len;
    int num_chars;

    FILE* fp = NULL;

    if ((request_len = recv(sock, request_buf, LEN_REQUEST_BUF, 0)) <= 0){
		perror("Receive request failed");
		exit(1);
	} else {
        if (port == HTTP_PORT) {
            response_len = get_response(response_buf, MOVED_PERMANENTLY, 0, NULL);
            send(sock, response_buf, response_len, 0);
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
            send(sock, response_buf, response_len + num_chars, 0);
        } else {
            response_len = get_response(response_buf, NOT_FOUND, 0, NULL);
            send(sock, response_buf, response_len, 0)
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