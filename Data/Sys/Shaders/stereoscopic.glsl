// Omega's 3D Stereoscopic filtering
// TODO: Need depth info!

SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	// Source Color
	float4 c0 = texture(samp9, uv0).rgba; 
	float sep = 5.0;
	float red   = c0.r;
	float green = c0.g;
	float blue  = c0.b;

	// Left Eye (Red)
	float4 c1 = texture(samp9, uv0 + float2(sep,0.0)*resolution.zw).rgba;
	red = max(c0.r, c1.r);

	// Right Eye (Cyan)
	float4 c2 = texture(samp9, uv0 + float2(-sep,0.0)*resolution.zw).rgba;
	float cyan = (c2.g + c2.b) / 2.0;
	green = max(c0.g, cyan);
	blue = max(c0.b, cyan);

	ocol0 = float4(red, green, blue, c0.a);
}
