#include <Windows.h>
#include <assert.h>

#include "OculusSystemLibraryHeader.h"

#define CONV_RESULT(result, error) ((g_libovr_version >= libovr_060) ? result : ((result & 0xFF) ? ovrSuccess : error))

TLibOvrVersion g_libovr_version;
ovrGraphicsLuid s_libovr_luid;
ovrGraphicsLuid *g_libovr_luid;

static PFUNC_CHAR ovr_InitializeRenderingShim_Real;
static PFUNC_CHAR_INT ovr_InitializeRenderingShimVersion_Real;
static PFUNC_INIT ovr_Initialize_Real;
static PFUNC_VOID ovr_Shutdown_Real;
static PFUNC_PCHAR ovr_GetVersionString_Real;
static PFUNC_INT ovrHmd_Detect_Real;
static PFUNC_HMD_INT_INT ovrHmd_ConfigureTracking_Real;

// SDK 0.5
static PFUNC_HMD_INT ovrHmd_Create_Real;
static PFUNC_HMD_INT32 ovrHmd_CreateDebug_Real;
static PFUNC_CHAR_HMD ovrHmd_DismissHSWDisplay_Real;
static PFUNC_HMD_HSW ovrHmd_GetHSWDisplayState_Real;

// SDK 0.6
static PFUNC_CREATE6 ovrHmd_Create_Real_6, ovrHmd_CreateDebug_Real_6;

// SDK 0.7
static PFUNC_HMDDESC ovr_GetHmdDesc_Real;
static PFUNC_CREATE7 ovr_Create_Real;

PFUNC_VOID_HMD ovrHmd_RecenterPose, ovrHmd_EndFrameTiming, ovrHmd_Destroy, ovr_ResetBackOfHeadTracking, ovr_ResetMulticameraTracking;
PFUNC_PCHAR_HMD ovrHmd_GetLastError, ovrHmd_GetLatencyTestResult;
PFUNC_ATTACH ovrHmd_AttachToWindow;
PFUNC_UINT_HMD ovrHmd_GetEnabledCaps;
PFUNC_HMD_UINT ovrHmd_SetEnabledCaps, ovrHmd_ResetFrameTiming;
PFUNC_TRACKING_STATE ovrHmd_GetTrackingState;
PFUNC_FOV ovrHmd_GetFovTextureSize;
PFUNC_CONFIG ovrHmd_ConfigureRendering;
PFUNC_BEGIN ovrHmd_BeginFrame, ovrHmd_GetFrameTiming, ovrHmd_BeginFrameTiming;
PFUNC_END ovrHmd_EndFrame;
PFUNC_EYEPOSES ovrHmd_GetEyePoses;
PFUNC_EYEPOSE ovrHmd_GetHmdPosePerEye;
PFUNC_RENDERDESC ovrHmd_GetRenderDesc;
PFUNC_DISTORTIONMESH ovrHmd_CreateDistortionMesh;
PFUNC_DISTORTIONMESHDEBUG ovrHmd_CreateDistortionMeshDebug;
PFUNC_DESTROYMESH ovrHmd_DestroyDistortionMesh;
PFUNC_SCALEOFFSET ovrHmd_GetRenderScaleAndOffset;
PFUNC_FRAMETIMING ovr_GetFrameTiming;
PFUNC_TIMEWARP ovrHmd_GetEyeTimewarpMatrices;
PFUNC_TIMEWARPDEBUG ovrHmd_GetEyeTimewarpMatricesDebug;
PFUNC_DOUBLE ovr_GetTimeInSeconds;
PFUNC_LATENCY ovrHmd_ProcessLatencyTest, ovrHmd_GetLatencyTest2DrawColor;
PFUNC_DOUBLE_DOUBLE ovr_WaitTillTime;
PFUNC_PROJECTION ovrMatrix4f_Projection;
PFUNC_GETBOOL ovrHmd_GetBool;
PFUNC_GETFLOAT ovrHmd_GetFloat;
PFUNC_GETFLOATARRAY ovrHmd_GetFloatArray;
PFUNC_GETINT ovrHmd_GetInt;
PFUNC_GETSTRING ovrHmd_GetString;
PFUNC_SETBOOL ovrHmd_SetBool;
PFUNC_SETFLOAT ovrHmd_SetFloat;
PFUNC_SETFLOATARRAY ovrHmd_SetFloatArray;
PFUNC_SETINT ovrHmd_SetInt;
PFUNC_SETSTRING ovrHmd_SetString;
PFUNC_TRACE ovr_TraceMessage;
PFUNC_ORTHO ovrMatrix4f_OrthoSubProjection;
PFUNC_TIMEWARPPROJ ovrTimewarpProjectionDesc_FromProjection;

PFUNC_CREATEMIRRORD3D116 ovrHmd_CreateMirrorTextureD3D11;
PFUNC_CREATESWAPD3D116 ovrHmd_CreateSwapTextureSetD3D11;

PFUNC_CREATEMIRRORGL ovrHmd_CreateMirrorTextureGL;
PFUNC_CREATESWAPGL ovrHmd_CreateSwapTextureSetGL;
PFUNC_DESTROYMIRROR ovrHmd_DestroyMirrorTexture;
PFUNC_DESTROYSWAP ovrHmd_DestroySwapTextureSet;
PFUNC_CALC ovr_CalcEyePoses;
PFUNC_ERRORINFO ovr_GetLastErrorInfo;
PFUNC_SUBMIT ovrHmd_SubmitFrame;

PFUNC_CREATEMIRRORD3D117 ovr_CreateMirrorTextureD3D11;
PFUNC_CREATESWAPD3D117 ovr_CreateSwapTextureSetD3D11;
PFUNC_INPUT ovr_GetInputState;
PFUNC_LOOKUP ovr_Lookup;
PFUNC_VIBE ovr_SetControllerVibration;
PFUNC_TRACKING_STATE7 ovr_GetTrackingState;
PFUNC_EYEPOSES7 ovr_GetEyePoses;

