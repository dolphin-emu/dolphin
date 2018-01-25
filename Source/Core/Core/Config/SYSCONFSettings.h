// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <variant>

#include "Common/Config/Config.h"
#include "Common/SysConf.h"

namespace Config
{
// Note: some settings are actually u8s, but stored as u32 in the layer because of limitations.

// SYSCONF.IPL

extern const ConfigInfo<bool> SYSCONF_SCREENSAVER;
extern const ConfigInfo<u32> SYSCONF_LANGUAGE;
extern const ConfigInfo<bool> SYSCONF_WIDESCREEN;
extern const ConfigInfo<bool> SYSCONF_PROGRESSIVE_SCAN;
extern const ConfigInfo<bool> SYSCONF_PAL60;

// SYSCONF.BT

extern const ConfigInfo<u32> SYSCONF_SENSOR_BAR_POSITION;
extern const ConfigInfo<u32> SYSCONF_SENSOR_BAR_SENSITIVITY;
extern const ConfigInfo<u32> SYSCONF_SPEAKER_VOLUME;
extern const ConfigInfo<bool> SYSCONF_WIIMOTE_MOTOR;

struct SYSCONFSetting
{
  std::variant<ConfigInfo<u32>, ConfigInfo<bool>> config_info;
  SysConf::Entry::Type type;
};

extern const std::array<SYSCONFSetting, 9> SYSCONF_SETTINGS;

}  // namespace Config
