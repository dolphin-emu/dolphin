// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureUpscaling.h"

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>
#include <stack>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderCompileUtils.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{

static const char TEXTURE_UPSCALING_DIR[] = "TextureUpscaling" DIR_SEP;

static bool LoadUpscalingShaderFromFile(const std::string& shader_name, std::string& out_code)
{
  std::string path =
      File::GetUserPath(D_SHADERS_IDX) + TEXTURE_UPSCALING_DIR + shader_name + ".glsl";

  if (!File::Exists(path))
  {
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + TEXTURE_UPSCALING_DIR + shader_name +
           ".glsl";
  }

  if (!File::ReadFileToString(path, out_code))
  {
    out_code = "";
    ERROR_LOG_FMT(VIDEO, "Texture upscaling shader not found: {}", path);
    return false;
  }

  return true;
}

TextureUpscaling::TextureUpscaling() = default;
TextureUpscaling::~TextureUpscaling() = default;

std::vector<std::string> TextureUpscaling::GetShaderList()
{
  std::vector<std::string> paths = Common::DoFileSearch(
      {{File::GetUserPath(D_SHADERS_IDX) + TEXTURE_UPSCALING_DIR,
        File::GetSysDirectory() + SHADERS_DIR DIR_SEP + TEXTURE_UPSCALING_DIR}},
      ".glsl");
  std::vector<std::string> result;
  for (const std::string& path : paths)
  {
    std::string name;
    SplitPath(path, nullptr, &name, nullptr);
    result.push_back(name);
  }
  return result;
}

bool TextureUpscaling::Initialize()
{
  if (g_ActiveConfig.sTextureUpscalingShader.empty())
    return true;  // Not an error, just nothing to do

  return RecompileShaderInternal();
}

bool TextureUpscaling::RecompileShaderInternal()
{
  const std::string& shader_name = g_ActiveConfig.sTextureUpscalingShader;
  if (shader_name.empty())
    return false;

  std::string shader_code;
  if (!LoadUpscalingShaderFromFile(shader_name, shader_code))
    return false;

  if (IsMultiPassShader(shader_code))
  {
    m_is_multipass = true;
    if (!ParseMultiPassShader(shader_code))
      return false;
    m_scale_factor = ParseScaleFromDirectives(m_passes);
    INFO_LOG_FMT(VIDEO, "Multi-pass shader '{}' with {} passes, {}x scale", shader_name,
                 m_passes.size(), m_scale_factor);
    if (!CompileVertexShader())
      return false;
    if (!CompileMultiPassShaders())
      return false;
    m_current_shader = shader_name;
    return true;
  }
  else
  {
    m_is_multipass = false;
    m_scale_factor = ParseScaleFactor(shader_code);
    INFO_LOG_FMT(VIDEO, "Single-pass shader '{}' declares {}x scale factor", shader_name,
                 m_scale_factor);
    if (!CompileSinglePassPixelShader(shader_code, shader_name))
      return false;
    if (!CompileVertexShader())
      return false;
    if (!CompilePipeline())
      return false;
    m_current_shader = shader_name;
    return true;
  }
}

void TextureUpscaling::RecompileShader()
{
  m_pipeline.reset();
  m_pixel_shader.reset();
  m_vertex_shader.reset();
  m_current_shader.clear();
  m_scale_factor = 2;
  m_is_multipass = false;
  m_passes.clear();
  m_intermediate_textures.clear();

  if (g_ActiveConfig.sTextureUpscalingShader.empty())
    return;

  RecompileShaderInternal();
}

bool TextureUpscaling::IsActive() const
{
  if (m_is_multipass)
    return !m_passes.empty() && !m_current_shader.empty() && m_vertex_shader != nullptr;
  return m_pipeline != nullptr && !m_current_shader.empty();
}

// ============================================================================
// Single-pass shader support (Dolphin format)
// ============================================================================

