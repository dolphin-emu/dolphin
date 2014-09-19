// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include "Common/CommonFuncs.h"
#include "Common/Hash.h"
#if _M_SSE >= 0x402
#include "Common/CPUDetect.h"
#include <nmmintrin.h>
#endif

static u64 (*ptrHashFunction)(const u8 *src, int len, u32 samples) = &GetMurmurHash3;

// uint32_t
// WARNING - may read one more byte!
// Implementation from Wikipedia.
u32 HashFletcher(const u8* data_u8, size_t length)
{
	const u16* data = (const u16*)data_u8; /* Pointer to the data to be summed */
	size_t len = (length + 1) / 2; /* Length in 16-bit words */
	u32 sum1 = 0xffff, sum2 = 0xffff;

	while (len)
	{
		size_t tlen = len > 360 ? 360 : len;
		len -= tlen;

		do {
			sum1 += *data++;
			sum2 += sum1;
		}
		while (--tlen);

		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}

	// Second reduction step to reduce sums to 16 bits
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	return(sum2 << 16 | sum1);
}


// Implementation from Wikipedia
// Slightly slower than Fletcher above, but slightly more reliable.
#define MOD_ADLER 65521
// data: Pointer to the data to be summed; len is in bytes
u32 HashAdler32(const u8* data, size_t len)
{
	u32 a = 1, b = 0;

	while (len)
	{
		size_t tlen = len > 5550 ? 5550 : len;
		len -= tlen;

		do
		{
			a += *data++;
			b += a;
		}
		while (--tlen);

		a = (a & 0xffff) + (a >> 16) * (65536 - MOD_ADLER);
		b = (b & 0xffff) + (b >> 16) * (65536 - MOD_ADLER);
	}

	// It can be shown that a <= 0x1013a here, so a single subtract will do.
	if (a >= MOD_ADLER)
	{
		a -= MOD_ADLER;
	}

	// It can be shown that b can reach 0xfff87 here.
	b = (b & 0xffff) + (b >> 16) * (65536 - MOD_ADLER);

	if (b >= MOD_ADLER)
	{
		b -= MOD_ADLER;
	}

	return((b << 16) | a);
}

// Stupid hash - but can't go back now :)
// Don't use for new things. At least it's reasonably fast.
u32 HashEctor(const u8* ptr, int length)
{
	u32 crc = 0;

	for (int i = 0; i < length; i++)
	{
		crc ^= ptr[i];
		crc = (crc << 3) | (crc >> 29);
	}

	return(crc);
}


#if _ARCH_64

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

inline u64 getblock(const u64 * p, int i)
{
	return p[i];
}

//----------
// Block mix - combine the key bits with the hash bits and scramble everything

inline void bmix64(u64 & h1, u64 & h2, u64 & k1, u64 & k2, u64 & c1, u64 & c2)
{
	k1 *= c1;
	k1  = _rotl64(k1,23);
	k1 *= c2;
	h1 ^= k1;
	h1 += h2;

	h2 = _rotl64(h2,41);

	k2 *= c2;
	k2  = _rotl64(k2,23);
	k2 *= c1;
	h2 ^= k2;
	h2 += h1;

	h1 = h1*3+0x52dce729;
	h2 = h2*3+0x38495ab5;

	c1 = c1*5+0x7b7d159c;
	c2 = c2*5+0x6bce6396;
}

//----------
// Finalization mix - avalanches all bits to within 0.05% bias

inline u64 fmix64(u64 k)
{
	k ^= k >> 33;
	k *= 0xff51afd7ed558ccd;
	k ^= k >> 33;
	k *= 0xc4ceb9fe1a85ec53;
	k ^= k >> 33;

	return k;
}

