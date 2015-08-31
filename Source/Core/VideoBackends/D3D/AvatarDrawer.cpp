// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
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

#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"

namespace DX11
{

	struct VERTEX
	{
		float X, Y, Z;      // position
		float nx, ny, nz;   // normal
		float u, v;         // texture coordinates
	};

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

	static const char AVATAR_DRAWER_PS[] =
		"// dolphin-emu AvatarDrawer pixel shader\n"

		"void main(out float4 ocol0 : SV_Target, in float4 position : SV_Position, in float4 color : COLOR, "
		"in float3 uv : TEXCOORD, in uint layer : SV_RenderTargetArrayIndex)\n"
		"{\n"
		"ocol0 = color;\n"
		"}\n"
		;

	static const D3D11_INPUT_ELEMENT_DESC QUAD_LAYOUT_DESC[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};


	AvatarDrawer::AvatarDrawer()
		: m_vertex_shader_params(nullptr),
		m_vertex_buffer(nullptr), m_index_buffer(nullptr), m_vertex_layout(nullptr),
		m_vertex_shader(nullptr), m_geometry_shader(nullptr), m_pixel_shader(nullptr),
		m_avatar_blend_state(nullptr), m_avatar_depth_state(nullptr),
		m_avatar_rast_state(nullptr), m_avatar_sampler(nullptr)
	{ }

	void AvatarDrawer::Init()
	{
		HRESULT hr;
		D3D11_BUFFER_DESC bd;

		// Load Hydra model
		{
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
		}

		// Create vertex buffer
		bd = CD3D11_BUFFER_DESC(m_vertex_count * sizeof(float), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC);
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA srd = { m_vertices, 0, 0 };
		hr = D3D::device->CreateBuffer(&bd, &srd, &m_vertex_buffer);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer vertex buffer");
		D3D::SetDebugObjectName(m_vertex_buffer, "AvatarDrawer vertex buffer");

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

		// Create constant buffer for uploading params to shaders

		bd = CD3D11_BUFFER_DESC(sizeof(VertexShaderParams),
			D3D11_BIND_CONSTANT_BUFFER);
		hr = D3D::device->CreateBuffer(&bd, nullptr, &m_vertex_shader_params);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer params buffer");
		D3D::SetDebugObjectName(m_vertex_shader_params, "AvatarDrawer params buffer");

		// Create geometry shader
		ShaderCode code;
		GenerateAvatarGeometryShaderCode(code, PRIMITIVE_TRIANGLES, API_D3D);
		if (!D3D::CompileGeometryShader(code.GetBuffer(), &bytecode))
		{
			ERROR_LOG(VR, "AvatarDrawer geometry shader failed to compile");
			return;
		}
		hr = D3D::device->CreateGeometryShader(bytecode->Data(), bytecode->Size(), nullptr, &m_geometry_shader);
		CHECK(SUCCEEDED(hr), "create AvatarDrawer geometry shader");
		D3D::SetDebugObjectName(m_geometry_shader, "AvatarDrawer geometry shader");
		bytecode->Release();

		// Create pixel shader

		m_pixel_shader = D3D::CompileAndCreatePixelShader(AVATAR_DRAWER_PS);
		if (!m_pixel_shader)
		{
			ERROR_LOG(VR, "AvatarDrawer pixel shader failed to compile");
			return;
		}
		D3D::SetDebugObjectName(m_pixel_shader, "AvatarDrawer pixel shader");

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
		SAFE_RELEASE(m_avatar_sampler);
		SAFE_RELEASE(m_avatar_rast_state);
		SAFE_RELEASE(m_avatar_depth_state);
		SAFE_RELEASE(m_avatar_blend_state);
		SAFE_RELEASE(m_pixel_shader);
		SAFE_RELEASE(m_geometry_shader);
		SAFE_RELEASE(m_vertex_layout);
		SAFE_RELEASE(m_vertex_shader);
		SAFE_RELEASE(m_vertex_buffer);
		SAFE_RELEASE(m_index_buffer);
		SAFE_RELEASE(m_vertex_shader_params);
	}

	void AvatarDrawer::DrawHydra(float *pos, ControllerStyle cs)
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
		// world matrix
		Matrix44 world, scale, location;
		float v[3] = { 0.0025f, 0.0025f, 0.0025f };
		Matrix44::Scale(scale, v);
		float p[3];
		for (int i = 0; i < 3; ++i)
			p[i] = pos[i] - hydra_mid[i] * 0.0025f;
		Matrix44::Translate(location, p);
		Matrix44::Multiply(location, scale, world);
		// copy matrices into buffer
		memcpy(params.world, world.data, 16 * sizeof(float));
		D3D::context->UpdateSubresource(m_vertex_shader_params, 0, nullptr, &params, 0, 0);

		D3D::stateman->SetVertexConstants(m_vertex_shader_params);
		D3D::stateman->SetPixelConstants(m_vertex_shader_params);
		D3D::stateman->SetTexture(0, nullptr);
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

		D3D::stateman->SetPixelShader(m_pixel_shader);
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
		Matrix44::Multiply(g_head_tracking_matrix, walk_matrix, view);
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

		// Draw Left Razer Hydra
		if (VR_GetLeftHydraPos(pos))
		{
			ControllerStyle cs = VR_GetHydraStyle(0);
			DrawHydra(pos, cs);
		}
		// Draw Right Razer Hydra
		if (VR_GetRightHydraPos(pos))
		{
			ControllerStyle cs = VR_GetHydraStyle(1);
			DrawHydra(pos, cs);
		}

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
