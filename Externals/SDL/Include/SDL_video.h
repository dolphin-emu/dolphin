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
 * \file SDL_video.h
 *
 * Header file for SDL video functions.
 */

#ifndef _SDL_video_h
#define _SDL_video_h

#include "SDL_stdinc.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_surface.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/**
 * \struct SDL_DisplayMode
 *
 * \brief  The structure that defines a display mode
 *
 * \sa SDL_GetNumDisplayModes()
 * \sa SDL_GetDisplayMode()
 * \sa SDL_GetDesktopDisplayMode()
 * \sa SDL_GetCurrentDisplayMode()
 * \sa SDL_GetClosestDisplayMode()
 * \sa SDL_SetDisplayMode()
 */
typedef struct
{
    Uint32 format;              /**< pixel format */
    int w;                      /**< width */
    int h;                      /**< height */
    int refresh_rate;           /**< refresh rate (or zero for unspecified) */
    void *driverdata;           /**< driver-specific data, initialize to 0 */
} SDL_DisplayMode;

/**
 * \typedef SDL_WindowID
 *
 * \brief The type used to identify a window
 *
 * \sa SDL_CreateWindow()
 * \sa SDL_CreateWindowFrom()
 * \sa SDL_DestroyWindow()
 * \sa SDL_GetWindowData()
 * \sa SDL_GetWindowFlags()
 * \sa SDL_GetWindowGrab()
 * \sa SDL_GetWindowPosition()
 * \sa SDL_GetWindowSize()
 * \sa SDL_GetWindowTitle()
 * \sa SDL_HideWindow()
 * \sa SDL_MaximizeWindow()
 * \sa SDL_MinimizeWindow()
 * \sa SDL_RaiseWindow()
 * \sa SDL_RestoreWindow()
 * \sa SDL_SetWindowData()
 * \sa SDL_SetWindowFullscreen()
 * \sa SDL_SetWindowGrab()
 * \sa SDL_SetWindowIcon()
 * \sa SDL_SetWindowPosition()
 * \sa SDL_SetWindowSize()
 * \sa SDL_SetWindowTitle()
 * \sa SDL_ShowWindow()
 */
typedef Uint32 SDL_WindowID;

/**
 * \enum SDL_WindowFlags
 *
 * \brief The flags on a window
 *
 * \sa SDL_GetWindowFlags()
 */
typedef enum
{
    SDL_WINDOW_FULLSCREEN = 0x00000001,         /**< fullscreen window, implies borderless */
    SDL_WINDOW_OPENGL = 0x00000002,             /**< window usable with OpenGL context */
    SDL_WINDOW_SHOWN = 0x00000004,              /**< window is visible */
    SDL_WINDOW_BORDERLESS = 0x00000008,         /**< no window decoration */
    SDL_WINDOW_RESIZABLE = 0x00000010,          /**< window can be resized */
    SDL_WINDOW_MINIMIZED = 0x00000020,          /**< minimized */
    SDL_WINDOW_MAXIMIZED = 0x00000040,          /**< maximized */
    SDL_WINDOW_INPUT_GRABBED = 0x00000100,      /**< window has grabbed input focus */
    SDL_WINDOW_INPUT_FOCUS = 0x00000200,        /**< window has input focus */
    SDL_WINDOW_MOUSE_FOCUS = 0x00000400         /**< window has mouse focus */
} SDL_WindowFlags;

/**
 * \def SDL_WINDOWPOS_UNDEFINED
 * \brief Used to indicate that you don't care what the window position is.
 */
#define SDL_WINDOWPOS_UNDEFINED 0x7FFFFFF
/**
 * \def SDL_WINDOWPOS_CENTERED
 * \brief Used to indicate that the window position should be centered.
 */
#define SDL_WINDOWPOS_CENTERED  0x7FFFFFE

/**
 * \enum SDL_WindowEventID
 *
 * \brief Event subtype for window events
 */
typedef enum
{
    SDL_WINDOWEVENT_NONE,               /**< Never used */
    SDL_WINDOWEVENT_SHOWN,              /**< Window has been shown */
    SDL_WINDOWEVENT_HIDDEN,             /**< Window has been hidden */
    SDL_WINDOWEVENT_EXPOSED,            /**< Window has been exposed and should be redrawn */
    SDL_WINDOWEVENT_MOVED,              /**< Window has been moved to data1,data2 */
    SDL_WINDOWEVENT_RESIZED,            /**< Window size changed to data1xdata2 */
    SDL_WINDOWEVENT_MINIMIZED,          /**< Window has been minimized */
    SDL_WINDOWEVENT_MAXIMIZED,          /**< Window has been maximized */
    SDL_WINDOWEVENT_RESTORED,           /**< Window has been restored to normal size and position */
    SDL_WINDOWEVENT_ENTER,              /**< The window has gained mouse focus */
    SDL_WINDOWEVENT_LEAVE,              /**< The window has lost mouse focus */
    SDL_WINDOWEVENT_FOCUS_GAINED,       /**< The window has gained keyboard focus */
    SDL_WINDOWEVENT_FOCUS_LOST,         /**< The window has lost keyboard focus */
    SDL_WINDOWEVENT_CLOSE               /**< The window manager requests that the window be closed */
} SDL_WindowEventID;

/**
 * \enum SDL_RendererFlags
 *
 * \brief Flags used when creating a rendering context
 */
typedef enum
{
    SDL_RENDERER_SINGLEBUFFER = 0x00000001,     /**< Render directly to the window, if possible */
    SDL_RENDERER_PRESENTCOPY = 0x00000002,      /**< Present uses a copy from back buffer to the front buffer */
    SDL_RENDERER_PRESENTFLIP2 = 0x00000004,     /**< Present uses a flip, swapping back buffer and front buffer */
    SDL_RENDERER_PRESENTFLIP3 = 0x00000008,     /**< Present uses a flip, rotating between two back buffers and a front buffer */
    SDL_RENDERER_PRESENTDISCARD = 0x00000010,   /**< Present leaves the contents of the backbuffer undefined */
    SDL_RENDERER_PRESENTVSYNC = 0x00000020,     /**< Present is synchronized with the refresh rate */
    SDL_RENDERER_ACCELERATED = 0x00000040       /**< The renderer uses hardware acceleration */
} SDL_RendererFlags;

/**
 * \struct SDL_RendererInfo
 *
 * \brief Information on the capabilities of a render driver or context
 */
typedef struct SDL_RendererInfo
{
    const char *name;           /**< The name of the renderer */
    Uint32 flags;               /**< Supported SDL_RendererFlags */
    Uint32 mod_modes;           /**< A mask of supported channel modulation */
    Uint32 blend_modes;         /**< A mask of supported blend modes */
    Uint32 scale_modes;         /**< A mask of supported scale modes */
    Uint32 num_texture_formats; /**< The number of available texture formats */
    Uint32 texture_formats[20]; /**< The available texture formats */
    int max_texture_width;      /**< The maximimum texture width */
    int max_texture_height;     /**< The maximimum texture height */
} SDL_RendererInfo;

