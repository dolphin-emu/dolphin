// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/HSP/HSP_DeviceARAMExpansion.h"

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/Swap.h"

#include "Core/ConfigManager.h"
#include "Core/HW/DSP.h"
#include "Core/HW/HSP/HSP.h"
#include "Core/HW/ProcessorInterface.h"

#include <cstring>

namespace HSP
{
CHSPDevice_ARAMExpansion::CHSPDevice_ARAMExpansion(HSPDevices device) : IHSPDevice(device)
{
  m_size = MathUtil::NextPowerOf2(SConfig::GetInstance().m_ARAMExpansion);
  m_mask = m_size - 1;
  m_ptr = static_cast<u8*>(Common::AllocateMemoryPages(m_size));
}

CHSPDevice_ARAMExpansion::~CHSPDevice_ARAMExpansion()
{
  Common::FreeMemoryPages(m_ptr, m_size);
  m_ptr = nullptr;
}

u64 CHSPDevice_ARAMExpansion::Read(u32 address)
{
  u64 value;
  std::memcpy(&value, &m_ptr[address & m_mask], sizeof(value));
  return Common::swap64(value);
}

void CHSPDevice_ARAMExpansion::Write(u32 address, u64 value)
{
  value = Common::swap64(value);
  std::memcpy(&value, &m_ptr[address & m_mask], sizeof(value));
}

void CHSPDevice_ARAMExpansion::DoState(PointerWrap& p)
{
  p.DoArray(m_ptr, m_size);
}

}  // namespace HSP
