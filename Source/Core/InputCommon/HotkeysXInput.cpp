// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "wx/string.h"

#include "InputCommon/HotkeysXInput.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/XInput/XInput.h"
#include "Core/CoreParameter.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

#define XINPUT_ANALOG_THRESHOLD 0.30

namespace HotkeysXInput
{

	void Update(){
		ciface::Core::Device* xinput_dev = nullptr;

		//Find XInput device
		for (ciface::Core::Device* d : g_controller_interface.Devices())
		{
			// for now, assume first XInput device is the one we want
			if (d->GetSource() == "XInput")
			{
				xinput_dev = d;
				break;
			}
		}

		// get inputs from xinput device
		std::vector<ciface::Core::Device::Input*>::const_iterator
			i = xinput_dev->Inputs().begin(),
			e = xinput_dev->Inputs().end();

		u16 button_states;
		bool binary_trigger_l;
		bool binary_trigger_r;
		bool binary_axis_l_x_pos;
		bool binary_axis_l_x_neg;
		bool binary_axis_l_y_pos;
		bool binary_axis_l_y_neg;
		bool binary_axis_r_x_pos;
		bool binary_axis_r_x_neg;
		bool binary_axis_r_y_pos;
		bool binary_axis_r_y_neg;

		for (i; i != e; ++i)
		{
			std::string control_name = (*i)->GetName();

			if (control_name == "Button A"){
				button_states = (*i)->GetStates();
			}
			else if (control_name == "Trigger L"){
				binary_trigger_l = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Trigger R"){
				binary_trigger_r = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left X+"){
				binary_axis_l_x_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left X-"){
				binary_axis_l_x_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left Y+"){
				binary_axis_l_y_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left Y-"){
				binary_axis_l_y_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right X+"){
				binary_axis_r_x_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right X-"){
				binary_axis_r_x_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right Y+"){
				binary_axis_r_y_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right Y-"){
				binary_axis_r_y_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
		}

		u32 full_controller_state =
			(binary_axis_l_x_neg << 25) |
			(binary_axis_l_x_pos << 24) |
			(binary_axis_l_y_neg << 23) |
			(binary_axis_l_y_pos << 22) |
			(binary_axis_r_x_neg << 21) |
			(binary_axis_r_x_pos << 20) |
			(binary_axis_r_y_neg << 19) |
			(binary_axis_r_y_pos << 18) |
			(binary_trigger_l << 17) |
			(binary_trigger_r << 16) |
			button_states;

		OnXInputPoll(&full_controller_state);
	}

	void OnXInputPoll(u32* XInput_State){

		static float debugSpeed = 0.05f; //How big the VR Camera Movement adjustments are.

		if (IsVRSettingsXInput(XInput_State, VR_POSITION_RESET)) {
			VertexShaderManager::ResetView();
#ifdef HAVE_OCULUSSDK
			if (g_has_rift)
			{
				ovrHmd_RecenterPose(hmd);
			}
#endif
		}
		if (g_Config.bFreeLook && IsVRSettingsXInput(XInput_State, VR_CAMERA_FORWARD)) {
			VertexShaderManager::TranslateView(0.0f, debugSpeed);
		}
		else if (g_Config.bFreeLook && IsVRSettingsXInput(XInput_State, VR_CAMERA_BACKWARD)) {
			VertexShaderManager::TranslateView(0.0f, -debugSpeed);
		}
		if (g_Config.bFreeLook && IsVRSettingsXInput(XInput_State, VR_CAMERA_UP)) {
			VertexShaderManager::TranslateView(0.0f, 0.0f, -debugSpeed / 2);
		}
		else if (g_Config.bFreeLook && IsVRSettingsXInput(XInput_State, VR_CAMERA_DOWN)) {
			VertexShaderManager::TranslateView(0.0f, 0.0f, debugSpeed / 2);
		}
		if (g_Config.bFreeLook && IsVRSettingsXInput(XInput_State, VR_CAMERA_LEFT)) {
			VertexShaderManager::TranslateView(debugSpeed, 0.0f);
		}
		else if (g_Config.bFreeLook && IsVRSettingsXInput(XInput_State, VR_CAMERA_RIGHT)) {
			VertexShaderManager::TranslateView(-debugSpeed, 0.0f);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_LARGER_SCALE)) {
			// Make everything 10% bigger (and further)
			g_Config.fUnitsPerMetre /= 1.10f;
			VertexShaderManager::ScaleView(1.10f);
			NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_SMALLER_SCALE)) {
			// Make everything 10% smaller (and closer)
			g_Config.fUnitsPerMetre *= 1.10f;
			VertexShaderManager::ScaleView(1.0f / 1.10f);
			NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_PERMANENT_CAMERA_FORWARD)) {
			// Move camera forward 10cm
			g_Config.fCameraForward += 0.1f;
			NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_PERMANENT_CAMERA_BACKWARD)) {
			// Move camera back 10cm
			g_Config.fCameraForward -= 0.1f;
			NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_CAMERA_TILT_UP)) {
			// Pitch camera up 5 degrees
			g_Config.fCameraPitch += 1.0f;
			NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_CAMERA_TILT_DOWN)) {
			// Pitch camera down 5 degrees
			g_Config.fCameraPitch -= 1.0f;
			NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_HUD_FORWARD)) {
			// Move HUD out 10cm
			g_Config.fHudDistance += 0.1f;
			NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_HUD_BACKWARD)) {
			// Move HUD in 10cm
			g_Config.fHudDistance -= 0.1f;
			if (g_Config.fHudDistance <= 0)
				g_Config.fHudDistance = 0;
			NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_HUD_THICKER)) {
			// Make HUD 10cm thicker
			if (g_Config.fHudThickness < 0.01f)
				g_Config.fHudThickness = 0.01f;
			else if (g_Config.fHudThickness < 0.1f)
				g_Config.fHudThickness += 0.01f;
			else
				g_Config.fHudThickness += 0.1f;
			NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_HUD_THINNER)) {
			// Make HUD 10cm thinner
			if (g_Config.fHudThickness <= 0.01f)
				g_Config.fHudThickness = 0;
			else if (g_Config.fHudThickness <= 0.1f)
				g_Config.fHudThickness -= 0.01f;
			else
				g_Config.fHudThickness -= 0.1f;
			NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_HUD_3D_CLOSER)) {
			// Make HUD 3D elements 5% closer (and smaller)
			if (g_Config.fHud3DCloser >= 0.95f)
				g_Config.fHud3DCloser = 1;
			else
				g_Config.fHud3DCloser += 0.05f;
			NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_HUD_3D_FURTHER)) {
			// Make HUD 3D elements 5% further (and smaller)
			if (g_Config.fHud3DCloser <= 0.05f)
				g_Config.fHud3DCloser = 0;
			else
				g_Config.fHud3DCloser -= 0.05f;
			NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_LARGER)) {
			// Make everything 20% smaller (and closer)
			g_Config.fScreenHeight *= 1.05f;
			NOTICE_LOG(VR, "Screen is %fm high", g_Config.fScreenHeight);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_SMALLER)) {
			// Make everything 20% bigger (and further)
			g_Config.fScreenHeight /= 1.05f;
			NOTICE_LOG(VR, "Screen is %fm High", g_Config.fScreenHeight);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_THICKER)) {
			// Make Screen 10cm thicker
			if (g_Config.fScreenThickness < 0.01f)
				g_Config.fScreenThickness = 0.01f;
			else if (g_Config.fScreenThickness < 0.1f)
				g_Config.fScreenThickness += 0.01f;
			else
				g_Config.fScreenThickness += 0.1f;
			NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_THINNER)) {
			// Make Screen 10cm thinner
			if (g_Config.fScreenThickness <= 0.01f)
				g_Config.fScreenThickness = 0;
			else if (g_Config.fScreenThickness <= 0.1f)
				g_Config.fScreenThickness -= 0.01f;
			else
				g_Config.fScreenThickness -= 0.1f;
			NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_FORWARD)) {
			// Move Screen in 10cm
			g_Config.fScreenDistance -= 0.1f;
			if (g_Config.fScreenDistance <= 0)
				g_Config.fScreenDistance = 0;
			NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_BACKWARD)) {
			// Move Screen out 10cm
			g_Config.fScreenDistance += 0.1f;
			NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_UP)) {
			// Move Screen Down (Camera Up) 10cm
			g_Config.fScreenUp -= 0.1f;
			NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_DOWN)) {
			// Move Screen Up (Camera Down) 10cm
			g_Config.fScreenUp += 0.1f;
			NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_TILT_UP)) {
			// Pitch camera up 5 degrees
			g_Config.fScreenPitch += 1.0f;
			NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);
		}
		else if (g_has_rift && IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_TILT_DOWN)) {
			// Pitch camera down 5 degrees
			g_Config.fScreenPitch -= 1.0f;
			NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);;
		}

		//if ((*XInput_State & ((1 << 18) | XINPUT_GAMEPAD_RIGHT_SHOULDER)) == ((1 << 18) | XINPUT_GAMEPAD_RIGHT_SHOULDER)) { //RIGHT STICK - UP
		//	VertexShaderManager::TranslateView(0.0f, 0.01f);
		//}
		//if ((*XInput_State & ((1 << 19) | XINPUT_GAMEPAD_RIGHT_SHOULDER)) == ((1 << 19) | XINPUT_GAMEPAD_RIGHT_SHOULDER)){ //RIGHT STICK - DOWN
		//	VertexShaderManager::TranslateView(0.0f, -0.01f);
		//}
		//if ((*XInput_State & ((1 << 20) | XINPUT_GAMEPAD_RIGHT_SHOULDER)) == ((1 << 20) | XINPUT_GAMEPAD_RIGHT_SHOULDER)){ //RIGHT STICK - RIGHT
		//	VertexShaderManager::TranslateView(-0.01f, 0.0f);
		//}
		//if ((*XInput_State & ((1 << 21) | XINPUT_GAMEPAD_RIGHT_SHOULDER)) == ((1 << 21) | XINPUT_GAMEPAD_RIGHT_SHOULDER)){ //RIGHT STICK - LEFT
		//	VertexShaderManager::TranslateView(0.01f, 0.0f);
		//}


	}

	u32 GetBinaryfromXInputIni(wxString ini_setting)
	{

		std::string ini_setting_str = std::string(ini_setting.mb_str());

		if (ini_setting_str == "Button A"){
			return XINPUT_GAMEPAD_A;
		}
		else if (ini_setting_str == "Button B"){
			return XINPUT_GAMEPAD_B;
		}
		else if (ini_setting_str == "Button A"){
			return XINPUT_GAMEPAD_X;
		}
		else if (ini_setting_str == "Button A"){
			return XINPUT_GAMEPAD_Y;
		}
		else if (ini_setting_str == "Pad N"){
			return XINPUT_GAMEPAD_DPAD_UP;
		}
		else if (ini_setting_str == "Pad S"){
			return XINPUT_GAMEPAD_DPAD_DOWN;
		}
		else if (ini_setting_str == "Pad W"){
			return XINPUT_GAMEPAD_DPAD_LEFT;
		}
		else if (ini_setting_str == "Pad E"){
			return XINPUT_GAMEPAD_DPAD_RIGHT;
		}
		else if (ini_setting_str == "Start"){
			return XINPUT_GAMEPAD_START;
		}
		else if (ini_setting_str == "Back"){
			return XINPUT_GAMEPAD_BACK;
		}
		else if (ini_setting_str == "Shoulder L"){
			return XINPUT_GAMEPAD_LEFT_SHOULDER;
		}
		else if (ini_setting_str == "Shoulder R"){
			return XINPUT_GAMEPAD_RIGHT_SHOULDER;
		}
		else if (ini_setting_str == "Guide"){
			return 0x0400;
		}
		else if (ini_setting_str == "Thumb L"){
			return XINPUT_GAMEPAD_LEFT_THUMB;
		}
		else if (ini_setting_str == "Thumb R"){
			return XINPUT_GAMEPAD_RIGHT_THUMB;
		}
		else if (ini_setting_str == "Trigger L"){
			return (1 << 17);
		}
		else if (ini_setting_str == "Trigger R"){
			return (1 << 16);
		}
		else if (ini_setting_str == "Left X+"){
			return (1 << 24);
		}
		else if (ini_setting_str == "Left X-"){
			return (1 << 25);
		}
		else if (ini_setting_str == "Left Y+"){
			return (1 << 22);
		}
		else if (ini_setting_str == "Left Y-"){
			return (1 << 23);
		}
		else if (ini_setting_str == "Right X+"){
			return (1 << 20);
		}
		else if (ini_setting_str == "Right X-"){
			return (1 << 21);
		}
		else if (ini_setting_str == "Right Y+"){
			return (1 << 18);
		}
		else if (ini_setting_str == "Right Y-"){
			return (1 << 19);
		}

		return -1;
	}

	bool IsVRSettingsXInput(u32* XInput_State, int Id)
	{
		u32 ini_setting = GetBinaryfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[Id]);
		return (((*XInput_State & ini_setting) == ini_setting) &&
			false == SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[Id]);
	}

}