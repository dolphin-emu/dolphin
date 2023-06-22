/* mz_os_posix.c -- System functions for posix
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_strm.h"
#include "mz_os.h"

#include <stdio.h> /* rename */
#include <errno.h>
#if defined(HAVE_ICONV)
#include <iconv.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#  include <utime.h>
#  include <unistd.h>
#endif
#if defined(__APPLE__)
#  include <mach/clock.h>
#  include <mach/mach.h>
#endif

#if defined(HAVE_GETRANDOM)
#  include <sys/random.h>
#endif
#if defined(HAVE_LIBBSD)
#  include <sys/types.h>
#  ifndef __u_char_defined
     typedef unsigned char  u_char;
#  endif
#  include <bsd/stdlib.h> /* arc4random_buf */
#endif

/***************************************************************************/

#if defined(HAVE_ICONV)
uint8_t *mz_os_utf8_string_create(const char *string, int32_t encoding) {
    iconv_t cd;
    const char *from_encoding = NULL;
    size_t result = 0;
    size_t string_length = 0;
    size_t string_utf8_size = 0;
    uint8_t *string_utf8 = NULL;
    uint8_t *string_utf8_ptr = NULL;

    if (string == NULL)
        return NULL;

    if (encoding == MZ_ENCODING_CODEPAGE_437)
        from_encoding = "CP437";
    else if (encoding == MZ_ENCODING_CODEPAGE_932)
        from_encoding = "CP932";
    else if (encoding == MZ_ENCODING_CODEPAGE_936)
        from_encoding = "CP936";
    else if (encoding == MZ_ENCODING_CODEPAGE_950)
        from_encoding = "CP950";
    else if (encoding == MZ_ENCODING_UTF8)
        from_encoding = "UTF-8";
    else
        return NULL;

    cd = iconv_open("UTF-8", from_encoding);
    if (cd == (iconv_t)-1)
        return NULL;

    string_length = strlen(string);
    string_utf8_size = string_length * 2;
    string_utf8 = (uint8_t *)MZ_ALLOC((int32_t)(string_utf8_size + 1));
    string_utf8_ptr = string_utf8;

    if (string_utf8) {
        memset(string_utf8, 0, string_utf8_size + 1);

        result = iconv(cd, (char **)&string, &string_length,
                (char **)&string_utf8_ptr, &string_utf8_size);
    }

    iconv_close(cd);

    if (result == (size_t)-1) {
        MZ_FREE(string_utf8);
        string_utf8 = NULL;
    }

    return string_utf8;
}
#else
uint8_t *mz_os_utf8_string_create(const char *string, int32_t encoding) {
    size_t string_length = 0;
    uint8_t *string_copy = NULL;

    string_length = strlen(string);
    string_copy = (uint8_t *)MZ_ALLOC((int32_t)(string_length + 1));
    strncpy((char *)string_copy, string, string_length);
    string_copy[string_length] = 0;

    return string_copy;
}
#endif

void mz_os_utf8_string_delete(uint8_t **string) {
    if (string != NULL) {
        MZ_FREE(*string);
        *string = NULL;
    }
}

/***************************************************************************/

#if defined(HAVE_ARC4RANDOM_BUF)
int32_t mz_os_rand(uint8_t *buf, int32_t size) {
    if (size < 0)
        return 0;
    arc4random_buf(buf, (uint32_t)size);
    return size;
}
#elif defined(HAVE_ARC4RANDOM)
int32_t mz_os_rand(uint8_t *buf, int32_t size) {
    int32_t left = size;
    for (; left > 2; left -= 3, buf += 3) {
        uint32_t val = arc4random();

        buf[0] = (val) & 0xFF;
        buf[1] = (val >> 8) & 0xFF;
        buf[2] = (val >> 16) & 0xFF;
    }
    for (; left > 0; left--, buf++) {
        *buf = arc4random() & 0xFF;
    }
    return size - left;
}
#elif defined(HAVE_GETRANDOM)
int32_t mz_os_rand(uint8_t *buf, int32_t size) {
    int32_t left = size;
    int32_t written = 0;

    while (left > 0) {
        written = getrandom(buf, left, 0);
        if (written < 0)
            return MZ_INTERNAL_ERROR;

        buf += written;
        left -= written;
    }
    return size - left;
}
#else
int32_t mz_os_rand(uint8_t *buf, int32_t size) {
    static unsigned calls = 0;
    int32_t i = 0;

    /* Ensure different random header each time */
    if (++calls == 1) {
        #define PI_SEED 3141592654UL
        srand((unsigned)(time(NULL) ^ PI_SEED));
    }

    while (i < size)
        buf[i++] = (rand() >> 7) & 0xff;

    return size;
}
#endif

