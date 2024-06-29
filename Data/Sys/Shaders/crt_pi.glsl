/*
[configuration]

[OptionRangeFloat]
GUIName = Bloom factor
OptionName = BLOOM_FACTOR
MaxValue = 5.0
MinValue = 0.0
DefaultValue = 1.5
StepAmount = 0.01

[OptionBool]
GUIName = Sharper
OptionName = SHARPER
DefaultValue = false

[OptionBool]
GUIName = Curvature
OptionName = CURVATURE
DefaultValue = true

[OptionRangeFloat]
GUIName = Horizontal
OptionName = CURVATURE_X
DependentOption = CURVATURE
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.10
StepAmount = 0.01

[OptionRangeFloat]
GUIName = Vertical
OptionName = CURVATURE_Y
DependentOption = CURVATURE
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.15
StepAmount = 0.01

[OptionBool]
GUIName = Scanlines
OptionName = SCANLINES
DefaultValue = true

[OptionRangeFloat]
GUIName = Weight
OptionName = SCANLINE_WEIGHT
DependentOption = SCANLINES
MaxValue = 15.0
MinValue = 0.0
DefaultValue = 6.0
StepAmount = 0.1

[OptionRangeFloat]
GUIName = Gap brightness
OptionName = SCANLINE_GAP_BRIGHTNESS
DependentOption = SCANLINES
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.12
StepAmount = 0.01

[OptionBool]
GUIName = Multisample
OptionName = MULTISAMPLE
DependentOption = SCANLINES
DefaultValue = true

[OptionBool]
GUIName = Gamma
OptionName = GAMMA
DefaultValue = true

[OptionBool]
GUIName = Fake Gamma
OptionName = FAKE_GAMMA
DependentOption = GAMMA
DefaultValue = false

[OptionRangeFloat]
GUIName = Input
OptionName = INPUT_GAMMA
DependentOption = GAMMA
MaxValue = 5.0
MinValue = 0.0
DefaultValue = 2.4
StepAmount = 0.01

[OptionRangeFloat]
GUIName = Output
OptionName = OUTPUT_GAMMA
DependentOption = GAMMA
MaxValue = 5.0
MinValue = 0.0
DefaultValue = 2.2
StepAmount = 0.01

[OptionBool]
GUIName = Shadow mask
OptionName = SHADOW_MASK
DefaultValue = true

[OptionBool]
GUIName = Trinitron(ish)
OptionName = MASK_TRINITRON
DependentOption = SHADOW_MASK
DefaultValue = true

[OptionRangeFloat]
GUIName = Brightness
OptionName = MASK_BRIGHTNESS
DependentOption = SHADOW_MASK
MaxValue = 1.0
MinValue = 0.0
DefaultValue = 0.70
StepAmount = 0.01

[/configuration]
*/

/*
    crt-pi - A Raspberry Pi friendly CRT shader.
    Copyright (C) 2015-2016 davej
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

Notes:
This shader is designed to work well on Raspberry Pi GPUs (i.e. 1080P @ 60Hz on
a game with a 4:3 aspect ratio). 
It pushes the Pi's GPU hard and enabling some features will slow it down so that
it is no longer able to match 1080P @ 60Hz. 
You will need to overclock your Pi to the fastest setting in raspi-config to get
the best results from this shader: 'Pi2' for Pi2 and 'Turbo' for original Pi and
Pi Zero. 
Note: Pi2s are slower at running the shader than other Pis, this seems to be
down to Pi2s lower maximum memory speed. 
Pi2s don't quite manage 1080P @ 60Hz - they drop about 1 in 1000 frames. 
You probably won't notice this, but if you do, try enabling FAKE_GAMMA.
SCANLINES enables scanlines. 
You'll almost certainly want to use it with MULTISAMPLE to reduce moire effects. 
SCANLINE_WEIGHT defines how wide scanlines are (it is an inverse value so a
higher number = thinner lines). 
SCANLINE_GAP_BRIGHTNESS defines how dark the gaps between the scan lines are. 
Darker gaps between scan lines make moire effects more likely.
GAMMA enables gamma correction using the values in INPUT_GAMMA and OUTPUT_GAMMA.
FAKE_GAMMA causes it to ignore the values in INPUT_GAMMA and OUTPUT_GAMMA and 
approximate gamma correction in a way which is faster than true gamma whilst 
still looking better than having none. 
You must have GAMMA defined to enable FAKE_GAMMA.
CURVATURE distorts the screen by CURVATURE_X and CURVATURE_Y. 
Curvature slows things down a lot.
By default the shader uses linear blending horizontally. If you find this too
blury, enable SHARPER.
BLOOM_FACTOR controls the increase in width for bright scanlines.
MASK_TYPE defines what, if any, shadow mask to use. MASK_BRIGHTNESS defines how
much the mask type darkens the screen.
[MASK_TYPE has been replaced with SHADOW_MASK and MASK_TRINITRON]
*/

