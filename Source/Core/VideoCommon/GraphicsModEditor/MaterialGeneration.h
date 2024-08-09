// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>

#include "VideoCommon/Assets/MaterialAsset.h"

namespace GraphicsModEditor
{
struct EditorState;
struct MaterialGenerationContext
{
  GraphicsModEditor::EditorState* state;
  std::string lookup_path;
  std::string input_path;
  std::string output_path;
  VideoCommon::MaterialData material_template_data;
  std::map<std::size_t, std::string> material_property_index_to_texture_filter;
  bool search_input_recursively = true;
};
void GenerateMaterials(MaterialGenerationContext* generation_context, std::string* error);
}  // namespace GraphicsModEditor
