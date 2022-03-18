/* mz_strm_bzip.h -- Stream for bzip inflate/deflate
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_STREAM_BZIP_H
#define MZ_STREAM_BZIP_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

int32_t mz_stream_bzip_open(void *stream, const char *filename, int32_t mode);
int32_t mz_stream_bzip_is_open(void *stream);
int32_t mz_stream_bzip_read(void *stream, void *buf, int32_t size);
int32_t mz_stream_bzip_write(void *stream, const void *buf, int32_t size);
int64_t mz_stream_bzip_tell(void *stream);
int32_t mz_stream_bzip_seek(void *stream, int64_t offset, int32_t origin);
int32_t mz_stream_bzip_close(void *stream);
int32_t mz_stream_bzip_error(void *stream);

int32_t mz_stream_bzip_get_prop_int64(void *stream, int32_t prop, int64_t *value);
int32_t mz_stream_bzip_set_prop_int64(void *stream, int32_t prop, int64_t value);

void*   mz_stream_bzip_create(void **stream);
void    mz_stream_bzip_delete(void **stream);

void*   mz_stream_bzip_get_interface(void);

void    bz_internal_error(int errcode);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
