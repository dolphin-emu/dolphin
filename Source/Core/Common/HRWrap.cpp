// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "HRWrap.h"

#include <comdef.h>
#include "Common/StringUtil.h"

namespace Common
{
std::string GetHResultMessage(HRESULT hr)
{
  // See https://stackoverflow.com/a/7008111
  _com_error err(hr);
  return TStrToUTF8(err.ErrorMessage());
}
}  // namespace Common
