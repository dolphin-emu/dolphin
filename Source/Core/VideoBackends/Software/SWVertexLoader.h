// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/NativeVertexFormat.h"

#include "VideoCommon/VertexLoader.h"

class PointerWrap;
class SetupUnit;

class SWVertexLoader
{
	u32 m_VertexSize;

	VAT* m_CurrentVat;

	InputVertexData m_Vertex;

	void ParseVertex(const PortableVertexDeclaration& vdec);

	SetupUnit *m_SetupUnit;

	bool m_TexGenSpecialCase;

	std::map<VertexLoaderUID, std::unique_ptr<VertexLoader>> m_VertexLoaderMap;
	std::vector<u8> m_LoadedVertices;
	VertexLoader* m_CurrentLoader;

	u8 m_attributeIndex;
	u8 m_primitiveType;

public:
	SWVertexLoader();
	~SWVertexLoader();

	void SetFormat(u8 attributeIndex, u8 primitiveType);

	u32 GetVertexSize() { return m_VertexSize; }

	void LoadVertex();
	void DoState(PointerWrap &p);
};
