/* mz_os.c -- System functions
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_crypt.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_os.h"

#include <ctype.h> /* tolower */

/***************************************************************************/

int32_t mz_path_combine(char *path, const char *join, int32_t max_path) {
    int32_t path_len = 0;

    if (path == NULL || join == NULL || max_path == 0)
        return MZ_PARAM_ERROR;

    path_len = (int32_t)strlen(path);

    if (path_len == 0) {
        strncpy(path, join, max_path - 1);
        path[max_path - 1] = 0;
    } else {
        mz_path_append_slash(path, max_path, MZ_PATH_SLASH_PLATFORM);
        strncat(path, join, max_path - path_len);
    }

    return MZ_OK;
}

int32_t mz_path_append_slash(char *path, int32_t max_path, char slash) {
    int32_t path_len = (int32_t)strlen(path);
    if ((path_len + 2) >= max_path)
        return MZ_BUF_ERROR;
    if (path[path_len - 1] != '\\' && path[path_len - 1] != '/') {
        path[path_len] = slash;
        path[path_len + 1] = 0;
    }
    return MZ_OK;
}

int32_t mz_path_remove_slash(char *path) {
    int32_t path_len = (int32_t)strlen(path);
    while (path_len > 0) {
        if (path[path_len - 1] == '\\' || path[path_len - 1] == '/')
            path[path_len - 1] = 0;
        else
            break;

        path_len -= 1;
    }
    return MZ_OK;
}

int32_t mz_path_has_slash(const char *path) {
    int32_t path_len = (int32_t)strlen(path);
    if (path[path_len - 1] != '\\' && path[path_len - 1] != '/')
        return MZ_EXIST_ERROR;
    return MZ_OK;
}

int32_t mz_path_convert_slashes(char *path, char slash) {
    int32_t i = 0;

    for (i = 0; i < (int32_t)strlen(path); i += 1) {
        if (path[i] == '\\' || path[i] == '/')
            path[i] = slash;
    }
    return MZ_OK;
}

