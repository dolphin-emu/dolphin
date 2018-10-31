// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <winternl.h>

#include "Common/LdrWatcher.h"

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
{
  ULONG Flags;                   // Reserved.
  PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
  PVOID DllBase;                 // A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;             // The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA
{
  ULONG Flags;                   // Reserved.
  PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
  PVOID DllBase;                 // A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;             // The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA
{
  LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
  LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;
typedef const LDR_DLL_NOTIFICATION_DATA* PCLDR_DLL_NOTIFICATION_DATA;

#define LDR_DLL_NOTIFICATION_REASON_LOADED (1)
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED (2)

typedef VOID NTAPI LDR_DLL_NOTIFICATION_FUNCTION(_In_ ULONG NotificationReason,
                                                 _In_ PCLDR_DLL_NOTIFICATION_DATA NotificationData,
                                                 _In_opt_ PVOID Context);
typedef LDR_DLL_NOTIFICATION_FUNCTION* PLDR_DLL_NOTIFICATION_FUNCTION;

typedef NTSTATUS(NTAPI* LdrRegisterDllNotification_t)(
    _In_ ULONG Flags, _In_ PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
    _In_opt_ PVOID Context, _Out_ PVOID* Cookie);

typedef NTSTATUS(NTAPI* LdrUnregisterDllNotification_t)(_In_ PVOID Cookie);

static void LdrObserverRun(const LdrObserver& observer, PCUNICODE_STRING module_name,
                           uintptr_t base_address)
{
  for (auto& needle : observer.module_names)
  {
    // Like RtlCompareUnicodeString, but saves dynamically resolving it.
    // NOTE: Does not compare null terminator.
    auto compare_length = module_name->Length / sizeof(wchar_t);
    if (!_wcsnicmp(needle.c_str(), module_name->Buffer, compare_length))
      observer.action({needle, base_address});
  }
}

static VOID DllNotificationCallback(ULONG NotificationReason,
                                    PCLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
{
  if (NotificationReason != LDR_DLL_NOTIFICATION_REASON_LOADED)
    return;
  auto& data = NotificationData->Loaded;
  auto observer = static_cast<const LdrObserver*>(Context);
  LdrObserverRun(*observer, data.BaseDllName, reinterpret_cast<uintptr_t>(data.DllBase));
}

// This only works on Vista+. On lower platforms, it will be a no-op.
class LdrDllNotifier
{
public:
  static LdrDllNotifier& GetInstance()
  {
    static LdrDllNotifier notifier;
    return notifier;
  };
  void Install(LdrObserver* observer);
  void Uninstall(LdrObserver* observer);

private:
  LdrDllNotifier();
  bool Init();
  LdrRegisterDllNotification_t LdrRegisterDllNotification{};
  LdrUnregisterDllNotification_t LdrUnregisterDllNotification{};
  bool initialized{};
};

LdrDllNotifier::LdrDllNotifier()
{
  initialized = Init();
}

bool LdrDllNotifier::Init()
{
  auto ntdll = GetModuleHandleW(L"ntdll");
  if (!ntdll)
    return false;
  LdrRegisterDllNotification = reinterpret_cast<decltype(LdrRegisterDllNotification)>(
      GetProcAddress(ntdll, "LdrRegisterDllNotification"));
  if (!LdrRegisterDllNotification)
    return false;
  LdrUnregisterDllNotification = reinterpret_cast<decltype(LdrUnregisterDllNotification)>(
      GetProcAddress(ntdll, "LdrUnregisterDllNotification"));
  if (!LdrUnregisterDllNotification)
    return false;
  return true;
}

void LdrDllNotifier::Install(LdrObserver* observer)
{
  if (!initialized)
    return;
  void* cookie{};
  if (!NT_SUCCESS(LdrRegisterDllNotification(0, DllNotificationCallback,
                                             static_cast<PVOID>(observer), &cookie)))
    cookie = {};
  observer->cookie = cookie;
  return;
}

void LdrDllNotifier::Uninstall(LdrObserver* observer)
{
  if (!initialized)
    return;
  LdrUnregisterDllNotification(observer->cookie);
  observer->cookie = {};
  return;
}

LdrWatcher::~LdrWatcher()
{
  UninstallAll();
}

// Needed for RtlInitUnicodeString
#pragma comment(lib, "ntdll")

bool LdrWatcher::InjectCurrentModules(const LdrObserver& observer)
{
  // Use TlHelp32 instead of psapi functions to reduce dolphin's dependency on psapi
  // (revisit this when Win7 support is dropped).
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE)
    return false;
  MODULEENTRY32 entry;
  entry.dwSize = sizeof(entry);
  for (BOOL rv = Module32First(snapshot, &entry); rv == TRUE; rv = Module32Next(snapshot, &entry))
  {
    UNICODE_STRING module_name;
    RtlInitUnicodeString(&module_name, entry.szModule);
    LdrObserverRun(observer, &module_name, reinterpret_cast<uintptr_t>(entry.modBaseAddr));
  }
  CloseHandle(snapshot);
  return true;
}

void LdrWatcher::Install(const LdrObserver& observer)
{
  observers.emplace_back(observer);
  auto& new_observer = observers.back();
  // Register for notifications before looking at the list of current modules.
  // This ensures none are missed, but there is a tiny chance some will be seen twice.
  LdrDllNotifier::GetInstance().Install(&new_observer);
  InjectCurrentModules(new_observer);
}

void LdrWatcher::UninstallAll()
{
  for (auto& observer : observers)
    LdrDllNotifier::GetInstance().Uninstall(&observer);
  observers.clear();
}
