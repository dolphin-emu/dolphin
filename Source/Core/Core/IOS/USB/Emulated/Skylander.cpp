// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylander.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include <libusb.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{

SkylanderPortal g_skyportal;

SkylanderUsb::SkylanderUsb(Kernel& ios, const std::string& device_name)
    : EmulatedUsbDevice(ios, device_name)
{
  m_vid = 0x1430;
  m_pid = 0x150;
  deviceDesc = DeviceDescriptor{18, 1, 512, 0, 0, 0, 64, 5168, 336, 256, 1, 2, 0, 1};
  configDesc.emplace_back(ConfigDescriptor{9, 2, 41, 1, 1, 0, 128, 250});
  interfaceDesc.emplace_back(InterfaceDescriptor{9, 4, 0, 0, 2, 3, 0, 0, 0});
  endpointDesc.emplace_back(EndpointDescriptor{7, 5, 129, 3, 64, 1});
  endpointDesc.emplace_back(EndpointDescriptor{7, 5, 2, 3, 64, 1});
}

SkylanderUsb::~SkylanderUsb()
{
}

DeviceDescriptor SkylanderUsb::GetDeviceDescriptor() const
{
  return deviceDesc;
}

std::vector<ConfigDescriptor> SkylanderUsb::GetConfigurations() const
{
  return configDesc;
}

std::vector<InterfaceDescriptor> SkylanderUsb::GetInterfaces(u8 config) const
{
  return interfaceDesc;
}

std::vector<EndpointDescriptor> SkylanderUsb::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return endpointDesc;
}

bool SkylanderUsb::Attach()
{
  if (m_device_attached)
  {
    return true;
  }
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  m_device_attached = true;
  if (!m_has_initialised && !Core::WantsDeterminism())
  {
    GetTransferThread().Start();
    m_has_initialised = true;
  }
  return true;
}

bool SkylanderUsb::AttachAndChangeInterface(const u8 interface)
{
  return true;
}

int SkylanderUsb::CancelTransfer(u8 endpoint)
{
  return IPC_SUCCESS;
}

int SkylanderUsb::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int SkylanderUsb::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int SkylanderUsb::SetAltSetting(u8 alt_setting)
{
  return 0;
}

