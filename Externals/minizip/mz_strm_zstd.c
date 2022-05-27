/* mz_strm_zstd.c -- Stream for zstd compress/decompress
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng
   Authors: Force Charlie
      https://github.com/fcharlie

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_zstd.h"

#include <zstd.h>

/***************************************************************************/

static mz_stream_vtbl mz_stream_zstd_vtbl = {
    mz_stream_zstd_open,
    mz_stream_zstd_is_open,
    mz_stream_zstd_read,
    mz_stream_zstd_write,
    mz_stream_zstd_tell,
    mz_stream_zstd_seek,
    mz_stream_zstd_close,
    mz_stream_zstd_error,
    mz_stream_zstd_create,
    mz_stream_zstd_delete,
    mz_stream_zstd_get_prop_int64,
    mz_stream_zstd_set_prop_int64
};

/***************************************************************************/

typedef struct mz_stream_zstd_s {
    mz_stream       stream;
    ZSTD_CStream    *zcstream;
    ZSTD_DStream    *zdstream;
    ZSTD_outBuffer  out;
    ZSTD_inBuffer   in;
    int32_t         mode;
    int32_t         error;
    uint8_t         buffer[INT16_MAX];
    int32_t         buffer_len;
    int64_t         total_in;
    int64_t         total_out;
    int64_t         max_total_in;
    int64_t         max_total_out;
    int8_t          initialized;
    uint32_t        preset;
} mz_stream_zstd;

/***************************************************************************/

int32_t mz_stream_zstd_open(void *stream, const char *path, int32_t mode) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;

    MZ_UNUSED(path);

    if (mode & MZ_OPEN_MODE_WRITE) {
#ifdef MZ_ZIP_NO_COMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        zstd->zcstream = ZSTD_createCStream();
        zstd->out.dst = zstd->buffer;
        zstd->out.size = sizeof(zstd->buffer);
        zstd->out.pos = 0;
#endif
    } else if (mode & MZ_OPEN_MODE_READ) {
#ifdef MZ_ZIP_NO_DECOMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        zstd->zdstream = ZSTD_createDStream();
        memset(&zstd->out, 0, sizeof(ZSTD_outBuffer));
#endif
    }

    memset(&zstd->in, 0, sizeof(ZSTD_inBuffer));

    zstd->initialized = 1;
    zstd->mode = mode;
    zstd->error = MZ_OK;

    return MZ_OK;
}

int32_t mz_stream_zstd_is_open(void *stream) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    if (zstd->initialized != 1)
        return MZ_OPEN_ERROR;
    return MZ_OK;
}

int32_t mz_stream_zstd_read(void *stream, void *buf, int32_t size) {
#ifdef MZ_ZIP_NO_DECOMPRESSION
    MZ_UNUSED(stream);
    MZ_UNUSED(buf);
    MZ_UNUSED(size);
    return MZ_SUPPORT_ERROR;
#else
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    uint64_t total_in_before = 0;
    uint64_t total_in_after = 0;
    uint64_t total_out_before = 0;
    uint64_t total_out_after = 0;
    int32_t total_in = 0;
    int32_t total_out = 0;
    int32_t in_bytes = 0;
    int32_t out_bytes = 0;
    int32_t bytes_to_read = sizeof(zstd->buffer);
    int32_t read = 0;
    size_t result = 0;

    zstd->out.dst = (void*)buf;
    zstd->out.size = (size_t)size;
    zstd->out.pos = 0;

    do {
        if (zstd->in.pos == zstd->in.size) {
            if (zstd->max_total_in > 0) {
                if ((int64_t)bytes_to_read > (zstd->max_total_in - zstd->total_in))
                    bytes_to_read = (int32_t)(zstd->max_total_in - zstd->total_in);
            }

            read = mz_stream_read(zstd->stream.base, zstd->buffer, bytes_to_read);

            if (read < 0)
                return read;

            zstd->in.src = (const void*)zstd->buffer;
            zstd->in.pos = 0;
            zstd->in.size = (size_t)read;
        }

        total_in_before = zstd->in.pos;
        total_out_before = zstd->out.pos;

        result = ZSTD_decompressStream(zstd->zdstream, &zstd->out, &zstd->in);

        if (ZSTD_isError(result)) {
            zstd->error = (int32_t)result;
            return MZ_DATA_ERROR;
        }

        total_in_after = zstd->in.pos;
        total_out_after = zstd->out.pos;
        if ((zstd->max_total_out != -1) && (int64_t)total_out_after > zstd->max_total_out)
            total_out_after = (uint64_t)zstd->max_total_out;

        in_bytes = (int32_t)(total_in_after - total_in_before);
        out_bytes = (int32_t)(total_out_after - total_out_before);

        total_in += in_bytes;
        total_out += out_bytes;

        zstd->total_in += in_bytes;
        zstd->total_out += out_bytes;

    } while ((zstd->in.size > 0 || out_bytes > 0) && (zstd->out.pos < zstd->out.size));

    return total_out;
#endif
}

