// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/font.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "DolphinWX/HotkeyDlg.h"
#include "DolphinWX/WXInputBase.h"

HotkeyConfigDialog::HotkeyConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
		const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
, m_ButtonMappingTimer(this)
{
	CreateHotkeyGUIControls();

	Bind(wxEVT_BUTTON, &HotkeyConfigDialog::OnButtonClick, this, 0, NUM_HOTKEYS - 1);
	Bind(wxEVT_TIMER, &HotkeyConfigDialog::OnButtonTimer, this);

	g_Pressed = 0;
	g_Modkey = 0;
	ClickedButton = nullptr;
	GetButtonWaitingID = 0;
	GetButtonWaitingTimer = 0;
}

HotkeyConfigDialog::~HotkeyConfigDialog()
{
}

// Save keyboard key mapping
void HotkeyConfigDialog::SaveButtonMapping(int Id, int Key, int Modkey)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[Id] = Modkey;
}

void HotkeyConfigDialog::EndGetButtons()
{
	m_ButtonMappingTimer.Stop();
	GetButtonWaitingTimer = 0;
	GetButtonWaitingID = 0;
	ClickedButton = nullptr;
	SetEscapeId(wxID_ANY);
}

void HotkeyConfigDialog::OnKeyDown(wxKeyEvent& event)
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
			for (wxButton* btn : m_Button_Hotkeys)
			{
				// We compare against this to see if we have a duplicate bind attempt.
				wxString existingHotkey = btn->GetLabel();

				wxString tentativeModKey = WxUtils::WXKeymodToString(g_Modkey);
				wxString tentativePressedKey = WxUtils::WXKeyToString(g_Pressed);
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
					WxUtils::WXKeyToString(g_Pressed),
					WxUtils::WXKeymodToString(g_Modkey));
			SaveButtonMapping(ClickedButton->GetId(), g_Pressed, g_Modkey);
		}
		EndGetButtons();
	}
}

// Update the textbox for the buttons
void HotkeyConfigDialog::SetButtonText(int id, const wxString &keystr, const wxString &modkeystr)
{
	m_Button_Hotkeys[id]->SetLabel(modkeystr + keystr);
}

void HotkeyConfigDialog::DoGetButtons(int _GetId)
{
	// Values used in this function
	const int Seconds = 4; // Seconds to wait for
	const int TimesPerSecond = 40; // How often to run the check

	// If the Id has changed or the timer is not running we should start one
	if ( GetButtonWaitingID != _GetId || !m_ButtonMappingTimer.IsRunning() )
	{
		if (m_ButtonMappingTimer.IsRunning())
			m_ButtonMappingTimer.Stop();

		// Save the button Id
		GetButtonWaitingID = _GetId;
		GetButtonWaitingTimer = 0;

		// Start the timer
		m_ButtonMappingTimer.Start(1000 / TimesPerSecond);
	}

	// Process results
	// Count each time
	GetButtonWaitingTimer++;

	// This is run every second
	if (GetButtonWaitingTimer % TimesPerSecond == 0)
	{
		// Current time
		int TmpTime = Seconds - (GetButtonWaitingTimer / TimesPerSecond);
		// Update text
		SetButtonText(_GetId, wxString::Format("[ %d ]", TmpTime));
	}

	// Time's up
	if (GetButtonWaitingTimer / TimesPerSecond >= Seconds)
	{
		// Revert back to old label
		SetButtonText(_GetId, OldLabel);
		EndGetButtons();
	}
}

// Input button clicked
void HotkeyConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	event.Skip();

	if (m_ButtonMappingTimer.IsRunning())
		return;

	// Get the button
	ClickedButton = (wxButton *)event.GetEventObject();
	SetEscapeId(wxID_CANCEL);
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(_("<Press Key>"));
	DoGetButtons(ClickedButton->GetId());
}

#define HOTKEY_NUM_COLUMNS 2

