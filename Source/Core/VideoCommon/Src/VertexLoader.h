// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _VERTEXLOADER_H
#define _VERTEXLOADER_H

// Top vertex loaders
// Metroid Prime: P I16-flt N I16-s16 T0 I16-u16 T1 i16-flt

#include <string>

#include "Common.h"

#include "CPMemory.h"
#include "DataReader.h"
#include "NativeVertexFormat.h"

#include "x64Emitter.h"

class VertexLoaderUID
{
	u32 vid[5];
public:
	VertexLoaderUID() {}
	void InitFromCurrentState(int vtx_attr_group) {
		vid[0] = g_VtxDesc.Hex & 0xFFFFFFFF;
		vid[1] = g_VtxDesc.Hex >> 32;
		vid[2] = g_VtxAttr[vtx_attr_group].g0.Hex & ~VAT_0_FRACBITS;
		vid[3] = g_VtxAttr[vtx_attr_group].g1.Hex & ~VAT_1_FRACBITS;
		vid[4] = g_VtxAttr[vtx_attr_group].g2.Hex & ~VAT_2_FRACBITS;
	}
	bool operator < (const VertexLoaderUID &other) const {
		if (vid[0] < other.vid[0])
			return true;
		else if (vid[0] > other.vid[0])
			return false;
		for (int i = 1; i < 5; ++i) {
			if (vid[i] < other.vid[i])
				return true;
			else if (vid[i] > other.vid[i])
				return false;
		}
		return false;
	}
};

class VertexLoader : public Gen::XCodeBlock
{
public:
	VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	~VertexLoader();

	int GetVertexSize() const {return m_VertexSize;}
	void RunVertices(int vtx_attr_group, int primitive, int count);

	// For debugging / profiling
	void AppendToString(std::string *dest) const;
	int GetNumLoadedVerts() const { return m_numLoadedVertices; }

private:
	enum
	{
		NRM_ZERO = 0,
		NRM_ONE = 1,
		NRM_THREE = 3,
	};

	int m_VertexSize;      // number of bytes of a raw GC vertex. Computed by CompileVertexTranslator.

	// GC vertex format
	TVtxAttr m_VtxAttr;  // VAT decoded into easy format
	TVtxDesc m_VtxDesc;  // Not really used currently - or well it is, but could be easily avoided.

	// PC vertex format
	NativeVertexFormat *m_NativeFmt;
	int native_stride;

	// Pipeline. To be JIT compiled in the future.
	TPipelineFunction m_PipelineStages[64];  // TODO - figure out real max. it's lower.
	int m_numPipelineStages;

	const u8 *m_compiledCode;

	int m_numLoadedVertices;

	void SetVAT(u32 _group0, u32 _group1, u32 _group2);

	void CompileVertexTranslator();

	void WriteCall(TPipelineFunction);

	void WriteGetVariable(int bits, Gen::OpArg dest, void *address);
	void WriteSetVariable(int bits, void *address, Gen::OpArg dest);

	DISALLOW_COPY_AND_ASSIGN(VertexLoader);
};									  

#endif
