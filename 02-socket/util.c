#include "https-server.h";



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