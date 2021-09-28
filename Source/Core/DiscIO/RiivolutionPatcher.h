// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/RiivolutionParser.h"

namespace DiscIO::Riivolution
{
class FileDataLoader
{
public:
  struct Node
  {
    std::string m_filename;
    bool m_is_directory;
  };

  virtual ~FileDataLoader();
  virtual std::optional<u64> GetExternalFileSize(std::string_view external_relative_path) = 0;
  virtual std::vector<u8> GetFileContents(std::string_view external_relative_path) = 0;
  virtual std::vector<Node> GetFolderContents(std::string_view external_relative_path) = 0;
  virtual BuilderContentSource MakeContentSource(std::string_view external_relative_path,
                                                 u64 external_offset, u64 external_size,
                                                 u64 disc_offset) = 0;
};

class FileDataLoaderHostFS : public FileDataLoader
{
public:
  // sd_root should be an absolute path to the folder representing our virtual SD card
  // xml_path should be an absolute path to the parsed XML file
  // patch_root should be the 'root' attribute given in the 'patch' or 'wiiroot' XML element
  FileDataLoaderHostFS(std::string sd_root, const std::string& xml_path,
                       std::string_view patch_root);

  std::optional<u64> GetExternalFileSize(std::string_view external_relative_path) override;
  std::vector<u8> GetFileContents(std::string_view external_relative_path) override;
  std::vector<FileDataLoader::Node>
  GetFolderContents(std::string_view external_relative_path) override;
  BuilderContentSource MakeContentSource(std::string_view external_relative_path,
                                         u64 external_offset, u64 external_size,
                                         u64 disc_offset) override;

private:
  std::optional<std::string> MakeAbsoluteFromRelative(std::string_view external_relative_path);

  std::string m_sd_root;
  std::string m_patch_root;
};

void ApplyPatchesToFiles(const std::vector<Patch>& patches,
                         std::vector<DiscIO::FSTBuilderNode>* fst,
                         DiscIO::FSTBuilderNode* dol_node);
void ApplyPatchesToMemory(const std::vector<Patch>& patches);
}  // namespace DiscIO::Riivolution
