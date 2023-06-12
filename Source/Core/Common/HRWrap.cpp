// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "HRWrap.h"

namespace Common
{
std::string GetHResultMessage(HRESULT hr)
{
  auto err = winrt::hresult_error(hr);
  return winrt::to_string(err.message());
}
std::string GetHResultMessage(const winrt::hresult& hr)
{
  return GetHResultMessage(hr.value);
}
}  // namespace Common
