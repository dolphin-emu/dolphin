// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/Render.h"

#include <array>
#include <cinttypes>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <tuple>
#include <unordered_map>

#include "Common/Atomic.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Television.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VRD3D.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#if defined(HAVE_FFMPEG)
#include "VideoCommon/AVIDump.h"
#endif
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/XFMemory.h"

static bool g_first_rift_frame = true;

namespace DX11
{
// Nvidia stereo blitting struct defined in "nvstereo.h" from the Nvidia SDK
typedef struct _Nv_Stereo_Image_Header
{
  unsigned int dwSignature;
  unsigned int dwWidth;
  unsigned int dwHeight;
  unsigned int dwBPP;
  unsigned int dwFlags;
} NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;

#define NVSTEREO_IMAGE_SIGNATURE 0x4433564e

struct GXPipelineState
{
  std::array<SamplerState, 8> samplers;
  BlendingState blend;
  DepthState zmode;
  RasterizationState raster;
};

static u32 s_last_multisamples = 1;
static bool s_last_stereo_mode = false;
static bool s_last_xfb_mode = false;
static bool s_last_fullscreen_mode = false;

static Television s_television;

static std::array<ID3D11BlendState*, 4> s_clear_blend_states{};
static std::array<ID3D11DepthStencilState*, 3> s_clear_depth_states{};
static ID3D11BlendState* s_reset_blend_state = nullptr;
static ID3D11DepthStencilState* s_reset_depth_state = nullptr;
static ID3D11RasterizerState* s_reset_rast_state = nullptr;

static ID3D11Texture2D* s_screenshot_texture = nullptr;
static D3DTexture2D* s_3d_vision_texture = nullptr;

static GXPipelineState s_gx_state;
static StateCache s_gx_state_cache;

static void SetupDeviceObjects()
{
  s_television.Init();

  HRESULT hr;

  D3D11_DEPTH_STENCIL_DESC ddesc;
  ddesc.DepthEnable = FALSE;
  ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  ddesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
  ddesc.StencilEnable = FALSE;
  ddesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
  ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
  hr = D3D::device->CreateDepthStencilState(&ddesc, &s_clear_depth_states[0]);
  CHECK(hr == S_OK, "Create depth state for Renderer::ClearScreen");
  ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  ddesc.DepthEnable = TRUE;
  hr = D3D::device->CreateDepthStencilState(&ddesc, &s_clear_depth_states[1]);
  CHECK(hr == S_OK, "Create depth state for Renderer::ClearScreen");
  ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  hr = D3D::device->CreateDepthStencilState(&ddesc, &s_clear_depth_states[2]);
  CHECK(hr == S_OK, "Create depth state for Renderer::ClearScreen");
  D3D::SetDebugObjectName(s_clear_depth_states[0],
                          "depth state for Renderer::ClearScreen (depth buffer disabled)");
  D3D::SetDebugObjectName(
      s_clear_depth_states[1],
      "depth state for Renderer::ClearScreen (depth buffer enabled, writing enabled)");
  D3D::SetDebugObjectName(
      s_clear_depth_states[2],
      "depth state for Renderer::ClearScreen (depth buffer enabled, writing disabled)");

  D3D11_BLEND_DESC blenddesc;
  blenddesc.AlphaToCoverageEnable = FALSE;
  blenddesc.IndependentBlendEnable = FALSE;
  blenddesc.RenderTarget[0].BlendEnable = FALSE;
  blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  hr = D3D::device->CreateBlendState(&blenddesc, &s_reset_blend_state);
  CHECK(hr == S_OK, "Create blend state for Renderer::ResetAPIState");
  D3D::SetDebugObjectName(s_reset_blend_state, "blend state for Renderer::ResetAPIState");

  s_clear_blend_states[0] = s_reset_blend_state;
  s_reset_blend_state->AddRef();

  blenddesc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
  hr = D3D::device->CreateBlendState(&blenddesc, &s_clear_blend_states[1]);
  CHECK(hr == S_OK, "Create blend state for Renderer::ClearScreen");

  blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
  hr = D3D::device->CreateBlendState(&blenddesc, &s_clear_blend_states[2]);
  CHECK(hr == S_OK, "Create blend state for Renderer::ClearScreen");

  blenddesc.RenderTarget[0].RenderTargetWriteMask = 0;
  hr = D3D::device->CreateBlendState(&blenddesc, &s_clear_blend_states[3]);
  CHECK(hr == S_OK, "Create blend state for Renderer::ClearScreen");

  ddesc.DepthEnable = FALSE;
  ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  ddesc.DepthFunc = D3D11_COMPARISON_LESS;
  ddesc.StencilEnable = FALSE;
  ddesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
  ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
  hr = D3D::device->CreateDepthStencilState(&ddesc, &s_reset_depth_state);
  CHECK(hr == S_OK, "Create depth state for Renderer::ResetAPIState");
  D3D::SetDebugObjectName(s_reset_depth_state, "depth stencil state for Renderer::ResetAPIState");

  D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false,
                                                          0, 0.f, 0.f, false, false, false, false);
  hr = D3D::device->CreateRasterizerState(&rastdesc, &s_reset_rast_state);
  CHECK(hr == S_OK, "Create rasterizer state for Renderer::ResetAPIState");
  D3D::SetDebugObjectName(s_reset_rast_state, "rasterizer state for Renderer::ResetAPIState");

  s_screenshot_texture = nullptr;
}

// Kill off all device objects
static void TeardownDeviceObjects()
{
  g_framebuffer_manager.reset();

  SAFE_RELEASE(s_clear_blend_states[0]);
  SAFE_RELEASE(s_clear_blend_states[1]);
  SAFE_RELEASE(s_clear_blend_states[2]);
  SAFE_RELEASE(s_clear_blend_states[3]);
  SAFE_RELEASE(s_clear_depth_states[0]);
  SAFE_RELEASE(s_clear_depth_states[1]);
  SAFE_RELEASE(s_clear_depth_states[2]);
  SAFE_RELEASE(s_reset_blend_state);
  SAFE_RELEASE(s_reset_depth_state);
  SAFE_RELEASE(s_reset_rast_state);
  SAFE_RELEASE(s_screenshot_texture);
  SAFE_RELEASE(s_3d_vision_texture);

  s_television.Shutdown();

  s_gx_state_cache.Clear();
}

