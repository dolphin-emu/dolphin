/* mz_strm_pkcrypt.c -- Code for traditional PKWARE encryption
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
      https://github.com/zlib-ng/minizip-ng
   Copyright (C) 1998-2005 Gilles Vollant
      Modifications for Info-ZIP crypting
      https://www.winimage.com/zLibDll/minizip.html
   Copyright (C) 2003 Terry Thorsen

   This code is a modified version of crypting code in Info-ZIP distribution

   Copyright (C) 1990-2000 Info-ZIP.  All rights reserved.

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.

   This encryption code is a direct transcription of the algorithm from
   Roger Schlafly, described by Phil Katz in the file appnote.txt. This
   file (appnote.txt) is distributed with the PKZIP program (even in the
   version without encryption capabilities).
*/


#include "mz.h"
#include "mz_crypt.h"
#include "mz_strm.h"
#include "mz_strm_pkcrypt.h"

/***************************************************************************/

static mz_stream_vtbl mz_stream_pkcrypt_vtbl = {
    mz_stream_pkcrypt_open,
    mz_stream_pkcrypt_is_open,
    mz_stream_pkcrypt_read,
    mz_stream_pkcrypt_write,
    mz_stream_pkcrypt_tell,
    mz_stream_pkcrypt_seek,
    mz_stream_pkcrypt_close,
    mz_stream_pkcrypt_error,
    mz_stream_pkcrypt_create,
    mz_stream_pkcrypt_delete,
    mz_stream_pkcrypt_get_prop_int64,
    mz_stream_pkcrypt_set_prop_int64
};

/***************************************************************************/

typedef struct mz_stream_pkcrypt_s {
    mz_stream       stream;
    int32_t         error;
    int16_t         initialized;
    uint8_t         buffer[UINT16_MAX];
    int64_t         total_in;
    int64_t         max_total_in;
    int64_t         total_out;
    uint32_t        keys[3];          /* keys defining the pseudo-random sequence */
    uint8_t         verify1;
    uint8_t         verify2;
    const char      *password;
} mz_stream_pkcrypt;

/***************************************************************************/

#define mz_stream_pkcrypt_decode(strm, c)                                   \
    (mz_stream_pkcrypt_update_keys(strm,                                    \
        c ^= mz_stream_pkcrypt_decrypt_byte(strm)))

#define mz_stream_pkcrypt_encode(strm, c, t)                                \
    (t = mz_stream_pkcrypt_decrypt_byte(strm),                              \
        mz_stream_pkcrypt_update_keys(strm, (uint8_t)c), (uint8_t)(t^(c)))

/***************************************************************************/

static uint8_t mz_stream_pkcrypt_decrypt_byte(void *stream) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;

    unsigned temp; /* POTENTIAL BUG:  temp*(temp^1) may overflow in an */
                   /* unpredictable manner on 16-bit systems; not a problem */
                   /* with any known compiler so far, though. */

    temp = pkcrypt->keys[2] | 2;
    return (uint8_t)(((temp * (temp ^ 1)) >> 8) & 0xff);
}

static uint8_t mz_stream_pkcrypt_update_keys(void *stream, uint8_t c) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    uint8_t buf = c;

    pkcrypt->keys[0] = (uint32_t)~mz_crypt_crc32_update(~pkcrypt->keys[0], &buf, 1);

    pkcrypt->keys[1] += pkcrypt->keys[0] & 0xff;
    pkcrypt->keys[1] *= 134775813L;
    pkcrypt->keys[1] += 1;

    buf = (uint8_t)(pkcrypt->keys[1] >> 24);
    pkcrypt->keys[2] = (uint32_t)~mz_crypt_crc32_update(~pkcrypt->keys[2], &buf, 1);

    return (uint8_t)c;
}

