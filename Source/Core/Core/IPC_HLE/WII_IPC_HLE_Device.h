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
#include "Common/StringUtil.h"
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

// A struct for IOS ioctlv calls
struct SIOCtlVBuffer
{
  explicit SIOCtlVBuffer(u32 address);

  const u32 m_Address;
  u32 Parameter;
  u32 NumberInBuffer;
  u32 NumberPayloadBuffer;
  u32 BufferVector;
  struct SBuffer
  {
    u32 m_Address, m_Size;
  };
  std::vector<SBuffer> InBuffer;
  std::vector<SBuffer> PayloadBuffer;
};

class IWII_IPC_HLE_Device
{
public:
  enum class DeviceType : u32
  {
    Static,  // Devices which appear in s_device_map.
    FileIO,  // FileIO devices which are created dynamically.
  };

  IWII_IPC_HLE_Device(u32 device_id, const std::string& device_name,
                      DeviceType type = DeviceType::Static);

  virtual ~IWII_IPC_HLE_Device() = default;
  // Release any resources which might interfere with savestating.
  virtual void PrepareForState(PointerWrap::Mode mode) {}
  virtual void DoState(PointerWrap& p);
  void DoStateShared(PointerWrap& p);

  const std::string& GetDeviceName() const { return m_name; }
  u32 GetDeviceID() const { return m_device_id; }
  virtual IPCCommandResult Open(u32 command_address, u32 mode);
  virtual IPCCommandResult Close(u32 command_address, bool force = false);
  virtual IPCCommandResult Seek(u32 command_address);
  virtual IPCCommandResult Read(u32 command_address);
  virtual IPCCommandResult Write(u32 command_address);
  virtual IPCCommandResult IOCtl(u32 command_address);
  virtual IPCCommandResult IOCtlV(u32 command_address);

  virtual void Update() {}
  virtual DeviceType GetDeviceType() const { return m_device_type; }
  virtual bool IsOpened() const { return m_is_active; }
  static IPCCommandResult GetDefaultReply();
  static IPCCommandResult GetNoReply();

protected:
  std::string m_name;
  // STATE_TO_SAVE
  u32 m_device_id;
  DeviceType m_device_type;
  bool m_is_active = false;

  // Write out the IPC struct from command_address to number_of_commands numbers
  // of 4 byte commands.
  void DumpCommands(u32 command_address, size_t number_of_commands = 8,
                    LogTypes::LOG_TYPE log_type = LogTypes::WII_IPC_HLE,
                    LogTypes::LOG_LEVELS verbosity = LogTypes::LDEBUG);

  void DumpAsync(u32 buffer_vector, u32 number_in_buffer, u32 number_io_buffer,
                 LogTypes::LOG_TYPE log_type = LogTypes::WII_IPC_HLE,
                 LogTypes::LOG_LEVELS verbosity = LogTypes::LDEBUG);
};
