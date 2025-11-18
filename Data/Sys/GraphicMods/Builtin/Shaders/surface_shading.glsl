#include "brdf.glsl"

#ifdef ANSIOTROPY_ENABLED
vec3 direct_lighting(vec3 normal, vec3 eye, vec3 base_color, vec3 light_dir, vec3 light_color, vec3 F0, float roughness, float metallic, float attenuation, float visibility, float ansiotropy, vec3 ansiotropy_direction, vec3 binormal)
#else
vec3 direct_lighting(vec3 normal, vec3 eye, vec3 base_color, vec3 light_dir, vec3 light_color, vec3 F0, float roughness, float metallic, float attenuation, float visibility)
#endif
{
	float normal_dot_light = max(dot(normal, light_dir), 0);

	// Compute half vector (half keyword is reserved)
	vec3 h = normalize(light_dir + eye);
	float normal_dot_view = max(dot(normal, eye), 0.0001);

#ifdef ANSIOTROPY_ENABLED
	float distribution = DistributionAnsiotropic(ansiotropy, ansiotropy_direction, roughness, normal, binormal, h);
#else
	float distribution = DistributionGGX(roughness, normal, h);
#endif

	float geometry = GeometrySchlickGGX(normal_dot_view, normal_dot_light, roughness);
	vec3 fresnel = FresnelSchlick(F0, eye, h);

	vec3 specular = vec3(0, 0, 0);
	if (normal_dot_light > 0.0)
	{
		specular = distribution * geometry * fresnel / (4 * normal_dot_light * normal_dot_view);
	}

	// Use fresnel as a simple way of combining the two layers
	vec3 diffuse_weight = 1.0 - fresnel;
	diffuse_weight *= 1.0 - metallic;
	vec3 brdf = base_color * Diffuse_Lambertian() * diffuse_weight + specular;

	return brdf * light_color * attenuation * normal_dot_light * visibility;
	
	// TODO: rimlight
	// TODO: Sheen
}
