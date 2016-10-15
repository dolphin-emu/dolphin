// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Core/Core.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"

class PointerWrap;
struct libusb_context;
struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

// /dev/usb/oh0
class CWII_IPC_HLE_Device_usb_oh0_real final : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_oh0_real(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_oh0_real() override;

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtlV(u32 command_address) override;

  void TriggerInsertionHook(u16 vid, u16 pid);

  void DoState(PointerWrap& p) override;
  u32 Update() override { return 0; }
private:
  // VID, PID and command address
  std::map<std::pair<u16, u16>, u32> m_insertion_hooks;
  std::mutex m_insertion_hooks_mutex;

  libusb_context* m_libusb_context = nullptr;
  int m_libusb_hotplug_callback_handle = -1;
  Common::Flag m_thread_running;
  std::thread m_thread;

  void SetUpHotplug();
  void StopHotplug();
  u8 GetDeviceList(u8 max_num, u8 interface_class, std::vector<u8>* buffer);
};

// This class implements an individual device (/dev/usb/oh0/xxx/xxx).
class CWII_IPC_HLE_Device_usb_oh0_real_device final : public CWII_IPC_HLE_Device_usb
{
public:
  CWII_IPC_HLE_Device_usb_oh0_real_device(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_oh0_real_device() override;

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtl(u32 command_address) override;
  IPCCommandResult IOCtlV(u32 command_address) override;

  void TriggerRemovalHook();

  void DoState(PointerWrap& p) override;
  u32 Update() override { return 0; }
private:
  u32 m_removal_hook_address = 0;
  std::mutex m_removal_hook_mutex;

  u16 m_pid;
  u16 m_vid;

  libusb_device_handle* m_handle = nullptr;
  libusb_context* m_libusb_context = nullptr;

  Common::Flag m_thread_running;
  std::thread m_thread;

  void StartThread();
  void StopThread();
  void ThreadFunc();
  static void ControlTransferCallback(libusb_transfer* transfer);
  static void TransferCallback(libusb_transfer* transfer);
  static void IsoTransferCallback(libusb_transfer* transfer);
};

#else
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_stub.h"

using CWII_IPC_HLE_Device_usb_oh0_real = CWII_IPC_HLE_Device_usb_stub;
using CWII_IPC_HLE_Device_usb_oh0_real_device = CWII_IPC_HLE_Device_usb_stub;
#endif
