// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/FS/HostBackend/FS.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/Movie.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

namespace IOS::HLE::FS
{
constexpr u32 BUFFER_CHUNK_SIZE = 65536;

HostFileSystem::HostFilename HostFileSystem::BuildFilename(const std::string& wii_path) const
{
  for (const auto& redirect : m_nand_redirects)
  {
    if (wii_path.starts_with(redirect.source_path) &&
        (wii_path.size() == redirect.source_path.size() ||
         wii_path[redirect.source_path.size()] == '/'))
    {
      std::string relative_to_redirect = wii_path.substr(redirect.source_path.size());
      return HostFilename{redirect.target_path + Common::EscapePath(relative_to_redirect), true};
    }
  }

  if (wii_path.starts_with("/"))
    return HostFilename{m_root_path + Common::EscapePath(wii_path), false};

  ASSERT_MSG(IOS_FS, false, "Invalid Wii path '{}' given to BuildFilename()", wii_path);
  return HostFilename{m_root_path, false};
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

// Convert the host directory entries into ones that can be exposed to the emulated system.
static u64 FixupDirectoryEntries(File::FSTEntry* dir, bool is_root)
{
  u64 removed_entries = 0;
  for (auto it = dir->children.begin(); it != dir->children.end();)
  {
    // Drop files in the root of the Wii NAND folder, since we store extra files there that the
    // emulated system shouldn't know about.
    if (is_root && !it->isDirectory)
    {
      ++removed_entries;
      it = dir->children.erase(it);
      continue;
    }

    // Decode escaped invalid file system characters so that games (such as Harry Potter and the
    // Half-Blood Prince) can find what they expect.
    if (it->virtualName.find("__") != std::string::npos)
      it->virtualName = Common::UnescapeFileName(it->virtualName);

    // Drop files that have too long filenames.
    if (!IsValidFilename(it->virtualName))
    {
      if (it->isDirectory)
        removed_entries += it->size;
      ++removed_entries;
      it = dir->children.erase(it);
      continue;
    }

    if (dir->isDirectory)
      removed_entries += FixupDirectoryEntries(&*it, false);

    ++it;
  }
  dir->size -= removed_entries;
  return removed_entries;
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

HostFileSystem::HostFileSystem(const std::string& root_path,
                               std::vector<NandRedirect> nand_redirects)
    : m_root_path{root_path}, m_nand_redirects(std::move(nand_redirects))
{
  while (m_root_path.ends_with('/'))
    m_root_path.pop_back();
  File::CreateFullPath(m_root_path + '/');
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
    ERROR_LOG_FMT(IOS_FS, "Failed to parse FST: at least one of the entries was invalid");
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
      PanicAlertFmt("IOS_FS: Failed to write new FST");
      return;
    }
  }
  if (!File::Rename(temp_path, dest_path))
    PanicAlertFmt("IOS_FS: Failed to rename temporary FST file");
}

HostFileSystem::FstEntry* HostFileSystem::GetFstEntryForPath(const std::string& path)
{
  if (path == "/")
    return &m_root_entry;

  if (!IsValidNonRootPath(path))
    return nullptr;

  auto host_file = BuildFilename(path);
  const File::FileInfo host_file_info{host_file.host_path};
  if (!host_file_info.Exists())
    return nullptr;

  FstEntry* entry = host_file.is_redirect ? &m_redirect_fst : &m_root_entry;
  std::string complete_path = "";
  for (const std::string& component : SplitString(std::string(path.substr(1)), '/'))
  {
    complete_path += '/' + component;
    const auto next = std::ranges::find_if(entry->children, GetNamePredicate(component));
    if (next != entry->children.end())
    {
      entry = &*next;
    }
    else
    {
      // Fall back to dummy data to avoid breaking existing filesystems.
      // This code path is also reached when creating a new file or directory;
      // proper metadata is filled in later.
      INFO_LOG_FMT(IOS_FS, "Creating a default entry for {} ({})", complete_path,
                   host_file.is_redirect ? "redirect" : "NAND");
      entry = &entry->children.emplace_back();
      entry->name = component;
      entry->data.modes = {Mode::ReadWrite, Mode::ReadWrite, Mode::ReadWrite};
    }
  }

  entry->data.is_file = host_file_info.IsFile();
  if (entry->data.is_file && !entry->children.empty())
  {
    WARN_LOG_FMT(IOS_FS, "{} is a file but also has children; clearing children", path);
    entry->children.clear();
  }

  return entry;
}

void HostFileSystem::DoStateRead(PointerWrap& p, std::string start_directory_path)
{
  std::string path = BuildFilename(start_directory_path).host_path;
  File::DeleteDirRecursively(path);
  File::CreateDir(path);

  // now restore from the stream
  while (true)
  {
    char type = 0;
    p.Do(type);
    if (!type)
      break;
    std::string file_name;
    p.Do(file_name);
    std::string name = path + "/" + file_name;
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
      char buf[BUFFER_CHUNK_SIZE];
      u32 count = size;
      while (count > BUFFER_CHUNK_SIZE)
      {
        p.DoArray(buf);
        handle.WriteArray(&buf[0], BUFFER_CHUNK_SIZE);
        count -= BUFFER_CHUNK_SIZE;
      }
      p.DoArray(&buf[0], count);
      handle.WriteArray(&buf[0], count);
      break;
    }
    }
  }
}

