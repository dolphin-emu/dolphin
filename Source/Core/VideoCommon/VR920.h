// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <Windows.h>

//__declspec(dllimport) extern WORD		IWRGetProductID(void);
//__declspec(dllimport) extern DWORD	IWRGetProductDetails(void);

typedef struct
{
  WORD dll_v1;
  WORD dll_v2;
  WORD dll_v3;
  WORD dll_v4;
  BYTE usb_firmware_major;
  BYTE usb_firmware_minor;
  BYTE tracker_firmware_major;
  BYTE tracker_firmware_minor;
  BYTE video_firmware;
} TVUZIX_VERSION, *PVUZIX_VERSION;

// tracker stuff
typedef DWORD(__cdecl* PVUZIX_DWORD)(void);
typedef WORD(__cdecl* PVUZIX_WORD)(void);
typedef void(__cdecl* PVUZIX_VOID)(void);
typedef DWORD(__cdecl* PVUZIX_LONG3)(LONG*, LONG*, LONG*);
typedef void(__cdecl* PVUZIX_BOOL)(BOOL);
typedef DWORD(__cdecl* PVUZIX_GETVERSION)(PVUZIX_VERSION ver);

extern PVUZIX_DWORD Vuzix_OpenTracker;
extern PVUZIX_LONG3 Vuzix_GetTracking;
extern PVUZIX_VOID Vuzix_ZeroSet;
extern PVUZIX_DWORD Vuzix_BeginCalibration;
extern PVUZIX_BOOL Vuzix_EndCalibration;
extern PVUZIX_BOOL Vuzix_SetFilterState;
extern PVUZIX_VOID Vuzix_CloseTracker;
extern PVUZIX_WORD Vuzix_GetProductID;
extern PVUZIX_DWORD Vuzix_GetProductDetails;
extern PVUZIX_GETVERSION Vuzix_GetVersion;

// VR920 freeze frame stereoscopic 3D stuff
typedef HANDLE(__cdecl* PVUZIX_HANDLE)(void);
typedef void(__cdecl* PVUZIX_CLOSEHANDLE)(HANDLE handle);
typedef BOOL(__cdecl* PVUZIX_HANDLEBOOL)(HANDLE, BOOL);
typedef BYTE(__cdecl* PVUZIX_BYTEHANDLE)(HANDLE, BOOL);

extern PVUZIX_HANDLE Vuzix_OpenStereo;
extern PVUZIX_HANDLEBOOL Vuzix_SetStereo;
extern PVUZIX_HANDLEBOOL Vuzix_SetEye;
extern PVUZIX_CLOSEHANDLE Vuzix_CloseStereo;
extern PVUZIX_BYTEHANDLE Vuzix_WaitForStereoAck;
extern PVUZIX_GETVERSION Vuzix_GetStereoVersion;

extern HANDLE g_vr920_stereo_handle;
extern bool g_has_vr920;
extern TVUZIX_VERSION g_vuzix_version, g_vuzix_3dversion;
extern WORD g_vuzix_productid;
extern DWORD g_vuzix_details;

void LoadVR920();
void FreeVR920();

bool VR920_StartStereo3D();
bool VR920_StopStereo3D();
void VR920_CleanupStereo3D();
