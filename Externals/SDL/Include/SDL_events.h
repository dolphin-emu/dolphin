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
 * \file SDL_events.h
 *
 * Include file for SDL event handling
 */

#ifndef _SDL_events_h
#define _SDL_events_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_joystick.h"
#include "SDL_quit.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* General keyboard/mouse state definitions */
#define SDL_RELEASED	0
#define SDL_PRESSED	1

/**
 * \enum SDL_EventType
 *
 * \brief The types of events that can be delivered
 */
typedef enum
{
    SDL_NOEVENT = 0,            /**< Unused (do not remove) */
    SDL_WINDOWEVENT,            /**< Window state change */
    SDL_KEYDOWN,                /**< Keys pressed */
    SDL_KEYUP,                  /**< Keys released */
    SDL_TEXTINPUT,              /**< Keyboard text input */
    SDL_MOUSEMOTION,            /**< Mouse moved */
    SDL_MOUSEBUTTONDOWN,        /**< Mouse button pressed */
    SDL_MOUSEBUTTONUP,          /**< Mouse button released */
    SDL_MOUSEWHEEL,             /**< Mouse wheel motion */
    SDL_JOYAXISMOTION,          /**< Joystick axis motion */
    SDL_JOYBALLMOTION,          /**< Joystick trackball motion */
    SDL_JOYHATMOTION,           /**< Joystick hat position change */
    SDL_JOYBUTTONDOWN,          /**< Joystick button pressed */
    SDL_JOYBUTTONUP,            /**< Joystick button released */
    SDL_QUIT,                   /**< User-requested quit */
    SDL_SYSWMEVENT,             /**< System specific event */
    SDL_PROXIMITYIN,            /**< Proximity In event */
    SDL_PROXIMITYOUT,           /**< Proximity Out event */
    SDL_EVENT_RESERVED1,        /**< Reserved for future use... */
    SDL_EVENT_RESERVED2,
    SDL_EVENT_RESERVED3,
    /* Events SDL_USEREVENT through SDL_MAXEVENTS-1 are for your use */
    SDL_USEREVENT = 24,
    /* This last event is only for bounding internal arrays
       It is the number of bits in the event mask datatype -- Uint32
     */
    SDL_NUMEVENTS = 32
} SDL_EventType;

/**
 * \enum SDL_EventMask
 *
 * \brief Predefined event masks
 */
#define SDL_EVENTMASK(X)	(1<<(X))
typedef enum
{
    SDL_WINDOWEVENTMASK = SDL_EVENTMASK(SDL_WINDOWEVENT),
    SDL_KEYDOWNMASK = SDL_EVENTMASK(SDL_KEYDOWN),
    SDL_KEYUPMASK = SDL_EVENTMASK(SDL_KEYUP),
    SDL_KEYEVENTMASK = SDL_EVENTMASK(SDL_KEYDOWN) | SDL_EVENTMASK(SDL_KEYUP),
    SDL_TEXTINPUTMASK = SDL_EVENTMASK(SDL_TEXTINPUT),
    SDL_MOUSEMOTIONMASK = SDL_EVENTMASK(SDL_MOUSEMOTION),
    SDL_MOUSEBUTTONDOWNMASK = SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN),
    SDL_MOUSEBUTTONUPMASK = SDL_EVENTMASK(SDL_MOUSEBUTTONUP),
    SDL_MOUSEWHEELMASK = SDL_EVENTMASK(SDL_MOUSEWHEEL),
    SDL_MOUSEEVENTMASK = SDL_EVENTMASK(SDL_MOUSEMOTION) |
        SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN) | SDL_EVENTMASK(SDL_MOUSEBUTTONUP),
    SDL_JOYAXISMOTIONMASK = SDL_EVENTMASK(SDL_JOYAXISMOTION),
    SDL_JOYBALLMOTIONMASK = SDL_EVENTMASK(SDL_JOYBALLMOTION),
    SDL_JOYHATMOTIONMASK = SDL_EVENTMASK(SDL_JOYHATMOTION),
    SDL_JOYBUTTONDOWNMASK = SDL_EVENTMASK(SDL_JOYBUTTONDOWN),
    SDL_JOYBUTTONUPMASK = SDL_EVENTMASK(SDL_JOYBUTTONUP),
    SDL_JOYEVENTMASK = SDL_EVENTMASK(SDL_JOYAXISMOTION) |
        SDL_EVENTMASK(SDL_JOYBALLMOTION) |
        SDL_EVENTMASK(SDL_JOYHATMOTION) |
        SDL_EVENTMASK(SDL_JOYBUTTONDOWN) | SDL_EVENTMASK(SDL_JOYBUTTONUP),
    SDL_QUITMASK = SDL_EVENTMASK(SDL_QUIT),
    SDL_SYSWMEVENTMASK = SDL_EVENTMASK(SDL_SYSWMEVENT),
    SDL_PROXIMITYINMASK = SDL_EVENTMASK(SDL_PROXIMITYIN),
    SDL_PROXIMITYOUTMASK = SDL_EVENTMASK(SDL_PROXIMITYOUT)
} SDL_EventMask;
#define SDL_ALLEVENTS		0xFFFFFFFF

