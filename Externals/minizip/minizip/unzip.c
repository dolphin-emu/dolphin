/* unzip.c -- IO for uncompress .zip files using zlib
   Version 1.1, February 14h, 2010
   part of the MiniZip project

   Copyright (C) 1998-2010 Gilles Vollant
     http://www.winimage.com/zLibDll/minizip.html
   Modifications of Unzip for Zip64
     Copyright (C) 2007-2008 Even Rouault
   Modifications for Zip64 support on both zip and unzip
     Copyright (C) 2009-2010 Mathias Svensson
     http://result42.com
   Modifications for AES, PKWARE disk spanning
     Copyright (C) 2010-2014 Nathan Moinvaziri

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "zlib.h"
#include "unzip.h"

#ifdef HAVE_AES
#  define AES_METHOD          (99)
#  define AES_PWVERIFYSIZE    (2)
#  define AES_MAXSALTLENGTH   (16)
#  define AES_AUTHCODESIZE    (10)
#  define AES_HEADERSIZE      (11)
#  define AES_KEYSIZE(mode)   (64 + (mode * 64))

#  include "aes/aes.h"
#  include "aes/fileenc.h"
#endif
#ifdef HAVE_APPLE_COMPRESSION
#  include <compression.h>
#endif

#ifndef NOUNCRYPT
#  include "crypt.h"
#endif

#define DISKHEADERMAGIC             (0x08074b50)
#define LOCALHEADERMAGIC            (0x04034b50)
#define CENTRALHEADERMAGIC          (0x02014b50)
#define ENDHEADERMAGIC              (0x06054b50)
#define ZIP64ENDHEADERMAGIC         (0x06064b50)
#define ZIP64ENDLOCHEADERMAGIC      (0x07064b50)

#define SIZECENTRALDIRITEM          (0x2e)
#define SIZECENTRALHEADERLOCATOR    (0x14)
#define SIZEZIPLOCALHEADER          (0x1e)

#ifndef BUFREADCOMMENT
#  define BUFREADCOMMENT            (0x400)
#endif

#ifndef UNZ_BUFSIZE
#  define UNZ_BUFSIZE               (UINT16_MAX)
#endif
#ifndef UNZ_MAXFILENAMEINZIP
#  define UNZ_MAXFILENAMEINZIP      (256)
#endif

#ifndef ALLOC
#  define ALLOC(size) (malloc(size))
#endif
#ifndef TRYFREE
#  define TRYFREE(p) {if (p) free(p);}
#endif

const char unz_copyright[] =
   " unzip 1.01 Copyright 1998-2004 Gilles Vollant - http://www.winimage.com/zLibDll";

/* unz_file_info_internal contain internal info about a file in zipfile*/
typedef struct unz_file_info64_internal_s
{
    uint64_t offset_curfile;            /* relative offset of local header 8 bytes */
    uint64_t byte_before_the_zipfile;   /* byte before the zipfile, (>0 for sfx) */
#ifdef HAVE_AES
    uint8_t  aes_encryption_mode;
    uint16_t aes_compression_method;
    uint16_t aes_version;
#endif
} unz_file_info64_internal;

/* file_in_zip_read_info_s contain internal information about a file in zipfile */
typedef struct
{
    uint8_t *read_buffer;               /* internal buffer for compressed data */
    z_stream stream;                    /* zLib stream structure for inflate */
#ifdef HAVE_BZIP2
    bz_stream bstream;                  /* bzLib stream structure for bziped */
#endif
#ifdef HAVE_APPLE_COMPRESSION
    compression_stream astream;         /* libcompression stream structure */
#endif
#ifdef HAVE_AES
    fcrypt_ctx aes_ctx;
#endif
    uint64_t pos_in_zipfile;            /* position in byte on the zipfile, for fseek */
    uint8_t  stream_initialised;        /* flag set if stream structure is initialised */

    uint64_t offset_local_extrafield;   /* offset of the local extra field */
    uint16_t size_local_extrafield;     /* size of the local extra field */
    uint64_t pos_local_extrafield;      /* position in the local extra field in read */
    uint64_t total_out_64;

    uint32_t crc32;                     /* crc32 of all data uncompressed */
    uint32_t crc32_wait;                /* crc32 we must obtain after decompress all */
    uint64_t rest_read_compressed;      /* number of byte to be decompressed */
    uint64_t rest_read_uncompressed;    /* number of byte to be obtained after decomp */

    zlib_filefunc64_32_def z_filefunc;

    voidpf   filestream;                /* io structore of the zipfile */
    uint16_t compression_method;        /* compression method (0==store) */
    uint64_t byte_before_the_zipfile;   /* byte before the zipfile, (>0 for sfx) */
    int      raw;
} file_in_zip64_read_info_s;

/* unz64_s contain internal information about the zipfile */
typedef struct
{
    zlib_filefunc64_32_def z_filefunc;

    voidpf filestream;                  /* io structure of the current zipfile */
    voidpf filestream_with_CD;          /* io structure of the disk with the central directory */

    unz_global_info64 gi;               /* public global information */

    uint64_t byte_before_the_zipfile;   /* byte before the zipfile, (>0 for sfx) */
    uint64_t num_file;                  /* number of the current file in the zipfile */
    uint64_t pos_in_central_dir;        /* pos of the current file in the central dir */
    uint64_t current_file_ok;           /* flag about the usability of the current file */
    uint64_t central_pos;               /* position of the beginning of the central dir */
    uint32_t number_disk;               /* number of the current disk, used for spanning ZIP */
    uint64_t size_central_dir;          /* size of the central directory */
    uint64_t offset_central_dir;        /* offset of start of central directory with
                                           respect to the starting disk number */

    unz_file_info64 cur_file_info;      /* public info about the current file in zip*/
    unz_file_info64_internal cur_file_info_internal;
                                        /* private info about it*/
    file_in_zip64_read_info_s *pfile_in_zip_read;
                                        /* structure about the current file if we are decompressing it */
    int is_zip64;                       /* is the current file zip64 */
#ifndef NOUNCRYPT
    uint32_t keys[3];                   /* keys defining the pseudo-random sequence */
    const z_crc_t *pcrc_32_tab;
#endif
} unz64_s;

/* Read a byte from a gz_stream; Return EOF for end of file. */
static int unzReadUInt8(const zlib_filefunc64_32_def *pzlib_filefunc_def, voidpf filestream, uint8_t *value)
{
    uint8_t c = 0;
    if (ZREAD64(*pzlib_filefunc_def, filestream, &c, 1) == 1)
    {
        *value = (uint8_t)c;
        return UNZ_OK;
    }
    *value = 0;
    if (ZERROR64(*pzlib_filefunc_def, filestream))
        return UNZ_ERRNO;
    return UNZ_EOF;
}

static int unzReadUInt16(const zlib_filefunc64_32_def *pzlib_filefunc_def, voidpf filestream, uint16_t *value)
{
    uint16_t x;
    uint8_t c = 0;
    int err = UNZ_OK;

    err = unzReadUInt8(pzlib_filefunc_def, filestream, &c);
    x = (uint16_t)c;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &c);
    x |= ((uint16_t)c) << 8;

    if (err == UNZ_OK)
        *value = x;
    else
        *value = 0;
    return err;
}

static int unzReadUInt32(const zlib_filefunc64_32_def *pzlib_filefunc_def, voidpf filestream, uint32_t *value)
{
    uint32_t x = 0;
    uint8_t c = 0;
    int err = UNZ_OK;

    err = unzReadUInt8(pzlib_filefunc_def, filestream, &c);
    x = (uint32_t)c;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &c);
    x |= ((uint32_t)c) << 8;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &c);
    x |= ((uint32_t)c) << 16;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &c);
    x += ((uint32_t)c) << 24;

    if (err == UNZ_OK)
        *value = x;
    else
        *value = 0;
    return err;
}

static int unzReadUInt64(const zlib_filefunc64_32_def *pzlib_filefunc_def, voidpf filestream, uint64_t *value)
{
    uint64_t x = 0;
    uint8_t i = 0;
    int err = UNZ_OK;

    err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x = (uint64_t)i;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 8;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 16;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 24;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 32;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 40;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 48;
    if (err == UNZ_OK)
        err = unzReadUInt8(pzlib_filefunc_def, filestream, &i);
    x |= ((uint64_t)i) << 56;

    if (err == UNZ_OK)
        *value = x;
    else
        *value = 0;
    return err;
}

