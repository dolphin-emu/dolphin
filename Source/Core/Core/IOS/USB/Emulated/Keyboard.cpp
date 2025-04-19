// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Keyboard.h"

#include <algorithm>
#include <ranges>

#include "Common/Assert.h"
#include "Common/Projection.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/Memmap.h"
#include "InputCommon/ControlReference/ControlReference.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace IOS::HLE::USB
{
namespace
{
#ifdef _WIN32
// Map Windows virtual key to HID key code
//
// NB:
//  - It seems Windows API is able to convert PS/2 scan code <-> virtual key
//  - I couldn't find something equivalent for HID usage id
//  - The following posts might be worth mentioning:
// https://stackoverflow.com/a/69600455
// https://stackoverflow.com/a/72869289
using VkHidPair = std::pair<int, u8>;
std::vector<VkHidPair> GetVkHidMap()
{
  // These constants are added for clarity
  static constexpr int VK_UNKNOWN = 0;
  static constexpr u16 HID_UNKNOWN = 0;
  static constexpr u16 HID_MODIFIER = 0;
  static constexpr auto DEPRECATED = [](auto) -> u8 { return 0; };

  // References:
  //  - Keyboard scan codes:
  // https://learn.microsoft.com/windows/win32/inputdev/about-keyboard-input
  //  - Windows virtual key codes:
  // https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes
  // https://learn.microsoft.com/globalization/windows-keyboard-layouts
  //  - HID Usage Tables, Keyboard/Keypad page (0x07):
  //  https://usb.org/sites/default/files/hut1_21.pdf
  std::vector<VkHidPair> map;

  // Windows virtual keys 0x01 to 0x07
  for (const int& virtual_key :
       {VK_LBUTTON, VK_RBUTTON, VK_CANCEL, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2, 0x07})
  {
    map.emplace_back(virtual_key, HID_UNKNOWN);
  }

  // Windows virtual keys 0x08 and 0x09
  map.emplace_back(VK_BACK, 0x2A);  // Backspace key
  map.emplace_back(VK_TAB, 0x2B);

  // Windows (reserved) virtual keys 0x0A and 0x0B
  for (const int& virtual_key : {0x0A, 0x0B})
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0x0C and 0x0D
  map.emplace_back(VK_CLEAR, 0x9C);
  map.emplace_back(VK_RETURN, 0x28);  // Enter key

  // Windows (unassigned) virtual keys 0x0E and 0x0F
  for (const int& virtual_key : {0x0E, 0x0F})
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0x10 and 0x14
  map.emplace_back(VK_SHIFT, HID_MODIFIER);
  map.emplace_back(VK_CONTROL, HID_MODIFIER);
  map.emplace_back(VK_MENU, HID_MODIFIER);  // Alt key
  map.emplace_back(VK_PAUSE, 0x48);
  map.emplace_back(VK_CAPITAL, 0x39);

  // Windows virtual keys 0x15 to 0x1A
  // TODO: Handle regional keys (kana, kanji, ...)
  for (const int& virtual_key : {0x15, VK_IME_ON, VK_JUNJA, VK_FINAL, 0x19, VK_IME_OFF})
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual key 0x1B
  map.emplace_back(VK_ESCAPE, 0x29);

  // Windows virtual keys 0x1C to 0x1F
  // TODO: Handle regional keys (kana, kanji, ...)
  for (const int& virtual_key : {VK_CONVERT, VK_NONCONVERT, VK_ACCEPT, VK_MODECHANGE})
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0x20 to 0x28
  map.emplace_back(VK_SPACE, 0x2C);
  map.emplace_back(VK_PRIOR, 0x4B);  // Page up key
  map.emplace_back(VK_NEXT, 0x4E);   // Page down key
  map.emplace_back(VK_END, 0x4D);
  map.emplace_back(VK_HOME, 0x4A);
  map.emplace_back(VK_LEFT, 0x50);
  map.emplace_back(VK_UP, 0x52);
  map.emplace_back(VK_RIGHT, 0x4F);
  map.emplace_back(VK_DOWN, 0x51);

  // Windows virtual keys 0x29 to 0x2B
  // NB: Modern keyboards don't have these keys so let's silent them
  map.emplace_back(VK_SELECT, DEPRECATED(0x77));
  map.emplace_back(VK_PRINT, DEPRECATED(HID_UNKNOWN));
  map.emplace_back(VK_EXECUTE, DEPRECATED(0x74));

  // Windows virtual keys 0x2C to 0x2F
  map.emplace_back(VK_SNAPSHOT, 0x46);
  map.emplace_back(VK_INSERT, 0x49);
  map.emplace_back(VK_DELETE, 0x4C);
  map.emplace_back(VK_HELP, 0x75);

  // Windows virtual keys `0` (0x30) to `9` (0x39)
  map.emplace_back('0', 0x27);
  map.emplace_back('1', 0x1E);
  map.emplace_back('2', 0x1F);
  map.emplace_back('3', 0x20);
  map.emplace_back('4', 0x21);
  map.emplace_back('5', 0x22);
  map.emplace_back('6', 0x23);
  map.emplace_back('7', 0x24);
  map.emplace_back('8', 0x25);
  map.emplace_back('9', 0x26);

  // Windows (undefined) virtual keys 0x3A to 0x40
  for (int virtual_key = 0x3A; virtual_key <= 0x40; ++virtual_key)
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys `A` (0x41) to `Z` (0x5A)
  for (int i = 0; i < 26; ++i)
    map.emplace_back('A' + i, 0x04 + i);

  // Windows virtual keys 0x5B to 0x5F
  map.emplace_back(VK_LWIN, HID_MODIFIER);
  map.emplace_back(VK_RWIN, HID_MODIFIER);
  map.emplace_back(VK_APPS, 0x65);
  map.emplace_back(0x5E, HID_UNKNOWN);  // Reserved virtual key
  map.emplace_back(VK_SLEEP, HID_UNKNOWN);

  // Windows virtual keys 0x60 to 0x69
  map.emplace_back(VK_NUMPAD0, 0x62);
  map.emplace_back(VK_NUMPAD1, 0x59);
  map.emplace_back(VK_NUMPAD2, 0x5A);
  map.emplace_back(VK_NUMPAD3, 0x5B);
  map.emplace_back(VK_NUMPAD4, 0x5C);
  map.emplace_back(VK_NUMPAD5, 0x5D);
  map.emplace_back(VK_NUMPAD6, 0x5E);
  map.emplace_back(VK_NUMPAD7, 0x5F);
  map.emplace_back(VK_NUMPAD8, 0x60);
  map.emplace_back(VK_NUMPAD9, 0x61);

  // Windows virtual keys 0x6A to 0x6F
  map.emplace_back(VK_MULTIPLY, 0x55);
  map.emplace_back(VK_ADD, 0x57);
  map.emplace_back(VK_SEPARATOR, 0x85);
  map.emplace_back(VK_SUBTRACT, 0x56);
  map.emplace_back(VK_DECIMAL, 0x63);
  map.emplace_back(VK_DIVIDE, 0x54);

  // Windows virtual keys 0x70 to 0x7B
  for (int i = 0; i < 12; ++i)
    map.emplace_back(VK_F1 + i, 0x3A + i);

  // Windows virtual keys 0x7C to 0x87
  // NB: Modern keyboards don't have these keys so let's silent them
  for (int i = 0; i < 12; ++i)
    map.emplace_back(VK_F13 + i, DEPRECATED(0x68 + i));

  // Windows (reserved) virtual keys 0x88 to 0x8F
  for (int virtual_key = 0x88; virtual_key <= 0x8F; ++virtual_key)
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0x90 and 0x91
  map.emplace_back(VK_NUMLOCK, 0x53);
  map.emplace_back(VK_SCROLL, 0x47);

  // Windows (OEM specific) virtual keys 0x92 to 0x96
  for (int virtual_key = 0x92; virtual_key <= 0x96; ++virtual_key)
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows (unassigned) virtual keys 0x97 to 0x9F
  for (int virtual_key = 0x97; virtual_key <= 0x9F; ++virtual_key)
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0xA0 to 0xA5
  for (const int& virtual_key :
       {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU})
  {
    map.emplace_back(virtual_key, HID_MODIFIER);
  }

  // Windows virtual keys 0xA6 to 0xAC
  for (const int& virtual_key :
       {VK_BROWSER_BACK, VK_BROWSER_FORWARD, VK_BROWSER_REFRESH, VK_BROWSER_STOP, VK_BROWSER_SEARCH,
        VK_BROWSER_FAVORITES, VK_BROWSER_HOME})
  {
    map.emplace_back(virtual_key, HID_UNKNOWN);
  }

  // Windows virtual keys 0xAD to 0xAF
  for (const int& virtual_key : {VK_VOLUME_MUTE, VK_VOLUME_DOWN, VK_VOLUME_UP})
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0xB0 to 0xB3
  for (const int& virtual_key :
       {VK_MEDIA_NEXT_TRACK, VK_MEDIA_PREV_TRACK, VK_MEDIA_STOP, VK_MEDIA_PLAY_PAUSE})
  {
    map.emplace_back(virtual_key, HID_UNKNOWN);
  }

  // Windows virtual keys 0xB4 to 0xB7
  for (const int& virtual_key :
       {VK_LAUNCH_MAIL, VK_LAUNCH_MEDIA_SELECT, VK_LAUNCH_APP1, VK_LAUNCH_APP2})
  {
    map.emplace_back(virtual_key, HID_UNKNOWN);
  }

  // Windows (reserved) virtual keys 0xB8 and 0xB9
  for (const int& virtual_key : {0xB8, 0xB9})
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // NB: The ones below are used for miscellaneous characters and can vary by keyboard.
  // TODO: Implement regional variants (e.g. Europe and Japan)

  // Windows virtual keys 0xBA to 0xC0
  map.emplace_back(VK_OEM_1, 0x33);  // US: ;: key
  map.emplace_back(VK_OEM_PLUS, 0x2E);
  map.emplace_back(VK_OEM_COMMA, 0x36);
  map.emplace_back(VK_OEM_MINUS, 0x2D);
  map.emplace_back(VK_OEM_PERIOD, 0x37);
  map.emplace_back(VK_OEM_2, 0x38);  // US: /? key
  map.emplace_back(VK_OEM_3, 0x35);  // US: `~ key

  // Windows (reserved) virtual keys 0xC1 to 0xDA
  for (int virtual_key = 0xC1; virtual_key <= 0xDA; ++virtual_key)
    map.emplace_back(virtual_key, HID_UNKNOWN);

  // Windows virtual keys 0xDB to 0xE2
  map.emplace_back(VK_OEM_4, 0x2F);  // US: [{ key
  map.emplace_back(VK_OEM_5, 0x32);  // US: \| key
  map.emplace_back(VK_OEM_6, 0x30);  // US: ]} key
  map.emplace_back(VK_OEM_7, 0x34);  // US: '" key
  map.emplace_back(VK_OEM_8, HID_UNKNOWN);
  map.emplace_back(0xE0, HID_UNKNOWN);  // Reserved virtual key
  map.emplace_back(0xE1, HID_UNKNOWN);  // OEM specific virtual key
  map.emplace_back(VK_OEM_102, 0x31);   // US: \| key

  // Nothing interesting past this point.
  ASSERT_MSG(IOS_USB, map[VK_OEM_102].first != VK_OEM_102,
             "Failed to generate emulated keyboard layout");

#ifdef HLE_KEYBOARD_DEBUG
  // Might be worth adding for debugging purpose
  for (u16 hid_usage_id = 1; hid_usage_id < 0xE8; ++hid_usage_id)
  {
    if (std::ranges::find(map, hid_usage_id, Common::Projection::Second{}) == map.end())
    {
      map.emplace_back(VK_UNKNOWN, hid_usage_id);
    }
  }
#endif

  return map;
}

std::vector<VkHidPair> CleanVkHidMap(const std::vector<VkHidPair>& map)
{
  // Ignore unsupported/ignored keys
  std::vector<VkHidPair> cleaned_map;
  for (const auto& [virtual_key, hid_usage_id] : map)
  {
    if (virtual_key != 0 && hid_usage_id != 0)
    {
      cleaned_map.emplace_back(virtual_key, hid_usage_id);
    }
#ifdef HLE_KEYBOARD_DEBUG
    else if (virtual_key == 0)
    {
      // Missing virtual key for HID usage ID
      static std::vector<u8> s_missing_vk_for_hid;
      s_missing_vk_for_hid.emplace_back(hid_usage_id);
    }
    else if (hid_usage_id == 0)
    {
      // Missing HID usage ID for virtual key
      static std::vector<int> s_missing_hid_for_vk;
      s_missing_hid_for_vk.emplace_back(virtual_key);
    }
#endif
  }
  return cleaned_map;
}

static const std::vector<VkHidPair> VK_HID_MAP = CleanVkHidMap(GetVkHidMap());
#endif
}  // namespace

Keyboard::Keyboard(IOS::HLE::EmulationKernel& ios) : m_ios(ios)
{
  m_id = u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1);
}

