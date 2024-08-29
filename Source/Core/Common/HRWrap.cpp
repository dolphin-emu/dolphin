// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "HRWrap.h"

#ifndef _MSC_VER
#include <comdef.h>
#endif
#include "Common/StringUtil.h"

namespace Common
{
#ifdef _MSC_VER
std::string GetHResultMessage(HRESULT hr)
{
  auto err = winrt::hresult_error(hr);
  return winrt::to_string(err.message());
}
std::string GetHResultMessage(const winrt::hresult& hr)
{
  return GetHResultMessage(hr.value);
}
#else
std::string GetHResultMessage(HRESULT hr)
{
  _com_error err(hr);
  return TStrToUTF8(err.ErrorMessage());
}
#endif
}  // namespace Common
