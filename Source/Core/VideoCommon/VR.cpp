// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>
#include "VideoCommon/VR920.h"
#endif

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HW/WiimoteEmu/HydraTLayer.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

void ClearDebugProj();

#ifdef OVR_MAJOR_VERSION
ovrHmd hmd = nullptr;
ovrHmdDesc hmdDesc;
ovrFovPort g_best_eye_fov[2], g_eye_fov[2], g_last_eye_fov[2];
ovrEyeRenderDesc g_eye_render_desc[2];
ovrFrameTiming g_rift_frame_timing;
ovrPosef g_eye_poses[2], g_front_eye_poses[2];
int g_ovr_frameindex;
#endif

std::mutex g_vr_lock;

bool g_force_vr = false;
bool g_has_hmd = false, g_has_rift = false, g_has_vr920 = false;
bool g_is_direct_mode = false, g_is_nes = false;
bool g_new_tracking_frame = true;
bool g_new_frame_tracker_for_efb_skip = true;
u32 skip_objects_count = 0;
Matrix44 g_head_tracking_matrix;
float g_head_tracking_position[3];
float g_left_hand_tracking_position[3], g_right_hand_tracking_position[3];
int g_hmd_window_width = 0, g_hmd_window_height = 0, g_hmd_window_x = 0, g_hmd_window_y = 0;
const char *g_hmd_device_name = nullptr;
float g_vr_speed = 0;
float vr_freelook_speed = 0;
bool g_fov_changed = false, g_vr_black_screen = false;
bool g_vr_had_3D_already = false;
float vr_widest_3d_HFOV = 0;
float vr_widest_3d_VFOV = 0;
float vr_widest_3d_zNear = 0;
float vr_widest_3d_zFar = 0;
float g_game_camera_pos[3];
Matrix44 g_game_camera_rotmat;

ControllerStyle vr_left_controller = CS_HYDRA_LEFT, vr_right_controller = CS_HYDRA_RIGHT;

std::vector<TimewarpLogEntry> timewarp_logentries;

bool g_opcode_replay_enabled = false;
bool g_new_frame_just_rendered = false;
bool g_opcodereplay_frame = false;
int skipped_opcode_replay_count = 0;

//std::vector<u8*> s_pCurBufferPointer_log;
//std::vector<u8*> s_pBaseBufferPointer_log;
//std::vector<u8*> s_pEndBufferPointer_log;

//std::vector<u32> CPBase_log;
//std::vector<u32> CPEnd_log;
//std::vector<u32> CPHiWatermark_log;
//std::vector<u32> CPLoWatermark_log;
//std::vector<u32> CPReadWriteDistance_log;
//std::vector<u32> CPWritePointer_log;
//std::vector<u32> CPReadPointer_log;
//std::vector<u32> CPBreakpoint_log;

#ifdef _WIN32
static char hmd_device_name[MAX_PATH] = "";
#endif

