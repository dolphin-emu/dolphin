// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "HRWrap.h"

#include <winrt/base.h>

namespace Common
{
std::string GetHResultMessage(HRESULT hr)
{
  auto err = winrt::hresult_error(hr);
  return winrt::to_string(err.message());
}
}  // namespace Common
