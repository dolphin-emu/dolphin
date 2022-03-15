/* mz_zip_rw.c -- Zip reader/writer
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_crypt.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_mem.h"
#include "mz_strm_os.h"
#include "mz_strm_split.h"
#include "mz_strm_wzaes.h"
#include "mz_zip.h"

#include "mz_zip_rw.h"

/***************************************************************************/

#define MZ_DEFAULT_PROGRESS_INTERVAL    (1000u)

#define MZ_ZIP_CD_FILENAME              ("__cdcd__")

/***************************************************************************/

typedef struct mz_zip_reader_s {
    void        *zip_handle;
    void        *file_stream;
    void        *buffered_stream;
    void        *split_stream;
    void        *mem_stream;
    void        *hash;
    uint16_t    hash_algorithm;
    uint16_t    hash_digest_size;
    mz_zip_file *file_info;
    const char  *pattern;
    uint8_t     pattern_ignore_case;
    const char  *password;
    void        *overwrite_userdata;
    mz_zip_reader_overwrite_cb
                overwrite_cb;
    void        *password_userdata;
    mz_zip_reader_password_cb
                password_cb;
    void        *progress_userdata;
    mz_zip_reader_progress_cb
                progress_cb;
    uint32_t    progress_cb_interval_ms;
    void        *entry_userdata;
    mz_zip_reader_entry_cb
                entry_cb;
    uint8_t     raw;
    uint8_t     buffer[UINT16_MAX];
    int32_t     encoding;
    uint8_t     sign_required;
    uint8_t     cd_verified;
    uint8_t     cd_zipped;
    uint8_t     entry_verified;
    uint8_t     recover;
} mz_zip_reader;

/***************************************************************************/

int32_t mz_zip_reader_is_open(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (reader == NULL)
        return MZ_PARAM_ERROR;
    if (reader->zip_handle == NULL)
        return MZ_PARAM_ERROR;
    return MZ_OK;
}

int32_t mz_zip_reader_open(void *handle, void *stream) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;

    reader->cd_verified = 0;
    reader->cd_zipped = 0;

    mz_zip_create(&reader->zip_handle);
    mz_zip_set_recover(reader->zip_handle, reader->recover);

    err = mz_zip_open(reader->zip_handle, stream, MZ_OPEN_MODE_READ);

    if (err != MZ_OK) {
        mz_zip_reader_close(handle);
        return err;
    }

    mz_zip_reader_unzip_cd(reader);
    return MZ_OK;
}

int32_t mz_zip_reader_open_file(void *handle, const char *path) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;


    mz_zip_reader_close(handle);

    mz_stream_os_create(&reader->file_stream);
    mz_stream_buffered_create(&reader->buffered_stream);
    mz_stream_split_create(&reader->split_stream);

    mz_stream_set_base(reader->buffered_stream, reader->file_stream);
    mz_stream_set_base(reader->split_stream, reader->buffered_stream);

    err = mz_stream_open(reader->split_stream, path, MZ_OPEN_MODE_READ);
    if (err == MZ_OK)
        err = mz_zip_reader_open(handle, reader->split_stream);
    return err;
}

int32_t mz_zip_reader_open_file_in_memory(void *handle, const char *path) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    void *file_stream = NULL;
    int64_t file_size = 0;
    int32_t err = 0;


    mz_zip_reader_close(handle);

    mz_stream_os_create(&file_stream);

    err = mz_stream_os_open(file_stream, path, MZ_OPEN_MODE_READ);

    if (err != MZ_OK) {
        mz_stream_os_delete(&file_stream);
        mz_zip_reader_close(handle);
        return err;
    }

    mz_stream_os_seek(file_stream, 0, MZ_SEEK_END);
    file_size = mz_stream_os_tell(file_stream);
    mz_stream_os_seek(file_stream, 0, MZ_SEEK_SET);

    if ((file_size <= 0) || (file_size > UINT32_MAX)) {
        /* Memory size is too large or too small */

        mz_stream_os_close(file_stream);
        mz_stream_os_delete(&file_stream);
        mz_zip_reader_close(handle);
        return MZ_MEM_ERROR;
    }

    mz_stream_mem_create(&reader->mem_stream);
    mz_stream_mem_set_grow_size(reader->mem_stream, (int32_t)file_size);
    mz_stream_mem_open(reader->mem_stream, NULL, MZ_OPEN_MODE_CREATE);

    err = mz_stream_copy(reader->mem_stream, file_stream, (int32_t)file_size);

    mz_stream_os_close(file_stream);
    mz_stream_os_delete(&file_stream);

    if (err == MZ_OK)
        err = mz_zip_reader_open(handle, reader->mem_stream);
    if (err != MZ_OK)
        mz_zip_reader_close(handle);

    return err;
}

int32_t mz_zip_reader_open_buffer(void *handle, uint8_t *buf, int32_t len, uint8_t copy) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;

    mz_zip_reader_close(handle);

    mz_stream_mem_create(&reader->mem_stream);

    if (copy) {
        mz_stream_mem_set_grow_size(reader->mem_stream, len);
        mz_stream_mem_open(reader->mem_stream, NULL, MZ_OPEN_MODE_CREATE);
        mz_stream_mem_write(reader->mem_stream, buf, len);
        mz_stream_mem_seek(reader->mem_stream, 0, MZ_SEEK_SET);
    } else {
        mz_stream_mem_open(reader->mem_stream, NULL, MZ_OPEN_MODE_READ);
        mz_stream_mem_set_buffer(reader->mem_stream, buf, len);
    }

    if (err == MZ_OK)
        err = mz_zip_reader_open(handle, reader->mem_stream);

    return err;
}

int32_t mz_zip_reader_close(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;

    if (reader->zip_handle != NULL) {
        err = mz_zip_close(reader->zip_handle);
        mz_zip_delete(&reader->zip_handle);
    }

    if (reader->split_stream != NULL) {
        mz_stream_split_close(reader->split_stream);
        mz_stream_split_delete(&reader->split_stream);
    }

    if (reader->buffered_stream != NULL)
        mz_stream_buffered_delete(&reader->buffered_stream);

    if (reader->file_stream != NULL)
        mz_stream_os_delete(&reader->file_stream);

    if (reader->mem_stream != NULL) {
        mz_stream_mem_close(reader->mem_stream);
        mz_stream_mem_delete(&reader->mem_stream);
    }

    return err;
}

/***************************************************************************/

int32_t mz_zip_reader_unzip_cd(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    mz_zip_file *cd_info = NULL;
    void *cd_mem_stream = NULL;
    void *new_cd_stream = NULL;
    void *file_extra_stream = NULL;
    uint64_t number_entry = 0;
    int32_t err = MZ_OK;


    err = mz_zip_reader_goto_first_entry(handle);
    if (err != MZ_OK)
        return err;
    err = mz_zip_reader_entry_get_info(handle, &cd_info);
    if (err != MZ_OK)
        return err;

    if (strcmp(cd_info->filename, MZ_ZIP_CD_FILENAME) != 0)
        return mz_zip_reader_goto_first_entry(handle);

    err = mz_zip_reader_entry_open(handle);
    if (err != MZ_OK)
        return err;

    mz_stream_mem_create(&file_extra_stream);
    mz_stream_mem_set_buffer(file_extra_stream, (void *)cd_info->extrafield, cd_info->extrafield_size);

    err = mz_zip_extrafield_find(file_extra_stream, MZ_ZIP_EXTENSION_CDCD, INT32_MAX, NULL);
    if (err == MZ_OK)
        err = mz_stream_read_uint64(file_extra_stream, &number_entry);

    mz_stream_mem_delete(&file_extra_stream);

    if (err != MZ_OK)
        return err;

    mz_zip_get_cd_mem_stream(reader->zip_handle, &cd_mem_stream);
    if (mz_stream_mem_is_open(cd_mem_stream) != MZ_OK)
        mz_stream_mem_open(cd_mem_stream, NULL, MZ_OPEN_MODE_CREATE);

    err = mz_stream_seek(cd_mem_stream, 0, MZ_SEEK_SET);
    if (err == MZ_OK)
        err = mz_stream_copy_stream(cd_mem_stream, NULL, handle, mz_zip_reader_entry_read,
            (int32_t)cd_info->uncompressed_size);

    if (err == MZ_OK) {
        reader->cd_zipped = 1;

        mz_zip_set_cd_stream(reader->zip_handle, 0, cd_mem_stream);
        mz_zip_set_number_entry(reader->zip_handle, number_entry);

        err = mz_zip_reader_goto_first_entry(handle);
    }

    reader->cd_verified = reader->entry_verified;

    mz_stream_mem_delete(&new_cd_stream);
    return err;
}

