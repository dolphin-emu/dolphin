// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "DolphinWX/WXInputBase.h"

#include "Core/Core.h"

#include "DolphinWX/ConfigVR.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/HotkeysXInput.h"

#include <wx/msgdlg.h>


BEGIN_EVENT_TABLE(CConfigVR, wxDialog)

EVT_CLOSE(CConfigVR::OnClose)
EVT_BUTTON(wxID_OK, CConfigVR::OnOk)

END_EVENT_TABLE()

CConfigVR::CConfigVR(wxWindow* parent, wxWindowID id, const wxString& title, 
	const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	, vconfig(g_Config)

{
	vconfig.LoadVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

	Bind(wxEVT_UPDATE_UI, &CConfigVR::OnUpdateUI, this);

	CreateGUIControls();

	UpdateDeviceComboBox();
}

CConfigVR::~CConfigVR()
{
}

namespace
{
	const float INPUT_DETECT_THRESHOLD = 0.55f;
}

// Used to restrict changing of some options while emulator is running
void CConfigVR::UpdateGUI()
{
	//if (Core::IsRunning())
	//{
		// Disable the Core stuff on GeneralPage
		//CPUThread->Disable();
		//SkipIdle->Disable();
		//EnableCheats->Disable();

		//CPUEngine->Disable();
		//_NTSCJ->Disable();
	//}

	device_cbox->SetValue(StrToWxStr(default_device.ToString())); //Update ComboBox
	
	int i = 0;

	//Update Buttons
	for (wxButton* button : m_Button_VRSettings)
	{
		SetButtonText(i,
			SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[i],
			SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[i],
			WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
			WxUtils::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]),
			HotkeysXInput::GetwxStringfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[i]));
		++i;
	}
}

#define VR_NUM_COLUMNS 2

