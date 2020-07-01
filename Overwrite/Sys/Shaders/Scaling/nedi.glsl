/*
[configuration]

[OptionBool]
GUIName = Nedi 3-passes
OptionName = NEDI_3P
DefaultValue = true

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
OutputScale=2.0
EntryPoint=nedi_p0

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=2.0
EntryPoint=nedi_p1

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
OutputScale=2.0
EntryPoint=nedi_p2
DependantOption = NEDI_3P

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=jinc2

[/configuration]
*/

// Nedi Shader developed by Shiandow.
// Ported to Ishiiruka by Hyllian.

// This file is a part of MPDN Extensions.
// https://github.com/zachsaw/MPDN_Extensions
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3.0 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library.
// 

#define NEDI_WEIGHT 4.0
#define NEDI_N 24.0
#define NEDI_E 0.0
#define ITERATIONS  3
#define WGT   2

#define width  (texture_size.x)
#define height (texture_size.y)

#define px (1.0 / (2*texture_size.x))
#define py (1.0 / (2*texture_size.y))

//#define offset 0.5
#define offset 0.0
#define Value(xy) (SampleInputLocation(0,tex+float2(px,py)*(xy)))//-float4(0,0.5,0.5,0))
//#define Get(xy) (Value(xy)[1]+offset)
#define Get(xy) (dot(Value(xy).rgb,float3(0.299,0.587,0.114))+offset)
#define Get4(xy) (float2(Get(xy+2*dir[0])+Get(xy+2*dir[1]),Get(xy+2*dir[2])+Get(xy+2*dir[3])))

#define sqr(x) (dot(x,x))
#define I (float2x2(1,0,0,1))


