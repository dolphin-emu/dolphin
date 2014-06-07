SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	// Info: http://www.oreillynet.com/cs/user/view/cs_msg/8691
	float avg = (0.222 * c0.r) + (0.707 * c0.g) + (0.071 * c0.b);
	ocol0 = vec4(avg, avg, avg, c0.a);
}
