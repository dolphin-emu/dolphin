// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylander.h"

#include <map>
#include <mutex>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
SkylanderUSB::SkylanderUSB(Kernel& ios, const std::string& device_name) : m_ios(ios)
{
  m_vid = 0x1430;
  m_pid = 0x150;
  m_id = (static_cast<u64>(m_vid) << 32 | static_cast<u64>(m_pid) << 16 | static_cast<u64>(9) << 8 |
          static_cast<u64>(1));
  m_device_descriptor = DeviceDescriptor{0x12,   0x1,   0x200, 0x0, 0x0, 0x0, 0x40,
                                         0x1430, 0x150, 0x100, 0x1, 0x2, 0x0, 0x1};
  m_config_descriptor.emplace_back(ConfigDescriptor{0x9, 0x2, 0x29, 0x1, 0x1, 0x0, 0x80, 0xFA});
  m_interface_descriptor.emplace_back(
      InterfaceDescriptor{0x9, 0x4, 0x0, 0x0, 0x2, 0x3, 0x0, 0x0, 0x0});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x81, 0x3, 0x40, 0x1});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x2, 0x3, 0x40, 0x1});
}

SkylanderUSB::~SkylanderUSB() = default;

DeviceDescriptor SkylanderUSB::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> SkylanderUSB::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> SkylanderUSB::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> SkylanderUSB::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return m_endpoint_descriptor;
}

bool SkylanderUSB::Attach()
{
  if (m_device_attached)
  {
    return true;
  }
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  m_device_attached = true;
  return true;
}

bool SkylanderUSB::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int SkylanderUSB::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int SkylanderUSB::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int SkylanderUSB::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int SkylanderUSB::SetAltSetting(u8 alt_setting)
{
  return 0;
}

