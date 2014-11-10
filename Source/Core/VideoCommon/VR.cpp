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
#include "VideoCommon/VR.h"

void ClearDebugProj();

#ifdef HAVE_OCULUSSDK
ovrHmd hmd = nullptr;
ovrHmdDesc hmdDesc;
ovrFovPort g_eye_fov[2];
ovrEyeRenderDesc g_eye_render_desc[2];
ovrFrameTiming g_rift_frame_timing;
ovrPosef g_eye_poses[2], g_front_eye_poses[2];
std::mutex g_ovr_lock;
int g_ovr_frameindex;
#endif

bool g_force_vr = false;
bool g_has_hmd = false, g_has_rift = false, g_has_vr920 = false;
bool g_new_tracking_frame = true;
Matrix44 g_head_tracking_matrix;
float g_head_tracking_position[3];
int g_hmd_window_width = 0, g_hmd_window_height = 0, g_hmd_window_x = 0, g_hmd_window_y = 0;
const char *g_hmd_device_name = nullptr;

#ifdef _WIN32
static char hmd_device_name[MAX_PATH] = "";
#endif

void NewVRFrame()
{
	g_new_tracking_frame = true;
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
		ovrHmd_Destroy(hmd);
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
		g_ovr_lock.lock();
#ifdef OCULUSSDK042
		g_eye_poses[ovrEye_Left] = ovrHmd_GetEyePose(hmd, ovrEye_Left);
		g_eye_poses[ovrEye_Right] = ovrHmd_GetEyePose(hmd, ovrEye_Right);
#endif
#ifdef OCULUSSDK043
		g_eye_poses[ovrEye_Left] = ovrHmd_GetHmdPosePerEye(hmd, ovrEye_Left);
		g_eye_poses[ovrEye_Right] = ovrHmd_GetHmdPosePerEye(hmd, ovrEye_Right);
#endif
		g_ovr_lock.unlock();
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

