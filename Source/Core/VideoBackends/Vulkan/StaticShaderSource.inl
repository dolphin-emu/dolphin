static const char PASSTHROUGH_VERTEX_SHADER_SOURCE[] = R"(

in float4 ipos;
in float4 itex0;
in float4 icol0;

out float4 otex0;
out float4 ocol0;

#if EFB_LAYERS > 1
	flat out int layer;
#endif

void main()
{
	gl_Position = ipos;
	otex0 = itex0;
	ocol0 = icol0;
	
	#if EFB_LAYERS > 1
		layer = 0;
	#endif
}

)";

static const char PASSTHROUGH_GEOMETRY_SHADER_SOURCE[] = R"(

layout(triangles) in;
layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

in VertexData
{
	float4 otex0;
	float4 ocol0;
} in_data[];

out VertexData
{
	float4 otex0;
	float4 ocol0;
} out_data;

void main()
{
	for (int j = 0; j < EFB_LAYERS; j++)
	{
		for (int i = 0; i < 3; i++)
		{
			gl_Layer = j;
			gl_Position = gl_in[i].gl_Position;
			out_data.otex0 = in_data[i].otex0;
			out_data.ocol0 = in_data[i].ocol0;
			EmitVertex();
		}
		EndPrimitive();
	}
}

)";

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

static const char CLEAR_FRAGMENT_SHADER_SOURCE[] = R"(

in float4 icol0;
out float4 ocol0;

void main()
{
	ocol0 = icol0;
}

)";