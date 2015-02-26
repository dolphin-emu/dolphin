// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/MathUtil.h"

#pragma once

namespace VRTracker
{
	void Shutdown();
	void Initialize(void* const hwnd);
	void GetTransformMatrix(Matrix44& mtx);
}
