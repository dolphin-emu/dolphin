// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define GCC_VER(x,y,z)  ((x) * 10000 + (y) * 100 + (z))
#define GCC_VERSION GCC_VER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)

#ifndef __has_include
#define __has_include(s) 0
#endif

#if GCC_VERSION >= GCC_VER(4,4,0) && __GXX_EXPERIMENTAL_CXX0X__

// GCC 4.4 provides <condition_variable>
#include <condition_variable> // IWYU pragma: export

#elif __has_include(<condition_variable>) && !ANDROID

// clang and libc++ provide <condition_variable> on OSX. However, the version
// of libc++ bundled with OSX 10.7 and 10.8 is buggy: it uses _ as a variable.
//
// We work around this issue by undefining and redefining _.

#include <condition_variable> // IWYU pragma: export

#elif _MSC_VER >= 1700

// The standard implementation is included since VS2012
#include <condition_variable> // IWYU pragma: export

#else

// partial std::condition_variable implementation for win32/pthread

#include "Common/StdMutex.h"

#if (_MSC_VER >= 1600) || (GCC_VERSION >= GCC_VER(4,3,0) && __GXX_EXPERIMENTAL_CXX0X__)
#define USE_RVALUE_REFERENCES
#endif

#if defined(_WIN32) && _M_X86_64
#define USE_CONDITION_VARIABLES
#elif defined(_WIN32)
#define USE_EVENTS
#endif

namespace std
{

class condition_variable
{
#if defined(_WIN32) && defined(USE_CONDITION_VARIABLES)
	typedef CONDITION_VARIABLE native_type;
#elif defined(_WIN32)
	typedef HANDLE native_type;
#else
	typedef pthread_cond_t native_type;
#endif

public:

#ifdef USE_EVENTS
	typedef native_type native_handle_type;
#else
	typedef native_type* native_handle_type;
#endif

	condition_variable()
	{
#if defined(_WIN32) && defined(USE_CONDITION_VARIABLES)
		InitializeConditionVariable(&m_handle);
#elif defined(_WIN32)
		m_handle = CreateEvent(nullptr, false, false, nullptr);
#else
		pthread_cond_init(&m_handle, nullptr);
#endif
	}

	~condition_variable()
	{
#if defined(_WIN32) && !defined(USE_CONDITION_VARIABLES)
		CloseHandle(m_handle);
#elif !defined(_WIN32)
		pthread_cond_destroy(&m_handle);
#endif
	}

	condition_variable(const condition_variable&) /*= delete*/;
	condition_variable& operator=(const condition_variable&) /*= delete*/;

	void notify_one()
	{
#if defined(_WIN32) && defined(USE_CONDITION_VARIABLES)
		WakeConditionVariable(&m_handle);
#elif defined(_WIN32)
		SetEvent(m_handle);
#else
		pthread_cond_signal(&m_handle);
#endif
	}

	void notify_all()
	{
#if defined(_WIN32) && defined(USE_CONDITION_VARIABLES)
		WakeAllConditionVariable(&m_handle);
#elif defined(_WIN32)
		// TODO: broken
		SetEvent(m_handle);
#else
		pthread_cond_broadcast(&m_handle);
#endif
	}

	void wait(unique_lock<mutex>& lock)
	{
#ifdef _WIN32
	#ifdef USE_SRWLOCKS
		SleepConditionVariableSRW(&m_handle, lock.mutex()->native_handle(), INFINITE, 0);
	#elif defined(USE_CONDITION_VARIABLES)
		SleepConditionVariableCS(&m_handle, lock.mutex()->native_handle(), INFINITE);
	#else
		// TODO: broken, the unlock and wait need to be atomic
		lock.unlock();
		WaitForSingleObject(m_handle, INFINITE);
		lock.lock();
	#endif
#else
		pthread_cond_wait(&m_handle, lock.mutex()->native_handle());
#endif
	}

	template <class Predicate>
	void wait(unique_lock<mutex>& lock, Predicate pred)
	{
		while (!pred())
			wait(lock);
	}

	//template <class Clock, class Duration>
	//cv_status wait_until(unique_lock<mutex>& lock,
	//	const chrono::time_point<Clock, Duration>& abs_time);

	//template <class Clock, class Duration, class Predicate>
	//	bool wait_until(unique_lock<mutex>& lock,
	//	const chrono::time_point<Clock, Duration>& abs_time,
	//	Predicate pred);

	//template <class Rep, class Period>
	//cv_status wait_for(unique_lock<mutex>& lock,
	//	const chrono::duration<Rep, Period>& rel_time);

	//template <class Rep, class Period, class Predicate>
	//	bool wait_for(unique_lock<mutex>& lock,
	//	const chrono::duration<Rep, Period>& rel_time,
	//	Predicate pred);

	native_handle_type native_handle()
	{
#ifdef USE_EVENTS
		return m_handle;
#else
		return &m_handle;
#endif
	}

private:
	native_type m_handle;
};

}

#endif
