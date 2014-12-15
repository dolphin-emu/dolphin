// Copyright 2013 Dolphin Emulator Project
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
			SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[i],
			WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
			WxUtils::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]),
			HotkeysXInput::GetwxStringfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[i]));
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
		wxFlexGridSizer* const szr_vr = new wxFlexGridSizer(2, 5, 5);

		// Scale
		{
			SettingNumber *const spin_scale = CreateNumber(page_vr, vconfig.fScale,
				wxGetTranslation(scale_desc), 0.001f, 100.0f, 0.01f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Scale:"));
			
			spin_scale->SetToolTip(wxGetTranslation(scale_desc));
			label->SetToolTip(wxGetTranslation(scale_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_scale);
		}
		// Lean back angle
		{
			SettingNumber *const spin_lean = CreateNumber(page_vr, vconfig.fLeanBackAngle,
				wxGetTranslation(lean_desc), -180.0f, 180.0f, 1.0f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Lean back angle:"));

			spin_lean->SetToolTip(wxGetTranslation(lean_desc));
			label->SetToolTip(wxGetTranslation(lean_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_lean);
		}
		// VR Player
		{
			const wxString vr_choices[] = { _("Player 1"), _("Player 2"), _("Player 3"), _("Player 4") };

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Player wearing HMD:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			wxChoice* const choice_vr = CreateChoice(page_vr, vconfig.iVRPlayer, wxGetTranslation(player_desc),
				sizeof(vr_choices) / sizeof(*vr_choices), vr_choices);
			szr_vr->Add(choice_vr, 1, 0, 0);
			choice_vr->Select(vconfig.iVRPlayer);
		}
		{
			U32Setting* spin_replay_buffer = new U32Setting(page_vr, _("Extra Opcode Replay Frames:"), vconfig.iExtraVideoLoops, 0, 100000);
			RegisterControl(spin_replay_buffer, replaybuffer_desc);
			spin_replay_buffer->SetToolTip(replaybuffer_desc);
			spin_replay_buffer->SetValue(vconfig.iExtraVideoLoops);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Extra Opcode Replay Frames:"));

			label->SetToolTip(wxGetTranslation(replaybuffer_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_replay_buffer);
		}
		{
			U32Setting* spin_replay_buffer_divider = new U32Setting(page_vr, _("Extra Opcode Replay Frame Divider:"), vconfig.iExtraVideoLoopsDivider, 0, 100000);
			RegisterControl(spin_replay_buffer_divider, replaybuffer_desc);
			spin_replay_buffer_divider->SetToolTip(replaybuffer_desc);
			spin_replay_buffer_divider->SetValue(vconfig.iExtraVideoLoopsDivider);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Extra Opcode Replay Frame Divider:"));

			label->SetToolTip(wxGetTranslation(replaybuffer_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_replay_buffer_divider);

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Quick Opcode Replay Settings (ALPHA TEST):")), 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20 = CreateCheckBox(page_vr, _("Pullup 20fps to 75fps"), wxGetTranslation(pullup20_desc), vconfig.bPullUp20fps);
			checkbox_pullup30 = CreateCheckBox(page_vr, _("Pullup 30fps to 75fps"), wxGetTranslation(pullup30_desc), vconfig.bPullUp30fps);
			checkbox_pullup60 = CreateCheckBox(page_vr, _("Pullup 60fps to 75fps"), wxGetTranslation(pullup60_desc), vconfig.bPullUp60fps);
			szr_vr->Add(checkbox_pullup20, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_pullup30, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_pullup60, 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup30->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup60->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup20->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp);
			checkbox_pullup30->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp);
			checkbox_pullup60->Enable(!vconfig.bPullUp20fpsTimewarp && !vconfig.bPullUp30fpsTimewarp && !vconfig.bPullUp60fpsTimewarp);
		}
		// Synchronous Timewarp extra frames per frame
		{
			U32Setting* spin_extra_frames = new U32Setting(page_vr, _("Extra Timewarped Frames:"), vconfig.iExtraFrames, 0, 4);
			RegisterControl(spin_extra_frames, extraframes_desc);
			spin_extra_frames->SetToolTip(extraframes_desc);
			spin_extra_frames->SetValue(vconfig.iExtraFrames);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Extra Timewarped Frames:"));

			label->SetToolTip(wxGetTranslation(extraframes_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_extra_frames);
		}
		{
			SettingNumber* const spin_timewarp_tweak = CreateNumber(page_vr, vconfig.fTimeWarpTweak, _("Timewarp VSync Tweak:"), -1.0f, 1.0f, 0.0001f);
			RegisterControl(spin_timewarp_tweak, timewarptweak_desc);
			spin_timewarp_tweak->SetToolTip(timewarptweak_desc);
			spin_timewarp_tweak->SetValue(vconfig.fTimeWarpTweak);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Timewarp VSync Tweak:"));

			label->SetToolTip(wxGetTranslation(timewarptweak_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin_timewarp_tweak);

			szr_vr->Add(new wxStaticText(page_vr, wxID_ANY, _("Timewarp Pull-Up:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20_timewarp = CreateCheckBox(page_vr, _("Timewarp 20fps to 75fps"), wxGetTranslation(pullup20timewarp_desc), vconfig.bPullUp20fpsTimewarp);
			checkbox_pullup30_timewarp = CreateCheckBox(page_vr, _("Timewarp 30fps to 75fps"), wxGetTranslation(pullup30timewarp_desc), vconfig.bPullUp30fpsTimewarp);
			checkbox_pullup60_timewarp = CreateCheckBox(page_vr, _("Timewarp 60fps to 75fps"), wxGetTranslation(pullup60timewarp_desc), vconfig.bPullUp60fpsTimewarp);
			szr_vr->Add(checkbox_pullup20_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_pullup30_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(checkbox_pullup60_timewarp, 1, wxALIGN_CENTER_VERTICAL, 0);
			checkbox_pullup20_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup30_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup60_timewarp->Bind(wxEVT_CHECKBOX, &CConfigVR::OnPullupCheckbox, this);
			checkbox_pullup20_timewarp->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);
			checkbox_pullup30_timewarp->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);
			checkbox_pullup60_timewarp->Enable(!vconfig.bPullUp20fps && !vconfig.bPullUp30fps && !vconfig.bPullUp60fps);

		}

		szr_vr->Add(CreateCheckBox(page_vr, _("Enable VR"), wxGetTranslation(enablevr_desc), vconfig.bEnableVR));
		szr_vr->Add(CreateCheckBox(page_vr, _("Low persistence"), wxGetTranslation(lowpersistence_desc), vconfig.bLowPersistence));
		szr_vr->Add(CreateCheckBox(page_vr, _("Dynamic prediction"), wxGetTranslation(dynamicpred_desc), vconfig.bDynamicPrediction));
		szr_vr->Add(CreateCheckBox(page_vr, _("Orientation tracking"), wxGetTranslation(orientation_desc), vconfig.bOrientationTracking));
		szr_vr->Add(CreateCheckBox(page_vr, _("Magnetic yaw"), wxGetTranslation(magyaw_desc), vconfig.bMagYawCorrection));
		szr_vr->Add(CreateCheckBox(page_vr, _("Position tracking"), wxGetTranslation(position_desc), vconfig.bPositionTracking));
		szr_vr->Add(CreateCheckBox(page_vr, _("Chromatic aberration"), wxGetTranslation(chromatic_desc), vconfig.bChromatic));
		szr_vr->Add(CreateCheckBox(page_vr, _("Timewarp"), wxGetTranslation(timewarp_desc), vconfig.bTimewarp));
		szr_vr->Add(CreateCheckBox(page_vr, _("Vignette"), wxGetTranslation(vignette_desc), vconfig.bVignette));
		szr_vr->Add(CreateCheckBox(page_vr, _("Don't restore"), wxGetTranslation(norestore_desc), vconfig.bNoRestore));
		szr_vr->Add(CreateCheckBox(page_vr, _("Flip vertical"), wxGetTranslation(flipvertical_desc), vconfig.bFlipVertical));
		szr_vr->Add(CreateCheckBox(page_vr, _("sRGB"), wxGetTranslation(srgb_desc), vconfig.bSRGB));
		szr_vr->Add(CreateCheckBox(page_vr, _("Overdrive"), wxGetTranslation(overdrive_desc), vconfig.bOverdrive));
		szr_vr->Add(CreateCheckBox(page_vr, _("HQ distortion"), wxGetTranslation(hqdistortion_desc), vconfig.bHqDistortion));
		szr_vr->Add(async_timewarp_checkbox = CreateCheckBox(page_vr, _("Asynchronous timewarp"), wxGetTranslation(async_desc), SConfig::GetInstance().m_LocalCoreStartupParameter.bAsynchronousTimewarp));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("All games"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

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
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Units per metre:"));
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
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Camera forward:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Camera pitch
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fCameraPitch,
				wxGetTranslation(temp_desc), -180, 360, 1);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Camera pitch:"));
			label->SetToolTip(wxGetTranslation(temp_desc));
			szr_vr->Add(label, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_vr->Add(spin);
		}
		// Aim distance
		{
			SettingNumber *const spin = CreateNumber(page_vr, vconfig.fAimDistance,
				wxGetTranslation(temp_desc), 0.01f, 10000, 0.1f);
			wxStaticText *label = new wxStaticText(page_vr, wxID_ANY, _("Aim distance:"));
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
		szr_vr->Add(CreateCheckBox(page_vr, _("HUD on top"), wxGetTranslation(hudontop_desc), vconfig.bHudOnTop));

		wxStaticBoxSizer* const group_vr = new wxStaticBoxSizer(wxVERTICAL, page_vr, _("For this game only"));
		group_vr->Add(szr_vr, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_vr_main->Add(group_vr, 0, wxEXPAND | wxALL, 5);

		wxButton* const btn_save = new wxButton(page_vr, wxID_ANY, _("Save"));
		btn_save->Bind(wxEVT_BUTTON, &CConfigVR::Event_ClickSave, this);
		szr_vr->Add(btn_save, 1, wxALIGN_CENTER_VERTICAL, 0);
		wxButton* const btn_reset = new wxButton(page_vr, wxID_ANY, _("Reset to defaults"));
		btn_reset->Bind(wxEVT_BUTTON, &CConfigVR::Event_ClickReset, this);
		szr_vr->Add(btn_reset, 1, wxALIGN_CENTER_VERTICAL, 0);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIniLocal == "")
		{
			btn_save->Disable();
			btn_reset->Disable();
			page_vr->Disable();
		}

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
		_("Larger Scale"),
		_("Smaller Scale"),
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
				SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[i],
				WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
				WxUtils::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]),
				HotkeysXInput::GetwxStringfromXInputIni(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[i]));

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

			wxSpinCtrlDouble *const spin_freelook_scale = new wxSpinCtrlDouble(Page, wxID_ANY, wxString::Format(wxT("%f"), SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity), wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0.001f, 100.0f, 1.00f, 0.05f);
			wxStaticText *spin_freelook_scale_label = new wxStaticText(Page, wxID_ANY, _(" Free Look Sensitivity: "));
			spin_freelook_scale->SetToolTip(_("Scales the rate at which Camera Forward/Backwards/Left/Right/Up/Down move per key or button press."));
			spin_freelook_scale_label->SetToolTip(_("Scales the rate at which Camera Forward/Backwards/Left/Right/Up/Down move per key or button press."));
			spin_freelook_scale->Bind(wxEVT_SPINCTRLDOUBLE, &CConfigVR::OnFreeLookSensitivity, this);

			wxCheckBox  *xInputPollEnableCheckbox = new wxCheckBox(Page, wxID_ANY, _("Enable XInput Polling"), wxDefaultPosition, wxDefaultSize);
			xInputPollEnableCheckbox->Bind(wxEVT_CHECKBOX, &CConfigVR::OnXInputPollCheckbox, this);
			xInputPollEnableCheckbox->SetToolTip(_("Check to enable XInput polling during game emulation. Uncheck to disable."));
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeysXInput)
			{
				xInputPollEnableCheckbox->SetValue(true);
			}

			options_gszr->Add(spin_freelook_scale_label, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL, 3);
			options_gszr->Add(spin_freelook_scale, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL, 3);
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
				"guide to help.\n\nDirect mode is working, but the performance is poor.  It is recommended to run in "
				"extended mode!\n\nOpenGL mode is currently undergoing a merge with Dolphin's native stereoscopic rendering "
				"code, and complete VR support is not finished (IPD, Convergence, Scale, etc. are not all automatically "
				"implemented).  It is left enabled for experimentation, but the seperation and convergence will have to be "
				"set manually for each game.  The experience is much better in most games with Direct3D right now, so try that "
				"first.\n\nIn the 'Config' tab, 'Framelimit' should be set to match your Rift's refresh rate (most likely "
				"75fps).  Games will run 25% too fast, but this will avoid judder. The Rift can be set to run at 60hz from "
				"your AMD/Nvidia control panel and still run with low persistence, but flickering can be seen in bright scenes.\n\n"
				"Under Graphics->Hacks->EFB Copies, 'Disable' and 'Remove Blank EFB Copy Box' should be checked for most games.\n\n"
				"Right-clicking on each game will give you the options to adjust VR settings, and remove rendered objects. "
				"Objects such as fake 16:9 bars can be removed from the game.  Some games already have object removal codes, "
				"so make sure to check them if you would like the object removed.  You can find your own codes by starting "
				"with an 8-bit code and clicking the up button until the object disappears.  Then move to 16bit, add two zeros "
				"to the right side of your 8bit code, and continue hitting the up button until the object disappears again. "
				"Continue until you have a long enough code for it to be unique.\n\nSome games run at 30fps as opposed to 60fps. "
				"There is a 1:2 pullup (timewarp) feature that can be enabled to fake a frame rate of 60fps. To enable this, "
				"go to the 'VR' tab and change 'Extra Timewarped Frames' to 1.  Setting this to 2 will insert two extra frames, "
				"so PAL games that run at 25fps will have head tracking at 75fps, and N64 games that run at 20fps (e.g. Zelda: OoT) "
				"will have head-tracking at 60fps. Make sure this is set to zero if running a 60fps game.\n\nTake time to set "
				"the VR-Hotkeys. You can create combinations for XInput by right clicking on the buttons. Remember you can also set "
				"a the gamecube/wii controller emulation to not happen during a certain button press, so setting freelook to 'Left "
				"Bumper + Right Analog Stick' and the gamecube C-Stick to '!Left Bumper + Right Analog Stick' works great. The "
				"freelook step size varies per game, so play with the 'Freelook Sensitivity' option to set it right for your game.\n\n"
				"Enjoy and watch for new updates, because we're making progress fast!"));

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

// On Checkbox Click
void CConfigVR::OnPullupCheckbox(wxCommandEvent& event)
{
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
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIniLocal != "")
		g_Config.GameIniSave();
}

void CConfigVR::Event_ClickReset(wxCommandEvent&)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIniLocal != "")
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
	SConfig::GetInstance().m_LocalCoreStartupParameter.fFreeLookSensitivity = spinctrl->GetValue();

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
			SaveXInputBinary(ClickedButton->GetId(), true, 0);
			SetButtonText(ClickedButton->GetId(), true, wxString());
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
					SetButtonText(btn->GetId(), true, wxString());
				}
			}

			// Proceed to apply the binding to the selected button.
			SetButtonText(ClickedButton->GetId(),
				true,
				WxUtils::WXKeyToString(g_Pressed),
				WxUtils::WXKeymodToString(g_Modkey));
			SaveButtonMapping(ClickedButton->GetId(), true, g_Pressed, g_Modkey);
		}
	}

	EndGetButtons();
}

