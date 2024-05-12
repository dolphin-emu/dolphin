// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/FS/FileSystemProxy.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/EnumUtils.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"
#include "Core/System.h"

namespace IOS::HLE
{
using namespace IOS::HLE::FS;

static IPCReply GetFSReply(s32 return_value, SystemTimers::TimeBaseTick extra_tb_ticks = {})
{
  return IPCReply{return_value, IPC_OVERHEAD_TICKS + extra_tb_ticks};
}

/// Duration of a superblock write (in timebase ticks).
constexpr SystemTimers::TimeBaseTick GetSuperblockWriteTbTicks(int ios_version)
{
  if (ios_version == 28 || ios_version == 80)
    return 3350000_tbticks;

  if (ios_version < 28)
    return 4100000_tbticks;

  return 3170000_tbticks;
}

/// Duration of a file cluster write (in timebase ticks).
constexpr SystemTimers::TimeBaseTick GetClusterWriteTbTicks(int ios_version)
{
  // Based on the time it takes to write two clusters (0x8000 bytes), divided by two.
  if (ios_version < 28)
    return 370000_tbticks;

  return 300000_tbticks;
}

/// Duration of a file cluster read (in timebase ticks).
constexpr SystemTimers::TimeBaseTick GetClusterReadTbTicks(int ios_version)
{
  if (ios_version == 28 || ios_version == 80)
    return 125000_tbticks;

  if (ios_version < 28)
    return 165000_tbticks;

  return 115000_tbticks;
}

constexpr SystemTimers::TimeBaseTick GetFSModuleMemcpyTbTicks(int ios_version, u64 copy_size)
{
  // FS handles cached read/writes by doing a simple memcpy on an internal buffer.
  // The following equations were obtained by doing cached reads with various read sizes
  // on real hardware and performing a linear regression. R^2 was close to 1 (0.9999),
  // which is not entirely surprising.

  // Older versions of IOS have a simplistic implementation of memcpy that does not optimize
  // large copies by copying 16 bytes or 32 bytes per chunk.
  if (ios_version < 28)
    return SystemTimers::TimeBaseTick(1.0 * copy_size + 3.0);

  return SystemTimers::TimeBaseTick(0.636 * copy_size + 150.0);
}

constexpr SystemTimers::TimeBaseTick GetFreeClusterCheckTbTicks()
{
  return 1000_tbticks;
}

constexpr size_t CLUSTER_DATA_SIZE = 0x4000;

FSCore::FSCore(Kernel& ios) : m_ios(ios)
{
  if (ios.GetFS()->Delete(PID_KERNEL, PID_KERNEL, "/tmp") == ResultCode::Success)
  {
    ios.GetFS()->CreateDirectory(PID_KERNEL, PID_KERNEL, "/tmp", 0,
                                 {Mode::ReadWrite, Mode::ReadWrite, Mode::ReadWrite});
  }
}

FSCore::~FSCore() = default;

FSDevice::FSDevice(EmulationKernel& ios, FSCore& core, const std::string& device_name)
    : EmulationDevice(ios, device_name), m_core(core)
{
}

FSDevice::~FSDevice() = default;

void FSDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(m_core.m_dirty_cache);
  p.Do(m_core.m_cache_chain_index);
  p.Do(m_core.m_cache_fd);
  p.Do(m_core.m_next_fd);
  p.Do(m_core.m_fd_map);
}

template <typename... Args>
static void LogResult(ResultCode code, fmt::format_string<Args...> format, Args&&... args)
{
  const std::string command = fmt::format(format, std::forward<Args>(args)...);
  const auto type =
      code == ResultCode::Success ? Common::Log::LogLevel::LINFO : Common::Log::LogLevel::LERROR;

  GENERIC_LOG_FMT(Common::Log::LogType::IOS_FS, type, "Command: {}: Result {}", command,
                  Common::ToUnderlying(ConvertResult(code)));
}

