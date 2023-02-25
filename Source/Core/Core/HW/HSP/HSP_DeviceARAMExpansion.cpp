// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP_DeviceARAMExpansion.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/Swap.h"

#include "Core/Config/MainSettings.h"

namespace HSP
{
CHSPDevice_ARAMExpansion::CHSPDevice_ARAMExpansion(HSPDeviceType device) : IHSPDevice(device)
{
  m_size = MathUtil::NextPowerOf2(Config::Get(Config::MAIN_ARAM_EXPANSION_SIZE));
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
  std::memcpy(&m_ptr[address & m_mask], &value, sizeof(value));
}

void CHSPDevice_ARAMExpansion::DoState(PointerWrap& p)
{
  p.DoArray(m_ptr, m_size);
}

}  // namespace HSP
