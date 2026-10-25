// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoaderPPC64.h"

#include <array>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexLoaderManager.h"

using namespace PPC64Gen;

// The loader is a leaf function: RunVertices(src, dst, count - 1), returning the number of vertices
// loaded. It uses the volatile registers, and R14-R16, which it saves below the stack pointer, in
// the ELFv2 ABI's protected zone.
constexpr GPR src_reg = GPR::R3;
constexpr GPR dst_reg = GPR::R4;
constexpr GPR remaining_reg = GPR::R5;
constexpr GPR skipped_reg = GPR::R6;
constexpr GPR saved_count = GPR::R7;
constexpr GPR stride_reg = GPR::R8;
constexpr GPR arraybase_reg = GPR::R9;
constexpr GPR scale_reg = GPR::R10;
// The base of an indexed attribute.
constexpr GPR scratch1_reg = GPR::R11;
constexpr GPR scratch2_reg = GPR::R12;
constexpr GPR scratch3_reg = GPR::R14;
constexpr GPR scratch4_reg = GPR::R15;
constexpr GPR scratch5_reg = GPR::R16;
// An offset for X-form loads and stores. R0 can't be a base register.
constexpr GPR offset_reg = GPR::R0;
constexpr VSR value_vsr = VSR::VS32;
constexpr VSR scale_vsr = VSR::VS33;

alignas(16) static const float scale_factors[] = {
    1.0 / (1ULL << 0),  1.0 / (1ULL << 1),  1.0 / (1ULL << 2),  1.0 / (1ULL << 3),
    1.0 / (1ULL << 4),  1.0 / (1ULL << 5),  1.0 / (1ULL << 6),  1.0 / (1ULL << 7),
    1.0 / (1ULL << 8),  1.0 / (1ULL << 9),  1.0 / (1ULL << 10), 1.0 / (1ULL << 11),
    1.0 / (1ULL << 12), 1.0 / (1ULL << 13), 1.0 / (1ULL << 14), 1.0 / (1ULL << 15),
    1.0 / (1ULL << 16), 1.0 / (1ULL << 17), 1.0 / (1ULL << 18), 1.0 / (1ULL << 19),
    1.0 / (1ULL << 20), 1.0 / (1ULL << 21), 1.0 / (1ULL << 22), 1.0 / (1ULL << 23),
    1.0 / (1ULL << 24), 1.0 / (1ULL << 25), 1.0 / (1ULL << 26), 1.0 / (1ULL << 27),
    1.0 / (1ULL << 28), 1.0 / (1ULL << 29), 1.0 / (1ULL << 30), 1.0 / (1ULL << 31),
};

VertexLoaderPPC64::VertexLoaderPPC64(const TVtxDesc& vtx_desc, const VAT& vtx_att)
    : VertexLoaderBase(vtx_desc, vtx_att)
{
  AllocCodeSpace(16384);
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  ClearCodeSpace();
  GenerateVertexLoader();
  WriteProtect(true);
}

// Returns the register to use as the base and an offset from that register.
// For indexed attributes, the address is computed into scratch1_reg, which is returned with no
// offset. For direct attributes, an offset from src_reg is returned.
std::pair<GPR, u32> VertexLoaderPPC64::GetVertexAddr(CPArray array, VertexComponentFormat attribute)
{
  if (!IsIndexed(attribute))
    return {src_reg, m_src_ofs};

  if (attribute == VertexComponentFormat::Index8)
  {
    LBZ(scratch1_reg, src_reg, static_cast<s16>(m_src_ofs));
    m_src_ofs += 1;
  }
  else  // Index16
  {
    LI(offset_reg, static_cast<s16>(m_src_ofs));
    LHBRX(scratch1_reg, src_reg, offset_reg);
    m_src_ofs += 2;
  }

  if (array == CPArray::Position)
  {
    // An index of all ones skips the vertex.
    CMPLWI(CRF::CR0, scratch1_reg, attribute == VertexComponentFormat::Index8 ? 0xFF : 0xFFFF);
    m_skip_vertex = BC(CCFlags::EQ, CRF::CR0);
  }

  LWZ(scratch2_reg, stride_reg, static_cast<s16>(static_cast<u8>(array) * 4));
  MULLW(scratch1_reg, scratch1_reg, scratch2_reg);
  LD(scratch2_reg, arraybase_reg, static_cast<s16>(static_cast<u8>(array) * 8));
  ADD(scratch1_reg, scratch1_reg, scratch2_reg);
  return {scratch1_reg, 0};
}

