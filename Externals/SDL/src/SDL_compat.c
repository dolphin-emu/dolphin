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
#include "SDL_config.h"

/* This file contains functions for backwards compatibility with SDL 1.2 */

#include "SDL.h"
#include "SDL_syswm.h"

#include "video/SDL_sysvideo.h"
#include "video/SDL_pixels_c.h"
#include "video/SDL_yuv_sw_c.h"

static SDL_WindowID SDL_VideoWindow = 0;
static SDL_RendererInfo SDL_VideoRendererInfo;
static SDL_TextureID SDL_VideoTexture = 0;
static SDL_Surface *SDL_VideoSurface = NULL;
static SDL_Surface *SDL_ShadowSurface = NULL;
static SDL_Surface *SDL_PublicSurface = NULL;
static SDL_GLContext *SDL_VideoContext = NULL;
static Uint32 SDL_VideoFlags = 0;
static char *wm_title = NULL;
static SDL_Surface *SDL_VideoIcon;
static int SDL_enabled_UNICODE = 0;

char *
SDL_AudioDriverName(char *namebuf, int maxlen)
{
    const char *name = SDL_GetCurrentAudioDriver();
    if (name) {
        SDL_strlcpy(namebuf, name, maxlen);
        return namebuf;
    }
    return NULL;
}

char *
SDL_VideoDriverName(char *namebuf, int maxlen)
{
    const char *name = SDL_GetCurrentVideoDriver();
    if (name) {
        SDL_strlcpy(namebuf, name, maxlen);
        return namebuf;
    }
    return NULL;
}

const SDL_VideoInfo *
SDL_GetVideoInfo(void)
{
    static SDL_VideoInfo info;
    SDL_DisplayMode mode;

    /* Memory leak, compatibility code, who cares? */
    if (!info.vfmt && SDL_GetDesktopDisplayMode(&mode) == 0) {
        int bpp;
        Uint32 Rmask, Gmask, Bmask, Amask;

        SDL_PixelFormatEnumToMasks(mode.format, &bpp, &Rmask, &Gmask, &Bmask,
                                   &Amask);
        info.vfmt = SDL_AllocFormat(bpp, Rmask, Gmask, Bmask, Amask);
        info.current_w = mode.w;
        info.current_h = mode.h;
    }
    return &info;
}

int
SDL_VideoModeOK(int width, int height, int bpp, Uint32 flags)
{
    int i, actual_bpp = 0;

    if (!SDL_GetVideoDevice()) {
        return 0;
    }

    if (!(flags & SDL_FULLSCREEN)) {
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(&mode);
        return SDL_BITSPERPIXEL(mode.format);
    }

    for (i = 0; i < SDL_GetNumDisplayModes(); ++i) {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(i, &mode);
        if (!mode.w || !mode.h || (width == mode.w && height == mode.h)) {
            if (!mode.format) {
                return bpp;
            }
            if (SDL_BITSPERPIXEL(mode.format) >= (Uint32) bpp) {
                actual_bpp = SDL_BITSPERPIXEL(mode.format);
            }
        }
    }
    return actual_bpp;
}

SDL_Rect **
SDL_ListModes(const SDL_PixelFormat * format, Uint32 flags)
{
    int i, nmodes;
    SDL_Rect **modes;

    if (!SDL_GetVideoDevice()) {
        return NULL;
    }

    if (!(flags & SDL_FULLSCREEN)) {
        return (SDL_Rect **) (-1);
    }

    if (!format) {
        format = SDL_GetVideoInfo()->vfmt;
    }

    /* Memory leak, but this is a compatibility function, who cares? */
    nmodes = 0;
    modes = NULL;
    for (i = 0; i < SDL_GetNumDisplayModes(); ++i) {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(i, &mode);
        if (!mode.w || !mode.h) {
            return (SDL_Rect **) (-1);
        }
        if (SDL_BITSPERPIXEL(mode.format) != format->BitsPerPixel) {
            continue;
        }
        if (nmodes > 0 && modes[nmodes - 1]->w == mode.w
            && modes[nmodes - 1]->h == mode.h) {
            continue;
        }

        modes = SDL_realloc(modes, (nmodes + 2) * sizeof(*modes));
        if (!modes) {
            return NULL;
        }
        modes[nmodes] = (SDL_Rect *) SDL_malloc(sizeof(SDL_Rect));
        if (!modes[nmodes]) {
            return NULL;
        }
        modes[nmodes]->x = 0;
        modes[nmodes]->y = 0;
        modes[nmodes]->w = mode.w;
        modes[nmodes]->h = mode.h;
        ++nmodes;
    }
    if (modes) {
        modes[nmodes] = NULL;
    }
    return modes;
}

static int
SDL_CompatEventFilter(void *userdata, SDL_Event * event)
{
    SDL_Event fake;

    switch (event->type) {
    case SDL_WINDOWEVENT:
        switch (event->window.event) {
        case SDL_WINDOWEVENT_EXPOSED:
            if (!SDL_HasEvent(SDL_VIDEOEXPOSEMASK)) {
                fake.type = SDL_VIDEOEXPOSE;
                SDL_PushEvent(&fake);
            }
            break;
        case SDL_WINDOWEVENT_RESIZED:
            SDL_PeepEvents(&fake, 1, SDL_GETEVENT, SDL_VIDEORESIZEMASK);
            fake.type = SDL_VIDEORESIZE;
            fake.resize.w = event->window.data1;
            fake.resize.h = event->window.data2;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            fake.type = SDL_ACTIVEEVENT;
            fake.active.gain = 0;
            fake.active.state = SDL_APPACTIVE;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_RESTORED:
            fake.type = SDL_ACTIVEEVENT;
            fake.active.gain = 1;
            fake.active.state = SDL_APPACTIVE;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_ENTER:
            fake.type = SDL_ACTIVEEVENT;
            fake.active.gain = 1;
            fake.active.state = SDL_APPMOUSEFOCUS;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_LEAVE:
            fake.type = SDL_ACTIVEEVENT;
            fake.active.gain = 0;
            fake.active.state = SDL_APPMOUSEFOCUS;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            fake.type = SDL_ACTIVEEVENT;
            fake.active.gain = 1;
            fake.active.state = SDL_APPINPUTFOCUS;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            fake.type = SDL_ACTIVEEVENT;
            fake.active.gain = 0;
            fake.active.state = SDL_APPINPUTFOCUS;
            SDL_PushEvent(&fake);
            break;
        case SDL_WINDOWEVENT_CLOSE:
            fake.type = SDL_QUIT;
            SDL_PushEvent(&fake);
            break;
        }
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        {
            Uint32 unicode = 0;
            if (event->key.type == SDL_KEYDOWN && event->key.keysym.sym < 256) {
                unicode = event->key.keysym.sym;
                if (unicode >= 'a' && unicode <= 'z') {
                    int shifted = !!(event->key.keysym.mod & KMOD_SHIFT);
                    int capslock = !!(event->key.keysym.mod & KMOD_CAPS);
                    if ((shifted ^ capslock) != 0) {
                        unicode = SDL_toupper(unicode);
                    }
                }
            }
            if (unicode) {
                event->key.keysym.unicode = unicode;
            }
            break;
        }
    case SDL_TEXTINPUT:
        {
            /* FIXME: Generate an old style key repeat event if needed */
            //printf("TEXTINPUT: '%s'\n", event->text.text);
            break;
        }
    case SDL_MOUSEWHEEL:
        {
            Uint8 button;
            int selected;
            int x, y;

            if (event->wheel.y == 0) {
                break;
            }

            selected = SDL_SelectMouse(event->wheel.which);
            SDL_GetMouseState(selected, &x, &y);
            SDL_SelectMouse(selected);

            if (event->wheel.y > 0) {
                button = SDL_BUTTON_WHEELUP;
            } else {
                button = SDL_BUTTON_WHEELDOWN;
            }

            fake.button.which = event->wheel.windowID;
            fake.button.button = button;
            fake.button.x = x;
            fake.button.y = y;
            fake.button.windowID = event->wheel.windowID;

            fake.type = SDL_MOUSEBUTTONDOWN;
            fake.button.state = SDL_PRESSED;
            SDL_PushEvent(&fake);

            fake.type = SDL_MOUSEBUTTONUP;
            fake.button.state = SDL_RELEASED;
            SDL_PushEvent(&fake);
            break;
        }

    }
    return 1;
}