void CConfigVR::CreateGUIControls()
{
	// Configuration controls sizes
	wxSize size(150, 20);
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxNotebook *Notebook = new wxNotebook(this, wxID_ANY);

	// -- VR --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, _("VR"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);

		// - vr
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 10);
		wxFlexGridSizer* const szr_options = new wxFlexGridSizer(4, 5, 10);

		// Scale
		{
			SettingNumber* const spin_scale = CreateNumber(page_vr, vconfig.fScale,
				wxGetTranslation(scale_desc), 0.001f, 100.0f, 0.01f);
			wxStaticText* label = new wxStaticText(page_vr, wxID_ANY, _("Scale Multiplier:   x"));

			spin_scale->SetToolTip(wxGetTranslation(scale_desc));
			label->SetToolTip(wxGetTranslation(scale_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_scale);
		}
		// Lean back angle
		{
			SettingNumber* const spin_lean = CreateNumber(page_vr, vconfig.fLeanBackAngle,
				wxGetTranslation(lean_desc), -180.0f, 180.0f, 1.0f);
			wxStaticText* label = new wxStaticText(page_vr, wxID_ANY, _("Lean Back Angle:"));

			spin_lean->SetToolTip(wxGetTranslation(lean_desc));
			label->SetToolTip(wxGetTranslation(lean_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_lean);
		}
		// Game Camera Control
		{
			checkbox_roll = CreateCheckBox(page_vr, _("Roll"), wxGetTranslation(stabilizeroll_desc), vconfig.bStabilizeRoll);
			checkbox_pitch = CreateCheckBox(page_vr, _("Pitch"), wxGetTranslation(stabilizepitch_desc), vconfig.bStabilizePitch);
			checkbox_yaw = CreateCheckBox(page_vr, _("Yaw"), wxGetTranslation(stabilizeyaw_desc), vconfig.bStabilizeYaw);
			checkbox_keyhole = CreateCheckBox(page_vr, _("Keyhole"), wxGetTranslation(keyhole_desc), vconfig.bKeyhole);
			checkbox_yaw->Bind(wxEVT_CHECKBOX, &CConfigVR::OnYawCheckbox, this);
			checkbox_keyhole->Bind(wxEVT_CHECKBOX, &CConfigVR::OnKeyholeCheckbox, this);

			wxStaticText* label_keyhole_width = new wxStaticText(page_vr, wxID_ANY, _("Keyhole Width:"));
			keyhole_width = CreateNumber(page_vr, vconfig.fKeyholeWidth, wxGetTranslation(keyholewidth_desc), 10.0f, 175.0f, 1.0f);
			keyhole_width->Enable(vconfig.bKeyhole);

			wxStaticText* label_snap = new wxStaticText(page_vr, wxID_ANY, _("Keyhole Snap:"));
			checkbox_keyhole_snap = CreateCheckBox(page_vr, _("Keyhole Snap"), wxGetTranslation(keyholesnap_desc), vconfig.bKeyholeSnap);
			checkbox_keyhole_snap->Bind(wxEVT_CHECKBOX, &CConfigVR::OnKeyholeSnapCheckbox, this);

			wxStaticText* label_snap_size = new wxStaticText(page_vr, wxID_ANY, _("Keyhole Snap Size:"));
			keyhole_snap_size = CreateNumber(page_vr, vconfig.fKeyholeSnapSize, wxGetTranslation(keyholesnapsize_desc), 10.0f, 120.0f, 1.0f);
			keyhole_snap_size->Enable(vconfig.bKeyholeSnap);

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Stabilize: ")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_roll, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_pitch, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_yaw, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Keyhole: ")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_keyhole, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(label_keyhole_width, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(keyhole_width);
			szr_vr->Add(label_snap, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_keyhole_snap, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(label_snap_size, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(keyhole_snap_size);
		}
		// VR Player
		{
			const wxString vr_choices[] = { _("Player 1"), _("Player 2"), _("Player 3"), _("Player 4") };

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Player wearing HMD:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iVRPlayer, wxGetTranslation(player_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iVRPlayer);
			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, _(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, _(""));
			szr_vr->Add(spacer1, 1, 0, 0);
			szr_vr->Add(spacer2, 1, 0, 0);
		}
		{
			szr_vr->Add(CreateCheckBox(page_vr, _("Enable VR"), wxGetTranslation(enablevr_desc), vconfig.bEnableVR));
			szr_vr->Add(CreateCheckBox(page_vr, _("Low Persistence"), wxGetTranslation(lowpersistence_desc), vconfig.bLowPersistence));
			szr_vr->Add(CreateCheckBox(page_vr, _("Dynamic Prediction"), wxGetTranslation(dynamicpred_desc), vconfig.bDynamicPrediction));
			szr_vr->Add(CreateCheckBox(page_vr, _("Orientation Tracking"), wxGetTranslation(orientation_desc), vconfig.bOrientationTracking));
			szr_vr->Add(CreateCheckBox(page_vr, _("Magnetic Yaw"), wxGetTranslation(magyaw_desc), vconfig.bMagYawCorrection));
			szr_vr->Add(CreateCheckBox(page_vr, _("Position Tracking"), wxGetTranslation(position_desc), vconfig.bPositionTracking));
			szr_vr->Add(CreateCheckBox(page_vr, _("Chromatic Aberration"), wxGetTranslation(chromatic_desc), vconfig.bChromatic));
			szr_vr->Add(CreateCheckBox(page_vr, _("Timewarp"), wxGetTranslation(timewarp_desc), vconfig.bTimewarp));
			szr_vr->Add(CreateCheckBox(page_vr, _("Vignette"), wxGetTranslation(vignette_desc), vconfig.bVignette));
			szr_vr->Add(CreateCheckBox(page_vr, _("Don't Restore"), wxGetTranslation(norestore_desc), vconfig.bNoRestore));
			szr_vr->Add(CreateCheckBox(page_vr, _("Flip Vertical"), wxGetTranslation(flipvertical_desc), vconfig.bFlipVertical));
			szr_vr->Add(CreateCheckBox(page_vr, _("sRGB"), wxGetTranslation(srgb_desc), vconfig.bSRGB));
			szr_vr->Add(CreateCheckBox(page_vr, _("Overdrive"), wxGetTranslation(overdrive_desc), vconfig.bOverdrive));
			szr_vr->Add(CreateCheckBox(page_vr, _("HQ Distortion"), wxGetTranslation(hqdistortion_desc), vconfig.bHqDistortion));
			szr_vr->Add(CreateCheckBox(page_vr, _("Direct Mode - Disable Mirroring"), wxGetTranslation(nomirrortowindow_desc), vconfig.bNoMirrorToWindow));

#ifdef OCULUSSDK042
			szr_vr->Add(async_timewarp_checkbox = CreateCheckBox(page_vr, _("Asynchronous timewarp"), wxGetTranslation(async_desc), SConfig::GetInstance().m_LocalCoreStartupParameter.bAsynchronousTimewarp));
#endif
			szr_vr->Add(CreateCheckBox(page_vr, _("Disable Near-Clipping"), wxGetTranslation(nearclip_desc), vconfig.bDisableNearClipping));
		}

		// Opcode Replay Buffer GUI Options
		wxFlexGridSizer* const szr_opcode = new wxFlexGridSizer(4, 10, 10);
		{
			spin_replay_buffer = new U32Setting(page_vr, _("Extra Opcode Replay Frames:"), vconfig.iExtraVideoLoops, 0, 100000);
			RegisterControl(spin_replay_buffer, replaybuffer_desc);
			spin_replay_buffer->SetToolTip(replaybuffer_desc);
			spin_replay_buffer->SetValue(vconfig.iExtraVideoLoops);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Extra Opcode Replay Frames:"));

			label->SetToolTip(wxGetTranslation(replaybuffer_desc));
			szr_opcode->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(spin_replay_buffer);
			spin_replay_buffer->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp && !vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);

			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, _(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, _(""));
			szr_opcode->Add(spacer1, 1, 0, 0);
			szr_opcode->Add(spacer2, 1, 0, 0);

			//spin_replay_buffer_divider = new U32Setting(page_vr, _("Extra Opcode Replay Frame Divider:"), vconfig.iExtraVideoLoopsDivider, 0, 100000);
			//RegisterControl(spin_replay_buffer_divider, replaybuffer_desc);
			//spin_replay_buffer_divider->SetToolTip(replaybuffer_desc);
			//spin_replay_buffer_divider->SetValue(vconfig.iExtraVideoLoopsDivider);
			//wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Extra Opcode Replay Frame Divider:"));

			//label->SetToolTip(wxGetTranslation(replaybuffer_desc));
			//szr_opcode->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			//szr_opcode->Add(spin_replay_buffer_divider);
			//spin_replay_buffer_divider->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp && !vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);

			szr_opcode->Add(new wxStaticText(page_vr, wxID_ANY, _("Quick Opcode Replay Settings:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20 = CreateCheckBox(page_vr, _("Pullup 20fps to 75fps"), wxGetTranslation(pullup20_desc), vconfig.bPullUp20fps);
			checkbox_pullup30 = CreateCheckBox(page_vr, _("Pullup 30fps to 75fps"), wxGetTranslation(pullup30_desc), vconfig.bPullUp30fps);
			checkbox_pullup60 = CreateCheckBox(page_vr, _("Pullup 60fps to 75fps"), wxGetTranslation(pullup60_desc), vconfig.bPullUp60fps);
			SettingCheckBox* checkbox_disable_warnings = CreateCheckBox(page_vr, _("Disable Opcode Warnings"), wxGetTranslation(opcodewarning_desc), vconfig.bOpcodeWarningDisable);
			szr_opcode->Add(checkbox_pullup20, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(checkbox_pullup30, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_opcode->Add(checkbox_pullup60, 1, wxALIGN_CENTER_VERTICAL, 0);
			wxStaticText* spacer3 = new wxStaticText(page_vr, wxID_ANY, _(""));
			szr_opcode->Add(spacer3, 1, 0, 0);
			szr_opcode->Add(checkbox_disable_warnings, 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup30->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup60->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup20->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp);
			checkbox_pullup30->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp);
			checkbox_pullup60->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp);
		}

		// Synchronous Timewarp GUI Options
		wxFlexGridSizer* const szr_timewarp = new wxFlexGridSizer(4, 10, 10);
		{
			spin_extra_frames = new U32Setting(page_vr, _("Extra Timewarped Frames:"), vconfig.iExtraFrames, 0, 4);
			RegisterControl(spin_extra_frames, extraframes_desc);
			spin_extra_frames->SetToolTip(extraframes_desc);
			spin_extra_frames->SetValue(vconfig.iExtraFrames);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Extra Timewarped Frames:"));

			label->SetToolTip(wxGetTranslation(extraframes_desc));
			szr_timewarp->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(spin_extra_frames);
			spin_extra_frames->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp && !vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);
			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, _(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, _(""));
			szr_timewarp->Add(spacer1, 1, 0, 0);
			szr_timewarp->Add(spacer2, 1, 0, 0);
		}
		{
			spin_timewarp_tweak = CreateNumber(page_vr, vconfig.fTimeWarpTweak, _("Timewarp VSync Tweak:"), -1.0f, 1.0f, 0.0001f);
			RegisterControl(spin_timewarp_tweak, timewarptweak_desc);
			spin_timewarp_tweak->SetToolTip(timewarptweak_desc);
			spin_timewarp_tweak->SetValue(vconfig.fTimeWarpTweak);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Timewarp VSync Tweak:"));
			spin_timewarp_tweak->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);

			label->SetToolTip(wxGetTranslation(timewarptweak_desc));
			szr_timewarp->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(spin_timewarp_tweak);
			wxStaticText* spacer1 = new wxStaticText(page_vr, wxID_ANY, _(""));
			wxStaticText* spacer2 = new wxStaticText(page_vr, wxID_ANY, _(""));
			szr_timewarp->Add(spacer1, 1, 0, 0);
			szr_timewarp->Add(spacer2, 1, 0, 0);

			szr_timewarp->Add(new wxStaticText(page_vr, wxID_ANY, _("Timewarp Pull-Up:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20_timewarp = CreateCheckBox(page_vr, _("Timewarp 20fps to 75fps"), wxGetTranslation(pullup20timewarp_desc), vconfig.bPullUp20fpsTimewarp);
			checkbox_pullup30_timewarp = CreateCheckBox(page_vr, _("Timewarp 30fps to 75fps"), wxGetTranslation(pullup30timewarp_desc), vconfig.bPullUp30fpsTimewarp);
			checkbox_pullup60_timewarp = CreateCheckBox(page_vr, _("Timewarp 60fps to 75fps"), wxGetTranslation(pullup60timewarp_desc), vconfig.bPullUp60fpsTimewarp);
			szr_timewarp->Add(checkbox_pullup20_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(checkbox_pullup30_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_timewarp->Add(checkbox_pullup60_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup30_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup60_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup20_timewarp->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);
			checkbox_pullup30_timewarp->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);
			checkbox_pullup60_timewarp->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);
		}

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("All Games"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);
		wxStaticBoxSizer* const group_opcode = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("Opcode Replay (BETA) - Note: Enable 'Idle Skipping' in Config Tab"));
		group_opcode->Add(szr_opcode, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_opcode, 0, wxEXPAND | wxALL, 5);
		wxStaticBoxSizer* const group_timewarp = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("Synchronous Timewarp"));
		group_timewarp->Add(szr_timewarp, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_timewarp, 0, wxEXPAND | wxALL, 5);

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	// -- VR Game --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, _("VR Game"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);

		// - vr
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 5);

		// Units Per Metre
		{
			SettingNumber *const spin_scale = CreateNumber(page_vr, vconfig.fUnitsPerMetre,
				wxGetTranslation(temp_desc), 0.0000001f, 10000000, 0.5f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Units Per Metre:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_scale);
		}
		// HUD 3D Items Closer (3D items drawn on the HUD, like A button in Zelda 64)
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fHud3DCloser,
				wxGetTranslation(temp_desc), 0.0f, 1.0f, 0.5f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("HUD 3D Items Closer:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// HUD distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fHudDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("HUD Distance:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// HUD thickness
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fHudThickness,
				wxGetTranslation(temp_desc), 0, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("HUD Thickness:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera forward
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fCameraForward,
				wxGetTranslation(temp_desc), -10000, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Camera Forward:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fCameraPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Camera Pitch:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Aim distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fAimDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Aim Distance:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Height
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenHeight,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("2D Screen Height:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("2D Screen Distance:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Thickness
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenThickness,
				wxGetTranslation(temp_desc), 0, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("2D Screen Thickness:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen Up
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenUp,
				wxGetTranslation(temp_desc), -10000, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("2D Screen Up:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Screen pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fScreenPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("2D Screen Pitch:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Min FOV
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fMinFOV,
				wxGetTranslation(temp_desc), 0, 179, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Min HFOV:"));
			label->SetToolTip(wxGetTranslation(minfov_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera read pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fReadPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Camera Read Pitch:"));
			label->SetToolTip(wxGetTranslation(readpitch_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}

		szr_vr->Add(CreateCheckBox(page_vr, _("HUD on Top"), wxGetTranslation(hudontop_desc), vconfig.bHudOnTop));
		szr_vr->Add(CreateCheckBox(page_vr, _("Don't Clear Screen"), wxGetTranslation(dontclearscreen_desc), vconfig.bDontClearScreen));
		szr_vr->Add(CreateCheckBox(page_vr, _("Read Camera Angles"), wxGetTranslation(canreadcamera_desc), vconfig.bCanReadCameraAngles));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("For This Game Only"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		wxButton* const btn_save = new wxButton(page_vr, wxID_ANY, _("Save"));
		btn_save->Bind(wxEVT_BUTTON, &CConfigVR::Event_ClickSave, this);
		szr_vr->Add(btn_save, 1, wxALIGN_CENTER_VERTICAL, 0);
		wxButton* const btn_reset = new wxButton(page_vr, wxID_ANY, _("Reset to Defaults"));
		btn_reset->Bind(wxEVT_BUTTON, &CConfigVR::Event_ClickReset, this);
		szr_vr->Add(btn_reset, 1, wxALIGN_CENTER_VERTICAL, 0);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID == "")
		{
			btn_save->Disable();
			btn_reset->Disable();
			page_vr->Disable();
		}

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	// -- Avatar --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, _("Avatar"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);

		// - vr
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 5);

		szr_vr->Add(CreateCheckBox(page_vr, _("Show Controller"), wxGetTranslation(showcontroller_desc), vconfig.bShowController));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Hands"), wxGetTranslation(showhands_desc), vconfig.bShowHands));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Feet"), wxGetTranslation(showfeet_desc), vconfig.bShowFeet));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Sensor Bar"), wxGetTranslation(showsensorbar_desc), vconfig.bShowSensorBar));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Game Camera"), wxGetTranslation(showgamecamera_desc), vconfig.bShowGameCamera));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Game Camera Frustum"), wxGetTranslation(showgamecamerafrustum_desc), vconfig.bShowGameFrustum));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Tracking Camera"), wxGetTranslation(showtrackingcamera_desc), vconfig.bShowTrackingCamera));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Tracking Volume"), wxGetTranslation(showtrackingvolume_desc), vconfig.bShowTrackingVolume));
		//szr_vr->Add(CreateCheckBox(page_vr, _("Show Hydra Base Station"), wxGetTranslation(showbasestation_desc), vconfig.bShowBaseStation));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("Show Avatar"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	// -- Motion Sickness --
	{
		wxPanel* const page_vr = new wxPanel(Notebook, -1);
		Notebook->AddPage(page_vr, _("Motion Sickness"));
		wxBoxSizer* const szr_vr_main = new wxBoxSizer(wxVERTICAL);
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(4, 5, 5);

		// Sky / Background
		{
			const wxString vr_choices[] = { _("Normal"), _("Hide") };
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Sky / Background:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iMotionSicknessSkybox, wxGetTranslation(hideskybox_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iMotionSicknessSkybox);
		}
		// Motion Sickness Prevention Method
		{
			const wxString vr_choices[] = { _("None"), _("Reduce FOV"), _("Black Screen")};
			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Motion Sickness Prevention Method:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iMotionSicknessMethod, wxGetTranslation(motionsicknessprevention_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iMotionSicknessMethod);
		}
		// Motion Sickness FOV
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fMotionSicknessFOV,
				wxGetTranslation(motionsicknessfov_desc), 1, 179, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Reduced FOV:"));
			label->SetToolTip(wxGetTranslation(motionsicknessfov_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}

		szr_vr->Add(CreateCheckBox(page_vr, _("Always"), wxGetTranslation(motionsicknessalways_desc), vconfig.bMotionSicknessAlways));
		szr_vr->Add(CreateCheckBox(page_vr, _("During Freelook"), wxGetTranslation(motionsicknessfreelook_desc), vconfig.bMotionSicknessFreelook));
		szr_vr->Add(CreateCheckBox(page_vr, _("On 2D Screens"), wxGetTranslation(motionsickness2d_desc), vconfig.bMotionSickness2D));
		szr_vr->Add(CreateCheckBox(page_vr, _("With Left Stick"), wxGetTranslation(motionsicknessleftstick_desc), vconfig.bMotionSicknessLeftStick));
		szr_vr->Add(CreateCheckBox(page_vr, _("With Right Stick"), wxGetTranslation(motionsicknessrightstick_desc), vconfig.bMotionSicknessRightStick));
		szr_vr->Add(CreateCheckBox(page_vr, _("With D-Pad"), wxGetTranslation(motionsicknessdpad_desc), vconfig.bMotionSicknessDPad));
		szr_vr->Add(CreateCheckBox(page_vr, _("With IR Pointer"), wxGetTranslation(motionsicknessir_desc), vconfig.bMotionSicknessIR));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("Motion Sickness"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		szr_vr_main->AddStretchSpacer();
		CreateDescriptionArea(page_vr, szr_vr_main);
		page_vr->SetSizerAndFit(szr_vr_main);
	}

	const wxString pageNames[] =
	{
		_("VR Hotkeys"),
		_("VR Instructions")
	};

	const wxString VRText[] =
	{
		_("Reset Camera"),
		_("Freelook Move In"),
		_("Freelook Move Out"),
		_("Freelook Move Left"),
		_("Freelook Move Right"),
		_("Freelook Move Up"),
		_("Freelook Move Down"),

		_("Permanent Camera Forward"),
		_("Permanent Camera Backward"),
		//_("Permanent Camera Up"),
		//_("Permanent Camera Down"),
		_("Less Units Per Metre"),
		_("More Units Per Metre"),
		_("Global Larger Scale"),
		_("Global Smaller Scale"),
		_("Tilt Camera Up"),
		_("Tilt Camera Down"),

		_("HUD Forward"),
		_("HUD Backward"),
		_("HUD Thicker"),
		_("HUD Thinner"),
		_("HUD 3D Items Closer"),
		_("HUD 3D Items Further"),

		_("2D Screen Larger"),
		_("2D Screen Smaller"),
		_("2D Camera Forward"),
		_("2D Camera Backward"),
		//_("2D Screen Left"), //Doesn't exist right now?
		//_("2D Screen Right"), //Doesn't exist right now?
		_("2D Camera Up"),
		_("2D Camera Down"),
		_("2D Camera Tilt Up"),
		_("2D Camera Tilt Down"),
		_("2D Screen Thicker"),
		_("2D Screen Thinner"),
		
	};

	const int page_breaks[3] = {VR_POSITION_RESET, NUM_VR_HOTKEYS, NUM_VR_HOTKEYS};

	button_already_clicked = false; //Used to determine whether a button has already been clicked.  If it has, don't allow more buttons to be clicked.

	for (int j = 0; j < 2; ++j)
	{
		wxPanel *Page = new wxPanel(Notebook, wxID_ANY);
		Notebook->AddPage(Page, pageNames[j]);

		wxGridBagSizer *sVRKeys = new wxGridBagSizer();

		// -- VR Hotkeys --
		// Header line
		if (j == 0)
		{
			for (int i = 0; i < VR_NUM_COLUMNS; ++i)
			{
				wxBoxSizer *HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
				wxStaticText *StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Action"));
				HeaderSizer->Add(StaticTextHeader, 1, wxALL, 2);
				StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Key"), wxDefaultPosition, size);
				HeaderSizer->Add(StaticTextHeader, 0, wxALL, 2);
				sVRKeys->Add(HeaderSizer, wxGBPosition(0, i), wxDefaultSpan, wxEXPAND | wxLEFT, (i > 0) ? 30 : 1);
			}
		}

		int column_break = (page_breaks[j+1] + page_breaks[j] + 1) / 2;

		for (int i = page_breaks[j]; i < page_breaks[j+1]; ++i)
		{
			// Text for the action
			wxStaticText *stHotkeys = new wxStaticText(Page, wxID_ANY, VRText[i]);

			// Key selection button
			m_Button_VRSettings[i] = new wxButton(Page, i, wxEmptyString, wxDefaultPosition, size);

			m_Button_VRSettings[i]->SetFont(m_SmallFont);
			m_Button_VRSettings[i]->SetToolTip(_("Left click to change the controlling key (assign space to clear).\nMiddle click to clear.\nRight click to choose XInput Combinations (if a gamepad is detected)"));
			SetButtonText(i, 
				SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[i],
				SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[i],
				WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
				WxUtils::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]),
				HotkeysXInput::GetwxStringfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[i]));

			m_Button_VRSettings[i]->Bind(wxEVT_BUTTON, &CConfigVR::DetectControl, this);
			m_Button_VRSettings[i]->Bind(wxEVT_MIDDLE_DOWN, &CConfigVR::ClearControl, this);
			m_Button_VRSettings[i]->Bind(wxEVT_RIGHT_UP, &CConfigVR::ConfigControl, this);

			wxBoxSizer *sVRKey = new wxBoxSizer(wxHORIZONTAL);
			sVRKey->Add(stHotkeys, 1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
			sVRKey->Add(m_Button_VRSettings[i], 0, wxALL, 2);
			sVRKeys->Add(sVRKey,
				wxGBPosition((i < column_break) ? i - page_breaks[j] + 1 : i - column_break + 1,
				(i < column_break) ? 0 : 1),
				wxDefaultSpan, wxEXPAND | wxLEFT, (i < column_break) ? 1 : 30);
		}

		if (j == 0) 
		{
			//Create "Device" box, and all buttons/options within it/
			wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxHORIZONTAL, Page, _("Device"));

			device_cbox = new wxComboBox(Page, wxID_ANY, "", wxDefaultPosition, wxSize(64, -1));
			device_cbox->ToggleWindowStyle(wxTE_PROCESS_ENTER);

			wxButton* refresh_button = new wxButton(Page, wxID_ANY, _("Refresh"), wxDefaultPosition, wxSize(60, -1));

			device_cbox->Bind(wxEVT_COMBOBOX, &CConfigVR::SetDevice, this);
			device_cbox->Bind(wxEVT_TEXT_ENTER, &CConfigVR::SetDevice, this);
			refresh_button->Bind(wxEVT_BUTTON, &CConfigVR::RefreshDevices, this);

			device_sbox->Add(device_cbox, 10, wxLEFT | wxRIGHT, 3);
			device_sbox->Add(refresh_button, 3, wxLEFT | wxRIGHT, 3);

			//Create "Options" box, and all buttons/options within it.
			wxStaticBoxSizer* const options_sbox = new wxStaticBoxSizer(wxHORIZONTAL, Page, _("Options"));
			wxGridBagSizer* const options_gszr = new wxGridBagSizer(3, 3);
			options_sbox->Add(options_gszr, 1, wxALIGN_CENTER_VERTICAL, 3);

			// Free Look Sensitivity
			SettingNumber *const spin_freelook_sensitivity = CreateNumber(Page, vconfig.fFreeLookSensitivity,
				wxGetTranslation(temp_desc), 0.0001f, 10000, 0.05f);
			wxStaticText *spin_freelook_scale_label = new wxStaticText(Page, wxID_ANY, _(" Free Look Sensitivity: "));
			spin_freelook_sensitivity->SetToolTip(_("Scales the rate at which Camera Forward/Backwards/Left/Right/Up/Down move per key or button press."));
			spin_freelook_scale_label->SetToolTip(_("Scales the rate at which Camera Forward/Backwards/Left/Right/Up/Down move per key or button press."));

			wxCheckBox  *xInputPollEnableCheckbox = new wxCheckBox(Page, wxID_ANY, _("Enable XInput/DInput Polling"), wxDefaultPosition, wxDefaultSize);
			xInputPollEnableCheckbox->Bind(wxEVT_CHECKBOX, &CConfigVR::OnXInputPollCheckbox, this);
			xInputPollEnableCheckbox->SetToolTip(_("Check to enable XInput polling during game emulation. Uncheck to disable."));
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeysXInput)
			{
				xInputPollEnableCheckbox->SetValue(true);
			}

			options_gszr->Add(spin_freelook_scale_label, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL, 3);
			options_gszr->Add(spin_freelook_sensitivity, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL, 3);
			options_gszr->Add(xInputPollEnableCheckbox, wxGBPosition(0, 3), wxDefaultSpan, wxALL, 3);

			//Create "VR Camera Controls" and add keys to it.
			wxStaticBoxSizer *vr_camera_controls_box = new wxStaticBoxSizer(wxVERTICAL, Page, _("VR Camera Controls"));
			vr_camera_controls_box->Add(sVRKeys);

			//Add all boxes to page.
			wxBoxSizer* const sPage = new wxBoxSizer(wxVERTICAL);
			sPage->Add(device_sbox, 0, wxEXPAND | wxALL, 5);
			sPage->Add(options_sbox, 0, wxEXPAND | wxALL, 5);
			sPage->Add(vr_camera_controls_box, 0, wxEXPAND | wxALL, 5);
			Page->SetSizer(sPage);
		}

		if (j == 1)
		{
			//Create "Device" box, and all buttons/options within it/
			wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer(wxHORIZONTAL, Page, _("Instructions"));

			wxStaticText* const instruction_box = new wxStaticText(Page, wxID_ANY,
				_("Dolphin VR has a lot of options and it can be confusing to set them all correctly. Here's a quick setup "
				"guide to help.\n\nDirect mode is working, but the performance on many games is poor. It is recommended to run in "
				"extended mode! If you do run in direct mode, make sure Aero is enabled. Some games are now working judder free, but "
				"many require 'dual core determinism' set to 'fake-completion', 'synchronize GPU thread' to be enabled, or dual core mode "
				"to be disabled. The mirrored window must also "
				"be enabled and manually resized to 0 pixels by 0 pixles. Testing has been limited so your mileage may vary."
				"\n\nIn the 'Config' tab, 'Framelimit' should be set to match your Rift's refresh rate (most likely 75fps). "
				"Games will run 25% too fast, but this will avoid judder. There are two options available to bring the game frame rate back down to "
				"normal speed. The first is the 'Opcode Replay Buffer', which rerenders game frames so headtracking runs at 75fps forcing "
				"the game to maintain its normal speed.  This feature works in around 75% of games, and is the preferred "
				"method.  Please leave 'Enable Idle Skipping' checked, as it is required for many games when the 'Opcode Replay Buffer' "
				"is enabled. 'Synchronous Timewarp' smudges the image to fake head-tracking at a higher rate.  This can be tried if 'Opcode Replay Buffer' fails "
				"but sometimes introduces artifacts and may judder depending on the game. As a last resort, the the Rift can be "
				"set to run at 60hz from your AMD/Nvidia control panel and still run with low persistence, but flickering can be seen "
				"in bright scenes. Different games run at differnet frame-rates, so choose the correct opcode replay/timewarp setting for the game.\n\n"
				"Under Graphics->Hacks->EFB Copies, 'Disable' and 'Remove Blank EFB Copy Box' can fix some games "
				"that display black, or large black boxes that can't be removed by the 'Hide Objects Codes'. It can also cause "
				"corruption in some games, so only enable it if it's needed.\n\n"
				"Right-clicking on each game will give you the options to adjust VR settings, hide rendered objects, and (in "
				"a limited number of games) disable culling through the use of included AR codes. Culling codes put extra strain "
				"on the emulated CPU as well as your PC.  You may need to override the Dolphin CPU clockspeed under "
				"the advanced section of the 'config' tab to maintain a full game speed. This will only help if gamecube/wii CPU is "
				"not fast enough to render the game without culling. "
				"Objects such as fake 16:9 bars can be removed from the game.  Some games already have hide objects codes, "
				"so make sure to check them if you would like the object removed.  You can find your own codes by starting "
				"with an 8-bit code and clicking the up button until the object disappears.  Then move to 16bit, add two zeros "
				"to the right side of your 8bit code, and continue hitting the up button until the object disappears again. "
				"Continue until you have a long enough code for it to be unique.\n\nTake time to set "
				"the VR-Hotkeys. You can create combinations for XInput by right clicking on the buttons. Remember you can also set "
				"a the gamecube/wii controller emulation to not happen during a certain button press, so setting freelook to 'Left "
				"Bumper + Right Analog Stick' and the gamecube C-Stick to '!Left Bumper + Right Analog Stick' works great. If you're "
				"changing the scale in game, use the 'global scale' instead of 'units per metre' to maintain a consistent free look step size.\n\n"
				"Enjoy and watch for new updates!"));

			instruction_box->Wrap(630);

			device_sbox->Add(instruction_box, wxEXPAND | wxALL);

			wxBoxSizer* const sPage = new wxBoxSizer(wxVERTICAL);
			sPage->Add(device_sbox, 0, wxEXPAND | wxALL, 5);
			Page->SetSizer(sPage);
		}
	}

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(Notebook, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);
	SetSizerAndFit(sMainSizer);
	SetFocus();

}

