// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <Objbase.h>
#include <mmdeviceapi.h>
#include <setupapi.h>
#include "VideoCommon/VR920.h"
// clang-format on
#endif

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/HW/WiimoteEmu/HydraTLayer.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VROculus.h"

const char* scm_vr_sdk_str = SCM_OCULUS_STR;

float g_current_fps = 60.0f, g_current_speed = 0.0f;  // g_current_speed is a percentage

#ifdef HAVE_OPENVR
#include "VideoCommon\VROpenVR.h"

vr::IVRSystem* m_pHMD;
vr::IVRRenderModels* m_pRenderModels;
vr::IVRCompositor* m_pCompositor;
std::string m_strDriver;
std::string m_strDisplay;
vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
bool m_bUseCompositor = true;
bool m_rbShowTrackedDevice[vr::k_unMaxTrackedDeviceCount];
int m_iValidPoseCount;
#endif

void ClearDebugProj();

#ifdef OVR_MAJOR_VERSION
ovrHmd hmd = nullptr;
ovrHmdDesc hmdDesc;
ovrFovPort g_best_eye_fov[2], g_eye_fov[2], g_last_eye_fov[2];
ovrEyeRenderDesc g_eye_render_desc[2];
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 7
ovrFrameTiming g_rift_frame_timing;
#endif
ovrPosef g_eye_poses[2], g_front_eye_poses[2];
int g_ovr_frameindex;
#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 7
ovrGraphicsLuid luid;
#endif
#endif

#ifdef _WIN32
LUID* g_hmd_luid = nullptr;
#endif

std::mutex g_vr_lock;

#if defined(OVR_MAJOR_VERSION)
#if OVR_PRODUCT_VERSION >= 1
bool g_vr_cant_motion_blur = true, g_vr_must_motion_blur = false;
bool g_vr_has_dynamic_predict = false, g_vr_has_configure_rendering = false,
     g_vr_has_hq_distortion = true;
bool g_vr_has_configure_tracking = false, g_vr_has_asynchronous_timewarp = true;
bool g_vr_supports_extended = false;
#elif OVR_MAJOR_VERSION >= 7
bool g_vr_cant_motion_blur = true, g_vr_must_motion_blur = false;
bool g_vr_has_dynamic_predict = false, g_vr_has_configure_rendering = false,
     g_vr_has_hq_distortion = true;
bool g_vr_has_configure_tracking = true, g_vr_has_asynchronous_timewarp = false;
bool g_vr_supports_extended = false;
#else
bool g_vr_cant_motion_blur = false, g_vr_must_motion_blur = false;
bool g_vr_has_dynamic_predict = true, g_vr_has_configure_rendering = true,
     g_vr_has_hq_distortion = true;
bool g_vr_has_configure_tracking = true, g_vr_has_asynchronous_timewarp = false;
bool g_vr_supports_extended = true;
#endif
#else
bool g_vr_cant_motion_blur = true, g_vr_must_motion_blur = false;
bool g_vr_has_dynamic_predict = false, g_vr_has_configure_rendering = false,
     g_vr_has_hq_distortion = true;
bool g_vr_has_configure_tracking = true, g_vr_has_asynchronous_timewarp = false;
bool g_vr_supports_extended = false;
#endif

#if defined(OVR_MAJOR_VERSION) && OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 5
bool g_vr_has_timewarp_tweak = true;
bool g_vr_needs_DXGIFactory1 = false;
bool g_vr_needs_endframe = true;
bool g_vr_can_disable_hsw = true;
#else
bool g_vr_has_timewarp_tweak = false;
#if defined(OVR_MAJOR_VERSION)
bool g_vr_can_disable_hsw = false;
bool g_vr_needs_DXGIFactory1 = true;
#else
bool g_vr_can_disable_hsw = true;
bool g_vr_needs_DXGIFactory1 = false;
#endif
bool g_vr_needs_endframe = false;
#endif

bool g_vr_should_swap_buffers = true, g_vr_dont_vsync = false;

bool g_force_vr = false, g_prefer_openvr = false, g_one_hmd = false;
bool g_has_hmd = false, g_has_two_hmds = false, g_has_rift = false, g_has_vr920 = false, g_has_openvr = false, g_openvr_is_vive = true, g_openvr_is_rift = false;
bool g_is_direct_mode = false, g_is_nes = false;
bool g_new_tracking_frame = true;
bool g_new_frame_tracker_for_efb_skip = true;
u32 skip_objects_count = 0;
Matrix44 g_head_tracking_matrix;
float g_head_tracking_position[3];
float g_left_hand_tracking_position[3], g_right_hand_tracking_position[3];
int g_hmd_window_width = 0, g_hmd_window_height = 0, g_hmd_window_x = 0, g_hmd_window_y = 0,
    g_hmd_refresh_rate = 90;
const char* g_hmd_device_name = nullptr;
float g_vr_speed = 0;
float vr_freelook_speed = 0;
bool g_fov_changed = false, g_vr_black_screen = false;
bool g_vr_had_3D_already = false;
float vr_widest_3d_HFOV = 0;
float vr_widest_3d_VFOV = 0;
float vr_widest_3d_zNear = 0;
float vr_widest_3d_zFar = 0;
float g_game_camera_pos[3];
Matrix44 g_game_camera_rotmat;

// used for calculating acceleration of vive controllers
double g_older_tracking_time = 0, g_old_tracking_time = 0, g_last_tracking_time = 0;
float g_openvr_ipd = 0.064f;

u8 g_vr_reading_wiimote_accel[5] = {}, g_vr_reading_wiimote_ir[5] = {},
   g_vr_reading_wiimote_ext[5] = {};
bool g_vr_has_ir = false;
float g_vr_ir_x = 0, g_vr_ir_y = 0, g_vr_ir_z = 0;

ControllerStyle vr_left_controller = CS_HYDRA_LEFT, vr_right_controller = CS_HYDRA_RIGHT;

std::vector<TimewarpLogEntry> timewarp_logentries;

bool g_opcode_replay_enabled = false;
bool g_new_frame_just_rendered = false;
bool g_first_pass = true;
bool g_first_pass_vs_constants = true;
bool g_opcode_replay_frame = false;
bool g_opcode_replay_log_frame = false;
int skipped_opcode_replay_count = 0;

// std::vector<u8*> s_pCurBufferPointer_log;
// std::vector<u8*> s_pBaseBufferPointer_log;
// std::vector<u8*> s_pEndBufferPointer_log;

// std::vector<u32> CPBase_log;
// std::vector<u32> CPEnd_log;
// std::vector<u32> CPHiWatermark_log;
// std::vector<u32> CPLoWatermark_log;
// std::vector<u32> CPReadWriteDistance_log;
// std::vector<u32> CPWritePointer_log;
// std::vector<u32> CPReadPointer_log;
// std::vector<u32> CPBreakpoint_log;

#ifdef _WIN32
static char hmd_device_name[MAX_PATH] = "";
#endif

void VR_NewVRFrame()
{
  g_new_tracking_frame = true;
  g_new_frame_tracker_for_efb_skip = true;
  if (!g_vr_had_3D_already)
  {
    Matrix44::LoadIdentity(g_game_camera_rotmat);
  }
  g_vr_had_3D_already = false;
  skip_objects_count = 0;
  ClearDebugProj();

  // Prevent motion sickness: estimate how fast we are moving, and reduce the FOV if we are moving
  // fast
  g_vr_speed = 0;
  if (g_ActiveConfig.bMotionSicknessAlways)
  {
    g_vr_speed = 1.0f;
  }
  else
  {
    if (g_ActiveConfig.bMotionSicknessDPad)
      g_vr_speed += HydraTLayer::vr_gc_dpad_speed + HydraTLayer::vr_cc_dpad_speed +
                    HydraTLayer::vr_wm_dpad_speed;
    if (g_ActiveConfig.bMotionSicknessLeftStick)
      g_vr_speed += HydraTLayer::vr_gc_leftstick_speed + HydraTLayer::vr_wm_leftstick_speed;
    if (g_ActiveConfig.bMotionSicknessRightStick)
      g_vr_speed += HydraTLayer::vr_gc_rightstick_speed + HydraTLayer::vr_wm_rightstick_speed;
    if (g_ActiveConfig.bMotionSickness2D && vr_widest_3d_HFOV <= 0)
      g_vr_speed += 1.0f;
    if (g_ActiveConfig.bMotionSicknessIR)
      g_vr_speed += HydraTLayer::vr_ir_speed;
    if (g_ActiveConfig.bMotionSicknessFreelook)
      g_vr_speed += vr_freelook_speed;
    vr_freelook_speed = 0;
  }
  if (g_has_hmd && g_ActiveConfig.iMotionSicknessMethod == 2)
  {
    // black the screen if we are moving fast
    g_vr_black_screen = (g_vr_speed > 0.15f);
  }
#ifdef OVR_MAJOR_VERSION
  else if (g_has_rift && g_ActiveConfig.iMotionSicknessMethod == 1 && !g_has_openvr)
  {
    g_vr_black_screen = false;
    // reduce the FOV if we are moving fast
    if (g_vr_speed > 0.15f)
    {
      float t = tan(DEGREES_TO_RADIANS(g_ActiveConfig.fMotionSicknessFOV / 2));
      g_eye_fov[0].LeftTan = std::min(g_best_eye_fov[0].LeftTan, t);
      g_eye_fov[0].RightTan = std::min(g_best_eye_fov[0].RightTan, t);
      g_eye_fov[0].UpTan = std::min(g_best_eye_fov[0].UpTan, t);
      g_eye_fov[0].DownTan = std::min(g_best_eye_fov[0].DownTan, t);
      g_eye_fov[1].LeftTan = std::min(g_best_eye_fov[1].LeftTan, t);
      g_eye_fov[1].RightTan = std::min(g_best_eye_fov[1].RightTan, t);
      g_eye_fov[1].UpTan = std::min(g_best_eye_fov[1].UpTan, t);
      g_eye_fov[1].DownTan = std::min(g_best_eye_fov[1].DownTan, t);
    }
    else
    {
      memcpy(g_eye_fov, g_best_eye_fov, 2 * sizeof(g_eye_fov[0]));
    }
    g_fov_changed = memcmp(g_eye_fov, g_last_eye_fov, 2 * sizeof(g_eye_fov[0])) != 0;
    memcpy(g_last_eye_fov, g_eye_fov, 2 * sizeof(g_eye_fov[0]));
  }
#endif
  else
  {
    g_vr_black_screen = false;
  }
}

