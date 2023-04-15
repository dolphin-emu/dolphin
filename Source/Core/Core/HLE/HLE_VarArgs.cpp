// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HLE/HLE_VarArgs.h"
#include "Core/System.h"

#include "Common/Logging/Log.h"

HLE::SystemVABI::VAList::~VAList() = default;

u32 HLE::SystemVABI::VAList::GetGPR(const Core::CPUThreadGuard&, u32 gpr) const
{
  return m_system.GetPPCState().gpr[gpr];
}

double HLE::SystemVABI::VAList::GetFPR(const Core::CPUThreadGuard&, u32 fpr) const
{
  return m_system.GetPPCState().ps[fpr].PS0AsDouble();
}

HLE::SystemVABI::VAListStruct::VAListStruct(Core::System& system, const Core::CPUThreadGuard& guard,
                                            u32 address)
    : VAList(system, 0), m_va_list{PowerPC::MMU::HostRead_U8(guard, address),
                                   PowerPC::MMU::HostRead_U8(guard, address + 1),
                                   PowerPC::MMU::HostRead_U32(guard, address + 4),
                                   PowerPC::MMU::HostRead_U32(guard, address + 8)},
      m_address(address), m_has_fpr_area(system.GetPPCState().cr.GetBit(6) == 1)
{
  m_stack = m_va_list.overflow_arg_area;
  m_gpr += m_va_list.gpr;
  m_fpr += m_va_list.fpr;
}

u32 HLE::SystemVABI::VAListStruct::GetGPRArea() const
{
  return m_va_list.reg_save_area;
}

u32 HLE::SystemVABI::VAListStruct::GetFPRArea() const
{
  return GetGPRArea() + 4 * 8;
}

u32 HLE::SystemVABI::VAListStruct::GetGPR(const Core::CPUThreadGuard& guard, u32 gpr) const
{
  if (gpr < 3 || gpr > 10)
  {
    ERROR_LOG_FMT(OSHLE, "VAListStruct at {:08x} doesn't have GPR{}!", m_address, gpr);
    return 0;
  }
  const u32 gpr_address = Common::AlignUp(GetGPRArea() + 4 * (gpr - 3), 4);
  return PowerPC::MMU::HostRead_U32(guard, gpr_address);
}

double HLE::SystemVABI::VAListStruct::GetFPR(const Core::CPUThreadGuard& guard, u32 fpr) const
{
  if (!m_has_fpr_area || fpr < 1 || fpr > 8)
  {
    ERROR_LOG_FMT(OSHLE, "VAListStruct at {:08x} doesn't have FPR{}!", m_address, fpr);
    return 0.0;
  }
  const u32 fpr_address = Common::AlignUp(GetFPRArea() + 8 * (fpr - 1), 8);
  return PowerPC::MMU::HostRead_F64(guard, fpr_address);
}