u64 GetMurmurHash3(const u8 *src, int len, u32 samples)
{
	const u8 * data = (const u8*)src;
	const int nblocks = len / 16;
	u32 Step = (len / 8);
	if (samples == 0) samples = std::max(Step, 1u);
	Step = Step / samples;
	if (Step < 1) Step = 1;

	u64 h1 = 0x9368e53c2f6af274;
	u64 h2 = 0x586dcd208f7cd3fd;

	u64 c1 = 0x87c37b91114253d5;
	u64 c2 = 0x4cf5ad432745937f;


	//----------
	// body

	const u64 * blocks = (const u64 *)(data);

	for (int i = 0; i < nblocks; i+=Step)
	{
		u64 k1 = getblock(blocks,i*2+0);
		u64 k2 = getblock(blocks,i*2+1);

		bmix64(h1,h2,k1,k2,c1,c2);
	}

	//----------
	// tail

	const u8 * tail = (const u8*)(data + nblocks*16);

	u64 k1 = 0;
	u64 k2 = 0;

	switch (len & 15)
	{
	case 15: k2 ^= u64(tail[14]) << 48;
	case 14: k2 ^= u64(tail[13]) << 40;
	case 13: k2 ^= u64(tail[12]) << 32;
	case 12: k2 ^= u64(tail[11]) << 24;
	case 11: k2 ^= u64(tail[10]) << 16;
	case 10: k2 ^= u64(tail[ 9]) << 8;
	case  9: k2 ^= u64(tail[ 8]) << 0;

	case  8: k1 ^= u64(tail[ 7]) << 56;
	case  7: k1 ^= u64(tail[ 6]) << 48;
	case  6: k1 ^= u64(tail[ 5]) << 40;
	case  5: k1 ^= u64(tail[ 4]) << 32;
	case  4: k1 ^= u64(tail[ 3]) << 24;
	case  3: k1 ^= u64(tail[ 2]) << 16;
	case  2: k1 ^= u64(tail[ 1]) << 8;
	case  1: k1 ^= u64(tail[ 0]) << 0;
			bmix64(h1,h2,k1,k2,c1,c2);
	};

	//----------
	// finalization

	h2 ^= len;

	h1 += h2;
	h2 += h1;

	h1 = fmix64(h1);
	h2 = fmix64(h2);

	h1 += h2;

	return h1;
}


// CRC32 hash using the SSE4.2 instruction
u64 GetCRC32(const u8 *src, int len, u32 samples)
{
#if _M_SSE >= 0x402
	u64 h = len;
	u32 Step = (len / 8);
	const u64 *data = (const u64 *)src;
	const u64 *end = data + Step;
	if (samples == 0) samples = std::max(Step, 1u);
	Step = Step / samples;
	if (Step < 1) Step = 1;
	while (data < end)
	{
		h = _mm_crc32_u64(h, data[0]);
		data += Step;
	}

	const u8 *data2 = (const u8*)end;
	return _mm_crc32_u64(h, u64(data2[0]));
#else
	return 0;
#endif
}


/*
 * NOTE: This hash function is used for custom texture loading/dumping, so
 * it should not be changed, which would require all custom textures to be
 * recalculated for their new hash values. If the hashing function is
 * changed, make sure this one is still used when the legacy parameter is
 * true.
 */
