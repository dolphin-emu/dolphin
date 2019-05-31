// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <queue>
#include <string>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

namespace IOS::HLE::Device
{
class USB_KBD : public Device
{
public:
  USB_KBD(Kernel& ios, const std::string& device_name);

  IPCCommandResult Open(const OpenRequest& request) override;
  IPCCommandResult Write(const ReadWriteRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  void Update() override;

private:
  enum
  {
    MSG_KBD_CONNECT = 0,
    MSG_KBD_DISCONNECT = 1,
    MSG_EVENT = 2
  };

  using PressedKeyData = std::array<u8, 6>;

#pragma pack(push, 1)
  struct MessageData
  {
    u32 MsgType = 0;
    u32 Unk1 = 0;
    u8 Modifiers = 0;
    u8 Unk2 = 0;
    PressedKeyData PressedKeys{};

    MessageData(u32 msg_type, u8 modifiers, PressedKeyData pressed_keys);
  };
  static_assert(std::is_trivially_copyable_v<MessageData>,
                "MessageData must be trivially copyable, as it's copied into emulated memory.");
#pragma pack(pop)
  std::queue<MessageData> m_MessageQueue;

  std::array<bool, 256> m_OldKeyBuffer{};
  u8 m_OldModifiers = 0;

  bool IsKeyPressed(int key) const;

  // This stuff should probably die
  enum
  {
    KBD_LAYOUT_QWERTY = 0,
    KBD_LAYOUT_AZERTY = 1
  };
  int m_KeyboardLayout = KBD_LAYOUT_QWERTY;
};
}  // namespace IOS::HLE::Device
