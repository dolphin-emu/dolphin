// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>
#include <sstream>

#ifdef HAVE_OCULUSSDK
#include "Kernel/OVR_Types.h"
#include "OVR_CAPI.h"
#include "Kernel/OVR_Math.h"
#endif

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/MetroidVR.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/XFMemory.h"

#define HACK_LOG INFO_LOG

static float GC_ALIGNED16(g_fProjectionMatrix[16]);

// TODO: remove
//VR Global variable shared from core. True if the Wii is set to Widescreen (so the game thinks it is rendering to 16:9)
//   or false if it isn't (so the game thinks it is rendering to 4:3). Which is different from how Dolphin will actually render it.
extern bool g_aspect_wide;

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged, bFreeLookChanged;
static BitSet32 nMaterialsChanged;
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
bool VertexShaderManager::m_layer_on_top;

bool VertexShaderManager::dirty;

//VR Virtual Reality debugging variables
int vr_render_eye = -1;
int debug_viewportNum = 0;
Viewport debug_vpList[64] = { 0 };
int debug_projNum = 0;
float debug_projList[64][7] = { 0 };
bool debug_newScene = true, debug_nextScene = false;
int vr_widest_3d_projNum = -1;
float vr_widest_3d_HFOV = 0;
float vr_widest_3d_VFOV = 0;
float vr_widest_3d_zNear = 0;
float vr_widest_3d_zFar = 0;
EFBRectangle g_final_screen_region = EFBRectangle(0, 0, 640, 528);
EFBRectangle g_requested_viewport = EFBRectangle(0, 0, 640, 528), g_rendered_viewport = EFBRectangle(0, 0, 640, 528);
enum ViewportType g_viewport_type = VIEW_FULLSCREEN, g_old_viewport_type = VIEW_FULLSCREEN;
enum SplitScreenType {
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

void ScaleRequestedToRendered(EFBRectangle *src)
{
	float m = (float)g_rendered_viewport.GetWidth() / g_requested_viewport.GetWidth();
	src->left = (int)(0.5f+(src->left - g_requested_viewport.left)*m + g_rendered_viewport.left);
	src->right = (int)(0.5f + (src->right - g_requested_viewport.left)*m + g_rendered_viewport.left);
	m = (float)g_rendered_viewport.GetHeight() / g_requested_viewport.GetHeight();
	src->top = (int)(0.5f + (src->top - g_requested_viewport.top)*m + g_rendered_viewport.top);
	src->bottom = (int)(0.5f + (src->bottom - g_requested_viewport.top)*m + g_rendered_viewport.top);
}

const char *GetViewportTypeName(ViewportType v)
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

void ClearDebugProj() { //VR
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

void SetViewportType(Viewport &v)
{
	// VR
	g_old_viewport_type = g_viewport_type;
	float left, top, width, height;
	left = v.xOrig - v.wd - 342;
	top = v.yOrig + v.ht - 342;
	width  =  2 * v.wd;
	height = -2 * v.ht;
	float screen_width = (float)g_final_screen_region.GetWidth();
	float screen_height = (float)g_final_screen_region.GetHeight();
	float min_screen_width = 0.90f * screen_width;
	float min_screen_height = 0.90f * screen_height;
	float max_top = screen_height - min_screen_height;
	float max_left = screen_width - min_screen_width;

	// Power of two square viewport in the corner of the screen means we are rendering to a texture,
	// usually a shadow texture from the POV of the light, that will be drawn multitextured onto 3D objects.
	// Note this is a temporary rendering to the backbuffer that will be cleared or overwritten after reading.
	// Twilighlight Princess GC uses square but non-power-of-2 textures: 216x216 and 384x384
	// Metroid Prime 2 uses square textures in the bottom left corner but screen_height is wrong.
	// So the relaxed rule is: square texture on any screen edge with size a multiple of 8
	//if (width == height
	//	&& (width == 32 || width == 64 || width == 128 || width == 256)
	//	&& ((left == 0 && top == 0) || (left == 0 && top == screen_height - height)
	//	|| (left == screen_width - width && top == 0) || (left == screen_width - width && top == screen_height - height)))
	if (width == height
		&& (width == 1 || width == 2 || width == 4 || ((int)width % 8 == 0))
		&& (left == 0 || top == 0 || top == screen_height - height || left == screen_width - width))
	{
		g_viewport_type = VIEW_RENDER_TO_TEXTURE;
	}
	// Zelda Twilight Princess uses this strange viewport for rendering the Map Screen's coloured map highlights to a texture.
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
	else if (width >= (min_screen_width / 2) && height >= (min_screen_height / 2)
	          && width <= (screen_width / 2) && height <= (screen_height / 2))
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
	else if (left >= g_final_screen_region.right || top >= g_final_screen_region.bottom || left+width <= g_final_screen_region.left || top+height <= g_final_screen_region.top)
	{
		g_viewport_type = VIEW_OFFSCREEN;
	}
	else
	{
		g_viewport_type = VIEW_HUD_ELEMENT;
	}
	if (g_viewport_type == VIEW_FULLSCREEN || g_viewport_type == VIEW_LETTERBOXED || (g_viewport_type >= VIEW_PLAYER_1 && g_viewport_type <= VIEW_PLAYER_4))
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

void DoLogViewport(int j, Viewport &v)
{
	//VR
	HACK_LOG(VR, "  Viewport %d: %s (%g,%g) %gx%g; near=%g (%g), far=%g (%g)", j, GetViewportTypeName(g_viewport_type), v.xOrig - v.wd - 342, v.yOrig + v.ht - 342, 2 * v.wd, -2 * v.ht, (v.farZ - v.zRange) / 16777216.0f, v.farZ - v.zRange, v.farZ / 16777216.0f, v.farZ);
	HACK_LOG(VR, "      copyTexSrc (%d,%d) %dx%d", g_final_screen_region.left, g_final_screen_region.top, g_final_screen_region.GetWidth(), g_final_screen_region.GetHeight());
}

void DoLogProj(int j, float p[], const char *s) { //VR
	if (j == g_ActiveConfig.iSelectedLayer)
		HACK_LOG(VR, "** SELECTED LAYER:");
	if (p[6] != 0) { // orthographic projection
		//float right = p[0]-(p[0]*p[1]);
		//float left = right - 2/p[0];

		float left = -(p[1] + 1) / p[0];
		float right = left + 2 / p[0];
		float bottom = -(p[3] + 1) / p[2];
		float top = bottom + 2 / p[2];
		float zfar = p[5] / p[4];
		float znear = (1 + p[4] * zfar) / p[4];
		HACK_LOG(VR, "%d: 2D: %s (%g, %g) to (%g, %g); z: %g to %g  [%g, %g]", j, s, left, top, right, bottom, znear, zfar, p[4], p[5]);
	}
	else if (p[0] != 0 || p[2] != 0) { // perspective projection
		float f = p[5] / p[4];
		float n = f*p[4] / (p[4] - 1);
		if (p[1] != 0.0f || p[3] != 0.0f) {
			HACK_LOG(VR, "%d: %s OFF-AXIS Perspective: 2n/w=%.2f A=%.2f; 2n/h=%.2f B=%.2f; n=%.2f f=%.2f", j, s, p[0], p[1], p[2], p[3], p[4], p[5]);
			HACK_LOG(VR, "	HFOV: %.2f    VFOV: %.2f   Aspect Ratio: 16:%.1f", 2 * atan(1.0f / p[0])*180.0f / 3.1415926535f, 2 * atan(1.0f / p[2])*180.0f / 3.1415926535f, 16 / (2 / p[0])*(2 / p[2]));
		}
		else {
			HACK_LOG(VR, "%d: %s HFOV: %.2fdeg; VFOV: %.2fdeg; Aspect Ratio: 16:%.1f; near:%f, far:%f", j, s, 2 * atan(1.0f / p[0])*180.0f / 3.1415926535f, 2 * atan(1.0f / p[2])*180.0f / 3.1415926535f, 16 / (2 / p[0])*(2 / p[2]), n, f);
		}
	}
	else { // invalid
		HACK_LOG(VR, "%d: %s ZERO", j, s);
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
		case 31:
			g_metroid_layer = GetMetroidPrime1WiiLayer(debug_projNum, hfov, vfov, n, f);
			break;
		case 2:
		case 32:
			g_metroid_layer = GetMetroidPrime2GCLayer(debug_projNum, hfov, vfov, n, f);
			break;
		case 113:
			g_metroid_layer = GetZeldaTPGCLayer(debug_projNum, hfov, vfov, n, f);
			break;
		case 0:
		default:
			g_metroid_layer = METROID_UNKNOWN;
			break;
		}

		if (debug_newScene && fabs(hfov) > vr_widest_3d_HFOV && fabs(hfov) <= 125 && (fabs(p[2]) != fabs(p[0]))) {
			DEBUG_LOG(VR, "***** New Widest 3D *****");

			vr_widest_3d_projNum = debug_projNum;
			vr_widest_3d_HFOV = fabs(hfov);
			vr_widest_3d_VFOV = fabs(vfov);
			vr_widest_3d_zNear = fabs(n);
			vr_widest_3d_zFar = fabs(f);
			DEBUG_LOG(VR, "%d: %g x %g deg, n=%g f=%g, p4=%g p5=%g; xs=%g ys=%g", vr_widest_3d_projNum, vr_widest_3d_HFOV, vr_widest_3d_VFOV, vr_widest_3d_zNear, vr_widest_3d_zFar, p[4], p[5], p[0], p[2]);
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
		case 31:
			g_metroid_layer = GetMetroidPrime1WiiLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
			break;
		case 2:
		case 32:
			g_metroid_layer = GetMetroidPrime2GCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
			break;
		case 113:
			g_metroid_layer = GetZeldaTPGCLayer2D(debug_projNum, left, right, top, bottom, znear, zfar);
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
		INFO_LOG(VIDEO, "\t\t--- Orthographic Projection Hack ON ---");

		fhacksign1 *= (iPhackvalue[1] == 1) ? -1.0f : fhacksign1;
		sTemp[0] = (iPhackvalue[1] == 1) ? " * (-1)" : "";
		fhacksign2 *= (iPhackvalue[2] == 1) ? -1.0f : fhacksign2;
		sTemp[1] = (iPhackvalue[2] == 1) ? " * (-1)" : "";

		fhackvalue1 = PHackValue(sPhackvalue[0]);
		INFO_LOG(VIDEO, "- zNear Correction = (%f + zNear)%s", fhackvalue1, sTemp[0]);

		fhackvalue2 = PHackValue(sPhackvalue[1]);
		INFO_LOG(VIDEO, "- zFar Correction =  (%f + zFar)%s", fhackvalue2, sTemp[1]);

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

	m_layer_on_top = false;

	memset(&xfmem, 0, sizeof(xfmem));
	memset(&constants, 0 , sizeof(constants));
	ResetView();

	// TODO: should these go inside ResetView()?
	Matrix44::LoadIdentity(s_viewportCorrection);
	memset(g_fProjectionMatrix, 0, sizeof(g_fProjectionMatrix));
	for (int i = 0; i < 4; ++i)
		g_fProjectionMatrix[i*5] = 1.0f;
	g_viewport_type = VIEW_FULLSCREEN;
	g_old_viewport_type = VIEW_FULLSCREEN;
	g_splitscreen_type = SS_FULLSCREEN;
	g_old_splitscreen_type = SS_FULLSCREEN;
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

	nMaterialsChanged = BitSet32::AllTrue(4);

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
				constants.materials[i][2] = (data >>  8) & 0xFF;
				constants.materials[i][3] =  data        & 0xFF;
		dirty = true;
	}
	nMaterialsChanged = BitSet32(0);

	if (bPosNormalMatrixChanged)
	{
		bPosNormalMatrixChanged = false;

		const float *pos = (const float *)xfmem.posMatrices + g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4;
		const float *norm = (const float *)xfmem.normalMatrices + 3 * (g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31);

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
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4],
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4],
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4],
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4]
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
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4],
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4],
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4],
			(const float *)&xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4]
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
		// VR, Check whether it is a skybox, fullscreen, letterboxed, splitscreen multiplayer, hud element, or offscreen
		SetViewportType(xfmem.viewport);
		LogViewport(xfmem.viewport);

		// The console GPU places the pixel center at 7/12 unless antialiasing
		// is enabled, while D3D and OpenGL place it at 0.5. See the comment
		// in VertexShaderGen.cpp for details.
		// NOTE: If we ever emulate antialiasing, the sample locations set by
		// BP registers 0x01-0x04 need to be considered here.
		const float pixel_center_correction = 7.0f / 12.0f - 0.5f;
		const float pixel_size_x = 2.f / Renderer::EFBToScaledXf(2.f * xfmem.viewport.wd);
		const float pixel_size_y = 2.f / Renderer::EFBToScaledXf(2.f * xfmem.viewport.ht);
		constants.pixelcentercorrection[0] = pixel_center_correction * pixel_size_x;
		constants.pixelcentercorrection[1] = pixel_center_correction * pixel_size_y;
		dirty = true;
		// This is so implementation-dependent that we can't have it here.
		g_renderer->SetViewport();
		// VR adjust the projection matrix for the new kind of viewport
		if (g_viewport_type != g_old_viewport_type && !bProjectionChanged)
			SetProjectionConstants();

		// Update projection if the viewport isn't 1:1 useable
		if (!g_ActiveConfig.backend_info.bSupportsOversizedViewports)
		{
			ViewportCorrectionMatrix(s_viewportCorrection);
			if (!bProjectionChanged)
				SetProjectionConstants();
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

	m_layer_on_top = false;
	bool bFullscreenLayer = g_ActiveConfig.bHudFullscreen && xfmem.projection.type != GX_PERSPECTIVE;
	bool bFlashing = (debug_projNum - 1) == g_ActiveConfig.iSelectedLayer;
	bool bStuckToHead = false, bHide = false;
	int flipped_x = 1, flipped_y = 1, iTelescopeHack = -1;
	float fScaleHack = 1, fWidthHack = 1, fHeightHack = 1, fUpHack = 0, fRightHack = 0;

	if (g_ActiveConfig.iMetroidPrime)
	{
		GetMetroidPrimeValues(&bStuckToHead, &bFullscreenLayer, &bHide, &bFlashing, 
			&fScaleHack, &fWidthHack, &fHeightHack, &fUpHack, &fRightHack, &iTelescopeHack);
	}

	// VR: in split-screen, only draw VR player TODO: fix offscreen to render to a separate texture in VR 
	bHide = bHide || (g_has_hmd && (g_viewport_type == VIEW_OFFSCREEN || (g_viewport_type >= VIEW_PLAYER_1 && g_viewport_type <= VIEW_PLAYER_4 && g_ActiveConfig.iVRPlayer!=g_viewport_type-VIEW_PLAYER_1)));
	// flash selected layer for debugging
	bHide = bHide || (bFlashing && g_ActiveConfig.iFlashState > 5);

	// Split WidthHack and HeightHack into left and right versions for telescopes
	float fLeftWidthHack = fWidthHack, fRightWidthHack = fWidthHack;
	float fLeftHeightHack = fHeightHack, fRightHeightHack = fHeightHack;
	bool bHideLeft = bHide, bHideRight = bHide, bTelescopeHUD = false, bNoForward = false;
	if (iTelescopeHack < 0 && g_ActiveConfig.iTelescopeEye && vr_widest_3d_VFOV <= g_ActiveConfig.fTelescopeMaxFOV && vr_widest_3d_VFOV > 1)
		iTelescopeHack = g_ActiveConfig.iTelescopeEye;
	if (iTelescopeHack > 0)
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
		g_fProjectionMatrix[15] = 1.0f + FLT_EPSILON; // hack to fix depth clipping precision issues (such as Sonic Unleashed UI)

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

	float UnitsPerMetre = g_ActiveConfig.fUnitsPerMetre * fScaleHack / g_ActiveConfig.fScale;

	if (bHide)
	{
		// If we are supposed to hide the layer, zero out the projection matrix
		memset(constants.projection, 0, 4 * 16);
		memset(constants_eye_projection[0], 0, 2 * 4 * 16);
		memset(GeometryShaderManager::constants.stereoparams, 0, 4 * 4);
		GeometryShaderManager::dirty = true;
	}
	// don't do anything fancy for rendering to a texture
	// render exactly as we are told, and in mono
	else if (g_viewport_type == VIEW_RENDER_TO_TEXTURE)
	{
		Matrix44 correctedMtx;
		Matrix44::Set(correctedMtx, g_fProjectionMatrix);

		memcpy(constants.projection, correctedMtx.data, 4 * 16);
		memcpy(constants_eye_projection[0], correctedMtx.data, 4 * 16);
		memcpy(constants_eye_projection[1], correctedMtx.data, 4 * 16);
		GeometryShaderManager::constants.stereoparams[0] = GeometryShaderManager::constants.stereoparams[1] = 0;
		GeometryShaderManager::constants.stereoparams[2] = GeometryShaderManager::constants.stereoparams[3] = 0;
		GeometryShaderManager::dirty = true;
	}
	// This was already copied from the fullscreen EFB.
	// Which makes it already correct for the HMD's FOV.
	// But we still need to correct it for the difference between the requested and rendered viewport.
	// Don't add any stereoscopy because that was already done when copied.
	else if (g_has_hmd && g_ActiveConfig.bEnableVR && bFullscreenLayer)
	{
		Matrix44 projMtx, scale_matrix, correctedMtx;
		Matrix44::Set(projMtx, g_fProjectionMatrix);

		projMtx.data[0 * 4 + 0] = projMtx.data[0 * 4 + 0] * fWidthHack;
		projMtx.data[1 * 4 + 1] = projMtx.data[1 * 4 + 1] * fHeightHack;
		projMtx.data[0 * 4 + 3] = projMtx.data[0 * 4 + 3] + fRightHack;
		projMtx.data[1 * 4 + 3] = projMtx.data[1 * 4 + 3] + fUpHack;

		Matrix44::LoadIdentity(scale_matrix);

		Matrix44::Multiply(scale_matrix, projMtx, correctedMtx);

		memcpy(constants.projection, correctedMtx.data, 4 * 16);
		memcpy(constants_eye_projection[0], correctedMtx.data, 4 * 16);
		memcpy(constants_eye_projection[1], correctedMtx.data, 4 * 16);
		GeometryShaderManager::constants.stereoparams[0] = GeometryShaderManager::constants.stereoparams[1] = 0;
		GeometryShaderManager::constants.stereoparams[2] = GeometryShaderManager::constants.stereoparams[3] = 0;
		GeometryShaderManager::dirty = true;
	}
	// VR Oculus Rift 3D projection matrix, needs to include head-tracking
	else if (g_has_hmd && g_ActiveConfig.bEnableVR)
	{
		float *p = rawProjection;
		// near clipping plane in game units
		float zfar, znear, zNear3D, hfov, vfov;

		// Real 3D scene
		if (xfmem.projection.type == GX_PERSPECTIVE && g_viewport_type != VIEW_HUD_ELEMENT && g_viewport_type != VIEW_OFFSCREEN)
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
		// or 3D HUD element that we will treat like a part of the 2D HUD 
		else
		{
			if (vr_widest_3d_HFOV > 0) {
				m_layer_on_top = g_ActiveConfig.bHudOnTop;
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
				// TODO: fix aspect ratio in virtual console games
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
			g_fProjectionMatrix[11] = -zfar*znear / (zfar - znear);
			if (debug_newScene)
			{
				if (xfmem.projection.type == GX_PERSPECTIVE)
					DEBUG_LOG(VR, "3D HUD: m[2][2]=%f m[2][3]=%f ", g_fProjectionMatrix[10], g_fProjectionMatrix[11]);
				else
					DEBUG_LOG(VR, "2D: m[2][2]=%f m[2][3]=%f ", g_fProjectionMatrix[10], g_fProjectionMatrix[11]);
			}

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
				INFO_LOG(VR, "Using VR920 FOV");
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
				DEBUG_LOG(VR, "O hfov=%8.4f    vfov=%8.4f      znear=%8.4f   zfar=%8.4f", hfov2, vfov2, znear2, zfar2);
				DEBUG_LOG(VR, "O [%8.4f %8.4f %8.4f   %8.4f]", rift_left.M[0][0], rift_left.M[0][1], rift_left.M[0][2], rift_left.M[0][3]);
				DEBUG_LOG(VR, "O [%8.4f %8.4f %8.4f   %8.4f]", rift_left.M[1][0], rift_left.M[1][1], rift_left.M[1][2], rift_left.M[1][3]);
				DEBUG_LOG(VR, "O [%8.4f %8.4f %8.4f   %8.4f]", rift_left.M[2][0], rift_left.M[2][1], rift_left.M[2][2], rift_left.M[2][3]);
				DEBUG_LOG(VR, "O {%8.4f %8.4f %8.4f   %8.4f}", rift_left.M[3][0], rift_left.M[3][1], rift_left.M[3][2], rift_left.M[3][3]);
				// green = Game's suggestion
				INFO_LOG(VR, "G [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[0 * 4 + 0], proj_left.data[0 * 4 + 1], proj_left.data[0 * 4 + 2], proj_left.data[0 * 4 + 3]);
				INFO_LOG(VR, "G [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[1 * 4 + 0], proj_left.data[1 * 4 + 1], proj_left.data[1 * 4 + 2], proj_left.data[1 * 4 + 3]);
				INFO_LOG(VR, "G [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[2 * 4 + 0], proj_left.data[2 * 4 + 1], proj_left.data[2 * 4 + 2], proj_left.data[2 * 4 + 3]);
				INFO_LOG(VR, "G {%8.4f %8.4f %8.4f   %8.4f}", proj_left.data[3 * 4 + 0], proj_left.data[3 * 4 + 1], proj_left.data[3 * 4 + 2], proj_left.data[3 * 4 + 3]);
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
				DEBUG_LOG(VR, "VR [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[0 * 4 + 0], proj_left.data[0 * 4 + 1], proj_left.data[0 * 4 + 2], proj_left.data[0 * 4 + 3]);
				DEBUG_LOG(VR, "VR [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[1 * 4 + 0], proj_left.data[1 * 4 + 1], proj_left.data[1 * 4 + 2], proj_left.data[1 * 4 + 3]);
				DEBUG_LOG(VR, "VR [%8.4f %8.4f %8.4f   %8.4f]", proj_left.data[2 * 4 + 0], proj_left.data[2 * 4 + 1], proj_left.data[2 * 4 + 2], proj_left.data[2 * 4 + 3]);
				DEBUG_LOG(VR, "VR {%8.4f %8.4f %8.4f   %8.4f}", proj_left.data[3 * 4 + 0], proj_left.data[3 * 4 + 1], proj_left.data[3 * 4 + 2], proj_left.data[3 * 4 + 3]);
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
			extra_pitch -= g_ActiveConfig.fLeanBackAngle;
			Matrix33 pitch_matrix33;
			Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
			Matrix44 pitch_matrix;
			Matrix44::LoadMatrix33(pitch_matrix, pitch_matrix33);
			if (g_ActiveConfig.bOrientationTracking)
				Matrix44::Multiply(g_head_tracking_matrix, pitch_matrix, rotation_matrix);
			else
				Matrix44::Set(rotation_matrix, pitch_matrix.data);
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
				//rotation_matrix.data[1] *= -1;
				//rotation_matrix.data[2] *= -1;
				//rotation_matrix.data[3] *= -1;
				//rotation_matrix.data[4] *= -1;
				//rotation_matrix.data[8] *= -1;
				//rotation_matrix.data[12] *= -1;
				flipped_x = -1;
			}
			if (rawProjection[2]<0)
			{
				if (debug_newScene)
					INFO_LOG(VR, "flipped Y");
				// flip all the y axis values, except y squared (data[5])
				// Needed for ABBA
				//rotation_matrix.data[1] *= -1;
				//rotation_matrix.data[4] *= -1;
				//rotation_matrix.data[6] *= -1;
				//rotation_matrix.data[7] *= -1;
				//rotation_matrix.data[9] *= -1;
				//rotation_matrix.data[13] *= -1;
				flipped_y = -1;
			}
		}

		Matrix44 walk_matrix, look_matrix;
		if (bStuckToHead || g_is_skybox)
		{
			Matrix44::LoadIdentity(walk_matrix);
		}
		else
		{
			float pos[3];
			if (g_ActiveConfig.bPositionTracking)
			{
				float head[3];
				for (int i = 0; i < 3; ++i)
					head[i] = g_head_tracking_position[i] * UnitsPerMetre;
				pos[0] = head[0];
				pos[1] = head[1] * cos(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle)) + head[2] * sin(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle));
				pos[2] = head[2] * cos(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle)) - head[1] * sin(DEGREES_TO_RADIANS(g_ActiveConfig.fLeanBackAngle));
			}
			else
			{
				pos[0] = 0;
				pos[1] = 0;
				pos[2] = 0;
			}
			for (int i = 0; i < 3; ++i)
				pos[i] += s_fViewTranslationVector[i] * UnitsPerMetre;
			if (!bNoForward)
				pos[2] += g_ActiveConfig.fCameraForward * UnitsPerMetre;
			//static int x = 0;
			//x++;
			Matrix44::Translate(walk_matrix, pos);
			//if (x>100)
			//{
			//	x = 0;
			//	//			NOTICE_LOG(VR, "walk pos = %f, %f, %f", s_fViewTranslationVector[0], s_fViewTranslationVector[1], s_fViewTranslationVector[2]);
			//	INFO_LOG(VR, "head pos = %5.0fcm, %5.0fcm, %5.0fcm, walk %5.1f, %5.1f, %5.1f", 100 * g_head_tracking_position[0], 100 * g_head_tracking_position[1], 100 * g_head_tracking_position[2], s_fViewTranslationVector[0], s_fViewTranslationVector[1], s_fViewTranslationVector[2]);
			//}
		}

		if (xfmem.projection.type == GX_PERSPECTIVE && g_viewport_type != VIEW_HUD_ELEMENT && g_viewport_type != VIEW_OFFSCREEN)
		{
			if (debug_newScene)
				INFO_LOG(VR, "3D: do normally");
			Matrix44::Multiply(rotation_matrix, walk_matrix, look_matrix);
		}
		else
		if (xfmem.projection.type != GX_PERSPECTIVE || g_viewport_type == VIEW_HUD_ELEMENT || g_viewport_type == VIEW_OFFSCREEN)
		{
			if (debug_newScene)
				INFO_LOG(VR, "2D: hacky test");

			float HudWidth, HudHeight, HudThickness, HudDistance, HudUp, CameraForward, AimDistance;

			g_fProjectionMatrix[14] = 0.0f;
			g_fProjectionMatrix[15] = 1.0f + FLT_EPSILON; // hack to fix depth clipping precision issues (such as Sonic Unleashed UI)
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
				// Now that we know how far away the box is, and what FOV it should fill, we can work out the width and height in game units
				// Note: the HUD won't line up exactly (except at AimDistance) if CameraForward is non-zero 
				//float HudWidth = 2.0f * tanf(hfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
				//float HudHeight = 2.0f * tanf(vfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
				HudWidth = 2.0f * tanf(DEGREES_TO_RADIANS(hfov / 2.0f)) * HudDistance * (AimDistance + CameraForward) / AimDistance;
				HudHeight = 2.0f * tanf(DEGREES_TO_RADIANS(vfov / 2.0f)) * HudDistance * (AimDistance + CameraForward) / AimDistance;
			}

			float scale[3]; // width, height, and depth of box in game units divided by 2D width, height, and depth 
			float position[3]; // position of front of box relative to the camera, in game units 

			float viewport_scale[2];
			float viewport_offset[2]; // offset as a fraction of the viewport's width
			if (g_viewport_type != VIEW_HUD_ELEMENT && g_viewport_type != VIEW_OFFSCREEN)
			{
				viewport_scale[0] = 1.0f;
				viewport_scale[1] = 1.0f;
				viewport_offset[0] = 0.0f;
				viewport_offset[1] = 0.0f;
			}
			else
			{
				Viewport &v = xfmem.viewport;
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
				float zNear2D = zFar2D*rawProjection[4] / (rawProjection[4] - 1);
				float zObj = zNear2D + (zFar2D-zNear2D) * g_ActiveConfig.fHud3DCloser;

				left2D *= zObj;
				right2D *= zObj;
				bottom2D *= zObj;
				top2D *= zObj;

				// Scale the width and height to fit the HUD in metres
				if (rawProjection[0] == 0 || right2D == left2D) {
					scale[0] = 0;
				}
				else {
					scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
				}
				if (rawProjection[2] == 0 || top2D == bottom2D) {
					scale[1] = 0;
				}
				else {
					scale[1] = viewport_scale[1] * HudHeight / (top2D - bottom2D); // note that positive means up in 3D
				}
				// Keep depth the same scale as width, so it looks like a real object
				if (rawProjection[4] == 0 || zFar2D == zNear2D) {
					scale[2] = scale[0];
				}
				else {
					scale[2] = scale[0];
				}
				// Adjust the position for off-axis projection matrices, and shifting the 2D screen
				position[0] = scale[0] * (-(right2D + left2D) / 2.0f) + viewport_offset[0] * HudWidth; // shift it right into the centre of the view
				position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight + HudUp; // shift it up into the centre of the view;
				// Shift it from the old near clipping plane to the HUD distance, and shift the camera forward
				if (vr_widest_3d_HFOV <= 0)
					position[2] = scale[2] * zObj - HudDistance;
				else
					position[2] = scale[2] * zObj - HudDistance - CameraForward;



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
				// also work out what fraction of the height we should shift it up, and what fraction of the width we should shift it left
				// only multiply by the extra scale after adjusting the position?

				if (rawProjection[0] == 0 || right2D == left2D) {
					scale[0] = 0;
				}
				else {
					scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
				}
				if (rawProjection[2] == 0 || top2D == bottom2D) {
					scale[1] = 0;
				}
				else {
					scale[1] = viewport_scale[1] * HudHeight / (top2D - bottom2D); // note that positive means up in 3D
				}
				if (rawProjection[4] == 0 || zFar2D == zNear2D) {
					scale[2] = 0; // The 2D layer was flat, so we make it flat instead of a box to avoid dividing by zero
				}
				else {
					scale[2] = HudThickness / (zFar2D - zNear2D); // Scale 2D z values into 3D game units so it is the right thickness
				}
				position[0] = scale[0] * (-(right2D + left2D) / 2.0f) + viewport_offset[0] * HudWidth; // shift it right into the centre of the view
				position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight + HudUp; // shift it up into the centre of the view;
				// Shift it from the zero plane to the HUD distance, and shift the camera forward
				if (vr_widest_3d_HFOV <= 0)
					position[2] = -HudDistance;
				else
					position[2] = -HudDistance - CameraForward;
			}

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
		if (g_has_rift && !bTelescopeHUD && !g_is_skybox)
		{
#ifdef OCULUSSDK042
			posLeft[0] = g_eye_render_desc[0].ViewAdjust.x * UnitsPerMetre;
			posLeft[1] = g_eye_render_desc[0].ViewAdjust.y * UnitsPerMetre;
			posLeft[2] = g_eye_render_desc[0].ViewAdjust.z * UnitsPerMetre;
			posRight[0] = g_eye_render_desc[1].ViewAdjust.x * UnitsPerMetre;
			posRight[1] = g_eye_render_desc[1].ViewAdjust.y * UnitsPerMetre;
			posRight[2] = g_eye_render_desc[1].ViewAdjust.z * UnitsPerMetre;
#else
			posLeft[0] = g_eye_render_desc[0].HmdToEyeViewOffset.x * UnitsPerMetre;
			posLeft[1] = g_eye_render_desc[0].HmdToEyeViewOffset.y * UnitsPerMetre;
			posLeft[2] = g_eye_render_desc[0].HmdToEyeViewOffset.z * UnitsPerMetre;
			posRight[0] = g_eye_render_desc[1].HmdToEyeViewOffset.x * UnitsPerMetre;
			posRight[1] = g_eye_render_desc[1].HmdToEyeViewOffset.y * UnitsPerMetre;
			posRight[2] = g_eye_render_desc[1].HmdToEyeViewOffset.z * UnitsPerMetre;
#endif
		}
#endif
		Matrix44 view_matrix_left, view_matrix_right;
		if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
		{
			Matrix44::Set(view_matrix_left, look_matrix.data);

			Matrix44::Translate(eye_pos_matrix_right, posRight);
			Matrix44::Multiply(eye_pos_matrix_right, look_matrix, view_matrix_right);
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
			//Needed for Action Girlz Racing, Backyard Baseball
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
		if (bHideLeft && (bHideRight || !(g_ActiveConfig.iStereoMode>0))) {
			memset(final_matrix_left.data, 0, 16 * sizeof(final_matrix_left.data[0]));
		}
		if (bHideRight)
		{
			memset(final_matrix_right.data, 0, 16 * sizeof(final_matrix_right.data[0]));
		}

		memcpy(constants.projection, final_matrix_left.data, 4 * 16);
		memcpy(constants_eye_projection[0], final_matrix_left.data, 4 * 16);
		memcpy(constants_eye_projection[1], final_matrix_right.data, 4 * 16);
		if (g_ActiveConfig.iStereoMode == STEREO_OCULUS)
		{
			GeometryShaderManager::constants.stereoparams[0] *= posLeft[0];
			GeometryShaderManager::constants.stereoparams[1] *= posRight[0];
			if (debug_newScene)
			{
				DEBUG_LOG(VR, "F=[%8.4f %8.4f %8.4f   %8.4f]", final_matrix_left.data[0 * 4 + 0], final_matrix_left.data[0 * 4 + 1], final_matrix_left.data[0 * 4 + 2], final_matrix_left.data[0 * 4 + 3]);
				DEBUG_LOG(VR, "F=[%8.4f %8.4f %8.4f   %8.4f]", final_matrix_left.data[1 * 4 + 0], final_matrix_left.data[1 * 4 + 1], final_matrix_left.data[1 * 4 + 2], final_matrix_left.data[1 * 4 + 3]);
				DEBUG_LOG(VR, "F=[%8.4f %8.4f %8.4f   %8.4f]", final_matrix_left.data[2 * 4 + 0], final_matrix_left.data[2 * 4 + 1], final_matrix_left.data[2 * 4 + 2], final_matrix_left.data[2 * 4 + 3]);
				DEBUG_LOG(VR, "F={%8.4f %8.4f %8.4f   %8.4f}", final_matrix_left.data[3 * 4 + 0], final_matrix_left.data[3 * 4 + 1], final_matrix_left.data[3 * 4 + 2], final_matrix_left.data[3 * 4 + 3]);
				DEBUG_LOG(VR, "StereoParams: %8.4f, %8.4f", GeometryShaderManager::constants.stereoparams[0], GeometryShaderManager::constants.stereoparams[2]);
				DEBUG_LOG(VR, "eye_x = %8.4f", g_eye_render_desc[0].HmdToEyeViewOffset.x);
			}
		}
		else
		{
			GeometryShaderManager::constants.stereoparams[0] = GeometryShaderManager::constants.stereoparams[1] = 0;
		}
		GeometryShaderManager::dirty = true;
		dirty = true;
	}
	else if (bFreeLookChanged && xfmem.projection.type == GX_PERSPECTIVE && g_viewport_type)
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

		if (g_ActiveConfig.iStereoMode > 0)
		{
			if (xfmem.projection.type == GX_PERSPECTIVE)
			{
				float offset = (g_ActiveConfig.iStereoDepth / 1000.0f) * (g_ActiveConfig.iStereoDepthPercentage / 100.0f);
				GeometryShaderManager::constants.stereoparams[0] = (g_ActiveConfig.bStereoSwapEyes) ? offset : -offset;
				GeometryShaderManager::constants.stereoparams[1] = (g_ActiveConfig.bStereoSwapEyes) ? -offset : offset;
				GeometryShaderManager::constants.stereoparams[2] = (g_ActiveConfig.iStereoConvergence / 10.0f) * (g_ActiveConfig.iStereoConvergencePercent / 100.0f);
			}
			else
			{
				GeometryShaderManager::constants.stereoparams[0] = GeometryShaderManager::constants.stereoparams[1] = 0;
			}
		}
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
		if (g_ActiveConfig.iStereoMode > 0)
		{
			if (xfmem.projection.type == GX_PERSPECTIVE)
			{
				float offset = (g_ActiveConfig.iStereoDepth / 1000.0f) * (g_ActiveConfig.iStereoDepthPercentage / 100.0f);
				GeometryShaderManager::constants.stereoparams[0] = (g_ActiveConfig.bStereoSwapEyes) ? offset : -offset;
				GeometryShaderManager::constants.stereoparams[1] = (g_ActiveConfig.bStereoSwapEyes) ? -offset : offset;
				GeometryShaderManager::constants.stereoparams[2] = (g_ActiveConfig.iStereoConvergence / 10.0f) * (g_ActiveConfig.iStereoConvergencePercent / 100.0f);
			}
			else
			{
				GeometryShaderManager::constants.stereoparams[0] = GeometryShaderManager::constants.stereoparams[1] = 0;
			}
		}
	}
	dirty = true;
}
//#pragma optimize("", on)


void VertexShaderManager::InvalidateXFRange(int start, int end)
{
	if (((u32)start >= (u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4 &&
		 (u32)start <  (u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4 + 12) ||
		((u32)start >= XFMEM_NORMALMATRICES + ((u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31) * 3 &&
		 (u32)start <  XFMEM_NORMALMATRICES + ((u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31) * 3 + 9))
	{
		bPosNormalMatrixChanged = true;
	}

	if (((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex0MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex0MtxIdx*4+12) ||
		((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex1MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex1MtxIdx*4+12) ||
		((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex2MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex2MtxIdx*4+12) ||
		((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex3MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex3MtxIdx*4+12))
	{
		bTexMatricesChanged[0] = true;
	}

	if (((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex4MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex4MtxIdx*4+12) ||
		((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex5MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex5MtxIdx*4+12) ||
		((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex6MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex6MtxIdx*4+12) ||
		((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex7MtxIdx*4 && (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex7MtxIdx*4+12))
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
	if (g_main_cp_state.matrix_index_a.Hex != Value)
	{
		VertexManager::Flush();
		if (g_main_cp_state.matrix_index_a.PosNormalMtxIdx != (Value&0x3f))
			bPosNormalMatrixChanged = true;
		bTexMatricesChanged[0] = true;
		g_main_cp_state.matrix_index_a.Hex = Value;
	}
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
	if (g_main_cp_state.matrix_index_b.Hex != Value)
	{
		VertexManager::Flush();
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

void VertexShaderManager::SetMaterialColorChanged(int index, u32 color)
{
	nMaterialsChanged[index] = true;
}

void VertexShaderManager::ScaleView(float scale)
{
	for (int i = 0; i < 3; i++)
		s_fViewTranslationVector[i] *= scale;

	if (s_fViewTranslationVector[0] || s_fViewTranslationVector[1] || s_fViewTranslationVector[2])
		bFreeLookChanged = true;
	else
		bFreeLookChanged = false;

	bProjectionChanged = true;
}

void VertexShaderManager::TranslateView(float x, float y, float z)
{
	float result[3];
	float vector[3] = { x,z,y };

	Matrix33::Multiply(s_viewInvRotationMatrix, vector, result);

	for (int i = 0; i < 3; i++)
		s_fViewTranslationVector[i] += result[i];

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
	memset(s_fViewTranslationVector, 0, sizeof(s_fViewTranslationVector));
	Matrix33::LoadIdentity(s_viewRotationMatrix);
	Matrix33::LoadIdentity(s_viewInvRotationMatrix);
	s_fViewRotation[0] = s_fViewRotation[1] = 0.0f;

	bFreeLookChanged = false;
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