std::string TextureUpscaling::GetShaderHeader() const
{
  std::ostringstream ss;

  // Uniform buffer — simplified version of PostProcessing's uniforms.
  ss << "UBO_BINDING(std140, 1) uniform TextureUpscalingBlock {\n";
  ss << "  float4 resolution;\n";     // source width, height, 1/width, 1/height
  ss << "  float4 src_rect;\n";       // left, top, width, height (in UV space)
  ss << "  int src_layer;\n";
  ss << "};\n\n";

  ss << "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n";

  if (g_backend_info.bSupportsGeometryShaders)
  {
    ss << "VARYING_LOCATION(0) in VertexData {\n";
    ss << "  float3 v_tex0;\n";
    ss << "};\n";
  }
  else
  {
    ss << "VARYING_LOCATION(0) in float3 v_tex0;\n";
  }

  ss << "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n";

  // Shader API — same functions as PostProcessing shaders
  ss << R"(
float4 Sample() { return texture(samp0, v_tex0); }
float4 SampleLocation(float2 location) { return texture(samp0, float3(location, float(v_tex0.z))); }
float4 SampleLayer(int layer) { return texture(samp0, float3(v_tex0.xy, float(layer))); }
#define SampleOffset(offset) textureOffset(samp0, v_tex0, offset)

float2 GetResolution()
{
  return resolution.xy;
}

float2 GetInvResolution()
{
  return resolution.zw;
}

float2 GetCoordinates()
{
  return v_tex0.xy;
}

float GetLayer()
{
  return v_tex0.z;
}

void SetOutput(float4 color)
{
  ocol0 = color;
}

#define GetOption(x) (x)
#define OptionEnabled(x) ((x) != 0)

)";
  return ss.str();
}

std::string TextureUpscaling::GetVertexShaderSource() const
{
  std::ostringstream ss;

  ss << "UBO_BINDING(std140, 1) uniform TextureUpscalingBlock {\n";
  ss << "  float4 resolution;\n";
  ss << "  float4 src_rect;\n";
  ss << "  int src_layer;\n";
  ss << "};\n\n";

  if (g_backend_info.bSupportsGeometryShaders)
  {
    ss << "VARYING_LOCATION(0) out VertexData {\n";
    ss << "  float3 v_tex0;\n";
    ss << "};\n";
  }
  else
  {
    ss << "VARYING_LOCATION(0) out float3 v_tex0;\n";
  }

  ss << "#define id gl_VertexID\n";
  ss << "#define opos gl_Position\n";
  ss << "void main() {\n";
  ss << "  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n";
  ss << "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n";
  ss << "  v_tex0 = float3(src_rect.xy + (src_rect.zw * v_tex0.xy), float(src_layer));\n";

  if (g_backend_info.api_type == APIType::Vulkan)
  {
    ss << "  opos.y = -opos.y;\n";
  }

  ss << "}\n";
  return ss.str();
}

bool TextureUpscaling::CompileVertexShader()
{
  std::string source = GetVertexShaderSource();
  m_vertex_shader = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source, nullptr,
                                                   "Texture upscaling vertex shader");
  if (!m_vertex_shader)
  {
    PanicAlertFmt("Failed to compile texture upscaling vertex shader");
    return false;
  }
  return true;
}

bool TextureUpscaling::CompileSinglePassPixelShader(const std::string& shader_code,
                                                     const std::string& shader_name)
{
  std::string source = GetShaderHeader() + shader_code;
  m_pixel_shader = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source, nullptr,
                                                   "Texture upscaling pixel shader: " + shader_name);
  if (!m_pixel_shader)
  {
    PanicAlertFmt("Failed to compile texture upscaling shader: {}", shader_name);
    return false;
  }
  return true;
}

int TextureUpscaling::ParseScaleFactor(const std::string& shader_code)
{
  const std::string prefix = "// UpscaleFactor:";
  auto pos = shader_code.find(prefix);
  if (pos == std::string::npos)
    return 2;

  pos += prefix.size();
  while (pos < shader_code.size() && (shader_code[pos] == ' ' || shader_code[pos] == '\t'))
    pos++;

  int factor = 0;
  while (pos < shader_code.size() && shader_code[pos] >= '0' && shader_code[pos] <= '9')
  {
    factor = factor * 10 + (shader_code[pos] - '0');
    pos++;
  }

  if (factor < 1)
    factor = 2;
  if (factor > 8)
    factor = 8;

  return factor;
}

bool TextureUpscaling::CompilePipeline()
{
  AbstractPipelineConfig config = {};
  config.vertex_shader = m_vertex_shader.get();
  config.geometry_shader = nullptr;
  config.pixel_shader = m_pixel_shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(AbstractTextureFormat::RGBA8);
  config.usage = AbstractPipelineUsage::Utility;
  m_pipeline = g_gfx->CreatePipeline(config);

  if (!m_pipeline)
  {
    PanicAlertFmt("Failed to compile texture upscaling pipeline");
    return false;
  }

  return true;
}

