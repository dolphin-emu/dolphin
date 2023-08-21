/* minizip.c
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/


#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include <stdio.h>  /* printf */

/***************************************************************************/

typedef struct minizip_opt_s {
    uint8_t     include_path;
    int16_t     compress_level;
    uint8_t     compress_method;
    uint8_t     overwrite;
    uint8_t     append;
    int64_t     disk_size;
    uint8_t     follow_links;
    uint8_t     store_links;
    uint8_t     zip_cd;
    int32_t     encoding;
    uint8_t     verbose;
    uint8_t     aes;
    const char *cert_path;
    const char *cert_pwd;
} minizip_opt;

/***************************************************************************/

int32_t minizip_banner(void);
int32_t minizip_help(void);

int32_t minizip_list(const char *path);

int32_t minizip_add_entry_cb(void *handle, void *userdata, mz_zip_file *file_info);
int32_t minizip_add_progress_cb(void *handle, void *userdata, mz_zip_file *file_info, int64_t position);
int32_t minizip_add_overwrite_cb(void *handle, void *userdata, const char *path);
int32_t minizip_add(const char *path, const char *password, minizip_opt *options, int32_t arg_count, const char **args);

int32_t minizip_extract_entry_cb(void *handle, void *userdata, mz_zip_file *file_info, const char *path);
int32_t minizip_extract_progress_cb(void *handle, void *userdata, mz_zip_file *file_info, int64_t position);
int32_t minizip_extract_overwrite_cb(void *handle, void *userdata, mz_zip_file *file_info, const char *path);
int32_t minizip_extract(const char *path, const char *pattern, const char *destination, const char *password, minizip_opt *options);

int32_t minizip_erase(const char *src_path, const char *target_path, int32_t arg_count, const char **args);

/***************************************************************************/

int32_t minizip_banner(void) {
    printf("minizip-ng %s - https://github.com/zlib-ng/minizip-ng\n", MZ_VERSION);
    printf("---------------------------------------------------\n");
    return MZ_OK;
}

int32_t minizip_help(void) {
    printf("Usage: minizip [-x][-d dir|-l|-e][-o][-f][-y][-c cp][-a][-0 to -9][-b|-m|-t][-k 512][-p pwd][-s] file.zip [files]\n\n" \
           "  -x  Extract files\n" \
           "  -l  List files\n" \
           "  -d  Destination directory\n" \
           "  -e  Erase files\n" \
           "  -o  Overwrite existing files\n" \
           "  -c  File names use cp437 encoding (or specified codepage)\n" \
           "  -a  Append to existing zip file\n" \
           "  -i  Include full path of files\n" \
           "  -f  Follow symbolic links\n" \
           "  -y  Store symbolic links\n" \
           "  -v  Verbose info\n" \
           "  -0  Store only\n" \
           "  -1  Compress faster\n" \
           "  -9  Compress better\n" \
           "  -k  Disk size in KB\n" \
           "  -z  Zip central directory\n" \
           "  -p  Encryption password\n" \
           "  -s  AES encryption\n" \
           "  -h  PKCS12 certificate path\n" \
           "  -w  PKCS12 certificate password\n" \
           "  -b  BZIP2 compression\n" \
           "  -m  LZMA compression\n" \
           "  -n  XZ compression\n" \
           "  -t  ZSTD compression\n\n");
    return MZ_OK;
}

/***************************************************************************/