const wxString hkText[] =
{
	wxTRANSLATE("Open"),
	wxTRANSLATE("Change Disc"),
	wxTRANSLATE("Refresh List"),

	wxTRANSLATE("Play/Pause"),
	wxTRANSLATE("Stop"),
	wxTRANSLATE("Reset"),
	wxTRANSLATE("Frame Advance"),

	wxTRANSLATE("Start Recording"),
	wxTRANSLATE("Play Recording"),
	wxTRANSLATE("Export Recording"),
	wxTRANSLATE("Read-only mode"),

	wxTRANSLATE("Toggle Fullscreen"),
	wxTRANSLATE("Take Screenshot"),
	wxTRANSLATE("Exit"),

	wxTRANSLATE("Connect Wiimote 1"),
	wxTRANSLATE("Connect Wiimote 2"),
	wxTRANSLATE("Connect Wiimote 3"),
	wxTRANSLATE("Connect Wiimote 4"),
	wxTRANSLATE("Connect Balance Board"),

	wxTRANSLATE("Volume Down"),
	wxTRANSLATE("Volume Up"),
	wxTRANSLATE("Volume Toggle Mute"),

	wxTRANSLATE("Increase IR"),
	wxTRANSLATE("Decrease IR"),

	wxTRANSLATE("Toggle IR"),
	wxTRANSLATE("Toggle Aspect Ratio"),
	wxTRANSLATE("Toggle EFB Copies"),
	wxTRANSLATE("Toggle Fog"),
	wxTRANSLATE("Toggle Frame limit"),
	wxTRANSLATE("Decrease Frame limit"),
	wxTRANSLATE("Increase Frame limit"),

	wxTRANSLATE("Freelook Decrease Speed"),
	wxTRANSLATE("Freelook Increase Speed"),
	wxTRANSLATE("Freelook Reset Speed"),
	wxTRANSLATE("Freelook Move Up"),
	wxTRANSLATE("Freelook Move Down"),
	wxTRANSLATE("Freelook Move Left"),
	wxTRANSLATE("Freelook Move Right"),
	wxTRANSLATE("Freelook Zoom In"),
	wxTRANSLATE("Freelook Zoom Out"),
	wxTRANSLATE("Freelook Reset"),

	wxTRANSLATE("Decrease Depth"),
	wxTRANSLATE("Increase Depth"),
	wxTRANSLATE("Decrease Convergence"),
	wxTRANSLATE("Increase Convergence"),

	wxTRANSLATE("Load State Slot 1"),
	wxTRANSLATE("Load State Slot 2"),
	wxTRANSLATE("Load State Slot 3"),
	wxTRANSLATE("Load State Slot 4"),
	wxTRANSLATE("Load State Slot 5"),
	wxTRANSLATE("Load State Slot 6"),
	wxTRANSLATE("Load State Slot 7"),
	wxTRANSLATE("Load State Slot 8"),
	wxTRANSLATE("Load State Slot 9"),
	wxTRANSLATE("Load State Slot 10"),

	wxTRANSLATE("Save State Slot 1"),
	wxTRANSLATE("Save State Slot 2"),
	wxTRANSLATE("Save State Slot 3"),
	wxTRANSLATE("Save State Slot 4"),
	wxTRANSLATE("Save State Slot 5"),
	wxTRANSLATE("Save State Slot 6"),
	wxTRANSLATE("Save State Slot 7"),
	wxTRANSLATE("Save State Slot 8"),
	wxTRANSLATE("Save State Slot 9"),
	wxTRANSLATE("Save State Slot 10"),

	wxTRANSLATE("Select State Slot 1"),
	wxTRANSLATE("Select State Slot 2"),
	wxTRANSLATE("Select State Slot 3"),
	wxTRANSLATE("Select State Slot 4"),
	wxTRANSLATE("Select State Slot 5"),
	wxTRANSLATE("Select State Slot 6"),
	wxTRANSLATE("Select State Slot 7"),
	wxTRANSLATE("Select State Slot 8"),
	wxTRANSLATE("Select State Slot 9"),
	wxTRANSLATE("Select State Slot 10"),

	wxTRANSLATE("Save to selected slot"),
	wxTRANSLATE("Load from selected slot"),

	wxTRANSLATE("Load State Last 1"),
	wxTRANSLATE("Load State Last 2"),
	wxTRANSLATE("Load State Last 3"),
	wxTRANSLATE("Load State Last 4"),
	wxTRANSLATE("Load State Last 5"),
	wxTRANSLATE("Load State Last 6"),
	wxTRANSLATE("Load State Last 7"),
	wxTRANSLATE("Load State Last 8"),

	wxTRANSLATE("Save Oldest State"),
	wxTRANSLATE("Undo Load State"),
	wxTRANSLATE("Undo Save State"),
	wxTRANSLATE("Save State"),
	wxTRANSLATE("Load State"),
};

