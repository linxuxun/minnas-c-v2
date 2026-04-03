/*
 * cas.c - Content-Addressable Store
 *
 * Core CAS operations built on top of the Backend abstraction.
 * Objects are deduplicated by SHA-256; storing the same content
 * twice returns the same SHA (no duplicate storage).
 */
#include "minnas.h"
#include "cas.h"
#include "sha256.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SHA256_STR_LEN 64

CAS *cas_create(Backend *backend) {
    if (!backend) return NULL;
    CAS *cas = calloc(1, sizeof(CAS));
    if (!cas) return NULL;
    cas->backend = backend;
    return cas;
}

void cas_free(CAS *cas) {
    free(cas);  /* Backend is owned by caller, not freed here */
}

char *cas_store(CAS *cas, const uint8_t *data, size_t len) {
    if (!cas || !data) { errno = EINVAL; return NULL; }

    /* Compute SHA-256 */
    uint8_t hash[SHA256_BIN_LEN];
    sha256_hash(data, len, hash);
    char sha_hex[SHA256_STR_LEN + 1];
    sha256_hex_to(sha_hex, hash);

    /* Deduplication: check if already stored */
    if (cas->backend->ops->exists(cas->backend->ctx, sha_hex)) {
        return xstrdup(sha_hex);
    }

    /* Write to backend */
    if (cas->backend->ops->write(cas->backend->ctx, sha_hex, data, len) != 0) {
        return NULL;
    }

    return xstrdup(sha_hex);
}

int cas_load(CAS *cas, const char *sha_hex, uint8_t **out_data, size_t *out_len) {
    if (!cas || !sha_hex || !out_data || !out_len) return -1;
    if (strlen(sha_hex) != SHA256_STR_LEN) { errno = EINVAL; return -1; }
    *out_data = NULL;
    *out_len = 0;
    return cas->backend->ops->read(cas->backend->ctx, sha_hex, out_data, out_len);
}

bool cas_exists(CAS *cas, const char *sha_hex) {
    if (!cas || !sha_hex) return false;
    return cas->backend->ops->exists(cas->backend->ctx, sha_hex) == 1;
}

int cas_delete(CAS *cas, const char *sha_hex) {
    if (!cas || !sha_hex) return -1;
    return cas->backend->ops->delete(cas->backend->ctx, sha_hex);
}

char **cas_list_all(CAS *cas, int *count) {
    if (!cas || !count) return NULL;
    return cas->backend->ops->list_all(cas->backend->ctx, count);
}

/* cas_gc - mark-and-sweep GC
 * roots: SHAs that are directly reachable from refs
 * We do a simple version: report total + reachable, no actual deletion
 * in this minimal implementation. Real GC would walk all object references.
 */
int cas_gc(CAS *cas, char **roots, int root_count, int *freed_count) {
    if (!cas || !freed_count) return -1;
    *freed_count = 0;

    int total = 0;
    char **all = cas_list_all(cas, &total);
    if (!all) return 0;

    /* Build reachable set: SHA → true (simple HashTable would be better) */
    /* For now, just report total; real reachability analysis is TODO */
    for (int i = 0; i < total; i++) free(all[i]);
    free(all);
    (void)roots; (void)root_count;
    return 0;
}

void cas_stats(CAS *cas, struct CAS_Stats *stats) {
    if (!cas || !stats) return;
    memset(stats, 0, sizeof(*stats));

    int count = 0;
    char **all = cas_list_all(cas, &count);
    if (!all) return;

    stats->object_count = count;
    for (int i = 0; i < count; i++) {
        uint8_t *data = NULL;
        size_t len = 0;
        if (cas->backend->ops->read(cas->backend->ctx, all[i], &data, &len) == 0) {
            stats->total_size += len;
            free(data);
        }
        free(all[i]);
    }
    free(all);
}
