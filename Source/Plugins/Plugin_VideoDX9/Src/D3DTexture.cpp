// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "D3DTexture.h"

#include "CPUDetect.h"

#if _M_SSE >= 0x401
#include <smmintrin.h>
#include <emmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

namespace DX9
{

namespace D3D
{

void ConvertRGBA_BGRA_SSE2(u32 *dst, const int dstPitch, u32 *pIn, const int width, const int height, const int pitch)
{
	// Converts RGBA to BGRA:
	// TODO: this would be totally unnecessary if we just change the TextureDecoder_RGBA to decode
	// to BGRA instead.
	for (int y = 0; y < height; y++, pIn += pitch)
	{
		u8 *pIn8 = (u8 *)pIn;
		u8 *pBits = (u8 *)((u8*)dst + (y * dstPitch));

		// Batch up loads/stores into 16 byte chunks to use SSE2 efficiently:
		int sse2blocks = (width * 4) / 16;
		int sse2remainder = (width * 4) & 15;

		// Do conversions in batches of 16 bytes:
		if (sse2blocks > 0)
		{
			// Generate a constant of all FF bytes:
			const __m128i allFFs128 = _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
			__m128i *src128 = (__m128i *)pIn8;
			__m128i *dst128 = (__m128i *)pBits;

			// Increment by 16 bytes at a time:
			for (int i = 0; i < sse2blocks; ++i, ++dst128, ++src128)
			{
				// Load up 4 colors simultaneously:
				__m128i rgba = _mm_loadu_si128(src128);
				// Swap the R and B components:
				// Isolate the B component and shift it left 16 bits:
				// ABGR
				const __m128i bMask = _mm_srli_epi32(allFFs128, 24);
				const __m128i bNew = _mm_slli_epi32(_mm_and_si128(rgba, bMask), 16);
				// Isolate the R component and shift it right 16 bits:
				const __m128i rMask = _mm_slli_epi32(bMask, 16);
				const __m128i rNew = _mm_srli_epi32(_mm_and_si128(rgba, rMask), 16);
				// Now mask off the old R and B components from the rgba data to get 0g0a:
				const __m128i _g_a = _mm_or_si128(
					_mm_and_si128(
						rgba,
						_mm_or_si128(
							_mm_slli_epi32(bMask, 8),
							_mm_slli_epi32(rMask, 8)
						)
					),
					_mm_or_si128(rNew, bNew)
				);
				// Finally, OR up all the individual components to get BGRA:
				const __m128i bgra = _mm_or_si128(_g_a, _mm_or_si128(rNew, bNew));
				_mm_storeu_si128(dst128, bgra);
			}
		}

		// Take the remainder colors at the end of the row that weren't able to
		// be included into the last 16 byte chunk:
		if (sse2remainder > 0)
		{
			for (int x = (sse2blocks * 16); x < (width * 4); x += 4)
			{
				pBits[x + 0] = pIn8[x + 2];
				pBits[x + 1] = pIn8[x + 1];
				pBits[x + 2] = pIn8[x + 0];
				pBits[x + 3] = pIn8[x + 3];
			}
		}
	}

	// Memory fence to make sure the stores are good:
	_mm_mfence();
}

void ConvertRGBA_BGRA_SSSE3(u32 *dst, const int dstPitch, u32 *pIn, const int width, const int height, const int pitch)
{
	__m128i mask = _mm_set_epi8(15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);
	for (int y = 0; y < height; y++, pIn += pitch)
	{
		u8 *pIn8 = (u8 *)pIn;
		u8 *pBits = (u8 *)((u8*)dst + (y * dstPitch));

		// Batch up loads/stores into 16 byte chunks to use SSE2 efficiently:
		int ssse3blocks = (width * 4) / 16;
		int ssse3remainder = (width * 4) & 15;

		// Do conversions in batches of 16 bytes:
		if (ssse3blocks > 0)
		{
			__m128i *src128 = (__m128i *)pIn8;
			__m128i *dst128 = (__m128i *)pBits;

			// Increment by 16 bytes at a time:
			for (int i = 0; i < ssse3blocks; ++i, ++dst128, ++src128)
			{
				_mm_storeu_si128(dst128, _mm_shuffle_epi8(_mm_loadu_si128(src128), mask));
			}
		}

		// Take the remainder colors at the end of the row that weren't able to
		// be included into the last 16 byte chunk:
		if (ssse3remainder > 0)
		{
			for (int x = (ssse3blocks * 16); x < (width * 4); x += 4)
			{
				pBits[x + 0] = pIn8[x + 2];
				pBits[x + 1] = pIn8[x + 1];
				pBits[x + 2] = pIn8[x + 0];
				pBits[x + 3] = pIn8[x + 3];
			}
		}
	}

	// Memory fence to make sure the stores are good:
	_mm_mfence();
}

LPDIRECT3DTEXTURE9 CreateTexture2D(const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt, bool swap_r_b, int levels)
{
	u32* pBuffer = (u32*)buffer;
	LPDIRECT3DTEXTURE9 pTexture;

	// crazy bitmagic, sorry :)
	bool isPow2 = !((width&(width-1)) || (height&(height-1)));
	bool bExpand = false;

	if (fmt == D3DFMT_A8P8) {
		fmt = D3DFMT_A8L8;
		bExpand = true;
	}

	HRESULT hr;
	if (levels > 0)
		hr = dev->CreateTexture(width, height, levels, 0, fmt, D3DPOOL_MANAGED, &pTexture, NULL);
	else
		hr = dev->CreateTexture(width, height, 0, D3DUSAGE_AUTOGENMIPMAP, fmt, D3DPOOL_MANAGED, &pTexture, NULL);

	if (FAILED(hr))
		return 0;
	int level = 0;
	D3DLOCKED_RECT Lock;
	pTexture->LockRect(level, &Lock, NULL, 0);
	switch (fmt) 
	{
	case D3DFMT_L8:
	case D3DFMT_A8:
	case D3DFMT_A4L4:
		{
			const u8 *pIn = buffer;
			for (int y = 0; y < height; y++)
			{
				u8* pBits = ((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width);
				pIn += pitch;
			}
		}
		break;
	case D3DFMT_R5G6B5:
		{
			const u16 *pIn = (u16*)buffer;
			for (int y = 0; y < height; y++)
			{
				u16* pBits = (u16*)((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width * 2);
				pIn += pitch;
			}
		}
		break;
	case D3DFMT_A8L8:
		{
			if (bExpand) { // I8
				const u8 *pIn = buffer;
				// TODO(XK): Find a better way that does not involve either unpacking
				//           or downsampling (i.e. A4L4)
				for (int y = 0; y < height; y++)
				{
					u8* pBits = ((u8*)Lock.pBits + (y * Lock.Pitch));
					for(int i = 0; i < width * 2; i += 2) {
						pBits[i] = pIn[i / 2];
						pBits[i + 1] = pIn[i / 2];
					}
					pIn += pitch;
				}
			} else { // IA8
				const u16 *pIn = (u16*)buffer;

				for (int y = 0; y < height; y++)
				{
					u16* pBits = (u16*)((u8*)Lock.pBits + (y * Lock.Pitch));
					memcpy(pBits, pIn, width * 2);
					pIn += pitch;
				}
			}
		}
		break;
	case D3DFMT_A8R8G8B8:
		{
			if(pitch * 4 == Lock.Pitch && !swap_r_b)
			{
				memcpy(Lock.pBits,buffer,Lock.Pitch*height);
			}
			else
			{
				u32* pIn = pBuffer;
				if (!swap_r_b) {
					for (int y = 0; y < height; y++)
					{
						u32 *pBits = (u32*)((u8*)Lock.pBits + (y * Lock.Pitch));
						memcpy(pBits, pIn, width * 4);
						pIn += pitch;
					}
				} else {
#if _M_SSE >= 0x301
					// Uses SSSE3 intrinsics to optimize RGBA -> BGRA swizzle:
					if (cpu_info.bSSSE3) {
						ConvertRGBA_BGRA_SSSE3((u32 *)Lock.pBits, Lock.Pitch, pIn, width, height, pitch);
					} else
#endif
					// Uses SSE2 intrinsics to optimize RGBA -> BGRA swizzle:
					{
						ConvertRGBA_BGRA_SSE2((u32 *)Lock.pBits, Lock.Pitch, pIn, width, height, pitch);
					}
#if 0
					for (int y = 0; y < height; y++)
					{
						u8 *pIn8 = (u8 *)pIn;
						u8 *pBits = (u8 *)((u8*)Lock.pBits + (y * Lock.Pitch));
						for (int x = 0; x < width * 4; x += 4) {
							pBits[x + 0] = pIn8[x + 2];
							pBits[x + 1] = pIn8[x + 1];
							pBits[x + 2] = pIn8[x + 0];
							pBits[x + 3] = pIn8[x + 3];
						}
						pIn += pitch;
					}
#endif
				}
			}
		}
		break;
	case D3DFMT_DXT1:
		memcpy(Lock.pBits,buffer,((width+3)/4)*((height+3)/4)*8);
		break;
	default:
		PanicAlert("D3D: Invalid texture format %i", fmt);
	}
	pTexture->UnlockRect(level); 
	return pTexture;
}

LPDIRECT3DTEXTURE9 CreateOnlyTexture2D(const int width, const int height, D3DFORMAT fmt)
{
	LPDIRECT3DTEXTURE9 pTexture;
	// crazy bitmagic, sorry :)
	bool isPow2 = !((width&(width-1)) || (height&(height-1)));
	bool bExpand = false;
	HRESULT hr;
	// TODO(ector): Allow mipmaps for non-pow textures on newer cards?
	// TODO(ector): Use the game-specified mipmaps?
	if (!isPow2)
		hr = dev->CreateTexture(width, height, 1, 0, fmt, D3DPOOL_MANAGED, &pTexture, NULL);
	else
		hr = dev->CreateTexture(width, height, 0, D3DUSAGE_AUTOGENMIPMAP, fmt, D3DPOOL_MANAGED, &pTexture, NULL);

	if (FAILED(hr))
		return 0;
	return pTexture;
}

void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt, bool swap_r_b, int level)
{
	u32* pBuffer = (u32*)buffer;
	D3DLOCKED_RECT Lock;
	pTexture->LockRect(level, &Lock, NULL, 0);
	u32* pIn = pBuffer;

	bool bExpand = false;

	if (fmt == D3DFMT_A8P8) {
		fmt = D3DFMT_A8L8;
		bExpand = true;
	}
	switch (fmt) 
	{
	case D3DFMT_A8R8G8B8:
		if(pitch * 4 == Lock.Pitch && !swap_r_b)
		{
			memcpy(Lock.pBits, pBuffer, Lock.Pitch*height);
		}
		else if (!swap_r_b)
		{
			for (int y = 0; y < height; y++)
			{
				u32 *pBits = (u32*)((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width * 4);
				pIn += pitch;
			}
		}
		else
		{
#if _M_SSE >= 0x301
			// Uses SSSE3 intrinsics to optimize RGBA -> BGRA swizzle:
			if (cpu_info.bSSSE3) {
				ConvertRGBA_BGRA_SSSE3((u32 *)Lock.pBits, Lock.Pitch, pIn, width, height, pitch);
			} else
#endif
			// Uses SSE2 intrinsics to optimize RGBA -> BGRA swizzle:
			{
				ConvertRGBA_BGRA_SSE2((u32 *)Lock.pBits, Lock.Pitch, pIn, width, height, pitch);
			}
#if 0
			for (int y = 0; y < height; y++)
			{
				u8 *pIn8 = (u8 *)pIn;
				u8 *pBits = (u8 *)((u8*)Lock.pBits + (y * Lock.Pitch));
				for (int x = 0; x < width * 4; x += 4)
				{
					pBits[x + 0] = pIn8[x + 2];
					pBits[x + 1] = pIn8[x + 1];
					pBits[x + 2] = pIn8[x + 0];
					pBits[x + 3] = pIn8[x + 3];
				}
				pIn += pitch;
			}
#endif
		}
		break;
	case D3DFMT_L8:
	case D3DFMT_A8:
	case D3DFMT_A4L4:
		{
			const u8 *pIn = buffer;
			for (int y = 0; y < height; y++)
			{
				u8* pBits = ((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width);
				pIn += pitch;
			}
		}
		break;
	case D3DFMT_R5G6B5:
		{
			const u16 *pIn = (u16*)buffer;
			for (int y = 0; y < height; y++)
			{
				u16* pBits = (u16*)((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width * 2);
				pIn += pitch;
			}
		}
		break;
	case D3DFMT_A8L8:
		{
			if (bExpand) { // I8
				const u8 *pIn = buffer;
				// TODO(XK): Find a better way that does not involve either unpacking
				//           or downsampling (i.e. A4L4)
				for (int y = 0; y < height; y++)
				{
					u8* pBits = ((u8*)Lock.pBits + (y * Lock.Pitch));
					for(int i = 0; i < width * 2; i += 2) {
						pBits[i] = pIn[i / 2];
						pBits[i + 1] = pIn[i / 2];
					}
					pIn += pitch;
				}
			} else { // IA8
				const u16 *pIn = (u16*)buffer;

				for (int y = 0; y < height; y++)
				{
					u16* pBits = (u16*)((u8*)Lock.pBits + (y * Lock.Pitch));
					memcpy(pBits, pIn, width * 2);
					pIn += pitch;
				}
			}
		}
		break;
	case D3DFMT_DXT1:
		memcpy(Lock.pBits,buffer,((width+3)/4)*((height+3)/4)*8);
		break;
	}
	pTexture->UnlockRect(level); 
}

}  // namespace

}  // namespace DX9