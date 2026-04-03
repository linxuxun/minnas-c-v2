#define _DEFAULT_SOURCE
#include "backend.h"
#include "hash.h"
#include "sha256.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct LocalBackend { char *root; } LocalBackend;

static char *local_obj_path(LocalBackend *lb, const char *sha_hex) {
    char prefix[3] = { sha_hex[0], sha_hex[1], '\0' };
    char *d = path_join(lb->root, "objects");
    char *p = path_join(d, prefix);
    free(d);
    char *fp = path_join(p, sha_hex + 2);
    free(p);
    return fp;
}

static int local_write(void *ctx, const char *sha_hex, const uint8_t *data, size_t len) {
    LocalBackend *lb = ctx;
    char prefix[3] = { sha_hex[0], sha_hex[1], '\0' };
    char *od = path_join(lb->root, "objects");
    char *by_prefix = path_join(od, prefix);
    free(od);
    if (ensure_dir(by_prefix) != 0) { free(by_prefix); return -1; }
    char *path = path_join(by_prefix, sha_hex + 2);
    free(by_prefix);
    int r = write_binary_file(path, data, len);
    free(path);
    return r;
}

static int local_read(void *ctx, const char *sha_hex, uint8_t **out_data, size_t *out_len) {
    char *path = local_obj_path(ctx, sha_hex);
    int r = read_binary_file(path, out_data, out_len);
    free(path);
    return r;
}
static int local_exists(void *ctx, const char *sha_hex) {
    char *path = local_obj_path(ctx, sha_hex);
    int exists = file_exists(path) ? 1 : 0;
    free(path);
    return exists;
}
static int local_delete(void *ctx, const char *sha_hex) {
    char *path = local_obj_path(ctx, sha_hex);
    int r = remove(path);
    free(path);
    return (r == 0 || errno == ENOENT) ? 0 : -1;
}
static char **local_list_all(void *ctx, int *out_count) {
    LocalBackend *lb = ctx;
    char **list = NULL;
    int count = 0;
    char *objroot = path_join(lb->root, "objects");
    DIR *dir = opendir(objroot);
    free(objroot);
    if (!dir) return NULL;
    struct dirent *pe;
    while ((pe = readdir(dir)) != NULL) {
        if (pe->d_type != DT_DIR) continue;
        if (strlen(pe->d_name) != 2) continue;
        if (pe->d_name[0] == '.') continue;
        char prefix[3] = { pe->d_name[0], pe->d_name[1], '\0' };
        char *sub = path_join(lb->root, "objects");
        char *subd = path_join(sub, prefix);
        free(sub);
        DIR *sd = opendir(subd);
        if (!sd) { free(subd); continue; }
        struct dirent *fe;
        while ((fe = readdir(sd)) != NULL) {
            if (fe->d_type != DT_REG) continue;
            if ((int)strlen(fe->d_name) != SHA256_HEX_LEN - 2) continue;
            char sha[SHA256_HEX_LEN + 1];
            snprintf(sha, sizeof(sha), "%s%s", prefix, fe->d_name);
            list = realloc(list, (size_t)(count + 1) * sizeof(char *));
            list[count++] = xstrdup(sha);
        }
        closedir(sd); free(subd);
    }
    closedir(dir);
    if (list) list[count] = NULL;
    *out_count = count;
    return list;
}
static void local_free(void *ctx) { if (!ctx) return; LocalBackend *lb = ctx; free(lb->root); free(lb); }
static const BackendOps local_ops = {
    local_write, local_read, local_exists, local_delete, local_list_all, local_free,
};
Backend *backend_local_create(const char *root_path) {
    if (!root_path) return NULL;
    LocalBackend *lb = calloc(1, sizeof(LocalBackend));
    Backend *b = calloc(1, sizeof(Backend));
    if (!lb || !b) { free(lb); free(b); return NULL; }
    lb->root = xstrdup(root_path);
    b->ops = &local_ops; b->ctx = lb; b->root = xstrdup(root_path);
    char *od = path_join(root_path, "objects");
    if (ensure_dir(od) != 0) { free(od); free(b->root); free(b); free(lb->root); free(lb); return NULL; }
    free(od);
    return b;
}

typedef struct MemEntry { uint8_t *data; size_t len; } MemEntry;
typedef struct MemBackend { HashTable *store; } MemBackend;
static int mem_write(void *ctx, const char *sha_hex, const uint8_t *data, size_t len) {
    MemBackend *mb = ctx;
    MemEntry *e = malloc(sizeof(MemEntry));
    if (!e) return -1;
    e->data = malloc(len);
    if (!e->data) { free(e); return -1; }
    memcpy(e->data, data, len); e->len = len;
    MemEntry *old = ht_get(mb->store, sha_hex);
    if (old) { free(old->data); free(old); }
    ht_set(mb->store, sha_hex, e);
    return 0;
}
static int mem_read(void *ctx, const char *sha_hex, uint8_t **out_data, size_t *out_len) {
    MemEntry *e = ht_get(ctx, sha_hex);
    if (!e) return -1;
    *out_data = malloc(e->len);
    if (!*out_data) return -1;
    memcpy(*out_data, e->data, e->len);
    *out_len = e->len;
    return 0;
}
static int mem_exists(void *ctx, const char *sha_hex) { return ht_has(ctx, sha_hex) ? 1 : 0; }
static int mem_delete(void *ctx, const char *sha_hex) {
    MemEntry *e = ht_get(ctx, sha_hex);
    if (e) { free(e->data); free(e); }
    return ht_del(ctx, sha_hex) ? 0 : -1;
}
static char **mem_list_all(void *ctx, int *out_count) { return ht_keys(ctx, out_count); }
static bool mem_iter(const char *k, void *v, void *u) { (void)k;(void)u; MemEntry *e = v; free(e->data); free(e); return true; }
static void mem_free(void *ctx) { if (!ctx) return; MemBackend *mb = ctx; ht_foreach(mb->store, mem_iter, NULL); ht_free(mb->store); free(mb); }
static const BackendOps mem_ops = {
    mem_write, mem_read, mem_exists, mem_delete, mem_list_all, mem_free,
};
Backend *backend_memory_create(void) {
    MemBackend *mb = calloc(1, sizeof(MemBackend));
    Backend *b = calloc(1, sizeof(Backend));
    if (!mb || !b) { free(mb); free(b); return NULL; }
    mb->store = ht_create();
    if (!mb->store) { free(mb); free(b); return NULL; }
    b->ops = &mem_ops; b->ctx = mb;
    return b;
}
Backend *backend_remote_create(const char *base_url, const char *auth_token) {
    (void)base_url; (void)auth_token;
    fprintf(stderr, "Remote backend not yet implemented.\\n");
    return NULL;
}
void backend_free(Backend *b) {
    if (!b) return;
    if (b->ctx && b->ops && b->ops->free) b->ops->free(b->ctx);
    free(b->root);
    free(b);
}
