/* vfs.c - Virtual File System */
#include "vfs.h"
#include "utils.h"
#include "blob.h"
#include "sha256.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define MAX_OPEN 128
typedef struct VFile { char*path; char mode; bool modified; char*blob_sha; uint8_t*buf; size_t sz; size_t cap; size_t pos; bool closed; } VFile;
struct VFS { CAS*cas; Tree tree; VFile*files[MAX_OPEN]; int next_fd; };
static char *np(const char*p){while(*p=='/')p++;return(char*)p;}
static int ti(VFS*v,const char*p){char*s=np(p);for(int i=0;i<v->tree.count;i++)if(strcmp(v->tree.paths[i],s)==0)return i;return -1;}
static void ut(VFS*v,const char*p,const char*sha){char*s=np(p);int i=ti(v,s);if(i>=0){free(v->tree.shas[i]);v->tree.shas[i]=xstrdup(sha);}else{v->tree.paths=realloc(v->tree.paths,(size_t)(++v->tree.count)*sizeof(char*));v->tree.shas=realloc(v->tree.shas,(size_t)v->tree.count*sizeof(char*));v->tree.paths[v->tree.count-1]=xstrdup(s);v->tree.shas[v->tree.count-1]=xstrdup(sha);}}
VFS*vfs_create(CAS*cas,Tree*init){VFS*v=calloc(1,sizeof(VFS));v->cas=cas;if(init){v->tree.count=init->count;v->tree.paths=malloc((size_t)init->count*sizeof(char*));v->tree.shas=malloc((size_t)init->count*sizeof(char*));for(int i=0;i<init->count;i++){v->tree.paths[i]=xstrdup(init->paths[i]);v->tree.shas[i]=xstrdup(init->shas[i]);}}return v;}
void vfs_free(VFS*v){if(!v)return;for(int i=0;i<MAX_OPEN;i++)if(v->files[i]&&!v->files[i]->closed){vfs_close(v,i+3);free(v->files[i]);}tree_free(v->tree);free(v);}
static int afd(VFS*v,VFile*f){for(int i=0;i<MAX_OPEN;i++)if(!v->files[i]){v->files[i]=f;return i+3;}free(f);errno=EMFILE;return -1;}
static VFile*gf(VFS*v,int fd){if(fd<3||fd>=3+MAX_OPEN){errno=EBADF;return NULL;}VFile*f=v->files[fd-3];if(!f){errno=EBADF;return NULL;}return f;}
int vfs_open(VFS*v,const char*path,const char*mode){VFile*f=calloc(1,sizeof(VFile));f->path=xstrdup(np(path));f->mode=mode[0];f->modified=false;f->buf=malloc(4096);f->cap=4096;f->sz=0;f->pos=0;f->closed=false;f->blob_sha=NULL;int idx=ti(v,path);if(mode[0]=='w'){if(idx>=0)f->blob_sha=xstrdup(v->tree.shas[idx]);}else if(mode[0]=='a'){f->mode='a';if(idx>=0){f->blob_sha=xstrdup(v->tree.shas[idx]);uint8_t*d=NULL;size_t dl=0;if(cas_load(v->cas,v->tree.shas[idx],&d,&dl)==0){size_t x;uint8_t*in=blob_read(d,dl,&x);if(in){if(x>f->cap)f->buf=realloc(f->buf,f->cap=x*2);memcpy(f->buf,in,x);f->sz=x;free(in);}free(d);}}}else{f->mode='r';if(idx<0){free(f->path);free(f->buf);free(f);errno=ENOENT;return -1;}f->blob_sha=xstrdup(v->tree.shas[idx]);uint8_t*d=NULL;size_t dl=0;if(cas_load(v->cas,v->tree.shas[idx],&d,&dl)==0){size_t x;uint8_t*in=blob_read(d,dl,&x);if(in){if(x>f->cap)f->buf=realloc(f->buf,f->cap=x*2);memcpy(f->buf,in,x);f->sz=x;free(in);}free(d);}}return afd(v,f);}
int vfs_close(VFS*v,int fd){VFile*f=gf(v,fd);if(!f||f->closed){errno=EBADF;return -1;}f->closed=true;if(f->modified||f->sz>0){size_t bl;uint8_t*blob=blob_build(f->buf,f->sz,&bl);if(blob){char hx[65];uint8_t dig[32];sha256_hash(blob,bl,dig);sha256_hex_to(hx,dig);cas_store(v->cas,blob,bl);ut(v,f->path,hx);free(blob);}}v->files[fd-3]=NULL;free(f->path);free(f->blob_sha);free(f->buf);free(f);return 0;}
int vfs_read(VFS*v,int fd,uint8_t*buf,int n){VFile*f=gf(v,fd);if(!f)return-1;size_t rem=f->sz>f->pos?f->sz-f->pos:0;size_t r=(size_t)n<rem?(size_t)n:rem;memcpy(buf,f->buf+f->pos,r);f->pos+=r;return(int)r;}
int vfs_write(VFS*v,int fd,const uint8_t*buf,int n){VFile*f=gf(v,fd);if(!f)return-1;if(f->mode=='r'){errno=EBADF;return-1;}f->modified=true;if(f->mode=='a')f->pos=f->sz;size_t need=f->pos+(size_t)n;if(need>f->cap){size_t nc=need*2;uint8_t*nb=realloc(f->buf,nc);if(!nb)return-1;f->buf=nb;f->cap=nc;}memcpy(f->buf+f->pos,buf,(size_t)n);f->pos+=(size_t)n;if(f->pos>f->sz)f->sz=f->pos;return n;}
int64_t vfs_lseek(VFS*v,int fd,int64_t off,int whence){VFile*f=gf(v,fd);if(!f)return-1;size_t np=0;if(whence==0)np=(size_t)off;else if(whence==1)np=f->pos+(size_t)off;else if(whence==2)np=f->sz+(size_t)off;else{errno=EINVAL;return-1;}f->pos=np;return(int64_t)np;}
int64_t vfs_tell(VFS*v,int fd){VFile*f=gf(v,fd);return f?(int64_t)f->pos:-1;}
int vfs_truncate(VFS*v,const char*path,off_t sz){int fd=vfs_open(v,path,"w");if(fd<0)return-1;VFile*f=gf(v,fd);if(f&&(size_t)sz<f->sz){f->sz=(size_t)sz;if(f->pos>f->sz)f->pos=f->sz;f->modified=true;}vfs_close(v,fd);return 0;}
int vfs_rm(VFS*v,const char*p){char*s=np(p);for(int i=0;i<v->tree.count;i++)if(strcmp(v->tree.paths[i],s)==0){free(v->tree.paths[i]);free(v->tree.shas[i]);for(int j=i;j<v->tree.count-1;j++){v->tree.paths[j]=v->tree.paths[j+1];v->tree.shas[j]=v->tree.shas[j+1];}v->tree.count--;return 0;}errno=ENOENT;return-1;}
bool vfs_exists(VFS*v,const char*p){return ti(v,p)>=0;}
char **vfs_listdir(VFS*v,const char*path,int*cnt){char**e=NULL;int n=0;(void)path;for(int i=0;i<v->tree.count;i++)e=realloc(e,(size_t)(n+1)*sizeof(char*)),e[n++]=xstrdup(v->tree.paths[i]);*cnt=n;return e;}
void vfs_listdir_free(char**e,int n){for(int i=0;i<n;i++)free(e[i]);free(e);}
int vfs_stat(VFS*v,const char*p,VFS_Stat*st){memset(st,0,sizeof(*st));int i=ti(v,p);if(i<0){st->exists=false;return 0;}st->exists=true;strncpy(st->sha,v->tree.shas[i],64);uint8_t*d=NULL;size_t dl=0;if(cas_load(v->cas,v->tree.shas[i],&d,&dl)==0){size_t x;uint8_t*in=blob_read(d,dl,&x);st->size=(off_t)x;free(in);free(d);}return 0;}
char*vfs_commit(VFS*v){return tree_build_json(v->tree.paths,v->tree.shas,v->tree.count);}
int vfs_checkout(VFS*v,const char*sha){Snapshot*s=snapshot_get(v->cas,sha);if(!s){errno=ENOENT;return-1;}Tree t=tree_parse(s->tree_json);tree_free(v->tree);v->tree=t;snapshot_free(s);return 0;}
