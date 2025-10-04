// from https://stackoverflow.com/questions/55089830/adding-shadows-to-parallax-occlusion-map
float ComputeSelfShadow(sampler2D depthMap, vec2 texCoords, vec3 lightDir, float heightScale)
{
	float minLayers = 0;
	float maxLayers = 32;
	float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), lightDir)));
	numLayers = 32;

	vec2 currentTexCoords = texCoords.xy;
	float currentDepthMapValue = texture(depthMap, currentTexCoords).r;
	float currentLayerDepth = currentDepthMapValue;
	float startingDepth = currentLayerDepth;

	float layerDepth = 1.0 / numLayers;
	vec2 P = lightDir.xy * heightScale;
	vec2 deltaTexCoords = P / numLayers;

	float shadow_amount = 0.0;
	for( int i = 0; i < maxLayers; i++ ) {
		if ( i >= numLayers )
			break;
		if( currentLayerDepth <= currentDepthMapValue && currentLayerDepth > 0.0 )
			break;
		// shift texture coordinates along direction of P
		currentTexCoords += deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = texture(depthMap, currentTexCoords).r;
		// get depth of next layer
		currentLayerDepth -= layerDepth;  
	}
	
	return currentLayerDepth > currentDepthMapValue ? 0.0 : 1.0;
}

void ComputePointLight(in vec3 light_position, in vec3 light_direction, in vec3 light_cos_attenuation, in vec3 light_distance_attentuation, in vec3 position, in vec3 normal, out vec3 computed_light_dir, out float computed_attenuation)
{
	computed_light_dir = normalize(light_position - position);
	float attenuation = (dot(normal, computed_light_dir) >= 0.0) ? max(0.0, dot(normal, light_direction)) : 0.0;
	float attenuation_sq = attenuation * attenuation;
	computed_attenuation = max(0.0, dot(light_cos_attenuation, vec3(1.0, attenuation, attenuation_sq))) / dot(light_distance_attentuation, vec3(1.0, attenuation, attenuation_sq));
}

void ComputeDirectionalLight(in vec3 light_position, in vec3 light_direction, in vec3 position, in vec3 normal, out vec3 computed_light_dir, out float computed_attenuation)
{
	computed_light_dir = normalize(light_position - position);
	if (length(computed_light_dir) == 0)
	{
		computed_light_dir = normal;
	}
	computed_attenuation = 1.0;
}

void ComputeSpotLight(in vec3 light_position, in vec3 light_direction, in vec3 light_cos_attenuation, in vec3 light_distance_attentuation, in vec3 position, in vec3 normal, out vec3 computed_light_dir, out float computed_attenuation)
{
	computed_light_dir = normalize(light_position - position);
	float distsq = dot(computed_light_dir, computed_light_dir);
	float dist = sqrt(distsq);
	vec3 light_dir_div = computed_light_dir / dist;
	float attenuation = max(0.0, dot(light_dir_div, light_direction));
	computed_attenuation = max(0.0, light_cos_attenuation.x + light_cos_attenuation.y*attenuation + light_cos_attenuation.z*attenuation*attenuation) / dot( light_distance_attentuation, float3(1.0, dist, distsq) );
}
