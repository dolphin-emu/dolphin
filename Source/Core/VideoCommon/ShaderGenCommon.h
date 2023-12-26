// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/StringUtil.h"
#include "Common/TypeUtils.h"

#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/VideoCommon.h"

/**
 * Common interface for classes that need to go through the shader generation path
 * (GenerateVertexShader, GenerateGeometryShader, GeneratePixelShader)
 * In particular, this includes the shader code generator (ShaderCode).
 * A different class (ShaderUid) can be used to uniquely identify each ShaderCode object.
 * More interesting things can be done with this, e.g. ShaderConstantProfile checks what shader
 * constants are being used. This can be used to optimize buffer management.
 * If the class does not use one or more of these methods (e.g. Uid class does not need code), the
 * method will be defined as a no-op by the base class, and the call
 * should be optimized out. The reason for this implementation is so that shader
 * selection/generation can be done in two passes, with only a cache lookup being
 * required if the shader has already been generated.
 */
class ShaderGeneratorInterface
{
public:
  /*
   * Used when the shader generator would write a piece of ShaderCode.
   * Can be used like printf.
   * @note In the ShaderCode implementation, this does indeed write the parameter string to an
   * internal buffer. However, you're free to do whatever you like with the parameter.
   */
  void Write(const char*, ...)
#ifdef __GNUC__
      __attribute__((format(printf, 2, 3)))
#endif
  {
  }

  /*
   * Tells us that a specific constant range (including last_index) is being used by the shader
   */
  void SetConstantsUsed(unsigned int first_index, unsigned int last_index) {}
};

/*
 * Shader UID class used to uniquely identify the ShaderCode output written in the shader generator.
 * uid_data can be any struct of parameters that uniquely identify each shader code output.
 * Unless performance is not an issue, uid_data should be tightly packed to reduce memory footprint.
 * Shader generators will write to specific uid_data fields; ShaderUid methods will only read raw
 * u32 values from a union.
 * NOTE: Because LinearDiskCache reads and writes the storage associated with a ShaderUid instance,
 * ShaderUid must be trivially copyable.
 */
template <class uid_data>
class ShaderUid : public ShaderGeneratorInterface
{
public:
  static_assert(std::is_trivially_copyable_v<uid_data>,
                "uid_data must be a trivially copyable type");

  ShaderUid() { memset(GetUidData(), 0, GetUidDataSize()); }

  bool operator==(const ShaderUid& obj) const
  {
    return memcmp(GetUidData(), obj.GetUidData(), GetUidDataSize()) == 0;
  }

  bool operator!=(const ShaderUid& obj) const { return !operator==(obj); }

  // determines the storage order inside STL containers
  bool operator<(const ShaderUid& obj) const
  {
    return memcmp(GetUidData(), obj.GetUidData(), GetUidDataSize()) < 0;
  }

  // Returns a pointer to an internally stored object of the uid_data type.
  uid_data* GetUidData() { return &data; }

  // Returns a pointer to an internally stored object of the uid_data type.
  const uid_data* GetUidData() const { return &data; }

  // Returns the raw bytes that make up the shader UID.
  const u8* GetUidDataRaw() const { return reinterpret_cast<const u8*>(&data); }

  // Returns the size of the underlying UID data structure in bytes.
  size_t GetUidDataSize() const { return sizeof(data); }

private:
  uid_data data{};
};

class ShaderCode : public ShaderGeneratorInterface
{
public:
  ShaderCode() { m_buffer.reserve(16384); }
  const std::string& GetBuffer() const { return m_buffer; }

  // Writes format strings using fmtlib format strings.
  template <typename... Args>
  void Write(fmt::format_string<Args...> format, Args&&... args)
  {
    fmt::format_to(std::back_inserter(m_buffer), format, std::forward<Args>(args)...);
  }

protected:
  std::string m_buffer;
};

/**
 * Generates a shader constant profile which can be used to query which constants are used in a
 * shader
 */
class ShaderConstantProfile : public ShaderGeneratorInterface
{
public:
  ShaderConstantProfile(int num_constants) { constant_usage.resize(num_constants); }
  void SetConstantsUsed(unsigned int first_index, unsigned int last_index)
  {
    for (unsigned int i = first_index; i < last_index + 1; ++i)
      constant_usage[i] = true;
  }

  bool ConstantIsUsed(unsigned int index) const
  {
    // TODO: Not ready for usage yet
    return true;
    // return constant_usage[index];
  }

private:
  std::vector<bool> constant_usage;  // TODO: Is vector<bool> appropriate here?
};

// Host config contains the settings which can influence generated shaders.
union ShaderHostConfig
{
  u32 bits;

