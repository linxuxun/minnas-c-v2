/*
 * minnas.c - Repository implementation
 *
 * A Repo ties together:
 *   - Backend (CAS storage engine)
 *   - CAS (content-addressable store)
 *   - VFS  (virtual filesystem view)
 *   - BranchMgr (branch refs + reflog)
 *   - NamespaceMgr (optional namespace isolation)
 *
 * The commit flow:
 *   1. VFS builds a Tree from current file state
 *   2. Tree JSON is stored in CAS → tree_sha
 *   3. Snapshot is built: { parent, tree_sha, author, message }
 *   4. Snapshot JSON is stored in CAS → snap_sha
 *   5. BranchMgr updates HEAD to snap_sha, appends reflog entry
 */

#include "minnas.h"
#include "backend.h"
#include "cas.h"
#include "snapshot.h"
#include "vfs.h"
#include "branch.h"
#include "namespace.h"
#include "blob.h"
#include "sha256.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/* ---------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static bool is_minnas_repo(const char *path) {
    char *objects = path_join(path, "objects");
    bool ok = ensure_dir(objects) == 0;
    free(objects);
    return ok;
}

static int ensure_minnas_dirs(const char *root) {
    if (ensure_dir(root) != 0) return MINNAS_ERR;
    if (ensure_dir(path_join(root, "objects")) != 0) return MINNAS_ERR;
    if (ensure_dir(path_join(root, "refs/heads")) != 0) return MINNAS_ERR;
    if (ensure_dir(path_join(root, "logs/refs/heads")) != 0) return MINNAS_ERR;
    if (ensure_dir(path_join(root, "namespaces")) != 0) return MINNAS_ERR;
    return MINNAS_OK;
}

/* ---------------------------------------------------------------------
 * repo_init / repo_open
 * --------------------------------------------------------------------- */

Repo *repo_init(const char *path, const char *type) { fprintf(stderr,"DEBUG repo_init path=%s type=%s\n",path,type); 
    if (!path || !type) {
        errno = EINVAL;
        return NULL;
    }

    Repo *r = calloc(1, sizeof(Repo));
    if (!r) return NULL;

    r->root = xstrdup(path);
    r->backend_type = xstrdup(type);

    if (ensure_minnas_dirs(path) != MINNAS_OK) {
        goto fail;
    }

    /* Create backend */
    if (strcmp(type, "local") == 0) {
        r->backend = backend_local_create(path);
    } else if (strcmp(type, "memory") == 0) {
        r->backend = backend_memory_create();
    } else {
        fprintf(stderr, "minnas: unknown backend type '%s'\n", type);
        goto fail;
    }

    if (!r->backend) goto fail;

    r->cas = cas_create(r->backend);
    if (!r->cas) goto fail;

    r->branchmgr = branchmgr_create(path, r->cas);
    if (!r->branchmgr) goto fail;

    /* VFS starts empty (no tree yet) */
    r->vfs = vfs_create(r->cas, NULL);
    if (!r->vfs) goto fail;

    return r;

fail:
    if (r->vfs)        vfs_free(r->vfs);
    if (r->branchmgr)  branchmgr_free(r->branchmgr);
    if (r->cas)        cas_free(r->cas);
    if (r->backend)    backend_free(r->backend);
    free(r->root);
    free(r->backend_type);
    free(r);
    return NULL;
}

Repo *repo_open(const char *path) {
    if (!path || !is_minnas_repo(path)) {
        errno = ENOENT;
        return NULL;
    }
    return repo_init(path, "local");
}

void repo_free(Repo *r) {
    if (!r) return;
    if (r->vfs)        vfs_free(r->vfs);
    if (r->branchmgr)  branchmgr_free(r->branchmgr);
    if (r->cas)       cas_free(r->cas);
    if (r->backend)   backend_free(r->backend);
    free(r->root);
    free(r->backend_type);
    free(r);
}

/* ---------------------------------------------------------------------
 * repo_commit
 * --------------------------------------------------------------------- */

