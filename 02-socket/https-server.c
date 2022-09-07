#include "https-server.h"

/* Start 2 threads listening on 80/443 port. */
int main() {
    pthread_t t;
    int https_port = HTTPS_PORT;
    int http_port = HTTP_PORT;

    if (pthread_create(&t, NULL, listen_port, &http_port) != 0) {
        perror("Create thread failed");
        exit(1);
    }
    listen_port(&https_port);
    pthread_detach(t);
}

/* Start a socket and listen on port PORT_NUM,
handle HTTP requests. */
void* listen_port(void* port_num) {
    int port = *((int*)port_num);

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

    if (SSL_accept(ssl) == -1) {
        perror("SSL accept failed\n");
        exit(1);
    }
    printf("SSL accept succeed! (port %d)\n", port);

    // read request into buffer
    request_len = SSL_read(ssl, request_buf, LEN_CONTENT_BUF);
    if (request_len < 0) {
        perror("SSL raed failed!\n");
        exit(1);
    }
    printf("Request received!\n");

    // parse request
    struct Request* req = parse_request(request_buf);

    printf("[DEBUG] Method: %d\n", req -> method);
    printf("[DEBUG] URL: %s\n", req -> url);
    printf("[DEBUG] HTTP-version: %s\n", req -> version);
    printf("[DEBUG] Headers:\n");
    struct Header *h;
    for (h=req->headers; h; h=h->next) {
        printf("%32s: %s\n", h->name, h->value);
    }
    puts("[DEBUG] message-body:");
    puts(req->body);



    // generate response and send
    int partial = 0;
    int response_len = 0;
    int code;
    if (port == HTTP_PORT) {
        code = MOVED_PERMANENTLY;
        char response_buf[LEN_CONTENT_BUF];
        char new_url[100];
        strcpy(new_url, "https:");
        response_len += sprintf(response_buf, "HTTP/1.1 %d %s\r\n", code, code2message(code));
        response_len += sprintf(response_buf + response_len, "Location: %s\r\n", strcat(new_url, req -> url));
        SSL_write(ssl, response_buf, response_len);
    } else if ((fp = fopen(url, "r")) == NULL) {
        code = NOT_FOUND;
        char response_buf[LEN_CONTENT_BUF];
        response_len += sprintf(response_buf, "HTTP/1.1 %d %s\r\n", code, code2message(code));
        SSL_write(ssl, response_buf, response_len);
    } else {
        file_len = get_file_len(fp);
        struct Header* h;
        int start = 0;
        int end = file_len; 
        char str_range[15];
        strcpy(str_range, "Range");
        for (h = req -> headers; h; h = h -> next) {
            if (strcmp(h -> name, str_range) == 0) {
                partial = 1;
                sscanf(h -> value, "bytes=%d-%d", &start, &end);
                break();
            }
        }
        if (partial == 1) {
            code = PARTIAL_CONTENT;
            int read_len = end - start;
            fseek(fp, start, SEEK_SET);
            char response_buf[LEN_CONTENT_BUF];
            response_len += sprintf(response_buf, "HTTP/1.1 %d %s\r\n", code, code2message(code));
            fread(response_buf + response_len, sizeof(char), read_len, fp);
            response_len += read_len;
            SSL_write(ssl, response_buf, response_len);
        } else {
            code = OK;
            char response_buf[LEN_CONTENT_BUF + file_len];
            response_len += sprintf(response_buf, "HTTP/1.1 %d %s\r\n", code, code2message(code));
            response_len += sprintf(response_buf + response_len, "Content-length: %d\r\n", file_len);
            fread(response_buf + response_len, sizeof(char), file_len, fp);
            response_len += file_len;
            SSL_write(ssl, response_buf, response_len);
        }
    }

    int sock = SSL_get_fd(ssl);
    SSL_free(ssl);
    close(sock);
}