static void CreateScreenshotTexture()
{
  // We can't render anything outside of the backbuffer anyway, so use the backbuffer size as the
  // screenshot buffer size.
  // This texture is released to be recreated when the window is resized in Renderer::SwapImpl.
  D3D11_TEXTURE2D_DESC scrtex_desc = CD3D11_TEXTURE2D_DESC(
      DXGI_FORMAT_R8G8B8A8_UNORM, D3D::GetBackBufferWidth(), D3D::GetBackBufferHeight(), 1, 1, 0,
      D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
  HRESULT hr = D3D::device->CreateTexture2D(&scrtex_desc, nullptr, &s_screenshot_texture);
  if (hr != S_OK && ARBruteForcer::ch_bruteforce)
  {
    Core::KillDolphinAndRestart();
  }
  CHECK(hr == S_OK, "Create screenshot staging texture");
  D3D::SetDebugObjectName(s_screenshot_texture, "staging screenshot texture");
}

static D3D11_BOX GetScreenshotSourceBox(const TargetRectangle& targetRc)
{
  // Since the screenshot buffer is copied back to the CPU via Map(), we can't access pixels that
  // fall outside the backbuffer bounds. Therefore, when crop is enabled and the target rect is
  // off-screen to the top/left, we clamp the origin at zero, as well as the bottom/right
  // coordinates at the backbuffer dimensions. This will result in a rectangle that can be
  // smaller than the backbuffer, but never larger.
  return CD3D11_BOX(std::max(targetRc.left, 0), std::max(targetRc.top, 0), 0,
                    std::min(D3D::GetBackBufferWidth(), (unsigned int)targetRc.right),
                    std::min(D3D::GetBackBufferHeight(), (unsigned int)targetRc.bottom), 1);
}

static void Create3DVisionTexture(int width, int height)
{
  // Create a staging texture for 3D vision with signature information in the last row.
  // Nvidia 3D Vision supports full SBS, so there is no loss in resolution during this process.
  NVSTEREOIMAGEHEADER header;
  header.dwSignature = NVSTEREO_IMAGE_SIGNATURE;
  header.dwWidth = static_cast<u32>(width * 2);
  header.dwHeight = static_cast<u32>(height + 1);
  header.dwBPP = 32;
  header.dwFlags = 0;

  const u32 pitch = static_cast<u32>(4 * width * 2);
  const auto memory = std::make_unique<u8[]>((height + 1) * pitch);
  u8* image_header_location = &memory[height * pitch];
  std::memcpy(image_header_location, &header, sizeof(header));

  D3D11_SUBRESOURCE_DATA sys_data;
  sys_data.SysMemPitch = pitch;
  sys_data.pSysMem = memory.get();

  s_3d_vision_texture =
      D3DTexture2D::Create(width * 2, height + 1, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT,
                           DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &sys_data);
}

Renderer::Renderer() : ::Renderer(D3D::GetBackBufferWidth(), D3D::GetBackBufferHeight())
{
  g_first_rift_frame = true;
  s_last_multisamples = g_ActiveConfig.iMultisamples;
  s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;
  s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
  s_last_fullscreen_mode = D3D::GetFullscreenState();

  g_framebuffer_manager = std::make_unique<FramebufferManager>(m_target_width, m_target_height);
  SetupDeviceObjects();

  // Setup GX pipeline state
  for (auto& sampler : s_gx_state.samplers)
    sampler.hex = RenderState::GetPointSamplerState().hex;

  s_gx_state.zmode.testenable = false;
  s_gx_state.zmode.updateenable = false;
  s_gx_state.zmode.func = ZMode::NEVER;
  s_gx_state.raster.cullmode = GenMode::CULL_NONE;

  s_gx_state.raster.depth_clip_enable = !g_ActiveConfig.bDisableNearClipping;

  // Clear EFB textures
  constexpr std::array<float, 4> clear_color{{0.f, 0.f, 0.f, 1.f}};
  D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                      clear_color.data());
  D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(),
                                      D3D11_CLEAR_DEPTH, 0.f, 0);

  D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)m_target_width, (float)m_target_height);
  D3D::context->RSSetViewports(1, &vp);
  FramebufferManager::BindEFBRenderTarget();
  D3D::BeginFrame();

  // Action Replay culling code brute-forcing
  // begin searching
  if (ARBruteForcer::ch_bruteforce)
  {
    ARBruteForcer::ch_begin_search = true;
    NOTICE_LOG(VR, "begin searching");
  }
}

Renderer::~Renderer()
{
  // On Oculus SDK 0.5 and below, we called BeginFrame so we need a matching EndFrame, but on 0.6
  // this
  // crashes
  if (g_vr_needs_endframe && g_has_rift && !g_first_rift_frame && g_ActiveConfig.bEnableVR &&
      !g_ActiveConfig.bAsynchronousTimewarp)
  {
    VR_PresentHMDFrame();
  }
  g_first_rift_frame = true;

  TeardownDeviceObjects();
  D3D::EndFrame();
  D3D::Present();
  D3D::Close();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
  D3D::font.DrawTextScaled((float)(left + 1), (float)(top + 1), 20.f, 0.0f, color & 0xFF000000,
                           text);
  D3D::font.DrawTextScaled((float)left, (float)top, 20.f, 0.0f, color, text);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = EFBToScaledX(rc.left);
  result.top = EFBToScaledY(rc.top);
  result.right = EFBToScaledX(rc.right);
  result.bottom = EFBToScaledY(rc.bottom);
  return result;
}

// With D3D, we have to resize the backbuffer if the window changed
// size.
bool Renderer::CheckForResize()
{
  RECT rcWindow;
  GetClientRect(D3D::hWnd, &rcWindow);
  int client_width = rcWindow.right - rcWindow.left;
  int client_height = rcWindow.bottom - rcWindow.top;

  // Sanity check
  if ((client_width != GetBackbufferWidth() || client_height != GetBackbufferHeight()) &&
      client_width >= 4 && client_height >= 4)
  {
    return true;
  }

  return false;
}

