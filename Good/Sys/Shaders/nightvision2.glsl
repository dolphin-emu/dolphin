void main()
{
	//variables
	float internalresolution = 1278.0;
	float4 c0 = Sample();

	//blur
	float4 blurtotal = float4(0.0, 0.0, 0.0, 0.0);
	float blursize = 1.5;
	blurtotal += SampleLocation(GetCoordinates() + float2(-blursize, -blursize) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2(-blursize, blursize) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2( blursize, -blursize) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2( blursize, blursize) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2(-blursize, 0.0) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2( blursize, 0.0) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2( 0.0, -blursize) * GetInvResolution());
	blurtotal += SampleLocation(GetCoordinates() + float2( 0.0, blursize) * GetInvResolution());
	blurtotal *= 0.125;
	c0 = blurtotal;

	//greyscale
	float grey = ((0.3 * c0.r) + (0.4 * c0.g) + (0.3 * c0.b));

	// brighten
	grey = grey * 0.5 + 0.7;

	// darken edges
	float x = GetCoordinates().x * GetResolution().x;
	float y = GetCoordinates().y * GetResolution().y;
	if (x > internalresolution/2.0)
		x = internalresolution-x;
	if (y > internalresolution/2.0)
		y = internalresolution-y;
	if (x > internalresolution/2.0*0.95)
		x = internalresolution/2.0*0.95;
	if (y > internalresolution/2.0*0.95)
		y = internalresolution/2.0*0.95;
	x = -x+641.0;
	y = -y+641.0;

	/*****inline square root routines*****/
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
	SetOutput(float4(0.0, grey, 0.0, 1.0));
}
