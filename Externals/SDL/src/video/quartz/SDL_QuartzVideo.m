/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_QuartzVideo.h"
#include "SDL_QuartzWindow.h"

#ifdef __powerpc__  /* I'm gambling they fixed this by 10.4. --ryan. */
/*
    Add methods to get at private members of NSScreen. 
    Since there is a bug in Apple's screen switching code
    that does not update this variable when switching
    to fullscreen, we'll set it manually (but only for the
    main screen).
*/
@interface NSScreen (NSScreenAccess)
- (void) setFrame:(NSRect)frame;
@end

@implementation NSScreen (NSScreenAccess)
- (void) setFrame:(NSRect)frame;
{
    _frame = frame;
}
@end
static inline void QZ_SetFrame(NSScreen *nsscreen, NSRect frame)
{
    [nsscreen setFrame:frame];
}
#else
static inline void QZ_SetFrame(NSScreen *nsscreen, NSRect frame)
{
}
#endif

@interface SDLTranslatorResponder : NSTextView
{
}
- (void) doCommandBySelector:(SEL)myselector;
@end

@implementation SDLTranslatorResponder
- (void) doCommandBySelector:(SEL) myselector {}
@end

/* absent in 10.3.9.  */
CG_EXTERN CGImageRef CGBitmapContextCreateImage (CGContextRef);

/* Bootstrap functions */
static int              QZ_Available ();
static SDL_VideoDevice* QZ_CreateDevice (int device_index);
static void             QZ_DeleteDevice (SDL_VideoDevice *device);

/* Initialization, Query, Setup, and Redrawing functions */
static int          QZ_VideoInit        (_THIS, SDL_PixelFormat *video_format);

static SDL_Rect**   QZ_ListModes        (_THIS, SDL_PixelFormat *format,
                                         Uint32 flags);
static void         QZ_UnsetVideoMode   (_THIS, BOOL to_desktop);

static SDL_Surface* QZ_SetVideoMode     (_THIS, SDL_Surface *current,
                                         int width, int height, int bpp,
                                         Uint32 flags);
static int          QZ_ToggleFullScreen (_THIS, int on);
static int          QZ_SetColors        (_THIS, int first_color,
                                         int num_colors, SDL_Color *colors);

static int          QZ_LockDoubleBuffer   (_THIS, SDL_Surface *surface);
static void         QZ_UnlockDoubleBuffer (_THIS, SDL_Surface *surface);
static int          QZ_ThreadFlip         (_THIS);
static int          QZ_FlipDoubleBuffer   (_THIS, SDL_Surface *surface);
static void         QZ_DoubleBufferUpdate (_THIS, int num_rects, SDL_Rect *rects);

static void         QZ_DirectUpdate     (_THIS, int num_rects, SDL_Rect *rects);
static void         QZ_UpdateRects      (_THIS, int num_rects, SDL_Rect *rects);
static void         QZ_VideoQuit        (_THIS);

/* Hardware surface functions (for fullscreen mode only) */
#if 0 /* Not used (apparently, it's really slow) */
static int  QZ_FillHWRect (_THIS, SDL_Surface *dst, SDL_Rect *rect, Uint32 color);
#endif
static int  QZ_LockHWSurface(_THIS, SDL_Surface *surface);
static void QZ_UnlockHWSurface(_THIS, SDL_Surface *surface);
static int QZ_AllocHWSurface(_THIS, SDL_Surface *surface);
static void QZ_FreeHWSurface (_THIS, SDL_Surface *surface);
/* static int  QZ_FlipHWSurface (_THIS, SDL_Surface *surface); */

/* Bootstrap binding, enables entry point into the driver */
VideoBootStrap QZ_bootstrap = {
    "Quartz", "Mac OS X CoreGraphics", QZ_Available, QZ_CreateDevice
};


/* Bootstrap functions */
static int QZ_Available ()
{
    return 1;
}

static SDL_VideoDevice* QZ_CreateDevice (int device_index)
{
#pragma unused (device_index)

    SDL_VideoDevice *device;
    SDL_PrivateVideoData *hidden;

    device = (SDL_VideoDevice*) SDL_malloc (sizeof (*device) );
    hidden = (SDL_PrivateVideoData*) SDL_malloc (sizeof (*hidden) );

    if (device == NULL || hidden == NULL)
        SDL_OutOfMemory ();

    SDL_memset (device, 0, sizeof (*device) );
    SDL_memset (hidden, 0, sizeof (*hidden) );

    device->hidden = hidden;

    device->VideoInit        = QZ_VideoInit;
    device->ListModes        = QZ_ListModes;
    device->SetVideoMode     = QZ_SetVideoMode;
    device->ToggleFullScreen = QZ_ToggleFullScreen;
    device->UpdateMouse      = QZ_UpdateMouse;
    device->SetColors        = QZ_SetColors;
    /* device->UpdateRects      = QZ_UpdateRects; this is determined by SetVideoMode() */
    device->VideoQuit        = QZ_VideoQuit;

    device->LockHWSurface   = QZ_LockHWSurface;
    device->UnlockHWSurface = QZ_UnlockHWSurface;
    device->AllocHWSurface   = QZ_AllocHWSurface;
    device->FreeHWSurface   = QZ_FreeHWSurface;
    /* device->FlipHWSurface   = QZ_FlipHWSurface */;

    device->SetGamma     = QZ_SetGamma;
    device->GetGamma     = QZ_GetGamma;
    device->SetGammaRamp = QZ_SetGammaRamp;
    device->GetGammaRamp = QZ_GetGammaRamp;

    device->GL_GetProcAddress = QZ_GL_GetProcAddress;
    device->GL_GetAttribute   = QZ_GL_GetAttribute;
    device->GL_MakeCurrent    = QZ_GL_MakeCurrent;
    device->GL_SwapBuffers    = QZ_GL_SwapBuffers;
    device->GL_LoadLibrary    = QZ_GL_LoadLibrary;

    device->FreeWMCursor   = QZ_FreeWMCursor;
    device->CreateWMCursor = QZ_CreateWMCursor;
    device->ShowWMCursor   = QZ_ShowWMCursor;
    device->WarpWMCursor   = QZ_WarpWMCursor;
    device->MoveWMCursor   = QZ_MoveWMCursor;
    device->CheckMouseMode = QZ_CheckMouseMode;
    device->InitOSKeymap   = QZ_InitOSKeymap;
    device->PumpEvents     = QZ_PumpEvents;

    device->SetCaption    = QZ_SetCaption;
    device->SetIcon       = QZ_SetIcon;
    device->IconifyWindow = QZ_IconifyWindow;
    /*device->GetWMInfo     = QZ_GetWMInfo;*/
    device->GrabInput     = QZ_GrabInput;

    /*
     * This is a big hassle, needing QuickDraw and QuickTime on older
     *  systems, and god knows what on 10.6, so we immediately fail here,
     *  which causes SDL to make an RGB surface and manage the YUV overlay
     *  in software. Sorry. Use SDL 1.3 if you want YUV rendering in a pixel
     *  shader.  :)
     */
    /*device->CreateYUVOverlay = QZ_CreateYUVOverlay;*/

    device->free             = QZ_DeleteDevice;

    return device;
}

