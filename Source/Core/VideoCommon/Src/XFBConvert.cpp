// Copyright (C) 2003-2008 Dolphin Project.

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

#include "XFBConvert.h"
#include "Common.h"

// TODO: Convert this thing into wicked fast SSE2.

namespace {

int bound(int i)
{
	return (i>255)?255:((i<0)?0:i);
}

void yuv2rgb(int y, int u, int v, int &r, int &g, int &b)
{
	b = bound((76283*(y - 16) + 132252*(u - 128))>>16);
	g = bound((76283*(y - 16) - 53281 *(v - 128) - 25624*(u - 128))>>16); //last one u?
	r = bound((76283*(y - 16) + 104595*(v - 128))>>16);
}

void rgb2yuv(int r, int g, int b, int &y, int &u, int &v)
{
	y = (((16843 * r) + (33030 * g) + (6423 * b)) >> 16) + 16;
	v = (((28770 * r) - (24117 * g) - (4653 * b)) >> 16) + 128;
	u = ((-(9699 * r) - (19071 * g) + (28770 * b)) >> 16) + 128;
}

}  // namespace

void ConvertFromXFB(u32 *dst, const u8* _pXFB, int width, int height)
{
	const unsigned char *src = _pXFB;

	u32 numBlocks = (width * height) / 2;

	for (u32 i = 0; i < numBlocks; i++, src += 4)
	{
		int Y1 = src[0];
		int U  = src[1];
		int Y2 = src[2];
		int V  = src[3];

		int r, g, b;
		yuv2rgb(Y1,U,V, r,g,b);
		*dst++ = 0xFF000000 | (r<<16) | (g<<8) | (b);
		yuv2rgb(Y2,U,V, r,g,b);
		*dst++ = 0xFF000000 | (r<<16) | (g<<8) | (b);
	}	
}

void ConvertToXFB(u32 *dst, const u8* _pEFB, int width, int height)
{
	const unsigned char *src = _pEFB;

	u32 numBlocks = (width * height) / 2;

	for (u32 i = 0; i < numBlocks; i++)
	{
		int y1 = (((16843 * src[0]) + (33030 * src[1]) + (6423 * src[2])) >> 16) + 16;		
		int u1 = ((-(9699 * src[0]) - (19071 * src[1]) + (28770 * src[2])) >> 16) + 128;
		src += 4;

		int y2 = (((16843 * src[0]) + (33030 * src[1]) + (6423 * src[2])) >> 16) + 16;
		int v2 = (((28770 * src[0]) - (24117 * src[1]) - (4653 * src[2])) >> 16) + 128;
		src += 4;

		*dst++ = (v2 << 24) | (y2 << 16) | (u1 << 8) | (y1);
	}	
}

