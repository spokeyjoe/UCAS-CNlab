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

    socklen_t socklen = sizeof(caddr);

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
    printf("Binding succeed! (port %d)\n", port);
    listen(sock, 128);
    printf("Listening on port %d...\n", port);
    while (1) {
        int csock = accept(sock, (struct sockaddr*)&caddr, &socklen);
        if (csock < 0) {
            printf("Error (port %d)\n", port);
            perror("Socket accept failed\n");
            exit(1);
        }
        printf("Connection accepted! (port %d)\n", port);

        if (port == HTTP_PORT) {
            handle_http_request(csock, port);
            close(csock);
        } else {
            SSL* ssl = SSL_new(ctx);
            SSL_set_fd(ssl, csock);
            handle_https_request(ssl, port);
            SSL_free(ssl);
            close(csock);
        }
    }
}

/* Read request from socket SOCK, generate response and send. */
void handle_http_request(int sock, int port) {
    char request_buf[LEN_CONTENT_BUF];
    int request_len;
    char response_buf[LEN_RESPONSE_BUF];
    int response_len;

    // receive request
    if ((request_len = recv(sock, request_buf, LEN_CONTENT_BUF, 0)) <= 0) {
        perror("Receive failed\n");
        exit(1);
    }
    printf("Socket request received!\n");

    response_len = get_response(request_buf, response_buf, port);

    if (send(sock, response_buf, response_len, 0) < 0) {
        perror("Send response failed\n");
        exit(1);
    }
    printf("Socket response sent!\n");
}

/* Read request from SSL descriptor, generate response and send. */
void handle_https_request(SSL* ssl, int port) {
    char request_buf[LEN_CONTENT_BUF];
    int request_len;
    char response_buf[LEN_RESPONSE_BUF];
    int response_len = 0;

    if (SSL_accept(ssl) == -1) {
        perror("SSL accept failed\n");
        exit(1);
    }

    // read request into buffer
    request_len = SSL_read(ssl, request_buf, LEN_CONTENT_BUF);
    if (request_len < 0) {
        perror("SSL read failed!\n");
        exit(1);
    }
    printf("SSL request received!\n");

    // get response
    response_len = get_response(request_buf, response_buf, port);

    // send
    SSL_write(ssl, response_buf, response_len);
    printf("SSL response sent!\n");
}

/* Get HTTP response according to request in REQUEST_BUF. 
Write response to RESPONSE_BUF. Return response length. */
int get_response(char* request_buf, char* response_buf, int port) {
    int response_len = 0;
    int code;

    // parse request
    struct Request* req = parse_request(request_buf);

    // port 80
    if (port == HTTP_PORT) {
        code = MOVED_PERMANENTLY;
        char new_url[100];
        strcpy(new_url, "https://10.0.0.1");
        strcat(new_url, req -> url);
        response_len += sprintf(response_buf, "%s %d %s\r\n", req -> version, code, code2message(code));
        response_len += sprintf(response_buf + response_len, "Location: %s\r\n", new_url); 
    } else {
        // port 443
        int partial = 0;
        FILE* fp;

        // search file
        char refined_url[100];
        refined_url[0] = '.';
        char* url_dst = refined_url + 1;
        strcpy(url_dst, req -> url);

        if ((fp = fopen(refined_url, "r")) == NULL) {
            code = NOT_FOUND;
            response_len += sprintf(response_buf, "%s %d %s\r\n", req -> version, code, code2message(code));      response_len += sprintf(response_buf + response_len, "\r\n");
            response_len += sprintf(response_buf + response_len, "\r\n");
        } else {
            int file_len = get_file_len(fp);
            struct Header* h;
            int start = 0;
            int end = file_len - 1; 
            char str_range[15];
            strcpy(str_range, "Range");
            for (h = req -> headers; h; h = h -> next) {
                if (strcmp(h -> name, str_range) == 0) {
                    partial = 1;
                    sscanf(h -> value, "bytes=%d-%d", &start, &end);
                    end += 1;
                    break;
                }
            }
            if (partial == 1) {
                code = PARTIAL_CONTENT;
                int read_len = end - start;
                fseek(fp, start, SEEK_SET);
                response_len += sprintf(response_buf, "%s %d %s\r\n", req -> version, code, code2message(code));
                response_len += sprintf(response_buf + response_len, "Content-length: %d\r\n", read_len);
                response_len += sprintf(response_buf + response_len, "\r\n");
                fread(response_buf + response_len, sizeof(char), read_len, fp);
                fseek(fp, 0, SEEK_SET);
                response_len += read_len;
            } else {
                code = OK;
                fseek(fp, 0, SEEK_SET);
                response_len += sprintf(response_buf, "%s %d %s\r\n", req -> version, code, code2message(code));
                response_len += sprintf(response_buf + response_len, "Content-length: %d\r\n", file_len);
                response_len += sprintf(response_buf + response_len, "\r\n");
                fread(response_buf + response_len, sizeof(char), file_len, fp);
                fseek(fp, 0, SEEK_SET);
                response_len += file_len;
            }
            fclose(fp);
        }
    }
    return response_len;
}
