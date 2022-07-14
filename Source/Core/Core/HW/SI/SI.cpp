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

#include "Common/BitFieldView.h"
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
struct SIChannelOut
{
  u32 hex = 0;

  BFVIEW(u8, 8, 0, OUTPUT1)
  BFVIEW(u8, 8, 8, OUTPUT0)
  BFVIEW(u8, 8, 16, CMD)
  BFVIEW(u8, 8, 24, reserved)
};

// SI Channel Input High u32
struct SIChannelIn_Hi
{
  u32 hex = 0;

  BFVIEW(u8, 8, 0, INPUT3)
  BFVIEW(u8, 8, 8, INPUT2)
  BFVIEW(u8, 8, 16, INPUT1)
  BFVIEW(u8, 6, 24, INPUT0)
  BFVIEW(bool, 1, 30, ERRLATCH)  // 0: no error  1: Error latched. Check SISR.
  BFVIEW(bool, 1, 31, ERRSTAT)   // 0: no error  1: error on last transfer
};

// SI Channel Input Low u32
struct SIChannelIn_Lo
{
  u32 hex = 0;

  BFVIEW(u8, 8, 0, INPUT7)
  BFVIEW(u8, 8, 8, INPUT6)
  BFVIEW(u8, 8, 16, INPUT5)
  BFVIEW(u8, 8, 24, INPUT4)
};

// SI Channel
struct SSIChannel
{
  SIChannelOut out{};
  SIChannelIn_Hi in_hi{};
  SIChannelIn_Lo in_lo{};
  std::unique_ptr<ISIDevice> device;

  bool has_recent_device_change = false;
};

// SI Poll: Controls how often a device is polled
struct SIPoll
{
  u32 hex = 0;

  BFVIEW(bool, 1, 0, VBCPY3)  // 1: write to output buffer only on vblank
  BFVIEW(bool, 1, 1, VBCPY2)
  BFVIEW(bool, 1, 2, VBCPY1)
  BFVIEW(bool, 1, 3, VBCPY0)
  BFVIEW(bool, 1, 4, EN3)  // Enable polling of channel
  BFVIEW(bool, 1, 5, EN2)  //  does not affect communication RAM transfers
  BFVIEW(bool, 1, 6, EN1)
  BFVIEW(bool, 1, 7, EN0)
  BFVIEW(u32, 8, 8, Y)    // Polls per frame
  BFVIEW(u32, 10, 16, X)  // Polls per X lines. begins at vsync, min 7, max depends on video mode
  BFVIEW(u32, 6, 26, reserved)
};

// SI Communication Control Status Register
struct SIComCSR
{
  u32 hex = 0;

  BFVIEW(bool, 1, 0, TSTART)  // write: start transfer  read: transfer status
  BFVIEW(u32, 2, 1, CHANNEL)  // determines which SI channel will be
                              // used on the communication interface.
  BFVIEW(u32, 3, 3, reserved_1)
  BFVIEW(bool, 1, 6, CALLBEN)  // Callback enable
  BFVIEW(bool, 1, 7, CMDEN)    // Command enable?
  BFVIEW(u32, 7, 8, INLNGTH)
  BFVIEW(bool, 1, 15, reserved_2)
  BFVIEW(u32, 7, 16, OUTLNGTH)  // Communication Channel Output Length in bytes
  BFVIEW(bool, 1, 23, reserved_3)
  BFVIEW(bool, 1, 24, CHANEN)      // Channel enable?
  BFVIEW(u32, 2, 25, CHANNUM)      // Channel number?
  BFVIEW(bool, 1, 27, RDSTINTMSK)  // Read Status Interrupt Status Mask
  BFVIEW(bool, 1, 28, RDSTINT)     // Read Status Interrupt Status
  BFVIEW(bool, 1, 29, COMERR)      // Communication Error (set 0)
  BFVIEW(bool, 1, 30, TCINTMSK)    // Transfer Complete Interrupt Mask
  BFVIEW(bool, 1, 31, TCINT)       // Transfer Complete Interrupt

