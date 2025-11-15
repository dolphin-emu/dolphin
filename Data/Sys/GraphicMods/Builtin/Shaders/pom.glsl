// Pulled from different details about the theory and some web tutorials
vec2 ComputeParallaxCoordinates(sampler2DArray depthMap, vec3 texCoords, vec3 viewDir, float scale)
{ 
	// number of depth layers
	const float minLayers = 8;
	const float maxLayers = 32;
	float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
	// calculate the size of each layer
	float layerDepth = 1.0 / numLayers;
	// depth of current layer
	float currentLayerDepth = 0.0;
	// the amount to shift the texture coordinates per layer (from vector P)
	vec2 P = viewDir.xy * scale; 
	vec2 deltaTexCoords = P / numLayers;

	// get initial values
	vec2  currentTexCoords     = texCoords.xy;
	float currentDepthMapValue = texture(depthMap, vec3(currentTexCoords, texCoords.z)).r;

	int loop_counter = 0;
	for( int i = 0; i < maxLayers; i++ ) {
		if ( i >= numLayers )
			break;
		if( currentLayerDepth >= currentDepthMapValue )
			break;
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = texture(depthMap, vec3(currentTexCoords, texCoords.z)).r;
		// get depth of next layer
		currentLayerDepth += layerDepth;  
		loop_counter++;
	}

	// get texture coordinates before collision (reverse operations)
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = texture(depthMap, vec3(prevTexCoords, texCoords.z)).r - currentLayerDepth + layerDepth;

	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords.xy;
}
