/* mz_strm_libcomp.c -- Stream for apple compression
   part of the minizip-ng project

   Copyright (C) Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_libcomp.h"

#include <compression.h>

/***************************************************************************/

static mz_stream_vtbl mz_stream_libcomp_vtbl = {
    mz_stream_libcomp_open,
    mz_stream_libcomp_is_open,
    mz_stream_libcomp_read,
    mz_stream_libcomp_write,
    mz_stream_libcomp_tell,
    mz_stream_libcomp_seek,
    mz_stream_libcomp_close,
    mz_stream_libcomp_error,
    mz_stream_libcomp_create,
    mz_stream_libcomp_delete,
    mz_stream_libcomp_get_prop_int64,
    mz_stream_libcomp_set_prop_int64
};

/***************************************************************************/

typedef struct mz_stream_libcomp_s {
    mz_stream   stream;
    compression_stream
                cstream;
    uint8_t     buffer[INT16_MAX];
    int32_t     buffer_len;
    int64_t     total_in;
    int64_t     total_out;
    int64_t     max_total_in;
    int8_t      initialized;
    int32_t     mode;
    int32_t     error;
    int16_t     method;
} mz_stream_libcomp;

/***************************************************************************/

int32_t mz_stream_libcomp_open(void *stream, const char *path, int32_t mode) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    int32_t err = 0;
    int16_t operation = 0;
    compression_algorithm algorithm = 0;

    MZ_UNUSED(path);

    if (libcomp->method == 0)
        return MZ_PARAM_ERROR;

    libcomp->total_in = 0;
    libcomp->total_out = 0;

    if (mode & MZ_OPEN_MODE_WRITE) {
#ifdef MZ_ZIP_NO_COMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        operation = COMPRESSION_STREAM_ENCODE;
#endif
    } else if (mode & MZ_OPEN_MODE_READ) {
#ifdef MZ_ZIP_NO_DECOMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        operation = COMPRESSION_STREAM_DECODE;
#endif
    }

    if (libcomp->method == MZ_COMPRESS_METHOD_DEFLATE)
        algorithm = COMPRESSION_ZLIB;
    else if (libcomp->method == MZ_COMPRESS_METHOD_XZ)
        algorithm = COMPRESSION_LZMA;
    else
        return MZ_SUPPORT_ERROR;

    err = compression_stream_init(&libcomp->cstream, (compression_stream_operation)operation, algorithm);

    if (err == COMPRESSION_STATUS_ERROR) {
        libcomp->error = err;
        return MZ_OPEN_ERROR;
    }

    libcomp->initialized = 1;
    libcomp->mode = mode;
    return MZ_OK;
}

int32_t mz_stream_libcomp_is_open(void *stream) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    if (libcomp->initialized != 1)
        return MZ_OPEN_ERROR;
    return MZ_OK;
}

int32_t mz_stream_libcomp_read(void *stream, void *buf, int32_t size) {
#ifdef MZ_ZIP_NO_DECOMPRESSION
    MZ_UNUSED(stream);
    MZ_UNUSED(buf);
    MZ_UNUSED(size);
    return MZ_SUPPORT_ERROR;
#else
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    uint64_t total_in_before = 0;
    uint64_t total_in_after = 0;
    uint64_t total_out_before = 0;
    uint64_t total_out_after = 0;
    int32_t total_in = 0;
    int32_t total_out = 0;
    int32_t in_bytes = 0;
    int32_t out_bytes = 0;
    int32_t bytes_to_read = sizeof(libcomp->buffer);
    int32_t read = 0;
    int32_t err = MZ_OK;
    int16_t flags = 0;

    libcomp->cstream.dst_ptr = buf;
    libcomp->cstream.dst_size = (size_t)size;

    do {
        if (libcomp->cstream.src_size == 0) {
            if (libcomp->max_total_in > 0) {
                if ((int64_t)bytes_to_read > (libcomp->max_total_in - libcomp->total_in))
                    bytes_to_read = (int32_t)(libcomp->max_total_in - libcomp->total_in);
            }

            read = mz_stream_read(libcomp->stream.base, libcomp->buffer, bytes_to_read);

            if (read < 0)
                return read;
            if (read == 0)
                flags = COMPRESSION_STREAM_FINALIZE;

            libcomp->cstream.src_ptr = libcomp->buffer;
            libcomp->cstream.src_size = (size_t)read;
        }

        total_in_before = libcomp->cstream.src_size;
        total_out_before = libcomp->cstream.dst_size;

        err = compression_stream_process(&libcomp->cstream, flags);
        if (err == COMPRESSION_STATUS_ERROR) {
            libcomp->error = err;
            break;
        }

        total_in_after = libcomp->cstream.src_size;
        total_out_after = libcomp->cstream.dst_size;

        in_bytes = (int32_t)(total_in_before - total_in_after);
        out_bytes = (int32_t)(total_out_before - total_out_after);

        total_in += in_bytes;
        total_out += out_bytes;

        libcomp->total_in += in_bytes;
        libcomp->total_out += out_bytes;

        if (err == COMPRESSION_STATUS_END)
            break;
        if (err != COMPRESSION_STATUS_OK) {
            libcomp->error = err;
            break;
        }
    } while (libcomp->cstream.dst_size > 0);

    if (libcomp->error != 0)
        return MZ_DATA_ERROR;

    return total_out;
#endif
}

