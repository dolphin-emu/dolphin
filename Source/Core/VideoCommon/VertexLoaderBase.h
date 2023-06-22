// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/NativeVertexFormat.h"

class VertexLoaderUID
{
  std::array<u32, 5> vid{};
  size_t hash = 0;

public:
  VertexLoaderUID() {}
  VertexLoaderUID(const TVtxDesc& vtx_desc, const VAT& vat)
  {
    vid[0] = vtx_desc.low.Hex;
    vid[1] = vtx_desc.high.Hex;
    vid[2] = vat.g0.Hex;
    vid[3] = vat.g1.Hex;
    vid[4] = vat.g2.Hex;
    hash = CalculateHash();
  }

  bool operator==(const VertexLoaderUID& rh) const { return vid == rh.vid; }
  size_t GetHash() const { return hash; }

private:
  size_t CalculateHash() const
  {
    size_t h = SIZE_MAX;

    for (auto word : vid)
    {
      h = h * 137 + word;
    }

    return h;
  }
};

namespace std
{
template <>
struct hash<VertexLoaderUID>
{
  size_t operator()(const VertexLoaderUID& uid) const { return uid.GetHash(); }
};
}  // namespace std

class VertexLoaderBase
{
public:
  static u32 GetVertexSize(const TVtxDesc& vtx_desc, const VAT& vtx_attr);
  static u32 GetVertexComponents(const TVtxDesc& vtx_desc, const VAT& vtx_attr);
  static std::unique_ptr<VertexLoaderBase> CreateVertexLoader(const TVtxDesc& vtx_desc,
                                                              const VAT& vtx_attr);
  virtual ~VertexLoaderBase() {}
  virtual int RunVertices(const u8* src, u8* dst, int count) = 0;

  // per loader public state
  PortableVertexDeclaration m_native_vtx_decl{};
  const u32 m_vertex_size;  // number of bytes of a raw GC vertex
  const u32 m_native_components;

  // used by VertexLoaderManager
  NativeVertexFormat* m_native_vertex_format = nullptr;
  int m_numLoadedVertices = 0;

protected:
  VertexLoaderBase(const TVtxDesc& vtx_desc, const VAT& vtx_attr)
      : m_vertex_size{GetVertexSize(vtx_desc, vtx_attr)}, m_native_components{GetVertexComponents(
                                                              vtx_desc, vtx_attr)},
        m_VtxAttr{vtx_attr}, m_VtxDesc{vtx_desc}
  {
  }

  // GC vertex format
  const VAT m_VtxAttr;
  const TVtxDesc m_VtxDesc;
};
