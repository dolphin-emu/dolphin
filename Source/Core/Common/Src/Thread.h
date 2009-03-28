// Copyright (C) 2003-2009 Dolphin Project.

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
#include <windows.h>
#define THREAD_RETURN DWORD WINAPI
#else
#define THREAD_RETURN void*
#include <unistd.h>
#ifdef _POSIX_THREADS
#include <pthread.h>
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

// -----------------------------------------
#ifdef SETUP_TIMER_WAITING
// -----------------
	typedef void (*EventCallBack)(void);
#endif
// ----------------------
///////////////////////////////////


namespace Common
{
class CriticalSection
{
#ifdef _WIN32
	CRITICAL_SECTION section;
#else
	pthread_mutex_t mutex;
#endif
public:

	CriticalSection(int spincount = 1000);
	~CriticalSection();
	void Enter();
	bool TryEnter();
	void Leave();
};

#ifdef _WIN32
typedef DWORD (WINAPI * ThreadFunc)(void* arg);
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
#ifdef _WIN32
	void WaitForDeath(const int _Wait = INFINITE);
#else
	void WaitForDeath();
#endif

private:

#ifdef _WIN32
	HANDLE m_hThread;
	DWORD m_threadId;
#else
	pthread_t thread_id;
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

	#ifdef SETUP_TIMER_WAITING
		bool TimerWait(EventCallBack WaitCB, int Id = 0, bool OptCondition = true);
		bool DoneWait();
		void SetTimer();
		bool DoneWaiting;
		bool StartWait;
		int Id;
		HANDLE hTimer;
		HANDLE hTimerQueue;		
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
	pthread_cond_t event_;
	pthread_mutex_t mutex_;
#endif
};

void InitThreading();
void SleepCurrentThread(int ms);

void SetCurrentThreadName(const char *name);

LONG SyncInterlockedExchangeAdd(LONG *Dest, LONG Val);
LONG SyncInterlockedExchange(LONG *Dest, LONG Val);
LONG SyncInterlockedIncrement(LONG *Dest);

} // namespace Common

#endif // _STRINGUTIL_H_