void VertexLoaderPPC64::ReadVertex(VertexComponentFormat attribute, ComponentFormat format,
                                   int count_in, int count_out, bool dequantize,
                                   u8 scaling_exponent, AttributeFormat* native_format, GPR reg,
                                   u32 offset)
{
  const int elem_size = GetElementSize(format);
  const int load_bytes = elem_size * count_in;
  const bool scale = format < ComponentFormat::Float && dequantize && scaling_exponent;

  if (scale)
  {
    LI(offset_reg, static_cast<s16>(scaling_exponent * 4));
    LXSSPX(scale_vsr, scale_reg, offset_reg);
  }

  // Where the Z-freeze caches go, if this attribute has one.
  enum class Cache
  {
    None,
    Position,
    Normal,
  } cache = Cache::None;
  const void* cache_base = nullptr;
  if (native_format == &m_native_vtx_decl.position)
  {
    cache = Cache::Position;
    cache_base = VertexLoaderManager::position_cache.data();
  }
  else if (native_format == &m_native_vtx_decl.normals[0])
  {
    cache = Cache::Normal;
    cache_base = VertexLoaderManager::normal_cache.data();
  }
  else if (native_format == &m_native_vtx_decl.normals[1])
  {
    cache = Cache::Normal;
    cache_base = VertexLoaderManager::tangent_cache.data();
  }
  else if (native_format == &m_native_vtx_decl.normals[2])
  {
    cache = Cache::Normal;
    cache_base = VertexLoaderManager::binormal_cache.data();
  }

  // The address of the cache entry, if it's to be written: for positions, the entry of
  // position_cache for the remaining count, if it's below 3; for normals, if it's 0.
  FixupBranch no_cache;
  if (cache != Cache::None)
  {
    MOVP2R(scratch5_reg, cache_base);
    if (cache == Cache::Position)
    {
      CMPWI(CRF::CR1, remaining_reg, 3);
      RLDICR(scratch4_reg, remaining_reg, 4, 59);  // * sizeof(std::array<float, 4>)
      ADD(scratch5_reg, scratch5_reg, scratch4_reg);
      no_cache = BC(CCFlags::GE, CRF::CR1);
    }
    else
    {
      CMPWI(CRF::CR1, remaining_reg, 0);
      no_cache = BC(CCFlags::NE, CRF::CR1);
    }
    // Mark the cache to be written with a non-zero scratch4_reg.
    LI(scratch4_reg, 1);
    const FixupBranch done = B();
    SetJumpTarget(no_cache);
    LI(scratch4_reg, 0);
    SetJumpTarget(done);
  }

  for (int i = 0; i < count_in; i++)
  {
    const s16 src = static_cast<s16>(offset + i * elem_size);
    const s16 dst = static_cast<s16>(m_dst_ofs + i * 4);
    if (format >= ComponentFormat::Float)
    {
      // Big-endian to little-endian.
      LI(offset_reg, src);
      LWBRX(scratch2_reg, reg, offset_reg);
      STW(scratch2_reg, dst_reg, dst);
    }
    else
    {
      switch (format)
      {
      case ComponentFormat::UByte:
        LBZ(scratch2_reg, reg, src);
        break;
      case ComponentFormat::Byte:
        LBZ(scratch2_reg, reg, src);
        EXTSB(scratch2_reg, scratch2_reg);
        break;
      case ComponentFormat::UShort:
        LI(offset_reg, src);
        LHBRX(scratch2_reg, reg, offset_reg);
        break;
      case ComponentFormat::Short:
      default:
        LI(offset_reg, src);
        LHBRX(scratch2_reg, reg, offset_reg);
        EXTSH(scratch2_reg, scratch2_reg);
        break;
      }
      // float(value) * scale, which is exact, in double precision, then stored as a single.
      MTVSRD(value_vsr, scratch2_reg);
      XSCVSXDDP(value_vsr, value_vsr);
      if (scale)
        XSMULDP(value_vsr, value_vsr, scale_vsr);
      LI(offset_reg, dst);
      STXSSPX(value_vsr, dst_reg, offset_reg);
      if (cache != Cache::None)
        LWZ(scratch2_reg, dst_reg, dst);
    }

    if (cache != Cache::None)
    {
      // scratch2_reg holds the float's bits.
      CMPWI(CRF::CR1, scratch4_reg, 0);
      const FixupBranch skip = BC(CCFlags::EQ, CRF::CR1);
      STW(scratch2_reg, scratch5_reg, static_cast<s16>(i * 4));
      SetJumpTarget(skip);
    }
  }

  // Elements that the attribute doesn't have, before a texture matrix index, are 0.
  for (int i = count_in; i < count_out; i++)
  {
    LI(scratch2_reg, 0);
    STW(scratch2_reg, dst_reg, static_cast<s16>(m_dst_ofs + i * 4));
  }

  native_format->components = count_out;
  native_format->enable = true;
  native_format->offset = m_dst_ofs;
  native_format->type = ComponentFormat::Float;
  native_format->integer = false;
  m_dst_ofs += sizeof(float) * count_out;

  if (attribute == VertexComponentFormat::Direct)
    m_src_ofs += load_bytes;
}

