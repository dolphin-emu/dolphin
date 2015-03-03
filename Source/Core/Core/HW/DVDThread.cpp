// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <mutex>
#include <thread>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Common/Logging/Log.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/DVDThread.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "DiscIO/Volume.h"

namespace DVDThread
{

static void DVDThread();

static void FinishRead(u64 userdata, int cyclesLate);
static int s_finish_read;

static std::thread s_dvd_thread;
static Common::Event s_dvd_thread_start_working;
static Common::Event s_dvd_thread_done_working;
static Common::Flag s_dvd_thread_exiting(false);

static std::vector<u8> s_dvd_buffer;
static u64 s_time_read_started;
static bool s_dvd_success;

static u64 s_dvd_offset;
static u32 s_output_address;
static u32 s_length;
static bool s_decrypt;

// Used to notify emulated software after executing command.
// Pointers don't work with savestates, so CoreTiming events are used instead
static int s_callback_event_type;

// The following time variables are only used for logging
static u64 s_realtime_started_us;
static u64 s_realtime_done_us;

void Start()
{
	s_finish_read = CoreTiming::RegisterEvent("FinishReadDVDThread", FinishRead);
	_assert_(!s_dvd_thread.joinable());
	s_dvd_thread = std::thread(DVDThread);
}

void Stop()
{
	_assert_(s_dvd_thread.joinable());

	// The DVD thread will return if s_DVD_thread_exiting
	// is set when it starts working
	s_dvd_thread_exiting.Set();
	s_dvd_thread_start_working.Set();

	s_dvd_thread.join();

	s_dvd_thread_exiting.Clear();
}

void DoState(PointerWrap &p)
{
	WaitUntilIdle();

	// TODO: Savestates can be smaller if s_DVD_buffer is not saved
	p.Do(s_dvd_buffer);
	p.Do(s_time_read_started);
	p.Do(s_dvd_success);

	p.Do(s_dvd_offset);
	p.Do(s_output_address);
	p.Do(s_length);
	p.Do(s_decrypt);
	p.Do(s_callback_event_type);

	// s_realtime_started_us and s_realtime_done_us aren't savestated
	// because they rely on the current system's time.
	// This means that loading a savestate might cause
	// incorrect times to be logged once.
}

void WaitUntilIdle()
{
	_assert_(Core::IsCPUThread());

	// Wait until DVD thread isn't working
	s_dvd_thread_done_working.Wait();

	// Set the event again so that we still know that the DVD thread isn't working
	s_dvd_thread_done_working.Set();
}

void StartRead(u64 dvd_offset, u32 output_address, u32 length, bool decrypt,
               int callback_event_type, int ticks_until_completion)
{
	_assert_(Core::IsCPUThread());

	s_dvd_thread_done_working.Wait();

	s_dvd_offset = dvd_offset;
	s_output_address = output_address;
	s_length = length;
	s_decrypt = decrypt;
	s_callback_event_type = callback_event_type;

	s_time_read_started = CoreTiming::GetTicks();
	s_realtime_started_us = Common::Timer::GetTimeUs();

	s_dvd_thread_start_working.Set();

	CoreTiming::ScheduleEvent(ticks_until_completion, s_finish_read);
}

static void FinishRead(u64 userdata, int cyclesLate)
{
	WaitUntilIdle();

	DEBUG_LOG(DVDINTERFACE, "Disc has been read. Real time: %" PRIu64 " us. "
	          "Real time including delay: %" PRIu64 " us. Emulated time including delay: %" PRIu64 " us.",
	          s_realtime_done_us - s_realtime_started_us,
	          Common::Timer::GetTimeUs() - s_realtime_started_us,
	          (CoreTiming::GetTicks() - s_time_read_started) / (SystemTimers::GetTicksPerSecond() / 1000 / 1000));

	if (s_dvd_success)
		Memory::CopyToEmu(s_output_address, s_dvd_buffer.data(), s_length);
	else
		PanicAlertT("The disc could not be read (at 0x%" PRIx64 " - 0x%" PRIx64 ").",
		            s_dvd_offset, s_dvd_offset + s_length);

	// This will make the buffer take less space in savestates.
	// Reducing the size doesn't change the amount of reserved memory,
	// so this doesn't lead to extra memory allocations.
	s_dvd_buffer.resize(0);

	// Notify the emulated software that the command has been executed
	CoreTiming::ScheduleEvent_Immediate(s_callback_event_type, DVDInterface::INT_TCINT);
}

static void DVDThread()
{
	Common::SetCurrentThreadName("DVD thread");

	while (true)
	{
		s_dvd_thread_done_working.Set();

		s_dvd_thread_start_working.Wait();

		if (s_dvd_thread_exiting.IsSet())
			return;

		s_dvd_buffer.resize(s_length);

		s_dvd_success = DVDInterface::GetVolume().Read(s_dvd_offset, s_length, s_dvd_buffer.data(), s_decrypt);

		s_realtime_done_us = Common::Timer::GetTimeUs();
	}
}

}
