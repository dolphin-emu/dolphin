// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "D3DCommandListManager.h"

#include "ShaderConstantsManager.h"

#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

enum SHADER_STAGE
{
	SHADER_STAGE_GEOMETRY_SHADER = 0,
	SHADER_STAGE_PIXEL_SHADER = 1,
	SHADER_STAGE_VERTEX_SHADER = 2,
	SHADER_STAGE_COUNT = 3
};

static ID3D12Resource* s_shader_constant_buffers[DX12::SHADER_STAGE_COUNT] = {};
static void* s_shader_constant_buffer_data[SHADER_STAGE_COUNT] = {};
static D3D12_GPU_VIRTUAL_ADDRESS s_shader_constant_buffer_gpu_va[SHADER_STAGE_COUNT] = {};

static const unsigned int s_shader_constant_buffer_padded_sizes[SHADER_STAGE_COUNT] = {
	(sizeof(GeometryShaderConstants) + 0xff) & ~0xff,
	(sizeof(PixelShaderConstants)    + 0xff) & ~0xff,
	(sizeof(VertexShaderConstants)   + 0xff) & ~0xff
};

static const unsigned int s_shader_constant_buffer_slot_count[SHADER_STAGE_COUNT] = {
	50000,
	50000,
	50000
};

static const unsigned int s_shader_constant_buffer_slot_rollover_threshold[SHADER_STAGE_COUNT] = {
	10000,
	10000,
	10000
};

static unsigned int s_shader_constant_buffer_current_slot_index[SHADER_STAGE_COUNT] = {};

void ShaderConstantsManager::Init()
{
	for (unsigned int i = 0; i < SHADER_STAGE_COUNT; i++)
	{
		s_shader_constant_buffer_current_slot_index[i] = 0;

		const unsigned int upload_heap_size = s_shader_constant_buffer_padded_sizes[i] * s_shader_constant_buffer_slot_count[i];

		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(upload_heap_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&s_shader_constant_buffers[i])
				)
			);

		D3D::SetDebugObjectName12(s_shader_constant_buffers[i], "constant buffer used to emulate the GX pipeline");

		// Obtain persistent CPU pointer, never needs to be unmapped.
		CheckHR(s_shader_constant_buffers[i]->Map(0, nullptr, &s_shader_constant_buffer_data[i]));

		// Obtain GPU VA for upload heap, to avoid repeated calls to ID3D12Resource::GetGPUVirtualAddress.
		s_shader_constant_buffer_gpu_va[i] = s_shader_constant_buffers[i]->GetGPUVirtualAddress();
	}
}

void ShaderConstantsManager::Shutdown()
{
	for (unsigned int i = 0; i < SHADER_STAGE_COUNT; i++)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_shader_constant_buffers[i]);

		s_shader_constant_buffers[i] = nullptr;
		s_shader_constant_buffer_current_slot_index[i] = 0;
		s_shader_constant_buffer_data[i] = nullptr;
	}
}

void ShaderConstantsManager::LoadAndSetGeometryShaderConstants()
{
	if (GeometryShaderManager::dirty)
	{
		s_shader_constant_buffer_current_slot_index[SHADER_STAGE_GEOMETRY_SHADER]++;

		memcpy(
			static_cast<u8*>(s_shader_constant_buffer_data[SHADER_STAGE_GEOMETRY_SHADER]) +
			s_shader_constant_buffer_padded_sizes[SHADER_STAGE_GEOMETRY_SHADER] *
			s_shader_constant_buffer_current_slot_index[SHADER_STAGE_GEOMETRY_SHADER],
			&GeometryShaderManager::constants,
			sizeof(GeometryShaderConstants));

		GeometryShaderManager::dirty = false;

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(GeometryShaderConstants));

		D3D::command_list_mgr->m_dirty_gs_cbv = true;
	}

	if (D3D::command_list_mgr->m_dirty_gs_cbv)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_GS_CBV,
			s_shader_constant_buffer_gpu_va[SHADER_STAGE_GEOMETRY_SHADER] +
			s_shader_constant_buffer_padded_sizes[SHADER_STAGE_GEOMETRY_SHADER] *
			s_shader_constant_buffer_current_slot_index[SHADER_STAGE_GEOMETRY_SHADER]
			);

		D3D::command_list_mgr->m_dirty_gs_cbv = false;
	}
}

