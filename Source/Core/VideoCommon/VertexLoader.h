// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Top vertex loaders
// Metroid Prime: P I16-flt N I16-s16 T0 I16-u16 T1 i16-flt

#include <algorithm>
#include <string>

#include "Common/Common.h"
#include "Common/x64Emitter.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/NativeVertexFormat.h"

#ifndef _M_GENERIC
#ifndef __APPLE__
#define USE_VERTEX_LOADER_JIT
#endif
#endif

class VertexLoaderUID
{
	u32 vid[5];
	size_t hash;
public:
	VertexLoaderUID()
	{
	}

	void InitFromCurrentState(int vtx_attr_group)
	{
		vid[0] = g_VtxDesc.Hex & 0xFFFFFFFF;
		vid[1] = g_VtxDesc.Hex >> 32;
		vid[2] = g_VtxAttr[vtx_attr_group].g0.Hex & ~VAT_0_FRACBITS;
		vid[3] = g_VtxAttr[vtx_attr_group].g1.Hex & ~VAT_1_FRACBITS;
		vid[4] = g_VtxAttr[vtx_attr_group].g2.Hex & ~VAT_2_FRACBITS;
		hash = CalculateHash();
	}

	bool operator < (const VertexLoaderUID &other) const
	{
		// This is complex because of speed.
		if (vid[0] < other.vid[0])
			return true;
		else if (vid[0] > other.vid[0])
			return false;

		for (int i = 1; i < 5; ++i)
		{
			if (vid[i] < other.vid[i])
				return true;
			else if (vid[i] > other.vid[i])
				return false;
		}

		return false;
	}

	bool operator == (const VertexLoaderUID& rh) const
	{
		return hash == rh.hash && std::equal(vid, vid + sizeof(vid) / sizeof(vid[0]), rh.vid);
	}

	size_t GetHash() const
	{
		return hash;
	}

private:

	size_t CalculateHash()
	{
		size_t h = -1;

		for (auto word : vid)
		{
			h = h * 137 + word;
		}

		return h;
	}
};

// ARMTODO: This should be done in a better way
#ifndef _M_GENERIC
class VertexLoader : public Gen::X64CodeBlock
#else
class VertexLoader
#endif
{
public:
	VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	~VertexLoader();

	int GetVertexSize() const {return m_VertexSize;}

	void SetupRunVertices(int vtx_attr_group, int primitive, int const count);
	void RunVertices(int vtx_attr_group, int primitive, int count);

	// For debugging / profiling
	void AppendToString(std::string *dest) const;
	int GetNumLoadedVerts() const { return m_numLoadedVertices; }

private:
	int m_VertexSize;      // number of bytes of a raw GC vertex. Computed by CompileVertexTranslator.

	// GC vertex format
	TVtxAttr m_VtxAttr;  // VAT decoded into easy format
	TVtxDesc m_VtxDesc;  // Not really used currently - or well it is, but could be easily avoided.

	// PC vertex format
	NativeVertexFormat *m_NativeFmt;
	int native_stride;

#ifndef USE_VERTEX_LOADER_JIT
	// Pipeline.
	TPipelineFunction m_PipelineStages[64];  // TODO - figure out real max. it's lower.
	int m_numPipelineStages;
#endif

	const u8 *m_compiledCode;

	int m_numLoadedVertices;

	void SetVAT(u32 _group0, u32 _group1, u32 _group2);

	void CompileVertexTranslator();
	void ConvertVertices(int count);

	void WriteCall(TPipelineFunction);

#ifndef _M_GENERIC
	void WriteGetVariable(int bits, Gen::OpArg dest, void *address);
	void WriteSetVariable(int bits, void *address, Gen::OpArg dest);
#endif
};
