// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "HotkeyDlg.h"
#include "ConfigManager.h"

BEGIN_EVENT_TABLE(HotkeyConfigDialog,wxDialog)
	EVT_CLOSE(HotkeyConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, HotkeyConfigDialog::CloseClick)
	EVT_BUTTON(ID_FULLSCREEN, HotkeyConfigDialog::OnButtonClick)
	EVT_BUTTON(ID_PLAY_PAUSE, HotkeyConfigDialog::OnButtonClick)
	EVT_BUTTON(ID_STOP, HotkeyConfigDialog::OnButtonClick)
	EVT_TIMER(IDTM_BUTTON, HotkeyConfigDialog::OnButtonTimer)
END_EVENT_TABLE()

HotkeyConfigDialog::HotkeyConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
		const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	CreateHotkeyGUIControls();

#if wxUSE_TIMER
	m_ButtonMappingTimer = new wxTimer(this, IDTM_BUTTON);
	g_Pressed = 0;
	g_Modkey = 0;
	ClickedButton = NULL;
	GetButtonWaitingID = 0;
	GetButtonWaitingTimer = 0;
#endif
}

HotkeyConfigDialog::~HotkeyConfigDialog()
{
	if (m_ButtonMappingTimer)
		delete m_ButtonMappingTimer;
}

void HotkeyConfigDialog::OnClose(wxCloseEvent& WXUNUSED (event))
{
	if (m_ButtonMappingTimer)
		m_ButtonMappingTimer->Stop();

	EndModal(wxID_CLOSE);
}

void HotkeyConfigDialog::CloseClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
		case ID_CLOSE:
			Close();
			break;
	}
}

// Save keyboard key mapping
void HotkeyConfigDialog::SaveButtonMapping(int Id, int Key, int Modkey)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[Id] = Key;
	SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[Id] = Modkey;
}

void HotkeyConfigDialog::EndGetButtons(void)
{
	wxTheApp->Disconnect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
			wxKeyEventHandler(HotkeyConfigDialog::OnKeyDown),
			(wxObject*)0, this);
	m_ButtonMappingTimer->Stop();
	GetButtonWaitingTimer = 0;
	GetButtonWaitingID = 0;
	ClickedButton = NULL;
}

void HotkeyConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	if(ClickedButton != NULL)
	{
		// Save the key
		g_Pressed = event.GetKeyCode();
		g_Modkey = event.GetModifiers();

		// Don't allow modifier keys
		if (g_Pressed == WXK_CONTROL || g_Pressed == WXK_ALT || g_Pressed == WXK_SHIFT)
			return;

		// Use the space key to set a blank key
		if (g_Pressed == WXK_SPACE)
		{
			SaveButtonMapping(ClickedButton->GetId(), -1, 0);
			SetButtonText(ClickedButton->GetId(), wxString());
		}
		else
		{
			SetButtonText(ClickedButton->GetId(),
				   	InputCommon::WXKeyToString(g_Pressed),
				   	InputCommon::WXKeymodToString(g_Modkey));
			SaveButtonMapping(ClickedButton->GetId(), g_Pressed, g_Modkey);
		}
		EndGetButtons();
	}
}

// Update the textbox for the buttons
void HotkeyConfigDialog::SetButtonText(int id, const wxString &keystr, const wxString &modkeystr)
{
	m_Button_Hotkeys[id]->SetLabel(modkeystr + (modkeystr.Len() ? _T("+") : _T("")) + keystr);
}

void HotkeyConfigDialog::DoGetButtons(int _GetId)
{
	// Values used in this function
	int Seconds = 4; // Seconds to wait for
	int TimesPerSecond = 40; // How often to run the check

	// If the Id has changed or the timer is not running we should start one
	if( GetButtonWaitingID != _GetId || !m_ButtonMappingTimer->IsRunning() )
	{
		if(m_ButtonMappingTimer->IsRunning())
			m_ButtonMappingTimer->Stop();

		 // Save the button Id
		GetButtonWaitingID = _GetId;
		GetButtonWaitingTimer = 0;

		// Start the timer
		#if wxUSE_TIMER
		m_ButtonMappingTimer->Start(1000 / TimesPerSecond);
		#endif
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
		SetButtonText(_GetId, wxString::Format(wxT("[ %d ]"), TmpTime));
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

	if (m_ButtonMappingTimer->IsRunning()) return;

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
			wxKeyEventHandler(HotkeyConfigDialog::OnKeyDown),
			(wxObject*)0, this);

	// Get the button
	ClickedButton = (wxButton *)event.GetEventObject();
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(wxT("<Press Key>"));
	DoGetButtons(ClickedButton->GetId());
}

void HotkeyConfigDialog::CreateHotkeyGUIControls(void)
{
	static const wxChar* hkText[] =
	{
		wxT("Toggle Fullscreen"),
		wxT("Play/Pause"),
		wxT("Stop"),
	};

	// Configuration controls sizes
	wxSize size(100,20);
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxStaticBoxSizer *sHotkeys = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Hotkeys"));

	// Header line
	wxBoxSizer *HeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText *StaticTextHeader = new wxStaticText(this, wxID_ANY, wxT("Action"));
	HeaderSizer->Add(StaticTextHeader, 1, wxALL, 2);
	HeaderSizer->AddStretchSpacer();
	StaticTextHeader = new wxStaticText(this, wxID_ANY, wxT("Key"), wxDefaultPosition, size);
	HeaderSizer->Add(StaticTextHeader, 0, wxALL, 2);
	sHotkeys->Add(HeaderSizer, 0, wxEXPAND | wxALL, 1);

	for (int i = 0; i < NUM_HOTKEYS; i++)
	{
		// Text for the action
		wxStaticText *stHotkeys = new wxStaticText(this, wxID_ANY, hkText[i]);

		// Key selection button
		m_Button_Hotkeys[i] = new wxButton(this, ID_FULLSCREEN + i, wxEmptyString,
				wxDefaultPosition, size);
		m_Button_Hotkeys[i]->SetFont(m_SmallFont);
		SetButtonText(i,
				InputCommon::WXKeyToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[i]),
				InputCommon::WXKeymodToString(SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[i]));

		wxBoxSizer *sHotkey = new wxBoxSizer(wxHORIZONTAL);
		sHotkey->Add(stHotkeys, 1, wxALIGN_LEFT | wxALL, 2);
		sHotkey->AddStretchSpacer();
		sHotkey->Add(m_Button_Hotkeys[i], 0, wxALL, 2);
		sHotkeys->Add(sHotkey, 0, wxEXPAND | wxALL, 1);
	}

	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));
	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Close, 0, (wxLEFT), 5);	

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(sHotkeys, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
	SetSizer(sMainSizer);
	Layout();
	Fit();
}

