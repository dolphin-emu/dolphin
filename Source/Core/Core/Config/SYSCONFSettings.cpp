// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/SYSCONFSettings.h"

#include "Core/Config/DefaultLocale.h"

namespace Config
{
// SYSCONF.IPL

const Info<bool> SYSCONF_SCREENSAVER{{System::SYSCONF, "IPL", "SSV"}, false};
const Info<u32> SYSCONF_LANGUAGE{{System::SYSCONF, "IPL", "LNG"},
                                 static_cast<u32>(GetDefaultLanguage())};
const Info<u32> SYSCONF_COUNTRY{{System::SYSCONF, "IPL", "SADR"}, GetDefaultCountry()};
const Info<bool> SYSCONF_WIDESCREEN{{System::SYSCONF, "IPL", "AR"}, true};
const Info<bool> SYSCONF_PROGRESSIVE_SCAN{{System::SYSCONF, "IPL", "PGS"}, true};
const Info<bool> SYSCONF_PAL60{{System::SYSCONF, "IPL", "E60"}, 0x01};
const Info<u32> SYSCONF_SOUND_MODE{{System::SYSCONF, "IPL", "SND"}, 0x01};

// SYSCONF.BT

const Info<u32> SYSCONF_SENSOR_BAR_POSITION{{System::SYSCONF, "BT", "BAR"}, 0x01};
const Info<u32> SYSCONF_SENSOR_BAR_SENSITIVITY{{System::SYSCONF, "BT", "SENS"}, 0x03};
const Info<u32> SYSCONF_SPEAKER_VOLUME{{System::SYSCONF, "BT", "SPKV"}, 0x58};
const Info<bool> SYSCONF_WIIMOTE_MOTOR{{System::SYSCONF, "BT", "MOT"}, true};

const std::array<SYSCONFSetting, 11> SYSCONF_SETTINGS{
    {{&SYSCONF_SCREENSAVER, SysConf::Entry::Type::Byte},
     {&SYSCONF_LANGUAGE, SysConf::Entry::Type::Byte},
     {&SYSCONF_COUNTRY, SysConf::Entry::Type::BigArray},
     {&SYSCONF_WIDESCREEN, SysConf::Entry::Type::Byte},
     {&SYSCONF_PROGRESSIVE_SCAN, SysConf::Entry::Type::Byte},
     {&SYSCONF_PAL60, SysConf::Entry::Type::Byte},
     {&SYSCONF_SOUND_MODE, SysConf::Entry::Type::Byte},
     {&SYSCONF_SENSOR_BAR_POSITION, SysConf::Entry::Type::Byte},
     {&SYSCONF_SENSOR_BAR_SENSITIVITY, SysConf::Entry::Type::Long},
     {&SYSCONF_SPEAKER_VOLUME, SysConf::Entry::Type::Byte},
     {&SYSCONF_WIIMOTE_MOTOR, SysConf::Entry::Type::Byte}}};
}  // namespace Config