SettingCheckBox* CConfigVR::CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingCheckBox* const cb = new SettingCheckBox(parent, label, wxString(), setting, reverse, style);
	RegisterControl(cb, description);
	return cb;
}

SettingChoice* CConfigVR::CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num, const wxString choices[], long style)
{
	SettingChoice* const ch = new SettingChoice(parent, setting, wxString(), num, choices, style);
	RegisterControl(ch, description);
	return ch;
}

SettingRadioButton* CConfigVR::CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingRadioButton* const rb = new SettingRadioButton(parent, label, wxString(), setting, reverse, style);
	RegisterControl(rb, description);
	return rb;
}

SettingNumber* CConfigVR::CreateNumber(wxWindow* parent, float &setting, const wxString& description, float min, float max, float inc, long style)
{
	SettingNumber* const sn = new SettingNumber(parent, wxString(), setting, min, max, inc, style);
	RegisterControl(sn, description);
	return sn;
}

/* Use this to register descriptions for controls which have NOT been created using the Create* functions from above */
wxControl* CConfigVR::RegisterControl(wxControl* const control, const wxString& description)
{
	ctrl_descs.insert(std::pair<wxWindow*, wxString>(control, description));
	control->Bind(wxEVT_ENTER_WINDOW, &CConfigVR::Evt_EnterControl, this);
	control->Bind(wxEVT_LEAVE_WINDOW, &CConfigVR::Evt_LeaveControl, this);
	return control;
}

