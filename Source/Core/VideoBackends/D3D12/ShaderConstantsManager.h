// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "ShaderCache.h"

namespace DX12
{

class ShaderConstantsManager final
{
public:
	static void Init();
	static void Shutdown();

	static void LoadAndSetGeometryShaderConstants();
	static void LoadAndSetPixelShaderConstants();
	static void LoadAndSetVertexShaderConstants();
	static void CheckToResetIndexPositionInUploadHeaps();
};

}