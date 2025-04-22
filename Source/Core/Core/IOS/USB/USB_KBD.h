// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <queue>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Keyboard.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

namespace IOS::HLE
{
class USB_KBD : public EmulationDevice
{
public:
  USB_KBD(EmulationKernel& ios, const std::string& device_name);

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> Close(u32 fd) override;
  std::optional<IPCReply> Write(const ReadWriteRequest& request) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  void Update() override;

private:
  enum class MessageType : u32
  {
    KeyboardConnect = 0,
    KeyboardDisconnect = 1,
    Event = 2
  };

#pragma pack(push, 1)
  struct MessageData
  {
    MessageType msg_type{};
    u32 unk1 = 0;
    u8 modifiers = 0;
    u8 unk2 = 0;
    Common::HIDPressedKeys pressed_keys{};

    MessageData(MessageType msg_type, const Common::HIDPressedState& state);
  };
  static_assert(std::is_trivially_copyable_v<MessageData>,
                "MessageData must be trivially copyable, as it's copied into emulated memory.");
#pragma pack(pop)
  std::queue<MessageData> m_message_queue;
  Common::HIDPressedState m_previous_state;
  int m_keyboard_layout = Common::KBD_LAYOUT_QWERTY;
  std::shared_ptr<Common::KeyboardContext> m_keyboard_context;
};
}  // namespace IOS::HLE
