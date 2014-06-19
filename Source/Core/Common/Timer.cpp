// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <chrono>
#include <cinttypes>
#include <ctime>
#include <string>

#ifdef _WIN32
#include <mmsystem.h>
#include <windows.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

namespace Common
{

u32 Timer::GetTimeMs()
{
	return std::chrono::duration_cast<std::chrono::duration<u32, std::milli>>(
	       std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// --------------------------------------------
// Initiate, Start, Stop, and Update the time
// --------------------------------------------

// Set initial values for the class
Timer::Timer()
	: m_LastTime(0), m_StartTime(0), m_Running(false)
{
	Update();
}

// Write the starting time
void Timer::Start()
{
	m_StartTime = GetTimeMs();
	m_Running = true;
}

// Stop the timer
void Timer::Stop()
{
	// Write the final time
	m_LastTime = GetTimeMs();
	m_Running = false;
}

// Update the last time variable
void Timer::Update()
{
	m_LastTime = GetTimeMs();
	//TODO(ector) - QPF
}

// -------------------------------------
// Get time difference and elapsed time
// -------------------------------------

// Get the number of milliseconds since the last Update()
u64 Timer::GetTimeDifference()
{
	return GetTimeMs() - m_LastTime;
}

// Add the time difference since the last Update() to the starting time.
// This is used to compensate for a paused game.
void Timer::AddTimeDifference()
{
	m_StartTime += GetTimeDifference();
}

// Get the time elapsed since the Start()
u64 Timer::GetTimeElapsed()
{
	// If we have not started yet, return 1 (because then I don't
	// have to change the FPS calculation in CoreRerecording.cpp .
	if (m_StartTime == 0)
		return 1;

	// Return the final timer time if the timer is stopped
	if (!m_Running)
		return (m_LastTime - m_StartTime);

	return (GetTimeMs() - m_StartTime);
}

// Get the formatted time elapsed since the Start()
std::string Timer::GetTimeElapsedFormatted() const
{
	// If we have not started yet, return zero
	if (m_StartTime == 0)
		return "00:00:00:000";

	// The number of milliseconds since the start.
	// Use a different value if the timer is stopped.
	u64 Milliseconds;
	if (m_Running)
		Milliseconds = GetTimeMs() - m_StartTime;
	else
		Milliseconds = m_LastTime - m_StartTime;
	// Seconds
	u32 Seconds = (u32)(Milliseconds / 1000);
	// Minutes
	u32 Minutes = Seconds / 60;
	// Hours
	u32 Hours = Minutes / 60;

	std::string TmpStr = StringFromFormat("%02i:%02i:%02i:%03" PRIu64,
		Hours, Minutes % 60, Seconds % 60, Milliseconds % 1000);
	return TmpStr;
}

// Get current time
void Timer::IncreaseResolution()
{
#ifdef _WIN32
	timeBeginPeriod(1);
#endif
}

void Timer::RestoreResolution()
{
#ifdef _WIN32
	timeEndPeriod(1);
#endif
}

// Get the number of seconds since January 1 1970
u64 Timer::GetTimeSinceJan1970()
{
	return std::chrono::duration_cast<std::chrono::seconds>(
	       std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

u64 Timer::GetLocalTimeSinceJan1970()
{
	time_t tzDiff, tzDST;
	struct tm* gmTime;

	auto now = std::chrono::system_clock::now();
	time_t sysTime = std::chrono::system_clock::to_time_t(now);

	// Account for DST where needed
	gmTime = localtime(&sysTime);
	if (gmTime->tm_isdst == 1)
		tzDST = 3600;
	else
		tzDST = 0;

	// Lazy way to get local time in sec
	gmTime = gmtime(&sysTime);
	tzDiff = sysTime - mktime(gmTime);

	return (u64)(sysTime + tzDiff + tzDST);
}

// Return the current time formatted as Minutes:Seconds:Milliseconds
// in the form 00:00:000.
std::string Timer::GetTimeFormatted()
{
	auto nowOld = std::chrono::system_clock::now();
	time_t sysTime = std::chrono::system_clock::to_time_t(nowOld);
	struct tm* gmTime = localtime(&sysTime);

	std::string tmp = StringFromFormat("%02d:%02d", gmTime->tm_min, gmTime->tm_sec);

	// Now tack on the milliseconds
	int millis = std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	return StringFromFormat("%s:%03d", tmp.c_str(), millis);
}

// Returns a timestamp with decimals for precise time comparisons
// ----------------
double Timer::GetDoubleTime()
{
	auto time = std::chrono::high_resolution_clock::now().time_since_epoch();

	// Get continuous timestamp
	u64 TmpSeconds = Common::Timer::GetTimeSinceJan1970();

	// Remove a few years. We only really want enough seconds to make
	// sure that we are detecting actual actions, perhaps 60 seconds is
	// enough really, but I leave a year of seconds anyway, in case the
	// user's clock is incorrect or something like that.
	TmpSeconds -= (38 * 365 * 24 * 60 * 60);

	// Make a smaller integer that fits in the double
	u32 Seconds = (u32)TmpSeconds;

	double ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(time).count();
	return Seconds + ms;
}

} // Namespace Common
