// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#if defined(_WIN32)

#include "CommonTypes.h"
#include <Windows.h>

struct PatchInfo {
	const wchar_t *module_name;
	u32 checksum;
	u32 rva;
	u32 length;
} static const s_patches[] = {
	// 10.0.10240.16384 (th1.150709-1700)
	{ L"ucrtbase.dll", 0xF61ED, 0x6AE7B, 5 },
	// 10.0.10240.16390 (th1_st1.150714-1601)
	{ L"ucrtbase.dll", 0xF5ED9, 0x6AE7B, 5 },
	// 10.0.10137.0 (th1.150602-2238)
	{ L"ucrtbase.dll", 0xF8B5E, 0x63ED6, 2 },
	// 10.0.10150.0 (th1.150616-1659)
	{ L"ucrtbased.dll", 0x1C1915 , 0x91905, 5 },
};

bool ApplyPatch(const PatchInfo &patch) {
	auto module = GetModuleHandleW(patch.module_name);
	if (module == nullptr)
	{
		return false;
	}

	auto ucrtbase_pe = (PIMAGE_NT_HEADERS)((uintptr_t)module + ((PIMAGE_DOS_HEADER)module)->e_lfanew);
	if (ucrtbase_pe->OptionalHeader.CheckSum != patch.checksum) {
		return false;
	}

	void *patch_addr = (void *)((uintptr_t)module + patch.rva);
	size_t patch_size = patch.length;

	DWORD old_protect;
	if (!VirtualProtect(patch_addr, patch_size, PAGE_EXECUTE_READWRITE, &old_protect))
	{
		return false;
	}

	memset(patch_addr, 0x90, patch_size);

	VirtualProtect(patch_addr, patch_size, old_protect, &old_protect);

	FlushInstructionCache(GetCurrentProcess(), patch_addr, patch_size);

	return true;
}

int __cdecl EnableucrtFreadWorkaround()
{
	// This patches ucrtbase such that fseek will always
	// synchronize the file object's internal buffer.

	bool applied_at_least_one = false;
	for (const auto &patch : s_patches) {
		if (ApplyPatch(patch)) {
			applied_at_least_one = true;
		}
	}

	/* For forward compat, do not fail if patches don't apply (e.g. version mismatch)
	if (!applied_at_least_one) {
		std::abort();
	}
	//*/

	return 0;
}

// Create a segment which is recognized by the linker to be part of the CRT
// initialization. XI* = C startup, XC* = C++ startup. "A" placement is reserved
// for system use. Thus, the earliest we can get is XIB (C startup is before
// C++).
#pragma section(".CRT$XIB", read)

// Place a symbol in the special segment, make it have C linkage so that
// referencing it doesn't require ugly decorated names.
// Use /include:EnableucrtFreadWorkaround linker flag to enable this.
extern "C" {
	__declspec(allocate(".CRT$XIB"))
	decltype(&EnableucrtFreadWorkaround) ucrtFreadWorkaround = EnableucrtFreadWorkaround;
};

#endif
