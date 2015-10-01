// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Icuiti/Vuzix VR920 HMD
// Only add this file to Windows projects.

#pragma once

#include <Windows.h>

#include "Common/Assert.h"
#include "Common/Common.h"
#include "VideoCommon/VR920.h"

HANDLE g_vr920_stereo_handle = INVALID_HANDLE_VALUE;
TVUZIX_VERSION g_vuzix_version, g_vuzix_3dversion;
WORD g_vuzix_productid = 0;
DWORD g_vuzix_details = 0;

PVUZIX_DWORD Vuzix_OpenTracker = nullptr;
PVUZIX_LONG3 Vuzix_GetTracking = nullptr;
PVUZIX_VOID Vuzix_ZeroSet = nullptr;
PVUZIX_DWORD Vuzix_BeginCalibration = nullptr;
PVUZIX_BOOL Vuzix_EndCalibration = nullptr;
PVUZIX_BOOL Vuzix_SetFilterState = nullptr;
PVUZIX_VOID Vuzix_CloseTracker = nullptr;
PVUZIX_GETVERSION Vuzix_GetVersion;
PVUZIX_WORD Vuzix_GetProductID = nullptr;
PVUZIX_DWORD Vuzix_GetProductDetails = nullptr;

PVUZIX_HANDLE Vuzix_OpenStereo = nullptr;
PVUZIX_HANDLEBOOL Vuzix_SetStereo = nullptr;
PVUZIX_HANDLEBOOL Vuzix_SetEye = nullptr;
PVUZIX_CLOSEHANDLE Vuzix_CloseStereo = nullptr;
PVUZIX_BYTEHANDLE Vuzix_WaitForStereoAck = nullptr;
PVUZIX_GETVERSION Vuzix_GetStereoVersion;

HMODULE iwrdrv_lib = nullptr;
HMODULE iwrstdrv_lib = nullptr;

