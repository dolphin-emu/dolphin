#include "VideoCommon/FramebufferShaderGen.h"
#include <sstream>
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexShaderGen.h"

namespace FramebufferShaderGen
{
static APIType GetAPIType()
{
  return g_ActiveConfig.backend_info.api_type;
}

static void EmitUniformBufferDeclaration(std::stringstream& ss)
{
  if (GetAPIType() == APIType::D3D)
    ss << "cbuffer UBO : register(b0)\n";
  else
    ss << "UBO_BINDING(std140, 1) uniform UBO\n";
}

static void EmitSamplerDeclarations(std::stringstream& ss, u32 start = 0, u32 end = 1,
                                    bool multisampled = false)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  {
    for (u32 i = start; i < end; i++)
    {
      ss << (multisampled ? "Texture2DMSArray<float4>" : "Texture2DArray<float4>") << " tex" << i
         << " : register(t" << i << ");\n";
      ss << "SamplerState"
         << " samp" << i << " : register(s" << i << ");\n";
    }
  }
  break;

  case APIType::OpenGL:
  case APIType::Vulkan:
  {
    for (u32 i = start; i < end; i++)
    {
      ss << "SAMPLER_BINDING(" << i << ") uniform "
         << (multisampled ? "sampler2DMSArray" : "sampler2DArray") << " samp" << i << ";\n";
    }
  }
  break;
  default:
    break;
  }
}

static void EmitSampleTexture(std::stringstream& ss, u32 n, const char* coords)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
    ss << "tex" << n << ".Sample(samp" << n << ", " << coords << ")";
    break;

  case APIType::OpenGL:
  case APIType::Vulkan:
    ss << "texture(samp" << n << ", " << coords << ")";
    break;

  default:
    break;
  }
}

// Emits a texel fetch/load instruction. Assumes that "coords" is a 4-element vector, with z
// containing the layer, and w containing the mipmap level.
static void EmitTextureLoad(std::stringstream& ss, u32 n, const char* coords)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
    ss << "tex" << n << ".Load(" << coords << ")";
    break;

  case APIType::OpenGL:
  case APIType::Vulkan:
    ss << "texelFetch(samp" << n << ", (" << coords << ").xyz, (" << coords << ").w)";
    break;

  default:
    break;
  }
}

static void EmitVertexMainDeclaration(std::stringstream& ss, u32 num_tex_inputs,
                                      u32 num_color_inputs, bool position_input,
                                      u32 num_tex_outputs, u32 num_color_outputs,
                                      const char* extra_inputs = "")
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  {
    ss << "void main(";
    for (u32 i = 0; i < num_tex_inputs; i++)
      ss << "in float3 rawtex" << i << " : TEXCOORD" << i << ", ";
    for (u32 i = 0; i < num_color_inputs; i++)
      ss << "in float4 rawcolor" << i << " : COLOR" << i << ", ";
    if (position_input)
      ss << "in float4 rawpos : POSITION, ";
    ss << extra_inputs;
    for (u32 i = 0; i < num_tex_outputs; i++)
      ss << "out float3 v_tex" << i << " : TEXCOORD" << i << ", ";
    for (u32 i = 0; i < num_color_outputs; i++)
      ss << "out float4 v_col" << i << " : COLOR" << i << ", ";
    ss << "out float4 opos : SV_Position)\n";
  }
  break;

  case APIType::OpenGL:
  case APIType::Vulkan:
  {
    for (u32 i = 0; i < num_tex_inputs; i++)
      ss << "ATTRIBUTE_LOCATION(" << (SHADER_TEXTURE0_ATTRIB + i) << ") in float3 rawtex" << i
         << ";\n";
    for (u32 i = 0; i < num_color_inputs; i++)
      ss << "ATTRIBUTE_LOCATION(" << (SHADER_COLOR0_ATTRIB + i) << ") in float4 rawcolor" << i
         << ";\n";
    if (position_input)
      ss << "ATTRIBUTE_LOCATION(" << SHADER_POSITION_ATTRIB << ") in float4 rawpos;\n";

    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      ss << "VARYING_LOCATION(0) out VertexData {\n";
      for (u32 i = 0; i < num_tex_outputs; i++)
        ss << "  float3 v_tex" << i << ";\n";
      for (u32 i = 0; i < num_color_outputs; i++)
        ss << "  float4 v_col" << i << ";\n";
      ss << "};\n";
    }
    else
    {
      for (u32 i = 0; i < num_tex_outputs; i++)
        ss << "VARYING_LOCATION(" << i << ") out float3 v_tex" << i << ";\n";
      for (u32 i = 0; i < num_color_outputs; i++)
        ss << "VARYING_LOCATION(" << (num_tex_inputs + i) << ") out float4 v_col" << i << ";\n";
    }
    ss << "#define opos gl_Position\n";
    ss << extra_inputs << "\n";
    ss << "void main()\n";
  }
  break;
  default:
    break;
  }
}