  BitField<0, 1, bool, u32> msaa;
  BitField<1, 1, bool, u32> ssaa;
  BitField<2, 1, bool, u32> stereo;
  BitField<3, 1, bool, u32> wireframe;
  BitField<4, 1, bool, u32> per_pixel_lighting;
  BitField<5, 1, bool, u32> vertex_rounding;
  BitField<6, 1, bool, u32> fast_depth_calc;
  BitField<7, 1, bool, u32> bounding_box;
  BitField<8, 1, bool, u32> backend_dual_source_blend;
  BitField<9, 1, bool, u32> backend_geometry_shaders;
  BitField<10, 1, bool, u32> backend_early_z;
  BitField<11, 1, bool, u32> backend_bbox;
  BitField<12, 1, bool, u32> backend_gs_instancing;
  BitField<13, 1, bool, u32> backend_clip_control;
  BitField<14, 1, bool, u32> backend_ssaa;
  BitField<15, 1, bool, u32> backend_atomics;
  BitField<16, 1, bool, u32> backend_depth_clamp;
  BitField<17, 1, bool, u32> backend_reversed_depth_range;
  BitField<18, 1, bool, u32> backend_bitfield;
  BitField<19, 1, bool, u32> backend_dynamic_sampler_indexing;
  BitField<20, 1, bool, u32> backend_shader_framebuffer_fetch;
  BitField<21, 1, bool, u32> backend_logic_op;
  BitField<22, 1, bool, u32> backend_palette_conversion;
  BitField<23, 1, bool, u32> enable_validation_layer;
  BitField<24, 1, bool, u32> manual_texture_sampling;
  BitField<25, 1, bool, u32> manual_texture_sampling_custom_texture_sizes;
  BitField<26, 1, bool, u32> backend_sampler_lod_bias;
  BitField<27, 1, bool, u32> backend_dynamic_vertex_loader;
  BitField<28, 1, bool, u32> backend_vs_point_line_expand;
  BitField<29, 1, bool, u32> backend_gl_layer_in_fs;

  static ShaderHostConfig GetCurrent();
};

// Gets the filename of the specified type of cache object (e.g. vertex shader, pipeline).
std::string GetDiskShaderCacheFileName(APIType api_type, const char* type, bool include_gameid,
                                       bool include_host_config, bool include_api = true);

void WriteIsNanHeader(ShaderCode& out, APIType api_type);
void WriteBitfieldExtractHeader(ShaderCode& out, APIType api_type,
                                const ShaderHostConfig& host_config);

void GenerateVSOutputMembers(ShaderCode& object, APIType api_type, u32 texgens,
                             const ShaderHostConfig& host_config, std::string_view qualifier,
                             ShaderStage stage);

void AssignVSOutputMembers(ShaderCode& object, std::string_view a, std::string_view b, u32 texgens,
                           const ShaderHostConfig& host_config);

void GenerateLineOffset(ShaderCode& object, std::string_view indent0, std::string_view indent1,
                        std::string_view pos_a, std::string_view pos_b, std::string_view sign);

void GenerateVSLineExpansion(ShaderCode& object, std::string_view indent, u32 texgens);

void GenerateVSPointExpansion(ShaderCode& object, std::string_view indent, u32 texgens);

// We use the flag "centroid" to fix some MSAA rendering bugs. With MSAA, the
// pixel shader will be executed for each pixel which has at least one passed sample.
// So there may be rendered pixels where the center of the pixel isn't in the primitive.
// As the pixel shader usually renders at the center of the pixel, this position may be
// outside the primitive. This will lead to sampling outside the texture, sign changes, ...
// As a workaround, we interpolate at the centroid of the coveraged pixel, which
// is always inside the primitive.
// Without MSAA, this flag is defined to have no effect.
const char* GetInterpolationQualifier(bool msaa, bool ssaa, bool in_glsl_interface_block = false,
                                      bool in = false);

// bitfieldExtract generator for BitField types
template <auto ptr_to_bitfield_member>
std::string BitfieldExtract(std::string_view source)
{
  using BitFieldT = Common::MemberType<ptr_to_bitfield_member>;
  return fmt::format("bitfieldExtract({}({}), {}, {})", BitFieldT::IsSigned() ? "int" : "uint",
                     source, static_cast<u32>(BitFieldT::StartBit()),
                     static_cast<u32>(BitFieldT::NumBits()));
}

template <auto last_member>
void WriteSwitch(ShaderCode& out, APIType ApiType, std::string_view variable,
                 const Common::EnumMap<std::string_view, last_member>& values, int indent,
                 bool break_)
{
  using enum_type = decltype(last_member);

  // Generate a tree of if statements recursively
  // std::function must be used because auto won't capture before initialization and thus can't be
  // used recursively
  std::function<void(u32, u32, u32)> BuildTree = [&](u32 cur_indent, u32 low, u32 high) {
    // Each generated statement is for low <= x < high
    if (high == low + 1)
    {
      // Down to 1 case (low <= x < low + 1 means x == low)
      const enum_type key = static_cast<enum_type>(low);
      // Note that this indentation behaves poorly for multi-line code
      out.Write("{:{}}{}  // {}\n", "", cur_indent, values[key], key);
    }
    else
    {
      u32 mid = low + ((high - low) / 2);
      out.Write("{:{}}if ({} < {}u) {{\n", "", cur_indent, variable, mid);
      BuildTree(cur_indent + 2, low, mid);
      out.Write("{:{}}}} else {{\n", "", cur_indent);
      BuildTree(cur_indent + 2, mid, high);
      out.Write("{:{}}}}\n", "", cur_indent);
    }
  };
  BuildTree(indent, 0, static_cast<u32>(last_member) + 1);
}

