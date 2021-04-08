// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VertexLoaderBase.h"

#include <array>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"

#ifdef _M_X86_64
#include "VideoCommon/VertexLoaderX64.h"
#elif defined(_M_ARM_64)
#include "VideoCommon/VertexLoaderARM64.h"
#endif

VertexLoaderBase::VertexLoaderBase(const TVtxDesc& vtx_desc, const VAT& vtx_attr)
    : m_VtxDesc{vtx_desc}, m_vat{vtx_attr}
{
  SetVAT(vtx_attr);
}

void VertexLoaderBase::SetVAT(const VAT& vat)
{
  m_VtxAttr.PosElements = vat.g0.PosElements;
  m_VtxAttr.PosFormat = vat.g0.PosFormat;
  m_VtxAttr.PosFrac = vat.g0.PosFrac;
  m_VtxAttr.NormalElements = vat.g0.NormalElements;
  m_VtxAttr.NormalFormat = vat.g0.NormalFormat;
  m_VtxAttr.color[0].Elements = vat.g0.Color0Elements;
  m_VtxAttr.color[0].Comp = vat.g0.Color0Comp;
  m_VtxAttr.color[1].Elements = vat.g0.Color1Elements;
  m_VtxAttr.color[1].Comp = vat.g0.Color1Comp;
  m_VtxAttr.texCoord[0].Elements = vat.g0.Tex0CoordElements;
  m_VtxAttr.texCoord[0].Format = vat.g0.Tex0CoordFormat;
  m_VtxAttr.texCoord[0].Frac = vat.g0.Tex0Frac;
  m_VtxAttr.ByteDequant = vat.g0.ByteDequant;
  m_VtxAttr.NormalIndex3 = vat.g0.NormalIndex3;

  m_VtxAttr.texCoord[1].Elements = vat.g1.Tex1CoordElements;
  m_VtxAttr.texCoord[1].Format = vat.g1.Tex1CoordFormat;
  m_VtxAttr.texCoord[1].Frac = vat.g1.Tex1Frac;
  m_VtxAttr.texCoord[2].Elements = vat.g1.Tex2CoordElements;
  m_VtxAttr.texCoord[2].Format = vat.g1.Tex2CoordFormat;
  m_VtxAttr.texCoord[2].Frac = vat.g1.Tex2Frac;
  m_VtxAttr.texCoord[3].Elements = vat.g1.Tex3CoordElements;
  m_VtxAttr.texCoord[3].Format = vat.g1.Tex3CoordFormat;
  m_VtxAttr.texCoord[3].Frac = vat.g1.Tex3Frac;
  m_VtxAttr.texCoord[4].Elements = vat.g1.Tex4CoordElements;
  m_VtxAttr.texCoord[4].Format = vat.g1.Tex4CoordFormat;

  m_VtxAttr.texCoord[4].Frac = vat.g2.Tex4Frac;
  m_VtxAttr.texCoord[5].Elements = vat.g2.Tex5CoordElements;
  m_VtxAttr.texCoord[5].Format = vat.g2.Tex5CoordFormat;
  m_VtxAttr.texCoord[5].Frac = vat.g2.Tex5Frac;
  m_VtxAttr.texCoord[6].Elements = vat.g2.Tex6CoordElements;
  m_VtxAttr.texCoord[6].Format = vat.g2.Tex6CoordFormat;
  m_VtxAttr.texCoord[6].Frac = vat.g2.Tex6Frac;
  m_VtxAttr.texCoord[7].Elements = vat.g2.Tex7CoordElements;
  m_VtxAttr.texCoord[7].Format = vat.g2.Tex7CoordFormat;
  m_VtxAttr.texCoord[7].Frac = vat.g2.Tex7Frac;
};

std::string VertexLoaderBase::ToString() const
{
  std::string dest;
  dest.reserve(250);

  dest += GetName();
  dest += ": ";

  dest += fmt::format("{}b skin: {} P: {} {}-{} ", m_VertexSize, m_VtxDesc.low.PosMatIdx,
                      m_VtxAttr.PosElements, m_VtxDesc.low.Position, m_VtxAttr.PosFormat);

  if (m_VtxDesc.low.Normal != VertexComponentFormat::NotPresent)
  {
    dest += fmt::format("Nrm: {} {}-{} ", m_VtxAttr.NormalElements, m_VtxDesc.low.Normal,
                        m_VtxAttr.NormalFormat);
  }

  for (size_t i = 0; i < g_main_cp_state.vtx_desc.low.Color.Size(); i++)
  {
    if (g_main_cp_state.vtx_desc.low.Color[i] == VertexComponentFormat::NotPresent)
      continue;

    const auto& color = m_VtxAttr.color[i];
    dest += fmt::format("C{}: {} {}-{} ", i, color.Elements, g_main_cp_state.vtx_desc.low.Color[i],
                        color.Comp);
  }

  for (size_t i = 0; i < g_main_cp_state.vtx_desc.high.TexCoord.Size(); i++)
  {
    if (g_main_cp_state.vtx_desc.high.TexCoord[i] == VertexComponentFormat::NotPresent)
      continue;

    const auto& tex_coord = m_VtxAttr.texCoord[i];
    dest += fmt::format("T{}: {} {}-{} ", i, tex_coord.Elements,
                        g_main_cp_state.vtx_desc.high.TexCoord[i], tex_coord.Format);
  }
  dest += fmt::format(" - {} v", m_numLoadedVertices);
  return dest;
}