static int32_t mz_stream_libcomp_flush(void *stream) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    if (mz_stream_write(libcomp->stream.base, libcomp->buffer, libcomp->buffer_len) != libcomp->buffer_len)
        return MZ_WRITE_ERROR;
    return MZ_OK;
}

static int32_t mz_stream_libcomp_deflate(void *stream, int flush) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    uint64_t total_out_before = 0;
    uint64_t total_out_after = 0;
    uint32_t out_bytes = 0;
    int32_t err = MZ_OK;

    do {
        if (libcomp->cstream.dst_size == 0) {
            err = mz_stream_libcomp_flush(libcomp);
            if (err != MZ_OK) {
                libcomp->error = err;
                return err;
            }

            libcomp->cstream.dst_size = sizeof(libcomp->buffer);
            libcomp->cstream.dst_ptr = libcomp->buffer;

            libcomp->buffer_len = 0;
        }

        total_out_before = libcomp->cstream.dst_size;
        err = compression_stream_process(&libcomp->cstream, flush);
        total_out_after = libcomp->cstream.dst_size;

        out_bytes = (uint32_t)(total_out_before - total_out_after);

        libcomp->buffer_len += out_bytes;
        libcomp->total_out += out_bytes;

        if (err == COMPRESSION_STATUS_END)
            break;
        if (err != COMPRESSION_STATUS_OK) {
            libcomp->error = err;
            return MZ_DATA_ERROR;
        }
    } while ((libcomp->cstream.src_size > 0) || (flush == COMPRESSION_STREAM_FINALIZE && err == COMPRESSION_STATUS_OK));

    return MZ_OK;
}

int32_t mz_stream_libcomp_write(void *stream, const void *buf, int32_t size) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    int32_t err = size;

#ifdef MZ_ZIP_NO_COMPRESSION
    MZ_UNUSED(libcomp);
    err = MZ_SUPPORT_ERROR;
#else
    libcomp->cstream.src_ptr = buf;
    libcomp->cstream.src_size = (size_t)size;

    mz_stream_libcomp_deflate(stream, 0);

    libcomp->total_in += size;
#endif
    return err;
}

int64_t mz_stream_libcomp_tell(void *stream) {
    MZ_UNUSED(stream);

    return MZ_TELL_ERROR;
}

int32_t mz_stream_libcomp_seek(void *stream, int64_t offset, int32_t origin) {
    MZ_UNUSED(stream);
    MZ_UNUSED(offset);
    MZ_UNUSED(origin);

    return MZ_SEEK_ERROR;
}

int32_t mz_stream_libcomp_close(void *stream) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;

    if (libcomp->mode & MZ_OPEN_MODE_WRITE) {
#ifdef MZ_ZIP_NO_COMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        mz_stream_libcomp_deflate(stream, COMPRESSION_STREAM_FINALIZE);
        mz_stream_libcomp_flush(stream);
#endif
    } else if (libcomp->mode & MZ_OPEN_MODE_READ) {
#ifdef MZ_ZIP_NO_DECOMPRESSION
        return MZ_SUPPORT_ERROR;
#endif
    }

    compression_stream_destroy(&libcomp->cstream);

    libcomp->initialized = 0;

    if (libcomp->error != MZ_OK)
        return MZ_CLOSE_ERROR;
    return MZ_OK;
}

int32_t mz_stream_libcomp_error(void *stream) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    return libcomp->error;
}

int32_t mz_stream_libcomp_get_prop_int64(void *stream, int32_t prop, int64_t *value) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_TOTAL_IN:
        *value = libcomp->total_in;
        break;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        *value = libcomp->max_total_in;
        break;
    case MZ_STREAM_PROP_TOTAL_OUT:
        *value = libcomp->total_out;
        break;
    case MZ_STREAM_PROP_HEADER_SIZE:
        *value = 0;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

int32_t mz_stream_libcomp_set_prop_int64(void *stream, int32_t prop, int64_t value) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_COMPRESS_METHOD:
        libcomp->method = (int16_t)value;
        break;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        libcomp->max_total_in = value;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

void *mz_stream_libcomp_create(void) {
    mz_stream_libcomp *libcomp = (mz_stream_libcomp *)calloc(1, sizeof(mz_stream_libcomp));
    if (libcomp)
        libcomp->stream.vtbl = &mz_stream_libcomp_vtbl;
    return libcomp;
}

void mz_stream_libcomp_delete(void **stream) {
    mz_stream_libcomp *libcomp = NULL;
    if (!stream)
        return;
    libcomp = (mz_stream_libcomp *)*stream;
    if (libcomp)
        free(libcomp);
    *stream = NULL;
}