void Renderer::SetScissorRect(const EFBRectangle& rc)
{
  TargetRectangle trc;
  // In VR we use the whole EFB instead of just the bpmem.copyTexSrc rectangle passed to this
  // function.
  if (g_has_hmd)
  {
    EFBRectangle sourceRc;
    sourceRc.left = 0;
    sourceRc.right = EFB_WIDTH - 1;
    sourceRc.top = 0;
    sourceRc.bottom = EFB_HEIGHT - 1;
    trc = g_renderer->ConvertEFBRectangle(sourceRc);
    D3D::context->RSSetScissorRects(1, trc.AsRECT());
  }
  else
  {
    trc = ConvertEFBRectangle(rc);
    D3D::context->RSSetScissorRects(1, trc.AsRECT());
  }
}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
//  - GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
//  - GX_PokeAlphaMode (TODO)
//  - GX_PokeAlphaUpdate (TODO)
//  - GX_PokeBlendMode (TODO)
//  - GX_PokeColorUpdate (TODO)
//  - GX_PokeDither (TODO)
//  - GX_PokeDstAlpha (TODO)
//  - GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
  // Convert EFB dimensions to the ones of our render target
  EFBRectangle efbPixelRc;
  efbPixelRc.left = x;
  efbPixelRc.top = y;
  efbPixelRc.right = x + 1;
  efbPixelRc.bottom = y + 1;
  TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

  // Take the mean of the resulting dimensions; TODO: Don't use the center pixel, compute the
  // average color instead
  D3D11_RECT RectToLock;
  if (type == EFBAccessType::PeekColor || type == EFBAccessType::PeekZ)
  {
    RectToLock.left = (targetPixelRc.left + targetPixelRc.right) / 2;
    RectToLock.top = (targetPixelRc.top + targetPixelRc.bottom) / 2;
    RectToLock.right = RectToLock.left + 1;
    RectToLock.bottom = RectToLock.top + 1;
  }
  else
  {
    RectToLock.left = targetPixelRc.left;
    RectToLock.right = targetPixelRc.right;
    RectToLock.top = targetPixelRc.top;
    RectToLock.bottom = targetPixelRc.bottom;
  }

  // Reset any game specific settings.
  ResetAPIState();
  D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, 1.f, 1.f);
  D3D::context->RSSetViewports(1, &vp);
  D3D::SetPointCopySampler();

  // Select copy and read textures depending on if we are doing a color or depth read (since they
  // are different formats).
  D3DTexture2D* source_tex;
  D3DTexture2D* read_tex;
  ID3D11Texture2D* staging_tex;
  if (type == EFBAccessType::PeekColor)
  {
    source_tex = FramebufferManager::GetEFBColorTexture();
    read_tex = FramebufferManager::GetEFBColorReadTexture();
    staging_tex = FramebufferManager::GetEFBColorStagingBuffer();
  }
  else
  {
    source_tex = FramebufferManager::GetEFBDepthTexture();
    read_tex = FramebufferManager::GetEFBDepthReadTexture();
    staging_tex = FramebufferManager::GetEFBDepthStagingBuffer();
  }

  // Select pixel shader (we don't want to average depth samples, instead select the minimum).
  ID3D11PixelShader* copy_pixel_shader;
  if (type == EFBAccessType::PeekZ && g_ActiveConfig.iMultisamples > 1)
    copy_pixel_shader = PixelShaderCache::GetDepthResolveProgram();
  else
    copy_pixel_shader = PixelShaderCache::GetColorCopyProgram(true);

  // Draw a quad to grab the texel we want to read.
  D3D::context->OMSetRenderTargets(1, &read_tex->GetRTV(), nullptr);
  D3D::drawShadedTexQuad(source_tex->GetSRV(), &RectToLock, Renderer::GetTargetWidth(),
                         Renderer::GetTargetHeight(), copy_pixel_shader,
                         VertexShaderCache::GetSimpleVertexShader(),
                         VertexShaderCache::GetSimpleInputLayout());

  // Restore expected game state.
  FramebufferManager::BindEFBRenderTarget();
  RestoreAPIState();

  // Copy the pixel from the renderable to cpu-readable buffer.
  D3D11_BOX box = CD3D11_BOX(0, 0, 0, 1, 1, 1);
  D3D::context->CopySubresourceRegion(staging_tex, 0, 0, 0, 0, read_tex->GetTex(), 0, &box);
  D3D11_MAPPED_SUBRESOURCE map;
  CHECK(D3D::context->Map(staging_tex, 0, D3D11_MAP_READ, 0, &map) == S_OK,
        "Map staging buffer failed");

  // Convert the framebuffer data to the format the game is expecting to receive.
  u32 ret;
  if (type == EFBAccessType::PeekColor)
  {
    u32 val;
    memcpy(&val, map.pData, sizeof(val));

    // our buffers are RGBA, yet a BGRA value is expected
    val = ((val & 0xFF00FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000));

    // check what to do with the alpha channel (GX_PokeAlphaRead)
    PixelEngine::UPEAlphaReadReg alpha_read_mode = PixelEngine::GetAlphaReadMode();

    if (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
    {
      val = RGBA8ToRGBA6ToRGBA8(val);
    }
    else if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
    {
      val = RGBA8ToRGB565ToRGBA8(val);
    }
    if (bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
    {
      val |= 0xFF000000;
    }

    if (alpha_read_mode.ReadMode == 2)
      ret = val;  // GX_READ_NONE
    else if (alpha_read_mode.ReadMode == 1)
      ret = (val | 0xFF000000);  // GX_READ_FF
    else                         /*if(alpha_read_mode.ReadMode == 0)*/
      ret = (val & 0x00FFFFFF);  // GX_READ_00
  }
  else  // type == EFBAccessType::PeekZ
  {
    float val;
    memcpy(&val, map.pData, sizeof(val));

    // depth buffer is inverted in the d3d backend
    val = 1.0f - val;

    if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
    {
      // if Z is in 16 bit format you must return a 16 bit integer
      ret = MathUtil::Clamp<u32>(static_cast<u32>(val * 65536.0f), 0, 0xFFFF);
    }
    else
    {
      ret = MathUtil::Clamp<u32>(static_cast<u32>(val * 16777216.0f), 0, 0xFFFFFF);
    }
  }

  D3D::context->Unmap(staging_tex, 0);
  return ret;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
  ResetAPIState();

  if (type == EFBAccessType::PokeColor)
  {
    D3D11_VIEWPORT vp =
        CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight());
    D3D::context->RSSetViewports(1, &vp);
    FramebufferManager::BindEFBRenderTarget(false);
  }
  else  // if (type == EFBAccessType::PokeZ)
  {
    D3D::stateman->PushBlendState(s_clear_blend_states[3]);
    D3D::stateman->PushDepthState(s_clear_depth_states[1]);

    D3D11_VIEWPORT vp =
        CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetTargetWidth(), (float)GetTargetHeight());

    D3D::context->RSSetViewports(1, &vp);
    FramebufferManager::BindEFBRenderTarget();
  }

  D3D::DrawEFBPokeQuads(type, points, num_points);

  if (type == EFBAccessType::PokeZ)
  {
    D3D::stateman->PopDepthState();
    D3D::stateman->PopBlendState();
  }

  RestoreAPIState();
}

