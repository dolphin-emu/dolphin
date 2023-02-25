// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/D3D12BoundingBox.h"

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/D3D12/D3D12Gfx.h"
#include "VideoBackends/D3D12/DX12Context.h"

namespace DX12
{
D3D12BoundingBox::~D3D12BoundingBox()
{
  if (m_gpu_descriptor)
    g_dx_context->GetDescriptorHeapManager().Free(m_gpu_descriptor);
}

bool D3D12BoundingBox::Initialize()
{
  if (!CreateBuffers())
    return false;

  Gfx::GetInstance()->SetPixelShaderUAV(m_gpu_descriptor.cpu_handle);
  return true;
}

std::vector<BBoxType> D3D12BoundingBox::Read(u32 index, u32 length)
{
  // Copy from GPU->CPU buffer, and wait for the GPU to finish the copy.
  ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  g_dx_context->GetCommandList()->CopyBufferRegion(m_readback_buffer.Get(), 0, m_gpu_buffer.Get(),
                                                   0, BUFFER_SIZE);
  ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                  D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  Gfx::GetInstance()->ExecuteCommandList(true);

  // Read back to cached values.
  std::vector<BBoxType> values(length);
  static constexpr D3D12_RANGE read_range = {0, BUFFER_SIZE};
  void* mapped_pointer;
  HRESULT hr = m_readback_buffer->Map(0, &read_range, &mapped_pointer);
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Map bounding box CPU buffer failed: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return values;

  // Copy out the values we want
  std::memcpy(values.data(), reinterpret_cast<const u8*>(mapped_pointer) + sizeof(BBoxType) * index,
              sizeof(BBoxType) * length);

  static constexpr D3D12_RANGE write_range = {0, 0};
  m_readback_buffer->Unmap(0, &write_range);

  return values;
}

void D3D12BoundingBox::Write(u32 index, const std::vector<BBoxType>& values)
{
  const u32 copy_size = static_cast<u32>(values.size()) * sizeof(BBoxType);
  if (!m_upload_buffer.ReserveMemory(copy_size, sizeof(BBoxType)))
  {
    WARN_LOG_FMT(VIDEO, "Executing command list while waiting for space in bbox stream buffer");
    Gfx::GetInstance()->ExecuteCommandList(false);
    if (!m_upload_buffer.ReserveMemory(copy_size, sizeof(BBoxType)))
    {
      PanicAlertFmt("Failed to allocate bbox stream buffer space");
      return;
    }
  }

  const u32 upload_buffer_offset = m_upload_buffer.GetCurrentOffset();
  std::memcpy(m_upload_buffer.GetCurrentHostPointer(), values.data(), copy_size);
  m_upload_buffer.CommitMemory(copy_size);

  ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

  g_dx_context->GetCommandList()->CopyBufferRegion(m_gpu_buffer.Get(), index * sizeof(BBoxType),
                                                   m_upload_buffer.GetBuffer(),
                                                   upload_buffer_offset, copy_size);

  ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

bool D3D12BoundingBox::CreateBuffers()
{
  static constexpr D3D12_HEAP_PROPERTIES gpu_heap_properties = {D3D12_HEAP_TYPE_DEFAULT};
  static constexpr D3D12_HEAP_PROPERTIES cpu_heap_properties = {D3D12_HEAP_TYPE_READBACK};
  D3D12_RESOURCE_DESC buffer_desc = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                     0,
                                     BUFFER_SIZE,
                                     1,
                                     1,
                                     1,
                                     DXGI_FORMAT_UNKNOWN,
                                     {1, 0},
                                     D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};

  HRESULT hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &gpu_heap_properties, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_COMMON,
      nullptr, IID_PPV_ARGS(&m_gpu_buffer));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating bounding box GPU buffer failed: {}", DX12HRWrap(hr));
  if (FAILED(hr) || !g_dx_context->GetDescriptorHeapManager().Allocate(&m_gpu_descriptor))
    return false;

  D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {DXGI_FORMAT_R32_SINT, D3D12_UAV_DIMENSION_BUFFER};
  uav_desc.Buffer.NumElements = NUM_BBOX_VALUES;
  g_dx_context->GetDevice()->CreateUnorderedAccessView(m_gpu_buffer.Get(), nullptr, &uav_desc,
                                                       m_gpu_descriptor.cpu_handle);

  buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
  hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &cpu_heap_properties, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr, IID_PPV_ARGS(&m_readback_buffer));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating bounding box CPU buffer failed: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  if (!m_upload_buffer.AllocateBuffer(STREAM_BUFFER_SIZE))
    return false;

  return true;
}
};  // namespace DX12