static void EmitPixelMainDeclaration(std::stringstream& ss, u32 num_tex_inputs,
                                     u32 num_color_inputs, const char* output_type = "float4",
                                     const char* extra_vars = "", bool emit_frag_coord = false)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  {
    ss << "void main(";
    for (u32 i = 0; i < num_tex_inputs; i++)
      ss << "in float3 v_tex" << i << " : TEXCOORD" << i << ", ";
    for (u32 i = 0; i < num_color_inputs; i++)
      ss << "in float4 v_col" << i << " : COLOR" << i << ", ";
    if (emit_frag_coord)
      ss << "in float4 frag_coord : SV_Position, ";
    ss << extra_vars << "out " << output_type << " ocol0 : SV_Target)\n";
  }
  break;

  case APIType::OpenGL:
  case APIType::Vulkan:
  {
    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      ss << "VARYING_LOCATION(0) in VertexData {\n";
      for (u32 i = 0; i < num_tex_inputs; i++)
        ss << "  in float3 v_tex" << i << ";\n";
      for (u32 i = 0; i < num_color_inputs; i++)
        ss << "  in float4 v_col" << i << ";\n";
      ss << "};\n";
    }
    else
    {
      for (u32 i = 0; i < num_tex_inputs; i++)
        ss << "VARYING_LOCATION(" << i << ") in float3 v_tex" << i << ";\n";
      for (u32 i = 0; i < num_color_inputs; i++)
        ss << "VARYING_LOCATION(" << (num_tex_inputs + i) << ") in float4 v_col" << i << ";\n";
    }

    ss << "FRAGMENT_OUTPUT_LOCATION(0) out " << output_type << " ocol0;\n";
    ss << extra_vars << "\n";
    if (emit_frag_coord)
      ss << "#define frag_coord gl_FragCoord\n";
    ss << "void main()\n";
  }
  break;

  default:
    break;
  }
}

std::string GenerateScreenQuadVertexShader()
{
  std::stringstream ss;
  EmitVertexMainDeclaration(ss, 0, 0, false, 1, 0,
                            GetAPIType() == APIType::D3D ? "in uint id : SV_VertexID, " :
                                                           "#define id gl_VertexID\n");
  ss << "{\n";
  ss << "  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n";
  ss << "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n";

  // NDC space is flipped in Vulkan. We also flip in GL so that (0,0) is in the lower-left.
  if (GetAPIType() == APIType::Vulkan || GetAPIType() == APIType::OpenGL)
    ss << "  opos.y = -opos.y;\n";

  ss << "}\n";

  return ss.str();
}

std::string GenerateTextureCopyVertexShader()
{
  std::stringstream ss;
  EmitUniformBufferDeclaration(ss);
  ss << "{";
  ss << "  float2 src_offset;\n";
  ss << "  float2 src_size;\n";
  ss << "};\n\n";

  EmitVertexMainDeclaration(ss, 0, 0, false, 1, 0,
                            GetAPIType() == APIType::D3D ? "in uint id : SV_VertexID, " :
                                                           "#define id gl_VertexID");
  ss << "{\n";
  ss << "  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n";
  ss << "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n";
  ss << "  v_tex0 = float3(src_offset + (src_size * v_tex0.xy), 0.0f);\n";

  // NDC space is flipped in Vulkan. We also flip in GL so that (0,0) is in the lower-left.
  if (GetAPIType() == APIType::Vulkan || GetAPIType() == APIType::OpenGL)
    ss << "  opos.y = -opos.y;\n";

  ss << "}\n";

  return ss.str();
}

