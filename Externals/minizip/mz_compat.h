/* mz_compat.h -- Backwards compatible interface for older versions
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_COMPAT_H
#define MZ_COMPAT_H

#include "mz.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

#if defined(HAVE_ZLIB) && defined(MAX_MEM_LEVEL)
#ifndef DEF_MEM_LEVEL
#  if MAX_MEM_LEVEL >= 8
#    define DEF_MEM_LEVEL 8
#  else
#    define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#  endif
#endif
#endif
#ifndef MAX_WBITS
#define MAX_WBITS     (15)
#endif
#ifndef DEF_MEM_LEVEL
#define DEF_MEM_LEVEL (8)
#endif

#ifndef ZEXPORT
#  define ZEXPORT MZ_EXPORT
#endif

/***************************************************************************/

#if defined(STRICTZIP) || defined(STRICTZIPUNZIP)
/* like the STRICT of WIN32, we define a pointer that cannot be converted
    from (void*) without cast */
typedef struct TagzipFile__ { int unused; } zip_file__;
typedef zip_file__ *zipFile;
#else
typedef void *zipFile;
#endif

/***************************************************************************/

typedef uint64_t ZPOS64_T;

#ifndef ZCALLBACK
#define ZCALLBACK
#endif

typedef void*         (ZCALLBACK *open_file_func)     (void *opaque, const char *filename, int mode);
typedef void*         (ZCALLBACK *open64_file_func)   (void *opaque, const void *filename, int mode);
typedef unsigned long (ZCALLBACK *read_file_func)     (void *opaque, void *stream, void* buf, unsigned long size);
typedef unsigned long (ZCALLBACK *write_file_func)    (void *opaque, void *stream, const void* buf, 
                                                       unsigned long size);
typedef int           (ZCALLBACK *close_file_func)    (void *opaque, void *stream);
typedef int           (ZCALLBACK *testerror_file_func)(void *opaque, void *stream);
typedef long          (ZCALLBACK *tell_file_func)     (void *opaque, void *stream);
typedef ZPOS64_T      (ZCALLBACK *tell64_file_func)   (void *opaque, void *stream);
typedef long          (ZCALLBACK *seek_file_func)     (void *opaque, void *stream, unsigned long offset, int origin);
typedef long          (ZCALLBACK *seek64_file_func)   (void *opaque, void *stream, ZPOS64_T offset, int origin);

typedef struct zlib_filefunc_def_s
{
    open_file_func      zopen_file;
    read_file_func      zread_file;
    write_file_func     zwrite_file;
    tell_file_func      ztell_file;
    seek_file_func      zseek_file;
    close_file_func     zclose_file;
    testerror_file_func zerror_file;
    void*               opaque;
} zlib_filefunc_def;

typedef struct zlib_filefunc64_def_s
{
    open64_file_func    zopen64_file;
    read_file_func      zread_file;
    write_file_func     zwrite_file;
    tell64_file_func    ztell64_file;
    seek64_file_func    zseek64_file;
    close_file_func     zclose_file;
    testerror_file_func zerror_file;
    void*               opaque;
} zlib_filefunc64_def;

/***************************************************************************/

#define ZLIB_FILEFUNC_SEEK_SET              (0)
#define ZLIB_FILEFUNC_SEEK_CUR              (1)
#define ZLIB_FILEFUNC_SEEK_END              (2)

#define ZLIB_FILEFUNC_MODE_READ             (1)
#define ZLIB_FILEFUNC_MODE_WRITE            (2)
#define ZLIB_FILEFUNC_MODE_READWRITEFILTER  (3)

#define ZLIB_FILEFUNC_MODE_EXISTING         (4)
#define ZLIB_FILEFUNC_MODE_CREATE           (8)

/***************************************************************************/

ZEXPORT void fill_fopen_filefunc(zlib_filefunc_def *pzlib_filefunc_def);
ZEXPORT void fill_fopen64_filefunc(zlib_filefunc64_def *pzlib_filefunc_def);
ZEXPORT void fill_win32_filefunc(zlib_filefunc_def *pzlib_filefunc_def);
ZEXPORT void fill_win32_filefunc64(zlib_filefunc64_def *pzlib_filefunc_def);
ZEXPORT void fill_win32_filefunc64A(zlib_filefunc64_def *pzlib_filefunc_def);
ZEXPORT void fill_memory_filefunc(zlib_filefunc_def *pzlib_filefunc_def);

