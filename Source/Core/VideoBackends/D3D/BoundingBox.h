// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "VideoBackends/D3D/D3DBase.h"

namespace DX11
{

class BBox
{
public:
	static ID3D11UnorderedAccessView* &GetUAV();
	static void Init();
	static void Shutdown();

	static void Set(int index, int value);
	static int Get(int index);
};

};
