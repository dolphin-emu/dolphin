/* zip.c -- Zip manipulation
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng
   Copyright (C) 2009-2010 Mathias Svensson
     Modifications for Zip64 support
     http://result42.com
   Copyright (C) 2007-2008 Even Rouault
     Modifications of Unzip for Zip64
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/


#include "mz.h"
#include "mz_crypt.h"
#include "mz_strm.h"
#ifdef HAVE_BZIP2
#  include "mz_strm_bzip.h"
#endif
#ifdef HAVE_LIBCOMP
#  include "mz_strm_libcomp.h"
#endif
#ifdef HAVE_LZMA
#  include "mz_strm_lzma.h"
#endif
#include "mz_strm_mem.h"
#ifdef HAVE_PKCRYPT
#  include "mz_strm_pkcrypt.h"
#endif
#ifdef HAVE_WZAES
#  include "mz_strm_wzaes.h"
#endif
#ifdef HAVE_ZLIB
#  include "mz_strm_zlib.h"
#endif
#ifdef HAVE_ZSTD
#  include "mz_strm_zstd.h"
#endif

#include "mz_zip.h"

#include <ctype.h> /* tolower */
#include <stdio.h> /* snprintf */

#if defined(_MSC_VER) || defined(__MINGW32__)
#  define localtime_r(t1,t2) (localtime_s(t2,t1) == 0 ? t1 : NULL)
#endif
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#  define snprintf _snprintf
#endif

/***************************************************************************/

#define MZ_ZIP_MAGIC_LOCALHEADER        (0x04034b50)
#define MZ_ZIP_MAGIC_LOCALHEADERU8      { 0x50, 0x4b, 0x03, 0x04 }
#define MZ_ZIP_MAGIC_CENTRALHEADER      (0x02014b50)
#define MZ_ZIP_MAGIC_CENTRALHEADERU8    { 0x50, 0x4b, 0x01, 0x02 }
#define MZ_ZIP_MAGIC_ENDHEADER          (0x06054b50)
#define MZ_ZIP_MAGIC_ENDHEADERU8        { 0x50, 0x4b, 0x05, 0x06 }
#define MZ_ZIP_MAGIC_ENDHEADER64        (0x06064b50)
#define MZ_ZIP_MAGIC_ENDLOCHEADER64     (0x07064b50)
#define MZ_ZIP_MAGIC_DATADESCRIPTOR     (0x08074b50)
#define MZ_ZIP_MAGIC_DATADESCRIPTORU8   { 0x50, 0x4b, 0x07, 0x08 }

#define MZ_ZIP_SIZE_LD_ITEM             (30)
#define MZ_ZIP_SIZE_CD_ITEM             (46)
#define MZ_ZIP_SIZE_CD_LOCATOR64        (20)
#define MZ_ZIP_SIZE_MAX_DATA_DESCRIPTOR (24)

#define MZ_ZIP_OFFSET_CRC_SIZES         (14)
#define MZ_ZIP_UNCOMPR_SIZE64_CUSHION   (2 * 1024 * 1024)

#ifndef MZ_ZIP_EOCD_MAX_BACK
#define MZ_ZIP_EOCD_MAX_BACK            (1 << 20)
#endif

/***************************************************************************/

typedef struct mz_zip_s {
    mz_zip_file file_info;
    mz_zip_file local_file_info;

    void *stream;                   /* main stream */
    void *cd_stream;                /* pointer to the stream with the cd */
    void *cd_mem_stream;            /* memory stream for central directory */
    void *compress_stream;          /* compression stream */
    void *crypt_stream;             /* encryption stream */
    void *file_info_stream;         /* memory stream for storing file info */
    void *local_file_info_stream;   /* memory stream for storing local file info */

    int32_t  open_mode;
    uint8_t  recover;
    uint8_t  data_descriptor;

    uint32_t disk_number_with_cd;   /* number of the disk with the central dir */
    int64_t  disk_offset_shift;     /* correction for zips that have wrong offset start of cd */

    int64_t  cd_start_pos;          /* pos of the first file in the central dir stream */
    int64_t  cd_current_pos;        /* pos of the current file in the central dir */
    int64_t  cd_offset;             /* offset of start of central directory */
    int64_t  cd_size;               /* size of the central directory */
    uint32_t cd_signature;          /* signature of central directory */

    uint8_t  entry_scanned;         /* entry header information read ok */
    uint8_t  entry_opened;          /* entry is open for read/write */
    uint8_t  entry_raw;             /* entry opened with raw mode */
    uint32_t entry_crc32;           /* entry crc32  */

    uint64_t number_entry;

    uint16_t version_madeby;
    char     *comment;
} mz_zip;

/***************************************************************************/

#if 0
#  define mz_zip_print printf
#else
#  define mz_zip_print(fmt,...)
#endif

/***************************************************************************/

/* Locate the end of central directory */
static int32_t mz_zip_search_eocd(void *stream, int64_t *central_pos) {
    int64_t file_size = 0;
    int64_t max_back = MZ_ZIP_EOCD_MAX_BACK;
    uint8_t find[4] = MZ_ZIP_MAGIC_ENDHEADERU8;
    int32_t err = MZ_OK;

    err = mz_stream_seek(stream, 0, MZ_SEEK_END);
    if (err != MZ_OK)
        return err;

    file_size = mz_stream_tell(stream);

    if (max_back <= 0 || max_back > file_size)
        max_back = file_size;

    return mz_stream_find_reverse(stream, (const void *)find, sizeof(find), max_back, central_pos);
}

/* Locate the end of central directory 64 of a zip file */
static int32_t mz_zip_search_zip64_eocd(void *stream, const int64_t end_central_offset, int64_t *central_pos) {
    int64_t offset = 0;
    uint32_t value32 = 0;
    int32_t err = MZ_OK;


    *central_pos = 0;

    /* Zip64 end of central directory locator */
    err = mz_stream_seek(stream, end_central_offset - MZ_ZIP_SIZE_CD_LOCATOR64, MZ_SEEK_SET);
    /* Read locator signature */
    if (err == MZ_OK) {
        err = mz_stream_read_uint32(stream, &value32);
        if (value32 != MZ_ZIP_MAGIC_ENDLOCHEADER64)
            err = MZ_FORMAT_ERROR;
    }
    /* Number of the disk with the start of the zip64 end of  central directory */
    if (err == MZ_OK)
        err = mz_stream_read_uint32(stream, &value32);
    /* Relative offset of the zip64 end of central directory record8 */
    if (err == MZ_OK)
        err = mz_stream_read_uint64(stream, (uint64_t *)&offset);
    /* Total number of disks */
    if (err == MZ_OK)
        err = mz_stream_read_uint32(stream, &value32);
    /* Goto end of central directory record */
    if (err == MZ_OK)
        err = mz_stream_seek(stream, (int64_t)offset, MZ_SEEK_SET);
    /* The signature */
    if (err == MZ_OK) {
        err = mz_stream_read_uint32(stream, &value32);
        if (value32 != MZ_ZIP_MAGIC_ENDHEADER64)
            err = MZ_FORMAT_ERROR;
    }

    if (err == MZ_OK)
        *central_pos = offset;

    return err;
}

/* Get PKWARE traditional encryption verifier */
static uint16_t mz_zip_get_pk_verify(uint32_t dos_date, uint64_t crc, uint16_t flag)
{
    /* Info-ZIP modification to ZipCrypto format: if bit 3 of the general
     * purpose bit flag is set, it uses high byte of 16-bit File Time. */
    if (flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR)
        return ((dos_date >> 16) & 0xff) << 8 | ((dos_date >> 8) & 0xff);
    return ((crc >> 16) & 0xff) << 8 | ((crc >> 24) & 0xff);
}