static int
SDL_VideoPaletteChanged(void *userdata, SDL_Palette * palette)
{
    if (userdata == SDL_ShadowSurface) {
        /* If the shadow palette changed, make the changes visible */
        if (!SDL_VideoSurface->format->palette) {
            SDL_UpdateRect(SDL_ShadowSurface, 0, 0, 0, 0);
        }
    }
    if (userdata == SDL_VideoSurface) {
        if (SDL_SetDisplayPalette(palette->colors, 0, palette->ncolors) < 0) {
            return -1;
        }
        if (SDL_SetTexturePalette
            (SDL_VideoTexture, palette->colors, 0, palette->ncolors) < 0) {
            return -1;
        }
    }
    return 0;
}

static void
GetEnvironmentWindowPosition(int w, int h, int *x, int *y)
{
    const char *window = SDL_getenv("SDL_VIDEO_WINDOW_POS");
    const char *center = SDL_getenv("SDL_VIDEO_CENTERED");
    if (window) {
        if (SDL_sscanf(window, "%d,%d", x, y) == 2) {
            return;
        }
        if (SDL_strcmp(window, "center") == 0) {
            center = window;
        }
    }
    if (center) {
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(&mode);
        *x = (mode.w - w) / 2;
        *y = (mode.h - h) / 2;
    }
}

static SDL_Surface *
CreateVideoSurface(SDL_TextureID textureID)
{
    SDL_Surface *surface;
    Uint32 format;
    int w, h;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    void *pixels;
    int pitch;

    if (SDL_QueryTexture(textureID, &format, NULL, &w, &h) < 0) {
        return NULL;
    }

    if (!SDL_PixelFormatEnumToMasks
        (format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
        SDL_SetError("Unknown texture format");
        return NULL;
    }

    if (SDL_QueryTexturePixels(textureID, &pixels, &pitch) == 0) {
        surface =
            SDL_CreateRGBSurfaceFrom(pixels, w, h, bpp, pitch, Rmask, Gmask,
                                     Bmask, Amask);
    } else {
        surface =
            SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);
    }
    return surface;
}

static void
ClearVideoSurface()
{
    Uint32 black;

    /* Clear the surface for display */
    black = SDL_MapRGB(SDL_PublicSurface->format, 0, 0, 0);
    SDL_FillRect(SDL_PublicSurface, NULL, black);
    SDL_UpdateRect(SDL_PublicSurface, 0, 0, 0, 0);
}

static void
SetupScreenSaver(int flags)
{
    const char *env;
    SDL_bool allow_screensaver;

    /* Allow environment override of screensaver disable */
    env = SDL_getenv("SDL_VIDEO_ALLOW_SCREENSAVER");
    if (env) {
        allow_screensaver = SDL_atoi(env) ? SDL_TRUE : SDL_FALSE;
    } else if (flags & SDL_FULLSCREEN) {
        allow_screensaver = SDL_FALSE;
    } else {
        allow_screensaver = SDL_TRUE;
    }
    if (allow_screensaver) {
        SDL_EnableScreenSaver();
    } else {
        SDL_DisableScreenSaver();
    }
}

int
SDL_ResizeVideoMode(int width, int height, int bpp, Uint32 flags)
{
    int w, h;
    Uint32 format;
    int access;
    void *pixels;
    int pitch;

    /* We can't resize something we don't have... */
    if (!SDL_VideoWindow) {
        return -1;
    }

    /* We probably have to recreate the window in fullscreen mode */
    if (flags & SDL_FULLSCREEN) {
        return -1;
    }

    /* I don't think there's any change we can gracefully make in flags */
    if (flags != SDL_VideoFlags) {
        return -1;
    }

    /* Resize the window */
    SDL_GetWindowSize(SDL_VideoWindow, &w, &h);
    if (w != width || h != height) {
        SDL_SetWindowSize(SDL_VideoWindow, width, height);
    }

    /* If we're in OpenGL mode, just resize the stub surface and we're done! */
    if (flags & SDL_OPENGL) {
        SDL_VideoSurface->w = width;
        SDL_VideoSurface->h = height;
        return 0;
    }

    /* Destroy the screen texture and recreate it */
    SDL_QueryTexture(SDL_VideoTexture, &format, &access, &w, &h);
    SDL_DestroyTexture(SDL_VideoTexture);
    SDL_VideoTexture = SDL_CreateTexture(format, access, width, height);
    if (!SDL_VideoTexture) {
        return -1;
    }

    SDL_VideoSurface->w = width;
    SDL_VideoSurface->h = height;
    if (SDL_QueryTexturePixels(SDL_VideoTexture, &pixels, &pitch) == 0) {
        SDL_VideoSurface->pixels = pixels;
        SDL_VideoSurface->pitch = pitch;
    } else {
        SDL_CalculatePitch(SDL_VideoSurface);
        SDL_VideoSurface->pixels =
            SDL_realloc(SDL_VideoSurface->pixels,
                        SDL_VideoSurface->h * SDL_VideoSurface->pitch);
    }
    SDL_SetClipRect(SDL_VideoSurface, NULL);
    SDL_InvalidateMap(SDL_VideoSurface->map);

    if (SDL_ShadowSurface) {
        SDL_ShadowSurface->w = width;
        SDL_ShadowSurface->h = height;
        SDL_ShadowSurface->pitch = SDL_CalculatePitch(SDL_ShadowSurface);
        SDL_ShadowSurface->pixels =
            SDL_realloc(SDL_ShadowSurface->pixels,
                        SDL_ShadowSurface->h * SDL_ShadowSurface->pitch);
        SDL_SetClipRect(SDL_ShadowSurface, NULL);
        SDL_InvalidateMap(SDL_ShadowSurface->map);
    }

    ClearVideoSurface();

    return 0;
}

