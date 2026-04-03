#define _DEFAULT_SOURCE
#include "branch.h"
#include "utils.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
struct BranchMgr { char *root; char *refs_dir; char *logs_dir; CAS *cas; };
static char *jp(const char *a, const char *b){size_t al=strlen(a),bl=strlen(b);char*r=malloc(al+bl+2);sprintf(r,"%s/%s",a,b);return r;}
static int edir(const char*p){char t[1024];for(size_t i=0;p[i];i++){if(p[i]=='/'){memcpy(t,p,i);t[i]='\0';if(mkdir(t,0755)!=0&&errno!=EEXIST)return-1;}}return(mkdir(p,0755)!=0&&errno!=EEXIST)?-1:0;}
static void init_refs(BranchMgr*bm){edir(bm->root);edir(bm->refs_dir);edir(bm->logs_dir);char*hp=jp(bm->root,"HEAD");FILE*f=fopen(hp,"a");if(f){if(ftell(f)==0)fprintf(f,"ref: refs/heads/main\n");fclose(f);}free(hp);char*mp=jp(bm->refs_dir,"main");FILE*mb=fopen(mp,"a");if(mb){if(ftell(mb)==0)fprintf(mb,"\n");fclose(mb);}free(mp);char*ml=jp(bm->logs_dir,"main");FILE*mj=fopen(ml,"a");if(mj)fclose(mj);free(ml);}
static char *rfl(const char*p){FILE*f=fopen(p,"r");if(!f)return NULL;char l[256]={0};char*r=NULL;if(fgets(l,sizeof(l),f)){char*e=l+strlen(l)-1;while(e>l&&(unsigned char)*e<=' ')e--;e[1]='\0';char*s=l;while(*s&&(unsigned char)*s<=' ')s++;r=xstrdup(*s?s:"");}fclose(f);return r;}
static void wref(const char*p,const char*s){FILE*f=fopen(p,"w");if(f){fprintf(f,"%s\n",s?s:"");fclose(f);}}
BranchMgr*branchmgr_create(const char*root,CAS*cas){fprintf(stderr,"DEBUG: branchmgr_create root=%s\n",root); BranchMgr*bm=calloc(1,sizeof(BranchMgr));bm->root=xstrdup(root);bm->refs_dir=jp(root,"refs/heads");bm->logs_dir=jp(root,"logs/refs/heads");bm->cas=cas;init_refs(bm);return bm;}
void branchmgr_free(BranchMgr*bm){if(!bm)return;free(bm->root);free(bm->refs_dir);free(bm->logs_dir);free(bm);}
char *branchmgr_get_current_sha(BranchMgr*bm){char*hp=jp(bm->root,"HEAD");char*ref=rfl(hp);free(hp);if(!ref||!*ref){free(ref);return NULL;}if(strncmp(ref,"ref: ",5)==0){char*r=jp(bm->root,ref+5);char*s=rfl(r);free(r);free(ref);return s;}return ref;}
char *branchmgr_get_current_branch(BranchMgr*bm){char*hp=jp(bm->root,"HEAD");char*ref=rfl(hp);free(hp);if(!ref)return NULL;if(strncmp(ref,"ref: refs/heads/",16)==0){char*n=xstrdup(ref+16);free(ref);return n;}free(ref);return NULL;}
int branchmgr_create_branch(BranchMgr*bm,const char*name,const char*sha){if(!bm||!name)return-1;char*p=jp(bm->refs_dir,name);wref(p, sha ? sha : "");free(p);char*lp=jp(bm->logs_dir,name);FILE*l=fopen(lp,"a");if(l)fclose(l);free(lp);return 0;}
int branchmgr_delete_branch(BranchMgr*bm,const char*name){if(!bm||!name)return-1;char*p=jp(bm->refs_dir,name);int r=unlink(p);free(p);char*lp=jp(bm->logs_dir,name);unlink(lp);free(lp);return(r==0||errno==ENOENT)?0:-1;}
int branchmgr_checkout(BranchMgr*bm,const char*name){if(!bm||!name)return-1;char*hp=jp(bm->root,"HEAD");FILE*f=fopen(hp,"w");free(hp);if(!f)return-1;fprintf(f,"ref: refs/heads/%s\n",name);fclose(f);return 0;}
int branchmgr_update_head(BranchMgr*bm,const char*sha,const char*action,const char*author,const char*msg){if(!bm||!sha)return-1;char*hp=jp(bm->root,"HEAD");char*ref=rfl(hp);free(hp);if(!ref||!*ref){free(ref);return-1;}char*rp=NULL;char*old=NULL;if(strncmp(ref,"ref: ",5)==0){rp=jp(bm->root,ref+5);old=rfl(rp);wref(rp,sha);}free(ref);if(rp){const char*lr=strstr(rp,"logs/refs/heads/");if(lr){char*lp=jp(bm->root,lr);FILE*l=fopen(lp,"a");if(l){fprintf(l,"%s %s %s %s %ld %s\n",old?old:"",sha,action?action:"commit",author?author:"anon",(long)time(NULL),msg?msg:"");fclose(l);}free(lp);}free(rp);}free(old);return 0;}
char **branchmgr_list_branches(BranchMgr*bm,int*count){if(!bm||!count)return NULL;char*cur=branchmgr_get_current_branch(bm);char**r=NULL;int n=0;DIR*d=opendir(bm->refs_dir);if(!d){*count=0;return NULL;}struct dirent*ent;while((ent=readdir(d))){if(ent->d_name[0]=='.'||ent->d_type!=DT_DIR)continue;char*p=jp(bm->refs_dir,ent->d_name);char*s=rfl(p);free(p);int ic=cur&&strcmp(ent->d_name,cur)==0;r=realloc(r,(size_t)(n+3)*sizeof(char*));r[n++]=xstrdup(ent->d_name);r[n++]=s?s:xstrdup("");{char b[16];snprintf(b,sizeof(b),"%d",ic);r[n++]=xstrdup(b);}}closedir(d);free(cur);*count=n;return r;}
ReflogEntry**branchmgr_get_reflog(BranchMgr*bm,int max,int*count){if(!bm||!count)return NULL;*count=0;char*br=branchmgr_get_current_branch(bm);if(!br)return NULL;char*lp=jp(bm->logs_dir,br);free(br);FILE*f=fopen(lp,"r");free(lp);if(!f)return NULL;ReflogEntry**e=NULL;char l[1024];while(*count<max&&fgets(l,sizeof(l),f)){ReflogEntry*x=calloc(1,sizeof(ReflogEntry));char*p=l;char*fi[6]={0};int fii=0;while(*p&&fii<6){while(*p==' ')p++;char*s=p;while(*p&&*p!=' '&&*p!='\n')p++;size_t len=(size_t)(p-s);if(len>0){fi[fii]=malloc(len+1);memcpy(fi[fii],s,len);fi[fii++][len]='\0';}if(*p=='\n'||!*p)break;p++;}if(fi[0])x->old_sha=fi[0];if(fi[1])x->new_sha=fi[1];if(fi[2])x->action=fi[2];if(fi[3])x->author=fi[3];if(fi[4])x->time=(time_t)strtod(fi[4],NULL);if(fi[5])x->message=fi[5];e=realloc(e,(size_t)(++(*count))*sizeof(void*));e[*count-1]=x;}fclose(f);return e;}
void reflog_free(ReflogEntry**e,int n){if(!e)return;for(int i=0;i<n;i++){free(e[i]->old_sha);free(e[i]->new_sha);free(e[i]->action);free(e[i]->author);free(e[i]->message);free(e[i]);}free(e);}
