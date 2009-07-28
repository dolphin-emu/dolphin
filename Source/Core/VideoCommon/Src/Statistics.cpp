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

#include <string.h>

#include "Statistics.h"

Statistics stats;

template <class T>
void Xchg(T& a, T&b)
{
	T c = a;
	a = b;
	b = c;
}

void Statistics::ResetFrame()
{
	memset(&thisFrame, 0, sizeof(ThisFrame));
}

void Statistics::SwapDL()
{
	Xchg(stats.thisFrame.numDLPrims, stats.thisFrame.numPrims);
	Xchg(stats.thisFrame.numXFLoadsInDL, stats.thisFrame.numXFLoads);
	Xchg(stats.thisFrame.numCPLoadsInDL, stats.thisFrame.numCPLoads);
	Xchg(stats.thisFrame.numBPLoadsInDL, stats.thisFrame.numBPLoads);
}
