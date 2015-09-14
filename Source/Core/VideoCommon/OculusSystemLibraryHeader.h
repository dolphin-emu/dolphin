// This is a header file for linking to the Oculus Rift system libraries to build for the Oculus Rift platform.
// Moral rights Carl Kenner
// Public Domain

#pragma once
#include <stdint.h>

#define OCULUSSYSTEMLIBRARYHEADER

#define ALIGN_TO_FOUR_BYTE_BOUNDARY __declspec(align(4))
#define ALIGN_TO_EIGHT_BYTE_BOUNDARY __declspec(align(8))
#ifdef _WIN64
#define ALIGN_TO_POINTER_BOUNDARY __declspec(align(8))
#else
#define ALIGN_TO_POINTER_BOUNDARY __declspec(align(4))
#endif

enum TLibOvrVersion
{
	libovr_none = 0,
	libovr_013,
	libovr_014,
	libovr_015,
	libovr_021,
	libovr_022,
	libovr_023,
	libovr_024,
	libovr_025,
	libovr_031,
	libovr_032,
	libovr_033,
	libovr_040,
	libovr_041,
	libovr_042,
	libovr_043,
	libovr_044,
	libovr_050,
	libovr_060,
	libovr_070,
};

extern TLibOvrVersion g_libovr_version;

#if !defined(OVR_MAJOR_VERSION)
#define ovrHmd_None             0
#define ovrHmd_DK1              3
#define ovrHmd_DKHD             4
#endif
#if !defined(OVR_MAJOR_VERSION) || (OVR_MAJOR_VERSION >= 5)
#define ovrHmd_CrystalCoveProto 5
#endif
#if !defined(OVR_MAJOR_VERSION) || (OVR_MAJOR_VERSION == 0 && OVR_MINOR_VERSION <= 3)
#define ovrHmd_DK2              6
#endif
#if !defined(OVR_MAJOR_VERSION) || OVR_MAJOR_VERSION != 5
#define ovrHmd_BlackStar        7
#endif
#if !defined(OVR_MAJOR_VERSION) || OVR_MAJOR_VERSION <= 4
#define ovrHmd_CB               8
#endif
#if !defined(OVR_MAJOR_VERSION)
#define ovrHmd_Other            9
#endif
#if !defined(OVR_MAJOR_VERSION) || OVR_MAJOR_VERSION <= 6
#define ovrHmd_E3_2015         10
#define ovrHmd_ES06            11
#endif

#if !defined(OVR_MAJOR_VERSION)
#define ovrEye_Left  0
#define ovrEye_Right 1
#define ovrEye_Count 2
#endif

#if !defined(OVR_MAJOR_VERSION) || OVR_MAJOR_VERSION <= 6
#define ovrHand_Left  0
#define ovrHand_Right 1
#endif

// only ovrHmdCap_DebugDevice is supported on SDK 0.7
#define ovrHmdCap_Present           0x0001
#define ovrHmdCap_Available         0x0002
#define ovrHmdCap_Captured          0x0004
#define ovrHmdCap_ExtendDesktop     0x0008
#define ovrHmdCap_DebugDevice       0x0010
#define ovrHmdCap_DisplayOff        0x0040
#define ovrHmdCap_LowPersistence    0x0080
#define ovrHmdCap_DynamicPrediction 0x0200
#define ovrHmdCap_NoVSync           0x1000
#define ovrHmdCap_NoMirrorToWindow  0x2000
#define ovrHmdCap_Service_Mask      0x22C0
#define ovrHmdCap_Writable_Mask     0x32C0

#define ovrTrackingCap_Orientation      0x010
#define ovrTrackingCap_MagYawCorrection 0x020
#define ovrTrackingCap_Position         0x040
#define ovrTrackingCap_Idle             0x100

#define ovrStatus_OrientationTracked 0x0001
#define ovrStatus_PositionTracked    0x0002
#define ovrStatus_CameraPoseTracked  0x0004
#define ovrStatus_PositionConnected  0x0020
#define ovrStatus_HmdConnected       0x0080

#define ovrDistortionCap_Chromatic          0x00001
#define ovrDistortionCap_TimeWarp           0x00002
#define ovrDistortionCap_Vignette           0x00008
#define ovrDistortionCap_NoRestore          0x00010
#define ovrDistortionCap_FlipInput          0x00020
#define ovrDistortionCap_SRGB               0x00040
#define ovrDistortionCap_Overdrive          0x00080
#define ovrDistortionCap_HqDistortion       0x00100
#define ovrDistortionCap_LinuxDevFullscreen 0x00200
#define ovrDistortionCap_ComputeShader      0x00400
#define ovrDistortionCap_NoTimewarpJit      0x00800
#define ovrDistortionCap_TimewarpJitDelay   0x01000
#define ovrDistortionCap_ProfileNoSpinWaits 0x10000

#define ovrLogLevel_Debug 0
#define ovrLogLevel_Info  1
#define ovrLogLevel_Error 2

#define ovrInit_Debug          1
#define ovrInit_ServerOptional 2
#define ovrInit_RequestVersion 4
#define ovrInit_ForceNoDebug   8

#define ovrFalse 0
#define ovrTrue  1