// Skylander Portal control transfers are handled via this method, if a transfer requires a
// response, then we request that data from the "Portal" and queue it to be responded to via the
// Interrupt Response, references can be found via:
// https://marijnkneppers.dev/posts
// https://github.com/tresni/PoweredPortals/wiki/USB-Protocols
// https://pastebin.com/EqtTRzeF
int SkylanderUSB::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  // If not HID Host to Device type, return invalid
  if (cmd->request_type != 0x21)
    return IPC_EINVAL;

  // Data to be sent back via the control transfer immediately
  std::array<u8, 64> control_response = {};
  s32 expected_count = 0;
  u64 expected_time_us = 100;

  // Non 0x09 Requests are handled here - no portal data is requested
  if (cmd->request != 0x09)
  {
    switch (cmd->request)
    {
    // Get Interface
    case 0x0A:
      expected_count = 8;
      break;
    // Set Interface
    case 0x0B:
      expected_count = 8;
      break;
    default:
      ERROR_LOG_FMT(IOS_USB, "Unhandled Request {}", cmd->request);
      break;
    }
  }
  else
  {
    // Skylander Portal Requests
    auto& system = Core::System::GetInstance();
    auto& memory = system.GetMemory();
    u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);
    if (cmd->length == 0 || buf == nullptr)
    {
      ERROR_LOG_FMT(IOS_USB, "Skylander command invalid");
      return IPC_EINVAL;
    }
    // Data to be queued to be sent back via the Interrupt Transfer (if needed)
    std::array<u8, 64> interrupt_response = {};

    // The first byte of the Control Request is always a char for Skylanders
    switch (buf[0])
    {
    case 'A':
    {
      // Activation
      // Command	{ 'A', (00 | 01), 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'A', (00 | 01),
      // ff, 77, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00 }
      // The 2nd byte of the command is whether to activate (0x01) or deactivate (0x00) the
      // portal. The response echos back the activation byte as the 2nd byte of the response. The
      // 3rd and 4th bytes of the response appear to vary from wired to wireless. On wired
      // portals, the bytes appear to always be ff 77. On wireless portals, during activation the
      // 3rd byte appears to count down from ff (possibly a battery power indication) and during
      // deactivation ed and eb responses have been observed. The 4th byte appears to always be 00
      // for wireless portals.

      // Wii U Wireless: 41 01 f4 00 41 00 ed 00 41 01 f4 00 41 00 eb 00 41 01 f3 00 41 00 ed 00
      if (cmd->length == 2)
      {
        control_response = {buf[0], buf[1]};
        interrupt_response = {0x41, buf[1], 0xFF, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        m_queries.push(interrupt_response);
        expected_count = 10;
        system.GetSkylanderPortal().Activate();
      }
      break;
    }
    case 'C':
    {
      // Color
      // Command	{ 'C', 12, 34, 56, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'C', 12, 34, 56, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // The 3 bytes {12, 34, 56} are RGB values.

      // This command should set the color of the LED in the portal, however this appears
      // deprecated in most of the recent portals. On portals that do not have LEDs, this command
      // is silently ignored and do not require a response.
      if (cmd->length == 4)
      {
        system.GetSkylanderPortal().SetLEDs(0x01, buf[1], buf[2], buf[3]);
        control_response = {0x43, buf[1], buf[2], buf[3]};
        expected_count = 12;
      }
      break;
    }
    case 'J':
    {
      // Sided color
      // The 2nd byte is the side
      // 0x00: right
      // 0x01: left and right
      // 0x02: left

      // The 3rd, 4th and 5th bytes are red, green and blue

      // The 6th byte is unknown. Observed values are 0x00, 0x0D and 0xF4

      // The 7th byte is the fade duration. Exact value-time corrolation unknown. Observed values
      // are 0x00, 0x01 and 0x07. Custom commands show that the higher this value the longer the
      // duration.

      // Empty J response is sent after the fade is completed.
      if (cmd->length == 7)
      {
        control_response = {buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]};
        expected_count = 15;
        interrupt_response = {buf[0]};
        m_queries.push(interrupt_response);
        system.GetSkylanderPortal().SetLEDs(buf[1], buf[2], buf[3], buf[4]);
      }
      break;
    }
    case 'L':
    {
      // Light
      // This command is used while playing audio through the portal

      // The 2nd bytes is the position
      // 0x00: right
      // 0x01: trap led
      // 0x02: left

      // The 3rd, 4th and 5th bytes are red, green and blue
      // the trap led is white-only
      // increasing or decreasing the values results in a brighter or dimmer light
      if (cmd->length == 5)
      {
        control_response = {buf[0], buf[1], buf[2], buf[3], buf[4]};
        expected_count = 13;

        u8 side = buf[1];
        if (side == 0x02)
        {
          side = 0x04;
        }
        system.GetSkylanderPortal().SetLEDs(side, buf[2], buf[3], buf[4]);
      }
      break;
    }
    case 'M':
    {
      // Audio Firmware version
      // Respond with version obtained from Trap Team wired portal
      if (cmd->length == 2)
      {
        control_response = {buf[0], buf[1]};
        expected_count = 10;
        interrupt_response = {buf[0], buf[1], 0x00, 0x19};
        m_queries.push(interrupt_response);
      }
      break;
    }
      // Query
      // Command	{ 'Q', 10, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'Q', 10, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // In the command the 2nd byte indicates which Skylander to query data
      // from. Index starts at 0x10 for the 1st Skylander (as reported in the Status command.) The
      // 16th Skylander indexed would be 0x20. The 3rd byte indicate which block to read from.

      // A response with the 2nd byte of 0x01 indicates an error in the read. Otherwise, the
      // response indicates the Skylander's index in the 2nd byte, the block read in the 3rd byte,
      // data (16 bytes) is contained in bytes 4-19.

      // A Skylander has 64 blocks of data indexed from 0x00 to 0x3f. SwapForce characters have 2
      // character indexes, these may not be sequential.
    case 'Q':
    {
      if (cmd->length == 3)
      {
        const u8 sky_num = buf[1] & 0xF;
        const u8 block = buf[2];
        system.GetSkylanderPortal().QueryBlock(sky_num, block, interrupt_response.data());
        m_queries.push(interrupt_response);
        control_response = {buf[0], buf[1], buf[2]};
        expected_count = 11;
      }
      break;
    }
    case 'R':
    {
      // Ready
      // Command	{ 'R', 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'R', 02, 0a, 03, 02, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // The 4 byte sequence after the R (0x52) is unknown, but appears consistent based on device
      // type.
      if (cmd->length == 2)
      {
        control_response = {0x52, 0x00};
        interrupt_response = {0x52, 0x02, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        m_queries.push(interrupt_response);
        expected_count = 10;
      }
      break;
    }
      // Status
      // Command	{ 'S', 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'S', 55, 00, 00, 55, 3e,
      // (00|01), 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00 }
      // Status is the default command. If you open the HID device and
      // activate the portal, you will get status outputs.

      // The 4 bytes {55, 00, 00, 55} are the status of characters on the portal. The 4 bytes are
      // treated as a 32-bit binary array. Each unique Skylander placed on a board is represented
      // by 2 bits starting with the first Skylander in the least significant bit. This bit is
      // present whenever the Skylandar is added or present on the portal. When the Skylander is
      // added to the board, both bits are set in the next status message as a one-time signal.
      // When a Skylander is removed from the board, only the most significant bit of the 2 bits
      // is set.

      // Different portals can track a different number of RFID tags. The Wii Wireless portal
      // tracks 4, the Wired portal can track 8. The maximum number of unique Skylanders tracked
      // at any time is 16, after which new Skylanders appear to cycle unused bits.

      // Certain Skylanders, e.g. SwapForce Skylanders, are represented as 2 ID in the bit array.
      // This may be due to the presence of 2 RFIDs, one for each half of the Skylander.

      // The 6th byte {3e} is a counter and increments by one. It will roll over when reaching
      // {ff}.

      // The purpose of the (00\|01) byte at the 7th position appear to indicate if the portal has
      // been activated: {01} when active and {00} when deactivated.
    case 'S':
    {
      if (cmd->length == 1)
      {
        // The Status interrupt responses are automatically handled via the GetStatus method
        control_response = {buf[0]};
        expected_count = 9;
      }
      break;
    }
    case 'V':
    {
      if (cmd->length == 4)
      {
        control_response = {buf[0], buf[1], buf[2], buf[3]};
        expected_count = 12;
      }
      break;
    }
      // Write
      // Command	{ 'W', 10, 00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'W', 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // In the command the 2nd byte indicates which Skylander to query data from. Index starts at
      // 0x10 for the 1st Skylander (as reported in the Status command.) The 16th Skylander
      // indexed would be 0x20.

      // 3rd byte is the block to write to.

      // Bytes 4 - 19 ({ 01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f }) are the
      // data to write.

      // The response does not appear to return the id of the Skylander being written, the 2nd
      // byte is 0x00; however, the 3rd byte echos the block that was written (0x00 in example
      // above.)

    case 'W':
    {
      if (cmd->length == 19)
      {
        const u8 sky_num = buf[1] & 0xF;
        const u8 block = buf[2];
        system.GetSkylanderPortal().WriteBlock(sky_num, block, &buf[3], interrupt_response.data());
        m_queries.push(interrupt_response);
        control_response = {buf[0],  buf[1],  buf[2],  buf[3],  buf[4],  buf[5],  buf[6],
                            buf[7],  buf[8],  buf[9],  buf[10], buf[11], buf[12], buf[13],
                            buf[14], buf[15], buf[16], buf[17], buf[18]};
        expected_count = 27;
      }
      break;
    }
    default:
      ERROR_LOG_FMT(IOS_USB, "Unhandled Skylander Portal Query: {}", buf[0]);
      break;
    }
  }

  if (expected_count == 0)
    return IPC_EINVAL;

  ScheduleTransfer(std::move(cmd), control_response, expected_count, expected_time_us);
  return 0;
}

