/*
[configuration]

[OptionRangeFloat]
GUIName = Inactive Field Brightness
OptionName = INACTIVE_BRIGHT
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 0.0

[OptionRangeFloat]
GUIName = Active Field Brightness
OptionName = ACTIVE_BRIGHT
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.1
DefaultValue = 1.0

[OptionBool]
GUIName = Alternate
OptionName = ALTERNATE
DefaultValue = true

[OptionBool]
GUIName = Use Internal Resolution
OptionName = REAL_RESOLUTION
DefaultValue = false

[/configuration]
*/

void main()
{
	float scanline = (bool(GetOption(REAL_RESOLUTION)) ? GetResolution().y : GetWindowResolution().y) * GetCoordinates().y;
	bool activeField = (int(scanline) & 1) == 0;
	activeField = (bool(GetFrame() % 2u) && bool(GetOption(ALTERNATE))) ? !activeField : activeField;
	SetOutput(SampleLocation(GetCoordinates()) * (activeField ? GetOption(ACTIVE_BRIGHT) : GetOption(INACTIVE_BRIGHT)));
}
