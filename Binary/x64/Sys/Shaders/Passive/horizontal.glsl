// Passive (horizontal rows) shader

void main()
{
	float screen_row = GetWindowResolution().y * GetCoordinates().y;
	SetOutput(SampleLayer(int(screen_row) % 2));
}
