/* mz_strm_buf.c -- Stream for buffering reads/writes
   part of the minizip-ng project

   This version of ioapi is designed to buffer IO.

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"

/***************************************************************************/

static mz_stream_vtbl mz_stream_buffered_vtbl = {
    mz_stream_buffered_open,
    mz_stream_buffered_is_open,
    mz_stream_buffered_read,
    mz_stream_buffered_write,
    mz_stream_buffered_tell,
    mz_stream_buffered_seek,
    mz_stream_buffered_close,
    mz_stream_buffered_error,
    mz_stream_buffered_create,
    mz_stream_buffered_delete,
    NULL,
    NULL
};

/***************************************************************************/

typedef struct mz_stream_buffered_s {
    mz_stream stream;
    int32_t   error;
    char      readbuf[INT16_MAX];
    int32_t   readbuf_len;
    int32_t   readbuf_pos;
    int32_t   readbuf_hits;
    int32_t   readbuf_misses;
    char      writebuf[INT16_MAX];
    int32_t   writebuf_len;
    int32_t   writebuf_pos;
    int32_t   writebuf_hits;
    int32_t   writebuf_misses;
    int64_t   position;
} mz_stream_buffered;

/***************************************************************************/

#if 0
#  define mz_stream_buffered_print printf
#else
#  define mz_stream_buffered_print(fmt,...)
#endif

/***************************************************************************/

static int32_t mz_stream_buffered_reset(void *stream) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;

    buffered->readbuf_len = 0;
    buffered->readbuf_pos = 0;
    buffered->writebuf_len = 0;
    buffered->writebuf_pos = 0;
    buffered->position = 0;

    return MZ_OK;
}

int32_t mz_stream_buffered_open(void *stream, const char *path, int32_t mode) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    mz_stream_buffered_print("Buffered - Open (mode %" PRId32 ")\n", mode);
    mz_stream_buffered_reset(buffered);
    return mz_stream_open(buffered->stream.base, path, mode);
}

int32_t mz_stream_buffered_is_open(void *stream) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    return mz_stream_is_open(buffered->stream.base);
}

static int32_t mz_stream_buffered_flush(void *stream, int32_t *written) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    int32_t total_bytes_written = 0;
    int32_t bytes_to_write = buffered->writebuf_len;
    int32_t bytes_left_to_write = buffered->writebuf_len;
    int32_t bytes_written = 0;

    *written = 0;

    while (bytes_left_to_write > 0) {
        bytes_written = mz_stream_write(buffered->stream.base,
            buffered->writebuf + (bytes_to_write - bytes_left_to_write), bytes_left_to_write);

        if (bytes_written != bytes_left_to_write)
            return MZ_WRITE_ERROR;

        buffered->writebuf_misses += 1;

        mz_stream_buffered_print("Buffered - Write flush (%" PRId32 ":%" PRId32 " len %" PRId32 ")\n",
            bytes_to_write, bytes_left_to_write, buffered->writebuf_len);

        total_bytes_written += bytes_written;
        bytes_left_to_write -= bytes_written;
        buffered->position += bytes_written;
    }

    buffered->writebuf_len = 0;
    buffered->writebuf_pos = 0;

    *written = total_bytes_written;
    return MZ_OK;
}

