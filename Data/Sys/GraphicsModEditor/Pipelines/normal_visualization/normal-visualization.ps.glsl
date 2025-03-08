void fragment(in DolphinFragmentInput frag_input, out DolphinFragmentOutput frag_output)
{
	if (frag_input.normal.xyz == vec3(0, 0, 0))
	{
		dolphin_emulated_fragment(frag_input, frag_output);
	}
	else
	{
		vec4 output_color = vec4(frag_input.normal.xyz * 0.5 + 0.5, 1);
		frag_output.main = ivec4(output_color * 255.0);
	}
}
