// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Icuiti/Vuzix VR920 HMD
// Only add this file to Windows projects.

#pragma once

#include <Windows.h>

#include "Common/Common.h"
#include "VideoCommon/VR920.h"

HANDLE g_vr920_stereo_handle = INVALID_HANDLE_VALUE;

PVUZIX_DWORD Vuzix_OpenTracker = nullptr;
PVUZIX_LONG3 Vuzix_GetTracking = nullptr;
PVUZIX_VOID Vuzix_ZeroSet = nullptr;
PVUZIX_DWORD Vuzix_BeginCalibration = nullptr;
PVUZIX_BOOL Vuzix_EndCalibration = nullptr;
PVUZIX_BOOL Vuzix_SetFilterState = nullptr;
PVUZIX_VOID Vuzix_CloseTracker = nullptr;

PVUZIX_HANDLE Vuzix_OpenStereo = nullptr;
PVUZIX_HANDLEBOOL Vuzix_SetStereo = nullptr;
PVUZIX_HANDLEBOOL Vuzix_SetLR = nullptr;
PVUZIX_CLOSEHANDLE Vuzix_CloseStereo = nullptr;
PVUZIX_BYTEHANDLE Vuzix_WaitForStereoAck = nullptr;

HMODULE iwrdrv_lib = nullptr;
HMODULE iwrstdrv_lib = nullptr;

bool VR920_StartStereo3D()
{
	if (g_vr920_stereo_handle != INVALID_HANDLE_VALUE)
		return true;
	if (!iwrstdrv_lib)
	{
		iwrstdrv_lib = LoadLibrary(_T("IWRSTDRV.DLL"));
		if (iwrstdrv_lib)
		{
			Vuzix_OpenStereo = (PVUZIX_HANDLE)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_Open");
			Vuzix_SetStereo = (PVUZIX_HANDLEBOOL)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_SetStereo");
			Vuzix_SetLR = (PVUZIX_HANDLEBOOL)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_SetLR");
			Vuzix_WaitForStereoAck = (PVUZIX_BYTEHANDLE)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_WaitForAck");
			Vuzix_CloseStereo = (PVUZIX_CLOSEHANDLE)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_Close");
		}
		else
		{
			Vuzix_OpenStereo = nullptr;
			Vuzix_SetStereo = nullptr;
			Vuzix_SetLR = nullptr;
			Vuzix_WaitForStereoAck = nullptr;
			Vuzix_CloseStereo = nullptr;
			WARN_LOG(VR, "Vuzix VR920 stereoscopic 3D driver not installed. VR920 support will not be available.");
			return false;
		}
	}
	if (Vuzix_OpenStereo && Vuzix_SetStereo && Vuzix_SetLR) {
		g_vr920_stereo_handle = Vuzix_OpenStereo();
		if (g_vr920_stereo_handle != INVALID_HANDLE_VALUE) {
			INFO_LOG(VR, "VR920 stereoscopic 3D opened.");
			Vuzix_SetStereo(g_vr920_stereo_handle, true);
			NOTICE_LOG(VR, "VR920 stereoscopic 3D started.");
		}
		else
		{
			DWORD error = GetLastError();
			switch (error)
			{
			case ERROR_INVALID_FUNCTION:
				ERROR_LOG(VR, "Vuzix VR920 needs firmware update to start 3D mode!");
				break;
			case ERROR_DEV_NOT_EXIST:
				ERROR_LOG(VR, "Vuzix VR920 is not plugged into USB!");
				break;
			default:
				ERROR_LOG(VR, "Unknown error starting VR920 3D mode!");
			}
			return false;
		}
		return true;
	}
	return false;
}

bool VR920_StopStereo3D()
{
	if (g_vr920_stereo_handle == INVALID_HANDLE_VALUE)
		return true;
	if (Vuzix_SetStereo)
	{
		Vuzix_SetStereo(g_vr920_stereo_handle, false);
		NOTICE_LOG(VR, "VR920 stereoscopic 3D stopped.");
		return true;
	}
	else return false;
}

void VR920_CleanupStereo3D()
{
	NOTICE_LOG(VR, "Cleaning up VR920 stereoscopic 3D.");
	VR920_StopStereo3D();
	if (Vuzix_CloseStereo)
		Vuzix_CloseStereo(g_vr920_stereo_handle);
	g_vr920_stereo_handle = INVALID_HANDLE_VALUE;
	Vuzix_OpenStereo = nullptr;
	Vuzix_SetStereo = nullptr;
	Vuzix_SetLR = nullptr;
	Vuzix_WaitForStereoAck = nullptr;
	Vuzix_CloseStereo = nullptr;
	if (iwrstdrv_lib)
		FreeLibrary(iwrstdrv_lib);
	iwrstdrv_lib = nullptr;
}

void LoadVR920()
{
	g_has_vr920 = false;
	if (!iwrdrv_lib)
	{
		iwrdrv_lib = LoadLibrary(_T("IWEARDRV.DLL"));
		if (iwrdrv_lib)
		{
			Vuzix_OpenTracker = (PVUZIX_DWORD)GetProcAddress(iwrdrv_lib, "IWROpenTracker");
			Vuzix_GetTracking = (PVUZIX_LONG3)GetProcAddress(iwrdrv_lib, "IWRGetTracking");
			Vuzix_ZeroSet = (PVUZIX_VOID)GetProcAddress(iwrdrv_lib, "IWRZeroSet");
			Vuzix_BeginCalibration = (PVUZIX_DWORD)GetProcAddress(iwrdrv_lib, "IWRBeginCalibrate");
			Vuzix_EndCalibration = (PVUZIX_BOOL)GetProcAddress(iwrdrv_lib, "IWREndCalibrate");
			Vuzix_SetFilterState = (PVUZIX_BOOL)GetProcAddress(iwrdrv_lib, "IWRSetFilterState");
			Vuzix_CloseTracker = (PVUZIX_VOID)GetProcAddress(iwrdrv_lib, "IWRCloseTracker");
		}
		else
		{
			Vuzix_OpenTracker = nullptr;
			Vuzix_GetTracking = nullptr;
			Vuzix_ZeroSet = nullptr;
			Vuzix_BeginCalibration = nullptr;
			Vuzix_EndCalibration = nullptr;
			Vuzix_SetFilterState = nullptr;
			Vuzix_CloseTracker = nullptr;
			WARN_LOG(VR, "Vuzix VR920 tracker driver is not installed, so Dolphin can't support the Vuzix VR920.");
		}
	}
	if ((!g_has_vr920) && Vuzix_OpenTracker && Vuzix_OpenTracker() == ERROR_SUCCESS)
	{
		g_has_vr920 = true;
		NOTICE_LOG(VR, "VR920 head tracker started.");
	}
}

void FreeVR920()
{
	g_has_vr920 = false;
	if (iwrdrv_lib)
	{
		if (Vuzix_CloseTracker)
			Vuzix_CloseTracker();
		FreeLibrary(iwrdrv_lib);
	}
	Vuzix_OpenTracker = nullptr;
	Vuzix_GetTracking = nullptr;
	Vuzix_ZeroSet = nullptr;
	Vuzix_BeginCalibration = nullptr;
	Vuzix_EndCalibration = nullptr;
	Vuzix_SetFilterState = nullptr;
	Vuzix_CloseTracker = nullptr;
}