template <typename T, typename... Args>
static void LogResult(const Result<T>& result, fmt::format_string<Args...> format, Args&&... args)
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
static SystemTimers::TimeBaseTick EstimateFileLookupTicks(const std::string& path,
                                                          FileLookupMode mode)
{
  const size_t number_of_path_components = std::count(path.cbegin(), path.cend(), '/');
  if (number_of_path_components == 0)
    return 0_tbticks;

  // Paths that end with a slash are invalid and rejected early in FS.
  if (!path.empty() && *path.rbegin() == '/')
    return 300_tbticks;

  if (mode == FileLookupMode::Normal)
    return SystemTimers::TimeBaseTick(680 * number_of_path_components);

  return SystemTimers::TimeBaseTick(1000 + 340 * number_of_path_components);
}

/// Get a reply with the correct timing for operations that modify the superblock.
///
/// A superblock flush takes a very large amount of time, so other delays are ignored
/// to simplify the implementation as they are insignificant.
static IPCReply GetReplyForSuperblockOperation(int ios_version, ResultCode result)
{
  const auto ticks =
      result == ResultCode::Success ? GetSuperblockWriteTbTicks(ios_version) : 0_tbticks;
  return GetFSReply(ConvertResult(result), ticks);
}

std::optional<IPCReply> FSDevice::Open(const OpenRequest& request)
{
  return MakeIPCReply([&](Ticks t) {
    return m_core
        .Open(request.uid, request.gid, request.path, static_cast<Mode>(request.flags & 3),
              request.fd, t)
        .Release();
  });
}

FSCore::ScopedFd FSCore::Open(FS::Uid uid, FS::Gid gid, const std::string& path, FS::Mode mode,
                              std::optional<u32> ipc_fd, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  if (m_fd_map.size() >= 16)
    return {this, ConvertResult(ResultCode::NoFreeHandle), ticks};

  if (path.size() >= 64)
    return {this, ConvertResult(ResultCode::Invalid), ticks};

  const u64 fd = ipc_fd.has_value() ? u64(*ipc_fd) : m_next_fd++;

  if (path == "/dev/fs")
  {
    m_fd_map[fd] = {gid, uid, INVALID_FD};
    return {this, static_cast<s64>(fd), ticks};
  }

  ticks.Add(EstimateFileLookupTicks(path, FileLookupMode::Normal));

  auto backend_fd = m_ios.GetFS()->OpenFile(uid, gid, path, mode);
  LogResult(backend_fd, "OpenFile({})", path);
  if (!backend_fd)
    return {this, ConvertResult(backend_fd.Error()), ticks};

  auto& handle = m_fd_map[fd] = {gid, uid, backend_fd->Release()};
  std::strncpy(handle.name.data(), path.c_str(), handle.name.size());
  return {this, static_cast<s64>(fd), ticks};
}

std::optional<IPCReply> FSDevice::Close(u32 fd)
{
  return MakeIPCReply([&](Ticks t) { return m_core.Close(static_cast<u64>(fd), t); });
}

s32 FSCore::Close(u64 fd, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const auto& handle = m_fd_map[fd];
  if (handle.fs_fd != INVALID_FD)
  {
    if (fd == m_cache_fd)
    {
      ticks.Add(SimulateFlushFileCache());
      m_cache_fd.reset();
    }

    if (handle.superblock_flush_needed)
      ticks.Add(GetSuperblockWriteTbTicks(m_ios.GetVersion()));

    const ResultCode result = m_ios.GetFS()->Close(handle.fs_fd);
    LogResult(result, "Close({})", handle.name.data());
    m_fd_map.erase(fd);
    if (result != ResultCode::Success)
      return ConvertResult(result);
  }
  else
  {
    m_fd_map.erase(fd);
  }
  return IPC_SUCCESS;
}

u64 FSCore::SimulatePopulateFileCache(u64 fd, u32 offset, u32 file_size)
{
  if (HasCacheForFile(fd, offset))
    return 0;

  u64 ticks = SimulateFlushFileCache();
  if ((offset % CLUSTER_DATA_SIZE != 0 || offset != file_size) && offset < file_size)
    ticks += GetClusterReadTbTicks(m_ios.GetVersion());

  m_cache_fd = fd;
  m_cache_chain_index = static_cast<u16>(offset / CLUSTER_DATA_SIZE);
  return ticks;
}