/***************************************************************************/

#if MZ_COMPAT_VERSION <= 110
#define mz_dos_date dosDate
#else
#define mz_dos_date dos_date
#endif

typedef struct tm tm_unz;
typedef struct tm tm_zip;

typedef struct {
    uint32_t    mz_dos_date;
    struct tm   tmz_date;
    uint16_t    internal_fa;        /* internal file attributes        2 bytes */
    uint32_t    external_fa;        /* external file attributes        4 bytes */
} zip_fileinfo;

typedef const char *zipcharpc;

/***************************************************************************/

#define ZIP_OK                          (0)
#define ZIP_EOF                         (0)
#define ZIP_ERRNO                       (-1)
#define ZIP_PARAMERROR                  (-102)
#define ZIP_BADZIPFILE                  (-103)
#define ZIP_INTERNALERROR               (-104)

#ifndef Z_DEFLATED
#define Z_DEFLATED                      (8)
#endif
#define Z_BZIP2ED                       (12)

#define APPEND_STATUS_CREATE            (0)
#define APPEND_STATUS_CREATEAFTER       (1)
#define APPEND_STATUS_ADDINZIP          (2)

/***************************************************************************/
/* Writing a zip file  */

ZEXPORT zipFile zipOpen(const char *path, int append);
ZEXPORT zipFile zipOpen64(const void *path, int append);
ZEXPORT zipFile zipOpen2(const char *path, int append, const char **globalcomment,
    zlib_filefunc_def *pzlib_filefunc_def);

ZEXPORT zipFile zipOpen2_64(const void *path, int append, const char **globalcomment,
    zlib_filefunc64_def *pzlib_filefunc_def);
ZEXPORT zipFile zipOpen_MZ(void *stream, int append, const char **globalcomment);

ZEXPORT void*   zipGetHandle_MZ(zipFile);
ZEXPORT void*   zipGetStream_MZ(zipFile file);

ZEXPORT int     zipOpenNewFileInZip(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level);
ZEXPORT int     zipOpenNewFileInZip_64(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int zip64);
ZEXPORT int     zipOpenNewFileInZip2(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw);
ZEXPORT int     zipOpenNewFileInZip2_64(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw, int zip64);
ZEXPORT int     zipOpenNewFileInZip3(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    unsigned long crc_for_crypting);
ZEXPORT int     zipOpenNewFileInZip3_64(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    uint32_t crc_for_crypting, int zip64);
ZEXPORT int     zipOpenNewFileInZip4(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    unsigned long crc_for_crypting, unsigned long version_madeby, unsigned long flag_base);
ZEXPORT int     zipOpenNewFileInZip4_64(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    unsigned long crc_for_crypting, unsigned long version_madeby, unsigned long flag_base, int zip64);
ZEXPORT int     zipOpenNewFileInZip5(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, int compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    unsigned long crc_for_crypting, unsigned long version_madeby, unsigned long flag_base, int zip64);

ZEXPORT int     zipWriteInFileInZip(zipFile file, const void *buf, uint32_t len);

ZEXPORT int     zipCloseFileInZipRaw(zipFile file, unsigned long uncompressed_size, unsigned long crc32);
ZEXPORT int     zipCloseFileInZipRaw64(zipFile file, uint64_t uncompressed_size, unsigned long crc32);
ZEXPORT int     zipCloseFileInZip(zipFile file);
ZEXPORT int     zipCloseFileInZip64(zipFile file);

ZEXPORT int     zipClose(zipFile file, const char *global_comment);
ZEXPORT int     zipClose_64(zipFile file, const char *global_comment);
ZEXPORT int     zipClose2_64(zipFile file, const char *global_comment, uint16_t version_madeby);
        int     zipClose_MZ(zipFile file, const char *global_comment);
        int     zipClose2_MZ(zipFile file, const char *global_comment, uint16_t version_madeby);

