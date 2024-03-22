# minizip-ng

minizip-ng is a zip manipulation library written in C that is supported on Windows, macOS, and Linux.

[![Master Branch Status](https://github.com/zlib-ng/minizip-ng/workflows/Build/badge.svg)](https://github.com/zlib-ng/minizip-ng/actions)
[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/minizip.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:minizip)
[![License: Zlib](https://img.shields.io/badge/license-zlib-lightgrey.svg)](https://github.com/zlib-ng/minizip-ng/blob/master/LICENSE)
[![codecov.io](https://codecov.io/github/zlib-ng/minizip-ng/coverage.svg?branch=develop)](https://codecov.io/github/zlib-ng/minizip-ng/)

Developed and maintained by Nathan Moinvaziri.

## Branches

| Name                                                          | Description                                                                                                                                                             |
|:--------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [master](https://github.com/zlib-ng/minizip-ng/tree/master)   | Most recent release.                                                                                                                                                    |
| [develop](https://github.com/zlib-ng/minizip-ng/tree/develop) | Latest development code.                                                                                                                                                |
| [1.2](https://github.com/zlib-ng/minizip-ng/tree/1.2)         | Old changes to original minizip that includes WinZip AES encryption, disk splitting, I/O buffering and some additional fixes. Not ABI compatible with original minizip. |
| [1.1](https://github.com/zlib-ng/minizip-ng/tree/1.1)         | Original minizip as of zlib 1.2.11.                                                                                                                                     |

## History

Minizip was originally developed by [Gilles Vollant](https://www.winimage.com/zLibDll/minizip.html) in 1998. It was first included in the zlib distribution as an additional code contribution starting in zlib 1.1.2. Since that time, it has been continually improved upon and contributed to by many people. The original [project](https://github.com/madler/zlib/tree/master/contrib/minizip) can still be found in the zlib distribution that is maintained by Mark Adler.

The motivation behind this repository has been the need for new features and bug fixes to the original library which had
not been maintained for a long period of time. The code has been largely refactored and rewritten in order to help improve maintainability and readability. A compatibility layer has been provided for consumers of the original minizip library.

## Features

+ Creating and extracting zip archives.
+ Adding and removing entries from zip archives.
+ Read and write raw zip entry data.
+ Reading and writing zip archives from memory.
+ Support for large files with ZIP64 extension.
+ Zlib, BZIP2, LZMA, XZ, and ZSTD compression methods.
+ Password protection through Traditional PKWARE and [WinZIP AES](https://www.winzip.com/aes_info.htm) encryption.
+ Buffered streaming for improved I/O performance.
+ NTFS timestamp support for UTC last modified, last accessed, and creation dates.
+ Disk split support for splitting zip archives into multiple files.
+ Preservation of file attributes across file systems.
+ Follow and store symbolic links.
+ Unicode filename support through UTF-8 encoding.
+ Legacy character encoding support CP437, CP932, CP936, CP950.
+ Turn off compilation of compression, decompression, or encryption.
+ Windows (Win32 & WinRT), macOS and Linux platform support.
+ Streaming interface for easy implementation of additional platforms.
+ Support for Apple's compression library ZLIB and XZ implementations.
+ Zero out local file header information.
+ Zip/unzip of central directory to reduce size.
+ Recover the central directory if it is corrupt or missing.
+ Example minizip and minigzip command line tools.

## Build

To generate project files for your platform:

1. [Download and install](https://cmake.org/install/) cmake (version 3.11 or later recommended).
2. Run cmake in the minizip directory.

```
cmake -S . -B build -D MZ_BUILD_TESTS=ON
cmake --build build
```

## Build Options

| Name                | Description                                                    | Default Value |
|:--------------------|:---------------------------------------------------------------|:-------------:|
| MZ_COMPAT           | Enables compatibility layer                                    |      ON       |
| MZ_ZLIB             | Enables ZLIB compression                                       |      ON       |
| MZ_BZIP2            | Enables BZIP2 compression                                      |      ON       |
| MZ_LZMA             | Enables LZMA & XZ compression                                  |      ON       |
| MZ_ZSTD             | Enables ZSTD compression                                       |      ON       |
| MZ_LIBCOMP          | Enables Apple compression                                      |     APPLE     |
| MZ_FETCH_LIBS       | Enables fetching third-party libraries if not found            |     WIN32     |
| MZ_FORCE_FETCH_LIBS | Enables fetching third-party libraries always                  |      OFF      |
| MZ_PKCRYPT          | Enables PKWARE traditional encryption                          |      ON       |
| MZ_WZAES            | Enables WinZIP AES encryption                                  |      ON       |
| MZ_OPENSSL          | Enables OpenSSL encryption                                     |     UNIX      |
| MZ_LIBBSD           | Builds with libbsd crypto random                               |     UNIX      |
| MZ_ICONV            | Enables iconv encoding conversion                              |      ON       |
| MZ_COMPRESS_ONLY    | Only support compression                                       |      OFF      |
| MZ_DECOMPRESS_ONLY  | Only support decompression                                     |      OFF      |
| MZ_FILE32_API       | Builds using posix 32-bit file api                             |      OFF      |
| MZ_BUILD_TESTS      | Builds minizip test executable                                 |      OFF      |
| MZ_BUILD_UNIT_TESTS | Builds minizip unit test project                               |      OFF      |
| MZ_BUILD_FUZZ_TESTS | Builds minizip fuzz executables                                |      OFF      |
| MZ_CODE_COVERAGE    | Build with code coverage flags                                 |      OFF      |
| MZ_SANITIZER        | Build with code sanitizer (Memory, Thread, Address, Undefined) |               |
| MZ_LIB_SUFFIX       | Library name suffix for packaging                              |               |

## Third-Party Libraries

Third-party libraries may be required based on the CMake options selected. If the system already has the library
installed then it will be used, otherwise CMake will retrieve the source code for the library from its official git repository and compile it in when the `MZ_FETCH_LIBS` option is enabled.

|Project|License|CMake Option|Comments|
|-|-|-|-|
[bzip2](https://www.sourceware.org/bzip2/)|[license](https://github.com/zlib-ng/minizip-ng/blob/develop/lib/bzip2/LICENSE)|`MZ_BZIP2`|Written by Julian Seward.|
|[liblzma](https://tukaani.org/xz/)|Public domain|`MZ_LZMA`|Written by Igor Pavlov and Lasse Collin.|
|[zlib](https://zlib.net/)|zlib|`MZ_ZLIB`|Written by Mark Adler and Jean-loup Gailly. Or alternatively, [zlib-ng](https://github.com/zlib-ng/zlib-ng) by Hans Kristian Rosbach.|
|[zstd](https://github.com/facebook/zstd)|[BSD](https://github.com/facebook/zstd/blob/dev/LICENSE)|`MZ_ZSTD`|Written by Facebook.|

This project uses the zlib [license](LICENSE).

## Acknowledgments

Thanks go out to all the people who have taken the time to contribute code reviews, testing and/or patches. This project would not have been as good without you.

Thanks to [Gilles Vollant](https://www.winimage.com/zLibDll/minizip.html) on which this work is originally based on.

The [ZIP format](https://github.com/zlib-ng/minizip-ng/blob/master/doc/zip/appnote.txt) was defined by Phil Katz of PKWARE.
