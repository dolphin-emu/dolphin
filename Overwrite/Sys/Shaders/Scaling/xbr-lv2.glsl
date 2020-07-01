/*
[configuration]

[OptionRangeFloat]
GUIName = Blend Sharpness
OptionName = XBR_SCALE
MinValue = 1.0
MaxValue = 5.0
StepAmount = 0.1
DefaultValue = 2.0

[OptionRangeFloat]
GUIName = Jinc2 Anti-Ringing
OptionName = JINC2_AR_STRENGTH
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 0.8

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
OutputScale=2.0
EntryPoint=main


[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=jinc2


[/configuration]
*/

/*

   Hyllian's xBR-lv2 Shader
   
   Copyright (C) 2011-2015 Hyllian - sergiogdb@gmail.com

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

   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
*/


// Uncomment just one of the three params below to choose the corner detection
#define CORNER_A
//#define CORNER_B
//#define CORNER_C
//#define CORNER_D

#define XBR_Y_WEIGHT 48.0
#define XBR_EQ_THRESHOLD 25.0
#define LV2_CF 2.0

#define blendness  GetOption(XBR_SCALE)



// Distance between vector components.
float4 ds(float4 A, float4 B)
{
    return float4(abs(A-B));
}

// Compare two vectors and return if their components are different.
float4 df(float4 A, float4 B)
{
    return float4(A!=B);
}

// Determine if two vector components are equal based on a threshold.
float4 eq(float4 A, float4 B)
{
    return (step(ds(A, B), float4(XBR_EQ_THRESHOLD, XBR_EQ_THRESHOLD, XBR_EQ_THRESHOLD, XBR_EQ_THRESHOLD)));
}

// Determine if two vector components are NOT equal based on a threshold.
float4 neq(float4 A, float4 B)
{
    return (float4(1.0, 1.0, 1.0, 1.0) - eq(A, B));
}

// Weighted distance.
float4 wd(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h)
{
    return (ds(a,b) + ds(a,c) + ds(d,e) + ds(d,f) + 4.0*ds(g,h));
}

// Color distance.
float c_ds(float3 c1, float3 c2) 
{
      float3 cds = abs(c1 - c2);
      return cds.r + cds.g + cds.b;
}


