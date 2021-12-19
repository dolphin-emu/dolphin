// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace HLE_OS
{
enum class ParameterType : bool
{
  ParameterList = false,
  VariableArgumentList = true
};

std::string GetStringVA(u32 str_reg = 3,
                        ParameterType parameter_type = ParameterType::ParameterList);

void HLE_GeneralDebugPrint();
void HLE_GeneralDebugVPrint();
void HLE_write_console();
void HLE_OSPanic();
void HLE_LogDPrint();
void HLE_LogVDPrint();
void HLE_LogFPrint();
void HLE_LogVFPrint();
}  // namespace HLE_OS
