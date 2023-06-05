// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/VideoUtils.h"

#include "Common/Assert.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoUtils
{
std::vector<std::string> GetAvailableAntialiasingModes(int& msaa_modes)
{
  std::vector<std::string> modes;
  const auto& aa_modes = g_Config.backend_info.AAModes;
  const bool supports_ssaa = g_Config.backend_info.bSupportsSSAA;
  msaa_modes = 0;

  for (const auto mode : aa_modes)
  {
    if (mode == 1)
    {
      modes.push_back("None");
      ASSERT_MSG(VIDEO, !supports_ssaa || msaa_modes == 0, "SSAA setting won't work correctly");
    }
    else
    {
      modes.push_back(std::to_string(mode) + "x MSAA");
      msaa_modes++;
    }
  }

  if (supports_ssaa)
  {
    for (const auto mode : aa_modes)
    {
      if (mode != 1)
        modes.push_back(std::to_string(mode) + "x SSAA");
    }
  }

  return modes;
}
}  // namespace VideoUtils
