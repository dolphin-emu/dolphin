// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/VertexShaderGen.h"

enum ViewportType {
	VIEW_FULLSCREEN = 0,
	VIEW_LETTERBOXED,
	VIEW_HUD_ELEMENT,
	VIEW_SKYBOX,
	VIEW_PLAYER_1,
	VIEW_PLAYER_2,
	VIEW_PLAYER_3,
	VIEW_PLAYER_4,
	VIEW_OFFSCREEN,
	VIEW_RENDER_TO_TEXTURE,
};
extern enum ViewportType g_viewport_type, g_old_viewport_type;

class PointerWrap;

void UpdateProjectionHack(int iParams[], std::string sParams[]);

// The non-API dependent parts.
class VertexShaderManager
{
public:
	static void Init();
	static void Dirty();
	static void Shutdown();
	static void DoState(PointerWrap &p);

	// constant management
	static void SetConstants();
	static void SetProjectionConstants();
	static void SetViewportConstants();
	static void CheckOrientationConstants();
	static void CheckSkybox();
	static void LockSkybox();

	static void InvalidateXFRange(int start, int end);
	static void SetTexMatrixChangedA(u32 value);
	static void SetTexMatrixChangedB(u32 value);
	static void SetViewportChanged();
	static void SetProjectionChanged();
	static void SetMaterialColorChanged(int index, u32 color);

	static void TranslateView(float left_metres, float forward_metres, float down_metres = 0.0f);
	static void RotateView(float x, float y);
	static void ScaleView(float scale);
	static void ResetView();

	// data: 3 floats representing the X, Y and Z vertex model coordinates and the posmatrix index.
	// out:  4 floats which will be initialized with the corresponding clip space coordinates
	// NOTE: g_fProjectionMatrix must be up to date when this is called
	//       (i.e. VertexShaderManager::SetConstants needs to be called before using this!)
	static void TransformToClipSpace(const float* data, float* out, u32 mtxIdx);

	static VertexShaderConstants constants;
	static std::vector<VertexShaderConstants> constants_replay;
	static float4 constants_eye_projection[2][4];
	static bool m_layer_on_top;
	static bool dirty;
};

void ScaleRequestedToRendered(EFBRectangle *src);
extern EFBRectangle g_final_screen_region, g_requested_viewport, g_rendered_viewport;
extern bool debug_newScene;
