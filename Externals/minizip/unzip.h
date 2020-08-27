/* unzip.h -- IO for uncompress .zip files using zlib
   Version 1.1, February 14h, 2010
   part of the MiniZip project

   Copyright (C) 1998-2010 Gilles Vollant
     http://www.winimage.com/zLibDll/minizip.html
   Modifications of Unzip for Zip64
     Copyright (C) 2007-2008 Even Rouault
   Modifications for Zip64 support on both zip and unzip
     Copyright (C) 2009-2010 Mathias Svensson
     http://result42.com

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef _UNZ_H
#define _UNZ_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ZLIB_H
#include "zlib.h"
#endif

#ifndef _ZLIBIOAPI_H
#include "ioapi.h"
#endif

#ifdef HAVE_BZIP2
#include "bzlib.h"
#endif

#define Z_BZIP2ED 12

#if defined(STRICTUNZIP) || defined(STRICTZIPUNZIP)
/* like the STRICT of WIN32, we define a pointer that cannot be converted
    from (void*) without cast */
typedef struct TagunzFile__ { int unused; } unz_file__;
typedef unz_file__ *unzFile;
#else
typedef voidp unzFile;
#endif

#define UNZ_OK                          (0)
#define UNZ_END_OF_LIST_OF_FILE         (-100)
#define UNZ_ERRNO                       (Z_ERRNO)
#define UNZ_EOF                         (0)
#define UNZ_PARAMERROR                  (-102)
#define UNZ_BADZIPFILE                  (-103)
#define UNZ_INTERNALERROR               (-104)
#define UNZ_CRCERROR                    (-105)
#define UNZ_BADPASSWORD                 (-106)

/* unz_global_info structure contain global data about the ZIPfile
   These data comes from the end of central dir */
typedef struct unz_global_info64_s
{
    uint64_t number_entry;          /* total number of entries in the central dir on this disk */
    uint32_t number_disk_with_CD;   /* number the the disk with central dir, used for spanning ZIP*/
    uint16_t size_comment;          /* size of the global comment of the zipfile */
} unz_global_info64;

typedef struct unz_global_info_s
{
    uint32_t number_entry;          /* total number of entries in the central dir on this disk */
    uint32_t number_disk_with_CD;   /* number the the disk with central dir, used for spanning ZIP*/
    uint16_t size_comment;          /* size of the global comment of the zipfile */
} unz_global_info;

