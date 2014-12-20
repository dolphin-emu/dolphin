// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>
#include "VideoCommon/VR920.h"
#endif
#ifdef HAVE_OCULUSSDK
#include "Kernel/OVR_Types.h"
#include "OVR_CAPI.h"
#include "Kernel/OVR_Math.h"
#endif

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

void ClearDebugProj();

#ifdef HAVE_OCULUSSDK
ovrHmd hmd = nullptr;
ovrHmdDesc hmdDesc;
ovrFovPort g_eye_fov[2];
ovrEyeRenderDesc g_eye_render_desc[2];
ovrFrameTiming g_rift_frame_timing;
ovrPosef g_eye_poses[2], g_front_eye_poses[2];
int g_ovr_frameindex;
#endif

std::mutex g_ovr_lock;

bool g_force_vr = false;
bool g_has_hmd = false, g_has_rift = false, g_has_vr920 = false;
bool g_new_tracking_frame = true;
bool g_new_frame_tracker_for_efb_skip = true;
u32 skip_objects_count = 0;
Matrix44 g_head_tracking_matrix;
float g_head_tracking_position[3];
int g_hmd_window_width = 0, g_hmd_window_height = 0, g_hmd_window_x = 0, g_hmd_window_y = 0;
const char *g_hmd_device_name = nullptr;

std::vector<DataReader> timewarp_log;
std::vector<bool> display_list_log;
std::vector<bool> is_preprocess_log;
std::vector<bool> cached_ram_location;
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
	skip_objects_count = 0;
	ClearDebugProj();
}

void InitVR()
{
	g_has_hmd = false;
	g_hmd_device_name = nullptr;
#ifdef HAVE_OCULUSSDK
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
#ifdef HAVE_OCULUSSDK
			if (g_force_vr)
			{
				WARN_LOG(VR, "Forcing VR mode, simulating Oculus Rift DK2.");
				hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
			}
#endif
		}
#ifdef HAVE_OCULUSSDK
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
			g_eye_fov[0] = hmdDesc.DefaultEyeFov[0];
			g_eye_fov[1] = hmdDesc.DefaultEyeFov[1];
			g_hmd_window_x = hmdDesc.WindowsPos.x;
			g_hmd_window_y = hmdDesc.WindowsPos.y;
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
	SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution = 
		StringFromFormat("%dx%d", g_hmd_window_width, g_hmd_window_height);
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos = g_hmd_window_x;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos = g_hmd_window_y;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth = g_hmd_window_width;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight = g_hmd_window_height;
	SConfig::GetInstance().m_special_case = true;
}

void ShutdownVR()
{
#ifdef HAVE_OCULUSSDK
	if (hmd)
	{
		// on my computer, on runtime 0.4.2, the Rift won't switch itself off without this:
		if (!(hmd->HmdCaps & ovrHmdCap_ExtendDesktop))
			ovrHmd_SetEnabledCaps(hmd, ovrHmdCap_DisplayOff);
		ovrHmd_Destroy(hmd);
		g_has_rift = false;
		g_has_hmd = false;
		NOTICE_LOG(VR, "Oculus Rift shut down.");
	}
	ovr_Shutdown();
#endif
}

void ReadHmdOrientation(float *roll, float *pitch, float *yaw, float *x, float *y, float *z)
{
#ifdef HAVE_OCULUSSDK
	if (g_has_rift && hmd)
	{
		// we can only call GetEyePose between BeginFrame and EndFrame
#ifdef OCULUSSDK042
		g_ovr_lock.lock();
		g_eye_poses[ovrEye_Left] = ovrHmd_GetEyePose(hmd, ovrEye_Left);
		g_eye_poses[ovrEye_Right] = ovrHmd_GetEyePose(hmd, ovrEye_Right);
		g_ovr_lock.unlock();
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
			*y = (pose.Translation.y * cos(DEGREES_TO_RADIANS(g_ActiveConfig.fCameraPitch))) - (pose.Translation.z * sin(DEGREES_TO_RADIANS(g_ActiveConfig.fCameraPitch)));
			*z = (pose.Translation.z * cos(DEGREES_TO_RADIANS(g_ActiveConfig.fCameraPitch))) + (pose.Translation.y * sin(DEGREES_TO_RADIANS(g_ActiveConfig.fCameraPitch)));
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
		Matrix33::LoadIdentity(ya);
		Matrix33::RotateY(ya, DEGREES_TO_RADIANS(yaw));
		Matrix33::LoadIdentity(p);
		Matrix33::RotateX(p, DEGREES_TO_RADIANS(pitch));
		Matrix33::Multiply(p, ya, yp);
		Matrix33::LoadIdentity(r);
		Matrix33::RotateZ(r, DEGREES_TO_RADIANS(roll));
		Matrix33::Multiply(r, yp, m);
		Matrix44::LoadMatrix33(g_head_tracking_matrix, m);
		g_new_tracking_frame = false;
	}
}

void OpcodeReplayBuffer()
{
	// Opcode Replay Buffer Code.  This enables the capture of all the Video Opcodes that occur during a frame,
	// and then plays them back with new headtracking information.  Allows ways to easily set headtracking at 75fps
	// for various games.  In Alpha right now, will crash many games/cause corruption.
	static int extra_video_loops_count = 0;
	static int real_frame_count = 0;
	if ((g_ActiveConfig.iExtraVideoLoops || g_ActiveConfig.bPullUp20fps || g_ActiveConfig.bPullUp30fps || g_ActiveConfig.bPullUp60fps) && !(g_ActiveConfig.bPullUp20fpsTimewarp || g_ActiveConfig.bPullUp30fpsTimewarp || g_ActiveConfig.bPullUp60fpsTimewarp))
	{
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

				for (int i = 0; i < timewarp_log.size(); ++i)
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

					if (is_preprocess_log.at(i))
					{
						OpcodeDecoder_Run<true>(timewarp_log.at(i), nullptr, display_list_log.at(i));
					}
					else
					{
						OpcodeDecoder_Run<false>(timewarp_log.at(i), nullptr, display_list_log.at(i));
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
			is_preprocess_log.clear();
			is_preprocess_log.resize(0);
			timewarp_log.clear();
			timewarp_log.resize(0);
			display_list_log.clear();
			display_list_log.resize(0);
		}
	}
	else
	{
		g_opcodereplay_frame = true; //Don't log frames
	}
}