int SkylanderUSB::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={} endpoint={}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  return 0;
}

// When an Interrupt Message is received by the Skylander Portal from the console,
// it needs to respond with either the status of the console if there are no Control Messages that
// require a response, or with the relevant response data that is requested from the Control
// Message.
int SkylanderUSB::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Interrupt: length={} endpoint={}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);
  if (cmd->length == 0 || buf == nullptr)
  {
    ERROR_LOG_FMT(IOS_USB, "Skylander command invalid");
    return IPC_EINVAL;
  }
  std::array<u8, 64> interrupt_response = {};
  s32 expected_count;
  u64 expected_time_us;
  // Audio requests are 64 bytes long, are the only Interrupt requests longer than 32 bytes,
  // echo the request as the response and respond after 1ms
  if (cmd->length > 32 && cmd->length <= 64)
  {
    std::array<u8, 64> audio_interrupt_response = {};
    u8* audio_buf = audio_interrupt_response.data();
    memcpy(audio_buf, buf, cmd->length);
    expected_time_us = 1000;
    expected_count = cmd->length;
    ScheduleTransfer(std::move(cmd), audio_interrupt_response, expected_count, expected_time_us);
    return 0;
  }
  // If some data was requested from the Control Message, then the Interrupt message needs to
  // respond with that data. Check if the queries queue is empty
  if (!m_queries.empty())
  {
    interrupt_response = m_queries.front();
    m_queries.pop();
    // This needs to happen after ~22 milliseconds
    expected_time_us = 22000;
  }
  // If there is no relevant data to respond with, respond with the currentstatus of the Portal
  else
  {
    interrupt_response = system.GetSkylanderPortal().GetStatus();
    expected_time_us = 2000;
  }
  expected_count = 32;
  ScheduleTransfer(std::move(cmd), interrupt_response, expected_count, expected_time_us);
  return 0;
}

