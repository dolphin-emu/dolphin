// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/FS/FS.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileIO.h"

namespace IOS
{
namespace HLE
{
static bool IsValidWiiPath(const std::string& path)
{
  return path.compare(0, 1, "/") == 0;
}

namespace Device
{
FS::FS(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
  const std::string tmp_dir = BuildFilename("/tmp");
  File::DeleteDirRecursively(tmp_dir);
  File::CreateDir(tmp_dir);
}

void FS::DoState(PointerWrap& p)
{
  DoStateShared(p);

  // handle /tmp

  std::string Path = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/tmp";
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
      std::string filename;
      p.Do(filename);
      std::string name = Path + DIR_SEP + filename;
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

    File::FSTEntry parentEntry = File::ScanDirectoryTree(Path, true);
    std::deque<File::FSTEntry> todo;
    todo.insert(todo.end(), parentEntry.children.begin(), parentEntry.children.end());

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
}

ReturnCode FS::Open(const OpenRequest& request)
{
  m_is_active = true;
  return IPC_SUCCESS;
}

// Get total filesize of contents of a directory (recursive)
// Only used for ES_GetUsage atm, could be useful elsewhere?
static u64 ComputeTotalFileSize(const File::FSTEntry& parentEntry)
{
  u64 sizeOfFiles = 0;
  for (const File::FSTEntry& entry : parentEntry.children)
  {
    if (entry.isDirectory)
      sizeOfFiles += ComputeTotalFileSize(entry);
    else
      sizeOfFiles += entry.size;
  }
  return sizeOfFiles;
}

IPCCommandResult FS::IOCtl(const IOCtlRequest& request)
{
  Memory::Memset(request.buffer_out, 0, request.buffer_out_size);

  switch (request.request)
  {
  case IOCTL_GET_STATS:
    return GetStats(request);
  case IOCTL_CREATE_DIR:
    return CreateDirectory(request);
  case IOCTL_SET_ATTR:
    return SetAttribute(request);
  case IOCTL_GET_ATTR:
    return GetAttribute(request);
  case IOCTL_DELETE_FILE:
    return DeleteFile(request);
  case IOCTL_RENAME_FILE:
    return RenameFile(request);
  case IOCTL_CREATE_FILE:
    return CreateFile(request);
  case IOCTL_SHUTDOWN:
    return Shutdown(request);
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_FILEIO);
    break;
  }

  return GetFSReply(FS_EINVAL);
}

IPCCommandResult FS::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  case IOCTLV_READ_DIR:
    return ReadDirectory(request);
  case IOCTLV_GETUSAGE:
    return GetUsage(request);
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_FILEIO);
    break;
  }

  return GetFSReply(IPC_SUCCESS);
}

// ~1/1000th of a second is too short and causes hangs in Wii Party
// Play it safe at 1/500th
IPCCommandResult FS::GetFSReply(const s32 return_value) const
{
  return {return_value, true, SystemTimers::GetTicksPerSecond() / 500};
}

