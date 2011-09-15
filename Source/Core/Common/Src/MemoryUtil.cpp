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
#else
#include <errno.h>
#include <stdio.h>
#endif

#if !defined(_WIN32) && defined(__x86_64__) && !defined(MAP_32BIT)
#include <unistd.h>
#define PAGE_MASK     (getpagesize() - 1)
#define round_page(x) ((((unsigned long)(x)) + PAGE_MASK) & ~(PAGE_MASK))
#endif

// This is purposely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(size_t size, bool low)
{
#if defined(_WIN32)
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
	static char *map_hint = 0;
#if defined(__x86_64__) && !defined(MAP_32BIT)
	if (low && (!map_hint))
		map_hint = (char*)round_page(512*1024*1024); /* 0.5 GB rounded up to the next page */
#endif
	void* ptr = mmap(map_hint, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANON | MAP_PRIVATE
#if defined(__x86_64__)
#if defined(MAP_32BIT)
		| (low ? MAP_32BIT : 0)
#else
		| (low ? MAP_FIXED : 0)
#endif /* defined(MAP_32BIT) */
#endif /* defined(__x86_64__) */
		, -1, 0);
#endif /* defined(_WIN32) */

	// printf("Mapped executable memory at %p (size %ld)\n", ptr,
	//	(unsigned long)size);
	
#if defined(__FreeBSD__)
	if (ptr == MAP_FAILED)
	{
		ptr = NULL;
#else
	if (ptr == NULL)
	{
#endif	
		PanicAlert("Failed to allocate executable memory");
	}
#if !defined(_WIN32) && defined(__x86_64__) && !defined(MAP_32BIT)
	else
	{
		if (low)
		{
			map_hint += size;
			map_hint = (char*)round_page(map_hint); /* round up to the next page */
			// printf("Next map will (hopefully) be at %p\n", map_hint);
		}
	}
#endif

#if defined(_M_X64)
	if ((u64)ptr >= 0x80000000 && low == true)
		PanicAlert("Executable memory ended up above 2GB!");
#endif

	return ptr;
}

void* AllocateMemoryPages(size_t size)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
#else
	void* ptr = mmap(0, size, PROT_READ | PROT_WRITE,
			MAP_ANON | MAP_PRIVATE, -1, 0);
#endif

	// printf("Mapped memory at %p (size %ld)\n", ptr,
	//	(unsigned long)size);

	if (ptr == NULL)
		PanicAlert("Failed to allocate raw memory");

	return ptr;
}

void* AllocateAlignedMemory(size_t size,size_t alignment)
{
#ifdef _WIN32
	void* ptr =  _aligned_malloc(size,alignment);
#else
	void* ptr = NULL;
	posix_memalign(&ptr, alignment, size);
;
#endif

	// printf("Mapped memory at %p (size %ld)\n", ptr,
	//	(unsigned long)size);

	if (ptr == NULL)
		PanicAlert("Failed to allocate aligned memory");

	return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
	if (ptr)
	{
#ifdef _WIN32
	
		if (!VirtualFree(ptr, 0, MEM_RELEASE))
			PanicAlert("FreeMemoryPages failed!\n%s", GetLastErrorMsg());
		ptr = NULL; // Is this our responsibility?
	
#else
		munmap(ptr, size);
#endif
	}
}

void FreeAlignedMemory(void* ptr)
{
	if (ptr)
	{
#ifdef _WIN32
	
		_aligned_free(ptr);
	
#else
	free(ptr);
#endif
	}
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
#pragma comment(lib, "psapi")
	DWORD processID = GetCurrentProcessId();
	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;
	std::string Ret;

	// Print information about the memory usage of the process.

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (NULL == hProcess) return "MemUsage Error";

	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		Ret = StringFromFormat("%s K", ThousandSeparate(pmc.WorkingSetSize / 1024, 7).c_str());

	CloseHandle(hProcess);
	return Ret;
#else
	return "";
#endif
}
