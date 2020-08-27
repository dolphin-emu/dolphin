/* Copyright (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_H__
#define LIBRETRO_H__

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#if defined(_MSC_VER) && _MSC_VER < 1800 && !defined(SN_TARGET_PS3)
/* Hack applied for MSVC when compiling in C89 mode
 * as it isn't C99-compliant. */
#define bool unsigned char
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#endif

#ifndef RETRO_CALLCONV
#  if defined(__GNUC__) && defined(__i386__) && !defined(__x86_64__)
#    define RETRO_CALLCONV __attribute__((cdecl))
#  elif defined(_MSC_VER) && defined(_M_X86) && !defined(_M_X64)
#    define RETRO_CALLCONV __cdecl
#  else
#    define RETRO_CALLCONV /* all other platforms only have one calling convention each */
#  endif
#endif

#ifndef RETRO_API
#  if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#    ifdef RETRO_IMPORT_SYMBOLS
#      ifdef __GNUC__
#        define RETRO_API RETRO_CALLCONV __attribute__((__dllimport__))
#      else
#        define RETRO_API RETRO_CALLCONV __declspec(dllimport)
#      endif
#    else
#      ifdef __GNUC__
#        define RETRO_API RETRO_CALLCONV __attribute__((__dllexport__))
#      else
#        define RETRO_API RETRO_CALLCONV __declspec(dllexport)
#      endif
#    endif
#  else
#      if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__CELLOS_LV2__)
#        define RETRO_API RETRO_CALLCONV __attribute__((__visibility__("default")))
#      else
#        define RETRO_API RETRO_CALLCONV
#      endif
#  endif
#endif

/* Used for checking API/ABI mismatches that can break libretro
 * implementations.
 * It is not incremented for compatible changes to the API.
 */
#define RETRO_API_VERSION         1

/*
 * Libretro's fundamental device abstractions.
 *
 * Libretro's input system consists of some standardized device types,
 * such as a joypad (with/without analog), mouse, keyboard, lightgun
 * and a pointer.
 *
 * The functionality of these devices are fixed, and individual cores
 * map their own concept of a controller to libretro's abstractions.
 * This makes it possible for frontends to map the abstract types to a
 * real input device, and not having to worry about binding input
 * correctly to arbitrary controller layouts.
 */

#define RETRO_DEVICE_TYPE_SHIFT         8
#define RETRO_DEVICE_MASK               ((1 << RETRO_DEVICE_TYPE_SHIFT) - 1)
#define RETRO_DEVICE_SUBCLASS(base, id) (((id + 1) << RETRO_DEVICE_TYPE_SHIFT) | base)

/* Input disabled. */
#define RETRO_DEVICE_NONE         0

/* The JOYPAD is called RetroPad. It is essentially a Super Nintendo
 * controller, but with additional L2/R2/L3/R3 buttons, similar to a
 * PS1 DualShock. */
#define RETRO_DEVICE_JOYPAD       1

/* The mouse is a simple mouse, similar to Super Nintendo's mouse.
 * X and Y coordinates are reported relatively to last poll (poll callback).
 * It is up to the libretro implementation to keep track of where the mouse
 * pointer is supposed to be on the screen.
 * The frontend must make sure not to interfere with its own hardware
 * mouse pointer.
 */
#define RETRO_DEVICE_MOUSE        2

/* KEYBOARD device lets one poll for raw key pressed.
 * It is poll based, so input callback will return with the current
 * pressed state.
 * For event/text based keyboard input, see
 * RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK.
 */
#define RETRO_DEVICE_KEYBOARD     3

/* LIGHTGUN device is similar to Guncon-2 for PlayStation 2.
 * It reports X/Y coordinates in screen space (similar to the pointer)
 * in the range [-0x8000, 0x7fff] in both axes, with zero being center and
 * -0x8000 being out of bounds.
 * As well as reporting on/off screen state. It features a trigger,
 * start/select buttons, auxiliary action buttons and a
 * directional pad. A forced off-screen shot can be requested for
 * auto-reloading function in some games.
 */
#define RETRO_DEVICE_LIGHTGUN     4

/* The ANALOG device is an extension to JOYPAD (RetroPad).
 * Similar to DualShock2 it adds two analog sticks and all buttons can
 * be analog. This is treated as a separate device type as it returns
 * axis values in the full analog range of [-0x7fff, 0x7fff],
 * although some devices may return -0x8000.
 * Positive X axis is right. Positive Y axis is down.
 * Buttons are returned in the range [0, 0x7fff].
 * Only use ANALOG type when polling for analog values.
 */
#define RETRO_DEVICE_ANALOG       5

/* Abstracts the concept of a pointing mechanism, e.g. touch.
 * This allows libretro to query in absolute coordinates where on the
 * screen a mouse (or something similar) is being placed.
 * For a touch centric device, coordinates reported are the coordinates
 * of the press.
 *
 * Coordinates in X and Y are reported as:
 * [-0x7fff, 0x7fff]: -0x7fff corresponds to the far left/top of the screen,
 * and 0x7fff corresponds to the far right/bottom of the screen.
 * The "screen" is here defined as area that is passed to the frontend and
 * later displayed on the monitor.
 *
 * The frontend is free to scale/resize this screen as it sees fit, however,
 * (X, Y) = (-0x7fff, -0x7fff) will correspond to the top-left pixel of the
 * game image, etc.
 *
 * To check if the pointer coordinates are valid (e.g. a touch display
 * actually being touched), PRESSED returns 1 or 0.
 *
 * If using a mouse on a desktop, PRESSED will usually correspond to the
 * left mouse button, but this is a frontend decision.
 * PRESSED will only return 1 if the pointer is inside the game screen.
 *
 * For multi-touch, the index variable can be used to successively query
 * more presses.
 * If index = 0 returns true for _PRESSED, coordinates can be extracted
 * with _X, _Y for index = 0. One can then query _PRESSED, _X, _Y with
 * index = 1, and so on.
 * Eventually _PRESSED will return false for an index. No further presses
 * are registered at this point. */
#define RETRO_DEVICE_POINTER      6

/* Buttons for the RetroPad (JOYPAD).
 * The placement of these is equivalent to placements on the
 * Super Nintendo controller.
 * L2/R2/L3/R3 buttons correspond to the PS1 DualShock.
 * Also used as id values for RETRO_DEVICE_INDEX_ANALOG_BUTTON */
#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_Y        1
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8
#define RETRO_DEVICE_ID_JOYPAD_X        9
#define RETRO_DEVICE_ID_JOYPAD_L       10
#define RETRO_DEVICE_ID_JOYPAD_R       11
#define RETRO_DEVICE_ID_JOYPAD_L2      12
#define RETRO_DEVICE_ID_JOYPAD_R2      13
#define RETRO_DEVICE_ID_JOYPAD_L3      14
#define RETRO_DEVICE_ID_JOYPAD_R3      15

#define RETRO_DEVICE_ID_JOYPAD_MASK    256

/* Index / Id values for ANALOG device. */
#define RETRO_DEVICE_INDEX_ANALOG_LEFT       0
#define RETRO_DEVICE_INDEX_ANALOG_RIGHT      1
#define RETRO_DEVICE_INDEX_ANALOG_BUTTON     2
#define RETRO_DEVICE_ID_ANALOG_X             0
#define RETRO_DEVICE_ID_ANALOG_Y             1

/* Id values for MOUSE. */
#define RETRO_DEVICE_ID_MOUSE_X                0
#define RETRO_DEVICE_ID_MOUSE_Y                1
#define RETRO_DEVICE_ID_MOUSE_LEFT             2
#define RETRO_DEVICE_ID_MOUSE_RIGHT            3
#define RETRO_DEVICE_ID_MOUSE_WHEELUP          4
#define RETRO_DEVICE_ID_MOUSE_WHEELDOWN        5
#define RETRO_DEVICE_ID_MOUSE_MIDDLE           6
#define RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP    7
#define RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN  8
#define RETRO_DEVICE_ID_MOUSE_BUTTON_4         9
#define RETRO_DEVICE_ID_MOUSE_BUTTON_5         10

/* Id values for LIGHTGUN. */
#define RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X        13 /*Absolute Position*/
#define RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y        14 /*Absolute*/
#define RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN    15 /*Status Check*/
#define RETRO_DEVICE_ID_LIGHTGUN_TRIGGER          2
#define RETRO_DEVICE_ID_LIGHTGUN_RELOAD          16 /*Forced off-screen shot*/
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_A            3
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_B            4
#define RETRO_DEVICE_ID_LIGHTGUN_START            6
#define RETRO_DEVICE_ID_LIGHTGUN_SELECT           7
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_C            8
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP          9
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN       10
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT       11
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT      12
/* deprecated */
#define RETRO_DEVICE_ID_LIGHTGUN_X                0 /*Relative Position*/
#define RETRO_DEVICE_ID_LIGHTGUN_Y                1 /*Relative*/
#define RETRO_DEVICE_ID_LIGHTGUN_CURSOR           3 /*Use Aux:A*/
#define RETRO_DEVICE_ID_LIGHTGUN_TURBO            4 /*Use Aux:B*/
#define RETRO_DEVICE_ID_LIGHTGUN_PAUSE            5 /*Use Start*/

/* Id values for POINTER. */
#define RETRO_DEVICE_ID_POINTER_X         0
#define RETRO_DEVICE_ID_POINTER_Y         1
#define RETRO_DEVICE_ID_POINTER_PRESSED   2
#define RETRO_DEVICE_ID_POINTER_COUNT     3

/* Returned from retro_get_region(). */
#define RETRO_REGION_NTSC  0
#define RETRO_REGION_PAL   1

/* Id values for LANGUAGE */
enum retro_language
{
   RETRO_LANGUAGE_ENGLISH             = 0,
   RETRO_LANGUAGE_JAPANESE            = 1,
   RETRO_LANGUAGE_FRENCH              = 2,
   RETRO_LANGUAGE_SPANISH             = 3,
   RETRO_LANGUAGE_GERMAN              = 4,
   RETRO_LANGUAGE_ITALIAN             = 5,
   RETRO_LANGUAGE_DUTCH               = 6,
   RETRO_LANGUAGE_PORTUGUESE_BRAZIL   = 7,
   RETRO_LANGUAGE_PORTUGUESE_PORTUGAL = 8,
   RETRO_LANGUAGE_RUSSIAN             = 9,
   RETRO_LANGUAGE_KOREAN              = 10,
   RETRO_LANGUAGE_CHINESE_TRADITIONAL = 11,
   RETRO_LANGUAGE_CHINESE_SIMPLIFIED  = 12,
   RETRO_LANGUAGE_ESPERANTO           = 13,
   RETRO_LANGUAGE_POLISH              = 14,
   RETRO_LANGUAGE_VIETNAMESE          = 15,
   RETRO_LANGUAGE_ARABIC              = 16,
   RETRO_LANGUAGE_GREEK               = 17,
   RETRO_LANGUAGE_TURKISH             = 18,
   RETRO_LANGUAGE_SLOVAK              = 19,
   RETRO_LANGUAGE_PERSIAN             = 20,
   RETRO_LANGUAGE_HEBREW              = 21,
   RETRO_LANGUAGE_ASTURIAN            = 22,
   RETRO_LANGUAGE_LAST,

   /* Ensure sizeof(enum) == sizeof(int) */
   RETRO_LANGUAGE_DUMMY          = INT_MAX
};

/* Passed to retro_get_memory_data/size().
 * If the memory type doesn't apply to the
 * implementation NULL/0 can be returned.
 */
#define RETRO_MEMORY_MASK        0xff

/* Regular save RAM. This RAM is usually found on a game cartridge,
 * backed up by a battery.
 * If save game data is too complex for a single memory buffer,
 * the SAVE_DIRECTORY (preferably) or SYSTEM_DIRECTORY environment
 * callback can be used. */
#define RETRO_MEMORY_SAVE_RAM    0

/* Some games have a built-in clock to keep track of time.
 * This memory is usually just a couple of bytes to keep track of time.
 */
#define RETRO_MEMORY_RTC         1

/* System ram lets a frontend peek into a game systems main RAM. */
#define RETRO_MEMORY_SYSTEM_RAM  2

/* Video ram lets a frontend peek into a game systems video RAM (VRAM). */
#define RETRO_MEMORY_VIDEO_RAM   3

/* Keysyms used for ID in input state callback when polling RETRO_KEYBOARD. */
enum retro_key
{
   RETROK_UNKNOWN        = 0,
   RETROK_FIRST          = 0,
   RETROK_BACKSPACE      = 8,
   RETROK_TAB            = 9,
   RETROK_CLEAR          = 12,
   RETROK_RETURN         = 13,
   RETROK_PAUSE          = 19,
   RETROK_ESCAPE         = 27,
   RETROK_SPACE          = 32,
   RETROK_EXCLAIM        = 33,
   RETROK_QUOTEDBL       = 34,
   RETROK_HASH           = 35,
   RETROK_DOLLAR         = 36,
   RETROK_AMPERSAND      = 38,
   RETROK_QUOTE          = 39,
   RETROK_LEFTPAREN      = 40,
   RETROK_RIGHTPAREN     = 41,
   RETROK_ASTERISK       = 42,
   RETROK_PLUS           = 43,
   RETROK_COMMA          = 44,
   RETROK_MINUS          = 45,
   RETROK_PERIOD         = 46,
   RETROK_SLASH          = 47,
   RETROK_0              = 48,
   RETROK_1              = 49,
   RETROK_2              = 50,
   RETROK_3              = 51,
   RETROK_4              = 52,
   RETROK_5              = 53,
   RETROK_6              = 54,
   RETROK_7              = 55,
   RETROK_8              = 56,
   RETROK_9              = 57,
   RETROK_COLON          = 58,
   RETROK_SEMICOLON      = 59,
   RETROK_LESS           = 60,
   RETROK_EQUALS         = 61,
   RETROK_GREATER        = 62,
   RETROK_QUESTION       = 63,
   RETROK_AT             = 64,
   RETROK_LEFTBRACKET    = 91,
   RETROK_BACKSLASH      = 92,
   RETROK_RIGHTBRACKET   = 93,
   RETROK_CARET          = 94,
   RETROK_UNDERSCORE     = 95,
   RETROK_BACKQUOTE      = 96,
   RETROK_a              = 97,
   RETROK_b              = 98,
   RETROK_c              = 99,
   RETROK_d              = 100,
   RETROK_e              = 101,
   RETROK_f              = 102,
   RETROK_g              = 103,
   RETROK_h              = 104,
   RETROK_i              = 105,
   RETROK_j              = 106,
   RETROK_k              = 107,
   RETROK_l              = 108,
   RETROK_m              = 109,
   RETROK_n              = 110,
   RETROK_o              = 111,
   RETROK_p              = 112,
   RETROK_q              = 113,
   RETROK_r              = 114,
   RETROK_s              = 115,
   RETROK_t              = 116,
   RETROK_u              = 117,
   RETROK_v              = 118,
   RETROK_w              = 119,
   RETROK_x              = 120,
   RETROK_y              = 121,
   RETROK_z              = 122,
   RETROK_LEFTBRACE      = 123,
   RETROK_BAR            = 124,
   RETROK_RIGHTBRACE     = 125,
   RETROK_TILDE          = 126,
   RETROK_DELETE         = 127,

