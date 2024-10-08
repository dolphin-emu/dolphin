// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/UberShaderVertex.h"

#include "Common/EnumUtils.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/UberShaderCommon.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"

namespace UberShader
{
VertexShaderUid GetVertexShaderUid()
{
  VertexShaderUid out;

  vertex_ubershader_uid_data* const uid_data = out.GetUidData();
  uid_data->num_texgens = xfmem.numTexGen.numTexGens;

  return out;
}

static void GenVertexShaderTexGens(APIType api_type, const ShaderHostConfig& host_config,
                                   u32 num_texgen, ShaderCode& out);
static void LoadVertexAttribute(ShaderCode& code, const ShaderHostConfig& host_config, u32 indent,
                                std::string_view name, std::string_view shader_type,
                                std::string_view stored_type, std::string_view offset_name = {});

ShaderCode GenVertexShader(APIType api_type, const ShaderHostConfig& host_config,
                           const vertex_ubershader_uid_data* uid_data)
{
  const bool msaa = host_config.msaa;
  const bool ssaa = host_config.ssaa;
  const bool per_pixel_lighting = host_config.per_pixel_lighting;
  const bool vertex_rounding = host_config.vertex_rounding;
  const bool vertex_loader =
      host_config.backend_dynamic_vertex_loader || host_config.backend_vs_point_line_expand;
  const u32 num_texgen = uid_data->num_texgens;
  ShaderCode out;

  out.Write("// {}\n\n", *uid_data);
  out.Write("{}", s_lighting_struct);

  // uniforms
  out.Write("UBO_BINDING(std140, 2) uniform VSBlock {{\n");
  out.Write("{}", s_shader_uniforms);
  out.Write("}};\n");

  if (vertex_loader)
  {
    out.Write("UBO_BINDING(std140, 4) uniform GSBlock {{\n");
    out.Write("{}", s_geometry_shader_uniforms);
    out.Write("}};\n");
  }

  out.Write("struct VS_OUTPUT {{\n");
  GenerateVSOutputMembers(out, api_type, num_texgen, host_config, "", ShaderStage::Vertex);
  out.Write("}};\n\n");

  WriteIsNanHeader(out, api_type);
  WriteBitfieldExtractHeader(out, api_type, host_config);
  WriteLightingFunction(out);

  if (vertex_loader)
  {
    out.Write(R"(
SSBO_BINDING(1) readonly restrict buffer Vertices {{
  uint vertex_buffer[];
}};
)");
    if (api_type == APIType::D3D)
    {
      // Write a function to get an offset into vertex_buffer corresponding to this vertex.
      // This must be done differently for D3D compared to OpenGL/Vulkan/Metal, as on OpenGL, etc.,
      // gl_VertexID starts counting at the base vertex specified in glDrawElementsBaseVertex,
      // while on D3D, SV_VertexID (which spirv-cross translates gl_VertexID into) starts counting
      // at 0 regardless of the BaseVertexLocation value passed to DrawIndexed.  In both cases,
      // offset 0 of vertex_buffer corresponds to index 0 with basevertex set to 0, so we have to
      // manually apply the basevertex offset for D3D
      // D3D12 uses a root constant for this uniform, since it changes with every draw.
      // D3D11 doesn't currently support dynamic vertex loader, and we'll have to figure something
      // out for it if we want to support it in the future.
      out.Write("UBO_BINDING(std140, 5) uniform DX_Constants {{\n"
                "  uint base_vertex;\n"
                "}};\n\n"
                "uint GetVertexBaseOffset(uint vertex_id) {{\n"
                "  return (vertex_id + base_vertex) * vertex_stride;\n"
                "}}\n");
    }
    else
    {
      out.Write("uint GetVertexBaseOffset(uint vertex_id) {{\n"
                "  return vertex_id * vertex_stride;\n"
                "}}\n");
    }

    out.Write(R"(
uint4 load_input_uint4_ubyte4(uint vtx_offset, uint attr_offset) {{
  uint value = vertex_buffer[vtx_offset + attr_offset];
  return uint4(value & 0xffu, (value >> 8) & 0xffu, (value >> 16) & 0xffu, value >> 24);
}}

float4 load_input_float4_ubyte4(uint vtx_offset, uint attr_offset) {{
  return float4(load_input_uint4_ubyte4(vtx_offset, attr_offset)) / 255.0f;
}}

float3 load_input_float3_float3(uint vtx_offset, uint attr_offset) {{
  uint offset = vtx_offset + attr_offset;
  return float3(uintBitsToFloat(vertex_buffer[offset + 0]),
                uintBitsToFloat(vertex_buffer[offset + 1]),
                uintBitsToFloat(vertex_buffer[offset + 2]));
}}

float4 load_input_float4_rawpos(uint vtx_offset, uint attr_offset) {{
  uint components = attr_offset >> 16;
  uint offset = vtx_offset + (attr_offset & 0xffff);
  if (components < 3)
    return float4(uintBitsToFloat(vertex_buffer[offset + 0]),
                  uintBitsToFloat(vertex_buffer[offset + 1]),
                  0.0f, 1.0f);
  else
    return float4(uintBitsToFloat(vertex_buffer[offset + 0]),
                  uintBitsToFloat(vertex_buffer[offset + 1]),
                  uintBitsToFloat(vertex_buffer[offset + 2]),
                  1.0f);
}}

float3 load_input_float3_rawtex(uint vtx_offset, uint attr_offset) {{
  uint components = attr_offset >> 16;
  uint offset = vtx_offset + (attr_offset & 0xffff);
  if (components < 2)
    return float3(uintBitsToFloat(vertex_buffer[offset + 0]), 0.0f, 0.0f);
  else if (components < 3)
    return float3(uintBitsToFloat(vertex_buffer[offset + 0]),
                  uintBitsToFloat(vertex_buffer[offset + 1]),
                  0.0f);
  else
    return float3(uintBitsToFloat(vertex_buffer[offset + 0]),
                  uintBitsToFloat(vertex_buffer[offset + 1]),
                  uintBitsToFloat(vertex_buffer[offset + 2]));
}}

)");
  }
  else
  {
    out.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawpos;\n", ShaderAttrib::Position);
    out.Write("ATTRIBUTE_LOCATION({:s}) in uint4 posmtx;\n", ShaderAttrib::PositionMatrix);
    out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawnormal;\n", ShaderAttrib::Normal);
    out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawtangent;\n", ShaderAttrib::Tangent);
    out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawbinormal;\n", ShaderAttrib::Binormal);
    out.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawcolor0;\n", ShaderAttrib::Color0);
    out.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawcolor1;\n", ShaderAttrib::Color1);
    for (u32 i = 0; i < 8; ++i)
      out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawtex{};\n", ShaderAttrib::TexCoord0 + i, i);
  }

  if (host_config.backend_geometry_shaders)
  {
    out.Write("VARYING_LOCATION(0) out VertexData {{\n");
    GenerateVSOutputMembers(out, api_type, num_texgen, host_config,
                            GetInterpolationQualifier(msaa, ssaa, true, false),
                            ShaderStage::Vertex);
    out.Write("}} vs;\n");
  }
  else
  {
    // Let's set up attributes
    u32 counter = 0;
    out.Write("VARYING_LOCATION({}) {} out float4 colors_0;\n", counter++,
              GetInterpolationQualifier(msaa, ssaa));
    out.Write("VARYING_LOCATION({}) {} out float4 colors_1;\n", counter++,
              GetInterpolationQualifier(msaa, ssaa));
    for (u32 i = 0; i < num_texgen; ++i)
    {
      out.Write("VARYING_LOCATION({}) {} out float3 tex{};\n", counter++,
                GetInterpolationQualifier(msaa, ssaa), i);
    }
    if (!host_config.fast_depth_calc)
    {
      out.Write("VARYING_LOCATION({}) {} out float4 clipPos;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
    }
    if (per_pixel_lighting)
    {
      out.Write("VARYING_LOCATION({}) {} out float3 Normal;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
      out.Write("VARYING_LOCATION({}) {} out float3 WorldPos;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
    }
  }

  out.Write("void main()\n{{\n");

  out.Write("VS_OUTPUT o;\n"
            "\n");
  if (host_config.backend_vs_point_line_expand)
  {
    out.Write("uint vertex_id = gl_VertexID;\n"
              "if (vs_expand != 0u) {{\n"
              "  vertex_id = vertex_id >> 2;\n"
              "}}\n"
              "uint vertex_base_offset = GetVertexBaseOffset(vertex_id);\n");
  }
  else if (host_config.backend_dynamic_vertex_loader)
  {
    out.Write("uint vertex_base_offset = GetVertexBaseOffset(gl_VertexID);\n");
  }
  // rawpos is always needed
  LoadVertexAttribute(out, host_config, 0, "rawpos", "float4", "rawpos");
  // Transforms
  out.Write("// Position matrix\n"
            "float4 P0;\n"
            "float4 P1;\n"
            "float4 P2;\n"
            "\n"
            "// Normal matrix\n"
            "float3 N0;\n"
            "float3 N1;\n"
            "float3 N2;\n"
            "\n"
            "if ((components & {}u) != 0u) {{ // VB_HAS_POSMTXIDX\n",
            Common::ToUnderlying(VB_HAS_POSMTXIDX));
  LoadVertexAttribute(out, host_config, 2, "posmtx", "uint4", "ubyte4");
  out.Write("  // Vertex format has a per-vertex matrix\n"
            "  int posidx = int(posmtx.r);\n"
            "  P0 = " I_TRANSFORMMATRICES "[posidx];\n"
            "  P1 = " I_TRANSFORMMATRICES "[posidx+1];\n"
            "  P2 = " I_TRANSFORMMATRICES "[posidx+2];\n"
            "\n"
            "  int normidx = posidx >= 32 ? (posidx - 32) : posidx;\n"
            "  N0 = " I_NORMALMATRICES "[normidx].xyz;\n"
            "  N1 = " I_NORMALMATRICES "[normidx+1].xyz;\n"
            "  N2 = " I_NORMALMATRICES "[normidx+2].xyz;\n"
            "}} else {{\n"
            "  // One shared matrix\n"
            "  P0 = " I_POSNORMALMATRIX "[0];\n"
            "  P1 = " I_POSNORMALMATRIX "[1];\n"
            "  P2 = " I_POSNORMALMATRIX "[2];\n"
            "  N0 = " I_POSNORMALMATRIX "[3].xyz;\n"
            "  N1 = " I_POSNORMALMATRIX "[4].xyz;\n"
            "  N2 = " I_POSNORMALMATRIX "[5].xyz;\n"
            "}}\n"
            "\n"
            "// Multiply the position vector by the position matrix\n"
            "float4 pos = float4(dot(P0, rawpos), dot(P1, rawpos), dot(P2, rawpos), 1.0);\n"
            "o.pos = float4(dot(" I_PROJECTION "[0], pos), dot(" I_PROJECTION
            "[1], pos), dot(" I_PROJECTION "[2], pos), dot(" I_PROJECTION "[3], pos));\n"
            "\n"
            "// The scale of the transform matrix is used to control the size of the emboss map\n"
            "// effect by changing the scale of the transformed binormals (which only get used by\n"
            "// emboss map texgens). By normalising the first transformed normal (which is used\n"
            "// by lighting calculations and needs to be unit length), the same transform matrix\n"
            "// can do double duty, scaling for emboss mapping, and not scaling for lighting.\n"
            "float3 _normal = float3(0.0, 0.0, 0.0);\n"
            "if ((components & {}u) != 0u) // VB_HAS_NORMAL\n"
            "{{\n",
            Common::ToUnderlying(VB_HAS_NORMAL));
  LoadVertexAttribute(out, host_config, 2, "rawnormal", "float3", "float3");
  out.Write("  _normal = normalize(float3(dot(N0, rawnormal), dot(N1, rawnormal), dot(N2, "
            "rawnormal)));\n"
            "}}\n"
            "\n"
            "float3 _tangent = float3(0.0, 0.0, 0.0);\n"
            "if ((components & {}u) != 0u) // VB_HAS_TANGENT\n"
            "{{\n",
            Common::ToUnderlying(VB_HAS_TANGENT));
  LoadVertexAttribute(out, host_config, 2, "rawtangent", "float3", "float3");
  out.Write("  _tangent = float3(dot(N0, rawtangent), dot(N1, rawtangent), dot(N2, rawtangent));\n"
            "}}\n"
            "else\n"
            "{{\n"
            "  _tangent = float3(dot(N0, " I_CACHED_TANGENT ".xyz), dot(N1, " I_CACHED_TANGENT
            ".xyz), dot(N2, " I_CACHED_TANGENT ".xyz));\n"
            "}}\n"
            "\n"
            "float3 _binormal = float3(0.0, 0.0, 0.0);\n"
            "if ((components & {}u) != 0u) // VB_HAS_BINORMAL\n"
            "{{\n",
            Common::ToUnderlying(VB_HAS_BINORMAL));
  LoadVertexAttribute(out, host_config, 2, "rawbinormal", "float3", "float3");
  out.Write("  _binormal = float3(dot(N0, rawbinormal), dot(N1, rawbinormal), dot(N2, "
            "rawbinormal));\n"
            "}}\n"
            "else\n"
            "{{\n"
            "  _binormal = float3(dot(N0, " I_CACHED_BINORMAL ".xyz), dot(N1, " I_CACHED_BINORMAL
            ".xyz), dot(N2, " I_CACHED_BINORMAL ".xyz));\n"
            "}}\n"
            "\n");

  // Hardware Lighting
  out.Write("// xfmem.numColorChans controls the number of color channels available to TEV,\n"
            "// but we still need to generate all channels here, as it can be used in texgen.\n"
            "// Cel-damage is an example of this.\n"
            "float4 vertex_color_0, vertex_color_1;\n"
            "\n");
  out.Write("// To use color 1, the vertex descriptor must have color 0 and 1.\n"
            "// If color 1 is present but not color 0, it is used for lighting channel 0.\n"
            "bool use_color_1 = ((components & {0}u) == {0}u); // VB_HAS_COL0 | VB_HAS_COL1\n",
            static_cast<u32>(VB_HAS_COL0 | VB_HAS_COL1));

  out.Write("if ((components & {0}u) == {0}u) // VB_HAS_COL0 | VB_HAS_COL1\n"
            "{{\n",
            static_cast<u32>(VB_HAS_COL0 | VB_HAS_COL1));
  LoadVertexAttribute(out, host_config, 2, "rawcolor0", "float4", "ubyte4");
  LoadVertexAttribute(out, host_config, 2, "rawcolor1", "float4", "ubyte4");
  out.Write("  vertex_color_0 = rawcolor0;\n"
            "  vertex_color_1 = rawcolor1;\n"
            "}}\n"
            "else if ((components & {}u) != 0u) // VB_HAS_COL0\n"
            "{{\n",
            Common::ToUnderlying(VB_HAS_COL0));
  LoadVertexAttribute(out, host_config, 2, "rawcolor0", "float4", "ubyte4");
  out.Write("  vertex_color_0 = rawcolor0;\n"
            "  vertex_color_1 = rawcolor0;\n"
            "}}\n"
            "else if ((components & {}u) != 0u) // VB_HAS_COL1\n"
            "{{\n",
            Common::ToUnderlying(VB_HAS_COL1));
  LoadVertexAttribute(out, host_config, 2, "rawcolor1", "float4", "ubyte4");
  out.Write("  vertex_color_0 = rawcolor1;\n"
            "  vertex_color_1 = rawcolor1;\n"
            "}}\n"
            "else\n"
            "{{\n"
            "  vertex_color_0 = missing_color_value;\n"
            "  vertex_color_1 = missing_color_value;\n"
            "}}\n");

  WriteVertexLighting(out, api_type, "pos.xyz", "_normal", "vertex_color_0", "vertex_color_1",
                      "o.colors_0", "o.colors_1");

  // Texture Coordinates
  if (num_texgen > 0)
    GenVertexShaderTexGens(api_type, host_config, num_texgen, out);

  if (host_config.backend_vs_point_line_expand)
  {
    out.Write("if (vs_expand == {}u) {{ // Line\n", Common::ToUnderlying(VSExpand::Line));
    out.Write("  bool is_bottom = (gl_VertexID & 2) != 0;\n"
              "  bool is_right = (gl_VertexID & 1) != 0;\n"
              "  uint other_base_offset = vertex_base_offset;\n"
              "  if (is_bottom) {{\n"
              "    other_base_offset -= vertex_stride;\n"
              "  }} else {{\n"
              "    other_base_offset += vertex_stride;\n"
              "  }}\n"
              "  float4 other_rawpos = load_input_float4_rawpos(other_base_offset, "
              "vertex_offset_rawpos);\n"
              "  float4 other_p0 = P0;\n"
              "  float4 other_p1 = P1;\n"
              "  float4 other_p2 = P2;\n"
              "  if ((components & {}u) != 0u) {{ // VB_HAS_POSMTXIDX\n",
              Common::ToUnderlying(VB_HAS_POSMTXIDX));
    out.Write("    uint other_posidx = load_input_uint4_ubyte4(other_base_offset, "
              "vertex_offset_posmtx).r;\n"
              "    other_p0 = " I_TRANSFORMMATRICES "[other_posidx];\n"
              "    other_p1 = " I_TRANSFORMMATRICES "[other_posidx+1];\n"
              "    other_p2 = " I_TRANSFORMMATRICES "[other_posidx+2];\n"
              "  }}\n"
              "  float4 other_pos = float4(dot(other_p0, other_rawpos), "
              "dot(other_p1, other_rawpos), dot(other_p2, other_rawpos), 1.0);\n");
    GenerateVSLineExpansion(out, "  ", num_texgen);
    out.Write("}} else if (vs_expand == {}u) {{ // Point\n", Common::ToUnderlying(VSExpand::Point));
    out.Write("  bool is_bottom = (gl_VertexID & 2) != 0;\n"
              "  bool is_right = (gl_VertexID & 1) != 0;\n");
    GenerateVSPointExpansion(out, "  ", num_texgen);
    out.Write("}}\n");
  }

  if (per_pixel_lighting)
  {
    out.Write("// When per-pixel lighting is enabled, the vertex colors are passed through\n"
              "// unmodified so we can evaluate the lighting in the pixel shader.\n"
              "// Lighting is also still computed in the vertex shader since it can be used to\n"
              "// generate texture coordinates. We generated them above, so now the colors can\n"
              "// be reverted to their previous stage.\n"
              "o.colors_0 = vertex_color_0;\n"
              "o.colors_1 = vertex_color_1;\n"
              "// Note that the numColorChans logic should be (but currently isn't)\n"
              "// performed in the pixel shader.\n");
  }
  else
  {
    out.Write("// The number of colors available to TEV is determined by numColorChans.\n"
              "// We have to provide the fields to match the interface, so set to zero\n"
              "// if it's not enabled.\n"
              "if (xfmem_numColorChans == 0u)\n"
              "  o.colors_0 = float4(0.0, 0.0, 0.0, 0.0);\n"
              "if (xfmem_numColorChans <= 1u)\n"
              "  o.colors_1 = float4(0.0, 0.0, 0.0, 0.0);\n");
  }

  if (!host_config.fast_depth_calc)
  {
    // clipPos/w needs to be done in pixel shader, not here
    out.Write("o.clipPos = o.pos;\n");
  }

  if (per_pixel_lighting)
  {
    out.Write("o.Normal = _normal;\n"
              "o.WorldPos = pos.xyz;\n");
  }

  // If we can disable the incorrect depth clipping planes using depth clamping, then we can do
  // our own depth clipping and calculate the depth range before the perspective divide if
  // necessary.
  if (host_config.backend_depth_clamp)
  {
    // Since we're adjusting z for the depth range before the perspective divide, we have to do our
    // own clipping. We want to clip so that -w <= z <= 0, which matches the console -1..0 range.
    // We adjust our depth value for clipping purposes to match the perspective projection in the
    // software backend, which is a hack to fix Sonic Adventure and Unleashed games.
    out.Write("float clipDepth = o.pos.z * (1.0 - 1e-7);\n"
              "float clipDist0 = clipDepth + o.pos.w;\n"  // Near: z < -w
              "float clipDist1 = -clipDepth;\n");         // Far: z > 0
    if (host_config.backend_geometry_shaders)
    {
      out.Write("o.clipDist0 = clipDist0;\n"
                "o.clipDist1 = clipDist1;\n");
    }
  }

  // Write the true depth value. If the game uses depth textures, then the pixel shader will
  // override it with the correct values if not then early z culling will improve speed.
  // There are two different ways to do this, when the depth range is oversized, we process
  // the depth range in the vertex shader, if not we let the host driver handle it.
  //
  // Adjust z for the depth range. We're using an equation which incorperates a depth inversion,
  // so we can map the console -1..0 range to the 0..1 range used in the depth buffer.
  // We have to handle the depth range in the vertex shader instead of after the perspective
  // divide, because some games will use a depth range larger than what is allowed by the
  // graphics API. These large depth ranges will still be clipped to the 0..1 range, so these
  // games effectively add a depth bias to the values written to the depth buffer.
  out.Write("o.pos.z = o.pos.w * " I_PIXELPOSITIONCORRECTION ".w - "
            "o.pos.z * " I_PIXELPOSITIONCORRECTION ".z;\n");

  if (!host_config.backend_clip_control)
  {
    // If the graphics API doesn't support a depth range of 0..1, then we need to map z to
    // the -1..1 range. Unfortunately we have to use a substraction, which is a lossy floating-point
    // operation that can introduce a round-trip error.
    out.Write("o.pos.z = o.pos.z * 2.0 - o.pos.w;\n");
  }

  // Correct for negative viewports by mirroring all vertices. We need to negate the height here,
  // since the viewport height is already negated by the render backend.
  out.Write("o.pos.xy *= sign(" I_PIXELPOSITIONCORRECTION ".xy * float2(1.0, -1.0));\n");

  if (vertex_rounding)
  {
    // By now our position is in clip space. However, higher resolutions than the Wii outputs
    // cause an additional pixel offset. Due to a higher pixel density we need to correct this
    // by converting our clip-space position into the Wii's screen-space.
    // Acquire the right pixel and then convert it back.
    out.Write("if (o.pos.w == 1.0f)\n"
              "{{\n");

    out.Write("\tfloat ss_pixel_x = ((o.pos.x + 1.0f) * (" I_VIEWPORT_SIZE ".x * 0.5f));\n"
              "\tfloat ss_pixel_y = ((o.pos.y + 1.0f) * (" I_VIEWPORT_SIZE ".y * 0.5f));\n");

    out.Write("\tss_pixel_x = round(ss_pixel_x);\n"
              "\tss_pixel_y = round(ss_pixel_y);\n");

    out.Write("\to.pos.x = ((ss_pixel_x / (" I_VIEWPORT_SIZE ".x * 0.5f)) - 1.0f);\n"
              "\to.pos.y = ((ss_pixel_y / (" I_VIEWPORT_SIZE ".y * 0.5f)) - 1.0f);\n"
              "}}\n");
  }

  if (host_config.backend_geometry_shaders)
  {
    AssignVSOutputMembers(out, "vs", "o", num_texgen, host_config);
  }
  else
  {
    // TODO: Pass interface blocks between shader stages even if geometry shaders
    // are not supported, however that will require at least OpenGL 3.2 support.
    for (u32 i = 0; i < num_texgen; ++i)
      out.Write("tex{}.xyz = o.tex{};\n", i, i);
    if (!host_config.fast_depth_calc)
      out.Write("clipPos = o.clipPos;\n");
    if (per_pixel_lighting)
    {
      out.Write("Normal = o.Normal;\n"
                "WorldPos = o.WorldPos;\n");
    }
    out.Write("colors_0 = o.colors_0;\n"
              "colors_1 = o.colors_1;\n");
  }

  if (host_config.backend_depth_clamp)
  {
    out.Write("gl_ClipDistance[0] = clipDist0;\n"
              "gl_ClipDistance[1] = clipDist1;\n");
  }

  // Vulkan NDC space has Y pointing down (right-handed NDC space).
  if (api_type == APIType::Vulkan)
    out.Write("gl_Position = float4(o.pos.x, -o.pos.y, o.pos.z, o.pos.w);\n");
  else
    out.Write("gl_Position = o.pos;\n");
  out.Write("}}\n");

  return out;
}

static void GenVertexShaderTexGens(APIType api_type, const ShaderHostConfig& host_config,
                                   u32 num_texgen, ShaderCode& out)
{
  // The HLSL compiler complains that the output texture coordinates are uninitialized when trying
  // to dynamically index them.
  for (u32 i = 0; i < num_texgen; i++)
    out.Write("o.tex{} = float3(0.0, 0.0, 0.0);\n", i);

  out.Write("// Texture coordinate generation\n");
  if (num_texgen == 1)
  {
    out.Write("{{ const uint texgen = 0u;\n");
  }
  else
  {
    out.Write("for (uint texgen = 0u; texgen < {}u; texgen++) {{\n", num_texgen);
  }

  out.Write("  // Texcoord transforms\n");
  out.Write("  float4 coord = float4(0.0, 0.0, 1.0, 1.0);\n"
            "  uint texMtxInfo = xfmem_texMtxInfo(texgen);\n");
  out.Write("  switch ({}) {{\n", BitfieldExtract<&TexMtxInfo::sourcerow>("texMtxInfo"));
  out.Write("  case {:s}:\n", SourceRow::Geom);
  out.Write("    coord.xyz = rawpos.xyz;\n");
  out.Write("    break;\n\n");
  out.Write("  case {:s}:\n", SourceRow::Normal);
  out.Write("    if ((components & {}u) != 0u) // VB_HAS_NORMAL\n"
            "    {{\n",
            Common::ToUnderlying(VB_HAS_NORMAL));
  LoadVertexAttribute(out, host_config, 6, "rawnormal", "float3", "float3");
  out.Write("      coord.xyz = rawnormal.xyz;\n"
            "    }}\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", SourceRow::BinormalT);
  out.Write("    if ((components & {}u) != 0u) // VB_HAS_TANGENT\n"
            "    {{\n",
            Common::ToUnderlying(VB_HAS_TANGENT));
  LoadVertexAttribute(out, host_config, 6, "rawtangent", "float3", "float3");
  out.Write("      coord.xyz = rawtangent.xyz;\n"
            "    }}\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", SourceRow::BinormalB);
  out.Write("    if ((components & {}u) != 0u) // VB_HAS_BINORMAL\n"
            "    {{\n",
            Common::ToUnderlying(VB_HAS_BINORMAL));
  LoadVertexAttribute(out, host_config, 6, "rawbinormal", "float3", "float3");
  out.Write("      coord.xyz = rawbinormal.xyz;\n"
            "    }}\n"
            "    break;\n\n");
  for (u32 i = 0; i < 8; i++)
  {
    out.Write("  case {:s}:\n", static_cast<SourceRow>(Common::ToUnderlying(SourceRow::Tex0) + i));
    out.Write("    if ((components & {}u) != 0u) // VB_HAS_UV{}\n"
              "    {{\n",
              VB_HAS_UV0 << i, i);
    LoadVertexAttribute(out, host_config, 6, fmt::format("rawtex{}", i), "float3", "rawtex",
                        fmt::format("rawtex[{}][{}]", i / 4, i % 4));
    out.Write("      coord = float4(rawtex{}.x, rawtex{}.y, 1.0f, 1.0f);\n"
              "    }}\n",
              i, i);
    out.Write("    break;\n\n");
  }
  out.Write("  }}\n"
            "\n");

  out.Write("  // Input form of AB11 sets z element to 1.0\n");
  out.Write("  if ({} == {:s}) // inputform == AB11\n",
            BitfieldExtract<&TexMtxInfo::inputform>("texMtxInfo"), TexInputForm::AB11);
  out.Write("    coord.z = 1.0f;\n"
            "\n");

  // Convert NaNs to 1 - needed to fix eyelids in Shadow the Hedgehog during cutscenes
  // See https://bugs.dolphin-emu.org/issues/11458
  out.Write("  // Convert NaN to 1\n");
  out.Write("  if (dolphin_isnan(coord.x)) coord.x = 1.0;\n");
  out.Write("  if (dolphin_isnan(coord.y)) coord.y = 1.0;\n");
  out.Write("  if (dolphin_isnan(coord.z)) coord.z = 1.0;\n");

  out.Write("  // first transformation\n");
  out.Write("  uint texgentype = {};\n", BitfieldExtract<&TexMtxInfo::texgentype>("texMtxInfo"));
  out.Write("  float3 output_tex;\n"
            "  switch (texgentype)\n"
            "  {{\n");
  out.Write("  case {:s}:\n", TexGenType::EmbossMap);
  out.Write("    {{\n");
  out.Write("      uint light = {};\n",
            BitfieldExtract<&TexMtxInfo::embosslightshift>("texMtxInfo"));
  out.Write("      uint source = {};\n",
            BitfieldExtract<&TexMtxInfo::embosssourceshift>("texMtxInfo"));
  out.Write("      switch (source) {{\n");
  for (u32 i = 0; i < num_texgen; i++)
    out.Write("      case {}u: output_tex.xyz = o.tex{}; break;\n", i, i);
  out.Write("      default: output_tex.xyz = float3(0.0, 0.0, 0.0); break;\n"
            "      }}\n"
            "      float3 ldir = normalize(" I_LIGHTS "[light].pos.xyz - pos.xyz);\n"
            "      output_tex.xyz += float3(dot(ldir, _tangent), dot(ldir, _binormal), 0.0);\n"
            "    }}\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", TexGenType::Color0);
  out.Write("    output_tex.xyz = float3(o.colors_0.x, o.colors_0.y, 1.0);\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", TexGenType::Color1);
  out.Write("    output_tex.xyz = float3(o.colors_1.x, o.colors_1.y, 1.0);\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", TexGenType::Regular);
  out.Write("  default:\n"
            "    {{\n");
  out.Write("      if ((components & ({}u /* VB_HAS_TEXMTXIDX0 */ << texgen)) != 0u) {{\n",
            Common::ToUnderlying(VB_HAS_TEXMTXIDX0));
  if (host_config.backend_dynamic_vertex_loader || host_config.backend_vs_point_line_expand)
  {
    out.Write("        int tmp = int(load_input_float3_rawtex(vertex_base_offset, "
              "vertex_offset_rawtex[texgen / 4][texgen % 4]).z);\n"
              "\n");
  }
  else
  {
    out.Write(
        "        // This is messy, due to dynamic indexing of the input texture coordinates.\n"
        "        // Hopefully the compiler will unroll this whole loop anyway and the switch.\n"
        "        int tmp = 0;\n"
        "        switch (texgen) {{\n");
    for (u32 i = 0; i < num_texgen; i++)
      out.Write("        case {}u: tmp = int(rawtex{}.z); break;\n", i, i);
    out.Write("        }}\n"
              "\n");
  }
  out.Write("        if ({} == {:s}) {{\n", BitfieldExtract<&TexMtxInfo::projection>("texMtxInfo"),
            TexSize::STQ);
  out.Write("          output_tex.xyz = float3(dot(coord, " I_TRANSFORMMATRICES "[tmp]),\n"
            "                                  dot(coord, " I_TRANSFORMMATRICES "[tmp + 1]),\n"
            "                                  dot(coord, " I_TRANSFORMMATRICES "[tmp + 2]));\n"
            "        }} else {{\n"
            "          output_tex.xyz = float3(dot(coord, " I_TRANSFORMMATRICES "[tmp]),\n"
            "                                  dot(coord, " I_TRANSFORMMATRICES "[tmp + 1]),\n"
            "                                  1.0);\n"
            "        }}\n"
            "      }} else {{\n");
  out.Write("        if ({} == {:s}) {{\n", BitfieldExtract<&TexMtxInfo::projection>("texMtxInfo"),
            TexSize::STQ);
  out.Write("          output_tex.xyz = float3(dot(coord, " I_TEXMATRICES "[3u * texgen]),\n"
            "                                  dot(coord, " I_TEXMATRICES "[3u * texgen + 1u]),\n"
            "                                  dot(coord, " I_TEXMATRICES "[3u * texgen + 2u]));\n"
            "        }} else {{\n"
            "          output_tex.xyz = float3(dot(coord, " I_TEXMATRICES "[3u * texgen]),\n"
            "                                  dot(coord, " I_TEXMATRICES "[3u * texgen + 1u]),\n"
            "                                  1.0);\n"
            "        }}\n"
            "      }}\n"
            "    }}\n"
            "    break;\n\n"
            "  }}\n"
            "\n");

  out.Write("  if (xfmem_dualTexInfo != 0u) {{\n");
  out.Write("    uint postMtxInfo = xfmem_postMtxInfo(texgen);");
  out.Write("    uint base_index = {};\n", BitfieldExtract<&PostMtxInfo::index>("postMtxInfo"));
  out.Write("    float4 P0 = " I_POSTTRANSFORMMATRICES "[base_index & 0x3fu];\n"
            "    float4 P1 = " I_POSTTRANSFORMMATRICES "[(base_index + 1u) & 0x3fu];\n"
            "    float4 P2 = " I_POSTTRANSFORMMATRICES "[(base_index + 2u) & 0x3fu];\n"
            "\n");
  out.Write("    if ({} != 0u)\n", BitfieldExtract<&PostMtxInfo::normalize>("postMtxInfo"));
  out.Write("      output_tex.xyz = normalize(output_tex.xyz);\n"
            "\n"
            "    // multiply by postmatrix\n"
            "    output_tex.xyz = float3(dot(P0.xyz, output_tex.xyz) + P0.w,\n"
            "                            dot(P1.xyz, output_tex.xyz) + P1.w,\n"
            "                            dot(P2.xyz, output_tex.xyz) + P2.w);\n"
            "  }}\n\n");

  // When q is 0, the GameCube appears to have a special case
  // This can be seen in devkitPro's neheGX Lesson08 example for Wii
  // Makes differences in Rogue Squadron 3 (Hoth sky) and The Last Story (shadow culling)
  out.Write("  if (texgentype == {:s} && output_tex.z == 0.0)\n", TexGenType::Regular);
  out.Write(
      "    output_tex.xy = clamp(output_tex.xy / 2.0f, float2(-1.0f,-1.0f), float2(1.0f,1.0f));\n"
      "\n");

  out.Write("  // Hopefully GPUs that can support dynamic indexing will optimize this.\n");
  out.Write("  switch (texgen) {{\n");
  for (u32 i = 0; i < num_texgen; i++)
    out.Write("  case {}u: o.tex{} = output_tex; break;\n", i, i);
  out.Write("  }}\n"
            "}}\n");
}

static void LoadVertexAttribute(ShaderCode& code, const ShaderHostConfig& host_config, u32 indent,
                                std::string_view name, std::string_view shader_type,
                                std::string_view stored_type, std::string_view offset_name)
{
  if (host_config.backend_dynamic_vertex_loader || host_config.backend_vs_point_line_expand)
  {
    code.Write("{:{}}{} {} = load_input_{}_{}(vertex_base_offset, vertex_offset_{});\n", "", indent,
               shader_type, name, shader_type, stored_type,
               offset_name.empty() ? name : offset_name);
  }
  // else inputs are always available
}

void EnumerateVertexShaderUids(const std::function<void(const VertexShaderUid&)>& callback)
{
  VertexShaderUid uid;

  for (u32 texgens = 0; texgens <= 8; texgens++)
  {
    vertex_ubershader_uid_data* const vuid = uid.GetUidData();
    vuid->num_texgens = texgens;
    callback(uid);
  }
}
}  // namespace UberShader
