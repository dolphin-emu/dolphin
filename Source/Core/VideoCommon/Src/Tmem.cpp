// Tmem.cpp
// Nolan Check
// Created 5/3/2011

#include "Tmem.h"

// TRAM
// STATE_TO_SAVE
GC_ALIGNED16(u8 g_texMem[TMEM_SIZE]);

void LoadTmem(u32 tmemAddr, const u8* src, u32 size)
{
	_assert_msg_(VIDEO, tmemAddr + size <= TMEM_SIZE,
		"Tried to write too much to TMEM");
	memcpy(&g_texMem[tmemAddr], src, size);
}
