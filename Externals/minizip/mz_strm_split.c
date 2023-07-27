/* mz_strm_split.c -- Stream for split files
   part of the minizip-ng project

   Copyright (C) Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_split.h"

#include <stdio.h> /* snprintf */
#include <string.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#  define snprintf _snprintf
#endif

/***************************************************************************/

#define MZ_ZIP_MAGIC_DISKHEADER (0x08074b50)

/***************************************************************************/

static mz_stream_vtbl mz_stream_split_vtbl = {
    mz_stream_split_open,
    mz_stream_split_is_open,
    mz_stream_split_read,
    mz_stream_split_write,
    mz_stream_split_tell,
    mz_stream_split_seek,
    mz_stream_split_close,
    mz_stream_split_error,
    mz_stream_split_create,
    mz_stream_split_delete,
    mz_stream_split_get_prop_int64,
    mz_stream_split_set_prop_int64
};

/***************************************************************************/

typedef struct mz_stream_split_s {
    mz_stream   stream;
    int32_t     is_open;
    int64_t     disk_size;
    int64_t     total_in;
    int64_t     total_in_disk;
    int64_t     total_out;
    int64_t     total_out_disk;
    int32_t     mode;
    char        *path_cd;
    char        *path_disk;
    uint32_t    path_disk_size;
    int32_t     number_disk;
    int32_t     current_disk;
    int64_t     current_disk_size;
    int32_t     reached_end;
} mz_stream_split;

/***************************************************************************/

#if 0
#  define mz_stream_split_print printf
#else
#  define mz_stream_split_print(fmt, ...)
#endif

/***************************************************************************/

static int32_t mz_stream_split_open_disk(void *stream, int32_t number_disk) {
    mz_stream_split *split = (mz_stream_split *)stream;
    uint32_t magic = 0;
    int64_t position = 0;
    int32_t i = 0;
    int32_t err = MZ_OK;
    int16_t disk_part = 0;

    /* Check if we are reading or writing a disk part or the cd disk */
    if (number_disk >= 0) {
        if ((split->mode & MZ_OPEN_MODE_WRITE) == 0)
            disk_part = MZ_OPEN_MODE_READ;
        else if (split->disk_size > 0)
            disk_part = MZ_OPEN_MODE_WRITE;
    }

    /* Construct disk path */
    if (disk_part > 0) {
        for (i = (int32_t)strlen(split->path_disk) - 1; i >= 0; i -= 1) {
            if (split->path_disk[i] != '.')
                continue;
            snprintf(&split->path_disk[i], split->path_disk_size - (uint32_t)i,
                ".z%02" PRId32, number_disk + 1);
            break;
        }
    } else {
        strncpy(split->path_disk, split->path_cd, split->path_disk_size - 1);
        split->path_disk[split->path_disk_size - 1] = 0;
    }

    mz_stream_split_print("Split - Goto disk - %s (disk %" PRId32 ")\n", split->path_disk, number_disk);

    /* If disk part doesn't exist during reading then return MZ_EXIST_ERROR */
    if (disk_part == MZ_OPEN_MODE_READ)
        err = mz_os_file_exists(split->path_disk);

    if (err == MZ_OK)
        err = mz_stream_open(split->stream.base, split->path_disk, split->mode);

    if (err == MZ_OK) {
        split->total_in_disk = 0;
        split->total_out_disk = 0;
        split->current_disk = number_disk;

        if (split->mode & MZ_OPEN_MODE_WRITE) {
            if ((split->current_disk == 0) && (split->disk_size > 0)) {
                err = mz_stream_write_uint32(split->stream.base, MZ_ZIP_MAGIC_DISKHEADER);

                split->total_out_disk += 4;
                split->total_out += split->total_out_disk;
            }
        } else if (split->mode & MZ_OPEN_MODE_READ) {
            if (split->current_disk == 0) {
                err = mz_stream_read_uint32(split->stream.base, &magic);
                if (magic != MZ_ZIP_MAGIC_DISKHEADER)
                    err = MZ_FORMAT_ERROR;
            }
        }
    }

    if (err == MZ_OK) {
        /* Get the size of the current disk we are on */
        position = mz_stream_tell(split->stream.base);
        mz_stream_seek(split->stream.base, 0, MZ_SEEK_END);
        split->current_disk_size = mz_stream_tell(split->stream.base);
        mz_stream_seek(split->stream.base, position, MZ_SEEK_SET);

        split->is_open = 1;
    }

    return err;
}

