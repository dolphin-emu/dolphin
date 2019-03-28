// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <memory>
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"
#include "VideoBackends/D3D12/StreamBuffer.h"

namespace DX12
{
class BoundingBox
{
public:
  ~BoundingBox();

  static std::unique_ptr<BoundingBox> Create();

  const DescriptorHandle& GetGPUDescriptor() const { return m_gpu_descriptor; }

  s32 Get(size_t index);
  void Set(size_t index, s32 value);

  void Invalidate();
  void Flush();

private:
  using ValueType = s32;
  static const u32 NUM_VALUES = 4;
  static const u32 BUFFER_SIZE = sizeof(ValueType) * NUM_VALUES;
  static const u32 MAX_UPDATES_PER_FRAME = 128;
  static const u32 STREAM_BUFFER_SIZE = BUFFER_SIZE * MAX_UPDATES_PER_FRAME;

  BoundingBox();

  bool CreateBuffers();
  void Readback();

  // Three buffers: GPU for read/write, CPU for reading back, and CPU for staging changes.
  ComPtr<ID3D12Resource> m_gpu_buffer;
  ComPtr<ID3D12Resource> m_readback_buffer;
  StreamBuffer m_upload_buffer;
  DescriptorHandle m_gpu_descriptor;
  std::array<ValueType, NUM_VALUES> m_values = {};
  std::array<bool, NUM_VALUES> m_dirty = {};
  bool m_valid = true;
};
};  // namespace DX12