void CConfigVR::Evt_EnterControl(wxMouseEvent& ev)
{
	// TODO: Re-Fit the sizer if necessary!

	// Get settings control object from event
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;

	// look up description text object from the control's parent (which is the wxPanel of the current tab)
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	// look up the description of the selected control and assign it to the current description text object's label
	descr_text->SetLabel(ctrl_descs[ctrl]);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);

	ev.Skip();
}

// TODO: Don't hardcode the size of the description area via line breaks
#define DEFAULT_DESC_TEXT _("Move the mouse pointer over an option to display a detailed description.\n\n\n\n\n\n\n")
void CConfigVR::Evt_LeaveControl(wxMouseEvent& ev)
{
	// look up description text control and reset its label
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	descr_text->SetLabel(DEFAULT_DESC_TEXT);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);
	ev.Skip();
}

void CConfigVR::CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer)
{
	// Create description frame
	wxStaticBoxSizer* const desc_sizer = new wxStaticBoxSizer(wxVERTICAL, page, _("Description"));
	sizer->Add(desc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Need to call SetSizerAndFit here, since we don't want the description texts to change the dialog width
	page->SetSizerAndFit(sizer);

	// Create description text
	wxStaticText* const desc_text = new wxStaticText(page, wxID_ANY, DEFAULT_DESC_TEXT);
	desc_text->Wrap(desc_sizer->GetSize().x - 20);
	desc_sizer->Add(desc_text, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Store description text object for later lookup
	desc_texts.insert(std::pair<wxWindow*, wxStaticText*>(page, desc_text));
}

// On Keyhole Checkbox Click
void CConfigVR::OnKeyholeCheckbox(wxCommandEvent& event)
{
	if (checkbox_keyhole->IsChecked())
	{
		vconfig.bKeyhole = true;
		keyhole_width->Enable();

		checkbox_yaw->SetValue(true);
		vconfig.bStabilizeYaw = true;
	}
	else
	{
		vconfig.bKeyhole = false;
		keyhole_width->Disable();

		checkbox_keyhole_snap->SetValue(false);
		vconfig.bKeyholeSnap = false;
		keyhole_snap_size->Disable();
	}
}

// On Keyhole Checkbox Click
void CConfigVR::OnKeyholeSnapCheckbox(wxCommandEvent& event)
{
	if (checkbox_keyhole_snap->IsChecked())
	{
		keyhole_snap_size->Enable();
		vconfig.bKeyholeSnap = true;
		checkbox_yaw->SetValue(true);
		checkbox_keyhole->SetValue(true);
		keyhole_width->Enable();
		vconfig.bStabilizeYaw = true;
		vconfig.bKeyhole = true;
	}
	else
	{
		keyhole_snap_size->Disable();
		vconfig.bKeyholeSnap = false;
	}
}

void CConfigVR::OnYawCheckbox(wxCommandEvent& event)
{
	if (checkbox_yaw->IsChecked())
	{
		vconfig.bStabilizeYaw = true;
	}
	else
	{
		checkbox_keyhole->SetValue(false);
		checkbox_keyhole_snap->SetValue(false);
		vconfig.bKeyholeSnap = false;
		vconfig.bStabilizeYaw = false;
		vconfig.bKeyhole = false;
		keyhole_width->Disable();
		keyhole_snap_size->Disable();
	}
}

// On Pullup Checkbox Click
void CConfigVR::OnPullupCheckbox(wxCommandEvent& event)
{
	spin_replay_buffer->Enable(!checkbox_pullup20_timewarp->IsChecked() && !checkbox_pullup30_timewarp->IsChecked() && !checkbox_pullup60_timewarp->IsChecked() && !checkbox_pullup20->IsChecked() && !checkbox_pullup30->IsChecked() && !checkbox_pullup60->IsChecked());
	//spin_replay_buffer_divider->Enable(!checkbox_pullup20_timewarp->IsChecked() && !checkbox_pullup30_timewarp->IsChecked() && !checkbox_pullup60_timewarp->IsChecked() && !checkbox_pullup20->IsChecked() && !checkbox_pullup30->IsChecked() && !checkbox_pullup60->IsChecked());
	spin_extra_frames->Enable(!checkbox_pullup20_timewarp->IsChecked() && !checkbox_pullup30_timewarp->IsChecked() && !checkbox_pullup60_timewarp->IsChecked() && !checkbox_pullup20->IsChecked() && !checkbox_pullup30->IsChecked() && !checkbox_pullup60->IsChecked());
	spin_timewarp_tweak->Enable(!checkbox_pullup20->IsChecked() && !checkbox_pullup30->IsChecked() && !checkbox_pullup60->IsChecked());

	checkbox_pullup20->Enable(!(checkbox_pullup20_timewarp->IsChecked() || checkbox_pullup30_timewarp->IsChecked() || checkbox_pullup60_timewarp->IsChecked()));
	checkbox_pullup30->Enable(!(checkbox_pullup20_timewarp->IsChecked() || checkbox_pullup30_timewarp->IsChecked() || checkbox_pullup60_timewarp->IsChecked()));
	checkbox_pullup60->Enable(!(checkbox_pullup20_timewarp->IsChecked() || checkbox_pullup30_timewarp->IsChecked() || checkbox_pullup60_timewarp->IsChecked()));

	checkbox_pullup20_timewarp->Enable(!(checkbox_pullup20->IsChecked() || checkbox_pullup30->IsChecked() || checkbox_pullup60->IsChecked()));
	checkbox_pullup30_timewarp->Enable(!(checkbox_pullup20->IsChecked() || checkbox_pullup30->IsChecked() || checkbox_pullup60->IsChecked()));
	checkbox_pullup60_timewarp->Enable(!(checkbox_pullup20->IsChecked() || checkbox_pullup30->IsChecked() || checkbox_pullup60->IsChecked()));

	event.Skip();
}

//Poll devices available and put them in the device combo box.
void CConfigVR::UpdateDeviceComboBox()
{
	ciface::Core::DeviceQualifier dq;
	device_cbox->Clear();

	int i = 0;
	for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
	{
		dq.FromDevice(d);
		device_cbox->Append(StrToWxStr(dq.ToString())); //Put Name of Device into Combo Box
		if (i == 0)
		{
			device_cbox->SetValue(StrToWxStr(dq.ToString())); //Show first device detected in Combo Box by default
		}
		++i;
	}

	default_device.FromString(WxStrToStr(device_cbox->GetValue())); //Set device to be used to match what's selected in the Combo Box
}

void CConfigVR::Event_ClickSave(wxCommandEvent&)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID != "")
		g_Config.GameIniSave();
}

