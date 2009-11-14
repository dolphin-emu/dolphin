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
 * \file SDL_surface.h
 *
 * Header file for SDL_surface definition and management functions
 */

#ifndef _SDL_surface_h
#define _SDL_surface_h

#include "SDL_stdinc.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_rwops.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* These are the currently supported flags for the SDL_surface */
/* Used internally (read-only) */
#define SDL_PREALLOC        0x00000001  /* Surface uses preallocated memory */
#define SDL_RLEACCEL        0x00000002  /* Surface is RLE encoded */

/* Evaluates to true if the surface needs to be locked before access */
#define SDL_MUSTLOCK(S)	(((S)->flags & SDL_RLEACCEL) != 0)

/**
 * \struct SDL_Surface
 *
 * \brief A collection of pixels used in software blitting
 *
 * \note  This structure should be treated as read-only, except for 'pixels',
 *        which, if not NULL, contains the raw pixel data for the surface.
 */
typedef struct SDL_Surface
{
    Uint32 flags;               /**< Read-only */
    SDL_PixelFormat *format;    /**< Read-only */
    int w, h;                   /**< Read-only */
    int pitch;                  /**< Read-only */
    void *pixels;               /**< Read-write */

    /* Application data associated with the surfade */
    void *userdata;             /**< Read-write */

    /* information needed for surfaces requiring locks */
    int locked;                 /**< Read-only */
    void *lock_data;            /**< Read-only */

    /* clipping information */
    SDL_Rect clip_rect;         /**< Read-only */

    /* info for fast blit mapping to other surfaces */
    struct SDL_BlitMap *map;    /**< Private */

    /* format version, bumped at every change to invalidate blit maps */
    unsigned int format_version;        /**< Private */

    /* Reference count -- used when freeing surface */
    int refcount;               /**< Read-mostly */
} SDL_Surface;

/**
 * \typedef SDL_blit
 *
 * \brief The type of function used for surface blitting functions
 */
typedef int (*SDL_blit) (struct SDL_Surface * src, SDL_Rect * srcrect,
                         struct SDL_Surface * dst, SDL_Rect * dstrect);

/*
 * Allocate and free an RGB surface (must be called after SDL_SetVideoMode)
 * If the depth is 4 or 8 bits, an empty palette is allocated for the surface.
 * If the depth is greater than 8 bits, the pixel format is set using the
 * flags '[RGB]mask'.
 * If the function runs out of memory, it will return NULL.
 *
 * The 'flags' are obsolete and should be set to 0.
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_CreateRGBSurface
    (Uint32 flags, int width, int height, int depth,
     Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
extern DECLSPEC SDL_Surface *SDLCALL SDL_CreateRGBSurfaceFrom(void *pixels,
                                                              int width,
                                                              int height,
                                                              int depth,
                                                              int pitch,
                                                              Uint32 Rmask,
                                                              Uint32 Gmask,
                                                              Uint32 Bmask,
                                                              Uint32 Amask);
extern DECLSPEC void SDLCALL SDL_FreeSurface(SDL_Surface * surface);

/**
 * \fn int SDL_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette)
 *
 * \brief Set the palette used by a surface.
 *
 * \return 0, or -1 if the surface format doesn't use a palette.
 *
 * \note A single palette can be shared with many surfaces.
 */
extern DECLSPEC int SDLCALL SDL_SetSurfacePalette(SDL_Surface * surface,
                                                  SDL_Palette * palette);

/*
 * SDL_LockSurface() sets up a surface for directly accessing the pixels.
 * Between calls to SDL_LockSurface()/SDL_UnlockSurface(), you can write
 * to and read from 'surface->pixels', using the pixel format stored in 
 * 'surface->format'.  Once you are done accessing the surface, you should 
 * use SDL_UnlockSurface() to release it.
 *
 * Not all surfaces require locking.  If SDL_MUSTLOCK(surface) evaluates
 * to 0, then you can read and write to the surface at any time, and the
 * pixel format of the surface will not change.
 * 
 * No operating system or library calls should be made between lock/unlock
 * pairs, as critical system locks may be held during this time.
 *
 * SDL_LockSurface() returns 0, or -1 if the surface couldn't be locked.
 */
extern DECLSPEC int SDLCALL SDL_LockSurface(SDL_Surface * surface);
extern DECLSPEC void SDLCALL SDL_UnlockSurface(SDL_Surface * surface);

