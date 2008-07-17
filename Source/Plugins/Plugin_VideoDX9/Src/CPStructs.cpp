#include <stdio.h>

#include "CPStructs.h"
#include "XFStructs.h"
#include "TransformEngine.h"
#include "VertexHandler.h"
#include "VertexLoader.h"

TMatrixIndexA MatrixIndexA;
TMatrixIndexB MatrixIndexB;

// PROBLEM - matrix switching within vbuffers may be stateful!

void CPUpdateMatricesA()
{
	float *flipmem = (float *)xfmem;
	CTransformEngine::SetPosNormalMatrix(
		flipmem + MatrixIndexA.PosNormalMtxIdx * 4, //CHECK
		flipmem + 0x400 + 3 * (MatrixIndexA.PosNormalMtxIdx & 31)); //CHECK
	CTransformEngine::SetTexMatrix(0,flipmem + MatrixIndexA.Tex0MtxIdx * 4);  
	CTransformEngine::SetTexMatrix(1,flipmem + MatrixIndexA.Tex1MtxIdx * 4);
	CTransformEngine::SetTexMatrix(2,flipmem + MatrixIndexA.Tex2MtxIdx * 4);
	CTransformEngine::SetTexMatrix(3,flipmem + MatrixIndexA.Tex3MtxIdx * 4);
}

void CPUpdateMatricesB()
{
	float *flipmem = (float *)xfmem;
	CTransformEngine::SetTexMatrix(4,flipmem + MatrixIndexB.Tex4MtxIdx * 4);  
	CTransformEngine::SetTexMatrix(5,flipmem + MatrixIndexB.Tex5MtxIdx * 4);
	CTransformEngine::SetTexMatrix(6,flipmem + MatrixIndexB.Tex6MtxIdx * 4);
	CTransformEngine::SetTexMatrix(7,flipmem + MatrixIndexB.Tex7MtxIdx * 4);
}

void LoadCPReg(u32 SubCmd, u32 Value)
{
	switch (SubCmd & 0xF0)
	{
	case 0x30: 	
		MatrixIndexA.Hex = Value; 
		CPUpdateMatricesA();
		break;
	case 0x40: 	
		MatrixIndexB.Hex = Value; 
		CPUpdateMatricesB();
		break;			

	case 0x50: CVertexHandler::Flush(); VertexLoader::SetVtxDesc_Lo(Value); break;
	case 0x60: CVertexHandler::Flush(); VertexLoader::SetVtxDesc_Hi(Value); break;

	case 0x70: g_VertexLoaders[SubCmd & 7].SetVAT_group0(Value); _assert_((SubCmd & 0x0F) < 8); break;
	case 0x80: g_VertexLoaders[SubCmd & 7].SetVAT_group1(Value); _assert_((SubCmd & 0x0F) < 8); break;
	case 0x90: g_VertexLoaders[SubCmd & 7].SetVAT_group2(Value); _assert_((SubCmd & 0x0F) < 8); break;

	case 0xA0: arraybases[SubCmd & 0xF]   = Value & 0xFFFFFFFF; break;
	case 0xB0: arraystrides[SubCmd & 0xF] = Value & 0xFF;      break;
	}				
}


#define BEGINSAVELOAD char *optr=ptr;
#define SAVELOAD(what,size) memcpy((void*)((save)?(void*)(ptr):(void*)(what)),(void*)((save)?(void*)(what):(void*)(ptr)),(size)); ptr+=(size);
#define ENDSAVELOAD return ptr-optr;

size_t CPSaveLoadState(char *ptr, BOOL save)
{
	BEGINSAVELOAD;
	SAVELOAD(arraybases,16*sizeof(u32));
	SAVELOAD(arraystrides,16*sizeof(u32));
	SAVELOAD(&MatrixIndexA,sizeof(TMatrixIndexA));
	SAVELOAD(&MatrixIndexB,sizeof(TMatrixIndexB));
	if (!save)
	{
		CPUpdateMatricesA();
		CPUpdateMatricesB();
	}
	ENDSAVELOAD;
}