void NewVRFrame()
{
	g_new_tracking_frame = true;
	g_new_frame_tracker_for_efb_skip = true;
	if (!g_vr_had_3D_already)
	{
		Matrix44::LoadIdentity(g_game_camera_rotmat);
	}
	g_vr_had_3D_already = false;
	skip_objects_count = 0;
	ClearDebugProj();

	// Prevent motion sickness: estimate how fast we are moving, and reduce the FOV if we are moving fast
	g_vr_speed = 0;
	if (g_ActiveConfig.bMotionSicknessAlways)
	{
		g_vr_speed = 1.0f;
	}
	else
	{
		if (g_ActiveConfig.bMotionSicknessDPad)
			g_vr_speed += HydraTLayer::vr_gc_dpad_speed + HydraTLayer::vr_cc_dpad_speed + HydraTLayer::vr_wm_dpad_speed;
		if (g_ActiveConfig.bMotionSicknessLeftStick)
			g_vr_speed += HydraTLayer::vr_gc_leftstick_speed + HydraTLayer::vr_wm_leftstick_speed;
		if (g_ActiveConfig.bMotionSicknessRightStick)
			g_vr_speed += HydraTLayer::vr_gc_rightstick_speed + HydraTLayer::vr_wm_rightstick_speed;
		if (g_ActiveConfig.bMotionSickness2D && vr_widest_3d_HFOV <= 0)
			g_vr_speed += 1.0f;
		if (g_ActiveConfig.bMotionSicknessIR)
			g_vr_speed += HydraTLayer::vr_ir_speed;
		if (g_ActiveConfig.bMotionSicknessFreelook)
			g_vr_speed += vr_freelook_speed;
		vr_freelook_speed = 0;
	}
	if (g_has_hmd && g_ActiveConfig.iMotionSicknessMethod == 2)
	{
		// black the screen if we are moving fast
		g_vr_black_screen = (g_vr_speed > 0.15f);
	}
#ifdef OVR_MAJOR_VERSION
	else if (g_has_rift && g_ActiveConfig.iMotionSicknessMethod == 1)
	{
		g_vr_black_screen = false;
		// reduce the FOV if we are moving fast
		if (g_vr_speed > 0.15f)
		{
			float t = tan(DEGREES_TO_RADIANS(g_ActiveConfig.fMotionSicknessFOV / 2));
			g_eye_fov[0].LeftTan = std::min(g_best_eye_fov[0].LeftTan, t);
			g_eye_fov[0].RightTan = std::min(g_best_eye_fov[0].RightTan, t);
			g_eye_fov[0].UpTan = std::min(g_best_eye_fov[0].UpTan, t);
			g_eye_fov[0].DownTan = std::min(g_best_eye_fov[0].DownTan, t);
			g_eye_fov[1].LeftTan = std::min(g_best_eye_fov[1].LeftTan, t);
			g_eye_fov[1].RightTan = std::min(g_best_eye_fov[1].RightTan, t);
			g_eye_fov[1].UpTan = std::min(g_best_eye_fov[1].UpTan, t);
			g_eye_fov[1].DownTan = std::min(g_best_eye_fov[1].DownTan, t);
		}
		else
		{
			memcpy(g_eye_fov, g_best_eye_fov, 2 * sizeof(g_eye_fov[0]));
		}
		g_fov_changed = memcmp(g_eye_fov, g_last_eye_fov, 2 * sizeof(g_eye_fov[0])) != 0;
		memcpy(g_last_eye_fov, g_eye_fov, 2 * sizeof(g_eye_fov[0]));
	}
#endif
	else
	{
		g_vr_black_screen = false;
	}
}

void InitVR()
{
	g_has_hmd = false;
	g_is_direct_mode = false;
	g_hmd_device_name = nullptr;
#ifdef OVR_MAJOR_VERSION
	memset(&g_rift_frame_timing, 0, sizeof(g_rift_frame_timing));
	ovr_Initialize();
	hmd = ovrHmd_Create(0);
	if (!hmd)
	{
		WARN_LOG(VR, "Oculus Rift not detected. Oculus Rift support will not be available.");
#endif
#ifdef _WIN32
		LoadVR920();
		if (g_has_vr920)
		{
			g_has_hmd = true;
			g_hmd_window_width = 800;
			g_hmd_window_height = 600;
			// Todo: find vr920
			g_hmd_window_x = 0;
			g_hmd_window_y = 0;
		}
		else
#endif
		{
			g_has_hmd = g_force_vr;
#ifdef OVR_MAJOR_VERSION
			if (g_force_vr)
			{
				WARN_LOG(VR, "Forcing VR mode, simulating Oculus Rift DK2.");
				hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
			}
#endif
		}
#ifdef OVR_MAJOR_VERSION
	}
	if (hmd)
	{
		// Get more details about the HMD
		//ovrHmd_GetDesc(hmd, &hmdDesc);
		hmdDesc = *hmd;
		ovrHmd_SetEnabledCaps(hmd, ovrHmd_GetEnabledCaps(hmd) | ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence);

		if (ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation | ovrTrackingCap_Position | ovrTrackingCap_MagYawCorrection,
			0))
		{
			g_has_rift = true;
			g_has_hmd = true;
			g_hmd_window_width = hmdDesc.Resolution.w;
			g_hmd_window_height = hmdDesc.Resolution.h;
			g_best_eye_fov[0] = hmdDesc.DefaultEyeFov[0];
			g_best_eye_fov[1] = hmdDesc.DefaultEyeFov[1];
			g_eye_fov[0] = g_best_eye_fov[0];
			g_eye_fov[1] = g_best_eye_fov[1];
			g_last_eye_fov[0] = g_eye_fov[0];
			g_last_eye_fov[1] = g_eye_fov[1];
			g_hmd_window_x = hmdDesc.WindowsPos.x;
			g_hmd_window_y = hmdDesc.WindowsPos.y;
			g_is_direct_mode = !(hmdDesc.HmdCaps & ovrHmdCap_ExtendDesktop);
#ifdef _WIN32
			g_hmd_device_name = hmdDesc.DisplayDeviceName;
			const char *p;
			if (g_hmd_device_name && (p = strstr(g_hmd_device_name, "\\Monitor")))
			{
				size_t n = p - g_hmd_device_name;
				if (n >= MAX_PATH)
					n = MAX_PATH - 1;
				g_hmd_device_name = strncpy(hmd_device_name, g_hmd_device_name, n);
				hmd_device_name[n] = '\0';
			}
#endif
			NOTICE_LOG(VR, "Oculus Rift head tracker started.");
		}
	}