/* Get info about the current file in the zip file */
static int32_t mz_zip_entry_read_header(void *stream, uint8_t local, mz_zip_file *file_info, void *file_extra_stream) {
    uint64_t ntfs_time = 0;
    uint32_t reserved = 0;
    uint32_t magic = 0;
    uint32_t dos_date = 0;
    uint32_t field_pos = 0;
    uint16_t field_type = 0;
    uint16_t field_length = 0;
    uint32_t field_length_read = 0;
    uint16_t ntfs_attrib_id = 0;
    uint16_t ntfs_attrib_size = 0;
    uint16_t linkname_size;
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    int64_t extrafield_pos = 0;
    int64_t comment_pos = 0;
    int64_t linkname_pos = 0;
    int64_t saved_pos = 0;
    int32_t err = MZ_OK;
    char *linkname = NULL;


    memset(file_info, 0, sizeof(mz_zip_file));

    /* Check the magic */
    err = mz_stream_read_uint32(stream, &magic);
    if (err == MZ_END_OF_STREAM)
        err = MZ_END_OF_LIST;
    else if (magic == MZ_ZIP_MAGIC_ENDHEADER || magic == MZ_ZIP_MAGIC_ENDHEADER64)
        err = MZ_END_OF_LIST;
    else if ((local) && (magic != MZ_ZIP_MAGIC_LOCALHEADER))
        err = MZ_FORMAT_ERROR;
    else if ((!local) && (magic != MZ_ZIP_MAGIC_CENTRALHEADER))
        err = MZ_FORMAT_ERROR;

    /* Read header fields */
    if (err == MZ_OK) {
        if (!local)
            err = mz_stream_read_uint16(stream, &file_info->version_madeby);
        if (err == MZ_OK)
            err = mz_stream_read_uint16(stream, &file_info->version_needed);
        if (err == MZ_OK)
            err = mz_stream_read_uint16(stream, &file_info->flag);
        if (err == MZ_OK)
            err = mz_stream_read_uint16(stream, &file_info->compression_method);
        if (err == MZ_OK) {
            err = mz_stream_read_uint32(stream, &dos_date);
            file_info->modified_date = mz_zip_dosdate_to_time_t(dos_date);
        }
        if (err == MZ_OK)
            err = mz_stream_read_uint32(stream, &file_info->crc);
#ifdef HAVE_PKCRYPT
        if (err == MZ_OK && file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) {
            /* Use dos_date from header instead of derived from time in zip extensions */
            file_info->pk_verify = mz_zip_get_pk_verify(dos_date, file_info->crc, file_info->flag);
        }
#endif
        if (err == MZ_OK) {
            err = mz_stream_read_uint32(stream, &value32);
            file_info->compressed_size = value32;
        }
        if (err == MZ_OK) {
            err = mz_stream_read_uint32(stream, &value32);
            file_info->uncompressed_size = value32;
        }
        if (err == MZ_OK)
            err = mz_stream_read_uint16(stream, &file_info->filename_size);
        if (err == MZ_OK)
            err = mz_stream_read_uint16(stream, &file_info->extrafield_size);
        if (!local) {
            if (err == MZ_OK)
                err = mz_stream_read_uint16(stream, &file_info->comment_size);
            if (err == MZ_OK) {
                err = mz_stream_read_uint16(stream, &value16);
                file_info->disk_number = value16;
            }
            if (err == MZ_OK)
                err = mz_stream_read_uint16(stream, &file_info->internal_fa);
            if (err == MZ_OK)
                err = mz_stream_read_uint32(stream, &file_info->external_fa);
            if (err == MZ_OK) {
                err = mz_stream_read_uint32(stream, &value32);
                file_info->disk_offset = value32;
            }
        }
    }

    if (err == MZ_OK)
        err = mz_stream_seek(file_extra_stream, 0, MZ_SEEK_SET);

    /* Copy variable length data to memory stream for later retrieval */
    if ((err == MZ_OK) && (file_info->filename_size > 0))
        err = mz_stream_copy(file_extra_stream, stream, file_info->filename_size);
    mz_stream_write_uint8(file_extra_stream, 0);
    extrafield_pos = mz_stream_tell(file_extra_stream);

    if ((err == MZ_OK) && (file_info->extrafield_size > 0))
        err = mz_stream_copy(file_extra_stream, stream, file_info->extrafield_size);
    mz_stream_write_uint8(file_extra_stream, 0);

    comment_pos = mz_stream_tell(file_extra_stream);
    if ((err == MZ_OK) && (file_info->comment_size > 0))
        err = mz_stream_copy(file_extra_stream, stream, file_info->comment_size);
    mz_stream_write_uint8(file_extra_stream, 0);

    linkname_pos = mz_stream_tell(file_extra_stream);
    /* Overwrite if we encounter UNIX1 extra block */
    mz_stream_write_uint8(file_extra_stream, 0);

    if ((err == MZ_OK) && (file_info->extrafield_size > 0)) {
        /* Seek to and parse the extra field */
        err = mz_stream_seek(file_extra_stream, extrafield_pos, MZ_SEEK_SET);

        while ((err == MZ_OK) && (field_pos + 4 <= file_info->extrafield_size)) {
            err = mz_zip_extrafield_read(file_extra_stream, &field_type, &field_length);
            if (err != MZ_OK)
                break;
            field_pos += 4;

            /* Don't allow field length to exceed size of remaining extrafield */
            if (field_length > (file_info->extrafield_size - field_pos))
                field_length = (uint16_t)(file_info->extrafield_size - field_pos);

            /* Read ZIP64 extra field */
            if ((field_type == MZ_ZIP_EXTENSION_ZIP64) && (field_length >= 8)) {
                if ((err == MZ_OK) && (file_info->uncompressed_size == UINT32_MAX)) {
                    err = mz_stream_read_int64(file_extra_stream, &file_info->uncompressed_size);
                    if (file_info->uncompressed_size < 0)
                        err = MZ_FORMAT_ERROR;
                }
                if ((err == MZ_OK) && (file_info->compressed_size == UINT32_MAX)) {
                    err = mz_stream_read_int64(file_extra_stream, &file_info->compressed_size);
                    if (file_info->compressed_size < 0)
                        err = MZ_FORMAT_ERROR;
                }
                if ((err == MZ_OK) && (file_info->disk_offset == UINT32_MAX)) {
                    err = mz_stream_read_int64(file_extra_stream, &file_info->disk_offset);
                    if (file_info->disk_offset < 0)
                        err = MZ_FORMAT_ERROR;
                }
                if ((err == MZ_OK) && (file_info->disk_number == UINT16_MAX))
                    err = mz_stream_read_uint32(file_extra_stream, &file_info->disk_number);
            }
            /* Read NTFS extra field */
            else if ((field_type == MZ_ZIP_EXTENSION_NTFS) && (field_length > 4)) {
                if (err == MZ_OK)
                    err = mz_stream_read_uint32(file_extra_stream, &reserved);
                field_length_read = 4;

                while ((err == MZ_OK) && (field_length_read + 4 <= field_length)) {
                    err = mz_stream_read_uint16(file_extra_stream, &ntfs_attrib_id);
                    if (err == MZ_OK)
                        err = mz_stream_read_uint16(file_extra_stream, &ntfs_attrib_size);
                    field_length_read += 4;

                    if ((err == MZ_OK) && (ntfs_attrib_id == 0x01) && (ntfs_attrib_size == 24)) {
                        err = mz_stream_read_uint64(file_extra_stream, &ntfs_time);
                        mz_zip_ntfs_to_unix_time(ntfs_time, &file_info->modified_date);

                        if (err == MZ_OK) {
                            err = mz_stream_read_uint64(file_extra_stream, &ntfs_time);
                            mz_zip_ntfs_to_unix_time(ntfs_time, &file_info->accessed_date);
                        }
                        if (err == MZ_OK) {
                            err = mz_stream_read_uint64(file_extra_stream, &ntfs_time);
                            mz_zip_ntfs_to_unix_time(ntfs_time, &file_info->creation_date);
                        }
                    } else if ((err == MZ_OK) && (field_length_read + ntfs_attrib_size <= field_length)) {
                        err = mz_stream_seek(file_extra_stream, ntfs_attrib_size, MZ_SEEK_CUR);
                    }

                    field_length_read += ntfs_attrib_size;
                }
            }
            /* Read UNIX1 extra field */
            else if ((field_type == MZ_ZIP_EXTENSION_UNIX1) && (field_length >= 12)) {
                if (err == MZ_OK) {
                    err = mz_stream_read_uint32(file_extra_stream, &value32);
                    if (err == MZ_OK && file_info->accessed_date == 0)
                        file_info->accessed_date = value32;
                }
                if (err == MZ_OK) {
                    err = mz_stream_read_uint32(file_extra_stream, &value32);
                    if (err == MZ_OK && file_info->modified_date == 0)
                        file_info->modified_date = value32;
                }
                if (err == MZ_OK)
                    err = mz_stream_read_uint16(file_extra_stream, &value16); /* User id */
                if (err == MZ_OK)
                    err = mz_stream_read_uint16(file_extra_stream, &value16); /* Group id */

                /* Copy linkname to end of file extra stream so we can return null
                   terminated string */
                linkname_size = field_length - 12;
                if ((err == MZ_OK) && (linkname_size > 0)) {
                    linkname = (char *)MZ_ALLOC(linkname_size);
                    if (linkname != NULL) {
                        if (mz_stream_read(file_extra_stream, linkname, linkname_size) != linkname_size)
                            err = MZ_READ_ERROR;
                        if (err == MZ_OK) {
                            saved_pos = mz_stream_tell(file_extra_stream);

                            mz_stream_seek(file_extra_stream, linkname_pos, MZ_SEEK_SET);
                            mz_stream_write(file_extra_stream, linkname, linkname_size);
                            mz_stream_write_uint8(file_extra_stream, 0);

                            mz_stream_seek(file_extra_stream, saved_pos, MZ_SEEK_SET);
                        }
                        MZ_FREE(linkname);
                    }
                }
            }
#ifdef HAVE_WZAES
            /* Read AES extra field */
            else if ((field_type == MZ_ZIP_EXTENSION_AES) && (field_length == 7)) {
                uint8_t value8 = 0;
                /* Verify version info */
                err = mz_stream_read_uint16(file_extra_stream, &value16);
                /* Support AE-1 and AE-2 */
                if (value16 != 1 && value16 != 2)
                    err = MZ_FORMAT_ERROR;
                file_info->aes_version = value16;
                if (err == MZ_OK)
                    err = mz_stream_read_uint8(file_extra_stream, &value8);
                if ((char)value8 != 'A')
                    err = MZ_FORMAT_ERROR;
                if (err == MZ_OK)
                    err = mz_stream_read_uint8(file_extra_stream, &value8);
                if ((char)value8 != 'E')
                    err = MZ_FORMAT_ERROR;
                /* Get AES encryption strength and actual compression method */
                if (err == MZ_OK) {
                    err = mz_stream_read_uint8(file_extra_stream, &value8);
                    file_info->aes_encryption_mode = value8;
                }
                if (err == MZ_OK) {
                    err = mz_stream_read_uint16(file_extra_stream, &value16);
                    file_info->compression_method = value16;
                }
            }
#endif
            else if (field_length > 0) {
                err = mz_stream_seek(file_extra_stream, field_length, MZ_SEEK_CUR);
            }

            field_pos += field_length;
        }
    }

    /* Get pointers to variable length data */
    mz_stream_mem_get_buffer(file_extra_stream, (const void **)&file_info->filename);
    mz_stream_mem_get_buffer_at(file_extra_stream, extrafield_pos, (const void **)&file_info->extrafield);
    mz_stream_mem_get_buffer_at(file_extra_stream, comment_pos, (const void **)&file_info->comment);
    mz_stream_mem_get_buffer_at(file_extra_stream, linkname_pos, (const void **)&file_info->linkname);

    /* Set to empty string just in-case */
    if (file_info->filename == NULL)
        file_info->filename = "";
    if (file_info->extrafield == NULL)
        file_info->extrafield_size = 0;
    if (file_info->comment == NULL)
        file_info->comment = "";
    if (file_info->linkname == NULL)
        file_info->linkname = "";

    if (err == MZ_OK) {
        mz_zip_print("Zip - Entry - Read header - %s (local %" PRId8 ")\n",
            file_info->filename, local);
        mz_zip_print("Zip - Entry - Read header compress (ucs %" PRId64 " cs %" PRId64 " crc 0x%08" PRIx32 ")\n",
            file_info->uncompressed_size, file_info->compressed_size, file_info->crc);
        if (!local) {
            mz_zip_print("Zip - Entry - Read header disk (disk %" PRIu32 " offset %" PRId64 ")\n",
                file_info->disk_number, file_info->disk_offset);
        }
        mz_zip_print("Zip - Entry - Read header variable (fnl %" PRId32 " efs %" PRId32 " cms %" PRId32 ")\n",
            file_info->filename_size, file_info->extrafield_size, file_info->comment_size);
    }

    return err;
}

static int32_t mz_zip_entry_read_descriptor(void *stream, uint8_t zip64, uint32_t *crc32, int64_t *compressed_size, int64_t *uncompressed_size) {
    uint32_t value32 = 0;
    int64_t value64 = 0;
    int32_t err = MZ_OK;


    err = mz_stream_read_uint32(stream, &value32);
    if (value32 != MZ_ZIP_MAGIC_DATADESCRIPTOR)
        err = MZ_FORMAT_ERROR;
    if (err == MZ_OK)
        err = mz_stream_read_uint32(stream, &value32);
    if ((err == MZ_OK) && (crc32 != NULL))
        *crc32 = value32;
    if (err == MZ_OK) {
        /* If zip 64 extension is enabled then read as 8 byte */
        if (!zip64) {
            err = mz_stream_read_uint32(stream, &value32);
            value64 = value32;
        } else {
            err = mz_stream_read_int64(stream, &value64);
            if (value64 < 0)
                err = MZ_FORMAT_ERROR;
        }
        if ((err == MZ_OK) && (compressed_size != NULL))
            *compressed_size = value64;
    }
    if (err == MZ_OK) {
        if (!zip64) {
            err = mz_stream_read_uint32(stream, &value32);
            value64 = value32;
        } else {
            err = mz_stream_read_int64(stream, &value64);
            if (value64 < 0)
                err = MZ_FORMAT_ERROR;
        }
        if ((err == MZ_OK) && (uncompressed_size != NULL))
            *uncompressed_size = value64;
    }

    return err;
}

static int32_t mz_zip_entry_write_crc_sizes(void *stream, uint8_t zip64, uint8_t mask, mz_zip_file *file_info) {
    int32_t err = MZ_OK;

    if (mask)
        err = mz_stream_write_uint32(stream, 0);
    else
        err = mz_stream_write_uint32(stream, file_info->crc); /* crc */

    /* For backwards-compatibility with older zip applications we set all sizes to UINT32_MAX
     * when zip64 is needed, instead of only setting sizes larger than UINT32_MAX. */

    if (err == MZ_OK) {
        if (zip64) /* compr size */
            err = mz_stream_write_uint32(stream, UINT32_MAX);
        else
            err = mz_stream_write_uint32(stream, (uint32_t)file_info->compressed_size);
    }
    if (err == MZ_OK) {
        if (mask) /* uncompr size */
            err = mz_stream_write_uint32(stream, 0);
        else if (zip64)
            err = mz_stream_write_uint32(stream, UINT32_MAX);
        else
            err = mz_stream_write_uint32(stream, (uint32_t)file_info->uncompressed_size);
    }
    return err;
}

static int32_t mz_zip_entry_needs_zip64(mz_zip_file *file_info, uint8_t local, uint8_t *zip64) {
    uint32_t max_uncompressed_size = UINT32_MAX;
    uint8_t needs_zip64 = 0;

    if (zip64 == NULL)
        return MZ_PARAM_ERROR;

    *zip64 = 0;

    if (local) {
        /* At local header we might not know yet whether compressed size will overflow unsigned
           32-bit integer which might happen for high entropy data so we give it some cushion */

        max_uncompressed_size -= MZ_ZIP_UNCOMPR_SIZE64_CUSHION;
    }

    needs_zip64 = (file_info->uncompressed_size >= max_uncompressed_size) ||
                  (file_info->compressed_size >= UINT32_MAX);

    if (!local) {
        /* Disk offset and number only used in central directory header */
        needs_zip64 |= (file_info->disk_offset >= UINT32_MAX) ||
                       (file_info->disk_number >= UINT16_MAX);
    }

    if (file_info->zip64 == MZ_ZIP64_AUTO) {
        /* If uncompressed size is unknown, assume zip64 for 64-bit data descriptors */
        if (local && file_info->uncompressed_size == 0) {
            /* Don't use zip64 for local header directory entries */
            if (mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby) != MZ_OK) {
                *zip64 = 1;
            }
        }
        *zip64 |= needs_zip64;
    } else if (file_info->zip64 == MZ_ZIP64_FORCE) {
        *zip64 = 1;
    } else if (file_info->zip64 == MZ_ZIP64_DISABLE) {
        /* Zip64 extension is required to zip file */
        if (needs_zip64)
            return MZ_PARAM_ERROR;
    }

    return MZ_OK;
}

static int32_t mz_zip_entry_write_header(void *stream, uint8_t local, mz_zip_file *file_info) {
    uint64_t ntfs_time = 0;
    uint32_t reserved = 0;
    uint32_t dos_date = 0;
    uint16_t extrafield_size = 0;
    uint16_t field_type = 0;
    uint16_t field_length = 0;
    uint16_t field_length_zip64 = 0;
    uint16_t field_length_ntfs = 0;
    uint16_t field_length_aes = 0;
    uint16_t field_length_unix1 = 0;
    uint16_t filename_size = 0;
    uint16_t filename_length = 0;
    uint16_t linkname_size = 0;
    uint16_t version_needed = 0;
    int32_t comment_size = 0;
    int32_t err = MZ_OK;
    int32_t err_mem = MZ_OK;
    uint8_t zip64 = 0;
    uint8_t skip_aes = 0;
    uint8_t mask = 0;
    uint8_t write_end_slash = 0;
    const char *filename = NULL;
    char masked_name[64];
    void *file_extra_stream = NULL;

    if (file_info == NULL)
        return MZ_PARAM_ERROR;

    if ((local) && (file_info->flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO))
        mask = 1;

    /* Determine if zip64 extra field is necessary */
    err = mz_zip_entry_needs_zip64(file_info, local, &zip64);
    if (err != MZ_OK)
        return err;

    /* Start calculating extra field sizes */
    if (zip64) {
        /* Both compressed and uncompressed sizes must be included (at least in local header) */
        field_length_zip64 = 8 + 8;
        if ((!local) && (file_info->disk_offset >= UINT32_MAX))
            field_length_zip64 += 8;

        extrafield_size += 4;
        extrafield_size += field_length_zip64;
    }

    /* Calculate extra field size and check for duplicates */
    if (file_info->extrafield_size > 0) {
        mz_stream_mem_create(&file_extra_stream);
        mz_stream_mem_set_buffer(file_extra_stream, (void *)file_info->extrafield,
            file_info->extrafield_size);

        do {
            err_mem = mz_stream_read_uint16(file_extra_stream, &field_type);
            if (err_mem == MZ_OK)
                err_mem = mz_stream_read_uint16(file_extra_stream, &field_length);
            if (err_mem != MZ_OK)
                break;

            /* Prefer incoming aes extensions over ours */
            if (field_type == MZ_ZIP_EXTENSION_AES)
                skip_aes = 1;

            /* Prefer our zip64, ntfs, unix1 extension over incoming */
            if (field_type != MZ_ZIP_EXTENSION_ZIP64 && field_type != MZ_ZIP_EXTENSION_NTFS &&
                field_type != MZ_ZIP_EXTENSION_UNIX1)
                extrafield_size += 4 + field_length;

            if (err_mem == MZ_OK)
                err_mem = mz_stream_seek(file_extra_stream, field_length, MZ_SEEK_CUR);
        } while (err_mem == MZ_OK);
    }

#ifdef HAVE_WZAES
    if (!skip_aes) {
        if ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version)) {
            field_length_aes = 1 + 1 + 1 + 2 + 2;
            extrafield_size += 4 + field_length_aes;
        }
    }
