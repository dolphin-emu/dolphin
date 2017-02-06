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
#include "Core/IOS/IPC.h"

namespace IOS
{
namespace HLE
{
enum ReturnCode : s32
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
  USB_ECANCELED = -7022,     // USB OH0 insertion hook cancelled
};

struct Request
{
  u32 address = 0;
  IPCCommandType command = IPC_CMD_OPEN;
  u32 fd = 0;
  explicit Request(u32 address);
  virtual ~Request() = default;
};

enum OpenMode : s32
{
  IOS_OPEN_READ = 1,
  IOS_OPEN_WRITE = 2,
  IOS_OPEN_RW = (IOS_OPEN_READ | IOS_OPEN_WRITE)
};

struct OpenRequest final : Request
{
  std::string path;
  OpenMode flags = IOS_OPEN_READ;
  explicit OpenRequest(u32 address);
};

struct ReadWriteRequest final : Request
{
  u32 buffer = 0;
  u32 size = 0;
  explicit ReadWriteRequest(u32 address);
};

struct SeekRequest final : Request
{
  enum SeekMode
  {
    IOS_SEEK_SET = 0,
    IOS_SEEK_CUR = 1,
    IOS_SEEK_END = 2,
  };
  u32 offset = 0;
  SeekMode mode = IOS_SEEK_SET;
  explicit SeekRequest(u32 address);
};

struct IOCtlRequest final : Request
{
  u32 request = 0;
  u32 buffer_in = 0;
  u32 buffer_in_size = 0;
  // Contrary to the name, the output buffer can also be used for input.
  u32 buffer_out = 0;
  u32 buffer_out_size = 0;
  explicit IOCtlRequest(u32 address);
  void Log(const std::string& description, LogTypes::LOG_TYPE type = LogTypes::IOS,
           LogTypes::LOG_LEVELS level = LogTypes::LINFO) const;
  void Dump(const std::string& description, LogTypes::LOG_TYPE type = LogTypes::IOS,
            LogTypes::LOG_LEVELS level = LogTypes::LINFO) const;
  void DumpUnknown(const std::string& description, LogTypes::LOG_TYPE type = LogTypes::IOS,
                   LogTypes::LOG_LEVELS level = LogTypes::LERROR) const;
};

struct IOCtlVRequest final : Request
{
  struct IOVector
  {
    u32 address = 0;
    u32 size = 0;
  };
  u32 request = 0;
  // In vectors are *mostly* used for input buffers. Sometimes they are also used as
  // output buffers (notably in the network code).
  // IO vectors are *mostly* used for output buffers. However, as indicated in the name,
  // they're also used as input buffers.
  // So both of them are technically IO vectors. But we're keeping them separated because
  // merging them into a single std::vector would make using the first out vector more complicated.
  std::vector<IOVector> in_vectors;
  std::vector<IOVector> io_vectors;
  explicit IOCtlVRequest(u32 address);
  bool HasInputVectorWithAddress(u32 vector_address) const;
  bool HasNumberOfValidVectors(size_t in_count, size_t io_count) const;
  void Dump(const std::string& description, LogTypes::LOG_TYPE type = LogTypes::IOS,
            LogTypes::LOG_LEVELS level = LogTypes::LINFO) const;
  void DumpUnknown(const std::string& description, LogTypes::LOG_TYPE type = LogTypes::IOS,
                   LogTypes::LOG_LEVELS level = LogTypes::LERROR) const;
};

namespace Device
{
class Device
{
public:
  enum class DeviceType : u32
  {
    Static,  // Devices which appear in s_device_map.
    FileIO,  // FileIO devices which are created dynamically.
    OH0,     // OH0 child devices which are created dynamically.
  };

  Device(u32 device_id, const std::string& device_name, DeviceType type = DeviceType::Static);

  virtual ~Device() = default;
  // Release any resources which might interfere with savestating.
  virtual void PrepareForState(PointerWrap::Mode mode) {}
  virtual void DoState(PointerWrap& p);
  void DoStateShared(PointerWrap& p);

  const std::string& GetDeviceName() const { return m_name; }
  u32 GetDeviceID() const { return m_device_id; }
  // Replies to Open and Close requests are sent by the IPC request handler (HandleCommand),
  // not by the devices themselves.
  virtual ReturnCode Open(const OpenRequest& request);
  virtual void Close();
  virtual IPCCommandResult Seek(const SeekRequest& seek) { return Unsupported(seek); }
  virtual IPCCommandResult Read(const ReadWriteRequest& read) { return Unsupported(read); }
  virtual IPCCommandResult Write(const ReadWriteRequest& write) { return Unsupported(write); }
  virtual IPCCommandResult IOCtl(const IOCtlRequest& ioctl) { return Unsupported(ioctl); }
  virtual IPCCommandResult IOCtlV(const IOCtlVRequest& ioctlv) { return Unsupported(ioctlv); }
  virtual void Update() {}
  virtual void UpdateWantDeterminism(bool new_want_determinism) {}
  virtual DeviceType GetDeviceType() const { return m_device_type; }
  virtual bool IsOpened() const { return m_is_active; }
  static IPCCommandResult GetDefaultReply(s32 return_value);
  static IPCCommandResult GetNoReply();

protected:
  std::string m_name;
  // STATE_TO_SAVE
  u32 m_device_id;
  DeviceType m_device_type;
  bool m_is_active = false;

private:
  IPCCommandResult Unsupported(const Request& request);
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
