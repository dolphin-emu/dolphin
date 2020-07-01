/*
[configuration]
[OptionBool]
GUIName = Phosphor Effect
GUIName.BRA = Efeito de Fósforo do Tubo CRT
OptionName = PHOSPHOR
DefaultValue = True
GUIDescription = A phosphor mask is applied over pixels.
GUIDescription.BRA = Uma máscara de fósforo é aplicada sobre os pixels.

[OptionRangeFloat]
GUIName = Encoded Gamma Input
GUIName.BRA = Gamma Codificado de Entrada
OptionName = InputGamma
MinValue = 1.0
MaxValue = 5.0
StepAmount = 0.1
DefaultValue = 2.5
GUIDescription = Value for input gamma. Common values are between 2.35 and 2.5.
GUIDescription.BRA = Valor para gamma de entrada. Valores comuns são entre 2.35 e 2.5.

[OptionRangeFloat]
GUIName = Encoded Gamma Output
GUIName.BRA = Gamma Codificado de Saída
OptionName = OutputGamma
MinValue = 1.0
MaxValue = 5.0
StepAmount = 0.1
DefaultValue = 2.2
GUIDescription = Value for output gamma.
GUIDescription.BRA = Valor para gamma de saída. Afeta as cores do jogo.

[OptionRangeInteger]
GUIName = Sharpness (Hack)
GUIName.BRA = Nitidez (Gambiarra)
OptionName = SHARPNESS
MinValue = 1
MaxValue = 5
StepAmount = 1
DefaultValue = 1
GUIDescription = Increase sharpness by hacking the texture sampling (hack).
GUIDescription.BRA = Aumenta a nitidez com uma gambiarra na amostragem de texturas.

[OptionRangeFloat]
GUIName = Color Boost
GUIName.BRA = Realce de Cor
OptionName = COLOR_BOOST
MinValue = 1.0
MaxValue = 3.0
StepAmount = 0.01
DefaultValue = 1.5
GUIDescription = Change this to adjust saturation.
GUIDescription.BRA = Altere o valor para ajustar a saturação das cores.

[OptionRangeFloat]
GUIName = Red Boost
GUIName.BRA = Realce do Vermelho
OptionName = RED_BOOST
MinValue = 1.0
MaxValue = 3.0
StepAmount = 0.01
DefaultValue = 1.0
GUIDescription = Change this to adjust red channel.
GUIDescription.BRA = Altere o valor para ajustar a saturação do vermelho.

[OptionRangeFloat]
GUIName = Green Boost
GUIName.BRA = Realce do Verde
OptionName = GREEN_BOOST
MinValue = 1.0
MaxValue = 3.0
StepAmount = 0.01
DefaultValue = 1.0
GUIDescription = Change this to adjust green channel.
GUIDescription.BRA = Altere o valor para ajustar a saturação do verde.

[OptionRangeFloat]
GUIName = Blue Boost
GUIName.BRA = Realce do Azul
OptionName = BLUE_BOOST
MinValue = 1.0
MaxValue = 3.0
StepAmount = 0.01
DefaultValue = 1.0
GUIDescription = Change this to adjust blue channel.
GUIDescription.BRA = Altere o valor para ajustar a saturação do azul.

[OptionRangeFloat]
GUIName = Scanlines Strength
GUIName.BRA = Intensidade das Scanlines
OptionName = SCANLINES_STRENGTH
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.72
GUIDescription = Change this to adjust the scanlines strength.
GUIDescription.BRA = Altere o valor para ajustar a intensidade das scanlines.

[OptionRangeFloat]
GUIName = Minimum Beam Width
GUIName.BRA = Mínima Largura do Feixe de Elétrons
OptionName = BEAM_MIN_WIDTH
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.86
GUIDescription = Set the minimum beam width.
GUIDescription.BRA = Altere o valor para ajustar a mínima largura do feixe de elétrons do CRT.

[OptionRangeFloat]
GUIName = Maximum Beam Width
GUIName.BRA = Máxima Largura do Feixe de Elétrons
OptionName = BEAM_MAX_WIDTH
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 1.0
GUIDescription = Set the maximum beam width.
GUIDescription.BRA = Altere o valor para ajustar a máxima largura do feixe de elétrons do CRT.

[OptionRangeFloat]
GUIName = Anti-Ringing
GUIName.BRA = Anti-Ringing
OptionName = CRT_ANTI_RINGING
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 0.8
GUIDescription = Set the anti-ringing strength.
GUIDescription.BRA = Altere o valor para ajustar a intensidade do anti-ringing.

[Pass]
Input0=ColorBuffer
Input0Mode=Clamp
Input0Filter=Nearest
EntryPoint=main
[/configuration]
*/

/*
   Hyllian's CRT Shader
  
   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com

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
#define PHOSPHOR 1.0
#define InputGamma 2.4
#define OutputGamma 2.2
#define SHARPNESS 1.0
#define COLOR_BOOST 1.5
#define RED_BOOST 1.0
#define GREEN_BOOST 1.0
#define BLUE_BOOST 1.0
#define SCANLINES_STRENGTH 0.72
#define BEAM_MIN_WIDTH 0.86
#define BEAM_MAX_WIDTH 1.0
#define CRT_ANTI_RINGING 0.8 
*/

#define GAMMA_IN(color)     pow(color, float3(GetOption(InputGamma), GetOption(InputGamma), GetOption(InputGamma)))
#define GAMMA_OUT(color)    pow(color, float3(1.0 / GetOption(OutputGamma), 1.0 / GetOption(OutputGamma), 1.0 / GetOption(OutputGamma)))