void HostFileSystem::DoStateWriteOrMeasure(PointerWrap& p, std::string start_directory_path)
{
  std::string path = BuildFilename(start_directory_path).host_path;
  File::FSTEntry parent_entry = File::ScanDirectoryTree(path, true);
  std::deque<File::FSTEntry> todo;
  todo.insert(todo.end(), parent_entry.children.begin(), parent_entry.children.end());

  while (!todo.empty())
  {
    File::FSTEntry& entry = todo.front();
    std::string name = entry.physicalName;
    name.erase(0, path.length() + 1);
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
      char buf[BUFFER_CHUNK_SIZE];
      u32 count = size;
      while (count > BUFFER_CHUNK_SIZE)
      {
        handle.ReadArray(&buf[0], BUFFER_CHUNK_SIZE);
        p.DoArray(buf);
        count -= BUFFER_CHUNK_SIZE;
      }
      handle.ReadArray(&buf[0], count);
      p.DoArray(&buf[0], count);
    }
    todo.pop_front();
  }

  char type = 0;
  p.Do(type);
}

void HostFileSystem::DoState(PointerWrap& p)
{
  // Temporarily close the file, to prevent any issues with the savestating of files/folders.
  for (Handle& handle : m_handles)
    handle.host_file.reset();

  // The format for the next part of the save state is follows:
  // 1. bool Movie::WasMovieActiveWhenStateSaved() &&
  // WiiRoot::WasWiiRootTemporaryDirectoryWhenStateSaved()
  // 2. Contents of the "/tmp" directory recursively.
  // 3. u32 size_of_nand_folder_saved_below (or 0, if the root
  // of the NAND folder is not savestated below).
  // 4. Contents of the "/" directory recursively (or nothing, if the
  // root of the NAND folder is not save stated).

  // The "/" directory is only saved when a savestate is made during a movie recording
  // and when the directory root is temporary (i.e. WiiSession).
  // If a save state is made during a movie recording and is loaded when no movie is active,
  // then a call to p.DoExternal() will be used to skip over reading the contents of the "/"
  // directory (it skips over the number of bytes specified by size_of_nand_folder_saved)

  auto& movie = Core::System::GetInstance().GetMovie();
  bool original_save_state_made_during_movie_recording =
      movie.IsMovieActive() && Core::WiiRootIsTemporary();
  p.Do(original_save_state_made_during_movie_recording);

  u32 temp_val = 0;

  if (!p.IsReadMode())
  {
    DoStateWriteOrMeasure(p, "/tmp");
    u8* previous_position = p.ReserveU32();
    if (original_save_state_made_during_movie_recording)
    {
      DoStateWriteOrMeasure(p, "/");
      if (p.IsWriteMode())
      {
        u32 size_of_nand = p.GetOffsetFromPreviousPosition(previous_position) - sizeof(u32);
        memcpy(previous_position, &size_of_nand, sizeof(u32));
      }
    }
  }
  else  // case where we're in read mode.
  {
    DoStateRead(p, "/tmp");
    if (!movie.IsMovieActive() || !original_save_state_made_during_movie_recording ||
        !Core::WiiRootIsTemporary() ||
        (original_save_state_made_during_movie_recording !=
         (movie.IsMovieActive() && Core::WiiRootIsTemporary())))
    {
      (void)p.DoExternal(temp_val);
    }
    else
    {
      p.Do(temp_val);
      if (movie.IsMovieActive() && Core::WiiRootIsTemporary())
        DoStateRead(p, "/");
    }
  }

  for (Handle& handle : m_handles)
  {
    p.Do(handle.opened);
    p.Do(handle.mode);
    p.Do(handle.wii_path);
    p.Do(handle.file_offset);
    if (handle.opened)
      handle.host_file = OpenHostFile(BuildFilename(handle.wii_path).host_path);
  }
}