void CConfigVR::Event_ClickReset(wxCommandEvent&)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID != "")
	{
		g_Config.GameIniReset();
		Close();
	}
}

void CConfigVR::OnClose(wxCloseEvent& WXUNUSED(event))
{
	g_Config.SaveVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	EndModal(wxID_OK);
	// Save the config. Dolphin crashes too often to only save the settings on closing
	SConfig::GetInstance().SaveSettings();
}

void CConfigVR::OnOk(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

//On Combo Box Selection
void CConfigVR::SetDevice(wxCommandEvent&)
{
	default_device.FromString(WxStrToStr(device_cbox->GetValue()));

	// show user what it was validated as
	device_cbox->SetValue(StrToWxStr(default_device.ToString()));

	// update references
	//std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
	//vr_hotkey_controller->UpdateReferences(g_controller_interface);
}

//On Refresh Button Click
void CConfigVR::RefreshDevices(wxCommandEvent&)
{
	//std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);

	// refresh devices
	g_controller_interface.Reinitialize();

	// update device cbox
	UpdateDeviceComboBox();
}

// On Checkbox Click
void CConfigVR::OnXInputPollCheckbox(wxCommandEvent& event)
{
	wxCheckBox* checkbox = (wxCheckBox*)event.GetEventObject();
	if (checkbox->IsChecked())
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeysXInput = 1;
	}
	else 
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeysXInput = 0;
	}

	event.Skip();
}

