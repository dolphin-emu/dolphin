// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __INTELLISENSE__
#define HAVE_OPENVR
#endif

#ifdef HAVE_OPENVR
#include <openvr.h>
#include <string>

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
