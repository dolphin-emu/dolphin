// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/ResourcePack/ResourcePack.h"

#include <algorithm>
#include <memory>

#include <mz_compat.h>
#include <mz_os.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/Manifest.h"

namespace ResourcePack
{
constexpr char TEXTURE_PATH[] = HIRES_TEXTURES_DIR DIR_SEP;

ResourcePack::ResourcePack(const std::string& path) : m_path(path)
{
  auto file = unzOpen(path.c_str());
  Common::ScopeGuard file_guard{[&] { unzClose(file); }};

  if (file == nullptr)
  {
    m_valid = false;
    m_error = "Failed to open resource pack";
    return;
  }

  if (unzLocateFile(file, "manifest.json", 0) == UNZ_END_OF_LIST_OF_FILE)
  {
    m_valid = false;
    m_error = "Resource pack is missing a manifest.";
    return;
  }

  unz_file_info64 manifest_info{};
  unzGetCurrentFileInfo64(file, &manifest_info, nullptr, 0, nullptr, 0, nullptr, 0);

  std::string manifest_contents(manifest_info.uncompressed_size, '\0');
  if (!Common::ReadFileFromZip(file, &manifest_contents))
  {
    m_valid = false;
    m_error = "Failed to read manifest.json";
    return;
  }
  unzCloseCurrentFile(file);

  m_manifest = std::make_shared<Manifest>(manifest_contents);
  if (!m_manifest->IsValid())
  {
    m_valid = false;
    m_error = "Manifest error: " + m_manifest->GetError();
    return;
  }

  if (unzLocateFile(file, "logo.png", 0) != UNZ_END_OF_LIST_OF_FILE)
  {
    unz_file_info64 logo_info{};
    unzGetCurrentFileInfo64(file, &logo_info, nullptr, 0, nullptr, 0, nullptr, 0);

    m_logo_data.resize(logo_info.uncompressed_size);

    if (!Common::ReadFileFromZip(file, &m_logo_data))
    {
      m_valid = false;
      m_error = "Failed to read logo.png";
      return;
    }
  }

  unzGoToFirstFile(file);

  do
  {
    std::string filename(256, '\0');

    unz_file_info64 texture_info{};
    unzGetCurrentFileInfo64(file, &texture_info, filename.data(), static_cast<u16>(filename.size()),
                            nullptr, 0, nullptr, 0);

    if (!filename.starts_with("textures/") || texture_info.uncompressed_size == 0)
      continue;

    // If a texture is compressed and the manifest doesn't state that, abort.
    if (!m_manifest->IsCompressed() && texture_info.compression_method != 0)
    {
      m_valid = false;
      m_error = "Texture " + filename + " is compressed!";
      return;
    }

    m_textures.push_back(filename.substr(9));
  } while (unzGoToNextFile(file) != UNZ_END_OF_LIST_OF_FILE);
}

bool ResourcePack::IsValid() const
{
  return m_valid;
}

const std::vector<char>& ResourcePack::GetLogo() const
{
  return m_logo_data;
}

const std::string& ResourcePack::GetPath() const
{
  return m_path;
}

const std::string& ResourcePack::GetError() const
{
  return m_error;
}

const Manifest* ResourcePack::GetManifest() const
{
  return m_manifest.get();
}

const std::vector<std::string>& ResourcePack::GetTextures() const
{
  return m_textures;
}

bool ResourcePack::Install(const std::string& path)
{
  if (!IsValid())
  {
    m_error = "Invalid pack";
    return false;
  }

  auto file = unzOpen(m_path.c_str());
  if (file == nullptr)
  {
    m_valid = false;
    m_error = "Failed to open resource pack";
    return false;
  }
  Common::ScopeGuard file_guard{[&] { unzClose(file); }};

  if (unzGoToFirstFile(file) != MZ_OK)
    return false;

  std::string texture_zip_path;
  do
  {
    texture_zip_path.resize(UINT16_MAX + 1, '\0');
    unz_file_info64 texture_info{};
    if (unzGetCurrentFileInfo64(file, &texture_info, texture_zip_path.data(), UINT16_MAX, nullptr,
                                0, nullptr, 0) != MZ_OK)
    {
      return false;
    }
    TruncateToCString(&texture_zip_path);

    const std::string texture_zip_path_prefix = "textures/";
    if (!texture_zip_path.starts_with(texture_zip_path_prefix))
      continue;
    const std::string texture_name = texture_zip_path.substr(texture_zip_path_prefix.size());

    auto texture_it = std::find_if(
        m_textures.cbegin(), m_textures.cend(), [&texture_name](const std::string& texture) {
          return mz_path_compare_wc(texture.c_str(), texture_name.c_str(), 1) == MZ_OK;
        });
    if (texture_it == m_textures.cend())
      continue;
    const auto texture = *texture_it;

    // Check if a higher priority pack already provides a given texture, don't overwrite it
    bool provided_by_other_pack = false;
    for (const auto& pack : GetHigherPriorityPacks(*this))
    {
      if (std::find(pack->GetTextures().begin(), pack->GetTextures().end(), texture) !=
          pack->GetTextures().end())
      {
        provided_by_other_pack = true;
        break;
      }
    }
    if (provided_by_other_pack)
      continue;

    const std::string texture_path = path + TEXTURE_PATH + texture;
    std::string texture_full_dir;
    if (!SplitPath(texture_path, &texture_full_dir, nullptr, nullptr))
      continue;

    if (!File::CreateFullPath(texture_full_dir))
    {
      m_error = "Failed to create full path " + texture_full_dir;
      return false;
    }

    const size_t data_size = static_cast<size_t>(texture_info.uncompressed_size);
    auto data = std::make_unique<u8[]>(data_size);
    if (!Common::ReadFileFromZip(file, data.get(), data_size))
    {
      m_error = "Failed to read texture " + texture;
      return false;
    }

    File::IOFile out(texture_path, "wb");
    if (!out)
    {
      m_error = "Failed to open " + texture;
      return false;
    }
    if (!out.WriteBytes(data.get(), data_size))
    {
      m_error = "Failed to write " + texture;
      return false;
    }
  } while (unzGoToNextFile(file) == MZ_OK);

  SetInstalled(*this, true);
  return true;
}

bool ResourcePack::Uninstall(const std::string& path)
{
  if (!IsValid())
  {
    m_error = "Invalid pack";
    return false;
  }

  auto lower = GetLowerPriorityPacks(*this);

  SetInstalled(*this, false);

  for (const auto& texture : m_textures)
  {
    bool provided_by_other_pack = false;

    // Check if a higher priority pack already provides a given texture, don't delete it
    for (const auto& pack : GetHigherPriorityPacks(*this))
    {
      if (::ResourcePack::IsInstalled(*pack) &&
          std::find(pack->GetTextures().begin(), pack->GetTextures().end(), texture) !=
              pack->GetTextures().end())
      {
        provided_by_other_pack = true;
        break;
      }
    }

    if (provided_by_other_pack)
      continue;

    // Check if a lower priority pack provides a given texture - if so, install it.
    for (auto& pack : lower)
    {
      if (::ResourcePack::IsInstalled(*pack) &&
          std::find(pack->GetTextures().rbegin(), pack->GetTextures().rend(), texture) !=
              pack->GetTextures().rend())
      {
        pack->Install(path);

        provided_by_other_pack = true;
        break;
      }
    }

    if (provided_by_other_pack)
      continue;

    const std::string texture_path = path + TEXTURE_PATH + texture;
    if (File::Exists(texture_path) && !File::Delete(texture_path))
    {
      m_error = "Failed to delete texture " + texture;
      return false;
    }

    // Recursively delete empty directories

    std::string dir;
    SplitPath(texture_path, &dir, nullptr, nullptr);

    while (dir.length() > (path + TEXTURE_PATH).length())
    {
      auto is_empty = Common::DoFileSearch({dir}).empty();

      if (is_empty)
        File::DeleteDir(dir);

      SplitPath(dir.substr(0, dir.size() - 2), &dir, nullptr, nullptr);
    }
  }

  return true;
}

bool ResourcePack::operator==(const ResourcePack& pack) const
{
  return pack.GetPath() == m_path;
}

bool ResourcePack::operator!=(const ResourcePack& pack) const
{
  return !operator==(pack);
}

}  // namespace ResourcePack