/***************************************************************************/

static int32_t mz_zip_reader_locate_entry_cb(void *handle, void *userdata, mz_zip_file *file_info) {
    mz_zip_reader *reader = (mz_zip_reader *)userdata;
    int32_t result = 0;
    MZ_UNUSED(handle);
    result = mz_path_compare_wc(file_info->filename, reader->pattern, reader->pattern_ignore_case);
    return result;
}

int32_t mz_zip_reader_goto_first_entry(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;

    if (mz_zip_reader_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    if (mz_zip_entry_is_open(reader->zip_handle) == MZ_OK)
        mz_zip_reader_entry_close(handle);

    if (reader->pattern == NULL)
        err = mz_zip_goto_first_entry(reader->zip_handle);
    else
        err = mz_zip_locate_first_entry(reader->zip_handle, reader, mz_zip_reader_locate_entry_cb);

    reader->file_info = NULL;
    if (err == MZ_OK)
        err = mz_zip_entry_get_info(reader->zip_handle, &reader->file_info);

    return err;
}

int32_t mz_zip_reader_goto_next_entry(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;

    if (mz_zip_reader_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    if (mz_zip_entry_is_open(reader->zip_handle) == MZ_OK)
        mz_zip_reader_entry_close(handle);

    if (reader->pattern == NULL)
        err = mz_zip_goto_next_entry(reader->zip_handle);
    else
        err = mz_zip_locate_next_entry(reader->zip_handle, reader, mz_zip_reader_locate_entry_cb);

    reader->file_info = NULL;
    if (err == MZ_OK)
        err = mz_zip_entry_get_info(reader->zip_handle, &reader->file_info);

    return err;
}

int32_t mz_zip_reader_locate_entry(void *handle, const char *filename, uint8_t ignore_case) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;

    if (mz_zip_entry_is_open(reader->zip_handle) == MZ_OK)
        mz_zip_reader_entry_close(handle);

    err = mz_zip_locate_entry(reader->zip_handle, filename, ignore_case);

    reader->file_info = NULL;
    if (err == MZ_OK)
        err = mz_zip_entry_get_info(reader->zip_handle, &reader->file_info);

    return err;
}

/***************************************************************************/

int32_t mz_zip_reader_entry_open(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;
    const char *password = NULL;
    char password_buf[120];


    reader->entry_verified = 0;

    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (reader->file_info == NULL)
        return MZ_PARAM_ERROR;

    /* If the entry isn't open for reading, open it */
    if (mz_zip_entry_is_open(reader->zip_handle) == MZ_OK)
        return MZ_OK;

    password = reader->password;

    /* Check if we need a password and ask for it if we need to */
    if ((reader->file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (password == NULL) &&
        (reader->password_cb != NULL)) {
        reader->password_cb(handle, reader->password_userdata, reader->file_info,
            password_buf, sizeof(password_buf));

        password = password_buf;
    }

    err = mz_zip_entry_read_open(reader->zip_handle, reader->raw, password);
#ifndef MZ_ZIP_NO_CRYPTO
    if (err != MZ_OK)
        return err;

    if (mz_zip_reader_entry_get_first_hash(handle, &reader->hash_algorithm, &reader->hash_digest_size) == MZ_OK) {
        mz_crypt_sha_create(&reader->hash);
        if (reader->hash_algorithm == MZ_HASH_SHA1)
            mz_crypt_sha_set_algorithm(reader->hash, MZ_HASH_SHA1);
        else if (reader->hash_algorithm == MZ_HASH_SHA256)
            mz_crypt_sha_set_algorithm(reader->hash, MZ_HASH_SHA256);
        else
            err = MZ_SUPPORT_ERROR;

        if (err == MZ_OK)
            mz_crypt_sha_begin(reader->hash);
#ifdef MZ_ZIP_SIGNING
        if (err == MZ_OK) {
            if (mz_zip_reader_entry_has_sign(handle) == MZ_OK) {
                err = mz_zip_reader_entry_sign_verify(handle);
                if (err == MZ_OK)
                    reader->entry_verified = 1;
            } else if (reader->sign_required && !reader->cd_verified)
                err = MZ_SIGN_ERROR;
        }
#endif
    } else if (reader->sign_required && !reader->cd_verified)
        err = MZ_SIGN_ERROR;
#endif

    return err;
}

int32_t mz_zip_reader_entry_close(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;
    int32_t err_close = MZ_OK;
#ifndef MZ_ZIP_NO_CRYPTO
    int32_t err_hash = MZ_OK;
    uint8_t computed_hash[MZ_HASH_MAX_SIZE];
    uint8_t expected_hash[MZ_HASH_MAX_SIZE];

    if (reader->hash != NULL) {
        mz_crypt_sha_end(reader->hash, computed_hash, sizeof(computed_hash));
        mz_crypt_sha_delete(&reader->hash);

        err_hash = mz_zip_reader_entry_get_hash(handle, reader->hash_algorithm, expected_hash,
            reader->hash_digest_size);

        if (err_hash == MZ_OK) {
            /* Verify expected hash against computed hash */
            if (memcmp(computed_hash, expected_hash, reader->hash_digest_size) != 0)
                err = MZ_CRC_ERROR;
        }
    }
#endif

    err_close = mz_zip_entry_close(reader->zip_handle);
    if (err == MZ_OK)
        err = err_close;
    return err;
}

int32_t mz_zip_reader_entry_read(void *handle, void *buf, int32_t len) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t read = 0;
    read = mz_zip_entry_read(reader->zip_handle, buf, len);
#ifndef MZ_ZIP_NO_CRYPTO
    if ((read > 0) && (reader->hash != NULL))
        mz_crypt_sha_update(reader->hash, buf, read);
#endif
    return read;
}

int32_t mz_zip_reader_entry_has_sign(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;

    if (reader == NULL || mz_zip_entry_is_open(reader->zip_handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    return mz_zip_extrafield_contains(reader->file_info->extrafield,
        reader->file_info->extrafield_size, MZ_ZIP_EXTENSION_SIGN, NULL);
}

#if !defined(MZ_ZIP_NO_CRYPTO) && defined(MZ_ZIP_SIGNING)
int32_t mz_zip_reader_entry_sign_verify(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    void *file_extra_stream = NULL;
    int32_t err = MZ_OK;
    uint8_t *signature = NULL;
    uint16_t signature_size = 0;
    uint8_t hash[MZ_HASH_MAX_SIZE];

    if (reader == NULL || mz_zip_entry_is_open(reader->zip_handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    mz_stream_mem_create(&file_extra_stream);
    mz_stream_mem_set_buffer(file_extra_stream, (void *)reader->file_info->extrafield,
        reader->file_info->extrafield_size);

    err = mz_zip_extrafield_find(file_extra_stream, MZ_ZIP_EXTENSION_SIGN, INT32_MAX, &signature_size);
    if ((err == MZ_OK) && (signature_size > 0)) {
        signature = (uint8_t *)MZ_ALLOC(signature_size);
        if (mz_stream_read(file_extra_stream, signature, signature_size) != signature_size)
            err = MZ_READ_ERROR;
    }

    mz_stream_mem_delete(&file_extra_stream);

    if (err == MZ_OK) {
        /* Get most secure hash to verify signature against */
        err = mz_zip_reader_entry_get_hash(handle, reader->hash_algorithm, hash, reader->hash_digest_size);
    }

    if (err == MZ_OK) {
        /* Verify the pkcs signature */
        err = mz_crypt_sign_verify(hash, reader->hash_digest_size, signature, signature_size);
    }

    if (signature != NULL)
        MZ_FREE(signature);

    return err;
}
#endif

int32_t mz_zip_reader_entry_get_hash(void *handle, uint16_t algorithm, uint8_t *digest, int32_t digest_size) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    void *file_extra_stream = NULL;
    int32_t err = MZ_OK;
    int32_t return_err = MZ_EXIST_ERROR;
    uint16_t cur_algorithm = 0;
    uint16_t cur_digest_size = 0;

    mz_stream_mem_create(&file_extra_stream);
    mz_stream_mem_set_buffer(file_extra_stream, (void *)reader->file_info->extrafield,
        reader->file_info->extrafield_size);

    do {
        err = mz_zip_extrafield_find(file_extra_stream, MZ_ZIP_EXTENSION_HASH, INT32_MAX, NULL);
        if (err != MZ_OK)
            break;

        err = mz_stream_read_uint16(file_extra_stream, &cur_algorithm);
        if (err == MZ_OK)
            err = mz_stream_read_uint16(file_extra_stream, &cur_digest_size);
        if ((err == MZ_OK) && (cur_algorithm == algorithm) && (cur_digest_size <= digest_size) &&
            (cur_digest_size <= MZ_HASH_MAX_SIZE)) {
            /* Read hash digest */
            if (mz_stream_read(file_extra_stream, digest, digest_size) == cur_digest_size)
                return_err = MZ_OK;
            break;
        } else {
            err = mz_stream_seek(file_extra_stream, cur_digest_size, MZ_SEEK_CUR);
        }
    } while (err == MZ_OK);

    mz_stream_mem_delete(&file_extra_stream);

    return return_err;
}

int32_t mz_zip_reader_entry_get_first_hash(void *handle, uint16_t *algorithm, uint16_t *digest_size) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    void *file_extra_stream = NULL;
    int32_t err = MZ_OK;
    uint16_t cur_algorithm = 0;
    uint16_t cur_digest_size = 0;

    if (reader == NULL || algorithm == NULL)
        return MZ_PARAM_ERROR;

    mz_stream_mem_create(&file_extra_stream);
    mz_stream_mem_set_buffer(file_extra_stream, (void *)reader->file_info->extrafield,
        reader->file_info->extrafield_size);

    err = mz_zip_extrafield_find(file_extra_stream, MZ_ZIP_EXTENSION_HASH, INT32_MAX, NULL);
    if (err == MZ_OK)
        err = mz_stream_read_uint16(file_extra_stream, &cur_algorithm);
    if (err == MZ_OK)
        err = mz_stream_read_uint16(file_extra_stream, &cur_digest_size);

    if (algorithm != NULL)
        *algorithm = cur_algorithm;
    if (digest_size != NULL)
        *digest_size = cur_digest_size;

    mz_stream_mem_delete(&file_extra_stream);

    return err;
}

int32_t mz_zip_reader_entry_get_info(void *handle, mz_zip_file **file_info) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;
    if (file_info == NULL || mz_zip_reader_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    *file_info = reader->file_info;
    if (*file_info == NULL)
        return MZ_EXIST_ERROR;
    return err;
}

int32_t mz_zip_reader_entry_is_dir(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (mz_zip_reader_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    return mz_zip_entry_is_dir(reader->zip_handle);
}

int32_t mz_zip_reader_entry_save_process(void *handle, void *stream, mz_stream_write_cb write_cb) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;
    int32_t read = 0;
    int32_t written = 0;


    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (reader->file_info == NULL)
        return MZ_PARAM_ERROR;
    if (write_cb == NULL)
        return MZ_PARAM_ERROR;

    /* If the entry isn't open for reading, open it */
    if (mz_zip_entry_is_open(reader->zip_handle) != MZ_OK)
        err = mz_zip_reader_entry_open(handle);

    if (err != MZ_OK)
        return err;

    /* Unzip entry in zip file */
    read = mz_zip_reader_entry_read(handle, reader->buffer, sizeof(reader->buffer));

    if (read == 0) {
        /* If we are done close the entry */
        err = mz_zip_reader_entry_close(handle);
        if (err != MZ_OK)
            return err;

        return MZ_END_OF_STREAM;
    }

    if (read > 0) {
        /* Write the data to the specified stream */
        written = write_cb(stream, reader->buffer, read);
        if (written != read)
            return MZ_WRITE_ERROR;
    }

    return read;
}

int32_t mz_zip_reader_entry_save(void *handle, void *stream, mz_stream_write_cb write_cb) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    uint64_t current_time = 0;
    uint64_t update_time = 0;
    int64_t current_pos = 0;
    int64_t update_pos = 0;
    int32_t err = MZ_OK;
    int32_t written = 0;

    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (reader->file_info == NULL)
        return MZ_PARAM_ERROR;

    /* Update the progress at the beginning */
    if (reader->progress_cb != NULL)
        reader->progress_cb(handle, reader->progress_userdata, reader->file_info, current_pos);

    /* Write data to stream until done */
    while (err == MZ_OK) {
        written = mz_zip_reader_entry_save_process(handle, stream, write_cb);
        if (written == MZ_END_OF_STREAM)
            break;
        if (written > 0)
            current_pos += written;
        if (written < 0)
            err = written;

        /* Update progress if enough time have passed */
        current_time = mz_os_ms_time();
        if ((current_time - update_time) > reader->progress_cb_interval_ms) {
            if (reader->progress_cb != NULL)
                reader->progress_cb(handle, reader->progress_userdata, reader->file_info, current_pos);

            update_pos = current_pos;
            update_time = current_time;
        }
    }

    /* Update the progress at the end */
    if (reader->progress_cb != NULL && update_pos != current_pos)
        reader->progress_cb(handle, reader->progress_userdata, reader->file_info, current_pos);

    return err;
}

int32_t mz_zip_reader_entry_save_file(void *handle, const char *path) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    void *stream = NULL;
    uint32_t target_attrib = 0;
    int32_t err_attrib = 0;
    int32_t err = MZ_OK;
    int32_t err_cb = MZ_OK;
    char pathwfs[512];
    char directory[512];

    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (reader->file_info == NULL || path == NULL)
        return MZ_PARAM_ERROR;

    /* Convert to forward slashes for unix which doesn't like backslashes */
    strncpy(pathwfs, path, sizeof(pathwfs) - 1);
    pathwfs[sizeof(pathwfs) - 1] = 0;
    mz_path_convert_slashes(pathwfs, MZ_PATH_SLASH_UNIX);

    if (reader->entry_cb != NULL)
        reader->entry_cb(handle, reader->entry_userdata, reader->file_info, pathwfs);

    strncpy(directory, pathwfs, sizeof(directory) - 1);
    directory[sizeof(directory) - 1] = 0;
    mz_path_remove_filename(directory);

    /* If it is a directory entry then create a directory instead of writing file */
    if ((mz_zip_entry_is_dir(reader->zip_handle) == MZ_OK) &&
        (mz_zip_entry_is_symlink(reader->zip_handle) != MZ_OK)) {
        err = mz_dir_make(directory);
        return err;
    }

    /* Check if file exists and ask if we want to overwrite */
    if ((mz_os_file_exists(pathwfs) == MZ_OK) && (reader->overwrite_cb != NULL)) {
        err_cb = reader->overwrite_cb(handle, reader->overwrite_userdata, reader->file_info, pathwfs);
        if (err_cb != MZ_OK)
            return err;
        /* We want to overwrite the file so we delete the existing one */
        mz_os_unlink(pathwfs);
    }

    /* If symbolic link then properly construct destination path and link path */
    if (mz_zip_entry_is_symlink(reader->zip_handle) == MZ_OK) {
        mz_path_remove_slash(pathwfs);
        mz_path_remove_filename(directory);
    }

    /* Create the output directory if it doesn't already exist */
    if (mz_os_is_dir(directory) != MZ_OK) {
        err = mz_dir_make(directory);
        if (err != MZ_OK)
            return err;
    }

    /* If it is a symbolic link then create symbolic link instead of writing file */
    if (mz_zip_entry_is_symlink(reader->zip_handle) == MZ_OK) {
        mz_os_make_symlink(pathwfs, reader->file_info->linkname);
        /* Don't check return value because we aren't validating symbolic link target */
        return err;
    }

    /* Create the file on disk so we can save to it */
    mz_stream_os_create(&stream);
    err = mz_stream_os_open(stream, pathwfs, MZ_OPEN_MODE_CREATE);

    if (err == MZ_OK)
        err = mz_zip_reader_entry_save(handle, stream, mz_stream_write);

    mz_stream_close(stream);
    mz_stream_delete(&stream);

    if (err == MZ_OK) {
        /* Set the time of the file that has been created */
        mz_os_set_file_date(pathwfs, reader->file_info->modified_date,
            reader->file_info->accessed_date, reader->file_info->creation_date);
    }

    if (err == MZ_OK) {
        /* Set file attributes for the correct system */
        err_attrib = mz_zip_attrib_convert(MZ_HOST_SYSTEM(reader->file_info->version_madeby),
            reader->file_info->external_fa, MZ_VERSION_MADEBY_HOST_SYSTEM, &target_attrib);

        if (err_attrib == MZ_OK)
            mz_os_set_file_attribs(pathwfs, target_attrib);
    }

    return err;
}

int32_t mz_zip_reader_entry_save_buffer(void *handle, void *buf, int32_t len) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    void *mem_stream = NULL;
    int32_t err = MZ_OK;

    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (reader->file_info == NULL)
        return MZ_PARAM_ERROR;
    if (reader->file_info->uncompressed_size > INT32_MAX)
        return MZ_PARAM_ERROR;
    if (len != (int32_t)reader->file_info->uncompressed_size)
        return MZ_BUF_ERROR;

    /* Create a memory stream backed by our buffer and save to it */
    mz_stream_mem_create(&mem_stream);
    mz_stream_mem_set_buffer(mem_stream, buf, len);

    err = mz_stream_mem_open(mem_stream, NULL, MZ_OPEN_MODE_READ);
    if (err == MZ_OK)
        err = mz_zip_reader_entry_save(handle, mem_stream, mz_stream_mem_write);

    mz_stream_mem_delete(&mem_stream);
    return err;
}

