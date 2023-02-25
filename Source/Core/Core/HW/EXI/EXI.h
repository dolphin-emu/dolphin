// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <initializer_list>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Core/CoreTiming.h"

class PointerWrap;
struct Sram;

namespace CoreTiming
{
enum class FromThread;
}
namespace MMIO
{
class Mapping;
}

namespace ExpansionInterface
{
class ExpansionInterfaceState
{
public:
  ExpansionInterfaceState();
  ExpansionInterfaceState(const ExpansionInterfaceState&) = delete;
  ExpansionInterfaceState(ExpansionInterfaceState&&) = delete;
  ExpansionInterfaceState& operator=(const ExpansionInterfaceState&) = delete;
  ExpansionInterfaceState& operator=(ExpansionInterfaceState&&) = delete;
  ~ExpansionInterfaceState();

  struct Data;
  Data& GetData() { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

class CEXIChannel;
class IEXIDevice;
enum class EXIDeviceType : int;

enum
{
  MAX_EXI_CHANNELS = 3
};

enum class Slot : int
{
  A,
  B,
  SP1,
};
// Note: using auto here results in a false warning on GCC
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80351
constexpr std::initializer_list<Slot> SLOTS = {Slot::A, Slot::B, Slot::SP1};
constexpr auto MAX_SLOT = Slot::SP1;
constexpr std::initializer_list<Slot> MEMCARD_SLOTS = {Slot::A, Slot::B};
constexpr auto MAX_MEMCARD_SLOT = Slot::B;
constexpr bool IsMemcardSlot(Slot slot)
{
  return slot == Slot::A || slot == Slot::B;
}

u8 SlotToEXIChannel(Slot slot);
u8 SlotToEXIDevice(Slot slot);

void Init(const Sram* override_sram);
void Shutdown();
void DoState(PointerWrap& p);
void PauseAndLock(bool doLock, bool unpauseOnUnlock);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void UpdateInterrupts();
void ScheduleUpdateInterrupts(CoreTiming::FromThread from, int cycles_late);

void ChangeDevice(Slot slot, EXIDeviceType device_type,
                  CoreTiming::FromThread from_thread = CoreTiming::FromThread::NON_CPU);
void ChangeDevice(u8 channel, u8 device_num, EXIDeviceType device_type,
                  CoreTiming::FromThread from_thread = CoreTiming::FromThread::NON_CPU);

CEXIChannel* GetChannel(u32 index);
IEXIDevice* GetDevice(Slot slot);

}  // namespace ExpansionInterface

template <>
struct fmt::formatter<ExpansionInterface::Slot> : EnumFormatter<ExpansionInterface::MAX_SLOT>
{
  constexpr formatter() : EnumFormatter({"Slot A", "Slot B", "Serial Port 1"}) {}
};
