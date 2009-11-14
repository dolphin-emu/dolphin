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
 * \file SDL_mouse.h
 *
 * Include file for SDL mouse event handling
 */

#ifndef _SDL_mouse_h
#define _SDL_mouse_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct SDL_Cursor SDL_Cursor;   /* Implementation dependent */

/* Function prototypes */

/**
 * \fn int SDL_GetNumMice(void)
 *
 * \brief Get the number of mouse input devices available.
 *
 * \sa SDL_SelectMouse()
 */
extern DECLSPEC int SDLCALL SDL_GetNumMice(void);

/**
 * \fn char* SDL_GetMouseName(int index)
 *
 * \brief Gets the name of a mouse with the given index.
 *
 * \param index is the index of the mouse, which name is to be returned.
 *
 * \return the name of the mouse with the specified index
 */
extern DECLSPEC char *SDLCALL SDL_GetMouseName(int index);

/**
 * \fn int SDL_SelectMouse(int index)
 *
 * \brief Set the index of the currently selected mouse.
 *
 * \return The index of the previously selected mouse.
 *
 * \note You can query the currently selected mouse by passing an index of -1.
 *
 * \sa SDL_GetNumMice()
 */
extern DECLSPEC int SDLCALL SDL_SelectMouse(int index);

/**
 * \fn SDL_WindowID SDL_GetMouseFocusWindow(int index)
 *
 * \brief Get the window which currently has focus for the currently selected mouse.
 */
extern DECLSPEC SDL_WindowID SDLCALL SDL_GetMouseFocusWindow(int index);

/**
 * \fn int SDL_SetRelativeMouseMode(int index, SDL_bool enabled)
 *
 * \brief Set relative mouse mode for the currently selected mouse.
 *
 * \param enabled Whether or not to enable relative mode
 *
 * \return 0 on success, or -1 if relative mode is not supported.
 *
 * While the mouse is in relative mode, the cursor is hidden, and the
 * driver will try to report continuous motion in the current window.
 * Only relative motion events will be delivered, the mouse position
 * will not change.
 *
 * \note This function will flush any pending mouse motion.
 *
 * \sa SDL_GetRelativeMouseMode()
 */
extern DECLSPEC int SDLCALL SDL_SetRelativeMouseMode(int index,
                                                     SDL_bool enabled);

/**
 * \fn SDL_bool SDL_GetRelativeMouseMode(int index)
 *
 * \brief Query whether relative mouse mode is enabled for the currently selected mouse.
 *
 * \sa SDL_SetRelativeMouseMode()
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GetRelativeMouseMode(int index);

/**
 * \fn Uint8 SDL_GetMouseState(int index, int *x, int *y)
 *
 * \brief Retrieve the current state of the currently selected mouse.
 *
 * The current button state is returned as a button bitmask, which can
 * be tested using the SDL_BUTTON(X) macros, and x and y are set to the
 * mouse cursor position relative to the focus window for the currently
 * selected mouse.  You can pass NULL for either x or y.
 */
extern DECLSPEC Uint8 SDLCALL SDL_GetMouseState(int index, int *x, int *y);

/**
 * \fn Uint8 SDL_GetRelativeMouseState(int index, int *x, int *y)
 *
 * \brief Retrieve the state of the currently selected mouse.
 *
 * The current button state is returned as a button bitmask, which can
 * be tested using the SDL_BUTTON(X) macros, and x and y are set to the
 * mouse deltas since the last call to SDL_GetRelativeMouseState().
 */
extern DECLSPEC Uint8 SDLCALL SDL_GetRelativeMouseState(int index, int *x,
                                                        int *y);

/**
 * \fn void SDL_WarpMouseInWindow(SDL_WindowID windowID, int x, int y)
 *
 * \brief Moves the currently selected mouse to the given position within the window.
 *
 * \param windowID The window to move the mouse into, or 0 for the current mouse focus
 * \param x The x coordinate within the window
 * \param y The y coordinate within the window
 *
 * \note This function generates a mouse motion event
 */
extern DECLSPEC void SDLCALL SDL_WarpMouseInWindow(SDL_WindowID windowID,
                                                   int x, int y);