/* Locate the Central directory of a zip file (at the end, just before the global comment) */
static uint64_t unzSearchCentralDir(const zlib_filefunc64_32_def *pzlib_filefunc_def, voidpf filestream)
{
    uint8_t buf[BUFREADCOMMENT + 4];
    uint64_t file_size = 0;
    uint64_t back_read = 4;
    uint64_t max_back = UINT16_MAX; /* maximum size of global comment */
    uint64_t pos_found = 0;
    uint32_t read_size = 0;
    uint64_t read_pos = 0;
    uint32_t i = 0;

    if (ZSEEK64(*pzlib_filefunc_def, filestream, 0, ZLIB_FILEFUNC_SEEK_END) != 0)
        return 0;

    file_size = ZTELL64(*pzlib_filefunc_def, filestream);

    if (max_back > file_size)
        max_back = file_size;

    while (back_read < max_back)
    {
        if (back_read + BUFREADCOMMENT > max_back)
            back_read = max_back;
        else
            back_read += BUFREADCOMMENT;

        read_pos = file_size - back_read;
        read_size = ((BUFREADCOMMENT + 4) < (file_size - read_pos)) ?
                     (BUFREADCOMMENT + 4) : (uint32_t)(file_size - read_pos);

        if (ZSEEK64(*pzlib_filefunc_def, filestream, read_pos, ZLIB_FILEFUNC_SEEK_SET) != 0)
            break;
        if (ZREAD64(*pzlib_filefunc_def, filestream, buf, read_size) != read_size)
            break;

        for (i = read_size - 3; (i--) > 0;)
            if (((*(buf+i)) == (ENDHEADERMAGIC & 0xff)) &&
                ((*(buf+i+1)) == (ENDHEADERMAGIC >> 8 & 0xff)) &&
                ((*(buf+i+2)) == (ENDHEADERMAGIC >> 16 & 0xff)) &&
                ((*(buf+i+3)) == (ENDHEADERMAGIC >> 24 & 0xff)))
            {
                pos_found = read_pos+i;
                break;
            }

        if (pos_found != 0)
            break;
    }

    return pos_found;
}

/* Locate the Central directory 64 of a zipfile (at the end, just before the global comment) */
static uint64_t unzSearchCentralDir64(const zlib_filefunc64_32_def *pzlib_filefunc_def, voidpf filestream,
    const uint64_t endcentraloffset)
{
    uint64_t offset = 0;
    uint32_t value32 = 0;

    /* Zip64 end of central directory locator */
    if (ZSEEK64(*pzlib_filefunc_def, filestream, endcentraloffset - SIZECENTRALHEADERLOCATOR, ZLIB_FILEFUNC_SEEK_SET) != 0)
        return 0;

    /* Read locator signature */
    if (unzReadUInt32(pzlib_filefunc_def, filestream, &value32) != UNZ_OK)
        return 0;
    if (value32 != ZIP64ENDLOCHEADERMAGIC)
        return 0;
    /* Number of the disk with the start of the zip64 end of  central directory */
    if (unzReadUInt32(pzlib_filefunc_def, filestream, &value32) != UNZ_OK)
        return 0;
    /* Relative offset of the zip64 end of central directory record */
    if (unzReadUInt64(pzlib_filefunc_def, filestream, &offset) != UNZ_OK)
        return 0;
    /* Total number of disks */
    if (unzReadUInt32(pzlib_filefunc_def, filestream, &value32) != UNZ_OK)
        return 0;
    /* Goto end of central directory record */
    if (ZSEEK64(*pzlib_filefunc_def, filestream, offset, ZLIB_FILEFUNC_SEEK_SET) != 0)
        return 0;
     /* The signature */
    if (unzReadUInt32(pzlib_filefunc_def, filestream, &value32) != UNZ_OK)
        return 0;
    if (value32 != ZIP64ENDHEADERMAGIC)
        return 0;

    return offset;
}

