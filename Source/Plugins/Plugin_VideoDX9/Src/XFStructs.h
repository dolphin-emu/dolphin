#ifndef _XFSTRUCTS_H
#define _XFSTRUCTS_H

#include "Common.h"
#include "Vec3.h"
#include "XFMemory.h"

extern float rawViewPort[6];
extern float rawProjection[7];

void XFUpdateVP();
void XFUpdatePJ();
void LoadXFReg(u32 transferSize, u32 address, u32 *pData);
void LoadIndexedXF(u32 val, int array);

#pragma pack()


#endif