static void QZ_DeleteDevice (SDL_VideoDevice *device)
{
    SDL_free (device->hidden);
    SDL_free (device);
}

static int QZ_VideoInit (_THIS, SDL_PixelFormat *video_format)
{
    NSRect r = NSMakeRect(0.0, 0.0, 0.0, 0.0);
    const char *env = NULL;
	
    /* Initialize the video settings; this data persists between mode switches */
    display_id = kCGDirectMainDisplay;

#if 0 /* The mouse event code needs to take this into account... */
    env = getenv("SDL_VIDEO_FULLSCREEN_DISPLAY");
    if ( env ) {
        int monitor = SDL_atoi(env);
    	CGDirectDisplayID activeDspys [3];
    	CGDisplayCount dspyCnt;
    	CGGetActiveDisplayList (3, activeDspys, &dspyCnt);
        if ( monitor >= 0 && monitor < dspyCnt ) {
    	    display_id = activeDspys[monitor];
        }
    }
#endif

    save_mode  = CGDisplayCurrentMode    (display_id);
    mode_list  = CGDisplayAvailableModes (display_id);
    palette    = CGPaletteCreateDefaultColorPalette ();

    /* Allow environment override of screensaver disable. */
    env = SDL_getenv("SDL_VIDEO_ALLOW_SCREENSAVER");
    if ( env ) {
        allow_screensaver = SDL_atoi(env);
    } else {
#ifdef SDL_VIDEO_DISABLE_SCREENSAVER
        allow_screensaver = 0;
#else
        allow_screensaver = 1;
#endif
    }

    /* Gather some information that is useful to know about the display */
    CFNumberGetValue (CFDictionaryGetValue (save_mode, kCGDisplayBitsPerPixel),
                      kCFNumberSInt32Type, &device_bpp);

    CFNumberGetValue (CFDictionaryGetValue (save_mode, kCGDisplayWidth),
                      kCFNumberSInt32Type, &device_width);

    CFNumberGetValue (CFDictionaryGetValue (save_mode, kCGDisplayHeight),
                      kCFNumberSInt32Type, &device_height);

    /* Determine the current screen size */
    this->info.current_w = device_width;
    this->info.current_h = device_height;

    /* Determine the default screen depth */
    video_format->BitsPerPixel = device_bpp;

    /* Set misc globals */
    current_grab_mode = SDL_GRAB_OFF;
    cursor_should_be_visible    = YES;
    cursor_visible              = YES;
    current_mods = 0;
    field_edit =  [[SDLTranslatorResponder alloc] initWithFrame:r];
    
    if ( Gestalt(gestaltSystemVersion, &system_version) != noErr )
        system_version = 0;
    
    /* register for sleep notifications so wake from sleep generates SDL_VIDEOEXPOSE */
    QZ_RegisterForSleepNotifications (this);
    
    /* Fill in some window manager capabilities */
    this->info.wm_available = 1;

    return 0;
}

