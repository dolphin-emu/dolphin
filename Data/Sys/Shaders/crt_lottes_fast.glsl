//_____________________________/\_______________________________
//==============================================================
//
//
//      [CRTS] PUBLIC DOMAIN CRT-STYLED SCALAR - 20180120b
//
//                      by Timothy Lottes
//             https://www.shadertoy.com/view/MtSfRK
//               adapted for RetroArch by hunterk
//                adapted for Dolphin by Clownacy
//
//
//==============================================================
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//_____________________________/\_______________________________
//==============================================================
//
//                         WHAT'S NEW
//
//--------------------------------------------------------------
// Evolution of prior shadertoy example
//--------------------------------------------------------------
// This one is semi-optimized
//  - Less texture fetches
//  - Didn't get to instruction level optimization
//  - Could likely use texture fetch to generate phosphor mask
//--------------------------------------------------------------
// Added options to disable unused features
//--------------------------------------------------------------
// Added in exposure matching
//  - Given scan-line effect and mask always darkens image
//  - Uses generalized tonemapper to boost mid-level
//  - Note this can compress highlights
//  - And won't get back peak brightness
//  - But best option if one doesn't want as much darkening
//--------------------------------------------------------------
// Includes option saturation and contrast controls
//--------------------------------------------------------------
// Added in subtractive aperture grille
//  - This is a bit brighter than prior
//--------------------------------------------------------------
// Make sure input to this filter is already low-resolution
//  - This is not designed to work on titles doing the following
//     - Rendering to hi-res with nearest sampling
//--------------------------------------------------------------
// Added a fast and more pixely option for 2 tap/pixel
//--------------------------------------------------------------
// Improved the vignette when WARP is enabled
//--------------------------------------------------------------
// Didn't test HLSL or CPU options
//  - Will incorportate patches if they are broken
//  - But out of time to try them myself
//==============================================================
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//_____________________________/\_______________________________
//==============================================================
//
//          LICENSE = UNLICENSE (aka PUBLIC DOMAIN)
//
//--------------------------------------------------------------
// This is free and unencumbered software released into the 
// public domain.
//--------------------------------------------------------------
// Anyone is free to copy, modify, publish, use, compile, sell, 
// or distribute this software, either in source code form or as
// a compiled binary, for any purpose, commercial or 
// non-commercial, and by any means.
//--------------------------------------------------------------
// In jurisdictions that recognize copyright laws, the author or
// authors of this software dedicate any and all copyright 
// interest in the software to the public domain. We make this
// dedication for the benefit of the public at large and to the
// detriment of our heirs and successors. We intend this 
// dedication to be an overt act of relinquishment in perpetuity
// of all present and future rights to this software under 
// copyright law.
//--------------------------------------------------------------
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
// KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT 
// OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
// DEALINGS IN THE SOFTWARE.
//--------------------------------------------------------------
// For more information, please refer to 
// <http://unlicense.org/>
//==============================================================
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

/*
[configuration]

[OptionRangeFloat]
GUIName = Mask Type
OptionName = MASK
MaxValue = 3.0
MinValue = 0.0
DefaultValue = 1.0
StepAmount = 1.0

[OptionRangeFloat]
GUIName = Mask Intensity
OptionName = MASK_INTENSITY
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.5
StepAmount = 0.05

[OptionRangeFloat]
GUIName = Scanline Intensity
OptionName = SCANLINE_THINNESS
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.5
StepAmount = 0.1

[OptionRangeFloat]
GUIName = Sharpness
OptionName = SCAN_BLUR
MaxValue = 3.0
MinValue = 1.0
DefaultValue = 2.5
StepAmount = 0.1

[OptionRangeFloat]
GUIName = Curvature
OptionName = CURVATURE
MaxValue = 0.25
MinValue = 0.0
DefaultValue = 0.02
StepAmount = 0.01

[OptionRangeFloat]
GUIName = Trinitron-style Curve
OptionName = TRINITRON_CURVE
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.0
StepAmount = 1.0

[OptionRangeFloat]
GUIName = Corner Round
OptionName = CORNER
MaxValue = 11.0
MinValue = 0.0
DefaultValue = 3.0
StepAmount = 1.0

[OptionRangeFloat]
GUIName = CRT Gamma
OptionName = CRT_GAMMA
MaxValue = 51.0
MinValue = 0.0
DefaultValue = 2.4
StepAmount = 0.1

[/configuration]
*/