/**
 * \enum SDL_TextureAccess
 *
 * \brief The access pattern allowed for a texture
 */
typedef enum
{
    SDL_TEXTUREACCESS_STATIC,    /**< Changes rarely, not lockable */
    SDL_TEXTUREACCESS_STREAMING  /**< Changes frequently, lockable */
} SDL_TextureAccess;

/**
 * \enum SDL_TextureModulate
 *
 * \brief The texture channel modulation used in SDL_RenderCopy()
 */
typedef enum
{
    SDL_TEXTUREMODULATE_NONE = 0x00000000,     /**< No modulation */
    SDL_TEXTUREMODULATE_COLOR = 0x00000001,    /**< srcC = srcC * color */
    SDL_TEXTUREMODULATE_ALPHA = 0x00000002     /**< srcA = srcA * alpha */
} SDL_TextureModulate;

/**
 * \enum SDL_BlendMode
 *
 * \brief The blend mode used in SDL_RenderCopy() and drawing operations
 */
typedef enum
{
    SDL_BLENDMODE_NONE = 0x00000000,     /**< No blending */
    SDL_BLENDMODE_MASK = 0x00000001,     /**< dst = A ? src : dst (alpha is mask) */
    SDL_BLENDMODE_BLEND = 0x00000002,    /**< dst = (src * A) + (dst * (1-A)) */
    SDL_BLENDMODE_ADD = 0x00000004,      /**< dst = (src * A) + dst */
    SDL_BLENDMODE_MOD = 0x00000008       /**< dst = src * dst */
} SDL_BlendMode;

/**
 * \enum SDL_TextureScaleMode
 *
 * \brief The texture scale mode used in SDL_RenderCopy()
 */
typedef enum
{
    SDL_TEXTURESCALEMODE_NONE = 0x00000000,     /**< No scaling, rectangles must match dimensions */
    SDL_TEXTURESCALEMODE_FAST = 0x00000001,     /**< Point sampling or equivalent algorithm */
    SDL_TEXTURESCALEMODE_SLOW = 0x00000002,     /**< Linear filtering or equivalent algorithm */
    SDL_TEXTURESCALEMODE_BEST = 0x00000004      /**< Bicubic filtering or equivalent algorithm */
} SDL_TextureScaleMode;

/**
 * \typedef SDL_TextureID
 *
 * \brief An efficient driver-specific representation of pixel data
 */
typedef Uint32 SDL_TextureID;

/**
 * \typedef SDL_GLContext
 *
 * \brief An opaque handle to an OpenGL context.
 */
typedef void *SDL_GLContext;

/**
 * \enum SDL_GLattr
 *
 * \brief OpenGL configuration attributes
 */
typedef enum
{
    SDL_GL_RED_SIZE,
    SDL_GL_GREEN_SIZE,
    SDL_GL_BLUE_SIZE,
    SDL_GL_ALPHA_SIZE,
    SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_STENCIL_SIZE,
    SDL_GL_ACCUM_RED_SIZE,
    SDL_GL_ACCUM_GREEN_SIZE,
    SDL_GL_ACCUM_BLUE_SIZE,
    SDL_GL_ACCUM_ALPHA_SIZE,
    SDL_GL_STEREO,
    SDL_GL_MULTISAMPLEBUFFERS,
    SDL_GL_MULTISAMPLESAMPLES,
    SDL_GL_ACCELERATED_VISUAL,
    SDL_GL_RETAINED_BACKING
} SDL_GLattr;


/* Function prototypes */

/**
 * \fn int SDL_GetNumVideoDrivers(void)
 *
 * \brief Get the number of video drivers compiled into SDL
 *
 * \sa SDL_GetVideoDriver()
 */
extern DECLSPEC int SDLCALL SDL_GetNumVideoDrivers(void);

/**
 * \fn const char *SDL_GetVideoDriver(int index)
 *
 * \brief Get the name of a built in video driver.
 *
 * \note The video drivers are presented in the order in which they are
 * normally checked during initialization.
 *
 * \sa SDL_GetNumVideoDrivers()
 */
extern DECLSPEC const char *SDLCALL SDL_GetVideoDriver(int index);

/**
 * \fn int SDL_VideoInit(const char *driver_name, Uint32 flags)
 *
 * \brief Initialize the video subsystem, optionally specifying a video driver.
 *
 * \param driver_name Initialize a specific driver by name, or NULL for the default video driver.
 * \param flags FIXME: Still needed?
 *
 * \return 0 on success, -1 on error
 *
 * This function initializes the video subsystem; setting up a connection
 * to the window manager, etc, and determines the available display modes
 * and pixel formats, but does not initialize a window or graphics mode.
 *
 * \sa SDL_VideoQuit()
 */
extern DECLSPEC int SDLCALL SDL_VideoInit(const char *driver_name,
                                          Uint32 flags);

/**
 * \fn void SDL_VideoQuit(void)
 *
 * \brief Shuts down the video subsystem.
 *
 * This function closes all windows, and restores the original video mode.
 *
 * \sa SDL_VideoInit()
 */
extern DECLSPEC void SDLCALL SDL_VideoQuit(void);

/**
 * \fn const char *SDL_GetCurrentVideoDriver(void)
 *
 * \brief Returns the name of the currently initialized video driver.
 *
 * \return The name of the current video driver or NULL if no driver
 *         has been initialized
 *
 * \sa SDL_GetNumVideoDrivers()
 * \sa SDL_GetVideoDriver()
 */
extern DECLSPEC const char *SDLCALL SDL_GetCurrentVideoDriver(void);

/**
 * \fn int SDL_GetNumVideoDisplays(void)
 *
 * \brief Returns the number of available video displays.
 *
 * \sa SDL_SelectVideoDisplay()
 */
extern DECLSPEC int SDLCALL SDL_GetNumVideoDisplays(void);

/**
 * \fn int SDL_SelectVideoDisplay(int index)
 *
 * \brief Set the index of the currently selected display.
 *
 * \return 0 on success, or -1 if the index is out of range.
 *
 * \sa SDL_GetNumVideoDisplays()
 * \sa SDL_GetCurrentVideoDisplay()
 */
extern DECLSPEC int SDLCALL SDL_SelectVideoDisplay(int index);

/**
 * \fn int SDL_GetCurrentVideoDisplay(void)
 *
 * \brief Get the index of the currently selected display.
 *
 * \return The index of the currently selected display.
 *
 * \sa SDL_GetNumVideoDisplays()
 * \sa SDL_SelectVideoDisplay()
 */
extern DECLSPEC int SDLCALL SDL_GetCurrentVideoDisplay(void);

/**
 * \fn int SDL_GetNumDisplayModes(void)
 *
 * \brief Returns the number of available display modes for the current display.
 *
 * \sa SDL_GetDisplayMode()
 */
extern DECLSPEC int SDLCALL SDL_GetNumDisplayModes(void);