static unzFile unzOpenInternal(const void *path, zlib_filefunc64_32_def *pzlib_filefunc64_32_def)
{
    unz64_s us;
    unz64_s *s = NULL;
    uint64_t central_pos = 0;
    uint64_t central_pos64 = 0;
    uint64_t number_entry_CD = 0;
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    uint64_t value64 = 0;
    voidpf filestream = NULL;
    int err = UNZ_OK;

    if (unz_copyright[0] != ' ')
        return NULL;

    us.filestream = NULL;
    us.filestream_with_CD = NULL;
    us.z_filefunc.zseek32_file = NULL;
    us.z_filefunc.ztell32_file = NULL;

    if (pzlib_filefunc64_32_def == NULL)
        fill_fopen64_filefunc(&us.z_filefunc.zfile_func64);
    else
        us.z_filefunc = *pzlib_filefunc64_32_def;

    us.filestream = ZOPEN64(us.z_filefunc, path, ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_EXISTING);

    if (us.filestream == NULL)
        return NULL;

    us.filestream_with_CD = us.filestream;
    us.is_zip64 = 0;

    /* Search for end of central directory header */
    central_pos = unzSearchCentralDir(&us.z_filefunc, us.filestream);
    if (central_pos)
    {
        if (ZSEEK64(us.z_filefunc, us.filestream, central_pos, ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = UNZ_ERRNO;

        /* The signature, already checked */
        if (unzReadUInt32(&us.z_filefunc, us.filestream, &value32) != UNZ_OK)
            err = UNZ_ERRNO;
        /* Number of this disk */
        if (unzReadUInt16(&us.z_filefunc, us.filestream, &value16) != UNZ_OK)
            err = UNZ_ERRNO;
        us.number_disk = value16;
        /* Number of the disk with the start of the central directory */
        if (unzReadUInt16(&us.z_filefunc, us.filestream, &value16) != UNZ_OK)
            err = UNZ_ERRNO;
        us.gi.number_disk_with_CD = value16;
        /* Total number of entries in the central directory on this disk */
        if (unzReadUInt16(&us.z_filefunc, us.filestream, &value16) != UNZ_OK)
            err = UNZ_ERRNO;
        us.gi.number_entry = value16;
        /* Total number of entries in the central directory */
        if (unzReadUInt16(&us.z_filefunc, us.filestream, &value16) != UNZ_OK)
            err = UNZ_ERRNO;
        number_entry_CD = value16;
        if (number_entry_CD != us.gi.number_entry)
            err = UNZ_BADZIPFILE;
        /* Size of the central directory */
        if (unzReadUInt32(&us.z_filefunc, us.filestream, &value32) != UNZ_OK)
            err = UNZ_ERRNO;
        us.size_central_dir = value32;
        /* Offset of start of central directory with respect to the starting disk number */
        if (unzReadUInt32(&us.z_filefunc, us.filestream, &value32) != UNZ_OK)
            err = UNZ_ERRNO;
        us.offset_central_dir = value32;
        /* Zipfile comment length */
        if (unzReadUInt16(&us.z_filefunc, us.filestream, &us.gi.size_comment) != UNZ_OK)
            err = UNZ_ERRNO;

        if (err == UNZ_OK)
        {
            /* Search for Zip64 end of central directory header */
            central_pos64 = unzSearchCentralDir64(&us.z_filefunc, us.filestream, central_pos);
            if (central_pos64)
            {
                central_pos = central_pos64;
                us.is_zip64 = 1;

                if (ZSEEK64(us.z_filefunc, us.filestream, central_pos, ZLIB_FILEFUNC_SEEK_SET) != 0)
                    err = UNZ_ERRNO;

                /* the signature, already checked */
                if (unzReadUInt32(&us.z_filefunc, us.filestream, &value32) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* size of zip64 end of central directory record */
                if (unzReadUInt64(&us.z_filefunc, us.filestream, &value64) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* version made by */
                if (unzReadUInt16(&us.z_filefunc, us.filestream, &value16) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* version needed to extract */
                if (unzReadUInt16(&us.z_filefunc, us.filestream, &value16) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* number of this disk */
                if (unzReadUInt32(&us.z_filefunc, us.filestream, &us.number_disk) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* number of the disk with the start of the central directory */
                if (unzReadUInt32(&us.z_filefunc, us.filestream, &us.gi.number_disk_with_CD) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* total number of entries in the central directory on this disk */
                if (unzReadUInt64(&us.z_filefunc, us.filestream, &us.gi.number_entry) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* total number of entries in the central directory */
                if (unzReadUInt64(&us.z_filefunc, us.filestream, &number_entry_CD) != UNZ_OK)
                    err = UNZ_ERRNO;
                if (number_entry_CD != us.gi.number_entry)
                    err = UNZ_BADZIPFILE;
                /* size of the central directory */
                if (unzReadUInt64(&us.z_filefunc, us.filestream, &us.size_central_dir) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* offset of start of central directory with respect to the starting disk number */
                if (unzReadUInt64(&us.z_filefunc, us.filestream, &us.offset_central_dir) != UNZ_OK)
                    err = UNZ_ERRNO;
            }
            else if ((us.gi.number_entry == UINT16_MAX) || (us.size_central_dir == UINT16_MAX) || (us.offset_central_dir == UINT32_MAX))
                err = UNZ_BADZIPFILE;
        }
    }
    else
        err = UNZ_ERRNO;

    if ((err == UNZ_OK) && (central_pos < us.offset_central_dir + us.size_central_dir))
        err = UNZ_BADZIPFILE;

    if (err != UNZ_OK)
    {
        ZCLOSE64(us.z_filefunc, us.filestream);
        return NULL;
    }

    if (us.gi.number_disk_with_CD == 0)
    {
        /* If there is only one disk open another stream so we don't have to seek between the CD
           and the file headers constantly */
        filestream = ZOPEN64(us.z_filefunc, path, ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_EXISTING);
        if (filestream != NULL)
            us.filestream = filestream;
    }

    /* Hack for zip files that have no respect for zip64
    if ((central_pos > 0xffffffff) && (us.offset_central_dir < 0xffffffff))
        us.offset_central_dir = central_pos - us.size_central_dir;*/

    us.byte_before_the_zipfile = central_pos - (us.offset_central_dir + us.size_central_dir);
    us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;

    s = (unz64_s*)ALLOC(sizeof(unz64_s));
    if (s != NULL)
    {
        *s = us;
        unzGoToFirstFile((unzFile)s);
    }
    return (unzFile)s;
}

extern unzFile ZEXPORT unzOpen2(const char *path, zlib_filefunc_def *pzlib_filefunc32_def)
{
    if (pzlib_filefunc32_def != NULL)
    {
        zlib_filefunc64_32_def zlib_filefunc64_32_def_fill;
        fill_zlib_filefunc64_32_def_from_filefunc32(&zlib_filefunc64_32_def_fill, pzlib_filefunc32_def);
        return unzOpenInternal(path, &zlib_filefunc64_32_def_fill);
    }
    return unzOpenInternal(path, NULL);
}

extern unzFile ZEXPORT unzOpen2_64(const void *path, zlib_filefunc64_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
    {
        zlib_filefunc64_32_def zlib_filefunc64_32_def_fill;
        zlib_filefunc64_32_def_fill.zfile_func64 = *pzlib_filefunc_def;
        zlib_filefunc64_32_def_fill.ztell32_file = NULL;
        zlib_filefunc64_32_def_fill.zseek32_file = NULL;
        return unzOpenInternal(path, &zlib_filefunc64_32_def_fill);
    }
    return unzOpenInternal(path, NULL);
}

extern unzFile ZEXPORT unzOpen(const char *path)
{
    return unzOpenInternal(path, NULL);
}

extern unzFile ZEXPORT unzOpen64(const void *path)
{
    return unzOpenInternal(path, NULL);
}

extern int ZEXPORT unzClose(unzFile file)
{
    unz64_s *s;
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    if (s->pfile_in_zip_read != NULL)
        unzCloseCurrentFile(file);

    if ((s->filestream != NULL) && (s->filestream != s->filestream_with_CD))
        ZCLOSE64(s->z_filefunc, s->filestream);
    if (s->filestream_with_CD != NULL)
        ZCLOSE64(s->z_filefunc, s->filestream_with_CD);

    s->filestream = NULL;
    s->filestream_with_CD = NULL;
    TRYFREE(s);
    return UNZ_OK;
}

/* Goto to the next available disk for spanned archives */
static int unzGoToNextDisk(unzFile file)
{
    unz64_s *s;
    uint32_t number_disk_next = 0;

    s = (unz64_s*)file;
    if (s == NULL)
        return UNZ_PARAMERROR;
    number_disk_next = s->number_disk;

    if ((s->pfile_in_zip_read != NULL) && (s->pfile_in_zip_read->rest_read_uncompressed > 0))
        /* We are currently reading a file and we need the next sequential disk */
        number_disk_next += 1;
    else
        /* Goto the disk for the current file */
        number_disk_next = s->cur_file_info.disk_num_start;

    if (number_disk_next != s->number_disk)
    {
        /* Switch disks */
        if ((s->filestream != NULL) && (s->filestream != s->filestream_with_CD))
            ZCLOSE64(s->z_filefunc, s->filestream);

        if (number_disk_next == s->gi.number_disk_with_CD)
        {
            s->filestream = s->filestream_with_CD;
        }
        else
        {
            s->filestream = ZOPENDISK64(s->z_filefunc, s->filestream_with_CD, number_disk_next,
                ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_EXISTING);
        }

        if (s->filestream == NULL)
            return UNZ_ERRNO;

        s->number_disk = number_disk_next;
    }

    return UNZ_OK;
}

extern int ZEXPORT unzGetGlobalInfo(unzFile file, unz_global_info* pglobal_info32)
{
    unz64_s *s = NULL;
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    pglobal_info32->number_entry = (uint32_t)s->gi.number_entry;
    pglobal_info32->size_comment = s->gi.size_comment;
    pglobal_info32->number_disk_with_CD = s->gi.number_disk_with_CD;
    return UNZ_OK;
}

extern int ZEXPORT unzGetGlobalInfo64(unzFile file, unz_global_info64 *pglobal_info)
{
    unz64_s *s = NULL;
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    *pglobal_info = s->gi;
    return UNZ_OK;
}

extern int ZEXPORT unzGetGlobalComment(unzFile file, char *comment, uint16_t comment_size)
{
    unz64_s *s = NULL;
    uint16_t bytes_to_read = comment_size;
    if (file == NULL)
        return (int)UNZ_PARAMERROR;
    s = (unz64_s*)file;

    if (bytes_to_read > s->gi.size_comment)
        bytes_to_read = s->gi.size_comment;

    if (ZSEEK64(s->z_filefunc, s->filestream_with_CD, s->central_pos + 22, ZLIB_FILEFUNC_SEEK_SET) != 0)
        return UNZ_ERRNO;

    if (bytes_to_read > 0)
    {
        *comment = 0;
        if (ZREAD64(s->z_filefunc, s->filestream_with_CD, comment, bytes_to_read) != bytes_to_read)
            return UNZ_ERRNO;
    }

    if ((comment != NULL) && (comment_size > s->gi.size_comment))
        *(comment + s->gi.size_comment) = 0;

    return (int)bytes_to_read;
}

static int unzGetCurrentFileInfoField(unzFile file, uint32_t *seek, void *field, uint16_t field_size, uint16_t size_file_field, int null_terminated_field)
{
    unz64_s *s = NULL;
    uint32_t bytes_to_read = 0;
    int err = UNZ_OK;

    if (file == NULL)
        return (int)UNZ_PARAMERROR;
    s = (unz64_s*)file;

    /* Read field */
    if (field != NULL)
    {
        if (size_file_field < field_size)
        {
            if (null_terminated_field)
                *((char *)field+size_file_field) = 0;

            bytes_to_read = size_file_field;
        }
        else
            bytes_to_read = field_size;
        
        if (*seek != 0)
        {
            if (ZSEEK64(s->z_filefunc, s->filestream_with_CD, *seek, ZLIB_FILEFUNC_SEEK_CUR) == 0)
                *seek = 0;
            else
                err = UNZ_ERRNO;
        }
        
        if ((size_file_field > 0) && (field_size > 0))
        {
            if (ZREAD64(s->z_filefunc, s->filestream_with_CD, field, bytes_to_read) != bytes_to_read)
                err = UNZ_ERRNO;
        }
        *seek += size_file_field - bytes_to_read;
    }
    else
    {
        *seek += size_file_field;
    }

    return err;
}

/* Get info about the current file in the zipfile, with internal only info */
static int unzGetCurrentFileInfoInternal(unzFile file, unz_file_info64 *pfile_info,
    unz_file_info64_internal *pfile_info_internal, char *filename, uint16_t filename_size, void *extrafield,
    uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    unz64_s *s = NULL;
    unz_file_info64 file_info;
    unz_file_info64_internal file_info_internal;
    uint32_t magic = 0;
    uint64_t current_pos = 0;
    uint32_t seek = 0;
    uint32_t extra_pos = 0;
    uint16_t extra_header_id = 0;
    uint16_t extra_data_size = 0;
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    uint64_t value64 = 0;
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    if (ZSEEK64(s->z_filefunc, s->filestream_with_CD,
            s->pos_in_central_dir + s->byte_before_the_zipfile, ZLIB_FILEFUNC_SEEK_SET) != 0)
        err = UNZ_ERRNO;

    /* Check the magic */
    if (err == UNZ_OK)
    {
        if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &magic) != UNZ_OK)
            err = UNZ_ERRNO;
        else if (magic != CENTRALHEADERMAGIC)
            err = UNZ_BADZIPFILE;
    }

    /* Read central directory header */
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.version) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.version_needed) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.flag) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.compression_method) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &file_info.dos_date) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &file_info.crc) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &value32) != UNZ_OK)
        err = UNZ_ERRNO;
    file_info.compressed_size = value32;
    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &value32) != UNZ_OK)
        err = UNZ_ERRNO;
    file_info.uncompressed_size = value32;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.size_filename) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.size_file_extra) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.size_file_comment) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &value16) != UNZ_OK)
        err = UNZ_ERRNO;
    file_info.disk_num_start = value16;
    if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &file_info.internal_fa) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &file_info.external_fa) != UNZ_OK)
        err = UNZ_ERRNO;
    /* Relative offset of local header */
    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &value32) != UNZ_OK)
        err = UNZ_ERRNO;

    file_info.size_file_extra_internal = 0;
    file_info.disk_offset = value32;
    file_info_internal.offset_curfile = value32;
#ifdef HAVE_AES
    file_info_internal.aes_compression_method = 0;
    file_info_internal.aes_encryption_mode = 0;
    file_info_internal.aes_version = 0;
