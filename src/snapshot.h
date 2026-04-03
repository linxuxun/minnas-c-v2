/* snapshot.h - Snapshot (commit) object */
#ifndef SNAPSHOT_H
#define SNAPSHOT_H
#include <time.h>
#include <stdbool.h>

typedef struct CAS CAS;
typedef struct Snapshot {
    char   *sha;        /* SHA-256 of this snapshot JSON (owned) */
    char   *parent_sha; /* Parent snapshot SHA, or NULL (owned) */
    char   *tree_sha;   /* SHA-256 of the tree JSON (owned) */
    char   *author;     /* Author name (owned) */
    char   *message;    /* Commit message (owned) */
    time_t  timestamp;
    char   *tree_json;  /* Raw tree JSON for tree_parse (owned) */
} Snapshot;

Snapshot *snapshot_create(const char *parent_sha, const char *tree_sha,
                         const char *author, const char *message);
Snapshot *snapshot_get(CAS *cas, const char *sha_hex);
void      snapshot_free(Snapshot *s);
char     *snapshot_serialize(const Snapshot *s);
bool      snapshot_verify(const Snapshot *s);

/* Tree: array of {path, sha256} pairs */
typedef struct Tree {
    char **paths;
    char **shas;
    int    count;
} Tree;

Tree   tree_parse(const char *json);
void   tree_free(Tree t);
char  *tree_build_json(char **paths, char **shas, int count);

#endif