Keyboard::~Keyboard() = default;

DeviceDescriptor Keyboard::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> Keyboard::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> Keyboard::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> Keyboard::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return m_endpoint_descriptor;
}

bool Keyboard::Attach()
{
  if (m_device_attached)
    return true;

  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening emulated keyboard", m_vid, m_pid);
  m_device_attached = true;
  return true;
}

bool Keyboard::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int Keyboard::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int Keyboard::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int Keyboard::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int Keyboard::SetAltSetting(u8 alt_setting)
{
  return 0;
}

int Keyboard::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  switch (cmd->request_type << 8 | cmd->request)
  {
  case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_INTERFACE, REQUEST_GET_INTERFACE):
  {
    const u8 data{1};
    cmd->FillBuffer(&data, sizeof(data));
    cmd->ScheduleTransferCompletion(1, 100);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_STANDARD, REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] REQUEST_SET_INTERFACE index={:04x} value={:04x}",
                 m_vid, m_pid, m_active_interface, cmd->index, cmd->value);
    if (static_cast<u8>(cmd->index) != m_active_interface)
    {
      const int ret = ChangeInterface(static_cast<u8>(cmd->index));
      if (ret < 0)
      {
        ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Failed to change interface to {}", m_vid, m_pid,
                      m_active_interface, cmd->index);
        return ret;
      }
    }
    const int ret = SetAltSetting(static_cast<u8>(cmd->value));
    if (ret == 0)
      m_ios.EnqueueIPCReply(cmd->ios_request, cmd->length);
    return ret;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, HIDRequestCodes::SET_REPORT):
  {
    // According to the HID specification:
    //  - A device might choose to ignore input Set_Report requests as meaningless.
    //  - Alternatively these reports could be used to reset the origin of a control
    // (that is, current position should report zero).
    //  - The effect of sent reports will also depend on whether the recipient controls
    // are absolute or relative.
    const u8 report_type = cmd->value >> 8;
    const u8 report_id = cmd->value & 0xFF;
    auto& system = m_ios.GetSystem();
    auto& memory = system.GetMemory();

    // The data seems to report LED status for keys such as:
    //  - NUM LOCK, CAPS LOCK
    u8* data = memory.GetPointerForRange(cmd->data_address, cmd->length);
    INFO_LOG_FMT(IOS_USB, "SET_REPORT ignored (report_type={:02x}, report_id={:02x}, index={})\n{}",
                 report_type, report_id, cmd->index, HexDump(data, cmd->length));
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, HIDRequestCodes::SET_IDLE):
  {
    WARN_LOG_FMT(IOS_USB, "SET_IDLE not implemented (value={:04x}, index={})", cmd->value,
                 cmd->index);
    // TODO: Handle idle duration and implement NAK
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, HIDRequestCodes::SET_PROTOCOL):
  {
    INFO_LOG_FMT(IOS_USB, "SET_PROTOCOL: value={}, index={}", cmd->value, cmd->index);
    const HIDProtocol protocol = static_cast<HIDProtocol>(cmd->value);
    switch (protocol)
    {
    case HIDProtocol::Boot:
    case HIDProtocol::Report:
      m_current_protocol = protocol;
      break;
    default:
      WARN_LOG_FMT(IOS_USB, "SET_PROTOCOL: Unknown protocol {} for interface {}", cmd->value,
                   cmd->index);
    }
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  default:
    WARN_LOG_FMT(IOS_USB, "Unknown command, req={:02x}, type={:02x}", cmd->request,
                 cmd->request_type);
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  }

  return IPC_SUCCESS;
}

