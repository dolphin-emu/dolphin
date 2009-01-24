#include <stdarg.h>
#include <stdio.h>

#include "Common.h"
#include "Globals.h"


// debugger externals that are needed even in Release builds
bool gSSBM = true;
bool gSSBMremedy1 = true;
bool gSSBMremedy2 = true;
bool gSequenced = true;
bool gVolume = true;
bool gReset = false;
float ratioFactor; // a global to get the ratio factor from MixAdd

void __Log(int, const char *fmt, ...)
{
	DebugLog(fmt);
}

void DebugLog(const char* _fmt, ...)
{
#ifdef _DEBUG
	char Msg[512];
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Msg, _fmt, ap);
	va_end(ap);

	g_dspInitialize.pLog(Msg, FALSE);
#endif
}

extern u8* g_pMemory;

// TODO: Wii support? Most likely audio data still must be in the old 24MB TRAM.
#define RAM_MASK 0x1FFFFFF

u8 Memory_Read_U8(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return g_pMemory[_uAddress];
}

u16 Memory_Read_U16(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return Common::swap16(*(u16*)&g_pMemory[_uAddress]);
}

u32 Memory_Read_U32(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return Common::swap32(*(u32*)&g_pMemory[_uAddress]);
}

float Memory_Read_Float(u32 _uAddress)
{
	u32 uTemp = Memory_Read_U32(_uAddress);
	return *(float*)&uTemp;
}
