// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "VideoBackends/D3D12/D3DBase.h"

namespace DX12
{

class BBox
{
public:
	static void Init();
	static void Bind();
	static void Invalidate();
	static void Shutdown();

	static void Set(int index, int value);
	static int Get(int index);
};

};
