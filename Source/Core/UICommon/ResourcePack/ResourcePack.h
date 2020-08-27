// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

#include "UICommon/ResourcePack/Manifest.h"

namespace ResourcePack
{
class ResourcePack
{
public:
  explicit ResourcePack(const std::string& path);

  bool IsValid() const;
  const std::vector<char>& GetLogo() const;

  const std::string& GetPath() const;
  const std::string& GetError() const;
  const Manifest* GetManifest() const;
  const std::vector<std::string>& GetTextures() const;

  bool Install(const std::string& path);
  bool Uninstall(const std::string& path);

  bool operator==(const ResourcePack& pack) const;
  bool operator!=(const ResourcePack& pack) const;

private:
  bool m_valid = true;

  std::string m_path;
  std::string m_error;

  std::shared_ptr<Manifest> m_manifest;
  std::vector<std::string> m_textures;
  std::vector<char> m_logo_data;
};
}  // namespace ResourcePack
