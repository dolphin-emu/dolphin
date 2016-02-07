// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoaderBase.h"

class DataReader;

class VertexLoaderARM64 : public VertexLoaderBase, public Arm64Gen::ARM64CodeBlock
{
public:
	VertexLoaderARM64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
	std::string GetName() const override { return "VertexLoaderARM64"; }
	bool IsInitialized() override { return true; }
	int RunVertices(DataReader src, DataReader dst, int count) override;

private:
	u32 m_src_ofs = 0;
	u32 m_dst_ofs = 0;
	Arm64Gen::FixupBranch m_skip_vertex;
	Arm64Gen::ARM64FloatEmitter m_float_emit;
	void GetVertexAddr(int array, u64 attribute, Arm64Gen::ARM64Reg reg);
	s32 GetAddressImm(int array, u64 attribute, Arm64Gen::ARM64Reg reg, u32 align);
	int ReadVertex(u64 attribute, int format, int count_in, int count_out, bool dequantize, u8 scaling_exponent, AttributeFormat* native_format, s32 offset = -1);
	void ReadColor(u64 attribute, int format, s32 offset);
	void GenerateVertexLoader();
};
