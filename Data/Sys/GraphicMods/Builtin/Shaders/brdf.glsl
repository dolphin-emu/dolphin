#include "math.glsl"

// Disney: https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf

// From: Karis 2013, "Real Shading in Unreal Engine 4"
// (https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)
float DistributionGGX(float roughness, vec3 normal, vec3 h)
{
	float a_sq = roughness * roughness;
	float normal_dot_half = dot(normal, h);
	float normal_dot_half_sq = normal_dot_half * normal_dot_half;
	float sub_denom = normal_dot_half_sq * (a_sq - 1.0) + 1.0;
	return a_sq / ( PI * sub_denom * sub_denom );
}

// From: Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
// (https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf)
float DistributionAnsiotropic(float ansiotropy, vec3 ansiotropy_direction, float roughness, vec3 normal, vec3 binormal, vec3 h)
{
	float a_sq = roughness * roughness;
	float a_tangent = a_sq;
	float a_binormal = mix(0, a_sq, 1 - ansiotropy);
	
	float normal_dot_half = dot(normal, h);
	float normal_dot_half_sq = normal_dot_half * normal_dot_half;

	float binormal_weight = dot(binormal, h) / a_binormal;
	float binormal_weight_sq = binormal_weight * binormal_weight;
	
	float tangent_weight = dot(ansiotropy_direction, h) / a_tangent;
	float tangent_weight_sq = tangent_weight * tangent_weight;
	
	float sub_denom = binormal_weight_sq + tangent_weight_sq + normal_dot_half_sq;
	return 1 / ( PI * a_tangent * a_binormal * sub_denom * sub_denom );
}

// From: Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
float GeometrySchlickGGX(float normal_dot_view, float normal_dot_light, float roughness)
{
	float a_sq = roughness * roughness;
	
	// View
	float view_sqrt_term = sqrt(a_sq + (1 - a_sq) * normal_dot_view * normal_dot_view);
	float view_numer = 2 * normal_dot_view;
	float view_denom = normal_dot_view + view_sqrt_term;
	float final_view = view_numer / view_denom;
	
	// Light
	float light_sqrt_term = sqrt(a_sq + (1 - a_sq) * normal_dot_light * normal_dot_light);
	float light_numer = 2 * normal_dot_light;
	float light_denom = normal_dot_light + light_sqrt_term;
	float final_light = light_numer / light_denom;

	return final_view * final_light;
}

// From: Karis 2013, "Real Shading in Unreal Engine 4"
vec3 FresnelSchlick(vec3 f0, vec3 view, vec3 h)
{
	float view_dot_half = saturate(dot(view, h));
	// TODO: test Disney's approach and see what the difference is
	float spherical_gaussian_approx = (-5.55473 * view_dot_half -6.98316) * view_dot_half;
	return f0 + (1.0 - f0) * pow(2.0, spherical_gaussian_approx);
}

float Diffuse_Lambertian()
{
	return 1.0 / PI;
}
