/*
[configuration]

[OptionBool]
GUIName = Super-xBR 3-passes
OptionName = SXBR_3P
DefaultValue = true

[OptionRangeFloat]
GUIName = Edge Strength
OptionName = XBR_EDGE_STR
MinValue = 0.0
MaxValue = 5.0
StepAmount = 0.1
DefaultValue = 1.0
GUIDescription = Controls the strength of edge direction.

[OptionRangeFloat]
GUIName = Filter Weight
OptionName = XBR_WEIGHT
MinValue = 0.0
MaxValue = 1.5
StepAmount = 0.1
DefaultValue = 1.0
GUIDescription = Controls the filter sharpness.

[OptionRangeFloat]
GUIName = Super-xBR Anti-Ringing
OptionName = XBR_ANTI_RINGING
MinValue = 0.0
MaxValue = 1.0
StepAmount = 1.0
DefaultValue = 1.0
GUIDescription = Set the anti-ringing strength.

[OptionRangeFloat]
GUIName = Jinc2 Anti-Ringing
OptionName = JINC2_AR_STRENGTH
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 0.8
GUIDescription = Set the anti-ringing strength.

[OptionBool]
GUIName = Natural-Vision
OptionName = NATURAL_VISION
DefaultValue = false

[OptionRangeFloat]
GUIName = Natural-Vision - Color Boost
OptionName = COLOR_BOOST
MinValue = 0.8
MaxValue = 1.6
StepAmount = 0.05
DefaultValue = 1.2
DependantOption = NATURAL_VISION

[Pass]
Input0=ColorBuffer
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=1.0
EntryPoint=natural_vision
DependantOption = NATURAL_VISION

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=1.0
EntryPoint=Super_xBR_p0

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
Input1=Pass0
Input1Mode=Clamp
Input1Filter=Nearest
OutputScale=2.0
EntryPoint=Super_xBR_p1

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=2.0
EntryPoint=Super_xBR_p2
DependantOption = SXBR_3P

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=jinc2

[/configuration]
*/

/*
   
  *******  Super XBR Shader  *******
   
  Copyright (c) 2015-2016 Hyllian - sergiogdb@gmail.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

*/

#define weight1 (GetOption(XBR_WEIGHT)*1.29633/10.0)
#define weight2 (GetOption(XBR_WEIGHT)*1.75068/10.0/2.0)
#define weight3 (GetOption(XBR_WEIGHT)*1.75068/10.0)
#define weight4 (GetOption(XBR_WEIGHT)*1.29633/10.0/2.0)

struct weights {
    float wp1;
    float wp2;
    float wp3;
    float wp4;
    float wp5;
    float wp6;
};


float RGBtoYUV(float3 color)
{
  const float3 Y = float3(.2126, .7152, .0722);

  return dot(color, Y);
}

float df(float A, float B)
{
  return abs(A-B);
}


/*
                              P1
     |P0|B |C |P1|         C     F4          |a0|b1|c2|d3|
     |D |E |F |F4|      B     F     I4       |b0|c1|d2|e3|   |e1|i1|i2|e2|
     |G |H |I |I4|   P0    E  A  I     P3    |c0|d1|e2|f3|   |e3|i3|i4|e4|
     |P2|H5|I5|P3|      D     H     I5       |d0|e1|f2|g3|
                           G     H5
                              P2
*/


float d_wd(float b0, float b1, float c0, float c1, float c2, float d0, float d1, float d2, float d3, float e1, float e2, float e3, float f2, float f3, weights w)
{
	return (w.wp1*(df(c1,c2) + df(c1,c0) + df(e2,e1) + df(e2,e3)) + w.wp2*(df(d2,d3) + df(d0,d1)) + w.wp3*(df(d1,d3) + df(d0,d2)) + w.wp4*df(d1,d2) + w.wp5*(df(c0,c2) + df(e1,e3)) + w.wp6*(df(b0,b1) + df(f2,f3)));
}

float hv_wd(float i1, float i2, float i3, float i4, float e1, float e2, float e3, float e4, weights w)
{
	return ( w.wp4*(df(i1,i2)+df(i3,i4)) + w.wp1*(df(i1,e1)+df(i2,e2)+df(i3,e3)+df(i4,e4)) + w.wp3*(df(i1,e2)+df(i3,e4)+df(e1,i2)+df(e3,i4)));
}

