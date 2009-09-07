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

#include "Setup.h"
#include "Thread.h"
#include "Common.h"

#ifdef USE_BEGINTHREADEX
#include <process.h>
#endif

namespace Common
{

int Thread::CurrentId()
{
	#ifdef _WIN32
		return GetCurrentThreadId();
	#else
		return 0;
	#endif
}

#ifdef _WIN32

void InitThreading()
{
	// Nothing to do in Win32 build.
}

CriticalSection::CriticalSection(int spincount)
{
	if (spincount)
	{
		if (!InitializeCriticalSectionAndSpinCount(&section, spincount))
			ERROR_LOG(COMMON, "CriticalSection could not be initialized!\n%s", GetLastErrorMsg());
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
	return TryEnterCriticalSection(&section) ? true : false;
}

void CriticalSection::Leave()
{
	LeaveCriticalSection(&section);
}

Thread::Thread(ThreadFunc function, void* arg)
	: m_hThread(NULL), m_threadId(0)
{
#ifdef USE_BEGINTHREADEX
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, function, arg, 0, &m_threadId);
#else
	m_hThread = CreateThread(NULL, 0, function, arg, 0, &m_threadId);
#endif
}

Thread::~Thread()
{
	WaitForDeath();
}

DWORD Thread::WaitForDeath(const int iWait)
{
	if (m_hThread)
	{
		DWORD Wait = WaitForSingleObject(m_hThread, iWait);
		CloseHandle(m_hThread);
		m_hThread = NULL;
		return Wait;
	}
	return NULL;
}

void Thread::SetAffinity(int mask)
{
	SetThreadAffinityMask(m_hThread, mask);
}

void Thread::SetPriority(int priority)
{
	SetThreadPriority(m_hThread, priority);
}

void Thread::SetCurrentThreadAffinity(int mask)
{
	SetThreadAffinityMask(GetCurrentThread(), mask);
}

// Regular same thread loop based waiting
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

inline HRESULT MsgWaitForSingleObject(HANDLE handle, DWORD timeout)
{
	return MsgWaitForMultipleObjects(1, &handle, FALSE, timeout, 0);
}

void Event::MsgWait()
{
	// Adapted from MSDN example http://msdn.microsoft.com/en-us/library/ms687060.aspx
	while (true)
	{
		DWORD result; 
		MSG msg; 
		// Read all of the messages in this next loop, 
		// removing each message as we read it.
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{ 
			// If it is a quit message, exit.
			if (msg.message == WM_QUIT)  
				return; 
			// Otherwise, dispatch the message.
			DispatchMessage(&msg); 
		}

		// Wait for any message sent or posted to this queue 
		// or for one of the passed handles be set to signaled.
		result = MsgWaitForSingleObject(m_hEvent, THREAD_WAIT_TIMEOUT); 

		// The result tells us the type of event we have.
		if (result == (WAIT_OBJECT_0 + 1))
		{
			// New messages have arrived. 
			// Continue to the top of the always while loop to 
			// dispatch them and resume waiting.
			continue;
		} 
		else
		{
			// result == WAIT_OBJECT_0
			// Our event got signaled
			return;
		}
	}
}

// Supporting functions
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

// This is implemented much nicer in upcoming msvc++, see:
// http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.100).aspx
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

#else // !WIN32, so must be POSIX threads

pthread_key_t threadname_key;

CriticalSection::CriticalSection(int spincount_unused)
{
	pthread_mutex_init(&mutex, NULL);
}


CriticalSection::~CriticalSection()
{
	pthread_mutex_destroy(&mutex);
}


void CriticalSection::Enter()
{
	int ret = pthread_mutex_lock(&mutex);
	if (ret) ERROR_LOG(COMMON, "%s: pthread_mutex_lock(%p) failed: %s\n", 
					__FUNCTION__, &mutex, strerror(ret));
}


bool CriticalSection::TryEnter()
{
	return(!pthread_mutex_trylock(&mutex));
}


void CriticalSection::Leave()
{
	int ret = pthread_mutex_unlock(&mutex);
	if (ret) ERROR_LOG(COMMON, "%s: pthread_mutex_unlock(%p) failed: %s\n", 
					__FUNCTION__, &mutex, strerror(ret));
}


Thread::Thread(ThreadFunc function, void* arg)
	: thread_id(0)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);
	int ret = pthread_create(&thread_id, &attr, function, arg);
	if (ret) ERROR_LOG(COMMON, "%s: pthread_create(%p, %p, %p, %p) failed: %s\n", 
		__FUNCTION__, &thread_id, &attr, function, arg, strerror(ret));
	
	INFO_LOG(COMMON, "created new thread %lu (func=%p, arg=%p)\n", thread_id, function, arg);
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
		int ret = pthread_join(thread_id, &exit_status);
		if (ret) ERROR_LOG(COMMON, "error joining thread %lu: %s\n", thread_id, strerror(ret));
        if (exit_status)
			ERROR_LOG(COMMON, "thread %lu exited with status %d\n", thread_id, *(int *)exit_status);
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

void InitThreading() {
	static int thread_init_done = 0;
	if (thread_init_done)
		return;
	
	if (pthread_key_create(&threadname_key, NULL/*free*/) != 0)
		perror("Unable to create thread name key: ");

	thread_init_done++;
}

void SleepCurrentThread(int ms)
{
	usleep(1000 * ms);
}


void SetCurrentThreadName(const TCHAR* szThreadName)
{
	pthread_setspecific(threadname_key, strdup(szThreadName));
	INFO_LOG(COMMON, "%s(%s)\n", __FUNCTION__, szThreadName);
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

} // namespace Common
