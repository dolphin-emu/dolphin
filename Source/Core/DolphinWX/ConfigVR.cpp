// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include <wx/bitmap.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcmemory.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/gdicmn.h>
#include <wx/layout.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/validate.h>
#include <wx/window.h>
#include <wx/windowid.h>


#include "DolphinWX/WXInputBase.h"

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/SysConf.h"

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/Core.h"

#include "DolphinWX/ConfigVR.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeWindow.h"

#define TEXT_BOX(page, text) new wxStaticText(page, wxID_ANY, text)

#ifdef WIN32
//only used with xgettext to be picked up as translatable string.
//win32 does not have wx on its path, the provided wxALL_FILES
//translation does not work there.
#define unusedALL_FILES wxTRANSLATE("All files (*.*)|*.*");
#endif

BEGIN_EVENT_TABLE(CConfigVR, wxDialog)

EVT_COMMAND_RANGE(0, NUM_VR_OPTIONS - 1, wxEVT_BUTTON, CConfigVR::OnButtonClick)
//EVT_TIMER(wxID_ANY, CConfigVR::OnButtonTimer)

EVT_CLOSE(CConfigVR::OnClose)
//EVT_RADIOBOX(ID_DSPENGINE, CConfigVR::AudioSettingsChanged)
//EVT_SLIDER(ID_VOLUME, CConfigVR::AudioSettingsChanged)
//EVT_CHOICE(ID_WII_IPL_LNG, CConfigVR::WiiSettingsChanged)
//EVT_LISTBOX(ID_ISOPATHS, CConfigVR::ISOPathsSelectionChanged)
//EVT_CHECKBOX(ID_RECURSIVEISOPATH, CConfigVR::RecursiveDirectoryChanged)
//EVT_BUTTON(ID_ADDISOPATH, CConfigVR::AddRemoveISOPaths)
//EVT_FILEPICKER_CHANGED(ID_DEFAULTISO, CConfigVR::DefaultISOChanged)
//EVT_DIRPICKER_CHANGED(ID_DVDROOT, CConfigVR::DVDRootChanged)

END_EVENT_TABLE()