int32_t minizip_list(const char *path) {
    mz_zip_file *file_info = NULL;
    uint32_t ratio = 0;
    int32_t err = MZ_OK;
    struct tm tmu_date;
    const char *method = NULL;
    char crypt = ' ';
    void *reader = NULL;


    mz_zip_reader_create(&reader);
    err = mz_zip_reader_open_file(reader, path);
    if (err != MZ_OK) {
        printf("Error %" PRId32 " opening archive %s\n", err, path);
        mz_zip_reader_delete(&reader);
        return err;
    }

    err = mz_zip_reader_goto_first_entry(reader);

    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        printf("Error %" PRId32 " going to first entry in archive\n", err);
        mz_zip_reader_delete(&reader);
        return err;
    }

    printf("      Packed     Unpacked Ratio Method   Attribs Date     Time  CRC-32     Name\n");
    printf("      ------     -------- ----- ------   ------- ----     ----  ------     ----\n");

    /* Enumerate all entries in the archive */
    do {
        err = mz_zip_reader_entry_get_info(reader, &file_info);

        if (err != MZ_OK) {
            printf("Error %" PRId32 " getting entry info in archive\n", err);
            break;
        }

        ratio = 0;
        if (file_info->uncompressed_size > 0)
            ratio = (uint32_t)((file_info->compressed_size * 100) / file_info->uncompressed_size);

        /* Display a '*' if the file is encrypted */
        if (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED)
            crypt = '*';
        else
            crypt = ' ';

        method = mz_zip_get_compression_method_string(file_info->compression_method);
        mz_zip_time_t_to_tm(file_info->modified_date, &tmu_date);

        /* Print entry information */
        printf("%12" PRId64 " %12" PRId64 "  %3" PRIu32 "%% %6s%c %8" PRIx32 " %2.2" PRIu32 \
               "-%2.2" PRIu32 "-%2.2" PRIu32 " %2.2" PRIu32 ":%2.2" PRIu32 " %8.8" PRIx32 "   %s\n",
               file_info->compressed_size, file_info->uncompressed_size, ratio,
               method, crypt, file_info->external_fa,
               (uint32_t)tmu_date.tm_mon + 1, (uint32_t)tmu_date.tm_mday,
               (uint32_t)tmu_date.tm_year % 100,
               (uint32_t)tmu_date.tm_hour, (uint32_t)tmu_date.tm_min,
               file_info->crc, file_info->filename);

        err = mz_zip_reader_goto_next_entry(reader);

        if (err != MZ_OK && err != MZ_END_OF_LIST) {
            printf("Error %" PRId32 " going to next entry in archive\n", err);
            break;
        }
    } while (err == MZ_OK);

    mz_zip_reader_delete(&reader);

    if (err == MZ_END_OF_LIST)
        return MZ_OK;

    return err;
}

/***************************************************************************/

int32_t minizip_add_entry_cb(void *handle, void *userdata, mz_zip_file *file_info) {
    MZ_UNUSED(handle);
    MZ_UNUSED(userdata);

    /* Print the current file we are trying to compress */
    printf("Adding %s\n", file_info->filename);
    return MZ_OK;
}

int32_t minizip_add_progress_cb(void *handle, void *userdata, mz_zip_file *file_info, int64_t position) {
    minizip_opt *options = (minizip_opt *)userdata;
    double progress = 0;
    uint8_t raw = 0;

    MZ_UNUSED(userdata);

    mz_zip_writer_get_raw(handle, &raw);

    if (raw && file_info->compressed_size > 0)
        progress = ((double)position / file_info->compressed_size) * 100;
    else if (!raw && file_info->uncompressed_size > 0)
        progress = ((double)position / file_info->uncompressed_size) * 100;

    /* Print the progress of the current compress operation */
    if (options->verbose)
        printf("%s - %" PRId64 " / %" PRId64 " (%.02f%%)\n", file_info->filename, position,
            file_info->uncompressed_size, progress);
    return MZ_OK;
}