int Keyboard::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={:04x} endpoint={:02x}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
}

int Keyboard::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  static auto start_time = std::chrono::steady_clock::now();
  const auto current_time = std::chrono::steady_clock::now();
  const bool should_poll = (current_time - start_time) >= std::chrono::milliseconds(1);

  const HIDKeyboardReport report = should_poll ? PollInputs() : m_last_report;
  cmd->FillBuffer(reinterpret_cast<const u8*>(&report), sizeof(report));
  m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);

  if (should_poll)
  {
    m_last_report = std::move(report);
    start_time = std::chrono::steady_clock::now();
  }
  return IPC_SUCCESS;
}

int Keyboard::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Isochronous: length={:04x} endpoint={:02x} num_packets={:02x}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, 2000);
  return IPC_SUCCESS;
}

void Keyboard::ScheduleTransfer(std::unique_ptr<TransferCommand> command, std::span<const u8> data,
                                u64 expected_time_us)
{
  command->FillBuffer(data.data(), data.size());
  command->ScheduleTransferCompletion(static_cast<s32>(data.size()), expected_time_us);
}

bool Keyboard::IsKeyPressed(int key) const
{
#ifdef _WIN32
  return (GetAsyncKeyState(key) & 0x8000) != 0;
#else
  // TODO: do it for non-Windows platforms
  return false;
#endif
}

