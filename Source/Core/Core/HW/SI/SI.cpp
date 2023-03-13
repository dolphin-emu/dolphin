// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>

#include "Common/BitField.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI_DeviceGBA.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace SerialInterface
{
// SI Internal Hardware Addresses
enum
{
  SI_CHANNEL_0_OUT = 0x00,
  SI_CHANNEL_0_IN_HI = 0x04,
  SI_CHANNEL_0_IN_LO = 0x08,
  SI_CHANNEL_1_OUT = 0x0C,
  SI_CHANNEL_1_IN_HI = 0x10,
  SI_CHANNEL_1_IN_LO = 0x14,
  SI_CHANNEL_2_OUT = 0x18,
  SI_CHANNEL_2_IN_HI = 0x1C,
  SI_CHANNEL_2_IN_LO = 0x20,
  SI_CHANNEL_3_OUT = 0x24,
  SI_CHANNEL_3_IN_HI = 0x28,
  SI_CHANNEL_3_IN_LO = 0x2C,
  SI_POLL = 0x30,
  SI_COM_CSR = 0x34,
  SI_STATUS_REG = 0x38,
  SI_EXI_CLOCK_COUNT = 0x3C,
  SI_IO_BUFFER = 0x80,
};

SerialInterfaceManager::SerialInterfaceManager(Core::System& system) : m_system(system)
{
}

SerialInterfaceManager::~SerialInterfaceManager() = default;

void SerialInterfaceManager::SetNoResponse(u32 channel)
{
  // raise the NO RESPONSE error
  switch (channel)
  {
  case 0:
    m_status_reg.NOREP0 = 1;
    break;
  case 1:
    m_status_reg.NOREP1 = 1;
    break;
  case 2:
    m_status_reg.NOREP2 = 1;
    break;
  case 3:
    m_status_reg.NOREP3 = 1;
    break;
  }
}

void SerialInterfaceManager::ChangeDeviceCallback(Core::System& system, u64 user_data,
                                                  s64 cycles_late)
{
  // The purpose of this callback is to simply re-enable device changes.
  auto& si = system.GetSerialInterface();
  si.m_channel[user_data].has_recent_device_change = false;
}

void SerialInterfaceManager::UpdateInterrupts()
{
  // check if we have to update the RDSTINT flag
  if (m_status_reg.RDST0 || m_status_reg.RDST1 || m_status_reg.RDST2 || m_status_reg.RDST3)
  {
    m_com_csr.RDSTINT = 1;
  }
  else
  {
    m_com_csr.RDSTINT = 0;
  }

  // check if we have to generate an interrupt
  const bool generate_interrupt = (m_com_csr.RDSTINT & m_com_csr.RDSTINTMSK) != 0 ||
                                  (m_com_csr.TCINT & m_com_csr.TCINTMSK) != 0;

  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_SI,
                                                generate_interrupt);
}

void SerialInterfaceManager::GenerateSIInterrupt(SIInterruptType type)
{
  switch (type)
  {
  case INT_RDSTINT:
    m_com_csr.RDSTINT = 1;
    break;
  case INT_TCINT:
    m_com_csr.TCINT = 1;
    break;
  }

  UpdateInterrupts();
}

constexpr u32 SI_XFER_LENGTH_MASK = 0x7f;

// Translate [0,1,2,...,126,127] to [128,1,2,...,126,127]
constexpr s32 ConvertSILengthField(u32 field)
{
  return ((field - 1) & SI_XFER_LENGTH_MASK) + 1;
}

void SerialInterfaceManager::GlobalRunSIBuffer(Core::System& system, u64 user_data, s64 cycles_late)
{
  system.GetSerialInterface().RunSIBuffer(user_data, cycles_late);
}