int32_t mz_zip_reader_entry_save_buffer_length(void *handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;

    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (reader->file_info == NULL)
        return MZ_PARAM_ERROR;
    if (reader->file_info->uncompressed_size > INT32_MAX)
        return MZ_PARAM_ERROR;

    /* Get the maximum size required for the save buffer */
    return (int32_t)reader->file_info->uncompressed_size;
}

/***************************************************************************/

int32_t mz_zip_reader_save_all(void *handle, const char *destination_dir) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    int32_t err = MZ_OK;
    uint8_t *utf8_string = NULL;
    char path[512];
    char utf8_name[256];
    char resolved_name[256];

    err = mz_zip_reader_goto_first_entry(handle);

    if (err == MZ_END_OF_LIST)
        return err;

    while (err == MZ_OK) {
        /* Construct output path */
        path[0] = 0;

        strncpy(utf8_name, reader->file_info->filename, sizeof(utf8_name) - 1);
        utf8_name[sizeof(utf8_name) - 1] = 0;

        if ((reader->encoding > 0) && (reader->file_info->flag & MZ_ZIP_FLAG_UTF8) == 0) {
            utf8_string = mz_os_utf8_string_create(reader->file_info->filename, reader->encoding);
            if (utf8_string) {
                strncpy(utf8_name, (char *)utf8_string, sizeof(utf8_name) - 1);
                utf8_name[sizeof(utf8_name) - 1] = 0;
                mz_os_utf8_string_delete(&utf8_string);
            }
        }

        err = mz_path_resolve(utf8_name, resolved_name, sizeof(resolved_name));
        if (err != MZ_OK)
            break;

        if (destination_dir != NULL)
            mz_path_combine(path, destination_dir, sizeof(path));

        mz_path_combine(path, resolved_name, sizeof(path));

        /* Save file to disk */
        err = mz_zip_reader_entry_save_file(handle, path);

        if (err == MZ_OK)
            err = mz_zip_reader_goto_next_entry(handle);
    }

    if (err == MZ_END_OF_LIST)
        return MZ_OK;

    return err;
}

