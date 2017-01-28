// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HLE/HLE_VarArgs.h"

HLE::SystemVABI::VAList::~VAList() = default;

u32 HLE::SystemVABI::VAList::GetGPR(u32 gpr) const
{
  return GPR(gpr);
}

double HLE::SystemVABI::VAList::GetFPR(u32 fpr) const
{
  return rPS0(fpr);
}
