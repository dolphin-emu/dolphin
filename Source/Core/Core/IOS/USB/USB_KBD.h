// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <queue>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

namespace IOS::HLE
{
class USB_KBD : public EmulationDevice
{
public:
  USB_KBD(EmulationKernel& ios, const std::string& device_name);

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> Write(const ReadWriteRequest& request) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  void DoState(PointerWrap& p) override;
  void Update() override;

private:
  enum class MessageType : u32
  {
    KeyboardConnect = 0,
    KeyboardDisconnect = 1,
    Event = 2
  };

  using PressedKeyData = std::array<u8, 6>;

#pragma pack(push, 1)
  struct MessageData
  {
    MessageType msg_type{};
    u32 unk1 = 0;
    u8 modifiers = 0;
    u8 unk2 = 0;
    PressedKeyData pressed_keys{};

    MessageData(MessageType msg_type, u8 modifiers, PressedKeyData pressed_keys);
  };
  static_assert(std::is_trivially_copyable_v<MessageData>,
                "MessageData must be trivially copyable, as it's copied into emulated memory.");
#pragma pack(pop)
  std::queue<MessageData> m_message_queue;

  std::array<bool, 256> m_old_key_buffer{};
  u8 m_old_modifiers = 0;

  bool IsKeyPressed(int key) const;

  // This stuff should probably die
  enum
  {
    KBD_LAYOUT_QWERTY = 0,
    KBD_LAYOUT_AZERTY = 1
  };
  int m_keyboard_layout = KBD_LAYOUT_QWERTY;

  std::optional<u32> m_pending_request{};
};
}  // namespace IOS::HLE
