// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdlib>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include "Common/StringUtil.h"
#else
#include <stdio.h>
#include <sys/mman.h>
#endif

#if !defined(_WIN32) && defined(_M_X86_64) && !defined(MAP_32BIT)
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
	static char *map_hint = nullptr;
#if defined(_M_X86_64) && !defined(MAP_32BIT)
	// This OS has no flag to enforce allocation below the 4 GB boundary,
	// but if we hint that we want a low address it is very likely we will
	// get one.
	// An older version of this code used MAP_FIXED, but that has the side
	// effect of discarding already mapped pages that happen to be in the
	// requested virtual memory range (such as the emulated RAM, sometimes).
	if (low && (!map_hint))
		map_hint = (char*)round_page(512*1024*1024); /* 0.5 GB rounded up to the next page */
#endif
	void* ptr = mmap(map_hint, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANON | MAP_PRIVATE
#if defined(_M_X86_64) && defined(MAP_32BIT)
		| (low ? MAP_32BIT : 0)
#endif
		, -1, 0);
#endif /* defined(_WIN32) */

	// printf("Mapped executable memory at %p (size %ld)\n", ptr,
	//	(unsigned long)size);

#ifdef _WIN32
	if (ptr == nullptr)
	{
#else
	if (ptr == MAP_FAILED)
	{
		ptr = nullptr;
#endif
		PanicAlert("Failed to allocate executable memory");
	}
#if !defined(_WIN32) && defined(_M_X86_64) && !defined(MAP_32BIT)
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

#if _M_X86_64
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
	void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
			MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED)
		ptr = nullptr;
#endif

	if (ptr == nullptr)
		PanicAlert("Failed to allocate raw memory");

	return ptr;
}

void* AllocateAlignedMemory(size_t size,size_t alignment)
{
#ifdef _WIN32
	void* ptr =  _aligned_malloc(size,alignment);
#else
	void* ptr = nullptr;
#ifdef ANDROID
	ptr = memalign(alignment, size);
#else
	if (posix_memalign(&ptr, alignment, size) != 0)
		ERROR_LOG(MEMMAP, "Failed to allocate aligned memory");
#endif
#endif

	// printf("Mapped memory at %p (size %ld)\n", ptr,
	//	(unsigned long)size);

	if (ptr == nullptr)
		PanicAlert("Failed to allocate aligned memory");

	return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
	if (ptr)
	{
		bool error_occurred = false;

#ifdef _WIN32
		if (!VirtualFree(ptr, 0, MEM_RELEASE))
			error_occurred = true;
#else
		int retval = munmap(ptr, size);

		if (retval != 0)
			error_occurred = true;
#endif

		if (error_occurred)
			PanicAlert("FreeMemoryPages failed!\n%s", GetLastErrorMsg());
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

void ReadProtectMemory(void* ptr, size_t size)
{
	bool error_occurred = false;

#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, PAGE_NOACCESS, &oldValue))
		error_occurred = true;
#else
	int retval = mprotect(ptr, size, PROT_NONE);

	if (retval != 0)
		error_occurred = true;
#endif

	if (error_occurred)
		PanicAlert("ReadProtectMemory failed!\n%s", GetLastErrorMsg());
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
	bool error_occurred = false;

#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, &oldValue))
		error_occurred = true;
#else
	int retval = mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ);

	if (retval != 0)
		error_occurred = true;
#endif

	if (error_occurred)
		PanicAlert("WriteProtectMemory failed!\n%s", GetLastErrorMsg());
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
	bool error_occurred = false;

#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &oldValue))
		error_occurred = true;
#else
	int retval = mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_WRITE | PROT_EXEC) : PROT_WRITE | PROT_READ);

	if (retval != 0)
		error_occurred = true;
#endif

	if (error_occurred)
		PanicAlert("UnWriteProtectMemory failed!\n%s", GetLastErrorMsg());
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
	if (nullptr == hProcess) return "MemUsage Error";

	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		Ret = StringFromFormat("%s K", ThousandSeparate(pmc.WorkingSetSize / 1024, 7).c_str());

	CloseHandle(hProcess);
	return Ret;
#else
	return "";
#endif
}