int32_t mz_stream_buffered_read(void *stream, void *buf, int32_t size) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    int32_t buf_len = 0;
    int32_t bytes_to_read = 0;
    int32_t bytes_to_copy = 0;
    int32_t bytes_left_to_read = size;
    int32_t bytes_read = 0;
    int32_t bytes_flushed = 0;

    mz_stream_buffered_print("Buffered - Read (size %" PRId32 " pos %" PRId64 ")\n", size, buffered->position);

    if (buffered->writebuf_len > 0) {
        int64_t position  = buffered->position + buffered->writebuf_pos

        mz_stream_buffered_print("Buffered - Switch from write to read, flushing (pos %" PRId64 ")\n", position);

        mz_stream_buffered_flush(stream, &bytes_flushed);
        mz_stream_buffered_seek(stream, position, MZ_SEEK_SET);
    }

    while (bytes_left_to_read > 0) {
        if ((buffered->readbuf_len == 0) || (buffered->readbuf_pos == buffered->readbuf_len)) {
            if (buffered->readbuf_len == sizeof(buffered->readbuf)) {
                buffered->readbuf_pos = 0;
                buffered->readbuf_len = 0;
            }

            bytes_to_read = (int32_t)sizeof(buffered->readbuf) - (buffered->readbuf_len - buffered->readbuf_pos);
            bytes_read = mz_stream_read(buffered->stream.base, buffered->readbuf + buffered->readbuf_pos, bytes_to_read);
            if (bytes_read < 0)
                return bytes_read;

            buffered->readbuf_misses += 1;
            buffered->readbuf_len += bytes_read;
            buffered->position += bytes_read;

            mz_stream_buffered_print("Buffered - Filled (read %" PRId32 "/%" PRId32 " buf %" PRId32 ":%" PRId32 " pos %" PRId64 ")\n",
                bytes_read, bytes_to_read, buffered->readbuf_pos, buffered->readbuf_len, buffered->position);

            if (bytes_read == 0)
                break;
        }

        if ((buffered->readbuf_len - buffered->readbuf_pos) > 0) {
            bytes_to_copy = buffered->readbuf_len - buffered->readbuf_pos;
            if (bytes_to_copy > bytes_left_to_read)
                bytes_to_copy = bytes_left_to_read;

            memcpy((char *)buf + buf_len, buffered->readbuf + buffered->readbuf_pos, bytes_to_copy);

            buf_len += bytes_to_copy;
            bytes_left_to_read -= bytes_to_copy;

            buffered->readbuf_hits += 1;
            buffered->readbuf_pos += bytes_to_copy;

            mz_stream_buffered_print("Buffered - Emptied (copied %" PRId32 " remaining %" PRId32 " buf %" PRId32 ":%" PRId32 " pos %" PRId64 ")\n",
                bytes_to_copy, bytes_left_to_read, buffered->readbuf_pos, buffered->readbuf_len, buffered->position);
        }
    }

    return size - bytes_left_to_read;
}

int32_t mz_stream_buffered_write(void *stream, const void *buf, int32_t size) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    int32_t bytes_to_write = size;
    int32_t bytes_left_to_write = size;
    int32_t bytes_to_copy = 0;
    int32_t bytes_used = 0;
    int32_t bytes_flushed = 0;
    int32_t err = MZ_OK;


    mz_stream_buffered_print("Buffered - Write (size %" PRId32 " len %" PRId32 " pos %" PRId64 ")\n",
        size, buffered->writebuf_len, buffered->position);

    if (buffered->readbuf_len > 0) {
        buffered->position -= buffered->readbuf_len;
        buffered->position += buffered->readbuf_pos;

        buffered->readbuf_len = 0;
        buffered->readbuf_pos = 0;

        mz_stream_buffered_print("Buffered - Switch from read to write (pos %" PRId64 ")\n", buffered->position);

        err = mz_stream_seek(buffered->stream.base, buffered->position, MZ_SEEK_SET);
        if (err != MZ_OK)
            return err;
    }

    while (bytes_left_to_write > 0) {
        bytes_used = buffered->writebuf_len;
        if (bytes_used > buffered->writebuf_pos)
            bytes_used = buffered->writebuf_pos;
        bytes_to_copy = (int32_t)sizeof(buffered->writebuf) - bytes_used;
        if (bytes_to_copy > bytes_left_to_write)
            bytes_to_copy = bytes_left_to_write;

        if (bytes_to_copy == 0) {
            err = mz_stream_buffered_flush(stream, &bytes_flushed);
            if (err != MZ_OK)
                return err;
            if (bytes_flushed == 0)
                return 0;

            continue;
        }

        memcpy(buffered->writebuf + buffered->writebuf_pos,
            (const char *)buf + (bytes_to_write - bytes_left_to_write), bytes_to_copy);

        mz_stream_buffered_print("Buffered - Write copy (remaining %" PRId32 " write %" PRId32 ":%" PRId32 " len %" PRId32 ")\n",
            bytes_to_copy, bytes_to_write, bytes_left_to_write, buffered->writebuf_len);

        bytes_left_to_write -= bytes_to_copy;

        buffered->writebuf_pos += bytes_to_copy;
        buffered->writebuf_hits += 1;
        if (buffered->writebuf_pos > buffered->writebuf_len)
            buffered->writebuf_len += buffered->writebuf_pos - buffered->writebuf_len;
    }

    return size - bytes_left_to_write;
}

int64_t mz_stream_buffered_tell(void *stream) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    int64_t position = mz_stream_tell(buffered->stream.base);

    buffered->position = position;

    mz_stream_buffered_print("Buffered - Tell (pos %" PRId64 " readpos %" PRId32 " writepos %" PRId32 ")\n",
        buffered->position, buffered->readbuf_pos, buffered->writebuf_pos);

    if (buffered->readbuf_len > 0)
        position -= ((int64_t)buffered->readbuf_len - buffered->readbuf_pos);
    if (buffered->writebuf_len > 0)
        position += buffered->writebuf_pos;
    return position;
}

