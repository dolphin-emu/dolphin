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

/* The high-level video driver subsystem */

#include "SDL.h"
#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_pixels_c.h"
#include "SDL_renderer_gl.h"
#include "SDL_renderer_gles.h"
#include "SDL_renderer_sw.h"
#include "../events/SDL_sysevents.h"
#include "../events/SDL_events_c.h"

#if SDL_VIDEO_OPENGL_ES
#include "SDL_opengles.h"
#endif /* SDL_VIDEO_OPENGL_ES */

#if SDL_VIDEO_OPENGL
#include "SDL_opengl.h"

/* On Windows, windows.h defines CreateWindow */
#ifdef CreateWindow
#undef CreateWindow
#endif
#endif /* SDL_VIDEO_OPENGL */

/* Available video drivers */
static VideoBootStrap *bootstrap[] = {
#if SDL_VIDEO_DRIVER_COCOA
    &COCOA_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_X11
    &X11_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_FBCON
    &FBCON_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_DIRECTFB
    &DirectFB_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_PS2GS
    &PS2GS_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_PS3
    &PS3_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_SVGALIB
    &SVGALIB_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_GAPI
    &GAPI_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_WIN32
    &WIN32_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_BWINDOW
    &BWINDOW_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_PHOTON
    &photon_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_QNXGF
    &qnxgf_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_EPOC
    &EPOC_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_RISCOS
    &RISCOS_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_NDS
    &NDS_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_UIKIT
    &UIKIT_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_DUMMY
    &DUMMY_bootstrap,
#endif
#if SDL_VIDEO_DRIVER_PANDORA
    &PND_bootstrap,
#endif
    NULL
};

static SDL_VideoDevice *_this = NULL;

/* Various local functions */
static void SDL_UpdateWindowGrab(SDL_Window * window);

static int
cmpmodes(const void *A, const void *B)
{
    SDL_DisplayMode a = *(const SDL_DisplayMode *) A;
    SDL_DisplayMode b = *(const SDL_DisplayMode *) B;

    if (a.w != b.w) {
        return b.w - a.w;
    }
    if (a.h != b.h) {
        return b.h - a.h;
    }
    if (SDL_BITSPERPIXEL(a.format) != SDL_BITSPERPIXEL(b.format)) {
        return SDL_BITSPERPIXEL(b.format) - SDL_BITSPERPIXEL(a.format);
    }
    if (SDL_PIXELLAYOUT(a.format) != SDL_PIXELLAYOUT(b.format)) {
        return SDL_PIXELLAYOUT(b.format) - SDL_PIXELLAYOUT(a.format);
    }
    if (a.refresh_rate != b.refresh_rate) {
        return b.refresh_rate - a.refresh_rate;
    }
    return 0;
}

static void
SDL_UninitializedVideo()
{
    SDL_SetError("Video subsystem has not been initialized");
}

int
SDL_GetNumVideoDrivers(void)
{
    return SDL_arraysize(bootstrap) - 1;
}

const char *
SDL_GetVideoDriver(int index)
{
    if (index >= 0 && index < SDL_GetNumVideoDrivers()) {
        return bootstrap[index]->name;
    }
    return NULL;
}

/*
 * Initialize the video and event subsystems -- determine native pixel format
 */
int
SDL_VideoInit(const char *driver_name, Uint32 flags)
{
    SDL_VideoDevice *video;
    int index;
    int i;

    /* Toggle the event thread flags, based on OS requirements */
#if defined(MUST_THREAD_EVENTS)
    flags |= SDL_INIT_EVENTTHREAD;
#elif defined(CANT_THREAD_EVENTS)
    if ((flags & SDL_INIT_EVENTTHREAD) == SDL_INIT_EVENTTHREAD) {
        SDL_SetError("OS doesn't support threaded events");
        return -1;
    }
#endif

    /* Start the event loop */
    if (SDL_StartEventLoop(flags) < 0) {
        return -1;
    }
    /* Check to make sure we don't overwrite '_this' */
    if (_this != NULL) {
        SDL_VideoQuit();
    }
    /* Select the proper video driver */
    index = 0;
    video = NULL;
    if (driver_name == NULL) {
        driver_name = SDL_getenv("SDL_VIDEODRIVER");
    }
    if (driver_name != NULL) {
        for (i = 0; bootstrap[i]; ++i) {
            if (SDL_strcasecmp(bootstrap[i]->name, driver_name) == 0) {
                if (bootstrap[i]->available()) {
                    video = bootstrap[i]->create(index);
                }
                break;
            }
        }
    } else {
        for (i = 0; bootstrap[i]; ++i) {
            if (bootstrap[i]->available()) {
                video = bootstrap[i]->create(index);
                if (video != NULL) {
                    break;
                }
            }
        }
    }
    if (video == NULL) {
        if (driver_name) {
            SDL_SetError("%s not available", driver_name);
        } else {
            SDL_SetError("No available video device");
        }
        return -1;
    }
    _this = video;
    _this->name = bootstrap[i]->name;
    _this->next_object_id = 1;


    /* Set some very sane GL defaults */
    _this->gl_config.driver_loaded = 0;
    _this->gl_config.dll_handle = NULL;
    _this->gl_config.red_size = 3;
    _this->gl_config.green_size = 3;
    _this->gl_config.blue_size = 2;
    _this->gl_config.alpha_size = 0;
    _this->gl_config.buffer_size = 0;
    _this->gl_config.depth_size = 16;
    _this->gl_config.stencil_size = 0;
    _this->gl_config.double_buffer = 1;
    _this->gl_config.accum_red_size = 0;
    _this->gl_config.accum_green_size = 0;
    _this->gl_config.accum_blue_size = 0;
    _this->gl_config.accum_alpha_size = 0;
    _this->gl_config.stereo = 0;
    _this->gl_config.multisamplebuffers = 0;
    _this->gl_config.multisamplesamples = 0;
    _this->gl_config.retained_backing = 1;
    _this->gl_config.accelerated = -1;  /* not known, don't set */
    _this->gl_config.major_version = 2;
    _this->gl_config.minor_version = 1;

    /* Initialize the video subsystem */
    if (_this->VideoInit(_this) < 0) {
        SDL_VideoQuit();
        return -1;
    }
    /* Make sure some displays were added */
    if (_this->num_displays == 0) {
        SDL_SetError("The video driver did not add any displays");
        SDL_VideoQuit();
        return (-1);
    }
    /* The software renderer is always available */
    for (i = 0; i < _this->num_displays; ++i) {
#if SDL_VIDEO_RENDER_OGL
        SDL_AddRenderDriver(i, &GL_RenderDriver);
#endif

#if SDL_VIDEO_RENDER_OGL_ES
        SDL_AddRenderDriver(i, &GL_ES_RenderDriver);
#endif
        if (_this->displays[i].num_render_drivers > 0) {
            SDL_AddRenderDriver(i, &SW_RenderDriver);
        }
    }

    /* We're ready to go! */
    return 0;
}

const char *
SDL_GetCurrentVideoDriver()
{
    if (!_this) {
        SDL_UninitializedVideo();
        return NULL;
    }
    return _this->name;
}

SDL_VideoDevice *
SDL_GetVideoDevice()
{
    return _this;
}

int
SDL_AddBasicVideoDisplay(const SDL_DisplayMode * desktop_mode)
{
    SDL_VideoDisplay display;

    SDL_zero(display);
    if (desktop_mode) {
        display.desktop_mode = *desktop_mode;
    }
    display.current_mode = display.desktop_mode;

    return SDL_AddVideoDisplay(&display);
}

int
SDL_AddVideoDisplay(const SDL_VideoDisplay * display)
{
    SDL_VideoDisplay *displays;
    int index = -1;

    displays =
        SDL_realloc(_this->displays,
                    (_this->num_displays + 1) * sizeof(*displays));
    if (displays) {
        index = _this->num_displays++;
        displays[index] = *display;
        displays[index].device = _this;
        _this->displays = displays;
    } else {
        SDL_OutOfMemory();
    }
    return index;
}

int
SDL_GetNumVideoDisplays(void)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return 0;
    }
    return _this->num_displays;
}

int
SDL_SelectVideoDisplay(int index)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return (-1);
    }
    if (index < 0 || index >= _this->num_displays) {
        SDL_SetError("index must be in the range 0 - %d",
                     _this->num_displays - 1);
        return -1;
    }
    _this->current_display = index;
    return 0;
}

int
SDL_GetCurrentVideoDisplay(void)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return (-1);
    }
    return _this->current_display;
}

SDL_bool
SDL_AddDisplayMode(int displayIndex, const SDL_DisplayMode * mode)
{
    SDL_VideoDisplay *display = &_this->displays[displayIndex];
    SDL_DisplayMode *modes;
    int i, nmodes;

    /* Make sure we don't already have the mode in the list */
    modes = display->display_modes;
    nmodes = display->num_display_modes;
    for (i = nmodes; i--;) {
        if (SDL_memcmp(mode, &modes[i], sizeof(*mode)) == 0) {
            return SDL_FALSE;
        }
    }

    /* Go ahead and add the new mode */
    if (nmodes == display->max_display_modes) {
        modes =
            SDL_realloc(modes,
                        (display->max_display_modes + 32) * sizeof(*modes));
        if (!modes) {
            return SDL_FALSE;
        }
        display->display_modes = modes;
        display->max_display_modes += 32;
    }
    modes[nmodes] = *mode;
    display->num_display_modes++;

    /* Re-sort video modes */
    SDL_qsort(display->display_modes, display->num_display_modes,
              sizeof(SDL_DisplayMode), cmpmodes);

    return SDL_TRUE;
}

int
SDL_GetNumDisplayModes()
{
    if (_this) {
        SDL_VideoDisplay *display = &SDL_CurrentDisplay;
        if (!display->num_display_modes && _this->GetDisplayModes) {
            _this->GetDisplayModes(_this);
            SDL_qsort(display->display_modes, display->num_display_modes,
                      sizeof(SDL_DisplayMode), cmpmodes);
        }
        return display->num_display_modes;
    }
    return 0;
}

int
SDL_GetDisplayMode(int index, SDL_DisplayMode * mode)
{
    if (index < 0 || index >= SDL_GetNumDisplayModes()) {
        SDL_SetError("index must be in the range of 0 - %d",
                     SDL_GetNumDisplayModes() - 1);
        return -1;
    }
    if (mode) {
        *mode = SDL_CurrentDisplay.display_modes[index];
    }
    return 0;
}

