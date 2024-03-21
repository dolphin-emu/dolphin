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

const float4 m_1 = float4(2610.0 / 16384.0);
const float4 m_2 = float4(128.0 * 2523.0 / 4096.0);
const float4 m_1_inv = float4(16384.0 / 2610.0);
const float4 m_2_inv = float4(4096.0 / (128.0 * 2523.0));

const float4 c_1 = float4(3424.0 / 4096.0);
const float4 c_2 = float4(2413.0 / 4096.0 * 32.0);
const float4 c_3 = float4(2392.0 / 4096.0 * 32.0);

float4 EOTF_inv(float4 lms) {
    float4 y = pow(lms, m_1);
    return pow((c_1 + c_2 * y) / (1.0 + c_3 * y), m_2);
}

float4 EOTF(float4 lms) {
    float4 x = pow(lms, m_2_inv);
    return pow(-(x - c_1) / (c_3 * x - c_2), m_1_inv);
}

// This is required as scaling in EOTF space is not linear.
float EOTF_AMPLIFICATION = EOTF_inv(float4(AMPLIFICATION)).x;

/***** Linear <--> ICtCp *****/

const mat4 RGBtoLMS = mat4(
    1688.0, 683.0,  99.0,   0.0,
    2146.0, 2951.0, 309.0,  0.0,
    262.0,  462.0,  3688.0, 0.0,
    0.0,    0.0,    0.0,    4096.0) / 4096.0;

const mat4 LMStoICtCp = mat4(
    +2048.0, +6610.0,  +17933.0, 0.0,
    +2048.0, -13613.0, -17390.0, 0.0,
    +0.0,    +7003.0,  -543.0,   0.0,
    +0.0,    +0.0,     +0.0,     4096.0) / 4096.0;

float4 LinearRGBToICtCP(float4 c)
{
    return LMStoICtCp * EOTF_inv(RGBtoLMS * c);
}

/***** ICtCp <--> Linear *****/

mat4 ICtCptoLMS = inverse(LMStoICtCp);
mat4 LMStoRGB = inverse(RGBtoLMS);

float4 ICtCpToLinearRGB(float4 c)
{
    return LMStoRGB * EOTF(ICtCptoLMS * c);
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
    // At low luminances, ~0.0, pow(EOTF_AMPLIFICATION, ~0.0) ~= 1.0, so the
    // color will appear to be unchanged. This is important as we don't want to
    // over expose dark colors which would not have otherwise been seen.
    //
    // At high luminances, ~1.0, pow(EOTF_AMPLIFICATION, ~1.0) ~= EOTF_AMPLIFICATION,
    // which is equivilant to scaling the color by EOTF_AMPLIFICATION. This is
    // important as we want to get the most out of the display, and we want to
    // get bright colors to hit their target brightness.
    //
    // For more information, see this desmos demonstrating this scaling process:
    // https://www.desmos.com/calculator/syjyrjsj5c
    const float luminance = ictcp_color.x;
    ictcp_color *= pow(EOTF_AMPLIFICATION, luminance);

    // Convert back to Linear RGB and output the color to the display.
    // We use hdr_paper_white to renormalize the color to the comfortable
    // SDR viewing range.
    SetOutput(hdr_paper_white * ICtCpToLinearRGB(ictcp_color));
}