CConfigVR::CConfigVR(wxWindow* parent, wxWindowID id, const wxString& title,
		const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

CConfigVR::~CConfigVR()
{
}

// Used to restrict changing of some options while emulator is running
void CConfigVR::UpdateGUI()
{
	if (Core::IsRunning())
	{
		// Disable the Core stuff on GeneralPage
		//CPUThread->Disable();
		//SkipIdle->Disable();
		//EnableCheats->Disable();

		//CPUEngine->Disable();
		//_NTSCJ->Disable();
	}
}
void CConfigVR::InitializeGUILists()
{
}

#define VR_NUM_COLUMNS 2

void CConfigVR::CreateGUIControls()
{
	InitializeGUILists();

	const wxString pageNames[] =
	{
		_("VR Freelook"),
		_("VR Options")
	};

	const wxString VRText[] =
	{
		_("Reset Camera"),
		_("Camera Forward"),
		_("Camera Backward"),
		_("Camera Left"),
		_("Camera Right"),
		_("Camera Up"),
		_("Camera Down"),

		_("Permanent Camera Forward"),
		_("Permanent Camera Backward"),
		_("Larger Scale"),
		_("Smaller Scale"),
		_("Tilt Camera Up"),
		_("Tilt Camera Down"),

		_("HUD Forward"),
		_("HUD Backward"),
		_("HUD Thicker"),
		_("HUD Thinner"),

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

	const int page_breaks[3] = {VR_POSITION_RESET, NUM_VR_OPTIONS, NUM_VR_OPTIONS};

	// Configuration controls sizes
	wxSize size(100,20);
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxNotebook *Notebook = new wxNotebook(this, wxID_ANY);

	for (int j = 0; j < 2; j++)
	{
		wxPanel *Page = new wxPanel(Notebook, wxID_ANY);
		Notebook->AddPage(Page, pageNames[j]);

		wxGridBagSizer *sVRKeys = new wxGridBagSizer();

		// Header line
		for (int i = 0; i < VR_NUM_COLUMNS; i++)
		{
			wxBoxSizer *HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText *StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Action"));
			HeaderSizer->Add(StaticTextHeader, 1, wxALL, 2);
			StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Key"), wxDefaultPosition, size);
			HeaderSizer->Add(StaticTextHeader, 0, wxALL, 2);
			sVRKeys->Add(HeaderSizer, wxGBPosition(0, i), wxDefaultSpan, wxEXPAND | wxLEFT, (i > 0) ? 30 : 1);
		}

		int column_break = (page_breaks[j+1] + page_breaks[j] + 1) / 2;

		for (int i = page_breaks[j]; i < page_breaks[j+1]; i++)
		{
			// Text for the action
			wxStaticText *stHotkeys = new wxStaticText(Page, wxID_ANY, VRText[i]);

			// Key selection button
			m_Button_VRSettings[i] = new wxButton(Page, i, wxEmptyString, wxDefaultPosition, size);
			m_Button_VRSettings[i]->SetFont(m_SmallFont);
			m_Button_VRSettings[i]->SetToolTip(_("Left click to change the controlling key.\nEnter space to clear."));
			SetButtonText(i,
				InputCommon::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[i]),
				InputCommon::WXKeymodToString(
				SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[i]));

			wxBoxSizer *sVRKey = new wxBoxSizer(wxHORIZONTAL);
			sVRKey->Add(stHotkeys, 1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
			sVRKey->Add(m_Button_VRSettings[i], 0, wxALL, 2);
			sVRKeys->Add(sVRKey,
				wxGBPosition((i < column_break) ? i - page_breaks[j] + 1 : i - column_break + 1,
				(i < column_break) ? 0 : 1),
				wxDefaultSpan, wxEXPAND | wxLEFT, (i < column_break) ? 1 : 30);
		}

		wxStaticBoxSizer *sVRKeyBox = new wxStaticBoxSizer(wxVERTICAL, Page, _("VR Camera Controls"));
		sVRKeyBox->Add(sVRKeys);

		wxBoxSizer* const sPage = new wxBoxSizer(wxVERTICAL);
		sPage->Add(sVRKeyBox, 0, wxEXPAND | wxALL, 5);
		Page->SetSizer(sPage);
	}

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(Notebook, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);
	SetSizerAndFit(sMainSizer);
	SetFocus();


#if 0

	// Create the notebook and pages
	Notebook = new wxNotebook(this, ID_NOTEBOOK);
	wxPanel* const VRLookPage = new wxPanel(Notebook, ID_VRLOOKPAGE);

	Notebook->AddPage(VRLookPage, _("VR Free Look"));

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);

	main_szr->Add(Notebook, 1, wxEXPAND | wxALL, 5);

	//top_box->AddSpacer(5);
	//top_box->Add(main_box);
	//top_box->AddSpacer(5);
	//top_box->Add(c_box);
	//top_box->AddSpacer(5);
	//bottom_box->AddSpacer(5);
	//bottom_box->Add(shoulder_box);
	//bottom_box->AddSpacer(5);
	//bottom_box->Add(buttons_box);
	//bottom_box->AddSpacer(5);

	//main_szr->Add(top_box);
	//main_szr->Add(bottom_box);

	wxBoxSizer* const sVRLookPage = new wxBoxSizer(wxVERTICAL);
	//sVRLookPage->Add(top_box, 0, wxEXPAND | wxALL, 5);
	//sVRLookPage->Add(bottom_box, 0, wxEXPAND | wxALL, 5);
	VRLookPage->SetSizer(sVRLookPage);


	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	InitializeGUIValues();
	InitializeGUITooltips();

	UpdateGUI(); //This function disables things that cannot be changed during emulation.

	//SetSizerAndFit(main_szr);
	Center();
	SetFocus();

#endif
}

void CConfigVR::OnClose(wxCloseEvent& WXUNUSED (event))
{
	EndModal(wxID_OK);
}

void CConfigVR::OnOk(wxCommandEvent& WXUNUSED (event))
{
	Close();

	// Save the config. Dolphin crashes too often to only save the settings on closing
	//SConfig::GetInstance().SaveSettings();
}


// Display and Interface settings
// This is used if settings are changed.  Loads them?
void CConfigVR::DisplaySettingsChanged(wxCommandEvent& event)
{
#if 0
	switch (event.GetId())
	{
	// Display - Interface
	case ID_INTERFACE_CONFIRMSTOP:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop = ConfirmStop->IsChecked();
		break;
	case ID_INTERFACE_USEPANICHANDLERS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers = UsePanicHandlers->IsChecked();
		SetEnableAlert(UsePanicHandlers->IsChecked());
		break;
	case ID_INTERFACE_ONSCREENDISPLAYMESSAGES:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bOnScreenDisplayMessages = OnScreenDisplayMessages->IsChecked();
		SetEnableAlert(OnScreenDisplayMessages->IsChecked());
		break;
	case ID_INTERFACE_LANG:
		if (SConfig::GetInstance().m_InterfaceLanguage != langIds[InterfaceLang->GetSelection()])
			SuccessAlertT("You must restart Dolphin in order for the change to take effect.");
		SConfig::GetInstance().m_InterfaceLanguage = langIds[InterfaceLang->GetSelection()];
		break;
	case ID_HOTKEY_CONFIG:
		{
			CConfigVR m_HotkeyDialog(this);
			m_HotkeyDialog.ShowModal();
		}
		// Update the GUI in case menu accelerators were changed
		main_frame->UpdateGUI();
		break;
	}
#endif
}

// Save keyboard key mapping
void CConfigVR::SaveButtonMapping(int Id, int Key, int Modkey)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[Id] = Modkey;
}