/***************************************************************************/

void mz_zip_reader_set_pattern(void *handle, const char *pattern, uint8_t ignore_case) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->pattern = pattern;
    reader->pattern_ignore_case = ignore_case;
}

void mz_zip_reader_set_password(void *handle, const char *password) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->password = password;
}

void mz_zip_reader_set_raw(void *handle, uint8_t raw) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->raw = raw;
}

int32_t mz_zip_reader_get_raw(void *handle, uint8_t *raw) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (raw == NULL)
        return MZ_PARAM_ERROR;
    *raw = reader->raw;
    return MZ_OK;
}

int32_t mz_zip_reader_get_zip_cd(void *handle, uint8_t *zip_cd) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (zip_cd == NULL)
        return MZ_PARAM_ERROR;
    *zip_cd = reader->cd_zipped;
    return MZ_OK;
}

int32_t mz_zip_reader_get_comment(void *handle, const char **comment) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (comment == NULL)
        return MZ_PARAM_ERROR;
    return mz_zip_get_comment(reader->zip_handle, comment);
}

int32_t mz_zip_reader_set_recover(void *handle, uint8_t recover) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (reader == NULL)
        return MZ_PARAM_ERROR;
    reader->recover = recover;
    return MZ_OK;
}

void mz_zip_reader_set_encoding(void *handle, int32_t encoding) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->encoding = encoding;
}

void mz_zip_reader_set_sign_required(void *handle, uint8_t sign_required) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->sign_required = sign_required;
}

void mz_zip_reader_set_overwrite_cb(void *handle, void *userdata, mz_zip_reader_overwrite_cb cb) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->overwrite_cb = cb;
    reader->overwrite_userdata = userdata;
}

void mz_zip_reader_set_password_cb(void *handle, void *userdata, mz_zip_reader_password_cb cb) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->password_cb = cb;
    reader->password_userdata = userdata;
}

void mz_zip_reader_set_progress_cb(void *handle, void *userdata, mz_zip_reader_progress_cb cb) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->progress_cb = cb;
    reader->progress_userdata = userdata;
}

void mz_zip_reader_set_progress_interval(void *handle, uint32_t milliseconds) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->progress_cb_interval_ms = milliseconds;
}

void mz_zip_reader_set_entry_cb(void *handle, void *userdata, mz_zip_reader_entry_cb cb) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    reader->entry_cb = cb;
    reader->entry_userdata = userdata;
}

int32_t mz_zip_reader_get_zip_handle(void *handle, void **zip_handle) {
    mz_zip_reader *reader = (mz_zip_reader *)handle;
    if (zip_handle == NULL)
        return MZ_PARAM_ERROR;
    *zip_handle = reader->zip_handle;
    if (*zip_handle == NULL)
        return MZ_EXIST_ERROR;
    return MZ_OK;
}

/***************************************************************************/

void *mz_zip_reader_create(void **handle) {
    mz_zip_reader *reader = NULL;

    reader = (mz_zip_reader *)MZ_ALLOC(sizeof(mz_zip_reader));
    if (reader != NULL) {
        memset(reader, 0, sizeof(mz_zip_reader));
        reader->recover = 1;
        reader->progress_cb_interval_ms = MZ_DEFAULT_PROGRESS_INTERVAL;
    }
    if (handle != NULL)
        *handle = reader;

    return reader;
}