#define ovrRenderAPI_None         0
#define ovrRenderAPI_OpenGL       1
#define ovrRenderAPI_Android_GLES 2
#define ovrRenderAPI_D3D9         3
#define ovrRenderAPI_D3D10        4
#define ovrRenderAPI_D3D11        5
#define ovrRenderAPI_Count        6

#define ovrProjection_None              0
#define ovrProjection_RightHanded       1
#define ovrProjection_FarLessThanNear   2
#define ovrProjection_FarClipAtInfinity 4
#define ovrProjection_ClipRangeOpenGL   8

#define ovrSuccess                               0
#define ovrSuccess_NotVisible                 1000
#define ovrSuccess_HMDFirmwareMismatch        4100
#define ovrSuccess_TrackerFirmwareMismatch    4101
#define ovrSuccess_ControllerFirmwareMismatch 4104
#define ovrError_MemoryAllocationFailure     -1000
#define ovrError_SocketCreationFailure       -1001
#define ovrError_InvalidHmd                  -1002
#define ovrError_Timeout                     -1003
#define ovrError_NotInitialized              -1004
#define ovrError_InvalidParameter            -1005
#define ovrError_ServiceError                -1006
#define ovrError_NoHmd                       -1007
#define ovrError_Initialize                  -3000
#define ovrError_LibLoad                     -3001
#define ovrError_LibVersion                  -3002
#define ovrError_ServiceConnection           -3003
#define ovrError_ServiceVersion              -3004
#define ovrError_IncompatibleOS              -3005
#define ovrError_DisplayInit                 -3006
#define ovrError_ServerStart                 -3007
#define ovrError_Reinitialization            -3008
#define ovrError_MismatchedAdapters          -3009
#define ovrError_LeakingResources            -3010
#define ovrError_ClientVersion               -3011
#define ovrError_InvalidBundleAdjustment     -4000
#define ovrError_USBBandwidth                -4001
#define ovrError_USBEnumeratedSpeed          -4002
#define ovrError_ImageSensorCommError        -4003
#define ovrError_GeneralTrackerFailure       -4004
#define ovrError_ExcessiveFrameTruncation    -4005
#define ovrError_ExcessiveFrameSkipping      -4006
#define ovrError_SyncDisconnected            -4007
#define ovrError_TrackerMemoryReadFailure    -4008
#define ovrError_TrackerMemoryWriteFailure   -4009
#define ovrError_TrackerFrameTimeout         -4010
#define ovrError_TrackerTruncatedFrame       -4011
#define ovrError_HMDFirmwareMismatch         -4100
#define ovrError_TrackerFirmwareMismatch     -4101
#define ovrError_BootloaderDeviceDetected    -4102
#define ovrError_TrackerCalibrationError     -4103
#define ovrError_ControllerFirmwareMismatch  -4104
#define ovrError_Incomplete                  -5000
#define ovrError_Abandoned                   -5001
#define ovrError_DisplayLost                 -6000

#define ovrButton_A             0x00000001
#define ovrTouch_A              ovrButton_A
#define ovrButton_B             0x00000002
#define ovrTouch_B              ovrButton_B
#define ovrButton_RThumb        0x00000004
#define ovrTouch_RThumb         ovrButton_RThumb
#define ovrButton_RShoulder     0x00000008
#define ovrTouch_RIndexTrigger  0x00000010
#define ovrTouch_RIndexPointing 0x00000020
#define ovrTouch_RThumbUp       0x00000040
#define ovrButton_X             0x00000100
#define ovrTouch_X              ovrButton_X
#define ovrButton_Y             0x00000200
#define ovrTouch_Y              ovrButton_Y
#define ovrButton_LThumb        0x00000400
#define ovrTouch_LThumb         ovrButton_LThumb
#define ovrButton_LShoulder     0x00000800
#define ovrTouch_LIndexTrigger  0x00001000
#define ovrTouch_LIndexPointing 0x00002000
#define ovrTouch_LThumbUp       0x00004000
#define ovrButton_Up            0x00010000
#define ovrButton_Down          0x00020000
#define ovrButton_Left          0x00040000
#define ovrButton_Right         0x00080000
#define ovrButton_Enter         0x00100000
#define ovrButton_Back          0x00200000

#define ovrControllerType_LTouch   0x01
#define ovrControllerType_RTouch   0x02
#define ovrControllerType_Touch    0x03
#define ovrControllerType_All      0xff

#define ovrLayerType_Disabled       0
#define ovrLayerType_EyeFov         1
#define ovrLayerType_EyeFovDepth    2
#define ovrLayerType_QuadInWorld    3
#define ovrLayerType_QuadHeadLocked 4
#define ovrLayerType_Direct         6

#define ovrLayerFlag_HighQuality               1
#define ovrLayerFlag_TextureOriginAtBottomLeft 2

#define ovrSwapTextureSetD3D11_Typeless 1

#define OVR_SUCCESS(x) (x>=0)
#define OVR_FAILURE(x) (x<0)

typedef void(__cdecl *ovrLogCallback5)(int level, const char *message);
typedef ovrLogCallback5 ovrLogCallback6;
typedef void(__cdecl *ovrLogCallback7)(uintptr_t userData, int level, const char* message);
typedef void *ovrLogCallback;