u64 GetHashHiresTexture(const u8 *src, int len, u32 samples)
{
	const u64 m = 0xc6a4a7935bd1e995;
	u64 h = len * m;
	const int r = 47;
	u32 Step = (len / 8);
	const u64 *data = (const u64 *)src;
	const u64 *end = data + Step;
	if (samples == 0) samples = std::max(Step, 1u);
	Step = Step / samples;
	if (Step < 1) Step = 1;
	while (data < end)
	{
		u64 k = data[0];
		data+=Step;
		k *= m;
		k ^= k >> r;
		k *= m;
		h ^= k;
		h *= m;
	}

	const u8 * data2 = (const u8*)end;

	switch (len & 7)
	{
	case 7: h ^= u64(data2[6]) << 48;
	case 6: h ^= u64(data2[5]) << 40;
	case 5: h ^= u64(data2[4]) << 32;
	case 4: h ^= u64(data2[3]) << 24;
	case 3: h ^= u64(data2[2]) << 16;
	case 2: h ^= u64(data2[1]) << 8;
	case 1: h ^= u64(data2[0]);
			h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}
#else
// CRC32 hash using the SSE4.2 instruction
u64 GetCRC32(const u8 *src, int len, u32 samples)
{
#if _M_SSE >= 0x402
	u32 h = len;
	u32 Step = (len/4);
	const u32 *data = (const u32 *)src;
	const u32 *end = data + Step;
	if (samples == 0) samples = std::max(Step, 1u);
	Step  = Step / samples;
	if (Step < 1) Step = 1;
	while (data < end)
	{
		h = _mm_crc32_u32(h, data[0]);
		data += Step;
	}

	const u8 *data2 = (const u8*)end;
	return (u64)_mm_crc32_u32(h, u32(data2[0]));
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

inline u32 getblock(const u32 * p, int i)
{
	return p[i];
}

//----------
// Finalization mix - force all bits of a hash block to avalanche

// avalanches all bits to within 0.25% bias

inline u32 fmix32(u32 h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

inline void bmix32(u32 & h1, u32 & h2, u32 & k1, u32 & k2, u32 & c1, u32 & c2)
{
	k1 *= c1;
	k1  = _rotl(k1,11);
	k1 *= c2;
	h1 ^= k1;
	h1 += h2;

	h2 = _rotl(h2,17);

	k2 *= c2;
	k2  = _rotl(k2,11);
	k2 *= c1;
	h2 ^= k2;
	h2 += h1;

	h1 = h1*3+0x52dce729;
	h2 = h2*3+0x38495ab5;

	c1 = c1*5+0x7b7d159c;
	c2 = c2*5+0x6bce6396;
}

//----------

u64 GetMurmurHash3(const u8* src, int len, u32 samples)
{
	const u8 * data = (const u8*)src;
	u32 out[2];
	const int nblocks = len / 8;
	u32 Step = (len / 4);
	if (samples == 0) samples = std::max(Step, 1u);
	Step = Step / samples;
	if (Step < 1) Step = 1;

	u32 h1 = 0x8de1c3ac;
	u32 h2 = 0xbab98226;

	u32 c1 = 0x95543787;
	u32 c2 = 0x2ad7eb25;

	//----------
	// body

	const u32 * blocks = (const u32 *)(data + nblocks*8);

	for (int i = -nblocks; i < 0; i+=Step)
	{
		u32 k1 = getblock(blocks,i*2+0);
		u32 k2 = getblock(blocks,i*2+1);

		bmix32(h1,h2,k1,k2,c1,c2);
	}

	//----------
	// tail

	const u8 * tail = (const u8*)(data + nblocks*8);

	u32 k1 = 0;
	u32 k2 = 0;

	switch (len & 7)
	{
	case 7: k2 ^= tail[6] << 16;
	case 6: k2 ^= tail[5] << 8;
	case 5: k2 ^= tail[4] << 0;
	case 4: k1 ^= tail[3] << 24;
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0] << 0;
	        bmix32(h1,h2,k1,k2,c1,c2);
	};

	//----------
	// finalization

	h2 ^= len;

	h1 += h2;
	h2 += h1;

	h1 = fmix32(h1);
	h2 = fmix32(h2);

	h1 += h2;
	h2 += h1;

	out[0] = h1;
	out[1] = h2;

	return *((u64 *)&out);
}

/*
 * FIXME: The old 32-bit version of this hash made different hashes than the
 * 64-bit version. Until someone can make a new version of the 32-bit one that
 * makes identical hashes, this is just a c/p of the 64-bit one.
 */
u64 GetHashHiresTexture(const u8 *src, int len, u32 samples)
{
	const u64 m = 0xc6a4a7935bd1e995ULL;
	u64 h = len * m;
	const int r = 47;
	u32 Step = (len / 8);
	const u64 *data = (const u64 *)src;
	const u64 *end = data + Step;
	if (samples == 0) samples = std::max(Step, 1u);
	Step = Step / samples;
	if (Step < 1) Step = 1;
	while (data < end)
	{
		u64 k = data[0];
		data+=Step;
		k *= m;
		k ^= k >> r;
		k *= m;
		h ^= k;
		h *= m;
	}

	const u8 * data2 = (const u8*)end;

	switch (len & 7)
	{
	case 7: h ^= u64(data2[6]) << 48;
	case 6: h ^= u64(data2[5]) << 40;
	case 5: h ^= u64(data2[4]) << 32;
	case 4: h ^= u64(data2[3]) << 24;
	case 3: h ^= u64(data2[2]) << 16;
	case 2: h ^= u64(data2[1]) << 8;
	case 1: h ^= u64(data2[0]);
			h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}
#endif

u64 GetHash64(const u8 *src, int len, u32 samples)
{
	return ptrHashFunction(src, len, samples);
}

// sets the hash function used for the texture cache
void SetHash64Function(bool useHiresTextures)
{
	if (useHiresTextures)
	{
		ptrHashFunction = &GetHashHiresTexture;
	}
#if _M_SSE >= 0x402
	else if (cpu_info.bSSE4_2 && !useHiresTextures) // sse crc32 version
	{
		ptrHashFunction = &GetCRC32;
	}
#endif
	else
	{
		ptrHashFunction = &GetMurmurHash3;
	}
}



