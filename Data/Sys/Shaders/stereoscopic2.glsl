// Omega's 3D Stereoscopic filtering (Amber/Blue)
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

	// Left Eye (Amber)
	float4 c2 = texture(samp9, uv0 + float2(sep,0.0)*resolution.zw).rgba;
	float amber = (c2.r + c2.g) / 2.0;
	red = max(c0.r, amber);
	green = max(c0.g, amber);

	// Right Eye (Blue)
	float4 c1 = texture(samp9, uv0 + float2(-sep,0.0)*resolution.zw).rgba;
	blue = max(c0.b, c1.b);

	ocol0 = float4(red, green, blue, c0.a);
}
