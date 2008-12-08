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

#include "main.h"
#include "Utils.h"
#include "DecodedVArray.h"
#include "VertexLoader.h"
#include "VertexLoader_Normal.h"

u8					VertexLoader_Normal::m_sizeTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS];
TPipelineFunction	VertexLoader_Normal::m_funcTable[NUM_NRM_TYPE][NUM_NRM_FORMAT][NUM_NRM_ELEMENTS];

bool VertexLoader_Normal::index3;
// __________________________________________________________________________________________________
// Init
//
void VertexLoader_Normal::Init(void)
{
	// size table
	m_sizeTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT]		= 3;
	m_sizeTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT]		= 3;
	m_sizeTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT]		= 6;
	m_sizeTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT]		= 6;
	m_sizeTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT]		= 12;
	m_sizeTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT3]	= 9;	
	m_sizeTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT3]	= 9;
	m_sizeTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT3]	= 18;
	m_sizeTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT3]	= 18;
	m_sizeTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT3]	= 36;

	m_sizeTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT]		= 1;
	m_sizeTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT]		= 1;
	m_sizeTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT]		= 1;
	m_sizeTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT]		= 1;
	m_sizeTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT]		= 1;
	m_sizeTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT3]	= 3;	
	m_sizeTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT3]	= 3;
	m_sizeTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT3]	= 3;
	m_sizeTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT3]	= 3;
	m_sizeTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT3]	= 3;

	m_sizeTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT]	= 2;
	m_sizeTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT]	= 2;
	m_sizeTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT]	= 2;
	m_sizeTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT]	= 2;
	m_sizeTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT]	= 2;
	m_sizeTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT3]	= 6;	
	m_sizeTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT3]	= 6;
	m_sizeTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT3]	= 6;
	m_sizeTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT3]	= 6;
	m_sizeTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT3]	= 6;

	// function table
	m_funcTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT]		= Normal_DirectByte; //HACK
	m_funcTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT]		= Normal_DirectByte;
	m_funcTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT]		= Normal_DirectShort; //HACK
	m_funcTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT]		= Normal_DirectShort;
	m_funcTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT]		= Normal_DirectFloat;
	m_funcTable[NRM_DIRECT][FORMAT_UBYTE] [NRM_NBT3]	= Normal_DirectByte3;	 //HACK
	m_funcTable[NRM_DIRECT][FORMAT_BYTE]  [NRM_NBT3]	= Normal_DirectByte3;
	m_funcTable[NRM_DIRECT][FORMAT_USHORT][NRM_NBT3]	= Normal_DirectShort3; //HACK
	m_funcTable[NRM_DIRECT][FORMAT_SHORT] [NRM_NBT3]	= Normal_DirectShort3;
	m_funcTable[NRM_DIRECT][FORMAT_FLOAT] [NRM_NBT3]	= Normal_DirectFloat3;

	m_funcTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT]		= Normal_Index8_Byte; //HACK
	m_funcTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT]		= Normal_Index8_Byte;
	m_funcTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT]		= Normal_Index8_Short; //HACK
	m_funcTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT]		= Normal_Index8_Short;
	m_funcTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT]		= Normal_Index8_Float;
	m_funcTable[NRM_INDEX8][FORMAT_UBYTE] [NRM_NBT3]	= Normal_Index8_Byte3;	 //HACK
	m_funcTable[NRM_INDEX8][FORMAT_BYTE]  [NRM_NBT3]	= Normal_Index8_Byte3;
	m_funcTable[NRM_INDEX8][FORMAT_USHORT][NRM_NBT3]	= Normal_Index8_Short3; //HACK
	m_funcTable[NRM_INDEX8][FORMAT_SHORT] [NRM_NBT3]	= Normal_Index8_Short3;
	m_funcTable[NRM_INDEX8][FORMAT_FLOAT] [NRM_NBT3]	= Normal_Index8_Float3;

	m_funcTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT]	= Normal_Index16_Byte; //HACK
	m_funcTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT]	= Normal_Index16_Byte;
	m_funcTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT]	= Normal_Index16_Short; //HACK
	m_funcTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT]	= Normal_Index16_Short;
	m_funcTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT]	= Normal_Index16_Float;
	m_funcTable[NRM_INDEX16][FORMAT_UBYTE] [NRM_NBT3]	= Normal_Index16_Byte3;	//HACK
	m_funcTable[NRM_INDEX16][FORMAT_BYTE]  [NRM_NBT3]	= Normal_Index16_Byte3;
	m_funcTable[NRM_INDEX16][FORMAT_USHORT][NRM_NBT3]	= Normal_Index16_Short3; //HACK
	m_funcTable[NRM_INDEX16][FORMAT_SHORT] [NRM_NBT3]	= Normal_Index16_Short3;
	m_funcTable[NRM_INDEX16][FORMAT_FLOAT] [NRM_NBT3]	= Normal_Index16_Float3;
}