//_____________________________/\_______________________________
//==============================================================
//
//                       GAMMA FUNCTIONS
//
//--------------------------------------------------------------
//--------------------------------------------------------------
// Since shadertoy doesn't have sRGB textures
// And we need linear input into shader
// Don't do this in your code
	float FromSrgb1(float c){
		return (c<=0.04045)?c*(1.0/12.92):
			pow(abs(c)*(1.0/1.055)+(0.055/1.055),GetOption(CRT_GAMMA));}
//--------------------------------------------------------------
vec3 FromSrgb(vec3 c){return vec3(
 FromSrgb1(c.r),FromSrgb1(c.g),FromSrgb1(c.b));}
 
// Convert from linear to sRGB
// Since shader toy output is not linear
float ToSrgb1(float c){
 return(c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);}
//--------------------------------------------------------------
vec3 ToSrgb(vec3 c){return vec3(
 ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));}
//--------------------------------------------------------------

//_____________________________/\_______________________________
//==============================================================
//
//                           DEFINES
//
//--------------------------------------------------------------
// CRTS_CPU - CPU code
// CRTS_GPU - GPU code
//--------------------------------------------------------------
// CRTS_GLSL - GLSL
// CRTS_HLSL - HLSL (not tested yet)
//--------------------------------------------------------------
// CRTS_DEBUG - Define to see on/off split screen
//--------------------------------------------------------------
// CRTS_WARP - Apply screen warp
//--------------------------------------------------------------
// CRTS_2_TAP - Faster very pixely 2-tap filter (off is 8)
//--------------------------------------------------------------
// CRTS_MASK_GRILLE      - Aperture grille (aka Trinitron)
// CRTS_MASK_GRILLE_LITE - Brighter (subtractive channels)
// CRTS_MASK_NONE        - No mask
// CRTS_MASK_SHADOW      - Horizontally stretched shadow mask
//--------------------------------------------------------------
// CRTS_TONE       - Normalize mid-level and process color
// CRTS_CONTRAST   - Process color - enable contrast control
// CRTS_SATURATION - Process color - enable saturation control
//--------------------------------------------------------------
#define CRTS_STATIC
#define CrtsPow
#define CRTS_RESTRICT
//==============================================================
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

//==============================================================
//                      SETUP FOR CRTS
//--------------------------------------------------------------
//==============================================================
//#define CRTS_DEBUG 1
#define CRTS_GPU 1
#define CRTS_GLSL 1
//--------------------------------------------------------------
//#define CRTS_2_TAP 1
//--------------------------------------------------------------
#define CRTS_TONE 1
#define CRTS_CONTRAST 0
#define CRTS_SATURATION 0
//--------------------------------------------------------------
#define CRTS_WARP 1
//--------------------------------------------------------------
// Try different masks -> moved to runtime parameters
//#define CRTS_MASK_GRILLE 1
//#define CRTS_MASK_GRILLE_LITE 1
//#define CRTS_MASK_NONE 1
//#define CRTS_MASK_SHADOW 1
//--------------------------------------------------------------
// Scanline thinness
//  0.50 = fused scanlines
//  0.70 = recommended default
//  1.00 = thinner scanlines (too thin)
#define INPUT_THIN (0.5 + (0.5 * GetOption(SCANLINE_THINNESS)))
//--------------------------------------------------------------
// Horizonal scan blur
//  -3.0 = pixely
//  -2.5 = default
//  -2.0 = smooth
//  -1.0 = too blurry
#define INPUT_BLUR (-1.0 * GetOption(SCAN_BLUR))
//--------------------------------------------------------------
// Shadow mask effect, ranges from,
//  0.25 = large amount of mask (not recommended, too dark)
//  0.50 = recommended default
//  1.00 = no shadow mask
#define INPUT_MASK (1.0 - GetOption(MASK_INTENSITY))
//--------------------------------------------------------------
//#define INPUT_X SourceSize.x
//#define INPUT_Y SourceSize.y
//--------------------------------------------------------------
// Setup the function which returns input image color
vec3 CrtsFetch(vec2 uv){
 // For shadertoy, scale to get native texels in the image
// uv*=vec2(INPUT_X,INPUT_Y)/SourceSize.xy;
 // Move towards intersting parts
// uv+=vec2(0.5,0.5);
 // Non-shadertoy case would not have the color conversion
 return FromSrgb(SampleLocation(uv.xy).rgb);}
 
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//_____________________________/\_______________________________
//==============================================================
//
//                          GPU CODE
//
//==============================================================
#ifdef CRTS_GPU
//_____________________________/\_______________________________
//==============================================================
//                         PORTABILITY
//==============================================================
 #ifdef CRTS_GLSL
  #define CrtsF1 float
  #define CrtsF2 vec2
  #define CrtsF3 vec3
  #define CrtsF4 vec4
  #define CrtsFractF1 fract
  #define CrtsRcpF1(x) (1.0/(x))
  #define CrtsSatF1(x) clamp((x),0.0,1.0)