static SDL_Rect** QZ_ListModes (_THIS, SDL_PixelFormat *format, Uint32 flags)
{
    CFIndex num_modes;
    CFIndex i;

    int list_size = 0;

    /* Any windowed mode is acceptable */
    if ( (flags & SDL_FULLSCREEN) == 0 )
        return (SDL_Rect**)-1;

    /* Free memory from previous call, if any */
    if ( client_mode_list != NULL ) {

        int i;

        for (i = 0; client_mode_list[i] != NULL; i++)
            SDL_free (client_mode_list[i]);

        SDL_free (client_mode_list);
        client_mode_list = NULL;
    }

    num_modes = CFArrayGetCount (mode_list);

    /* Build list of modes with the requested bpp */
    for (i = 0; i < num_modes; i++) {

        CFDictionaryRef onemode;
        CFNumberRef     number;
        int bpp;

        onemode = CFArrayGetValueAtIndex (mode_list, i);
        number = CFDictionaryGetValue (onemode, kCGDisplayBitsPerPixel);
        CFNumberGetValue (number, kCFNumberSInt32Type, &bpp);

        if (bpp == format->BitsPerPixel) {

            int intvalue;
            int hasMode;
            int width, height;

            number = CFDictionaryGetValue (onemode, kCGDisplayWidth);
            CFNumberGetValue (number, kCFNumberSInt32Type, &intvalue);
            width = (Uint16) intvalue;

            number = CFDictionaryGetValue (onemode, kCGDisplayHeight);
            CFNumberGetValue (number, kCFNumberSInt32Type, &intvalue);
            height = (Uint16) intvalue;

            /* Check if mode is already in the list */
            {
                int i;
                hasMode = SDL_FALSE;
                for (i = 0; i < list_size; i++) {
                    if (client_mode_list[i]->w == width && 
                        client_mode_list[i]->h == height) {
                        hasMode = SDL_TRUE;
                        break;
                    }
                }
            }

            /* Grow the list and add mode to the list */
            if ( ! hasMode ) {

                SDL_Rect *rect;

                list_size++;

                if (client_mode_list == NULL)
                    client_mode_list = (SDL_Rect**) 
                        SDL_malloc (sizeof(*client_mode_list) * (list_size+1) );
                else
                    client_mode_list = (SDL_Rect**) 
                        SDL_realloc (client_mode_list, sizeof(*client_mode_list) * (list_size+1));

                rect = (SDL_Rect*) SDL_malloc (sizeof(**client_mode_list));

                if (client_mode_list == NULL || rect == NULL) {
                    SDL_OutOfMemory ();
                    return NULL;
                }

                rect->x = rect->y = 0;
                rect->w = width;
                rect->h = height;

                client_mode_list[list_size-1] = rect;
                client_mode_list[list_size]   = NULL;
            }
        }
    }

    /* Sort list largest to smallest (by area) */
    {
        int i, j;
        for (i = 0; i < list_size; i++) {
            for (j = 0; j < list_size-1; j++) {

                int area1, area2;
                area1 = client_mode_list[j]->w * client_mode_list[j]->h;
                area2 = client_mode_list[j+1]->w * client_mode_list[j+1]->h;

                if (area1 < area2) {
                    SDL_Rect *tmp = client_mode_list[j];
                    client_mode_list[j] = client_mode_list[j+1];
                    client_mode_list[j+1] = tmp;
                }
            }
        }
    }
    return client_mode_list;
}

