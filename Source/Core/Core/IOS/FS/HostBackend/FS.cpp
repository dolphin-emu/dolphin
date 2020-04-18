// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/HostBackend/FS.h"
#include "Core/IOS/IOS.h"

namespace IOS::HLE::FS
{
std::string HostFileSystem::BuildFilename(const std::string& wii_path) const
{
  if (wii_path.compare(0, 1, "/") == 0)
    return m_root_path + Common::EscapePath(wii_path);

  ASSERT(false);
  return m_root_path;
}

// Get total filesize of contents of a directory (recursive)
// Only used for ES_GetUsage atm, could be useful elsewhere?
static u64 ComputeTotalFileSize(const File::FSTEntry& parent_entry)
{
  u64 sizeOfFiles = 0;
  for (const File::FSTEntry& entry : parent_entry.children)
  {
    if (entry.isDirectory)
      sizeOfFiles += ComputeTotalFileSize(entry);
    else
      sizeOfFiles += entry.size;
  }
  return sizeOfFiles;
}

namespace
{
struct SerializedFstEntry
{
  std::string_view GetName() const { return {name.data(), strnlen(name.data(), name.size())}; }
  void SetName(std::string_view new_name)
  {
    std::memcpy(name.data(), new_name.data(), std::min(name.size(), new_name.length()));
  }

