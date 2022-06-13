// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLObjectCache.h"

#include "VideoCommon/VideoConfig.h"

MRCOwned<id<MTLDevice>> Metal::g_device;
MRCOwned<id<MTLCommandQueue>> Metal::g_queue;
std::unique_ptr<Metal::ObjectCache> Metal::g_object_cache;

static void SetupDepthStencil(
    MRCOwned<id<MTLDepthStencilState>> (&dss)[Metal::DepthStencilSelector::N_VALUES]);

Metal::ObjectCache::ObjectCache()
{
  SetupDepthStencil(m_dss);
}

Metal::ObjectCache::~ObjectCache()
{
}

void Metal::ObjectCache::Initialize(MRCOwned<id<MTLDevice>> device)
{
  g_device = std::move(device);
  g_queue = MRCTransfer([g_device newCommandQueue]);
  g_object_cache = std::unique_ptr<ObjectCache>(new ObjectCache);
}

void Metal::ObjectCache::Shutdown()
{
  g_object_cache.reset();
  g_queue = nullptr;
  g_device = nullptr;
}

// MARK: Depth Stencil State

// clang-format off

static MTLCompareFunction Convert(CompareMode mode)
{
  const bool invert_depth = !g_Config.backend_info.bSupportsReversedDepthRange;
  switch (mode)
  {
  case CompareMode::Never:   return MTLCompareFunctionNever;
  case CompareMode::Less:    return invert_depth ? MTLCompareFunctionGreater
                                                 : MTLCompareFunctionLess;
  case CompareMode::Equal:   return MTLCompareFunctionEqual;
  case CompareMode::LEqual:  return invert_depth ? MTLCompareFunctionGreaterEqual
                                                 : MTLCompareFunctionLessEqual;
  case CompareMode::Greater: return invert_depth ? MTLCompareFunctionLess
                                                 : MTLCompareFunctionGreater;
  case CompareMode::NEqual:  return MTLCompareFunctionNotEqual;
  case CompareMode::GEqual:  return invert_depth ? MTLCompareFunctionLessEqual
                                                 : MTLCompareFunctionGreaterEqual;
  case CompareMode::Always:  return MTLCompareFunctionAlways;
  }
}

static const char* to_string(MTLCompareFunction compare)
{
  switch (compare)
  {
  case MTLCompareFunctionNever:        return "Never";
  case MTLCompareFunctionGreater:      return "Greater";
  case MTLCompareFunctionEqual:        return "Equal";
  case MTLCompareFunctionGreaterEqual: return "GEqual";
  case MTLCompareFunctionLess:         return "Less";
  case MTLCompareFunctionNotEqual:     return "NEqual";
  case MTLCompareFunctionLessEqual:    return "LEqual";
  case MTLCompareFunctionAlways:       return "Always";
  }
}

// clang-format on

static void SetupDepthStencil(
    MRCOwned<id<MTLDepthStencilState>> (&dss)[Metal::DepthStencilSelector::N_VALUES])
{
  auto desc = MRCTransfer([MTLDepthStencilDescriptor new]);
  Metal::DepthStencilSelector sel;
  for (size_t i = 0; i < std::size(dss); ++i)
  {
    sel.value = i;
    MTLCompareFunction mcompare = Convert(sel.CompareMode());
    [desc setDepthWriteEnabled:sel.UpdateEnable()];
    [desc setDepthCompareFunction:mcompare];
    [desc setLabel:[NSString stringWithFormat:@"DSS %s%s", to_string(mcompare),
                                              sel.UpdateEnable() ? "+Write" : ""]];
    dss[i] = MRCTransfer([Metal::g_device newDepthStencilStateWithDescriptor:desc]);
  }
}

// MARK: Samplers

// clang-format off

static MTLSamplerMinMagFilter ConvertMinMag(FilterMode filter)
{
  switch (filter)
  {
  case FilterMode::Linear: return MTLSamplerMinMagFilterLinear;
  case FilterMode::Near:   return MTLSamplerMinMagFilterNearest;
  }
}

static MTLSamplerMipFilter ConvertMip(FilterMode filter)
{
  switch (filter)
  {
  case FilterMode::Linear: return MTLSamplerMipFilterLinear;
  case FilterMode::Near:   return MTLSamplerMipFilterNearest;
  }
}

static MTLSamplerAddressMode Convert(WrapMode wrap)
{
  switch (wrap)
  {
  case WrapMode::Clamp:  return MTLSamplerAddressModeClampToEdge;
  case WrapMode::Mirror: return MTLSamplerAddressModeMirrorRepeat;
  case WrapMode::Repeat: return MTLSamplerAddressModeRepeat;
  }
}

static const char* to_string(FilterMode filter)
{
  switch (filter)
  {
  case FilterMode::Linear: return "Ln";
  case FilterMode::Near:   return "Pt";
  }
}
static const char* to_string(WrapMode wrap)
{
  switch (wrap)
  {
  case WrapMode::Clamp:  return "C";
  case WrapMode::Mirror: return "M";
  case WrapMode::Repeat: return "R";
  }
}

// clang-format on

MRCOwned<id<MTLSamplerState>> Metal::ObjectCache::CreateSampler(SamplerSelector sel)
{
  @autoreleasepool
  {
    auto desc = MRCTransfer([MTLSamplerDescriptor new]);
    [desc setMinFilter:ConvertMinMag(sel.MinFilter())];
    [desc setMagFilter:ConvertMinMag(sel.MagFilter())];
    [desc setMipFilter:ConvertMip(sel.MipFilter())];
    [desc setSAddressMode:Convert(sel.WrapU())];
    [desc setTAddressMode:Convert(sel.WrapV())];
    [desc setMaxAnisotropy:1 << (sel.AnisotropicFiltering() ? g_ActiveConfig.iMaxAnisotropy : 0)];
    [desc setLabel:MRCTransfer([[NSString alloc]
                       initWithFormat:@"%s%s%s %s%s%s", to_string(sel.MinFilter()),
                                      to_string(sel.MagFilter()), to_string(sel.MipFilter()),
                                      to_string(sel.WrapU()), to_string(sel.WrapV()),
                                      sel.AnisotropicFiltering() ? "(AF)" : ""])];
    return MRCTransfer([Metal::g_device newSamplerStateWithDescriptor:desc]);
  }
}

void Metal::ObjectCache::ReloadSamplers()
{
  for (auto& sampler : m_samplers)
    sampler = nullptr;
}
