// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Crypto/Sha.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <vector>

#include <wolfssl/wolfcrypt/sha.h>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
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

static void ConvertContext(const ShaDevice::ShaContext& src, wc_Sha* dest)
{
  std::ranges::copy(src.states, std::begin(dest->digest));
  dest->loLen = src.length[0];
  dest->hiLen = src.length[1];
  dest->buffLen = 0;
}

static void ConvertContext(const wc_Sha& src, ShaDevice::ShaContext* dest)
{
  std::ranges::copy(src.digest, std::begin(dest->states));
  dest->length = {src.loLen, src.hiLen};
}

HLE::ReturnCode ShaDevice::ProcessShaCommand(ShaIoctlv command, const IOCtlVRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  std::array<u8, 20> output_hash{};
  wc_Sha context;
  int ret = wc_InitSha(&context);
  if (ret != 0)
    return HLE::ReturnCode::IPC_EACCES;

  ShaDevice::ShaContext engine_context;
  memory.CopyFromEmu(&engine_context, request.io_vectors[0].address, sizeof(ShaDevice::ShaContext));

  if (command != ShaIoctlv::InitState)
  {
    ConvertContext(engine_context, &context);

    std::vector<u8> input_data(request.in_vectors[0].size);
    memory.CopyFromEmu(input_data.data(), request.in_vectors[0].address, input_data.size());
    ret = wc_ShaUpdate(&context, input_data.data(), static_cast<word32>(input_data.size()));
    if (!ret && command == ShaIoctlv::FinalizeState)
    {
      // wc_ShaFinal resets the context: write back the final state and the length of the data.
      const word32 lo_len = context.loLen;
      const word32 hi_len = context.hiLen;
      ret = wc_ShaFinal(&context, output_hash.data());
      if (!ret)
      {
        for (size_t i = 0; i < std::size(context.digest); ++i)
          context.digest[i] = Common::swap32(&output_hash[i * 4]);
        context.loLen = lo_len;
        context.hiLen = hi_len;
      }
    }
  }

  ConvertContext(context, &engine_context);
  memory.CopyToEmu(request.io_vectors[0].address, &engine_context, sizeof(ShaDevice::ShaContext));
  if (!ret && command == ShaIoctlv::FinalizeState)
    memory.CopyToEmu(request.io_vectors[1].address, output_hash.data(), output_hash.size());

  wc_ShaFree(&context);
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
