// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif
#endif

void* AllocateExecutableMemory(size_t size, void* map_hint)
{
#if defined(_WIN32)
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
	static uintptr_t page_mask = ~(sysconf(_SC_PAGE_SIZE) - 1);
	map_hint = (void*)((uintptr_t)map_hint & page_mask);
	void* ptr = mmap(map_hint, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	                 MAP_ANON | MAP_PRIVATE, -1, 0);
#endif /* defined(_WIN32) */

#ifdef _WIN32
	if (ptr == nullptr)
#else
	if (ptr == MAP_FAILED)
#endif
	{
		ptr = nullptr;
		PanicAlert("Failed to allocate executable memory.");
	}

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

void* AllocateAlignedMemory(size_t size, size_t alignment)
{
	void* ptr = nullptr;
#ifdef _WIN32
	if (!(ptr = _aligned_malloc(size, alignment)))
#else
	if (posix_memalign(&ptr, alignment, size))
#endif
		PanicAlert("Failed to allocate aligned memory");

	return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
#ifdef _WIN32
	if (ptr && !VirtualFree(ptr, 0, MEM_RELEASE))
#else
	if (ptr && munmap(ptr, size))
#endif
		PanicAlert("FreeMemoryPages failed!\n%s", GetLastErrorMsg().c_str());
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
#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, PAGE_NOACCESS, &oldValue))
#else
	if (mprotect(ptr, size, PROT_NONE))
#endif
		PanicAlert("ReadProtectMemory failed!\n%s", GetLastErrorMsg().c_str());
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, &oldValue))
#else
	if (mprotect(ptr, size, PROT_READ | (allowExecute ? PROT_EXEC : 0)))
#endif
		PanicAlert("WriteProtectMemory failed!\n%s", GetLastErrorMsg().c_str());
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &oldValue))
#else
	if (mprotect(ptr, size, PROT_READ | PROT_WRITE | (allowExecute ? PROT_EXEC : 0)))
#endif
		PanicAlert("UnWriteProtectMemory failed!\n%s", GetLastErrorMsg().c_str());
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
	if (nullptr == hProcess)
		return "MemUsage Error";

	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		Ret = StringFromFormat("%s K", ThousandSeparate(pmc.WorkingSetSize / 1024, 7).c_str());

	CloseHandle(hProcess);
	return Ret;
#else
	return "";
#endif
}


size_t MemPhysical()
{
#ifdef _WIN32
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);
	return memInfo.ullTotalPhys;
#elif defined(__APPLE__)
	int mib[2];
	size_t physical_memory;
	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;
	size_t length = sizeof(size_t);
	sysctl(mib, 2, &physical_memory, &length, NULL, 0);
	return physical_memory;
#else
	struct sysinfo memInfo;
	sysinfo (&memInfo);
	return (size_t)memInfo.totalram * memInfo.mem_unit;
#endif
}
