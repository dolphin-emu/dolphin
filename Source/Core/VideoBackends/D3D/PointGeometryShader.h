// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoCommon.h"

struct ID3D11Buffer;
struct ID3D11GeometryShader;

namespace DX11
{

// This class manages a collection of point geometry shaders, one for each
// vertex format.
class PointGeometryShader
{

public:

	PointGeometryShader();

	void Init();
	void Shutdown();
	// Returns true on success, false on failure
	bool SetShader(u32 components, float pointSize, float texOffset,
		float vpWidth, float vpHeight, const bool* texOffsetEnable);

private:

	bool m_ready;

	ID3D11Buffer* m_paramsBuffer;

	typedef std::map<u32, ID3D11GeometryShader*> ComboMap;

	ComboMap m_shaders;

};

}
