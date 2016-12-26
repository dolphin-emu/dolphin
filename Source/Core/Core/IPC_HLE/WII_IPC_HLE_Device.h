// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"

enum IOSReturnCode : s32
{
  IPC_SUCCESS = 0,           // Success
  IPC_EACCES = -1,           // Permission denied
  IPC_EEXIST = -2,           // File exists
  IPC_EINVAL = -4,           // Invalid argument or fd
  IPC_ENOENT = -6,           // File not found
  IPC_EQUEUEFULL = -8,       // Queue full
  IPC_EIO = -12,             // ECC error
  IPC_ENOMEM = -22,          // Alloc failed during request
  FS_EINVAL = -101,          // Invalid path
  FS_EACCESS = -102,         // Permission denied
  FS_ECORRUPT = -103,        // Corrupted NAND
  FS_EEXIST = -105,          // File exists
  FS_ENOENT = -106,          // No such file or directory
  FS_ENFILE = -107,          // Too many fds open
  FS_EFBIG = -108,           // Max block count reached?
  FS_EFDEXHAUSTED = -109,    // Too many fds open
  FS_ENAMELEN = -110,        // Pathname is too long
  FS_EFDOPEN = -111,         // FD is already open
  FS_EIO = -114,             // ECC error
  FS_ENOTEMPTY = -115,       // Directory not empty
  FS_EDIRDEPTH = -116,       // Max directory depth exceeded
  FS_EBUSY = -118,           // Resource busy
  IPC_EESEXHAUSTED = -1016,  // Max of 2 ES handles exceeded
};

struct IOSResourceRequest
{
  u32 address;
  IPCCommandType command;
  s32 return_value;
  u32 fd;
  IOSResourceRequest() = default;
  explicit IOSResourceRequest(u32 command_address);
  virtual ~IOSResourceRequest() = default;
  void SetReturnValue(s32 new_return_value);

  // Write out the IPC struct from command_address to number_of_commands numbers
  // of 4 byte commands.
  void DumpCommands(size_t number_of_commands = 8, LogTypes::LOG_TYPE type = LogTypes::WII_IPC_HLE,
                    LogTypes::LOG_LEVELS verbosity = LogTypes::LDEBUG) const;
};

enum IOSOpenMode : s32
{
  IOS_OPEN_READ = 1,
  IOS_OPEN_WRITE = 2,
  IOS_OPEN_RW = (IOS_OPEN_READ | IOS_OPEN_WRITE)
};

struct IOSResourceOpenRequest final : IOSResourceRequest
{
  std::string path;
  IOSOpenMode flags;
  explicit IOSResourceOpenRequest(u32 command_address);
};

struct IOSResourceReadWriteRequest final : IOSResourceRequest
{
  u32 data_addr;
  u32 length;
  std::vector<u8> MakeBuffer() const;
  void FillBuffer(const void* data, size_t source_size) const;
  explicit IOSResourceReadWriteRequest(u32 command_address);
};

struct IOSResourceSeekRequest final : IOSResourceRequest
{
  enum SeekMode
  {
    IOS_SEEK_SET = 0,
    IOS_SEEK_CUR = 1,
    IOS_SEEK_END = 2,
  };
  u32 offset;
  SeekMode mode;
  explicit IOSResourceSeekRequest(u32 command_address);
};

struct IOSResourceIOCtlRequest final : IOSResourceRequest
{
  u32 request;
  u32 in_addr;
  u32 in_size;
  u32 out_addr;
  u32 out_size;
  explicit IOSResourceIOCtlRequest(u32 command_address);
  std::vector<u8> MakeInBuffer() const;
  std::vector<u8> MakeOutBuffer() const;
  void FillOutBuffer(const void* data, size_t source_size) const;
  void Dump(const std::string& device_name, LogTypes::LOG_TYPE log_type = LogTypes::WII_IPC_HLE,
            LogTypes::LOG_LEVELS verbosity = LogTypes::LINFO) const;
  void Log(const std::string& device_name, LogTypes::LOG_TYPE log_type = LogTypes::WII_IPC_HLE,
           LogTypes::LOG_LEVELS verbosity = LogTypes::LINFO) const;
};

struct IOSResourceIOCtlVRequest final : IOSResourceRequest
{
  struct IOVector
  {
    u32 addr;
    u32 size;
    std::vector<u8> MakeBuffer() const;
    void FillBuffer(const void* data, size_t source_size) const;
  };
  u32 request;
  std::vector<IOVector> in_vectors;
  std::vector<IOVector> io_vectors;
  explicit IOSResourceIOCtlVRequest(u32 command_address);
  bool HasInVectorWithAddress(u32 address) const;
  void Dump(const std::string& device_name, LogTypes::LOG_TYPE log_type = LogTypes::WII_IPC_HLE,
            LogTypes::LOG_LEVELS verbosity = LogTypes::LDEBUG) const;
};

class IWII_IPC_HLE_Device
{
public:
  IWII_IPC_HLE_Device(u32 device_id, const std::string& device_name, bool hardware = true);

  virtual ~IWII_IPC_HLE_Device() = default;
  // Release any resources which might interfere with savestating.
  virtual void PrepareForState(PointerWrap::Mode mode) {}
  virtual void DoState(PointerWrap& p);
  void DoStateShared(PointerWrap& p);

  const std::string& GetDeviceName() const { return m_name; }
  u32 GetDeviceID() const { return m_device_id; }
  // Replies to Open and Close requests are sent by WII_IPC_HLE, not by the devices themselves.
  virtual IOSReturnCode Open(IOSResourceOpenRequest& request);
  virtual void Close();
  virtual IPCCommandResult Seek(IOSResourceSeekRequest& request);
  virtual IPCCommandResult Read(IOSResourceReadWriteRequest& request);
  virtual IPCCommandResult Write(IOSResourceReadWriteRequest& request);
  virtual IPCCommandResult IOCtl(IOSResourceIOCtlRequest& request);
  virtual IPCCommandResult IOCtlV(IOSResourceIOCtlVRequest& request);

  virtual u32 Update() { return 0; }
  virtual bool IsHardware() const { return m_is_hardware; }
  virtual bool IsOpened() const { return m_is_active; }
  static IPCCommandResult GetDefaultReply();
  static IPCCommandResult GetNoReply();

  std::string m_name;

protected:
  // STATE_TO_SAVE
  u32 m_device_id;
  bool m_is_hardware;
  bool m_is_active = false;
};