std::string GenerateTextureCopyPixelShader()
{
  std::stringstream ss;
  EmitSamplerDeclarations(ss, 0, 1, false);
  EmitPixelMainDeclaration(ss, 1, 0);
  ss << "{\n";
  ss << "  ocol0 = ";
  EmitSampleTexture(ss, 0, "v_tex0");
  ss << ";\n";
  ss << "}\n";
  return ss.str();
}

std::string GenerateColorPixelShader()
{
  std::stringstream ss;
  EmitPixelMainDeclaration(ss, 0, 1);
  ss << "{\n";
  ss << "  ocol0 = v_col0;\n";
  ss << "}\n";
  return ss.str();
}

std::string GenerateResolveDepthPixelShader(u32 samples)
{
  std::stringstream ss;
  EmitSamplerDeclarations(ss, 0, 1, true);
  EmitPixelMainDeclaration(ss, 1, 0, "float",
                           GetAPIType() == APIType::D3D ? "in float4 ipos : SV_Position, " : "");
  ss << "{\n";
  ss << "  int layer = int(v_tex0.z);\n";
  if (GetAPIType() == APIType::D3D)
    ss << "  int3 coords = int3(int2(ipos.xy), layer);\n";
  else
    ss << "  int3 coords = int3(int2(gl_FragCoord.xy), layer);\n";

  // Take the minimum of all depth samples.
  if (GetAPIType() == APIType::D3D)
    ss << "  ocol0 = tex0.Load(coords, 0).r;\n";
  else
    ss << "  ocol0 = texelFetch(samp0, coords, 0).r;\n";
  ss << "  for (int i = 1; i < " << samples << "; i++)\n";
  if (GetAPIType() == APIType::D3D)
    ss << "    ocol0 = min(ocol0, tex0.Load(coords, i).r);\n";
  else
    ss << "    ocol0 = min(ocol0, texelFetch(samp0, coords, i).r);\n";

  ss << "}\n";
  return ss.str();
}

std::string GenerateClearVertexShader()
{
  std::stringstream ss;
  EmitUniformBufferDeclaration(ss);
  ss << "{\n";
  ss << "  float4 clear_color;\n";
  ss << "  float clear_depth;\n";
  ss << "};\n";

  EmitVertexMainDeclaration(ss, 0, 0, false, 0, 1,
                            GetAPIType() == APIType::D3D ? "in uint id : SV_VertexID, " :
                                                           "#define id gl_VertexID\n");
  ss << "{\n";
  ss << "  float2 coord = float2(id & 1, id & 2);\n";
  ss << "  opos = float4(coord * 4.0f - 1.0f, clear_depth, 1.0f);\n";
  ss << "  v_col0 = clear_color;\n";

  // NDC space is flipped in Vulkan
  if (GetAPIType() != APIType::Vulkan)
    ss << "  opos.y = -opos.y;\n";

  ss << "}\n";

  return ss.str();
}

std::string GenerateEFBPokeVertexShader()
{
  std::stringstream ss;
  EmitVertexMainDeclaration(ss, 0, 1, true, 0, 1);
  ss << "{\n";
  ss << "  v_col0 = rawcolor0;\n";
  ss << "  opos = float4(rawpos.xyz, 1.0f);\n";
  if (g_ActiveConfig.backend_info.bSupportsLargePoints)
    ss << "  gl_PointSize = rawpos.w;\n";

  // NDC space is flipped in Vulkan.
  if (GetAPIType() == APIType::Vulkan)
    ss << "  opos.y = -opos.y;\n";

  ss << "}\n";
  return ss.str();
}