void Renderer::SetViewport()
{
  // reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
  // [0] = width/2
  // [1] = height/2
  // [2] = 16777215 * (farz - nearz)
  // [3] = xorig + width/2 + 342
  // [4] = yorig + height/2 + 342
  // [5] = 16777215 * farz

  // D3D crashes for zero viewports
  if (xfmem.viewport.wd == 0 || xfmem.viewport.ht == 0)
    return;

  int scissorXOff = bpmem.scissorOffset.x * 2;
  int scissorYOff = bpmem.scissorOffset.y * 2;

  float X, Y, Wd, Ht;
  X = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff);
  Y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff);
  Wd = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
  Ht = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
  float min_depth = (xfmem.viewport.farZ - xfmem.viewport.zRange) / 16777216.0f;
  float max_depth = xfmem.viewport.farZ / 16777216.0f;
  if (Wd < 0.0f)
  {
    X += Wd;
    Wd = -Wd;
  }
  if (Ht < 0.0f)
  {
    Y += Ht;
    Ht = -Ht;
  }
  g_requested_viewport = EFBRectangle((int)X, (int)Y, (int)Wd, (int)Ht);

  if (g_viewport_type != VIEW_RENDER_TO_TEXTURE && g_has_hmd && g_ActiveConfig.bEnableVR)
  {
    // In VR we must use the entire EFB, not just the copyTexSrc area that is normally used.
    // So scale from copyTexSrc to entire EFB, and we won't use copyTexSrc during rendering.
    // X = (xfmem.viewport.xOrig - xfmem.viewport.wd - bpmem.copyTexSrcXY.x - (float)scissorXOff) *
    // (float)GetTargetWidth() / (float)bpmem.copyTexSrcWH.x;
    // Y = (xfmem.viewport.yOrig + xfmem.viewport.ht - bpmem.copyTexSrcXY.y - (float)scissorYOff) *
    // (float)GetTargetHeight() / (float)bpmem.copyTexSrcWH.y;
    // Wd = (2.0f * xfmem.viewport.wd) * (float)GetTargetWidth() / (float)bpmem.copyTexSrcWH.x;
    // Ht = (-2.0f * xfmem.viewport.ht) * (float)GetTargetHeight() / (float)bpmem.copyTexSrcWH.y;
    X = 0.0f;
    Y = 0.0f;
    Wd = (float)GetTargetWidth();
    Ht = (float)GetTargetHeight();
  }

  // If an inverted or oversized depth range is used, we need to calculate the depth range in the
  // vertex shader.
  if (UseVertexDepthRange())
  {
    // We need to ensure depth values are clamped the maximum value supported by the console GPU.
    min_depth = 0.0f;
    max_depth = GX_MAX_DEPTH;
  }

  // In D3D, the viewport rectangle must fit within the render target.
  X = (X >= 0.f) ? X : 0.f;
  Y = (Y >= 0.f) ? Y : 0.f;
  Wd = (X + Wd <= GetTargetWidth()) ? Wd : (GetTargetWidth() - X);
  Ht = (Y + Ht <= GetTargetHeight()) ? Ht : (GetTargetHeight() - Y);
  g_rendered_viewport = EFBRectangle((int)X, (int)Y, (int)Wd, (int)Ht);

  // We use an inverted depth range here to apply the Reverse Z trick.
  // This trick makes sure we match the precision provided by the 1:0
  // clipping depth range on the hardware.
  D3D11_VIEWPORT vp = CD3D11_VIEWPORT(X, Y, Wd, Ht, 1.0f - max_depth, 1.0f - min_depth);
  D3D::context->RSSetViewports(1, &vp);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                           u32 color, u32 z)
{
  ResetAPIState();

  if (colorEnable && alphaEnable)
    D3D::stateman->PushBlendState(s_clear_blend_states[0]);
  else if (colorEnable)
    D3D::stateman->PushBlendState(s_clear_blend_states[1]);
  else if (alphaEnable)
    D3D::stateman->PushBlendState(s_clear_blend_states[2]);
  else
    D3D::stateman->PushBlendState(s_clear_blend_states[3]);

  // TODO: Should we enable Z testing here?
  // if (!bpmem.zmode.testenable) D3D::stateman->PushDepthState(s_clear_depth_states[0]);
  // else
  if (zEnable)
    D3D::stateman->PushDepthState(s_clear_depth_states[1]);
  else /*if (!zEnable)*/
    D3D::stateman->PushDepthState(s_clear_depth_states[2]);

  // Update the view port for clearing the picture
  TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
  D3D11_VIEWPORT vp =
      CD3D11_VIEWPORT((float)targetRc.left, (float)targetRc.top, (float)targetRc.GetWidth(),
                      (float)targetRc.GetHeight(), 0.f, 1.f);
  D3D::context->RSSetViewports(1, &vp);
  FramebufferManager::SetIntegerEFBRenderTarget(false);

  // Color is passed in bgra mode so we need to convert it to rgba
  u32 rgbaColor = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000);
  D3D::drawClearQuad(rgbaColor, 1.0f - (z & 0xFFFFFF) / 16777216.0f);

  D3D::stateman->PopDepthState();
  D3D::stateman->PopBlendState();

  RestoreAPIState();
}

void Renderer::SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable)
{
  ResetAPIState();

  if (colorEnable && alphaEnable)
    D3D::stateman->PushBlendState(s_clear_blend_states[0]);
  else if (colorEnable)
    D3D::stateman->PushBlendState(s_clear_blend_states[1]);
  else if (alphaEnable)
    D3D::stateman->PushBlendState(s_clear_blend_states[2]);
  else
    D3D::stateman->PushBlendState(s_clear_blend_states[3]);

  // TODO: Should we enable Z testing here?
  /*if (!bpmem.zmode.testenable) D3D::stateman->PushDepthState(s_clear_depth_states[0]);
	else */ if (zEnable)
    D3D::stateman->PushDepthState(s_clear_depth_states[1]);
  else /*if (!zEnable)*/
    D3D::stateman->PushDepthState(s_clear_depth_states[2]);

  // To Do: Not needed?
  // D3D::context->VSSetShader(VertexShaderCache::GetClearVertexShader(), nullptr, 0);
  // D3D::context->PSSetShader(PixelShaderCache::GetClearProgram(), nullptr, 0);
  // D3D::context->IASetInputLayout(VertexShaderCache::GetClearInputLayout());

  D3D::stateman->Apply();

  D3D::stateman->PopDepthState();
  D3D::stateman->PopBlendState();

  RestoreAPIState();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
  // TODO: MSAA support..
  D3D11_RECT source = CD3D11_RECT(0, 0, GetTargetWidth(), GetTargetHeight());

  ID3D11PixelShader* pixel_shader;
  if (convtype == 0)
    pixel_shader = PixelShaderCache::ReinterpRGB8ToRGBA6(true);
  else if (convtype == 2)
    pixel_shader = PixelShaderCache::ReinterpRGBA6ToRGB8(true);
  else
  {
    ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d",
              convtype);
    return;
  }

  // convert data and set the target texture as our new EFB
  ResetAPIState();

  D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, static_cast<float>(GetTargetWidth()),
                                      static_cast<float>(GetTargetHeight()));
  D3D::context->RSSetViewports(1, &vp);

  D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTempTexture()->GetRTV(),
                                   nullptr);
  D3D::SetPointCopySampler();
  D3D::drawShadedTexQuad(
      FramebufferManager::GetEFBColorTexture()->GetSRV(), &source, GetTargetWidth(),
      GetTargetHeight(), pixel_shader, VertexShaderCache::GetSimpleVertexShader(),
      VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader());

  RestoreAPIState();

  FramebufferManager::SwapReinterpretTexture();
  FramebufferManager::BindEFBRenderTarget();
}

void Renderer::SetBlendingState(const BlendingState& state)
{
  s_gx_state.blend.hex = state.hex;
}

