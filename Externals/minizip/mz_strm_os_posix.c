/* mz_strm_posix.c -- Stream for filesystem access for posix/linux
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng
   Modifications for Zip64 support
     Copyright (C) 2009-2010 Mathias Svensson
     http://result42.com
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/


#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_os.h"

#include <stdio.h> /* fopen, fread.. */
#include <errno.h>

/***************************************************************************/

#define fopen64 fopen
#ifndef MZ_FILE32_API
#  ifndef NO_FSEEKO
#    define ftello64 ftello
#    define fseeko64 fseeko
#  elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#    define ftello64 _ftelli64
#    define fseeko64 _fseeki64
#  endif
#endif
#ifndef ftello64
#  define ftello64 ftell
#endif
#ifndef fseeko64
#  define fseeko64 fseek
#endif

/***************************************************************************/

static mz_stream_vtbl mz_stream_os_vtbl = {
    mz_stream_os_open,
    mz_stream_os_is_open,
    mz_stream_os_read,
    mz_stream_os_write,
    mz_stream_os_tell,
    mz_stream_os_seek,
    mz_stream_os_close,
    mz_stream_os_error,
    mz_stream_os_create,
    mz_stream_os_delete,
    NULL,
    NULL
};

/***************************************************************************/

typedef struct mz_stream_posix_s {
    mz_stream   stream;
    int32_t     error;
    FILE        *handle;
} mz_stream_posix;

/***************************************************************************/

int32_t mz_stream_os_open(void *stream, const char *path, int32_t mode) {
    mz_stream_posix *posix = (mz_stream_posix *)stream;
    const char *mode_fopen = NULL;

    if (path == NULL)
        return MZ_PARAM_ERROR;

    if ((mode & MZ_OPEN_MODE_READWRITE) == MZ_OPEN_MODE_READ)
        mode_fopen = "rb";
    else if (mode & MZ_OPEN_MODE_APPEND)
        mode_fopen = "r+b";
    else if (mode & MZ_OPEN_MODE_CREATE)
        mode_fopen = "wb";
    else
        return MZ_OPEN_ERROR;

    posix->handle = fopen64(path, mode_fopen);
    if (posix->handle == NULL) {
        posix->error = errno;
        return MZ_OPEN_ERROR;
    }

    if (mode & MZ_OPEN_MODE_APPEND)
        return mz_stream_os_seek(stream, 0, MZ_SEEK_END);

    return MZ_OK;
}

int32_t mz_stream_os_is_open(void *stream) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    if (posix->handle == NULL)
        return MZ_OPEN_ERROR;
    return MZ_OK;
}

int32_t mz_stream_os_read(void *stream, void *buf, int32_t size) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    int32_t read = (int32_t)fread(buf, 1, (size_t)size, posix->handle);
    if (read < size && ferror(posix->handle)) {
        posix->error = errno;
        return MZ_READ_ERROR;
    }
    return read;
}

int32_t mz_stream_os_write(void *stream, const void *buf, int32_t size) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    int32_t written = (int32_t)fwrite(buf, 1, (size_t)size, posix->handle);
    if (written < size && ferror(posix->handle)) {
        posix->error = errno;
        return MZ_WRITE_ERROR;
    }
    return written;
}

int64_t mz_stream_os_tell(void *stream) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    int64_t position = ftello64(posix->handle);
    if (position == -1) {
        posix->error = errno;
        return MZ_TELL_ERROR;
    }
    return position;
}

int32_t mz_stream_os_seek(void *stream, int64_t offset, int32_t origin) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    int32_t fseek_origin = 0;

    switch (origin) {
    case MZ_SEEK_CUR:
        fseek_origin = SEEK_CUR;
        break;
    case MZ_SEEK_END:
        fseek_origin = SEEK_END;
        break;
    case MZ_SEEK_SET:
        fseek_origin = SEEK_SET;
        break;
    default:
        return MZ_SEEK_ERROR;
    }

    if (fseeko64(posix->handle, offset, fseek_origin) != 0) {
        posix->error = errno;
        return MZ_SEEK_ERROR;
    }

    return MZ_OK;
}

int32_t mz_stream_os_close(void *stream) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    int32_t closed = 0;
    if (posix->handle != NULL) {
        closed = fclose(posix->handle);
        posix->handle = NULL;
    }
    if (closed != 0) {
        posix->error = errno;
        return MZ_CLOSE_ERROR;
    }
    return MZ_OK;
}

int32_t mz_stream_os_error(void *stream) {
    mz_stream_posix *posix = (mz_stream_posix*)stream;
    return posix->error;
}

void *mz_stream_os_create(void **stream) {
    mz_stream_posix *posix = NULL;

    posix = (mz_stream_posix *)MZ_ALLOC(sizeof(mz_stream_posix));
    if (posix != NULL) {
        memset(posix, 0, sizeof(mz_stream_posix));
        posix->stream.vtbl = &mz_stream_os_vtbl;
    }
    if (stream != NULL)
        *stream = posix;

    return posix;
}

void mz_stream_os_delete(void **stream) {
    mz_stream_posix *posix = NULL;
    if (stream == NULL)
        return;
    posix = (mz_stream_posix *)*stream;
    if (posix != NULL)
        MZ_FREE(posix);
    *stream = NULL;
}

void *mz_stream_os_get_interface(void) {
    return (void *)&mz_stream_os_vtbl;
}