#else
    MZ_UNUSED(field_length_aes);
    MZ_UNUSED(skip_aes);
#endif
    /* NTFS timestamps */
    if ((file_info->modified_date != 0) &&
        (file_info->accessed_date != 0) &&
        (file_info->creation_date != 0) && (!mask)) {
        field_length_ntfs = 8 + 8 + 8 + 4 + 2 + 2;
        extrafield_size += 4 + field_length_ntfs;
    }

    /* Unix1 symbolic links */
    if (file_info->linkname != NULL && *file_info->linkname != 0) {
        linkname_size = (uint16_t)strlen(file_info->linkname);
        field_length_unix1 = 12 + linkname_size;
        extrafield_size += 4 + field_length_unix1;
    }

    if (local)
        err = mz_stream_write_uint32(stream, MZ_ZIP_MAGIC_LOCALHEADER);
    else {
        err = mz_stream_write_uint32(stream, MZ_ZIP_MAGIC_CENTRALHEADER);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, file_info->version_madeby);
    }

    /* Calculate version needed to extract */
    if (err == MZ_OK) {
        version_needed = file_info->version_needed;
        if (version_needed == 0) {
            version_needed = 20;
            if (zip64)
                version_needed = 45;
#ifdef HAVE_WZAES
            if ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version))
                version_needed = 51;
#endif
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
            if ((file_info->compression_method == MZ_COMPRESS_METHOD_LZMA) ||
                (file_info->compression_method == MZ_COMPRESS_METHOD_XZ))
                version_needed = 63;
#endif
        }
        err = mz_stream_write_uint16(stream, version_needed);
    }
    if (err == MZ_OK)
        err = mz_stream_write_uint16(stream, file_info->flag);
    if (err == MZ_OK) {
#ifdef HAVE_WZAES
        if ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version))
            err = mz_stream_write_uint16(stream, MZ_COMPRESS_METHOD_AES);
        else
#endif
            err = mz_stream_write_uint16(stream, file_info->compression_method);
    }
    if (err == MZ_OK) {
        if (file_info->modified_date != 0 && !mask)
            dos_date = mz_zip_time_t_to_dos_date(file_info->modified_date);
        err = mz_stream_write_uint32(stream, dos_date);
    }

    if (err == MZ_OK)
        err = mz_zip_entry_write_crc_sizes(stream, zip64, mask, file_info);

    if (mask) {
        snprintf(masked_name, sizeof(masked_name), "%" PRIx32 "_%" PRIx64,
            file_info->disk_number, file_info->disk_offset);
        filename = masked_name;
    } else {
        filename = file_info->filename;
    }

    filename_length = (uint16_t)strlen(filename);
    filename_size += filename_length;

    if ((mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby) == MZ_OK) &&
        ((filename[filename_length - 1] != '/') && (filename[filename_length - 1] != '\\'))) {
        filename_size += 1;
        write_end_slash = 1;
    }

    if (err == MZ_OK)
        err = mz_stream_write_uint16(stream, filename_size);
    if (err == MZ_OK)
        err = mz_stream_write_uint16(stream, extrafield_size);

    if (!local) {
        if (file_info->comment != NULL) {
            comment_size = (int32_t)strlen(file_info->comment);
            if (comment_size > UINT16_MAX)
                comment_size = UINT16_MAX;
        }
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, (uint16_t)comment_size);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, (uint16_t)file_info->disk_number);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, file_info->internal_fa);
        if (err == MZ_OK)
            err = mz_stream_write_uint32(stream, file_info->external_fa);
        if (err == MZ_OK) {
            if (file_info->disk_offset >= UINT32_MAX)
                err = mz_stream_write_uint32(stream, UINT32_MAX);
            else
                err = mz_stream_write_uint32(stream, (uint32_t)file_info->disk_offset);
        }
    }

    if (err == MZ_OK) {
        if (mz_stream_write(stream, filename, filename_length) != filename_length)
            err = MZ_WRITE_ERROR;

        /* Ensure that directories have a slash appended to them for compatibility */
        if (err == MZ_OK && write_end_slash)
            err = mz_stream_write_uint8(stream, '/');
    }

    /* Write ZIP64 extra field first so we can update sizes later if data descriptor not used */
    if ((err == MZ_OK) && (zip64)) {
        err = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_ZIP64, field_length_zip64);
        if (err == MZ_OK) {
            if (mask)
                err = mz_stream_write_int64(stream, 0);
            else
                err = mz_stream_write_int64(stream, file_info->uncompressed_size);
        }
        if (err == MZ_OK)
            err = mz_stream_write_int64(stream, file_info->compressed_size);
        if ((err == MZ_OK) && (!local) && (file_info->disk_offset >= UINT32_MAX))
            err = mz_stream_write_int64(stream, file_info->disk_offset);
        if ((err == MZ_OK) && (!local) && (file_info->disk_number >= UINT16_MAX))
            err = mz_stream_write_uint32(stream, file_info->disk_number);
    }
    /* Write NTFS extra field */
    if ((err == MZ_OK) && (field_length_ntfs > 0)) {
        err = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_NTFS, field_length_ntfs);
        if (err == MZ_OK)
            err = mz_stream_write_uint32(stream, reserved);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, 0x01);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, field_length_ntfs - 8);
        if (err == MZ_OK) {
            mz_zip_unix_to_ntfs_time(file_info->modified_date, &ntfs_time);
            err = mz_stream_write_uint64(stream, ntfs_time);
        }
        if (err == MZ_OK) {
            mz_zip_unix_to_ntfs_time(file_info->accessed_date, &ntfs_time);
            err = mz_stream_write_uint64(stream, ntfs_time);
        }
        if (err == MZ_OK) {
            mz_zip_unix_to_ntfs_time(file_info->creation_date, &ntfs_time);
            err = mz_stream_write_uint64(stream, ntfs_time);
        }
    }
    /* Write UNIX extra block extra field */
    if ((err == MZ_OK) && (field_length_unix1 > 0)) {
        err = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_UNIX1, field_length_unix1);
        if (err == MZ_OK)
            err = mz_stream_write_uint32(stream, (uint32_t)file_info->accessed_date);
        if (err == MZ_OK)
            err = mz_stream_write_uint32(stream, (uint32_t)file_info->modified_date);
        if (err == MZ_OK) /* User id */
            err = mz_stream_write_uint16(stream, 0);
        if (err == MZ_OK) /* Group id */
            err = mz_stream_write_uint16(stream, 0);
        if (err == MZ_OK && linkname_size > 0) {
            if (mz_stream_write(stream, file_info->linkname, linkname_size) != linkname_size)
                err = MZ_WRITE_ERROR;
        }
    }
#ifdef HAVE_WZAES
    /* Write AES extra field */
    if ((err == MZ_OK) && (!skip_aes) && (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) && (file_info->aes_version)) {
        err = mz_zip_extrafield_write(stream, MZ_ZIP_EXTENSION_AES, field_length_aes);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, file_info->aes_version);
        if (err == MZ_OK)
            err = mz_stream_write_uint8(stream, 'A');
        if (err == MZ_OK)
            err = mz_stream_write_uint8(stream, 'E');
        if (err == MZ_OK)
            err = mz_stream_write_uint8(stream, file_info->aes_encryption_mode);
        if (err == MZ_OK)
            err = mz_stream_write_uint16(stream, file_info->compression_method);
    }
#endif

    if (file_info->extrafield_size > 0) {
        err_mem = mz_stream_mem_seek(file_extra_stream, 0, MZ_SEEK_SET);
        while (err == MZ_OK && err_mem == MZ_OK) {
            err_mem = mz_stream_read_uint16(file_extra_stream, &field_type);
            if (err_mem == MZ_OK)
                err_mem = mz_stream_read_uint16(file_extra_stream, &field_length);
            if (err_mem != MZ_OK)
                break;

            /* Prefer our zip 64, ntfs, unix1 extensions over incoming */
            if (field_type == MZ_ZIP_EXTENSION_ZIP64 || field_type == MZ_ZIP_EXTENSION_NTFS ||
                field_type == MZ_ZIP_EXTENSION_UNIX1) {
                err_mem = mz_stream_seek(file_extra_stream, field_length, MZ_SEEK_CUR);
                continue;
            }

            err = mz_stream_write_uint16(stream, field_type);
            if (err == MZ_OK)
                err = mz_stream_write_uint16(stream, field_length);
            if (err == MZ_OK)
                err = mz_stream_copy(stream, file_extra_stream, field_length);
        }

        mz_stream_mem_delete(&file_extra_stream);
    }

    if ((err == MZ_OK) && (!local) && (file_info->comment != NULL)) {
        if (mz_stream_write(stream, file_info->comment, file_info->comment_size) != file_info->comment_size)
            err = MZ_WRITE_ERROR;
    }

    return err;
}

static int32_t mz_zip_entry_write_descriptor(void *stream, uint8_t zip64, uint32_t crc32, int64_t compressed_size, int64_t uncompressed_size) {
    int32_t err = MZ_OK;

    err = mz_stream_write_uint32(stream, MZ_ZIP_MAGIC_DATADESCRIPTOR);
    if (err == MZ_OK)
        err = mz_stream_write_uint32(stream, crc32);

    /* Store data descriptor as 8 bytes if zip 64 extension enabled */
    if (err == MZ_OK) {
        /* Zip 64 extension is enabled when uncompressed size is > UINT32_MAX */
        if (!zip64)
            err = mz_stream_write_uint32(stream, (uint32_t)compressed_size);
        else
            err = mz_stream_write_int64(stream, compressed_size);
    }
    if (err == MZ_OK) {
        if (!zip64)
            err = mz_stream_write_uint32(stream, (uint32_t)uncompressed_size);
        else
            err = mz_stream_write_int64(stream, uncompressed_size);
    }

    return err;
}

