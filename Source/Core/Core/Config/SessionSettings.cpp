// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/SessionSettings.h"

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

namespace Config
{
const Info<bool> SESSION_USE_FMA{{System::Session, "Core", "UseFMA"}, CPUInfo().bFMA};
}  // namespace Config
