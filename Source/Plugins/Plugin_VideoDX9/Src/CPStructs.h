#ifndef _CPSTRUCTS_H
#define _CPSTRUCTS_H

#include "Common.h"
#include "CPMemory.h"
#include "XFMemory.h"

extern TMatrixIndexA MatrixIndexA;
extern TMatrixIndexB MatrixIndexB;
extern u32 arraybases[16];
extern u32 arraystrides[16];

void CPUpdateMatricesA();
void CPUpdateMatricesB();
size_t CPSaveLoadState(char *ptr, BOOL save);
void LoadCPReg(u32 SubCmd, u32 Value);

#endif