static int32_t mz_stream_split_close_disk(void *stream) {
    mz_stream_split *split = (mz_stream_split *)stream;

    if (mz_stream_is_open(split->stream.base) != MZ_OK)
        return MZ_OK;

    mz_stream_split_print("Split - Close disk\n");
    return mz_stream_close(split->stream.base);
}

static int32_t mz_stream_split_goto_disk(void *stream, int32_t number_disk) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int32_t err = MZ_OK;
    int32_t err_is_open = MZ_OK;

    err_is_open = mz_stream_is_open(split->stream.base);

    if ((split->disk_size == 0) && (split->mode & MZ_OPEN_MODE_WRITE)) {
        if (err_is_open != MZ_OK)
            err = mz_stream_split_open_disk(stream, number_disk);
    } else if ((number_disk != split->current_disk) || (err_is_open != MZ_OK)) {
        err = mz_stream_split_close_disk(stream);
        if (err == MZ_OK) {
            err = mz_stream_split_open_disk(stream, number_disk);
            if (err == MZ_OK)
                split->number_disk = number_disk;
        }
    }

    return err;
}

int32_t mz_stream_split_open(void *stream, const char *path, int32_t mode) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int32_t number_disk = 0;

    split->mode = mode;
    split->path_cd = strdup(path);

    if (!split->path_cd)
        return MZ_MEM_ERROR;

    mz_stream_split_print("Split - Open - %s (disk %" PRId32 ")\n", split->path_cd, number_disk);

    split->path_disk_size = (uint32_t)strlen(path) + 10;
    split->path_disk = (char *)malloc(split->path_disk_size);

    if (!split->path_disk) {
        free(split->path_cd);
        return MZ_MEM_ERROR;
    }

    strncpy(split->path_disk, path, split->path_disk_size - 1);
    split->path_disk[split->path_disk_size - 1] = 0;

    if ((mode & MZ_OPEN_MODE_WRITE) && ((mode & MZ_OPEN_MODE_APPEND) == 0)) {
        number_disk = 0;
        split->current_disk = -1;
    } else {
        number_disk = -1;
        split->current_disk = 0;
    }

    return mz_stream_split_goto_disk(stream, number_disk);
}

int32_t mz_stream_split_is_open(void *stream) {
    mz_stream_split *split = (mz_stream_split *)stream;
    if (split->is_open != 1)
        return MZ_OPEN_ERROR;
    return MZ_OK;
}

int32_t mz_stream_split_read(void *stream, void *buf, int32_t size) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int32_t bytes_left = size;
    int32_t read = 0;
    int32_t err = MZ_OK;
    uint8_t *buf_ptr = (uint8_t *)buf;

    err = mz_stream_split_goto_disk(stream, split->number_disk);
    if (err != MZ_OK)
        return err;

    while (bytes_left > 0) {
        read = mz_stream_read(split->stream.base, buf_ptr, bytes_left);

        mz_stream_split_print("Split - Read disk - %" PRId32 "\n", read);

        if (read < 0)
            return read;
        if (read == 0) {
            if (split->current_disk < 0) /* No more disks to goto */
                break;
            err = mz_stream_split_goto_disk(stream, split->current_disk + 1);
            if (err == MZ_EXIST_ERROR) {
                split->current_disk = -1;
                break;
            }
            if (err != MZ_OK)
                return err;
        }

        bytes_left -= read;
        buf_ptr += read;
        split->total_in += read;
        split->total_in_disk += read;
    }
    return size - bytes_left;
}