float3 min4(float3 a, float3 b, float3 c, float3 d)
{
    return min(a, min(b, min(c, d)));
}
float3 max4(float3 a, float3 b, float3 c, float3 d)
{
    return max(a, max(b, max(c, d)));
}

     

     
     
void Super_xBR_p0()
{
	const weights w0 = {  2.0,  1.0, -1.0,  4.0,  -1.0,  1.0}; 
//	const weights w0 = {  1.0,  0.0,  0.0,  2.0,  -1.0,  0.0}; 

	float2 texcoord = GetCoordinates();
	float2 texture_size = GetResolution();

	float3 P0 = SampleInputOffset(0, int2(-1,-1)).xyz;
	float3  B = SampleInputOffset(0, int2( 0,-1)).xyz;
	float3  C = SampleInputOffset(0, int2( 1,-1)).xyz;
	float3 P1 = SampleInputOffset(0, int2( 2,-1)).xyz;

	float3  D = SampleInputOffset(0, int2(-1, 0)).xyz;
	float3  E = SampleInputOffset(0, int2( 0, 0)).xyz;
	float3  F = SampleInputOffset(0, int2( 1, 0)).xyz;
	float3 F4 = SampleInputOffset(0, int2( 2, 0)).xyz;

	float3  G = SampleInputOffset(0, int2(-1, 1)).xyz;
	float3  H = SampleInputOffset(0, int2( 0, 1)).xyz;
	float3  I = SampleInputOffset(0, int2( 1, 1)).xyz;
	float3 I4 = SampleInputOffset(0, int2( 2, 1)).xyz;

	float3 P2 = SampleInputOffset(0, int2(-1, 2)).xyz;
	float3 H5 = SampleInputOffset(0, int2( 0, 2)).xyz;
	float3 I5 = SampleInputOffset(0, int2( 1, 2)).xyz;
	float3 P3 = SampleInputOffset(0, int2( 2, 2)).xyz;

	float b = RGBtoYUV( B );
	float c = RGBtoYUV( C );
	float d = RGBtoYUV( D );
	float e = RGBtoYUV( E );
	float f = RGBtoYUV( F );
	float g = RGBtoYUV( G );
	float h = RGBtoYUV( H );
	float i = RGBtoYUV( I );

	float i4 = RGBtoYUV( I4 ); float p0 = RGBtoYUV( P0 );
	float i5 = RGBtoYUV( I5 ); float p1 = RGBtoYUV( P1 );
	float h5 = RGBtoYUV( H5 ); float p2 = RGBtoYUV( P2 );
	float f4 = RGBtoYUV( F4 ); float p3 = RGBtoYUV( P3 );


	/* Calc edgeness in diagonal directions. */
	float d_edge  = (d_wd( d, b, g, e, c, p2, h, f, p1, h5, i, f4, i5, i4, w0 ) - d_wd( c, f4, b, f, i4, p0, e, i, p3, d, h, i5, g, h5, w0 ));

	/* Calc edgeness in horizontal/vertical directions. */
	float hv_edge = (hv_wd(f, i, e, h, c, i5, b, h5, w0) - hv_wd(e, f, h, i, d, f4, g, i4, w0));

	float limits = GetOption(XBR_EDGE_STR) + 0.000001;
	float edge_strength = smoothstep(0.0, limits, abs(d_edge));

	/* Filter weights. Two taps only. */
	float4 wgt1 = float4(-weight1, weight1+0.5, weight1+0.5, -weight1);
	float4 wgt2 = float4(-weight2, weight2+0.25, weight2+0.25, -weight2);

	/* Filtering and normalization in four direction generating four colors. */
        float3 c1 = mul(wgt1, float4x3( P2,   H,   F,   P1 ));
        float3 c2 = mul(wgt1, float4x3( P0,   E,   I,   P3 ));
	float3 c3 = mul(wgt2, float4x3(D+G, E+H, F+I, F4+I4));
        float3 c4 = mul(wgt2, float4x3(C+B, F+E, I+H, I5+H5));

	/* Smoothly blends the two strongest directions (one in diagonal and the other in vert/horiz direction). */
	float3 color =  lerp(lerp(c1, c2, step(0.0, d_edge)), lerp(c3, c4, step(0.0, hv_edge)), 1 - edge_strength);

	/* Anti-ringing code. */
	float3 min_sample = min4( E, F, H, I ) + (1-GetOption(XBR_ANTI_RINGING))*lerp((P2-H)*(F-P1), (P0-E)*(I-P3), step(0.0, d_edge));
	float3 max_sample = max4( E, F, H, I ) - (1-GetOption(XBR_ANTI_RINGING))*lerp((P2-H)*(F-P1), (P0-E)*(I-P3), step(0.0, d_edge));
	color = clamp(color, min_sample, max_sample);

	SetOutput(float4(color, 1.0));
}


