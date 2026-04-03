#define _DEFAULT_SOURCE
#include <sys/stat.h>
#include "namespace.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
struct NamespaceMgr { char*root; char*ns_dir; char*current; };
static char*jp(const char*a,const char*b){size_t al=strlen(a),bl=strlen(b);char*r=malloc(al+bl+2);sprintf(r,"%s/%s",a,b);return r;}
static int edir(const char*p){char t[1024];for(size_t i=0;p[i];i++){if(p[i]=='/'){memcpy(t,p,i);t[i]='\0';if(mkdir(t,0755)!=0&&errno!=EEXIST)return-1;}}return(mkdir(p,0755)!=0&&errno!=EEXIST)?-1:0;}
static char*rf(const char*p){FILE*f=fopen(p,"r");if(!f)return xstrdup("default");char l[256]={0};char*r=NULL;if(fgets(l,sizeof(l),f)){char*e=l+strlen(l)-1;while(e>l&&(unsigned char)*e<=' ')e--;e[1]='\0';char*s=l;while(*s&&(unsigned char)*s<=' ')s++;r=xstrdup(*s?s:"default");}fclose(f);return r?r:xstrdup("default");}
static void wf(const char*p,const char*v){FILE*f=fopen(p,"w");if(f){fprintf(f,"%s\n",v?v:"");fclose(f);}}
NamespaceMgr*nsmgr_create(const char*repo_root,CAS*cas){(void)cas;NamespaceMgr*m=calloc(1,sizeof(NamespaceMgr));m->root=xstrdup(repo_root);m->ns_dir=jp(repo_root,"namespaces");edir(m->ns_dir);edir(jp(m->ns_dir,"default"));m->current=rf(jp(m->ns_dir,"current"));wf(jp(m->ns_dir,"current"),m->current);return m;}
void nsmgr_free(NamespaceMgr*m){if(!m)return;free(m->root);free(m->ns_dir);free(m->current);free(m);}
int nsmgr_create_ns(NamespaceMgr*m,const char*name){char*p=jp(m->ns_dir,name);int r=edir(p);free(p);return r;}
int nsmgr_delete_ns(NamespaceMgr*m,const char*name){if(!m||!name)return-1;if(strcmp(name,"default")==0)return-1;char*p=jp(m->ns_dir,name);int r=remove(p);free(p);return(r==0||errno==ENOENT)?0:-1;}
int nsmgr_switch(NamespaceMgr*m,const char*name){char*p=jp(m->ns_dir,name);FILE*f=fopen(p,"r");if(!f){free(p);errno=ENOENT;return-1;}free(p);free(m->current);m->current=xstrdup(name);wf(jp(m->ns_dir,"current"),name);return 0;}
char*nsmgr_current(NamespaceMgr*m){return xstrdup(m->current?m->current:"default");}
