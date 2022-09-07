#include "https-server.h"

/* Start 2 threads listening on 80/443 port. */
int main() {
    pthread_t t;
    int https_port = HTTPS_PORT;
    int http_port = HTTP_PORT;

    if (pthread_create(&t, NULL, listen, &http_port) != 0) {
        perror("Create thread failed");
        exit(1);
    }
    listen(&https_port);
    pthread_detach(t);
}

/* Start a socket and listen on port PORT_NUM,
handle HTTP requests. */
void* listen(void* port_num) {
    int port = (int)*port_num   // TODO: *((int*)port_num) ?

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

    // init socket
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    struct sockaddr_in caddr;
    bzero(&caddr, sizeof(caddr));

    socklen_t socklen;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    // init socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Open socket failed\n");
        exit(1);
    }
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Set socket failed\n");
        exit(1);
    }

    // bind & listen
    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Binding failed\n");
        exit(1);
    }
    printf("Binding succeed (port %d!)\n", port);
    listen(sock, 128);
    printf("Listening on port %d...\n", port);
    while (1) {
        int csock = accept(sock, (struct sockaddr*)&caddr, &socklen);
        if (csock < 0) {
            perror("Socket accept failed\n");
            exit(1);
        }
        printf("Connection accepted! (port %d)\n", port);
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, csock);
        handle_https_request(ssl, port);
    }
}

/* Read request from SSL descriptor, generate response and send. 
Use PORT to identify requests from 80. */
void handle_https_request(SSL* ssl, int port) {
    // alloc request buffer
    char request_buf[LEN_CONTENT_BUF];

    int request_len;
    int response_len;

    char method[10];
    char url[100];
    char version[10];
    // TODO: is it elegant?
    char field_names[10][20];
    char field_values[10][100];

    if (SSL_accept(ssl) == -1) {
        perror("SSL accept failed\n");
        exit(1);
    }
    pritnf("SSL accept succeed! (port %d)\n", port);

    // read request into buffer
    request_len = SSL_read(ssl, request_buf, LEN_CONTENT_BUF);
    if (request_len < 0) {
        perror("SSL raed failed!\n");
        exit(1);
    }
    printf("Request received!\n");

    // TODO: parse request into fields


    // Generate response
    // 301
    if (port == HTTP_PORT) {
        char response_buf[LEN_CONTENT_BUF];
        response_len = get_response(response_buf, MOVED_PERMANENTLY, length, url);
        SSL_write(ssl, response_buf, response_len);
    } else if ((fp = fopen(url, "r")) != NULL) {
        





        int file_len = get_file_len(fp);
        char response_buf[LEN_CONTENT_BUF + file_len];
        response_len = get_response(response_buf, OK, file_len, NULL);
    }
}

/* Generate HTTP response according to status code CODE
and content LENGTH. For port 80 requests, generate new HTTPS URL from URL. 

Return response length. */
int get_response(char* response_buf, int code, int length, char* url) {
    int len = 0;
    len += sprintf(response_buf, "HTTP/1.1 %d %s\r\n", code, code2message(code));
    if (code == OK) {
        len += sprintf(response_buf + len, "Content-length: %d\r\n", length);
    } else if (code == MOVED_PERMANENTLY) {
        char new_url[100];
        strcpy(new_url, "https:");
        len += sprintf(response_buf + len, "Location: %s\r\n", strcat(new_url, url))
    }
}

/* Cound file length. */
int get_file_len(FILE* fp) {
    // TODO
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}