void mz_zip_reader_delete(void **handle) {
    mz_zip_reader *reader = NULL;
    if (handle == NULL)
        return;
    reader = (mz_zip_reader *)*handle;
    if (reader != NULL) {
        mz_zip_reader_close(reader);
        MZ_FREE(reader);
    }
    *handle = NULL;
}

/***************************************************************************/

typedef struct mz_zip_writer_s {
    void        *zip_handle;
    void        *file_stream;
    void        *buffered_stream;
    void        *split_stream;
    void        *sha256;
    void        *mem_stream;
    void        *file_extra_stream;
    mz_zip_file file_info;
    void        *overwrite_userdata;
    mz_zip_writer_overwrite_cb
                overwrite_cb;
    void        *password_userdata;
    mz_zip_writer_password_cb
                password_cb;
    void        *progress_userdata;
    mz_zip_writer_progress_cb
                progress_cb;
    uint32_t    progress_cb_interval_ms;
    void        *entry_userdata;
    mz_zip_writer_entry_cb
                entry_cb;
    const char  *password;
    const char  *comment;
    uint8_t     *cert_data;
    int32_t     cert_data_size;
    const char  *cert_pwd;
    uint16_t    compress_method;
    int16_t     compress_level;
    uint8_t     follow_links;
    uint8_t     store_links;
    uint8_t     zip_cd;
    uint8_t     aes;
    uint8_t     raw;
    uint8_t     buffer[UINT16_MAX];
} mz_zip_writer;

/***************************************************************************/

int32_t mz_zip_writer_zip_cd(void *handle) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    mz_zip_file cd_file;
    uint64_t number_entry = 0;
    int64_t cd_mem_length = 0;
    int32_t err = MZ_OK;
    int32_t extrafield_size = 0;
    void *file_extra_stream = NULL;
    void *cd_mem_stream = NULL;


    memset(&cd_file, 0, sizeof(cd_file));

    mz_zip_get_number_entry(writer->zip_handle, &number_entry);
    mz_zip_get_cd_mem_stream(writer->zip_handle, &cd_mem_stream);
    mz_stream_seek(cd_mem_stream, 0, MZ_SEEK_END);
    cd_mem_length = (uint32_t)mz_stream_tell(cd_mem_stream);
    mz_stream_seek(cd_mem_stream, 0, MZ_SEEK_SET);

    cd_file.filename = MZ_ZIP_CD_FILENAME;
    cd_file.modified_date = time(NULL);
    cd_file.version_madeby = MZ_VERSION_MADEBY;
    cd_file.compression_method = writer->compress_method;
    cd_file.uncompressed_size = (int32_t)cd_mem_length;
    cd_file.flag = MZ_ZIP_FLAG_UTF8;

    if (writer->password != NULL)
        cd_file.flag |= MZ_ZIP_FLAG_ENCRYPTED;

    mz_stream_mem_create(&file_extra_stream);
    mz_stream_mem_open(file_extra_stream, NULL, MZ_OPEN_MODE_CREATE);

    mz_zip_extrafield_write(file_extra_stream, MZ_ZIP_EXTENSION_CDCD, 8);

    mz_stream_write_uint64(file_extra_stream, number_entry);

    mz_stream_mem_get_buffer(file_extra_stream, (const void **)&cd_file.extrafield);
    mz_stream_mem_get_buffer_length(file_extra_stream, &extrafield_size);
    cd_file.extrafield_size = (uint16_t)extrafield_size;

    err = mz_zip_writer_entry_open(handle, &cd_file);
    if (err == MZ_OK) {
        mz_stream_copy_stream(handle, mz_zip_writer_entry_write, cd_mem_stream,
            NULL, (int32_t)cd_mem_length);

        mz_stream_seek(cd_mem_stream, 0, MZ_SEEK_SET);
        mz_stream_mem_set_buffer_limit(cd_mem_stream, 0);

        err = mz_zip_writer_entry_close(writer);
    }

    mz_stream_mem_delete(&file_extra_stream);

    return err;
}

/***************************************************************************/

int32_t mz_zip_writer_is_open(void *handle) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    if (writer == NULL)
        return MZ_PARAM_ERROR;
    if (writer->zip_handle == NULL)
        return MZ_PARAM_ERROR;
    return MZ_OK;
}

static int32_t mz_zip_writer_open_int(void *handle, void *stream, int32_t mode) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t err = MZ_OK;

    mz_zip_create(&writer->zip_handle);
    err = mz_zip_open(writer->zip_handle, stream, mode);

    if (err != MZ_OK) {
        mz_zip_writer_close(handle);
        return err;
    }

    return MZ_OK;
}

int32_t mz_zip_writer_open(void *handle, void *stream, uint8_t append) {
    int32_t mode = MZ_OPEN_MODE_WRITE;

    if (append) {
        mode |= MZ_OPEN_MODE_APPEND;
    } else {
        mode |= MZ_OPEN_MODE_CREATE;
    }

    return mz_zip_writer_open_int(handle, stream, mode);
}

int32_t mz_zip_writer_open_file(void *handle, const char *path, int64_t disk_size, uint8_t append) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t mode = MZ_OPEN_MODE_READWRITE;
    int32_t err = MZ_OK;
    int32_t err_cb = 0;
    char directory[320];

    mz_zip_writer_close(handle);

    if (mz_os_file_exists(path) != MZ_OK) {
        /* If the file doesn't exist, we don't append file */
        mode |= MZ_OPEN_MODE_CREATE;

        /* Create destination directory if it doesn't already exist */
        if (strchr(path, '/') != NULL || strrchr(path, '\\') != NULL) {
            strncpy(directory, path, sizeof(directory));
            mz_path_remove_filename(directory);
            if (mz_os_file_exists(directory) != MZ_OK)
                mz_dir_make(directory);
        }
    } else if (append) {
        mode |= MZ_OPEN_MODE_APPEND;
    } else {
        if (writer->overwrite_cb != NULL)
            err_cb = writer->overwrite_cb(handle, writer->overwrite_userdata, path);

        if (err_cb == MZ_INTERNAL_ERROR)
            return err;

        if (err_cb == MZ_OK)
            mode |= MZ_OPEN_MODE_CREATE;
        else
            mode |= MZ_OPEN_MODE_APPEND;
    }

    mz_stream_os_create(&writer->file_stream);
    mz_stream_buffered_create(&writer->buffered_stream);
    mz_stream_split_create(&writer->split_stream);

    mz_stream_set_base(writer->buffered_stream, writer->file_stream);
    mz_stream_set_base(writer->split_stream, writer->buffered_stream);

    mz_stream_split_set_prop_int64(writer->split_stream, MZ_STREAM_PROP_DISK_SIZE, disk_size);

    err = mz_stream_open(writer->split_stream, path, mode);
    if (err == MZ_OK)
        err = mz_zip_writer_open_int(handle, writer->split_stream, mode);

    return err;
}

int32_t mz_zip_writer_open_file_in_memory(void *handle, const char *path) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    void *file_stream = NULL;
    int64_t file_size = 0;
    int32_t err = 0;


    mz_zip_writer_close(handle);

    mz_stream_os_create(&file_stream);

    err = mz_stream_os_open(file_stream, path, MZ_OPEN_MODE_READ);

    if (err != MZ_OK) {
        mz_stream_os_delete(&file_stream);
        mz_zip_writer_close(handle);
        return err;
    }

    mz_stream_os_seek(file_stream, 0, MZ_SEEK_END);
    file_size = mz_stream_os_tell(file_stream);
    mz_stream_os_seek(file_stream, 0, MZ_SEEK_SET);

    if ((file_size <= 0) || (file_size > UINT32_MAX)) {
        /* Memory size is too large or too small */

        mz_stream_os_close(file_stream);
        mz_stream_os_delete(&file_stream);
        mz_zip_writer_close(handle);
        return MZ_MEM_ERROR;
    }

    mz_stream_mem_create(&writer->mem_stream);
    mz_stream_mem_set_grow_size(writer->mem_stream, (int32_t)file_size);
    mz_stream_mem_open(writer->mem_stream, NULL, MZ_OPEN_MODE_CREATE);

    err = mz_stream_copy(writer->mem_stream, file_stream, (int32_t)file_size);

    mz_stream_os_close(file_stream);
    mz_stream_os_delete(&file_stream);

    if (err == MZ_OK)
        err = mz_zip_writer_open(handle, writer->mem_stream, 1);
    if (err != MZ_OK)
        mz_zip_writer_close(handle);

    return err;
}

