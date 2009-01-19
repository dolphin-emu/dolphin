/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/**
 * \file SDL_rwops.h
 *
 * This file provides a general interface for SDL to read and write
 * data sources.  It can easily be extended to files, memory, etc.
 */

#ifndef _SDL_rwops_h
#define _SDL_rwops_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* This is the read/write operation structure -- very basic */

typedef struct SDL_RWops
{
    /* Seek to 'offset' relative to whence, one of stdio's whence values:
       RW_SEEK_SET, RW_SEEK_CUR, RW_SEEK_END
       Returns the final offset in the data source.
     */
    long (SDLCALL * seek) (struct SDL_RWops * context, long offset,
                           int whence);

    /* Read up to 'num' objects each of size 'objsize' from the data
       source to the area pointed at by 'ptr'.
       Returns the number of objects read, or 0 at error or end of file.
     */
      size_t(SDLCALL * read) (struct SDL_RWops * context, void *ptr,
                              size_t size, size_t maxnum);

    /* Write exactly 'num' objects each of size 'objsize' from the area
       pointed at by 'ptr' to data source.
       Returns the number of objects written, or 0 at error or end of file.
     */
      size_t(SDLCALL * write) (struct SDL_RWops * context, const void *ptr,
                               size_t size, size_t num);

    /* Close and free an allocated SDL_RWops structure.
       Returns 0 if successful or -1 on write error when flushing data.
     */
    int (SDLCALL * close) (struct SDL_RWops * context);

    Uint32 type;
    union
    {
#ifdef __WIN32__
        struct
        {
            SDL_bool append;
            void *h;
            struct
            {
                void *data;
                int size;
                int left;
            } buffer;
        } win32io;
#endif
#ifdef HAVE_STDIO_H
        struct
        {
            SDL_bool autoclose;
            FILE *fp;
        } stdio;
#endif
        struct
        {
            Uint8 *base;
            Uint8 *here;
            Uint8 *stop;
        } mem;
        struct
        {
            void *data1;
        } unknown;
    } hidden;

} SDL_RWops;


/* Functions to create SDL_RWops structures from various data sources */

extern DECLSPEC SDL_RWops *SDLCALL SDL_RWFromFile(const char *file,
                                                  const char *mode);

#ifdef HAVE_STDIO_H
extern DECLSPEC SDL_RWops *SDLCALL SDL_RWFromFP(FILE * fp,
                                                SDL_bool autoclose);
#endif

extern DECLSPEC SDL_RWops *SDLCALL SDL_RWFromMem(void *mem, int size);
extern DECLSPEC SDL_RWops *SDLCALL SDL_RWFromConstMem(const void *mem,
                                                      int size);

extern DECLSPEC SDL_RWops *SDLCALL SDL_AllocRW(void);
extern DECLSPEC void SDLCALL SDL_FreeRW(SDL_RWops * area);

#define RW_SEEK_SET	0       /* Seek from the beginning of data */
#define RW_SEEK_CUR	1       /* Seek relative to current read point */
#define RW_SEEK_END	2       /* Seek relative to the end of data */

/* Macros to easily read and write from an SDL_RWops structure */
#define SDL_RWseek(ctx, offset, whence)	(ctx)->seek(ctx, offset, whence)
#define SDL_RWtell(ctx)			(ctx)->seek(ctx, 0, RW_SEEK_CUR)
#define SDL_RWread(ctx, ptr, size, n)	(ctx)->read(ctx, ptr, size, n)
#define SDL_RWwrite(ctx, ptr, size, n)	(ctx)->write(ctx, ptr, size, n)
#define SDL_RWclose(ctx)		(ctx)->close(ctx)


/* Read an item of the specified endianness and return in native format */
extern DECLSPEC Uint16 SDLCALL SDL_ReadLE16(SDL_RWops * src);
extern DECLSPEC Uint16 SDLCALL SDL_ReadBE16(SDL_RWops * src);
extern DECLSPEC Uint32 SDLCALL SDL_ReadLE32(SDL_RWops * src);
extern DECLSPEC Uint32 SDLCALL SDL_ReadBE32(SDL_RWops * src);
extern DECLSPEC Uint64 SDLCALL SDL_ReadLE64(SDL_RWops * src);
extern DECLSPEC Uint64 SDLCALL SDL_ReadBE64(SDL_RWops * src);

/* Write an item of native format to the specified endianness */
extern DECLSPEC int SDLCALL SDL_WriteLE16(SDL_RWops * dst, Uint16 value);
extern DECLSPEC int SDLCALL SDL_WriteBE16(SDL_RWops * dst, Uint16 value);
extern DECLSPEC int SDLCALL SDL_WriteLE32(SDL_RWops * dst, Uint32 value);
extern DECLSPEC int SDLCALL SDL_WriteBE32(SDL_RWops * dst, Uint32 value);
extern DECLSPEC int SDLCALL SDL_WriteLE64(SDL_RWops * dst, Uint64 value);
extern DECLSPEC int SDLCALL SDL_WriteBE64(SDL_RWops * dst, Uint64 value);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_rwops_h */

/* vi: set ts=4 sw=4 expandtab: */