int32_t mz_os_rename(const char *source_path, const char *target_path) {
    if (rename(source_path, target_path) == -1)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_os_unlink(const char *path) {
    if (unlink(path) == -1)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_os_file_exists(const char *path) {
    struct stat path_stat;

    memset(&path_stat, 0, sizeof(path_stat));
    if (stat(path, &path_stat) == 0)
        return MZ_OK;
    return MZ_EXIST_ERROR;
}

int64_t mz_os_get_file_size(const char *path) {
    struct stat path_stat;

    memset(&path_stat, 0, sizeof(path_stat));
    if (stat(path, &path_stat) == 0) {
        /* Stat returns size taken up by directory entry, so return 0 */
        if (S_ISDIR(path_stat.st_mode))
            return 0;

        return path_stat.st_size;
    }

    return 0;
}

int32_t mz_os_get_file_date(const char *path, time_t *modified_date, time_t *accessed_date, time_t *creation_date) {
    struct stat path_stat;
    char *name = NULL;
    size_t len = 0;
    int32_t err = MZ_INTERNAL_ERROR;

    memset(&path_stat, 0, sizeof(path_stat));

    if (strcmp(path, "-") != 0) {
        /* Not all systems allow stat'ing a file with / appended */
        len = strlen(path);
        name = (char *)malloc(len + 1);
        strncpy(name, path, len + 1);
        mz_path_remove_slash(name);

        if (stat(name, &path_stat) == 0) {
            if (modified_date != NULL)
                *modified_date = path_stat.st_mtime;
            if (accessed_date != NULL)
                *accessed_date = path_stat.st_atime;
            /* Creation date not supported */
            if (creation_date != NULL)
                *creation_date = 0;

            err = MZ_OK;
        }

        free(name);
    }

    return err;
}

int32_t mz_os_set_file_date(const char *path, time_t modified_date, time_t accessed_date, time_t creation_date) {
    struct utimbuf ut;

    ut.actime = accessed_date;
    ut.modtime = modified_date;

    /* Creation date not supported */
    MZ_UNUSED(creation_date);

    if (utime(path, &ut) != 0)
        return MZ_INTERNAL_ERROR;

    return MZ_OK;
}

int32_t mz_os_get_file_attribs(const char *path, uint32_t *attributes) {
    struct stat path_stat;
    int32_t err = MZ_OK;

    memset(&path_stat, 0, sizeof(path_stat));
    if (lstat(path, &path_stat) == -1)
        err = MZ_INTERNAL_ERROR;
    *attributes = path_stat.st_mode;
    return err;
}

int32_t mz_os_set_file_attribs(const char *path, uint32_t attributes) {
    int32_t err = MZ_OK;

    if (chmod(path, (mode_t)attributes) == -1)
        err = MZ_INTERNAL_ERROR;

    return err;
}

int32_t mz_os_make_dir(const char *path) {
    int32_t err = 0;

    err = mkdir(path, 0755);

    if (err != 0 && errno != EEXIST)
        return MZ_INTERNAL_ERROR;

    return MZ_OK;
}

DIR* mz_os_open_dir(const char *path) {
    return opendir(path);
}

struct dirent* mz_os_read_dir(DIR *dir) {
    if (dir == NULL)
        return NULL;
    return readdir(dir);
}

int32_t mz_os_close_dir(DIR *dir) {
    if (dir == NULL)
        return MZ_PARAM_ERROR;
    if (closedir(dir) == -1)
        return MZ_INTERNAL_ERROR;
    return MZ_OK;
}

int32_t mz_os_is_dir(const char *path) {
    struct stat path_stat;

    memset(&path_stat, 0, sizeof(path_stat));
    stat(path, &path_stat);
    if (S_ISDIR(path_stat.st_mode))
        return MZ_OK;

    return MZ_EXIST_ERROR;
}

int32_t mz_os_is_symlink(const char *path) {
    struct stat path_stat;

    memset(&path_stat, 0, sizeof(path_stat));
    lstat(path, &path_stat);
    if (S_ISLNK(path_stat.st_mode))
        return MZ_OK;

    return MZ_EXIST_ERROR;
}

int32_t mz_os_make_symlink(const char *path, const char *target_path) {
    if (symlink(target_path, path) != 0)
        return MZ_INTERNAL_ERROR;
    return MZ_OK;
}

int32_t mz_os_read_symlink(const char *path, char *target_path, int32_t max_target_path) {
    size_t length = 0;

    length = (size_t)readlink(path, target_path, max_target_path - 1);
    if (length == (size_t)-1)
        return MZ_EXIST_ERROR;

    target_path[length] = 0;
    return MZ_OK;
}

uint64_t mz_os_ms_time(void) {
    struct timespec ts;

#if defined(__APPLE__)
    clock_serv_t cclock;
    mach_timespec_t mts;

    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);

    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

    return ((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec / 1000000);
}
