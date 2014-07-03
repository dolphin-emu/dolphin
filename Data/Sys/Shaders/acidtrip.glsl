uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	ocol0 = (texture(samp9, uv0+resolution.zw) - texture(samp9, uv0-resolution.zw)) * 8.0;
}
