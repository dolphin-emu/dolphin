/*
[configuration]

[OptionRangeFloat]
GUIName = Anti-Ringing
OptionName = JINC2_AR_STRENGTH
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 0.8

[Pass]
Input0=ColorBuffer
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=1.0
EntryPoint=tag4x

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=1.0
EntryPoint=jinc_2x

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale1=1.0
EntryPoint=tag2x

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale0=1.0
EntryPoint=jinc_4x

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=jinc


[/configuration]
*/


float reduce(float3 A)
{
  const float3 dtt = float3(65536,255,1);

  return dot(A, dtt);
}


bool lq(float A, float B, float C, float D)
{
  return (A==B && B==C && C==D);
}

bool lq3(float A, float B, float C)
{
  return (A==B && B==C);
}

void downscaler()
{
	SetOutput(Sample());
}

void tag4x()
{
      float2 texcoord = GetCoordinates();
      float2 texture_size = GetResolution();
      float2 dx = float2(2.0/texture_size.x, 0.0);
      float2 dy = float2(0.0, 2.0/texture_size.y);

	//   A  B  C 
	//   D  E  F 
	//   G  H  I 

	float3 A  = SampleLocation(texcoord    -dx    -dy).rgb;
	float3 B  = SampleLocation(texcoord           -dy).rgb;
	float3 C  = SampleLocation(texcoord    +dx    -dy).rgb;

	float3 D  = SampleLocation(texcoord    -dx       ).rgb;
	float3 E  = SampleLocation(texcoord              ).rgb;
	float3 F  = SampleLocation(texcoord    +dx       ).rgb;

	float3 G  = SampleLocation(texcoord    -dx    +dy).rgb;
	float3 H  = SampleLocation(texcoord           +dy).rgb;
	float3 I  = SampleLocation(texcoord    +dx    +dy).rgb;

	float a  = reduce(A ); float b  = reduce(B ); float c  = reduce(C );
	float d  = reduce(D ); float e  = reduce(E ); float f  = reduce(F );
	float g  = reduce(G ); float h  = reduce(H ); float i  = reduce(I );

	bool lowres = ( (lq(e,b,a,d)) || (lq(e,d,g,h)) || (lq(e,h,i,f)) || (lq(e,f,c,b)) );

	float alpha = lowres ? 1.0 : 0.0;

	SetOutput(float4(E, alpha));
}



