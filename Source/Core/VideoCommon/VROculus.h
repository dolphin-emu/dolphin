// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __INTELLISENSE__
#define HAVE_OCULUSSDK
#endif

#ifdef _WIN32
#include <windows.h>
#undef max
#endif

#ifdef HAVE_OCULUSSDK
#include "OVR_Version.h"
#ifndef OVR_PRODUCT_VERSION
#define OVR_PRODUCT_VERSION 0
#endif

#if OVR_PRODUCT_VERSION >= 1
#define OCULUSSDK044ORABOVE
#include "OVR_CAPI.h"
#include "OVR_CAPI_Audio.h"
typedef ovrSession ovrHmd;
#include "Extras/OVR_Math.h"
#else
#if OVR_MAJOR_VERSION <= 4
#include "Kernel/OVR_Types.h"
#else
#define OCULUSSDK044ORABOVE
#define OVR_DLL_BUILD
#endif
#include "OVR_CAPI.h"
#if OVR_MAJOR_VERSION >= 5
#include "Extras/OVR_Math.h"
#else
#include "Kernel/OVR_Math.h"

// Detect which version of the Oculus SDK we are using
#if OVR_MINOR_VERSION >= 4
#if OVR_BUILD_VERSION >= 4
#define OCULUSSDK044ORABOVE
#elif OVR_BUILD_VERSION >= 3
#define OCULUSSDK043
#else
#define OCULUSSDK042
#endif
#else
Error, Oculus SDK 0.3.x is no longer supported
#endif

extern "C" {
void ovrhmd_EnableHSWDisplaySDKRender(ovrHmd hmd, ovrBool enabled);
}
#endif
#endif

#ifdef HAVE_OPENVR
#define SCM_OCULUS_STR ", Oculus SDK " OVR_VERSION_STRING " or OpenVR"
#else
#define SCM_OCULUS_STR ", Oculus SDK " OVR_VERSION_STRING
#endif
#else
#ifdef _WIN32
#include "OculusSystemLibraryHeader.h"
#define OCULUSSDK044ORABOVE
#ifdef HAVE_OPENVR
#define SCM_OCULUS_STR ", for Oculus DLL " OVR_VERSION_STRING " or OpenVR"
#else
#define SCM_OCULUS_STR ", for Oculus DLL " OVR_VERSION_STRING
#endif
#else
#ifdef HAVE_OPENVR
#define SCM_OCULUS_STR ", OpenVR"
#else
#define SCM_OCULUS_STR ", no Oculus SDK"
#endif
#endif
#endif

#ifdef OVR_MAJOR_VERSION
extern ovrHmd hmd;
extern ovrHmdDesc hmdDesc;
extern ovrFovPort g_eye_fov[2];
extern ovrEyeRenderDesc g_eye_render_desc[2];
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 7
extern ovrFrameTiming g_rift_frame_timing;
#endif
extern ovrPosef g_eye_poses[2], g_front_eye_poses[2];
#endif

#if defined(OVR_MAJOR_VERSION) && (OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 7)
#define ovrHmd_GetFrameTiming ovr_GetFrameTiming
#define ovrHmd_SubmitFrame ovr_SubmitFrame
#define ovrHmd_GetRenderDesc ovr_GetRenderDesc
#define ovrHmd_DestroySwapTextureSet ovr_DestroySwapTextureSet
#define ovrHmd_DestroyMirrorTexture ovr_DestroyMirrorTexture
#define ovrHmd_SetEnabledCaps ovr_SetEnabledCaps
#define ovrHmd_GetEnabledCaps ovr_GetEnabledCaps
#define ovrHmd_ConfigureTracking ovr_ConfigureTracking
#if OVR_PRODUCT_VERSION >= 1
#define ovrHmd_RecenterPose ovr_RecenterTrackingOrigin
#else
#define ovrHmd_RecenterPose ovr_RecenterPose
#endif
#define ovrHmd_Destroy ovr_Destroy
#define ovrHmd_GetFovTextureSize ovr_GetFovTextureSize
#define ovrHmd_GetFloat ovr_GetFloat
#define ovrHmd_SetBool ovr_SetBool
#define ovrHmd_GetTrackingState ovr_GetTrackingState
#endif