/**
 * \fn SDL_Cursor *SDL_CreateCursor (const Uint8 * data, const Uint8 * mask, int w, int h, int hot_x, int hot_y)
 *
 * \brief Create a cursor for the currently selected mouse, using the
 *        specified bitmap data and mask (in MSB format).
 *
 * The cursor width must be a multiple of 8 bits.
 *
 * The cursor is created in black and white according to the following:
 * data  mask    resulting pixel on screen
 *  0     1       White
 *  1     1       Black
 *  0     0       Transparent
 *  1     0       Inverted color if possible, black if not.
 *
 * \sa SDL_FreeCursor()
 */
extern DECLSPEC SDL_Cursor *SDLCALL SDL_CreateCursor(const Uint8 * data,
                                                     const Uint8 * mask,
                                                     int w, int h, int hot_x,
                                                     int hot_y);

/**
 * \fn void SDL_SetCursor(SDL_Cursor * cursor)
 *
 * \brief Set the active cursor for the currently selected mouse.
 *
 * \note The cursor must have been created for the selected mouse.
 */
extern DECLSPEC void SDLCALL SDL_SetCursor(SDL_Cursor * cursor);

/**
 * \fn SDL_Cursor *SDL_GetCursor(void)
 *
 * \brief Return the active cursor for the currently selected mouse.
 */
extern DECLSPEC SDL_Cursor *SDLCALL SDL_GetCursor(void);

/**
 * \fn void SDL_FreeCursor(SDL_Cursor * cursor)
 *
 * \brief Frees a cursor created with SDL_CreateCursor().
 *
 * \sa SDL_CreateCursor()
 */
extern DECLSPEC void SDLCALL SDL_FreeCursor(SDL_Cursor * cursor);

/**
 * \fn int SDL_ShowCursor(int toggle)
 *
 * \brief Toggle whether or not the cursor is shown for the currently selected mouse.
 *
 * \param toggle 1 to show the cursor, 0 to hide it, -1 to query the current state.
 *
 * \return 1 if the cursor is shown, or 0 if the cursor is hidden.
 */
extern DECLSPEC int SDLCALL SDL_ShowCursor(int toggle);

/* Used as a mask when testing buttons in buttonstate
   Button 1:	Left mouse button
   Button 2:	Middle mouse button
   Button 3:	Right mouse button
 */

/**
 * \fn int SDL_GetCursorsNumber(int index)
 *
 * \brief Gets the number of cursors a pointing device supports.
 * Useful for tablet users. Useful only under Windows.
 *
 * \param index is the index of the pointing device, which number of cursors we
 * want to receive.
 *
 * \return the number of cursors supported by the pointing device. On Windows
 * if a device is a tablet it returns a number >=1. Normal mice always return 1.
 * On Linux every device reports one cursor.
 */
extern DECLSPEC int SDLCALL SDL_GetCursorsNumber(int index);

/**
 * \fn int SDL_GetCurrentCursor(int index)
 *
 * \brief Returns the index of the current cursor used by a specific pointing
 * device. Useful only under Windows.
 *
 * \param index is the index of the pointing device, which cursor index we want
 * to receive.
 *
 * \return the index of the cursor currently used by a specific pointing device.
 * Always 0 under Linux. On Windows if the device isn't a tablet it returns 0.
 * If the device is the tablet it returns the cursor index.
 * 0 - stylus, 1 - eraser, 2 - cursor.
 */
extern DECLSPEC int SDLCALL SDL_GetCurrentCursor(int index);

#define SDL_BUTTON(X)		(1 << ((X)-1))
#define SDL_BUTTON_LEFT		1
#define SDL_BUTTON_MIDDLE	2
#define SDL_BUTTON_RIGHT	3
#define SDL_BUTTON_X1		4
#define SDL_BUTTON_X2		5
#define SDL_BUTTON_LMASK	SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK	SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK	SDL_BUTTON(SDL_BUTTON_RIGHT)
#define SDL_BUTTON_X1MASK	SDL_BUTTON(SDL_BUTTON_X1)
#define SDL_BUTTON_X2MASK	SDL_BUTTON(SDL_BUTTON_X2)


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_mouse_h */

/* vi: set ts=4 sw=4 expandtab: */