int32_t minizip_add_overwrite_cb(void *handle, void *userdata, const char *path) {
    minizip_opt *options = (minizip_opt *)userdata;

    MZ_UNUSED(handle);

    if (options->overwrite == 0) {
        /* If ask the user what to do because append and overwrite args not set */
        char rep = 0;
        do {
            char answer[128];
            printf("The file %s exists. Overwrite ? [y]es, [n]o, [a]ppend : ", path);
            if (scanf("%1s", answer) != 1)
                exit(EXIT_FAILURE);
            rep = answer[0];

            if ((rep >= 'a') && (rep <= 'z'))
                rep -= 0x20;
        } while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));

        if (rep == 'A') {
            return MZ_EXIST_ERROR;
        } else if (rep == 'N') {
            return MZ_INTERNAL_ERROR;
        }
    }

    return MZ_OK;
}

int32_t minizip_add(const char *path, const char *password, minizip_opt *options, int32_t arg_count, const char **args) {
    void *writer = NULL;
    int32_t err = MZ_OK;
    int32_t err_close = MZ_OK;
    int32_t i = 0;
    const char *filename_in_zip = NULL;


    printf("Archive %s\n", path);

    /* Create zip writer */
    mz_zip_writer_create(&writer);
    mz_zip_writer_set_password(writer, password);
    mz_zip_writer_set_aes(writer, options->aes);
    mz_zip_writer_set_compress_method(writer, options->compress_method);
    mz_zip_writer_set_compress_level(writer, options->compress_level);
    mz_zip_writer_set_follow_links(writer, options->follow_links);
    mz_zip_writer_set_store_links(writer, options->store_links);
    mz_zip_writer_set_overwrite_cb(writer, options, minizip_add_overwrite_cb);
    mz_zip_writer_set_progress_cb(writer, options, minizip_add_progress_cb);
    mz_zip_writer_set_entry_cb(writer, options, minizip_add_entry_cb);
    mz_zip_writer_set_zip_cd(writer, options->zip_cd);
    if (options->cert_path != NULL)
        mz_zip_writer_set_certificate(writer, options->cert_path, options->cert_pwd);

    err = mz_zip_writer_open_file(writer, path, options->disk_size, options->append);

    if (err == MZ_OK) {
        for (i = 0; i < arg_count; i += 1) {
            filename_in_zip = args[i];

            /* Add file system path to archive */
            err = mz_zip_writer_add_path(writer, filename_in_zip, NULL, options->include_path, 1);
            if (err != MZ_OK)
                printf("Error %" PRId32 " adding path to archive %s\n", err, filename_in_zip);
        }
    } else {
        printf("Error %" PRId32 " opening archive for writing\n", err);
    }

    err_close = mz_zip_writer_close(writer);
    if (err_close != MZ_OK) {
        printf("Error %" PRId32 " closing archive for writing %s\n", err_close, path);
        err = err_close;
    }

    mz_zip_writer_delete(&writer);
    return err;
}

/***************************************************************************/

int32_t minizip_extract_entry_cb(void *handle, void *userdata, mz_zip_file *file_info, const char *path) {
    MZ_UNUSED(handle);
    MZ_UNUSED(userdata);
    MZ_UNUSED(path);

    /* Print the current entry extracting */
    printf("Extracting %s\n", file_info->filename);
    return MZ_OK;
}

int32_t minizip_extract_progress_cb(void *handle, void *userdata, mz_zip_file *file_info, int64_t position) {
    minizip_opt *options = (minizip_opt *)userdata;
    double progress = 0;
    uint8_t raw = 0;

    MZ_UNUSED(userdata);

    mz_zip_reader_get_raw(handle, &raw);

    if (raw && file_info->compressed_size > 0)
        progress = ((double)position / file_info->compressed_size) * 100;
    else if (!raw && file_info->uncompressed_size > 0)
        progress = ((double)position / file_info->uncompressed_size) * 100;

    /* Print the progress of the current extraction */
    if (options->verbose)
        printf("%s - %" PRId64 " / %" PRId64 " (%.02f%%)\n", file_info->filename, position,
            file_info->uncompressed_size, progress);

    return MZ_OK;
}

