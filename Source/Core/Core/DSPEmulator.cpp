// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSPEmulator.h"

#include <memory>

#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPLLE/DSPLLE.h"

std::unique_ptr<DSPEmulator> CreateDSPEmulator(bool hle)
{
  if (hle)
    return std::make_unique<DSP::HLE::DSPHLE>();

  return std::make_unique<DSP::LLE::DSPLLE>();
}
