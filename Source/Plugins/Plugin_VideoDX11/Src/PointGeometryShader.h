// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _POINTGEOMETRYSHADER_H
#define _POINTGEOMETRYSHADER_H

#include "VideoCommon.h"

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

#endif
