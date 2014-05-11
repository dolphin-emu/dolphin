uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	ocol0 = texture(samp9, vec2(1.0, 1.0) - uv0);
}
