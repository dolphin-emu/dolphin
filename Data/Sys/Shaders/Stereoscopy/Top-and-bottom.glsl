#if GLSL
	#define FIRST_LAYER_INDEX (1)
	#define SECOND_LAYER_INDEX (0)
#else
	#define FIRST_LAYER_INDEX (0)
	#define SECOND_LAYER_INDEX (1)
#endif

void main()
{
	float2 texCoord = GetCoordinates();

	if (texCoord.y < 0.5f)
	{
		texCoord.y = texCoord.y * 2.0f;
		SetOutput(SampleLayerLocation(FIRST_LAYER_INDEX, texCoord));
	}
	else
	{
		texCoord.y = texCoord.y * 2.0f - 1.0f;
		SetOutput(SampleLayerLocation(SECOND_LAYER_INDEX, texCoord));
	}
}