/**
 * \fn int SDL_GetDisplayMode(int index, SDL_DisplayMode *mode)
 *
 * \brief Fill in information about a specific display mode.
 *
 * \note The display modes are sorted in this priority:
 *       \li bits per pixel -> more colors to fewer colors
 *       \li width -> largest to smallest
 *       \li height -> largest to smallest
 *       \li refresh rate -> highest to lowest
 *
 * \sa SDL_GetNumDisplayModes()
 */
extern DECLSPEC int SDLCALL SDL_GetDisplayMode(int index,
                                               SDL_DisplayMode * mode);

/**
 * \fn int SDL_GetDesktopDisplayMode(SDL_DisplayMode *mode)
 *
 * \brief Fill in information about the desktop display mode for the current display.
 */
extern DECLSPEC int SDLCALL SDL_GetDesktopDisplayMode(SDL_DisplayMode * mode);

/**
 * \fn int SDL_GetCurrentDisplayMode(SDL_DisplayMode *mode)
 *
 * \brief Fill in information about the current display mode.
 */
extern DECLSPEC int SDLCALL SDL_GetCurrentDisplayMode(SDL_DisplayMode * mode);

/**
 * \fn SDL_DisplayMode *SDL_GetClosestDisplayMode(const SDL_DisplayMode *mode, SDL_DisplayMode *closest)
 *
 * \brief Get the closest match to the requested display mode.
 *
 * \param mode The desired display mode
 * \param closest A pointer to a display mode to be filled in with the closest match of the available display modes.
 *
 * \return The passed in value 'closest', or NULL if no matching video mode was available.
 *
 * The available display modes are scanned, and 'closest' is filled in with the closest mode matching the requested mode and returned.  The mode format and refresh_rate default to the desktop mode if they are 0.  The modes are scanned with size being first priority, format being second priority, and finally checking the refresh_rate.  If all the available modes are too small, then NULL is returned.
 *
 * \sa SDL_GetNumDisplayModes()
 * \sa SDL_GetDisplayMode()
 */
extern DECLSPEC SDL_DisplayMode *SDLCALL SDL_GetClosestDisplayMode(const
                                                                   SDL_DisplayMode
                                                                   * mode,
                                                                   SDL_DisplayMode
                                                                   * closest);

/**
 * \fn int SDL_SetFullscreenDisplayMode(const SDL_DisplayMode *mode)
 *
 * \brief Set the display mode used when a fullscreen window is visible
 *        on the currently selected display.
 *
 * \param mode The mode to use, or NULL for the desktop mode.
 *
 * \return 0 on success, or -1 if setting the display mode failed.
 *
 * \sa SDL_SetWindowFullscreen()
 */
extern DECLSPEC int SDLCALL SDL_SetFullscreenDisplayMode(const SDL_DisplayMode
                                                         * mode);

/**
 * \fn int SDL_GetFullscreenDisplayMode(SDL_DisplayMode *mode)
 *
 * \brief Fill in information about the display mode used when a fullscreen
 *        window is visible on the currently selected display.
 */
extern DECLSPEC int SDLCALL SDL_GetFullscreenDisplayMode(SDL_DisplayMode *
                                                         mode);

/**
 * \fn int SDL_SetDisplayPalette(const SDL_Color *colors, int firstcolor, int ncolors)
 *
 * \brief Set the palette entries for indexed display modes.
 *
 * \return 0 on success, or -1 if the display mode isn't palettized or the colors couldn't be set.
 */
extern DECLSPEC int SDLCALL SDL_SetDisplayPalette(const SDL_Color * colors,
                                                  int firstcolor,
                                                  int ncolors);

/**
 * \fn int SDL_GetDisplayPalette(SDL_Color *colors, int firstcolor, int ncolors)
 *
 * \brief Gets the palette entries for indexed display modes.
 *
 * \return 0 on success, or -1 if the display mode isn't palettized
 */
extern DECLSPEC int SDLCALL SDL_GetDisplayPalette(SDL_Color * colors,
                                                  int firstcolor,
                                                  int ncolors);

/**
 * \fn int SDL_SetGamma(float red, float green, float blue)
 *
 * \brief Set the gamma correction for each of the color channels on the currently selected display.
 *
 * \return 0 on success, or -1 if setting the gamma isn't supported.
 *
 * \sa SDL_SetGammaRamp()
 */
extern DECLSPEC int SDLCALL SDL_SetGamma(float red, float green, float blue);

/**
 * \fn int SDL_SetGammaRamp(const Uint16 * red, const Uint16 * green, const Uint16 * blue)
 *
 * \brief Set the gamma ramp for the currently selected display.
 *
 * \param red The translation table for the red channel, or NULL
 * \param green The translation table for the green channel, or NULL
 * \param blue The translation table for the blue channel, or NULL
 *
 * \return 0 on success, or -1 if gamma ramps are unsupported.
 *
 * Set the gamma translation table for the red, green, and blue channels
 * of the video hardware.  Each table is an array of 256 16-bit quantities,
 * representing a mapping between the input and output for that channel.
 * The input is the index into the array, and the output is the 16-bit
 * gamma value at that index, scaled to the output color precision.
 * 
 * \sa SDL_GetGammaRamp()
 */
extern DECLSPEC int SDLCALL SDL_SetGammaRamp(const Uint16 * red,
                                             const Uint16 * green,
                                             const Uint16 * blue);

/**
 * \fn int SDL_GetGammaRamp(Uint16 * red, Uint16 * green, Uint16 * blue)
 *
 * \brief Get the gamma ramp for the currently selected display.
 *
 * \param red A pointer to a 256 element array of 16-bit quantities to hold the translation table for the red channel, or NULL.
 * \param green A pointer to a 256 element array of 16-bit quantities to hold the translation table for the green channel, or NULL.
 * \param blue A pointer to a 256 element array of 16-bit quantities to hold the translation table for the blue channel, or NULL.
 * 
 * \return 0 on success, or -1 if gamma ramps are unsupported.
 *
 * \sa SDL_SetGammaRamp()
 */
extern DECLSPEC int SDLCALL SDL_GetGammaRamp(Uint16 * red, Uint16 * green,
                                             Uint16 * blue);


/**
 * \fn SDL_WindowID SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
 *
 * \brief Create a window with the specified position, dimensions, and flags.
 *
 * \param title The title of the window, in UTF-8 encoding
 * \param x The x position of the window, SDL_WINDOWPOS_CENTERED, or SDL_WINDOWPOS_UNDEFINED
 * \param y The y position of the window, SDL_WINDOWPOS_CENTERED, or SDL_WINDOWPOS_UNDEFINED
 * \param w The width of the window
 * \param h The height of the window
 * \param flags The flags for the window, a mask of any of the following: SDL_WINDOW_FULLSCREEN, SDL_WINDOW_OPENGL, SDL_WINDOW_SHOWN, SDL_WINDOW_BORDERLESS, SDL_WINDOW_RESIZABLE, SDL_WINDOW_MAXIMIZED, SDL_WINDOW_MINIMIZED, SDL_WINDOW_INPUT_GRABBED
 *
 * \return The id of the window created, or zero if window creation failed.
 *
 * \sa SDL_DestroyWindow()
 */
