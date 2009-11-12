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

#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_cocoavideo.h"

static __inline__ void ConvertNSRect(NSRect *r)
{
    /* FIXME: Cache the display used for this window */
    r->origin.y = CGDisplayPixelsHigh(kCGDirectMainDisplay) - r->origin.y - r->size.height;
}

@implementation Cocoa_WindowListener

- (void)listen:(SDL_WindowData *)data
{
    NSNotificationCenter *center;

    _data = data;

    center = [NSNotificationCenter defaultCenter];

    [_data->window setNextResponder:self];
    if ([_data->window delegate] != nil) {
        [center addObserver:self selector:@selector(windowDisExpose:) name:NSWindowDidExposeNotification object:_data->window];
        [center addObserver:self selector:@selector(windowDidMove:) name:NSWindowDidMoveNotification object:_data->window];
        [center addObserver:self selector:@selector(windowDidResize:) name:NSWindowDidResizeNotification object:_data->window];
        [center addObserver:self selector:@selector(windowDidMiniaturize:) name:NSWindowDidMiniaturizeNotification object:_data->window];
        [center addObserver:self selector:@selector(windowDidDeminiaturize:) name:NSWindowDidDeminiaturizeNotification object:_data->window];
        [center addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:_data->window];
        [center addObserver:self selector:@selector(windowDidResignKey:) name:NSWindowDidResignKeyNotification object:_data->window];
    } else {
        [_data->window setDelegate:self];
    }
    [center addObserver:self selector:@selector(windowDidHide:) name:NSApplicationDidHideNotification object:NSApp];
    [center addObserver:self selector:@selector(windowDidUnhide:) name:NSApplicationDidUnhideNotification object:NSApp];

    [_data->window setAcceptsMouseMovedEvents:YES];
}

- (void)close
{
    NSNotificationCenter *center;

    center = [NSNotificationCenter defaultCenter];

    [_data->window setNextResponder:nil];
    if ([_data->window delegate] != self) {
        [center removeObserver:self name:NSWindowDidExposeNotification object:_data->window];
        [center removeObserver:self name:NSWindowDidMoveNotification object:_data->window];
        [center removeObserver:self name:NSWindowDidResizeNotification object:_data->window];
        [center removeObserver:self name:NSWindowDidMiniaturizeNotification object:_data->window];
        [center removeObserver:self name:NSWindowDidDeminiaturizeNotification object:_data->window];
        [center removeObserver:self name:NSWindowDidBecomeKeyNotification object:_data->window];
        [center removeObserver:self name:NSWindowDidResignKeyNotification object:_data->window];
    } else {
        [_data->window setDelegate:nil];
    }
    [center removeObserver:self name:NSApplicationDidHideNotification object:NSApp];
    [center removeObserver:self name:NSApplicationDidUnhideNotification object:NSApp];
}

- (BOOL)windowShouldClose:(id)sender
{
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_CLOSE, 0, 0);
    return NO;
}

- (void)windowDidExpose:(NSNotification *)aNotification
{
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_EXPOSED, 0, 0);
}

- (void)windowDidMove:(NSNotification *)aNotification
{
    int x, y;
    NSRect rect = [_data->window contentRectForFrameRect:[_data->window frame]];
    ConvertNSRect(&rect);
    x = (int)rect.origin.x;
    y = (int)rect.origin.y;
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_MOVED, x, y);
}

- (void)windowDidResize:(NSNotification *)aNotification
{
    int w, h;
    NSRect rect = [_data->window contentRectForFrameRect:[_data->window frame]];
    w = (int)rect.size.width;
    h = (int)rect.size.height;
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_RESIZED, w, h);
}

- (void)windowDidMiniaturize:(NSNotification *)aNotification
{
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
}

- (void)windowDidDeminiaturize:(NSNotification *)aNotification
{
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_RESTORED, 0, 0);
}

- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
    int index;

    /* We're going to get keyboard events, since we're key. */
    index = _data->videodata->keyboard;
    SDL_SetKeyboardFocus(index, _data->windowID);
}

- (void)windowDidResignKey:(NSNotification *)aNotification
{
    int index;
    SDL_Mouse *mouse;

    /* Some other window will get mouse events, since we're not key. */
    index = _data->videodata->mouse;
    mouse = SDL_GetMouse(index);
    if (mouse->focus == _data->windowID) {
        SDL_SetMouseFocus(index, 0);
    }

    /* Some other window will get keyboard events, since we're not key. */
    index = _data->videodata->keyboard;
    SDL_SetKeyboardFocus(index, 0);
}

- (void)windowDidHide:(NSNotification *)aNotification
{
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_HIDDEN, 0, 0);
}

- (void)windowDidUnhide:(NSNotification *)aNotification
{
    SDL_SendWindowEvent(_data->windowID, SDL_WINDOWEVENT_SHOWN, 0, 0);
}

