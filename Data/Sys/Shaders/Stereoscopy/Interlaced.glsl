void main()
{
	float screen_row = GetWindowRect().y + GetFragmentCoord().y;
	int eye = int(screen_row) % 2;
	SetOutput(SampleLayer(eye));
}
