// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace LED
{

// initialize the LED state of supported keyboards, called once on startup
void Initialize();
// called when a game starts; should be used to update LEDs to match the users configuration
void Start();
// called when a game stops; should be used to revert the LED state of supported keyboards back to its initial configuration
void Stop();
// restores the LED state of supported keyboards to an initial configuration, called once on shutdown
void Shutdown();

}