char *repo_commit(Repo *r, const char *message, const char *author) {
    if (!r || !message) {
        errno = EINVAL;
        return NULL;
    }

    /* Get parent commit SHA (current HEAD) */
    char *parent_sha = branchmgr_get_current_sha(r->branchmgr);

    /* Build tree JSON from VFS state */
    char *tree_json = vfs_commit(r->vfs);
    if (!tree_json) {
        free(parent_sha);
        errno = ENOMEM;
        return NULL;
    }

    /* Store tree in CAS */
    char *tree_sha = cas_store(r->cas, (uint8_t *)tree_json, strlen(tree_json));
    free(tree_json);
    if (!tree_sha) {
        free(parent_sha);
        return NULL;
    }

    /* Build snapshot/commit object */
    Snapshot *snap = snapshot_create(
        parent_sha,           /* parent SHA or NULL */
        tree_sha,            /* tree SHA */
        author  ? author  : "anonymous",
        message ? message : "(no message)"
    );
    free(tree_sha);
    if (!snap) {
        free(parent_sha);
        return NULL;
    }

    char *snap_json = snapshot_serialize(snap);
    snapshot_free(snap);
    if (!snap_json) {
        free(parent_sha);
        return NULL;
    }

    /* Store snapshot in CAS */
    char *snap_sha = cas_store(r->cas, (uint8_t *)snap_json, strlen(snap_json));
    free(snap_json);
    if (!snap_sha) {
        free(parent_sha);
        return NULL;
    }

    /* Update branch HEAD + reflog */
    branchmgr_update_head(r->branchmgr, snap_sha,
                          "commit", author, message);

    /* Reload VFS tree from snapshot for consistency */
    Snapshot *loaded = snapshot_get(r->cas, snap_sha);
    if (loaded) {
        Tree t = tree_parse(loaded->tree_json);
        /* Update VFS internal tree — reload from snapshot */
        (void)t; /* tree_parse returns a copy; VFS reload handled below */
        vfs_checkout(r->vfs, snap_sha);
        snapshot_free(loaded);
    }

    return snap_sha; /* caller frees */
}

/* ---------------------------------------------------------------------
 * repo_status — diff between VFS state and last commit
 * --------------------------------------------------------------------- */

RepoStatus *repo_status(Repo *r) {
    if (!r) return NULL;

    RepoStatus *s = calloc(1, sizeof(RepoStatus));
    if (!s) return NULL;

    /* TODO: compare current VFS tree vs HEAD tree
     * For now, report "clean" since we don't track working-tree changes */
    (void)r;
    return s;
}

void repo_status_free(RepoStatus *s) {
    if (!s) return;
    for (int i = 0; i < s->modified_count; i++) free(s->modified[i]);
    for (int i = 0; i < s->added_count;   i++) free(s->added[i]);
    for (int i = 0; i < s->deleted_count; i++) free(s->deleted[i]);
    free(s->modified);
    free(s->added);
    free(s->deleted);
    free(s);
}

/* ---------------------------------------------------------------------
 * repo_log
 * --------------------------------------------------------------------- */

RepoLogEntry **repo_log(Repo *r, int max_count, int *count) {
    if (!r || !count) return NULL;

    ReflogEntry **entries = branchmgr_get_reflog(r->branchmgr, max_count, count);
    if (!entries) return NULL;

    RepoLogEntry **result = calloc((size_t)*count, sizeof(RepoLogEntry *));
    if (!result) {
        reflog_free(entries, *count);
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < *count; i++) {
        result[i] = calloc(1, sizeof(RepoLogEntry));
        result[i]->sha     = xstrdup(entries[i]->new_sha);
        result[i]->message = xstrdup(entries[i]->message);
        result[i]->author  = xstrdup(entries[i]->author);
        result[i]->time    = entries[i]->time;
    }

    reflog_free(entries, *count);
    return result;
}

void repo_log_free(RepoLogEntry **entries, int count) {
    if (!entries) return;
    for (int i = 0; i < count; i++) {
        free(entries[i]->sha);
        free(entries[i]->message);
        free(entries[i]->author);
        free(entries[i]);
    }
    free(entries);
}

/* ---------------------------------------------------------------------
 * repo_list_snapshots
 * --------------------------------------------------------------------- */

Snapshot **repo_list_snapshots(Repo *r, int *count) {
    if (!r || !count) return NULL;

    int n = 0;
    char **shas = cas_list_all(r->cas, &n);
    if (!shas || n == 0) {
        *count = 0;
        return NULL;
    }

    Snapshot **snaps = calloc((size_t)n, sizeof(Snapshot *));
    if (!snaps) {
        for (int i = 0; i < n; i++) free(shas[i]);
        free(shas);
        *count = 0;
        return NULL;
    }

    int stored = 0;
    for (int i = 0; i < n; i++) {
        Snapshot *s = snapshot_get(r->cas, shas[i]);
        if (s) snaps[stored++] = s;
        free(shas[i]);
    }
    free(shas);
    *count = stored;
    return snaps;
}