#endif
	if (g_has_hmd)
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution =
			StringFromFormat("%dx%d", g_hmd_window_width, g_hmd_window_height);
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos = g_hmd_window_x;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos = g_hmd_window_y;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth = g_hmd_window_width;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight = g_hmd_window_height;
		SConfig::GetInstance().m_special_case = true;
	}
	else
	{
		SConfig::GetInstance().m_special_case = false;
	}
}

void VR_StopRendering()
{
#ifdef _WIN32
	if (g_has_vr920)
	{
		VR920_StopStereo3D();
	}
#endif
#ifdef OVR_MAJOR_VERSION
	// Shut down rendering and release resources (by passing NULL)
	if (g_has_rift)
	{
		ovrHmd_ConfigureRendering(hmd, nullptr, 0, g_eye_fov, g_eye_render_desc);
	}
#endif
}

void ShutdownVR()
{
#ifdef OVR_MAJOR_VERSION
	if (hmd)
	{
		// on my computer, on runtime 0.4.2, the Rift won't switch itself off without this:
		if (g_is_direct_mode)
			ovrHmd_SetEnabledCaps(hmd, ovrHmdCap_DisplayOff);
		ovrHmd_Destroy(hmd);
		g_has_rift = false;
		g_has_hmd = false;
		g_is_direct_mode = false;
		NOTICE_LOG(VR, "Oculus Rift shut down.");
	}
	ovr_Shutdown();
#endif
}

void VR_RecenterHMD()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		ovrHmd_RecenterPose(hmd);
	}
#endif
}

void VR_ConfigureHMDTracking()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		int cap = 0;
		if (g_ActiveConfig.bOrientationTracking)
			cap |= ovrTrackingCap_Orientation;
		if (g_ActiveConfig.bMagYawCorrection)
			cap |= ovrTrackingCap_MagYawCorrection;
		if (g_ActiveConfig.bPositionTracking)
			cap |= ovrTrackingCap_Position;
		ovrHmd_ConfigureTracking(hmd, cap, 0);
	}
#endif
}

void VR_ConfigureHMDPrediction()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		int caps = ovrHmd_GetEnabledCaps(hmd) & ~(ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence | ovrHmdCap_NoMirrorToWindow);
		if (g_Config.bLowPersistence)
			caps |= ovrHmdCap_LowPersistence;
		if (g_Config.bDynamicPrediction)
			caps |= ovrHmdCap_DynamicPrediction;
		if (g_Config.bNoMirrorToWindow)
			caps |= ovrHmdCap_NoMirrorToWindow;

		ovrHmd_SetEnabledCaps(hmd, caps);
	}
#endif
}

void VR_BeginFrame()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		g_rift_frame_timing = ovrHmd_BeginFrame(hmd, ++g_ovr_frameindex);
	}
#endif
}

void VR_GetEyePoses()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
#ifdef OCULUSSDK042
		g_eye_poses[ovrEye_Left] = ovrHmd_GetEyePose(hmd, ovrEye_Left);
		g_eye_poses[ovrEye_Right] = ovrHmd_GetEyePose(hmd, ovrEye_Right);
#else
		ovrVector3f useHmdToEyeViewOffset[2] = { g_eye_render_desc[0].HmdToEyeViewOffset, g_eye_render_desc[1].HmdToEyeViewOffset };
		ovrHmd_GetEyePoses(hmd, g_ovr_frameindex, useHmdToEyeViewOffset, g_eye_poses, nullptr);
#endif
	}
#endif
}