   RETROK_KP0            = 256,
   RETROK_KP1            = 257,
   RETROK_KP2            = 258,
   RETROK_KP3            = 259,
   RETROK_KP4            = 260,
   RETROK_KP5            = 261,
   RETROK_KP6            = 262,
   RETROK_KP7            = 263,
   RETROK_KP8            = 264,
   RETROK_KP9            = 265,
   RETROK_KP_PERIOD      = 266,
   RETROK_KP_DIVIDE      = 267,
   RETROK_KP_MULTIPLY    = 268,
   RETROK_KP_MINUS       = 269,
   RETROK_KP_PLUS        = 270,
   RETROK_KP_ENTER       = 271,
   RETROK_KP_EQUALS      = 272,

   RETROK_UP             = 273,
   RETROK_DOWN           = 274,
   RETROK_RIGHT          = 275,
   RETROK_LEFT           = 276,
   RETROK_INSERT         = 277,
   RETROK_HOME           = 278,
   RETROK_END            = 279,
   RETROK_PAGEUP         = 280,
   RETROK_PAGEDOWN       = 281,

   RETROK_F1             = 282,
   RETROK_F2             = 283,
   RETROK_F3             = 284,
   RETROK_F4             = 285,
   RETROK_F5             = 286,
   RETROK_F6             = 287,
   RETROK_F7             = 288,
   RETROK_F8             = 289,
   RETROK_F9             = 290,
   RETROK_F10            = 291,
   RETROK_F11            = 292,
   RETROK_F12            = 293,
   RETROK_F13            = 294,
   RETROK_F14            = 295,
   RETROK_F15            = 296,

   RETROK_NUMLOCK        = 300,
   RETROK_CAPSLOCK       = 301,
   RETROK_SCROLLOCK      = 302,
   RETROK_RSHIFT         = 303,
   RETROK_LSHIFT         = 304,
   RETROK_RCTRL          = 305,
   RETROK_LCTRL          = 306,
   RETROK_RALT           = 307,
   RETROK_LALT           = 308,
   RETROK_RMETA          = 309,
   RETROK_LMETA          = 310,
   RETROK_LSUPER         = 311,
   RETROK_RSUPER         = 312,
   RETROK_MODE           = 313,
   RETROK_COMPOSE        = 314,

   RETROK_HELP           = 315,
   RETROK_PRINT          = 316,
   RETROK_SYSREQ         = 317,
   RETROK_BREAK          = 318,
   RETROK_MENU           = 319,
   RETROK_POWER          = 320,
   RETROK_EURO           = 321,
   RETROK_UNDO           = 322,
   RETROK_OEM_102        = 323,

   RETROK_LAST,

   RETROK_DUMMY          = INT_MAX /* Ensure sizeof(enum) == sizeof(int) */
};

enum retro_mod
{
   RETROKMOD_NONE       = 0x0000,

   RETROKMOD_SHIFT      = 0x01,
   RETROKMOD_CTRL       = 0x02,
   RETROKMOD_ALT        = 0x04,
   RETROKMOD_META       = 0x08,

   RETROKMOD_NUMLOCK    = 0x10,
   RETROKMOD_CAPSLOCK   = 0x20,
   RETROKMOD_SCROLLOCK  = 0x40,

   RETROKMOD_DUMMY = INT_MAX /* Ensure sizeof(enum) == sizeof(int) */
};

/* If set, this call is not part of the public libretro API yet. It can
 * change or be removed at any time. */
#define RETRO_ENVIRONMENT_EXPERIMENTAL 0x10000
/* Environment callback to be used internally in frontend. */
#define RETRO_ENVIRONMENT_PRIVATE 0x20000

/* Environment commands. */
#define RETRO_ENVIRONMENT_SET_ROTATION  1  /* const unsigned * --
                                            * Sets screen rotation of graphics.
                                            * Valid values are 0, 1, 2, 3, which rotates screen by 0, 90, 180,
                                            * 270 degrees counter-clockwise respectively.
                                            */
#define RETRO_ENVIRONMENT_GET_OVERSCAN  2  /* bool * --
                                            * NOTE: As of 2019 this callback is considered deprecated in favor of
                                            * using core options to manage overscan in a more nuanced, core-specific way.
                                            *
                                            * Boolean value whether or not the implementation should use overscan,
                                            * or crop away overscan.
                                            */
#define RETRO_ENVIRONMENT_GET_CAN_DUPE  3  /* bool * --
                                            * Boolean value whether or not frontend supports frame duping,
                                            * passing NULL to video frame callback.
                                            */

                                           /* Environ 4, 5 are no longer supported (GET_VARIABLE / SET_VARIABLES),
                                            * and reserved to avoid possible ABI clash.
                                            */

#define RETRO_ENVIRONMENT_SET_MESSAGE   6  /* const struct retro_message * --
                                            * Sets a message to be displayed in implementation-specific manner
                                            * for a certain amount of 'frames'.
                                            * Should not be used for trivial messages, which should simply be
                                            * logged via RETRO_ENVIRONMENT_GET_LOG_INTERFACE (or as a
                                            * fallback, stderr).
                                            */
#define RETRO_ENVIRONMENT_SHUTDOWN      7  /* N/A (NULL) --
                                            * Requests the frontend to shutdown.
                                            * Should only be used if game has a specific
                                            * way to shutdown the game from a menu item or similar.
                                            */
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL 8
                                           /* const unsigned * --
                                            * Gives a hint to the frontend how demanding this implementation
                                            * is on a system. E.g. reporting a level of 2 means
                                            * this implementation should run decently on all frontends
                                            * of level 2 and up.
                                            *
                                            * It can be used by the frontend to potentially warn
                                            * about too demanding implementations.
                                            *
                                            * The levels are "floating".
                                            *
                                            * This function can be called on a per-game basis,
                                            * as certain games an implementation can play might be
                                            * particularly demanding.
                                            * If called, it should be called in retro_load_game().
                                            */
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
                                           /* const char ** --
                                            * Returns the "system" directory of the frontend.
                                            * This directory can be used to store system specific
                                            * content such as BIOSes, configuration data, etc.
                                            * The returned value can be NULL.
                                            * If so, no such directory is defined,
                                            * and it's up to the implementation to find a suitable directory.
                                            *
                                            * NOTE: Some cores used this folder also for "save" data such as
                                            * memory cards, etc, for lack of a better place to put it.
                                            * This is now discouraged, and if possible, cores should try to
                                            * use the new GET_SAVE_DIRECTORY.
                                            */
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
                                           /* const enum retro_pixel_format * --
                                            * Sets the internal pixel format used by the implementation.
                                            * The default pixel format is RETRO_PIXEL_FORMAT_0RGB1555.
                                            * This pixel format however, is deprecated (see enum retro_pixel_format).
                                            * If the call returns false, the frontend does not support this pixel
                                            * format.
                                            *
                                            * This function should be called inside retro_load_game() or
                                            * retro_get_system_av_info().
                                            */
#define RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS 11
                                           /* const struct retro_input_descriptor * --
                                            * Sets an array of retro_input_descriptors.
                                            * It is up to the frontend to present this in a usable way.
                                            * The array is terminated by retro_input_descriptor::description
                                            * being set to NULL.
                                            * This function can be called at any time, but it is recommended
                                            * to call it as early as possible.
                                            */
#define RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK 12
                                           /* const struct retro_keyboard_callback * --
                                            * Sets a callback function used to notify core about keyboard events.
                                            */
#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE 13
                                           /* const struct retro_disk_control_callback * --
                                            * Sets an interface which frontend can use to eject and insert
                                            * disk images.
                                            * This is used for games which consist of multiple images and
                                            * must be manually swapped out by the user (e.g. PSX).
                                            */
#define RETRO_ENVIRONMENT_SET_HW_RENDER 14
                                           /* struct retro_hw_render_callback * --
                                            * Sets an interface to let a libretro core render with
                                            * hardware acceleration.
                                            * Should be called in retro_load_game().
                                            * If successful, libretro cores will be able to render to a
                                            * frontend-provided framebuffer.
                                            * The size of this framebuffer will be at least as large as
                                            * max_width/max_height provided in get_av_info().
                                            * If HW rendering is used, pass only RETRO_HW_FRAME_BUFFER_VALID or
                                            * NULL to retro_video_refresh_t.
                                            */
#define RETRO_ENVIRONMENT_GET_VARIABLE 15
                                           /* struct retro_variable * --
                                            * Interface to acquire user-defined information from environment
                                            * that cannot feasibly be supported in a multi-system way.
                                            * 'key' should be set to a key which has already been set by
                                            * SET_VARIABLES.
                                            * 'data' will be set to a value or NULL.
                                            */
#define RETRO_ENVIRONMENT_SET_VARIABLES 16
                                           /* const struct retro_variable * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterward it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * 'data' points to an array of retro_variable structs
                                            * terminated by a { NULL, NULL } element.
                                            * retro_variable::key should be namespaced to not collide
                                            * with other implementations' keys. E.g. A core called
                                            * 'foo' should use keys named as 'foo_option'.
                                            * retro_variable::value should contain a human readable
                                            * description of the key as well as a '|' delimited list
                                            * of expected values.
                                            *
                                            * The number of possible options should be very limited,
                                            * i.e. it should be feasible to cycle through options
                                            * without a keyboard.
                                            *
                                            * First entry should be treated as a default.
                                            *
                                            * Example entry:
                                            * { "foo_option", "Speed hack coprocessor X; false|true" }
                                            *
                                            * Text before first ';' is description. This ';' must be
                                            * followed by a space, and followed by a list of possible
                                            * values split up with '|'.
                                            *
                                            * Only strings are operated on. The possible values will
                                            * generally be displayed and stored as-is by the frontend.
                                            */
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE 17
                                           /* bool * --
                                            * Result is set to true if some variables are updated by
                                            * frontend since last call to RETRO_ENVIRONMENT_GET_VARIABLE.
                                            * Variables should be queried with GET_VARIABLE.
                                            */
#define RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME 18
                                           /* const bool * --
                                            * If true, the libretro implementation supports calls to
                                            * retro_load_game() with NULL as argument.
                                            * Used by cores which can run without particular game data.
                                            * This should be called within retro_set_environment() only.
                                            */
#define RETRO_ENVIRONMENT_GET_LIBRETRO_PATH 19
                                           /* const char ** --
                                            * Retrieves the absolute path from where this libretro
                                            * implementation was loaded.
                                            * NULL is returned if the libretro was loaded statically
                                            * (i.e. linked statically to frontend), or if the path cannot be
                                            * determined.
                                            * Mostly useful in cooperation with SET_SUPPORT_NO_GAME as assets can
                                            * be loaded without ugly hacks.
                                            */

                                           /* Environment 20 was an obsolete version of SET_AUDIO_CALLBACK.
                                            * It was not used by any known core at the time,
                                            * and was removed from the API. */
#define RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK 21
                                           /* const struct retro_frame_time_callback * --
                                            * Lets the core know how much time has passed since last
                                            * invocation of retro_run().
                                            * The frontend can tamper with the timing to fake fast-forward,
                                            * slow-motion, frame stepping, etc.
                                            * In this case the delta time will use the reference value
                                            * in frame_time_callback..
                                            */
#define RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK 22
                                           /* const struct retro_audio_callback * --
                                            * Sets an interface which is used to notify a libretro core about audio
                                            * being available for writing.
                                            * The callback can be called from any thread, so a core using this must
                                            * have a thread safe audio implementation.
                                            * It is intended for games where audio and video are completely
                                            * asynchronous and audio can be generated on the fly.
                                            * This interface is not recommended for use with emulators which have
                                            * highly synchronous audio.
                                            *
                                            * The callback only notifies about writability; the libretro core still
                                            * has to call the normal audio callbacks
                                            * to write audio. The audio callbacks must be called from within the
                                            * notification callback.
                                            * The amount of audio data to write is up to the implementation.
                                            * Generally, the audio callback will be called continously in a loop.
                                            *
                                            * Due to thread safety guarantees and lack of sync between audio and
                                            * video, a frontend  can selectively disallow this interface based on
                                            * internal configuration. A core using this interface must also
                                            * implement the "normal" audio interface.
                                            *
                                            * A libretro core using SET_AUDIO_CALLBACK should also make use of
                                            * SET_FRAME_TIME_CALLBACK.
                                            */
#define RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE 23
                                           /* struct retro_rumble_interface * --
                                            * Gets an interface which is used by a libretro core to set
                                            * state of rumble motors in controllers.
                                            * A strong and weak motor is supported, and they can be
                                            * controlled indepedently.
                                            */
#define RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES 24
                                           /* uint64_t * --
                                            * Gets a bitmask telling which device type are expected to be
                                            * handled properly in a call to retro_input_state_t.
                                            * Devices which are not handled or recognized always return
                                            * 0 in retro_input_state_t.
                                            * Example bitmask: caps = (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG).
                                            * Should only be called in retro_run().
                                            */
#define RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE (25 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_sensor_interface * --
                                            * Gets access to the sensor interface.
                                            * The purpose of this interface is to allow
                                            * setting state related to sensors such as polling rate,
                                            * enabling/disable it entirely, etc.
                                            * Reading sensor state is done via the normal
                                            * input_state_callback API.
                                            */
#define RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE (26 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_camera_callback * --
                                            * Gets an interface to a video camera driver.
                                            * A libretro core can use this interface to get access to a
                                            * video camera.
                                            * New video frames are delivered in a callback in same
                                            * thread as retro_run().
                                            *
                                            * GET_CAMERA_INTERFACE should be called in retro_load_game().
                                            *
                                            * Depending on the camera implementation used, camera frames
                                            * will be delivered as a raw framebuffer,
                                            * or as an OpenGL texture directly.
                                            *
                                            * The core has to tell the frontend here which types of
                                            * buffers can be handled properly.
                                            * An OpenGL texture can only be handled when using a
                                            * libretro GL core (SET_HW_RENDER).
                                            * It is recommended to use a libretro GL core when
                                            * using camera interface.
                                            *
                                            * The camera is not started automatically. The retrieved start/stop
                                            * functions must be used to explicitly
                                            * start and stop the camera driver.
                                            */
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
                                           /* struct retro_log_callback * --
                                            * Gets an interface for logging. This is useful for
                                            * logging in a cross-platform way
                                            * as certain platforms cannot use stderr for logging.
                                            * It also allows the frontend to
                                            * show logging information in a more suitable way.
                                            * If this interface is not used, libretro cores should
                                            * log to stderr as desired.
                                            */