  /// File name
  std::array<char, 12> name{};
  /// File owner user ID
  Common::BigEndianValue<Uid> uid{};
  /// File owner group ID
  Common::BigEndianValue<Gid> gid{};
  /// Is this a file or a directory?
  bool is_file = false;
  /// File access modes
  Modes modes{};
  /// File attribute
  FileAttribute attribute{};
  /// Unknown property
  Common::BigEndianValue<u32> x3{};
  /// Number of children
  Common::BigEndianValue<u32> num_children{};
};
static_assert(std::is_standard_layout<SerializedFstEntry>());
static_assert(sizeof(SerializedFstEntry) == 0x20);

template <typename T>
auto GetMetadataFields(T& obj)
{
  return std::tie(obj.uid, obj.gid, obj.is_file, obj.modes, obj.attribute);
}

auto GetNamePredicate(const std::string& name)
{
  return [&name](const auto& entry) { return entry.name == name; };
}
}  // namespace

bool HostFileSystem::FstEntry::CheckPermission(Uid caller_uid, Gid caller_gid,
                                               Mode requested_mode) const
{
  if (caller_uid == 0)
    return true;
  Mode file_mode = data.modes.other;
  if (data.uid == caller_uid)
    file_mode = data.modes.owner;
  else if (data.gid == caller_gid)
    file_mode = data.modes.group;
  return (u8(requested_mode) & u8(file_mode)) == u8(requested_mode);
}

HostFileSystem::HostFileSystem(const std::string& root_path) : m_root_path{root_path}
{
  File::CreateFullPath(m_root_path + "/");
  ResetFst();
  LoadFst();
}

HostFileSystem::~HostFileSystem() = default;

std::string HostFileSystem::GetFstFilePath() const
{
  return fmt::format("{}/fst.bin", m_root_path);
}

void HostFileSystem::ResetFst()
{
  m_root_entry = {};
  m_root_entry.name = "/";
  // Mode 0x16 (Directory | Owner_None | Group_Read | Other_Read) in the FS sysmodule
  m_root_entry.data.modes = {Mode::None, Mode::Read, Mode::Read};
}

void HostFileSystem::LoadFst()
{
  File::IOFile file{GetFstFilePath(), "rb"};
  // Existing filesystems will not have a FST. This is not a problem,
  // as the rest of HostFileSystem will use sane defaults.
  if (!file)
    return;

  const auto parse_entry = [&file](const auto& parse, size_t depth) -> std::optional<FstEntry> {
    if (depth > MaxPathDepth)
      return std::nullopt;

    SerializedFstEntry entry;
    if (!file.ReadArray(&entry, 1))
      return std::nullopt;

    FstEntry result;
    result.name = entry.GetName();
    GetMetadataFields(result.data) = GetMetadataFields(entry);
    for (size_t i = 0; i < entry.num_children; ++i)
    {
      const auto maybe_child = parse(parse, depth + 1);
      if (!maybe_child.has_value())
        return std::nullopt;
      result.children.push_back(*maybe_child);
    }
    return result;
  };

  const auto root_entry = parse_entry(parse_entry, 0);
  if (!root_entry.has_value())
  {
    ERROR_LOG(IOS_FS, "Failed to parse FST: at least one of the entries was invalid");
    return;
  }
  m_root_entry = *root_entry;
}

void HostFileSystem::SaveFst()
{
  std::vector<SerializedFstEntry> to_write;
  auto collect_entries = [&to_write](const auto& collect, const FstEntry& entry) -> void {
    SerializedFstEntry& serialized = to_write.emplace_back();
    serialized.SetName(entry.name);
    GetMetadataFields(serialized) = GetMetadataFields(entry.data);
    serialized.num_children = u32(entry.children.size());
    for (const FstEntry& child : entry.children)
      collect(collect, child);
  };
  collect_entries(collect_entries, m_root_entry);

  const std::string dest_path = GetFstFilePath();
  const std::string temp_path = File::GetTempFilenameForAtomicWrite(dest_path);
  {
    // This temporary file must be closed before it can be renamed.
    File::IOFile file{temp_path, "wb"};
    if (!file.WriteArray(to_write.data(), to_write.size()))
    {
      PanicAlert("IOS_FS: Failed to write new FST");
      return;
    }
  }
  if (!File::Rename(temp_path, dest_path))
    PanicAlert("IOS_FS: Failed to rename temporary FST file");
}

HostFileSystem::FstEntry* HostFileSystem::GetFstEntryForPath(const std::string& path)
{
  if (path == "/")
    return &m_root_entry;

  if (!IsValidNonRootPath(path))
    return nullptr;

  const File::FileInfo host_file_info{BuildFilename(path)};
  if (!host_file_info.Exists())
    return nullptr;

  FstEntry* entry = &m_root_entry;
  std::string complete_path = "";
  for (const std::string& component : SplitString(std::string(path.substr(1)), '/'))
  {
    complete_path += '/' + component;
    const auto next =
        std::find_if(entry->children.begin(), entry->children.end(), GetNamePredicate(component));
    if (next != entry->children.end())
    {
      entry = &*next;
    }
    else
    {
      // Fall back to dummy data to avoid breaking existing filesystems.
      // This code path is also reached when creating a new file or directory;
      // proper metadata is filled in later.
      INFO_LOG(IOS_FS, "Creating a default entry for %s", complete_path.c_str());
      entry = &entry->children.emplace_back();
      entry->name = component;
      entry->data.modes = {Mode::ReadWrite, Mode::ReadWrite, Mode::ReadWrite};
    }
  }

  entry->data.is_file = host_file_info.IsFile();
  if (entry->data.is_file && !entry->children.empty())
  {
    WARN_LOG(IOS_FS, "%s is a file but also has children; clearing children", path.c_str());
    entry->children.clear();
  }

  return entry;
}

void HostFileSystem::DoState(PointerWrap& p)
{
  // Temporarily close the file, to prevent any issues with the savestating of /tmp
  for (Handle& handle : m_handles)
    handle.host_file.reset();

  // handle /tmp
  std::string Path = BuildFilename("/tmp");
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    File::DeleteDirRecursively(Path);
    File::CreateDir(Path);

    // now restore from the stream
    while (1)
    {
      char type = 0;
      p.Do(type);
      if (!type)
        break;
      std::string file_name;
      p.Do(file_name);
      std::string name = Path + "/" + file_name;
      switch (type)
      {
      case 'd':
      {
        File::CreateDir(name);
        break;
      }
      case 'f':
      {
        u32 size = 0;
        p.Do(size);

        File::IOFile handle(name, "wb");
        char buf[65536];
        u32 count = size;
        while (count > 65536)
        {
          p.DoArray(buf);
          handle.WriteArray(&buf[0], 65536);
          count -= 65536;
        }
        p.DoArray(&buf[0], count);
        handle.WriteArray(&buf[0], count);
        break;
      }
      }
    }
  }
  else
  {
    // recurse through tmp and save dirs and files

    File::FSTEntry parent_entry = File::ScanDirectoryTree(Path, true);
    std::deque<File::FSTEntry> todo;
    todo.insert(todo.end(), parent_entry.children.begin(), parent_entry.children.end());

    while (!todo.empty())
    {
      File::FSTEntry& entry = todo.front();
      std::string name = entry.physicalName;
      name.erase(0, Path.length() + 1);
      char type = entry.isDirectory ? 'd' : 'f';
      p.Do(type);
      p.Do(name);
      if (entry.isDirectory)
      {
        todo.insert(todo.end(), entry.children.begin(), entry.children.end());
      }
      else
      {
        u32 size = (u32)entry.size;
        p.Do(size);

        File::IOFile handle(entry.physicalName, "rb");
        char buf[65536];
        u32 count = size;
        while (count > 65536)
        {
          handle.ReadArray(&buf[0], 65536);
          p.DoArray(buf);
          count -= 65536;
        }
        handle.ReadArray(&buf[0], count);
        p.DoArray(&buf[0], count);
      }
      todo.pop_front();
    }

    char type = 0;
    p.Do(type);
  }

