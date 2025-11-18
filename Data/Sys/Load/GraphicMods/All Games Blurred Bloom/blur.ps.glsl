void process_fragment(in DolphinFragmentInput frag_input, out DolphinFragmentOutput frag_output)
{
	int layer = int(frag_input.tex0.z);
	int r = int(efb_scale) * int(SPREAD_MULTIPLIER);
	int coord;
	int length;
	int2 initial_coords = int2(frag_input.tex0.xy * source_resolution.xy);
	vec4 brightness = float4(1.0, 1.0, 1.0, 1.0);
	if (HORIZONTAL)
	{
		length = int(source_resolution.x);
		coord = initial_coords.x;
		brightness = BRIGHTNESS_MULTIPLIER * brightness;
	}
	else
	{
		length = int(source_resolution.y);
		coord = initial_coords.y;
	}

	int offset;
	vec4 count = vec4(0.0,0.0,0.0,0.0);
	vec4 col = vec4(0.0,0.0,0.0,0.0);
	for (offset = -r; offset <= r; offset++)
	{
		int pos = coord + offset;
		if (pos <= length && pos >= 0)
		{
			int3 sample_coords;
			if (HORIZONTAL)
			{
				sample_coords = int3(int2(pos, initial_coords.y), layer);
			}
			else
			{
				sample_coords = int3(int2(initial_coords.x, pos), layer);
			}
			
			col += texelFetch(samp0, sample_coords, 0);
			count += vec4(1.0,1.0,1.0,1.0);
		}
	}
	frag_output.main = col / count * brightness;

}
