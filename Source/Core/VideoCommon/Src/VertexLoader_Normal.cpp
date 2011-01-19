// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"
#include "VideoCommon.h"
#include "VertexLoader.h"
#include "VertexLoader_Normal.h"
#include "VertexManagerBase.h"
#include "CPUDetect.h"

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

#define LOG_NORM8()  // PRIM_LOG("norm: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[-3], ((float*)VertexManager::s_pCurBufferPointer)[-2], ((float*)VertexManager::s_pCurBufferPointer)[-1]);
#define LOG_NORM16() // PRIM_LOG("norm: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[-3], ((float*)VertexManager::s_pCurBufferPointer)[-2], ((float*)VertexManager::s_pCurBufferPointer)[-1]);
#define LOG_NORMF()  // PRIM_LOG("norm: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[-3], ((float*)VertexManager::s_pCurBufferPointer)[-2], ((float*)VertexManager::s_pCurBufferPointer)[-1]);

VertexLoader_Normal::Set VertexLoader_Normal::m_Table[NUM_NRM_TYPE][NUM_NRM_INDICES][NUM_NRM_ELEMENTS][NUM_NRM_FORMAT];

void VertexLoader_Normal::Init(void)
{
	// HACK is for signed instead of unsigned to prevent crashes.
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT] [FORMAT_UBYTE] 	= Set(3,  Normal_DirectByte); //HACK
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT] [FORMAT_BYTE]   = Set(3,  Normal_DirectByte);
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT] [FORMAT_USHORT]	= Set(6,  Normal_DirectShort); //HACK
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT] [FORMAT_SHORT] 	= Set(6,  Normal_DirectShort);
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT] [FORMAT_FLOAT] 	= Set(12, Normal_DirectFloat);
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] 	= Set(9,  Normal_DirectByte3); //HACK	
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT3][FORMAT_BYTE]  	= Set(9,  Normal_DirectByte3);
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT3][FORMAT_USHORT]	= Set(18, Normal_DirectShort3); //HACK
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] 	= Set(18, Normal_DirectShort3);
    m_Table[NRM_DIRECT] [NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] 	= Set(36, Normal_DirectFloat3);
								  			
	m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT] [FORMAT_UBYTE] 	= Set(3,  Normal_DirectByte); //HACK
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT] [FORMAT_BYTE]  	= Set(3,  Normal_DirectByte);
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT] [FORMAT_USHORT]	= Set(6,  Normal_DirectShort); //HACK
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT] [FORMAT_SHORT] 	= Set(6,  Normal_DirectShort);
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT] [FORMAT_FLOAT] 	= Set(12, Normal_DirectFloat);
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] 	= Set(9,  Normal_DirectByte3); //HACK	
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT3][FORMAT_BYTE]  	= Set(9,  Normal_DirectByte3);
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT3][FORMAT_USHORT]	= Set(18, Normal_DirectShort3); //HACK
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] 	= Set(18, Normal_DirectShort3);
    m_Table[NRM_DIRECT] [NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] 	= Set(36, Normal_DirectFloat3);
								  			
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT] [FORMAT_UBYTE] 	= Set(1,  Normal_Index8_Byte); //HACK
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT] [FORMAT_BYTE]  	= Set(1,  Normal_Index8_Byte);
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT] [FORMAT_USHORT]	= Set(1,  Normal_Index8_Short); //HACK
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT] [FORMAT_SHORT] 	= Set(1,  Normal_Index8_Short);
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT] [FORMAT_FLOAT] 	= Set(1,  Normal_Index8_Float);
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] 	= Set(1,  Normal_Index8_Byte3_Indices1); //HACK	
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT3][FORMAT_BYTE]  	= Set(1,  Normal_Index8_Byte3_Indices1);
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT3][FORMAT_USHORT]	= Set(1,  Normal_Index8_Short3_Indices1); //HACK
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] 	= Set(1,  Normal_Index8_Short3_Indices1);
    m_Table[NRM_INDEX8] [NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] 	= Set(1,  Normal_Index8_Float3_Indices1);
								  			
	m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT] [FORMAT_UBYTE] 	= Set(1,  Normal_Index8_Byte); //HACK
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT] [FORMAT_BYTE]  	= Set(1,  Normal_Index8_Byte);
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT] [FORMAT_USHORT]	= Set(1,  Normal_Index8_Short); //HACK
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT] [FORMAT_SHORT] 	= Set(1,  Normal_Index8_Short);
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT] [FORMAT_FLOAT] 	= Set(1,  Normal_Index8_Float);
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] 	= Set(3,  Normal_Index8_Byte3_Indices3); //HACK	
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT3][FORMAT_BYTE]  	= Set(3,  Normal_Index8_Byte3_Indices3);
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT3][FORMAT_USHORT]	= Set(3,  Normal_Index8_Short3_Indices3); //HACK
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] 	= Set(3,  Normal_Index8_Short3_Indices3);
    m_Table[NRM_INDEX8] [NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] 	= Set(3,  Normal_Index8_Float3_Indices3);
								  												  			
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT] [FORMAT_UBYTE] 	= Set(2,  Normal_Index16_Byte); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT] [FORMAT_BYTE]  	= Set(2,  Normal_Index16_Byte);
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT] [FORMAT_USHORT]	= Set(2,  Normal_Index16_Short); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT] [FORMAT_SHORT] 	= Set(2,  Normal_Index16_Short);
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT] [FORMAT_FLOAT] 	= Set(2,  Normal_Index16_Float);
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] 	= Set(2,  Normal_Index16_Byte3_Indices1); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE]  	= Set(2,  Normal_Index16_Byte3_Indices1);
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT]	= Set(2,  Normal_Index16_Short3_Indices1); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] 	= Set(2,  Normal_Index16_Short3_Indices1);
    m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] 	= Set(2,  Normal_Index16_Float3_Indices1);
								  			
	m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT] [FORMAT_UBYTE] 	= Set(2,  Normal_Index16_Byte); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT] [FORMAT_BYTE]  	= Set(2,  Normal_Index16_Byte);
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT] [FORMAT_USHORT]	= Set(2,  Normal_Index16_Short); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT] [FORMAT_SHORT] 	= Set(2,  Normal_Index16_Short);
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT] [FORMAT_FLOAT] 	= Set(2,  Normal_Index16_Float);
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] 	= Set(6,  Normal_Index16_Byte3_Indices3); //HACK	
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE]  	= Set(6,  Normal_Index16_Byte3_Indices3);
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT]	= Set(6,  Normal_Index16_Short3_Indices3); //HACK
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] 	= Set(6,  Normal_Index16_Short3_Indices3);
    m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] 	= Set(6,  Normal_Index16_Float3_Indices3);
}