void SerialInterfaceManager::RunSIBuffer(u64 user_data, s64 cycles_late)
{
  if (m_com_csr.TSTART)
  {
    const s32 request_length = ConvertSILengthField(m_com_csr.OUTLNGTH);
    const s32 expected_response_length = ConvertSILengthField(m_com_csr.INLNGTH);
    const std::vector<u8> request_copy(m_si_buffer.data(), m_si_buffer.data() + request_length);

    const std::unique_ptr<ISIDevice>& device = m_channel[m_com_csr.CHANNEL].device;
    const s32 actual_response_length = device->RunBuffer(m_si_buffer.data(), request_length);

    DEBUG_LOG_FMT(SERIALINTERFACE,
                  "RunSIBuffer  chan: {}  request_length: {}  expected_response_length: {}  "
                  "actual_response_length: {}",
                  m_com_csr.CHANNEL, request_length, expected_response_length,
                  actual_response_length);
    if (actual_response_length > 0 && expected_response_length != actual_response_length)
    {
      std::ostringstream ss;
      for (u8 b : request_copy)
      {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b << ' ';
      }
      DEBUG_LOG_FMT(
          SERIALINTERFACE,
          "RunSIBuffer: expected_response_length({}) != actual_response_length({}): request: {}",
          expected_response_length, actual_response_length, ss.str());
    }

    // TODO:
    // 1) Wait a reasonable amount of time for the result to be available:
    //    request is N bytes, ends with a stop bit
    //    response in M bytes, ends with a stop bit
    //    processing controller-side takes K us (investigate?)
    //    each bit takes 4us ([3us low/1us high] for a 0, [1us low/3us high] for a 1)
    //    time until response is available is at least: K + ((N*8 + 1) + (M*8 + 1)) * 4 us
    // 2) Investigate the timeout period for NOREP0
    if (actual_response_length != 0)
    {
      m_com_csr.TSTART = 0;
      m_com_csr.COMERR = actual_response_length < 0;
      if (actual_response_length < 0)
        SetNoResponse(m_com_csr.CHANNEL);
      GenerateSIInterrupt(INT_TCINT);
    }
    else
    {
      m_system.GetCoreTiming().ScheduleEvent(device->TransferInterval() - cycles_late,
                                             m_event_type_tranfer_pending);
    }
  }
}

void SerialInterfaceManager::DoState(PointerWrap& p)
{
  for (int i = 0; i < MAX_SI_CHANNELS; i++)
  {
    p.Do(m_channel[i].in_hi.hex);
    p.Do(m_channel[i].in_lo.hex);
    p.Do(m_channel[i].out.hex);
    p.Do(m_channel[i].has_recent_device_change);

    std::unique_ptr<ISIDevice>& device = m_channel[i].device;
    SIDevices type = device->GetDeviceType();
    p.Do(type);

    if (type != device->GetDeviceType())
    {
      AddDevice(SIDevice_Create(m_system, type, i));
    }

    device->DoState(p);
  }

  p.Do(m_poll);
  p.Do(m_com_csr);
  p.Do(m_status_reg);
  p.Do(m_exi_clock_count);
  p.Do(m_si_buffer);
}

template <int device_number>
void SerialInterfaceManager::DeviceEventCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& si = system.GetSerialInterface();
  si.m_channel[device_number].device->OnEvent(userdata, cyclesLate);
}

void SerialInterfaceManager::RegisterEvents()
{
  auto& core_timing = m_system.GetCoreTiming();
  m_event_type_change_device = core_timing.RegisterEvent("ChangeSIDevice", ChangeDeviceCallback);
  m_event_type_tranfer_pending = core_timing.RegisterEvent("SITransferPending", GlobalRunSIBuffer);

  constexpr std::array<CoreTiming::TimedCallback, MAX_SI_CHANNELS> event_callbacks = {
      DeviceEventCallback<0>,
      DeviceEventCallback<1>,
      DeviceEventCallback<2>,
      DeviceEventCallback<3>,
  };
  for (int i = 0; i < MAX_SI_CHANNELS; ++i)
  {
    m_event_types_device[i] =
        core_timing.RegisterEvent(fmt::format("SIEventChannel{}", i), event_callbacks[i]);
  }
}

void SerialInterfaceManager::ScheduleEvent(int device_number, s64 cycles_into_future, u64 userdata)
{
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.ScheduleEvent(cycles_into_future, m_event_types_device[device_number], userdata);
}

void SerialInterfaceManager::RemoveEvent(int device_number)
{
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.RemoveEvent(m_event_types_device[device_number]);
}

