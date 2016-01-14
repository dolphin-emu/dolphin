void main()
{
	float2 texCoord = GetCoordinates();
	if (texCoord.x < 0.5f)
	{
		texCoord.x = texCoord.x * 2.0f;
		SetOutput(SampleLayerLocation(0, texCoord));
	}
	else
	{
		texCoord.x = texCoord.x * 2.0f - 1.0f;
		SetOutput(SampleLayerLocation(1, texCoord));
	}
}