unsigned int VertexLoader_Normal::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int _index3)
{
	return m_Table[_type][_index3][_elements][_format].gc_size;
}

TPipelineFunction VertexLoader_Normal::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int _index3, bool allow_signed_bytes)
{
	TPipelineFunction pFunc = m_Table[_type][_index3][_elements][_format].function;
    return pFunc;
}

// This fracs are fixed acording to format
#define S8FRAC 0.015625f; // 1.0f / (1U << 6)
#define S16FRAC 0.00006103515625f; // 1.0f / (1U << 14)
// --- Direct ---


inline void ReadDirectS8()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = DataReadS8() * S8FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[1] = DataReadS8() * S8FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[2] = DataReadS8() * S8FRAC;
	VertexManager::s_pCurBufferPointer += 12;
	LOG_NORM8();
}

inline void ReadDirectS16()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = ((s16)DataReadU16()) * S16FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[1] = ((s16)DataReadU16()) * S16FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[2] = ((s16)DataReadU16()) * S16FRAC;
    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORM16()
}

inline void ReadDirectFloat()
{
	((u32*)VertexManager::s_pCurBufferPointer)[0] = DataReadU32();
    ((u32*)VertexManager::s_pCurBufferPointer)[1] = DataReadU32();
    ((u32*)VertexManager::s_pCurBufferPointer)[2] = DataReadU32();
    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORMF()
}

inline void ReadIndirectS8(const s8* pData)
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = pData[0] * S8FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[1] = pData[1] * S8FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[2] = pData[2] * S8FRAC;
	VertexManager::s_pCurBufferPointer += 12;
	LOG_NORM8();
}

inline void ReadIndirectS16(const u16* pData)
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = ((s16)Common::swap16(pData[0])) * S16FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[1] = ((s16)Common::swap16(pData[1])) * S16FRAC;
    ((float*)VertexManager::s_pCurBufferPointer)[2] = ((s16)Common::swap16(pData[2])) * S16FRAC;
	VertexManager::s_pCurBufferPointer += 12;
    LOG_NORM16()
}

inline void ReadIndirectFloat(const u32* pData)
{
	((u32*)VertexManager::s_pCurBufferPointer)[0] = Common::swap32(pData[0]);	
    ((u32*)VertexManager::s_pCurBufferPointer)[1] = Common::swap32(pData[1]);
    ((u32*)VertexManager::s_pCurBufferPointer)[2] = Common::swap32(pData[2]);
    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORMF();
}

