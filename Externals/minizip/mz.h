/* mz.h -- Errors codes, zip flags and magic
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_H
#define MZ_H

/***************************************************************************/

/* MZ_VERSION */
#define MZ_VERSION                      ("3.0.4")
#define MZ_VERSION_BUILD                (030004)

/* MZ_ERROR */
#define MZ_OK                           (0)  /* zlib */
#define MZ_STREAM_ERROR                 (-1) /* zlib */
#define MZ_DATA_ERROR                   (-3) /* zlib */
#define MZ_MEM_ERROR                    (-4) /* zlib */
#define MZ_BUF_ERROR                    (-5) /* zlib */
#define MZ_VERSION_ERROR                (-6) /* zlib */

#define MZ_END_OF_LIST                  (-100)
#define MZ_END_OF_STREAM                (-101)

#define MZ_PARAM_ERROR                  (-102)
#define MZ_FORMAT_ERROR                 (-103)
#define MZ_INTERNAL_ERROR               (-104)
#define MZ_CRC_ERROR                    (-105)
#define MZ_CRYPT_ERROR                  (-106)
#define MZ_EXIST_ERROR                  (-107)
#define MZ_PASSWORD_ERROR               (-108)
#define MZ_SUPPORT_ERROR                (-109)
#define MZ_HASH_ERROR                   (-110)
#define MZ_OPEN_ERROR                   (-111)
#define MZ_CLOSE_ERROR                  (-112)
#define MZ_SEEK_ERROR                   (-113)
#define MZ_TELL_ERROR                   (-114)
#define MZ_READ_ERROR                   (-115)
#define MZ_WRITE_ERROR                  (-116)
#define MZ_SIGN_ERROR                   (-117)
#define MZ_SYMLINK_ERROR                (-118)

/* MZ_OPEN */
#define MZ_OPEN_MODE_READ               (0x01)
#define MZ_OPEN_MODE_WRITE              (0x02)
#define MZ_OPEN_MODE_READWRITE          (MZ_OPEN_MODE_READ | MZ_OPEN_MODE_WRITE)
#define MZ_OPEN_MODE_APPEND             (0x04)
#define MZ_OPEN_MODE_CREATE             (0x08)
#define MZ_OPEN_MODE_EXISTING           (0x10)

/* MZ_SEEK */
#define MZ_SEEK_SET                     (0)
#define MZ_SEEK_CUR                     (1)
#define MZ_SEEK_END                     (2)

/* MZ_COMPRESS */
#define MZ_COMPRESS_METHOD_STORE        (0)
#define MZ_COMPRESS_METHOD_DEFLATE      (8)
#define MZ_COMPRESS_METHOD_BZIP2        (12)
#define MZ_COMPRESS_METHOD_LZMA         (14)
#define MZ_COMPRESS_METHOD_ZSTD         (93)
#define MZ_COMPRESS_METHOD_XZ           (95)
#define MZ_COMPRESS_METHOD_AES          (99)

#define MZ_COMPRESS_LEVEL_DEFAULT       (-1)
#define MZ_COMPRESS_LEVEL_FAST          (2)
#define MZ_COMPRESS_LEVEL_NORMAL        (6)
#define MZ_COMPRESS_LEVEL_BEST          (9)

/* MZ_ZIP_FLAG */
#define MZ_ZIP_FLAG_ENCRYPTED           (1 << 0)
#define MZ_ZIP_FLAG_LZMA_EOS_MARKER     (1 << 1)
#define MZ_ZIP_FLAG_DEFLATE_MAX         (1 << 1)
#define MZ_ZIP_FLAG_DEFLATE_NORMAL      (0)
#define MZ_ZIP_FLAG_DEFLATE_FAST        (1 << 2)
#define MZ_ZIP_FLAG_DEFLATE_SUPER_FAST  (MZ_ZIP_FLAG_DEFLATE_FAST | \
                                         MZ_ZIP_FLAG_DEFLATE_MAX)
#define MZ_ZIP_FLAG_DATA_DESCRIPTOR     (1 << 3)
#define MZ_ZIP_FLAG_UTF8                (1 << 11)
#define MZ_ZIP_FLAG_MASK_LOCAL_INFO     (1 << 13)

/* MZ_ZIP_EXTENSION */
#define MZ_ZIP_EXTENSION_ZIP64          (0x0001)
#define MZ_ZIP_EXTENSION_NTFS           (0x000a)
#define MZ_ZIP_EXTENSION_AES            (0x9901)
#define MZ_ZIP_EXTENSION_UNIX1          (0x000d)
#define MZ_ZIP_EXTENSION_SIGN           (0x10c5)
#define MZ_ZIP_EXTENSION_HASH           (0x1a51)
#define MZ_ZIP_EXTENSION_CDCD           (0xcdcd)

/* MZ_ZIP64 */
#define MZ_ZIP64_AUTO                   (0)
#define MZ_ZIP64_FORCE                  (1)
#define MZ_ZIP64_DISABLE                (2)

/* MZ_HOST_SYSTEM */
#define MZ_HOST_SYSTEM(VERSION_MADEBY)  ((uint8_t)(VERSION_MADEBY >> 8))
#define MZ_HOST_SYSTEM_MSDOS            (0)
#define MZ_HOST_SYSTEM_UNIX             (3)
#define MZ_HOST_SYSTEM_WINDOWS_NTFS     (10)
#define MZ_HOST_SYSTEM_RISCOS           (13)
#define MZ_HOST_SYSTEM_OSX_DARWIN       (19)