std::string GenerateFormatConversionShader(EFBReinterpretType convtype, u32 samples)
{
  std::stringstream ss;
  EmitSamplerDeclarations(ss, 0, 1, samples > 1);
  EmitPixelMainDeclaration(
      ss, 1, 0, "float4",
      GetAPIType() == APIType::D3D ?
          (g_ActiveConfig.bSSAA ?
               "in float4 ipos : SV_Position, in uint isample : SV_SampleIndex, " :
               "in float4 ipos : SV_Position, ") :
          "");
  ss << "{\n";
  ss << "  int layer = int(v_tex0.z);\n";
  if (GetAPIType() == APIType::D3D)
    ss << "  int3 coords = int3(int2(ipos.xy), layer);\n";
  else
    ss << "  int3 coords = int3(int2(gl_FragCoord.xy), layer);\n";

  if (samples == 1)
  {
    // No MSAA at all.
    if (GetAPIType() == APIType::D3D)
      ss << "  float4 val = tex0.Load(int4(coords, 0));\n";
    else
      ss << "  float4 val = texelFetch(samp0, coords, 0);\n";
  }
  else if (g_ActiveConfig.bSSAA)
  {
    // Sample shading, shader runs once per sample
    if (GetAPIType() == APIType::D3D)
      ss << "  float4 val = tex0.Load(coords, isample);";
    else
      ss << "  float4 val = texelFetch(samp0, coords, gl_SampleID);";
  }
  else
  {
    // MSAA without sample shading, average out all samples.
    ss << "  float4 val = float4(0.0f, 0.0f, 0.0f, 0.0f);\n";
    ss << "  for (int i = 0; i < " << samples << "; i++)\n";
    if (GetAPIType() == APIType::D3D)
      ss << "    val += tex0.Load(coords, i);\n";
    else
      ss << "    val += texelFetch(samp0, coords, i);\n";
    ss << "  val /= float(" << samples << ");\n";
  }

  switch (convtype)
  {
  case EFBReinterpretType::RGB8ToRGBA6:
    ss << "  int4 src8 = int4(round(val * 255.f));\n";
    ss << "  int4 dst6;\n";
    ss << "  dst6.r = src8.r >> 2;\n";
    ss << "  dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n";
    ss << "  dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n";
    ss << "  dst6.a = src8.b & 0x3F;\n";
    ss << "  ocol0 = float4(dst6) / 63.f;\n";
    break;

  case EFBReinterpretType::RGB8ToRGB565:
    ss << "  ocol0 = val;\n";
    break;

  case EFBReinterpretType::RGBA6ToRGB8:
    ss << "  int4 src6 = int4(round(val * 63.f));\n";
    ss << "  int4 dst8;\n";
    ss << "  dst8.r = (src6.r << 2) | (src6.g >> 4);\n";
    ss << "  dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n";
    ss << "  dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n";
    ss << "  dst8.a = 255;\n";
    ss << "  ocol0 = float4(dst8) / 255.f;\n";
    break;

  case EFBReinterpretType::RGBA6ToRGB565:
    ss << "  ocol0 = val;\n";
    break;

  case EFBReinterpretType::RGB565ToRGB8:
    ss << "  ocol0 = val;\n";
    break;

  case EFBReinterpretType::RGB565ToRGBA6:
    //
    ss << "  ocol0 = val;\n";
    break;
  }

  ss << "}\n";
  return ss.str();
}

