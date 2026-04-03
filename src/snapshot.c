/*
 * snapshot.c - Snapshot (commit) + Tree serialization
 *
 * Snapshot format (JSON):
 * {
 *   "parent": "sha..." | null,
 *   "tree":   "sha...",
 *   "author": "...",
 *   "message":"...",
 *   "time":   1234567890
 * }
 *
 * The "tree" field holds the SHA of the tree JSON, which itself
 * contains an array of {path, sha} pairs.
 */
#include "snapshot.h"
#include "cas.h"
#include "sha256.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Snapshot *snapshot_create(const char *parent_sha, const char *tree_sha,
                          const char *author, const char *message) {
    Snapshot *s = calloc(1, sizeof(Snapshot));
    if (!s) return NULL;
    s->parent_sha = parent_sha ? xstrdup(parent_sha) : NULL;
    s->tree_sha   = xstrdup(tree_sha);
    s->author     = xstrdup(author  ? author  : "anonymous");
    s->message    = xstrdup(message ? message : "(no message)");
    s->timestamp  = time(NULL);
    return s;
}

void snapshot_free(Snapshot *s) {
    if (!s) return;
    free(s->sha);
    free(s->parent_sha);
    free(s->tree_sha);
    free(s->author);
    free(s->message);
    free(s->tree_json);
    free(s);
}

char *snapshot_serialize(const Snapshot *s) {
    if (!s) return NULL;
    size_t needed = 512 + strlen(s->tree_sha) + strlen(s->author) + strlen(s->message);
    char *buf = malloc(needed);
    if (!buf) return NULL;
    int off = 0;
    off += snprintf(buf + off, needed - (size_t)off, "{");
    if (s->parent_sha)
        off += snprintf(buf + off, needed - (size_t)off, "\"parent\": \"%s\", ", s->parent_sha);
    else
        off += snprintf(buf + off, needed - (size_t)off, "\"parent\": null, ");
    off += snprintf(buf + off, needed - (size_t)off,
                    "\"tree\": \"%s\", \"author\": \"%s\", \"message\": \"",
                    s->tree_sha, s->author);
    for (const char *p = s->message; *p; p++) {
        if (*p == '"' || *p == '\\') buf[off++] = '\\';
        buf[off++] = *p;
    }
    snprintf(buf + off, needed - (size_t)off, "\", \"time\": %ld}", (long)s->timestamp);
    return buf;
}

Snapshot *snapshot_get(CAS *cas, const char *sha_hex) {
    uint8_t *data = NULL;
    size_t len = 0;
    if (cas_load(cas, sha_hex, &data, &len) != 0) return NULL;

    /* Parse minimal JSON: find "parent", "tree", "author", "message", "time" */
    Snapshot *s = calloc(1, sizeof(Snapshot));
    if (!s) { free(data); return NULL; }
    s->sha = xstrdup(sha_hex);

    /* Very simple JSON field extraction (no library) */
    const char *p = (const char *)data;
    char *fields[5] = {0}; /* parent, tree, author, message, time */
    int fi = 0;

    /* Extract quoted string values */
    while (*p && fi < 4) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++;
        const char *start = p;
        while (*p && *p != '"') p++;
        if (!*p) break;
        size_t len2 = (size_t)(p - start);
        fields[fi++] = malloc(len2 + 1);
        memcpy(fields[fi-1], start, len2);
        fields[fi-1][len2] = '\0';
        p++; /* skip closing quote */
    }

    if (fields[0]) { s->parent_sha = fields[0]; }
    if (fields[1]) { s->tree_sha   = fields[1]; }
    if (fields[2]) { s->author     = fields[2]; }
    if (fields[3]) { s->message    = fields[3]; }

    /* Extract time: find "time": */
    const char *tp = strstr((const char *)data, "\"time\":");
    if (tp) s->timestamp = (time_t)strtod(tp + 7, NULL);

    s->tree_json = xstrdup(s->tree_sha ? s->tree_sha : "");

    free(data);
    return s;
}

bool snapshot_verify(const Snapshot *s) {
    if (!s) return false;
    return s->tree_sha != NULL && strlen(s->tree_sha) == SHA256_HEX_LEN;
}

/* =====================================================================
 * Tree
 * ===================================================================== */

/* Tree JSON format: [{"path": "foo.txt", "sha": "abc..."}, ...] */

Tree tree_parse(const char *json) {
    Tree t = {0};
    if (!json) return t;

    /* Count objects: count '{' at depth 1 */
    int capacity = 32;
    t.paths = malloc((size_t)capacity * sizeof(char *));
    t.shas  = malloc((size_t)capacity * sizeof(char *));

    const char *p = json;
    while (*p) {
        /* Find next "path" */
        const char *path_kw = strstr(p, "\"path\"");
        const char *sha_kw  = strstr(p, "\"sha\"");
        if (!path_kw && !sha_kw) break;

        /* Use whichever comes first */
        const char *obj_start;
        const char *field;
        if (!path_kw) { field = sha_kw; obj_start = sha_kw; }
        else if (!sha_kw) { field = path_kw; obj_start = path_kw; }
        else { obj_start = (path_kw < sha_kw) ? path_kw : sha_kw; field = obj_start; }

        if (field == path_kw) {
            /* Extract path value */
            const char *q = strchr(field + 5, '"');
            if (!q) break;
            const char *val = q + 1;
            q = strchr(val, '"');
            if (!q) break;
            size_t vl = (size_t)(q - val);
            if ((size_t)t.count >= (size_t)capacity) {
                capacity *= 2;
                t.paths = realloc(t.paths, (size_t)capacity * sizeof(char *));
                t.shas  = realloc(t.shas,  (size_t)capacity * sizeof(char *));
            }
            t.paths[t.count] = malloc(vl + 1);
            memcpy(t.paths[t.count], val, vl);
            t.paths[t.count][vl] = '\0';
            p = q + 1;
        } else {
            /* Extract sha value */
            const char *q = strchr(field + 4, '"');
            if (!q) break;
            const char *val = q + 1;
            q = strchr(val, '"');
            if (!q) break;
            size_t vl = (size_t)(q - val);
            if ((size_t)t.count >= (size_t)capacity) {
                capacity *= 2;
                t.paths = realloc(t.paths, (size_t)capacity * sizeof(char *));
                t.shas  = realloc(t.shas,  (size_t)capacity * sizeof(char *));
            }
            t.shas[t.count] = malloc(vl + 1);
            memcpy(t.shas[t.count], val, vl);
            t.shas[t.count][vl] = '\0';
            t.count++;
            p = q + 1;
        }
    }
    return t;
}

void tree_free(Tree t) {
    for (int i = 0; i < t.count; i++) {
        free(t.paths[i]);
        free(t.shas[i]);
    }
    free(t.paths);
    free(t.shas);
}

char *tree_build_json(char **paths, char **shas, int count) {
    if (!paths || !shas || count < 0) return xstrdup("[]");
    char *json = malloc(8192);
    if (!json) return NULL;
    strcpy(json, "[");
    for (int i = 0; i < count; i++) {
        int off = (int)strlen(json);
        if (i > 0) json[off++] = ',';
        json[off] = '\0';
        off += snprintf(json + off, (size_t)(8192 - (size_t)off),
                        "{\"path\": \"%s\", \"sha\": \"%s\"}",
                        paths[i], shas[i]);
    }
    size_t len = strlen(json);
    json = realloc(json, len + 2);
    json[len] = ']';
    json[len + 1] = '\0';
    return json;
}
