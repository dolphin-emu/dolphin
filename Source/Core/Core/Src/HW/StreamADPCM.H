/*********************************************************************

        Nintendo GameCube ADPCM Decoder Core Class
        Original Author: Shinji Chiba <ch3@mail.goo.ne.jp>
		Modified to fit Dolphin code style by ector

        History: Mar.19 2001 - Create.

*********************************************************************/

#ifndef _STREAMADPCM_H
#define _STREAMADPCM_H

#include "Common.h"
class ChunkFile;

#define ONE_BLOCK_SIZE		32
#define SAMPLES_PER_BLOCK	28
#define MONO				1
#define STEREO				2

class NGCADPCM
{
public:
	static void InitFilter();
	static void DecodeBlock( short*pcm, u8 *adpcm);
private:
	static float iir1[STEREO], iir2[STEREO];
	static short DecodeSample(int, int, int);
};

#endif