void CConfigVR::OnFreeLookSensitivity(wxCommandEvent& event)
{
	wxSpinCtrlDouble* spinctrl = (wxSpinCtrlDouble*)event.GetEventObject();
	g_ActiveConfig.fFreeLookSensitivity = spinctrl->GetValue();

	event.Skip();
}

// Input button clicked
void CConfigVR::OnButtonClick(wxCommandEvent& event)
{
	event.Skip();

	//if (m_ButtonMappingTimer.IsRunning())
	//	return;

	wxTheApp->Bind(wxEVT_KEY_DOWN, &CConfigVR::OnKeyDown, this);

	// Get the button
	ClickedButton = (wxButton *)event.GetEventObject();
	SetEscapeId(wxID_CANCEL);  //This stops escape from exiting the whole ConfigVR box.
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(_("<Press Key>"));
	//DoGetButtons(ClickedButton->GetId());
}

void CConfigVR::OnKeyDown(wxKeyEvent& event)
{
	if (ClickedButton != nullptr)
	{
		// Save the key
		g_Pressed = event.GetKeyCode();
		g_Modkey = event.GetModifiers();

		// Don't allow modifier keys
		if (g_Pressed == WXK_CONTROL || g_Pressed == WXK_ALT ||
			g_Pressed == WXK_SHIFT || g_Pressed == WXK_COMMAND)
			return;

		// Use the space key to set a blank key
		if (g_Pressed == WXK_SPACE)
		{
			SaveButtonMapping(ClickedButton->GetId(), true, -1, 0);
			SaveXInputBinary(ClickedButton->GetId(), true, false, 0, 0);
			SetButtonText(ClickedButton->GetId(), true, false, wxString());
		}
		// Cancel and restore the old label if escape is hit.
		else if (g_Pressed == WXK_ESCAPE)
		{
			ClickedButton->SetLabel(OldLabel);
			button_already_clicked = false;
			return;
		}
		else
		{
			// Check if the hotkey combination was already applied to another button
			// and unapply it if necessary.
			for (wxButton* btn : m_Button_VRSettings)
			{
				// We compare against this to see if we have a duplicate bind attempt.
				wxString existingHotkey = btn->GetLabel();

				wxString tentativeModKey = WxUtils::WXKeymodToString(g_Modkey);
				wxString tentativePressedKey = WxUtils::WXKeyToString(g_Pressed);
				wxString tentativeHotkey(tentativeModKey + tentativePressedKey);

				// Found a button that already has this binding. Unbind it.
				if (tentativeHotkey == existingHotkey)
				{
					SaveButtonMapping(btn->GetId(), true, -1, 0); //TO DO: Should this set to true? Probably should be an if statement before this...
					SetButtonText(btn->GetId(), true, false, wxString());
				}
			}

			// Proceed to apply the binding to the selected button.
			SetButtonText(ClickedButton->GetId(),
				true,
				false,
				WxUtils::WXKeyToString(g_Pressed),
				WxUtils::WXKeymodToString(g_Modkey));
			SaveButtonMapping(ClickedButton->GetId(), true, g_Pressed, g_Modkey);
		}
	}

	EndGetButtons();
}

// Update the textbox for the buttons
void CConfigVR::SetButtonText(int id, bool KBM, bool DInput, const wxString &keystr, const wxString &modkeystr, const wxString &XInputMapping)
{
	if (KBM == true)
	{
		m_Button_VRSettings[id]->SetLabel(modkeystr + keystr);
	}
	else if (DInput == false)
	{
		wxString xinput_gui_string = "";
		static const char* const xinput_strings[]
		{
			"Button A",
			"Button B",
			"Button X",
			"Button Y",
			"Pad N",
			"Pad S",
			"Pad W",
			"Pad E",
			"Start",
			"Back",
			"Shoulder L",
			"Shoulder R",
			"Guide",
			"Thumb L",
			"Thumb R",
			"Trigger L",
			"Trigger R",
			"Left X-",
			"Left X+",
			"Left Y+",
			"Left Y-",
			"Right X+",
			"Right X-",
			"Right Y+",
			"Right Y-"
		};
		
		for (int i = 0; i < 25; ++i)
		{
			if (HotkeysXInput::IsXInputButtonSet(xinput_strings[i], id))
			{
				//Concat string
				if (xinput_gui_string != ""){
					xinput_gui_string += " && ";
				}
				xinput_gui_string += xinput_strings[i];
			}
		}
		m_Button_VRSettings[id]->SetLabel(xinput_gui_string);
	}
	else
	{
		wxString dinput_gui_string = "";
		static const char* const dinput_strings_buttons[]
		{
			"Button 0",
			"Button 1",
			"Button 2",
			"Button 3",
			"Button 4",
			"Button 5",
			"Button 6",
			"Button 7",
			"Button 8",
			"Button 9",
			"Button 10",
			"Button 11",
			"Button 12",
			"Button 13",
			"Button 14",
			"Button 15",
			"Button 16",
			"Button 17",
			"Button 18",
			"Button 19",
			"Button 20",
			"Button 21",
			"Button 22",
			"Button 23",
			"Button 24",
			"Button 25",
			"Button 26",
			"Button 27",
			"Button 28",
			"Button 29",
			"Button 30",
			"Button 31"
		};

		static const char* const dinput_strings_others[]
		{
			"Hat 0 N",
			"Hat 0 S",
			"Hat 0 W",
			"Hat 0 E",
			"Hat 1 N",
			"Hat 1 S",
			"Hat 1 W",
			"Hat 1 E",
			"Hat 2 N",
			"Hat 2 S",
			"Hat 2 W",
			"Hat 2 E",
			"Hat 3 N",
			"Hat 3 S",
			"Hat 3 W",
			"Hat 3 E",
			"Axis X-",
			"Axis X+",
			"Axis Y+",
			"Axis Y-",
			"Axis Z+",
			"Axis Z-",
			"Axis Zr+",
			"Axis Zr-"
		};

		for (int i = 0; i < (sizeof(dinput_strings_buttons) / sizeof(dinput_strings_buttons[0])); ++i)
		{
			if (HotkeysXInput::IsDInputButtonSet(dinput_strings_buttons[i], id))
			{
				//Concat string
				if (dinput_gui_string != ""){
					dinput_gui_string += " && ";
				}
				dinput_gui_string += dinput_strings_buttons[i];
			}
		}

		for (int i = 0; i < (sizeof(dinput_strings_others) / sizeof(dinput_strings_others[0])); ++i)
		{
			if (HotkeysXInput::IsDInputOthersSet(dinput_strings_others[i], id))
			{
				//Concat string
				if (dinput_gui_string != ""){
					dinput_gui_string += " && ";
				}
				dinput_gui_string += dinput_strings_others[i];
			}
		}

		m_Button_VRSettings[id]->SetLabel(dinput_gui_string);
	}
}

