// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CommonFuncs.h"
#include "Core/HW/Memmap.h"
#include "VideoBackends/Software/BPMemLoader.h"
#include "VideoBackends/Software/TextureSampler.h"
#include "VideoCommon/TextureDecoder.h"

#define ALLOW_MIPMAP 1

namespace TextureSampler
{

static inline void WrapCoord(int &coord, int wrapMode, int imageSize)
{
	switch (wrapMode)
	{
		case 0: // clamp
			coord = (coord>imageSize)?imageSize:(coord<0)?0:coord;
			break;
		case 1: // wrap
			coord = coord % (imageSize + 1);
			coord = (coord<0)?imageSize+coord:coord;
			break;
		case 2: // mirror
			{
				int sizePlus1 = imageSize + 1;
				int div = coord / sizePlus1;
				coord = coord - (div * sizePlus1);
				coord = (coord<0)?-coord:coord;
				coord = (div&1)?imageSize - coord:coord;
			}
			break;
	}
}

static inline void SetTexel(u8 *inTexel, u32 *outTexel, u32 fract)
{
	outTexel[0] = inTexel[0] * fract;
	outTexel[1] = inTexel[1] * fract;
	outTexel[2] = inTexel[2] * fract;
	outTexel[3] = inTexel[3] * fract;
}

static inline void AddTexel(u8 *inTexel, u32 *outTexel, u32 fract)
{
	outTexel[0] += inTexel[0] * fract;
	outTexel[1] += inTexel[1] * fract;
	outTexel[2] += inTexel[2] * fract;
	outTexel[3] += inTexel[3] * fract;
}

void Sample(s32 s, s32 t, s32 lod, bool linear, u8 texmap, u8 *sample)
{
	int baseMip = 0;
	bool mipLinear = false;

#if (ALLOW_MIPMAP)
	FourTexUnits& texUnit = bpmem.tex[(texmap >> 2) & 1];
	TexMode0& tm0 = texUnit.texMode0[texmap & 3];

	s32 lodFract = lod & 0xf;

	if (lod > 0 && tm0.min_filter & 3)
	{
		// use mipmap
		baseMip = lod >> 4;
		mipLinear = (lodFract && tm0.min_filter & 2);

		// if using nearest mip filter and lodFract >= 0.5 round up to next mip
		baseMip += (lodFract >> 3) & (tm0.min_filter & 1);
	}

	if (mipLinear)
	{
		u8 sampledTex[4];
		u32 texel[4];

		SampleMip(s, t, baseMip, linear, texmap, sampledTex);
		SetTexel(sampledTex, texel, (16 - lodFract));

		SampleMip(s, t, baseMip + 1, linear, texmap, sampledTex);
		AddTexel(sampledTex, texel, lodFract);

		sample[0] = (u8)(texel[0] >> 4);
		sample[1] = (u8)(texel[1] >> 4);
		sample[2] = (u8)(texel[2] >> 4);
		sample[3] = (u8)(texel[3] >> 4);
	}
	else
#endif
	{
		SampleMip(s, t, baseMip, linear, texmap, sample);
	}
}

void SampleMip(s32 s, s32 t, s32 mip, bool linear, u8 texmap, u8 *sample)
{
	FourTexUnits& texUnit = bpmem.tex[(texmap >> 2) & 1];
	u8 subTexmap = texmap & 3;

	TexMode0& tm0 = texUnit.texMode0[subTexmap];
	TexImage0& ti0 = texUnit.texImage0[subTexmap];
	TexTLUT& texTlut = texUnit.texTlut[subTexmap];
	TlutFormat tlutfmt = (TlutFormat) texTlut.tlut_format;

	u8 *imageSrc, *imageSrcOdd = nullptr;
	if (texUnit.texImage1[subTexmap].image_type)
	{
		imageSrc = &texMem[texUnit.texImage1[subTexmap].tmem_even * TMEM_LINE_SIZE];
		if (ti0.format == GX_TF_RGBA8)
			imageSrcOdd = &texMem[texUnit.texImage2[subTexmap].tmem_odd * TMEM_LINE_SIZE];
	}
	else
	{
		u32 imageBase = texUnit.texImage3[subTexmap].image_base << 5;
		imageSrc = Memory::GetPointer(imageBase);
	}

	int imageWidth = ti0.width;
	int imageHeight = ti0.height;

	int tlutAddress = texTlut.tmem_offset << 9;
	const u8* tlut = &texMem[tlutAddress];

	// reduce sample location and texture size to mip level
	// move texture pointer to mip location
	if (mip)
	{
		int mipWidth = imageWidth + 1;
		int mipHeight = imageHeight + 1;

		int fmtWidth = TexDecoder_GetBlockWidthInTexels(ti0.format);
		int fmtHeight = TexDecoder_GetBlockHeightInTexels(ti0.format);
		int fmtDepth = TexDecoder_GetTexelSizeInNibbles(ti0.format);

		imageWidth >>= mip;
		imageHeight >>= mip;
		s >>= mip;
		t >>= mip;

		while (mip)
		{
			mipWidth = std::max(mipWidth, fmtWidth);
			mipHeight = std::max(mipHeight, fmtHeight);
			u32 size = (mipWidth * mipHeight * fmtDepth) >> 1;

			imageSrc += size;
			mipWidth >>= 1;
			mipHeight >>= 1;
			mip--;
		}
	}

	if (linear)
	{
		// offset linear sampling
		s -= 64;
		t -= 64;

		// integer part of sample location
		int imageS = s >> 7;
		int imageT = t >> 7;

		// linear sampling
		int imageSPlus1 = imageS + 1;
		int fractS = s & 0x7f;

		int imageTPlus1 = imageT + 1;
		int fractT = t & 0x7f;

		u8 sampledTex[4];
		u32 texel[4];

		WrapCoord(imageS, tm0.wrap_s, imageWidth);
		WrapCoord(imageT, tm0.wrap_t, imageHeight);
		WrapCoord(imageSPlus1, tm0.wrap_s, imageWidth);
		WrapCoord(imageTPlus1, tm0.wrap_t, imageHeight);

		if (!(ti0.format == GX_TF_RGBA8 && texUnit.texImage1[subTexmap].image_type))
		{
			TexDecoder_DecodeTexel(sampledTex, imageSrc, imageS, imageT, imageWidth, ti0.format, tlut, tlutfmt);
			SetTexel(sampledTex, texel, (128 - fractS) * (128 - fractT));

			TexDecoder_DecodeTexel(sampledTex, imageSrc, imageSPlus1, imageT, imageWidth, ti0.format, tlut, tlutfmt);
			AddTexel(sampledTex, texel, (fractS) * (128 - fractT));

			TexDecoder_DecodeTexel(sampledTex, imageSrc, imageS, imageTPlus1, imageWidth, ti0.format, tlut, tlutfmt);
			AddTexel(sampledTex, texel, (128 - fractS) * (fractT));

			TexDecoder_DecodeTexel(sampledTex, imageSrc, imageSPlus1, imageTPlus1, imageWidth, ti0.format, tlut, tlutfmt);
			AddTexel(sampledTex, texel, (fractS) * (fractT));
		}
		else
		{
			TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, imageSrc, imageSrcOdd, imageS, imageT, imageWidth);
			SetTexel(sampledTex, texel, (128 - fractS) * (128 - fractT));

			TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, imageSrc, imageSrcOdd, imageSPlus1, imageT, imageWidth);
			AddTexel(sampledTex, texel, (fractS) * (128 - fractT));

			TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, imageSrc, imageSrcOdd, imageS, imageTPlus1, imageWidth);
			AddTexel(sampledTex, texel, (128 - fractS) * (fractT));

			TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, imageSrc, imageSrcOdd, imageSPlus1, imageTPlus1, imageWidth);
			AddTexel(sampledTex, texel, (fractS) * (fractT));
		}

		sample[0] = (u8)(texel[0] >> 14);
		sample[1] = (u8)(texel[1] >> 14);
		sample[2] = (u8)(texel[2] >> 14);
		sample[3] = (u8)(texel[3] >> 14);
	}
	else
	{
		// integer part of sample location
		int imageS = s >> 7;
		int imageT = t >> 7;

		// nearest neighbor sampling
		WrapCoord(imageS, tm0.wrap_s, imageWidth);
		WrapCoord(imageT, tm0.wrap_t, imageHeight);

		if (!(ti0.format == GX_TF_RGBA8 && texUnit.texImage1[subTexmap].image_type))
			TexDecoder_DecodeTexel(sample, imageSrc, imageS, imageT, imageWidth, ti0.format, tlut, tlutfmt);
		else
			TexDecoder_DecodeTexelRGBA8FromTmem(sample, imageSrc, imageSrcOdd, imageS, imageT, imageWidth);
	}
}

}