void SerialInterfaceManager::Init()
{
  RegisterEvents();

  for (int i = 0; i < MAX_SI_CHANNELS; i++)
  {
    m_channel[i].out.hex = 0;
    m_channel[i].in_hi.hex = 0;
    m_channel[i].in_lo.hex = 0;
    m_channel[i].has_recent_device_change = false;

    if (Movie::IsMovieActive())
    {
      m_desired_device_types[i] = SIDEVICE_NONE;

      if (Movie::IsUsingGBA(i))
      {
        m_desired_device_types[i] = SIDEVICE_GC_GBA_EMULATED;
      }
      else if (Movie::IsUsingPad(i))
      {
        SIDevices current = Config::Get(Config::GetInfoForSIDevice(i));
        // GC pad-compatible devices can be used for both playing and recording
        if (Movie::IsUsingBongo(i))
          m_desired_device_types[i] = SIDEVICE_GC_TARUKONGA;
        else if (SIDevice_IsGCController(current))
          m_desired_device_types[i] = current;
        else
          m_desired_device_types[i] = SIDEVICE_GC_CONTROLLER;
      }
    }
    else if (!NetPlay::IsNetPlayRunning())
    {
      m_desired_device_types[i] = Config::Get(Config::GetInfoForSIDevice(i));
    }

    AddDevice(m_desired_device_types[i], i);
  }

  m_poll.hex = 0;
  m_poll.X = 492;

  m_com_csr.hex = 0;

  m_status_reg.hex = 0;

  m_exi_clock_count.hex = 0;

  // Supposedly set on reset, but logs from real Wii don't look like it is...
  // m_exi_clock_count.LOCK = 1;

  m_si_buffer = {};
}

void SerialInterfaceManager::Shutdown()
{
  for (int i = 0; i < MAX_SI_CHANNELS; i++)
    RemoveDevice(i);
  GBAConnectionWaiter_Shutdown();
}

void SerialInterfaceManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Register SI buffer direct accesses.
  const u32 io_buffer_base = base | SI_IO_BUFFER;
  for (size_t i = 0; i < m_si_buffer.size(); i += sizeof(u32))
  {
    const u32 address = base | static_cast<u32>(io_buffer_base + i);

    mmio->Register(address, MMIO::ComplexRead<u32>([i](Core::System& system, u32) {
                     auto& si = system.GetSerialInterface();
                     u32 val;
                     std::memcpy(&val, &si.m_si_buffer[i], sizeof(val));
                     return Common::swap32(val);
                   }),
                   MMIO::ComplexWrite<u32>([i](Core::System& system, u32, u32 val) {
                     auto& si = system.GetSerialInterface();
                     val = Common::swap32(val);
                     std::memcpy(&si.m_si_buffer[i], &val, sizeof(val));
                   }));
  }
  for (size_t i = 0; i < m_si_buffer.size(); i += sizeof(u16))
  {
    const u32 address = base | static_cast<u32>(io_buffer_base + i);

    mmio->Register(address, MMIO::ComplexRead<u16>([i](Core::System& system, u32) {
                     auto& si = system.GetSerialInterface();
                     u16 val;
                     std::memcpy(&val, &si.m_si_buffer[i], sizeof(val));
                     return Common::swap16(val);
                   }),
                   MMIO::ComplexWrite<u16>([i](Core::System& system, u32, u16 val) {
                     auto& si = system.GetSerialInterface();
                     val = Common::swap16(val);
                     std::memcpy(&si.m_si_buffer[i], &val, sizeof(val));
                   }));
  }

  // In and out for the 4 SI channels.
  for (u32 i = 0; i < u32(MAX_SI_CHANNELS); ++i)
  {
    // We need to clear the RDST bit for the SI channel when reading.
    // CH0 -> Bit 24 + 5
    // CH1 -> Bit 16 + 5
    // CH2 -> Bit 8 + 5
    // CH3 -> Bit 0 + 5
    const u32 rdst_bit = 8 * (3 - i) + 5;

    mmio->Register(base | (SI_CHANNEL_0_OUT + 0xC * i),
                   MMIO::DirectRead<u32>(&m_channel[i].out.hex),
                   MMIO::DirectWrite<u32>(&m_channel[i].out.hex));
    mmio->Register(base | (SI_CHANNEL_0_IN_HI + 0xC * i),
                   MMIO::ComplexRead<u32>([i, rdst_bit](Core::System& system, u32) {
                     auto& si = system.GetSerialInterface();
                     si.m_status_reg.hex &= ~(1U << rdst_bit);
                     si.UpdateInterrupts();
                     return si.m_channel[i].in_hi.hex;
                   }),
                   MMIO::DirectWrite<u32>(&m_channel[i].in_hi.hex));
    mmio->Register(base | (SI_CHANNEL_0_IN_LO + 0xC * i),
                   MMIO::ComplexRead<u32>([i, rdst_bit](Core::System& system, u32) {
                     auto& si = system.GetSerialInterface();
                     si.m_status_reg.hex &= ~(1U << rdst_bit);
                     si.UpdateInterrupts();
                     return si.m_channel[i].in_lo.hex;
                   }),
                   MMIO::DirectWrite<u32>(&m_channel[i].in_lo.hex));
  }

  mmio->Register(base | SI_POLL, MMIO::DirectRead<u32>(&m_poll.hex),
                 MMIO::DirectWrite<u32>(&m_poll.hex));

  mmio->Register(base | SI_COM_CSR, MMIO::DirectRead<u32>(&m_com_csr.hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& si = system.GetSerialInterface();
                   const USIComCSR tmp_com_csr(val);

                   si.m_com_csr.CHANNEL = tmp_com_csr.CHANNEL.Value();
                   si.m_com_csr.INLNGTH = tmp_com_csr.INLNGTH.Value();
                   si.m_com_csr.OUTLNGTH = tmp_com_csr.OUTLNGTH.Value();
                   si.m_com_csr.RDSTINTMSK = tmp_com_csr.RDSTINTMSK.Value();
                   si.m_com_csr.TCINTMSK = tmp_com_csr.TCINTMSK.Value();

                   if (tmp_com_csr.RDSTINT)
                     si.m_com_csr.RDSTINT = 0;
                   if (tmp_com_csr.TCINT)
                     si.m_com_csr.TCINT = 0;

                   // be careful: run si-buffer after updating the INT flags
                   if (tmp_com_csr.TSTART)
                   {
                     if (si.m_com_csr.TSTART)
                       system.GetCoreTiming().RemoveEvent(si.m_event_type_tranfer_pending);
                     si.m_com_csr.TSTART = 1;
                     si.RunSIBuffer(0, 0);
                   }

                   if (!si.m_com_csr.TSTART)
                     si.UpdateInterrupts();
                 }));

  mmio->Register(base | SI_STATUS_REG, MMIO::DirectRead<u32>(&m_status_reg.hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& si = system.GetSerialInterface();
                   const USIStatusReg tmp_status(val);

                   // clear bits ( if (tmp.bit) SISR.bit=0 )
                   if (tmp_status.NOREP0)
                     si.m_status_reg.NOREP0 = 0;
                   if (tmp_status.COLL0)
                     si.m_status_reg.COLL0 = 0;
                   if (tmp_status.OVRUN0)
                     si.m_status_reg.OVRUN0 = 0;
                   if (tmp_status.UNRUN0)
                     si.m_status_reg.UNRUN0 = 0;

                   if (tmp_status.NOREP1)
                     si.m_status_reg.NOREP1 = 0;
                   if (tmp_status.COLL1)
                     si.m_status_reg.COLL1 = 0;
                   if (tmp_status.OVRUN1)
                     si.m_status_reg.OVRUN1 = 0;
                   if (tmp_status.UNRUN1)
                     si.m_status_reg.UNRUN1 = 0;

                   if (tmp_status.NOREP2)
                     si.m_status_reg.NOREP2 = 0;
                   if (tmp_status.COLL2)
                     si.m_status_reg.COLL2 = 0;
                   if (tmp_status.OVRUN2)
                     si.m_status_reg.OVRUN2 = 0;
                   if (tmp_status.UNRUN2)
                     si.m_status_reg.UNRUN2 = 0;

                   if (tmp_status.NOREP3)
                     si.m_status_reg.NOREP3 = 0;
                   if (tmp_status.COLL3)
                     si.m_status_reg.COLL3 = 0;
                   if (tmp_status.OVRUN3)
                     si.m_status_reg.OVRUN3 = 0;
                   if (tmp_status.UNRUN3)
                     si.m_status_reg.UNRUN3 = 0;

                   // send command to devices
                   if (tmp_status.WR)
                   {
                     si.m_channel[0].device->SendCommand(si.m_channel[0].out.hex, si.m_poll.EN0);
                     si.m_channel[1].device->SendCommand(si.m_channel[1].out.hex, si.m_poll.EN1);
                     si.m_channel[2].device->SendCommand(si.m_channel[2].out.hex, si.m_poll.EN2);
                     si.m_channel[3].device->SendCommand(si.m_channel[3].out.hex, si.m_poll.EN3);

                     si.m_status_reg.WR = 0;
                     si.m_status_reg.WRST0 = 0;
                     si.m_status_reg.WRST1 = 0;
                     si.m_status_reg.WRST2 = 0;
                     si.m_status_reg.WRST3 = 0;
                   }
                 }));

  mmio->Register(base | SI_EXI_CLOCK_COUNT, MMIO::DirectRead<u32>(&m_exi_clock_count.hex),
                 MMIO::DirectWrite<u32>(&m_exi_clock_count.hex));
}