// __________________________________________________________________________________________________
// GetSize
//
unsigned int
VertexLoader_Normal::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements)
{
	return m_sizeTable[_type][_format][_elements];
}

// __________________________________________________________________________________________________
// GetFunction
//
TPipelineFunction
VertexLoader_Normal::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements)
{
	TPipelineFunction pFunc = m_funcTable[_type][_format][_elements];
	return pFunc;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// --- Direct ---
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Normal_DirectByte
//
void LOADERDECL 
VertexLoader_Normal::Normal_DirectByte(const void* _p)
{
	varray->SetNormalX(0, ((float)(signed char)DataReadU8()+0.5f) / 127.5f);
	varray->SetNormalY(0, ((float)(signed char)DataReadU8()+0.5f) / 127.5f);
	varray->SetNormalZ(0, ((float)(signed char)DataReadU8()+0.5f) / 127.5f);
}

// __________________________________________________________________________________________________
// Normal_DirectShort
//
void LOADERDECL 
VertexLoader_Normal::Normal_DirectShort(const void* _p)
{
	varray->SetNormalX(0, ((float)(signed short)DataReadU16()+0.5f) / 32767.5f);
	varray->SetNormalY(0, ((float)(signed short)DataReadU16()+0.5f) / 32767.5f);
	varray->SetNormalZ(0, ((float)(signed short)DataReadU16()+0.5f) / 32767.5f);
}

// __________________________________________________________________________________________________
// Normal_DirectFloat
//
void LOADERDECL 
VertexLoader_Normal::Normal_DirectFloat(const void* _p)
{
	varray->SetNormalX(0, DataReadF32());
	varray->SetNormalY(0, DataReadF32());
	varray->SetNormalZ(0, DataReadF32());
}

// __________________________________________________________________________________________________
// Normal_DirectByte3
//
void LOADERDECL 
VertexLoader_Normal::Normal_DirectByte3(const void* _p)
{
	for (int i=0; i<3; i++)
	{
		varray->SetNormalX(i, ((float)(signed char)DataReadU8()+0.5f) / 127.5f);
		varray->SetNormalY(i, ((float)(signed char)DataReadU8()+0.5f) / 127.5f);
		varray->SetNormalZ(i, ((float)(signed char)DataReadU8()+0.5f) / 127.5f);
	}
}

// __________________________________________________________________________________________________
// Normal_DirectShort3
//
void LOADERDECL 
VertexLoader_Normal::Normal_DirectShort3(const void* _p)
{
	for (int i=0; i<3; i++)
	{
		varray->SetNormalX(i, ((float)(signed short)DataReadU16()+0.5f) / 32767.5f);
		varray->SetNormalY(i, ((float)(signed short)DataReadU16()+0.5f) / 32767.5f);
		varray->SetNormalZ(i, ((float)(signed short)DataReadU16()+0.5f) / 32767.5f);
	}
}

// __________________________________________________________________________________________________
// Normal_DirectFloat3
//
void LOADERDECL 
VertexLoader_Normal::Normal_DirectFloat3(const void* _p)
{
	for (int i=0; i<3; i++)
	{
		varray->SetNormalX(i, DataReadF32());
		varray->SetNormalY(i, DataReadF32());
		varray->SetNormalZ(i, DataReadF32());
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// --- Index8 ---
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Normal_Index8_Byte
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index8_Byte(const void* _p)
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);

	varray->SetNormalX(0, ((float)(signed char)Memory_Read_U8(iAddress  )+0.5f) / 127.5f);
	varray->SetNormalY(0, ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f);
	varray->SetNormalZ(0, ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f);
}

// __________________________________________________________________________________________________
// Normal_Index8_Short
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index8_Short(const void* _p)
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);

	varray->SetNormalX(0, ((float)(signed short)Memory_Read_U16(iAddress  )+0.5f) / 32767.5f);
	varray->SetNormalY(0, ((float)(signed short)Memory_Read_U16(iAddress+2)+0.5f) / 32767.5f);
	varray->SetNormalZ(0, ((float)(signed short)Memory_Read_U16(iAddress+4)+0.5f) / 32767.5f);
}

