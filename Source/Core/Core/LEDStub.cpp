// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// this file includes a no-op stub for LED functionality.
// it should be used on platforms that do not (yet) have
// means available to control the LED state of an input device.

#include "Core/LED.h"

namespace LED
{

void Initialize() {}
void Start() {}
void Stop() {}
void Shutdown() {}

}
