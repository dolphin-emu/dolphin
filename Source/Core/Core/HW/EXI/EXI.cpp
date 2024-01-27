// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI.h"

#include <array>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI_Channel.h"
#include "Core/HW/EXI/EXI_DeviceMemoryCard.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/Sram.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "DiscIO/Enums.h"

namespace ExpansionInterface
{
ExpansionInterfaceManager::ExpansionInterfaceManager(Core::System& system) : m_system(system)
{
}

ExpansionInterfaceManager::~ExpansionInterfaceManager() = default;

void ExpansionInterfaceManager::AddMemoryCard(Slot slot)
{
  EXIDeviceType memorycard_device;

  auto& movie = m_system.GetMovie();
  if (movie.IsPlayingInput() && movie.IsConfigSaved())
  {
    if (movie.IsUsingMemcard(slot))
    {
      memorycard_device = Config::Get(Config::GetInfoForEXIDevice(slot));
      if (memorycard_device != EXIDeviceType::MemoryCardFolder &&
          memorycard_device != EXIDeviceType::MemoryCard)
      {
        PanicAlertFmtT(
            "The movie indicates that a memory card should be inserted into {0:n}, but one is not "
            "currently inserted (instead, {1} is inserted).  For the movie to sync properly, "
            "please change the selected device to Memory Card or GCI Folder.",
            slot, Common::GetStringT(fmt::format("{:n}", memorycard_device).c_str()));
      }
    }
    else
    {
      memorycard_device = EXIDeviceType::None;
    }
  }
  else
  {
    memorycard_device = Config::Get(Config::GetInfoForEXIDevice(slot));
  }

  m_channels[SlotToEXIChannel(slot)]->AddDevice(memorycard_device, SlotToEXIDevice(slot));
}

u8 SlotToEXIChannel(Slot slot)
{
  switch (slot)
  {
  case Slot::A:
    return 0;
  case Slot::B:
    return 1;
  case Slot::SP1:
    return 0;
  default:
    PanicAlertFmt("Unhandled slot {}", slot);
    return 0;
  }
}

u8 SlotToEXIDevice(Slot slot)
{
  switch (slot)
  {
  case Slot::A:
    return 0;
  case Slot::B:
    return 0;
  case Slot::SP1:
    return 2;
  default:
    PanicAlertFmt("Unhandled slot {}", slot);
    return 0;
  }
}

void ExpansionInterfaceManager::Init(const Sram* override_sram)
{
  auto& sram = m_system.GetSRAM();
  if (override_sram)
  {
    sram = *override_sram;
    m_using_overridden_sram = true;
  }
  else
  {
    InitSRAM(&sram, SConfig::GetInstance().m_strSRAM);
    m_using_overridden_sram = false;
  }

  auto& core_timing = m_system.GetCoreTiming();
  CEXIMemoryCard::Init(core_timing);

  {
    u16 size_mbits = Memcard::MBIT_SIZE_MEMORY_CARD_2043;
    int size_override = Config::Get(Config::MAIN_MEMORY_CARD_SIZE);
    if (size_override >= 0 && size_override <= 4)
      size_mbits = Memcard::MBIT_SIZE_MEMORY_CARD_59 << size_override;
    const bool shift_jis =
        Config::ToGameCubeRegion(SConfig::GetInstance().m_region) == DiscIO::Region::NTSC_J;
    const CardFlashId& flash_id = sram.settings_ex.flash_id[Memcard::SLOT_A];
    const u32 rtc_bias = sram.settings.rtc_bias;
    const u32 sram_language = static_cast<u32>(sram.settings.language);
    const u64 format_time =
        Common::Timer::GetLocalTimeSinceJan1970() - ExpansionInterface::CEXIIPL::GC_EPOCH;

    for (u32 i = 0; i < MAX_EXI_CHANNELS; i++)
    {
      Memcard::HeaderData header_data;
      Memcard::InitializeHeaderData(&header_data, flash_id, size_mbits, shift_jis, rtc_bias,
                                    sram_language, format_time + i);
      m_channels[i] = std::make_unique<CEXIChannel>(m_system, i, header_data);
    }
  }

  for (Slot slot : MEMCARD_SLOTS)
    AddMemoryCard(slot);

  m_channels[0]->AddDevice(EXIDeviceType::MaskROM, 1);
  m_channels[SlotToEXIChannel(Slot::SP1)]->AddDevice(Config::Get(Config::MAIN_SERIAL_PORT_1),
                                                     SlotToEXIDevice(Slot::SP1));
  m_channels[2]->AddDevice(EXIDeviceType::AD16, 0);

  m_event_type_change_device = core_timing.RegisterEvent("ChangeEXIDevice", ChangeDeviceCallback);
  m_event_type_update_interrupts =
      core_timing.RegisterEvent("EXIUpdateInterrupts", UpdateInterruptsCallback);
}

void ExpansionInterfaceManager::Shutdown()
{
  for (auto& channel : m_channels)
    channel.reset();

  CEXIMemoryCard::Shutdown();

  if (!m_using_overridden_sram)
  {
    File::IOFile file(SConfig::GetInstance().m_strSRAM, "wb");
    auto& sram = m_system.GetSRAM();
    file.WriteArray(&sram, 1);
  }
}

void ExpansionInterfaceManager::DoState(PointerWrap& p)
{
  for (auto& channel : m_channels)
    channel->DoState(p);
}

void ExpansionInterfaceManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  for (int i = 0; i < MAX_EXI_CHANNELS; ++i)
  {
    DEBUG_ASSERT(m_channels[i] != nullptr);
    // Each channel has 5 32 bit registers assigned to it. We offset the
    // base that we give to each channel for registration.
    //
    // Be careful: this means the base is no longer aligned on a page
    // boundary and using "base | FOO" is not valid!
    m_channels[i]->RegisterMMIO(mmio, base + 5 * 4 * i);
  }
}

