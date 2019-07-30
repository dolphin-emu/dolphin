const float amount = 0.60; // suitable range = 0.00 - 1.00
const float power = 0.5; // suitable range = 0.0 - 1.0

void main()
{
  float2 vCoord0 = GetCoordinates();
  float3 color = Sample().xyz;
  float4 sum = vec4(0);
  float3 bloom;

  for(int i= -3 ;i < 3; i++)
  {
    sum += SampleLocation(vCoord0 + vec2(-1, i) * 0.004) * amount;
    sum += SampleLocation(vCoord0 + vec2( 0, i) * 0.004) * amount;
    sum += SampleLocation(vCoord0 + vec2( 1, i) * 0.004) * amount;
  }

  if (color.r < 0.3 && color.g < 0.3 && color.b < 0.3)
  {
    bloom = sum.xyz * sum.xyz * 0.012 + color;
  }
  else
  {
    if (color.r < 0.5 && color.g < 0.5 && color.b < 0.5)
    {
      bloom = sum.xyz * sum.xyz * 0.009 + color;
    }
    else
    {
      bloom = sum.xyz * sum.xyz * 0.0075 + color;
    }
  }

  SetOutput(float4(mix(color, bloom, power), 1.0));
}
