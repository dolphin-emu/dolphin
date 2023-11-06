CustomShaderOutput custom_main( in CustomShaderData data )
{
	CustomShaderOutput custom_output;
	custom_output.main_rt_src0 = data.final_color;
	custom_output.main_rt_src1 = data.final_color1;
	return custom_output;
}