void HotkeyConfigDialog::CreateHotkeyGUIControls()
{
	const wxString pageNames[] =
	{
		_("General"),
		_("State Saves")
	};

	const int page_breaks[3] = {HK_OPEN, HK_LOAD_STATE_SLOT_1, NUM_HOTKEYS};

	// Configuration controls sizes
	wxSize size(100,20);
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxNotebook *Notebook = new wxNotebook(this, wxID_ANY);

	for (int j = 0; j < 2; j++)
	{
		wxPanel *Page = new wxPanel(Notebook);
		Notebook->AddPage(Page, pageNames[j]);

		wxGridBagSizer *sHotkeys = new wxGridBagSizer();

		// Header line
		for (int i = 0; i < HOTKEY_NUM_COLUMNS; i++)
		{
			wxBoxSizer *HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText *StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Action"));
			HeaderSizer->Add(StaticTextHeader, 1, wxALL, 2);
			StaticTextHeader = new wxStaticText(Page, wxID_ANY, _("Key"), wxDefaultPosition, size);
			HeaderSizer->Add(StaticTextHeader, 0, wxALL, 2);
			sHotkeys->Add(HeaderSizer, wxGBPosition(0, i), wxDefaultSpan, wxEXPAND | wxLEFT, (i > 0) ? 30 : 1);
		}

		int column_break = (page_breaks[j+1] + page_breaks[j] + 1) / 2;

		for (int i = page_breaks[j]; i < page_breaks[j+1]; i++)
		{
			// Text for the action
			wxStaticText *stHotkeys = new wxStaticText(Page, wxID_ANY, wxGetTranslation(hkText[i]));

			// Key selection button
			m_Button_Hotkeys[i] = new wxButton(Page, i, wxEmptyString, wxDefaultPosition, size);
			m_Button_Hotkeys[i]->SetFont(m_SmallFont);
			m_Button_Hotkeys[i]->SetToolTip(_("Left click to detect hotkeys.\nEnter space to clear."));
			m_Button_Hotkeys[i]->Bind(wxEVT_KEY_DOWN, &HotkeyConfigDialog::OnKeyDown, this);
			SetButtonText(i,
					WxUtils::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[i]),
					WxUtils::WXKeymodToString(
						SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[i]));

			wxBoxSizer *sHotkey = new wxBoxSizer(wxHORIZONTAL);
			sHotkey->Add(stHotkeys, 1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 2);
			sHotkey->Add(m_Button_Hotkeys[i], 0, wxALL, 2);
			sHotkeys->Add(sHotkey,
					wxGBPosition((i < column_break) ? i - page_breaks[j] + 1 : i - column_break + 1,
						(i < column_break) ? 0 : 1),
					wxDefaultSpan, wxEXPAND | wxLEFT, (i < column_break) ? 1 : 30);
		}

		wxStaticBoxSizer *sHotkeyBox = new wxStaticBoxSizer(wxVERTICAL, Page, _("Hotkeys"));
		sHotkeyBox->Add(sHotkeys);

		wxBoxSizer* const sPage = new wxBoxSizer(wxVERTICAL);
		sPage->Add(sHotkeyBox, 0, wxEXPAND | wxALL, 5);
		Page->SetSizer(sPage);
	}

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(Notebook, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxDOWN, 5);
	SetSizerAndFit(sMainSizer);
	SetFocus();
}

