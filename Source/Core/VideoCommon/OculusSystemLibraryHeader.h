// This is a header file for linking to the Oculus Rift system libraries to build for the Oculus Rift platform.
// Moral rights Carl Kenner
// Public Domain

#pragma once
#define OCULUSSYSTEMLIBRARYHEADER

#define ALIGN_TO_EIGHT_BYTE_BOUNDARY __declspec(align(8))

#define OVR_PRODUCT_VERSION 0
#define OVR_MAJOR_VERSION   5
#define OVR_MINOR_VERSION   0
#define OVR_BUILD_VERSION   0
#define OVR_PATCH_VERSION   1
#define OVR_BUILD_NUMBER    0
#define OVR_VERSION_STRING "0.5.0.1"

#define ovrHmd_None             0
#define ovrHmd_DK1              3
#define ovrHmd_DKHD             4
#define ovrHmd_CrystalCoveProto 5
#define ovrHmd_DK2              6
#define ovrHmd_BlackStar        7
#define ovrHmd_CB               8
#define ovrHmd_Other            9

#define ovrEye_Left  0
#define ovrEye_Right 1
#define ovrEye_Count 2

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


typedef void(__cdecl *ovrLogCallback)(int level, const char *message);

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
} ovrHmdDesc, *ovrHmd;

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
} ovrFrameTiming;

typedef struct { ovrQuatf Orientation; ovrVector3f Position; } ovrPosef;

typedef struct { ovrScalar M[4][4]; } ovrMatrix4f;

typedef struct ALIGN_TO_EIGHT_BYTE_BOUNDARY
{
	char Displayed, unused[7];
	double StartTime, DismissibleTime;
} ovrHSWDisplayState;

typedef struct
{
	unsigned Flags, RequestedMinorVersion;
	ovrLogCallback LogCallback;
	unsigned ConnectionTimeoutMS;
} ovrInitParams;

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
} ovrTextureHeader;

typedef struct
{
	ovrTextureHeader Header;
	void *PlatformData[8];
} ovrTexture;

typedef struct
{
	ovrVector3f Accelerometer, Gyro, Magnetometer;
	ovrScalar Temperature, TimeInSeconds;
} ovrSensorData;

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
	ovrTextureHeader Header;
	unsigned TexId;
} ovrGLTextureData;

typedef union
{
	ovrGLTextureData OGL;
	ovrTexture Texture;
} ovrGLTexture;

typedef char(__cdecl *PFUNC_INIT)(void *InitParams);
typedef void(__cdecl *PFUNC_VOID)(void);
typedef char(__cdecl *PFUNC_CHAR)(void);
typedef char(__cdecl *PFUNC_CHAR_INT)(int);
typedef char *(__cdecl *PFUNC_PCHAR)(void);
typedef int(__cdecl *PFUNC_INT)(void);
typedef ovrHmd(__cdecl *PFUNC_HMD_INT)(int number);
typedef ovrHmd(__cdecl *PFUNC_HMD_INT32)(int version_ovrHmd);
typedef void(__cdecl *PFUNC_VOID_HMD)(ovrHmd hmd);
typedef const char *(__cdecl *PFUNC_PCHAR_HMD)(ovrHmd hmd);
typedef char(__cdecl *PFUNC_ATTACH)(ovrHmd hmd, void *hwnd, ovrRecti *dest, ovrRecti *source);
typedef unsigned(__cdecl *PFUNC_UINT_HMD)(ovrHmd hmd);
typedef void(__cdecl *PFUNC_HMD_UINT)(ovrHmd hmd, unsigned);
typedef char(__cdecl *PFUNC_HMD_INT_INT)(ovrHmd hmd, unsigned supported_ovrTrackingCap, unsigned required_ovrTrackingCap);
typedef ovrTrackingState (__cdecl *PFUNC_TRACKING_STATE)(ovrHmd hmd, double time);
typedef ovrSizei(__cdecl *PFUNC_FOV)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs, ovrScalar pixel_ratio);
typedef char(__cdecl *PFUNC_CONFIG)(ovrHmd hmd, const ovrRenderAPIConfig *address_of_cfg_dot_Config, unsigned flags_ovrDistortionCap, const ovrFovPort *tan_of_fovs_array, ovrEyeRenderDesc *result_array);
typedef ovrFrameTiming(__cdecl *PFUNC_BEGIN)(ovrHmd hmd, unsigned frame_number);
typedef void(__cdecl *PFUNC_END)(ovrHmd hmd, const ovrPosef *render_pose_array, const ovrTexture *eye_texture_array);
typedef void(__cdecl *PFUNC_EYEPOSES)(ovrHmd hmd, unsigned frame_number, const ovrVector3f *eye_offset_array_result, ovrPosef *eye_pose_array_result, ovrTrackingState *tracking_state_result);
typedef ovrPosef(__cdecl *PFUNC_EYEPOSE)(ovrHmd hmd, int eye);
typedef ovrEyeRenderDesc(__cdecl *PFUNC_RENDERDESC)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs);
typedef char(__cdecl *PFUNC_DISTORTIONMESH)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs, unsigned flags_ovrDistortionCap, ovrDistortionMesh *mesh_result);
typedef char(__cdecl *PFUNC_DISTORTIONMESHDEBUG)(ovrHmd hmd, int eye, ovrFovPort tan_of_fovs, unsigned flags_ovrDistortionCap, ovrDistortionMesh *mesh_result, ovrScalar eye_relief_in_metres);
typedef void(__cdecl *PFUNC_DESTROYMESH)(ovrDistortionMesh *mesh);
typedef void(__cdecl *PFUNC_SCALEOFFSET)(ovrFovPort tan_of_fovs, ovrSizei size_of_texture, ovrRecti viewport, ovrVector2f *scale_and_offset_array_result);
typedef ovrFrameTiming(__cdecl *PFUNC_FRAMETIMING)(ovrHmd hmd, unsigned frame_number);
typedef double(__cdecl *PFUNC_DOUBLE)(void);
typedef double(__cdecl *PFUNC_TIMEWARP)(ovrHmd hmd, int eye, ovrPosef render_pose, ovrMatrix4f *timewarp_matrices_array_result);
typedef double(__cdecl *PFUNC_TIMEWARPDEBUG)(ovrHmd hmd, int eye, ovrPosef render_pose, ovrQuatf playerTorsoMotion, ovrMatrix4f *timewarp_matrices_array_result, double seconds_of_debug_offset);
typedef void(__cdecl *PFUNC_HMD_HSW)(ovrHmd hmd, ovrHSWDisplayState *hsw_state_result);
typedef char(__cdecl *PFUNC_CHAR_HMD)(ovrHmd hmd);
typedef char(__cdecl *PFUNC_LATENCY)(ovrHmd hmd, unsigned char *colour_rgb_array);
typedef double(__cdecl *PFUNC_DOUBLE_DOUBLE)(double time);
typedef ovrMatrix4f(__cdecl *PFUNC_PROJECTION)(ovrFovPort tan_of_fovs, float near_z, float far_z, unsigned flags_ovrProjection);