// Update the textbox for the buttons
void CConfigVR::SetButtonText(int id, bool KBM, const wxString &keystr, const wxString &modkeystr, const wxString &XInputMapping)
{
	if (KBM == true)
	{
		m_Button_VRSettings[id]->SetLabel(modkeystr + keystr);
	}
	else 
	{
		wxString xinput_gui_string = "";
		ciface::Core::DeviceQualifier dq;
		for (ciface::Core::Device* d : g_controller_interface.Devices()) //For Every Device Attached
		{
			dq.FromDevice(d);
			if (dq.source == "XInput")
			{
				for (ciface::Core::Device::Input* input : d->Inputs())
				{
					if (HotkeysXInput::IsXInputButtonSet(input->GetName(), id))
					{
						//Concat string
						if (xinput_gui_string != ""){
							xinput_gui_string += " && ";
						}
						xinput_gui_string += input->GetName();
					}
				}
			}
		}
		m_Button_VRSettings[id]->SetLabel(xinput_gui_string);
	}
}

// Save keyboard key mapping
void CConfigVR::SaveButtonMapping(int Id, bool KBM, int Key, int Modkey)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[Id] = KBM;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[Id] = Modkey;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[Id] = 0;
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
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyKBM[hk] = KBM;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[hk] = Key;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[hk] = Modkey;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyXInputMapping[hk] = 0;
	}
}

