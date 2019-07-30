// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <vector>
#include <wrl/client.h>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#define CHECK(cond, Message, ...)                                                                  \
  if (!(cond))                                                                                     \
  {                                                                                                \
    PanicAlert("%s failed in %s at line %d: " Message, __func__, __FILE__, __LINE__, __VA_ARGS__); \
  }

namespace DX11
{
using Microsoft::WRL::ComPtr;
class SwapChain;

namespace D3D
{
extern ComPtr<IDXGIFactory> dxgi_factory;
extern ComPtr<ID3D11Device> device;
extern ComPtr<ID3D11Device1> device1;
extern ComPtr<ID3D11DeviceContext> context;
extern D3D_FEATURE_LEVEL feature_level;

bool Create(u32 adapter_index, bool enable_debug_layer);
void Destroy();

// Returns a list of supported AA modes for the current device.
std::vector<u32> GetAAModes(u32 adapter_index);

// Checks for support of the given texture format.
bool SupportsTextureFormat(DXGI_FORMAT format);

}  // namespace D3D

}  // namespace DX11
