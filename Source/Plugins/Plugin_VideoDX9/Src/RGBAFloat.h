#ifndef _RGBAFLOAT_H
#define _RGBAFLOAT_H

struct RGBAFloat
{
	float r,g,b,a;
	RGBAFloat(){a=r=g=b=1.0f;}
	RGBAFloat(float _r, float _g, float _b, float _a)
	{
		r=_r;g=_g;b=_b;a=_a;
	}
	void clamp()
	{
		if (r>1) r=1;
		if (g>1) g=1;
		if (b>1) b=1;
		if (a>1) a=1;
		if (r<0) r=0;
		if (g<0) g=0;
		if (b<0) b=0;
		if (a<0) a=0;
	}
	void convertToD3DColor(u32 &color)
	{
		clamp();
		color = (int(a*255)<<24) | (int(r*255)<<16) | (int(g*255)<<8) | (int(b*255));
	}
	void convertRGB_GC(u32 color)
	{
		r=((color>>24)&0xFF)/255.0f; 
		g=((color>>16)&0xFF)/255.0f; 
		b=((color>>8)&0xFF)/255.0f;
	}
	void convertRGB(u32 color)
	{
		r=((color>>16)&0xFF)/255.0f; 
		g=((color>>8)&0xFF)/255.0f; 
		b=((color)&0xFF)/255.0f;
	}
	void convertA(u32 color)
	{
		a=((color>>24)&0xFF)/255.0f;
	}
	void convertA_GC(u32 color)
	{
		a=((color)&0xFF)/255.0f;
	}
	void convert(u32 color)
	{
		convertRGB(color);
		convertA(color);
	}
	void convert_GC(u32 color)
	{
		convertRGB_GC(color);
		convertA_GC(color);
	}
	void operator *=(float f)
	{
		r*=f;g*=f;b*=f;	a*=f;
	}
	RGBAFloat operator *(float f)
	{
		return RGBAFloat(r*f,g*f,b*f,a*f);
	}
	RGBAFloat operator *(RGBAFloat &o)
	{
		return RGBAFloat(r*o.r,g*o.g,b*o.b,a*o.a);
	}
	void operator *=(RGBAFloat &o)
	{
		r*=o.r;g*=o.g;b*=o.b;a*=o.a;
	}
	RGBAFloat operator +(RGBAFloat &o)
	{
		return RGBAFloat(r+o.r,g+o.g,b+o.b,a+o.a);
	}
	void operator +=(RGBAFloat &o)
	{
		r+=o.r;g+=o.g;b+=o.b;a+=o.a;
	}
};

#endif