// Save keyboard key mapping
void CConfigVR::SaveButtonMapping(int Id, bool KBM, int Key, int Modkey)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[Id] = KBM;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[Id] = Modkey;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[Id] = 0;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[Id] = 0;
	int hk = -1;
	switch (Id)
	{
	case VR_POSITION_RESET:
		hk = HK_FREELOOK_RESET;
		break;
	case VR_CAMERA_BACKWARD:
		hk = HK_FREELOOK_ZOOM_OUT;
		break;
	case VR_CAMERA_FORWARD:
		hk = HK_FREELOOK_ZOOM_IN;
		break;
	case VR_CAMERA_LEFT:
		hk = HK_FREELOOK_LEFT;
		break;
	case VR_CAMERA_RIGHT:
		hk = HK_FREELOOK_RIGHT;
		break;
	case VR_CAMERA_UP:
		hk = HK_FREELOOK_UP;
		break;
	case VR_CAMERA_DOWN:
		hk = HK_FREELOOK_DOWN;
		break;
	}
	if (hk > 0)
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeyKBM[hk] = KBM;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[hk] = Key;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[hk] = Modkey;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyDandXInputMapping[hk] = 0;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyDInputMappingExtra[hk] = 0;
	}
}

void CConfigVR::SaveXInputBinary(int Id, bool KBM, bool DInput, u32 Key, u32 DInputExtra)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[Id] = KBM;
	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[Id] = DInput;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[Id] = DInputExtra;
	int hk = -1;
	switch (Id)
	{
	case VR_POSITION_RESET:
		hk = HK_FREELOOK_RESET;
		break;
	case VR_CAMERA_BACKWARD:
		hk = HK_FREELOOK_ZOOM_OUT;
		break;
	case VR_CAMERA_FORWARD:
		hk = HK_FREELOOK_ZOOM_IN;
		break;
	case VR_CAMERA_LEFT:
		hk = HK_FREELOOK_LEFT;
		break;
	case VR_CAMERA_RIGHT:
		hk = HK_FREELOOK_RIGHT;
		break;
	case VR_CAMERA_UP:
		hk = HK_FREELOOK_UP;
		break;
	case VR_CAMERA_DOWN:
		hk = HK_FREELOOK_DOWN;
		break;
	}
	if (hk > 0)
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeyKBM[hk] = KBM;
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeyDInput[hk] = DInput;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyDandXInputMapping[hk] = Key;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyDInputMappingExtra[hk] = DInputExtra;
	}
}

void CConfigVR::EndGetButtons()
{
	wxTheApp->Unbind(wxEVT_KEY_DOWN, &CConfigVR::OnKeyDown, this);
	button_already_clicked = false;
	ClickedButton = nullptr;
	SetEscapeId(wxID_ANY);
}

void CConfigVR::EndGetButtonsXInput()
{
	button_already_clicked = false;
	ClickedButton = nullptr;
	SetEscapeId(wxID_ANY);
}

void CConfigVR::ConfigControl(wxEvent& event)
{
	ClickedButton = (wxButton *)event.GetEventObject();
	
	ciface::Core::DeviceQualifier dq;

	int count = 0;

	for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
	{
		dq.FromDevice(d);
		if (dq.source == "XInput")
		{
			m_vr_dialog = new VRDialog(this, ClickedButton->GetId());
			m_vr_dialog->ShowModal();
			m_vr_dialog->Destroy();
			count++;
			break;
		}
		if (d->GetSource() == "DInput" && d->GetName() != "Keyboard Mouse")
		{
			m_vr_dialog = new VRDialog(this, ClickedButton->GetId());
			m_vr_dialog->ShowModal();
			m_vr_dialog->Destroy();
			count++;
			break;
		}
	}

	if (count == 0)
	{
		wxMessageDialog m_no_xinput(
			this,
			_("No XInput Device Detected.\nAttach an XInput Device to use this Feature."),
			_("No XInput Device"),
			wxOK | wxSTAY_ON_TOP,
			wxDefaultPosition);

		m_no_xinput.ShowModal();
	}

	// update changes that were made in the dialog
	UpdateGUI();
}

void CConfigVR::ClearControl(wxEvent& event)
{

	ClickedButton = (wxButton *)event.GetEventObject();
	SaveButtonMapping(ClickedButton->GetId(), true, -1, 0);
	SaveXInputBinary(ClickedButton->GetId(), true, false, 0, 0);
	SetButtonText(ClickedButton->GetId(), true, false, wxString());
   
}

inline bool IsAlphabeticVR(wxString &str)
{
	for (wxUniChar c : str)
		if (!isalpha(c))
			return false;

	return true;
}

inline void GetExpressionForControlVR(wxString &expr,
	wxString &control_name,
	ciface::Core::DeviceQualifier *control_device = nullptr,
	ciface::Core::DeviceQualifier *default_device = nullptr)
{
	expr = "";

	// non-default device
	if (control_device && default_device && !(*control_device == *default_device))
	{
		expr += control_device->ToString();
		expr += ":";
	}

	// append the control name
	expr += control_name;

	if (!IsAlphabeticVR(expr))
		expr = wxString::Format("%s", expr);
}