// ============================================================================
// Multi-pass shader support (mpv format)
// ============================================================================

bool TextureUpscaling::IsMultiPassShader(const std::string& code)
{
  return code.find("//!HOOK") != std::string::npos;
}

static std::string TrimWhitespace(const std::string& s)
{
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool TextureUpscaling::ParseMultiPassShader(const std::string& code)
{
  m_passes.clear();

  // Split into lines
  std::istringstream stream(code);
  std::string line;

  ShaderPass current_pass;
  bool in_pass = false;
  std::ostringstream glsl_body;

  auto finish_pass = [&]() {
    if (in_pass)
    {
      current_pass.glsl_code = glsl_body.str();
      m_passes.push_back(std::move(current_pass));
      current_pass = ShaderPass{};
      glsl_body.str("");
      glsl_body.clear();
      in_pass = false;
    }
  };

  while (std::getline(stream, line))
  {
    std::string trimmed = TrimWhitespace(line);

    // Check for mpv directives
    if (trimmed.substr(0, 7) == "//!DESC")
    {
      finish_pass();
      in_pass = true;
      current_pass.desc = TrimWhitespace(trimmed.substr(7));
      continue;
    }

    if (trimmed.substr(0, 7) == "//!HOOK")
    {
      // If we're already in a pass and see a new HOOK, this pass had no DESC
      // directive — finish the current pass first
      if (in_pass && !current_pass.hook.empty())
      {
        finish_pass();
      }
      in_pass = true;
      current_pass.hook = TrimWhitespace(trimmed.substr(7));
      continue;
    }

    if (trimmed.substr(0, 7) == "//!BIND")
    {
      current_pass.binds.push_back(TrimWhitespace(trimmed.substr(7)));
      continue;
    }

    if (trimmed.substr(0, 7) == "//!SAVE")
    {
      current_pass.save = TrimWhitespace(trimmed.substr(7));
      continue;
    }

    if (trimmed.substr(0, 8) == "//!WIDTH")
    {
      current_pass.width_expr = TrimWhitespace(trimmed.substr(8));
      continue;
    }

    if (trimmed.substr(0, 9) == "//!HEIGHT")
    {
      current_pass.height_expr = TrimWhitespace(trimmed.substr(9));
      continue;
    }

    if (trimmed.substr(0, 13) == "//!COMPONENTS")
    {
      std::string val = TrimWhitespace(trimmed.substr(13));
      current_pass.components = std::clamp(std::atoi(val.c_str()), 1, 4);
      continue;
    }

    // Skip //!WHEN — we always execute all passes
    if (trimmed.substr(0, 7) == "//!WHEN")
    {
      continue;
    }

    // Skip license comments at the top (before first pass)
    if (!in_pass)
      continue;

    // Regular GLSL code
    glsl_body << line << "\n";
  }

  finish_pass();

  if (m_passes.empty())
  {
    ERROR_LOG_FMT(VIDEO, "No passes found in multi-pass shader");
    return false;
  }

  // In mpv, the hook target is always implicitly available for sampling.
  // If neither HOOKED nor the hook target name appears in the binds list,
  // prepend the hook target so it gets a sampler slot and macros.
  for (auto& p : m_passes)
  {
    bool has_hook_target = false;
    for (const auto& b : p.binds)
    {
      if (b == "HOOKED" || b == p.hook)
      {
        has_hook_target = true;
        break;
      }
    }
    if (!has_hook_target)
    {
      p.binds.insert(p.binds.begin(), "HOOKED");
    }
  }

  INFO_LOG_FMT(VIDEO, "Parsed {} passes from multi-pass shader", m_passes.size());
  for (size_t i = 0; i < m_passes.size(); i++)
  {
    const auto& p = m_passes[i];
    std::string binds_str;
    for (const auto& b : p.binds)
      binds_str += b + " ";
    INFO_LOG_FMT(VIDEO,
                 "  Pass {}: desc='{}' HOOK={} SAVE={} WIDTH='{}' HEIGHT='{}' COMPONENTS={} "
                 "BINDS=[{}] code_size={}",
                 i, p.desc, p.hook, p.save.empty() ? "(final)" : p.save, p.width_expr,
                 p.height_expr, p.components, binds_str, p.glsl_code.size());
  }

  return true;
}

int TextureUpscaling::ParseScaleFromDirectives(const std::vector<ShaderPass>& passes)
{
  // Look for the maximum scale multiplier in WIDTH/HEIGHT expressions
  // Typical: "MAIN.w 2 *" or "MAIN.w 4 *"
  int max_scale = 1;
  for (const auto& pass : passes)
  {
    for (const auto& expr : {pass.width_expr, pass.height_expr})
    {
      if (expr.empty())
        continue;
      // Look for "N *" pattern at end of expression
      auto star_pos = expr.rfind('*');
      if (star_pos != std::string::npos && star_pos > 0)
      {
        // Find the number before the *
        std::string before = TrimWhitespace(expr.substr(0, star_pos));
        auto space_pos = before.rfind(' ');
        std::string num_str = (space_pos != std::string::npos) ?
                               before.substr(space_pos + 1) : before;
        int val = std::atoi(num_str.c_str());
        if (val > max_scale)
          max_scale = val;
      }
    }
  }
  return std::clamp(max_scale, 1, 8);
}

std::string TextureUpscaling::GeneratePassPixelShader(const ShaderPass& pass,
                                                       bool is_last_pass) const
{
  std::ostringstream ss;

  // Uniform buffer
  ss << "UBO_BINDING(std140, 1) uniform TextureUpscalingBlock {\n";
  ss << "  float4 resolution;\n";
  ss << "  float4 src_rect;\n";
  ss << "  int src_layer;\n";
  ss << "};\n\n";

  // Varying input from vertex shader
  if (g_backend_info.bSupportsGeometryShaders)
  {
    ss << "VARYING_LOCATION(0) in VertexData {\n";
    ss << "  float3 v_tex0;\n";
    ss << "};\n";
  }
  else
  {
    ss << "VARYING_LOCATION(0) in float3 v_tex0;\n";
  }

  ss << "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n\n";

  // Declare samplers for each bound texture (max 16 — Dolphin's MAX_PIXEL_SHADER_SAMPLERS)
  for (size_t i = 0; i < pass.binds.size() && i < 16; i++)
  {
    ss << "SAMPLER_BINDING(" << i << ") uniform sampler2DArray samp_" << i << ";\n";
  }
  ss << "\n";

  // Generate macro definitions for each bound texture
  // Use textureSize() for correct per-texture pixel sizes (intermediate textures
  // may have different dimensions than the source texture)
  bool hooked_defined = false;
  for (size_t i = 0; i < pass.binds.size() && i < 16; i++)
  {
    const std::string& bind_name = pass.binds[i];

    // Resolve HOOKED -> the actual hook target name
    std::string actual_name = bind_name;
    if (bind_name == "HOOKED")
      actual_name = pass.hook;

    // FOO_tex(pos) -> sample from the bound sampler
    ss << "#define " << actual_name << "_tex(pos) texture(samp_" << i
       << ", float3((pos), 0.0))\n";

    // FOO_pt -> pixel size, derived from actual texture dimensions via textureSize()
    ss << "#define " << actual_name
       << "_pt (vec2(1.0) / vec2(textureSize(samp_" << i << ", 0).xy))\n";

    // FOO_size -> texture dimensions
    ss << "#define " << actual_name
       << "_size (vec2(textureSize(samp_" << i << ", 0).xy))\n";

    // FOO_texOff(off) -> sample at current pos + offset * pixel size
    ss << "#define " << actual_name << "_texOff(off) texture(samp_" << i
       << ", float3(v_tex0.xy + (off) * " << actual_name << "_pt, 0.0))\n";

    // FOO_pos -> current texture coordinate
    ss << "#define " << actual_name << "_pos v_tex0.xy\n";

    // If bind is HOOKED, also define HOOKED_ variants that alias to the hook target
    if (bind_name == "HOOKED")
    {
      ss << "#define HOOKED_tex(pos) " << actual_name << "_tex(pos)\n";
      ss << "#define HOOKED_texOff(off) " << actual_name << "_texOff(off)\n";
      ss << "#define HOOKED_pos " << actual_name << "_pos\n";
      ss << "#define HOOKED_pt " << actual_name << "_pt\n";
      ss << "#define HOOKED_size " << actual_name << "_size\n";
      hooked_defined = true;
    }
    // If the hook target appears directly (not via HOOKED alias), also define HOOKED_ macros
    else if (bind_name == pass.hook && !hooked_defined)
    {
      ss << "#define HOOKED_tex(pos) " << actual_name << "_tex(pos)\n";
      ss << "#define HOOKED_texOff(off) " << actual_name << "_texOff(off)\n";
      ss << "#define HOOKED_pos " << actual_name << "_pos\n";
      ss << "#define HOOKED_pt " << actual_name << "_pt\n";
      ss << "#define HOOKED_size " << actual_name << "_size\n";
      hooked_defined = true;
    }
    ss << "\n";
  }

  // Add the shader body
  ss << pass.glsl_code << "\n";

  // Wrap hook() -> main()
  // For the last pass, we may need to fix alpha:
  // - CNN/GAN "Depth-to-Space" passes produce neural network values in alpha
  //   that have no relation to the source alpha → override from source.
  // - DoG/DTD/Original passes do "HOOKED_tex(pos) + correction", where the
  //   correction also beneficially sharpens alpha edges → keep as-is.
  if (is_last_pass)
  {
    // Detect neural network shaders by checking for "Depth-to-Space" in the
    // pass description (CNN/GAN final passes always use this).
    const bool is_neural_network =
        pass.desc.find("Depth-to-Space") != std::string::npos;

    ss << "void main() {\n";
    ss << "  vec4 result = hook();\n";
    if (is_neural_network)
    {
      ss << "  result.a = HOOKED_tex(HOOKED_pos).a;\n";
    }
    ss << "  ocol0 = result;\n";
    ss << "}\n";
  }
  else
  {
    ss << "void main() {\n";
    ss << "  ocol0 = hook();\n";
    ss << "}\n";
  }

  return ss.str();
}

bool TextureUpscaling::CompileMultiPassShaders()
{
  for (size_t i = 0; i < m_passes.size(); i++)
  {
    auto& pass = m_passes[i];
    const bool is_last = (i == m_passes.size() - 1);
    std::string source = GeneratePassPixelShader(pass, is_last);

    pass.pixel_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Pixel, source, nullptr,
        "Texture upscaling pass " + std::to_string(i) + ": " + pass.desc);

    if (!pass.pixel_shader)
    {
      ERROR_LOG_FMT(VIDEO, "Failed to compile multi-pass shader pass {}: {}", i, pass.desc);
      PanicAlertFmt("Failed to compile texture upscaling shader pass {}: {}", i, pass.desc);
      m_passes.clear();
      return false;
    }

    // Build pipeline for this pass
    AbstractPipelineConfig config = {};
    config.vertex_shader = m_vertex_shader.get();
    config.geometry_shader = nullptr;
    config.pixel_shader = pass.pixel_shader.get();
    config.rasterization_state =
        RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
    config.depth_state = RenderState::GetNoDepthTestingDepthState();
    config.blending_state = RenderState::GetNoBlendingBlendState();
    // Intermediate passes render to RGBA16F textures; the last pass renders to
    // the destination texture which is RGBA8.  The pipeline's framebuffer state
    // must match the actual render target format.
    const auto fb_format =
        is_last ? AbstractTextureFormat::RGBA8 : AbstractTextureFormat::RGBA16F;
    config.framebuffer_state = RenderState::GetColorFramebufferState(fb_format);
    config.usage = AbstractPipelineUsage::Utility;
    pass.pipeline = g_gfx->CreatePipeline(config);

    if (!pass.pipeline)
    {
      ERROR_LOG_FMT(VIDEO, "Failed to create pipeline for pass {}: {}", i, pass.desc);
      m_passes.clear();
      return false;
    }

    INFO_LOG_FMT(VIDEO, "Compiled multi-pass shader pass {}: {}", i, pass.desc);
  }

  return true;
}

