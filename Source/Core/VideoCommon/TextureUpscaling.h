// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;

namespace VideoCommon
{

// Represents one render pass in a multi-pass mpv-format shader
struct ShaderPass
{
  std::string desc;
  std::string hook;                  // e.g. "MAIN"
  std::vector<std::string> binds;    // e.g. ["HOOKED", "LINELUMA"]
  std::string save;                  // output name, empty = replace HOOK target
  std::string width_expr;            // RPN expression, e.g. "MAIN.w 2 *"
  std::string height_expr;           // RPN expression, e.g. "MAIN.h 2 *"
  int components = 4;
  std::string glsl_code;             // The raw GLSL body (after directives)

  std::unique_ptr<AbstractShader> pixel_shader;
  std::unique_ptr<AbstractPipeline> pipeline;
};

class TextureUpscaling
{
public:
  TextureUpscaling();
  ~TextureUpscaling();

  // Returns a list of available upscaling shaders (filenames without extension)
  static std::vector<std::string> GetShaderList();

  // Initialize the module (first-time compile)
  bool Initialize();

  // Recompile if the config has changed
  void RecompileShader();

  // Upscale a texture: renders src_texture into dst_framebuffer using the
  // configured upscaling shader. dst_framebuffer should be a larger texture.
  void UpscaleTexture(AbstractFramebuffer* dst_framebuffer,
                      const MathUtil::Rectangle<int>& dst_rect,
                      const AbstractTexture* src_texture,
                      const MathUtil::Rectangle<int>& src_rect);

  // Returns true if an upscaling shader is configured and compiled
  bool IsActive() const;

  // Returns the scale factor declared by the current shader (default 2)
  int GetScaleFactor() const { return m_scale_factor; }

private:
  bool RecompileShaderInternal();

  // Single-pass (Dolphin-format) shader compilation
  bool CompileVertexShader();
  bool CompileSinglePassPixelShader(const std::string& shader_code,
                                     const std::string& shader_name);
  bool CompilePipeline();
  std::string GetShaderHeader() const;
  std::string GetVertexShaderSource() const;

  // Multi-pass (mpv-format) shader support
  static bool IsMultiPassShader(const std::string& code);
  bool ParseMultiPassShader(const std::string& code);
  bool CompileMultiPassShaders();
  std::string GeneratePassPixelShader(const ShaderPass& pass, bool is_last_pass) const;
  void UpscaleTextureMultiPass(AbstractFramebuffer* dst_framebuffer,
                                const MathUtil::Rectangle<int>& dst_rect,
                                const AbstractTexture* src_texture,
                                const MathUtil::Rectangle<int>& src_rect);
  int EvalSizeExpr(const std::string& expr, int main_w, int main_h) const;
  void EnsureIntermediateTexture(const std::string& name, u32 width, u32 height, int components);

  static int ParseScaleFactor(const std::string& shader_code);
  static int ParseScaleFromDirectives(const std::vector<ShaderPass>& passes);

  std::string m_current_shader;
  int m_scale_factor = 2;
  bool m_is_multipass = false;

  // Single-pass state
  std::unique_ptr<AbstractShader> m_vertex_shader;
  std::unique_ptr<AbstractShader> m_pixel_shader;
  std::unique_ptr<AbstractPipeline> m_pipeline;

  // Multi-pass state
  std::vector<ShaderPass> m_passes;

  struct IntermediateTexture
  {
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    u32 width = 0;
    u32 height = 0;
  };
  std::unordered_map<std::string, IntermediateTexture> m_intermediate_textures;
};

}  // namespace VideoCommon
