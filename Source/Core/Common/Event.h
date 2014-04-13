// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Multithreaded event class. This allows waiting in a thread for an event to
// be triggered in another thread. While waiting, the CPU will be available for
// other tasks.
// * Set(): triggers the event and wakes up the waiting thread.
// * Wait(): waits for the event to be triggered.
// * Reset(): tries to reset the event before the waiting thread sees it was
//            triggered. Usually a bad idea.

#pragma once

#include "Common/StdConditionVariable.h"
#include "Common/StdMutex.h"

namespace Common {

class Event
{
public:
	Event()
		: is_set(false)
	{}

	void Set()
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		if (!is_set)
		{
			is_set = true;
			m_condvar.notify_one();
		}
	}

	void Wait()
	{
		std::unique_lock<std::mutex> lk(m_mutex);
		m_condvar.wait(lk, [&]{ return is_set; });
		is_set = false;
	}

	void Reset()
	{
		std::unique_lock<std::mutex> lk(m_mutex);
		// no other action required, since wait loops on
		// the predicate and any lingering signal will get
		// cleared on the first iteration
		is_set = false;
	}

private:
	volatile bool is_set;
	std::condition_variable m_condvar;
	std::mutex m_mutex;
};

}  // namespace Common