int SkylanderUSB::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Isochronous: length={} endpoint={} num_packets={}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);
  return 0;
}

void SkylanderUSB::ScheduleTransfer(std::unique_ptr<TransferCommand> command,
                                    const std::array<u8, 64>& data, s32 expected_count,
                                    u64 expected_time_us)
{
  command->FillBuffer(data.data(), expected_count);
  command->ScheduleTransferCompletion(expected_count, expected_time_us);
}

void Skylander::Save()
{
  if (!sky_file)
    return;

  sky_file.Seek(0, File::SeekOrigin::Begin);
  sky_file.WriteBytes(data.data(), 0x40 * 0x10);
}

void SkylanderPortal::Activate()
{
  std::lock_guard lock(sky_mutex);
  if (m_activated)
  {
    // If the portal was already active no change is needed
    return;
  }

  // If not we need to advertise change to all the figures present on the portal
  for (auto& s : skylanders)
  {
    if (s.status & 1)
    {
      s.queued_status.push(Skylander::ADDED);
      s.queued_status.push(Skylander::READY);
    }
  }

  m_activated = true;
}

void SkylanderPortal::Deactivate()
{
  std::lock_guard lock(sky_mutex);

  for (auto& s : skylanders)
  {
    // check if at the end of the updates there would be a figure on the portal
    if (!s.queued_status.empty())
    {
      s.status = s.queued_status.back();
      s.queued_status = std::queue<u8>();
    }

    s.status &= 1;
  }

  m_activated = false;
}

bool SkylanderPortal::IsActivated()
{
  std::lock_guard lock(sky_mutex);

  return m_activated;
}

void SkylanderPortal::UpdateStatus()
{
  std::lock_guard lock(sky_mutex);

  if (!m_status_updated)
  {
    for (auto& s : skylanders)
    {
      if (s.status & 1)
      {
        s.queued_status.push(Skylander::REMOVED);
        s.queued_status.push(Skylander::ADDED);
        s.queued_status.push(Skylander::READY);
      }
    }
    m_status_updated = true;
  }
}

