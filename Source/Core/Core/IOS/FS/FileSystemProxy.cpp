// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/FS/FileSystemProxy.h"

#include <cstring>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/FS/FileSystem.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
using namespace IOS::HLE::FS;

static s32 ConvertResult(ResultCode code)
{
  if (code == ResultCode::Success)
    return IPC_SUCCESS;
  return -(static_cast<s32>(code) + 100);
}

static IPCCommandResult GetFSReply(s32 return_value, u64 extra_tb_ticks = 0)
{
  // According to hardware tests, FS takes at least 2700 TB ticks to reply to commands.
  return {return_value, true, (2700 + extra_tb_ticks) * SystemTimers::TIMER_RATIO};
}

FS::FS(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

void FS::DoState(PointerWrap& p)
{
  p.Do(m_fd_map);
}

static void LogResult(const std::string& command, ResultCode code)
{
  GENERIC_LOG(LogTypes::IOS_FS, (code == ResultCode::Success ? LogTypes::LINFO : LogTypes::LERROR),
              "%s: result %d", command.c_str(), ConvertResult(code));
}

template <typename T>
static void LogResult(const std::string& command, const Result<T>& result)
{
  LogResult(command, result.Succeeded() ? ResultCode::Success : result.Error());
}

IPCCommandResult FS::Open(const OpenRequest& request)
{
  if (m_fd_map.size() >= 16)
    return GetDefaultReply(ConvertResult(ResultCode::NoFreeHandle));

  if (request.path.size() >= 64)
    return GetDefaultReply(ConvertResult(ResultCode::Invalid));

  if (request.path == "/dev/fs")
  {
    m_fd_map[request.fd] = {request.gid, request.uid, INVALID_FD};
    return GetDefaultReply(IPC_SUCCESS);
  }

  auto backend_fd = m_ios.GetFS()->OpenFile(request.uid, request.gid, request.path,
                                            static_cast<Mode>(request.flags & 3));
  LogResult(StringFromFormat("OpenFile(%s)", request.path.c_str()), backend_fd);
  if (!backend_fd)
    return GetFSReply(ConvertResult(backend_fd.Error()));

  m_fd_map[request.fd] = {request.gid, request.uid, backend_fd->Release()};
  std::strncpy(m_fd_map[request.fd].name.data(), request.path.c_str(), 64);
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::Close(u32 fd)
{
  if (m_fd_map[fd].fs_fd != INVALID_FD)
  {
    const ResultCode result = m_ios.GetFS()->Close(m_fd_map[fd].fs_fd);
    LogResult(StringFromFormat("Close(%s)", m_fd_map[fd].name.data()), result);
    m_fd_map.erase(fd);
    if (result != ResultCode::Success)
      return GetFSReply(ConvertResult(result));
  }
  else
  {
    m_fd_map.erase(fd);
  }
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::Read(const ReadWriteRequest& request)
{
  const Handle& handle = m_fd_map[request.fd];
  if (handle.fs_fd == INVALID_FD)
    return GetDefaultReply(ConvertResult(ResultCode::Invalid));

  const Result<u32> result = m_ios.GetFS()->ReadBytesFromFile(
      handle.fs_fd, Memory::GetPointer(request.buffer), request.size);
  LogResult(
      StringFromFormat("Read(%s, 0x%08x, %u)", handle.name.data(), request.buffer, request.size),
      result);
  if (!result)
    return GetDefaultReply(ConvertResult(result.Error()));
  return GetDefaultReply(*result);
}

IPCCommandResult FS::Write(const ReadWriteRequest& request)
{
  const Handle& handle = m_fd_map[request.fd];
  if (handle.fs_fd == INVALID_FD)
    return GetDefaultReply(ConvertResult(ResultCode::Invalid));

  const Result<u32> result = m_ios.GetFS()->WriteBytesToFile(
      handle.fs_fd, Memory::GetPointer(request.buffer), request.size);
  LogResult(
      StringFromFormat("Write(%s, 0x%08x, %u)", handle.name.data(), request.buffer, request.size),
      result);
  if (!result)
    return GetDefaultReply(ConvertResult(result.Error()));
  return GetDefaultReply(*result);
}

IPCCommandResult FS::Seek(const SeekRequest& request)
{
  const Handle& handle = m_fd_map[request.fd];
  if (handle.fs_fd == INVALID_FD)
    return GetDefaultReply(ConvertResult(ResultCode::Invalid));

  const Result<u32> result =
      m_ios.GetFS()->SeekFile(handle.fs_fd, request.offset, IOS::HLE::FS::SeekMode(request.mode));
  LogResult(
      StringFromFormat("Seek(%s, 0x%08x, %u)", handle.name.data(), request.offset, request.mode),
      result);
  if (!result)
    return GetDefaultReply(ConvertResult(result.Error()));
  return GetDefaultReply(*result);
}

#pragma pack(push, 1)
struct ISFSParams
{
  Common::BigEndianValue<Uid> uid;
  Common::BigEndianValue<Gid> gid;
  char path[64];
  Mode owner_mode;
  Mode group_mode;
  Mode other_mode;
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
  return GetFSReply(ConvertResult(result));
}

IPCCommandResult FS::GetStats(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_out_size < sizeof(ISFSNandStats))
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const Result<NandStats> stats = m_ios.GetFS()->GetNandStats();
  LogResult("GetNandStats", stats);
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

  const ResultCode result =
      m_ios.GetFS()->CreateDirectory(handle.uid, handle.gid, params->path, params->attribute,
                                     params->owner_mode, params->group_mode, params->other_mode);
  LogResult(StringFromFormat("CreateDirectory(%s)", params->path), result);
  return GetFSReply(ConvertResult(result));
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
  LogResult(StringFromFormat("ReadDirectory(%s)", directory.c_str()), list);
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
    file_list_address += 13;
  }
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::SetAttribute(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result = m_ios.GetFS()->SetMetadata(
      handle.uid, params->path, params->uid, params->gid, params->attribute, params->owner_mode,
      params->group_mode, params->other_mode);
  LogResult(StringFromFormat("SetMetadata(%s)", params->path), result);
  return GetFSReply(ConvertResult(result));
}

IPCCommandResult FS::GetAttribute(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64 || request.buffer_out_size < sizeof(ISFSParams))
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const std::string path = Memory::GetString(request.buffer_in, 64);
  const Result<Metadata> metadata = m_ios.GetFS()->GetMetadata(handle.uid, handle.gid, path);
  LogResult(StringFromFormat("GetMetadata(%s)", path.c_str()), metadata);
  if (!metadata)
    return GetFSReply(ConvertResult(metadata.Error()));

  // Yes, the other members aren't copied at all. Actually, IOS does not even memset
  // the struct at all, which means uninitialised bytes from the stack are returned.
  // For the sake of determinism, let's just zero initialise the struct.
  ISFSParams out{};
  out.uid = metadata->uid;
  out.gid = metadata->gid;
  out.attribute = metadata->attribute;
  out.owner_mode = metadata->owner_mode;
  out.group_mode = metadata->group_mode;
  out.other_mode = metadata->other_mode;
  Memory::CopyToEmu(request.buffer_out, &out, sizeof(out));
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::DeleteFile(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const std::string path = Memory::GetString(request.buffer_in, 64);
  const ResultCode result = m_ios.GetFS()->Delete(handle.uid, handle.gid, path);
  LogResult(StringFromFormat("Delete(%s)", path.c_str()), result);
  return GetFSReply(ConvertResult(result));
}

IPCCommandResult FS::RenameFile(const Handle& handle, const IOCtlRequest& request)
{
  if (request.buffer_in_size < 64 * 2)
    return GetFSReply(ConvertResult(ResultCode::Invalid));

  const std::string old_path = Memory::GetString(request.buffer_in, 64);
  const std::string new_path = Memory::GetString(request.buffer_in + 64, 64);
  const ResultCode result = m_ios.GetFS()->Rename(handle.uid, handle.gid, old_path, new_path);
  LogResult(StringFromFormat("Rename(%s, %s)", old_path.c_str(), new_path.c_str()), result);
  return GetFSReply(ConvertResult(result));
}

IPCCommandResult FS::CreateFile(const Handle& handle, const IOCtlRequest& request)
{
  const auto params = GetParams<ISFSParams>(request);
  if (!params)
    return GetFSReply(ConvertResult(params.Error()));

  const ResultCode result =
      m_ios.GetFS()->CreateFile(handle.uid, handle.gid, params->path, params->attribute,
                                params->owner_mode, params->group_mode, params->other_mode);
  LogResult(StringFromFormat("CreateFile(%s)", params->path), result);
  return GetFSReply(ConvertResult(result));
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
  LogResult(StringFromFormat("GetFileStatus(%s)", handle.name.data()), status);
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
  LogResult(StringFromFormat("GetDirectoryStats(%s)", directory.c_str()), stats);
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
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