int TextureUpscaling::EvalSizeExpr(const std::string& expr, int main_w, int main_h) const
{
  if (expr.empty())
    return 0;

  // Simple RPN evaluator for expressions like "MAIN.w 2 *"
  std::istringstream stream(expr);
  std::string token;
  std::stack<float> stack;

  while (stream >> token)
  {
    if (token == "MAIN.w" || token == "HOOKED.w")
      stack.push(static_cast<float>(main_w));
    else if (token == "MAIN.h" || token == "HOOKED.h")
      stack.push(static_cast<float>(main_h));
    else if (token == "*" && stack.size() >= 2)
    {
      float b = stack.top(); stack.pop();
      float a = stack.top(); stack.pop();
      stack.push(a * b);
    }
    else if (token == "/" && stack.size() >= 2)
    {
      float b = stack.top(); stack.pop();
      float a = stack.top(); stack.pop();
      stack.push(b != 0 ? a / b : 0);
    }
    else if (token == "+" && stack.size() >= 2)
    {
      float b = stack.top(); stack.pop();
      float a = stack.top(); stack.pop();
      stack.push(a + b);
    }
    else if (token == "-" && stack.size() >= 2)
    {
      float b = stack.top(); stack.pop();
      float a = stack.top(); stack.pop();
      stack.push(a - b);
    }
    else
    {
      // Try to parse as a number
      float val = static_cast<float>(std::atof(token.c_str()));
      stack.push(val);

      // Also handle FOO.w and FOO.h references for intermediate textures
      if (token.size() > 2 && token[token.size() - 2] == '.')
      {
        stack.pop();  // remove the 0 we just pushed
        std::string tex_name = token.substr(0, token.size() - 2);
        char dim = token.back();
        auto it = m_intermediate_textures.find(tex_name);
        if (it != m_intermediate_textures.end())
        {
          if (dim == 'w')
            stack.push(static_cast<float>(it->second.width));
          else if (dim == 'h')
            stack.push(static_cast<float>(it->second.height));
          else
            stack.push(0);
        }
        else
        {
          // Might reference MAIN
          if (tex_name == "MAIN" || tex_name == "HOOKED")
          {
            stack.push(dim == 'w' ? static_cast<float>(main_w) : static_cast<float>(main_h));
          }
          else
          {
            stack.push(0);
          }
        }
      }
    }
  }

  return stack.empty() ? 0 : static_cast<int>(std::round(stack.top()));
}