  for (Handle& handle : m_handles)
  {
    p.Do(handle.opened);
    p.Do(handle.mode);
    p.Do(handle.wii_path);
    p.Do(handle.file_offset);
    if (handle.opened)
      handle.host_file = OpenHostFile(BuildFilename(handle.wii_path));
  }
}

ResultCode HostFileSystem::Format(Uid uid)
{
  if (uid != 0)
    return ResultCode::AccessDenied;
  if (m_root_path.empty())
    return ResultCode::AccessDenied;
  const std::string root = BuildFilename("/");
  if (!File::DeleteDirRecursively(root) || !File::CreateDir(root))
    return ResultCode::UnknownError;
  ResetFst();
  SaveFst();
  // Reset and close all handles.
  m_handles = {};
  return ResultCode::Success;
}

ResultCode HostFileSystem::CreateFileOrDirectory(Uid uid, Gid gid, const std::string& path,
                                                 FileAttribute attr, Modes modes, bool is_file)
{
  if (!IsValidNonRootPath(path) || !std::all_of(path.begin(), path.end(), IsPrintableCharacter))
    return ResultCode::Invalid;

  if (!is_file && std::count(path.begin(), path.end(), '/') > int(MaxPathDepth))
    return ResultCode::TooManyPathComponents;

  const auto split_path = SplitPathAndBasename(path);
  const std::string host_path = BuildFilename(path);

  FstEntry* parent = GetFstEntryForPath(split_path.parent);
  if (!parent)
    return ResultCode::NotFound;

  if (!parent->CheckPermission(uid, gid, Mode::Write))
    return ResultCode::AccessDenied;

  if (File::Exists(host_path))
    return ResultCode::AlreadyExists;

  const bool ok = is_file ? File::CreateEmptyFile(host_path) : File::CreateDir(host_path);
  if (!ok)
  {
    ERROR_LOG(IOS_FS, "Failed to create file or directory: %s", host_path.c_str());
    return ResultCode::UnknownError;
  }

  FstEntry* child = GetFstEntryForPath(path);
  *child = {};
  child->name = split_path.file_name;
  child->data.is_file = is_file;
  child->data.modes = modes;
  child->data.uid = uid;
  child->data.gid = gid;
  child->data.attribute = attr;
  SaveFst();
  return ResultCode::Success;
}

ResultCode HostFileSystem::CreateFile(Uid uid, Gid gid, const std::string& path, FileAttribute attr,
                                      Modes modes)
{
  return CreateFileOrDirectory(uid, gid, path, attr, modes, true);
}

ResultCode HostFileSystem::CreateDirectory(Uid uid, Gid gid, const std::string& path,
                                           FileAttribute attr, Modes modes)
{
  return CreateFileOrDirectory(uid, gid, path, attr, modes, false);
}

bool HostFileSystem::IsFileOpened(const std::string& path) const
{
  return std::any_of(m_handles.begin(), m_handles.end(), [&path](const Handle& handle) {
    return handle.opened && handle.wii_path == path;
  });
}

bool HostFileSystem::IsDirectoryInUse(const std::string& path) const
{
  return std::any_of(m_handles.begin(), m_handles.end(), [&path](const Handle& handle) {
    return handle.opened && StringBeginsWith(handle.wii_path, path);
  });
}

