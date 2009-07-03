// Adapted from in_cube by hcs & destop

#include "StreamADPCM.H"

#define ONE_BLOCK_SIZE		32
#define SAMPLES_PER_BLOCK	28

// STATE_TO_SAVE (not saved yet!)
static int histl1;
static int histl2;
static int histr1;
static int histr2;

short ADPDecodeSample(int bits, int q, int *hist1p, int *hist2p)
{
	const int hist1 = *hist1p;
	const int hist2 = *hist2p;
	
	int hist = 0;
	switch (q >> 4)
	{
	case 0:
		hist = 0;
		break;
	case 1:
		hist = (hist1 * 0x3c);
		break;
	case 2:
		hist = (hist1 * 0x73) - (hist2 * 0x34);
		break;
	case 3:
		hist = (hist1 * 0x62) - (hist2 * 0x37);
		break;
	}
	hist = (hist + 0x20) >> 6;
	if (hist >  0x1fffff) hist =  0x1fffff;
	if (hist < -0x200000) hist = -0x200000;

	int cur = (((short)(bits << 12) >> (q & 0xf)) << 6) + hist;
	
	*hist2p = *hist1p;
	*hist1p = cur;

	cur >>= 6;

	if (cur < -0x8000) return -0x8000;
	if (cur >  0x7fff) return  0x7fff;

	return (short)cur;
}

void NGCADPCM::InitFilter()
{
	histl1 = 0;
	histl2 = 0;
	histr1 = 0;
	histr2 = 0;
}

void NGCADPCM::DecodeBlock(short *pcm, const u8 *adpcm)
{
	for (int i = 0; i < SAMPLES_PER_BLOCK; i++)
	{
		pcm[i * 2]     = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] & 0xf, adpcm[0], &histl1, &histl2);
		pcm[i * 2 + 1] = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] >> 4,  adpcm[1], &histr1, &histr2);
	}
}