void TextureUpscaling::EnsureIntermediateTexture(const std::string& name, u32 width, u32 height,
                                                  int components)
{
  auto it = m_intermediate_textures.find(name);
  if (it != m_intermediate_textures.end() && it->second.width == width &&
      it->second.height == height)
  {
    return;  // Already the right size
  }

  // Use RGBA16F for intermediate textures. Neural network passes (CNN/GAN)
  // produce intermediate activation values that can be negative or >1.0.
  // RGBA8 would clamp these, destroying the computations between layers.
  TextureConfig tex_config(width, height, 1, 1, 1, AbstractTextureFormat::RGBA16F,
                            AbstractTextureFlag_RenderTarget, AbstractTextureType::Texture_2DArray);

  auto texture = g_gfx->CreateTexture(tex_config);
  if (!texture)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create intermediate texture '{}' ({}x{})", name, width,
                  height);
    return;
  }

  auto framebuffer = g_gfx->CreateFramebuffer(texture.get(), nullptr);
  if (!framebuffer)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create framebuffer for intermediate texture '{}'", name);
    return;
  }

  IntermediateTexture& entry = m_intermediate_textures[name];
  entry.texture = std::move(texture);
  entry.framebuffer = std::move(framebuffer);
  entry.width = width;
  entry.height = height;
}

