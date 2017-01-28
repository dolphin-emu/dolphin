// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HLE/HLE_VarArgs.h"

#include "Common/Logging/Log.h"

HLE::SystemVABI::VAList::~VAList() = default;

u32 HLE::SystemVABI::VAList::GetGPR(u32 gpr) const
{
  return GPR(gpr);
}

double HLE::SystemVABI::VAList::GetFPR(u32 fpr) const
{
  return rPS0(fpr);
}

HLE::SystemVABI::VAListStruct::VAListStruct(u32 address)
    : VAList(0), m_va_list{PowerPC::HostRead_U8(address), PowerPC::HostRead_U8(address + 1),
                           PowerPC::HostRead_U32(address + 4), PowerPC::HostRead_U32(address + 8)},
      m_address(address), m_has_fpr_area(GetCRBit(6) == 1)
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

u32 HLE::SystemVABI::VAListStruct::GetGPR(u32 gpr) const
{
  if (gpr < 3 || gpr > 10)
  {
    ERROR_LOG(OSHLE, "VAListStruct at %08x doesn't have GPR%d!", m_address, gpr);
    return 0;
  }
  const u32 gpr_address = Common::AlignUp(GetGPRArea() + 4 * (gpr - 3), 4);
  return PowerPC::HostRead_U32(gpr_address);
}

double HLE::SystemVABI::VAListStruct::GetFPR(u32 fpr) const
{
  double value = 0.0;

  if (!m_has_fpr_area || fpr < 1 || fpr > 8)
  {
    ERROR_LOG(OSHLE, "VAListStruct at %08x doesn't have FPR%d!", m_address, fpr);
  }
  else
  {
    const u32 fpr_address = Common::AlignUp(GetFPRArea() + 8 * (fpr - 1), 8);
    const u64 integral = PowerPC::HostRead_U64(fpr_address);
    std::memcpy(&value, &integral, sizeof(double));
  }
  return value;
}
