// Simple interlaced of 2 layers

void main()
{
	float screen_row = GetWindowRect().y + GetFragmentCoord().y;
	int eye = (int(screen_row) & 1);
	SetOutput(SampleLayer(eye));
}
