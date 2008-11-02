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

#include "Globals.h"
#include "Config.h"
#include "VertexLoader.h"
#include "VertexManager.h"
#include "VertexLoader_Normal.h"

#define LOG_NORM8() PRIM_LOG("norm: %f %f %f, ", ((s8*)VertexManager::s_pCurBufferPointer)[-3]/127.0f, ((s8*)VertexManager::s_pCurBufferPointer)[-2]/127.0f, ((s8*)VertexManager::s_pCurBufferPointer)[-1]/127.0f);
#define LOG_NORM16() PRIM_LOG("norm: %f %f %f, ", ((s16*)VertexManager::s_pCurBufferPointer)[-3]/32767.0f, ((s16*)VertexManager::s_pCurBufferPointer)[-2]/32767.0f, ((s16*)VertexManager::s_pCurBufferPointer)[-1]/32767.0f);
#define LOG_NORMF() PRIM_LOG("norm: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[-3], ((float*)VertexManager::s_pCurBufferPointer)[-2], ((float*)VertexManager::s_pCurBufferPointer)[-1]);

u8					VertexLoader_Normal::m_sizeTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS][NUM_NRM_INDICES];
TPipelineFunction	VertexLoader_Normal::m_funcTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS][NUM_NRM_INDICES];

void VertexLoader_Normal::Init(void)
{
    // size table
    m_sizeTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES1]		= 3;
    m_sizeTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES1]		= 3;
    m_sizeTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT] [NRM_INDICES1]		= 6;
    m_sizeTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES1]		= 6;
    m_sizeTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES1]		= 12;
    m_sizeTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES1]		= 9;	
    m_sizeTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES1]		= 9;
    m_sizeTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES1]		= 18;
    m_sizeTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES1]		= 18;
    m_sizeTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES1]		= 36;

	m_sizeTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES3]		= 3;
    m_sizeTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES3]		= 3;
    m_sizeTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT] [NRM_INDICES3]		= 6;
    m_sizeTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES3]		= 6;
    m_sizeTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES3]		= 12;
    m_sizeTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES3]		= 9;	
    m_sizeTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES3]		= 9;
    m_sizeTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES3]		= 18;
    m_sizeTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES3]		= 18;
    m_sizeTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES3]		= 36;

    m_sizeTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES1]		= 1;	
    m_sizeTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES1]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES1]		= 1;

	m_sizeTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES3]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES3]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT] [NRM_INDICES3]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES3]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES3]		= 1;
    m_sizeTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES3]		= 3;	
    m_sizeTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES3]		= 3;
    m_sizeTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES3]		= 3;
    m_sizeTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES3]		= 3;
    m_sizeTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES3]		= 3;

    m_sizeTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES1]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES1]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT] [NRM_INDICES1]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES1]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES1]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES1]	= 2;	
    m_sizeTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES1]	= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES1]	= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES1]	= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES1]	= 2;

	m_sizeTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES3]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES3]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT] [NRM_INDICES3]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES3]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES3]		= 2;
    m_sizeTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES3]	= 6;	
    m_sizeTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES3]	= 6;
    m_sizeTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES3]	= 6;
    m_sizeTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES3]	= 6;
    m_sizeTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES3]	= 6;

    // function table
    m_funcTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES1]		= Normal_DirectByte; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES1]		= Normal_DirectByte;
    m_funcTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT] [NRM_INDICES1]		= Normal_DirectShort; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES1]		= Normal_DirectShort;
    m_funcTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES1]		= Normal_DirectFloat;
    m_funcTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES1]		= Normal_DirectByte3; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES1]		= Normal_DirectByte3;
    m_funcTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES1]		= Normal_DirectShort3; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES1]		= Normal_DirectShort3;
    m_funcTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES1]		= Normal_DirectFloat3;

	m_funcTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES3]		= Normal_DirectByte; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES3]		= Normal_DirectByte;
    m_funcTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT] [NRM_INDICES3]		= Normal_DirectShort; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES3]		= Normal_DirectShort;
    m_funcTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES3]		= Normal_DirectFloat;
    m_funcTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES3]		= Normal_DirectByte3; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES3]		= Normal_DirectByte3;
    m_funcTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES3]		= Normal_DirectShort3; //HACK
    m_funcTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES3]		= Normal_DirectShort3;
    m_funcTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES3]		= Normal_DirectFloat3;

    m_funcTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES1]		= Normal_Index8_Byte; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES1]		= Normal_Index8_Byte;
    m_funcTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT] [NRM_INDICES1]		= Normal_Index8_Short; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES1]		= Normal_Index8_Short;
    m_funcTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES1]		= Normal_Index8_Float;
    m_funcTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES1]		= Normal_Index8_Byte3_Indices1; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES1]		= Normal_Index8_Byte3_Indices1;
    m_funcTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES1]		= Normal_Index8_Short3_Indices1; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES1]		= Normal_Index8_Short3_Indices1;
    m_funcTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES1]		= Normal_Index8_Float3_Indices1;

	m_funcTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES3]		= Normal_Index8_Byte; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES3]		= Normal_Index8_Byte;
    m_funcTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT] [NRM_INDICES3]		= Normal_Index8_Short; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES3]		= Normal_Index8_Short;
    m_funcTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES3]		= Normal_Index8_Float;
    m_funcTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES3]		= Normal_Index8_Byte3_Indices3; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES3]		= Normal_Index8_Byte3_Indices3;
    m_funcTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES3]		= Normal_Index8_Short3_Indices3; //HACK
    m_funcTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES3]		= Normal_Index8_Short3_Indices3;
    m_funcTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES3]		= Normal_Index8_Float3_Indices3;

    m_funcTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES1]		= Normal_Index16_Byte; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES1]		= Normal_Index16_Byte;
    m_funcTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT] [NRM_INDICES1]		= Normal_Index16_Short; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES1]		= Normal_Index16_Short;
    m_funcTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES1]		= Normal_Index16_Float;
    m_funcTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES1]	= Normal_Index16_Byte3_Indices1; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES1]	= Normal_Index16_Byte3_Indices1;
    m_funcTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES1]	= Normal_Index16_Short3_Indices1; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES1]	= Normal_Index16_Short3_Indices1;
    m_funcTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES1]	= Normal_Index16_Float3_Indices1;

	m_funcTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT] [NRM_INDICES3]		= Normal_Index16_Byte; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT] [NRM_INDICES3]		= Normal_Index16_Byte;
    m_funcTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT] [NRM_INDICES3]		= Normal_Index16_Short; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT] [NRM_INDICES3]		= Normal_Index16_Short;
    m_funcTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT] [NRM_INDICES3]		= Normal_Index16_Float;
    m_funcTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT3] [NRM_INDICES3]	= Normal_Index16_Byte3_Indices3; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT3] [NRM_INDICES3]	= Normal_Index16_Byte3_Indices3;
    m_funcTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT3] [NRM_INDICES3]	= Normal_Index16_Short3_Indices3; //HACK
    m_funcTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT3] [NRM_INDICES3]	= Normal_Index16_Short3_Indices3;
    m_funcTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT3] [NRM_INDICES3]	= Normal_Index16_Float3_Indices3;
}