/**
 * \struct SDL_WindowEvent
 *
 * \brief Window state change event data (event.window.*)
 */
typedef struct SDL_WindowEvent
{
    Uint8 type;             /**< SDL_WINDOWEVENT */
    Uint8 event;            /**< SDL_WindowEventID */
    int data1;              /**< event dependent data */
    int data2;              /**< event dependent data */
    SDL_WindowID windowID;  /**< The associated window */
} SDL_WindowEvent;

/**
 * \struct SDL_KeyboardEvent
 *
 * \brief Keyboard button event structure (event.key.*)
 */
typedef struct SDL_KeyboardEvent
{
    Uint8 type;             /**< SDL_KEYDOWN or SDL_KEYUP */
    Uint8 which;            /**< The keyboard device index */
    Uint8 state;            /**< SDL_PRESSED or SDL_RELEASED */
    SDL_keysym keysym;      /**< The key that was pressed or released */
    SDL_WindowID windowID;  /**< The window with keyboard focus, if any */
} SDL_KeyboardEvent;

/**
 * \struct SDL_TextInputEvent
 *
 * \brief Keyboard text input event structure (event.text.*)
 */
#define SDL_TEXTINPUTEVENT_TEXT_SIZE (32)
typedef struct SDL_TextInputEvent
{
    Uint8 type;                               /**< SDL_TEXTINPUT */
    Uint8 which;                              /**< The keyboard device index */
    char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];  /**< The input text */
    SDL_WindowID windowID;                    /**< The window with keyboard focus, if any */
} SDL_TextInputEvent;

/**
 * \struct SDL_MouseMotionEvent
 *
 * \brief Mouse motion event structure (event.motion.*)
 */
typedef struct SDL_MouseMotionEvent
{
    Uint8 type;             /**< SDL_MOUSEMOTION */
    Uint8 which;            /**< The mouse device index */
    Uint8 state;            /**< The current button state */
    int x;                  /**< X coordinate, relative to window */
    int y;                  /**< Y coordinate, relative to window */
    int z;                  /**< Z coordinate, for future use */
    int pressure;           /**< Pressure reported by tablets */
    int pressure_max;       /**< Maximum value of the pressure reported by the device */
    int pressure_min;       /**< Minimum value of the pressure reported by the device */
    int rotation;           /**< For future use */
    int tilt;               /**< For future use */
    int cursor;             /**< The cursor being used in the event */
    int xrel;               /**< The relative motion in the X direction */
    int yrel;               /**< The relative motion in the Y direction */
    SDL_WindowID windowID;  /**< The window with mouse focus, if any */
} SDL_MouseMotionEvent;

/**
 * \struct SDL_MouseButtonEvent
 *
 * \brief Mouse button event structure (event.button.*)
 */
typedef struct SDL_MouseButtonEvent
{
    Uint8 type;             /**< SDL_MOUSEBUTTONDOWN or SDL_MOUSEBUTTONUP */
    Uint8 which;            /**< The mouse device index */
    Uint8 button;           /**< The mouse button index */
    Uint8 state;            /**< SDL_PRESSED or SDL_RELEASED */
    int x;                  /**< X coordinate, relative to window */
    int y;                  /**< Y coordinate, relative to window */
    SDL_WindowID windowID;  /**< The window with mouse focus, if any */
} SDL_MouseButtonEvent;

/**
 * \struct SDL_MouseWheelEvent
 *
 * \brief Mouse wheel event structure (event.wheel.*)
 */
typedef struct SDL_MouseWheelEvent
{
    Uint8 type;             /**< SDL_MOUSEWHEEL */
    Uint8 which;            /**< The mouse device index */
    int x;                  /**< The amount scrolled horizontally */
    int y;                  /**< The amount scrolled vertically */
    SDL_WindowID windowID;  /**< The window with mouse focus, if any */
} SDL_MouseWheelEvent;

/**
 * \struct SDL_JoyAxisEvent
 *
 * \brief Joystick axis motion event structure (event.jaxis.*)
 */
typedef struct SDL_JoyAxisEvent
{
    Uint8 type;         /**< SDL_JOYAXISMOTION */
    Uint8 which;        /**< The joystick device index */
    Uint8 axis;         /**< The joystick axis index */
    int value;          /**< The axis value (range: -32768 to 32767) */
} SDL_JoyAxisEvent;

