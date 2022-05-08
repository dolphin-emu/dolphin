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

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace SerialInterface
{
// SI Interrupt Types
enum SIInterruptType
{
  INT_RDSTINT = 0,
  INT_TCINT = 1,
};

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

// SI Channel Output
union USIChannelOut
{
  u32 hex = 0;

  BitField<0, 8, u32> OUTPUT1;
  BitField<8, 8, u32> OUTPUT0;
  BitField<16, 8, u32> CMD;
  BitField<24, 8, u32> reserved;
};

// SI Channel Input High u32
union USIChannelIn_Hi
{
  u32 hex = 0;

  BitField<0, 8, u32> INPUT3;
  BitField<8, 8, u32> INPUT2;
  BitField<16, 8, u32> INPUT1;
  BitField<24, 6, u32> INPUT0;
  BitField<30, 1, u32> ERRLATCH;  // 0: no error  1: Error latched. Check SISR.
  BitField<31, 1, u32> ERRSTAT;   // 0: no error  1: error on last transfer
};

// SI Channel Input Low u32
union USIChannelIn_Lo
{
  u32 hex = 0;

  BitField<0, 8, u32> INPUT7;
  BitField<8, 8, u32> INPUT6;
  BitField<16, 8, u32> INPUT5;
  BitField<24, 8, u32> INPUT4;
};

// SI Channel
struct SSIChannel
{
  USIChannelOut out{};
  USIChannelIn_Hi in_hi{};
  USIChannelIn_Lo in_lo{};
  std::unique_ptr<ISIDevice> device;

  bool has_recent_device_change = false;
};

// SI Poll: Controls how often a device is polled
union USIPoll
{
  u32 hex = 0;

  BitField<0, 1, u32> VBCPY3;  // 1: write to output buffer only on vblank
  BitField<1, 1, u32> VBCPY2;
  BitField<2, 1, u32> VBCPY1;
  BitField<3, 1, u32> VBCPY0;
  BitField<4, 1, u32> EN3;  // Enable polling of channel
  BitField<5, 1, u32> EN2;  //  does not affect communication RAM transfers
  BitField<6, 1, u32> EN1;
  BitField<7, 1, u32> EN0;
  BitField<8, 8, u32> Y;    // Polls per frame
  BitField<16, 10, u32> X;  // Polls per X lines. begins at vsync, min 7, max depends on video mode
  BitField<26, 6, u32> reserved;
};

// SI Communication Control Status Register
union USIComCSR
{
  u32 hex = 0;

  BitField<0, 1, u32> TSTART;   // write: start transfer  read: transfer status
  BitField<1, 2, u32> CHANNEL;  // determines which SI channel will be
                                // used on the communication interface.
  BitField<3, 3, u32> reserved_1;
  BitField<6, 1, u32> CALLBEN;  // Callback enable
  BitField<7, 1, u32> CMDEN;    // Command enable?
  BitField<8, 7, u32> INLNGTH;
  BitField<15, 1, u32> reserved_2;
  BitField<16, 7, u32> OUTLNGTH;  // Communication Channel Output Length in bytes
  BitField<23, 1, u32> reserved_3;
  BitField<24, 1, u32> CHANEN;      // Channel enable?
  BitField<25, 2, u32> CHANNUM;     // Channel number?
  BitField<27, 1, u32> RDSTINTMSK;  // Read Status Interrupt Status Mask
  BitField<28, 1, u32> RDSTINT;     // Read Status Interrupt Status
  BitField<29, 1, u32> COMERR;      // Communication Error (set 0)
  BitField<30, 1, u32> TCINTMSK;    // Transfer Complete Interrupt Mask
  BitField<31, 1, u32> TCINT;       // Transfer Complete Interrupt

  USIComCSR() = default;
  explicit USIComCSR(u32 value) : hex{value} {}
};

// SI Status Register
union USIStatusReg
{
  u32 hex = 0;

  BitField<0, 1, u32> UNRUN3;       // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BitField<1, 1, u32> OVRUN3;       // (RWC) write 1: bit cleared  read 1: overrun error
  BitField<2, 1, u32> COLL3;        // (RWC) write 1: bit cleared  read 1: collision error
  BitField<3, 1, u32> NOREP3;       // (RWC) write 1: bit cleared  read 1: response error
  BitField<4, 1, u32> WRST3;        // (R) 1: buffer channel0 not copied
  BitField<5, 1, u32> RDST3;        // (R) 1: new Data available
  BitField<6, 2, u32> reserved_1;   // 7:6
  BitField<8, 1, u32> UNRUN2;       // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BitField<9, 1, u32> OVRUN2;       // (RWC) write 1: bit cleared  read 1: overrun error
  BitField<10, 1, u32> COLL2;       // (RWC) write 1: bit cleared  read 1: collision error
  BitField<11, 1, u32> NOREP2;      // (RWC) write 1: bit cleared  read 1: response error
  BitField<12, 1, u32> WRST2;       // (R) 1: buffer channel0 not copied
  BitField<13, 1, u32> RDST2;       // (R) 1: new Data available
  BitField<14, 2, u32> reserved_2;  // 15:14
  BitField<16, 1, u32> UNRUN1;      // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BitField<17, 1, u32> OVRUN1;      // (RWC) write 1: bit cleared  read 1: overrun error
  BitField<18, 1, u32> COLL1;       // (RWC) write 1: bit cleared  read 1: collision error
  BitField<19, 1, u32> NOREP1;      // (RWC) write 1: bit cleared  read 1: response error
  BitField<20, 1, u32> WRST1;       // (R) 1: buffer channel0 not copied
  BitField<21, 1, u32> RDST1;       // (R) 1: new Data available
  BitField<22, 2, u32> reserved_3;  // 23:22
  BitField<24, 1, u32> UNRUN0;      // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BitField<25, 1, u32> OVRUN0;      // (RWC) write 1: bit cleared  read 1: overrun error
  BitField<26, 1, u32> COLL0;       // (RWC) write 1: bit cleared  read 1: collision error
  BitField<27, 1, u32> NOREP0;      // (RWC) write 1: bit cleared  read 1: response error
  BitField<28, 1, u32> WRST0;       // (R) 1: buffer channel0 not copied
  BitField<29, 1, u32> RDST0;       // (R) 1: new Data available
  BitField<30, 1, u32> reserved_4;
  BitField<31, 1, u32> WR;  // (RW) write 1 start copy, read 0 copy done

  USIStatusReg() = default;
  explicit USIStatusReg(u32 value) : hex{value} {}
};

// SI EXI Clock Count
union USIEXIClockCount
{
  u32 hex = 0;

  BitField<0, 1, u32> LOCK;  // 1: prevents CPU from setting EXI clock to 32MHz
  BitField<1, 30, u32> reserved;
};

static CoreTiming::EventType* s_change_device_event;
static CoreTiming::EventType* s_tranfer_pending_event;
static std::array<CoreTiming::EventType*, MAX_SI_CHANNELS> s_device_events;

// User-configured device type. possibly overridden by TAS/Netplay
static std::array<std::atomic<SIDevices>, MAX_SI_CHANNELS> s_desired_device_types;

// STATE_TO_SAVE
static std::array<SSIChannel, MAX_SI_CHANNELS> s_channel;
static USIPoll s_poll;
static USIComCSR s_com_csr;
static USIStatusReg s_status_reg;
static USIEXIClockCount s_exi_clock_count;
static std::array<u8, 128> s_si_buffer;

static void SetNoResponse(u32 channel)
{
  // raise the NO RESPONSE error
  switch (channel)
  {
  case 0:
    s_status_reg.NOREP0 = 1;
    break;
  case 1:
    s_status_reg.NOREP1 = 1;
    break;
  case 2:
    s_status_reg.NOREP2 = 1;
    break;
  case 3:
    s_status_reg.NOREP3 = 1;
    break;
  }
}

static void ChangeDeviceCallback(u64 user_data, s64 cycles_late)
{
  // The purpose of this callback is to simply re-enable device changes.
  s_channel[user_data].has_recent_device_change = false;
}

static void UpdateInterrupts()
{
  // check if we have to update the RDSTINT flag
  if (s_status_reg.RDST0 || s_status_reg.RDST1 || s_status_reg.RDST2 || s_status_reg.RDST3)
    s_com_csr.RDSTINT = 1;
  else
    s_com_csr.RDSTINT = 0;

  // check if we have to generate an interrupt
  const bool generate_interrupt = (s_com_csr.RDSTINT & s_com_csr.RDSTINTMSK) != 0 ||
                                  (s_com_csr.TCINT & s_com_csr.TCINTMSK) != 0;

  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_SI, generate_interrupt);
}

