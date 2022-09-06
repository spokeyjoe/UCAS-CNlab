#ifndef HTTPS_SERVER_H
#define HTTPS_SERVER_H

#define HTTP_PORT 80
#define HTTPS_PORT 443

#define OK                200
#define MOVED_PERMANENTLY 301
#define PARTIAL_CONTENT   206
#define NOT_FOUND         404

#define LEN_REQUEST_BUF   1024
#define LEN_RESPONSE_BUF  1024


const char* code2message(int code) {
    switch (code) {
        case 200:
            return "OK";
        case 206:
            return "PARTIAL CONTENT";
        case 301:
            return "MOVED PERMANENTLY";
        case 404:
            return "NOT FOUND";
        default:
            return "UNDEFINED CODE";
    }
}

#endif