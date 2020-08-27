// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/BoundingBox.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D12/DXContext.h"
#include "VideoBackends/D3D12/Renderer.h"

namespace DX12
{
BoundingBox::BoundingBox() = default;

BoundingBox::~BoundingBox()
{
  if (m_gpu_descriptor)
    g_dx_context->GetDescriptorHeapManager().Free(m_gpu_descriptor);
}

std::unique_ptr<BoundingBox> BoundingBox::Create()
{
  auto bbox = std::unique_ptr<BoundingBox>(new BoundingBox());
  if (!bbox->CreateBuffers())
    return nullptr;

  return bbox;
}

bool BoundingBox::CreateBuffers()
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
      &gpu_heap_properties, D3D12_HEAP_FLAG_NONE, &buffer_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_gpu_buffer));
  CHECK(SUCCEEDED(hr), "Creating bounding box GPU buffer failed");
  if (FAILED(hr) || !g_dx_context->GetDescriptorHeapManager().Allocate(&m_gpu_descriptor))
    return false;

  D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {DXGI_FORMAT_R32_SINT, D3D12_UAV_DIMENSION_BUFFER};
  uav_desc.Buffer.NumElements = NUM_VALUES;
  g_dx_context->GetDevice()->CreateUnorderedAccessView(m_gpu_buffer.Get(), nullptr, &uav_desc,
                                                       m_gpu_descriptor.cpu_handle);

  buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
  hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &cpu_heap_properties, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr, IID_PPV_ARGS(&m_readback_buffer));
  CHECK(SUCCEEDED(hr), "Creating bounding box CPU buffer failed");
  if (FAILED(hr))
    return false;

  if (!m_upload_buffer.AllocateBuffer(STREAM_BUFFER_SIZE))
    return false;

  // Both the CPU and GPU buffer's contents is unknown, so force a flush the first time.
  m_values.fill(0);
  m_dirty.fill(true);
  m_valid = true;
  return true;
}

void BoundingBox::Readback()
{
  // Copy from GPU->CPU buffer, and wait for the GPU to finish the copy.
  ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  g_dx_context->GetCommandList()->CopyBufferRegion(m_readback_buffer.Get(), 0, m_gpu_buffer.Get(),
                                                   0, BUFFER_SIZE);
  ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                  D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  Renderer::GetInstance()->ExecuteCommandList(true);

  // Read back to cached values.
  static constexpr D3D12_RANGE read_range = {0, BUFFER_SIZE};
  void* mapped_pointer;
  HRESULT hr = m_readback_buffer->Map(0, &read_range, &mapped_pointer);
  CHECK(SUCCEEDED(hr), "Map bounding box CPU buffer");
  if (FAILED(hr))
    return;

  static constexpr D3D12_RANGE write_range = {0, 0};
  std::array<s32, NUM_VALUES> new_values;
  std::memcpy(new_values.data(), mapped_pointer, BUFFER_SIZE);
  m_readback_buffer->Unmap(0, &write_range);

  // Preserve dirty values, that way we don't need to sync.
  for (u32 i = 0; i < NUM_VALUES; i++)
  {
    if (!m_dirty[i])
      m_values[i] = new_values[i];
  }
  m_valid = true;
}

s32 BoundingBox::Get(size_t index)
{
  if (!m_valid)
    Readback();

  return m_values[index];
}

void BoundingBox::Set(size_t index, s32 value)
{
  m_values[index] = value;
  m_dirty[index] = true;
}

void BoundingBox::Invalidate()
{
  m_dirty.fill(false);
  m_valid = false;
}

void BoundingBox::Flush()
{
  bool in_copy_state = false;
  for (u32 start = 0; start < NUM_VALUES;)
  {
    if (!m_dirty[start])
    {
      start++;
      continue;
    }

    u32 end = start + 1;
    m_dirty[start] = false;
    for (; end < NUM_VALUES; end++)
    {
      if (!m_dirty[end])
        break;

      m_dirty[end] = false;
    }

    const u32 copy_size = (end - start) * sizeof(ValueType);
    if (!m_upload_buffer.ReserveMemory(copy_size, sizeof(ValueType)))
    {
      WARN_LOG(VIDEO, "Executing command list while waiting for space in bbox stream buffer");
      Renderer::GetInstance()->ExecuteCommandList(false);
      if (!m_upload_buffer.ReserveMemory(copy_size, sizeof(ValueType)))
      {
        PanicAlert("Failed to allocate bbox stream buffer space");
        return;
      }
    }

    const u32 upload_buffer_offset = m_upload_buffer.GetCurrentOffset();
    std::memcpy(m_upload_buffer.GetCurrentHostPointer(), &m_values[start], copy_size);
    m_upload_buffer.CommitMemory(copy_size);

    if (!in_copy_state)
    {
      ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
      in_copy_state = true;
    }

    g_dx_context->GetCommandList()->CopyBufferRegion(m_gpu_buffer.Get(), start * sizeof(ValueType),
                                                     m_upload_buffer.GetBuffer(),
                                                     upload_buffer_offset, copy_size);
    start = end;
  }

  if (in_copy_state)
  {
    ResourceBarrier(g_dx_context->GetCommandList(), m_gpu_buffer.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }
}
};  // namespace DX12