void LOADERDECL VertexLoader_Normal::Normal_DirectByte()
{
    ReadDirectS8();
}

void LOADERDECL VertexLoader_Normal::Normal_DirectShort()
{
	ReadDirectS16();
}

void LOADERDECL VertexLoader_Normal::Normal_DirectFloat()
{
    ReadDirectFloat();
}

void LOADERDECL VertexLoader_Normal::Normal_DirectByte3()
{
    for (int i = 0; i < 3; i++)
    {
        ReadDirectS8();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_DirectShort3()
{
    for (int i = 0; i < 3; i++)
    {
        ReadDirectS16();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_DirectFloat3()
{
    for (int i = 0; i < 3; i++)
    {
        ReadDirectFloat();
    }
}


// --- Index8 ---

void LOADERDECL VertexLoader_Normal::Normal_Index8_Byte()
{
    u8 Index = DataReadU8();
	const s8* pData = (const s8 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
	ReadIndirectS8(pData);
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Short()
{
    u8 Index = DataReadU8();
	const u16* pData = (const u16 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
	ReadIndirectS16(pData);
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Float()
{
    u8 Index = DataReadU8();
	const u32* pData = (const u32 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
	ReadIndirectFloat(pData);	
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Byte3_Indices1()
{
    u8 Index = DataReadU8();
	const s8* pData = (const s8 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
    for (int i = 0; i < 3; i++)
	{
		ReadIndirectS8((const s8*)(&pData[3 * i]));
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Short3_Indices1()
{
    u8 Index = DataReadU8();
	const u16* pData = (const u16 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
    for (int i = 0; i < 3; i++)
	{
		ReadIndirectS16((const u16*)(&pData[3 * i]));
    }    
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Float3_Indices1()
{
    u8 Index = DataReadU8();	
	const u32* pData = (const u32 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
    for (int i = 0; i < 3; i++)
	{
		ReadIndirectFloat((const u32*)(&pData[3 * i]));
    }    
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Byte3_Indices3()
{
    for (int i = 0; i < 3; i++)
	{
        u8 Index = DataReadU8();
		const s8* pData = (const s8*)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i);
        ReadIndirectS8(pData);
    }
}


void LOADERDECL VertexLoader_Normal::Normal_Index8_Short3_Indices3()
{
    for (int i = 0; i < 3; i++)
	{
        u8 Index = DataReadU8();
		const u16* pData = (const u16 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i);
        ReadIndirectS16(pData);
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Float3_Indices3()
{
    for (int i = 0; i < 3; i++)
	{
        u8 Index = DataReadU8();
		const u32* pData = (const u32 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i);
        ReadIndirectFloat(pData);
    }    
}


// --- Index16 ---


void LOADERDECL VertexLoader_Normal::Normal_Index16_Byte()
{
    u16 Index = DataReadU16();
	const s8* pData = (const s8 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
	ReadIndirectS8(pData);
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Short()
{
    u16 Index = DataReadU16();
	const u16* pData = (const u16 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
	ReadIndirectS16(pData);
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Float()
{
    u16 Index = DataReadU16();
	const u32* pData = (const u32 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
    ReadIndirectFloat(pData);
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Byte3_Indices1()
{
    u16 Index = DataReadU16();
	const s8* pData = (const s8 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));
    for (int i = 0; i < 3; i++)
	{
		ReadIndirectS8((const s8 *)(&pData[3 * i]));        
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Short3_Indices1()
{
    u16 Index = DataReadU16();
	const u16* pData = (const u16 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));

    for (int i = 0; i < 3; i++)
    {
        ReadIndirectS16((const u16 *)(&pData[3 * i]));
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Float3_Indices1()
{
    u16 Index = DataReadU16();
	const u32* pData = (const u32 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]));

    for (int i = 0; i < 3; i++)
    {
        ReadIndirectFloat((const u32 *)(&pData[3 * i]));
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Byte3_Indices3()
{
    for (int i = 0; i < 3; i++)
	{
        u16 Index = DataReadU16();
		const s8* pData = (const s8*)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i);
        ReadIndirectS8(pData);
    }    
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Short3_Indices3()
{
    for (int i = 0; i < 3; i++)
    {
        u16 Index = DataReadU16();
		const u16* pData = (const u16 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i);
        ReadIndirectS16(pData);
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Float3_Indices3()
{
    for (int i = 0; i < 3; i++)
    {
        u16 Index = DataReadU16();
		const u32* pData = (const u32 *)(cached_arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i);
        ReadIndirectFloat(pData);
    }    
}
