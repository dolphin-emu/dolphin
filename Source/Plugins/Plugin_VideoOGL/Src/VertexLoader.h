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

#include "CPMemory.h"
#include "DataReader.h"

#include "NativeVertexFormat.h"

class VertexLoader
{
public:
    enum
    {
        NRM_ZERO = 0,
        NRM_ONE = 1,
        NRM_THREE = 3
    };

private:
	// The 3 possible values (0, 1, 2) should be documented here.
	enum {
		AD_CLEAN = 0,
		AD_DIRTY = 1,
		AD_VAT_DIRTY = 2,
	} m_AttrDirty;

    int m_VertexSize;      // number of bytes of a raw GC vertex

	// Flipper vertex format

	// Raw VAttr
    UVAT_group0 m_group0;
    UVAT_group1 m_group1;
    UVAT_group2 m_group2;
    TVtxAttr m_VtxAttr;  // Decoded into easy format

    // Vtx desc
    TVtxDesc m_VtxDesc;

	// PC vertex format, + converter
	NativeVertexFormat m_NativeFmt;

	// Pipeline. To be JIT compiled in the future.
	TPipelineFunction m_PipelineStages[32];
	int m_numPipelineStages;

    void SetupColor(int num, int _iMode, int _iFormat, int _iElements);
    void SetupTexCoord(int num, int _iMode, int _iFormat, int _iElements, int _iFrac);
	void RunPipelineOnce() const;

public:
    // constructor
    VertexLoader();
    ~VertexLoader();
    
    // run the pipeline 
    void CompileVertexTranslator();
    void RunVertices(int primitive, int count);
    void WriteCall(TPipelineFunction);
    
    int GetGCVertexSize()   const { _assert_( !m_AttrDirty ); return m_VertexSize; }
    int GetVBVertexStride() const { _assert_( !m_AttrDirty);  return m_NativeFmt.m_VBVertexStride; }

    int ComputeVertexSize();

    void SetVAT_group0(u32 _group0);
    void SetVAT_group1(u32 _group1);       
    void SetVAT_group2(u32 _group2);
};									  

#endif
