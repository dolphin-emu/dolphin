#include <stdio.h>

#include "x64Emitter.h"

#include "Common.h"
#include "VertexHandler.h"
#include "VertexLoader.h"
#include "XFStructs.h"
#include "BPStructs.h"
#include "DataReader.h"
#include "DecodedVArray.h"

//these don't need to be saved
float posScale;
float tcScale[8];
int tcElements[8];
int tcFormat[8];
int colElements[2];
float tcScaleU[8];
float tcScaleV[8];
int tcIndex;
int colIndex;
u32 addr;

DecodedVArray *varray;

int ComputeVertexSize(u32 comp)
{
	int size = 0;
	if (comp & VertexLoader::VB_HAS_POSMTXIDX)
		size += 4;
	if (comp & (VertexLoader::VB_HAS_TEXMTXIDX0 | VertexLoader::VB_HAS_TEXMTXIDX1 | VertexLoader::VB_HAS_TEXMTXIDX2 | VertexLoader::VB_HAS_TEXMTXIDX3))
		size += 4;
	if (comp & (VertexLoader::VB_HAS_TEXMTXIDX4 | VertexLoader::VB_HAS_TEXMTXIDX5 | VertexLoader::VB_HAS_TEXMTXIDX6 | VertexLoader::VB_HAS_TEXMTXIDX7))
		size += 4;
	if (comp & VertexLoader::VB_HAS_NRM0)
		size += 4;
	if (comp & (VertexLoader::VB_HAS_NRM1 | VertexLoader::VB_HAS_NRM2)) //combine into single check for speed
		size += 8;
	if (comp & VertexLoader::VB_HAS_COL0)
		size += 4; 
	if (comp & VertexLoader::VB_HAS_COL1)
		size += 4;
	for (int i = 0; i < 8; i++)
		if (comp & (VertexLoader::VB_HAS_UV0 << i))
			size += 8;
	return size;
}

void VertexLoader::SetVArray(DecodedVArray *_varray)
{
	varray = _varray;
}
/*
inline u8    ReadBuffer8()   { return fifo.Read8();   }
inline u16   ReadBuffer16()  { return fifo.Read16();  }
inline u32   ReadBuffer32()  { return fifo.Read32();  }
inline float ReadBuffer32F() { return fifo.Read32F(); }
*/
inline u8 ReadBuffer8()
{
	return g_pDataReader->Read8();
}

inline u16 ReadBuffer16()
{
	//PowerPC byte ordering :(
	return g_pDataReader->Read16();
}

inline u32 ReadBuffer32()
{
	//PowerPC byte ordering :(
	return g_pDataReader->Read32();
}

inline float ReadBuffer32F()
{
	u32 temp = g_pDataReader->Read32();
	return *(float*)(&temp);
}

#include "VertexLoader_MtxIndex.h"
#include "VertexLoader_Position.h"
#include "VertexLoader_Normal.h"
#include "VertexLoader_Color.h"
#include "VertexLoader_textCoord.h"

VertexLoader g_VertexLoaders[8];
TVtxDesc VertexLoader::m_VtxDesc;
bool VertexLoader::m_DescDirty = true;

VertexLoader::VertexLoader() 
{
	m_numPipelineStates = 0;
	m_VertexSize = 0;
	m_AttrDirty = true;
	VertexLoader_Normal::Init();
}

VertexLoader::~VertexLoader() 
{

} 