SDL_Surface *
SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags)
{
    SDL_DisplayMode desktop_mode;
    SDL_DisplayMode mode;
    int window_x = SDL_WINDOWPOS_UNDEFINED;
    int window_y = SDL_WINDOWPOS_UNDEFINED;
    Uint32 window_flags;
    Uint32 desktop_format;
    Uint32 desired_format;
    Uint32 surface_flags;

    if (!SDL_GetVideoDevice()) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
            return NULL;
        }
    }

    SDL_GetDesktopDisplayMode(&desktop_mode);

    if (width == 0) {
        width = desktop_mode.w;
    }
    if (height == 0) {
        height = desktop_mode.h;
    }

    /* See if we can simply resize the existing window and surface */
    if (SDL_ResizeVideoMode(width, height, bpp, flags) == 0) {
        return SDL_PublicSurface;
    }

    /* Destroy existing window */
    SDL_PublicSurface = NULL;
    if (SDL_ShadowSurface) {
        SDL_FreeSurface(SDL_ShadowSurface);
        SDL_ShadowSurface = NULL;
    }
    if (SDL_VideoSurface) {
        SDL_DelPaletteWatch(SDL_VideoSurface->format->palette,
                            SDL_VideoPaletteChanged, NULL);
        SDL_FreeSurface(SDL_VideoSurface);
        SDL_VideoSurface = NULL;
    }
    if (SDL_VideoContext) {
        /* SDL_GL_MakeCurrent(0, NULL); *//* Doesn't do anything */
        SDL_GL_DeleteContext(SDL_VideoContext);
        SDL_VideoContext = NULL;
    }
    if (SDL_VideoWindow) {
        SDL_GetWindowPosition(SDL_VideoWindow, &window_x, &window_y);
        SDL_DestroyWindow(SDL_VideoWindow);
    }

    /* Set up the event filter */
    if (!SDL_GetEventFilter(NULL, NULL)) {
        SDL_SetEventFilter(SDL_CompatEventFilter, NULL);
    }

    /* Create a new window */
    window_flags = SDL_WINDOW_SHOWN;
    if (flags & SDL_FULLSCREEN) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (flags & SDL_OPENGL) {
        window_flags |= SDL_WINDOW_OPENGL;
    }
    if (flags & SDL_RESIZABLE) {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }
    if (flags & SDL_NOFRAME) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }
    GetEnvironmentWindowPosition(width, height, &window_x, &window_y);
    SDL_SetFullscreenDisplayMode(NULL);
    SDL_VideoWindow =
        SDL_CreateWindow(wm_title, window_x, window_y, width, height,
                         window_flags);
    if (!SDL_VideoWindow) {
        return NULL;
    }
    SDL_SetWindowIcon(SDL_VideoWindow, SDL_VideoIcon);

    window_flags = SDL_GetWindowFlags(SDL_VideoWindow);
    surface_flags = 0;
    if (window_flags & SDL_WINDOW_FULLSCREEN) {
        surface_flags |= SDL_FULLSCREEN;
    }
    if (window_flags & SDL_WINDOW_OPENGL) {
        surface_flags |= SDL_OPENGL;
    }
    if (window_flags & SDL_WINDOW_RESIZABLE) {
        surface_flags |= SDL_RESIZABLE;
    }
    if (window_flags & SDL_WINDOW_BORDERLESS) {
        surface_flags |= SDL_NOFRAME;
    }

    /* Set up the desired display mode */
    desktop_format = desktop_mode.format;
    if (desktop_format && ((flags & SDL_ANYFORMAT)
                           || (bpp == SDL_BITSPERPIXEL(desktop_format)))) {
        desired_format = desktop_format;
    } else {
        switch (bpp) {
        case 0:
            if (desktop_format) {
                desired_format = desktop_format;
            } else {
                desired_format = SDL_PIXELFORMAT_RGB888;
            }
            bpp = SDL_BITSPERPIXEL(desired_format);
            break;
        case 8:
            desired_format = SDL_PIXELFORMAT_INDEX8;
            break;
        case 15:
            desired_format = SDL_PIXELFORMAT_RGB555;
            break;
        case 16:
            desired_format = SDL_PIXELFORMAT_RGB565;
            break;
        case 24:
            desired_format = SDL_PIXELFORMAT_RGB24;
            break;
        case 32:
            desired_format = SDL_PIXELFORMAT_RGB888;
            break;
        default:
            SDL_SetError("Unsupported bpp in SDL_SetVideoMode()");
            return NULL;
        }
    }
    mode.format = desired_format;
    mode.w = width;
    mode.h = height;
    mode.refresh_rate = 0;

    /* Set the desired display mode */
    if (flags & SDL_FULLSCREEN) {
        if (SDL_SetFullscreenDisplayMode(&mode) < 0) {
            return NULL;
        }
    }

    /* If we're in OpenGL mode, just create a stub surface and we're done! */
    if (flags & SDL_OPENGL) {
        SDL_VideoContext = SDL_GL_CreateContext(SDL_VideoWindow);
        if (!SDL_VideoContext) {
            return NULL;
        }
        if (SDL_GL_MakeCurrent(SDL_VideoWindow, SDL_VideoContext) < 0) {
            return NULL;
        }
        SDL_VideoSurface =
            SDL_CreateRGBSurfaceFrom(NULL, width, height, bpp, 0, 0, 0, 0, 0);
        if (!SDL_VideoSurface) {
            return NULL;
        }
        SDL_VideoSurface->flags |= surface_flags;
        SDL_PublicSurface = SDL_VideoSurface;
        return SDL_PublicSurface;
    }

    /* Create a renderer for the window */
    if (SDL_CreateRenderer
        (SDL_VideoWindow, -1,
         SDL_RENDERER_SINGLEBUFFER | SDL_RENDERER_PRESENTDISCARD) < 0) {
        return NULL;
    }
    SDL_GetRendererInfo(&SDL_VideoRendererInfo);

    /* Create a texture for the screen surface */
    SDL_VideoTexture =
        SDL_CreateTexture(desired_format, SDL_TEXTUREACCESS_STREAMING, width,
                          height);

    if (!SDL_VideoTexture) {
        SDL_VideoTexture =
            SDL_CreateTexture(desktop_format,
                              SDL_TEXTUREACCESS_STREAMING, width, height);
    }
    if (!SDL_VideoTexture) {
        return NULL;
    }

    /* Create the screen surface */
    SDL_VideoSurface = CreateVideoSurface(SDL_VideoTexture);
    if (!SDL_VideoSurface) {
        return NULL;
    }
    SDL_VideoSurface->flags |= surface_flags;

    /* Set a default screen palette */
    if (SDL_VideoSurface->format->palette) {
        SDL_VideoSurface->flags |= SDL_HWPALETTE;
        SDL_DitherColors(SDL_VideoSurface->format->palette->colors,
                         SDL_VideoSurface->format->BitsPerPixel);
        SDL_AddPaletteWatch(SDL_VideoSurface->format->palette,
                            SDL_VideoPaletteChanged, SDL_VideoSurface);
        SDL_SetPaletteColors(SDL_VideoSurface->format->palette,
                             SDL_VideoSurface->format->palette->colors, 0,
                             SDL_VideoSurface->format->palette->ncolors);
    }

    /* Create a shadow surface if necessary */
    if ((bpp != SDL_VideoSurface->format->BitsPerPixel)
        && !(flags & SDL_ANYFORMAT)) {
        SDL_ShadowSurface =
            SDL_CreateRGBSurface(0, width, height, bpp, 0, 0, 0, 0);
        if (!SDL_ShadowSurface) {
            return NULL;
        }
        SDL_ShadowSurface->flags |= surface_flags;

        /* 8-bit SDL_ShadowSurface surfaces report that they have exclusive palette */
        if (SDL_ShadowSurface->format->palette) {
            SDL_ShadowSurface->flags |= SDL_HWPALETTE;
            if (SDL_VideoSurface->format->palette) {
                SDL_SetSurfacePalette(SDL_ShadowSurface,
                                      SDL_VideoSurface->format->palette);
            } else {
                SDL_DitherColors(SDL_ShadowSurface->format->palette->colors,
                                 SDL_ShadowSurface->format->BitsPerPixel);
            }
            SDL_AddPaletteWatch(SDL_ShadowSurface->format->palette,
                                SDL_VideoPaletteChanged, SDL_ShadowSurface);
        }
    }
    SDL_PublicSurface =
        (SDL_ShadowSurface ? SDL_ShadowSurface : SDL_VideoSurface);

    SDL_VideoFlags = flags;

    ClearVideoSurface();

    SetupScreenSaver(flags);

    /* We're finally done! */
    return SDL_PublicSurface;
}