static int32_t mz_zip_read_cd(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    uint64_t number_entry_cd64 = 0;
    uint64_t number_entry_cd = 0;
    int64_t eocd_pos = 0;
    int64_t eocd_pos64 = 0;
    int64_t value64i = 0;
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    uint64_t value64 = 0;
    uint16_t comment_size = 0;
    int32_t comment_read = 0;
    int32_t err = MZ_OK;


    if (zip == NULL)
        return MZ_PARAM_ERROR;

    /* Read and cache central directory records */
    err = mz_zip_search_eocd(zip->stream, &eocd_pos);
    if (err == MZ_OK) {
        /* The signature, already checked */
        err = mz_stream_read_uint32(zip->stream, &value32);
        /* Number of this disk */
        if (err == MZ_OK)
            err = mz_stream_read_uint16(zip->stream, &value16);
        /* Number of the disk with the start of the central directory */
        if (err == MZ_OK)
            err = mz_stream_read_uint16(zip->stream, &value16);
        zip->disk_number_with_cd = value16;
        /* Total number of entries in the central dir on this disk */
        if (err == MZ_OK)
            err = mz_stream_read_uint16(zip->stream, &value16);
        zip->number_entry = value16;
        /* Total number of entries in the central dir */
        if (err == MZ_OK)
            err = mz_stream_read_uint16(zip->stream, &value16);
        number_entry_cd = value16;
        if (number_entry_cd != zip->number_entry)
            err = MZ_FORMAT_ERROR;
        /* Size of the central directory */
        if (err == MZ_OK)
            err = mz_stream_read_uint32(zip->stream, &value32);
        if (err == MZ_OK)
            zip->cd_size = value32;
        /* Offset of start of central directory with respect to the starting disk number */
        if (err == MZ_OK)
            err = mz_stream_read_uint32(zip->stream, &value32);
        if (err == MZ_OK)
            zip->cd_offset = value32;
        /* Zip file global comment length */
        if (err == MZ_OK)
            err = mz_stream_read_uint16(zip->stream, &comment_size);
        if ((err == MZ_OK) && (comment_size > 0)) {
            zip->comment = (char *)MZ_ALLOC(comment_size + 1);
            if (zip->comment != NULL) {
                comment_read = mz_stream_read(zip->stream, zip->comment, comment_size);
                /* Don't fail if incorrect comment length read, not critical */
                if (comment_read < 0)
                    comment_read = 0;
                zip->comment[comment_read] = 0;
            }
        }

        if ((err == MZ_OK) && ((number_entry_cd == UINT16_MAX) || (zip->cd_offset == UINT32_MAX))) {
            /* Format should be Zip64, as the central directory or file size is too large */
            if (mz_zip_search_zip64_eocd(zip->stream, eocd_pos, &eocd_pos64) == MZ_OK) {
                eocd_pos = eocd_pos64;

                err = mz_stream_seek(zip->stream, eocd_pos, MZ_SEEK_SET);
                /* The signature, already checked */
                if (err == MZ_OK)
                    err = mz_stream_read_uint32(zip->stream, &value32);
                /* Size of zip64 end of central directory record */
                if (err == MZ_OK)
                    err = mz_stream_read_uint64(zip->stream, &value64);
                /* Version made by */
                if (err == MZ_OK)
                    err = mz_stream_read_uint16(zip->stream, &zip->version_madeby);
                /* Version needed to extract */
                if (err == MZ_OK)
                    err = mz_stream_read_uint16(zip->stream, &value16);
                /* Number of this disk */
                if (err == MZ_OK)
                    err = mz_stream_read_uint32(zip->stream, &value32);
                /* Number of the disk with the start of the central directory */
                if (err == MZ_OK)
                    err = mz_stream_read_uint32(zip->stream, &zip->disk_number_with_cd);
                /* Total number of entries in the central directory on this disk */
                if (err == MZ_OK)
                    err = mz_stream_read_uint64(zip->stream, &zip->number_entry);
                /* Total number of entries in the central directory */
                if (err == MZ_OK)
                    err = mz_stream_read_uint64(zip->stream, &number_entry_cd64);
                if (zip->number_entry != number_entry_cd64)
                    err = MZ_FORMAT_ERROR;
                /* Size of the central directory */
                if (err == MZ_OK) {
                    err = mz_stream_read_int64(zip->stream, &zip->cd_size);
                    if (zip->cd_size < 0)
                        err = MZ_FORMAT_ERROR;
                }
                /* Offset of start of central directory with respect to the starting disk number */
                if (err == MZ_OK) {
                    err = mz_stream_read_int64(zip->stream, &zip->cd_offset);
                    if (zip->cd_offset < 0)
                        err = MZ_FORMAT_ERROR;
                }
            } else if ((zip->number_entry == UINT16_MAX) || (number_entry_cd != zip->number_entry) ||
                       (zip->cd_size == UINT16_MAX) || (zip->cd_offset == UINT32_MAX)) {
                err = MZ_FORMAT_ERROR;
            }
        }
    }

    if (err == MZ_OK) {
        mz_zip_print("Zip - Read cd (disk %" PRId32 " entries %" PRId64 " offset %" PRId64 " size %" PRId64 ")\n",
            zip->disk_number_with_cd, zip->number_entry, zip->cd_offset, zip->cd_size);

        /* Verify central directory signature exists at offset */
        err = mz_stream_seek(zip->stream, zip->cd_offset, MZ_SEEK_SET);
        if (err == MZ_OK)
            err = mz_stream_read_uint32(zip->stream, &zip->cd_signature);
        if ((err == MZ_OK) && (zip->cd_signature != MZ_ZIP_MAGIC_CENTRALHEADER)) {
            /* If cd exists in large file and no zip-64 support, error for recover */
            if (eocd_pos > UINT32_MAX && eocd_pos64 == 0)
                err = MZ_FORMAT_ERROR;
            /* If cd not found attempt to seek backward to find it */
            if (err == MZ_OK)
                err = mz_stream_seek(zip->stream, eocd_pos - zip->cd_size, MZ_SEEK_SET);
            if (err == MZ_OK)
                err = mz_stream_read_uint32(zip->stream, &zip->cd_signature);
            if ((err == MZ_OK) && (zip->cd_signature == MZ_ZIP_MAGIC_CENTRALHEADER)) {

                /* If found compensate for incorrect locations */
                value64i = zip->cd_offset;
                zip->cd_offset = eocd_pos - zip->cd_size;
                /* Assume disk has prepended data */
                zip->disk_offset_shift = zip->cd_offset - value64i;
            }
        }
    }

    if (err == MZ_OK) {
        if (eocd_pos < zip->cd_offset) {
            /* End of central dir should always come after central dir */
            err = MZ_FORMAT_ERROR;
        } else if ((uint64_t)eocd_pos < (uint64_t)zip->cd_offset + zip->cd_size) {
            /* Truncate size of cd if incorrect size or offset provided */
            zip->cd_size = eocd_pos - zip->cd_offset;
        }
    }

    return err;
}

static int32_t mz_zip_write_cd(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    int64_t zip64_eocd_pos_inzip = 0;
    int64_t disk_number = 0;
    int64_t disk_size = 0;
    int32_t comment_size = 0;
    int32_t err = MZ_OK;


    if (zip == NULL)
        return MZ_PARAM_ERROR;

    if (mz_stream_get_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &disk_number) == MZ_OK)
        zip->disk_number_with_cd = (uint32_t)disk_number;
    if (mz_stream_get_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_SIZE, &disk_size) == MZ_OK && disk_size > 0)
        zip->disk_number_with_cd += 1;
    mz_stream_set_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, -1);
    if ((zip->disk_number_with_cd > 0) && (zip->open_mode & MZ_OPEN_MODE_APPEND)) {
        // Overwrite existing central directory if using split disks
        mz_stream_seek(zip->stream, 0, MZ_SEEK_SET);
    }

    zip->cd_offset = mz_stream_tell(zip->stream);
    mz_stream_seek(zip->cd_mem_stream, 0, MZ_SEEK_END);
    zip->cd_size = (uint32_t)mz_stream_tell(zip->cd_mem_stream);
    mz_stream_seek(zip->cd_mem_stream, 0, MZ_SEEK_SET);

    err = mz_stream_copy(zip->stream, zip->cd_mem_stream, (int32_t)zip->cd_size);

    mz_zip_print("Zip - Write cd (disk %" PRId32 " entries %" PRId64 " offset %" PRId64 " size %" PRId64 ")\n",
        zip->disk_number_with_cd, zip->number_entry, zip->cd_offset, zip->cd_size);

    if (zip->cd_size == 0 && zip->number_entry > 0) {
        // Zip does not contain central directory, open with recovery option
        return MZ_FORMAT_ERROR;
    }

    /* Write the ZIP64 central directory header */
    if (zip->cd_offset >= UINT32_MAX || zip->number_entry >= UINT16_MAX) {
        zip64_eocd_pos_inzip = mz_stream_tell(zip->stream);

        err = mz_stream_write_uint32(zip->stream, MZ_ZIP_MAGIC_ENDHEADER64);

        /* Size of this 'zip64 end of central directory' */
        if (err == MZ_OK)
            err = mz_stream_write_uint64(zip->stream, (uint64_t)44);
        /* Version made by */
        if (err == MZ_OK)
            err = mz_stream_write_uint16(zip->stream, zip->version_madeby);
        /* Version needed */
        if (err == MZ_OK)
            err = mz_stream_write_uint16(zip->stream, (uint16_t)45);
        /* Number of this disk */
        if (err == MZ_OK)
            err = mz_stream_write_uint32(zip->stream, zip->disk_number_with_cd);
        /* Number of the disk with the start of the central directory */
        if (err == MZ_OK)
            err = mz_stream_write_uint32(zip->stream, zip->disk_number_with_cd);
        /* Total number of entries in the central dir on this disk */
        if (err == MZ_OK)
            err = mz_stream_write_uint64(zip->stream, zip->number_entry);
        /* Total number of entries in the central dir */
        if (err == MZ_OK)
            err = mz_stream_write_uint64(zip->stream, zip->number_entry);
        /* Size of the central directory */
        if (err == MZ_OK)
            err = mz_stream_write_int64(zip->stream, zip->cd_size);
        /* Offset of start of central directory with respect to the starting disk number */
        if (err == MZ_OK)
            err = mz_stream_write_int64(zip->stream, zip->cd_offset);
        if (err == MZ_OK)
            err = mz_stream_write_uint32(zip->stream, MZ_ZIP_MAGIC_ENDLOCHEADER64);

        /* Number of the disk with the start of the central directory */
        if (err == MZ_OK)
            err = mz_stream_write_uint32(zip->stream, zip->disk_number_with_cd);
        /* Relative offset to the end of zip64 central directory */
        if (err == MZ_OK)
            err = mz_stream_write_int64(zip->stream, zip64_eocd_pos_inzip);
        /* Number of the disk with the start of the central directory */
        if (err == MZ_OK)
            err = mz_stream_write_uint32(zip->stream, zip->disk_number_with_cd + 1);
    }

    /* Write the central directory header */

    /* Signature */
    if (err == MZ_OK)
        err = mz_stream_write_uint32(zip->stream, MZ_ZIP_MAGIC_ENDHEADER);
    /* Number of this disk */
    if (err == MZ_OK)
        err = mz_stream_write_uint16(zip->stream, (uint16_t)zip->disk_number_with_cd);
    /* Number of the disk with the start of the central directory */
    if (err == MZ_OK)
        err = mz_stream_write_uint16(zip->stream, (uint16_t)zip->disk_number_with_cd);
    /* Total number of entries in the central dir on this disk */
    if (err == MZ_OK) {
        if (zip->number_entry >= UINT16_MAX)
            err = mz_stream_write_uint16(zip->stream, UINT16_MAX);
        else
            err = mz_stream_write_uint16(zip->stream, (uint16_t)zip->number_entry);
    }
    /* Total number of entries in the central dir */
    if (err == MZ_OK) {
        if (zip->number_entry >= UINT16_MAX)
            err = mz_stream_write_uint16(zip->stream, UINT16_MAX);
        else
            err = mz_stream_write_uint16(zip->stream, (uint16_t)zip->number_entry);
    }
    /* Size of the central directory */
    if (err == MZ_OK)
        err = mz_stream_write_uint32(zip->stream, (uint32_t)zip->cd_size);
    /* Offset of start of central directory with respect to the starting disk number */
    if (err == MZ_OK) {
        if (zip->cd_offset >= UINT32_MAX)
            err = mz_stream_write_uint32(zip->stream, UINT32_MAX);
        else
            err = mz_stream_write_uint32(zip->stream, (uint32_t)zip->cd_offset);
    }

    /* Write global comment */
    if (zip->comment != NULL) {
        comment_size = (int32_t)strlen(zip->comment);
        if (comment_size > UINT16_MAX)
            comment_size = UINT16_MAX;
    }
    if (err == MZ_OK)
        err = mz_stream_write_uint16(zip->stream, (uint16_t)comment_size);
    if (err == MZ_OK) {
        if (mz_stream_write(zip->stream, zip->comment, comment_size) != comment_size)
            err = MZ_READ_ERROR;
    }
    return err;
}

static int32_t mz_zip_recover_cd(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    mz_zip_file local_file_info;
    void *local_file_info_stream = NULL;
    void *cd_mem_stream = NULL;
    uint64_t number_entry = 0;
    int64_t descriptor_pos = 0;
    int64_t next_header_pos = 0;
    int64_t disk_offset = 0;
    int64_t disk_number = 0;
    int64_t compressed_pos = 0;
    int64_t compressed_end_pos = 0;
    int64_t compressed_size = 0;
    int64_t uncompressed_size = 0;
    uint8_t descriptor_magic[4] = MZ_ZIP_MAGIC_DATADESCRIPTORU8;
    uint8_t local_header_magic[4] = MZ_ZIP_MAGIC_LOCALHEADERU8;
    uint8_t central_header_magic[4] = MZ_ZIP_MAGIC_CENTRALHEADERU8;
    uint32_t crc32 = 0;
    int32_t disk_number_with_cd = 0;
    int32_t err = MZ_OK;
    uint8_t zip64 = 0;
    uint8_t eof = 0;


    mz_zip_print("Zip - Recover - Start\n");

    mz_zip_get_cd_mem_stream(handle, &cd_mem_stream);

    /* Determine if we are on a split disk or not */
    mz_stream_set_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, 0);
    if (mz_stream_tell(zip->stream) < 0) {
        mz_stream_set_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, -1);
        mz_stream_seek(zip->stream, 0, MZ_SEEK_SET);
    } else
        disk_number_with_cd = 1;

    if (mz_stream_is_open(cd_mem_stream) != MZ_OK)
        err = mz_stream_mem_open(cd_mem_stream, NULL, MZ_OPEN_MODE_CREATE);

    mz_stream_mem_create(&local_file_info_stream);
    mz_stream_mem_open(local_file_info_stream, NULL, MZ_OPEN_MODE_CREATE);

    if (err == MZ_OK) {
        err = mz_stream_find(zip->stream, (const void *)local_header_magic, sizeof(local_header_magic),
                INT64_MAX, &next_header_pos);
    }

    while (err == MZ_OK && !eof) {
        /* Get current offset and disk number for central dir record */
        disk_offset = mz_stream_tell(zip->stream);
        mz_stream_get_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &disk_number);

        /* Read local headers */
        memset(&local_file_info, 0, sizeof(local_file_info));
        err = mz_zip_entry_read_header(zip->stream, 1, &local_file_info, local_file_info_stream);
        if (err != MZ_OK)
            break;

        local_file_info.disk_offset = disk_offset;
        if (disk_number < 0)
            disk_number = 0;
        local_file_info.disk_number = (uint32_t)disk_number;

        compressed_pos = mz_stream_tell(zip->stream);

        if ((err == MZ_OK) && (local_file_info.compressed_size > 0)) {
            mz_stream_seek(zip->stream, local_file_info.compressed_size, MZ_SEEK_CUR);
        }

        for (;;) {
            /* Search for the next local header */
            err = mz_stream_find(zip->stream, (const void *)local_header_magic, sizeof(local_header_magic),
                    INT64_MAX, &next_header_pos);

            if (err == MZ_EXIST_ERROR) {
                mz_stream_seek(zip->stream, compressed_pos, MZ_SEEK_SET);

                /* Search for central dir if no local header found */
                err = mz_stream_find(zip->stream, (const void *)central_header_magic, sizeof(central_header_magic),
                    INT64_MAX, &next_header_pos);

                if (err == MZ_EXIST_ERROR) {
                    /* Get end of stream if no central header found */
                    mz_stream_seek(zip->stream, 0, MZ_SEEK_END);
                    next_header_pos = mz_stream_tell(zip->stream);
                }

                eof = 1;
            }

            if (local_file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR || local_file_info.compressed_size == 0) {
                /* Search backwards for the descriptor, seeking too far back will be incorrect if compressed size is small */
                err = mz_stream_find_reverse(zip->stream, (const void *)descriptor_magic, sizeof(descriptor_magic),
                            MZ_ZIP_SIZE_MAX_DATA_DESCRIPTOR, &descriptor_pos);
                if (err == MZ_OK) {
                    if (mz_zip_extrafield_contains(local_file_info.extrafield,
                        local_file_info.extrafield_size, MZ_ZIP_EXTENSION_ZIP64, NULL) == MZ_OK)
                        zip64 = 1;

                    err = mz_zip_entry_read_descriptor(zip->stream, zip64, &crc32,
                        &compressed_size, &uncompressed_size);

                    if (err == MZ_OK) {
                        if (local_file_info.crc == 0)
                            local_file_info.crc = crc32;
                        if (local_file_info.compressed_size == 0)
                            local_file_info.compressed_size = compressed_size;
                        if (local_file_info.uncompressed_size == 0)
                            local_file_info.uncompressed_size = uncompressed_size;
                    }

                    compressed_end_pos = descriptor_pos;
                } else if (eof) {
                    compressed_end_pos = next_header_pos;
                } else if (local_file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR) {
                    /* Wrong local file entry found, keep searching */
                    next_header_pos += 1;
                    mz_stream_seek(zip->stream, next_header_pos, MZ_SEEK_SET);
                    continue;
                }
            } else {
                compressed_end_pos = next_header_pos;
            }

            break;
        }

        compressed_size = compressed_end_pos - compressed_pos;

        if (compressed_size > UINT32_MAX) {
            /* Update sizes if 4GB file is written with no ZIP64 support */
            if (local_file_info.uncompressed_size < UINT32_MAX) {
                local_file_info.compressed_size = compressed_size;
                local_file_info.uncompressed_size = 0;
            }
        }

        mz_zip_print("Zip - Recover - Entry %s (csize %" PRId64 " usize %" PRId64 " flags 0x%" PRIx16 ")\n",
            local_file_info.filename, local_file_info.compressed_size, local_file_info.uncompressed_size,
            local_file_info.flag);

        /* Rewrite central dir with local headers and offsets */
        err = mz_zip_entry_write_header(cd_mem_stream, 0, &local_file_info);
        if (err == MZ_OK)
            number_entry += 1;

        err = mz_stream_seek(zip->stream, next_header_pos, MZ_SEEK_SET);
    }

    mz_stream_mem_delete(&local_file_info_stream);

    mz_zip_print("Zip - Recover - Complete (cddisk %" PRId32 " entries %" PRId64 ")\n",
        disk_number_with_cd, number_entry);

    if (number_entry == 0)
        return err;

    /* Set new upper seek boundary for central dir mem stream */
    disk_offset = mz_stream_tell(cd_mem_stream);
    mz_stream_mem_set_buffer_limit(cd_mem_stream, (int32_t)disk_offset);

    /* Set new central directory info */
    mz_zip_set_cd_stream(handle, 0, cd_mem_stream);
    mz_zip_set_number_entry(handle, number_entry);
    mz_zip_set_disk_number_with_cd(handle, disk_number_with_cd);

    return MZ_OK;
}