  SIComCSR() = default;
  explicit SIComCSR(u32 value) : hex{value} {}
};

// SI Status Register
struct SIStatusReg
{
  u32 hex = 0;

  BFVIEW(bool, 1, 0, UNRUN3)      // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BFVIEW(bool, 1, 1, OVRUN3)      // (RWC) write 1: bit cleared  read 1: overrun error
  BFVIEW(bool, 1, 2, COLL3)       // (RWC) write 1: bit cleared  read 1: collision error
  BFVIEW(bool, 1, 3, NOREP3)      // (RWC) write 1: bit cleared  read 1: response error
  BFVIEW(bool, 1, 4, WRST3)       // (R) 1: buffer channel0 not copied
  BFVIEW(bool, 1, 5, RDST3)       // (R) 1: new Data available
  BFVIEW(u32, 2, 6, reserved_1)   // 7:6
  BFVIEW(bool, 1, 8, UNRUN2)      // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BFVIEW(bool, 1, 9, OVRUN2)      // (RWC) write 1: bit cleared  read 1: overrun error
  BFVIEW(bool, 1, 10, COLL2)      // (RWC) write 1: bit cleared  read 1: collision error
  BFVIEW(bool, 1, 11, NOREP2)     // (RWC) write 1: bit cleared  read 1: response error
  BFVIEW(bool, 1, 12, WRST2)      // (R) 1: buffer channel0 not copied
  BFVIEW(bool, 1, 13, RDST2)      // (R) 1: new Data available
  BFVIEW(u32, 2, 14, reserved_2)  // 15:14
  BFVIEW(bool, 1, 16, UNRUN1)     // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BFVIEW(bool, 1, 17, OVRUN1)     // (RWC) write 1: bit cleared  read 1: overrun error
  BFVIEW(bool, 1, 18, COLL1)      // (RWC) write 1: bit cleared  read 1: collision error
  BFVIEW(bool, 1, 19, NOREP1)     // (RWC) write 1: bit cleared  read 1: response error
  BFVIEW(bool, 1, 20, WRST1)      // (R) 1: buffer channel0 not copied
  BFVIEW(bool, 1, 21, RDST1)      // (R) 1: new Data available
  BFVIEW(u32, 2, 22, reserved_3)  // 23:22
  BFVIEW(bool, 1, 24, UNRUN0)     // (RWC) write 1: bit cleared  read 1: main proc underrun error
  BFVIEW(bool, 1, 25, OVRUN0)     // (RWC) write 1: bit cleared  read 1: overrun error
  BFVIEW(bool, 1, 26, COLL0)      // (RWC) write 1: bit cleared  read 1: collision error
  BFVIEW(bool, 1, 27, NOREP0)     // (RWC) write 1: bit cleared  read 1: response error
  BFVIEW(bool, 1, 28, WRST0)      // (R) 1: buffer channel0 not copied
  BFVIEW(bool, 1, 29, RDST0)      // (R) 1: new Data available
  BFVIEW(u32, 1, 30, reserved_4)
  BFVIEW(bool, 1, 31, WR)  // (RW) write 1 start copy, read 0 copy done

  SIStatusReg() = default;
  explicit SIStatusReg(u32 value) : hex{value} {}
};

// SI EXI Clock Count
struct SIEXIClockCount
{
  u32 hex = 0;

  BFVIEW(bool, 1, 0, LOCK)  // 1: prevents CPU from setting EXI clock to 32MHz
  BFVIEW(u32, 30, 1, reserved)
};

struct SerialInterfaceState::Data
{
  CoreTiming::EventType* event_type_change_device;
  CoreTiming::EventType* event_type_tranfer_pending;
  std::array<CoreTiming::EventType*, MAX_SI_CHANNELS> event_types_device;

  // User-configured device type. possibly overridden by TAS/Netplay
  std::array<std::atomic<SIDevices>, MAX_SI_CHANNELS> desired_device_types;

  std::array<SSIChannel, MAX_SI_CHANNELS> channel;
  SIPoll poll;
  SIComCSR com_csr;
  SIStatusReg status_reg;
  SIEXIClockCount exi_clock_count;
  std::array<u8, 128> si_buffer;
};

