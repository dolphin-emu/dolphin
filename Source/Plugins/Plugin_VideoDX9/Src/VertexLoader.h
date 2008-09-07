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

/*
Ideas for new intermediate vertex format

Produce a mask specifying with components are present.

Decoder always produces a canonical format for all components:

Ubyte4 for matrix indices (x,y,z,w) and colors

FloatN for all others, texcoords are always XY at this stage

The decoders will write a continuous stream of vertices, to take maximum
advantage of write combining hardware

a map<mask, vdecl> will keep track of vertex declarations

this time, we are going to minimize transfers to the gfx card

it turns out that with PS2.0 we can easily do all the pipeline in hardware.
The decision will be how granular to be with the number of shaders and lighting settin

*/

int ComputeVertexSize(u32 components);

#include "CPStructs.h"
#include "DecodedVArray.h"

#define LOADERDECL __cdecl
typedef void (LOADERDECL *TPipelineFunction)(void*);

class VertexLoader
{
public:
	enum
	{
		NRM_ZERO = 0,
		NRM_ONE = 1,
		NRM_THREE = 3
	};

	enum {
		VB_HAS_POSMTXIDX =(1<<1),
		VB_HAS_TEXMTXIDX0=(1<<2),
		VB_HAS_TEXMTXIDX1=(1<<3),
		VB_HAS_TEXMTXIDX2=(1<<4),
		VB_HAS_TEXMTXIDX3=(1<<5),
		VB_HAS_TEXMTXIDX4=(1<<6),
		VB_HAS_TEXMTXIDX5=(1<<7),
		VB_HAS_TEXMTXIDX6=(1<<8),
		VB_HAS_TEXMTXIDX7=(1<<9),
		VB_HAS_TEXMTXIDXALL=(0xff<<2),
		//VB_HAS_POS=0, // Implied, it always has pos! don't bother testing
		VB_HAS_NRM0=(1<<10),
		VB_HAS_NRM1=(1<<11),
		VB_HAS_NRM2=(1<<12),
		VB_HAS_NRMALL=(7<<10),

		VB_HAS_COL0=(1<<13),
		VB_HAS_COL1=(1<<14),

		VB_HAS_UV0=(1<<15),
		VB_HAS_UV1=(1<<16),
		VB_HAS_UV2=(1<<17),
		VB_HAS_UV3=(1<<18),
		VB_HAS_UV4=(1<<19),
		VB_HAS_UV5=(1<<20),
		VB_HAS_UV6=(1<<21),
		VB_HAS_UV7=(1<<22),
		VB_HAS_UVALL=(0xff<<15),
		VB_HAS_UVTEXMTXSHIFT=13,
	};

private:	
	TPipelineFunction m_PipelineStates[32];
	int m_numPipelineStates;	
	int m_VertexSize;
	int m_counter;
	u8 m_compiledCode[1024];

	u32 m_components;

	UVAT_group0 m_group0;
	UVAT_group1 m_group1;
	UVAT_group2 m_group2;

	bool m_AttrDirty;

	TVtxAttr m_VtxAttr; //Decoded into easy format

	//common for all loaders
	static TVtxDesc m_VtxDesc;
	static bool m_DescDirty;

	// seup the pipeline with this vertex fmt
	void SetupColor(int num, int _iMode, int _iFormat, int _iElements);
	void SetupTexCoord(int num, int _iMode, int _iFormat, int _iElements, int _iFrac);

public:
	// constructor
	VertexLoader();
	~VertexLoader();
	// run the pipeline 
	static void SetVArray(DecodedVArray *_varray);
	void Setup();
	void Compile();
	void PrepareRun();
	void RunVertices(int count);
	void WriteCall(void  (LOADERDECL *func)(void *));
	int GetVertexSize(){return m_VertexSize;}

	//VtxDesc - global
	static void SetVtxDesc_Lo(u32 _iValue)
	{
		u64 old = m_VtxDesc.Hex;
		m_VtxDesc.Hex &= ~0x1FFFF; // keep the Upper bits
		m_VtxDesc.Hex |= _iValue;
		if (m_VtxDesc.Hex != old)
			m_DescDirty = true;
	};

	static void SetVtxDesc_Hi(u32 _iValue)
	{
		u64 old = m_VtxDesc.Hex;
		m_VtxDesc.Hex &= 0x1FFFF; // keep the lower 17Bits
		m_VtxDesc.Hex |= (u64)_iValue << 17;
		if (m_VtxDesc.Hex != old)
			m_DescDirty = true;
	};

	static TVtxDesc &GetVtxDesc() {return m_VtxDesc;}

	void SetVAT_group0(u32 _group0) 
	{
		if (m_group0.Hex == _group0)
			return;		
		m_AttrDirty = true;
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
		if (m_group1.Hex == _group1)
			return;		
		m_AttrDirty = true;
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
		if (m_group2.Hex == _group2)
			return;		
		m_AttrDirty = true;
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
extern DecodedVArray* varray;
extern inline u8 ReadBuffer8();
extern inline u16 ReadBuffer16();
extern inline u32 ReadBuffer32();
extern inline float ReadBuffer32F();


#endif