#ifdef HAVE_OPENVR
//-----------------------------------------------------------------------------
// Purpose: Helper to get a string from a tracked device property and turn it
//			into a std::string
//-----------------------------------------------------------------------------
std::string GetTrackedDeviceString(vr::IVRSystem* pHmd, vr::TrackedDeviceIndex_t unDevice,
                                   vr::TrackedDeviceProperty prop,
                                   vr::TrackedPropertyError* peError = nullptr)
{
  uint32_t unRequiredBufferLen =
      pHmd->GetStringTrackedDeviceProperty(unDevice, prop, nullptr, 0, peError);
  if (unRequiredBufferLen == 0)
    return "";

  char* pchBuffer = new char[unRequiredBufferLen];
  unRequiredBufferLen =
      pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
  std::string sResult = pchBuffer;
  delete[] pchBuffer;
  return sResult;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BInitCompositor()
{
  vr::EVRInitError peError = vr::VRInitError_None;

  m_pCompositor =
      (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &peError);

  if (peError != vr::VRInitError_None)
  {
    m_pCompositor = nullptr;

    NOTICE_LOG(VR, "Compositor initialization failed with error: %s: %s\n",
               vr::VR_GetVRInitErrorAsSymbol(peError),
               vr::VR_GetVRInitErrorAsEnglishDescription(peError));
    return false;
  }

  // change grid room colour
  // m_pCompositor->FadeToColor(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, true);
  m_pCompositor->SetTrackingSpace(vr::TrackingUniverseSeated);

  return true;
}
#endif

void SetTwoHmdFOV()
{
#if defined(HAVE_OPENVR) && defined(OVR_MAJOR_VERSION)
  float left, right, top, bottom;
  for (int eye = 0; eye < 2; ++eye)
  {
    m_pHMD->GetProjectionRaw((vr::EVREye)eye, &left, &right, &top, &bottom);
    g_eye_fov[eye].LeftTan = fabs(left);
    g_eye_fov[eye].RightTan = fabs(right);
    g_eye_fov[eye].UpTan = fabs(top);
    g_eye_fov[eye].DownTan = fabs(bottom);
  }
  memcpy(g_best_eye_fov, g_eye_fov, 2 * sizeof(g_eye_fov[0]));
  memcpy(g_last_eye_fov, g_eye_fov, 2 * sizeof(g_eye_fov[0]));
//NOTICE_LOG(VR, "********** %f to %f, %f to %f", left, right, top, bottom);
  //ERROR_LOG(VR, "********** %f° to %f°, %f° to %f°", RADIANS_TO_DEGREES(atan(left)), RADIANS_TO_DEGREES(atan(right)), RADIANS_TO_DEGREES(atan(top)), RADIANS_TO_DEGREES(atan(bottom)));
//#else
  //int eye = 0;
  //NOTICE_LOG(VR, "********** %f to %f, %f to %f", g_eye_fov[eye].LeftTan, g_eye_fov[eye].RightTan, g_eye_fov[eye].UpTan, g_eye_fov[eye].DownTan);
  //ERROR_LOG(VR, "********** %f° to %f°, %f° to %f°", RADIANS_TO_DEGREES(atan(g_eye_fov[eye].LeftTan)), RADIANS_TO_DEGREES(atan(g_eye_fov[eye].RightTan)), RADIANS_TO_DEGREES(atan(g_eye_fov[eye].UpTan)), RADIANS_TO_DEGREES(atan(g_eye_fov[eye].DownTan)));
//CV1: 0.964926 to 0.715264, 0.889498 to 1.110925
//CV1: 43.977375° to 35.574768°, 41.653034° to 48.008022°
#endif
}

bool InitOpenVR()
{
#ifdef HAVE_OPENVR
  g_has_two_hmds = false;

  // Loading the OpenVR Runtime
  vr::EVRInitError eError = vr::VRInitError_None;
  m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);

  if (eError != vr::VRInitError_None)
  {
    m_pHMD = nullptr;
    ERROR_LOG(VR, "Unable to init OpenVR: %s: %s", vr::VR_GetVRInitErrorAsSymbol(eError),
              vr::VR_GetVRInitErrorAsEnglishDescription(eError));
    g_has_openvr = false;
  }
  else
  {
    m_strDriver = GetTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd,
      vr::Prop_TrackingSystemName_String);
    g_openvr_is_vive = (m_strDriver == "lighthouse");
    g_openvr_is_rift = (m_strDriver == "oculus");
    NOTICE_LOG(VR, "OpenVR strDriver = '%s'", m_strDriver.c_str());
    m_strDisplay = GetTrackedDeviceString(m_pHMD, vr::k_unTrackedDeviceIndex_Hmd,
      vr::Prop_SerialNumber_String);
    NOTICE_LOG(VR, "OpenVR strDisplay = '%s'", m_strDisplay.c_str());

    if (g_has_rift && g_openvr_is_rift)
    {
      m_pHMD = nullptr;
      g_has_openvr = false;
    }
    else
    {
      m_pRenderModels =
        (vr::IVRRenderModels*)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
      if (!m_pRenderModels)
      {
        m_pHMD = nullptr;
        vr::VR_Shutdown();

        ERROR_LOG(VR, "Unable to get render model interface: %s: %s",
          vr::VR_GetVRInitErrorAsSymbol(eError),
          vr::VR_GetVRInitErrorAsEnglishDescription(eError));
        g_has_openvr = false;
      }
      else
      {
        NOTICE_LOG(VR, "VR_Init Succeeded");
        g_has_openvr = true;
        g_has_hmd = true;
        g_has_two_hmds = g_has_rift;
      }

      u32 m_nWindowWidth = 512;
      u32 m_nWindowHeight = 512;
      // m_pHMD->GetWindowBounds(&g_hmd_window_x, &g_hmd_window_y, &m_nWindowWidth, &m_nWindowHeight);
      g_hmd_window_width = m_nWindowWidth;
      g_hmd_window_height = m_nWindowHeight;
      // NOTICE_LOG(VR, "OpenVR WindowBounds (%d,%d) %dx%d", g_hmd_window_x, g_hmd_window_y,
      // g_hmd_window_width, g_hmd_window_height);

      vr::TrackedPropertyError error;
      g_hmd_refresh_rate =
        (int)(0.5f +
          m_pHMD->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
            vr::Prop_DisplayFrequency_Float, &error));
      g_openvr_ipd = m_pHMD->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
        vr::Prop_UserIpdMeters_Float, &error);

      if (m_bUseCompositor)
      {
        if (!BInitCompositor())
        {
          ERROR_LOG(VR, "%s - Failed to initialize OpenVR Compositor!\n", __FUNCTION__);
          g_has_openvr = false;
        }
      }
      if (g_has_openvr)
      {
        g_vr_needs_DXGIFactory1 = true;
        g_vr_cant_motion_blur = true;
        g_vr_has_dynamic_predict = false;
        g_vr_has_configure_rendering = false;
        g_vr_has_configure_tracking = false;
        g_vr_has_hq_distortion = false;
        g_vr_should_swap_buffers = true;  // todo: check if this is right
        g_vr_has_timewarp_tweak = false;
        g_vr_has_asynchronous_timewarp = false;
      }
      return g_has_openvr;
    }
  }
#endif
  return false;
}

bool InitOculusDebugVR()
{
#if defined(OVR_MAJOR_VERSION) && OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 6
  if (g_force_vr)
  {
    NOTICE_LOG(VR, "Forcing VR mode, simulating Oculus Rift DK2.");
#if OVR_MAJOR_VERSION >= 6
    if (ovrHmd_CreateDebug(ovrHmd_DK2, &hmd) != ovrSuccess)
      hmd = nullptr;
#else
    hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
#endif
    return (hmd != nullptr);
  }
#endif
  return false;
}

bool InitOculusHMD()
{
#ifdef OVR_MAJOR_VERSION
  if (hmd)
  {
    g_vr_dont_vsync = true;
#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 5 ||                                          \
    (OVR_MINOR_VERSION == 4 && OVR_BUILD_VERSION >= 2)
    g_vr_has_hq_distortion = true;
#else
    g_vr_has_hq_distortion = false;
#endif
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 5
    g_vr_should_swap_buffers = false;
    g_vr_needs_DXGIFactory1 = false;
#else
    g_vr_needs_DXGIFactory1 = true;
    g_vr_should_swap_buffers = true;
#endif

#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 5
    g_vr_has_timewarp_tweak = true;
#else
    g_vr_has_timewarp_tweak = false;
#endif

// Get more details about the HMD
#if OVR_PRODUCT_VERSION >= 1
    g_vr_cant_motion_blur = true;
    g_vr_has_dynamic_predict = false;
    g_vr_has_configure_rendering = false;
    hmdDesc = ovr_GetHmdDesc(hmd);
#elif OVR_MAJOR_VERSION >= 7
    g_vr_cant_motion_blur = true;
    g_vr_has_dynamic_predict = false;
    g_vr_has_configure_rendering = false;
    hmdDesc = ovr_GetHmdDesc(hmd);
    ovr_SetEnabledCaps(hmd, ovrHmd_GetEnabledCaps(hmd) | 0);
#else
    g_vr_cant_motion_blur = false;
    g_vr_has_dynamic_predict = true;
    g_vr_has_configure_rendering = true;
    // ovrHmd_GetDesc(hmd, &hmdDesc);
    hmdDesc = *hmd;
    ovrHmd_SetEnabledCaps(hmd, ovrHmd_GetEnabledCaps(hmd) | ovrHmdCap_DynamicPrediction |
                                   ovrHmdCap_LowPersistence);
#endif

#if OVR_PRODUCT_VERSION >= 1
    g_vr_can_disable_hsw = false;
    g_vr_has_asynchronous_timewarp = true;
    // no need to configure tracking
    g_vr_has_configure_tracking = false;
#elif OVR_MAJOR_VERSION >= 6
    g_vr_can_disable_hsw = false;
    g_vr_has_asynchronous_timewarp = false;
    g_vr_has_configure_tracking = true;
    if (OVR_SUCCESS(ovrHmd_ConfigureTracking(hmd,
                                             ovrTrackingCap_Orientation | ovrTrackingCap_Position |
                                                 ovrTrackingCap_MagYawCorrection,
                                             0)))
#else
    g_vr_can_disable_hsw = true;
    g_vr_has_asynchronous_timewarp = false;
    g_vr_has_configure_tracking = true;
    if (ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation | ovrTrackingCap_Position |
                                          ovrTrackingCap_MagYawCorrection,
                                 0))
#endif
    {
      g_has_rift = true;
      g_has_hmd = true;
      g_hmd_window_width = hmdDesc.Resolution.w;
      g_hmd_window_height = hmdDesc.Resolution.h;
      g_best_eye_fov[0] = hmdDesc.DefaultEyeFov[0];
      g_best_eye_fov[1] = hmdDesc.DefaultEyeFov[1];
      g_eye_fov[0] = g_best_eye_fov[0];
      g_eye_fov[1] = g_best_eye_fov[1];
      g_last_eye_fov[0] = g_eye_fov[0];
      g_last_eye_fov[1] = g_eye_fov[1];
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION < 6
      g_hmd_window_x = hmdDesc.WindowsPos.x;
      g_hmd_window_y = hmdDesc.WindowsPos.y;
      g_is_direct_mode = !(hmdDesc.HmdCaps & ovrHmdCap_ExtendDesktop);
      if (hmdDesc.Type < 6)
        g_hmd_refresh_rate = 60;
      else if (hmdDesc.Type > 6)
        g_hmd_refresh_rate = 90;
      else
        g_hmd_refresh_rate = 75;
#else
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION == 6
      g_hmd_refresh_rate = (int)(1.0f / ovrHmd_GetFloat(hmd, "VsyncToNextVsync", 0.f) + 0.5f);
#else
      g_hmd_refresh_rate = (int)(hmdDesc.DisplayRefreshRate + 0.5f);
#endif
      g_hmd_window_x = 0;
      g_hmd_window_y = 0;
      g_is_direct_mode = true;
#endif
#ifdef _WIN32
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION < 6
      g_hmd_device_name = hmdDesc.DisplayDeviceName;
#else
      g_hmd_device_name = nullptr;
#endif
      
      if (g_hmd_device_name)
      {
		const char* p = strstr(g_hmd_device_name, "\\Monitor");
        if (p)
        {
          size_t n = p - g_hmd_device_name;
          if (n >= MAX_PATH)
            n = MAX_PATH - 1;
          g_hmd_device_name = strncpy(hmd_device_name, g_hmd_device_name, n);
          hmd_device_name[n] = '\0';
        }
      }
#endif
      NOTICE_LOG(VR, "Oculus Rift head tracker started.");
    }
    return g_has_rift;
  }
#endif
  return false;
}