int SkylanderUsb::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  cmd->expected_time = Common::Timer::NowUs() + 100;
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);
  std::array<u8, 64> q_result = {};
  std::array<u8, 64> q_data = {};
  // Control transfers are instantaneous
  switch (cmd->request_type)
  {
  // HID host to device type
  case 0x21:
    switch (cmd->request)
    {
    case 0x09:
      switch (buf[0])
      {
      case 'A':
      {
        // Activation
        // Command	{ 'A', (00 | 01), 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 } Response	{ 'A', (00 | 01),
        // ff, 77, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00 } The 3rd byte of the command is whether to activate
        // (0x01) or deactivate (0x00) the portal. The response echos back the activation byte as
        // the 2nd byte of the response. The 3rd and 4th bytes of the response appear to vary from
        // wired to wireless. On wired portals, the bytes appear to always be ff 77. On wireless
        // portals, during activation the 3rd byte appears to count down from ff (possibly a battery
        // power indication) and during deactivation ed and eb responses have been observed. The 4th
        // byte appears to always be 00 for wireless portals.

        // Wii U Wireless: 41 01 f4 00 41 00 ed 00 41 01 f4 00 41 00 eb 00 41 01 f3 00 41 00 ed 00
        if (cmd->length == 2 || cmd->length == 32)
        {
          q_data = {buf[0], buf[1]};
          q_result = {0x41, buf[1], 0xFF, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
          q_queries.push(q_result);
          cmd->expected_count = 10;
          g_skyportal.Activate();
        }
        break;
      }
      case 'C':
      {
        // Color
        // Command	{ 'C', 12, 34, 56, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 } Response	{ 'C', 12, 34, 56, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00 } The 3 bytes {12, 34, 56} are RGB values.

        // This command should set the color of the LED in the portal, however this appears
        // deprecated in most of the recent portals. On portals that do not have LEDs, this command
        // is silently ignored and do not require a response.
        if (cmd->length == 4 || cmd->length == 32)
        {
          g_skyportal.SetLEDs(buf[1], buf[2], buf[3]);
          q_data = {0x43, buf[1], buf[2], buf[3]};
          cmd->expected_count = 12;
        }
        break;
      }
      case 'J':
      {
        // Sync status from game?
        if (cmd->length == 7)
        {
          q_data = {buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]};
          cmd->expected_count = 15;
          q_result = {buf[0]};
          q_queries.push(q_result);
        }
        break;
      }
      case 'L':
      {
        // Audio Download status?
        if (cmd->length == 5)
        {
          q_data = {buf[0], buf[1], buf[2], buf[3], buf[4]};
          cmd->expected_count = 13;
        }
        break;
      }
      case 'M':
      {
        // Audio Firmware version
        // Return version of 0 to prevent attempts to
        // play audio on the portal
        if (cmd->length == 2)
        {
          q_data = {buf[0], buf[1]};
          cmd->expected_count = 10;
          q_result = {buf[0], buf[1], 0x00, 0x19};
          q_queries.push(q_result);
        }
        break;
      }
        // Query
        // Command	{ 'Q', 10, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 } Response	{ 'Q', 10, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00 } In the command the 3rd byte indicates which Skylander to query data
        // from. Index starts at 0x10 for the 1st Skylander (as reported in the Status command.) The
        // 16th Skylander indexed would be 0x20.

        // A response with the 2nd byte of 0x01 indicates an error in the read. Otherwise, the
        // response indicates the Skylander's index in the 2nd byte, the block read in the 3rd byte,
        // data (16 bytes) is contained in bytes 4-20.

        // A Skylander has 64 blocks of data indexed from 0x00 to 0x3f. SwapForce characters have 2
        // character indexes, these may not be sequential.
      case 'Q':
      {
        // Queries a block
        const u8 sky_num = buf[1] & 0xF;
        const u8 block = buf[2];
        g_skyportal.QueryBlock(sky_num, block, q_result.data());
        q_queries.push(q_result);
        q_data = {buf[0], buf[1], buf[2]};
        cmd->expected_count = 11;
        break;
      }
      case 'R':
      {
        // Ready
        // Command	{ 'R', 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 } Response	{ 'R', 02, 0a, 03, 02, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00 } The 4 byte sequence after the R (0x52) is unknown, but appears
        // consistent based on device type.
        if (cmd->length == 2 || cmd->length == 32)
        {
          q_data = {0x52, 0x00};
          q_result = {0x52, 0x02, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
          q_queries.push(q_result);
          cmd->expected_count = 10;
        }
        break;
      }
        // Status
        // Command	{ 'S', 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 } Response	{ 'S', 55, 00, 00, 55, 3e,
        // (00|01), 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00 } Status is the default command. If you open the HID device and
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
        q_data = {buf[0]};
        cmd->expected_count = 9;
        g_skyportal.UpdateStatus();
        break;
      }
      case 'V':
      {
        q_data = {buf[0], buf[1], buf[2], buf[3]};
        cmd->expected_count = 12;
        break;
      }
        // Write
        // Command	{ 'W', 10, 00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 } Response	{ 'W', 00, 00, 00, 00, 00,
        // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        // 00, 00, 00, 00 } In the command the 3rd byte indicates which Skylander to query data
        // from. Index starts at 0x10 for the 1st Skylander (as reported in the Status command.) The
        // 16th Skylander indexed would be 0x20.

        // 4th byte is the block to write to.

        // Bytes 5 - 20 ({ 01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f }) are the
        // data to write.

        // The response does not appear to return the id of the Skylander being written, the 2nd
        // byte is 0x00; however, the 3rd byte echos the block that was written (0x00 in example
        // above.)

      case 'W':
      {
        const u8 sky_num = buf[1] & 0xF;
        const u8 block = buf[2];
        g_skyportal.WriteBlock(sky_num, block, &buf[3], q_result.data());
        q_queries.push(q_result);
        q_data = {buf[0],  buf[1],  buf[2],  buf[3],  buf[4],  buf[5],  buf[6],
                  buf[7],  buf[8],  buf[9],  buf[10], buf[11], buf[12], buf[13],
                  buf[14], buf[15], buf[16], buf[17], buf[18]};
        cmd->expected_count = 19;
        break;
      }
      default:
        ERROR_LOG_FMT(IOS_USB, "Unhandled Skylander Portal Query: {}", buf[0]);
        break;
      }
      break;
      break;
    case 0x0A:
      cmd->expected_count = 8;
      cmd->OnTransferComplete(8);
      return 0;
    case 0x0B:
      cmd->expected_count = 8;
      cmd->OnTransferComplete(8);
      return 0;
    default:
      ERROR_LOG_FMT(IOS_USB, "Unhandled Request {}", cmd->request);
      break;
    }
    break;

  default:
    break;
  }
  cmd->expected_time = Common::Timer::NowUs() + 100;
  GetTransferThread().AddTransfer(std::move(cmd), q_data);
  return 0;
}
int SkylanderUsb::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={} endpoint={}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  return 0;
}
int SkylanderUsb::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Interrupt: length={} endpoint={}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);
  std::array<u8, 64> q_result = {};
  if(cmd->length > 32)
  {
    std::array<u8, 64> q_audio_result = {};
    u8* audio_buf = q_audio_result.data();
    memcpy(audio_buf, buf, cmd->length);
    cmd->expected_time = Common::Timer::NowUs() + 1000;
    cmd->expected_count = cmd->length;
    GetTransferThread().AddTransfer(std::move(cmd), q_audio_result);
    return 0;
  }
  if (!q_queries.empty())
  {
    q_result = q_queries.front();
    q_queries.pop();
    // This needs to happen after ~22 milliseconds
    cmd->expected_time = Common::Timer::NowUs() + 22000;
  }
  else
  {
    q_result = g_skyportal.GetStatus(buf);
    cmd->expected_time = Common::Timer::NowUs() + 2000;
  }
  cmd->expected_count = 32;
  GetTransferThread().AddTransfer(std::move(cmd), q_result);
  return 0;
}
int SkylanderUsb::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Isochronous: length={} endpoint={} num_packets={}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);
  return 0;
}

