/*
ported from https://github.com/Fubaxiusz/fubax-shaders/blob/master/Shaders/LUTTools.fx

Display LUT PS v1.3.3 (c) 2018 Jacob Maximilian Fober;
(remix of LUT shader 1.0 (c) 2016 Marty McFly)
This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/

void main()
{
	float2 LutBounds;
	#if LUT_VERTICAL == 1
		LutBounds = float2(_LutSize, _LutSize*_LutSize);
	#else
		LutBounds = float2(_LutSize*_LutSize, _LutSize);
	#endif
	LutBounds *= GetInvPrevResolution();

	if( GetCoordinates().x >= LutBounds.x || GetCoordinates().y >= LutBounds.y)
	{
		SetOutput(float4(Sample().rgb, 1.0));
	}
	else
	{
		// Generate pattern UV
		float2 pattern = GetCoordinates()*GetPrevResolution()/_LutSize;
		// Convert pattern to RGB LUT
		float3 color;
		color.rg = frac(pattern)-0.5/_LutSize;
		color.rg /= 1.0-1.0/_LutSize;
#if LUT_VERTICAL == 1
		color.b = floor(pattern.g)/(_LutSize-1);
#else
		color.b = floor(pattern.r)/(_LutSize-1);
#endif
		// Display LUT texture
		SetOutput(float4(color, 1.0));
	}
}
