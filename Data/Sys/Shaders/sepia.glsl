uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);

	// Same coefficients as grayscale2 at this point
	float avg = (0.222 * c0.r) + (0.707 * c0.g) + (0.071 * c0.b);
	float red=avg;

	// Not sure about these coefficients, they just seem to produce the proper yellow
	float green=avg*.75;
	float blue=avg*.5;
	ocol0 = vec4(red, green, blue, c0.a);
}
