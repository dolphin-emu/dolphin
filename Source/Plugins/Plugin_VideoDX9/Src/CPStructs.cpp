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

#include <stdio.h>

#include "CPStructs.h"
#include "XFStructs.h"
#include "VertexManager.h"
#include "VertexLoader.h"

// PROBLEM - matrix switching within vbuffers may be stateful!

void CPUpdateMatricesA()
{
	const float *flipmem = (const float *)xfmem;
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

	case 0x50:
		VertexManager::Flush(); VertexLoader::SetVtxDesc_Lo(Value);
		break;
	case 0x60:
		VertexManager::Flush(); VertexLoader::SetVtxDesc_Hi(Value);
		break;

	case 0x70: g_VertexLoaders[SubCmd & 7].SetVAT_group0(Value); _assert_((SubCmd & 0x0F) < 8); break;
	case 0x80: g_VertexLoaders[SubCmd & 7].SetVAT_group1(Value); _assert_((SubCmd & 0x0F) < 8); break;
	case 0x90: g_VertexLoaders[SubCmd & 7].SetVAT_group2(Value); _assert_((SubCmd & 0x0F) < 8); break;

	case 0xA0: arraybases[SubCmd & 0xF]   = Value & 0xFFFFFFFF; break;
	case 0xB0: arraystrides[SubCmd & 0xF] = Value & 0xFF;      break;
	}				
}
