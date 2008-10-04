#ifndef _GLOBALS_H
#define _GLOBALS_H



#include "Common.h"
#include "pluginspecs_dsp.h"

extern DSPInitialize g_dspInitialize;
void DebugLog(const char* _fmt, ...);

u8 Memory_Read_U8(u32 _uAddress);
u16 Memory_Read_U16(u32 _uAddress);
u32 Memory_Read_U32(u32 _uAddress);
float Memory_Read_Float(u32 _uAddress);


#endif

