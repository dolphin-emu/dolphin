// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "SOIL/SOIL.h"
#include "VideoBackends/D3D/AvatarDrawer.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/tiny_obj_loader.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VRGameMatrices.h"

#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"

extern float s_fViewTranslationVector[3];
extern EFBRectangle g_final_screen_region;

namespace DX11
{

	static const char AVATAR_DRAWER_VS[] =
		"// dolphin-emu AvatarDrawer vertex shader\n"

		"cbuffer MatrixBuffer : register(b0)\n"
		"{\n"
		"matrix worldMatrix;\n"
		"matrix viewMatrix;\n"
		"matrix projectionMatrix;\n"
		"float4 color;\n"
		"}\n"

		"struct Output\n"
		"{\n"
		"float4 position : SV_POSITION;\n"
		"float4 color : COLOR;\n"
		"float3 uv: TEXCOORD;\n"
		"};\n"

		"Output main(in float4 position : POSITION, in float3 normal : NORMAL, in float2 uv : TEXCOORD)\n"
		"{\n"
		"Output o;\n"
		"float3 light = float3(-0.2, -0.8, -0.3);\n"
		// Change the position vector to be 4 units for proper matrix calculations.
		"position.w = 1.0;\n"

		// Calculate the position of the vertex against the world, view, and projection matrices.
		"o.position = mul(position, worldMatrix);\n"
		"o.position = mul(o.position, viewMatrix);\n"
		"o.position = mul(o.position, projectionMatrix);\n"

		"normal = mul(normal, worldMatrix);\n"
		"normal = -normalize(mul(normal, viewMatrix));\n"

		// calculate the colour and texture mapping
		"o.color = saturate(color * (0.2 + dot(light, normal)));\n"
		"o.color.w = color.w;\n"
		"o.uv = float3(uv.x, uv.y, 0);\n"
		"return o;\n"
		"}\n"
		;

	static const char AVATAR_LINE_DRAWER_VS[] =
		"// dolphin-emu AvatarDrawer line vertex shader\n"

		"cbuffer MatrixBuffer : register(b0)\n"
		"{\n"
		"matrix worldMatrix;\n"
		"matrix viewMatrix;\n"
		"matrix projectionMatrix;\n"
		"float4 color;\n"
		"}\n"

		"struct Output\n"
		"{\n"
		"float4 position : SV_POSITION;\n"
		"float4 color : COLOR;\n"
		"float3 uv: TEXCOORD;\n"
		"};\n"

		"Output main(in float4 position : POSITION, in float3 normal : NORMAL, in float2 uv : TEXCOORD)\n"
		"{\n"
		"Output o;\n"
		// Change the position vector to be 4 units for proper matrix calculations.
		"position.w = 1.0;\n"

		// Calculate the position of the vertex against the world, view, and projection matrices.
		"o.position = mul(position, worldMatrix);\n"
		"o.position = mul(o.position, viewMatrix);\n"
		"o.position = mul(o.position, projectionMatrix);\n"

		// calculate the colour and texture mapping
		"o.color = color;\n"
		"o.uv = float3(uv.x, uv.y, 0);\n"
		"return o;\n"
		"}\n"
		;

	static const char AVATAR_DRAWER_PS[] =
		"// dolphin-emu AvatarDrawer pixel shader\n"

		"void main(out float4 ocol0 : SV_Target, in float4 position : SV_Position, in float4 color : COLOR, "
		"in float3 uv : TEXCOORD, in uint layer : SV_RenderTargetArrayIndex)\n"
		"{\n"
		"ocol0 = color;\n"
		"}\n"
		;

	static const char AVATAR_DRAWER_TEXTURE_PS[] =
		"// dolphin-emu AvatarDrawer texture pixel shader\n"

		"SamplerState samp : register(s0);\n"
		"Texture2D Tex : register(t0);\n"

		"void main(out float4 ocol0 : SV_Target, in float4 position : SV_Position, in float4 color : COLOR, "
		"in float3 uv : TEXCOORD, in uint layer : SV_RenderTargetArrayIndex)\n"
		"{\n"
		"float4 textureColor;\n"
		"textureColor = Tex.Sample(samp, uv.xy);\n"
		"ocol0 = textureColor * color;\n"
		"}\n"
		;

	static const D3D11_INPUT_ELEMENT_DESC QUAD_LAYOUT_DESC[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};


	AvatarDrawer::AvatarDrawer()
		: m_vertex_shader_params(nullptr),
		m_vertex_buffer(nullptr), m_line_vertex_buffer(nullptr), m_index_buffer(nullptr), m_vertex_layout(nullptr),
		m_vertex_shader(nullptr), m_line_vertex_shader(nullptr), m_geometry_shader(nullptr), m_line_geometry_shader(nullptr), m_color_pixel_shader(nullptr), m_texture_pixel_shader(nullptr),
		m_avatar_blend_state(nullptr), m_avatar_depth_state(nullptr),
		m_avatar_rast_state(nullptr), m_avatar_line_rast_state(nullptr), m_avatar_sampler(nullptr)
	{ }