void VertexLoader::Setup()
{
	if (!m_AttrDirty && !m_DescDirty)
		return;

    DVSTARTPROFILE();

	// Reset pipeline
	m_VertexSize = 0;
	m_numPipelineStates = 0;
	m_components = 0;

	// Position Matrix Index
	if (m_VtxDesc.PosMatIdx) 
	{
		m_PipelineStates[m_numPipelineStates++] = PosMtx_ReadDirect_UByte;
		m_VertexSize += 1;
		m_components |= VB_HAS_POSMTXIDX;
	}

	// Texture matrix indices
	if (m_VtxDesc.Tex0MatIdx) {m_components|=VB_HAS_TEXMTXIDX0; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex1MatIdx) {m_components|=VB_HAS_TEXMTXIDX1; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex2MatIdx) {m_components|=VB_HAS_TEXMTXIDX2; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex3MatIdx) {m_components|=VB_HAS_TEXMTXIDX3; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex4MatIdx) {m_components|=VB_HAS_TEXMTXIDX4; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex5MatIdx) {m_components|=VB_HAS_TEXMTXIDX5; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex6MatIdx) {m_components|=VB_HAS_TEXMTXIDX6; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	if (m_VtxDesc.Tex7MatIdx) {m_components|=VB_HAS_TEXMTXIDX7; WriteCall(TexMtx_ReadDirect_UByte); m_VertexSize+=1;}
	
	// Position
	switch (m_VtxDesc.Position)
	{
	case NOT_PRESENT:	{_assert_msg_(0,"Vertex descriptor without position!","WTF?");} break;
	case DIRECT:	
		{
			int SizePro = 0;
			switch (m_VtxAttr.PosFormat)
			{
			case FORMAT_UBYTE:	SizePro=1; WriteCall(Pos_ReadDirect_UByte); break;
			case FORMAT_BYTE:	SizePro=1; WriteCall(Pos_ReadDirect_Byte); break;
			case FORMAT_USHORT:	SizePro=2; WriteCall(Pos_ReadDirect_UShort); break;
			case FORMAT_SHORT:	SizePro=2; WriteCall(Pos_ReadDirect_Short); break;
			case FORMAT_FLOAT:	SizePro=4; WriteCall(Pos_ReadDirect_Float); break;
			default: _assert_(0); break;
			}
			if (m_VtxAttr.PosElements == 1)
				m_VertexSize += SizePro * 3;
			else
				m_VertexSize += SizePro * 2;
		}
		break;
	case INDEX8:		
		m_VertexSize+=1;
		switch (m_VtxAttr.PosFormat)
		{
		case FORMAT_UBYTE:	WriteCall(Pos_ReadIndex8_UByte); break; //WTF?
		case FORMAT_BYTE:	WriteCall(Pos_ReadIndex8_Byte); break;
		case FORMAT_USHORT:	WriteCall(Pos_ReadIndex8_UShort); break;
		case FORMAT_SHORT:	WriteCall(Pos_ReadIndex8_Short); break;
		case FORMAT_FLOAT:	WriteCall(Pos_ReadIndex8_Float); break;
		default: _assert_(0); break;
		}
		break;
	case INDEX16:
		m_VertexSize+=2;
		switch (m_VtxAttr.PosFormat)
		{
		case FORMAT_UBYTE:	WriteCall(Pos_ReadIndex16_UByte); break;
		case FORMAT_BYTE:	WriteCall(Pos_ReadIndex16_Byte); break;
		case FORMAT_USHORT:	WriteCall(Pos_ReadIndex16_UShort); break;
		case FORMAT_SHORT:	WriteCall(Pos_ReadIndex16_Short); break;
		case FORMAT_FLOAT:	WriteCall(Pos_ReadIndex16_Float); break;
		default: _assert_(0); break;
		}
		break;
	}

	// Normals
	if (m_VtxDesc.Normal != NOT_PRESENT)
	{
		VertexLoader_Normal::index3 = m_VtxAttr.NormalIndex3 ? true : false;
		unsigned int uSize = VertexLoader_Normal::GetSize(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements);
		TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements);
		if (pFunc == 0)
		{
			char temp[256];
			sprintf(temp,"%i %i %i", m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements);
			MessageBox(0,"VertexLoader_Normal::GetFunction returned zero!",temp,0);
		}
		WriteCall(pFunc);
		m_VertexSize += uSize;
		int m_numNormals = (m_VtxAttr.NormalElements==1) ? NRM_THREE : NRM_ONE;
		m_components |= VB_HAS_NRM0;
		if (m_numNormals == NRM_THREE)
			m_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}
	
	// Colors
	int col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	for (int i = 0; i < 2; i++)
		SetupColor(i,col[i], m_VtxAttr.color[i].Comp, m_VtxAttr.color[i].Elements);

	// TextureCoord
	int tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord,
	};
	for (int i = 0; i < 8; i++)
		SetupTexCoord(i, tc[i], 
		              m_VtxAttr.texCoord[i].Format, 
		              m_VtxAttr.texCoord[i].Elements,
					  m_VtxAttr.texCoord[i].Frac);

	Compile();
	//RET();
