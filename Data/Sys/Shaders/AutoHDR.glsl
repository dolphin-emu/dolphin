// Based on https://github.com/Filoppi/PumboAutoHDR

/*
[configuration]

[OptionBool]
GUIName = Limit to Display Peak Luminance (DX11+)
OptionName = USE_DISPLAY_PEAK_LUMINANCE
DefaultValue = 0

[OptionRangeFloat]
GUIName = HDR Display Max Nits
OptionName = HDR_DISPLAY_MAX_NITS
MinValue = 80
MaxValue = 2000
StepAmount = 1
DefaultValue = 400

[OptionRangeFloat]
GUIName = Shoulder Start Alpha
OptionName = AUTO_HDR_SHOULDER_START_ALPHA
MinValue = 0
MaxValue = 1
StepAmount = 0.01
DefaultValue = 0

[OptionRangeFloat]
GUIName = Shoulder Pow
OptionName = AUTO_HDR_SHOULDER_POW
MinValue = 1
MaxValue = 10
StepAmount = 0.05
DefaultValue = 2.5

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

	// Restore the original SDR (0-1) brightness (we might or might not restore it later)
	color.rgb /= hdr_paper_white;

	// Find the color luminance (it works better than average)
	float sdr_ratio = luminance(color.rgb);

	// When "Limit to Display Peak Luminance" is enabled, clamp the user's target brightness to
	// the peak luminance reported by the display (only available on DirectX 11/12), preventing
	// over-expansion on displays that can't reach the slider value. Falls back to the slider on
	// other APIs or if no display data is available.
	const float display_max_nits = (OptionEnabled(USE_DISPLAY_PEAK_LUMINANCE) && hdr_max_luminance_nits > 0.0)
		? min(hdr_max_luminance_nits, HDR_DISPLAY_MAX_NITS)
		: HDR_DISPLAY_MAX_NITS;
	const float auto_hdr_max_white = max(display_max_nits / (hdr_paper_white_nits / hdr_sdr_white_nits), hdr_sdr_white_nits) / hdr_sdr_white_nits;
	if (sdr_ratio > AUTO_HDR_SHOULDER_START_ALPHA && AUTO_HDR_SHOULDER_START_ALPHA < 1.0)
	{
		const float auto_hdr_shoulder_ratio = 1.0 - (max(1.0 - sdr_ratio, 0.0) / (1.0 - AUTO_HDR_SHOULDER_START_ALPHA));
		const float auto_hdr_extra_ratio = pow(auto_hdr_shoulder_ratio, AUTO_HDR_SHOULDER_POW) * (auto_hdr_max_white - 1.0);
		const float auto_hdr_total_ratio = sdr_ratio + auto_hdr_extra_ratio;
		color.rgb *= auto_hdr_total_ratio / sdr_ratio;
	}

	color.rgb *= hdr_paper_white;

	SetOutput(color);
}
