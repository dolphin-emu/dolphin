// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoBackends/Software/NativeVertexFormat.h"

#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexManagerBase.h"

class SetupUnit;

class SWVertexLoader : public VertexManagerBase
{
public:
	SWVertexLoader();
	~SWVertexLoader();

	NativeVertexFormat* CreateNativeVertexFormat(const PortableVertexDeclaration& vdec) override;

protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() { return &LocalIBuffer[0]; }
private:
	void vFlush(bool useDstAlpha) override;
	std::vector<u8> LocalVBuffer;
	std::vector<u16> LocalIBuffer;

	InputVertexData m_Vertex;

	void ParseVertex(const PortableVertexDeclaration& vdec, int index);

	SetupUnit *m_SetupUnit;

	bool m_TexGenSpecialCase;

public:

	void SetFormat(u8 attributeIndex, u8 primitiveType);
};