- (void)mouseDown:(NSEvent *)theEvent
{
    int index;
    int button;

    index = _data->videodata->mouse;
    switch ([theEvent buttonNumber]) {
    case 0:
        button = SDL_BUTTON_LEFT;
        break;
    case 1:
        button = SDL_BUTTON_RIGHT;
        break;
    case 2:
        button = SDL_BUTTON_MIDDLE;
        break;
    default:
        button = [theEvent buttonNumber];
        break;
    }
    SDL_SendMouseButton(index, SDL_PRESSED, button);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
    int index;
    int button;

    index = _data->videodata->mouse;
    switch ([theEvent buttonNumber]) {
    case 0:
        button = SDL_BUTTON_LEFT;
        break;
    case 1:
        button = SDL_BUTTON_RIGHT;
        break;
    case 2:
        button = SDL_BUTTON_MIDDLE;
        break;
    default:
        button = [theEvent buttonNumber];
        break;
    }
    SDL_SendMouseButton(index, SDL_RELEASED, button);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    SDL_Window *window = SDL_GetWindowFromID(_data->windowID);
    int index;
    SDL_Mouse *mouse;
    NSPoint point;
    NSRect rect;

    index = _data->videodata->mouse;
    mouse = SDL_GetMouse(index);

    point = [NSEvent mouseLocation];
    if ( (window->flags & SDL_WINDOW_FULLSCREEN) ) {
        rect.size.width = CGDisplayPixelsWide(kCGDirectMainDisplay);
        rect.size.height = CGDisplayPixelsHigh(kCGDirectMainDisplay);
        point.x = point.x - rect.origin.x;
        point.y = rect.size.height - point.y;
    } else {
        rect = [_data->window contentRectForFrameRect:[_data->window frame]];
        point.y = rect.size.height - (point.y - rect.origin.y);
    }
    if ( point.x < 0 || point.x >= rect.size.width ||
         point.y < 0 || point.y >= rect.size.height ) {
        if (mouse->focus != 0) {
            SDL_SetMouseFocus(index, 0);
        }
    } else {
        if (mouse->focus != _data->windowID) {
            SDL_SetMouseFocus(index, _data->windowID);
        }
        SDL_SendMouseMotion(index, 0, (int)point.x, (int)point.y, 0);
    }
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    [self mouseMoved:theEvent];
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    [self mouseMoved:theEvent];
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    [self mouseMoved:theEvent];
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    int index;

    index = _data->videodata->mouse;
    SDL_SendMouseWheel(index, (int)([theEvent deltaX]+0.9f), (int)([theEvent deltaY]+0.9f));
}

@end

@interface SDLWindow : NSWindow
/* These are needed for borderless/fullscreen windows */
- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;
@end

@implementation SDLWindow
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}
@end

static int
SetupWindowData(_THIS, SDL_Window * window, NSWindow *nswindow, SDL_bool created)
{
    NSAutoreleasePool *pool;
    SDL_VideoData *videodata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *data;

    /* Allocate the window data */
    data = (SDL_WindowData *) SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    data->windowID = window->id;
    data->window = nswindow;
    data->created = created;
    data->videodata = videodata;

    pool = [[NSAutoreleasePool alloc] init];

    /* Create an event listener for the window */
    data->listener = [[Cocoa_WindowListener alloc] init];
    [data->listener listen:data];

    /* Fill in the SDL window with the window data */
    {
        NSRect rect = [nswindow contentRectForFrameRect:[nswindow frame]];
        ConvertNSRect(&rect);
        window->x = (int)rect.origin.x;
        window->y = (int)rect.origin.y;
        window->w = (int)rect.size.width;
        window->h = (int)rect.size.height;
    }
    if ([nswindow isVisible]) {
        window->flags |= SDL_WINDOW_SHOWN;
    } else {
        window->flags &= ~SDL_WINDOW_SHOWN;
    }
    {
        unsigned int style = [nswindow styleMask];

        if ((style & ~NSResizableWindowMask) == NSBorderlessWindowMask) {
            window->flags |= SDL_WINDOW_BORDERLESS;
        } else {
            window->flags &= ~SDL_WINDOW_BORDERLESS;
        }
        if (style & NSResizableWindowMask) {
            window->flags |= SDL_WINDOW_RESIZABLE;
        } else {
            window->flags &= ~SDL_WINDOW_RESIZABLE;
        }
    }
    if ([nswindow isZoomed]) {
        window->flags |= SDL_WINDOW_MAXIMIZED;
    } else {
        window->flags &= ~SDL_WINDOW_MAXIMIZED;
    }
    if ([nswindow isMiniaturized]) {
        window->flags |= SDL_WINDOW_MINIMIZED;
    } else {
        window->flags &= ~SDL_WINDOW_MINIMIZED;
    }
    if ([nswindow isKeyWindow]) {
        int index = data->videodata->keyboard;
        window->flags |= SDL_WINDOW_INPUT_FOCUS;
        SDL_SetKeyboardFocus(index, data->windowID);

        if (window->flags & SDL_WINDOW_INPUT_GRABBED) {
            /* FIXME */
        }
    }

    /* All done! */
    [pool release];
    window->driverdata = data;
    return 0;
}

