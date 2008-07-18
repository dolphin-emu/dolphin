#ifndef _LOOKUPTABLES_H
#define _LOOKUPTABLES_H

#include "Common.h"

extern int lut3to8[8];
extern int lut4to8[16];
extern int lut5to8[32];
extern int lut6to8[64];
extern float lutu8tosfloat[256];
extern float lutu8toufloat[256];
extern float luts8tosfloat[256];

void InitLUTs();

#endif
