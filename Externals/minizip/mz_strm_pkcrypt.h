/* mz_strm_pkcrypt.h -- Code for traditional PKWARE encryption
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_STREAM_PKCRYPT_H
#define MZ_STREAM_PKCRYPT_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

int32_t mz_stream_pkcrypt_open(void *stream, const char *filename, int32_t mode);
int32_t mz_stream_pkcrypt_is_open(void *stream);
int32_t mz_stream_pkcrypt_read(void *stream, void *buf, int32_t size);
int32_t mz_stream_pkcrypt_write(void *stream, const void *buf, int32_t size);
int64_t mz_stream_pkcrypt_tell(void *stream);
int32_t mz_stream_pkcrypt_seek(void *stream, int64_t offset, int32_t origin);
int32_t mz_stream_pkcrypt_close(void *stream);
int32_t mz_stream_pkcrypt_error(void *stream);

void    mz_stream_pkcrypt_set_password(void *stream, const char *password);
void    mz_stream_pkcrypt_set_verify(void *stream, uint8_t verify1, uint8_t verify2);
void    mz_stream_pkcrypt_get_verify(void *stream, uint8_t *verify1, uint8_t *verify2);
int32_t mz_stream_pkcrypt_get_prop_int64(void *stream, int32_t prop, int64_t *value);
int32_t mz_stream_pkcrypt_set_prop_int64(void *stream, int32_t prop, int64_t value);

void*   mz_stream_pkcrypt_create(void **stream);
void    mz_stream_pkcrypt_delete(void **stream);

void*   mz_stream_pkcrypt_get_interface(void);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