/*
 * Load a surface from a seekable SDL data source (memory or file.)
 * If 'freesrc' is non-zero, the source will be closed after being read.
 * Returns the new surface, or NULL if there was an error.
 * The new surface should be freed with SDL_FreeSurface().
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_LoadBMP_RW(SDL_RWops * src,
                                                    int freesrc);

/* Convenience macro -- load a surface from a file */
#define SDL_LoadBMP(file)	SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1)

/*
 * Save a surface to a seekable SDL data source (memory or file.)
 * If 'freedst' is non-zero, the source will be closed after being written.
 * Returns 0 if successful or -1 if there was an error.
 */
extern DECLSPEC int SDLCALL SDL_SaveBMP_RW
    (SDL_Surface * surface, SDL_RWops * dst, int freedst);

/* Convenience macro -- save a surface to a file */
#define SDL_SaveBMP(surface, file) \
		SDL_SaveBMP_RW(surface, SDL_RWFromFile(file, "wb"), 1)

/*
 * \fn int SDL_SetSurfaceRLE(SDL_Surface *surface, int flag)
 *
 * \brief Sets the RLE acceleration hint for a surface.
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \note If RLE is enabled, colorkey and alpha blending blits are much faster,
 *       but the surface must be locked before directly accessing the pixels.
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceRLE(SDL_Surface * surface,
                                              int flag);

/*
 * \fn int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key)
 *
 * \brief Sets the color key (transparent pixel) in a blittable surface.
 *
 * \param surface The surface to update
 * \param flag Non-zero to enable colorkey and 0 to disable colorkey 
 * \param key The transparent pixel in the native surface format
 *
 * \return 0 on success, or -1 if the surface is not valid
 */
extern DECLSPEC int SDLCALL SDL_SetColorKey(SDL_Surface * surface,
                                            Uint32 flag, Uint32 key);

/**
 * \fn int SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b)
 *
 * \brief Set an additional color value used in blit operations
 *
 * \param surface The surface to update
 * \param r The red source color value multiplied into blit operations
 * \param g The green source color value multiplied into blit operations
 * \param b The blue source color value multiplied into blit operations
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \sa SDL_GetSurfaceColorMod()
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceColorMod(SDL_Surface * surface,
                                                   Uint8 r, Uint8 g, Uint8 b);


/**
 * \fn int SDL_GetSurfaceColorMod(SDL_Surface *surface, Uint8 *r, Uint8 *g, Uint8 *b)
 *
 * \brief Get the additional color value used in blit operations
 *
 * \param surface The surface to query
 * \param r A pointer filled in with the source red color value
 * \param g A pointer filled in with the source green color value
 * \param b A pointer filled in with the source blue color value
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \sa SDL_SetSurfaceColorMod()
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceColorMod(SDL_Surface * surface,
                                                   Uint8 * r, Uint8 * g,
                                                   Uint8 * b);

/**
 * \fn int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 alpha)
 *
 * \brief Set an additional alpha value used in blit operations
 *
 * \param surface The surface to update
 * \param alpha The source alpha value multiplied into blit operations.
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \sa SDL_GetSurfaceAlphaMod()
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceAlphaMod(SDL_Surface * surface,
                                                   Uint8 alpha);

/**
 * \fn int SDL_GetSurfaceAlphaMod(SDL_Surface *surface, Uint8 *alpha)
 *
 * \brief Get the additional alpha value used in blit operations
 *
 * \param surface The surface to query
 * \param alpha A pointer filled in with the source alpha value
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \sa SDL_SetSurfaceAlphaMod()
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceAlphaMod(SDL_Surface * surface,
                                                   Uint8 * alpha);

/**
 * \fn int SDL_SetSurfaceBlendMode(SDL_Surface *surface, int blendMode)
 *
 * \brief Set the blend mode used for blit operations
 *
 * \param surface The surface to update
 * \param blendMode SDL_TextureBlendMode to use for blit blending
 *
 * \return 0 on success, or -1 if the parameters are not valid
 *
 * \sa SDL_GetSurfaceBlendMode()
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceBlendMode(SDL_Surface * surface,
                                                    int blendMode);

/**
 * \fn int SDL_GetSurfaceBlendMode(SDL_Surface *surface, int *blendMode)
 *
 * \brief Get the blend mode used for blit operations
 *
 * \param surface The surface to query
 * \param blendMode A pointer filled in with the current blend mode
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \sa SDL_SetSurfaceBlendMode()
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceBlendMode(SDL_Surface * surface,
                                                    int *blendMode);

/**
 * \fn int SDL_SetSurfaceScaleMode(SDL_Surface *surface, int scaleMode)
 *
 * \brief Set the scale mode used for blit operations
 *
 * \param surface The surface to update
 * \param scaleMode SDL_TextureScaleMode to use for blit scaling
 *
 * \return 0 on success, or -1 if the surface is not valid or the scale mode is not supported
 *
 * \note If the scale mode is not supported, the closest supported mode is chosen.  Currently only SDL_TEXTURESCALEMODE_FAST is supported on surfaces.
 *
 * \sa SDL_GetSurfaceScaleMode()
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceScaleMode(SDL_Surface * surface,
                                                    int scaleMode);

/**
 * \fn int SDL_GetSurfaceScaleMode(SDL_Surface *surface, int *scaleMode)
 *
 * \brief Get the scale mode used for blit operations
 *
 * \param surface The surface to query
 * \param scaleMode A pointer filled in with the current scale mode
 *
 * \return 0 on success, or -1 if the surface is not valid
 *
 * \sa SDL_SetSurfaceScaleMode()
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceScaleMode(SDL_Surface * surface,
                                                    int *scaleMode);

/*
 * Sets the clipping rectangle for the destination surface in a blit.
 *
 * If the clip rectangle is NULL, clipping will be disabled.
 * If the clip rectangle doesn't intersect the surface, the function will
 * return SDL_FALSE and blits will be completely clipped.  Otherwise the
 * function returns SDL_TRUE and blits to the surface will be clipped to
 * the intersection of the surface area and the clipping rectangle.
 *
 * Note that blits are automatically clipped to the edges of the source
 * and destination surfaces.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_SetClipRect(SDL_Surface * surface,
                                                 const SDL_Rect * rect);

/*
 * Gets the clipping rectangle for the destination surface in a blit.
 * 'rect' must be a pointer to a valid rectangle which will be filled
 * with the correct values.
 */