#define RETRO_ENVIRONMENT_GET_PERF_INTERFACE 28
                                           /* struct retro_perf_callback * --
                                            * Gets an interface for performance counters. This is useful
                                            * for performance logging in a cross-platform way and for detecting
                                            * architecture-specific features, such as SIMD support.
                                            */
#define RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE 29
                                           /* struct retro_location_callback * --
                                            * Gets access to the location interface.
                                            * The purpose of this interface is to be able to retrieve
                                            * location-based information from the host device,
                                            * such as current latitude / longitude.
                                            */
#define RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY 30 /* Old name, kept for compatibility. */
#define RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY 30
                                           /* const char ** --
                                            * Returns the "core assets" directory of the frontend.
                                            * This directory can be used to store specific assets that the
                                            * core relies upon, such as art assets,
                                            * input data, etc etc.
                                            * The returned value can be NULL.
                                            * If so, no such directory is defined,
                                            * and it's up to the implementation to find a suitable directory.
                                            */
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31
                                           /* const char ** --
                                            * Returns the "save" directory of the frontend, unless there is no
                                            * save directory available. The save directory should be used to
                                            * store SRAM, memory cards, high scores, etc, if the libretro core
                                            * cannot use the regular memory interface (retro_get_memory_data()).
                                            *
                                            * If the frontend cannot designate a save directory, it will return
                                            * NULL to indicate that the core should attempt to operate without a
                                            * save directory set.
                                            *
                                            * NOTE: early libretro cores used the system directory for save
                                            * files. Cores that need to be backwards-compatible can still check
                                            * GET_SYSTEM_DIRECTORY.
                                            */
#define RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO 32
                                           /* const struct retro_system_av_info * --
                                            * Sets a new av_info structure. This can only be called from
                                            * within retro_run().
                                            * This should *only* be used if the core is completely altering the
                                            * internal resolutions, aspect ratios, timings, sampling rate, etc.
                                            * Calling this can require a full reinitialization of video/audio
                                            * drivers in the frontend,
                                            *
                                            * so it is important to call it very sparingly, and usually only with
                                            * the users explicit consent.
                                            * An eventual driver reinitialize will happen so that video and
                                            * audio callbacks
                                            * happening after this call within the same retro_run() call will
                                            * target the newly initialized driver.
                                            *
                                            * This callback makes it possible to support configurable resolutions
                                            * in games, which can be useful to
                                            * avoid setting the "worst case" in max_width/max_height.
                                            *
                                            * ***HIGHLY RECOMMENDED*** Do not call this callback every time
                                            * resolution changes in an emulator core if it's
                                            * expected to be a temporary change, for the reasons of possible
                                            * driver reinitialization.
                                            * This call is not a free pass for not trying to provide
                                            * correct values in retro_get_system_av_info(). If you need to change
                                            * things like aspect ratio or nominal width/height,
                                            * use RETRO_ENVIRONMENT_SET_GEOMETRY, which is a softer variant
                                            * of SET_SYSTEM_AV_INFO.
                                            *
                                            * If this returns false, the frontend does not acknowledge a
                                            * changed av_info struct.
                                            */
#define RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK 33
                                           /* const struct retro_get_proc_address_interface * --
                                            * Allows a libretro core to announce support for the
                                            * get_proc_address() interface.
                                            * This interface allows for a standard way to extend libretro where
                                            * use of environment calls are too indirect,
                                            * e.g. for cases where the frontend wants to call directly into the core.
                                            *
                                            * If a core wants to expose this interface, SET_PROC_ADDRESS_CALLBACK
                                            * **MUST** be called from within retro_set_environment().
                                            */
#define RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO 34
                                           /* const struct retro_subsystem_info * --
                                            * This environment call introduces the concept of libretro "subsystems".
                                            * A subsystem is a variant of a libretro core which supports
                                            * different kinds of games.
                                            * The purpose of this is to support e.g. emulators which might
                                            * have special needs, e.g. Super Nintendo's Super GameBoy, Sufami Turbo.
                                            * It can also be used to pick among subsystems in an explicit way
                                            * if the libretro implementation is a multi-system emulator itself.
                                            *
                                            * Loading a game via a subsystem is done with retro_load_game_special(),
                                            * and this environment call allows a libretro core to expose which
                                            * subsystems are supported for use with retro_load_game_special().
                                            * A core passes an array of retro_game_special_info which is terminated
                                            * with a zeroed out retro_game_special_info struct.
                                            *
                                            * If a core wants to use this functionality, SET_SUBSYSTEM_INFO
                                            * **MUST** be called from within retro_set_environment().
                                            */
#define RETRO_ENVIRONMENT_SET_CONTROLLER_INFO 35
                                           /* const struct retro_controller_info * --
                                            * This environment call lets a libretro core tell the frontend
                                            * which controller subclasses are recognized in calls to
                                            * retro_set_controller_port_device().
                                            *
                                            * Some emulators such as Super Nintendo support multiple lightgun
                                            * types which must be specifically selected from. It is therefore
                                            * sometimes necessary for a frontend to be able to tell the core
                                            * about a special kind of input device which is not specifcally
                                            * provided by the Libretro API.
                                            *
                                            * In order for a frontend to understand the workings of those devices,
                                            * they must be defined as a specialized subclass of the generic device
                                            * types already defined in the libretro API.
                                            *
                                            * The core must pass an array of const struct retro_controller_info which
                                            * is terminated with a blanked out struct. Each element of the
                                            * retro_controller_info struct corresponds to the ascending port index
                                            * that is passed to retro_set_controller_port_device() when that function
                                            * is called to indicate to the core that the frontend has changed the
                                            * active device subclass. SEE ALSO: retro_set_controller_port_device()
                                            *
                                            * The ascending input port indexes provided by the core in the struct
                                            * are generally presented by frontends as ascending User # or Player #,
                                            * such as Player 1, Player 2, Player 3, etc. Which device subclasses are
                                            * supported can vary per input port.
                                            *
                                            * The first inner element of each entry in the retro_controller_info array
                                            * is a retro_controller_description struct that specifies the names and
                                            * codes of all device subclasses that are available for the corresponding
                                            * User or Player, beginning with the generic Libretro device that the
                                            * subclasses are derived from. The second inner element of each entry is the
                                            * total number of subclasses that are listed in the retro_controller_description.
                                            *
                                            * NOTE: Even if special device types are set in the libretro core,
                                            * libretro should only poll input based on the base input device types.
                                            */
#define RETRO_ENVIRONMENT_SET_MEMORY_MAPS (36 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const struct retro_memory_map * --
                                            * This environment call lets a libretro core tell the frontend
                                            * about the memory maps this core emulates.
                                            * This can be used to implement, for example, cheats in a core-agnostic way.
                                            *
                                            * Should only be used by emulators; it doesn't make much sense for
                                            * anything else.
                                            * It is recommended to expose all relevant pointers through
                                            * retro_get_memory_* as well.
                                            *
                                            * Can be called from retro_init and retro_load_game.
                                            */
#define RETRO_ENVIRONMENT_SET_GEOMETRY 37
                                           /* const struct retro_game_geometry * --
                                            * This environment call is similar to SET_SYSTEM_AV_INFO for changing
                                            * video parameters, but provides a guarantee that drivers will not be
                                            * reinitialized.
                                            * This can only be called from within retro_run().
                                            *
                                            * The purpose of this call is to allow a core to alter nominal
                                            * width/heights as well as aspect ratios on-the-fly, which can be
                                            * useful for some emulators to change in run-time.
                                            *
                                            * max_width/max_height arguments are ignored and cannot be changed
                                            * with this call as this could potentially require a reinitialization or a
                                            * non-constant time operation.
                                            * If max_width/max_height are to be changed, SET_SYSTEM_AV_INFO is required.
                                            *
                                            * A frontend must guarantee that this environment call completes in
                                            * constant time.
                                            */
#define RETRO_ENVIRONMENT_GET_USERNAME 38
                                           /* const char **
                                            * Returns the specified username of the frontend, if specified by the user.
                                            * This username can be used as a nickname for a core that has online facilities
                                            * or any other mode where personalization of the user is desirable.
                                            * The returned value can be NULL.
                                            * If this environ callback is used by a core that requires a valid username,
                                            * a default username should be specified by the core.
                                            */
#define RETRO_ENVIRONMENT_GET_LANGUAGE 39
                                           /* unsigned * --
                                            * Returns the specified language of the frontend, if specified by the user.
                                            * It can be used by the core for localization purposes.
                                            */
#define RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER (40 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_framebuffer * --
                                            * Returns a preallocated framebuffer which the core can use for rendering
                                            * the frame into when not using SET_HW_RENDER.
                                            * The framebuffer returned from this call must not be used
                                            * after the current call to retro_run() returns.
                                            *
                                            * The goal of this call is to allow zero-copy behavior where a core
                                            * can render directly into video memory, avoiding extra bandwidth cost by copying
                                            * memory from core to video memory.
                                            *
                                            * If this call succeeds and the core renders into it,
                                            * the framebuffer pointer and pitch can be passed to retro_video_refresh_t.
                                            * If the buffer from GET_CURRENT_SOFTWARE_FRAMEBUFFER is to be used,
                                            * the core must pass the exact
                                            * same pointer as returned by GET_CURRENT_SOFTWARE_FRAMEBUFFER;
                                            * i.e. passing a pointer which is offset from the
                                            * buffer is undefined. The width, height and pitch parameters
                                            * must also match exactly to the values obtained from GET_CURRENT_SOFTWARE_FRAMEBUFFER.
                                            *
                                            * It is possible for a frontend to return a different pixel format
                                            * than the one used in SET_PIXEL_FORMAT. This can happen if the frontend
                                            * needs to perform conversion.
                                            *
                                            * It is still valid for a core to render to a different buffer
                                            * even if GET_CURRENT_SOFTWARE_FRAMEBUFFER succeeds.
                                            *
                                            * A frontend must make sure that the pointer obtained from this function is
                                            * writeable (and readable).
                                            */
#define RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE (41 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const struct retro_hw_render_interface ** --
                                            * Returns an API specific rendering interface for accessing API specific data.
                                            * Not all HW rendering APIs support or need this.
                                            * The contents of the returned pointer is specific to the rendering API
                                            * being used. See the various headers like libretro_vulkan.h, etc.
                                            *
                                            * GET_HW_RENDER_INTERFACE cannot be called before context_reset has been called.
                                            * Similarly, after context_destroyed callback returns,
                                            * the contents of the HW_RENDER_INTERFACE are invalidated.
                                            */
#define RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS (42 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const bool * --
                                            * If true, the libretro implementation supports achievements
                                            * either via memory descriptors set with RETRO_ENVIRONMENT_SET_MEMORY_MAPS
                                            * or via retro_get_memory_data/retro_get_memory_size.
                                            *
                                            * This must be called before the first call to retro_run.
                                            */
#define RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE (43 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const struct retro_hw_render_context_negotiation_interface * --
                                            * Sets an interface which lets the libretro core negotiate with frontend how a context is created.
                                            * The semantics of this interface depends on which API is used in SET_HW_RENDER earlier.
                                            * This interface will be used when the frontend is trying to create a HW rendering context,
                                            * so it will be used after SET_HW_RENDER, but before the context_reset callback.
                                            */
#define RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS 44
                                           /* uint64_t * --
                                            * Sets quirk flags associated with serialization. The frontend will zero any flags it doesn't
                                            * recognize or support. Should be set in either retro_init or retro_load_game, but not both.
                                            */
#define RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT (44 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* N/A (null) * --
                                            * The frontend will try to use a 'shared' hardware context (mostly applicable
                                            * to OpenGL) when a hardware context is being set up.
                                            *
                                            * Returns true if the frontend supports shared hardware contexts and false
                                            * if the frontend does not support shared hardware contexts.
                                            *
                                            * This will do nothing on its own until SET_HW_RENDER env callbacks are
                                            * being used.
                                            */
#define RETRO_ENVIRONMENT_GET_VFS_INTERFACE (45 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_vfs_interface_info * --
                                            * Gets access to the VFS interface.
                                            * VFS presence needs to be queried prior to load_game or any
                                            * get_system/save/other_directory being called to let front end know
                                            * core supports VFS before it starts handing out paths.
                                            * It is recomended to do so in retro_set_environment
                                            */
#define RETRO_ENVIRONMENT_GET_LED_INTERFACE (46 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_led_interface * --
                                            * Gets an interface which is used by a libretro core to set
                                            * state of LEDs.
                                            */
#define RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE (47 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* int * --
                                            * Tells the core if the frontend wants audio or video.
                                            * If disabled, the frontend will discard the audio or video,
                                            * so the core may decide to skip generating a frame or generating audio.
                                            * This is mainly used for increasing performance.
                                            * Bit 0 (value 1): Enable Video
                                            * Bit 1 (value 2): Enable Audio
                                            * Bit 2 (value 4): Use Fast Savestates.
                                            * Bit 3 (value 8): Hard Disable Audio
                                            * Other bits are reserved for future use and will default to zero.
                                            * If video is disabled:
                                            * * The frontend wants the core to not generate any video,
                                            *   including presenting frames via hardware acceleration.
                                            * * The frontend's video frame callback will do nothing.
                                            * * After running the frame, the video output of the next frame should be
                                            *   no different than if video was enabled, and saving and loading state
                                            *   should have no issues.
                                            * If audio is disabled:
                                            * * The frontend wants the core to not generate any audio.
                                            * * The frontend's audio callbacks will do nothing.
                                            * * After running the frame, the audio output of the next frame should be
                                            *   no different than if audio was enabled, and saving and loading state
                                            *   should have no issues.
                                            * Fast Savestates:
                                            * * Guaranteed to be created by the same binary that will load them.
                                            * * Will not be written to or read from the disk.
                                            * * Suggest that the core assumes loading state will succeed.
                                            * * Suggest that the core updates its memory buffers in-place if possible.
                                            * * Suggest that the core skips clearing memory.
                                            * * Suggest that the core skips resetting the system.
                                            * * Suggest that the core may skip validation steps.
                                            * Hard Disable Audio:
                                            * * Used for a secondary core when running ahead.
                                            * * Indicates that the frontend will never need audio from the core.
                                            * * Suggests that the core may stop synthesizing audio, but this should not
                                            *   compromise emulation accuracy.
                                            * * Audio output for the next frame does not matter, and the frontend will
                                            *   never need an accurate audio state in the future.
                                            * * State will never be saved when using Hard Disable Audio.
                                            */
#define RETRO_ENVIRONMENT_GET_MIDI_INTERFACE (48 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_midi_interface ** --
                                            * Returns a MIDI interface that can be used for raw data I/O.
                                            */

