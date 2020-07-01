/*
[configuration]

[OptionRangeFloat]
GUIName = Sharpening Strength
OptionName = CURVE_HEIGHT
MinValue = 0.1
MaxValue = 2.0
StepAmount = 0.1
DefaultValue = 0.2

[Pass]
Input0=ColorBuffer
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=AdaptiveSharpen_p1

[Pass]
Input0=PreviousPass
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=AdaptiveSharpen_p2
[/configuration]
*/


/* Ported by Hyllian - 2016 */



// Copyright (c) 2015, bacondither
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer
//    in this position and unchanged.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#define VIDEO_LEVEL_OUT 0.0

//--------------------------------------- Settings ------------------------------------------------

#define curve_height  GetOption(CURVE_HEIGHT)   // Main sharpening strength, POSITIVE VALUES ONLY!
                                                // 0.3 <-> 2.0 is a reasonable range of values

#define video_level_out VIDEO_LEVEL_OUT       // True to preserve BTB & WTW (minor summation error)
                                              // Normally it should be set to false

//-------------------------------------------------------------------------------------------------
// Defined values under this row are "optimal" DO NOT CHANGE IF YOU DO NOT KNOW WHAT YOU ARE DOING!

#define curveslope      (curve_height*0.7)   // Sharpening curve slope, high edge values

#define D_overshoot     0.009                // Max dark overshoot before max compression, >0!
#define D_compr_low     0.250                // Max compression ratio, dark overshoot (1/0.250=4x)
#define D_compr_high    0.500                // -||- pixel surrounded by edges (1/0.500=2x)

#define L_overshoot     0.003                // Max light overshoot before max compression, >0!
#define L_compr_low     0.167                // Max compression ratio, light overshoot (1/0.167=6x)
#define L_compr_high    0.334                // -||- pixel surrounded by edges (1/0.334=3x)

#define max_scale_lim   0.1                  // Abs change before max compression (0.1 = +-10%)

#define alpha_out       1.0                  // MPDN requires the alpha channel output to be 1

//-------------------------------------------------------------------------------------------------
#define w_offset        0.0                  // Edge channel offset, must be the same in all passes
//-------------------------------------------------------------------------------------------------

// Saturation loss reduction
#define minim_satloss  ((c[0].rgb*(CtL(c[0].rgb+sharpdiff)/c0_Y)+(c[0].rgb+sharpdiff))/2)

// Soft if, fast
#define soft_if(a,b,c) (clamp((a+b+c-3*w_offset)/(clamp(maxedge, 0.0, 1.0)+0.0067)-0.85, 0.0, 1.0))

// Soft limit
#define soft_lim(v,s)  (((exp(2*min(abs(v),s*16)/s)-1)/(exp(2*min(abs(v),s*16)/s)+1))*s)

// Get destination pixel values
#define get(x,y)       (SampleLocation(tex+float2(x*px,y*py)))
#define sat(input)     (float4(clamp((input).xyz, 0.0, 1.0),(input).w))

// Maximum of four values
#define max4(a,b,c,d)  (max(max(a,b),max(c,d)))

// Colour to luma, fast approx gamma
#define CtL(RGB)       (sqrt(dot(float3(0.256,0.651,0.093),clamp((RGB).rgb*abs(RGB).rgb, 0.0, 1.0))))

// Center pixel diff
#define mdiff(a,b,c,d,e,f,g) (abs(luma[g]-luma[a])+abs(luma[g]-luma[b])+abs(luma[g]-luma[c])+abs(luma[g]-luma[d])+0.5*(abs(luma[g]-luma[e])+abs(luma[g]-luma[f])))




// Adaptive sharpen - version 2015-12-09 - (requires ps >= 3.0)
// Tuned for use post resize, EXPECTS FULL RANGE GAMMA LIGHT

// Get destination pixel values
#define getsat(x,y)  (clamp(SampleLocation(tex+float2(x*px,y*py)).rgb, 0.0, 1.0))

