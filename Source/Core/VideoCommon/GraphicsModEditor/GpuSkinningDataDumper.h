// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>

#include "VideoCommon/GraphicsModEditor/GpuSkinningDataUtils.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor
{
enum class TransformType
{
  None,
  Object,
  GPU
};

class GpuSkinningDataDumper
{
public:
  void SetOutputPath(std::string path);
  void WriteToFile();
  void AddData(GraphicsModSystem::DrawCallID draw_call_id,
               const GraphicsModSystem::DrawDataView& draw_data, TransformType apply_transform_type,
               std::span<const u16> index_data);

private:
  std::string m_path;
  NativeMeshData m_native_mesh_data;
};
}  // namespace GraphicsModEditor
