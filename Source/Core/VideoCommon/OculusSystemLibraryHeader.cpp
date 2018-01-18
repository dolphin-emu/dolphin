#include <Windows.h>
#include <assert.h>

#include "OculusSystemLibraryHeader.h"

#pragma warning(push)
#pragma warning(disable: 4706)

static PFUNC_CHAR ovr_InitializeRenderingShim_Real;
static PFUNC_CHAR_INT ovr_InitializeRenderingShimVersion_Real;
static PFUNC_INIT ovr_Initialize_Real;
static PFUNC_VOID ovr_Shutdown_Real;
static PFUNC_PCHAR ovr_GetVersionString_Real;
static PFUNC_INT ovrHmd_Detect_Real;
static PFUNC_HMD_INT ovrHmd_Create_Real;
static PFUNC_HMD_INT32 ovrHmd_CreateDebug_Real;

PFUNC_VOID_HMD ovrHmd_RecenterPose, ovrHmd_EndFrameTiming, ovrHmd_Destroy;
PFUNC_PCHAR_HMD ovrHmd_GetLastError, ovrHmd_GetLatencyTestResult;
PFUNC_ATTACH ovrHmd_AttachToWindow;
PFUNC_UINT_HMD ovrHmd_GetEnabledCaps;
PFUNC_HMD_UINT ovrHmd_SetEnabledCaps, ovrHmd_ResetFrameTiming;
PFUNC_HMD_INT_INT ovrHmd_ConfigureTracking;
PFUNC_TRACKING_STATE ovrHmd_GetTrackingState;
PFUNC_FOV ovrHmd_GetFovTextureSize;
PFUNC_CONFIG ovrHmd_ConfigureRendering;
PFUNC_BEGIN ovrHmd_BeginFrame;
PFUNC_END ovrHmd_EndFrame;
PFUNC_EYEPOSES ovrHmd_GetEyePoses;
PFUNC_EYEPOSE ovrHmd_GetHmdPosePerEye;
PFUNC_RENDERDESC ovrHmd_GetRenderDesc;
PFUNC_DISTORTIONMESH ovrHmd_CreateDistortionMesh;
PFUNC_DISTORTIONMESHDEBUG ovrHmd_CreateDistortionMeshDebug;
PFUNC_DESTROYMESH ovrHmd_DestroyDistortionMesh;
PFUNC_SCALEOFFSET ovrHmd_GetRenderScaleAndOffset;
PFUNC_FRAMETIMING ovrHmd_GetFrameTiming, ovrHmd_BeginFrameTiming;
PFUNC_TIMEWARP ovrHmd_GetEyeTimewarpMatrices;
PFUNC_TIMEWARPDEBUG ovrHmd_GetEyeTimewarpMatricesDebug;
PFUNC_DOUBLE ovr_GetTimeInSeconds;
PFUNC_HMD_HSW ovrHmd_GetHSWDisplayState;
PFUNC_CHAR_HMD ovrHmd_DismissHSWDisplay;
PFUNC_LATENCY ovrHmd_ProcessLatencyTest, ovrHmd_GetLatencyTest2DrawColor;
PFUNC_DOUBLE_DOUBLE ovr_WaitTillTime;
PFUNC_PROJECTION ovrMatrix4f_Projection;

static bool dll_loaded = false;
static HMODULE dll = 0;

