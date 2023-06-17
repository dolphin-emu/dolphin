/*
   Grade-mini - CRT emulation and color manipulation shader

   Copyright (C) 2020-2023 Dogway (Jose Linares)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


/*
   Grade-mini (16-06-2023)

   > CRT emulation shader (composite signal, phosphor, gamma, temperature...)
   > Abridged port of RetroArch's Grade shader.


    #####################################...STANDARDS...######################################
    ##########################################################################################
    ###                                                                                    ###
    ###    PAL                                                                             ###
    ###        Phosphor: 470BG (#3)                                                        ###
    ###        WP: D65 (6504K)               (in practice more like 7000K-7500K range)     ###
    ###        Saturation: -0.02                                                           ###
    ###                                                                                    ###
    ###    NTSC-U                                                                          ###
    ###        Phosphor: P22/SMPTE-C (#1 #-3)(or a SMPTE-C based CRT phosphor gamut)       ###
    ###        WP: D65 (6504K)               (in practice more like 7000K-7500K range)     ###
    ###                                                                                    ###
    ###    NTSC-J (Default)                                                                ###
    ###        Phosphor: NTSC-J (#2)         (or a NTSC-J based CRT phosphor gamut)        ###
    ###        WP: 9300K+27MPCD (8945K)      (CCT from x:0.281 y:0.311)(in practice ~8500K)###
    ###                                                                                    ###
    ###                                                                                    ###
    ##########################################################################################
    ##########################################################################################
*/


// Test the following Phosphor gamuts and try to reach to a conclusion
// For GC  Japan developed games you can use -2 (Rear Projection TVs) or 2 (CRT Tube)
// For Wii Japan developed games probably -3 (SMPTE-C), -2 (Rear Projection TVs) or 0 (sRGB/noop)
// For Japan developed games use a temperature ~8500K (default)
// For EU/US developed games use a temperature ~7100K


/*
[configuration]

[OptionBool]
GUIName = Composite Signal Type
OptionName = g_signal_type
DefaultValue = true

[OptionRangeInteger]
GUIName = Phosphor (-3:170M -2:RPTV-95s -1:P22-80s 1:P22-90s 2:NTSC-J 3:PAL)
OptionName = g_crtgamut
MinValue = -3
MaxValue = 3
StepAmount = 1
DefaultValue = 2

[OptionRangeInteger]
GUIName = Diplay Color Space (0:709 1:sRGB 2:P3-D65 3:Custom (Edit L508))
OptionName = g_space_out
MinValue = 0
MaxValue = 3
StepAmount = 1
DefaultValue = 0

[OptionRangeInteger]
GUIName = White Point
OptionName = wp_temperature
MinValue = 5004
MaxValue = 12004
StepAmount = 50
DefaultValue = 8504

[OptionRangeFloat]
GUIName = CRT U/V Multiplier
OptionName = g_MUL
MinValue = 0.0, 0.0
MaxValue = 2.0, 2.0
StepAmount = 0.01, 0.01
DefaultValue = 0.9, 0.9

[OptionRangeFloat]
GUIName = CRT Gamma
OptionName = g_CRT_l
MinValue = 2.30
MaxValue = 2.60
StepAmount = 0.05
DefaultValue = 2.50

[OptionBool]
GUIName = Dark to Dim adaptation
OptionName = g_Dark_to_Dim
DefaultValue = true

[OptionBool]
GUIName = Gamut Compression
OptionName = g_GCompress
DefaultValue = true

[OptionRangeFloat]
GUIName = CRT Beam (Red, Gren, Blue)
OptionName = g_CRT
MinValue = 0.0, 0.0, 0.0
MaxValue = 1.2, 1.2, 1.2
StepAmount = 0.1, 0.1, 0.1
DefaultValue = 1.0, 1.0, 1.0

[/configuration]
*/


#define g_crtgamut GetOption(g_crtgamut)
#define g_space_out GetOption(g_space_out)

#define g_U_MUL GetOption(g_MUL.x)
#define g_V_MUL GetOption(g_MUL.y)