std::string GenerateTextureReinterpretShader(TextureFormat from_format, TextureFormat to_format)
{
  std::stringstream ss;
  EmitSamplerDeclarations(ss, 0, 1, false);
  EmitPixelMainDeclaration(ss, 1, 0, "float4", "", true);
  ss << "{\n";
  ss << "  int layer = int(v_tex0.z);\n";
  ss << "  int4 coords = int4(int2(frag_coord.xy), layer, 0);\n";

  // Convert to a 32-bit value encompassing all channels, filling the most significant bits with
  // zeroes.
  ss << "  uint raw_value;\n";
  switch (from_format)
  {
  case TextureFormat::I8:
  case TextureFormat::C8:
  {
    ss << "  float4 temp_value = ";
    EmitTextureLoad(ss, 0, "coords");
    ss << ";\n";
    ss << "  raw_value = uint(temp_value.r * 255.0);\n";
  }
  break;

  case TextureFormat::IA8:
  {
    ss << "  float4 temp_value = ";
    EmitTextureLoad(ss, 0, "coords");
    ss << ";\n";
    ss << "  raw_value = uint(temp_value.r * 255.0) | (uint(temp_value.a * 255.0) << 8);\n";
  }
  break;

  case TextureFormat::IA4:
  {
    ss << "  float4 temp_value = ";
    EmitTextureLoad(ss, 0, "coords");
    ss << ";\n";
    ss << "  raw_value = uint(temp_value.r * 15.0) | (uint(temp_value.a * 15.0) << 4);\n";
  }
  break;

  case TextureFormat::RGB565:
  {
    ss << "  float4 temp_value = ";
    EmitTextureLoad(ss, 0, "coords");
    ss << ";\n";
    ss << "  raw_value = uint(temp_value.b * 31.0) | (uint(temp_value.g * 63.0) << 5) |\n";
    ss << "              (uint(temp_value.r * 31.0) << 11);\n";
  }
  break;

  case TextureFormat::RGB5A3:
  {
    ss << "  float4 temp_value = ";
    EmitTextureLoad(ss, 0, "coords");
    ss << ";\n";

    // 0.8784 = 224 / 255 which is the maximum alpha value that can be represented in 3 bits
    ss << "  if (temp_value.a > 0.878f) {\n";
    ss << "    raw_value = (uint(temp_value.b * 31.0)) | (uint(temp_value.g * 31.0) << 5) |\n";
    ss << "                (uint(temp_value.r * 31.0) << 10) | 0x8000u;\n";
    ss << "  } else {\n";
    ss << "     raw_value = (uint(temp_value.b * 15.0)) | (uint(temp_value.g * 15.0) << 4) |\n";
    ss << "                 (uint(temp_value.r * 15.0) << 8) | (uint(temp_value.a * 7.0) << 12);\n";
    ss << "  }\n";
  }
  break;
  }

  // Now convert it to its new representation.
  switch (to_format)
  {
  case TextureFormat::I8:
  case TextureFormat::C8:
  {
    ss << "  ocol0 = float4(float(raw_value & 0xFFu) / 255.0);\n";
  }
  break;

  case TextureFormat::IA8:
  {
    ss << "  float orgb = float(raw_value & 0xFFu) / 255.0;\n";
    ss << "  ocol0 = float4(orgb, orgb, orgb, float((raw_value >> 8) & 0xFFu) / 255.0);\n";
  }
  break;

  case TextureFormat::IA4:
  {
    ss << "  float orgb = float(raw_value & 0xFu) / 15.0;\n";
    ss << "  ocol0 = float4(orgb, orgb, orgb, float((raw_value >> 4) & 0xFu) / 15.0);\n";
  }
  break;

  case TextureFormat::RGB565:
  {
    ss << "  ocol0 = float4(float((raw_value >> 10) & 0x1Fu) / 31.0,\n";
    ss << "                 float((raw_value >> 5) & 0x1Fu) / 31.0,\n";
    ss << "                 float(raw_value & 0x1Fu) / 31.0, 1.0);\n";
  }
  break;

  case TextureFormat::RGB5A3:
  {
    ss << "  if ((raw_value & 0x8000u) != 0u) {\n";
    ss << "    ocol0 = float4(float((raw_value >> 10) & 0x1Fu) / 31.0,\n";
    ss << "                   float((raw_value >> 5) & 0x1Fu) / 31.0,\n";
    ss << "                   float(raw_value & 0x1Fu) / 31.0, 1.0);\n";
    ss << "  } else {\n";
    ss << "    ocol0 = float4(float((raw_value >> 8) & 0x0Fu) / 15.0,\n";
    ss << "                   float((raw_value >> 4) & 0x0Fu) / 15.0,\n";
    ss << "                   float(raw_value & 0x0Fu) / 15.0,\n";
    ss << "                   float((raw_value >> 12) & 0x07u) / 7.0);\n";
    ss << "  }\n";
  }
  break;
  }

  ss << "}\n";
  return ss.str();
}

}  // namespace FramebufferShaderGen
