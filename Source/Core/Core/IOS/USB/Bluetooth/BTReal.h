// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Timer.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/IOS/USB/Bluetooth/hci.h"
#include "Core/IOS/USB/USBV0.h"

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

using linkkey_t = std::array<u8, 16>;

namespace Device
{
class BluetoothReal final : public BluetoothBase
{
public:
  BluetoothReal(Kernel& ios, const std::string& device_name);
  ~BluetoothReal() override;

  ReturnCode Open(const OpenRequest& request) override;
  ReturnCode Close(u32 fd) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void DoState(PointerWrap& p) override;
  void UpdateSyncButtonState(bool is_held) override;
  void TriggerSyncButtonPressedEvent() override;
  void TriggerSyncButtonHeldEvent() override;

  void HandleCtrlTransfer(libusb_transfer* finished_transfer);
  void HandleBulkOrIntrTransfer(libusb_transfer* finished_transfer);

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

  std::mutex m_transfers_mutex;
  struct PendingTransfer
  {
    PendingTransfer(std::unique_ptr<USB::TransferCommand> command_, std::unique_ptr<u8[]> buffer_)
        : command(std::move(command_)), buffer(std::move(buffer_))
    {
    }
    std::unique_ptr<USB::TransferCommand> command;
    std::unique_ptr<u8[]> buffer;
  };
  std::map<libusb_transfer*, PendingTransfer> m_current_transfers;

  // Set when we received a command to which we need to fake a reply
  Common::Flag m_fake_read_buffer_size_reply;
  Common::Flag m_fake_vendor_command_reply;
  u16 m_fake_vendor_command_reply_opcode;

  // This stores the address of paired devices and associated link keys.
  // It is needed because some adapters forget all stored link keys when they are reset,
  // which breaks pairings because the Wii relies on the Bluetooth module to remember them.
  std::map<bdaddr_t, linkkey_t> m_link_keys;
  Common::Flag m_need_reset_keys;

  // This flag is set when a libusb transfer failed (for reasons other than timing out)
  // and we showed an OSD message about it.
  Common::Flag m_showed_failed_transfer;

  bool m_is_wii_bt_module = false;

  void WaitForHCICommandComplete(u16 opcode);
  void SendHCIResetCommand();
  void SendHCIDeleteLinkKeyCommand();
  bool SendHCIStoreLinkKeyCommand();
  void FakeVendorCommandReply(USB::V0IntrMessage& ctrl);
  void FakeReadBufferSizeReply(USB::V0IntrMessage& ctrl);
  void FakeSyncButtonEvent(USB::V0IntrMessage& ctrl, const u8* payload, u8 size);
  void FakeSyncButtonPressedEvent(USB::V0IntrMessage& ctrl);
  void FakeSyncButtonHeldEvent(USB::V0IntrMessage& ctrl);

  void LoadLinkKeys();
  void SaveLinkKeys();

  bool OpenDevice(libusb_device* device);
  void StartTransferThread();
  void StopTransferThread();
  void TransferThread();
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS

#else
#include "Core/IOS/USB/Bluetooth/BTStub.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
using BluetoothReal = BluetoothStub;
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
#endif