/**
 * \struct SDL_JoyBallEvent
 *
 * \brief Joystick trackball motion event structure (event.jball.*)
 */
typedef struct SDL_JoyBallEvent
{
    Uint8 type;         /**< SDL_JOYBALLMOTION */
    Uint8 which;        /**< The joystick device index */
    Uint8 ball;         /**< The joystick trackball index */
    int xrel;           /**< The relative motion in the X direction */
    int yrel;           /**< The relative motion in the Y direction */
} SDL_JoyBallEvent;

/**
 * \struct SDL_JoyHatEvent
 *
 * \brief Joystick hat position change event structure (event.jhat.*)
 */
typedef struct SDL_JoyHatEvent
{
    Uint8 type;         /**< SDL_JOYHATMOTION */
    Uint8 which;        /**< The joystick device index */
    Uint8 hat;          /**< The joystick hat index */
    Uint8 value;        /**< The hat position value:
                             SDL_HAT_LEFTUP   SDL_HAT_UP       SDL_HAT_RIGHTUP
                             SDL_HAT_LEFT     SDL_HAT_CENTERED SDL_HAT_RIGHT
                             SDL_HAT_LEFTDOWN SDL_HAT_DOWN     SDL_HAT_RIGHTDOWN
                             Note that zero means the POV is centered.
                         */
} SDL_JoyHatEvent;

/**
 * \struct SDL_JoyButtonEvent
 *
 * \brief Joystick button event structure (event.jbutton.*)
 */
typedef struct SDL_JoyButtonEvent
{
    Uint8 type;         /**< SDL_JOYBUTTONDOWN or SDL_JOYBUTTONUP */
    Uint8 which;        /**< The joystick device index */
    Uint8 button;       /**< The joystick button index */
    Uint8 state;        /**< SDL_PRESSED or SDL_RELEASED */
} SDL_JoyButtonEvent;

/**
 * \struct SDL_QuitEvent
 *
 * \brief The "quit requested" event
 */
typedef struct SDL_QuitEvent
{
    Uint8 type;         /**< SDL_QUIT */
} SDL_QuitEvent;

/**
 * \struct SDL_UserEvent
 *
 * \brief A user-defined event type (event.user.*)
 */
typedef struct SDL_UserEvent
{
    Uint8 type;             /**< SDL_USEREVENT through SDL_NUMEVENTS-1 */
    int code;               /**< User defined event code */
    void *data1;            /**< User defined data pointer */
    void *data2;            /**< User defined data pointer */
    SDL_WindowID windowID;  /**< The associated window if any*/
} SDL_UserEvent;

/**
 * \struct SDL_SysWMEvent
 *
 * \brief A video driver dependent system event (event.syswm.*)
 *
 * \note If you want to use this event, you should include SDL_syswm.h
 */
struct SDL_SysWMmsg;
typedef struct SDL_SysWMmsg SDL_SysWMmsg;
typedef struct SDL_SysWMEvent
{
    Uint8 type;         /**< SDL_SYSWMEVENT */
    SDL_SysWMmsg *msg;  /**< driver dependent data, defined in SDL_syswm.h */
} SDL_SysWMEvent;

/* Typedefs for backwards compatibility */
typedef struct SDL_ActiveEvent
{
    Uint8 type;
    Uint8 gain;
    Uint8 state;
} SDL_ActiveEvent;
typedef struct SDL_ResizeEvent
{
    Uint8 type;
    int w;
    int h;
} SDL_ResizeEvent;

typedef struct SDL_ProximityEvent
{
    Uint8 type;
    Uint8 which;
    int cursor;
    int x;
    int y;
} SDL_ProximityEvent;

/**
 * \union SDL_Event
 *
 * \brief General event structure
 */
typedef union SDL_Event
{
    Uint8 type;                     /**< Event type, shared with all events */
    SDL_WindowEvent window;         /**< Window event data */
    SDL_KeyboardEvent key;          /**< Keyboard event data */
    SDL_TextInputEvent text;        /**< Text input event data */
    SDL_MouseMotionEvent motion;    /**< Mouse motion event data */
    SDL_MouseButtonEvent button;    /**< Mouse button event data */
    SDL_MouseWheelEvent wheel;      /**< Mouse wheel event data */
    SDL_JoyAxisEvent jaxis;         /**< Joystick axis event data */
    SDL_JoyBallEvent jball;         /**< Joystick ball event data */
    SDL_JoyHatEvent jhat;           /**< Joystick hat event data */
    SDL_JoyButtonEvent jbutton;     /**< Joystick button event data */
    SDL_QuitEvent quit;             /**< Quit request event data */
    SDL_UserEvent user;             /**< Custom event data */
    SDL_SysWMEvent syswm;           /**< System dependent window event data */
    SDL_ProximityEvent proximity;   /**< Proximity In or Out event */

    /* Temporarily here for backwards compatibility */
    SDL_ActiveEvent active;
    SDL_ResizeEvent resize;
} SDL_Event;


