/*
 * minnas.h - MiniNAS: Git-like content-addressable file storage
 *
 * Design goals:
 *   - Content-addressable storage (CAS) for deduplication
 *   - Snapshot-based versioning with branch support
 *   - Namespace isolation for multi-tenant usage
 *   - Virtual filesystem (VFS) API for file operations
 *
 * Architecture:
 *   Repo → Backend (CAS) → VFS → Blob → SHA-256
 *        → BranchMgr  → Reflog
 *        → NamespaceMgr
 */

#ifndef MINNAS_H
#define MINNAS_H

#include <time.h>
#include <stdbool.h>
#include <stddef.h>

/* =====================================================================
 * Error handling
 * ===================================================================== */
#define MINNAS_OK          0
#define MINNAS_ERR        -1
#define MINNAS_ERR_NOMEM  -2
#define MINNAS_ERR_NOTFOUND -3
#define MINNAS_ERR_EXISTS  -4
#define MINNAS_ERR_INVALID -5
#define MINNAS_ERR_PERM    -6

const char *minnas_error(int errcode);

/* =====================================================================
 * Forward declarations
 * ===================================================================== */
typedef struct Backend Backend;
typedef struct CAS CAS;
typedef struct VFS VFS;
typedef struct BranchMgr BranchMgr;
typedef struct NamespaceMgr NamespaceMgr;
typedef struct Snapshot Snapshot;
typedef struct Tree Tree;

/* =====================================================================
 * Repository
 * ===================================================================== */
typedef struct Repo {
    char          *root;          /* Repository root path */
    char          *backend_type;  /* "local" or "memory" */
    Backend       *backend;       /* CAS backend */
    CAS           *cas;           /* Content-addressable store */
    NamespaceMgr  *nsmgr;         /* Namespace manager */
    BranchMgr     *branchmgr;     /* Branch + reflog manager */
    VFS           *vfs;           /* Virtual filesystem */
    char           error[256];    /* Last error message */
} Repo;

/**
 * repo_init - Create and initialize a new repository
 * @path:   Root directory path
 * @type:   Backend type ("local" or "memory")
 *
 * Returns new Repo on success, NULL on failure.
 * Caller owns the returned pointer; call repo_free() to release.
 */
Repo *repo_init(const char *path, const char *type);

/**
 * repo_open - Open an existing repository
 * @path:  Root directory path
 *
 * Returns Repo on success, NULL if not a valid MinNAS repo.
 */
Repo *repo_open(const char *path);

/**
 * repo_free - Free a repository (does NOT save any pending state)
 * @r:  Repository to free
 */
void repo_free(Repo *r);

/**
 * repo_commit - Create a new commit snapshot
 * @r:      Repository
 * @msg:    Commit message
 * @author: Author name
 *
 * Returns SHA-1 hex string of new commit on success (caller frees),
 * NULL on failure.
 */
char *repo_commit(Repo *r, const char *msg, const char *author);

/* =====================================================================
 * Repository status
 * ===================================================================== */
typedef struct RepoStatus {
    char  **modified;   /* Paths modified since last commit */
    char  **added;      /* Paths added since last commit   */
    char  **deleted;    /* Paths deleted since last commit */
    int    modified_count;
    int    added_count;
    int    deleted_count;
} RepoStatus;

RepoStatus *repo_status(Repo *r);
void        repo_status_free(RepoStatus *s);

/* =====================================================================
 * Commit log
 * ===================================================================== */
typedef struct RepoLogEntry {
    char   *sha;      /* Commit SHA (64 hex chars, caller frees) */
    char   *message; /* Commit message (caller frees) */
    char   *author;  /* Author name (caller frees) */
    time_t  time;    /* Commit timestamp */
} RepoLogEntry;

/**
 * repo_log - Get commit history
 * @r:        Repository
 * @max_count: Maximum number of entries to return
 * @count:    [out] Actual number of entries returned
 *
 * Returns array of RepoLogEntry* (caller frees array and entries
 * via repo_log_free()).
 */
RepoLogEntry **repo_log(Repo *r, int max_count, int *count);
void           repo_log_free(RepoLogEntry **entries, int count);

/* =====================================================================
 * Snapshot management
 * ===================================================================== */
/**
 * repo_list_snapshots - List all snapshots in the repository
 * @r:      Repository
 * @count:  [out] Number of snapshots returned
 *
 * Returns array of Snapshot* (free each with snapshot_free(),
 * free array with free()).
 */
Snapshot **repo_list_snapshots(Repo *r, int *count);

/**
 * repo_checkout_snapshot - Checkout a snapshot as the current state
 * @r:   Repository
 * @sha: Snapshot SHA to checkout
 *
 * Returns MINNAS_OK on success, error code on failure.
 */
int repo_checkout_snapshot(Repo *r, const char *sha);

/* =====================================================================
 * Diff
 * ===================================================================== */
typedef struct Change {
    char   *path;    /* File path */
    char    status;  /* 'A'dd, 'D'elete, 'M'odify */
    char   *old_sha; /* SHA before (NULL for new files) */
    char   *new_sha; /* SHA after (NULL for deleted files) */
} Change;

/**
 * repo_diff - Compare two snapshots
 * @r:      Repository
 * @sha1:   Earlier snapshot SHA (or NULL for empty)
 * @sha2:   Later snapshot SHA (or NULL for current)
 * @count:  [out] Number of changes
 *
 * Returns array of Change* (caller frees each + array).
 */
Change **repo_diff(Repo *r, const char *sha1, const char *sha2, int *count);
void     repo_diff_free(Change **changes, int count);

/* =====================================================================
 * Garbage collection
 * ===================================================================== */
/**
 * repo_gc - Run garbage collection
 * @r:  Repository
 *
 * Returns number of unreachable objects freed, or -1 on error.
 */
int repo_gc(Repo *r);

/* =====================================================================
 * Statistics
 * ===================================================================== */
typedef struct CAS_Stats {
    int   object_count;   /* Number of CAS objects */
    size_t total_size;    /* Total byte size of all objects */
} CAS_Stats;

void repo_stats(Repo *r, CAS_Stats *cs, int *snap_count, int *branch_count);

#endif /* MINNAS_H */
