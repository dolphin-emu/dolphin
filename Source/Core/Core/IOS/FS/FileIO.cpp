// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/FS/FileIO.h"

#include <cinttypes>
#include <cstdio>
#include <map>
#include <memory>
#include <utility>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Core/CommonTitles.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/IOS.h"

namespace IOS
{
namespace HLE
{
static std::map<std::string, std::weak_ptr<File::IOFile>> openFiles;

// This is used by several of the FileIO and /dev/fs functions
std::string BuildFilename(const std::string& wii_path)
{
  std::string nand_path = File::GetUserPath(D_SESSION_WIIROOT_IDX);
  if (wii_path.compare(0, 1, "/") == 0)
    return nand_path + Common::EscapePath(wii_path);

  _assert_(false);
  return nand_path;
}

void CreateVirtualFATFilesystem()
{
  const int cdbSize = 0x01400000;
  const std::string cdbPath =
      Common::GetTitleDataPath(Titles::SYSTEM_MENU, Common::FROM_SESSION_ROOT) + "cdb.vff";
  if ((int)File::GetSize(cdbPath) < cdbSize)
  {
    // cdb.vff is a virtual Fat filesystem created on first launch of sysmenu
    // we create it here as it is faster ~3 minutes for me when sysmenu does it ~1 second created
    // here
    const u8 cdbHDR[0x20] = {'V', 'F', 'F', 0x20, 0xfe, 0xff, 1, 0, 1, 0x40, 0, 0, 0, 0x20};
    const u8 cdbFAT[4] = {0xf0, 0xff, 0xff, 0xff};

    File::IOFile cdbFile(cdbPath, "wb");
    if (cdbFile)
    {
      cdbFile.WriteBytes(cdbHDR, 0x20);
      cdbFile.WriteBytes(cdbFAT, 0x4);
      cdbFile.Seek(0x14020, SEEK_SET);
      cdbFile.WriteBytes(cdbFAT, 0x4);
      // 20 MiB file
      cdbFile.Seek(cdbSize - 1, SEEK_SET);
      // write the final 0 to 0 file from the second FAT to 20 MiB
      cdbFile.WriteBytes(cdbHDR + 14, 1);
      if (!cdbFile.IsGood())
      {
        cdbFile.Close();
        File::Delete(cdbPath);
      }
      cdbFile.Flush();
      cdbFile.Close();
    }
  }
}

namespace Device
{
FileIO::FileIO(Kernel& ios, const std::string& device_name)
    : Device(ios, device_name, DeviceType::FileIO)
{
}

ReturnCode FileIO::Close(u32 fd)
{
  INFO_LOG(IOS_FILEIO, "FileIO: Close %s", m_name.c_str());
  m_Mode = 0;

  // Let go of our pointer to the file, it will automatically close if we are the last handle
  // accessing it.
  m_file.reset();

  m_is_active = false;
  return IPC_SUCCESS;
}

ReturnCode FileIO::Open(const OpenRequest& request)
{
  m_Mode = request.flags;

  static const char* const Modes[] = {"Unk Mode", "Read only", "Write only", "Read and Write"};

  m_filepath = BuildFilename(m_name);

  // The file must exist before we can open it
  // It should be created by ISFS_CreateFile, not here
  if (!File::IsFile(m_filepath))
  {
    WARN_LOG(IOS_FILEIO, "FileIO: Open (%s) failed - File doesn't exist %s", Modes[m_Mode],
             m_filepath.c_str());
    return FS_ENOENT;
  }

  INFO_LOG(IOS_FILEIO, "FileIO: Open %s (%s == %08X)", m_name.c_str(), Modes[m_Mode], m_Mode);
  OpenFile();

  m_is_active = true;
  return IPC_SUCCESS;
}

// This isn't theadsafe, but it's only called from the CPU thread.
void FileIO::OpenFile()
{
  // On the wii, all file operations are strongly ordered.
  // If a game opens the same file twice (or 8 times, looking at you PokePark Wii)
  // and writes to one file handle, it will be able to immediately read the written
  // data from the other handle.
  // On 'real' operating systems, there are various buffers and caches meaning
  // applications doing such naughty things will not get expected results.

  // So we fix this by catching any attempts to open the same file twice and
  // only opening one file. Accesses to a single file handle are ordered.
  //
  // Hall of Shame:
  //    - PokePark Wii (gets stuck on the loading screen of Pikachu falling)
  //    - PokePark 2 (Also gets stuck while loading)
  //    - Wii System Menu (Can't access the system settings, gets stuck on blank screen)
  //    - The Beatles: Rock Band (saving doesn't work)

  // Check if the file has already been opened.
  auto search = openFiles.find(m_name);
  if (search != openFiles.end())
  {
    m_file = search->second.lock();  // Lock a shared pointer to use.
  }
  else
  {
    std::string path = m_name;
    // This code will be called when all references to the shared pointer below have been removed.
    auto deleter = [path](File::IOFile* ptr) {
      delete ptr;             // IOFile's deconstructor closes the file.
      openFiles.erase(path);  // erase the weak pointer from the list of open files.
    };

    // All files are opened read/write. Actual access rights will be controlled per handle by the
    // read/write functions below
    m_file = std::shared_ptr<File::IOFile>(new File::IOFile(m_filepath, "r+b"),
                                           deleter);  // Use the custom deleter from above.

    // Store a weak pointer to our newly opened file in the cache.
    openFiles[path] = std::weak_ptr<File::IOFile>(m_file);
  }
}

IPCCommandResult FileIO::Seek(const SeekRequest& request)
{
  if (!m_file->IsOpen())
    return GetDefaultReply(FS_ENOENT);

  const u32 file_size = static_cast<u32>(m_file->GetSize());
  DEBUG_LOG(IOS_FILEIO, "FileIO: Seek Pos: 0x%08x, Mode: %i (%s, Length=0x%08x)", request.offset,
            request.mode, m_name.c_str(), file_size);

  u32 new_position = 0;
  switch (request.mode)
  {
  case IOS_SEEK_SET:
    new_position = request.offset;
    break;

  case IOS_SEEK_CUR:
    new_position = m_SeekPos + request.offset;
    break;

  case IOS_SEEK_END:
    new_position = file_size + request.offset;
    break;

  default:
    return GetDefaultReply(FS_EINVAL);
  }

  if (new_position > file_size)
    return GetDefaultReply(FS_EINVAL);

  m_SeekPos = new_position;
  return GetDefaultReply(new_position);
}

IPCCommandResult FileIO::Read(const ReadWriteRequest& request)
{
  if (!m_file->IsOpen())
  {
    ERROR_LOG(IOS_FILEIO, "Failed to read from %s (Addr=0x%08x Size=0x%x) - file could "
                          "not be opened or does not exist",
              m_name.c_str(), request.buffer, request.size);
    return GetDefaultReply(FS_ENOENT);
  }

  if (m_Mode == IOS_OPEN_WRITE)
  {
    WARN_LOG(IOS_FILEIO, "Attempted to read 0x%x bytes to 0x%08x on a write-only file %s",
             request.size, request.buffer, m_name.c_str());
    return GetDefaultReply(FS_EACCESS);
  }

  u32 requested_read_length = request.size;
  const u32 file_size = static_cast<u32>(m_file->GetSize());
  // IOS has this check in the read request handler.
  if (requested_read_length + m_SeekPos > file_size)
    requested_read_length = file_size - m_SeekPos;

  DEBUG_LOG(IOS_FILEIO, "Read 0x%x bytes to 0x%08x from %s", request.size, request.buffer,
            m_name.c_str());
  m_file->Seek(m_SeekPos, SEEK_SET);  // File might be opened twice, need to seek before we read
  const u32 number_of_bytes_read = static_cast<u32>(
      fread(Memory::GetPointer(request.buffer), 1, requested_read_length, m_file->GetHandle()));

  if (number_of_bytes_read != requested_read_length && ferror(m_file->GetHandle()))
    return GetDefaultReply(FS_EACCESS);

  // IOS returns the number of bytes read and adds that value to the seek position,
  // instead of adding the *requested* read length.
  m_SeekPos += number_of_bytes_read;
  return GetDefaultReply(number_of_bytes_read);
}

IPCCommandResult FileIO::Write(const ReadWriteRequest& request)
{
  s32 return_value = FS_EACCESS;
  if (m_file->IsOpen())
  {
    if (m_Mode == IOS_OPEN_READ)
    {
      WARN_LOG(IOS_FILEIO,
               "FileIO: Attempted to write 0x%x bytes from 0x%08x to a read-only file %s",
               request.size, request.buffer, m_name.c_str());
    }
    else
    {
      DEBUG_LOG(IOS_FILEIO, "FileIO: Write 0x%04x bytes from 0x%08x to %s", request.size,
                request.buffer, m_name.c_str());
      m_file->Seek(m_SeekPos,
                   SEEK_SET);  // File might be opened twice, need to seek before we write
      if (m_file->WriteBytes(Memory::GetPointer(request.buffer), request.size))
      {
        return_value = request.size;
        m_SeekPos += request.size;
      }
    }
  }
  else
  {
    ERROR_LOG(IOS_FILEIO, "FileIO: Failed to read from %s (Addr=0x%08x Size=0x%x) - file could "
                          "not be opened or does not exist",
              m_name.c_str(), request.buffer, request.size);
    return_value = FS_ENOENT;
  }

  return GetDefaultReply(return_value);
}

IPCCommandResult FileIO::IOCtl(const IOCtlRequest& request)
{
  DEBUG_LOG(IOS_FILEIO, "FileIO: IOCtl (Device=%s)", m_name.c_str());

  switch (request.request)
  {
  case ISFS_IOCTL_GETFILESTATS:
    return GetFileStats(request);

  default:
    request.Log(GetDeviceName(), LogTypes::IOS_FILEIO, LogTypes::LERROR);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

void FileIO::PrepareForState(PointerWrap::Mode mode)
{
  // Temporally close the file, to prevent any issues with the savestating of /tmp
  // it can be opened again with another call to OpenFile()
  m_file.reset();
}

void FileIO::DoState(PointerWrap& p)
{
  DoStateShared(p);

  p.Do(m_Mode);
  p.Do(m_SeekPos);

  m_filepath = BuildFilename(m_name);

  // The file was closed during state (and might now be pointing at another file)
  // Open it again
  OpenFile();
}

IPCCommandResult FileIO::GetFileStats(const IOCtlRequest& request)
{
  if (!m_file->IsOpen())
    return GetDefaultReply(FS_ENOENT);

  DEBUG_LOG(IOS_FILEIO, "File: %s, Length: %" PRIu64 ", Pos: %u", m_name.c_str(), m_file->GetSize(),
            m_SeekPos);
  Memory::Write_U32(static_cast<u32>(m_file->GetSize()), request.buffer_out);
  Memory::Write_U32(m_SeekPos, request.buffer_out + 4);
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
