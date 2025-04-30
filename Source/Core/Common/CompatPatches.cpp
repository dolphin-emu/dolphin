// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <Windows.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <winternl.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/LdrWatcher.h"
#include "Common/StringUtil.h"

typedef NTSTATUS(NTAPI* PRTL_HEAP_COMMIT_ROUTINE)(IN PVOID Base, IN OUT PVOID* CommitAddress,
                                                  IN OUT PSIZE_T CommitSize);

typedef struct _RTL_HEAP_PARAMETERS
{
  ULONG Length;
  SIZE_T SegmentReserve;
  SIZE_T SegmentCommit;
  SIZE_T DeCommitFreeBlockThreshold;
  SIZE_T DeCommitTotalFreeThreshold;
  SIZE_T MaximumAllocationSize;
  SIZE_T VirtualMemoryThreshold;
  SIZE_T InitialCommit;
  SIZE_T InitialReserve;
  PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
  SIZE_T Reserved[2];
} RTL_HEAP_PARAMETERS, *PRTL_HEAP_PARAMETERS;

typedef PVOID (*RtlCreateHeap_t)(_In_ ULONG Flags, _In_opt_ PVOID HeapBase,
                                 _In_opt_ SIZE_T ReserveSize, _In_opt_ SIZE_T CommitSize,
                                 _In_opt_ PVOID Lock, _In_opt_ PRTL_HEAP_PARAMETERS Parameters);

static HANDLE WINAPI HeapCreateLow4GB(_In_ DWORD flOptions, _In_ SIZE_T dwInitialSize,
                                      _In_ SIZE_T dwMaximumSize)
{
  auto ntdll = GetModuleHandleW(L"ntdll");
  if (!ntdll)
    return nullptr;
  auto RtlCreateHeap = reinterpret_cast<RtlCreateHeap_t>(GetProcAddress(ntdll, "RtlCreateHeap"));
  if (!RtlCreateHeap)
    return nullptr;
  // These values are arbitrary; just change them if problems are encountered later.
  uintptr_t target_addr = 0x00200000;
  size_t max_heap_size = 0x01000000;
  uintptr_t highest_addr = (1ull << 32) - max_heap_size;
  void* low_heap = nullptr;
  for (; !low_heap && target_addr <= highest_addr; target_addr += 0x1000)
    low_heap = VirtualAlloc((void*)target_addr, max_heap_size, MEM_RESERVE, PAGE_READWRITE);
  if (!low_heap)
    return nullptr;
  return RtlCreateHeap(0, low_heap, 0, 0, nullptr, nullptr);
}

static bool ModifyProtectedRegion(void* address, size_t size, std::function<void()> func)
{
  DWORD old_protect;
  if (!VirtualProtect(address, size, PAGE_READWRITE, &old_protect))
    return false;
  func();
  if (!VirtualProtect(address, size, old_protect, &old_protect))
    return false;
  return true;
}

// Does not do input sanitization - assumes well-behaved input since Ldr has already parsed it.
class ImportPatcher
{
public:
  ImportPatcher(uintptr_t module_base) : base(module_base)
  {
    auto mz = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    auto pe = reinterpret_cast<PIMAGE_NT_HEADERS>(base + mz->e_lfanew);
    directories = pe->OptionalHeader.DataDirectory;
  }
  template <typename T>
  T GetRva(uint32_t rva)
  {
    return reinterpret_cast<T>(base + rva);
  }
  bool PatchIAT(const char* module_name, const char* function_name, void* value)
  {
    auto import_dir = &directories[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto import_desc = GetRva<PIMAGE_IMPORT_DESCRIPTOR>(import_dir->VirtualAddress);
         import_desc->OriginalFirstThunk; import_desc++)
    {
      auto module = GetRva<const char*>(import_desc->Name);
      auto names = GetRva<PIMAGE_THUNK_DATA>(import_desc->OriginalFirstThunk);
      auto thunks = GetRva<PIMAGE_THUNK_DATA>(import_desc->FirstThunk);
      if (!stricmp(module, module_name))
      {
        for (auto name = names; name->u1.Function; name++)
        {
          if (!IMAGE_SNAP_BY_ORDINAL(name->u1.Ordinal))
          {
            auto import = GetRva<PIMAGE_IMPORT_BY_NAME>(name->u1.AddressOfData);
            if (!strcmp(import->Name, function_name))
            {
              auto index = name - names;
              return ModifyProtectedRegion(&thunks[index], sizeof(thunks[index]), [=] {
                thunks[index].u1.Function =
                    reinterpret_cast<decltype(thunks[index].u1.Function)>(value);
              });
            }
          }
        }
        // Function not found
        return false;
      }
    }
    // Module not found
    return false;
  }

private:
  uintptr_t base;
  PIMAGE_DATA_DIRECTORY directories;
};

void CompatPatchesInstall(LdrWatcher* watcher)
{
  watcher->Install({{L"EZFRD64.dll", L"811EZFRD64.DLL"}, [](const LdrDllLoadEvent& event) {
                      // *EZFRD64 is included in software packages for cheapo third-party gamepads
                      // (and gamepad adapters). The module cannot handle its heap being above 4GB,
                      // which tends to happen very often on modern Windows.
                      // NOTE: The patch will always be applied, but it will only actually avoid the
                      // crash if applied before module initialization (i.e. called on the Ldr
                      // callout path).
                      auto patcher = ImportPatcher(event.base_address);
                      patcher.PatchIAT("kernel32.dll", "HeapCreate", HeapCreateLow4GB);
                    }});
}

int __cdecl EnableCompatPatches()
{
  static LdrWatcher watcher;
  CompatPatchesInstall(&watcher);
  return 0;
}

// Create a segment which is recognized by the linker to be part of the CRT
// initialization. XI* = C startup, XC* = C++ startup. "A" placement is reserved
// for system use. C startup is before C++.
// Use last C++ slot in hopes that makes using C++ from this code safe.
#pragma section(".CRT$XCZ", read)

// Place a symbol in the special segment, make it have C linkage so that
// referencing it doesn't require ugly decorated names.
// Use /include:enableCompatPatches linker flag to enable this.
extern "C" {
__declspec(allocate(".CRT$XCZ")) decltype(&EnableCompatPatches) enableCompatPatches =
    EnableCompatPatches;
}