#define g_CRT_br GetOption(g_CRT.x)
#define g_CRT_bg GetOption(g_CRT.y)
#define g_CRT_bb GetOption(g_CRT.z)

// D65 Reference White
#define RW float3(0.950457397565471, 1.0, 1.089436035930324)
#define M_PI 3.1415926535897932384626433832795/180.0
#define g_bl -(100000.*log((72981.-500000./(3.*max(2.3,GetOption(g_CRT_l))))/9058.))/945461.




///////////////////////// Color Space Transformations //////////////////////////


mat3 RGB_to_XYZ_mat(mat3 primaries) {

    float3 T = RW * inverse(primaries);

    mat3  TB = mat3(
                 T.x, 0.0, 0.0,
                 0.0, T.y, 0.0,
                 0.0, 0.0, T.z);

    return TB * primaries;
 }



///////////////////////// White Point Mapping /////////////////////////
//
//
// PAL: D65        NTSC-U: D65       NTSC-J: CCT 9300K+27MPCD
// PAL: 6503.512K  NTSC-U: 6503.512K NTSC-J: ~8945.436K
// [x:0.31266142   y:0.3289589]      [x:0.281 y:0.311]

// For NTSC-J there's not a common agreed value, measured consumer units span from 8229.87K to 8945.623K with accounts for 8800K as well.
// Recently it's been standardized to 9300K which is closer to what master monitors (and not consumer units) were (x=0.2838 y=0.2984) (~9177.98K)

// "RGB to XYZ -> Temperature -> XYZ to RGB" joint matrix
float3 wp_adjust(float3 RGB, float temperature, mat3 primaries, mat3 display) {

    float temp3 = 1000.       /     temperature;
    float temp6 = 1000000.    / pow(temperature, 2.);
    float temp9 = 1000000000. / pow(temperature, 3.);

    float3 wp = float3(1.);

    wp.x = (temperature < 5500.) ? 0.244058 + 0.0989971 * temp3 + 2.96545 * temp6 - 4.59673 * temp9 : \
           (temperature < 8000.) ? 0.200033 + 0.9545630 * temp3 - 2.53169 * temp6 + 7.08578 * temp9 : \
                                   0.237045 + 0.2437440 * temp3 + 1.94062 * temp6 - 2.11004 * temp9 ;

    wp.y = -0.275275 + 2.87396 * wp.x - 3.02034 * pow(wp.x,2) + 0.0297408 * pow(wp.x,3);
    wp.z = 1. - wp.x - wp.y;

    const mat3 CAT16 = mat3(
     0.401288,-0.250268, -0.002079,
     0.650173, 1.204414,  0.048952,
    -0.051461, 0.045854,  0.953127);

    float3 VKV = (float3(wp.x/wp.y,1.,wp.z/wp.y) * CAT16) / (RW * CAT16);

    mat3 VK = mat3(
                VKV.x, 0.0, 0.0,
                0.0, VKV.y, 0.0,
                0.0, 0.0, VKV.z);

    mat3 CAM  = CAT16 * (VK * inverse(CAT16));

    mat3 mata = RGB_to_XYZ_mat(primaries);
    mat3 matb = RGB_to_XYZ_mat(display);

    return RGB.rgb * ((mata * CAM) * inverse(matb));
 }


////////////////////////////////////////////////////////////////////////////////


// CRT EOTF Function
//----------------------------------------------------------------------

float EOTF_1886a(float color, float bl, float brightness, float contrast) {

    // Defaults:
    //  Black Level = 0.1
    //  Brightness  = 0
    //  Contrast    = 100

    const float wl = 100.0;
          float b  = pow(bl, 1/2.4);
          float a  = pow(wl, 1/2.4)-b;
                b  = (brightness-50) / 250. + b/a;                 // -0.20 to +0.20
                a  = contrast!=50 ? pow(2,(contrast-50)/50.) : 1.; //  0.50 to +2.00

    const float Vc = 0.35;                           // Offset
          float Lw =   wl/100. * a;                  // White level
          float Lb = clamp( b  * a,0.0,Vc);          // Black level
    const float a1 = 2.6;                            // Shoulder gamma
    const float a2 = 3.0;                            // Knee gamma
          float k  = Lw /pow(1  + Lb,    a1);
          float sl = k * pow(Vc + Lb, a1-a2);        // Slope for knee gamma

    color = color >= Vc ? k * pow(color + Lb, a1 ) : sl * pow(color + Lb, a2 );
    return color;
 }

