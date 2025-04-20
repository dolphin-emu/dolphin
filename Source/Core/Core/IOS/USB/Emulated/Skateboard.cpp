// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skateboard.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/InputConfig.h"

namespace IOS::HLE::USB
{

static InputConfig s_config{"Skateboard", _trans("Skateboard"), "Skateboard", "Skateboard"};

SkateboardController::SkateboardController()
{
  // Button layout on the skateboard:
  //
  // LEDs:                      (Up)
  // 1 2 3 4 (-) (Power) (Left)      (Right)  (A) (B) (1) (2)  [distance sensor]   (big '+' button)
  //                           (Down)
  //
  groups.emplace_back(m_buttons[1] = new ControllerEmu::Buttons(_trans("A, B, 1, 2")));
  groups.emplace_back(m_buttons[0] = new ControllerEmu::Buttons(_trans("+/-, Power")));
  groups.emplace_back(m_buttons[2] = new ControllerEmu::Buttons(_trans("D-Pad")));

  m_buttons[0]->AddInput(ControllerEmu::Translatability::DoNotTranslate, "A");
  m_buttons[0]->AddInput(ControllerEmu::Translatability::DoNotTranslate, "B");
  m_buttons[0]->AddInput(ControllerEmu::Translatability::DoNotTranslate, "1");
  m_buttons[0]->AddInput(ControllerEmu::Translatability::DoNotTranslate, "2");

  m_buttons[1]->AddInput(ControllerEmu::Translatability::Translate, _trans("+ (big button)"));
  m_buttons[1]->AddInput(ControllerEmu::Translatability::DoNotTranslate, "-");
  m_buttons[1]->AddInput(ControllerEmu::Translatability::Translate, _trans("Power"));

  m_buttons[2]->AddInput(ControllerEmu::Translatability::Translate, _trans("Up"));
  m_buttons[2]->AddInput(ControllerEmu::Translatability::Translate, _trans("Down"));
  m_buttons[2]->AddInput(ControllerEmu::Translatability::Translate, _trans("Left"));
  m_buttons[2]->AddInput(ControllerEmu::Translatability::Translate, _trans("Right"));

  const auto nose = _trans("Nose Accelerometer");
  const auto tail = _trans("Tail Accelerometer");
  groups.emplace_back(m_accel[0] = new ControllerEmu::IMUAccelerometer(nose, nose));
  groups.emplace_back(m_accel[1] = new ControllerEmu::IMUAccelerometer(tail, tail));

  groups.emplace_back(m_slider[0] = new ControllerEmu::Slider(_trans("Nose IR sensor")));
  groups.emplace_back(m_slider[1] = new ControllerEmu::Slider(_trans("Tail IR sensor")));
  groups.emplace_back(m_slider[2] = new ControllerEmu::Slider(_trans("Left IR sensor")));
  groups.emplace_back(m_slider[3] = new ControllerEmu::Slider(_trans("Right IR sensor")));
}

std::string SkateboardController::GetName() const
{
  return _trans("Skateboard");
}

InputConfig* SkateboardController::GetConfig() const
{
  return &s_config;
}

SkateboardHidReport SkateboardController::GetState() const
{
  SkateboardHidReport report;
  const u8 byte0[] = {
      0x02,  // A
      0x04,  // B
      0x01,  // 1
      0x08,  // 2
  };
  const u8 byte1[] = {
      0x02,  // +
      0x01,  // -
      0x10,  // Power
  };
  const u8 byte2[] = {
      0x0F,  // Up
      0x0B,  // Down
      0x09,  // Left
      0x0D,  // Right
  };
  m_buttons[0]->GetState(&report.m_buttons[0], byte0);
  m_buttons[1]->GetState(&report.m_buttons[1], byte1);
  m_buttons[2]->GetState(&report.m_buttons[2], byte2);
  // TODO: D-pad buttons don't map 1:1 to inverted bits
  report.m_buttons[2] ^= 0b1111;
  constexpr u8 MAX_IR_VALUE = 0x38;
  for (int i = 0; i < 4; ++i)
    report.m_lidar[i] = static_cast<u8>(MAX_IR_VALUE * m_slider[i]->GetState().value);
  // With the buttons on the left of the board:
  // accelerometer 0 is in the nose
  // accelerometer 1 is in the tail
  // x = forward/backward
  // y = left/right
  // z = up/down
  if (auto state0 = m_accel[0]->GetState())
  {
    report.m_accel0_x = 1023 * state0->z;
    report.m_accel0_y = 1023 * state0->y;
    report.m_accel0_z = 255 * state0->x;
  }
  if (auto state1 = m_accel[1]->GetState())
  {
    report.m_accel1_x = 1023 * state1->z;
    report.m_accel1_y = 1023 * state1->y;
    report.m_accel1_z = 256 * state1->x;
  }
  report.Encrypt();
  return report;
}

DeviceDescriptor SkateboardUSB::s_device_descriptor{
    .bLength = 18,
    .bDescriptorType = 1,
    .bcdUSB = 0x0100,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x1430,
    .idProduct = 0x0100,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

ConfigDescriptor SkateboardUSB::s_config_descriptor{
    .bLength = 9,
    .bDescriptorType = 2,
    .wTotalLength = 41,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .MaxPower = 250,
};

InterfaceDescriptor SkateboardUSB::s_interface_descriptor{
    .bLength = 9,
    .bDescriptorType = 4,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = 3,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
};

std::vector<EndpointDescriptor> SkateboardUSB::s_endpoint_descriptors{
    {
        .bLength = 7,
        .bDescriptorType = 5,
        .bEndpointAddress = 0x81,
        .bmAttributes = 3,
        .wMaxPacketSize = 64,
        .bInterval = 10,
    },
    {
        .bLength = 7,
        .bDescriptorType = 5,
        .bEndpointAddress = 0x02,
        .bmAttributes = 3,
        .wMaxPacketSize = 64,
        .bInterval = 10,
    },
};

// clang-format off
const u32 SkateboardHidReport::sbox[2][16] = {
    {
        0xB9A36, 0x3AB63, 0x59503, 0xE45DD, 0xDAAC5, 0x845AC, 0x24EA2, 0x3EED2,
        0xAF432, 0x7EAB3, 0x423CC, 0x2457D, 0x6BEEF, 0xD789E, 0xE8932, 0x1A8EF,
    },
    {
        0x3E532, 0xA445E, 0x145AA, 0xC729B, 0x4B67C, 0x892AE, 0x7AEEB, 0x5ACB8,
        0xA42EB, 0x9AA34, 0xB781A, 0x2EC87, 0xE425C, 0xD8A11, 0x4AED7, 0x9F49A,
    },
};

std::pair<u32, u32> SkateboardHidReport::ReorderNibbles(u32 a, u32 b)
{
  u32 c4 = (a <<  0) & 0xF0000;
  u32 c3 = (b << 12) & 0x0F000;
  u32 c2 = (b <<  4) & 0x00F00;
  u32 c1 = (a <<  4) & 0x000F0;
  u32 c0 = (a >>  4) & 0x0000F;
  u32 d4 = (b <<  8) & 0xF0000;
  u32 d3 = (b <<  0) & 0x0F000;
  u32 d2 = (b >>  8) & 0x00F00;
  u32 d1 = (a >>  4) & 0x000F0;
  u32 d0 = (a >> 12) & 0x0000F;
  return {c4 | c3 | c2 | c1 | c0, d4 | d3 | d2 | d1 | d0};
}
// clang-format on

void SkateboardHidReport::Encrypt()
{
  u32 c = ((m_accel0_x & 0x1F) << 15) | ((m_accel0_y & 0x3FF) << 5) | ((m_accel0_x & 0x3E0) >> 5);
  u32 d = ((m_accel1_y & 0x7F) << 13) | ((m_accel1_x & 0x3FF) << 3) | ((m_accel1_y & 0x380) >> 7);
  auto [a, b] = ReorderNibbles(c, d);
  a ^= sbox[0][m_lidar[2] & 15];
  b ^= sbox[1][m_lidar[3] & 15];
  m_accel0_x = a & 0x3FF;
  m_accel0_y = (a >> 10) & 0x3FF;
  m_accel1_x = (b >> 10) & 0x3FF;
  m_accel1_y = b & 0x3FF;
}

void SkateboardHidReport::Decrypt()
{
  // See 0x801E6770 (Ride) or 0x801C1FA0 (Shred).
  u32 a = u32(m_accel0_y & 0x3FF) << 10 | (m_accel0_x & 0x3FF);
  u32 b = u32(m_accel1_x & 0x3FF) << 10 | (m_accel1_y & 0x3FF);
  a ^= sbox[0][m_lidar[2] & 15];
  b ^= sbox[1][m_lidar[3] & 15];
  const auto [c, d] = ReorderNibbles(a, b);
  m_accel0_x = ((c << 5) & 0x3E0) | ((c >> 15) & 0x1F);
  m_accel0_y = (c >> 5) & 0x3FF;
  m_accel1_x = (d >> 3) & 0x3FF;
  m_accel1_y = ((d << 7) & 0x380) | ((d >> 13) & 0x7F);
}

void SkateboardHidReport::Dump(const char* prefix) const
{
#if 0
  auto sext10 = [](u16 v){ return (s32)v << 22 >> 22; };
  INFO_LOG_FMT(IOS_USB, "{}: {:5} {:5} {:4}  {:5} {:5} {:4}",
    prefix,
    sext10(m_accel0_x), sext10(m_accel0_y), m_accel0_z,
    sext10(m_accel1_x), sext10(m_accel1_y), m_accel1_z);
#else
  INFO_LOG_FMT(IOS_USB, "{}: {}", prefix, HexDump(reinterpret_cast<const u8*>(&m_lidar), 4));
#endif
}

DeviceDescriptor SkateboardUSB::GetDeviceDescriptor() const
{
  return s_device_descriptor;
}

std::vector<ConfigDescriptor> SkateboardUSB::GetConfigurations() const
{
  return {s_config_descriptor};
}

std::vector<InterfaceDescriptor> SkateboardUSB::GetInterfaces(u8 config) const
{
  return {s_interface_descriptor};
}

std::vector<EndpointDescriptor> SkateboardUSB::GetEndpoints(u8 config, u8, u8 alt) const
{
  return s_endpoint_descriptors;
}

bool SkateboardUSB::Attach()
{
  // This function gets called over and over,
  // so don't print anything here.
  return true;
}

bool SkateboardUSB::AttachAndChangeInterface(u8 interface)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME: Skateboard AttachAndChangeInterface");
  return true;
}

int SkateboardUSB::CancelTransfer(u8 endpoint)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME: Skateboard CancelTransfer");
  return 0;
}

