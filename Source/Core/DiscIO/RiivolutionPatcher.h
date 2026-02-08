// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/RiivolutionParser.h"

namespace Core
{
class CPUThreadGuard;
}

namespace DiscIO::Riivolution
{
struct SavegameRedirect
{
  std::string m_target_path;
  bool m_clone;
};

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
  virtual std::optional<std::string>
  ResolveSavegameRedirectPath(std::string_view external_relative_path) = 0;
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
  std::optional<std::string>
  ResolveSavegameRedirectPath(std::string_view external_relative_path) override;

private:
  std::optional<std::string> MakeAbsoluteFromRelative(std::string_view external_relative_path);

  std::string m_sd_root;
  std::string m_patch_root;
};

enum class PatchIndex
{
  FileSystem,
  DolphinSysFiles,
};

void ApplyPatchesToFiles(std::span<const Patch> patches, PatchIndex index,
                         std::vector<FSTBuilderNode>* fst, FSTBuilderNode* dol_node);
void ApplyGeneralMemoryPatches(const Core::CPUThreadGuard& guard, std::span<const Patch> patches);
void ApplyApploaderMemoryPatches(const Core::CPUThreadGuard& guard, std::span<const Patch> patches,
                                 u32 ram_address, u32 ram_length);
std::optional<SavegameRedirect> ExtractSavegameRedirect(std::span<const Patch> riivolution_patches);
}  // namespace DiscIO::Riivolution