SDL_Surface *
SDL_GetVideoSurface(void)
{
    return SDL_PublicSurface;
}

int
SDL_SetAlpha(SDL_Surface * surface, Uint32 flag, Uint8 value)
{
    if (flag & SDL_SRCALPHA) {
        /* According to the docs, value is ignored for alpha surfaces */
        if (surface->format->Amask) {
            value = 0xFF;
        }
        SDL_SetSurfaceAlphaMod(surface, value);
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    } else {
        SDL_SetSurfaceAlphaMod(surface, 0xFF);
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
    }
    SDL_SetSurfaceRLE(surface, (flag & SDL_RLEACCEL));

    return 0;
}

SDL_Surface *
SDL_DisplayFormat(SDL_Surface * surface)
{
    SDL_PixelFormat *format;

    if (!SDL_PublicSurface) {
        SDL_SetError("No video mode has been set");
        return NULL;
    }
    format = SDL_PublicSurface->format;

    /* Set the flags appropriate for copying to display surface */
    return SDL_ConvertSurface(surface, format, SDL_RLEACCEL);
}

SDL_Surface *
SDL_DisplayFormatAlpha(SDL_Surface * surface)
{
    SDL_PixelFormat *vf;
    SDL_PixelFormat *format;
    SDL_Surface *converted;
    /* default to ARGB8888 */
    Uint32 amask = 0xff000000;
    Uint32 rmask = 0x00ff0000;
    Uint32 gmask = 0x0000ff00;
    Uint32 bmask = 0x000000ff;

    if (!SDL_PublicSurface) {
        SDL_SetError("No video mode has been set");
        return NULL;
    }
    vf = SDL_PublicSurface->format;

    switch (vf->BytesPerPixel) {
    case 2:
        /* For XGY5[56]5, use, AXGY8888, where {X, Y} = {R, B}.
           For anything else (like ARGB4444) it doesn't matter
           since we have no special code for it anyway */
        if ((vf->Rmask == 0x1f) &&
            (vf->Bmask == 0xf800 || vf->Bmask == 0x7c00)) {
            rmask = 0xff;
            bmask = 0xff0000;
        }
        break;

    case 3:
    case 4:
        /* Keep the video format, as long as the high 8 bits are
           unused or alpha */
        if ((vf->Rmask == 0xff) && (vf->Bmask == 0xff0000)) {
            rmask = 0xff;
            bmask = 0xff0000;
        }
        break;

    default:
        /* We have no other optimised formats right now. When/if a new
           optimised alpha format is written, add the converter here */
        break;
    }
    format = SDL_AllocFormat(32, rmask, gmask, bmask, amask);
    converted = SDL_ConvertSurface(surface, format, SDL_RLEACCEL);
    SDL_FreeFormat(format);
    return converted;
}

int
SDL_Flip(SDL_Surface * screen)
{
    SDL_UpdateRect(screen, 0, 0, 0, 0);
    return 0;
}

void
SDL_UpdateRect(SDL_Surface * screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h)
{
    if (screen) {
        SDL_Rect rect;

        /* Fill the rectangle */
        rect.x = (int) x;
        rect.y = (int) y;
        rect.w = (int) (w ? w : screen->w);
        rect.h = (int) (h ? h : screen->h);
        SDL_UpdateRects(screen, 1, &rect);
    }
}