typedef int ovrResult;
typedef float ovrScalar;
typedef struct { int w, h; } ovrSizei;
typedef struct { int x, y; } ovrVector2i;
typedef struct { ovrVector2i Pos; ovrSizei Size; } ovrRecti;
typedef struct { ovrScalar x, y; } ovrVector2f;
typedef struct { ovrScalar x, y, z; } ovrVector3f;
typedef struct { ovrScalar x, y, z, w; } ovrQuatf;

typedef struct { ovrScalar UpTan, DownTan, LeftTan, RightTan; } ovrFovPort;

typedef struct
{
	void *internal;
	int Type;
	const char *ProductName, *Manufacturer;
	short VendorId, ProductId;
	char SerialNumber[24];
	short FirmwareMajor, FirmwareMinor;
	ovrScalar CameraFrustumHFovInRadians, CameraFrustumVFovInRadians, CameraFrustumNearZInMeters, CameraFrustumFarZInMeters;
	unsigned int HmdCaps, TrackingCaps, DistortionCaps;
	ovrFovPort DefaultEyeFov[2], MaxEyeFov[2];
	int EyeRenderOrder[2];
	ovrSizei Resolution;
	ovrVector2i WindowsPos;
	char *DisplayDeviceName;
	int DisplayId;
} ovrHmdDesc5, *ovrHmd;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	void *internal;
	int Type;
#ifdef _WIN64
	int padding;
#endif
	const char *ProductName, *Manufacturer;
	short VendorId, ProductId;
	char SerialNumber[24];
	short FirmwareMajor, FirmwareMinor;
	ovrScalar CameraFrustumHFovInRadians, CameraFrustumVFovInRadians, CameraFrustumNearZInMeters, CameraFrustumFarZInMeters;
	unsigned int HmdCaps, TrackingCaps;
	ovrFovPort DefaultEyeFov[2], MaxEyeFov[2];
	int EyeRenderOrder[2];
	ovrSizei Resolution;
} ovrHmdDesc6;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	int Type;
#ifdef _WIN64
	int padding;
#endif
	char ProductName[64], Manufacturer[64];
 	short VendorId, ProductId;
	char SerialNumber[24];
	short FirmwareMajor, FirmwareMinor;
	ovrScalar CameraFrustumHFovInRadians, CameraFrustumVFovInRadians, CameraFrustumNearZInMeters, CameraFrustumFarZInMeters;
	unsigned int AvailableHmdCaps, DefaultHmdCaps, AvailableTrackingCaps, DefaultTrackingCaps;
	ovrFovPort DefaultEyeFov[2], MaxEyeFov[2];
	ovrSizei Resolution;
	ovrScalar DisplayRefreshRate;
#ifdef _WIN64
	char padding2[4];
#endif
} ovrHmdDesc7;

typedef struct
{
	void *internal;
	int Type;
	char ProductName[64], Manufacturer[64];
	short VendorId, ProductId;
	char SerialNumber[24];
	short FirmwareMajor, FirmwareMinor;
	ovrScalar CameraFrustumHFovInRadians, CameraFrustumVFovInRadians, CameraFrustumNearZInMeters, CameraFrustumFarZInMeters;
	unsigned int AvailableHmdCaps, DefaultHmdCaps, AvailableTrackingCaps, DefaultTrackingCaps, HmdCaps, TrackingCaps, DistortionCaps;
	ovrFovPort DefaultEyeFov[2], MaxEyeFov[2];
	int EyeRenderOrder[2];
	ovrSizei Resolution;
	ovrScalar DisplayRefreshRate;
	ovrVector2i WindowsPos;
	char DisplayDeviceName[128];
	int DisplayId;
} ovrHmdDesc, ovrHmdDescComplete;

typedef struct {
	int Eye;
	ovrFovPort Fov;
	ovrRecti DistortedViewport;
	ovrVector2f PixelsPerTanAngleAtCenter;
	ovrVector3f HmdToEyeViewOffset;
} ovrEyeRenderDesc;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	ovrScalar DeltaSeconds, unused;
	double ThisFrameSeconds, TimewarpPointSeconds, NextFrameSeconds, ScanoutMidpointSeconds, EyeScanoutSeconds[2];
} ovrFrameTiming5;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	double DisplayMidpointSeconds, FrameIntervalSeconds;
	unsigned AppFrameIndex, DisplayFrameIndex;
} ovrFrameTiming6;

typedef struct { ovrQuatf Orientation; ovrVector3f Position; } ovrPosef;

typedef struct { ovrScalar M[4][4]; } ovrMatrix4f;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	char Displayed, unused[7];
	double StartTime, DismissibleTime;
} ovrHSWDisplayState;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	unsigned Flags, RequestedMinorVersion;
	ovrLogCallback LogCallback;
	unsigned ConnectionTimeoutMS;
	unsigned padding[3];
} ovrInitParams, ovrInitParams5, ovrInitParams6;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	unsigned Flags, RequestedMinorVersion;
	ovrLogCallback7 LogCallback;
	uintptr_t UserData;
	unsigned ConnectionTimeoutMS;
	unsigned padding[1];
} ovrInitParams7;

typedef struct
{
	ovrResult Result;
	char ErrorString[512];
} ovrErrorInfo;

typedef struct
{
	int API;
	union
	{
		ovrSizei BackBufferSize;
		ovrSizei RTSize;
	};
	int Multisample;
} ovrRenderAPIConfigHeader;

