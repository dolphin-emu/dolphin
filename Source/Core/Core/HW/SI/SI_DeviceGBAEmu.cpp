// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceGBAEmu.h"

#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GBACore.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Host.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

namespace SerialInterface
{
static s64 GetSyncInterval(const SystemTimers::SystemTimersManager& timers)
{
  return timers.GetTicksPerSecond() / 1000;
}

CSIDevice_GBAEmu::CSIDevice_GBAEmu(Core::System& system, SIDevices device, int device_number)
    : ISIDevice(system, device, device_number)
{
  m_core = std::make_shared<HW::GBA::Core>(system, m_device_number);
  m_core->Start(system.GetCoreTiming().GetTicks());
  m_gbahost = Host_CreateGBAHost(m_core);
  m_core->SetHost(m_gbahost);
  system.GetSerialInterface().ScheduleEvent(m_device_number,
                                            GetSyncInterval(system.GetSystemTimers()));
}

CSIDevice_GBAEmu::~CSIDevice_GBAEmu()
{
  m_system.GetSerialInterface().RemoveEvent(m_device_number);
  m_core->Stop();
  m_gbahost.reset();
  m_core.reset();
}

int CSIDevice_GBAEmu::RunBuffer(u8* buffer, int request_length)
{
  switch (m_next_action)
  {
  case NextAction::SendCommand:
  {
#ifdef _DEBUG
    NOTICE_LOG_FMT(SERIALINTERFACE, "{} cmd {:02x} [> {:02x}{:02x}{:02x}{:02x}]", m_device_number,
                   buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
#endif
    m_last_cmd = static_cast<EBufferCommands>(buffer[0]);
    m_timestamp_sent = m_system.GetCoreTiming().GetTicks();
    m_core->SendJoybusCommand(m_timestamp_sent, TransferInterval(), buffer, m_keys);

    auto& si = m_system.GetSerialInterface();
    si.RemoveEvent(m_device_number);
    si.ScheduleEvent(m_device_number,
                     TransferInterval() + GetSyncInterval(m_system.GetSystemTimers()));
    for (int i = 0; i < MAX_SI_CHANNELS; ++i)
    {
      if (i == m_device_number || si.GetDeviceType(i) != GetDeviceType())
        continue;
      si.RemoveEvent(i);
      si.ScheduleEvent(i, 0, static_cast<u64>(TransferInterval()));
    }

    m_next_action = NextAction::WaitTransferTime;
    [[fallthrough]];
  }

  case NextAction::WaitTransferTime:
  {
    int elapsed_time = static_cast<int>(m_system.GetCoreTiming().GetTicks() - m_timestamp_sent);
    // Tell SI to ask again after TransferInterval() cycles
    if (TransferInterval() > elapsed_time)
      return 0;
    m_next_action = NextAction::ReceiveResponse;
    [[fallthrough]];
  }

  case NextAction::ReceiveResponse:
  {
    m_next_action = NextAction::SendCommand;

    std::vector<u8> response = m_core->GetJoybusResponse();
    if (response.empty())
      return -1;
    std::ranges::copy(response, buffer);

#ifdef _DEBUG
    const Common::Log::LogLevel log_level =
        (m_last_cmd == EBufferCommands::CMD_STATUS || m_last_cmd == EBufferCommands::CMD_RESET) ?
            Common::Log::LogLevel::LERROR :
            Common::Log::LogLevel::LWARNING;
    GENERIC_LOG_FMT(Common::Log::LogType::SERIALINTERFACE, log_level,
                    "{}                              [< {:02x}{:02x}{:02x}{:02x}{:02x}] ({})",
                    m_device_number, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
                    response.size());
#endif

    return static_cast<int>(response.size());
  }
  }

  // This should never happen, but appease MSVC which thinks it might.
  ERROR_LOG_FMT(SERIALINTERFACE, "Unknown state {}\n", static_cast<int>(m_next_action));
  return -1;
}

int CSIDevice_GBAEmu::TransferInterval()
{
  return SIDevice_GetGBATransferTime(m_system.GetSystemTimers(), m_last_cmd);
}

bool CSIDevice_GBAEmu::GetData(u32& hi, u32& low)
{
  GCPadStatus pad_status{};
  if (!NetPlay::IsNetPlayRunning())
    pad_status = Pad::GetGBAStatus(m_device_number);
  SerialInterface::CSIDevice_GCController::HandleMoviePadStatus(m_system.GetMovie(),
                                                                m_device_number, &pad_status);

  static constexpr std::array<PadButton, 10> buttons_map = {
      PadButton::PAD_BUTTON_A,      // A
      PadButton::PAD_BUTTON_B,      // B
      PadButton::PAD_TRIGGER_Z,     // Select
      PadButton::PAD_BUTTON_START,  // Start
      PadButton::PAD_BUTTON_RIGHT,  // Right
      PadButton::PAD_BUTTON_LEFT,   // Left
      PadButton::PAD_BUTTON_UP,     // Up
      PadButton::PAD_BUTTON_DOWN,   // Down
      PadButton::PAD_TRIGGER_R,     // R
      PadButton::PAD_TRIGGER_L,     // L
  };

  m_keys = 0;
  for (size_t i = 0; i < buttons_map.size(); ++i)
    m_keys |= static_cast<u16>(static_cast<bool>((pad_status.button & buttons_map[i]))) << i;

  // Use X button as a reset signal for NetPlay/Movies
  if (pad_status.button & PadButton::PAD_BUTTON_X)
    m_core->Reset();

  return false;
}

void CSIDevice_GBAEmu::SendCommand(u32 command, u8 poll)
{
}

void CSIDevice_GBAEmu::DoState(PointerWrap& p)
{
  p.Do(m_next_action);
  p.Do(m_last_cmd);
  p.Do(m_timestamp_sent);
  p.Do(m_keys);
  m_core->DoState(p);
}

void CSIDevice_GBAEmu::OnEvent(u64 userdata, s64 cycles_late)
{
  m_core->SendJoybusCommand(m_system.GetCoreTiming().GetTicks() + userdata, 0, nullptr, m_keys);

  const auto num_cycles = userdata + GetSyncInterval(m_system.GetSystemTimers());
  m_system.GetSerialInterface().ScheduleEvent(m_device_number, num_cycles);
}
}  // namespace SerialInterface
