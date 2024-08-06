// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Device.h"

#include <algorithm>
#include <map>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/IOS.h"
#include "Core/System.h"

namespace IOS::HLE
{
Request::Request(const Core::System& system, const u32 address_) : address(address_)
{
  const auto& memory = system.GetMemory();
  command = static_cast<IPCCommandType>(memory.Read_U32(address));
  fd = memory.Read_U32(address + 8);
}

OpenRequest::OpenRequest(const Core::System& system, const u32 address_) : Request(system, address_)
{
  const auto& memory = system.GetMemory();
  path = memory.GetString(memory.Read_U32(address + 0xc));
  flags = static_cast<OpenMode>(memory.Read_U32(address + 0x10));
  const EmulationKernel* ios = system.GetIOS();
  if (ios)
  {
    uid = ios->GetUidForPPC();
    gid = ios->GetGidForPPC();
  }
}

ReadWriteRequest::ReadWriteRequest(const Core::System& system, const u32 address_)
    : Request(system, address_)
{
  const auto& memory = system.GetMemory();
  buffer = memory.Read_U32(address + 0xc);
  size = memory.Read_U32(address + 0x10);
}

SeekRequest::SeekRequest(const Core::System& system, const u32 address_) : Request(system, address_)
{
  const auto& memory = system.GetMemory();
  offset = memory.Read_U32(address + 0xc);
  mode = static_cast<SeekMode>(memory.Read_U32(address + 0x10));
}

IOCtlRequest::IOCtlRequest(const Core::System& system, const u32 address_) : Request(system, address_)
{
  const auto& memory = system.GetMemory();
  request = memory.Read_U32(address + 0x0c);
  buffer_in = memory.Read_U32(address + 0x10);
  buffer_in_size = memory.Read_U32(address + 0x14);
  buffer_out = memory.Read_U32(address + 0x18);
  buffer_out_size = memory.Read_U32(address + 0x1c);
}

IOCtlVRequest::IOCtlVRequest(const Core::System& system, const u32 address_) : Request(system, address_)
{
  const auto& memory = system.GetMemory();
  request = memory.Read_U32(address + 0x0c);
  const u32 in_number = memory.Read_U32(address + 0x10);
  const u32 out_number = memory.Read_U32(address + 0x14);
  const u32 vectors_base = memory.Read_U32(address + 0x18);  // address to vectors

  u32 offset = 0;
  for (size_t i = 0; i < (in_number + out_number); ++i)
  {
    IOVector vector;
    vector.address = memory.Read_U32(vectors_base + offset);
    vector.size = memory.Read_U32(vectors_base + offset + 4);
    offset += 8;
    if (i < in_number)
      in_vectors.emplace_back(vector);
    else
      io_vectors.emplace_back(vector);
  }
}

const IOCtlVRequest::IOVector* IOCtlVRequest::GetVector(const size_t index) const
{
  if (index >= in_vectors.size() + io_vectors.size())
    return nullptr;
  if (index < in_vectors.size())
    return &in_vectors[index];
  return &io_vectors[index - in_vectors.size()];
}

bool IOCtlVRequest::HasNumberOfValidVectors(const size_t in_count, const size_t io_count) const
{
  if (in_vectors.size() != in_count || io_vectors.size() != io_count)
    return false;

  auto IsValidVector = [](const auto& vector) { return vector.size == 0 || vector.address != 0; };
  return std::ranges::all_of(in_vectors, IsValidVector) &&
         std::ranges::all_of(io_vectors, IsValidVector);
}

void IOCtlRequest::Log(const std::string_view device_name, const Common::Log::LogType type,
                       const Common::Log::LogLevel verbosity) const
{
  GENERIC_LOG_FMT(type, verbosity, "{} (fd {}) - IOCtl {:#x} (in_size={:#x}, out_size={:#x})",
                  device_name, fd, request, buffer_in_size, buffer_out_size);
}

void IOCtlRequest::Dump(const Core::System& system, const std::string& description,
                        const Common::Log::LogType type, const Common::Log::LogLevel level) const
{
  const auto& memory = system.GetMemory();

  Log("===== " + description, type, level);
  GENERIC_LOG_FMT(type, level, "In buffer\n{}",
                  HexDump(memory.GetPointerForRange(buffer_in, buffer_in_size), buffer_in_size));
  GENERIC_LOG_FMT(type, level, "Out buffer\n{}",
                  HexDump(memory.GetPointerForRange(buffer_out, buffer_out_size), buffer_out_size));
}

void IOCtlRequest::DumpUnknown(const Core::System& system, const std::string& description,
                               const Common::Log::LogType type, const Common::Log::LogLevel level) const
{
  Dump(system, "Unknown IOCtl - " + description, type, level);
}

void IOCtlVRequest::Dump(const Core::System& system, const std::string_view description,
                         const Common::Log::LogType type, const Common::Log::LogLevel level) const
{
  const auto& memory = system.GetMemory();

  GENERIC_LOG_FMT(type, level, "===== {} (fd {}) - IOCtlV {:#x} ({} in, {} io)", description, fd,
                  request, in_vectors.size(), io_vectors.size());

  size_t i = 0;
  for (const auto& [address, size] : in_vectors)
  {
    GENERIC_LOG_FMT(type, level, "in[{}] (size={:#x}):\n{}", i++, size,
                    HexDump(memory.GetPointerForRange(address, size), size));
  }

  i = 0;
  for (const auto& [_address, size] : io_vectors)
    GENERIC_LOG_FMT(type, level, "io[{}] (size={:#x})", i++, size);
}

void IOCtlVRequest::DumpUnknown(const Core::System& system, const std::string& description,
                                const Common::Log::LogType type, const Common::Log::LogLevel level) const
{
  Dump(system, "Unknown IOCtlV - " + description, type, level);
}

Device::Device(Kernel& ios, const std::string& device_name, const DeviceType type)
    : m_ios(ios), m_name(device_name), m_device_type(type)
{
}

void Device::DoState(PointerWrap& p)
{
  p.Do(m_name);
  p.Do(m_device_type);
  p.Do(m_is_active);
}

std::optional<IPCReply> Device::Open(const OpenRequest& request)
{
  m_is_active = true;
  return IPCReply{IPC_SUCCESS};
}

std::optional<IPCReply> Device::Close(u32 fd)
{
  m_is_active = false;
  return IPCReply{IPC_SUCCESS};
}

std::optional<IPCReply> Device::Unsupported(const Request& request) const
{
  static const std::map<IPCCommandType, std::string_view> names{{
      {IPC_CMD_READ, "Read"},
      {IPC_CMD_WRITE, "Write"},
      {IPC_CMD_SEEK, "Seek"},
      {IPC_CMD_IOCTL, "IOCtl"},
      {IPC_CMD_IOCTLV, "IOCtlV"},
  }};

  WARN_LOG_FMT(IOS, "{} does not support {}()", m_name, names.at(request.command));
  return IPCReply{IPC_EINVAL};
}
}  // namespace IOS::HLE
