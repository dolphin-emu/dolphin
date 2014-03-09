// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define GCC_VER(x,y,z) ((x) * 10000 + (y) * 100 + (z))
#define GCC_VERSION GCC_VER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)

#ifndef __has_include
#define __has_include(s) 0
#endif

#if GCC_VERSION >= GCC_VER(4,4,0) && __GXX_EXPERIMENTAL_CXX0X__
// GCC 4.4 provides <mutex>
#include <mutex> // IWYU pragma: export
#elif __has_include(<mutex>) && !ANDROID
// Clang + libc++
#include <mutex> // IWYU pragma: export

#elif _MSC_VER >= 1700

// The standard implementation is included since VS2012
#include <mutex> // IWYU pragma: export

#else

// partial <mutex> implementation for win32/pthread

#include <algorithm>

#if defined(_WIN32)
// WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#else
// POSIX
#include <pthread.h>

#endif

#if (_MSC_VER >= 1600) || (GCC_VERSION >= GCC_VER(4,3,0) && __GXX_EXPERIMENTAL_CXX0X__)
#define USE_RVALUE_REFERENCES
#endif

#if defined(_WIN32) && _M_X86_64
#define USE_SRWLOCKS
#endif

namespace std
{

class recursive_mutex
{
#ifdef _WIN32
	typedef CRITICAL_SECTION native_type;
#else
	typedef pthread_mutex_t native_type;
#endif

public:
	typedef native_type* native_handle_type;

	recursive_mutex(const recursive_mutex&) /*= delete*/;
	recursive_mutex& operator=(const recursive_mutex&) /*= delete*/;

	recursive_mutex()
	{
#ifdef _WIN32
		InitializeCriticalSection(&m_handle);
#else
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_handle, &attr);
#endif
	}

	~recursive_mutex()
	{
#ifdef _WIN32
		DeleteCriticalSection(&m_handle);
#else
		pthread_mutex_destroy(&m_handle);
#endif
	}

	void lock()
	{
#ifdef _WIN32
		EnterCriticalSection(&m_handle);
#else
		pthread_mutex_lock(&m_handle);
#endif
	}

	void unlock()
	{
#ifdef _WIN32
		LeaveCriticalSection(&m_handle);
#else
		pthread_mutex_unlock(&m_handle);
#endif
	}

	bool try_lock()
	{
#ifdef _WIN32
		return (0 != TryEnterCriticalSection(&m_handle));
#else
		return !pthread_mutex_trylock(&m_handle);
#endif
	}

	native_handle_type native_handle()
	{
		return &m_handle;
	}

private:
	native_type m_handle;
};

#if !defined(_WIN32) || defined(USE_SRWLOCKS)

class mutex
{
#ifdef _WIN32
	typedef SRWLOCK native_type;
#else
	typedef pthread_mutex_t native_type;
#endif

public:
	typedef native_type* native_handle_type;

	mutex(const mutex&) /*= delete*/;
	mutex& operator=(const mutex&) /*= delete*/;

	mutex()
	{
#ifdef _WIN32
		InitializeSRWLock(&m_handle);
#else
		pthread_mutex_init(&m_handle, nullptr);
#endif
	}

	~mutex()
	{
#ifdef _WIN32
#else
		pthread_mutex_destroy(&m_handle);
#endif
	}

	void lock()
	{
#ifdef _WIN32
		AcquireSRWLockExclusive(&m_handle);
#else
		pthread_mutex_lock(&m_handle);
#endif
	}

	void unlock()
	{
#ifdef _WIN32
		ReleaseSRWLockExclusive(&m_handle);
#else
		pthread_mutex_unlock(&m_handle);
#endif
	}

	bool try_lock()
	{
#ifdef _WIN32
		// XXX TryAcquireSRWLockExclusive requires Windows 7!
		// return (0 != TryAcquireSRWLockExclusive(&m_handle));
		return false;
#else
		return !pthread_mutex_trylock(&m_handle);
#endif
	}

	native_handle_type native_handle()
	{
		return &m_handle;
	}

private:
	native_type m_handle;
};

#else
typedef recursive_mutex mutex; // just use CriticalSections

#endif

enum defer_lock_t { defer_lock };
enum try_to_lock_t { try_to_lock };
enum adopt_lock_t { adopt_lock };

template <class Mutex>
class lock_guard
{
public:
	typedef Mutex mutex_type;

	explicit lock_guard(mutex_type& m)
		: pm(m)
	{
		m.lock();
	}

	lock_guard(mutex_type& m, adopt_lock_t)
		: pm(m)
	{
	}

	~lock_guard()
	{
		pm.unlock();
	}

	lock_guard(lock_guard const&) /*= delete*/;
	lock_guard& operator=(lock_guard const&) /*= delete*/;

private:
	mutex_type& pm;
};

template <class Mutex>
class unique_lock
{
public:
	typedef Mutex mutex_type;

	unique_lock()
		: pm(nullptr), owns(false)
	{}

	/*explicit*/ unique_lock(mutex_type& m)
		: pm(&m), owns(true)
	{
		m.lock();
	}

	unique_lock(mutex_type& m, defer_lock_t)
		: pm(&m), owns(false)
	{}

	unique_lock(mutex_type& m, try_to_lock_t)
		: pm(&m), owns(m.try_lock())
	{}

	unique_lock(mutex_type& m, adopt_lock_t)
		: pm(&m), owns(true)
	{}

	//template <class Clock, class Duration>
	//unique_lock(mutex_type& m, const chrono::time_point<Clock, Duration>& abs_time);

	//template <class Rep, class Period>
	//unique_lock(mutex_type& m, const chrono::duration<Rep, Period>& rel_time);

	~unique_lock()
	{
		if (owns_lock())
			mutex()->unlock();
	}

#ifdef USE_RVALUE_REFERENCES
	unique_lock& operator=(const unique_lock&) /*= delete*/;

	unique_lock& operator=(unique_lock&& other)
	{
#else
	unique_lock& operator=(const unique_lock& u)
	{
		// ugly const_cast to get around lack of rvalue references
		unique_lock& other = const_cast<unique_lock&>(u);
#endif
		swap(other);
		return *this;
	}

#ifdef USE_RVALUE_REFERENCES
	unique_lock(const unique_lock&) /*= delete*/;

	unique_lock(unique_lock&& other)
		: pm(nullptr), owns(false)
	{
#else
	unique_lock(const unique_lock& u)
		: pm(nullptr), owns(false)
	{
		// ugly const_cast to get around lack of rvalue references
		unique_lock& other = const_cast<unique_lock&>(u);
#endif
		swap(other);
	}

	void lock()
	{
		mutex()->lock();
		owns = true;
	}

	bool try_lock()
	{
		owns = mutex()->try_lock();
		return owns;
	}

	//template <class Rep, class Period>
	//bool try_lock_for(const chrono::duration<Rep, Period>& rel_time);
	//template <class Clock, class Duration>
	//bool try_lock_until(const chrono::time_point<Clock, Duration>& abs_time);

	void unlock()
	{
		mutex()->unlock();
		owns = false;
	}

	void swap(unique_lock& u)
	{
		std::swap(pm, u.pm);
		std::swap(owns, u.owns);
	}

	mutex_type* release()
	{
		auto const ret = mutex();

		pm = nullptr;
		owns = false;

		return ret;
	}

	bool owns_lock() const
	{
		return owns;
	}

	mutex_type* mutex() const
	{
		return pm;
	}

private:
	mutex_type* pm;
	bool owns;
};

template <class Mutex>
void swap(unique_lock<Mutex>& x, unique_lock<Mutex>& y)
{
	x.swap(y);
}

}

#endif
