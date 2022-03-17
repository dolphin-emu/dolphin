/* mz_strm_mem.h -- Stream for memory access
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_STREAM_MEM_H
#define MZ_STREAM_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

int32_t mz_stream_mem_open(void *stream, const char *filename, int32_t mode);
int32_t mz_stream_mem_is_open(void *stream);
int32_t mz_stream_mem_read(void *stream, void *buf, int32_t size);
int32_t mz_stream_mem_write(void *stream, const void *buf, int32_t size);
int64_t mz_stream_mem_tell(void *stream);
int32_t mz_stream_mem_seek(void *stream, int64_t offset, int32_t origin);
int32_t mz_stream_mem_close(void *stream);
int32_t mz_stream_mem_error(void *stream);

void    mz_stream_mem_set_buffer(void *stream, void *buf, int32_t size);
int32_t mz_stream_mem_get_buffer(void *stream, const void **buf);
int32_t mz_stream_mem_get_buffer_at(void *stream, int64_t position, const void **buf);
int32_t mz_stream_mem_get_buffer_at_current(void *stream, const void **buf);
void    mz_stream_mem_get_buffer_length(void *stream, int32_t *length);
void    mz_stream_mem_set_buffer_limit(void *stream, int32_t limit);
void    mz_stream_mem_set_grow_size(void *stream, int32_t grow_size);

void*   mz_stream_mem_create(void **stream);
void    mz_stream_mem_delete(void **stream);

void*   mz_stream_mem_get_interface(void);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