void
SDL_UpdateRects(SDL_Surface * screen, int numrects, SDL_Rect * rects)
{
    int i;

    if (screen == SDL_ShadowSurface) {
        for (i = 0; i < numrects; ++i) {
            SDL_LowerBlit(SDL_ShadowSurface, &rects[i], SDL_VideoSurface,
                          &rects[i]);
        }

        /* Fall through to video surface update */
        screen = SDL_VideoSurface;
    }
    if (screen == SDL_VideoSurface) {
        if (screen->flags & SDL_PREALLOC) {
            /* The surface memory is maintained by the renderer */
            SDL_DirtyTexture(SDL_VideoTexture, numrects, rects);
        } else {
            /* The surface memory needs to be copied to texture */
            int pitch = screen->pitch;
            int psize = screen->format->BytesPerPixel;
            for (i = 0; i < numrects; ++i) {
                const SDL_Rect *rect = &rects[i];
                void *pixels =
                    (Uint8 *) screen->pixels + rect->y * pitch +
                    rect->x * psize;
                SDL_UpdateTexture(SDL_VideoTexture, rect, pixels, pitch);
            }
        }
        if (SDL_VideoRendererInfo.flags & SDL_RENDERER_PRESENTCOPY) {
            for (i = 0; i < numrects; ++i) {
                SDL_RenderCopy(SDL_VideoTexture, &rects[i], &rects[i]);
            }
        } else {
            SDL_Rect rect;
            rect.x = 0;
            rect.y = 0;
            rect.w = screen->w;
            rect.h = screen->h;
            SDL_RenderCopy(SDL_VideoTexture, &rect, &rect);
        }
        SDL_RenderPresent();
    }
}

void
SDL_WM_SetCaption(const char *title, const char *icon)
{
    if (wm_title) {
        SDL_free(wm_title);
    }
    if (title) {
        wm_title = SDL_strdup(title);
    } else {
        wm_title = NULL;
    }
    SDL_SetWindowTitle(SDL_VideoWindow, wm_title);
}

void
SDL_WM_GetCaption(const char **title, const char **icon)
{
    if (title) {
        *title = wm_title;
    }
    if (icon) {
        *icon = "";
    }
}

void
SDL_WM_SetIcon(SDL_Surface * icon, Uint8 * mask)
{
    SDL_VideoIcon = icon;
}

int
SDL_WM_IconifyWindow(void)
{
    SDL_MinimizeWindow(SDL_VideoWindow);
    return 0;
}

int
SDL_WM_ToggleFullScreen(SDL_Surface * surface)
{
    if (SDL_GetWindowFlags(SDL_VideoWindow) & SDL_WINDOW_FULLSCREEN) {
        if (SDL_SetWindowFullscreen(SDL_VideoWindow, 0) < 0) {
            return 0;
        }
        SDL_PublicSurface->flags &= ~SDL_FULLSCREEN;
    } else {
        if (SDL_SetWindowFullscreen(SDL_VideoWindow, 1) < 0) {
            return 0;
        }
        SDL_PublicSurface->flags |= SDL_FULLSCREEN;
    }
    return 1;
}

SDL_GrabMode
SDL_WM_GrabInput(SDL_GrabMode mode)
{
    if (mode != SDL_GRAB_QUERY) {
        SDL_SetWindowGrab(SDL_VideoWindow, mode);
    }
    return (SDL_GrabMode) SDL_GetWindowGrab(SDL_VideoWindow);
}

void
SDL_WarpMouse(Uint16 x, Uint16 y)
{
    SDL_WarpMouseInWindow(SDL_VideoWindow, x, y);
}

Uint8
SDL_GetAppState(void)
{
    Uint8 state = 0;
    Uint32 flags = 0;

    flags = SDL_GetWindowFlags(SDL_VideoWindow);
    if ((flags & SDL_WINDOW_SHOWN) && !(flags & SDL_WINDOW_MINIMIZED)) {
        state |= SDL_APPACTIVE;
    }
    if (flags & SDL_WINDOW_INPUT_FOCUS) {
        state |= SDL_APPINPUTFOCUS;
    }
    if (flags & SDL_WINDOW_MOUSE_FOCUS) {
        state |= SDL_APPMOUSEFOCUS;
    }
    return state;
}

const SDL_version *
SDL_Linked_Version(void)
{
    static SDL_version version;
    SDL_VERSION(&version);
    return &version;
}

int
SDL_SetPalette(SDL_Surface * surface, int flags, const SDL_Color * colors,
               int firstcolor, int ncolors)
{
    return SDL_SetColors(surface, colors, firstcolor, ncolors);
}

int
SDL_SetColors(SDL_Surface * surface, const SDL_Color * colors, int firstcolor,
              int ncolors)
{
    if (SDL_SetPaletteColors
        (surface->format->palette, colors, firstcolor, ncolors) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int
SDL_GetWMInfo(SDL_SysWMinfo * info)
{
    return SDL_GetWindowWMInfo(SDL_VideoWindow, info);
}

#if 0
void
SDL_MoveCursor(int x, int y)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    /* Erase and update the current mouse position */
    if (SHOULD_DRAWCURSOR(SDL_cursorstate)) {
        /* Erase and redraw mouse cursor in new position */
        SDL_LockCursor();
        SDL_EraseCursor(SDL_VideoSurface);
        SDL_cursor->area.x = (x - SDL_cursor->hot_x);
        SDL_cursor->area.y = (y - SDL_cursor->hot_y);
        SDL_DrawCursor(SDL_VideoSurface);
        SDL_UnlockCursor();
    } else if (_this->MoveWMCursor) {
        _this->MoveWMCursor(_this, x, y);
    }
}

/* Keep track of the current cursor colors */
static int palette_changed = 1;
static Uint8 pixels8[2];

void
SDL_CursorPaletteChanged(void)
{
    palette_changed = 1;
}

void
SDL_MouseRect(SDL_Rect * area)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    int clip_diff;

    *area = SDL_cursor->area;
    if (area->x < 0) {
        area->w += area->x;
        area->x = 0;
    }
    if (area->y < 0) {
        area->h += area->y;
        area->y = 0;
    }
    clip_diff = (area->x + area->w) - SDL_VideoSurface->w;
    if (clip_diff > 0) {
        area->w = area->w < clip_diff ? 0 : area->w - clip_diff;
    }
    clip_diff = (area->y + area->h) - SDL_VideoSurface->h;
    if (clip_diff > 0) {
        area->h = area->h < clip_diff ? 0 : area->h - clip_diff;
    }
}

