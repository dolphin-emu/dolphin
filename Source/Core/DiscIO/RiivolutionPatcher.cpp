// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/RiivolutionPatcher.h"

#include <algorithm>
#include <cctype>
#include <locale>
#include <string>
#include <string_view>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
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
    auto r = MakeAbsoluteFromRelative(patch_root);
    if (r)
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

  std::string result = StringBeginsWith(external_relative_path, "/") ? m_sd_root : m_patch_root;
  std::string_view work = external_relative_path;

  // Strip away all leading and trailing path separators.
  while (StringBeginsWith(work, "/"))
    work = work.substr(1);
  while (StringEndsWith(work, "/"))
    work = work.substr(0, work.size() - 1);
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
    else
    {
      // We're going down a level.
      ++depth;

      // Append path element to result string.
      result += '/';
      result += element;
    }

    // If this was the last path element, we're done.
    if (separator_position == std::string_view::npos)
      break;

    // Remove element from work string.
    work = work.substr(separator_position + 1);

    // Remove any potential extra path separators.
    while (StringBeginsWith(work, "/"))
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
  ::File::IOFile f(*path, "rb");
  if (!f)
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
    std::get<ContentFile>(after->m_source).m_offset += before->m_size;
  else if (std::holds_alternative<const u8*>(after->m_source))
    std::get<const u8*>(after->m_source) += before->m_size;
  else if (std::holds_alternative<ContentPartition>(after->m_source))
    std::get<ContentPartition>(after->m_source).m_offset += before->m_size;
  else if (std::holds_alternative<ContentVolume>(after->m_source))
    std::get<ContentVolume>(after->m_source).m_offset += before->m_size;
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
  ApplyPatchToFile(patch, file_node, file_patch.m_external, file_patch.m_offset,
                   file_patch.m_fileoffset, file_patch.m_length, file_patch.m_resize);
}

static bool CaseInsensitiveEquals(std::string_view a, std::string_view b)
{
  if (a.size() != b.size())
    return false;
  return std::equal(a.begin(), a.end(), b.begin(), [](char ca, char cb) {
    return std::tolower(ca, std::locale::classic()) == std::tolower(cb, std::locale::classic());
  });
}