typedef struct
{
	ovrRenderAPIConfigHeader Header;
	void *PlatformData[8];
} ovrRenderAPIConfig;

typedef struct
{
	int API;
	ovrSizei TextureSize;
	ovrRecti RenderViewport;
} ovrTextureHeader5;

typedef struct
{
	ovrTextureHeader5 Header;
	void *PlatformData[8];
} ovrTexture5;

typedef struct
{
	int API;
	ovrSizei TextureSize;
} ovrTextureHeader6, ovrTextureHeader7;

typedef struct
{
	ovrTextureHeader6 Header;
#ifdef _WIN64
	unsigned padding;
#endif
	void *PlatformData[8];
} ovrTexture6, ovrTexture7;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	ovrTexture6 *Textures;
	int TextureCount, CurrentIndex;
} ovrSwapTextureSet;

typedef struct
{
	ovrVector3f Accelerometer, Gyro, Magnetometer;
	ovrScalar Temperature, TimeInSeconds;
} ovrSensorData;

// unchanged from 0.5 to 0.7
typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	ovrPosef ThePose;
	ovrVector3f AngularVelocity, LinearVelocity, AngularAcceleration, LinearAcceleration;
	ovrScalar unused;
	double TimeInSeconds;
} ovrPoseStatef;

typedef struct
{
	ovrPoseStatef HeadPose;
	ovrPosef CameraPose, LeveledCameraPose;
	ovrSensorData RawSensorData;
	unsigned StatusFlags, LastCameraFrameCounter, unusued;
} ovrTrackingState;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	ovrPoseStatef HeadPose;
	ovrPosef CameraPose, LeveledCameraPose;
	ovrPoseStatef HandPoses[2];
	ovrSensorData RawSensorData;
	unsigned StatusFlags, LastCameraFrameCounter, unusued;
} ovrTrackingState7;

typedef struct
{
	double TimeInSeconds;
	unsigned ConnectedControllerTypes, Buttons, Touches;
	ovrScalar IndexTrigger[2], HandTrigger[2];
	ovrVector2f Thumbstick[2];
} ovrInputState;

typedef struct
{
	ovrVector2f ScreenPosNDC;
	ovrScalar TimeWarpFactor, VignetteFactor;
	ovrVector2f TanEyeAnglesR, TanEyeAnglesG, TanEyeAnglesB;
} ovrDistortionVertex;

typedef struct
{
	ovrDistortionVertex *pVertexData;
	unsigned short *pIndexData;
	unsigned VertexCount, IndexCount;
} ovrDistortionMesh;

typedef struct
{
	unsigned char platform_dependent[8];
} ovrGraphicsLuid;

typedef struct ALIGN_TO_FOUR_BYTE_BOUNDARY
{
	ovrScalar Projection22, Projection23, Projection32;
} ovrTimewarpProjectionDesc;

typedef struct ALIGN_TO_FOUR_BYTE_BOUNDARY
{
	ovrVector3f HmdToEyeViewOffset[2];
	ovrScalar HmdSpaceToWorldScaleInMeters;
} ovrViewScaleDesc;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	unsigned Type, Flags;
} ovrLayerHeader;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	ovrLayerHeader Header;
	ovrSwapTextureSet *ColorTexture[2];
	ovrRecti Viewport[2];
	ovrFovPort Fov[2];
	ovrPosef RenderPose[2];
} ovrLayerEyeFov;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	ovrLayerHeader Header;
	ovrSwapTextureSet *ColorTexture[2];
	ovrRecti Viewport[2];
	ovrFovPort Fov[2];
	ovrPosef RenderPose[2];
	ovrSwapTextureSet *DepthTexture[2];
	ovrTimewarpProjectionDesc ProjectionDesc;
} ovrLayerEyeFovDepth;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	ovrLayerHeader Header;
	ovrSwapTextureSet *ColorTexture;
	ovrRecti Viewport;
	ovrPosef QuadPoseCenter;
	ovrVector2f QuadSize;
} ovrLayerQuad;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
	ovrLayerHeader Header;
	ovrSwapTextureSet *ColorTexture[2];
	ovrRecti Viewport[2];
} ovrLayerDirect;

typedef union
{
	ovrLayerHeader Header;
	ovrLayerEyeFov EyeFov;
	ovrLayerEyeFovDepth EyeFovDepth;
	ovrLayerQuad Quad;
	ovrLayerDirect Direct;
} ovrLayer_Union;

#ifdef _WIN32

typedef struct
{
	ovrRenderAPIConfigHeader Header;
	HWND Window;
	HDC DC;
} ovrGLConfigData;

#else
#ifdef __linux__

typedef struct
{
	ovrRenderAPIConfigHeader Header;
	struct _XDisplay *Disp;
} ovrGLConfigData;

#else

typedef struct
{
	ovrRenderAPIConfigHeader Header;
} ovrGLConfigData;

#endif
#endif

typedef union
{
	ovrGLConfigData OGL;
	ovrRenderAPIConfig Config;
} ovrGLConfig;

typedef struct
{
	ovrTextureHeader5 Header;
	unsigned TexId;
} ovrGLTextureData5;

typedef union
{
	ovrGLTextureData5 OGL;
	ovrTexture5 Texture;
} ovrGLTexture5;