//--------------------------------------------------------------
  CrtsF1 CrtsMax3F1(CrtsF1 a,CrtsF1 b,CrtsF1 c){
   return max(a,max(b,c));}
 #endif
//==============================================================
 #ifdef CRTS_HLSL
  #define CrtsF1 float
  #define CrtsF2 float2
  #define CrtsF3 float3
  #define CrtsF4 float4
  #define CrtsFractF1 frac
  #define CrtsRcpF1(x) (1.0/(x))
  #define CrtsSatF1(x) saturate(x)
//--------------------------------------------------------------
  CrtsF1 CrtsMax3F1(CrtsF1 a,CrtsF1 b,CrtsF1 c){
   return max(a,max(b,c));}
 #endif
//_____________________________/\_______________________________
//==============================================================
//              TONAL CONTROL CONSTANT GENERATION
//--------------------------------------------------------------
// This is in here for rapid prototyping
// Please use the CPU code and pass in as constants
//==============================================================
 CrtsF4 CrtsTone(
 CrtsF1 contrast,
 CrtsF1 saturation,
 CrtsF1 thin,
 CrtsF1 mask){
//--------------------------------------------------------------
  if(GetOption(MASK) == 0.0) mask=1.0;
//--------------------------------------------------------------
  if(GetOption(MASK) == 1.0){
   // Normal R mask is {1.0,mask,mask}
   // LITE   R mask is {mask,1.0,1.0}
   mask=0.5+mask*0.5;
   }
//--------------------------------------------------------------
  CrtsF4 ret;
  CrtsF1 midOut=0.18/((1.5-thin)*(0.5*mask+0.5));
  CrtsF1 pMidIn=pow(0.18,contrast);
  ret.x=contrast;
  ret.y=((-pMidIn)+midOut)/((1.0-pMidIn)*midOut);
  ret.z=((-pMidIn)*midOut+pMidIn)/(midOut*(-pMidIn)+midOut);
  ret.w=contrast+saturation;
  return ret;}