// a hacky implementation to compare two vertex loaders
class VertexLoaderTester : public VertexLoaderBase
{
public:
  VertexLoaderTester(std::unique_ptr<VertexLoaderBase> a_, std::unique_ptr<VertexLoaderBase> b_,
                     const TVtxDesc& vtx_desc, const VAT& vtx_attr)
      : VertexLoaderBase(vtx_desc, vtx_attr), a(std::move(a_)), b(std::move(b_))
  {
    m_initialized = a && b && a->IsInitialized() && b->IsInitialized();

    if (m_initialized)
    {
      m_initialized = a->m_VertexSize == b->m_VertexSize &&
                      a->m_native_components == b->m_native_components &&
                      a->m_native_vtx_decl.stride == b->m_native_vtx_decl.stride;

      if (m_initialized)
      {
        m_VertexSize = a->m_VertexSize;
        m_native_components = a->m_native_components;
        memcpy(&m_native_vtx_decl, &a->m_native_vtx_decl, sizeof(PortableVertexDeclaration));
      }
      else
      {
        ERROR_LOG_FMT(VIDEO, "Can't compare vertex loaders that expect different vertex formats!");
        ERROR_LOG_FMT(VIDEO, "a: m_VertexSize {}, m_native_components {:#010x}, stride {}",
                      a->m_VertexSize, a->m_native_components, a->m_native_vtx_decl.stride);
        ERROR_LOG_FMT(VIDEO, "b: m_VertexSize {}, m_native_components {:#010x}, stride {}",
                      b->m_VertexSize, b->m_native_components, b->m_native_vtx_decl.stride);
      }
    }
  }
  ~VertexLoaderTester() override {}
  int RunVertices(DataReader src, DataReader dst, int count) override
  {
    buffer_a.resize(count * a->m_native_vtx_decl.stride + 4);
    buffer_b.resize(count * b->m_native_vtx_decl.stride + 4);

    int count_a =
        a->RunVertices(src, DataReader(buffer_a.data(), buffer_a.data() + buffer_a.size()), count);
    int count_b =
        b->RunVertices(src, DataReader(buffer_b.data(), buffer_b.data() + buffer_b.size()), count);

    if (count_a != count_b)
    {
      ERROR_LOG_FMT(
          VIDEO,
          "The two vertex loaders have loaded a different amount of vertices (a: {}, b: {}).",
          count_a, count_b);
    }

    if (memcmp(buffer_a.data(), buffer_b.data(),
               std::min(count_a, count_b) * m_native_vtx_decl.stride))
    {
      ERROR_LOG_FMT(VIDEO,
                    "The two vertex loaders have loaded different data "
                    "(guru meditation {:#010x}, {:#010x}, {:#010x}, {:#010x}, {:#010x}).",
                    m_VtxDesc.low.Hex, m_VtxDesc.high.Hex, m_vat.g0.Hex, m_vat.g1.Hex,
                    m_vat.g2.Hex);
    }

    memcpy(dst.GetPointer(), buffer_a.data(), count_a * m_native_vtx_decl.stride);
    m_numLoadedVertices += count;
    return count_a;
  }
  std::string GetName() const override { return "CompareLoader"; }
  bool IsInitialized() override { return m_initialized; }

private:
  bool m_initialized;

  std::unique_ptr<VertexLoaderBase> a;
  std::unique_ptr<VertexLoaderBase> b;

  std::vector<u8> buffer_a;
  std::vector<u8> buffer_b;
};

std::unique_ptr<VertexLoaderBase> VertexLoaderBase::CreateVertexLoader(const TVtxDesc& vtx_desc,
                                                                       const VAT& vtx_attr)
{
  std::unique_ptr<VertexLoaderBase> loader;

  //#define COMPARE_VERTEXLOADERS

#if defined(COMPARE_VERTEXLOADERS) && defined(_M_X86_64)
  // first try: Any new VertexLoader vs the old one
  loader = std::make_unique<VertexLoaderTester>(
      std::make_unique<VertexLoader>(vtx_desc, vtx_attr),     // the software one
      std::make_unique<VertexLoaderX64>(vtx_desc, vtx_attr),  // the new one to compare
      vtx_desc, vtx_attr);
  if (loader->IsInitialized())
    return loader;
#elif defined(_M_X86_64)
  loader = std::make_unique<VertexLoaderX64>(vtx_desc, vtx_attr);
  if (loader->IsInitialized())
    return loader;
#elif defined(_M_ARM_64)
  loader = std::make_unique<VertexLoaderARM64>(vtx_desc, vtx_attr);
  if (loader->IsInitialized())
    return loader;
#endif

  // last try: The old VertexLoader
  loader = std::make_unique<VertexLoader>(vtx_desc, vtx_attr);
  if (loader->IsInitialized())
    return loader;

  PanicAlertFmt("No Vertex Loader found.");
  return nullptr;
}
