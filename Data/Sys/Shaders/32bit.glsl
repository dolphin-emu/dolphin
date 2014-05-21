uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	//Change this number to increase the pixel size.
	float pixelSize = 2.0;

	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;

	vec2 pos = floor(uv0 * resolution.xy / pixelSize) * pixelSize * resolution.zw;

	vec4 c0 = texture(samp9, pos);

	if (c0.r < 0.06)
		red = 0.06;
	else if (c0.r < 0.13)
		red = 0.13;
	else if (c0.r < 0.26)
		red = 0.26;
	else if (c0.r < 0.33)
		red = 0.33;
	else if (c0.r < 0.46)
		red = 0.46;
	else if (c0.r < 0.60)
		red = 0.60;
	else if (c0.r < 0.73)
		red = 0.73;
	else if (c0.r < 0.80)
		red = 0.80;
	else if (c0.r < 0.93)
		red = 0.93;
	else
		red = 1.0;

	if (c0.b < 0.06)
		blue = 0.06;
	else if (c0.b < 0.13)
		blue = 0.13;
	else if (c0.b < 0.26)
		blue = 0.26;
	else if (c0.b < 0.33)
		blue = 0.33;
	else if (c0.b < 0.46)
		blue = 0.46;
	else if (c0.b < 0.60)
		blue = 0.60;
	else if (c0.b < 0.73)
		blue = 0.73;
	else if (c0.b < 0.80)
		blue = 0.80;
	else if( c0.b < 0.93)
		blue = 0.93;
	else
		blue = 1.0;


	if (c0.g < 0.06)
		green = 0.06;
	else if (c0.g < 0.13)
		green = 0.13;
	else if (c0.g < 0.26)
		green = 0.26;
	else if (c0.g < 0.33)
		green = 0.33;
	else if (c0.g < 0.46)
		green = 0.46;
	else if (c0.g < 0.60)
		green = 0.60;
	else if (c0.g < 0.73)
		green = 0.73;
	else if (c0.g < 0.80)
		green = 0.80;
	else if( c0.g < 0.93)
		green = 0.93;
	else
		green = 1.0;

	ocol0 = vec4(red, green, blue, c0.a);
}
