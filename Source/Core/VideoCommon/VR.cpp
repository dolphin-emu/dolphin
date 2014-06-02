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

#define M_PI 3.14159265358979323846
#define RADIANS_TO_DEGREES(rad) ((float) rad * (float) (180.0 / M_PI))

#ifdef HAVE_OCULUSSDK
ovrHmd hmd = nullptr;
ovrHmdDesc hmdDesc;
ovrFovPort g_eye_fov[2];
ovrEyeRenderDesc g_eye_render_desc[2];
#endif

bool g_has_hmd = false, g_has_rift = false, g_has_vr920 = false;
bool g_new_tracking_frame = true;
Matrix44 g_head_tracking_matrix;
int g_hmd_window_width = 0, g_hmd_window_height = 0;

void NewVRFrame()
{
	g_new_tracking_frame = true;
}

void InitVR()
{
#ifdef HAVE_OCULUSSDK
	ovr_Initialize();
	hmd = ovrHmd_Create(0);
	if (hmd)
	{
		// Get more details about the HMD
		ovrHmd_GetDesc(hmd, &hmdDesc);

		if (ovrHmd_StartSensor(hmd, ovrSensorCap_Orientation | ovrSensorCap_Position | ovrSensorCap_YawCorrection,
			0))
		{
			g_has_rift = true;
			g_has_hmd = true;
			g_hmd_window_width = hmdDesc.Resolution.w;
			g_hmd_window_height = hmdDesc.Resolution.h;
			g_eye_fov[0] = hmdDesc.DefaultEyeFov[0];
			g_eye_fov[1] = hmdDesc.DefaultEyeFov[1];

			NOTICE_LOG(VR, "Oculus Rift head tracker started.");
		}
	}
	else
#endif
	{
		WARN_LOG(VR, "Oculus Rift not detected. Oculus Rift support will not be available.");
#ifdef _WIN32
		LoadVR920();
		if (g_has_vr920)
		{
			g_has_hmd = true;
			g_hmd_window_width = 800;
			g_hmd_window_height = 600;
		}
#endif
	}
}

void ReadHmdOrientation(float *roll, float *pitch, float *yaw)
{
#ifdef HAVE_OCULUSSDK
	if (g_has_rift && hmd)
	{
		//float predictionDelta = in_sensorPrediction.GetFloat() * (1.0f / 1000.0f);
		// Query the HMD for the sensor state at a given time. "0.0" means "most recent time".
		ovrSensorState ss = ovrHmd_GetSensorState(hmd, 0.0);
		if (ss.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked))
		{
			OVR::Transformf pose = ss.Recorded.Pose; // don't use prediction, currently prediction is like random noise.
			float y = 0.0f, p = 0.0f, r = 0.0f;
			pose.Rotation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&y, &p, &r);
			*roll = -RADIANS_TO_DEGREES(r);  // ???
			*pitch = -RADIANS_TO_DEGREES(p); // should be degrees down
			*yaw = -RADIANS_TO_DEGREES(y);   // should be degrees right
		}
	}
	else
#endif
#ifdef _WIN32
	if (g_has_vr920 && Vuzix_GetTracking)
#endif
	{
#ifdef _WIN32
		LONG y = 0, p = 0, r = 0;
		if (Vuzix_GetTracking(&y, &p, &r) == ERROR_SUCCESS)
		{
			*yaw = -y * 180.0f / 32767.0f;
			*pitch = p * -180.0f / 32767.0f;
			*roll = r * 180.0f / 32767.0f;
		}
#endif
	}
}

void UpdateHeadTrackingIfNeeded()
{
	if (g_new_tracking_frame)
	{
		float roll = 0, pitch = 0, yaw = 0;
		ReadHmdOrientation(&roll, &pitch, &yaw);
		Matrix33 m, yp, y, p, r;
		Matrix33::LoadIdentity(y);
		Matrix33::RotateY(y, yaw*3.14159f / 180.0f);
		Matrix33::LoadIdentity(p);
		Matrix33::RotateX(p, pitch*3.14159f / 180.0f);
		Matrix33::Multiply(p, y, yp);
		Matrix33::LoadIdentity(r);
		Matrix33::RotateZ(r, roll*3.14159f / 180.0f);
		Matrix33::Multiply(r, yp, m);
		Matrix44::LoadMatrix33(g_head_tracking_matrix, m);
		g_new_tracking_frame = false;
	}
}

