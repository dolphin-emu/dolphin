/*
[configuration]

[OptionRangeFloat]
GUIName = Distortion amount
OptionName = DISTORTION_FACTOR
MinValue = 1.0
MaxValue = 10.0
StepAmount = 0.5
DefaultValue = 4.0

[OptionRangeFloat]
GUIName = Eye Distance Offset
OptionName = EYE_OFFSET
MinValue = 0.0
MaxValue = 10.0
StepAmount = 0.25
DefaultValue = 5.0

[OptionRangeFloat]
GUIName = Zoom adjustment
OptionName = SIZE_ADJUST
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.025
DefaultValue = 0.5

[OptionRangeFloat]
GUIName = Aspect Ratio adjustment
OptionName = ASPECT_ADJUST
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.025
DefaultValue = 0.5

[/configuration]
*/


void main()
{
  // Base Cardboard distortion parameters
  float factor = GetOption(DISTORTION_FACTOR) * 0.01f;
  float ka = factor * 3.0f;
  float kb = factor * 5.0f;

  // size and aspect adjustment
  float sizeAdjust = 1.0f - GetOption(SIZE_ADJUST) + 0.5f;
  float aspectAdjustment = 1.25f - GetOption(ASPECT_ADJUST);

  // offset centering per eye
  float stereoOffset = GetOption(EYE_OFFSET) * 0.01f;
  float offsetAdd;

  // layer0 = left eye, layer1 = right eye
  if (layer == 1)
  {
    offsetAdd = stereoOffset;
  }
  else
  {
    offsetAdd = 0.0 - stereoOffset;
  }

  // convert coordinates to NDC space
  float2 fragPos = (GetCoordinates() - 0.5f - vec2(offsetAdd, 0.0f)) * 2.0f;

  // Calculate the source location "radius" (distance from the centre of the viewport)
  float destR = length(fragPos);

  // find the radius multiplier
  float srcR = destR * sizeAdjust + ( ka * pow(destR, 2.0) + kb * pow(destR, 4.0));

  // Calculate the source vector (radial)
  vec2 correctedRadial = normalize(fragPos) * srcR;

  // fix aspect ratio
  vec2 widenedRadial = correctedRadial * vec2(aspectAdjustment, 1.0f);

  // Transform the coordinates (from [-1,1]^2 to [0, 1]^2)
  vec2 uv = (widenedRadial/2.0f) + vec2(0.5f) + vec2(offsetAdd, 0.0f);

  // Sample the texture at the source location
  if(clamp(uv, 0.0, 1.0) != uv)
  {
    // black if beyond bounds
    SetOutput(float4(0.0, 0.0, 0.0, 0.0));
  }
  else
  {
    SetOutput(SampleLocation(uv));
  }
}