SerialInterfaceState::SerialInterfaceState() : m_data(std::make_unique<Data>())
{
}

SerialInterfaceState::~SerialInterfaceState() = default;

static void SetNoResponse(u32 channel)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  // raise the NO RESPONSE error
  switch (channel)
  {
  case 0:
    state.status_reg.NOREP0() = true;
    break;
  case 1:
    state.status_reg.NOREP1() = true;
    break;
  case 2:
    state.status_reg.NOREP2() = true;
    break;
  case 3:
    state.status_reg.NOREP3() = true;
    break;
  }
}

static void ChangeDeviceCallback(u64 user_data, s64 cycles_late)
{
  // The purpose of this callback is to simply re-enable device changes.
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  state.channel[user_data].has_recent_device_change = false;
}

static void UpdateInterrupts()
{
  // check if we have to update the RDSTINT flag
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  if (state.status_reg.RDST0() || state.status_reg.RDST1() || state.status_reg.RDST2() ||
      state.status_reg.RDST3())
  {
    state.com_csr.RDSTINT() = true;
  }
  else
  {
    state.com_csr.RDSTINT() = false;
  }

  // check if we have to generate an interrupt
  const bool generate_interrupt = (state.com_csr.RDSTINT() && state.com_csr.RDSTINTMSK()) ||
                                  (state.com_csr.TCINT() & state.com_csr.TCINTMSK());

  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_SI, generate_interrupt);
}

static void GenerateSIInterrupt(SIInterruptType type)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  switch (type)
  {
  case INT_RDSTINT:
    state.com_csr.RDSTINT() = true;
    break;
  case INT_TCINT:
    state.com_csr.TCINT() = true;
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
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  if (state.com_csr.TSTART())
  {
    const s32 request_length = ConvertSILengthField(state.com_csr.OUTLNGTH());
    const s32 expected_response_length = ConvertSILengthField(state.com_csr.INLNGTH());
    const std::vector<u8> request_copy(state.si_buffer.data(),
                                       state.si_buffer.data() + request_length);

    const std::unique_ptr<ISIDevice>& device = state.channel[state.com_csr.CHANNEL()].device;
    const s32 actual_response_length = device->RunBuffer(state.si_buffer.data(), request_length);

    DEBUG_LOG_FMT(SERIALINTERFACE,
                  "RunSIBuffer  chan: {}  request_length: {}  expected_response_length: {}  "
                  "actual_response_length: {}",
                  state.com_csr.CHANNEL(), request_length, expected_response_length,
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
      state.com_csr.TSTART() = 0;
      state.com_csr.COMERR() = actual_response_length < 0;
      if (actual_response_length < 0)
        SetNoResponse(state.com_csr.CHANNEL());
      GenerateSIInterrupt(INT_TCINT);
    }
    else
    {
      CoreTiming::ScheduleEvent(device->TransferInterval() - cycles_late,
                                state.event_type_tranfer_pending);
    }
  }
}

void DoState(PointerWrap& p)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  for (int i = 0; i < MAX_SI_CHANNELS; i++)
  {
    p.Do(state.channel[i].in_hi.hex);
    p.Do(state.channel[i].in_lo.hex);
    p.Do(state.channel[i].out.hex);
    p.Do(state.channel[i].has_recent_device_change);

    std::unique_ptr<ISIDevice>& device = state.channel[i].device;
    SIDevices type = device->GetDeviceType();
    p.Do(type);

    if (type != device->GetDeviceType())
    {
      AddDevice(SIDevice_Create(type, i));
    }

    device->DoState(p);
  }

  p.Do(state.poll);
  p.Do(state.com_csr);
  p.Do(state.status_reg);
  p.Do(state.exi_clock_count);
  p.Do(state.si_buffer);
}

template <int device_number>
static void DeviceEventCallback(u64 userdata, s64 cyclesLate)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  state.channel[device_number].device->OnEvent(userdata, cyclesLate);
}