int
SDL_GetDesktopDisplayMode(SDL_DisplayMode * mode)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (mode) {
        *mode = SDL_CurrentDisplay.desktop_mode;
    }
    return 0;
}

int
SDL_GetCurrentDisplayMode(SDL_DisplayMode * mode)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (mode) {
        *mode = SDL_CurrentDisplay.current_mode;
    }
    return 0;
}

SDL_DisplayMode *
SDL_GetClosestDisplayMode(const SDL_DisplayMode * mode,
                          SDL_DisplayMode * closest)
{
    Uint32 target_format;
    int target_refresh_rate;
    int i;
    SDL_DisplayMode *current, *match;

    if (!_this || !mode || !closest) {
        return NULL;
    }
    /* Default to the desktop format */
    if (mode->format) {
        target_format = mode->format;
    } else {
        target_format = SDL_CurrentDisplay.desktop_mode.format;
    }

    /* Default to the desktop refresh rate */
    if (mode->refresh_rate) {
        target_refresh_rate = mode->refresh_rate;
    } else {
        target_refresh_rate = SDL_CurrentDisplay.desktop_mode.refresh_rate;
    }

    match = NULL;
    for (i = 0; i < SDL_GetNumDisplayModes(); ++i) {
        current = &SDL_CurrentDisplay.display_modes[i];

        if (current->w && (current->w < mode->w)) {
            /* Out of sorted modes large enough here */
            break;
        }
        if (current->h && (current->h < mode->h)) {
            if (current->w && (current->w == mode->w)) {
                /* Out of sorted modes large enough here */
                break;
            }
            /* Wider, but not tall enough, due to a different
               aspect ratio. This mode must be skipped, but closer
               modes may still follow. */
            continue;
        }
        if (!match || current->w < match->w || current->h < match->h) {
            match = current;
            continue;
        }
        if (current->format != match->format) {
            /* Sorted highest depth to lowest */
            if (current->format == target_format ||
                (SDL_BITSPERPIXEL(current->format) >=
                 SDL_BITSPERPIXEL(target_format)
                 && SDL_PIXELTYPE(current->format) ==
                 SDL_PIXELTYPE(target_format))) {
                match = current;
            }
            continue;
        }
        if (current->refresh_rate != match->refresh_rate) {
            /* Sorted highest refresh to lowest */
            if (current->refresh_rate >= target_refresh_rate) {
                match = current;
            }
        }
    }
    if (match) {
        if (match->format) {
            closest->format = match->format;
        } else {
            closest->format = mode->format;
        }
        if (match->w && match->h) {
            closest->w = match->w;
            closest->h = match->h;
        } else {
            closest->w = mode->w;
            closest->h = mode->h;
        }
        if (match->refresh_rate) {
            closest->refresh_rate = match->refresh_rate;
        } else {
            closest->refresh_rate = mode->refresh_rate;
        }
        closest->driverdata = match->driverdata;

        /*
         * Pick some reasonable defaults if the app and driver don't
         * care
         */
        if (!closest->format) {
            closest->format = SDL_PIXELFORMAT_RGB888;
        }
        if (!closest->w) {
            closest->w = 640;
        }
        if (!closest->h) {
            closest->h = 480;
        }
        return closest;
    }
    return NULL;
}

int
SDL_SetDisplayMode(const SDL_DisplayMode * mode)
{
    SDL_VideoDisplay *display;
    SDL_DisplayMode display_mode;
    SDL_DisplayMode current_mode;
    int i, ncolors;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    display = &SDL_CurrentDisplay;
    if (!mode) {
        mode = &display->desktop_mode;
    }
    display_mode = *mode;

    /* Default to the current mode */
    if (!display_mode.format) {
        display_mode.format = display->current_mode.format;
    }
    if (!display_mode.w) {
        display_mode.w = display->current_mode.w;
    }
    if (!display_mode.h) {
        display_mode.h = display->current_mode.h;
    }
    if (!display_mode.refresh_rate) {
        display_mode.refresh_rate = display->current_mode.refresh_rate;
    }
    /* Get a good video mode, the closest one possible */
    if (!SDL_GetClosestDisplayMode(&display_mode, &display_mode)) {
        SDL_SetError("No video mode large enough for %dx%d",
                     display_mode.w, display_mode.h);
        return -1;
    }
    /* See if there's anything left to do */
    SDL_GetCurrentDisplayMode(&current_mode);
    if (SDL_memcmp(&display_mode, &current_mode, sizeof(display_mode)) == 0) {
        return 0;
    }
    /* Actually change the display mode */
    if (_this->SetDisplayMode(_this, &display_mode) < 0) {
        return -1;
    }
    display->current_mode = display_mode;

    /* Set up a palette, if necessary */
    if (SDL_ISPIXELFORMAT_INDEXED(display_mode.format)) {
        ncolors = (1 << SDL_BITSPERPIXEL(display_mode.format));
    } else {
        ncolors = 0;
    }
    if ((!ncolors && display->palette) || (ncolors && !display->palette)
        || (ncolors && ncolors != display->palette->ncolors)) {
        if (display->palette) {
            SDL_FreePalette(display->palette);
            display->palette = NULL;
        }
        if (ncolors) {
            display->palette = SDL_AllocPalette(ncolors);
            if (!display->palette) {
                return -1;
            }
            SDL_DitherColors(display->palette->colors,
                             SDL_BITSPERPIXEL(display_mode.format));
        }
    }
    /* Move any fullscreen windows into position */
    for (i = 0; i < display->num_windows; ++i) {
        SDL_Window *window = &display->windows[i];
        if (FULLSCREEN_VISIBLE(window)) {
            SDL_SetWindowPosition(window->id, window->x, window->y);
        }
    }

    return 0;
}

int
SDL_SetFullscreenDisplayMode(const SDL_DisplayMode * mode)
{
    SDL_VideoDisplay *display;
    SDL_DisplayMode fullscreen_mode;
    int i;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    display = &SDL_CurrentDisplay;
    if (!mode) {
        mode = &display->desktop_mode;
    }
    if (!SDL_GetClosestDisplayMode(mode, &fullscreen_mode)) {
        SDL_SetError("Couldn't find display mode match");
        return -1;
    }

    if (SDL_memcmp
        (&fullscreen_mode, &display->fullscreen_mode,
         sizeof(fullscreen_mode)) == 0) {
        /* Nothing to do... */
        return 0;
    }
    display->fullscreen_mode = fullscreen_mode;

    /* Actually set the mode if we have a fullscreen window visible */
    for (i = 0; i < display->num_windows; ++i) {
        SDL_Window *window = &display->windows[i];
        if (FULLSCREEN_VISIBLE(window)) {
            if (SDL_SetDisplayMode(&display->fullscreen_mode) < 0) {
                return -1;
            }
        }
        if (window->flags & SDL_WINDOW_FULLSCREEN) {
            SDL_OnWindowResized(window);
        }
    }
    return 0;
}

int
SDL_GetFullscreenDisplayMode(SDL_DisplayMode * mode)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (mode) {
        *mode = SDL_CurrentDisplay.fullscreen_mode;
    }
    return 0;
}

int
SDL_SetDisplayPalette(const SDL_Color * colors, int firstcolor, int ncolors)
{
    SDL_Palette *palette;
    int status = 0;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    palette = SDL_CurrentDisplay.palette;
    if (!palette) {
        SDL_SetError("Display mode does not have a palette");
        return -1;
    }
    status = SDL_SetPaletteColors(palette, colors, firstcolor, ncolors);

    if (_this->SetDisplayPalette) {
        if (_this->SetDisplayPalette(_this, palette) < 0) {
            status = -1;
        }
    }
    return status;
}

int
SDL_GetDisplayPalette(SDL_Color * colors, int firstcolor, int ncolors)
{
    SDL_Palette *palette;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    palette = SDL_CurrentDisplay.palette;
    if (!palette->ncolors) {
        SDL_SetError("Display mode does not have a palette");
        return -1;
    }
    if (firstcolor < 0 || (firstcolor + ncolors) > palette->ncolors) {
        SDL_SetError("Palette indices are out of range");
        return -1;
    }
    SDL_memcpy(colors, &palette->colors[firstcolor],
               ncolors * sizeof(*colors));
    return 0;
}

SDL_WindowID
SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    const Uint32 allowed_flags = (SDL_WINDOW_FULLSCREEN |
                                  SDL_WINDOW_OPENGL |
                                  SDL_WINDOW_BORDERLESS |
                                  SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_INPUT_GRABBED);
    SDL_VideoDisplay *display;
    SDL_Window window;
    int num_windows;
    SDL_Window *windows;

    if (!_this) {
        /* Initialize the video system if needed */
        if (SDL_VideoInit(NULL, 0) < 0) {
            return 0;
        }
    }
    if (flags & SDL_WINDOW_OPENGL) {
        if (!_this->GL_CreateContext) {
            SDL_SetError("No OpenGL support in video driver");
            return 0;
        }
        SDL_GL_LoadLibrary(NULL);
    }
    SDL_zero(window);
    window.id = _this->next_object_id++;
    window.x = x;
    window.y = y;
    window.w = w;
    window.h = h;
    window.flags = (flags & allowed_flags);
    window.display = _this->current_display;

    if (_this->CreateWindow && _this->CreateWindow(_this, &window) < 0) {
        if (flags & SDL_WINDOW_OPENGL) {
            SDL_GL_UnloadLibrary();
        }
        return 0;
    }
    display = &SDL_CurrentDisplay;
    num_windows = display->num_windows;
    windows =
        SDL_realloc(display->windows, (num_windows + 1) * sizeof(*windows));
    if (!windows) {
        if (_this->DestroyWindow) {
            _this->DestroyWindow(_this, &window);
        }
        if (flags & SDL_WINDOW_OPENGL) {
            SDL_GL_UnloadLibrary();
        }
        return 0;
    }
    windows[num_windows] = window;
    display->windows = windows;
    display->num_windows++;

    if (title) {
        SDL_SetWindowTitle(window.id, title);
    }
    if (flags & SDL_WINDOW_MAXIMIZED) {
        SDL_MaximizeWindow(window.id);
    }
    if (flags & SDL_WINDOW_MINIMIZED) {
        SDL_MinimizeWindow(window.id);
    }
    if (flags & SDL_WINDOW_SHOWN) {
        SDL_ShowWindow(window.id);
    }
    SDL_UpdateWindowGrab(&window);

    return window.id;
}