/* Function prototypes */

/* Pumps the event loop, gathering events from the input devices.
   This function updates the event queue and internal input device state.
   This should only be run in the thread that sets the video mode.
*/
extern DECLSPEC void SDLCALL SDL_PumpEvents(void);

/* Checks the event queue for messages and optionally returns them.
   If 'action' is SDL_ADDEVENT, up to 'numevents' events will be added to
   the back of the event queue.
   If 'action' is SDL_PEEKEVENT, up to 'numevents' events at the front
   of the event queue, matching 'mask', will be returned and will not
   be removed from the queue.
   If 'action' is SDL_GETEVENT, up to 'numevents' events at the front 
   of the event queue, matching 'mask', will be returned and will be
   removed from the queue.
   This function returns the number of events actually stored, or -1
   if there was an error.  This function is thread-safe.
*/
typedef enum
{
    SDL_ADDEVENT,
    SDL_PEEKEVENT,
    SDL_GETEVENT
} SDL_eventaction;
/* */
extern DECLSPEC int SDLCALL SDL_PeepEvents(SDL_Event * events, int numevents,
                                           SDL_eventaction action,
                                           Uint32 mask);

/* Checks to see if certain event types are in the event queue.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasEvent(Uint32 mask);

/* Polls for currently pending events, and returns 1 if there are any pending
   events, or 0 if there are none available.  If 'event' is not NULL, the next
   event is removed from the queue and stored in that area.
 */
extern DECLSPEC int SDLCALL SDL_PollEvent(SDL_Event * event);

/* Waits indefinitely for the next available event, returning 1, or 0 if there
   was an error while waiting for events.  If 'event' is not NULL, the next
   event is removed from the queue and stored in that area.
 */
extern DECLSPEC int SDLCALL SDL_WaitEvent(SDL_Event * event);

/* Add an event to the event queue.
   This function returns 1 on success, 0 if the event was filtered,
   or -1 if the event queue was full or there was some other error.
 */
extern DECLSPEC int SDLCALL SDL_PushEvent(SDL_Event * event);

/*
  This function sets up a filter to process all events before they
  change internal state and are posted to the internal event queue.

  The filter is protypted as:
*/
typedef int (SDLCALL * SDL_EventFilter) (void *userdata, SDL_Event * event);
/*
  If the filter returns 1, then the event will be added to the internal queue.
  If it returns 0, then the event will be dropped from the queue, but the 
  internal state will still be updated.  This allows selective filtering of
  dynamically arriving events.

  WARNING:  Be very careful of what you do in the event filter function, as 
            it may run in a different thread!

  There is one caveat when dealing with the SDL_QUITEVENT event type.  The
  event filter is only called when the window manager desires to close the
  application window.  If the event filter returns 1, then the window will
  be closed, otherwise the window will remain open if possible.
  If the quit event is generated by an interrupt signal, it will bypass the
  internal queue and be delivered to the application at the next event poll.
*/
extern DECLSPEC void SDLCALL SDL_SetEventFilter(SDL_EventFilter filter,
                                                void *userdata);

/*
  Return the current event filter - can be used to "chain" filters.
  If there is no event filter set, this function returns SDL_FALSE.
*/
extern DECLSPEC SDL_bool SDLCALL SDL_GetEventFilter(SDL_EventFilter * filter,
                                                    void **userdata);

/*
  Run the filter function on the current event queue, removing any
  events for which the filter returns 0.
*/
extern DECLSPEC void SDLCALL SDL_FilterEvents(SDL_EventFilter filter,
                                              void *userdata);

/*
  This function allows you to set the state of processing certain events.
  If 'state' is set to SDL_IGNORE, that event will be automatically dropped
  from the event queue and will not event be filtered.
  If 'state' is set to SDL_ENABLE, that event will be processed normally.
  If 'state' is set to SDL_QUERY, SDL_EventState() will return the 
  current processing state of the specified event.
*/
#define SDL_QUERY	-1
#define SDL_IGNORE	 0
#define SDL_DISABLE	 0
#define SDL_ENABLE	 1
extern DECLSPEC Uint8 SDLCALL SDL_EventState(Uint8 type, int state);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_events_h */

/* vi: set ts=4 sw=4 expandtab: */
