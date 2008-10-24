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

#ifndef VERTEXHANDLER_TEXCOORD_H
#define VERTEXHANDLER_TEXCOORD_H

extern int tcIndex;

void LOADERDECL TexCoord_ReadDirect_UByte(const void* _p)
{
	varray->SetU(tcIndex, DataReadU8() * tcScaleU[tcIndex]);   
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, DataReadU8() * tcScaleV[tcIndex]); 
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Byte(const void* _p)
{
	varray->SetU(tcIndex, (s8)DataReadU8() * tcScaleU[tcIndex]);   
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (s8)DataReadU8() * tcScaleV[tcIndex]);   
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_UShort(const void* _p)
{
	varray->SetU(tcIndex, DataReadU16() * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, DataReadU16() * tcScaleV[tcIndex]);
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Short(const void* _p)
{
	varray->SetU(tcIndex, (s16)DataReadU16() * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (s16)DataReadU16() * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Float(const void* _p)
{
	varray->SetU(tcIndex, DataReadF32() * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, DataReadF32() * tcScaleV[tcIndex]);
	tcIndex++;
}

// ==================================================================================
void LOADERDECL TexCoord_ReadIndex8_UByte(const void* _p)	
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(u8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(u8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Byte(const void* _p)		
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(s8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(s8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex]);

	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_UShort(const void* _p)	
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(u16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(u16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Short(const void* _p)	
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(s16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(s16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Float(const void* _p)	
{
	u16 Index = DataReadU8(); 
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	u32 uTemp;
	uTemp = Memory_Read_U32(iAddress  ); 
	varray->SetU(tcIndex, *(float*)&uTemp * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
	{
		uTemp = Memory_Read_U32(iAddress+4);  
		varray->SetV(tcIndex, *(float*)&uTemp * tcScaleV[tcIndex]);
	}
	tcIndex++;
}


// ==================================================================================
void LOADERDECL TexCoord_ReadIndex16_UByte(const void* _p)	
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	u32 uTemp;
	uTemp = (u8)Memory_Read_U8(iAddress  ); 
	varray->SetU(tcIndex, float(uTemp)* tcScaleU[tcIndex]);
	
	if (tcElements[tcIndex])
	{
		uTemp = (u8)Memory_Read_U8(iAddress+1); 
		varray->SetV(tcIndex, float(uTemp)* tcScaleV[tcIndex]);
	}
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Byte(const void* _p)	
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	u32 uTemp;
	uTemp = (s8)Memory_Read_U8(iAddress  ); 
	varray->SetU(tcIndex, float(uTemp)* tcScaleU[tcIndex]);
	
	if (tcElements[tcIndex])
	{
		uTemp = (s8)Memory_Read_U8(iAddress+1); 
		varray->SetV(tcIndex, float(uTemp)* tcScaleV[tcIndex]);
	}
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_UShort(const void* _p)	
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	float uTemp;
	uTemp = (float)(u16)Memory_Read_U16(iAddress  ); 
	varray->SetU(tcIndex, uTemp * tcScaleU[tcIndex]);

	if (tcElements[tcIndex])
	{
		uTemp = (float)(u16)Memory_Read_U16(iAddress+2); 
		varray->SetV(tcIndex, uTemp * tcScaleV[tcIndex]);
	}
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Short(const void* _p)	
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	float uTemp;
	uTemp = (float)(s16)Memory_Read_U16(iAddress  ); 
	varray->SetU(tcIndex, uTemp * tcScaleU[tcIndex]);

	if (tcElements[tcIndex])
	{
		uTemp = (float)(s16)Memory_Read_U16(iAddress+2); 
		varray->SetV(tcIndex, uTemp * tcScaleV[tcIndex]);
	}
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Float(const void* _p)	
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	u32 uTemp;
	uTemp = Memory_Read_U32(iAddress  ); 
	varray->SetU(tcIndex, *(float*)&uTemp * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
	{
		uTemp = Memory_Read_U32(iAddress+4); 
		varray->SetV(tcIndex, *(float*)&uTemp * tcScaleV[tcIndex]);
	}
	tcIndex++;
}

#endif