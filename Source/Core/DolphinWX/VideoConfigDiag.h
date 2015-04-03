// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/window.h>

#include "Common/CommonTypes.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "DolphinWX/PostProcessingConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

class wxBoxSizer;
class wxControl;
class wxPanel;

template <typename W>
class BoolSetting : public W
{
public:
	BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse = false, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = (ev.GetInt() != 0) ^ m_reverse;
		ev.Skip();
	}
private:
	bool &m_setting;
	const bool m_reverse;
};

typedef BoolSetting<wxCheckBox> SettingCheckBox;
typedef BoolSetting<wxRadioButton> SettingRadioButton;

template <typename T>
class IntegerSetting : public wxSpinCtrl
{
public:
	IntegerSetting(wxWindow* parent, const wxString& label, T& setting, int minVal, int maxVal, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = ev.GetInt();
		ev.Skip();
	}
private:
	T& m_setting;
};

typedef IntegerSetting<u32> U32Setting;

template <typename T>
class FloatSetting : public wxSpinCtrlDouble
{
public:
	FloatSetting(wxWindow* parent, const wxString& label, T& setting, T minVal, T maxVal, T increment = 0, long style = 0);

	void UpdateValue(wxSpinDoubleEvent& ev)
	{
		m_setting = ev.GetValue();
		ev.Skip();
	}
private:
	T& m_setting;
};

typedef FloatSetting<double> SettingDouble;
typedef FloatSetting<float> SettingNumber;

class SettingChoice : public wxChoice
{
public:
	SettingChoice(wxWindow* parent, int &setting, const wxString& tooltip, int num = 0, const wxString choices[] = nullptr, long style = 0);
	void UpdateValue(wxCommandEvent& ev);
private:
	int &m_setting;
};

class VideoConfigDiag : public wxDialog
{
public:
	VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& ininame);