void Renderer::AsyncTimewarpDraw()
{
  // TODO: D3D11 Asynchronous Timewarp
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight,
                        const EFBRectangle& rc, u64 ticks, float Gamma)
{
  // rafa
  if (ARBruteForcer::ch_bruteforce)
  {
    ARBruteForcer::ch_last_search = true;
    WARN_LOG(VR, "last search");
  }

  // VR - before the first frame we need BeginFrame, and we need to configure the tracking
  if (g_first_rift_frame && g_has_hmd && g_ActiveConfig.bEnableVR)
  {
    if (!g_ActiveConfig.bAsynchronousTimewarp)
    {
      VR_BeginFrame();
      VR_GetEyePoses();
    }
    g_first_rift_frame = false;

    VR_ConfigureHMDTracking();
  }

  // With frame skipping (or if the GC/Wii didn't draw anything), return without drawing anything
  // but if we are recording an .avi file, save the frame to disk again to maintain the right frame
  // rate.
  if (Fifo::WillSkipCurrentFrame() || (!m_xfb_written && !g_ActiveConfig.RealXFBEnabled()) ||
      !fbWidth || !fbHeight)
  {
    Core::Callback_VideoCopiedToXFB(false);
    return;
  }

  // Check what XFB we are supposed to draw
  // If we are using virtual XFB, but there is no XFB, then count it as an empty frame and exit
  u32 xfbCount = 0;
  const XFBSourceBase* const* xfbSourceList =
      FramebufferManager::GetXFBSource(xfbAddr, fbStride, fbHeight, &xfbCount);
  if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
  {
    Core::Callback_VideoCopiedToXFB(false);
    return;
  }

  ResetAPIState();

  // Prepare to copy the XFBs to our backbuffer
  UpdateDrawRectangle();
  TargetRectangle targetRc = GetTargetRectangle();

  // TODO: Do we still need to set and clear the backbuffer like this in VR mode with the Oculus
  // Rift?
  D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);

  constexpr std::array<float, 4> clear_color{{0.f, 0.f, 0.f, 1.f}};
  D3D::context->ClearRenderTargetView(D3D::GetBackBuffer()->GetRTV(), clear_color.data());

  // activate linear filtering for the buffer copies
  D3D::SetLinearCopySampler();

  if (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB)
  {
    // TODO: Television should be used to render Virtual XFB mode as well.
    D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)targetRc.left, (float)targetRc.top,
                                        (float)targetRc.GetWidth(), (float)targetRc.GetHeight());
    D3D::context->RSSetViewports(1, &vp);

    s_television.Submit(xfbAddr, fbStride, fbWidth, fbHeight);
    s_television.Render();
  }
  else if (g_has_hmd && g_ActiveConfig.bEnableVR)
  {
    // Draw our Razer Hydra
    if (!g_ActiveConfig.bUseXFB)
      DX11::s_avatarDrawer.Draw();

    EFBRectangle sourceRc;
    // In VR we use the whole EFB instead of just the bpmem.copyTexSrc rectangle passed to this
    // function.
    sourceRc.left = 0;
    sourceRc.right = EFB_WIDTH;
    sourceRc.top = 0;
    sourceRc.bottom = EFB_HEIGHT;
    targetRc = ConvertEFBRectangle(sourceRc);

    if (g_ActiveConfig.bUseXFB)
    {
      const XFBSource* xfbSource;

      // draw each xfb source
      for (u32 i = 0; i < xfbCount; ++i)
      {
        xfbSource = (const XFBSource*)xfbSourceList[i];

        TargetRectangle drawRc;

        // use virtual xfb with offset
        int xfbHeight = xfbSource->srcHeight;
        int xfbWidth = xfbSource->srcWidth;
        int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

        drawRc.top = targetRc.top + hOffset * targetRc.GetHeight() / (s32)fbHeight;
        drawRc.bottom = targetRc.top + (hOffset + xfbHeight) * targetRc.GetHeight() / (s32)fbHeight;
        drawRc.left = targetRc.left +
                      (targetRc.GetWidth() - xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;
        drawRc.right = targetRc.left +
                       (targetRc.GetWidth() + xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;

        TargetRectangle xfbTexRc;
        xfbTexRc.left = 0;
        xfbTexRc.top = 0;
        xfbTexRc.right = (int)xfbSource->texWidth;
        xfbTexRc.bottom = (int)xfbSource->texHeight;

        xfbTexRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);

        D3DTexture2D* read_texture = xfbSource->tex;

        D3D11_VIEWPORT Vp = CD3D11_VIEWPORT((float)drawRc.left, (float)drawRc.top,
                                            (float)drawRc.GetWidth(), (float)drawRc.GetHeight());

        // Render to left eye
        VR_RenderToEyebuffer(0);
        D3D::context->RSSetViewports(1, &Vp);
        D3D::drawShadedTexQuad(read_texture->GetSRV(), xfbTexRc.AsRECT(), xfbSource->texWidth,
                               xfbSource->texHeight, PixelShaderCache::GetColorCopyProgram(false),
                               VertexShaderCache::GetSimpleVertexShader(),
                               VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);
        if (g_has_two_hmds)
        {
          VR_RenderToEyebuffer(0, 1);
          D3D::drawShadedTexQuad(read_texture->GetSRV(), xfbTexRc.AsRECT(), xfbSource->texWidth,
                                 xfbSource->texHeight, PixelShaderCache::GetColorCopyProgram(false),
                                 VertexShaderCache::GetSimpleVertexShader(),
                                 VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);
        }

        // Render to right eye
        VR_RenderToEyebuffer(1);
        D3D::drawShadedTexQuad(read_texture->GetSRV(), xfbTexRc.AsRECT(), xfbSource->texWidth,
                               xfbSource->texHeight, PixelShaderCache::GetColorCopyProgram(false),
                               VertexShaderCache::GetSimpleVertexShader(),
                               VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
        if (g_has_two_hmds)
        {
          VR_RenderToEyebuffer(1, 1);
          D3D::drawShadedTexQuad(read_texture->GetSRV(), xfbTexRc.AsRECT(), xfbSource->texWidth,
                                 xfbSource->texHeight, PixelShaderCache::GetColorCopyProgram(false),
                                 VertexShaderCache::GetSimpleVertexShader(),
                                 VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
        }
      }
    }
    else
    {
      // TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled
      // EFB texture as source
      D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();

      D3D11_VIEWPORT Vp = CD3D11_VIEWPORT((float)0, (float)0, (float)Renderer::GetTargetWidth(),
                                          (float)Renderer::GetTargetHeight());

      // Render to left eye
      VR_RenderToEyebuffer(0);
      D3D::context->RSSetViewports(1, &Vp);
      D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(), Renderer::GetTargetWidth(),
                             Renderer::GetTargetHeight(),
                             PixelShaderCache::GetColorCopyProgram(false),
                             VertexShaderCache::GetSimpleVertexShader(),
                             VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);
      if (g_has_two_hmds)
      {
        VR_RenderToEyebuffer(0, 1);
        D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(),
                               Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
                               PixelShaderCache::GetColorCopyProgram(false),
                               VertexShaderCache::GetSimpleVertexShader(),
                               VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);
      }

      // Render to right eye
      VR_RenderToEyebuffer(1);
      D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(), Renderer::GetTargetWidth(),
                             Renderer::GetTargetHeight(),
                             PixelShaderCache::GetColorCopyProgram(false),
                             VertexShaderCache::GetSimpleVertexShader(),
                             VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
      if (g_has_two_hmds)
      {
        VR_RenderToEyebuffer(1, 1);
        D3D::drawShadedTexQuad(read_texture->GetSRV(), targetRc.AsRECT(),
                               Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
                               PixelShaderCache::GetColorCopyProgram(false),
                               VertexShaderCache::GetSimpleVertexShader(),
                               VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
      }
    }
    // Reset viewport for drawing text
    D3D11_VIEWPORT vp = CD3D11_VIEWPORT(400.0f, 400.0f, Renderer::GetTargetWidth() - 400.0f,
                                        Renderer::GetTargetHeight() - 400.0f);
    D3D::context->RSSetViewports(1, &vp);
    Renderer::DrawDebugText();
    OSD::DrawMessages();

    if (!g_ActiveConfig.bAsynchronousTimewarp)
    {
      VR_PresentHMDFrame();
      Core::g_drawn_vr++;

      // VR Synchronous Timewarp
      static int real_frame_count_for_timewarp = 0;
      SConfig& startup_parameter = SConfig::GetInstance();

      static int timewarp_count = 0;
      static int old_rate = 0;
      int real_framerate = ((int)((g_current_fps + 2.5) / 5)) * 5;
      if (real_framerate != old_rate)
      {
        // MessageBeep(MB_ICONASTERISK);
        WARN_LOG(VR, "new FPS = %d", real_framerate);
        timewarp_count = 0;
        old_rate = real_framerate;
      }
      if (real_framerate < 19 || real_framerate > g_hmd_refresh_rate ||
          (g_vr_has_asynchronous_timewarp && g_current_speed < 95.0f))
      {
        real_framerate = g_hmd_refresh_rate;
        timewarp_count = 0;
      }
      int timewarps_per_second = 0;

      if (g_ActiveConfig.bSynchronousTimewarp)
      {
        if ((startup_parameter.bSkipIdle && startup_parameter.bSyncGPUOnSkipIdleHack) ||
            startup_parameter.bSyncGPU ||
            startup_parameter.m_GPUDeterminismMode == GPU_DETERMINISM_FAKE_COMPLETION ||
            !startup_parameter.bCPUThread || SConfig::GetInstance().m_EmulationSpeed == 1.0f)
          SConfig::GetInstance().m_AudioSlowDown = 1.00;
        else
          SConfig::GetInstance().m_AudioSlowDown = 1.25;

        if (g_ActiveConfig.bPullUpAutoTimewarp)
        {
          timewarps_per_second = g_hmd_refresh_rate - real_framerate;
          timewarp_count += timewarps_per_second;
          u32 extra = 0;
          while (timewarp_count >= real_framerate)
          {
            ++extra;
            timewarp_count -= real_framerate;
          }
          g_ActiveConfig.iExtraTimewarpedFrames = extra;
        }
        else if (g_hmd_refresh_rate == 75)
        {
          if (g_ActiveConfig.bPullUp20fpsTimewarp)
          {
            if (real_frame_count_for_timewarp % 4 == 1)
              g_ActiveConfig.iExtraTimewarpedFrames = 2;
            else
              g_ActiveConfig.iExtraTimewarpedFrames = 3;
          }
          else if (g_ActiveConfig.bPullUp30fpsTimewarp)
          {
            if (real_frame_count_for_timewarp % 2 == 1)
              g_ActiveConfig.iExtraTimewarpedFrames = 1;
            else
              g_ActiveConfig.iExtraTimewarpedFrames = 2;
          }
          else if (g_ActiveConfig.bPullUp60fpsTimewarp)
          {
            if (real_frame_count_for_timewarp % 4 == 0)
              g_ActiveConfig.iExtraTimewarpedFrames = 1;
            else
              g_ActiveConfig.iExtraTimewarpedFrames = 0;
          }
        }
        else
        {
          // 90 FPS
          if (g_ActiveConfig.bPullUp20fpsTimewarp)
          {
            if (real_frame_count_for_timewarp % 2 == 0)
              g_ActiveConfig.iExtraTimewarpedFrames = 4;
            else
              g_ActiveConfig.iExtraTimewarpedFrames = 3;
          }
          else if (g_ActiveConfig.bPullUp30fpsTimewarp)
          {
            g_ActiveConfig.iExtraTimewarpedFrames = 2;
          }
          else if (g_ActiveConfig.bPullUp60fpsTimewarp)
          {
            if (real_frame_count_for_timewarp % 2 == 0)
              g_ActiveConfig.iExtraTimewarpedFrames = 1;
            else
              g_ActiveConfig.iExtraTimewarpedFrames = 0;
          }
        }
        ++real_frame_count_for_timewarp;
      }
      else if (g_opcode_replay_enabled)
      {
        if ((startup_parameter.bSkipIdle && startup_parameter.bSyncGPUOnSkipIdleHack) ||
            startup_parameter.bSyncGPU ||
            startup_parameter.m_GPUDeterminismMode == GPU_DETERMINISM_FAKE_COMPLETION ||
            !startup_parameter.bCPUThread || SConfig::GetInstance().m_EmulationSpeed == 1.0f)
          SConfig::GetInstance().m_AudioSlowDown = 1.00;
        else
          SConfig::GetInstance().m_AudioSlowDown = 1.25;
        g_ActiveConfig.iExtraTimewarpedFrames = 0;
        timewarp_count = 0;
      }
      else
      {
        SConfig::GetInstance().m_AudioSlowDown = startup_parameter.fAudioSlowDown;
        timewarp_count = 0;
      }

      // If 30fps loop once, if 20fps (Zelda: OoT for instance) loop twice.
      for (int i = 0; i < (int)g_ActiveConfig.iExtraTimewarpedFrames; ++i)
      {
        VR_DrawTimewarpFrame();
        Core::g_drawn_vr++;
      }
    }
    else
    {
      // VR TODO - Direct3D Asynchronous timewarp
    }
  }
  else if (g_ActiveConfig.bUseXFB)
  {
    // draw each xfb source
    for (u32 i = 0; i < xfbCount; ++i)
    {
      const auto* const xfbSource = static_cast<const XFBSource*>(xfbSourceList[i]);

      // use virtual xfb with offset
      int xfbHeight = xfbSource->srcHeight;
      int xfbWidth = xfbSource->srcWidth;
      int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

      TargetRectangle drawRc;
      drawRc.top = targetRc.top + hOffset * targetRc.GetHeight() / (s32)fbHeight;
      drawRc.bottom = targetRc.top + (hOffset + xfbHeight) * targetRc.GetHeight() / (s32)fbHeight;
      drawRc.left = targetRc.left +
                    (targetRc.GetWidth() - xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;
      drawRc.right = targetRc.left +
                     (targetRc.GetWidth() + xfbWidth * targetRc.GetWidth() / (s32)fbStride) / 2;

      // The following code disables auto stretch.  Kept for reference.
      // scale draw area for a 1 to 1 pixel mapping with the draw target
      // float vScale = (float)fbHeight / (float)s_backbuffer_height;
      // float hScale = (float)fbWidth / (float)s_backbuffer_width;
      // drawRc.top *= vScale;
      // drawRc.bottom *= vScale;
      // drawRc.left *= hScale;
      // drawRc.right *= hScale;

      TargetRectangle sourceRc;
      sourceRc.left = xfbSource->sourceRc.left;
      sourceRc.top = xfbSource->sourceRc.top;
      sourceRc.right = xfbSource->sourceRc.right;
      sourceRc.bottom = xfbSource->sourceRc.bottom;

      sourceRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);

      BlitScreen(sourceRc, drawRc, xfbSource->tex, xfbSource->texWidth, xfbSource->texHeight,
                 Gamma);
    }
  }
  else
  {
    TargetRectangle sourceRc = Renderer::ConvertEFBRectangle(rc);

    // TODO: Improve sampling algorithm for the pixel shader so that we can use the multisampled EFB
    // texture as source
    D3DTexture2D* read_texture = FramebufferManager::GetResolvedEFBColorTexture();
    BlitScreen(sourceRc, targetRc, read_texture, GetTargetWidth(), GetTargetHeight(), Gamma);
  }

  // Enable screenshot and write csv if bruteforcing is on
  if (ARBruteForcer::ch_bruteforce && ARBruteForcer::ch_take_screenshot > 0)
    ARBruteForcer::SetupScreenshotAndWriteCSV(this);
  else if (ARBruteForcer::ch_bruteforce)
    WARN_LOG(VR, "ch_take_screenshot = %d", ARBruteForcer::ch_take_screenshot);

// Dump frames
#if defined(HAVE_FFMPEG)
  if (IsFrameDumping())
  {
    if (!s_screenshot_texture)
      CreateScreenshotTexture();

    D3D11_BOX source_box = GetScreenshotSourceBox(targetRc);
    unsigned int source_width = source_box.right - source_box.left;
    unsigned int source_height = source_box.bottom - source_box.top;
    D3D::context->CopySubresourceRegion(s_screenshot_texture, 0, 0, 0, 0,
                                        D3D::GetBackBuffer()->GetTex(), 0, &source_box);

    D3D11_MAPPED_SUBRESOURCE map;
    D3D::context->Map(s_screenshot_texture, 0, D3D11_MAP_READ, 0, &map);

    AVIDump::Frame state = AVIDump::FetchState(ticks);
    DumpFrameData(reinterpret_cast<const u8*>(map.pData), source_width, source_height, map.RowPitch,
                  state);
    FinishFrameData();

    D3D::context->Unmap(s_screenshot_texture, 0);
  }
#endif

  if (!g_has_hmd)
  {
    // Reset viewport for drawing text
    D3D11_VIEWPORT vp =
        CD3D11_VIEWPORT(0.0f, 0.0f, (float)GetBackbufferWidth(), (float)GetBackbufferHeight());
    D3D::context->RSSetViewports(1, &vp);

    Renderer::DrawDebugText();

    OSD::DrawMessages();
  }
  D3D::EndFrame();

  g_texture_cache->Cleanup(frameCount);

  if (g_has_hmd)
  {
    if (g_Config.bLowPersistence != g_ActiveConfig.bLowPersistence ||
        g_Config.bDynamicPrediction != g_ActiveConfig.bDynamicPrediction ||
        (g_Config.iMirrorPlayer == VR_PLAYER_NONE) !=
            (g_ActiveConfig.iMirrorPlayer == VR_PLAYER_NONE) ||
        (g_Config.iMirrorStyle == VR_MIRROR_DISABLED) !=
            (g_ActiveConfig.iMirrorStyle == VR_MIRROR_DISABLED))
    {
      VR_ConfigureHMDPrediction();
    }

    if (g_Config.bOrientationTracking != g_ActiveConfig.bOrientationTracking ||
        g_Config.bMagYawCorrection != g_ActiveConfig.bMagYawCorrection ||
        g_Config.bPositionTracking != g_ActiveConfig.bPositionTracking)
    {
      VR_ConfigureHMDTracking();
    }

    if (g_Config.bChromatic != g_ActiveConfig.bChromatic ||
        g_Config.bTimewarp != g_ActiveConfig.bTimewarp ||
        g_Config.bVignette != g_ActiveConfig.bVignette ||
        g_Config.bNoRestore != g_ActiveConfig.bNoRestore ||
        g_Config.bFlipVertical != g_ActiveConfig.bFlipVertical ||
        g_Config.bSRGB != g_ActiveConfig.bSRGB ||
        g_Config.bOverdrive != g_ActiveConfig.bOverdrive ||
        g_Config.bHqDistortion != g_ActiveConfig.bHqDistortion || g_fov_changed)
    {
      VR_ConfigureHMD();
    }
  }

  // VR layer debugging, sometimes layers need to flash.
  g_Config.iFlashState++;
  if (g_Config.iFlashState >= 10)
    g_Config.iFlashState = 0;

  // Enable configuration changes
  UpdateActiveConfig();
  // VR Real XFB isn't implemented yet, so always disable it for VR
  if (g_has_hmd && g_ActiveConfig.bEnableVR)
  {
    g_ActiveConfig.bUseRealXFB = false;
    // always stretch to fit
    g_ActiveConfig.iAspectRatio = 3;
  }
  g_texture_cache->OnConfigChanged(g_ActiveConfig);
  VertexShaderCache::RetreiveAsyncShaders();
  if (g_has_hmd && g_ActiveConfig.bEnableVR && !g_ActiveConfig.bAsynchronousTimewarp)
    VR_BeginFrame();

  SetWindowSize(fbStride, fbHeight);

  bool window_resized = CheckForResize();
  const bool fullscreen = D3D::GetFullscreenState();
  const bool fs_changed = s_last_fullscreen_mode != fullscreen;

  bool xfbchanged = s_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

  if (FramebufferManagerBase::LastXfbWidth() != fbStride ||
      FramebufferManagerBase::LastXfbHeight() != fbHeight)
  {
    xfbchanged = true;
    unsigned int xfb_w = (fbStride < 1 || fbStride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbStride;
    unsigned int xfb_h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
    FramebufferManagerBase::SetLastXfbWidth(xfb_w);
    FramebufferManagerBase::SetLastXfbHeight(xfb_h);
  }

  // Flip/present backbuffer to frontbuffer here
  if (!g_has_hmd)
    D3D::Present();

  VR_NewVRFrame();

  // Resize the back buffers NOW to avoid flickering
  if (CalculateTargetSize() || xfbchanged || window_resized || fs_changed ||
      s_last_multisamples != g_ActiveConfig.iMultisamples ||
      s_last_stereo_mode != (g_ActiveConfig.iStereoMode > 0))
  {
    s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;
    s_last_multisamples = g_ActiveConfig.iMultisamples;
    s_last_fullscreen_mode = fullscreen;
    PixelShaderCache::InvalidateMSAAShaders();

    if (window_resized || fs_changed)
    {
      // TODO: Aren't we still holding a reference to the back buffer right now?
      D3D::Reset();
      SAFE_RELEASE(s_screenshot_texture);
      SAFE_RELEASE(s_3d_vision_texture);
      m_backbuffer_width = D3D::GetBackBufferWidth();
      m_backbuffer_height = D3D::GetBackBufferHeight();
    }

    UpdateDrawRectangle();

    s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;

    D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);

    if (g_ActiveConfig.bAsynchronousTimewarp)
      g_vr_lock.lock();
    g_framebuffer_manager.reset();
    g_framebuffer_manager = std::make_unique<FramebufferManager>(m_target_width, m_target_height);

    D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                        clear_color.data());
    D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(),
                                        D3D11_CLEAR_DEPTH, 0.f, 0);
    if (g_ActiveConfig.bAsynchronousTimewarp)
      g_vr_lock.unlock();
  }
  else if (g_has_hmd && !g_ActiveConfig.bDontClearScreen)
  {
    // cegli - clearing the screen here causes flickering in games that fake 60fps by only actually
    // updating
    // the entire screen once every 2 frames.  They rely on the fact that nothing is cleared on the
    // fake frame.
    // An example of this is Beyond Good and Evil. Removing it aligns D3D with OGL, but adds the
    // same smearing
    // problem OGL has in the BG&E menu.  Without clearing the screen, some games like PM: TTYD have
    // smearing
    // around the edges.  How does OGL handle this gracefully?
    // To Do: Figure out the best thing to do here.


    // VR Clear screen before every frame
    float clear_col[4] = {0.f, 0.f, 0.f, 1.f};
    D3D::context->ClearRenderTargetView(FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                        clear_col);
    D3D::context->ClearDepthStencilView(FramebufferManager::GetEFBDepthTexture()->GetDSV(),
                                        D3D11_CLEAR_DEPTH, 0.f, 0);
  }

  if (CheckForHostConfigChanges())
  {
    VertexShaderCache::Reload();
    GeometryShaderCache::Reload();
    PixelShaderCache::Reload();
  }

  // begin next frame
  RestoreAPIState();
  D3D::BeginFrame();
  FramebufferManager::BindEFBRenderTarget();
  SetViewport();
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
  D3D::stateman->PushBlendState(s_reset_blend_state);
  D3D::stateman->PushDepthState(s_reset_depth_state);
  D3D::stateman->PushRasterizerState(s_reset_rast_state);
}

