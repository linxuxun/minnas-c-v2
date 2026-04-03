/*
 * hash.h - Simple hash table implementation for string keys
 * No external dependencies
 */

#ifndef HASH_H
#define HASH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct HashTable HashTable;

HashTable *ht_create(void);
void ht_free(HashTable *ht);

int ht_set(HashTable *ht, const char *key, void *value);
void *ht_get(HashTable *ht, const char *key);
bool ht_has(HashTable *ht, const char *key);
int ht_del(HashTable *ht, const char *key);

typedef bool (*HT_IterFn)(const char *key, void *value, void *udata);
void ht_foreach(HashTable *ht, HT_IterFn fn, void *udata);

int ht_size(const HashTable *ht);
bool ht_is_empty(const HashTable *ht);

char **ht_keys(const HashTable *ht, int *count);  // caller frees

#endif /* HASH_H */
