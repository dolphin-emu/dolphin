// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <sstream>

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "VideoCommon/MetroidVR.h"
#include "VideoCommon/VR.h"

// helmet, combat visor, radar dot, map, scan visor, scan text, and scan hologram should be attached to face
// unknown, world, and reticle should NOT be attached to face

TMetroidLayer g_metroid_layer;
bool g_metroid_scan_visor = false, g_metroid_scan_visor_active = false, 
g_metroid_xray_visor = false, g_metroid_thermal_visor = false,
g_metroid_map_screen = false, g_metroid_inventory = false,
g_metroid_dark_visor = false, g_metroid_echo_visor = false, g_metroid_morphball_active = false,
g_metroid_is_demo1 = false;
int g_metroid_wide_count = 0, g_metroid_normal_count = 0;
int g_zelda_normal_count = 0, g_zelda_effect_count = 0;

extern float vr_widest_3d_HFOV, vr_widest_3d_VFOV, vr_widest_3d_zNear, vr_widest_3d_zFar;

//#pragma optimize("", off)

void NewMetroidFrame()
{
	g_metroid_wide_count = 0;
	g_metroid_normal_count = 0;
	g_zelda_normal_count = 0;
	g_zelda_effect_count = 0;
}

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
	case ZELDA_DARK_EFFECT:
		return "Zelda Dark Effect";
	case ZELDA_DIALOG:
		return "Zelda Dialog";
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
	case ZELDA_WORLD:
		return "Zelda World";
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

	if (v == h && v < 100 && h < 100 && layer == 0)
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
				else if (g_metroid_dark_visor)
				{
					result = METROID_HUD;
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
	else if (layer == 3)
	{
		if (l == -32000 && n == -409600)
		{
			result = METROID_SCAN_DARKEN;
		}
		else
		{
			result = METROID_UNKNOWN_2D;
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


TMetroidLayer GetZeldaTPGCLayer2D(int layer, float left, float right, float top, float bottom, float znear, float zfar)
{
	TMetroidLayer result;
	int r = Round100(right);
	int n = Round100(znear);
	int f = Round100(zfar);
	if (r == 400 && f == 1000)
	{
		result = ZELDA_DARK_EFFECT;
	}
	else if (r == 60800 && f == 10000)
	{
		result = ZELDA_UNKNOWN_EFFECT;
	}
	else if (r == 60800 && n == 10000000 && g_zelda_normal_count)
	{
		++g_zelda_effect_count;
		switch (g_zelda_effect_count)
		{
		case 1:
			result = ZELDA_DIALOG;
			break;
		case 2:
			result = ZELDA_NEXT;
			break;
		default:
			result = ZELDA_UNKNOWN_2D;
		}
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

	++g_zelda_normal_count;
	switch (g_zelda_normal_count)
	{
	case 1:
		result = ZELDA_WORLD;
		break;
	case 2:
		result = ZELDA_REFLECTION;
		break;
	default:
		result = ZELDA_UNKNOWN;
		break;
	}
	return result;
}

void GetMetroidPrimeValues(bool *bStuckToHead, bool *bFullscreenLayer, bool *bHide, bool *bFlashing,
	float* fScaleHack, float *fWidthHack, float *fHeightHack, float *fUpHack, float *fRightHack)
{
	switch (g_metroid_layer)
	{
	case METROID_SHADOWS:
	case METROID_BODY_SHADOWS:
		*bStuckToHead = false;
		//*bHide = true;
	case METROID_ECHO_EFFECT:
		*bStuckToHead = true;
		*fHeightHack = 3.0f;
		*fWidthHack = 1.85f;
		*fUpHack = 0.15f;
		*fRightHack = -0.4f;
		*fScaleHack = 0.5;
		break;
	case METROID_THERMAL_EFFECT:
	case METROID_THERMAL_EFFECT_GUN:
		*bStuckToHead = true;
		*fHeightHack = 4.2f;
		*fWidthHack = 2.1f;
		*fUpHack = 0;
		*fRightHack = -0.4f;
		*fScaleHack = 0.01f;
		break;

	case METROID_XRAY_EFFECT:
		*bStuckToHead = true;
		*fHeightHack = 6.0f;
		*fWidthHack = 3.0f;
		*fUpHack = 0;
		*fRightHack = -0.5f;
		*fScaleHack = 0.01f;
		break;
	case METROID_DARK_EFFECT:
		*bStuckToHead = true;
		*fHeightHack = 3.0f;
		*fWidthHack = 1.85f;
		*fUpHack = 0.15f;
		*fRightHack = -0.4f;
		*fScaleHack = 0.5f;
		break;
	case METROID_DARK_CENTRAL:
		*bStuckToHead = true;
		break;
	case METROID_VISOR_DIRT:
		// todo: move to helmet depth (hard with a 2D layer)
		*bStuckToHead = true;
		*bFullscreenLayer = false;
		*fScaleHack = 2;
		break;
	case METROID_SCAN_DARKEN:
		*bStuckToHead = true;
		*bFullscreenLayer = true;
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
		*fUpHack = 0.16f;
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
	case METROID_UNKNOWN:
	case METROID_WORLD:
	case METROID_MORPHBALL_WORLD:
	case METROID_XRAY_WORLD:
	case METROID_THERMAL_GUN_AND_DOOR:
	case METROID_GUN:
	case METROID_RETICLE:
	case METROID_SCAN_ICONS:
	case METROID_SCAN_CROSS:
	case METROID_UNKNOWN_WORLD:
	case METROID_UNKNOWN_2D:
	case METROID_SHADOW_2D:
		*bStuckToHead = false;
		break;
	case METROID_MAP_0:
	case METROID_MAP_1:
	case METROID_MAP_2:
		*bStuckToHead = false;
		*fScaleHack = 0.02f;
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
		*fWidthHack = 0.78f;
		*fHeightHack = 0.78f;
		break;
	case METROID_UNKNOWN_HUD:
	case METROID_HUD:
		*bStuckToHead = true;
		*fScaleHack = 30;
		*fWidthHack = 0.78f;
		*fHeightHack = 0.78f;
		break;
	case METROID_VISOR_RADAR_HINT:
	case METROID_RADAR_DOT:
		*bStuckToHead = true;
		*fScaleHack = 32;
		*fWidthHack = 0.78f;
		*fHeightHack = 0.78f;
		break;
	case METROID_MAP_OR_HINT:
	case METROID_MAP:
		*bStuckToHead = true;
		*fScaleHack = 30;
		*fWidthHack = 0.78f;
		*fHeightHack = 0.78f;
		break;
	case METROID_HELMET:
		*bStuckToHead = true;
		*fScaleHack = 100; // 1/100
		*fWidthHack = 1.7f;
		*fHeightHack = 1.5f;
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
		// move the hologram inside the box when scanning ship
		// and make it bigger
		*bStuckToHead = false;
		break;

	case ZELDA_UNKNOWN:
	case ZELDA_WORLD:
	case ZELDA_DIALOG:
	case ZELDA_NEXT:
	case ZELDA_UNKNOWN_2D:
	case ZELDA_UNKNOWN_EFFECT:
		// render most things like normal
		break;

	case ZELDA_DARK_EFFECT:
		*bHide = true;
		break;
	case ZELDA_REFLECTION:
		*bHide = true;
		break;

	default:
		*bStuckToHead = true;
		*fScaleHack = 30; // 1/30
		break;
	}
}

//#pragma optimize("", on)