static bool LoadOculusDLL()
{
  if (dll_loaded)
    return true;
  assert(sizeof(int) == 4);
#ifdef _WIN32
#ifdef _WIN64
  dll = LoadLibraryA("LibOVRRT64_0_5.dll");
#else
  dll = LoadLibraryA("LibOVRRT32_0_5.dll");
#endif
  if (!dll)
    return false;
  bool b = true;
  b = b && (ovr_InitializeRenderingShim_Real =
                (PFUNC_CHAR)GetProcAddress(dll, "ovr_InitializeRenderingShim"));
  b = b && (ovr_InitializeRenderingShimVersion_Real =
                (PFUNC_CHAR_INT)GetProcAddress(dll, "ovr_InitializeRenderingShimVersion"));
  b = b && (ovr_Initialize_Real = (PFUNC_INIT)GetProcAddress(dll, "ovr_Initialize"));
  b = b && (ovr_Shutdown_Real = (PFUNC_VOID)GetProcAddress(dll, "ovr_Shutdown"));
  b = b && (ovr_GetVersionString_Real = (PFUNC_PCHAR)GetProcAddress(dll, "ovr_GetVersionString"));
  b = b && (ovrHmd_Detect_Real = (PFUNC_INT)GetProcAddress(dll, "ovrHmd_Detect"));
  b = b && (ovrHmd_Create_Real = (PFUNC_HMD_INT)GetProcAddress(dll, "ovrHmd_Create"));
  b = b && (ovrHmd_CreateDebug_Real = (PFUNC_HMD_INT32)GetProcAddress(dll, "ovrHmd_CreateDebug"));

  b = b && (ovrHmd_Destroy = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovrHmd_Destroy"));
  b = b && (ovrHmd_RecenterPose = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovrHmd_RecenterPose"));
  b = b && (ovrHmd_EndFrameTiming = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovrHmd_EndFrameTiming"));
  b = b && (ovrHmd_GetLastError = (PFUNC_PCHAR_HMD)GetProcAddress(dll, "ovrHmd_GetLastError"));
  b = b && (ovrHmd_GetLatencyTestResult =
                (PFUNC_PCHAR_HMD)GetProcAddress(dll, "ovrHmd_GetLatencyTestResult"));
  b = b && (ovrHmd_AttachToWindow = (PFUNC_ATTACH)GetProcAddress(dll, "ovrHmd_AttachToWindow"));
  b = b && (ovrHmd_GetEnabledCaps = (PFUNC_UINT_HMD)GetProcAddress(dll, "ovrHmd_GetEnabledCaps"));
  b = b && (ovrHmd_SetEnabledCaps = (PFUNC_HMD_UINT)GetProcAddress(dll, "ovrHmd_SetEnabledCaps"));
  b = b &&
      (ovrHmd_ResetFrameTiming = (PFUNC_HMD_UINT)GetProcAddress(dll, "ovrHmd_ResetFrameTiming"));
  b = b && (ovrHmd_ConfigureTracking =
                (PFUNC_HMD_INT_INT)GetProcAddress(dll, "ovrHmd_ConfigureTracking"));
  b = b && (ovrHmd_GetTrackingState =
                (PFUNC_TRACKING_STATE)GetProcAddress(dll, "ovrHmd_GetTrackingState"));
  b = b && (ovrHmd_GetFovTextureSize = (PFUNC_FOV)GetProcAddress(dll, "ovrHmd_GetFovTextureSize"));
  b = b &&
      (ovrHmd_ConfigureRendering = (PFUNC_CONFIG)GetProcAddress(dll, "ovrHmd_ConfigureRendering"));
  b = b && (ovrHmd_BeginFrame = (PFUNC_BEGIN)GetProcAddress(dll, "ovrHmd_BeginFrame"));
  b = b && (ovrHmd_EndFrame = (PFUNC_END)GetProcAddress(dll, "ovrHmd_EndFrame"));
  b = b && (ovrHmd_GetEyePoses = (PFUNC_EYEPOSES)GetProcAddress(dll, "ovrHmd_GetEyePoses"));
  b = b &&
      (ovrHmd_GetHmdPosePerEye = (PFUNC_EYEPOSE)GetProcAddress(dll, "ovrHmd_GetHmdPosePerEye"));
  b = b && (ovrHmd_GetRenderDesc = (PFUNC_RENDERDESC)GetProcAddress(dll, "ovrHmd_GetRenderDesc"));
  b = b && (ovrHmd_CreateDistortionMesh =
                (PFUNC_DISTORTIONMESH)GetProcAddress(dll, "ovrHmd_CreateDistortionMesh"));
  b = b && (ovrHmd_CreateDistortionMeshDebug =
                (PFUNC_DISTORTIONMESHDEBUG)GetProcAddress(dll, "ovrHmd_CreateDistortionMeshDebug"));
  b = b && (ovrHmd_DestroyDistortionMesh =
                (PFUNC_DESTROYMESH)GetProcAddress(dll, "ovrHmd_DestroyDistortionMesh"));
  b = b && (ovrHmd_GetRenderScaleAndOffset =
                (PFUNC_SCALEOFFSET)GetProcAddress(dll, "ovrHmd_GetRenderScaleAndOffset"));
  b = b &&
      (ovrHmd_BeginFrameTiming = (PFUNC_FRAMETIMING)GetProcAddress(dll, "ovrHmd_BeginFrameTiming"));
  b = b &&
      (ovrHmd_GetFrameTiming = (PFUNC_FRAMETIMING)GetProcAddress(dll, "ovrHmd_GetFrameTiming"));
  b = b && (ovrHmd_GetEyeTimewarpMatrices =
                (PFUNC_TIMEWARP)GetProcAddress(dll, "ovrHmd_GetEyeTimewarpMatrices"));
  b = b && (ovrHmd_GetEyeTimewarpMatricesDebug =
                (PFUNC_TIMEWARPDEBUG)GetProcAddress(dll, "ovrHmd_GetEyeTimewarpMatricesDebug"));
  b = b && (ovr_GetTimeInSeconds = (PFUNC_DOUBLE)GetProcAddress(dll, "ovr_GetTimeInSeconds"));
  b = b &&
      (ovrHmd_GetHSWDisplayState = (PFUNC_HMD_HSW)GetProcAddress(dll, "ovrHmd_GetHSWDisplayState"));
  b = b &&
      (ovrHmd_DismissHSWDisplay = (PFUNC_CHAR_HMD)GetProcAddress(dll, "ovrHmd_DismissHSWDisplay"));
  b = b &&
      (ovrHmd_ProcessLatencyTest = (PFUNC_LATENCY)GetProcAddress(dll, "ovrHmd_ProcessLatencyTest"));
  b = b && (ovrHmd_GetLatencyTest2DrawColor =
                (PFUNC_LATENCY)GetProcAddress(dll, "ovrHmd_GetLatencyTest2DrawColor"));
  b = b && (ovr_WaitTillTime = (PFUNC_DOUBLE_DOUBLE)GetProcAddress(dll, "ovr_WaitTillTime"));
  b = b &&
      (ovrMatrix4f_Projection = (PFUNC_PROJECTION)GetProcAddress(dll, "ovrMatrix4f_Projection"));
  if (!b)
    FreeLibrary(dll);
  dll_loaded = b;
  return b;
#else
  return false;
#endif
}

static void FreeOculusDLL()
{
  if (dll_loaded)
  {
#ifdef _WIN32
    FreeLibrary(dll);
    dll = 0;
#endif
  }
  dll_loaded = false;
}

bool ovr_InitializeRenderingShim()
{
  if (!LoadOculusDLL())
    return false;
  return ovr_InitializeRenderingShim_Real() != 0;
}

bool ovr_InitializeRenderingShimVersion(int MinorVersion)
{
  if (!LoadOculusDLL())
    return false;
  return ovr_InitializeRenderingShimVersion_Real(MinorVersion) != 0;
}

bool ovr_Initialize(void* initParams)
{
  if (!LoadOculusDLL())
    return false;
  return ovr_Initialize_Real(initParams) != 0;
}

const char* ovr_GetVersionString(void)
{
  if (!dll_loaded)
    return "0.0.0.0";
  return ovr_GetVersionString_Real();
}

int ovrHmd_Detect(void)
{
  if (!dll_loaded)
    return -1;
  return ovrHmd_Detect_Real();
}

ovrHmd ovrHmd_Create(int number)
{
  if (!dll_loaded)
    return nullptr;
  return ovrHmd_Create_Real(number);
}

ovrHmd ovrHmd_CreateDebug(int version_ovrHmd)
{
  if (!dll_loaded)
    return nullptr;
  return ovrHmd_CreateDebug_Real(version_ovrHmd);
}

void ovr_Shutdown()
{
  if (!dll_loaded)
    return;
  if (ovr_Shutdown_Real)
    ovr_Shutdown_Real();
  FreeOculusDLL();
}

#pragma warning(pop)