const int char_width = 8;
const int char_height = 13;
const int char_count = 95;
const int char_pixels = char_width*char_height;
const float2 char_dim = float2(char_width, char_height);
const float2 font_scale = float2(1.0/float(char_width)/float(char_count), 1.0/float(char_height));

void main()
{
	float2 char_pos = floor(GetCoordinates()*GetResolution()/char_dim);
	float2 pixel_offset = floor(GetCoordinates()*GetResolution()) - char_pos*char_dim;

	// just a big number
	float mindiff = float(char_width*char_height) * 100.0;

	float minc = 0.0;
	float4 mina = float4(0.0, 0.0, 0.0, 0.0);
	float4 minb = float4(0.0, 0.0, 0.0, 0.0);

	for (int i=0; i<char_count; i++)
	{
		float4 ff = float4(0.0, 0.0, 0.0, 0.0);
		float4 f = float4(0.0, 0.0, 0.0, 0.0);
		float4 ft = float4(0.0, 0.0, 0.0, 0.0);
		float4 t = float4(0.0, 0.0, 0.0, 0.0);
		float4 tt = float4(0.0, 0.0, 0.0, 0.0);

		for (int x=0; x<char_width; x++)
		{
			for (int y=0; y<char_height; y++)
			{
				float2 tex_pos = char_pos*char_dim + float2(x,y) + 0.5;
				float4 tex = SampleLocation(tex_pos * GetInvResolution());

				float2 font_pos = float2(x+i*char_width, y) + 0.5;
				float4 font = SampleFontLocation(font_pos * font_scale);

				// generates sum of texture and font and their squares
				ff += font*font;
				f += font;
				ft += font*tex;
				t += tex;
				tt += tex*tex;
			}
		}

		// The next lines are a bit harder, hf :-)

		// The idea is to find the perfect char with the perfect background color and the perfect font color.
		// As this is an equation with three unknowns, we can't just try all chars and color combinations.

		// As criterion how "perfect" the selection is, we compare the "mean squared error" of the resulted colors of all chars.
		// So, now the big issue: how to calculate the MSE without knowing the two colors ...

		// In the next steps, "a" is the font color, "b" is the background color, "f" is the font value at this pixel, "t" is the texture value

		// So the square error of one pixel is:
		// e = ( t - a⋅f - b⋅(1-f) ) ^ 2

		// In longer:
		// e = a^2⋅f^2 - 2⋅a⋅b⋅f^2 + 2⋅a⋅b⋅f - 2⋅a⋅f⋅t + b^2⋅f^2 - 2⋅b^2⋅f + b^2 + 2⋅b⋅f⋅t - 2⋅b⋅t + t^2

		// The sum of all errors is: (as shortcut, ff,f,ft,t,tt are now the sums like declared above, sum(1) is the count of pixels)
		// sum(e) = a^2⋅ff - 2⋅a^2⋅ff + 2⋅a⋅b⋅f - 2⋅a⋅ft + b^2⋅ff - 2⋅b^2⋅f + b^2⋅sum(1) + 2⋅b⋅ft - 2⋅b⋅t + tt

		// To find the minimum, we have to derive this by "a" and "b":
		// d/da sum(e) = 2⋅a⋅ff + 2⋅b⋅f - 2⋅b⋅ff - 2⋅ft
		// d/db sum(e) = 2⋅a⋅f - 2⋅a⋅ff - 4⋅b⋅f + 2⋅b⋅ff + 2⋅b⋅sum(1) + 2⋅ft - 2⋅t

		// So, both equations must be zero at minimum and there is only one solution.

		float4 a = (f*ft - ff*t + f*t - ft*float(char_pixels)) / (f*f  - ff*float(char_pixels));
		float4 b = (f*ft - ff*t) / (f*f  - ff*float(char_pixels));

		float4 diff = a*a*ff + 2.0*a*b*f - 2.0*a*b*ff - 2.0*a*ft + b*b *(-2.0*f + ff + float(char_pixels)) + 2.0*b*ft - 2.0*b*t + tt;
		float diff_f = dot(diff, float4(1.0, 1.0, 1.0, 1.0));

		if (diff_f < mindiff)
		{
			mindiff = diff_f;
			minc = float(i);
			mina = a;
			minb = b;
		}
	}

	float2 font_pos_res = float2(minc * float(char_width), 0.0) + pixel_offset + 0.5;

	float4 col = SampleFontLocation(font_pos_res * font_scale);
	SetOutput(mina * col + minb * (float4(1.0,1.0,1.0,1.0) - col));
}
