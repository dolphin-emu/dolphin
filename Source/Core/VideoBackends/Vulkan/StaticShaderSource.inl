static const char PASSTHROUGH_VERTEX_SHADER_SOURCE[] = R"(

layout(location = 0) in float4 ipos;
layout(location = 5) in float4 icol0;
layout(location = 8) in float4 itex0;

layout(location = 0) out float4 uv0;
layout(location = 1) out float4 col0;

#if EFB_LAYERS > 1
	flat out int layer;
#endif

void main()
{
	gl_Position = ipos;
	uv0 = itex0;
	col0 = icol0;
	
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
	float4 uv0;
	float4 col0;
} in_data[];

out VertexData
{
	float4 uv0;
	float4 col0;
} out_data;

void main()
{
	for (int j = 0; j < EFB_LAYERS; j++)
	{
		for (int i = 0; i < 3; i++)
		{
			gl_Layer = j;
			gl_Position = gl_in[i].gl_Position;
			out_data.uv0 = in_data[i].uv0;
			out_data.col0 = in_data[i].col0;
			EmitVertex();
		}
		EndPrimitive();
	}
}

)";

static const char SCREEN_QUAD_VERTEX_SHADER_SOURCE[] = R"(

layout(location = 0) out float4 uv0;

#if EFB_LAYERS > 1
	flat out int layer;
#endif

void main()
{
	vec2 rawpos = float2(float(gl_VertexID & 1), float(gl_VertexID & 2));
	gl_Position = float4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);
	uv0 = float4(rawpos, 0.0f, 0.0f);

	#if EFB_LAYERS > 1
		layer = 0;
	#endif
}

)";

static const char CLEAR_FRAGMENT_SHADER_SOURCE[] = R"(

layout(location = 0) in float4 uv0;
layout(location = 1) in float4 col0;
layout(location = 0) out float4 ocol0;

void main()
{
	ocol0 = col0;
}

)";

static const char COPY_FRAGMENT_SHADER_SOURCE[] = R"(

layout(set = 1, binding = 0) uniform sampler2DArray samp0;

layout(location = 0) in float2 uv0;
layout(location = 1) in float4 col0;
layout(location = 0) out float4 ocol0;

void main()
{
	ocol0 = texture(samp0, float3(uv0, 0.0f));
}

)";