SDL_WindowID
SDL_CreateWindowFrom(const void *data)
{
    SDL_VideoDisplay *display;
    SDL_Window window;
    int num_windows;
    SDL_Window *windows;

    if (!_this) {
        SDL_UninitializedVideo();
        return (0);
    }
    SDL_zero(window);
    window.id = _this->next_object_id++;
    window.display = _this->current_display;
    window.flags = SDL_WINDOW_FOREIGN;

    if (!_this->CreateWindowFrom ||
        _this->CreateWindowFrom(_this, &window, data) < 0) {
        return 0;
    }
    display = &SDL_CurrentDisplay;
    num_windows = display->num_windows;
    windows =
        SDL_realloc(display->windows, (num_windows + 1) * sizeof(*windows));
    if (!windows) {
        if (_this->DestroyWindow) {
            _this->DestroyWindow(_this, &window);
        }
        if (window.title) {
            SDL_free(window.title);
        }
        return 0;
    }
    windows[num_windows] = window;
    display->windows = windows;
    display->num_windows++;

    return window.id;
}

int
SDL_RecreateWindow(SDL_Window * window, Uint32 flags)
{
    const Uint32 allowed_flags = (SDL_WINDOW_FULLSCREEN |
                                  SDL_WINDOW_OPENGL |
                                  SDL_WINDOW_BORDERLESS |
                                  SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_INPUT_GRABBED);
    char *title = window->title;

    if ((flags & SDL_WINDOW_OPENGL) && !_this->GL_CreateContext) {
        SDL_SetError("No OpenGL support in video driver");
        return -1;
    }
    if ((window->flags & SDL_WINDOW_OPENGL) != (flags & SDL_WINDOW_OPENGL)) {
        if (flags & SDL_WINDOW_OPENGL) {
            SDL_GL_LoadLibrary(NULL);
        } else {
            SDL_GL_UnloadLibrary();
        }
    }

    if (window->flags & SDL_WINDOW_FOREIGN) {
        /* Can't destroy and re-create foreign windows, hrm */
        flags |= SDL_WINDOW_FOREIGN;
    } else {
        flags &= ~SDL_WINDOW_FOREIGN;
    }

    if (_this->DestroyWindow && !(flags & SDL_WINDOW_FOREIGN)) {
        _this->DestroyWindow(_this, window);
    }

    window->title = NULL;
    window->flags = (flags & allowed_flags);

    if (_this->CreateWindow && !(flags & SDL_WINDOW_FOREIGN)) {
        if (_this->CreateWindow(_this, window) < 0) {
            if (flags & SDL_WINDOW_OPENGL) {
                SDL_GL_UnloadLibrary();
            }
            return -1;
        }
    }

    if (title) {
        SDL_SetWindowTitle(window->id, title);
        SDL_free(title);
    }
    if (flags & SDL_WINDOW_MAXIMIZED) {
        SDL_MaximizeWindow(window->id);
    }
    if (flags & SDL_WINDOW_MINIMIZED) {
        SDL_MinimizeWindow(window->id);
    }
    if (flags & SDL_WINDOW_SHOWN) {
        SDL_ShowWindow(window->id);
    }
    SDL_UpdateWindowGrab(window);

    return 0;
}

SDL_Window *
SDL_GetWindowFromID(SDL_WindowID windowID)
{
    int i, j;

    if (!_this) {
        SDL_UninitializedVideo();
        return NULL;
    }
    if (windowID) {
        for (i = 0; i < _this->num_displays; ++i) {
            SDL_VideoDisplay *display = &_this->displays[i];
            for (j = 0; j < display->num_windows; ++j) {
                SDL_Window *window = &display->windows[j];
                if (window->id == windowID) {
                    return window;
                }
            }
        }
    } else {
        /* Just return the first active window */
        for (i = 0; i < _this->num_displays; ++i) {
            SDL_VideoDisplay *display = &_this->displays[i];
            for (j = 0; j < display->num_windows; ++j) {
                SDL_Window *window = &display->windows[j];
                return window;
            }
        }
    }
    /* Couldn't find the window with the requested ID */
    SDL_SetError("Invalid window ID");
    return NULL;
}

SDL_VideoDisplay *
SDL_GetDisplayFromWindow(SDL_Window * window)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return NULL;
    }
    if (!window) {
        return NULL;
    }
    return &_this->displays[window->display];
}

static __inline__ SDL_Renderer *
SDL_GetCurrentRenderer()
{
    if (!_this) {
        SDL_UninitializedVideo();
        return NULL;
    }
    if (!SDL_CurrentRenderer) {
        if (SDL_CreateRenderer(0, -1, 0) < 0) {
            return NULL;
        }
    }
    return SDL_CurrentRenderer;
}

Uint32
SDL_GetWindowFlags(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return 0;
    }
    return window->flags;
}

void
SDL_SetWindowTitle(SDL_WindowID windowID, const char *title)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || title == window->title) {
        return;
    }
    if (window->title) {
        SDL_free(window->title);
    }
    if (title) {
        window->title = SDL_strdup(title);
    } else {
        window->title = NULL;
    }

    if (_this->SetWindowTitle) {
        _this->SetWindowTitle(_this, window);
    }
}

const char *
SDL_GetWindowTitle(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return NULL;
    }
    return window->title;
}

void
SDL_SetWindowIcon(SDL_WindowID windowID, SDL_Surface * icon)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return;
    }
    if (_this->SetWindowIcon) {
        _this->SetWindowIcon(_this, window, icon);
    }
}

void
SDL_SetWindowData(SDL_WindowID windowID, void *userdata)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return;
    }
    window->userdata = userdata;
}

void *
SDL_GetWindowData(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return NULL;
    }
    return window->userdata;
}

void
SDL_SetWindowPosition(SDL_WindowID windowID, int x, int y)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);

    if (!window) {
        return;
    }
    if (x != SDL_WINDOWPOS_UNDEFINED) {
        window->x = x;
    }
    if (y != SDL_WINDOWPOS_UNDEFINED) {
        window->y = y;
    }
    if (_this->SetWindowPosition) {
        _this->SetWindowPosition(_this, window);
    }
    SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_MOVED, x, y);
}

void
SDL_GetWindowPosition(SDL_WindowID windowID, int *x, int *y)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return;
    }
    if (x) {
        *x = window->x;
    }
    if (y) {
        *y = window->y;
    }
}

void
SDL_SetWindowSize(SDL_WindowID windowID, int w, int h)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return;
    }
    window->w = w;
    window->h = h;

    if (_this->SetWindowSize) {
        _this->SetWindowSize(_this, window);
    }
    SDL_OnWindowResized(window);
}

void
SDL_GetWindowSize(SDL_WindowID windowID, int *w, int *h)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (window) {
        if (w) {
            *w = window->w;
        }
        if (h) {
            *h = window->h;
        }
    } else {
        if (w) {
            *w = 0;
        }
        if (h) {
            *h = 0;
        }
    }
}

void
SDL_ShowWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || (window->flags & SDL_WINDOW_SHOWN)) {
        return;
    }

    if (_this->ShowWindow) {
        _this->ShowWindow(_this, window);
    }
    SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_SHOWN, 0, 0);
}

void
SDL_HideWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || !(window->flags & SDL_WINDOW_SHOWN)) {
        return;
    }

    if (_this->HideWindow) {
        _this->HideWindow(_this, window);
    }
    SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_HIDDEN, 0, 0);
}

void
SDL_RaiseWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || !(window->flags & SDL_WINDOW_SHOWN)) {
        return;
    }
    if (_this->RaiseWindow) {
        _this->RaiseWindow(_this, window);
    }
}

void
SDL_MaximizeWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || (window->flags & SDL_WINDOW_MAXIMIZED)) {
        return;
    }

    if (_this->MaximizeWindow) {
        _this->MaximizeWindow(_this, window);
    }
    SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_MAXIMIZED, 0, 0);
}

void
SDL_MinimizeWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || (window->flags & SDL_WINDOW_MINIMIZED)) {
        return;
    }

    if (_this->MinimizeWindow) {
        _this->MinimizeWindow(_this, window);
    }
    SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
}

void
SDL_RestoreWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window
        || !(window->flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) {
        return;
    }

    if (_this->RestoreWindow) {
        _this->RestoreWindow(_this, window);
    }
    SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_RESTORED, 0, 0);
}

int
SDL_SetWindowFullscreen(SDL_WindowID windowID, int fullscreen)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return -1;
    }
    if (fullscreen) {
        fullscreen = SDL_WINDOW_FULLSCREEN;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN) == fullscreen) {
        return 0;
    }
    if (fullscreen) {
        window->flags |= SDL_WINDOW_FULLSCREEN;

        if (FULLSCREEN_VISIBLE(window)) {
            SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);

            /* Hide any other fullscreen windows */
            int i;
            for (i = 0; i < display->num_windows; ++i) {
                SDL_Window *other = &display->windows[i];
                if (other->id != windowID && FULLSCREEN_VISIBLE(other)) {
                    SDL_MinimizeWindow(other->id);
                }
            }

            SDL_SetDisplayMode(&display->fullscreen_mode);
        }
    } else {
        window->flags &= ~SDL_WINDOW_FULLSCREEN;

        if (FULLSCREEN_VISIBLE(window)) {
            SDL_SetDisplayMode(NULL);
        }
    }
    return 0;
}

void
SDL_SetWindowGrab(SDL_WindowID windowID, int mode)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || (!!mode == !!(window->flags & SDL_WINDOW_INPUT_GRABBED))) {
        return;
    }
    if (mode) {
        window->flags |= SDL_WINDOW_INPUT_GRABBED;
    } else {
        window->flags &= ~SDL_WINDOW_INPUT_GRABBED;
    }
    SDL_UpdateWindowGrab(window);
}

static void
SDL_UpdateWindowGrab(SDL_Window * window)
{
    if ((window->flags & SDL_WINDOW_INPUT_FOCUS) && _this->SetWindowGrab) {
        _this->SetWindowGrab(_this, window);
    }
}