void ShaderConstantsManager::LoadAndSetPixelShaderConstants()
{
	if (PixelShaderManager::dirty)
	{
		s_shader_constant_buffer_current_slot_index[SHADER_STAGE_PIXEL_SHADER]++;

		memcpy(
			static_cast<u8*>(s_shader_constant_buffer_data[SHADER_STAGE_PIXEL_SHADER]) +
			s_shader_constant_buffer_padded_sizes[SHADER_STAGE_PIXEL_SHADER] *
			s_shader_constant_buffer_current_slot_index[SHADER_STAGE_PIXEL_SHADER],
			&PixelShaderManager::constants,
			sizeof(PixelShaderConstants));

		PixelShaderManager::dirty = false;

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(PixelShaderConstants));

		D3D::command_list_mgr->m_dirty_ps_cbv = true;
	}

	if (D3D::command_list_mgr->m_dirty_ps_cbv)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_PS_CBVONE,
			s_shader_constant_buffer_gpu_va[SHADER_STAGE_PIXEL_SHADER] +
			s_shader_constant_buffer_padded_sizes[SHADER_STAGE_PIXEL_SHADER] *
			s_shader_constant_buffer_current_slot_index[SHADER_STAGE_PIXEL_SHADER]
			);

		D3D::command_list_mgr->m_dirty_ps_cbv = false;
	}
}

void ShaderConstantsManager::LoadAndSetVertexShaderConstants()
{
	if (VertexShaderManager::dirty)
	{
		s_shader_constant_buffer_current_slot_index[SHADER_STAGE_VERTEX_SHADER]++;

		memcpy(
			static_cast<u8*>(s_shader_constant_buffer_data[SHADER_STAGE_VERTEX_SHADER]) +
			s_shader_constant_buffer_padded_sizes[SHADER_STAGE_VERTEX_SHADER] *
			s_shader_constant_buffer_current_slot_index[SHADER_STAGE_VERTEX_SHADER],
			&VertexShaderManager::constants,
			sizeof(VertexShaderConstants));

		VertexShaderManager::dirty = false;

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(VertexShaderConstants));

		D3D::command_list_mgr->m_dirty_vs_cbv = true;
	}

	if (D3D::command_list_mgr->m_dirty_vs_cbv)
	{
		const D3D12_GPU_VIRTUAL_ADDRESS calculated_gpu_va =
			s_shader_constant_buffer_gpu_va[SHADER_STAGE_VERTEX_SHADER] +
			s_shader_constant_buffer_padded_sizes[SHADER_STAGE_VERTEX_SHADER] *
			s_shader_constant_buffer_current_slot_index[SHADER_STAGE_VERTEX_SHADER];

		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_VS_CBV,
			calculated_gpu_va
			);

		if (g_ActiveConfig.bEnablePixelLighting)
			D3D::current_command_list->SetGraphicsRootConstantBufferView(
				DESCRIPTOR_TABLE_PS_CBVTWO,
				calculated_gpu_va
				);

		D3D::command_list_mgr->m_dirty_vs_cbv = false;
	}
}

void ShaderConstantsManager::CheckToResetIndexPositionInUploadHeaps()
{
	for (unsigned int i = 0; i < SHADER_STAGE_COUNT; i++)
	{
		if (s_shader_constant_buffer_current_slot_index[i] > s_shader_constant_buffer_slot_rollover_threshold[i])
		{
			s_shader_constant_buffer_current_slot_index[i] = 0;
		}
	}
}

}