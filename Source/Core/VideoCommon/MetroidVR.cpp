// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <sstream>

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/MetroidVR.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

// helmet, combat visor, radar dot, map, scan visor, scan text, and scan hologram should be attached
// to face
// unknown, world, and reticle should NOT be attached to face

TMetroidLayer g_metroid_layer;
bool g_metroid_scan_visor = false, g_metroid_scan_visor_active = false,
     g_metroid_xray_visor = false, g_metroid_thermal_visor = false,
     g_metroid_has_thermal_effect = false, g_metroid_map_screen = false,
     g_metroid_inventory = false, g_metroid_dark_visor = false, g_metroid_echo_visor = false,
     g_metroid_morphball_active = false, g_metroid_is_demo1 = false, g_metroid_cinematic = false,
     g_metroid_menu = false, g_metroid_after_cursor = false;
int g_metroid_wide_count = 0, g_metroid_normal_count = 0, g_metroid_helmet_index = 4;
int g_zelda_world_count = 0, g_zelda_hud_count = 0;
int g_metroid_vres = 448;
bool g_zelda_hawkeye = false, g_zelda_had_unknown_effect = false;

extern float vr_widest_3d_HFOV, vr_widest_3d_VFOV, vr_widest_3d_zNear, vr_widest_3d_zFar;

//#pragma optimize("", off)

void NewMetroidFrame()
{
  g_metroid_wide_count = 0;
  g_metroid_normal_count = 0;
  g_metroid_after_cursor = false;
  g_metroid_has_thermal_effect = false;
  g_metroid_menu = false;

  g_zelda_world_count = 0;
  g_zelda_hud_count = 0;
  g_zelda_hawkeye = false;
  g_zelda_had_unknown_effect = false;
}

int Round100(float x)
{
  return (int)floor(x * 100 + 0.5f);
}

const char* MetroidLayerName(TMetroidLayer layer)
{
  switch (layer)
  {
  case METROID_BACKGROUND_3D:
    return "Background 3D";
  case METROID_CHARGE_BEAM_EFFECT:
    return "Charge Beam Effect";
  case METROID_CINEMATIC_WORLD:
    return "Cinematic World";
  case METROID_BLACK_BARS:
    return "Black Bars";
  case METROID_DARK_CENTRAL:
    return "Dark Visor Central";
  case METROID_DARK_EFFECT:
    return "Dark Visor Effect";
  case METROID_DARK_VISOR_HUD:
    return "Dark Visor HUD";
  case METROID_DIALOG:
    return "Metroid Dialog";
  case METROID_ECHO_EFFECT:
    return "Echo Visor Effect";
  case METROID_EFB_COPY:
    return "EFB Copy";
  case METROID_GUN:
    return "Gun";
  case METROID_MAP_OR_HINT:
    return "Map or Hint";
  case METROID_HELMET:
    return "Helmet";
  case METROID_HUD:
    return "HUD";
  case METROID_INVENTORY_SAMUS:
    return "Inventory Samus";
  case METROID_INVENTORY_SAMUS_OUTLINE:
    return "Inventory Samus Outline";
  case METROID_MAP:
    return "Map";
  case METROID_MAP_0:
    return "Map Screen 0";
  case METROID_MAP_1:
    return "Map Screen 1";
  case METROID_MAP_2:
    return "Map Screen 2";
  case METROID_MAP_LEGEND:
    return "Map Screen Legend";
  case METROID_MAP_MAP:
    return "Map Screen Map";
  case METROID_MAP_NORTH:
    return "Map Screen North";
  case METROID_MORPHBALL_HUD:
    return "Morphball HUD";
  case METROID_MORPHBALL_MAP:
    return "Morphball Map";
  case METROID_MORPHBALL_MAP_OR_HINT:
    return "Morphball Map or Hint";
  case METROID_MORPHBALL_WORLD:
    return "Morphball World";
  case METROID_MOUSE_POINTER:
    return "Mouse Pointer";
  case METROID_RADAR_DOT:
    return "Radar Dot";
  case METROID_RETICLE:
    return "Reticle";
  case METROID_SCAN_BOX:
    return "Scan Box";
  case METROID_SCAN_CIRCLE:
    return "Scan Circle";
  case METROID_SCAN_CROSS:
    return "Scan Cross";
  case METROID_SCAN_DARKEN:
    return "Scan Darken";
  case METROID_SCAN_HIGHLIGHTER:
    return "Scan Highlighter";
  case METROID_SCAN_HOLOGRAM:
    return "Scan Hologram";
  case METROID_SCAN_ICONS:
    return "Scan Icons";
  case METROID_VISOR_BOOTUP:
    return "Visor Boot-Up";
  case METROID_VISOR_DIRT:
    return "Visor Dirt";
  case METROID_SCAN_RETICLE:
    return "Scan Reticle";
  case METROID_SCAN_TEXT:
    return "Scan Text Box";
  case METROID_SCAN_VISOR:
    return "Scan Visor";
  case METROID_SCREEN_FADE:
    return "Screen Fade";
  case METROID_SHADOW_2D:
    return "Shadow 2D";
  case METROID_SHADOWS:
    return "World Shadows";
  case METROID_BODY_SHADOWS:
    return "Body Shadows";
  case METROID_THERMAL_EFFECT:
    return "Thermal Effect";
  case METROID_THERMAL_EFFECT_GUN:
    return "Thermal Effect Gun";
  case METROID_THERMAL_GUN_AND_DOOR:
    return "THERMAL GUN AND DOOR";
  case METROID_UNKNOWN:
    return "Unknown";
  case METROID_UNKNOWN_HUD:
    return "Unknown HUD";
  case METROID_UNKNOWN_WORLD:
    return "Unknown World";
  case METROID_UNKNOWN_VISOR:
    return "Unknown Visor";
  case METROID_VISOR:
    return "Metroid Visor";
  case METROID_VISOR_RADAR_HINT:
    return "Visor/Radar/Hint";
  case METROID_WII_RETICLE:
    return "Wii Reticle";
  case METROID_WII_WORLD:
    return "Wii World";
  case METROID_WORLD:
    return "World";
  case METROID_XRAY_EFFECT:
    return "X-Ray Effect";
  case METROID_XRAY_HUD:
    return "X-Ray HUD";
  case METROID_XRAY_WORLD:
    return "X-Ray World";
  case METROID_SCREEN_OVERLAY:
    return "Screen Overlay";
  case METROID_UNKNOWN_2D:
    return "Unknown 2D";

  case ZELDA_BACKGROUND:
    return "Zelda Background";
  case ZELDA_CREATE_MAP:
    return "Zelda Create Map";
  case ZELDA_CREATE_SHADOW:
    return "Zelda Create Shadow";
  case ZELDA_DARK_EFFECT:
    return "Zelda Dark Effect";
  case ZELDA_DIALOG:
    return "Zelda Dialog";
  case ZELDA_FISHING:
    return "Zelda Fishing";
  case ZELDA_HAWKEYE:
    return "Zelda Hawkeye";
  case ZELDA_HAWKEYE_HUD:
    return "Zelda Hawkeye HUD";
  case ZELDA_NEXT:
    return "Zelda Next";
  case ZELDA_REFLECTION:
    return "Zelda Water Reflection";
  case ZELDA_UNKNOWN:
    return "Zelda Unknown";
  case ZELDA_UNKNOWN_2D:
    return "Zelda Unknown 2D";
  case ZELDA_UNKNOWN_EFFECT:
    return "Zelda Unknown Effect";
  case ZELDA_LINK:
    return "Zelda Link";
  case ZELDA_WORLD:
    return "Zelda World";
  case NES_RENDER_TO_TEXTURE:
    return "NES Render To Texture";
  case NES_SHOW_TEXTURE:
    return "NES Show Texture";
  case NES_WII_GUI_BACKGROUND:
    return "NES Wii Gui Background";
  case NES_WII_GUI_ERROR:
    return "NES Wii Gui Error";
  case NES_WII_GUI_MENU:
    return "NES Wii Gui Menu";
  case NES_UNKNOWN_2D:
    return "NES Unknown 2D";
  default:
    return "Error";
  }
}

