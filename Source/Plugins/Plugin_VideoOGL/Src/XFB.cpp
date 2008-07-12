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

#include "Common.h"
#include "Globals.h"

// __________________________________________________________________________________________________
// Video_UpdateXFB
//

// TODO(ector): Write protect XFB. As soon as something pokes there, 
// switch to a mode where we actually display the XFB on top of the 3D. 
// If no writes have happened within 5 frames, revert to normal mode.

// Also, write a crazy SSE2 optimized version of this :P

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

void ConvertXFB(int *dst, const u8* _pXFB, int width, int height)
{
	const unsigned char *src = _pXFB;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int Y1 = *src++;
			int U = *src++;
			int Y2 = *src++;
			int V = *src++;

			int r,g,b;
			yuv2rgb(Y1, U, V, r, g, b);
			*dst++ = 0xFF000000 | (r<<16) | (g<<8) | (b);
			yuv2rgb(Y2, U, V, r, g, b);
			*dst++ = 0xFF000000 | (r<<16) | (g<<8) | (b);
		}
	}
}
