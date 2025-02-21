// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Crypto/AesDevice.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE
{
AesDevice::AesDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

std::optional<IPCReply> AesDevice::Open(const OpenRequest& request)
{
  return Device::Open(request);
}

std::optional<IPCReply> AesDevice::IOCtlV(const IOCtlVRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  HLE::ReturnCode return_code = IPC_EINVAL;
  AesIoctlv command = static_cast<AesIoctlv>(request.request);

  switch (command)
  {
  case AesIoctlv::Copy:
  {
    if (!request.HasNumberOfValidVectors(1, 1))
      break;

    std::vector<u8> input = std::vector<u8>(request.in_vectors[0].size);
    memory.CopyFromEmu(input.data(), request.in_vectors[0].address, input.size());
    memory.CopyToEmu(request.io_vectors[0].address, input.data(), input.size());
    return_code = ReturnCode::IPC_SUCCESS;
    break;
  }
  case AesIoctlv::Decrypt:
  case AesIoctlv::Encrypt:
  {
    if (!request.HasNumberOfValidVectors(2, 2))
      break;

    if (request.in_vectors[1].size != 0x10 || (request.in_vectors[1].address & 3) != 0 ||
        request.io_vectors[1].size != 0x10 || (request.io_vectors[1].address & 3) != 0)
    {
      break;
    }

    std::vector<u8> input = std::vector<u8>(request.in_vectors[0].size);
    std::vector<u8> output = std::vector<u8>(request.io_vectors[0].size);
    std::array<u8, 0x10> key = {0};
    std::array<u8, 0x10> iv = {0};
    memory.CopyFromEmu(input.data(), request.in_vectors[0].address, input.size());
    memory.CopyFromEmu(key.data(), request.in_vectors[1].address, key.size());
    memory.CopyFromEmu(iv.data(), request.io_vectors[1].address, iv.size());

    auto context = command == AesIoctlv::Encrypt ? Common::AES::CreateContextEncrypt(key.data()) :
                                                   Common::AES::CreateContextDecrypt(key.data());

    context->Crypt(iv.data(), iv.data(), input.data(), output.data(),
                   std::min(output.size(), input.size()));

    memory.CopyToEmu(request.io_vectors[0].address, output.data(), output.size());
    memory.CopyToEmu(request.io_vectors[1].address, iv.data(), iv.size());
    return_code = ReturnCode::IPC_SUCCESS;
    break;
  }
  }

  return IPCReply(return_code);
}

}  // namespace IOS::HLE
