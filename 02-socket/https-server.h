#ifndef HTTPS_SERVER_H
#define HTTPS_SERVER_H

#include <openssl/ssl.h>
#include <openssl/err.h>
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

#define HTTP_PORT 80
#define HTTPS_PORT 443

#define LEN_CONTENT_BUF 512
#define LEN_RESPONSE_BUF 81920

#define OK                200
#define MOVED_PERMANENTLY 301
#define PARTIAL_CONTENT   206
#define NOT_FOUND         404

typedef enum Method {UNSUPPORTED, GET, HEAD} Method;

/* Structure for HTTP header. */
typedef struct Header {
    char *name;
    char *value;
    struct Header *next;
} Header;

/* Structure for HTTP request. */
typedef struct Request {
    enum Method method;
    char *url;
    char *version;
    struct Header *headers;
    char *body;
} Request;

void* listen_port(void* port_num);
void handle_http_request(int sock, int port);
void handle_https_request(SSL* ssl, int port);
int get_response(char* request_buf, char* response_buf, int port);
struct Request* parse_request(const char* raw_requset);
void free_header(struct Header* header);
void free_request(struct Request *request);
const char* code2message(int code);
int get_file_len(FILE* fp);

#endif