void tag2x()
{
      float2 texcoord = GetCoordinates();
      float2 texture_size = GetResolution();
      float2 dx = float2(1.0/texture_size.x, 0.0);
      float2 dy = float2(0.0, 1.0/texture_size.y);

	//   A  B  C 
	//   D  E  F 
	//   G  H  I 

	float3 A  = SampleLocation(texcoord    -dx    -dy).rgb;
	float3 B  = SampleLocation(texcoord           -dy).rgb;
	float3 C  = SampleLocation(texcoord    +dx    -dy).rgb;

	float3 D  = SampleLocation(texcoord    -dx       ).rgb;
	float3 E  = SampleLocation(texcoord              ).rgb;
	float3 F  = SampleLocation(texcoord    +dx       ).rgb;

	float3 G  = SampleLocation(texcoord    -dx    +dy).rgb;
	float3 H  = SampleLocation(texcoord           +dy).rgb;
	float3 I  = SampleLocation(texcoord    +dx    +dy).rgb;

	float a  = reduce(A ); float b  = reduce(B ); float c  = reduce(C );
	float d  = reduce(D ); float e  = reduce(E ); float f  = reduce(F );
	float g  = reduce(G ); float h  = reduce(H ); float i  = reduce(I );

	bool lowres = ( (lq(e,b,a,d)) || (lq(e,d,g,h)) || (lq(e,h,i,f)) || (lq(e,f,c,b)) );

	float alpha = lowres ? 1.0 : 0.0;

	SetOutput(float4(E, alpha));
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

float3 min4(float3 a, float3 b, float3 c, float3 d)
{
    return min(a, min(b, min(c, d)));
}
float3 max4(float3 a, float3 b, float3 c, float3 d)
{
    return max(a, max(b, max(c, d)));
}

    float4 resampler(float4 x)
    {
      float4 res;

      res = (x==float4(0.0, 0.0, 0.0, 0.0)) ?  float4(wa*wb, wa*wb, wa*wb, wa*wb)  :  sin(x*wa)*sin(x*wb)/(x*x);

      return res;
    }

void jinc_2x()
{

      float3 color;
      float4x4 weights;

      float2 dx = float2(1.0, 0.0);
      float2 dy = float2(0.0, 1.0);

      float2 texcoord = GetCoordinates();
      float2 texture_size = GetResolution();

      float  tag_res = 4.0;

      float2 pc = texcoord*texture_size/tag_res;

      float2 tc = (floor(pc-float2(0.5,0.5))+float2(0.5,0.5));
     
      weights[0] = resampler(float4(d(pc, tc    -dx    -dy), d(pc, tc           -dy), d(pc, tc    +dx    -dy), d(pc, tc+2.0*dx    -dy)));
      weights[1] = resampler(float4(d(pc, tc    -dx       ), d(pc, tc              ), d(pc, tc    +dx       ), d(pc, tc+2.0*dx       )));
      weights[2] = resampler(float4(d(pc, tc    -dx    +dy), d(pc, tc           +dy), d(pc, tc    +dx    +dy), d(pc, tc+2.0*dx    +dy)));
      weights[3] = resampler(float4(d(pc, tc    -dx+2.0*dy), d(pc, tc       +2.0*dy), d(pc, tc    +dx+2.0*dy), d(pc, tc+2.0*dx+2.0*dy)));

      dx = dx*tag_res/texture_size;
      dy = dy*tag_res/texture_size;
      tc = tc*tag_res/texture_size;

      float2 ps = 1.0/texture_size;
      float2 g1 = float2(ps.x, 0.0);
      float2 g2 = float2(0.0, ps.y);
     
     // reading the texels
     
      float4 c00 = SampleLocation(tc    -dx    -dy);
      float4 c10 = SampleLocation(tc           -dy);
      float4 c20 = SampleLocation(tc    +dx    -dy);
      float4 c30 = SampleLocation(tc+2.0*dx    -dy);
      float4 c01 = SampleLocation(tc    -dx       );
      float4 c11 = SampleLocation(tc              );
      float4 c21 = SampleLocation(tc    +dx       );
      float4 c31 = SampleLocation(tc+2.0*dx       );
      float4 c02 = SampleLocation(tc    -dx    +dy);
      float4 c12 = SampleLocation(tc           +dy);
      float4 c22 = SampleLocation(tc    +dx    +dy);
      float4 c32 = SampleLocation(tc+2.0*dx    +dy);
      float4 c03 = SampleLocation(tc    -dx+2.0*dy);
      float4 c13 = SampleLocation(tc       +2.0*dy);
      float4 c23 = SampleLocation(tc    +dx+2.0*dy);
      float4 c33 = SampleLocation(tc+2.0*dx+2.0*dy);

      float4 E = SampleLocation(texcoord);
      float alpha = E.a;

      c00 = E.a==c00.a ? c00 : E;
      c10 = E.a==c10.a ? c10 : E;
      c20 = E.a==c20.a ? c20 : E;
      c30 = E.a==c30.a ? c30 : E;
      c01 = E.a==c01.a ? c01 : E;
      c11 = E.a==c11.a ? c11 : E;
      c21 = E.a==c21.a ? c21 : E;
      c31 = E.a==c31.a ? c31 : E;
      c02 = E.a==c02.a ? c02 : E;
      c12 = E.a==c12.a ? c12 : E;
      c22 = E.a==c22.a ? c22 : E;
      c32 = E.a==c32.a ? c32 : E;
      c03 = E.a==c03.a ? c03 : E;
      c13 = E.a==c13.a ? c13 : E;
      c23 = E.a==c23.a ? c23 : E;
      c33 = E.a==c33.a ? c33 : E;

      //  Get min/max samples
      float3 min_sample = min4(c11.rgb, c21.rgb, c12.rgb, c22.rgb);
      float3 max_sample = max4(c11.rgb, c21.rgb, c12.rgb, c22.rgb);


      color = mul(weights[0], float4x3(c00.rgb, c10.rgb, c20.rgb, c30.rgb));
      color+= mul(weights[1], float4x3(c01.rgb, c11.rgb, c21.rgb, c31.rgb));
      color+= mul(weights[2], float4x3(c02.rgb, c12.rgb, c22.rgb, c32.rgb));
      color+= mul(weights[3], float4x3(c03.rgb, c13.rgb, c23.rgb, c33.rgb));
      color = color/(dot(mul(weights, float4(1.0, 1.0, 1.0, 1.0)), float4(1.0, 1.0, 1.0, 1.0)));

      // Anti-ringing
      float3 aux = color;
      color = clamp(color, min_sample, max_sample);

      color = lerp(aux, color, GetOption(JINC2_AR_STRENGTH));
 
      color = lerp(E.xyz, color, alpha);

      // final sum and weight normalization
      SetOutput(float4(color, 1.0));
}

void jinc_4x()
{

      float3 color;
      float4x4 weights;

      float2 dx = float2(1.0, 0.0);
      float2 dy = float2(0.0, 1.0);

      float2 texcoord = GetCoordinates();
      float2 texture_size = GetResolution();

      float  tag_res = 2.0;

      float2 pc = texcoord*texture_size/tag_res;

      float2 tc = (floor(pc-float2(0.5,0.5))+float2(0.5,0.5));
     
      weights[0] = resampler(float4(d(pc, tc    -dx    -dy), d(pc, tc           -dy), d(pc, tc    +dx    -dy), d(pc, tc+2.0*dx    -dy)));
      weights[1] = resampler(float4(d(pc, tc    -dx       ), d(pc, tc              ), d(pc, tc    +dx       ), d(pc, tc+2.0*dx       )));
      weights[2] = resampler(float4(d(pc, tc    -dx    +dy), d(pc, tc           +dy), d(pc, tc    +dx    +dy), d(pc, tc+2.0*dx    +dy)));
      weights[3] = resampler(float4(d(pc, tc    -dx+2.0*dy), d(pc, tc       +2.0*dy), d(pc, tc    +dx+2.0*dy), d(pc, tc+2.0*dx+2.0*dy)));

      dx = dx*tag_res/texture_size;
      dy = dy*tag_res/texture_size;
      tc = tc*tag_res/texture_size;

      float2 ps = 1.0/texture_size;
      float2 g1 = float2(ps.x, 0.0);
      float2 g2 = float2(0.0, ps.y);
     
     // reading the texels
     
      float4 c00 = SampleLocation(tc    -dx    -dy);
      float4 c10 = SampleLocation(tc           -dy);
      float4 c20 = SampleLocation(tc    +dx    -dy);
      float4 c30 = SampleLocation(tc+2.0*dx    -dy);
      float4 c01 = SampleLocation(tc    -dx       );
      float4 c11 = SampleLocation(tc              );
      float4 c21 = SampleLocation(tc    +dx       );
      float4 c31 = SampleLocation(tc+2.0*dx       );
      float4 c02 = SampleLocation(tc    -dx    +dy);
      float4 c12 = SampleLocation(tc           +dy);
      float4 c22 = SampleLocation(tc    +dx    +dy);
      float4 c32 = SampleLocation(tc+2.0*dx    +dy);
      float4 c03 = SampleLocation(tc    -dx+2.0*dy);
      float4 c13 = SampleLocation(tc       +2.0*dy);
      float4 c23 = SampleLocation(tc    +dx+2.0*dy);
      float4 c33 = SampleLocation(tc+2.0*dx+2.0*dy);

      float4 E = SampleLocation(texcoord);
      float alpha = E.a;

      c00 = E.a==c00.a ? c00 : E;
      c10 = E.a==c10.a ? c10 : E;
      c20 = E.a==c20.a ? c20 : E;
      c30 = E.a==c30.a ? c30 : E;
      c01 = E.a==c01.a ? c01 : E;
      c11 = E.a==c11.a ? c11 : E;
      c21 = E.a==c21.a ? c21 : E;
      c31 = E.a==c31.a ? c31 : E;
      c02 = E.a==c02.a ? c02 : E;
      c12 = E.a==c12.a ? c12 : E;
      c22 = E.a==c22.a ? c22 : E;
      c32 = E.a==c32.a ? c32 : E;
      c03 = E.a==c03.a ? c03 : E;
      c13 = E.a==c13.a ? c13 : E;
      c23 = E.a==c23.a ? c23 : E;
      c33 = E.a==c33.a ? c33 : E;

      //  Get min/max samples
      float3 min_sample = min4(c11.rgb, c21.rgb, c12.rgb, c22.rgb);
      float3 max_sample = max4(c11.rgb, c21.rgb, c12.rgb, c22.rgb);

      color = mul(weights[0], float4x3(c00.rgb, c10.rgb, c20.rgb, c30.rgb));
      color+= mul(weights[1], float4x3(c01.rgb, c11.rgb, c21.rgb, c31.rgb));
      color+= mul(weights[2], float4x3(c02.rgb, c12.rgb, c22.rgb, c32.rgb));
      color+= mul(weights[3], float4x3(c03.rgb, c13.rgb, c23.rgb, c33.rgb));
      color = color/(dot(mul(weights, float4(1.0, 1.0, 1.0, 1.0)), float4(1.0, 1.0, 1.0, 1.0)));

      // Anti-ringing
      float3 aux = color;
      color = clamp(color, min_sample, max_sample);

      color = lerp(aux, color, GetOption(JINC2_AR_STRENGTH));
 
      color = lerp(E.xyz, color, alpha);

      // final sum and weight normalization
      SetOutput(float4(color, 1.0));
}


void jinc()
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
