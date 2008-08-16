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

#ifndef __AOSOUNDSTREAM_H__
#define __AOSOUNDSTREAM_H__

namespace AOSound
{
typedef void (*StreamCallback)(short* buffer, int numSamples, int bits, int rate, int channels);

bool AOSound_StartSound(int sampleRate, StreamCallback _callback);
void AOSound_UpdateSound();
void AOSound_StopSound();

float AOSound_GetTimer();
int AOSound_GetCurSample();
int AOSound_GetSampleRate();
}


#endif //__AOSOUNDSTREAM_H__
