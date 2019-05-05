// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/NativeVertexFormat.h"

class DataReader;

class VertexLoaderUID
{
  std::array<u32, 5> vid;
  size_t hash;

public:
  VertexLoaderUID() {}
  VertexLoaderUID(const TVtxDesc& vtx_desc, const VAT& vat)
  {
    vid[0] = vtx_desc.Hex & 0xFFFFFFFF;
    vid[1] = vtx_desc.Hex >> 32;
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
  static std::unique_ptr<VertexLoaderBase> CreateVertexLoader(const TVtxDesc& vtx_desc,
                                                              const VAT& vtx_attr);
  virtual ~VertexLoaderBase() {}
  virtual int RunVertices(DataReader src, DataReader dst, int count) = 0;

  virtual bool IsInitialized() = 0;

  // For debugging / profiling
  std::string ToString() const;

  virtual std::string GetName() const = 0;

  // per loader public state
  int m_VertexSize = 0;  // number of bytes of a raw GC vertex
  PortableVertexDeclaration m_native_vtx_decl{};
  u32 m_native_components = 0;

  // used by VertexLoaderManager
  NativeVertexFormat* m_native_vertex_format = nullptr;
  int m_numLoadedVertices = 0;

protected:
  VertexLoaderBase(const TVtxDesc& vtx_desc, const VAT& vtx_attr);
  void SetVAT(const VAT& vat);

  // GC vertex format
  TVtxAttr m_VtxAttr;  // VAT decoded into easy format
  TVtxDesc m_VtxDesc;  // Not really used currently - or well it is, but could be easily avoided.
  VAT m_vat;
};
