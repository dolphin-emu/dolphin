// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <variant>
#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "VideoCommon/Assets/CustomAsset.h"

class ShaderCode;

namespace VideoCommon
{
struct MaterialProperty
{
  static void WriteToMemory(u8*& buffer, const MaterialProperty& property);
  static std::size_t GetMemorySize(const MaterialProperty& property);
  static void WriteAsShaderCode(ShaderCode& shader_source, const MaterialProperty& property);
  using Value = std::variant<CustomAssetLibrary::AssetID, s32, std::array<s32, 2>,
                             std::array<s32, 3>, std::array<s32, 4>, float, std::array<float, 2>,
                             std::array<float, 3>, std::array<float, 4>, bool>;
  std::string m_code_name;
  Value m_value;
};

struct MaterialData
{
  static bool FromJson(const CustomAssetLibrary::AssetID& asset_id, const picojson::object& json,
                       MaterialData* data);
  static void ToJson(picojson::object* obj, const MaterialData& data);
  std::string shader_asset;
  std::vector<MaterialProperty> properties;
};

// Much like Unity and Unreal materials, a Dolphin material does very little on its own
// Its sole purpose is to provide data (through properties) that are used in conjunction
// with a shader asset that is provided by name.  It is up to user of this asset to
// use the two together to create the relevant runtime data
class MaterialAsset final : public CustomLoadableAsset<MaterialData>
{
public:
  using CustomLoadableAsset::CustomLoadableAsset;

private:
  CustomAssetLibrary::LoadInfo LoadImpl(const CustomAssetLibrary::AssetID& asset_id) override;
};

}  // namespace VideoCommon