void CConfigVR::EndGetButtons()
{
	wxTheApp->Unbind(wxEVT_KEY_DOWN, &CConfigVR::OnKeyDown, this);
	//m_ButtonMappingTimer.Stop();
	//GetButtonWaitingTimer = 0;
	//GetButtonWaitingID = 0;
	ClickedButton = nullptr;
	SetEscapeId(wxID_ANY);
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
			SaveButtonMapping(ClickedButton->GetId(), -1, 0);
			SetButtonText(ClickedButton->GetId(), wxString());
		}
		else
		{
			// Check if the hotkey combination was already applied to another button
			// and unapply it if necessary.
			for (wxButton* btn : m_Button_VRSettings)
			{
				// We compare against this to see if we have a duplicate bind attempt.
				wxString existingHotkey = btn->GetLabel();

				wxString tentativeModKey = InputCommon::WXKeymodToString(g_Modkey);
				wxString tentativePressedKey = InputCommon::WXKeyToString(g_Pressed);
				wxString tentativeHotkey(tentativeModKey + tentativePressedKey);

				// Found a button that already has this binding. Unbind it.
				if (tentativeHotkey == existingHotkey)
				{
					SaveButtonMapping(btn->GetId(), -1, 0);
					SetButtonText(btn->GetId(), wxString());
				}
			}

			// Proceed to apply the binding to the selected button.
			SetButtonText(ClickedButton->GetId(),
				InputCommon::WXKeyToString(g_Pressed),
				InputCommon::WXKeymodToString(g_Modkey));
			SaveButtonMapping(ClickedButton->GetId(), g_Pressed, g_Modkey);
		}
		EndGetButtons();
	}
}

// Update the textbox for the buttons
void CConfigVR::SetButtonText(int id, const wxString &keystr, const wxString &modkeystr)
{
	m_Button_VRSettings[id]->SetLabel(modkeystr + keystr);
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
	SetEscapeId(wxID_CANCEL);
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(_("<Press Key>"));
	//DoGetButtons(ClickedButton->GetId());
}