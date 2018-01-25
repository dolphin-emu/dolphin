// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Core.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/MetroidVR.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VRTracker.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

#define HACK_LOG INFO_LOG

static float GC_ALIGNED16(g_fProjectionMatrix[16]);
static float s_locked_skybox[3 * 4];
static bool s_had_skybox = false;

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged,
    bFreeLookChanged, bFrameChanged;
static bool bTexMtxInfoChanged, bLightingConfigChanged;
static BitSet32 nMaterialsChanged;
static int nTransformMatricesChanged[2];      // min,max
static int nNormalMatricesChanged[2];         // min,max
static int nPostTransformMatricesChanged[2];  // min,max
static int nLightsChanged[2];                 // min,max

static Matrix44 s_viewportCorrection;
static Matrix33 s_viewRotationMatrix;
static Matrix33 s_viewInvRotationMatrix;
float s_fViewTranslationVector[3];
static float s_fViewRotation[2];

static Matrix44 s_VRTransformationMatrix;

VertexShaderConstants VertexShaderManager::constants;
std::vector<VertexShaderConstants> VertexShaderManager::constants_replay;
float4 VertexShaderManager::constants_eye_projection[2][4];
bool VertexShaderManager::m_layer_on_top;

bool VertexShaderManager::dirty;

// VR Virtual Reality debugging variables
int vr_render_eye = -1;
int debug_viewportNum = 0;
Viewport debug_vpList[64] = {0};
int debug_projNum = 0;
float debug_projList[64][7] = {0};
bool debug_newScene = true, debug_nextScene = false;
int vr_widest_3d_projNum = -1;
EFBRectangle g_final_screen_region = EFBRectangle(0, 0, 640, 528);
EFBRectangle g_requested_viewport = EFBRectangle(0, 0, 640, 528),
             g_rendered_viewport = EFBRectangle(0, 0, 640, 528);
enum ViewportType g_viewport_type = VIEW_FULLSCREEN, g_old_viewport_type = VIEW_FULLSCREEN;
enum SplitScreenType
{
  SS_FULLSCREEN = 0,
  SS_2_PLAYER_SIDE_BY_SIDE,
  SS_2_PLAYER_OVER_UNDER,
  SS_QUADRANTS,
  SS_3_PLAYER_TOP,
  SS_3_PLAYER_LEFT,
  SS_3_PLAYER_RIGHT,
  SS_3_PLAYER_BOTTOM,
  SS_3_PLAYER_COLUMNS,
  SS_CUSTOM
};
enum SplitScreenType g_splitscreen_type = SS_FULLSCREEN, g_old_splitscreen_type = SS_FULLSCREEN;
bool g_is_skybox = false;

static float oldpos[3] = {0, 0, 0}, totalpos[3] = {0, 0, 0};

// convert from the gamecube game's requested EFB region to the actual EFB region we used in Virtual
// Reality to render that part of the view
void ScaleRequestedToRendered(EFBRectangle* requested, EFBRectangle* rendered)
{
  float m = (float)g_rendered_viewport.GetWidth() / g_requested_viewport.GetWidth();
  rendered->left =
      (int)(0.5f + (requested->left - g_requested_viewport.left) * m + g_rendered_viewport.left);
  rendered->right =
      (int)(0.5f + (requested->right - g_requested_viewport.left) * m + g_rendered_viewport.left);
  m = (float)g_rendered_viewport.GetHeight() / g_requested_viewport.GetHeight();
  rendered->top =
      (int)(0.5f + (requested->top - g_requested_viewport.top) * m + g_rendered_viewport.top);
  rendered->bottom =
      (int)(0.5f + (requested->bottom - g_requested_viewport.top) * m + g_rendered_viewport.top);
}

const char* GetViewportTypeName(ViewportType v)
{
  if (g_is_skybox)
    return "Skybox";
  switch (v)
  {
  case VIEW_FULLSCREEN:
    return "Fullscreen";
  case VIEW_LETTERBOXED:
    return "Letterboxed";
  case VIEW_HUD_ELEMENT:
    return "HUD element";
  case VIEW_OFFSCREEN:
    return "Offscreen";
  case VIEW_RENDER_TO_TEXTURE:
    return "Render to Texture";
  case VIEW_PLAYER_1:
    return "Player 1";
  case VIEW_PLAYER_2:
    return "Player 2";
  case VIEW_PLAYER_3:
    return "Player 3";
  case VIEW_PLAYER_4:
    return "Player 4";
  case VIEW_SKYBOX:
    return "Skybox";
  default:
    return "Error";
  }
}

//#pragma optimize("", off)

void ClearDebugProj()
{  // VR
  bFrameChanged = true;

  debug_newScene = debug_nextScene;
  if (debug_newScene)
  {
    HACK_LOG(VR, "***** New scene *****");
    // General VR hacks
    vr_widest_3d_projNum = -1;
    vr_widest_3d_HFOV = 0;
    vr_widest_3d_VFOV = 0;
    vr_widest_3d_zNear = 0;
    vr_widest_3d_zFar = 0;
  }
  debug_nextScene = false;
  debug_projNum = 0;
  debug_viewportNum = 0;
  // Metroid Prime hacks
  NewMetroidFrame();
}

void SetViewportType(Viewport& v)
{
  // VR
  g_old_viewport_type = g_viewport_type;
  float left, top, width, height;
  left = v.xOrig - v.wd - 342;
  top = v.yOrig + v.ht - 342;
  width = 2 * v.wd;
  height = -2 * v.ht;
  float screen_width = (float)g_final_screen_region.GetWidth();
  float screen_height = (float)g_final_screen_region.GetHeight();
  float min_screen_width = 0.90f * screen_width;
  float min_screen_height = 0.90f * screen_height;
  float max_top = screen_height - min_screen_height;
  float max_left = screen_width - min_screen_width;

  // Power of two square viewport in the corner of the screen means we are rendering to a texture,
  // usually a shadow texture from the POV of the light, that will be drawn multitextured onto 3D
  // objects.
  // Note this is a temporary rendering to the backbuffer that will be cleared or overwritten after
  // reading.
  // Twilighlight Princess GC uses square but non-power-of-2 textures: 216x216 and 384x384
  // Metroid Prime 2 uses square textures in the bottom left corner but screen_height is wrong.
  // So the relaxed rule is: square texture on any screen edge with size a multiple of 8
  // Bad Boys 2 and 007 Everything or Nothing use 512x512 viewport and 512x512 screen size for
  // non-render-targets
  // if (width == height
  //	&& (width == 32 || width == 64 || width == 128 || width == 256)
  //	&& ((left == 0 && top == 0) || (left == 0 && top == screen_height - height)
  //	|| (left == screen_width - width && top == 0) || (left == screen_width - width && top ==
  // screen_height - height)))
  if (width == height && (width == 1 || width == 2 || width == 4 || ((int)width % 8 == 0)) &&
      (left == 0 || top == 0 || top == screen_height - height || left == screen_width - width) &&
      !(width == 512 && screen_width == 512 && screen_height == 512))
  {
    g_viewport_type = VIEW_RENDER_TO_TEXTURE;
  }
  // NES games render to the EFB copy and end up being projected twice, but this is now handled
  // elsewhere
  // else if (g_is_nes && width == 512 && height == 228 && left == 0 && top == 0)
  //{
  //	g_viewport_type = VIEW_RENDER_TO_TEXTURE;
  //}
  // Zelda Twilight Princess uses this strange viewport for rendering the Map Screen's coloured map
  // highlights to a texture.
  // I don't think it will break any other games, because it makes little sense as a real viewport.
  else if (width == 457 && height == 341 && left == 0 && top == 0)
  {
    g_viewport_type = VIEW_RENDER_TO_TEXTURE;
  }
  // Full width could mean fullscreen, letterboxed, or splitscreen top and bottom.
  else if (width >= min_screen_width)
  {
    if (left <= max_left)
    {
      if (height >= min_screen_height)
      {
        if (top <= max_top)
        {
          if (width == screen_width && height == screen_height)
            g_viewport_type = VIEW_FULLSCREEN;
          else
            g_viewport_type = VIEW_LETTERBOXED;
        }
        else
        {
          g_viewport_type = VIEW_OFFSCREEN;
        }
      }
      else if (height >= min_screen_height / 2 && height <= screen_height / 2)
      {
        if (top <= max_top)
        {
          // 2 Player Split Screen, top half
          g_viewport_type = VIEW_PLAYER_1;
          if (g_splitscreen_type != SS_3_PLAYER_TOP)
            g_splitscreen_type = SS_2_PLAYER_SIDE_BY_SIDE;
        }
        else if (top >= height && top <= height + max_top)
        {
          // 2 Player Split Screen, bottom half
          if (g_splitscreen_type != SS_3_PLAYER_BOTTOM)
          {
            g_splitscreen_type = SS_2_PLAYER_OVER_UNDER;
            g_viewport_type = VIEW_PLAYER_2;
          }
          else
          {
            // 3 Player Split Screen, bottom half
            g_viewport_type = VIEW_PLAYER_3;
          }
        }
        else
        {
          // band across middle of screen
          g_viewport_type = VIEW_LETTERBOXED;
        }
      }
      else
      {
        // band across middle of screen
        g_viewport_type = VIEW_LETTERBOXED;
        // setting this to HUD element breaks morphball mode in Metroid Prime
        //     HUD element (0,45) 640x358; near=0.999 (1.67604e+007), far=1 (1.67772e+007)
      }
    }
    else
    {
      g_viewport_type = VIEW_OFFSCREEN;
    }
  }
  else if (height >= min_screen_height)
  {
    if (top <= max_top)
    {
      if (width >= min_screen_width / 2)
      {
        if (left <= max_left)
        {
          // 2 Player Split Screen, left half
          g_viewport_type = VIEW_PLAYER_1;
          if (g_splitscreen_type != SS_3_PLAYER_LEFT)
            g_splitscreen_type = SS_2_PLAYER_SIDE_BY_SIDE;
        }
        else if (left >= width)
        {
          if (g_splitscreen_type != SS_3_PLAYER_RIGHT)
          {
            // 2 Player Split Screen, right half
            g_viewport_type = VIEW_PLAYER_2;
            g_splitscreen_type = SS_2_PLAYER_SIDE_BY_SIDE;
          }
          else
          {
            // 3 Player Split Screen, right half
            g_viewport_type = VIEW_PLAYER_3;
          }
        }
        else
        {
          // column down middle of screen
          g_viewport_type = VIEW_HUD_ELEMENT;
        }
      }
      else
      {
        // column down middle of screen
        g_viewport_type = VIEW_LETTERBOXED;
      }
    }
    else
    {
      g_viewport_type = VIEW_OFFSCREEN;
    }
  }
  // Quadrants
  else if (width >= (min_screen_width / 2) && height >= (min_screen_height / 2) &&
           width <= (screen_width / 2) && height <= (screen_height / 2))
  {
    // top left
    if (left <= max_left && top <= max_top)
    {
      g_viewport_type = VIEW_PLAYER_1;
      if (g_splitscreen_type != SS_3_PLAYER_RIGHT && g_splitscreen_type != SS_3_PLAYER_BOTTOM)
      {
        g_splitscreen_type = SS_QUADRANTS;
      }
    }
    // top right
    else if (left >= width && top <= max_top)
    {
      g_viewport_type = VIEW_PLAYER_2;
      if (g_splitscreen_type != SS_3_PLAYER_LEFT && g_splitscreen_type != SS_3_PLAYER_BOTTOM)
      {
        g_splitscreen_type = SS_QUADRANTS;
      }
    }
    // bottom left
    else if (left <= max_left && top >= height)
    {
      if (g_splitscreen_type == SS_3_PLAYER_RIGHT || g_splitscreen_type == SS_3_PLAYER_TOP)
      {
        g_viewport_type = VIEW_PLAYER_2;
      }
      else
      {
        g_viewport_type = VIEW_PLAYER_3;
        g_splitscreen_type = SS_QUADRANTS;
      }
    }
    // bottom right
    else if (left >= width && top >= height)
    {
      if (g_splitscreen_type == SS_3_PLAYER_LEFT || g_splitscreen_type == SS_3_PLAYER_TOP)
      {
        g_viewport_type = VIEW_PLAYER_3;
      }
      else
      {
        g_viewport_type = VIEW_PLAYER_4;
        g_splitscreen_type = SS_QUADRANTS;
      }
    }
    else
    {
      g_viewport_type = VIEW_HUD_ELEMENT;
    }
  }
  else if (left >= g_final_screen_region.right || top >= g_final_screen_region.bottom ||
           left + width <= g_final_screen_region.left || top + height <= g_final_screen_region.top)
  {
    g_viewport_type = VIEW_OFFSCREEN;
  }
  else
  {
    g_viewport_type = VIEW_HUD_ELEMENT;
  }
  if (g_viewport_type == VIEW_FULLSCREEN || g_viewport_type == VIEW_LETTERBOXED ||
      (g_viewport_type >= VIEW_PLAYER_1 && g_viewport_type <= VIEW_PLAYER_4))
  {
    // check if it is a skybox
    float znear = (v.farZ - v.zRange) / 16777216.0f;
    float zfar = v.farZ / 16777216.0f;

    if (znear >= 0.99f && zfar >= 0.999f)
      g_is_skybox = true;
    else
      g_is_skybox = false;
  }
  else
  {
    g_is_skybox = false;
  }
}

void DoLogViewport(int j, Viewport& v)
{
  // VR
  HACK_LOG(VR, "  Viewport %d: %s (%g,%g) %gx%g; near=%g (%g), far=%g (%g)", j,
           GetViewportTypeName(g_viewport_type), v.xOrig - v.wd - 342, v.yOrig + v.ht - 342,
           2 * v.wd, -2 * v.ht, (v.farZ - v.zRange) / 16777216.0f, v.farZ - v.zRange,
           v.farZ / 16777216.0f, v.farZ);
  HACK_LOG(VR, "      copyTexSrc (%d,%d) %dx%d", g_final_screen_region.left,
           g_final_screen_region.top, g_final_screen_region.GetWidth(),
           g_final_screen_region.GetHeight());
}