// Constant variable names
#define I_COLORS "color"
#define I_KCOLORS "k"
#define I_ALPHA "alphaRef"
#define I_TEXDIMS "texdim"
#define I_ZBIAS "czbias"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX "cindmtx"
#define I_FOGCOLOR "cfogcolor"
#define I_FOGI "cfogi"
#define I_FOGF "cfogf"
#define I_FOGRANGE "cfogrange"
#define I_ZSLOPE "czslope"
#define I_EFBSCALE "cefbscale"

#define I_POSNORMALMATRIX "cpnmtx"
#define I_PROJECTION "cproj"
#define I_MATERIALS "cmtrl"
#define I_LIGHTS "clights"
#define I_TEXMATRICES "ctexmtx"
#define I_TRANSFORMMATRICES "ctrmtx"
#define I_NORMALMATRICES "cnmtx"
#define I_POSTTRANSFORMMATRICES "cpostmtx"
#define I_PIXELCENTERCORRECTION "cpixelcenter"
#define I_VIEWPORT_SIZE "cviewport"
#define I_CACHED_TANGENT "ctangent"
#define I_CACHED_BINORMAL "cbinormal"

#define I_STEREOPARAMS "cstereo"
#define I_LINEPTPARAMS "clinept"
#define I_TEXOFFSET "ctexoffset"

static const char s_shader_uniforms[] = "\tuint    components;\n"
                                        "\tuint    xfmem_dualTexInfo;\n"
                                        "\tuint    xfmem_numColorChans;\n"
                                        "\tuint    missing_color_hex;\n"
                                        "\tfloat4  missing_color_value;\n"
                                        "\tfloat4 " I_POSNORMALMATRIX "[6];\n"
                                        "\tfloat4 " I_PROJECTION "[4];\n"
                                        "\tint4 " I_MATERIALS "[4];\n"
                                        "\tLight " I_LIGHTS "[8];\n"
                                        "\tfloat4 " I_TEXMATRICES "[24];\n"
                                        "\tfloat4 " I_TRANSFORMMATRICES "[64];\n"
                                        "\tfloat4 " I_NORMALMATRICES "[32];\n"
                                        "\tfloat4 " I_POSTTRANSFORMMATRICES "[64];\n"
                                        "\tfloat4 " I_PIXELCENTERCORRECTION ";\n"
                                        "\tfloat2 " I_VIEWPORT_SIZE ";\n"
                                        "\tuint4   xfmem_pack1[8];\n"
                                        "\tfloat4 " I_CACHED_TANGENT ";\n"
                                        "\tfloat4 " I_CACHED_BINORMAL ";\n"
                                        "\tuint vertex_stride;\n"
                                        "\tuint vertex_offset_rawnormal;\n"
                                        "\tuint vertex_offset_rawtangent;\n"
                                        "\tuint vertex_offset_rawbinormal;\n"
                                        "\tuint vertex_offset_rawpos;\n"
                                        "\tuint vertex_offset_posmtx;\n"
                                        "\tuint vertex_offset_rawcolor0;\n"
                                        "\tuint vertex_offset_rawcolor1;\n"
                                        "\tuint4 vertex_offset_rawtex[2];\n"  // std140 is pain
                                        "\t#define xfmem_texMtxInfo(i) (xfmem_pack1[(i)].x)\n"
                                        "\t#define xfmem_postMtxInfo(i) (xfmem_pack1[(i)].y)\n"
                                        "\t#define xfmem_color(i) (xfmem_pack1[(i)].z)\n"
                                        "\t#define xfmem_alpha(i) (xfmem_pack1[(i)].w)\n";

static const char s_geometry_shader_uniforms[] = "\tfloat4 " I_STEREOPARAMS ";\n"
                                                 "\tfloat4 " I_LINEPTPARAMS ";\n"
                                                 "\tint4 " I_TEXOFFSET ";\n"
                                                 "\tuint vs_expand;\n";

constexpr std::string_view CUSTOM_PIXELSHADER_COLOR_FUNC = "customShaderColor";

struct CustomPixelShader
{
  std::string custom_shader;
  std::string material_uniform_block;

  bool operator==(const CustomPixelShader& other) const = default;
};

struct CustomPixelShaderContents
{
  std::vector<CustomPixelShader> shaders;

  bool operator==(const CustomPixelShaderContents& other) const = default;
};

void WriteCustomShaderStructDef(ShaderCode* out, u32 numtexgens);