int32_t mz_zip_writer_close(void *handle) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t err = MZ_OK;


    if (writer->zip_handle != NULL) {
        mz_zip_set_version_madeby(writer->zip_handle, MZ_VERSION_MADEBY);
        if (writer->comment)
            mz_zip_set_comment(writer->zip_handle, writer->comment);
        if (writer->zip_cd)
            mz_zip_writer_zip_cd(writer);

        err = mz_zip_close(writer->zip_handle);
        mz_zip_delete(&writer->zip_handle);
    }

    if (writer->split_stream != NULL) {
        mz_stream_split_close(writer->split_stream);
        mz_stream_split_delete(&writer->split_stream);
    }

    if (writer->buffered_stream != NULL)
        mz_stream_buffered_delete(&writer->buffered_stream);

    if (writer->file_stream != NULL)
        mz_stream_os_delete(&writer->file_stream);

    if (writer->mem_stream != NULL) {
        mz_stream_mem_close(writer->mem_stream);
        mz_stream_mem_delete(&writer->mem_stream);
    }

    return err;
}

/***************************************************************************/

int32_t mz_zip_writer_entry_open(void *handle, mz_zip_file *file_info) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t err = MZ_OK;
    const char *password = NULL;
    char password_buf[120];

    /* Copy file info to access data upon close */
    memcpy(&writer->file_info, file_info, sizeof(mz_zip_file));

    if (writer->entry_cb != NULL)
        writer->entry_cb(handle, writer->entry_userdata, &writer->file_info);

    password = writer->password;

    /* Check if we need a password and ask for it if we need to */
    if ((writer->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED) && (password == NULL) &&
        (writer->password_cb != NULL)) {
        writer->password_cb(handle, writer->password_userdata, &writer->file_info,
            password_buf, sizeof(password_buf));
        password = password_buf;
    }

#ifndef MZ_ZIP_NO_CRYPTO
    if (mz_zip_attrib_is_dir(writer->file_info.external_fa, writer->file_info.version_madeby) != MZ_OK) {
        /* Start calculating sha256 */
        mz_crypt_sha_create(&writer->sha256);
        mz_crypt_sha_set_algorithm(writer->sha256, MZ_HASH_SHA256);
        mz_crypt_sha_begin(writer->sha256);
    }
#endif

    /* Open entry in zip */
    err = mz_zip_entry_write_open(writer->zip_handle, &writer->file_info, writer->compress_level,
        writer->raw, password);

    return err;
}

#if !defined(MZ_ZIP_NO_CRYPTO) && defined(MZ_ZIP_SIGNING)
int32_t mz_zip_writer_entry_sign(void *handle, uint8_t *message, int32_t message_size,
    uint8_t *cert_data, int32_t cert_data_size, const char *cert_pwd) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t err = MZ_OK;
    int32_t signature_size = 0;
    uint8_t *signature = NULL;


    if (writer == NULL || cert_data == NULL || cert_data_size <= 0)
        return MZ_PARAM_ERROR;
    if (mz_zip_entry_is_open(writer->zip_handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    /* Sign message with certificate */
    err = mz_crypt_sign(message, message_size, cert_data, cert_data_size, cert_pwd,
        &signature, &signature_size);

    if ((err == MZ_OK) && (signature != NULL)) {
        /* Write signature zip extra field */
        err = mz_zip_extrafield_write(writer->file_extra_stream, MZ_ZIP_EXTENSION_SIGN,
            (uint16_t)signature_size);

        if (err == MZ_OK) {
            if (mz_stream_write(writer->file_extra_stream, signature, signature_size) != signature_size)
                err = MZ_WRITE_ERROR;
        }

        MZ_FREE(signature);
    }

    return err;
}
#endif

int32_t mz_zip_writer_entry_close(void *handle) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t err = MZ_OK;
#ifndef MZ_ZIP_NO_CRYPTO
    const uint8_t *extrafield = NULL;
    int32_t extrafield_size = 0;
    int16_t field_length_hash = 0;
    uint8_t sha256[MZ_HASH_SHA256_SIZE];


    if (writer->sha256 != NULL) {
        mz_crypt_sha_end(writer->sha256, sha256, sizeof(sha256));
        mz_crypt_sha_delete(&writer->sha256);

        /* Copy extrafield so we can append our own fields before close */
        mz_stream_mem_create(&writer->file_extra_stream);
        mz_stream_mem_open(writer->file_extra_stream, NULL, MZ_OPEN_MODE_CREATE);

        /* Write sha256 hash to extrafield */
        field_length_hash = 4 + MZ_HASH_SHA256_SIZE;
        err = mz_zip_extrafield_write(writer->file_extra_stream, MZ_ZIP_EXTENSION_HASH, field_length_hash);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(writer->file_extra_stream, MZ_HASH_SHA256);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(writer->file_extra_stream, MZ_HASH_SHA256_SIZE);
        if (err == MZ_OK) {
            if (mz_stream_write(writer->file_extra_stream, sha256, sizeof(sha256)) != MZ_HASH_SHA256_SIZE)
                err = MZ_WRITE_ERROR;
        }

#ifdef MZ_ZIP_SIGNING
        if ((err == MZ_OK) && (writer->cert_data != NULL) && (writer->cert_data_size > 0)) {
            /* Sign entry if not zipping cd or if it is cd being zipped */
            if (!writer->zip_cd || strcmp(writer->file_info.filename, MZ_ZIP_CD_FILENAME) == 0) {
                err = mz_zip_writer_entry_sign(handle, sha256, sizeof(sha256),
                    writer->cert_data, writer->cert_data_size, writer->cert_pwd);
            }
        }
#endif

        if ((writer->file_info.extrafield != NULL) && (writer->file_info.extrafield_size > 0))
            mz_stream_mem_write(writer->file_extra_stream, writer->file_info.extrafield,
                writer->file_info.extrafield_size);

        /* Update extra field for central directory after adding extra fields */
        mz_stream_mem_get_buffer(writer->file_extra_stream, (const void **)&extrafield);
        mz_stream_mem_get_buffer_length(writer->file_extra_stream, &extrafield_size);

        mz_zip_entry_set_extrafield(writer->zip_handle, extrafield, (uint16_t)extrafield_size);
    }
#endif

    if (err == MZ_OK) {
        if (writer->raw)
            err = mz_zip_entry_close_raw(writer->zip_handle, writer->file_info.uncompressed_size,
                writer->file_info.crc);
        else
            err = mz_zip_entry_close(writer->zip_handle);
    }

    if (writer->file_extra_stream != NULL)
        mz_stream_mem_delete(&writer->file_extra_stream);

    return err;
}

int32_t mz_zip_writer_entry_write(void *handle, const void *buf, int32_t len) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t written = 0;
    written = mz_zip_entry_write(writer->zip_handle, buf, len);
#ifndef MZ_ZIP_NO_CRYPTO
    if ((written > 0) && (writer->sha256 != NULL))
        mz_crypt_sha_update(writer->sha256, buf, written);
#endif
    return written;
}
/***************************************************************************/