extern DECLSPEC void SDLCALL SDL_GetClipRect(SDL_Surface * surface,
                                             SDL_Rect * rect);

/*
 * Creates a new surface of the specified format, and then copies and maps 
 * the given surface to it so the blit of the converted surface will be as 
 * fast as possible.  If this function fails, it returns NULL.
 *
 * The 'flags' parameter is passed to SDL_CreateRGBSurface() and has those 
 * semantics.  You can also pass SDL_RLEACCEL in the flags parameter and
 * SDL will try to RLE accelerate colorkey and alpha blits in the resulting
 * surface.
 *
 * This function is used internally by SDL_DisplayFormat().
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_ConvertSurface
    (SDL_Surface * src, SDL_PixelFormat * fmt, Uint32 flags);

/*
 * This function draws a point with 'color'
 * The color should be a pixel of the format used by the surface, and 
 * can be generated by the SDL_MapRGB() function.
 * This function returns 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_DrawPoint
    (SDL_Surface * dst, int x, int y, Uint32 color);

/*
 * This function blends a point with an RGBA value
 * The color should be a pixel of the format used by the surface, and 
 * can be generated by the SDL_MapRGB() function.
 * This function returns 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_BlendPoint
    (SDL_Surface * dst, int x, int y, int blendMode,
     Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/*
 * This function draws a line with 'color'
 * The color should be a pixel of the format used by the surface, and 
 * can be generated by the SDL_MapRGB() function.
 * This function returns 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_DrawLine
    (SDL_Surface * dst, int x1, int y1, int x2, int y2, Uint32 color);

/*
 * This function blends an RGBA value along a line
 * This function returns 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_BlendLine
    (SDL_Surface * dst, int x1, int y1, int x2, int y2, int blendMode,
     Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/*
 * This function performs a fast fill of the given rectangle with 'color'
 * The given rectangle is clipped to the destination surface clip area
 * and the final fill rectangle is saved in the passed in pointer.
 * If 'dstrect' is NULL, the whole surface will be filled with 'color'
 * The color should be a pixel of the format used by the surface, and 
 * can be generated by the SDL_MapRGB() function.
 * This function returns 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_FillRect
    (SDL_Surface * dst, SDL_Rect * dstrect, Uint32 color);

/*
 * This function blends an RGBA value into the given rectangle.
 * The given rectangle is clipped to the destination surface clip area
 * and the final fill rectangle is saved in the passed in pointer.
 * If 'dstrect' is NULL, the whole surface will be filled with 'color'
 * This function returns 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_BlendRect
    (SDL_Surface * dst, SDL_Rect * dstrect, int blendMode, Uint8 r, Uint8 g,
     Uint8 b, Uint8 a);

/*
 * This performs a fast blit from the source surface to the destination
 * surface.  It assumes that the source and destination rectangles are
 * the same size.  If either 'srcrect' or 'dstrect' are NULL, the entire
 * surface (src or dst) is copied.  The final blit rectangles are saved
 * in 'srcrect' and 'dstrect' after all clipping is performed.
 * If the blit is successful, it returns 0, otherwise it returns -1.
 *
 * The blit function should not be called on a locked surface.
 *
 * The blit semantics for surfaces with and without alpha and colorkey
 * are defined as follows:
 *
 * RGBA->RGB:
 *     SDL_SRCALPHA set:
 * 	alpha-blend (using alpha-channel).
 * 	SDL_SRCCOLORKEY ignored.
 *     SDL_SRCALPHA not set:
 * 	copy RGB.
 * 	if SDL_SRCCOLORKEY set, only copy the pixels matching the
 * 	RGB values of the source colour key, ignoring alpha in the
 * 	comparison.
 * 
 * RGB->RGBA:
 *     SDL_SRCALPHA set:
 * 	alpha-blend (using the source per-surface alpha value);
 * 	set destination alpha to opaque.
 *     SDL_SRCALPHA not set:
 * 	copy RGB, set destination alpha to source per-surface alpha value.
 *     both:
 * 	if SDL_SRCCOLORKEY set, only copy the pixels matching the
 * 	source colour key.
 * 
 * RGBA->RGBA:
 *     SDL_SRCALPHA set:
 * 	alpha-blend (using the source alpha channel) the RGB values;
 * 	leave destination alpha untouched. [Note: is this correct?]
 * 	SDL_SRCCOLORKEY ignored.
 *     SDL_SRCALPHA not set:
 * 	copy all of RGBA to the destination.
 * 	if SDL_SRCCOLORKEY set, only copy the pixels matching the
 * 	RGB values of the source colour key, ignoring alpha in the
 * 	comparison.
 * 
 * RGB->RGB: 
 *     SDL_SRCALPHA set:
 * 	alpha-blend (using the source per-surface alpha value).
 *     SDL_SRCALPHA not set:
 * 	copy RGB.
 *     both:
 * 	if SDL_SRCCOLORKEY set, only copy the pixels matching the
 * 	source colour key.
 *
 * If either of the surfaces were in video memory, and the blit returns -2,
 * the video memory was lost, so it should be reloaded with artwork and 
 * re-blitted:
	while ( SDL_BlitSurface(image, imgrect, screen, dstrect) == -2 ) {
		while ( SDL_LockSurface(image) < 0 )
			Sleep(10);
		-- Write image pixels to image->pixels --
		SDL_UnlockSurface(image);
	}
 * This happens under DirectX 5.0 when the system switches away from your
 * fullscreen application.  The lock will also fail until you have access
 * to the video memory again.
 */
