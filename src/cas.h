/* cas.h - Content-Addressable Store */
#ifndef CAS_H
#define CAS_H
#include "backend.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct CAS {
    Backend *backend;
} CAS;

CAS  *cas_create(Backend *backend);
void  cas_free(CAS *cas);

/** cas_store - Store data, return SHA-256 hex (caller frees) */
char *cas_store(CAS *cas, const uint8_t *data, size_t len);

/** cas_load - Load data by SHA (caller frees *out_data) */
int   cas_load(CAS *cas, const char *sha_hex, uint8_t **out_data, size_t *out_len);

/** cas_exists - Check if an object exists */
bool  cas_exists(CAS *cas, const char *sha_hex);

/** cas_delete - Delete an object */
int   cas_delete(CAS *cas, const char *sha_hex);

/** cas_list_all - List all SHA-256 hex strings (caller frees each) */
char **cas_list_all(CAS *cas, int *count);

/** cas_gc - Garbage collect unreachable objects */
int   cas_gc(CAS *cas, char **roots, int root_count, int *freed_count);

struct CAS_Stats; /* defined in minnas.h */
void  cas_stats(CAS *cas, struct CAS_Stats *stats);

#endif
