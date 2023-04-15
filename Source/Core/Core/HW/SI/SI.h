// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <memory>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}
namespace MMIO
{
class Mapping;
}

namespace SerialInterface
{
class ISIDevice;
enum SIDevices : int;

// SI number of channels
enum
{
  MAX_SI_CHANNELS = 0x04
};

class SerialInterfaceManager
{
public:
  explicit SerialInterfaceManager(Core::System& system);
  SerialInterfaceManager(const SerialInterfaceManager&) = delete;
  SerialInterfaceManager(SerialInterfaceManager&&) = delete;
  SerialInterfaceManager& operator=(const SerialInterfaceManager&) = delete;
  SerialInterfaceManager& operator=(SerialInterfaceManager&&) = delete;
  ~SerialInterfaceManager();

  void Init();
  void Shutdown();
  void DoState(PointerWrap& p);

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  void ScheduleEvent(int device_number, s64 cycles_into_future, u64 userdata = 0);
  void RemoveEvent(int device_number);

  void UpdateDevices();

  void RemoveDevice(int device_number);
  void AddDevice(SIDevices device, int device_number);
  void AddDevice(std::unique_ptr<ISIDevice> device);

  void ChangeDevice(SIDevices device, int channel);

  SIDevices GetDeviceType(int channel);

  u32 GetPollXLines();

private:
  // SI Interrupt Types
  enum SIInterruptType
  {
    INT_RDSTINT = 0,
    INT_TCINT = 1,
  };

  void SetNoResponse(u32 channel);
  void UpdateInterrupts();
  void GenerateSIInterrupt(SIInterruptType type);

  void ChangeDeviceDeterministic(SIDevices device, int channel);

  void RegisterEvents();
  void RunSIBuffer(u64 user_data, s64 cycles_late);
  static void GlobalRunSIBuffer(Core::System& system, u64 user_data, s64 cycles_late);
  static void ChangeDeviceCallback(Core::System& system, u64 user_data, s64 cycles_late);
  template <int device_number>
  static void DeviceEventCallback(Core::System& system, u64 userdata, s64 cyclesLate);

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
    BitField<8, 8, u32> Y;  // Polls per frame
    BitField<16, 10, u32>
        X;  // Polls per X lines. begins at vsync, min 7, max depends on video mode
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

    BitField<0, 1, u32> UNRUN3;      // (RWC) write 1: bit cleared  read 1: main proc underrun error
    BitField<1, 1, u32> OVRUN3;      // (RWC) write 1: bit cleared  read 1: overrun error
    BitField<2, 1, u32> COLL3;       // (RWC) write 1: bit cleared  read 1: collision error
    BitField<3, 1, u32> NOREP3;      // (RWC) write 1: bit cleared  read 1: response error
    BitField<4, 1, u32> WRST3;       // (R) 1: buffer channel0 not copied
    BitField<5, 1, u32> RDST3;       // (R) 1: new Data available
    BitField<6, 2, u32> reserved_1;  // 7:6
    BitField<8, 1, u32> UNRUN2;      // (RWC) write 1: bit cleared  read 1: main proc underrun error
    BitField<9, 1, u32> OVRUN2;      // (RWC) write 1: bit cleared  read 1: overrun error
    BitField<10, 1, u32> COLL2;      // (RWC) write 1: bit cleared  read 1: collision error
    BitField<11, 1, u32> NOREP2;     // (RWC) write 1: bit cleared  read 1: response error
    BitField<12, 1, u32> WRST2;      // (R) 1: buffer channel0 not copied
    BitField<13, 1, u32> RDST2;      // (R) 1: new Data available
    BitField<14, 2, u32> reserved_2;  // 15:14
    BitField<16, 1, u32> UNRUN1;  // (RWC) write 1: bit cleared  read 1: main proc underrun error
    BitField<17, 1, u32> OVRUN1;  // (RWC) write 1: bit cleared  read 1: overrun error
    BitField<18, 1, u32> COLL1;   // (RWC) write 1: bit cleared  read 1: collision error
    BitField<19, 1, u32> NOREP1;  // (RWC) write 1: bit cleared  read 1: response error
    BitField<20, 1, u32> WRST1;   // (R) 1: buffer channel0 not copied
    BitField<21, 1, u32> RDST1;   // (R) 1: new Data available
    BitField<22, 2, u32> reserved_3;  // 23:22
    BitField<24, 1, u32> UNRUN0;  // (RWC) write 1: bit cleared  read 1: main proc underrun error
    BitField<25, 1, u32> OVRUN0;  // (RWC) write 1: bit cleared  read 1: overrun error
    BitField<26, 1, u32> COLL0;   // (RWC) write 1: bit cleared  read 1: collision error
    BitField<27, 1, u32> NOREP0;  // (RWC) write 1: bit cleared  read 1: response error
    BitField<28, 1, u32> WRST0;   // (R) 1: buffer channel0 not copied
    BitField<29, 1, u32> RDST0;   // (R) 1: new Data available
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

  CoreTiming::EventType* m_event_type_change_device = nullptr;
  CoreTiming::EventType* m_event_type_tranfer_pending = nullptr;
  std::array<CoreTiming::EventType*, MAX_SI_CHANNELS> m_event_types_device{};

  // User-configured device type. possibly overridden by TAS/Netplay
  std::array<std::atomic<SIDevices>, MAX_SI_CHANNELS> m_desired_device_types{};

  std::array<SSIChannel, MAX_SI_CHANNELS> m_channel;
  USIPoll m_poll;
  USIComCSR m_com_csr;
  USIStatusReg m_status_reg;
  USIEXIClockCount m_exi_clock_count;
  std::array<u8, 128> m_si_buffer{};

  Core::System& m_system;
};
}  // namespace SerialInterface
