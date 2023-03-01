// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureData.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureInfo.h"

enum class TextureFormat;

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id);

class HiresTexture
{
public:
  static void Init();
  static void Update();
  static void Clear();
  static void Shutdown();

  static std::shared_ptr<HiresTexture> Search(const TextureInfo& texture_info);

  static std::string GenBaseName(const TextureInfo& texture_info, bool dump = false);

  ~HiresTexture();

  AbstractTextureFormat GetFormat() const;
  bool HasArbitraryMipmaps() const;

  VideoCommon::CustomTextureData& GetData() { return m_data; }
  const VideoCommon::CustomTextureData& GetData() const { return m_data; }

private:
  static std::unique_ptr<HiresTexture> Load(const std::string& base_filename, u32 width,
                                            u32 height);
  static void Prefetch();

  HiresTexture() = default;

  VideoCommon::CustomTextureData m_data;
  bool m_has_arbitrary_mipmaps = false;
};
