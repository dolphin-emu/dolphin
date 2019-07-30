#pragma once

#include <memory>
#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConfig.h"

class NativeVertexFormat;
class AbstractTexture;
class AbstractPipeline;

namespace VideoCommon
{
class RasterFont
{
public:
  bool Initialize(AbstractTextureFormat format);

  void Prepare();
  void Draw(const std::string& text, float start_x, float start_y, u32 color);

private:
  bool CreateTexture();
  bool CreatePipeline();

  AbstractTextureFormat m_framebuffer_format;
  std::unique_ptr<NativeVertexFormat> m_vertex_format;
  std::unique_ptr<AbstractTexture> m_texture;
  std::unique_ptr<AbstractPipeline> m_pipeline;
};
} // namespace VideoCommon