u64 FSCore::SimulateFlushFileCache()
{
  if (!m_cache_fd.has_value() || !m_dirty_cache)
    return 0;
  m_dirty_cache = false;
  m_fd_map[*m_cache_fd].superblock_flush_needed = true;
  return GetClusterWriteTbTicks(m_ios.GetVersion());
}

// Simulate parts of the FS read/write logic to estimate ticks for file operations correctly.
u64 FSCore::EstimateTicksForReadWrite(const Handle& handle, u64 fd, IPCCommandType command,
                                      u32 size)
{
  u64 ticks = 0;

  const bool is_write = command == IPC_CMD_WRITE;
  const Result<FileStatus> status = m_ios.GetFS()->GetFileStatus(handle.fs_fd);
  u32 offset = status->offset;
  u32 count = size;
  if (!is_write && count + offset > status->size)
    count = status->size - offset;

  while (count != 0)
  {
    u32 copy_length;
    // Fast path (if not cached): FS copies an entire cluster directly from/to the request.
    if (!HasCacheForFile(fd, offset) && count >= CLUSTER_DATA_SIZE &&
        offset % CLUSTER_DATA_SIZE == 0)
    {
      ticks += (is_write ? GetClusterWriteTbTicks : GetClusterReadTbTicks)(m_ios.GetVersion());
      copy_length = CLUSTER_DATA_SIZE;
      if (is_write)
        m_fd_map[fd].superblock_flush_needed = true;
    }
    else
    {
      ticks += SimulatePopulateFileCache(fd, offset, status->size);

      const u32 start = offset - m_cache_chain_index * CLUSTER_DATA_SIZE;
      copy_length = std::min<u32>(CLUSTER_DATA_SIZE - start, count);

      // FS essentially just does a memcpy from/to an internal buffer.
      ticks += GetFSModuleMemcpyTbTicks(m_ios.GetVersion(), copy_length);

      if (is_write)
        ticks += GetFreeClusterCheckTbTicks();

      m_dirty_cache = is_write;

      if (is_write && (offset + copy_length) % CLUSTER_DATA_SIZE == 0)
        ticks += SimulateFlushFileCache();
    }
    offset += copy_length;
    count -= copy_length;
  }
  return ticks;
}

bool FSCore::HasCacheForFile(u64 fd, u32 offset) const
{
  const u16 chain_index = static_cast<u16>(offset / CLUSTER_DATA_SIZE);
  return m_cache_fd == fd && m_cache_chain_index == chain_index;
}

std::optional<IPCReply> FSDevice::Read(const ReadWriteRequest& request)
{
  return MakeIPCReply([&](Ticks t) {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    return m_core.Read(request.fd, memory.GetPointerForRange(request.buffer, request.size),
                       request.size, request.buffer, t);
  });
}

s32 FSCore::Read(u64 fd, u8* data, u32 size, std::optional<u32> ipc_buffer_addr, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const Handle& handle = m_fd_map[fd];
  if (handle.fs_fd == INVALID_FD)
    return ConvertResult(ResultCode::Invalid);

  // Simulate the FS read logic to estimate ticks. Note: this must be done before reading.
  ticks.Add(EstimateTicksForReadWrite(handle, fd, IPC_CMD_READ, size));

  const Result<u32> result = m_ios.GetFS()->ReadBytesFromFile(handle.fs_fd, data, size);
  if (ipc_buffer_addr)
    LogResult(result, "Read({}, 0x{:08x}, {})", handle.name.data(), *ipc_buffer_addr, size);

  if (!result)
    return ConvertResult(result.Error());

  return *result;
}