ResultCode HostFileSystem::Format(Uid uid)
{
  if (uid != 0)
    return ResultCode::AccessDenied;
  if (m_root_path.empty())
    return ResultCode::AccessDenied;
  const std::string root = BuildFilename("/").host_path;
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
  if (!IsValidNonRootPath(path) || !std::ranges::all_of(path, Common::IsPrintableCharacter))
  {
    return ResultCode::Invalid;
  }

  if (!is_file && std::ranges::count(path, '/') > int(MaxPathDepth))
    return ResultCode::TooManyPathComponents;

  const auto split_path = SplitPathAndBasename(path);
  const std::string host_path = BuildFilename(path).host_path;

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
    ERROR_LOG_FMT(IOS_FS, "Failed to create file or directory: {}", host_path);
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
  return std::ranges::any_of(m_handles, [&path](const Handle& handle) {
    return handle.opened && handle.wii_path == path;
  });
}

bool HostFileSystem::IsDirectoryInUse(const std::string& path) const
{
  return std::ranges::any_of(m_handles, [&path](const Handle& handle) {
    return handle.opened && handle.wii_path.starts_with(path);
  });
}

ResultCode HostFileSystem::Delete(Uid uid, Gid gid, const std::string& path)
{
  if (!IsValidNonRootPath(path))
    return ResultCode::Invalid;

  const std::string host_path = BuildFilename(path).host_path;
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

  const auto it = std::ranges::find_if(parent->children, GetNamePredicate(split_path.file_name));
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

  const auto host_old_info = BuildFilename(old_path);
  const auto host_new_info = BuildFilename(new_path);
  const std::string& host_old_path = host_old_info.host_path;
  const std::string& host_new_path = host_new_info.host_path;

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
    if (host_old_info.is_redirect || host_new_info.is_redirect)
    {
      // If either path is a redirect, the source and target may be on a different partition or
      // device, so a simple rename may not work. Fall back to Copy & Delete and see if that works.
      if (!File::CopyRegularFile(host_old_path, host_new_path))
      {
        ERROR_LOG_FMT(IOS_FS, "Copying {} to {} in Rename fallback failed", host_old_path,
                      host_new_path);
        return ResultCode::NotFound;
      }
      if (!File::Delete(host_old_path))
      {
        ERROR_LOG_FMT(IOS_FS, "Deleting {} in Rename fallback failed", host_old_path);
        return ResultCode::Invalid;
      }
    }
    else
    {
      ERROR_LOG_FMT(IOS_FS, "Rename {} to {} - failed", host_old_path, host_new_path);
      return ResultCode::NotFound;
    }
  }

  FstEntry* new_entry = GetFstEntryForPath(new_path);
  new_entry->name = split_new_path.file_name;

  // Finally, remove the child from the old parent and move it to the new parent.
  const auto it =
      std::ranges::find_if(old_parent->children, GetNamePredicate(split_old_path.file_name));
  if (it != old_parent->children.end())
  {
    new_entry->data = it->data;
    new_entry->children = it->children;

    old_parent->children.erase(it);
  }

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

  const std::string host_path = BuildFilename(path).host_path;
  File::FSTEntry host_entry = File::ScanDirectoryTree(host_path, false);
  FixupDirectoryEntries(&host_entry, path == "/");

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
  std::ranges::sort(host_entry.children,
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
  metadata.size = File::GetSize(BuildFilename(path).host_path);
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

  const bool is_empty = File::GetSize(BuildFilename(path).host_path) == 0;
  if (entry->data.uid != uid && entry->data.is_file && !is_empty)
    return ResultCode::FileNotEmpty;

  if (entry->data.gid != gid || entry->data.uid != uid || entry->data.attribute != attr ||
      entry->data.modes != modes)
  {
    entry->data.gid = gid;
    entry->data.uid = uid;
    entry->data.attribute = attr;
    entry->data.modes = modes;
    SaveFst();
  }

  return ResultCode::Success;
}