#endif

    if (err == UNZ_OK)
        err = unzGetCurrentFileInfoField(file, &seek, filename, filename_size, file_info.size_filename, 1);

    /* Read extrafield */
    if (err == UNZ_OK)
        err = unzGetCurrentFileInfoField(file, &seek, extrafield, extrafield_size, file_info.size_file_extra, 0);

    if ((err == UNZ_OK) && (file_info.size_file_extra != 0))
    {
        if (seek != 0)
        {
            if (ZSEEK64(s->z_filefunc, s->filestream_with_CD, seek, ZLIB_FILEFUNC_SEEK_CUR) == 0)
                seek = 0;
            else
                err = UNZ_ERRNO;
        }

        /* We are going to parse the extra field so we need to move back */
        current_pos = ZTELL64(s->z_filefunc, s->filestream_with_CD);
        if (current_pos < file_info.size_file_extra)
            err = UNZ_ERRNO;
        current_pos -= file_info.size_file_extra;
        if (ZSEEK64(s->z_filefunc, s->filestream_with_CD, current_pos, ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = UNZ_ERRNO;

        while ((err != UNZ_ERRNO) && (extra_pos < file_info.size_file_extra))
        {
            if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &extra_header_id) != UNZ_OK)
                err = UNZ_ERRNO;
            if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &extra_data_size) != UNZ_OK)
                err = UNZ_ERRNO;

            /* ZIP64 extra fields */
            if (extra_header_id == 0x0001)
            {
                /* Subtract size of ZIP64 field, since ZIP64 is handled internally */
                file_info.size_file_extra_internal += 2 + 2 + extra_data_size;

                if (file_info.uncompressed_size == UINT32_MAX)
                {
                    if (unzReadUInt64(&s->z_filefunc, s->filestream_with_CD, &file_info.uncompressed_size) != UNZ_OK)
                        err = UNZ_ERRNO;
                }
                if (file_info.compressed_size == UINT32_MAX)
                {
                    if (unzReadUInt64(&s->z_filefunc, s->filestream_with_CD, &file_info.compressed_size) != UNZ_OK)
                        err = UNZ_ERRNO;
                }
                if (file_info_internal.offset_curfile == UINT32_MAX)
                {
                    /* Relative Header offset */
                    if (unzReadUInt64(&s->z_filefunc, s->filestream_with_CD, &value64) != UNZ_OK)
                        err = UNZ_ERRNO;
                    file_info_internal.offset_curfile = value64;
                    file_info.disk_offset = value64;
                }
                if (file_info.disk_num_start == UINT32_MAX)
                {
                    /* Disk Start Number */
                    if (unzReadUInt32(&s->z_filefunc, s->filestream_with_CD, &file_info.disk_num_start) != UNZ_OK)
                        err = UNZ_ERRNO;
                }
            }
#ifdef HAVE_AES
            /* AES header */
            else if (extra_header_id == 0x9901)
            {
                uint8_t value8 = 0;

                /* Subtract size of AES field, since AES is handled internally */
                file_info.size_file_extra_internal += 2 + 2 + extra_data_size;

                /* Verify version info */
                if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &value16) != UNZ_OK)
                    err = UNZ_ERRNO;
                /* Support AE-1 and AE-2 */
                if (value16 != 1 && value16 != 2)
                    err = UNZ_ERRNO;
                file_info_internal.aes_version = value16;
                if (unzReadUInt8(&s->z_filefunc, s->filestream_with_CD, &value8) != UNZ_OK)
                    err = UNZ_ERRNO;
                if ((char)value8 != 'A')
                    err = UNZ_ERRNO;
                if (unzReadUInt8(&s->z_filefunc, s->filestream_with_CD, &value8) != UNZ_OK)
                    err = UNZ_ERRNO;
                if ((char)value8 != 'E')
                    err = UNZ_ERRNO;
                /* Get AES encryption strength and actual compression method */
                if (unzReadUInt8(&s->z_filefunc, s->filestream_with_CD, &value8) != UNZ_OK)
                    err = UNZ_ERRNO;
                file_info_internal.aes_encryption_mode = value8;
                if (unzReadUInt16(&s->z_filefunc, s->filestream_with_CD, &value16) != UNZ_OK)
                    err = UNZ_ERRNO;
                file_info_internal.aes_compression_method = value16;
            }
#endif
            else
            {
                if (ZSEEK64(s->z_filefunc, s->filestream_with_CD,extra_data_size, ZLIB_FILEFUNC_SEEK_CUR) != 0)
                    err = UNZ_ERRNO;
            }

            extra_pos += 2 + 2 + extra_data_size;
        }
    }

    if (file_info.disk_num_start == s->gi.number_disk_with_CD)
        file_info_internal.byte_before_the_zipfile = s->byte_before_the_zipfile;
    else
        file_info_internal.byte_before_the_zipfile = 0;

    if (err == UNZ_OK)
        err = unzGetCurrentFileInfoField(file, &seek, comment, comment_size, file_info.size_file_comment, 1);

    if ((err == UNZ_OK) && (pfile_info != NULL))
        *pfile_info = file_info;
    if ((err == UNZ_OK) && (pfile_info_internal != NULL))
        *pfile_info_internal = file_info_internal;

    return err;
}

extern int ZEXPORT unzGetCurrentFileInfo(unzFile file, unz_file_info *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    unz_file_info64 file_info64;
    int err = UNZ_OK;

    err = unzGetCurrentFileInfoInternal(file, &file_info64, NULL, filename, filename_size,
                extrafield, extrafield_size, comment, comment_size);

    if ((err == UNZ_OK) && (pfile_info != NULL))
    {
        pfile_info->version = file_info64.version;
        pfile_info->version_needed = file_info64.version_needed;
        pfile_info->flag = file_info64.flag;
        pfile_info->compression_method = file_info64.compression_method;
        pfile_info->dos_date = file_info64.dos_date;
        pfile_info->crc = file_info64.crc;

        pfile_info->size_filename = file_info64.size_filename;
        pfile_info->size_file_extra = file_info64.size_file_extra - file_info64.size_file_extra_internal;
        pfile_info->size_file_comment = file_info64.size_file_comment;

        pfile_info->disk_num_start = (uint16_t)file_info64.disk_num_start;
        pfile_info->internal_fa = file_info64.internal_fa;
        pfile_info->external_fa = file_info64.external_fa;

        pfile_info->compressed_size = (uint32_t)file_info64.compressed_size;
        pfile_info->uncompressed_size = (uint32_t)file_info64.uncompressed_size;
    }
    return err;
}

extern int ZEXPORT unzGetCurrentFileInfo64(unzFile file, unz_file_info64 * pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    return unzGetCurrentFileInfoInternal(file, pfile_info, NULL, filename, filename_size,
        extrafield, extrafield_size, comment,comment_size);
}

/* Read the local header of the current zipfile. Check the coherency of the local header and info in the
   end of central directory about this file store in *piSizeVar the size of extra info in local header
   (filename and size of extra field data) */
static int unzCheckCurrentFileCoherencyHeader(unz64_s *s, uint32_t *psize_variable, uint64_t *poffset_local_extrafield,
    uint16_t *psize_local_extrafield)
{
    uint32_t magic = 0;
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    uint32_t flags = 0;
    uint16_t size_filename = 0;
    uint16_t size_extra_field = 0;
    uint16_t compression_method = 0;
    int err = UNZ_OK;

    if (psize_variable == NULL)
        return UNZ_PARAMERROR;
    *psize_variable = 0;
    if (poffset_local_extrafield == NULL)
        return UNZ_PARAMERROR;
    *poffset_local_extrafield = 0;
    if (psize_local_extrafield == NULL)
        return UNZ_PARAMERROR;
    *psize_local_extrafield = 0;

    err = unzGoToNextDisk((unzFile)s);
    if (err != UNZ_OK)
        return err;

    if (ZSEEK64(s->z_filefunc, s->filestream, s->cur_file_info_internal.offset_curfile +
        s->cur_file_info_internal.byte_before_the_zipfile, ZLIB_FILEFUNC_SEEK_SET) != 0)
        return UNZ_ERRNO;

    if (err == UNZ_OK)
    {
        if (unzReadUInt32(&s->z_filefunc, s->filestream, &magic) != UNZ_OK)
            err = UNZ_ERRNO;
        else if (magic != LOCALHEADERMAGIC)
            err = UNZ_BADZIPFILE;
    }

    if (unzReadUInt16(&s->z_filefunc, s->filestream, &value16) != UNZ_OK)
        err = UNZ_ERRNO;
    if (unzReadUInt16(&s->z_filefunc, s->filestream, &value16) != UNZ_OK)
        err = UNZ_ERRNO;
    flags = value16;
    if (unzReadUInt16(&s->z_filefunc, s->filestream, &value16) != UNZ_OK)
        err = UNZ_ERRNO;
    else if ((err == UNZ_OK) && (value16 != s->cur_file_info.compression_method))
        err = UNZ_BADZIPFILE;

    compression_method = s->cur_file_info.compression_method;
#ifdef HAVE_AES
    if (compression_method == AES_METHOD)
        compression_method = s->cur_file_info_internal.aes_compression_method;
#endif

    if ((err == UNZ_OK) && (compression_method != 0) && (compression_method != Z_DEFLATED))
    {
#ifdef HAVE_BZIP2
        if (compression_method != Z_BZIP2ED)
#endif
            err = UNZ_BADZIPFILE;
    }

    if (unzReadUInt32(&s->z_filefunc, s->filestream, &value32) != UNZ_OK) /* date/time */
        err = UNZ_ERRNO;
    if (unzReadUInt32(&s->z_filefunc, s->filestream, &value32) != UNZ_OK) /* crc */
        err = UNZ_ERRNO;
    else if ((err == UNZ_OK) && (value32 != s->cur_file_info.crc) && ((flags & 8) == 0))
        err = UNZ_BADZIPFILE;
    if (unzReadUInt32(&s->z_filefunc, s->filestream, &value32) != UNZ_OK) /* size compr */
        err = UNZ_ERRNO;
    else if ((value32 != UINT32_MAX) && (err == UNZ_OK) && (value32 != s->cur_file_info.compressed_size) && ((flags & 8) == 0))
        err = UNZ_BADZIPFILE;
    if (unzReadUInt32(&s->z_filefunc, s->filestream, &value32) != UNZ_OK) /* size uncompr */
        err = UNZ_ERRNO;
    else if ((value32 != UINT32_MAX) && (err == UNZ_OK) && (value32 != s->cur_file_info.uncompressed_size) && ((flags & 8) == 0))
        err = UNZ_BADZIPFILE;
    if (unzReadUInt16(&s->z_filefunc, s->filestream, &size_filename) != UNZ_OK)
        err = UNZ_ERRNO;
    else if ((err == UNZ_OK) && (size_filename != s->cur_file_info.size_filename))
        err = UNZ_BADZIPFILE;

    *psize_variable += size_filename;

    if (unzReadUInt16(&s->z_filefunc, s->filestream, &size_extra_field) != UNZ_OK)
        err = UNZ_ERRNO;

    *poffset_local_extrafield = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + size_filename;
    *psize_local_extrafield = size_extra_field;
    *psize_variable += size_extra_field;

    return err;
}

