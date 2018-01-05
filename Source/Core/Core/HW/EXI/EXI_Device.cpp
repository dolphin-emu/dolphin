// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_Device.h"

#include <memory>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_DeviceAD16.h"
#include "Core/HW/EXI/EXI_DeviceAGP.h"
#include "Core/HW/EXI/EXI_DeviceDummy.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"
#include "Core/HW/EXI/EXI_DeviceGecko.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/EXI/EXI_DeviceMemoryCard.h"
#include "Core/HW/EXI/EXI_DeviceMic.h"
#include "Core/HW/Memmap.h"

namespace ExpansionInterface
{
void IEXIDevice::ImmWrite(u32 data, u32 size)
{
  while (size--)
  {
    u8 byte = data >> 24;
    TransferByte(byte);
    data <<= 8;
  }
}

u32 IEXIDevice::ImmRead(u32 size)
{
  u32 result = 0;
  u32 position = 0;
  while (size--)
  {
    u8 byte = 0;
    TransferByte(byte);
    result |= byte << (24 - (position++ * 8));
  }
  return result;
}

void IEXIDevice::ImmReadWrite(u32& data, u32 size)
{
}

void IEXIDevice::DMAWrite(u32 address, u32 size)
{
  while (size--)
  {
    u8 byte = Memory::Read_U8(address++);
    TransferByte(byte);
  }
}

void IEXIDevice::DMARead(u32 address, u32 size)
{
  while (size--)
  {
    u8 byte = 0;
    TransferByte(byte);
    Memory::Write_U8(byte, address++);
  }
}

IEXIDevice* IEXIDevice::FindDevice(TEXIDevices device_type, int custom_index)
{
  return (device_type == m_device_type) ? this : nullptr;
}

bool IEXIDevice::UseDelayedTransferCompletion() const
{
  return false;
}

bool IEXIDevice::IsPresent() const
{
  return false;
}

void IEXIDevice::SetCS(int cs)
{
}

void IEXIDevice::DoState(PointerWrap& p)
{
}

void IEXIDevice::PauseAndLock(bool do_lock, bool resume_on_unlock)
{
}

bool IEXIDevice::IsInterruptSet()
{
  return false;
}

void IEXIDevice::TransferByte(u8& byte)
{
}

// F A C T O R Y
std::unique_ptr<IEXIDevice> EXIDevice_Create(const TEXIDevices device_type, const int channel_num)
{
  std::unique_ptr<IEXIDevice> result;

  switch (device_type)
  {
  case EXIDEVICE_DUMMY:
    result = std::make_unique<CEXIDummy>("Dummy");
    break;

  case EXIDEVICE_MEMORYCARD:
  case EXIDEVICE_MEMORYCARDFOLDER:
  {
    bool gci_folder = (device_type == EXIDEVICE_MEMORYCARDFOLDER);
    result = std::make_unique<CEXIMemoryCard>(channel_num, gci_folder);
    break;
  }
  case EXIDEVICE_MASKROM:
    result = std::make_unique<CEXIIPL>();
    break;

  case EXIDEVICE_AD16:
    result = std::make_unique<CEXIAD16>();
    break;

  case EXIDEVICE_MIC:
    result = std::make_unique<CEXIMic>(channel_num);
    break;

  case EXIDEVICE_ETH:
    result = std::make_unique<CEXIETHERNET>();
    break;

  case EXIDEVICE_GECKO:
    result = std::make_unique<CEXIGecko>();
    break;

  case EXIDEVICE_AGP:
    result = std::make_unique<CEXIAgp>(channel_num);
    break;

  case EXIDEVICE_AM_BASEBOARD:
  case EXIDEVICE_NONE:
  default:
    result = std::make_unique<IEXIDevice>();
    break;
  }

  if (result != nullptr)
    result->m_device_type = device_type;

  return result;
}
}  // namespace ExpansionInterface
