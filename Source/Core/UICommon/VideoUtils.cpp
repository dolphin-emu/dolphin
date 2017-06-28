// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/VideoUtils.h"

#include "Common/Assert.h"
#include "VideoCommon/VideoConfig.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "UICommon/X11Utils.h"
#endif

namespace VideoUtils
{
#if !defined(__APPLE__)
std::vector<std::string> GetAvailableResolutions(X11Utils::XRRConfiguration* xrr_config)
{
  std::vector<std::string> resos;
#ifdef _WIN32
  DWORD iModeNum = 0;
  DEVMODE dmi;
  ZeroMemory(&dmi, sizeof(dmi));
  dmi.dmSize = sizeof(dmi);

  while (EnumDisplaySettings(nullptr, iModeNum++, &dmi) != 0)
  {
    char res[100];
    sprintf(res, "%dx%d", dmi.dmPelsWidth, dmi.dmPelsHeight);
    std::string strRes(res);
    // Only add unique resolutions
    if (std::find(resos.begin(), resos.end(), strRes) == resos.end())
    {
      resos.push_back(strRes);
    }
    ZeroMemory(&dmi, sizeof(dmi));
  }
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
  xrr_config->AddResolutions(resos);
#endif
  return resos;
}
#endif

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
      _assert_msg_(VIDEO, !supports_ssaa || msaa_modes == 0, "SSAA setting won't work correctly");
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
}
