// Adapted from in_cube by hcs & destop

#ifndef _STREAMADPCM_H
#define _STREAMADPCM_H

#include "Common.h"

class NGCADPCM
{
public:
	static void InitFilter();
	static void DecodeBlock(short *pcm, const u8 *adpcm);
};

#endif

