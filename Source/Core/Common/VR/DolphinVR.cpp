#include <cstring>

#include "Common/VR/DolphinVR.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRInput.h"
#include "Common/VR/VRRenderer.h"

/*
================================================================================

VR controller mapping

================================================================================
*/

enum ControlID
{
  GCPAD_A_BUTTON = 0,
  GCPAD_B_BUTTON = 1,
  GCPAD_X_BUTTON = 2,
  GCPAD_Y_BUTTON = 3,
  GCPAD_Z_BUTTON = 4,
  GCPAD_START_BUTTON = 5,
  GCPAD_DPAD_UP = 6,
  GCPAD_DPAD_DOWN = 7,
  GCPAD_DPAD_LEFT = 8,
  GCPAD_DPAD_RIGHT = 9,
  GCPAD_L_DIGITAL = 10,
  GCPAD_R_DIGITAL = 11,
  GCPAD_L_ANALOG = 12,
  GCPAD_R_ANALOG = 13,
  GCPAD_MAIN_STICK_X = 14,
  GCPAD_MAIN_STICK_Y = 15,
  GCPAD_C_STICK_X = 16,
  GCPAD_C_STICK_Y = 17,

  WIIMOTE_A_BUTTON = 18,
  WIIMOTE_B_BUTTON = 19,
  WIIMOTE_ONE_BUTTON = 20,
  WIIMOTE_TWO_BUTTON = 21,
  WIIMOTE_PLUS_BUTTON = 22,
  WIIMOTE_MINUS_BUTTON = 23,
  WIIMOTE_HOME_BUTTON = 24,
  WIIMOTE_DPAD_UP = 25,
  WIIMOTE_DPAD_DOWN = 26,
  WIIMOTE_DPAD_LEFT = 27,
  WIIMOTE_DPAD_RIGHT = 28,
  WIIMOTE_IR_X = 29,
  WIIMOTE_IR_Y = 30,

  NUNCHUK_C_BUTTON = 31,
  NUNCHUK_Z_BUTTON = 32,
  NUNCHUK_STICK_X = 33,
  NUNCHUK_STICK_Y = 34,

  CLASSIC_A_BUTTON = 35,
  CLASSIC_B_BUTTON = 36,
  CLASSIC_X_BUTTON = 37,
  CLASSIC_Y_BUTTON = 38,
  CLASSIC_ZL_BUTTON = 39,
  CLASSIC_ZR_BUTTON = 40,
  CLASSIC_PLUS_BUTTON = 41,
  CLASSIC_MINUS_BUTTON = 42,
  CLASSIC_HOME_BUTTON = 43,
  CLASSIC_DPAD_UP = 44,
  CLASSIC_DPAD_DOWN = 45,
  CLASSIC_DPAD_LEFT = 46,
  CLASSIC_DPAD_RIGHT = 47,
  CLASSIC_L_DIGITAL = 48,
  CLASSIC_R_DIGITAL = 49,
  CLASSIC_L_ANALOG = 50,
  CLASSIC_R_ANALOG = 51,
  CLASSIC_LEFT_STICK_X = 52,
  CLASSIC_LEFT_STICK_Y = 53,
  CLASSIC_RIGHT_STICK_X = 54,
  CLASSIC_RIGHT_STICK_Y = 55,
};

static void (*SetControlState)(int controller_index, int control, double state);

void SetVRCallbacks(void (*callback)(int controller_index, int control, double state))
{
  SetControlState = callback;
}