#define RETRO_ENVIRONMENT_GET_FASTFORWARDING (49 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                            /* bool * --
                                            * Boolean value that indicates whether or not the frontend is in
                                            * fastforwarding mode.
                                            */

#define RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE (50 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                            /* float * --
                                            * Float value that lets us know what target refresh rate
                                            * is curently in use by the frontend.
                                            *
                                            * The core can use the returned value to set an ideal
                                            * refresh rate/framerate.
                                            */

#define RETRO_ENVIRONMENT_GET_INPUT_BITMASKS (51 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                            /* bool * --
                                            * Boolean value that indicates whether or not the frontend supports
                                            * input bitmasks being returned by retro_input_state_t. The advantage
                                            * of this is that retro_input_state_t has to be only called once to
                                            * grab all button states instead of multiple times.
                                            *
                                            * If it returns true, you can pass RETRO_DEVICE_ID_JOYPAD_MASK as 'id'
                                            * to retro_input_state_t (make sure 'device' is set to RETRO_DEVICE_JOYPAD).
                                            * It will return a bitmask of all the digital buttons.
                                            */

#define RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION 52
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the core options
                                            * interface supported by the frontend. If callback return false,
                                            * API version is assumed to be 0.
                                            *
                                            * In legacy code, core options are set by passing an array of
                                            * retro_variable structs to RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This may be still be done regardless of the core options
                                            * interface version.
                                            *
                                            * If version is >= 1 however, core options may instead be set by
                                            * passing an array of retro_core_option_definition structs to
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS, or a 2D array of
                                            * retro_core_option_definition structs to RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.
                                            * This allows the core to additionally set option sublabel information
                                            * and/or provide localisation support.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS 53
                                           /* const struct retro_core_option_definition ** --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 1.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * 'data' points to an array of retro_core_option_definition structs
                                            * terminated by a { NULL, NULL, NULL, {{0}}, NULL } element.
                                            * retro_core_option_definition::key should be namespaced to not collide
                                            * with other implementations' keys. e.g. A core called
                                            * 'foo' should use keys named as 'foo_option'.
                                            * retro_core_option_definition::desc should contain a human readable
                                            * description of the key.
                                            * retro_core_option_definition::info should contain any additional human
                                            * readable information text that a typical user may need to
                                            * understand the functionality of the option.
                                            * retro_core_option_definition::values is an array of retro_core_option_value
                                            * structs terminated by a { NULL, NULL } element.
                                            * > retro_core_option_definition::values[index].value is an expected option
                                            *   value.
                                            * > retro_core_option_definition::values[index].label is a human readable
                                            *   label used when displaying the value on screen. If NULL,
                                            *   the value itself is used.
                                            * retro_core_option_definition::default_value is the default core option
                                            * setting. It must match one of the expected option values in the
                                            * retro_core_option_definition::values array. If it does not, or the
                                            * default value is NULL, the first entry in the
                                            * retro_core_option_definition::values array is treated as the default.
                                            *
                                            * The number of possible options should be very limited,
                                            * and must be less than RETRO_NUM_CORE_OPTION_VALUES_MAX.
                                            * i.e. it should be feasible to cycle through options
                                            * without a keyboard.
                                            *
                                            * Example entry:
                                            * {
                                            *     "foo_option",
                                            *     "Speed hack coprocessor X",
                                            *     "Provides increased performance at the expense of reduced accuracy",
                                            * 	  {
                                            *         { "false",    NULL },
                                            *         { "true",     NULL },
                                            *         { "unstable", "Turbo (Unstable)" },
                                            *         { NULL, NULL },
                                            *     },
                                            *     "false"
                                            * }
                                            *
                                            * Only strings are operated on. The possible values will
                                            * generally be displayed and stored as-is by the frontend.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL 54
                                           /* const struct retro_core_options_intl * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 1.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * This is fundamentally the same as RETRO_ENVIRONMENT_SET_CORE_OPTIONS,
                                            * with the addition of localisation support. The description of the
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS callback should be consulted
                                            * for further details.
                                            *
                                            * 'data' points to a retro_core_options_intl struct.
                                            *
                                            * retro_core_options_intl::us is a pointer to an array of
                                            * retro_core_option_definition structs defining the US English
                                            * core options implementation. It must point to a valid array.
                                            *
                                            * retro_core_options_intl::local is a pointer to an array of
                                            * retro_core_option_definition structs defining core options for
                                            * the current frontend language. It may be NULL (in which case
                                            * retro_core_options_intl::us is used by the frontend). Any items
                                            * missing from this array will be read from retro_core_options_intl::us
                                            * instead.
                                            *
                                            * NOTE: Default core option values are always taken from the
                                            * retro_core_options_intl::us array. Any default values in
                                            * retro_core_options_intl::local array will be ignored.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY 55
                                           /* struct retro_core_option_display * --
                                            *
                                            * Allows an implementation to signal the environment to show
                                            * or hide a variable when displaying core options. This is
                                            * considered a *suggestion*. The frontend is free to ignore
                                            * this callback, and its implementation not considered mandatory.
                                            *
                                            * 'data' points to a retro_core_option_display struct
                                            *
                                            * retro_core_option_display::key is a variable identifier
                                            * which has already been set by SET_VARIABLES/SET_CORE_OPTIONS.
                                            *
                                            * retro_core_option_display::visible is a boolean, specifying
                                            * whether variable should be displayed
                                            *
                                            * Note that all core option variables will be set visible by
                                            * default when calling SET_VARIABLES/SET_CORE_OPTIONS.
                                            */

#define RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER 56
                                           /* unsigned * --
                                            *
                                            * Allows an implementation to ask frontend preferred hardware
                                            * context to use. Core should use this information to deal
                                            * with what specific context to request with SET_HW_RENDER.
                                            *
                                            * 'data' points to an unsigned variable
                                            */

#define RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION 57
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the disk control
                                            * interface supported by the frontend. If callback return false,
                                            * API version is assumed to be 0.
                                            *
                                            * In legacy code, the disk control interface is defined by passing
                                            * a struct of type retro_disk_control_callback to
                                            * RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE.
                                            * This may be still be done regardless of the disk control
                                            * interface version.
                                            *
                                            * If version is >= 1 however, the disk control interface may
                                            * instead be defined by passing a struct of type
                                            * retro_disk_control_ext_callback to
                                            * RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE.
                                            * This allows the core to provide additional information about
                                            * disk images to the frontend and/or enables extra
                                            * disk control functionality by the frontend.
                                            */

#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE 58
                                           /* const struct retro_disk_control_ext_callback * --
                                            * Sets an interface which frontend can use to eject and insert
                                            * disk images, and also obtain information about individual
                                            * disk image files registered by the core.
                                            * This is used for games which consist of multiple images and
                                            * must be manually swapped out by the user (e.g. PSX, floppy disk
                                            * based systems).
                                            */

#define RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION 59
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the message
                                            * interface supported by the frontend. If callback returns
                                            * false, API version is assumed to be 0.
                                            *
                                            * In legacy code, messages may be displayed in an
                                            * implementation-specific manner by passing a struct
                                            * of type retro_message to RETRO_ENVIRONMENT_SET_MESSAGE.
                                            * This may be still be done regardless of the message
                                            * interface version.
                                            *
                                            * If version is >= 1 however, messages may instead be
                                            * displayed by passing a struct of type retro_message_ext
                                            * to RETRO_ENVIRONMENT_SET_MESSAGE_EXT. This allows the
                                            * core to specify message logging level, priority and
                                            * destination (OSD, logging interface or both).
                                            */

#define RETRO_ENVIRONMENT_SET_MESSAGE_EXT 60
                                           /* const struct retro_message_ext * --
                                            * Sets a message to be displayed in an implementation-specific
                                            * manner for a certain amount of 'frames'. Additionally allows
                                            * the core to specify message logging level, priority and
                                            * destination (OSD, logging interface or both).
                                            * Should not be used for trivial messages, which should simply be
                                            * logged via RETRO_ENVIRONMENT_GET_LOG_INTERFACE (or as a
                                            * fallback, stderr).
                                            */

#define RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS 61
                                           /* unsigned * --
                                            * Unsigned value is the number of active input devices
                                            * provided by the frontend. This may change between
                                            * frames, but will remain constant for the duration
                                            * of each frame.
                                            * If callback returns true, a core need not poll any
                                            * input device with an index greater than or equal to
                                            * the number of active devices.
                                            * If callback returns false, the number of active input
                                            * devices is unknown. In this case, all input devices
                                            * should be considered active.
                                            */

/* VFS functionality */

/* File paths:
 * File paths passed as parameters when using this API shall be well formed UNIX-style,
 * using "/" (unquoted forward slash) as directory separator regardless of the platform's native separator.
 * Paths shall also include at least one forward slash ("game.bin" is an invalid path, use "./game.bin" instead).
 * Other than the directory separator, cores shall not make assumptions about path format:
 * "C:/path/game.bin", "http://example.com/game.bin", "#game/game.bin", "./game.bin" (without quotes) are all valid paths.
 * Cores may replace the basename or remove path components from the end, and/or add new components;
 * however, cores shall not append "./", "../" or multiple consecutive forward slashes ("//") to paths they request to front end.
 * The frontend is encouraged to make such paths work as well as it can, but is allowed to give up if the core alters paths too much.
 * Frontends are encouraged, but not required, to support native file system paths (modulo replacing the directory separator, if applicable).
 * Cores are allowed to try using them, but must remain functional if the front rejects such requests.
 * Cores are encouraged to use the libretro-common filestream functions for file I/O,
 * as they seamlessly integrate with VFS, deal with directory separator replacement as appropriate
 * and provide platform-specific fallbacks in cases where front ends do not support VFS. */

/* Opaque file handle
 * Introduced in VFS API v1 */
struct retro_vfs_file_handle;

/* Opaque directory handle
 * Introduced in VFS API v3 */
struct retro_vfs_dir_handle;

/* File open flags
 * Introduced in VFS API v1 */
#define RETRO_VFS_FILE_ACCESS_READ            (1 << 0) /* Read only mode */
#define RETRO_VFS_FILE_ACCESS_WRITE           (1 << 1) /* Write only mode, discard contents and overwrites existing file unless RETRO_VFS_FILE_ACCESS_UPDATE is also specified */
#define RETRO_VFS_FILE_ACCESS_READ_WRITE      (RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_WRITE) /* Read-write mode, discard contents and overwrites existing file unless RETRO_VFS_FILE_ACCESS_UPDATE is also specified*/
#define RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING (1 << 2) /* Prevents discarding content of existing files opened for writing */

/* These are only hints. The frontend may choose to ignore them. Other than RAM/CPU/etc use,
   and how they react to unlikely external interference (for example someone else writing to that file,
   or the file's server going down), behavior will not change. */
#define RETRO_VFS_FILE_ACCESS_HINT_NONE              (0)
/* Indicate that the file will be accessed many times. The frontend should aggressively cache everything. */
#define RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS   (1 << 0)

/* Seek positions */
#define RETRO_VFS_SEEK_POSITION_START    0
#define RETRO_VFS_SEEK_POSITION_CURRENT  1
#define RETRO_VFS_SEEK_POSITION_END      2

/* stat() result flags
 * Introduced in VFS API v3 */
#define RETRO_VFS_STAT_IS_VALID               (1 << 0)
#define RETRO_VFS_STAT_IS_DIRECTORY           (1 << 1)
#define RETRO_VFS_STAT_IS_CHARACTER_SPECIAL   (1 << 2)

/* Get path from opaque handle. Returns the exact same path passed to file_open when getting the handle
 * Introduced in VFS API v1 */
typedef const char *(RETRO_CALLCONV *retro_vfs_get_path_t)(struct retro_vfs_file_handle *stream);

/* Open a file for reading or writing. If path points to a directory, this will
 * fail. Returns the opaque file handle, or NULL for error.
 * Introduced in VFS API v1 */
typedef struct retro_vfs_file_handle *(RETRO_CALLCONV *retro_vfs_open_t)(const char *path, unsigned mode, unsigned hints);

/* Close the file and release its resources. Must be called if open_file returns non-NULL. Returns 0 on success, -1 on failure.
 * Whether the call succeeds ot not, the handle passed as parameter becomes invalid and should no longer be used.
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_close_t)(struct retro_vfs_file_handle *stream);

/* Return the size of the file in bytes, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_size_t)(struct retro_vfs_file_handle *stream);

/* Truncate file to specified size. Returns 0 on success or -1 on error
 * Introduced in VFS API v2 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_truncate_t)(struct retro_vfs_file_handle *stream, int64_t length);

/* Get the current read / write position for the file. Returns -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_tell_t)(struct retro_vfs_file_handle *stream);

/* Set the current read/write position for the file. Returns the new position, -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_seek_t)(struct retro_vfs_file_handle *stream, int64_t offset, int seek_position);

/* Read data from a file. Returns the number of bytes read, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_read_t)(struct retro_vfs_file_handle *stream, void *s, uint64_t len);

/* Write data to a file. Returns the number of bytes written, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_write_t)(struct retro_vfs_file_handle *stream, const void *s, uint64_t len);

/* Flush pending writes to file, if using buffered IO. Returns 0 on sucess, or -1 on failure.
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_flush_t)(struct retro_vfs_file_handle *stream);

/* Delete the specified file. Returns 0 on success, -1 on failure
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_remove_t)(const char *path);

/* Rename the specified file. Returns 0 on success, -1 on failure
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_rename_t)(const char *old_path, const char *new_path);

/* Stat the specified file. Retruns a bitmask of RETRO_VFS_STAT_* flags, none are set if path was not valid.
 * Additionally stores file size in given variable, unless NULL is given.
 * Introduced in VFS API v3 */
typedef int (RETRO_CALLCONV *retro_vfs_stat_t)(const char *path, int32_t *size);

/* Create the specified directory. Returns 0 on success, -1 on unknown failure, -2 if already exists.
 * Introduced in VFS API v3 */
typedef int (RETRO_CALLCONV *retro_vfs_mkdir_t)(const char *dir);

/* Open the specified directory for listing. Returns the opaque dir handle, or NULL for error.
 * Support for the include_hidden argument may vary depending on the platform.
 * Introduced in VFS API v3 */
typedef struct retro_vfs_dir_handle *(RETRO_CALLCONV *retro_vfs_opendir_t)(const char *dir, bool include_hidden);

/* Read the directory entry at the current position, and move the read pointer to the next position.
 * Returns true on success, false if already on the last entry.
 * Introduced in VFS API v3 */
typedef bool (RETRO_CALLCONV *retro_vfs_readdir_t)(struct retro_vfs_dir_handle *dirstream);

/* Get the name of the last entry read. Returns a string on success, or NULL for error.
 * The returned string pointer is valid until the next call to readdir or closedir.
 * Introduced in VFS API v3 */
typedef const char *(RETRO_CALLCONV *retro_vfs_dirent_get_name_t)(struct retro_vfs_dir_handle *dirstream);