bool InitOculusVR()
{
#ifdef OVR_MAJOR_VERSION
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 7
  memset(&g_rift_frame_timing, 0, sizeof(g_rift_frame_timing));
#endif

#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 7
  ovr_Initialize(nullptr);
  if (ovr_Create(&hmd, &luid) != ovrSuccess)
    hmd = nullptr;
#ifdef _WIN32
  else
    g_hmd_luid = reinterpret_cast<LUID*>(&luid);
#endif

#else
#if OVR_MAJOR_VERSION >= 6
  ovr_Initialize(nullptr);
  if (ovrHmd_Create(0, &hmd) != ovrSuccess)
    hmd = nullptr;
#else
  ovr_Initialize();
  hmd = ovrHmd_Create(0);
#endif
#endif
  g_has_rift = (hmd != nullptr);
  g_has_hmd = g_has_hmd || g_has_rift;
  g_has_two_hmds = g_has_rift && g_has_openvr;
  if (!g_has_rift)
    WARN_LOG(VR, "Oculus Rift not detected. Oculus Rift support will not be available.");
  return (hmd != nullptr);
#else
  return false;
#endif
}

bool InitVR920VR()
{
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
    g_hmd_refresh_rate = 60;  // or 30, depending on how we implement it
    g_vr_must_motion_blur = true;
    g_vr_has_dynamic_predict = false;
    g_vr_has_configure_rendering = false;
    g_vr_has_configure_tracking = false;
    g_vr_has_hq_distortion = false;
    g_vr_should_swap_buffers = true;
    g_vr_has_timewarp_tweak = false;
    g_vr_has_asynchronous_timewarp =
        false;  // but it doesn't need it either, so maybe this should be true?
    return true;
  }
#endif
  return false;
}

void VR_Init()
{
  g_has_hmd = false;
  g_is_direct_mode = false;
  g_hmd_device_name = nullptr;
  g_has_openvr = false;
  g_has_rift = false;
#ifdef _WIN32
  g_hmd_luid = nullptr;
#endif

  if (g_prefer_openvr)
  {
    InitOpenVR();
    if (!(g_has_openvr && g_one_hmd))
    {
      if (g_openvr_is_rift || !InitOculusVR())
        InitVR920VR();
      if (!g_has_hmd)
        InitOculusDebugVR();
    }
    if (g_force_vr)
      g_has_hmd = true;
  }
  else
  {
    InitOculusVR();
    if (!(g_has_rift && g_one_hmd))
    {
      if (!InitOpenVR())
        InitVR920VR();
      if (!g_has_hmd)
        InitOculusDebugVR();
    }
    if (g_force_vr)
      g_has_hmd = true;
  }
  if (g_has_two_hmds)
    NOTICE_LOG(VR, "Two HMDs detected!");
  InitOculusHMD();
  if (g_has_openvr && g_has_rift)
    SetTwoHmdFOV();

  if (g_has_hmd && g_hmd_window_width > 0 && g_hmd_window_height > 0)
  {
    SConfig::GetInstance().strFullscreenResolution =
        StringFromFormat("%dx%d", g_hmd_window_width, g_hmd_window_height);
    SConfig::GetInstance().iRenderWindowXPos = g_hmd_window_x;
    SConfig::GetInstance().iRenderWindowYPos = g_hmd_window_y;
    SConfig::GetInstance().iRenderWindowWidth = g_hmd_window_width;
    SConfig::GetInstance().iRenderWindowHeight = g_hmd_window_height;
    SConfig::GetInstance().m_special_case = true;
  }
  else
  {
    SConfig::GetInstance().m_special_case = false;
  }
}

void VR_StopRendering()
{
#ifdef _WIN32
  if (g_has_vr920)
  {
    VR920_StopStereo3D();
  }
#endif
#ifdef OVR_MAJOR_VERSION
  // Shut down rendering and release resources (by passing NULL)
  if (g_has_rift)
  {
#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 6
    for (int i = 0; i < ovrEye_Count; ++i)
      g_eye_render_desc[i] = ovrHmd_GetRenderDesc(hmd, (ovrEyeType)i, g_eye_fov[i]);
#else
    ovrHmd_ConfigureRendering(hmd, nullptr, 0, g_eye_fov, g_eye_render_desc);
#endif
  }
#endif
}

void VR_Shutdown()
{
#ifdef HAVE_OPENVR
  if (g_has_openvr && m_pHMD)
  {
    g_has_openvr = false;
    m_pHMD = nullptr;
    // crashes if OpenGL
    vr::VR_Shutdown();
  }
#endif
#ifdef OVR_MAJOR_VERSION
  if (hmd)
  {
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION < 6
    // on my computer, on runtime 0.4.2, the Rift won't switch itself off without this:
    if (g_is_direct_mode)
      ovrHmd_SetEnabledCaps(hmd, ovrHmdCap_DisplayOff);
#endif
    ovrHmd_Destroy(hmd);
    g_has_rift = false;
    g_has_hmd = false;
    g_is_direct_mode = false;
    NOTICE_LOG(VR, "Oculus Rift shut down.");
  }
  ovr_Shutdown();
#endif
}

void VR_RecenterHMD()
{
#ifdef HAVE_OPENVR
  if (g_has_openvr && m_pHMD)
  {
    m_pHMD->ResetSeatedZeroPose();
  }
#endif
#ifdef OVR_MAJOR_VERSION
  if (g_has_rift)
  {
    ovrHmd_RecenterPose(hmd);
  }
#endif
}

void VR_CheckStatus(bool* ShouldRecenter, bool* ShouldQuit)
{
#if defined(OVR_MAJOR_VERSION) && (OVR_PRODUCT_VERSION >= 1)
  if (g_has_rift)
  {
    ovrSessionStatus sessionStatus;
    ovr_GetSessionStatus(hmd, &sessionStatus);
    *ShouldRecenter = sessionStatus.ShouldRecenter ? true : false;
    *ShouldQuit = sessionStatus.ShouldQuit ? true : false;
  }
  else
  {
    *ShouldRecenter = *ShouldQuit = false;
  }
#else
  *ShouldRecenter = *ShouldQuit = false;
#endif
}

void VR_ConfigureHMDTracking()
{
#if defined(OVR_MAJOR_VERSION) && OVR_PRODUCT_VERSION == 0
  if (g_has_rift)
  {
    int cap = 0;
    if (g_ActiveConfig.bOrientationTracking)
      cap |= ovrTrackingCap_Orientation;
    if (g_ActiveConfig.bMagYawCorrection)
      cap |= ovrTrackingCap_MagYawCorrection;
    if (g_ActiveConfig.bPositionTracking)
      cap |= ovrTrackingCap_Position;
    ovrHmd_ConfigureTracking(hmd, cap, 0);
  }
#endif
}

void VR_ConfigureHMDPrediction()
{
#if defined(OVR_MAJOR_VERSION) && OVR_PRODUCT_VERSION == 0
  if (g_has_rift)
  {
#if OVR_MAJOR_VERSION <= 5
    int caps =
        ovrHmd_GetEnabledCaps(hmd) &
        ~(ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence | ovrHmdCap_NoMirrorToWindow);
#else
#if OVR_MAJOR_VERSION >= 7
    int caps = ovrHmd_GetEnabledCaps(hmd) & ~(0);
#else
    int caps =
        ovrHmd_GetEnabledCaps(hmd) & ~(ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence);
#endif
#endif
#if OVR_MAJOR_VERSION <= 6
    if (g_Config.bLowPersistence)
      caps |= ovrHmdCap_LowPersistence;
    if (g_Config.bDynamicPrediction)
      caps |= ovrHmdCap_DynamicPrediction;
#if OVR_MAJOR_VERSION <= 5
    if (g_Config.iMirrorPlayer == VR_PLAYER_NONE || g_Config.iMirrorStyle == VR_MIRROR_DISABLED)
      caps |= ovrHmdCap_NoMirrorToWindow;
#endif
#endif
    ovrHmd_SetEnabledCaps(hmd, caps);
  }
#endif
}

void VR_GetEyePoses()
{
#ifdef OVR_MAJOR_VERSION
  if (g_has_rift)
  {
#ifdef OCULUSSDK042
    g_eye_poses[ovrEye_Left] = ovrHmd_GetEyePose(hmd, ovrEye_Left);
    g_eye_poses[ovrEye_Right] = ovrHmd_GetEyePose(hmd, ovrEye_Right);
#elif OVR_PRODUCT_VERSION >= 1
    ovrVector3f useHmdToEyeViewOffset[2] = {g_eye_render_desc[0].HmdToEyeOffset,
                                            g_eye_render_desc[1].HmdToEyeOffset};
#else
    ovrVector3f useHmdToEyeViewOffset[2] = {g_eye_render_desc[0].HmdToEyeViewOffset,
                                            g_eye_render_desc[1].HmdToEyeViewOffset};
#if OVR_MAJOR_VERSION >= 8
    ovr_GetEyePoses(hmd, g_ovr_frameindex, false, useHmdToEyeViewOffset, g_eye_poses, nullptr);
#elif OVR_MAJOR_VERSION >= 7
    ovr_GetEyePoses(hmd, g_ovr_frameindex, useHmdToEyeViewOffset, g_eye_poses, nullptr);
#else
    ovrHmd_GetEyePoses(hmd, g_ovr_frameindex, useHmdToEyeViewOffset, g_eye_poses, nullptr);
#endif
#endif
  }
#endif
#if HAVE_OPENVR
  if (g_has_openvr)
  {
    if (m_pCompositor)
    {
      // m_pCompositor->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr,
      // 0);
    }
  }
#endif
}

#ifdef HAVE_OPENVR
//-----------------------------------------------------------------------------
// Purpose: Processes a single VR event
//-----------------------------------------------------------------------------
void ProcessVREvent(const vr::VREvent_t& event)
{
  switch (event.eventType)
  {
  case vr::VREvent_TrackedDeviceActivated:
  {
    // SetupRenderModelForTrackedDevice(event.trackedDeviceIndex);
    NOTICE_LOG(VR, "Device %u attached. Setting up render model.\n", event.trackedDeviceIndex);
    break;
  }
  case vr::VREvent_TrackedDeviceDeactivated:
  {
    NOTICE_LOG(VR, "Device %u detached.\n", event.trackedDeviceIndex);
    break;
  }
  case vr::VREvent_TrackedDeviceUpdated:
  {
    NOTICE_LOG(VR, "Device %u updated.\n", event.trackedDeviceIndex);
    break;
  }
  case vr::VREvent_IpdChanged:
  {
    g_openvr_ipd = event.data.ipd.ipdMeters;
    NOTICE_LOG(VR, "IPD changed to %fm", g_openvr_ipd);
  }
  }
}
#endif

void ConvertOpenVRToOculusPose()
{
#if defined(OVR_MAJOR_VERSION) && defined(HAVE_OPENVR)
  OVR::Posef pose;
  ovrMatrix4f m;
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      m.M[r][c] = g_head_tracking_matrix.data[c * 4 + r];
  pose.Rotation = OVR::Quatf(m);
  pose.Translation = OVR::Vector3f(-g_head_tracking_position[0], -g_head_tracking_position[1], -g_head_tracking_position[2]);

  g_eye_poses[0] = pose;
  g_eye_poses[1] = pose;
#endif
}

