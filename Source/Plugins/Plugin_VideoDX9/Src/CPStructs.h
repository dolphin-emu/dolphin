#ifndef _CPSTRUCTS_H
#define _CPSTRUCTS_H

#include "Common.h"
#include "CPMemory.h"
#include "XFMemory.h"

void CPUpdateMatricesA();
void CPUpdateMatricesB();
size_t CPSaveLoadState(char *ptr, BOOL save);
void LoadCPReg(u32 SubCmd, u32 Value);

#endif
