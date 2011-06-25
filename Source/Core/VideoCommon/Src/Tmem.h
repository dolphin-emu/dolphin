// Tmem.h
// Nolan Check
// Created 5/3/2011

#ifndef _TMEM_H
#define _TMEM_H

#include "VideoCommon.h"

enum 
{
    TMEM_SIZE = 1024*1024,
	TMEM_HALF = TMEM_SIZE/2
};

// This does not hold the real cache. It only holds TLUTs and preloaded
// textures. Cached areas are handled externally.
extern u8 g_texMem[];

void LoadTmem(u32 tmemAddr, const u8* src, u32 size);

#endif
