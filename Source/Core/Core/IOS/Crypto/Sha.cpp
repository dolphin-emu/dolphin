// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Crypto/Sha.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include <mbedtls/sha1.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE
{
ShaDevice::ShaDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

std::optional<IPCReply> ShaDevice::Open(const OpenRequest& request)
{
  return Device::Open(request);
}

static void ConvertContext(const ShaDevice::ShaContext& src, mbedtls_sha1_context* dest)
{
  std::copy(std::begin(src.length), std::end(src.length), std::begin(dest->total));
  std::copy(std::begin(src.states), std::end(src.states), std::begin(dest->state));
}

static void ConvertContext(const mbedtls_sha1_context& src, ShaDevice::ShaContext* dest)
{
  std::copy(std::begin(src.total), std::end(src.total), std::begin(dest->length));
  std::copy(std::begin(src.state), std::end(src.state), std::begin(dest->states));
}

HLE::ReturnCode ShaDevice::ProcessShaCommand(ShaIoctlv command, const IOCtlVRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  auto ret = 0;
  std::array<u8, 20> output_hash{};
  mbedtls_sha1_context context;
  ShaDevice::ShaContext engine_context;
  memory.CopyFromEmu(&engine_context, request.io_vectors[0].address, sizeof(ShaDevice::ShaContext));
  ConvertContext(engine_context, &context);

  // reset the context
  if (command == ShaIoctlv::InitState)
  {
    ret = mbedtls_sha1_starts_ret(&context);
  }
  else
  {
    std::vector<u8> input_data(request.in_vectors[0].size);
    memory.CopyFromEmu(input_data.data(), request.in_vectors[0].address, input_data.size());
    ret = mbedtls_sha1_update_ret(&context, input_data.data(), input_data.size());
    if (!ret && command == ShaIoctlv::FinalizeState)
    {
      ret = mbedtls_sha1_finish_ret(&context, output_hash.data());
    }
  }

  ConvertContext(context, &engine_context);
  memory.CopyToEmu(request.io_vectors[0].address, &engine_context, sizeof(ShaDevice::ShaContext));
  if (!ret && command == ShaIoctlv::FinalizeState)
    memory.CopyToEmu(request.io_vectors[1].address, output_hash.data(), output_hash.size());

  mbedtls_sha1_free(&context);
  return ret ? HLE::ReturnCode::IPC_EACCES : HLE::ReturnCode::IPC_SUCCESS;
}

std::optional<IPCReply> ShaDevice::IOCtlV(const IOCtlVRequest& request)
{
  HLE::ReturnCode return_code = IPC_EINVAL;
  ShaIoctlv command = static_cast<ShaIoctlv>(request.request);

  switch (command)
  {
  case ShaIoctlv::InitState:
  case ShaIoctlv::ContributeState:
  case ShaIoctlv::FinalizeState:
    if (!request.HasNumberOfValidVectors(1, 2))
      break;

    return_code = ProcessShaCommand(command, request);
    break;

  case ShaIoctlv::ShaCommandUnknown:
    break;
  }

  return IPCReply(return_code);
}

}  // namespace IOS::HLE