/* MZ_PKCRYPT */
#define MZ_PKCRYPT_HEADER_SIZE          (12)

/* MZ_AES */
#define MZ_AES_VERSION                  (1)
#define MZ_AES_ENCRYPTION_MODE_128      (0x01)
#define MZ_AES_ENCRYPTION_MODE_192      (0x02)
#define MZ_AES_ENCRYPTION_MODE_256      (0x03)
#define MZ_AES_KEY_LENGTH(MODE)         (8 * (MODE & 3) + 8)
#define MZ_AES_KEY_LENGTH_MAX           (32)
#define MZ_AES_BLOCK_SIZE               (16)
#define MZ_AES_HEADER_SIZE(MODE)        ((4 * (MODE & 3) + 4) + 2)
#define MZ_AES_FOOTER_SIZE              (10)

/* MZ_HASH */
#define MZ_HASH_MD5                     (10)
#define MZ_HASH_MD5_SIZE                (16)
#define MZ_HASH_SHA1                    (20)
#define MZ_HASH_SHA1_SIZE               (20)
#define MZ_HASH_SHA256                  (23)
#define MZ_HASH_SHA256_SIZE             (32)
#define MZ_HASH_MAX_SIZE                (256)

/* MZ_ENCODING */
#define MZ_ENCODING_CODEPAGE_437        (437)
#define MZ_ENCODING_CODEPAGE_932        (932)
#define MZ_ENCODING_CODEPAGE_936        (936)
#define MZ_ENCODING_CODEPAGE_950        (950)
#define MZ_ENCODING_UTF8                (65001)

/* MZ_UTILITY */
#define MZ_UNUSED(SYMBOL)               ((void)SYMBOL)

#ifndef MZ_CUSTOM_ALLOC
#define MZ_ALLOC(SIZE)                  (malloc((SIZE)))
#endif
#ifndef MZ_CUSTOM_FREE
#define MZ_FREE(PTR)                    (free(PTR))
#endif

#if defined(_WIN32) && defined(MZ_EXPORTS)
#define MZ_EXPORT __declspec(dllexport)
#else
#define MZ_EXPORT
#endif

/***************************************************************************/

#include <stdlib.h> /* size_t, NULL, malloc */
#include <time.h>   /* time_t, time() */
#include <string.h> /* memset, strncpy, strlen */
#include <limits.h>

#if defined(HAVE_STDINT_H)
#  include <stdint.h>
#elif defined(__has_include)
#  if __has_include(<stdint.h>)
#    include <stdint.h>
#  endif
#endif

#ifndef INT8_MAX
typedef signed char        int8_t;
#endif
#ifndef INT16_MAX
typedef short              int16_t;
#endif
#ifndef INT32_MAX
typedef int                int32_t;
#endif
#ifndef INT64_MAX
typedef long long          int64_t;
#endif
#ifndef UINT8_MAX
typedef unsigned char      uint8_t;
#endif
#ifndef UINT16_MAX
typedef unsigned short     uint16_t;
#endif
#ifndef UINT32_MAX
typedef unsigned int       uint32_t;
#endif
#ifndef UINT64_MAX
typedef unsigned long long uint64_t;
#endif

#if defined(HAVE_INTTYPES_H)
#  include <inttypes.h>
#elif defined(__has_include)
#  if __has_include(<inttypes.h>)
#    include <inttypes.h>
#  endif
#endif

#ifndef PRId8
#  define PRId8  "hhd"
#endif
#ifndef PRIu8
#  define PRIu8  "hhu"
#endif
#ifndef PRIx8
#  define PRIx8  "hhx"
#endif
#ifndef PRId16
#  define PRId16 "hd"
#endif
#ifndef PRIu16
#  define PRIu16 "hu"
#endif
#ifndef PRIx16
#  define PRIx16 "hx"
#endif
#ifndef PRId32
#  define PRId32 "d"
#endif
#ifndef PRIu32
#  define PRIu32 "u"
#endif
#ifndef PRIx32
#  define PRIx32 "x"
#endif
#if ULONG_MAX == 0xfffffffful
#  ifndef PRId64
#    define PRId64 "ld"
#  endif
#  ifndef PRIu64
#    define PRIu64 "lu"
#  endif
#  ifndef PRIx64
#    define PRIx64 "lx"
#  endif
#else
#  ifndef PRId64
#    define PRId64 "lld"
#  endif
#  ifndef PRIu64
#    define PRIu64 "llu"
#  endif
#  ifndef PRIx64
#    define PRIx64 "llx"
#  endif
#endif

#ifndef INT16_MAX
#  define INT16_MAX   32767
#endif
#ifndef INT32_MAX
#  define INT32_MAX   2147483647L
#endif
#ifndef INT64_MAX
#  define INT64_MAX   9223372036854775807LL
#endif
#ifndef UINT16_MAX
#  define UINT16_MAX  65535U
#endif
#ifndef UINT32_MAX
#  define UINT32_MAX  4294967295UL
#endif
#ifndef UINT64_MAX
#  define UINT64_MAX  18446744073709551615ULL
#endif

/***************************************************************************/

#endif
