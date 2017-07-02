// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoCommon/FramebufferManagerBase.h"

// On the GameCube, the game sends a request for the graphics processor to
// transfer its internal EFB (Embedded Framebuffer) to an area in GameCube RAM
// called the XFB (External Framebuffer). The size and location of the XFB is
// decided at the time of the copy, and the format is always YUYV. The video
// interface is given a pointer to the XFB, which will be decoded and
// displayed on the TV.
//
// There are two ways for Dolphin to emulate this:
//
// Real XFB mode:
//
// Dolphin will behave like the GameCube and encode the EFB to
// a portion of GameCube RAM. The emulated video interface will decode the data
// for output to the screen.
//
// Advantages: Behaves exactly like the GameCube.
// Disadvantages: Resolution will be limited.
//
// Virtual XFB mode:
//
// When a request is made to copy the EFB to an XFB, Dolphin
// will remember the RAM location and size of the XFB in a Virtual XFB list.
// The video interface will look up the XFB in the list and use the enhanced
// data stored there, if available.
//
// Advantages: Enables high resolution graphics, better than real hardware.
// Disadvantages: If the GameCube CPU writes directly to the XFB (which is
// possible but uncommon), the Virtual XFB will not capture this information.

// There may be multiple XFBs in GameCube RAM. This is the maximum number to
// virtualize.

namespace OGL
{
struct XFBSource : public XFBSourceBase
{
  XFBSource(GLuint tex, int layers) : texture(tex), m_layers(layers) {}
  ~XFBSource();

  void CopyEFB(float Gamma) override;
  void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override;

  const GLuint texture;
  const int m_layers;
};

class FramebufferManager : public FramebufferManagerBase
{
public:
  FramebufferManager(int targetWidth, int targetHeight, int msaaSamples,
                     bool enable_stencil_buffer);
  ~FramebufferManager();

  // To get the EFB in texture form, these functions may have to transfer
  // the EFB to a resolved texture first.
  static GLuint GetEFBColorTexture(const EFBRectangle& sourceRc);
  static GLuint GetEFBDepthTexture(const EFBRectangle& sourceRc);
  static void ResolveEFBStencilTexture();

  static GLuint GetEFBFramebuffer(unsigned int layer = 0)
  {
    return (layer < m_EFBLayers) ? m_efbFramebuffer[layer] : m_efbFramebuffer.back();
  }
  static GLuint GetXFBFramebuffer() { return m_xfbFramebuffer; }
  // Resolved framebuffer is only used in MSAA mode.
  static GLuint GetResolvedFramebuffer();
  static void SetFramebuffer(GLuint fb);
  static void FramebufferTexture(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                 GLint level);

  // If in MSAA mode, this will perform a resolve of the specified rectangle, and return the resolve
  // target as a texture ID.
  // Thus, this call may be expensive. Don't repeat it unnecessarily.
  // If not in MSAA mode, will just return the render target texture ID.
  // After calling this, before you render anything else, you MUST bind the framebuffer you want to
  // draw to.
  static GLuint ResolveAndGetRenderTarget(const EFBRectangle& rect);

  // Same as above but for the depth Target.
  // After calling this, before you render anything else, you MUST bind the framebuffer you want to
  // draw to.
  static GLuint ResolveAndGetDepthTarget(const EFBRectangle& rect);

  // Convert EFB content on pixel format change.
  // convtype=0 -> rgb8->rgba6, convtype=2 -> rgba6->rgb8
  static void ReinterpretPixelData(unsigned int convtype);

  static void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points);
  static bool HasStencilBuffer();

private:
  GLuint CreateTexture(GLenum texture_type, GLenum internal_format, GLenum pixel_format,
                       GLenum data_type);
  void BindLayeredTexture(GLuint texture, const std::vector<GLuint>& framebuffers,
                          GLenum attachment, GLenum texture_type);
  std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width,
                                                 unsigned int target_height,
                                                 unsigned int layers) override;
  std::pair<u32, u32> GetTargetSize() const override;

  void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc,
                     float Gamma) override;

  static int m_targetWidth;
  static int m_targetHeight;
  static int m_msaaSamples;

  static GLenum m_textureType;
  static std::vector<GLuint> m_efbFramebuffer;
  static GLuint m_xfbFramebuffer;
  static GLuint m_efbColor;
  static GLuint m_efbDepth;
  static GLuint
      m_efbColorSwap;  // will be hot swapped with m_efbColor when reinterpreting EFB pixel formats

  static bool m_enable_stencil_buffer;

  // Only used in MSAA mode, TODO: try to avoid them
  static std::vector<GLuint> m_resolvedFramebuffer;
  static GLuint m_resolvedColorTexture;
  static GLuint m_resolvedDepthTexture;

  // For pixel format draw
  static SHADER m_pixel_format_shaders[2];

  // For EFB pokes
  static GLuint m_EfbPokes_VBO;
  static GLuint m_EfbPokes_VAO;
  static SHADER m_EfbPokes;
};

}  // namespace OGL
