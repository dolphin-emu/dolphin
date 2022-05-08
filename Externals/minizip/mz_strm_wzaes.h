/* mz_strm_wzaes.h -- Stream for WinZIP AES encryption
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_STREAM_WZAES_SHA1_H
#define MZ_STREAM_WZAES_SHA1_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

int32_t mz_stream_wzaes_open(void *stream, const char *filename, int32_t mode);
int32_t mz_stream_wzaes_is_open(void *stream);
int32_t mz_stream_wzaes_read(void *stream, void *buf, int32_t size);
int32_t mz_stream_wzaes_write(void *stream, const void *buf, int32_t size);
int64_t mz_stream_wzaes_tell(void *stream);
int32_t mz_stream_wzaes_seek(void *stream, int64_t offset, int32_t origin);
int32_t mz_stream_wzaes_close(void *stream);
int32_t mz_stream_wzaes_error(void *stream);

void    mz_stream_wzaes_set_password(void *stream, const char *password);
void    mz_stream_wzaes_set_encryption_mode(void *stream, int16_t encryption_mode);

int32_t mz_stream_wzaes_get_prop_int64(void *stream, int32_t prop, int64_t *value);
int32_t mz_stream_wzaes_set_prop_int64(void *stream, int32_t prop, int64_t value);

void*   mz_stream_wzaes_create(void **stream);
void    mz_stream_wzaes_delete(void **stream);

void*   mz_stream_wzaes_get_interface(void);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