/***************************************************************************/

#if defined(STRICTUNZIP) || defined(STRICTZIPUNZIP)
/* like the STRICT of WIN32, we define a pointer that cannot be converted
    from (void*) without cast */
typedef struct TagunzFile__ { int unused; } unz_file__;
typedef unz_file__ *unzFile;
#else
typedef void *unzFile;
#endif

/***************************************************************************/

#define UNZ_OK                          (0)
#define UNZ_END_OF_LIST_OF_FILE         (-100)
#define UNZ_ERRNO                       (-1)
#define UNZ_EOF                         (0)
#define UNZ_PARAMERROR                  (-102)
#define UNZ_BADZIPFILE                  (-103)
#define UNZ_INTERNALERROR               (-104)
#define UNZ_CRCERROR                    (-105)
#define UNZ_BADPASSWORD                 (-106)

/***************************************************************************/

typedef struct unz_global_info64_s {
    uint64_t number_entry;          /* total number of entries in the central dir on this disk */
    uint32_t number_disk_with_CD;   /* number the the disk with central dir, used for spanning ZIP */
    uint16_t size_comment;          /* size of the global comment of the zipfile */
} unz_global_info64;

typedef struct unz_global_info_s {
    uint32_t number_entry;          /* total number of entries in the central dir on this disk */
    uint32_t number_disk_with_CD;   /* number the the disk with central dir, used for spanning ZIP */
    uint16_t size_comment;          /* size of the global comment of the zipfile */
} unz_global_info;

typedef struct unz_file_info64_s {
    uint16_t version;               /* version made by                 2 bytes */
    uint16_t version_needed;        /* version needed to extract       2 bytes */
    uint16_t flag;                  /* general purpose bit flag        2 bytes */
    uint16_t compression_method;    /* compression method              2 bytes */
    uint32_t mz_dos_date;           /* last mod file date in Dos fmt   4 bytes */
    struct tm tmu_date;
    uint32_t crc;                   /* crc-32                          4 bytes */
    uint64_t compressed_size;       /* compressed size                 8 bytes */
    uint64_t uncompressed_size;     /* uncompressed size               8 bytes */
    uint16_t size_filename;         /* filename length                 2 bytes */
    uint16_t size_file_extra;       /* extra field length              2 bytes */
    uint16_t size_file_comment;     /* file comment length             2 bytes */

    uint32_t disk_num_start;        /* disk number start               4 bytes */
    uint16_t internal_fa;           /* internal file attributes        2 bytes */
    uint32_t external_fa;           /* external file attributes        4 bytes */

    uint64_t disk_offset;

    uint16_t size_file_extra_internal;
} unz_file_info64;

typedef struct unz_file_info_s {
    uint16_t version;               /* version made by                 2 bytes */
    uint16_t version_needed;        /* version needed to extract       2 bytes */
    uint16_t flag;                  /* general purpose bit flag        2 bytes */
    uint16_t compression_method;    /* compression method              2 bytes */
    uint32_t mz_dos_date;           /* last mod file date in Dos fmt   4 bytes */
    struct tm tmu_date;
    uint32_t crc;                   /* crc-32                          4 bytes */
    uint32_t compressed_size;       /* compressed size                 4 bytes */
    uint32_t uncompressed_size;     /* uncompressed size               4 bytes */
    uint16_t size_filename;         /* filename length                 2 bytes */
    uint16_t size_file_extra;       /* extra field length              2 bytes */
    uint16_t size_file_comment;     /* file comment length             2 bytes */

    uint16_t disk_num_start;        /* disk number start               2 bytes */
    uint16_t internal_fa;           /* internal file attributes        2 bytes */
    uint32_t external_fa;           /* external file attributes        4 bytes */

    uint64_t disk_offset;
} unz_file_info;

/***************************************************************************/

typedef int (*unzFileNameComparer)(unzFile file, const char *filename1, const char *filename2);
typedef int (*unzIteratorFunction)(unzFile file);
typedef int (*unzIteratorFunction2)(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment,
    uint16_t comment_size);

/***************************************************************************/
/* Reading a zip file */

