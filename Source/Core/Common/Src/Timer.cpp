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

#ifdef _WIN32
#include <mmsystem.h>
#endif

#include <time.h>

#include "Common.h"
#include "Timer.h"

#ifdef __GNUC__
#include <sys/timeb.h>

u32 timeGetTime()
{
	struct timeb t;
	ftime(&t);
	return((u32)(t.time * 1000 + t.millitm));
}


#endif


namespace Common
{
Timer::Timer(void)
	: m_LastTime(0)
{
	Update();

#ifdef _WIN32
	QueryPerformanceFrequency((LARGE_INTEGER*)&m_frequency);
#endif
}


void Timer::Update(void)
{
	m_LastTime = timeGetTime();
	//TODO(ector) - QPF
}


s64 Timer::GetTimeDifference(void)
{
	return(timeGetTime() - m_LastTime);
}


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


#ifdef __GNUC__
void _time64(u64* t)
{
	*t = 0; //TODO
}
#endif



u64 Timer::GetTimeSinceJan1970(void)
{
	time_t ltime;
	time(&ltime);
	return((u64)ltime);
}

u64 Timer::GetLocalTimeSinceJan1970(void)
{
	time_t sysTime, tzDiff;
	struct tm * gmTime;

	time(&sysTime);
	// Lazy way to get local time in sec
	gmTime	= gmtime(&sysTime);
	tzDiff = sysTime - mktime(gmTime);

	return (u64)(sysTime + tzDiff);
}
} // end of namespace Common
