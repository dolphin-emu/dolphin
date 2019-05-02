// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/ResourcePack/ResourcePack.h"

#include <algorithm>

#include <minizip/unzip.h>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/Manifest.h"

static const char* TEXTURE_PATH = "Load/Textures/";

namespace ResourcePack
{
// Since minzip doesn't provide a way to unzip a file of a length > 65535, we have to implement
// this ourselves
static bool ReadCurrentFileUnlimited(unzFile file, std::vector<char>& destination)
{
  const uint32_t MAX_BUFFER_SIZE = 65535;

  if (unzOpenCurrentFile(file) != UNZ_OK)
    return false;

  uint32_t bytes_to_go = static_cast<uint32_t>(destination.size());

  while (bytes_to_go > 0)
  {
    int bytes_read = unzReadCurrentFile(file, &destination[destination.size() - bytes_to_go],
                                        std::min(bytes_to_go, MAX_BUFFER_SIZE));

    if (bytes_read < 0)
    {
      unzCloseCurrentFile(file);
      return false;
    }

    bytes_to_go -= bytes_read;
  }

  unzCloseCurrentFile(file);

  return true;
}

ResourcePack::ResourcePack(const std::string& path) : m_path(path)
{
  auto file = unzOpen(path.c_str());

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

  unz_file_info manifest_info;

  unzGetCurrentFileInfo(file, &manifest_info, nullptr, 0, nullptr, 0, nullptr, 0);

  std::vector<char> manifest_contents;

  manifest_contents.resize(manifest_info.uncompressed_size);

  if (!ReadCurrentFileUnlimited(file, manifest_contents))
  {
    m_valid = false;
    m_error = "Failed to read manifest.json";
    return;
  }

  unzCloseCurrentFile(file);

  m_manifest =
      std::make_shared<Manifest>(std::string(manifest_contents.begin(), manifest_contents.end()));

  if (!m_manifest->IsValid())
  {
    m_valid = false;
    m_error = "Manifest error: " + m_manifest->GetError();
    return;
  }

  if (unzLocateFile(file, "logo.png", 0) != UNZ_END_OF_LIST_OF_FILE)
  {
    unz_file_info logo_info;

    unzGetCurrentFileInfo(file, &logo_info, nullptr, 0, nullptr, 0, nullptr, 0);

    m_logo_data.resize(logo_info.uncompressed_size);

    if (!ReadCurrentFileUnlimited(file, m_logo_data))
    {
      m_valid = false;
      m_error = "Failed to read logo.png";
      return;
    }
  }

  unzGoToFirstFile(file);

  do
  {
    std::string filename;

    filename.resize(256);

    unz_file_info texture_info;

    unzGetCurrentFileInfo(file, &texture_info, &filename[0], static_cast<uint16_t>(filename.size()),
                          nullptr, 0, nullptr, 0);

    if (filename.compare(0, 9, "textures/") != 0 || texture_info.uncompressed_size == 0)
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

  unzClose(file);
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

  for (const auto& texture : m_textures)
  {
    bool provided_by_other_pack = false;

    // Check if a higher priority pack already provides a given texture, don't overwrite it
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

    if (unzLocateFile(file, ("textures/" + texture).c_str(), 0) != UNZ_OK)
    {
      m_error = "Failed to locate texture " + texture;
      return false;
    }

    std::string m_full_dir;

    SplitPath(path + TEXTURE_PATH + texture, &m_full_dir, nullptr, nullptr);

    if (!File::CreateFullPath(m_full_dir))
    {
      m_error = "Failed to create full path " + m_full_dir;
      return false;
    }

    unz_file_info texture_info;

    unzGetCurrentFileInfo(file, &texture_info, nullptr, 0, nullptr, 0, nullptr, 0);

    std::vector<char> data;
    data.resize(texture_info.uncompressed_size);

    if (!ReadCurrentFileUnlimited(file, data))
    {
      m_error = "Failed to read texture " + texture;
      return false;
    }

    std::ofstream out(path + TEXTURE_PATH + texture, std::ios::trunc | std::ios::binary);

    if (!out.good())
    {
      m_error = "Failed to write " + texture;
      return false;
    }

    out.write(data.data(), data.size());
    out.flush();
  }

  unzClose(file);

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

    if (File::Exists(path + TEXTURE_PATH + texture) && !File::Delete(path + TEXTURE_PATH + texture))
    {
      m_error = "Failed to delete texture " + texture;
      return false;
    }

    // Recursively delete empty directories

    std::string dir;

    SplitPath(path + TEXTURE_PATH + texture, &dir, nullptr, nullptr);

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
