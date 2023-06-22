// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12StreamBuffer.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"

#include "VideoCommon/BoundingBox.h"

namespace DX12
{
class D3D12BoundingBox final : public BoundingBox
{
public:
  ~D3D12BoundingBox() override;

  bool Initialize() override;

protected:
  std::vector<BBoxType> Read(u32 index, u32 length) override;
  void Write(u32 index, const std::vector<BBoxType>& values) override;

private:
  static constexpr u32 BUFFER_SIZE = sizeof(BBoxType) * NUM_BBOX_VALUES;
  static constexpr u32 MAX_UPDATES_PER_FRAME = 128;
  static constexpr u32 STREAM_BUFFER_SIZE = BUFFER_SIZE * MAX_UPDATES_PER_FRAME;

  bool CreateBuffers();

  // Three buffers: GPU for read/write, CPU for reading back, and CPU for staging changes.
  ComPtr<ID3D12Resource> m_gpu_buffer;
  ComPtr<ID3D12Resource> m_readback_buffer;
  StreamBuffer m_upload_buffer;
  DescriptorHandle m_gpu_descriptor{};
};

}  // namespace DX12