static FSTBuilderNode* FindFileNodeInFST(std::string_view path, std::vector<FSTBuilderNode>* fst,
                                         bool create_if_not_exists)
{
  const size_t path_separator = path.find('/');
  const bool is_file = path_separator == std::string_view::npos;
  const std::string_view name = is_file ? path : path.substr(0, path_separator);
  const auto it = std::find_if(fst->begin(), fst->end(), [&](const FSTBuilderNode& node) {
    return CaseInsensitiveEquals(node.m_filename, name);
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

static void FindFilenameNodesInFST(std::vector<DiscIO::FSTBuilderNode*>* nodes_out,
                                   std::string_view filename, std::vector<FSTBuilderNode>* fst)
{
  for (FSTBuilderNode& node : *fst)
  {
    if (node.IsFolder())
    {
      FindFilenameNodesInFST(nodes_out, filename,
                             &std::get<std::vector<FSTBuilderNode>>(node.m_content));
    }
    else if (node.m_filename == filename)
    {
      nodes_out->push_back(&node);
    }
  }
}

static void ApplyFolderPatchToFST(const Patch& patch, const Folder& folder,
                                  const std::vector<FileDataLoader::Node>& external_files,
                                  const std::string& external_path, bool recursive,
                                  std::string_view disc_path,
                                  std::vector<DiscIO::FSTBuilderNode>* fst)
{
  for (const auto& child : external_files)
  {
    const std::string child_disc_path = std::string(disc_path) + "/" + child.m_filename;
    const std::string child_external_path = external_path + "/" + child.m_filename;
    if (child.m_is_directory)
    {
      if (recursive)
      {
        ApplyFolderPatchToFST(patch, folder,
                              patch.m_file_data_loader->GetFolderContents(child_external_path),
                              child_external_path, recursive, child_disc_path, fst);
      }
    }
    else
    {
      DiscIO::FSTBuilderNode* node = FindFileNodeInFST(child_disc_path, fst, folder.m_create);
      if (node)
        ApplyPatchToFile(patch, node, child_external_path, 0, 0, folder.m_length, folder.m_resize);
    }
  }
}

static void ApplyUnknownFolderPatchToFST(const Patch& patch, const Folder& folder,
                                         const std::vector<FileDataLoader::Node>& external_files,
                                         const std::string& external_path, bool recursive,
                                         std::vector<DiscIO::FSTBuilderNode>* fst)
{
  for (const auto& child : external_files)
  {
    const std::string child_external_path = external_path + "/" + child.m_filename;
    if (child.m_is_directory)
    {
      if (recursive)
      {
        ApplyUnknownFolderPatchToFST(
            patch, folder, patch.m_file_data_loader->GetFolderContents(child_external_path),
            child_external_path, recursive, fst);
      }
    }
    else
    {
      std::vector<DiscIO::FSTBuilderNode*> nodes;
      FindFilenameNodesInFST(&nodes, child.m_filename, fst);
      for (auto* node : nodes)
        ApplyPatchToFile(patch, node, child_external_path, 0, 0, folder.m_length, folder.m_resize);
    }
  }
}

void ApplyPatchesToFiles(const std::vector<Patch>& patches,
                         std::vector<DiscIO::FSTBuilderNode>* fst, DiscIO::FSTBuilderNode* dol_node)
{
  // For file searching purposes, Riivolution assumes that the game's main.dol is in the root of the
  // file system. So to avoid doing a bunch of special case handling for that, we just put a node
  // for this into the FST and remove it again after the file patching is done.
  dol_node->m_filename = "main.dol";
  fst->push_back(*dol_node);

  for (const auto& patch : patches)
  {
    for (const auto& file : patch.m_file_patches)
    {
      if (!file.m_disc.empty() && file.m_disc[0] == '/')
      {
        // If the disc path starts with a / then we should patch that specific disc path.
        DiscIO::FSTBuilderNode* node =
            FindFileNodeInFST(std::string_view(file.m_disc).substr(1), fst, file.m_create);
        if (node)
          ApplyPatchToFile(patch, file, node);
      }
      else
      {
        // Otherwise we want to patch any file on the entire disc matching that filename.
        std::vector<DiscIO::FSTBuilderNode*> nodes;
        FindFilenameNodesInFST(&nodes, file.m_disc, fst);
        for (auto* node : nodes)
          ApplyPatchToFile(patch, file, node);
      }
    }

    for (const auto& folder : patch.m_folder_patches)
    {
      const auto external_files = patch.m_file_data_loader->GetFolderContents(folder.m_external);

      std::string_view disc_path = folder.m_disc;
      while (!disc_path.empty() && disc_path.front() == '/')
        disc_path = disc_path.substr(1);
      while (!disc_path.empty() && disc_path.back() == '/')
        disc_path = disc_path.substr(0, disc_path.size() - 1);

      if (disc_path.empty())
      {
        ApplyUnknownFolderPatchToFST(patch, folder, external_files, folder.m_external,
                                     folder.m_recursive, fst);
      }
      else
      {
        ApplyFolderPatchToFST(patch, folder, external_files, folder.m_external, folder.m_recursive,
                              disc_path, fst);
      }
    }
  }

  auto main_dol_node_in_fst =
      std::find_if(fst->begin(), fst->end(), [&](const DiscIO::FSTBuilderNode& node) {
        return node.m_filename == "main.dol";
      });
  if (main_dol_node_in_fst != fst->end())
  {
    *dol_node = *main_dol_node_in_fst;
    fst->erase(main_dol_node_in_fst);
  }
}

static bool MemoryMatchesAt(u32 offset, const std::vector<u8>& value)
{
  for (u32 i = 0; i < value.size(); ++i)
  {
    auto result = PowerPC::HostTryReadU8(offset + i);
    if (!result || result->value != value[i])
      return false;
  }
  return true;
}

static void ApplyMemoryPatch(u32 offset, const std::vector<u8>& value,
                             const std::vector<u8>& original)
{
  if (value.empty())
    return;

  if (!original.empty() && !MemoryMatchesAt(offset, original))
    return;

  for (u32 i = 0; i < value.size(); ++i)
    PowerPC::HostTryWriteU8(value[i], offset + i);
}

static std::vector<u8> GetMemoryPatchValue(const Patch& patch, const Memory& memory_patch)
{
  if (!memory_patch.m_valuefile.empty())
    return patch.m_file_data_loader->GetFileContents(memory_patch.m_valuefile);
  return memory_patch.m_value;
}

static void ApplyMemoryPatch(const Patch& patch, const Memory& memory_patch)
{
  ApplyMemoryPatch(memory_patch.m_offset | 0x80000000, GetMemoryPatchValue(patch, memory_patch),
                   memory_patch.m_original);
}

static void ApplySearchMemoryPatch(const Patch& patch, const Memory& memory_patch)
{
  if (memory_patch.m_original.empty())
    return;

  const u32 ram_size = ::Memory::GetRamSize();
  const u32 stride = memory_patch.m_align < 1 ? 1 : memory_patch.m_align;
  for (u32 i = 0; i < ram_size - (stride - 1); i += stride)
  {
    const u32 address = i | 0x80000000;
    if (MemoryMatchesAt(address, memory_patch.m_original))
    {
      ApplyMemoryPatch(address, GetMemoryPatchValue(patch, memory_patch), {});
      break;
    }
  }
}

static void ApplyOcarinaMemoryPatch(const Patch& patch, const Memory& memory_patch)
{
  if (memory_patch.m_value.empty())
    return;

  const u32 ram_size = ::Memory::GetRamSize();
  for (u32 i = 0; i < ram_size; i += 4)
  {
    // first find the pattern
    const u32 address = i | 0x80000000;
    if (MemoryMatchesAt(address, memory_patch.m_value))
    {
      for (; i < ram_size; i += 4)
      {
        // from the pattern find the next blr instruction
        const u32 blr_address = i | 0x80000000;
        auto blr = PowerPC::HostTryReadU32(blr_address);
        if (blr && blr->value == 0x4e800020)
        {
          // and replace it with a jump to the given offset
          const u32 target = memory_patch.m_offset | 0x80000000;
          const u32 jmp = ((target - blr_address) & 0x03fffffc) | 0x48000000;
          PowerPC::HostTryWriteU32(jmp, blr_address);
          return;
        }
      }
      return;
    }
  }
}

void ApplyPatchesToMemory(const std::vector<Patch>& patches)
{
  for (const auto& patch : patches)
  {
    for (const auto& memory : patch.m_memory_patches)
    {
      if (memory.m_ocarina)
        ApplyOcarinaMemoryPatch(patch, memory);
      else if (memory.m_search)
        ApplySearchMemoryPatch(patch, memory);
      else
        ApplyMemoryPatch(patch, memory);
    }
  }
}
}  // namespace DiscIO::Riivolution