void ExpansionInterfaceManager::ChangeDeviceCallback(Core::System& system, u64 userdata,
                                                     s64 cycles_late)
{
  u8 channel = (u8)(userdata >> 32);
  u8 type = (u8)(userdata >> 16);
  u8 num = (u8)userdata;

  system.GetExpansionInterface().m_channels.at(channel)->AddDevice(static_cast<EXIDeviceType>(type),
                                                                   num);
}

void ExpansionInterfaceManager::ChangeDevice(Slot slot, EXIDeviceType device_type,
                                             CoreTiming::FromThread from_thread)
{
  ChangeDevice(SlotToEXIChannel(slot), SlotToEXIDevice(slot), device_type, from_thread);
}

void ExpansionInterfaceManager::ChangeDevice(u8 channel, u8 device_num, EXIDeviceType device_type,
                                             CoreTiming::FromThread from_thread)
{
  // Let the hardware see no device for 1 second
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.ScheduleEvent(0, m_event_type_change_device,
                            ((u64)channel << 32) | ((u64)EXIDeviceType::None << 16) | device_num,
                            from_thread);
  core_timing.ScheduleEvent(
      m_system.GetSystemTimers().GetTicksPerSecond(), m_event_type_change_device,
      ((u64)channel << 32) | ((u64)device_type << 16) | device_num, from_thread);
}

CEXIChannel* ExpansionInterfaceManager::GetChannel(u32 index)
{
  return m_channels.at(index).get();
}

IEXIDevice* ExpansionInterfaceManager::GetDevice(Slot slot)
{
  return m_channels.at(SlotToEXIChannel(slot))->GetDevice(1 << SlotToEXIDevice(slot));
}

void ExpansionInterfaceManager::UpdateInterrupts()
{
  // Interrupts are mapped a bit strangely:
  // Channel 0 Device 0 generates interrupt on channel 0
  // Channel 0 Device 2 generates interrupt on channel 2
  // Channel 1 Device 0 generates interrupt on channel 1
  m_channels[2]->SetEXIINT(m_channels[0]->GetDevice(4)->IsInterruptSet());

  bool causeInt = false;
  for (auto& channel : m_channels)
    causeInt |= channel->IsCausingInterrupt();

  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_EXI, causeInt);
}

void ExpansionInterfaceManager::UpdateInterruptsCallback(Core::System& system, u64 userdata,
                                                         s64 cycles_late)
{
  system.GetExpansionInterface().UpdateInterrupts();
}

void ExpansionInterfaceManager::ScheduleUpdateInterrupts(CoreTiming::FromThread from,
                                                         int cycles_late)
{
  m_system.GetCoreTiming().ScheduleEvent(cycles_late, m_event_type_update_interrupts, 0, from);
}

}  // namespace ExpansionInterface
