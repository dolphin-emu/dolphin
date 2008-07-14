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
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "Common.h"
#include "MemArena.h"


const char* ram_temp_file = "/tmp/gc_mem.tmp";

void MemArena::GrabLowMemSpace(size_t size)
{
#ifdef _WIN32
	hMemoryMapping = CreateFileMapping(NULL, NULL, PAGE_READWRITE, 0, (DWORD)(size), _T("All GC Memory"));
#else
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	fd = open(ram_temp_file, O_RDWR | O_CREAT, mode);
	ftruncate(fd, size);
	return;
#endif

}


void MemArena::ReleaseSpace()
{
#ifdef _WIN32
	CloseHandle(hMemoryMapping);
	hMemoryMapping = 0;
#else
	close(fd);
	unlink(ram_temp_file);
#endif
}


void* MemArena::CreateView(s64 offset, size_t size, bool ensure_low_mem)
{
#ifdef _WIN32
	return(MapViewOfFile(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size));

#else
	void* ptr = mmap(0, size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED | (ensure_low_mem ? MAP_32BIT : 0),
			fd, offset);

	if (!ptr)
	{
		PanicAlert("Failed to create view");
	}

	return(ptr);
#endif

}


void* MemArena::CreateViewAt(s64 offset, size_t size, void* base)
{
#ifdef _WIN32
	return(MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base));

#else
	return(mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, offset));
#endif

}


void MemArena::ReleaseView(void* view, size_t size)
{
#ifdef _WIN32
	UnmapViewOfFile(view);
#else
	munmap(view, size);
#endif
}


u64 MemArena::Find4GBBase()
{
#ifdef _M_X64
#ifdef _WIN32
	// The highest thing in any 1GB section of memory space is the locked cache. We only need to fit it.
	u8* base = (u8*)VirtualAlloc(0, 0xE1000000, MEM_RESERVE, PAGE_READWRITE);
	VirtualFree(base, 0, MEM_RELEASE);
	return((u64)base);

#else
	// Very precarious - mmap cannot return an error when trying to map already used pages.
	// This makes the Windows approach above unusable on Linux, so we will simply pray...
	return(0x2300000000ULL);
#endif

#else
	// Only grab a bit less than 1GB
	u8* base = (u8*)VirtualAlloc(0, 0x31000000, MEM_RESERVE, PAGE_READWRITE);
	VirtualFree(base, 0, MEM_RELEASE);
	return((u64)base);
#endif

}


