#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
char *path_join(const char *a, const char *b);
char *path_normalize(const char *path);
char *path_dirname(const char *path);
char *path_basename(const char *path);
int ensure_dir(const char *path);
int remove_dir_recursive(const char *path);
bool file_exists(const char *path);
int read_file(const char *path, char **out_data, size_t *out_len);
int read_binary_file(const char *path, uint8_t **out_data, size_t *out_len);
int write_file(const char *path, const char *data, size_t len);
int write_binary_file(const char *path, const uint8_t *data, size_t len);
int append_file(const char *path, const char *data, size_t len);
long file_size(const char *path);
int copy_file(const char *src, const char *dst);
char **strlist_append(char **list, int *count, const char *s);
void strlist_free(char **list, int count);
uint8_t *hex_to_bin(const char *hex, size_t *out_len);
char *bin_to_hex(const uint8_t *bin, size_t len);
char *strtrim(char *s);
const char *skip_ws(const char *s);
bool str_ends_with(const char *s, const char *suffix);

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
