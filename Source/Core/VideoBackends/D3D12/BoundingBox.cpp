// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D12/BoundingBox.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

constexpr size_t BBOX_BUFFER_SIZE = sizeof(int) * 4;
constexpr size_t BBOX_STREAM_BUFFER_SIZE = BBOX_BUFFER_SIZE * 128;

static ID3D12Resource* s_bbox_buffer;
static ID3D12Resource* s_bbox_staging_buffer;
static void* s_bbox_staging_buffer_map;
static std::unique_ptr<D3DStreamBuffer> s_bbox_stream_buffer;
static D3D12_GPU_DESCRIPTOR_HANDLE s_bbox_descriptor_handle;

void BBox::Init()
{
	CD3DX12_RESOURCE_DESC buffer_desc(CD3DX12_RESOURCE_DESC::Buffer(BBOX_BUFFER_SIZE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0));
	CD3DX12_RESOURCE_DESC staging_buffer_desc(CD3DX12_RESOURCE_DESC::Buffer(BBOX_BUFFER_SIZE, D3D12_RESOURCE_FLAG_NONE, 0));

	CheckHR(D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&buffer_desc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&s_bbox_buffer)));

	CheckHR(D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&staging_buffer_desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&s_bbox_staging_buffer)));

	s_bbox_stream_buffer = std::make_unique<D3DStreamBuffer>(BBOX_STREAM_BUFFER_SIZE, BBOX_STREAM_BUFFER_SIZE, nullptr);

	// D3D12 root signature UAV must be raw or structured buffers, not typed. Since we used a typed buffer,
	// we have to use a descriptor table. Luckily, we only have to allocate this once, and it never changes.
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle;
	if (!D3D::gpu_descriptor_heap_mgr->Allocate(&cpu_descriptor_handle, &s_bbox_descriptor_handle, nullptr, false))
		PanicAlert("Failed to create bounding box UAV descriptor");

	D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc = { DXGI_FORMAT_R32_SINT, D3D12_UAV_DIMENSION_BUFFER };
	view_desc.Buffer.FirstElement = 0;
	view_desc.Buffer.NumElements = 4;
	view_desc.Buffer.StructureByteStride = 0;
	view_desc.Buffer.CounterOffsetInBytes = 0;
	view_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	D3D::device12->CreateUnorderedAccessView(s_bbox_buffer, nullptr, &view_desc, cpu_descriptor_handle);

	Bind();
}

void BBox::Bind()
{
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_UAV, s_bbox_descriptor_handle);
}

void BBox::Invalidate()
{
	if (!s_bbox_staging_buffer_map)
		return;

	D3D12_RANGE write_range = {};
	s_bbox_staging_buffer->Unmap(0, &write_range);
	s_bbox_staging_buffer_map = nullptr;
}

void BBox::Shutdown()
{
	Invalidate();

	if (s_bbox_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_buffer);
		s_bbox_buffer = nullptr;
	}

	if (s_bbox_staging_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_staging_buffer);
		s_bbox_staging_buffer = nullptr;
	}

	s_bbox_stream_buffer.reset();
}

void BBox::Set(int index, int value)
{
	// If the buffer is currently mapped, compare the value, and update the staging buffer.
	if (s_bbox_staging_buffer_map)
	{
		int current_value;
		memcpy(&current_value, reinterpret_cast<u8*>(s_bbox_staging_buffer_map) + (index * sizeof(int)), sizeof(int));
		if (current_value == value)
		{
			// Value hasn't changed. So skip updating completely.
			return;
		}

		memcpy(reinterpret_cast<u8*>(s_bbox_staging_buffer_map) + (index * sizeof(int)), &value, sizeof(int));
	}

	s_bbox_stream_buffer->AllocateSpaceInBuffer(sizeof(int), sizeof(int));

	// Allocate temporary bytes in upload buffer, then copy to real buffer.
	memcpy(s_bbox_stream_buffer->GetCPUAddressOfCurrentAllocation(), &value, sizeof(int));
	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST, 0);
	D3D::current_command_list->CopyBufferRegion(s_bbox_buffer, index * sizeof(int), s_bbox_stream_buffer->GetBuffer(), s_bbox_stream_buffer->GetOffsetOfCurrentAllocation(), sizeof(int));
	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);
}

int BBox::Get(int index)
{
	if (!s_bbox_staging_buffer_map)
	{
		D3D::command_list_mgr->CPUAccessNotify();

		// Copy from real buffer to staging buffer, then block until we have the results.
		D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		D3D::current_command_list->CopyBufferRegion(s_bbox_staging_buffer, 0, s_bbox_buffer, 0, BBOX_BUFFER_SIZE);
		D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);

		D3D::command_list_mgr->ExecuteQueuedWork(true);

		D3D12_RANGE read_range = { 0, BBOX_BUFFER_SIZE };
		CheckHR(s_bbox_staging_buffer->Map(0, &read_range, &s_bbox_staging_buffer_map));
	}

	int value;
	memcpy(&value, &reinterpret_cast<int*>(s_bbox_staging_buffer_map)[index], sizeof(int));
	return value;
}

};