std::optional<IPCReply> FSDevice::Write(const ReadWriteRequest& request)
{
  return MakeIPCReply([&](Ticks t) {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    return m_core.Write(request.fd, memory.GetPointerForRange(request.buffer, request.size),
                        request.size, request.buffer, t);
  });
}

s32 FSCore::Write(u64 fd, const u8* data, u32 size, std::optional<u32> ipc_buffer_addr, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const Handle& handle = m_fd_map[fd];
  if (handle.fs_fd == INVALID_FD)
    return ConvertResult(ResultCode::Invalid);

  // Simulate the FS write logic to estimate ticks. Must be done before writing.
  ticks.Add(EstimateTicksForReadWrite(handle, fd, IPC_CMD_WRITE, size));

  const Result<u32> result = m_ios.GetFS()->WriteBytesToFile(handle.fs_fd, data, size);
  if (ipc_buffer_addr)
    LogResult(result, "Write({}, 0x{:08x}, {})", handle.name.data(), *ipc_buffer_addr, size);

  if (!result)
    return ConvertResult(result.Error());

  return *result;
}

std::optional<IPCReply> FSDevice::Seek(const SeekRequest& request)
{
  return MakeIPCReply([&](Ticks t) {
    return m_core.Seek(request.fd, request.offset, HLE::FS::SeekMode(request.mode), t);
  });
}

