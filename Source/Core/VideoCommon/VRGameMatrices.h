// Copyright 2016 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/MathUtil.h"

bool CalculateTrackingSpaceToViewSpaceMatrix(int kind, Matrix44& look_matrix);
bool CalculateViewMatrix(int kind, Matrix44& look_matrix);
void VRCalculateIRPointer();