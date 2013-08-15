/*
 *  Portable interface to the CPU cycle counter
 *
 *  Copyright (C) 2006-2010, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "polarssl/config.h"

#if defined(POLARSSL_TIMING_C)

#include "polarssl/timing.h"

#if defined(_WIN32)

#include <windows.h>
#include <winbase.h>

struct _hr_time
{
    LARGE_INTEGER start;
};

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

struct _hr_time
{
    struct timeval start;
};

#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&  \
	(defined(_MSC_VER) && defined(_M_IX86)) || defined(__WATCOMC__)

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long tsc;
    __asm   rdtsc
    __asm   mov  [tsc], eax
    return( tsc );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&  \
    defined(__GNUC__) && defined(__i386__)

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long lo, hi;
    asm( "rdtsc" : "=a" (lo), "=d" (hi) );
    return( lo );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&  \
    defined(__GNUC__) && (defined(__amd64__) || defined(__x86_64__))

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long lo, hi;
    asm( "rdtsc" : "=a" (lo), "=d" (hi) ); 
    return( lo | (hi << 32) );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&  \
    defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__))

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long tbl, tbu0, tbu1;

    do
    {
        asm( "mftbu %0" : "=r" (tbu0) );
        asm( "mftb  %0" : "=r" (tbl ) );
        asm( "mftbu %0" : "=r" (tbu1) );
    }
    while( tbu0 != tbu1 );

    return( tbl );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&  \
    defined(__GNUC__) && defined(__sparc64__)

#if defined(__OpenBSD__)
#warning OpenBSD does not allow access to tick register using software version instead
#else
#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long tick;
    asm( "rdpr %%tick, %0;" : "=&r" (tick) );
    return( tick );
}
#endif
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&  \
    defined(__GNUC__) && defined(__sparc__) && !defined(__sparc64__)

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long tick;
    asm( ".byte 0x83, 0x41, 0x00, 0x00" );
    asm( "mov   %%g1, %0" : "=r" (tick) );
    return( tick );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&      \
    defined(__GNUC__) && defined(__alpha__)

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long cc;
    asm( "rpcc %0" : "=r" (cc) );
    return( cc & 0xFFFFFFFF );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(POLARSSL_HAVE_ASM) &&      \
    defined(__GNUC__) && defined(__ia64__)

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    unsigned long itc;
    asm( "mov %0 = ar.itc" : "=r" (itc) );
    return( itc );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK) && defined(_MSC_VER)

#define POLARSSL_HAVE_HARDCLOCK

unsigned long hardclock( void )
{
    LARGE_INTEGER offset;
    
	QueryPerformanceCounter( &offset );

	return (unsigned long)( offset.QuadPart );
}
#endif

#if !defined(POLARSSL_HAVE_HARDCLOCK)

#define POLARSSL_HAVE_HARDCLOCK

static int hardclock_init = 0;
static struct timeval tv_init;

unsigned long hardclock( void )
{
    struct timeval tv_cur;

    if( hardclock_init == 0 )
    {
        gettimeofday( &tv_init, NULL );
        hardclock_init = 1;
    }

    gettimeofday( &tv_cur, NULL );
    return( ( tv_cur.tv_sec  - tv_init.tv_sec  ) * 1000000
          + ( tv_cur.tv_usec - tv_init.tv_usec ) );
}
#endif

volatile int alarmed = 0;

#if defined(_WIN32)

unsigned long get_timer( struct hr_time *val, int reset )
{
    unsigned long delta;
    LARGE_INTEGER offset, hfreq;
    struct _hr_time *t = (struct _hr_time *) val;

    QueryPerformanceCounter(  &offset );
    QueryPerformanceFrequency( &hfreq );

    delta = (unsigned long)( ( 1000 *
        ( offset.QuadPart - t->start.QuadPart ) ) /
           hfreq.QuadPart );

    if( reset )
        QueryPerformanceCounter( &t->start );

    return( delta );
}

DWORD WINAPI TimerProc( LPVOID uElapse )
{   
    Sleep( (DWORD) uElapse );
    alarmed = 1; 
    return( TRUE );
}

void set_alarm( int seconds )
{   
    DWORD ThreadId;

    alarmed = 0; 
    CloseHandle( CreateThread( NULL, 0, TimerProc,
        (LPVOID) ( seconds * 1000 ), 0, &ThreadId ) );
}

void m_sleep( int milliseconds )
{
    Sleep( milliseconds );
}

#else

unsigned long get_timer( struct hr_time *val, int reset )
{
    unsigned long delta;
    struct timeval offset;
    struct _hr_time *t = (struct _hr_time *) val;

    gettimeofday( &offset, NULL );

    delta = ( offset.tv_sec  - t->start.tv_sec  ) * 1000
          + ( offset.tv_usec - t->start.tv_usec ) / 1000;

    if( reset )
    {
        t->start.tv_sec  = offset.tv_sec;
        t->start.tv_usec = offset.tv_usec;
    }

    return( delta );
}

#if defined(INTEGRITY)
void m_sleep( int milliseconds )
{
    usleep( milliseconds * 1000 );
}

#else

static void sighandler( int signum )
{   
    alarmed = 1;
    signal( signum, sighandler );
}

void set_alarm( int seconds )
{
    alarmed = 0;
    signal( SIGALRM, sighandler );
    alarm( seconds );
}

void m_sleep( int milliseconds )
{
    struct timeval tv;

    tv.tv_sec  = milliseconds / 1000;
    tv.tv_usec = milliseconds * 1000;

    select( 0, NULL, NULL, NULL, &tv );
}
#endif /* INTEGRITY */

#endif

#endif