void ReadHmdOrientation(float *roll, float *pitch, float *yaw, float *x, float *y, float *z)
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift && hmd)
	{
		// we can only call GetEyePose between BeginFrame and EndFrame
#ifdef OCULUSSDK042
		g_vr_lock.lock();
		g_eye_poses[ovrEye_Left] = ovrHmd_GetEyePose(hmd, ovrEye_Left);
		g_eye_poses[ovrEye_Right] = ovrHmd_GetEyePose(hmd, ovrEye_Right);
		g_vr_lock.unlock();
#else
		ovrVector3f useHmdToEyeViewOffset[2] = { g_eye_render_desc[0].HmdToEyeViewOffset, g_eye_render_desc[1].HmdToEyeViewOffset };
		ovrHmd_GetEyePoses(hmd, g_ovr_frameindex, useHmdToEyeViewOffset, g_eye_poses, nullptr);
#endif
		//ovrTrackingState ss = ovrHmd_GetTrackingState(hmd, g_rift_frame_timing.ScanoutMidpointSeconds);
		//if (ss.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked))
		{
			//OVR::Posef pose = ss.HeadPose.ThePose;
			OVR::Posef pose = g_eye_poses[ovrEye_Left];
			float ya = 0.0f, p = 0.0f, r = 0.0f;
			pose.Rotation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&ya, &p, &r);
			*roll = -RADIANS_TO_DEGREES(r);  // ???
			*pitch = -RADIANS_TO_DEGREES(p); // should be degrees down
			*yaw = -RADIANS_TO_DEGREES(ya);   // should be degrees right
			*x = pose.Translation.x;
			*y = pose.Translation.y;
			*z = pose.Translation.z;
		}
	}
	else
#endif
#ifdef _WIN32
	if (g_has_vr920 && Vuzix_GetTracking)
#endif
	{
#ifdef _WIN32
		LONG ya = 0, p = 0, r = 0;
		if (Vuzix_GetTracking(&ya, &p, &r) == ERROR_SUCCESS)
		{
			*yaw = -ya * 180.0f / 32767.0f;
			*pitch = p * -180.0f / 32767.0f;
			*roll = r * 180.0f / 32767.0f;
			*x = 0;
			*y = 0;
			*z = 0;
		}
#endif
	}
}

void UpdateHeadTrackingIfNeeded()
{
	if (g_new_tracking_frame)
	{
		float x = 0, y = 0, z = 0, roll = 0, pitch = 0, yaw = 0;
		ReadHmdOrientation(&roll, &pitch, &yaw, &x, &y, &z);
		g_head_tracking_position[0] = -x;
		g_head_tracking_position[1] = -y;
		g_head_tracking_position[2] = 0.06f-z;
		Matrix33 m, yp, ya, p, r;
		Matrix33::RotateY(ya, DEGREES_TO_RADIANS(yaw));
		Matrix33::RotateX(p, DEGREES_TO_RADIANS(pitch));
		Matrix33::Multiply(p, ya, yp);
		Matrix33::RotateZ(r, DEGREES_TO_RADIANS(roll));
		Matrix33::Multiply(r, yp, m);
		Matrix44::LoadMatrix33(g_head_tracking_matrix, m);
		g_new_tracking_frame = false;
	}
}

void VR_GetProjectionHalfTan(float &hmd_halftan)
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		hmd_halftan = fabs(g_eye_fov[0].LeftTan);
		if (fabs(g_eye_fov[0].RightTan) > hmd_halftan)
			hmd_halftan = fabs(g_eye_fov[0].RightTan);
		if (fabs(g_eye_fov[0].UpTan) > hmd_halftan)
			hmd_halftan = fabs(g_eye_fov[0].UpTan);
		if (fabs(g_eye_fov[0].DownTan) > hmd_halftan)
			hmd_halftan = fabs(g_eye_fov[0].DownTan);
	}
	else
#endif
	{
		hmd_halftan = tan(DEGREES_TO_RADIANS(32.0f / 2))*3.0f / 4.0f;
	}
}

void VR_GetProjectionMatrices(Matrix44 &left_eye, Matrix44 &right_eye, float znear, float zfar)
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		ovrMatrix4f rift_left = ovrMatrix4f_Projection(g_eye_fov[0], znear, zfar, true);
		ovrMatrix4f rift_right = ovrMatrix4f_Projection(g_eye_fov[1], znear, zfar, true);
		Matrix44::Set(left_eye, rift_left.M[0]);
		Matrix44::Set(right_eye, rift_right.M[0]);
	}
	else