void CConfigVR::DetectControl(wxCommandEvent& event)
{
	//Stop the user from being able to select multiple buttons at once
	if (button_already_clicked)
	{ 
		return;
	}
	else 
	{
		button_already_clicked = true;
		// find devices
		ciface::Core::Device* const dev = g_controller_interface.FindDevice(default_device);
		if (dev)
		{
			if (default_device.name == "Keyboard Mouse") 
			{
				OnButtonClick(event);
			}
			//If it's a DInput device that's not a Keyboard/Mouse or XInput Device
			else if (default_device.source == "XInput" || default_device.source == "DInput") 
			{
				// Get the button
				ClickedButton = (wxButton *)event.GetEventObject();
				SetEscapeId(wxID_CANCEL);  //This stops escape from exiting the whole ConfigVR box.
				// Save old label so we can revert back
				OldLabel = ClickedButton->GetLabel();

				ClickedButton->SetLabel(_("<Press Button>"));

				// This makes the "Press Button" text work on Linux. true (only if needed) prevents crash on Windows
				wxTheApp->Yield(true);

				//std::lock_guard<std::recursive_mutex> lk(m_config.controls_lock);
				ciface::Core::Device::Control* const ctrl = InputDetect(DETECT_WAIT_TIME, dev);

				// if we got input, update expression and reference
				if (ctrl)
				{
					wxString control_name = ctrl->GetName();
					wxString expr;
					GetExpressionForControlVR(expr, control_name);
					ClickedButton->SetLabel(expr);
					if (default_device.source == "XInput")
					{
						u32 xinput_binary = HotkeysXInput::GetBinaryfromXInputIniStr(expr);
						SaveXInputBinary(ClickedButton->GetId(), false, false, xinput_binary, 0);
					}
					else
					{
						u32 dinput_binary_other = 0;
						u32 dinput_binary_buttons = HotkeysXInput::GetBinaryfromDInputIniStr(expr);
						if (!dinput_binary_buttons)
						{
							dinput_binary_other = HotkeysXInput::GetBinaryfromDInputExtraIniStr(expr);
						}
						SaveXInputBinary(ClickedButton->GetId(), false, true, dinput_binary_buttons, dinput_binary_other);
					}
				}
				else 
				{
					ClickedButton->SetLabel(OldLabel);
				}

				EndGetButtonsXInput();
			}
		}
		else 
		{
			button_already_clicked = false;
		}
	}

}


ciface::Core::Device::Control* CConfigVR::InputDetect(const unsigned int ms, ciface::Core::Device* const device)
{
	unsigned int time = 0;
	std::vector<bool> states(device->Inputs().size());

	if (device->Inputs().size() == 0)
		return nullptr;

	// get starting state of all inputs,
	// so we can ignore those that were activated at time of Detect start
	std::vector<ciface::Core::Device::Input*>::const_iterator
		i = device->Inputs().begin(),
		e = device->Inputs().end();
	for (std::vector<bool>::iterator state = states.begin(); i != e; ++i)
		*state++ = ((*i)->GetState() > (1 - INPUT_DETECT_THRESHOLD));

	while (time < ms)
	{
		device->UpdateInput();
		i = device->Inputs().begin();
		for (std::vector<bool>::iterator state = states.begin(); i != e; ++i, ++state)
		{
			// detected an input
			if ((*i)->IsDetectable() && (*i)->GetState() > INPUT_DETECT_THRESHOLD)
			{
				// input was released at some point during Detect call
				// return the detected input
				if (false == *state)
					return *i;
			}
			else if ((*i)->GetState() < (1 - INPUT_DETECT_THRESHOLD))
			{
				*state = false;
			}
		}
		Common::SleepCurrentThread(10); time += 10;
	}

	// no input was detected
	return nullptr;
}

VRDialog::VRDialog(CConfigVR* const parent, int from_button)
	: wxDialog(parent, wxID_ANY, _("Configure Control"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxBoxSizer* const input_szr1 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const input_szr2 = new wxBoxSizer(wxVERTICAL); 
	wxBoxSizer* const input_szr3 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const input_szr4 = new wxBoxSizer(wxVERTICAL);
	bool dinput_detected = false;
	bool xinput_detected = false;

	button_id = from_button;

	ciface::Core::DeviceQualifier dq;
	for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
	{
		dq.FromDevice(d);
		if (dq.source == "XInput")
		{
			int i = 0;
			for (ciface::Core::Device::Input* input : d->Inputs())
			{
				wxCheckBox  *XInputCheckboxes = new wxCheckBox(this, wxID_ANY, wxGetTranslation(StrToWxStr(input->GetName())), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
				XInputCheckboxes->Bind(wxEVT_CHECKBOX, &VRDialog::OnCheckBoxXInput, this);
				if (HotkeysXInput::IsXInputButtonSet(input->GetName(), button_id))
				{
					XInputCheckboxes->SetValue(true);
				}
				if (i<13)
				{
					input_szr1->Add(XInputCheckboxes, 1, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
				}
				else 
				{
					input_szr2->Add(XInputCheckboxes, 1, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
				}
				++i;
			}
			xinput_detected = true;
		}
		if (dq.source == "DInput" && d->GetName() != "Keyboard Mouse")
		{
			int i = 0;
			for (ciface::Core::Device::Input* input : d->Inputs())
			{
				if ((input->IsDetectable()))
				{
					std::string input_string = input->GetName();

					wxCheckBox  *DInputCheckboxes = new wxCheckBox(this, wxID_ANY, wxGetTranslation(StrToWxStr(input_string)), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);

					if (input_string.find("Button") != std::string::npos)
					{
						DInputCheckboxes->Bind(wxEVT_CHECKBOX, &VRDialog::OnCheckBoxDInputButtons, this);
						if (HotkeysXInput::IsDInputButtonSet(input_string, button_id))
						{
							DInputCheckboxes->SetValue(true);
						}
						input_szr3->Add(DInputCheckboxes, 1, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
					}
					else
					{
						DInputCheckboxes->Bind(wxEVT_CHECKBOX, &VRDialog::OnCheckBoxDInputOthers, this);
						if (HotkeysXInput::IsDInputOthersSet(input_string, button_id))
						{
							DInputCheckboxes->SetValue(true);
						}
						input_szr4->Add(DInputCheckboxes, 1, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
					}
					++i;
				}
			}
			dinput_detected = true;
		}
	}

	wxBoxSizer* const horizontal_szr = new wxStaticBoxSizer(wxHORIZONTAL, this, "XInput Combinations"); //Horizontal box to add both columns of checkboxes to.
	wxBoxSizer* const vertical_szr = new wxBoxSizer(wxVERTICAL);

	if (xinput_detected)
	{
		horizontal_szr->Add(input_szr1, 1, wxEXPAND | wxALL, 5);
		horizontal_szr->Add(input_szr2, 1, wxEXPAND | wxALL, 5);
	}
	if (dinput_detected)
	{
		horizontal_szr->Add(input_szr3, 1, wxEXPAND | wxALL, 5);
		horizontal_szr->Add(input_szr4, 1, wxEXPAND | wxALL, 5);
	}

	vertical_szr->Add(horizontal_szr, 1, wxEXPAND | wxALL, 5);
	vertical_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);
	SetSizerAndFit(vertical_szr); // needed

	SetFocus();
}

void VRDialog::OnCheckBoxXInput(wxCommandEvent& event)
{
	bool dinput = SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[button_id];

	if (dinput)
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id] = 0;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[button_id] = 0;
	}

	wxCheckBox* checkbox = (wxCheckBox*)event.GetEventObject();
	u32 single_button_mask = HotkeysXInput::GetBinaryfromXInputIniStr(checkbox->GetLabel());
	u32 value = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id];

	if (checkbox->IsChecked())
		value |= single_button_mask;
	else 
		value &= ~single_button_mask;

	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[button_id] = FALSE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[button_id] = FALSE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id] = value;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[button_id] = 0;

	event.Skip();
}

void VRDialog::OnCheckBoxDInputButtons(wxCommandEvent& event)
{
	bool dinput = SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[button_id];

	if (!dinput)
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id] = 0;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[button_id] = 0;
	}
	wxCheckBox* checkbox = (wxCheckBox*)event.GetEventObject();
	u32 single_button_mask = HotkeysXInput::GetBinaryfromDInputIniStr(checkbox->GetLabel());
	u32 value = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id];

	if (checkbox->IsChecked())
		value |= single_button_mask;
	else
		value &= ~single_button_mask;

	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[button_id] = FALSE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[button_id] = TRUE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id] = value;

	event.Skip();
}

void VRDialog::OnCheckBoxDInputOthers(wxCommandEvent& event)
{
	bool dinput = SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[button_id];

	if (!dinput)
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[button_id] = 0;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[button_id] = 0;
	}

	wxCheckBox* checkbox = (wxCheckBox*)event.GetEventObject();
	u32 single_button_mask = HotkeysXInput::GetBinaryfromDInputExtraIniStr(checkbox->GetLabel());
	u32 value = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[button_id];

	if (checkbox->IsChecked())
		value |= single_button_mask;
	else
		value &= ~single_button_mask;

	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[button_id] = FALSE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsDInput[button_id] = TRUE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[button_id] = value;

	event.Skip();
}
