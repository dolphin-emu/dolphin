/* mz_strm_lzma.h -- Stream for lzma inflate/deflate
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as lzma.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_STREAM_LZMA_H
#define MZ_STREAM_LZMA_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

int32_t mz_stream_lzma_open(void *stream, const char *filename, int32_t mode);
int32_t mz_stream_lzma_is_open(void *stream);
int32_t mz_stream_lzma_read(void *stream, void *buf, int32_t size);
int32_t mz_stream_lzma_write(void *stream, const void *buf, int32_t size);
int64_t mz_stream_lzma_tell(void *stream);
int32_t mz_stream_lzma_seek(void *stream, int64_t offset, int32_t origin);
int32_t mz_stream_lzma_close(void *stream);
int32_t mz_stream_lzma_error(void *stream);

int32_t mz_stream_lzma_get_prop_int64(void *stream, int32_t prop, int64_t *value);
int32_t mz_stream_lzma_set_prop_int64(void *stream, int32_t prop, int64_t value);

void*   mz_stream_lzma_create(void **stream);
void    mz_stream_lzma_delete(void **stream);

void*   mz_stream_lzma_get_interface(void);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