void Renderer::RestoreAPIState()
{
  // Gets us back into a more game-like state.
  D3D::stateman->PopBlendState();
  D3D::stateman->PopDepthState();
  D3D::stateman->PopRasterizerState();
  SetViewport();
  BPFunctions::SetScissor();
  s_gx_state.raster.depth_clip_enable = !g_ActiveConfig.bDisableNearClipping;
}

void Renderer::ApplyState()
{
  D3D::stateman->PushBlendState(s_gx_state_cache.Get(s_gx_state.blend));
  D3D::stateman->PushDepthState(s_gx_state_cache.Get(s_gx_state.zmode));
  D3D::stateman->PushRasterizerState(s_gx_state_cache.Get(s_gx_state.raster));
  D3D::stateman->SetPrimitiveTopology(
      StateCache::GetPrimitiveTopology(s_gx_state.raster.primitive));
  FramebufferManager::SetIntegerEFBRenderTarget(s_gx_state.blend.logicopenable != 0);

  for (u32 stage = 0; stage < static_cast<u32>(s_gx_state.samplers.size()); stage++)
    D3D::stateman->SetSampler(stage, s_gx_state_cache.Get(s_gx_state.samplers[stage]));

  ID3D11Buffer* vertexConstants = VertexShaderCache::GetConstantBuffer();

  D3D::stateman->SetPixelConstants(PixelShaderCache::GetConstantBuffer(),
                                   g_ActiveConfig.bEnablePixelLighting ? vertexConstants : nullptr);
  D3D::stateman->SetVertexConstants(vertexConstants);
  D3D::stateman->SetGeometryConstants(GeometryShaderCache::GetConstantBuffer());
}

