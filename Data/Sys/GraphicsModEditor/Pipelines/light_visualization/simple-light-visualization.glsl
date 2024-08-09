CustomShaderOutput custom_main( in CustomShaderData data )
{
	if (data.normal.xyz == vec3(0, 0, 0))
	{
		CustomShaderOutput no_normal_color;
		no_normal_color.main_rt = data.final_color;
		return no_normal_color;
	}

	vec3 light_accumulation = vec3(0, 0, 0);
	for (int i = 0; i < data.light_chan0_color_count; i++)
	{
		if (data.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_POINT)
		{
			vec3 light_dir = normalize(data.lights_chan0_color[i].position - data.position.xyz);
			float attenuation = (dot(data.normal, light_dir) >= 0.0) ? max(0.0, dot(data.normal, data.lights_chan0_color[i].direction.xyz)) : 0.0;
			vec3 cosAttn = data.lights_chan0_color[i].cosatt.xyz;
			vec3 distAttn = data.lights_chan0_color[i].distatt.xyz;
			attenuation = max(0.0, dot(cosAttn, vec3(1.0, attenuation, attenuation*attenuation))) / dot(distAttn, vec3(1.0, attenuation, attenuation * attenuation));

			vec3 light_color = data.lights_chan0_color[i].color;
			light_accumulation += attenuation * dot(light_dir, data.normal) * light_color;
		}
		else if (data.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_NONE || data.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_DIR)
		{
			vec3 light_dir = normalize(data.lights_chan0_color[i].position - data.position.xyz);
			if (length(light_dir) == 0)
			{
				light_dir = data.normal.xyz;
			}

			vec3 light_color = data.lights_chan0_color[i].color;
			light_accumulation += dot(light_dir, data.normal) * light_color;
		}
		else if (data.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_SPOT)
		{
			vec3 light_dir = normalize(data.lights_chan0_color[i].position - data.position.xyz);
			float distsq = dot(light_dir, light_dir);
			float dist = sqrt(distsq);
			vec3 light_dir_div = light_dir / dist;
			float attenuation = max(0.0, dot(light_dir_div, data.lights_chan0_color[i].direction.xyz));

			vec3 cosAttn = data.lights_chan0_color[i].cosatt.xyz;
			vec3 distAttn = data.lights_chan0_color[i].distatt.xyz;
			attenuation = max(0.0, cosAttn.x + cosAttn.y*attenuation + cosAttn.z*attenuation*attenuation) / dot( distAttn, vec3(1.0, dist, distsq) );

			vec3 light_color = data.lights_chan0_color[i].color;
			light_accumulation += attenuation * dot(light_dir, data.normal) * light_color;
		}
	}
	light_accumulation = clamp(light_accumulation, 0, 255);

	CustomShaderOutput result;
	result.main_rt = vec4((light_accumulation + data.ambient_lighting[0].xyz) * data.base_material[0].xyz, data.final_color.a);
	return result;
}
