void main()
{
	float4 c0 = Sample();

	// Same coefficients as grayscale2 at this point
	float avg = (0.222 * c0.r) + (0.707 * c0.g) + (0.071 * c0.b);
	float red=avg;

	// Not sure about these coefficients, they just seem to produce the proper yellow
	float green=avg*.75;
	float blue=avg*.5;
	SetOutput(float4(red, green, blue, c0.a));
}