//Cramer's method
float2 solve(float2x2 A,float2 b) { return float2(determinant(float2x2(b,A[1])),determinant(float2x2(A[0],b)))/determinant(A); }

    
void nedi_p0()
{
	float2 texture_size = GetResolution();
	float2 tex = GetCoordinates();

	float4 c0 = Sample();

	//Skip pixels on wrong grid
	if ((frac(tex.x*width)<0.5)||(frac(tex.y*height)<0.5)) SetOutput(c0);
	else
	{
	
	//Define window and directions
	float2 dir[4] = {{-1,-1},{1,1},{-1,1},{1,-1}};
	float4x2 wind[4] = {{{-1,-1},{-1,1},{1,-1},{1,1}},{{-3,-1},{-1,3},{1,-3},{3,1}},{{-1,-3},{-3,1},{3,-1},{1,3}},{{-3,-3},{-3,3},{3,-3},{3,3}}};

	//Initialization
	float2x2 R = 0;
	float2 r = 0;

        float m[7] = {NEDI_WEIGHT, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

	//Calculate (local) autocorrelation coefficients
	for (int k = 0; k<ITERATIONS; k+= 1){
		float4 y = float4(Get(wind[k][0]),Get(wind[k][1]),Get(wind[k][2]),Get(wind[k][3]));
		float4x2 C = float4x2(Get4(wind[k][0]),Get4(wind[k][1]),Get4(wind[k][2]),Get4(wind[k][3]));
		R += mul(transpose(C),m[k]*C);
		r += mul(y,m[k]*C);
	}
	
	//Normalize
	float n = NEDI_N;
	R /= n; r /= n;

	//Calculate a =  R^-1 . r
	float e = NEDI_E;
	float2 a = solve(R+e*e*I,r+e*e/2.0);

	//Nomalize 'a' (prevents overshoot)
	a = .25 + float2(.5,-.5)*clamp(a[0]-a[1],-1,1);

	//Calculate result
	float2x4 x = float2x4(Value(dir[0])+Value(dir[1]),Value(dir[2])+Value(dir[3]));
	float4 c = mul(float1x2(a),x);
	
	SetOutput(c);
	}
}


#undef px
#undef py
#define px (1.0 / (texture_size.x))
#define py (1.0 / (texture_size.y))

void nedi_p1()
{
	float2 texture_size = GetResolution();
	float2 tex = GetCoordinates();

	float4 c0 = Sample();

	//Skip pixels on wrong grid
	if ((frac(tex.x*width/2.0)<0.5)&&(frac(tex.y*height/2.0)<0.5) || (frac(tex.x*width/2.0)>0.5)&&(frac(tex.y*height/2.0)>0.5)) SetOutput(c0);
	else
	{
	
	//Define window and directions
  	float2 dir[4] =  {{-1,0},{1,0},{0,1},{0,-1}};
  	float4x2 wind[7] = {{{-1,0},{1,0},{0,1},{0,-1}},{{-2,-1},{2,1},{-1,2},{1,-2}},{{-3,-2},{3,2},{-2,3},{2,-3}},
                                                        {{-2,1},{2,-1},{1,2},{-1,-2}},{{-3,2},{3,-2},{2,3},{-2,-3}},
							{{-4,-1},{4,1},{-1,4},{1,-4}},{{-4,1},{4,-1},{1,4},{-1,-4}}};


/*
                                         wind[1]              wind[2]
-3                                                                 
-2             dir        wind[0]            2                 1 
-1              4           4          4                             4  
 0            1   2       1   2                   
 1              3           3                   3           3       
 2                                        1                       2
 3                                                                     
*/

/*
                                         wind[1]              wind[2]
-3                                            3                1   
-2             dir        wind[0] 
-1            1   4        1   3      1                                3
 0                                     
 1            3   2        2   4                  4        2    
 2
 3                                        2                        4   
*/

	//Initialization
	float2x2 R = 0;
	float2 r = 0;

        float m[7] = {NEDI_WEIGHT, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

	//Calculate (local) autocorrelation coefficients
	for (int k = 0; k<ITERATIONS; k+= 1){
		float4 y = float4(Get(wind[k][0]),Get(wind[k][1]),Get(wind[k][2]),Get(wind[k][3]));
		float4x2 C = float4x2(Get4(wind[k][0]),Get4(wind[k][1]),Get4(wind[k][2]),Get4(wind[k][3]));
		R += mul(transpose(C),m[k]*C);
		r += mul(y,m[k]*C);
	}
	
	//Normalize
	float n = NEDI_N;
	R /= n; r /= n;

	//Calculate a =  R^-1 . r
	float e = NEDI_E;
	float2 a = solve(R+e*e*I,r+e*e/2.0);

	//Nomalize 'a' (prevents overshoot)
	a = .25 + float2(.5,-.5)*clamp(a[0]-a[1],-1.0,1.0);

	//Calculate result
	float2x4 x = float2x4(Value(dir[0])+Value(dir[1]),Value(dir[2])+Value(dir[3]));
	float4 c = mul(float1x2(a),x);
	
	SetOutput(c);
	}
}


#undef px
#undef py
#define px (1.0 / (2.0*texture_size.x))
#define py (1.0 / (2.0*texture_size.y))


void nedi_p2()
{
	float2 texture_size = GetResolution();
	float2 tex = GetCoordinates() - float2(0.5,0.5)/texture_size;
	
	//Define window and directions
	float2 dir[4] = {{-1,-1},{1,1},{-1,1},{1,-1}};
	float4x2 wind[4] = {{{-1,-1},{1,1},{-1,1},{1,-1}},{{-3,-1},{3,1},{-1,3},{1,-3}},{{-3,1},{3,-1},{1,3},{-1,-3}},{{-3,-3},{ 3,3},{-3, 3},{3,-3}}};

	//Initialization
	float2x2 R = 0;
	float2 r = 0;

        float m[7] = {NEDI_WEIGHT, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

	//Calculate (local) autocorrelation coefficients
	for (int k = 0; k<ITERATIONS; k+= 1){
		float4 y = float4(Get(wind[k][0]),Get(wind[k][1]),Get(wind[k][2]),Get(wind[k][3]));
		float4x2 C = float4x2(Get4(wind[k][0]),Get4(wind[k][1]),Get4(wind[k][2]),Get4(wind[k][3]));
		R += mul(transpose(C),m[k]*C);
		r += mul(y,m[k]*C);
	}
	
	//Normalize
	float n = NEDI_N;
	R /= n; r /= n;

	//Calculate a =  R^-1 . r
	float e = NEDI_E;
	float2 a = solve(R+e*e*I,r+e*e/2.0);

	//Nomalize 'a' (prevents overshoot)
	a = .25 + float2(.5,-.5)*clamp(a[0]-a[1],-1,1);

	//Calculate result
	float2x4 x = float2x4(Value(dir[0])+Value(dir[1]),Value(dir[2])+Value(dir[3]));
	float4 c = mul(float1x2(a),x);

	SetOutput(c);
}



float3 min4(float3 a, float3 b, float3 c, float3 d)
{
    return min(a, min(b, min(c, d)));
}
float3 max4(float3 a, float3 b, float3 c, float3 d)
{
    return max(a, max(b, max(c, d)));
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