/* You should call SDL_BlitSurface() unless you know exactly how SDL
   blitting works internally and how to use the other blit functions.
*/
#define SDL_BlitSurface SDL_UpperBlit

/* This is the public blit function, SDL_BlitSurface(), and it performs
   rectangle validation and clipping before passing it to SDL_LowerBlit()
*/
extern DECLSPEC int SDLCALL SDL_UpperBlit
    (SDL_Surface * src, SDL_Rect * srcrect,
     SDL_Surface * dst, SDL_Rect * dstrect);
/* This is a semi-private blit function and it performs low-level surface
   blitting only.
*/
extern DECLSPEC int SDLCALL SDL_LowerBlit
    (SDL_Surface * src, SDL_Rect * srcrect,
     SDL_Surface * dst, SDL_Rect * dstrect);

/**
 * \fn int SDL_SoftStretch(SDL_Surface * src, const SDL_Rect * srcrect, SDL_Surface * dst, const SDL_Rect * dstrect)
 *
 * \brief Perform a fast, low quality, stretch blit between two surfaces of the same pixel format.
 *
 * \note This function uses a static buffer, and is not thread-safe.
 */
extern DECLSPEC int SDLCALL SDL_SoftStretch(SDL_Surface * src,
                                            const SDL_Rect * srcrect,
                                            SDL_Surface * dst,
                                            const SDL_Rect * dstrect);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_surface_h */

/* vi: set ts=4 sw=4 expandtab: */