static void RegisterEvents()
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  state.event_type_change_device =
      CoreTiming::RegisterEvent("ChangeSIDevice", ChangeDeviceCallback);
  state.event_type_tranfer_pending = CoreTiming::RegisterEvent("SITransferPending", RunSIBuffer);

  constexpr std::array<CoreTiming::TimedCallback, MAX_SI_CHANNELS> event_callbacks = {
      DeviceEventCallback<0>,
      DeviceEventCallback<1>,
      DeviceEventCallback<2>,
      DeviceEventCallback<3>,
  };
  for (int i = 0; i < MAX_SI_CHANNELS; ++i)
  {
    state.event_types_device[i] =
        CoreTiming::RegisterEvent(fmt::format("SIEventChannel{}", i), event_callbacks[i]);
  }
}

void ScheduleEvent(int device_number, s64 cycles_into_future, u64 userdata)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  CoreTiming::ScheduleEvent(cycles_into_future, state.event_types_device[device_number], userdata);
}

void RemoveEvent(int device_number)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  CoreTiming::RemoveEvent(state.event_types_device[device_number]);
}

void Init()
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  RegisterEvents();

  for (int i = 0; i < MAX_SI_CHANNELS; i++)
  {
    state.channel[i].out.hex = 0;
    state.channel[i].in_hi.hex = 0;
    state.channel[i].in_lo.hex = 0;
    state.channel[i].has_recent_device_change = false;

    if (Movie::IsMovieActive())
    {
      state.desired_device_types[i] = SIDEVICE_NONE;

      if (Movie::IsUsingGBA(i))
      {
        state.desired_device_types[i] = SIDEVICE_GC_GBA_EMULATED;
      }
      else if (Movie::IsUsingPad(i))
      {
        SIDevices current = Config::Get(Config::GetInfoForSIDevice(i));
        // GC pad-compatible devices can be used for both playing and recording
        if (Movie::IsUsingBongo(i))
          state.desired_device_types[i] = SIDEVICE_GC_TARUKONGA;
        else if (SIDevice_IsGCController(current))
          state.desired_device_types[i] = current;
        else
          state.desired_device_types[i] = SIDEVICE_GC_CONTROLLER;
      }
    }
    else if (!NetPlay::IsNetPlayRunning())
    {
      state.desired_device_types[i] = Config::Get(Config::GetInfoForSIDevice(i));
    }

    AddDevice(state.desired_device_types[i], i);
  }

  state.poll.hex = 0;
  state.poll.X() = 492;

  state.com_csr.hex = 0;

  state.status_reg.hex = 0;

  state.exi_clock_count.hex = 0;

  // Supposedly set on reset, but logs from real Wii don't look like it is...
  // state.exi_clock_count.LOCK = 1;

  state.si_buffer = {};
}