TMetroidLayer GetMetroidPrime1GCLayer2D(int layer, float left, float right, float top, float bottom,
                                        float znear, float zfar)
{
  if (layer == 0)
  {
    g_metroid_xray_visor = false;
  }

  TMetroidLayer result;
  int l = Round100(left);
  int t = Round100(top);
  int b = Round100(bottom);
  int r = Round100(right);
  int n = Round100(znear);
  int f = Round100(zfar);
  if (abs(t) == 44800 || abs(b) == 44800)
    g_metroid_vres = 448;
  else if (abs(t) == 52800 || abs(b) == 52800)
    g_metroid_vres = 528;
  if (layer == 0 && (l == -88 || l == -87) && n == 0)
  {
    result = METROID_SHADOW_2D;
    g_metroid_morphball_active = true;
    g_metroid_thermal_visor = false;
  }
  else if (layer == 0 && g_metroid_map_screen && l == 0 && n == -100)
  {
    result = METROID_MAP_0;
    g_metroid_morphball_active = false;
  }
  else if (layer == 0 && l == 0 && (t == 44800 || t == 52800) && n == -409600 && f == 409600)
  {
    // actually it is the menu background
    result = METROID_UNKNOWN_2D;
    g_metroid_menu = true;
  }
  else if (layer == 1)
  {
    g_metroid_dark_visor = false;
    if (l == 0 && n == -100 && f == 100 && g_metroid_xray_visor)
    {
      result = METROID_XRAY_EFFECT;
      g_metroid_dark_visor = false;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
      g_metroid_thermal_visor = false;
    }
    else if (l == -32000 && n == -409600 && g_metroid_xray_visor)
    {
      result = METROID_VISOR_DIRT;
      g_metroid_map_screen = false;
      g_metroid_thermal_visor = false;
    }
    else if ((l == -88 || l == -87) && n == 0)
    {
      result = METROID_SHADOW_2D;
      g_metroid_morphball_active = true;
      g_metroid_thermal_visor = false;
    }
    else if (l == -32000 && n == -409600)
    {
      result = METROID_MAP_1;
      g_metroid_map_screen = true;
      g_metroid_thermal_visor = false;
      g_metroid_morphball_active = false;
    }
    else if (l == 0 && t == 0 && n == -409600)
    {
      g_metroid_scan_visor = false;
      g_metroid_has_thermal_effect = true;
      if (g_metroid_thermal_visor)
      {
        result = METROID_THERMAL_EFFECT;
      }
      else
      {
        result = METROID_CHARGE_BEAM_EFFECT;
      }
    }
    else if (l == -1570)
    {
      // actually we know it is the menu's text
      result = METROID_UNKNOWN_2D;
      g_metroid_menu = true;
      g_metroid_scan_visor = false;
      g_metroid_thermal_visor = false;
      g_metroid_xray_visor = false;
      g_metroid_morphball_active = false;
      g_metroid_inventory = false;
      g_metroid_map_screen = false;
      g_metroid_cinematic = false;
    }
    else
    {
      g_metroid_scan_visor = false;
      if (l == -105)
        result = METROID_SHADOW_2D;
      else
        result = METROID_UNKNOWN_2D;
    }
  }
  else if (layer == 2)
  {
    if (l == 0 && n == -100 && f == 100 && g_metroid_xray_visor)
    {
      result = METROID_XRAY_EFFECT;
      g_metroid_dark_visor = false;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
      g_metroid_thermal_visor = false;
      g_metroid_has_thermal_effect = false;
    }
    else if (l == 0 && t == 0 && r == 64000 && n == -409600)
    {
      g_metroid_has_thermal_effect = true;
      if (g_metroid_thermal_visor)
      {
        result = METROID_THERMAL_EFFECT;
        g_metroid_thermal_visor = true;
      }
      else
      {
        result = METROID_CHARGE_BEAM_EFFECT;
      }
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
      g_metroid_xray_visor = false;
    }
    else if (l == -32000 && n == -409600 && g_metroid_map_screen)
    {
      result = METROID_MAP_2;
      g_metroid_xray_visor = false;
      g_metroid_thermal_visor = false;
    }
    else if (l == -32000 && t == 22400 && n == -409600 && f == 409600 && g_metroid_cinematic)
    {
      result = METROID_BLACK_BARS;
      g_metroid_thermal_visor = false;
    }
    else if (l == -32000 && n == -409600 && f == 409600)
    {
      result = METROID_VISOR_DIRT;
    }
    else if (l == -320)
    {
      // This happens when transitioning from Morphball to Combat visor,
      // because the shadows are no longer rendered it ends up as an earlier layer
      result = METROID_MORPHBALL_HUD;
      g_metroid_morphball_active = true;
      g_metroid_thermal_visor = false;
    }
    else
    {
      result = METROID_UNKNOWN_2D;
      g_metroid_dark_visor = false;
      g_metroid_xray_visor = false;
      g_metroid_thermal_visor = false;
    }
  }
  else if (l == -32000 && t == 22400 && n == -409600 && f == 409600 && g_metroid_cinematic)
  {
    result = METROID_BLACK_BARS;
    g_metroid_thermal_visor = false;
  }
  else if (l == -32000 && n == -409600 && f == 409600)
  {
    g_metroid_thermal_visor = false;
    if (g_metroid_map_screen)
      result = METROID_MAP_NORTH;
    else if (layer == 4 && !g_metroid_scan_visor)
      result = METROID_BLACK_BARS;  // could also be METROID_VISOR_DIRT
    else
      result = METROID_SCAN_DARKEN;
  }
  else if (l == -32000 && n == -100 && f == 100)
  {
    result = METROID_SCAN_BOX;
    g_metroid_thermal_visor = false;
  }
  else if (l == -320)
  {
    result = METROID_MORPHBALL_HUD;
    g_metroid_morphball_active = true;
    g_metroid_thermal_visor = false;
  }
  else if (l == 0 && t == 0 && n == -409600 && g_metroid_has_thermal_effect)
  {
    result = METROID_THERMAL_EFFECT_GUN;
    g_metroid_thermal_visor = true;
    g_metroid_dark_visor = false;
    g_metroid_scan_visor = false;
    g_metroid_scan_visor_active = false;
    g_metroid_morphball_active = false;
    g_metroid_xray_visor = false;
  }
  else if (l == 0 && t == 0 && r == 64000 && n == -409600)
  {
    g_metroid_scan_visor = false;
    g_metroid_has_thermal_effect = true;
    if (g_metroid_thermal_visor)
    {
      result = METROID_THERMAL_EFFECT;
    }
    else
    {
      // On the PAL version, which renders shadows every frame, this shows up as layer 4
      result = METROID_CHARGE_BEAM_EFFECT;
    }
  }
  else if (g_metroid_inventory && l == 0 && r == 100 && t == 100 && n == -100 && f == 100)
  {
    result = METROID_INVENTORY_SAMUS_OUTLINE;
  }
  else
  {
    result = METROID_UNKNOWN_2D;
  }
  return result;
}