void *mz_zip_create(void **handle) {
    mz_zip *zip = NULL;

    zip = (mz_zip *)MZ_ALLOC(sizeof(mz_zip));
    if (zip != NULL) {
        memset(zip, 0, sizeof(mz_zip));
        zip->data_descriptor = 1;
    }
    if (handle != NULL)
        *handle = zip;

    return zip;
}

void mz_zip_delete(void **handle) {
    mz_zip *zip = NULL;
    if (handle == NULL)
        return;
    zip = (mz_zip *)*handle;
    if (zip != NULL) {
        MZ_FREE(zip);
    }
    *handle = NULL;
}

int32_t mz_zip_open(void *handle, void *stream, int32_t mode) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;


    if (zip == NULL)
        return MZ_PARAM_ERROR;

    mz_zip_print("Zip - Open\n");

    zip->stream = stream;

    mz_stream_mem_create(&zip->cd_mem_stream);

    if (mode & MZ_OPEN_MODE_WRITE) {
        mz_stream_mem_open(zip->cd_mem_stream, NULL, MZ_OPEN_MODE_CREATE);
        zip->cd_stream = zip->cd_mem_stream;
    } else {
        zip->cd_stream = stream;
    }

    if ((mode & MZ_OPEN_MODE_READ) || (mode & MZ_OPEN_MODE_APPEND)) {
        if ((mode & MZ_OPEN_MODE_CREATE) == 0) {
            err = mz_zip_read_cd(zip);
            if (err != MZ_OK) {
                mz_zip_print("Zip - Error detected reading cd (%" PRId32 ")\n", err);
                if (zip->recover && mz_zip_recover_cd(zip) == MZ_OK)
                    err = MZ_OK;
            }
        }

        if ((err == MZ_OK) && (mode & MZ_OPEN_MODE_APPEND)) {
            if (zip->cd_size > 0) {
                /* Store central directory in memory */
                err = mz_stream_seek(zip->stream, zip->cd_offset, MZ_SEEK_SET);
                if (err == MZ_OK)
                    err = mz_stream_copy(zip->cd_mem_stream, zip->stream, (int32_t)zip->cd_size);
                if (err == MZ_OK)
                    err = mz_stream_seek(zip->stream, zip->cd_offset, MZ_SEEK_SET);
            } else {
                if (zip->cd_signature == MZ_ZIP_MAGIC_ENDHEADER) {
                    /* If tiny zip then overwrite end header */
                    err = mz_stream_seek(zip->stream, zip->cd_offset, MZ_SEEK_SET);
                } else {
                    /* If no central directory, append new zip to end of file */
                    err = mz_stream_seek(zip->stream, 0, MZ_SEEK_END);
                }
            }

            if (zip->disk_number_with_cd > 0) {
                /* Move to last disk to begin appending */
                mz_stream_set_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, zip->disk_number_with_cd - 1);
            }
        } else {
            zip->cd_start_pos = zip->cd_offset;
        }
    }

    if (err != MZ_OK) {
        mz_zip_close(zip);
        return err;
    }

    /* Memory streams used to store variable length file info data */
    mz_stream_mem_create(&zip->file_info_stream);
    mz_stream_mem_open(zip->file_info_stream, NULL, MZ_OPEN_MODE_CREATE);

    mz_stream_mem_create(&zip->local_file_info_stream);
    mz_stream_mem_open(zip->local_file_info_stream, NULL, MZ_OPEN_MODE_CREATE);

    zip->open_mode = mode;

    return err;
}

int32_t mz_zip_close(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    mz_zip_print("Zip - Close\n");

    if (mz_zip_entry_is_open(handle) == MZ_OK)
        err = mz_zip_entry_close(handle);

    if ((err == MZ_OK) && (zip->open_mode & MZ_OPEN_MODE_WRITE))
        err = mz_zip_write_cd(handle);

    if (zip->cd_mem_stream != NULL) {
        mz_stream_close(zip->cd_mem_stream);
        mz_stream_delete(&zip->cd_mem_stream);
    }

    if (zip->file_info_stream != NULL) {
        mz_stream_mem_close(zip->file_info_stream);
        mz_stream_mem_delete(&zip->file_info_stream);
    }
    if (zip->local_file_info_stream != NULL) {
        mz_stream_mem_close(zip->local_file_info_stream);
        mz_stream_mem_delete(&zip->local_file_info_stream);
    }

    if (zip->comment) {
        MZ_FREE(zip->comment);
        zip->comment = NULL;
    }

    zip->stream = NULL;
    zip->cd_stream = NULL;

    return err;
}

int32_t mz_zip_get_comment(void *handle, const char **comment) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || comment == NULL)
        return MZ_PARAM_ERROR;
    if (zip->comment == NULL)
        return MZ_EXIST_ERROR;
    *comment = zip->comment;
    return MZ_OK;
}

int32_t mz_zip_set_comment(void *handle, const char *comment) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t comment_size = 0;
    if (zip == NULL || comment == NULL)
        return MZ_PARAM_ERROR;
    if (zip->comment != NULL)
        MZ_FREE(zip->comment);
    comment_size = (int32_t)strlen(comment);
    if (comment_size > UINT16_MAX)
        return MZ_PARAM_ERROR;
    zip->comment = (char *)MZ_ALLOC(comment_size+1);
    if (zip->comment == NULL)
        return MZ_MEM_ERROR;
    memset(zip->comment, 0, comment_size+1);
    strncpy(zip->comment, comment, comment_size);
    return MZ_OK;
}

int32_t mz_zip_get_version_madeby(void *handle, uint16_t *version_madeby) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || version_madeby == NULL)
        return MZ_PARAM_ERROR;
    *version_madeby = zip->version_madeby;
    return MZ_OK;
}

int32_t mz_zip_set_version_madeby(void *handle, uint16_t version_madeby) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    zip->version_madeby = version_madeby;
    return MZ_OK;
}

int32_t mz_zip_set_recover(void *handle, uint8_t recover) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    zip->recover = recover;
    return MZ_OK;
}

int32_t mz_zip_set_data_descriptor(void *handle, uint8_t data_descriptor) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    zip->data_descriptor = data_descriptor;
    return MZ_OK;
}

int32_t mz_zip_get_stream(void *handle, void **stream) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || stream == NULL)
        return MZ_PARAM_ERROR;
    *stream = zip->stream;
    if (*stream == NULL)
        return MZ_EXIST_ERROR;
    return MZ_OK;
}

int32_t mz_zip_set_cd_stream(void *handle, int64_t cd_start_pos, void *cd_stream) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || cd_stream == NULL)
        return MZ_PARAM_ERROR;
    zip->cd_offset = 0;
    zip->cd_stream = cd_stream;
    zip->cd_start_pos = cd_start_pos;
    return MZ_OK;
}

int32_t mz_zip_get_cd_mem_stream(void *handle, void **cd_mem_stream) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || cd_mem_stream == NULL)
        return MZ_PARAM_ERROR;
    *cd_mem_stream = zip->cd_mem_stream;
    if (*cd_mem_stream == NULL)
        return MZ_EXIST_ERROR;
    return MZ_OK;
}

int32_t mz_zip_set_number_entry(void *handle, uint64_t number_entry) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    zip->number_entry = number_entry;
    return MZ_OK;
}

int32_t mz_zip_get_number_entry(void *handle, uint64_t *number_entry) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || number_entry == NULL)
        return MZ_PARAM_ERROR;
    *number_entry = zip->number_entry;
    return MZ_OK;
}

int32_t mz_zip_set_disk_number_with_cd(void *handle, uint32_t disk_number_with_cd) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    zip->disk_number_with_cd = disk_number_with_cd;
    return MZ_OK;
}

int32_t mz_zip_get_disk_number_with_cd(void *handle, uint32_t *disk_number_with_cd) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || disk_number_with_cd == NULL)
        return MZ_PARAM_ERROR;
    *disk_number_with_cd = zip->disk_number_with_cd;
    return MZ_OK;
}

static int32_t mz_zip_entry_close_int(void *handle) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip->crypt_stream != NULL)
        mz_stream_delete(&zip->crypt_stream);
    zip->crypt_stream = NULL;
    if (zip->compress_stream != NULL)
        mz_stream_delete(&zip->compress_stream);
    zip->compress_stream = NULL;

    zip->entry_opened = 0;

    return MZ_OK;
}

static int32_t mz_zip_entry_open_int(void *handle, uint8_t raw, int16_t compress_level, const char *password) {
    mz_zip *zip = (mz_zip *)handle;
    int64_t max_total_in = 0;
    int64_t header_size = 0;
    int64_t footer_size = 0;
    int32_t err = MZ_OK;
    uint8_t use_crypt = 0;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    switch (zip->file_info.compression_method) {
    case MZ_COMPRESS_METHOD_STORE:
    case MZ_COMPRESS_METHOD_DEFLATE:
#ifdef HAVE_BZIP2
    case MZ_COMPRESS_METHOD_BZIP2:
#endif
#ifdef HAVE_LZMA
    case MZ_COMPRESS_METHOD_LZMA:
#endif
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
    case MZ_COMPRESS_METHOD_XZ:
#endif
#ifdef HAVE_ZSTD
    case MZ_COMPRESS_METHOD_ZSTD:
#endif
        err = MZ_OK;
        break;
    default:
        return MZ_SUPPORT_ERROR;
    }

#ifndef HAVE_WZAES
    if (zip->file_info.aes_version)
        return MZ_SUPPORT_ERROR;
#endif

    zip->entry_raw = raw;

    if ((zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED) && (password != NULL)) {
        if (zip->open_mode & MZ_OPEN_MODE_WRITE) {
            /* Encrypt only when we are not trying to write raw and password is supplied. */
            if (!zip->entry_raw)
                use_crypt = 1;
        } else if (zip->open_mode & MZ_OPEN_MODE_READ) {
            /* Decrypt only when password is supplied. Don't error when password */
            /* is not supplied as we may want to read the raw encrypted data. */
            use_crypt = 1;
        }
    }

    if ((err == MZ_OK) && (use_crypt)) {
#ifdef HAVE_WZAES
        if (zip->file_info.aes_version) {
            mz_stream_wzaes_create(&zip->crypt_stream);
            mz_stream_wzaes_set_password(zip->crypt_stream, password);
            mz_stream_wzaes_set_encryption_mode(zip->crypt_stream, zip->file_info.aes_encryption_mode);
        } else
#endif
        {
#ifdef HAVE_PKCRYPT
            uint8_t verify1 = (uint8_t)((zip->file_info.pk_verify >> 8) & 0xff);
            uint8_t verify2 = (uint8_t)((zip->file_info.pk_verify) & 0xff);

            mz_stream_pkcrypt_create(&zip->crypt_stream);
            mz_stream_pkcrypt_set_password(zip->crypt_stream, password);
            mz_stream_pkcrypt_set_verify(zip->crypt_stream, verify1, verify2);
#endif
        }
    }

    if (err == MZ_OK) {
        if (zip->crypt_stream == NULL)
            mz_stream_raw_create(&zip->crypt_stream);

        mz_stream_set_base(zip->crypt_stream, zip->stream);

        err = mz_stream_open(zip->crypt_stream, NULL, zip->open_mode);
    }

    if (err == MZ_OK) {
        if (zip->entry_raw || zip->file_info.compression_method == MZ_COMPRESS_METHOD_STORE)
            mz_stream_raw_create(&zip->compress_stream);
#ifdef HAVE_ZLIB
        else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_DEFLATE)
            mz_stream_zlib_create(&zip->compress_stream);
#endif
#ifdef HAVE_BZIP2
        else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_BZIP2)
            mz_stream_bzip_create(&zip->compress_stream);