float3 EOTF_1886a_f3( float3 color, float BlackLevel, float brightness, float contrast) {

    color.r = EOTF_1886a( color.r, BlackLevel, brightness, contrast);
    color.g = EOTF_1886a( color.g, BlackLevel, brightness, contrast);
    color.b = EOTF_1886a( color.b, BlackLevel, brightness, contrast);
    return color.rgb;
 }



// Monitor Curve Functions: https://github.com/ampas/aces-dev
//----------------------------------------------------------------------


float moncurve_r( float color, float gamma, float offs) {

    // Reverse monitor curve
    color = clamp(color, 0.0, 1.0);
    float yb = pow( offs * gamma / ( ( gamma - 1.0) * ( 1.0 + offs)), gamma);
    float rs = pow( ( gamma - 1.0) / offs, gamma - 1.0) * pow( ( 1.0 + offs) / gamma, gamma);

    color = ( color > yb) ? ( 1.0 + offs) * pow( color, 1.0 / gamma) - offs : color * rs;
    return color;
 }


float3 moncurve_r_f3( float3 color, float gamma, float offs) {

    color.r = moncurve_r( color.r, gamma, offs);
    color.g = moncurve_r( color.g, gamma, offs);
    color.b = moncurve_r( color.b, gamma, offs);
    return color.rgb;
 }


//---------------------- Gamut Compression -------------------


// RGB 'Desaturate' Gamut Compression (by Jed Smith: https://github.com/jedypod/gamut-compress)
float3 GamutCompression (float3 rgb, float grey) {

    // Limit/Thres order is Cyan, Magenta, Yellow
    float temp = max(0,abs(GetOption(wp_temperature)-7000)-1000)/825.0+1; // center at 1
    float3 sat = GetOption(wp_temperature) < 7000 ? float3(1,temp,(temp-1)/2+1) : float3((temp-1)/2+1,temp,1);

    mat2x3 LimThres =      mat2x3( 0.100000,0.100000,0.100000,
                                   0.125000,0.125000,0.125000);
    if (g_space_out<2.0) {

       LimThres =
       g_crtgamut == 3.0 ? mat2x3( 0.000000,0.044065,0.000000,
                                   0.000000,0.095638,0.000000) : \
       g_crtgamut == 2.0 ? mat2x3( 0.006910,0.092133,0.000000,
                                   0.039836,0.121390,0.000000) : \
       g_crtgamut == 1.0 ? mat2x3( 0.018083,0.059489,0.017911,
                                   0.066570,0.105996,0.066276) : \
       g_crtgamut ==-1.0 ? mat2x3( 0.014947,0.098571,0.017911,
                                   0.060803,0.123793,0.066276) : \
       g_crtgamut ==-2.0 ? mat2x3( 0.004073,0.030307,0.012697,
                                   0.028222,0.083075,0.056029) : \
       g_crtgamut ==-3.0 ? mat2x3( 0.018424,0.053469,0.016841,
                                   0.067146,0.102294,0.064393) : LimThres;
    } else if (g_space_out==2.0) {

       LimThres =
       g_crtgamut == 3.0 ? mat2x3( 0.000000,0.234229,0.007680,
                                   0.000000,0.154983,0.042446) : \
       g_crtgamut == 2.0 ? mat2x3( 0.078526,0.108432,0.006143,
                                   0.115731,0.127194,0.037039) : \
       g_crtgamut == 1.0 ? mat2x3( 0.021531,0.237184,0.013466,
                                   0.072018,0.155438,0.057731) : \
       g_crtgamut ==-1.0 ? mat2x3( 0.051640,0.103332,0.013550,
                                   0.101092,0.125474,0.057912) : \
       g_crtgamut ==-2.0 ? mat2x3( 0.032717,0.525361,0.023928,
                                   0.085609,0.184491,0.075381) : \
       g_crtgamut ==-3.0 ? mat2x3( 0.000000,0.377522,0.043076,
                                   0.000000,0.172390,0.094873) : LimThres;
    }

    // Amount of outer gamut to affect
    float3 th = 1.0-float3(LimThres[1])*(0.4*sat+0.3);

    // Distance limit: How far beyond the gamut boundary to compress
    float3 dl = 1.0+float3(LimThres[0])*sat;

    // Calculate scale so compression function passes through distance limit: (x=dl, y=1)
    float3 s = (float3(1.0)-th)/sqrt(max(float3(1.001), dl)-1.0);

    // Achromatic axis
    float ac = max(rgb.x, max(rgb.y, rgb.z));

    // Inverse RGB Ratios: distance from achromatic axis
    float3 d = ac==0.0?float3(0.0):(ac-rgb)/abs(ac);

    // Compressed distance. Parabolic compression function: https://www.desmos.com/calculator/nvhp63hmtj
    float3 cd;
    float3 ss = s*s/4.0;
    float3 sf = s*sqrt(d-th+ss)-s*sqrt(ss)+th;
    cd.x = (d.x < th.x) ? d.x : sf.x;
    cd.y = (d.y < th.y) ? d.y : sf.y;
    cd.z = (d.z < th.z) ? d.z : sf.z;

    // Inverse RGB Ratios to RGB
    // and Mask with "luma"
    return mix(rgb, ac-cd.xyz*abs(ac), pow(grey,1.0/GetOption(g_CRT_l)));
    }



