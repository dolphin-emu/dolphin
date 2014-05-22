uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float red	= 0.0;
	float green	= 0.0;

	if (c0.r < 0.35 || c0.b > 0.35)
		green = c0.g + (c0.b / 2.0);
	else
		red = c0.r + 0.4;

	ocol0 = vec4(red, green, 0.0, 1.0);
}
