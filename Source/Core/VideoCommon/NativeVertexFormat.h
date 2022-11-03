// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <functional>  // for hash

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"

// m_components
enum
{
  VB_HAS_POSMTXIDX = (1 << 1),
  VB_HAS_TEXMTXIDX0 = (1 << 2),
  VB_HAS_TEXMTXIDX1 = (1 << 3),
  VB_HAS_TEXMTXIDX2 = (1 << 4),
  VB_HAS_TEXMTXIDX3 = (1 << 5),
  VB_HAS_TEXMTXIDX4 = (1 << 6),
  VB_HAS_TEXMTXIDX5 = (1 << 7),
  VB_HAS_TEXMTXIDX6 = (1 << 8),
  VB_HAS_TEXMTXIDX7 = (1 << 9),
  VB_HAS_TEXMTXIDXALL = (0xff << 2),

  // VB_HAS_POS=0, // Implied, it always has pos! don't bother testing
  VB_HAS_NORMAL = (1 << 10),
  VB_HAS_TANGENT = (1 << 11),
  VB_HAS_BINORMAL = (1 << 12),

  VB_COL_SHIFT = 13,
  VB_HAS_COL0 = (1 << 13),
  VB_HAS_COL1 = (1 << 14),

  VB_HAS_UV0 = (1 << 15),
  VB_HAS_UV1 = (1 << 16),
  VB_HAS_UV2 = (1 << 17),
  VB_HAS_UV3 = (1 << 18),
  VB_HAS_UV4 = (1 << 19),
  VB_HAS_UV5 = (1 << 20),
  VB_HAS_UV6 = (1 << 21),
  VB_HAS_UV7 = (1 << 22),
  VB_HAS_UVALL = (0xff << 15),
  VB_HAS_UVTEXMTXSHIFT = 13,
};

struct AttributeFormat
{
  ComponentFormat type;
  int components;
  int offset;
  bool enable;
  bool integer;
};

struct PortableVertexDeclaration
{
  int stride;

  AttributeFormat position;
  std::array<AttributeFormat, 3> normals;
  std::array<AttributeFormat, 2> colors;
  std::array<AttributeFormat, 8> texcoords;
  AttributeFormat posmtx;

  // Make sure we initialize padding to 0 since padding is included in the == memcmp
  PortableVertexDeclaration() { memset(this, 0, sizeof(*this)); }

  inline bool operator<(const PortableVertexDeclaration& b) const
  {
    return memcmp(this, &b, sizeof(PortableVertexDeclaration)) < 0;
  }
  inline bool operator==(const PortableVertexDeclaration& b) const
  {
    return memcmp(this, &b, sizeof(PortableVertexDeclaration)) == 0;
  }
};

static_assert(std::is_trivially_copyable_v<PortableVertexDeclaration>,
              "Make sure we can memset-initialize");

namespace std
{
template <>
struct hash<PortableVertexDeclaration>
{
  // Implementation from Wikipedia.
  template <typename T>
  u32 Fletcher32(const T& data) const
  {
    static_assert(sizeof(T) % sizeof(u16) == 0);

    auto buf = reinterpret_cast<const u16*>(&data);
    size_t len = sizeof(T) / sizeof(u16);
    u32 sum1 = 0xffff, sum2 = 0xffff;

    while (len)
    {
      size_t tlen = len > 360 ? 360 : len;
      len -= tlen;

      do
      {
        sum1 += *buf++;
        sum2 += sum1;
      } while (--tlen);

      sum1 = (sum1 & 0xffff) + (sum1 >> 16);
      sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }

    // Second reduction step to reduce sums to 16 bits
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    return (sum2 << 16 | sum1);
  }
  size_t operator()(const PortableVertexDeclaration& decl) const { return Fletcher32(decl); }
};
}  // namespace std

// The implementation of this class is specific for GL/DX, so NativeVertexFormat.cpp
// is in the respective backend, not here in VideoCommon.

// Note that this class can't just invent arbitrary vertex formats out of its input -
// all the data loading code must always be made compatible.
class NativeVertexFormat
{
public:
  NativeVertexFormat(const PortableVertexDeclaration& vtx_decl) : m_decl(vtx_decl) {}
  virtual ~NativeVertexFormat() {}

  NativeVertexFormat(const NativeVertexFormat&) = delete;
  NativeVertexFormat& operator=(const NativeVertexFormat&) = delete;
  NativeVertexFormat(NativeVertexFormat&&) = default;
  NativeVertexFormat& operator=(NativeVertexFormat&&) = default;

  u32 GetVertexStride() const { return m_decl.stride; }
  const PortableVertexDeclaration& GetVertexDeclaration() const { return m_decl; }

protected:
  PortableVertexDeclaration m_decl;
};
