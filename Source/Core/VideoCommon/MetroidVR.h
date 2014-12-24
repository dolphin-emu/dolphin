// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

typedef enum
{
	METROID_UNKNOWN = 0,
	METROID_BACKGROUND_3D,
	METROID_SHADOWS,
	METROID_SHADOW_2D,
	METROID_BODY_SHADOWS,
	METROID_SCREEN_OVERLAY,
	METROID_WORLD,
	METROID_XRAY_WORLD,
	METROID_XRAY_EFFECT,
	METROID_THERMAL_EFFECT,
	METROID_THERMAL_EFFECT_GUN,
	METROID_GUN,
	METROID_CINEMATIC_WORLD,
	METROID_BLACK_BARS,
	METROID_SCREEN_FADE,
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
	METROID_DARK_VISOR_HUD,
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

	ZELDA_UNKNOWN,
	ZELDA_WORLD,
	ZELDA_REFLECTION,
	ZELDA_DARK_EFFECT,
	ZELDA_DIALOG,
	ZELDA_NEXT,
	ZELDA_UNKNOWN_2D,
	ZELDA_UNKNOWN_EFFECT,

	METROID_LAYER_COUNT
} TMetroidLayer;

// helmet, combat visor, radar dot, map, scan visor, scan text, and scan hologram should be attached to face
// unknown, world, and reticle should NOT be attached to face

extern TMetroidLayer g_metroid_layer;
extern bool g_metroid_scan_visor, g_metroid_scan_visor_active, 
g_metroid_xray_visor, g_metroid_thermal_visor,
g_metroid_map_screen, g_metroid_inventory,
g_metroid_dark_visor, g_metroid_echo_visor, g_metroid_morphball_active;
extern int g_metroid_wide_count, g_metroid_normal_count;

void NewMetroidFrame();
int Round100(float x);
const char *MetroidLayerName(TMetroidLayer layer);
TMetroidLayer GetMetroidPrime1GCLayer2D(int layer, float left, float right, float top, float bottom, float znear, float zfar);
TMetroidLayer GetMetroidPrime1GCLayer(int layer, float hfov, float vfov, float znear, float zfar);
TMetroidLayer GetMetroidPrime2GCLayer2D(int layer, float left, float right, float top, float bottom, float znear, float zfar);
TMetroidLayer GetMetroidPrime2GCLayer(int layer, float hfov, float vfov, float znear, float zfar);
TMetroidLayer GetZeldaTPGCLayer2D(int layer, float left, float right, float top, float bottom, float znear, float zfar);
TMetroidLayer GetZeldaTPGCLayer(int layer, float hfov, float vfov, float znear, float zfar);
void GetMetroidPrimeValues(bool *bStuckToHead, bool *bFullscreenLayer, bool *bHide, bool *bFlashing,
	float* fScaleHack, float *fWidthHack, float *fHeightHack, float *fUpHack, float *fRightHack);

