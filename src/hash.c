#define _DEFAULT_SOURCE
/*
 * hash.c - Simple hash table for string keys
 */

#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HT_LOAD_FACTOR 0.75

typedef struct HTEntry {
    char *key;
    void *value;
    struct HTEntry *next;
} HTEntry;

struct HashTable {
    HTEntry **buckets;
    size_t capacity;
    size_t count;
};

static size_t ht_hash(const char *key, size_t capacity) {
    size_t h = 5381;
    while (*key) {
        h = ((h << 5) + h) + (unsigned char)(*key);
        key++;
    }
    return h % capacity;
}

static bool ht_resize(HashTable *ht, size_t new_cap) {
    HTEntry **old_buckets = ht->buckets;
    size_t old_cap = ht->capacity;
    HTEntry **new_buckets = calloc(new_cap, sizeof(HTEntry *));
    if (!new_buckets) return false;

    ht->buckets = new_buckets;
    ht->capacity = new_cap;

    for (size_t i = 0; i < old_cap; i++) {
        HTEntry *e = old_buckets[i];
        while (e) {
            HTEntry *next = e->next;
            size_t h = ht_hash(e->key, new_cap);
            e->next = new_buckets[h];
            new_buckets[h] = e;
            e = next;
        }
    }
    free(old_buckets);
    return true;
}

HashTable *ht_create(void) { fprintf(stderr,"DEBUG ht_create\n"); 
    HashTable *ht = calloc(1, sizeof(HashTable));
    if (!ht) return NULL;
    ht->capacity = 16;
    ht->buckets = calloc(ht->capacity, sizeof(HTEntry *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    return ht;
}

void ht_free(HashTable *ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->capacity; i++) {
        HTEntry *e = ht->buckets[i];
        while (e) {
            HTEntry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

int ht_set(HashTable *ht, const char *key, void *value) {
    if (!ht || !key) return -1;
    if ((double)ht->count / ht->capacity > HT_LOAD_FACTOR) {
        if (!ht_resize(ht, ht->capacity * 2)) return -1;
    }

    size_t h = ht_hash(key, ht->capacity);
    HTEntry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;
            return 0;
        }
        e = e->next;
    }

    HTEntry *new_e = malloc(sizeof(HTEntry));
    if (!new_e) return -1;
    new_e->key = strdup(key);
    new_e->value = value;
    new_e->next = ht->buckets[h];
    ht->buckets[h] = new_e;
    ht->count++;
    return 0;
}

void *ht_get(HashTable *ht, const char *key) {
    if (!ht || !key) return NULL;
    size_t h = ht_hash(key, ht->capacity);
    HTEntry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) return e->value;
        e = e->next;
    }
    return NULL;
}

bool ht_has(HashTable *ht, const char *key) {
    return ht_get(ht, key) != NULL;
}

int ht_del(HashTable *ht, const char *key) {
    if (!ht || !key) return -1;
    size_t h = ht_hash(key, ht->capacity);
    HTEntry *prev = NULL;
    HTEntry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            if (prev) prev->next = e->next;
            else ht->buckets[h] = e->next;
            free(e->key);
            free(e);
            ht->count--;
            return 0;
        }
        prev = e;
        e = e->next;
    }
    return -1;
}

void ht_foreach(HashTable *ht, HT_IterFn fn, void *udata) {
    if (!ht || !fn) return;
    for (size_t i = 0; i < ht->capacity; i++) {
        HTEntry *e = ht->buckets[i];
        while (e) {
            HTEntry *next = e->next;
            if (!fn(e->key, e->value, udata)) return;
            e = next;
        }
    }
}

int ht_size(const HashTable *ht) {
    return ht ? (int)ht->count : 0;
}

bool ht_is_empty(const HashTable *ht) {
    return ht && ht->count == 0;
}

char **ht_keys(const HashTable *ht, int *count) {
    *count = 0;
    if (!ht || ht->count == 0) {
        char **r = malloc(sizeof(char *));
        r[0] = NULL;
        return r;
    }
    char **keys = malloc(sizeof(char *) * (ht->count + 1));
    int idx = 0;
    for (size_t i = 0; i < ht->capacity; i++) {
        HTEntry *e = ht->buckets[i];
        while (e) {
            keys[idx++] = e->key;
            e = e->next;
        }
    }
    keys[idx] = NULL;
    *count = idx;
    return keys;
}
