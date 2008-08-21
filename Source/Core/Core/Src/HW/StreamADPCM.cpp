/*********************************************************************

        Nintendo GameCube ADPCM Decoder Core Class
        Author: Shinji Chiba <ch3@mail.goo.ne.jp>

*********************************************************************/

#include "StreamADPCM.H"

// STATE_TO_SAVE
float NGCADPCM::iir1[STEREO],
	  NGCADPCM::iir2[STEREO];

void NGCADPCM::InitFilter()
{
	iir1[0] = iir1[1] =
	iir2[0] = iir2[1] = 0.0f;
}

short NGCADPCM::DecodeSample( int bits, int q, int ch )
{
	static const float coef[4] = { 0.86f, 1.8f, 0.82f, 1.53f };
	float iir_filter;

	iir_filter = (float) ((short) (bits << 12) >> (q & 0xf));
	switch (q >> 4)
	{
	case 1:
		iir_filter += (iir1[ch] * coef[0]);
		break;
	case 2:
		iir_filter += (iir1[ch] * coef[1] - iir2[ch] * coef[2]);
		break;
	case 3:
		iir_filter += (iir1[ch] * coef[3] - iir2[ch] * coef[0]);
	}

	iir2[ch] = iir1[ch];
	if ( iir_filter <= -32768.5f )
	{
		iir1[ch] = -32767.5f;
		return -32767;
	}
	else if ( iir_filter >= 32767.5f )
	{
		iir1[ch] = 32767.5f;
		return 32767;
	}
	return (short) ((iir1[ch] = iir_filter) * 0.5f);
}

void NGCADPCM::DecodeBlock( short *pcm, u8* adpcm)
{
	int ch = 1;
	int i;
	for( i = 0; i < SAMPLES_PER_BLOCK; i++ )
	{
		pcm[i * STEREO]        = DecodeSample( adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] & 0xf, adpcm[0],  0 );
		pcm[i * STEREO + MONO] = DecodeSample( adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] >> 4,  adpcm[ch], ch );
	}
}
