/* errors.c - Centralized MinNAS error handling */
#include <string.h>

const char *minnas_error(int errcode) {
    switch (errcode) {
        case  0: return "success";
        case -1: return "generic error";
        case -2: return "out of memory";
        case -3: return "not found";
        case -4: return "already exists";
        case -5: return "invalid argument";
        case -6: return "permission denied";
        default: return "unknown error";
    }
}
