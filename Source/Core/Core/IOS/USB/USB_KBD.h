// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"

namespace IOS
{
namespace HLE
{
class CWII_IPC_HLE_Device_usb_kbd : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_kbd(u32 _DeviceID, const std::string& _rDeviceName);

  IOSReturnCode Open(const IOSOpenRequest& request) override;
  IPCCommandResult IOCtl(const IOSIOCtlRequest& request) override;
  void Update() override;

private:
  enum
  {
    MSG_KBD_CONNECT = 0,
    MSG_KBD_DISCONNECT = 1,
    MSG_EVENT = 2
  };

#pragma pack(push, 1)
  struct SMessageData
  {
    u32 MsgType;
    u32 Unk1;
    u8 Modifiers;
    u8 Unk2;
    u8 PressedKeys[6];

    SMessageData(u32 msg_type, u8 modifiers, u8* pressed_keys);
  };
#pragma pack(pop)
  std::queue<SMessageData> m_MessageQueue;

  bool m_OldKeyBuffer[256];
  u8 m_OldModifiers;

  virtual bool IsKeyPressed(int _Key);

  // This stuff should probably die
  enum
  {
    KBD_LAYOUT_QWERTY = 0,
    KBD_LAYOUT_AZERTY = 1
  };
  int m_KeyboardLayout;
  static u8 m_KeyCodesQWERTY[256];
  static u8 m_KeyCodesAZERTY[256];
};
}  // namespace HLE
}  // namespace IOS
