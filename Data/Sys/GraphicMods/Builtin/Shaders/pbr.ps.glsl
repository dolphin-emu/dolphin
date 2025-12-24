#include <lighting.glsl>
#include <surface_shading.glsl>
#include <pom.glsl>

void process_fragment(in DolphinFragmentInput frag_input, out DolphinFragmentOutput frag_output)
{
	if (frag_input.normal.xyz == float3(0, 0, 0))
	{
		dolphin_process_emulated_fragment(frag_input, frag_output);
	}
	else
	{
		vec3 camera_position = vec3(0, 0, 0);
		mat4x4 freelook = dolphin_freelook_matrix();
		mat4x4 freelook_inverse = inverse(freelook);
		camera_position = (vec4(camera_position, 1) * freelook_inverse).xyz;
		vec3 eye = normalize(camera_position - frag_input.position);
		float2 texcoord = frag_input.tex0.z == 0.0 ? frag_input.tex0.xy : frag_input.tex0.xy / frag_input.tex0.z;

#ifdef HAS_HEIGHT_TEX
		mat3 TBN = cotangent_frame( normalize(frag_input.normal), -eye, texcoord.xy);
		mat3 TBN_transpose = transpose(TBN);
		if (custom_uniforms.parallax_scale != 0.0)
		{
			vec33 eye_tangent = normalize(TBN_transpose * eye);
			float2 new_uv = ComputeParallaxCoordinates(samp_HEIGHT_TEX, texcoord, eye_tangent, custom_uniforms.parallax_scale);
			if(new_uv.x > 1.0 || new_uv.y > 1.0 || new_uv.x < 0.0 || new_uv.y < 0.0)
				discard;
			texcoord = new_uv;
		}
#endif
#ifdef HAS_ALBEDO_TEX
		vec4 tex = texture(samp_ALBEDO_TEX, texcoord);
		vec4 base_color = vec4(pow(tex.r, 2.2), pow(tex.g, 2.2), pow(tex.b, 2.2), tex.a) * custom_uniforms.albedo_tint;
#else
		vec4 base_color = custom_uniforms.albedo_tint;
#endif
#ifdef HAS_NORMAL_TEX
		vec4 map_rgb = texture(samp_NORMAL_TEX, texcoord);
		vec4 map = map_rgb * 2.0 - 1.0;
		vec3 normal = perturb_normal(normalize(frag_input.normal), frag_input.position, texcoord.xy, map.xyz) * custom_uniforms.normal_modifier;
#else
		vec3 normal = frag_input.normal.xyz * custom_uniforms.normal_modifier;
#endif

#ifdef HAS_METALLIC_TEX
		float metallic = texture(samp_METALLIC_TEX, texcoord).r * custom_uniforms.metallic_modifier;
#else
		float metallic = custom_uniforms.metallic_modifier;
#endif

#ifdef HAS_ROUGHNESS_TEX
		float roughness = texture(samp_ROUGHNESS_TEX, texcoord).r * custom_uniforms.roughness_modifier;
#else
		float roughness = custom_uniforms.roughness_modifier;
#endif

#ifdef HAS_AMBIENT_OCCLUSION_TEX
		float ao = texture(samp_AMBIENT_OCCLUSION_TEX, texcoord).r * custom_uniforms.ao_modifier;
#else
		float ao = custom_uniforms.ao_modifier;
#endif

#ifdef ANSIOTROPY_ENABLED
		float ansiotropy = custom_uniforms.ansiotropy;
#ifdef HAS_ANSIOTROPY_FLOW_TEX
		vec3 ansiotropy_direction = texture(samp_ANSIOTROPY_FLOW_TEX, texcoord).rgb;
#else
		vec3 ansiotropy_direction = cross(normal, binormal);
#endif
#endif

		vec3 F0 = vec3(0.04, 0.04, 0.04);
		F0 = mix(F0, base_color.rgb, metallic);

		vec3 sum_color = vec3(0, 0, 0);
		for (int i = 0; i < frag_input.light_chan0_color_count; i++)
		{
			float attenuation = 1.0;
			vec3 light_dir;
			vec3 light_color = frag_input.lights_chan0_color[i].color;
			if (frag_input.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_POINT)
			{
				ComputePointLight(frag_input.lights_chan0_color[i].position.xyz, frag_input.lights_chan0_color[i].direction.xyz, frag_input.lights_chan0_color[i].cosatt.xyz, frag_input.lights_chan0_color[i].distatt.xyz, frag_input.position.xyz, normal, light_dir, attenuation);
			}
			if (frag_input.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_NONE || frag_input.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_DIR)
			{
				ComputeDirectionalLight(frag_input.lights_chan0_color[i].position.xyz, frag_input.lights_chan0_color[i].direction.xyz, frag_input.position.xyz, normal, light_dir, attenuation);
			}
			if (frag_input.lights_chan0_color[i].attenuation_type == CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_SPOT)
			{
				ComputeSpotLight(frag_input.lights_chan0_color[i].position.xyz, frag_input.lights_chan0_color[i].direction.xyz, frag_input.lights_chan0_color[i].cosatt.xyz, frag_input.lights_chan0_color[i].distatt.xyz, frag_input.position.xyz, normal, light_dir, attenuation);
			}

			float visibility = 1.0;
#ifdef HAS_HEIGHT_TEX
			vec3 light_dir_tangent = normalize(TBN_transpose * light_dir);
			
			float normal_dot_light = dot(normal, light_dir);
			if (normal_dot_light <= 0 && custom_uniforms.parallax_scale > 0)
			{
				visibility = 1.0 - ComputeSelfShadow(samp_HEIGHT_TEX, texcoord, light_dir_tangent, custom_uniforms.parallax_scale);
			}
#endif

#ifdef ANSIOTROPY_ENABLED
			sum_color += direct_lighting(normal, eye, base_color.rgb, light_dir, light_color, F0, roughness, metallic, attenuation, visibility, ansiotropy, ansiotropy_direction, binormal);
#else
			sum_color += direct_lighting(normal, eye, base_color.rgb, light_dir, light_color, F0, roughness, metallic, attenuation, visibility);
#endif
		}

#ifdef HAS_EMISSIVE_TEX
		sum_color += texture(samp_EMISSIVE_TEX, texcoord).rgb * custom_uniforms.emissive_strength;
		vec3 result = sum_color;
#else
		vec3 ambient_color = frag_input.ambient_lighting[0].rgb * ao;
		vec3 result = sum_color + ambient_color;
#endif
		result = result / (result + vec3(1.0));
		result = pow(result, vec3(1.0/2.2));
		frag_output.main = ivec4(vec4(result, base_color.a) * 255.0);
	}
}