int32_t mz_zip_writer_add_process(void *handle, void *stream, mz_stream_read_cb read_cb) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t read = 0;
    int32_t written = 0;
    int32_t err = MZ_OK;

    if (mz_zip_writer_is_open(writer) != MZ_OK)
        return MZ_PARAM_ERROR;
    /* If the entry isn't open for writing, open it */
    if (mz_zip_entry_is_open(writer->zip_handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (read_cb == NULL)
        return MZ_PARAM_ERROR;

    read = read_cb(stream, writer->buffer, sizeof(writer->buffer));
    if (read == 0)
        return MZ_END_OF_STREAM;
    if (read < 0) {
        err = read;
        return err;
    }

    written = mz_zip_writer_entry_write(handle, writer->buffer, read);
    if (written != read)
        return MZ_WRITE_ERROR;

    return written;
}

int32_t mz_zip_writer_add(void *handle, void *stream, mz_stream_read_cb read_cb) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    uint64_t current_time = 0;
    uint64_t update_time = 0;
    int64_t current_pos = 0;
    int64_t update_pos = 0;
    int32_t err = MZ_OK;
    int32_t written = 0;

    /* Update the progress at the beginning */
    if (writer->progress_cb != NULL)
        writer->progress_cb(handle, writer->progress_userdata, &writer->file_info, current_pos);

    /* Write data to stream until done */
    while (err == MZ_OK) {
        written = mz_zip_writer_add_process(handle, stream, read_cb);
        if (written == MZ_END_OF_STREAM)
            break;
        if (written > 0)
            current_pos += written;
        if (written < 0)
            err = written;

        /* Update progress if enough time have passed */
        current_time = mz_os_ms_time();
        if ((current_time - update_time) > writer->progress_cb_interval_ms) {
            if (writer->progress_cb != NULL)
                writer->progress_cb(handle, writer->progress_userdata, &writer->file_info, current_pos);

            update_pos = current_pos;
            update_time = current_time;
        }
    }

    /* Update the progress at the end */
    if (writer->progress_cb != NULL && update_pos != current_pos)
        writer->progress_cb(handle, writer->progress_userdata, &writer->file_info, current_pos);

    return err;
}

int32_t mz_zip_writer_add_info(void *handle, void *stream, mz_stream_read_cb read_cb, mz_zip_file *file_info) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    int32_t err = MZ_OK;


    if (mz_zip_writer_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (file_info == NULL)
        return MZ_PARAM_ERROR;

    /* Add to zip */
    err = mz_zip_writer_entry_open(handle, file_info);
    if (err != MZ_OK)
        return err;

    if (stream != NULL) {
        if (mz_zip_attrib_is_dir(writer->file_info.external_fa, writer->file_info.version_madeby) != MZ_OK) {
            err = mz_zip_writer_add(handle, stream, read_cb);
            if (err != MZ_OK)
                return err;
        }
    }

    err = mz_zip_writer_entry_close(handle);

    return err;
}

int32_t mz_zip_writer_add_buffer(void *handle, void *buf, int32_t len, mz_zip_file *file_info) {
    void *mem_stream = NULL;
    int32_t err = MZ_OK;

    if (mz_zip_writer_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (buf == NULL)
        return MZ_PARAM_ERROR;

    /* Create a memory stream backed by our buffer and add from it */
    mz_stream_mem_create(&mem_stream);
    mz_stream_mem_set_buffer(mem_stream, buf, len);

    err = mz_stream_mem_open(mem_stream, NULL, MZ_OPEN_MODE_READ);
    if (err == MZ_OK)
        err = mz_zip_writer_add_info(handle, mem_stream, mz_stream_mem_read, file_info);

    mz_stream_mem_delete(&mem_stream);
    return err;
}

int32_t mz_zip_writer_add_file(void *handle, const char *path, const char *filename_in_zip) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    mz_zip_file file_info;
    uint32_t target_attrib = 0;
    uint32_t src_attrib = 0;
    int32_t err = MZ_OK;
    uint8_t src_sys = 0;
    void *stream = NULL;
    char link_path[1024];
    const char *filename = filename_in_zip;


    if (mz_zip_writer_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (path == NULL)
        return MZ_PARAM_ERROR;

    if (filename == NULL) {
        err = mz_path_get_filename(path, &filename);
        if (err != MZ_OK)
            return err;
    }

    memset(&file_info, 0, sizeof(file_info));

    /* The path name saved, should not include a leading slash. */
    /* If it did, windows/xp and dynazip couldn't read the zip file. */

    while (filename[0] == '\\' || filename[0] == '/')
        filename += 1;

    /* Get information about the file on disk so we can store it in zip */

    file_info.version_madeby = MZ_VERSION_MADEBY;
    file_info.compression_method = writer->compress_method;
    file_info.filename = filename;
    file_info.uncompressed_size = mz_os_get_file_size(path);
    file_info.flag = MZ_ZIP_FLAG_UTF8;

    if (writer->zip_cd)
        file_info.flag |= MZ_ZIP_FLAG_MASK_LOCAL_INFO;
    if (writer->aes)
        file_info.aes_version = MZ_AES_VERSION;

    mz_os_get_file_date(path, &file_info.modified_date, &file_info.accessed_date,
        &file_info.creation_date);
    mz_os_get_file_attribs(path, &src_attrib);

    src_sys = MZ_HOST_SYSTEM(file_info.version_madeby);

    if ((src_sys != MZ_HOST_SYSTEM_MSDOS) && (src_sys != MZ_HOST_SYSTEM_WINDOWS_NTFS)) {
        /* High bytes are OS specific attributes, low byte is always DOS attributes */
        if (mz_zip_attrib_convert(src_sys, src_attrib, MZ_HOST_SYSTEM_MSDOS, &target_attrib) == MZ_OK)
            file_info.external_fa = target_attrib;
        file_info.external_fa |= (src_attrib << 16);
    } else {
        file_info.external_fa = src_attrib;
    }

    if (writer->store_links && mz_os_is_symlink(path) == MZ_OK) {
        err = mz_os_read_symlink(path, link_path, sizeof(link_path));
        if (err == MZ_OK)
            file_info.linkname = link_path;
    } else if (mz_os_is_dir(path) != MZ_OK) {
        mz_stream_os_create(&stream);
        err = mz_stream_os_open(stream, path, MZ_OPEN_MODE_READ);
    }

    if (err == MZ_OK)
        err = mz_zip_writer_add_info(handle, stream, mz_stream_read, &file_info);

    if (stream != NULL) {
        mz_stream_close(stream);
        mz_stream_delete(&stream);
    }

    return err;
}

int32_t mz_zip_writer_add_path(void *handle, const char *path, const char *root_path,
    uint8_t include_path, uint8_t recursive) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    int32_t err = MZ_OK;
    int16_t is_dir = 0;
    const char *filename = NULL;
    const char *filenameinzip = path;
    char *wildcard_ptr = NULL;
    char full_path[1024];
    char path_dir[1024];


    if (strrchr(path, '*') != NULL) {
        strncpy(path_dir, path, sizeof(path_dir) - 1);
        path_dir[sizeof(path_dir) - 1] = 0;
        mz_path_remove_filename(path_dir);
        wildcard_ptr = path_dir + strlen(path_dir) + 1;
        root_path = path = path_dir;
    } else {
        if (mz_os_is_dir(path) == MZ_OK)
            is_dir = 1;

        /* Construct the filename that our file will be stored in the zip as */
        if (root_path == NULL)
            root_path = path;

        /* Should the file be stored with any path info at all? */
        if (!include_path) {
            if (!is_dir && root_path == path) {
                if (mz_path_get_filename(filenameinzip, &filename) == MZ_OK)
                    filenameinzip = filename;
            } else {
                filenameinzip += strlen(root_path);
            }
        }

        if (!writer->store_links && !writer->follow_links) {
            if (mz_os_is_symlink(path) == MZ_OK)
                return err;
        }

        if (*filenameinzip != 0)
            err = mz_zip_writer_add_file(handle, path, filenameinzip);

        if (!is_dir)
            return err;

        if (writer->store_links) {
            if (mz_os_is_symlink(path) == MZ_OK)
                return err;
        }
    }

    dir = mz_os_open_dir(path);

    if (dir == NULL)
        return MZ_EXIST_ERROR;

    while ((entry = mz_os_read_dir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        full_path[0] = 0;
        mz_path_combine(full_path, path, sizeof(full_path));
        mz_path_combine(full_path, entry->d_name, sizeof(full_path));

        if (!recursive && mz_os_is_dir(full_path) == MZ_OK)
            continue;

        if ((wildcard_ptr != NULL) && (mz_path_compare_wc(entry->d_name, wildcard_ptr, 1) != MZ_OK))
            continue;

        err = mz_zip_writer_add_path(handle, full_path, root_path, include_path, recursive);
        if (err != MZ_OK)
            break;
    }

    mz_os_close_dir(dir);
    return err;
}

int32_t mz_zip_writer_copy_from_reader(void *handle, void *reader) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    mz_zip_file *file_info = NULL;
    int64_t compressed_size = 0;
    int64_t uncompressed_size = 0;
    uint32_t crc32 = 0;
    int32_t err = MZ_OK;
    uint8_t original_raw = 0;
    void *reader_zip_handle = NULL;
    void *writer_zip_handle = NULL;


    if (mz_zip_reader_is_open(reader) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (mz_zip_writer_is_open(writer) != MZ_OK)
        return MZ_PARAM_ERROR;

    err = mz_zip_reader_entry_get_info(reader, &file_info);

    if (err != MZ_OK)
        return err;

    mz_zip_reader_get_zip_handle(reader, &reader_zip_handle);
    mz_zip_writer_get_zip_handle(writer, &writer_zip_handle);

    /* Open entry for raw reading */
    err = mz_zip_entry_read_open(reader_zip_handle, 1, NULL);

    if (err == MZ_OK) {
        /* Write entry raw, save original raw value */
        original_raw = writer->raw;
        writer->raw = 1;

        err = mz_zip_writer_entry_open(writer, file_info);

        if ((err == MZ_OK) &&
            (mz_zip_attrib_is_dir(writer->file_info.external_fa, writer->file_info.version_madeby) != MZ_OK)) {
            err = mz_zip_writer_add(writer, reader_zip_handle, mz_zip_entry_read);
        }

        if (err == MZ_OK) {
            err = mz_zip_entry_read_close(reader_zip_handle, &crc32, &compressed_size, &uncompressed_size);
            if (err == MZ_OK)
                err = mz_zip_entry_write_close(writer_zip_handle, crc32, compressed_size, uncompressed_size);
        }

        if (mz_zip_entry_is_open(reader_zip_handle) == MZ_OK)
            mz_zip_entry_close(reader_zip_handle);

        if (mz_zip_entry_is_open(writer_zip_handle) == MZ_OK)
            mz_zip_entry_close(writer_zip_handle);

        writer->raw = original_raw;
    }

    return err;
}

/***************************************************************************/

void mz_zip_writer_set_password(void *handle, const char *password) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->password = password;
}