protected:
	void Event_Backend(wxCommandEvent &ev)
	{
		VideoBackend* new_backend = g_available_video_backends[ev.GetInt()];
		if (g_video_backend != new_backend)
		{
			bool do_switch = !Core::IsRunning();
			if (new_backend->GetName() == "Software Renderer")
			{
				do_switch = (wxYES == wxMessageBox(_("Software rendering is an order of magnitude slower than using the other backends.\nIt's only useful for debugging purposes.\nDo you really want to enable software rendering? If unsure, select 'No'."),
							_("Warning"), wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION, wxWindow::FindFocus()));
			}

			if (do_switch)
			{
				// TODO: Only reopen the dialog if the software backend is
				// selected (make sure to reinitialize backend info)
				// reopen the dialog
				Close();

				g_video_backend = new_backend;
				SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend = g_video_backend->GetName();

				g_video_backend->ShowConfig(GetParent());
			}
			else
			{
				// Select current backend again
				choice_backend->SetStringSelection(StrToWxStr(g_video_backend->GetName()));
			}
		}

		ev.Skip();
	}
	void Event_Adapter(wxCommandEvent &ev) { ev.Skip(); } // TODO

	void Event_DisplayResolution(wxCommandEvent &ev);

	void Event_ProgressiveScan(wxCommandEvent &ev)
	{
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", ev.GetInt());
		SConfig::GetInstance().m_LocalCoreStartupParameter.bProgressive = ev.IsChecked();

		ev.Skip();
	}

	void Event_Stc(wxCommandEvent &ev)
	{
		int samples[] = { 0, 512, 128 };
		vconfig.iSafeTextureCache_ColorSamples = samples[ev.GetInt()];

		ev.Skip();
	}

	void Event_PPShader(wxCommandEvent &ev)
	{
		const int sel = ev.GetInt();
		if (sel)
			vconfig.sPostProcessingShader = WxStrToStr(ev.GetString());
		else
			vconfig.sPostProcessingShader.clear();

		// Should we enable the configuration button?
		PostProcessingShaderConfiguration postprocessing_shader;
		postprocessing_shader.LoadShader(vconfig.sPostProcessingShader);
		button_config_pp->Enable(postprocessing_shader.HasOptions());

		ev.Skip();
	}

	void Event_ConfigurePPShader(wxCommandEvent &ev)
	{
		PostProcessingConfigDiag dialog(this, vconfig.sPostProcessingShader);
		dialog.ShowModal();

		ev.Skip();
	}

	void Event_StereoDepth(wxCommandEvent &ev)
	{
		vconfig.iStereoDepth = ev.GetInt();

		ev.Skip();
	}

	void Event_StereoConvergence(wxCommandEvent &ev)
	{
		vconfig.iStereoConvergence = ev.GetInt();

		ev.Skip();
	}

	void Event_StereoMode(wxCommandEvent &ev)
	{
		if (vconfig.backend_info.bSupportsPostProcessing)
		{
			// Anaglyph overrides post-processing shaders
			choice_ppshader->Clear();
		}

		ev.Skip();
	}

	void Event_ClickClose(wxCommandEvent&);
	void Event_ClickSave(wxCommandEvent&);
	void Event_Close(wxCloseEvent&);

	// Enables/disables UI elements depending on current config
	void OnUpdateUI(wxUpdateUIEvent& ev)
	{
		// Anti-aliasing
		choice_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);
		text_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);

		// EFB copy
		efbcopy_clear_disable->Enable(!vconfig.bEFBCopyEnable);
		efbcopy_texture->Enable(vconfig.bEFBCopyEnable);
		efbcopy_ram->Enable(vconfig.bEFBCopyEnable);

		// XFB
		virtual_xfb->Enable(vconfig.bUseXFB);
		real_xfb->Enable(vconfig.bUseXFB);

		// Repopulating the post-processing shaders can't be done from an event
		if (choice_ppshader && choice_ppshader->IsEmpty())
			PopulatePostProcessingShaders();

		// Things which shouldn't be changed during emulation
		if (Core::IsRunning())
		{
			choice_backend->Disable();
			label_backend->Disable();

			// D3D only
			if (vconfig.backend_info.Adapters.size())
			{
				choice_adapter->Disable();
				label_adapter->Disable();
			}

#ifndef __APPLE__
			// This isn't supported on OS X.

			choice_display_resolution->Disable();
			label_display_resolution->Disable();
#endif

			progressive_scan_checkbox->Disable();
			render_to_main_checkbox->Disable();
		}
		ev.Skip();
	}

	// Creates controls and connects their enter/leave window events to Evt_Enter/LeaveControl
	SettingCheckBox* CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse = false, long style = 0);
	SettingChoice* CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num = 0, const wxString choices[] = nullptr, long style = 0);
	SettingRadioButton* CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse = false, long style = 0);
	SettingNumber* CreateNumber(wxWindow* parent, float &setting, const wxString& description, float min, float max, float inc, long style = 0);

	// Same as above but only connects enter/leave window events
	wxControl* RegisterControl(wxControl* const control, const wxString& description);

	void Evt_EnterControl(wxMouseEvent& ev);
	void Evt_LeaveControl(wxMouseEvent& ev);
	void CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer);
	void PopulatePostProcessingShaders();

	wxChoice* choice_backend;
	wxChoice* choice_adapter;
	wxChoice* choice_display_resolution;

	wxStaticText* label_backend;
	wxStaticText* label_adapter;

	wxStaticText* text_aamode;
	SettingChoice* choice_aamode;

	wxStaticText* label_display_resolution;

	wxButton* button_config_pp;

	SettingCheckBox* borderless_fullscreen;
	SettingCheckBox* render_to_main_checkbox;
	SettingCheckBox* async_timewarp_checkbox;
	SettingCheckBox* efbcopy_clear_disable;

	SettingRadioButton* efbcopy_texture;
	SettingRadioButton* efbcopy_ram;

	SettingRadioButton* virtual_xfb;
	SettingRadioButton* real_xfb;

	wxCheckBox* progressive_scan_checkbox;

	wxChoice* choice_ppshader;

	std::map<wxWindow*, wxString> ctrl_descs; // maps setting controls to their descriptions
	std::map<wxWindow*, wxStaticText*> desc_texts; // maps dialog tabs (which are the parents of the setting controls) to their description text objects

	VideoConfig &vconfig;
	std::string ininame;
};