int
SDL_GetWindowGrab(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return 0;
    }
    return ((window->flags & SDL_WINDOW_INPUT_GRABBED) != 0);
}

void
SDL_OnWindowShown(SDL_Window * window)
{
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
    }
}

void
SDL_OnWindowHidden(SDL_Window * window)
{
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SendWindowEvent(window->id, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
    }
}

void
SDL_OnWindowResized(SDL_Window * window)
{
    SDL_Renderer *renderer = window->renderer;

    if (renderer && renderer->DisplayModeChanged) {
        renderer->DisplayModeChanged(renderer);
    }
}

void
SDL_OnWindowFocusGained(SDL_Window * window)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SetDisplayMode(&display->fullscreen_mode);
    }
    if (display->gamma && _this->SetDisplayGammaRamp) {
        _this->SetDisplayGammaRamp(_this, display->gamma);
    }
    if ((window->flags & (SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_FULLSCREEN))
        && _this->SetWindowGrab) {
        _this->SetWindowGrab(_this, window);
    }
}

void
SDL_OnWindowFocusLost(SDL_Window * window)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        SDL_MinimizeWindow(window->id);
        SDL_SetDisplayMode(NULL);
    }
    if (display->gamma && _this->SetDisplayGammaRamp) {
        _this->SetDisplayGammaRamp(_this, display->saved_gamma);
    }
    if ((window->flags & (SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_FULLSCREEN))
        && _this->SetWindowGrab) {
        _this->SetWindowGrab(_this, window);
    }
}

SDL_WindowID
SDL_GetFocusWindow(void)
{
    SDL_VideoDisplay *display;
    int i;

    if (!_this) {
        return 0;
    }
    display = &SDL_CurrentDisplay;
    for (i = 0; i < display->num_windows; ++i) {
        SDL_Window *window = &display->windows[i];

        if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
            return window->id;
        }
    }
    return 0;
}

void
SDL_DestroyWindow(SDL_WindowID windowID)
{
    int i, j;

    if (!_this) {
        return;
    }
    /* Restore video mode, etc. */
    SDL_SendWindowEvent(windowID, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);

    for (i = 0; i < _this->num_displays; ++i) {
        SDL_VideoDisplay *display = &_this->displays[i];
        for (j = 0; j < display->num_windows; ++j) {
            SDL_Window *window = &display->windows[j];
            if (window->id != windowID) {
                continue;
            }
            if (window->title) {
                SDL_free(window->title);
                window->title = NULL;
            }
            if (window->renderer) {
                SDL_DestroyRenderer(window->id);
                window->renderer = NULL;
            }
            if (_this->DestroyWindow) {
                _this->DestroyWindow(_this, window);
            }
            if (window->flags & SDL_WINDOW_OPENGL) {
                SDL_GL_UnloadLibrary();
            }
            if (j != display->num_windows - 1) {
                SDL_memcpy(&display->windows[i],
                           &display->windows[i + 1],
                           (display->num_windows - i - 1) * sizeof(*window));
            }
            --display->num_windows;
            return;
        }
    }
}

void
SDL_AddRenderDriver(int displayIndex, const SDL_RenderDriver * driver)
{
    SDL_VideoDisplay *display;
    SDL_RenderDriver *render_drivers;

    if (displayIndex >= _this->num_displays) {
        return;
    }
    display = &_this->displays[displayIndex];

    render_drivers =
        SDL_realloc(display->render_drivers,
                    (display->num_render_drivers +
                     1) * sizeof(*render_drivers));
    if (render_drivers) {
        render_drivers[display->num_render_drivers] = *driver;
        display->render_drivers = render_drivers;
        display->num_render_drivers++;
    }
}

int
SDL_GetNumRenderDrivers(void)
{
    if (_this) {
        return SDL_CurrentDisplay.num_render_drivers;
    }
    return 0;
}

int
SDL_GetRenderDriverInfo(int index, SDL_RendererInfo * info)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (index < 0 || index >= SDL_GetNumRenderDrivers()) {
        SDL_SetError("index must be in the range of 0 - %d",
                     SDL_GetNumRenderDrivers() - 1);
        return -1;
    }
    *info = SDL_CurrentDisplay.render_drivers[index].info;
    return 0;
}

int
SDL_CreateRenderer(SDL_WindowID windowID, int index, Uint32 flags)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        SDL_SetError("Invalid window ID");
        return -1;
    }

    /* Free any existing renderer */
    SDL_DestroyRenderer(windowID);

    if (index < 0) {
        const char *override = SDL_getenv("SDL_VIDEO_RENDERER");
        if (override) {
            int i, n = SDL_GetNumRenderDrivers();
            for (i = 0; i < n; ++i) {
                SDL_RenderDriver *driver =
                    &SDL_CurrentDisplay.render_drivers[i];
                if (SDL_strcasecmp(override, driver->info.name) == 0) {
                    index = i;
                    break;
                }
            }
        }
    }
    if (index < 0) {
        int n = SDL_GetNumRenderDrivers();
        for (index = 0; index < n; ++index) {
            SDL_RenderDriver *driver =
                &SDL_CurrentDisplay.render_drivers[index];

            if ((driver->info.flags & flags) == flags) {
                /* Create a new renderer instance */
                window->renderer = SDL_CurrentDisplay.render_drivers[index].CreateRenderer(window, flags);
                if (window->renderer) {
                    /* Yay, we got one! */
                    break;
                }
            }
        }
        if (index == n) {
            SDL_SetError("Couldn't find matching render driver");
            return -1;
        }
    } else {
        if (index >= SDL_GetNumRenderDrivers()) {
            SDL_SetError("index must be -1 or in the range of 0 - %d",
                         SDL_GetNumRenderDrivers() - 1);
            return -1;
        }

        /* Create a new renderer instance */
        window->renderer = SDL_CurrentDisplay.render_drivers[index].CreateRenderer(window, flags);
    }

    if (window->renderer == NULL) {
        /* Assuming renderer set its error */
        return -1;
    }

    SDL_SelectRenderer(window->id);

    return 0;
}

int
SDL_SelectRenderer(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);
    SDL_Renderer *renderer;

    if (!window) {
        SDL_SetError("Invalid window ID");
        return -1;
    }
    renderer = window->renderer;
    if (renderer) {
        if (renderer->ActivateRenderer) {
            if (renderer->ActivateRenderer(renderer) < 0) {
                return -1;
            }
        }
        SDL_CurrentDisplay.current_renderer = renderer;
    } else {
        if (SDL_CreateRenderer(windowID, -1, 0) < 0) {
            return -1;
        }
    }
    return 0;
}

int
SDL_GetRendererInfo(SDL_RendererInfo * info)
{
    SDL_Renderer *renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    *info = renderer->info;
    return 0;
}

SDL_TextureID
SDL_CreateTexture(Uint32 format, int access, int w, int h)
{
    int hash;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return 0;
    }
    if (!renderer->CreateTexture) {
        SDL_Unsupported();
        return 0;
    }
    texture = (SDL_Texture *) SDL_calloc(1, sizeof(*texture));
    if (!texture) {
        SDL_OutOfMemory();
        return 0;
    }
    texture->id = _this->next_object_id++;
    texture->format = format;
    texture->access = access;
    texture->w = w;
    texture->h = h;
    texture->r = 255;
    texture->g = 255;
    texture->b = 255;
    texture->a = 255;
    texture->renderer = renderer;

    if (renderer->CreateTexture(renderer, texture) < 0) {
        if (renderer->DestroyTexture) {
            renderer->DestroyTexture(renderer, texture);
        }
        SDL_free(texture);
        return 0;
    }
    hash = (texture->id % SDL_arraysize(SDL_CurrentDisplay.textures));
    texture->next = SDL_CurrentDisplay.textures[hash];
    SDL_CurrentDisplay.textures[hash] = texture;

    return texture->id;
}