static void
SDL_DrawCursorFast(SDL_Surface * screen, SDL_Rect * area)
{
    const Uint32 pixels[2] = { 0xFFFFFFFF, 0x00000000 };
    int i, w, h;
    Uint8 *data, datab;
    Uint8 *mask, maskb;

    data = SDL_cursor->data + area->y * SDL_cursor->area.w / 8;
    mask = SDL_cursor->mask + area->y * SDL_cursor->area.w / 8;
    switch (screen->format->BytesPerPixel) {

    case 1:
        {
            Uint8 *dst;
            int dstskip;

            if (palette_changed) {
                pixels8[0] =
                    (Uint8) SDL_MapRGB(screen->format, 255, 255, 255);
                pixels8[1] = (Uint8) SDL_MapRGB(screen->format, 0, 0, 0);
                palette_changed = 0;
            }
            dst = (Uint8 *) screen->pixels +
                (SDL_cursor->area.y + area->y) * screen->pitch +
                SDL_cursor->area.x;
            dstskip = screen->pitch - area->w;

            for (h = area->h; h; h--) {
                for (w = area->w / 8; w; w--) {
                    maskb = *mask++;
                    datab = *data++;
                    for (i = 0; i < 8; ++i) {
                        if (maskb & 0x80) {
                            *dst = pixels8[datab >> 7];
                        }
                        maskb <<= 1;
                        datab <<= 1;
                        dst++;
                    }
                }
                dst += dstskip;
            }
        }
        break;

    case 2:
        {
            Uint16 *dst;
            int dstskip;

            dst = (Uint16 *) screen->pixels +
                (SDL_cursor->area.y + area->y) * screen->pitch / 2 +
                SDL_cursor->area.x;
            dstskip = (screen->pitch / 2) - area->w;

            for (h = area->h; h; h--) {
                for (w = area->w / 8; w; w--) {
                    maskb = *mask++;
                    datab = *data++;
                    for (i = 0; i < 8; ++i) {
                        if (maskb & 0x80) {
                            *dst = (Uint16) pixels[datab >> 7];
                        }
                        maskb <<= 1;
                        datab <<= 1;
                        dst++;
                    }
                }
                dst += dstskip;
            }
        }
        break;

    case 3:
        {
            Uint8 *dst;
            int dstskip;

            dst = (Uint8 *) screen->pixels +
                (SDL_cursor->area.y + area->y) * screen->pitch +
                SDL_cursor->area.x * 3;
            dstskip = screen->pitch - area->w * 3;

            for (h = area->h; h; h--) {
                for (w = area->w / 8; w; w--) {
                    maskb = *mask++;
                    datab = *data++;
                    for (i = 0; i < 8; ++i) {
                        if (maskb & 0x80) {
                            SDL_memset(dst, pixels[datab >> 7], 3);
                        }
                        maskb <<= 1;
                        datab <<= 1;
                        dst += 3;
                    }
                }
                dst += dstskip;
            }
        }
        break;

    case 4:
        {
            Uint32 *dst;
            int dstskip;

            dst = (Uint32 *) screen->pixels +
                (SDL_cursor->area.y + area->y) * screen->pitch / 4 +
                SDL_cursor->area.x;
            dstskip = (screen->pitch / 4) - area->w;

            for (h = area->h; h; h--) {
                for (w = area->w / 8; w; w--) {
                    maskb = *mask++;
                    datab = *data++;
                    for (i = 0; i < 8; ++i) {
                        if (maskb & 0x80) {
                            *dst = pixels[datab >> 7];
                        }
                        maskb <<= 1;
                        datab <<= 1;
                        dst++;
                    }
                }
                dst += dstskip;
            }
        }
        break;
    }
}

static void
SDL_DrawCursorSlow(SDL_Surface * screen, SDL_Rect * area)
{
    const Uint32 pixels[2] = { 0xFFFFFF, 0x000000 };
    int h;
    int x, minx, maxx;
    Uint8 *data, datab = 0;
    Uint8 *mask, maskb = 0;
    Uint8 *dst;
    int dstbpp, dstskip;

    data = SDL_cursor->data + area->y * SDL_cursor->area.w / 8;
    mask = SDL_cursor->mask + area->y * SDL_cursor->area.w / 8;
    dstbpp = screen->format->BytesPerPixel;
    dst = (Uint8 *) screen->pixels +
        (SDL_cursor->area.y + area->y) * screen->pitch +
        SDL_cursor->area.x * dstbpp;
    dstskip = screen->pitch - SDL_cursor->area.w * dstbpp;

    minx = area->x;
    maxx = area->x + area->w;
    if (screen->format->BytesPerPixel == 1) {
        if (palette_changed) {
            pixels8[0] = (Uint8) SDL_MapRGB(screen->format, 255, 255, 255);
            pixels8[1] = (Uint8) SDL_MapRGB(screen->format, 0, 0, 0);
            palette_changed = 0;
        }
        for (h = area->h; h; h--) {
            for (x = 0; x < SDL_cursor->area.w; ++x) {
                if ((x % 8) == 0) {
                    maskb = *mask++;
                    datab = *data++;
                }
                if ((x >= minx) && (x < maxx)) {
                    if (maskb & 0x80) {
                        SDL_memset(dst, pixels8[datab >> 7], dstbpp);
                    }
                }
                maskb <<= 1;
                datab <<= 1;
                dst += dstbpp;
            }
            dst += dstskip;
        }
    } else {
        for (h = area->h; h; h--) {
            for (x = 0; x < SDL_cursor->area.w; ++x) {
                if ((x % 8) == 0) {
                    maskb = *mask++;
                    datab = *data++;
                }
                if ((x >= minx) && (x < maxx)) {
                    if (maskb & 0x80) {
                        SDL_memset(dst, pixels[datab >> 7], dstbpp);
                    }
                }
                maskb <<= 1;
                datab <<= 1;
                dst += dstbpp;
            }
            dst += dstskip;
        }
    }
}

/* This handles the ugly work of converting the saved cursor background from
   the pixel format of the shadow surface to that of the video surface.
   This is only necessary when blitting from a shadow surface of a different
   pixel format than the video surface, and using a software rendered cursor.
*/
static void
SDL_ConvertCursorSave(SDL_Surface * screen, int w, int h)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_BlitInfo info;
    SDL_loblit RunBlit;

    /* Make sure we can steal the blit mapping */
    if (screen->map->dst != SDL_VideoSurface) {
        return;
    }

    /* Set up the blit information */
    info.s_pixels = SDL_cursor->save[1];
    info.s_width = w;
    info.s_height = h;
    info.s_skip = 0;
    info.d_pixels = SDL_cursor->save[0];
    info.d_width = w;
    info.d_height = h;
    info.d_skip = 0;
    info.aux_data = screen->map->sw_data->aux_data;
    info.src = screen->format;
    info.table = screen->map->table;
    info.dst = SDL_VideoSurface->format;
    RunBlit = screen->map->sw_data->blit;

    /* Run the actual software blit */
    RunBlit(&info);
}

