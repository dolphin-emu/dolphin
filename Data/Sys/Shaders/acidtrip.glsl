void main()
{
	SetOutput((SampleOffset(int2(1, 1)) - SampleOffset(int2(-1, -1))) * 8.0);
}