static bool dll_loaded = false;
static HMODULE dll = 0;

#ifdef _WIN32
static bool Load7()
{
	TLibOvrVersion v = libovr_070;
#ifdef _WIN64
	dll = LoadLibraryA("LibOVRRT64_0_7.dll");
#else
	dll = LoadLibraryA("LibOVRRT32_0_7.dll");
#endif
	if (!dll)
		return false;
	bool b = true;
	ovr_InitializeRenderingShim_Real = nullptr;
	ovr_InitializeRenderingShimVersion_Real = nullptr;
	ovrHmd_Detect_Real = nullptr;
	ovrHmd_DismissHSWDisplay_Real = nullptr;
	ovrHmd_Create_Real_6 = nullptr;
	ovrHmd_CreateDebug_Real = nullptr;
	ovrHmd_CreateDebug_Real_6 = nullptr;

	ovrHmd_GetHSWDisplayState = nullptr;
	ovrHmd_EndFrameTiming = nullptr;
	ovrHmd_GetLastError = nullptr;
	ovrHmd_GetLatencyTestResult = nullptr;
	ovrHmd_AttachToWindow = nullptr;
	ovrHmd_ResetFrameTiming = nullptr;
	ovrHmd_ConfigureRendering = nullptr;
	ovrHmd_BeginFrame = nullptr;
	ovrHmd_EndFrame = nullptr;
	ovrHmd_GetHmdPosePerEye = nullptr;
	ovrHmd_CreateDistortionMesh = nullptr;
	ovrHmd_CreateDistortionMeshDebug = nullptr;
	ovrHmd_DestroyDistortionMesh = nullptr;
	ovrHmd_GetRenderScaleAndOffset = nullptr;
	ovrHmd_BeginFrameTiming = nullptr;
	ovrHmd_GetEyeTimewarpMatrices = nullptr;
	ovrHmd_GetEyeTimewarpMatricesDebug = nullptr;
	ovrHmd_ProcessLatencyTest = nullptr;
	ovrHmd_GetLatencyTest2DrawColor = nullptr;
	ovr_WaitTillTime = nullptr;

	b = b && (ovrMatrix4f_OrthoSubProjection = (PFUNC_ORTHO)GetProcAddress(dll, "ovrMatrix4f_OrthoSubProjection"));
	b = b && (ovrMatrix4f_Projection = (PFUNC_PROJECTION)GetProcAddress(dll, "ovrMatrix4f_Projection"));
	b = b && (ovrTimewarpProjectionDesc_FromProjection = (PFUNC_TIMEWARPPROJ)GetProcAddress(dll, "ovrTimewarpProjectionDesc_FromProjection"));
	b = b && (ovr_CalcEyePoses = (PFUNC_CALC)GetProcAddress(dll, "ovr_CalcEyePoses"));
	b = b && (ovrHmd_ConfigureTracking_Real = (PFUNC_HMD_INT_INT)GetProcAddress(dll, "ovr_ConfigureTracking"));
	b = b && (ovr_Create_Real = (PFUNC_CREATE7)GetProcAddress(dll, "ovr_Create"));
	b = b && (ovrHmd_CreateMirrorTextureGL = (PFUNC_CREATEMIRRORGL)GetProcAddress(dll, "ovr_CreateMirrorTextureGL"));
	b = b && (ovr_CreateMirrorTextureD3D11 = (PFUNC_CREATEMIRRORD3D117)GetProcAddress(dll, "ovr_CreateMirrorTextureD3D11"));
	b = b && (ovrHmd_CreateSwapTextureSetGL = (PFUNC_CREATESWAPGL)GetProcAddress(dll, "ovr_CreateSwapTextureSetGL"));
	b = b && (ovr_CreateSwapTextureSetD3D11 = (PFUNC_CREATESWAPD3D117)GetProcAddress(dll, "ovr_CreateSwapTextureSetD3D11"));
	b = b && (ovrHmd_DestroyMirrorTexture = (PFUNC_DESTROYMIRROR)GetProcAddress(dll, "ovr_DestroyMirrorTexture"));
	b = b && (ovrHmd_DestroySwapTextureSet = (PFUNC_DESTROYSWAP)GetProcAddress(dll, "ovr_DestroySwapTextureSet"));
	b = b && (ovrHmd_Destroy = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovr_Destroy"));
	b = b && (ovrHmd_GetBool = (PFUNC_GETBOOL)GetProcAddress(dll, "ovr_GetBool"));
	b = b && (ovr_GetEyePoses = (PFUNC_EYEPOSES7)GetProcAddress(dll, "ovr_GetEyePoses"));
	b = b && (ovrHmd_GetEnabledCaps = (PFUNC_UINT_HMD)GetProcAddress(dll, "ovr_GetEnabledCaps"));
	b = b && (ovrHmd_GetFloat = (PFUNC_GETFLOAT)GetProcAddress(dll, "ovr_GetFloat"));
	b = b && (ovrHmd_GetFloatArray = (PFUNC_GETFLOATARRAY)GetProcAddress(dll, "ovr_GetFloatArray"));
	b = b && (ovrHmd_GetFovTextureSize = (PFUNC_FOV)GetProcAddress(dll, "ovr_GetFovTextureSize"));
	b = b && (ovr_GetFrameTiming = (PFUNC_FRAMETIMING)GetProcAddress(dll, "ovr_GetFrameTiming"));
	b = b && (ovr_GetHmdDesc_Real = (PFUNC_HMDDESC)GetProcAddress(dll, "ovr_GetHmdDesc"));
	b = b && (ovr_GetInputState = (PFUNC_INPUT)GetProcAddress(dll, "ovr_GetInputState"));
	b = b && (ovrHmd_GetInt = (PFUNC_GETINT)GetProcAddress(dll, "ovr_GetInt"));
	b = b && (ovr_GetLastErrorInfo = (PFUNC_ERRORINFO)GetProcAddress(dll, "ovr_GetLastErrorInfo"));
	b = b && (ovrHmd_GetRenderDesc = (PFUNC_RENDERDESC)GetProcAddress(dll, "ovr_GetRenderDesc"));
	b = b && (ovrHmd_GetString = (PFUNC_GETSTRING)GetProcAddress(dll, "ovr_GetString"));
	b = b && (ovr_GetTimeInSeconds = (PFUNC_DOUBLE)GetProcAddress(dll, "ovr_GetTimeInSeconds"));
	b = b && (ovr_GetTrackingState = (PFUNC_TRACKING_STATE7)GetProcAddress(dll, "ovr_GetTrackingState"));
	b = b && (ovr_GetVersionString_Real = (PFUNC_PCHAR)GetProcAddress(dll, "ovr_GetVersionString"));
	b = b && (ovr_Initialize_Real = (PFUNC_INIT)GetProcAddress(dll, "ovr_Initialize"));
	b = b && (ovr_Lookup = (PFUNC_LOOKUP)GetProcAddress(dll, "ovr_Lookup"));
	b = b && (ovrHmd_RecenterPose = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovr_RecenterPose"));
	b = b && (ovr_ResetBackOfHeadTracking = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovr_ResetBackOfHeadTracking"));
	b = b && (ovr_ResetMulticameraTracking = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovr_ResetMulticameraTracking"));
	b = b && (ovrHmd_SetBool = (PFUNC_SETBOOL)GetProcAddress(dll, "ovr_SetBool"));
	b = b && (ovr_SetControllerVibration = (PFUNC_VIBE)GetProcAddress(dll, "ovr_SetControllerVibration"));
	b = b && (ovrHmd_SetEnabledCaps = (PFUNC_HMD_UINT)GetProcAddress(dll, "ovr_SetEnabledCaps"));
	b = b && (ovrHmd_SetFloat = (PFUNC_SETFLOAT)GetProcAddress(dll, "ovr_SetFloat"));
	b = b && (ovrHmd_SetFloatArray = (PFUNC_SETFLOATARRAY)GetProcAddress(dll, "ovr_SetFloatArray"));
	b = b && (ovrHmd_SetInt = (PFUNC_SETINT)GetProcAddress(dll, "ovr_SetInt"));
	b = b && (ovrHmd_SetString = (PFUNC_SETSTRING)GetProcAddress(dll, "ovr_SetString"));
	b = b && (ovr_Shutdown_Real = (PFUNC_VOID)GetProcAddress(dll, "ovr_Shutdown"));
	b = b && (ovrHmd_SubmitFrame = (PFUNC_SUBMIT)GetProcAddress(dll, "ovr_SubmitFrame"));
	b = b && (ovr_TraceMessage = (PFUNC_TRACE)GetProcAddress(dll, "ovr_TraceMessage"));
	if (!b)
		FreeLibrary(dll);
	else
		g_libovr_version = libovr_070;
	dll_loaded = b;
	return b;
}

static bool Load6()
{
#ifdef _WIN64
	dll = LoadLibraryA("LibOVRRT64_0_6.dll");
#else
	dll = LoadLibraryA("LibOVRRT32_0_6.dll");
#endif
	if (!dll)
		return false;

	ovr_InitializeRenderingShim_Real = nullptr;
	ovr_Create_Real = nullptr;
	ovr_GetHmdDesc_Real = nullptr;
	ovr_CreateMirrorTextureD3D11 = nullptr;
	ovr_CreateSwapTextureSetD3D11 = nullptr;
	ovr_GetInputState = nullptr;
	ovr_Lookup = nullptr;
	ovr_SetControllerVibration = nullptr;
	ovr_GetEyePoses = nullptr;

	bool b = true;
	b = b && (ovrHmd_ConfigureTracking_Real = (PFUNC_HMD_INT_INT)GetProcAddress(dll, "ovr_ConfigureTracking"));
	b = b && (ovrHmd_Create_Real_6 = (PFUNC_CREATE6)GetProcAddress(dll, "ovrHmd_Create"));
	b = b && (ovrHmd_CreateDebug_Real_6 = (PFUNC_CREATE6)GetProcAddress(dll, "ovrHmd_CreateDebug"));
	b = b && (ovrHmd_CreateMirrorTextureD3D11 = (PFUNC_CREATEMIRRORD3D116)GetProcAddress(dll, "ovrHmd_CreateMirrorTextureD3D11"));
	b = b && (ovrHmd_CreateMirrorTextureGL = (PFUNC_CREATEMIRRORGL)GetProcAddress(dll, "ovrHmd_CreateMirrorTextureGL"));
	b = b && (ovrHmd_CreateSwapTextureSetD3D11 = (PFUNC_CREATESWAPD3D116)GetProcAddress(dll, "ovrHmd_CreateSwapTextureSetD3D11"));
	b = b && (ovrHmd_CreateSwapTextureSetGL = (PFUNC_CREATESWAPGL)GetProcAddress(dll, "ovrHmd_CreateSwapTextureSetGL"));
	b = b && (ovrHmd_Destroy = (PFUNC_VOID_HMD)GetProcAddress(dll, "ovrHmd_Destroy"));
	b = b && (ovrHmd_DestroyMirrorTexture = (PFUNC_DESTROYMIRROR)GetProcAddress(dll, "ovrHmd_DestroyMirrorTexture"));
	b = b && (ovrHmd_DestroySwapTextureSet = (PFUNC_DESTROYSWAP)GetProcAddress(dll, "ovrHmd_DestroySwapTextureSet"));
	b = b && (ovrHmd_Detect_Real = (PFUNC_INT)GetProcAddress(dll, "ovrHmd_Detect"));
	b = b && (ovrHmd_GetBool = (PFUNC_GETBOOL)GetProcAddress(dll, "ovrHmd_GetBool"));
	b = b && (ovrHmd_GetEnabledCaps = (PFUNC_UINT_HMD)GetProcAddress(dll, "ovrHmd_GetEnabledCaps"));
	b = b && (ovrHmd_GetEyePoses = (PFUNC_EYEPOSES)GetProcAddress(dll, "ovrHmd_GetEyePoses"));
	b = b && (ovrHmd_GetFloat = (PFUNC_GETFLOAT)GetProcAddress(dll, "ovrHmd_GetFloat"));
	b = b && (ovrHmd_GetFloatArray = (PFUNC_GETFLOATARRAY)GetProcAddress(dll, "ovrHmd_GetFloatArray"));
	b = b && (ovrHmd_GetFovTextureSize = (PFUNC_FOV)GetProcAddress(dll, "ovrHmd_GetFovTextureSize"));
	b = b && (ovr_GetFrameTiming = (PFUNC_FRAMETIMING)GetProcAddress(dll, "ovrHmd_GetFrameTiming"));
	b = b && (ovrHmd_GetInt = (PFUNC_GETINT)GetProcAddress(dll, "ovrHmd_GetInt"));
	b = b && (ovrHmd_GetRenderDesc = (PFUNC_RENDERDESC)GetProcAddress(dll, "ovrHmd_GetRenderDesc"));
	b = b && (ovrHmd_GetString = (PFUNC_GETSTRING)GetProcAddress(dll, "ovrHmd_GetString"));
	b = b && (ovrHmd_SetBool = (PFUNC_SETBOOL)GetProcAddress(dll, "ovrHmd_SetBool"));
	b = b && (ovrHmd_SetEnabledCaps = (PFUNC_HMD_UINT)GetProcAddress(dll, "ovrHmd_SetEnabledCaps"));
	b = b && (ovrHmd_SetFloat = (PFUNC_SETFLOAT)GetProcAddress(dll, "ovrHmd_SetFloat"));
	b = b && (ovrHmd_SetFloatArray = (PFUNC_SETFLOATARRAY)GetProcAddress(dll, "ovrHmd_SetFloatArray"));
	b = b && (ovrHmd_SetInt = (PFUNC_SETINT)GetProcAddress(dll, "ovrHmd_SetInt"));
	b = b && (ovrHmd_SetString = (PFUNC_SETSTRING)GetProcAddress(dll, "ovrHmd_SetString"));
	b = b && (ovrHmd_SubmitFrame = (PFUNC_SUBMIT)GetProcAddress(dll, "ovrHmd_SubmitFrame"));
	b = b && (ovrMatrix4f_OrthoSubProjection = (PFUNC_ORTHO)GetProcAddress(dll, "ovrMatrix4f_OrthoSubProjection"));
	b = b && (ovrMatrix4f_Projection = (PFUNC_PROJECTION)GetProcAddress(dll, "ovrMatrix4f_Projection"));
	b = b && (ovrTimewarpProjectionDesc_FromProjection = (PFUNC_TIMEWARPPROJ)GetProcAddress(dll, "ovrTimewarpProjectionDesc_FromProjection"));
	b = b && (ovr_CalcEyePoses = (PFUNC_CALC)GetProcAddress(dll, "ovr_CalcEyePoses"));
	b = b && (ovr_GetLastErrorInfo = (PFUNC_ERRORINFO)GetProcAddress(dll, "ovr_GetLastErrorInfo"));
	b = b && (ovr_Initialize_Real = (PFUNC_INIT)GetProcAddress(dll, "ovr_Initialize"));
	b = b && (ovr_InitializeRenderingShimVersion_Real = (PFUNC_CHAR_INT)GetProcAddress(dll, "ovr_InitializeRenderingShimVersion"));
	b = b && (ovr_Shutdown_Real = (PFUNC_VOID)GetProcAddress(dll, "ovr_Shutdown"));
	b = b && (ovr_TraceMessage = (PFUNC_TRACE)GetProcAddress(dll, "ovr_TraceMessage"));
	b = b && (ovr_WaitTillTime = (PFUNC_DOUBLE_DOUBLE)GetProcAddress(dll, "ovr_WaitTillTime"));
	if (!b)
		FreeLibrary(dll);
	else
		g_libovr_version = libovr_060;
	dll_loaded = b;
	return b;
}

static bool Load5()
{
#ifdef _WIN64
	dll = LoadLibraryA("LibOVRRT64_0_5.dll");
#else
	dll = LoadLibraryA("LibOVRRT32_0_5.dll");
#endif
	if (!dll)
		return false;

	ovrHmd_Create_Real_6 = nullptr;
	ovrHmd_CreateDebug_Real_6 = nullptr;
	ovrHmd_CreateMirrorTextureGL = nullptr;
	ovrHmd_CreateMirrorTextureD3D11 = nullptr;
	ovrHmd_CreateSwapTextureSetGL = nullptr;
	ovrHmd_CreateSwapTextureSetD3D11 = nullptr;
	ovrHmd_DestroyMirrorTexture = nullptr;
	ovrHmd_DestroySwapTextureSet = nullptr;
	ovrHmd_SubmitFrame = nullptr;

	ovr_CalcEyePoses = nullptr;
	ovr_Create_Real = nullptr;
	ovr_GetHmdDesc_Real = nullptr;
	ovr_CreateMirrorTextureD3D11 = nullptr;
	ovr_CreateSwapTextureSetD3D11 = nullptr;
	ovr_GetInputState = nullptr;
	ovr_GetLastErrorInfo = nullptr;
	ovr_Lookup = nullptr;
	ovr_SetControllerVibration = nullptr;
	ovr_GetEyePoses = nullptr;

	bool b = true;
	b = b && (ovr_InitializeRenderingShim_Real = (PFUNC_CHAR)GetProcAddress(dll, "ovr_InitializeRenderingShim"));
	b = b && (ovr_InitializeRenderingShimVersion_Real = (PFUNC_CHAR_INT)GetProcAddress(dll, "ovr_InitializeRenderingShimVersion"));
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
	b = b && (ovrHmd_GetLatencyTestResult = (PFUNC_PCHAR_HMD)GetProcAddress(dll, "ovrHmd_GetLatencyTestResult"));
	b = b && (ovrHmd_AttachToWindow = (PFUNC_ATTACH)GetProcAddress(dll, "ovrHmd_AttachToWindow"));
	b = b && (ovrHmd_GetEnabledCaps = (PFUNC_UINT_HMD)GetProcAddress(dll, "ovrHmd_GetEnabledCaps"));
	b = b && (ovrHmd_SetEnabledCaps = (PFUNC_HMD_UINT)GetProcAddress(dll, "ovrHmd_SetEnabledCaps"));
	b = b && (ovrHmd_ResetFrameTiming = (PFUNC_HMD_UINT)GetProcAddress(dll, "ovrHmd_ResetFrameTiming"));
	b = b && (ovrHmd_ConfigureTracking_Real = (PFUNC_HMD_INT_INT)GetProcAddress(dll, "ovrHmd_ConfigureTracking"));
	b = b && (ovrHmd_GetTrackingState = (PFUNC_TRACKING_STATE)GetProcAddress(dll, "ovrHmd_GetTrackingState"));
	b = b && (ovrHmd_GetFovTextureSize = (PFUNC_FOV)GetProcAddress(dll, "ovrHmd_GetFovTextureSize"));
	b = b && (ovrHmd_ConfigureRendering = (PFUNC_CONFIG)GetProcAddress(dll, "ovrHmd_ConfigureRendering"));
	b = b && (ovrHmd_BeginFrame = (PFUNC_BEGIN)GetProcAddress(dll, "ovrHmd_BeginFrame"));
	b = b && (ovrHmd_EndFrame = (PFUNC_END)GetProcAddress(dll, "ovrHmd_EndFrame"));
	b = b && (ovrHmd_GetEyePoses = (PFUNC_EYEPOSES)GetProcAddress(dll, "ovrHmd_GetEyePoses"));
	b = b && (ovrHmd_GetHmdPosePerEye = (PFUNC_EYEPOSE)GetProcAddress(dll, "ovrHmd_GetHmdPosePerEye"));
	b = b && (ovrHmd_GetRenderDesc = (PFUNC_RENDERDESC)GetProcAddress(dll, "ovrHmd_GetRenderDesc"));
	b = b && (ovrHmd_CreateDistortionMesh = (PFUNC_DISTORTIONMESH)GetProcAddress(dll, "ovrHmd_CreateDistortionMesh"));
	b = b && (ovrHmd_CreateDistortionMeshDebug = (PFUNC_DISTORTIONMESHDEBUG)GetProcAddress(dll, "ovrHmd_CreateDistortionMeshDebug"));
	b = b && (ovrHmd_DestroyDistortionMesh = (PFUNC_DESTROYMESH)GetProcAddress(dll, "ovrHmd_DestroyDistortionMesh"));
	b = b && (ovrHmd_GetRenderScaleAndOffset = (PFUNC_SCALEOFFSET)GetProcAddress(dll, "ovrHmd_GetRenderScaleAndOffset"));
	b = b && (ovrHmd_BeginFrameTiming = (PFUNC_BEGIN)GetProcAddress(dll, "ovrHmd_BeginFrameTiming"));
	b = b && (ovrHmd_GetFrameTiming = (PFUNC_BEGIN)GetProcAddress(dll, "ovrHmd_GetFrameTiming"));
	b = b && (ovrHmd_GetEyeTimewarpMatrices = (PFUNC_TIMEWARP)GetProcAddress(dll, "ovrHmd_GetEyeTimewarpMatrices"));
	b = b && (ovrHmd_GetEyeTimewarpMatricesDebug = (PFUNC_TIMEWARPDEBUG)GetProcAddress(dll, "ovrHmd_GetEyeTimewarpMatricesDebug"));
	b = b && (ovr_GetTimeInSeconds = (PFUNC_DOUBLE)GetProcAddress(dll, "ovr_GetTimeInSeconds"));
	b = b && (ovrHmd_GetHSWDisplayState = (PFUNC_HMD_HSW)GetProcAddress(dll, "ovrHmd_GetHSWDisplayState"));
	b = b && (ovrHmd_DismissHSWDisplay_Real = (PFUNC_CHAR_HMD)GetProcAddress(dll, "ovrHmd_DismissHSWDisplay"));
	b = b && (ovrHmd_ProcessLatencyTest = (PFUNC_LATENCY)GetProcAddress(dll, "ovrHmd_ProcessLatencyTest"));
	b = b && (ovrHmd_GetLatencyTest2DrawColor = (PFUNC_LATENCY)GetProcAddress(dll, "ovrHmd_GetLatencyTest2DrawColor"));
	b = b && (ovr_WaitTillTime = (PFUNC_DOUBLE_DOUBLE)GetProcAddress(dll, "ovr_WaitTillTime"));
	b = b && (ovrMatrix4f_OrthoSubProjection = (PFUNC_ORTHO)GetProcAddress(dll, "ovrMatrix4f_OrthoSubProjection"));
	b = b && (ovrMatrix4f_Projection = (PFUNC_PROJECTION)GetProcAddress(dll, "ovrMatrix4f_Projection"));
	b = b && (ovrTimewarpProjectionDesc_FromProjection = (PFUNC_TIMEWARPPROJ)GetProcAddress(dll, "ovrTimewarpProjectionDesc_FromProjection"));
	b = b && (ovrHmd_GetBool = (PFUNC_GETBOOL)GetProcAddress(dll, "ovrHmd_GetBool"));
	b = b && (ovrHmd_GetFloat = (PFUNC_GETFLOAT)GetProcAddress(dll, "ovrHmd_GetFloat"));
	b = b && (ovrHmd_GetFloatArray = (PFUNC_GETFLOATARRAY)GetProcAddress(dll, "ovrHmd_GetFloatArray"));
	b = b && (ovrHmd_GetInt = (PFUNC_GETINT)GetProcAddress(dll, "ovrHmd_GetInt"));
	b = b && (ovrHmd_GetString = (PFUNC_GETSTRING)GetProcAddress(dll, "ovrHmd_GetString"));
	b = b && (ovrHmd_SetBool = (PFUNC_SETBOOL)GetProcAddress(dll, "ovrHmd_SetBool"));
	b = b && (ovrHmd_SetFloat = (PFUNC_SETFLOAT)GetProcAddress(dll, "ovrHmd_SetFloat"));
	b = b && (ovrHmd_SetFloatArray = (PFUNC_SETFLOATARRAY)GetProcAddress(dll, "ovrHmd_SetFloatArray"));
	b = b && (ovrHmd_SetInt = (PFUNC_SETINT)GetProcAddress(dll, "ovrHmd_SetInt"));
	b = b && (ovrHmd_SetString = (PFUNC_SETSTRING)GetProcAddress(dll, "ovrHmd_SetString"));
	b = b && (ovr_TraceMessage = (PFUNC_TRACE)GetProcAddress(dll, "ovr_TraceMessage"));
	if (!b)
		FreeLibrary(dll);
	else
		g_libovr_version = libovr_050;
	dll_loaded = b;
	return b;
}

#endif

static bool LoadOculusDLL()
{
	if (dll_loaded)
		return true;
	assert(sizeof(int)==4);
#ifdef _WIN32
	if (Load7() || Load6() || Load5())
	{
		return true;
	}
	else
#endif
	{
		g_libovr_version = libovr_none;
		return false;
	}
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
	else if (g_libovr_version >= libovr_070)
		return true; // 0.7.0.0 DLL doesn't contain these functions
	else if (g_libovr_version >= libovr_060)
		return ovr_InitializeRenderingShimVersion(0); // 0.6.0.0 or 0.6.0.1 are both minor version 0
	else
		return ovr_InitializeRenderingShim_Real() != 0;
}

bool ovr_InitializeRenderingShimVersion(int MinorVersion)
{
	if (!LoadOculusDLL())
		return false;
	else if (g_libovr_version >= libovr_070)
		return true; // 0.7.0.0 DLL doesn't contain these functions
	else
		return ovr_InitializeRenderingShimVersion_Real(MinorVersion) != 0;
}

ovrResult ovr_Initialize(void *initParams)
{
	if (!LoadOculusDLL())
		return ovrError_LibLoad;
	return CONV_RESULT(ovr_Initialize_Real(initParams), ovrError_Initialize);
}

const char *ovr_GetVersionString(void)
{
	if (!dll_loaded)
		return "0.0.0.0";
	return ovr_GetVersionString_Real();
}

int ovrHmd_Detect()
{
	if (!dll_loaded)
		return ovrError_NotInitialized;
	else if (g_libovr_version >= libovr_070)
	{
		ovrHmdDesc7 desc = ovr_GetHmdDesc_Real(nullptr);
		if (desc.Type)
			return 1;
		else
			return 0;
	}
	else
	{
		return ovrHmd_Detect_Real();
	}
}

ovrHmd ovrHmd_Create(int number)
{
	if (!dll_loaded)
		return nullptr;
	if (g_libovr_version == libovr_050)
	{
		g_libovr_luid = nullptr;
		return ovrHmd_Create_Real(number);
	}
	else if (g_libovr_version == libovr_060)
	{
		g_libovr_luid = nullptr;
		ovrHmd hmd;
		if (OVR_FAILURE(ovrHmd_Create_Real_6(number, &hmd)))
			hmd = nullptr;
		return hmd;
	}
	else if (g_libovr_version == libovr_070)
	{
		g_libovr_luid = &s_libovr_luid;
		ovrHmd hmd;
		if (OVR_FAILURE(ovr_Create_Real(&hmd, g_libovr_luid)))
		{
			hmd = nullptr;
			g_libovr_luid = nullptr;
		}
		return hmd;
	}
	else
	{
		return nullptr;
	}
}

ovrResult ovrHmd_Create(int number, ovrHmd *hmd_pointer)
{
	if (!dll_loaded)
		return ovrError_NotInitialized;
	if (g_libovr_version == libovr_050)
	{
		g_libovr_luid = nullptr;
		*hmd_pointer = ovrHmd_Create_Real(number);
		if (*hmd_pointer)
			return ovrSuccess;
		else
			return ovrError_NoHmd;
	}
	else if (g_libovr_version == libovr_060)
	{
		g_libovr_luid = nullptr;
		return ovrHmd_Create_Real_6(number, hmd_pointer);
	}
	else if (g_libovr_version == libovr_070)
	{
		g_libovr_luid = &s_libovr_luid;
		return ovr_Create_Real(hmd_pointer, g_libovr_luid);
	}
	else
	{
		g_libovr_luid = nullptr;
		*hmd_pointer = nullptr;
		return ovrError_LibVersion;
	}
}

ovrResult ovr_Create(ovrHmd *hmd_pointer, ovrGraphicsLuid *LUID_pointer)
{
	if (!dll_loaded)
		return ovrError_NotInitialized;
	if (g_libovr_version == libovr_050)
	{
		g_libovr_luid = nullptr;
		memset(LUID_pointer, 0, sizeof(*LUID_pointer));
		*hmd_pointer = ovrHmd_Create_Real(0);
		if (*hmd_pointer)
			return ovrSuccess;
		else
			return ovrError_NoHmd;
	}
	else if (g_libovr_version == libovr_060)
	{
		g_libovr_luid = nullptr;
		memset(LUID_pointer, 0, sizeof(*LUID_pointer));
		return ovrHmd_Create_Real_6(0, hmd_pointer);
	}
	else if (g_libovr_version == libovr_070)
	{
		g_libovr_luid = LUID_pointer;
		return ovr_Create_Real(hmd_pointer, LUID_pointer);
	}
	else
	{
		g_libovr_luid = nullptr;
		memset(LUID_pointer, 0, sizeof(*LUID_pointer));
		*hmd_pointer = nullptr;
		return ovrError_LibVersion;
	}
}

ovrHmd ovrHmd_CreateDebug(int version_ovrHmd)
{
	if (!dll_loaded)
		return nullptr;
	return ovrHmd_CreateDebug_Real(version_ovrHmd);
}

ovrResult ovrHmd_CreateDebug(int version_ovrHmd, ovrHmd *hmd_pointer)
{
	if (!dll_loaded)
		return ovrError_NotInitialized;
	if (g_libovr_version == libovr_050)
	{
		g_libovr_luid = nullptr;
		*hmd_pointer = ovrHmd_CreateDebug_Real(version_ovrHmd);
		if (*hmd_pointer)
			return ovrSuccess;
		else
			return ovrError_NoHmd;
	}
	else if (g_libovr_version == libovr_060)
	{
		g_libovr_luid = nullptr;
		return ovrHmd_CreateDebug_Real_6(version_ovrHmd, hmd_pointer);
	}
	else if (g_libovr_version == libovr_070)
	{
		g_libovr_luid = &s_libovr_luid;
		// can't create debug in this SDK version
		return ovr_Create_Real(hmd_pointer, g_libovr_luid);
	}
	else
	{
		g_libovr_luid = nullptr;
		*hmd_pointer = nullptr;
		return ovrError_LibVersion;
	}
}

void ovr_Shutdown()
{
	if (!dll_loaded)
		return;
	if (ovr_Shutdown_Real)
		ovr_Shutdown_Real();
	//FreeOculusDLL();
}

ovrHmdDescComplete ovr_GetHmdDesc(ovrHmd hmd)
{
	ovrHmdDescComplete result = { 0 };
	if (!dll_loaded)
		return result;
 	else if (ovr_GetHmdDesc_Real && g_libovr_version == libovr_070)
	{
		ovrHmdDesc7 desc = ovr_GetHmdDesc_Real(hmd);
		result.AvailableHmdCaps = desc.AvailableHmdCaps;
		result.AvailableTrackingCaps = desc.AvailableTrackingCaps;
		result.CameraFrustumFarZInMeters = desc.CameraFrustumFarZInMeters;
		result.CameraFrustumHFovInRadians = desc.CameraFrustumHFovInRadians;
		result.CameraFrustumNearZInMeters = desc.CameraFrustumNearZInMeters;
		result.CameraFrustumVFovInRadians = desc.CameraFrustumVFovInRadians;
		result.DefaultEyeFov[0] = desc.DefaultEyeFov[0];
		result.DefaultEyeFov[1] = desc.DefaultEyeFov[1];
		result.DefaultHmdCaps = desc.DefaultHmdCaps;
		result.DefaultTrackingCaps = desc.DefaultTrackingCaps;
		result.DisplayRefreshRate = desc.DisplayRefreshRate;
		result.DistortionCaps = ovrDistortionCap_Chromatic | ovrDistortionCap_TimeWarp | ovrDistortionCap_Vignette | ovrDistortionCap_SRGB;
		result.EyeRenderOrder[0] = ovrEye_Left;
		result.EyeRenderOrder[1] = ovrEye_Right;
		result.FirmwareMajor = desc.FirmwareMajor;
		result.FirmwareMinor = desc.FirmwareMinor;
		result.HmdCaps = desc.DefaultHmdCaps;
		memcpy(result.Manufacturer, desc.Manufacturer, sizeof(result.Manufacturer));
		result.MaxEyeFov[0] = desc.MaxEyeFov[0];
		result.MaxEyeFov[1] = desc.MaxEyeFov[1];
		result.ProductId = desc.ProductId;
		memcpy(result.ProductName, desc.Manufacturer, sizeof(result.ProductName));
		result.Resolution = desc.Resolution;
		memcpy(result.SerialNumber, desc.SerialNumber, sizeof(result.SerialNumber));
		result.TrackingCaps = desc.DefaultTrackingCaps;
		result.Type = desc.Type;
		result.VendorId = desc.VendorId;
		// todo: result.WindowPos and similar values
	}
	else if (g_libovr_version == libovr_060)
	{
		if (hmd == nullptr)
			return result;
		ovrHmdDesc6 desc = *(ovrHmdDesc6 *)hmd;
		result.internal = desc.internal;
		result.AvailableHmdCaps = desc.HmdCaps | ovrHmdCap_Writable_Mask;
		result.AvailableTrackingCaps = desc.TrackingCaps | ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position | ovrTrackingCap_Idle;
		result.CameraFrustumFarZInMeters = desc.CameraFrustumFarZInMeters;
		result.CameraFrustumHFovInRadians = desc.CameraFrustumHFovInRadians;
		result.CameraFrustumNearZInMeters = desc.CameraFrustumNearZInMeters;
		result.CameraFrustumVFovInRadians = desc.CameraFrustumVFovInRadians;
		result.DefaultEyeFov[0] = desc.DefaultEyeFov[0];
		result.DefaultEyeFov[1] = desc.DefaultEyeFov[1];
		result.DefaultHmdCaps = desc.HmdCaps;
		result.DefaultTrackingCaps = desc.TrackingCaps;
		switch (desc.Type)
		{
		case ovrHmd_CrystalCoveProto:
		case ovrHmd_DK2:
		case ovrHmd_BlackStar:
				result.DisplayRefreshRate = 75;
				break;
		case ovrHmd_CB:
		case ovrHmd_E3_2015:
		case ovrHmd_ES06:
		case ovrHmd_Other:
			result.DisplayRefreshRate = 90;
		default:
				result.DisplayRefreshRate = 60;
		}
		result.DistortionCaps = ovrDistortionCap_Chromatic | ovrDistortionCap_TimeWarp | ovrDistortionCap_Vignette | ovrDistortionCap_SRGB;
		result.EyeRenderOrder[0] = desc.EyeRenderOrder[0];
		result.EyeRenderOrder[1] = desc.EyeRenderOrder[1];
		result.FirmwareMajor = desc.FirmwareMajor;
		result.FirmwareMinor = desc.FirmwareMinor;
		result.HmdCaps = desc.HmdCaps;
		memcpy(result.Manufacturer, desc.Manufacturer, sizeof(result.Manufacturer));
		result.MaxEyeFov[0] = desc.MaxEyeFov[0];
		result.MaxEyeFov[1] = desc.MaxEyeFov[1];
		result.ProductId = desc.ProductId;
		memcpy(result.ProductName, desc.Manufacturer, sizeof(result.ProductName));
		result.Resolution = desc.Resolution;
		memcpy(result.SerialNumber, desc.SerialNumber, sizeof(result.SerialNumber));
		result.TrackingCaps = desc.TrackingCaps;
		result.Type = desc.Type;
		result.VendorId = desc.VendorId;
		// todo: result.WindowPos and similar values
	}
	else if (g_libovr_version == libovr_050)
	{
		if (hmd == nullptr)
			return result;
		ovrHmdDesc5 desc = *(ovrHmdDesc5 *)hmd;
		result.internal = desc.internal;
		result.AvailableHmdCaps = desc.HmdCaps | ovrHmdCap_Writable_Mask;
		result.AvailableTrackingCaps = desc.TrackingCaps | ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position | ovrTrackingCap_Idle;
		result.CameraFrustumFarZInMeters = desc.CameraFrustumFarZInMeters;
		result.CameraFrustumHFovInRadians = desc.CameraFrustumHFovInRadians;
		result.CameraFrustumNearZInMeters = desc.CameraFrustumNearZInMeters;
		result.CameraFrustumVFovInRadians = desc.CameraFrustumVFovInRadians;
		result.DefaultEyeFov[0] = desc.DefaultEyeFov[0];
		result.DefaultEyeFov[1] = desc.DefaultEyeFov[1];
		result.DefaultHmdCaps = desc.HmdCaps;
		result.DefaultTrackingCaps = desc.TrackingCaps;
		switch (desc.Type)
		{
		case ovrHmd_CrystalCoveProto:
		case ovrHmd_DK2:
		case ovrHmd_BlackStar:
			result.DisplayRefreshRate = 75;
			break;
		case ovrHmd_CB:
		case ovrHmd_E3_2015:
		case ovrHmd_ES06:
		case ovrHmd_Other:
			result.DisplayRefreshRate = 90;
		default:
			result.DisplayRefreshRate = 60;
		}
		result.DistortionCaps = desc.DistortionCaps;
		result.EyeRenderOrder[0] = desc.EyeRenderOrder[0];
		result.EyeRenderOrder[1] = desc.EyeRenderOrder[1];
		result.FirmwareMajor = desc.FirmwareMajor;
		result.FirmwareMinor = desc.FirmwareMinor;
		result.HmdCaps = desc.HmdCaps;
		memcpy(result.Manufacturer, desc.Manufacturer, sizeof(result.Manufacturer));
		result.MaxEyeFov[0] = desc.MaxEyeFov[0];
		result.MaxEyeFov[1] = desc.MaxEyeFov[1];
		result.ProductId = desc.ProductId;
		memcpy(result.ProductName, desc.Manufacturer, sizeof(result.ProductName));
		result.Resolution = desc.Resolution;
		memcpy(result.SerialNumber, desc.SerialNumber, sizeof(result.SerialNumber));
		result.TrackingCaps = desc.TrackingCaps;
		result.Type = desc.Type;
		result.VendorId = desc.VendorId;
		result.WindowsPos = desc.WindowsPos;
		result.DisplayId = desc.DisplayId;
		if (desc.DisplayDeviceName)
			strcpy(result.DisplayDeviceName, desc.DisplayDeviceName);
	}
	return result;
}

ovrResult ovrHmd_ConfigureTracking(ovrHmd hmd, unsigned supported_ovrTrackingCap, unsigned required_ovrTrackingCap)
{
	if (!dll_loaded)
		return ovrError_NotInitialized;
	ovrResult result = ovrHmd_ConfigureTracking_Real(hmd, supported_ovrTrackingCap, required_ovrTrackingCap);
	return CONV_RESULT(result, ovrError_GeneralTrackerFailure);
}

ovrResult ovrHmd_DismissHSWDisplay(ovrHmd hmd)
{
	if (!dll_loaded)
		return ovrError_NotInitialized;
	if (g_libovr_version == 050)
		return ovrHmd_DismissHSWDisplay_Real(hmd) ? ovrSuccess : ovrError_Timeout;
	else
		return ovrSuccess;
}
