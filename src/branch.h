/* branch.h - Branch + reflog manager */
#ifndef BRANCH_H
#define BRANCH_H
#include "cas.h"
#include "snapshot.h"
#include <time.h>

typedef struct BranchMgr BranchMgr;

typedef struct ReflogEntry {
    char   *old_sha;
    char   *new_sha;
    char   *action;   /* "commit", "checkout", "reset"... */
    char   *author;
    char   *message;
    time_t  time;
} ReflogEntry;

BranchMgr *branchmgr_create(const char *repo_root, CAS *cas);
void       branchmgr_free(BranchMgr *bm);

char *branchmgr_get_current_sha(BranchMgr *bm); /* caller frees */
char *branchmgr_get_current_branch(BranchMgr *bm); /* caller frees */

int   branchmgr_create_branch(BranchMgr *bm, const char *name, const char *sha);
int   branchmgr_delete_branch(BranchMgr *bm, const char *name);
int   branchmgr_checkout(BranchMgr *bm, const char *branch_name);
int   branchmgr_update_head(BranchMgr *bm, const char *sha,
                            const char *action, const char *author, const char *msg);

/** branchmgr_list_branches - Returns [name, sha, is_current(int)] triplets (caller frees) */
char **branchmgr_list_branches(BranchMgr *bm, int *count);

ReflogEntry **branchmgr_get_reflog(BranchMgr *bm, int max_count, int *count);
void          reflog_free(ReflogEntry **entries, int count);

#endif
