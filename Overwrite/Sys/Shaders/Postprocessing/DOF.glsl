// Simple Depth of Field
[configuration]
[OptionRangeFloat]
GUIName = Blur Radius
OptionName = Blur
MinValue = 0.5
MaxValue = 2.0
DefaultValue = 1.0
StepAmount = 0.01
[OptionRangeFloat]
GUIName = Focus Position
OptionName = focus
MinValue = 0.0, 0.0
MaxValue = 1.0, 1.0
DefaultValue = 0.5, 0.5
StepAmount = 0.01, 0.01
[/configuration]

void main()
{
	float focusDepth = SampleDepthLocation(GetOption(focus).xy);
	float depth = SampleDepth();
	depth = clamp(abs((depth - focusDepth) / depth), 0.0, 1.0);
	float4 pixelColor = Sample();
	float2 unit = GetInvResolution() * GetOption(Blur);
	float2 coords = GetCoordinates();
	float4 color1 = SampleLocation(coords + unit * float2(-1.50, -0.66));
	float4 color2 = SampleLocation(coords + unit * float2(0.66, -1.50));
	float4 color3 = SampleLocation(coords + unit * float2(1.50, 0.66));
	float4 color4 = SampleLocation(coords + unit * float2(-0.66, 1.50));

	float4 blurred = (color1 + color2 + color3 + color4 + pixelColor) / 5.0;
	SetOutput(lerp(pixelColor, blurred, depth));
}