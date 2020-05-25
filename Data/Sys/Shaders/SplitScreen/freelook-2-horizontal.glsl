// Freelook 2-view preferring horizontal

void main()
{
	if (GetCoordinates().x <= 0.5)
	{
		SetOutput(texture(samp0, float3(v_tex0.x * 2, v_tex0.y, 0)));
	}
	else
	{
		SetOutput(texture(samp0, float3(v_tex0.x * 2 - 1, v_tex0.y, 1)));
	}
}
