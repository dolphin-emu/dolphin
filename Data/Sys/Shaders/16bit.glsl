uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	//Change this number to increase the pixel size.
	float pixelSize = 3.0;

	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;

	vec2 pos = floor(uv0 * resolution.xy / pixelSize) * pixelSize * resolution.zw;

	vec4 c0 = texture(samp9, pos);

	if (c0.r < 0.1)
		red = 0.1;
	else if (c0.r < 0.20)
		red = 0.20;
	else if (c0.r < 0.40)
		red = 0.40;
	else if (c0.r < 0.60)
		red = 0.60;
	else if (c0.r < 0.80)
		red = 0.80;
	else
		red = 1.0;

	if (c0.b < 0.1)
		blue = 0.1;
	else if (c0.b < 0.20)
		blue = 0.20;
	else if (c0.b < 0.40)
		blue = 0.40;
	else if (c0.b < 0.60)
		blue = 0.60;
	else if (c0.b < 0.80)
		blue = 0.80;
	else
		blue = 1.0;

	if (c0.g < 0.1)
		green = 0.1;
	else if (c0.g < 0.20)
		green = 0.20;
	else if (c0.g < 0.40)
		green = 0.40;
	else if (c0.g < 0.60)
		green = 0.60;
	else if (c0.g < 0.80)
		green = 0.80;
	else
		green = 1.0;

	ocol0 = vec4(red, green, blue, c0.a);
}