#ifdef OVR_MAJOR_VERSION
void UpdateOculusHeadTracking()
{
// we can only call GetEyePose between BeginFrame and EndFrame
#ifdef OCULUSSDK042
  g_vr_lock.lock();
  g_eye_poses[ovrEye_Left] = ovrHmd_GetEyePose(hmd, ovrEye_Left);
  g_eye_poses[ovrEye_Right] = ovrHmd_GetEyePose(hmd, ovrEye_Right);
  OVR::Posef pose = g_eye_poses[ovrEye_Left];
  g_vr_lock.unlock();
#else
#if OVR_PRODUCT_VERSION >= 1
  ovrVector3f useHmdToEyeViewOffset[2] = {g_eye_render_desc[0].HmdToEyeOffset,
                                          g_eye_render_desc[1].HmdToEyeOffset};
#else
  ovrVector3f useHmdToEyeViewOffset[2] = {g_eye_render_desc[0].HmdToEyeViewOffset,
                                          g_eye_render_desc[1].HmdToEyeViewOffset};
#endif
#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 8
  double display_time = ovr_GetPredictedDisplayTime(hmd, g_ovr_frameindex);
  ovrTrackingState state = ovr_GetTrackingState(hmd, display_time, false);
  ovr_CalcEyePoses(state.HeadPose.ThePose, useHmdToEyeViewOffset, g_eye_poses);
  OVR::Posef pose = state.HeadPose.ThePose;
#elif OVR_MAJOR_VERSION >= 7
  ovr_GetEyePoses(hmd, g_ovr_frameindex, useHmdToEyeViewOffset, g_eye_poses, nullptr);
  OVR::Posef pose = g_eye_poses[ovrEye_Left];
#else
  ovrHmd_GetEyePoses(hmd, g_ovr_frameindex, useHmdToEyeViewOffset, g_eye_poses, nullptr);
  OVR::Posef pose = g_eye_poses[ovrEye_Left];
#endif
#endif
  // ovrTrackingState ss = ovrHmd_GetTrackingState(hmd, g_rift_frame_timing.ScanoutMidpointSeconds);
  // if (ss.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked))
  {
    // OVR::Posef pose = ss.HeadPose.ThePose;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    pose.Rotation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);

    float x = 0, y = 0, z = 0;
    roll = -RADIANS_TO_DEGREES(roll);    // ???
    pitch = -RADIANS_TO_DEGREES(pitch);  // should be degrees down
    yaw = -RADIANS_TO_DEGREES(yaw);      // should be degrees right
    x = pose.Translation.x;
    y = pose.Translation.y;
    z = pose.Translation.z;
    g_head_tracking_position[0] = -x;
    g_head_tracking_position[1] = -y;
#if OVR_PRODUCT_VERSION == 0 && OVR_MAJOR_VERSION <= 4
    g_head_tracking_position[2] = 0.06f - z;
#else
    g_head_tracking_position[2] = -z;
#endif
    Matrix33 m, yp, ya, p, r;
    Matrix33::RotateY(ya, DEGREES_TO_RADIANS(yaw));
    Matrix33::RotateX(p, DEGREES_TO_RADIANS(pitch));
    Matrix33::Multiply(p, ya, yp);
    Matrix33::RotateZ(r, DEGREES_TO_RADIANS(roll));
    Matrix33::Multiply(r, yp, m);
    Matrix44::LoadMatrix33(g_head_tracking_matrix, m);
  }
}
#endif

#ifdef HAVE_OPENVR
void UpdateOpenVRHeadTracking()
{
  // Process OpenVR events
  vr::VREvent_t event;
  while (m_pHMD->PollNextEvent(&event, sizeof(event)))
  {
    ProcessVREvent(event);
  }

  // float fSecondsUntilPhotons = 0.0f;
  // m_pHMD->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, fSecondsUntilPhotons,
  // m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount);
  m_iValidPoseCount = 0;
  // for ( int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice )
  //{
  //	if ( m_rTrackedDevicePose[nDevice].bPoseIsValid )
  //	{
  //		m_iValidPoseCount++;
  //		//m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking;
  //	}
  //}

  if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
  {
    float x =
        m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m[0][3];
    float y =
        m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m[1][3];
    float z =
        m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m[2][3];
    g_head_tracking_position[0] = -x;
    g_head_tracking_position[1] = -y;
    g_head_tracking_position[2] = -z;
    Matrix33 m;
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        m.data[r * 3 + c] =
            m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m[c][r];
    Matrix44::LoadMatrix33(g_head_tracking_matrix, m);
  }
}
#endif

#ifdef _WIN32
void UpdateVuzixHeadTracking()
{
  LONG ya = 0, p = 0, r = 0;
  if (Vuzix_GetTracking(&ya, &p, &r) == ERROR_SUCCESS)
  {
    float yaw = -ya * 180.0f / 32767.0f;
    float pitch = p * -180.0f / 32767.0f;
    float roll = r * 180.0f / 32767.0f;
    // todo: use head and neck model
    float x = 0;
    float y = 0;
    float z = 0;
    Matrix33 m, yp, yawm, pitchm, rollm;
    Matrix33::RotateY(yawm, DEGREES_TO_RADIANS(yaw));
    Matrix33::RotateX(pitchm, DEGREES_TO_RADIANS(pitch));
    Matrix33::Multiply(pitchm, yawm, yp);
    Matrix33::RotateZ(rollm, DEGREES_TO_RADIANS(roll));
    Matrix33::Multiply(rollm, yp, m);
    Matrix44::LoadMatrix33(g_head_tracking_matrix, m);
    g_head_tracking_position[0] = -x;
    g_head_tracking_position[1] = -y;
    g_head_tracking_position[2] = -z;
  }
}
#endif

void VR_UpdateHeadTrackingIfNeeded()
{
  if (g_new_tracking_frame)
  {
    g_new_tracking_frame = false;
#ifdef _WIN32
    if (g_has_vr920 && Vuzix_GetTracking)
      UpdateVuzixHeadTracking();
#endif
#ifdef HAVE_OPENVR
    if (g_has_openvr)
      UpdateOpenVRHeadTracking();
#endif
#ifdef OVR_MAJOR_VERSION
    if (g_has_rift && g_has_openvr)
      ConvertOpenVRToOculusPose();
    else if (g_has_rift)
      UpdateOculusHeadTracking();
#endif
  }
}

void VR_GetProjectionHalfTan(float& hmd_halftan)
{
#ifdef OVR_MAJOR_VERSION
  if (g_has_rift)
  {
    hmd_halftan = fabs(g_eye_fov[0].LeftTan);
    if (fabs(g_eye_fov[0].RightTan) > hmd_halftan)
      hmd_halftan = fabs(g_eye_fov[0].RightTan);
    if (fabs(g_eye_fov[0].UpTan) > hmd_halftan)
      hmd_halftan = fabs(g_eye_fov[0].UpTan);
    if (fabs(g_eye_fov[0].DownTan) > hmd_halftan)
      hmd_halftan = fabs(g_eye_fov[0].DownTan);
  }
  else
#endif
      if (g_has_openvr)
  {
    // rough approximation, can't be bothered to work this out properly
    hmd_halftan = tan(DEGREES_TO_RADIANS(100.0f / 2));
  }
  else
  {
    hmd_halftan = tan(DEGREES_TO_RADIANS(32.0f / 2)) * 3.0f / 4.0f;
  }
}

void VR_GetProjectionMatrices(Matrix44& left_eye, Matrix44& right_eye, float znear, float zfar)
{
#ifdef OVR_MAJOR_VERSION
  if (g_has_rift && !g_has_two_hmds)
  {
#if OVR_PRODUCT_VERSION >= 1
    ovrMatrix4f rift_left = ovrMatrix4f_Projection(g_eye_fov[0], znear, zfar, 0);
    ovrMatrix4f rift_right = ovrMatrix4f_Projection(g_eye_fov[1], znear, zfar, 0);
#else
    ovrMatrix4f rift_left = ovrMatrix4f_Projection(g_eye_fov[0], znear, zfar, true);
    ovrMatrix4f rift_right = ovrMatrix4f_Projection(g_eye_fov[1], znear, zfar, true);
#endif
    Matrix44::Set(left_eye, rift_left.M[0]);
    Matrix44::Set(right_eye, rift_right.M[0]);
  }
  else
#endif
#ifdef HAVE_OPENVR
      if (g_has_openvr)
  {
#ifdef OPENVR_105_OR_ABOVE
    vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(
        vr::Eye_Left, znear,
        zfar);  // if error here, use OpenVR 1.05 or above instead of 1.03/1.04 OR use line below
#else
    vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(vr::Eye_Left, znear, zfar, OPENVR_DirectX);
#endif
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        left_eye.data[r * 4 + c] = mat.m[r][c];
#ifdef OPENVR_105_OR_ABOVE
    mat = m_pHMD->GetProjectionMatrix(
        vr::Eye_Right, znear,
        zfar);  // if error here, use OpenVR 1.05 or above instead of 1.03/1.04 OR use line below
#else
    mat = m_pHMD->GetProjectionMatrix(vr::Eye_Right, znear, zfar, OPENVR_DirectX);
#endif
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        right_eye.data[r * 4 + c] = mat.m[r][c];
  }
  else
#endif
  {
    Matrix44::LoadIdentity(left_eye);
    left_eye.data[10] = -znear / (zfar - znear);
    left_eye.data[11] = -zfar * znear / (zfar - znear);
    left_eye.data[14] = -1.0f;
    left_eye.data[15] = 0.0f;
    // 32 degrees HFOV, 4:3 aspect ratio
    left_eye.data[0 * 4 + 0] = 1.0f / tan(32.0f / 2.0f * 3.1415926535f / 180.0f);
    left_eye.data[1 * 4 + 1] = 4.0f / 3.0f * left_eye.data[0 * 4 + 0];
    Matrix44::Set(right_eye, left_eye.data);
  }
}

void VR_GetEyePos(float* posLeft, float* posRight)
{
#ifdef OVR_MAJOR_VERSION
  if (g_has_rift)
  {
#ifdef OCULUSSDK042
    posLeft[0] = g_eye_render_desc[0].ViewAdjust.x;
    posLeft[1] = g_eye_render_desc[0].ViewAdjust.y;
    posLeft[2] = g_eye_render_desc[0].ViewAdjust.z;
    posRight[0] = g_eye_render_desc[1].ViewAdjust.x;
    posRight[1] = g_eye_render_desc[1].ViewAdjust.y;
    posRight[2] = g_eye_render_desc[1].ViewAdjust.z;
#elif OVR_PRODUCT_VERSION >= 1
    posLeft[0] = g_eye_render_desc[0].HmdToEyeOffset.x;
    posLeft[1] = g_eye_render_desc[0].HmdToEyeOffset.y;
    posLeft[2] = g_eye_render_desc[0].HmdToEyeOffset.z;
    posRight[0] = g_eye_render_desc[1].HmdToEyeOffset.x;
    posRight[1] = g_eye_render_desc[1].HmdToEyeOffset.y;
    posRight[2] = g_eye_render_desc[1].HmdToEyeOffset.z;
#else
    posLeft[0] = g_eye_render_desc[0].HmdToEyeViewOffset.x;
    posLeft[1] = g_eye_render_desc[0].HmdToEyeViewOffset.y;
    posLeft[2] = g_eye_render_desc[0].HmdToEyeViewOffset.z;
    posRight[0] = g_eye_render_desc[1].HmdToEyeViewOffset.x;
    posRight[1] = g_eye_render_desc[1].HmdToEyeViewOffset.y;
    posRight[2] = g_eye_render_desc[1].HmdToEyeViewOffset.z;
#endif
#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 6
    for (int i = 0; i < 3; ++i)
    {
      posLeft[i] = -posLeft[i];
      posRight[i] = -posRight[i];
    }
#endif
  }
  else
#endif
#ifdef HAVE_OPENVR
      if (g_has_openvr)
  {
    posLeft[0] = g_openvr_ipd / 2;
    posRight[0] = -posLeft[0];
    posLeft[1] = posRight[1] = 0;
    posLeft[2] = posRight[2] = 0;
  }
  else
#endif
  {
    // assume 64mm IPD
    posLeft[0] = 0.032f;
    posRight[0] = -0.032f;
    posLeft[1] = posRight[1] = 0;
    posLeft[2] = posRight[2] = 0;
  }
}

void VR_GetFovTextureSize(int* width, int* height)
{
#if defined(OVR_MAJOR_VERSION)
  if (g_has_rift)
  {
    ovrSizei size = ovrHmd_GetFovTextureSize(hmd, ovrEye_Left, g_eye_fov[0], 1.0f);
    *width = size.w;
    *height = size.h;
  }
#endif
}