/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is UNZ_OK.
*/
extern int ZEXPORT unzOpenCurrentFile3(unzFile file, int *method, int *level, int raw, const char *password)
{
    unz64_s *s = NULL;
    file_in_zip64_read_info_s *pfile_in_zip_read_info = NULL;
    uint16_t compression_method = 0;
    uint64_t offset_local_extrafield = 0;
    uint16_t size_local_extrafield = 0;
    uint32_t size_variable = 0;
    int err = UNZ_OK;
#ifndef NOUNCRYPT
    char source[12];
#else
    if (password != NULL)
        return UNZ_PARAMERROR;
#endif
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (!s->current_file_ok)
        return UNZ_PARAMERROR;

    if (s->pfile_in_zip_read != NULL)
        unzCloseCurrentFile(file);

    if (unzCheckCurrentFileCoherencyHeader(s, &size_variable, &offset_local_extrafield, &size_local_extrafield) != UNZ_OK)
        return UNZ_BADZIPFILE;

    pfile_in_zip_read_info = (file_in_zip64_read_info_s*)ALLOC(sizeof(file_in_zip64_read_info_s));
    if (pfile_in_zip_read_info == NULL)
        return UNZ_INTERNALERROR;

    pfile_in_zip_read_info->read_buffer = (uint8_t*)ALLOC(UNZ_BUFSIZE);
    pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
    pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
    pfile_in_zip_read_info->pos_local_extrafield = 0;
    pfile_in_zip_read_info->raw = raw;

    if (pfile_in_zip_read_info->read_buffer == NULL)
    {
        TRYFREE(pfile_in_zip_read_info);
        return UNZ_INTERNALERROR;
    }

    pfile_in_zip_read_info->stream_initialised = 0;

    compression_method = s->cur_file_info.compression_method;
#ifdef HAVE_AES
    if (compression_method == AES_METHOD)
    {
        compression_method = s->cur_file_info_internal.aes_compression_method;
        if (password == NULL)
        {
            TRYFREE(pfile_in_zip_read_info);
            return UNZ_PARAMERROR;
        }
    }
#endif

    if (method != NULL)
        *method = compression_method;

    if (level != NULL)
    {
        *level = 6;
        switch (s->cur_file_info.flag & 0x06)
        {
          case 6 : *level = 1; break;
          case 4 : *level = 2; break;
          case 2 : *level = 9; break;
        }
    }

    if ((compression_method != 0) && (compression_method != Z_DEFLATED))
    {
#ifdef HAVE_BZIP2
        if (compression_method != Z_BZIP2ED)
#endif
        {
            TRYFREE(pfile_in_zip_read_info);
            return UNZ_BADZIPFILE;
        }
    }

    pfile_in_zip_read_info->crc32_wait = s->cur_file_info.crc;
    pfile_in_zip_read_info->crc32 = 0;
    pfile_in_zip_read_info->total_out_64 = 0;
    pfile_in_zip_read_info->compression_method = compression_method;
    pfile_in_zip_read_info->filestream = s->filestream;
    pfile_in_zip_read_info->z_filefunc = s->z_filefunc;
    if (s->number_disk == s->gi.number_disk_with_CD)
        pfile_in_zip_read_info->byte_before_the_zipfile = s->byte_before_the_zipfile;
    else
        pfile_in_zip_read_info->byte_before_the_zipfile = 0;
    pfile_in_zip_read_info->stream.total_out = 0;
    pfile_in_zip_read_info->stream.total_in = 0;
    pfile_in_zip_read_info->stream.next_in = NULL;

    if (!raw)
    {
        if (compression_method == Z_BZIP2ED)
        {
#ifdef HAVE_BZIP2
            pfile_in_zip_read_info->bstream.bzalloc = (void *(*) (void *, int, int))0;
            pfile_in_zip_read_info->bstream.bzfree = (free_func)0;
            pfile_in_zip_read_info->bstream.opaque = (voidpf)0;
            pfile_in_zip_read_info->bstream.state = (voidpf)0;

            pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
            pfile_in_zip_read_info->stream.zfree = (free_func)0;
            pfile_in_zip_read_info->stream.opaque = (voidpf)0;
            pfile_in_zip_read_info->stream.next_in = (voidpf)0;
            pfile_in_zip_read_info->stream.avail_in = 0;

            err = BZ2_bzDecompressInit(&pfile_in_zip_read_info->bstream, 0, 0);
            if (err == Z_OK)
            {
                pfile_in_zip_read_info->stream_initialised = Z_BZIP2ED;
            }
            else
            {
                TRYFREE(pfile_in_zip_read_info);
                return err;
            }
#else
            pfile_in_zip_read_info->raw = 1;
#endif
        }
        else if (compression_method == Z_DEFLATED)
        {
            pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
            pfile_in_zip_read_info->stream.zfree = (free_func)0;
            pfile_in_zip_read_info->stream.opaque = (voidpf)s;
            pfile_in_zip_read_info->stream.next_in = 0;
            pfile_in_zip_read_info->stream.avail_in = 0;

#ifdef HAVE_APPLE_COMPRESSION
            err = compression_stream_init(&pfile_in_zip_read_info->astream, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB);
            if (err == COMPRESSION_STATUS_ERROR)
                err = UNZ_INTERNALERROR;
            else
                err = Z_OK;
#else
            err = inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
#endif
            if (err == Z_OK)
            {
                pfile_in_zip_read_info->stream_initialised = Z_DEFLATED;
            }
            else
            {
                TRYFREE(pfile_in_zip_read_info);
                return err;
            }
            /* windowBits is passed < 0 to tell that there is no zlib header.
             * Note that in this case inflate *requires* an extra "dummy" byte
             * after the compressed stream in order to complete decompression and
             * return Z_STREAM_END.
             * In unzip, i don't wait absolutely Z_STREAM_END because I known the
             * size of both compressed and uncompressed data
             */
        }
    }

    pfile_in_zip_read_info->rest_read_compressed = s->cur_file_info.compressed_size;
    pfile_in_zip_read_info->rest_read_uncompressed = s->cur_file_info.uncompressed_size;
    pfile_in_zip_read_info->pos_in_zipfile = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + size_variable;
    pfile_in_zip_read_info->stream.avail_in = 0;

    s->pfile_in_zip_read = pfile_in_zip_read_info;

#ifndef NOUNCRYPT
    s->pcrc_32_tab = NULL;

    if ((password != NULL) && ((s->cur_file_info.flag & 1) != 0))
    {
        if (ZSEEK64(s->z_filefunc, s->filestream,
                  s->pfile_in_zip_read->pos_in_zipfile + s->pfile_in_zip_read->byte_before_the_zipfile,
                  ZLIB_FILEFUNC_SEEK_SET) != 0)
            return UNZ_INTERNALERROR;
#ifdef HAVE_AES
        if (s->cur_file_info.compression_method == AES_METHOD)
        {
            unsigned char passverify_archive[AES_PWVERIFYSIZE];
            unsigned char passverify_password[AES_PWVERIFYSIZE];
            unsigned char salt_value[AES_MAXSALTLENGTH];
            uint32_t salt_length = 0;

            if ((s->cur_file_info_internal.aes_encryption_mode < 1) ||
                (s->cur_file_info_internal.aes_encryption_mode > 3))
                return UNZ_INTERNALERROR;

            salt_length = SALT_LENGTH(s->cur_file_info_internal.aes_encryption_mode);

            if (ZREAD64(s->z_filefunc, s->filestream, salt_value, salt_length) != salt_length)
                return UNZ_INTERNALERROR;
            if (ZREAD64(s->z_filefunc, s->filestream, passverify_archive, AES_PWVERIFYSIZE) != AES_PWVERIFYSIZE)
                return UNZ_INTERNALERROR;

            fcrypt_init(s->cur_file_info_internal.aes_encryption_mode, (uint8_t *)password,
                (uint32_t)strlen(password), salt_value, passverify_password, &s->pfile_in_zip_read->aes_ctx);

            if (memcmp(passverify_archive, passverify_password, AES_PWVERIFYSIZE) != 0)
                return UNZ_BADPASSWORD;

            s->pfile_in_zip_read->rest_read_compressed -= salt_length + AES_PWVERIFYSIZE;
            s->pfile_in_zip_read->rest_read_compressed -= AES_AUTHCODESIZE;

            s->pfile_in_zip_read->pos_in_zipfile += salt_length + AES_PWVERIFYSIZE;
        }
        else
#endif
        {
            int i;
            s->pcrc_32_tab = (const z_crc_t*)get_crc_table();
            init_keys(password, s->keys, s->pcrc_32_tab);

            if (ZREAD64(s->z_filefunc, s->filestream, source, 12) < 12)
                return UNZ_INTERNALERROR;

            for (i = 0; i < 12; i++)
                zdecode(s->keys, s->pcrc_32_tab, source[i]);

            s->pfile_in_zip_read->rest_read_compressed -= 12;
            s->pfile_in_zip_read->pos_in_zipfile += 12;
        }
    }
#endif

    return UNZ_OK;
}

