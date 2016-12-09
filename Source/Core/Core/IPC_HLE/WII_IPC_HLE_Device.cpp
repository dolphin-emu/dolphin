// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

IOSResourceRequest::IOSResourceRequest(const u32 command_address) : address(command_address)
{
  command = static_cast<IPCCommandType>(Memory::Read_U32(command_address));  // 0x00
  return_value = Memory::Read_U32(command_address + 4);                      // 0x04
  fd = Memory::Read_U32(command_address + 8);                                // 0x08
}

void IOSResourceRequest::SetReturnValue(const s32 new_return_value)
{
  return_value = new_return_value;
  Memory::Write_U32(static_cast<u32>(return_value), address + 4);
}

// Write out the IPC struct from command_address to num_commands numbers of 4 byte commands.
void IOSResourceRequest::DumpCommands(size_t num_commands, LogTypes::LOG_TYPE log_type,
                                      LogTypes::LOG_LEVELS verbosity) const
{
  GENERIC_LOG(log_type, verbosity, "Dump of request 0x%08x (fd %u)", address, fd);
  for (size_t i = 0; i < num_commands; i++)
  {
    GENERIC_LOG(log_type, verbosity, "    %02zu: 0x%08x", i,
                Memory::Read_U32(static_cast<u32>(address + i * 4)));
  }
}

IOSResourceOpenRequest::IOSResourceOpenRequest(const u32 command_address)
    : IOSResourceRequest(command_address)
{
  path = Memory::GetString(Memory::Read_U32(address + 0xc));           // arg0
  flags = static_cast<IOSOpenMode>(Memory::Read_U32(address + 0x10));  // arg1
}

IOSResourceReadWriteRequest::IOSResourceReadWriteRequest(const u32 command_address)
    : IOSResourceRequest(command_address)
{
  data_addr = Memory::Read_U32(address + 0xc);  // arg0
  length = Memory::Read_U32(address + 0x10);    // arg1
}

IOSResourceSeekRequest::IOSResourceSeekRequest(const u32 command_address)
    : IOSResourceRequest(command_address)
{
  offset = Memory::Read_U32(address + 0xc);                        // arg0
  mode = static_cast<SeekMode>(Memory::Read_U32(address + 0x10));  // arg1
}

IOSResourceIOCtlRequest::IOSResourceIOCtlRequest(const u32 command_address)
    : IOSResourceRequest(command_address)
{
  request = Memory::Read_U32(address + 0x0c);   // arg0
  in_addr = Memory::Read_U32(address + 0x10);   // arg1
  in_size = Memory::Read_U32(address + 0x14);   // arg2
  out_addr = Memory::Read_U32(address + 0x18);  // arg3
  out_size = Memory::Read_U32(address + 0x1c);  // arg4
}

IOSResourceIOCtlVRequest::IOSResourceIOCtlVRequest(const u32 command_address)
    : IOSResourceRequest(command_address)
{
  request = Memory::Read_U32(address + 0x0c);               // arg0
  const u32 in_number = Memory::Read_U32(address + 0x10);   // arg1
  const u32 out_number = Memory::Read_U32(address + 0x14);  // arg2
  const u32 pairs_addr = Memory::Read_U32(address + 0x18);  // arg3 (address to io vectors)

  u32 offset = 0;
  for (size_t i = 0; i < (in_number + out_number); ++i)
  {
    IOVector vector;
    vector.addr = Memory::Read_U32(pairs_addr + offset);
    offset += 4;
    vector.size = Memory::Read_U32(pairs_addr + offset);
    offset += 4;
    if (i < in_number)
      in_vectors.emplace_back(vector);
    else
      io_vectors.emplace_back(vector);
  }
}

bool IOSResourceIOCtlVRequest::HasInVectorWithAddress(const u32 vector_address) const
{
  return std::find_if(in_vectors.begin(), in_vectors.end(), [&](const auto& in_vector) {
           return in_vector.addr == vector_address;
         }) != in_vectors.end();
}

