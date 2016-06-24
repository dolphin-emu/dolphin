static const char SCREEN_QUAD_VERTEX_SHADER_SOURCE[] = R"(

out float2 uv0;

#if EFB_LAYERS > 1
	flat out int layer;
#endif

void main()
{
	vec2 rawpos = vec2(float(gl_VertexID & 1), float(gl_VertexID & 2));
	gl_Position = vec4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);

	#if EFB_LAYERS > 1
		layer = 0;
	#endif
}

)";

static const char BLIT_FRAGMENT_SHADER_SOURCE[] = R"(

SAMPLER_BINDING(0) uniform sampler2DArray samp0;

out float4 ocol0;
in float2 uv0;

void main()
{
	ocol0 = texture(samp0, float3(uv0, 0.0f));
}

)";
