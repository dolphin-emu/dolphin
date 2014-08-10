// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <sstream>

#ifdef HAVE_OCULUSSDK
#include "Kernel/OVR_Types.h"
#include "OVR_CAPI.h"
#include "Kernel/OVR_Math.h"
#endif

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/XFMemory.h"

static float GC_ALIGNED16(g_fProjectionMatrix[16]);

// TODO: remove
//VR Global variable shared from core. True if the Wii is set to Widescreen (so the game thinks it is rendering to 16:9)
//   or false if it isn't (so the game thinks it is rendering to 4:3). Which is different from how Dolphin will actually render it.
extern bool g_aspect_wide;

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged;
static int nMaterialsChanged;
static int nTransformMatricesChanged[2]; // min,max
static int nNormalMatricesChanged[2]; // min,max
static int nPostTransformMatricesChanged[2]; // min,max
static int nLightsChanged[2]; // min,max

static Matrix44 s_viewportCorrection;
static Matrix33 s_viewRotationMatrix;
static Matrix33 s_viewInvRotationMatrix;
static float s_fViewTranslationVector[3];
static float s_fViewRotation[2];

VertexShaderConstants VertexShaderManager::constants;
float4 VertexShaderManager::constants_eye_projection[2][4];

bool VertexShaderManager::dirty;

//VR Virtual Reality debugging variables
typedef enum
{
	METROID_UNKNOWN = 0,
	METROID_BACKGROUND_3D,
	METROID_SHADOW_2D,
	METROID_SCREEN_OVERLAY,
	METROID_WORLD,
	METROID_XRAY_WORLD,
	METROID_XRAY_EFFECT,
	METROID_THERMAL_EFFECT,
	METROID_THERMAL_EFFECT_GUN,
	METROID_ECHO_EFFECT,
	METROID_DARK_CENTRAL,
	METROID_DARK_EFFECT,
	METROID_VISOR_DIRT,
	METROID_SCAN_HIGHLIGHTER,
	METROID_SCAN_BOX,
	METROID_THERMAL_GUN_AND_DOOR,
	METROID_SCAN_RETICLE,
	METROID_RETICLE,
	METROID_SCAN_ICONS,
	METROID_SCAN_CROSS,
	METROID_MORPHBALL_WORLD,
	METROID_UNKNOWN_WORLD,
	METROID_SCAN_DARKEN,
	METROID_HELMET,
	METROID_HUD,
	METROID_MORPHBALL_HUD,
	METROID_XRAY_HUD,
	METROID_VISOR_RADAR_HINT,
	METROID_RADAR_DOT,
	METROID_MORPHBALL_MAP_OR_HINT,
	METROID_MAP_OR_HINT,
	METROID_MORPHBALL_MAP,
	METROID_MAP,
	METROID_MAP_0,
	METROID_MAP_1,
	METROID_MAP_2,
	METROID_MAP_MAP,
	METROID_MAP_LEGEND,
	METROID_INVENTORY_SAMUS,
	METROID_MAP_NORTH,
	METROID_SCAN_VISOR,
	METROID_SCAN_TEXT,
	METROID_SCAN_HOLOGRAM,
	METROID_UNKNOWN_VISOR,
	METROID_UNKNOWN_HUD,
	METROID_UNKNOWN_2D,
	METROID_LAYER_COUNT
} TMetroidLayer;

// helmet, combat visor, radar dot, map, scan visor, scan text, and scan hologram should be attached to face
// unknown, world, and reticle should NOT be attached to face

int vr_render_eye = -1;
int debug_viewportNum = 0;
Viewport debug_vpList[64] = { 0 };
int debug_projNum = 0;
float debug_projList[64][7] = { 0 };
bool debug_newScene = true, debug_nextScene = false;
TMetroidLayer g_metroid_layer;
bool g_metroid_scan_visor = false, g_metroid_scan_visor_active = false, 
g_metroid_xray_visor = false, g_metroid_thermal_visor = false,
g_metroid_map_screen = false, g_metroid_inventory = false,
g_metroid_dark_visor = false, g_metroid_echo_visor = false, g_metroid_morphball_active = false;
int g_metroid_wide_count = 0, g_metroid_normal_count = 0;
int vr_widest_3d_projNum = -1;
float vr_widest_3d_HFOV = 0;
float vr_widest_3d_VFOV = 0;
float vr_widest_3d_zNear = 0;
float vr_widest_3d_zFar = 0;

//#pragma optimize("", off)

int Round100(float x)
{
	return (int)floor(x * 100 + 0.5f);
}

const char *MetroidLayerName(TMetroidLayer layer)
{
	switch (layer)
	{
	case METROID_BACKGROUND_3D:
		return "Background 3D";
	case METROID_DARK_CENTRAL:
		return "Dark Visor Central";
	case METROID_DARK_EFFECT:
		return "Dark Visor Effect";
	case METROID_ECHO_EFFECT:
		return "Echo Visor Effect";
	case METROID_MAP_OR_HINT:
		return "Map or Hint";
	case METROID_HELMET:
		return "Helmet";
	case METROID_HUD:
		return "HUD";
	case METROID_INVENTORY_SAMUS:
		return "Inventory Samus";
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
	case METROID_RADAR_DOT:
		return "Radar Dot";
	case METROID_RETICLE:
		return "Reticle";
	case METROID_SCAN_BOX:
		return "Scan Box";
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
	case METROID_VISOR_DIRT:
		return "Visor Dirt";
	case METROID_SCAN_RETICLE:
		return "Scan Reticle";
	case METROID_SCAN_TEXT:
		return "Scan Text Box";
	case METROID_SCAN_VISOR:
		return "Scan Visor";
	case METROID_SHADOW_2D:
		return "Shadow 2D";
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
	case METROID_VISOR_RADAR_HINT:
		return "Visor/Radar/Hint";
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
	default:
		return "Error";
	}
}