// ============================================================================
// Rendering
// ============================================================================

void TextureUpscaling::UpscaleTexture(AbstractFramebuffer* dst_framebuffer,
                                       const MathUtil::Rectangle<int>& dst_rect,
                                       const AbstractTexture* src_texture,
                                       const MathUtil::Rectangle<int>& src_rect)
{
  if (!IsActive())
    return;

  if (m_is_multipass)
  {
    UpscaleTextureMultiPass(dst_framebuffer, dst_rect, src_texture, src_rect);
    return;
  }

  // Single-pass rendering (original path)
  g_gfx->BeginUtilityDrawing();

  const auto converted_src_rect =
      g_gfx->ConvertFramebufferRectangle(src_rect, src_texture->GetWidth(),
                                          src_texture->GetHeight());
  const float rcp_src_width = 1.0f / src_texture->GetWidth();
  const float rcp_src_height = 1.0f / src_texture->GetHeight();

  struct
  {
    std::array<float, 4> resolution;
    std::array<float, 4> src_rect;
    s32 src_layer;
    s32 padding[3];
  } uniforms;

  uniforms.resolution = {{static_cast<float>(src_texture->GetWidth()),
                           static_cast<float>(src_texture->GetHeight()), rcp_src_width,
                           rcp_src_height}};
  uniforms.src_rect = {{converted_src_rect.left * rcp_src_width,
                         converted_src_rect.top * rcp_src_height,
                         converted_src_rect.GetWidth() * rcp_src_width,
                         converted_src_rect.GetHeight() * rcp_src_height}};
  uniforms.src_layer = 0;

  g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

  if (static_cast<u32>(dst_rect.GetWidth()) == dst_framebuffer->GetWidth() &&
      static_cast<u32>(dst_rect.GetHeight()) == dst_framebuffer->GetHeight())
  {
    g_gfx->SetAndDiscardFramebuffer(dst_framebuffer);
  }
  else
  {
    g_gfx->SetFramebuffer(dst_framebuffer);
  }

  g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(dst_rect, dst_framebuffer));
  g_gfx->SetPipeline(m_pipeline.get());
  g_gfx->SetTexture(0, src_texture);
  g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_gfx->Draw(0, 3);

  g_gfx->EndUtilityDrawing();
  if (dst_framebuffer->GetColorAttachment())
    dst_framebuffer->GetColorAttachment()->FinishedRendering();
}

