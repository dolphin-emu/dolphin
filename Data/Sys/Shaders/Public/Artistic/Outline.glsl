/**
 * Depth-buffer based cel shading for ENB by kingeric1992
 * http://enbseries.enbdev.com/forum/viewtopic.php?f=7&t=3244#p53168
 *
 * Modified and optimized for ReShade by JPulowski
 * http://reshade.me/forum/shader-presentation/261
 *
 * Do not distribute without giving credit to the original author(s).
 *
 * Ported to Dolphin by iwubcode (based off of 1.3)
 *
 */

float3 GetEdgeSample(float2 coord)
{
	if (GetOption(_EdgeDetectionMode) == 1)
	{
		float4 depth = float4(
			SampleDepthLocation(coord + GetInvResolution() * float2(1, 0)),
			SampleDepthLocation(coord - GetInvResolution() * float2(1, 0)),
			SampleDepthLocation(coord + GetInvResolution() * float2(0, 1)),
			SampleDepthLocation(coord - GetInvResolution() * float2(0, 1)));

		float2 depth_buffer_size = float2(SampleInputSize(DEPTH_BUFFER_INPUT_INDEX, 0));
		return normalize(float3(float2(depth.x - depth.y, depth.z - depth.w) * depth_buffer_size, 1.0));
	}
	else
	{
		return SampleLocation(coord).rgb;
	}
}

void main()
{
	float3 color = OptionEnabled(_UseBackgroundColor) ? GetOption(_BackgroundColor) : Sample().rgb;
	float3 origcolor = color;

	// Sobel operator matrices
	const float3 Gx[3] =
	{
		float3(-1.0, 0.0, 1.0),
		float3(-2.0, 0.0, 2.0),
		float3(-1.0, 0.0, 1.0)
	};
	const float3 Gy[3] =
	{
		float3( 1.0,  2.0,  1.0),
		float3( 0.0,  0.0,  0.0),
		float3(-1.0, -2.0, -1.0)
	};
	
	float3 dotx = float3(0.0, 0.0, 0.0), doty = float3(0.0, 0.0, 0.0);
	
	// Edge detection
	for (int i = 0, j; i < 3; i++)
	{
		j = i - 1;

		dotx += Gx[i].x * GetEdgeSample(GetCoordinates() + GetInvResolution() * float2(-1, j));
		dotx += Gx[i].y * GetEdgeSample(GetCoordinates() + GetInvResolution() * float2( 0, j));
		dotx += Gx[i].z * GetEdgeSample(GetCoordinates() + GetInvResolution() * float2( 1, j));
		
		doty += Gy[i].x * GetEdgeSample(GetCoordinates() + GetInvResolution() * float2(-1, j));
		doty += Gy[i].y * GetEdgeSample(GetCoordinates() + GetInvResolution() * float2( 0, j));
		doty += Gy[i].z * GetEdgeSample(GetCoordinates() + GetInvResolution() * float2( 1, j));
	}
	
	// Boost edge detection
	dotx *= GetOption(_EdgeDetectionAccuracy);
	doty *= GetOption(_EdgeDetectionAccuracy);

	// Return custom color when weight over threshold
	float threshold = float(sqrt(dot(dotx, dotx) + dot(doty, doty)) >= GetOption(_EdgeSlope));
	color = lerp(color, GetOption(_OutlineColor).rgb, threshold);
	
	// Set opacity
	color = lerp(origcolor, color, GetOption(_OutlineColor).a);
	
	SetOutput(float4(color, 1.0));
}
