// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _THREAD_H_
#define _THREAD_H_

#ifdef _WIN32

#if defined(_MSC_VER) && defined(_MT)
// When linking with LIBCMT (the multithreaded C library), Microsoft recommends
// using _beginthreadex instead of CreateThread.
#define USE_BEGINTHREADEX
#endif

#include <windows.h>

#ifdef USE_BEGINTHREADEX
#define THREAD_RETURN unsigned __stdcall
#else
#define THREAD_RETURN DWORD WINAPI
#endif

#else

#include <xmmintrin.h>
#define THREAD_RETURN void*
#include <unistd.h>
#ifdef _POSIX_THREADS
#include <pthread.h>
#elif GEKKO
#include <ogc/lwp_threads.h>
#else
#error unsupported platform (no pthreads?)
#endif

#endif

// Don't include common.h here as it will break LogManager
#include "CommonTypes.h"
#include <stdio.h>
#include <string.h>

// This may not be defined outside _WIN32
#ifndef _WIN32
	#ifndef INFINITE
	#define INFINITE 0xffffffff
	#endif
#endif


namespace Common
{

class CriticalSection
{
#ifdef _WIN32
	CRITICAL_SECTION section;
#else
#ifdef _POSIX_THREADS
	pthread_mutex_t mutex;
#endif
#endif
public:

	CriticalSection(int spincount = 1000);
	~CriticalSection();
	void Enter();
	bool TryEnter();
	void Leave();
};

#ifdef _WIN32

#ifdef USE_BEGINTHREADEX
typedef unsigned (__stdcall *ThreadFunc)(void* arg);
#else
typedef DWORD (WINAPI *ThreadFunc)(void* arg);
#endif

#else

typedef void* (*ThreadFunc)(void* arg);

#endif

class Thread
{
public:
	Thread(ThreadFunc entry, void* arg);
	~Thread();

	void SetAffinity(int mask);
	static void SetCurrentThreadAffinity(int mask);
	static int CurrentId();
#ifdef _WIN32	
	void SetPriority(int priority);
	DWORD WaitForDeath(const int iWait = INFINITE);
#else
	void WaitForDeath();
#endif

private:

#ifdef _WIN32

	HANDLE m_hThread;
#ifdef USE_BEGINTHREADEX
	unsigned m_threadId;
#else
	DWORD m_threadId;
#endif

#else

#ifdef _POSIX_THREADS
	pthread_t thread_id;
#endif

#endif
};



class Event
{
public:
	Event();

	void Init();
	void Shutdown();

	void Set();
	void Wait();
#ifdef _WIN32
	void MsgWait();
#else
	void MsgWait() {Wait();}
#endif


private:
#ifdef _WIN32

	HANDLE m_hEvent;
	/* If we have waited more than five seconds we can be pretty sure that the thread is deadlocked.
	   So then we can just as well continue and hope for the best. I could try several times that
	   this works after a five second timeout (with works meaning that the game stopped and I could
	   start another game without any noticable problems). But several times it failed to, and ended
	   with a crash. But it's better than an infinite deadlock. */
	static const int THREAD_WAIT_TIMEOUT = 5000; // INFINITE or 5000 for example

#else

	bool is_set_;
#ifdef _POSIX_THREADS
	pthread_cond_t event_;
	pthread_mutex_t mutex_;
#endif

#endif
};

void InitThreading();
void SleepCurrentThread(int ms);

// YieldCPU: Use this function during a spin-wait to make the current thread
// relax while another thread is working. This may be more efficient than using
// events because event functions use kernel calls.
inline void YieldCPU()
{
#ifdef _WIN32
	YieldProcessor();
#elif defined(_M_IX86) || defined(_M_X64)
	_mm_pause();
#endif
}

void SetCurrentThreadName(const char *name);

} // namespace Common

#endif // _THREAD_H_
