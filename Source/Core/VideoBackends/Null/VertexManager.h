// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "VideoCommon/VertexManagerBase.h"

namespace Null
{

class VertexManager : public VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();
	NativeVertexFormat* CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;

protected:
	void ResetBuffer(u32 stride) override;
private:
	void vFlush(bool useDstAlpha) override;
	std::vector<u8> LocalVBuffer;
	std::vector<u16> LocalIBuffer;
};

}
