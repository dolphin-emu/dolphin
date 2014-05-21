uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	float4 c0 = texture(samp9, uv0).rgba;
	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;

	if (c0.r > 0.25)
		red = c0.r;

	if (c0.g > 0.25)
		green = c0.g;

	if (c0.b > 0.25)
		blue = c0.b;

	ocol0 = float4(red, green, blue, 1.0);
}