// __________________________________________________________________________________________________
// Normal_Index8_Float
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index8_Float(const void* _p)
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);

	varray->SetNormalX(0, Memory_Read_Float(iAddress));
	varray->SetNormalY(0, Memory_Read_Float(iAddress+4));
	varray->SetNormalZ(0, Memory_Read_Float(iAddress+8));
}

// __________________________________________________________________________________________________
// Normal_Index8_Byte3
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index8_Byte3(const void* _p)
{
	if (index3)
	{
		for (int i=0; i<3; i++)
		{
			u8 Index = DataReadU8();
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;

			varray->SetNormalX(i, ((float)(signed char)Memory_Read_U8(iAddress  )+0.5f) / 127.5f);
			varray->SetNormalY(i, ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f);
			varray->SetNormalZ(i, ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f);
		}
	}
	else
	{
		u8 Index = DataReadU8();
		for (int i=0; i<3; i++)
		{
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;

			varray->SetNormalX(i, ((float)(signed char)Memory_Read_U8(iAddress  )+0.5f) / 127.5f);
			varray->SetNormalY(i, ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f);
			varray->SetNormalZ(i, ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f);
		}
	}
}

// __________________________________________________________________________________________________
// Normal_Index8_Short3
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index8_Short3(const void* _p)
{
	if (index3)
	{
		for (int i=0; i<3; i++)
		{
			u8 Index = DataReadU8();
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;

			varray->SetNormalX(i, ((float)(signed short)Memory_Read_U16(iAddress  )+0.5f) / 32767.5f);
			varray->SetNormalY(i, ((float)(signed short)Memory_Read_U16(iAddress+2)+0.5f) / 32767.5f);
			varray->SetNormalZ(i, ((float)(signed short)Memory_Read_U16(iAddress+4)+0.5f) / 32767.5f);
		}
	}
	else
	{
		u8 Index = DataReadU8();
		for (int i=0; i<3; i++)
		{
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;

			varray->SetNormalX(i, ((float)(signed short)Memory_Read_U16(iAddress  )+0.5f) / 32767.5f);
			varray->SetNormalY(i, ((float)(signed short)Memory_Read_U16(iAddress+2)+0.5f) / 32767.5f);
			varray->SetNormalZ(i, ((float)(signed short)Memory_Read_U16(iAddress+4)+0.5f) / 32767.5f);
		}
	}
}

// __________________________________________________________________________________________________
// Normal_Index8_Float3
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index8_Float3(const void* _p)
{
	if (index3)
	{
		for (int i=0; i<3; i++)
		{
			u8 Index = DataReadU8();
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;

			varray->SetNormalX(i, Memory_Read_Float(iAddress));
			varray->SetNormalY(i, Memory_Read_Float(iAddress+4));
			varray->SetNormalZ(i, Memory_Read_Float(iAddress+8));
		}
	}
	else
	{
		u8 Index = DataReadU8();
		for (int i=0; i<3; i++)
		{
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;

			varray->SetNormalX(i, Memory_Read_Float(iAddress));
			varray->SetNormalY(i, Memory_Read_Float(iAddress+4));
			varray->SetNormalZ(i, Memory_Read_Float(iAddress+8));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// --- Index16 ---
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Normal_Index16_Byte
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index16_Byte(const void* _p)
{
	u16 Index = DataReadU16();
	u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);

	varray->SetNormalX(0, ((float)(signed char)Memory_Read_U8(iAddress  )+0.5f) / 127.5f);
	varray->SetNormalY(0, ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f);
	varray->SetNormalZ(0, ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f);
}

// __________________________________________________________________________________________________
// Normal_Index16_Short
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index16_Short(const void* _p)
{
	u16 Index = DataReadU16();
	u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);

	varray->SetNormalX(0, ((float)(signed short)Memory_Read_U16(iAddress  )+0.5f) / 32767.5f);
	varray->SetNormalY(0, ((float)(signed short)Memory_Read_U16(iAddress+2)+0.5f) / 32767.5f);
	varray->SetNormalZ(0, ((float)(signed short)Memory_Read_U16(iAddress+4)+0.5f) / 32767.5f);
}

// __________________________________________________________________________________________________
// Normal_Index8_Float
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index16_Float(const void* _p)
{
	u16 Index = DataReadU16();
	u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]);

	varray->SetNormalX(0, Memory_Read_Float(iAddress));
	varray->SetNormalY(0, Memory_Read_Float(iAddress+4));
	varray->SetNormalZ(0, Memory_Read_Float(iAddress+8));
}

