uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	float4 c0 = texture(samp9, uv0);
	float4 c1 = texture(samp9, uv0 - float2(1.0, 0.0) * resolution.zw);
	float4 c2 = texture(samp9, uv0 - float2(0.0, 1.0) * resolution.zw);
	float4 c3 = texture(samp9, uv0 + float2(1.0, 0.0) * resolution.zw);
	float4 c4 = texture(samp9, uv0 + float2(0.0, 1.0) * resolution.zw);

	float red = c0.r;
	float blue = c0.b;
	float green = c0.g;

	float red2 = (c1.r + c2.r + c3.r + c4.r) / 4.0;
	float blue2 = (c1.b + c2.b + c3.b + c4.b) / 4.0;
	float green2 = (c1.g + c2.g + c3.g + c4.g) / 4.0;

	if (red2 > 0.3)
		red = c0.r + c0.r / 2.0;
	else
		red = c0.r - c0.r / 2.0;

	if (green2 > 0.3)
		green = c0.g+ c0.g / 2.0;
	else
		green = c0.g - c0.g / 2.0;

	if (blue2  > 0.3)
		blue = c0.b+ c0.b / 2.0;
	else
		blue = c0.b - c0.b / 2.0;

	ocol0 = float4(red, green, blue, c0.a);
}
