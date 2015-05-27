// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "Common/Event.h"
#include "Common/Flag.h"

namespace Common
{

// This class provides a synchronized loop.
// It's a thread-safe way to trigger a new iteration without busy loops.
// It's optimized for high-usage iterations which usually are already running while it's triggered often.
class BlockingLoop
{
public:
	BlockingLoop()
	{
		m_stopped.Set();
	}

	~BlockingLoop()
	{
		Stop();
	}

	// Triggers to rerun the payload of the Run() function at least once again.
	// This function will never block and is designed to finish as fast as possible.
	void Wakeup()
	{
		// already running, so no need for a wakeup
		if (m_is_running.IsSet())
			return;

		m_is_running.Set();
		m_is_pending.Set();
		m_new_work_event.Set();
	}

	// Wait for a complete payload run after the last Wakeup() call.
	// If stopped, this returns immediately.
	void Wait()
	{
		// We have to give the loop a chance to exit.
		m_may_sleep.Set();

		if (m_stopped.IsSet() || (!m_is_running.IsSet() && !m_is_pending.IsSet()))
			return;

		// notifying this event will only wake up one thread, so use a mutex here to
		// allow only one waiting thread. And in this way, we get an event free wakeup
		// but for the first thread for free
		std::lock_guard<std::mutex> lk(m_wait_lock);

		while (!m_stopped.IsSet() && (m_is_running.IsSet() || m_is_pending.IsSet()))
		{
			m_may_sleep.Set();
			m_done_event.Wait();
		}
	}

	// Half start the worker.
	// So this object is in running state and Wait() will block until the worker calls Run().
	// This may be called from any thread and is supposed to call at least once before Wait() is used.
	void Prepare()
	{
		// There is a race condition if the other threads call this function while
		// the loop thread is initializing. Using this lock will ensure a valid state.
		std::lock_guard<std::mutex> lk(m_prepare_lock);

		if (!m_stopped.TestAndClear())
			return;
		m_is_pending.Set();
		m_shutdown.Clear();
		m_may_sleep.Set();
	}

	// Mainloop of this object.
	// The payload callback is called at least as often as it's needed to match the Wakeup() requirements.
	template<class F> void Run(F payload)
	{
		Prepare();

		while (!m_shutdown.IsSet())
		{
			payload();

			m_is_pending.Clear();
			m_done_event.Set();

			if (m_is_running.IsSet())
			{
				if (m_may_sleep.IsSet())
				{
					m_is_pending.Set();
					m_is_running.Clear();

					// We'll sleep after the next iteration now,
					// so clear this flag now and we won't sleep another times.
					m_may_sleep.Clear();
				}
			}
			else
			{
				m_new_work_event.WaitFor(std::chrono::milliseconds(100));
			}

		}

		m_is_running.Clear();
		m_is_pending.Clear();
		m_stopped.Set();

		m_done_event.Set();
	}

	// Quits the mainloop.
	// By default, it will wait until the Mainloop quits.
	// Be careful to not use the blocking way within the payload of the Run() method.
	void Stop(bool block = true)
	{
		if (m_stopped.IsSet())
			return;

		m_shutdown.Set();
		Wakeup();

		if (block)
			Wait();
	}

	bool IsRunning() const
	{
		return !m_stopped.IsSet() && !m_shutdown.IsSet();
	}

	void AllowSleep()
	{
		m_may_sleep.Set();
	}

private:
	std::mutex m_wait_lock;
	std::mutex m_prepare_lock;

	Flag m_stopped; // This one is set, Wait() shall not block.
	Flag m_shutdown; // If this one is set, the loop shall be quit.

	Event m_new_work_event;
	Flag m_is_running; // If this one is set, the loop will be called at least once again.

	Event m_done_event;
	Flag m_is_pending; // If this one is set, there might still be work to do.

	Flag m_may_sleep; // If this one is set, we fall back from the busy loop to an event based synchronization.
};

}