//*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/



// Matrices in column-major


//----------------------- Y'UV color model -----------------------


//  0-235 YUV PAL
//  0-235 YUV NTSC-J
// 16-235 YUV NTSC


// Bymax 0.885515
// Rymax 0.701088
// R'G'B' full range to Decorrelated Intermediate (Y,B-Y,R-Y)
// Rows should sum to 0, except first one which sums 1
const mat3 YByRy = mat3(
    0.298912, 0.586603, 0.114485,
   -0.298912,-0.586603, 0.885515,
    0.701088,-0.586603,-0.114485);


// Umax 0.435812284313725
// Vmax 0.615857694117647
// R'G'B' full to Y'UV limited
// YUV is defined with headroom and footroom (TV range),
// UV excursion is limited to Umax and Vmax
// Y  excursion is limited to 16-235 for NTSC-U and 0-235 for PAL and NTSC-J
float3 r601_YUV(float3 RGB, float NTSC_U) {

    const float sclU = ((0.5*(235-16)+16)/255.); // This yields Luma   grey  at around 0.49216 or 125.5 in 8-bit
    const float sclV =       (240-16)    /255. ; // This yields Chroma range at around 0.87843 or 224   in 8-bit

    mat3 conv_mat = mat3(
                   float3(YByRy[0]),
    float3(sclU) * float3(YByRy[1]),
    float3(sclV) * float3(YByRy[2]));

// -0.147111592156863  -0.288700692156863   0.435812284313725
//  0.615857694117647  -0.515290478431373  -0.100567215686275

    float3 YUV   = RGB.rgb * conv_mat;
           YUV.x = NTSC_U==1.0 ? YUV.x * 219.0 + 16.0 : YUV.x * 235.0;
    return float3(YUV.x/255.0,YUV.yz);
 }


// Y'UV limited to R'G'B' full
float3 YUV_r601(float3 YUV, float NTSC_U) {

const mat3 conv_mat = mat3(
    1.0000000, -0.000000029378826483,  1.1383928060531616,
    1.0000000, -0.396552562713623050, -0.5800843834877014,
    1.0000000,  2.031872510910034000,  0.0000000000000000);

    YUV.x = (YUV.x - (NTSC_U == 1.0 ? 16.0/255.0 : 0.0 )) * (255.0/(NTSC_U == 1.0 ? 219.0 : 235.0));
    return YUV.xyz * conv_mat;
 }