void VertexLoaderPPC64::ReadColor(VertexComponentFormat attribute, ColorFormat format, GPR reg,
                                  u32 offset)
{
  // The output is the bytes R, G, B, A, which is ABGR in a little-endian word.
  const GPR in = scratch3_reg;
  const GPR out = scratch4_reg;
  const GPR tmp = scratch5_reg;

  // out |= expand(field) << shift, where the field is width bits at the given bit of in, and
  // expanding repeats its top bits below it, to make 8 bits.
  const auto expand_field = [&](u32 bit, u32 width, u32 shift, bool first) {
    // tmp = field << (8 - width)
    RLWINM(tmp, in, (32 - bit + 8 - width) % 32, 24, 31 - (8 - width));
    // tmp |= tmp >> width
    if (first)
    {
      SRWI(out, tmp, width);
      OR(out, out, tmp);
      if (shift != 0)
        SLWI(out, out, shift);
    }
    else
    {
      SRWI(scratch2_reg, tmp, width);
      OR(tmp, tmp, scratch2_reg);
      if (shift != 0)
        SLWI(tmp, tmp, shift);
      OR(out, out, tmp);
    }
  };

  int load_bytes = 0;
  switch (format)
  {
  case ColorFormat::RGB888:
  case ColorFormat::RGB888x:
  case ColorFormat::RGBA8888:
    LWZ(out, reg, static_cast<s16>(offset));
    if (format != ColorFormat::RGBA8888)
      ORIS(out, out, 0xFF00);
    STW(out, dst_reg, static_cast<s16>(m_dst_ofs));
    load_bytes = format == ColorFormat::RGB888 ? 3 : 4;
    break;

  case ColorFormat::RGB565:
    // RRRRRGGG GGGBBBBB, big-endian
    LI(offset_reg, static_cast<s16>(offset));
    LHBRX(in, reg, offset_reg);
    expand_field(11, 5, 0, true);   // R
    expand_field(5, 6, 8, false);   // G
    expand_field(0, 5, 16, false);  // B
    ORIS(out, out, 0xFF00);         // A
    STW(out, dst_reg, static_cast<s16>(m_dst_ofs));
    load_bytes = 2;
    break;

  case ColorFormat::RGBA4444:
    // RRRRGGGG BBBBAAAA, read little-endian: BBBBAAAA RRRRGGGG
    LHZ(in, reg, static_cast<s16>(offset));
    RLWINM(out, in, 32 - 4, 28, 31);  // R
    RLWINM(tmp, in, 8, 20, 23);       // G
    OR(out, out, tmp);
    RLWINM(tmp, in, 4, 12, 15);  // B
    OR(out, out, tmp);
    RLWINM(tmp, in, 16, 4, 7);  // A
    OR(out, out, tmp);
    // Repeat each nibble.
    SLWI(tmp, out, 4);
    OR(out, out, tmp);
    STW(out, dst_reg, static_cast<s16>(m_dst_ofs));
    load_bytes = 2;
    break;

  case ColorFormat::RGBA6666:
  default:
    // RRRRRRGG GGGGBBBB BBAAAAAA, big-endian, read as the low 24 bits of the word that ends with it
    LI(offset_reg, static_cast<s16>(offset - 1));
    LWBRX(in, reg, offset_reg);
    expand_field(18, 6, 0, true);   // R
    expand_field(12, 6, 8, false);  // G
    expand_field(6, 6, 16, false);  // B
    expand_field(0, 6, 24, false);  // A
    STW(out, dst_reg, static_cast<s16>(m_dst_ofs));
    load_bytes = 3;
    break;
  }
  if (attribute == VertexComponentFormat::Direct)
    m_src_ofs += load_bytes;
}

