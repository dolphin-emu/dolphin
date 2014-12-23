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

// Official Git repository and contact information can be found at
// https://github.com/dolphin-emu/dolphin

// Stubs to make DSPCore compile as part of DSPSpy.

/*
#include <stdlib.h>
#include <stdio.h>

#include <string>

#include "Thread.h"

void *AllocateMemoryPages(size_t size)
{
	return malloc(size);
}

void FreeMemoryPages(void *pages, size_t size)
{
	free(pages);
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
}

bool DSPHost_OnThread()
{
	return false;
}

// Well, it's just RAM right? :)
u8 DSPHost_ReadHostMemory(u32 address)
{
	u8 *ptr = (u8*)address;
	return *ptr;
}

void DSPHost_WriteHostMemory(u8 value, u32 addr) {}


void DSPHost_CodeLoaded(const u8 *code, int size)
{
}


namespace Common
{

CriticalSection::CriticalSection(int)
{
}
CriticalSection::~CriticalSection()
{
}

void CriticalSection::Enter()
{
}

void CriticalSection::Leave()
{
}

}  // namespace

namespace File
{

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename)
{
	FILE *f = fopen(filename, text_file ? "w" : "wb");
	if (!f)
		return false;
	size_t len = str.size();
	if (len != fwrite(str.data(), 1, str.size(), f))
	{
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

bool ReadFileToString(bool text_file, const char *filename, std::string &str)
{
	FILE *f = fopen(filename, text_file ? "r" : "rb");
	if (!f)
		return false;
	fseeko(f, 0, SEEK_END);
	size_t len = ftello(f);
	fseeko(f, 0, SEEK_SET);
	char *buf = new char[len + 1];
	buf[fread(buf, 1, len, f)] = 0;
	str = std::string(buf, len);
	fclose(f);
	delete [] buf;
	return true;
}

}

*/
