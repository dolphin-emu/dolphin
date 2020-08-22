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
#include "Core/HW/MMIO.h"
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
  case 0: // HACK - SD
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

  Memcard::HeaderData old_header_data = m_memcard_header_data;
  p.Do(m_memcard_header_data);

  for (int device_index = 0; device_index < NUM_DEVICES; ++device_index)
  {
    std::unique_ptr<IEXIDevice>& device = m_devices[device_index];
    EXIDeviceType type = device->m_device_type;
    p.Do(type);

    if (type == device->m_device_type)
    {
      device->DoState(p);
    }
    else
    {
      std::unique_ptr<IEXIDevice> save_device =
          EXIDevice_Create(m_system, type, m_channel_id, m_memcard_header_data);
      save_device->DoState(p);
      AddDevice(std::move(save_device), device_index, false);
    }

    if (type == EXIDeviceType::MemoryCardFolder && old_header_data != m_memcard_header_data &&
        !m_system.GetMovie().IsMovieActive())
    {
      // We have loaded a savestate that has a GCI folder memcard that is different to the virtual
      // card that is currently active. In order to prevent the game from recognizing this card as a
      // 'different' memory card and preventing saving on it, we need to reinitialize the GCI folder
      // card here with the loaded header data.
      // We're intentionally calling ExpansionInterface::ChangeDevice() here instead of changing it
      // directly so we don't switch immediately but after a delay, as if changed in the GUI. This
      // should prevent games from assuming any stale data about the memory card, such as location
      // of the individual save blocks, which may be different on the reinitialized card.
      // Additionally, we immediately force the memory card to None so that any 'in-flight' writes
      // (if someone managed to savestate while saving...) don't happen to hit the card.
      // TODO: It might actually be enough to just switch to the card with the
      // notify_presence_changed flag set to true? Not sure how software behaves if the previous and
      // the new device type are identical in this case. I assume there is a reason we have this
      // grace period when switching in the GUI.
      AddDevice(EXIDeviceType::None, device_index);
      m_system.GetExpansionInterface().ChangeDevice(
          m_channel_id, device_index, EXIDeviceType::MemoryCardFolder, CoreTiming::FromThread::CPU);
    }
  }
}

void CEXIChannel::SetEXIINT(bool exiint)
{
  m_status.EXIINT = !!exiint;
}
}  // namespace ExpansionInterface