extern DECLSPEC SDL_WindowID SDLCALL SDL_CreateWindow(const char *title,
                                                      int x, int y, int w,
                                                      int h, Uint32 flags);

/**
 * \fn SDL_WindowID SDL_CreateWindowFrom(void *data)
 *
 * \brief Create an SDL window struct from an existing native window.
 *
 * \param data A pointer to driver-dependent window creation data
 *
 * \return The id of the window created, or zero if window creation failed.
 *
 * \warning This function is NOT SUPPORTED, use at your own risk!
 *
 * \sa SDL_DestroyWindow()
 */
extern DECLSPEC SDL_WindowID SDLCALL SDL_CreateWindowFrom(const void *data);

/**
 * \fn Uint32 SDL_GetWindowFlags(SDL_WindowID windowID)
 *
 * \brief Get the window flags.
 */
extern DECLSPEC Uint32 SDLCALL SDL_GetWindowFlags(SDL_WindowID windowID);

/**
 * \fn void SDL_SetWindowTitle(SDL_WindowID windowID, const char *title)
 *
 * \brief Set the title of the window, in UTF-8 format.
 *
 * \sa SDL_GetWindowTitle()
 */
extern DECLSPEC void SDLCALL SDL_SetWindowTitle(SDL_WindowID windowID,
                                                const char *title);

/**
 * \fn const char *SDL_GetWindowTitle(SDL_WindowID windowID)
 *
 * \brief Get the title of the window, in UTF-8 format.
 *
 * \sa SDL_SetWindowTitle()
 */
extern DECLSPEC const char *SDLCALL SDL_GetWindowTitle(SDL_WindowID windowID);

/**
 * \fn void SDL_SetWindowIcon(SDL_WindowID windowID, SDL_Surface *icon)
 *
 * \brief Set the icon of the window.
 *
 * \param icon The icon for the window
 */
extern DECLSPEC void SDLCALL SDL_SetWindowIcon(SDL_WindowID windowID,
                                               SDL_Surface * icon);

/**
 * \fn void SDL_SetWindowData(SDL_WindowID windowID, void *userdata)
 *
 * \brief Associate an arbitrary pointer with the window.
 *
 * \sa SDL_GetWindowData()
 */
extern DECLSPEC void SDLCALL SDL_SetWindowData(SDL_WindowID windowID,
                                               void *userdata);

/**
 * \fn void *SDL_GetWindowData(SDL_WindowID windowID)
 *
 * \brief Retrieve the data pointer associated with the window.
 *
 * \sa SDL_SetWindowData()
 */
extern DECLSPEC void *SDLCALL SDL_GetWindowData(SDL_WindowID windowID);

/**
 * \fn void SDL_SetWindowPosition(SDL_WindowID windowID, int x, int y)
 *
 * \brief Set the position of the window.
 *
 * \param windowID The window to reposition
 * \param x The x coordinate of the window, SDL_WINDOWPOS_CENTERED, or SDL_WINDOWPOS_UNDEFINED
 * \param y The y coordinate of the window, SDL_WINDOWPOS_CENTERED, or SDL_WINDOWPOS_UNDEFINED
 *
 * \note The window coordinate origin is the upper left of the display.
 *
 * \sa SDL_GetWindowPosition()
 */
extern DECLSPEC void SDLCALL SDL_SetWindowPosition(SDL_WindowID windowID,
                                                   int x, int y);

/**
 * \fn void SDL_GetWindowPosition(SDL_WindowID windowID, int *x, int *y)
 *
 * \brief Get the position of the window.
 *
 * \sa SDL_SetWindowPosition()
 */
extern DECLSPEC void SDLCALL SDL_GetWindowPosition(SDL_WindowID windowID,
                                                   int *x, int *y);

/**
 * \fn void SDL_SetWindowSize(SDL_WindowID windowID, int w, int w)
 *
 * \brief Set the size of the window's client area.
 *
 * \note You can't change the size of a fullscreen window, it automatically
 * matches the size of the display mode.
 *
 * \sa SDL_GetWindowSize()
 */
extern DECLSPEC void SDLCALL SDL_SetWindowSize(SDL_WindowID windowID, int w,
                                               int h);

/**
 * \fn void SDL_GetWindowSize(SDL_WindowID windowID, int *w, int *h)
 *
 * \brief Get the size of the window's client area.
 *
 * \sa SDL_SetWindowSize()
 */
extern DECLSPEC void SDLCALL SDL_GetWindowSize(SDL_WindowID windowID, int *w,
                                               int *h);

/**
 * \fn void SDL_ShowWindow(SDL_WindowID windowID)
 *
 * \brief Show the window
 *
 * \sa SDL_HideWindow()
 */
extern DECLSPEC void SDLCALL SDL_ShowWindow(SDL_WindowID windowID);

/**
 * \fn void SDL_HideWindow(SDL_WindowID windowID)
 *
 * \brief Hide the window
 *
 * \sa SDL_ShowWindow()
 */
extern DECLSPEC void SDLCALL SDL_HideWindow(SDL_WindowID windowID);

/**
 * \fn void SDL_RaiseWindow(SDL_WindowID windowID)
 *
 * \brief Raise the window above other windows and set the input focus.
 */
extern DECLSPEC void SDLCALL SDL_RaiseWindow(SDL_WindowID windowID);

/**
 * \fn void SDL_MaximizeWindow(SDL_WindowID windowID)
 *
 * \brief Make the window as large as possible.
 *
 * \sa SDL_RestoreWindow()
 */
extern DECLSPEC void SDLCALL SDL_MaximizeWindow(SDL_WindowID windowID);

/**
 * \fn void SDL_MinimizeWindow(SDL_WindowID windowID)
 *
 * \brief Minimize the window to an iconic representation.
 *
 * \sa SDL_RestoreWindow()
 */
extern DECLSPEC void SDLCALL SDL_MinimizeWindow(SDL_WindowID windowID);

/**
 * \fn void SDL_RestoreWindow(SDL_WindowID windowID)
 *
 * \brief Restore the size and position of a minimized or maximized window.
 *
 * \sa SDL_MaximizeWindow()
 * \sa SDL_MinimizeWindow()
 */
extern DECLSPEC void SDLCALL SDL_RestoreWindow(SDL_WindowID windowID);

/**
 * \fn int SDL_SetWindowFullscreen(SDL_WindowID windowID, int fullscreen)
 *
 * \brief Set the window's fullscreen state.
 *
 * \return 0 on success, or -1 if setting the display mode failed.
 *
 * \sa SDL_SetFullscreenDisplayMode()
 */
extern DECLSPEC int SDLCALL SDL_SetWindowFullscreen(SDL_WindowID windowID,
                                                    int fullscreen);

/**
 * \fn void SDL_SetWindowGrab(SDL_WindowID windowID, int mode)
 *
 * \brief Set the window's input grab mode.
 *
 * \param mode This is 1 to grab input, and 0 to release input.
 *
 * \sa SDL_GetWindowGrab()
 */
