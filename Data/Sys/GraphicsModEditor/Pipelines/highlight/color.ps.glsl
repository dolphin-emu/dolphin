void fragment(in DolphinFragmentInput frag_input, out DolphinFragmentOutput frag_output)
{
	dolphin_emulated_fragment(frag_input, frag_output);
	vec3 tint_color = vec3(0.0, 0.0, 1.0);
	vec3 input_color = frag_output.main.rgb / 255.0;
	vec3 output_color = (0.5 * input_color) + (0.5 * tint_color.rgb);
	frag_output.main.rgb = ivec3(output_color * 255);
}
