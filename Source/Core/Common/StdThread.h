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
// GCC 4.4 provides <thread>
#ifndef _GLIBCXX_USE_SCHED_YIELD
#define _GLIBCXX_USE_SCHED_YIELD
#endif
#include <thread> // IWYU pragma: export
#elif __has_include(<thread>) && !ANDROID
// Clang + libc++
#include <thread> // IWYU pragma: export

#elif _MSC_VER >= 1700

// The standard implementation is included since VS2012
#include <thread> // IWYU pragma: export

#else

// partial std::thread implementation for win32/pthread

#include <algorithm>

#if (_MSC_VER >= 1600) || (GCC_VERSION >= GCC_VER(4,3,0) && __GXX_EXPERIMENTAL_CXX0X__)
#define USE_RVALUE_REFERENCES
#endif

#ifdef __APPLE__
#import <Foundation/NSAutoreleasePool.h>
#endif

#if defined(_WIN32)
// WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if defined(_MSC_VER) && defined(_MT)
// When linking with LIBCMT (the multithreaded C library), Microsoft recommends
// using _beginthreadex instead of CreateThread.
#define USE_BEGINTHREADEX
#include <process.h>
#endif

#ifdef USE_BEGINTHREADEX
#define THREAD_ID unsigned
#define THREAD_RETURN unsigned __stdcall
#else
#define THREAD_ID DWORD
#define THREAD_RETURN DWORD WINAPI
#endif
#define THREAD_HANDLE HANDLE

#else
// PTHREAD

#include <unistd.h>

#ifndef _POSIX_THREADS
#error unsupported platform (no pthreads?)
#endif

#include <pthread.h>

#define THREAD_ID pthread_t
#define THREAD_HANDLE pthread_t
#define THREAD_RETURN void*

#endif

namespace std
{

class thread
{
public:
	typedef THREAD_HANDLE native_handle_type;

	class id
	{
		friend class thread;
	public:
		id() : m_thread(0) {}
		id(THREAD_ID _id) : m_thread(_id) {}

		bool operator==(const id& rhs) const
		{
			return m_thread == rhs.m_thread;
		}

		bool operator!=(const id& rhs) const
		{
			return !(*this == rhs);
		}

		bool operator<(const id& rhs) const
		{
			return m_thread < rhs.m_thread;
		}

	private:
		THREAD_ID m_thread;
	};

	// no variadic template support in msvc
	//template <typename C, typename... A>
	//thread(C&& func, A&&... args);

	template <typename C>
	thread(C func)
	{
		StartThread(new Func<C>(func));
	}

	template <typename C, typename A>
	thread(C func, A arg)
	{
		StartThread(new FuncArg<C, A>(func, arg));
	}

	thread() /*= default;*/ {}

#ifdef USE_RVALUE_REFERENCES
	thread(const thread&) /*= delete*/;

	thread(thread&& other)
	{
#else
	thread(const thread& t)
	{
		// ugly const_cast to get around lack of rvalue references
		thread& other = const_cast<thread&>(t);
#endif
		swap(other);
	}

#ifdef USE_RVALUE_REFERENCES
	thread& operator=(const thread&) /*= delete*/;

	thread& operator=(thread&& other)
	{
#else
	thread& operator=(const thread& t)
	{
		// ugly const_cast to get around lack of rvalue references
		thread& other = const_cast<thread&>(t);
#endif
		if (joinable())
			detach();
		swap(other);
		return *this;
	}

	~thread()
	{
		if (joinable())
			detach();
	}

	bool joinable() const
	{
		return m_id != id();
	}

	id get_id() const
	{
		return m_id;
	}

	native_handle_type native_handle()
	{
#ifdef _WIN32
		return m_handle;
#else
		return m_id.m_thread;
#endif
	}

	void join()
	{
#ifdef _WIN32
		WaitForSingleObject(m_handle, INFINITE);
		detach();
#else
		pthread_join(m_id.m_thread, nullptr);
		m_id = id();
#endif
	}

	void detach()
	{
#ifdef _WIN32
		CloseHandle(m_handle);
#else
		pthread_detach(m_id.m_thread);
#endif
		m_id = id();
	}

	void swap(thread& other)
	{
		std::swap(m_id, other.m_id);
#ifdef _WIN32
		std::swap(m_handle, other.m_handle);
#endif
	}

	static unsigned hardware_concurrency()
	{
#ifdef _WIN32
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return static_cast<unsigned>(sysinfo.dwNumberOfProcessors);
#else
		return 0;
#endif
	}

private:
	id m_id;

#ifdef _WIN32
	native_handle_type m_handle;
#endif

	template <typename F>
	void StartThread(F* param)
	{
#ifdef USE_BEGINTHREADEX
		m_handle = (HANDLE)_beginthreadex(nullptr, 0, &RunAndDelete<F>, param, 0, &m_id.m_thread);
#elif defined(_WIN32)
		m_handle = CreateThread(nullptr, 0, &RunAndDelete<F>, param, 0, &m_id.m_thread);
#else
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 1024 * 1024);
		if (pthread_create(&m_id.m_thread, &attr, &RunAndDelete<F>, param))
			m_id = id();
#endif
	}

	template <typename C>
	class Func
	{
	public:
		Func(C _func) : func(_func) {}

		void Run() { func(); }

	private:
		C const func;
	};

	template <typename C, typename A>
	class FuncArg
	{
	public:
		FuncArg(C _func, A _arg) : func(_func), arg(_arg) {}

		void Run() { func(arg); }

	private:
		C const func;
		A arg;
	};

	template <typename F>
	static THREAD_RETURN RunAndDelete(void* param)
	{
#ifdef __APPLE__
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#endif
		static_cast<F*>(param)->Run();
		delete static_cast<F*>(param);
#ifdef __APPLE__
		[pool release];
#endif
		return 0;
	}
};

namespace this_thread
{

inline void yield()
{
#ifdef _WIN32
	SwitchToThread();
#else
	sleep(0);
#endif
}

inline thread::id get_id()
{
#ifdef _WIN32
	return GetCurrentThreadId();
#else
	return pthread_self();
#endif
}

} // namespace this_thread

} // namespace std

#undef USE_RVALUE_REFERENCES
#undef USE_BEGINTHREADEX
#undef THREAD_ID
#undef THREAD_RETURN
#undef THREAD_HANDLE

#endif