void IOSResourceIOCtlRequest::Dump(const std::string& device_name, LogTypes::LOG_TYPE type,
                                   LogTypes::LOG_LEVELS verbosity) const
{
  Log(device_name, type, verbosity);
  GENERIC_LOG(type, verbosity, "  in (size 0x%x):", in_size);
  GENERIC_LOG(type, verbosity, "    %s",
              ArrayToString(Memory::GetPointer(in_addr), in_size).c_str());
  GENERIC_LOG(type, verbosity, "  out (size 0x%x):", out_size);
  GENERIC_LOG(type, verbosity, "    %s",
              ArrayToString(Memory::GetPointer(out_addr), out_size).c_str());
}

void IOSResourceIOCtlRequest::Log(const std::string& device_name, LogTypes::LOG_TYPE type,
                                  LogTypes::LOG_LEVELS verbosity) const
{
  GENERIC_LOG(type, verbosity, "%s (fd %u) - IOCtl %d (in 0x%08x %u, out 0x%08x %u)",
              device_name.c_str(), fd, request, in_addr, in_size, out_addr, out_size);
}

void IOSResourceIOCtlVRequest::Dump(const std::string& device_name, LogTypes::LOG_TYPE type,
                                    LogTypes::LOG_LEVELS verbosity) const
{
  GENERIC_LOG(type, verbosity, "======= Dump ======");
  GENERIC_LOG(type, verbosity, "%s (fd %u) - IOCtlV %d (%zu in, %zu io)", device_name.c_str(), fd,
              request, in_vectors.size(), io_vectors.size());

  size_t i = 0;
  for (const auto& vector : in_vectors)
  {
    GENERIC_LOG(type, verbosity, "  in[%zu] (0x%08x, size 0x%x):", i, vector.addr, vector.size);
    GENERIC_LOG(type, verbosity, "    %s",
                ArrayToString(Memory::GetPointer(vector.addr), vector.size).c_str());
    ++i;
  }
  for (const auto& vector : io_vectors)
  {
    GENERIC_LOG(type, verbosity, "  io[%zu] (0x%08x, size 0x%x)", i, vector.addr, vector.size);
    ++i;
  }
}

IWII_IPC_HLE_Device::IWII_IPC_HLE_Device(const u32 device_id, const std::string& device_name,
                                         const bool hardware)
    : m_name(device_name), m_device_id(device_id), m_is_hardware(hardware)
{
}

void IWII_IPC_HLE_Device::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_is_active);
}

void IWII_IPC_HLE_Device::DoStateShared(PointerWrap& p)
{
  p.Do(m_name);
  p.Do(m_device_id);
  p.Do(m_is_hardware);
  p.Do(m_is_active);
}

IOSReturnCode IWII_IPC_HLE_Device::Open(IOSResourceOpenRequest& request)
{
  m_is_active = true;
  return IPC_SUCCESS;
}

void IWII_IPC_HLE_Device::Close()
{
  m_is_active = false;
}

IPCCommandResult IWII_IPC_HLE_Device::Seek(IOSResourceSeekRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support Seek()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Read(IOSResourceReadWriteRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support Read()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Write(IOSResourceReadWriteRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support Write()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::IOCtl(IOSResourceIOCtlRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support IOCtl()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::IOCtlV(IOSResourceIOCtlVRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support IOCtlV()", m_name.c_str());
  return GetDefaultReply();
}

// Returns an IPCCommandResult for a reply that takes 250 us (arbitrarily chosen value)
IPCCommandResult IWII_IPC_HLE_Device::GetDefaultReply()
{
  return {true, SystemTimers::GetTicksPerSecond() / 4000};
}

// Returns an IPCCommandResult with no reply. Useful for async commands that will generate a reply
// later
IPCCommandResult IWII_IPC_HLE_Device::GetNoReply()
{
  return {false, 0};
}
