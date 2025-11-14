// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::Pipes
{
// To create a piped controller input, create a named pipe in the
// Pipes directory and write commands out to it. Commands are separated
// by a newline character, with spaces separating command tokens.
// Command syntax is as follows, where curly brackets are one-of and square
// brackets are inclusive numeric ranges. Cases are sensitive. Numeric inputs
// are clamped to [0, 1] and otherwise invalid commands are discarded.
// {PRESS, RELEASE} {A, B, X, Y, Z, START, L, R, D_UP, D_DOWN, D_LEFT, D_RIGHT}
// SET {L, R} [0, 1]
// SET {MAIN, C} [0, 1] [0, 1]

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

}  // namespace ciface::Pipes