void Super_xBR_p1()
{
	const weights w1 = {  3.0,  0.0,  0.0,  0.0,   0.0,  0.0}; 
//	const weights w1 = {  1.0,  0.0,  0.0,  4.0,   0.0,  0.0}; 

	float2 texcoord = GetCoordinates();
	float2 texture_size = GetResolution();

	//Skip pixels on wrong grid
	float2 fp = frac(texcoord*texture_size);
	float2 dir = fp - float2(0.5,0.5);
 	
	if ((dir.x*dir.y)>0.0)
	{
		SetOutput((fp.x>0.5) ? SampleInput(0) : SampleInput(1));
	}
	else
	{

	float2 g1 = (fp.x>0.5) ? float2(0.5/texture_size.x, 0.0) : float2(0.0, 0.5/texture_size.y);
	float2 g2 = (fp.x>0.5) ? float2(0.0, 0.5/texture_size.y) : float2(0.5/texture_size.x, 0.0);

	float3 P0 = SampleInputLocation(1, texcoord -3.0*g1        ).xyz;
	float3 P1 = SampleInputLocation(0, texcoord         -3.0*g2).xyz;
	float3 P2 = SampleInputLocation(0, texcoord         +3.0*g2).xyz;
	float3 P3 = SampleInputLocation(1, texcoord +3.0*g1        ).xyz;

	float3  B = SampleInputLocation(0, texcoord -2.0*g1     -g2).xyz;
	float3  C = SampleInputLocation(1, texcoord     -g1 -2.0*g2).xyz;
	float3  D = SampleInputLocation(0, texcoord -2.0*g1     +g2).xyz;
	float3  E = SampleInputLocation(1, texcoord     -g1        ).xyz;
	float3  F = SampleInputLocation(0, texcoord             -g2).xyz;
	float3  G = SampleInputLocation(1, texcoord     -g1 +2.0*g2).xyz;
	float3  H = SampleInputLocation(0, texcoord             +g2).xyz;
	float3  I = SampleInputLocation(1, texcoord     +g1        ).xyz;

	float3 F4 = SampleInputLocation(1, texcoord     +g1 -2.0*g2).xyz;
	float3 I4 = SampleInputLocation(0, texcoord +2.0*g1     -g2).xyz;
	float3 H5 = SampleInputLocation(1, texcoord     +g1 +2.0*g2).xyz;
	float3 I5 = SampleInputLocation(0, texcoord +2.0*g1     +g2).xyz;

	float b = RGBtoYUV( B );
	float c = RGBtoYUV( C );
	float d = RGBtoYUV( D );
	float e = RGBtoYUV( E );
	float f = RGBtoYUV( F );
	float g = RGBtoYUV( G );
	float h = RGBtoYUV( H );
	float i = RGBtoYUV( I );

	float i4 = RGBtoYUV( I4 ); float p0 = RGBtoYUV( P0 );
	float i5 = RGBtoYUV( I5 ); float p1 = RGBtoYUV( P1 );
	float h5 = RGBtoYUV( H5 ); float p2 = RGBtoYUV( P2 );
	float f4 = RGBtoYUV( F4 ); float p3 = RGBtoYUV( P3 );


	/* Calc edgeness in diagonal directions. */
	float d_edge  = (d_wd( d, b, g, e, c, p2, h, f, p1, h5, i, f4, i5, i4, w1 ) - d_wd( c, f4, b, f, i4, p0, e, i, p3, d, h, i5, g, h5, w1 ));

	/* Calc edgeness in horizontal/vertical directions. */
	float hv_edge = (hv_wd(f, i, e, h, c, i5, b, h5, w1) - hv_wd(e, f, h, i, d, f4, g, i4, w1));

	float limits = GetOption(XBR_EDGE_STR) + 0.000001;
	float edge_strength = smoothstep(0.0, limits, abs(d_edge));

	/* Filter weights. Two taps only. */
	float4 wgt3 = float4(-weight3, weight3+0.5, weight3+0.5, -weight3);
	float4 wgt4 = float4(-weight4, weight4+0.25, weight4+0.25, -weight4);

	/* Filtering and normalization in four direction generating four colors. */
        float3 c1 = mul(wgt3, float4x3( P2,   H,   F,   P1 ));
        float3 c2 = mul(wgt3, float4x3( P0,   E,   I,   P3 ));
	float3 c3 = mul(wgt4, float4x3(D+G, E+H, F+I, F4+I4));
        float3 c4 = mul(wgt4, float4x3(C+B, F+E, I+H, I5+H5));

	/* Smoothly blends the two strongest directions (one in diagonal and the other in vert/horiz direction). */
	float3 color =  lerp(lerp(c1, c2, step(0.0, d_edge)), lerp(c3, c4, step(0.0, hv_edge)), 1 - edge_strength);

	/* Anti-ringing code. */
	float3 min_sample = min4( E, F, H, I ) + (1-GetOption(XBR_ANTI_RINGING))*lerp((P2-H)*(F-P1), (P0-E)*(I-P3), step(0.0, d_edge));
	float3 max_sample = max4( E, F, H, I ) - (1-GetOption(XBR_ANTI_RINGING))*lerp((P2-H)*(F-P1), (P0-E)*(I-P3), step(0.0, d_edge));
	color = clamp(color, min_sample, max_sample);

	SetOutput(float4(color, 1.0));	
	}
}