/* Check if the last entry read was a directory. Returns true if it was, false otherwise (or on error).
 * Introduced in VFS API v3 */
typedef bool (RETRO_CALLCONV *retro_vfs_dirent_is_dir_t)(struct retro_vfs_dir_handle *dirstream);

/* Close the directory and release its resources. Must be called if opendir returns non-NULL. Returns 0 on success, -1 on failure.
 * Whether the call succeeds ot not, the handle passed as parameter becomes invalid and should no longer be used.
 * Introduced in VFS API v3 */
typedef int (RETRO_CALLCONV *retro_vfs_closedir_t)(struct retro_vfs_dir_handle *dirstream);

struct retro_vfs_interface
{
   /* VFS API v1 */
	retro_vfs_get_path_t get_path;
	retro_vfs_open_t open;
	retro_vfs_close_t close;
	retro_vfs_size_t size;
	retro_vfs_tell_t tell;
	retro_vfs_seek_t seek;
	retro_vfs_read_t read;
	retro_vfs_write_t write;
	retro_vfs_flush_t flush;
	retro_vfs_remove_t remove;
	retro_vfs_rename_t rename;
   /* VFS API v2 */
   retro_vfs_truncate_t truncate;
   /* VFS API v3 */
   retro_vfs_stat_t stat;
   retro_vfs_mkdir_t mkdir;
   retro_vfs_opendir_t opendir;
   retro_vfs_readdir_t readdir;
   retro_vfs_dirent_get_name_t dirent_get_name;
   retro_vfs_dirent_is_dir_t dirent_is_dir;
   retro_vfs_closedir_t closedir;
};

struct retro_vfs_interface_info
{
   /* Set by core: should this be higher than the version the front end supports,
    * front end will return false in the RETRO_ENVIRONMENT_GET_VFS_INTERFACE call
    * Introduced in VFS API v1 */
   uint32_t required_interface_version;

   /* Frontend writes interface pointer here. The frontend also sets the actual
    * version, must be at least required_interface_version.
    * Introduced in VFS API v1 */
   struct retro_vfs_interface *iface;
};

enum retro_hw_render_interface_type
{
	RETRO_HW_RENDER_INTERFACE_VULKAN = 0,
	RETRO_HW_RENDER_INTERFACE_D3D9   = 1,
	RETRO_HW_RENDER_INTERFACE_D3D10  = 2,
	RETRO_HW_RENDER_INTERFACE_D3D11  = 3,
	RETRO_HW_RENDER_INTERFACE_D3D12  = 4,
   RETRO_HW_RENDER_INTERFACE_GSKIT_PS2  = 5,
   RETRO_HW_RENDER_INTERFACE_DUMMY  = INT_MAX
};

/* Base struct. All retro_hw_render_interface_* types
 * contain at least these fields. */
struct retro_hw_render_interface
{
   enum retro_hw_render_interface_type interface_type;
   unsigned interface_version;
};

typedef void (RETRO_CALLCONV *retro_set_led_state_t)(int led, int state);
struct retro_led_interface
{
    retro_set_led_state_t set_led_state;
};

/* Retrieves the current state of the MIDI input.
 * Returns true if it's enabled, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_input_enabled_t)(void);

/* Retrieves the current state of the MIDI output.
 * Returns true if it's enabled, false otherwise */
typedef bool (RETRO_CALLCONV *retro_midi_output_enabled_t)(void);

/* Reads next byte from the input stream.
 * Returns true if byte is read, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_read_t)(uint8_t *byte);

/* Writes byte to the output stream.
 * 'delta_time' is in microseconds and represent time elapsed since previous write.
 * Returns true if byte is written, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_write_t)(uint8_t byte, uint32_t delta_time);

/* Flushes previously written data.
 * Returns true if successful, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_flush_t)(void);

struct retro_midi_interface
{
   retro_midi_input_enabled_t input_enabled;
   retro_midi_output_enabled_t output_enabled;
   retro_midi_read_t read;
   retro_midi_write_t write;
   retro_midi_flush_t flush;
};

enum retro_hw_render_context_negotiation_interface_type
{
   RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN = 0,
   RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_DUMMY = INT_MAX
};

/* Base struct. All retro_hw_render_context_negotiation_interface_* types
 * contain at least these fields. */
struct retro_hw_render_context_negotiation_interface
{
   enum retro_hw_render_context_negotiation_interface_type interface_type;
   unsigned interface_version;
};

/* Serialized state is incomplete in some way. Set if serialization is
 * usable in typical end-user cases but should not be relied upon to
 * implement frame-sensitive frontend features such as netplay or
 * rerecording. */
#define RETRO_SERIALIZATION_QUIRK_INCOMPLETE (1 << 0)
/* The core must spend some time initializing before serialization is
 * supported. retro_serialize() will initially fail; retro_unserialize()
 * and retro_serialize_size() may or may not work correctly either. */
#define RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE (1 << 1)
/* Serialization size may change within a session. */
#define RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE (1 << 2)
/* Set by the frontend to acknowledge that it supports variable-sized
 * states. */
#define RETRO_SERIALIZATION_QUIRK_FRONT_VARIABLE_SIZE (1 << 3)
/* Serialized state can only be loaded during the same session. */
#define RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION (1 << 4)
/* Serialized state cannot be loaded on an architecture with a different
 * endianness from the one it was saved on. */
#define RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT (1 << 5)
/* Serialized state cannot be loaded on a different platform from the one it
 * was saved on for reasons other than endianness, such as word size
 * dependence */
#define RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT (1 << 6)

#define RETRO_MEMDESC_CONST      (1 << 0)   /* The frontend will never change this memory area once retro_load_game has returned. */
#define RETRO_MEMDESC_BIGENDIAN  (1 << 1)   /* The memory area contains big endian data. Default is little endian. */
#define RETRO_MEMDESC_SYSTEM_RAM (1 << 2)   /* The memory area is system RAM.  This is main RAM of the gaming system. */
#define RETRO_MEMDESC_SAVE_RAM   (1 << 3)   /* The memory area is save RAM. This RAM is usually found on a game cartridge, backed up by a battery. */
#define RETRO_MEMDESC_VIDEO_RAM  (1 << 4)   /* The memory area is video RAM (VRAM) */
#define RETRO_MEMDESC_ALIGN_2    (1 << 16)  /* All memory access in this area is aligned to their own size, or 2, whichever is smaller. */
#define RETRO_MEMDESC_ALIGN_4    (2 << 16)
#define RETRO_MEMDESC_ALIGN_8    (3 << 16)
#define RETRO_MEMDESC_MINSIZE_2  (1 << 24)  /* All memory in this region is accessed at least 2 bytes at the time. */
#define RETRO_MEMDESC_MINSIZE_4  (2 << 24)
#define RETRO_MEMDESC_MINSIZE_8  (3 << 24)
struct retro_memory_descriptor
{
   uint64_t flags;

   /* Pointer to the start of the relevant ROM or RAM chip.
    * It's strongly recommended to use 'offset' if possible, rather than
    * doing math on the pointer.
    *
    * If the same byte is mapped my multiple descriptors, their descriptors
    * must have the same pointer.
    * If 'start' does not point to the first byte in the pointer, put the
    * difference in 'offset' instead.
    *
    * May be NULL if there's nothing usable here (e.g. hardware registers and
    * open bus). No flags should be set if the pointer is NULL.
    * It's recommended to minimize the number of descriptors if possible,
    * but not mandatory. */
   void *ptr;
   size_t offset;

   /* This is the location in the emulated address space
    * where the mapping starts. */
   size_t start;

   /* Which bits must be same as in 'start' for this mapping to apply.
    * The first memory descriptor to claim a certain byte is the one
    * that applies.
    * A bit which is set in 'start' must also be set in this.
    * Can be zero, in which case each byte is assumed mapped exactly once.
    * In this case, 'len' must be a power of two. */
   size_t select;

   /* If this is nonzero, the set bits are assumed not connected to the
    * memory chip's address pins. */
   size_t disconnect;

   /* This one tells the size of the current memory area.
    * If, after start+disconnect are applied, the address is higher than
    * this, the highest bit of the address is cleared.
    *
    * If the address is still too high, the next highest bit is cleared.
    * Can be zero, in which case it's assumed to be infinite (as limited
    * by 'select' and 'disconnect'). */
   size_t len;

   /* To go from emulated address to physical address, the following
    * order applies:
    * Subtract 'start', pick off 'disconnect', apply 'len', add 'offset'. */

   /* The address space name must consist of only a-zA-Z0-9_-,
    * should be as short as feasible (maximum length is 8 plus the NUL),
    * and may not be any other address space plus one or more 0-9A-F
    * at the end.
    * However, multiple memory descriptors for the same address space is
    * allowed, and the address space name can be empty. NULL is treated
    * as empty.
    *
    * Address space names are case sensitive, but avoid lowercase if possible.
    * The same pointer may exist in multiple address spaces.
    *
    * Examples:
    * blank+blank - valid (multiple things may be mapped in the same namespace)
    * 'Sp'+'Sp' - valid (multiple things may be mapped in the same namespace)
    * 'A'+'B' - valid (neither is a prefix of each other)
    * 'S'+blank - valid ('S' is not in 0-9A-F)
    * 'a'+blank - valid ('a' is not in 0-9A-F)
    * 'a'+'A' - valid (neither is a prefix of each other)
    * 'AR'+blank - valid ('R' is not in 0-9A-F)
    * 'ARB'+blank - valid (the B can't be part of the address either, because
    *                      there is no namespace 'AR')
    * blank+'B' - not valid, because it's ambigous which address space B1234
    *             would refer to.
    * The length can't be used for that purpose; the frontend may want
    * to append arbitrary data to an address, without a separator. */
   const char *addrspace;

   /* TODO: When finalizing this one, add a description field, which should be
    * "WRAM" or something roughly equally long. */

   /* TODO: When finalizing this one, replace 'select' with 'limit', which tells
    * which bits can vary and still refer to the same address (limit = ~select).
    * TODO: limit? range? vary? something else? */

   /* TODO: When finalizing this one, if 'len' is above what 'select' (or
    * 'limit') allows, it's bankswitched. Bankswitched data must have both 'len'
    * and 'select' != 0, and the mappings don't tell how the system switches the
    * banks. */

   /* TODO: When finalizing this one, fix the 'len' bit removal order.
    * For len=0x1800, pointer 0x1C00 should go to 0x1400, not 0x0C00.
    * Algorithm: Take bits highest to lowest, but if it goes above len, clear
    * the most recent addition and continue on the next bit.
    * TODO: Can the above be optimized? Is "remove the lowest bit set in both
    * pointer and 'len'" equivalent? */

   /* TODO: Some emulators (MAME?) emulate big endian systems by only accessing
    * the emulated memory in 32-bit chunks, native endian. But that's nothing
    * compared to Darek Mihocka <http://www.emulators.com/docs/nx07_vm101.htm>
    * (section Emulation 103 - Nearly Free Byte Reversal) - he flips the ENTIRE
    * RAM backwards! I'll want to represent both of those, via some flags.
    *
    * I suspect MAME either didn't think of that idea, or don't want the #ifdef.
    * Not sure which, nor do I really care. */

   /* TODO: Some of those flags are unused and/or don't really make sense. Clean
    * them up. */
};

/* The frontend may use the largest value of 'start'+'select' in a
 * certain namespace to infer the size of the address space.
 *
 * If the address space is larger than that, a mapping with .ptr=NULL
 * should be at the end of the array, with .select set to all ones for
 * as long as the address space is big.
 *
 * Sample descriptors (minus .ptr, and RETRO_MEMFLAG_ on the flags):
 * SNES WRAM:
 * .start=0x7E0000, .len=0x20000
 * (Note that this must be mapped before the ROM in most cases; some of the
 * ROM mappers
 * try to claim $7E0000, or at least $7E8000.)
 * SNES SPC700 RAM:
 * .addrspace="S", .len=0x10000
 * SNES WRAM mirrors:
 * .flags=MIRROR, .start=0x000000, .select=0xC0E000, .len=0x2000
 * .flags=MIRROR, .start=0x800000, .select=0xC0E000, .len=0x2000
 * SNES WRAM mirrors, alternate equivalent descriptor:
 * .flags=MIRROR, .select=0x40E000, .disconnect=~0x1FFF
 * (Various similar constructions can be created by combining parts of
 * the above two.)
 * SNES LoROM (512KB, mirrored a couple of times):
 * .flags=CONST, .start=0x008000, .select=0x408000, .disconnect=0x8000, .len=512*1024
 * .flags=CONST, .start=0x400000, .select=0x400000, .disconnect=0x8000, .len=512*1024
 * SNES HiROM (4MB):
 * .flags=CONST,                 .start=0x400000, .select=0x400000, .len=4*1024*1024
 * .flags=CONST, .offset=0x8000, .start=0x008000, .select=0x408000, .len=4*1024*1024
 * SNES ExHiROM (8MB):
 * .flags=CONST, .offset=0,                  .start=0xC00000, .select=0xC00000, .len=4*1024*1024
 * .flags=CONST, .offset=4*1024*1024,        .start=0x400000, .select=0xC00000, .len=4*1024*1024
 * .flags=CONST, .offset=0x8000,             .start=0x808000, .select=0xC08000, .len=4*1024*1024
 * .flags=CONST, .offset=4*1024*1024+0x8000, .start=0x008000, .select=0xC08000, .len=4*1024*1024
 * Clarify the size of the address space:
 * .ptr=NULL, .select=0xFFFFFF
 * .len can be implied by .select in many of them, but was included for clarity.
 */

struct retro_memory_map
{
   const struct retro_memory_descriptor *descriptors;
   unsigned num_descriptors;
};

struct retro_controller_description
{
   /* Human-readable description of the controller. Even if using a generic
    * input device type, this can be set to the particular device type the
    * core uses. */
   const char *desc;

   /* Device type passed to retro_set_controller_port_device(). If the device
    * type is a sub-class of a generic input device type, use the
    * RETRO_DEVICE_SUBCLASS macro to create an ID.
    *
    * E.g. RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1). */
   unsigned id;
};

struct retro_controller_info
{
   const struct retro_controller_description *types;
   unsigned num_types;
};

struct retro_subsystem_memory_info
{
   /* The extension associated with a memory type, e.g. "psram". */
   const char *extension;

   /* The memory type for retro_get_memory(). This should be at
    * least 0x100 to avoid conflict with standardized
    * libretro memory types. */
   unsigned type;
};

struct retro_subsystem_rom_info
{
   /* Describes what the content is (SGB BIOS, GB ROM, etc). */
   const char *desc;

   /* Same definition as retro_get_system_info(). */
   const char *valid_extensions;

   /* Same definition as retro_get_system_info(). */
   bool need_fullpath;

