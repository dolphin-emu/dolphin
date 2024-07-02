float4 SampleTexmap(uint texmap, float3 coords)
{
	for (uint i = 0; i < 8; i++)
	{
		if (texmap == i)
		{
			return texture(samp[i], coords);
		}
	}
	return float4(0, 0, 0, 1);
}

float2 GetTextureSize(uint texmap)
{
	for (uint i = 0; i < 8; i++)
	{
		if (texmap == i)
		{
			return float2(textureSize(samp[i], 0));
		}
	}
	return float2(0, 0);
}

vec4 custom_main( in CustomShaderData data )
{
	if (data.texcoord_count == 0)
	{
		return data.final_color;
	}
	
	if (data.tev_stage_count == 0)
	{
		return data.final_color;
	}

	uint efb = data.tev_stages[0].texmap;
	float3 coords = data.texcoord[0];
	float4 out_color = SampleTexmap(efb, coords);
	float2 size = GetTextureSize(efb);
	
	// If options are added to the UI, include custom radius and intensity, radius should be around IR - 1. 
	// Small values decrease bloom area, but can lead to broken bloom if too small.
	float intensity = 1.0;
	
	float radius = 3;
	float dx = 1.0/size.x;
	float dy = 1.0/size.y;
	float x;
	float y;
	float count = 1.0;
	float4 color = float4(0.0, 0.0, 0.0, 0.0);
	
	for (x = -radius; x <= radius; x++)
	{
		for (y = -radius; y <= radius; y++)
		{
			count += 1.0;
			float3 off_coords = float3(coords.x + x*dx, coords.y + y*dy, coords.z);
			color += SampleTexmap(efb, off_coords);
		}
	}
	
	out_color = color / count * intensity;
	return out_color;
}
