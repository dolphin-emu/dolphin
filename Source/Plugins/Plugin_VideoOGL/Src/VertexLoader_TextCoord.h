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

#ifndef VERTEXLOADER_TEXCOORD_H
#define VERTEXLOADER_TEXCOORD_H

#define LOG_TEX1() PRIM_LOG("tex: %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0]);
#define LOG_TEX2() PRIM_LOG("tex: %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1]);

extern int tcIndex;

void LOADERDECL TexCoord_Read_Dummy(const void *_p)
{
    tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_UByte1(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU8() * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_UByte2(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU8() * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)DataReadU8() * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Byte1(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)DataReadU8() * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Byte2(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)DataReadU8() * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s8)DataReadU8() * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_UShort1(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU16() * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_UShort2(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU16() * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)DataReadU16() * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Short1(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)DataReadU16() * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Short2(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)DataReadU16() * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)DataReadU16() * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Float1(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = DataReadF32() * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Float2(const void *_p)
{
    ((float*)VertexManager::s_pCurBufferPointer)[0] = DataReadF32() * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = DataReadF32() * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

// ==================================================================================
void LOADERDECL TexCoord_ReadIndex8_UByte1(const void *_p)	
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_UByte2(const void *_p)	
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(u8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_Byte1(const void *_p)		
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Byte2(const void *_p)		
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_UShort1(const void *_p)	
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_UShort2(const void *_p)	
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(u16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_Short1(const void *_p)	
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Short2(const void *_p)	
{
    u8 Index = DataReadU8();
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_Float1(const void *_p)	
{
    u16 Index = DataReadU8(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
    u32 uTemp;
    uTemp = Memory_Read_U32(iAddress); 
    ((float*)VertexManager::s_pCurBufferPointer)[0] = *(float*)&uTemp * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Float2(const void *_p)	
{
    u16 Index = DataReadU8(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
    u32 uTemp;
    uTemp = Memory_Read_U32(iAddress); 
    ((float*)VertexManager::s_pCurBufferPointer)[0] = *(float*)&uTemp * tcScaleU[tcIndex];
    uTemp = Memory_Read_U32(iAddress+4);  
    ((float*)VertexManager::s_pCurBufferPointer)[1] = *(float*)&uTemp * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

// ==================================================================================
void LOADERDECL TexCoord_ReadIndex16_UByte1(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(u8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_UByte2(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(u8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] =  (float)(u8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_Byte1(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(s8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Byte2(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(s8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] =  (float)(s8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_UShort1(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_UShort2(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(u16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_Short1(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Short2(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

    ((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex];
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_Float1(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
    u32 uTemp;
    uTemp = Memory_Read_U32(iAddress  ); 
    ((float*)VertexManager::s_pCurBufferPointer)[0] = *(float*)&uTemp * tcScaleU[tcIndex];
    LOG_TEX1();
    VertexManager::s_pCurBufferPointer += 4;
    tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Float2(const void *_p)	
{
    u16 Index = DataReadU16(); 
    u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
    u32 uTemp;
    uTemp = Memory_Read_U32(iAddress  ); 
    ((float*)VertexManager::s_pCurBufferPointer)[0] = *(float*)&uTemp * tcScaleU[tcIndex];
    uTemp = Memory_Read_U32(iAddress+4); 
    ((float*)VertexManager::s_pCurBufferPointer)[1] = *(float*)&uTemp * tcScaleV[tcIndex];
    LOG_TEX2();
    VertexManager::s_pCurBufferPointer += 8;
    tcIndex++;
}

#endif