std::wstring VR_GetAudioDeviceId()
{
#if defined(OVR_PRODUCT_VERSION) && OVR_PRODUCT_VERSION >= 1
  if (g_has_rift)
  {
    // this is a standard GUID, but the windows GUID include file system is insane, so just define
    // it here
    const GUID DEVINTERFACE_AUDIO_RENDER_ = {
        0xe6327cad, 0xdcec, 0x4949, {0xae, 0x8a, 0x99, 0x1e, 0x97, 0x6a, 0x79, 0xd2}};
    GUID rift_guid;
    ovr_GetAudioDeviceOutGuid(&rift_guid);
    wchar_t guid_wstr[40];
    StringFromGUID2(rift_guid, guid_wstr, 40);
    NOTICE_LOG(VR, "Rift Audio GUID: '%S'", guid_wstr);
    HDEVINFO const device_info = SetupDiGetClassDevs(&DEVINTERFACE_AUDIO_RENDER_, 0, 0,
                                                     DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

    SP_DEVICE_INTERFACE_DATA device_data = {};
    device_data.cbSize = sizeof(device_data);
    PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = nullptr;

    for (int index = 0; SetupDiEnumDeviceInterfaces(
             device_info, nullptr, &DEVINTERFACE_AUDIO_RENDER_, index, &device_data);
         ++index)
    {
      // Get the size of the data block required
      DWORD len;
      SetupDiGetDeviceInterfaceDetail(device_info, &device_data, nullptr, 0, &len, nullptr);
      detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(len);
      detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

      SP_DEVINFO_DATA device_info_data = {};
      device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

      // Query the data for this device
      if (SetupDiGetDeviceInterfaceDetail(device_info, &device_data, detail_data, len, nullptr,
                                          &device_info_data))
      {
        std::wstring device_path(detail_data->DevicePath);
        for_each(device_path.begin(), device_path.end(), [](wchar_t& in) { in = ::toupper(in); });
        if (device_path.find(guid_wstr) != std::string::npos)
        {
          NOTICE_LOG(VR, "Found Rift audio device %d: DevicePath = '%S', ", index,
                     detail_data->DevicePath);
          auto result = std::wstring(detail_data->DevicePath);
          free(detail_data);
          SetupDiDestroyDeviceInfoList(device_info);
          return result;
        }
        else
        {
          // NOTICE_LOG(VR, "%d: DevicePath = '%S', ", index, detail_data->DevicePath);
        }
      }
      free(detail_data);
    }
    SetupDiDestroyDeviceInfoList(device_info);
    // couldn't find the audio device Oculus specified
    ERROR_LOG(VR, "Couldn't find Rift audio device.");
    return std::wstring();
  }
  else
#endif
#ifdef HAVE_OPENVR
      if (g_has_openvr)
  {
    return std::wstring();
  }
  else
#endif
  {
    return std::wstring();
  }
}

bool VR_GetRemoteButtons(u32* buttons)
{
  *buttons = 0;
#if defined(OVR_MAJOR_VERSION) && OVR_PRODUCT_VERSION >= 1
  if (g_has_rift)
  {
    ovrInputState sidInput = {};
    bool HasInputState = OVR_SUCCESS(ovr_GetInputState(hmd, ovrControllerType_Remote, &sidInput));
    *buttons = sidInput.Buttons;
    return HasInputState;
  }
  else
#endif
  {
    return false;
  }
}

bool VR_GetTouchButtons(u32* buttons, u32* touches, float m_triggers[], float m_axes[])
{
  *buttons = 0;
  *touches = 0;
#if defined(OVR_MAJOR_VERSION) && (OVR_MAJOR_VERSION > 7 || OVR_PRODUCT_VERSION >= 1)
  if (g_has_rift)
  {
    ovrInputState touchInput = {};
    bool HasInputState = OVR_SUCCESS(ovr_GetInputState(hmd, ovrControllerType_Touch, &touchInput));
    *buttons = touchInput.Buttons;
    *touches = touchInput.Touches;
    memcpy(m_triggers, touchInput.IndexTrigger, 2 * sizeof(float));
    memcpy(&m_triggers[2], touchInput.HandTrigger, 2 * sizeof(float));
    memcpy(m_axes, touchInput.Thumbstick, 4 * sizeof(float));
    return HasInputState;
  }
  else
#endif
  {
    return false;
  }
}

bool VR_SetTouchVibration(int hands, float freq, float amplitude)
{
#if defined(OVR_MAJOR_VERSION) && (OVR_MAJOR_VERSION > 7 || OVR_PRODUCT_VERSION >= 1)
  if (g_has_rift)
  {
    if (amplitude <= 0)
      amplitude = 0;
    else
      amplitude = 1;
    return OVR_SUCCESS(ovr_SetControllerVibration(hmd, (ovrControllerType)hands, freq, amplitude));
  }
  else
#endif
  {
    return false;
  }
}

// 0 = unknown, 1 = buttons, -1 = analog
static int s_vive_button_mode[2] = {};
static bool s_vive_was_touched[2] = {};
static float s_vive_initial_touch_x[2] = {};
static float s_vive_initial_touch_y[2] = {};

void ProcessViveTouchpad(int hand, bool touched, bool pressed, float x, float y, u32* specials,
                         float analogs[])
{
  *specials = 0;
  analogs[0] = 0;
  analogs[1] = 0;
  if (!touched && !pressed)
  {
    s_vive_button_mode[hand] = 0;
  }
  else
  {
    if (pressed)
    {
      s_vive_button_mode[hand] = 1;
      // dpad or classic controller diamond button layout
      if (x < -1.0f / 3.0f)
        *specials |= VIVE_SPECIAL_DPAD_LEFT;
      else if (x > 1.0f / 3.0f)
        *specials |= VIVE_SPECIAL_DPAD_RIGHT;
      else if (-1.0f / 3.0f < y && y < 1.0f / 3.0f)
        *specials |= VIVE_SPECIAL_DPAD_MIDDLE;
      if (y < -1.0f / 3.0f)
        *specials |= VIVE_SPECIAL_DPAD_DOWN;
      else if (y > 1.0f / 3.0f)
        *specials |= VIVE_SPECIAL_DPAD_UP;
      // GameCube style buttons
      float angle = RADIANS_TO_DEGREES(atan2f(y, x));
      float dd = x * x + y * y;
#define A_RADIUS 0.372f
#define INNER_XY_RADIUS 0.498f
#define OUTER_XY_RADIUS 0.856f
#define INNER_B_RADIUS 0.544f
#define B_MIN_ANGLE -170.0f
#define B_MAX_ANGLE -134.0f
#define EMPTY_MIN_ANGLE -100.0f
#define EMPTY_MAX_ANGLE -52.0f
#define X_MIN_ANGLE -18.0f
#define X_MAX_ANGLE 39.0f
#define Y_MIN_ANGLE 76.0f
#define Y_MAX_ANGLE 137.0f
      // pressing just the A button
      if (dd < A_RADIUS * A_RADIUS)
      {
        *specials |= VIVE_SPECIAL_GC_A;
      }
      else
      {
        // pressing the B button
        if (angle > B_MIN_ANGLE && angle < B_MAX_ANGLE)
        {
          *specials |= VIVE_SPECIAL_GC_B;
          // pressing in between A and B counts as both
          if (dd < INNER_B_RADIUS * INNER_B_RADIUS)
            *specials |= VIVE_SPECIAL_GC_A;
        }
        else
        {
          // pressing the X button (may also be pressing Y)
          if (angle >= X_MIN_ANGLE && angle < Y_MIN_ANGLE)
          {
            *specials |= VIVE_SPECIAL_GC_X;
            // pressing in between A and X counts as both
            if (dd < INNER_XY_RADIUS * INNER_XY_RADIUS)
              *specials |= VIVE_SPECIAL_GC_A;
          }
          // pressing the Y button (may also be pressing X)
          if (angle > X_MAX_ANGLE && angle <= Y_MAX_ANGLE)
          {
            *specials |= VIVE_SPECIAL_GC_Y;
            // pressing in between A and Y counts as both
            if (dd < INNER_XY_RADIUS * INNER_XY_RADIUS)
              *specials |= VIVE_SPECIAL_GC_A;
          }
          // pressing the empty space below B and X
          else if (angle >= EMPTY_MIN_ANGLE && angle <= EMPTY_MAX_ANGLE &&
                   dd > INNER_B_RADIUS * INNER_B_RADIUS)
          {
            *specials |= VIVE_SPECIAL_GC_EMPTY;
          }
        }
      }
      // quadrant buttons, for NES, TurboGraphx, sideways wiimote, etc.
      if (y > -0.15)
      {
        if (x < 0.15)
          *specials |= VIVE_SPECIAL_TOPLEFT;
        if (x > -0.15)
          *specials |= VIVE_SPECIAL_TOPRIGHT;
      }
      if (y < 0.15)
      {
        if (x < 0.15)
          *specials |= VIVE_SPECIAL_BOTTOMLEFT;
        if (x > -0.15)
          *specials |= VIVE_SPECIAL_BOTTOMRIGHT;
      }

      // 6 buttons diagonally, for N64, sega, arcade
      angle = DEGREES_TO_RADIANS(40);
      float xd = x * cos(angle) + y * sin(angle);
      float yd = y * cos(angle) - x * sin(angle);
      // top row
      if (yd >= -0.1)
      {
        if (xd < -1 / 3.0f + 0.04f)
          *specials |= VIVE_SPECIAL_SIX_X;
        if (xd > -1 / 3.0f - 0.04f && xd < 1 / 3.0f + 0.04f)
          *specials |= VIVE_SPECIAL_SIX_Y;
        if (xd > 1 / 3.0f - 0.04f)
          *specials |= VIVE_SPECIAL_SIX_Z;
      }
      // bottom row
      if (yd <= 0.1)
      {
        if (xd < -1 / 3.0f + 0.04f)
          *specials |= VIVE_SPECIAL_SIX_A;
        if (xd > -1 / 3.0f - 0.04f && xd < 1 / 3.0f + 0.04f)
          *specials |= VIVE_SPECIAL_SIX_B;
        if (xd > 1 / 3.0f - 0.04f)
          *specials |= VIVE_SPECIAL_SIX_C;
      }
      // todo: various wiimote face button layouts
    }
    if (!s_vive_was_touched[hand])
    {
      // touching for first time
      s_vive_initial_touch_x[hand] = x;
      s_vive_initial_touch_y[hand] = y;
    }
    else if (!pressed && s_vive_button_mode[hand] == 0)
    {
      const float min_dist = 0.4f;
      float dx = x - s_vive_initial_touch_x[hand];
      float dy = y - s_vive_initial_touch_y[hand];
      float dist_squared = dx * dx + dy * dy;
      if (dist_squared > min_dist * min_dist)
        s_vive_button_mode[hand] = -1;
    }
    if (s_vive_button_mode[hand] < 0)
    {
      analogs[0] = x;
      analogs[1] = y;
    }
  }
  s_vive_was_touched[hand] = touched;
}

double last_good_tracking_time = 0;
bool VR_GetViveButtons(u32* buttons, u32* touches, u64* specials, float triggers[], float axes[])
{
  *buttons = 0;
  *touches = 0;
#if defined(HAVE_OPENVR)
  bool result = false;
  if (m_pHMD)
  {
    // find the controllers for each hand, 100 = not found
    vr::TrackedDeviceIndex_t left_hand = 100, right_hand = 100;
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedControllerRole hand = m_pHMD->GetControllerRoleForTrackedDeviceIndex(i);
      if (hand == vr::TrackedControllerRole_LeftHand)
        left_hand = i;
      else if (hand == vr::TrackedControllerRole_RightHand)
        right_hand = i;
    }
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedDeviceClass kind = m_pHMD->GetTrackedDeviceClass(i);
      if (kind == vr::TrackedDeviceClass_Controller)
      {
        if (left_hand == 100 && i != right_hand)
          left_hand = i;
        else if (right_hand == 100 && i != left_hand)
          right_hand = i;
      }
    }
    if (g_ActiveConfig.bAutoPairViveControllers &&
        (left_hand == 100 || right_hand == 100 || !m_rTrackedDevicePose[left_hand].bPoseIsValid ||
         !m_rTrackedDevicePose[right_hand].bPoseIsValid))
    {
      // one of the controllers lost tracking
      double t = Common::Timer::GetTimeMs() / 1000.0;
      // if we haven't had tracking for a whole second, try repairing
      if (t - last_good_tracking_time > 1.0)
      {
        VR_PairViveControllers();
        // don't try again for 20 seconds
        last_good_tracking_time = t + 20;
      }
    }
    else
    {
      last_good_tracking_time = Common::Timer::GetTimeMs() / 1000.0;
    }
    // get the state of each hand
    vr::VRControllerState_t states[2];
    ZeroMemory(&states, 2 * sizeof(*states));
#ifdef OPENVR_104_OR_ABOVE
    if (m_pHMD->GetControllerState(left_hand, &states[0], sizeof(*states)))  // if error here, use
                                                                             // OpenVR 1.05 or above
                                                                             // instead of 1.03 OR
                                                                             // use line below
#else
    if (m_pHMD->GetControllerState(left_hand, &states[0]))
#endif
      result = true;
#ifdef OPENVR_104_OR_ABOVE
    if (m_pHMD->GetControllerState(right_hand, &states[1], sizeof(*states)))  // if error here, use
                                                                              // OpenVR 1.05 or
                                                                              // above instead of
                                                                              // 1.03 OR use line
                                                                              // below
#else
    if (m_pHMD->GetControllerState(right_hand, &states[1]))
#endif
      result = true;
    // save the results in our own format
    *buttons =
        (states[0].ulButtonPressed & 0xFF) | ((states[0].ulButtonPressed >> 24) & 0xFF00) |
        (((states[1].ulButtonPressed & 0xFF) | ((states[1].ulButtonPressed >> 24) & 0xFF00)) << 16);
    *touches =
        (states[0].ulButtonTouched & 0xFF) | ((states[0].ulButtonTouched >> 24) & 0xFF00) |
        (((states[1].ulButtonTouched & 0xFF) | ((states[1].ulButtonTouched >> 24) & 0xFF00)) << 16);
    axes[4] = states[0].rAxis[0].x;
    axes[5] = states[0].rAxis[0].y;
    axes[6] = states[1].rAxis[0].x;
    axes[7] = states[1].rAxis[0].y;
    triggers[0] = states[0].rAxis[1].x;
    triggers[1] = states[1].rAxis[1].x;
    // our own special processing
    for (int hand = 0; hand < 2; ++hand)
      ProcessViveTouchpad(
          hand, (states[hand].ulButtonTouched & ((u64)1 << vr::k_EButton_SteamVR_Touchpad)) != 0,
          (states[hand].ulButtonPressed & ((u64)1 << vr::k_EButton_SteamVR_Touchpad)) != 0,
          states[hand].rAxis[0].x, states[hand].rAxis[0].y, ((u32*)specials) + hand,
          &axes[hand * 2]);
    return result;
  }
  else