typedef struct
{
	ovrTextureHeader6 Header;
	unsigned TexId;
} ovrGLTextureData6, ovrGLTextureData7;

typedef union
{
	ovrGLTextureData6 OGL;
	ovrTexture6 Texture;
} ovrGLTexture6, ovrGLTexture7;

typedef ovrResult(__cdecl *PFUNC_INIT)(void *InitParams);
typedef void(__cdecl *PFUNC_VOID)(void);
typedef ovrResult(__cdecl *PFUNC_CHAR)(void);
typedef ovrResult(__cdecl *PFUNC_CHAR_INT)(int);
typedef char *(__cdecl *PFUNC_PCHAR)(void);
typedef int(__cdecl *PFUNC_INT)(void);
typedef ovrHmd(__cdecl *PFUNC_HMD_INT)(int number);
typedef ovrHmd(__cdecl *PFUNC_HMD_INT32)(int version_ovrHmd);
typedef void(__cdecl *PFUNC_VOID_HMD)(ovrHmd hmd);
typedef const char *(__cdecl *PFUNC_PCHAR_HMD)(ovrHmd hmd);
typedef char(__cdecl *PFUNC_ATTACH)(ovrHmd hmd, void *hwnd, ovrRecti *dest, ovrRecti *source);
typedef unsigned(__cdecl *PFUNC_UINT_HMD)(ovrHmd hmd);
typedef void(__cdecl *PFUNC_HMD_UINT)(ovrHmd hmd, unsigned);
typedef ovrResult(__cdecl *PFUNC_HMD_INT_INT)(ovrHmd hmd, unsigned supported_ovrTrackingCap, unsigned required_ovrTrackingCap);
typedef ovrTrackingState(__cdecl *PFUNC_TRACKING_STATE)(ovrHmd hmd, double time);
typedef ovrSizei(__cdecl *PFUNC_FOV)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs, ovrScalar pixel_ratio);
typedef char(__cdecl *PFUNC_CONFIG)(ovrHmd hmd, const ovrRenderAPIConfig *address_of_cfg_dot_Config, unsigned flags_ovrDistortionCap, const ovrFovPort *tan_of_fovs_array, ovrEyeRenderDesc *result_array);
typedef ovrFrameTiming5(__cdecl *PFUNC_BEGIN)(ovrHmd hmd, unsigned frame_number);
typedef ovrFrameTiming6(__cdecl *PFUNC_FRAMETIMING)(ovrHmd hmd, unsigned frame_number);
typedef void(__cdecl *PFUNC_END)(ovrHmd hmd, const ovrPosef *render_pose_array, const ovrTexture5 *eye_texture_array);
typedef void(__cdecl *PFUNC_EYEPOSES)(ovrHmd hmd, unsigned frame_number, const ovrVector3f *eye_offset_array_result, ovrPosef *eye_pose_array_result, ovrTrackingState *tracking_state_result);
typedef ovrPosef(__cdecl *PFUNC_EYEPOSE)(ovrHmd hmd, int eye);
typedef ovrEyeRenderDesc(__cdecl *PFUNC_RENDERDESC)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs);
typedef char(__cdecl *PFUNC_DISTORTIONMESH)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs, unsigned flags_ovrDistortionCap, ovrDistortionMesh *mesh_result);
typedef char(__cdecl *PFUNC_DISTORTIONMESHDEBUG)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs, unsigned flags_ovrDistortionCap, ovrDistortionMesh *mesh_result, ovrScalar eye_relief_in_metres);
typedef void(__cdecl *PFUNC_DESTROYMESH)(ovrDistortionMesh *mesh);
typedef void(__cdecl *PFUNC_SCALEOFFSET)(ovrFovPort tan_of_fovs, ovrSizei size_of_texture, ovrRecti viewport, ovrVector2f *scale_and_offset_array_result);
typedef double(__cdecl *PFUNC_DOUBLE)(void);
typedef double(__cdecl *PFUNC_TIMEWARP)(ovrHmd hmd, int eye, ovrPosef render_pose, ovrMatrix4f *timewarp_matrices_array_result);
typedef double(__cdecl *PFUNC_TIMEWARPDEBUG)(ovrHmd hmd, int eye, ovrPosef render_pose, ovrQuatf playerTorsoMotion, ovrMatrix4f *timewarp_matrices_array_result, double seconds_of_debug_offset);
typedef void(__cdecl *PFUNC_HMD_HSW)(ovrHmd hmd, ovrHSWDisplayState *hsw_state_result);
typedef char(__cdecl *PFUNC_CHAR_HMD)(ovrHmd hmd);
typedef char(__cdecl *PFUNC_LATENCY)(ovrHmd hmd, unsigned char *colour_rgb_array);
typedef double(__cdecl *PFUNC_DOUBLE_DOUBLE)(double time);
typedef ovrMatrix4f(__cdecl *PFUNC_PROJECTION)(ovrFovPort tan_of_fovs, ovrScalar near_z, ovrScalar far_z, unsigned flags_ovrProjection);
typedef ovrMatrix4f(__cdecl *PFUNC_ORTHO)(ovrMatrix4f projection, ovrScalar ortho_scale, ovrScalar ortho_distance, ovrScalar hmd_to_eye_horizontal_offset);
typedef ovrTimewarpProjectionDesc(__cdecl *PFUNC_TIMEWARPPROJ)(ovrMatrix4f projection, unsigned projection_mod_flags);
typedef char(__cdecl *PFUNC_GETBOOL)(ovrHmd hmd, const char *property, char default);
typedef int(__cdecl *PFUNC_GETINT)(ovrHmd hmd, const char *property, int default);
typedef ovrScalar(__cdecl *PFUNC_GETFLOAT)(ovrHmd hmd, const char *property, float default);
typedef unsigned(__cdecl *PFUNC_GETFLOATARRAY)(ovrHmd hmd, const char *property, ovrScalar *result_array, unsigned result_array_size);
typedef const char *(__cdecl *PFUNC_GETSTRING)(ovrHmd hmd, const char *property, const char *default);
typedef char(__cdecl *PFUNC_SETBOOL)(ovrHmd hmd, const char *property, char new_value);
typedef char(__cdecl *PFUNC_SETINT)(ovrHmd hmd, const char *property, int new_value);
typedef char(__cdecl *PFUNC_SETFLOAT)(ovrHmd hmd, const char *property, ovrScalar new_value);
typedef char(__cdecl *PFUNC_SETFLOATARRAY)(ovrHmd hmd, const char *property, const ovrScalar *new_value_array, unsigned new_value_array_size);
typedef char(__cdecl *PFUNC_SETSTRING)(ovrHmd hmd, const char *property, const char *new_value);
typedef int(__cdecl *PFUNC_TRACE)(int ovrloglevel, const char *message);