int32_t minizip_extract_overwrite_cb(void *handle, void *userdata, mz_zip_file *file_info, const char *path) {
    minizip_opt *options = (minizip_opt *)userdata;

    MZ_UNUSED(handle);
    MZ_UNUSED(file_info);

    /* Verify if we want to overwrite current entry on disk */
    if (options->overwrite == 0) {
        char rep = 0;
        do {
            char answer[128];
            printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ", path);
            if (scanf("%1s", answer) != 1)
                exit(EXIT_FAILURE);
            rep = answer[0];
            if ((rep >= 'a') && (rep <= 'z'))
                rep -= 0x20;
        } while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));

        if (rep == 'N')
            return MZ_EXIST_ERROR;
        if (rep == 'A')
            options->overwrite = 1;
    }

    return MZ_OK;
}

int32_t minizip_extract(const char *path, const char *pattern, const char *destination, const char *password, minizip_opt *options) {
    void *reader = NULL;
    int32_t err = MZ_OK;
    int32_t err_close = MZ_OK;


    printf("Archive %s\n", path);

    /* Create zip reader */
    mz_zip_reader_create(&reader);
    mz_zip_reader_set_pattern(reader, pattern, 1);
    mz_zip_reader_set_password(reader, password);
    mz_zip_reader_set_encoding(reader, options->encoding);
    mz_zip_reader_set_entry_cb(reader, options, minizip_extract_entry_cb);
    mz_zip_reader_set_progress_cb(reader, options, minizip_extract_progress_cb);
    mz_zip_reader_set_overwrite_cb(reader, options, minizip_extract_overwrite_cb);

    err = mz_zip_reader_open_file(reader, path);

    if (err != MZ_OK) {
        printf("Error %" PRId32 " opening archive %s\n", err, path);
    } else {
        /* Save all entries in archive to destination directory */
        err = mz_zip_reader_save_all(reader, destination);

        if (err == MZ_END_OF_LIST) {
            if (pattern != NULL) {
                printf("Files matching %s not found in archive\n", pattern);
            } else {
                printf("No files in archive\n");
                err = MZ_OK;
            }
        } else if (err != MZ_OK) {
            printf("Error %" PRId32 " saving entries to disk %s\n", err, path);
        }
    }

    err_close = mz_zip_reader_close(reader);
    if (err_close != MZ_OK) {
        printf("Error %" PRId32 " closing archive for reading\n", err_close);
        err = err_close;
    }

    mz_zip_reader_delete(&reader);
    return err;
}

/***************************************************************************/

