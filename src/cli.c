/* cli.c - MiniNAS command-line interface (v2) */
#include "minnas.h"
#include "backend.h"
#include "cas.h"
#include "snapshot.h"
#include "vfs.h"
#include "branch.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_RED   "\033[31m"
#define C_GRN   "\033[32m"
#define C_YEL   "\033[33m"
#define C_BLU   "\033[36m"
#define C_BLD   "\033[1m"
#define C_RST   "\033[0m"
#define C_OK    C_GRN "✓" C_RST
#define C_FAIL  C_RED "✗" C_RST

static Repo *active_repo = NULL;

static void open_repo(const char *path) {
    if (active_repo) { repo_free(active_repo); active_repo = NULL; }
    active_repo = repo_open(path);
    if (!active_repo) fprintf(stderr, C_FAIL " Not a MinNAS repository: %s\n", path);
}

/* Parse -p/--path flag from argv, return updated repo_path in out_path */
static const char *parse_repo_path(int argc, char **argv, char *out_path) {
    strcpy(out_path, ".");
    for (int i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--path") == 0) {
            strncpy(out_path, argv[i+1], 255);
            out_path[255] = '\0';
        }
    }
    return out_path;
}

static void cmd_init(int argc, char **argv) {
    char path[256]; parse_repo_path(argc, argv, path);
    const char *type = "local";
    for (int i = 0; i < argc; i++) {
        if ((strcmp(argv[i],"-b")==0 || strcmp(argv[i],"--backend")==0) && i+1 < argc)
            type = argv[++i];
    }
    Repo *r = repo_init(path, type);
    if (r) {
        printf(C_OK " Initialized MinNAS repository at %s\n", path);
        printf("  backend: " C_BLU "%s" C_RST "\n", type);
        repo_free(r);
    } else {
        fprintf(stderr, C_FAIL " Init failed\n");
    }
}

static void cmd_status(const char *repo_path) {
    open_repo(repo_path);
    if (!active_repo) return;
    printf(C_BLD "MinNAS Status" C_RST "\n");
    char *br = branchmgr_get_current_branch(active_repo->branchmgr);
    char *sh = branchmgr_get_current_sha(active_repo->branchmgr);
    printf("  branch:   " C_BLU "%s" C_RST "\n", br ? br : "(detached)");
    printf("  commit:   " C_BLU "%.8s" C_RST "\n", sh ? sh : "none");
    free(br); free(sh);
    RepoStatus *s = repo_status(active_repo);
    if (s) { repo_status_free(s); }
}

static void cmd_commit(int argc, char **argv, const char *repo_path) {
    if (argc == 0) { fprintf(stderr, "Usage: commit <message>\n"); return; }
    open_repo(repo_path);
    if (!active_repo) return;
    const char *msg = argv[0];
    const char *author = "anonymous";
    for (int i = 1; i < argc; i++)
        if ((strcmp(argv[i],"-a")==0||strcmp(argv[i],"--author")==0) && i+1 < argc)
            author = argv[++i];
    char *sha = repo_commit(active_repo, msg, author);
    if (sha) {
        printf(C_OK " Committed: " C_BLU "%.8s" C_RST " — %s\n", sha, msg);
        free(sha);
    } else {
        fprintf(stderr, C_FAIL " Commit failed\n");
    }
}

static void cmd_log(int argc, char **argv, const char *repo_path) {
    int n = 10;
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i],"-n")==0 && i+1 < argc) n = atoi(argv[++i]);
    open_repo(repo_path);
    if (!active_repo) return;
    int count;
    RepoLogEntry **e = repo_log(active_repo, n, &count);
    for (int i = 0; i < count; i++) {
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", localtime(&e[i]->time));
        printf(C_BLU "%.8s" C_RST " %s " C_YEL "%s" C_RST "\n    %s\n",
               e[i]->sha, ts, e[i]->author, e[i]->message);
        free(e[i]->sha); free(e[i]->message); free(e[i]->author); free(e[i]);
    }
    free(e);
}

static void cmd_snapshot_list(const char *repo_path) {
    open_repo(repo_path);
    if (!active_repo) return;
    int count;
    Snapshot **snaps = repo_list_snapshots(active_repo, &count);
    if (!snaps || count == 0) { printf("(no snapshots)\n"); return; }
    for (int i = 0; i < count; i++) {
        printf(C_BLU "%.8s" C_RST " %s\n", snaps[i]->sha, snaps[i]->message);
        snapshot_free(snaps[i]);
    }
    free(snaps);
}