int32_t mz_stream_buffered_seek(void *stream, int64_t offset, int32_t origin) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    int32_t bytes_flushed = 0;
    int32_t err = MZ_OK;

    mz_stream_buffered_print("Buffered - Seek (origin %" PRId32 " offset %" PRId64 " pos %" PRId64 ")\n",
        origin, offset, buffered->position);

    switch (origin) {
    case MZ_SEEK_SET:

        if ((buffered->readbuf_len > 0) && (offset < buffered->position) &&
            (offset >= buffered->position - buffered->readbuf_len)) {
            buffered->readbuf_pos = (int32_t)(offset - (buffered->position - buffered->readbuf_len));
            return MZ_OK;
        }
        if (buffered->writebuf_len > 0) {
            if ((offset >= buffered->position) && (offset <= buffered->position + buffered->writebuf_len)) {
                buffered->writebuf_pos = (int32_t)(offset - buffered->position);
                return MZ_OK;
            }
        }

        err = mz_stream_buffered_flush(stream, &bytes_flushed);
        if (err != MZ_OK)
            return err;

        buffered->position = offset;
        break;

    case MZ_SEEK_CUR:

        if (buffered->readbuf_len > 0) {
            if (offset <= ((int64_t)buffered->readbuf_len - buffered->readbuf_pos)) {
                buffered->readbuf_pos += (uint32_t)offset;
                return MZ_OK;
            }
            offset -= ((int64_t)buffered->readbuf_len - buffered->readbuf_pos);
            buffered->position += offset;
        }
        if (buffered->writebuf_len > 0) {
            if (offset <= ((int64_t)buffered->writebuf_len - buffered->writebuf_pos)) {
                buffered->writebuf_pos += (uint32_t)offset;
                return MZ_OK;
            }
            /* offset -= (buffered->writebuf_len - buffered->writebuf_pos); */
        }

        err = mz_stream_buffered_flush(stream, &bytes_flushed);
        if (err != MZ_OK)
            return err;

        break;

    case MZ_SEEK_END:

        if (buffered->writebuf_len > 0) {
            buffered->writebuf_pos = buffered->writebuf_len;
            return MZ_OK;
        }
        break;
    }

    buffered->readbuf_len = 0;
    buffered->readbuf_pos = 0;
    buffered->writebuf_len = 0;
    buffered->writebuf_pos = 0;

    return mz_stream_seek(buffered->stream.base, offset, origin);
}

int32_t mz_stream_buffered_close(void *stream) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    int32_t bytes_flushed = 0;

    mz_stream_buffered_flush(stream, &bytes_flushed);
    mz_stream_buffered_print("Buffered - Close (flushed %" PRId32 ")\n", bytes_flushed);

    if (buffered->readbuf_hits + buffered->readbuf_misses > 0) {
        mz_stream_buffered_print("Buffered - Read efficiency %.02f%%\n",
            (buffered->readbuf_hits / ((float)buffered->readbuf_hits + buffered->readbuf_misses)) * 100);
    }

    if (buffered->writebuf_hits + buffered->writebuf_misses > 0) {
        mz_stream_buffered_print("Buffered - Write efficiency %.02f%%\n",
            (buffered->writebuf_hits / ((float)buffered->writebuf_hits + buffered->writebuf_misses)) * 100);
    }

    mz_stream_buffered_reset(buffered);

    return mz_stream_close(buffered->stream.base);
}

int32_t mz_stream_buffered_error(void *stream) {
    mz_stream_buffered *buffered = (mz_stream_buffered *)stream;
    return mz_stream_error(buffered->stream.base);
}

void *mz_stream_buffered_create(void **stream) {
    mz_stream_buffered *buffered = NULL;

    buffered = (mz_stream_buffered *)MZ_ALLOC(sizeof(mz_stream_buffered));
    if (buffered != NULL) {
        memset(buffered, 0, sizeof(mz_stream_buffered));
        buffered->stream.vtbl = &mz_stream_buffered_vtbl;
    }
    if (stream != NULL)
        *stream = buffered;

    return buffered;
}

void mz_stream_buffered_delete(void **stream) {
    mz_stream_buffered *buffered = NULL;
    if (stream == NULL)
        return;
    buffered = (mz_stream_buffered *)*stream;
    if (buffered != NULL)
        MZ_FREE(buffered);
    *stream = NULL;
}

void *mz_stream_buffered_get_interface(void) {
    return (void *)&mz_stream_buffered_vtbl;
}