bool VR920_StartStereo3D()
{
	if (g_vr920_stereo_handle != INVALID_HANDLE_VALUE)
		return true;
	if (!g_has_vr920)
	{
		LoadVR920();
	}
	if (!iwrstdrv_lib)
	{
		iwrstdrv_lib = LoadLibrary(_T("IWRSTDRV.DLL"));
		if (iwrstdrv_lib)
		{
			Vuzix_OpenStereo = (PVUZIX_HANDLE)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_Open");
			Vuzix_SetStereo = (PVUZIX_HANDLEBOOL)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_SetStereo");
			Vuzix_SetEye = (PVUZIX_HANDLEBOOL)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_SetLR");
			Vuzix_WaitForStereoAck = (PVUZIX_BYTEHANDLE)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_WaitForAck");
			Vuzix_CloseStereo = (PVUZIX_CLOSEHANDLE)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_Close");
			Vuzix_GetStereoVersion = (PVUZIX_GETVERSION)GetProcAddress(iwrstdrv_lib, "IWRSTEREO_GetVersion");
		}
		else
		{
			Vuzix_OpenStereo = nullptr;
			Vuzix_SetStereo = nullptr;
			Vuzix_SetEye = nullptr;
			Vuzix_WaitForStereoAck = nullptr;
			Vuzix_CloseStereo = nullptr;
			Vuzix_GetStereoVersion = nullptr;
			WARN_LOG(VR, "Vuzix VR920 stereoscopic 3D driver not installed. VR920 support will not be available.");
			return false;
		}
	}
	if (Vuzix_GetStereoVersion)
	{
		Vuzix_GetStereoVersion(&g_vuzix_3dversion);
		PanicAlertT("Vuzix 3D DLL %d.%d.%d.%d, tracker: %d.%d,\n usb: %d.%d, video: %d", g_vuzix_3dversion.dll_v1, g_vuzix_3dversion.dll_v2, g_vuzix_3dversion.dll_v3, g_vuzix_3dversion.dll_v4, 
			g_vuzix_3dversion.tracker_firmware_major, g_vuzix_3dversion.tracker_firmware_minor, 
			g_vuzix_3dversion.usb_firmware_major, g_vuzix_3dversion.usb_firmware_minor,
			g_vuzix_3dversion.video_firmware);
	}
	if (Vuzix_OpenStereo && Vuzix_SetStereo && Vuzix_SetEye) {
		g_vr920_stereo_handle = Vuzix_OpenStereo();
		// Unfortunately, this function always succeeds on x64,
		// even if the firmware doesn't support freeze-frame 3D.
		// But it fails correctly on 32 bit.
		if (g_vr920_stereo_handle != INVALID_HANDLE_VALUE) {
			INFO_LOG(VR, "VR920 stereoscopic 3D opened.");
			Vuzix_SetStereo(g_vr920_stereo_handle, true);
			NOTICE_LOG(VR, "VR920 stereoscopic 3D started.");
			Vuzix_SetEye(g_vr920_stereo_handle, 0);
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
	Vuzix_SetEye = nullptr;
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
			Vuzix_GetVersion = (PVUZIX_GETVERSION)GetProcAddress(iwrdrv_lib, "IWRGetVersion");
			Vuzix_GetProductID = (PVUZIX_WORD)GetProcAddress(iwrdrv_lib, "IWRGetProductID");
			Vuzix_GetProductDetails = (PVUZIX_DWORD)GetProcAddress(iwrdrv_lib, "IWRGetProductDetails");
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
			Vuzix_GetVersion = nullptr;
			Vuzix_GetProductID = nullptr;
			Vuzix_GetProductDetails = nullptr;
			WARN_LOG(VR, "Vuzix VR920 tracker driver is not installed, so Dolphin can't support the Vuzix VR920.");
		}
	}
	if ((!g_has_vr920) && Vuzix_OpenTracker && Vuzix_OpenTracker() == ERROR_SUCCESS)
	{
		g_has_vr920 = true;
		NOTICE_LOG(VR, "VR920 head tracker started.");
		if (Vuzix_GetProductID)
			g_vuzix_productid = Vuzix_GetProductID();
		if (Vuzix_GetProductDetails)
			g_vuzix_details = Vuzix_GetProductDetails();
		else if (g_vuzix_productid <= 227)
			g_vuzix_details = 0x30000000;
		else
			g_vuzix_details = 0;
		INFO_LOG(VR, "Vuzix ProductID=%d, Details=%08x", g_vuzix_productid, g_vuzix_details);
		if (Vuzix_GetVersion)
		{
			Vuzix_GetVersion(&g_vuzix_version);
			if (g_vuzix_productid <= 227 && g_vuzix_version.usb_firmware_major * 100 + g_vuzix_version.usb_firmware_minor < 109)
			{
				PanicAlertT("Your Icuiti/Vuzix VR920 firmware (1.%d%02d.%d%02d:%d) is too old to support freezeframe 3D. Go to:\nhttp://www.vuzix.com/support/Downloads_drivers_archive/\nand download and install the latest (1.110.104.13) firmware.",
					g_vuzix_version.usb_firmware_major, g_vuzix_version.usb_firmware_minor,
					g_vuzix_version.tracker_firmware_major, g_vuzix_version.tracker_firmware_minor,
					g_vuzix_version.video_firmware);
			}
			if (g_vuzix_productid <= 227 && g_vuzix_version.usb_firmware_major * 100 + g_vuzix_version.usb_firmware_minor < 110)
			{
				WARN_LOG(VR, "Your Icuiti/Vuzix VR920 firmware (1.%d%02d.%d%02d:%d) is out of date. Go to:\nhttp://www.vuzix.com/support/Downloads_drivers_archive/\nand download and install the latest (1.110.104.13) firmware.",
					g_vuzix_version.usb_firmware_major, g_vuzix_version.usb_firmware_minor,
					g_vuzix_version.tracker_firmware_major, g_vuzix_version.tracker_firmware_minor,
					g_vuzix_version.video_firmware);
			}
		}
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
	Vuzix_GetVersion = nullptr;
	Vuzix_GetProductID = nullptr;
	Vuzix_GetProductDetails = nullptr;
}
