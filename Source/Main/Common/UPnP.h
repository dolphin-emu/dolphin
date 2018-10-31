// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef USE_UPNP

#include "Common/CommonTypes.h"

namespace UPnP
{
void TryPortmapping(u16 port);
void StopPortmapping();
}

#endif