void mz_zip_writer_set_comment(void *handle, const char *comment) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->comment = comment;
}

void mz_zip_writer_set_raw(void *handle, uint8_t raw) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->raw = raw;
}

int32_t mz_zip_writer_get_raw(void *handle, uint8_t *raw) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    if (raw == NULL)
        return MZ_PARAM_ERROR;
    *raw = writer->raw;
    return MZ_OK;
}

void mz_zip_writer_set_aes(void *handle, uint8_t aes) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->aes = aes;
}

void mz_zip_writer_set_compress_method(void *handle, uint16_t compress_method) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->compress_method = compress_method;
}

void mz_zip_writer_set_compress_level(void *handle, int16_t compress_level) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->compress_level = compress_level;
}

void mz_zip_writer_set_follow_links(void *handle, uint8_t follow_links) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->follow_links = follow_links;
}

void mz_zip_writer_set_store_links(void *handle, uint8_t store_links) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->store_links = store_links;
}

void mz_zip_writer_set_zip_cd(void *handle, uint8_t zip_cd) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->zip_cd = zip_cd;
}

int32_t mz_zip_writer_set_certificate(void *handle, const char *cert_path, const char *cert_pwd) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    void *cert_stream = NULL;
    uint8_t *cert_data = NULL;
    int32_t cert_data_size = 0;
    int32_t err = MZ_OK;

    if (cert_path == NULL)
        return MZ_PARAM_ERROR;

    cert_data_size = (int32_t)mz_os_get_file_size(cert_path);

    if (cert_data_size == 0)
        return MZ_PARAM_ERROR;

    if (writer->cert_data != NULL) {
        MZ_FREE(writer->cert_data);
        writer->cert_data = NULL;
    }

    cert_data = (uint8_t *)MZ_ALLOC(cert_data_size);

    /* Read pkcs12 certificate from disk */
    mz_stream_os_create(&cert_stream);
    err = mz_stream_os_open(cert_stream, cert_path, MZ_OPEN_MODE_READ);
    if (err == MZ_OK) {
        if (mz_stream_os_read(cert_stream, cert_data, cert_data_size) != cert_data_size)
            err = MZ_READ_ERROR;
        mz_stream_os_close(cert_stream);
    }
    mz_stream_os_delete(&cert_stream);

    if (err == MZ_OK) {
        writer->cert_data = cert_data;
        writer->cert_data_size = cert_data_size;
        writer->cert_pwd = cert_pwd;
    } else {
        MZ_FREE(cert_data);
    }

    return err;
}

void mz_zip_writer_set_overwrite_cb(void *handle, void *userdata, mz_zip_writer_overwrite_cb cb) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->overwrite_cb = cb;
    writer->overwrite_userdata = userdata;
}

void mz_zip_writer_set_password_cb(void *handle, void *userdata, mz_zip_writer_password_cb cb) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->password_cb = cb;
    writer->password_userdata = userdata;
}

void mz_zip_writer_set_progress_cb(void *handle, void *userdata, mz_zip_writer_progress_cb cb) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->progress_cb = cb;
    writer->progress_userdata = userdata;
}

void mz_zip_writer_set_progress_interval(void *handle, uint32_t milliseconds) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->progress_cb_interval_ms = milliseconds;
}

void mz_zip_writer_set_entry_cb(void *handle, void *userdata, mz_zip_writer_entry_cb cb) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    writer->entry_cb = cb;
    writer->entry_userdata = userdata;
}

int32_t mz_zip_writer_get_zip_handle(void *handle, void **zip_handle) {
    mz_zip_writer *writer = (mz_zip_writer *)handle;
    if (zip_handle == NULL)
        return MZ_PARAM_ERROR;
    *zip_handle = writer->zip_handle;
    if (*zip_handle == NULL)
        return MZ_EXIST_ERROR;
    return MZ_OK;
}

/***************************************************************************/

void *mz_zip_writer_create(void **handle) {
    mz_zip_writer *writer = NULL;

    writer = (mz_zip_writer *)MZ_ALLOC(sizeof(mz_zip_writer));
    if (writer != NULL) {
        memset(writer, 0, sizeof(mz_zip_writer));
#if defined(HAVE_WZAES)
        writer->aes = 1;
#endif
#if defined(HAVE_ZLIB) || defined(HAVE_LIBCOMP)
        writer->compress_method = MZ_COMPRESS_METHOD_DEFLATE;
#elif defined(HAVE_BZIP2)
        writer->compress_method = MZ_COMPRESS_METHOD_BZIP2;
#elif defined(HAVE_LZMA)
        writer->compress_method = MZ_COMPRESS_METHOD_LZMA;
#else
        writer->compress_method = MZ_COMPRESS_METHOD_STORE;
#endif
        writer->compress_level = MZ_COMPRESS_LEVEL_BEST;
        writer->progress_cb_interval_ms = MZ_DEFAULT_PROGRESS_INTERVAL;
    }
    if (handle != NULL)
        *handle = writer;

    return writer;
}

void mz_zip_writer_delete(void **handle) {
    mz_zip_writer *writer = NULL;
    if (handle == NULL)
        return;
    writer = (mz_zip_writer *)*handle;
    if (writer != NULL) {
        mz_zip_writer_close(writer);

        if (writer->cert_data != NULL)
            MZ_FREE(writer->cert_data);

        writer->cert_data = NULL;
        writer->cert_data_size = 0;

        MZ_FREE(writer);
    }
    *handle = NULL;
}

/***************************************************************************/
