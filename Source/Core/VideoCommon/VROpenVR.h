// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __INTELLISENSE__
#define HAVE_OPENVR
#endif

#ifdef HAVE_OPENVR
// Supports OpenVR versions 1.0.5, 1.0.2 - 0.9.17
// Does not support 1.0.4 - 1.0.3, or 0.9.16 or below
#include <openvr.h>
#include <string>

// These change their names depending on OpenVR version, so just define them ourselves.
// Handle is an ID3D11Texture. Normalized Z goes from 0 at the viewer to 1 at the far clip plane.
#define OPENVR_DirectX (decltype(vr::Texture_t::eType))0
// Handle is an OpenGL texture name or an OpenGL render buffer name, depending on submit flags. Normalized Z goes from 1 at the viewer to -1 at the far clip plane.
#define OPENVR_OpenGL (decltype(vr::Texture_t::eType))1
// Handle is a pointer to a VRVulkanTextureData_t structure
#define OPENVR_Vulkan (decltype(vr::Texture_t::eType))2

#ifdef INVALID_SHARED_TEXTURE_HANDLE
#define OPENVR_103_OR_ABOVE
#define OPENVR_0921_OR_ABOVE
#define OPENVR_0903_OR_ABOVE
#elif defined(INVALID_TRACKED_CAMERA_HANDLE)
#define OPENVR_0921_OR_ABOVE
#define OPENVR_0903_OR_ABOVE
#elif defined(VR_INTERFACE)
#define OPENVR_0903_OR_ABOVE
#endif

// There's no way to detect 1.04 or 1.05, so just assume
#ifdef OPENVR_103_OR_ABOVE
#define OPENVR_104_OR_ABOVE
#define OPENVR_105_OR_ABOVE
#endif

extern vr::IVRSystem *m_pHMD;
extern vr::IVRRenderModels *m_pRenderModels;
extern vr::IVRCompositor *m_pCompositor;
extern std::string m_strDriver;
extern std::string m_strDisplay;
extern vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
extern bool m_bUseCompositor;
extern bool m_rbShowTrackedDevice[vr::k_unMaxTrackedDeviceCount];
extern int m_iValidPoseCount;
#endif