IPCCommandResult FS::GetStats(const IOCtlRequest& request)
{
  if (request.buffer_out_size < 0x1c)
    return GetFSReply(-1017);

  WARN_LOG(IOS_FILEIO, "FS: GET STATS - returning static values for now");

  // TODO: scrape the real amounts from somewhere...
  NANDStat fs;
  fs.BlockSize = 0x4000;
  fs.FreeUserBlocks = 0x5DEC;
  fs.UsedUserBlocks = 0x1DD4;
  fs.FreeSysBlocks = 0x10;
  fs.UsedSysBlocks = 0x02F0;
  fs.Free_INodes = 0x146B;
  fs.Used_Inodes = 0x0394;

  std::memcpy(Memory::GetPointer(request.buffer_out), &fs, sizeof(NANDStat));

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::CreateDirectory(const IOCtlRequest& request)
{
  _dbg_assert_(IOS_FILEIO, request.buffer_out_size == 0);
  u32 Addr = request.buffer_in;

  u32 OwnerID = Memory::Read_U32(Addr);
  Addr += 4;
  u16 GroupID = Memory::Read_U16(Addr);
  Addr += 2;

  const std::string wii_path = Memory::GetString(Addr, 64);
  if (!IsValidWiiPath(wii_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string DirName(BuildFilename(wii_path));
  Addr += 64;
  Addr += 9;  // owner attribs, permission
  u8 Attribs = Memory::Read_U8(Addr);

  INFO_LOG(IOS_FILEIO, "FS: CREATE_DIR %s, OwnerID %#x, GroupID %#x, Attributes %#x",
           DirName.c_str(), OwnerID, GroupID, Attribs);

  DirName += DIR_SEP;
  File::CreateFullPath(DirName);
  _dbg_assert_msg_(IOS_FILEIO, File::IsDirectory(DirName), "FS: CREATE_DIR %s failed",
                   DirName.c_str());

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::SetAttribute(const IOCtlRequest& request)
{
  u32 Addr = request.buffer_in;

  u32 OwnerID = Memory::Read_U32(Addr);
  Addr += 4;
  u16 GroupID = Memory::Read_U16(Addr);
  Addr += 2;

  const std::string wii_path = Memory::GetString(Addr, 64);
  if (!IsValidWiiPath(wii_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string Filename = BuildFilename(wii_path);
  Addr += 64;
  u8 OwnerPerm = Memory::Read_U8(Addr);
  Addr += 1;
  u8 GroupPerm = Memory::Read_U8(Addr);
  Addr += 1;
  u8 OtherPerm = Memory::Read_U8(Addr);
  Addr += 1;
  u8 Attributes = Memory::Read_U8(Addr);
  Addr += 1;

  INFO_LOG(IOS_FILEIO, "FS: SetAttrib %s", Filename.c_str());
  DEBUG_LOG(IOS_FILEIO, "    OwnerID: 0x%08x", OwnerID);
  DEBUG_LOG(IOS_FILEIO, "    GroupID: 0x%04x", GroupID);
  DEBUG_LOG(IOS_FILEIO, "    OwnerPerm: 0x%02x", OwnerPerm);
  DEBUG_LOG(IOS_FILEIO, "    GroupPerm: 0x%02x", GroupPerm);
  DEBUG_LOG(IOS_FILEIO, "    OtherPerm: 0x%02x", OtherPerm);
  DEBUG_LOG(IOS_FILEIO, "    Attributes: 0x%02x", Attributes);

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::GetAttribute(const IOCtlRequest& request)
{
  _dbg_assert_msg_(IOS_FILEIO, request.buffer_out_size == 76,
                   "    GET_ATTR needs an 76 bytes large output buffer but it is %i bytes large",
                   request.buffer_out_size);

  u32 OwnerID = 0;
  u16 GroupID = 0x3031;  // this is also known as makercd, 01 (0x3031) for nintendo and 08
                         // (0x3038) for MH3 etc

  const std::string wii_path = Memory::GetString(request.buffer_in, 64);
  if (!IsValidWiiPath(wii_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string Filename = BuildFilename(wii_path);
  u8 OwnerPerm = 0x3;    // read/write
  u8 GroupPerm = 0x3;    // read/write
  u8 OtherPerm = 0x3;    // read/write
  u8 Attributes = 0x00;  // no attributes

  // Hack: if the path that is being accessed is within an installed title directory, get the
  // UID/GID from the installed title TMD.
  u64 title_id;
  if (IsTitlePath(Filename, Common::FROM_SESSION_ROOT, &title_id))
  {
    IOS::ES::TMDReader tmd = GetIOS()->GetES()->FindInstalledTMD(title_id);
    if (tmd.IsValid())
    {
      GroupID = tmd.GetGroupId();
    }
  }

  if (File::IsDirectory(Filename))
  {
    INFO_LOG(IOS_FILEIO, "FS: GET_ATTR Directory %s - all permission flags are set",
             Filename.c_str());
  }
  else
  {
    if (File::Exists(Filename))
    {
      INFO_LOG(IOS_FILEIO, "FS: GET_ATTR %s - all permission flags are set", Filename.c_str());
    }
    else
    {
      INFO_LOG(IOS_FILEIO, "FS: GET_ATTR unknown %s", Filename.c_str());
      return GetFSReply(FS_ENOENT);
    }
  }

  // write answer to buffer
  if (request.buffer_out_size == 76)
  {
    u32 Addr = request.buffer_out;
    Memory::Write_U32(OwnerID, Addr);
    Addr += 4;
    Memory::Write_U16(GroupID, Addr);
    Addr += 2;
    memcpy(Memory::GetPointer(Addr), Memory::GetPointer(request.buffer_in), 64);
    Addr += 64;
    Memory::Write_U8(OwnerPerm, Addr);
    Addr += 1;
    Memory::Write_U8(GroupPerm, Addr);
    Addr += 1;
    Memory::Write_U8(OtherPerm, Addr);
    Addr += 1;
    Memory::Write_U8(Attributes, Addr);
    Addr += 1;
  }

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::DeleteFile(const IOCtlRequest& request)
{
  _dbg_assert_(IOS_FILEIO, request.buffer_out_size == 0);
  int Offset = 0;

  const std::string wii_path = Memory::GetString(request.buffer_in + Offset, 64);
  if (!IsValidWiiPath(wii_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string Filename = BuildFilename(wii_path);
  Offset += 64;
  if (File::Delete(Filename))
  {
    INFO_LOG(IOS_FILEIO, "FS: DeleteFile %s", Filename.c_str());
  }
  else if (File::DeleteDir(Filename))
  {
    INFO_LOG(IOS_FILEIO, "FS: DeleteDir %s", Filename.c_str());
  }
  else
  {
    WARN_LOG(IOS_FILEIO, "FS: DeleteFile %s - failed!!!", Filename.c_str());
  }

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::RenameFile(const IOCtlRequest& request)
{
  _dbg_assert_(IOS_FILEIO, request.buffer_out_size == 0);
  int Offset = 0;

  const std::string wii_path = Memory::GetString(request.buffer_in + Offset, 64);
  if (!IsValidWiiPath(wii_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path.c_str());
    return GetFSReply(FS_EINVAL);
  }
  std::string Filename = BuildFilename(wii_path);
  Offset += 64;

  const std::string wii_path_rename = Memory::GetString(request.buffer_in + Offset, 64);
  if (!IsValidWiiPath(wii_path_rename))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path_rename.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string FilenameRename = BuildFilename(wii_path_rename);
  Offset += 64;

  // try to make the basis directory
  File::CreateFullPath(FilenameRename);

  // if there is already a file, delete it
  if (File::Exists(Filename) && File::Exists(FilenameRename))
  {
    File::Delete(FilenameRename);
  }

  // finally try to rename the file
  if (File::Rename(Filename, FilenameRename))
  {
    INFO_LOG(IOS_FILEIO, "FS: Rename %s to %s", Filename.c_str(), FilenameRename.c_str());
  }
  else
  {
    ERROR_LOG(IOS_FILEIO, "FS: Rename %s to %s - failed", Filename.c_str(), FilenameRename.c_str());
    return GetFSReply(FS_ENOENT);
  }

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::CreateFile(const IOCtlRequest& request)
{
  _dbg_assert_(IOS_FILEIO, request.buffer_out_size == 0);

  u32 Addr = request.buffer_in;
  u32 OwnerID = Memory::Read_U32(Addr);
  Addr += 4;
  u16 GroupID = Memory::Read_U16(Addr);
  Addr += 2;

  const std::string wii_path = Memory::GetString(Addr, 64);
  if (!IsValidWiiPath(wii_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", wii_path.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string Filename(BuildFilename(wii_path));
  Addr += 64;
  u8 OwnerPerm = Memory::Read_U8(Addr);
  Addr++;
  u8 GroupPerm = Memory::Read_U8(Addr);
  Addr++;
  u8 OtherPerm = Memory::Read_U8(Addr);
  Addr++;
  u8 Attributes = Memory::Read_U8(Addr);
  Addr++;

  INFO_LOG(IOS_FILEIO, "FS: CreateFile %s", Filename.c_str());
  DEBUG_LOG(IOS_FILEIO, "    OwnerID: 0x%08x", OwnerID);
  DEBUG_LOG(IOS_FILEIO, "    GroupID: 0x%04x", GroupID);
  DEBUG_LOG(IOS_FILEIO, "    OwnerPerm: 0x%02x", OwnerPerm);
  DEBUG_LOG(IOS_FILEIO, "    GroupPerm: 0x%02x", GroupPerm);
  DEBUG_LOG(IOS_FILEIO, "    OtherPerm: 0x%02x", OtherPerm);
  DEBUG_LOG(IOS_FILEIO, "    Attributes: 0x%02x", Attributes);

  // check if the file already exist
  if (File::Exists(Filename))
  {
    INFO_LOG(IOS_FILEIO, "\tresult = FS_EEXIST");
    return GetFSReply(FS_EEXIST);
  }

  // create the file
  File::CreateFullPath(Filename);  // just to be sure
  bool Result = File::CreateEmptyFile(Filename);
  if (!Result)
  {
    ERROR_LOG(IOS_FILEIO, "FS: couldn't create new file");
    PanicAlert("FS: couldn't create new file");
    return GetFSReply(FS_EINVAL);
  }

  INFO_LOG(IOS_FILEIO, "\tresult = IPC_SUCCESS");
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::Shutdown(const IOCtlRequest& request)
{
  // TODO: stop emulation
  INFO_LOG(IOS_FILEIO, "Wii called Shutdown()");
  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::ReadDirectory(const IOCtlVRequest& request)
{
  const std::string relative_path =
      Memory::GetString(request.in_vectors[0].address, request.in_vectors[0].size);

  if (!IsValidWiiPath(relative_path))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", relative_path.c_str());
    return GetFSReply(FS_EINVAL);
  }

  // the Wii uses this function to define the type (dir or file)
  std::string DirName(BuildFilename(relative_path));

  INFO_LOG(IOS_FILEIO, "FS: IOCTL_READ_DIR %s", DirName.c_str());

  const File::FileInfo file_info(DirName);

  if (!file_info.Exists())
  {
    WARN_LOG(IOS_FILEIO, "FS: Search not found: %s", DirName.c_str());
    return GetFSReply(FS_ENOENT);
  }

  if (!file_info.IsDirectory())
  {
    // It's not a directory, so error.
    // Games don't usually seem to care WHICH error they get, as long as it's <
    // Well the system menu CARES!
    WARN_LOG(IOS_FILEIO, "\tNot a directory - return FS_EINVAL");
    return GetFSReply(FS_EINVAL);
  }

  File::FSTEntry entry = File::ScanDirectoryTree(DirName, false);

  // it is one
  if ((request.in_vectors.size() == 1) && (request.io_vectors.size() == 1))
  {
    size_t numFile = entry.children.size();
    INFO_LOG(IOS_FILEIO, "\t%zu files found", numFile);

    Memory::Write_U32((u32)numFile, request.io_vectors[0].address);
  }
  else
  {
    for (File::FSTEntry& child : entry.children)
    {
      // Decode escaped invalid file system characters so that games (such as
      // Harry Potter and the Half-Blood Prince) can find what they expect.
      child.virtualName = Common::UnescapeFileName(child.virtualName);
    }

    std::sort(entry.children.begin(), entry.children.end(),
              [](const File::FSTEntry& one, const File::FSTEntry& two) {
                return one.virtualName < two.virtualName;
              });

    u32 MaxEntries = Memory::Read_U32(request.in_vectors[0].address);

    memset(Memory::GetPointer(request.io_vectors[0].address), 0, request.io_vectors[0].size);

    size_t numFiles = 0;
    char* pFilename = (char*)Memory::GetPointer((u32)(request.io_vectors[0].address));

    for (size_t i = 0; i < entry.children.size() && i < MaxEntries; i++)
    {
      const std::string& FileName = entry.children[i].virtualName;

      strcpy(pFilename, FileName.c_str());
      pFilename += FileName.length();
      *pFilename++ = 0x00;  // termination
      numFiles++;

      INFO_LOG(IOS_FILEIO, "\tFound: %s", FileName.c_str());
    }

    Memory::Write_U32((u32)numFiles, request.io_vectors[1].address);
  }

  return GetFSReply(IPC_SUCCESS);
}

IPCCommandResult FS::GetUsage(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_FILEIO, request.io_vectors.size() == 2);
  _dbg_assert_(IOS_FILEIO, request.io_vectors[0].size == 4);
  _dbg_assert_(IOS_FILEIO, request.io_vectors[1].size == 4);

  // this command sucks because it asks of the number of used
  // fsBlocks and inodes
  // It should be correct, but don't count on it...
  std::string relativepath =
      Memory::GetString(request.in_vectors[0].address, request.in_vectors[0].size);

  if (!IsValidWiiPath(relativepath))
  {
    WARN_LOG(IOS_FILEIO, "Not a valid path: %s", relativepath.c_str());
    return GetFSReply(FS_EINVAL);
  }

  std::string path(BuildFilename(relativepath));
  u32 fsBlocks = 0;
  u32 iNodes = 0;

  INFO_LOG(IOS_FILEIO, "IOCTL_GETUSAGE %s", path.c_str());
  if (File::IsDirectory(path))
  {
    File::FSTEntry parentDir = File::ScanDirectoryTree(path, true);
    // add one for the folder itself
    iNodes = 1 + (u32)parentDir.size;

    u64 totalSize = ComputeTotalFileSize(parentDir);  // "Real" size, to be converted to nand blocks

    fsBlocks = (u32)(totalSize / (16 * 1024));  // one bock is 16kb

    INFO_LOG(IOS_FILEIO, "FS: fsBlock: %i, iNodes: %i", fsBlocks, iNodes);
  }
  else
  {
    fsBlocks = 0;
    iNodes = 0;
    WARN_LOG(IOS_FILEIO, "FS: fsBlock failed, cannot find directory: %s", path.c_str());
  }

  Memory::Write_U32(fsBlocks, request.io_vectors[0].address);
  Memory::Write_U32(iNodes, request.io_vectors[1].address);

  return GetFSReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
