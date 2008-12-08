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

#include "Common.h"
#include "WaveFile.h"

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

bool WaveFileWriter::Start(const char *filename)
{
	if (!conv_buffer)
		conv_buffer = new short[BUF_SIZE];

	if (file)
		return false;
	file = fopen(filename, "wb");
	if (!file)
		return false;

	Write4("RIFF");
	Write(100 * 1000 * 1000);  // write big value in case the file gets truncated
	Write4("WAVE");
	Write4("fmt ");
	Write(16);  // size of fmt block
	Write(0x00020001); //two channels, uncompressed
	const u32 sample_rate = 32000;
	Write(sample_rate);
	Write(sample_rate * 2 * 2); //two channels, 16bit
	Write(0x00100004);
	Write4("data");
	Write(100 * 1000 * 1000 - 32);
	// We are now at offset 44
	if (ftell(file) != 44)
		PanicAlert("wrong offset: %i", ftell(file));

        return true;
}

void WaveFileWriter::Stop()
{
	if (!file)
		return;
        //	u32 file_size = (u32)ftell(file);
	fseek(file, 4, SEEK_SET);
	Write(audio_size + 36);
	fseek(file, 40, SEEK_SET);
	Write(audio_size);
	fclose(file);
	file = 0;
}

void WaveFileWriter::Write(u32 value)
{
	fwrite(&value, 4, 1, file);
}

void WaveFileWriter::Write4(const char *ptr)
{
	fwrite(ptr, 4, 1, file);
}

void WaveFileWriter::AddStereoSamples(const short *sample_data, int count)
{
	if (!file)
		PanicAlert("WaveFileWriter - file not open.");
	if (skip_silence) {
		bool all_zero = true;
		for (int i = 0; i < count * 2; i++)
			if (sample_data[i])
				all_zero = false;
		if (all_zero)
			return;
	}
	fwrite(sample_data, count * 4, 1, file);
	audio_size += count * 4;
}

void WaveFileWriter::AddStereoSamplesBE(const short *sample_data, int count)
{
	if (!file)
		PanicAlert("WaveFileWriter - file not open.");
	if (count > BUF_SIZE * 2)
		PanicAlert("WaveFileWriter - buffer too small (count = %i).", count);
	if (skip_silence) {
		bool all_zero = true;
		for (int i = 0; i < count * 2; i++)
			if (sample_data[i])
				all_zero = false;
		if (all_zero)
			return;
	}
	for (int i = 0; i < count * 2; i++) {
		conv_buffer[i] = Common::swap16((u16)sample_data[i]);
	}
	fwrite(conv_buffer, count * 4, 1, file);
	audio_size += count * 4;
}
