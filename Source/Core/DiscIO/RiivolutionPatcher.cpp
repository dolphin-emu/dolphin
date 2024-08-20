// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/RiivolutionPatcher.h"

#include <algorithm>
#include <locale>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/StringUtil.h"
#include "Core/AchievementManager.h"
#include "Core/Core.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"
#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/RiivolutionParser.h"

namespace DiscIO::Riivolution
{
FileDataLoader::~FileDataLoader() = default;

FileDataLoaderHostFS::FileDataLoaderHostFS(std::string sd_root, const std::string& xml_path,
                                           std::string_view patch_root)
    : m_sd_root(std::move(sd_root))
{
  // Riivolution treats 'external' file paths as follows:
  // - If it starts with a '/', it's an absolute path, ie. relative to the SD card root.
  // - Otherwise:
  //   - If the 'root' parameter of the current patch is not set or is empty, the path is relative
  //     to the folder the XML file is in.
  //   - If the 'root' parameter of the current patch starts with a '/', the path is relative to
  //     that folder on the SD card, starting at the SD card root.
  //   - If the 'root' parameter of the current patch starts without a '/', the path is relative to
  //     that folder on the SD card, starting at the folder the XML file is in.
  // The following initialization should properly replicate this behavior.

  // First set m_patch_root to the folder the parsed XML file is in.
  SplitPath(xml_path, &m_patch_root, nullptr, nullptr);

  // Then try to resolve the given patch_root as if it was a file path, and on success replace the
  // m_patch_root with it.
  if (!patch_root.empty())
  {
    if (auto r = MakeAbsoluteFromRelative(patch_root))
      m_patch_root = std::move(*r);
  }
}

std::optional<std::string>
FileDataLoaderHostFS::MakeAbsoluteFromRelative(std::string_view external_relative_path)
{
#ifdef _WIN32
  // Riivolution treats a backslash as just a standard filename character, but we can't replicate
  // this properly on Windows. So if a file contains a backslash, immediately error out.
  if (external_relative_path.find("\\") != std::string_view::npos)
    return std::nullopt;
#endif

  const std::string& root = external_relative_path.starts_with('/') ? m_sd_root : m_patch_root;
  std::string result = root;
  std::string_view work = external_relative_path;

  // Strip away all leading and trailing path separators.
  while (work.starts_with('/'))
    work.remove_prefix(1);
  while (work.ends_with('/'))
    work.remove_suffix(1);
  size_t depth = 0;
  while (true)
  {
    if (work.empty())
      break;

    // Extract a single path element.
    size_t separator_position = work.find('/');
    std::string_view element = work.substr(0, separator_position);

    if (element == ".")
    {
      // This is a harmless element, doesn't change any state.
    }
    else if (element == "..")
    {
      // We're going up a level.
      // If this isn't possible someone is trying to exit the root directory, prevent that.
      if (depth == 0)
        return std::nullopt;
      --depth;

      // Remove the last path element from the result string.
      // This must have been previously attached in the branch below (otherwise depth would have
      // been 0), so there's no need to check whether the string is empty or anything like that.
      while (result.back() != '/')
        result.pop_back();
      result.pop_back();
    }
    else if (std::all_of(element.begin(), element.end(), [](char c) { return c == '.'; }))
    {
      // This is a triple, quadruple, etc. dot.
      // Some file systems treat this as several 'up' path traversals, but Riivolution does not.
      // If someone tries this just error out, it wouldn't work sensibly in Riivolution anyway.
      return std::nullopt;
    }
    else
    {
      // We're going down a level.
      ++depth;

      // Append path element to result string.
      result += '/';
      result += element;

      // Riivolution assumes a case-insensitive file system, which means it's possible that an XML
      // file references a 'file.bin' but the actual file is named 'File.bin' or 'FILE.BIN'. To
      // preserve this behavior, we modify the file path to match any existing file in the file
      // system, if one exists.
      if (!::File::Exists(result))
      {
        // Drop path element again so we can search in the directory.
        result.erase(result.size() - element.size(), element.size());

        // Re-attach an element that actually matches the capitalization in the host filesystem.
        auto possible_files = ::File::ScanDirectoryTree(result, false);
        bool found = false;
        for (auto& f : possible_files.children)
        {
          if (Common::CaseInsensitiveEquals(element, f.virtualName))
          {
            result += f.virtualName;
            found = true;
            break;
          }
        }

        // If there isn't any file that matches just use the given element.
        if (!found)
          result += element;
      }
    }

    // If this was the last path element, we're done.
    if (separator_position == std::string_view::npos)
      break;

    // Remove element from work string.
    work = work.substr(separator_position + 1);

    // Remove any potential extra path separators.
    while (work.starts_with('/'))
      work = work.substr(1);
  }
  return result;
}

std::optional<u64>
FileDataLoaderHostFS::GetExternalFileSize(std::string_view external_relative_path)
{
  auto path = MakeAbsoluteFromRelative(external_relative_path);
  if (!path)
    return std::nullopt;
  ::File::FileInfo f(*path);
  if (!f.IsFile())
    return std::nullopt;
  return f.GetSize();
}

std::vector<u8> FileDataLoaderHostFS::GetFileContents(std::string_view external_relative_path)
{
  auto path = MakeAbsoluteFromRelative(external_relative_path);
  if (!path)
    return {};
  ::File::IOFile f(*path, "rb");
  if (!f)
    return {};
  const u64 length = f.GetSize();
  std::vector<u8> value;
  value.resize(length);
  if (!f.ReadBytes(value.data(), length))
    return {};
  return value;
}

std::vector<FileDataLoader::Node>
FileDataLoaderHostFS::GetFolderContents(std::string_view external_relative_path)
{
  auto path = MakeAbsoluteFromRelative(external_relative_path);
  if (!path)
    return {};
  ::File::FSTEntry external_files = ::File::ScanDirectoryTree(*path, false);
  std::vector<FileDataLoader::Node> nodes;
  nodes.reserve(external_files.children.size());
  for (auto& file : external_files.children)
    nodes.emplace_back(FileDataLoader::Node{std::move(file.virtualName), file.isDirectory});
  return nodes;
}

BuilderContentSource
FileDataLoaderHostFS::MakeContentSource(std::string_view external_relative_path,
                                        u64 external_offset, u64 external_size, u64 disc_offset)
{
  auto path = MakeAbsoluteFromRelative(external_relative_path);
  if (!path)
    return BuilderContentSource{disc_offset, external_size, ContentFixedByte{0}};
  return BuilderContentSource{disc_offset, external_size,
                              ContentFile{std::move(*path), external_offset}};
}

std::optional<std::string>
FileDataLoaderHostFS::ResolveSavegameRedirectPath(std::string_view external_relative_path)
{
  return MakeAbsoluteFromRelative(external_relative_path);
}

// 'before' and 'after' should be two copies of the same source
// 'split_at' needs to be between the start and end of the source, may not match either boundary
static void SplitAt(BuilderContentSource* before, BuilderContentSource* after, u64 split_at)
{
  const u64 start = before->m_offset;
  const u64 size = before->m_size;
  const u64 end = start + size;

  // The source before the split point just needs its length reduced.
  before->m_size = split_at - start;

  // The source after the split needs its length reduced and its start point adjusted.
  after->m_offset += before->m_size;
  after->m_size = end - split_at;
  if (std::holds_alternative<ContentFile>(after->m_source))
  {
    std::get<ContentFile>(after->m_source).m_offset += before->m_size;
  }
  else if (std::holds_alternative<ContentMemory>(after->m_source))
  {
    after->m_source = std::make_shared<std::vector<u8>>(
        std::get<ContentMemory>(after->m_source)->begin() + before->m_size,
        std::get<ContentMemory>(after->m_source)->end());
  }
  else if (std::holds_alternative<ContentPartition>(after->m_source))
  {
    std::get<ContentPartition>(after->m_source).m_offset += before->m_size;
  }
  else if (std::holds_alternative<ContentVolume>(after->m_source))
  {
    std::get<ContentVolume>(after->m_source).m_offset += before->m_size;
  }
}

static void ApplyPatchToFile(const Patch& patch, DiscIO::FSTBuilderNode* file_node,
                             std::string_view external_filename, u64 file_patch_offset,
                             u64 raw_external_file_offset, u64 file_patch_length, bool resize)
{
  const auto f = patch.m_file_data_loader->GetExternalFileSize(external_filename);
  if (!f)
    return;

  auto& content = std::get<std::vector<BuilderContentSource>>(file_node->m_content);

  const u64 raw_external_filesize = *f;
  const u64 external_file_offset = std::min(raw_external_file_offset, raw_external_filesize);
  const u64 external_filesize = raw_external_filesize - external_file_offset;

  const u64 patch_start = file_patch_offset;
  const u64 patch_size = file_patch_length == 0 ? external_filesize : file_patch_length;
  const u64 patch_end = patch_start + patch_size;

  const u64 target_filesize = resize ? patch_end : std::max(file_node->m_size, patch_end);

  size_t insert_where = 0;
  if (patch_start >= file_node->m_size)
  {
    // If the patch is at or past the end of the existing file no existing content needs to be
    // touched, just extend the file.
    if (patch_start > file_node->m_size)
    {
      // Insert an padding area between the old file and the patch data.
      content.emplace_back(BuilderContentSource{file_node->m_size, patch_start - file_node->m_size,
                                                ContentFixedByte{0}});
    }

    insert_where = content.size();
  }
  else
  {
    // Patch is at the start or somewhere in the middle of the existing file. At least one source
    // needs to be modified or removed, and a new source with the patch data inserted instead.
    // To make this easier, we first split up existing sources at the patch start and patch end
    // offsets, then discard all overlapping sources and insert the patch sources there.
    for (size_t i = 0; i < content.size(); ++i)
    {
      const u64 source_start = content[i].m_offset;
      const u64 source_end = source_start + content[i].m_size;
      if (patch_start > source_start && patch_start < source_end)
      {
        content.insert(content.begin() + i + 1, content[i]);
        SplitAt(&content[i], &content[i + 1], patch_start);
        continue;
      }
      if (patch_end > source_start && patch_end < source_end)
      {
        content.insert(content.begin() + i + 1, content[i]);
        SplitAt(&content[i], &content[i + 1], patch_end);
      }
    }

    // Now discard the overlapping areas and remember where they were so we can insert there.
    for (size_t i = 0; i < content.size(); ++i)
    {
      if (patch_start == content[i].m_offset)
      {
        insert_where = i;
        while (i < content.size() && patch_end >= content[i].m_offset + content[i].m_size)
          ++i;
        content.erase(content.begin() + insert_where, content.begin() + i);
        break;
      }
    }
  }

  // Insert the actual patch data.
  if (patch_size > 0 && external_filesize > 0)
  {
    BuilderContentSource source = patch.m_file_data_loader->MakeContentSource(
        external_filename, external_file_offset, std::min(patch_size, external_filesize),
        patch_start);
    content.emplace(content.begin() + insert_where, std::move(source));
    ++insert_where;
  }

  // Pad with zeroes if the patch file is smaller than the patch size.
  if (external_filesize < patch_size)
  {
    BuilderContentSource padding{patch_start + external_filesize, patch_size - external_filesize,
                                 ContentFixedByte{0}};
    content.emplace(content.begin() + insert_where, std::move(padding));
  }

  // Update the filesize of the file.
  file_node->m_size = target_filesize;

  // Drop any source past the new end of the file -- this can happen on file truncation.
  while (!content.empty() && content.back().m_offset >= target_filesize)
    content.pop_back();
}

static void ApplyPatchToFile(const Patch& patch, const File& file_patch,
                             DiscIO::FSTBuilderNode* file_node)
{
  // The last two bits of the offset seem to be ignored by actual Riivolution.
  ApplyPatchToFile(patch, file_node, file_patch.m_external, file_patch.m_offset & ~u64(3),
                   file_patch.m_fileoffset, file_patch.m_length, file_patch.m_resize);
}

static FSTBuilderNode* FindFileNodeInFST(std::string_view path, std::vector<FSTBuilderNode>* fst,
                                         bool create_if_not_exists)
{
  const size_t path_separator = path.find('/');
  const bool is_file = path_separator == std::string_view::npos;
  const std::string_view name = is_file ? path : path.substr(0, path_separator);
  const auto it = std::find_if(fst->begin(), fst->end(), [&](const FSTBuilderNode& node) {
    return Common::CaseInsensitiveEquals(node.m_filename, name);
  });

  if (it == fst->end())
  {
    if (!create_if_not_exists)
      return nullptr;

    if (is_file)
    {
      return &fst->emplace_back(
          DiscIO::FSTBuilderNode{std::string(name), 0, std::vector<BuilderContentSource>()});
    }

    auto& new_folder = fst->emplace_back(
        DiscIO::FSTBuilderNode{std::string(name), 0, std::vector<FSTBuilderNode>()});
    return FindFileNodeInFST(path.substr(path_separator + 1),
                             &std::get<std::vector<FSTBuilderNode>>(new_folder.m_content), true);
  }

  const bool is_existing_node_file = it->IsFile();
  if (is_file != is_existing_node_file)
    return nullptr;
  if (is_file)
    return &*it;

  return FindFileNodeInFST(path.substr(path_separator + 1),
                           &std::get<std::vector<FSTBuilderNode>>(it->m_content),
                           create_if_not_exists);
}

static DiscIO::FSTBuilderNode* FindFilenameNodeInFST(std::string_view filename,
                                                     std::vector<FSTBuilderNode>& fst)
{
  for (FSTBuilderNode& node : fst)
  {
    if (node.IsFolder())
    {
      if (DiscIO::FSTBuilderNode* result = FindFilenameNodeInFST(filename, node.GetFolderContent()))
        return result;
    }
    else if (Common::CaseInsensitiveEquals(node.m_filename, filename))
    {
      return &node;
    }
  }

  return nullptr;
}

static void ApplyFilePatchToFST(const Patch& patch, const File& file,
                                std::vector<DiscIO::FSTBuilderNode>* fst,
                                DiscIO::FSTBuilderNode* dol_node)
{
  if (!file.m_disc.empty() && file.m_disc[0] == '/')
  {
    // If the disc path starts with a / then we should patch that specific disc path.
    DiscIO::FSTBuilderNode* node =
        FindFileNodeInFST(std::string_view(file.m_disc).substr(1), fst, file.m_create);
    if (node)
      ApplyPatchToFile(patch, file, node);
  }
  else if (dol_node && Common::CaseInsensitiveEquals(file.m_disc, "main.dol"))
  {
    // Special case: If the filename is "main.dol", we want to patch the main executable.
    ApplyPatchToFile(patch, file, dol_node);
  }
  else
  {
    // Otherwise we want to patch the first file in the FST that matches that filename.
    if (DiscIO::FSTBuilderNode* node = FindFilenameNodeInFST(file.m_disc, *fst))
      ApplyPatchToFile(patch, file, node);
  }
}

static void ApplyFolderPatchToFST(const Patch& patch, const Folder& folder,
                                  std::vector<DiscIO::FSTBuilderNode>* fst,
                                  DiscIO::FSTBuilderNode* dol_node, std::string_view disc_path,
                                  std::string_view external_path)
{
  const auto external_files = patch.m_file_data_loader->GetFolderContents(external_path);
  for (const auto& child : external_files)
  {
    const auto combine_paths = [](std::string_view a, std::string_view b) {
      if (a.empty())
        return std::string(b);
      if (b.empty())
        return std::string(a);
      if (a.ends_with('/'))
        a.remove_suffix(1);
      if (b.starts_with('/'))
        b.remove_prefix(1);
      return fmt::format("{}/{}", a, b);
    };
    std::string child_disc_path = combine_paths(disc_path, child.m_filename);
    std::string child_external_path = combine_paths(external_path, child.m_filename);

    if (child.m_is_directory)
    {
      if (folder.m_recursive)
        ApplyFolderPatchToFST(patch, folder, fst, dol_node, child_disc_path, child_external_path);
    }
    else
    {
      File file;
      file.m_disc = std::move(child_disc_path);
      file.m_external = std::move(child_external_path);
      file.m_resize = folder.m_resize;
      file.m_create = folder.m_create;
      file.m_length = folder.m_length;
      ApplyFilePatchToFST(patch, file, fst, dol_node);
    }
  }
}

static void ApplyFolderPatchToFST(const Patch& patch, const Folder& folder,
                                  std::vector<DiscIO::FSTBuilderNode>* fst,
                                  DiscIO::FSTBuilderNode* dol_node)
{
  ApplyFolderPatchToFST(patch, folder, fst, dol_node, folder.m_disc, folder.m_external);
}

void ApplyPatchesToFiles(std::span<const Patch> patches, PatchIndex index,
                         std::vector<FSTBuilderNode>* fst, FSTBuilderNode* dol_node)
{
  for (const auto& patch : patches)
  {
    const auto& file_patches =
        index == PatchIndex::DolphinSysFiles ? patch.m_sys_file_patches : patch.m_file_patches;
    const auto& folder_patches =
        index == PatchIndex::DolphinSysFiles ? patch.m_sys_folder_patches : patch.m_folder_patches;

    for (const auto& file : file_patches)
      ApplyFilePatchToFST(patch, file, fst, dol_node);

    for (const auto& folder : folder_patches)
      ApplyFolderPatchToFST(patch, folder, fst, dol_node);
  }
}

static bool MemoryMatchesAt(const Core::CPUThreadGuard& guard, u32 offset,
                            std::span<const u8> value)
{
  for (u32 i = 0; i < value.size(); ++i)
  {
    auto result = PowerPC::MMU::HostTryReadU8(guard, offset + i);
    if (!result || result->value != value[i])
      return false;
  }
  return true;
}

static void ApplyMemoryPatch(const Core::CPUThreadGuard& guard, u32 offset,
                             std::span<const u8> value, std::span<const u8> original)
{
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
    return;

  if (value.empty())
    return;

  if (!original.empty() && !MemoryMatchesAt(guard, offset, original))
    return;

  auto& system = guard.GetSystem();
  const u32 size = static_cast<u32>(value.size());
  for (u32 i = 0; i < size; ++i)
    PowerPC::MMU::HostTryWriteU8(guard, value[i], offset + i);
  const u32 overlapping_hook_count = HLE::UnpatchRange(system, offset, offset + size);
  if (overlapping_hook_count != 0)
  {
    WARN_LOG_FMT(OSHLE, "Riivolution memory patch overlaps {} HLE hook(s) at {:08x} (size: {})",
                 overlapping_hook_count, offset, value.size());
  }
}

static std::vector<u8> GetMemoryPatchValue(const Patch& patch, const Memory& memory_patch)
{
  if (!memory_patch.m_valuefile.empty())
    return patch.m_file_data_loader->GetFileContents(memory_patch.m_valuefile);
  return memory_patch.m_value;
}

static void ApplyMemoryPatch(const Core::CPUThreadGuard& guard, const Patch& patch,
                             const Memory& memory_patch)
{
  if (memory_patch.m_offset == 0)
    return;

  ApplyMemoryPatch(guard, memory_patch.m_offset | 0x80000000,
                   GetMemoryPatchValue(patch, memory_patch), memory_patch.m_original);
}

static void ApplySearchMemoryPatch(const Core::CPUThreadGuard& guard, const Patch& patch,
                                   const Memory& memory_patch, u32 ram_start, u32 length)
{
  if (memory_patch.m_original.empty() || memory_patch.m_align == 0)
    return;

  const u32 stride = memory_patch.m_align;
  for (u32 i = 0; i < length - (stride - 1); i += stride)
  {
    const u32 address = ram_start + i;
    if (MemoryMatchesAt(guard, address, memory_patch.m_original))
    {
      ApplyMemoryPatch(guard, address, GetMemoryPatchValue(patch, memory_patch), {});
      break;
    }
  }
}

static void ApplyOcarinaMemoryPatch(const Core::CPUThreadGuard& guard, const Patch& patch,
                                    const Memory& memory_patch, u32 ram_start, u32 length)
{
  if (memory_patch.m_offset == 0)
    return;
  const std::vector<u8> value = GetMemoryPatchValue(patch, memory_patch);
  if (value.empty())
    return;

  auto& system = guard.GetSystem();
  for (u32 i = 0; i < length; i += 4)
  {
    // first find the pattern
    const u32 address = ram_start + i;
    if (MemoryMatchesAt(guard, address, value))
    {
      for (; i < length; i += 4)
      {
        // from the pattern find the next blr instruction
        const u32 blr_address = ram_start + i;
        auto blr = PowerPC::MMU::HostTryReadU32(guard, blr_address);
        if (blr && blr->value == 0x4e800020)
        {
          // and replace it with a jump to the given offset
          const u32 target = memory_patch.m_offset | 0x80000000;
          const u32 jmp = ((target - blr_address) & 0x03fffffc) | 0x48000000;
          PowerPC::MMU::HostTryWriteU32(guard, jmp, blr_address);
          const u32 overlapping_hook_count =
              HLE::UnpatchRange(system, blr_address, blr_address + 4);
          if (overlapping_hook_count != 0)
          {
            WARN_LOG_FMT(OSHLE, "Riivolution ocarina patch overlaps HLE hook at {}", blr_address);
          }
          return;
        }
      }
      return;
    }
  }
}

void ApplyGeneralMemoryPatches(const Core::CPUThreadGuard& guard, std::span<const Patch> patches)
{
  const auto& system = guard.GetSystem();
  const auto& system_memory = system.GetMemory();

  for (const auto& patch : patches)
  {
    for (const auto& memory : patch.m_memory_patches)
    {
      if (memory.m_ocarina)
        continue;

      if (memory.m_search)
        ApplySearchMemoryPatch(guard, patch, memory, 0x80000000, system_memory.GetRamSize());
      else
        ApplyMemoryPatch(guard, patch, memory);
    }
  }
}

void ApplyApploaderMemoryPatches(const Core::CPUThreadGuard& guard, std::span<const Patch> patches,
                                 u32 ram_address, u32 ram_length)
{
  for (const auto& patch : patches)
  {
    for (const auto& memory : patch.m_memory_patches)
    {
      if (!memory.m_ocarina && !memory.m_search)
        continue;

      if (memory.m_ocarina)
        ApplyOcarinaMemoryPatch(guard, patch, memory, ram_address, ram_length);
      else
        ApplySearchMemoryPatch(guard, patch, memory, ram_address, ram_length);
    }
  }
}

std::optional<SavegameRedirect> ExtractSavegameRedirect(std::span<const Patch> riivolution_patches)
{
  for (const auto& patch : riivolution_patches)
  {
    if (!patch.m_savegame_patches.empty())
    {
      const auto& save_patch = patch.m_savegame_patches[0];
      if (auto resolved = patch.m_file_data_loader->ResolveSavegameRedirectPath(save_patch.m_external))
        return SavegameRedirect{std::move(*resolved), save_patch.m_clone};
      return std::nullopt;
    }
  }
  return std::nullopt;
}
}  // namespace DiscIO::Riivolution