void Super_xBR_p2()
{
	const weights w2 = {  2.0,  1.0, -1.0,  4.0,  -1.0,  1.0}; 
//	const weights w2 = {  1.0,  0.0,  0.0,  0.0,  -1.0,  0.0}; 

	float3 P0 = SampleInputOffset(0, int2(-2,-2)).xyz;
	float3  B = SampleInputOffset(0, int2(-1,-2)).xyz;
	float3  C = SampleInputOffset(0, int2( 0,-2)).xyz;
	float3 P1 = SampleInputOffset(0, int2( 1,-2)).xyz;

	float3  D = SampleInputOffset(0, int2(-2,-1)).xyz;
	float3  E = SampleInputOffset(0, int2(-1,-1)).xyz;
	float3  F = SampleInputOffset(0, int2( 0,-1)).xyz;
	float3 F4 = SampleInputOffset(0, int2( 1,-1)).xyz;

	float3  G = SampleInputOffset(0, int2(-2, 0)).xyz;
	float3  H = SampleInputOffset(0, int2(-1, 0)).xyz;
	float3  I = SampleInputOffset(0, int2( 0, 0)).xyz;
	float3 I4 = SampleInputOffset(0, int2( 1, 0)).xyz;

	float3 P2 = SampleInputOffset(0, int2(-2, 1)).xyz;
	float3 H5 = SampleInputOffset(0, int2(-1, 1)).xyz;
	float3 I5 = SampleInputOffset(0, int2( 0, 1)).xyz;
	float3 P3 = SampleInputOffset(0, int2( 1, 1)).xyz;

	float b = RGBtoYUV( B );
	float c = RGBtoYUV( C );
	float d = RGBtoYUV( D );
	float e = RGBtoYUV( E );
	float f = RGBtoYUV( F );
	float g = RGBtoYUV( G );
	float h = RGBtoYUV( H );
	float i = RGBtoYUV( I );

	float i4 = RGBtoYUV( I4 ); float p0 = RGBtoYUV( P0 );
	float i5 = RGBtoYUV( I5 ); float p1 = RGBtoYUV( P1 );
	float h5 = RGBtoYUV( H5 ); float p2 = RGBtoYUV( P2 );
	float f4 = RGBtoYUV( F4 ); float p3 = RGBtoYUV( P3 );


	/* Calc edgeness in diagonal directions. */
	float d_edge  = (d_wd( d, b, g, e, c, p2, h, f, p1, h5, i, f4, i5, i4, w2 ) - d_wd( c, f4, b, f, i4, p0, e, i, p3, d, h, i5, g, h5, w2 ));

	/* Calc edgeness in horizontal/vertical directions. */
	float hv_edge = (hv_wd(f, i, e, h, c, i5, b, h5, w2) - hv_wd(e, f, h, i, d, f4, g, i4, w2));

	float limits = GetOption(XBR_EDGE_STR) + 0.000001;
	float edge_strength = smoothstep(0.0, limits, abs(d_edge));

	/* Filter weights. Two taps only. */
	float4 wgt1 = float4(-weight1, weight1+0.5, weight1+0.5, -weight1);
	float4 wgt2 = float4(-weight2, weight2+0.25, weight2+0.25, -weight2);

	/* Filtering and normalization in four direction generating four colors. */
        float3 c1 = mul(wgt1, float4x3( P2,   H,   F,   P1 ));
        float3 c2 = mul(wgt1, float4x3( P0,   E,   I,   P3 ));
	float3 c3 = mul(wgt2, float4x3(D+G, E+H, F+I, F4+I4));
        float3 c4 = mul(wgt2, float4x3(C+B, F+E, I+H, I5+H5));

	/* Smoothly blends the two strongest directions (one in diagonal and the other in vert/horiz direction). */
	float3 color =  lerp(lerp(c1, c2, step(0.0, d_edge)), lerp(c3, c4, step(0.0, hv_edge)), 1 - edge_strength);

	/* Anti-ringing code. */
	float3 min_sample = min4( E, F, H, I ) + (1-GetOption(XBR_ANTI_RINGING))*lerp((P2-H)*(F-P1), (P0-E)*(I-P3), step(0.0, d_edge));
	float3 max_sample = max4( E, F, H, I ) - (1-GetOption(XBR_ANTI_RINGING))*lerp((P2-H)*(F-P1), (P0-E)*(I-P3), step(0.0, d_edge));
	color = clamp(color, min_sample, max_sample);

	SetOutput(float4(color, 1.0));	
}


