// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataUtils.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor
{
class GpuSkinningDataDumper
{
public:
  void SetOutputPath(std::string path);
  void WriteToFile();
  void AddData(GraphicsModSystem::DrawCallID draw_call_id,
               const GraphicsModSystem::DrawDataView& draw_data, bool apply_gpu_skinning,
               std::span<const u16> index_data);

private:
  std::string m_path;
  GpuSkinningData m_gpu_skinning_data;
};
}  // namespace GraphicsModEditor
