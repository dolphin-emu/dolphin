// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTexture.h"

class AbstractShader;
class AbstractTexture;
class AbstractPipeline;
class NativeVertexFormat;

namespace VideoCommon
{
// Raster font for rendering OSD.
class RasterFont
{
public:
  RasterFont();
  ~RasterFont();

  bool Initialize(AbstractTextureFormat framebuffer_format);
  bool SetFramebufferFormat(AbstractTextureFormat format);

  void RenderText(const std::string& str, float x, float y, u32 color, u32 viewport_width,
                  u32 viewport_height, bool shadow);

private:
  struct Vertex
  {
    // Coordinates are in clip-space.
    float x, y;
    float u, v;
    u32 color;
  };

  static bool IsPrintableCharacter(char ch);
  void GenerateCharacterVertices(char ch, float current_x, float current_y, u32 color,
                                 float delta_x, float delta_y);
  void GenerateVertices(const std::string& str, float x, float y, u32 color, u32 viewport_width,
                        u32 viewport_height, bool shadow);

  void CreateVertexFormat();
  bool CompileShaders();
  bool CreatePipeline();
  bool CreateTexture();

  std::unique_ptr<AbstractShader> m_vertex_shader;
  std::unique_ptr<AbstractShader> m_pixel_shader;
  std::unique_ptr<AbstractTexture> m_texture;
  std::unique_ptr<AbstractPipeline> m_pipeline;
  std::unique_ptr<NativeVertexFormat> m_vertex_format;
  std::vector<Vertex> m_vertices;
  AbstractTextureFormat m_framebuffer_format = AbstractTextureFormat::Undefined;
};

}  // namespace VideoCommon
