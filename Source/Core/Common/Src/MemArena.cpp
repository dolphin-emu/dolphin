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
#include "MemArena.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif 

#ifndef _WIN32
static const char* ram_temp_file = "/tmp/gc_mem.tmp";
#endif

void MemArena::GrabLowMemSpace(size_t size)
{
#ifdef _WIN32
	hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)(size), _T("All GC Memory"));
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
	return MapViewOfFile(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size);
#else
	void* ptr = mmap(0, size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED
#ifdef __x86_64__
			| (ensure_low_mem ? MAP_32BIT : 0)
#endif
			, fd, offset);
	return ptr;
#endif
}


void* MemArena::CreateViewAt(s64 offset, size_t size, void* base)
{
#ifdef _WIN32
	return MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
#else
	return mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, offset);
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


u8* MemArena::Find4GBBase()
{
#ifdef _M_X64
#ifdef _WIN32
	// 64 bit
	u8* base = (u8*)VirtualAlloc(0, 0xE1000000, MEM_RESERVE, PAGE_READWRITE);
	VirtualFree(base, 0, MEM_RELEASE);
	return base;
#else
	// Very precarious - mmap cannot return an error when trying to map already used pages.
	// This makes the Windows approach above unusable on Linux, so we will simply pray...
	return reinterpret_cast<u8*>(0x2300000000ULL);
#endif

#else
	// 32 bit
#ifdef _WIN32
	// The highest thing in any 1GB section of memory space is the locked cache. We only need to fit it.
	u8* base = (u8*)VirtualAlloc(0, 0x31000000, MEM_RESERVE, PAGE_READWRITE);
	if (base) {
		VirtualFree(base, 0, MEM_RELEASE);
	}
	return base;
#else
	void* base = mmap(0, 0x31000000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
	if (base == MAP_FAILED) {
		PanicAlert("Failed to map 1 GB of memory space: %s", strerror(errno));
		return 0;
	}
	munmap(base, 0x31000000);
	return static_cast<u8*>(base);
#endif
#endif
}


// yeah, this could also be done in like two bitwise ops...
#define SKIP(a_flags, b_flags) \
	if (!(a_flags & MV_WII_ONLY) && (b_flags & MV_WII_ONLY)) \
		continue; \
	if (!(a_flags & MV_FAKE_VMEM) && (b_flags & MV_FAKE_VMEM)) \
		continue; \


static bool Memory_TryBase(u8 *base, const MemoryView *views, int num_views, u32 flags, MemArena *arena) {
	// OK, we know where to find free space. Now grab it!
	// We just mimic the popular BAT setup.
	u32 position = 0;
	u32 last_position = 0;

	// Zero all the pointers to be sure.
	for (int i = 0; i < num_views; i++)
	{
		if (views[i].out_ptr_low)
			*views[i].out_ptr_low = 0;
		if (views[i].out_ptr)
			*views[i].out_ptr = 0;
	}

	int i;
	for (i = 0; i < num_views; i++)
	{
		SKIP(flags, views[i].flags);
		if (views[i].flags & MV_MIRROR_PREVIOUS) {
			position = last_position;
		} else {
			*(views[i].out_ptr_low) = (u8*)arena->CreateView(position, views[i].size);
			if (!*views[i].out_ptr_low)
				goto bail;
		}
#ifdef _M_X64
		*views[i].out_ptr = (u8*)arena->CreateViewAt(
			position, views[i].size, base + views[i].virtual_address);
#else
		if (views[i].flags & MV_MIRROR_PREVIOUS) {
			// No need to create multiple identical views.
			*views[i].out_ptr = *views[i - 1].out_ptr;
		} else {
			*views[i].out_ptr = (u8*)arena->CreateViewAt(
				position, views[i].size, base + (views[i].virtual_address & 0x3FFFFFFF));
			if (!*views[i].out_ptr)
				goto bail;
		}
#endif
		last_position = position;
		position += views[i].size;
	}

	return true;

bail:
	// Argh! ERROR! Free what we grabbed so far so we can try again.
	for (int j = 0; j <= i; j++)
	{
		SKIP(flags, views[i].flags);
		if (views[j].out_ptr_low && *views[j].out_ptr_low)
		{
			arena->ReleaseView(*views[j].out_ptr_low, views[j].size);
			*views[j].out_ptr_low = NULL;
		}
		if (*views[j].out_ptr)
		{
#ifdef _M_X64
			arena->ReleaseView(*views[j].out_ptr, views[j].size);
#else
			if (!(views[j].flags & MV_MIRROR_PREVIOUS))
			{
				arena->ReleaseView(*views[j].out_ptr, views[j].size);
			}
#endif
			*views[j].out_ptr = NULL;
		}
	}
	return false;
}

u8 *MemoryMap_Setup(const MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	u32 total_mem = 0;
	int base_attempts = 0;

	for (int i = 0; i < num_views; i++)
	{
		SKIP(flags, views[i].flags);
		if ((views[i].flags & MV_MIRROR_PREVIOUS) == 0)
			total_mem += views[i].size;
	}
	// Grab some pagefile backed memory out of the void ...
	arena->GrabLowMemSpace(total_mem);

	// Now, create views in high memory where there's plenty of space.
#ifdef _M_X64
	u8 *base = MemArena::Find4GBBase();
	// This really shouldn't fail - in 64-bit, there will always be enough
	// address space.
	if (!Memory_TryBase(base, views, num_views, flags, arena))
	{
		PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
		exit(0);
		return 0;
	}
#else
#ifdef _WIN32
	// Try a whole range of possible bases. Return once we got a valid one.
	u32 max_base_addr = 0x7FFF0000 - 0x31000000;
	u8 *base = NULL;

	for (u32 base_addr = 0x40000; base_addr < max_base_addr; base_addr += 0x40000)
	{
		base_attempts++;
		base = (u8 *)base_addr;
		if (Memory_TryBase(base, views, num_views, flags, arena)) 
		{
			INFO_LOG(MEMMAP, "Found valid memory base at %p after %i tries.", base, base_attempts);
			base_attempts = 0;
			break;
		}
		
	}
#else
	// Linux32 is fine with the x64 method, although limited to 32-bit with no automirrors.
	u8 *base = MemArena::Find4GBBase();
	if (!Memory_TryBase(base, views, num_views, flags, arena))
	{
		PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
		exit(0);
		return 0;
	}
#endif

#endif
	if (base_attempts)
		PanicAlert("No possible memory base pointer found!");
	return base;
}

void MemoryMap_Shutdown(const MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	for (int i = 0; i < num_views; i++)
	{
		SKIP(flags, views[i].flags);
		if (views[i].out_ptr_low && *views[i].out_ptr_low)
			arena->ReleaseView(*views[i].out_ptr_low, views[i].size);
		if (*views[i].out_ptr && (views[i].out_ptr_low && *views[i].out_ptr != *views[i].out_ptr_low))
			arena->ReleaseView(*views[i].out_ptr, views[i].size);
		*views[i].out_ptr = NULL;
		if (views[i].out_ptr_low) 
			*views[i].out_ptr_low = NULL;
	}
}