SDL_TextureID
SDL_CreateTextureFromSurface(Uint32 format, SDL_Surface * surface)
{
    SDL_TextureID textureID;
    Uint32 requested_format = format;
    SDL_PixelFormat *fmt;
    SDL_Renderer *renderer;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (!surface) {
        SDL_SetError("SDL_CreateTextureFromSurface() passed NULL surface");
        return 0;
    }
    fmt = surface->format;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return 0;
    }

    if (format) {
        if (!SDL_PixelFormatEnumToMasks
            (format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
            SDL_SetError("Unknown pixel format");
            return 0;
        }
    } else {
        if (surface->format->Amask
            || !(surface->map->info.flags &
                 (SDL_COPY_COLORKEY | SDL_COPY_MASK | SDL_COPY_BLEND))) {
            Uint32 it;
            int pfmt;

            /* Pixel formats, sorted by best first */
            static const Uint32 sdl_pformats[] = {
                SDL_PIXELFORMAT_ARGB8888,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_PIXELFORMAT_ABGR8888,
                SDL_PIXELFORMAT_BGRA8888,
                SDL_PIXELFORMAT_RGB888,
                SDL_PIXELFORMAT_BGR888,
                SDL_PIXELFORMAT_RGB24,
                SDL_PIXELFORMAT_BGR24,
                SDL_PIXELFORMAT_RGB565,
                SDL_PIXELFORMAT_BGR565,
                SDL_PIXELFORMAT_ARGB1555,
                SDL_PIXELFORMAT_ABGR1555,
                SDL_PIXELFORMAT_RGB555,
                SDL_PIXELFORMAT_BGR555,
                SDL_PIXELFORMAT_ARGB4444,
                SDL_PIXELFORMAT_ABGR4444,
                SDL_PIXELFORMAT_RGB444,
                SDL_PIXELFORMAT_ARGB2101010,
                SDL_PIXELFORMAT_INDEX8,
                SDL_PIXELFORMAT_INDEX4LSB,
                SDL_PIXELFORMAT_INDEX4MSB,
                SDL_PIXELFORMAT_RGB332,
                SDL_PIXELFORMAT_INDEX1LSB,
                SDL_PIXELFORMAT_INDEX1MSB,
                SDL_PIXELFORMAT_UNKNOWN
            };

            bpp = fmt->BitsPerPixel;
            Rmask = fmt->Rmask;
            Gmask = fmt->Gmask;
            Bmask = fmt->Bmask;
            Amask = fmt->Amask;

            format =
                SDL_MasksToPixelFormatEnum(bpp, Rmask, Gmask, Bmask, Amask);
            if (!format) {
                SDL_SetError("Unknown pixel format");
                return 0;
            }

            /* Search requested format in the supported texture */
            /* formats by current renderer                      */
            for (it = 0; it < renderer->info.num_texture_formats; it++) {
                if (renderer->info.texture_formats[it] == format) {
                    break;
                }
            }

            /* If requested format can't be found, search any best */
            /* format which renderer provides                      */
            if (it == renderer->info.num_texture_formats) {
                pfmt = 0;
                for (;;) {
                    if (sdl_pformats[pfmt] == SDL_PIXELFORMAT_UNKNOWN) {
                        break;
                    }

                    for (it = 0; it < renderer->info.num_texture_formats;
                         it++) {
                        if (renderer->info.texture_formats[it] ==
                            sdl_pformats[pfmt]) {
                            break;
                        }
                    }

                    if (it != renderer->info.num_texture_formats) {
                        /* The best format has been found */
                        break;
                    }
                    pfmt++;
                }

                /* If any format can't be found, then return an error */
                if (it == renderer->info.num_texture_formats) {
                    SDL_SetError
                        ("Any of the supported pixel formats can't be found");
                    return 0;
                }

                /* Convert found pixel format back to color masks */
                if (SDL_PixelFormatEnumToMasks
                    (renderer->info.texture_formats[it], &bpp, &Rmask, &Gmask,
                     &Bmask, &Amask) != SDL_TRUE) {
                    SDL_SetError("Unknown pixel format");
                    return 0;
                }
            }
        } else {
            /* Need a format with alpha */
            Uint32 it;
            int apfmt;

            /* Pixel formats with alpha, sorted by best first */
            static const Uint32 sdl_alpha_pformats[] = {
                SDL_PIXELFORMAT_ARGB8888,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_PIXELFORMAT_ABGR8888,
                SDL_PIXELFORMAT_BGRA8888,
                SDL_PIXELFORMAT_ARGB1555,
                SDL_PIXELFORMAT_ABGR1555,
                SDL_PIXELFORMAT_ARGB4444,
                SDL_PIXELFORMAT_ABGR4444,
                SDL_PIXELFORMAT_ARGB2101010,
                SDL_PIXELFORMAT_UNKNOWN
            };

            if (surface->format->Amask) {
                /* If surface already has alpha, then try an original */
                /* surface format first                               */
                bpp = fmt->BitsPerPixel;
                Rmask = fmt->Rmask;
                Gmask = fmt->Gmask;
                Bmask = fmt->Bmask;
                Amask = fmt->Amask;
            } else {
                bpp = 32;
                Rmask = 0x00FF0000;
                Gmask = 0x0000FF00;
                Bmask = 0x000000FF;
                Amask = 0xFF000000;
            }

            format =
                SDL_MasksToPixelFormatEnum(bpp, Rmask, Gmask, Bmask, Amask);
            if (!format) {
                SDL_SetError("Unknown pixel format");
                return 0;
            }

            /* Search this format in the supported texture formats */
            /* by current renderer                                 */
            for (it = 0; it < renderer->info.num_texture_formats; it++) {
                if (renderer->info.texture_formats[it] == format) {
                    break;
                }
            }

            /* If this format can't be found, search any best       */
            /* compatible format with alpha which renderer provides */
            if (it == renderer->info.num_texture_formats) {
                apfmt = 0;
                for (;;) {
                    if (sdl_alpha_pformats[apfmt] == SDL_PIXELFORMAT_UNKNOWN) {
                        break;
                    }

                    for (it = 0; it < renderer->info.num_texture_formats;
                         it++) {
                        if (renderer->info.texture_formats[it] ==
                            sdl_alpha_pformats[apfmt]) {
                            break;
                        }
                    }

                    if (it != renderer->info.num_texture_formats) {
                        /* Compatible format has been found */
                        break;
                    }
                    apfmt++;
                }

                /* If compatible format can't be found, then return an error */
                if (it == renderer->info.num_texture_formats) {
                    SDL_SetError("Compatible pixel format can't be found");
                    return 0;
                }

                /* Convert found pixel format back to color masks */
                if (SDL_PixelFormatEnumToMasks
                    (renderer->info.texture_formats[it], &bpp, &Rmask, &Gmask,
                     &Bmask, &Amask) != SDL_TRUE) {
                    SDL_SetError("Unknown pixel format");
                    return 0;
                }
            }
        }

        format = SDL_MasksToPixelFormatEnum(bpp, Rmask, Gmask, Bmask, Amask);
        if (!format) {
            SDL_SetError("Unknown pixel format");
            return 0;
        }
    }

    textureID =
        SDL_CreateTexture(format, SDL_TEXTUREACCESS_STATIC, surface->w,
                          surface->h);
    if (!textureID && !requested_format) {
        SDL_DisplayMode desktop_mode;
        SDL_GetDesktopDisplayMode(&desktop_mode);
        format = desktop_mode.format;
        textureID =
            SDL_CreateTexture(format, SDL_TEXTUREACCESS_STATIC, surface->w,
                              surface->h);
    }
    if (!textureID) {
        return 0;
    }
    if (bpp == fmt->BitsPerPixel && Rmask == fmt->Rmask && Gmask == fmt->Gmask
        && Bmask == fmt->Bmask && Amask == fmt->Amask) {
        if (SDL_MUSTLOCK(surface)) {
            SDL_LockSurface(surface);
            SDL_UpdateTexture(textureID, NULL, surface->pixels,
                              surface->pitch);
            SDL_UnlockSurface(surface);
        } else {
            SDL_UpdateTexture(textureID, NULL, surface->pixels,
                              surface->pitch);
        }
    } else {
        SDL_PixelFormat dst_fmt;
        SDL_Surface *dst = NULL;

        /* Set up a destination surface for the texture update */
        SDL_InitFormat(&dst_fmt, bpp, Rmask, Gmask, Bmask, Amask);
        if (SDL_ISPIXELFORMAT_INDEXED(format)) {
            dst_fmt.palette =
                SDL_AllocPalette((1 << SDL_BITSPERPIXEL(format)));
            if (dst_fmt.palette) {
                /*
                 * FIXME: Should we try to copy
                 * fmt->palette?
                 */
                SDL_DitherColors(dst_fmt.palette->colors,
                                 SDL_BITSPERPIXEL(format));
            }
        }
        dst = SDL_ConvertSurface(surface, &dst_fmt, 0);
        if (dst) {
            SDL_UpdateTexture(textureID, NULL, dst->pixels, dst->pitch);
            SDL_FreeSurface(dst);
        }
        if (dst_fmt.palette) {
            SDL_FreePalette(dst_fmt.palette);
        }
        if (!dst) {
            SDL_DestroyTexture(textureID);
            return 0;
        }
    }

    {
        Uint8 r, g, b, a;
        int blendMode;
        int scaleMode;

        SDL_GetSurfaceColorMod(surface, &r, &g, &b);
        SDL_SetTextureColorMod(textureID, r, g, b);

        SDL_GetSurfaceAlphaMod(surface, &a);
        SDL_SetTextureAlphaMod(textureID, a);

        SDL_GetSurfaceBlendMode(surface, &blendMode);
        SDL_SetTextureBlendMode(textureID, blendMode);

        SDL_GetSurfaceScaleMode(surface, &scaleMode);
        SDL_SetTextureScaleMode(textureID, scaleMode);
    }

    if (SDL_ISPIXELFORMAT_INDEXED(format) && fmt->palette) {
        SDL_SetTexturePalette(textureID, fmt->palette->colors, 0,
                              fmt->palette->ncolors);
    }
    return textureID;
}

static __inline__ SDL_Texture *
SDL_GetTextureFromID(SDL_TextureID textureID)
{
    int hash;
    SDL_Texture *texture;

    if (!_this) {
        return NULL;
    }
    hash = (textureID % SDL_arraysize(SDL_CurrentDisplay.textures));
    for (texture = SDL_CurrentDisplay.textures[hash]; texture;
         texture = texture->next) {
        if (texture->id == textureID) {
            return texture;
        }
    }
    return NULL;
}

int
SDL_QueryTexture(SDL_TextureID textureID, Uint32 * format, int *access,
                 int *w, int *h)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);

    if (!texture) {
        return -1;
    }
    if (format) {
        *format = texture->format;
    }
    if (access) {
        *access = texture->access;
    }
    if (w) {
        *w = texture->w;
    }
    if (h) {
        *h = texture->h;
    }
    return 0;
}

int
SDL_QueryTexturePixels(SDL_TextureID textureID, void **pixels, int *pitch)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->QueryTexturePixels) {
        SDL_Unsupported();
        return -1;
    }
    return renderer->QueryTexturePixels(renderer, texture, pixels, pitch);
}

int
SDL_SetTexturePalette(SDL_TextureID textureID, const SDL_Color * colors,
                      int firstcolor, int ncolors)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->SetTexturePalette) {
        SDL_Unsupported();
        return -1;
    }
    return renderer->SetTexturePalette(renderer, texture, colors, firstcolor,
                                       ncolors);
}

int
SDL_GetTexturePalette(SDL_TextureID textureID, SDL_Color * colors,
                      int firstcolor, int ncolors)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->GetTexturePalette) {
        SDL_Unsupported();
        return -1;
    }
    return renderer->GetTexturePalette(renderer, texture, colors, firstcolor,
                                       ncolors);
}

int
SDL_SetTextureColorMod(SDL_TextureID textureID, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->SetTextureColorMod) {
        SDL_Unsupported();
        return -1;
    }
    if (r < 255 || g < 255 || b < 255) {
        texture->modMode |= SDL_TEXTUREMODULATE_COLOR;
    } else {
        texture->modMode &= ~SDL_TEXTUREMODULATE_COLOR;
    }
    texture->r = r;
    texture->g = g;
    texture->b = b;
    return renderer->SetTextureColorMod(renderer, texture);
}

int
SDL_GetTextureColorMod(SDL_TextureID textureID, Uint8 * r, Uint8 * g,
                       Uint8 * b)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (r) {
        *r = texture->r;
    }
    if (g) {
        *g = texture->g;
    }
    if (b) {
        *b = texture->b;
    }
    return 0;
}