unsigned int VertexLoader_Normal::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int _index3)
{
	return m_sizeTable[_type][_format][_elements][_index3];
}

TPipelineFunction VertexLoader_Normal::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int _index3)
{
    TPipelineFunction pFunc = m_funcTable[_type][_format][_elements][_index3];
    return pFunc;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// --- Direct ---
/////////////////////////////////////////////////////////////////////////////////////////////////////
void LOADERDECL VertexLoader_Normal::Normal_DirectByte(const void *_p)
{
    *VertexManager::s_pCurBufferPointer++ = DataReadU8();
    *VertexManager::s_pCurBufferPointer++ = DataReadU8();
    *VertexManager::s_pCurBufferPointer++ = DataReadU8();
    VertexManager::s_pCurBufferPointer++;
	LOG_NORM8();
//    ((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(signed char)DataReadU8()+0.5f) / 127.5f;
}

void LOADERDECL VertexLoader_Normal::Normal_DirectShort(const void *_p)
{
    ((u16*)VertexManager::s_pCurBufferPointer)[0] = DataReadU16();
    ((u16*)VertexManager::s_pCurBufferPointer)[1] = DataReadU16();
    ((u16*)VertexManager::s_pCurBufferPointer)[2] = DataReadU16();
    VertexManager::s_pCurBufferPointer += 8;
    LOG_NORM16()
//    ((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(signed short)DataReadU16()+0.5f) / 32767.5f;
//    ((float*)VertexManager::s_pCurBufferPointer)[1] = ((float)(signed short)DataReadU16()+0.5f) / 32767.5f;
//    ((float*)VertexManager::s_pCurBufferPointer)[2] = ((float)(signed short)DataReadU16()+0.5f) / 32767.5f;
}

void LOADERDECL VertexLoader_Normal::Normal_DirectFloat(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = DataReadF32();
    ((float*)VertexManager::s_pCurBufferPointer)[1] = DataReadF32();
    ((float*)VertexManager::s_pCurBufferPointer)[2] = DataReadF32();
    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORMF()
}

void LOADERDECL VertexLoader_Normal::Normal_DirectByte3(const void *_p)
{
    for (int i = 0; i < 3; i++)
    {
        *VertexManager::s_pCurBufferPointer++ = DataReadU8();
        *VertexManager::s_pCurBufferPointer++ = DataReadU8();
        *VertexManager::s_pCurBufferPointer++ = DataReadU8();
        VertexManager::s_pCurBufferPointer++;
        LOG_NORM8();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_DirectShort3(const void *_p)
{
    for (int i = 0; i < 3; i++)
    {
        ((u16*)VertexManager::s_pCurBufferPointer)[0] = DataReadU16();
        ((u16*)VertexManager::s_pCurBufferPointer)[1] = DataReadU16();
        ((u16*)VertexManager::s_pCurBufferPointer)[2] = DataReadU16();
        VertexManager::s_pCurBufferPointer += 8;
        LOG_NORM16();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_DirectFloat3(const void *_p)
{
    for (int i = 0; i < 3; i++)
    {
        ((float*)VertexManager::s_pCurBufferPointer)[0] = DataReadF32();
        ((float*)VertexManager::s_pCurBufferPointer)[1] = DataReadF32();
        ((float*)VertexManager::s_pCurBufferPointer)[2] = DataReadF32();
        VertexManager::s_pCurBufferPointer += 12;
        LOG_NORMF();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// --- Index8 ---
/////////////////////////////////////////////////////////////////////////////////////////////////////
void LOADERDECL VertexLoader_Normal::Normal_Index8_Byte(const void *_p)
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);
    *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress);
    *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+1);
    *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+2);
	VertexManager::s_pCurBufferPointer++;
//    ((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(signed char)Memory_Read_U8(iAddress)+0.5f) / 127.5f;
//    ((float*)VertexManager::s_pCurBufferPointer)[1] = ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f;
//    ((float*)VertexManager::s_pCurBufferPointer)[2] = ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f;
//    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORM8();
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Short(const void *_p)
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);
    ((u16*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U16(iAddress);
    ((u16*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U16(iAddress+2);
    ((u16*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U16(iAddress+4);
    VertexManager::s_pCurBufferPointer += 8;
    LOG_NORM16();
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Float(const void *_p)
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);
    ((float*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_Float(iAddress);
    ((float*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_Float(iAddress+4);
    ((float*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_Float(iAddress+8);
    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORMF();
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Byte3_Indices1(const void *_p)
{
    u8 Index = DataReadU8();
    for (int i = 0; i < 3; i++)
	{
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+1);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+2);
		VertexManager::s_pCurBufferPointer++;
        LOG_NORM8();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Short3_Indices1(const void *_p)
{
    u8 Index = DataReadU8();
    for (int i = 0; i < 3; i++)
	{
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;
        ((u16*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U16(iAddress);
        ((u16*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U16(iAddress+2);
        ((u16*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U16(iAddress+4);
        VertexManager::s_pCurBufferPointer += 8;
        LOG_NORM16();
    }    
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Float3_Indices1(const void *_p)
{
    u8 Index = DataReadU8();
    for (int i = 0; i < 3; i++)
	{
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;
        ((float*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_Float(iAddress);
        ((float*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_Float(iAddress+4);
        ((float*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_Float(iAddress+8);
        VertexManager::s_pCurBufferPointer += 12;
        LOG_NORMF();
    }    
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Byte3_Indices3(const void *_p)
{
    for (int i = 0; i < 3; i++)
	{
        u8 Index = DataReadU8();
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+1);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+2);
        *VertexManager::s_pCurBufferPointer++;
        LOG_NORM8();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Short3_Indices3(const void *_p)
{
    for (int i = 0; i < 3; i++)
	{
        u8 Index = DataReadU8();
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;
        ((u16*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U16(iAddress);
        ((u16*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U16(iAddress+2);
        ((u16*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U16(iAddress+4);
        VertexManager::s_pCurBufferPointer += 8;
        LOG_NORM16();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index8_Float3_Indices3(const void *_p)
{
    for (int i = 0; i < 3; i++)
	{
        u8 Index = DataReadU8();
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;
        ((float*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_Float(iAddress);
        ((float*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_Float(iAddress+4);
        ((float*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_Float(iAddress+8);
        VertexManager::s_pCurBufferPointer += 12;
        LOG_NORMF();
    }    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// --- Index16 ---
/////////////////////////////////////////////////////////////////////////////////////////////////////

void LOADERDECL VertexLoader_Normal::Normal_Index16_Byte(const void *_p)
{
    u16 Index = DataReadU16();
    u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);
    *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress);
    *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+1);
    *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+2);
	VertexManager::s_pCurBufferPointer++;
    LOG_NORM8();
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Short(const void *_p)
{
    u16 Index = DataReadU16();
    u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);
    ((u16*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U16(iAddress);
    ((u16*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U16(iAddress+2);
    ((u16*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U16(iAddress+4);
    VertexManager::s_pCurBufferPointer += 8;
    LOG_NORM16();
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Float(const void *_p)
{
    u16 Index = DataReadU16();
    u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);
    ((float*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_Float(iAddress);
    ((float*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_Float(iAddress+4);
    ((float*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_Float(iAddress+8);
    VertexManager::s_pCurBufferPointer += 12;
    LOG_NORMF();
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Byte3_Indices1(const void *_p)
{
    u16 Index = DataReadU16();
    for (int i = 0; i < 3; i++)
	{
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+1);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+2);
		VertexManager::s_pCurBufferPointer++;
        LOG_NORM8();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Short3_Indices1(const void *_p)
{
    u16 Index = DataReadU16();
    for (int i = 0; i < 3; i++)
    {
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;
        ((u16*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U16(iAddress);
        ((u16*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U16(iAddress+2);
        ((u16*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U16(iAddress+4);
        VertexManager::s_pCurBufferPointer += 8;
        LOG_NORM16();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Float3_Indices1(const void *_p)
{
    u16 Index = DataReadU16();
    for (int i = 0; i < 3; i++)
    {
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;
        ((float*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_Float(iAddress);
        ((float*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_Float(iAddress+4);
        ((float*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_Float(iAddress+8);
        VertexManager::s_pCurBufferPointer += 12;
        LOG_NORMF();
    }
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Byte3_Indices3(const void *_p)
{
    for (int i = 0; i < 3; i++)
	{
        u16 Index = DataReadU16();
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+1);
        *VertexManager::s_pCurBufferPointer++ = Memory_Read_U8(iAddress+2);
		VertexManager::s_pCurBufferPointer++;
        LOG_NORM8();
    }    
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Short3_Indices3(const void *_p)
{
    for (int i = 0; i < 3; i++)
    {
        u16 Index = DataReadU16();
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;
        ((u16*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U16(iAddress);
        ((u16*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U16(iAddress+2);
        ((u16*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U16(iAddress+4);
        VertexManager::s_pCurBufferPointer += 8;
        LOG_NORM16();
    }
   
}

void LOADERDECL VertexLoader_Normal::Normal_Index16_Float3_Indices3(const void *_p)
{
    for (int i = 0; i < 3; i++)
    {
        u16 Index = DataReadU16();
        u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;
        ((float*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_Float(iAddress);
        ((float*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_Float(iAddress+4);
        ((float*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_Float(iAddress+8);
        VertexManager::s_pCurBufferPointer += 12;
        LOG_NORMF();
    }    
}
