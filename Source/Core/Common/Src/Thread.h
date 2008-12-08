// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _THREAD_H
#define _THREAD_H

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
#define THREAD_RETURN DWORD WINAPI
#else
#define THREAD_RETURN void*
#endif

#include "Common.h"


namespace Common
{
class CriticalSection
{
#ifdef _WIN32
	CRITICAL_SECTION section;
#elif __GNUC__
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
#elif __GNUC__
typedef void* (*ThreadFunc)(void* arg);
#endif

class Thread
{
public:
	Thread(ThreadFunc entry, void* arg);
	~Thread();

	void WaitForDeath();
	void SetAffinity(int mask);
	static void SetCurrentThreadAffinity(int mask);


private:

#ifdef _WIN32
	HANDLE m_hThread;
	DWORD m_threadId;
#elif __GNUC__
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


	private:

#ifdef _WIN32
		HANDLE m_hEvent;
#elif __GNUC__
		bool is_set_;
		pthread_cond_t event_;
		pthread_mutex_t mutex_;
#endif
};

void SleepCurrentThread(int ms);

void SetCurrentThreadName(const char *name);

LONG SyncInterlockedExchangeAdd(LONG *Dest, LONG Val);
LONG SyncInterlockedExchange(LONG *Dest, LONG Val);
LONG SyncInterlockedIncrement(LONG *Dest);

} // end of namespace Common


#endif
