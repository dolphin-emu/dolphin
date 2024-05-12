/*
[configuration]

[OptionRangeFloat]
GUIName = Amplificiation
OptionName = AMPLIFICATION
MinValue = 1.0
MaxValue = 6.0
StepAmount = 0.25
DefaultValue = 2.5

[/configuration]
*/

// ICtCP Colorspace as defined by Dolby here:
// https://professional.dolby.com/siteassets/pdfs/ictcp_dolbywhitepaper_v071.pdf

/***** Transfer Function *****/

const float a = 0.17883277;
const float b = 1.0 - 4.0 * a;
const float c = 0.5 - a * log(4.0 * a);

float HLG_f(float x)
{
    if (x < 0.0) {
        return 0.0;
    }

    else if (x < 1.0 / 12.0) {
        return sqrt(3.0 * x);
    }

    return a * log(12.0 * x - b) + c;
}

float HLG_inv_f(float x)
{
    if (x < 0.0) {
        return 0.0;
    }

    else if (x < 1.0 / 2.0) {
        return x * x / 3.0;
    }

    return (exp((x - c) / a) + b) / 12.0;
}

float4 HLG(float4 lms)
{
    return float4(HLG_f(lms.x), HLG_f(lms.y), HLG_f(lms.z), lms.w);
}

float4 HLG_inv(float4 lms)
{
    return float4(HLG_inv_f(lms.x), HLG_inv_f(lms.y), HLG_inv_f(lms.z), lms.w);
}

/***** Linear <--> ICtCp *****/

const mat4 RGBtoLMS = mat4(
                          1688.0, 683.0, 99.0, 0.0,
                          2146.0, 2951.0, 309.0, 0.0,
                          262.0, 462.0, 3688.0, 0.0,
                          0.0, 0.0, 0.0, 4096.0)
    / 4096.0;

const mat4 LMStoICtCp = mat4(
                            +2048.0, +3625.0, +9500.0, 0.0,
                            +2048.0, -7465.0, -9212.0, 0.0,
                            +0.0, +3840.0, -288.0, 0.0,
                            +0.0, +0.0, +0.0, 4096.0)
    / 4096.0;

float4 LinearRGBToICtCP(float4 c)
{
    return LMStoICtCp * HLG(RGBtoLMS * c);
}

/***** ICtCp <--> Linear *****/

mat4 ICtCptoLMS = inverse(LMStoICtCp);
mat4 LMStoRGB = inverse(RGBtoLMS);

float4 ICtCpToLinearRGB(float4 c)
{
    return LMStoRGB * HLG_inv(ICtCptoLMS * c);
}

void main()
{
    float4 color = Sample();

    // Nothing to do here, we are in SDR
    if (!OptionEnabled(hdr_output) || !OptionEnabled(linear_space_output)) {
        SetOutput(color);
        return;
    }

    // Renormalize Color to be in [0.0 - 1.0] SDR Space. We will revert this later.
    const float hdr_paper_white = hdr_paper_white_nits / hdr_sdr_white_nits;
    color.rgb /= hdr_paper_white;

    // Convert Color to Perceptual Color Space. This will allow us to do perceptual
    // scaling while also being able to use the luminance channel.
    float4 ictcp_color = LinearRGBToICtCP(color);

    // Scale the color in perceptual space depending on the percieved luminance.
    //
    // At low luminances, ~0.0, pow(AMPLIFICATION, ~0.0) ~= 1.0, so the
    // color will appear to be unchanged. This is important as we don't want to
    // over expose dark colors which would not have otherwise been seen.
    //
    // At high luminances, ~1.0, pow(AMPLIFICATION, ~1.0) ~= AMPLIFICATION,
    // which is equivilant to scaling the color by AMPLIFICATION. This is
    // important as we want to get the most out of the display, and we want to
    // get bright colors to hit their target brightness.
    //
    // For more information, see this desmos demonstrating this scaling process:
    // https://www.desmos.com/calculator/syjyrjsj5c
    float exposure = length(ictcp_color.xyz);
    ictcp_color *= pow(HLG_f(AMPLIFICATION), exposure);

    // Convert back to Linear RGB and output the color to the display.
    // We use hdr_paper_white to renormalize the color to the comfortable
    // SDR viewing range.
    SetOutput(hdr_paper_white * ICtCpToLinearRGB(ictcp_color));
}
