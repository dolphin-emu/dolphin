#ifndef _CPSTRUCTS_H
#define _CPSTRUCTS_H

#include "Common.h"
#include "CPMemory.h"

#pragma pack(4)

//////////////////////////////////////////////////////////////////////////
// Matrix indices
//////////////////////////////////////////////////////////////////////////

union TMatrixIndexA
{
	u32 Hex;
	struct
	{
		unsigned PosNormalMtxIdx : 6;
		unsigned Tex0MtxIdx : 6;
		unsigned Tex1MtxIdx : 6;
		unsigned Tex2MtxIdx : 6;
		unsigned Tex3MtxIdx : 6;
	};
};

union TMatrixIndexB
{
	u32 Hex;
	struct
	{
		unsigned Tex4MtxIdx : 6;
		unsigned Tex5MtxIdx : 6;
		unsigned Tex6MtxIdx : 6;
		unsigned Tex7MtxIdx : 6;
	};
};

#pragma pack ()

extern TMatrixIndexA MatrixIndexA;
extern TMatrixIndexB MatrixIndexB;
extern u32 arraybases[16];
extern u32 arraystrides[16];

void CPUpdateMatricesA();
void CPUpdateMatricesB();
size_t CPSaveLoadState(char *ptr, BOOL save);
void LoadCPReg(u32 SubCmd, u32 Value);

#endif