   /* Same definition as retro_get_system_info(). */
   bool block_extract;

   /* This is set if the content is required to load a game.
    * If this is set to false, a zeroed-out retro_game_info can be passed. */
   bool required;

   /* Content can have multiple associated persistent
    * memory types (retro_get_memory()). */
   const struct retro_subsystem_memory_info *memory;
   unsigned num_memory;
};

struct retro_subsystem_info
{
   /* Human-readable string of the subsystem type, e.g. "Super GameBoy" */
   const char *desc;

   /* A computer friendly short string identifier for the subsystem type.
    * This name must be [a-z].
    * E.g. if desc is "Super GameBoy", this can be "sgb".
    * This identifier can be used for command-line interfaces, etc.
    */
   const char *ident;

   /* Infos for each content file. The first entry is assumed to be the
    * "most significant" content for frontend purposes.
    * E.g. with Super GameBoy, the first content should be the GameBoy ROM,
    * as it is the most "significant" content to a user.
    * If a frontend creates new file paths based on the content used
    * (e.g. savestates), it should use the path for the first ROM to do so. */
   const struct retro_subsystem_rom_info *roms;

   /* Number of content files associated with a subsystem. */
   unsigned num_roms;

   /* The type passed to retro_load_game_special(). */
   unsigned id;
};

typedef void (RETRO_CALLCONV *retro_proc_address_t)(void);

/* libretro API extension functions:
 * (None here so far).
 *
 * Get a symbol from a libretro core.
 * Cores should only return symbols which are actual
 * extensions to the libretro API.
 *
 * Frontends should not use this to obtain symbols to standard
 * libretro entry points (static linking or dlsym).
 *
 * The symbol name must be equal to the function name,
 * e.g. if void retro_foo(void); exists, the symbol must be called "retro_foo".
 * The returned function pointer must be cast to the corresponding type.
 */
typedef retro_proc_address_t (RETRO_CALLCONV *retro_get_proc_address_t)(const char *sym);

struct retro_get_proc_address_interface
{
   retro_get_proc_address_t get_proc_address;
};

enum retro_log_level
{
   RETRO_LOG_DEBUG = 0,
   RETRO_LOG_INFO,
   RETRO_LOG_WARN,
   RETRO_LOG_ERROR,

   RETRO_LOG_DUMMY = INT_MAX
};

/* Logging function. Takes log level argument as well. */
typedef void (RETRO_CALLCONV *retro_log_printf_t)(enum retro_log_level level,
      const char *fmt, ...);

struct retro_log_callback
{
   retro_log_printf_t log;
};

/* Performance related functions */

/* ID values for SIMD CPU features */
#define RETRO_SIMD_SSE      (1 << 0)
#define RETRO_SIMD_SSE2     (1 << 1)
#define RETRO_SIMD_VMX      (1 << 2)
#define RETRO_SIMD_VMX128   (1 << 3)
#define RETRO_SIMD_AVX      (1 << 4)
#define RETRO_SIMD_NEON     (1 << 5)
#define RETRO_SIMD_SSE3     (1 << 6)
#define RETRO_SIMD_SSSE3    (1 << 7)
#define RETRO_SIMD_MMX      (1 << 8)
#define RETRO_SIMD_MMXEXT   (1 << 9)
#define RETRO_SIMD_SSE4     (1 << 10)
#define RETRO_SIMD_SSE42    (1 << 11)
#define RETRO_SIMD_AVX2     (1 << 12)
#define RETRO_SIMD_VFPU     (1 << 13)
#define RETRO_SIMD_PS       (1 << 14)
#define RETRO_SIMD_AES      (1 << 15)
#define RETRO_SIMD_VFPV3    (1 << 16)
#define RETRO_SIMD_VFPV4    (1 << 17)
#define RETRO_SIMD_POPCNT   (1 << 18)
#define RETRO_SIMD_MOVBE    (1 << 19)
#define RETRO_SIMD_CMOV     (1 << 20)
#define RETRO_SIMD_ASIMD    (1 << 21)

typedef uint64_t retro_perf_tick_t;
typedef int64_t retro_time_t;

struct retro_perf_counter
{
   const char *ident;
   retro_perf_tick_t start;
   retro_perf_tick_t total;
   retro_perf_tick_t call_cnt;

   bool registered;
};

/* Returns current time in microseconds.
 * Tries to use the most accurate timer available.
 */
typedef retro_time_t (RETRO_CALLCONV *retro_perf_get_time_usec_t)(void);

/* A simple counter. Usually nanoseconds, but can also be CPU cycles.
 * Can be used directly if desired (when creating a more sophisticated
 * performance counter system).
 * */
typedef retro_perf_tick_t (RETRO_CALLCONV *retro_perf_get_counter_t)(void);

/* Returns a bit-mask of detected CPU features (RETRO_SIMD_*). */
typedef uint64_t (RETRO_CALLCONV *retro_get_cpu_features_t)(void);

/* Asks frontend to log and/or display the state of performance counters.
 * Performance counters can always be poked into manually as well.
 */
typedef void (RETRO_CALLCONV *retro_perf_log_t)(void);

/* Register a performance counter.
 * ident field must be set with a discrete value and other values in
 * retro_perf_counter must be 0.
 * Registering can be called multiple times. To avoid calling to
 * frontend redundantly, you can check registered field first. */
typedef void (RETRO_CALLCONV *retro_perf_register_t)(struct retro_perf_counter *counter);

/* Starts a registered counter. */
typedef void (RETRO_CALLCONV *retro_perf_start_t)(struct retro_perf_counter *counter);

/* Stops a registered counter. */
typedef void (RETRO_CALLCONV *retro_perf_stop_t)(struct retro_perf_counter *counter);

/* For convenience it can be useful to wrap register, start and stop in macros.
 * E.g.:
 * #ifdef LOG_PERFORMANCE
 * #define RETRO_PERFORMANCE_INIT(perf_cb, name) static struct retro_perf_counter name = {#name}; if (!name.registered) perf_cb.perf_register(&(name))
 * #define RETRO_PERFORMANCE_START(perf_cb, name) perf_cb.perf_start(&(name))
 * #define RETRO_PERFORMANCE_STOP(perf_cb, name) perf_cb.perf_stop(&(name))
 * #else
 * ... Blank macros ...
 * #endif
 *
 * These can then be used mid-functions around code snippets.
 *
 * extern struct retro_perf_callback perf_cb;  * Somewhere in the core.
 *
 * void do_some_heavy_work(void)
 * {
 *    RETRO_PERFORMANCE_INIT(cb, work_1;
 *    RETRO_PERFORMANCE_START(cb, work_1);
 *    heavy_work_1();
 *    RETRO_PERFORMANCE_STOP(cb, work_1);
 *
 *    RETRO_PERFORMANCE_INIT(cb, work_2);
 *    RETRO_PERFORMANCE_START(cb, work_2);
 *    heavy_work_2();
 *    RETRO_PERFORMANCE_STOP(cb, work_2);
 * }
 *
 * void retro_deinit(void)
 * {
 *    perf_cb.perf_log();  * Log all perf counters here for example.
 * }
 */

struct retro_perf_callback
{
   retro_perf_get_time_usec_t    get_time_usec;
   retro_get_cpu_features_t      get_cpu_features;

   retro_perf_get_counter_t      get_perf_counter;
   retro_perf_register_t         perf_register;
   retro_perf_start_t            perf_start;
   retro_perf_stop_t             perf_stop;
   retro_perf_log_t              perf_log;
};

/* FIXME: Document the sensor API and work out behavior.
 * It will be marked as experimental until then.
 */
enum retro_sensor_action
{
   RETRO_SENSOR_ACCELEROMETER_ENABLE = 0,
   RETRO_SENSOR_ACCELEROMETER_DISABLE,
   RETRO_SENSOR_GYROSCOPE_ENABLE,
   RETRO_SENSOR_GYROSCOPE_DISABLE,
   RETRO_SENSOR_ILLUMINANCE_ENABLE,
   RETRO_SENSOR_ILLUMINANCE_DISABLE,

   RETRO_SENSOR_DUMMY = INT_MAX
};

/* Id values for SENSOR types. */
#define RETRO_SENSOR_ACCELEROMETER_X 0
#define RETRO_SENSOR_ACCELEROMETER_Y 1
#define RETRO_SENSOR_ACCELEROMETER_Z 2
#define RETRO_SENSOR_GYROSCOPE_X 3
#define RETRO_SENSOR_GYROSCOPE_Y 4
#define RETRO_SENSOR_GYROSCOPE_Z 5
#define RETRO_SENSOR_ILLUMINANCE 6

typedef bool (RETRO_CALLCONV *retro_set_sensor_state_t)(unsigned port,
      enum retro_sensor_action action, unsigned rate);

typedef float (RETRO_CALLCONV *retro_sensor_get_input_t)(unsigned port, unsigned id);

struct retro_sensor_interface
{
   retro_set_sensor_state_t set_sensor_state;
   retro_sensor_get_input_t get_sensor_input;
};

enum retro_camera_buffer
{
   RETRO_CAMERA_BUFFER_OPENGL_TEXTURE = 0,
   RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER,

   RETRO_CAMERA_BUFFER_DUMMY = INT_MAX
};

/* Starts the camera driver. Can only be called in retro_run(). */
typedef bool (RETRO_CALLCONV *retro_camera_start_t)(void);

/* Stops the camera driver. Can only be called in retro_run(). */
typedef void (RETRO_CALLCONV *retro_camera_stop_t)(void);

/* Callback which signals when the camera driver is initialized
 * and/or deinitialized.
 * retro_camera_start_t can be called in initialized callback.
 */
typedef void (RETRO_CALLCONV *retro_camera_lifetime_status_t)(void);

/* A callback for raw framebuffer data. buffer points to an XRGB8888 buffer.
 * Width, height and pitch are similar to retro_video_refresh_t.
 * First pixel is top-left origin.
 */
typedef void (RETRO_CALLCONV *retro_camera_frame_raw_framebuffer_t)(const uint32_t *buffer,
      unsigned width, unsigned height, size_t pitch);

/* A callback for when OpenGL textures are used.
 *
 * texture_id is a texture owned by camera driver.
 * Its state or content should be considered immutable, except for things like
 * texture filtering and clamping.
 *
 * texture_target is the texture target for the GL texture.
 * These can include e.g. GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE, and possibly
 * more depending on extensions.
 *
 * affine points to a packed 3x3 column-major matrix used to apply an affine
 * transform to texture coordinates. (affine_matrix * vec3(coord_x, coord_y, 1.0))
 * After transform, normalized texture coord (0, 0) should be bottom-left
 * and (1, 1) should be top-right (or (width, height) for RECTANGLE).
 *
 * GL-specific typedefs are avoided here to avoid relying on gl.h in
 * the API definition.
 */
typedef void (RETRO_CALLCONV *retro_camera_frame_opengl_texture_t)(unsigned texture_id,
      unsigned texture_target, const float *affine);

struct retro_camera_callback
{
   /* Set by libretro core.
    * Example bitmask: caps = (1 << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE) | (1 << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER).
    */
   uint64_t caps;

   /* Desired resolution for camera. Is only used as a hint. */
   unsigned width;
   unsigned height;

   /* Set by frontend. */
   retro_camera_start_t start;
   retro_camera_stop_t stop;

   /* Set by libretro core if raw framebuffer callbacks will be used. */
   retro_camera_frame_raw_framebuffer_t frame_raw_framebuffer;

   /* Set by libretro core if OpenGL texture callbacks will be used. */
   retro_camera_frame_opengl_texture_t frame_opengl_texture;

   /* Set by libretro core. Called after camera driver is initialized and
    * ready to be started.
    * Can be NULL, in which this callback is not called.
    */
   retro_camera_lifetime_status_t initialized;

   /* Set by libretro core. Called right before camera driver is
    * deinitialized.
    * Can be NULL, in which this callback is not called.
    */
   retro_camera_lifetime_status_t deinitialized;
};

/* Sets the interval of time and/or distance at which to update/poll
 * location-based data.
 *
 * To ensure compatibility with all location-based implementations,
 * values for both interval_ms and interval_distance should be provided.
 *
 * interval_ms is the interval expressed in milliseconds.
 * interval_distance is the distance interval expressed in meters.
 */
typedef void (RETRO_CALLCONV *retro_location_set_interval_t)(unsigned interval_ms,
      unsigned interval_distance);

/* Start location services. The device will start listening for changes to the
 * current location at regular intervals (which are defined with
 * retro_location_set_interval_t). */
typedef bool (RETRO_CALLCONV *retro_location_start_t)(void);

/* Stop location services. The device will stop listening for changes
 * to the current location. */
typedef void (RETRO_CALLCONV *retro_location_stop_t)(void);

/* Get the position of the current location. Will set parameters to
 * 0 if no new  location update has happened since the last time. */
typedef bool (RETRO_CALLCONV *retro_location_get_position_t)(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);

/* Callback which signals when the location driver is initialized
 * and/or deinitialized.
 * retro_location_start_t can be called in initialized callback.
 */
typedef void (RETRO_CALLCONV *retro_location_lifetime_status_t)(void);

struct retro_location_callback
{
   retro_location_start_t         start;
   retro_location_stop_t          stop;
   retro_location_get_position_t  get_position;
   retro_location_set_interval_t  set_interval;

   retro_location_lifetime_status_t initialized;
   retro_location_lifetime_status_t deinitialized;
};

enum retro_rumble_effect
{
   RETRO_RUMBLE_STRONG = 0,
   RETRO_RUMBLE_WEAK = 1,

   RETRO_RUMBLE_DUMMY = INT_MAX
};

/* Sets rumble state for joypad plugged in port 'port'.
 * Rumble effects are controlled independently,
 * and setting e.g. strong rumble does not override weak rumble.
 * Strength has a range of [0, 0xffff].
 *
 * Returns true if rumble state request was honored.
 * Calling this before first retro_run() is likely to return false. */
