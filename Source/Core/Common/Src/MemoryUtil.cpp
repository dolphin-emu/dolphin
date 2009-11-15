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
#include "MemoryUtil.h"
#include "StringUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
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

// This is purposely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(size_t size, bool low)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if ((u64)ptr >= 0x80000000)
	{
		PanicAlert("Executable memory ended up above 2GB!");
		// If this happens, we have to implement a free ram search scheme. ector knows how.
	}

	return ptr;

#else
	void* retval = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE
#ifdef __x86_64__
		 | (low ? MAP_32BIT : 0)
#endif
         , -1, 0);  // | MAP_FIXED
	// printf("Mapped executable memory at %p (size %i)\n", retval, size);

	if (!retval)
	{
		PanicAlert("Failed to allocate executable memory, errno=%i", errno);
	}

	return retval;
#endif

}

void* AllocateMemoryPages(size_t size)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);

	if (!ptr)
	{
		PanicAlert("Failed to allocate raw memory");
	}

	return ptr;

#else
	void* retval = mmap(0, size, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); // | MAP_FIXED
	// printf("Mapped memory at %p (size %i)\n", retval, size);

	if (!retval)
	{
		PanicAlert("Failed to allocate raw memory, errno=%i", errno);
	}

	return retval;
#endif

}

void FreeMemoryPages(void* ptr, size_t size)
{
#ifdef _WIN32
	if (ptr)
	{
		if (!VirtualFree(ptr, 0, MEM_RELEASE))
			PanicAlert("FreeMemoryPages failed!\n%s", GetLastErrorMsg());
		ptr = NULL; // Is this our responsibility?
	}
#else
	munmap(ptr, size);
#endif
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, &oldValue))
		PanicAlert("WriteProtectMemory failed!\n%s", GetLastErrorMsg());
#else
	mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ);
#endif
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &oldValue))
		PanicAlert("UnWriteProtectMemory failed!\n%s", GetLastErrorMsg());
#else
	mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_WRITE | PROT_EXEC) : PROT_WRITE | PROT_READ);
#endif
}


std::string MemUsage()
{
	#ifdef _WIN32
	DWORD processID = GetCurrentProcessId();
    HANDLE hProcess;
    PROCESS_MEMORY_COUNTERS pmc;
	std::string Ret;

    // Print information about the memory usage of the process.

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (NULL == hProcess) return "MemUsage Error";

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		Ret = StringFromFormat("%s K", ThS((int)(pmc.WorkingSetSize / 1024), true, 7).c_str());

    CloseHandle(hProcess);
	return Ret;

	#else
	return "";
	#endif
}
