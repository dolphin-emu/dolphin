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

#include "Common.h"
#include "WaveFile.h"
#include "../../Core/Src/ConfigManager.h"

enum {BUF_SIZE = 32*1024};

WaveFileWriter::WaveFileWriter()
{
	conv_buffer = 0;
	skip_silence = false;
}

WaveFileWriter::~WaveFileWriter()
{
	delete [] conv_buffer;
	Stop();
}

bool WaveFileWriter::Start(const char *filename, unsigned int HLESampleRate)
{
	if (!conv_buffer)
		conv_buffer = new short[BUF_SIZE];

	// Check if the file is already open
	if (file)
	{
		PanicAlertT("The file %s was already open, the file header will not be written.", filename);
		return false;
	}

	file.Open(filename, "wb");
	if (!file)
	{
		PanicAlertT("The file %s could not be opened for writing. Please check if it's already opened by another program.", filename);
		return false;
	}

	// -----------------
	// Write file header
	// -----------------
	Write4("RIFF");
	Write(100 * 1000 * 1000);  // write big value in case the file gets truncated
	Write4("WAVE");
	Write4("fmt ");

	Write(16);  // size of fmt block
	Write(0x00020001); //two channels, uncompressed

	const u32 sample_rate = HLESampleRate;
	Write(sample_rate);
	Write(sample_rate * 2 * 2); //two channels, 16bit

	Write(0x00100004);
	Write4("data");
	Write(100 * 1000 * 1000 - 32);

	// We are now at offset 44
	if (file.Tell() != 44)
		PanicAlert("wrong offset: %lld", (long long)file.Tell());

	return true;
}

void WaveFileWriter::Stop()
{
	// u32 file_size = (u32)ftello(file);
	file.Seek(4, SEEK_SET);
	Write(audio_size + 36);

	file.Seek(40, SEEK_SET);
	Write(audio_size);

	file.Close();
}

void WaveFileWriter::Write(u32 value)
{
	file.WriteArray(&value, 1);
}

void WaveFileWriter::Write4(const char *ptr)
{
	file.WriteBytes(ptr, 4);
}

void WaveFileWriter::AddStereoSamples(const short *sample_data, int count)
{
	if (!file)
		PanicAlertT("WaveFileWriter - file not open.");
	if (skip_silence) {
		bool all_zero = true;
		for (int i = 0; i < count * 2; i++)
			if (sample_data[i]) all_zero = false;
		if (all_zero) return;
	}
	file.WriteBytes(sample_data, count * 4);
	audio_size += count * 4;
}

void WaveFileWriter::AddStereoSamplesBE(const short *sample_data, int count)
{
	if (!file)
		PanicAlertT("WaveFileWriter - file not open.");

	if (count > BUF_SIZE * 2)
		PanicAlert("WaveFileWriter - buffer too small (count = %i).", count);

	if (skip_silence) 
	{
		bool all_zero = true;
		for (int i = 0; i < count * 2; i++)
			if (sample_data[i])
				all_zero = false;
		if (all_zero)
			return;
	}

	for (int i = 0; i < count * 2; i++)
		conv_buffer[i] = Common::swap16((u16)sample_data[i]);

	file.WriteBytes(conv_buffer, count * 4);
	audio_size += count * 4;
}