typedef bool (RETRO_CALLCONV *retro_set_rumble_state_t)(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

struct retro_rumble_interface
{
   retro_set_rumble_state_t set_rumble_state;
};

/* Notifies libretro that audio data should be written. */
typedef void (RETRO_CALLCONV *retro_audio_callback_t)(void);

/* True: Audio driver in frontend is active, and callback is
 * expected to be called regularily.
 * False: Audio driver in frontend is paused or inactive.
 * Audio callback will not be called until set_state has been
 * called with true.
 * Initial state is false (inactive).
 */
typedef void (RETRO_CALLCONV *retro_audio_set_state_callback_t)(bool enabled);

struct retro_audio_callback
{
   retro_audio_callback_t callback;
   retro_audio_set_state_callback_t set_state;
};

/* Notifies a libretro core of time spent since last invocation
 * of retro_run() in microseconds.
 *
 * It will be called right before retro_run() every frame.
 * The frontend can tamper with timing to support cases like
 * fast-forward, slow-motion and framestepping.
 *
 * In those scenarios the reference frame time value will be used. */
typedef int64_t retro_usec_t;
typedef void (RETRO_CALLCONV *retro_frame_time_callback_t)(retro_usec_t usec);
struct retro_frame_time_callback
{
   retro_frame_time_callback_t callback;
   /* Represents the time of one frame. It is computed as
    * 1000000 / fps, but the implementation will resolve the
    * rounding to ensure that framestepping, etc is exact. */
   retro_usec_t reference;
};

/* Pass this to retro_video_refresh_t if rendering to hardware.
 * Passing NULL to retro_video_refresh_t is still a frame dupe as normal.
 * */
#define RETRO_HW_FRAME_BUFFER_VALID ((void*)-1)

/* Invalidates the current HW context.
 * Any GL state is lost, and must not be deinitialized explicitly.
 * If explicit deinitialization is desired by the libretro core,
 * it should implement context_destroy callback.
 * If called, all GPU resources must be reinitialized.
 * Usually called when frontend reinits video driver.
 * Also called first time video driver is initialized,
 * allowing libretro core to initialize resources.
 */
typedef void (RETRO_CALLCONV *retro_hw_context_reset_t)(void);

/* Gets current framebuffer which is to be rendered to.
 * Could change every frame potentially.
 */
typedef uintptr_t (RETRO_CALLCONV *retro_hw_get_current_framebuffer_t)(void);

/* Get a symbol from HW context. */
typedef retro_proc_address_t (RETRO_CALLCONV *retro_hw_get_proc_address_t)(const char *sym);

enum retro_hw_context_type
{
   RETRO_HW_CONTEXT_NONE             = 0,
   /* OpenGL 2.x. Driver can choose to use latest compatibility context. */
   RETRO_HW_CONTEXT_OPENGL           = 1,
   /* OpenGL ES 2.0. */
   RETRO_HW_CONTEXT_OPENGLES2        = 2,
   /* Modern desktop core GL context. Use version_major/
    * version_minor fields to set GL version. */
   RETRO_HW_CONTEXT_OPENGL_CORE      = 3,
   /* OpenGL ES 3.0 */
   RETRO_HW_CONTEXT_OPENGLES3        = 4,
   /* OpenGL ES 3.1+. Set version_major/version_minor. For GLES2 and GLES3,
    * use the corresponding enums directly. */
   RETRO_HW_CONTEXT_OPENGLES_VERSION = 5,

   /* Vulkan, see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE. */
   RETRO_HW_CONTEXT_VULKAN           = 6,

   /* Direct3D, set version_major to select the type of interface
    * returned by RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE */
   RETRO_HW_CONTEXT_DIRECT3D         = 7,

   RETRO_HW_CONTEXT_DUMMY = INT_MAX
};

struct retro_hw_render_callback
{
   /* Which API to use. Set by libretro core. */
   enum retro_hw_context_type context_type;

   /* Called when a context has been created or when it has been reset.
    * An OpenGL context is only valid after context_reset() has been called.
    *
    * When context_reset is called, OpenGL resources in the libretro
    * implementation are guaranteed to be invalid.
    *
    * It is possible that context_reset is called multiple times during an
    * application lifecycle.
    * If context_reset is called without any notification (context_destroy),
    * the OpenGL context was lost and resources should just be recreated
    * without any attempt to "free" old resources.
    */
   retro_hw_context_reset_t context_reset;

   /* Set by frontend.
    * TODO: This is rather obsolete. The frontend should not
    * be providing preallocated framebuffers. */
   retro_hw_get_current_framebuffer_t get_current_framebuffer;

   /* Set by frontend.
    * Can return all relevant functions, including glClear on Windows. */
   retro_hw_get_proc_address_t get_proc_address;

   /* Set if render buffers should have depth component attached.
    * TODO: Obsolete. */
   bool depth;

   /* Set if stencil buffers should be attached.
    * TODO: Obsolete. */
   bool stencil;

   /* If depth and stencil are true, a packed 24/8 buffer will be added.
    * Only attaching stencil is invalid and will be ignored. */

   /* Use conventional bottom-left origin convention. If false,
    * standard libretro top-left origin semantics are used.
    * TODO: Move to GL specific interface. */
   bool bottom_left_origin;

   /* Major version number for core GL context or GLES 3.1+. */
   unsigned version_major;

   /* Minor version number for core GL context or GLES 3.1+. */
   unsigned version_minor;

   /* If this is true, the frontend will go very far to avoid
    * resetting context in scenarios like toggling fullscreen, etc.
    * TODO: Obsolete? Maybe frontend should just always assume this ...
    */
   bool cache_context;

   /* The reset callback might still be called in extreme situations
    * such as if the context is lost beyond recovery.
    *
    * For optimal stability, set this to false, and allow context to be
    * reset at any time.
    */

   /* A callback to be called before the context is destroyed in a
    * controlled way by the frontend. */
   retro_hw_context_reset_t context_destroy;

   /* OpenGL resources can be deinitialized cleanly at this step.
    * context_destroy can be set to NULL, in which resources will
    * just be destroyed without any notification.
    *
    * Even when context_destroy is non-NULL, it is possible that
    * context_reset is called without any destroy notification.
    * This happens if context is lost by external factors (such as
    * notified by GL_ARB_robustness).
    *
    * In this case, the context is assumed to be already dead,
    * and the libretro implementation must not try to free any OpenGL
    * resources in the subsequent context_reset.
    */

   /* Creates a debug context. */
   bool debug_context;
};

/* Callback type passed in RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK.
 * Called by the frontend in response to keyboard events.
 * down is set if the key is being pressed, or false if it is being released.
 * keycode is the RETROK value of the char.
 * character is the text character of the pressed key. (UTF-32).
 * key_modifiers is a set of RETROKMOD values or'ed together.
 *
 * The pressed/keycode state can be indepedent of the character.
 * It is also possible that multiple characters are generated from a
 * single keypress.
 * Keycode events should be treated separately from character events.
 * However, when possible, the frontend should try to synchronize these.
 * If only a character is posted, keycode should be RETROK_UNKNOWN.
 *
 * Similarily if only a keycode event is generated with no corresponding
 * character, character should be 0.
 */
typedef void (RETRO_CALLCONV *retro_keyboard_event_t)(bool down, unsigned keycode,
      uint32_t character, uint16_t key_modifiers);

struct retro_keyboard_callback
{
   retro_keyboard_event_t callback;
};

/* Callbacks for RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE &
 * RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE.
 * Should be set for implementations which can swap out multiple disk
 * images in runtime.
 *
 * If the implementation can do this automatically, it should strive to do so.
 * However, there are cases where the user must manually do so.
 *
 * Overview: To swap a disk image, eject the disk image with
 * set_eject_state(true).
 * Set the disk index with set_image_index(index). Insert the disk again
 * with set_eject_state(false).
 */

/* If ejected is true, "ejects" the virtual disk tray.
 * When ejected, the disk image index can be set.
 */
typedef bool (RETRO_CALLCONV *retro_set_eject_state_t)(bool ejected);

/* Gets current eject state. The initial state is 'not ejected'. */
typedef bool (RETRO_CALLCONV *retro_get_eject_state_t)(void);

/* Gets current disk index. First disk is index 0.
 * If return value is >= get_num_images(), no disk is currently inserted.
 */
typedef unsigned (RETRO_CALLCONV *retro_get_image_index_t)(void);

/* Sets image index. Can only be called when disk is ejected.
 * The implementation supports setting "no disk" by using an
 * index >= get_num_images().
 */
typedef bool (RETRO_CALLCONV *retro_set_image_index_t)(unsigned index);

/* Gets total number of images which are available to use. */
typedef unsigned (RETRO_CALLCONV *retro_get_num_images_t)(void);

struct retro_game_info;

/* Replaces the disk image associated with index.
 * Arguments to pass in info have same requirements as retro_load_game().
 * Virtual disk tray must be ejected when calling this.
 *
 * Replacing a disk image with info = NULL will remove the disk image
 * from the internal list.
 * As a result, calls to get_image_index() can change.
 *
 * E.g. replace_image_index(1, NULL), and previous get_image_index()
 * returned 4 before.
 * Index 1 will be removed, and the new index is 3.
 */
typedef bool (RETRO_CALLCONV *retro_replace_image_index_t)(unsigned index,
      const struct retro_game_info *info);

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set
 * with replace_image_index. */
typedef bool (RETRO_CALLCONV *retro_add_image_index_t)(void);

/* Sets initial image to insert in drive when calling
 * core_load_game().
 * Since we cannot pass the initial index when loading
 * content (this would require a major API change), this
 * is set by the frontend *before* calling the core's
 * retro_load_game()/retro_load_game_special() implementation.
 * A core should therefore cache the index/path values and handle
 * them inside retro_load_game()/retro_load_game_special().
 * - If 'index' is invalid (index >= get_num_images()), the
 *   core should ignore the set value and instead use 0
 * - 'path' is used purely for error checking - i.e. when
 *   content is loaded, the core should verify that the
 *   disk specified by 'index' has the specified file path.
 *   This is to guard against auto selecting the wrong image
 *   if (for example) the user should modify an existing M3U
 *   playlist. We have to let the core handle this because
 *   set_initial_image() must be called before loading content,
 *   i.e. the frontend cannot access image paths in advance
 *   and thus cannot perform the error check itself.
 *   If set path and content path do not match, the core should
 *   ignore the set 'index' value and instead use 0
 * Returns 'false' if index or 'path' are invalid, or core
 * does not support this functionality
 */
typedef bool (RETRO_CALLCONV *retro_set_initial_image_t)(unsigned index, const char *path);

/* Fetches the path of the specified disk image file.
 * Returns 'false' if index is invalid (index >= get_num_images())
 * or path is otherwise unavailable.
 */
typedef bool (RETRO_CALLCONV *retro_get_image_path_t)(unsigned index, char *path, size_t len);

/* Fetches a core-provided 'label' for the specified disk
 * image file. In the simplest case this may be a file name
 * (without extension), but for cores with more complex
 * content requirements information may be provided to
 * facilitate user disk swapping - for example, a core
 * running floppy-disk-based content may uniquely label
 * save disks, data disks, level disks, etc. with names
 * corresponding to in-game disk change prompts (so the
 * frontend can provide better user guidance than a 'dumb'
 * disk index value).
 * Returns 'false' if index is invalid (index >= get_num_images())
 * or label is otherwise unavailable.
 */
typedef bool (RETRO_CALLCONV *retro_get_image_label_t)(unsigned index, char *label, size_t len);

struct retro_disk_control_callback
{
   retro_set_eject_state_t set_eject_state;
   retro_get_eject_state_t get_eject_state;

   retro_get_image_index_t get_image_index;
   retro_set_image_index_t set_image_index;
   retro_get_num_images_t  get_num_images;

   retro_replace_image_index_t replace_image_index;
   retro_add_image_index_t add_image_index;
};

struct retro_disk_control_ext_callback
{
   retro_set_eject_state_t set_eject_state;
   retro_get_eject_state_t get_eject_state;

   retro_get_image_index_t get_image_index;
   retro_set_image_index_t set_image_index;
   retro_get_num_images_t  get_num_images;

   retro_replace_image_index_t replace_image_index;
   retro_add_image_index_t add_image_index;

   /* NOTE: Frontend will only attempt to record/restore
    * last used disk index if both set_initial_image()
    * and get_image_path() are implemented */
   retro_set_initial_image_t set_initial_image; /* Optional - may be NULL */

   retro_get_image_path_t get_image_path;       /* Optional - may be NULL */
   retro_get_image_label_t get_image_label;     /* Optional - may be NULL */
};

enum retro_pixel_format
{
   /* 0RGB1555, native endian.
    * 0 bit must be set to 0.
    * This pixel format is default for compatibility concerns only.
    * If a 15/16-bit pixel format is desired, consider using RGB565. */
   RETRO_PIXEL_FORMAT_0RGB1555 = 0,

   /* XRGB8888, native endian.
    * X bits are ignored. */
   RETRO_PIXEL_FORMAT_XRGB8888 = 1,

   /* RGB565, native endian.
    * This pixel format is the recommended format to use if a 15/16-bit
    * format is desired as it is the pixel format that is typically
    * available on a wide range of low-power devices.
    *
    * It is also natively supported in APIs like OpenGL ES. */
   RETRO_PIXEL_FORMAT_RGB565   = 2,

   /* Ensure sizeof() == sizeof(int). */
   RETRO_PIXEL_FORMAT_UNKNOWN  = INT_MAX
};

struct retro_message
{
   const char *msg;        /* Message to be displayed. */
   unsigned    frames;     /* Duration in frames of message. */
};

enum retro_message_target
{
   RETRO_MESSAGE_TARGET_ALL = 0,
   RETRO_MESSAGE_TARGET_OSD,
   RETRO_MESSAGE_TARGET_LOG
};

enum retro_message_type
{
   RETRO_MESSAGE_TYPE_NOTIFICATION = 0,
   RETRO_MESSAGE_TYPE_NOTIFICATION_ALT,
   RETRO_MESSAGE_TYPE_STATUS,
   RETRO_MESSAGE_TYPE_PROGRESS
};

struct retro_message_ext
{
   /* Message string to be displayed/logged */
   const char *msg;
   /* Duration (in ms) of message when targeting the OSD */
   unsigned duration;
   /* Message priority when targeting the OSD
    * > When multiple concurrent messages are sent to
    *   the frontend and the frontend does not have the
    *   capacity to display them all, messages with the
    *   *highest* priority value should be shown
    * > There is no upper limit to a message priority
    *   value (within the bounds of the unsigned data type)
    * > In the reference frontend (RetroArch), the same
    *   priority values are used for frontend-generated
    *   notifications, which are typically assigned values
    *   between 0 and 3 depending upon importance */
   unsigned priority;
   /* Message logging level (info, warn, error, etc.) */
   enum retro_log_level level;
   /* Message destination: OSD, logging interface or both */
   enum retro_message_target target;
   /* Message 'type' when targeting the OSD
    * > RETRO_MESSAGE_TYPE_NOTIFICATION: Specifies that a
    *   message should be handled in identical fashion to
    *   a standard frontend-generated notification
    * > RETRO_MESSAGE_TYPE_NOTIFICATION_ALT: Specifies that
    *   message is a notification that requires user attention
    *   or action, but that it should be displayed in a manner
    *   that differs from standard frontend-generated notifications.
    *   This would typically correspond to messages that should be
    *   displayed immediately (independently from any internal
    *   frontend message queue), and/or which should be visually
    *   distinguishable from frontend-generated notifications.
    *   For example, a core may wish to inform the user of
    *   information related to a disk-change event. It is
    *   expected that the frontend itself may provide a
    *   notification in this case; if the core sends a
    *   message of type RETRO_MESSAGE_TYPE_NOTIFICATION, an
    *   uncomfortable 'double-notification' may occur. A message
    *   of RETRO_MESSAGE_TYPE_NOTIFICATION_ALT should therefore
    *   be presented such that visual conflict with regular
    *   notifications does not occur
    * > RETRO_MESSAGE_TYPE_STATUS: Indicates that message
    *   is not a standard notification. This typically
    *   corresponds to 'status' indicators, such as a core's
    *   internal FPS, which are intended to be displayed
    *   either permanently while a core is running, or in
    *   a manner that does not suggest user attention or action
    *   is required. 'Status' type messages should therefore be
    *   displayed in a different on-screen location and in a manner
    *   easily distinguishable from both standard frontend-generated
    *   notifications and messages of type RETRO_MESSAGE_TYPE_NOTIFICATION_ALT
    * > RETRO_MESSAGE_TYPE_PROGRESS: Indicates that message reports
    *   the progress of an internal core task. For example, in cases
    *   where a core itself handles the loading of content from a file,
    *   this may correspond to the percentage of the file that has been
    *   read. Alternatively, an audio/video playback core may use a
    *   message of type RETRO_MESSAGE_TYPE_PROGRESS to display the current
    *   playback position as a percentage of the runtime. 'Progress' type
    *   messages should therefore be displayed as a literal progress bar,
    *   where:
    *   - 'retro_message_ext.msg' is the progress bar title/label
    *   - 'retro_message_ext.progress' determines the length of
    *     the progress bar
    * NOTE: Message type is a *hint*, and may be ignored
    * by the frontend. If a frontend lacks support for
    * displaying messages via alternate means than standard
    * frontend-generated notifications, it will treat *all*
    * messages as having the type RETRO_MESSAGE_TYPE_NOTIFICATION */
   enum retro_message_type type;
   /* Task progress when targeting the OSD and message is
    * of type RETRO_MESSAGE_TYPE_PROGRESS
    * > -1:    Unmetered/indeterminate
    * > 0-100: Current progress percentage
    * NOTE: Since message type is a hint, a frontend may ignore
    * progress values. Where relevant, a core should therefore
    * include progress percentage within the message string,
    * such that the message intent remains clear when displayed
    * as a standard frontend-generated notification */
   int8_t progress;
};

/* Describes how the libretro implementation maps a libretro input bind
 * to its internal input system through a human readable string.
 * This string can be used to better let a user configure input. */
struct retro_input_descriptor
{
   /* Associates given parameters with a description. */
   unsigned port;
   unsigned device;
   unsigned index;
   unsigned id;

   /* Human readable description for parameters.
    * The pointer must remain valid until
    * retro_unload_game() is called. */
   const char *description;
};

struct retro_system_info
{
   /* All pointers are owned by libretro implementation, and pointers must
    * remain valid until retro_deinit() is called. */

   const char *library_name;      /* Descriptive name of library. Should not
                                   * contain any version numbers, etc. */
   const char *library_version;   /* Descriptive version of core. */

   const char *valid_extensions;  /* A string listing probably content
                                   * extensions the core will be able to
                                   * load, separated with pipe.
                                   * I.e. "bin|rom|iso".
                                   * Typically used for a GUI to filter
                                   * out extensions. */

   /* Libretro cores that need to have direct access to their content
    * files, including cores which use the path of the content files to
    * determine the paths of other files, should set need_fullpath to true.
    *
    * Cores should strive for setting need_fullpath to false,
    * as it allows the frontend to perform patching, etc.
    *
    * If need_fullpath is true and retro_load_game() is called:
    *    - retro_game_info::path is guaranteed to have a valid path
    *    - retro_game_info::data and retro_game_info::size are invalid
    *
    * If need_fullpath is false and retro_load_game() is called:
    *    - retro_game_info::path may be NULL
    *    - retro_game_info::data and retro_game_info::size are guaranteed
    *      to be valid
    *
    * See also:
    *    - RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY
    *    - RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY
    */
   bool        need_fullpath;

   /* If true, the frontend is not allowed to extract any archives before
    * loading the real content.
    * Necessary for certain libretro implementations that load games
    * from zipped archives. */
   bool        block_extract;
};

struct retro_game_geometry
{
   unsigned base_width;    /* Nominal video width of game. */
   unsigned base_height;   /* Nominal video height of game. */
   unsigned max_width;     /* Maximum possible width of game. */
   unsigned max_height;    /* Maximum possible height of game. */

   float    aspect_ratio;  /* Nominal aspect ratio of game. If
                            * aspect_ratio is <= 0.0, an aspect ratio
                            * of base_width / base_height is assumed.
                            * A frontend could override this setting,
                            * if desired. */
};

struct retro_system_timing
{
   double fps;             /* FPS of video content. */
   double sample_rate;     /* Sampling rate of audio. */
};

struct retro_system_av_info
{
   struct retro_game_geometry geometry;
   struct retro_system_timing timing;
};

struct retro_variable
{
   /* Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE.
    * If NULL, obtains the complete environment string if more
    * complex parsing is necessary.
    * The environment string is formatted as key-value pairs
    * delimited by semicolons as so:
    * "key1=value1;key2=value2;..."
    */
   const char *key;

   /* Value to be obtained. If key does not exist, it is set to NULL. */
   const char *value;
};

struct retro_core_option_display
{
   /* Variable to configure in RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY */
   const char *key;

   /* Specifies whether variable should be displayed
    * when presenting core options to the user */
   bool visible;
};

/* Maximum number of values permitted for a core option
 * > Note: We have to set a maximum value due the limitations
 *   of the C language - i.e. it is not possible to create an
 *   array of structs each containing a variable sized array,
 *   so the retro_core_option_definition values array must
 *   have a fixed size. The size limit of 128 is a balancing
 *   act - it needs to be large enough to support all 'sane'
 *   core options, but setting it too large may impact low memory
 *   platforms. In practise, if a core option has more than
 *   128 values then the implementation is likely flawed.
 *   To quote the above API reference:
 *      "The number of possible options should be very limited
 *       i.e. it should be feasible to cycle through options
 *       without a keyboard."
 */
#define RETRO_NUM_CORE_OPTION_VALUES_MAX 128

struct retro_core_option_value
{
   /* Expected option value */
   const char *value;

   /* Human-readable value label. If NULL, value itself
    * will be displayed by the frontend */
   const char *label;
};

struct retro_core_option_definition
{
   /* Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE. */
   const char *key;

   /* Human-readable core option description (used as menu label) */
   const char *desc;

   /* Human-readable core option information (used as menu sublabel) */
   const char *info;

   /* Array of retro_core_option_value structs, terminated by NULL */
   struct retro_core_option_value values[RETRO_NUM_CORE_OPTION_VALUES_MAX];

   /* Default core option value. Must match one of the values
    * in the retro_core_option_value array, otherwise will be
    * ignored */
   const char *default_value;
};

struct retro_core_options_intl
{
   /* Pointer to an array of retro_core_option_definition structs
    * - US English implementation
    * - Must point to a valid array */
   struct retro_core_option_definition *us;

   /* Pointer to an array of retro_core_option_definition structs
    * - Implementation for current frontend language
    * - May be NULL */
   struct retro_core_option_definition *local;
};

struct retro_game_info
{
   const char *path;       /* Path to game, UTF-8 encoded.
                            * Sometimes used as a reference for building other paths.
                            * May be NULL if game was loaded from stdin or similar,
                            * but in this case some cores will be unable to load `data`.
                            * So, it is preferable to fabricate something here instead
                            * of passing NULL, which will help more cores to succeed.
                            * retro_system_info::need_fullpath requires
                            * that this path is valid. */
   const void *data;       /* Memory buffer of loaded game. Will be NULL
                            * if need_fullpath was set. */
   size_t      size;       /* Size of memory buffer. */
   const char *meta;       /* String of implementation specific meta-data. */
};

#define RETRO_MEMORY_ACCESS_WRITE (1 << 0)
   /* The core will write to the buffer provided by retro_framebuffer::data. */
#define RETRO_MEMORY_ACCESS_READ (1 << 1)
   /* The core will read from retro_framebuffer::data. */
#define RETRO_MEMORY_TYPE_CACHED (1 << 0)
   /* The memory in data is cached.
    * If not cached, random writes and/or reading from the buffer is expected to be very slow. */
struct retro_framebuffer
{
   void *data;                      /* The framebuffer which the core can render into.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER.
                                       The initial contents of data are unspecified. */
   unsigned width;                  /* The framebuffer width used by the core. Set by core. */
   unsigned height;                 /* The framebuffer height used by the core. Set by core. */
   size_t pitch;                    /* The number of bytes between the beginning of a scanline,
                                       and beginning of the next scanline.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER. */
   enum retro_pixel_format format;  /* The pixel format the core must use to render into data.
                                       This format could differ from the format used in
                                       SET_PIXEL_FORMAT.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER. */

   unsigned access_flags;           /* How the core will access the memory in the framebuffer.
                                       RETRO_MEMORY_ACCESS_* flags.
                                       Set by core. */
   unsigned memory_flags;           /* Flags telling core how the memory has been mapped.
                                       RETRO_MEMORY_TYPE_* flags.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER. */
};

/* Callbacks */

/* Environment callback. Gives implementations a way of performing
 * uncommon tasks. Extensible. */
typedef bool (RETRO_CALLCONV *retro_environment_t)(unsigned cmd, void *data);

/* Render a frame. Pixel format is 15-bit 0RGB1555 native endian
 * unless changed (see RETRO_ENVIRONMENT_SET_PIXEL_FORMAT).
 *
 * Width and height specify dimensions of buffer.
 * Pitch specifices length in bytes between two lines in buffer.
 *
 * For performance reasons, it is highly recommended to have a frame
 * that is packed in memory, i.e. pitch == width * byte_per_pixel.
 * Certain graphic APIs, such as OpenGL ES, do not like textures
 * that are not packed in memory.
 */
typedef void (RETRO_CALLCONV *retro_video_refresh_t)(const void *data, unsigned width,
      unsigned height, size_t pitch);

/* Renders a single audio frame. Should only be used if implementation
 * generates a single sample at a time.
 * Format is signed 16-bit native endian.
 */
typedef void (RETRO_CALLCONV *retro_audio_sample_t)(int16_t left, int16_t right);

/* Renders multiple audio frames in one go.
 *
 * One frame is defined as a sample of left and right channels, interleaved.
 * I.e. int16_t buf[4] = { l, r, l, r }; would be 2 frames.
 * Only one of the audio callbacks must ever be used.
 */
typedef size_t (RETRO_CALLCONV *retro_audio_sample_batch_t)(const int16_t *data,
      size_t frames);

/* Polls input. */
typedef void (RETRO_CALLCONV *retro_input_poll_t)(void);

/* Queries for input for player 'port'. device will be masked with
 * RETRO_DEVICE_MASK.
 *
 * Specialization of devices such as RETRO_DEVICE_JOYPAD_MULTITAP that
 * have been set with retro_set_controller_port_device()
 * will still use the higher level RETRO_DEVICE_JOYPAD to request input.
 */
typedef int16_t (RETRO_CALLCONV *retro_input_state_t)(unsigned port, unsigned device,
      unsigned index, unsigned id);

/* Sets callbacks. retro_set_environment() is guaranteed to be called
 * before retro_init().
 *
 * The rest of the set_* functions are guaranteed to have been called
 * before the first call to retro_run() is made. */
RETRO_API void retro_set_environment(retro_environment_t);
RETRO_API void retro_set_video_refresh(retro_video_refresh_t);
RETRO_API void retro_set_audio_sample(retro_audio_sample_t);
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
RETRO_API void retro_set_input_poll(retro_input_poll_t);
RETRO_API void retro_set_input_state(retro_input_state_t);

/* Library global initialization/deinitialization. */
RETRO_API void retro_init(void);
RETRO_API void retro_deinit(void);

/* Must return RETRO_API_VERSION. Used to validate ABI compatibility
 * when the API is revised. */
RETRO_API unsigned retro_api_version(void);

/* Gets statically known system info. Pointers provided in *info
 * must be statically allocated.
 * Can be called at any time, even before retro_init(). */
RETRO_API void retro_get_system_info(struct retro_system_info *info);

/* Gets information about system audio/video timings and geometry.
 * Can be called only after retro_load_game() has successfully completed.
 * NOTE: The implementation of this function might not initialize every
 * variable if needed.
 * E.g. geom.aspect_ratio might not be initialized if core doesn't
 * desire a particular aspect ratio. */
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info);

