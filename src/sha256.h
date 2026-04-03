#ifndef SHA256_H
#define SHA256_H
#include <stddef.h>
#include <stdint.h>
#define SHA256_HEX_LEN 64
#define SHA256_BIN_LEN 32
typedef struct SHA256Context { uint32_t state[8]; uint64_t bitlen; uint32_t datalen; uint8_t data[64]; } SHA256Context;
void sha256_init(SHA256Context *ctx);
void sha256_update(SHA256Context *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256Context *ctx, uint8_t *hash);
void sha256_hash(const uint8_t *data, size_t len, uint8_t *hash_out);
char *sha256_hex(const uint8_t *data, size_t len);
char *sha256_hex_from_bin(const uint8_t *hash_bin);
void sha256_hex_to(char out[65], const uint8_t hash_bin[32]);
void sha256_file(const char *path, uint8_t *hash_out);
char *sha256_file_hex(const char *path);
#endif
