// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSPEmulator.h"

#include <memory>

#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPLLE/DSPLLE.h"

DSPEmulator::~DSPEmulator() = default;

std::unique_ptr<DSPEmulator> CreateDSPEmulator(bool hle)
{
  if (hle)
    return std::make_unique<DSP::HLE::DSPHLE>();

  return std::make_unique<DSP::LLE::DSPLLE>();
}
