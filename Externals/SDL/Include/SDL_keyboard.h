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
 * \file SDL_keyboard.h
 *
 * Include file for SDL keyboard event handling
 */

#ifndef _SDL_keyboard_h
#define _SDL_keyboard_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_keysym.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/**
 * \struct SDL_keysym
 *
 * \brief The SDL keysym structure, used in key events.
 */
typedef struct SDL_keysym
{
    SDL_scancode scancode;      /**< SDL physical key code - see ::SDL_scancode for details */
    SDLKey sym;                 /**< SDL virtual key code - see ::SDLKey for details */
    Uint16 mod;                 /**< current key modifiers */
    Uint32 unicode;             /**< OBSOLETE, use SDL_TextInputEvent instead */
} SDL_keysym;

/* Function prototypes */

/**
 * \fn int SDL_GetNumKeyboards(void)
 *
 * \brief Get the number of keyboard input devices available.
 *
 * \sa SDL_SelectKeyboard()
 */
extern DECLSPEC int SDLCALL SDL_GetNumKeyboards(void);

/**
 * \fn int SDL_SelectKeyboard(int index)
 *
 * \brief Set the index of the currently selected keyboard.
 *
 * \return The index of the previously selected keyboard.
 *
 * \note You can query the currently selected keyboard by passing an index of -1.
 *
 * \sa SDL_GetNumKeyboards()
 */
extern DECLSPEC int SDLCALL SDL_SelectKeyboard(int index);

/**
 * \fn Uint8 *SDL_GetKeyboardState(int *numkeys)
 *
 * \brief Get a snapshot of the current state of the selected keyboard.
 *
 * \param numkeys if non-NULL, receives the length of the returned array.
 *
 * \return An array of key states. Indexes into this array are obtained by using ::SDL_scancode values.
 *
 * Example:
 * 	Uint8 *state = SDL_GetKeyboardState(NULL);
 *	if ( state[SDL_SCANCODE_RETURN)] ) ... <RETURN> is pressed.
 */
extern DECLSPEC Uint8 *SDLCALL SDL_GetKeyboardState(int *numkeys);

/**
 * \fn SDLMod SDL_GetModState(void)
 *
 * \brief Get the current key modifier state for the selected keyboard.
 */
extern DECLSPEC SDLMod SDLCALL SDL_GetModState(void);

/**
 * \fn void SDL_SetModState(SDLMod modstate)
 *
 * \brief Set the current key modifier state for the selected keyboard.
 *
 * \note This does not change the keyboard state, only the key modifier flags.
 */
extern DECLSPEC void SDLCALL SDL_SetModState(SDLMod modstate);

/**
 * \fn SDLKey SDL_GetKeyFromScancode(SDL_scancode scancode)
 * 
 * \brief Get the key code corresponding to the given scancode according to the current keyboard layout.
 *
 * See ::SDLKey for details.
 *
 * \sa SDL_GetKeyName()
 */
extern DECLSPEC SDLKey SDLCALL SDL_GetKeyFromScancode(SDL_scancode scancode);

/**
 * \fn SDL_scancode SDL_GetScancodeFromKey(SDLKey key)
 * 
 * \brief Get the scancode corresponding to the given key code according to the current keyboard layout.
 *
 * See ::SDL_scancode for details.
 *
 * \sa SDL_GetScancodeName()
 */
extern DECLSPEC SDL_scancode SDLCALL SDL_GetScancodeFromKey(SDLKey key);

/**
 * \fn const char *SDL_GetScancodeName(SDL_scancode scancode)
 * 
 * \brief Get a human-readable name for a scancode.
 *
 * \return A pointer to a UTF-8 string that stays valid at least until the next call to this function. If you need it around any longer, you must copy it.  If the scancode doesn't have a name, this function returns "".
 *
 * \sa SDL_scancode
 */
extern DECLSPEC const char *SDLCALL SDL_GetScancodeName(SDL_scancode
                                                        scancode);

/**
 * \fn const char *SDL_GetKeyName(SDLKey key)
 * 
 * \brief Get a human-readable name for a key.
 *
 * \return A pointer to a UTF-8 string that stays valid at least until the next call to this function. If you need it around any longer, you must copy it.  If the key doesn't have a name, this function returns "".
 *
 * \sa SDLKey
 */
extern DECLSPEC const char *SDLCALL SDL_GetKeyName(SDLKey key);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_keyboard_h */

/* vi: set ts=4 sw=4 expandtab: */
