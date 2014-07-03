// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"
#include "Common/Common.h"

#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/NativeVertexFormat.h"

class SetupUnit;

class SWVertexLoader
{
	u32 m_VertexSize;

	VAT* m_CurrentVat;

	TPipelineFunction m_positionLoader;
	TPipelineFunction m_normalLoader;
	TPipelineFunction m_colorLoader[2];
	TPipelineFunction m_texCoordLoader[8];

	InputVertexData m_Vertex;

	typedef void (*AttributeLoader)(SWVertexLoader*, InputVertexData*, u8);
	struct AttrLoaderCall
	{
		AttributeLoader loader;
		u8 index;
	};
	AttrLoaderCall m_AttributeLoaders[1+8+1+1+2+8];
	int m_NumAttributeLoaders;
	void AddAttributeLoader(AttributeLoader loader, u8 index=0);

	// attribute loader functions
	static void LoadPosMtx(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 unused);
	static void LoadTexMtx(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 index);
	static void LoadPosition(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 unused);
	static void LoadNormal(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 unused);
	static void LoadColor(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 index);
	static void LoadTexCoord(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 index);

	SetupUnit *m_SetupUnit;

	bool m_TexGenSpecialCase;

public:
	SWVertexLoader();
	~SWVertexLoader();

	void SetFormat(u8 attributeIndex, u8 primitiveType);

	u32 GetVertexSize() { return m_VertexSize; }

	void LoadVertex();
	void DoState(PointerWrap &p);
};
