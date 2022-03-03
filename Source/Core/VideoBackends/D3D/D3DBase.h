// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <fmt/format.h>
#include <vector>
#include <wrl/client.h>

#include "Common/CommonTypes.h"
#include "Common/HRWrap.h"

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

// Checks for logic op support.
bool SupportsLogicOp(u32 adapter_index);

}  // namespace D3D

// Wrapper for HRESULT to be used with fmt.  Note that we can't create a fmt::formatter directly
// for HRESULT as HRESULT is simply a typedef on long and not a distinct type.
// Unlike the version in Common, this variant also knows to call GetDeviceRemovedReason if needed.
struct DX11HRWrap
{
  constexpr explicit DX11HRWrap(HRESULT hr) : m_hr(hr) {}
  const HRESULT m_hr;
};

}  // namespace DX11

template <>
struct fmt::formatter<DX11::DX11HRWrap>
{
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const DX11::DX11HRWrap& hr, FormatContext& ctx) const
  {
    if (hr.m_hr == DXGI_ERROR_DEVICE_REMOVED && DX11::D3D::device != nullptr)
    {
      return fmt::format_to(ctx.out(), "{}\nDevice removal reason: {}", Common::HRWrap(hr.m_hr),
                            Common::HRWrap(DX11::D3D::device->GetDeviceRemovedReason()));
    }
    else
    {
      return fmt::format_to(ctx.out(), "{}", Common::HRWrap(hr.m_hr));
    }
  }
};