// __________________________________________________________________________________________________
// Normal_Index16_Byte3
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index16_Byte3(const void* _p)
{
	if (index3)
	{
		for (int i=0; i<3; i++)
		{
			u16 Index = DataReadU16();
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;

			varray->SetNormalX(i, ((float)(signed char)Memory_Read_U8(iAddress  )+0.5f) / 127.5f);
			varray->SetNormalY(i, ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f);
			varray->SetNormalZ(i, ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f);
		}
	}
	else
	{
		u16 Index = DataReadU16();
		for (int i=0; i<3; i++)
		{
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 1*3*i;

			varray->SetNormalX(i, ((float)(signed char)Memory_Read_U8(iAddress  )+0.5f) / 127.5f);
			varray->SetNormalY(i, ((float)(signed char)Memory_Read_U8(iAddress+1)+0.5f) / 127.5f);
			varray->SetNormalZ(i, ((float)(signed char)Memory_Read_U8(iAddress+2)+0.5f) / 127.5f);
		}
	}
}

// __________________________________________________________________________________________________
// Normal_Index16_Short3
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index16_Short3(const void* _p)
{
	if (index3)
	{
		for (int i=0; i<3; i++)
		{
			u16 Index = DataReadU16();
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;

			varray->SetNormalX(i, ((float)(signed short)Memory_Read_U16(iAddress  )+0.5f) / 32767.5f);
			varray->SetNormalY(i, ((float)(signed short)Memory_Read_U16(iAddress+2)+0.5f) / 32767.5f);
			varray->SetNormalZ(i, ((float)(signed short)Memory_Read_U16(iAddress+4)+0.5f) / 32767.5f);
		}
	}
	else
	{
		u16 Index = DataReadU16();
		for (int i=0; i<3; i++)
		{
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 2*3*i;

			varray->SetNormalX(i, ((float)(signed short)Memory_Read_U16(iAddress  )+0.5f) / 32767.5f);
			varray->SetNormalY(i, ((float)(signed short)Memory_Read_U16(iAddress+2)+0.5f) / 32767.5f);
			varray->SetNormalZ(i, ((float)(signed short)Memory_Read_U16(iAddress+4)+0.5f) / 32767.5f);
		}
	}
}

// __________________________________________________________________________________________________
// Normal_Index16_Float3
//
void LOADERDECL 
VertexLoader_Normal::Normal_Index16_Float3(const void* _p)
{
	if (index3)
	{
		for (int i=0; i<3; i++)
		{
			u16 Index = DataReadU16();
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;

			varray->SetNormalX(i, Memory_Read_Float(iAddress  ));
			varray->SetNormalY(i, Memory_Read_Float(iAddress+4));
			varray->SetNormalZ(i, Memory_Read_Float(iAddress+8));
		}
	}
	else
	{
		u16 Index = DataReadU16();
		for (int i=0; i<3; i++)
		{
			u32 iAddress = arraybases[ARRAY_NORMAL] + (Index * arraystrides[ARRAY_NORMAL]) + 4*3*i;

			varray->SetNormalX(i, Memory_Read_Float(iAddress  ));
			varray->SetNormalY(i, Memory_Read_Float(iAddress+4));
			varray->SetNormalZ(i, Memory_Read_Float(iAddress+8));
		}
	}
}