static wxString async_desc = wxTRANSLATE("Render head rotation updates in a separate thread at full frame rate using Timewarp even when the game runs at a lower frame rate.");
static wxString temp_desc = wxTRANSLATE("Game specific VR option, in metres or degrees");
static wxString minfov_desc = wxTRANSLATE("Minimum horizontal degrees of your view that the action will fill.\nWhen the game tries to render from a distance with a zoom lens, this will move thhe camera closer instead. When the FOV is less than the minimum the camera will move forward until objects at Aim Distance fill the minimum FOV.\nIf unsure, leave this at 10 degrees.");
static wxString scale_desc = wxTRANSLATE("(Don't change this until the game's Units Per Metre setting is already lifesize!)\n\nScale multiplier for all VR worlds.\n1x = lifesize, 2x = Giant size\n0.5x = Child size, 0.17x = Barbie doll size, 0.02x = Lego size\n\nIf unsure, use 1.00.");
static wxString lean_desc = wxTRANSLATE("How many degrees leaning back should count as vertical.\n0 = sitting/standing, 45 = reclining\n90 = playing lying on your back, -90 = on your front\n\nIf unsure, use 0.");
static wxString extraframes_desc = wxTRANSLATE("How many extra frames to render via timewarp.  Set to 0 for 60fps games, 1 for 30fps games, 2 for 20fps games and the framelimiter to your Rift's refresh rate.  For 25fps PAL games, set to 2 and set the frame limiter to 60 (assuming the Rift's refresh rate is set to 75hz).  If unsure, use 0.");
static wxString replaybuffer_desc = wxTRANSLATE("How many extra frames to render through replaying the video loop.  Set to 0 for 60fps games, 1 for 30fps games, 2 for 20fps games and the framelimiter to your Rift's refresh rate.  For 25fps PAL games, set to 2 and set the frame limiter to 60 (assuming the Rift's refresh rate is set to 75hz).  If unsure, use 0.");
static wxString stabilizeroll_desc = wxTRANSLATE("Counteract the game's camera roll.  Requires 'Read Camera Angles' to be checked in the 'VR Game' tab.\nIf unsure, leave this unchecked.");
static wxString stabilizepitch_desc = wxTRANSLATE("Counteract the game's camera pitch. Fixes tilt in 3rd person games and allows free y-axis aiming in 1st person games.  Requires 'Read Camera Angles' to be checked in the 'VR Game' tab.\nIf unsure, leave this unchecked.");
static wxString stabilizeyaw_desc = wxTRANSLATE("Counteract the game's camera yaw. Shouldn't be enabled for most 3rd person games. Allows completely free x-axis aiming in 1st person games.  Requires 'Read Camera Angles' to be checked in the 'VR Game' tab.\nIf unsure, leave this unchecked.");
static wxString keyhole_desc = wxTRANSLATE("Allows keyhole aiming in 1st person games by manipulating the yaw correction. Great for first person shooters.\nIf unsure, leave this unchecked.");
static wxString keyholewidth_desc = wxTRANSLATE("The width of the horizontal keyhole in degrees.\nIf unsure, use 45.");
static wxString keyholesnap_desc = wxTRANSLATE("Snaps the view a certain amount of degrees to the side if the edge of the keyhole is reached. This lowers motion sickness during 1st person turns, but may be considered less immersive.\nIf unsure, leave this unchcked.");
static wxString keyholesnapsize_desc = wxTRANSLATE("The size of the heyhole snap jump in degrees.\nIf unsure, use 30.");
static wxString pullup20_desc = wxTRANSLATE("Make headtracking work at 75fps for a 20fps game. Enable this on 20fps games to fix judder.\nIf unsure, leave this unchecked.");
static wxString pullup30_desc = wxTRANSLATE("Make headtracking work at 75fps for a 30fps game. Enable this on 30fps games to fix judder.\nIf unsure, leave this unchecked.");
static wxString pullup60_desc = wxTRANSLATE("Make headtracking work at 75fps for a 60fps game. Enable this on 60fps games to fix judder.\nIf unsure, leave this unchecked.");
static wxString opcodewarning_desc = wxTRANSLATE("Turn off Opcode Warnings.  Will ignore errors in the Opcode Replay Buffer, allowing many games with minor problems to be played.  This should be turned on if trying to play a game with Opcode Replay enabled. Once all the bugs in the Opcode Buffer are fixed, this option will be removed!");
static wxString pullup20timewarp_desc = wxTRANSLATE("Timewarp headtracking up to 75fps for a 20fps game. Enable this on 20fps games to timewarp the headtracking to 75fps.\nIf unsure, leave this unchecked.");
static wxString pullup30timewarp_desc = wxTRANSLATE("Timewarp headtracking up to 75fps for a 30fps game. Enable this on 30fps games to timewarp the headtracking to 75fps.\nIf unsure, leave this unchecked.");
static wxString pullup60timewarp_desc = wxTRANSLATE("Timewarp headtracking up to 75fps for a 60fps game. Enable this on 60fps games to timewarp the headtracking to 75fps.\nIf unsure, leave this unchecked.");
static wxString timewarptweak_desc = wxTRANSLATE("How long before the expected Vsync the timewarped frame should be injected. Ideally this value should zero, but some configurations may benefit from an earlier injection.  Only used if 'Extra Timewarped Frames' is non-zero. If unsure, set this to 0.");
static wxString enablevr_desc = wxTRANSLATE("Enable Virtual Reality (if your HMD was detected when you started Dolphin).\n\nIf unsure, leave this checked.");
static wxString player_desc = wxTRANSLATE("During split-screen games, which player is wearing the Oculus Rift?\nPlayer 1 is top left, player 2 is top right, player 3 is bottom left, player 4 is bottom right.\nThe player in the Rift will only see their player's view.\n\nIf unsure, say Player 1.");
static wxString cameracontrol_desc = wxTRANSLATE("Let the game rotate the camera only in these axes, to prevent motion sickness.\nDoesn't work in all games.\n\nIf unsure, choose Yaw only.");
static wxString readpitch_desc = wxTRANSLATE("How many extra degrees pitch to add to the camera when reading camera angles.\nThis only has an effect when 'Read Camera Angles' is checked and 'Let the Game Camera Control' is not set to 'yaw, pitch, and roll'. It is for games where the read camera angle is looking at the floor.\nThis will normally be 0 or 90.\nIf unsure, leave this at 0 degrees.");
static wxString lowpersistence_desc = wxTRANSLATE("Use low persistence on DK2 to reduce motion blur when turning your head.\n\nIf unsure, leave this checked.");
static wxString dynamicpred_desc = wxTRANSLATE("\"Adjust prediction dynamically based on internally measured latency.\"\n\nIf unsure, leave this checked.");
static wxString nomirrortowindow_desc = wxTRANSLATE("Disable the mirrored window in direct mode.  Current tests show better performance with this unchecked and the mirrored window resized to 0 pixels by 0 pixels. Leave this unchecked.");
static wxString orientation_desc = wxTRANSLATE("Use orientation tracking.\n\nLeave this checked.");
static wxString magyaw_desc = wxTRANSLATE("Use the Rift's magnetometers to prevent yaw drift when out of camera range.\n\nIf unsure, leave this checked.");
static wxString position_desc = wxTRANSLATE("Use position tracking (both from camera and head/neck model).\n\nLeave this checked.");
static wxString chromatic_desc = wxTRANSLATE("Use chromatic aberration correction to prevent red and blue fringes at the edges of objects.\n\nIf unsure, leave this checked.");
static wxString timewarp_desc = wxTRANSLATE("Shift the warped display after rendering to correct for head movement during rendering.\n\nIf unsure, leave this checked.");
static wxString vignette_desc = wxTRANSLATE("Fade out the edges of the screen to make the screen sides look less harsh.\nNot needed on DK1.\n\nIf unsure, leave this unchecked.");
static wxString norestore_desc = wxTRANSLATE("Tell the Oculus SDK not to restore OpenGL state.\n\nIf unsure, leave this unchecked.");
static wxString flipvertical_desc = wxTRANSLATE("Flip the screen vertically.\n\nIf unsure, leave this unchecked.");
static wxString srgb_desc = wxTRANSLATE("\"Assume input images are in sRGB gamma-corrected color space.\"\n\nIf unsure, leave this unchecked.");
static wxString overdrive_desc = wxTRANSLATE("Try to fix true black smearing by overdriving brightness transitions.\n\nIf unsure, leave this unchecked.");
static wxString hqdistortion_desc = wxTRANSLATE("\"High-quality sampling of distortion buffer for anti-aliasing\".\n\nIf unsure, leave this unchecked.");
static wxString hudontop_desc = wxTRANSLATE("Always draw the HUD on top of everything else.\nUse this when you can't see the HUD because the world is covering it up.\n\nIf unsure, leave this unchecked.");
static wxString dontclearscreen_desc = wxTRANSLATE("Don't clear the screen every frame. This prevents some games from flashing.\n\nIf unsure, leave this unchecked.");
static wxString canreadcamera_desc = wxTRANSLATE("Read the camera angle from the angle of the first drawn 3D object.\nThis will only work in some games, but helps keep the ground horizontal.\n\nIf unsure, leave this unchecked.");
static wxString nearclip_desc = wxTRANSLATE("Always draw things which are close to (or far from) the camera.\nThis fixes the problem of things disappearing when you move your head close.\nThere can still be problems when two objects are in front of the near clipping plane, if they are drawn in the wrong order.\n\nIf unsure, leave this checked.");
static wxString showcontroller_desc = wxTRANSLATE("Show the razer hydra, wiimote, or gamecube controller inside the game world. Note: Only works in Direct3D for now. \n\nIf unsure, leave this unchecked.");
static wxString showhands_desc = wxTRANSLATE("Show your hands inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString showfeet_desc = wxTRANSLATE("Show your feet inside the game world.\nBased on your height in Oculus Configuration Utility.\n\nIf unsure, leave this unchecked.");
static wxString showgamecamera_desc = wxTRANSLATE("Show the location of the game's camera inside the game world.\nNormally this will be where your head is unless you move your head.\n\nIf unsure, leave this unchecked.");
static wxString showgamecamerafrustum_desc = wxTRANSLATE("Show the pyramid coming out of the game camera representing what the game thinks you can see.\n\nIf unsure, leave this unchecked.");
static wxString showsensorbar_desc = wxTRANSLATE("Show the virtual or real sensor bar inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString showtrackingcamera_desc = wxTRANSLATE("Show the Oculus Rift tracking camera inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString showtrackingvolume_desc = wxTRANSLATE("Show the pyramid coming out of the Rift's tracking camera representing where your head can be tracked.\n\nIf unsure, leave this unchecked.");
static wxString showbasestation_desc = wxTRANSLATE("Show the Razer Hydra base station inside the game world.\n\nIf unsure, leave this unchecked.");
static wxString hideskybox_desc = wxTRANSLATE("Don't draw the background or sky in some games.\nThat may reduce motion sickness by reducing vection in your periphery, but most games won't look as good.\n\nIf unsure, chose 'normal'.");
static wxString lockskybox_desc = wxTRANSLATE("Lock the game's background or sky to the real world in some games.\nTurning in-game will rotate the level but not the background. Oculus suggested this at Connect. It will reduce motion sickness at the cost of decreased immersion.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessprevention_desc = wxTRANSLATE("A list of options that can help reduce motion sickness:\nReduce FOV - Lower the FOV temporarily during certain actions selected below.\nBlack Screen - Blank the screen entirely during certain actions selected below.\n\nIf unsure, select 'none'.");
static wxString motionsicknessfov_desc = wxTRANSLATE("Set the restricted FOV to be used when the motion sickness reduction option is activated. A range somewhere between 80 to 40 degrees is ideal.");
static wxString motionsicknessalways_desc = wxTRANSLATE("Always enable the selected motion sickness reduction option.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessfreelook_desc = wxTRANSLATE("Enable the selected motion sickness reduction option during fast freelook movements.\n\nIf unsure, leave this unchecked.");
static wxString motionsickness2d_desc = wxTRANSLATE("Enable the selected motion sickness reduction option in 2D scenes, when there is no 3D world detected.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessleftstick_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the left joystick.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessrightstick_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the right joystick.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessdpad_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the D-Pad.\n\nIf unsure, leave this unchecked.");
static wxString motionsicknessir_desc = wxTRANSLATE("Enable the selected motion sickness reduction option when using the IR pointer.\n\nIf unsure, leave this unchecked.");