SkylanderUsb::FakeTransferThread::~FakeTransferThread()
{
  Stop();
}

void SkylanderUsb::FakeTransferThread::Start()
{
  if (Core::WantsDeterminism())
    return;

  if (m_thread_running.TestAndSet())
  {
    m_thread = std::thread([this] {
      Common::SetCurrentThreadName("Fake Transfer Thread");
      while (m_thread_running.IsSet())
      {
        if (!m_transfers.empty())
        {
          std::lock_guard lk{m_transfers_mutex};
          u64 timestamp = Common::Timer::NowUs();
          for (auto iterator = m_transfers.begin(); iterator != m_transfers.end();)
          {
            auto* command = iterator->second.get();
            if (command->expected_time > timestamp)
            {
              ++iterator;
              continue;
            }
            command->FillBuffer(iterator->first.data(), command->expected_count);
            command->OnTransferComplete(command->expected_count);
            iterator = m_transfers.erase(iterator);
          }
        }
      }
    });
  }
}

void SkylanderUsb::FakeTransferThread::Stop()
{
  if (m_thread_running.TestAndClear())
    m_thread.join();
}

void SkylanderUsb::FakeTransferThread::AddTransfer(std::unique_ptr<TransferCommand> command,
                                                   std::array<u8, 64> data)
{
  std::lock_guard lk{m_transfers_mutex};
  m_transfers.emplace(data, std::move(command));
}