static SDL_bool QZ_WindowPosition(_THIS, int *x, int *y)
{
    const char *window = getenv("SDL_VIDEO_WINDOW_POS");
    if ( window ) {
        if ( sscanf(window, "%d,%d", x, y) == 2 ) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static void QZ_UnsetVideoMode (_THIS, BOOL to_desktop)
{
    /* Reset values that may change between switches */
    this->info.blit_fill  = 0;
    this->FillHWRect      = NULL;
    this->UpdateRects     = NULL;
    this->LockHWSurface   = NULL;
    this->UnlockHWSurface = NULL;
    
    if (cg_context) {
        CGContextFlush (cg_context);
        CGContextRelease (cg_context);
        cg_context = nil;
    }
    
    /* Release fullscreen resources */
    if ( mode_flags & SDL_FULLSCREEN ) {

        NSRect screen_rect;
        
        /*  Release double buffer stuff */
        if ( mode_flags & SDL_DOUBLEBUF) {
            quit_thread = YES;
            SDL_SemPost (sem1);
            SDL_WaitThread (thread, NULL);
            SDL_DestroySemaphore (sem1);
            SDL_DestroySemaphore (sem2);
            SDL_free (sw_buffers[0]);
        }
        
        /* If we still have a valid window, close it. */
        if ( qz_window ) {
            NSCAssert([ qz_window delegate ] == nil, @"full screen window shouldn't have a delegate"); /* if that should ever change, we'd have to release it here */
            [ qz_window close ]; /* includes release because [qz_window isReleasedWhenClosed] */
            qz_window = nil;
            window_view = nil;
        }
        /* 
            Release the OpenGL context
            Do this first to avoid trash on the display before fade
        */
        if ( mode_flags & SDL_OPENGL ) {
        
            QZ_TearDownOpenGL (this);
            CGLSetFullScreen (NULL);
        }
        if (to_desktop) {
            ShowMenuBar ();
            /* Restore original screen resolution/bpp */
            CGDisplaySwitchToMode (display_id, save_mode);
            CGReleaseAllDisplays ();
            /* 
                Reset the main screen's rectangle
                See comment in QZ_SetVideoFullscreen for why we do this
            */
            screen_rect = NSMakeRect(0,0,device_width,device_height);
            QZ_SetFrame([ NSScreen mainScreen ], screen_rect);
        }
    }
    /* Release window mode resources */
    else {
        id delegate = [ qz_window delegate ];
        [ qz_window close ]; /* includes release because [qz_window isReleasedWhenClosed] */
        if (delegate != nil) [ delegate release ];
        qz_window = nil;
        window_view = nil;

        /* Release the OpenGL context */
        if ( mode_flags & SDL_OPENGL )
            QZ_TearDownOpenGL (this);
    }

    /* Signal successful teardown */
    video_set = SDL_FALSE;
}

static SDL_Surface* QZ_SetVideoFullScreen (_THIS, SDL_Surface *current, int width,
                                           int height, int bpp, Uint32 flags)
{
    boolean_t exact_match = 0;
    NSRect screen_rect;
    CGError error;
    NSRect contentRect;
    CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;

    /* Fade to black to hide resolution-switching flicker (and garbage
       that is displayed by a destroyed OpenGL context, if applicable) */
    if ( CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess ) {
        CGDisplayFade (fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
    }
    
    /* Destroy any previous mode */
    if (video_set == SDL_TRUE)
        QZ_UnsetVideoMode (this, FALSE);

    /* Sorry, QuickDraw was ripped out. */
    if (getenv("SDL_NSWindowPointer") || getenv("SDL_NSQuickDrawViewPointer")) {
        SDL_SetError ("Embedded QuickDraw windows are no longer supported");
        goto ERR_NO_MATCH;
    }

    /* See if requested mode exists */
    mode = CGDisplayBestModeForParameters (display_id, bpp, width,
                                           height, &exact_match);

    /* Require an exact match to the requested mode */
    if ( ! exact_match ) {
        SDL_SetError ("Failed to find display resolution: %dx%dx%d", width, height, bpp);
        goto ERR_NO_MATCH;
    }

    /* Put up the blanking window (a window above all other windows) */
    if (getenv ("SDL_SINGLEDISPLAY"))
        error = CGDisplayCapture (display_id);
    else
        error = CGCaptureAllDisplays ();
        
    if ( CGDisplayNoErr != error ) {
        SDL_SetError ("Failed capturing display");
        goto ERR_NO_CAPTURE;
    }

    /* Do the physical switch */
    if ( CGDisplayNoErr != CGDisplaySwitchToMode (display_id, mode) ) {
        SDL_SetError ("Failed switching display resolution");
        goto ERR_NO_SWITCH;
    }

    current->pixels = (Uint32*) CGDisplayBaseAddress (display_id);
    current->pitch  = CGDisplayBytesPerRow (display_id);

    current->flags = 0;
    current->w = width;
    current->h = height;
    current->flags |= SDL_FULLSCREEN;
    current->flags |= SDL_HWSURFACE;
    current->flags |= SDL_PREALLOC;
    /* current->hwdata = (void *) CGDisplayGetDrawingContext (display_id); */

    this->UpdateRects     = QZ_DirectUpdate;
    this->LockHWSurface   = QZ_LockHWSurface;
    this->UnlockHWSurface = QZ_UnlockHWSurface;

    /* Setup double-buffer emulation */
    if ( flags & SDL_DOUBLEBUF ) {
        
        /*
            Setup a software backing store for reasonable results when
            double buffering is requested (since a single-buffered hardware
            surface looks hideous).
            
            The actual screen blit occurs in a separate thread to allow 
            other blitting while waiting on the VBL (and hence results in higher framerates).
        */
        this->LockHWSurface = NULL;
        this->UnlockHWSurface = NULL;
        this->UpdateRects = NULL;
        
        current->flags |= (SDL_HWSURFACE|SDL_DOUBLEBUF);
        this->UpdateRects = QZ_DoubleBufferUpdate;
        this->LockHWSurface = QZ_LockDoubleBuffer;
        this->UnlockHWSurface = QZ_UnlockDoubleBuffer;
        this->FlipHWSurface = QZ_FlipDoubleBuffer;

        current->pixels = SDL_malloc (current->pitch * current->h * 2);
        if (current->pixels == NULL) {
            SDL_OutOfMemory ();
            goto ERR_DOUBLEBUF;
        }
        
        sw_buffers[0] = current->pixels;
        sw_buffers[1] = (Uint8*)current->pixels + current->pitch * current->h;
        
        quit_thread = NO;
        sem1 = SDL_CreateSemaphore (0);
        sem2 = SDL_CreateSemaphore (1);
        thread = SDL_CreateThread ((int (*)(void *))QZ_ThreadFlip, this);
    }

    if ( CGDisplayCanSetPalette (display_id) )
        current->flags |= SDL_HWPALETTE;

    /* Check if we should recreate the window */
    if (qz_window == nil) {
        /* Manually create a window, avoids having a nib file resource */
        qz_window = [ [ SDL_QuartzWindow alloc ] 
            initWithContentRect:contentRect
                styleMask:0
                    backing:NSBackingStoreBuffered
                        defer:NO ];

        if (qz_window != nil) {
            [ qz_window setAcceptsMouseMovedEvents:YES ];
            [ qz_window setViewsNeedDisplay:NO ];
        }
    }
    /* We already have a window, just change its size */
    else {
        [ qz_window setContentSize:contentRect.size ];
        current->flags |= (SDL_NOFRAME|SDL_RESIZABLE) & mode_flags;
        [ window_view setFrameSize:contentRect.size ];
    }

    /* Setup OpenGL for a fullscreen context */
    if (flags & SDL_OPENGL) {

        CGLError err;
        CGLContextObj ctx;

        if ( ! QZ_SetupOpenGL (this, bpp, flags) ) {
            goto ERR_NO_GL;
        }

        /* Initialize the NSView and add it to our window.  The presence of a valid window and
           view allow the cursor to be changed whilst in fullscreen.*/
        window_view = [ [ NSView alloc ] initWithFrame:contentRect ];
        [ [ qz_window contentView ] addSubview:window_view ];	
        [ window_view release ];

        ctx = QZ_GetCGLContextObj (gl_context);
        err = CGLSetFullScreen (ctx);

        if (err) {
            SDL_SetError ("Error setting OpenGL fullscreen: %s", CGLErrorString(err));
            goto ERR_NO_GL;
        }

        [ gl_context makeCurrentContext];

        glClear (GL_COLOR_BUFFER_BIT);

        [ gl_context flushBuffer ];

        current->flags |= SDL_OPENGL;
    }

    /* If we don't hide menu bar, it will get events and interrupt the program */
    HideMenuBar ();

    /* Fade in again (asynchronously) */
    if ( fade_token != kCGDisplayFadeReservationInvalidToken ) {
        CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation(fade_token);
    }

    /* 
        There is a bug in Cocoa where NSScreen doesn't synchronize
        with CGDirectDisplay, so the main screen's frame is wrong.
        As a result, coordinate translation produces incorrect results.
        We can hack around this bug by setting the screen rect
        ourselves. This hack should be removed if/when the bug is fixed.
    */
    screen_rect = NSMakeRect(0,0,width,height);
    QZ_SetFrame([ NSScreen mainScreen ], screen_rect);

    /* Save the flags to ensure correct tear-down */
    mode_flags = current->flags;

    /* Set app state, hide cursor if necessary, ... */
    QZ_DoActivate(this);

    return current;

    /* Since the blanking window covers *all* windows (even force quit) correct recovery is crucial */
ERR_NO_GL:      
ERR_DOUBLEBUF:  CGDisplaySwitchToMode (display_id, save_mode);
ERR_NO_SWITCH:  CGReleaseAllDisplays ();
ERR_NO_CAPTURE:
ERR_NO_MATCH:   if ( fade_token != kCGDisplayFadeReservationInvalidToken ) {
                    CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
                    CGReleaseDisplayFadeReservation (fade_token);
                }
                return NULL;
}

static SDL_Surface* QZ_SetVideoWindowed (_THIS, SDL_Surface *current, int width,
                                         int height, int *bpp, Uint32 flags)
{
    unsigned int style;
    NSRect contentRect;
    int center_window = 1;
    int origin_x, origin_y;
    CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;

    current->flags = 0;
    current->w = width;
    current->h = height;
    
    contentRect = NSMakeRect (0, 0, width, height);

    /*
        Check if we should completely destroy the previous mode 
        - If it is fullscreen
        - If it has different noframe or resizable attribute
        - If it is OpenGL (since gl attributes could be different)
        - If new mode is OpenGL, but previous mode wasn't
    */
    if (video_set == SDL_TRUE) {
        if (mode_flags & SDL_FULLSCREEN) {
            /* Fade to black to hide resolution-switching flicker (and garbage
               that is displayed by a destroyed OpenGL context, if applicable) */
            if (CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess) {
                CGDisplayFade (fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
            }
            QZ_UnsetVideoMode (this, TRUE);
        }
        else if ( ((mode_flags ^ flags) & (SDL_NOFRAME|SDL_RESIZABLE)) ||
                  (mode_flags & SDL_OPENGL) || 
                  (flags & SDL_OPENGL) ) {
            QZ_UnsetVideoMode (this, TRUE);
        }
    }
    
    /* Sorry, QuickDraw was ripped out. */
    if (getenv("SDL_NSWindowPointer") || getenv("SDL_NSQuickDrawViewPointer")) {
        SDL_SetError ("Embedded QuickDraw windows are no longer supported");
        if (fade_token != kCGDisplayFadeReservationInvalidToken) {
            CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
            CGReleaseDisplayFadeReservation (fade_token);
        }
        return NULL;
    }

    /* Check if we should recreate the window */
    if (qz_window == nil) {
    
        /* Set the window style based on input flags */
        if ( flags & SDL_NOFRAME ) {
            style = NSBorderlessWindowMask;
            current->flags |= SDL_NOFRAME;
        } else {
            style = NSTitledWindowMask;
            style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
            if ( flags & SDL_RESIZABLE ) {
                style |= NSResizableWindowMask;
                current->flags |= SDL_RESIZABLE;
            }
        }

        /* Manually create a window, avoids having a nib file resource */
        qz_window = [ [ SDL_QuartzWindow alloc ] 
            initWithContentRect:contentRect
                styleMask:style 
                    backing:NSBackingStoreBuffered
                        defer:NO ];
                          
        if (qz_window == nil) {
            SDL_SetError ("Could not create the Cocoa window");
            if (fade_token != kCGDisplayFadeReservationInvalidToken) {
                CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
                CGReleaseDisplayFadeReservation (fade_token);
            }
            return NULL;
        }

        /*[ qz_window setReleasedWhenClosed:YES ];*/ /* no need to set this as it's the default for NSWindows */
        QZ_SetCaption(this, this->wm_title, this->wm_icon);
        [ qz_window setAcceptsMouseMovedEvents:YES ];
        [ qz_window setViewsNeedDisplay:NO ];

        if ( QZ_WindowPosition(this, &origin_x, &origin_y) ) {
            /* have to flip the Y value (NSPoint is lower left corner origin) */
            [ qz_window setFrameTopLeftPoint:NSMakePoint((float) origin_x, (float) (this->info.current_h - origin_y))];
            center_window = 0;
        } else if ( center_window ) {
            [ qz_window center ];
        }

        [ qz_window setDelegate:
            [ [ SDL_QuartzWindowDelegate alloc ] init ] ];
        [ qz_window setContentView: [ [ [ SDL_QuartzView alloc ] init ] autorelease ] ];
    }
    /* We already have a window, just change its size */
    else {
        [ qz_window setContentSize:contentRect.size ];
        current->flags |= (SDL_NOFRAME|SDL_RESIZABLE) & mode_flags;
        [ window_view setFrameSize:contentRect.size ];
    }

    /* For OpenGL, we bind the context to a subview */
    if ( flags & SDL_OPENGL ) {

        if ( ! QZ_SetupOpenGL (this, *bpp, flags) ) {
            if (fade_token != kCGDisplayFadeReservationInvalidToken) {
                CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
                CGReleaseDisplayFadeReservation (fade_token);
            }
            return NULL;
        }

        window_view = [ [ NSView alloc ] initWithFrame:contentRect ];
        [ window_view setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
        [ [ qz_window contentView ] addSubview:window_view ];
        [ gl_context setView: window_view ];
        [ window_view release ];
        [ gl_context makeCurrentContext];
        [ qz_window makeKeyAndOrderFront:nil ];
        current->flags |= SDL_OPENGL;
    }
    /* For 2D, we build a CGBitmapContext */
    else {
        CGColorSpaceRef cgColorspace;

        /* Only recreate the view if it doesn't already exist */
        if (window_view == nil) {
        
            window_view = [ [ NSView alloc ] initWithFrame:contentRect ];
            [ window_view setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
            [ [ qz_window contentView ] addSubview:window_view ];
            [ window_view release ];
            [ qz_window makeKeyAndOrderFront:nil ];
        }
        
        cgColorspace = CGColorSpaceCreateDeviceRGB();
        current->pitch = 4 * current->w;
        current->pixels = SDL_malloc (current->h * current->pitch);
        
        cg_context = CGBitmapContextCreate (current->pixels, current->w, current->h,
                        8, current->pitch, cgColorspace,
                        kCGImageAlphaNoneSkipFirst);
        CGColorSpaceRelease (cgColorspace);
        
        current->flags |= SDL_SWSURFACE;
        current->flags |= SDL_ASYNCBLIT;
        current->hwdata = (void *) cg_context;
        
        this->UpdateRects     = QZ_UpdateRects;
        this->LockHWSurface   = QZ_LockHWSurface;
        this->UnlockHWSurface = QZ_UnlockHWSurface;
    }

    /* Save flags to ensure correct teardown */
    mode_flags = current->flags;

    /* Fade in again (asynchronously) if we came from a fullscreen mode and faded to black */
    if (fade_token != kCGDisplayFadeReservationInvalidToken) {
        CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation (fade_token);
    }

    return current;
}

static SDL_Surface* QZ_SetVideoMode (_THIS, SDL_Surface *current, int width,
                                     int height, int bpp, Uint32 flags)
{
    current->flags = 0;
    current->pixels = NULL;

    /* Setup full screen video */
    if ( flags & SDL_FULLSCREEN ) {
        current = QZ_SetVideoFullScreen (this, current, width, height, bpp, flags );
        if (current == NULL)
            return NULL;
    }
    /* Setup windowed video */
    else {
        /* Force bpp to 32 */
        bpp = 32;
        current = QZ_SetVideoWindowed (this, current, width, height, &bpp, flags);
        if (current == NULL)
            return NULL;
    }

    /* Setup the new pixel format */
    {
        int amask = 0,
        rmask = 0,
        gmask = 0,
        bmask = 0;

        switch (bpp) {
            case 16:   /* (1)-5-5-5 RGB */
                amask = 0;
                rmask = 0x7C00;
                gmask = 0x03E0;
                bmask = 0x001F;
                break;
            case 24:
                SDL_SetError ("24bpp is not available");
                return NULL;
            case 32:   /* (8)-8-8-8 ARGB */
                amask = 0x00000000;
		if ( flags & SDL_FULLSCREEN )
		{
			rmask = 0x00FF0000;
			gmask = 0x0000FF00;
			bmask = 0x000000FF;
		}
		else
		{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
			rmask = 0x0000FF00;
			gmask = 0x00FF0000;
			bmask = 0xFF000000;
#else
			rmask = 0x00FF0000;
			gmask = 0x0000FF00;
			bmask = 0x000000FF;
#endif
		}
                break;
        }

        if ( ! SDL_ReallocFormat (current, bpp,
                                  rmask, gmask, bmask, amask ) ) {
            SDL_SetError ("Couldn't reallocate pixel format");
            return NULL;
        }
    }

    /* Signal successful completion (used internally) */
    video_set = SDL_TRUE;

    return current;
}

static int QZ_ToggleFullScreen (_THIS, int on)
{
    return 0;
}

static int QZ_SetColors (_THIS, int first_color, int num_colors,
                         SDL_Color *colors)
{
    CGTableCount  index;
    CGDeviceColor color;

    for (index = first_color; index < first_color+num_colors; index++) {

        /* Clamp colors between 0.0 and 1.0 */
        color.red   = colors->r / 255.0;
        color.blue  = colors->b / 255.0;
        color.green = colors->g / 255.0;

        colors++;

        CGPaletteSetColorAtIndex (palette, color, index);
    }

    if ( CGDisplayNoErr != CGDisplaySetPalette (display_id, palette) )
        return 0;

    return 1;
}

static int QZ_LockDoubleBuffer (_THIS, SDL_Surface *surface)
{
    return 1;
}

static void QZ_UnlockDoubleBuffer (_THIS, SDL_Surface *surface)
{
}

/* The VBL delay is based on code by Ian R Ollmann's RezLib <iano@cco.caltech.edu> */
static AbsoluteTime QZ_SecondsToAbsolute ( double seconds )
{
    union
    {
        UInt64 i;
        Nanoseconds ns;
    } temp;
        
    temp.i = seconds * 1000000000.0;
    
    return NanosecondsToAbsolute ( temp.ns );
}

static int QZ_ThreadFlip (_THIS)
{
    Uint8 *src, *dst;
    int skip, len, h;
    
    /*
        Give this thread the highest scheduling priority possible,
        in the hopes that it will immediately run after the VBL delay
    */
    {
        pthread_t current_thread;
        int policy;
        struct sched_param param;
        
        current_thread = pthread_self ();
        pthread_getschedparam (current_thread, &policy, &param);
        policy = SCHED_RR;
        param.sched_priority = sched_get_priority_max (policy);
        pthread_setschedparam (current_thread, policy, &param);
    }
    
    while (1) {
    
        SDL_SemWait (sem1);
        if (quit_thread)
            return 0;
                
        /*
         * We have to add SDL_VideoSurface->offset here, since we might be a
         *  smaller surface in the center of the framebuffer (you asked for
         *  a fullscreen resolution smaller than the hardware could supply
         *  so SDL is centering it in a bigger resolution)...
         */
        dst = (Uint8 *)CGDisplayBaseAddress (display_id) + SDL_VideoSurface->offset;
        src = current_buffer + SDL_VideoSurface->offset;
        len = SDL_VideoSurface->w * SDL_VideoSurface->format->BytesPerPixel;
        h = SDL_VideoSurface->h;
        skip = SDL_VideoSurface->pitch;
    
        /* Wait for the VBL to occur (estimated since we don't have a hardware interrupt) */
        {
            
            /* The VBL delay is based on Ian Ollmann's RezLib <iano@cco.caltech.edu> */
            double refreshRate;
            double linesPerSecond;
            double target;
            double position;
            double adjustment;
            AbsoluteTime nextTime;        
            CFNumberRef refreshRateCFNumber;
            
            refreshRateCFNumber = CFDictionaryGetValue (mode, kCGDisplayRefreshRate);
            if ( NULL == refreshRateCFNumber ) {
                SDL_SetError ("Mode has no refresh rate");
                goto ERROR;
            }
            
            if ( 0 == CFNumberGetValue (refreshRateCFNumber, kCFNumberDoubleType, &refreshRate) ) {
                SDL_SetError ("Error getting refresh rate");
                goto ERROR;
            }
            
            if ( 0 == refreshRate ) {
               
               SDL_SetError ("Display has no refresh rate, using 60hz");
                
                /* ok, for LCD's we'll emulate a 60hz refresh, which may or may not look right */
                refreshRate = 60.0;
            }
            
            linesPerSecond = refreshRate * h;
            target = h;
        
            /* Figure out the first delay so we start off about right */
            position = CGDisplayBeamPosition (display_id);
            if (position > target)
                position = 0;
            
            adjustment = (target - position) / linesPerSecond; 
            
            nextTime = AddAbsoluteToAbsolute (UpTime (), QZ_SecondsToAbsolute (adjustment));
        
            MPDelayUntil (&nextTime);
        }
        
        
        /* On error, skip VBL delay */
        ERROR:
        
        /* TODO: use CGContextDrawImage here too!  Create two CGContextRefs the same way we
           create two buffers, replace current_buffer with current_context and set it
           appropriately in QZ_FlipDoubleBuffer.  */
        while ( h-- ) {
        
            SDL_memcpy (dst, src, len);
            src += skip;
            dst += skip;
        }
        
        /* signal flip completion */
        SDL_SemPost (sem2);
    }
    
    return 0;
}
        
static int QZ_FlipDoubleBuffer (_THIS, SDL_Surface *surface)
{
    /* wait for previous flip to complete */
    SDL_SemWait (sem2);
    
    current_buffer = surface->pixels;
        
    if (surface->pixels == sw_buffers[0])
        surface->pixels = sw_buffers[1];
    else
        surface->pixels = sw_buffers[0];
    
    /* signal worker thread to do the flip */
    SDL_SemPost (sem1);
    
    return 0;
}

static void QZ_DoubleBufferUpdate (_THIS, int num_rects, SDL_Rect *rects)
{
    /* perform a flip if someone calls updaterects on a doublebuferred surface */
    this->FlipHWSurface (this, SDL_VideoSurface);
}

static void QZ_DirectUpdate (_THIS, int num_rects, SDL_Rect *rects)
{
#pragma unused(this,num_rects,rects)
}


/* Resize icon, BMP format */
static const unsigned char QZ_ResizeIcon[] = {
    0x42,0x4d,0x31,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x28,0x00,
    0x00,0x00,0x0d,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,
    0x00,0x00,0xfb,0x01,0x00,0x00,0x13,0x0b,0x00,0x00,0x13,0x0b,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0b,0xff,0xff,
    0xff,0xda,0xda,0xda,0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xda,0xda,0xda,
    0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xda,0xda,0xda,0x87,0x87,0x87,0xe8,
    0xe8,0xe8,0xff,0xff,0xff,0x0b,0xff,0xff,0xff,0xff,0xff,0xff,0xda,0xda,0xda,0x87,
    0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xda,0xda,0xda,0x87,0x87,0x87,0xe8,0xe8,
    0xe8,0xff,0xff,0xff,0xda,0xda,0xda,0x87,0x87,0x87,0xff,0xff,0xff,0x0b,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd5,0xd5,0xd5,0x87,0x87,0x87,0xe8,0xe8,0xe8,
    0xff,0xff,0xff,0xda,0xda,0xda,0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xda,
    0xda,0xda,0xff,0xff,0xff,0x0b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xd7,0xd7,0xd7,0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xda,0xda,
    0xda,0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xff,0xff,0xff,0x0b,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd7,0xd7,0xd7,
    0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xda,0xda,0xda,0x87,0x87,0x87,0xe8,
    0xe8,0xe8,0xff,0xff,0xff,0x0b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd7,0xd7,0xd7,0x87,0x87,0x87,0xe8,0xe8,
    0xe8,0xff,0xff,0xff,0xdc,0xdc,0xdc,0x87,0x87,0x87,0xff,0xff,0xff,0x0b,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xd9,0xd9,0xd9,0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xdc,
    0xdc,0xdc,0xff,0xff,0xff,0x0b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xdb,0xdb,
    0xdb,0x87,0x87,0x87,0xe8,0xe8,0xe8,0xff,0xff,0xff,0xff,0xff,0xff,0x0b,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xdb,0xdb,0xdb,0x87,0x87,0x87,0xe8,
    0xe8,0xe8,0xff,0xff,0xff,0x0b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xdc,0xdc,0xdc,0x87,0x87,0x87,0xff,0xff,0xff,0x0b,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xdc,
    0xdc,0xdc,0xff,0xff,0xff,0x0b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0b
};

static void QZ_DrawResizeIcon (_THIS)
{
    /* Check if we should draw the resize icon */
    if (SDL_VideoSurface->flags & SDL_RESIZABLE) {
    
        SDL_Rect icon_rect;
        
        /* Create the icon image */
        if (resize_icon == NULL) {
        
            SDL_RWops *rw;
            SDL_Surface *tmp;
            
            rw = SDL_RWFromConstMem (QZ_ResizeIcon, sizeof(QZ_ResizeIcon));
            tmp = SDL_LoadBMP_RW (rw, SDL_TRUE);
                                                            
            resize_icon = SDL_ConvertSurface (tmp, SDL_VideoSurface->format, SDL_SRCCOLORKEY);
            SDL_SetColorKey (resize_icon, SDL_SRCCOLORKEY, 0xFFFFFF);
            
            SDL_FreeSurface (tmp);
        }
            
        icon_rect.x = SDL_VideoSurface->w - 13;
        icon_rect.y = SDL_VideoSurface->h - 13;
        icon_rect.w = 13;
        icon_rect.h = 13;
            
        SDL_BlitSurface (resize_icon, NULL, SDL_VideoSurface, &icon_rect);
    }
}

static void QZ_UpdateRects (_THIS, int numRects, SDL_Rect *rects)
{
    if (SDL_VideoSurface->flags & SDL_OPENGLBLIT) {
        QZ_GL_SwapBuffers (this);
    }
    else if ( [ qz_window isMiniaturized ] ) {
    
        /* Do nothing if miniaturized */
    }
    
    else {
        CGContextRef cgc = (CGContextRef)
            [[NSGraphicsContext graphicsContextWithWindow: qz_window]
                graphicsPort];
        QZ_DrawResizeIcon (this);
        CGContextFlush (cg_context);
        CGImageRef image = CGBitmapContextCreateImage (cg_context);
        CGRect rectangle = CGRectMake (0,0,[window_view frame].size.width,[window_view frame].size.height);
        
        CGContextDrawImage (cgc, rectangle, image);
        CGImageRelease(image);
        CGContextFlush (cgc);
        CGContextRelease (cgc);
    }
}

static void QZ_VideoQuit (_THIS)
{
    CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;

    /* Restore gamma settings */
    CGDisplayRestoreColorSyncSettings ();

    /* Ensure the cursor will be visible and working when we quit */
    CGDisplayShowCursor (display_id);
    CGAssociateMouseAndMouseCursorPosition (1);
    
    if (mode_flags & SDL_FULLSCREEN) {
        /* Fade to black to hide resolution-switching flicker (and garbage
           that is displayed by a destroyed OpenGL context, if applicable) */
        if (CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess) {
            CGDisplayFade (fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
        }
        QZ_UnsetVideoMode (this, TRUE);
        if (fade_token != kCGDisplayFadeReservationInvalidToken) {
            CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
            CGReleaseDisplayFadeReservation (fade_token);
        }
    }
    else
        QZ_UnsetVideoMode (this, TRUE);
    
    CGPaletteRelease (palette);

    if (opengl_library) {
        SDL_UnloadObject(opengl_library);
        opengl_library = NULL;
    }
    this->gl_config.driver_loaded = 0;

    if (field_edit) {
        [field_edit release];
        field_edit = NULL;
    }
}

#if 0 /* Not used (apparently, it's really slow) */
static int  QZ_FillHWRect (_THIS, SDL_Surface *dst, SDL_Rect *rect, Uint32 color)
{
    CGSDisplayHWFill (display_id, rect->x, rect->y, rect->w, rect->h, color);

    return 0;
}
#endif

static int  QZ_LockHWSurface(_THIS, SDL_Surface *surface)
{
    return 1;
}

static void QZ_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
}

static int QZ_AllocHWSurface(_THIS, SDL_Surface *surface)
{
    return(-1); /* unallowed (no HWSURFACE support here). */
}

static void QZ_FreeHWSurface (_THIS, SDL_Surface *surface)
{
}

/*
 int QZ_FlipHWSurface (_THIS, SDL_Surface *surface) {
     return 0;
 }
 */

/* Gamma functions */
int QZ_SetGamma (_THIS, float red, float green, float blue)
{
    const CGGammaValue min = 0.0, max = 1.0;

    if (red == 0.0)
        red = FLT_MAX;
    else
        red = 1.0 / red;

    if (green == 0.0)
        green = FLT_MAX;
    else
        green = 1.0 / green;

    if (blue == 0.0)
        blue = FLT_MAX;
    else
        blue  = 1.0 / blue;

    if ( CGDisplayNoErr == CGSetDisplayTransferByFormula
         (display_id, min, max, red, min, max, green, min, max, blue) ) {

        return 0;
    }
    else {

        return -1;
    }
}

int QZ_GetGamma (_THIS, float *red, float *green, float *blue)
{
    CGGammaValue dummy;
    if ( CGDisplayNoErr == CGGetDisplayTransferByFormula
         (display_id, &dummy, &dummy, red,
          &dummy, &dummy, green, &dummy, &dummy, blue) )

        return 0;
    else
        return -1;
}

int QZ_SetGammaRamp (_THIS, Uint16 *ramp)
{
    const CGTableCount tableSize = 255;
    CGGammaValue redTable[tableSize];
    CGGammaValue greenTable[tableSize];
    CGGammaValue blueTable[tableSize];

    int i;

    /* Extract gamma values into separate tables, convert to floats between 0.0 and 1.0 */
    for (i = 0; i < 256; i++)
        redTable[i % 256] = ramp[i] / 65535.0;

    for (i=256; i < 512; i++)
        greenTable[i % 256] = ramp[i] / 65535.0;

    for (i=512; i < 768; i++)
        blueTable[i % 256] = ramp[i] / 65535.0;

    if ( CGDisplayNoErr == CGSetDisplayTransferByTable
         (display_id, tableSize, redTable, greenTable, blueTable) )
        return 0;
    else
        return -1;
}

int QZ_GetGammaRamp (_THIS, Uint16 *ramp)
{
    const CGTableCount tableSize = 255;
    CGGammaValue redTable[tableSize];
    CGGammaValue greenTable[tableSize];
    CGGammaValue blueTable[tableSize];
    CGTableCount actual;
    int i;

    if ( CGDisplayNoErr != CGGetDisplayTransferByTable
         (display_id, tableSize, redTable, greenTable, blueTable, &actual) ||
         actual != tableSize)

        return -1;

    /* Pack tables into one array, with values from 0 to 65535 */
    for (i = 0; i < 256; i++)
        ramp[i] = redTable[i % 256] * 65535.0;

    for (i=256; i < 512; i++)
        ramp[i] = greenTable[i % 256] * 65535.0;

    for (i=512; i < 768; i++)
        ramp[i] = blueTable[i % 256] * 65535.0;

    return 0;
}