typedef ovrResult(__cdecl *PFUNC_CREATE6)(int number, ovrHmd *hmd_pointer);
typedef ovrResult(__cdecl *PFUNC_CREATEMIRRORD3D116)(ovrHmd hmd, void *id3d11device, const void *d3d11_texture2d_desc, ovrTexture6 **mirror_texture_result_pointer);
typedef ovrResult(__cdecl *PFUNC_CREATESWAPD3D116)(ovrHmd hmd, void *id3d11device, const void *d3d11_texture2d_desc, ovrSwapTextureSet **swap_texture_set_result_pointer);

typedef ovrResult(__cdecl *PFUNC_CREATEMIRRORGL)(ovrHmd hmd, unsigned gl_colour_format, int width, int height, ovrTexture6 **mirror_texture_result_pointer);
typedef ovrResult(__cdecl *PFUNC_CREATESWAPGL)(ovrHmd hmd, unsigned gl_colour_format, int width, int height, ovrSwapTextureSet **swap_texture_set_result_pointer);
typedef ovrResult(__cdecl *PFUNC_DESTROYMIRROR)(ovrHmd hmd, ovrTexture6 *mirror_texture);
typedef ovrResult(__cdecl *PFUNC_DESTROYSWAP)(ovrHmd hmd, ovrSwapTextureSet *swap_texture_set);
typedef ovrResult(__cdecl *PFUNC_CALC)(ovrPosef head_pose, const ovrVector3f hmd_to_eye_view_offset[2], ovrPosef out_eye_poses[2]);
typedef void(__cdecl *PFUNC_ERRORINFO)(ovrErrorInfo *error_info_result);
typedef ovrResult(__cdecl *PFUNC_SUBMIT)(ovrHmd hmd, unsigned frame_number, const ovrViewScaleDesc *view_scale_desc, ovrLayerHeader const * const *layer_ptr_list, unsigned layer_count);

typedef ovrHmdDesc7(__cdecl *PFUNC_HMDDESC)(ovrHmd hmd);
typedef ovrResult(__cdecl *PFUNC_CREATE7)(ovrHmd *hmd_pointer, ovrGraphicsLuid *LUID_pointer);
typedef ovrResult(__cdecl *PFUNC_CREATEMIRRORD3D117)(ovrHmd hmd, void *id3d11device, unsigned ovrswaptexturesetd3d11_typeless_flag, const void *d3d11_texture2d_desc, ovrTexture6 **mirror_texture_result_pointer);
typedef ovrResult(__cdecl *PFUNC_CREATESWAPD3D117)(ovrHmd hmd, void *id3d11device, unsigned ovrswaptexturesetd3d11_typeless_flag, const void *d3d11_texture2d_desc, ovrSwapTextureSet **swap_texture_set_result_pointer);
typedef ovrResult(__cdecl *PFUNC_INPUT)(ovrHmd hmd, unsigned controller_type_mask, ovrInputState *input_state_result);
typedef ovrResult(__cdecl *PFUNC_LOOKUP)(const char *name, void **data_result_pointer);
typedef ovrResult(__cdecl *PFUNC_VIBE)(ovrHmd hmd, unsigned controller_types_mask, ovrScalar frequency_zero_half_or_one, ovrScalar amplitude_zero_to_one);
typedef ovrTrackingState7(__cdecl *PFUNC_TRACKING_STATE7)(ovrHmd hmd, double time);
typedef void(__cdecl *PFUNC_EYEPOSES7)(ovrHmd hmd, unsigned frame_number, const ovrVector3f *eye_offset_array_result, ovrPosef *eye_pose_array_result, ovrTrackingState7 *tracking_state_result);