//_____________________________/\_______________________________
//==============================================================
//                            MASK
//--------------------------------------------------------------
// Letting LCD/OLED pixel elements function like CRT phosphors
// So "phosphor" resolution scales with display resolution
//--------------------------------------------------------------
// Not applying any warp to the mask (want high frequency)
// Real aperture grille has a mask which gets wider on ends
// Not attempting to be "real" but instead look the best
//--------------------------------------------------------------
// Shadow mask is stretched horizontally
//  RRGGBB
//  GBBRRG
//  RRGGBB
// This tends to look better on LCDs than vertical
// Also 2 pixel width is required to get triad centered
//--------------------------------------------------------------
// The LITE version of the Aperture Grille is brighter
// Uses {dark,1.0,1.0} for R channel
// Non LITE version uses {1.0,dark,dark}
//--------------------------------------------------------------
// 'pos' - This is 'fragCoord.xy'
//         Pixel {0,0} should be {0.5,0.5}
//         Pixel {1,1} should be {1.5,1.5} 
//--------------------------------------------------------------
// 'dark' - Exposure of of masked channel
//          0.0=fully off, 1.0=no effect
//==============================================================
 CrtsF3 CrtsMask(CrtsF2 pos,CrtsF1 dark){
  if(GetOption(MASK) == 2.0){
   CrtsF3 m=CrtsF3(dark,dark,dark);
   CrtsF1 x=CrtsFractF1(pos.x*(1.0/3.0));
   if(x<(1.0/3.0))m.r=1.0;
   else if(x<(2.0/3.0))m.g=1.0;
   else m.b=1.0;
   return m;
  }
//--------------------------------------------------------------
  else if(GetOption(MASK) == 1.0){
   CrtsF3 m=CrtsF3(1.0,1.0,1.0);
   CrtsF1 x=CrtsFractF1(pos.x*(1.0/3.0));
   if(x<(1.0/3.0))m.r=dark;
   else if(x<(2.0/3.0))m.g=dark;
   else m.b=dark;
   return m;
  }
//--------------------------------------------------------------
  else if(GetOption(MASK) == 3.0){
   pos.x+=pos.y*2.9999;
   CrtsF3 m=CrtsF3(dark,dark,dark);
   CrtsF1 x=CrtsFractF1(pos.x*(1.0/6.0));
   if(x<(1.0/3.0))m.r=1.0;
   else if(x<(2.0/3.0))m.g=1.0;
   else m.b=1.0;
   return m;
  }
//--------------------------------------------------------------
  else{
   return CrtsF3(1.0,1.0,1.0);
  }
 }