extern int ZEXPORT unzOpenCurrentFile(unzFile file)
{
    return unzOpenCurrentFile3(file, NULL, NULL, 0, NULL);
}

extern int ZEXPORT unzOpenCurrentFilePassword(unzFile file, const char *password)
{
    return unzOpenCurrentFile3(file, NULL, NULL, 0, password);
}

extern int ZEXPORT unzOpenCurrentFile2(unzFile file, int *method, int *level, int raw)
{
    return unzOpenCurrentFile3(file, method, level, raw, NULL);
}

/* Read bytes from the current file.
   buf contain buffer where data must be copied
   len the size of buf.

   return the number of byte copied if some bytes are copied
   return 0 if the end of file was reached
   return <0 with error code if there is an error (UNZ_ERRNO for IO error, or zLib error for uncompress error) */
extern int ZEXPORT unzReadCurrentFile(unzFile file, voidp buf, uint32_t len)
{
    unz64_s *s = NULL;
    uint32_t read = 0;
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    if (s->pfile_in_zip_read == NULL)
        return UNZ_PARAMERROR;
    if (s->pfile_in_zip_read->read_buffer == NULL)
        return UNZ_END_OF_LIST_OF_FILE;
    if (len == 0)
        return 0;
    if (len > UINT16_MAX)
        return UNZ_PARAMERROR;

    s->pfile_in_zip_read->stream.next_out = (uint8_t*)buf;
    s->pfile_in_zip_read->stream.avail_out = (uint16_t)len;

    if (s->pfile_in_zip_read->raw)
    {
        if (len > s->pfile_in_zip_read->rest_read_compressed + s->pfile_in_zip_read->stream.avail_in)
            s->pfile_in_zip_read->stream.avail_out = (uint16_t)s->pfile_in_zip_read->rest_read_compressed +
                s->pfile_in_zip_read->stream.avail_in;
    }
    else
    {
        if (len > s->pfile_in_zip_read->rest_read_uncompressed)
            s->pfile_in_zip_read->stream.avail_out = (uint16_t)s->pfile_in_zip_read->rest_read_uncompressed;
    }

    while (s->pfile_in_zip_read->stream.avail_out > 0)
    {
        if (s->pfile_in_zip_read->stream.avail_in == 0)
        {
            uint32_t bytes_to_read = UNZ_BUFSIZE;
            uint32_t bytes_not_read = 0;
            uint32_t bytes_read = 0;
            uint32_t total_bytes_read = 0;

            if (s->pfile_in_zip_read->stream.next_in != NULL)
                bytes_not_read = (uint32_t)(s->pfile_in_zip_read->read_buffer + UNZ_BUFSIZE -
                    s->pfile_in_zip_read->stream.next_in);
            bytes_to_read -= bytes_not_read;
            if (bytes_not_read > 0)
                memcpy(s->pfile_in_zip_read->read_buffer, s->pfile_in_zip_read->stream.next_in, bytes_not_read);
            if (s->pfile_in_zip_read->rest_read_compressed < bytes_to_read)
                bytes_to_read = (uint16_t)s->pfile_in_zip_read->rest_read_compressed;

            while (total_bytes_read != bytes_to_read)
            {
                if (ZSEEK64(s->pfile_in_zip_read->z_filefunc, s->pfile_in_zip_read->filestream,
                        s->pfile_in_zip_read->pos_in_zipfile + s->pfile_in_zip_read->byte_before_the_zipfile,
                        ZLIB_FILEFUNC_SEEK_SET) != 0)
                    return UNZ_ERRNO;

                bytes_read = ZREAD64(s->pfile_in_zip_read->z_filefunc, s->pfile_in_zip_read->filestream,
                          s->pfile_in_zip_read->read_buffer + bytes_not_read + total_bytes_read,
                          bytes_to_read - total_bytes_read);

                total_bytes_read += bytes_read;
                s->pfile_in_zip_read->pos_in_zipfile += bytes_read;

                if (bytes_read == 0)
                {
                    if (ZERROR64(s->pfile_in_zip_read->z_filefunc, s->pfile_in_zip_read->filestream))
                        return UNZ_ERRNO;

                    err = unzGoToNextDisk(file);
                    if (err != UNZ_OK)
                        return err;

                    s->pfile_in_zip_read->pos_in_zipfile = 0;
                    s->pfile_in_zip_read->filestream = s->filestream;
                }
            }

#ifndef NOUNCRYPT
            if ((s->cur_file_info.flag & 1) != 0)
            {
#ifdef HAVE_AES
                if (s->cur_file_info.compression_method == AES_METHOD)
                {
                    fcrypt_decrypt(s->pfile_in_zip_read->read_buffer, bytes_to_read, &s->pfile_in_zip_read->aes_ctx);
                }
                else
#endif
                if (s->pcrc_32_tab != NULL)
                {
                    uint32_t i = 0;

                    for (i = 0; i < total_bytes_read; i++)
                      s->pfile_in_zip_read->read_buffer[i] =
                          zdecode(s->keys, s->pcrc_32_tab, s->pfile_in_zip_read->read_buffer[i]);
                }
            }
#endif

            s->pfile_in_zip_read->rest_read_compressed -= total_bytes_read;
            s->pfile_in_zip_read->stream.next_in = (uint8_t*)s->pfile_in_zip_read->read_buffer;
            s->pfile_in_zip_read->stream.avail_in = (uint16_t)(bytes_not_read + total_bytes_read);
        }

        if ((s->pfile_in_zip_read->compression_method == 0) || (s->pfile_in_zip_read->raw))
        {
            uint32_t i = 0;
            uint32_t copy = 0;

            if ((s->pfile_in_zip_read->stream.avail_in == 0) &&
                (s->pfile_in_zip_read->rest_read_compressed == 0))
                return (read == 0) ? UNZ_EOF : read;

            if (s->pfile_in_zip_read->stream.avail_out < s->pfile_in_zip_read->stream.avail_in)
                copy = s->pfile_in_zip_read->stream.avail_out;
            else
                copy = s->pfile_in_zip_read->stream.avail_in;

            for (i = 0; i < copy; i++)
                *(s->pfile_in_zip_read->stream.next_out + i) =
                        *(s->pfile_in_zip_read->stream.next_in + i);

            s->pfile_in_zip_read->total_out_64 = s->pfile_in_zip_read->total_out_64 + copy;
            s->pfile_in_zip_read->rest_read_uncompressed -= copy;
            s->pfile_in_zip_read->crc32 = (uint32_t)crc32(s->pfile_in_zip_read->crc32,
                                s->pfile_in_zip_read->stream.next_out, copy);

            s->pfile_in_zip_read->stream.avail_in -= copy;
            s->pfile_in_zip_read->stream.avail_out -= copy;
            s->pfile_in_zip_read->stream.next_out += copy;
            s->pfile_in_zip_read->stream.next_in += copy;
            s->pfile_in_zip_read->stream.total_out += copy;

            read += copy;
        }
        else if (s->pfile_in_zip_read->compression_method == Z_BZIP2ED)
        {
#ifdef HAVE_BZIP2
            uint64_t total_out_before = 0;
            uint64_t total_out_after = 0;
            uint64_t out_bytes = 0;
            const uint8_t *buf_before = NULL;

            s->pfile_in_zip_read->bstream.next_in        = (char*)s->pfile_in_zip_read->stream.next_in;
            s->pfile_in_zip_read->bstream.avail_in       = s->pfile_in_zip_read->stream.avail_in;
            s->pfile_in_zip_read->bstream.total_in_lo32  = (uint32_t)s->pfile_in_zip_read->stream.total_in;
            s->pfile_in_zip_read->bstream.total_in_hi32  = s->pfile_in_zip_read->stream.total_in >> 32;
            
            s->pfile_in_zip_read->bstream.next_out       = (char*)s->pfile_in_zip_read->stream.next_out;
            s->pfile_in_zip_read->bstream.avail_out      = s->pfile_in_zip_read->stream.avail_out;
            s->pfile_in_zip_read->bstream.total_out_lo32 = (uint32_t)s->pfile_in_zip_read->stream.total_out;
            s->pfile_in_zip_read->bstream.total_out_hi32 = s->pfile_in_zip_read->stream.total_out >> 32;

            total_out_before = s->pfile_in_zip_read->bstream.total_out_lo32 + 
                (((uint32_t)s->pfile_in_zip_read->bstream.total_out_hi32) << 32);
            buf_before = (const uint8_t*)s->pfile_in_zip_read->bstream.next_out;

            err = BZ2_bzDecompress(&s->pfile_in_zip_read->bstream);

            total_out_after = s->pfile_in_zip_read->bstream.total_out_lo32 + 
                (((uint32_t)s->pfile_in_zip_read->bstream.total_out_hi32) << 32);

            out_bytes = total_out_after - total_out_before;

            s->pfile_in_zip_read->total_out_64 = s->pfile_in_zip_read->total_out_64 + out_bytes;
            s->pfile_in_zip_read->rest_read_uncompressed -= out_bytes;
            s->pfile_in_zip_read->crc32 = crc32(s->pfile_in_zip_read->crc32, buf_before, (uint32_t)out_bytes);

            read += (uint32_t)out_bytes;

            s->pfile_in_zip_read->stream.next_in   = (uint8_t*)s->pfile_in_zip_read->bstream.next_in;
            s->pfile_in_zip_read->stream.avail_in  = s->pfile_in_zip_read->bstream.avail_in;
            s->pfile_in_zip_read->stream.total_in  = s->pfile_in_zip_read->bstream.total_in_lo32;
            s->pfile_in_zip_read->stream.next_out  = (uint8_t*)s->pfile_in_zip_read->bstream.next_out;
            s->pfile_in_zip_read->stream.avail_out = s->pfile_in_zip_read->bstream.avail_out;
            s->pfile_in_zip_read->stream.total_out = s->pfile_in_zip_read->bstream.total_out_lo32;

            if (err == BZ_STREAM_END)
                return (read == 0) ? UNZ_EOF : read;
            if (err != BZ_OK)
                break;
#endif
        }
#ifdef HAVE_APPLE_COMPRESSION
        else
        {
            uint64_t total_out_before = 0;
            uint64_t total_out_after = 0;
            uint64_t out_bytes = 0;
            const uint8_t *buf_before = NULL;

            s->pfile_in_zip_read->astream.src_ptr = s->pfile_in_zip_read->stream.next_in;
            s->pfile_in_zip_read->astream.src_size = s->pfile_in_zip_read->stream.avail_in;
            s->pfile_in_zip_read->astream.dst_ptr = s->pfile_in_zip_read->stream.next_out;
            s->pfile_in_zip_read->astream.dst_size = len;

            total_out_before = s->pfile_in_zip_read->stream.total_out;
            buf_before = s->pfile_in_zip_read->stream.next_out;

            compression_status status;
            compression_stream_flags flags;

            if (s->pfile_in_zip_read->stream.avail_in == 0)
            {
                flags = COMPRESSION_STREAM_FINALIZE;
            }

            status = compression_stream_process(&s->pfile_in_zip_read->astream, flags);

            total_out_after = len - s->pfile_in_zip_read->astream.dst_size;
            out_bytes = total_out_after - total_out_before;

            s->pfile_in_zip_read->total_out_64 += out_bytes;
            s->pfile_in_zip_read->rest_read_uncompressed -= out_bytes;
            s->pfile_in_zip_read->crc32 =
                crc32(s->pfile_in_zip_read->crc32, buf_before, (uint32_t)out_bytes);

            read += (uint32_t)out_bytes;

            s->pfile_in_zip_read->stream.next_in = s->pfile_in_zip_read->astream.src_ptr;
            s->pfile_in_zip_read->stream.avail_in = s->pfile_in_zip_read->astream.src_size;
            s->pfile_in_zip_read->stream.next_out = s->pfile_in_zip_read->astream.dst_ptr;
            s->pfile_in_zip_read->stream.avail_out = s->pfile_in_zip_read->astream.dst_size;

            if (status == COMPRESSION_STATUS_END)
                return (read == 0) ? UNZ_EOF : read;
            if (status == COMPRESSION_STATUS_ERROR)
                return Z_DATA_ERROR;
            return read;
        }
#else
        else
        {

            uint64_t total_out_before = 0;
            uint64_t total_out_after = 0;
            uint64_t out_bytes = 0;
            const uint8_t *buf_before = NULL;
            int flush = Z_SYNC_FLUSH;

            total_out_before = s->pfile_in_zip_read->stream.total_out;
            buf_before = s->pfile_in_zip_read->stream.next_out;

            /*
            if ((pfile_in_zip_read_info->rest_read_uncompressed ==
                     pfile_in_zip_read_info->stream.avail_out) &&
                (pfile_in_zip_read_info->rest_read_compressed == 0))
                flush = Z_FINISH;
            */
            err = inflate(&s->pfile_in_zip_read->stream, flush);

            if ((err >= 0) && (s->pfile_in_zip_read->stream.msg != NULL))
                err = Z_DATA_ERROR;

            total_out_after = s->pfile_in_zip_read->stream.total_out;
            out_bytes = total_out_after - total_out_before;

            s->pfile_in_zip_read->total_out_64 += out_bytes;
            s->pfile_in_zip_read->rest_read_uncompressed -= out_bytes;
            s->pfile_in_zip_read->crc32 =
                (uint32_t)crc32(s->pfile_in_zip_read->crc32,buf_before, (uint32_t)out_bytes);

            read += (uint32_t)out_bytes;

            if (err == Z_STREAM_END)
                return (read == 0) ? UNZ_EOF : read;
            if (err != Z_OK)
                break;
        }
#endif
    }

    if (err == Z_OK)
        return read;
    return err;
}