// These functions will work without the DLL
bool ovr_InitializeRenderingShimVersion(int MinorVersion);
bool ovr_InitializeRenderingShim(void);
ovrResult ovr_Initialize(void *initParams = nullptr);
void ovr_Shutdown(void);
ovrHmd ovrHmd_Create(int number);
ovrHmd ovrHmd_CreateDebug(int version_ovrHmd);
ovrResult ovrHmd_Create(int number, ovrHmd *hmd_pointer);
ovrResult ovrHmd_CreateDebug(int version_ovrHmd, ovrHmd *hmd_pointer);
ovrResult ovr_Create(ovrHmd *hmd_pointer, ovrGraphicsLuid *LUID_pointer);
int ovrHmd_Detect(void);
const char *ovr_GetVersionString(void);
ovrHmdDescComplete ovr_GetHmdDesc(ovrHmd hmd);
ovrResult ovrHmd_ConfigureTracking(ovrHmd hmd, unsigned supported_ovrTrackingCap, unsigned required_ovrTrackingCap);
ovrResult ovrHmd_DismissHSWDisplay(ovrHmd hmd);

// These functions will crash unless ovr_Initialize returned successfully
extern PFUNC_VOID_HMD ovrHmd_RecenterPose, ovrHmd_EndFrameTiming, ovrHmd_Destroy, ovr_ResetBackOfHeadTracking, ovr_ResetMulticameraTracking;
extern PFUNC_PCHAR_HMD ovrHmd_GetLastError, ovrHmd_GetLatencyTestResult;
extern PFUNC_ATTACH ovrHmd_AttachToWindow;
extern PFUNC_UINT_HMD ovrHmd_GetEnabledCaps;
extern PFUNC_HMD_UINT ovrHmd_SetEnabledCaps, ovrHmd_ResetFrameTiming;
extern PFUNC_TRACKING_STATE ovrHmd_GetTrackingState;
extern PFUNC_FOV ovrHmd_GetFovTextureSize;
extern PFUNC_CONFIG ovrHmd_ConfigureRendering;
extern PFUNC_BEGIN ovrHmd_BeginFrame, ovrHmd_GetFrameTiming, ovrHmd_BeginFrameTiming;
extern PFUNC_END ovrHmd_EndFrame;
extern PFUNC_EYEPOSES ovrHmd_GetEyePoses;
extern PFUNC_EYEPOSE ovrHmd_GetHmdPosePerEye;
extern PFUNC_RENDERDESC ovrHmd_GetRenderDesc;
extern PFUNC_DISTORTIONMESH ovrHmd_CreateDistortionMesh;
extern PFUNC_DISTORTIONMESHDEBUG ovrHmd_CreateDistortionMeshDebug;
extern PFUNC_DESTROYMESH ovrHmd_DestroyDistortionMesh;
extern PFUNC_SCALEOFFSET ovrHmd_GetRenderScaleAndOffset;
extern PFUNC_FRAMETIMING ovr_GetFrameTiming;
extern PFUNC_TIMEWARP ovrHmd_GetEyeTimewarpMatrices;
extern PFUNC_TIMEWARPDEBUG ovrHmd_GetEyeTimewarpMatricesDebug;
extern PFUNC_DOUBLE ovr_GetTimeInSeconds;
extern PFUNC_HMD_HSW ovrHmd_GetHSWDisplayState;
extern PFUNC_LATENCY ovrHmd_ProcessLatencyTest, ovrHmd_GetLatencyTest2DrawColor;
extern PFUNC_DOUBLE_DOUBLE ovr_WaitTillTime;
extern PFUNC_PROJECTION ovrMatrix4f_Projection;
extern PFUNC_GETBOOL ovrHmd_GetBool;
extern PFUNC_GETFLOAT ovrHmd_GetFloat;
extern PFUNC_GETFLOATARRAY ovrHmd_GetFloatArray;
extern PFUNC_GETINT ovrHmd_GetInt;
extern PFUNC_GETSTRING ovrHmd_GetString;
extern PFUNC_SETBOOL ovrHmd_SetBool;
extern PFUNC_SETFLOAT ovrHmd_SetFloat;
extern PFUNC_SETFLOATARRAY ovrHmd_SetFloatArray;
extern PFUNC_SETINT ovrHmd_SetInt;
extern PFUNC_SETSTRING ovrHmd_SetString;
extern PFUNC_TRACE ovr_TraceMessage;
extern PFUNC_ORTHO ovrMatrix4f_OrthoSubProjection;
extern PFUNC_TIMEWARPPROJ ovrTimewarpProjectionDesc_FromProjection;

extern PFUNC_CREATEMIRRORGL ovrHmd_CreateMirrorTextureGL;
extern PFUNC_CREATEMIRRORD3D116 ovrHmd_CreateMirrorTextureD3D11;
extern PFUNC_CREATESWAPGL ovrHmd_CreateSwapTextureSetGL;
extern PFUNC_CREATESWAPD3D116 ovrHmd_CreateSwapTextureSetD3D11;
extern PFUNC_DESTROYMIRROR ovrHmd_DestroyMirrorTexture;
extern PFUNC_DESTROYSWAP ovrHmd_DestroySwapTextureSet;
extern PFUNC_CALC ovr_CalcEyePoses;
extern PFUNC_ERRORINFO ovr_GetLastErrorInfo;
extern PFUNC_SUBMIT ovrHmd_SubmitFrame;