void SerialInterfaceManager::RemoveDevice(int device_number)
{
  m_channel.at(device_number).device.reset();
}

void SerialInterfaceManager::AddDevice(std::unique_ptr<ISIDevice> device)
{
  int device_number = device->GetDeviceNumber();

  // Delete the old device
  RemoveDevice(device_number);

  // Set the new one
  m_channel.at(device_number).device = std::move(device);
}

void SerialInterfaceManager::AddDevice(const SIDevices device, int device_number)
{
  AddDevice(SIDevice_Create(m_system, device, device_number));
}

void SerialInterfaceManager::ChangeDevice(SIDevices device, int channel)
{
  // Actual device change will happen in UpdateDevices.
  m_desired_device_types[channel] = device;
}

void SerialInterfaceManager::ChangeDeviceDeterministic(SIDevices device, int channel)
{
  if (m_channel[channel].has_recent_device_change)
    return;

  if (GetDeviceType(channel) != SIDEVICE_NONE)
  {
    // Detach the current device before switching to the new one.
    device = SIDEVICE_NONE;
  }

  m_channel[channel].out.hex = 0;
  m_channel[channel].in_hi.hex = 0;
  m_channel[channel].in_lo.hex = 0;

  SetNoResponse(channel);

  AddDevice(device, channel);

  // Prevent additional device changes on this channel for one second.
  m_channel[channel].has_recent_device_change = true;
  m_system.GetCoreTiming().ScheduleEvent(SystemTimers::GetTicksPerSecond(),
                                         m_event_type_change_device, channel);
}

void SerialInterfaceManager::UpdateDevices()
{
  // Check for device change requests:
  for (int i = 0; i != MAX_SI_CHANNELS; ++i)
  {
    const SIDevices current_type = GetDeviceType(i);
    const SIDevices desired_type = m_desired_device_types[i];

    if (current_type != desired_type)
    {
      ChangeDeviceDeterministic(desired_type, i);
    }
  }

  // Hinting NetPlay that all controllers will be polled in
  // succession, in order to optimize networking
  NetPlay::SetSIPollBatching(true);

  // Update inputs at the rate of SI
  // Typically 120hz but is variable
  g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::SerialInterface);
  g_controller_interface.UpdateInput();

  // Update channels and set the status bit if there's new data
  m_status_reg.RDST0 =
      !!m_channel[0].device->GetData(m_channel[0].in_hi.hex, m_channel[0].in_lo.hex);
  m_status_reg.RDST1 =
      !!m_channel[1].device->GetData(m_channel[1].in_hi.hex, m_channel[1].in_lo.hex);
  m_status_reg.RDST2 =
      !!m_channel[2].device->GetData(m_channel[2].in_hi.hex, m_channel[2].in_lo.hex);
  m_status_reg.RDST3 =
      !!m_channel[3].device->GetData(m_channel[3].in_hi.hex, m_channel[3].in_lo.hex);

  UpdateInterrupts();

  // Polling finished
  NetPlay::SetSIPollBatching(false);
}

SIDevices SerialInterfaceManager::GetDeviceType(int channel)
{
  if (channel < 0 || channel >= MAX_SI_CHANNELS || !m_channel[channel].device)
    return SIDEVICE_NONE;

  return m_channel[channel].device->GetDeviceType();
}

u32 SerialInterfaceManager::GetPollXLines()
{
  return m_poll.X;
}

}  // namespace SerialInterface
