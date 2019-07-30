const float scaleoffset = 0.8; //edge detection offset
const float bb = 0.5; // effects black border sensitivity; from 0.0 to 1.0

void main()
{
  float2 delta = GetInvResolution() * scaleoffset;
  float2 dg1 = float2( delta.x, delta.y);
  float2 dg2 = float2(-delta.x, delta.y);
  float2 dx  = float2( delta.x, 0.0);
  float2 dy  = float2( 0.0, delta.y);

  float4 v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4, v_texcoord5, v_texcoord6;
  
  v_texcoord1.xy = GetCoordinates() - dy;
  v_texcoord2.xy = GetCoordinates() + dy;
  v_texcoord3.xy = GetCoordinates() - dx;
  v_texcoord4.xy = GetCoordinates() + dx;
  v_texcoord5.xy = GetCoordinates() - dg1;
  v_texcoord6.xy = GetCoordinates() + dg1;
  v_texcoord1.zw = GetCoordinates() - dg2;
  v_texcoord2.zw = GetCoordinates() + dg2;

  float3 c00 = SampleLocation(v_texcoord5.xy).xyz; 
  float3 c10 = SampleLocation(v_texcoord1.xy).xyz; 
  float3 c20 = SampleLocation(v_texcoord2.zw).xyz; 
  float3 c01 = SampleLocation(v_texcoord3.xy).xyz; 
  float3 c11 = SampleLocation(GetCoordinates()).xyz; 
  float3 c21 = SampleLocation(v_texcoord4.xy).xyz; 
  float3 c02 = SampleLocation(v_texcoord1.zw).xyz; 
  float3 c12 = SampleLocation(v_texcoord2.xy).xyz; 
  float3 c22 = SampleLocation(v_texcoord6.xy).xyz; 
  float3 dt = float3(1.0, 1.0, 1.0); 

  float d1 = dot(abs(c00 - c22), dt);
  float d2 = dot(abs(c20 - c02), dt);
  float hl = dot(abs(c01 - c21), dt);
  float vl = dot(abs(c10 - c12), dt);
  float  d = bb * (d1 + d2 + hl + vl) / (dot(c11, dt) + 0.15);

  float lc = 4.0 * length(c11);
  float f = fract(lc); f*=f;
  lc = 0.25 * (floor(lc) + f * f) + 0.05;
  c11 = 4.0 * normalize(c11); 
  float3 frct = fract(c11); frct *= frct;
  c11 = floor(c11) + 0.05 * dt + frct * frct;
  SetOutput(float4(0.25 * lc * (1.1 - d * sqrt(d)) * c11, 1.0));
}