	void AvatarDrawer::Init()
	{
		HRESULT hr;
		D3D11_BUFFER_DESC bd;

		// set up vertices for drawing lines (laser pointer) and rectangles
		m_line_vertices = new VERTEX[34];
		ZeroMemory(m_line_vertices, sizeof(m_line_vertices[0]) * 34);
		m_line_vertices[0].X = 0; m_line_vertices[0].Y = 0; m_line_vertices[0].Z = 0;
		m_line_vertices[1].X = 0; m_line_vertices[1].Y = 0; m_line_vertices[1].Z = -1000; // -ve = forwards
		m_line_vertices[0].nx = 1; m_line_vertices[0].ny = 0; m_line_vertices[0].nz = 0;
		m_line_vertices[0].u = 0;
		m_line_vertices[1].u = 1;
		// rectangle
		for (size_t i = 0; i < 8; ++i)
		{
			m_line_vertices[2 + i] = m_line_vertices[0];
			m_line_vertices[2 + i].nx = 0;
			m_line_vertices[2 + i].nz = 1;
			m_line_vertices[2 + i].u = (float)(i % 2);
		}
		m_line_vertices[3].X = 1;
		m_line_vertices[4] = m_line_vertices[3];
		m_line_vertices[5].X = 1; m_line_vertices[5].Y = 1;
		m_line_vertices[6] = m_line_vertices[5];
		m_line_vertices[7].X = 0; m_line_vertices[7].Y = 1;
		m_line_vertices[8] = m_line_vertices[7];
		m_line_vertices[9] = m_line_vertices[2];
		for (int i = 0; i < 8; ++i)
		{
			m_line_vertices[10 + i] = m_line_vertices[2 + i];
			m_line_vertices[10 + i].Z = 1.0f;
			m_line_vertices[18 + i] = m_line_vertices[2 + i];
			m_line_vertices[18 + i].Z = -1.0f;
		}
		m_line_vertices[26] = m_line_vertices[10];
		m_line_vertices[27] = m_line_vertices[18];
		m_line_vertices[28] = m_line_vertices[10 + 2];
		m_line_vertices[29] = m_line_vertices[18 + 2];
		m_line_vertices[30] = m_line_vertices[10 + 4];
		m_line_vertices[31] = m_line_vertices[18 + 4];
		m_line_vertices[32] = m_line_vertices[10 + 6];
		m_line_vertices[33] = m_line_vertices[18 + 6];

#ifdef HAVE_OPENVR
		// Load Vive model
		if (g_has_steamvr)
		{
			vr::RenderModel_t *pModel = nullptr;
			vr::EVRRenderModelError error;
			const char *openvrpath = vr::VR_RuntimePath();
			char path[MAX_PATH] = "";
			sprintf(path, "%s\\resources\\rendermodels\\vr_controller_vive_1_5\\vr_controller_vive_1_5.obj", openvrpath);
			while (true)
			{
				error = vr::VRRenderModels()->LoadRenderModel_Async(path, &pModel);
				if (error != vr::VRRenderModelError_Loading)
					break;
				Sleep(2);
			}
			vr::RenderModel_TextureMap_t *pTexture = nullptr;
			while (1)
			{
				error = vr::VRRenderModels()->LoadTexture_Async(pModel->diffuseTextureId, &pTexture);
				if (error != vr::VRRenderModelError_Loading)
					break;
				Sleep(2);
			}

			int width, height, channels;
			u8 *img = SOIL_load_image((File::GetSysDirectory() + "\\Resources\\Textures\\vive_wiimote.png").c_str(), &width, &height, &channels, SOIL_LOAD_RGBA);

			if (error != vr::VRRenderModelError_None)
			{
				NOTICE_LOG(VR, "Unable to load render model %s - %s\n", path, vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(error));
				m_vertex_count = 0;
				m_index_count = 0;
				m_scale = 1;
				m_texture = nullptr;
			}
			else
			{
				NOTICE_LOG(VR, "Loaded render model %s\n", path);
				m_vertex_count = pModel->unVertexCount * sizeof(VERTEX) / sizeof(float);
				m_index_count = pModel->unTriangleCount * 3;
				m_vertices = new float[m_vertex_count];
				m_indices = new u16[m_index_count];
				memcpy(m_vertices, pModel->rVertexData, m_vertex_count * sizeof(float));
				memcpy(m_indices, pModel->rIndexData, m_index_count * sizeof(u16));
				m_scale = 1;
				if (img)
				{
					D3D11_SUBRESOURCE_DATA srd = { img, width * 4, width * height * 4 };
					m_texture = DX11::D3DTexture2D::Create(width, height, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &srd);
				}
				else
				{
					D3D11_SUBRESOURCE_DATA srd = { pTexture->rubTextureMapData, pTexture->unWidth * 4, pTexture->unWidth * pTexture->unHeight * 4 };
					m_texture = DX11::D3DTexture2D::Create(pTexture->unWidth, pTexture->unHeight, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &srd);
				}
			}
			vr::VRRenderModels()->FreeRenderModel(pModel);
			vr::VRRenderModels()->FreeTexture(pTexture);
			SOIL_free_image_data(img);
		}
		else
#endif
		// Load Hydra model
		{
			m_scale = 0.0025f;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;
			std::string path = File::GetSysDirectory() + "\\Resources\\Models\\HydraModel.obj";
			std::string path2 = File::GetSysDirectory() + "\\Resources\\Models";
			std::string err = tinyobj::LoadObj(shapes, materials, path.c_str(), nullptr);
			GetModelSize(&shapes[0], &hydra_size[0], &hydra_size[1], &hydra_size[2], &hydra_mid[0], &hydra_mid[1], &hydra_mid[2]);
			m_vertex_count = shapes[0].mesh.vertices.size();
			m_index_count = shapes[0].mesh.indices.size();
			m_vertices = new float[m_vertex_count];
			m_indices = new u16[m_index_count];
			for (size_t i = 0; i < m_vertex_count; ++i)
			{
				m_vertices[i] = shapes[0].mesh.vertices[i];
			}
			for (size_t i = 0; i < m_index_count; ++i)
			{
				m_indices[i] = shapes[0].mesh.indices[i];
			}
			// debug model loading
			DEBUG_LOG(VR, "LoadObj = '%s'", err);
			DEBUG_LOG(VR, "# of shapes: %d,  # of materials: %d", shapes.size(), materials.size());
			DEBUG_LOG(VR, "Size=%f, %f, %f,   Midpoint=%f, %f, %f", hydra_size[0], hydra_size[1], hydra_size[2], hydra_mid[0], hydra_mid[1], hydra_mid[2]);
			for (size_t i = 0; i < shapes.size(); i++) {
				DEBUG_LOG(VR, "shape[%ld].name = %s", i, shapes[i].name.c_str());
				DEBUG_LOG(VR, "Size of shape[%ld].indices: %ld", i, shapes[i].mesh.indices.size());
				DEBUG_LOG(VR, "Size of shape[%ld].material_ids: %ld", i, shapes[i].mesh.material_ids.size());
				DEBUG_LOG(VR, "shape[%ld].vertices: %ld", i, shapes[i].mesh.vertices.size());
				DEBUG_LOG(VR, "shape[%ld].positions: %ld", i, shapes[i].mesh.positions.size());
				DEBUG_LOG(VR, "shape[%ld].normals: %ld", i, shapes[i].mesh.normals.size());
				DEBUG_LOG(VR, "shape[%ld].texcoords: %ld", i, shapes[i].mesh.texcoords.size());
			}

			for (size_t i = 0; i < 0; i++) {
				DEBUG_LOG(VR, "shape[%ld].name = %s\n", i, shapes[i].name.c_str());
				DEBUG_LOG(VR, "Size of shape[%ld].indices: %ld\n", i, shapes[i].mesh.indices.size());
				DEBUG_LOG(VR, "Size of shape[%ld].material_ids: %ld\n", i, shapes[i].mesh.material_ids.size());
				assert((shapes[i].mesh.indices.size() % 3) == 0);
				for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++) {
					DEBUG_LOG(VR, "  idx[%ld] = %d, %d, %d. mat_id = %d\n", f, shapes[i].mesh.indices[3 * f + 0], shapes[i].mesh.indices[3 * f + 1], shapes[i].mesh.indices[3 * f + 2], shapes[i].mesh.material_ids[f]);
				}

				DEBUG_LOG(VR, "shape[%ld].vertices: %ld\n", i, shapes[i].mesh.positions.size());
				assert((shapes[i].mesh.positions.size() % 3) == 0);
				for (size_t v = 0; v < shapes[i].mesh.positions.size() / 3; v++) {
					DEBUG_LOG(VR, "  v[%ld] = (%f, %f, %f)\n", v,
						shapes[i].mesh.positions[3 * v + 0],
						shapes[i].mesh.positions[3 * v + 1],
						shapes[i].mesh.positions[3 * v + 2]);
				}
			}
			for (size_t i = 0; i < materials.size(); i++) {
				DEBUG_LOG(VR, "material[%ld].name = %s\n", i, materials[i].name.c_str());
				DEBUG_LOG(VR, "  material.Ka = (%f, %f ,%f)\n", materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
				DEBUG_LOG(VR, "  material.Kd = (%f, %f ,%f)\n", materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
				DEBUG_LOG(VR, "  material.Ks = (%f, %f ,%f)\n", materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
				DEBUG_LOG(VR, "  material.Tr = (%f, %f ,%f)\n", materials[i].transmittance[0], materials[i].transmittance[1], materials[i].transmittance[2]);
				DEBUG_LOG(VR, "  material.Ke = (%f, %f ,%f)\n", materials[i].emission[0], materials[i].emission[1], materials[i].emission[2]);
				DEBUG_LOG(VR, "  material.Ns = %f\n", materials[i].shininess);
				DEBUG_LOG(VR, "  material.Ni = %f\n", materials[i].ior);
				DEBUG_LOG(VR, "  material.dissolve = %f\n", materials[i].dissolve);
				DEBUG_LOG(VR, "  material.illum = %d\n", materials[i].illum);
				DEBUG_LOG(VR, "  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
				DEBUG_LOG(VR, "  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
				DEBUG_LOG(VR, "  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
				DEBUG_LOG(VR, "  material.map_Ns = %s\n", materials[i].normal_texname.c_str());
				std::map<std::string, std::string>::const_iterator it(materials[i].unknown_parameter.begin());
				std::map<std::string, std::string>::const_iterator itEnd(materials[i].unknown_parameter.end());
				for (; it != itEnd; it++) {
					DEBUG_LOG(VR, "  material.%s = %s\n", it->first.c_str(), it->second.c_str());
				}
				DEBUG_LOG(VR, "\n");
			}
			m_texture = nullptr;
		}

		// Create vertex buffer
		bd = CD3D11_BUFFER_DESC(m_vertex_count * sizeof(float), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC);
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA srd = { m_vertices, 0, 0 };
		hr = D3D::device->CreateBuffer(&bd, &srd, &m_vertex_buffer);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer vertex buffer");
		D3D::SetDebugObjectName(m_vertex_buffer, "AvatarDrawer vertex buffer");

		// Create line vertex buffer
		bd = CD3D11_BUFFER_DESC(34 * sizeof(VERTEX), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC);
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		srd = { m_line_vertices, 0, 0 };
		hr = D3D::device->CreateBuffer(&bd, &srd, &m_line_vertex_buffer);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer line vertex buffer");
		D3D::SetDebugObjectName(m_vertex_buffer, "AvatarDrawer line vertex buffer");

		// Create index buffer
		bd = CD3D11_BUFFER_DESC(m_index_count * sizeof(u16), D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC);
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		srd = { m_indices, 0, 0 };
		hr = D3D::device->CreateBuffer(&bd, &srd, &m_index_buffer);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer index buffer");
		D3D::SetDebugObjectName(m_index_buffer, "AvatarDrawer index buffer");

		// Create vertex shader
		D3DBlob* bytecode = nullptr;
		if (!D3D::CompileVertexShader(AVATAR_DRAWER_VS, &bytecode))
		{
			ERROR_LOG(VR, "AvatarDrawer vertex shader failed to compile");
			return;
		}
		hr = D3D::device->CreateVertexShader(bytecode->Data(), bytecode->Size(), nullptr, &m_vertex_shader);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer vertex shader");
		D3D::SetDebugObjectName(m_vertex_shader, "AvatarDrawer vertex shader");

		// Create input layout for vertex buffer using bytecode from vertex shader
		hr = D3D::device->CreateInputLayout(QUAD_LAYOUT_DESC,
			sizeof(QUAD_LAYOUT_DESC) / sizeof(D3D11_INPUT_ELEMENT_DESC),
			bytecode->Data(), bytecode->Size(), &m_vertex_layout);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer vertex layout");
		D3D::SetDebugObjectName(m_vertex_layout, "AvatarDrawer vertex layout");

		bytecode->Release();

		// Create line vertex shader
		bytecode = nullptr;
		if (!D3D::CompileVertexShader(AVATAR_LINE_DRAWER_VS, &bytecode))
		{
			ERROR_LOG(VR, "AvatarDrawer line vertex shader failed to compile");
			return;
		}
		hr = D3D::device->CreateVertexShader(bytecode->Data(), bytecode->Size(), nullptr, &m_line_vertex_shader);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer line vertex shader");
		D3D::SetDebugObjectName(m_line_vertex_shader, "AvatarDrawer line vertex shader");
		bytecode->Release();

		// Create constant buffer for uploading params to shaders

		bd = CD3D11_BUFFER_DESC(sizeof(VertexShaderParams),
			D3D11_BIND_CONSTANT_BUFFER);
		hr = D3D::device->CreateBuffer(&bd, nullptr, &m_vertex_shader_params);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer params buffer");
		D3D::SetDebugObjectName(m_vertex_shader_params, "AvatarDrawer params buffer");

		// Create triangle geometry shader
		ShaderCode code;
		code = GenerateAvatarGeometryShaderCode(PRIMITIVE_TRIANGLES, API_D3D);
		if (!D3D::CompileGeometryShader(code.GetBuffer(), &bytecode))
		{
			ERROR_LOG(VR, "AvatarDrawer geometry shader failed to compile");
			return;
		}
		hr = D3D::device->CreateGeometryShader(bytecode->Data(), bytecode->Size(), nullptr, &m_geometry_shader);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer geometry shader");
		D3D::SetDebugObjectName(m_geometry_shader, "AvatarDrawer geometry shader");
		bytecode->Release();
		// Create line geometry shader
		code = GenerateAvatarGeometryShaderCode(PRIMITIVE_LINES, API_D3D);
		if (!D3D::CompileGeometryShader(code.GetBuffer(), &bytecode))
		{
			ERROR_LOG(VR, "AvatarDrawer geometry shader failed to compile");
			return;
		}
		hr = D3D::device->CreateGeometryShader(bytecode->Data(), bytecode->Size(), nullptr, &m_line_geometry_shader);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer line geometry shader");
		D3D::SetDebugObjectName(m_line_geometry_shader, "AvatarDrawer line geometry shader");
		bytecode->Release();

		// Create pixel shader

		m_texture_pixel_shader = D3D::CompileAndCreatePixelShader(AVATAR_DRAWER_TEXTURE_PS);
		if (!m_texture_pixel_shader)
		{
			ERROR_LOG(VR, "AvatarDrawer texture pixel shader failed to compile");
			return;
		}
		D3D::SetDebugObjectName(m_texture_pixel_shader, "AvatarDrawer texture pixel shader");

		m_color_pixel_shader = D3D::CompileAndCreatePixelShader(AVATAR_DRAWER_PS);
		if (!m_color_pixel_shader)
		{
			ERROR_LOG(VR, "AvatarDrawer color pixel shader failed to compile");
			return;
		}
		D3D::SetDebugObjectName(m_color_pixel_shader, "AvatarDrawer color pixel shader");

		// Create blend state

		D3D11_BLEND_DESC bld = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
		hr = D3D::device->CreateBlendState(&bld, &m_avatar_blend_state);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer blend state");
		D3D::SetDebugObjectName(m_avatar_blend_state, "AvatarDrawer blend state");

		// Create depth state

		D3D11_DEPTH_STENCIL_DESC dsd = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
		dsd.DepthEnable = TRUE;
		dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = D3D::device->CreateDepthStencilState(&dsd, &m_avatar_depth_state);
		CHECK(SUCCEEDED(hr), "AvatarDrawer depth state");
		D3D::SetDebugObjectName(m_avatar_depth_state, "AvatarDrawer depth state");

		// Create rasterizer state

		D3D11_RASTERIZER_DESC rd = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
		rd.CullMode = D3D11_CULL_BACK;
		rd.FrontCounterClockwise = TRUE;
		rd.DepthClipEnable = FALSE;
		hr = D3D::device->CreateRasterizerState(&rd, &m_avatar_rast_state);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer rasterizer state");
		D3D::SetDebugObjectName(m_avatar_rast_state, "AvatarDrawer rast state");

		// Create line rasterizer state

		rd = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
		rd.CullMode = D3D11_CULL_NONE;
		rd.FrontCounterClockwise = TRUE;
		rd.DepthClipEnable = FALSE;
		hr = D3D::device->CreateRasterizerState(&rd, &m_avatar_line_rast_state);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer line rasterizer state");
		D3D::SetDebugObjectName(m_avatar_line_rast_state, "AvatarDrawer line rast state");

		// Create texture sampler

		D3D11_SAMPLER_DESC sd = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
		// FIXME: Should we really use point sampling here?
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		hr = D3D::device->CreateSamplerState(&sd, &m_avatar_sampler);
		CHECK(SUCCEEDED(hr), "create avatar texture sampler");
		D3D::SetDebugObjectName(m_avatar_sampler, "avatar texture sampler");
	}

	void AvatarDrawer::Shutdown()
	{
		SAFE_RELEASE(m_texture);
		SAFE_RELEASE(m_avatar_sampler);
		SAFE_RELEASE(m_avatar_rast_state);
		SAFE_RELEASE(m_avatar_line_rast_state);
		SAFE_RELEASE(m_avatar_depth_state);
		SAFE_RELEASE(m_avatar_blend_state);
		SAFE_RELEASE(m_color_pixel_shader);
		SAFE_RELEASE(m_texture_pixel_shader);
		SAFE_RELEASE(m_geometry_shader);
		SAFE_RELEASE(m_line_geometry_shader);
		SAFE_RELEASE(m_vertex_layout);
		SAFE_RELEASE(m_vertex_shader);
		SAFE_RELEASE(m_line_vertex_shader);
		SAFE_RELEASE(m_vertex_buffer);
		SAFE_RELEASE(m_line_vertex_buffer);
		SAFE_RELEASE(m_index_buffer);
		SAFE_RELEASE(m_vertex_shader_params);
	}

	void AvatarDrawer::DrawLine(float *pos, Matrix33 &m, float r, float g, float b)
	{
		params.color[0] = r;
		params.color[1] = g;
		params.color[2] = b;
		params.color[3] = 1.0f;
		// world matrix
		Matrix44 world, rotation, location;
		rotation = m;
		Matrix44::Translate(location, pos);
		world = rotation * location;
		// copy matrices into buffer
		memcpy(params.world, world.data, 16 * sizeof(float));
		D3D::context->UpdateSubresource(m_vertex_shader_params, 0, nullptr, &params, 0, 0);

		D3D::stateman->SetVertexConstants(m_vertex_shader_params);
		D3D::stateman->SetPixelConstants(m_vertex_shader_params);
		D3D::stateman->SetTexture(0, nullptr);
		D3D::stateman->SetSampler(0, m_avatar_sampler);

		// Draw!
		D3D::stateman->Apply();
		D3D::context->Draw(2, 0);

		// Clean up state
		D3D::stateman->SetSampler(0, nullptr);
		D3D::stateman->SetTexture(0, nullptr);
		D3D::stateman->SetPixelConstants(nullptr);
		D3D::stateman->SetVertexConstants(nullptr);
	}

	void AvatarDrawer::DrawBox(int kind, float *pos, Matrix33 &m, float r, float g, float b)
	{
		// view matrix
		if (!CalculateViewMatrix(kind))
			return;
		// colour
		params.color[0] = r;
		params.color[1] = g;
		params.color[2] = b;
		params.color[3] = 1.0f;
		// world matrix
		Matrix44 world, rotation, location;
		rotation = m;
		Matrix44::Translate(location, pos);
		world = rotation * location;
		// copy matrices into buffer
		memcpy(params.world, world.data, 16 * sizeof(float));
		D3D::context->UpdateSubresource(m_vertex_shader_params, 0, nullptr, &params, 0, 0);

		D3D::stateman->SetVertexConstants(m_vertex_shader_params);
		D3D::stateman->SetPixelConstants(m_vertex_shader_params);
		D3D::stateman->SetTexture(0, nullptr);
		D3D::stateman->SetSampler(0, m_avatar_sampler);

		// Draw!
		D3D::stateman->Apply();
		if (kind == 0)
			D3D::context->Draw(8, 2);
		else
			D3D::context->Draw(32, 2);

		// Clean up state
		D3D::stateman->SetSampler(0, nullptr);
		D3D::stateman->SetTexture(0, nullptr);
		D3D::stateman->SetPixelConstants(nullptr);
		D3D::stateman->SetVertexConstants(nullptr);
	}


	bool AvatarDrawer::CalculateViewMatrix(int kind)
	{
		bool bStuckToHead = false, bIsSkybox = false, bIsPerspective = false, bHasWidest = (vr_widest_3d_HFOV > 0);
		bool bIsHudElement = false, bIsOffscreen = false, bAspectWide = true, bNoForward = false, bShowAim = false;
		float UnitsPerMetre = 1.0f;

		// Show HUD
		if (kind == 1)
		{
			if (!bHasWidest)
				return false;
		}
		// Show Aim
		else if (kind == 0)
		{
			if (bHasWidest)
				bShowAim = true;
		}
		// Show 2D
		else if (kind == 2)
		{
			bHasWidest = false;
		}

		float zoom_forward = 0.0f;
		if (vr_widest_3d_HFOV <= g_ActiveConfig.fMinFOV && bHasWidest)
		{
			zoom_forward = g_ActiveConfig.fAimDistance * tanf(DEGREES_TO_RADIANS(g_ActiveConfig.fMinFOV) / 2) / tanf(DEGREES_TO_RADIANS(vr_widest_3d_HFOV) / 2);
			zoom_forward -= g_ActiveConfig.fAimDistance;
		}

		float hfov = 0, vfov = 0;
		hfov = vr_widest_3d_HFOV;
		vfov = vr_widest_3d_VFOV;

		//VR Headtracking and leaning back compensation
		Matrix44 rotation_matrix;
		Matrix44 lean_back_matrix;
		Matrix44 camera_pitch_matrix;
		if (bStuckToHead)
		{
			Matrix44::LoadIdentity(rotation_matrix);
			Matrix44::LoadIdentity(lean_back_matrix);
			Matrix44::LoadIdentity(camera_pitch_matrix);
		}
		else
		{
			// head tracking
			if (g_ActiveConfig.bOrientationTracking)
			{
				VR_UpdateHeadTrackingIfNeeded();
				rotation_matrix = g_head_tracking_matrix;
			}
			else
			{
				Matrix44::LoadIdentity(rotation_matrix);
			}

			Matrix33 pitch_matrix33;

			// leaning back
			float extra_pitch = -g_ActiveConfig.fLeanBackAngle;
			Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
			lean_back_matrix = pitch_matrix33;

			// camera pitch
			if ((g_ActiveConfig.bStabilizePitch || g_ActiveConfig.bStabilizeRoll || g_ActiveConfig.bStabilizeYaw) && g_ActiveConfig.bCanReadCameraAngles && (g_ActiveConfig.iMotionSicknessSkybox != 2 || !bIsSkybox))
			{
				if (!g_ActiveConfig.bStabilizePitch)
				{
					Matrix44 user_pitch44;
					Matrix44 roll_and_yaw_matrix;

					if (bIsPerspective || bHasWidest)
						extra_pitch = g_ActiveConfig.fCameraPitch;
					else
						extra_pitch = g_ActiveConfig.fScreenPitch;
					Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
					user_pitch44 = pitch_matrix33;
					roll_and_yaw_matrix = g_game_camera_rotmat;
					camera_pitch_matrix = user_pitch44 * roll_and_yaw_matrix;
				}
				else
				{
					camera_pitch_matrix = g_game_camera_rotmat;
				}
			}
			else
			{
				if (xfmem.projection.type == GX_PERSPECTIVE || bHasWidest)
					extra_pitch = g_ActiveConfig.fCameraPitch;
				else
					extra_pitch = g_ActiveConfig.fScreenPitch;
				Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
				camera_pitch_matrix = pitch_matrix33;
			}
		}

		// Position matrices
		Matrix44 head_position_matrix, free_look_matrix, camera_forward_matrix, camera_position_matrix;
		if (bStuckToHead || bIsSkybox)
		{
			Matrix44::LoadIdentity(head_position_matrix);
			Matrix44::LoadIdentity(free_look_matrix);
			Matrix44::LoadIdentity(camera_position_matrix);
		}
		else
		{
			float pos[3];
			// head tracking
			if (g_ActiveConfig.bPositionTracking)
			{
				for (int i = 0; i < 3; ++i)
					pos[i] = g_head_tracking_position[i] * UnitsPerMetre;
				Matrix44::Translate(head_position_matrix, pos);
			}
			else
			{
				Matrix44::LoadIdentity(head_position_matrix);
			}

			// freelook camera position
			for (int i = 0; i < 3; ++i)
				pos[i] = s_fViewTranslationVector[i] * UnitsPerMetre;
			Matrix44::Translate(free_look_matrix, pos);

			// camera position stabilisation
			if (g_ActiveConfig.bStabilizeX || g_ActiveConfig.bStabilizeY || g_ActiveConfig.bStabilizeZ)
			{
				for (int i = 0; i < 3; ++i)
					pos[i] = -g_game_camera_pos[i] * UnitsPerMetre;
				Matrix44::Translate(camera_position_matrix, pos);
			}
			else
			{
				Matrix44::LoadIdentity(camera_position_matrix);
			}
		}

		Matrix44 look_matrix;
		if (bIsPerspective && !bIsHudElement && !bIsOffscreen)
		{
			// Transformations must be applied in the following order for VR:
			// camera position stabilisation
			// camera forward
			// camera pitch
			// free look
			// leaning back
			// head position tracking
			// head rotation tracking
			if (bNoForward || bIsSkybox || bStuckToHead)
			{
				Matrix44::LoadIdentity(camera_forward_matrix);
			}
			else
			{
				float pos[3];
				pos[0] = 0;
				pos[1] = 0;
				pos[2] = (g_ActiveConfig.fCameraForward + zoom_forward) * UnitsPerMetre;
				Matrix44::Translate(camera_forward_matrix, pos);
			}

			look_matrix = camera_forward_matrix * camera_position_matrix * camera_pitch_matrix * free_look_matrix * lean_back_matrix * head_position_matrix * rotation_matrix;
		}
		else
		{
			float HudWidth, HudHeight, HudThickness, HudDistance, HudUp, CameraForward, AimDistance;

			// 2D Screen
			if (!bHasWidest)
			{
				HudThickness = g_ActiveConfig.fScreenThickness * UnitsPerMetre;
				HudDistance = g_ActiveConfig.fScreenDistance * UnitsPerMetre;
				HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
				HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
				// NES games are supposed to be 1.175:1 (16:13.62) even though VC normally renders them as 16:9
				// http://forums.nesdev.com/viewtopic.php?t=8063
				if (g_is_nes)
					HudWidth = HudHeight * 1.175f;
				else if (bAspectWide)
					HudWidth = HudHeight * (float)16 / 9;
				else
					HudWidth = HudHeight * (float)4 / 3;
				CameraForward = 0;
				HudUp = g_ActiveConfig.fScreenUp * UnitsPerMetre;
				AimDistance = HudDistance;
			}
			else
				// HUD over 3D world
			{
				// The HUD distance might have been carefully chosen to line up with objects, so we should scale it with the world
				// But we can't make the HUD too close or it's hard to look at, and we should't make the HUD too far or it stops looking 3D
				const float MinHudDistance = 0.28f, MaxHudDistance = 3.00f; // HUD shouldn't go closer than 28 cm when shrinking scale, or further than 3m when growing
				float HUDScale = g_ActiveConfig.fScale;
				if (HUDScale < 1.0f && g_ActiveConfig.fHudDistance >= MinHudDistance && g_ActiveConfig.fHudDistance * HUDScale < MinHudDistance)
					HUDScale = MinHudDistance / g_ActiveConfig.fHudDistance;
				else if (HUDScale > 1.0f && g_ActiveConfig.fHudDistance <= MaxHudDistance && g_ActiveConfig.fHudDistance * HUDScale > MaxHudDistance)
					HUDScale = MaxHudDistance / g_ActiveConfig.fHudDistance;

				// Give the 2D layer a 3D effect if different parts of the 2D layer are rendered at different z coordinates
				HudThickness = g_ActiveConfig.fHudThickness * HUDScale * UnitsPerMetre;  // the 2D layer is actually a 3D box this many game units thick
				HudDistance = g_ActiveConfig.fHudDistance * HUDScale * UnitsPerMetre;   // depth 0 on the HUD should be this far away

				HudUp = 0;
				if (bNoForward)
					CameraForward = 0;
				else
					CameraForward = (g_ActiveConfig.fCameraForward + zoom_forward) * g_ActiveConfig.fScale * UnitsPerMetre;
				// When moving the camera forward, correct the size of the HUD so that aiming is correct at AimDistance
				AimDistance = g_ActiveConfig.fAimDistance * g_ActiveConfig.fScale * UnitsPerMetre;
				if (AimDistance <= 0)
					AimDistance = HudDistance;
				if (bShowAim)
				{
					HudThickness = 0;
					HudDistance = AimDistance;
					HUDScale = g_ActiveConfig.fScale;
				}
				// Now that we know how far away the box is, and what FOV it should fill, we can work out the width and height in game units
				// Note: the HUD won't line up exactly (except at AimDistance) if CameraForward is non-zero 
				//float HudWidth = 2.0f * tanf(hfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
				//float HudHeight = 2.0f * tanf(vfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
				HudWidth = 2.0f * tanf(DEGREES_TO_RADIANS(hfov / 2.0f)) * HudDistance * (AimDistance + CameraForward) / AimDistance;
				HudHeight = 2.0f * tanf(DEGREES_TO_RADIANS(vfov / 2.0f)) * HudDistance * (AimDistance + CameraForward) / AimDistance;
			}

			float scale[3]; // width, height, and depth of box in game units divided by 2D width, height, and depth 
			float position[3]; // position of front of box relative to the camera, in game units 

			float viewport_scale[2];
			float viewport_offset[2]; // offset as a fraction of the viewport's width
			if (!bIsHudElement && !bIsOffscreen)
			{
				viewport_scale[0] = 1.0f;
				viewport_scale[1] = 1.0f;
				viewport_offset[0] = 0.0f;
				viewport_offset[1] = 0.0f;
			}
			else
			{
				Viewport &v = xfmem.viewport;
				float left, top, width, height;
				left = v.xOrig - v.wd - 342;
				top = v.yOrig + v.ht - 342;
				width = 2 * v.wd;
				height = -2 * v.ht;
				float screen_width = (float)g_final_screen_region.GetWidth();
				float screen_height = (float)g_final_screen_region.GetHeight();
				viewport_scale[0] = width / screen_width;
				viewport_scale[1] = height / screen_height;
				viewport_offset[0] = ((left + (width / 2)) - (0 + (screen_width / 2))) / screen_width;
				viewport_offset[1] = -((top + (height / 2)) - (0 + (screen_height / 2))) / screen_height;
			}

			// 3D HUD elements (may be part of 2D screen or HUD)
			if (bIsPerspective)
			{
				// these are the edges of the near clipping plane in game coordinates
				float left2D = 0;
				float right2D = 1;
				float bottom2D = 1;
				float top2D = 0;
				float zFar2D = 1;
				float zNear2D = -1;
				float zObj = zNear2D + (zFar2D - zNear2D) * g_ActiveConfig.fHud3DCloser;

				left2D *= zObj;
				right2D *= zObj;
				bottom2D *= zObj;
				top2D *= zObj;

				// Scale the width and height to fit the HUD in metres
				if (right2D == left2D) {
					scale[0] = 0;
				}
				else {
					scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
				}
				if (top2D == bottom2D) {
					scale[1] = 0;
				}
				else {
					scale[1] = viewport_scale[1] * HudHeight / (top2D - bottom2D); // note that positive means up in 3D
				}
				// Keep depth the same scale as width, so it looks like a real object
				if (zFar2D == zNear2D) {
					scale[2] = scale[0];
				}
				else {
					scale[2] = scale[0];
				}
				// Adjust the position for off-axis projection matrices, and shifting the 2D screen
				position[0] = scale[0] * (-(right2D + left2D) / 2.0f) + viewport_offset[0] * HudWidth; // shift it right into the centre of the view
				position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight + HudUp; // shift it up into the centre of the view;
																												// Shift it from the old near clipping plane to the HUD distance, and shift the camera forward
				if (!bHasWidest)
					position[2] = scale[2] * zObj - HudDistance;
				else
					position[2] = scale[2] * zObj - HudDistance; // - CameraForward;
			}
			// 2D layer, or 2D viewport (may be part of 2D screen or HUD)
			else
			{
				float left2D = 0;
				float right2D = 1;
				float bottom2D = 1;
				float top2D = 0;
				float zNear2D = -1;
				float zFar2D = 1;

				// for 2D, work out the fraction of the HUD we should fill, and multiply the scale by that
				// also work out what fraction of the height we should shift it up, and what fraction of the width we should shift it left
				// only multiply by the extra scale after adjusting the position?

				if (right2D == left2D) {
					scale[0] = 0;
				}
				else {
					scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
				}
				if (top2D == bottom2D) {
					scale[1] = 0;
				}
				else {
					scale[1] = viewport_scale[1] * HudHeight / (top2D - bottom2D); // note that positive means up in 3D
				}
				if (zFar2D == zNear2D) {
					scale[2] = 0; // The 2D layer was flat, so we make it flat instead of a box to avoid dividing by zero
				}
				else {
					scale[2] = HudThickness / (zFar2D - zNear2D); // Scale 2D z values into 3D game units so it is the right thickness
				}
				position[0] = scale[0] * (-(right2D + left2D) / 2.0f) + viewport_offset[0] * HudWidth; // shift it right into the centre of the view
				position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight + HudUp; // shift it up into the centre of the view;
																												// Shift it from the zero plane to the HUD distance, and shift the camera forward
				if (!bHasWidest)
					position[2] = -HudDistance;
				else
					position[2] = -HudDistance; // - CameraForward;
			}

			Matrix44 scale_matrix, position_matrix;
			Matrix44::Scale(scale_matrix, scale);
			Matrix44::Translate(position_matrix, position);

			// order: scale, position
			look_matrix = scale_matrix * position_matrix * camera_position_matrix * camera_pitch_matrix * free_look_matrix * lean_back_matrix * head_position_matrix * rotation_matrix;
		}

		// copy matrices into buffer
		memcpy(params.view, look_matrix.data, 16 * sizeof(float));
		return true;
	}

	void AvatarDrawer::DrawHydra(float *pos, Matrix33 &m, ControllerStyle cs)
	{
		params.color[3] = 1.0f;
		switch (cs)
		{
			case CS_HYDRA_LEFT:
			case CS_HYDRA_RIGHT:
			case CS_NES_LEFT:
			case CS_NES_RIGHT:
			case CS_SEGA_LEFT:
			case CS_SEGA_RIGHT:
			case CS_GENESIS_LEFT:
			case CS_GENESIS_RIGHT:
			case CS_TURBOGRAFX_LEFT:
			case CS_TURBOGRAFX_RIGHT:
				params.color[0] = 0.1f;
				params.color[1] = 0.1f;
				params.color[2] = 0.1f;
				break;
			case CS_GC_LEFT:
			case CS_GC_RIGHT:
				params.color[0] = 0.3f;
				params.color[1] = 0.28f;
				params.color[2] = 0.5f;
				break;
			case CS_WIIMOTE:
			case CS_WIIMOTE_LEFT:
			case CS_WIIMOTE_RIGHT:
			case CS_CLASSIC_LEFT:
			case CS_CLASSIC_RIGHT:
			case CS_NUNCHUK:
				params.color[0] = 1.0f;
				params.color[1] = 1.0f;
				params.color[2] = 1.0f;
				break;
			case CS_NUNCHUK_UNREAD:
				params.color[0] = 0.8f;
				params.color[1] = 0.8f;
				params.color[2] = 0.8f;
				break;
			case CS_WIIMOTE_IR:
				params.color[0] = 1.0f;
				params.color[1] = 0.8f;
				params.color[2] = 0.8f;
				break;
			case CS_FAMICON_LEFT:
			case CS_SNES_LEFT:
				params.color[0] = 0.75f;
				params.color[1] = 0.75f;
				params.color[2] = 0.75f;
				break;
			case CS_FAMICON_RIGHT:
			case CS_ARCADE_LEFT:
			case CS_ARCADE_RIGHT:
				params.color[0] = 0.4f;
				params.color[1] = 0.0f;
				params.color[2] = 0.0f;
				break;

			case CS_N64_LEFT:
			case CS_N64_RIGHT:
			case CS_SNES_RIGHT:
			default:
				params.color[0] = 0.5f;
				params.color[1] = 0.5f;
				params.color[2] = 0.5f;
				break;
		}
		params.color[3] = 1.0f;
		// world matrix
		Matrix44 world, scale, rotation, scalerot, location, offset;
		float v[3] = { m_scale, m_scale, m_scale };
		rotation = m;
		Matrix44::Scale(scale, v);
		scalerot = scale * rotation;
		float p[3];
		for (int i = 0; i < 3; ++i)
			p[i] = -hydra_mid[i];
		Matrix44::Translate(offset, p);
		rotation = offset * scalerot;
		Matrix44::Translate(location, pos);
		world = rotation * location;
		// copy matrices into buffer
		memcpy(params.world, world.data, 16 * sizeof(float));
		D3D::context->UpdateSubresource(m_vertex_shader_params, 0, nullptr, &params, 0, 0);

		D3D::stateman->SetVertexConstants(m_vertex_shader_params);
		D3D::stateman->SetPixelConstants(m_vertex_shader_params);
		D3D::stateman->SetTexture(0, m_texture->GetSRV());
		D3D::stateman->SetSampler(0, m_avatar_sampler);

		// Draw!
		D3D::stateman->Apply();
		D3D::context->DrawIndexed(m_index_count, 0, 0);

		// Clean up state
		D3D::stateman->SetSampler(0, nullptr);
		D3D::stateman->SetTexture(0, nullptr);
		D3D::stateman->SetPixelConstants(nullptr);
		D3D::stateman->SetVertexConstants(nullptr);
	}

	void AvatarDrawer::Draw()
	{
		if (!g_ActiveConfig.bShowController)
			return;

		// Reset API

		g_renderer->ResetAPIState();

		// Set up all the state for Avatar drawing

		D3D::stateman->SetPixelShader(m_texture_pixel_shader);
		D3D::stateman->SetVertexShader(m_vertex_shader);
		D3D::stateman->SetGeometryShader(m_geometry_shader);

		D3D::stateman->PushBlendState(m_avatar_blend_state);
		D3D::stateman->PushDepthState(m_avatar_depth_state);
		D3D::stateman->PushRasterizerState(m_avatar_rast_state);

		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)Renderer::GetTargetWidth(), (float)Renderer::GetTargetHeight());
		D3D::context->RSSetViewports(1, &vp);

		D3D::stateman->SetInputLayout(m_vertex_layout);
		D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		UINT stride = sizeof(VERTEX);
		UINT offset = 0;
		D3D::stateman->SetVertexBuffer(m_vertex_buffer, stride, offset);
		D3D::stateman->SetIndexBuffer(m_index_buffer);

		D3D::context->OMSetRenderTargets(1,
			&FramebufferManager::GetEFBColorTexture()->GetRTV(),
			FramebufferManager::GetEFBDepthTexture()->GetDSV());

		// update geometry shader buffer
		Matrix44 hmd_left, hmd_right;
		VR_GetProjectionMatrices(hmd_left, hmd_right, 0.1f, 10000.0f);
		GeometryShaderManager::constants.stereoparams[0] = hmd_left.data[0 * 4 + 0];
		GeometryShaderManager::constants.stereoparams[1] = hmd_right.data[0 * 4 + 0];
		GeometryShaderManager::constants.stereoparams[2] = hmd_left.data[0 * 4 + 2];
		GeometryShaderManager::constants.stereoparams[3] = hmd_right.data[0 * 4 + 2];
		float posLeft[3] = { 0, 0, 0 };
		float posRight[3] = { 0, 0, 0 };
		VR_GetEyePos(posLeft, posRight);
		GeometryShaderManager::constants.stereoparams[0] *= posLeft[0];
		GeometryShaderManager::constants.stereoparams[1] *= posRight[0];
		GeometryShaderManager::dirty = true;
		GeometryShaderCache::GetConstantBuffer();

		// Update Projection, View, and World matrices
		Matrix44 proj, view;
		Matrix44::LoadIdentity(proj);
		Matrix44::LoadIdentity(view);
		float pos[3] = { 0, 0, 0 };
		float wmpos[3] = { 0, 0, 0 };
		Matrix33 wmrot;

		// view matrix
		if (g_ActiveConfig.bPositionTracking)
		{
			float head[3];
			for (int i = 0; i < 3; ++i)
				head[i] = g_head_tracking_position[i];
			pos[0] = head[0];
			pos[1] = head[1];// *cos(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle)) + head[2] * sin(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle));
			pos[2] = head[2];// *cos(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle)) - head[1] * sin(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle));
		}
		else
		{
			pos[0] = 0;
			pos[1] = 0;
			pos[2] = 0;
		}
		Matrix44 walk_matrix;
		Matrix44::Translate(walk_matrix, pos);
		view = walk_matrix * g_head_tracking_matrix;
		// projection matrix
		Matrix44::Set(proj, hmd_left.data);
		// remove the part that is different for each eye
		if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
		{
			proj.data[0 * 4 + 2] = 0;
		}
		// copy matrices into buffer
		memcpy(params.projection, proj.data, 16 * sizeof(float));
		memcpy(params.view, view.data, 16 * sizeof(float));

		// Clear depth buffer
		D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);

		Matrix33 m;
		ControllerStyle cs;
		// Draw Left Razer Hydra
		if (VR_GetLeftHydraPos(pos, &m))
		{
			cs = VR_GetHydraStyle(0);
			DrawHydra(pos, m, cs);
		}
		// Draw Right Razer Hydra
		bool has_right_controller = VR_GetRightHydraPos(pos, &wmrot);
		if (has_right_controller)
		{
			cs = VR_GetHydraStyle(1);
			DrawHydra(pos, wmrot, cs);
			wmpos[0] = pos[0];
			wmpos[1] = pos[1];
			wmpos[2] = pos[2];
		}

		// Draw Lines
		bool draw_laser_pointer = g_ActiveConfig.bShowLaserPointer && has_right_controller && cs == CS_WIIMOTE_IR;
		if (g_ActiveConfig.bShowAimRectangle || g_ActiveConfig.bShowHudBox || g_ActiveConfig.bShow2DBox || draw_laser_pointer)
		{
			D3D::stateman->SetVertexShader(m_line_vertex_shader);
			D3D::stateman->SetGeometryShader(m_line_geometry_shader);
			D3D::stateman->SetPixelShader(m_color_pixel_shader);
			D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			D3D::stateman->PushRasterizerState(m_avatar_line_rast_state);
			D3D::stateman->SetVertexBuffer(m_line_vertex_buffer, stride, offset);
			// Draw Laser Sight
			if (draw_laser_pointer)
			{
				DrawLine(pos, wmrot, 1, 0, 0);
			}

			pos[0] = 0; pos[1] = 0; pos[2] = 0;
			Matrix33 m;
			if (g_ActiveConfig.bShow2DBox)
			{
				Matrix33::LoadIdentity(m);
				DrawBox(2, pos, m, 0.1f, 0.3f, 1.0f);
			}
			if (g_ActiveConfig.bShowAimRectangle)
			{
				Matrix33::LoadIdentity(m);
				DrawBox(0, pos, m, 1, 0.3f, 0.3f);
			}
			if (g_ActiveConfig.bShowHudBox)
			{
				Matrix33::LoadIdentity(m);
				DrawBox(1, pos, m, 0.1f, 1.0f, 0.1f);
			}

			D3D::stateman->PopRasterizerState();
		}

		// restore state
		D3D::stateman->SetPixelShader(nullptr);
		D3D::stateman->SetVertexShader(nullptr);
		D3D::stateman->SetGeometryShader(nullptr);

		D3D::stateman->PopRasterizerState();
		D3D::stateman->PopDepthState();
		D3D::stateman->PopBlendState();

		// Restore API

		g_renderer->RestoreAPIState();
		D3D::context->OMSetRenderTargets(1,
			&FramebufferManager::GetEFBColorTexture()->GetRTV(),
			FramebufferManager::GetEFBDepthTexture()->GetDSV());
	}

	void GetModelSize(tinyobj::shape_t *shape, float *width, float *height, float *depth, float *x, float *y, float *z)
	{
		*width = *height = *depth = *x = *y = *z = 0;
		if (!shape || shape->mesh.positions.size() == 0)
			return;
		float min[3] = { 100000.0f, 100000.0f, 100000.0f };
		float max[3] = { -100000.0f, -100000.0f, -100000.0f };
		for (size_t i = 0; i < shape->mesh.positions.size(); ++i)
		{
			float v = shape->mesh.positions[i];
			if (v > max[i % 3])
				max[i % 3] = v;
			if (v < min[i % 3])
				min[i % 3] = v;
		}
		*width = max[0] - min[0];
		*height = max[1] - min[1];
		*depth = max[2] - min[2];
		*x = (max[0] + min[0]) / 2.0f;
		*y = (max[1] + min[1]) / 2.0f;
		*z = (max[2] + min[2]) / 2.0f;
	}

}
