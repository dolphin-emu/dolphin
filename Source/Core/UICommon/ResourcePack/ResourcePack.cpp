// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/ResourcePack/ResourcePack.h"

#include <algorithm>
#include <array>
#include <utility>

#include <unzip.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/Manifest.h"

namespace ResourcePack
{
constexpr char TEXTURE_PATH[] = HIRES_TEXTURES_DIR DIR_SEP;
constexpr char DYNAMIC_INPUT_PATH[] = DYNAMICINPUT_DIR DIR_SEP;

bool ResourcePack::ResourceFile::operator<(const ResourcePack::ResourceFile& file) const
{
  return input_path < file.input_path;
}

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

  unz_file_info manifest_info;
  unzGetCurrentFileInfo(file, &manifest_info, nullptr, 0, nullptr, 0, nullptr, 0);

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
    unz_file_info logo_info;

    unzGetCurrentFileInfo(file, &logo_info, nullptr, 0, nullptr, 0, nullptr, 0);

    m_logo_data.resize(logo_info.uncompressed_size);

    if (!Common::ReadFileFromZip(file, &m_logo_data))
    {
      m_valid = false;
      m_error = "Failed to read logo.png";
      return;
    }
  }

  unzGoToFirstFile(file);

  constexpr std::array<ResourceType, 2> resource_types = {ResourceType::TEXTURE,
                                                          ResourceType::DYNAMIC_INPUT};

  do
  {
    std::string filename(256, '\0');

    unz_file_info file_info;
    unzGetCurrentFileInfo(file, &file_info, filename.data(), static_cast<u16>(filename.size()),
                          nullptr, 0, nullptr, 0);

    for (auto resource_type : resource_types)
    {
      std::string zip_directory_root;
      std::string load_directory_root;
      if (resource_type == ResourceType::TEXTURE)
      {
        zip_directory_root = "textures/";
        load_directory_root = TEXTURE_PATH;
      }
      else
      {
        zip_directory_root = "dynamic_input/";
        load_directory_root = DYNAMIC_INPUT_PATH;
      }

      if (filename.compare(0, zip_directory_root.size(), zip_directory_root) != 0 ||
          file_info.uncompressed_size == 0)
      {
        continue;
      }

      // If a file is compressed and the manifest doesn't state that, abort.
      if (!m_manifest->IsCompressed() && file_info.compression_method != 0)
      {
        m_valid = false;
        m_error =
            "File " + filename + " is compressed when the resource pack says it shouldn't be!";
        return;
      }

      m_files.emplace(ResourceFile{filename,
                                   load_directory_root + filename.substr(zip_directory_root.size()),
                                   resource_type});
      m_types_provided |= static_cast<u32>(resource_type);
    }
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

const std::set<ResourcePack::ResourceFile>& ResourcePack::GetFiles() const
{
  return m_files;
}

u32 ResourcePack::GetResourceTypesSupported() const
{
  return m_types_provided;
}

bool ResourcePack::Install(const std::string& path)
{
  if (!IsValid())
  {
    m_error = "Invalid pack";
    return false;
  }

  auto resource_pack = unzOpen(m_path.c_str());
  Common::ScopeGuard file_guard{[&] { unzClose(resource_pack); }};

  for (const auto& file : m_files)
  {
    bool provided_by_other_pack = false;

    // Check if a higher priority pack already provides a given file, don't overwrite it
    for (const auto& pack : GetHigherPriorityPacks(*this))
    {
      if (pack->GetFiles().count(file) != 0)
      {
        provided_by_other_pack = true;
        break;
      }
    }

    if (provided_by_other_pack)
      continue;

    if (unzLocateFile(resource_pack, (file.input_path).c_str(), 0) != UNZ_OK)
    {
      m_error = "Failed to locate file " + file.input_path;
      return false;
    }

    const std::string output_path = path + file.output_path;

    std::string m_full_dir;
    SplitPath(output_path, &m_full_dir, nullptr, nullptr);

    if (!File::CreateFullPath(m_full_dir))
    {
      m_error = "Failed to create full path " + m_full_dir;
      return false;
    }

    unz_file_info file_info;
    unzGetCurrentFileInfo(resource_pack, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);

    std::vector<char> data(file_info.uncompressed_size);
    if (!Common::ReadFileFromZip(resource_pack, &data))
    {
      m_error = "Failed to read " + file.input_path;
      return false;
    }

    std::ofstream out;
    File::OpenFStream(out, output_path, std::ios::trunc | std::ios::binary);

    if (!out.good())
    {
      m_error = "Failed to write " + output_path;
      return false;
    }

    out.write(data.data(), data.size());
    out.flush();
  }

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

  for (const auto& file : m_files)
  {
    bool provided_by_other_pack = false;

    // Check if a higher priority pack already provides a given file, don't delete it
    for (const auto& pack : GetHigherPriorityPacks(*this))
    {
      if (::ResourcePack::IsInstalled(*pack) && pack->GetFiles().count(file) != 0)
      {
        provided_by_other_pack = true;
        break;
      }
    }

    if (provided_by_other_pack)
      continue;

    // Check if a lower priority pack provides a given file - if so, install it.
    for (auto& pack : lower)
    {
      if (::ResourcePack::IsInstalled(*pack) && pack->GetFiles().count(file) != 0)
      {
        pack->Install(path);

        provided_by_other_pack = true;
        break;
      }
    }

    if (provided_by_other_pack)
      continue;

    const std::string output_path = path + file.output_path;
    if (File::Exists(output_path) && !File::Delete(output_path))
    {
      m_error = "Failed to delete file " + output_path;
      return false;
    }

    // Recursively delete empty directories

    std::string dir;
    SplitPath(output_path, &dir, nullptr, nullptr);

    std::string root = "";
    if (file.type == ResourceType::TEXTURE)
    {
      root = path + TEXTURE_PATH;
    }
    else if (file.type == ResourceType::DYNAMIC_INPUT)
    {
      root = path + DYNAMIC_INPUT_PATH;
    }

    while (dir.length() > root.length())
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