int32_t minizip_erase(const char *src_path, const char *target_path, int32_t arg_count, const char **args) {
    mz_zip_file *file_info = NULL;
    const char *filename_in_zip = NULL;
    const char *target_path_ptr = target_path;
    void *reader = NULL;
    void *writer = NULL;
    int32_t skip = 0;
    int32_t err = MZ_OK;
    int32_t i = 0;
    uint8_t zip_cd = 0;
    char bak_path[256];
    char tmp_path[256];

    if (target_path == NULL) {
        /* Construct temporary zip name */
        strncpy(tmp_path, src_path, sizeof(tmp_path) - 1);
        tmp_path[sizeof(tmp_path) - 1] = 0;
        strncat(tmp_path, ".tmp.zip", sizeof(tmp_path) - strlen(tmp_path) - 1);
        target_path_ptr = tmp_path;
    }

    mz_zip_reader_create(&reader);
    mz_zip_writer_create(&writer);

    /* Open original archive we want to erase an entry in */
    err = mz_zip_reader_open_file(reader, src_path);
    if (err != MZ_OK) {
        printf("Error %" PRId32 " opening archive for reading %s\n", err, src_path);
        mz_zip_reader_delete(&reader);
        return err;
    }

    /* Open temporary archive */
    err = mz_zip_writer_open_file(writer, target_path_ptr, 0, 0);
    if (err != MZ_OK) {
        printf("Error %" PRId32 " opening archive for writing %s\n", err, target_path_ptr);
        mz_zip_reader_delete(&reader);
        mz_zip_writer_delete(&writer);
        return err;
    }

    err = mz_zip_reader_goto_first_entry(reader);

    if (err != MZ_OK && err != MZ_END_OF_LIST)
        printf("Error %" PRId32 " going to first entry in archive\n", err);

    while (err == MZ_OK) {
        err = mz_zip_reader_entry_get_info(reader, &file_info);
        if (err != MZ_OK) {
            printf("Error %" PRId32 " getting info from archive\n", err);
            break;
        }

        /* Copy all entries from original archive to temporary archive
           except the ones we don't want */
        for (i = 0, skip = 0; i < arg_count; i += 1) {
            filename_in_zip = args[i];

            if (mz_path_compare_wc(file_info->filename, filename_in_zip, 1) == MZ_OK)
                skip = 1;
        }

        if (skip) {
            printf("Skipping %s\n", file_info->filename);
        } else {
            printf("Copying %s\n", file_info->filename);
            err = mz_zip_writer_copy_from_reader(writer, reader);
        }

        if (err != MZ_OK) {
            printf("Error %" PRId32 " copying entry into new zip\n", err);
            break;
        }

        err = mz_zip_reader_goto_next_entry(reader);

        if (err != MZ_OK && err != MZ_END_OF_LIST)
            printf("Error %" PRId32 " going to next entry in archive\n", err);
    }

    mz_zip_reader_get_zip_cd(reader, &zip_cd);
    mz_zip_writer_set_zip_cd(writer, zip_cd);

    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);

    mz_zip_writer_close(writer);
    mz_zip_writer_delete(&writer);

    if (err == MZ_END_OF_LIST) {
        if (target_path == NULL) {
            /* Swap original archive with temporary archive, backup old archive if possible */
            strncpy(bak_path, src_path, sizeof(bak_path) - 1);
            bak_path[sizeof(bak_path) - 1] = 0;
            strncat(bak_path, ".bak", sizeof(bak_path) - strlen(bak_path) - 1);

            if (mz_os_file_exists(bak_path) == MZ_OK)
                mz_os_unlink(bak_path);

            if (mz_os_rename(src_path, bak_path) != MZ_OK)
                printf("Error backing up archive before replacing %s\n", bak_path);

            if (mz_os_rename(tmp_path, src_path) != MZ_OK)
                printf("Error replacing archive with temp %s\n", tmp_path);
        }

        return MZ_OK;
    }

    return err;
}

/***************************************************************************/