static u64 ComputeUsedClusters(const File::FSTEntry& parent_entry)
{
  u64 clusters = 0;
  for (const File::FSTEntry& entry : parent_entry.children)
  {
    if (entry.isDirectory)
      clusters += ComputeUsedClusters(entry);
    else
      clusters += Common::AlignUp(entry.size, CLUSTER_SIZE) / CLUSTER_SIZE;
  }
  return clusters;
}

Result<NandStats> HostFileSystem::GetNandStats()
{
  const auto root_stats = GetDirectoryStats("/");
  if (!root_stats)
    return root_stats.Error();  // TODO: is this right? can this fail on hardware?

  NandStats stats{};
  stats.cluster_size = CLUSTER_SIZE;
  stats.free_clusters = USABLE_CLUSTERS - root_stats->used_clusters;
  stats.used_clusters = root_stats->used_clusters;
  stats.bad_clusters = 0;
  stats.reserved_clusters = RESERVED_CLUSTERS;
  stats.free_inodes = TOTAL_INODES - root_stats->used_inodes;
  stats.used_inodes = root_stats->used_inodes;

  return stats;
}

Result<DirectoryStats> HostFileSystem::GetDirectoryStats(const std::string& wii_path)
{
  const auto result = GetExtendedDirectoryStats(wii_path);
  if (!result)
    return result.Error();

  DirectoryStats stats{};
  stats.used_inodes = static_cast<u32>(std::min<u64>(result->used_inodes, TOTAL_INODES));
  stats.used_clusters = static_cast<u32>(std::min<u64>(result->used_clusters, USABLE_CLUSTERS));
  return stats;
}

Result<ExtendedDirectoryStats>
HostFileSystem::GetExtendedDirectoryStats(const std::string& wii_path)
{
  if (!IsValidPath(wii_path))
    return ResultCode::Invalid;

  ExtendedDirectoryStats stats{};
  std::string path(BuildFilename(wii_path).host_path);
  File::FileInfo info(path);
  if (!info.Exists())
  {
    return ResultCode::NotFound;
  }
  if (info.IsDirectory())
  {
    File::FSTEntry parent_dir = File::ScanDirectoryTree(path, true);
    FixupDirectoryEntries(&parent_dir, wii_path == "/");

    // add one for the folder itself
    stats.used_inodes = 1 + parent_dir.size;
    stats.used_clusters = ComputeUsedClusters(parent_dir);
  }
  else
  {
    return ResultCode::Invalid;
  }
  return stats;
}

void HostFileSystem::SetNandRedirects(std::vector<NandRedirect> nand_redirects)
{
  m_nand_redirects = std::move(nand_redirects);
}
}  // namespace IOS::HLE::FS