#endif
	{
		Matrix44::LoadIdentity(left_eye);
		left_eye.data[10] = -znear / (zfar - znear);
		left_eye.data[11] = -zfar*znear / (zfar - znear);
		left_eye.data[14] = -1.0f;
		left_eye.data[15] = 0.0f;
		// 32 degrees HFOV, 4:3 aspect ratio
		left_eye.data[0 * 4 + 0] = 1.0f / tan(32.0f / 2.0f * 3.1415926535f / 180.0f);
		left_eye.data[1 * 4 + 1] = 4.0f / 3.0f * left_eye.data[0 * 4 + 0];
		Matrix44::Set(right_eye, left_eye.data);
	}
}

void VR_GetEyePos(float *posLeft, float *posRight)
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
#ifdef OCULUSSDK042
		posLeft[0] = g_eye_render_desc[0].ViewAdjust.x;
		posLeft[1] = g_eye_render_desc[0].ViewAdjust.y;
		posLeft[2] = g_eye_render_desc[0].ViewAdjust.z;
		posRight[0] = g_eye_render_desc[1].ViewAdjust.x;
		posRight[1] = g_eye_render_desc[1].ViewAdjust.y;
		posRight[2] = g_eye_render_desc[1].ViewAdjust.z;
#else
		posLeft[0] = g_eye_render_desc[0].HmdToEyeViewOffset.x;
		posLeft[1] = g_eye_render_desc[0].HmdToEyeViewOffset.y;
		posLeft[2] = g_eye_render_desc[0].HmdToEyeViewOffset.z;
		posRight[0] = g_eye_render_desc[1].HmdToEyeViewOffset.x;
		posRight[1] = g_eye_render_desc[1].HmdToEyeViewOffset.y;
		posRight[2] = g_eye_render_desc[1].HmdToEyeViewOffset.z;
#endif
	}
	else
#endif
	{
		// assume 62mm IPD
		posLeft[0] = -0.031f;
		posRight[0] = 0.031f;
		posLeft[1] = posRight[1] = 0;
		posLeft[2] = posRight[2] = 0;
	}
}

bool VR_GetLeftHydraPos(float *pos)
{
	pos[0] = -0.15f;
	pos[1] = -0.30f;
	pos[2] = -0.4f;
	return true;
}

bool VR_GetRightHydraPos(float *pos)
{
	pos[0] = 0.15f;
	pos[1] = -0.30f;
	pos[2] = -0.4f;
	return true;
}

void VR_SetGame(bool is_wii, bool is_nand, std::string id)
{
	g_is_nes = false;
	// GameCube uses GameCube controller
	if (!is_wii)
	{
		vr_left_controller = CS_GC_LEFT;
		vr_right_controller = CS_GC_RIGHT;
	}
	// Wii Discs or homebrew files use the Wiimote and Nunchuk
	else if (!is_nand)
	{
		vr_left_controller = CS_NUNCHUK;
		vr_right_controller = CS_WIIMOTE;
	}
	else
	{
		char c = ' ';
		if (id.length() > 0)
			c = id[0];
		switch (c)
		{
		case 'C':
		case 'X':
			// C64, MSX (or WiiWare demos)
			vr_left_controller = CS_ARCADE_LEFT;
			vr_right_controller = CS_ARCADE_RIGHT;
			break;
		case 'E':
			// Virtual arcade, Neo Geo
			vr_left_controller = CS_ARCADE_LEFT;
			vr_right_controller = CS_ARCADE_RIGHT;
			break;
		case 'F':
			// NES
			g_is_nes = true;
			if (id.length() > 3 && id[3] == 'J')
			{
				vr_left_controller = CS_FAMICON_LEFT;
				vr_right_controller = CS_FAMICON_RIGHT;
			}
			else
			{
				vr_left_controller = CS_NES_LEFT;
				vr_right_controller = CS_NES_RIGHT;
			}
			break;
		case 'J':
			vr_left_controller = CS_SNES_LEFT;
			// SNES
			if (id.length() > 3 && id[3] == 'E')
				vr_right_controller = CS_SNES_NTSC_RIGHT;
			else
				vr_right_controller = CS_SNES_RIGHT;
			break;
		case 'L':
			// Sega
			vr_left_controller = CS_SEGA_LEFT;
			vr_right_controller = CS_SEGA_RIGHT;
			break;
		case 'M':
			// Sega Genesis
			vr_left_controller = CS_GENESIS_LEFT;
			vr_right_controller = CS_GENESIS_RIGHT;
			break;
		case 'N':
			// N64
			vr_left_controller = CS_N64_LEFT;
			vr_right_controller = CS_N64_RIGHT;
			break;
		case 'P':
		case 'Q':
			// TurboGrafx
			vr_left_controller = CS_TURBOGRAFX_LEFT;
			vr_right_controller = CS_TURBOGRAFX_RIGHT;
			break;
		case 'H':
		case 'W':
		default:
			// WiiWare
			vr_left_controller = CS_NUNCHUK;
			vr_right_controller = CS_WIIMOTE;
			break;
		}
	}
}