void DoLogProj(int j, float p[], const char* s)
{  // VR
  if (j == g_ActiveConfig.iSelectedLayer)
    HACK_LOG(VR, "** SELECTED LAYER:");
  if (p[6] != 0)
  {  // orthographic projection
    // float right = p[0]-(p[0]*p[1]);
    // float left = right - 2/p[0];

    float left = -(p[1] + 1) / p[0];
    float right = left + 2 / p[0];
    float bottom = -(p[3] + 1) / p[2];
    float top = bottom + 2 / p[2];
    float zfar = p[5] / p[4];
    float znear = (1 + p[4] * zfar) / p[4];
    HACK_LOG(VR, "%d: 2D: %s (%g, %g) to (%g, %g); z: %g to %g  [%g, %g]", j, s, left, top, right,
             bottom, znear, zfar, p[4], p[5]);
  }
  else if (p[0] != 0 || p[2] != 0)
  {  // perspective projection
    float f = p[5] / p[4];
    float n = f * p[4] / (p[4] - 1);
    if (p[0] == 1.0f && p[1] == 1.0f && p[2] == 1.0f && p[3] == 1.0f)
    {
      HACK_LOG(VR, "%d: %s N64 pre-transformed perspective: n=%.5f f=%.5f", j, s, n, f);
    }
    else if (p[1] != 0.0f || p[3] != 0.0f)
    {
      HACK_LOG(VR, "%d: %s OFF-AXIS Perspective: 2n/w=%.2f A=%.2f; 2n/h=%.2f B=%.2f; n=%.2f f=%.2f",
               j, s, p[0], p[1], p[2], p[3], p[4], p[5]);
      HACK_LOG(VR, "	HFOV: %.2f    VFOV: %.2f   Aspect Ratio: 16:%.1f",
               2 * atan(1.0f / p[0]) * 180.0f / 3.1415926535f,
               2 * atan(1.0f / p[2]) * 180.0f / 3.1415926535f, 16 / (2 / p[0]) * (2 / p[2]));
    }
    else
    {
      HACK_LOG(VR, "%d: %s HFOV: %.2fdeg; VFOV: %.2fdeg; Aspect Ratio: 16:%.1f; near:%f, far:%f", j,
               s, 2 * atan(1.0f / p[0]) * 180.0f / 3.1415926535f,
               2 * atan(1.0f / p[2]) * 180.0f / 3.1415926535f, 16 / (2 / p[0]) * (2 / p[2]), n, f);
    }
  }
  else
  {  // invalid
    HACK_LOG(VR, "%d: %s ZERO", j, s);
  }
}

void LogProj(float p[])
{
  // VR
  if (p[6] == 0)
  {  // perspective projection
    bool bN64 = p[0] == 1.0f && p[1] == 1.0f && p[2] == 1.0f && p[3] == 1.0f;
    // don't change this formula!
    // metroid layer detection depends on exact values
    float vfov = (2 * atan(1.0f / p[2]) * 180.0f / 3.1415926535f);
    float hfov = (2 * atan(1.0f / p[0]) * 180.0f / 3.1415926535f);
    float f = p[5] / p[4];
    float n = f * p[4] / (p[4] - 1);
    if (bN64)
    {
      hfov = g_ActiveConfig.fN64FOV;
      vfov = 2 * 180.0f / 3.1415926535f *
             atanf(tanf((hfov * 3.1415926535f / 180.0f) / 2) * 3.0f / 4.0f);  // 4:3 aspect ratio
    }
    switch (g_ActiveConfig.iMetroidPrime)
    {
    case 1:
      g_metroid_layer = GetMetroidPrime1GCLayer(debug_projNum, hfov, vfov, n, f);
      break;
    case 31:
      g_metroid_layer = GetMetroidPrime1WiiLayer(debug_projNum, hfov, vfov, n, f);
      break;
    case 2:
    case 32:
      g_metroid_layer = GetMetroidPrime2GCLayer(debug_projNum, hfov, vfov, n, f);
      break;
    case 3:
      g_metroid_layer = GetMetroidPrime3Layer(debug_projNum, hfov, vfov, n, f);
      break;
    case 113:
      g_metroid_layer = GetZeldaTPGCLayer(debug_projNum, hfov, vfov, n, f);
      break;
    case 0:
    default:
      g_metroid_layer = METROID_UNKNOWN;
      break;
    }

    // Square projections are usually render to texture, eg. shadows, and shouldn't affect widest 3D
    // FOV
    // but some N64 games use a strange square projection where p[0]=p[1]=p[2]=p[3]=1
    if (debug_newScene && fabs(hfov) > vr_widest_3d_HFOV && fabs(hfov) <= 125 &&
        (fabs(p[2]) != fabs(p[0]) || (p[0] == 1 && p[1] == 1)))
    {
      DEBUG_LOG(VR, "***** New Widest 3D *****");

      vr_widest_3d_projNum = debug_projNum;
      vr_widest_3d_HFOV = fabs(hfov);
      vr_widest_3d_VFOV = fabs(vfov);
      vr_widest_3d_zNear = fabs(n);
      vr_widest_3d_zFar = fabs(f);
      DEBUG_LOG(VR, "%d: %g x %g deg, n=%g f=%g, p4=%g p5=%g; xs=%g ys=%g", vr_widest_3d_projNum,
                vr_widest_3d_HFOV, vr_widest_3d_VFOV, vr_widest_3d_zNear, vr_widest_3d_zFar, p[4],
                p[5], p[0], p[2]);
    }
  }
  else
  {
    float left = -(p[1] + 1) / p[0];
    float right = left + 2 / p[0];
    float bottom = -(p[3] + 1) / p[2];
    float top = bottom + 2 / p[2];
    float zfar = p[5] / p[4];
    float znear = (1 + p[4] * zfar) / p[4];
    switch (g_ActiveConfig.iMetroidPrime)
    {
    case 1:
      g_metroid_layer =
          GetMetroidPrime1GCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
      break;
    case 31:
      g_metroid_layer =
          GetMetroidPrime1WiiLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
      break;
    case 2:
    case 32:
      g_metroid_layer =
          GetMetroidPrime2GCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
      break;
    case 3:
      g_metroid_layer =
          GetMetroidPrime3Layer2D(debug_projNum, left, right, top, bottom, znear, zfar);
      break;
    case 113:
      g_metroid_layer = GetZeldaTPGCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
      break;
    case 0:
    default:
      if (g_is_nes)
        g_metroid_layer = GetNESLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
      else
        g_metroid_layer = METROID_UNKNOWN_2D;
      break;
    }
  }

  if (debug_projNum >= 64)
    return;
  if (!debug_newScene)
  {
    for (int i = 0; i < 7; i++)
    {
      if (debug_projList[debug_projNum][i] != p[i])
      {
        debug_nextScene = true;
        debug_projList[debug_projNum][i] = p[i];
      }
    }
    // wait until next frame
    // if (debug_newScene) {
    //	INFO_LOG(VIDEO,"***** New scene *****");
    //	for (int j=0; j<debug_projNum; j++) {
    //		DoLogProj(j, debug_projList[j]);
    //	}
    //}
  }
  else
  {
    debug_nextScene = false;
    INFO_LOG(VR, "%f Units Per Metre", g_ActiveConfig.fUnitsPerMetre);
    INFO_LOG(VR, "HUD is %.1fm away and %.1fm thick", g_ActiveConfig.fHudDistance,
             g_ActiveConfig.fHudThickness);
    DoLogProj(debug_projNum, debug_projList[debug_projNum], MetroidLayerName(g_metroid_layer));
  }
  debug_projNum++;
}

void LogViewport(Viewport& v)
{  // VR
  if (debug_viewportNum >= 64)
    return;
  if (!debug_newScene)
  {
    if (debug_vpList[debug_viewportNum].farZ != v.farZ ||
        debug_vpList[debug_viewportNum].ht != v.ht || debug_vpList[debug_viewportNum].wd != v.wd ||
        debug_vpList[debug_viewportNum].xOrig != v.xOrig ||
        debug_vpList[debug_viewportNum].yOrig != v.yOrig ||
        debug_vpList[debug_viewportNum].zRange != v.zRange)
    {
      debug_nextScene = true;
      debug_vpList[debug_viewportNum] = v;
    }
  }
  else
  {
    debug_nextScene = false;
    DoLogViewport(debug_viewportNum, debug_vpList[debug_viewportNum]);
  }
  debug_viewportNum++;
}
//#pragma optimize("", on)

struct ProjectionHack
{
  float sign;
  float value;
  ProjectionHack() {}
  ProjectionHack(float new_sign, float new_value) : sign(new_sign), value(new_value) {}
};

namespace
{
// Control Variables
static ProjectionHack g_proj_hack_near;
static ProjectionHack g_proj_hack_far;
}  // Namespace

static float PHackValue(std::string sValue)
{
  float f = 0;
  bool fp = false;
  const char* cStr = sValue.c_str();
  char* c = new char[strlen(cStr) + 1];
  std::istringstream sTof("");

  for (unsigned int i = 0; i <= strlen(cStr); ++i)
  {
    if (i == 20)
    {
      c[i] = '\0';
      break;
    }

    c[i] = (cStr[i] == ',') ? '.' : *(cStr + i);
    if (c[i] == '.')
      fp = true;
  }

  cStr = c;
  sTof.str(cStr);
  sTof >> f;

  if (!fp)
    f /= 0xF4240;

  delete[] c;
  return f;
}

void UpdateProjectionHack(const ProjectionHackConfig& config)
{
  float near_value = 0, far_value = 0;
  float near_sign = 1.0, far_sign = 1.0;

  if (config.m_enable)
  {
    const char* near_sign_str = "";
    const char* far_sign_str = "";

    NOTICE_LOG(VIDEO, "\t\t--- Orthographic Projection Hack ON ---");

    if (config.m_sznear)
    {
      near_sign *= -1.0f;
      near_sign_str = " * (-1)";
    }
    if (config.m_szfar)
    {
      far_sign *= -1.0f;
      far_sign_str = " * (-1)";
    }

    near_value = PHackValue(config.m_znear);
    NOTICE_LOG(VIDEO, "- zNear Correction = (%f + zNear)%s", near_value, near_sign_str);

    far_value = PHackValue(config.m_zfar);
    NOTICE_LOG(VIDEO, "- zFar Correction =  (%f + zFar)%s", far_value, far_sign_str);
  }

  // Set the projections hacks
  g_proj_hack_near = ProjectionHack(near_sign, near_value);
  g_proj_hack_far = ProjectionHack(far_sign, far_value);
}

// Viewport correction:
// In D3D, the viewport rectangle must fit within the render target.
// Say you want a viewport at (ix, iy) with size (iw, ih),
// but your viewport must be clamped at (ax, ay) with size (aw, ah).
// Just multiply the projection matrix with the following to get the same
// effect:
// [   (iw/aw)         0     0    ((iw - 2*(ax-ix)) / aw - 1)   ]
// [         0   (ih/ah)     0   ((-ih + 2*(ay-iy)) / ah + 1)   ]
// [         0         0     1                              0   ]
// [         0         0     0                              1   ]
static void ViewportCorrectionMatrix(Matrix44& result)
{
  int scissorXOff = bpmem.scissorOffset.x * 2;
  int scissorYOff = bpmem.scissorOffset.y * 2;

  // TODO: ceil, floor or just cast to int?
  // TODO: Directly use the floats instead of rounding them?
  float intendedX = xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff;
  float intendedY = xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff;
  float intendedWd = 2.0f * xfmem.viewport.wd;
  float intendedHt = -2.0f * xfmem.viewport.ht;

  if (intendedWd < 0.f)
  {
    intendedX += intendedWd;
    intendedWd = -intendedWd;
  }
  if (intendedHt < 0.f)
  {
    intendedY += intendedHt;
    intendedHt = -intendedHt;
  }

  // fit to EFB size
  float X = (intendedX >= 0.f) ? intendedX : 0.f;
  float Y = (intendedY >= 0.f) ? intendedY : 0.f;
  float Wd = (X + intendedWd <= EFB_WIDTH) ? intendedWd : (EFB_WIDTH - X);
  float Ht = (Y + intendedHt <= EFB_HEIGHT) ? intendedHt : (EFB_HEIGHT - Y);

  Matrix44::LoadIdentity(result);
  if (Wd == 0 || Ht == 0)
    return;

  result.data[4 * 0 + 0] = intendedWd / Wd;
  result.data[4 * 0 + 3] = (intendedWd - 2.f * (X - intendedX)) / Wd - 1.f;
  result.data[4 * 1 + 1] = intendedHt / Ht;
  result.data[4 * 1 + 3] = (-intendedHt + 2.f * (Y - intendedY)) / Ht + 1.f;
}

void VertexShaderManager::Init()
{
  // Initialize state tracking variables
  nTransformMatricesChanged[0] = -1;
  nTransformMatricesChanged[1] = -1;
  nNormalMatricesChanged[0] = -1;
  nNormalMatricesChanged[1] = -1;
  nPostTransformMatricesChanged[0] = -1;
  nPostTransformMatricesChanged[1] = -1;
  nLightsChanged[0] = -1;
  nLightsChanged[1] = -1;
  nMaterialsChanged = BitSet32(0);
  bTexMatricesChanged[0] = false;
  bTexMatricesChanged[1] = false;
  bPosNormalMatrixChanged = false;
  bProjectionChanged = true;
  bViewportChanged = false;
  bTexMtxInfoChanged = false;
  bLightingConfigChanged = false;

  m_layer_on_top = false;

  std::memset(&xfmem, 0, sizeof(xfmem));
  constants = {};
  ResetView();

  // TODO: should these go inside ResetView()?
  Matrix44::LoadIdentity(s_viewportCorrection);
  Matrix44::LoadIdentity(s_VRTransformationMatrix);
  memset(g_fProjectionMatrix, 0, sizeof(g_fProjectionMatrix));
  for (int i = 0; i < 4; ++i)
    g_fProjectionMatrix[i * 5] = 1.0f;
  g_viewport_type = VIEW_FULLSCREEN;
  g_old_viewport_type = VIEW_FULLSCREEN;
  g_splitscreen_type = SS_FULLSCREEN;
  g_old_splitscreen_type = SS_FULLSCREEN;
  Matrix44::LoadIdentity(g_game_camera_rotmat);

  for (int i = 0; i < 3; ++i)
  {
    g_game_camera_pos[i] = totalpos[i] = oldpos[i] = 0;
  }
  s_had_skybox = false;

  dirty = true;
}

void VertexShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  bProjectionChanged = true;
  bFrameChanged = true;

  dirty = true;
}

// Syncs the shader constant buffers with xfmem
// TODO: A cleaner way to control the matrices without making a mess in the parameters field
void VertexShaderManager::SetConstants()
{
  static bool temp_skybox = false;
  bool position_changed = false, skybox_changed = false;
  if (nTransformMatricesChanged[0] >= 0)
  {
    int startn = nTransformMatricesChanged[0] / 4;
    int endn = (nTransformMatricesChanged[1] + 3) / 4;
    memcpy(constants.transformmatrices[startn].data(), &xfmem.posMatrices[startn * 4],
           (endn - startn) * sizeof(float4));
    dirty = true;
    nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
    position_changed = true;
  }

  if (nNormalMatricesChanged[0] >= 0)
  {
    int startn = nNormalMatricesChanged[0] / 3;
    int endn = (nNormalMatricesChanged[1] + 2) / 3;
    for (int i = startn; i < endn; i++)
    {
      memcpy(constants.normalmatrices[i].data(), &xfmem.normalMatrices[3 * i], 12);
    }
    dirty = true;
    nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
  }

  if (nPostTransformMatricesChanged[0] >= 0)
  {
    int startn = nPostTransformMatricesChanged[0] / 4;
    int endn = (nPostTransformMatricesChanged[1] + 3) / 4;
    memcpy(constants.posttransformmatrices[startn].data(), &xfmem.postMatrices[startn * 4],
           (endn - startn) * sizeof(float4));
    dirty = true;
    nPostTransformMatricesChanged[0] = nPostTransformMatricesChanged[1] = -1;
  }

  if (nLightsChanged[0] >= 0)
  {
    // TODO: Outdated comment
    // lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
    int istart = nLightsChanged[0] / 0x10;
    int iend = (nLightsChanged[1] + 15) / 0x10;

    for (int i = istart; i < iend; ++i)
    {
      const Light& light = xfmem.lights[i];
      VertexShaderConstants::Light& dstlight = constants.lights[i];

      // xfmem.light.color is packed as abgr in u8[4], so we have to swap the order
      dstlight.color[0] = light.color[3];
      dstlight.color[1] = light.color[2];
      dstlight.color[2] = light.color[1];
      dstlight.color[3] = light.color[0];

      dstlight.cosatt[0] = light.cosatt[0];
      dstlight.cosatt[1] = light.cosatt[1];
      dstlight.cosatt[2] = light.cosatt[2];

      if (fabs(light.distatt[0]) < 0.00001f && fabs(light.distatt[1]) < 0.00001f &&
          fabs(light.distatt[2]) < 0.00001f)
      {
        // dist attenuation, make sure not equal to 0!!!
        dstlight.distatt[0] = .00001f;
      }
      else
      {
        dstlight.distatt[0] = light.distatt[0];
      }
      dstlight.distatt[1] = light.distatt[1];
      dstlight.distatt[2] = light.distatt[2];

      dstlight.pos[0] = light.dpos[0];
      dstlight.pos[1] = light.dpos[1];
      dstlight.pos[2] = light.dpos[2];

      double norm = double(light.ddir[0]) * double(light.ddir[0]) +
                    double(light.ddir[1]) * double(light.ddir[1]) +
                    double(light.ddir[2]) * double(light.ddir[2]);
      norm = 1.0 / sqrt(norm);
      float norm_float = static_cast<float>(norm);
      dstlight.dir[0] = light.ddir[0] * norm_float;
      dstlight.dir[1] = light.ddir[1] * norm_float;
      dstlight.dir[2] = light.ddir[2] * norm_float;
    }
    dirty = true;

    nLightsChanged[0] = nLightsChanged[1] = -1;
  }

  for (int i : nMaterialsChanged)
  {
    u32 data = i >= 2 ? xfmem.matColor[i - 2] : xfmem.ambColor[i];
    constants.materials[i][0] = (data >> 24) & 0xFF;
    constants.materials[i][1] = (data >> 16) & 0xFF;
    constants.materials[i][2] = (data >> 8) & 0xFF;
    constants.materials[i][3] = data & 0xFF;
    dirty = true;
  }
  nMaterialsChanged = BitSet32(0);

  if (bPosNormalMatrixChanged)
  {
    bPosNormalMatrixChanged = false;

    const float* pos = &xfmem.posMatrices[g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4];
    const float* norm =
        &xfmem.normalMatrices[3 * (g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31)];

    memcpy(constants.posnormalmatrix.data(), pos, 3 * sizeof(float4));
    memcpy(constants.posnormalmatrix[3].data(), norm, 3 * sizeof(float));
    memcpy(constants.posnormalmatrix[4].data(), norm + 3, 3 * sizeof(float));
    memcpy(constants.posnormalmatrix[5].data(), norm + 6, 3 * sizeof(float));
    dirty = true;
    position_changed = true;
  }

  if (bTexMatricesChanged[0])
  {
    bTexMatricesChanged[0] = false;
    const float* pos_matrix_ptrs[] = {
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4]};

    for (size_t i = 0; i < ArraySize(pos_matrix_ptrs); ++i)
    {
      memcpy(constants.texmatrices[3 * i].data(), pos_matrix_ptrs[i], 3 * sizeof(float4));
    }
    dirty = true;
  }

  if (bTexMatricesChanged[1])
  {
    bTexMatricesChanged[1] = false;
    const float* pos_matrix_ptrs[] = {
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4]};

    for (size_t i = 0; i < ArraySize(pos_matrix_ptrs); ++i)
    {
      memcpy(constants.texmatrices[3 * i + 12].data(), pos_matrix_ptrs[i], 3 * sizeof(float4));
    }
    dirty = true;
  }

  if (position_changed && temp_skybox)
  {
    g_is_skybox = false;
    temp_skybox = false;
    skybox_changed = true;
  }

  if (bViewportChanged)
  {
    bViewportChanged = false;
    // VR, Check whether it is a skybox, fullscreen, letterboxed, splitscreen multiplayer, hud
    // element, or offscreen
    SetViewportType(xfmem.viewport);
    LogViewport(xfmem.viewport);

    SetViewportConstants();

    // Update projection if the viewport isn't 1:1 useable
    if (!g_ActiveConfig.backend_info.bSupportsOversizedViewports)
    {
      ViewportCorrectionMatrix(s_viewportCorrection);
      skybox_changed = true;
    }
    // VR adjust the projection matrix for the new kind of viewport
    else if (g_viewport_type != g_old_viewport_type)
    {
      skybox_changed = true;
    }
  }

  if (position_changed && g_ActiveConfig.bDetectSkybox && !g_is_skybox)
  {
    CheckSkybox();
    if (g_is_skybox)
    {
      temp_skybox = true;
      skybox_changed = true;
    }
  }

  if (bProjectionChanged || bFrameChanged)
  {
    if (bProjectionChanged)
      LogProj(xfmem.projection.rawProjection);
    bProjectionChanged = false;
    bFrameChanged = false;
    SetProjectionConstants();
  }
  else if (skybox_changed)
  {
    SetProjectionConstants();
  }

  if (g_ActiveConfig.iMotionSicknessSkybox == 2 && g_is_skybox)
    LockSkybox();
}

//#pragma optimize("", off)

