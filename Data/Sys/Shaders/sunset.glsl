uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	ocol0 = vec4(c0.r * 1.5, c0.g, c0.b * 0.5, c0.a);
}
