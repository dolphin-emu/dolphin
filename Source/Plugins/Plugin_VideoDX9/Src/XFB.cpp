#include "XFB.h"
#include "Common.h"

// __________________________________________________________________________________________________
// Video_UpdateXFB
//

// TODO(ector): Write protect XFB. As soon as something pokes there, 
// switch to a mode where we actually display the XFB on top of the 3D. 
// If no writes have happened within 5 frames, revert to normal mode.

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
	for (int y=0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int Y1 = *src++;
			int U = *src++;
			int Y2 = *src++;
			int V = *src++;

			int r,g,b;
			yuv2rgb(Y1,U,V,r,g,b);
			*dst++ = 0xFF000000 | (r<<16) | (g<<8) | (b);
			yuv2rgb(Y2,U,V,r,g,b);
			*dst++ = 0xFF000000 | (r<<16) | (g<<8) | (b);
		}
	}
}