extern int ZEXPORT unzGetLocalExtrafield(unzFile file, voidp buf, uint32_t len)
{
    unz64_s *s = NULL;
    uint64_t size_to_read = 0;
    uint32_t read_now = 0;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (s->pfile_in_zip_read == NULL)
        return UNZ_PARAMERROR;

    size_to_read = s->pfile_in_zip_read->size_local_extrafield - s->pfile_in_zip_read->pos_local_extrafield;

    if (buf == NULL)
        return (int)size_to_read;

    if (len > size_to_read)
        read_now = (uint32_t)size_to_read;
    else
        read_now = len;

    if (read_now == 0)
        return 0;

    if (ZSEEK64(s->pfile_in_zip_read->z_filefunc, s->pfile_in_zip_read->filestream,
        s->pfile_in_zip_read->offset_local_extrafield + s->pfile_in_zip_read->pos_local_extrafield,
        ZLIB_FILEFUNC_SEEK_SET) != 0)
        return UNZ_ERRNO;

    if (ZREAD64(s->pfile_in_zip_read->z_filefunc, s->pfile_in_zip_read->filestream, buf, read_now) != read_now)
        return UNZ_ERRNO;

    return (int)read_now;
}

extern int ZEXPORT unzCloseCurrentFile(unzFile file)
{
    unz64_s *s = NULL;
    file_in_zip64_read_info_s *pfile_in_zip_read_info = NULL;
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    pfile_in_zip_read_info = s->pfile_in_zip_read;
    if (pfile_in_zip_read_info == NULL)
        return UNZ_PARAMERROR;

#ifdef HAVE_AES
    if (s->cur_file_info.compression_method == AES_METHOD)
    {
        unsigned char authcode[AES_AUTHCODESIZE];
        unsigned char rauthcode[AES_AUTHCODESIZE];

        if (ZREAD64(s->z_filefunc, s->filestream, authcode, AES_AUTHCODESIZE) != AES_AUTHCODESIZE)
            return UNZ_ERRNO;

        if (fcrypt_end(rauthcode, &s->pfile_in_zip_read->aes_ctx) != AES_AUTHCODESIZE)
            err = UNZ_CRCERROR;
        if (memcmp(authcode, rauthcode, AES_AUTHCODESIZE) != 0)
            err = UNZ_CRCERROR;
    }
    /* AES zip version AE-1 will expect a valid crc as well */
    if ((s->cur_file_info.compression_method != AES_METHOD) ||
        (s->cur_file_info_internal.aes_version == 0x0001))
#endif
    {
        if ((pfile_in_zip_read_info->rest_read_uncompressed == 0) &&
            (!pfile_in_zip_read_info->raw))
        {
            if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
                err = UNZ_CRCERROR;
        }
    }

    TRYFREE(pfile_in_zip_read_info->read_buffer);
    pfile_in_zip_read_info->read_buffer = NULL;
    if (pfile_in_zip_read_info->stream_initialised == Z_DEFLATED)
    {
#ifdef HAVE_APPLE_COMPRESSION
        if (compression_stream_destroy)
            compression_stream_destroy(&pfile_in_zip_read_info->astream);
#else
        inflateEnd(&pfile_in_zip_read_info->stream);
#endif
        
    }
#ifdef HAVE_BZIP2
    else if (pfile_in_zip_read_info->stream_initialised == Z_BZIP2ED)
        BZ2_bzDecompressEnd(&pfile_in_zip_read_info->bstream);
#endif

    pfile_in_zip_read_info->stream_initialised = 0;
    TRYFREE(pfile_in_zip_read_info);

    s->pfile_in_zip_read = NULL;

    return err;
}

extern int ZEXPORT unzGoToFirstFile2(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    unz64_s *s = NULL;
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    s->pos_in_central_dir = s->offset_central_dir;
    s->num_file = 0;

    err = unzGetCurrentFileInfoInternal(file, &s->cur_file_info, &s->cur_file_info_internal,
            filename, filename_size, extrafield, extrafield_size, comment,comment_size);

    s->current_file_ok = (err == UNZ_OK);
    if ((err == UNZ_OK) && (pfile_info != NULL))
        memcpy(pfile_info, &s->cur_file_info, sizeof(unz_file_info64));

    return err;
}

extern int ZEXPORT unzGoToFirstFile(unzFile file)
{
    return unzGoToFirstFile2(file, NULL, NULL, 0, NULL, 0, NULL, 0);
}

