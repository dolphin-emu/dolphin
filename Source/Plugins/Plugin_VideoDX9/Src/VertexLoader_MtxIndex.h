#pragma once

#include "Common.h"

#include "TransformEngine.h"

// ==============================================================================
// Direct
// ==============================================================================
void LOADERDECL PosMtx_ReadDirect_UByte(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	int index = ReadBuffer8() & 0x3f;
	float *flipmem = (float *)xfmem;
	varray->SetPosNrmIdx(index);
}

int s_texmtxread = 0, s_texmtxwrite = 0;
void LOADERDECL TexMtx_ReadDirect_UByte(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	int index = ReadBuffer8() & 0x3f;
	varray->SetTcIdx(s_texmtxread++, index);
}