void
SDL_DrawCursorNoLock(SDL_Surface * screen)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Rect area;

    /* Get the mouse rectangle, clipped to the screen */
    SDL_MouseRect(&area);
    if ((area.w == 0) || (area.h == 0)) {
        return;
    }

    /* Copy mouse background */
    {
        int w, h, screenbpp;
        Uint8 *src, *dst;

        /* Set up the copy pointers */
        screenbpp = screen->format->BytesPerPixel;
        if ((screen == SDL_VideoSurface) ||
            FORMAT_EQUAL(screen->format, SDL_VideoSurface->format)) {
            dst = SDL_cursor->save[0];
        } else {
            dst = SDL_cursor->save[1];
        }
        src = (Uint8 *) screen->pixels + area.y * screen->pitch +
            area.x * screenbpp;

        /* Perform the copy */
        w = area.w * screenbpp;
        h = area.h;
        while (h--) {
            SDL_memcpy(dst, src, w);
            dst += w;
            src += screen->pitch;
        }
    }

    /* Draw the mouse cursor */
    area.x -= SDL_cursor->area.x;
    area.y -= SDL_cursor->area.y;
    if ((area.x == 0) && (area.w == SDL_cursor->area.w)) {
        SDL_DrawCursorFast(screen, &area);
    } else {
        SDL_DrawCursorSlow(screen, &area);
    }
}

void
SDL_DrawCursor(SDL_Surface * screen)
{
    /* Lock the screen if necessary */
    if (screen == NULL) {
        return;
    }
    if (SDL_MUSTLOCK(screen)) {
        if (SDL_LockSurface(screen) < 0) {
            return;
        }
    }

    SDL_DrawCursorNoLock(screen);

    /* Unlock the screen and update if necessary */
    if (SDL_MUSTLOCK(screen)) {
        SDL_UnlockSurface(screen);
    }
    if (screen->flags & SDL_SCREEN_SURFACE) {
        SDL_VideoDevice *_this = SDL_GetVideoDevice();
        SDL_Window *window;
        SDL_Rect area;

        window = SDL_GetWindowFromSurface(screen);
        if (!window) {
            return;
        }

        SDL_MouseRect(&area);

        if (_this->UpdateWindowSurface) {
            _this->UpdateWindowSurface(_this, window, 1, &area);
        }
    }
}

void
SDL_EraseCursorNoLock(SDL_Surface * screen)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Window *window;
    SDL_Rect area;

    /* Get the window associated with the surface */
    window = SDL_GetWindowFromSurface(screen);
    if (!window || !window->surface) {
        return;
    }

    /* Get the mouse rectangle, clipped to the screen */
    SDL_MouseRect(&area);
    if ((area.w == 0) || (area.h == 0)) {
        return;
    }

    /* Copy mouse background */
    {
        int w, h, screenbpp;
        Uint8 *src, *dst;

        /* Set up the copy pointers */
        screenbpp = screen->format->BytesPerPixel;
        if ((screen->flags & SDL_SCREEN_SURFACE) ||
            FORMAT_EQUAL(screen->format, window->surface->format)) {
            src = SDL_cursor->save[0];
        } else {
            src = SDL_cursor->save[1];
        }
        dst = (Uint8 *) screen->pixels + area.y * screen->pitch +
            area.x * screenbpp;

        /* Perform the copy */
        w = area.w * screenbpp;
        h = area.h;
        while (h--) {
            SDL_memcpy(dst, src, w);
            src += w;
            dst += screen->pitch;
        }

        /* Perform pixel conversion on cursor background */
        if (src > SDL_cursor->save[1]) {
            SDL_ConvertCursorSave(screen, area.w, area.h);
        }
    }
}

void
SDL_EraseCursor(SDL_Surface * screen)
{
    /* Lock the screen if necessary */
    if (screen == NULL) {
        return;
    }
    if (SDL_MUSTLOCK(screen)) {
        if (SDL_LockSurface(screen) < 0) {
            return;
        }
    }

    SDL_EraseCursorNoLock(screen);

    /* Unlock the screen and update if necessary */
    if (SDL_MUSTLOCK(screen)) {
        SDL_UnlockSurface(screen);
    }
    if (screen->flags & SDL_SCREEN_SURFACE) {
        SDL_VideoDevice *_this = SDL_GetVideoDevice();
        SDL_Window *window;
        SDL_Rect area;

        window = SDL_GetWindowFromSurface(screen);
        if (!window) {
            return;
        }

        SDL_MouseRect(&area);

        if (_this->UpdateWindowSurface) {
            _this->UpdateWindowSurface(_this, window, 1, &area);
        }
    }
}

/* Reset the cursor on video mode change
   FIXME:  Keep track of all cursors, and reset them all.
 */
void
SDL_ResetCursor(void)
{
    int savelen;

    if (SDL_cursor) {
        savelen = SDL_cursor->area.w * 4 * SDL_cursor->area.h;
        SDL_cursor->area.x = 0;
        SDL_cursor->area.y = 0;
        SDL_memset(SDL_cursor->save[0], 0, savelen);
    }
}
#endif

struct private_yuvhwdata
{
    Uint16 pitches[3];
    Uint8 *planes[3];

    SDL_SW_YUVTexture *sw;

    SDL_TextureID textureID;
    Uint32 texture_format;
};