void VertexShaderManager::SetViewportConstants()
{
  // The console GPU places the pixel center at 7/12 unless antialiasing
  // is enabled, while D3D and OpenGL place it at 0.5. See the comment
  // in VertexShaderGen.cpp for details.
  // NOTE: If we ever emulate antialiasing, the sample locations set by
  // BP registers 0x01-0x04 need to be considered here.
  const float pixel_center_correction = 7.0f / 12.0f - 0.5f;
  const bool bUseVertexRounding =
      g_ActiveConfig.bVertexRounding && g_ActiveConfig.iEFBScale != SCALE_1X;
  const float viewport_width = bUseVertexRounding ?
                                   (2.f * xfmem.viewport.wd) :
                                   g_renderer->EFBToScaledXf(2.f * xfmem.viewport.wd);
  const float viewport_height = bUseVertexRounding ?
                                    (2.f * xfmem.viewport.ht) :
                                    g_renderer->EFBToScaledXf(2.f * xfmem.viewport.ht);
  const float pixel_size_x = 2.f / viewport_width;
  const float pixel_size_y = 2.f / viewport_height;
  constants.pixelcentercorrection[0] = pixel_center_correction * pixel_size_x;
  constants.pixelcentercorrection[1] = pixel_center_correction * pixel_size_y;

  // By default we don't change the depth value at all in the vertex shader.
  constants.pixelcentercorrection[2] = 1.0f;
  constants.pixelcentercorrection[3] = 0.0f;
  constants.viewport[0] = (2.f * xfmem.viewport.wd);
  constants.viewport[1] = (2.f * xfmem.viewport.ht);

  if (g_renderer->UseVertexDepthRange())
  {
      // Oversized depth ranges are handled in the vertex shader. We need to reverse
      // the far value to use the reversed-Z trick.
      if (g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      {
        // Sometimes the console also tries to use the reversed-Z trick. We can only do
        // that with the expected accuracy if the backend can reverse the depth range.
        constants.pixelcentercorrection[2] = fabs(xfmem.viewport.zRange) / 16777215.0f;
        if (xfmem.viewport.zRange < 0.0f)
          constants.pixelcentercorrection[3] = xfmem.viewport.farZ / 16777215.0f;
        else
          constants.pixelcentercorrection[3] = 1.0f - xfmem.viewport.farZ / 16777215.0f;
      }
      else
      {
        // For backends that don't support reversing the depth range we can still render
        // cases where the console uses the reversed-Z trick. But we simply can't provide
        // the expected accuracy, which might result in z-fighting.
        constants.pixelcentercorrection[2] = xfmem.viewport.zRange / 16777215.0f;
        constants.pixelcentercorrection[3] = 1.0f - xfmem.viewport.farZ / 16777215.0f;
      }
  }

  dirty = true;
  // This is so implementation-dependent that we can't have it here.
  g_renderer->SetViewport();
}

void VertexShaderManager::SetProjectionConstants()
{
  // Transformations must be applied in the following order for VR:
  // HUD
  // camera position stabilisation
  // camera forward
  // camera pitch or rotation stabilisation
  // free look
  // leaning back
  // head position tracking
  // head rotation tracking
  // eye pos
  // projection

  ///////////////////////////////////////////////////////
  // First, identify any special layers and hacks

  m_layer_on_top = false;
  bool bFullscreenLayer = g_ActiveConfig.bHudFullscreen && xfmem.projection.type != GX_PERSPECTIVE;
  bool bFlashing = (debug_projNum - 1) == g_ActiveConfig.iSelectedLayer;
  bool bStuckToHead = false, bHide = false;
  int flipped_x = 1, flipped_y = 1, iTelescopeHack = -1;
  float fScaleHack = 1, fWidthHack = 1, fHeightHack = 1, fUpHack = 0, fRightHack = 0;

  if (g_ActiveConfig.iMetroidPrime || g_is_nes)
  {
    GetMetroidPrimeValues(&bStuckToHead, &bFullscreenLayer, &bHide, &bFlashing, &fScaleHack,
                          &fWidthHack, &fHeightHack, &fUpHack, &fRightHack, &iTelescopeHack);
  }

  // VR: in split-screen, only draw VR player TODO: fix offscreen to render to a separate texture in
  // VR
  bHide = bHide ||
          (g_has_hmd && (g_viewport_type == VIEW_OFFSCREEN ||
                         (g_viewport_type >= VIEW_PLAYER_1 && g_viewport_type <= VIEW_PLAYER_4 &&
                          g_ActiveConfig.iVRPlayer != g_viewport_type - VIEW_PLAYER_1)));
  // flash selected layer for debugging
  bHide = bHide || (bFlashing && g_ActiveConfig.iFlashState > 5);
  // hide skybox or everything to reduce motion sickness
  bHide = bHide || (g_is_skybox && g_ActiveConfig.iMotionSicknessSkybox == 1) || g_vr_black_screen;

  // Split WidthHack and HeightHack into left and right versions for telescopes
  float fLeftWidthHack = fWidthHack, fRightWidthHack = fWidthHack;
  float fLeftHeightHack = fHeightHack, fRightHeightHack = fHeightHack;
  bool bHideLeft = bHide, bHideRight = bHide, bNoForward = false;
  // bool bTelescopeHUD = false;
  if (iTelescopeHack < 0 && g_ActiveConfig.iTelescopeEye &&
      vr_widest_3d_VFOV <= g_ActiveConfig.fTelescopeMaxFOV && vr_widest_3d_VFOV > 1 &&
      (g_ActiveConfig.fTelescopeMaxFOV <= g_ActiveConfig.fMinFOV ||
       (g_ActiveConfig.fTelescopeMaxFOV > g_ActiveConfig.fMinFOV &&
        vr_widest_3d_VFOV > g_ActiveConfig.fMinFOV)))
    iTelescopeHack = g_ActiveConfig.iTelescopeEye;
  if (g_has_hmd && iTelescopeHack > 0)
  {
    bNoForward = true;
    // Calculate telescope scale
    float hmd_halftan, telescope_scale;
    VR_GetProjectionHalfTan(hmd_halftan);
    telescope_scale = fabs(hmd_halftan / tan(DEGREES_TO_RADIANS(vr_widest_3d_VFOV) / 2));
    if (iTelescopeHack & 1)
    {
      fLeftWidthHack *= telescope_scale;
      fLeftHeightHack *= telescope_scale;
      bHideLeft = false;
    }
    if (iTelescopeHack & 2)
    {
      fRightWidthHack *= telescope_scale;
      fRightHeightHack *= telescope_scale;
      bHideRight = false;
    }
  }

  ///////////////////////////////////////////////////////
  // Second, set the original projection matrix and save its stats

  float* rawProjection = xfmem.projection.rawProjection;

  switch (xfmem.projection.type)
  {
  case GX_PERSPECTIVE:

    g_fProjectionMatrix[0] = rawProjection[0] * g_ActiveConfig.fAspectRatioHackW;
    g_fProjectionMatrix[1] = 0.0f;
    g_fProjectionMatrix[2] = rawProjection[1] * g_ActiveConfig.fAspectRatioHackW;
    g_fProjectionMatrix[3] = 0.0f;

    g_fProjectionMatrix[4] = 0.0f;
    g_fProjectionMatrix[5] = rawProjection[2] * g_ActiveConfig.fAspectRatioHackH;
    g_fProjectionMatrix[6] = rawProjection[3] * g_ActiveConfig.fAspectRatioHackH;
    g_fProjectionMatrix[7] = 0.0f;

    g_fProjectionMatrix[8] = 0.0f;
    g_fProjectionMatrix[9] = 0.0f;
    g_fProjectionMatrix[10] = rawProjection[4];
    g_fProjectionMatrix[11] = rawProjection[5];

    g_fProjectionMatrix[12] = 0.0f;
    g_fProjectionMatrix[13] = 0.0f;
    // donkopunchstania suggested the GC GPU might round differently
    // He had thus changed this to -(1 + epsilon) to fix clipping issues.
    // I (neobrain) don't think his conjecture is true and thus reverted his change.
    g_fProjectionMatrix[14] = -1.0f;
    g_fProjectionMatrix[15] = 0.0f;

    SETSTAT_FT(stats.gproj_0, g_fProjectionMatrix[0]);
    SETSTAT_FT(stats.gproj_1, g_fProjectionMatrix[1]);
    SETSTAT_FT(stats.gproj_2, g_fProjectionMatrix[2]);
    SETSTAT_FT(stats.gproj_3, g_fProjectionMatrix[3]);
    SETSTAT_FT(stats.gproj_4, g_fProjectionMatrix[4]);
    SETSTAT_FT(stats.gproj_5, g_fProjectionMatrix[5]);
    SETSTAT_FT(stats.gproj_6, g_fProjectionMatrix[6]);
    SETSTAT_FT(stats.gproj_7, g_fProjectionMatrix[7]);
    SETSTAT_FT(stats.gproj_8, g_fProjectionMatrix[8]);
    SETSTAT_FT(stats.gproj_9, g_fProjectionMatrix[9]);
    SETSTAT_FT(stats.gproj_10, g_fProjectionMatrix[10]);
    SETSTAT_FT(stats.gproj_11, g_fProjectionMatrix[11]);
    SETSTAT_FT(stats.gproj_12, g_fProjectionMatrix[12]);
    SETSTAT_FT(stats.gproj_13, g_fProjectionMatrix[13]);
    SETSTAT_FT(stats.gproj_14, g_fProjectionMatrix[14]);
    SETSTAT_FT(stats.gproj_15, g_fProjectionMatrix[15]);
    break;

  case GX_ORTHOGRAPHIC:
    g_fProjectionMatrix[0] = rawProjection[0];
    g_fProjectionMatrix[1] = 0.0f;
    g_fProjectionMatrix[2] = 0.0f;
    g_fProjectionMatrix[3] = rawProjection[1];

    g_fProjectionMatrix[4] = 0.0f;
    g_fProjectionMatrix[5] = rawProjection[2];
    g_fProjectionMatrix[6] = 0.0f;
    g_fProjectionMatrix[7] = rawProjection[3];

    g_fProjectionMatrix[8] = 0.0f;
    g_fProjectionMatrix[9] = 0.0f;
    g_fProjectionMatrix[10] = (g_proj_hack_near.value + rawProjection[4]) *
                              ((g_proj_hack_near.sign == 0) ? 1.0f : g_proj_hack_near.sign);
    g_fProjectionMatrix[11] = (g_proj_hack_far.value + rawProjection[5]) *
                              ((g_proj_hack_far.sign == 0) ? 1.0f : g_proj_hack_far.sign);

    g_fProjectionMatrix[12] = 0.0f;
    g_fProjectionMatrix[13] = 0.0f;

    g_fProjectionMatrix[14] = 0.0f;
    g_fProjectionMatrix[15] = 1.0f;

    SETSTAT_FT(stats.g2proj_0, g_fProjectionMatrix[0]);
    SETSTAT_FT(stats.g2proj_1, g_fProjectionMatrix[1]);
    SETSTAT_FT(stats.g2proj_2, g_fProjectionMatrix[2]);
    SETSTAT_FT(stats.g2proj_3, g_fProjectionMatrix[3]);
    SETSTAT_FT(stats.g2proj_4, g_fProjectionMatrix[4]);
    SETSTAT_FT(stats.g2proj_5, g_fProjectionMatrix[5]);
    SETSTAT_FT(stats.g2proj_6, g_fProjectionMatrix[6]);
    SETSTAT_FT(stats.g2proj_7, g_fProjectionMatrix[7]);
    SETSTAT_FT(stats.g2proj_8, g_fProjectionMatrix[8]);
    SETSTAT_FT(stats.g2proj_9, g_fProjectionMatrix[9]);
    SETSTAT_FT(stats.g2proj_10, g_fProjectionMatrix[10]);
    SETSTAT_FT(stats.g2proj_11, g_fProjectionMatrix[11]);
    SETSTAT_FT(stats.g2proj_12, g_fProjectionMatrix[12]);
    SETSTAT_FT(stats.g2proj_13, g_fProjectionMatrix[13]);
    SETSTAT_FT(stats.g2proj_14, g_fProjectionMatrix[14]);
    SETSTAT_FT(stats.g2proj_15, g_fProjectionMatrix[15]);

    SETSTAT_FT(stats.proj_0, rawProjection[0]);
    SETSTAT_FT(stats.proj_1, rawProjection[1]);
    SETSTAT_FT(stats.proj_2, rawProjection[2]);
    SETSTAT_FT(stats.proj_3, rawProjection[3]);
    SETSTAT_FT(stats.proj_4, rawProjection[4]);
    SETSTAT_FT(stats.proj_5, rawProjection[5]);
    break;

  default:
    ERROR_LOG(VIDEO, "Unknown projection type: %d", xfmem.projection.type);
  }

  PRIM_LOG("Projection: %f %f %f %f %f %f", rawProjection[0], rawProjection[1], rawProjection[2],
           rawProjection[3], rawProjection[4], rawProjection[5]);
  dirty = true;
  GeometryShaderManager::dirty = true;

  bool bN64 = (xfmem.projection.type == GX_PERSPECTIVE && rawProjection[0] == 1.0f &&
               rawProjection[1] == 1.0f && rawProjection[2] == 1.0f && rawProjection[3] == 1.0f);

  ///////////////////////////////////////////////////////
  // What happens last depends on what kind of rendering we are doing for this layer
  // Hide: don't render anything
  // Render to texture: render in 2D exactly the same as the real console would, for projection
  // shadows etc.
  // Free Look
  // Normal emulation
  // VR Fullscreen layer: render EFB copies or screenspace effects so they fill the full screen they
  // were copied from
  // VR: Render everything as part of a virtual world, there are a few separate alternatives here:
  //     2D HUD as thick pane of glass floating in 3D space
  //     3D HUD element as a 3D object attached to that pane of glass
  //     3D world
  float UnitsPerMetre = g_ActiveConfig.fUnitsPerMetre * fScaleHack / g_ActiveConfig.fScale;

  bHide = bHide && (bFlashing || (g_has_hmd && g_ActiveConfig.bEnableVR));

  if (bHide)
  {
    // If we are supposed to hide the layer, zero out the projection matrix
    memset(constants.projection.data(), 0, 4 * 16);
    memset(constants_eye_projection[0], 0, 2 * 4 * 16);
    memset(GeometryShaderManager::constants.stereoparams.data(), 0, 4 * 4);
    return;
  }
  // don't do anything fancy for rendering to a texture
  // render exactly as we are told, and in mono
  else if (g_viewport_type == VIEW_RENDER_TO_TEXTURE)
  {
    // we aren't applying viewport correction, because Render To Texture never has a viewport larger
    // than the framebufffer
    Matrix44 correctedMtx;
    Matrix44::Set(correctedMtx, g_fProjectionMatrix);

    memcpy(constants.projection.data(), correctedMtx.data, 4 * 16);
    memcpy(constants_eye_projection[0], correctedMtx.data, 4 * 16);
    memcpy(constants_eye_projection[1], correctedMtx.data, 4 * 16);
    GeometryShaderManager::constants.stereoparams[0] =
        GeometryShaderManager::constants.stereoparams[1] = 0;
    GeometryShaderManager::constants.stereoparams[2] =
        GeometryShaderManager::constants.stereoparams[3] = 0;
    return;
  }
  else if (!g_has_hmd || !g_ActiveConfig.bEnableVR)
  {
    Matrix44 mtxA;
    Matrix44 mtxB;
    Matrix44 viewMtx;

    if ((bFreeLookChanged || ARBruteForcer::ch_bruteforce) &&
        xfmem.projection.type == GX_PERSPECTIVE)
    {
      // use the freelook camera position, which should still be in metres even for non-VR so it is
      // a consistent speed between games
      float pos[3];
      for (int i = 0; i < 3; ++i)
        pos[i] = s_fViewTranslationVector[i] * UnitsPerMetre;
      Matrix44::Translate(mtxA, pos);
      Matrix44::LoadMatrix33(mtxB, s_viewRotationMatrix);
      Matrix44::Multiply(mtxB, mtxA, viewMtx);  // view = rotation x translation
    }
    else
    {
      Matrix44::LoadIdentity(viewMtx);
    }

    Matrix44::Set(mtxB, g_fProjectionMatrix);
    Matrix44::Multiply(mtxB, viewMtx, mtxA);               // mtxA = projection x view
    Matrix44::Multiply(s_viewportCorrection, mtxA, mtxB);  // mtxB = viewportCorrection x mtxA

    memcpy(constants.projection.data(), mtxB.data, 4 * 16);
    memcpy(constants_eye_projection[0], mtxB.data, 4 * 16);
    memcpy(constants_eye_projection[1], mtxB.data, 4 * 16);

    if (g_ActiveConfig.iStereoMode > 0)
    {
      if (xfmem.projection.type == GX_PERSPECTIVE)
      {
        float offset = (g_ActiveConfig.iStereoDepth / 1000.0f) *
                       (g_ActiveConfig.iStereoDepthPercentage / 100.0f);
        GeometryShaderManager::constants.stereoparams[0] =
            (g_ActiveConfig.bStereoSwapEyes) ? offset : -offset;
        GeometryShaderManager::constants.stereoparams[1] =
            (g_ActiveConfig.bStereoSwapEyes) ? -offset : offset;
      }
      else
      {
        GeometryShaderManager::constants.stereoparams[0] =
            GeometryShaderManager::constants.stereoparams[1] = 0;
      }
      GeometryShaderManager::constants.stereoparams[2] =
          (float)(g_ActiveConfig.iStereoConvergence *
                  (g_ActiveConfig.iStereoConvergencePercentage / 100.0f));
    }
    return;
  }
  // This was already copied from the fullscreen EFB.
  // Which makes it already correct for the HMD's FOV.
  // But we still need to correct it for the difference between the requested and rendered viewport.
  // Don't add any stereoscopy because that was already done when copied.
  else if (bFullscreenLayer)
  {
    Matrix44 projMtx, scale_matrix, correctedMtx;
    Matrix44::Set(projMtx, g_fProjectionMatrix);

    projMtx.xx *= fWidthHack;
    projMtx.yy *= fHeightHack;
    projMtx.wx += fRightHack;
    projMtx.wy += fUpHack;

    Matrix44::LoadIdentity(scale_matrix);
    correctedMtx = projMtx * scale_matrix;

    memcpy(constants.projection.data(), correctedMtx.data, 4 * 16);
    memcpy(constants_eye_projection[0], correctedMtx.data, 4 * 16);
    memcpy(constants_eye_projection[1], correctedMtx.data, 4 * 16);
    GeometryShaderManager::constants.stereoparams[0] =
        GeometryShaderManager::constants.stereoparams[1] = 0;
    GeometryShaderManager::constants.stereoparams[2] =
        GeometryShaderManager::constants.stereoparams[3] = 0;
    return;
  }
  // VR HMD 3D projection matrix, needs to include head-tracking
  else
  {
    float* p = rawProjection;
    // near clipping plane in game units
    float zfar, znear, zNear3D, hfov, vfov;

    // if the camera is zoomed in so much that the action only fills a tiny part of your FOV,
    // we need to move the camera forwards until objects at AimDistance fill the minimum FOV.
    float zoom_forward = 0.0f;
    if (vr_widest_3d_HFOV <= g_ActiveConfig.fMinFOV && vr_widest_3d_HFOV > 0 && iTelescopeHack <= 0)
    {
      zoom_forward = g_ActiveConfig.fAimDistance *
                     tanf(DEGREES_TO_RADIANS(g_ActiveConfig.fMinFOV) / 2) /
                     tanf(DEGREES_TO_RADIANS(vr_widest_3d_HFOV) / 2);
      zoom_forward -= g_ActiveConfig.fAimDistance;
    }

    // Real 3D scene
    if (xfmem.projection.type == GX_PERSPECTIVE && g_viewport_type != VIEW_HUD_ELEMENT &&
        g_viewport_type != VIEW_OFFSCREEN)
    {
      zfar = p[5] / p[4];
      znear = (1 + p[5]) / p[4];
      float zn2 = p[5] / (p[4] - 1);
      float zf2 = p[5] / (p[4] + 1);
      hfov = 2 * atan(1.0f / p[0]) * 180.0f / 3.1415926535f;
      vfov = 2 * atan(1.0f / p[2]) * 180.0f / 3.1415926535f;
      if (debug_newScene)
        INFO_LOG(VR, "Real 3D scene: hfov=%8.4f    vfov=%8.4f      znear=%8.4f or %8.4f   "
                     "zfar=%8.4f or %8.4f",
                 hfov, vfov, znear, zn2, zfar, zf2);
      // prevent near z-clipping by moving near clipping plane closer (may cause z-fighting though)
      // needed for Animal Crossing on GameCube
      // znear *= 0.3f;

      // Find the game's camera angle and position by looking at the view/model matrix of the first
      // real 3D object drawn.
      // This won't work for all games.
      if (!g_vr_had_3D_already)
      {
        CheckOrientationConstants();
        g_vr_had_3D_already = true;
      }
    }
    // 2D layer we will turn into a 3D scene
    // or 3D HUD element that we will treat like a part of the 2D HUD
    else
    {
      m_layer_on_top = g_ActiveConfig.bHudOnTop;
      if (vr_widest_3d_HFOV > 0)
      {
        znear = vr_widest_3d_zNear;
        zfar = vr_widest_3d_zFar;
        if (zoom_forward != 0)
        {
          hfov = g_ActiveConfig.fMinFOV;
          vfov = g_ActiveConfig.fMinFOV * vr_widest_3d_VFOV / vr_widest_3d_HFOV;
        }
        else
        {
          hfov = vr_widest_3d_HFOV;
          vfov = vr_widest_3d_VFOV;
        }
        if (debug_newScene)
          INFO_LOG(VR, "2D to fit 3D world: hfov=%8.4f    vfov=%8.4f      znear=%8.4f   zfar=%8.4f",
                   hfov, vfov, znear, zfar);
      }
      else
      {
        // NES games have a flickery Wii menu otherwise
        if (g_is_nes)
          m_layer_on_top = true;
        // default, if no 3D in scene
        znear = 0.2f * UnitsPerMetre * 20;  // 50cm
        zfar = 40 * UnitsPerMetre;          // 40m
        hfov = 70;                          // 70 degrees
        if (g_is_nes)
          vfov =
              180.0f / 3.14159f * 2 * atanf(tanf((hfov * 3.14159f / 180.0f) / 2) * 1.0f / 1.175f);
        else if (g_renderer->m_aspect_wide)
          vfov =
              180.0f / 3.14159f * 2 * atanf(tanf((hfov * 3.14159f / 180.0f) / 2) * 9.0f /
                                            16.0f);  // 2D screen is meant to be 16:9 aspect ratio
        else
          vfov = 180.0f / 3.14159f * 2 * atanf(tanf((hfov * 3.14159f / 180.0f) / 2) * 3.0f /
                                               4.0f);  //  2D screen is meant to be 4:3 aspect
                                                       //  ratio, make it the same width but taller
        // TODO: fix aspect ratio in other virtual console games
        if (debug_newScene)
          DEBUG_LOG(VR, "Only 2D Projecting: %g x %g, n=%fm f=%fm", hfov, vfov, znear, zfar);
      }
      zNear3D = znear;
      znear /= 40.0f;
      if (debug_newScene)
        DEBUG_LOG(VR, "2D: zNear3D = %f, znear = %f, zFar = %f", zNear3D, znear, zfar);
      g_fProjectionMatrix[0] = 1.0f;
      g_fProjectionMatrix[1] = 0.0f;
      g_fProjectionMatrix[2] = 0.0f;
      g_fProjectionMatrix[3] = 0.0f;

      g_fProjectionMatrix[4] = 0.0f;
      g_fProjectionMatrix[5] = 1.0f;
      g_fProjectionMatrix[6] = 0.0f;
      g_fProjectionMatrix[7] = 0.0f;

      g_fProjectionMatrix[8] = 0.0f;
      g_fProjectionMatrix[9] = 0.0f;
      g_fProjectionMatrix[10] = -znear / (zfar - znear);
      g_fProjectionMatrix[11] = -zfar * znear / (zfar - znear);

      g_fProjectionMatrix[12] = 0.0f;
      g_fProjectionMatrix[13] = 0.0f;

      g_fProjectionMatrix[14] = -1.0f;
      g_fProjectionMatrix[15] = 0.0f;

    }

    Matrix44 proj_left, proj_right, hmd_left, hmd_right;
    Matrix44::Set(proj_left, g_fProjectionMatrix);
    Matrix44::Set(proj_right, g_fProjectionMatrix);

    VR_GetProjectionMatrices(hmd_left, hmd_right, znear, zfar);

    float hfov2 = 2 * atan(1.0f / hmd_left.data[0 * 4 + 0]) * 180.0f / 3.1415926535f;
    float vfov2 = 2 * atan(1.0f / hmd_left.data[1 * 4 + 1]) * 180.0f / 3.1415926535f;
    float zfar2 = hmd_left.data[2 * 4 + 3] / hmd_left.data[2 * 4 + 2];
    float znear2 = (1 + hmd_left.data[2 * 4 + 2] * zfar) / hmd_left.data[2 * 4 + 2];
    if (debug_newScene)
    {
      // yellow = HMD's suggestion
      DEBUG_LOG(VR, "O hfov=%8.4f    vfov=%8.4f      znear=%8.4f   zfar=%8.4f", hfov2, vfov2,
                znear2, zfar2);
      DEBUG_LOG(VR, "O [%8.4f %8.4f %8.4f   %8.4f]", hmd_left.data[0 * 4 + 0],
                hmd_left.data[0 * 4 + 1], hmd_left.data[0 * 4 + 2], hmd_left.data[0 * 4 + 3]);
      DEBUG_LOG(VR, "O [%8.4f %8.4f %8.4f   %8.4f]", hmd_left.data[1 * 4 + 0],
                hmd_left.data[1 * 4 + 1], hmd_left.data[1 * 4 + 2], hmd_left.data[1 * 4 + 3]);
      DEBUG_LOG(VR, "O [%8.4f %8.4f %8.4f   %8.4f]", hmd_left.data[2 * 4 + 0],
                hmd_left.data[2 * 4 + 1], hmd_left.data[2 * 4 + 2], hmd_left.data[2 * 4 + 3]);
      DEBUG_LOG(VR, "O {%8.4f %8.4f %8.4f   %8.4f}", hmd_left.data[3 * 4 + 0],
                hmd_left.data[3 * 4 + 1], hmd_left.data[3 * 4 + 2], hmd_left.data[3 * 4 + 3]);
      // green = Game's suggestion
      INFO_LOG(VR, "G [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[0 * 4 + 0],
               proj_left.data[0 * 4 + 1], proj_left.data[0 * 4 + 2], proj_left.data[0 * 4 + 3]);
      INFO_LOG(VR, "G [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[1 * 4 + 0],
               proj_left.data[1 * 4 + 1], proj_left.data[1 * 4 + 2], proj_left.data[1 * 4 + 3]);
      INFO_LOG(VR, "G [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[2 * 4 + 0],
               proj_left.data[2 * 4 + 1], proj_left.data[2 * 4 + 2], proj_left.data[2 * 4 + 3]);
      INFO_LOG(VR, "G {%8.4f %8.4f %8.4f   %8.4f}", proj_left.data[3 * 4 + 0],
               proj_left.data[3 * 4 + 1], proj_left.data[3 * 4 + 2], proj_left.data[3 * 4 + 3]);
    }
    // red = my combination
    proj_left.data[0 * 4 + 0] =
        hmd_left.data[0 * 4 + 0] * SignOf(proj_left.data[0 * 4 + 0]) * fLeftWidthHack;  // h fov
    proj_left.data[1 * 4 + 1] =
        hmd_left.data[1 * 4 + 1] * SignOf(proj_left.data[1 * 4 + 1]) * fLeftHeightHack;  // v fov
    proj_left.data[0 * 4 + 2] =
        hmd_left.data[0 * 4 + 2] * SignOf(proj_left.data[0 * 4 + 0]) - fRightHack;  // h off-axis
    proj_left.data[1 * 4 + 2] =
        hmd_left.data[1 * 4 + 2] * SignOf(proj_left.data[1 * 4 + 1]) - fUpHack;  // v off-axis
    proj_right.data[0 * 4 + 0] =
        hmd_right.data[0 * 4 + 0] * SignOf(proj_right.data[0 * 4 + 0]) * fRightWidthHack;
    proj_right.data[1 * 4 + 1] =
        hmd_right.data[1 * 4 + 1] * SignOf(proj_right.data[1 * 4 + 1]) * fRightHeightHack;
    proj_right.data[0 * 4 + 2] =
        hmd_right.data[0 * 4 + 2] * SignOf(proj_right.data[0 * 4 + 0]) - fRightHack;
    proj_right.data[1 * 4 + 2] =
        hmd_right.data[1 * 4 + 2] * SignOf(proj_right.data[1 * 4 + 1]) - fUpHack;
    GeometryShaderManager::constants.stereoparams[0] = proj_left.data[0 * 4 + 0];
    GeometryShaderManager::constants.stereoparams[1] = proj_right.data[0 * 4 + 0];
    GeometryShaderManager::constants.stereoparams[2] = proj_left.data[0 * 4 + 2];
    GeometryShaderManager::constants.stereoparams[3] = proj_right.data[0 * 4 + 2];
    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      proj_left.data[0 * 4 + 2] = 0;
    }

    if (debug_newScene)
    {
      DEBUG_LOG(VR, "VR [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[0 * 4 + 0],
                proj_left.data[0 * 4 + 1], proj_left.data[0 * 4 + 2], proj_left.data[0 * 4 + 3]);
      DEBUG_LOG(VR, "VR [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[1 * 4 + 0],
                proj_left.data[1 * 4 + 1], proj_left.data[1 * 4 + 2], proj_left.data[1 * 4 + 3]);
      DEBUG_LOG(VR, "VR [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[2 * 4 + 0],
                proj_left.data[2 * 4 + 1], proj_left.data[2 * 4 + 2], proj_left.data[2 * 4 + 3]);
      DEBUG_LOG(VR, "VR {%8.4f %8.4f %8.4f   %8.4f}", proj_left.data[3 * 4 + 0],
                proj_left.data[3 * 4 + 1], proj_left.data[3 * 4 + 2], proj_left.data[3 * 4 + 3]);
    }

    // VR Headtracking and leaning back compensation
    Matrix44 rotation_matrix;
    Matrix44 lean_back_matrix;
    Matrix44 camera_pitch_matrix;
    if (bStuckToHead)
    {
      Matrix44::LoadIdentity(rotation_matrix);
      Matrix44::LoadIdentity(lean_back_matrix);
      Matrix44::LoadIdentity(camera_pitch_matrix);
    }
    else
    {
      // head tracking
      if (g_ActiveConfig.bOrientationTracking)
      {
        VR_UpdateHeadTrackingIfNeeded();
        rotation_matrix = g_head_tracking_matrix;
      }
      else
      {
        Matrix44::LoadIdentity(rotation_matrix);
      }

      Matrix33 pitch_matrix33;

      // leaning back
      float extra_pitch = -g_ActiveConfig.fLeanBackAngle;
      Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
      lean_back_matrix = pitch_matrix33;

      // camera pitch
      if ((g_ActiveConfig.bStabilizePitch || g_ActiveConfig.bStabilizeRoll ||
           g_ActiveConfig.bStabilizeYaw) &&
          g_ActiveConfig.bCanReadCameraAngles &&
          (g_ActiveConfig.iMotionSicknessSkybox != 2 || !g_is_skybox))
      {
        if (!g_ActiveConfig.bStabilizePitch)
        {
          Matrix44 user_pitch44;
          Matrix44 roll_and_yaw_matrix;

          if (xfmem.projection.type == GX_PERSPECTIVE || vr_widest_3d_HFOV > 0)
            extra_pitch = g_ActiveConfig.fCameraPitch;
          else
            extra_pitch = g_ActiveConfig.fScreenPitch;
          Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
          user_pitch44 = pitch_matrix33;
          roll_and_yaw_matrix = g_game_camera_rotmat;
          camera_pitch_matrix = user_pitch44 * roll_and_yaw_matrix;
        }
        else
        {
          camera_pitch_matrix = g_game_camera_rotmat;
        }
      }
      else
      {
        if (xfmem.projection.type == GX_PERSPECTIVE || vr_widest_3d_HFOV > 0)
          extra_pitch = g_ActiveConfig.fCameraPitch;
        else
          extra_pitch = g_ActiveConfig.fScreenPitch;
        Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
        camera_pitch_matrix = pitch_matrix33;
      }
    }

    // VR sometimes yaw needs to be inverted for games that use a flipped x axis
    // (ActionGirlz even uses flipped matrices and non-flipped matrices in the same frame)
    if (xfmem.projection.type == GX_PERSPECTIVE)
    {
      if (rawProjection[0] < 0)
      {
        if (debug_newScene)
          INFO_LOG(VR, "flipped X");
        // flip all the x axis values, except x squared (data[0])
        // Needed for Action Girlz Racing, Backyard Baseball
        // rotation_matrix.data[1] *= -1;
        // rotation_matrix.data[2] *= -1;
        // rotation_matrix.data[3] *= -1;
        // rotation_matrix.data[4] *= -1;
        // rotation_matrix.data[8] *= -1;
        // rotation_matrix.data[12] *= -1;
        flipped_x = -1;
      }
      if (rawProjection[2] < 0)
      {
        if (debug_newScene)
          INFO_LOG(VR, "flipped Y");
        // flip all the y axis values, except y squared (data[5])
        // Needed for ABBA
        // rotation_matrix.data[1] *= -1;
        // rotation_matrix.data[4] *= -1;
        // rotation_matrix.data[6] *= -1;
        // rotation_matrix.data[7] *= -1;
        // rotation_matrix.data[9] *= -1;
        // rotation_matrix.data[13] *= -1;
        flipped_y = -1;
      }
    }

    // Position matrices
    Matrix44 head_position_matrix, free_look_matrix, camera_forward_matrix, camera_position_matrix;
    if (bStuckToHead || g_is_skybox)
    {
      Matrix44::LoadIdentity(head_position_matrix);
      Matrix44::LoadIdentity(free_look_matrix);
      Matrix44::LoadIdentity(camera_position_matrix);
    }
    else
    {
      float pos[3];
      // head tracking
      if (g_ActiveConfig.bPositionTracking)
      {
        for (int i = 0; i < 3; ++i)
          pos[i] = g_head_tracking_position[i] * UnitsPerMetre;
        Matrix44::Translate(head_position_matrix, pos);
      }
      else
      {
        Matrix44::LoadIdentity(head_position_matrix);
      }

      // freelook camera position
      for (int i = 0; i < 3; ++i)
        pos[i] = s_fViewTranslationVector[i] * UnitsPerMetre;
      Matrix44::Translate(free_look_matrix, pos);

      // camera position stabilisation
      if (g_ActiveConfig.bStabilizeX || g_ActiveConfig.bStabilizeY || g_ActiveConfig.bStabilizeZ)
      {
        for (int i = 0; i < 3; ++i)
          pos[i] = -g_game_camera_pos[i] * UnitsPerMetre;
        Matrix44::Translate(camera_position_matrix, pos);
      }
      else
      {
        Matrix44::LoadIdentity(camera_position_matrix);
      }
    }

    Matrix44 look_matrix;
    if (xfmem.projection.type == GX_PERSPECTIVE && g_viewport_type != VIEW_HUD_ELEMENT &&
        g_viewport_type != VIEW_OFFSCREEN)
    {
      // Transformations must be applied in the following order for VR:
      // camera position stabilisation
      // camera forward
      // camera pitch
      // free look
      // leaning back
      // head position tracking
      // head rotation tracking
      if (bNoForward || g_is_skybox || bStuckToHead)
      {
        Matrix44::LoadIdentity(camera_forward_matrix);
      }
      else
      {
        float pos[3];
        pos[0] = 0;
        pos[1] = 0;
        pos[2] = (g_ActiveConfig.fCameraForward + zoom_forward) * UnitsPerMetre;
        Matrix44::Translate(camera_forward_matrix, pos);
      }

      look_matrix = camera_forward_matrix * camera_position_matrix * camera_pitch_matrix *
                    free_look_matrix * lean_back_matrix * head_position_matrix * rotation_matrix;
    }
    else
    // if (xfmem.projection.type != GX_PERSPECTIVE || g_viewport_type == VIEW_HUD_ELEMENT ||
    // g_viewport_type == VIEW_OFFSCREEN)
    {
      if (debug_newScene)
        INFO_LOG(VR, "2D: hacky test");

      float HudWidth, HudHeight, HudThickness, HudDistance, HudUp, CameraForward, AimDistance;

      g_fProjectionMatrix[14] = 0.0f;
      // 2D Screen
      if (vr_widest_3d_HFOV <= 0)
      {
        HudThickness = g_ActiveConfig.fScreenThickness * UnitsPerMetre;
        HudDistance = g_ActiveConfig.fScreenDistance * UnitsPerMetre;
        HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
        HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
        // NES games are supposed to be 1.175:1 (16:13.62) even though VC normally renders them as
        // 16:9
        // http://forums.nesdev.com/viewtopic.php?t=8063
        if (g_is_nes)
          HudWidth = HudHeight * 1.175f;
        else if (g_renderer->m_aspect_wide)
          HudWidth = HudHeight * (float)16 / 9;
        else
          HudWidth = HudHeight * (float)4 / 3;
        CameraForward = 0;
        HudUp = g_ActiveConfig.fScreenUp * UnitsPerMetre;
        AimDistance = HudDistance;
      }
      else
      // HUD over 3D world
      {
        // The HUD distance might have been carefully chosen to line up with objects, so we should
        // scale it with the world
        // But we can't make the HUD too close or it's hard to look at, and we should't make the HUD
        // too far or it stops looking 3D
        const float MinHudDistance = 0.28f,
                    MaxHudDistance = 3.00f;  // HUD shouldn't go closer than 28 cm when shrinking
                                             // scale, or further than 3m when growing
        float HUDScale = g_ActiveConfig.fScale;
        if (HUDScale < 1.0f && g_ActiveConfig.fHudDistance >= MinHudDistance &&
            g_ActiveConfig.fHudDistance * HUDScale < MinHudDistance)
          HUDScale = MinHudDistance / g_ActiveConfig.fHudDistance;
        else if (HUDScale > 1.0f && g_ActiveConfig.fHudDistance <= MaxHudDistance &&
                 g_ActiveConfig.fHudDistance * HUDScale > MaxHudDistance)
          HUDScale = MaxHudDistance / g_ActiveConfig.fHudDistance;

        // Give the 2D layer a 3D effect if different parts of the 2D layer are rendered at
        // different z coordinates
        HudThickness =
            g_ActiveConfig.fHudThickness * HUDScale *
            UnitsPerMetre;  // the 2D layer is actually a 3D box this many game units thick
        HudDistance = g_ActiveConfig.fHudDistance * HUDScale *
                      UnitsPerMetre;  // depth 0 on the HUD should be this far away
        HudUp = 0;
        if (bNoForward)
          CameraForward = 0;
        else
          CameraForward = (g_ActiveConfig.fCameraForward + zoom_forward) * g_ActiveConfig.fScale *
                          UnitsPerMetre;
        // When moving the camera forward, correct the size of the HUD so that aiming is correct at
        // AimDistance
        AimDistance = g_ActiveConfig.fAimDistance * g_ActiveConfig.fScale * UnitsPerMetre;
        if (AimDistance <= 0)
          AimDistance = HudDistance;
        // Now that we know how far away the box is, and what FOV it should fill, we can work out
        // the width and height in game units
        // Note: the HUD won't line up exactly (except at AimDistance) if CameraForward is non-zero
        // float HudWidth = 2.0f * tanf(hfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) *
        // Correction;
        // float HudHeight = 2.0f * tanf(vfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) *
        // Correction;
        HudWidth = 2.0f * tanf(DEGREES_TO_RADIANS(hfov / 2.0f)) * HudDistance *
                   (AimDistance + CameraForward) / AimDistance;
        HudHeight = 2.0f * tanf(DEGREES_TO_RADIANS(vfov / 2.0f)) * HudDistance *
                    (AimDistance + CameraForward) / AimDistance;
      }

      float scale[3];  // width, height, and depth of box in game units divided by 2D width, height,
                       // and depth
      float position[3];  // position of front of box relative to the camera, in game units

      float viewport_scale[2];
      float viewport_offset[2];  // offset as a fraction of the viewport's width
      if (g_viewport_type != VIEW_HUD_ELEMENT && g_viewport_type != VIEW_OFFSCREEN)
      {
        viewport_scale[0] = 1.0f;
        viewport_scale[1] = 1.0f;
        viewport_offset[0] = 0.0f;
        viewport_offset[1] = 0.0f;
      }
      else
      {
        Viewport& v = xfmem.viewport;
        float left, top, width, height;
        left = v.xOrig - v.wd - 342;
        top = v.yOrig + v.ht - 342;
        width = 2 * v.wd;
        height = -2 * v.ht;
        float screen_width = (float)g_final_screen_region.GetWidth();
        float screen_height = (float)g_final_screen_region.GetHeight();
        viewport_scale[0] = width / screen_width;
        viewport_scale[1] = height / screen_height;
        viewport_offset[0] = ((left + (width / 2)) - (0 + (screen_width / 2))) / screen_width;
        viewport_offset[1] = -((top + (height / 2)) - (0 + (screen_height / 2))) / screen_height;
      }

      // 3D HUD elements (may be part of 2D screen or HUD)
      if (xfmem.projection.type == GX_PERSPECTIVE)
      {
        // these are the edges of the near clipping plane in game coordinates
        float left2D = -(rawProjection[1] + 1) / rawProjection[0];
        float right2D = left2D + 2 / rawProjection[0];
        float bottom2D = -(rawProjection[3] + 1) / rawProjection[2];
        float top2D = bottom2D + 2 / rawProjection[2];
        float zFar2D = rawProjection[5] / rawProjection[4];
        float zNear2D = zFar2D * rawProjection[4] / (rawProjection[4] - 1);
        float zObj = zNear2D + (zFar2D - zNear2D) * g_ActiveConfig.fHud3DCloser;
		
        left2D *= zObj;
        right2D *= zObj;
        bottom2D *= zObj;
        top2D *= zObj;

        // Scale the width and height to fit the HUD in metres
        if (rawProjection[0] == 0 || right2D == left2D)
        {
          scale[0] = 0;
        }
        else
        {
          scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
        }
        if (rawProjection[2] == 0 || top2D == bottom2D)
        {
          scale[1] = 0;
        }
        else
        {
          scale[1] = viewport_scale[1] * HudHeight /
                     (top2D - bottom2D);  // note that positive means up in 3D
        }
        // Keep depth the same scale as width, so it looks like a real object
        if (rawProjection[4] == 0 || zFar2D == zNear2D)
        {
          scale[2] = scale[0];
        }
        else
        {
          scale[2] = scale[0];
        }
        // Adjust the position for off-axis projection matrices, and shifting the 2D screen
        position[0] = scale[0] * (-(right2D + left2D) / 2.0f) +
                      viewport_offset[0] * HudWidth;  // shift it right into the centre of the view
        position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight +
                      HudUp;  // shift it up into the centre of the view;
        // Shift it from the old near clipping plane to the HUD distance, and shift the camera
        // forward
        if (vr_widest_3d_HFOV <= 0)
          position[2] = scale[2] * zObj - HudDistance;
        else
          position[2] = scale[2] * zObj - HudDistance;  // - CameraForward;
      }
      // 2D layer, or 2D viewport (may be part of 2D screen or HUD)
      else
      {
        float left2D = -(rawProjection[1] + 1) / rawProjection[0];
        float right2D = left2D + 2 / rawProjection[0];
        float bottom2D = -(rawProjection[3] + 1) / rawProjection[2];
        float top2D = bottom2D + 2 / rawProjection[2];
        float zFar2D, zNear2D;
        zFar2D = rawProjection[5] / rawProjection[4];
        zNear2D = (1 + rawProjection[4] * zFar2D) / rawProjection[4];

        // for 2D, work out the fraction of the HUD we should fill, and multiply the scale by that
        // also work out what fraction of the height we should shift it up, and what fraction of the
        // width we should shift it left
        // only multiply by the extra scale after adjusting the position?

        if (rawProjection[0] == 0 || right2D == left2D)
        {
          scale[0] = 0;
        }
        else
        {
          scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
        }
        if (rawProjection[2] == 0 || top2D == bottom2D)
        {
          scale[1] = 0;
        }
        else
        {
          scale[1] = viewport_scale[1] * HudHeight /
                     (top2D - bottom2D);  // note that positive means up in 3D
        }
        if (rawProjection[4] == 0 || zFar2D == zNear2D)
        {
          scale[2] = 0;  // The 2D layer was flat, so we make it flat instead of a box to avoid
                         // dividing by zero
        }
        else
        {
          scale[2] =
              HudThickness /
              (zFar2D -
               zNear2D);  // Scale 2D z values into 3D game units so it is the right thickness
        }
        position[0] = scale[0] * (-(right2D + left2D) / 2.0f) +
                      viewport_offset[0] * HudWidth;  // shift it right into the centre of the view
        position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight +
                      HudUp;  // shift it up into the centre of the view;
        // Shift it from the zero plane to the HUD distance, and shift the camera forward
        if (vr_widest_3d_HFOV <= 0)
          position[2] = -HudDistance;
        else
          position[2] = -HudDistance;  // - CameraForward;
      }
	  
	  
	  position[0] += (g_ActiveConfig.fHudDespPosition0 * g_ActiveConfig.fUnitsPerMetre);
	  position[1] += (g_ActiveConfig.fHudDespPosition1 * g_ActiveConfig.fUnitsPerMetre);
	  position[2] += (g_ActiveConfig.fHudDespPosition2 * g_ActiveConfig.fUnitsPerMetre);
	  
	  Matrix44 hud_matrix;
	  Matrix44::LoadMatrix33(hud_matrix, g_ActiveConfig.matrixHudrot);

      Matrix44 scale_matrix, position_matrix;
      Matrix44::Scale(scale_matrix, scale);
      Matrix44::Translate(position_matrix, position);
	  
	  look_matrix = scale_matrix * hud_matrix * position_matrix * camera_position_matrix * camera_pitch_matrix *
		  free_look_matrix * lean_back_matrix * head_position_matrix * rotation_matrix;
    }

    // N64 games give us coordinates that were already transformed into clip space
    // we need to untransform them before we do anything else
    if (bN64)
    {
      Matrix44 shift_matrix, scale_matrix;
      shift_matrix.setIdentity();
      shift_matrix.data[0 * 4 + 2] = 1.0f;
      shift_matrix.data[1 * 4 + 2] = 1.0f;
      float scale[3];
      float w = tanf(DEGREES_TO_RADIANS(g_ActiveConfig.fN64FOV) * 0.5f);
      scale[0] = w;
      scale[1] = w * (3.0f / 4.0f);  // N64 aspect ratio is 4:3
      scale[2] = 1.0f;               // the Z coordinates are already correct
      scale_matrix.setScaling(scale);
      look_matrix = shift_matrix * scale_matrix * look_matrix;
    }

    Matrix44 eye_pos_matrix_left, eye_pos_matrix_right;
    float posLeft[3] = {0, 0, 0};
    float posRight[3] = {0, 0, 0};
    if (!g_is_skybox)
    {
      VR_GetEyePos(posLeft, posRight);
      for (int i = 0; i < 3; ++i)
      {
        posLeft[i] *= UnitsPerMetre;
        posRight[i] *= UnitsPerMetre;
      }
    }

    Matrix44 view_matrix_left, view_matrix_right;
    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      Matrix44::Set(view_matrix_left, look_matrix.data);
      Matrix44::Set(view_matrix_right, view_matrix_left.data);
    }
    else
    {
      Matrix44::Translate(eye_pos_matrix_left, posLeft);
      Matrix44::Translate(eye_pos_matrix_right, posRight);
      Matrix44::Multiply(eye_pos_matrix_left, look_matrix, view_matrix_left);
      Matrix44::Multiply(eye_pos_matrix_right, look_matrix, view_matrix_right);
    }
    Matrix44 final_matrix_left, final_matrix_right;
    Matrix44::Multiply(proj_left, view_matrix_left, final_matrix_left);
    Matrix44::Multiply(proj_right, view_matrix_right, final_matrix_right);
    if (flipped_x < 0)
    {
      // flip all the x axis values, except x squared (data[0])
      // Needed for Action Girlz Racing, Backyard Baseball
      final_matrix_left.data[1] *= -1;
      final_matrix_left.data[2] *= -1;
      final_matrix_left.data[3] *= -1;
      GeometryShaderManager::constants.stereoparams[2] *= -1;
      final_matrix_left.data[4] *= -1;
      final_matrix_left.data[8] *= -1;
      final_matrix_left.data[12] *= -1;
      final_matrix_right.data[1] *= -1;
      final_matrix_right.data[2] *= -1;
      final_matrix_right.data[3] *= -1;
      GeometryShaderManager::constants.stereoparams[3] *= -1;
      final_matrix_right.data[4] *= -1;
      final_matrix_right.data[8] *= -1;
      final_matrix_right.data[12] *= -1;
      GeometryShaderManager::constants.stereoparams[0] *= -1;
      GeometryShaderManager::constants.stereoparams[1] *= -1;
    }
    if (flipped_y < 0)
    {
      final_matrix_left.data[1] *= -1;
      final_matrix_left.data[4] *= -1;
      final_matrix_left.data[6] *= -1;
      final_matrix_left.data[7] *= -1;
      final_matrix_left.data[9] *= -1;
      final_matrix_left.data[13] *= -1;
      final_matrix_right.data[1] *= -1;
      final_matrix_right.data[4] *= -1;
      final_matrix_right.data[6] *= -1;
      final_matrix_right.data[7] *= -1;
      final_matrix_right.data[9] *= -1;
      final_matrix_right.data[13] *= -1;
    }

    // If we are supposed to hide the layer, zero out the projection matrix
    if (bHideLeft && (bHideRight || !(g_ActiveConfig.iStereoMode > 0)))
    {
      memset(final_matrix_left.data, 0, 16 * sizeof(final_matrix_left.data[0]));
    }
    if (bHideRight)
    {
      memset(final_matrix_right.data, 0, 16 * sizeof(final_matrix_right.data[0]));
    }

    memcpy(constants.projection.data(), final_matrix_left.data, 4 * 16);
    memcpy(constants_eye_projection[0], final_matrix_left.data, 4 * 16);
    memcpy(constants_eye_projection[1], final_matrix_right.data, 4 * 16);
    if (g_ActiveConfig.iStereoMode == STEREO_OCULUS)
    {
      GeometryShaderManager::constants.stereoparams[0] *= posLeft[0];
      GeometryShaderManager::constants.stereoparams[1] *= posRight[0];
      if (debug_newScene)
      {
        DEBUG_LOG(VR, "F=[%8.4f %8.4f %8.4f   %8.4f]", final_matrix_left.data[0 * 4 + 0],
                  final_matrix_left.data[0 * 4 + 1], final_matrix_left.data[0 * 4 + 2],
                  final_matrix_left.data[0 * 4 + 3]);
        DEBUG_LOG(VR, "F=[%8.4f %8.4f %8.4f   %8.4f]", final_matrix_left.data[1 * 4 + 0],
                  final_matrix_left.data[1 * 4 + 1], final_matrix_left.data[1 * 4 + 2],
                  final_matrix_left.data[1 * 4 + 3]);
        DEBUG_LOG(VR, "F=[%8.4f %8.4f %8.4f   %8.4f]", final_matrix_left.data[2 * 4 + 0],
                  final_matrix_left.data[2 * 4 + 1], final_matrix_left.data[2 * 4 + 2],
                  final_matrix_left.data[2 * 4 + 3]);
        DEBUG_LOG(VR, "F={%8.4f %8.4f %8.4f   %8.4f}", final_matrix_left.data[3 * 4 + 0],
                  final_matrix_left.data[3 * 4 + 1], final_matrix_left.data[3 * 4 + 2],
                  final_matrix_left.data[3 * 4 + 3]);
        DEBUG_LOG(VR, "StereoParams: %8.4f, %8.4f",
                  GeometryShaderManager::constants.stereoparams[0],
                  GeometryShaderManager::constants.stereoparams[2]);
      }
    }
    else
    {
      GeometryShaderManager::constants.stereoparams[0] =
          GeometryShaderManager::constants.stereoparams[1] = 0;
    }
  }

  if (bTexMtxInfoChanged)
  {
    bTexMtxInfoChanged = false;
    constants.xfmem_dualTexInfo = xfmem.dualTexTrans.enabled;
    for (size_t i = 0; i < ArraySize(xfmem.texMtxInfo); i++)
      constants.xfmem_pack1[i][0] = xfmem.texMtxInfo[i].hex;
    for (size_t i = 0; i < ArraySize(xfmem.postMtxInfo); i++)
      constants.xfmem_pack1[i][1] = xfmem.postMtxInfo[i].hex;

    dirty = true;
  }

  if (bLightingConfigChanged)
  {
    bLightingConfigChanged = false;

    for (size_t i = 0; i < 2; i++)
    {
      constants.xfmem_pack1[i][2] = xfmem.color[i].hex;
      constants.xfmem_pack1[i][3] = xfmem.alpha[i].hex;
    }
    constants.xfmem_numColorChans = xfmem.numChan.numColorChans;

    dirty = true;
  }
}