ResultCode HostFileSystem::Delete(Uid uid, Gid gid, const std::string& path)
{
  if (!IsValidNonRootPath(path))
    return ResultCode::Invalid;

  const std::string host_path = BuildFilename(path);
  const auto split_path = SplitPathAndBasename(path);

  FstEntry* parent = GetFstEntryForPath(split_path.parent);
  if (!parent)
    return ResultCode::NotFound;

  if (!parent->CheckPermission(uid, gid, Mode::Write))
    return ResultCode::AccessDenied;

  if (!File::Exists(host_path))
    return ResultCode::NotFound;

  if (File::IsFile(host_path) && !IsFileOpened(path))
    File::Delete(host_path);
  else if (File::IsDirectory(host_path) && !IsDirectoryInUse(path))
    File::DeleteDirRecursively(host_path);
  else
    return ResultCode::InUse;

  const auto it = std::find_if(parent->children.begin(), parent->children.end(),
                               GetNamePredicate(split_path.file_name));
  if (it != parent->children.end())
    parent->children.erase(it);
  SaveFst();

  return ResultCode::Success;
}

ResultCode HostFileSystem::Rename(Uid uid, Gid gid, const std::string& old_path,
                                  const std::string& new_path)
{
  if (!IsValidNonRootPath(old_path) || !IsValidNonRootPath(new_path))
    return ResultCode::Invalid;

  const auto split_old_path = SplitPathAndBasename(old_path);
  const auto split_new_path = SplitPathAndBasename(new_path);

  FstEntry* old_parent = GetFstEntryForPath(split_old_path.parent);
  FstEntry* new_parent = GetFstEntryForPath(split_new_path.parent);
  if (!old_parent || !new_parent)
    return ResultCode::NotFound;

  if (!old_parent->CheckPermission(uid, gid, Mode::Write) ||
      !new_parent->CheckPermission(uid, gid, Mode::Write))
  {
    return ResultCode::AccessDenied;
  }

  FstEntry* entry = GetFstEntryForPath(old_path);
  if (!entry)
    return ResultCode::NotFound;

  // For files, the file name is not allowed to change.
  if (entry->data.is_file && split_old_path.file_name != split_new_path.file_name)
    return ResultCode::Invalid;

  if ((!entry->data.is_file && IsDirectoryInUse(old_path)) ||
      (entry->data.is_file && IsFileOpened(old_path)))
  {
    return ResultCode::InUse;
  }

  const std::string host_old_path = BuildFilename(old_path);
  const std::string host_new_path = BuildFilename(new_path);

  // If there is already something of the same type at the new path, delete it.
  if (File::Exists(host_new_path))
  {
    const bool old_is_file = File::IsFile(host_old_path);
    const bool new_is_file = File::IsFile(host_new_path);
    if (old_is_file && new_is_file)
      File::Delete(host_new_path);
    else if (!old_is_file && !new_is_file)
      File::DeleteDirRecursively(host_new_path);
    else
      return ResultCode::Invalid;
  }

  if (!File::Rename(host_old_path, host_new_path))
  {
    ERROR_LOG(IOS_FS, "Rename %s to %s - failed", host_old_path.c_str(), host_new_path.c_str());
    return ResultCode::NotFound;
  }

  // Finally, remove the child from the old parent and move it to the new parent.
  const auto it = std::find_if(old_parent->children.begin(), old_parent->children.end(),
                               GetNamePredicate(split_old_path.file_name));
  FstEntry* new_entry = GetFstEntryForPath(new_path);
  if (it != old_parent->children.end())
  {
    *new_entry = *it;
    old_parent->children.erase(it);
  }
  new_entry->name = split_new_path.file_name;
  SaveFst();

  return ResultCode::Success;
}

Result<std::vector<std::string>> HostFileSystem::ReadDirectory(Uid uid, Gid gid,
                                                               const std::string& path)
{
  if (!IsValidPath(path))
    return ResultCode::Invalid;

  const FstEntry* entry = GetFstEntryForPath(path);
  if (!entry)
    return ResultCode::NotFound;

  if (!entry->CheckPermission(uid, gid, Mode::Read))
    return ResultCode::AccessDenied;

  if (entry->data.is_file)
    return ResultCode::Invalid;

  const std::string host_path = BuildFilename(path);
  File::FSTEntry host_entry = File::ScanDirectoryTree(host_path, false);
  for (File::FSTEntry& child : host_entry.children)
  {
    // Decode escaped invalid file system characters so that games (such as
    // Harry Potter and the Half-Blood Prince) can find what they expect.
    child.virtualName = Common::UnescapeFileName(child.virtualName);
  }

  // Sort files according to their order in the FST tree (issue 10234).
  // The result should look like this:
  //     [FilesNotInFst, ..., OldestFileInFst, ..., NewestFileInFst]
  std::unordered_map<std::string_view, int> sort_keys;
  sort_keys.reserve(entry->children.size());
  for (size_t i = 0; i < entry->children.size(); ++i)
    sort_keys.emplace(entry->children[i].name, int(i));

  const auto get_key = [&sort_keys](std::string_view key) {
    const auto it = sort_keys.find(key);
    // As a fallback, files that are not in the FST are put at the beginning.
    return it != sort_keys.end() ? it->second : -1;
  };

  // Now sort in reverse order because Nintendo traverses a linked list
  // in which new elements are inserted at the front.
  std::sort(host_entry.children.begin(), host_entry.children.end(),
            [&get_key](const File::FSTEntry& one, const File::FSTEntry& two) {
              const int key1 = get_key(one.virtualName);
              const int key2 = get_key(two.virtualName);
              if (key1 != key2)
                return key1 > key2;

              // For files that are not in the FST, sort lexicographically to ensure that
              // results are consistent no matter what the underlying filesystem is.
              return one.virtualName > two.virtualName;
            });

  std::vector<std::string> output;
  for (const File::FSTEntry& child : host_entry.children)
    output.emplace_back(child.virtualName);
  return output;
}

