// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/tiny_obj_loader.h"
#include "VideoCommon/VideoCommon.h"

struct ID3D11Texture2D;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11GeometryShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

namespace DX11
{

	typedef enum
	{
		CS_HYDRA_LEFT,
		CS_HYDRA_RIGHT,
		CS_WIIMOTE,
		CS_NUNCHUK,
		CS_WIIMOTE_LEFT,
		CS_WIIMOTE_RIGHT,
		CS_CLASSIC_LEFT,
		CS_CLASSIC_RIGHT,
		CS_GC_LEFT,
		CS_GC_RIGHT,
		CS_N64_LEFT,
		CS_N64_RIGHT,
		CS_SNES_LEFT,
		CS_SNES_RIGHT,
		CS_SNES_NTSC_RIGHT,
		CS_NES_LEFT,
		CS_NES_RIGHT,
		CS_FAMICON_LEFT,
		CS_FAMICON_RIGHT,
		CS_SEGA_LEFT,
		CS_SEGA_RIGHT,
		CS_GENESIS_LEFT,
		CS_GENESIS_RIGHT,
		CS_TURBOGRAFX_LEFT,
		CS_TURBOGRAFX_RIGHT,
		CS_PCENGINE_LEFT,
		CS_PCENGINE_RIGHT,
		CS_ARCADE_LEFT,
		CS_ARCADE_RIGHT
	} ControllerStyle;

	union VertexShaderParams
	{
		struct
		{
			float world[16];
			float view[16];
			float projection[16];
			float color[4];
		};
		// Constant buffers must be a multiple of 16 bytes in size
	};

	class AvatarDrawer
	{

	public:
		AvatarDrawer();

		void Init();
		void Shutdown();

		void Draw();

	private:
		void DrawHydra(float *pos, ControllerStyle cs);

		ID3D11Buffer* m_vertex_shader_params;
		ID3D11Buffer* m_vertex_buffer;
		ID3D11Buffer* m_index_buffer;
		ID3D11InputLayout* m_vertex_layout;
		ID3D11VertexShader* m_vertex_shader;
		ID3D11GeometryShader* m_geometry_shader;
		ID3D11PixelShader* m_pixel_shader;
		ID3D11BlendState* m_avatar_blend_state;
		ID3D11DepthStencilState* m_avatar_depth_state;
		ID3D11RasterizerState* m_avatar_rast_state;
		ID3D11SamplerState* m_avatar_sampler;
		VertexShaderParams params;

		float *m_vertices;
		u16 *m_indices;
		int m_vertex_count, m_index_count;
		float hydra_size[3], hydra_mid[3];
	};

	void GetModelSize(tinyobj::shape_t *shape, float *width, float *height, float *depth, float *x, float *y, float *z);
}