void Renderer::RestoreState()
{
  D3D::stateman->PopBlendState();
  D3D::stateman->PopDepthState();
  D3D::stateman->PopRasterizerState();
}

void Renderer::SetRasterizationState(const RasterizationState& state)
{
  s_gx_state.raster.hex = state.hex;
}

void Renderer::SetDepthState(const DepthState& state)
{
  s_gx_state.zmode.hex = state.hex;
}

void Renderer::SetSamplerState(u32 index, const SamplerState& state)
{
  s_gx_state.samplers[index].hex = state.hex;
}

void Renderer::SetInterlacingMode()
{
  // TODO
}

u16 Renderer::BBoxRead(int index)
{
  // Here we get the min/max value of the truncated position of the upscaled framebuffer.
  // So we have to correct them to the unscaled EFB sizes.
  int value = BBox::Get(index);

  if (index < 2)
  {
    // left/right
    value = value * EFB_WIDTH / m_target_width;
  }
  else
  {
    // up/down
    value = value * EFB_HEIGHT / m_target_height;
  }
  if (index & 1)
    value++;  // fix max values to describe the outer border

  return value;
}

void Renderer::BBoxWrite(int index, u16 _value)
{
  int value = _value;  // u16 isn't enough to multiply by the efb width
  if (index & 1)
    value--;
  if (index < 2)
  {
    value = value * m_target_width / EFB_WIDTH;
  }
  else
  {
    value = value * m_target_height / EFB_HEIGHT;
  }

  BBox::Set(index, value);
}