int32_t mz_path_compare_wc(const char *path, const char *wildcard, uint8_t ignore_case) {
    while (*path != 0) {
        switch (*wildcard) {
        case '*':

            if (*(wildcard + 1) == 0)
                return MZ_OK;

            while (*path != 0) {
                if (mz_path_compare_wc(path, (wildcard + 1), ignore_case) == MZ_OK)
                    return MZ_OK;

                path += 1;
            }

            return MZ_EXIST_ERROR;

        default:
            /* Ignore differences in path slashes on platforms */
            if ((*path == '\\' && *wildcard == '/') || (*path == '/' && *wildcard == '\\'))
                break;

            if (ignore_case) {
                if (tolower(*path) != tolower(*wildcard))
                    return MZ_EXIST_ERROR;
            } else {
                if (*path != *wildcard)
                    return MZ_EXIST_ERROR;
            }

            break;
        }

        path += 1;
        wildcard += 1;
    }

    if ((*wildcard != 0) && (*wildcard != '*'))
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_path_resolve(const char *path, char *output, int32_t max_output) {
    const char *source = path;
    const char *check = output;
    char *target = output;


    if (max_output <= 0)
        return MZ_PARAM_ERROR;

    while (*source != 0 && max_output > 1) {
        check = source;
        if ((*check == '\\') || (*check == '/'))
            check += 1;

        if ((source == path) || (target == output) || (check != source)) {
            /* Skip double paths */
            if ((*check == '\\') || (*check == '/')) {
                source += 1;
                continue;
            }
            if (*check == '.') {
                check += 1;

                /* Remove . if at end of string and not at the beginning */
                if ((*check == 0) && (source != path && target != output)) {
                    /* Copy last slash */
                    *target = *source;
                    target += 1;
                    max_output -= 1;
                    source += (check - source);
                    continue;
                }
                /* Remove . if not at end of string */
                else if ((*check == '\\') || (*check == '/')) {
                    source += (check - source);
                    /* Skip slash if at beginning of string */
                    if (target == output && *source != 0)
                        source += 1;
                    continue;
                }
                /* Go to parent directory .. */
                else if (*check == '.') {
                    check += 1;
                    if ((*check == 0) || (*check == '\\' || *check == '/')) {
                        source += (check - source);

                        /* Search backwards for previous slash */
                        if (target != output) {
                            target -= 1;
                            do {
                                if ((*target == '\\') || (*target == '/'))
                                    break;

                                target -= 1;
                                max_output += 1;
                            } while (target > output);
                        }

                        if ((target == output) && (*source != 0))
                            source += 1;
                        if ((*target == '\\' || *target == '/') && (*source == 0))
                            target += 1;

                        *target = 0;
                        continue;
                    }
                }
            }
        }

        *target = *source;

        source += 1;
        target += 1;
        max_output -= 1;
    }

    *target = 0;

    if (*path == 0)
        return MZ_INTERNAL_ERROR;

    return MZ_OK;
}

int32_t mz_path_remove_filename(char *path) {
    char *path_ptr = NULL;

    if (path == NULL)
        return MZ_PARAM_ERROR;

    path_ptr = path + strlen(path) - 1;

    while (path_ptr > path) {
        if ((*path_ptr == '/') || (*path_ptr == '\\')) {
            *path_ptr = 0;
            break;
        }

        path_ptr -= 1;
    }

    if (path_ptr == path)
        *path_ptr = 0;

    return MZ_OK;
}

int32_t mz_path_remove_extension(char *path) {
    char *path_ptr = NULL;

    if (path == NULL)
        return MZ_PARAM_ERROR;

    path_ptr = path + strlen(path) - 1;

    while (path_ptr > path) {
        if ((*path_ptr == '/') || (*path_ptr == '\\'))
            break;
        if (*path_ptr == '.') {
            *path_ptr = 0;
            break;
        }

        path_ptr -= 1;
    }

    if (path_ptr == path)
        *path_ptr = 0;

    return MZ_OK;
}

int32_t mz_path_get_filename(const char *path, const char **filename) {
    const char *match = NULL;

    if (path == NULL || filename == NULL)
        return MZ_PARAM_ERROR;

    *filename = NULL;

    for (match = path; *match != 0; match += 1) {
        if ((*match == '\\') || (*match == '/'))
            *filename = match + 1;
    }

    if (*filename == NULL)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_dir_make(const char *path) {
    int32_t err = MZ_OK;
    int16_t len = 0;
    char *current_dir = NULL;
    char *match = NULL;
    char hold = 0;


    len = (int16_t)strlen(path);
    if (len <= 0)
        return 0;

    current_dir = (char *)MZ_ALLOC((uint16_t)len + 1);
    if (current_dir == NULL)
        return MZ_MEM_ERROR;

    strcpy(current_dir, path);
    mz_path_remove_slash(current_dir);

    err = mz_os_make_dir(current_dir);
    if (err != MZ_OK) {
        match = current_dir + 1;
        while (1) {
            while (*match != 0 && *match != '\\' && *match != '/')
                match += 1;
            hold = *match;
            *match = 0;

            err = mz_os_make_dir(current_dir);
            if (err != MZ_OK)
                break;
            if (hold == 0)
                break;

            *match = hold;
            match += 1;
        }
    }

    MZ_FREE(current_dir);
    return err;
}

int32_t mz_file_get_crc(const char *path, uint32_t *result_crc) {
    void *stream = NULL;
    uint32_t crc32 = 0;
    int32_t read = 0;
    int32_t err = MZ_OK;
    uint8_t buf[16384];

    mz_stream_os_create(&stream);

    err = mz_stream_os_open(stream, path, MZ_OPEN_MODE_READ);

    if (err == MZ_OK) {
        do {
            read = mz_stream_os_read(stream, buf, sizeof(buf));

            if (read < 0) {
                err = read;
                break;
            }

            crc32 = mz_crypt_crc32_update(crc32, buf, read);
        } while ((err == MZ_OK) && (read > 0));

        mz_stream_os_close(stream);
    }

    *result_crc = crc32;

    mz_stream_os_delete(&stream);

    return err;
}

/***************************************************************************/
