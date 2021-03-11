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
                    m_VtxDesc.low.Hex, m_VtxDesc.high.Hex, m_VtxAttr.g0.Hex, m_VtxAttr.g1.Hex,
                    m_VtxAttr.g2.Hex);
    }

    memcpy(dst.GetPointer(), buffer_a.data(), count_a * m_native_vtx_decl.stride);
    m_numLoadedVertices += count;
    return count_a;
  }
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