static void cmd_gc(const char *repo_path) {
    open_repo(repo_path);
    if (!active_repo) return;
    int freed = repo_gc(active_repo);
    printf(C_OK " GC freed %d object(s)\n", freed);
}

static void cmd_stats(const char *repo_path) {
    open_repo(repo_path);
    if (!active_repo) return;
    CAS_Stats cs; int sc = 0, bc = 0;
    repo_stats(active_repo, &cs, &sc, &bc);
    printf(C_BLD "Repository Stats" C_RST "\n");
    printf("  Objects:   %d\n", cs.object_count);
    printf("  Total size: %zu bytes\n", cs.total_size);
    printf("  Branches:   %d\n", bc);
    printf("  Snapshots:  %d\n", sc);
}

static void cmd_fs_ls(const char *repo_path, const char *path) {
    open_repo(repo_path);
    if (!active_repo) return;
    int count;
    char **e = vfs_listdir(active_repo->vfs, path ? path : "/", &count);
    for (int i = 0; i < count; i++) { printf("  %s\n", e[i]); free(e[i]); }
    free(e);
}

static void cmd_fs_cat(const char *repo_path, const char *path) {
    open_repo(repo_path);
    if (!active_repo) return;
    int fd = vfs_open(active_repo->vfs, path, "r");
    if (fd < 0) { fprintf(stderr, C_FAIL " Cannot open: %s\n", path); return; }
    uint8_t buf[4096]; int n;
    while ((n = vfs_read(active_repo->vfs, fd, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, (size_t)n, stdout);
    vfs_close(active_repo->vfs, fd);
}

static void cmd_fs_write(const char *repo_path, const char *path, const char *content) {
    open_repo(repo_path);
    if (!active_repo) return;
    int fd = vfs_open(active_repo->vfs, path, "w");
    if (fd < 0) { fprintf(stderr, C_FAIL " Cannot open: %s\n", path); return; }
    vfs_write(active_repo->vfs, fd, (uint8_t*)content, (int)strlen(content));
    vfs_close(active_repo->vfs, fd);
    printf(C_OK " Wrote %zu bytes to %s\n", strlen(content), path);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf(C_BLD "MiniNAS v2" C_RST " — Git-style CAS file storage\n");
        printf("Usage: minnas [-p <path>] <command> [args]\n");
        return 0;
    }
    /* Global -p flag */
    char repo_path[256] = ".";
    for (int i = 1; i < argc - 1; i++) {
        if ((strcmp(argv[i],"-p")==0 || strcmp(argv[i],"--path")==0)) {
            strncpy(repo_path, argv[i+1], 255); repo_path[255] = '\0';
        }
    }
    /* Remove -p flags from argv for command parsing */
    char *nargv[64]; int nargc = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i],"-p")==0 || strcmp(argv[i],"--path")==0) { i++; continue; }
        nargv[nargc++] = argv[i];
    }
    if (nargc == 0) { printf("No command.\n"); return 0; }

    if (strcmp(nargv[0],"init")==0) cmd_init(nargc, nargv);
    else if (strcmp(nargv[0],"status")==0) cmd_status(repo_path);
    else if (strcmp(nargv[0],"commit")==0) cmd_commit(nargc-1, nargv+1, repo_path);
    else if (strcmp(nargv[0],"log")==0) cmd_log(nargc-1, nargv+1, repo_path);
    else if (strcmp(nargv[0],"snapshot")==0) cmd_snapshot_list(repo_path);
    else if (strcmp(nargv[0],"gc")==0) cmd_gc(repo_path);
    else if (strcmp(nargv[0],"stats")==0) cmd_stats(repo_path);
    else if (strcmp(nargv[0],"fs")==0) {
        if (nargc >= 3 && strcmp(nargv[1],"ls")==0) cmd_fs_ls(repo_path, nargc>=3?nargv[2]:"/");
        else if (nargc >= 4 && strcmp(nargv[1],"cat")==0) cmd_fs_cat(repo_path, nargv[2]);
        else if (nargc >= 5 && strcmp(nargv[1],"write")==0) cmd_fs_write(repo_path, nargv[2], nargv[3]);
        else cmd_fs_ls(repo_path, "/");
    } else {
        fprintf(stderr, C_FAIL " Unknown command: %s\n", nargv[0]);
        return 1;
    }
    if (active_repo) repo_free(active_repo);
    return 0;
}