// FP32 to 8-bit mid-tread uniform quantization
float Quantize8(float col) {
    col = min(255.0,floor(col * 255.0 + 0.5));
    return col;
 }

float3 Quantize8_f3(float3 col) {
    col.r = Quantize8(col.r);
    col.g = Quantize8(col.g);
    col.b = Quantize8(col.b);
    return col.rgb;
 }




//*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/


//----------------------- Phosphor Gamuts -----------------------

////// STANDARDS ///////
// SMPTE RP 145-1994 (SMPTE-C), 170M-1999
// SMPTE-C - Standard Phosphor (Rec.601 NTSC)
// ILLUMINANT: D65->[0.31266142,0.3289589]
const mat3 SMPTE170M_ph = mat3(
     0.630, 0.310, 0.155,
     0.340, 0.595, 0.070,
     0.030, 0.095, 0.775);

// ITU-R BT.470/601 (B/G)
// EBU Tech.3213 PAL - Standard Phosphor for Studio Monitors
// ILLUMINANT: D65->[0.31266142,0.3289589]
const mat3 SMPTE470BG_ph = mat3(
     0.640, 0.290, 0.150,
     0.330, 0.600, 0.060,
     0.030, 0.110, 0.790);

// NTSC-J P22
// Mix between averaging KV-20M20, KDS VS19, Dell D93, 4-TR-B09v1_0.pdf and Phosphor Handbook 'P22'
// ILLUMINANT: D93->[0.281000,0.311000] (CCT of 8945.436K)
// ILLUMINANT: D97->[0.285000,0.285000] (CCT of 9696K) for Nanao MS-2930s series (around 10000.0K for wp_adjust() daylight fit)
const mat3 P22_J_ph = mat3(
     0.625, 0.280, 0.152,
     0.350, 0.605, 0.062,
     0.025, 0.115, 0.786);



////// P22 ///////
// You can run any of these P22 primaries either through D65 or D93 indistinctly but typically these were D65 based.
// P22_80 is roughly the same as the old P22 gamut in Grade 2020. P22 1979-1994 meta measurement.
// ILLUMINANT: D65->[0.31266142,0.3289589]
const mat3 P22_80s_ph = mat3(
     0.6470, 0.2820, 0.1472,
     0.3430, 0.6200, 0.0642,
     0.0100, 0.0980, 0.7886);

// P22 improved with tinted phosphors (Use this for NTSC-U 16-bits, and above for 8-bits)
const mat3 P22_90s_ph = mat3(
     0.6661, 0.3134, 0.1472,
     0.3329, 0.6310, 0.0642,
     0.0010, 0.0556, 0.7886);

// RPTV (Rear Projection TV) for NTSC-U late 90s, early 00s
const mat3 RPTV_95s_ph = mat3(
     0.640, 0.341, 0.150,
     0.335, 0.586, 0.070,
     0.025, 0.073, 0.780);


//*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/


//----------------------- Display Primaries -----------------------

// sRGB (IEC 61966-2-1) and ITU-R BT.709-6 (originally CCIR Rec.709)
const mat3 sRGB_prims = mat3(
     0.640, 0.300, 0.150,
     0.330, 0.600, 0.060,
     0.030, 0.100, 0.790);

// SMPTE RP 432-2 (DCI-P3)
const mat3 DCIP3_prims = mat3(
     0.680, 0.265, 0.150,
     0.320, 0.690, 0.060,
     0.000, 0.045, 0.790);

// Custom - Add here the primaries of your D65 calibrated display to -partially- color manage Dolphin. Only the matrix part (hue+saturation, gamma is left out)
// How: Check the log of DisplayCAL calibration/profiling, search where it says "Increasing saturation of actual primaries..."
// Note down in vertical order (column-major) the R xy, G xy and B xy values before "->" mark
// For full Dolphin color management, alongside the custom matrix you should also have DisplayCAL Profile Loader enabled...
// ...since it will also load the VCGT/LUT part -white point, grey balance and tone response-)
const mat3 Custom_prims = mat3(
     1.000, 0.000, 0.000,
     0.000, 1.000, 0.000,
     0.000, 0.000, 1.000);