extern PFUNC_CREATEMIRRORD3D117 ovr_CreateMirrorTextureD3D11;
extern PFUNC_CREATESWAPD3D117 ovr_CreateSwapTextureSetD3D11;
extern PFUNC_INPUT ovr_GetInputState;
extern PFUNC_LOOKUP ovr_Lookup;
extern PFUNC_VIBE ovr_SetControllerVibration;
extern PFUNC_TRACKING_STATE7 ovr_GetTrackingState;

#define ovr_Destroy ovrHmd_Destroy
#define ovr_ConfigureTracking ovrHmd_ConfigureTracking
#define ovr_DestroyMirrorTexture ovrHmd_DestroyMirrorTexture
#define ovr_DestroySwapTextureSet ovrHmd_DestroySwapTextureSet
#define ovr_CreateMirrorTextureGL ovrHmd_CreateMirrorTextureGL
#define ovr_CreateSwapTextureSetGL ovrHmd_CreateSwapTextureSetGL
#define ovr_GetBool ovrHmd_GetBool
#define ovr_GetFloat ovrHmd_GetFloat
#define ovr_GetFloatArray ovrHmd_GetFloatArray
#define ovr_GetInt ovrHmd_GetInt
#define ovr_GetString ovrHmd_GetString
#define ovr_SetBool ovrHmd_SetBool
#define ovr_SetFloat ovrHmd_SetFloat
#define ovr_SetFloatArray ovrHmd_SetFloatArray
#define ovr_SetInt ovrHmd_SetInt
#define ovr_SetString ovrHmd_SetString
#define ovr_GetEnabledCaps ovrHmd_GetEnabledCaps
#define ovr_SetEnabledCaps ovrHmd_SetEnabledCaps
#define ovr_GetFovTextureSize ovrHmd_GetFovTextureSize
#define ovr_RecenterPose ovrHmd_RecenterPose
#define ovr_SubmitFrame ovrHmd_SubmitFrame
#define ovr_GetRenderDesc ovrHmd_GetRenderDesc
#define ovrHmd_GetFrameTiming6 ovr_GetFrameTiming

namespace OVR
{
	enum Axis { Axis_X, Axis_Y, Axis_Z };
	class Quatf
	{
	public:
		float x, y, z, w;

		Quatf()
			: x(0), y(0), z(0), w(1) {}
		Quatf(const ovrQuatf &other)
			: x(other.x), y(other.y), z(other.z), w(other.w) {}

		template <Axis AXIS1, Axis AXIS2, Axis AXIS3>
		void GetEulerAngles(float *axis1_result, float *axis2_result, float *axis3_result) const
		{
			float sign = ((AXIS2 == (AXIS1 + 1) % 3) && (AXIS3 == (AXIS2 + 1) % 3)) ? 1 : -1;
			float Q[3] = { x, y, z };
			float sin_axis_2 = sign * 2 * (Q[AXIS1]*Q[AXIS3] + sign*w*Q[AXIS2]);
			float epsilon = 1e-7f;
			if (sin_axis_2 > 1.0f - epsilon)
			{
				*axis1_result = 0.0f;
				*axis2_result = (float)(3.14159265358979323846 / 2);
				*axis3_result = atan2(2 * (sign*Q[AXIS1]*Q[AXIS2] + w*Q[AXIS3]),  w*w + Q[AXIS2]*Q[AXIS2] - Q[AXIS1]*Q[AXIS1] - Q[AXIS3]*Q[AXIS3]);
			} 
			else if (sin_axis_2 < -1.0f + epsilon)
			{
				*axis1_result = 0.0f;
				*axis2_result = (float)(-3.14159265358979323846 / 2);
				*axis3_result = atan2(2 * (sign*Q[AXIS1]*Q[AXIS2] + w*Q[AXIS3]),  w*w + Q[AXIS2]*Q[AXIS2] - Q[AXIS1]*Q[AXIS1] - Q[AXIS3]*Q[AXIS3]);
			}
			else
			{
				*axis1_result = -atan2(-2*(w*Q[AXIS1] - sign*Q[AXIS2]*Q[AXIS3]),  w*w + Q[AXIS3]*Q[AXIS3] - Q[AXIS1]*Q[AXIS1] - Q[AXIS2]*Q[AXIS2]);
				*axis2_result = asin(sin_axis_2);
				*axis3_result = atan2(2*(w*Q[AXIS3] - sign*Q[AXIS1]*Q[AXIS2]),    w*w + Q[AXIS1]*Q[AXIS1] - Q[AXIS2]*Q[AXIS2] - Q[AXIS3]*Q[AXIS3]);
			}
		}
	};

	class Posef
	{
	public:
		Posef() {}
		Posef(const Posef &other)
			: Rotation(other.Rotation), Translation(other.Translation) {}
		Posef(const ovrPosef &other)
			: Rotation(other.Orientation), Translation(other.Position) {}

		Quatf Rotation;
		ovrVector3f Translation;
	};

}

#define OVR_PRODUCT_VERSION 0
#define OVR_MAJOR_VERSION   5
#define OVR_MINOR_VERSION   0
#define OVR_BUILD_VERSION   0
#define OVR_PATCH_VERSION   1
#define OVR_BUILD_NUMBER    0
#define OVR_VERSION_STRING "0.5.0.1"
