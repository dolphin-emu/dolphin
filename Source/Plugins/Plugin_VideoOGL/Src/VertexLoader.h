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

using namespace std;

#include "CPMemory.h"
#include "DataReader.h"

#define LOADERDECL __cdecl
typedef void (LOADERDECL *TPipelineFunction)(void*);

// There are 8 of these. Most games only use the first, and just reconfigure it all the time
// as needed, unfortunately.
// TODO - clarify the role of this class. It seems to have taken on some irrelevant stuff.
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
    TPipelineFunction m_PipelineStages[32];
    int m_numPipelineStages;

    int m_VertexSize;      // number of bytes of a raw GC vertex
    int m_VBVertexStride;  // PC-side vertex stride
	int m_VBStridePad;

    u32 m_components;  // VB_HAS_X. Bitmask telling what vertex components are present.

	// Raw VAttr
    UVAT_group0 m_group0;
    UVAT_group1 m_group1;
    UVAT_group2 m_group2;
    TVtxAttr m_VtxAttr;  // Decoded into easy format

    u8* m_compiledCode;

    // Common for all loaders  (? then why is it here?)
    TVtxDesc m_VtxDesc;

    void SetupColor(int num, int _iMode, int _iFormat, int _iElements);
    void SetupTexCoord(int num, int _iMode, int _iFormat, int _iElements, int _iFrac);

	// The 3 possible values (0, 1, 2) should be documented here.
	enum {
		AD_CLEAN = 0,
		AD_DIRTY = 1,
		AD_VAT_DIRTY = 2,
	} m_AttrDirty;

public:
    // constructor
    VertexLoader();
    ~VertexLoader();
    
    // run the pipeline 
    void PrepareForVertexFormat();
    void RunVertices(int primitive, int count);
    void WriteCall(void  (LOADERDECL *func)(void *));
    
    int GetGCVertexSize()   const { _assert_( !m_AttrDirty ); return m_VertexSize; }
    int GetVBVertexStride() const { _assert_( !m_AttrDirty);  return m_VBVertexStride; }

    int ComputeVertexSize();

    void SetVAT_group0(u32 _group0) 
    {
        if ((m_group0.Hex & ~0x3e0001f0) != (_group0 & ~0x3e0001f0)) {
            m_AttrDirty = AD_VAT_DIRTY;
        }
        m_group0.Hex = _group0;

        m_VtxAttr.PosElements			= m_group0.PosElements;
        m_VtxAttr.PosFormat				= m_group0.PosFormat;
        m_VtxAttr.PosFrac				= m_group0.PosFrac;
        m_VtxAttr.NormalElements		= m_group0.NormalElements;
        m_VtxAttr.NormalFormat			= m_group0.NormalFormat;
        m_VtxAttr.color[0].Elements		= m_group0.Color0Elements;
        m_VtxAttr.color[0].Comp			= m_group0.Color0Comp;
        m_VtxAttr.color[1].Elements		= m_group0.Color1Elements;
        m_VtxAttr.color[1].Comp			= m_group0.Color1Comp;
        m_VtxAttr.texCoord[0].Elements	= m_group0.Tex0CoordElements;
        m_VtxAttr.texCoord[0].Format	= m_group0.Tex0CoordFormat;
        m_VtxAttr.texCoord[0].Frac		= m_group0.Tex0Frac;
        m_VtxAttr.ByteDequant			= m_group0.ByteDequant;
        m_VtxAttr.NormalIndex3			= m_group0.NormalIndex3;
    };

    void SetVAT_group1(u32 _group1) 
    {
        if ((m_group1.Hex & ~0x7c3e1f0) != (_group1 & ~0x7c3e1f0)) {
            m_AttrDirty = AD_VAT_DIRTY;
        }
        m_group1.Hex = _group1;

        m_VtxAttr.texCoord[1].Elements	= m_group1.Tex1CoordElements;
        m_VtxAttr.texCoord[1].Format	= m_group1.Tex1CoordFormat;
        m_VtxAttr.texCoord[1].Frac		= m_group1.Tex1Frac;
        m_VtxAttr.texCoord[2].Elements	= m_group1.Tex2CoordElements;
        m_VtxAttr.texCoord[2].Format	= m_group1.Tex2CoordFormat;
        m_VtxAttr.texCoord[2].Frac		= m_group1.Tex2Frac;
        m_VtxAttr.texCoord[3].Elements	= m_group1.Tex3CoordElements;
        m_VtxAttr.texCoord[3].Format	= m_group1.Tex3CoordFormat;
        m_VtxAttr.texCoord[3].Frac      = m_group1.Tex3Frac;
        m_VtxAttr.texCoord[4].Elements	= m_group1.Tex4CoordElements;
        m_VtxAttr.texCoord[4].Format	= m_group1.Tex4CoordFormat;
    };									  
                                          
    void SetVAT_group2(u32 _group2)		  
    {
        if ((m_group2.Hex & ~0xf87c3e1f) != (_group2 & ~0xf87c3e1f)) {
            m_AttrDirty = AD_VAT_DIRTY;
        }
        m_group2.Hex = _group2;

        m_VtxAttr.texCoord[4].Frac		= m_group2.Tex4Frac;
        m_VtxAttr.texCoord[5].Elements	= m_group2.Tex5CoordElements;
        m_VtxAttr.texCoord[5].Format	= m_group2.Tex5CoordFormat;
        m_VtxAttr.texCoord[5].Frac		= m_group2.Tex5Frac;
        m_VtxAttr.texCoord[6].Elements	= m_group2.Tex6CoordElements;
        m_VtxAttr.texCoord[6].Format	= m_group2.Tex6CoordFormat;
        m_VtxAttr.texCoord[6].Frac		= m_group2.Tex6Frac;
        m_VtxAttr.texCoord[7].Elements	= m_group2.Tex7CoordElements;
        m_VtxAttr.texCoord[7].Format	= m_group2.Tex7CoordFormat;
        m_VtxAttr.texCoord[7].Frac		= m_group2.Tex7Frac;
    };
};									  

extern VertexLoader g_VertexLoaders[8];

#endif
