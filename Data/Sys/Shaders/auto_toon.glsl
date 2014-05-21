uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	vec4 to_gray = vec4(0.3,0.59,0.11,0);

	float x1 = dot(to_gray, texture(samp9, uv0+vec2(1,1)*resolution.zw));
	float x0 = dot(to_gray, texture(samp9, uv0+vec2(-1,-1)*resolution.zw));
	float x3 = dot(to_gray, texture(samp9, uv0+vec2(1,-1)*resolution.zw));
	float x2 = dot(to_gray, texture(samp9, uv0+vec2(-1,1)*resolution.zw));

	float edge = (x1 - x0) * (x1 - x0) + (x3 - x2) * (x3 - x2);

	float4 color = texture(samp9, uv0).rgba;

	ocol0 = color - vec4(edge, edge, edge, edge) * 12.0;
}