// Side:
// 0x00 = right
// 0x01 = left and right
// 0x02 = left
// 0x03 = trap
void SkylanderPortal::SetLEDs(u8 side, u8 red, u8 green, u8 blue)
{
  std::lock_guard lock(sky_mutex);
  if (side == 0x00)
  {
    m_color_right.red = red;
    m_color_right.green = green;
    m_color_right.blue = blue;
  }
  else if (side == 0x01)
  {
    m_color_right.red = red;
    m_color_right.green = green;
    m_color_right.blue = blue;

    m_color_left.red = red;
    m_color_left.green = green;
    m_color_left.blue = blue;
  }
  else if (side == 0x02)
  {
    m_color_left.red = red;
    m_color_left.green = green;
    m_color_left.blue = blue;
  }
  else if (side == 0x03)
  {
    m_color_trap.red = red;
    m_color_trap.green = green;
    m_color_trap.blue = blue;
  }
}

std::array<u8, 64> SkylanderPortal::GetStatus()
{
  std::lock_guard lock(sky_mutex);

  u32 status = 0;
  u8 active = 0x00;

  if (m_activated)
  {
    active = 0x01;
  }

  for (int i = MAX_SKYLANDERS - 1; i >= 0; i--)
  {
    auto& s = skylanders[i];

    if (!s.queued_status.empty())
    {
      s.status = s.queued_status.front();
      s.queued_status.pop();
    }
    status <<= 2;
    status |= s.status;
  }

  std::array<u8, 64> interrupt_response = {0x53,   0x00, 0x00, 0x00, 0x00, m_interrupt_counter++,
                                           active, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00};
  memcpy(&interrupt_response[1], &status, sizeof(status));
  return interrupt_response;
}

void SkylanderPortal::QueryBlock(u8 sky_num, u8 block, u8* reply_buf)
{
  if (!IsSkylanderNumberValid(sky_num) || !IsBlockNumberValid(block))
    return;

  std::lock_guard lock(sky_mutex);

  const auto& skylander = skylanders[sky_num];

  reply_buf[0] = 'Q';
  reply_buf[2] = block;
  if (skylander.status & 1)
  {
    reply_buf[1] = (0x10 | sky_num);
    memcpy(reply_buf + 3, skylander.data.data() + (16 * block), 16);
  }
  else
  {
    reply_buf[1] = sky_num;
  }
}

void SkylanderPortal::WriteBlock(u8 sky_num, u8 block, const u8* to_write_buf, u8* reply_buf)
{
  if (!IsSkylanderNumberValid(sky_num) || !IsBlockNumberValid(block))
    return;

  std::lock_guard lock(sky_mutex);

  auto& skylander = skylanders[sky_num];

  reply_buf[0] = 'W';
  reply_buf[2] = block;

  if (skylander.status & 1)
  {
    reply_buf[1] = (0x10 | sky_num);
    memcpy(skylander.data.data() + (block * 16), to_write_buf, 16);
    skylander.Save();
  }
  else
  {
    reply_buf[1] = sky_num;
  }
}

u16 SkylanderCRC16(u16 init_value, const u8* buffer, u32 size)
{
  const unsigned short CRC_CCITT_TABLE[256] = {
      0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A,
      0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294,
      0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462,
      0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509,
      0xE5EE, 0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695,
      0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5,
      0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948,
      0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
      0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
      0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B,
      0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F,
      0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB,
      0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046,
      0x6067, 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290,
      0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E,
      0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
      0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691,
      0x16B0, 0x6657, 0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
      0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D,
      0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16,
      0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8,
      0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E,
      0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93,
      0x3EB2, 0x0ED1, 0x1EF0};

  u16 crc = init_value;

  for (u32 i = 0; i < size; i++)
  {
    const u16 tmp = (crc >> 8) ^ buffer[i];
    crc = (crc << 8) ^ CRC_CCITT_TABLE[tmp];
  }

  return crc;
}

