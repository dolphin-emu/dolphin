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

/* Window event handling code for SDL */

#include "SDL_events.h"
#include "SDL_events_c.h"
#include "SDL_mouse_c.h"
#include "../video/SDL_sysvideo.h"

int
SDL_SendWindowEvent(SDL_WindowID windowID, Uint8 windowevent, int data1,
                    int data2)
{
    int posted;
    SDL_Window *window;

    window = SDL_GetWindowFromID(windowID);
    if (!window) {
        return 0;
    }
    switch (windowevent) {
    case SDL_WINDOWEVENT_SHOWN:
        if (window->flags & SDL_WINDOW_SHOWN) {
            return 0;
        }
        window->flags |= SDL_WINDOW_SHOWN;
        SDL_OnWindowShown(window);
        break;
    case SDL_WINDOWEVENT_HIDDEN:
        if (!(window->flags & SDL_WINDOW_SHOWN)) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_SHOWN;
        SDL_OnWindowHidden(window);
        break;
    case SDL_WINDOWEVENT_MOVED:
        if (window->flags & SDL_WINDOW_FULLSCREEN) {
            return 0;
        }
        if (data1 == SDL_WINDOWPOS_UNDEFINED) {
            data1 = window->x;
        }
        if (data2 == SDL_WINDOWPOS_UNDEFINED) {
            data2 = window->y;
        }
        if (data1 == window->x && data2 == window->y) {
            return 0;
        }
        window->x = data1;
        window->y = data2;
        break;
    case SDL_WINDOWEVENT_RESIZED:
        if (window->flags & SDL_WINDOW_FULLSCREEN) {
            return 0;
        }
        if (data1 == window->w && data2 == window->h) {
            return 0;
        }
        window->w = data1;
        window->h = data2;
        SDL_OnWindowResized(window);
        break;
    case SDL_WINDOWEVENT_MINIMIZED:
        if (window->flags & SDL_WINDOW_MINIMIZED) {
            return 0;
        }
        window->flags |= SDL_WINDOW_MINIMIZED;
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        if (window->flags & SDL_WINDOW_MAXIMIZED) {
            return 0;
        }
        window->flags |= SDL_WINDOW_MAXIMIZED;
        break;
    case SDL_WINDOWEVENT_RESTORED:
        if (!(window->flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))) {
            return 0;
        }
        window->flags &= ~(SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED);
        break;
    case SDL_WINDOWEVENT_ENTER:
        if (window->flags & SDL_WINDOW_MOUSE_FOCUS) {
            return 0;
        }
        window->flags |= SDL_WINDOW_MOUSE_FOCUS;
        break;
    case SDL_WINDOWEVENT_LEAVE:
        if (!(window->flags & SDL_WINDOW_MOUSE_FOCUS)) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_MOUSE_FOCUS;
        break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
            return 0;
        }
        window->flags |= SDL_WINDOW_INPUT_FOCUS;
        SDL_OnWindowFocusGained(window);
        break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
        if (!(window->flags & SDL_WINDOW_INPUT_FOCUS)) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_INPUT_FOCUS;
        SDL_OnWindowFocusLost(window);
        break;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_ProcessEvents[SDL_WINDOWEVENT] == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_WINDOWEVENT;
        event.window.event = windowevent;
        event.window.data1 = data1;
        event.window.data2 = data2;
        event.window.windowID = windowID;
        posted = (SDL_PushEvent(&event) > 0);
    }
    return (posted);
}

/* vi: set ts=4 sw=4 expandtab: */
