// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BoundingBox.h"

#include <Metal/Metal.h>

#include "VideoBackends/Metal/MRCHelpers.h"

namespace Metal
{
class BoundingBox final : public ::BoundingBox
{
public:
  ~BoundingBox() override;

  bool Initialize() override;

protected:
  std::vector<BBoxType> Read(u32 index, u32 length) override;
  void Write(u32 index, std::span<const BBoxType> values) override;

private:
  BBoxType* m_cpu_buffer_ptr;
  MRCOwned<id<MTLFence>> m_download_fence;
  MRCOwned<id<MTLFence>> m_upload_fence;
  MRCOwned<id<MTLBuffer>> m_cpu_buffer;
  MRCOwned<id<MTLBuffer>> m_gpu_buffer;
};
}  // namespace Metal