Result<Metadata> HostFileSystem::GetMetadata(Uid uid, Gid gid, const std::string& path)
{
  const FstEntry* entry = nullptr;
  if (path == "/")
  {
    entry = &m_root_entry;
  }
  else
  {
    if (!IsValidNonRootPath(path))
      return ResultCode::Invalid;

    const auto split_path = SplitPathAndBasename(path);
    const FstEntry* parent = GetFstEntryForPath(split_path.parent);
    if (!parent)
      return ResultCode::NotFound;
    if (!parent->CheckPermission(uid, gid, Mode::Read))
      return ResultCode::AccessDenied;
    entry = GetFstEntryForPath(path);
  }

  if (!entry)
    return ResultCode::NotFound;

  Metadata metadata = entry->data;
  metadata.size = File::GetSize(BuildFilename(path));
  return metadata;
}

ResultCode HostFileSystem::SetMetadata(Uid caller_uid, const std::string& path, Uid uid, Gid gid,
                                       FileAttribute attr, Modes modes)
{
  if (!IsValidPath(path))
    return ResultCode::Invalid;

  FstEntry* entry = GetFstEntryForPath(path);
  if (!entry)
    return ResultCode::NotFound;

  if (caller_uid != 0 && caller_uid != entry->data.uid)
    return ResultCode::AccessDenied;
  if (caller_uid != 0 && uid != entry->data.uid)
    return ResultCode::AccessDenied;

  const bool is_empty = File::GetSize(BuildFilename(path)) == 0;
  if (entry->data.uid != uid && entry->data.is_file && !is_empty)
    return ResultCode::FileNotEmpty;

  entry->data.gid = gid;
  entry->data.uid = uid;
  entry->data.attribute = attr;
  entry->data.modes = modes;
  SaveFst();

  return ResultCode::Success;
}

Result<NandStats> HostFileSystem::GetNandStats()
{
  WARN_LOG(IOS_FS, "GET STATS - returning static values for now");

  // TODO: scrape the real amounts from somewhere...
  NandStats stats{};
  stats.cluster_size = 0x4000;
  stats.free_clusters = 0x5DEC;
  stats.used_clusters = 0x1DD4;
  stats.bad_clusters = 0x10;
  stats.reserved_clusters = 0x02F0;
  stats.free_inodes = 0x146B;
  stats.used_inodes = 0x0394;

  return stats;
}

Result<DirectoryStats> HostFileSystem::GetDirectoryStats(const std::string& wii_path)
{
  if (!IsValidPath(wii_path))
    return ResultCode::Invalid;

  DirectoryStats stats{};
  std::string path(BuildFilename(wii_path));
  if (File::IsDirectory(path))
  {
    File::FSTEntry parent_dir = File::ScanDirectoryTree(path, true);
    // add one for the folder itself
    stats.used_inodes = 1 + (u32)parent_dir.size;

    u64 total_size = ComputeTotalFileSize(parent_dir);  // "Real" size to convert to nand blocks

    stats.used_clusters = (u32)(total_size / (16 * 1024));  // one block is 16kb
  }
  else
  {
    WARN_LOG(IOS_FS, "fsBlock failed, cannot find directory: %s", path.c_str());
  }
  return stats;
}

}  // namespace IOS::HLE::FS