extern DECLSPEC void SDLCALL SDL_SetWindowGrab(SDL_WindowID windowID,
                                               int mode);

/**
 * \fn int SDL_GetWindowGrab(SDL_WindowID windowID)
 *
 * \brief Get the window's input grab mode.
 *
 * \return This returns 1 if input is grabbed, and 0 otherwise.
 *
 * \sa SDL_SetWindowGrab()
 */
extern DECLSPEC int SDLCALL SDL_GetWindowGrab(SDL_WindowID windowID);

/**
 * \fn SDL_bool SDL_GetWindowWMInfo(SDL_WindowID windowID, struct SDL_SysWMinfo * info)
 *
 * \brief Get driver specific information about a window.
 *
 * \note Include SDL_syswm.h for the declaration of SDL_SysWMinfo.
 */
struct SDL_SysWMinfo;
extern DECLSPEC SDL_bool SDLCALL SDL_GetWindowWMInfo(SDL_WindowID windowID,
                                                     struct SDL_SysWMinfo
                                                     *info);

/**
 * \fn void SDL_DestroyWindow(SDL_WindowID windowID)
 *
 * \brief Destroy a window.
 */
extern DECLSPEC void SDLCALL SDL_DestroyWindow(SDL_WindowID windowID);

/**
 * \fn int SDL_GetNumRenderDrivers(void)
 *
 * \brief Get the number of 2D rendering drivers available for the current display.
 *
 * A render driver is a set of code that handles rendering and texture
 * management on a particular display.  Normally there is only one, but
 * some drivers may have several available with different capabilities.
 *
 * \sa SDL_GetRenderDriverInfo()
 * \sa SDL_CreateRenderer()
 */
extern DECLSPEC int SDLCALL SDL_GetNumRenderDrivers(void);

/**
 * \fn int SDL_GetRenderDriverInfo(int index, SDL_RendererInfo *info)
 *
 * \brief Get information about a specific 2D rendering driver for the current display.
 *
 * \param index The index of the driver to query information about.
 * \param info A pointer to an SDL_RendererInfo struct to be filled with information on the rendering driver.
 *
 * \return 0 on success, -1 if the index was out of range
 *
 * \sa SDL_CreateRenderer()
 */
extern DECLSPEC int SDLCALL SDL_GetRenderDriverInfo(int index,
                                                    SDL_RendererInfo * info);

/**
 * \fn int SDL_CreateRenderer(SDL_WindowID window, int index, Uint32 flags)
 *
 * \brief Create and make active a 2D rendering context for a window.
 *
 * \param windowID The window used for rendering
 * \param index The index of the rendering driver to initialize, or -1 to initialize the first one supporting the requested flags.
 * \param flags SDL_RendererFlags
 *
 * \return 0 on success, -1 if the flags were not supported, or -2 if
 *         there isn't enough memory to support the requested flags
 *
 * \sa SDL_SelectRenderer()
 * \sa SDL_GetRendererInfo()
 * \sa SDL_DestroyRenderer()
 */
extern DECLSPEC int SDLCALL SDL_CreateRenderer(SDL_WindowID windowID,
                                               int index, Uint32 flags);

/**
 * \fn int SDL_SelectRenderer(SDL_WindowID windowID)
 *
 * \brief Select the rendering context for a particular window.
 *
 * \return 0 on success, -1 if the selected window doesn't have a
 *         rendering context.
 */
extern DECLSPEC int SDLCALL SDL_SelectRenderer(SDL_WindowID windowID);

/**
 * \fn int SDL_GetRendererInfo(SDL_RendererInfo *info)
 *
 * \brief Get information about the current rendering context.
 */
extern DECLSPEC int SDLCALL SDL_GetRendererInfo(SDL_RendererInfo * info);

/**
 * \fn SDL_TextureID SDL_CreateTexture(Uint32 format, int access, int w, int h)
 *
 * \brief Create a texture for the current rendering context.
 *
 * \param format The format of the texture
 * \param access One of the enumerated values in SDL_TextureAccess
 * \param w The width of the texture in pixels
 * \param h The height of the texture in pixels
 *
 * \return The created texture is returned, or 0 if no rendering context was active,  the format was unsupported, or the width or height were out of range.
 *
 * \sa SDL_QueryTexture()
 * \sa SDL_DestroyTexture()
 */
extern DECLSPEC SDL_TextureID SDLCALL SDL_CreateTexture(Uint32 format,
                                                        int access, int w,
                                                        int h);

/**
 * \fn SDL_TextureID SDL_CreateTextureFromSurface(Uint32 format, SDL_Surface *surface)
 *
 * \brief Create a texture from an existing surface.
 *
 * \param format The format of the texture, or 0 to pick an appropriate format
 * \param surface The surface containing pixel data used to fill the texture
 *
 * \return The created texture is returned, or 0 if no rendering context was active,  the format was unsupported, or the surface width or height were out of range.
 *
 * \note The surface is not modified or freed by this function.
 *
 * \sa SDL_QueryTexture()
 * \sa SDL_DestroyTexture()
 */
extern DECLSPEC SDL_TextureID SDLCALL SDL_CreateTextureFromSurface(Uint32
                                                                   format,
                                                                   SDL_Surface
                                                                   * surface);

/**
 * \fn int SDL_QueryTexture(SDL_TextureID textureID, Uint32 *format, int *access, int *w, int *h)
 *
 * \brief Query the attributes of a texture
 *
 * \param texture A texture to be queried
 * \param format A pointer filled in with the raw format of the texture.  The actual format may differ, but pixel transfers will use this format.
 * \param access A pointer filled in with the actual access to the texture.
 * \param w A pointer filled in with the width of the texture in pixels
 * \param h A pointer filled in with the height of the texture in pixels
 *
 * \return 0 on success, or -1 if the texture is not valid
 */
extern DECLSPEC int SDLCALL SDL_QueryTexture(SDL_TextureID textureID,
                                             Uint32 * format, int *access,
                                             int *w, int *h);

/**
 * \fn int SDL_QueryTexturePixels(SDL_TextureID textureID, void **pixels, int pitch)
 *
 * \brief Query the pixels of a texture, if the texture does not need to be locked for pixel access.
 *
 * \param texture A texture to be queried, which was created with SDL_TEXTUREACCESS_STREAMING
 * \param pixels A pointer filled with a pointer to the pixels for the texture 
 * \param pitch A pointer filled in with the pitch of the pixel data
 *
 * \return 0 on success, or -1 if the texture is not valid, or must be locked for pixel access.
 */
extern DECLSPEC int SDLCALL SDL_QueryTexturePixels(SDL_TextureID textureID,
                                                   void **pixels, int *pitch);

