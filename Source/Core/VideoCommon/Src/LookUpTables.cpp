#include "LookUpTables.h"

int lut3to8[8];
int lut4to8[16];
int lut5to8[32];
int lut6to8[64];
float lutu8tosfloat[256];
float lutu8toufloat[256];
float luts8tosfloat[256];

void InitLUTs()
{
    for (int i = 0; i < 8; i++)
        lut3to8[i] = (i*255) / 7;
    for (int i = 0; i < 16; i++)
        lut4to8[i] = (i*255) / 15;
    for (int i = 0; i < 32; i++)
        lut5to8[i] = (i*255) / 31;	
    for (int i = 0; i < 64; i++)
        lut6to8[i] = (i*255) / 63;
    for (int i = 0; i < 256; i++)
    {
        lutu8tosfloat[i] = (float)(i-128) / 127.0f;
        lutu8toufloat[i] = (float)(i) / 255.0f;
        luts8tosfloat[i] = ((float)(signed char)(char)i) / 127.0f;
    }
}
