// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Distances are in metres, angles are in degrees.
const float DEFAULT_VR_UNITS_PER_METRE = 1.0f, DEFAULT_VR_FREE_LOOK_SENSITIVITY = 1.0f, DEFAULT_VR_HUD_DISTANCE = 1.5f,
	DEFAULT_VR_HUD_THICKNESS = 0.5f, DEFAULT_VR_HUD_3D_CLOSER = 0.5f,
	DEFAULT_VR_CAMERA_FORWARD = 0.0f, DEFAULT_VR_CAMERA_PITCH = 0.0f, DEFAULT_VR_AIM_DISTANCE = 7.0f,
	DEFAULT_VR_SCREEN_HEIGHT = 2.0f, DEFAULT_VR_SCREEN_DISTANCE = 1.5f, DEFAULT_VR_SCREEN_THICKNESS = 0.5f,
	DEFAULT_VR_SCREEN_UP = 0.0f, DEFAULT_VR_SCREEN_RIGHT = 0.0f, DEFAULT_VR_SCREEN_PITCH = 0.0f,
	DEFAULT_VR_TIMEWARP_TWEAK = 0, DEFAULT_VR_MIN_FOV = 10.0f, DEFAULT_VR_MOTION_SICKNESS_FOV = 45.0f;
const int DEFAULT_VR_EXTRA_FRAMES = 0;
const int DEFAULT_VR_EXTRA_VIDEO_LOOPS = 0;
const int DEFAULT_VR_EXTRA_VIDEO_LOOPS_DIVIDER = 0;

#ifdef HAVE_OCULUSSDK
#include "OVR_Version.h"
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

extern "C"
{
	void ovrhmd_EnableHSWDisplaySDKRender(ovrHmd hmd, ovrBool enabled);
}
#endif

#ifdef HAVE_OPENVR
#define SCM_OCULUS_STR ", Oculus SDK " OVR_VERSION_STRING " or SteamVR"
#else
#define SCM_OCULUS_STR ", Oculus SDK " OVR_VERSION_STRING
#endif
#else
#ifdef _WIN32
#include "OculusSystemLibraryHeader.h"
#define OCULUSSDK044ORABOVE
#ifdef HAVE_OPENVR
#define SCM_OCULUS_STR ", for Oculus DLL " OVR_VERSION_STRING " or SteamVR"
#else
#define SCM_OCULUS_STR ", for Oculus DLL " OVR_VERSION_STRING
#endif
#else
#ifdef HAVE_OPENVR
#define SCM_OCULUS_STR ", SteamVR"
#else
#define SCM_OCULUS_STR ", no Oculus SDK"
#endif
#endif
#endif

#ifdef HAVE_OPENVR
#include <openvr.h>
#endif

#include <atomic>
#include <mutex>

#include "Common/MathUtil.h"
#include "VideoCommon/DataReader.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define RADIANS_TO_DEGREES(rad) ((float) rad * (float) (180.0 / M_PI))
#define DEGREES_TO_RADIANS(deg) ((float) deg * (float) (M_PI / 180.0))
//#define RECURSIVE_OPCODE
#define INLINE_OPCODE

typedef enum
{
	CS_HYDRA_LEFT,
	CS_HYDRA_RIGHT,
	CS_WIIMOTE,
	CS_NUNCHUK,
	CS_WIIMOTE_LEFT,
	CS_WIIMOTE_RIGHT,
	CS_CLASSIC_LEFT,
	CS_CLASSIC_RIGHT,
	CS_GC_LEFT,
	CS_GC_RIGHT,
	CS_N64_LEFT,
	CS_N64_RIGHT,
	CS_SNES_LEFT,
	CS_SNES_RIGHT,
	CS_SNES_NTSC_RIGHT,
	CS_NES_LEFT,
	CS_NES_RIGHT,
	CS_FAMICON_LEFT,
	CS_FAMICON_RIGHT,
	CS_SEGA_LEFT,
	CS_SEGA_RIGHT,
	CS_GENESIS_LEFT,
	CS_GENESIS_RIGHT,
	CS_TURBOGRAFX_LEFT,
	CS_TURBOGRAFX_RIGHT,
	CS_PCENGINE_LEFT,
	CS_PCENGINE_RIGHT,
	CS_ARCADE_LEFT,
	CS_ARCADE_RIGHT
} ControllerStyle;