ZEXPORT unzFile unzOpen(const char *path);
ZEXPORT unzFile unzOpen64(const void *path);
ZEXPORT unzFile unzOpen2(const char *path, zlib_filefunc_def *pzlib_filefunc_def);
ZEXPORT unzFile unzOpen2_64(const void *path, zlib_filefunc64_def *pzlib_filefunc_def);
        unzFile unzOpen_MZ(void *stream);

ZEXPORT int     unzClose(unzFile file);
ZEXPORT int     unzClose_MZ(unzFile file);

ZEXPORT void*   unzGetHandle_MZ(unzFile file);
ZEXPORT void*   unzGetStream_MZ(zipFile file);

ZEXPORT int     unzGetGlobalInfo(unzFile file, unz_global_info* pglobal_info32);
ZEXPORT int     unzGetGlobalInfo64(unzFile file, unz_global_info64 *pglobal_info);
ZEXPORT int     unzGetGlobalComment(unzFile file, char *comment, unsigned long comment_size);

ZEXPORT int     unzOpenCurrentFile(unzFile file);
ZEXPORT int     unzOpenCurrentFilePassword(unzFile file, const char *password);
ZEXPORT int     unzOpenCurrentFile2(unzFile file, int *method, int *level, int raw);
ZEXPORT int     unzOpenCurrentFile3(unzFile file, int *method, int *level, int raw, const char *password);
ZEXPORT int     unzReadCurrentFile(unzFile file, void *buf, uint32_t len);
ZEXPORT int     unzCloseCurrentFile(unzFile file);

ZEXPORT int     unzGetCurrentFileInfo(unzFile file, unz_file_info *pfile_info, char *filename,
    unsigned long filename_size, void *extrafield, unsigned long extrafield_size, char *comment,
    unsigned long comment_size);
ZEXPORT int     unzGetCurrentFileInfo64(unzFile file, unz_file_info64 * pfile_info, char *filename,
    unsigned long filename_size, void *extrafield, unsigned long extrafield_size, char *comment,
    unsigned long comment_size);

ZEXPORT int     unzGoToFirstFile(unzFile file);
ZEXPORT int     unzGoToNextFile(unzFile file);
ZEXPORT int     unzLocateFile(unzFile file, const char *filename, unzFileNameComparer filename_compare_func);

ZEXPORT int     unzGetLocalExtrafield(unzFile file, void *buf, unsigned int len);

/***************************************************************************/
/* Raw access to zip file */

typedef struct unz_file_pos_s {
    uint32_t pos_in_zip_directory;  /* offset in zip file directory */
    uint32_t num_of_file;           /* # of file */
} unz_file_pos;

ZEXPORT int     unzGetFilePos(unzFile file, unz_file_pos *file_pos);
ZEXPORT int     unzGoToFilePos(unzFile file, unz_file_pos *file_pos);

typedef struct unz64_file_pos_s {
    int64_t  pos_in_zip_directory;   /* offset in zip file directory  */
    uint64_t num_of_file;            /* # of file */
} unz64_file_pos;

ZEXPORT int     unzGetFilePos64(unzFile file, unz64_file_pos *file_pos);
ZEXPORT int     unzGoToFilePos64(unzFile file, const unz64_file_pos *file_pos);

ZEXPORT int64_t unzGetOffset64(unzFile file);
ZEXPORT unsigned long
                unzGetOffset(unzFile file);
ZEXPORT int     unzSetOffset64(unzFile file, int64_t pos);
ZEXPORT int     unzSetOffset(unzFile file, unsigned long pos);
ZEXPORT int32_t unztell(unzFile file);
ZEXPORT int32_t unzTell(unzFile file);
ZEXPORT uint64_t unztell64(unzFile file);
ZEXPORT uint64_t unzTell64(unzFile file);
ZEXPORT int     unzSeek(unzFile file, int32_t offset, int origin);
ZEXPORT int     unzSeek64(unzFile file, int64_t offset, int origin);
ZEXPORT int     unzEndOfFile(unzFile file);
ZEXPORT int     unzeof(unzFile file);
ZEXPORT void*   unzGetStream(unzFile file);

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
