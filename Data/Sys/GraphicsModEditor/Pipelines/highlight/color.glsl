vec4 custom_main( in CustomShaderData data )
{
  vec3 tint_color = vec3(0.0, 0.0, 1.0);
  vec3 color = (0.5 * data.final_color.rgb) + (0.5 * tint_color.rgb);
  return vec4(color, 1.0);
}
