// Based on https://github.com/Filoppi/PumboAutoHDR

/*
[configuration]

[OptionRangeFloat]
GUIName = HDR Peak White Nits
OptionName = HDR_PEAK_WHITE_NITS
MinValue = 80
MaxValue = 1000
StepAmount = 1
DefaultValue = 400

[OptionRangeFloat]
GUIName = Shoulder Pow
OptionName = AUTO_HDR_SHOULDER_POW
MinValue = 1
MaxValue = 10
StepAmount = 0.05
DefaultValue = 2.5

[OptionRangeFloat]
GUIName = Saturation Expansion
OptionName = AUTO_HDR_SATURATION_EXPANSION
MinValue = 0
MaxValue = 1
StepAmount = 0.01
DefaultValue = 0.2

[/configuration]
*/

float luminance(float3 color)
{
	return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

void main()
{
	float4 color = Sample();

	// Nothing to do here, we are in SDR
	if (!OptionEnabled(hdr_output) || !OptionEnabled(linear_space_output))
	{
		SetOutput(color);
		return;
	}

	const float hdr_paper_white = hdr_paper_white_nits / hdr_sdr_white_nits;
	const float auto_hdr_max_white = clamp(HDR_PEAK_WHITE_NITS / hdr_paper_white, hdr_sdr_white_nits, peak_white_nits / hdr_paper_white) / hdr_sdr_white_nits;

	// Restore the original SDR (0-1) brightness (we might or might not restore it later)
	color.rgb /= hdr_paper_white;

	// First do the HDR extrapolation on luminance (hue conservative)
	float sdr_ratio = luminance(color.rgb);
	const float auto_hdr_extra_ratio = pow(clamp(sdr_ratio, 0.0, 1.0), AUTO_HDR_SHOULDER_POW) * (auto_hdr_max_white - 1.0);
	const float auto_hdr_total_ratio = sdr_ratio + auto_hdr_extra_ratio;
	float single_color_scale = (sdr_ratio > 0.0) ? (auto_hdr_total_ratio / sdr_ratio) : 1.0;

	// Then calculate it again but with "per channel", which would expand gamut (not hue conservative)
	float3 sdr_ratio_3 = color.rgb;
	float3 auto_hdr_extra_ratio_3 = pow(clamp(sdr_ratio_3, 0.0, 1.0), AUTO_HDR_SHOULDER_POW) * (auto_hdr_max_white - 1.f);
	float3 auto_hdr_total_ratio_3 = sdr_ratio_3 + auto_hdr_extra_ratio_3;
	float3 per_channel_color_scale = (sdr_ratio_3 > 0.0) ? (auto_hdr_total_ratio_3 / sdr_ratio_3) : 1.0;

	color.rgb *= lerp(single_color_scale.xxx, per_channel_color_scale, AUTO_HDR_SATURATION_EXPANSION.xxx);

	color.rgb *= hdr_paper_white;

	SetOutput(color);
}
