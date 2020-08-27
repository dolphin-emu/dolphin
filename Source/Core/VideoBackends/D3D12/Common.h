// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "Common/MsgHandler.h"
#include "VideoBackends/D3DCommon/Common.h"

#define CHECK(cond, Message, ...)                                                                  \
  if (!(cond))                                                                                     \
  {                                                                                                \
    PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__,              \
               __VA_ARGS__);                                                                       \
  }

namespace DX12
{
using Microsoft::WRL::ComPtr;

static void ResourceBarrier(ID3D12GraphicsCommandList* cmdlist, ID3D12Resource* resource,
                            D3D12_RESOURCE_STATES from_state, D3D12_RESOURCE_STATES to_state)
{
  const D3D12_RESOURCE_BARRIER barrier = {
      D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      D3D12_RESOURCE_BARRIER_FLAG_NONE,
      {{resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from_state, to_state}}};
  cmdlist->ResourceBarrier(1, &barrier);
}
}  // namespace DX12
