// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_Channel.h"

#include <memory>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/MMIO.h"

namespace ExpansionInterface
{
enum
{
  EXI_READ,
  EXI_WRITE,
  EXI_READWRITE
};

CEXIChannel::CEXIChannel(u32 channel_id) : m_channel_id(channel_id)
{
  if (m_channel_id == 0 || m_channel_id == 1)
    m_status.EXTINT = 1;
  if (m_channel_id == 1)
    m_status.CHIP_SELECT = 1;

  for (auto& device : m_devices)
    device = EXIDevice_Create(EXIDEVICE_NONE, m_channel_id);
}

CEXIChannel::~CEXIChannel()
{
  RemoveDevices();
}

void CEXIChannel::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Warning: the base is not aligned on a page boundary here. We can't use |
  // to select a register address, instead we need to use +.

  mmio->Register(base + EXI_STATUS, MMIO::ComplexRead<u32>([this](u32) {
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
                 MMIO::ComplexWrite<u32>([this](u32, u32 val) {
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

                   ExpansionInterface::UpdateInterrupts();
                 }));

  mmio->Register(base + EXI_DMA_ADDRESS, MMIO::DirectRead<u32>(&m_dma_memory_address),
                 MMIO::DirectWrite<u32>(&m_dma_memory_address));
  mmio->Register(base + EXI_DMA_LENGTH, MMIO::DirectRead<u32>(&m_dma_length),
                 MMIO::DirectWrite<u32>(&m_dma_length));
  mmio->Register(base + EXI_DMA_CONTROL, MMIO::DirectRead<u32>(&m_control.Hex),
                 MMIO::ComplexWrite<u32>([this](u32, u32 val) {
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
                         _dbg_assert_msg_(EXPANSIONINTERFACE, 0,
                                          "EXI Imm: Unknown transfer type %i", m_control.RW);
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
                         _dbg_assert_msg_(EXPANSIONINTERFACE, 0,
                                          "EXI DMA: Unknown transfer type %i", m_control.RW);
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
  ExpansionInterface::UpdateInterrupts();
}

void CEXIChannel::RemoveDevices()
{
  for (auto& device : m_devices)
    device.reset(nullptr);
}

void CEXIChannel::AddDevice(const TEXIDevices device_type, const int device_num)
{
  AddDevice(EXIDevice_Create(device_type, m_channel_id), device_num);
}

void CEXIChannel::AddDevice(std::unique_ptr<IEXIDevice> device, const int device_num,
                            bool notify_presence_changed)
{
  _dbg_assert_(EXPANSIONINTERFACE, device_num < NUM_DEVICES);

  // Replace it with the new one
  m_devices[device_num] = std::move(device);

  if (notify_presence_changed)
  {
    // This means "device presence changed", software has to check
    // m_status.EXT to see if it is now present or not
    if (m_channel_id != 2)
    {
      m_status.EXTINT = 1;
      ExpansionInterface::UpdateInterrupts();
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
  p.DoPOD(m_status);
  p.Do(m_dma_memory_address);
  p.Do(m_dma_length);
  p.Do(m_control);
  p.Do(m_imm_data);

  for (int device_index = 0; device_index < NUM_DEVICES; ++device_index)
  {
    std::unique_ptr<IEXIDevice>& device = m_devices[device_index];
    TEXIDevices type = device->m_device_type;
    p.Do(type);

    if (type == device->m_device_type)
    {
      device->DoState(p);
    }
    else
    {
      std::unique_ptr<IEXIDevice> save_device = EXIDevice_Create(type, m_channel_id);
      save_device->DoState(p);
      AddDevice(std::move(save_device), device_index, false);
    }
  }
}

void CEXIChannel::PauseAndLock(bool do_lock, bool resume_on_unlock)
{
  for (auto& device : m_devices)
    device->PauseAndLock(do_lock, resume_on_unlock);
}

void CEXIChannel::SetEXIINT(bool exiint)
{
  m_status.EXIINT = !!exiint;
}

IEXIDevice* CEXIChannel::FindDevice(TEXIDevices device_type, int custom_index)
{
  for (auto& sup : m_devices)
  {
    IEXIDevice* device = sup->FindDevice(device_type, custom_index);
    if (device)
      return device;
  }
  return nullptr;
}
}  // namespace ExpansionInterface
