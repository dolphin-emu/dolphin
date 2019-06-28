// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/FS/FileSystemProxy.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/FS/FileSystem.h"

namespace IOS::HLE::Device
{
using namespace IOS::HLE::FS;

static IPCCommandResult GetFSReply(s32 return_value, u64 extra_tb_ticks = 0)
{
  // According to hardware tests, FS takes at least 2700 TB ticks to reply to commands.
  return {return_value, true, (2700 + extra_tb_ticks) * SystemTimers::TIMER_RATIO};
}

/// Amount of TB ticks required for a superblock write to complete.
constexpr u64 SUPERBLOCK_WRITE_TICKS = 3370000;
/// Amount of TB ticks required to write a cluster (for a file).
constexpr u64 CLUSTER_WRITE_TICKS = 300000;
/// Amount of TB ticks required to read a cluster (for a file).
constexpr u64 CLUSTER_READ_TICKS = 115000;
constexpr size_t CLUSTER_DATA_SIZE = 0x4000;

FS::FS(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

void FS::DoState(PointerWrap& p)
{
  p.Do(m_fd_map);
  p.Do(m_cache_fd);
  p.Do(m_cache_chain_index);
  p.Do(m_dirty_cache);
}

template <typename... Args>
static void LogResult(ResultCode code, std::string_view format, Args&&... args)
{
  const std::string command = fmt::format(format, std::forward<Args>(args)...);
  GENERIC_LOG(LogTypes::IOS_FS, (code == ResultCode::Success ? LogTypes::LINFO : LogTypes::LERROR),
              "%s: result %d", command.c_str(), ConvertResult(code));
}

template <typename T, typename... Args>
static void LogResult(const Result<T>& result, std::string_view format, Args&&... args)
{
  const auto result_code = result.Succeeded() ? ResultCode::Success : result.Error();
  LogResult(result_code, format, std::forward<Args>(args)...);
}

enum class FileLookupMode
{
  Normal,
  /// Timing model to use when FS splits the path into a parent and a file name
  /// and looks up each of them individually.
  Split,
};
// Note: lookups normally stop at the first non existing path (as the FST cannot be traversed
// further when a directory doesn't exist). However for the sake of simplicity we assume that the
// entire lookup succeeds because otherwise we'd need to check whether every path component exists.
static u64 EstimateFileLookupTicks(const std::string& path, FileLookupMode mode)
{
  const size_t number_of_path_components = std::count(path.cbegin(), path.cend(), '/');
  if (number_of_path_components == 0)
    return 0;
  // Paths that end with a slash are invalid and rejected early in FS.
  if (!path.empty() && *path.rbegin() == '/')
    return 300;
  if (mode == FileLookupMode::Normal)
    return 680 * number_of_path_components;
  return 1000 + 340 * number_of_path_components;
}

/// Get a reply with the correct timing for operations that modify the superblock.
///
/// A superblock flush takes a very large amount of time, so other delays are ignored
/// to simplify the implementation as they are insignificant.
static IPCCommandResult GetReplyForSuperblockOperation(ResultCode result)
{
  const u64 ticks = result == ResultCode::Success ? SUPERBLOCK_WRITE_TICKS : 0;
  return GetFSReply(ConvertResult(result), ticks);
}

IPCCommandResult FS::Open(const OpenRequest& request)
{
  if (m_fd_map.size() >= 16)
    return GetFSReply(ConvertResult(ResultCode::NoFreeHandle));

  if (request.path.size() >= 64)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  if (request.path == "/dev/fs")
  {
    m_fd_map[request.fd] = {request.gid, request.uid, INVALID_FD};
    return GetFSReply(IPC_SUCCESS);
  }

  const u64 ticks = EstimateFileLookupTicks(request.path, FileLookupMode::Normal);

  auto backend_fd = m_ios.GetFS()->OpenFile(request.uid, request.gid, request.path,
                                            static_cast<Mode>(request.flags & 3));
  LogResult(backend_fd, "OpenFile({})", request.path);
  if (!backend_fd)
    return GetFSReply(ConvertResult(backend_fd.Error()), ticks);

  m_fd_map[request.fd] = {request.gid, request.uid, backend_fd->Release()};
  std::strncpy(m_fd_map[request.fd].name.data(), request.path.c_str(), 64);
  return GetFSReply(IPC_SUCCESS, ticks);
}

IPCCommandResult FS::Close(u32 fd)
{
  u64 ticks = 0;
  if (m_fd_map[fd].fs_fd != INVALID_FD)
  {
    if (fd == m_cache_fd)
    {
      ticks += SimulateFlushFileCache();
      m_cache_fd = INVALID_FD;
    }

    if (m_fd_map[fd].superblock_flush_needed)
      ticks += SUPERBLOCK_WRITE_TICKS;

    const ResultCode result = m_ios.GetFS()->Close(m_fd_map[fd].fs_fd);
    LogResult(result, "Close({})", m_fd_map[fd].name.data());
    m_fd_map.erase(fd);
    if (result != ResultCode::Success)
      return GetFSReply(ConvertResult(result));
  }
  else
  {
    m_fd_map.erase(fd);
  }
  return GetFSReply(IPC_SUCCESS, ticks);
}

u64 FS::SimulatePopulateFileCache(u32 fd, u32 offset, u32 file_size)
{
  if (HasCacheForFile(fd, offset))
    return 0;

  u64 ticks = SimulateFlushFileCache();
  if ((offset % CLUSTER_DATA_SIZE != 0 || offset != file_size) && offset < file_size)
    ticks += CLUSTER_READ_TICKS;

  m_cache_fd = fd;
  m_cache_chain_index = static_cast<u16>(offset / CLUSTER_DATA_SIZE);
  return ticks;
}

u64 FS::SimulateFlushFileCache()
{
  if (m_cache_fd == INVALID_FD || !m_dirty_cache)
    return 0;
  m_dirty_cache = false;
  m_fd_map[m_cache_fd].superblock_flush_needed = true;
  return CLUSTER_WRITE_TICKS;
}

// Simulate parts of the FS read/write logic to estimate ticks for file operations correctly.
u64 FS::EstimateTicksForReadWrite(const Handle& handle, const ReadWriteRequest& request)
{
  u64 ticks = 0;

  const bool is_write = request.command == IPC_CMD_WRITE;
  const Result<FileStatus> status = m_ios.GetFS()->GetFileStatus(handle.fs_fd);
  u32 offset = status->offset;
  u32 count = request.size;
  if (!is_write && count + offset > status->size)
    count = status->size - offset;

  while (count != 0)
  {
    u32 copy_length;
    // Fast path (if not cached): FS copies an entire cluster directly from/to the request.
    if (!HasCacheForFile(request.fd, offset) && count >= CLUSTER_DATA_SIZE &&
        offset % CLUSTER_DATA_SIZE == 0)
    {
      ticks += is_write ? CLUSTER_WRITE_TICKS : CLUSTER_READ_TICKS;
      copy_length = CLUSTER_DATA_SIZE;
      if (is_write)
        m_fd_map[request.fd].superblock_flush_needed = true;
    }
    else
    {
      ticks += SimulatePopulateFileCache(request.fd, offset, status->size);

      const u32 start = offset - m_cache_chain_index * CLUSTER_DATA_SIZE;
      copy_length = std::min<u32>(CLUSTER_DATA_SIZE - start, count);
      // FS essentially just does a memcpy from/to an internal buffer.
      ticks += 0.63f * copy_length + (request.command == IPC_CMD_WRITE ? 1000 : 0);
      m_dirty_cache = is_write;

      if (is_write && (offset + copy_length) % CLUSTER_DATA_SIZE == 0)
        ticks += SimulateFlushFileCache();
    }
    offset += copy_length;
    count -= copy_length;
  }
  return ticks;
}

bool FS::HasCacheForFile(u32 fd, u32 offset) const
{
  const u16 chain_index = static_cast<u16>(offset / CLUSTER_DATA_SIZE);
  return m_cache_fd == fd && m_cache_chain_index == chain_index;
}

IPCCommandResult FS::Read(const ReadWriteRequest& request)
{
  const Handle& handle = m_fd_map[request.fd];
  if (handle.fs_fd == INVALID_FD)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  // Simulate the FS read logic to estimate ticks. Note: this must be done before reading.
  const u64 ticks = EstimateTicksForReadWrite(handle, request);

  const Result<u32> result = m_ios.GetFS()->ReadBytesFromFile(
      handle.fs_fd, Memory::GetPointer(request.buffer), request.size);
  LogResult(result, "Read({}, 0x{:08x}, {})", handle.name.data(), request.buffer, request.size);
  if (!result)
    return GetFSReply(ConvertResult(result.Error()));

  return GetFSReply(*result, ticks);
}

IPCCommandResult FS::Write(const ReadWriteRequest& request)
{
  const Handle& handle = m_fd_map[request.fd];
  if (handle.fs_fd == INVALID_FD)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  // Simulate the FS write logic to estimate ticks. Must be done before writing.
  const u64 ticks = EstimateTicksForReadWrite(handle, request);

  const Result<u32> result = m_ios.GetFS()->WriteBytesToFile(
      handle.fs_fd, Memory::GetPointer(request.buffer), request.size);
  LogResult(result, "Write({}, 0x{:08x}, {})", handle.name.data(), request.buffer, request.size);
  if (!result)
    return GetFSReply(ConvertResult(result.Error()));

  return GetFSReply(*result, ticks);
}

IPCCommandResult FS::Seek(const SeekRequest& request)
{
  const Handle& handle = m_fd_map[request.fd];
  if (handle.fs_fd == INVALID_FD)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const Result<u32> result =
      m_ios.GetFS()->SeekFile(handle.fs_fd, request.offset, IOS::HLE::FS::SeekMode(request.mode));
  LogResult(result, "Seek({}, 0x{:08x}, {})", handle.name.data(), request.offset, request.mode);
  if (!result)
    return GetFSReply(ConvertResult(result.Error()));
  return GetFSReply(*result);
}

#pragma pack(push, 1)
struct ISFSParams
{
  Common::BigEndianValue<Uid> uid;
  Common::BigEndianValue<Gid> gid;
  char path[64];
  Modes modes;
  FileAttribute attribute;
};

struct ISFSNandStats
{
  Common::BigEndianValue<u32> cluster_size;
  Common::BigEndianValue<u32> free_clusters;
  Common::BigEndianValue<u32> used_clusters;
  Common::BigEndianValue<u32> bad_clusters;
  Common::BigEndianValue<u32> reserved_clusters;
  Common::BigEndianValue<u32> free_inodes;
  Common::BigEndianValue<u32> used_inodes;
};

struct ISFSFileStats
{
  Common::BigEndianValue<u32> size;
  Common::BigEndianValue<u32> seek_position;
};
#pragma pack(pop)

template <typename T>
static Result<T> GetParams(const IOCtlRequest& request)
{
  if (request.buffer_in_size < sizeof(T))
    return ResultCode::Invalid;

  T params;
  Memory::CopyFromEmu(&params, request.buffer_in, sizeof(params));
  return params;
}

IPCCommandResult FS::IOCtl(const IOCtlRequest& request)
{
  const auto it = m_fd_map.find(request.fd);
  if (it == m_fd_map.end())
    return GetDefaultReply(ConvertResult(ResultCode::Invalid));

  switch (request.request)
  {
  case ISFS_IOCTL_FORMAT:
    return Format(it->second, request);
  case ISFS_IOCTL_GETSTATS:
    return GetStats(it->second, request);
  case ISFS_IOCTL_CREATEDIR:
    return CreateDirectory(it->second, request);
  case ISFS_IOCTL_SETATTR:
    return SetAttribute(it->second, request);
  case ISFS_IOCTL_GETATTR:
    return GetAttribute(it->second, request);
  case ISFS_IOCTL_DELETE:
    return DeleteFile(it->second, request);
  case ISFS_IOCTL_RENAME:
    return RenameFile(it->second, request);
  case ISFS_IOCTL_CREATEFILE:
    return CreateFile(it->second, request);
  case ISFS_IOCTL_SETFILEVERCTRL:
    return SetFileVersionControl(it->second, request);
  case ISFS_IOCTL_GETFILESTATS:
    return GetFileStats(it->second, request);
  case ISFS_IOCTL_SHUTDOWN:
    return Shutdown(it->second, request);
  default:
    return GetFSReply(ConvertResult(ResultCode::Invalid));
  }
}

IPCCommandResult FS::IOCtlV(const IOCtlVRequest& request)
{
  const auto it = m_fd_map.find(request.fd);
  if (it == m_fd_map.end())
    return GetDefaultReply(ConvertResult(ResultCode::Invalid));

  switch (request.request)
  {
  case ISFS_IOCTLV_READDIR:
    return ReadDirectory(it->second, request);
  case ISFS_IOCTLV_GETUSAGE:
    return GetUsage(it->second, request);
  default:
    return GetFSReply(ConvertResult(ResultCode::Invalid));
  }
}

IPCCommandResult FS::Format(const Handle& handle, const IOCtlRequest& request)
{
  if (handle.uid != 0)
    return GetFSReply(ConvertResult(ResultCode::AccessDenied));

  const ResultCode result = m_ios.GetFS()->Format(handle.uid);
  return GetReplyForSuperblockOperation(result);
}

IPCCommandResult FS::GetStats(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_out_size < sizeof(ISFSNandStats))
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const Result<NandStats> stats = m_ios.GetFS()->GetNandStats();
  LogResult(stats, "GetNandStats");
  if (!stats)
    return GetDefaultReply(ConvertResult(stats.Error()));

  ISFSNandStats out;
  out.cluster_size = stats->cluster_size;
  out.free_clusters = stats->free_clusters;
  out.used_clusters = stats->used_clusters;
  out.bad_clusters = stats->bad_clusters;
  out.reserved_clusters = stats->reserved_clusters;
  out.free_inodes = stats->free_inodes;
  out.used_inodes = stats->used_inodes;
  Memory::CopyToEmu(request.buffer_out, &out, sizeof(out));
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult FS::CreateDirectory(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result = m_ios.GetFS()->CreateDirectory(handle.uid, handle.gid, params->path,
                                                           params->attribute, params->modes);
  LogResult(result, "CreateDirectory({})", params->path);
  return GetReplyForSuperblockOperation(result);
}

IPCCommandResult FS::ReadDirectory(const Handle& handle, const IOCtlVRequest& request)
{
  if (request.in_vectors.empty() || request.in_vectors.size() != request.io_vectors.size() ||
      request.in_vectors.size() > 2 || request.in_vectors[0].size != 64)
  {
    return GetFSReply(ConvertResult(ResultCode::Invalid));
  }

  u32 file_list_address, file_count_address, max_count;
  if (request.in_vectors.size() == 2)
  {
    if (request.in_vectors[1].size != 4 || request.io_vectors[1].size != 4)
      return GetFSReply(ConvertResult(ResultCode::Invalid));
    max_count = Memory::Read_U32(request.in_vectors[1].address);
    file_count_address = request.io_vectors[1].address;
    file_list_address = request.io_vectors[0].address;
    if (request.io_vectors[0].size != 13 * max_count)
      return GetFSReply(ConvertResult(ResultCode::Invalid));
    Memory::Write_U32(max_count, file_count_address);
  }
  else
  {
    if (request.io_vectors[0].size != 4)
      return GetFSReply(ConvertResult(ResultCode::Invalid));
    max_count = Memory::Read_U32(request.io_vectors[0].address);
    file_count_address = request.io_vectors[0].address;
    file_list_address = 0;
  }

  const std::string directory = Memory::GetString(request.in_vectors[0].address, 64);
  const Result<std::vector<std::string>> list =
      m_ios.GetFS()->ReadDirectory(handle.uid, handle.gid, directory);
  LogResult(list, "ReadDirectory({})", directory);
  if (!list)
    return GetFSReply(ConvertResult(list.Error()));

  if (!file_list_address)
  {
    Memory::Write_U32(static_cast<u32>(list->size()), file_count_address);
    return GetFSReply(IPC_SUCCESS);
  }

  for (size_t i = 0; i < list->size() && i < max_count; ++i)
  {
    Memory::Memset(file_list_address, 0, 13);
    Memory::CopyToEmu(file_list_address, (*list)[i].data(), (*list)[i].size());
    Memory::Write_U8(0, file_list_address + 12);
    file_list_address += static_cast<u32>((*list)[i].size()) + 1;
  }
  // Write the actual number of entries in the buffer.
  Memory::Write_U32(std::min(max_count, static_cast<u32>(list->size())), file_count_address);
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::SetAttribute(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result = m_ios.GetFS()->SetMetadata(
      handle.uid, params->path, params->uid, params->gid, params->attribute, params->modes);
  LogResult(result, "SetMetadata({})", params->path);
  return GetReplyForSuperblockOperation(result);
}

IPCCommandResult FS::GetAttribute(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64 || request.buffer_out_size < sizeof(ISFSParams))
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const std::string path = Memory::GetString(request.buffer_in, 64);
  const u64 ticks = EstimateFileLookupTicks(path, FileLookupMode::Split);
  const Result<Metadata> metadata = m_ios.GetFS()->GetMetadata(handle.uid, handle.gid, path);
  LogResult(metadata, "GetMetadata({})", path);
  if (!metadata)
    return GetFSReply(ConvertResult(metadata.Error()), ticks);

  // Yes, the other members aren't copied at all. Actually, IOS does not even memset
  // the struct at all, which means uninitialised bytes from the stack are returned.
  // For the sake of determinism, let's just zero initialise the struct.
  ISFSParams out{};
  out.uid = metadata->uid;
  out.gid = metadata->gid;
  out.attribute = metadata->attribute;
  out.modes = metadata->modes;
  Memory::CopyToEmu(request.buffer_out, &out, sizeof(out));
  return GetFSReply(IPC_SUCCESS, ticks);
}

IPCCommandResult FS::DeleteFile(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const std::string path = Memory::GetString(request.buffer_in, 64);
  const ResultCode result = m_ios.GetFS()->Delete(handle.uid, handle.gid, path);
  LogResult(result, "Delete({})", path);
  return GetReplyForSuperblockOperation(result);
}

IPCCommandResult FS::RenameFile(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64 * 2)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const std::string old_path = Memory::GetString(request.buffer_in, 64);
  const std::string new_path = Memory::GetString(request.buffer_in + 64, 64);
  const ResultCode result = m_ios.GetFS()->Rename(handle.uid, handle.gid, old_path, new_path);
  LogResult(result, "Rename({}, {})", old_path, new_path);
  return GetReplyForSuperblockOperation(result);
}

IPCCommandResult FS::CreateFile(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result = m_ios.GetFS()->CreateFile(handle.uid, handle.gid, params->path,
                                                      params->attribute, params->modes);
  LogResult(result, "CreateFile({})", params->path);
  return GetReplyForSuperblockOperation(result);
}

IPCCommandResult FS::SetFileVersionControl(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  // FS_SetFileVersionControl(ctx->uid, params->path, params->attribute)
  ERROR_LOG(IOS_FS, "SetFileVersionControl(%s, 0x%x): Stubbed", params->path, params->attribute);
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::GetFileStats(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_out_size < 8 || handle.fs_fd == INVALID_FD)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const Result<FileStatus> status = m_ios.GetFS()->GetFileStatus(handle.fs_fd);
  LogResult(status, "GetFileStatus({})", handle.name.data());
  if (!status)
    return GetDefaultReply(ConvertResult(status.Error()));

  ISFSFileStats out;
  out.size = status->size;
  out.seek_position = status->offset;
  Memory::CopyToEmu(request.buffer_out, &out, sizeof(out));
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult FS::GetUsage(const Handle& handle, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 2) || request.in_vectors[0].size != 64 ||
      request.io_vectors[0].size != 4 || request.io_vectors[1].size != 4)
  {
    return GetFSReply(ConvertResult(ResultCode::Invalid));
  }

  const std::string directory = Memory::GetString(request.in_vectors[0].address, 64);
  const Result<DirectoryStats> stats = m_ios.GetFS()->GetDirectoryStats(directory);
  LogResult(stats, "GetDirectoryStats({})", directory);
  if (!stats)
    return GetFSReply(ConvertResult(stats.Error()));

  Memory::Write_U32(stats->used_clusters, request.io_vectors[0].address);
  Memory::Write_U32(stats->used_inodes, request.io_vectors[1].address);
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::Shutdown(const Handle& handle, const IOCtlRequest& request)
{
  INFO_LOG(IOS_FS, "Shutdown");
  return GetFSReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE::Device