ControllerStyle VR_GetHydraStyle(int hand)
{
	if (hand)
		return vr_right_controller;
	else
		return vr_left_controller;
}


void OpcodeReplayBuffer()
{
	// Opcode Replay Buffer Code.  This enables the capture of all the Video Opcodes that occur during a frame,
	// and then plays them back with new headtracking information.  Allows ways to easily set headtracking at 75fps
	// for various games.  In Alpha right now, will crash many games/cause corruption.
	static int extra_video_loops_count = 0;
	static int real_frame_count = 0;
	if (g_ActiveConfig.bOpcodeReplay && SConfig::GetInstance().m_LocalCoreStartupParameter.m_GPUDeterminismMode != GPU_DETERMINISM_FAKE_COMPLETION)
	{
		g_opcode_replay_enabled = true;
		if (g_ActiveConfig.bPullUp20fps)
		{
			if (real_frame_count % 4 == 1)
			{
				g_ActiveConfig.iExtraVideoLoops = 2;
				g_ActiveConfig.iExtraVideoLoopsDivider = 0;
			}
			else
			{
				g_ActiveConfig.iExtraVideoLoops = 3;
				g_ActiveConfig.iExtraVideoLoopsDivider = 0;
			}
		}
		else if (g_ActiveConfig.bPullUp30fps)
		{
			if (real_frame_count % 2 == 1)
			{
				g_ActiveConfig.iExtraVideoLoops = 1;
				g_ActiveConfig.iExtraVideoLoopsDivider = 0;
			}
			else
			{
				g_ActiveConfig.iExtraVideoLoops = 2;
				g_ActiveConfig.iExtraVideoLoopsDivider = 0;
			}
		}
		else if (g_ActiveConfig.bPullUp60fps)
		{
			//if (real_frame_count % 4 == 0)
			//{
			//	g_ActiveConfig.iExtraVideoLoops = 1;
			//	g_ActiveConfig.iExtraVideoLoopsDivider = 0;
			//}
			//else
			//{
			//	g_ActiveConfig.iExtraVideoLoops = 0;
			//	g_ActiveConfig.iExtraVideoLoopsDivider = 0;
			//}
			g_ActiveConfig.iExtraVideoLoops = 1;
			g_ActiveConfig.iExtraVideoLoopsDivider = 3;
		}

		if ((g_opcodereplay_frame && (extra_video_loops_count >= (int)g_ActiveConfig.iExtraVideoLoops)))
		{
			g_opcodereplay_frame = false;
			++real_frame_count;
			extra_video_loops_count = 0;
		}
		else
		{
			if (skipped_opcode_replay_count >= (int)g_ActiveConfig.iExtraVideoLoopsDivider)
			{
				g_opcodereplay_frame = true;
				++extra_video_loops_count;
				skipped_opcode_replay_count = 0;

				for (TimewarpLogEntry& entry : timewarp_logentries)
				{
					//VertexManager::s_pCurBufferPointer = s_pCurBufferPointer_log.at(i);
					//VertexManager::s_pEndBufferPointer = s_pEndBufferPointer_log.at(i);
					//VertexManager::s_pBaseBufferPointer = s_pBaseBufferPointer_log.at(i);

					//if (i == 0)
					//{
					//SCPFifoStruct &fifo = CommandProcessor::fifo;

					//fifo.CPBase = CPBase_log.at(i);
					//fifo.CPEnd = CPEnd_log.at(i);
					//fifo.CPHiWatermark = CPHiWatermark_log.at(i);
					//fifo.CPLoWatermark = CPLoWatermark_log.at(i);
					//fifo.CPReadWriteDistance = CPReadWriteDistance_log.at(i);
					//fifo.CPWritePointer = CPWritePointer_log.at(i);
					//fifo.CPReadPointer = CPReadPointer_log.at(i);
					//fifo.CPBreakpoint = CPBreakpoint_log.at(i);
					//}

					if (entry.is_preprocess_log)
					{
						OpcodeDecoder_Run<true>(entry.timewarp_log, nullptr, false);
					}
					else
					{
						OpcodeDecoder_Run<false>(entry.timewarp_log, nullptr, false);
					}
				}
			}
			else
			{
				++skipped_opcode_replay_count;
			}
			//CPBase_log.resize(0);
			//CPEnd_log.resize(0);
			//CPHiWatermark_log.resize(0);
			//CPLoWatermark_log.resize(0);
			//CPReadWriteDistance_log.resize(0);
			//CPWritePointer_log.resize(0);
			//CPReadPointer_log.resize(0);
			//CPBreakpoint_log.resize(0);
			//CPBase_log.clear();
			//CPEnd_log.clear();
			//CPHiWatermark_log.clear();
			//CPLoWatermark_log.clear();
			//CPReadWriteDistance_log.clear();
			//CPWritePointer_log.clear();
			//CPReadPointer_log.clear();
			//CPBreakpoint_log.clear();
			//s_pCurBufferPointer_log.clear();
			//s_pCurBufferPointer_log.resize(0);
			//s_pEndBufferPointer_log.clear();
			//s_pEndBufferPointer_log.resize(0);
			//s_pBaseBufferPointer_log.clear();
			//s_pBaseBufferPointer_log.resize(0);
			timewarp_logentries.clear();
			timewarp_logentries.resize(0);
		}
	}
	else
	{
		if (g_opcode_replay_enabled)
		{
			timewarp_logentries.clear();
			timewarp_logentries.resize(0);
		}
		g_opcode_replay_enabled = false;
		g_opcodereplay_frame = true; //Don't log frames
	}
}

