#pragma once

#include "Common.h"

#include "TransformEngine.h"

// ==============================================================================
// Direct
// ==============================================================================
void LOADERDECL PosMtx_ReadDirect_UByte(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	int index = ReadBuffer8();
	float *flipmem = (float *)xfmem;
	varray->SetPosNrmIdx(index);
}

#define MAKETEX(n) \
void LOADERDECL TexMtx_ReadDirect_UByte##n(void* _p) \
{                                         \
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;   \
	int index = ReadBuffer8();            \
	varray->SetTcIdx(n, index);           \
}

MAKETEX(0)
MAKETEX(1)
MAKETEX(2)
MAKETEX(3)
MAKETEX(4)
MAKETEX(5)
MAKETEX(6)
MAKETEX(7)