void TextureUpscaling::UpscaleTextureMultiPass(AbstractFramebuffer* dst_framebuffer,
                                                const MathUtil::Rectangle<int>& dst_rect,
                                                const AbstractTexture* src_texture,
                                                const MathUtil::Rectangle<int>& src_rect)
{
  const int main_w = static_cast<int>(src_texture->GetWidth());
  const int main_h = static_cast<int>(src_texture->GetHeight());

  g_gfx->BeginUtilityDrawing();

  // Track the "current" MAIN texture.  Initially it is the source, but if
  // a pass saves to MAIN (no //!SAVE → defaults to hook target = MAIN),
  // subsequent passes must see the modified version (mpv in-place semantics).
  const AbstractTexture* main_texture = src_texture;

  // Store source texture resolution for uniform setup
  const float rcp_src_width = 1.0f / src_texture->GetWidth();
  const float rcp_src_height = 1.0f / src_texture->GetHeight();

  for (size_t pass_idx = 0; pass_idx < m_passes.size(); pass_idx++)
  {
    const auto& pass = m_passes[pass_idx];
    const bool is_last_pass = (pass_idx == m_passes.size() - 1);

    // Determine output size
    int out_w, out_h;
    if (!pass.width_expr.empty())
      out_w = EvalSizeExpr(pass.width_expr, main_w, main_h);
    else
      out_w = main_w;

    if (!pass.height_expr.empty())
      out_h = EvalSizeExpr(pass.height_expr, main_w, main_h);
    else
      out_h = main_h;

    if (out_w <= 0 || out_h <= 0)
    {
      out_w = main_w;
      out_h = main_h;
    }

    DEBUG_LOG_FMT(VIDEO,
                 "Multi-pass exec pass {}/{}: desc='{}' out={}x{} is_last={} "
                 "save='{}' binds={}",
                 pass_idx, m_passes.size(), pass.desc, out_w, out_h, is_last_pass,
                 pass.save.empty() ? "(final)" : pass.save, pass.binds.size());

    // Determine where to render to
    AbstractFramebuffer* target_fb = nullptr;
    if (is_last_pass)
    {
      // Last pass renders directly to dst
      target_fb = dst_framebuffer;
    }
    else
    {
      // Intermediate pass — render to a named texture
      std::string out_name = pass.save.empty() ? pass.hook : pass.save;

      // Detect read-write conflict: a pass that both BINDs and SAVEs the same
      // texture would simultaneously read and write the same GPU resource, which
      // is undefined behavior.  Use ping-pong: write to a temp texture, then
      // swap it into the real name after drawing.
      bool read_write_conflict = false;
      for (const auto& b : pass.binds)
      {
        std::string resolved = (b == "HOOKED") ? pass.hook : b;
        if (resolved == out_name)
        {
          read_write_conflict = true;
          break;
        }
      }

      std::string actual_out_name = read_write_conflict ? (out_name + "__ping") : out_name;

      EnsureIntermediateTexture(actual_out_name, static_cast<u32>(out_w), static_cast<u32>(out_h),
                                 pass.components);
      auto it = m_intermediate_textures.find(actual_out_name);
      if (it == m_intermediate_textures.end() || !it->second.framebuffer)
      {
        ERROR_LOG_FMT(VIDEO, "Failed to get intermediate framebuffer for pass {}", pass_idx);
        continue;
      }
      target_fb = it->second.framebuffer.get();

      // --- Set up uniforms, framebuffer, bind textures, draw (below) ---

      // Set up uniforms — use the source texture dimensions for resolution
      // (this is what HOOKED_pt and MAIN_pt reference)
      struct
      {
        std::array<float, 4> resolution;
        std::array<float, 4> src_rect;
        s32 src_layer;
        s32 padding[3];
      } uniforms;

      uniforms.resolution = {{static_cast<float>(main_w), static_cast<float>(main_h),
                               rcp_src_width, rcp_src_height}};
      uniforms.src_rect = {{0.0f, 0.0f, 1.0f, 1.0f}};
      uniforms.src_layer = 0;

      g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

      g_gfx->SetAndDiscardFramebuffer(target_fb);
      g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(
          MathUtil::Rectangle<int>(0, 0, out_w, out_h), target_fb));

      // Bind textures (reads from the OLD version of out_name if conflict)
      for (size_t bind_idx = 0; bind_idx < pass.binds.size() && bind_idx < 16; bind_idx++)
      {
        std::string bind_name = pass.binds[bind_idx];
        if (bind_name == "HOOKED")
          bind_name = pass.hook;

        const AbstractTexture* tex = nullptr;

        if (bind_name == "MAIN")
        {
          tex = main_texture;
        }
        else
        {
          auto bit = m_intermediate_textures.find(bind_name);
          if (bit != m_intermediate_textures.end())
            tex = bit->second.texture.get();
        }

        if (tex)
        {
          g_gfx->SetTexture(static_cast<u32>(bind_idx), tex);
          g_gfx->SetSamplerState(static_cast<u32>(bind_idx),
                                  RenderState::GetLinearSamplerState());
        }
      }

      // Draw
      g_gfx->SetPipeline(pass.pipeline.get());
      g_gfx->Draw(0, 3);

      // Render barrier
      it = m_intermediate_textures.find(actual_out_name);
      if (it != m_intermediate_textures.end() && it->second.texture)
        it->second.texture->FinishedRendering();

      // If ping-pong was used, swap the new texture into the canonical name
      if (read_write_conflict)
      {
        m_intermediate_textures[out_name] = std::move(m_intermediate_textures[actual_out_name]);
        m_intermediate_textures.erase(actual_out_name);
      }

      // If this pass saved to MAIN, update the tracked main_texture
      if (out_name == "MAIN")
        main_texture = m_intermediate_textures[out_name].texture.get();

      continue;  // skip the shared code below (used only for last pass)
    }

    // --- Last pass: shared setup for rendering to dst_framebuffer ---

    // Set up uniforms
    struct
    {
      std::array<float, 4> resolution;
      std::array<float, 4> src_rect;
      s32 src_layer;
      s32 padding[3];
    } uniforms;

    uniforms.resolution = {{static_cast<float>(main_w), static_cast<float>(main_h),
                             rcp_src_width, rcp_src_height}};
    uniforms.src_rect = {{0.0f, 0.0f, 1.0f, 1.0f}};
    uniforms.src_layer = 0;

    g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

    // Set framebuffer
    if (static_cast<u32>(dst_rect.GetWidth()) == dst_framebuffer->GetWidth() &&
        static_cast<u32>(dst_rect.GetHeight()) == dst_framebuffer->GetHeight())
    {
      g_gfx->SetAndDiscardFramebuffer(target_fb);
    }
    else
    {
      g_gfx->SetFramebuffer(target_fb);
    }
    g_gfx->SetViewportAndScissor(
        g_gfx->ConvertFramebufferRectangle(dst_rect, dst_framebuffer));

    // Bind textures
    for (size_t bind_idx = 0; bind_idx < pass.binds.size() && bind_idx < 16; bind_idx++)
    {
      std::string bind_name = pass.binds[bind_idx];
      if (bind_name == "HOOKED")
        bind_name = pass.hook;

      const AbstractTexture* tex = nullptr;

      if (bind_name == "MAIN")
      {
        tex = main_texture;
      }
      else
      {
        auto bit = m_intermediate_textures.find(bind_name);
        if (bit != m_intermediate_textures.end())
          tex = bit->second.texture.get();
      }

      if (tex)
      {
        g_gfx->SetTexture(static_cast<u32>(bind_idx), tex);
        g_gfx->SetSamplerState(static_cast<u32>(bind_idx),
                                RenderState::GetLinearSamplerState());
      }
    }

    // Draw
    g_gfx->SetPipeline(pass.pipeline.get());
    g_gfx->Draw(0, 3);
  }

  g_gfx->EndUtilityDrawing();
  if (dst_framebuffer->GetColorAttachment())
    dst_framebuffer->GetColorAttachment()->FinishedRendering();
}

}  // namespace VideoCommon