static void GenerateSIInterrupt(SIInterruptType type)
{
  switch (type)
  {
  case INT_RDSTINT:
    s_com_csr.RDSTINT = 1;
    break;
  case INT_TCINT:
    s_com_csr.TCINT = 1;
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

static void RunSIBuffer(u64 user_data, s64 cycles_late)
{
  if (s_com_csr.TSTART)
  {
    const s32 request_length = ConvertSILengthField(s_com_csr.OUTLNGTH);
    const s32 expected_response_length = ConvertSILengthField(s_com_csr.INLNGTH);
    const std::vector<u8> request_copy(s_si_buffer.data(), s_si_buffer.data() + request_length);

    const std::unique_ptr<ISIDevice>& device = s_channel[s_com_csr.CHANNEL].device;
    const s32 actual_response_length = device->RunBuffer(s_si_buffer.data(), request_length);

    DEBUG_LOG_FMT(SERIALINTERFACE,
                  "RunSIBuffer  chan: {}  request_length: {}  expected_response_length: {}  "
                  "actual_response_length: {}",
                  s_com_csr.CHANNEL, request_length, expected_response_length,
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
      s_com_csr.TSTART = 0;
      s_com_csr.COMERR = actual_response_length < 0;
      if (actual_response_length < 0)
        SetNoResponse(s_com_csr.CHANNEL);
      GenerateSIInterrupt(INT_TCINT);
    }
    else
    {
      CoreTiming::ScheduleEvent(device->TransferInterval() - cycles_late, s_tranfer_pending_event);
    }
  }
}

void DoState(PointerWrap& p)
{
  for (int i = 0; i < MAX_SI_CHANNELS; i++)
  {
    p.Do(s_channel[i].in_hi.hex);
    p.Do(s_channel[i].in_lo.hex);
    p.Do(s_channel[i].out.hex);
    p.Do(s_channel[i].has_recent_device_change);

    std::unique_ptr<ISIDevice>& device = s_channel[i].device;
    SIDevices type = device->GetDeviceType();
    p.Do(type);

    if (type != device->GetDeviceType())
    {
      AddDevice(SIDevice_Create(type, i));
    }

    device->DoState(p);
  }

  p.Do(s_poll);
  p.DoPOD(s_com_csr);
  p.DoPOD(s_status_reg);
  p.Do(s_exi_clock_count);
  p.Do(s_si_buffer);
}

template <int device_number>
static void DeviceEventCallback(u64 userdata, s64 cyclesLate)
{
  s_channel[device_number].device->OnEvent(userdata, cyclesLate);
}

static void RegisterEvents()
{
  s_change_device_event = CoreTiming::RegisterEvent("ChangeSIDevice", ChangeDeviceCallback);
  s_tranfer_pending_event = CoreTiming::RegisterEvent("SITransferPending", RunSIBuffer);

  constexpr std::array<CoreTiming::TimedCallback, MAX_SI_CHANNELS> event_callbacks = {
      DeviceEventCallback<0>,
      DeviceEventCallback<1>,
      DeviceEventCallback<2>,
      DeviceEventCallback<3>,
  };
  for (int i = 0; i < MAX_SI_CHANNELS; ++i)
  {
    s_device_events[i] =
        CoreTiming::RegisterEvent(fmt::format("SIEventChannel{}", i), event_callbacks[i]);
  }
}

void ScheduleEvent(int device_number, s64 cycles_into_future, u64 userdata)
{
  CoreTiming::ScheduleEvent(cycles_into_future, s_device_events[device_number], userdata);
}

void RemoveEvent(int device_number)
{
  CoreTiming::RemoveEvent(s_device_events[device_number]);
}

void Init()
{
  RegisterEvents();

  for (int i = 0; i < MAX_SI_CHANNELS; i++)
  {
    s_channel[i].out.hex = 0;
    s_channel[i].in_hi.hex = 0;
    s_channel[i].in_lo.hex = 0;
    s_channel[i].has_recent_device_change = false;

    if (Movie::IsMovieActive())
    {
      s_desired_device_types[i] = SIDEVICE_NONE;

      if (Movie::IsUsingGBA(i))
      {
        s_desired_device_types[i] = SIDEVICE_GC_GBA_EMULATED;
      }
      else if (Movie::IsUsingPad(i))
      {
        SIDevices current = Config::Get(Config::GetInfoForSIDevice(i));
        // GC pad-compatible devices can be used for both playing and recording
        if (Movie::IsUsingBongo(i))
          s_desired_device_types[i] = SIDEVICE_GC_TARUKONGA;
        else if (SIDevice_IsGCController(current))
          s_desired_device_types[i] = current;
        else
          s_desired_device_types[i] = SIDEVICE_GC_CONTROLLER;
      }
    }
    else if (!NetPlay::IsNetPlayRunning())
    {
      s_desired_device_types[i] = Config::Get(Config::GetInfoForSIDevice(i));
    }

    AddDevice(s_desired_device_types[i], i);
  }

  s_poll.hex = 0;
  s_poll.X = 492;

  s_com_csr.hex = 0;

  s_status_reg.hex = 0;

  s_exi_clock_count.hex = 0;

  // Supposedly set on reset, but logs from real Wii don't look like it is...
  // s_exi_clock_count.LOCK = 1;

  s_si_buffer = {};
}

void Shutdown()
{
  for (int i = 0; i < MAX_SI_CHANNELS; i++)
    RemoveDevice(i);
  GBAConnectionWaiter_Shutdown();
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Register SI buffer direct accesses.
  const u32 io_buffer_base = base | SI_IO_BUFFER;
  for (size_t i = 0; i < s_si_buffer.size(); i += sizeof(u32))
  {
    const u32 address = base | static_cast<u32>(io_buffer_base + i);

    mmio->Register(address, MMIO::ComplexRead<u32>([i](u32) {
                     u32 val;
                     std::memcpy(&val, &s_si_buffer[i], sizeof(val));
                     return Common::swap32(val);
                   }),
                   MMIO::ComplexWrite<u32>([i](u32, u32 val) {
                     val = Common::swap32(val);
                     std::memcpy(&s_si_buffer[i], &val, sizeof(val));
                   }));
  }
  for (size_t i = 0; i < s_si_buffer.size(); i += sizeof(u16))
  {
    const u32 address = base | static_cast<u32>(io_buffer_base + i);

    mmio->Register(address, MMIO::ComplexRead<u16>([i](u32) {
                     u16 val;
                     std::memcpy(&val, &s_si_buffer[i], sizeof(val));
                     return Common::swap16(val);
                   }),
                   MMIO::ComplexWrite<u16>([i](u32, u16 val) {
                     val = Common::swap16(val);
                     std::memcpy(&s_si_buffer[i], &val, sizeof(val));
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
                   MMIO::DirectRead<u32>(&s_channel[i].out.hex),
                   MMIO::DirectWrite<u32>(&s_channel[i].out.hex));
    mmio->Register(base | (SI_CHANNEL_0_IN_HI + 0xC * i),
                   MMIO::ComplexRead<u32>([i, rdst_bit](u32) {
                     s_status_reg.hex &= ~(1U << rdst_bit);
                     UpdateInterrupts();
                     return s_channel[i].in_hi.hex;
                   }),
                   MMIO::DirectWrite<u32>(&s_channel[i].in_hi.hex));
    mmio->Register(base | (SI_CHANNEL_0_IN_LO + 0xC * i),
                   MMIO::ComplexRead<u32>([i, rdst_bit](u32) {
                     s_status_reg.hex &= ~(1U << rdst_bit);
                     UpdateInterrupts();
                     return s_channel[i].in_lo.hex;
                   }),
                   MMIO::DirectWrite<u32>(&s_channel[i].in_lo.hex));
  }

  mmio->Register(base | SI_POLL, MMIO::DirectRead<u32>(&s_poll.hex),
                 MMIO::DirectWrite<u32>(&s_poll.hex));

  mmio->Register(base | SI_COM_CSR, MMIO::DirectRead<u32>(&s_com_csr.hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   const USIComCSR tmp_com_csr(val);

                   s_com_csr.CHANNEL = tmp_com_csr.CHANNEL.Value();
                   s_com_csr.INLNGTH = tmp_com_csr.INLNGTH.Value();
                   s_com_csr.OUTLNGTH = tmp_com_csr.OUTLNGTH.Value();
                   s_com_csr.RDSTINTMSK = tmp_com_csr.RDSTINTMSK.Value();
                   s_com_csr.TCINTMSK = tmp_com_csr.TCINTMSK.Value();

                   if (tmp_com_csr.RDSTINT)
                     s_com_csr.RDSTINT = 0;
                   if (tmp_com_csr.TCINT)
                     s_com_csr.TCINT = 0;

                   // be careful: run si-buffer after updating the INT flags
                   if (tmp_com_csr.TSTART)
                   {
                     if (s_com_csr.TSTART)
                       CoreTiming::RemoveEvent(s_tranfer_pending_event);
                     s_com_csr.TSTART = 1;
                     RunSIBuffer(0, 0);
                   }

                   if (!s_com_csr.TSTART)
                     UpdateInterrupts();
                 }));

  mmio->Register(base | SI_STATUS_REG, MMIO::DirectRead<u32>(&s_status_reg.hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   const USIStatusReg tmp_status(val);

                   // clear bits ( if (tmp.bit) SISR.bit=0 )
                   if (tmp_status.NOREP0)
                     s_status_reg.NOREP0 = 0;
                   if (tmp_status.COLL0)
                     s_status_reg.COLL0 = 0;
                   if (tmp_status.OVRUN0)
                     s_status_reg.OVRUN0 = 0;
                   if (tmp_status.UNRUN0)
                     s_status_reg.UNRUN0 = 0;

                   if (tmp_status.NOREP1)
                     s_status_reg.NOREP1 = 0;
                   if (tmp_status.COLL1)
                     s_status_reg.COLL1 = 0;
                   if (tmp_status.OVRUN1)
                     s_status_reg.OVRUN1 = 0;
                   if (tmp_status.UNRUN1)
                     s_status_reg.UNRUN1 = 0;

                   if (tmp_status.NOREP2)
                     s_status_reg.NOREP2 = 0;
                   if (tmp_status.COLL2)
                     s_status_reg.COLL2 = 0;
                   if (tmp_status.OVRUN2)
                     s_status_reg.OVRUN2 = 0;
                   if (tmp_status.UNRUN2)
                     s_status_reg.UNRUN2 = 0;

                   if (tmp_status.NOREP3)
                     s_status_reg.NOREP3 = 0;
                   if (tmp_status.COLL3)
                     s_status_reg.COLL3 = 0;
                   if (tmp_status.OVRUN3)
                     s_status_reg.OVRUN3 = 0;
                   if (tmp_status.UNRUN3)
                     s_status_reg.UNRUN3 = 0;

                   // send command to devices
                   if (tmp_status.WR)
                   {
                     s_channel[0].device->SendCommand(s_channel[0].out.hex, s_poll.EN0);
                     s_channel[1].device->SendCommand(s_channel[1].out.hex, s_poll.EN1);
                     s_channel[2].device->SendCommand(s_channel[2].out.hex, s_poll.EN2);
                     s_channel[3].device->SendCommand(s_channel[3].out.hex, s_poll.EN3);

                     s_status_reg.WR = 0;
                     s_status_reg.WRST0 = 0;
                     s_status_reg.WRST1 = 0;
                     s_status_reg.WRST2 = 0;
                     s_status_reg.WRST3 = 0;
                   }
                 }));

  mmio->Register(base | SI_EXI_CLOCK_COUNT, MMIO::DirectRead<u32>(&s_exi_clock_count.hex),
                 MMIO::DirectWrite<u32>(&s_exi_clock_count.hex));
}

void RemoveDevice(int device_number)
{
  s_channel.at(device_number).device.reset();
}

void AddDevice(std::unique_ptr<ISIDevice> device)
{
  int device_number = device->GetDeviceNumber();

  // Delete the old device
  RemoveDevice(device_number);

  // Set the new one
  s_channel.at(device_number).device = std::move(device);
}

void AddDevice(const SIDevices device, int device_number)
{
  AddDevice(SIDevice_Create(device, device_number));
}

void ChangeDevice(SIDevices device, int channel)
{
  // Actual device change will happen in UpdateDevices.
  s_desired_device_types[channel] = device;
}

static void ChangeDeviceDeterministic(SIDevices device, int channel)
{
  if (s_channel[channel].has_recent_device_change)
    return;

  if (GetDeviceType(channel) != SIDEVICE_NONE)
  {
    // Detach the current device before switching to the new one.
    device = SIDEVICE_NONE;
  }

  s_channel[channel].out.hex = 0;
  s_channel[channel].in_hi.hex = 0;
  s_channel[channel].in_lo.hex = 0;

  SetNoResponse(channel);

  AddDevice(device, channel);

  // Prevent additional device changes on this channel for one second.
  s_channel[channel].has_recent_device_change = true;
  CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond(), s_change_device_event, channel);
}

void UpdateDevices()
{
  // Check for device change requests:
  for (int i = 0; i != MAX_SI_CHANNELS; ++i)
  {
    const SIDevices current_type = GetDeviceType(i);
    const SIDevices desired_type = s_desired_device_types[i];

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
  s_status_reg.RDST0 =
      !!s_channel[0].device->GetData(s_channel[0].in_hi.hex, s_channel[0].in_lo.hex);
  s_status_reg.RDST1 =
      !!s_channel[1].device->GetData(s_channel[1].in_hi.hex, s_channel[1].in_lo.hex);
  s_status_reg.RDST2 =
      !!s_channel[2].device->GetData(s_channel[2].in_hi.hex, s_channel[2].in_lo.hex);
  s_status_reg.RDST3 =
      !!s_channel[3].device->GetData(s_channel[3].in_hi.hex, s_channel[3].in_lo.hex);

  UpdateInterrupts();

  // Polling finished
  NetPlay::SetSIPollBatching(false);
}

SIDevices GetDeviceType(int channel)
{
  if (channel < 0 || channel >= MAX_SI_CHANNELS || !s_channel[channel].device)
    return SIDEVICE_NONE;

  return s_channel[channel].device->GetDeviceType();
}

u32 GetPollXLines()
{
  return s_poll.X;
}

}  // namespace SerialInterface
