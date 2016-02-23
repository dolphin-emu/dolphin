// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "VideoCommon/VertexLoaderBase.h"

class VertexLoaderX64 : public VertexLoaderBase, public Gen::X64CodeBlock
{
public:
	VertexLoaderX64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
	std::string GetName() const override { return "VertexLoaderX64"; }
	bool IsInitialized() override { return true; }
	int RunVertices(DataReader src, DataReader dst, int count) override;

private:
	u32 m_src_ofs = 0;
	u32 m_dst_ofs = 0;
	Gen::FixupBranch m_skip_vertex;
	Gen::OpArg GetVertexAddr(int array, u64 attribute);
	int ReadVertex(Gen::OpArg data, u64 attribute, int format, int count_in, int count_out, bool dequantize, u8 scaling_exponent, AttributeFormat* native_format);
	void ReadColor(Gen::OpArg data, u64 attribute, int format);
	void GenerateVertexLoader();
};
