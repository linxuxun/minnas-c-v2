/* vfs.h - Virtual File System layer */
#ifndef VFS_H
#define VFS_H
#include "cas.h"
#include "snapshot.h"
#include <sys/types.h>
#include <stdbool.h>

typedef struct VFS VFS;

VFS  *vfs_create(CAS *cas, Tree *initial_tree);
void  vfs_free(VFS *vfs);

/* File operations (POSIX-like, fd starts at 3) */
int   vfs_open(VFS *vfs, const char *path, const char *mode); /* "r", "w", "a" */
int   vfs_close(VFS *vfs, int fd);
int   vfs_read(VFS *vfs, int fd, uint8_t *buf, int n);
int   vfs_write(VFS *vfs, int fd, const uint8_t *buf, int n);
int64_t vfs_lseek(VFS *vfs, int fd, int64_t off, int whence);
int64_t vfs_tell(VFS *vfs, int fd);

int   vfs_truncate(VFS *vfs, const char *path, off_t sz);
int   vfs_rm(VFS *vfs, const char *path);
bool  vfs_exists(VFS *vfs, const char *path);

char **vfs_listdir(VFS *vfs, const char *path, int *count); /* caller frees */
void   vfs_listdir_free(char **entries, int count);

typedef struct VFS_Stat {
    bool    exists;
    char    sha[65];
    off_t   size;
} VFS_Stat;

int vfs_stat(VFS *vfs, const char *path, VFS_Stat *st);
char *vfs_commit(VFS *vfs); /* Build tree JSON from current VFS state (caller frees) */
int   vfs_checkout(VFS *vfs, const char *snapshot_sha); /* Load snapshot into VFS */

#endif