/*
   Hyllian's jinc windowed-jinc 2-lobe with anti-ringing Shader
   
   Copyright (C) 2011-2016 Hyllian/Jararaca - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/

      /*
         This is an approximation of Jinc(x)*Jinc(x*r1/r2) for x < 2.5,
         where r1 and r2 are the first two zeros of jinc function.
         For a jinc 2-lobe best approximation, use A=0.5 and B=0.825.
      */  

// A=0.5, B=0.825 is the best jinc approximation for x<2.5. if B=1.0, it's a lanczos filter.
// Increase A to get more blur. Decrease it to get a sharper picture. 
// B = 0.825 to get rid of dithering. Increase B to get a fine sharpness, though dithering returns.

#define JINC2_WINDOW_SINC 0.42
#define JINC2_SINC 0.92
//#define JINC2_AR_STRENGTH 0.8


#define halfpi  1.5707963267948966192313216916398
#define pi    3.1415926535897932384626433832795
#define wa    (JINC2_WINDOW_SINC*pi)
#define wb    (JINC2_SINC*pi)

// Calculates the distance between two points
float d(float2 pt1, float2 pt2)
{
  float2 v = pt2 - pt1;
  return sqrt(dot(v,v));
}

    float4 resampler(float4 x)
    {
      float4 res;

      res = (x==float4(0.0, 0.0, 0.0, 0.0)) ?  float4(wa*wb, wa*wb, wa*wb, wa*wb)  :  sin(x*wa)*sin(x*wb)/(x*x);

      return res;
    }