int
SDL_SetTextureAlphaMod(SDL_TextureID textureID, Uint8 alpha)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->SetTextureAlphaMod) {
        SDL_Unsupported();
        return -1;
    }
    if (alpha < 255) {
        texture->modMode |= SDL_TEXTUREMODULATE_ALPHA;
    } else {
        texture->modMode &= ~SDL_TEXTUREMODULATE_ALPHA;
    }
    texture->a = alpha;
    return renderer->SetTextureAlphaMod(renderer, texture);
}

int
SDL_GetTextureAlphaMod(SDL_TextureID textureID, Uint8 * alpha)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);

    if (!texture) {
        return -1;
    }
    if (alpha) {
        *alpha = texture->a;
    }
    return 0;
}

int
SDL_SetTextureBlendMode(SDL_TextureID textureID, int blendMode)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->SetTextureBlendMode) {
        SDL_Unsupported();
        return -1;
    }
    texture->blendMode = blendMode;
    return renderer->SetTextureBlendMode(renderer, texture);
}

int
SDL_GetTextureBlendMode(SDL_TextureID textureID, int *blendMode)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);

    if (!texture) {
        return -1;
    }
    if (blendMode) {
        *blendMode = texture->blendMode;
    }
    return 0;
}

int
SDL_SetTextureScaleMode(SDL_TextureID textureID, int scaleMode)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->SetTextureScaleMode) {
        SDL_Unsupported();
        return -1;
    }
    texture->scaleMode = scaleMode;
    return renderer->SetTextureScaleMode(renderer, texture);
}

int
SDL_GetTextureScaleMode(SDL_TextureID textureID, int *scaleMode)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);

    if (!texture) {
        return -1;
    }
    if (scaleMode) {
        *scaleMode = texture->scaleMode;
    }
    return 0;
}

int
SDL_UpdateTexture(SDL_TextureID textureID, const SDL_Rect * rect,
                  const void *pixels, int pitch)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;
    SDL_Rect full_rect;

    if (!texture) {
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->UpdateTexture) {
        SDL_Unsupported();
        return -1;
    }
    if (!rect) {
        full_rect.x = 0;
        full_rect.y = 0;
        full_rect.w = texture->w;
        full_rect.h = texture->h;
        rect = &full_rect;
    }
    return renderer->UpdateTexture(renderer, texture, rect, pixels, pitch);
}

int
SDL_LockTexture(SDL_TextureID textureID, const SDL_Rect * rect, int markDirty,
                void **pixels, int *pitch)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;
    SDL_Rect full_rect;

    if (!texture) {
        return -1;
    }
    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        SDL_SetError("SDL_LockTexture(): texture must be streaming");
        return -1;
    }
    renderer = texture->renderer;
    if (!renderer->LockTexture) {
        SDL_Unsupported();
        return -1;
    }
    if (!rect) {
        full_rect.x = 0;
        full_rect.y = 0;
        full_rect.w = texture->w;
        full_rect.h = texture->h;
        rect = &full_rect;
    }
    return renderer->LockTexture(renderer, texture, rect, markDirty, pixels,
                                 pitch);
}

void
SDL_UnlockTexture(SDL_TextureID textureID)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return;
    }
    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        return;
    }
    renderer = texture->renderer;
    if (!renderer->UnlockTexture) {
        return;
    }
    renderer->UnlockTexture(renderer, texture);
}

void
SDL_DirtyTexture(SDL_TextureID textureID, int numrects,
                 const SDL_Rect * rects)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;

    if (!texture) {
        return;
    }
    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        return;
    }
    renderer = texture->renderer;
    if (!renderer->DirtyTexture) {
        return;
    }
    renderer->DirtyTexture(renderer, texture, numrects, rects);
}

int
SDL_SetRenderDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Renderer *renderer;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    renderer->r = r;
    renderer->g = g;
    renderer->b = b;
    renderer->a = a;
    if (renderer->SetDrawColor) {
        return renderer->SetDrawColor(renderer);
    } else {
        return 0;
    }
}

int
SDL_GetRenderDrawColor(Uint8 * r, Uint8 * g, Uint8 * b, Uint8 * a)
{
    SDL_Renderer *renderer;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (r) {
        *r = renderer->r;
    }
    if (g) {
        *g = renderer->g;
    }
    if (b) {
        *b = renderer->b;
    }
    if (a) {
        *a = renderer->a;
    }
    return 0;
}

int
SDL_SetRenderDrawBlendMode(int blendMode)
{
    SDL_Renderer *renderer;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    renderer->blendMode = blendMode;
    if (renderer->SetDrawBlendMode) {
        return renderer->SetDrawBlendMode(renderer);
    } else {
        return 0;
    }
}

int
SDL_GetRenderDrawBlendMode(int *blendMode)
{
    SDL_Renderer *renderer;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    *blendMode = renderer->blendMode;
    return 0;
}

int
SDL_RenderPoint(int x, int y)
{
    SDL_Renderer *renderer;
    SDL_Window *window;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (!renderer->RenderPoint) {
        SDL_Unsupported();
        return -1;
    }
    window = SDL_GetWindowFromID(renderer->window);
    if (x < 0 || y < 0 || x >= window->w || y >= window->h) {
        return 0;
    }
    return renderer->RenderPoint(renderer, x, y);
}

int
SDL_RenderLine(int x1, int y1, int x2, int y2)
{
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Rect real_rect;

    if (x1 == x2 && y1 == y2) {
        return SDL_RenderPoint(x1, y1);
    }

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (!renderer->RenderLine) {
        SDL_Unsupported();
        return -1;
    }
    window = SDL_GetWindowFromID(renderer->window);

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = window->w;
    real_rect.h = window->h;
    if (!SDL_IntersectRectAndLine(&real_rect, &x1, &y1, &x2, &y2)) {
        return (0);
    }
    return renderer->RenderLine(renderer, x1, y1, x2, y2);
}

int
SDL_RenderFill(const SDL_Rect * rect)
{
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Rect real_rect;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (!renderer->RenderFill) {
        SDL_Unsupported();
        return -1;
    }
    window = SDL_GetWindowFromID(renderer->window);

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = window->w;
    real_rect.h = window->h;
    if (rect) {
        if (!SDL_IntersectRect(rect, &real_rect, &real_rect)) {
            return 0;
        }
    }
    return renderer->RenderFill(renderer, &real_rect);
}

int
SDL_RenderCopy(SDL_TextureID textureID, const SDL_Rect * srcrect,
               const SDL_Rect * dstrect)
{
    SDL_Texture *texture = SDL_GetTextureFromID(textureID);
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Rect real_srcrect;
    SDL_Rect real_dstrect;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (!texture) {
        SDL_SetError("Texture not found");
        return -1;
    }
    if (texture->renderer != renderer) {
        SDL_SetError("Texture was not created with this renderer");
        return -1;
    }
    if (!renderer->RenderCopy) {
        SDL_Unsupported();
        return -1;
    }
    window = SDL_GetWindowFromID(renderer->window);

    real_srcrect.x = 0;
    real_srcrect.y = 0;
    real_srcrect.w = texture->w;
    real_srcrect.h = texture->h;
    if (srcrect) {
        if (!SDL_IntersectRect(srcrect, &real_srcrect, &real_srcrect)) {
            return 0;
        }
    }

    real_dstrect.x = 0;
    real_dstrect.y = 0;
    real_dstrect.w = window->w;
    real_dstrect.h = window->h;
    if (dstrect) {
        if (!SDL_IntersectRect(dstrect, &real_dstrect, &real_dstrect)) {
            return 0;
        }
        /* Clip srcrect by the same amount as dstrect was clipped */
        if (dstrect->w != real_dstrect.w) {
            int deltax = (real_dstrect.x - dstrect->x);
            int deltaw = (real_dstrect.w - dstrect->w);
            real_srcrect.x += (deltax * real_srcrect.w) / dstrect->w;
            real_srcrect.w += (deltaw * real_srcrect.w) / dstrect->w;
        }
        if (dstrect->h != real_dstrect.h) {
            int deltay = (real_dstrect.y - dstrect->y);
            int deltah = (real_dstrect.h - dstrect->h);
            real_srcrect.y += (deltay * real_srcrect.h) / dstrect->h;
            real_srcrect.h += (deltah * real_srcrect.h) / dstrect->h;
        }
    }

    return renderer->RenderCopy(renderer, texture, &real_srcrect,
                                &real_dstrect);
}

int
SDL_RenderReadPixels(const SDL_Rect * rect, void * pixels, int pitch)
{
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Rect real_rect;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (!renderer->RenderReadPixels) {
        SDL_Unsupported();
        return -1;
    }
    window = SDL_GetWindowFromID(renderer->window);

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = window->w;
    real_rect.h = window->h;
    if (rect) {
        if (!SDL_IntersectRect(rect, &real_rect, &real_rect)) {
            return 0;
        }
        if (real_rect.y > rect->y) {
            pixels = (Uint8 *)pixels + pitch * (real_rect.y - rect->y);
        }
        if (real_rect.x > rect->x) {
            Uint32 format = SDL_CurrentDisplay.current_mode.format;
            int bpp = SDL_BYTESPERPIXEL(format);
            pixels = (Uint8 *)pixels + bpp * (real_rect.x - rect->x);
        }
    }

    return renderer->RenderReadPixels(renderer, &real_rect, pixels, pitch);
}

int
SDL_RenderWritePixels(const SDL_Rect * rect, const void * pixels, int pitch)
{
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Rect real_rect;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer) {
        return -1;
    }
    if (!renderer->RenderWritePixels) {
        SDL_Unsupported();
        return -1;
    }
    window = SDL_GetWindowFromID(renderer->window);

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = window->w;
    real_rect.h = window->h;
    if (rect) {
        if (!SDL_IntersectRect(rect, &real_rect, &real_rect)) {
            return 0;
        }
        if (real_rect.y > rect->y) {
            pixels = (const Uint8 *)pixels + pitch * (real_rect.y - rect->y);
        }
        if (real_rect.x > rect->x) {
            Uint32 format = SDL_CurrentDisplay.current_mode.format;
            int bpp = SDL_BYTESPERPIXEL(format);
            pixels = (const Uint8 *)pixels + bpp * (real_rect.x - rect->x);
        }
    }

    return renderer->RenderWritePixels(renderer, &real_rect, pixels, pitch);
}