#endif
  {
    return false;
  }
}

bool VR_ViveHapticPulse(int hands, int microseconds)
{
#if defined(HAVE_OPENVR)
  if (g_has_openvr && hands)
  {
    // find the controllers for each hand, 100 = not found
    vr::TrackedDeviceIndex_t left_hand = 100, right_hand = 100;
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedControllerRole hand = m_pHMD->GetControllerRoleForTrackedDeviceIndex(i);
      if (hand == vr::TrackedControllerRole_LeftHand)
        left_hand = i;
      else if (hand == vr::TrackedControllerRole_RightHand)
        right_hand = i;
    }
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedDeviceClass kind = m_pHMD->GetTrackedDeviceClass(i);
      if (kind == vr::TrackedDeviceClass_Controller)
      {
        if (left_hand == 100 && i != right_hand)
          left_hand = i;
        else if (right_hand == 100 && i != left_hand)
          right_hand = i;
      }
    }
    if (hands & 1)
      m_pHMD->TriggerHapticPulse(left_hand, 0, microseconds);
    if (hands & 2)
      m_pHMD->TriggerHapticPulse(right_hand, 0, microseconds);
  }
#endif
  return false;
}

float left_hand_old_velocity[3] = {}, left_hand_older_velocity[3] = {};
float right_hand_old_velocity[3] = {}, right_hand_older_velocity[3] = {};

bool VR_GetAccel(int index, bool sideways, bool has_extension, float* gx, float* gy, float* gz)
{
#if defined(HAVE_OPENVR)
  if (g_has_openvr)
  {
    // find the controllers for each hand, 100 = not found
    vr::TrackedDeviceIndex_t left_hand = 100, right_hand = 100;
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedControllerRole hand = m_pHMD->GetControllerRoleForTrackedDeviceIndex(i);
      if (hand == vr::TrackedControllerRole_LeftHand)
        left_hand = i;
      else if (hand == vr::TrackedControllerRole_RightHand)
        right_hand = i;
    }
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedDeviceClass kind = m_pHMD->GetTrackedDeviceClass(i);
      if (kind == vr::TrackedDeviceClass_Controller)
      {
        if (left_hand == 100 && i != right_hand)
          left_hand = i;
        else if (right_hand == 100 && i != left_hand)
          right_hand = i;
      }
    }
    if (right_hand >= vr::k_unMaxTrackedDeviceCount ||
        !m_rTrackedDevicePose[right_hand].bPoseIsValid)
    {
      // NOTICE_LOG(VR, "invalid!");
      return false;
    }
    float rx = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[0][3];
    float ry = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[1][3];
    float rz = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[2][3];
    Matrix33 m;
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        m.data[r * 3 + c] = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[c][r];
    float acc[3] = {};
    float dt = (float)(g_last_tracking_time - g_older_tracking_time);
    if (dt < 0.001f)
    {
      // NOTICE_LOG(VR, "too fast!");
      // return false;
    }
    for (int axis = 0; axis < 3; ++axis)
    {
      acc[axis] =
          (m_rTrackedDevicePose[right_hand].vVelocity.v[axis] - right_hand_older_velocity[axis]) /
          dt;
      right_hand_older_velocity[axis] = right_hand_old_velocity[axis];
      right_hand_old_velocity[axis] = m_rTrackedDevicePose[right_hand].vVelocity.v[axis];
    }
    // World-space accelerations need to be converted into accelerations relative to the Wiimote's
    // sensor.
    float rel_acc[3];
    for (int i = 0; i < 3; ++i)
    {
      rel_acc[i] =
          acc[0] * m.data[i * 3 + 0] + acc[1] * m.data[i * 3 + 1] + acc[2] * m.data[i * 3 + 2];
      // todo: check if this is correct!
    }

    // Note that here gX means to the CONTROLLER'S left, gY means to the CONTROLLER'S tail, and gZ
    // means to the CONTROLLER'S top!
    // Tilt sensing.
    // If the left Vive controller is off, or an extension is plugged in then just
    // hold the right Vive sideways yourself. Otherwise in sideways mode
    // with no extension pitch is controlled by the angle between the Vive controllers.
    if (sideways && !has_extension && left_hand != 100)
    {
      // Left vive controller's left side = front of wiimote
      // Right vive controller's right side = back of wiimote
      // Right vive controller's front = right side of wiimote
      // Right vive controller's face = top side of wiimote

      // angle between the controllers
      float lx = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[0][3];
      float ly = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[1][3];
      float lz = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[2][3];
      float x = rx - lx;
      float y = ry - ly;
      float z = rz - lz;
      float dist = sqrtf(x * x + y * y + z * z);
      if (dist > 0)
      {
        x = x / dist;
        y = y / dist;
        z = z / dist;
      }
      else
      {
        x = 1;
        y = 0;
        z = 0;
      }
      *gy = y;
      float tail_up = m.data[2 * 3 + 1];
      float touchpad_up = m.data[1 * 3 + 1];
      float len = sqrtf(tail_up * tail_up + touchpad_up * touchpad_up);
      if (len == 0)
      {
        // neither the tail or the touchpad is up, the side is up
        *gx = 0;
        *gz = 0;
      }
      else
      {
        float horiz_dist = sqrtf(x * x + z * z);
        *gx = horiz_dist * tail_up / len;
        *gz = horiz_dist * touchpad_up / len;
      }

      // Convert rel acc from m/s/s to G's, and to sideways Wiimote's coordinate system.
      *gx += rel_acc[2] / 9.8f;
      *gz += rel_acc[1] / 9.8f;
      *gy -= rel_acc[0] / 9.8f;
    }
    else
    {
      // Tilt sensing.
      *gx = -m.data[0 * 3 + 1];
      *gz = m.data[1 * 3 + 1];
      *gy = m.data[2 * 3 + 1];
      // NOTICE_LOG(VR, "gx=%f, gy=%f, gz=%f", *gx, *gy, *gz);

      // Convert rel acc from m/s/s to G's, and to Wiimote's coordinate system.
      *gx -= rel_acc[0] / 9.8f;
      *gz += rel_acc[1] / 9.8f;
      *gy += rel_acc[2] / 9.8f;
      // NOTICE_LOG(VR, "dt=%f", dt);
      // NOTICE_LOG(VR, "gx=%f, gy=%f, gz=%f", -rel_acc[0] / 9.8f, rel_acc[2] / 9.8f, rel_acc[1] /
      // 9.8f);
    }
    return true;
  }
#endif
  return false;
}

bool VR_GetNunchuckAccel(int index, float* gx, float* gy, float* gz)
{
#if defined(HAVE_OPENVR)
  if (g_has_openvr && index == 0)
  {
    // find the controllers for each hand, 100 = not found
    vr::TrackedDeviceIndex_t left_hand = 100, right_hand = 100;
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedControllerRole hand = m_pHMD->GetControllerRoleForTrackedDeviceIndex(i);
      if (hand == vr::TrackedControllerRole_LeftHand)
        left_hand = i;
      else if (hand == vr::TrackedControllerRole_RightHand)
        right_hand = i;
    }
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedDeviceClass kind = m_pHMD->GetTrackedDeviceClass(i);
      if (kind == vr::TrackedDeviceClass_Controller)
      {
        if (left_hand == 100 && i != right_hand)
          left_hand = i;
        else if (right_hand == 100 && i != left_hand)
          right_hand = i;
      }
    }
    if (left_hand >= vr::k_unMaxTrackedDeviceCount || !m_rTrackedDevicePose[left_hand].bPoseIsValid)
    {
      // NOTICE_LOG(VR, "invalid!");
      return false;
    }
    // float lx = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[0][3];
    // float ly = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[1][3];
    // float lz = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[2][3];
    Matrix33 m;
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        m.data[r * 3 + c] = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[c][r];
    float acc[3] = {};
    float dt = (float)(g_last_tracking_time - g_older_tracking_time);
    if (dt < 0.001f)
    {
      // NOTICE_LOG(VR, "too fast!");
      // return false;
    }
    for (int axis = 0; axis < 3; ++axis)
    {
      acc[axis] =
          (m_rTrackedDevicePose[left_hand].vVelocity.v[axis] - left_hand_older_velocity[axis]) / dt;
      left_hand_older_velocity[axis] = left_hand_old_velocity[axis];
      left_hand_old_velocity[axis] = m_rTrackedDevicePose[left_hand].vVelocity.v[axis];
    }
    // World-space accelerations need to be converted into accelerations relative to the Nunchuk's
    // sensor.
    float rel_acc[3];
    for (int i = 0; i < 3; ++i)
    {
      rel_acc[i] =
          acc[0] * m.data[i * 3 + 0] + acc[1] * m.data[i * 3 + 1] + acc[2] * m.data[i * 3 + 2];
      // todo: check if this is correct!
    }

    {
      // Tilt sensing.
      *gx = -m.data[0 * 3 + 1];
      *gz = m.data[1 * 3 + 1];
      *gy = m.data[2 * 3 + 1];

      // Convert from metres per second per second to G's, and to Wiimote's coordinate system.
      *gx -= rel_acc[0] / 9.8f;
      *gz += rel_acc[1] / 9.8f;
      *gy += rel_acc[2] / 9.8f;
    }
    return true;
  }
