// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Device.h"

#include <algorithm>
#include <map>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/IOS.h"

namespace IOS
{
namespace HLE
{
Request::Request(const u32 address_) : address(address_)
{
  command = static_cast<IPCCommandType>(Memory::Read_U32(address));
  fd = Memory::Read_U32(address + 8);
}

OpenRequest::OpenRequest(const u32 address_) : Request(address_)
{
  path = Memory::GetString(Memory::Read_U32(address + 0xc));
  flags = static_cast<OpenMode>(Memory::Read_U32(address + 0x10));
  const Kernel* ios = GetIOS();
  if (ios)
  {
    uid = ios->GetUidForPPC();
    gid = ios->GetGidForPPC();
  }
}

ReadWriteRequest::ReadWriteRequest(const u32 address_) : Request(address_)
{
  buffer = Memory::Read_U32(address + 0xc);
  size = Memory::Read_U32(address + 0x10);
}

SeekRequest::SeekRequest(const u32 address_) : Request(address_)
{
  offset = Memory::Read_U32(address + 0xc);
  mode = static_cast<SeekMode>(Memory::Read_U32(address + 0x10));
}

IOCtlRequest::IOCtlRequest(const u32 address_) : Request(address_)
{
  request = Memory::Read_U32(address + 0x0c);
  buffer_in = Memory::Read_U32(address + 0x10);
  buffer_in_size = Memory::Read_U32(address + 0x14);
  buffer_out = Memory::Read_U32(address + 0x18);
  buffer_out_size = Memory::Read_U32(address + 0x1c);
}

IOCtlVRequest::IOCtlVRequest(const u32 address_) : Request(address_)
{
  request = Memory::Read_U32(address + 0x0c);
  const u32 in_number = Memory::Read_U32(address + 0x10);
  const u32 out_number = Memory::Read_U32(address + 0x14);
  const u32 vectors_base = Memory::Read_U32(address + 0x18);  // address to vectors

  u32 offset = 0;
  for (size_t i = 0; i < (in_number + out_number); ++i)
  {
    IOVector vector;
    vector.address = Memory::Read_U32(vectors_base + offset);
    vector.size = Memory::Read_U32(vectors_base + offset + 4);
    offset += 8;
    if (i < in_number)
      in_vectors.emplace_back(vector);
    else
      io_vectors.emplace_back(vector);
  }
}

bool IOCtlVRequest::HasNumberOfValidVectors(const size_t in_count, const size_t io_count) const
{
  if (in_vectors.size() != in_count || io_vectors.size() != io_count)
    return false;

  auto IsValidVector = [](const auto& vector) { return vector.size == 0 || vector.address != 0; };
  return std::all_of(in_vectors.begin(), in_vectors.end(), IsValidVector) &&
         std::all_of(io_vectors.begin(), io_vectors.end(), IsValidVector);
}

void IOCtlRequest::Log(const std::string& device_name, LogTypes::LOG_TYPE type,
                       LogTypes::LOG_LEVELS verbosity) const
{
  GENERIC_LOG(type, verbosity, "%s (fd %u) - IOCtl 0x%x (in_size=0x%x, out_size=0x%x)",
              device_name.c_str(), fd, request, buffer_in_size, buffer_out_size);
}

void IOCtlRequest::Dump(const std::string& description, LogTypes::LOG_TYPE type,
                        LogTypes::LOG_LEVELS level) const
{
  Log("===== " + description, type, level);
  GENERIC_LOG(type, level, "In buffer\n%s",
              HexDump(Memory::GetPointer(buffer_in), buffer_in_size).c_str());
  GENERIC_LOG(type, level, "Out buffer\n%s",
              HexDump(Memory::GetPointer(buffer_out), buffer_out_size).c_str());
}

void IOCtlRequest::DumpUnknown(const std::string& description, LogTypes::LOG_TYPE type,
                               LogTypes::LOG_LEVELS level) const
{
  Dump("Unknown IOCtl - " + description, type, level);
}

void IOCtlVRequest::Dump(const std::string& description, LogTypes::LOG_TYPE type,
                         LogTypes::LOG_LEVELS level) const
{
  GENERIC_LOG(type, level, "===== %s (fd %u) - IOCtlV 0x%x (%zu in, %zu io)", description.c_str(),
              fd, request, in_vectors.size(), io_vectors.size());

  size_t i = 0;
  for (const auto& vector : in_vectors)
    GENERIC_LOG(type, level, "in[%zu] (size=0x%x):\n%s", i++, vector.size,
                HexDump(Memory::GetPointer(vector.address), vector.size).c_str());

  i = 0;
  for (const auto& vector : io_vectors)
    GENERIC_LOG(type, level, "io[%zu] (size=0x%x)", i++, vector.size);
}

void IOCtlVRequest::DumpUnknown(const std::string& description, LogTypes::LOG_TYPE type,
                                LogTypes::LOG_LEVELS level) const
{
  Dump("Unknown IOCtlV - " + description, type, level);
}

namespace Device
{
Device::Device(Kernel& ios, const std::string& device_name, const DeviceType type)
    : m_ios(ios), m_name(device_name), m_device_type(type)
{
}

void Device::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_is_active);
}

void Device::DoStateShared(PointerWrap& p)
{
  p.Do(m_name);
  p.Do(m_device_type);
  p.Do(m_is_active);
}

ReturnCode Device::Open(const OpenRequest& request)
{
  m_is_active = true;
  return IPC_SUCCESS;
}

ReturnCode Device::Close(u32 fd)
{
  m_is_active = false;
  return IPC_SUCCESS;
}

IPCCommandResult Device::Unsupported(const Request& request)
{
  static std::map<IPCCommandType, std::string> names = {{{IPC_CMD_READ, "Read"},
                                                         {IPC_CMD_WRITE, "Write"},
                                                         {IPC_CMD_SEEK, "Seek"},
                                                         {IPC_CMD_IOCTL, "IOCtl"},
                                                         {IPC_CMD_IOCTLV, "IOCtlV"}}};
  WARN_LOG(IOS, "%s does not support %s()", m_name.c_str(), names[request.command].c_str());
  return GetDefaultReply(IPC_EINVAL);
}

// Returns an IPCCommandResult for a reply that takes 250 us (arbitrarily chosen value)
IPCCommandResult Device::GetDefaultReply(const s32 return_value)
{
  return {return_value, true, SystemTimers::GetTicksPerSecond() / 4000};
}

// Returns an IPCCommandResult with no reply. Useful for async commands that will generate a reply
// later. This takes no return value because it won't be used.
IPCCommandResult Device::GetNoReply()
{
  return {IPC_SUCCESS, false, 0};
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
