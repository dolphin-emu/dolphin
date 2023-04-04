#pragma once

#include "VRBase.h"

typedef enum ovrButton_ {
	ovrButton_A = 0x00000001, // Set for trigger pulled on the Gear VR and Go Controllers
	ovrButton_B = 0x00000002,
	ovrButton_RThumb = 0x00000004,
	ovrButton_RShoulder = 0x00000008,

	ovrButton_X = 0x00000100,
	ovrButton_Y = 0x00000200,
	ovrButton_LThumb = 0x00000400,
	ovrButton_LShoulder = 0x00000800,

	ovrButton_Up = 0x00010000,
	ovrButton_Down = 0x00020000,
	ovrButton_Left = 0x00040000,
	ovrButton_Right = 0x00080000,
	ovrButton_Enter = 0x00100000, //< Set for touchpad click on the Go Controller, menu
	// button on Left Quest Controller
	ovrButton_Back = 0x00200000, //< Back button on the Go Controller (only set when
	// a short press comes up)
	ovrButton_GripTrigger = 0x04000000, //< grip trigger engaged
	ovrButton_Trigger = 0x20000000, //< Index Trigger engaged
	ovrButton_Joystick = 0x80000000, //< Click of the Joystick

	ovrButton_EnumSize = 0x7fffffff
} ovrButton;

void IN_VRInit( engine_t *engine );
void IN_VRInputFrame( engine_t* engine );
uint32_t IN_VRGetButtonState( int controllerIndex );
XrVector2f IN_VRGetJoystickState( int controllerIndex );
XrPosef IN_VRGetPose( int controllerIndex );
void INVR_Vibrate( int duration, int chan, float intensity );