int repo_checkout_snapshot(Repo *r, const char *sha) {
    if (!r || !sha) return MINNAS_ERR_INVALID;
    return vfs_checkout(r->vfs, sha) == 0 ? MINNAS_OK : MINNAS_ERR;
}

/* ---------------------------------------------------------------------
 * repo_diff
 * --------------------------------------------------------------------- */

static Tree load_tree(CAS *cas, const char *sha) {
    if (!sha) { Tree t = {0}; return t; }
    Snapshot *s = snapshot_get(cas, sha);
    if (!s) { Tree t = {0}; return t; }
    Tree t = tree_parse(s->tree_json);
    snapshot_free(s);
    return t;
}

Change **repo_diff(Repo *r, const char *sha1, const char *sha2, int *count) {
    if (!r || !count) return NULL;

    Tree t1 = load_tree(r->cas, sha1);
    Tree t2 = load_tree(r->cas, sha2);

    Change **changes = NULL;
    int n = 0;

    /* Files in t2 not in t1 → added */
    for (int i = 0; i < t2.count; i++) {
        bool found = false;
        for (int j = 0; j < t1.count; j++) {
            if (strcmp(t2.paths[i], t1.paths[j]) == 0) {
                found = true;
                if (strcmp(t2.shas[i], t1.shas[j]) != 0) {
                    /* Modified */
                    Change *c = calloc(1, sizeof(Change));
                    c->path   = xstrdup(t2.paths[i]);
                    c->status = 'M';
                    c->old_sha = xstrdup(t1.shas[j]);
                    c->new_sha = xstrdup(t2.shas[i]);
                    changes = realloc(changes, (size_t)(++n) * sizeof(Change *));
                    changes[n-1] = c;
                }
                break;
            }
        }
        if (!found) {
            Change *c = calloc(1, sizeof(Change));
            c->path   = xstrdup(t2.paths[i]);
            c->status = 'A';
            c->new_sha = xstrdup(t2.shas[i]);
            changes = realloc(changes, (size_t)(++n) * sizeof(Change *));
            changes[n-1] = c;
        }
    }

    /* Files in t1 not in t2 → deleted */
    for (int i = 0; i < t1.count; i++) {
        bool found = false;
        for (int j = 0; j < t2.count; j++) {
            if (strcmp(t1.paths[i], t2.paths[j]) == 0) { found = true; break; }
        }
        if (!found) {
            Change *c = calloc(1, sizeof(Change));
            c->path   = xstrdup(t1.paths[i]);
            c->status = 'D';
            c->old_sha = xstrdup(t1.shas[i]);
            changes = realloc(changes, (size_t)(++n) * sizeof(Change *));
            changes[n-1] = c;
        }
    }

    tree_free(t1);
    tree_free(t2);
    *count = n;
    return changes;
}

void repo_diff_free(Change **changes, int count) {
    if (!changes) return;
    for (int i = 0; i < count; i++) {
        free(changes[i]->path);
        free(changes[i]->old_sha);
        free(changes[i]->new_sha);
        free(changes[i]);
    }
    free(changes);
}

/* ---------------------------------------------------------------------
 * repo_gc
 * --------------------------------------------------------------------- */

int repo_gc(Repo *r) {
    if (!r) return MINNAS_ERR_INVALID;

    int freed = 0;
    int total = 0;
    char **all = cas_list_all(r->cas, &total);
    if (!all) return 0;

    /* Collect all referenced SHAs from all branches and snapshots */
    /* TODO: implement proper reachability GC */
    (void)r;
    (void)freed;

    for (int i = 0; i < total; i++) free(all[i]);
    free(all);
    return freed;
}

/* ---------------------------------------------------------------------
 * repo_stats
 * --------------------------------------------------------------------- */

void repo_stats(Repo *r, CAS_Stats *cs, int *snap_count, int *branch_count) {
    if (!r) return;
    if (cs) cas_stats(r->cas, cs);
    if (snap_count) {
        *snap_count = 0;
        Snapshot **snaps = repo_list_snapshots(r, snap_count);
        for (int i = 0; i < *snap_count; i++) snapshot_free(snaps[i]);
        free(snaps);
    }
    if (branch_count) {
        *branch_count = 0;
        char **br = branchmgr_list_branches(r->branchmgr, branch_count);
        for (int i = 0; i < *branch_count * 3; i++) free(br[i]);
        free(br);
    }
}
