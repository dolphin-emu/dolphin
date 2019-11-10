void main()
{
	float4 c0 = Sample();
	// Info: https://web.archive.org/web/20040101053504/http://www.oreillynet.com:80/cs/user/view/cs_msg/8691
	float avg = (0.222 * c0.r) + (0.707 * c0.g) + (0.071 * c0.b);
	SetOutput(float4(avg, avg, avg, c0.a));
}