#if !defined(MZ_ZIP_NO_MAIN)
int main(int argc, const char *argv[]) {
    minizip_opt options;
    int32_t path_arg = 0;
    int32_t err = 0;
    int32_t i = 0;
    uint8_t do_list = 0;
    uint8_t do_extract = 0;
    uint8_t do_erase = 0;
    const char *path = NULL;
    const char *password = NULL;
    const char *destination = NULL;
    const char *filename_to_extract = NULL;


    minizip_banner();
    if (argc == 1) {
        minizip_help();
        return 0;
    }

    memset(&options, 0, sizeof(options));

    options.compress_method = MZ_COMPRESS_METHOD_DEFLATE;
    options.compress_level = MZ_COMPRESS_LEVEL_DEFAULT;

    /* Parse command line options */
    for (i = 1; i < argc; i += 1) {
        printf("%s ", argv[i]);
        if (argv[i][0] == '-') {
            char c = argv[i][1];
            if ((c == 'l') || (c == 'L'))
                do_list = 1;
            else if ((c == 'x') || (c == 'X'))
                do_extract = 1;
            else if ((c == 'e') || (c == 'E'))
                do_erase = 1;
            else if ((c == 'a') || (c == 'A'))
                options.append = 1;
            else if ((c == 'o') || (c == 'O'))
                options.overwrite = 1;
            else if ((c == 'f') || (c == 'F'))
                options.follow_links = 1;
            else if ((c == 'y') || (c == 'Y'))
                options.store_links = 1;
            else if ((c == 'i') || (c == 'I'))
                options.include_path = 1;
            else if ((c == 'z') || (c == 'Z'))
                options.zip_cd = 1;
            else if ((c == 'v') || (c == 'V'))
                options.verbose = 1;
            else if ((c >= '0') && (c <= '9')) {
                options.compress_level = (c - '0');
                if (options.compress_level == 0)
                    options.compress_method = MZ_COMPRESS_METHOD_STORE;
            } else if ((c == 'b') || (c == 'B'))
#ifdef HAVE_BZIP2
                options.compress_method = MZ_COMPRESS_METHOD_BZIP2;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if ((c == 'm') || (c == 'M'))
#ifdef HAVE_LZMA
                options.compress_method = MZ_COMPRESS_METHOD_LZMA;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if ((c == 'n') || (c == 'N'))
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
                options.compress_method = MZ_COMPRESS_METHOD_XZ;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if ((c == 't') || (c == 'T'))
#ifdef HAVE_ZSTD
                options.compress_method = MZ_COMPRESS_METHOD_ZSTD;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if ((c == 's') || (c == 'S'))
#ifdef HAVE_WZAES
                options.aes = 1;
#else
                err = MZ_SUPPORT_ERROR;
#endif
            else if (((c == 'h') || (c == 'H')) && (i + 1 < argc)) {
#ifdef MZ_ZIP_SIGNING
                options.cert_path = argv[i + 1];
                printf("%s ", argv[i + 1]);
#else
                err = MZ_SUPPORT_ERROR;
#endif
                i += 1;
            } else if (((c == 'w') || (c == 'W')) && (i + 1 < argc)) {
#ifdef MZ_ZIP_SIGNING
                options.cert_pwd = argv[i + 1];
                printf("%s ", argv[i + 1]);
#else
                err = MZ_SUPPORT_ERROR;
#endif
                i += 1;
            } else if (((c == 'c') || (c == 'C')) && (i + 1 < argc)) {
                options.encoding = (int32_t)atoi(argv[i + 1]);
                i += 1;
            } else if (((c == 'k') || (c == 'K')) && (i + 1 < argc)) {
                options.disk_size = (int64_t)atoi(argv[i + 1]) * 1024;
                printf("%s ", argv[i + 1]);
                i += 1;
            } else if (((c == 'd') || (c == 'D')) && (i + 1 < argc)) {
                destination = argv[i + 1];
                printf("%s ", argv[i + 1]);
                i += 1;
            } else if (((c == 'p') || (c == 'P')) && (i + 1 < argc)) {
#ifndef MZ_ZIP_NO_ENCRYPTION
                password = argv[i + 1];
                printf("*** ");
#else
                err = MZ_SUPPORT_ERROR;
#endif
                i += 1;
            }
        } else if (path_arg == 0)
            path_arg = i;
    }
    printf("\n");

    if (err == MZ_SUPPORT_ERROR) {
        printf("Feature not supported\n");
        return err;
    }

    if (path_arg == 0) {
        minizip_help();
        return 0;
    }

    path = argv[path_arg];

    if (do_list) {
        /* List archive contents */
        err = minizip_list(path);
    } else if (do_extract) {
        if (argc > path_arg + 1)
            filename_to_extract = argv[path_arg + 1];

        /* Extract archive */
        err = minizip_extract(path, filename_to_extract, destination, password, &options);
    } else if (do_erase) {
        /* Erase file in archive */
        err = minizip_erase(path, NULL, argc - (path_arg + 1), &argv[path_arg + 1]);
    } else {
        /* Add files to archive */
        err = minizip_add(path, password, &options, argc - (path_arg + 1), &argv[path_arg + 1]);
    }

    return err;
}
#endif