static void mz_stream_pkcrypt_init_keys(void *stream, const char *password) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;

    pkcrypt->keys[0] = 305419896L;
    pkcrypt->keys[1] = 591751049L;
    pkcrypt->keys[2] = 878082192L;

    while (*password != 0) {
        mz_stream_pkcrypt_update_keys(stream, (uint8_t)*password);
        password += 1;
    }
}

/***************************************************************************/

int32_t mz_stream_pkcrypt_open(void *stream, const char *path, int32_t mode) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    uint16_t t = 0;
    int16_t i = 0;
    uint8_t verify1 = 0;
    uint8_t verify2 = 0;
    uint8_t header[MZ_PKCRYPT_HEADER_SIZE];
    const char *password = path;

    pkcrypt->total_in = 0;
    pkcrypt->total_out = 0;
    pkcrypt->initialized = 0;

    if (mz_stream_is_open(pkcrypt->stream.base) != MZ_OK)
        return MZ_OPEN_ERROR;

    if (password == NULL)
        password = pkcrypt->password;
    if (password == NULL)
        return MZ_PARAM_ERROR;

    mz_stream_pkcrypt_init_keys(stream, password);

    if (mode & MZ_OPEN_MODE_WRITE) {
        /* First generate RAND_HEAD_LEN - 2 random bytes. */
        mz_crypt_rand(header, MZ_PKCRYPT_HEADER_SIZE - 2);

        /* Encrypt random header (last two bytes is high word of crc) */
        for (i = 0; i < MZ_PKCRYPT_HEADER_SIZE - 2; i++)
            header[i] = mz_stream_pkcrypt_encode(stream, header[i], t);

        header[i++] = mz_stream_pkcrypt_encode(stream, pkcrypt->verify1, t);
        header[i++] = mz_stream_pkcrypt_encode(stream, pkcrypt->verify2, t);

        if (mz_stream_write(pkcrypt->stream.base, header, sizeof(header)) != sizeof(header))
            return MZ_WRITE_ERROR;

        pkcrypt->total_out += MZ_PKCRYPT_HEADER_SIZE;
    } else if (mode & MZ_OPEN_MODE_READ) {
        if (mz_stream_read(pkcrypt->stream.base, header, sizeof(header)) != sizeof(header))
            return MZ_READ_ERROR;

        for (i = 0; i < MZ_PKCRYPT_HEADER_SIZE - 2; i++)
            header[i] = mz_stream_pkcrypt_decode(stream, header[i]);

        verify1 = mz_stream_pkcrypt_decode(stream, header[i++]);
        verify2 = mz_stream_pkcrypt_decode(stream, header[i++]);

        /* Older versions used 2 byte check, newer versions use 1 byte check. */
        MZ_UNUSED(verify1);
        if ((verify2 != 0) && (verify2 != pkcrypt->verify2))
            return MZ_PASSWORD_ERROR;

        pkcrypt->total_in += MZ_PKCRYPT_HEADER_SIZE;
    }

    pkcrypt->initialized = 1;
    return MZ_OK;
}

int32_t mz_stream_pkcrypt_is_open(void *stream) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    if (pkcrypt->initialized == 0)
        return MZ_OPEN_ERROR;
    return MZ_OK;
}

int32_t mz_stream_pkcrypt_read(void *stream, void *buf, int32_t size) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    uint8_t *buf_ptr = (uint8_t *)buf;
    int32_t bytes_to_read = size;
    int32_t read = 0;
    int32_t i = 0;


    if ((int64_t)bytes_to_read > (pkcrypt->max_total_in - pkcrypt->total_in))
        bytes_to_read = (int32_t)(pkcrypt->max_total_in - pkcrypt->total_in);

    read = mz_stream_read(pkcrypt->stream.base, buf, bytes_to_read);

    for (i = 0; i < read; i++)
        buf_ptr[i] = mz_stream_pkcrypt_decode(stream, buf_ptr[i]);

    if (read > 0)
        pkcrypt->total_in += read;

    return read;
}