int
Cocoa_CreateWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool;
    NSWindow *nswindow;
    NSRect rect;
    unsigned int style;
    NSString *title;
    int status;

    pool = [[NSAutoreleasePool alloc] init];

    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->x == SDL_WINDOWPOS_CENTERED) {
        rect.origin.x = (CGDisplayPixelsWide(kCGDirectMainDisplay) - window->w) / 2;
    } else if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        rect.origin.x = 0;
    } else {
        rect.origin.x = window->x;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->y == SDL_WINDOWPOS_CENTERED) {
        rect.origin.y = (CGDisplayPixelsHigh(kCGDirectMainDisplay) - window->h) / 2;
    } else if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        rect.origin.y = 0;
    } else {
        rect.origin.y = window->y;
    }
    rect.size.width = window->w;
    rect.size.height = window->h;
    ConvertNSRect(&rect);

    if (window->flags & SDL_WINDOW_BORDERLESS) {
        style = NSBorderlessWindowMask;
    } else {
        style = (NSTitledWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask);
    }
    if (window->flags & SDL_WINDOW_RESIZABLE) {
        style |= NSResizableWindowMask;
    }

    nswindow = [[SDLWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:FALSE];

    [pool release];

    if (SetupWindowData(_this, window, nswindow, SDL_TRUE) < 0) {
        [nswindow release];
        return -1;
    }
    return 0;
}

int
Cocoa_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    NSAutoreleasePool *pool;
    NSWindow *nswindow = (NSWindow *) data;
    NSString *title;
    int status;

    pool = [[NSAutoreleasePool alloc] init];

    /* Query the title from the existing window */
    title = [nswindow title];
    if (title) {
        window->title = SDL_strdup([title UTF8String]);
    }

    [pool release];

    return SetupWindowData(_this, window, nswindow, SDL_FALSE);
}

void
Cocoa_SetWindowTitle(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;
    NSString *string;

    if(window->title) {
        string = [[NSString alloc] initWithUTF8String:window->title];
    } else {
        string = [[NSString alloc] init];
    }
    [nswindow setTitle:string];
    [string release];

    [pool release];
}

void
Cocoa_SetWindowPosition(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;
    NSRect rect;

    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->x == SDL_WINDOWPOS_CENTERED) {
        rect.origin.x = (CGDisplayPixelsWide(kCGDirectMainDisplay) - window->w) / 2;
    } else {
        rect.origin.x = window->x;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN)
        || window->y == SDL_WINDOWPOS_CENTERED) {
        rect.origin.y = (CGDisplayPixelsHigh(kCGDirectMainDisplay) - window->h) / 2;
    } else {
        rect.origin.y = window->y;
    }
    rect.size.width = window->w;
    rect.size.height = window->h;
    ConvertNSRect(&rect);
    rect = [nswindow frameRectForContentRect:rect];
    [nswindow setFrameOrigin:rect.origin];
    [pool release];
}

void
Cocoa_SetWindowSize(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;
    NSSize size;

    size.width = window->w;
    size.height = window->h;
    [nswindow setContentSize:size];
    [pool release];
}

void
Cocoa_ShowWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    if (![nswindow isMiniaturized]) {
        [nswindow makeKeyAndOrderFront:nil];
    }
    [pool release];
}

void
Cocoa_HideWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    [nswindow orderOut:nil];
    [pool release];
}

void
Cocoa_RaiseWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    [nswindow makeKeyAndOrderFront:nil];
    [pool release];
}

void
Cocoa_MaximizeWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    [nswindow zoom:nil];
    [pool release];
}

void
Cocoa_MinimizeWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    [nswindow miniaturize:nil];
    [pool release];
}

void
Cocoa_RestoreWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    if ([nswindow isMiniaturized]) {
        [nswindow deminiaturize:nil];
    } else if ([nswindow isZoomed]) {
        [nswindow zoom:nil];
    }
    [pool release];
}

void
Cocoa_SetWindowGrab(_THIS, SDL_Window * window)
{
    if ((window->flags & SDL_WINDOW_INPUT_GRABBED) &&
        (window->flags & SDL_WINDOW_INPUT_FOCUS)) {
        /* FIXME: Grab mouse */
    } else {
        /* FIXME: Release mouse */
    }
}

void
Cocoa_DestroyWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (data) {
        [data->listener close];
        [data->listener release];
        if (data->created) {
            [data->window close];
        }
        SDL_free(data);
    }
    [pool release];
}

SDL_bool
Cocoa_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    NSWindow *nswindow = ((SDL_WindowData *) window->driverdata)->window;

    if (info->version.major <= SDL_MAJOR_VERSION) {
        //info->window = nswindow;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

/* vi: set ts=4 sw=4 expandtab: */