/**
 * \fn int SDL_SetTexturePalette(SDL_TextureID textureID, const SDL_Color * colors, int firstcolor, int ncolors)
 *
 * \brief Update an indexed texture with a color palette
 *
 * \param texture The texture to update
 * \param colors The array of RGB color data
 * \param firstcolor The first index to update
 * \param ncolors The number of palette entries to fill with the color data
 *
 * \return 0 on success, or -1 if the texture is not valid or not an indexed texture
 */
extern DECLSPEC int SDLCALL SDL_SetTexturePalette(SDL_TextureID textureID,
                                                  const SDL_Color * colors,
                                                  int firstcolor,
                                                  int ncolors);

/**
 * \fn int SDL_GetTexturePalette(SDL_TextureID textureID, SDL_Color * colors, int firstcolor, int ncolors)
 *
 * \brief Update an indexed texture with a color palette
 *
 * \param texture The texture to update
 * \param colors The array to fill with RGB color data
 * \param firstcolor The first index to retrieve
 * \param ncolors The number of palette entries to retrieve
 *
 * \return 0 on success, or -1 if the texture is not valid or not an indexed texture
 */
extern DECLSPEC int SDLCALL SDL_GetTexturePalette(SDL_TextureID textureID,
                                                  SDL_Color * colors,
                                                  int firstcolor,
                                                  int ncolors);

/**
 * \fn int SDL_SetTextureColorMod(SDL_TextureID textureID, Uint8 r, Uint8 g, Uint8 b)
 *
 * \brief Set an additional color value used in render copy operations
 *
 * \param texture The texture to update
 * \param r The red source color value multiplied into copy operations
 * \param g The green source color value multiplied into copy operations
 * \param b The blue source color value multiplied into copy operations
 *
 * \return 0 on success, or -1 if the texture is not valid or color modulation is not supported
 *
 * \sa SDL_GetTextureColorMod()
 */
extern DECLSPEC int SDLCALL SDL_SetTextureColorMod(SDL_TextureID textureID,
                                                   Uint8 r, Uint8 g, Uint8 b);


/**
 * \fn int SDL_GetTextureColorMod(SDL_TextureID textureID, Uint8 *r, Uint8 *g, Uint8 *b)
 *
 * \brief Get the additional color value used in render copy operations
 *
 * \param texture The texture to query
 * \param r A pointer filled in with the source red color value
 * \param g A pointer filled in with the source green color value
 * \param b A pointer filled in with the source blue color value
 *
 * \return 0 on success, or -1 if the texture is not valid
 *
 * \sa SDL_SetTextureColorMod()
 */
extern DECLSPEC int SDLCALL SDL_GetTextureColorMod(SDL_TextureID textureID,
                                                   Uint8 * r, Uint8 * g,
                                                   Uint8 * b);

/**
 * \fn int SDL_SetTextureAlphaMod(SDL_TextureID textureID, Uint8 alpha)
 *
 * \brief Set an additional alpha value used in render copy operations
 *
 * \param texture The texture to update
 * \param alpha The source alpha value multiplied into copy operations.
 *
 * \return 0 on success, or -1 if the texture is not valid or alpha modulation is not supported
 *
 * \sa SDL_GetTextureAlphaMod()
 */
extern DECLSPEC int SDLCALL SDL_SetTextureAlphaMod(SDL_TextureID textureID,
                                                   Uint8 alpha);

/**
 * \fn int SDL_GetTextureAlphaMod(SDL_TextureID textureID, Uint8 *alpha)
 *
 * \brief Get the additional alpha value used in render copy operations
 *
 * \param texture The texture to query
 * \param alpha A pointer filled in with the source alpha value
 *
 * \return 0 on success, or -1 if the texture is not valid
 *
 * \sa SDL_SetTextureAlphaMod()
 */
extern DECLSPEC int SDLCALL SDL_GetTextureAlphaMod(SDL_TextureID textureID,
                                                   Uint8 * alpha);

/**
 * \fn int SDL_SetTextureBlendMode(SDL_TextureID textureID, int blendMode)
 *
 * \brief Set the blend mode used for texture copy operations
 *
 * \param texture The texture to update
 * \param blendMode SDL_TextureBlendMode to use for texture blending
 *
 * \return 0 on success, or -1 if the texture is not valid or the blend mode is not supported
 *
 * \note If the blend mode is not supported, the closest supported mode is chosen.
 *
 * \sa SDL_GetTextureBlendMode()
 */
extern DECLSPEC int SDLCALL SDL_SetTextureBlendMode(SDL_TextureID textureID,
                                                    int blendMode);

/**
 * \fn int SDL_GetTextureBlendMode(SDL_TextureID textureID, int *blendMode)
 *
 * \brief Get the blend mode used for texture copy operations
 *
 * \param texture The texture to query
 * \param blendMode A pointer filled in with the current blend mode
 *
 * \return 0 on success, or -1 if the texture is not valid
 *
 * \sa SDL_SetTextureBlendMode()
 */
extern DECLSPEC int SDLCALL SDL_GetTextureBlendMode(SDL_TextureID textureID,
                                                    int *blendMode);

/**
 * \fn int SDL_SetTextureScaleMode(SDL_TextureID textureID, int scaleMode)
 *
 * \brief Set the scale mode used for texture copy operations
 *
 * \param texture The texture to update
 * \param scaleMode SDL_TextureScaleMode to use for texture scaling
 *
 * \return 0 on success, or -1 if the texture is not valid or the scale mode is not supported
 *
 * \note If the scale mode is not supported, the closest supported mode is chosen.
 *
 * \sa SDL_GetTextureScaleMode()
 */
extern DECLSPEC int SDLCALL SDL_SetTextureScaleMode(SDL_TextureID textureID,
                                                    int scaleMode);

/**
 * \fn int SDL_GetTextureScaleMode(SDL_TextureID textureID, int *scaleMode)
 *
 * \brief Get the scale mode used for texture copy operations
 *
 * \param texture The texture to query
 * \param scaleMode A pointer filled in with the current scale mode
 *
 * \return 0 on success, or -1 if the texture is not valid
 *
 * \sa SDL_SetTextureScaleMode()
 */
extern DECLSPEC int SDLCALL SDL_GetTextureScaleMode(SDL_TextureID textureID,
                                                    int *scaleMode);

/**
 * \fn int SDL_UpdateTexture(SDL_TextureID textureID, const SDL_Rect *rect, const void *pixels, int pitch)
 *
 * \brief Update the given texture rectangle with new pixel data.
 *
 * \param texture The texture to update
 * \param rect A pointer to the rectangle of pixels to update, or NULL to update the entire texture.
 * \param pixels The raw pixel data
 * \param pitch The number of bytes between rows of pixel data
 *
 * \return 0 on success, or -1 if the texture is not valid
 *
 * \note This is a fairly slow function.
 */
extern DECLSPEC int SDLCALL SDL_UpdateTexture(SDL_TextureID textureID,
                                              const SDL_Rect * rect,
                                              const void *pixels, int pitch);

