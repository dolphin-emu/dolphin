// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

SIOCtlVBuffer::SIOCtlVBuffer(const u32 address) : m_Address(address)
{
  // These are the Ioctlv parameters in the IOS communication. The BufferVector
  // is a memory address offset at where the in and out buffer addresses are
  // stored.
  Parameter = Memory::Read_U32(m_Address + 0x0C);            // command 3, arg0
  NumberInBuffer = Memory::Read_U32(m_Address + 0x10);       // 4, arg1
  NumberPayloadBuffer = Memory::Read_U32(m_Address + 0x14);  // 5, arg2
  BufferVector = Memory::Read_U32(m_Address + 0x18);         // 6, arg3

  // The start of the out buffer
  u32 BufferVectorOffset = BufferVector;

  // Write the address and size for all in messages
  for (u32 i = 0; i < NumberInBuffer; i++)
  {
    SBuffer Buffer;
    Buffer.m_Address = Memory::Read_U32(BufferVectorOffset);
    BufferVectorOffset += 4;
    Buffer.m_Size = Memory::Read_U32(BufferVectorOffset);
    BufferVectorOffset += 4;
    InBuffer.push_back(Buffer);
    DEBUG_LOG(WII_IPC_HLE, "SIOCtlVBuffer in%i: 0x%08x, 0x%x", i, Buffer.m_Address, Buffer.m_Size);
  }

  // Write the address and size for all out or in-out messages
  for (u32 i = 0; i < NumberPayloadBuffer; i++)
  {
    SBuffer Buffer;
    Buffer.m_Address = Memory::Read_U32(BufferVectorOffset);
    BufferVectorOffset += 4;
    Buffer.m_Size = Memory::Read_U32(BufferVectorOffset);
    BufferVectorOffset += 4;
    PayloadBuffer.push_back(Buffer);
    DEBUG_LOG(WII_IPC_HLE, "SIOCtlVBuffer io%i: 0x%08x, 0x%x", i, Buffer.m_Address, Buffer.m_Size);
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

IPCCommandResult IWII_IPC_HLE_Device::Open(u32 command_address, u32 mode)
{
  m_is_active = true;
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Close(u32 command_address, bool force)
{
  m_is_active = false;
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Seek(u32 command_address)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support Seek()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Read(u32 command_address)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support Read()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Write(u32 command_address)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support Write()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::IOCtl(u32 command_address)
{
  WARN_LOG(WII_IPC_HLE, "%s does not support IOCtl()", m_name.c_str());
  return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::IOCtlV(u32 command_address)
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

// Write out the IPC struct from command_address to num_commands numbers
// of 4 byte commands.
void IWII_IPC_HLE_Device::DumpCommands(u32 command_address, size_t num_commands,
                                       LogTypes::LOG_TYPE log_type, LogTypes::LOG_LEVELS verbosity)
{
  GENERIC_LOG(log_type, verbosity, "CommandDump of %s", GetDeviceName().c_str());
  for (u32 i = 0; i < num_commands; i++)
  {
    GENERIC_LOG(log_type, verbosity, "    Command%02i: 0x%08x", i,
                Memory::Read_U32(command_address + i * 4));
  }
}

void IWII_IPC_HLE_Device::DumpAsync(u32 buffer_vector, u32 number_in_buffer, u32 number_io_buffer,
                                    LogTypes::LOG_TYPE log_type, LogTypes::LOG_LEVELS verbosity)
{
  GENERIC_LOG(log_type, verbosity, "======= DumpAsync ======");

  u32 BufferOffset = buffer_vector;
  for (u32 i = 0; i < number_in_buffer; i++)
  {
    u32 InBuffer = Memory::Read_U32(BufferOffset);
    BufferOffset += 4;
    u32 InBufferSize = Memory::Read_U32(BufferOffset);
    BufferOffset += 4;

    GENERIC_LOG(log_type, LogTypes::LINFO, "%s - IOCtlV InBuffer[%i]:", GetDeviceName().c_str(), i);

    std::string Temp;
    for (u32 j = 0; j < InBufferSize; j++)
    {
      Temp += StringFromFormat("%02x ", Memory::Read_U8(InBuffer + j));
    }

    GENERIC_LOG(log_type, LogTypes::LDEBUG, "    Buffer: %s", Temp.c_str());
  }

  for (u32 i = 0; i < number_io_buffer; i++)
  {
    u32 OutBuffer = Memory::Read_U32(BufferOffset);
    BufferOffset += 4;
    u32 OutBufferSize = Memory::Read_U32(BufferOffset);
    BufferOffset += 4;

    GENERIC_LOG(log_type, LogTypes::LINFO, "%s - IOCtlV OutBuffer[%i]:", GetDeviceName().c_str(),
                i);
    GENERIC_LOG(log_type, LogTypes::LINFO, "    OutBuffer: 0x%08x (0x%x):", OutBuffer,
                OutBufferSize);

    if (verbosity >= LogTypes::LOG_LEVELS::LINFO)
      DumpCommands(OutBuffer, OutBufferSize, log_type, verbosity);
  }
}
