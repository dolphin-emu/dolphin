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

#include "Common.h"

#ifdef _WIN32
#include <windows.h>
#elif __GNUC__
#include <unistd.h>
#include <pthread.h>
#else
#error unsupported platform
#endif

#include "Thread.h"

namespace Common
{
#ifdef _WIN32
CriticalSection::CriticalSection(int spincount)
{
	if (spincount)
	{
		InitializeCriticalSectionAndSpinCount(&section, spincount);
	}
	else
	{
		InitializeCriticalSection(&section);
	}
}


CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&section);
}


void CriticalSection::Enter()
{
	EnterCriticalSection(&section);
}


bool CriticalSection::TryEnter()
{
	return(TryEnterCriticalSection(&section) ? true : false);
}


void CriticalSection::Leave()
{
	LeaveCriticalSection(&section);
}


Thread::Thread(ThreadFunc function, void* arg)
	: m_hThread(NULL), m_threadId(0)
{
	m_hThread = CreateThread(
			0, // Security attributes
			0, // Stack size
			function,
			arg,
			0,
			&m_threadId);
}


Thread::~Thread()
{
	WaitForDeath();
}


void Thread::WaitForDeath()
{
	if (m_hThread)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
}


void Thread::SetAffinity(int mask)
{
	SetThreadAffinityMask(m_hThread, mask);
}


void Thread::SetCurrentThreadAffinity(int mask)
{
	SetThreadAffinityMask(GetCurrentThread(), mask);
}


Event::Event()
{
	m_hEvent = 0;
}


void Event::Init()
{
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}


void Event::Shutdown()
{
	CloseHandle(m_hEvent);
	m_hEvent = 0;
}


void Event::Set()
{
	SetEvent(m_hEvent);
}


void Event::Wait()
{
	WaitForSingleObject(m_hEvent, INFINITE);
}


void SleepCurrentThread(int ms)
{
	Sleep(ms);
}


typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // must be 0x1000
	LPCSTR szName; // pointer to name (in user addr space)
	DWORD dwThreadID; // thread ID (-1=caller thread)
	DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;
// Usage: SetThreadName (-1, "MainThread");
//
// Sets the debugger-visible name of the current thread.
// Uses undocumented (actually, it is now documented) trick.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vsdebug/html/vxtsksettingthreadname.asp

void SetCurrentThreadName(const TCHAR* szThreadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
#ifdef UNICODE
	//TODO: Find the proper way to do this.
	char tname[256];
	unsigned int i;

	for (i = 0; i < _tcslen(szThreadName); i++)
	{
		tname[i] = (char)szThreadName[i]; //poor man's unicode->ansi, TODO: fix
	}

	tname[i] = 0;
	info.szName = tname;
#else
	info.szName = szThreadName;
#endif

	info.dwThreadID = -1; //dwThreadID;
	info.dwFlags = 0;
	__try
	{
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (ULONG_PTR*)&info);
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{}
}


#elif __GNUC__
CriticalSection::CriticalSection(int spincount_unused)
{
	pthread_mutex_init(&mutex, 0);
}


CriticalSection::~CriticalSection()
{
	pthread_mutex_destroy(&mutex);
}


void CriticalSection::Enter()
{
	pthread_mutex_lock(&mutex);
}


bool CriticalSection::TryEnter()
{
	return(!pthread_mutex_trylock(&mutex));
}


void CriticalSection::Leave()
{
	pthread_mutex_unlock(&mutex);
}


Thread::Thread(ThreadFunc function, void* arg)
	: thread_id(0)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);
	pthread_create(&thread_id, &attr, function, arg);
}


Thread::~Thread()
{
	WaitForDeath();
}


void Thread::WaitForDeath()
{
	if (thread_id)
	{
		void* exit_status;
		pthread_join(thread_id, &exit_status);
		thread_id = 0;
	}
}


void Thread::SetAffinity(int mask)
{
	// This is non-standard
#ifdef __linux__
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);

	for (unsigned int i = 0; i < sizeof(mask) * 8; i++)
	{
		if ((mask >> i) & 1){CPU_SET(i, &cpu_set);}
	}

	pthread_setaffinity_np(thread_id, sizeof(cpu_set), &cpu_set);
#endif
}


void Thread::SetCurrentThreadAffinity(int mask)
{
#ifdef __linux__
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);

	for (size_t i = 0; i < sizeof(mask) * 8; i++)
	{
		if ((mask >> i) & 1){CPU_SET(i, &cpu_set);}
	}

	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
#endif
}


void SleepCurrentThread(int ms)
{
	usleep(1000 * ms);
}


void SetCurrentThreadName(const TCHAR* szThreadName)
{
	// noop
}


Event::Event()
{
	is_set_ = false;
}


void Event::Init()
{
	pthread_cond_init(&event_, 0);
	pthread_mutex_init(&mutex_, 0);
}


void Event::Shutdown()
{
	pthread_mutex_destroy(&mutex_);
	pthread_cond_destroy(&event_);
}


void Event::Set()
{
	pthread_mutex_lock(&mutex_);

	if (!is_set_)
	{
		is_set_ = true;
		pthread_cond_signal(&event_);
	}

	pthread_mutex_unlock(&mutex_);
}


void Event::Wait()
{
	pthread_mutex_lock(&mutex_);

	while (!is_set_)
	{
		pthread_cond_wait(&event_, &mutex_);
	}

	is_set_ = false;
	pthread_mutex_unlock(&mutex_);
}
#endif


} // end of namespace Common
