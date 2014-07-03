uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;
	float avg = (c0.r + c0.g + c0.b) / 3.0;

	red = c0.r + (c0.g / 2.0) + (c0.b / 3.0);
	green = c0.r / 3.0;

	ocol0 = vec4(red, green, blue, 1.0);
}