SDL_Overlay *
SDL_CreateYUVOverlay(int w, int h, Uint32 format, SDL_Surface * display)
{
    SDL_Overlay *overlay;
    Uint32 texture_format;

    if ((display->flags & SDL_OPENGL) == SDL_OPENGL) {
        SDL_SetError("YUV overlays are not supported in OpenGL mode");
        return NULL;
    }

    if (display != SDL_PublicSurface) {
        SDL_SetError("YUV display is only supported on the screen surface");
        return NULL;
    }

    switch (format) {
    case SDL_YV12_OVERLAY:
        texture_format = SDL_PIXELFORMAT_YV12;
        break;
    case SDL_IYUV_OVERLAY:
        texture_format = SDL_PIXELFORMAT_IYUV;
        break;
    case SDL_YUY2_OVERLAY:
        texture_format = SDL_PIXELFORMAT_YUY2;
        break;
    case SDL_UYVY_OVERLAY:
        texture_format = SDL_PIXELFORMAT_UYVY;
        break;
    case SDL_YVYU_OVERLAY:
        texture_format = SDL_PIXELFORMAT_YVYU;
        break;
    default:
        SDL_SetError("Unknown YUV format");
        return NULL;
    }

    overlay = (SDL_Overlay *) SDL_malloc(sizeof(*overlay));
    if (!overlay) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zerop(overlay);

    overlay->hwdata =
        (struct private_yuvhwdata *) SDL_malloc(sizeof(*overlay->hwdata));
    if (!overlay->hwdata) {
        SDL_free(overlay);
        SDL_OutOfMemory();
        return NULL;
    }

    overlay->format = format;
    overlay->w = w;
    overlay->h = h;
    if (format == SDL_YV12_OVERLAY || format == SDL_IYUV_OVERLAY) {
        overlay->planes = 3;
    } else {
        overlay->planes = 1;
    }
    overlay->pitches = overlay->hwdata->pitches;
    overlay->pixels = overlay->hwdata->planes;

    switch (format) {
    case SDL_YV12_OVERLAY:
    case SDL_IYUV_OVERLAY:
        overlay->pitches[0] = overlay->w;
        overlay->pitches[1] = overlay->w / 2;
        overlay->pitches[2] = overlay->w / 2;
        break;
    case SDL_YUY2_OVERLAY:
    case SDL_UYVY_OVERLAY:
    case SDL_YVYU_OVERLAY:
        overlay->pitches[0] = overlay->w * 2;
        break;
    }

    overlay->hwdata->textureID =
        SDL_CreateTexture(texture_format, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (overlay->hwdata->textureID) {
        overlay->hwdata->sw = NULL;
    } else {
        SDL_DisplayMode current_mode;

        overlay->hwdata->sw = SDL_SW_CreateYUVTexture(texture_format, w, h);
        if (!overlay->hwdata->sw) {
            SDL_FreeYUVOverlay(overlay);
            return NULL;
        }

        /* Create a supported RGB format texture for display */
        SDL_GetCurrentDisplayMode(&current_mode);
        texture_format = current_mode.format;
        overlay->hwdata->textureID =
            SDL_CreateTexture(texture_format,
                              SDL_TEXTUREACCESS_STREAMING, w, h);
    }
    if (!overlay->hwdata->textureID) {
        SDL_FreeYUVOverlay(overlay);
        return NULL;
    }
    overlay->hwdata->texture_format = texture_format;

    return overlay;
}

int
SDL_LockYUVOverlay(SDL_Overlay * overlay)
{
    void *pixels;
    int pitch;

    if (!overlay) {
        SDL_SetError("Passed a NULL overlay");
        return -1;
    }
    if (overlay->hwdata->sw) {
        if (SDL_SW_QueryYUVTexturePixels(overlay->hwdata->sw, &pixels, &pitch)
            < 0) {
            return -1;
        }
    } else {
        if (SDL_LockTexture
            (overlay->hwdata->textureID, NULL, 1, &pixels, &pitch)
            < 0) {
            return -1;
        }
    }
    overlay->pixels[0] = (Uint8 *) pixels;
    overlay->pitches[0] = pitch;
    switch (overlay->format) {
    case SDL_YV12_OVERLAY:
    case SDL_IYUV_OVERLAY:
        overlay->pitches[1] = pitch / 2;
        overlay->pitches[2] = pitch / 2;
        overlay->pixels[1] =
            overlay->pixels[0] + overlay->pitches[0] * overlay->h;
        overlay->pixels[2] =
            overlay->pixels[1] + overlay->pitches[1] * overlay->h / 2;
        break;
    case SDL_YUY2_OVERLAY:
    case SDL_UYVY_OVERLAY:
    case SDL_YVYU_OVERLAY:
        break;
    }
    return 0;
}

void
SDL_UnlockYUVOverlay(SDL_Overlay * overlay)
{
    if (!overlay) {
        return;
    }
    if (overlay->hwdata->sw) {
        void *pixels;
        int pitch;
        if (SDL_LockTexture
            (overlay->hwdata->textureID, NULL, 1, &pixels, &pitch) == 0) {
            SDL_Rect srcrect;

            srcrect.x = 0;
            srcrect.y = 0;
            srcrect.w = overlay->w;
            srcrect.h = overlay->h;
            SDL_SW_CopyYUVToRGB(overlay->hwdata->sw, &srcrect,
                                overlay->hwdata->texture_format,
                                overlay->w, overlay->h, pixels, pitch);
            SDL_UnlockTexture(overlay->hwdata->textureID);
        }
    } else {
        SDL_UnlockTexture(overlay->hwdata->textureID);
    }
}

int
SDL_DisplayYUVOverlay(SDL_Overlay * overlay, SDL_Rect * dstrect)
{
    if (!overlay || !dstrect) {
        SDL_SetError("Passed a NULL overlay or dstrect");
        return -1;
    }
    if (SDL_RenderCopy(overlay->hwdata->textureID, NULL, dstrect) < 0) {
        return -1;
    }
    SDL_RenderPresent();
    return 0;
}

void
SDL_FreeYUVOverlay(SDL_Overlay * overlay)
{
    if (!overlay) {
        return;
    }
    if (overlay->hwdata) {
        if (overlay->hwdata->textureID) {
            SDL_DestroyTexture(overlay->hwdata->textureID);
        }
        SDL_free(overlay->hwdata);
    }
    SDL_free(overlay);
}

void
SDL_GL_SwapBuffers(void)
{
    SDL_GL_SwapWindow(SDL_VideoWindow);
}


int
SDL_EnableKeyRepeat(int delay, int interval)
{
    return 0;
}

void
SDL_GetKeyRepeat(int *delay, int *interval)
{
    if (delay) {
        *delay = SDL_DEFAULT_REPEAT_DELAY;
    }
    if (interval) {
        *interval = SDL_DEFAULT_REPEAT_INTERVAL;
    }
}

int
SDL_EnableUNICODE(int enable)
{
    int previous = SDL_enabled_UNICODE;

    switch (enable) {
    case 1:
        SDL_enabled_UNICODE = 1;
        SDL_StartTextInput();
        break;
    case 0:
        SDL_enabled_UNICODE = 0;
        SDL_StopTextInput();
        break;
    }
    return previous;
}

/* vi: set ts=4 sw=4 expandtab: */