void
SDL_RenderPresent(void)
{
    SDL_Renderer *renderer;

    renderer = SDL_GetCurrentRenderer();
    if (!renderer || !renderer->RenderPresent) {
        return;
    }
    renderer->RenderPresent(renderer);
}

void
SDL_DestroyTexture(SDL_TextureID textureID)
{
    int hash;
    SDL_Texture *prev, *texture;
    SDL_Renderer *renderer;

    if (!_this) {
        SDL_UninitializedVideo();
        return;
    }
    /* Look up the texture in the hash table */
    hash = (textureID % SDL_arraysize(SDL_CurrentDisplay.textures));
    prev = NULL;
    for (texture = SDL_CurrentDisplay.textures[hash]; texture;
         prev = texture, texture = texture->next) {
        if (texture->id == textureID) {
            break;
        }
    }
    if (!texture) {
        return;
    }
    /* Unlink the texture from the list */
    if (prev) {
        prev->next = texture->next;
    } else {
        SDL_CurrentDisplay.textures[hash] = texture->next;
    }

    /* Free the texture */
    renderer = texture->renderer;
    renderer->DestroyTexture(renderer, texture);
    SDL_free(texture);
}

void
SDL_DestroyRenderer(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);
    SDL_Renderer *renderer;
    int i;

    if (!window) {
        return;
    }
    renderer = window->renderer;
    if (!renderer) {
        return;
    }
    /* Free existing textures for this renderer */
    for (i = 0; i < SDL_arraysize(SDL_CurrentDisplay.textures); ++i) {
        SDL_Texture *texture;
        SDL_Texture *prev = NULL;
        SDL_Texture *next;
        for (texture = SDL_CurrentDisplay.textures[i]; texture;
             texture = next) {
            next = texture->next;
            if (texture->renderer == renderer) {
                if (prev) {
                    prev->next = next;
                } else {
                    SDL_CurrentDisplay.textures[i] = next;
                }
                renderer->DestroyTexture(renderer, texture);
                SDL_free(texture);
            } else {
                prev = texture;
            }
        }
    }

    /* Free the renderer instance */
    renderer->DestroyRenderer(renderer);

    /* Clear references */
    window->renderer = NULL;
    if (SDL_CurrentDisplay.current_renderer == renderer) {
        SDL_CurrentDisplay.current_renderer = NULL;
    }
}

SDL_bool
SDL_IsScreenSaverEnabled()
{
    if (!_this) {
        return SDL_TRUE;
    }
    return _this->suspend_screensaver ? SDL_FALSE : SDL_TRUE;
}

void
SDL_EnableScreenSaver()
{
    if (!_this) {
        return;
    }
    if (!_this->suspend_screensaver) {
        return;
    }
    _this->suspend_screensaver = SDL_FALSE;
    if (_this->SuspendScreenSaver) {
        _this->SuspendScreenSaver(_this);
    }
}

void
SDL_DisableScreenSaver()
{
    if (!_this) {
        return;
    }
    if (_this->suspend_screensaver) {
        return;
    }
    _this->suspend_screensaver = SDL_TRUE;
    if (_this->SuspendScreenSaver) {
        _this->SuspendScreenSaver(_this);
    }
}

void
SDL_VideoQuit(void)
{
    int i, j;

    if (!_this) {
        return;
    }
    /* Halt event processing before doing anything else */
    SDL_StopEventLoop();
    SDL_EnableScreenSaver();

    /* Clean up the system video */
    for (i = _this->num_displays; i--;) {
        SDL_VideoDisplay *display = &_this->displays[i];
        for (j = display->num_windows; j--;) {
            SDL_DestroyWindow(display->windows[i].id);
        }
        if (display->windows) {
            SDL_free(display->windows);
            display->windows = NULL;
        }
        display->num_windows = 0;
        if (display->render_drivers) {
            SDL_free(display->render_drivers);
            display->render_drivers = NULL;
        }
        display->num_render_drivers = 0;
    }
    _this->VideoQuit(_this);

    for (i = _this->num_displays; i--;) {
        SDL_VideoDisplay *display = &_this->displays[i];
        for (j = display->num_display_modes; j--;) {
            if (display->display_modes[j].driverdata) {
                SDL_free(display->display_modes[j].driverdata);
                display->display_modes[j].driverdata = NULL;
            }
        }
        if (display->display_modes) {
            SDL_free(display->display_modes);
            display->display_modes = NULL;
        }
        if (display->desktop_mode.driverdata) {
            SDL_free(display->desktop_mode.driverdata);
            display->desktop_mode.driverdata = NULL;
        }
        if (display->palette) {
            SDL_FreePalette(display->palette);
            display->palette = NULL;
        }
        if (display->gamma) {
            SDL_free(display->gamma);
            display->gamma = NULL;
        }
        if (display->driverdata) {
            SDL_free(display->driverdata);
            display->driverdata = NULL;
        }
    }
    if (_this->displays) {
        SDL_free(_this->displays);
        _this->displays = NULL;
    }
    _this->free(_this);
    _this = NULL;
}

int
SDL_GL_LoadLibrary(const char *path)
{
    int retval;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (_this->gl_config.driver_loaded) {
        if (path && SDL_strcmp(path, _this->gl_config.driver_path) != 0) {
            SDL_SetError("OpenGL library already loaded");
            return -1;
        }
        retval = 0;
    } else {
        if (!_this->GL_LoadLibrary) {
            SDL_SetError("No dynamic GL support in video driver");
            return -1;
        }
        retval = _this->GL_LoadLibrary(_this, path);
    }
    if (retval == 0) {
        ++_this->gl_config.driver_loaded;
    }
    return (retval);
}

void *
SDL_GL_GetProcAddress(const char *proc)
{
    void *func;

    if (!_this) {
        SDL_UninitializedVideo();
        return NULL;
    }
    func = NULL;
    if (_this->GL_GetProcAddress) {
        if (_this->gl_config.driver_loaded) {
            func = _this->GL_GetProcAddress(_this, proc);
        } else {
            SDL_SetError("No GL driver has been loaded");
        }
    } else {
        SDL_SetError("No dynamic GL support in video driver");
    }
    return func;
}

void
SDL_GL_UnloadLibrary(void)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return;
    }
    if (_this->gl_config.driver_loaded > 0) {
        if (--_this->gl_config.driver_loaded > 0) {
            return;
        }
        if (_this->GL_UnloadLibrary) {
            _this->GL_UnloadLibrary(_this);
        }
    }
}

SDL_bool
SDL_GL_ExtensionSupported(const char *extension)
{
#if SDL_VIDEO_OPENGL || SDL_VIDEO_OPENGL_ES
    const GLubyte *(APIENTRY * glGetStringFunc) (GLenum);
    const char *extensions;
    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = SDL_strchr(extension, ' ');
    if (where || *extension == '\0') {
        return SDL_FALSE;
    }
    /* See if there's an environment variable override */
    start = SDL_getenv(extension);
    if (start && *start == '0') {
        return SDL_FALSE;
    }
    /* Lookup the available extensions */
    glGetStringFunc = SDL_GL_GetProcAddress("glGetString");
    if (glGetStringFunc) {
        extensions = (const char *) glGetStringFunc(GL_EXTENSIONS);
    } else {
        extensions = NULL;
    }
    if (!extensions) {
        return SDL_FALSE;
    }
    /*
     * It takes a bit of care to be fool-proof about parsing the OpenGL
     * extensions string. Don't be fooled by sub-strings, etc.
     */

    start = extensions;

    for (;;) {
        where = SDL_strstr(start, extension);
        if (!where)
            break;

        terminator = where + SDL_strlen(extension);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return SDL_TRUE;

        start = terminator;
    }
    return SDL_FALSE;
#else
    return SDL_FALSE;
#endif
}

int
SDL_GL_SetAttribute(SDL_GLattr attr, int value)
{
#if SDL_VIDEO_OPENGL || SDL_VIDEO_OPENGL_ES
    int retval;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    retval = 0;
    switch (attr) {
    case SDL_GL_RED_SIZE:
        _this->gl_config.red_size = value;
        break;
    case SDL_GL_GREEN_SIZE:
        _this->gl_config.green_size = value;
        break;
    case SDL_GL_BLUE_SIZE:
        _this->gl_config.blue_size = value;
        break;
    case SDL_GL_ALPHA_SIZE:
        _this->gl_config.alpha_size = value;
        break;
    case SDL_GL_DOUBLEBUFFER:
        _this->gl_config.double_buffer = value;
        break;
    case SDL_GL_BUFFER_SIZE:
        _this->gl_config.buffer_size = value;
        break;
    case SDL_GL_DEPTH_SIZE:
        _this->gl_config.depth_size = value;
        break;
    case SDL_GL_STENCIL_SIZE:
        _this->gl_config.stencil_size = value;
        break;
    case SDL_GL_ACCUM_RED_SIZE:
        _this->gl_config.accum_red_size = value;
        break;
    case SDL_GL_ACCUM_GREEN_SIZE:
        _this->gl_config.accum_green_size = value;
        break;
    case SDL_GL_ACCUM_BLUE_SIZE:
        _this->gl_config.accum_blue_size = value;
        break;
    case SDL_GL_ACCUM_ALPHA_SIZE:
        _this->gl_config.accum_alpha_size = value;
        break;
    case SDL_GL_STEREO:
        _this->gl_config.stereo = value;
        break;
    case SDL_GL_MULTISAMPLEBUFFERS:
        _this->gl_config.multisamplebuffers = value;
        break;
    case SDL_GL_MULTISAMPLESAMPLES:
        _this->gl_config.multisamplesamples = value;
        break;
    case SDL_GL_ACCELERATED_VISUAL:
        _this->gl_config.accelerated = value;
        break;
    case SDL_GL_RETAINED_BACKING:
        _this->gl_config.retained_backing = value;
        break;
    case SDL_GL_CONTEXT_MAJOR_VERSION:
        _this->gl_config.major_version = value;
        break;
    case SDL_GL_CONTEXT_MINOR_VERSION:
        _this->gl_config.minor_version = value;
        break;
    default:
        SDL_SetError("Unknown OpenGL attribute");
        retval = -1;
        break;
    }
    return retval;
#else
    SDL_Unsupported();
    return -1;
#endif /* SDL_VIDEO_OPENGL */
}