void main()
{
const  float4 Ao = float4( 1.0, -1.0, -1.0, 1.0 );
const  float4 Bo = float4( 1.0,  1.0, -1.0,-1.0 );
const  float4 Co = float4( 1.5,  0.5, -0.5, 0.5 );
const  float4 Ax = float4( 1.0, -1.0, -1.0, 1.0 );
const  float4 Bx = float4( 0.5,  2.0, -0.5,-2.0 );
const  float4 Cx = float4( 1.0,  1.0, -0.5, 0.0 );
const  float4 Ay = float4( 1.0, -1.0, -1.0, 1.0 );
const  float4 By = float4( 2.0,  0.5, -2.0,-0.5 );
const  float4 Cy = float4( 2.0,  0.0, -1.0, 0.5 );
const  float4 Ci = float4(0.25, 0.25, 0.25, 0.25);

const  float3 Y = float3(0.2126, 0.7152, 0.0722);

	float4 edri, edr, edr_l, edr_u, px; // px = pixel, edr = edge detection rule
	float4 irlv0, irlv1, irlv2_l, irlv2_u;
	float4 fx, fx_l, fx_u; // inequations of straight lines.

	float4 delta         = float4(1.0/blendness, 1.0/blendness, 1.0/blendness, 1.0/blendness);
	float4 deltaL        = float4(0.5/blendness, 1.0/blendness, 0.5/blendness, 1.0/blendness);
	float4 deltaU        = deltaL.yxwz;

	float2 texcoord = GetCoordinates();
	float2 texture_size = GetResolution();

	float2 fp = frac(texcoord*texture_size);

	float3 A1 = SampleOffset(int2(-1,-2)).rgb;
	float3 B1 = SampleOffset(int2( 0,-2)).rgb;
	float3 C1 = SampleOffset(int2( 1,-2)).rgb;

	float3 A  = SampleOffset(int2(-1,-1)).rgb;
	float3 B  = SampleOffset(int2( 0,-1)).rgb;
	float3 C  = SampleOffset(int2( 1,-1)).rgb;

	float3 D  = SampleOffset(int2(-1, 0)).rgb;
	float3 E  = SampleOffset(int2( 0, 0)).rgb;
	float3 F  = SampleOffset(int2( 1, 0)).rgb;

	float3 G  = SampleOffset(int2(-1, 1)).rgb;
	float3 H  = SampleOffset(int2( 0, 1)).rgb;
	float3 I  = SampleOffset(int2( 1, 1)).rgb;

	float3 G5 = SampleOffset(int2(-1, 2)).rgb;
	float3 H5 = SampleOffset(int2( 0, 2)).rgb;
	float3 I5 = SampleOffset(int2( 1, 2)).rgb;

	float3 A0 = SampleOffset(int2(-2,-1)).rgb;
	float3 D0 = SampleOffset(int2(-2, 0)).rgb;
	float3 G0 = SampleOffset(int2(-2, 1)).rgb;

	float3 C4 = SampleOffset(int2( 2,-1)).rgb;
	float3 F4 = SampleOffset(int2( 2, 0)).rgb;
	float3 I4 = SampleOffset(int2( 2, 1)).rgb;

	float4 b = mul( float4x3(B, D, H, F), XBR_Y_WEIGHT*Y );
	float4 c = mul( float4x3(C, A, G, I), XBR_Y_WEIGHT*Y );
	float4 e = mul( float4x3(E, E, E, E), XBR_Y_WEIGHT*Y );
	float4 d = b.yzwx;
	float4 f = b.wxyz;
	float4 g = c.zwxy;
	float4 h = b.zwxy;
	float4 i = c.wxyz;

	float4 i4 = mul( float4x3(I4, C1, A0, G5), XBR_Y_WEIGHT*Y );
	float4 i5 = mul( float4x3(I5, C4, A1, G0), XBR_Y_WEIGHT*Y );
	float4 h5 = mul( float4x3(H5, F4, B1, D0), XBR_Y_WEIGHT*Y );
	float4 f4 = h5.yzwx;

	// These inequations define the line below which interpolation occurs.
	fx   = (Ao*fp.y+Bo*fp.x); 
	fx_l = (Ax*fp.y+Bx*fp.x);
	fx_u = (Ay*fp.y+By*fp.x);

        irlv1 = irlv0 = df(e,f)*df(e,h);

#ifdef CORNER_B
	irlv1      = (irlv0  *  ( neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) );
#endif
#ifdef CORNER_D
	float4 c1 = i4.yzwx;
	float4 g0 = i5.wxyz;
	irlv1      = (irlv0  *  ( neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) * (df(f,f4) * df(f,i) + df(h,h5) * df(h,i) + df(h,g) + df(f,c) + eq(b,c1) * eq(d,g0)));
#endif
#ifdef CORNER_C
	irlv1      = (irlv0  * ( neq(f,b) * neq(f,c) + neq(h,d) * neq(h,g) + eq(e,i) * (neq(f,f4) * neq(f,i4) + neq(h,h5) * neq(h,i5)) + eq(e,g) + eq(e,c)) );
#endif

	irlv2_l = df(e,g) * df(d,g);
	irlv2_u = df(e,c) * df(b,c);

	float4 fx45i = clamp((fx   + delta  -Co - Ci)/(2*delta ), 0.0, 1.0);
	float4 fx45  = clamp((fx   + delta  -Co     )/(2*delta ), 0.0, 1.0);
	float4 fx30  = clamp((fx_l + deltaL -Cx     )/(2*deltaL), 0.0, 1.0);
	float4 fx60  = clamp((fx_u + deltaU -Cy     )/(2*deltaU), 0.0, 1.0);

	float4 wd1 = wd( e, c, g, i, h5, f4, h, f);
	float4 wd2 = wd( h, d, i5, f, i4, b, e, i);

	edri  = step(wd1, wd2) * irlv0;
	edr   = step(wd1 + float4(0.1, 0.1, 0.1, 0.1), wd2) * step(float4(0.5, 0.5, 0.5, 0.5), irlv1);

#ifdef CORNER_A
	edr   = edr * (float4(1.0, 1.0, 1.0, 1.0) - edri.yzwx * edri.wxyz);
	edr_l = step( LV2_CF*ds(f,g), ds(h,c) ) * irlv2_l * edr * (float4(1.0, 1.0, 1.0, 1.0) - edri.yzwx) * eq(e,c);
	edr_u = step( LV2_CF*ds(h,c), ds(f,g) ) * irlv2_u * edr * (float4(1.0, 1.0, 1.0, 1.0) - edri.wxyz) * eq(e,g);
#endif
#ifndef CORNER_A
	edr_l = step( LV2_CF*ds(f,g), ds(h,c) ) * irlv2_l * edr;
	edr_u = step( LV2_CF*ds(h,c), ds(f,g) ) * irlv2_u * edr;
#endif



	fx45  = edr*fx45;
	fx30  = edr_l*fx30;
	fx60  = edr_u*fx60;
	fx45i = edri*fx45i;

	px = step(ds(e,f), ds(e,h));

	float4 maximos = max(max(fx30, fx60), max(fx45, fx45i));

	float3 res1 = E;
	res1 = lerp(res1, lerp(H, F, px.x), maximos.x);
	res1 = lerp(res1, lerp(B, D, px.z), maximos.z);
	
	float3 res2 = E;
	res2 = lerp(res2, lerp(F, B, px.y), maximos.y);
	res2 = lerp(res2, lerp(D, H, px.w), maximos.w);
	
	float3 res = lerp(res1, res2, step(c_ds(E, res1), c_ds(E, res2)));

	SetOutput(float4(res, 1.0));
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

float3 min4(float3 a, float3 b, float3 c, float3 d)
{
    return min(a, min(b, min(c, d)));
}
float3 max4(float3 a, float3 b, float3 c, float3 d)
{
    return max(a, max(b, max(c, d)));
}

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

