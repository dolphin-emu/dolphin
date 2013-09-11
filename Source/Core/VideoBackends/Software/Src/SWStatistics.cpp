// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "SWStatistics.h"

SWStatistics swstats;

template <class T>
void Xchg(T& a, T&b)
{
	T c = a;
	a = b;
	b = c;
}

SWStatistics::SWStatistics()
{
	frameCount = 0;
}

void SWStatistics::ResetFrame()
{
	memset(&thisFrame, 0, sizeof(ThisFrame));
}