TMetroidLayer GetMetroidPrime1GCLayer2D(int layer, float left, float right, float top, float bottom, float znear, float zfar)
{
	if (layer == 0)
	{
		g_metroid_xray_visor = false;
		g_metroid_thermal_visor = false;
	}

	TMetroidLayer result;
	int l = Round100(left);
	int n = Round100(znear);
	int f = Round100(zfar);
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
		else if (l == -32000 && n == -409600)
		{
			result = METROID_MAP_1;
			g_metroid_map_screen = true;
		}
		else if (l == 0 && n == -409600)
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
		else if (l == 0 && n == -409600)
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
		else if (l == -32000 && n == -409600 && f == 409600)
		{
			result = METROID_VISOR_DIRT;
		}
		else
		{
			result = METROID_UNKNOWN_2D;
			g_metroid_dark_visor = false;
			g_metroid_xray_visor = false;
		}
	}
	else if (l == -320)
	{
		result = METROID_MORPHBALL_HUD;
		g_metroid_morphball_active = true;
	}
	else if (l == -32000 && n == -409600 && f == 409600)
	{
		if (g_metroid_map_screen)
			result = METROID_MAP_NORTH;
		else
			result = METROID_SCAN_DARKEN;
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

TMetroidLayer GetMetroidPrime1GCLayer(int layer, float hfov, float vfov, float znear, float zfar)
{
	if (layer == 0)
	{
		g_metroid_xray_visor = false;
		g_metroid_thermal_visor = false;
	}

	int h = Round100(hfov);
	int v = Round100(vfov);
	//	int n = Round100(znear);
	int f = Round100(zfar);
	TMetroidLayer result;
	// In Dark Visor layer 2 would be 2D not 3D
	if (layer == 2)
		g_metroid_dark_visor = false;

	if (f == 409600 && v == 6500)
	{
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
				else if (g_metroid_dark_visor && layer == 1)
				{
					result = METROID_DARK_CENTRAL;
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
					result = METROID_HUD;
					g_metroid_scan_visor_active = false;
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
				else if (g_metroid_dark_visor)
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
		result = METROID_MAP_LEGEND;
		g_metroid_map_screen = true;
	}
	else if (f == 75000 && v == 4958)
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
	}
	else if (f == 75000 && h == 7327)
	{
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
	else
	{
		result = METROID_UNKNOWN;
	}
	return result;
}

TMetroidLayer GetMetroidPrime2GCLayer2D(int layer, float left, float right, float top, float bottom, float znear, float zfar)
{
	TMetroidLayer result;
	int l = Round100(left);
	int n = Round100(znear);
	int f = Round100(zfar);
	if (layer == 1)
	{
		g_metroid_dark_visor = false;
		if (l == -32000 && n == -409600)
		{
			result = METROID_SCAN_HIGHLIGHTER;
			g_metroid_scan_visor = true;
			g_metroid_echo_visor = false;
		}
		else if (l == 0 && n == -409600)
		{
			result = METROID_ECHO_EFFECT;
			g_metroid_echo_visor = true;
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
		if (l == 0 && n == -409600)
		{
			result = METROID_DARK_EFFECT;
			g_metroid_dark_visor = true;
			g_metroid_scan_visor = false;
			g_metroid_scan_visor_active = false;
			g_metroid_morphball_active = false;
		}
		else
		{
			result = METROID_UNKNOWN_2D;
			g_metroid_dark_visor = false;
		}
	}
	else if (layer == 4 && l == -32000 && n == -100 && f == 100)
		result = METROID_SCAN_BOX;
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
	if (layer == 2)
		g_metroid_dark_visor = false;

	if (f == 409600 && v == 6500)
	{
		++g_metroid_wide_count;
		switch (g_metroid_wide_count)
		{
			case 1:
				if (g_metroid_morphball_active)
					result = METROID_MORPHBALL_HUD;
				else if (g_metroid_dark_visor && layer == 1)
					result = METROID_DARK_CENTRAL;
				else
					result = METROID_HELMET;
				break;
			case 2:
				if (g_metroid_morphball_active)
					result = METROID_MORPHBALL_MAP_OR_HINT;
				else if (g_metroid_dark_visor)
					result = METROID_HELMET;
				else
					result = METROID_HUD;
				break;
			case 3:
				// visor
				if (g_metroid_morphball_active)
				{
					result = METROID_MORPHBALL_MAP;
					g_metroid_scan_visor_active = false;
				}
				else if (g_metroid_dark_visor)
				{
					result = METROID_HUD;
					g_metroid_scan_visor_active = false;
				}
				else if (h == 8243)
				{
					result = METROID_RADAR_DOT;
					g_metroid_scan_visor_active = false;
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
	else if (f == 75000)
	{
		++g_metroid_normal_count;
		switch (g_metroid_normal_count)
		{
		case 1:
			if (v == 4958)
			{
				result = METROID_MORPHBALL_WORLD;
				g_metroid_morphball_active = true;
			}
			else
			{
				result = METROID_WORLD;
				g_metroid_morphball_active = false;
			}
			break;
		case 2:
			// The scan reticle needs to be handled differently
			// because it is locked to your helmet, not the world.
			if (g_metroid_scan_visor)
				result = METROID_SCAN_RETICLE;
			else
				result = METROID_RETICLE;
			break;
		default:
			result = METROID_UNKNOWN_WORLD;
			break;
		}
	}
	else
	{
		result = METROID_UNKNOWN;
	}
	return result;
}

void ClearDebugProj() { //VR
	debug_newScene = debug_nextScene;
	if (debug_newScene)
	{
		NOTICE_LOG(VR, "***** New scene *****");
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
	g_metroid_wide_count = 0;
	g_metroid_normal_count = 0;
}

void DoLogViewport(int j, Viewport &v) { //VR
	NOTICE_LOG(VR, "  Viewport %d: (%g,%g) %gx%g; near=%g, far=%g", j, v.xOrig - v.wd - 342, v.yOrig + v.ht - 342, 2 * v.wd, -2 * v.ht, (v.farZ - v.zRange) / 16777216.0f, v.farZ / 16777216.0f);
}

void DoLogProj(int j, float p[], const char *s) { //VR
	if (j == g_ActiveConfig.iSelectedLayer)
		NOTICE_LOG(VR, "** SELECTED LAYER:");
	if (p[6] != 0) { // orthographic projection
		//float right = p[0]-(p[0]*p[1]);
		//float left = right - 2/p[0];

		float left = -(p[1] + 1) / p[0];
		float right = left + 2 / p[0];
		float bottom = -(p[3] + 1) / p[2];
		float top = bottom + 2 / p[2];
		float zfar = p[5] / p[4];
		float znear = (1 + p[4] * zfar) / p[4];
		NOTICE_LOG(VR, "%d: 2D: %s (%g, %g) to (%g, %g); z: %g to %g  [%g, %g]", j, s, left, top, right, bottom, znear, zfar, p[4], p[5]);
	}
	else if (p[0] != 0 || p[2] != 0) { // perspective projection
		float f = p[5] / p[4];
		float n = f*p[4] / (p[4] - 1);
		if (p[1] != 0.0f || p[3] != 0.0f) {
			NOTICE_LOG(VR, "%d: %s OFF-AXIS Perspective: 2n/w=%.2f A=%.2f; 2n/h=%.2f B=%.2f; n=%.2f f=%.2f", j, s, p[0], p[1], p[2], p[3], p[4], p[5]);
			NOTICE_LOG(VR, "	HFOV: %.2f    VFOV: %.2f   Aspect Ratio: 16:%.1f", 2 * atan(1.0f / p[0])*180.0f / 3.1415926535f, 2 * atan(1.0f / p[2])*180.0f / 3.1415926535f, 16 / (2 / p[0])*(2 / p[2]));
		}
		else {
			NOTICE_LOG(VR, "%d: %s HFOV: %.2fdeg; VFOV: %.2fdeg; Aspect Ratio: 16:%.1f; near:%f, far:%f", j, s, 2 * atan(1.0f / p[0])*180.0f / 3.1415926535f, 2 * atan(1.0f / p[2])*180.0f / 3.1415926535f, 16 / (2 / p[0])*(2 / p[2]), n, f);
		}
	}
	else { // invalid
		NOTICE_LOG(VR, "%d: %s ZERO", j, s);
	}
}

void LogProj(float p[]) { //VR
	if (p[6] == 0) { // perspective projection
		// don't change this formula!
		// metroid layer detection depends on exact values
		float vfov = (2 * atan(1.0f / p[2])*180.0f / 3.1415926535f);
		float hfov = (2 * atan(1.0f / p[0])*180.0f / 3.1415926535f);
		float f = p[5] / p[4];
		float n = f*p[4] / (p[4] - 1);
		switch (g_ActiveConfig.iMetroidPrime)
		{
		case 1:
			g_metroid_layer = GetMetroidPrime1GCLayer(debug_projNum, hfov, vfov, n, f);
			break;
		case 2:
			g_metroid_layer = GetMetroidPrime2GCLayer(debug_projNum, hfov, vfov, n, f);
			break;
		case 0:
		default:
			g_metroid_layer = METROID_UNKNOWN;
			break;
		}

		if (debug_newScene && fabs(hfov) > vr_widest_3d_HFOV && fabs(hfov) <= 125 && (fabs(p[2]) != fabs(p[0]))) {
			WARN_LOG(VR, "***** New Widest 3D *****");

			vr_widest_3d_projNum = debug_projNum;
			vr_widest_3d_HFOV = fabs(hfov);
			vr_widest_3d_VFOV = fabs(vfov);
			vr_widest_3d_zNear = fabs(n);
			vr_widest_3d_zFar = fabs(f);
			WARN_LOG(VR, "%d: %g x %g deg, n=%g f=%g, p4=%g p5=%g; xs=%g ys=%g", vr_widest_3d_projNum, vr_widest_3d_HFOV, vr_widest_3d_VFOV, vr_widest_3d_zNear, vr_widest_3d_zFar, p[4], p[5], p[0], p[2]);
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
			g_metroid_layer = GetMetroidPrime1GCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
			break;
		case 2:
			g_metroid_layer = GetMetroidPrime2GCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
			break;
		case 0:
		default:
			g_metroid_layer = METROID_UNKNOWN_2D;
			break;
		}
	}

	if (debug_projNum >= 64)
		return;
	if (!debug_newScene) {
		for (int i = 0; i<7; i++) {
			if (debug_projList[debug_projNum][i] != p[i]) {
				debug_nextScene = true;
				debug_projList[debug_projNum][i] = p[i];
			}
		}
		// wait until next frame
		//if (debug_newScene) {
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
		INFO_LOG(VR, "HUD is %.1fm away and %.1fm thick", g_ActiveConfig.fHudDistance, g_ActiveConfig.fHudThickness);
		DoLogProj(debug_projNum, debug_projList[debug_projNum], MetroidLayerName(g_metroid_layer));
	}
	debug_projNum++;
}

void LogViewport(Viewport &v) { //VR
	if (debug_viewportNum >= 64)
		return;
	if (!debug_newScene) {
		if (debug_vpList[debug_viewportNum].farZ != v.farZ ||
			debug_vpList[debug_viewportNum].ht != v.ht ||
			debug_vpList[debug_viewportNum].wd != v.wd ||
			debug_vpList[debug_viewportNum].xOrig != v.xOrig ||
			debug_vpList[debug_viewportNum].yOrig != v.yOrig ||
			debug_vpList[debug_viewportNum].zRange != v.zRange) {
			debug_nextScene = true;
			debug_vpList[debug_viewportNum] = v;
		}
	}
	else {
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
	ProjectionHack() { }
	ProjectionHack(float new_sign, float new_value)
		: sign(new_sign), value(new_value) {}
};

namespace
{
// Control Variables
static ProjectionHack g_ProjHack1;
static ProjectionHack g_ProjHack2;
} // Namespace

static float PHackValue(std::string sValue)
{
	float f = 0;
	bool fp = false;
	const char *cStr = sValue.c_str();
	char *c = new char[strlen(cStr)+1];
	std::istringstream sTof("");

	for (unsigned int i=0; i<=strlen(cStr); ++i)
	{
		if (i == 20)
		{
			c[i] = '\0';
			break;
		}

		c[i] = (cStr[i] == ',') ? '.' : *(cStr+i);
		if (c[i] == '.')
			fp = true;
	}

	cStr = c;
	sTof.str(cStr);
	sTof >> f;

	if (!fp)
		f /= 0xF4240;

	delete [] c;
	return f;
}

void UpdateProjectionHack(int iPhackvalue[], std::string sPhackvalue[])
{
	float fhackvalue1 = 0, fhackvalue2 = 0;
	float fhacksign1 = 1.0, fhacksign2 = 1.0;
	const char *sTemp[2];

	if (iPhackvalue[0] == 1)
	{
		NOTICE_LOG(VIDEO, "\t\t--- Orthographic Projection Hack ON ---");

		fhacksign1 *= (iPhackvalue[1] == 1) ? -1.0f : fhacksign1;
		sTemp[0] = (iPhackvalue[1] == 1) ? " * (-1)" : "";
		fhacksign2 *= (iPhackvalue[2] == 1) ? -1.0f : fhacksign2;
		sTemp[1] = (iPhackvalue[2] == 1) ? " * (-1)" : "";

		fhackvalue1 = PHackValue(sPhackvalue[0]);
		NOTICE_LOG(VIDEO, "- zNear Correction = (%f + zNear)%s", fhackvalue1, sTemp[0]);

		fhackvalue2 = PHackValue(sPhackvalue[1]);
		NOTICE_LOG(VIDEO, "- zFar Correction =  (%f + zFar)%s", fhackvalue2, sTemp[1]);

	}

	// Set the projections hacks
	g_ProjHack1 = ProjectionHack(fhacksign1, fhackvalue1);
	g_ProjHack2 = ProjectionHack(fhacksign2, fhackvalue2);
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

	result.data[4*0+0] = intendedWd / Wd;
	result.data[4*0+3] = (intendedWd - 2.f * (X - intendedX)) / Wd - 1.f;
	result.data[4*1+1] = intendedHt / Ht;
	result.data[4*1+3] = (-intendedHt + 2.f * (Y - intendedY)) / Ht + 1.f;
}

void VertexShaderManager::Init()
{
	Dirty();

	memset(&xfmem, 0, sizeof(xfmem));
	memset(&constants, 0 , sizeof(constants));
	ResetView();

	// TODO: should these go inside ResetView()?
	Matrix44::LoadIdentity(s_viewportCorrection);
	memset(g_fProjectionMatrix, 0, sizeof(g_fProjectionMatrix));
	for (int i = 0; i < 4; ++i)
		g_fProjectionMatrix[i*5] = 1.0f;
}

void VertexShaderManager::Shutdown()
{
}

void VertexShaderManager::Dirty()
{
	nTransformMatricesChanged[0] = 0;
	nTransformMatricesChanged[1] = 256;

	nNormalMatricesChanged[0] = 0;
	nNormalMatricesChanged[1] = 96;

	nPostTransformMatricesChanged[0] = 0;
	nPostTransformMatricesChanged[1] = 256;

	nLightsChanged[0] = 0;
	nLightsChanged[1] = 0x80;

	bPosNormalMatrixChanged = true;
	bTexMatricesChanged[0] = true;
	bTexMatricesChanged[1] = true;

	bProjectionChanged = true;

	nMaterialsChanged = 15;

	dirty = true;
}

// Syncs the shader constant buffers with xfmem
// TODO: A cleaner way to control the matrices without making a mess in the parameters field
void VertexShaderManager::SetConstants()
{
	if (nTransformMatricesChanged[0] >= 0)
	{
		int startn = nTransformMatricesChanged[0] / 4;
		int endn = (nTransformMatricesChanged[1] + 3) / 4;
		memcpy(constants.transformmatrices[startn], &xfmem.posMatrices[startn * 4], (endn - startn) * 16);
		dirty = true;
		nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
	}

	if (nNormalMatricesChanged[0] >= 0)
	{
		int startn = nNormalMatricesChanged[0] / 3;
		int endn = (nNormalMatricesChanged[1] + 2) / 3;
		for (int i=startn; i<endn; i++)
		{
			memcpy(constants.normalmatrices[i], &xfmem.normalMatrices[3*i], 12);
		}
		dirty = true;
		nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
	}

	if (nPostTransformMatricesChanged[0] >= 0)
	{
		int startn = nPostTransformMatricesChanged[0] / 4;
		int endn = (nPostTransformMatricesChanged[1] + 3 ) / 4;
		memcpy(constants.posttransformmatrices[startn], &xfmem.postMatrices[startn * 4], (endn - startn) * 16);
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

			if (fabs(light.distatt[0]) < 0.00001f &&
			    fabs(light.distatt[1]) < 0.00001f &&
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
			dstlight.dir[0] = light.ddir[0] * norm;
			dstlight.dir[1] = light.ddir[1] * norm;
			dstlight.dir[2] = light.ddir[2] * norm;
		}
		dirty = true;

		nLightsChanged[0] = nLightsChanged[1] = -1;
	}

	if (nMaterialsChanged)
	{
		for (int i = 0; i < 2; ++i)
		{
			if (nMaterialsChanged & (1 << i))
			{
				u32 data = xfmem.ambColor[i];
				constants.materials[i][0] = (data >> 24) & 0xFF;
				constants.materials[i][1] = (data >> 16) & 0xFF;
				constants.materials[i][2] = (data >>  8) & 0xFF;
				constants.materials[i][3] =  data        & 0xFF;
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (nMaterialsChanged & (1 << (i + 2)))
			{
				u32 data = xfmem.matColor[i];
				constants.materials[i+2][0] = (data >> 24) & 0xFF;
				constants.materials[i+2][1] = (data >> 16) & 0xFF;
				constants.materials[i+2][2] = (data >>  8) & 0xFF;
				constants.materials[i+2][3] =  data        & 0xFF;
			}
		}
		dirty = true;

		nMaterialsChanged = 0;
	}

	if (bPosNormalMatrixChanged)
	{
		bPosNormalMatrixChanged = false;

		const float *pos = (const float *)xfmem.posMatrices + MatrixIndexA.PosNormalMtxIdx * 4;
		const float *norm = (const float *)xfmem.normalMatrices + 3 * (MatrixIndexA.PosNormalMtxIdx & 31);

		memcpy(constants.posnormalmatrix, pos, 3*16);
		memcpy(constants.posnormalmatrix[3], norm, 12);
		memcpy(constants.posnormalmatrix[4], norm+3, 12);
		memcpy(constants.posnormalmatrix[5], norm+6, 12);
		dirty = true;
	}

	if (bTexMatricesChanged[0])
	{
		bTexMatricesChanged[0] = false;
		const float *fptrs[] =
		{
			(const float *)&xfmem.posMatrices[MatrixIndexA.Tex0MtxIdx * 4],
			(const float *)&xfmem.posMatrices[MatrixIndexA.Tex1MtxIdx * 4],
			(const float *)&xfmem.posMatrices[MatrixIndexA.Tex2MtxIdx * 4],
			(const float *)&xfmem.posMatrices[MatrixIndexA.Tex3MtxIdx * 4]
		};

		for (int i = 0; i < 4; ++i)
		{
			memcpy(constants.texmatrices[3*i], fptrs[i], 3*16);
		}
		dirty = true;
	}

	if (bTexMatricesChanged[1])
	{
		bTexMatricesChanged[1] = false;
		const float *fptrs[] = {
			(const float *)&xfmem.posMatrices[MatrixIndexB.Tex4MtxIdx * 4],
			(const float *)&xfmem.posMatrices[MatrixIndexB.Tex5MtxIdx * 4],
			(const float *)&xfmem.posMatrices[MatrixIndexB.Tex6MtxIdx * 4],
			(const float *)&xfmem.posMatrices[MatrixIndexB.Tex7MtxIdx * 4]
		};

		for (int i = 0; i < 4; ++i)
		{
			memcpy(constants.texmatrices[3*i+12], fptrs[i], 3*16);
		}
		dirty = true;
	}

	if (bViewportChanged)
	{
		bViewportChanged = false;
		LogViewport(xfmem.viewport);
		constants.depthparams[0] = xfmem.viewport.farZ / 16777216.0f;
		constants.depthparams[1] = xfmem.viewport.zRange / 16777216.0f;

		// The console GPU places the pixel center at 7/12 unless antialiasing
		// is enabled, while D3D and OpenGL place it at 0.5. See the comment
		// in VertexShaderGen.cpp for details.
		// NOTE: If we ever emulate antialiasing, the sample locations set by
		// BP registers 0x01-0x04 need to be considered here.
		const float pixel_center_correction = 7.0f / 12.0f - 0.5f;
		const float pixel_size_x = 2.f / Renderer::EFBToScaledXf(2.f * xfmem.viewport.wd);
		const float pixel_size_y = 2.f / Renderer::EFBToScaledXf(2.f * xfmem.viewport.ht);
		constants.depthparams[2] = pixel_center_correction * pixel_size_x;
		constants.depthparams[3] = pixel_center_correction * pixel_size_y;
		dirty = true;
		// This is so implementation-dependent that we can't have it here.
		g_renderer->SetViewport();

		// Update projection if the viewport isn't 1:1 useable
		if (!g_ActiveConfig.backend_info.bSupportsOversizedViewports)
		{
			ViewportCorrectionMatrix(s_viewportCorrection);
			bProjectionChanged = true;
		}
	}

	if (bProjectionChanged)
	{
		bProjectionChanged = false;
		LogProj(xfmem.projection.rawProjection);
		SetProjectionConstants();
	}
}

//#pragma optimize("", off)

void VertexShaderManager::SetProjectionConstants()
{
	float *rawProjection = xfmem.projection.rawProjection;

	bool bFullscreenLayer = g_ActiveConfig.bHudFullscreen && xfmem.projection.type != GX_PERSPECTIVE;
	bool bFlashing = (debug_projNum - 1) == g_ActiveConfig.iSelectedLayer;
	bool bStuckToHead = false, bHide = false;
	float fScaleHack = 1, fWidthHack = 1, fHeightHack = 1, fUpHack = 0, fRightHack = 0;

	if (g_ActiveConfig.iMetroidPrime)
	{
		switch (g_metroid_layer)
		{
		case METROID_ECHO_EFFECT:
			bStuckToHead = true;
			fHeightHack = 3.0f;
			fWidthHack = 1.85f;
			fUpHack = 0.15f;
			fRightHack = -0.4f;
			fScaleHack = 0.5;
			break;
		case METROID_THERMAL_EFFECT:
		case METROID_THERMAL_EFFECT_GUN:
			bStuckToHead = true;
			fHeightHack = 4.2f;
			fWidthHack = 2.1f;
			fUpHack = 0;
			fRightHack = -0.4f;
			fScaleHack = 0.01f;
			break;

		case METROID_XRAY_EFFECT:
			bStuckToHead = true;
			fHeightHack = 6.0f;
			fWidthHack = 3.0f;
			fUpHack = 0;
			fRightHack = -0.5f;
			fScaleHack = 0.01f;
			break;
		case METROID_DARK_EFFECT:
			bStuckToHead = true;
			fHeightHack = 3.0f;
			fWidthHack = 1.85f;
			fUpHack = 0.15f;
			fRightHack = -0.4f;
			fScaleHack = 0.5f;
			break;
		case METROID_DARK_CENTRAL:
			bStuckToHead = true;
			break;
		case METROID_VISOR_DIRT:
			// todo: move to helmet depth (hard with a 2D layer)
			bStuckToHead = true;
			bFullscreenLayer = false;
			fScaleHack = 2;
			break;
		case METROID_SCAN_DARKEN:
			bStuckToHead = true;
			bFullscreenLayer = true;
			break;
		case METROID_SCAN_BOX:
			bHide = true;
			break;
		case METROID_SCAN_HIGHLIGHTER:
			// Because of the way EFB copies work in VR,
			// the scan cursor ends up in the corner of the screen.
			// Force the highlighter to almost line up with the object.
			// This only works correctly on DK1!!!
			bStuckToHead = true;
			//bFullscreenLayer = false;
			fHeightHack = 3.0f;
			fWidthHack = 1.8f;
			fUpHack = 0.14f;
			// Should actually be about 0.38, but hard to look at then.
			fRightHack = -0.36f;
			break;
		case METROID_SCAN_RETICLE:
			// Because of the way EFB copies work in VR,
			// the scan cursor ends up in the corner of the screen.
			// So make it bigger and easier to see.
			fHeightHack = 1.5f;
			fWidthHack = 1.5f;
			bStuckToHead = true;
			fUpHack = 0.14f;
			// Should actually be about 0.38, but hard to look at then.
			fRightHack = -0.35f;
			break;
		case METROID_MORPHBALL_HUD:
		case METROID_MORPHBALL_MAP_OR_HINT:
		case METROID_MORPHBALL_MAP:
			bStuckToHead = false;
			fScaleHack = 20.0f;
			fWidthHack = 0.8f;
			fHeightHack = 0.8f;
			break;
		case METROID_UNKNOWN:
		case METROID_WORLD:
		case METROID_MORPHBALL_WORLD:
		case METROID_XRAY_WORLD:
		case METROID_THERMAL_GUN_AND_DOOR:
		case METROID_RETICLE:
		case METROID_SCAN_ICONS:
		case METROID_SCAN_CROSS:
		case METROID_UNKNOWN_WORLD:
		case METROID_UNKNOWN_2D:
		case METROID_SHADOW_2D:
			bStuckToHead = false;
			break;
		case METROID_MAP_0:
		case METROID_MAP_1:
		case METROID_MAP_2:
			bStuckToHead = false;
			fScaleHack = 0.02f;
			break;

		case METROID_MAP_LEGEND:
			bStuckToHead = false;
			fScaleHack = 30;
			break;
		case METROID_MAP_MAP:
		case METROID_MAP_NORTH:
			bStuckToHead = false;
			break;
		case METROID_XRAY_HUD:
			bStuckToHead = false;
			fScaleHack = 30;
			fWidthHack = 0.78f;
			fHeightHack = 0.78f;
			break;
		case METROID_UNKNOWN_HUD:
		case METROID_HUD:
			bStuckToHead = true;
			fScaleHack = 30;
			fWidthHack = 0.78f;
			fHeightHack = 0.78f;
			break;
		case METROID_VISOR_RADAR_HINT:
		case METROID_RADAR_DOT:
			bStuckToHead = true;
			fScaleHack = 32;
			fWidthHack = 0.78f;
			fHeightHack = 0.78f;
			break;
		case METROID_MAP_OR_HINT:
		case METROID_MAP:
			bStuckToHead = true;
			fScaleHack = 30;
			fWidthHack = 0.78f;
			fHeightHack = 0.78f;
			break;
		case METROID_HELMET:
			bStuckToHead = true;
			fScaleHack = 100; // 1/100
			fWidthHack = 1.7f;
			fHeightHack = 1.5f;
			break;
		case METROID_SCAN_TEXT:
			// the text box (and side boxes) seen when scanning
			// they should be almost within reach.
			bStuckToHead = true;
			fScaleHack = 32;
			fWidthHack = 0.8f;
			fHeightHack = 0.8f;
			break;
		case METROID_SCAN_HOLOGRAM:
			// move the hologram inside the box when scanning ship
			// and make it bigger
			bStuckToHead = true;
			fScaleHack = 40;
			fWidthHack = 1.2f;
			fHeightHack = 1.2f;
			break;
		case METROID_INVENTORY_SAMUS:
			// move the hologram inside the box when scanning ship
			// and make it bigger
			bStuckToHead = false;
			break;
		default:
			bStuckToHead = true;
			fScaleHack = 30; // 1/30
			break;
		}
	}

	bHide = bHide || (bFlashing && g_ActiveConfig.iFlashState > 5);

	// Split WidthHack and HeightHack into left and right versions for telescopes
	float fLeftWidthHack = fWidthHack, fRightWidthHack = fWidthHack;
	float fLeftHeightHack = fHeightHack, fRightHeightHack = fHeightHack;
	bool bHideLeft = bHide, bHideRight = bHide, bTelescopeHUD = false, bNoForward = false;
	if (g_ActiveConfig.iTelescopeEye && vr_widest_3d_VFOV <= g_ActiveConfig.fTelescopeMaxFOV && vr_widest_3d_VFOV > 1 )
	{
		bNoForward = true;
		// Calculate telescope scale
		float hmd_halftan, telescope_scale;
#ifdef HAVE_OCULUSSDK
		if (g_has_rift)
		{
			hmd_halftan = fabs(g_eye_fov[0].LeftTan);
			if (fabs(g_eye_fov[0].RightTan) > hmd_halftan)
				hmd_halftan = fabs(g_eye_fov[0].RightTan);
			if (fabs(g_eye_fov[0].UpTan) > hmd_halftan)
				hmd_halftan = fabs(g_eye_fov[0].UpTan);
			if (fabs(g_eye_fov[0].DownTan) > hmd_halftan)
				hmd_halftan = fabs(g_eye_fov[0].DownTan);
			//hmd_halftan;
		}
		else
#endif
		{
			hmd_halftan = tan(DEGREES_TO_RADIANS(32.0f / 2))*3.0f / 4.0f;
		}
		telescope_scale = fabs(hmd_halftan / tan(DEGREES_TO_RADIANS(vr_widest_3d_VFOV) / 2));
		if (xfmem.projection.type != GX_PERSPECTIVE)
		{
			bHideLeft = true; 
			bHideRight = true;
			bStuckToHead = true;
			bTelescopeHUD = true;
		}
		if (g_ActiveConfig.iTelescopeEye & 1)
		{
			fLeftWidthHack *= telescope_scale;
			fLeftHeightHack *= telescope_scale;
			bHideLeft = false;
		}
		if (g_ActiveConfig.iTelescopeEye & 2)
		{
			fRightWidthHack *= telescope_scale;
			fRightHeightHack *= telescope_scale;
			bHideRight = false;
		}
	}

	switch (xfmem.projection.type)
	{
	case GX_PERSPECTIVE:

		g_fProjectionMatrix[0] = rawProjection[0] * g_ActiveConfig.fAspectRatioHackW;
		g_fProjectionMatrix[1] = 0.0f;
		g_fProjectionMatrix[2] = rawProjection[1];
		g_fProjectionMatrix[3] = 0.0f;

		g_fProjectionMatrix[4] = 0.0f;
		g_fProjectionMatrix[5] = rawProjection[2] * g_ActiveConfig.fAspectRatioHackH;
		g_fProjectionMatrix[6] = rawProjection[3];
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
		g_fProjectionMatrix[10] = (g_ProjHack1.value + rawProjection[4]) * ((g_ProjHack1.sign == 0) ? 1.0f : g_ProjHack1.sign);
		g_fProjectionMatrix[11] = (g_ProjHack2.value + rawProjection[5]) * ((g_ProjHack2.sign == 0) ? 1.0f : g_ProjHack2.sign);

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

	PRIM_LOG("Projection: %f %f %f %f %f %f\n", rawProjection[0], rawProjection[1], rawProjection[2], rawProjection[3], rawProjection[4], rawProjection[5]);

	float UnitsPerMetre = g_ActiveConfig.fUnitsPerMetre * fScaleHack;

	// VR Oculus Rift 3D projection matrix, needs to include head-tracking
	if (g_has_hmd && !bFullscreenLayer)
	{
		float *p = rawProjection;
		// near clipping plane in game units
		float zfar, znear, zNear3D, hfov, vfov;

		// Real 3D scene
		if (xfmem.projection.type == GX_PERSPECTIVE)
		{
			zfar = p[5] / p[4];
			znear = (1 + p[5]) / p[4];
			float zn2 = p[5] / (p[4] - 1);
			float zf2 = p[5] / (p[4] + 1);
			hfov = 2 * atan(1.0f / p[0])*180.0f / 3.1415926535f;
			vfov = 2 * atan(1.0f / p[2])*180.0f / 3.1415926535f;
			if (debug_newScene)
				INFO_LOG(VR, "Real 3D scene: hfov=%8.4f    vfov=%8.4f      znear=%8.4f or %8.4f   zfar=%8.4f or %8.4f", hfov, vfov, znear, zn2, zfar, zf2);
			// prevent near z-clipping by moving near clipping plane closer (may cause z-fighting though)
			// needed for Animal Crossing on GameCube
			// znear *= 0.3f;
		}
		// 2D layer we will turn into a 3D scene
		else
		{
			if (vr_widest_3d_HFOV > 0) {
				znear = vr_widest_3d_zNear;
				zfar = vr_widest_3d_zFar;
				hfov = vr_widest_3d_HFOV;
				vfov = vr_widest_3d_VFOV;
				if (debug_newScene)
					INFO_LOG(VR, "2D to fit 3D world: hfov=%8.4f    vfov=%8.4f      znear=%8.4f   zfar=%8.4f", hfov, vfov, znear, zfar);
			}
			else { // default, if no 3D in scene
				znear = 0.2f*UnitsPerMetre * 20; // 50cm
				zfar = 40 *UnitsPerMetre; // 40m
				hfov = 70; // 70 degrees
				if (g_aspect_wide)
					vfov = 180.0f / 3.14159f * 2 * atanf(tanf((hfov*3.14159f / 180.0f) / 2)* 9.0f / 16.0f); // 2D screen is meant to be 16:9 aspect ratio
				else
					vfov = 180.0f / 3.14159f * 2 * atanf(tanf((hfov*3.14159f / 180.0f) / 2)* 3.0f / 4.0f); //  2D screen is meant to be 4:3 aspect ratio, make it the same width but taller
				if (debug_newScene)
					ERROR_LOG(VR, "Only 2D Projecting: %g x %g, n=%fm f=%fm", hfov, vfov, znear, zfar);
			}
			zNear3D = znear;
			znear /= 40.0f;
			if (debug_newScene)
				WARN_LOG(VR, "2D: zNear3D = %f, znear = %f, zFar = %f", zNear3D, znear, zfar);
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
			g_fProjectionMatrix[11] = -zfar*znear / (zfar - znear);
			if (debug_newScene)
				WARN_LOG(VR, "2D: m[2][2]=%f m[2][3]=%f ", g_fProjectionMatrix[10], g_fProjectionMatrix[11]);

			g_fProjectionMatrix[12] = 0.0f;
			g_fProjectionMatrix[13] = 0.0f;
			// donkopunchstania suggested the GC GPU might round differently
			// He had thus changed this to -(1 + epsilon) to fix clipping issues.
			// I (neobrain) don't think his conjecture is true and thus reverted his change.
			g_fProjectionMatrix[14] = -1.0f;
			g_fProjectionMatrix[15] = 0.0f;

		}

		Matrix44 proj_left, proj_right;
		Matrix44::Set(proj_left, g_fProjectionMatrix);
		Matrix44::Set(proj_right, g_fProjectionMatrix);
		if (g_has_vr920)
		{
			// 32 degrees HFOV, 4:3 aspect ratio
			proj_left.data[0 * 4 + 0] = 1.0f / tan(32.0f / 2.0f * 3.1415926535f / 180.0f);
			proj_left.data[1 * 4 + 1] = 4.0f / 3.0f * proj_left.data[0 * 4 + 0];
			proj_right.data[0 * 4 + 0] = 1.0f / tan(32.0f / 2.0f * 3.1415926535f / 180.0f);
			proj_right.data[1 * 4 + 1] = 4.0f / 3.0f * proj_right.data[0 * 4 + 0];
			if (debug_newScene)
				NOTICE_LOG(VR, "Using VR920 FOV");
		}
#ifdef HAVE_OCULUSSDK
		else if (g_has_rift)
		{
			if (debug_newScene)
				INFO_LOG(VR, "g_has_rift");
			ovrMatrix4f rift_left = ovrMatrix4f_Projection(g_eye_fov[0], znear, zfar, true);
			ovrMatrix4f rift_right = ovrMatrix4f_Projection(g_eye_fov[1], znear, zfar, true);
			if (xfmem.projection.type != GX_PERSPECTIVE)
			{
				//proj_left.data[2 * 4 + 2] = rift_left.M[2][2];
				//proj_left.data[2 * 4 + 3] = rift_left.M[2][3];
				//proj_right.data[2 * 4 + 2] = rift_right.M[2][2];
				//proj_right.data[2 * 4 + 3] = rift_right.M[2][3];
			}

			float hfov2 = 2 * atan(1.0f / rift_left.M[0][0])*180.0f / 3.1415926535f;
			float vfov2 = 2 * atan(1.0f / rift_left.M[1][1])*180.0f / 3.1415926535f;
			float zfar2 = rift_left.M[2][3] / rift_left.M[2][2];
			float znear2 = (1 + rift_left.M[2][2] * zfar) / rift_left.M[2][2];
			if (debug_newScene)
			{
				// yellow = Oculus's suggestion
				DEBUG_LOG(VR, "hfov=%8.4f    vfov=%8.4f      znear=%8.4f   zfar=%8.4f", hfov2, vfov2, znear2, zfar2);
				DEBUG_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", rift_left.M[0][0], rift_left.M[0][1], rift_left.M[0][2], rift_left.M[0][3]);
				DEBUG_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", rift_left.M[1][0], rift_left.M[1][1], rift_left.M[1][2], rift_left.M[1][3]);
				DEBUG_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", rift_left.M[2][0], rift_left.M[2][1], rift_left.M[2][2], rift_left.M[2][3]);
				DEBUG_LOG(VR, "{%8.4f %8.4f %8.4f   %8.4f}", rift_left.M[3][0], rift_left.M[3][1], rift_left.M[3][2], rift_left.M[3][3]);
				// green = Game's suggestion
				INFO_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[0 * 4 + 0], proj_left.data[0 * 4 + 1], proj_left.data[0 * 4 + 2], proj_left.data[0 * 4 + 3]);
				INFO_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[1 * 4 + 0], proj_left.data[1 * 4 + 1], proj_left.data[1 * 4 + 2], proj_left.data[1 * 4 + 3]);
				INFO_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[2 * 4 + 0], proj_left.data[2 * 4 + 1], proj_left.data[2 * 4 + 2], proj_left.data[2 * 4 + 3]);
				INFO_LOG(VR, "{%8.4f %8.4f %8.4f   %8.4f}", proj_left.data[3 * 4 + 0], proj_left.data[3 * 4 + 1], proj_left.data[3 * 4 + 2], proj_left.data[3 * 4 + 3]);
			}
			// red = my combination
			proj_left.data[0 * 4 + 0] = rift_left.M[0][0] * SignOf(proj_left.data[0 * 4 + 0]) * fLeftWidthHack; // h fov
			proj_left.data[1 * 4 + 1] = rift_left.M[1][1] * SignOf(proj_left.data[1 * 4 + 1]) * fLeftHeightHack; // v fov
			proj_left.data[0 * 4 + 2] = rift_left.M[0][2] * SignOf(proj_left.data[0 * 4 + 0]) - fRightHack; // h off-axis
			proj_left.data[1 * 4 + 2] = rift_left.M[1][2] * SignOf(proj_left.data[1 * 4 + 1]) - fUpHack; // v off-axis
			proj_right.data[0 * 4 + 0] = rift_right.M[0][0] * SignOf(proj_right.data[0 * 4 + 0]) * fRightWidthHack;
			proj_right.data[1 * 4 + 1] = rift_right.M[1][1] * SignOf(proj_right.data[1 * 4 + 1]) * fRightHeightHack;
			proj_right.data[0 * 4 + 2] = rift_right.M[0][2] * SignOf(proj_right.data[0 * 4 + 0]) - fRightHack;
			proj_right.data[1 * 4 + 2] = rift_right.M[1][2] * SignOf(proj_right.data[1 * 4 + 1]) - fUpHack;
			if (debug_newScene)
			{
				DEBUG_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[0 * 4 + 0], proj_left.data[0 * 4 + 1], proj_left.data[0 * 4 + 2], proj_left.data[0 * 4 + 3]);
				DEBUG_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[1 * 4 + 0], proj_left.data[1 * 4 + 1], proj_left.data[1 * 4 + 2], proj_left.data[1 * 4 + 3]);
				DEBUG_LOG(VR, "[%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[2 * 4 + 0], proj_left.data[2 * 4 + 1], proj_left.data[2 * 4 + 2], proj_left.data[2 * 4 + 3]);
				DEBUG_LOG(VR, "{%8.4f %8.4f %8.4f   %8.4f}", proj_left.data[3 * 4 + 0], proj_left.data[3 * 4 + 1], proj_left.data[3 * 4 + 2], proj_left.data[3 * 4 + 3]);
			}
		}
#endif
		//VR Headtracking
		Matrix44 rotation_matrix;
		if (bStuckToHead)
		{
			Matrix44::LoadIdentity(rotation_matrix);
		}
		else
		{
			UpdateHeadTrackingIfNeeded();
			float extra_pitch;
			if (xfmem.projection.type == GX_PERSPECTIVE || vr_widest_3d_HFOV > 0)
				extra_pitch = g_ActiveConfig.fCameraPitch;
			else
				extra_pitch = g_ActiveConfig.fScreenPitch;
			Matrix33 pitch_matrix33;
			Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
			Matrix44 pitch_matrix;
			Matrix44::LoadMatrix33(pitch_matrix, pitch_matrix33);
			Matrix44::Multiply(g_head_tracking_matrix, pitch_matrix, rotation_matrix);
		}
		//VR sometimes yaw needs to be inverted for games that use a flipped x axis
		// (ActionGirlz even uses flipped matrices and non-flipped matrices in the same frame)
		if (xfmem.projection.type == GX_PERSPECTIVE)
		{
			if (rawProjection[0]<0)
			{
				if (debug_newScene)
					INFO_LOG(VR, "flipped X");
				// flip all the x axis values, except x squared (data[0])
				//Needed for Action Girlz Racing, Backyard Baseball
				rotation_matrix.data[1] *= -1;
				rotation_matrix.data[2] *= -1;
				rotation_matrix.data[3] *= -1;
				rotation_matrix.data[4] *= -1;
				rotation_matrix.data[8] *= -1;
				rotation_matrix.data[12] *= -1;
			}
			if (rawProjection[2]<0)
			{
				if (debug_newScene)
					INFO_LOG(VR, "flipped Y");
				// flip all the y axis values, except y squared (data[5])
				// Needed for ABBA
				rotation_matrix.data[1] *= -1;
				rotation_matrix.data[4] *= -1;
				rotation_matrix.data[6] *= -1;
				rotation_matrix.data[7] *= -1;
				rotation_matrix.data[9] *= -1;
				rotation_matrix.data[13] *= -1;
			}
		}

		Matrix44 walk_matrix, look_matrix;
		if (bStuckToHead)
		{
			Matrix44::LoadIdentity(walk_matrix);
		}
		else
		{
			float pos[3];
			for (int i = 0; i < 3; ++i)
				pos[i] = s_fViewTranslationVector[i] + g_head_tracking_position[i] * UnitsPerMetre;
			if (!bNoForward)
				pos[2] += g_ActiveConfig.fCameraForward * UnitsPerMetre;
			static int x = 0;
			x++;
			Matrix44::Translate(walk_matrix, pos);
			if (x>100)
			{
				x = 0;
				//			NOTICE_LOG(VR, "walk pos = %f, %f, %f", s_fViewTranslationVector[0], s_fViewTranslationVector[1], s_fViewTranslationVector[2]);
				INFO_LOG(VR, "head pos = %5.0fcm, %5.0fcm, %5.0fcm, walk %5.1f, %5.1f, %5.1f", 100 * g_head_tracking_position[0], 100 * g_head_tracking_position[1], 100 * g_head_tracking_position[2], s_fViewTranslationVector[0], s_fViewTranslationVector[1], s_fViewTranslationVector[2]);
			}
		}

		if (xfmem.projection.type == GX_PERSPECTIVE)
		{
			if (debug_newScene)
				INFO_LOG(VR, "3D: do normally");
			Matrix44::Multiply(rotation_matrix, walk_matrix, look_matrix);
		}
		else
		if (xfmem.projection.type != GX_PERSPECTIVE)
		{
			if (debug_newScene)
				INFO_LOG(VR, "2D: hacky test");

			float HudWidth, HudHeight, HudThickness, HudDistance, HudUp, CameraForward, AimDistance;

			// 2D Screen
			if (vr_widest_3d_HFOV <= 0)
			{
				HudThickness = g_ActiveConfig.fScreenThickness * UnitsPerMetre;
				HudDistance = g_ActiveConfig.fScreenDistance * UnitsPerMetre;
				HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
				HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
				if (g_aspect_wide)
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
				// Give the 2D layer a 3D effect if different parts of the 2D layer are rendered at different z coordinates
				HudThickness = g_ActiveConfig.fHudThickness * UnitsPerMetre;  // the 2D layer is actually a 3D box this many game units thick
				HudDistance = g_ActiveConfig.fHudDistance * UnitsPerMetre;   // depth 0 on the HUD should be this far away
				HudUp = 0;
				if (bNoForward)
					CameraForward = 0;
				else
					CameraForward = g_ActiveConfig.fCameraForward * UnitsPerMetre;
				// When moving the camera forward, correct the size of the HUD so that aiming is correct at AimDistance
				AimDistance = g_ActiveConfig.fAimDistance * UnitsPerMetre;
				if (AimDistance <= 0)
					AimDistance = HudDistance;
				HudWidth = 2.0f * tanf(DEGREES_TO_RADIANS(hfov / 2.0f)) * HudDistance * (AimDistance + CameraForward) / AimDistance;
				HudHeight = 2.0f * tanf(DEGREES_TO_RADIANS(vfov / 2.0f)) * HudDistance * (AimDistance + CameraForward) / AimDistance;
			}



			// Now that we know how far away the box is, and what FOV it should fill, we can work out the width and height in game units
			// Note: the HUD won't line up exactly (except at AimDistance) if CameraForward is non-zero 
			//float HudWidth = 2.0f * tanf(hfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
			//float HudHeight = 2.0f * tanf(vfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;

			float left2D = -(rawProjection[1] + 1) / rawProjection[0];
			float right2D = left2D + 2 / rawProjection[0];
			float bottom2D = -(rawProjection[3] + 1) / rawProjection[2];
			float top2D = bottom2D + 2 / rawProjection[2];
			float zFar2D = rawProjection[5] / rawProjection[4];
			float zNear2D = (1 + rawProjection[4] * zFar2D) / rawProjection[4];

			float scale[3]; // width, height, and depth of box in game units divided by 2D width, height, and depth 
			float position[3]; // position of front of box relative to the camera, in game units 
			if (rawProjection[0] == 0 || right2D == left2D) {
				scale[0] = 0;
			}
			else {
				scale[0] = HudWidth / (right2D - left2D);
			}
			if (rawProjection[2] == 0 || top2D == bottom2D) {
				scale[1] = 0;
			}
			else {
				scale[1] = HudHeight / (top2D - bottom2D); // note that positive means up in 3D
			}
			if (rawProjection[4] == 0 || zFar2D == zNear2D) {
				scale[2] = 0; // The 2D layer was flat, so we make it flat instead of a box to avoid dividing by zero
			}
			else {
				scale[2] = HudThickness / (zFar2D - zNear2D); // Scale 2D z values into 3D game units so it is the right thickness
			}
			position[0] = scale[0] * (-(right2D + left2D) / 2.0f); // shift it left into the centre of the view
			position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + HudUp; // shift it up into the centre of the view;
			position[2] = -HudDistance -CameraForward;
			Matrix44 scale_matrix, position_matrix, box_matrix, temp_matrix;
			Matrix44::Scale(scale_matrix, scale);
			Matrix44::Translate(position_matrix, position);

			// order: scale, walk, rotate
			Matrix44::Multiply(rotation_matrix, walk_matrix, temp_matrix);
			Matrix44::Multiply(position_matrix, scale_matrix, box_matrix);
			Matrix44::Multiply(temp_matrix, box_matrix, look_matrix);
		}

		Matrix44 eye_pos_matrix_left, eye_pos_matrix_right;
		float posLeft[3] = { 0, 0, 0 };
		float posRight[3] = { 0, 0, 0 };
#ifdef HAVE_OCULUSSDK
		if (g_has_rift && !bTelescopeHUD)
		{
			posLeft[0] = g_eye_render_desc[0].ViewAdjust.x * UnitsPerMetre;
			posLeft[1] = g_eye_render_desc[0].ViewAdjust.y * UnitsPerMetre;
			posLeft[2] = g_eye_render_desc[0].ViewAdjust.z * UnitsPerMetre;
			posRight[0] = g_eye_render_desc[1].ViewAdjust.x * UnitsPerMetre;
			posRight[1] = g_eye_render_desc[1].ViewAdjust.y * UnitsPerMetre;
			posRight[2] = g_eye_render_desc[1].ViewAdjust.z * UnitsPerMetre;
		}
#endif
		Matrix44::Translate(eye_pos_matrix_left, posLeft);
		Matrix44::Translate(eye_pos_matrix_right, posRight);

		Matrix44 view_matrix_left, view_matrix_right;
		Matrix44::Multiply(eye_pos_matrix_left, look_matrix, view_matrix_left);
		Matrix44::Multiply(eye_pos_matrix_right, look_matrix, view_matrix_right);

		Matrix44 final_matrix_left, final_matrix_right;
		Matrix44::Multiply(proj_left, view_matrix_left, final_matrix_left);
		Matrix44::Multiply(proj_right, view_matrix_right, final_matrix_right);
		// If we are supposed to hide the layer, zero out the projection matrix
		if (bHideLeft) {
			memset(final_matrix_left.data, 0, 16 * sizeof(final_matrix_left.data[0]));
		}
		if (bHideRight)
		{
			memset(final_matrix_right.data, 0, 16 * sizeof(final_matrix_right.data[0]));
		}

		memcpy(constants.projection, final_matrix_left.data, 4 * 16);
		memcpy(constants_eye_projection[0], final_matrix_left.data, 4 * 16);
		memcpy(constants_eye_projection[1], final_matrix_right.data, 4 * 16);
		//Matrix44 mtxA, mtxB, mtxView;
		//if (MetroidPrime_DrawingHelmet) {
		//	if (vr_render_eye == 0) // mono
		//		Matrix44::LoadIdentity(mtxA);
		//	else { // 3D
		//		float v[3] = { 8.0f, 0.3f, 3.0f }; // move camera left into left eye hole
		//		if (vr_render_eye>0) v[0] = -v[0]; // or right
		//		Matrix44::Translate(mtxA, v);
		//	}
		//	Matrix44::LoadIdentity(mtxB); // keep locked to head
		//	Matrix44::Multiply(mtxB, mtxA, mtxView);
		//	Matrix44::Set(projMtx, g_fProjectionMatrix);
		//	Matrix44::Multiply(projMtx, mtxView, rotatedMtx);
		//}
		//else if (MetroidPrime_DrawingVisor) {
		//	if (vr_render_eye == 0) // mono
		//		Matrix44::LoadIdentity(mtxA);
		//	else { // 3D
		//		float v[3] = { 8.1f, 0.0f, 3.0f }; // move camera left into left eye hole
		//		if (vr_render_eye>0) v[0] = -v[0]; // or right
		//		Matrix44::Translate(mtxA, v);
		//	}
		//	Matrix44::LoadIdentity(mtxB); // keep locked to head
		//	Matrix44::Multiply(mtxB, mtxA, mtxView);
		//	Matrix44::Set(projMtx, g_fProjectionMatrix);
		//	Matrix44::Multiply(projMtx, mtxView, rotatedMtx);
		//}
		//else { //VR Normal Oculus Rift Headtracking
		//	Matrix44::Set(projMtx, g_fProjectionMatrix);
		//	Matrix44::Multiply(projMtx, g_ActiveConfig.mHeadTracking, rotatedMtx);
		//}
	}
	else if ((g_ActiveConfig.bFreeLook || g_ActiveConfig.bAnaglyphStereo) && xfmem.projection.type == GX_PERSPECTIVE)
	{
		Matrix44 mtxA;
		Matrix44 mtxB;
		Matrix44 viewMtx;

		Matrix44::Translate(mtxA, s_fViewTranslationVector);
		Matrix44::LoadMatrix33(mtxB, s_viewRotationMatrix);
		Matrix44::Multiply(mtxB, mtxA, viewMtx); // view = rotation x translation
		Matrix44::Set(mtxB, g_fProjectionMatrix);
		Matrix44::Multiply(mtxB, viewMtx, mtxA); // mtxA = projection x view
		Matrix44::Multiply(s_viewportCorrection, mtxA, mtxB); // mtxB = viewportCorrection x mtxA

		// If we are supposed to hide the layer, zero out the projection matrix
		if (bHide) {
			memset(mtxB.data, 0, 16 * sizeof(mtxB.data[0]));
		}
		memcpy(constants.projection, mtxB.data, 4 * 16);
		memcpy(constants_eye_projection[0], mtxB.data, 4 * 16);
		memcpy(constants_eye_projection[1], mtxB.data, 4 * 16);
	}
	else
	{
		Matrix44 projMtx;
		Matrix44::Set(projMtx, g_fProjectionMatrix);

		Matrix44 correctedMtx;
		Matrix44::Multiply(s_viewportCorrection, projMtx, correctedMtx);
		// If we are supposed to hide the layer, zero out the projection matrix
		if (bHide) {
			memset(correctedMtx.data, 0, 16 * sizeof(correctedMtx.data[0]));
		}
		memcpy(constants.projection, correctedMtx.data, 4 * 16);
		memcpy(constants_eye_projection[0], correctedMtx.data, 4 * 16);
		memcpy(constants_eye_projection[1], correctedMtx.data, 4 * 16);
	}
	dirty = true;
}
//#pragma optimize("", on)


void VertexShaderManager::InvalidateXFRange(int start, int end)
{
	if (((u32)start >= (u32)MatrixIndexA.PosNormalMtxIdx * 4 &&
		 (u32)start <  (u32)MatrixIndexA.PosNormalMtxIdx * 4 + 12) ||
		((u32)start >= XFMEM_NORMALMATRICES + ((u32)MatrixIndexA.PosNormalMtxIdx & 31) * 3 &&
		 (u32)start <  XFMEM_NORMALMATRICES + ((u32)MatrixIndexA.PosNormalMtxIdx & 31) * 3 + 9))
	{
		bPosNormalMatrixChanged = true;
	}

	if (((u32)start >= (u32)MatrixIndexA.Tex0MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex0MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexA.Tex1MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex1MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexA.Tex2MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex2MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexA.Tex3MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex3MtxIdx*4+12))
	{
		bTexMatricesChanged[0] = true;
	}

	if (((u32)start >= (u32)MatrixIndexB.Tex4MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex4MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexB.Tex5MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex5MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexB.Tex6MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex6MtxIdx*4+12) ||
		((u32)start >= (u32)MatrixIndexB.Tex7MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex7MtxIdx*4+12))
	{
		bTexMatricesChanged[1] = true;
	}

	if (start < XFMEM_POSMATRICES_END)
	{
		if (nTransformMatricesChanged[0] == -1)
		{
			nTransformMatricesChanged[0] = start;
			nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
		}
		else
		{
			if (nTransformMatricesChanged[0] > start) nTransformMatricesChanged[0] = start;
			if (nTransformMatricesChanged[1] < end) nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
		}
	}

	if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES)
	{
		int _start = start < XFMEM_NORMALMATRICES ? 0 : start-XFMEM_NORMALMATRICES;
		int _end = end < XFMEM_NORMALMATRICES_END ? end-XFMEM_NORMALMATRICES : XFMEM_NORMALMATRICES_END-XFMEM_NORMALMATRICES;

		if (nNormalMatricesChanged[0] == -1)
		{
			nNormalMatricesChanged[0] = _start;
			nNormalMatricesChanged[1] = _end;
		}
		else
		{
			if (nNormalMatricesChanged[0] > _start) nNormalMatricesChanged[0] = _start;
			if (nNormalMatricesChanged[1] < _end) nNormalMatricesChanged[1] = _end;
		}
	}

	if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES)
	{
		int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start-XFMEM_POSTMATRICES;
		int _end = end < XFMEM_POSTMATRICES_END ? end-XFMEM_POSTMATRICES : XFMEM_POSTMATRICES_END-XFMEM_POSTMATRICES;

		if (nPostTransformMatricesChanged[0] == -1)
		{
			nPostTransformMatricesChanged[0] = _start;
			nPostTransformMatricesChanged[1] = _end;
		}
		else
		{
			if (nPostTransformMatricesChanged[0] > _start) nPostTransformMatricesChanged[0] = _start;
			if (nPostTransformMatricesChanged[1] < _end) nPostTransformMatricesChanged[1] = _end;
		}
	}

	if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
	{
		int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start-XFMEM_LIGHTS;
		int _end = end < XFMEM_LIGHTS_END ? end-XFMEM_LIGHTS : XFMEM_LIGHTS_END-XFMEM_LIGHTS;

		if (nLightsChanged[0] == -1 )
		{
			nLightsChanged[0] = _start;
			nLightsChanged[1] = _end;
		}
		else
		{
			if (nLightsChanged[0] > _start) nLightsChanged[0] = _start;
			if (nLightsChanged[1] < _end)   nLightsChanged[1] = _end;
		}
	}
}

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
{
	if (MatrixIndexA.Hex != Value)
	{
		VertexManager::Flush();
		if (MatrixIndexA.PosNormalMtxIdx != (Value&0x3f))
			bPosNormalMatrixChanged = true;
		bTexMatricesChanged[0] = true;
		MatrixIndexA.Hex = Value;
	}
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
	if (MatrixIndexB.Hex != Value)
	{
		VertexManager::Flush();
		bTexMatricesChanged[1] = true;
		MatrixIndexB.Hex = Value;
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

void VertexShaderManager::SetMaterialColorChanged(int index, u32 color)
{
	nMaterialsChanged  |= (1 << index);
}

void VertexShaderManager::TranslateView(float x, float y, float z)
{
	float result[3];
	float vector[3] = { x,z,y };

	Matrix33::Multiply(s_viewInvRotationMatrix, vector, result);

	for (int i = 0; i < 3; i++)
		s_fViewTranslationVector[i] += result[i];

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

	bProjectionChanged = true;
}

void VertexShaderManager::ResetView()
{
	memset(s_fViewTranslationVector, 0, sizeof(s_fViewTranslationVector));
	Matrix33::LoadIdentity(s_viewRotationMatrix);
	Matrix33::LoadIdentity(s_viewInvRotationMatrix);
	s_fViewRotation[0] = s_fViewRotation[1] = 0.0f;

	bProjectionChanged = true;
}

void VertexShaderManager::DoState(PointerWrap &p)
{
	p.Do(g_fProjectionMatrix);
	p.Do(s_viewportCorrection);
	p.Do(s_viewRotationMatrix);
	p.Do(s_viewInvRotationMatrix);
	p.Do(s_fViewTranslationVector);
	p.Do(s_fViewRotation);
	p.Do(constants);
	p.Do(dirty);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		Dirty();
	}
}
