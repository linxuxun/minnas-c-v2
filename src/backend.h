/*
 * backend.h - Storage backend abstraction
 *
 * A Backend provides the low-level read/write/exist/delete operations
 * for CAS objects. The same interface can back onto:
 *   - Local filesystem (objects/XX/XXXX...)
 *   - In-memory hash table (testing / ephemeral)
 *   - Remote HTTP API (future)
 *
 * Object layout on "local" backend:
 *   {root}/objects/{first-2-hex}/{full-sha256}
 *   e.g. objects/AB/CDEF1234...
 */

#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * Backend operations vtable
 * ===================================================================== */
typedef struct Backend Backend;  /* opaque handle */

typedef struct BackendOps {
    /**
     * write - Store an object
     * @ctx:  Backend-specific context
     * @sha:  SHA-256 hex string (64 chars, NUL-terminated)
     * @data: Object bytes
     * @len:  Byte length
     * Returns 0 on success, -1 on error (sets errno)
     */
    int  (*write)(void *ctx, const char *sha, const uint8_t *data, size_t len);

    /**
     * read - Load an object (caller must free *out_data)
     * Returns 0 on success, -1 if not found or error.
     */
    int  (*read)(void *ctx, const char *sha, uint8_t **out_data, size_t *out_len);

    /** exists - Check if an object exists. Returns 1 if yes, 0 if no, -1 on error. */
    int  (*exists)(void *ctx, const char *sha);

    /** delete - Remove an object. Returns 0 on success, -1 on error. */
    int  (*delete)(void *ctx, const char *sha);

    /** list_all - Enumerate all object SHAs. Returns NULL-terminated array (caller frees each). */
    char **(*list_all)(void *ctx, int *out_count);

    /** free - Release backend context */
    void (*free)(void *ctx);
} BackendOps;

struct Backend {
    const BackendOps *ops;  /* vtable, never NULL after creation */
    void             *ctx;  /* backend-specific data */
    char             *root; /* backend root (for local backend) */
};

/* =====================================================================
 * Backend factories
 * ===================================================================== */

/**
 * backend_local_create - Filesystem-based storage
 * @root_path: Repository root directory
 *
 * Stores objects as: {root_path}/objects/{2hex}/{38hex}
 * Creates the objects directory if missing.
 * Returns NULL on memory allocation failure.
 */
Backend *backend_local_create(const char *root_path);

/**
 * backend_memory_create - Ephemeral in-memory storage
 *
 * All data is lost when the process exits.
 * Useful for testing.
 * Returns NULL on allocation failure.
 */
Backend *backend_memory_create(void);

/**
 * backend_remote_create - Remote HTTP API backend (stub)
 * Returns NULL immediately; prints "not yet implemented" to stderr.
 */
Backend *backend_remote_create(const char *base_url, const char *auth_token);

/**
 * backend_free - Release a backend and all its resources
 * @b: Backend to free (safe to pass NULL)
 */
void backend_free(Backend *b);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
