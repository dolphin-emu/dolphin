SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float avg = (c0.r + c0.g + c0.b) / 3.0;
	ocol0 = vec4(avg, avg, avg, c0.a);
}
