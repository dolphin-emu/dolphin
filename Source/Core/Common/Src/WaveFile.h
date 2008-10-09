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

// WaveFileWriter

// Simple utility class to make it easy to write long 16-bit stereo
// audio streams to disk.
// Use Start() to start recording to a file, and AddStereoSamples to add wave data.
// The float variant will convert from -1.0-1.0 range and clamp.
// Alternatively, AddSamplesBE for big endian wave data.
// If Stop is not called when it destructs, the destructor will call Stop().

#ifndef _WAVEFILE_H
#define _WAVEFILE_H

#include <stdio.h>

class WaveFileWriter
{
	FILE *file;
	bool skip_silence;
	u32 audio_size;
	short *conv_buffer;
	void Write(u32 value);
	void Write4(const char *ptr);

public:
	WaveFileWriter();
	~WaveFileWriter();

	bool Start(const char *filename);
	void Stop();

	void SetSkipSilence(bool skip) { skip_silence = skip; }

	void AddStereoSamples(const short *sample_data, int count);
	void AddStereoSamplesBE(const short *sample_data, int count);  // big endian
	u32 GetAudioSize() { return audio_size; }
};

#endif  // _WAVEFILE_H