s32 FSCore::Seek(u64 fd, u32 offset, FS::SeekMode mode, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const Handle& handle = m_fd_map[fd];
  if (handle.fs_fd == INVALID_FD)
    return ConvertResult(ResultCode::Invalid);

  const Result<u32> result = m_ios.GetFS()->SeekFile(handle.fs_fd, offset, mode);
  LogResult(result, "Seek({}, 0x{:08x}, {})", handle.name.data(), offset, static_cast<int>(mode));
  if (!result)
    return ConvertResult(result.Error());
  return *result;
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
static Result<T> GetParams(Memory::MemoryManager& memory, const IOCtlRequest& request)
{
  if (request.buffer_in_size < sizeof(T))
    return ResultCode::Invalid;

  T params;
  memory.CopyFromEmu(&params, request.buffer_in, sizeof(params));
  return params;
}

std::optional<IPCReply> FSDevice::IOCtl(const IOCtlRequest& request)
{
  const auto it = m_core.m_fd_map.find(request.fd);
  if (it == m_core.m_fd_map.end())
    return IPCReply(ConvertResult(ResultCode::Invalid));

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

std::optional<IPCReply> FSDevice::IOCtlV(const IOCtlVRequest& request)
{
  const auto it = m_core.m_fd_map.find(request.fd);
  if (it == m_core.m_fd_map.end())
    return IPCReply(ConvertResult(ResultCode::Invalid));

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

IPCReply FSDevice::Format(const Handle& handle, const IOCtlRequest& request)
{
  if (handle.uid != 0)
    return GetFSReply(ConvertResult(ResultCode::AccessDenied));

  const ResultCode result = m_ios.GetFS()->Format(handle.uid);
  return GetReplyForSuperblockOperation(m_ios.GetVersion(), result);
}

IPCReply FSDevice::GetStats(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_out_size < sizeof(ISFSNandStats))
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const Result<NandStats> stats = m_ios.GetFS()->GetNandStats();
  LogResult(stats, "GetNandStats");
  if (!stats)
    return IPCReply(ConvertResult(stats.Error()));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  ISFSNandStats out;
  out.cluster_size = stats->cluster_size;
  out.free_clusters = stats->free_clusters;
  out.used_clusters = stats->used_clusters;
  out.bad_clusters = stats->bad_clusters;
  out.reserved_clusters = stats->reserved_clusters;
  out.free_inodes = stats->free_inodes;
  out.used_inodes = stats->used_inodes;
  memory.CopyToEmu(request.buffer_out, &out, sizeof(out));
  return IPCReply(IPC_SUCCESS);
}

IPCReply FSDevice::CreateDirectory(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(GetSystem().GetMemory(), request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result = m_ios.GetFS()->CreateDirectory(handle.uid, handle.gid, params->path,
                                                           params->attribute, params->modes);
  LogResult(result, "CreateDirectory({})", params->path);
  return GetReplyForSuperblockOperation(m_ios.GetVersion(), result);
}

IPCReply FSDevice::ReadDirectory(const Handle& handle, const IOCtlVRequest& request)
{
  if (request.in_vectors.empty() || request.in_vectors.size() != request.io_vectors.size() ||
      request.in_vectors.size() > 2 || request.in_vectors[0].size != 64)
  {
    return GetFSReply(ConvertResult(ResultCode::Invalid));
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  u32 file_list_address, file_count_address, max_count;
  if (request.in_vectors.size() == 2)
  {
    if (request.in_vectors[1].size != 4 || request.io_vectors[1].size != 4)
      return GetFSReply(ConvertResult(ResultCode::Invalid));
    max_count = memory.Read_U32(request.in_vectors[1].address);
    file_count_address = request.io_vectors[1].address;
    file_list_address = request.io_vectors[0].address;
    if (request.io_vectors[0].size != 13 * max_count)
      return GetFSReply(ConvertResult(ResultCode::Invalid));
    memory.Write_U32(max_count, file_count_address);
  }
  else
  {
    if (request.io_vectors[0].size != 4)
      return GetFSReply(ConvertResult(ResultCode::Invalid));
    max_count = memory.Read_U32(request.io_vectors[0].address);
    file_count_address = request.io_vectors[0].address;
    file_list_address = 0;
  }

  const std::string directory = memory.GetString(request.in_vectors[0].address, 64);
  const Result<std::vector<std::string>> list =
      m_ios.GetFS()->ReadDirectory(handle.uid, handle.gid, directory);
  LogResult(list, "ReadDirectory({})", directory);
  if (!list)
    return GetFSReply(ConvertResult(list.Error()));

  if (!file_list_address)
  {
    memory.Write_U32(static_cast<u32>(list->size()), file_count_address);
    return GetFSReply(IPC_SUCCESS);
  }

  for (size_t i = 0; i < list->size() && i < max_count; ++i)
  {
    memory.Memset(file_list_address, 0, 13);
    memory.CopyToEmu(file_list_address, (*list)[i].data(), (*list)[i].size());
    memory.Write_U8(0, file_list_address + 12);
    file_list_address += static_cast<u32>((*list)[i].size()) + 1;
  }
  // Write the actual number of entries in the buffer.
  memory.Write_U32(std::min(max_count, static_cast<u32>(list->size())), file_count_address);
  return GetFSReply(IPC_SUCCESS);
}

IPCReply FSDevice::SetAttribute(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(GetSystem().GetMemory(), request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result = m_ios.GetFS()->SetMetadata(
      handle.uid, params->path, params->uid, params->gid, params->attribute, params->modes);
  LogResult(result, "SetMetadata({})", params->path);
  return GetReplyForSuperblockOperation(m_ios.GetVersion(), result);
}

IPCReply FSDevice::GetAttribute(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64 || request.buffer_out_size < sizeof(ISFSParams))
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const std::string path = memory.GetString(request.buffer_in, 64);
  const auto ticks = EstimateFileLookupTicks(path, FileLookupMode::Split);
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
  memory.CopyToEmu(request.buffer_out, &out, sizeof(out));
  return GetFSReply(IPC_SUCCESS, ticks);
}

FS::ResultCode FSCore::DeleteFile(FS::Uid uid, FS::Gid gid, const std::string& path, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const ResultCode result = m_ios.GetFS()->Delete(uid, gid, path);
  ticks.Add(GetSuperblockWriteTbTicks(m_ios.GetVersion()));
  LogResult(result, "Delete({})", path);
  return result;
}

IPCReply FSDevice::DeleteFile(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const std::string path = memory.GetString(request.buffer_in, 64);
  return MakeIPCReply([&](Ticks ticks) {
    return ConvertResult(m_core.DeleteFile(handle.uid, handle.gid, path, ticks));
  });
}

FS::ResultCode FSCore::RenameFile(FS::Uid uid, FS::Gid gid, const std::string& old_path,
                                  const std::string& new_path, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const ResultCode result = m_ios.GetFS()->Rename(uid, gid, old_path, new_path);
  ticks.Add(GetSuperblockWriteTbTicks(m_ios.GetVersion()));
  LogResult(result, "Rename({}, {})", old_path, new_path);
  return result;
}

IPCReply FSDevice::RenameFile(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64 * 2)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const std::string old_path = memory.GetString(request.buffer_in, 64);
  const std::string new_path = memory.GetString(request.buffer_in + 64, 64);
  return MakeIPCReply([&](Ticks ticks) {
    return ConvertResult(m_core.RenameFile(handle.uid, handle.gid, old_path, new_path, ticks));
  });
}

FS::ResultCode FSCore::CreateFile(FS::Uid uid, FS::Gid gid, const std::string& path,
                                  FS::FileAttribute attribute, FS::Modes modes, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);

  const ResultCode result = m_ios.GetFS()->CreateFile(uid, gid, path, attribute, modes);
  ticks.Add(GetSuperblockWriteTbTicks(m_ios.GetVersion()));
  LogResult(result, "CreateFile({})", path);
  return result;
}

IPCReply FSDevice::CreateFile(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(GetSystem().GetMemory(), request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));
  return MakeIPCReply([&](Ticks ticks) {
    return ConvertResult(
        m_core.CreateFile(handle.uid, handle.gid, params->path, params->attribute, params->modes));
  });
}

IPCReply FSDevice::SetFileVersionControl(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(GetSystem().GetMemory(), request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  // FS_SetFileVersionControl(ctx->uid, params->path, params->attribute)
  ERROR_LOG_FMT(IOS_FS, "SetFileVersionControl({}, {:#x}): Stubbed", params->path,
                params->attribute);
  return GetFSReply(IPC_SUCCESS);
}

IPCReply FSDevice::GetFileStats(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_out_size < 8)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  return MakeIPCReply([&](Ticks ticks) {
    const Result<FileStatus> status = m_core.GetFileStatus(request.fd, ticks);
    if (!status)
      return ConvertResult(status.Error());

    auto& system = GetSystem();
    auto& memory = system.GetMemory();

    ISFSFileStats out;
    out.size = status->size;
    out.seek_position = status->offset;
    memory.CopyToEmu(request.buffer_out, &out, sizeof(out));
    return IPC_SUCCESS;
  });
}

FS::Result<FS::FileStatus> FSCore::GetFileStatus(u64 fd, Ticks ticks)
{
  ticks.Add(IPC_OVERHEAD_TICKS);
  const auto& handle = m_fd_map[fd];
  if (handle.fs_fd == INVALID_FD)
    return ResultCode::Invalid;

  auto status = m_ios.GetFS()->GetFileStatus(handle.fs_fd);
  LogResult(status, "GetFileStatus({})", handle.name.data());
  return status;
}

IPCReply FSDevice::GetUsage(const Handle& handle, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 2) || request.in_vectors[0].size != 64 ||
      request.io_vectors[0].size != 4 || request.io_vectors[1].size != 4)
  {
    return GetFSReply(ConvertResult(ResultCode::Invalid));
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const std::string directory = memory.GetString(request.in_vectors[0].address, 64);
  const Result<DirectoryStats> stats = m_ios.GetFS()->GetDirectoryStats(directory);
  LogResult(stats, "GetDirectoryStats({})", directory);
  if (!stats)
    return GetFSReply(ConvertResult(stats.Error()));

  memory.Write_U32(stats->used_clusters, request.io_vectors[0].address);
  memory.Write_U32(stats->used_inodes, request.io_vectors[1].address);
  return GetFSReply(IPC_SUCCESS);
}

IPCReply FSDevice::Shutdown(const Handle& handle, const IOCtlRequest& request)
{
  INFO_LOG_FMT(IOS_FS, "Shutdown");
  return GetFSReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