#endif
#ifdef HAVE_LIBCOMP
        else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_DEFLATE ||
                 zip->file_info.compression_method == MZ_COMPRESS_METHOD_XZ) {
            mz_stream_libcomp_create(&zip->compress_stream);
            mz_stream_set_prop_int64(zip->compress_stream, MZ_STREAM_PROP_COMPRESS_METHOD,
                zip->file_info.compression_method);
        }
#endif
#ifdef HAVE_LZMA
        else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_LZMA ||
                 zip->file_info.compression_method == MZ_COMPRESS_METHOD_XZ) {
            mz_stream_lzma_create(&zip->compress_stream);
            mz_stream_set_prop_int64(zip->compress_stream, MZ_STREAM_PROP_COMPRESS_METHOD,
                zip->file_info.compression_method);
        }
#endif
#ifdef HAVE_ZSTD
        else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_ZSTD)
            mz_stream_zstd_create(&zip->compress_stream);
#endif
        else
            err = MZ_PARAM_ERROR;
    }

    if (err == MZ_OK) {
        if (zip->open_mode & MZ_OPEN_MODE_WRITE) {
            mz_stream_set_prop_int64(zip->compress_stream, MZ_STREAM_PROP_COMPRESS_LEVEL, compress_level);
        } else {
            int32_t set_end_of_stream = 0;

#ifndef HAVE_LIBCOMP
            if (zip->entry_raw ||
                zip->file_info.compression_method == MZ_COMPRESS_METHOD_STORE ||
                zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED)
#endif
            {
                max_total_in = zip->file_info.compressed_size;
                mz_stream_set_prop_int64(zip->crypt_stream, MZ_STREAM_PROP_TOTAL_IN_MAX, max_total_in);

                if (mz_stream_get_prop_int64(zip->crypt_stream, MZ_STREAM_PROP_HEADER_SIZE, &header_size) == MZ_OK)
                    max_total_in -= header_size;
                if (mz_stream_get_prop_int64(zip->crypt_stream, MZ_STREAM_PROP_FOOTER_SIZE, &footer_size) == MZ_OK)
                    max_total_in -= footer_size;

                mz_stream_set_prop_int64(zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN_MAX, max_total_in);
            }

            switch (zip->file_info.compression_method) {
            case MZ_COMPRESS_METHOD_LZMA:
            case MZ_COMPRESS_METHOD_XZ:
                set_end_of_stream = (zip->file_info.flag & MZ_ZIP_FLAG_LZMA_EOS_MARKER);
                break;
            case MZ_COMPRESS_METHOD_ZSTD:
                set_end_of_stream = 1;
                break;
            }

            if (set_end_of_stream) {
                mz_stream_set_prop_int64(zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN_MAX, zip->file_info.compressed_size);
                mz_stream_set_prop_int64(zip->compress_stream, MZ_STREAM_PROP_TOTAL_OUT_MAX, zip->file_info.uncompressed_size);
            }
        }

        mz_stream_set_base(zip->compress_stream, zip->crypt_stream);

        err = mz_stream_open(zip->compress_stream, NULL, zip->open_mode);
    }

    if (err == MZ_OK) {
        zip->entry_opened = 1;
        zip->entry_crc32 = 0;
    } else {
        mz_zip_entry_close_int(handle);
    }

    return err;
}

int32_t mz_zip_entry_is_open(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    if (zip->entry_opened == 0)
        return MZ_EXIST_ERROR;
    return MZ_OK;
}

int32_t mz_zip_entry_read_open(void *handle, uint8_t raw, const char *password) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;
    int32_t err_shift = MZ_OK;

#if defined(MZ_ZIP_NO_ENCRYPTION)
    if (password != NULL)
        return MZ_SUPPORT_ERROR;
#endif
    if (zip == NULL)
        return MZ_PARAM_ERROR;
    if ((zip->open_mode & MZ_OPEN_MODE_READ) == 0)
        return MZ_PARAM_ERROR;
    if (zip->entry_scanned == 0)
        return MZ_PARAM_ERROR;

    mz_zip_print("Zip - Entry - Read open (raw %" PRId32 ")\n", raw);

    err = mz_zip_entry_seek_local_header(handle);
    if (err == MZ_OK)
        err = mz_zip_entry_read_header(zip->stream, 1, &zip->local_file_info, zip->local_file_info_stream);

    if (err == MZ_FORMAT_ERROR && zip->disk_offset_shift > 0) {
        /* Perhaps we didn't compensated correctly for incorrect cd offset */
        err_shift = mz_stream_seek(zip->stream, zip->file_info.disk_offset, MZ_SEEK_SET);
        if (err_shift == MZ_OK)
            err_shift = mz_zip_entry_read_header(zip->stream, 1, &zip->local_file_info, zip->local_file_info_stream);
        if (err_shift == MZ_OK) {
            zip->disk_offset_shift = 0;
            err = err_shift;
        }
    }

#ifdef MZ_ZIP_NO_DECOMPRESSION
    if (!raw && zip->file_info.compression_method != MZ_COMPRESS_METHOD_STORE)
        err = MZ_SUPPORT_ERROR;
#endif
    if (err == MZ_OK)
        err = mz_zip_entry_open_int(handle, raw, 0, password);

    return err;
}

int32_t mz_zip_entry_write_open(void *handle, const mz_zip_file *file_info, int16_t compress_level, uint8_t raw, const char *password) {
    mz_zip *zip = (mz_zip *)handle;
    int64_t filename_pos = -1;
    int64_t extrafield_pos = 0;
    int64_t comment_pos = 0;
    int64_t linkname_pos = 0;
    int64_t disk_number = 0;
    uint8_t is_dir = 0;
    int32_t err = MZ_OK;

#if defined(MZ_ZIP_NO_ENCRYPTION)
    if (password != NULL)
        return MZ_SUPPORT_ERROR;
#endif
    if (zip == NULL || file_info == NULL || file_info->filename == NULL)
        return MZ_PARAM_ERROR;

    if (mz_zip_entry_is_open(handle) == MZ_OK) {
        err = mz_zip_entry_close(handle);
        if (err != MZ_OK)
            return err;
    }

    memcpy(&zip->file_info, file_info, sizeof(mz_zip_file));

    mz_zip_print("Zip - Entry - Write open - %s (level %" PRId16 " raw %" PRId8 ")\n",
        zip->file_info.filename, compress_level, raw);

    mz_stream_seek(zip->file_info_stream, 0, MZ_SEEK_SET);
    mz_stream_write(zip->file_info_stream, file_info, sizeof(mz_zip_file));

    /* Copy filename, extrafield, and comment internally */
    filename_pos = mz_stream_tell(zip->file_info_stream);
    if (file_info->filename != NULL)
        mz_stream_write(zip->file_info_stream, file_info->filename, (int32_t)strlen(file_info->filename));
    mz_stream_write_uint8(zip->file_info_stream, 0);

    extrafield_pos = mz_stream_tell(zip->file_info_stream);
    if (file_info->extrafield != NULL)
        mz_stream_write(zip->file_info_stream, file_info->extrafield, file_info->extrafield_size);
    mz_stream_write_uint8(zip->file_info_stream, 0);

    comment_pos = mz_stream_tell(zip->file_info_stream);
    if (file_info->comment != NULL)
        mz_stream_write(zip->file_info_stream, file_info->comment, file_info->comment_size);
    mz_stream_write_uint8(zip->file_info_stream, 0);

    linkname_pos = mz_stream_tell(zip->file_info_stream);
    if (file_info->linkname != NULL)
        mz_stream_write(zip->file_info_stream, file_info->linkname, (int32_t)strlen(file_info->linkname));
    mz_stream_write_uint8(zip->file_info_stream, 0);

    mz_stream_mem_get_buffer_at(zip->file_info_stream, filename_pos, (const void **)&zip->file_info.filename);
    mz_stream_mem_get_buffer_at(zip->file_info_stream, extrafield_pos, (const void **)&zip->file_info.extrafield);
    mz_stream_mem_get_buffer_at(zip->file_info_stream, comment_pos, (const void **)&zip->file_info.comment);
    mz_stream_mem_get_buffer_at(zip->file_info_stream, linkname_pos, (const void **)&zip->file_info.linkname);

    if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_DEFLATE) {
        if ((compress_level == 8) || (compress_level == 9))
            zip->file_info.flag |= MZ_ZIP_FLAG_DEFLATE_MAX;
        if (compress_level == 2)
            zip->file_info.flag |= MZ_ZIP_FLAG_DEFLATE_FAST;
        if (compress_level == 1)
            zip->file_info.flag |= MZ_ZIP_FLAG_DEFLATE_SUPER_FAST;
    }
#if defined(HAVE_LZMA) || defined(HAVE_LIBCOMP)
    else if (zip->file_info.compression_method == MZ_COMPRESS_METHOD_LZMA ||
             zip->file_info.compression_method == MZ_COMPRESS_METHOD_XZ)
        zip->file_info.flag |= MZ_ZIP_FLAG_LZMA_EOS_MARKER;
#endif

    if (mz_zip_attrib_is_dir(zip->file_info.external_fa, zip->file_info.version_madeby) == MZ_OK)
        is_dir = 1;

    if (!is_dir) {
        if (zip->data_descriptor)
            zip->file_info.flag |= MZ_ZIP_FLAG_DATA_DESCRIPTOR;
        if (password != NULL)
            zip->file_info.flag |= MZ_ZIP_FLAG_ENCRYPTED;
    }

    mz_stream_get_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &disk_number);
    zip->file_info.disk_number = (uint32_t)disk_number;
    zip->file_info.disk_offset = mz_stream_tell(zip->stream);

    if (zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED) {
#ifdef HAVE_PKCRYPT
        /* Pre-calculated CRC value is required for PKWARE traditional encryption */
        uint32_t dos_date = mz_zip_time_t_to_dos_date(zip->file_info.modified_date);
        zip->file_info.pk_verify = mz_zip_get_pk_verify(dos_date, zip->file_info.crc, zip->file_info.flag);
#endif
#ifdef HAVE_WZAES
        if (zip->file_info.aes_version && zip->file_info.aes_encryption_mode == 0)
            zip->file_info.aes_encryption_mode = MZ_AES_ENCRYPTION_MODE_256;
#endif
    }

    zip->file_info.crc = 0;
    zip->file_info.compressed_size = 0;

    if ((compress_level == 0) || (is_dir))
        zip->file_info.compression_method = MZ_COMPRESS_METHOD_STORE;

#ifdef MZ_ZIP_NO_COMPRESSION
    if (zip->file_info.compression_method != MZ_COMPRESS_METHOD_STORE)
        err = MZ_SUPPORT_ERROR;
#endif
    if (err == MZ_OK)
        err = mz_zip_entry_write_header(zip->stream, 1, &zip->file_info);
    if (err == MZ_OK)
        err = mz_zip_entry_open_int(handle, raw, compress_level, password);

    return err;
}

int32_t mz_zip_entry_read(void *handle, void *buf, int32_t len) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t read = 0;

    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    if (UINT_MAX == UINT16_MAX && len > UINT16_MAX) /* zlib limitation */
        return MZ_PARAM_ERROR;
    if (len == 0)
        return MZ_PARAM_ERROR;

    if (zip->file_info.compressed_size == 0)
        return 0;

    /* Read entire entry even if uncompressed_size = 0, otherwise */
    /* aes encryption validation will fail if compressed_size > 0 */
    read = mz_stream_read(zip->compress_stream, buf, len);
    if (read > 0)
        zip->entry_crc32 = mz_crypt_crc32_update(zip->entry_crc32, buf, read);

    mz_zip_print("Zip - Entry - Read - %" PRId32 " (max %" PRId32 ")\n", read, len);

    return read;
}

int32_t mz_zip_entry_write(void *handle, const void *buf, int32_t len) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t written = 0;

    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    written = mz_stream_write(zip->compress_stream, buf, len);
    if (written > 0)
        zip->entry_crc32 = mz_crypt_crc32_update(zip->entry_crc32, buf, written);

    mz_zip_print("Zip - Entry - Write - %" PRId32 " (max %" PRId32 ")\n", written, len);

    return written;
}