void InitVR();
void VR_StopRendering();
void ShutdownVR();
void VR_RecenterHMD();
void VR_ConfigureHMDTracking();
void VR_ConfigureHMDPrediction();
void NewVRFrame();
void VR_BeginFrame();
void VR_GetEyePoses();
void ReadHmdOrientation(float *roll, float *pitch, float *yaw, float *x, float *y, float *z);
void UpdateHeadTrackingIfNeeded();
void VR_GetProjectionHalfTan(float &hmd_halftan);
void VR_GetProjectionMatrices(Matrix44 &left_eye, Matrix44 &right_eye, float znear, float zfar);
void VR_GetEyePos(float *posLeft, float *posRight);
void VR_GetFovTextureSize(int *width, int *height);

void VR_SetGame(bool is_wii, bool is_nand, std::string id);
bool VR_GetLeftHydraPos(float *pos);
bool VR_GetRightHydraPos(float *pos);
ControllerStyle VR_GetHydraStyle(int hand);

void OpcodeReplayBuffer();
void OpcodeReplayBufferInline();

extern bool g_force_vr, g_prefer_steamvr;
extern bool g_has_hmd, g_has_rift, g_has_vr920, g_has_steamvr, g_is_direct_mode, g_is_nes;
extern bool g_new_tracking_frame;
extern bool g_new_frame_tracker_for_efb_skip;
extern u32 skip_objects_count;
extern Matrix44 g_head_tracking_matrix;
extern float g_head_tracking_position[3];
extern float g_left_hand_tracking_position[3], g_right_hand_tracking_position[3];
extern int g_hmd_window_width, g_hmd_window_height, g_hmd_window_x, g_hmd_window_y; 
extern const char *g_hmd_device_name;
extern bool g_fov_changed, g_vr_black_screen;
extern bool g_vr_had_3D_already;
extern float vr_freelook_speed;
extern float vr_widest_3d_HFOV;
extern float vr_widest_3d_VFOV;
extern float vr_widest_3d_zNear;
extern float vr_widest_3d_zFar;
extern float g_game_camera_pos[3];
extern Matrix44 g_game_camera_rotmat;

//Opcode Replay Buffer
struct TimewarpLogEntry {
	DataReader timewarp_log;
	bool is_preprocess_log;
};
extern std::vector<TimewarpLogEntry> timewarp_logentries;
extern bool g_opcode_replay_enabled;
extern bool g_new_frame_just_rendered;
extern bool g_first_pass;
extern bool g_first_pass_vs_constants;
extern bool g_opcode_replay_frame;
extern bool g_opcode_replay_log_frame;
extern int skipped_opcode_replay_count;

//extern std::vector<u8*> s_pCurBufferPointer_log;
//extern std::vector<u8*> s_pBaseBufferPointer_log;
//extern std::vector<u8*> s_pEndBufferPointer_log;

//extern std::vector<u32> CPBase_log;
//extern std::vector<u32> CPEnd_log;
//extern std::vector<u32> CPHiWatermark_log;
//extern std::vector<u32> CPLoWatermark_log;
//extern std::vector<u32> CPReadWriteDistance_log;
//extern std::vector<u32> CPWritePointer_log;
//extern std::vector<u32> CPReadPointer_log;
//extern std::vector<u32> CPBreakpoint_log;

extern std::mutex g_vr_lock;
extern std::atomic<u32> g_drawn_vr;

extern bool debug_nextScene;

#ifdef HAVE_OPENVR
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

#ifdef OVR_MAJOR_VERSION
extern ovrHmd hmd;
extern ovrHmdDesc hmdDesc;
extern ovrFovPort g_eye_fov[2];
extern ovrEyeRenderDesc g_eye_render_desc[2];
#if OVR_MAJOR_VERSION <= 7
extern ovrFrameTiming g_rift_frame_timing;
#endif
extern ovrPosef g_eye_poses[2], g_front_eye_poses[2];
extern int g_ovr_frameindex;
#endif

#if defined(OVR_MAJOR_VERSION) && OVR_MAJOR_VERSION >= 7
#define ovrHmd_GetFrameTiming ovr_GetFrameTiming
#define ovrHmd_SubmitFrame ovr_SubmitFrame
#define ovrHmd_GetRenderDesc ovr_GetRenderDesc
#define ovrHmd_DestroySwapTextureSet ovr_DestroySwapTextureSet
#define ovrHmd_DestroyMirrorTexture ovr_DestroyMirrorTexture
#define ovrHmd_SetEnabledCaps ovr_SetEnabledCaps
#define ovrHmd_GetEnabledCaps ovr_GetEnabledCaps
#define ovrHmd_ConfigureTracking ovr_ConfigureTracking
#define ovrHmd_RecenterPose ovr_RecenterPose
#define ovrHmd_Destroy ovr_Destroy
#define ovrHmd_GetFovTextureSize ovr_GetFovTextureSize
#endif

#ifdef _WIN32
extern LUID *g_hmd_luid;
#endif
