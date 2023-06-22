/* minigzip.c
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/


#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_os.h"
#include "mz_strm_zlib.h"

#include <stdio.h>  /* printf */

/***************************************************************************/

#define MZ_GZIP_COMPRESS    (1)
#define MZ_GZIP_DECOMPRESS  (2)

int32_t minigzip_banner(void);
int32_t minigzip_help(void);

/***************************************************************************/

int32_t minigzip_banner(void) {
    printf("Minigzip %s - https://github.com/zlib-ng/minizip-ng\n", MZ_VERSION);
    printf("---------------------------------------------------\n");
    return MZ_OK;
}

int32_t minigzip_help(void) {
    printf("Usage: minigzip [-x] [-d] [-0 to -9] [files]\n\n" \
           "  -x  Extract file\n" \
           "  -d  Destination directory\n" \
           "  -0  Store only\n" \
           "  -1  Compress faster\n" \
           "  -9  Compress better\n\n");
    return MZ_OK;
}

/***************************************************************************/

int32_t minigzip_copy(const char *path, const char *destination, int16_t operation, int16_t level) {
    void *target_stream = NULL;
    void *source_stream = NULL;
    void *zlib_stream = NULL;
    const char *filename = NULL;
    char target_path[1024];
    int32_t err = 0;


    memset(target_path, 0, sizeof(target_path));

    if (destination != NULL) {
        if (mz_os_file_exists(destination) != MZ_OK)
            mz_dir_make(destination);
    }

    if (operation == MZ_GZIP_COMPRESS) {
        mz_path_combine(target_path, path, sizeof(target_path));
        strncat(target_path, ".gz", sizeof(target_path) - strlen(target_path) - 1);
        printf("Compressing to %s\n", target_path);
    } else if (operation == MZ_GZIP_DECOMPRESS) {
        if (destination != NULL)
            mz_path_combine(target_path, destination, sizeof(target_path));

        if (mz_path_get_filename(path, &filename) != MZ_OK)
            filename = path;

        mz_path_combine(target_path, filename, sizeof(target_path));
        mz_path_remove_extension(target_path);
        printf("Decompressing to %s\n", target_path);
    }

    mz_stream_zlib_create(&zlib_stream);
    mz_stream_zlib_set_prop_int64(zlib_stream, MZ_STREAM_PROP_COMPRESS_WINDOW, 15 + 16);

    mz_stream_os_create(&source_stream);
    err = mz_stream_os_open(source_stream, path, MZ_OPEN_MODE_READ);

    if (err == MZ_OK) {
        mz_stream_os_create(&target_stream);
        err = mz_stream_os_open(target_stream, target_path, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE);

        if (err == MZ_OK) {
            if (operation == MZ_GZIP_COMPRESS) {
                mz_stream_zlib_set_prop_int64(zlib_stream, MZ_STREAM_PROP_COMPRESS_LEVEL, level);
                mz_stream_zlib_open(zlib_stream, target_path, MZ_OPEN_MODE_WRITE);
                mz_stream_set_base(zlib_stream, target_stream);
                err = mz_stream_copy_to_end(zlib_stream, source_stream);
            } else if (operation == MZ_GZIP_DECOMPRESS) {
                mz_stream_zlib_open(zlib_stream, path, MZ_OPEN_MODE_READ);
                mz_stream_set_base(zlib_stream, source_stream);
                err = mz_stream_copy_to_end(target_stream, zlib_stream);
            }

            if (err != MZ_OK)
                printf("Error %d in zlib stream (%d)\n", err, mz_stream_zlib_error(zlib_stream));
            else
                printf("Operation completed successfully\n");

            mz_stream_zlib_close(zlib_stream);
        } else {
            printf("Error %d opening target path %s\n", err, target_path);
        }

        mz_stream_os_close(target_stream);
        mz_stream_os_delete(&target_stream);
    } else {
        printf("Error %d opening source path %s\n", err, path);
    }

    mz_stream_os_close(source_stream);
    mz_stream_os_delete(&source_stream);

    mz_stream_zlib_delete(&zlib_stream);
    return err;
}

/***************************************************************************/

#if !defined(MZ_ZIP_NO_MAIN)
int main(int argc, const char *argv[]) {
    int16_t operation_level = MZ_COMPRESS_LEVEL_DEFAULT;
    int32_t path_arg = 0;
    int32_t err = 0;
    int32_t i = 0;
    uint8_t operation = MZ_GZIP_COMPRESS;
    const char *path = NULL;
    const char *destination = NULL;


    minigzip_banner();
    if (argc == 1) {
        minigzip_help();
        return 0;
    }

    /* Parse command line options */
    for (i = 1; i < argc; i += 1) {
        printf("%s ", argv[i]);
        if (argv[i][0] == '-') {
            char c = argv[i][1];
            if ((c == 'x') || (c == 'X'))
                operation = MZ_GZIP_DECOMPRESS;
            else if ((c >= '0') && (c <= '9'))
                operation_level = (c - '0');
            else if (((c == 'd') || (c == 'D')) && (i + 1 < argc)) {
                destination = argv[i + 1];
                printf("%s ", argv[i + 1]);
                i += 1;
            } else {
                err = MZ_SUPPORT_ERROR;
            }
        } else if (path_arg == 0) {
            path_arg = i;
            break;
        }
    }
    printf("\n");

    if (err == MZ_SUPPORT_ERROR) {
        printf("Feature not supported\n");
        return err;
    }

    if (path_arg == 0) {
        minigzip_help();
        return 0;
    }

    path = argv[path_arg];
    err = minigzip_copy(path, destination, operation, operation_level);

    return err;
}
#endif