/*
	char temp[256];
	sprintf(temp,"Size: %08x Pos: %i Norm:%i Col0: %i Col1: %i",m_VertexSize,m_VtxDesc.Position,m_VtxDesc.Normal,m_VtxDesc.Color0,m_VtxDesc.Color1);
	g_VideoInitialize.pLog(temp);
	sprintf(temp,"Pos: %i Norm:%i Col0: %i Col1: %i",_VtxAttr.PosFormat,_VtxAttr.NormalFormat,_VtxAttr.color[0].Comp,_VtxAttr.color[1].Comp);
	g_VideoInitialize.pLog(temp);*/
}

void VertexLoader::SetupColor(int num, int mode, int format, int elements)
{
	m_components |= VB_HAS_COL0 << num;
	switch (mode)
	{
	case NOT_PRESENT: 
		m_components &= ~(VB_HAS_COL0 << num);
		break;
	case DIRECT:
		switch (format)
		{
		case FORMAT_16B_565:	m_VertexSize+=2; WriteCall(Color_ReadDirect_16b_565); break;
		case FORMAT_24B_888:	m_VertexSize+=3; WriteCall(Color_ReadDirect_24b_888); break;
		case FORMAT_32B_888x:	m_VertexSize+=4; WriteCall(Color_ReadDirect_32b_888x); break;
		case FORMAT_16B_4444:	m_VertexSize+=2; WriteCall(Color_ReadDirect_16b_4444); break;
		case FORMAT_24B_6666:	m_VertexSize+=3; WriteCall(Color_ReadDirect_24b_6666); break;
		case FORMAT_32B_8888:	m_VertexSize+=4; WriteCall(Color_ReadDirect_32b_8888); break;
		default: _assert_(0); break;
		}
		break;
	case INDEX8:	
		switch (format)
		{
		case FORMAT_16B_565:	WriteCall(Color_ReadIndex8_16b_565); break;
		case FORMAT_24B_888:	WriteCall(Color_ReadIndex8_24b_888); break;
		case FORMAT_32B_888x:	WriteCall(Color_ReadIndex8_32b_888x); break;
		case FORMAT_16B_4444:	WriteCall(Color_ReadIndex8_16b_4444); break;
		case FORMAT_24B_6666:	WriteCall(Color_ReadIndex8_24b_6666); break;
		case FORMAT_32B_8888:	WriteCall(Color_ReadIndex8_32b_8888); break;
		default: _assert_(0); break;
		}
		m_VertexSize+=1;
		break;
	case INDEX16:
		switch (format)
		{
		case FORMAT_16B_565:	WriteCall(Color_ReadIndex16_16b_565); break;
		case FORMAT_24B_888:	WriteCall(Color_ReadIndex16_24b_888); break;
		case FORMAT_32B_888x:	WriteCall(Color_ReadIndex16_32b_888x); break;
		case FORMAT_16B_4444:	WriteCall(Color_ReadIndex16_16b_4444); break;
		case FORMAT_24B_6666:	WriteCall(Color_ReadIndex16_24b_6666); break;
		case FORMAT_32B_8888:	WriteCall(Color_ReadIndex16_32b_8888); break;
		default: _assert_(0); break;
		}
		m_VertexSize+=2;
		break;
	}
}