int32_t mz_zip_entry_read_close(void *handle, uint32_t *crc32, int64_t *compressed_size,
    int64_t *uncompressed_size) {
    mz_zip *zip = (mz_zip *)handle;
    int64_t total_in = 0;
    int32_t err = MZ_OK;
    uint8_t zip64 = 0;

    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    mz_stream_close(zip->compress_stream);

    mz_zip_print("Zip - Entry - Read Close\n");

    if (crc32 != NULL)
        *crc32 = zip->file_info.crc;
    if (compressed_size != NULL)
        *compressed_size = zip->file_info.compressed_size;
    if (uncompressed_size != NULL)
        *uncompressed_size = zip->file_info.uncompressed_size;

    mz_stream_get_prop_int64(zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN, &total_in);

    if ((zip->file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR) &&
        ((zip->file_info.flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO) == 0) &&
        (crc32 != NULL || compressed_size != NULL || uncompressed_size != NULL)) {
        /* Check to see if data descriptor is zip64 bit format or not */
        if (mz_zip_extrafield_contains(zip->local_file_info.extrafield,
            zip->local_file_info.extrafield_size, MZ_ZIP_EXTENSION_ZIP64, NULL) == MZ_OK)
            zip64 = 1;

        err = mz_zip_entry_seek_local_header(handle);

        /* Seek to end of compressed stream since we might have over-read during compression */
        if (err == MZ_OK)
            err = mz_stream_seek(zip->stream, MZ_ZIP_SIZE_LD_ITEM +
                (int64_t)zip->local_file_info.filename_size +
                (int64_t)zip->local_file_info.extrafield_size +
                total_in, MZ_SEEK_CUR);

        /* Read data descriptor */
        if (err == MZ_OK)
            err = mz_zip_entry_read_descriptor(zip->stream, zip64,
                crc32, compressed_size, uncompressed_size);
    }

    /* If entire entry was not read verification will fail */
    if ((err == MZ_OK) && (total_in > 0) && (!zip->entry_raw)) {
#ifdef HAVE_WZAES
        /* AES zip version AE-1 will expect a valid crc as well */
        if (zip->file_info.aes_version <= 0x0001)
#endif
        {
            if (zip->entry_crc32 != zip->file_info.crc) {
                mz_zip_print("Zip - Entry - Crc failed (actual 0x%08" PRIx32 " expected 0x%08" PRIx32 ")\n",
                    zip->entry_crc32, zip->file_info.crc);

                err = MZ_CRC_ERROR;
            }
        }
    }

    mz_zip_entry_close_int(handle);

    return err;
}

int32_t mz_zip_entry_write_close(void *handle, uint32_t crc32, int64_t compressed_size,
    int64_t uncompressed_size) {
    mz_zip *zip = (mz_zip *)handle;
    int64_t end_disk_number = 0;
    int32_t err = MZ_OK;
    uint8_t zip64 = 0;

    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    mz_stream_close(zip->compress_stream);

    if (!zip->entry_raw)
        crc32 = zip->entry_crc32;

    mz_zip_print("Zip - Entry - Write Close (crc 0x%08" PRIx32 " cs %" PRId64 " ucs %" PRId64 ")\n",
        crc32, compressed_size, uncompressed_size);

    /* If sizes are not set, then read them from the compression stream */
    if (compressed_size < 0)
        mz_stream_get_prop_int64(zip->compress_stream, MZ_STREAM_PROP_TOTAL_OUT, &compressed_size);
    if (uncompressed_size < 0)
        mz_stream_get_prop_int64(zip->compress_stream, MZ_STREAM_PROP_TOTAL_IN, &uncompressed_size);

    if (zip->file_info.flag & MZ_ZIP_FLAG_ENCRYPTED) {
        mz_stream_set_base(zip->crypt_stream, zip->stream);
        err = mz_stream_close(zip->crypt_stream);

        mz_stream_get_prop_int64(zip->crypt_stream, MZ_STREAM_PROP_TOTAL_OUT, &compressed_size);
    }

    mz_zip_entry_needs_zip64(&zip->file_info, 1, &zip64);

    if ((err == MZ_OK) && (zip->file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR)) {
        /* Determine if we need to write data descriptor in zip64 format,
           if local extrafield was saved with zip64 extrafield */

        if (zip->file_info.flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO)
            err = mz_zip_entry_write_descriptor(zip->stream,
                zip64, 0, compressed_size, 0);
        else
            err = mz_zip_entry_write_descriptor(zip->stream,
                zip64, crc32, compressed_size, uncompressed_size);
    }

    /* Write file info to central directory */

    mz_zip_print("Zip - Entry - Write cd (ucs %" PRId64 " cs %" PRId64 " crc 0x%08" PRIx32 ")\n",
        uncompressed_size, compressed_size, crc32);

    zip->file_info.crc = crc32;
    zip->file_info.compressed_size = compressed_size;
    zip->file_info.uncompressed_size = uncompressed_size;

    if (err == MZ_OK)
        err = mz_zip_entry_write_header(zip->cd_mem_stream, 0, &zip->file_info);

    /* Update local header with crc32 and sizes */
    if ((err == MZ_OK) && ((zip->file_info.flag & MZ_ZIP_FLAG_DATA_DESCRIPTOR) == 0) &&
        ((zip->file_info.flag & MZ_ZIP_FLAG_MASK_LOCAL_INFO) == 0)) {
        /* Save the disk number and position we are to seek back after updating local header */
        int64_t end_pos = mz_stream_tell(zip->stream);
        mz_stream_get_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, &end_disk_number);

        err = mz_zip_entry_seek_local_header(handle);

        if (err == MZ_OK) {
            /* Seek to crc32 and sizes offset in local header */
            err = mz_stream_seek(zip->stream, MZ_ZIP_OFFSET_CRC_SIZES, MZ_SEEK_CUR);
        }

        if (err == MZ_OK)
            err = mz_zip_entry_write_crc_sizes(zip->stream, zip64, 0, &zip->file_info);

        /* Seek to and update zip64 extension sizes */
        if ((err == MZ_OK) && (zip64)) {
            int64_t filename_size = zip->file_info.filename_size;

            if (filename_size == 0)
                filename_size = strlen(zip->file_info.filename);

            /* Since we write zip64 extension first we know its offset */
            err = mz_stream_seek(zip->stream, 2 + 2 + filename_size + 4, MZ_SEEK_CUR);

            if (err == MZ_OK)
                err = mz_stream_write_uint64(zip->stream, zip->file_info.uncompressed_size);
            if (err == MZ_OK)
                err = mz_stream_write_uint64(zip->stream, zip->file_info.compressed_size);
        }

        mz_stream_set_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, end_disk_number);
        mz_stream_seek(zip->stream, end_pos, MZ_SEEK_SET);
    }

    zip->number_entry += 1;

    mz_zip_entry_close_int(handle);

    return err;
}

int32_t mz_zip_entry_seek_local_header(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    int64_t disk_size = 0;
    uint32_t disk_number = zip->file_info.disk_number;

    if (disk_number == zip->disk_number_with_cd) {
        mz_stream_get_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_SIZE, &disk_size);
        if ((disk_size == 0) || ((zip->open_mode & MZ_OPEN_MODE_WRITE) == 0))
            disk_number = (uint32_t)-1;
    }

    mz_stream_set_prop_int64(zip->stream, MZ_STREAM_PROP_DISK_NUMBER, disk_number);

    mz_zip_print("Zip - Entry - Seek local (disk %" PRId32 " offset %" PRId64 ")\n",
        disk_number, zip->file_info.disk_offset);

    /* Guard against seek overflows */
    if ((zip->disk_offset_shift > 0) &&
        (zip->file_info.disk_offset > (INT64_MAX - zip->disk_offset_shift)))
        return MZ_FORMAT_ERROR;

    return mz_stream_seek(zip->stream, zip->file_info.disk_offset + zip->disk_offset_shift, MZ_SEEK_SET);
}

int32_t mz_zip_entry_close(void *handle) {
    return mz_zip_entry_close_raw(handle, UINT64_MAX, 0);
}

int32_t mz_zip_entry_close_raw(void *handle, int64_t uncompressed_size, uint32_t crc32) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;

    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    if (zip->open_mode & MZ_OPEN_MODE_WRITE)
        err = mz_zip_entry_write_close(handle, crc32, UINT64_MAX, uncompressed_size);
    else
        err = mz_zip_entry_read_close(handle, NULL, NULL, NULL);

    return err;
}

int32_t mz_zip_entry_is_dir(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t filename_length = 0;

    if (zip == NULL)
        return MZ_PARAM_ERROR;
    if (zip->entry_scanned == 0)
        return MZ_PARAM_ERROR;
    if (mz_zip_attrib_is_dir(zip->file_info.external_fa, zip->file_info.version_madeby) == MZ_OK)
        return MZ_OK;

    filename_length = (int32_t)strlen(zip->file_info.filename);
    if (filename_length > 0) {
        if ((zip->file_info.filename[filename_length - 1] == '/') ||
            (zip->file_info.filename[filename_length - 1] == '\\'))
            return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}

int32_t mz_zip_entry_is_symlink(void *handle) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL)
        return MZ_PARAM_ERROR;
    if (zip->entry_scanned == 0)
        return MZ_PARAM_ERROR;
    if (mz_zip_attrib_is_symlink(zip->file_info.external_fa, zip->file_info.version_madeby) != MZ_OK)
        return MZ_EXIST_ERROR;
    if (zip->file_info.linkname == NULL || *zip->file_info.linkname == 0)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_zip_entry_get_info(void *handle, mz_zip_file **file_info) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    if ((zip->open_mode & MZ_OPEN_MODE_WRITE) == 0) {
        if (!zip->entry_scanned)
            return MZ_PARAM_ERROR;
    }

    *file_info = &zip->file_info;
    return MZ_OK;
}

int32_t mz_zip_entry_get_local_info(void *handle, mz_zip_file **local_file_info) {
    mz_zip *zip = (mz_zip *)handle;
    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;
    *local_file_info = &zip->local_file_info;
    return MZ_OK;
}

int32_t mz_zip_entry_set_extrafield(void *handle, const uint8_t *extrafield, uint16_t extrafield_size) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL || mz_zip_entry_is_open(handle) != MZ_OK)
        return MZ_PARAM_ERROR;

    zip->file_info.extrafield = extrafield;
    zip->file_info.extrafield_size = extrafield_size;
    return MZ_OK;
}

static int32_t mz_zip_goto_next_entry_int(void *handle) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    zip->entry_scanned = 0;

    mz_stream_set_prop_int64(zip->cd_stream, MZ_STREAM_PROP_DISK_NUMBER, -1);

    err = mz_stream_seek(zip->cd_stream, zip->cd_current_pos, MZ_SEEK_SET);
    if (err == MZ_OK)
        err = mz_zip_entry_read_header(zip->cd_stream, 0, &zip->file_info, zip->file_info_stream);
    if (err == MZ_OK)
        zip->entry_scanned = 1;
    return err;
}

int64_t mz_zip_get_entry(void *handle) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    return zip->cd_current_pos;
}

int32_t mz_zip_goto_entry(void *handle, int64_t cd_pos) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    if (cd_pos < zip->cd_start_pos || cd_pos > zip->cd_start_pos + zip->cd_size)
        return MZ_PARAM_ERROR;

    zip->cd_current_pos = cd_pos;

    return mz_zip_goto_next_entry_int(handle);
}

int32_t mz_zip_goto_first_entry(void *handle) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    zip->cd_current_pos = zip->cd_start_pos;

    return mz_zip_goto_next_entry_int(handle);
}

int32_t mz_zip_goto_next_entry(void *handle) {
    mz_zip *zip = (mz_zip *)handle;

    if (zip == NULL)
        return MZ_PARAM_ERROR;

    zip->cd_current_pos += (int64_t)MZ_ZIP_SIZE_CD_ITEM + zip->file_info.filename_size +
        zip->file_info.extrafield_size + zip->file_info.comment_size;

    return mz_zip_goto_next_entry_int(handle);
}

int32_t mz_zip_locate_entry(void *handle, const char *filename, uint8_t ignore_case) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;
    int32_t result = 0;

    if (zip == NULL || filename == NULL)
        return MZ_PARAM_ERROR;

    /* If we are already on the current entry, no need to search */
    if ((zip->entry_scanned) && (zip->file_info.filename != NULL)) {
        result = mz_zip_path_compare(zip->file_info.filename, filename, ignore_case);
        if (result == 0)
            return MZ_OK;
    }

    /* Search all entries starting at the first */
    err = mz_zip_goto_first_entry(handle);
    while (err == MZ_OK) {
        result = mz_zip_path_compare(zip->file_info.filename, filename, ignore_case);
        if (result == 0)
            return MZ_OK;

        err = mz_zip_goto_next_entry(handle);
    }

    return err;
}

int32_t mz_zip_locate_first_entry(void *handle, void *userdata, mz_zip_locate_entry_cb cb) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;
    int32_t result = 0;

    /* Search first entry looking for match */
    err = mz_zip_goto_first_entry(handle);
    if (err != MZ_OK)
        return err;

    result = cb(handle, userdata, &zip->file_info);
    if (result == 0)
        return MZ_OK;

    return mz_zip_locate_next_entry(handle, userdata, cb);
}

int32_t mz_zip_locate_next_entry(void *handle, void *userdata, mz_zip_locate_entry_cb cb) {
    mz_zip *zip = (mz_zip *)handle;
    int32_t err = MZ_OK;
    int32_t result = 0;

    /* Search next entries looking for match */
    err = mz_zip_goto_next_entry(handle);
    while (err == MZ_OK) {
        result = cb(handle, userdata, &zip->file_info);
        if (result == 0)
            return MZ_OK;

        err = mz_zip_goto_next_entry(handle);
    }

    return err;
}