int SkateboardUSB::ChangeInterface(const u8 interface)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME: Skateboard ChangeInterface");
  return 0;
}

int SkateboardUSB::GetNumberOfAltSettings(u8 interface)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME: Skateboard GetNumberOfAltSettings");
  return 0;
}

int SkateboardUSB::SetAltSetting(u8 alt_setting)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME: Skateboard SetAltSetting");
  return 0;
}

int SkateboardUSB::SubmitTransfer(std::unique_ptr<CtrlMessage> message)
{
  static constexpr u8 GET_DESCRIPTOR = 0x06;
  static constexpr u8 HID_SET_REPORT = 0x09;
  static constexpr u8 HID_SET_PROTOCOL = 0x0B;

  if (message->request_type == 0x21 && message->request == HID_SET_REPORT)
  {
    // The game sends these once every 1.25 seconds.
  }
  else if (message->request_type == 0x80 && message->request == GET_DESCRIPTOR &&
           (message->value == 0x301 || message->value == 0x302))
  {
    // TODO: string descriptors
    // 1: "Licensed by Nintendo of America"
    // 2: "Skateboard Controller"
  }
  else if (message->request_type == 0x21 && message->request == HID_SET_PROTOCOL)
  {
    // TODO: switch between boot and report protocol (not sure what that means)
  }
  else
  {
    auto buf = message->MakeBuffer(message->length);
    ERROR_LOG_FMT(IOS_USB,
                  "FIXME: CtrlMessage bRequestType={:02x} bRequest={:02x} wValue={:04x} "
                  "wIndex={:04x} wLength={:04x}",
                  message->request_type, message->request, message->value, message->index,
                  message->length);
  }
  message->ScheduleTransferCompletion(message->length, 0);
  return 0;
}

int SkateboardUSB::SubmitTransfer(std::unique_ptr<IntrMessage> message)
{
  if (message->endpoint != 0x81)
    ERROR_LOG_FMT(IOS_USB, "Skateboard: unexpected IntrMessage");

  const SkateboardHidReport report = m_inputs.GetState();
  message->FillBuffer(reinterpret_cast<const u8*>(&report), sizeof(report));
  message->ScheduleTransferCompletion(sizeof(report), 1000);
  return 0;
}

int SkateboardUSB::SubmitTransfer(std::unique_ptr<BulkMessage> message)
{
  ERROR_LOG_FMT(IOS_USB, "Skateboard: unexpected BulkMessage");
  return 0;
}

int SkateboardUSB::SubmitTransfer(std::unique_ptr<IsoMessage> message)
{
  ERROR_LOG_FMT(IOS_USB, "Skateboard: unexpected IsoMessage");
  return 0;
}

}  // namespace IOS::HLE::USB
