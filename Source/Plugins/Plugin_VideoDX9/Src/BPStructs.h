#ifndef _BPSTRUCTS_H
#define _BPSTRUCTS_H

#pragma pack(4)

#include "BPMemory.h"

void BPInit();
//bool BPWritten(int addr, int changes);
void LoadBPReg(u32 value0);
void ActivateTextures();

#endif