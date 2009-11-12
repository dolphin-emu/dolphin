/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

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

    Contributed by Bob Pendleton, bob@pendleton.com
 */

/**
 *  \file SDL_atomic.h
 *  
 *  Atomic operations.
 *  
 *  These operations may, or may not, actually be implemented using
 *  processor specific atomic operations. When possible they are
 *  implemented as true processor specific atomic operations. When that
 *  is not possible the are implemented using locks that *do* use the
 *  available atomic operations.
 *  
 *  At the very minimum spin locks must be implemented. Without spin
 *  locks it is not possible (AFAICT) to emulate the rest of the atomic
 *  operations.
 */

#ifndef _SDL_atomic_h_
#define _SDL_atomic_h_

#include "SDL_stdinc.h"
#include "SDL_platform.h"

#include "begin_code.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* Function prototypes */

/**
 *  \name SDL AtomicLock
 *  
 *  The spin lock functions and type are required and can not be
 *  emulated because they are used in the emulation code.
 */
/*@{*/

typedef volatile Uint32 SDL_SpinLock;

/**
 *  \brief Lock a spin lock by setting it to a none zero value.
 *  
 *  \param lock Points to the lock.
 */
extern DECLSPEC void SDLCALL SDL_AtomicLock(SDL_SpinLock *lock);

/**
 *  \brief Unlock a spin lock by setting it to 0. Always returns immediately
 *
 *  \param lock Points to the lock.
 */
extern DECLSPEC void SDLCALL SDL_AtomicUnlock(SDL_SpinLock *lock);

/*@}*//*SDL AtomicLock*/

/**
 *  \name 32 bit atomic operations
 */
/*@{*/

/**
 *  \brief Check to see if \c *ptr == 0 and set it to 1.
 *  
 *  \return SDL_True if the value pointed to by \c ptr was zero and
 *          SDL_False if it was not zero
 *  
 *  \param ptr Points to the value to be tested and set.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_AtomicTestThenSet32(volatile Uint32 * ptr);

/**
 *  \brief Set the value pointed to by \c ptr to be zero.
 *  
 *  \param ptr Address of the value to be set to zero
 */
extern DECLSPEC void SDLCALL SDL_AtomicClear32(volatile Uint32 * ptr);

/**
 *  \brief Fetch the current value of \c *ptr and then increment that
 *         value in place.
 *  
 *  \return The value before it was incremented.
 *  
 *  \param ptr Address of the value to fetch and increment
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicFetchThenIncrement32(volatile Uint32 * ptr);

/**
 *  \brief Fetch \c *ptr and then decrement the value in place.
 *  
 *  \return The value before it was decremented.
 *  
 *  \param ptr Address of the value to fetch and drement
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicFetchThenDecrement32(volatile Uint32 * ptr);

/**
 *  \brief Fetch the current value at \c ptr and then add \c value to \c *ptr.
 *  
 *  \return \c *ptr before the addition took place.
 *  
 *  \param ptr The address of data we are changing.
 *  \param value The value to add to \c *ptr. 
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicFetchThenAdd32(volatile Uint32 * ptr, Uint32 value);

/**
 *  \brief Fetch \c *ptr and then subtract \c value from it.
 *  
 *  \return \c *ptr before the subtraction took place.
 *  
 *  \param ptr The address of the data being changed.
 *  \param value The value to subtract from \c *ptr.
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicFetchThenSubtract32(volatile Uint32 * ptr, Uint32 value);

/**
 *  \brief Add one to the data pointed to by \c ptr and return that value.
 *  
 *  \return The incremented value.
 *  
 *  \param ptr The address of the data to increment.
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicIncrementThenFetch32(volatile Uint32 * ptr);

/**
 *  \brief Subtract one from data pointed to by \c ptr and return the new value.
 *  
 *  \return The decremented value.
 *  
 *  \param ptr The address of the data to decrement.
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicDecrementThenFetch32(volatile Uint32 * ptr);

/**
 *  \brief Add \c value to the data pointed to by \c ptr and return result.
 *  
 *  \return The sum of \c *ptr and \c value.
 *  
 *  \param ptr The address of the data to be modified.
 *  \param value The value to be added.
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicAddThenFetch32(volatile Uint32 * ptr, Uint32 value);

/**
 *  \brief Subtract \c value from the data pointed to by \c ptr and return the result.
 *  
 *  \return The difference between \c *ptr and \c value.
 *  
 *  \param ptr The address of the data to be modified.
 *  \param value The value to be subtracted.
 */
extern DECLSPEC Uint32 SDLCALL SDL_AtomicSubtractThenFetch32(volatile Uint32 * ptr, Uint32 value);

/*@}*//*32 bit atomic operations*/

/**
 *  \name 64 bit atomic operations
 */
/*@{*/
#ifdef SDL_HAS_64BIT_TYPE

extern DECLSPEC SDL_bool SDLCALL SDL_AtomicTestThenSet64(volatile Uint64 * ptr);
extern DECLSPEC void SDLCALL SDL_AtomicClear64(volatile Uint64 * ptr);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicFetchThenIncrement64(volatile Uint64 * ptr);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicFetchThenDecrement64(volatile Uint64 * ptr);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicFetchThenAdd64(volatile Uint64 * ptr, Uint64 value);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicFetchThenSubtract64(volatile Uint64 * ptr, Uint64 value);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicIncrementThenFetch64(volatile Uint64 * ptr);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicDecrementThenFetch64(volatile Uint64 * ptr);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicAddThenFetch64(volatile Uint64 * ptr, Uint64 value);
extern DECLSPEC Uint64 SDLCALL SDL_AtomicSubtractThenFetch64(volatile Uint64 * ptr, Uint64 value);
#endif /*  SDL_HAS_64BIT_TYPE */

/*@}*//*64 bit atomic operations*/

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#include "close_code.h"

#endif /* _SDL_atomic_h_ */

/* vi: set ts=4 sw=4 expandtab: */