void Shutdown()
{
  for (int i = 0; i < MAX_SI_CHANNELS; i++)
    RemoveDevice(i);
  GBAConnectionWaiter_Shutdown();
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();

  // Register SI buffer direct accesses.
  const u32 io_buffer_base = base | SI_IO_BUFFER;
  for (size_t i = 0; i < state.si_buffer.size(); i += sizeof(u32))
  {
    const u32 address = base | static_cast<u32>(io_buffer_base + i);

    mmio->Register(address, MMIO::ComplexRead<u32>([i](u32) {
                     auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                     u32 val;
                     std::memcpy(&val, &state.si_buffer[i], sizeof(val));
                     return Common::swap32(val);
                   }),
                   MMIO::ComplexWrite<u32>([i](u32, u32 val) {
                     auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                     val = Common::swap32(val);
                     std::memcpy(&state.si_buffer[i], &val, sizeof(val));
                   }));
  }
  for (size_t i = 0; i < state.si_buffer.size(); i += sizeof(u16))
  {
    const u32 address = base | static_cast<u32>(io_buffer_base + i);

    mmio->Register(address, MMIO::ComplexRead<u16>([i](u32) {
                     auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                     u16 val;
                     std::memcpy(&val, &state.si_buffer[i], sizeof(val));
                     return Common::swap16(val);
                   }),
                   MMIO::ComplexWrite<u16>([i](u32, u16 val) {
                     auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                     val = Common::swap16(val);
                     std::memcpy(&state.si_buffer[i], &val, sizeof(val));
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
                   MMIO::DirectRead<u32>(&state.channel[i].out.hex),
                   MMIO::DirectWrite<u32>(&state.channel[i].out.hex));
    mmio->Register(base | (SI_CHANNEL_0_IN_HI + 0xC * i),
                   MMIO::ComplexRead<u32>([i, rdst_bit](u32) {
                     auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                     state.status_reg.hex &= ~(1U << rdst_bit);
                     UpdateInterrupts();
                     return state.channel[i].in_hi.hex;
                   }),
                   MMIO::DirectWrite<u32>(&state.channel[i].in_hi.hex));
    mmio->Register(base | (SI_CHANNEL_0_IN_LO + 0xC * i),
                   MMIO::ComplexRead<u32>([i, rdst_bit](u32) {
                     auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                     state.status_reg.hex &= ~(1U << rdst_bit);
                     UpdateInterrupts();
                     return state.channel[i].in_lo.hex;
                   }),
                   MMIO::DirectWrite<u32>(&state.channel[i].in_lo.hex));
  }

  mmio->Register(base | SI_POLL, MMIO::DirectRead<u32>(&state.poll.hex),
                 MMIO::DirectWrite<u32>(&state.poll.hex));

  mmio->Register(base | SI_COM_CSR, MMIO::DirectRead<u32>(&state.com_csr.hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
                   const SIComCSR tmp_com_csr(val);

                   state.com_csr.CHANNEL() = tmp_com_csr.CHANNEL();
                   state.com_csr.INLNGTH() = tmp_com_csr.INLNGTH();
                   state.com_csr.OUTLNGTH() = tmp_com_csr.OUTLNGTH();
                   state.com_csr.RDSTINTMSK() = tmp_com_csr.RDSTINTMSK();
                   state.com_csr.TCINTMSK() = tmp_com_csr.TCINTMSK();

                   if (tmp_com_csr.RDSTINT())
                     state.com_csr.RDSTINT() = false;
                   if (tmp_com_csr.TCINT())
                     state.com_csr.TCINT() = false;

                   // be careful: run si-buffer after updating the INT flags
                   if (tmp_com_csr.TSTART())
                   {
                     if (state.com_csr.TSTART())
                       CoreTiming::RemoveEvent(state.event_type_tranfer_pending);
                     state.com_csr.TSTART() = true;
                     RunSIBuffer(0, 0);
                   }

                   if (!state.com_csr.TSTART())
                     UpdateInterrupts();
                 }));

  mmio->Register(
      base | SI_STATUS_REG, MMIO::DirectRead<u32>(&state.status_reg.hex),
      MMIO::ComplexWrite<u32>([](u32, u32 val) {
        auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
        const SIStatusReg tmp_status(val);

        // clear bits ( if (tmp.bit) SISR.bit=0 )
        if (tmp_status.NOREP0())
          state.status_reg.NOREP0() = false;
        if (tmp_status.COLL0())
          state.status_reg.COLL0() = false;
        if (tmp_status.OVRUN0())
          state.status_reg.OVRUN0() = false;
        if (tmp_status.UNRUN0())
          state.status_reg.UNRUN0() = false;

        if (tmp_status.NOREP1())
          state.status_reg.NOREP1() = false;
        if (tmp_status.COLL1())
          state.status_reg.COLL1() = false;
        if (tmp_status.OVRUN1())
          state.status_reg.OVRUN1() = false;
        if (tmp_status.UNRUN1())
          state.status_reg.UNRUN1() = false;

        if (tmp_status.NOREP2())
          state.status_reg.NOREP2() = false;
        if (tmp_status.COLL2())
          state.status_reg.COLL2() = false;
        if (tmp_status.OVRUN2())
          state.status_reg.OVRUN2() = false;
        if (tmp_status.UNRUN2())
          state.status_reg.UNRUN2() = false;

        if (tmp_status.NOREP3())
          state.status_reg.NOREP3() = false;
        if (tmp_status.COLL3())
          state.status_reg.COLL3() = false;
        if (tmp_status.OVRUN3())
          state.status_reg.OVRUN3() = false;
        if (tmp_status.UNRUN3())
          state.status_reg.UNRUN3() = false;

        // send command to devices
        if (tmp_status.WR())
        {
          state.channel[0].device->SendCommand(state.channel[0].out.hex, state.poll.EN0());
          state.channel[1].device->SendCommand(state.channel[1].out.hex, state.poll.EN1());
          state.channel[2].device->SendCommand(state.channel[2].out.hex, state.poll.EN2());
          state.channel[3].device->SendCommand(state.channel[3].out.hex, state.poll.EN3());

          state.status_reg.WR() = false;
          state.status_reg.WRST0() = false;
          state.status_reg.WRST1() = false;
          state.status_reg.WRST2() = false;
          state.status_reg.WRST3() = false;
        }
      }));

  mmio->Register(base | SI_EXI_CLOCK_COUNT, MMIO::DirectRead<u32>(&state.exi_clock_count.hex),
                 MMIO::DirectWrite<u32>(&state.exi_clock_count.hex));
}

void RemoveDevice(int device_number)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  state.channel.at(device_number).device.reset();
}