TMetroidLayer GetMetroidPrime1GCLayer(int layer, float hfov, float vfov, float znear, float zfar)
{
  if (layer == 0)
  {
    g_metroid_xray_visor = false;
  }

  int h = Round100(hfov);
  int v = Round100(vfov);
  //	int n = Round100(znear);
  int f = Round100(zfar);
  TMetroidLayer result;

  if (layer == 2)
  {
    // would be a 2D layer if it was still the map screen.
    g_metroid_map_screen = false;
  }

  if (v == h && v < 100 && h < 100 && layer == 0)
  {
    result = METROID_SHADOWS;
  }
  else if (v == h && v == 5500 && (layer == 1 || layer == 2))
  {
    result = METROID_BODY_SHADOWS;
  }
  else if (f == 300 && v == 5500)
  {
    g_metroid_is_demo1 = true;
    result = METROID_GUN;
  }
  else if (f == 409600 && v == 6500)
  {
    g_metroid_cinematic = false;
    if (h == 8055)
    {
      g_metroid_morphball_active = false;
      g_metroid_map_screen = false;
      g_metroid_inventory = false;
      if (g_metroid_xray_visor || g_metroid_thermal_visor)
      {
        result = METROID_XRAY_HUD;
      }
      else
      {
        // This layer is not always present when scanning.
        // Only when the text box is visible. It also includes
        // the data boxes on the sides when you scan your ship.
        result = METROID_SCAN_TEXT;
      }
    }
    else
    {
      // thermal visor normally has first helmet/visor layer at 6, but use 5 to be extra careful
      if (layer < 5 || !g_metroid_has_thermal_effect)
        g_metroid_thermal_visor = false;
      ++g_metroid_wide_count;
      switch (g_metroid_wide_count)
      {
      case 1:
        if (g_metroid_map_screen || g_metroid_inventory)
        {
          if (g_metroid_inventory)
          {
            g_metroid_map_screen = false;
            result = METROID_HELMET;
          }
          else
          {
            g_metroid_map_screen = true;
            result = METROID_MAP_MAP;
          }
        }
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
        {
          result = METROID_VISOR_RADAR_HINT;
        }
        else if (g_metroid_is_demo1)
        {
          if (g_metroid_morphball_active)
            result = METROID_MORPHBALL_MAP_OR_HINT;
          else
            result = METROID_HELMET;
        }
        else
        {
          result = METROID_HUD;
        }
        break;
      case 2:
        if (g_metroid_morphball_active)
          result = METROID_MORPHBALL_MAP_OR_HINT;
        else if (g_metroid_map_screen)
          result = METROID_HELMET;
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
          result = METROID_RADAR_DOT;
        else if (g_metroid_dark_visor)
          result = METROID_HELMET;
        else if (g_metroid_is_demo1)
          result = METROID_HUD;
        else
          result = METROID_VISOR_RADAR_HINT;
        break;
      case 3:
        // visor
        if (g_metroid_morphball_active)
        {
          result = METROID_MORPHBALL_MAP;
          g_metroid_scan_visor_active = false;
        }
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
        {
          result = METROID_MAP;
          g_metroid_scan_visor_active = false;
        }
        else if (g_metroid_dark_visor)
        {
          result = METROID_DARK_VISOR_HUD;
          g_metroid_scan_visor_active = false;
        }
        else if (g_metroid_is_demo1)
        {
          result = METROID_VISOR_RADAR_HINT;
        }
        else if (h == 8243)
        {
          result = METROID_RADAR_DOT;
          g_metroid_scan_visor_active = false;
        }
        else
        {
          result = METROID_UNKNOWN_VISOR;
          g_metroid_scan_visor_active = false;
        }
        break;
      case 4:
        if (g_metroid_morphball_active)
          result = METROID_MORPHBALL_MAP;
        else if (g_metroid_scan_visor_active && h != 8243)
          result = METROID_SCAN_HOLOGRAM;
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
          result = METROID_HELMET;
        else if (g_metroid_dark_visor || g_metroid_is_demo1)
          result = METROID_RADAR_DOT;
        else if (g_metroid_scan_visor_active)
          result = METROID_UNKNOWN_HUD;
        else if (h == 8243)
          result = METROID_MAP;
        else
          result = METROID_UNKNOWN_HUD;
        break;
      case 5:
        if (g_metroid_dark_visor)
          result = METROID_MAP_OR_HINT;
        else if (g_metroid_is_demo1)
          result = METROID_MAP;
        else if (h == 8243 && !g_metroid_scan_visor_active)
          result = METROID_HELMET;
        else
          result = METROID_UNKNOWN_HUD;
        break;
      case 6:
        if (g_metroid_dark_visor)
          result = METROID_MAP;
        else
          result = METROID_UNKNOWN_HUD;
        break;
      default:
        result = METROID_UNKNOWN_HUD;
        break;
      }
    }
  }
  else if (f == 409600 && v == 5500)
  {
    g_metroid_thermal_visor = false;
    if ((g_metroid_map_screen || g_metroid_inventory) && h == 7327)
    {
      result = METROID_INVENTORY_SAMUS;
      g_metroid_map_screen = false;
      g_metroid_inventory = true;
    }
    else
    {
      result = METROID_SCAN_HOLOGRAM;
      g_metroid_scan_visor_active = true;
    }
  }
  else if (f == 409600 && v == 5443)
  {
    if (layer != 3 && !g_metroid_menu)
    {
      result = METROID_MAP_LEGEND;
      g_metroid_map_screen = true;
      g_metroid_thermal_visor = false;
    }
    else
    {
      // this is the main menu's memory card notification and corners
      // or the question when you enter your ship
      result = METROID_DIALOG;
    }
  }
  else if (f == 75000 && (v == 4958 || v == 4522))
  {
    result = METROID_MORPHBALL_WORLD;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = true;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
    g_metroid_thermal_visor = false;
  }
  else if (f == 75000 && v == 5021)
  {
    result = METROID_XRAY_WORLD;
    g_metroid_xray_visor = true;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = false;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_thermal_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  // 73.27 = GameCube, 85.64 = Wii (widescreen)
  else if (f == 75000 && (h == 7327 || h == 8564))
  {
    g_metroid_cinematic = false;
    ++g_metroid_normal_count;
    g_metroid_map_screen = false;
    g_metroid_inventory = false;
    switch (g_metroid_normal_count)
    {
    case 1:
      if (g_metroid_xray_visor)
      {
        result = METROID_RETICLE;
        g_metroid_morphball_active = false;
        if (layer > 1)
          g_metroid_scan_visor = false;
      }
      else
      {
        result = METROID_WORLD;
        vr_widest_3d_HFOV = abs(hfov);
        vr_widest_3d_VFOV = abs(vfov);
        vr_widest_3d_zNear = znear;
        vr_widest_3d_zFar = zfar;
        g_metroid_morphball_active = false;
        if (layer > 1)
          g_metroid_scan_visor = false;
      }
      break;
    case 2:
      if (g_metroid_thermal_visor)
        result = METROID_THERMAL_GUN_AND_DOOR;
      else if (g_metroid_scan_visor)
        result = METROID_SCAN_ICONS;
      else
        result = METROID_RETICLE;
      break;
    case 3:
      if (g_metroid_thermal_visor)
      {
        result = METROID_RETICLE;
      }
      if (g_metroid_xray_visor)
      {
        result = METROID_UNKNOWN_HUD;
      }
      else
      {
        result = METROID_SCAN_CROSS;
        g_metroid_scan_visor = true;
      }
      break;
    default:
      result = METROID_UNKNOWN_WORLD;
      break;
    }
  }
  else if (f == 75000 && h == 6594)
  {
    result = METROID_WII_WORLD;
  }
  else if (g_metroid_morphball_active && v >= 4958 && v < 5500)
  {
    result = METROID_MORPHBALL_WORLD;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = true;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
    g_metroid_thermal_visor = false;
  }
  else
  {
    g_metroid_cinematic = true;
    g_metroid_thermal_visor = false;
    result = METROID_CINEMATIC_WORLD;
  }
  return result;
}

TMetroidLayer GetMetroidPrime1WiiLayer2D(int layer, float left, float right, float top,
                                         float bottom, float znear, float zfar)
{
  if (layer == 0)
  {
    g_metroid_xray_visor = false;
    g_metroid_thermal_visor = false;
  }

  TMetroidLayer result;
  int l = Round100(left);
  int r = Round100(right);
  int t = Round100(top);
  int b = Round100(bottom);
  int n = Round100(znear);
  int f = Round100(zfar);
  if (abs(t) == 44800 || abs(b) == 44800)
    g_metroid_vres = 448;
  else if (abs(t) == 52800 || abs(b) == 52800)
    g_metroid_vres = 528;
  if (layer == 0 && g_metroid_map_screen && l == 0 && n == -100)
  {
    result = METROID_MAP_0;
  }
  else if (layer == 1)
  {
    g_metroid_dark_visor = false;
    if (l == 0 && n == -100 && f == 100 && g_metroid_xray_visor)
    {
      result = METROID_XRAY_EFFECT;
      g_metroid_dark_visor = false;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
    }
    else if (l == -32000 && n == -409600 && g_metroid_xray_visor)
    {
      result = METROID_VISOR_DIRT;
      g_metroid_map_screen = false;
    }
    else if (l == -88 && n == 0)
    {
      result = METROID_SHADOW_2D;
      g_metroid_morphball_active = true;
    }
    else if (l == -32000 && t == 22400 && n == -409600 && f == 409600)
    {
      result = METROID_EFB_COPY;
    }
    else if (l == 0 && t == 0 && n == -409600)
    {
      result = METROID_THERMAL_EFFECT;
      g_metroid_thermal_visor = true;
      g_metroid_scan_visor = false;
    }
    else
    {
      g_metroid_scan_visor = false;
      g_metroid_echo_visor = false;
      if (l == -105)
        result = METROID_SHADOW_2D;
      else
        result = METROID_UNKNOWN_2D;
    }
  }
  else if (layer == 2)
  {
    if (l == 0 && n == -100 && f == 100 && g_metroid_xray_visor)
    {
      result = METROID_XRAY_EFFECT;
      g_metroid_dark_visor = false;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
      g_metroid_thermal_visor = false;
    }
    else if (l == 0 && t == 0 && r == 64000 && n == -409600)
    {
      result = METROID_THERMAL_EFFECT;
      g_metroid_thermal_visor = true;
      g_metroid_dark_visor = false;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
      g_metroid_xray_visor = false;
    }
    else if (l == -32000 && n == -409600 && g_metroid_map_screen)
    {
      result = METROID_MAP_2;
      g_metroid_xray_visor = false;
    }
    else if (l == -32000 && t == 22400 && n == -409600 && f == 409600)
    {
      result = METROID_EFB_COPY;
    }
    else
    {
      result = METROID_UNKNOWN_2D;
      g_metroid_dark_visor = false;
      g_metroid_xray_visor = false;
    }
  }
  else if (l == -32000 && t == 22400 && n == -409600 && f == 409600 && g_metroid_cinematic)
  {
    result = METROID_EFB_COPY;
  }
  else if (l == -320)
  {
    result = METROID_MORPHBALL_HUD;
    g_metroid_morphball_active = true;
  }
  else if (l == -32000 && (t == 22400 || t == 26400) && n == -409600 && f == 409600)
  {
    if (g_metroid_map_screen)
      result = METROID_MAP_NORTH;
    else if ((layer == 3 || layer == 4) && !g_metroid_scan_visor)
      result = METROID_EFB_COPY;
    else
      result = METROID_SCAN_CIRCLE;
  }
  else if (l == -32000 && n == -100 && f == 100)
  {
    result = METROID_SCAN_BOX;
  }
  else if (l == 0 && n == -409600 && g_metroid_thermal_visor)
  {
    result = METROID_THERMAL_EFFECT_GUN;
    g_metroid_dark_visor = false;
    g_metroid_scan_visor = false;
    g_metroid_scan_visor_active = false;
    g_metroid_morphball_active = false;
    g_metroid_xray_visor = false;
  }
  else
    result = METROID_UNKNOWN_2D;
  return result;
}

TMetroidLayer GetMetroidPrime1WiiLayer(int layer, float hfov, float vfov, float znear, float zfar)
{
  if (layer == 0)
  {
    g_metroid_xray_visor = false;
    g_metroid_thermal_visor = false;
  }

  int h = Round100(hfov);
  int v = Round100(vfov);
  int a = Round100(hfov / vfov);
  //	int n = Round100(znear);
  int f = Round100(zfar);
  TMetroidLayer result;

  if (layer == 2)
  {
    // would be a 2D layer if it was still the map screen.
    g_metroid_map_screen = false;
  }

  if ((v == h || a == 125) && v < 100 && h < 100 && layer == 0)
  {
    result = METROID_SHADOWS;
  }
  else if (v == h && v == 5500 && layer == 1)
  {
    result = METROID_BODY_SHADOWS;
  }
  else if (f == 300 && v == 5500)
  {
    g_metroid_is_demo1 = true;
    result = METROID_GUN;
  }
  else if (f == 409600 && v == 6500)
  {
    g_metroid_cinematic = false;
    if (h == 8055)
    {
      g_metroid_morphball_active = false;
      g_metroid_map_screen = false;
      g_metroid_inventory = false;
      if (g_metroid_xray_visor || g_metroid_thermal_visor)
      {
        result = METROID_XRAY_HUD;
      }
      else
      {
        // This layer is not always present when scanning.
        // Only when the text box is visible. It also includes
        // the data boxes on the sides when you scan your ship.
        result = METROID_SCAN_TEXT;
      }
    }
    else
    {
      ++g_metroid_wide_count;
      switch (g_metroid_wide_count)
      {
      case 1:
        if (g_metroid_map_screen || g_metroid_inventory)
        {
          if (g_metroid_inventory)
          {
            g_metroid_map_screen = false;
            result = METROID_HELMET;
          }
          else
          {
            g_metroid_map_screen = true;
            result = METROID_MAP_MAP;
          }
        }
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
        {
          result = METROID_VISOR_RADAR_HINT;
        }
        else
        {
          result = METROID_HUD;
        }
        break;
      case 2:
        if (g_metroid_morphball_active)
          result = METROID_MORPHBALL_MAP_OR_HINT;
        else if (g_metroid_map_screen)
          result = METROID_HELMET;
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
          result = METROID_RADAR_DOT;
        else if (g_metroid_dark_visor)
          result = METROID_HELMET;
        else if (g_metroid_is_demo1)
        {
          result = METROID_HUD;
          g_metroid_morphball_active = false;
        }
        else
          result = METROID_VISOR_RADAR_HINT;
        break;
      case 3:
        // visor
        if (g_metroid_morphball_active)
        {
          result = METROID_MORPHBALL_MAP;
          g_metroid_scan_visor_active = false;
        }
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
        {
          result = METROID_MAP;
          g_metroid_scan_visor_active = false;
        }
        else if (h == 8243)
        {
          if (g_metroid_scan_visor)
            result = METROID_VISOR;
          else
            result = METROID_RADAR_DOT;
        }
        else
        {
          result = METROID_UNKNOWN_VISOR;
          g_metroid_scan_visor_active = false;
        }
        break;
      case 4:
        if (g_metroid_morphball_active)
          result = METROID_MORPHBALL_MAP;
        else if (g_metroid_scan_visor_active && h != 8243)
          result = METROID_SCAN_HOLOGRAM;
        else if (g_metroid_xray_visor || g_metroid_thermal_visor)
          result = METROID_HELMET;
        else if (g_metroid_scan_visor_active)
          result = METROID_UNKNOWN_HUD;
        else if (h == 8243)
          result = METROID_MAP;
        else
          result = METROID_UNKNOWN_HUD;
        break;
      case 5:
        if (g_metroid_dark_visor)
          result = METROID_MAP_OR_HINT;
        else if (g_metroid_is_demo1)
          result = METROID_MAP;
        else if (h == 8243 && !g_metroid_scan_visor_active)
          result = METROID_HELMET;
        else
          result = METROID_UNKNOWN_HUD;
        break;
      case 6:
        if (g_metroid_dark_visor)
          result = METROID_MAP;
        else
          result = METROID_UNKNOWN_HUD;
        break;
      default:
        result = METROID_UNKNOWN_HUD;
        break;
      }
    }
  }
  else if (f == 409600 && v == 5500)
  {
    if ((g_metroid_map_screen || g_metroid_inventory) && (h == 7327 || h == 8564))
    {
      result = METROID_INVENTORY_SAMUS;
      g_metroid_map_screen = false;
      g_metroid_inventory = true;
    }
    else
    {
      result = METROID_SCAN_HOLOGRAM;
      g_metroid_scan_visor_active = true;
    }
  }
  else if (f == 409600 && v == 5443)
  {
    result = METROID_MAP_LEGEND;
    g_metroid_map_screen = true;
  }
  else if (f == 75000 && (v == 4958 || v == 4522))
  {
    result = METROID_MORPHBALL_WORLD;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = true;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  else if (f == 75000 && v == 5021)
  {
    result = METROID_XRAY_WORLD;
    g_metroid_xray_visor = true;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = false;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  // 73.27 = GameCube, 85.64 = Wii (widescreen)
  else if (f == 75000 && (h == 7327 || h == 8564))
  {
    g_metroid_cinematic = false;
    ++g_metroid_normal_count;
    g_metroid_map_screen = false;
    g_metroid_inventory = false;
    switch (g_metroid_normal_count)
    {
    case 1:
      if (g_metroid_xray_visor)
      {
        if (h == 8564)
          result = METROID_WII_RETICLE;
        else
          result = METROID_RETICLE;
        g_metroid_morphball_active = false;
        if (layer > 1)
          g_metroid_scan_visor = false;
      }
      else
      {
        result = METROID_WORLD;
        vr_widest_3d_HFOV = abs(hfov);
        vr_widest_3d_VFOV = abs(vfov);
        vr_widest_3d_zNear = znear;
        vr_widest_3d_zFar = zfar;
        g_metroid_morphball_active = false;
        if (layer > 1)
          g_metroid_scan_visor = false;
      }
      break;
    case 2:
      if (g_metroid_thermal_visor)
        result = METROID_THERMAL_GUN_AND_DOOR;
      else if (g_metroid_scan_visor)
        result = METROID_SCAN_ICONS;
      else if (h == 8564)
        result = METROID_WII_RETICLE;
      else
        result = METROID_RETICLE;
      break;
    case 3:
      if (g_metroid_thermal_visor)
      {
        if (h == 8564)
          result = METROID_WII_RETICLE;
        else
          result = METROID_RETICLE;
      }
      if (g_metroid_xray_visor)
      {
        result = METROID_UNKNOWN_HUD;
      }
      else
      {
        result = METROID_SCAN_CROSS;
        g_metroid_scan_visor = true;
      }
      break;
    default:
      result = METROID_UNKNOWN_WORLD;
      break;
    }
  }
  else if (f == 75000 && h == 6594)
  {
    result = METROID_WII_WORLD;
  }
  else
  {
    g_metroid_cinematic = true;
    result = METROID_CINEMATIC_WORLD;
  }
  return result;
}

TMetroidLayer GetMetroidPrime2GCLayer2D(int layer, float left, float right, float top, float bottom,
                                        float znear, float zfar)
{
  TMetroidLayer result;
  int l = Round100(left);
  int r = Round100(right);
  int t = Round100(top);
  int b = Round100(bottom);
  int n = Round100(znear);
  int f = Round100(zfar);
  if (abs(t) == 44800 || abs(b) == 44800)
    g_metroid_vres = 448;
  else if (abs(t) == 52800 || abs(b) == 52800)
    g_metroid_vres = 528;
  if (layer == 0)
  {
    g_metroid_dark_visor = false;
    if (l == 0 && r == 32000 && (t == 22400 || t == 26400) && n == -100 && f == 100)
    {
      g_metroid_map_screen = true;
      g_metroid_morphball_active = false;
      g_metroid_scan_visor = false;
      g_metroid_echo_visor = false;
      result = METROID_MAP_0;
    }
    else if ((l == -75 || l == -76) && n == 0)
    {
      // 0: 2D: Unknown (-0.758217, 0.902008) to (0.758217, -0.902008); z: -0 to 7.67479
      // [-0.130297, -1]
      result = METROID_SHADOW_2D;
    }
    else
    {
      result = METROID_UNKNOWN_2D;
    }
  }
  else if (layer == 1)
  {
    g_metroid_dark_visor = false;
    if (l == -32000 && n == -409600)
    {
      if (g_metroid_map_screen)
      {
        result = METROID_MAP_1;
        g_metroid_scan_visor = false;
        g_metroid_echo_visor = false;
        g_metroid_morphball_active = false;
      }
      else if (g_metroid_cinematic)
      {
        result = METROID_BLACK_BARS;
        g_metroid_scan_visor = false;
        g_metroid_scan_visor_active = false;
      }
      else
      {
        result = METROID_SCAN_HIGHLIGHTER;
        g_metroid_scan_visor = true;
        g_metroid_echo_visor = false;
      }
    }
    else if (l == 0 && n == -409600)
    {
      result = METROID_ECHO_EFFECT;
      g_metroid_echo_visor = true;
      g_metroid_scan_visor = false;
    }
    else if ((l == -75 || l == -76) && n == 0)
    {
      result = METROID_SHADOW_2D;
      g_metroid_scan_visor = false;
    }
    else if (l == -3200 && t == 3200)
    {
      result = METROID_SHADOW_2D;
      g_metroid_scan_visor = false;
    }
    else
    {
      g_metroid_scan_visor = false;
      g_metroid_echo_visor = false;
      if (l == -105)
        result = METROID_SHADOW_2D;
      else
        result = METROID_UNKNOWN_2D;
    }
  }
  else if (layer == 2)
  {
    if (g_metroid_cinematic && l == 0 && n == -409600)
    {
      result = METROID_UNKNOWN_2D;
      g_metroid_dark_visor = false;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
    }
    else if (l == 0 && n == -409600)
    {
      result = METROID_DARK_EFFECT;
      g_metroid_dark_visor = true;
      g_metroid_scan_visor = false;
      g_metroid_scan_visor_active = false;
      g_metroid_morphball_active = false;
    }
    else if (l == -32000 && (t == 22400 || t == 26400) && n == -409600 && f == 409600 &&
             g_metroid_cinematic)
    {
      result = METROID_BLACK_BARS;
      g_metroid_thermal_visor = false;
    }
    else if (l == -32000 && t >= 18000 && t <= 26400 && n == -409600 && f == 409600 &&
             g_metroid_cinematic)
    {
      result = METROID_SCREEN_OVERLAY;
    }
    else if (l == -32000 && (t == 22400 || t == 26400) && n == -409600)
    {
      if (g_metroid_scan_visor)
      {
        result = METROID_SCREEN_OVERLAY;
      }
      else
      {
        result = METROID_MAP_2;
        g_metroid_dark_visor = false;
        g_metroid_morphball_active = false;
      }
    }
    else if (l == -3200 && t == 3200)
    {
      result = METROID_SHADOW_2D;
    }
    else
    {
      result = METROID_UNKNOWN_2D;
      g_metroid_dark_visor = false;
    }
  }
  else if (layer == 3)
  {
    if (l == -32000 && (t == 22400 || t == 26400) && n == -409600 && f == 409600 &&
        g_metroid_cinematic)
    {
      result = METROID_BLACK_BARS;
      g_metroid_thermal_visor = false;
    }
    else if (l == -32000 && t >= 18000 && t <= 26400 && n == -409600 && f == 409600 &&
             g_metroid_cinematic)
    {
      result = METROID_SCREEN_OVERLAY;
    }
    else if (l == -32000 && n == -409600)
    {
      result = METROID_SCAN_DARKEN;
    }
    else if (l == 0 && n == -409600)
    {
      if (g_metroid_cinematic)
      {
        result = METROID_UNKNOWN_2D;  // intro cinematic FMV
        g_metroid_dark_visor = false;
        g_metroid_scan_visor = false;
        g_metroid_scan_visor_active = false;
        g_metroid_morphball_active = false;
      }
      else
      {
        result = METROID_DARK_EFFECT;
        g_metroid_dark_visor = true;
        g_metroid_scan_visor = false;
        g_metroid_scan_visor_active = false;
        g_metroid_morphball_active = false;
        g_metroid_cinematic = false;
      }
    }
    else if (l == 0 && (t == 22400 || t == 26400) && r == 32000 && b == 0 && n == -100 && f == 100)
    {
      // 3: 2D : Unknown 2D (-0, 224) to(320, -0); z: -1 to 1[-0.5, -0.5]
      result = METROID_VISOR_BOOTUP;
    }
    else
    {
      result = METROID_UNKNOWN_2D;
    }
  }
  else if ((layer == 4 || layer == 5 || layer == 6) && l == -32000 && n == -100 && f == 100)
    result = METROID_SCAN_BOX;
  else if (layer == 5 && l == -32000 && (t == 22400 || t == 26400) && n == -409600 &&
           g_metroid_map_screen)
    result = METROID_MAP_NORTH;
  else if (l == -32000 && (t == 22400 || t == 26400) && n == -409600 && f == 409600 &&
           g_metroid_cinematic)
  {
    result = METROID_BLACK_BARS;
  }
  else if ((layer == 4 || (layer == 5 && g_metroid_normal_count == 2)) && l == -32000 &&
           (t == 22400 || t == 26400) && n == -409600 && f == 409600)
  {
    // while actively scanning in the fog
    result = METROID_SCAN_DARKEN;
  }
  else if (l == -32000 && t >= 18000 && t <= 26400 && n == -409600 && f == 409600 &&
           g_metroid_cinematic)
  {
    // NTSC: 4: 2D : Unknown 2D (-320, 180) to(320, -180); z: -4096 to 4096[-0.00012207, -0.5]
    // untested on PAL
    // This is used in the cinematic near the start when she jumps in the hole
    result = METROID_SCREEN_OVERLAY;
  }
  else if ((layer == 6 || layer == 7) && l == -32000 && (t == 22400 || t == 26400) &&
           n == -409600 && f == 409600)
  {
    // 7: 2D : Unknown 2D (-320, 224) to(320, -224); z: -4096 to 4096[-0.00012207, -0.5]
    // this is actually the round red damage indicators that show which direction you are being hurt
    // from.
    // Unfortunately, I can't think of a good way to incorporate head turn.
    result = METROID_SCREEN_OVERLAY;
  }
  else
    result = METROID_UNKNOWN_2D;
  return result;
}

TMetroidLayer GetMetroidPrime2GCLayer(int layer, float hfov, float vfov, float znear, float zfar)
{
  int h = Round100(hfov);
  int v = Round100(vfov);
  //	int n = Round100(znear);
  int f = Round100(zfar);
  TMetroidLayer result;
  // In Dark Visor layer 2 would be 2D not 3D
  // (not necessarily, when I tested I found layer 3 was the 2D dark visor)
  // if (layer == 2)
  //	g_metroid_dark_visor = false;

  if (layer == 1)
    g_metroid_scan_visor = false;

  if (v == h && layer <= 1)
  {
    // 1: Unknown 2D HFOV: 1.24deg; VFOV: 1.24deg; Aspect Ratio: 16:16.0; near:0.100000,
    // far:1000.000000
    result = METROID_SHADOWS;
  }
  else if (f == 409600 && v == 6500)
  {
    ++g_metroid_wide_count;
    switch (g_metroid_wide_count)
    {
    case 1:
      if (g_metroid_morphball_active)
        result = METROID_MORPHBALL_HUD;
      else if (g_metroid_map_screen)
        result = METROID_MAP_MAP;
      else if (g_metroid_dark_visor && (layer == 1 || layer == 2))
        result = METROID_DARK_CENTRAL;
      else
        result = METROID_HELMET;
      break;
    case 2:
      g_metroid_inventory = false;
      if (g_metroid_morphball_active)
        result = METROID_MORPHBALL_MAP_OR_HINT;
      else if (g_metroid_map_screen)
        result = METROID_MAP_LEGEND;
      else if (g_metroid_dark_visor)
        result = METROID_HELMET;
      else
        result = METROID_HUD;
      break;
    case 3:
      g_metroid_map_screen = false;
      // visor
      if (g_metroid_morphball_active)
      {
        result = METROID_MORPHBALL_MAP;
        g_metroid_scan_visor_active = false;
      }
      else if (g_metroid_dark_visor)
      {
        result = METROID_DARK_VISOR_HUD;
        g_metroid_scan_visor_active = false;
      }
      else if (h == 8243)
      {
        result = METROID_RADAR_DOT;
        g_metroid_scan_visor_active = false;
        g_metroid_scan_visor = false;
      }
      else if (h == 8055)
      {
        // This layer is not always present when scanning.
        // Only when the text box is visible. It also includes
        // the data boxes on the sides when you scan your ship.
        result = METROID_SCAN_TEXT;
        g_metroid_scan_visor_active = true;
        g_metroid_morphball_active = false;
      }
      else
      {
        result = METROID_UNKNOWN_VISOR;
        g_metroid_scan_visor_active = false;
      }
      break;
    case 4:
      if (g_metroid_morphball_active)
        result = METROID_MORPHBALL_MAP;
      else if (g_metroid_scan_visor_active && h != 8243)
        result = METROID_SCAN_HOLOGRAM;
      else if (g_metroid_dark_visor)
        result = METROID_RADAR_DOT;
      else if (g_metroid_scan_visor_active)
        result = METROID_UNKNOWN_HUD;
      else if (h == 8243)
        result = METROID_MAP_OR_HINT;
      else
        result = METROID_UNKNOWN_HUD;
      break;
    case 5:
      if (g_metroid_dark_visor)
        result = METROID_MAP_OR_HINT;
      else if (h == 8243 && !g_metroid_scan_visor_active)
        result = METROID_MAP;
      else
        result = METROID_UNKNOWN_HUD;
      break;
    case 6:
      if (g_metroid_dark_visor)
        result = METROID_MAP;
      else
        result = METROID_UNKNOWN_HUD;
      break;
    default:
      result = METROID_UNKNOWN_HUD;
      break;
    }
  }
  else if (f == 409600 && v == 5500)
  {
    result = METROID_SCAN_HOLOGRAM;
    g_metroid_scan_visor_active = true;
  }
  else if (f == 409600 && v == 5443)
  {
    result = METROID_MAP_LEGEND;
    g_metroid_map_screen = true;
  }
  else if (f == 75000 && (v == 4958 || v == 4522))
  {
    result = METROID_MORPHBALL_WORLD;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = true;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  else if (f == 75000 && (h == 7327 || h == 8564))
  {
    g_metroid_cinematic = false;
    ++g_metroid_normal_count;
    g_metroid_map_screen = false;
    switch (g_metroid_normal_count)
    {
    case 1:
      result = METROID_WORLD;
      g_metroid_morphball_active = false;
      break;
    case 2:
      // The scan reticle needs to be handled differently
      // because it is locked to your helmet, not the world.
      if (g_metroid_scan_visor)
        result = METROID_SCAN_RETICLE;
      else if (g_metroid_dark_visor)
        result = METROID_UNKNOWN_WORLD;
      else
        result = METROID_RETICLE;
      break;
    case 3:
      // The dark visor uses the normal reticle, but there is an UNKNOWN world layer before it
      if (g_metroid_dark_visor)
        result = METROID_RETICLE;
      else
        result = METROID_UNKNOWN_WORLD;
      break;
    default:
      result = METROID_UNKNOWN_WORLD;
      break;
    }
  }
  else if (f == 409600 && layer == 4 && g_metroid_map_screen)
  {
    result = METROID_INVENTORY_SAMUS;
    g_metroid_inventory = true;
    g_metroid_cinematic = false;
  }
  else if (g_metroid_morphball_active && v >= 4958 && v < 5500)
  {
    result = METROID_MORPHBALL_WORLD;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = true;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  else
  {
    g_metroid_cinematic = true;
    g_metroid_scan_visor = false;
    g_metroid_map_screen = false;
    result = METROID_CINEMATIC_WORLD;
  }
  return result;
}

TMetroidLayer GetMetroidPrime3Layer2D(int layer, float left, float right, float top, float bottom,
                                      float znear, float zfar)
{
  TMetroidLayer result;
  int l = Round100(left);
  int r = Round100(right);
  int t = Round100(top);
  int b = Round100(bottom);
  int n = Round100(znear);
  int f = Round100(zfar);
  if (abs(t) == 44800 || abs(b) == 44800)
    g_metroid_vres = 448;
  else if (abs(t) == 52800 || abs(b) == 52800)
    g_metroid_vres = 528;

  if (l == -32000 && t == g_metroid_vres * 50 && r == 32000 && b == -g_metroid_vres * 50 &&
      n == -100 && f == -1000)
  {
    // this is always the last layer, but I don't know what it is for
    result = METROID_UNKNOWN_2D;
  }
  else if (layer == 0 && l == 0 && t == g_metroid_vres * 50 && r == 32000 && b == 0 && n == -100 &&
           f == 100)
  {
    result = METROID_MAP_0;
    g_metroid_map_screen = true;
    g_metroid_morphball_active = false;
  }
  else if (layer == 1)
  {
    if (l == -3200 && r == 3200 && t == 3200 && b == -3200 && n == -100 && f == -1000)
    {
      result = METROID_SHADOW_2D;
    }
    else if (l == -32000 && t == g_metroid_vres * 50 && r == 32000 && b == -g_metroid_vres * 50 &&
             n == -409600 && f == 409600)
    {
      result = METROID_MAP_1;
      g_metroid_map_screen = true;
      g_metroid_morphball_active = false;
    }
    else if (l == 0 && t == g_metroid_vres * 100 && r == 64000 && b == 0 && n == -409600 &&
             f == 409600)
    {
      // This is some sort of fullscreen effect over a cutscene
      result = METROID_EFB_COPY;
    }
    else
    {
      result = METROID_UNKNOWN_2D;
    }
  }
  else if (layer > 1 && l == 0 && t == g_metroid_vres * 100 && r == 64000 && b == 0 &&
           n == -409600 && f == 409600)
  {
    // This is some sort of fullscreen effect
    result = METROID_EFB_COPY;
  }
  else if (layer > 1 && l == 0 && t == 0 && r == 640 && b == 528 && n == -100 && f == 100)
  {
    result = METROID_MOUSE_POINTER;
  }
  else if (layer > 1 && l == -32000 && t == g_metroid_vres * 50 && r == 32000 &&
           b == -g_metroid_vres * 50 && n == -409600 && f == 409600)
  {
    // used in the cinematic cutscene when you first enter the federation ship
    result = METROID_SCREEN_FADE;
  }
  else
    result = METROID_UNKNOWN_2D;
  return result;
}

TMetroidLayer GetMetroidPrime3Layer(int layer, float hfov, float vfov, float znear, float zfar)
{
  int h = Round100(hfov);
  int v = Round100(vfov);
  int a = Round100(hfov / vfov);
  int n = Round100(znear);
  int f = Round100(zfar);
  TMetroidLayer result;

  if (layer < 2)
  {
    // would be a 2D layer if it was still the map screen.
    g_metroid_map_screen = false;
  }

  if ((v == h || a == 125) && v < 300 && h < 300 && layer == 0)
  {
    result = METROID_SHADOWS;
  }
  else if (f == 409600 && v == 6500)
  {
    g_metroid_cinematic = false;
    if (h == 8055)
    {
      g_metroid_inventory = false;

      ++g_metroid_wide_count;
      if (g_metroid_morphball_active)
      {
        result = METROID_MORPHBALL_HUD;
      }
      else if (g_metroid_after_cursor && !g_metroid_map_screen)
      {
        result = METROID_MAP;
      }
      else if (g_metroid_wide_count == g_metroid_helmet_index && !g_metroid_map_screen)
      {
        result = METROID_HELMET;
      }
      else
      {
        switch (g_metroid_wide_count)
        {
        case 1:
          if (layer == 2 && g_metroid_map_screen)
            result = METROID_MAP_2;
          else
            result = METROID_HUD;
          break;
        case 2:
          if (g_metroid_map_screen)
            result = METROID_HELMET;
          else
            // actually it is the satellite dish
            result = METROID_RADAR_DOT;
          break;
        case 3:
          // hint message
          if (g_metroid_map_screen)
            // unknown
            result = METROID_MAP_MAP;
          else
            result = METROID_VISOR_RADAR_HINT;
          break;
        case 4:
          // helmet
          if (g_metroid_map_screen)
          {
            // really this is the legend
            result = METROID_MAP_MAP;
          }
          else
          {
            g_metroid_helmet_index = 4;
            result = METROID_HELMET;
          }
          break;
        case 5:
          // map - note this always comes after the cursor layer!
          if (g_metroid_map_screen)
            result = METROID_MAP_NORTH;
          else
            result = METROID_MAP;
          break;
        case 6:
          if (g_metroid_map_screen)
            // really this is the visor HUD
            result = METROID_MAP_MAP;
          else
            result = METROID_UNKNOWN_HUD;
          break;
        default:
          result = METROID_UNKNOWN_HUD;
          break;
        }
      }
    }
    else
    {
      result = METROID_UNKNOWN_HUD;
    }
  }
  else if (f == 75000 && v == 6000)
  {
    result = METROID_MORPHBALL_WORLD;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = true;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  else if (f == 75000 && v == 5021)
  {
    result = METROID_XRAY_WORLD;
    g_metroid_xray_visor = true;
    vr_widest_3d_HFOV = abs(hfov);
    vr_widest_3d_VFOV = abs(vfov);
    vr_widest_3d_zNear = znear;
    vr_widest_3d_zFar = zfar;
    g_metroid_morphball_active = false;
    g_metroid_map_screen = false;
    g_metroid_scan_visor = false;
    g_metroid_inventory = false;
    g_metroid_cinematic = false;
  }
  // 73.27 = GameCube, 85.64 = Wii (widescreen)
  else if (f == 75000 && (h == 8951))
  {
    ++g_metroid_normal_count;
    g_metroid_cinematic = false;
    g_metroid_morphball_active = false;
    g_metroid_map_screen = false;
    g_metroid_inventory = false;
    vr_widest_3d_HFOV = hfov;
    vr_widest_3d_VFOV = vfov;
    vr_widest_3d_zFar = zfar;
    vr_widest_3d_zNear = znear;
    if (g_metroid_wide_count > 0)
    {
      result = METROID_WII_RETICLE;
      g_metroid_after_cursor = true;
      g_metroid_helmet_index = g_metroid_wide_count;
    }
    else
    {
      switch (g_metroid_normal_count)
      {
      case 1:
        result = METROID_GUN;
        break;
      case 2:
        result = METROID_WORLD;
        break;
      case 3:
        result = METROID_RETICLE;
        break;
      case 4:
        result = METROID_RETICLE;
        break;
      default:
        result = METROID_UNKNOWN_WORLD;
        break;
      }
    }
  }
  else if (f == 75000 && h == 6594)
  {
    result = METROID_WII_WORLD;
  }
  else if (h == 5830 && v == 3264 && f == 7500)
  {
    // Menu
    result = METROID_UNKNOWN_WORLD;
  }
  else if (h == 8213 && v == 5400 && n == 100 && f == 409600)
  {
    // Miis
    result = METROID_UNKNOWN_WORLD;
  }
  else
  {
    g_metroid_cinematic = true;
    g_metroid_morphball_active = false;
    result = METROID_CINEMATIC_WORLD;
  }
  return result;
}

TMetroidLayer GetZeldaTPGCLayer2D(int layer, float left, float right, float top, float bottom,
                                  float znear, float zfar)
{
  TMetroidLayer result;
  int l = Round100(left);
  int r = Round100(right);
  int t = Round100(top);
  int b = Round100(bottom);
  int n = Round100(znear);
  int f = Round100(zfar);
  if (l == 0 && r == 60800 && t == 0 && n == 0 && f == 100)
  {
    result = ZELDA_BACKGROUND;
  }
  else if (r == 400 && f == 1000)
  {
    // 5: 2D: **Screenspace effects** Unknown 2D (-0, 0) to (4, 4); z: -0 to 10  [-0.1, -1]
    result = ZELDA_DARK_EFFECT;
  }
  else if (g_zelda_hawkeye)
  {
    result = ZELDA_HAWKEYE_HUD;
  }
  else if (r == 60800 && f == 10000)
  {
    // inside your house when game is looking at fire
    result = ZELDA_UNKNOWN_EFFECT;
    g_zelda_had_unknown_effect = true;
    //--g_zelda_world_count;
  }
  else if (r == 60800 && n == 10000000 && g_zelda_world_count)
  {
    ++g_zelda_hud_count;
    switch (g_zelda_hud_count)
    {
    case 1:
      // 6: 2D : **Map on HUD** Unknown 2D (-0, 0) to(608, 448); z: 100000 to - 100000[5e-006, -0.5]
      result = ZELDA_DIALOG;
      break;
    case 2:
      // 7: 2D: **HUD** Unknown 2D (-0, 0) to (608, 448); z: 100000 to -100000  [5e-006, -0.5]
      result = ZELDA_NEXT;
      break;
    default:
      result = ZELDA_UNKNOWN_2D;
    }
  }
  else if (l == -r && r > 60000 && t == r && b == -t && n == 0 && f == 1000000)
  {
    // 1: 2D: **Render Map to texture** Unknown 2D (-15620.9, 15620.9) to (15620.9, -15620.9); z: -0
    // to 10000  [-0.0001, -1]
    result = ZELDA_CREATE_MAP;
  }
  else if (l == -r && r > 60000 && t > 60000 && b == -t && n == 0 && f == 1000000)
  {
    // map screen map
    // Viewport 1: Render to Texture(0, 0) 457x341; near = 0 (0), far = 1 (1.67772e+07)
    // copyTexSrc(0, 0) 608x448
    // 1: 2D : (create map map) Zelda Unknown 2D(-67369.3, 50269) to(67369.3, -50269); z: -0 to
    // 10000[-0.0001, -1]
    result = ZELDA_CREATE_MAP;
  }
  else if (l == -r && r < 60000 && t == r && b == -t && n == 100 && f == 1000000)
  {
    // 2: 2D : **Render Shadow to texture** Unknown 2D (-320, 320) to(320, -320); z: 1.00007 to
    // 10000[-0.00010001, -1.0001]
    // if r==320 then it's Link's shadow, if r==400 then it's Epona's shadow, other values for other
    // characters
    result = ZELDA_CREATE_SHADOW;
  }
  else
  {
    result = ZELDA_UNKNOWN_2D;
  }
  return result;
}

TMetroidLayer GetZeldaTPGCLayer(int layer, float hfov, float vfov, float znear, float zfar)
{
  TMetroidLayer result;
  // int h = Round100(hfov);
  int v = Round100(vfov);
  int n = Round100(znear);
  int f = Round100(zfar);

  ++g_zelda_world_count;
  switch (g_zelda_world_count)
  {
  case 1:
    // if (h == 2026 && v == 1500 && n == 500)
    // 3: Zelda World HFOV: 20.26deg; VFOV: 15.00deg; Aspect Ratio: 16:11.8; near:5.000000,
    // far:30000.001953
    // This is probably wrong.
    // result = ZELDA_FISHING;
    if (v == 4500 && n == 100 && f == 10000000)
    {
      result = ZELDA_LINK;
      g_zelda_hawkeye = false;
    }
    else if (v <= 5000 && v >= 590 && n == 3000)
    {
      // 3: Zelda World HFOV: 64.60deg; VFOV: 49.95deg; Aspect Ratio: 16:11.8; near:30.000002,
      // far:30000.001953
      result = ZELDA_HAWKEYE;
      g_zelda_hawkeye = true;
    }
    else
    {
      // 3: **World** Unknown HFOV : 76.16deg; VFOV: 60.00deg; Aspect Ratio : 16 : 11.8;
      // near:5.000000, far : 30000.001953
      result = ZELDA_WORLD;
      g_zelda_hawkeye = false;
    }
    break;
  case 2:
    // 4: **WATER reflection** Unknown HFOV : 76.16deg; VFOV: 60.00deg; Aspect Ratio : 16 : 11.8;
    // near:5.000000, far : 30000.001953
    if (g_zelda_had_unknown_effect)
      result = ZELDA_WORLD;
    else
      result = ZELDA_REFLECTION;
    break;
  default:
    // could be a yellow targetting triangle
    result = ZELDA_UNKNOWN;
    break;
  }
  return result;
}

TMetroidLayer GetNESLayer2D(int layer, float left, float right, float top, float bottom,
                            float znear, float zfar)
{
  TMetroidLayer result;
  int l = Round100(left);
  int r = Round100(right);
  int t = Round100(top);
  int b = Round100(bottom);
  // int n = Round100(znear);
  int f = Round100(zfar);
  if (l == 0 && t == 0 && r == 51200 && (b == -22800 || b == -23200))
  {
    // Mario NTSC: 0: 2D: NES Render To Texture (-0, 0) to (512, -228); z: -0 to 2000  [-0.0005, -1]
    result = NES_RENDER_TO_TEXTURE;
  }
  else if (l == 0 && t == 0 && (r == 25600 || r == 51200) && (b == 22800 || b == 23200))
  {
    // Mario NTSC: 1 : 2D : NES Unknown 2D (-0, 0) to(512, 228); z: 0 to - 1[1, -1]
    result = NES_SHOW_TEXTURE;
  }
  else if (l == -640 && r == 26220 && f == -100)
  {
    // NTSC: 0 : 2D : NES Unknown 2D (-6.39999, 0) to(262.2, 228); z: 0 to - 1[1, -1]
    // PAL:  0 : 2D : NES Unknown 2D (-6.39999, -19.5) to(262.2, 251.5); z: 0 to - 1[1, -1]
    result = NES_WII_GUI_BACKGROUND;
  }
  else if (l == -2800 && r == 54000 && t == 0 && b == 32000 && f == 1500)
  {
    // NTSC: 1 : 2D : NES Unknown 2D (-28, 0) to(540, 320); z: -0 to 15[-0.0666667, -1]
    // PAL:  1 : 2D : NES Unknown 2D (-28, 0) to(540, 380.351); z: -0 to 15[-0.0666667, -1]
    result = NES_WII_GUI_ERROR;
  }
  else if (l == -30400 && r == 30400 && t == 22800 && b == -22800 && f == 50000)
  {
    // 1 : 2D : NES Unknown 2D (-304, 228) to(304, -228); z: -0 to 500[-0.002, -1]
    result = NES_WII_GUI_MENU;
  }
  else
  {
    result = NES_UNKNOWN_2D;
  }
  return result;
}

void GetMetroidPrimeValues(bool* bStuckToHead, bool* bFullscreenLayer, bool* bHide, bool* bFlashing,
                           float* fScaleHack, float* fWidthHack, float* fHeightHack, float* fUpHack,
                           float* fRightHack, int* iTelescope)
{
  switch (g_metroid_layer)
  {
  case METROID_EFB_COPY:
    *bHide = true;
    break;
  case METROID_BLACK_BARS:
    *bHide = true;
    break;
  case METROID_SCREEN_FADE:
    *bFullscreenLayer = true;
    *bStuckToHead = true;
    break;
  case METROID_SHADOWS:
  case METROID_BODY_SHADOWS:
    *bStuckToHead = false;
    break;
  case METROID_ECHO_EFFECT:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    *fHeightHack = (float)g_metroid_vres / 528.0f;
    *fWidthHack = 1.0f;
    *fUpHack = 0.0f;
    *fRightHack = 0.0f;
    *fScaleHack = 0.5;
    break;
  case METROID_THERMAL_EFFECT:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    *fHeightHack = (float)g_metroid_vres / 528.0f;
    *fWidthHack = 1.0f;
    *fUpHack = 0.0f;
    *fRightHack = 0.0f;
    break;
  case METROID_CHARGE_BEAM_EFFECT:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    *fHeightHack = (float)g_metroid_vres / 528.0f;
    *fWidthHack = 1.0f;
    *fUpHack = 0.0f;
    *fRightHack = 0.0f;
    break;
  case METROID_THERMAL_EFFECT_GUN:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    *fHeightHack = (float)g_metroid_vres / 528.0f;
    *fWidthHack = 1.0f;
    *fUpHack = 0.0f;
    *fRightHack = 0.0f;
    // only left eye will line up for distant objects
    break;
  case METROID_XRAY_EFFECT:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    if (g_metroid_vres == 528)
    {
      *fHeightHack = 1.0f;
      *fUpHack = 0.0f;
    }
    else
    {
      // This isn't quite right, but I can't work out what it should be.
      *fHeightHack = 402.0f / 528.0f;
      *fUpHack = 0.38f;
    }
    *fWidthHack = 1.0f;
    //*fRightHack = 0.01f;
    break;
  case METROID_DARK_EFFECT:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    *fHeightHack = (float)g_metroid_vres / 528.0f;
    *fWidthHack = 1.0f;
    *fUpHack = 0.0f;
    *fRightHack = 0.0f;
    break;
  case METROID_DARK_CENTRAL:
    *bStuckToHead = true;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_VISOR_DIRT:
    // todo: move to helmet depth (hard with a 2D layer)
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    //*fScaleHack = 30;
    //*fHeightHack = 2.0f;
    break;
  case METROID_VISOR_BOOTUP:
  case METROID_SCREEN_OVERLAY:
  case METROID_SCAN_DARKEN:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    break;
  case METROID_SCAN_CIRCLE:
    *bStuckToHead = false;
    break;
  case METROID_SCAN_BOX:
    *bStuckToHead = true;
    *fUpHack = 0.14f;
    *bHide = true;
    break;
  case METROID_SCAN_HIGHLIGHTER:
    // Because of the way EFB copies work in VR,
    // the scan cursor ends up in the corner of the screen.
    // Force the highlighter to almost line up with the object.
    // This only works correctly on DK1!!!
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    *fHeightHack = 448.0f / 528.0f;
    *fWidthHack = 1.01f;
    *fUpHack = 0.1588f;
    *fRightHack = 0.015f;

    // Old DK1 settings which probably no longer make sense
    //*fHeightHack = 3.0f;
    //*fWidthHack = 1.8f;
    //*fUpHack = 0.14f;
    // Should actually be about 0.38, but hard to look at then.
    //*fRightHack = -0.36f;
    break;
  case METROID_SCAN_RETICLE:
    // Because of the way EFB copies work in VR,
    // the scan cursor ends up in the corner of the screen.
    // So make it bigger and easier to see.
    *fHeightHack = 1.0f;
    *fWidthHack = 1.0f;
    *bStuckToHead = true;
    *fUpHack = 0.14f;
    *fRightHack = 0.01f;
    break;
  case METROID_MORPHBALL_HUD:
  case METROID_MORPHBALL_MAP_OR_HINT:
  case METROID_MORPHBALL_MAP:
    *bStuckToHead = false;
    *fScaleHack = 20.0f;
    *fWidthHack = 0.8f;
    *fHeightHack = 0.8f;
    break;
  case METROID_WII_WORLD:
  case METROID_UNKNOWN:
  case METROID_WORLD:
  case METROID_CINEMATIC_WORLD:
  case METROID_MORPHBALL_WORLD:
  case METROID_XRAY_WORLD:
  case METROID_THERMAL_GUN_AND_DOOR:
  case METROID_GUN:
  case METROID_RETICLE:
  case METROID_SCAN_ICONS:
  case METROID_SCAN_CROSS:
  case METROID_UNKNOWN_WORLD:
  case METROID_UNKNOWN_2D:
  case METROID_MOUSE_POINTER:
  case METROID_SHADOW_2D:
    *bStuckToHead = false;
    break;
  case METROID_WII_RETICLE:
    *bStuckToHead = false;
    // make it further away
    *fScaleHack = 0.1f;
    break;
  case METROID_MAP_0:
  case METROID_MAP_1:
  case METROID_MAP_2:
    *bStuckToHead = false;
    *fScaleHack = 0.02f;
    //*fHeightHack = 1.8f;
    //*fWidthHack = 1.2f;
    //*fUpHack = 0.1f;
    break;

  case METROID_DIALOG:
    *bStuckToHead = false;
    *fScaleHack = 5;
    break;
  case METROID_MAP_LEGEND:
    *bStuckToHead = false;
    *fScaleHack = 30;
    break;
  case METROID_MAP_MAP:
  case METROID_MAP_NORTH:
    *bStuckToHead = false;
    break;
  case METROID_XRAY_HUD:
    *bStuckToHead = false;
    *fScaleHack = 30;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_UNKNOWN_HUD:
  case METROID_HUD:
    *bStuckToHead = true;
    *fScaleHack = 30;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_DARK_VISOR_HUD:
    *bStuckToHead = true;
    *fScaleHack = 5;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_VISOR_RADAR_HINT:
    *bStuckToHead = true;
    *fScaleHack = 32;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_RADAR_DOT:
    *bStuckToHead = true;
    *fScaleHack = 36;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_VISOR:
    *bStuckToHead = true;
    *fScaleHack = 90;
    *fWidthHack = 1.0f;
    *fHeightHack = 1.0f;
    break;
  case METROID_MAP_OR_HINT:
  case METROID_MAP:
    *bStuckToHead = true;
    *fScaleHack = 30;
    *fWidthHack = 0.79f;
    *fHeightHack = 0.79f;
    break;
  case METROID_HELMET:
    *bStuckToHead = true;
    *fScaleHack = 100;  // 1/100
    *fWidthHack = 1.7f;
    *fHeightHack = 1.7f;
    break;
  case METROID_SCAN_TEXT:
    // the text box (and side boxes) seen when scanning
    // they should be almost within reach.
    *bStuckToHead = true;
    *fScaleHack = 32;
    *fWidthHack = 0.8f;
    *fHeightHack = 0.8f;
    break;
  case METROID_SCAN_HOLOGRAM:
    // move the hologram inside the box when scanning ship
    // and make it bigger
    *bStuckToHead = true;
    *fScaleHack = 40;
    *fWidthHack = 1.2f;
    *fHeightHack = 1.2f;
    break;
  case METROID_INVENTORY_SAMUS:
    *bStuckToHead = false;
    break;
  case METROID_INVENTORY_SAMUS_OUTLINE:
    *bStuckToHead = true;
    *bFullscreenLayer = true;
    break;

  case ZELDA_HAWKEYE:
    *iTelescope = 3;
    break;
  case ZELDA_HAWKEYE_HUD:
    *iTelescope = 3;
    //*fScaleHack = 100.0f;
    break;
  case ZELDA_FISHING:
    *iTelescope = 0;
    break;
  case ZELDA_UNKNOWN:
  case ZELDA_WORLD:
  case ZELDA_LINK:
  case ZELDA_DIALOG:
  case ZELDA_NEXT:
  case ZELDA_UNKNOWN_2D:
  case ZELDA_UNKNOWN_EFFECT:
    *iTelescope = 0;
    // render most things like normal
    break;

  case ZELDA_DARK_EFFECT:
    *iTelescope = 0;
    *bHide = true;
    break;
  case ZELDA_REFLECTION:
    *iTelescope = 0;
    *bHide = true;
    break;

  case NES_RENDER_TO_TEXTURE:
    g_viewport_type = VIEW_RENDER_TO_TEXTURE;
    if (g_has_hmd && g_ActiveConfig.bEnableVR)
    {
      // The Wii doesn't change viewports here, but we need to, because we are going from fullscreen
      // VR to rendering to a texture.
      VertexShaderManager::SetViewportConstants();
    }
    break;
  case NES_SHOW_TEXTURE:
    g_viewport_type = VIEW_LETTERBOXED;
    if (g_has_hmd && g_ActiveConfig.bEnableVR)
    {
      // The Wii doesn't change viewports here, but we need to, because we are going from rendering
      // to a texture to fullscreen VR rendering
      VertexShaderManager::SetViewportConstants();
      // On the real Wii, it doesn't need to clear the EFB before drawing, because the drawing will
      // cover up the part we drew to before.
      // But in VR, we need to clear the EFB here or we get the old drawing stuck in the top left
      // corner of our view.
      if (g_ActiveConfig.bEFBCopyEnable)
        BPFunctions::ClearScreen(EFBRectangle(0, 0, EFB_WIDTH, EFB_HEIGHT), false);
    }
    break;
  case NES_WII_GUI_BACKGROUND:
    g_viewport_type = VIEW_LETTERBOXED;
    if (g_has_hmd && g_ActiveConfig.bEnableVR)
    {
      VertexShaderManager::SetViewportConstants();
    }
    break;
  case NES_WII_GUI_ERROR:
  case NES_WII_GUI_MENU:
  case NES_UNKNOWN_2D:
    g_viewport_type = VIEW_LETTERBOXED;
    *fScaleHack = 2.0f;
    if (g_has_hmd && g_ActiveConfig.bEnableVR)
    {
      VertexShaderManager::SetViewportConstants();
    }
    break;

  default:
    *bStuckToHead = true;
    *fScaleHack = 30;  // 1/30
    break;
  }
}

//#pragma optimize("", on)