#ifndef MZ_ZIP_NO_COMPRESSION
static int32_t mz_stream_zstd_flush(void *stream) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    if (mz_stream_write(zstd->stream.base, zstd->buffer, zstd->buffer_len) != zstd->buffer_len)
        return MZ_WRITE_ERROR;
    return MZ_OK;
}

static int32_t mz_stream_zstd_compress(void *stream, ZSTD_EndDirective flush) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    uint64_t total_out_before = 0;
    uint64_t total_out_after = 0;
    int32_t out_bytes = 0;
    size_t result = 0;
    int32_t err = 0;

    do {
        if (zstd->out.pos == zstd->out.size) {
            err = mz_stream_zstd_flush(zstd);
            if (err != MZ_OK)
                return err;

            zstd->out.dst = zstd->buffer;
            zstd->out.size = sizeof(zstd->buffer);
            zstd->out.pos = 0;

            zstd->buffer_len = 0;
        }

        total_out_before = zstd->out.pos;

        result = ZSTD_compressStream2(zstd->zcstream, &zstd->out, &zstd->in, flush);

        total_out_after = zstd->out.pos;

        out_bytes = (uint32_t)(total_out_after - total_out_before);

        zstd->buffer_len += out_bytes;
        zstd->total_out += out_bytes;

        if (ZSTD_isError(result)) {
            zstd->error = (int32_t)result;
            return MZ_DATA_ERROR;
        }
    } while ((zstd->in.pos < zstd->in.size) || (flush == ZSTD_e_end && result != 0));

    return MZ_OK;
}
#endif

int32_t mz_stream_zstd_write(void *stream, const void *buf, int32_t size) {
#ifdef MZ_ZIP_NO_COMPRESSION
    MZ_UNUSED(stream);
    MZ_UNUSED(buf);
    MZ_UNUSED(size);
    return MZ_SUPPORT_ERROR;
#else
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    int32_t err = MZ_OK;

    zstd->in.src = buf;
    zstd->in.pos = 0;
    zstd->in.size = size;

    err = mz_stream_zstd_compress(stream, ZSTD_e_continue);
    if (err != MZ_OK) {
        return err;
    }

    zstd->total_in += size;
    return size;
#endif
}

int64_t mz_stream_zstd_tell(void *stream) {
    MZ_UNUSED(stream);

    return MZ_TELL_ERROR;
}

int32_t mz_stream_zstd_seek(void *stream, int64_t offset, int32_t origin) {
    MZ_UNUSED(stream);
    MZ_UNUSED(offset);
    MZ_UNUSED(origin);

    return MZ_SEEK_ERROR;
}

int32_t mz_stream_zstd_close(void *stream) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;

    if (zstd->mode & MZ_OPEN_MODE_WRITE) {
#ifdef MZ_ZIP_NO_COMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        mz_stream_zstd_compress(stream, ZSTD_e_end);
        mz_stream_zstd_flush(stream);

        ZSTD_freeCStream(zstd->zcstream);
        zstd->zcstream = NULL;
#endif
    } else if (zstd->mode & MZ_OPEN_MODE_READ) {
#ifdef MZ_ZIP_NO_DECOMPRESSION
        return MZ_SUPPORT_ERROR;
#else
        ZSTD_freeDStream(zstd->zdstream);
        zstd->zdstream = NULL;
#endif
    }
    zstd->initialized = 0;
    return MZ_OK;
}

int32_t mz_stream_zstd_error(void *stream) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    return zstd->error;
}

int32_t mz_stream_zstd_get_prop_int64(void *stream, int32_t prop, int64_t *value) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_TOTAL_IN:
        *value = zstd->total_in;
        break;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        *value = zstd->max_total_in;
        break;
    case MZ_STREAM_PROP_TOTAL_OUT:
        *value = zstd->total_out;
        break;
    case MZ_STREAM_PROP_TOTAL_OUT_MAX:
        *value = zstd->max_total_out;
        break;
    case MZ_STREAM_PROP_HEADER_SIZE:
        *value = 0;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

int32_t mz_stream_zstd_set_prop_int64(void *stream, int32_t prop, int64_t value) {
    mz_stream_zstd *zstd = (mz_stream_zstd *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_COMPRESS_LEVEL:
        if (value < 0)
            zstd->preset = 6;
        else
            zstd->preset = (int16_t)value;
        return MZ_OK;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        zstd->max_total_in = value;
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}

void *mz_stream_zstd_create(void **stream) {
    mz_stream_zstd *zstd = NULL;
    zstd = (mz_stream_zstd *)MZ_ALLOC(sizeof(mz_stream_zstd));
    if (zstd != NULL) {
        memset(zstd, 0, sizeof(mz_stream_zstd));
        zstd->stream.vtbl = &mz_stream_zstd_vtbl;
        zstd->max_total_out = -1;
    }
    if (stream != NULL)
        *stream = zstd;
    return zstd;
}

void mz_stream_zstd_delete(void **stream) {
    mz_stream_zstd *zstd = NULL;
    if (stream == NULL)
        return;
    zstd = (mz_stream_zstd *)*stream;
    if (zstd != NULL)
        MZ_FREE(zstd);
    *stream = NULL;
}

void *mz_stream_zstd_get_interface(void) {
    return (void *)&mz_stream_zstd_vtbl;
}
