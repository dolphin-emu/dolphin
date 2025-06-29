// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__LIBUSB__)
#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/IOS/USB/Bluetooth/LibUSBBluetoothAdapter.h"
#include "Core/IOS/USB/Bluetooth/hci.h"
#include "Core/IOS/USB/USBV0.h"

class PointerWrap;

namespace IOS::HLE
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

class BluetoothRealDevice final : public BluetoothBaseDevice
{
public:
  BluetoothRealDevice(EmulationKernel& ios, const std::string& device_name);
  ~BluetoothRealDevice() override;

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> Close(u32 fd) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;
  void Update() override;

  void DoState(PointerWrap& p) override;
  void UpdateSyncButtonState(bool is_held) override;
  void TriggerSyncButtonPressedEvent() override;
  void TriggerSyncButtonHeldEvent() override;

private:
  using BufferType = LibUSBBluetoothAdapter::BufferType;

  std::unique_ptr<LibUSBBluetoothAdapter> m_lib_usb_bt_adapter;

  static constexpr u32 SYNC_BUTTON_HOLD_MS_TO_RESET = 10000;

  std::atomic<SyncButtonState> m_sync_button_state{SyncButtonState::Unpressed};
  Common::Timer m_sync_button_held_timer;

  // Enqueued when we observe a command to which we need to fake a reply.
  std::queue<std::function<void(USB::V0IntrMessage&)>> m_fake_replies;

  // This stores the address of paired devices and associated link keys.
  // It is needed because some adapters forget all stored link keys when they are reset,
  // which breaks pairings because the Wii relies on the Bluetooth module to remember them.
  std::map<bdaddr_t, linkkey_t> m_link_keys;

  // Endpoints provided by the emulated software.
  std::unique_ptr<USB::V0IntrMessage> m_hci_endpoint;
  std::unique_ptr<USB::V0BulkMessage> m_acl_endpoint;

  // Used for proper Bluetooth packet timing, especially for Wii remote speaker data.
  TimePoint GetTargetTime() const;

  void TryToFillHCIEndpoint();
  void TryToFillACLEndpoint();

  [[nodiscard]] BufferType ProcessHCIEvent(BufferType buffer);

  void SendHCIResetCommand();
  void SendHCIDeleteLinkKeyCommand();
  bool SendHCIStoreLinkKeyCommand();

  void FakeVendorCommandReply(u16 opcode, USB::V0IntrMessage& ctrl);

  void FakeSyncButtonEvent(USB::V0IntrMessage& ctrl, std::span<const u8> payload);
  void FakeSyncButtonPressedEvent(USB::V0IntrMessage& ctrl);
  void FakeSyncButtonHeldEvent(USB::V0IntrMessage& ctrl);

  void LoadLinkKeys();
  void SaveLinkKeys();
};
}  // namespace IOS::HLE

#else
#include "Core/IOS/USB/Bluetooth/BTStub.h"

namespace IOS::HLE
{
using BluetoothRealDevice = BluetoothStubDevice;
}  // namespace IOS::HLE
#endif
