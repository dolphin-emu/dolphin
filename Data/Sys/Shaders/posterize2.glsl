uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

float bound(float color)
{
	if (color < 0.35)
	{
		if (color < 0.25)
			return color;

		return 0.5;
	}

	return 1.0;
}

void main()
{
	float4 c0 = texture(samp9, uv0);
	ocol0 = float4(bound(c0.r), bound(c0.g), bound(c0.b), c0.a);
}
