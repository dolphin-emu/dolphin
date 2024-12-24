// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_Channel.h"

#include <memory>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceMemoryCard.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/System.h"

namespace ExpansionInterface
{
enum
{
  EXI_READ,
  EXI_WRITE,
  EXI_READWRITE
};

CEXIChannel::CEXIChannel(Core::System& system, u32 channel_id,
                         const Memcard::HeaderData& memcard_header_data)
    : m_system(system), m_channel_id(channel_id), m_memcard_header_data(memcard_header_data)
{
  if (m_channel_id == 0 || m_channel_id == 1)
    m_status.EXTINT = 1;
  if (m_channel_id == 1)
    m_status.CHIP_SELECT = 1;

  for (auto& device : m_devices)
    device = EXIDevice_Create(system, EXIDeviceType::None, m_channel_id, m_memcard_header_data);
}

CEXIChannel::~CEXIChannel()
{
  RemoveDevices();
}

void CEXIChannel::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Warning: the base is not aligned on a page boundary here. We can't use |
  // to select a register address, instead we need to use +.

  mmio->Register(base + EXI_STATUS, MMIO::ComplexRead<u32>([this](Core::System&, u32) {
                   // check if external device is present
                   // pretty sure it is memcard only, not entirely sure
                   if (m_channel_id == 2)
                   {
                     m_status.EXT = 0;
                   }
                   else
                   {
                     m_status.EXT = GetDevice(1)->IsPresent() ? 1 : 0;
                   }

                   return m_status.Hex;
                 }),
                 MMIO::ComplexWrite<u32>([this](Core::System& system, u32, u32 val) {
                   UEXI_STATUS new_status(val);

                   m_status.EXIINTMASK = new_status.EXIINTMASK;
                   if (new_status.EXIINT)
                     m_status.EXIINT = 0;

                   m_status.TCINTMASK = new_status.TCINTMASK;
                   if (new_status.TCINT)
                     m_status.TCINT = 0;

                   m_status.CLK = new_status.CLK;

                   if (m_channel_id == 0 || m_channel_id == 1)
                   {
                     m_status.EXTINTMASK = new_status.EXTINTMASK;

                     if (new_status.EXTINT)
                       m_status.EXTINT = 0;
                   }

                   if (m_channel_id == 0)
                     m_status.ROMDIS = new_status.ROMDIS;

                   IEXIDevice* device = GetDevice(m_status.CHIP_SELECT ^ new_status.CHIP_SELECT);
                   m_status.CHIP_SELECT = new_status.CHIP_SELECT;
                   if (device != nullptr)
                     device->SetCS(m_status.CHIP_SELECT);

                   system.GetExpansionInterface().UpdateInterrupts();
                 }));

  mmio->Register(base + EXI_DMA_ADDRESS, MMIO::DirectRead<u32>(&m_dma_memory_address),
                 MMIO::DirectWrite<u32>(&m_dma_memory_address));
  mmio->Register(base + EXI_DMA_LENGTH, MMIO::DirectRead<u32>(&m_dma_length),
                 MMIO::DirectWrite<u32>(&m_dma_length));
  mmio->Register(base + EXI_DMA_CONTROL, MMIO::DirectRead<u32>(&m_control.Hex),
                 MMIO::ComplexWrite<u32>([this](Core::System&, u32, u32 val) {
                   m_control.Hex = val;

                   if (m_control.TSTART)
                   {
                     IEXIDevice* device = GetDevice(m_status.CHIP_SELECT);
                     if (device == nullptr)
                       return;

                     if (m_control.DMA == 0)
                     {
                       // immediate data
                       switch (m_control.RW)
                       {
                       case EXI_READ:
                         m_imm_data = device->ImmRead(m_control.TLEN + 1);
                         break;
                       case EXI_WRITE:
                         device->ImmWrite(m_imm_data, m_control.TLEN + 1);
                         break;
                       case EXI_READWRITE:
                         device->ImmReadWrite(m_imm_data, m_control.TLEN + 1);
                         break;
                       default:
                         DEBUG_ASSERT_MSG(EXPANSIONINTERFACE, 0,
                                          "EXI Imm: Unknown transfer type {}", m_control.RW);
                       }
                     }
                     else
                     {
                       // DMA
                       switch (m_control.RW)
                       {
                       case EXI_READ:
                         device->DMARead(m_dma_memory_address, m_dma_length);
                         break;
                       case EXI_WRITE:
                         device->DMAWrite(m_dma_memory_address, m_dma_length);
                         break;
                       default:
                         DEBUG_ASSERT_MSG(EXPANSIONINTERFACE, 0,
                                          "EXI DMA: Unknown transfer type {}", m_control.RW);
                       }
                     }

                     m_control.TSTART = 0;

                     // Check if device needs specific timing, otherwise just complete transfer
                     // immediately
                     if (!device->UseDelayedTransferCompletion())
                       SendTransferComplete();
                   }
                 }));

  mmio->Register(base + EXI_IMM_DATA, MMIO::DirectRead<u32>(&m_imm_data),
                 MMIO::DirectWrite<u32>(&m_imm_data));
}

void CEXIChannel::SendTransferComplete()
{
  m_status.TCINT = 1;
  m_system.GetExpansionInterface().UpdateInterrupts();
}

void CEXIChannel::RemoveDevices()
{
  for (auto& device : m_devices)
    device.reset(nullptr);
}

void CEXIChannel::AddDevice(const EXIDeviceType device_type, const int device_num)
{
  AddDevice(EXIDevice_Create(m_system, device_type, m_channel_id, m_memcard_header_data),
            device_num);
}

