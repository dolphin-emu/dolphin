// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <array>
#include <atomic>
#include <string>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Timer.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"

class PointerWrap;
struct libusb_context;
struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

namespace IOS
{
namespace HLE
{
enum class SyncButtonState
{
  Unpressed,
  Held,
  Pressed,
  LongPressed,
  // On a real Wii, after a long press, the button release is ignored and doesn't trigger a sync.
  Ignored,
};

using btaddr_t = std::array<u8, 6>;
using linkkey_t = std::array<u8, 16>;

class CWII_IPC_HLE_Device_usb_oh1_57e_305_real final
    : public CWII_IPC_HLE_Device_usb_oh1_57e_305_base
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_real(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_oh1_57e_305_real() override;

  IOSReturnCode Open(const IOSOpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtlV(const IOSIOCtlVRequest& request) override;

  void DoState(PointerWrap& p) override;
  void UpdateSyncButtonState(bool is_held) override;
  void TriggerSyncButtonPressedEvent() override;
  void TriggerSyncButtonHeldEvent() override;

private:
  static constexpr u8 INTERFACE = 0x00;
  // Arbitrarily chosen value that allows emulated software to send commands often enough
  // so that the sync button event is triggered at least every 200ms.
  // Ideally this should be equal to 0, so we don't trigger unnecessary libusb transfers.
  static constexpr int TIMEOUT = 200;
  static constexpr int SYNC_BUTTON_HOLD_MS_TO_RESET = 10000;

  std::atomic<SyncButtonState> m_sync_button_state{SyncButtonState::Unpressed};
  Common::Timer m_sync_button_held_timer;

  libusb_device* m_device = nullptr;
  libusb_device_handle* m_handle = nullptr;
  libusb_context* m_libusb_context = nullptr;

  Common::Flag m_thread_running;
  std::thread m_thread;

  // Set when we received a command to which we need to fake a reply
  Common::Flag m_fake_read_buffer_size_reply;
  Common::Flag m_fake_vendor_command_reply;
  u16 m_fake_vendor_command_reply_opcode;

  bool m_is_wii_bt_module = false;

  void WaitForHCICommandComplete(u16 opcode);
  void SendHCIResetCommand();
  void SendHCIDeleteLinkKeyCommand();
  bool SendHCIStoreLinkKeyCommand();
  void FakeVendorCommandReply(CtrlBuffer& ctrl);
  void FakeReadBufferSizeReply(CtrlBuffer& ctrl);
  void FakeSyncButtonEvent(CtrlBuffer& ctrl, const u8* payload, u8 size);
  void FakeSyncButtonPressedEvent(CtrlBuffer& ctrl);
  void FakeSyncButtonHeldEvent(CtrlBuffer& ctrl);

  void LoadLinkKeys();
  void SaveLinkKeys();

  bool OpenDevice(libusb_device* device);
  void StartTransferThread();
  void StopTransferThread();
  void TransferThread();
  static void CommandCallback(libusb_transfer* transfer);
  static void TransferCallback(libusb_transfer* transfer);
};
}  // namespace HLE
}  // namespace IOS

#else
#include "Core/IOS/USB/Bluetooth/BTStub.h"

namespace IOS
{
namespace HLE
{
using CWII_IPC_HLE_Device_usb_oh1_57e_305_real = CWII_IPC_HLE_Device_usb_oh1_57e_305_stub;
}  // namespace HLE
}  // namespace IOS
#endif
