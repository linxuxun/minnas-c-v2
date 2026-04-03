/*
 * blob.c - Git-style blob implementation
 *
 * Blob format: "blob {size}\0{data...}"
 * The header is: "blob " (5 bytes) + decimal size + \0
 * This is the same format as Git uses, ensuring compatibility.
 */
#include "blob.h"
#include "sha256.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *blob_build(const uint8_t *data, size_t data_len, size_t *out_len) {
    if (!data || !out_len) return NULL;
    char header[32];
    int hlen = snprintf(header, sizeof(header), "blob %zu", data_len);
    /* header is NOT NUL-terminated by snprintf at the right spot for Git compat.
     * Git uses: "blob {size}\0" — the \0 separates header from data. */
    size_t blob_len = (size_t)hlen + 1 + data_len; /* +1 for the \0 separator */
    uint8_t *blob = malloc(blob_len);
    if (!blob) return NULL;
    memcpy(blob, header, (size_t)hlen);
    blob[hlen] = '\0';
    memcpy(blob + hlen + 1, data, data_len);
    *out_len = blob_len;
    return blob;
}

/* Parse decimal size from "blob {size}\0..." position */
static size_t parse_size(const char *p) {
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (!*p) return 0;
    size_t n = 0;
    while (isdigit((unsigned char)*p)) {
        n = n * 10 + (size_t)(*p - '0');
        p++;
    }
    return n;
}

uint8_t *blob_read(const uint8_t *blob, size_t blob_len, size_t *out_len) {
    if (!blob || blob_len == 0 || !out_len) return NULL;
    if (memcmp(blob, "blob ", 5) != 0) return NULL;

    size_t size = parse_size((const char *)blob + 5);

    /* Find the \0 separator (end of header) */
    const uint8_t *p = blob + 5;
    while (*p != '\0' && (size_t)(p - blob) < blob_len) p++;
    p++; /* skip the \0 separator */
    if ((size_t)(p - blob) > blob_len) return NULL;

    size_t data_start = (size_t)(p - blob);
    if (data_start + size != blob_len) return NULL;

    uint8_t *copy = malloc(size);
    if (!copy) return NULL;
    memcpy(copy, blob + data_start, size);
    *out_len = size;
    return copy;
}

bool blob_verify(const uint8_t *blob, size_t blob_len) {
    if (!blob || blob_len < 6) return false;
    if (memcmp(blob, "blob ", 5) != 0) return false;
    size_t size = parse_size((const char *)blob + 5);
    const uint8_t *p = blob + 5;
    while (*p != '\0' && (size_t)(p - blob) < blob_len) p++;
    p++;
    return (size_t)(p - blob) + size == blob_len;
}

const char *blob_describe(const uint8_t *blob, size_t blob_len) {
    static char buf[128];
    if (!blob_verify(blob, blob_len)) {
        snprintf(buf, sizeof(buf), "<invalid blob len=%zu>", blob_len);
        return buf;
    }
    size_t data_len;
    blob_read(blob, blob_len, &data_len);
    snprintf(buf, sizeof(buf), "blob %zu bytes (total %zu with header)",
             data_len, blob_len);
    return buf;
}
