// Anime4K-inspired Texture Upscaler
// Push-gradient edge refinement for sharp, clean upscaling
// Inspired by Anime4K's line/edge sharpening approach
// UpscaleFactor: 2

void main()
{
	float2 texCoord = GetCoordinates();
	float2 invRes = GetInvResolution();

	// Sample center and neighbors
	float4 C = SampleLocation(texCoord);

	float4 N  = SampleLocation(texCoord + float2( 0.0, -1.0) * invRes);
	float4 S  = SampleLocation(texCoord + float2( 0.0,  1.0) * invRes);
	float4 E  = SampleLocation(texCoord + float2( 1.0,  0.0) * invRes);
	float4 W  = SampleLocation(texCoord + float2(-1.0,  0.0) * invRes);

	float4 NW = SampleLocation(texCoord + float2(-1.0, -1.0) * invRes);
	float4 NE = SampleLocation(texCoord + float2( 1.0, -1.0) * invRes);
	float4 SW = SampleLocation(texCoord + float2(-1.0,  1.0) * invRes);
	float4 SE = SampleLocation(texCoord + float2( 1.0,  1.0) * invRes);

	// Compute luminance
	float3 luma = float3(0.299, 0.587, 0.114);
	float lumC  = dot(C.rgb,  luma);
	float lumN  = dot(N.rgb,  luma);
	float lumS  = dot(S.rgb,  luma);
	float lumE  = dot(E.rgb,  luma);
	float lumW  = dot(W.rgb,  luma);
	float lumNW = dot(NW.rgb, luma);
	float lumNE = dot(NE.rgb, luma);
	float lumSW = dot(SW.rgb, luma);
	float lumSE = dot(SE.rgb, luma);

	// Compute the luminance gradient (Sobel-like)
	float gx = (-lumNW - 2.0 * lumW - lumSW) + (lumNE + 2.0 * lumE + lumSE);
	float gy = (-lumNW - 2.0 * lumN - lumNE) + (lumSW + 2.0 * lumS + lumSE);

	// Gradient magnitude
	float grad_mag = sqrt(gx * gx + gy * gy);

	// Normalize gradient direction
	float2 grad_dir = float2(gx, gy);
	float grad_len = length(grad_dir);
	if (grad_len > 0.001)
		grad_dir = grad_dir / grad_len;

	// "Push" sharpening: sample along and against the gradient
	float push_strength = 0.5;
	float4 along  = SampleLocation(texCoord + grad_dir * invRes * push_strength);
	float4 against = SampleLocation(texCoord - grad_dir * invRes * push_strength);

	// Compute local min/max for clamping (prevents ringing)
	float4 local_min = min(min(min(N, S), min(E, W)), C);
	float4 local_max = max(max(max(N, S), max(E, W)), C);

	// Anime4K-style push: sharpen by moving away from the blurred direction
	float lumAlong = dot(along.rgb, luma);
	float lumAgainst = dot(against.rgb, luma);

	// The pixel should be "pushed" toward the side that makes the edge sharper
	float sharpen_amount = clamp(grad_mag * 2.0, 0.0, 1.0);

	float4 sharpened;
	if (abs(lumC - lumAlong) < abs(lumC - lumAgainst))
	{
		// Center is closer to "along" — push away from "against"
		sharpened = C + (C - against) * sharpen_amount * 0.4;
	}
	else
	{
		// Center is closer to "against" — push away from "along"
		sharpened = C + (C - along) * sharpen_amount * 0.4;
	}

	// Clamp to local neighborhood to prevent ringing artifacts
	sharpened = clamp(sharpened, local_min, local_max);

	// Blend based on edge strength — flat areas are untouched
	float blend = clamp(grad_mag * 4.0, 0.0, 1.0);
	SetOutput(lerp(C, sharpened, blend));
}