extern int ZEXPORT unzGoToNextFile2(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    unz64_s *s = NULL;
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    if (!s->current_file_ok)
        return UNZ_END_OF_LIST_OF_FILE;
    if (s->gi.number_entry != UINT16_MAX)    /* 2^16 files overflow hack */
    {
        if (s->num_file+1 == s->gi.number_entry)
            return UNZ_END_OF_LIST_OF_FILE;
    }

    s->pos_in_central_dir += SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
            s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment;
    s->num_file += 1;

    err = unzGetCurrentFileInfoInternal(file, &s->cur_file_info, &s->cur_file_info_internal,
            filename, filename_size, extrafield,extrafield_size, comment, comment_size);

    s->current_file_ok = (err == UNZ_OK);
    if ((err == UNZ_OK) && (pfile_info != NULL))
        memcpy(pfile_info, &s->cur_file_info, sizeof(unz_file_info64));

    return err;
}

extern int ZEXPORT unzGoToNextFile(unzFile file)
{
    return unzGoToNextFile2(file, NULL, NULL, 0, NULL, 0, NULL, 0);
}

extern int ZEXPORT unzLocateFile(unzFile file, const char *filename, unzFileNameComparer filename_compare_func)
{
    unz64_s *s = NULL;
    unz_file_info64 cur_file_info_saved;
    unz_file_info64_internal cur_file_info_internal_saved;
    uint64_t num_file_saved = 0;
    uint64_t pos_in_central_dir_saved = 0;
    char current_filename[UNZ_MAXFILENAMEINZIP+1];
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    if (strlen(filename) >= UNZ_MAXFILENAMEINZIP)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (!s->current_file_ok)
        return UNZ_END_OF_LIST_OF_FILE;

    /* Save the current state */
    num_file_saved = s->num_file;
    pos_in_central_dir_saved = s->pos_in_central_dir;
    cur_file_info_saved = s->cur_file_info;
    cur_file_info_internal_saved = s->cur_file_info_internal;

    err = unzGoToFirstFile2(file, NULL, current_filename, sizeof(current_filename)-1, NULL, 0, NULL, 0);

    while (err == UNZ_OK)
    {
        if (filename_compare_func != NULL)
            err = filename_compare_func(file, current_filename, filename);
        else
            err = strcmp(current_filename, filename);
        if (err == 0)
            return UNZ_OK;
        err = unzGoToNextFile2(file, NULL, current_filename, sizeof(current_filename)-1, NULL, 0, NULL, 0);
    }

    /* We failed, so restore the state of the 'current file' to where we were. */
    s->num_file = num_file_saved;
    s->pos_in_central_dir = pos_in_central_dir_saved;
    s->cur_file_info = cur_file_info_saved;
    s->cur_file_info_internal = cur_file_info_internal_saved;
    return err;
}

extern int ZEXPORT unzGetFilePos(unzFile file, unz_file_pos *file_pos)
{
    unz64_file_pos file_pos64;
    int err = unzGetFilePos64(file, &file_pos64);
    if (err == UNZ_OK)
    {
        file_pos->pos_in_zip_directory = (uint32_t)file_pos64.pos_in_zip_directory;
        file_pos->num_of_file = (uint32_t)file_pos64.num_of_file;
    }
    return err;
}

extern int ZEXPORT unzGoToFilePos(unzFile file, unz_file_pos *file_pos)
{
    unz64_file_pos file_pos64;
    if (file_pos == NULL)
        return UNZ_PARAMERROR;
    file_pos64.pos_in_zip_directory = file_pos->pos_in_zip_directory;
    file_pos64.num_of_file = file_pos->num_of_file;
    return unzGoToFilePos64(file, &file_pos64);
}

extern int ZEXPORT unzGetFilePos64(unzFile file, unz64_file_pos *file_pos)
{
    unz64_s *s = NULL;

    if (file == NULL || file_pos == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (!s->current_file_ok)
        return UNZ_END_OF_LIST_OF_FILE;

    file_pos->pos_in_zip_directory  = s->pos_in_central_dir;
    file_pos->num_of_file = s->num_file;
    return UNZ_OK;
}

extern int ZEXPORT unzGoToFilePos64(unzFile file, const unz64_file_pos *file_pos)
{
    unz64_s *s = NULL;
    int err = UNZ_OK;

    if (file == NULL || file_pos == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    /* Jump to the right spot */
    s->pos_in_central_dir = file_pos->pos_in_zip_directory;
    s->num_file = file_pos->num_of_file;

    /* Set the current file */
    err = unzGetCurrentFileInfoInternal(file, &s->cur_file_info, &s->cur_file_info_internal, NULL, 0, NULL, 0, NULL, 0);
    /* Return results */
    s->current_file_ok = (err == UNZ_OK);
    return err;
}

extern int32_t ZEXPORT unzGetOffset(unzFile file)
{
    uint64_t offset64 = 0;

    if (file == NULL)
        return UNZ_PARAMERROR;
    offset64 = unzGetOffset64(file);
    return (int32_t)offset64;
}

extern int64_t ZEXPORT unzGetOffset64(unzFile file)
{
    unz64_s *s = NULL;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (!s->current_file_ok)
        return 0;
    if (s->gi.number_entry != 0 && s->gi.number_entry != UINT16_MAX)
    {
        if (s->num_file == s->gi.number_entry)
            return 0;
    }
    return s->pos_in_central_dir;
}

extern int ZEXPORT unzSetOffset(unzFile file, uint32_t pos)
{
    return unzSetOffset64(file, pos);
}

extern int ZEXPORT unzSetOffset64(unzFile file, uint64_t pos)
{
    unz64_s *s = NULL;
    int err = UNZ_OK;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    s->pos_in_central_dir = pos;
    s->num_file = s->gi.number_entry; /* hack */

    err = unzGetCurrentFileInfoInternal(file, &s->cur_file_info, &s->cur_file_info_internal, NULL, 0, NULL, 0, NULL, 0);

    s->current_file_ok = (err == UNZ_OK);
    return err;
}

extern int32_t ZEXPORT unzTell(unzFile file)
{
    unz64_s *s = NULL;
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (s->pfile_in_zip_read == NULL)
        return UNZ_PARAMERROR;
    return (int32_t)s->pfile_in_zip_read->stream.total_out;
}

extern int64_t ZEXPORT unzTell64(unzFile file)
{
    unz64_s *s = NULL;
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (s->pfile_in_zip_read == NULL)
        return UNZ_PARAMERROR;
    return s->pfile_in_zip_read->total_out_64;
}

extern int ZEXPORT unzSeek(unzFile file, uint32_t offset, int origin)
{
    return unzSeek64(file, offset, origin);
}

extern int ZEXPORT unzSeek64(unzFile file, uint64_t offset, int origin)
{
    unz64_s *s = NULL;
    uint64_t stream_pos_begin = 0;
    uint64_t stream_pos_end = 0;
    uint64_t position = 0;
    int is_within_buffer = 0;

    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;

    if (s->pfile_in_zip_read == NULL)
        return UNZ_ERRNO;
    if (s->pfile_in_zip_read->compression_method != 0)
        return UNZ_ERRNO;

    if (origin == SEEK_SET)
        position = offset;
    else if (origin == SEEK_CUR)
        position = s->pfile_in_zip_read->total_out_64 + offset;
    else if (origin == SEEK_END)
        position = s->cur_file_info.compressed_size + offset;
    else
        return UNZ_PARAMERROR;

    if (position > s->cur_file_info.compressed_size)
        return UNZ_PARAMERROR;

    stream_pos_end = s->pfile_in_zip_read->pos_in_zipfile;
    stream_pos_begin = stream_pos_end;

    if (stream_pos_begin > UNZ_BUFSIZE)
        stream_pos_begin -= UNZ_BUFSIZE;
    else
        stream_pos_begin = 0;

    is_within_buffer = 
        (s->pfile_in_zip_read->stream.avail_in != 0) &&
        (s->pfile_in_zip_read->rest_read_compressed != 0 || s->cur_file_info.compressed_size < UNZ_BUFSIZE) &&
        (position >= stream_pos_begin && position < stream_pos_end);

    if (is_within_buffer)
    {
        s->pfile_in_zip_read->stream.next_in += position - s->pfile_in_zip_read->total_out_64;
        s->pfile_in_zip_read->stream.avail_in = (uInt)(stream_pos_end - position);
    }
    else
    {
        s->pfile_in_zip_read->stream.avail_in = 0;
        s->pfile_in_zip_read->stream.next_in = 0;

        s->pfile_in_zip_read->pos_in_zipfile = s->pfile_in_zip_read->offset_local_extrafield + position;
        s->pfile_in_zip_read->rest_read_compressed = s->cur_file_info.compressed_size - position;
    }

    s->pfile_in_zip_read->rest_read_uncompressed -= (position - s->pfile_in_zip_read->total_out_64);
    s->pfile_in_zip_read->stream.total_out = (uint32_t)position;
    s->pfile_in_zip_read->total_out_64 = position;

    return UNZ_OK;
}

extern int ZEXPORT unzEndOfFile(unzFile file)
{
    unz64_s *s = NULL;
    if (file == NULL)
        return UNZ_PARAMERROR;
    s = (unz64_s*)file;
    if (s->pfile_in_zip_read == NULL)
        return UNZ_PARAMERROR;
    if (s->pfile_in_zip_read->rest_read_uncompressed == 0)
        return 1;
    return 0;
}
