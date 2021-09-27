/*
ported from https://github.com/Fubaxiusz/fubax-shaders/blob/master/Shaders/LUTTools.fx

Apply LUT PS v2.0.0 (c) 2018 Jacob Maximilian Fober,
(remix of LUT shader 1.0 (c) 2016 Marty McFly)
This work is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International License.
To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/4.0/.
*/

#if LUT_VERTICAL == 1
	#define LUT_DIMENSIONS int2(LUT_BLOCK_SIZE, LUT_BLOCK_SIZE*LUT_BLOCK_SIZE)
#else
	#define LUT_DIMENSIONS int2(LUT_BLOCK_SIZE*LUT_BLOCK_SIZE, LUT_BLOCK_SIZE)
#endif
#define LUT_PIXEL_SIZE 1.0/LUT_DIMENSIONS

void main()
{
	float3 color = Sample().rgb;

	// Convert to sub pixel coordinates
	float3 lut3D = color*(LUT_BLOCK_SIZE-1);

	// Get 2D LUT coordinates
	float2 lut2D[2];
	#if LUT_VERTICAL == 1
		// Front
		lut2D[0].x = lut3D.x;
		lut2D[0].y = floor(lut3D.z)*LUT_BLOCK_SIZE+lut3D.y;
		// Back
		lut2D[1].x = lut3D.x;
		lut2D[1].y = ceil(lut3D.z)*LUT_BLOCK_SIZE+lut3D.y;
	#else
		// Front
		lut2D[0].x = floor(lut3D.z)*LUT_BLOCK_SIZE+lut3D.x;
		lut2D[0].y = lut3D.y;
		// Back
		lut2D[1].x = ceil(lut3D.z)*LUT_BLOCK_SIZE+lut3D.x;
		lut2D[1].y = lut3D.y;
	#endif

	// Convert from texel to texture coords
	lut2D[0] = (lut2D[0]+0.5)*LUT_PIXEL_SIZE;
	lut2D[1] = (lut2D[1]+0.5)*LUT_PIXEL_SIZE;

	// Bicubic LUT interpolation
	float3 color_from_table = mix(
		SampleInputLocation(1, lut2D[0]).rgb, // Front Z
		SampleInputLocation(1, lut2D[1]).rgb, // Back Z
		frac(lut3D.z)
	);

	// Blend LUT image with original
	if ( _LutChromaLuma.x == 1.0 && _LutChromaLuma.y == 1.0 )
		color = color_from_table;
	else
	{
		color = mix(
			normalize(color),
			normalize(color_from_table),
			_LutChromaLuma.x
		)*mix(
			length(color),
			length(color_from_table),
			_LutChromaLuma.y
		);
	}
	
	SetOutput(float4(color, 1.0));
}