//*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/




void main()
{

    float4    c0  = Sample();
    float3    src = c0.rgb;

// Clipping Logic / Gamut Limiting
    bool   NTSC_U = g_crtgamut < 2.0;

    float2 UVmax  = float2(Quantize8(0.435812284313725), Quantize8(0.615857694117647));
    float2 Ymax   = NTSC_U ? float2(16.0, 235.0) : float2(0.0, 235.0);


// Assumes framebuffer in Rec.601 full range with baked gamma
// Quantize to 8-bit to replicate CRT's circuit board arithmetics
    float3 col    = clamp(Quantize8_f3(r601_YUV(src, NTSC_U ? 1.0 : 0.0)), float3(Ymax.x,  -UVmax.x, -UVmax.y),      \
                                                                           float3(Ymax.y,   UVmax.x,  UVmax.y))/255.0;

// YUV Analogue Color Controls (Color Burst)
    float hue_radians = 0 * M_PI;
    float    hue  = atan(col.z,  col.y) + hue_radians;
    float chroma  = sqrt(col.z * col.z  + col.y * col.y);  // Euclidean Distance
    col.yz        = float2(chroma * cos(hue), chroma * sin(hue)) * float2(g_U_MUL,g_V_MUL);

// Back to R'G'B' full
    col   = OptionEnabled(g_signal_type) ? max(Quantize8_f3(YUV_r601(col.xyz, NTSC_U ? 1.0 : 0.0))/255.0, 0.0) : src;

// CRT EOTF. To Display Referred Linear: Undo developer baked CRT gamma (from 2.40 at default 0.1 CRT black level, to 2.60 at 0.0 CRT black level)
    col   = EOTF_1886a_f3(col, g_bl, 50, 50);


//_   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _   _
// \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \


// CRT Phosphor Gamut (0.0 is sRGB/noop)
    mat3 m_in;

    if (g_crtgamut  == -3.0) { m_in = SMPTE170M_ph;    } else
    if (g_crtgamut  == -2.0) { m_in = RPTV_95s_ph;     } else
    if (g_crtgamut  == -1.0) { m_in = P22_80s_ph;      } else
    if (g_crtgamut  ==  1.0) { m_in = P22_90s_ph;      } else
    if (g_crtgamut  ==  2.0) { m_in = P22_J_ph;        } else
    if (g_crtgamut  ==  3.0) { m_in = SMPTE470BG_ph;   } else
                             { m_in = sRGB_prims;      }


// Display color space
    mat3 m_ou;

    if (g_space_out ==  3.0) { m_ou = Custom_prims;    } else
    if (g_space_out ==  2.0) { m_ou = DCIP3_prims;     } else
                             { m_ou = sRGB_prims;      }


// White Point Mapping
    float3 src_h  = wp_adjust(col, GetOption(wp_temperature), m_in, m_ou);
           src_h *= float3(g_CRT_br,g_CRT_bg,g_CRT_bb);


// RGB 'Desaturate' Gamut Compression (by Jed Smith: https://github.com/jedypod/gamut-compress)
    float3 coeff = RGB_to_XYZ_mat(m_ou)[1];
    src_h = OptionEnabled(g_GCompress) ? clamp(GamutCompression(src_h, dot(coeff.xyz, src_h)), 0.0, 1.0) : clamp(src_h, 0.0, 1.0);


// Dark to Dim adaptation OOTF
    float  DtD = OptionEnabled(g_Dark_to_Dim) ? 1/0.9811 : 1.0;

// EOTF^-1 - Inverted Electro-Optical Transfer Function
    float3 TRC = (g_space_out == 1.0) ? moncurve_r_f3(src_h,             2.20 + 0.20,         0.0550) : \
                                        clamp(  pow(  src_h, float3(1./((2.20 + 0.20)*DtD))), 0., 1.) ;

    SetOutput(float4(TRC, c0.a));
}