/**
 * \fn void SDL_LockTexture(SDL_TextureID textureID, const SDL_Rect *rect, int markDirty, void **pixels, int *pitch)
 *
 * \brief Lock a portion of the texture for pixel access.
 *
 * \param textureID The texture to lock for access, which was created with SDL_TEXTUREACCESS_STREAMING.
 * \param rect A pointer to the rectangle to lock for access. If the rect is NULL, the entire texture will be locked.
 * \param markDirty If this is nonzero, the locked area will be marked dirty when the texture is unlocked.
 * \param pixels This is filled in with a pointer to the locked pixels, appropriately offset by the locked area.
 * \param pitch This is filled in with the pitch of the locked pixels.
 *
 * \return 0 on success, or -1 if the texture is not valid or was created with SDL_TEXTUREACCESS_STATIC
 *
 * \sa SDL_DirtyTexture()
 * \sa SDL_UnlockTexture()
 */
extern DECLSPEC int SDLCALL SDL_LockTexture(SDL_TextureID textureID,
                                            const SDL_Rect * rect,
                                            int markDirty, void **pixels,
                                            int *pitch);

/**
 * \fn void SDL_UnlockTexture(SDL_TextureID textureID)
 *
 * \brief Unlock a texture, uploading the changes to video memory, if needed.
 *
 * \sa SDL_LockTexture()
 * \sa SDL_DirtyTexture()
 */
extern DECLSPEC void SDLCALL SDL_UnlockTexture(SDL_TextureID textureID);

/**
 * \fn void SDL_DirtyTexture(SDL_TextureID textureID, int numrects, const SDL_Rect * rects)
 *
 * \brief Mark the specified rectangles of the texture as dirty.
 *
 * \param textureID The texture to mark dirty, which was created with SDL_TEXTUREACCESS_STREAMING.
 * \param numrects The number of rectangles pointed to by rects.
 * \param rects The pointer to an array of dirty rectangles.
 *
 * \sa SDL_LockTexture()
 * \sa SDL_UnlockTexture()
 */
extern DECLSPEC void SDLCALL SDL_DirtyTexture(SDL_TextureID textureID,
                                              int numrects,
                                              const SDL_Rect * rects);

/**
 * \fn void SDL_SetRenderDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
 *
 * \brief Set the color used for drawing operations (Fill and Line).
 *
 * \param r The red value used to draw on the rendering target
 * \param g The green value used to draw on the rendering target
 * \param b The blue value used to draw on the rendering target
 * \param a The alpha value used to draw on the rendering target, usually SDL_ALPHA_OPAQUE (255)
 * \return 0 on success, or -1 if there is no rendering context current
 */
extern DECLSPEC int SDL_SetRenderDrawColor(Uint8 r, Uint8 g, Uint8 b,
                                           Uint8 a);

/**
 * \fn void SDL_GetRenderDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
 *
 * \brief Get the color used for drawing operations (Fill and Line).
 *
 * \param r A pointer to the red value used to draw on the rendering target
 * \param g A pointer to the green value used to draw on the rendering target
 * \param b A pointer to the blue value used to draw on the rendering target
 * \param a A pointer to the alpha value used to draw on the rendering target, usually SDL_ALPHA_OPAQUE (255)
 * \return 0 on success, or -1 if there is no rendering context current
 */
extern DECLSPEC int SDL_GetRenderDrawColor(Uint8 * r, Uint8 * g, Uint8 * b,
                                           Uint8 * a);

/**
 * \fn int SDL_SetRenderDrawBlendMode(int blendMode)
 *
 * \brief Set the blend mode used for drawing operations (Fill and Line).
 *
 * \param blendMode SDL_BlendMode to use for blending
 *
 * \return 0 on success, or -1 if there is no rendering context current
 *
 * \note If the blend mode is not supported, the closest supported mode is chosen.
 *
 * \sa SDL_SetRenderDrawBlendMode()
 */
extern DECLSPEC int SDLCALL SDL_SetRenderDrawBlendMode(int blendMode);

/**
 * \fn int SDL_GetRenderDrawBlendMode(int *blendMode)
 *
 * \brief Get the blend mode used for drawing operations
 *
 * \param blendMode A pointer filled in with the current blend mode
 *
 * \return 0 on success, or -1 if there is no rendering context current
 *
 * \sa SDL_SetRenderDrawBlendMode()
 */
extern DECLSPEC int SDLCALL SDL_GetRenderDrawBlendMode(int *blendMode);

/**
 * \fn int SDL_RenderPoint(int x, int y)
 *
 * \brief Draw a point on the current rendering target.
 *
 * \param x The x coordinate of the point
 * \param y The y coordinate of the point
 *
 * \return 0 on success, or -1 if there is no rendering context current
 */
extern DECLSPEC int SDLCALL SDL_RenderPoint(int x, int y);

/**
 * \fn int SDL_RenderLine(int x1, int y1, int x2, int y2)
 *
 * \brief Draw a line on the current rendering target.
 *
 * \param x1 The x coordinate of the start point
 * \param y1 The y coordinate of the start point
 * \param x2 The x coordinate of the end point
 * \param y2 The y coordinate of the end point
 *
 * \return 0 on success, or -1 if there is no rendering context current
 */
extern DECLSPEC int SDLCALL SDL_RenderLine(int x1, int y1, int x2, int y2);

/**
 * \fn void SDL_RenderFill(const SDL_Rect *rect)
 *
 * \brief Fill the current rendering target with the drawing color.
 *
 * \param r The red value used to fill the rendering target
 * \param g The green value used to fill the rendering target
 * \param b The blue value used to fill the rendering target
 * \param a The alpha value used to fill the rendering target, usually SDL_ALPHA_OPAQUE (255)
 * \param rect A pointer to the destination rectangle, or NULL for the entire rendering target.
 *
 * \return 0 on success, or -1 if there is no rendering context current
 */
extern DECLSPEC int SDLCALL SDL_RenderFill(const SDL_Rect * rect);

/**
 * \fn int SDL_RenderCopy(SDL_TextureID textureID, const SDL_Rect *srcrect, const SDL_Rect *dstrect)
 *
 * \brief Copy a portion of the texture to the current rendering target.
 *
 * \param texture The source texture.
 * \param srcrect A pointer to the source rectangle, or NULL for the entire texture.
 * \param dstrect A pointer to the destination rectangle, or NULL for the entire rendering target.
 *
 * \return 0 on success, or -1 if there is no rendering context current, or the driver doesn't support the requested operation.
 */
extern DECLSPEC int SDLCALL SDL_RenderCopy(SDL_TextureID textureID,
                                           const SDL_Rect * srcrect,
                                           const SDL_Rect * dstrect);

/**
 * \fn int SDL_RenderReadPixels(const SDL_Rect *rect, void *pixels, int pitch)
 *
 * \brief Read pixels from the current rendering target.
 *
 * \param rect A pointer to the rectangle to read, or NULL for the entire render target
 * \param pixels A pointer to be filled in with the pixel data
 * \param pitch The pitch of the pixels parameter
 *
 * \return 0 on success, or -1 if pixel reading is not supported.
 *
 * \warning This is a very slow operation, and should not be used frequently.
 */