/***************************************************************************/

int32_t mz_zip_attrib_is_dir(uint32_t attrib, int32_t version_madeby) {
    uint32_t posix_attrib = 0;
    uint8_t system = MZ_HOST_SYSTEM(version_madeby);
    int32_t err = MZ_OK;

    err = mz_zip_attrib_convert(system, attrib, MZ_HOST_SYSTEM_UNIX, &posix_attrib);
    if (err == MZ_OK) {
        if ((posix_attrib & 0170000) == 0040000) /* S_ISDIR */
            return MZ_OK;
    }

    return MZ_EXIST_ERROR;
}

int32_t mz_zip_attrib_is_symlink(uint32_t attrib, int32_t version_madeby) {
    uint32_t posix_attrib = 0;
    uint8_t system = MZ_HOST_SYSTEM(version_madeby);
    int32_t err = MZ_OK;

    err = mz_zip_attrib_convert(system, attrib, MZ_HOST_SYSTEM_UNIX, &posix_attrib);
    if (err == MZ_OK) {
        if ((posix_attrib & 0170000) == 0120000) /* S_ISLNK */
            return MZ_OK;
    }

    return MZ_EXIST_ERROR;
}

int32_t mz_zip_attrib_convert(uint8_t src_sys, uint32_t src_attrib, uint8_t target_sys, uint32_t *target_attrib) {
    if (target_attrib == NULL)
        return MZ_PARAM_ERROR;

    *target_attrib = 0;

    if ((src_sys == MZ_HOST_SYSTEM_MSDOS) || (src_sys == MZ_HOST_SYSTEM_WINDOWS_NTFS)) {
        if ((target_sys == MZ_HOST_SYSTEM_MSDOS) || (target_sys == MZ_HOST_SYSTEM_WINDOWS_NTFS)) {
            *target_attrib = src_attrib;
            return MZ_OK;
        }
        if ((target_sys == MZ_HOST_SYSTEM_UNIX) || (target_sys == MZ_HOST_SYSTEM_OSX_DARWIN) || (target_sys == MZ_HOST_SYSTEM_RISCOS))
            return mz_zip_attrib_win32_to_posix(src_attrib, target_attrib);
    } else if ((src_sys == MZ_HOST_SYSTEM_UNIX) || (src_sys == MZ_HOST_SYSTEM_OSX_DARWIN) || (src_sys == MZ_HOST_SYSTEM_RISCOS)) {
        if ((target_sys == MZ_HOST_SYSTEM_UNIX) || (target_sys == MZ_HOST_SYSTEM_OSX_DARWIN) || (target_sys == MZ_HOST_SYSTEM_RISCOS)) {
            /* If high bytes are set, it contains unix specific attributes */
            if ((src_attrib >> 16) != 0)
                src_attrib >>= 16;

            *target_attrib = src_attrib;
            return MZ_OK;
        }
        if ((target_sys == MZ_HOST_SYSTEM_MSDOS) || (target_sys == MZ_HOST_SYSTEM_WINDOWS_NTFS))
            return mz_zip_attrib_posix_to_win32(src_attrib, target_attrib);
    }

    return MZ_SUPPORT_ERROR;
}

int32_t mz_zip_attrib_posix_to_win32(uint32_t posix_attrib, uint32_t *win32_attrib) {
    if (win32_attrib == NULL)
        return MZ_PARAM_ERROR;

    *win32_attrib = 0;

    /* S_IWUSR | S_IWGRP | S_IWOTH | S_IXUSR | S_IXGRP | S_IXOTH */
    if ((posix_attrib & 0000333) == 0 && (posix_attrib & 0000444) != 0)
        *win32_attrib |= 0x01;      /* FILE_ATTRIBUTE_READONLY */
    /* S_IFLNK */
    if ((posix_attrib & 0170000) == 0120000)
        *win32_attrib |= 0x400;     /* FILE_ATTRIBUTE_REPARSE_POINT */
    /* S_IFDIR */
    else if ((posix_attrib & 0170000) == 0040000)
        *win32_attrib |= 0x10;      /* FILE_ATTRIBUTE_DIRECTORY */
    /* S_IFREG */
    else
        *win32_attrib |= 0x80;      /* FILE_ATTRIBUTE_NORMAL */

    return MZ_OK;
}

int32_t mz_zip_attrib_win32_to_posix(uint32_t win32_attrib, uint32_t *posix_attrib) {
    if (posix_attrib == NULL)
        return MZ_PARAM_ERROR;

    *posix_attrib = 0000444;        /* S_IRUSR | S_IRGRP | S_IROTH */
    /* FILE_ATTRIBUTE_READONLY */
    if ((win32_attrib & 0x01) == 0)
        *posix_attrib |= 0000222;   /* S_IWUSR | S_IWGRP | S_IWOTH */
    /* FILE_ATTRIBUTE_REPARSE_POINT */
    if ((win32_attrib & 0x400) == 0x400)
        *posix_attrib |= 0120000;   /* S_IFLNK */
    /* FILE_ATTRIBUTE_DIRECTORY */
    else if ((win32_attrib & 0x10) == 0x10)
        *posix_attrib |= 0040111;   /* S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH */
    else
        *posix_attrib |= 0100000;   /* S_IFREG */

    return MZ_OK;
}

/***************************************************************************/

int32_t mz_zip_extrafield_find(void *stream, uint16_t type, int32_t max_seek, uint16_t *length) {
    int32_t err = MZ_OK;
    uint16_t field_type = 0;
    uint16_t field_length = 0;


    if (max_seek < 4)
        return MZ_EXIST_ERROR;

    do {
        err = mz_stream_read_uint16(stream, &field_type);
        if (err == MZ_OK)
            err = mz_stream_read_uint16(stream, &field_length);
        if (err != MZ_OK)
            break;

        if (type == field_type) {
            if (length != NULL)
                *length = field_length;
            return MZ_OK;
        }

        max_seek -= field_length - 4;
        if (max_seek < 0)
            return MZ_EXIST_ERROR;

        err = mz_stream_seek(stream, field_length, MZ_SEEK_CUR);
    } while (err == MZ_OK);

    return MZ_EXIST_ERROR;
}

int32_t mz_zip_extrafield_contains(const uint8_t *extrafield, int32_t extrafield_size,
    uint16_t type, uint16_t *length) {
    void *file_extra_stream = NULL;
    int32_t err = MZ_OK;

    if (extrafield == NULL || extrafield_size == 0)
        return MZ_PARAM_ERROR;

    mz_stream_mem_create(&file_extra_stream);
    mz_stream_mem_set_buffer(file_extra_stream, (void *)extrafield, extrafield_size);

    err = mz_zip_extrafield_find(file_extra_stream, type, extrafield_size, length);

    mz_stream_mem_delete(&file_extra_stream);

    return err;
}

int32_t mz_zip_extrafield_read(void *stream, uint16_t *type, uint16_t *length) {
    int32_t err = MZ_OK;
    if (type == NULL || length == NULL)
        return MZ_PARAM_ERROR;
    err = mz_stream_read_uint16(stream, type);
    if (err == MZ_OK)
        err = mz_stream_read_uint16(stream, length);
    return err;
}

int32_t mz_zip_extrafield_write(void *stream, uint16_t type, uint16_t length) {
    int32_t err = MZ_OK;
    err = mz_stream_write_uint16(stream, type);
    if (err == MZ_OK)
        err = mz_stream_write_uint16(stream, length);
    return err;
}

/***************************************************************************/

static int32_t mz_zip_invalid_date(const struct tm *ptm) {
#define datevalue_in_range(min, max, value) ((min) <= (value) && (value) <= (max))
    return (!datevalue_in_range(0, 127 + 80, ptm->tm_year) ||  /* 1980-based year, allow 80 extra */
            !datevalue_in_range(0, 11, ptm->tm_mon) ||
            !datevalue_in_range(1, 31, ptm->tm_mday) ||
            !datevalue_in_range(0, 23, ptm->tm_hour) ||
            !datevalue_in_range(0, 59, ptm->tm_min) ||
            !datevalue_in_range(0, 59, ptm->tm_sec));
#undef datevalue_in_range
}

static void mz_zip_dosdate_to_raw_tm(uint64_t dos_date, struct tm *ptm) {
    uint64_t date = (uint64_t)(dos_date >> 16);

    ptm->tm_mday  = (uint16_t)(date & 0x1f);
    ptm->tm_mon   = (uint16_t)(((date & 0x1E0) / 0x20) - 1);
    ptm->tm_year  = (uint16_t)(((date & 0x0FE00) / 0x0200) + 80);
    ptm->tm_hour  = (uint16_t)((dos_date & 0xF800) / 0x800);
    ptm->tm_min   = (uint16_t)((dos_date & 0x7E0) / 0x20);
    ptm->tm_sec   = (uint16_t)(2 * (dos_date & 0x1f));
    ptm->tm_isdst = -1;
}

int32_t mz_zip_dosdate_to_tm(uint64_t dos_date, struct tm *ptm) {
    if (ptm == NULL)
        return MZ_PARAM_ERROR;

    mz_zip_dosdate_to_raw_tm(dos_date, ptm);

    if (mz_zip_invalid_date(ptm)) {
        /* Invalid date stored, so don't return it */
        memset(ptm, 0, sizeof(struct tm));
        return MZ_FORMAT_ERROR;
    }
    return MZ_OK;
}

time_t mz_zip_dosdate_to_time_t(uint64_t dos_date) {
    struct tm ptm;
    mz_zip_dosdate_to_raw_tm(dos_date, &ptm);
    return mktime(&ptm);
}

int32_t mz_zip_time_t_to_tm(time_t unix_time, struct tm *ptm) {
    struct tm ltm;
    if (ptm == NULL)
        return MZ_PARAM_ERROR;
    if (localtime_r(&unix_time, &ltm) == NULL) { /* Returns a 1900-based year */
        /* Invalid date stored, so don't return it */
        memset(ptm, 0, sizeof(struct tm));
        return MZ_INTERNAL_ERROR;
    }
    memcpy(ptm, &ltm, sizeof(struct tm));
    return MZ_OK;
}

uint32_t mz_zip_time_t_to_dos_date(time_t unix_time) {
    struct tm ptm;
    mz_zip_time_t_to_tm(unix_time, &ptm);
    return mz_zip_tm_to_dosdate((const struct tm *)&ptm);
}

uint32_t mz_zip_tm_to_dosdate(const struct tm *ptm) {
    struct tm fixed_tm;

    /* Years supported: */

    /* [00, 79]      (assumed to be between 2000 and 2079) */
    /* [80, 207]     (assumed to be between 1980 and 2107, typical output of old */
    /*                software that does 'year-1900' to get a double digit year) */
    /* [1980, 2107]  (due to format limitations, only years 1980-2107 can be stored.) */

    memcpy(&fixed_tm, ptm, sizeof(struct tm));
    if (fixed_tm.tm_year >= 1980) /* range [1980, 2107] */
        fixed_tm.tm_year -= 1980;
    else if (fixed_tm.tm_year >= 80) /* range [80, 207] */
        fixed_tm.tm_year -= 80;
    else /* range [00, 79] */
        fixed_tm.tm_year += 20;

    if (mz_zip_invalid_date(&fixed_tm))
        return 0;

    return (((uint32_t)fixed_tm.tm_mday + (32 * ((uint32_t)fixed_tm.tm_mon + 1)) + (512 * (uint32_t)fixed_tm.tm_year)) << 16) |
        (((uint32_t)fixed_tm.tm_sec / 2) + (32 * (uint32_t)fixed_tm.tm_min) + (2048 * (uint32_t)fixed_tm.tm_hour));
}

int32_t mz_zip_ntfs_to_unix_time(uint64_t ntfs_time, time_t *unix_time) {
    *unix_time = (time_t)((ntfs_time - 116444736000000000LL) / 10000000);
    return MZ_OK;
}

int32_t mz_zip_unix_to_ntfs_time(time_t unix_time, uint64_t *ntfs_time) {
    *ntfs_time = ((uint64_t)unix_time * 10000000) + 116444736000000000LL;
    return MZ_OK;
}

/***************************************************************************/

int32_t mz_zip_path_compare(const char *path1, const char *path2, uint8_t ignore_case) {
    do {
        if ((*path1 == '\\' && *path2 == '/') ||
            (*path2 == '\\' && *path1 == '/')) {
            /* Ignore comparison of path slashes */
        } else if (ignore_case) {
            if (tolower(*path1) != tolower(*path2))
                break;
        } else if (*path1 != *path2) {
            break;
        }

        path1 += 1;
        path2 += 1;
    } while (*path1 != 0 && *path2 != 0);

    if (ignore_case)
        return (int32_t)(tolower(*path1) - tolower(*path2));

    return (int32_t)(*path1 - *path2);
}

/***************************************************************************/

const char* mz_zip_get_compression_method_string(int32_t compression_method)
{
    const char *method = "?";
    switch (compression_method) {
    case MZ_COMPRESS_METHOD_STORE:
        method = "stored";
        break;
    case MZ_COMPRESS_METHOD_DEFLATE:
        method = "deflate";
        break;
    case MZ_COMPRESS_METHOD_BZIP2:
        method = "bzip2";
        break;
    case MZ_COMPRESS_METHOD_LZMA:
        method = "lzma";
        break;
    case MZ_COMPRESS_METHOD_XZ:
        method = "xz";
        break;
    case MZ_COMPRESS_METHOD_ZSTD:
        method = "zstd";
        break;
    }
    return method;
}

/***************************************************************************/