#endif
  return false;
}

bool VR_GetIR(int index, double* irx, double* iry, double* irz)
{
#if defined(HAVE_OPENVR)
  if (g_has_openvr)
  {
    if (g_vr_has_ir)
    {
      *irx = g_vr_ir_x;
      *iry = g_vr_ir_y;
      *irz = g_vr_ir_z;
      return true;
    }
  }
#endif
  return false;
}

#if defined(OVR_MAJOR_VERSION)
bool WasItTapped(ovrVector3f linearAcc, double time)
{
  const float thresholdForTap = 10.0f;
  const float thresholdForReset = 2.0f;
  const double keepDownTime = 0.050;
  float magOfAccelSquared =
      linearAcc.x * linearAcc.x + linearAcc.y * linearAcc.y + linearAcc.z * linearAcc.z;
  static bool readyForNewSingleTap = false;
  static double lastTapTime = 0.0;
  if (magOfAccelSquared < thresholdForReset * thresholdForReset)
    readyForNewSingleTap = true;
  if ((readyForNewSingleTap) && (magOfAccelSquared > thresholdForTap * thresholdForTap))
  {
    readyForNewSingleTap = false;
    lastTapTime = time;
    return (true);
  }
  return time - lastTapTime < keepDownTime;
}
#endif

bool VR_GetHMDGestures(u32* gestures)
{
  *gestures = 0;
#if defined(OVR_MAJOR_VERSION)
  if (g_has_rift)
  {
#if OVR_PRODUCT_VERSION >= 1 || OVR_MAJOR_VERSION >= 8
    ovrTrackingState t = ovr_GetTrackingState(hmd, 0, false);
#else
    ovrTrackingState t = ovrHmd_GetTrackingState(hmd, g_rift_frame_timing.ScanoutMidpointSeconds);
#endif
    if (WasItTapped(t.HeadPose.LinearAcceleration, t.HeadPose.TimeInSeconds))
    {
      *gestures |= OCULUS_BUTTON_A;
    }
    return true;
  }
  else
#endif
  {
    return false;
  }
}

void VR_UpdateWiimoteReportingMode(int index, u8 accel, u8 ir, u8 ext)
{
  g_vr_reading_wiimote_accel[index] = accel;
  g_vr_reading_wiimote_ir[index] = ir;
  g_vr_reading_wiimote_ext[index] = ext;
}

bool VR_GetLeftControllerPos(float* pos, float* thumbpos, Matrix33* m)
{
#if defined(HAVE_OPENVR)
  if (g_has_openvr)
  {
    // find the controllers for each hand, 100 = not found
    vr::TrackedDeviceIndex_t left_hand = 100, right_hand = 100;
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedControllerRole hand = m_pHMD->GetControllerRoleForTrackedDeviceIndex(i);
      if (hand == vr::TrackedControllerRole_LeftHand)
        left_hand = i;
      else if (hand == vr::TrackedControllerRole_RightHand)
        right_hand = i;
    }
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedDeviceClass kind = m_pHMD->GetTrackedDeviceClass(i);
      if (kind == vr::TrackedDeviceClass_Controller)
      {
        if (left_hand == 100 && i != right_hand)
          left_hand = i;
        else if (right_hand == 100 && i != left_hand)
          right_hand = i;
      }
    }
    if (left_hand >= vr::k_unMaxTrackedDeviceCount || !m_rTrackedDevicePose[left_hand].bPoseIsValid)
    {
      // NOTICE_LOG(VR, "invalid!");
      pos[0] = pos[1] = pos[2] = 0;
      return false;
    }
    float lx = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[0][3];
    float ly = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[1][3];
    float lz = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[2][3];
    pos[0] = lx;
    pos[1] = ly;
    pos[2] = lz;
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        m->data[r * 3 + c] = m_rTrackedDevicePose[left_hand].mDeviceToAbsoluteTracking.m[r][c];
    vr::VRControllerState_t cs;
#ifdef OPENVR_104_OR_ABOVE
    if (m_pHMD->GetControllerState(
            left_hand, &cs,
            sizeof(
                cs)))  // if error here, use OpenVR 1.05 or above instead of 1.03 OR use line below
#else
    if (m_pHMD->GetControllerState(left_hand, &cs))
#endif
    {
      thumbpos[0] = cs.rAxis[0].x;
      thumbpos[1] = cs.rAxis[0].y;
      if (cs.ulButtonPressed & ((u64)1 << vr::k_EButton_SteamVR_Touchpad))
        thumbpos[2] = 1;
      else if (cs.ulButtonTouched & ((u64)1 << vr::k_EButton_SteamVR_Touchpad))
        thumbpos[2] = 0;
      else
        thumbpos[2] = -1;
      if ((cs.ulButtonPressed | cs.ulButtonTouched) & ((u64)1 << vr::k_EButton_ApplicationMenu))
      {
        if (cs.ulButtonTouched & ((u64)1 << vr::k_EButton_SteamVR_Touchpad))
          thumbpos[1] = 1.18f;
        else
          thumbpos[1] = 1.5f;
        thumbpos[2] = 1;
      }
    }
    else
    {
      thumbpos[0] = thumbpos[1] = 0;
      thumbpos[2] = -1;
    }
    return true;
  }
  else
#endif
  {
    pos[0] = -0.15f;
    pos[1] = -0.30f;
    pos[2] = -0.4f;
    return true;
  }
}

bool VR_GetRightControllerPos(float* pos, float* thumbpos, Matrix33* m)
{
#if defined(HAVE_OPENVR)
  if (g_has_openvr)
  {
    // find the controllers for each hand, 100 = not found
    vr::TrackedDeviceIndex_t left_hand = 100, right_hand = 100;
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedControllerRole hand = m_pHMD->GetControllerRoleForTrackedDeviceIndex(i);
      if (hand == vr::TrackedControllerRole_LeftHand)
        left_hand = i;
      else if (hand == vr::TrackedControllerRole_RightHand)
        right_hand = i;
    }
    for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
      vr::ETrackedDeviceClass kind = m_pHMD->GetTrackedDeviceClass(i);
      if (kind == vr::TrackedDeviceClass_Controller)
      {
        if (left_hand == 100 && i != right_hand)
          left_hand = i;
        else if (right_hand == 100 && i != left_hand)
          right_hand = i;
      }
    }
    if (right_hand >= vr::k_unMaxTrackedDeviceCount ||
        !m_rTrackedDevicePose[right_hand].bPoseIsValid)
    {
      // NOTICE_LOG(VR, "invalid!");
      pos[0] = pos[1] = pos[2] = 0;
      return false;
    }
    float rx = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[0][3];
    float ry = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[1][3];
    float rz = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[2][3];
    pos[0] = rx;
    pos[1] = ry;
    pos[2] = rz;
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        m->data[r * 3 + c] = m_rTrackedDevicePose[right_hand].mDeviceToAbsoluteTracking.m[r][c];
    vr::VRControllerState_t cs;
#ifdef OPENVR_104_OR_ABOVE
    if (m_pHMD->GetControllerState(
            left_hand, &cs,
            sizeof(
                cs)))  // if error here, use OpenVR 1.05 or above instead of 1.03 OR use line below
#else
    if (m_pHMD->GetControllerState(right_hand, &cs))
#endif
    {
      thumbpos[0] = cs.rAxis[0].x;
      thumbpos[1] = cs.rAxis[0].y;
      if (cs.ulButtonPressed & ((u64)1 << vr::k_EButton_SteamVR_Touchpad))
        thumbpos[2] = 1;
      else if (cs.ulButtonTouched & ((u64)1 << vr::k_EButton_SteamVR_Touchpad))
        thumbpos[2] = 0;
      else
        thumbpos[2] = -1;
      if ((cs.ulButtonPressed | cs.ulButtonTouched) & ((u64)1 << vr::k_EButton_ApplicationMenu))
      {
        if (cs.ulButtonTouched & ((u64)1 << vr::k_EButton_SteamVR_Touchpad))
          thumbpos[1] = 1.18f;
        else
          thumbpos[1] = 1.5f;
        thumbpos[2] = 1;
      }
    }
    else
    {
      thumbpos[0] = thumbpos[1] = 0;
      thumbpos[2] = -1;
    }
    return true;
  }
  else
#endif
  {
    pos[0] = 0.15f;
    pos[1] = -0.30f;
    pos[2] = -0.4f;
    return true;
  }
}

void VR_SetGame(bool is_wii, bool is_nand, std::string id)
{
  g_is_nes = false;
  // GameCube uses GameCube controller
  if (!is_wii)
  {
    vr_left_controller = CS_GC_LEFT;
    vr_right_controller = CS_GC_RIGHT;
  }
  // Wii Discs or homebrew files use the Wiimote and Nunchuk
  else if (!is_nand)
  {
    vr_left_controller = CS_NUNCHUK_UNREAD;
    vr_right_controller = CS_WIIMOTE;
  }
  else
  {
    char c = ' ';
    if (id.length() > 0)
      c = id[0];
    switch (c)
    {
    case 'C':
    case 'X':
      // C64, MSX (or WiiWare demos)
      vr_left_controller = CS_ARCADE_LEFT;
      vr_right_controller = CS_ARCADE_RIGHT;
      break;
    case 'E':
      // Virtual arcade, Neo Geo
      vr_left_controller = CS_ARCADE_LEFT;
      vr_right_controller = CS_ARCADE_RIGHT;
      break;
    case 'F':
      // NES
      g_is_nes = true;
      if (id.length() > 3 && id[3] == 'J')
      {
        vr_left_controller = CS_FAMICON_LEFT;
        vr_right_controller = CS_FAMICON_RIGHT;
      }
      else
      {
        vr_left_controller = CS_NES_LEFT;
        vr_right_controller = CS_NES_RIGHT;
      }
      break;
    case 'J':
      vr_left_controller = CS_SNES_LEFT;
      // SNES
      if (id.length() > 3 && id[3] == 'E')
        vr_right_controller = CS_SNES_NTSC_RIGHT;
      else
        vr_right_controller = CS_SNES_RIGHT;
      break;
    case 'L':
      // Sega
      vr_left_controller = CS_SEGA_LEFT;
      vr_right_controller = CS_SEGA_RIGHT;
      break;
    case 'M':
      // Sega Genesis
      vr_left_controller = CS_GENESIS_LEFT;
      vr_right_controller = CS_GENESIS_RIGHT;
      break;
    case 'N':
      // N64
      vr_left_controller = CS_N64_LEFT;
      vr_right_controller = CS_N64_RIGHT;
      break;
    case 'P':
    case 'Q':
      // TurboGrafx
      vr_left_controller = CS_TURBOGRAFX_LEFT;
      vr_right_controller = CS_TURBOGRAFX_RIGHT;
      break;
    case 'H':
    case 'W':
    default:
      // WiiWare
      vr_left_controller = CS_NUNCHUK_UNREAD;
      vr_right_controller = CS_WIIMOTE;
      break;
    }
  }
}

ControllerStyle VR_GetHydraStyle(int hand)
{
  if (hand)
  {
    if (vr_right_controller == CS_WIIMOTE && g_vr_reading_wiimote_ir[0])
      vr_right_controller = CS_WIIMOTE_IR;
    else if (vr_right_controller == CS_WIIMOTE_IR && !g_vr_reading_wiimote_ir[0])
      vr_right_controller = CS_WIIMOTE;
    return vr_right_controller;
  }
  else
  {
    if (vr_left_controller == CS_NUNCHUK && !g_vr_reading_wiimote_ext[0])
      vr_left_controller = CS_NUNCHUK_UNREAD;
    else if (vr_left_controller == CS_NUNCHUK_UNREAD && g_vr_reading_wiimote_ext[0])
      vr_left_controller = CS_NUNCHUK;
    return vr_left_controller;
  }
}