void VertexShaderManager::CheckOrientationConstants()
{
#define sqr(a) ((a) * (a))
  bool can_read = g_ActiveConfig.bCanReadCameraAngles &&
                  (g_ActiveConfig.bStabilizePitch || g_ActiveConfig.bStabilizeRoll ||
                   g_ActiveConfig.bStabilizeYaw || g_ActiveConfig.bStabilizeX ||
                   g_ActiveConfig.bStabilizeY || g_ActiveConfig.bStabilizeZ);

  if (can_read)
  {
    static int old_vertex_count = 0, old_prim_count = 0;
    // int vertex_count = g_vertex_manager->GetNumberOfVertices();
    // if (vertex_count != old_vertex_count) {
    //	NOTICE_LOG(VR, "*************** vertex_count = %d", vertex_count);
    //	old_vertex_count = vertex_count;
    //}
    int prim_count = stats.prevFrame.numPrims + stats.prevFrame.numDLPrims;
    if (prim_count != old_prim_count)
    {
      WARN_LOG(VR, "*************** stats.prevFrame.numPrims = %d       %d", prim_count,
               stats.thisFrame.numPrims + stats.thisFrame.numDLPrims);
      old_prim_count = prim_count;
    }
    if (prim_count < (int)g_ActiveConfig.iCameraMinPoly)
      can_read = false;
  }
  if (can_read)
  {
    float* p = constants.posnormalmatrix[0].data();
    float pos[3], worldspacepos[3], movement[3];
    pos[0] = p[0 * 4 + 3];
    pos[1] = p[1 * 4 + 3];
    pos[2] = p[2 * 4 + 3];
    Matrix33 rot;
    memcpy(&rot.data[0 * 3], &p[0 * 4], 3 * sizeof(float));
    memcpy(&rot.data[1 * 3], &p[1 * 4], 3 * sizeof(float));
    memcpy(&rot.data[2 * 3], &p[2 * 4], 3 * sizeof(float));
    // normalize rotation matrix
    float scale =
        sqrt(sqr(rot.data[0 * 3 + 0]) + sqr(rot.data[0 * 3 + 1]) + sqr(rot.data[0 * 3 + 2]));
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        rot.data[r * 3 + c] /= scale;
    // Position is already in current camera space (has had rot matrix and scale applied to it).
    // Convert camera-space position into world space position, by applying inverse rot matrix.
    // Note that we are not undoing the scale, because game units are measured in camera space after
    // the scale.
    Matrix33 inverse;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        inverse.data[r * 3 + c] = rot.data[c * 3 + r];
    Matrix33::Multiply(inverse, pos, worldspacepos);
    // Work out how far (in unscaled game units) and in which dimensions it has moved in world space
    // since the previous frame
    for (int i = 0; i < 3; ++i)
      movement[i] = worldspacepos[i] - oldpos[i];
    float distance = sqrt(sqr(movement[0]) + sqr(movement[1]) + sqr(movement[2]));

    INFO_LOG(VR, "WorldPos: %5.2fm, %5.2fm, %5.2fm; Move: %5.2fm, %5.2fm, %5.2fm; Distance: "
                 "%5.2fm; Scale: x%5.2f",
             pos[0] / g_ActiveConfig.fUnitsPerMetre, pos[1] / g_ActiveConfig.fUnitsPerMetre,
             pos[2] / g_ActiveConfig.fUnitsPerMetre, movement[0] / g_ActiveConfig.fUnitsPerMetre,
             movement[1] / g_ActiveConfig.fUnitsPerMetre,
             movement[2] / g_ActiveConfig.fUnitsPerMetre, distance / g_ActiveConfig.fUnitsPerMetre,
             scale);
    // moving more than 2 metres per frame (before VR scaling down to toy size) means we probably
    // jumped to a new object
    // That is 216 kilometres per hour (135 miles per hour) at 30 FPS, or 432 kph (270 mph) at 60
    // FPS
    // so only add actual camera motion, not jumps, to totalpos
    if (distance / g_ActiveConfig.fUnitsPerMetre <= 2.0f &&
        (oldpos[0] != 0 || oldpos[1] != 0 || oldpos[2] != 0))
    {
      for (int i = 0; i < 3; ++i)
        totalpos[i] += movement[i];
      INFO_LOG(VR, "Total Pos: %5.2f, %5.2f, %5.2f", totalpos[0], totalpos[1], totalpos[2]);
    }
    for (int i = 0; i < 3; ++i)
      oldpos[i] = worldspacepos[i];
    // rotate total position back into current game-camera space, and save it globally in metres
    Matrix33::Multiply(rot, totalpos, g_game_camera_pos);
    for (int i = 0; i < 3; ++i)
      g_game_camera_pos[i] = g_game_camera_pos[i] / g_ActiveConfig.fUnitsPerMetre;
    INFO_LOG(VR, "g_game_camera_pos: %5.2fm, %5.2fm, %5.2fm", g_game_camera_pos[0],
             g_game_camera_pos[1], g_game_camera_pos[2]);

    // add pitch to rotation matrix
    if (g_ActiveConfig.fReadPitch != 0)
    {
      Matrix33 rp, result;
      Matrix33::RotateX(rp, DEGREES_TO_RADIANS(g_ActiveConfig.fReadPitch));
      Matrix33::Multiply(rot, rp, result);
      memcpy(rot.data, result.data, 3 * 3 * sizeof(float));
    }
    // extract yaw, pitch, and roll from rotation matrix
    float yaw, pitch, roll;
    Matrix33::GetPieYawPitchRollR(rot, yaw, pitch, roll);

    if (abs(roll) == 3.1415926535f)
    {
      roll = 0;  // Unlikely the camera should actually be flipped exactly 180 degrees. We most
                 // likely chose the wrong object.
    }

    // if (abs(yaw) == 3.1415926535f)
    //{
    //	yaw = 0; // Unlikely the camera should actually be flipped exactly 180 degrees. We most
    // likely chose the wrong object.
    //}

    if (g_ActiveConfig.bKeyhole)
    {
      static float keyhole_center = 0;
      float keyhole_snap = 0;

      if (g_ActiveConfig.bKeyholeSnap)
        keyhole_snap = DEGREES_TO_RADIANS(g_ActiveConfig.fKeyholeSnapSize);

      float keyhole_width = DEGREES_TO_RADIANS(g_ActiveConfig.fKeyholeWidth / 2);
      float keyhole_left_bound = keyhole_center + keyhole_width;
      float keyhole_right_bound = keyhole_center - keyhole_width;

      // Correct left and right bounds if they calculated incorrectly and are out of the range of
      // -PI to PI.
      if (keyhole_left_bound > (float)(M_PI))
        keyhole_left_bound -= (2 * (float)(M_PI));
      else if (keyhole_right_bound < -(float)(M_PI))
        keyhole_right_bound += (2 * (float)(M_PI));

      // Crossing from positive to negative half, counter-clockwise
      if (yaw < 0 && keyhole_left_bound > 0 && keyhole_right_bound > 0 &&
          yaw < keyhole_width - (float)(M_PI))
      {
        keyhole_center = yaw - keyhole_width + keyhole_snap;
      }
      // Crossing from negative to positive half, clockwise
      else if (yaw > 0 && keyhole_left_bound < 0 && keyhole_right_bound < 0 &&
               yaw > (float)(M_PI)-keyhole_width)
      {
        keyhole_center = yaw + keyhole_width - keyhole_snap;
      }
      // Already within the negative and positive range
      else if (keyhole_left_bound < 0 && keyhole_right_bound > 0)
      {
        if (yaw < keyhole_right_bound && yaw > 0)
          keyhole_center = yaw + keyhole_width - keyhole_snap;
        else if (yaw > keyhole_left_bound && yaw < 0)
          keyhole_center = yaw - keyhole_width + keyhole_snap;
      }
      // Anywhere within the normal range
      else
      {
        if (yaw < keyhole_right_bound)
          keyhole_center = yaw + keyhole_width - keyhole_snap;
        else if (yaw > keyhole_left_bound)
          keyhole_center = yaw - keyhole_width + keyhole_snap;
      }

      yaw -= keyhole_center;
    }

    // NOTICE_LOG(VR, "Pos(%d): %5.2f, %5.2f, %5.2f; scale: x%5.2f",
    // g_main_cp_state.matrix_index_a.PosNormalMtxIdx, pos[0], pos[1], pos[2], scale);
    // debug - show which object is being used
    // static float first_x = 0;
    // if (first_x == 0)
    //	first_x = pos[0];
    // else if (g_ActiveConfig.iFlashState > 5)
    //	constants.posnormalmatrix[0][0 * 4 + 3] = first_x;

    Matrix33 matrix_pitch, matrix_yaw, matrix_roll, temp;

    if (g_ActiveConfig.bStabilizeRoll && g_ActiveConfig.bStabilizeYaw)
    {
      Matrix33::RotateZ(matrix_roll, -roll);
      Matrix33::RotateY(matrix_yaw, yaw);
      Matrix33::Multiply(matrix_roll, matrix_yaw, temp);
    }
    else if (g_ActiveConfig.bStabilizeRoll)
    {
      Matrix33::RotateZ(matrix_roll, -roll);
      temp = matrix_roll;
    }
    else if (g_ActiveConfig.bStabilizeYaw)
    {
      Matrix33::RotateY(matrix_yaw, yaw);
      temp = matrix_yaw;
    }
    else
    {
      Matrix33::LoadIdentity(temp);
    }

    if (g_ActiveConfig.bStabilizePitch)
    {
      Matrix33::RotateX(matrix_pitch, -pitch);
      rot = matrix_pitch * temp;
      g_game_camera_rotmat = rot;
    }
    else
    {
      g_game_camera_rotmat = temp;
    }

    // A more elegant solution to all of the if statements above, but probably a lot slower.
    // A lot of excess multiplication if everything is not checked.
    //#define GET_MATRIX_MULTIPLIER(a, b, aset) aset ? a : b

    // Matrix33 identity_matrix = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    // Matrix33::Multiply(GET_MATRIX_MULTIPLIER(matrix_roll, identity_matrix,
    // g_ActiveConfig.bStabilizeRoll), GET_MATRIX_MULTIPLIER(matrix_yaw, identity_matrix,
    // g_ActiveConfig.bStabilizeYaw), temp);
    // Matrix33::Multiply(temp, GET_MATRIX_MULTIPLIER(matrix_pitch, identity_matrix,
    // g_ActiveConfig.bStabilizePitch), rot);

    // yaw = RADIANS_TO_DEGREES(yaw);
    // pitch = RADIANS_TO_DEGREES(pitch);
    // roll = RADIANS_TO_DEGREES(roll);
    // WARN_LOG(VR, "Yaw = %5.2f, Pitch = %5.2f, Roll = %5.2f", yaw, pitch, roll);
    // ERROR_LOG(VR, "Rot: [%5.2f, %5.2f, %5.2f][%5.2f, %5.2f, %5.2f][%5.2f, %5.2f, %5.2f]",
    // rot.data[0 * 3 + 0], rot.data[0 * 3 + 1], rot.data[0 * 3 + 2], rot.data[1 * 3 + 0],
    // rot.data[1 * 3 + 1], rot.data[1 * 3 + 2], rot.data[2 * 3 + 0], rot.data[2 * 3 + 1],
    // rot.data[2 * 3 + 2]);
  }
  else
  {
    Matrix44::LoadIdentity(g_game_camera_rotmat);
    memset(g_game_camera_pos, 0, 3 * sizeof(float));
  }
}