void Skylander::save()
{
  if (!sky_file)
  {
    return;
  }

  {
    sky_file.Seek(0, File::SeekOrigin::Begin);
    sky_file.WriteBytes(data.data(), 0x40 * 0x10);
  }
}

void SkylanderPortal::Activate()
{
  std::lock_guard lock(sky_mutex);
  if (activated)
  {
    // If the portal was already active no change is needed
    return;
  }

  // If not we need to advertise change to all the figures present on the portal
  for (auto& s : skylanders)
  {
    if (s.status & 1)
    {
      s.queued_status.push(3);
      s.queued_status.push(1);
    }
  }

  activated = true;
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

  activated = false;
}

void SkylanderPortal::UpdateStatus()
{
  std::lock_guard lock(sky_mutex);

  if (!status_updated)
  {
    for (auto& s : skylanders)
    {
      if (s.status & 1)
      {
        s.queued_status.push(0);
        s.queued_status.push(3);
        s.queued_status.push(1);
      }
    }
    status_updated = true;
  }
}

void SkylanderPortal::SetLEDs(u8 red, u8 green, u8 blue)
{
  std::lock_guard lock(sky_mutex);
  this->r = red;
  this->g = green;
  this->b = blue;
}

std::array<u8, 64> SkylanderPortal::GetStatus(u8* reply_buf)
{
  std::lock_guard lock(sky_mutex);

  u16 status = 0;
  u8 active = 0x00;

  if (activated)
  {
    active = 0x01;
  }

  for (int i = 7; i >= 0; i--)
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

  std::memset(reply_buf, 0, 0x20);
  // write_to_ptr<le_t<u16>>(reply_buf, 1, status);

  std::array<u8, 64> q_result = {0x53,   0x00, 0x00, 0x00, 0x00, interrupt_counter++,
                                 active, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00,   0x00};
  memcpy(&q_result.data()[1], &status, sizeof(status));
  return q_result;
}

void SkylanderPortal::QueryBlock(u8 sky_num, u8 block, u8* reply_buf)
{
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
  std::lock_guard lock(sky_mutex);

  auto& skylander = skylanders[sky_num];

  reply_buf[0] = 'W';
  reply_buf[2] = block;

  if (skylander.status & 1)
  {
    reply_buf[1] = (0x10 | sky_num);
    memcpy(skylander.data.data() + (block * 16), to_write_buf, 16);
    skylander.save();
  }
  else
  {
    reply_buf[1] = sky_num;
  }
}

bool SkylanderPortal::RemoveSkylander(u8 sky_num)
{
  DEBUG_LOG_FMT(IOS_USB, "Cleared Skylander from slot {}", sky_num);
  std::lock_guard lock(sky_mutex);
  auto& skylander = skylanders[sky_num];

  if (skylander.status & 1)
  {
    skylander.status = 2;
    skylander.queued_status.push(2);
    skylander.queued_status.push(0);
    skylander.sky_file.Close();
    return true;
  }

  return false;
}

u8 SkylanderPortal::LoadSkylander(u8* buf, File::IOFile in_file)
{
  std::lock_guard lock(sky_mutex);


  // u32 sky_serial = *utils::bless<le_t<u32>>(buf);
  u32 sky_serial = 0;
  for (int i = 3; i > -1; i--)
  {
    sky_serial <<= 8;
    sky_serial |= buf[i];
  }
  u8 found_slot = 0xFF;

  // mimics spot retaining on the portal
  for (auto i = 0; i < 8; i++)
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
    DEBUG_LOG_FMT(IOS_USB, "Skylander Data: \n{}", HexDump(skylander.data.data(), skylander.data.size()));
    skylander.sky_file = std::move(in_file);
    skylander.status = 3;
    skylander.queued_status.push(3);
    skylander.queued_status.push(1);
    skylander.last_id = sky_serial;
  }
  return found_slot;
}

}  // namespace IOS::HLE::USB