bool VR_PairViveControllers()
{
#ifdef _WIN32
  HWND SteamVRStatusWindow = FindWindowA("Qt5QWindowIcon", "SteamVR Status");
  if (!SteamVRStatusWindow)
    return false;
  POINT C;
  BOOL hasC = GetCursorPos(&C);
  // HWND W = GetForegroundWindow();
  // RECT rw;
  // GetWindowRect(W, &rw);
  // SetForegroundWindow(SteamVRStatusWindow);
  INPUT input[16] = {};
  RECT r;
  if (!GetWindowRect(SteamVRStatusWindow, &r))
    return false;
  SetCursorPos(r.left + 100, r.top + 100);
  input[0].type = INPUT_MOUSE;
  input[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
  input[1].type = INPUT_MOUSE;
  input[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
  SendInput(2, &input[0], sizeof(INPUT));
  Sleep(100);

  input[2].type = INPUT_KEYBOARD;
  input[2].ki.wVk = VK_DOWN;
  input[3].ki.dwFlags = 0;
  input[3].type = INPUT_KEYBOARD;
  input[3].ki.wVk = VK_DOWN;
  input[3].ki.dwFlags = KEYEVENTF_KEYUP;
  input[6] = input[4] = input[2];
  input[7] = input[5] = input[3];
  SendInput(4, &input[2], sizeof(INPUT));
  Sleep(100);

  input[6].ki.wVk = VK_RETURN;
  input[7].ki.wVk = VK_RETURN;
  SendInput(2, &input[6], sizeof(INPUT));
  Sleep(100);

  if (hasC)
  {
    SetCursorPos(C.x, C.y);
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, input, sizeof(INPUT));
  }
  return true;
#else
  return false;
#endif
}

void OpcodeReplayBuffer()
{
  // Opcode Replay Buffer Code.  This enables the capture of all the Video Opcodes that occur during
  // a frame,
  // and then plays them back with new headtracking information.  Allows ways to easily set
  // headtracking at 75fps
  // for various games.  In Alpha right now, will crash many games/cause corruption.
  static int extra_video_loops_count = 0;
  static int real_frame_count = 0;
  if (g_ActiveConfig.bOpcodeReplay &&
      SConfig::GetInstance().m_GPUDeterminismMode != GPU_DETERMINISM_FAKE_COMPLETION)
  {
    g_opcode_replay_enabled = true;
    if (g_hmd_refresh_rate == 75)
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
        // if (real_frame_count % 4 == 0)
        //{
        //	g_ActiveConfig.iExtraVideoLoops = 1;
        //	g_ActiveConfig.iExtraVideoLoopsDivider = 0;
        //}
        // else
        //{
        //	g_ActiveConfig.iExtraVideoLoops = 0;
        //	g_ActiveConfig.iExtraVideoLoopsDivider = 0;
        //}
        g_ActiveConfig.iExtraVideoLoops = 1;
        g_ActiveConfig.iExtraVideoLoopsDivider = 3;
      }
    }
    else
    {
      // 90 FPS
      if (g_ActiveConfig.bPullUp20fps)
      {
        if (real_frame_count % 2 == 1)
        {
          g_ActiveConfig.iExtraVideoLoops = 4;
          g_ActiveConfig.iExtraVideoLoopsDivider = 0;
        }
        else
        {
          g_ActiveConfig.iExtraVideoLoops = 5;
          g_ActiveConfig.iExtraVideoLoopsDivider = 0;
        }
      }
      else if (g_ActiveConfig.bPullUp30fps)
      {
        g_ActiveConfig.iExtraVideoLoops = 2;
        g_ActiveConfig.iExtraVideoLoopsDivider = 0;
      }
      else if (g_ActiveConfig.bPullUp60fps)
      {
        // if (real_frame_count % 4 == 0)
        //{
        //	g_ActiveConfig.iExtraVideoLoops = 1;
        //	g_ActiveConfig.iExtraVideoLoopsDivider = 0;
        //}
        // else
        //{
        //	g_ActiveConfig.iExtraVideoLoops = 0;
        //	g_ActiveConfig.iExtraVideoLoopsDivider = 0;
        //}
        g_ActiveConfig.iExtraVideoLoops = 1;
        g_ActiveConfig.iExtraVideoLoopsDivider = 1;
      }
    }

    if ((g_opcode_replay_frame &&
         (extra_video_loops_count >= (int)g_ActiveConfig.iExtraVideoLoops)))
    {
      g_opcode_replay_frame = false;
      ++real_frame_count;
      extra_video_loops_count = 0;
    }
    else
    {
      if (skipped_opcode_replay_count >= (int)g_ActiveConfig.iExtraVideoLoopsDivider)
      {
        g_opcode_replay_frame = true;
        ++extra_video_loops_count;
        skipped_opcode_replay_count = 0;

        for (TimewarpLogEntry& entry : timewarp_logentries)
        {
          // VertexManager::s_pCurBufferPointer = s_pCurBufferPointer_log.at(i);
          // VertexManager::s_pEndBufferPointer = s_pEndBufferPointer_log.at(i);
          // VertexManager::s_pBaseBufferPointer = s_pBaseBufferPointer_log.at(i);

          // if (i == 0)
          //{
          // SCPFifoStruct &fifo = CommandProcessor::fifo;

          // fifo.CPBase = CPBase_log.at(i);
          // fifo.CPEnd = CPEnd_log.at(i);
          // fifo.CPHiWatermark = CPHiWatermark_log.at(i);
          // fifo.CPLoWatermark = CPLoWatermark_log.at(i);
          // fifo.CPReadWriteDistance = CPReadWriteDistance_log.at(i);
          // fifo.CPWritePointer = CPWritePointer_log.at(i);
          // fifo.CPReadPointer = CPReadPointer_log.at(i);
          // fifo.CPBreakpoint = CPBreakpoint_log.at(i);
          //}

          if (entry.is_preprocess_log)
          {
            OpcodeDecoder::Run<true>(entry.timewarp_log, nullptr, false);
          }
          else
          {
            OpcodeDecoder::Run<false>(entry.timewarp_log, nullptr, false);
          }
        }
      }
      else
      {
        ++skipped_opcode_replay_count;
      }
      // CPBase_log.resize(0);
      // CPEnd_log.resize(0);
      // CPHiWatermark_log.resize(0);
      // CPLoWatermark_log.resize(0);
      // CPReadWriteDistance_log.resize(0);
      // CPWritePointer_log.resize(0);
      // CPReadPointer_log.resize(0);
      // CPBreakpoint_log.resize(0);
      // CPBase_log.clear();
      // CPEnd_log.clear();
      // CPHiWatermark_log.clear();
      // CPLoWatermark_log.clear();
      // CPReadWriteDistance_log.clear();
      // CPWritePointer_log.clear();
      // CPReadPointer_log.clear();
      // CPBreakpoint_log.clear();
      // s_pCurBufferPointer_log.clear();
      // s_pCurBufferPointer_log.resize(0);
      // s_pEndBufferPointer_log.clear();
      // s_pEndBufferPointer_log.resize(0);
      // s_pBaseBufferPointer_log.clear();
      // s_pBaseBufferPointer_log.resize(0);
      timewarp_logentries.clear();
      timewarp_logentries.resize(0);
    }
  }
  else
  {
    if (g_opcode_replay_enabled)
    {
      timewarp_logentries.clear();
      timewarp_logentries.resize(0);
    }
    g_opcode_replay_enabled = false;
    g_opcode_replay_log_frame = false;
  }
}

void OpcodeReplayBufferInline()
{
  // Opcode Replay Buffer Code.  This enables the capture of all the Video Opcodes that occur during
  // a frame,
  // and then plays them back with new headtracking information.  Allows ways to easily set
  // headtracking at 75fps
  // for various games.  In Alpha right now, will crash many games/cause corruption.
  static int real_frame_count = 0;
  int extra_video_loops;
  if (g_ActiveConfig.bOpcodeReplay &&
      SConfig::GetInstance().m_GPUDeterminismMode != GPU_DETERMINISM_FAKE_COMPLETION)
  {
    g_opcode_replay_enabled = true;
    g_opcode_replay_log_frame = true;
    if (g_ActiveConfig.bPullUpAuto)
    {
      static int replay_count = 0;
      static int old_rate = 0;
      int real_framerate = ((int)((g_current_fps + 2.5) / 5)) * 5;
      if (real_framerate != old_rate)
      {
        // MessageBeep(MB_ICONASTERISK);
        WARN_LOG(VR, "new FPS = %d", real_framerate);
        replay_count = 0;
        old_rate = real_framerate;
      }
      if (real_framerate < 19 || real_framerate > g_hmd_refresh_rate ||
          (g_vr_has_asynchronous_timewarp && g_current_speed < 95.0f))
      {
        real_framerate = g_hmd_refresh_rate;
        replay_count = 0;
      }
      int replays_per_second = g_hmd_refresh_rate - real_framerate;
      extra_video_loops = 0;
      replay_count += replays_per_second;
      while (replay_count >= real_framerate)
      {
        ++extra_video_loops;
        replay_count -= real_framerate;
      }
      // check if next frame will need replays
      if (replay_count + replays_per_second < real_framerate)
        g_opcode_replay_log_frame = false;
    }
    else if (g_hmd_refresh_rate == 75)
    {
      if (g_ActiveConfig.bPullUp60fps)
      {
        if (real_frame_count % 4 == 0)
        {
          extra_video_loops = 1;
        }
        else
        {
          extra_video_loops = 0;
          if ((real_frame_count + 1) % 4 != 0)
            g_opcode_replay_log_frame = false;
        }
      }
      else if (g_ActiveConfig.bPullUp30fps)
      {
        if (real_frame_count % 2 == 1)
        {
          extra_video_loops = 1;
        }
        else
        {
          extra_video_loops = 2;
        }
      }
      else if (g_ActiveConfig.bPullUp20fps)
      {
        if (real_frame_count % 4 == 1)
        {
          extra_video_loops = 2;
        }
        else
        {
          extra_video_loops = 3;
        }
      }
      else
      {
        extra_video_loops = g_ActiveConfig.iExtraVideoLoops;
      }
    }
    else
    {
      // 90 FPS
      if (g_ActiveConfig.bPullUp60fps)
      {
        if (real_frame_count % 2 == 0)
        {
          extra_video_loops = 1;
        }
        else
        {
          extra_video_loops = 0;
          // if ((real_frame_count + 1) % 2 != 0)
          //	g_opcode_replay_log_frame = false;
        }
      }
      else if (g_ActiveConfig.bPullUp30fps)
      {
        extra_video_loops = 2;
      }
      else if (g_ActiveConfig.bPullUp20fps)
      {
        if (real_frame_count % 2 == 1)
        {
          extra_video_loops = 4;
        }
        else
        {
          extra_video_loops = 5;
        }
      }
      else
      {
        extra_video_loops = g_ActiveConfig.iExtraVideoLoops;
      }
    }
    ++real_frame_count;
    g_opcode_replay_frame = true;
    skipped_opcode_replay_count = 0;

    for (int num_extra_frames = 0; num_extra_frames < extra_video_loops; ++num_extra_frames)
    {
      for (TimewarpLogEntry& entry : timewarp_logentries)
      {
        if (entry.is_preprocess_log)
        {
          OpcodeDecoder::Run<true>(entry.timewarp_log, nullptr, false);
        }
        else
        {
          OpcodeDecoder::Run<false>(entry.timewarp_log, nullptr, false);
        }
      }
    }
    timewarp_logentries.clear();
    timewarp_logentries.resize(0);
    g_opcode_replay_frame = false;
  }
  else
  {
    if (g_opcode_replay_enabled)
    {
      timewarp_logentries.clear();
      timewarp_logentries.resize(0);
    }
    g_opcode_replay_enabled = false;
    g_opcode_replay_log_frame = false;
  }
  g_Config.iExtraVideoLoopsDivider = 0;
}