// These functions will work without the DLL
bool ovr_InitializeRenderingShimVersion(int MinorVersion);
bool ovr_InitializeRenderingShim(void);
bool ovr_Initialize(void *initParams = nullptr);
void ovr_Shutdown(void);
ovrHmd ovrHmd_Create(int number);
ovrHmd ovrHmd_CreateDebug(int version_ovrHmd);
int ovrHmd_Detect(void);
const char *ovr_GetVersionString(void);

// These functions will crash unless ovr_Initialize returned successfully
extern PFUNC_VOID_HMD ovrHmd_RecenterPose, ovrHmd_EndFrameTiming, ovrHmd_Destroy;
extern PFUNC_PCHAR_HMD ovrHmd_GetLastError, ovrHmd_GetLatencyTestResult;
extern PFUNC_ATTACH ovrHmd_AttachToWindow;
extern PFUNC_UINT_HMD ovrHmd_GetEnabledCaps;
extern PFUNC_HMD_UINT ovrHmd_SetEnabledCaps, ovrHmd_ResetFrameTiming;
extern PFUNC_HMD_INT_INT ovrHmd_ConfigureTracking;
extern PFUNC_TRACKING_STATE ovrHmd_GetTrackingState;
extern PFUNC_FOV ovrHmd_GetFovTextureSize;
extern PFUNC_CONFIG ovrHmd_ConfigureRendering;
extern PFUNC_BEGIN ovrHmd_BeginFrame;
extern PFUNC_END ovrHmd_EndFrame;
extern PFUNC_EYEPOSES ovrHmd_GetEyePoses;
extern PFUNC_EYEPOSE ovrHmd_GetHmdPosePerEye;
extern PFUNC_RENDERDESC ovrHmd_GetRenderDesc;
extern PFUNC_DISTORTIONMESH ovrHmd_CreateDistortionMesh;
extern PFUNC_DISTORTIONMESHDEBUG ovrHmd_CreateDistortionMeshDebug;
extern PFUNC_DESTROYMESH ovrHmd_DestroyDistortionMesh;
extern PFUNC_SCALEOFFSET ovrHmd_GetRenderScaleAndOffset;
extern PFUNC_FRAMETIMING ovrHmd_GetFrameTiming, ovrHmd_BeginFrameTiming;
extern PFUNC_TIMEWARP ovrHmd_GetEyeTimewarpMatrices;
extern PFUNC_TIMEWARPDEBUG ovrHmd_GetEyeTimewarpMatricesDebug;
extern PFUNC_DOUBLE ovr_GetTimeInSeconds;
extern PFUNC_HMD_HSW ovrHmd_GetHSWDisplayState;
extern PFUNC_CHAR_HMD ovrHmd_DismissHSWDisplay;
extern PFUNC_LATENCY ovrHmd_ProcessLatencyTest, ovrHmd_GetLatencyTest2DrawColor;
extern PFUNC_DOUBLE_DOUBLE ovr_WaitTillTime;
extern PFUNC_PROJECTION ovrMatrix4f_Projection;

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