void VertexLoader::SetupTexCoord(int num, int mode, int format, int elements, int _iFrac)
{
	m_components |= VB_HAS_UV0 << num;
	switch (mode)
	{
	case NOT_PRESENT: 
		m_components &= ~(VB_HAS_UV0 << num);
		break;
	case DIRECT:
		{
			int sizePro=0;
			switch (format)
			{
			case FORMAT_UBYTE:	sizePro=1; WriteCall(TexCoord_ReadDirect_UByte);  break;
			case FORMAT_BYTE:	sizePro=1; WriteCall(TexCoord_ReadDirect_Byte);   break;
			case FORMAT_USHORT:	sizePro=2; WriteCall(TexCoord_ReadDirect_UShort); break;
			case FORMAT_SHORT:	sizePro=2; WriteCall(TexCoord_ReadDirect_Short);  break;
			case FORMAT_FLOAT:	sizePro=4; WriteCall(TexCoord_ReadDirect_Float);  break;
			default: _assert_(0); break;
			}
			m_VertexSize += sizePro * (elements?2:1);
		}
		break;
	case INDEX8:	
		switch (format)
		{
		case FORMAT_UBYTE:	WriteCall(TexCoord_ReadIndex8_UByte);  break;
		case FORMAT_BYTE:	WriteCall(TexCoord_ReadIndex8_Byte);   break;
		case FORMAT_USHORT:	WriteCall(TexCoord_ReadIndex8_UShort); break;
		case FORMAT_SHORT:	WriteCall(TexCoord_ReadIndex8_Short);  break;
		case FORMAT_FLOAT:	WriteCall(TexCoord_ReadIndex8_Float);  break;
		default: _assert_(0); break;
		}
		m_VertexSize+=1;
		break;
	case INDEX16:
		switch (format)
		{
		case FORMAT_UBYTE:	WriteCall(TexCoord_ReadIndex16_UByte);  break;
		case FORMAT_BYTE:	WriteCall(TexCoord_ReadIndex16_Byte);   break;
		case FORMAT_USHORT:	WriteCall(TexCoord_ReadIndex16_UShort); break;
		case FORMAT_SHORT:	WriteCall(TexCoord_ReadIndex16_Short);  break;
		case FORMAT_FLOAT:	WriteCall(TexCoord_ReadIndex16_Float);  break;
		default: _assert_(0);
		}
		m_VertexSize+=2;
		break;
	}
}

void VertexLoader::WriteCall(void  (LOADERDECL *func)(void *))
{
	m_PipelineStates[m_numPipelineStates++] = func;;
}

using namespace Gen;

// See comment in RunVertices
void VertexLoader::Compile()
{
	return;
/*	Gen::SetCodePtr(m_compiledCode);
	//INT3();
	Gen::Util::EmitPrologue(0);
	
	const u8 *loopStart = Gen::GetCodePtr();
	MOV(32, M(&tcIndex), Imm32(0));
	MOV(32, M(&colIndex), Imm32(0));

	for (int i = 0; i < m_numPipelineStates; i++)
	{
		PUSH(32, Imm32((u32)&m_VtxAttr));
		CALL(m_PipelineStates[i]);
		ADD(32, R(ESP), Imm8(4));
	}

	ADD(32, M(&varray->count), Imm8(1));
	SUB(32, M(&this->m_counter), Imm8(1));
	J_CC(CC_NZ, loopStart, true);
	// Epilogue
	Gen::Util::EmitEpilogue(0);
	if (Gen::GetCodePtr()-(u8*)m_compiledCode > sizeof(m_compiledCode))
	{
		Crash();
	}*/
}

void VertexLoader::PrepareRun()
{
	posScale = shiftLookup[m_VtxAttr.PosFrac];
	for (int i = 0; i < 8; i++)
	{
		tcScaleU[i]   = shiftLookup[m_VtxAttr.texCoord[i].Frac];
		tcScaleV[i]   = shiftLookup[m_VtxAttr.texCoord[i].Frac];
		tcElements[i] = m_VtxAttr.texCoord[i].Elements;
		tcFormat[i]   = m_VtxAttr.texCoord[i].Format;
	}
	for (int i = 0; i < 2; i++)
		colElements[i] = m_VtxAttr.color[i].Elements;

	varray->Reset();
	varray->SetComponents(m_components);
}

void VertexLoader::RunVertices(int count)
{
    DVSTARTPROFILE();

	for (int v = 0; v < count; v++)
	{
		tcIndex = 0;
		colIndex = 0;
		s_texmtxread = 0;
		for (int i = 0; i < m_numPipelineStates; i++)
		{
			m_PipelineStates[i](&m_VtxAttr);
		}
		varray->Next();
	}
	/*
	This is not the bottleneck ATM, so compiling etc doesn't really help.
	At least not when all we do is compile it to a list of function calls. 
	Should help more when we inline, but this requires the new vertex format.
	Maybe later, and with smarter caching.

	if (count)
	{
		this->m_counter = count;
		((void (*)())((void*)&m_compiledCode[0]))();
	}*/
}