/* Sets device to be used for player 'port'.
 * By default, RETRO_DEVICE_JOYPAD is assumed to be plugged into all
 * available ports.
 * Setting a particular device type is not a guarantee that libretro cores
 * will only poll input based on that particular device type. It is only a
 * hint to the libretro core when a core cannot automatically detect the
 * appropriate input device type on its own. It is also relevant when a
 * core can change its behavior depending on device type.
 *
 * As part of the core's implementation of retro_set_controller_port_device,
 * the core should call RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS to notify the
 * frontend if the descriptions for any controls have changed as a
 * result of changing the device type.
 */
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device);

/* Resets the current game. */
RETRO_API void retro_reset(void);

/* Runs the game for one video frame.
 * During retro_run(), input_poll callback must be called at least once.
 *
 * If a frame is not rendered for reasons where a game "dropped" a frame,
 * this still counts as a frame, and retro_run() should explicitly dupe
 * a frame if GET_CAN_DUPE returns true.
 * In this case, the video callback can take a NULL argument for data.
 */
RETRO_API void retro_run(void);

/* Returns the amount of data the implementation requires to serialize
 * internal state (save states).
 * Between calls to retro_load_game() and retro_unload_game(), the
 * returned size is never allowed to be larger than a previous returned
 * value, to ensure that the frontend can allocate a save state buffer once.
 */
RETRO_API size_t retro_serialize_size(void);

/* Serializes internal state. If failed, or size is lower than
 * retro_serialize_size(), it should return false, true otherwise. */
RETRO_API bool retro_serialize(void *data, size_t size);
RETRO_API bool retro_unserialize(const void *data, size_t size);

RETRO_API void retro_cheat_reset(void);
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code);

/* Loads a game.
 * Return true to indicate successful loading and false to indicate load failure.
 */
RETRO_API bool retro_load_game(const struct retro_game_info *game);

/* Loads a "special" kind of game. Should not be used,
 * except in extreme cases. */
RETRO_API bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
);

/* Unloads the currently loaded game. Called before retro_deinit(void). */
RETRO_API void retro_unload_game(void);

/* Gets region of game. */
RETRO_API unsigned retro_get_region(void);

/* Gets region of memory. */
RETRO_API void *retro_get_memory_data(unsigned id);
RETRO_API size_t retro_get_memory_size(unsigned id);

#ifdef __cplusplus
}
#endif

#endif
