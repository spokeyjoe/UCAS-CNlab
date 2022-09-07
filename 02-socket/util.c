#include "https-server.h";

/* Parse the raw request string RAW_REQUEST,
return the defined request structure. */
struct Request* parse_request(const char* raw_request) {
    struct Request *req = NULL;
    req = malloc(sizeof(struct Request));
    memset(req, 0, sizeof(struct Request));

    // parse method
    size_t method_len = strcspn(raw_request, " ");
    if (memcmp(raw_request, "GET", strlen("GET")) == 0) {
        req -> method = GET;
    } else if (memcmp(raw_request, "HEAD", strlen("HEAD")) == 0) {
        req -> method = HEAD;
    } else {
        req -> method = UNSUPPORTED;
    }
    raw_request += (method_len + 1); // move past <SP>

    // parse URL
    size_t url_len = strcspn(raw_request, " ");
    req -> url = malloc(url_len + 1);
    memcpy(req -> url, raw_request, url_len);
    req -> url[url_len] = '\0';
    raw_request += url_len + 1; // move past <SP>

    // parse HTTP version
    size_t version_len = strcspn(raw_request, "\r\n");
    req -> version = malloc(version_len + 1);
    memcpy(req -> version, raw_request, version_len);
    req -> version[version_len] = '\0';
    raw_request += version_len + 2; // move past <CR><LF>

    struct Header* header = NULL, *last = NULL;
    while (raw_request[0] != '\r' || raw_request[1] != '\n') {
        last = header;
        header = malloc(sizeof(struct Header));

        // name
        size_t name_len = strcspn(raw_request, ":");
        header -> name = malloc(name_len + 1);
        memcpy(header -> name, raw_request, name_len);
        header -> name[name_len] = '\0';
        raw_request += name_len + 1; // move past ':'
        while (*raw_request == ' ') {
            raw_request += 1;
        }

        // value
        size_t value_len = strcspn(raw_request, "\r\n");
        header -> value = malloc(value_len + 1);
        memcpy(header -> value, raw_request, value_len);
        header -> value[value_len] = '\0';
        raw_request += value_len + 2; // move past <CR><LF>

        // next
        header -> next = last;
    }
    req -> headers = header;
    raw_request += 2; // move past <CR><LF>

    // body
    size_t body_len = strlen(raw_request);
    req -> body = malloc(body_len + 1);
    memcpy(req -> body, raw_request, body_len);
    req -> body[body_len] = '\0';

    return req;
}

/* Get status message from code. */
const char* code2message(int code) {
    switch (code) {
        case OK:
            return "OK";
        case PARTIAL_CONTENT:
            return "PARTIAL CONTENT";
        case MOVED_PERMANENTLY:
            return "MOVED PERMANENTLY";
        case NOT_FOUND:
            return "NOT FOUND";
        default:
            return "UNDEFINED CODE";
    }
}

/* Count file length. */
int get_file_len(FILE* fp) {
    // TODO
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}