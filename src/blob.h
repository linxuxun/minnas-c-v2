/* blob.h - Git-style blob (type header + raw data) */
#ifndef BLOB_H
#define BLOB_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * blob_build - Wrap raw data in a Git-style blob
 * "blob {size}\0{data...}"
 * @data:     Raw file bytes
 * @data_len: Byte length of data
 * @out_len:  [out] Length of returned blob
 * Returns blob bytes (caller frees), or NULL on error.
 */
uint8_t *blob_build(const uint8_t *data, size_t data_len, size_t *out_len);

/**
 * blob_read - Extract raw data from a blob
 * @blob:     Blob bytes (from blob_build)
 * @blob_len: Total blob length
 * @out_len:  [out] Length of extracted data
 * Returns raw data bytes (caller frees), or NULL if invalid.
 */
uint8_t *blob_read(const uint8_t *blob, size_t blob_len, size_t *out_len);

/**
 * blob_verify - Check blob integrity
 * Returns true if blob is structurally valid.
 */
bool blob_verify(const uint8_t *blob, size_t blob_len);

/** blob_describe - Human-readable blob summary (static buffer) */
const char *blob_describe(const uint8_t *blob, size_t blob_len);

#endif
