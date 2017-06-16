// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>
#include <winternl.h>

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

struct UcrtPatchInfo
{
  u32 checksum;
  u32 rva;
  u32 length;
};

bool ApplyUcrtPatch(const wchar_t* name, const UcrtPatchInfo& patch)
{
  auto module = GetModuleHandleW(name);
  if (!module)
    return false;
  auto pe = (PIMAGE_NT_HEADERS)((uintptr_t)module + ((PIMAGE_DOS_HEADER)module)->e_lfanew);
  if (pe->OptionalHeader.CheckSum != patch.checksum)
    return false;
  void* patch_addr = (void*)((uintptr_t)module + patch.rva);
  size_t patch_size = patch.length;
  ModifyProtectedRegion(patch_addr, patch_size, [=] { memset(patch_addr, 0x90, patch_size); });
  FlushInstructionCache(GetCurrentProcess(), patch_addr, patch_size);
  return true;
}

#pragma comment(lib, "version.lib")

struct Version
{
  u16 major;
  u16 minor;
  u16 build;
  u16 qfe;
  Version& operator=(u64&& rhs)
  {
    major = static_cast<u16>(rhs >> 48);
    minor = static_cast<u16>(rhs >> 32);
    build = static_cast<u16>(rhs >> 16);
    qfe = static_cast<u16>(rhs);
    return *this;
  }
};

static bool GetModulePath(const wchar_t* name, std::wstring* path)
{
  auto module = GetModuleHandleW(name);
  if (module == nullptr)
    return false;
  DWORD path_len = MAX_PATH;
retry:
  path->resize(path_len);
  path_len = GetModuleFileNameW(module, const_cast<wchar_t*>(path->data()),
                                static_cast<DWORD>(path->size()));
  if (!path_len)
    return false;
  auto error = GetLastError();
  if (error == ERROR_SUCCESS)
    return true;
  if (error == ERROR_INSUFFICIENT_BUFFER)
    goto retry;
  return false;
}

static bool GetModuleVersion(const wchar_t* name, Version* version)
{
  std::wstring path;
  if (!GetModulePath(name, &path))
    return false;
  DWORD handle;
  DWORD data_len = GetFileVersionInfoSizeW(path.c_str(), &handle);
  if (!data_len)
    return false;
  std::vector<u8> block(data_len);
  if (!GetFileVersionInfoW(path.c_str(), handle, data_len, block.data()))
    return false;
  void* buf;
  UINT buf_len;
  if (!VerQueryValueW(block.data(), LR"(\)", &buf, &buf_len))
    return false;
  auto info = static_cast<VS_FIXEDFILEINFO*>(buf);
  *version = (static_cast<u64>(info->dwFileVersionMS) << 32) | info->dwFileVersionLS;
  return true;
}

void CompatPatchesInstall(LdrWatcher* watcher)
{
  watcher->Install({{L"EZFRD64.dll", L"811EZFRD64.DLL"},
                    [](const LdrDllLoadEvent& event) {
                      // *EZFRD64 is incldued in software packages for cheapo third-party gamepads
                      // (and gamepad adapters). The module cannot handle its heap being above 4GB,
                      // which tends to happen very often on modern Windows.
                      // NOTE: The patch will always be applied, but it will only actually avoid the
                      // crash if applied before module initialization (i.e. called on the Ldr
                      // callout path).
                      auto patcher = ImportPatcher(event.base_address);
                      patcher.PatchIAT("kernel32.dll", "HeapCreate", HeapCreateLow4GB);
                    }});
  watcher->Install({{L"ucrtbase.dll"},
                    [](const LdrDllLoadEvent& event) {
                      // ucrtbase implements caching between fseek/fread, old versions have a bug
                      // such that some reads return incorrect data. This causes noticable bugs
                      // in dolphin since we use these APIs for reading game images.
                      Version version;
                      if (!GetModuleVersion(event.name.c_str(), &version))
                        return;
                      const u16 fixed_build = 10548;
                      if (version.build >= fixed_build)
                        return;
                      const UcrtPatchInfo patches[] = {
                          // 10.0.10240.16384 (th1.150709-1700)
                          {0xF61ED, 0x6AE7B, 5},
                          // 10.0.10240.16390 (th1_st1.150714-1601)
                          {0xF5ED9, 0x6AE7B, 5},
                          // 10.0.10137.0 (th1.150602-2238)
                          {0xF8B5E, 0x63ED6, 2},
                      };
                      for (const auto& patch : patches)
                      {
                        if (ApplyUcrtPatch(event.name.c_str(), patch))
                          return;
                      }
                      // If we reach here, the version is buggy (afaik) and patching failed
                      auto msg = StringFromFormat(
                          "You are running %S version %d.%d.%d.%d.\n"
                          "An important fix affecting Dolphin was introduced in build %d.\n"
                          "You can use Dolphin, but there will be known bugs.\n"
                          "Please update this file by installing the latest Universal C Runtime.\n",
                          event.name.c_str(), version.major, version.minor, version.build,
                          version.qfe, fixed_build);
                      // Use MessageBox for maximal user annoyance
                      MessageBoxA(nullptr, msg.c_str(), "WARNING: BUGGY UCRT VERSION",
                                  MB_ICONEXCLAMATION);
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
__declspec(allocate(".CRT$XCZ")) decltype(&EnableCompatPatches)
    enableCompatPatches = EnableCompatPatches;
};