bool SkylanderPortal::CreateSkylander(const std::string& file_path, u16 sky_id, u16 sky_var)
{
  File::IOFile sky_file(file_path, "w+b");
  if (!sky_file)
  {
    return false;
  }

  std::array<u8, 0x40 * 0x10> buf{};
  const auto file_data = buf.data();
  // Set the block permissions
  u32 first_block = 0x690F0F0F;
  u32 other_blocks = 0x69080F7F;
  memcpy(&file_data[0x36], &first_block, sizeof(first_block));
  for (u32 index = 1; index < 0x10; index++)
  {
    memcpy(&file_data[(index * 0x40) + 0x36], &other_blocks, sizeof(other_blocks));
  }

  // Set the NUID of the figure
  Common::Random::Generate(&file_data[0], 4);

  // The BCC (Block Check Character)
  file_data[4] = file_data[0] ^ file_data[1] ^ file_data[2] ^ file_data[3];

  // ATQA
  file_data[5] = 0x81;
  file_data[6] = 0x01;

  // SAK
  file_data[7] = 0x0F;

  // Set the skylander info
  memcpy(&file_data[0x10], &sky_id, sizeof(sky_id));
  memcpy(&file_data[0x1C], &sky_var, sizeof(sky_var));

  // Set checksum
  u16 checksum = SkylanderCRC16(0xFFFF, file_data, 0x1E);
  memcpy(&file_data[0x1E], &checksum, sizeof(checksum));

  sky_file.WriteBytes(buf.data(), buf.size());
  return true;
}

bool SkylanderPortal::RemoveSkylander(u8 sky_num)
{
  if (!IsSkylanderNumberValid(sky_num))
    return false;

  DEBUG_LOG_FMT(IOS_USB, "Cleared Skylander from slot {}", sky_num);
  std::lock_guard lock(sky_mutex);
  auto& skylander = skylanders[sky_num];

  if (skylander.status & 1)
  {
    skylander.status = Skylander::REMOVING;
    skylander.queued_status.push(Skylander::REMOVING);
    skylander.queued_status.push(Skylander::REMOVED);
    return true;
  }

  return false;
}

u8 SkylanderPortal::LoadSkylander(u8* buf, File::IOFile in_file)
{
  std::lock_guard lock(sky_mutex);

  u32 sky_serial = 0;
  for (int i = 3; i > -1; i--)
  {
    sky_serial <<= 8;
    sky_serial |= buf[i];
  }
  u8 found_slot = 0xFF;

  // mimics spot retaining on the portal
  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    if ((skylanders[i].status & 1) == 0)
    {
      if (skylanders[i].last_id == sky_serial)
      {
        DEBUG_LOG_FMT(IOS_USB, "Last Id: {}", skylanders[i].last_id);

        found_slot = i;
        break;
      }

      if (i < found_slot)
      {
        DEBUG_LOG_FMT(IOS_USB, "Last Id: {}", skylanders[i].last_id);
        found_slot = i;
      }
    }
  }

  if (found_slot != 0xFF)
  {
    auto& skylander = skylanders[found_slot];
    memcpy(skylander.data.data(), buf, skylander.data.size());
    DEBUG_LOG_FMT(IOS_USB, "Skylander Data: \n{}",
                  HexDump(skylander.data.data(), skylander.data.size()));
    skylander.sky_file = std::move(in_file);
    skylander.status = Skylander::ADDED;
    skylander.queued_status.push(Skylander::ADDED);
    skylander.queued_status.push(Skylander::READY);
    skylander.last_id = sky_serial;
  }
  return found_slot;
}

bool SkylanderPortal::IsSkylanderNumberValid(u8 sky_num)
{
  return sky_num < MAX_SKYLANDERS;
}

bool SkylanderPortal::IsBlockNumberValid(u8 block)
{
  return block < 64;
}

}  // namespace IOS::HLE::USB