void OpcodeReplayBufferInline()
{
	// Opcode Replay Buffer Code.  This enables the capture of all the Video Opcodes that occur during a frame,
	// and then plays them back with new headtracking information.  Allows ways to easily set headtracking at 75fps
	// for various games.  In Alpha right now, will crash many games/cause corruption.
	static int real_frame_count = 0;
	int extra_video_loops;
	if (g_ActiveConfig.bOpcodeReplay && SConfig::GetInstance().m_LocalCoreStartupParameter.m_GPUDeterminismMode != GPU_DETERMINISM_FAKE_COMPLETION)
	{
		g_opcode_replay_enabled = true;
		if (g_ActiveConfig.bPullUp60fps)
		{
			if (real_frame_count % 4 == 0)
				extra_video_loops = 1;
			else
				extra_video_loops = 0;
		}
		else if (g_ActiveConfig.bPullUp30fps)
		{
			if (real_frame_count % 2 == 1)
			{
				extra_video_loops = 1;
			}
			else
			{
				extra_video_loops = 2;
			}
		}
		else if (g_ActiveConfig.bPullUp20fps)
		{
			if (real_frame_count % 4 == 1)
			{
				extra_video_loops = 2;
			}
			else
			{
				extra_video_loops = 3;
			}
		}
		else
		{
			extra_video_loops = g_ActiveConfig.iExtraVideoLoops;
		}

		++real_frame_count;
		g_opcodereplay_frame = true;
		skipped_opcode_replay_count = 0;

		for (int num_extra_frames = 0; num_extra_frames < extra_video_loops; ++num_extra_frames)
		{
			for (TimewarpLogEntry& entry : timewarp_logentries)
			{
				if (entry.is_preprocess_log)
				{
					OpcodeDecoder_Run<true>(entry.timewarp_log, nullptr, false);
				}
				else
				{
					OpcodeDecoder_Run<false>(entry.timewarp_log, nullptr, false);
				}
			}
		}
		timewarp_logentries.clear();
		timewarp_logentries.resize(0);
		g_opcodereplay_frame = false;
	}
	else
	{
		if (g_opcode_replay_enabled)
		{
			timewarp_logentries.clear();
			timewarp_logentries.resize(0);
		}
		g_opcode_replay_enabled = false;
		g_opcodereplay_frame = true; //Don't log frames
	}
}
