/*
 * sha256.c - SHA-256 implementation in pure C
 * Based on FIPS 180-4 standard
 */

#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void sha256_init(SHA256Context *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitlen = 0;
    ctx->datalen = 0;
}

static void sha256_transform(SHA256Context *ctx) {
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2],
             d = ctx->state[3], e = ctx->state[4], f = ctx->state[5],
             g = ctx->state[6], h = ctx->state[7];
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)ctx->data[i*4] << 24 |
               (uint32_t)ctx->data[i*4+1] << 16 |
               (uint32_t)ctx->data[i*4+2] << 8 |
               (uint32_t)ctx->data[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    }
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + EP1(e) + CH(e, f, g) + K[i] + w[i];
        uint32_t t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
    ctx->datalen = 0;
}

void sha256_update(SHA256Context *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        ctx->bitlen += 8;
        if (ctx->datalen == 64) sha256_transform(ctx);
    }
}

void sha256_final(SHA256Context *ctx, uint8_t *hash) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += (uint64_t)ctx->datalen * 8;
    for (int j = 7; j >= 0; j--) ctx->data[63 - j] = (uint8_t)(ctx->bitlen >> (j * 8));
    sha256_transform(ctx);
    for (i = 0; i < 8; i++) {
        hash[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        hash[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i*4+3] = (uint8_t)ctx->state[i];
    }
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t *hash_out) {
    SHA256Context ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash_out);
}

char *sha256_hex(const uint8_t *data, size_t len) {
    uint8_t hash[32];
    sha256_hash(data, len, hash);
    return sha256_hex_from_bin(hash);
}

char *sha256_hex_from_bin(const uint8_t *hash_bin) {
    static const char hex_digits[] = "0123456789abcdef";
    char *hex = malloc(65);
    if (!hex) return NULL;
    for (int i = 0; i < 32; i++) {
        hex[i*2]   = hex_digits[hash_bin[i] >> 4];
        hex[i*2+1] = hex_digits[hash_bin[i] & 0xF];
    }
    hex[64] = '\0';
    return hex;
}

void sha256_file(const char *path, uint8_t *hash_out) {
    SHA256Context ctx;
    sha256_init(&ctx);
    FILE *f = fopen(path, "rb");
    if (!f) { perror("sha256_file fopen"); exit(1); }
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) sha256_update(&ctx, buf, n);
    sha256_final(&ctx, hash_out);
    fclose(f);
}

char *sha256_file_hex(const char *path) {
    uint8_t hash[32];
    sha256_file(path, hash);
    return sha256_hex_from_bin(hash);
}

void sha256_hex_to(char out[65], const uint8_t hash_bin[32]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i*2]   = hex[hash_bin[i] >> 4];
        out[i*2+1] = hex[hash_bin[i] & 0xF];
    }
    out[64] = '\0';
}
