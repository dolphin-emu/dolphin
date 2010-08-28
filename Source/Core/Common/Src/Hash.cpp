// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Hash.h"

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
// Slightly slower than Fletcher above, but slighly more reliable.
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


// Another fast and decent hash
u32 HashFNV(const u8* ptr, int length)
{
	u32 hash = 0x811c9dc5;

	for (int i = 0; i < length; i++)
	{
		hash *= 1677761;
		hash ^= ptr[i];
	}

	return(hash);
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

#ifdef _M_X64
u64 GetHash64(const u8 *src, int len, u32 samples)
{
	const u64 m = 0xc6a4a7935bd1e995;
	const int r = 47;

	u64 h = len * m;
	u32 Step = (len/8);
	const u64 * data = (const u64 *)src;
	const u64 * end = data + Step;
	if(samples == 0) samples = Step;
	Step  = Step / samples;
	if(Step < 1) Step = 1;
	while(data < end)
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

	switch(len & 7)
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
u64 GetHash64(const u8 *src, int len, u32 samples)
{
	const u32 m = 0x5bd1e995;
	const int r = 24;
	
	u32 h1 = len;
	u32 h2 = 0;

	u32 Step = (len / 4);
	const u32 * data = (const u32 *)src;
	const u32 * end = data + Step;
	const u8 * uEnd = (const u8 *)end;
	if(samples == 0) samples = Step;
	Step = Step / samples;

	if(Step < 2) Step = 2;	

	while(data < end)
	{
		u32 k1 = data[0];
		k1 *= m; 
		k1 ^= k1 >> r; 
		k1 *= m;
		h1 *= m; 
		h1 ^= k1;
		

		u32 k2 = data[1];
		k2 *= m; 
		k2 ^= k2 >> r; 
		k2 *= m;
		h2 *= m; 
		h2 ^= k2;
		data+=Step;
	}

	if((len & 7) > 3)
	{
		u32 k1 = *(end - 1);
		k1 *= m; 
		k1 ^= k1 >> r; 
		k1 *= m;
		h1 *= m; 
		h1 ^= k1;
		len -= 4;
	}

	switch(len & 3)
	{
	case 3: h2 ^= uEnd[2] << 16;
	case 2: h2 ^= uEnd[1] << 8;
	case 1: h2 ^= uEnd[0];
			h2 *= m;
	};

	h1 ^= h2 >> 18; h1 *= m;
	h2 ^= h1 >> 22; h2 *= m;
	h1 ^= h2 >> 17; h1 *= m;
	h2 ^= h1 >> 19; h2 *= m;

	u64 h = h1;

	h = (h << 32) | h2;

	return h;
}


#endif