void VertexShaderManager::CheckSkybox()
{
  if (xfmem.projection.type == GX_PERSPECTIVE)
  {
    float* p = constants.posnormalmatrix[0].data();
    float pos[3];
    pos[0] = p[0 * 4 + 3];
    pos[1] = p[1 * 4 + 3];
    pos[2] = p[2 * 4 + 3];
    // If we are drawing at precisely the origin (camera position) it's probably a skybox
    if (pos[0] == 0 && pos[1] == 0 && pos[2] == 0)
    {
      if (p[0 * 4 + 0] != 1.0f)
      {
        // ERROR_LOG(VR, "SKYBOX!!!!");
        g_is_skybox = true;
      }
      else
      {
        // ERROR_LOG(VR, "NOT a skybox! Identity matrix.");
      }
    }
  }
}

void VertexShaderManager::LockSkybox()
{
  if (xfmem.projection.type == GX_PERSPECTIVE)
  {
    float* p = constants.posnormalmatrix[0].data();
    if (s_had_skybox)
    {
      memcpy(p, s_locked_skybox, 4 * 3 * sizeof(float));
    }
    else
    {
      memcpy(s_locked_skybox, p, 4 * 3 * sizeof(float));
      s_had_skybox = true;
    }

    // for (int r = 0; r < 3; ++r)
    //	for (int c = 0; c < 3; ++c)
    //		p[r * 4 + c] = (r == c) ? 1.0f : 0.0f;
  }
}