int
SDL_GL_GetAttribute(SDL_GLattr attr, int *value)
{
#if SDL_VIDEO_OPENGL || SDL_VIDEO_OPENGL_ES
    void (APIENTRY * glGetIntegervFunc) (GLenum pname, GLint * params);
    GLenum(APIENTRY * glGetErrorFunc) (void);
    GLenum attrib = 0;
    GLenum error = 0;

    glGetIntegervFunc = SDL_GL_GetProcAddress("glGetIntegerv");
    if (!glGetIntegervFunc) {
        return -1;
    }

    glGetErrorFunc = SDL_GL_GetProcAddress("glGetError");
    if (!glGetErrorFunc) {
        return -1;
    }

    /* Clear value in any case */
    *value = 0;

    switch (attr) {
    case SDL_GL_RETAINED_BACKING:
        *value = _this->gl_config.retained_backing;
        return 0;
    case SDL_GL_RED_SIZE:
        attrib = GL_RED_BITS;
        break;
    case SDL_GL_BLUE_SIZE:
        attrib = GL_BLUE_BITS;
        break;
    case SDL_GL_GREEN_SIZE:
        attrib = GL_GREEN_BITS;
        break;
    case SDL_GL_ALPHA_SIZE:
        attrib = GL_ALPHA_BITS;
        break;
    case SDL_GL_DOUBLEBUFFER:
#ifndef SDL_VIDEO_OPENGL_ES
        attrib = GL_DOUBLEBUFFER;
        break;
#else
        /* OpenGL ES 1.0 and above specifications have EGL_SINGLE_BUFFER      */
        /* parameter which switches double buffer to single buffer. OpenGL ES */
        /* SDL driver must set proper value after initialization              */
        *value = _this->gl_config.double_buffer;
        return 0;
#endif
    case SDL_GL_DEPTH_SIZE:
        attrib = GL_DEPTH_BITS;
        break;
    case SDL_GL_STENCIL_SIZE:
        attrib = GL_STENCIL_BITS;
        break;
#ifndef SDL_VIDEO_OPENGL_ES
    case SDL_GL_ACCUM_RED_SIZE:
        attrib = GL_ACCUM_RED_BITS;
        break;
    case SDL_GL_ACCUM_GREEN_SIZE:
        attrib = GL_ACCUM_GREEN_BITS;
        break;
    case SDL_GL_ACCUM_BLUE_SIZE:
        attrib = GL_ACCUM_BLUE_BITS;
        break;
    case SDL_GL_ACCUM_ALPHA_SIZE:
        attrib = GL_ACCUM_ALPHA_BITS;
        break;
    case SDL_GL_STEREO:
        attrib = GL_STEREO;
        break;
#else
    case SDL_GL_ACCUM_RED_SIZE:
    case SDL_GL_ACCUM_GREEN_SIZE:
    case SDL_GL_ACCUM_BLUE_SIZE:
    case SDL_GL_ACCUM_ALPHA_SIZE:
    case SDL_GL_STEREO:
        /* none of these are supported in OpenGL ES */
        *value = 0;
        return 0;
#endif
    case SDL_GL_MULTISAMPLEBUFFERS:
#ifndef SDL_VIDEO_OPENGL_ES
        attrib = GL_SAMPLE_BUFFERS_ARB;
#else
        attrib = GL_SAMPLE_BUFFERS;
#endif
        break;
    case SDL_GL_MULTISAMPLESAMPLES:
#ifndef SDL_VIDEO_OPENGL_ES
        attrib = GL_SAMPLES_ARB;
#else
        attrib = GL_SAMPLES;
#endif
        break;
    case SDL_GL_BUFFER_SIZE:
        {
            GLint bits = 0;
            GLint component;

            /*
             * there doesn't seem to be a single flag in OpenGL
             * for this!
             */
            glGetIntegervFunc(GL_RED_BITS, &component);
            bits += component;
            glGetIntegervFunc(GL_GREEN_BITS, &component);
            bits += component;
            glGetIntegervFunc(GL_BLUE_BITS, &component);
            bits += component;
            glGetIntegervFunc(GL_ALPHA_BITS, &component);
            bits += component;

            *value = bits;
            return 0;
        }
    case SDL_GL_ACCELERATED_VISUAL:
        {
            /* FIXME: How do we get this information? */
            *value = (_this->gl_config.accelerated != 0);
            return 0;
        }
    default:
        SDL_SetError("Unknown OpenGL attribute");
        return -1;
    }

    glGetIntegervFunc(attrib, (GLint *) value);
    error = glGetErrorFunc();
    if (error != GL_NO_ERROR) {
        switch (error) {
        case GL_INVALID_ENUM:
            {
                SDL_SetError("OpenGL error: GL_INVALID_ENUM");
            }
            break;
        case GL_INVALID_VALUE:
            {
                SDL_SetError("OpenGL error: GL_INVALID_VALUE");
            }
            break;
        default:
            {
                SDL_SetError("OpenGL error: %08X", error);
            }
            break;
        }
        return -1;
    }
    return 0;
#else
    SDL_Unsupported();
    return -1;
#endif /* SDL_VIDEO_OPENGL */
}

SDL_GLContext
SDL_GL_CreateContext(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return NULL;
    }
    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_SetError("The specified window isn't an OpenGL window");
        return NULL;
    }
    return _this->GL_CreateContext(_this, window);
}

int
SDL_GL_MakeCurrent(SDL_WindowID windowID, SDL_GLContext context)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (window && !(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_SetError("The specified window isn't an OpenGL window");
        return -1;
    }
    if (!context) {
        window = NULL;
    }
    return _this->GL_MakeCurrent(_this, window, context);
}

int
SDL_GL_SetSwapInterval(int interval)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (_this->GL_SetSwapInterval) {
        return _this->GL_SetSwapInterval(_this, interval);
    } else {
        SDL_SetError("Setting the swap interval is not supported");
        return -1;
    }
}

int
SDL_GL_GetSwapInterval(void)
{
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    if (_this->GL_GetSwapInterval) {
        return _this->GL_GetSwapInterval(_this);
    } else {
        SDL_SetError("Getting the swap interval is not supported");
        return -1;
    }
}

void
SDL_GL_SwapWindow(SDL_WindowID windowID)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window) {
        return;
    }
    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_SetError("The specified window isn't an OpenGL window");
        return;
    }
    _this->GL_SwapWindow(_this, window);
}

void
SDL_GL_DeleteContext(SDL_GLContext context)
{
    if (!_this || !context) {
        return;
    }
    _this->GL_MakeCurrent(_this, NULL, NULL);
    _this->GL_DeleteContext(_this, context);
}

#if 0                           // FIXME
/*
 * Utility function used by SDL_WM_SetIcon(); flags & 1 for color key, flags
 * & 2 for alpha channel.
 */
static void
CreateMaskFromColorKeyOrAlpha(SDL_Surface * icon, Uint8 * mask, int flags)
{
    int x, y;
    Uint32 colorkey;
#define SET_MASKBIT(icon, x, y, mask) \
	mask[(y*((icon->w+7)/8))+(x/8)] &= ~(0x01<<(7-(x%8)))

    colorkey = icon->format->colorkey;
    switch (icon->format->BytesPerPixel) {
    case 1:
        {
            Uint8 *pixels;
            for (y = 0; y < icon->h; ++y) {
                pixels = (Uint8 *) icon->pixels + y * icon->pitch;
                for (x = 0; x < icon->w; ++x) {
                    if (*pixels++ == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                }
            }
        }
        break;

    case 2:
        {
            Uint16 *pixels;
            for (y = 0; y < icon->h; ++y) {
                pixels = (Uint16 *) icon->pixels + y * icon->pitch / 2;
                for (x = 0; x < icon->w; ++x) {
                    if ((flags & 1) && *pixels == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    } else if ((flags & 2)
                               && (*pixels & icon->format->Amask) == 0) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                    pixels++;
                }
            }
        }
        break;

    case 4:
        {
            Uint32 *pixels;
            for (y = 0; y < icon->h; ++y) {
                pixels = (Uint32 *) icon->pixels + y * icon->pitch / 4;
                for (x = 0; x < icon->w; ++x) {
                    if ((flags & 1) && *pixels == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    } else if ((flags & 2)
                               && (*pixels & icon->format->Amask) == 0) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                    pixels++;
                }
            }
        }
        break;
    }
}

/*
 * Sets the window manager icon for the display window.
 */
void
SDL_WM_SetIcon(SDL_Surface * icon, Uint8 * mask)
{
    if (icon && _this->SetIcon) {
        /* Generate a mask if necessary, and create the icon! */
        if (mask == NULL) {
            int mask_len = icon->h * (icon->w + 7) / 8;
            int flags = 0;
            mask = (Uint8 *) SDL_malloc(mask_len);
            if (mask == NULL) {
                return;
            }
            SDL_memset(mask, ~0, mask_len);
            if (icon->flags & SDL_SRCCOLORKEY)
                flags |= 1;
            if (icon->flags & SDL_SRCALPHA)
                flags |= 2;
            if (flags) {
                CreateMaskFromColorKeyOrAlpha(icon, mask, flags);
            }
            _this->SetIcon(_this, icon, mask);
            SDL_free(mask);
        } else {
            _this->SetIcon(_this, icon, mask);
        }
    }
}
#endif

SDL_bool
SDL_GetWindowWMInfo(SDL_WindowID windowID, struct SDL_SysWMinfo *info)
{
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (!window || !_this->GetWindowWMInfo) {
        return SDL_FALSE;
    }
    return (_this->GetWindowWMInfo(_this, window, info));
}

void
SDL_StartTextInput(void)
{
    if (_this->StartTextInput) {
        _this->StartTextInput(_this);
    }
    SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);
    SDL_EventState(SDL_TEXTEDITING, SDL_ENABLE);
}

void
SDL_StopTextInput(void)
{
    if (_this->StopTextInput) {
        _this->StopTextInput(_this);
    }
    SDL_EventState(SDL_TEXTINPUT, SDL_DISABLE);
    SDL_EventState(SDL_TEXTEDITING, SDL_DISABLE);
}

void
SDL_SetTextInputRect(SDL_Rect *rect)
{
    if (_this->SetTextInputRect) {
        _this->SetTextInputRect(_this, rect);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
