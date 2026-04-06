// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CommonFuncs.h"

#include <Foundation/Foundation.h>

namespace Common
{
MacOSVersion GetMacOSVersion()
{
  const NSOperatingSystemVersion ver = [[NSProcessInfo processInfo] operatingSystemVersion];
  return {ver.majorVersion, ver.minorVersion, ver.patchVersion};
}
}  // namespace Common
