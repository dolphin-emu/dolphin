// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <unordered_set>

#include "Common/CommonTypes.h"

class AbstractTexture;

namespace VideoCommon::TextureUtils
{
class TextureDumper
{
public:
  // Only dumps if texture did not already exist anywhere within the dump-textures path.
  void DumpTexture(const ::AbstractTexture& texture, std::string basename, u32 level,
                   bool is_arbitrary);

private:
  std::unordered_set<std::string> m_dumped_textures;
};

void DumpTexture(const ::AbstractTexture& texture, std::string basename, u32 level,
                 bool is_arbitrary);

}  // namespace VideoCommon::TextureUtils
