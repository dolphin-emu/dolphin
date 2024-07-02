CustomShaderOutput custom_main( in CustomShaderData data )
{
	CustomShaderOutput custom_output;
	custom_output.main_rt = data.final_color;
	vec3 tint_color = vec3(0.0, 0.0, 1.0);
	custom_output.main_rt.rgb = (0.5 * custom_output.main_rt.rgb) + (0.5 * tint_color.rgb);
	return custom_output;
}
