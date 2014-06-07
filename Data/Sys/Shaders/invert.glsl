SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	ocol0 = vec4(1.0, 1.0, 1.0, 1.0) - texture(samp9, uv0);
}
