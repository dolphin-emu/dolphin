// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

namespace X11Utils
{
class XRRConfiguration;
}

namespace VideoUtils
{
#if !defined(__APPLE__)
std::vector<std::string> GetAvailableResolutions(X11Utils::XRRConfiguration* xrr_config);
#endif

std::vector<std::string> GetAvailableAntialiasingModes(int& m_msaa_modes);
}  // namespace VideoUtils