void CConfigVR::SaveXInputBinary(int Id, bool KBM, u32 Key)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[Id] = KBM;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[Id] = Key;
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
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyKBM[hk] = KBM;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyXInputMapping[hk] = Key;
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
	SaveXInputBinary(ClickedButton->GetId(), true, 0);
	SetButtonText(ClickedButton->GetId(), true, wxString());
   
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
			else if (default_device.source == "XInput") 
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
					u32 xinput_binary = HotkeysXInput::GetBinaryfromXInputIniStr(expr);
					SaveXInputBinary(ClickedButton->GetId(), false, xinput_binary);
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
				XInputCheckboxes->Bind(wxEVT_CHECKBOX, &VRDialog::OnCheckBox, this);
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
		}

	}

	wxBoxSizer* const horizontal_szr = new wxStaticBoxSizer(wxHORIZONTAL, this, "Input"); //Horizontal box to add both columns of checkboxes to.
	horizontal_szr->Add(input_szr1, 1, wxEXPAND | wxALL, 5);
	horizontal_szr->Add(input_szr2, 1, wxEXPAND | wxALL, 5);
	wxBoxSizer* const vertical_szr = new wxBoxSizer(wxVERTICAL); //Horizontal box to add both columns of checkboxes to.
	vertical_szr->Add(horizontal_szr, 1, wxEXPAND | wxALL, 5);
	vertical_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);

	SetSizerAndFit(vertical_szr); // needed
	SetFocus();
}

void VRDialog::OnCheckBox(wxCommandEvent& event)
{
	wxCheckBox* checkbox = (wxCheckBox*)event.GetEventObject();
	u32 single_button_mask = HotkeysXInput::GetBinaryfromXInputIniStr(checkbox->GetLabel());
	u32 value = SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[button_id];
	if (checkbox->IsChecked())
		value |= single_button_mask;
	else 
		value &= ~single_button_mask;

	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsKBM[button_id] = FALSE;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsXInputMapping[button_id] = value;

	event.Skip();
}