void VertexLoaderPPC64::GenerateVertexLoader()
{
  ASSERT(m_vertex_size <= 255);

  bool has_tc = false;
  bool has_tc_scale = false;
  for (size_t i = 0; i < m_VtxDesc.high.TexCoord.Size(); i++)
  {
    has_tc |= m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent;
    has_tc_scale |= (m_VtxAttr.GetTexFrac(i) != 0);
  }

  const bool need_scale = (m_VtxAttr.g0.ByteDequant && m_VtxAttr.g0.PosFrac) ||
                          (has_tc && has_tc_scale) ||
                          (m_VtxDesc.low.Normal != VertexComponentFormat::NotPresent);

  AlignCode16();
  STD(scratch3_reg, GPR::SP, -8);
  STD(scratch4_reg, GPR::SP, -16);
  STD(scratch5_reg, GPR::SP, -24);
  if (IsIndexed(m_VtxDesc.low.Position))
    LI(skipped_reg, 0);
  ADDI(saved_count, remaining_reg, 1);

  MOVP2R(stride_reg, g_main_cp_state.array_strides.data());
  MOVP2R(arraybase_reg, VertexLoaderManager::cached_arraybases.data());
  if (need_scale)
    MOVP2R(scale_reg, scale_factors);

  const u8* loop_start = GetCodePtr();

  if (m_VtxDesc.low.PosMatIdx)
  {
    LBZ(scratch2_reg, src_reg, static_cast<s16>(m_src_ofs));
    CLRLWI(scratch2_reg, scratch2_reg, 26);  // & 0x3F
    STW(scratch2_reg, dst_reg, static_cast<s16>(m_dst_ofs));

    // Z-Freeze
    CMPWI(CRF::CR1, remaining_reg, 3);
    const FixupBranch dont_store = BC(CCFlags::GE, CRF::CR1);
    MOVP2R(scratch5_reg, VertexLoaderManager::position_matrix_index_cache.data());
    SLDI(scratch4_reg, remaining_reg, 2);
    STWX(scratch2_reg, scratch5_reg, scratch4_reg);
    SetJumpTarget(dont_store);

    m_native_vtx_decl.posmtx.components = 4;
    m_native_vtx_decl.posmtx.enable = true;
    m_native_vtx_decl.posmtx.offset = m_dst_ofs;
    m_native_vtx_decl.posmtx.type = ComponentFormat::UByte;
    m_native_vtx_decl.posmtx.integer = true;
    m_src_ofs += sizeof(u8);
    m_dst_ofs += sizeof(u32);
  }

  std::array<u32, 8> texmatidx_ofs;
  for (size_t i = 0; i < m_VtxDesc.low.TexMatIdx.Size(); i++)
  {
    if (m_VtxDesc.low.TexMatIdx[i])
      texmatidx_ofs[i] = m_src_ofs++;
  }

  // Position
  {
    const int pos_elements = m_VtxAttr.g0.PosElements == CoordComponentCount::XY ? 2 : 3;

    const auto [reg, offset] = GetVertexAddr(CPArray::Position, m_VtxDesc.low.Position);
    ReadVertex(m_VtxDesc.low.Position, m_VtxAttr.g0.PosFormat, pos_elements, pos_elements,
               m_VtxAttr.g0.ByteDequant, m_VtxAttr.g0.PosFrac, &m_native_vtx_decl.position, reg,
               offset);
  }

  if (m_VtxDesc.low.Normal != VertexComponentFormat::NotPresent)
  {
    static constexpr Common::EnumMap<u8, ComponentFormat::InvalidFloat7> SCALE_MAP = {7, 6, 15, 14,
                                                                                      0, 0, 0,  0};
    const u8 scaling_exponent = SCALE_MAP[m_VtxAttr.g0.NormalFormat];

    // Normal
    auto [reg, offset] = GetVertexAddr(CPArray::Normal, m_VtxDesc.low.Normal);
    ReadVertex(m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true, scaling_exponent,
               &m_native_vtx_decl.normals[0], reg, offset);

    if (m_VtxAttr.g0.NormalElements == NormalComponentCount::NTB)
    {
      const bool index3 = IsIndexed(m_VtxDesc.low.Normal) && m_VtxAttr.g0.NormalIndex3;
      const int elem_size = GetElementSize(m_VtxAttr.g0.NormalFormat);
      const int load_bytes = elem_size * 3;

      // Tangent. In Index3 mode, with indexed components, a new index replaces the index. The
      // tangent comes after the normal; even in Index3 mode, an extra offset of load_bytes is
      // applied, which is different from adding 1 to the index, as the stride may differ.
      if (index3)
        std::tie(reg, offset) = GetVertexAddr(CPArray::Normal, m_VtxDesc.low.Normal);
      ReadVertex(m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true, scaling_exponent,
                 &m_native_vtx_decl.normals[1], reg, offset + load_bytes);
      // Binormal
      if (index3)
        std::tie(reg, offset) = GetVertexAddr(CPArray::Normal, m_VtxDesc.low.Normal);
      ReadVertex(m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true, scaling_exponent,
                 &m_native_vtx_decl.normals[2], reg, offset + load_bytes * 2);
    }
  }

  for (u8 i = 0; i < m_VtxDesc.low.Color.Size(); i++)
  {
    m_native_vtx_decl.colors[i].components = 4;
    m_native_vtx_decl.colors[i].type = ComponentFormat::UByte;
    m_native_vtx_decl.colors[i].integer = false;

    if (m_VtxDesc.low.Color[i] != VertexComponentFormat::NotPresent)
    {
      const auto [reg, offset] = GetVertexAddr(CPArray::Color0 + i, m_VtxDesc.low.Color[i]);
      ReadColor(m_VtxDesc.low.Color[i], m_VtxAttr.GetColorFormat(i), reg, offset);
      m_native_vtx_decl.colors[i].components = 4;
      m_native_vtx_decl.colors[i].enable = true;
      m_native_vtx_decl.colors[i].offset = m_dst_ofs;
      m_native_vtx_decl.colors[i].type = ComponentFormat::UByte;
      m_native_vtx_decl.colors[i].integer = false;
      m_dst_ofs += 4;
    }
  }

  for (u8 i = 0; i < m_VtxDesc.high.TexCoord.Size(); i++)
  {
    m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;
    m_native_vtx_decl.texcoords[i].type = ComponentFormat::Float;
    m_native_vtx_decl.texcoords[i].integer = false;

    const int elements = m_VtxAttr.GetTexElements(i) == TexComponentCount::S ? 1 : 2;
    if (m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent)
    {
      const auto [reg, offset] = GetVertexAddr(CPArray::TexCoord0 + i, m_VtxDesc.high.TexCoord[i]);
      const u8 scaling_exponent = m_VtxAttr.GetTexFrac(i);
      ReadVertex(m_VtxDesc.high.TexCoord[i], m_VtxAttr.GetTexFormat(i), elements,
                 m_VtxDesc.low.TexMatIdx[i] ? 2 : elements, m_VtxAttr.g0.ByteDequant,
                 scaling_exponent, &m_native_vtx_decl.texcoords[i], reg, offset);
    }
    if (m_VtxDesc.low.TexMatIdx[i])
    {
      m_native_vtx_decl.texcoords[i].components = 3;
      m_native_vtx_decl.texcoords[i].enable = true;
      m_native_vtx_decl.texcoords[i].type = ComponentFormat::Float;
      m_native_vtx_decl.texcoords[i].integer = false;

      // float(index)
      LBZ(scratch2_reg, src_reg, static_cast<s16>(texmatidx_ofs[i]));
      MTVSRD(value_vsr, scratch2_reg);
      XSCVSXDDP(value_vsr, value_vsr);

      if (m_VtxDesc.high.TexCoord[i] == VertexComponentFormat::NotPresent)
      {
        m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;
        LI(scratch2_reg, 0);
        STW(scratch2_reg, dst_reg, static_cast<s16>(m_dst_ofs));
        STW(scratch2_reg, dst_reg, static_cast<s16>(m_dst_ofs + 4));
        m_dst_ofs += sizeof(float) * 2;
      }
      LI(offset_reg, static_cast<s16>(m_dst_ofs));
      STXSSPX(value_vsr, dst_reg, offset_reg);
      m_dst_ofs += sizeof(float);
    }
  }

  // Prepare for the next vertex.
  ADDI(dst_reg, dst_reg, static_cast<s16>(m_dst_ofs));
  const u8* cont = GetCodePtr();
  ADDI(src_reg, src_reg, static_cast<s16>(m_src_ofs));

  ADDI(remaining_reg, remaining_reg, -1);
  CMPWI(CRF::CR0, remaining_reg, 0);
  BC(CCFlags::GE, CRF::CR0, loop_start);

  if (IsIndexed(m_VtxDesc.low.Position))
    SUBF(GPR::R3, skipped_reg, saved_count);
  else
    MR(GPR::R3, saved_count);
  LD(scratch3_reg, GPR::SP, -8);
  LD(scratch4_reg, GPR::SP, -16);
  LD(scratch5_reg, GPR::SP, -24);
  BLR();

  if (IsIndexed(m_VtxDesc.low.Position))
  {
    SetJumpTarget(m_skip_vertex);
    ADDI(skipped_reg, skipped_reg, 1);
    B(cont);
  }

  FlushIcache();

  ASSERT_MSG(VIDEO, m_vertex_size == m_src_ofs,
             "Vertex size from vertex loader ({}) does not match expected vertex size ({})!\nVtx "
             "desc: {:08x} {:08x}\nVtx attr: {:08x} {:08x} {:08x}",
             m_src_ofs, m_vertex_size, m_VtxDesc.low.Hex, m_VtxDesc.high.Hex, m_VtxAttr.g0.Hex,
             m_VtxAttr.g1.Hex, m_VtxAttr.g2.Hex);
  m_native_vtx_decl.stride = m_dst_ofs;
}

int VertexLoaderPPC64::RunVertices(const u8* src, u8* dst, int count)
{
  m_numLoadedVertices += count;
  return reinterpret_cast<int (*)(const u8*, u8*, int)>(region)(src, dst, count - 1);
}