void AddDevice(std::unique_ptr<ISIDevice> device)
{
  int device_number = device->GetDeviceNumber();

  // Delete the old device
  RemoveDevice(device_number);

  // Set the new one
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  state.channel.at(device_number).device = std::move(device);
}

void AddDevice(const SIDevices device, int device_number)
{
  AddDevice(SIDevice_Create(device, device_number));
}

void ChangeDevice(SIDevices device, int channel)
{
  // Actual device change will happen in UpdateDevices.
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  state.desired_device_types[channel] = device;
}

static void ChangeDeviceDeterministic(SIDevices device, int channel)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  if (state.channel[channel].has_recent_device_change)
    return;

  if (GetDeviceType(channel) != SIDEVICE_NONE)
  {
    // Detach the current device before switching to the new one.
    device = SIDEVICE_NONE;
  }

  state.channel[channel].out.hex = 0;
  state.channel[channel].in_hi.hex = 0;
  state.channel[channel].in_lo.hex = 0;

  SetNoResponse(channel);

  AddDevice(device, channel);

  // Prevent additional device changes on this channel for one second.
  state.channel[channel].has_recent_device_change = true;
  CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond(), state.event_type_change_device,
                            channel);
}

void UpdateDevices()
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();

  // Check for device change requests:
  for (int i = 0; i != MAX_SI_CHANNELS; ++i)
  {
    const SIDevices current_type = GetDeviceType(i);
    const SIDevices desired_type = state.desired_device_types[i];

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
  state.status_reg.RDST0() =
      state.channel[0].device->GetData(state.channel[0].in_hi.hex, state.channel[0].in_lo.hex);
  state.status_reg.RDST1() =
      state.channel[1].device->GetData(state.channel[1].in_hi.hex, state.channel[1].in_lo.hex);
  state.status_reg.RDST2() =
      state.channel[2].device->GetData(state.channel[2].in_hi.hex, state.channel[2].in_lo.hex);
  state.status_reg.RDST3() =
      state.channel[3].device->GetData(state.channel[3].in_hi.hex, state.channel[3].in_lo.hex);

  UpdateInterrupts();

  // Polling finished
  NetPlay::SetSIPollBatching(false);
}

SIDevices GetDeviceType(int channel)
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  if (channel < 0 || channel >= MAX_SI_CHANNELS || !state.channel[channel].device)
    return SIDEVICE_NONE;

  return state.channel[channel].device->GetDeviceType();
}

u32 GetPollXLines()
{
  auto& state = Core::System::GetInstance().GetSerialInterfaceState().GetData();
  return state.poll.X();
}

}  // namespace SerialInterface