void UpdateVRInput()
{
  int leftController = IN_VRGetButtonState(0);
  SetControlState(0, WIIMOTE_PLUS_BUTTON, leftController & xrButton_X ? 1 : 0);
  SetControlState(0, WIIMOTE_MINUS_BUTTON, leftController & xrButton_Y ? 1 : 0);
  SetControlState(0, WIIMOTE_HOME_BUTTON, leftController & xrButton_Back ? 1 : 0);
  SetControlState(0, NUNCHUK_C_BUTTON, leftController & xrButton_Trigger ? 1 : 0);
  SetControlState(0, NUNCHUK_Z_BUTTON, leftController & xrButton_GripTrigger ? 1 : 0);
  SetControlState(0, NUNCHUK_STICK_X, IN_VRGetJoystickState(0).x);
  SetControlState(0, NUNCHUK_STICK_Y, IN_VRGetJoystickState(0).y);

  int rightController = IN_VRGetButtonState(1);
  SetControlState(0, WIIMOTE_A_BUTTON, rightController & xrButton_Trigger ? 1 : 0);
  SetControlState(0, WIIMOTE_B_BUTTON, rightController & xrButton_GripTrigger ? 1 : 0);
  SetControlState(0, WIIMOTE_ONE_BUTTON, rightController & xrButton_A ? 1 : 0);
  SetControlState(0, WIIMOTE_TWO_BUTTON, rightController & xrButton_B ? 1 : 0);
  SetControlState(0, WIIMOTE_DPAD_UP, rightController & xrButton_Up ? 1 : 0);
  SetControlState(0, WIIMOTE_DPAD_DOWN, rightController & xrButton_Down ? 1 : 0);
  SetControlState(0, WIIMOTE_DPAD_LEFT, rightController & xrButton_Left ? 1 : 0);
  SetControlState(0, WIIMOTE_DPAD_RIGHT, rightController & xrButton_Right ? 1 : 0);

  XrPosef pose = IN_VRGetPose(1);
  XrVector3f angles = XrQuaternionf_ToEulerAngles(pose.orientation);
  float x = -tan(ToRadians(angles.y - VR_GetConfigFloat(VR_CONFIG_MENU_YAW)));
  float y = -tan(ToRadians(angles.x)) * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);
  SetControlState(0, WIIMOTE_IR_X, x);
  SetControlState(0, WIIMOTE_IR_Y, y);
}

/*
================================================================================

VR app flow integration

================================================================================
*/

bool IsVREnabled()
{
#ifdef OPENXR
  return true;
#else
  return false;
#endif
}

#ifdef ANDROID
void InitVROnAndroid(void* vm, void* activity, const char* vendor, int version, const char* name)
{
  // Set platform flags
  if (strcmp(vendor, "Pico") == 0)
  {
    VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_PICO, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_INSTANCE, true);
  }
  else if ((strcmp(vendor, "Meta") == 0) || (strcmp(vendor, "Oculus") == 0))
  {
    VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_QUEST, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_FOVEATION, false);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_PERFORMANCE, false);
  }

  // Init VR
  ovrJava java;
  java.Vm = (JavaVM*)vm;
  java.ActivityObject = (jobject)activity;
  VR_Init(&java, name, version);
}
#endif

void EnterVR(bool firstStart)
{
  if (firstStart)
  {
    engine_t* engine = VR_GetEngine();
    VR_EnterVR(engine);
    IN_VRInit(engine);
  }
  VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, false);
}

void GetVRResolutionPerEye(int* width, int* height)
{
  if (VR_GetEngine()->appState.Instance)
  {
    VR_GetResolution(VR_GetEngine(), width, height);
  }
}

/*
================================================================================

VR rendering integration

================================================================================
*/

void BindVRFramebuffer()
{
  VR_BindFramebuffer(VR_GetEngine());
}

bool StartVRRender()
{
  if (!VR_GetConfig(VR_CONFIG_VIEWPORT_VALID))
  {
    VR_InitRenderer(VR_GetEngine(), false);
    VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, true);
  }

  if (VR_InitFrame(VR_GetEngine()))
  {
    VR_SetConfigFloat(VR_CONFIG_CANVAS_DISTANCE, 6.0f);
    VR_SetConfigFloat(VR_CONFIG_CANVAS_ASPECT, 1.0f);
    VR_SetConfig(VR_CONFIG_MODE, VR_MODE_MONO_SCREEN);
    UpdateVRInput();
    return true;
  }
  return false;
}

void FinishVRRender()
{
  VR_FinishFrame(VR_GetEngine());
}

void PreVRFrameRender(int fboIndex)
{
  VR_BeginFrame(VR_GetEngine(), fboIndex);
}

void PostVRFrameRender()
{
  VR_EndFrame(VR_GetEngine());
}

int GetVRFBOIndex()
{
  return VR_GetConfig(VR_CONFIG_CURRENT_FBO);
}