extern DECLSPEC int SDLCALL SDL_RenderReadPixels(const SDL_Rect * rect,
                                                 void *pixels, int pitch);

/**
 * \fn int SDL_RenderWritePixels(const SDL_Rect *rect, const void *pixels, int pitch)
 *
 * \brief Write pixels to the current rendering target.
 *
 * \param rect A pointer to the rectangle to write, or NULL for the entire render target
 * \param pixels A pointer to the pixel data to write
 * \param pitch The pitch of the pixels parameter
 *
 * \return 0 on success, or -1 if pixel writing is not supported.
 *
 * \warning This is a very slow operation, and should not be used frequently.
 */
extern DECLSPEC int SDLCALL SDL_RenderWritePixels(const SDL_Rect * rect,
                                                  const void *pixels,
                                                  int pitch);

/**
 * \fn void SDL_RenderPresent(void)
 *
 * \brief Update the screen with rendering performed.
 */
extern DECLSPEC void SDLCALL SDL_RenderPresent(void);

/**
 * \fn void SDL_DestroyTexture(SDL_TextureID textureID);
 *
 * \brief Destroy the specified texture.
 *
 * \sa SDL_CreateTexture()
 * \sa SDL_CreateTextureFromSurface()
 */
extern DECLSPEC void SDLCALL SDL_DestroyTexture(SDL_TextureID textureID);

/**
 * \fn void SDL_DestroyRenderer(SDL_WindowID windowID);
 *
 * \brief Destroy the rendering context for a window and free associated
 *        textures.
 *
 * \sa SDL_CreateRenderer()
 */
extern DECLSPEC void SDLCALL SDL_DestroyRenderer(SDL_WindowID windowID);

/**
 * \fn SDL_bool SDL_IsScreenSaverEnabled();
 *
 * \brief Returns whether the screensaver is currently enabled (default off).
 *
 * \sa SDL_EnableScreenSaver()
 * \sa SDL_DisableScreenSaver()
 */
extern DECLSPEC SDL_bool SDLCALL SDL_IsScreenSaverEnabled(void);

/**
 * \fn void SDL_EnableScreenSaver();
 *
 * \brief Allow the screen to be blanked by a screensaver
 *
 * \sa SDL_IsScreenSaverEnabled()
 * \sa SDL_DisableScreenSaver()
 */
extern DECLSPEC void SDLCALL SDL_EnableScreenSaver(void);

/**
 * \fn void SDL_DisableScreenSaver();
 *
 * \brief Prevent the screen from being blanked by a screensaver
 *
 * \sa SDL_IsScreenSaverEnabled()
 * \sa SDL_EnableScreenSaver()
 */
extern DECLSPEC void SDLCALL SDL_DisableScreenSaver(void);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* OpenGL support functions.                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * \fn int SDL_GL_LoadLibrary(const char *path)
 *
 * \brief Dynamically load an OpenGL library.
 *
 * \param path The platform dependent OpenGL library name, or NULL to open the default OpenGL library
 *
 * \return 0 on success, or -1 if the library couldn't be loaded
 *
 * This should be done after initializing the video driver, but before
 * creating any OpenGL windows.  If no OpenGL library is loaded, the default
 * library will be loaded upon creation of the first OpenGL window.
 *
 * \note If you do this, you need to retrieve all of the GL functions used in
 *       your program from the dynamic library using SDL_GL_GetProcAddress().
 *
 * \sa SDL_GL_GetProcAddress()
 */
extern DECLSPEC int SDLCALL SDL_GL_LoadLibrary(const char *path);

/**
 * \fn void *SDL_GL_GetProcAddress(const char *proc)
 *
 * \brief Get the address of an OpenGL function.
 */
extern DECLSPEC void *SDLCALL SDL_GL_GetProcAddress(const char *proc);

/**
 * \fn SDL_bool SDL_GL_ExtensionSupported(const char *extension)
 *
 * \brief Return true if an OpenGL extension is supported for the current context.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GL_ExtensionSupported(const char
                                                           *extension);

/**
 * \fn int SDL_GL_SetAttribute(SDL_GLattr attr, int value)
 *
 * \brief Set an OpenGL window attribute before window creation.
 */
extern DECLSPEC int SDLCALL SDL_GL_SetAttribute(SDL_GLattr attr, int value);

/**
 * \fn int SDL_GL_GetWindowAttribute(SDL_WindowID windowID, SDL_GLattr attr, int *value)
 *
 * \brief Get the actual value for an attribute from the current context.
 */
extern DECLSPEC int SDLCALL SDL_GL_GetAttribute(SDL_GLattr attr, int *value);

/**
 * \fn SDL_GLContext SDL_GL_CreateContext(SDL_WindowID windowID)
 *
 * \brief Create an OpenGL context for use with an OpenGL window, and make it current.
 *
 * \sa SDL_GL_DeleteContext()
 */
extern DECLSPEC SDL_GLContext SDLCALL SDL_GL_CreateContext(SDL_WindowID
                                                           windowID);

/**
 * \fn int SDL_GL_MakeCurrent(SDL_WindowID windowID, SDL_GLContext context)
 *
 * \brief Set up an OpenGL context for rendering into an OpenGL window.
 *
 * \note The context must have been created with a compatible window.
 */
extern DECLSPEC int SDLCALL SDL_GL_MakeCurrent(SDL_WindowID windowID,
                                               SDL_GLContext context);

/**
 * \fn int SDL_GL_SetSwapInterval(int interval)
 *
 * \brief Set the swap interval for the current OpenGL context.
 *
 * \param interval 0 for immediate updates, 1 for updates synchronized with the vertical retrace
 *
 * \return 0 on success, or -1 if setting the swap interval is not supported.
 *
 * \sa SDL_GL_GetSwapInterval()
 */
extern DECLSPEC int SDLCALL SDL_GL_SetSwapInterval(int interval);

/**
 * \fn int SDL_GL_GetSwapInterval(void)
 *
 * \brief Get the swap interval for the current OpenGL context.
 *
 * \return 0 if there is no vertical retrace synchronization, 1 if the buffer swap is synchronized with the vertical retrace, and -1 if getting the swap interval is not supported.
 *
 * \sa SDL_GL_SetSwapInterval()
 */
extern DECLSPEC int SDLCALL SDL_GL_GetSwapInterval(void);

/**
 * \fn void SDL_GL_SwapWindow(SDL_WindowID windowID)
 *
 * \brief Swap the OpenGL buffers for the window, if double-buffering is supported.
 */
extern DECLSPEC void SDLCALL SDL_GL_SwapWindow(SDL_WindowID windowID);

/**
 * \fn void SDL_GL_DeleteContext(SDL_GLContext context)
 *
 * \brief Delete an OpenGL context.
 *
 * \sa SDL_GL_CreateContext()
 */
extern DECLSPEC void SDLCALL SDL_GL_DeleteContext(SDL_GLContext context);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_video_h */

/* vi: set ts=4 sw=4 expandtab: */