// Compute diff
#define b_diff(z) (abs(blur-c[z]))

// First pass
void AdaptiveSharpen_p1()
{

float2 tex = GetCoordinates();
float2 texture_size = GetResolution();

float px = 1.0/texture_size.x;
float py = 1.0/texture_size.y;


	// Get points and saturate out of range values (BTB & WTW)
	// [                c22               ]
	// [           c24, c9,  c23          ]
	// [      c21, c1,  c2,  c3, c18      ]
	// [ c19, c10, c4,  c0,  c5, c11, c16 ]
	// [      c20, c6,  c7,  c8, c17      ]
	// [           c15, c12, c14          ]
	// [                c13               ]
	float3 c[25] = { getsat( 0, 0), getsat(-1,-1), getsat( 0,-1), getsat( 1,-1), getsat(-1, 0),
	                 getsat( 1, 0), getsat(-1, 1), getsat( 0, 1), getsat( 1, 1), getsat( 0,-2),
	                 getsat(-2, 0), getsat( 2, 0), getsat( 0, 2), getsat( 0, 3), getsat( 1, 2),
	                 getsat(-1, 2), getsat( 3, 0), getsat( 2, 1), getsat( 2,-1), getsat(-3, 0),
	                 getsat(-2, 1), getsat(-2,-1), getsat( 0,-3), getsat( 1,-2), getsat(-1,-2) };

	// Blur, gauss 3x3
	float3 blur   = (2*(c[2]+c[4]+c[5]+c[7]) + (c[1]+c[3]+c[6]+c[8]) + 4*c[0])/16;
	float  blur_Y = (blur.r/3 + blur.g/3 + blur.b/3);

	// Contrast compression, center = 0.5, scaled to 1/3
	float c_comp = clamp(0.266666681f + 0.9*pow(2, (-7.4*blur_Y)), 0.0, 1.0);

	// Edge detection
	// Matrix weights
	// [         1/4,        ]
	// [      4,  1,  4      ]
	// [ 1/4, 4,  1,  4, 1/4 ]
	// [      4,  1,  4      ]
	// [         1/4         ]
	float edge = length( b_diff(0) + b_diff(1) + b_diff(2) + b_diff(3)
	                   + b_diff(4) + b_diff(5) + b_diff(6) + b_diff(7) + b_diff(8)
	                   + 0.25*(b_diff(9) + b_diff(10) + b_diff(11) + b_diff(12)) );

	SetOutput(float4( (Sample().rgb), (edge*c_comp + w_offset) ));

}