// Horizontal cubic filter.

// Some known filters use these values:

//    B = 0.0, C = 0.0  =>  Hermite cubic filter.
//    B = 1.0, C = 0.0  =>  Cubic B-Spline filter.
//    B = 0.0, C = 0.5  =>  Catmull-Rom Spline filter. This is the default used in this shader.
//    B = C = 1.0/3.0   =>  Mitchell-Netravali cubic filter.
//    B = 0.3782, C = 0.3109  =>  Robidoux filter.
//    B = 0.2620, C = 0.3690  =>  Robidoux Sharp filter.
//    B = 0.36, C = 0.28  =>  My best config for ringing elimination in pixel art (Hyllian).


// For more info, see: http://www.imagemagick.org/Usage/img_diagrams/cubic_survey.gif

// Change these params to configure the horizontal filter.
#define  B 0.0
#define  C 0.5  



void main()
{
    const float4x4 invX = float4x4(            (-B - 6.0*C)/6.0,         (3.0*B + 12.0*C)/6.0,     (-3.0*B - 6.0*C)/6.0,             B/6.0,
                                        (12.0 - 9.0*B - 6.0*C)/6.0, (-18.0 + 12.0*B + 6.0*C)/6.0,                      0.0, (6.0 - 2.0*B)/6.0,
                                       -(12.0 - 9.0*B - 6.0*C)/6.0, (18.0 - 15.0*B - 12.0*C)/6.0,      (3.0*B + 6.0*C)/6.0,             B/6.0,
                                                   (B + 6.0*C)/6.0,                           -C,                      0.0,               0.0);


    float2 texcoord = GetCoordinates();
    float2 texture_size = GetResolution();

    float2 TextureSize = float2(GetOption(SHARPNESS)*texture_size.x, texture_size.y);

    float3 color;
    float2 dx = float2(1.0/TextureSize.x, 0.0);
    float2 dy = float2(0.0, 1.0/TextureSize.y);
    float2 pix_coord = texcoord*TextureSize+float2(-0.5,0.5);

    float2 tc = (floor(pix_coord)+float2(0.5,0.5))/TextureSize;

    float2 fp = frac(pix_coord);

    float3 c00 = GAMMA_IN(SampleLocation(tc     - dx - dy).xyz);
    float3 c01 = GAMMA_IN(SampleLocation(tc          - dy).xyz);
    float3 c02 = GAMMA_IN(SampleLocation(tc     + dx - dy).xyz);
    float3 c03 = GAMMA_IN(SampleLocation(tc + 2.0*dx - dy).xyz);
    float3 c10 = GAMMA_IN(SampleLocation(tc     - dx).xyz);
    float3 c11 = GAMMA_IN(SampleLocation(tc         ).xyz);
    float3 c12 = GAMMA_IN(SampleLocation(tc     + dx).xyz);
    float3 c13 = GAMMA_IN(SampleLocation(tc + 2.0*dx).xyz);

    //  Get min/max samples
    float3 min_sample = min(min(c01,c11), min(c02,c12));
    float3 max_sample = max(max(c01,c11), max(c02,c12));

    float4x3 color_matrix0 = float4x3(c00, c01, c02, c03);
    float4x3 color_matrix1 = float4x3(c10, c11, c12, c13);

    float4 invX_Px  = mul(invX, float4(fp.x*fp.x*fp.x, fp.x*fp.x, fp.x, 1.0));
    float3 color0   = mul(invX_Px, color_matrix0);
    float3 color1   = mul(invX_Px, color_matrix1);

    // Anti-ringing
    float3 aux = color0;
    color0 = clamp(color0, min_sample, max_sample);
    color0 = lerp(aux, color0, GetOption(CRT_ANTI_RINGING));
    aux = color1;
    color1 = clamp(color1, min_sample, max_sample);
    color1 = lerp(aux, color1, GetOption(CRT_ANTI_RINGING));

    float pos0 = fp.y;
    float pos1 = 1 - fp.y;

    float bminw = GetOption(BEAM_MIN_WIDTH);
    float bmaxw = GetOption(BEAM_MAX_WIDTH);

    float3 lum0 = lerp(float3(bminw, bminw, bminw), float3(bmaxw, bmaxw, bmaxw), color0);
    float3 lum1 = lerp(float3(bminw, bminw, bminw), float3(bmaxw, bmaxw, bmaxw), color1);

    float3 d0 = clamp(pos0/(lum0+0.0000001), 0.0, 1.0);
    float3 d1 = clamp(pos1/(lum1+0.0000001), 0.0, 1.0);

    d0 = exp(-10.0*GetOption(SCANLINES_STRENGTH)*d0*d0);
    d1 = exp(-10.0*GetOption(SCANLINES_STRENGTH)*d1*d1);

    color = clamp(color0*d0+color1*d1, 0.0, 1.0);            

    color *= GetOption(COLOR_BOOST)*float3(GetOption(RED_BOOST), GetOption(GREEN_BOOST), GetOption(BLUE_BOOST));

    float mod_factor = texcoord.x * (GetTargetResolution()).x * texture_size.x / (GetInputResolution(0)).x;

    float3 dotMaskWeights = lerp(
                                 float3(1.0, 0.7, 1.0),
                                 float3(0.7, 1.0, 0.7),
                                 floor(fmod(mod_factor, 2.0))
                                  );

    color.rgb *= lerp(float3(1.0, 1.0, 1.0), dotMaskWeights, GetOption(PHOSPHOR));

    color  = GAMMA_OUT(color);

    SetOutput(float4(color, 1.0));
}