int32_t mz_stream_pkcrypt_write(void *stream, const void *buf, int32_t size) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    const uint8_t *buf_ptr = (const uint8_t *)buf;
    int32_t bytes_to_write = sizeof(pkcrypt->buffer);
    int32_t total_written = 0;
    int32_t written = 0;
    int32_t i = 0;
    uint16_t t = 0;

    if (size < 0)
        return MZ_PARAM_ERROR;

    do {
        if (bytes_to_write > (size - total_written))
            bytes_to_write = (size - total_written);

        for (i = 0; i < bytes_to_write; i += 1) {
            pkcrypt->buffer[i] = mz_stream_pkcrypt_encode(stream, *buf_ptr, t);
            buf_ptr += 1;
        }

        written = mz_stream_write(pkcrypt->stream.base, pkcrypt->buffer, bytes_to_write);
        if (written < 0)
            return written;

        total_written += written;
    } while (total_written < size && written > 0);

    pkcrypt->total_out += total_written;
    return total_written;
}

int64_t mz_stream_pkcrypt_tell(void *stream) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    return mz_stream_tell(pkcrypt->stream.base);
}

int32_t mz_stream_pkcrypt_seek(void *stream, int64_t offset, int32_t origin) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    return mz_stream_seek(pkcrypt->stream.base, offset, origin);
}

int32_t mz_stream_pkcrypt_close(void *stream) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    pkcrypt->initialized = 0;
    return MZ_OK;
}

int32_t mz_stream_pkcrypt_error(void *stream) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    return pkcrypt->error;
}

void mz_stream_pkcrypt_set_password(void *stream, const char *password) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    pkcrypt->password = password;
}

void mz_stream_pkcrypt_set_verify(void *stream, uint8_t verify1, uint8_t verify2) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    pkcrypt->verify1 = verify1;
    pkcrypt->verify2 = verify2;
}

void mz_stream_pkcrypt_get_verify(void *stream, uint8_t *verify1, uint8_t *verify2) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    *verify1 = pkcrypt->verify1;
    *verify2 = pkcrypt->verify2;
}

int32_t mz_stream_pkcrypt_get_prop_int64(void *stream, int32_t prop, int64_t *value) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_TOTAL_IN:
        *value = pkcrypt->total_in;
        break;
    case MZ_STREAM_PROP_TOTAL_OUT:
        *value = pkcrypt->total_out;
        break;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        *value = pkcrypt->max_total_in;
        break;
    case MZ_STREAM_PROP_HEADER_SIZE:
        *value = MZ_PKCRYPT_HEADER_SIZE;
        break;
    case MZ_STREAM_PROP_FOOTER_SIZE:
        *value = 0;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

int32_t mz_stream_pkcrypt_set_prop_int64(void *stream, int32_t prop, int64_t value) {
    mz_stream_pkcrypt *pkcrypt = (mz_stream_pkcrypt *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        pkcrypt->max_total_in = value;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

void *mz_stream_pkcrypt_create(void **stream) {
    mz_stream_pkcrypt *pkcrypt = NULL;

    pkcrypt = (mz_stream_pkcrypt *)MZ_ALLOC(sizeof(mz_stream_pkcrypt));
    if (pkcrypt != NULL) {
        memset(pkcrypt, 0, sizeof(mz_stream_pkcrypt));
        pkcrypt->stream.vtbl = &mz_stream_pkcrypt_vtbl;
    }

    if (stream != NULL)
        *stream = pkcrypt;
    return pkcrypt;
}

void mz_stream_pkcrypt_delete(void **stream) {
    mz_stream_pkcrypt *pkcrypt = NULL;
    if (stream == NULL)
        return;
    pkcrypt = (mz_stream_pkcrypt *)*stream;
    if (pkcrypt != NULL)
        MZ_FREE(pkcrypt);
    *stream = NULL;
}

void *mz_stream_pkcrypt_get_interface(void) {
    return (void *)&mz_stream_pkcrypt_vtbl;
}