void Renderer::BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture,
                          u32 src_width, u32 src_height, float Gamma)
{
  if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB ||
      g_ActiveConfig.iStereoMode == STEREO_OSVR)
  {
    TargetRectangle leftRc, rightRc;
    std::tie(leftRc, rightRc) = ConvertStereoRectangle(dst);

    D3D11_VIEWPORT leftVp = CD3D11_VIEWPORT((float)leftRc.left, (float)leftRc.top,
                                            (float)leftRc.GetWidth(), (float)leftRc.GetHeight());
    D3D11_VIEWPORT rightVp = CD3D11_VIEWPORT((float)rightRc.left, (float)rightRc.top,
                                             (float)rightRc.GetWidth(), (float)rightRc.GetHeight());

    D3D::context->RSSetViewports(1, &leftVp);
    D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height,
                           (g_Config.iStereoMode == STEREO_OSVR) ?
                               PixelShaderCache::GetOSVRProgram() :
                               PixelShaderCache::GetColorCopyProgram(false),
                           VertexShaderCache::GetSimpleVertexShader(),
                           VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);

    D3D::context->RSSetViewports(1, &rightVp);
    D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height,
                           (g_Config.iStereoMode == STEREO_OSVR) ?
                               PixelShaderCache::GetOSVRProgram() :
                               PixelShaderCache::GetColorCopyProgram(false),
                           VertexShaderCache::GetSimpleVertexShader(),
                           VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);
  }
  else if (g_ActiveConfig.iStereoMode == STEREO_3DVISION)
  {
    if (!s_3d_vision_texture)
      Create3DVisionTexture(m_backbuffer_width, m_backbuffer_height);

    D3D11_VIEWPORT leftVp = CD3D11_VIEWPORT((float)dst.left, (float)dst.top, (float)dst.GetWidth(),
                                            (float)dst.GetHeight());
    D3D11_VIEWPORT rightVp = CD3D11_VIEWPORT((float)(dst.left + m_backbuffer_width), (float)dst.top,
                                             (float)dst.GetWidth(), (float)dst.GetHeight());

    // Render to staging texture which is double the width of the backbuffer
    D3D::context->OMSetRenderTargets(1, &s_3d_vision_texture->GetRTV(), nullptr);

    D3D::context->RSSetViewports(1, &leftVp);
    D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height,
                           PixelShaderCache::GetColorCopyProgram(false),
                           VertexShaderCache::GetSimpleVertexShader(),
                           VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 0);

    D3D::context->RSSetViewports(1, &rightVp);
    D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height,
                           PixelShaderCache::GetColorCopyProgram(false),
                           VertexShaderCache::GetSimpleVertexShader(),
                           VertexShaderCache::GetSimpleInputLayout(), nullptr, Gamma, 1);

    // Copy the left eye to the backbuffer, if Nvidia 3D Vision is enabled it should
    // recognize the signature and automatically include the right eye frame.
    D3D11_BOX box = CD3D11_BOX(0, 0, 0, m_backbuffer_width, m_backbuffer_height, 1);
    D3D::context->CopySubresourceRegion(D3D::GetBackBuffer()->GetTex(), 0, 0, 0, 0,
                                        s_3d_vision_texture->GetTex(), 0, &box);

    // Restore render target to backbuffer
    D3D::context->OMSetRenderTargets(1, &D3D::GetBackBuffer()->GetRTV(), nullptr);
  }
  else
  {
    D3D11_VIEWPORT vp = CD3D11_VIEWPORT((float)dst.left, (float)dst.top, (float)dst.GetWidth(),
                                        (float)dst.GetHeight());
    D3D::context->RSSetViewports(1, &vp);

    ID3D11PixelShader* pixelShader = (g_Config.iStereoMode == STEREO_ANAGLYPH) ?
                                         PixelShaderCache::GetAnaglyphProgram() :
                                         PixelShaderCache::GetColorCopyProgram(false);
    ID3D11GeometryShader* geomShader = (g_ActiveConfig.iStereoMode == STEREO_QUADBUFFER) ?
                                           GeometryShaderCache::GetCopyGeometryShader() :
                                           nullptr;
    D3D::drawShadedTexQuad(src_texture->GetSRV(), src.AsRECT(), src_width, src_height, pixelShader,
                           VertexShaderCache::GetSimpleVertexShader(),
                           VertexShaderCache::GetSimpleInputLayout(), geomShader, Gamma);
  }
}

void Renderer::SetFullscreen(bool enable_fullscreen)
{
  D3D::SetFullscreenState(enable_fullscreen);
}

bool Renderer::IsFullscreen() const
{
  return D3D::GetFullscreenState();
}

}  // namespace DX11
