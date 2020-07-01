/*
[configuration]

[OptionRangeFloat]
GUIName = Lens Center Offset
OptionName = u_lensCenterOffset
MinValue = -1.0, -1.0
MaxValue = 1.0, 1.0
StepAmount = 0.001, 0.001
DefaultValue = 0.0, 0.0

[OptionRangeFloat]
GUIName = Distortion
OptionName = u_distortion
MinValue = 0.0, 0.0, 0.0, 0.0
MaxValue = 1.0, 1.0, 1.0, 1.0
StepAmount = 0.001, 0.001, 0.001, 0.001
DefaultValue = 1.0, 0.22, 0.0, 0.24

[/configuration]
*/

//------------------------------------------------------------------------------
// Barrel Distortion
//------------------------------------------------------------------------------

float distortionScale(float2 offset) {
	// Note that this performs piecewise multiplication,
	// NOT a dot or cross product
	float2 offsetSquared = offset * offset;
	float radiusSquared = offsetSquared.x + offsetSquared.y;
	float4 distortion = GetOption(u_distortion);
	float distortionScale = //
		distortion.x + //
		distortion.y * radiusSquared + //
		distortion.z * radiusSquared * radiusSquared + //
		distortion.w * radiusSquared * radiusSquared * radiusSquared;
	return distortionScale;
}

float2 textureCoordsToDistortionOffsetCoords(float2 texCoord) {
	// Convert the texture coordinates from "0 to 1" to "-1 to 1"
	float2 result = texCoord * 2.0 - 1.0;

	// Convert from using the center of the screen as the origin to
	// using the lens center as the origin
	result -= GetOption(u_lensCenterOffset);

	// Correct for the aspect ratio
	result.y *= GetInvResolution().x * GetResolution().y;

	return result;
}

float2 distortionOffsetCoordsToTextureCoords(float2 offset) {
	// Scale the distorted result so that we fill the desired amount of pixel real-estate
	float2 result = offset;

	// Correct for the aspect ratio
	result.y *= GetInvResolution().y * GetResolution().x;

	// Convert from using the lens center as the origin to
	// using the screen center as the origin
	result += GetOption(u_lensCenterOffset);

	// Convert the texture coordinates from "-1 to 1" to "0 to 1"
	result *= 0.5;  result += 0.5;

	return result;
}

void main(){
	// Grab the texture coordinate, which will be in the range 0-1 in both X and Y
	float2 offset = textureCoordsToDistortionOffsetCoords(GetCoordinates());

	// Determine the amount of distortion based on the distance from the lens center
	float scale = distortionScale(offset);

	// Scale the offset coordinate by the distortion factor introduced by the Rift lens
	float2 distortedOffset = offset * scale;

	// Now convert the data back into actual texture coordinates
	float2 actualTextureCoords = distortionOffsetCoordsToTextureCoords(distortedOffset);


	if (actualTextureCoords.x < 0 || actualTextureCoords.x > 1 || actualTextureCoords.y < 0 || actualTextureCoords.y > 1) 
	{
		SetOutput(float4(0,0,0,0));
	}
	else
	{
		SetOutput(SampleLocation(actualTextureCoords));
	}
}