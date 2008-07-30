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
#include "MemoryUtil.h"

#ifdef _WIN32
#include <windows.h>
#elif __GNUC__
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif 

// MacOSX does not support MAP_VARIABLE
#ifndef MAP_VARIABLE
#define MAP_VARIABLE 0
#endif

// This is purposedely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(int size)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if ((u64)ptr >= 0x80000000)
	{
		PanicAlert("Executable memory ended up above 2GB!");
		// If this happens, we have to implement a free ram search scheme. ector knows how.
	}

	return(ptr);

#else
	void* retval = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);  // | MAP_FIXED
	printf("mappah exe %p %i\n", retval, size);

	if (!retval)
	{
		PanicAlert("Failed to allocate executable memory, errno=%i", errno);
	}

	return(retval);
#endif

}


void* AllocateMemoryPages(int size)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);

	if (!ptr)
	{
		PanicAlert("Failed to allocate raw memory");
	}

	return(ptr);

#else
	void* retval = mmap(0, size, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); // | MAP_FIXED
	printf("mappah %p %i\n", retval, size);

	if (!retval)
	{
		PanicAlert("Failed to allocate raw memory, errno=%i", errno);
	}

	return(retval);
#endif

}


void FreeMemoryPages(void* ptr, int size)
{
#ifdef _WIN32
	VirtualFree(ptr, size, MEM_RELEASE);
#else
	munmap(ptr, size);
#endif
}


void WriteProtectMemory(void* ptr, int size, bool allowExecute)
{
#ifdef _WIN32
	VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, 0);
#else
	mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ);
#endif
}


void UnWriteProtectMemory(void* ptr, int size, bool allowExecute)
{
#ifdef _WIN32
	VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READWRITE : PAGE_READONLY, 0);
#else
	mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_WRITE | PROT_EXEC) : PROT_WRITE | PROT_READ);
#endif
}

