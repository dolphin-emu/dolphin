// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputCommon/HotkeysXInput.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "Core/CoreParameter.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

//Copied from XInput.h, so compatibility is not broken on Linux/MacOS.
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_GUIDE            0x0400
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

#define XINPUT_ANALOG_THRESHOLD         0.30

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

		//If XInput device detected
		if (xinput_dev){
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

			//Don't waste CPU cycles on OnXInputPoll if nothing is pressed on XInput device.
			if (full_controller_state){
				OnXInputPoll(&full_controller_state);
			}
		}
	}

	void OnXInputPoll(u32* XInput_State){

		static float oldfUnitsPerMetre;
		static float oldfFreeLookSensitivity;
		static float oldfScale;
		static float freeLookSpeed;

		//Recalculate only when fUnitsPerMetre, fFreeLookSensitivity, or fScale changes.
		if (g_has_hmd && (g_ActiveConfig.fScale != oldfScale || g_Config.fUnitsPerMetre != oldfUnitsPerMetre || SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity != oldfFreeLookSensitivity)){
			freeLookSpeed = (20 / (g_Config.fUnitsPerMetre / g_ActiveConfig.fScale)) * SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity;
		}
		else if (!g_has_hmd && SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity != oldfFreeLookSensitivity){
			freeLookSpeed = 10 * SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity;
		}

		oldfUnitsPerMetre = g_Config.fUnitsPerMetre;
		oldfFreeLookSensitivity = SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity;
		oldfScale = g_ActiveConfig.fScale;

		if (IsVRSettingsXInput(XInput_State, VR_POSITION_RESET)) {
			VertexShaderManager::ResetView();
#ifdef HAVE_OCULUSSDK
			if (g_has_rift)
			{
				ovrHmd_RecenterPose(hmd);
			}
#endif
		}
		if (IsVRSettingsXInput(XInput_State, VR_CAMERA_FORWARD)) {
			VertexShaderManager::TranslateView(0.0f, freeLookSpeed);
		}
		else if (IsVRSettingsXInput(XInput_State, VR_CAMERA_BACKWARD)) {
			VertexShaderManager::TranslateView(0.0f, -freeLookSpeed);
		}
		if (IsVRSettingsXInput(XInput_State, VR_CAMERA_UP)) {
			VertexShaderManager::TranslateView(0.0f, 0.0f, -freeLookSpeed / 2);
		}
		else if (IsVRSettingsXInput(XInput_State, VR_CAMERA_DOWN)) {
			VertexShaderManager::TranslateView(0.0f, 0.0f, freeLookSpeed / 2);
		}
		if (IsVRSettingsXInput(XInput_State, VR_CAMERA_LEFT)) {
			VertexShaderManager::TranslateView(freeLookSpeed, 0.0f);
		}
		else if (IsVRSettingsXInput(XInput_State, VR_CAMERA_RIGHT)) {
			VertexShaderManager::TranslateView(-freeLookSpeed, 0.0f);
		}
		else if (g_has_hmd){
			if (IsVRSettingsXInput(XInput_State, VR_LARGER_SCALE)) {
				// Make everything 10% bigger (and further)
				g_Config.fUnitsPerMetre /= 1.10f;
				VertexShaderManager::ScaleView(1.10f);
				NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_SMALLER_SCALE)) {
				// Make everything 10% smaller (and closer)
				g_Config.fUnitsPerMetre *= 1.10f;
				VertexShaderManager::ScaleView(1.0f / 1.10f);
				NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_PERMANENT_CAMERA_FORWARD)) {
				// Move camera forward 10cm
				g_Config.fCameraForward += freeLookSpeed;
				NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_PERMANENT_CAMERA_BACKWARD)) {
				// Move camera back 10cm
				g_Config.fCameraForward -= freeLookSpeed;
				NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_CAMERA_TILT_UP)) {
				// Pitch camera up 5 degrees
				g_Config.fCameraPitch += 1.0f;
				NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_CAMERA_TILT_DOWN)) {
				// Pitch camera down 5 degrees
				g_Config.fCameraPitch -= 1.0f;
				NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_HUD_FORWARD)) {
				// Move HUD out 10cm
				g_Config.fHudDistance += 0.1f;
				NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_HUD_BACKWARD)) {
				// Move HUD in 10cm
				g_Config.fHudDistance -= 0.1f;
				if (g_Config.fHudDistance <= 0)
					g_Config.fHudDistance = 0;
				NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_HUD_THICKER)) {
				// Make HUD 10cm thicker
				if (g_Config.fHudThickness < 0.01f)
					g_Config.fHudThickness = 0.01f;
				else if (g_Config.fHudThickness < 0.1f)
					g_Config.fHudThickness += 0.01f;
				else
					g_Config.fHudThickness += 0.1f;
				NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_HUD_THINNER)) {
				// Make HUD 10cm thinner
				if (g_Config.fHudThickness <= 0.01f)
					g_Config.fHudThickness = 0;
				else if (g_Config.fHudThickness <= 0.1f)
					g_Config.fHudThickness -= 0.01f;
				else
					g_Config.fHudThickness -= 0.1f;
				NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_HUD_3D_CLOSER)) {
				// Make HUD 3D elements 5% closer (and smaller)
				if (g_Config.fHud3DCloser >= 0.95f)
					g_Config.fHud3DCloser = 1;
				else
					g_Config.fHud3DCloser += 0.05f;
				NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_HUD_3D_FURTHER)) {
				// Make HUD 3D elements 5% further (and smaller)
				if (g_Config.fHud3DCloser <= 0.05f)
					g_Config.fHud3DCloser = 0;
				else
					g_Config.fHud3DCloser -= 0.05f;
				NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_LARGER)) {
				// Make everything 20% smaller (and closer)
				g_Config.fScreenHeight *= 1.05f;
				NOTICE_LOG(VR, "Screen is %fm high", g_Config.fScreenHeight);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_SMALLER)) {
				// Make everything 20% bigger (and further)
				g_Config.fScreenHeight /= 1.05f;
				NOTICE_LOG(VR, "Screen is %fm High", g_Config.fScreenHeight);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_THICKER)) {
				// Make Screen 10cm thicker
				if (g_Config.fScreenThickness < 0.01f)
					g_Config.fScreenThickness = 0.01f;
				else if (g_Config.fScreenThickness < 0.1f)
					g_Config.fScreenThickness += 0.01f;
				else
					g_Config.fScreenThickness += 0.1f;
				NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_SCREEN_THINNER)) {
				// Make Screen 10cm thinner
				if (g_Config.fScreenThickness <= 0.01f)
					g_Config.fScreenThickness = 0;
				else if (g_Config.fScreenThickness <= 0.1f)
					g_Config.fScreenThickness -= 0.01f;
				else
					g_Config.fScreenThickness -= 0.1f;
				NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_FORWARD)) {
				// Move Screen in 10cm
				g_Config.fScreenDistance -= 0.1f;
				if (g_Config.fScreenDistance <= 0)
					g_Config.fScreenDistance = 0;
				NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_BACKWARD)) {
				// Move Screen out 10cm
				g_Config.fScreenDistance += 0.1f;
				NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_UP)) {
				// Move Screen Down (Camera Up) 10cm
				g_Config.fScreenUp -= 0.1f;
				NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_DOWN)) {
				// Move Screen Up (Camera Down) 10cm
				g_Config.fScreenUp += 0.1f;
				NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_TILT_UP)) {
				// Pitch camera up 5 degrees
				g_Config.fScreenPitch += 1.0f;
				NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);
			}
			else if (IsVRSettingsXInput(XInput_State, VR_2D_CAMERA_TILT_DOWN)) {
				// Pitch camera down 5 degrees
				g_Config.fScreenPitch -= 1.0f;
				NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);
			}
		}


	}

	u32 GetBinaryfromXInputIniStr(wxString ini_setting)
	{

		std::string ini_setting_str = std::string(ini_setting.mb_str());

		if (ini_setting_str == "Button A"){
			return XINPUT_GAMEPAD_A;
		}
		else if (ini_setting_str == "Button B"){
			return XINPUT_GAMEPAD_B;
		}
		else if (ini_setting_str == "Button X"){
			return XINPUT_GAMEPAD_X;
		}
		else if (ini_setting_str == "Button Y"){
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
			return XINPUT_GAMEPAD_GUIDE;
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

	wxString GetwxStringfromXInputIni(u32 ini_setting)
	{

		if (ini_setting == XINPUT_GAMEPAD_A){
			return "Button A";
		}
		else if (ini_setting == XINPUT_GAMEPAD_B){
			return "Button B";
		}
		else if (ini_setting == XINPUT_GAMEPAD_X){
			return "Button X";
		}
		else if (ini_setting == XINPUT_GAMEPAD_Y){
			return "Button Y";
		}
		else if (ini_setting == XINPUT_GAMEPAD_DPAD_UP){
			return "Pad N";
		}
		else if (ini_setting == XINPUT_GAMEPAD_DPAD_DOWN){
			return "Pad S";
		}
		else if (ini_setting == XINPUT_GAMEPAD_DPAD_LEFT){
			return "Pad W";
		}
		else if (ini_setting == XINPUT_GAMEPAD_DPAD_RIGHT){
			return "Pad E";
		}
		else if (ini_setting == XINPUT_GAMEPAD_START){
			return "Start";
		}
		else if (ini_setting == XINPUT_GAMEPAD_BACK){
			return "Back";
		}
		else if (ini_setting == XINPUT_GAMEPAD_LEFT_SHOULDER){
			return "Shoulder L";
		}
		else if (ini_setting == XINPUT_GAMEPAD_RIGHT_SHOULDER){
			return "Shoulder R";
		}
		else if (ini_setting == XINPUT_GAMEPAD_GUIDE){
			return "Guide";
		}
		else if (ini_setting == XINPUT_GAMEPAD_LEFT_THUMB){
			return "Thumb L";
		}
		else if (ini_setting == XINPUT_GAMEPAD_RIGHT_THUMB){
			return "Thumb R";
		}
		else if (ini_setting == (1 << 17)){
			return "Trigger L";
		}
		else if (ini_setting == (1 << 16)){
			return "Trigger R";
		}
		else if (ini_setting == (1 << 24)){
			return "Left X+";
		}
		else if (ini_setting == (1 << 25)){
			return "Left X-";
		}
		else if (ini_setting == (1 << 22)){
			return "Left Y+";
		}
		else if (ini_setting == (1 << 23)){
			return "Left Y-";
		}
		else if (ini_setting == (1 << 20)){
			return "Right X+";
		}
		else if (ini_setting == (1 << 21)){
			return "Right X-";
		}
		else if (ini_setting == (1 << 18)){
			return "Right Y+";
		}
		else if (ini_setting == (1 << 19)){
			return "Right Y-";
		}

		return "";
	}

	//Take in ID of setting and Current XInput State, and compare to see if the right buttons are being pressed.
	bool IsVRSettingsXInput(u32* XInput_State, int id)
	{
		u32 ini_setting = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[id];
		return (((*XInput_State & ini_setting) == ini_setting) &&
			false == SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[id]);
	}

	//Take xinput_string and see if it is currently set in ini.
	bool IsXInputButtonSet(wxString button_string, int id)
	{
		u32 ini_setting = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[id];
		u32 xinput_value = GetBinaryfromXInputIniStr(button_string);

		if ((xinput_value & ini_setting) == xinput_value)
			return true;
		else
			return false;
	}

}