/* unz_file_info contain information about a file in the zipfile */
typedef struct unz_file_info64_s
{
    uint16_t version;               /* version made by                 2 bytes */
    uint16_t version_needed;        /* version needed to extract       2 bytes */
    uint16_t flag;                  /* general purpose bit flag        2 bytes */
    uint16_t compression_method;    /* compression method              2 bytes */
    uint32_t dos_date;              /* last mod file date in Dos fmt   4 bytes */
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

typedef struct unz_file_info_s
{
    uint16_t version;               /* version made by                 2 bytes */
    uint16_t version_needed;        /* version needed to extract       2 bytes */
    uint16_t flag;                  /* general purpose bit flag        2 bytes */
    uint16_t compression_method;    /* compression method              2 bytes */
    uint32_t dos_date;              /* last mod file date in Dos fmt   4 bytes */
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
/* Opening and close a zip file */

extern unzFile ZEXPORT unzOpen(const char *path);
extern unzFile ZEXPORT unzOpen64(const void *path);
/* Open a Zip file.

   path should contain the full path (by example, on a Windows XP computer 
      "c:\\zlib\\zlib113.zip" or on an Unix computer "zlib/zlib113.zip". 
   return NULL if zipfile cannot be opened or doesn't exist
   return unzFile handle if no error

   NOTE: The "64" function take a const void *pointer, because  the path is just the value passed to the
   open64_file_func callback. Under Windows, if UNICODE is defined, using fill_fopen64_filefunc, the path 
   is a pointer to a wide unicode string  (LPCTSTR is LPCWSTR), so const char *does not describe the reality */

extern unzFile ZEXPORT unzOpen2(const char *path, zlib_filefunc_def *pzlib_filefunc_def);
/* Open a Zip file, like unzOpen, but provide a set of file low level API for read/write operations */
extern unzFile ZEXPORT unzOpen2_64(const void *path, zlib_filefunc64_def *pzlib_filefunc_def);
/* Open a Zip file, like unz64Open, but provide a set of file low level API for read/write 64-bit operations */

extern int ZEXPORT unzClose(unzFile file);
/* Close a ZipFile opened with unzOpen. If there is files inside the .Zip opened with unzOpenCurrentFile,
   these files MUST be closed with unzipCloseCurrentFile before call unzipClose.

   return UNZ_OK if there is no error */

extern int ZEXPORT unzGetGlobalInfo(unzFile file, unz_global_info *pglobal_info);
extern int ZEXPORT unzGetGlobalInfo64(unzFile file, unz_global_info64 *pglobal_info);
/* Write info about the ZipFile in the *pglobal_info structure.

   return UNZ_OK if no error */

extern int ZEXPORT unzGetGlobalComment(unzFile file, char *comment, uint16_t comment_size);
/* Get the global comment string of the ZipFile, in the comment buffer.

   uSizeBuf is the size of the szComment buffer.
   return the number of byte copied or an error code <0 */

/***************************************************************************/
/* Reading the content of the current zipfile, you can open it, read data from it, and close it
   (you can close it before reading all the file) */

extern int ZEXPORT unzOpenCurrentFile(unzFile file);
/* Open for reading data the current file in the zipfile.

   return UNZ_OK if no error */

extern int ZEXPORT unzOpenCurrentFilePassword(unzFile file, const char *password);
/* Open for reading data the current file in the zipfile.
   password is a crypting password

   return UNZ_OK if no error */

extern int ZEXPORT unzOpenCurrentFile2(unzFile file, int *method, int *level, int raw);
/* Same as unzOpenCurrentFile, but open for read raw the file (not uncompress)
   if raw==1 *method will receive method of compression, *level will receive level of compression

   NOTE: you can set level parameter as NULL (if you did not want known level,
         but you CANNOT set method parameter as NULL */

extern int ZEXPORT unzOpenCurrentFile3(unzFile file, int *method, int *level, int raw, const char *password);
/* Same as unzOpenCurrentFile, but takes extra parameter password for encrypted files */

extern int ZEXPORT unzReadCurrentFile(unzFile file, voidp buf, uint32_t len);
/* Read bytes from the current file (opened by unzOpenCurrentFile)
   buf contain buffer where data must be copied
   len the size of buf.

   return the number of byte copied if somes bytes are copied
   return 0 if the end of file was reached
   return <0 with error code if there is an error (UNZ_ERRNO for IO error, or zLib error for uncompress error) */

extern int ZEXPORT unzGetCurrentFileInfo(unzFile file, unz_file_info *pfile_info, char *filename, 
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size);
extern int ZEXPORT unzGetCurrentFileInfo64(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size);
/* Get Info about the current file

   pfile_info if != NULL, the *pfile_info structure will contain somes info about the current file
   filename if != NULL, the file name string will be copied in filename 
   filename_size is the size of the filename buffer
   extrafield if != NULL, the extra field information from the central header will be copied in to
   extrafield_size is the size of the extraField buffer 
   comment if != NULL, the comment string of the file will be copied in to
   comment_size is the size of the comment buffer */

extern int ZEXPORT unzGetLocalExtrafield(unzFile file, voidp buf, uint32_t len);
/* Read extra field from the current file (opened by unzOpenCurrentFile)
   This is the local-header version of the extra field (sometimes, there is
   more info in the local-header version than in the central-header)

   if buf == NULL, it return the size of the local extra field
   if buf != NULL, len is the size of the buffer, the extra header is copied in buf.

   return number of bytes copied in buf, or (if <0) the error code */

extern int ZEXPORT unzCloseCurrentFile(unzFile file);
/* Close the file in zip opened with unzOpenCurrentFile

   return UNZ_CRCERROR if all the file was read but the CRC is not good */

/***************************************************************************/
/* Browse the directory of the zipfile */

typedef int (*unzFileNameComparer)(unzFile file, const char *filename1, const char *filename2);
typedef int (*unzIteratorFunction)(unzFile file);
typedef int (*unzIteratorFunction2)(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size);

extern int ZEXPORT unzGoToFirstFile(unzFile file);
/* Set the current file of the zipfile to the first file.

   return UNZ_OK if no error */

extern int ZEXPORT unzGoToFirstFile2(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size);
/* Set the current file of the zipfile to the first file and retrieves the current info on success. 
   Not as seek intensive as unzGoToFirstFile + unzGetCurrentFileInfo.

   return UNZ_OK if no error */

extern int ZEXPORT unzGoToNextFile(unzFile file);
/* Set the current file of the zipfile to the next file.

   return UNZ_OK if no error
   return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest */

extern int ZEXPORT unzGoToNextFile2(unzFile file, unz_file_info64 *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size);
/* Set the current file of the zipfile to the next file and retrieves the current 
   info on success. Does less seeking around than unzGotoNextFile + unzGetCurrentFileInfo.

   return UNZ_OK if no error
   return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest */

extern int ZEXPORT unzLocateFile(unzFile file, const char *filename, unzFileNameComparer filename_compare_func);
/* Try locate the file szFileName in the zipfile. For custom filename comparison pass in comparison function.

   return UNZ_OK if the file is found (it becomes the current file)
   return UNZ_END_OF_LIST_OF_FILE if the file is not found */

/***************************************************************************/
/* Raw access to zip file */

typedef struct unz_file_pos_s
{
    uint32_t pos_in_zip_directory;  /* offset in zip file directory */
    uint32_t num_of_file;           /* # of file */
} unz_file_pos;

extern int ZEXPORT unzGetFilePos(unzFile file, unz_file_pos *file_pos);
extern int ZEXPORT unzGoToFilePos(unzFile file, unz_file_pos *file_pos);

typedef struct unz64_file_pos_s
{
    uint64_t pos_in_zip_directory;   /* offset in zip file directory */
    uint64_t num_of_file;            /* # of file */
} unz64_file_pos;

extern int ZEXPORT unzGetFilePos64(unzFile file, unz64_file_pos *file_pos);
extern int ZEXPORT unzGoToFilePos64(unzFile file, const unz64_file_pos *file_pos);

extern int32_t ZEXPORT unzGetOffset(unzFile file);
extern int64_t ZEXPORT unzGetOffset64(unzFile file);
/* Get the current file offset */

extern int ZEXPORT unzSetOffset(unzFile file, uint32_t pos);
extern int ZEXPORT unzSetOffset64(unzFile file, uint64_t pos);
/* Set the current file offset */

extern int32_t ZEXPORT unzTell(unzFile file);
extern int64_t ZEXPORT unzTell64(unzFile file);
/* return current position in uncompressed data */

extern int ZEXPORT unzSeek(unzFile file, uint32_t offset, int origin);
extern int ZEXPORT unzSeek64(unzFile file, uint64_t offset, int origin);
/* Seek within the uncompressed data if compression method is storage */

extern int ZEXPORT unzEndOfFile(unzFile file);
/* return 1 if the end of file was reached, 0 elsewhere */

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _UNZ_H */