void CEXIChannel::AddDevice(std::unique_ptr<IEXIDevice> device, const int device_num,
                            bool notify_presence_changed)
{
  DEBUG_ASSERT(device_num < NUM_DEVICES);

  INFO_LOG_FMT(EXPANSIONINTERFACE,
               "Changing EXI channel {}, device {} to type {} (notify software: {})", m_channel_id,
               device_num, static_cast<int>(device->m_device_type),
               notify_presence_changed ? "true" : "false");

  // Replace it with the new one
  m_devices[device_num] = std::move(device);

  if (notify_presence_changed)
  {
    // This means "device presence changed", software has to check
    // m_status.EXT to see if it is now present or not
    if (m_channel_id != 2)
    {
      m_status.EXTINT = 1;
      m_system.GetExpansionInterface().UpdateInterrupts();
    }
  }
}

bool CEXIChannel::IsCausingInterrupt()
{
  if (m_channel_id != 2 && GetDevice(1)->IsInterruptSet())
    m_status.EXIINT = 1;  // Always check memcard slots
  else if (GetDevice(m_status.CHIP_SELECT))
    if (GetDevice(m_status.CHIP_SELECT)->IsInterruptSet())
      m_status.EXIINT = 1;

  if ((m_status.EXIINT & m_status.EXIINTMASK) || (m_status.TCINT & m_status.TCINTMASK) ||
      (m_status.EXTINT & m_status.EXTINTMASK))
  {
    return true;
  }
  else
  {
    return false;
  }
}

IEXIDevice* CEXIChannel::GetDevice(const u8 chip_select)
{
  switch (chip_select)
  {
  case 1:
    return m_devices[0].get();
  case 2:
    return m_devices[1].get();
  case 4:
    return m_devices[2].get();
  }
  return nullptr;
}

void CEXIChannel::DoState(PointerWrap& p)
{
  p.Do(m_status);
  p.Do(m_dma_memory_address);
  p.Do(m_dma_length);
  p.Do(m_control);
  p.Do(m_imm_data);
  p.Do(m_memcard_header_data);

  for (int device_index = 0; device_index < NUM_DEVICES; ++device_index)
  {
    EXIDeviceType type = m_devices[device_index]->m_device_type;
    p.Do(type);

    if (type != m_devices[device_index]->m_device_type)
    {
      AddDevice(EXIDevice_Create(m_system, type, m_channel_id, m_memcard_header_data), device_index,
                false);
    }

    m_devices[device_index]->DoState(p);

    const bool is_memcard =
        (type == EXIDeviceType::MemoryCard || type == EXIDeviceType::MemoryCardFolder);
    if (is_memcard && !m_system.GetMovie().IsMovieActive())
    {
      // We are processing a savestate that has a memory card plugged into the system, but don't
      // want to actually store the memory card in the savestate, so loading older savestates
      // doesn't confusingly revert in-game saves done since then.

      // Handling this properly is somewhat complex. When loading a state, the game still needs to
      // see the memory card device as it was (or at least close enough) when the state was made,
      // but at the same time we need to tell the game that the data on the card may have been
      // changed and it should not assume that the data (especially the file system blocks) it may
      // or may not have read before is still valid.

      // To accomplish this we do the following:

      CEXIMemoryCard* card_device = static_cast<CEXIMemoryCard*>(m_devices[device_index].get());
      constexpr s32 file_system_data_size = 0xa000;

      if (p.IsReadMode())
      {
        // When loading a state, we compare the previously written file system block data with the
        // data currently in the card. If it mismatches in any way, we make sure to notify the game.
        bool should_notify_game = false;
        bool has_card_file_system_data;
        p.Do(has_card_file_system_data);
        if (has_card_file_system_data)
        {
          std::array<u8, file_system_data_size> state_file_system_data;
          p.Do(state_file_system_data);
          std::array<u8, file_system_data_size> card_file_system_data;
          const s32 bytes_read =
              card_device->ReadFromMemcard(0, file_system_data_size, card_file_system_data.data());
          bool read_success = bytes_read == file_system_data_size;
          should_notify_game = !(read_success && state_file_system_data == card_file_system_data);
        }
        else
        {
          should_notify_game = true;
        }

        if (should_notify_game)
        {
          // We want to tell the game that the card is different, but don't want to immediately
          // destroy the memory card device, as there may be pending operations running on the CPU
          // side (at the very least, I've seen memory accesses to 0 when switching the device
          // immediately in F-Zero GX). In order to ensure data on the card isn't corrupted by
          // these, we disable writes to the memory card device.
          card_device->DisableWrites();

          // And then we force a re-load of the memory card by switching to a null device a frame
          // from now, then back to the memory card a few frames later. This also makes sure that
          // the GCI folder header matches what the game expects, so the game never recognizes it as
          // a 'different' memory card and prevents saving on it.
          const u32 cycles_per_second = m_system.GetSystemTimers().GetTicksPerSecond();
          m_system.GetExpansionInterface().ChangeDevice(
              m_channel_id, device_index, type, cycles_per_second / 60, cycles_per_second / 3,
              CoreTiming::FromThread::CPU);
        }
      }
      else
      {
        // When saving a state (or when we're measuring or verifiying the state data) we read the
        // file system blocks from the currently active memory card and push them into p, so we have
        // a reference of how the card file system looked at the time of savestate creation.
        std::array<u8, file_system_data_size> card_file_system_data;
        const s32 bytes_read =
            card_device->ReadFromMemcard(0, file_system_data_size, card_file_system_data.data());
        bool read_success = bytes_read == file_system_data_size;
        p.Do(read_success);
        if (read_success)
          p.Do(card_file_system_data);
      }
    }
  }
}

void CEXIChannel::SetEXIINT(bool exiint)
{
  m_status.EXIINT = !!exiint;
}
}  // namespace ExpansionInterface