vec2 CURVATURE_DISTORTION = vec2(GetOption(CURVATURE_X), GetOption(CURVATURE_Y));
// Barrel distortion shrinks the display area a bit, this will allow us to counteract that.
vec2 barrelScale = 1.0 - (0.23 * CURVATURE_DISTORTION);

vec2 Distort(vec2 coord)
{
//  coord *= screenScale; // not necessary in slang
    coord -= vec2(0.5);
    float rsq = coord.x * coord.x + coord.y * coord.y;
    coord += coord * (CURVATURE_DISTORTION * rsq);
    coord *= barrelScale;
    if (abs(coord.x) >= 0.5 || abs(coord.y) >= 0.5)
        coord = vec2(-1.0);     // If out of bounds, return an invalid value.
    else
    {
        coord += vec2(0.5);
//      coord /= screenScale; // not necessary in slang
    }

    return coord;
}

float CalcScanLineWeight(float dist)
{
    return max(1.0-dist*dist*GetOption(SCANLINE_WEIGHT), GetOption(SCANLINE_GAP_BRIGHTNESS));
}

float filterWidth;

float CalcScanLine(float dy)
{
    float scanLineWeight = CalcScanLineWeight(dy);
    if (OptionEnabled(MULTISAMPLE))
    {
        scanLineWeight += CalcScanLineWeight(dy - filterWidth);
        scanLineWeight += CalcScanLineWeight(dy + filterWidth);
        scanLineWeight *= 0.3333333;
    }
    return scanLineWeight;
}

void main()
{
    vec2 vTexCoord = GetCoordinates();
    vec4 OutputSize = vec4(GetWindowResolution(), GetInvWindowResolution());
    vec4 SourceSize = vec4(GetResolution(), GetInvResolution());
    filterWidth = (SourceSize.y * OutputSize.w) * 0.333333333;

    vec2 texcoord = vTexCoord;

    if (OptionEnabled(CURVATURE))
    {
        texcoord = Distort(texcoord);
        if (texcoord.x < 0.0) 
        {
            SetOutput(vec4(0.0));
            return;
        }
    }

    vec2 texcoordInPixels = texcoord * SourceSize.xy;
    vec2 tc;
    float scanLineWeight;

    if (OptionEnabled(SHARPER))
    {
        vec2 tempCoord       = floor(texcoordInPixels) + 0.5;
        vec2 coord           = tempCoord * SourceSize.zw;
        vec2 deltas          = texcoordInPixels - tempCoord;
        scanLineWeight = CalcScanLine(deltas.y);
    
        vec2 signs = sign(deltas);
    
        deltas   = abs(deltas) * 2.0;
        deltas.x = deltas.x * deltas.x;
        deltas.y = deltas.y * deltas.y * deltas.y;
        deltas  *= 0.5 * SourceSize.zw * signs;

        tc = coord + deltas;
    }
    else
    {
        float tempCoord       = floor(texcoordInPixels.y) + 0.5;
        float coord           = tempCoord * SourceSize.w;
        float deltas          = texcoordInPixels.y - tempCoord;
        scanLineWeight  = CalcScanLine(deltas);
    
        float signs = sign(deltas);
    
        deltas   = abs(deltas) * 2.0;
        deltas   = deltas * deltas * deltas;
        deltas  *= 0.5 * SourceSize.w * signs;

        tc = vec2(texcoord.x, coord + deltas);
    }

    vec3 colour = SampleLocation(tc).rgb;

    if (OptionEnabled(SCANLINES))
    {
        if (OptionEnabled(GAMMA))
        {
            if (OptionEnabled(FAKE_GAMMA))
                colour = colour * colour;
            else
                colour = pow(colour, vec3(GetOption(INPUT_GAMMA)));
        }
    
        /* Apply scanlines */
        scanLineWeight *= GetOption(BLOOM_FACTOR);
        colour *= scanLineWeight;

        if (OptionEnabled(GAMMA))
        {
            if (OptionEnabled(FAKE_GAMMA))
                colour = sqrt(colour);
            else
                colour = pow(colour, vec3(1.0/GetOption(OUTPUT_GAMMA)));
        }
    }

    if (OptionEnabled(SHADOW_MASK))
    {
        if (OptionEnabled(MASK_TRINITRON))
        {
            /* Trinitron(ish) */
            float whichMask = fract((vTexCoord.x * OutputSize.x)  * 0.3333333);
            vec3 mask       = vec3(GetOption(MASK_BRIGHTNESS));
    
            if      (whichMask < 0.3333333) mask.r = 1.0;
            else if (whichMask < 0.6666666) mask.g = 1.0;
            else                            mask.b = 1.0;

            colour *= mask;
        }
        else
        {
            /* Green/magenta */
            float whichMask = fract((vTexCoord.x * OutputSize.x) * 0.5);
            vec3 mask       = vec3(1.0);

            if (whichMask < 0.5) mask.rb = vec2(GetOption(MASK_BRIGHTNESS));
            else                 mask.g  = GetOption(MASK_BRIGHTNESS);

            colour *= mask;
        }
    }

    SetOutput(vec4(colour, 1.0));
}
