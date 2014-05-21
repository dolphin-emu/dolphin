uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	//variables
	float internalresolution = 1278.0;
	float4 c0 = texture(samp9, uv0).rgba;

	//blur
	float4 blurtotal = float4(0.0, 0.0, 0.0, 0.0);
	float blursize = 1.5;
	blurtotal += texture(samp9, uv0 + float2(-blursize, -blursize)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2(-blursize,  blursize)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2( blursize, -blursize)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2( blursize,  blursize)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2(-blursize,  0.0)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2( blursize,  0.0)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2( 0.0, -blursize)*resolution.zw);
	blurtotal += texture(samp9, uv0 + float2( 0.0,  blursize)*resolution.zw);
	blurtotal *= 0.125;
	c0 = blurtotal;

	//greyscale
	float grey = ((0.3 * c0.r) + (0.4 * c0.g) + (0.3 * c0.b));

	// brighten and apply horizontal scanlines
	// This would have been much simpler if I could get the stupid modulo (%) to work
	// If anyone who is more well versed in Cg knows how to do this it'd be slightly more efficient
	// float lineIntensity = ((uv0[1] % 9) - 4) / 40;
	float vPos = uv0.y*resolution.y / 9.0;
	float lineIntensity = (((vPos - floor(vPos)) * 9.0) - 4.0) / 40.0;
	grey = grey * 0.5 + 0.7 + lineIntensity;

	// darken edges
	float x = uv0.x * resolution.x;
	float y = uv0.y * resolution.y;

	if (x > internalresolution/2.0)
		x = internalresolution-x;

	if (y > internalresolution/2.0)
		y = internalresolution-y;

	if (x > internalresolution/2.0*0.95)
		x = internalresolution/2.0*0.95;

	if (y > internalresolution/2.0*0.95)
		y = internalresolution/2.0*0.95;

	x = -x + 641.0;
	y = -y + 641.0;

	//****inline square root routines*****/
	// bit of a performance bottleneck.
	// neccessary to make the darkened area rounded
	// instead of rhombus-shaped.
	float sqrt = x / 10.0;
	while ((sqrt*sqrt) < x)
		sqrt+=0.1;
	x = sqrt;
	sqrt = y / 10.0;
	while ((sqrt*sqrt) < y)
		sqrt+=0.1;
	y = sqrt;

	x *= 2.0;
	y *= 2.0;
	grey -= x / 200.0;
	grey -= y / 200.0;

	// output
	ocol0 = float4(0.0, grey, 0.0, 1.0);
}