HIDPressedKeys Keyboard::PollHIDPressedKeys()
{
  HIDPressedKeys pressed_keys{};
  std::size_t pressed_keys_count = 0;

#ifdef _WIN32
  for (const auto& [virtual_key, hid_usage_id] : VK_HID_MAP)
  {
    if (!IsKeyPressed(virtual_key))
      continue;

    pressed_keys[pressed_keys_count++] = hid_usage_id;
    if (pressed_keys_count == pressed_keys.size())
      break;
  }
#else
  // TODO: Implementation for non-Windows platforms
#endif
  return pressed_keys;
}

u8 Keyboard::PollHIDModifiers()
{
  u8 modifiers = 0;
#ifdef _WIN32
  // References:
  // https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes
  // https://www.usb.org/document-library/device-class-definition-hid-111
  static const std::vector<VkHidPair> MODIFIERS_MAP{
      {VK_LCONTROL, 0x01},  // HID modifier: Bit 0 - LEFT CTRL
      {VK_LSHIFT, 0x02},    // HID modifier: Bit 1 - LEFT SHIFT
      {VK_LMENU, 0x04},     // HID modifier: Bit 2 - LEFT ALT
      {VK_LWIN, 0x08},      // HID modifier: Bit 3 - LEFT GUI
      {VK_RCONTROL, 0x10},  // HID modifier: Bit 4 - RIGHT CTRL
      {VK_RSHIFT, 0x20},    // HID modifier: Bit 5 - RIGHT SHIFT
      {VK_RMENU, 0x40},     // HID modifier: Bit 6 - RIGHT ALT
      {VK_RWIN, 0x80}       // HID modifier: Bit 7 - RIGHT GUI
  };
  for (const auto& [virtual_key, hid_modifier] : MODIFIERS_MAP)
  {
    if (IsKeyPressed(virtual_key))
      modifiers |= hid_modifier;
  }
#else
  // TODO: Implementation for non-Windows platforms
#endif
  return modifiers;
}

// WARNING: Current implementation kills performance
HIDKeyboardReport Keyboard::PollInputs()
{
  if (!Config::Get(Config::MAIN_WII_KEYBOARD) ||
      // Check if input should be captured
      !ControlReference::GetInputGate())
  {
    return {};
  }

  const HIDKeyboardReport result{
      .modifiers = PollHIDModifiers(), .oem = 0, .pressed_keys = PollHIDPressedKeys()};

  return result;
}
}  // namespace IOS::HLE::USB