// Second pass
void AdaptiveSharpen_p2()
{


float2 tex = GetCoordinates();
float2 texture_size = GetResolution();

float px = 1.0/texture_size.x;
float py = 1.0/texture_size.y;

	float4 orig0  = Sample();
	float c_edge = orig0.w - w_offset;

	// Displays a green screen if the edge data is not inside a valid range in the .w channel
	if (c_edge > 32 || c_edge < -0.5  ) { SetOutput(float4(0, 1, 0, alpha_out)); }

	// Get points, saturate colour data in c[0]
	// [                c22               ]
	// [           c24, c9,  c23          ]
	// [      c21, c1,  c2,  c3, c18      ]
	// [ c19, c10, c4,  c0,  c5, c11, c16 ]
	// [      c20, c6,  c7,  c8, c17      ]
	// [           c15, c12, c14          ]
	// [                c13               ]
	float4 c[25] = { sat( orig0), get(-1,-1), get( 0,-1), get( 1,-1), get(-1, 0),
	                 get( 1, 0), get(-1, 1), get( 0, 1), get( 1, 1), get( 0,-2),
	                 get(-2, 0), get( 2, 0), get( 0, 2), get( 0, 3), get( 1, 2),
	                 get(-1, 2), get( 3, 0), get( 2, 1), get( 2,-1), get(-3, 0),
	                 get(-2, 1), get(-2,-1), get( 0,-3), get( 1,-2), get(-1,-2) };

	// Allow for higher overshoot if the current edge pixel is surrounded by similar edge pixels
	float maxedge = (max4( max4(c[1].w,c[2].w,c[3].w,c[4].w), max4(c[5].w,c[6].w,c[7].w,c[8].w),
	                       max4(c[9].w,c[10].w,c[11].w,c[12].w), c[0].w ))-w_offset;

	// [          x          ]
	// [       z, x, w       ]
	// [    z, z, x, w, w    ]
	// [ y, y, y, 0, y, y, y ]
	// [    w, w, x, z, z    ]
	// [       w, x, z       ]
	// [          x          ]
	float var = soft_if(c[2].w,c[9].w,c[22].w) *soft_if(c[7].w,c[12].w,c[13].w)  // x dir
	          + soft_if(c[4].w,c[10].w,c[19].w)*soft_if(c[5].w,c[11].w,c[16].w)  // y dir
	          + soft_if(c[1].w,c[24].w,c[21].w)*soft_if(c[8].w,c[14].w,c[17].w)  // z dir
	          + soft_if(c[3].w,c[23].w,c[18].w)*soft_if(c[6].w,c[20].w,c[15].w); // w dir

	float s[2] = { lerp( L_compr_low, L_compr_high, smoothstep(2, 3.1, var) ),
	               lerp( D_compr_low, D_compr_high, smoothstep(2, 3.1, var) ) };

	// RGB to luma
	float c0_Y = CtL(c[0]);

	float luma[25] = { c0_Y, CtL(c[1]), CtL(c[2]), CtL(c[3]), CtL(c[4]), CtL(c[5]), CtL(c[6]),
	                   CtL(c[7]),  CtL(c[8]),  CtL(c[9]),  CtL(c[10]), CtL(c[11]), CtL(c[12]),
	                   CtL(c[13]), CtL(c[14]), CtL(c[15]), CtL(c[16]), CtL(c[17]), CtL(c[18]),
	                   CtL(c[19]), CtL(c[20]), CtL(c[21]), CtL(c[22]), CtL(c[23]), CtL(c[24]) };

	// Precalculated default squared kernel weights
	float3 w1 = float3(0.5,           1.0, 1.41421356237); // 0.25, 1.0, 2.0
	float3 w2 = float3(0.86602540378, 1.0, 0.5477225575);  // 0.75, 1.0, 0.3

	// Transition to a concave kernel if the center edge val is above thr
	float3 dW = pow(lerp( w1, w2, smoothstep( 0.3, 0.6, c_edge)), 2);

	float mdiff_c0  = 0.02 + 3*( abs(luma[0]-luma[2]) + abs(luma[0]-luma[4])
	                           + abs(luma[0]-luma[5]) + abs(luma[0]-luma[7])
	                           + 0.25*(abs(luma[0]-luma[1]) + abs(luma[0]-luma[3])
	                                  +abs(luma[0]-luma[6]) + abs(luma[0]-luma[8])) );

	// Use lower weights for pixels in a more active area relative to center pixel area.
	float weights[12]  = { ( dW.x ), ( dW.x ), ( dW.x ), ( dW.x ), // c2, // c4, // c5, // c7
	                       ( min(mdiff_c0/mdiff(24, 21, 2,  4,  9,  10, 1),  dW.y) ),   // c1
	                       ( min(mdiff_c0/mdiff(23, 18, 5,  2,  9,  11, 3),  dW.y) ),   // c3
	                       ( min(mdiff_c0/mdiff(4,  20, 15, 7,  10, 12, 6),  dW.y) ),   // c6
	                       ( min(mdiff_c0/mdiff(5,  7,  17, 14, 12, 11, 8),  dW.y) ),   // c8
	                       ( min(mdiff_c0/mdiff(2,  24, 23, 22, 1,  3,  9),  dW.z) ),   // c9
	                       ( min(mdiff_c0/mdiff(20, 19, 21, 4,  1,  6,  10), dW.z) ),   // c10
	                       ( min(mdiff_c0/mdiff(17, 5,  18, 16, 3,  8,  11), dW.z) ),   // c11
	                       ( min(mdiff_c0/mdiff(13, 15, 7,  14, 6,  8,  12), dW.z) ) }; // c12

	weights[4] = (max(max((weights[8]  + weights[9])/4,  weights[4]), 0.25) + weights[4])/2;
	weights[5] = (max(max((weights[8]  + weights[10])/4, weights[5]), 0.25) + weights[5])/2;
	weights[6] = (max(max((weights[9]  + weights[11])/4, weights[6]), 0.25) + weights[6])/2;
	weights[7] = (max(max((weights[10] + weights[11])/4, weights[7]), 0.25) + weights[7])/2;

	// Calculate the negative part of the laplace kernel and the low threshold weight
	float lowthsum    = 0;
	float weightsum   = 0;
	float neg_laplace = 0;

	int order[12] = { 2, 4, 5, 7, 1, 3, 6, 8, 9, 10, 11, 12 };

	for (int pix = 0; pix < 12; ++pix)
	{
		float x       = clamp((c[order[pix]].w - w_offset - 0.01)/0.11, 0.0, 1.0);
		float lowth   = x*x*(2.99 - 2*x) + 0.01;

		neg_laplace  += pow(luma[order[pix]] + 0.064, 2.4)*(weights[pix]*lowth);
		weightsum    += weights[pix]*lowth;
		lowthsum     += lowth;
	}

	neg_laplace = pow((neg_laplace/weightsum), (1.0/2.4)) - 0.064;

	// Compute sharpening magnitude function
	float sharpen_val = (lowthsum/12)*(curve_height/(curveslope*pow(abs(c_edge), 3.5) + 0.5));

	// Calculate sharpening diff and scale
	float sharpdiff = (c0_Y - neg_laplace)*(sharpen_val*0.8 + 0.01);

	// Calculate local near min & max, partial cocktail sort (No branching!)
	for (int i = 0; i < 3; ++i)
	{
		for (int i1 = 1+i; i1 < 25-i; ++i1)
		{
			float temp = luma[i1-1];
			luma[i1-1] = min(luma[i1-1], luma[i1]);
			luma[i1]   = max(temp, luma[i1]);
		}

		for (int i2 = 23-i; i2 > i; --i2)
		{
			float temp = luma[i2-1];
			luma[i2-1] = min(luma[i2-1], luma[i2]);
			luma[i2]   = max(temp, luma[i2]);
		}
	}

	float nmax = max(((luma[22] + luma[23]*2 + luma[24])/4), c0_Y);
	float nmin = min(((luma[0]  + luma[1]*2  + luma[2])/4),  c0_Y);

	// Calculate tanh scale factor, pos/neg
	float nmax_scale = min(((nmax - c0_Y) + L_overshoot), max_scale_lim);
	float nmin_scale = min(((c0_Y - nmin) + D_overshoot), max_scale_lim);

	// Soft limit sharpening with tanh, lerp to control maximum compression
	sharpdiff = lerp(  (soft_lim(max(sharpdiff, 0), nmax_scale)), max(sharpdiff, 0), s[0] )
	          + lerp( -(soft_lim(min(sharpdiff, 0), nmin_scale)), min(sharpdiff, 0), s[1] );

	if (video_level_out == true)
	{
		if (sharpdiff > 0) { SetOutput(float4( orig0.rgb + (minim_satloss - c[0].rgb), alpha_out )); }

		else { SetOutput(float4( (orig0.rgb + sharpdiff), alpha_out )); }
	}

	// Normal path
	if (sharpdiff > 0) { SetOutput(float4( minim_satloss, alpha_out )); }

	else { SetOutput(float4( (c[0].rgb + sharpdiff), alpha_out )); }
}
