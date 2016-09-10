// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <thread>

#include <libusb.h>

#include "Common/Flag.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"

enum class SyncButtonState
{
  Unpressed,
  Held,
  Pressed,
  LongPressed,
  // On a real Wii, after a long press, the button release is ignored and doesn't trigger a sync.
  Ignored,
};

class CWII_IPC_HLE_Device_usb_oh1_57e_305_real final
    : public CWII_IPC_HLE_Device_usb_oh1_57e_305_base
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_real(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_oh1_57e_305_real() override;

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtlV(u32 command_address) override;

  void DoState(PointerWrap& p) override;
  u32 Update() override { return 0; }
  static void UpdateSyncButtonState(bool is_held);
  // These two functions trigger the events immediately, unlike UpdateSyncButtonState.
  static void TriggerSyncButtonPressedEvent();
  static void TriggerSyncButtonHeldEvent();

private:
  static constexpr u8 INTERFACE = 0x00;
  static constexpr u8 SUBCLASS = 0x01;
  static constexpr u8 PROTOCOL_BLUETOOTH = 0x01;
  // Arbitrarily chosen value that allows emulated software to send commands often enough
  // so that the sync button event is triggered at least every 200ms.
  // Ideally this should be equal to 0, so we don't trigger unnecessary libusb transfers.
  static constexpr int TIMEOUT = 200;
  static constexpr int SYNC_BUTTON_HOLD_MS_TO_RESET = 10000;

  libusb_device* m_device = nullptr;
  libusb_device_handle* m_handle = nullptr;
  libusb_context* m_libusb_context = nullptr;

  Common::Flag m_thread_running;
  std::thread m_thread;

  void SendHCIResetCommand();
  void FakeSyncButtonEvent(const CtrlBuffer& ctrl, const u8* payload, u8 size);
  void FakeSyncButtonPressedEvent(const CtrlBuffer& ctrl);
  void FakeSyncButtonHeldEvent(const CtrlBuffer& ctrl);

  bool OpenDevice(libusb_device* device);
  void StartTransferThread();
  void StopTransferThread();
  void TransferThread();
  static void CommandCallback(libusb_transfer* transfer);
  static void TransferCallback(libusb_transfer* transfer);
};

#else
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"
using CWII_IPC_HLE_Device_usb_oh1_57e_305_real = CWII_IPC_HLE_Device_usb_oh1_57e_305_stub;
#endif