//_____________________________/\_______________________________
//==============================================================
//                        FILTER ENTRY
//--------------------------------------------------------------
// Input must be linear
// Output color is linear
//--------------------------------------------------------------
// Must have fetch function setup: CrtsF3 CrtsFetch(CrtsF2 uv)
//  - The 'uv' range is {0.0 to 1.0} for input texture
//  - Output of this must be linear color
//--------------------------------------------------------------
// SCANLINE MATH & AUTO-EXPOSURE NOTES
// ===================================
// Each output line has contribution from at most 2 scanlines
// Scanlines are shaped by a windowed cosine function
// This shape blends together well with only 2 lines of overlap
//--------------------------------------------------------------
// Base scanline intensity is as follows
// which leaves output intensity range from {0 to 1.0}
// --------
// thin := range {thick 0.5 to thin 1.0}
// off  := range {0.0 to <1.0}, 
//         sub-pixel offset between two scanlines
//  --------
//  a0=cos(min(0.5,     off *thin)*2pi)*0.5+0.5;
//  a1=cos(min(0.5,(1.0-off)*thin)*2pi)*0.5+0.5;
//--------------------------------------------------------------
// This leads to a image darkening factor of roughly: 
//  {(1.5-thin)/1.0}
// This is further reduced by the mask: 
//  {1.0/2.0+mask*1.0/2.0}
// Reciprocal of combined effect is used for auto-exposure
//  to scale up the mid-level in the tonemapper
//==============================================================
 CrtsF3 CrtsFilter(
//--------------------------------------------------------------
  // SV_POSITION, fragCoord.xy
  CrtsF2 ipos,
//--------------------------------------------------------------
  // inputSize / outputSize (in pixels)
  CrtsF2 inputSizeDivOutputSize,     
//--------------------------------------------------------------
  // 0.5 * inputSize (in pixels)
  CrtsF2 halfInputSize,
//--------------------------------------------------------------
  // 1.0 / inputSize (in pixels)
  CrtsF2 rcpInputSize,
//--------------------------------------------------------------
  // 1.0 / outputSize (in pixels)
  CrtsF2 rcpOutputSize,
//--------------------------------------------------------------
  // 2.0 / outputSize (in pixels)
  CrtsF2 twoDivOutputSize,   
//--------------------------------------------------------------
  // inputSize.y
  CrtsF1 inputHeight,
//--------------------------------------------------------------
  // Warp scanlines but not phosphor mask
  //  0.0 = no warp
  //  1.0/64.0 = light warping
  //  1.0/32.0 = more warping
  // Want x and y warping to be different (based on aspect)
  CrtsF2 warp,
//--------------------------------------------------------------
  // Scanline thinness
  //  0.50 = fused scanlines
  //  0.70 = recommended default
  //  1.00 = thinner scanlines (too thin)
  // Shared with CrtsTone() function
  CrtsF1 thin,
//--------------------------------------------------------------
  // Horizonal scan blur
  //  -3.0 = pixely
  //  -2.5 = default
  //  -2.0 = smooth
  //  -1.0 = too blurry
  CrtsF1 blur,
//--------------------------------------------------------------
  // Shadow mask effect, ranges from,
  //  0.25 = large amount of mask (not recommended, too dark)
  //  0.50 = recommended default
  //  1.00 = no shadow mask
  // Shared with CrtsTone() function
  CrtsF1 mask,
//--------------------------------------------------------------
  // Tonal curve parameters generated by CrtsTone()
  CrtsF4 tone
//--------------------------------------------------------------
 ){
//--------------------------------------------------------------
  #ifdef CRTS_DEBUG
   CrtsF2 uv=ipos*rcpOutputSize;
   // Show second half processed, and first half un-processed
   if(uv.x<0.5){
    // Force nearest to get squares
    uv*=1.0/rcpInputSize;
    uv=floor(uv)+CrtsF2(0.5,0.5);
    uv*=rcpInputSize;
    CrtsF3 color=CrtsFetch(uv);
    return color;}
  #endif
//--------------------------------------------------------------
  // Optional apply warp
  CrtsF2 pos;
  #ifdef CRTS_WARP
   // Convert to {-1 to 1} range
   pos=ipos*twoDivOutputSize-CrtsF2(1.0,1.0);
   // Distort pushes image outside {-1 to 1} range
   pos*=CrtsF2(
    1.0+(pos.y*pos.y)*warp.x,
    1.0+(pos.x*pos.x)*warp.y);
   // TODO: Vignette needs optimization
   CrtsF1 vin=(1.0-(
    (1.0-CrtsSatF1(pos.x*pos.x))*(1.0-CrtsSatF1(pos.y*pos.y)))) * (0.998 + (0.001 * GetOption(CORNER)));
   vin=CrtsSatF1((-vin)*inputHeight+inputHeight);
   // Leave in {0 to inputSize}
   pos=pos*halfInputSize+halfInputSize;     
  #else
   pos=ipos*inputSizeDivOutputSize;
  #endif
//--------------------------------------------------------------
  // Snap to center of first scanline
  CrtsF1 y0=floor(pos.y-0.5)+0.5;
  #ifdef CRTS_2_TAP
   // Using Inigo's "Improved Texture Interpolation"
   // http://iquilezles.org/www/articles/texture/texture.htm
   pos.x+=0.5;
   CrtsF1 xi=floor(pos.x);
   CrtsF1 xf=pos.x-xi;
   xf=xf*xf*xf*(xf*(xf*6.0-15.0)+10.0);  
   CrtsF1 x0=xi+xf-0.5;
   CrtsF2 p=CrtsF2(x0*rcpInputSize.x,y0*rcpInputSize.y);     
   // Coordinate adjusted bilinear fetch from 2 nearest scanlines
   CrtsF3 colA=CrtsFetch(p);
   p.y+=rcpInputSize.y;
   CrtsF3 colB=CrtsFetch(p);
  #else
   // Snap to center of one of four pixels
   CrtsF1 x0=floor(pos.x-1.5)+0.5;
   // Inital UV position
   CrtsF2 p=CrtsF2(x0*rcpInputSize.x,y0*rcpInputSize.y);     
   // Fetch 4 nearest texels from 2 nearest scanlines
   CrtsF3 colA0=CrtsFetch(p);
   p.x+=rcpInputSize.x;
   CrtsF3 colA1=CrtsFetch(p);
   p.x+=rcpInputSize.x;
   CrtsF3 colA2=CrtsFetch(p);
   p.x+=rcpInputSize.x;
   CrtsF3 colA3=CrtsFetch(p);
   p.y+=rcpInputSize.y;
   CrtsF3 colB3=CrtsFetch(p);
   p.x-=rcpInputSize.x;
   CrtsF3 colB2=CrtsFetch(p);
   p.x-=rcpInputSize.x;
   CrtsF3 colB1=CrtsFetch(p);
   p.x-=rcpInputSize.x;
   CrtsF3 colB0=CrtsFetch(p);
  #endif
//--------------------------------------------------------------
  // Vertical filter
  // Scanline intensity is using sine wave
  // Easy filter window and integral used later in exposure
  CrtsF1 off=pos.y-y0;
  CrtsF1 pi2=6.28318530717958;
  CrtsF1 hlf=0.5;
  CrtsF1 scanA=cos(min(0.5,  off *thin     )*pi2)*hlf+hlf;
  CrtsF1 scanB=cos(min(0.5,(-off)*thin+thin)*pi2)*hlf+hlf;
//--------------------------------------------------------------
  #ifdef CRTS_2_TAP
   #ifdef CRTS_WARP
    // Get rid of wrong pixels on edge
    scanA*=vin;
    scanB*=vin;
   #endif
   // Apply vertical filter
   CrtsF3 color=(colA*scanA)+(colB*scanB);
  #else
   // Horizontal kernel is simple gaussian filter
   CrtsF1 off0=pos.x-x0;
   CrtsF1 off1=off0-1.0;
   CrtsF1 off2=off0-2.0;
   CrtsF1 off3=off0-3.0;
   CrtsF1 pix0=exp2(blur*off0*off0);
   CrtsF1 pix1=exp2(blur*off1*off1);
   CrtsF1 pix2=exp2(blur*off2*off2);
   CrtsF1 pix3=exp2(blur*off3*off3);
   CrtsF1 pixT=CrtsRcpF1(pix0+pix1+pix2+pix3);
   #ifdef CRTS_WARP
    // Get rid of wrong pixels on edge
    pixT*=vin;
   #endif
   scanA*=pixT;
   scanB*=pixT;
   // Apply horizontal and vertical filters
   CrtsF3 color=
    (colA0*pix0+colA1*pix1+colA2*pix2+colA3*pix3)*scanA +
    (colB0*pix0+colB1*pix1+colB2*pix2+colB3*pix3)*scanB;
  #endif
//--------------------------------------------------------------
  // Apply phosphor mask          
  color*=CrtsMask(ipos,mask);
//--------------------------------------------------------------
  // Optional color processing
  #ifdef CRTS_TONE
   // Tonal control, start by protecting from /0
   CrtsF1 peak=max(1.0/(256.0*65536.0),
    CrtsMax3F1(color.r,color.g,color.b));
   // Compute the ratios of {R,G,B}
   CrtsF3 ratio=color*CrtsRcpF1(peak);
   // Apply tonal curve to peak value
   #ifdef CRTS_CONTRAST
    peak=pow(peak,tone.x);
   #endif
   peak=peak*CrtsRcpF1(peak*tone.y+tone.z);
   // Apply saturation
   #ifdef CRTS_SATURATION
    ratio=pow(ratio,CrtsF3(tone.w,tone.w,tone.w));
   #endif
   // Reconstruct color
   return ratio*peak;
 #else
  return color;
 #endif
//--------------------------------------------------------------
 }
#endif

void main()
{
	vec4 OutputSize = vec4(GetWindowResolution(), GetInvWindowResolution());
	vec4 SourceSize = vec4(GetResolution(), GetInvResolution());
	vec2 warp_factor;
	warp_factor.x = GetOption(CURVATURE);
	warp_factor.y = (3.0 / 4.0) * warp_factor.x; // assume 4:3 aspect
	warp_factor.x *= (1.0 - GetOption(TRINITRON_CURVE));
	vec3 color = CrtsFilter(GetCoordinates().xy * OutputSize.xy,
		SourceSize.xy * OutputSize.zw,
		SourceSize.xy * vec2(0.5,0.5),
		SourceSize.zw,
		OutputSize.zw,
		2.0 * OutputSize.zw,
		SourceSize.y,
		warp_factor,
		INPUT_THIN,
		INPUT_BLUR,
		INPUT_MASK,
		CrtsTone(1.0,0.0,INPUT_THIN,INPUT_MASK));
	
	// Shadertoy outputs non-linear color
	color = ToSrgb(color);
	SetOutput(vec4(color, 1.0));
}
