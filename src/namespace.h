/* namespace.h - Namespace isolation */
#ifndef NAMESPACE_H
#define NAMESPACE_H
#include "cas.h"

typedef struct NamespaceMgr NamespaceMgr;

NamespaceMgr *nsmgr_create(const char *repo_root, CAS *cas);
void          nsmgr_free(NamespaceMgr *mgr);

int   nsmgr_create_ns(NamespaceMgr *mgr, const char *name);
int   nsmgr_delete_ns(NamespaceMgr *mgr, const char *name);
int   nsmgr_switch(NamespaceMgr *mgr, const char *name);
char *nsmgr_current(NamespaceMgr *mgr); /* caller frees */

#endif