//#pragma optimize("", on)

void VertexShaderManager::InvalidateXFRange(int start, int end)
{
  if (((u32)start >= (u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4 + 12) ||
      ((u32)start >=
           XFMEM_NORMALMATRICES + ((u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31) * 3 &&
       (u32)start < XFMEM_NORMALMATRICES +
                        ((u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31) * 3 + 9))
  {
    bPosNormalMatrixChanged = true;
  }

  if (((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4 + 12))
  {
    bTexMatricesChanged[0] = true;
  }

  if (((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4 + 12))
  {
    bTexMatricesChanged[1] = true;
  }

  if (start < XFMEM_POSMATRICES_END)
  {
    if (nTransformMatricesChanged[0] == -1)
    {
      nTransformMatricesChanged[0] = start;
      nTransformMatricesChanged[1] = end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
    }
    else
    {
      if (nTransformMatricesChanged[0] > start)
        nTransformMatricesChanged[0] = start;

      if (nTransformMatricesChanged[1] < end)
        nTransformMatricesChanged[1] = end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
    }
  }

  if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES)
  {
    int _start = start < XFMEM_NORMALMATRICES ? 0 : start - XFMEM_NORMALMATRICES;
    int _end = end < XFMEM_NORMALMATRICES_END ? end - XFMEM_NORMALMATRICES :
                                                XFMEM_NORMALMATRICES_END - XFMEM_NORMALMATRICES;

    if (nNormalMatricesChanged[0] == -1)
    {
      nNormalMatricesChanged[0] = _start;
      nNormalMatricesChanged[1] = _end;
    }
    else
    {
      if (nNormalMatricesChanged[0] > _start)
        nNormalMatricesChanged[0] = _start;

      if (nNormalMatricesChanged[1] < _end)
        nNormalMatricesChanged[1] = _end;
    }
  }

  if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES)
  {
    int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start - XFMEM_POSTMATRICES;
    int _end = end < XFMEM_POSTMATRICES_END ? end - XFMEM_POSTMATRICES :
                                              XFMEM_POSTMATRICES_END - XFMEM_POSTMATRICES;

    if (nPostTransformMatricesChanged[0] == -1)
    {
      nPostTransformMatricesChanged[0] = _start;
      nPostTransformMatricesChanged[1] = _end;
    }
    else
    {
      if (nPostTransformMatricesChanged[0] > _start)
        nPostTransformMatricesChanged[0] = _start;

      if (nPostTransformMatricesChanged[1] < _end)
        nPostTransformMatricesChanged[1] = _end;
    }
  }

  if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
  {
    int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start - XFMEM_LIGHTS;
    int _end = end < XFMEM_LIGHTS_END ? end - XFMEM_LIGHTS : XFMEM_LIGHTS_END - XFMEM_LIGHTS;

    if (nLightsChanged[0] == -1)
    {
      nLightsChanged[0] = _start;
      nLightsChanged[1] = _end;
    }
    else
    {
      if (nLightsChanged[0] > _start)
        nLightsChanged[0] = _start;

      if (nLightsChanged[1] < _end)
        nLightsChanged[1] = _end;
    }
  }
}

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
{
  if (g_main_cp_state.matrix_index_a.Hex != Value)
  {
    g_vertex_manager->Flush();
    if (g_main_cp_state.matrix_index_a.PosNormalMtxIdx != (Value & 0x3f))
      bPosNormalMatrixChanged = true;
    bTexMatricesChanged[0] = true;
    g_main_cp_state.matrix_index_a.Hex = Value;
  }
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
  if (g_main_cp_state.matrix_index_b.Hex != Value)
  {
    g_vertex_manager->Flush();
    bTexMatricesChanged[1] = true;
    g_main_cp_state.matrix_index_b.Hex = Value;
  }
}

void VertexShaderManager::SetViewportChanged()
{
  bViewportChanged = true;
}

void VertexShaderManager::SetProjectionChanged()
{
  bProjectionChanged = true;
}

void VertexShaderManager::SetMaterialColorChanged(int index)
{
  nMaterialsChanged[index] = true;
}

void VertexShaderManager::ScaleView(float scale)
{
  // keep the camera in the same virtual world location when scaling the virtual world
  for (int i = 0; i < 3; i++)
    s_fViewTranslationVector[i] *= scale;

  if (s_fViewTranslationVector[0] || s_fViewTranslationVector[1] || s_fViewTranslationVector[2])
    bFreeLookChanged = true;
  else
    bFreeLookChanged = false;

  bProjectionChanged = true;
}

// Moves the freelook camera a number of scaled metres relative to the current freelook camera
// direction
void VertexShaderManager::TranslateView(float left_metres, float forward_metres, float down_metres)
{
  float result[3];
  float vector[3] = {left_metres, down_metres, forward_metres};

  // use scaled metres in VR, or real metres otherwise
  if (g_has_hmd)
    for (int i = 0; i < 3; ++i)
      vector[i] *= g_ActiveConfig.fScale;

  Matrix33::Multiply(s_viewInvRotationMatrix, vector, result);

  for (size_t i = 0; i < ArraySize(result); i++)
  {
    s_fViewTranslationVector[i] += result[i];
    vr_freelook_speed += result[i];
  }

  if (s_fViewTranslationVector[0] || s_fViewTranslationVector[1] || s_fViewTranslationVector[2])
    bFreeLookChanged = true;
  else
    bFreeLookChanged = false;

  bProjectionChanged = true;
}

void VertexShaderManager::RotateView(float x, float y)
{
  s_fViewRotation[0] += x;
  s_fViewRotation[1] += y;

  Matrix33 mx;
  Matrix33 my;
  Matrix33::RotateX(mx, s_fViewRotation[1]);
  Matrix33::RotateY(my, s_fViewRotation[0]);
  Matrix33::Multiply(mx, my, s_viewRotationMatrix);

  // reverse rotation
  Matrix33::RotateX(mx, -s_fViewRotation[1]);
  Matrix33::RotateY(my, -s_fViewRotation[0]);
  Matrix33::Multiply(my, mx, s_viewInvRotationMatrix);

  if (s_fViewRotation[0] || s_fViewRotation[1])
    bFreeLookChanged = true;
  else
    bFreeLookChanged = false;

  bProjectionChanged = true;
}

void VertexShaderManager::ResetView()
{
  VRTracker::ResetView();

  memset(s_fViewTranslationVector, 0, sizeof(s_fViewTranslationVector));
  Matrix33::LoadIdentity(s_viewRotationMatrix);
  Matrix33::LoadIdentity(s_viewInvRotationMatrix);
  s_fViewRotation[0] = s_fViewRotation[1] = 0.0f;

  bFreeLookChanged = false;
  bProjectionChanged = true;
}

void VertexShaderManager::SetVertexFormat(u32 components)
{
  if (components != constants.components)
  {
    constants.components = components;
    dirty = true;
  }
}

void VertexShaderManager::SetTexMatrixInfoChanged(int index)
{
  // TODO: Should we track this with more precision, like which indices changed?
  // The whole vertex constants are probably going to be uploaded regardless.
  bTexMtxInfoChanged = true;
}

void VertexShaderManager::SetLightingConfigChanged()
{
  bLightingConfigChanged = true;
}

void VertexShaderManager::TransformToClipSpace(const float* data, float* out, u32 MtxIdx)
{
  const float* world_matrix = &xfmem.posMatrices[(MtxIdx & 0x3f) * 4];

  // We use the projection matrix calculated by VertexShaderManager, because it
  // includes any free look transformations.
  // Make sure VertexShaderManager::SetConstants() has been called first.
  const float* proj_matrix = &g_fProjectionMatrix[0];

  const float t[3] = {data[0] * world_matrix[0] + data[1] * world_matrix[1] +
                          data[2] * world_matrix[2] + world_matrix[3],
                      data[0] * world_matrix[4] + data[1] * world_matrix[5] +
                          data[2] * world_matrix[6] + world_matrix[7],
                      data[0] * world_matrix[8] + data[1] * world_matrix[9] +
                          data[2] * world_matrix[10] + world_matrix[11]};

  out[0] = t[0] * proj_matrix[0] + t[1] * proj_matrix[1] + t[2] * proj_matrix[2] + proj_matrix[3];
  out[1] = t[0] * proj_matrix[4] + t[1] * proj_matrix[5] + t[2] * proj_matrix[6] + proj_matrix[7];
  out[2] = t[0] * proj_matrix[8] + t[1] * proj_matrix[9] + t[2] * proj_matrix[10] + proj_matrix[11];
  out[3] =
      t[0] * proj_matrix[12] + t[1] * proj_matrix[13] + t[2] * proj_matrix[14] + proj_matrix[15];
}

void VertexShaderManager::DoState(PointerWrap& p)
{
  p.Do(g_fProjectionMatrix);
  p.Do(s_viewportCorrection);
  p.Do(s_viewRotationMatrix);
  p.Do(s_viewInvRotationMatrix);
  p.Do(s_fViewTranslationVector);
  p.Do(s_fViewRotation);

  p.Do(nTransformMatricesChanged);
  p.Do(nNormalMatricesChanged);
  p.Do(nPostTransformMatricesChanged);
  p.Do(nLightsChanged);

  p.Do(nMaterialsChanged);
  p.Do(bTexMatricesChanged);
  p.Do(bPosNormalMatrixChanged);
  p.Do(bProjectionChanged);
  p.Do(bViewportChanged);
  p.Do(bTexMtxInfoChanged);
  p.Do(bLightingConfigChanged);

  p.Do(constants);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    Dirty();
  }
}
