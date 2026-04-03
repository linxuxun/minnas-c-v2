#define _DEFAULT_SOURCE
/*
 * utils.c - Utility functions implementation
 */

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>

/* Platform-specific mkdir with parents */
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *r = strdup(s);
    if (!r) { perror("strdup"); exit(1); }
    return r;
}

char *xstrndup(const char *s, size_t n) {
    if (!s) return NULL;
    char *r = strndup(s, n);
    if (!r) { perror("strndup"); exit(1); }
    return r;
}

char *path_join(const char *a, const char *b) {
    if (!a || !b) return NULL;
    size_t alen = strlen(a);
    while (alen > 0 && a[alen-1] == '/') alen--;
    while (*b == '/') b++;
    char *r = malloc(alen + 1 + strlen(b) + 2);
    if (!r) return NULL;
    memcpy(r, a, alen);
    r[alen] = '/';
    strcpy(r + alen + 1, b);
    return r;
}

char *path_dirname(const char *path) {
    if (!path) return NULL;
    char *p = xstrdup(path);
    char *d = dirname(p);
    char *r = xstrdup(d);
    free(p);
    return r;
}

char *path_basename(const char *path) {
    if (!path) return NULL;
    char *p = xstrdup(path);
    char *b = basename(p);
    char *r = xstrdup(b);
    free(p);
    return r;
}

char *path_normalize(const char *path) {
    if (!path) return NULL;
    char *r = xstrdup(path);
    /* Simple normalization: remove trailing / */
    size_t len = strlen(r);
    while (len > 1 && r[len-1] == '/') {
        r[--len] = '\0';
    }
    return r;
}

int ensure_dir(const char *path) {
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }
    /* Try to create recursively */
    char *p = xstrdup(path);
    for (char *s = p + 1; *s; s++) {
        if (*s == '/') {
            *s = '\0';
            if (stat(p, &st) != 0) {
                if (mkdir(p, 0755) != 0 && errno != EEXIST) {
                    free(p);
                    errno = ENOENT;
                    return -1;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                free(p);
                errno = ENOTDIR;
                return -1;
            }
            *s = '/';
        }
    }
    if (mkdir(p, 0755) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }
    free(p);
    return 0;
}

int remove_dir_recursive(const char *path) {
    if (!path) return -1;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    return system(cmd);
}

bool file_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0;
}

int read_file(const char *path, char **out_data, size_t *out_len) {
    return read_binary_file(path, (uint8_t **)out_data, out_len);
}

int read_binary_file(const char *path, uint8_t **out_data, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if ((long)n != sz) {
        free(buf);
        return -1;
    }
    buf[sz] = '\0';
    *out_data = buf;
    *out_len = (size_t)sz;
    return 0;
}

int write_file(const char *path, const char *data, size_t len) {
    return write_binary_file(path, (const uint8_t *)data, len);
}

int write_binary_file(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    if (n != len) return -1;
    return 0;
}

int append_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "ab");
    if (!f) return -1;
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    if (n != len) return -1;
    return 0;
}

long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

int copy_file(const char *src, const char *dst) {
    uint8_t *buf = NULL;
    size_t len = 0;
    if (read_binary_file(src, &buf, &len) != 0) return -1;
    int r = write_binary_file(dst, buf, len);
    free(buf);
    return r;
}

char **strlist_append(char **list, int *count, const char *s) {
    list = realloc(list, (*count + 2) * sizeof(char *));
    list[*count] = xstrdup(s);
    (*count)++;
    list[*count] = NULL;
    return list;
}

void strlist_free(char **list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) free(list[i]);
    free(list);
}

uint8_t *hex_to_bin(const char *hex, size_t *out_len) {
    size_t hlen = strlen(hex);
    if (hlen % 2 != 0) return NULL;
    *out_len = hlen / 2;
    uint8_t *bin = malloc(*out_len);
    if (!bin) return NULL;
    for (size_t i = 0; i < *out_len; i++) {
        unsigned int val;
        if (sscanf(hex + 2*i, "%2x", &val) != 1) { free(bin); return NULL; }
        bin[i] = (uint8_t)val;
    }
    return bin;
}

char *bin_to_hex(const uint8_t *bin, size_t len) {
    char *hex = malloc(len * 2 + 1);
    if (!hex) return NULL;
    static const char hex_digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex[i*2] = hex_digits[bin[i] >> 4];
        hex[i*2+1] = hex_digits[bin[i] & 0xF];
    }
    hex[len*2] = '\0';
    return hex;
}

char *strtrim(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return s;
}

const char *skip_ws(const char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    return s;
}

bool str_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t sl = strlen(s), fl = strlen(suffix);
    if (fl > sl) return false;
    return strcmp(s + sl - fl, suffix) == 0;
}