int32_t mz_stream_split_write(void *stream, const void *buf, int32_t size) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int64_t position = 0;
    int32_t written = 0;
    int32_t bytes_left = size;
    int32_t bytes_to_write = 0;
    int32_t bytes_avail = 0;
    int32_t number_disk = -1;
    int32_t err = MZ_OK;
    const uint8_t *buf_ptr = (const uint8_t *)buf;

    position = mz_stream_tell(split->stream.base);

    while (bytes_left > 0) {
        bytes_to_write = bytes_left;

        if (split->disk_size > 0) {
            if ((split->total_out_disk == split->disk_size && split->total_out > 0) ||
                (split->number_disk == -1 && split->number_disk != split->current_disk)) {
                if (split->number_disk != -1)
                    number_disk = split->current_disk + 1;

                err = mz_stream_split_goto_disk(stream, number_disk);
                if (err != MZ_OK)
                    return err;

                position = 0;
            }

            if (split->number_disk != -1) {
                bytes_avail = (int32_t)(split->disk_size - split->total_out_disk);
                if (bytes_to_write > bytes_avail)
                    bytes_to_write = bytes_avail;
            }
        }

        written = mz_stream_write(split->stream.base, buf_ptr, bytes_to_write);
        if (written != bytes_to_write)
            return MZ_WRITE_ERROR;

        mz_stream_split_print("Split - Write disk - %" PRId32 "\n", written);

        bytes_left -= written;
        buf_ptr += written;

        split->total_out += written;
        split->total_out_disk += written;

        position += written;
        if (position > split->current_disk_size)
            split->current_disk_size = position;
    }

    return size - bytes_left;
}

int64_t mz_stream_split_tell(void *stream) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int32_t err = MZ_OK;
    err = mz_stream_split_goto_disk(stream, split->number_disk);
    if (err != MZ_OK)
        return err;
    return mz_stream_tell(split->stream.base);
}

int32_t mz_stream_split_seek(void *stream, int64_t offset, int32_t origin) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int64_t disk_left = 0;
    int64_t position = 0;
    int32_t err = MZ_OK;

    err = mz_stream_split_goto_disk(stream, split->number_disk);

    if (err != MZ_OK)
        return err;

    mz_stream_split_print("Split - Seek disk - %" PRId64 " (origin %" PRId32 ")\n", offset, origin);

    if ((origin == MZ_SEEK_CUR) && (split->number_disk != -1)) {
        position = mz_stream_tell(split->stream.base);
        disk_left = split->current_disk_size - position;

        while (offset > disk_left) {
            err = mz_stream_split_goto_disk(stream, split->current_disk + 1);
            if (err != MZ_OK)
                return err;

            offset -= disk_left;
            disk_left = split->current_disk_size;
        }
    }

    return mz_stream_seek(split->stream.base, offset, origin);
}

int32_t mz_stream_split_close(void *stream) {
    mz_stream_split *split = (mz_stream_split *)stream;
    int32_t err = MZ_OK;

    err = mz_stream_split_close_disk(stream);
    split->is_open = 0;
    return err;
}

int32_t mz_stream_split_error(void *stream) {
    mz_stream_split *split = (mz_stream_split *)stream;
    return mz_stream_error(split->stream.base);
}

int32_t mz_stream_split_get_prop_int64(void *stream, int32_t prop, int64_t *value) {
    mz_stream_split *split = (mz_stream_split *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_TOTAL_OUT:
        *value = split->total_out;
        break;
    case MZ_STREAM_PROP_DISK_NUMBER:
        *value = split->number_disk;
        break;
    case MZ_STREAM_PROP_DISK_SIZE:
        *value = split->disk_size;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

int32_t mz_stream_split_set_prop_int64(void *stream, int32_t prop, int64_t value) {
    mz_stream_split *split = (mz_stream_split *)stream;
    switch (prop) {
    case MZ_STREAM_PROP_DISK_NUMBER:
        split->number_disk = (int32_t)value;
        break;
    case MZ_STREAM_PROP_DISK_SIZE:
        split->disk_size = value;
        break;
    default:
        return MZ_EXIST_ERROR;
    }
    return MZ_OK;
}

void *mz_stream_split_create(void) {
    mz_stream_split *split = (mz_stream_split *)calloc(1, sizeof(mz_stream_split));
    if (split)
        split->stream.vtbl = &mz_stream_split_vtbl;
    return split;
}

void mz_stream_split_delete(void **stream) {
    mz_stream_split *split = NULL;
    if (!stream)
        return;
    split = (mz_stream_split *)*stream;
    if (split) {
        if (split->path_cd)
            free(split->path_cd);
        if (split->path_disk)
            free(split->path_disk);

        free(split);
    }
    *stream = NULL;
}

void *mz_stream_split_get_interface(void) {
    return (void *)&mz_stream_split_vtbl;
}
