/*
[configuration]

[OptionRangeFloat]
GUIName = Amplificiation
OptionName = AMPLIFICATION
MinValue = 1.0
MaxValue = 6.0
StepAmount = 0.25
DefaultValue = 2.5

[OptionRangeFloat]
GUIName = Desaturation
OptionName = DESATURATION
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 0.0

[/configuration]
*/

/***** Linear <--> Oklab *****/

const mat4 RGBtoLMS = mat4(
    0.4122214708, 0.2119034982, 0.0883024619, 0.0000000000,
    0.5363325363, 0.6806995451, 0.2817188376, 0.0000000000,
    0.0514459929, 0.1073969566, 0.6299787005, 0.0000000000,
    0.0000000000, 0.0000000000, 0.0000000000, 1.0000000000);

const mat4 LMStoOklab = mat4(
    0.2104542553, 1.9779984951, 0.0259040371, 0.0000000000,
    0.7936177850, -2.4285922050, 0.7827717662, 0.0000000000,
    -0.0040720468, 0.4505937099, -0.8086757660, 0.0000000000,
    0.0000000000, 0.0000000000, 0.0000000000, 1.0000000000);

float4 LinearRGBToOklab(float4 c)
{
    return LMStoOklab * pow(RGBtoLMS * c, float4(1.0 / 3.0));
}

const mat4 OklabtoLMS = mat4(
    1.0000000000, 1.0000000000, 1.0000000000, 0.0000000000,
    0.3963377774, -0.1055613458, -0.0894841775, 0.0000000000,
    0.2158037573, -0.0638541728, -1.2914855480, 0.0000000000,
    0.0000000000, 0.0000000000, 0.0000000000, 1.0000000000);

const mat4 LMStoRGB = mat4(
    4.0767416621, -1.2684380046, -0.0041960863, 0.0000000000,
    -3.3077115913, 2.6097574011, -0.7034186147, 0.0000000000,
    0.2309699292, -0.3413193965, 1.7076147010, 0.0000000000,
    0.0000000000, 0.0000000000, 0.0000000000, 1.0000000000);

float4 OklabToLinearRGB(float4 c)
{
    return max(LMStoRGB * pow(OklabtoLMS * c, float4(3.0)), 0.0);
}

void main()
{
    float4 color = Sample();

    // Nothing to do here, we are in SDR
    if (!OptionEnabled(hdr_output) || !OptionEnabled(linear_space_output)) {
        SetOutput(color);
        return;
    }

    // Renormalize Color to be in SDR Space
    const float hdr_paper_white = hdr_paper_white_nits / hdr_sdr_white_nits;
    color.rgb /= hdr_paper_white;

    // Convert Color to Oklab (previous conditions garuntee color is linear)
    float4 oklab_color = LinearRGBToOklab(color);

    // Amount to raise hdr_paper_white to the power of.
    // We divide by 3 because Oklab is a cubic space, this accounts for that.
    float lum_pow = pow(oklab_color.x, 1.0) / 3.0;
    float sat_pow = pow(oklab_color.x, DESATURATION) / 3.0;

    // The reason we raise hdr_paper_white to a power is so that at low
    // luminosities, very little about the colors / brightnesses change.
    // However at luminosities of 1.0, the colors and brightnesses are
    // able to reach the full range of hdr_paper_white.

    // This is the key to PerceptualHDR working.
    oklab_color.x *= pow(AMPLIFICATION, lum_pow);
    oklab_color.z *= pow(AMPLIFICATION, sat_pow);
    oklab_color.y *= pow(AMPLIFICATION, sat_pow);

    SetOutput(hdr_paper_white * OklabToLinearRGB(oklab_color));
}