void jinc2()
{

      float3 color;
      float4x4 weights;

      float2 dx = float2(1.0, 0.0);
      float2 dy = float2(0.0, 1.0);

      float2 texcoord = GetCoordinates();
      float2 texture_size = GetResolution();

      float2 pc = texcoord*texture_size;

      float2 tc = (floor(pc-float2(0.5,0.5))+float2(0.5,0.5));
     
      weights[0] = resampler(float4(d(pc, tc    -dx    -dy), d(pc, tc           -dy), d(pc, tc    +dx    -dy), d(pc, tc+2.0*dx    -dy)));
      weights[1] = resampler(float4(d(pc, tc    -dx       ), d(pc, tc              ), d(pc, tc    +dx       ), d(pc, tc+2.0*dx       )));
      weights[2] = resampler(float4(d(pc, tc    -dx    +dy), d(pc, tc           +dy), d(pc, tc    +dx    +dy), d(pc, tc+2.0*dx    +dy)));
      weights[3] = resampler(float4(d(pc, tc    -dx+2.0*dy), d(pc, tc       +2.0*dy), d(pc, tc    +dx+2.0*dy), d(pc, tc+2.0*dx+2.0*dy)));

      dx = dx/texture_size;
      dy = dy/texture_size;
      tc = tc/texture_size;
     
     // reading the texels
     
      float3 c00 = SampleLocation(tc    -dx    -dy).rgb;
      float3 c10 = SampleLocation(tc           -dy).rgb;
      float3 c20 = SampleLocation(tc    +dx    -dy).rgb;
      float3 c30 = SampleLocation(tc+2.0*dx    -dy).rgb;
      float3 c01 = SampleLocation(tc    -dx       ).rgb;
      float3 c11 = SampleLocation(tc              ).rgb;
      float3 c21 = SampleLocation(tc    +dx       ).rgb;
      float3 c31 = SampleLocation(tc+2.0*dx       ).rgb;
      float3 c02 = SampleLocation(tc    -dx    +dy).rgb;
      float3 c12 = SampleLocation(tc           +dy).rgb;
      float3 c22 = SampleLocation(tc    +dx    +dy).rgb;
      float3 c32 = SampleLocation(tc+2.0*dx    +dy).rgb;
      float3 c03 = SampleLocation(tc    -dx+2.0*dy).rgb;
      float3 c13 = SampleLocation(tc       +2.0*dy).rgb;
      float3 c23 = SampleLocation(tc    +dx+2.0*dy).rgb;
      float3 c33 = SampleLocation(tc+2.0*dx+2.0*dy).rgb;

      //  Get min/max samples
      float3 min_sample = min4(c11, c21, c12, c22);
      float3 max_sample = max4(c11, c21, c12, c22);

      color = mul(weights[0], float4x3(c00, c10, c20, c30));
      color+= mul(weights[1], float4x3(c01, c11, c21, c31));
      color+= mul(weights[2], float4x3(c02, c12, c22, c32));
      color+= mul(weights[3], float4x3(c03, c13, c23, c33));
      color = color/(dot(mul(weights, float4(1.0, 1.0, 1.0, 1.0)), float4(1.0, 1.0, 1.0, 1.0)));

      // Anti-ringing
      float3 aux = color;
      color = clamp(color, min_sample, max_sample);

      color = lerp(aux, color, GetOption(JINC2_AR_STRENGTH));
 
      // final sum and weight normalization
      SetOutput(float4(color, 1.0));
}


/*
   ShadX's Natural Vision Shader

   Ported and tweaked by Hyllian - 2016

*/

void natural_vision()
{
	const float3x3 RGBtoYIQ = float3x3(0.299, 0.596, 0.212, 
                                           0.587,-0.275,-0.523, 
                                           0.114,-0.321, 0.311);

	const float3x3 YIQtoRGB = float3x3(1.0, 1.0, 1.0,
                                           0.95568806036115671171,-0.27158179694405859326,-1.1081773266826619523,
                                           0.61985809445637075388,-0.64687381613840131330, 1.7050645599191817149);

	float cb = GetOption(COLOR_BOOST);

	float3 val00 = float3( cb, cb, cb);

	float3 c0,c1;

	c0 = Sample().xyz;
	c1 = mul(c0,RGBtoYIQ);
	c1 = float3(pow(c1.x,val00.x),c1.yz*val00.yz);

	SetOutput(float4(mul(c1,YIQtoRGB), 1.0));
}

