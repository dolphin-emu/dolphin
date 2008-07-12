#ifndef VERTEXHANDLER_TEXCOORD_H
#define VERTEXHANDLER_TEXCOORD_H

extern int tcIndex;

void LOADERDECL TexCoord_ReadDirect_UByte(void* _p)
{
	varray->SetU(tcIndex, ReadBuffer8() * tcScaleU[tcIndex]);   
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, ReadBuffer8() * tcScaleV[tcIndex]); 
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Byte(void* _p)
{
	varray->SetU(tcIndex, (s8)ReadBuffer8() * tcScaleU[tcIndex]);   
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (s8)ReadBuffer8() * tcScaleV[tcIndex]);   
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_UShort(void* _p)
{
	varray->SetU(tcIndex, ReadBuffer16() * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, ReadBuffer16() * tcScaleV[tcIndex]);
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Short(void* _p)
{
	varray->SetU(tcIndex, (s16)ReadBuffer16() * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (s16)ReadBuffer16() * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Float(void* _p)
{
	varray->SetU(tcIndex, ReadBuffer32F() * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, ReadBuffer32F() * tcScaleV[tcIndex]);
	tcIndex++;
}

// ==================================================================================
void LOADERDECL TexCoord_ReadIndex8_UByte(void* _p)	
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(u8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(u8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Byte(void* _p)		
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(s8)Memory_Read_U8(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(s8)Memory_Read_U8(iAddress+1) * tcScaleV[tcIndex]);

	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_UShort(void* _p)	
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(u16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(u16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Short(void* _p)	
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	varray->SetU(tcIndex, (float)(s16)Memory_Read_U16(iAddress) * tcScaleU[tcIndex]);
	if (tcElements[tcIndex])
		varray->SetV(tcIndex, (float)(s16)Memory_Read_U16(iAddress+2) * tcScaleV[tcIndex]);
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Float(void* _p)	
{
	u16 Index = ReadBuffer8(); 
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
void LOADERDECL TexCoord_ReadIndex16_UByte(void* _p)	
{
	u16 Index = ReadBuffer16(); 
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
void LOADERDECL TexCoord_ReadIndex16_Byte(void* _p)	
{
	u16 Index = ReadBuffer16(); 
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

void LOADERDECL TexCoord_ReadIndex16_UShort(void* _p)	
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = ReadBuffer16(); 
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
void LOADERDECL TexCoord_ReadIndex16_Short(void* _p)	
{
	u16 Index = ReadBuffer16(); 
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
void LOADERDECL TexCoord_ReadIndex16_Float(void* _p)	
{
	u16 Index = ReadBuffer16(); 
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