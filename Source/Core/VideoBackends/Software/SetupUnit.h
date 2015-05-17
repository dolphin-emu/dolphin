// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoBackends/Software/NativeVertexFormat.h"

class PointerWrap;

class SetupUnit
{
	u8 m_PrimType;
	int m_VertexCounter;

	OutputVertexData m_Vertices[3];
	OutputVertexData *m_VertPointer[3];
	OutputVertexData *m_VertWritePointer;

	void SetupQuad();
	void SetupTriangle();
	void SetupTriStrip();
	void SetupTriFan();
	void SetupLine();
	void SetupLineStrip();
	void SetupPoint();

public:
	void Init(u8 primitiveType);

	OutputVertexData* GetVertex() { return m_VertWritePointer; }

	void SetupVertex();
	void DoState(PointerWrap &p);
};
