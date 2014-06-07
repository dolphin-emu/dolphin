SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	vec4 a = texture(samp9, uv0+resolution.zw);
	vec4 b = texture(samp9, uv0-resolution.zw);
	ocol0 = ( a*a*1.3 - b ) * 8.0;
}
