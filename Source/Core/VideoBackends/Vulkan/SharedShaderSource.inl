static const char PASSTHROUGH_VERTEX_SHADER_SOURCE[] = R"(

layout(location = 0) in float4 ipos;
layout(location = 5) in float4 icol0;
layout(location = 8) in float3 itex0;

layout(location = 0) out float3 uv0;
layout(location = 1) out float4 col0;

void main()
{
	gl_Position = ipos;
	uv0 = itex0;
	col0 = icol0;
}

)";

static const char PASSTHROUGH_GEOMETRY_SHADER_SOURCE[] = R"(

layout(triangles) in;
layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

in VertexData
{
	float3 uv0;
	float4 col0;
} in_data[];

out VertexData
{
	float3 uv0;
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
			out_data.uv0 = float3(in_data[i].uv0.xy, float(j));
			out_data.col0 = in_data[i].col0;
			EmitVertex();
		}
		EndPrimitive();
	}
}

)";

static const char SCREEN_QUAD_VERTEX_SHADER_SOURCE[] = R"(

layout(location = 0) out float3 uv0;

void main()
{
    /*
     * id	&1 &2	clamp	*2-1
     * 0	0 0	0 0	-1 -1	TL
     * 1	1 0	1 0	1 -1	TR
     * 2	0 2	0 1	-1 1	BL
     * 3	1 2	1 1	1 1	BR
     */
    vec2 rawpos = float2(float(gl_VertexID & 1), clamp(float(gl_VertexID & 2), 0.0f, 1.0f));
    gl_Position = float4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);
    uv0 = float3(rawpos, 0.0f);
}

)";

static const char SCREEN_QUAD_GEOMETRY_SHADER_SOURCE[] = R"(

layout(triangles) in;
layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

in VertexData
{
	float3 uv0;
} in_data[];

out VertexData
{
	float3 uv0;
} out_data;

void main()
{
	for (int j = 0; j < EFB_LAYERS; j++)
	{
		for (int i = 0; i < 3; i++)
		{
			gl_Layer = j;
			gl_Position = gl_in[i].gl_Position;
			out_data.uv0 = float3(in_data[i].uv0.xy, float(j));
			EmitVertex();
		}
		EndPrimitive();
	}
}

)";

static const char CLEAR_FRAGMENT_SHADER_SOURCE[] = R"(

layout(location = 0) in float3 uv0;
layout(location = 1) in float4 col0;
layout(location = 0) out float4 ocol0;

void main()
{
	ocol0 = col0;
}

)";

static const char COPY_FRAGMENT_SHADER_SOURCE[] = R"(

layout(set = 1, binding = 0) uniform sampler2DArray samp0;

layout(location = 0) in float3 uv0;
layout(location = 1) in float4 col0;
layout(location = 0) out float4 ocol0;

void main()
{
	ocol0 = texture(samp0, uv0);
}

)";

static const char COLOR_MATRIX_FRAGMENT_SHADER_SOURCE[] = R"(

SAMPLER_BINDING(0) uniform sampler2DArray samp0;

layout(std140, set = 0, binding = 2) uniform PSBlock
{
	vec4 colmat[7];
};

layout(location = 0) in vec3 uv0;
layout(location = 1) in vec4 col0;
layout(location = 0) out vec4 ocol0;

void main()
{
	float4 texcol = texture(samp0, uv0);
	texcol = round(texcol * colmat[5]) * colmat[6];
	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];
}

)";

static const char DEPTH_MATRIX_FRAGMENT_SHADER_SOURCE[] = R"(

SAMPLER_BINDING(0) uniform sampler2DArray samp0;

layout(std140, set = 0, binding = 2) uniform PSBlock
{
	vec4 colmat[5];
};

layout(location = 0) in vec3 uv0;
layout(location = 1) in vec4 col0;
layout(location = 0) out vec4 ocol0;

void main()
{
	vec4 texcol = texture(samp0, uv0);
	int depth = int((1.0 - texcol.x) * 16777216.0);

	// Convert to Z24 format
	ivec4 workspace;
	workspace.r = (depth >> 16) & 255;
	workspace.g = (depth >> 8) & 255;
	workspace.b = depth & 255;

	// Convert to Z4 format
	workspace.a = (depth >> 16) & 0xF0;

	// Normalize components to [0.0..1.0]
	texcol = vec4(workspace) / 255.0;

	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];
}

)";

