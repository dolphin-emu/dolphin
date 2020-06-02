// Freelook 2-view preferring horizontal

void main()
{
	if (GetCoordinates().y <= 0.5)
	{
		SetOutput(texture(samp0, float3(v_tex0.x, v_tex0.y * 2, 0)));
	}
	else
	{
		SetOutput(texture(samp0, float3(v_tex0.x, v_tex0.y * 2 - 1, 1)));
	}
}
