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

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigBasicDlg.h"
#include "ConfigPadDlg.h"
#include "Config.h"
#include "EmuMain.h" // for SetDefaultExtensionRegistry
#include "EmuSubroutines.h" // for WmRequestStatus

BEGIN_EVENT_TABLE(WiimoteBasicConfigDialog,wxDialog)
	EVT_CLOSE(WiimoteBasicConfigDialog::OnClose)
	EVT_BUTTON(wxID_OK, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(wxID_CANCEL, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONMAPPING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONRECORDING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, WiimoteBasicConfigDialog::NotebookPageChanged)

	EVT_BUTTON(IDB_PAIRUP_REAL, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(IDB_REFRESH_REAL, WiimoteBasicConfigDialog::ButtonClick)

	EVT_CHOICE(IDC_INPUT_SOURCE, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_SIDEWAYSWIIMOTE, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_UPRIGHTWIIMOTE, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_MOTIONPLUSCONNECTED, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_WIIAUTORECONNECT, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_WIIAUTOUNPAIR, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_WIIAUTOPAIR, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_EXTCONNECTED, WiimoteBasicConfigDialog::GeneralSettingsChanged)

	//UDPWii
	EVT_CHECKBOX(IDC_UDPWIIENABLE, WiimoteBasicConfigDialog::UDPWiiSettingsChanged)
	EVT_CHECKBOX(IDC_UDPWIIACCEL, WiimoteBasicConfigDialog::UDPWiiSettingsChanged)
	EVT_CHECKBOX(IDC_UDPWIIBUTT, WiimoteBasicConfigDialog::UDPWiiSettingsChanged)
	EVT_CHECKBOX(IDC_UDPWIIIR, WiimoteBasicConfigDialog::UDPWiiSettingsChanged)
	EVT_CHECKBOX(IDC_UDPWIINUN, WiimoteBasicConfigDialog::UDPWiiSettingsChanged)
	EVT_TEXT(IDT_UDPWIIPORT, WiimoteBasicConfigDialog::UDPWiiSettingsChanged)

	// IR cursor
	EVT_COMMAND_SCROLL(IDS_WIDTH, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_HEIGHT, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_LEFT, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_TOP, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_LEVEL, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_TIMEOUT, WiimoteBasicConfigDialog::IRCursorChanged)//scrollevent

	EVT_TIMER(IDTM_UPDATE_ONCE, WiimoteBasicConfigDialog::UpdateOnce)
END_EVENT_TABLE()


WiimoteBasicConfigDialog::WiimoteBasicConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	#if wxUSE_TIMER
		m_TimeoutOnce = new wxTimer(this, IDTM_UPDATE_ONCE);
		m_ShutDownTimer = new wxTimer(this, IDTM_SHUTDOWN);
	#endif

	ControlsCreated = false;
	m_Page = 0;

#if defined HAVE_WIIUSE && HAVE_WIIUSE
	// Initialize the Real WiiMotes here, so we get a count of how many were found and set everything properly
	WiiMoteReal::Initialize();
#endif

	CreateGUIControls();
	UpdateGUI();
}

void WiimoteBasicConfigDialog::OnClose(wxCloseEvent& event)
{
	EndModal(wxID_CLOSE);
}

void WiimoteBasicConfigDialog::ButtonClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case wxID_OK:
#if defined HAVE_WIIUSE && HAVE_WIIUSE
		WiiMoteReal::Allocate();
#endif
		g_Config.Save();
		Close();
		break;
	case wxID_CANCEL:
		g_Config.Load();
		Close();
		break;
	case ID_BUTTONMAPPING:
		g_Config.CurrentPage = m_Page;
		m_PadConfigFrame = new WiimotePadConfigDialog(this);
		m_PadConfigFrame->ShowModal();
		m_PadConfigFrame->Destroy();
		m_PadConfigFrame = NULL;
		m_Page = g_Config.CurrentPage;
		m_Notebook->ChangeSelection(g_Config.CurrentPage);
		UpdateGUI();
		break;
#ifdef _WIN32
	case IDB_PAIRUP_REAL:
		if (g_EmulatorState != PLUGIN_EMUSTATE_PLAY)
		{
			WiiMoteReal::g_StartAutopairThread.Set();		
		}
		break;
#endif
	case IDB_REFRESH_REAL:
		// If the config dialog was open when the user click on Play/Pause, the GUI was not updated, so this could crash everything!
		if (g_EmulatorState != PLUGIN_EMUSTATE_PLAY)
		{
			DoRefreshReal();
		}
		UpdateGUI();
		break;
	}
}

void WiimoteBasicConfigDialog::CreateGUIControls()
{
	wxArrayString arrayStringFor_source;
	arrayStringFor_source.Add(wxT("Inactive"));
	arrayStringFor_source.Add(wxT("Emulated Wiimote"));
	arrayStringFor_source.Add(wxT("Real Wiimote"));	

	wxArrayString arrayStringFor_extension;
	arrayStringFor_extension.Add(wxT("None"));
	arrayStringFor_extension.Add(wxT("Nunchuck"));
	arrayStringFor_extension.Add(wxT("Classic Controller"));
	arrayStringFor_extension.Add(wxT("Guitar Hero 3 Guitar"));

	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		m_Controller[i] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1 + i, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Controller[i], wxString::Format(wxT("Wiimote %d"), i+1));

		// Basic Settings
		m_InputSource[i] = new wxChoice(m_Controller[i], IDC_INPUT_SOURCE, wxDefaultPosition, wxDefaultSize, arrayStringFor_source, 0, wxDefaultValidator);
		m_InputSource[i]->SetToolTip(wxT("This can only be changed when the emulator is paused or stopped."));

		// Emulated Wiimote
		m_SidewaysWiimote[i] = new wxCheckBox(m_Controller[i], IDC_SIDEWAYSWIIMOTE, wxT("Sideways Wiimote"));
		m_SidewaysWiimote[i]->SetToolTip(wxT("Treat the sideways position as neutral"));
		m_UprightWiimote[i] = new wxCheckBox(m_Controller[i], IDC_UPRIGHTWIIMOTE, wxT("Upright Wiimote"));
		m_UprightWiimote[i]->SetToolTip(wxT("Treat the upright position as neutral"));

		m_WiiMotionPlusConnected[i] = new wxCheckBox(m_Controller[i], IDC_MOTIONPLUSCONNECTED, wxT("Wii Motion Plus Connected"));

		m_Extension[i] = new wxChoice(m_Controller[i], IDC_EXTCONNECTED, wxDefaultPosition, wxDefaultSize, arrayStringFor_extension, 0, wxDefaultValidator);
		
		// UDPWii
		m_UDPWiiEnable[i] = new wxCheckBox(m_Controller[i], IDC_UDPWIIENABLE, wxT("Enable UDPWii"));
		m_UDPWiiEnable[i]->SetToolTip(wxT("Enable listening for wiimote data on the network. Requires emulation restart"));
		m_UDPWiiAccel[i] = new wxCheckBox(m_Controller[i], IDC_UDPWIIACCEL, wxT("Update acceleration from UDPWii"));
		m_UDPWiiAccel[i]->SetToolTip(wxT("Retrieve acceleration data from a device on the network"));
		m_UDPWiiButt[i] = new wxCheckBox(m_Controller[i], IDC_UDPWIIBUTT, wxT("Update buttons from UDPWii"));
		m_UDPWiiButt[i]->SetToolTip(wxT("Retrieve button data from a device on the network. This doesn't affect keyboard mappings"));
		m_UDPWiiIR[i] = new wxCheckBox(m_Controller[i], IDC_UDPWIIIR, wxT("Update IR pointer from UDPWii"));
		m_UDPWiiIR[i]->SetToolTip(wxT("Retrieve IR pointer from a device on the network. Disables using mouse as pointer"));
		m_UDPWiiNun[i] = new wxCheckBox(m_Controller[i], IDC_UDPWIINUN, wxT("Update nunchuck from UDPWii"));
		m_UDPWiiNun[i]->SetToolTip(wxT("Retrieve nunchuck data from a device on the network."));
		m_UDPWiiPort[i] = new wxTextCtrl(m_Controller[i], IDT_UDPWIIPORT);
		m_UDPWiiPort[i]->SetMaxLength(9);
		m_TextUDPWiiPort[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("UDP Port:"));
		m_TextUDPWiiPort[i]->SetToolTip(wxT("The UDP port on witch UDPWii listens for this remote."));

#ifdef _WIN32
		m_PairUpRealWiimote[i] = new wxButton(m_Controller[i], IDB_PAIRUP_REAL, wxT("Pair Up"));
		m_PairUpRealWiimote[i]->SetToolTip(wxT("Press the Buttons 1 and 2 on your Wiimote.\nThis might take a few seconds.\nIt only works if you are using Microsoft Bluetooth stack.")); // Only working with MS BT Stack.
#endif

		m_TextFoundRealWiimote[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Connected to 0 Real Wiimotes"));
		m_ConnectRealWiimote[i] = new wxButton(m_Controller[i], IDB_REFRESH_REAL, wxT("Refresh"));
		m_ConnectRealWiimote[i]->SetToolTip(wxT("This can only be done when the emulator is paused or stopped."));

		m_TextWiimoteTimeout[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Timeout: 000 ms"));
		m_WiimoteTimeout[i] = new wxSlider(m_Controller[i], IDS_TIMEOUT, 0, 10, 200, wxDefaultPosition, wxSize(75, -1));
		m_WiimoteTimeout[i]->SetToolTip(wxT("General Real Wiimote Read Timeout, Default: 10 (ms). Higher values might eliminate frequent disconnects."));

#ifdef _WIN32
		//Real Wiimote / automatic settings
		m_WiiAutoReconnect[i] = new wxCheckBox(m_Controller[i], IDC_WIIAUTORECONNECT, wxT("Reconnect Wiimote on disconnect"));
		m_WiiAutoReconnect[i]->SetToolTip(wxT("This makes dolphin automatically reconnect a wiimote when it has being disconnected.\nThis will cause problems when 2 controllers are connected for a 1 player game."));
		m_WiiAutoUnpair[i] = new wxCheckBox(m_Controller[i], IDC_WIIAUTOUNPAIR, wxT("Unpair Wiimote on close"));
		m_WiiAutoUnpair[i]->SetToolTip(wxT("This makes dolphin automatically unpair a wiimote when dolphin is about to be closed."));
		m_WiiExtendedPairUp[i] = new wxCheckBox(m_Controller[i], IDC_WIIAUTOPAIR, wxT("Extended PairUp/Connect"));
		m_WiiExtendedPairUp[i]->SetToolTip(wxT("This makes dolphin automatically pair up and connect Wiimotes on pressing 1+2 on your Wiimote."));
#endif
		
		//IR Pointer
		m_TextScreenWidth[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Width: 000"));
		m_TextScreenHeight[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Height: 000"));
		m_TextScreenLeft[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left: 000"));
		m_TextScreenTop[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Top: 000"));
		m_TextScreenIrLevel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Sensivity: 000"));

		m_SliderWidth[i] = new wxSlider(m_Controller[i], IDS_WIDTH, 0, 100, 923, wxDefaultPosition, wxSize(75, -1));
		m_SliderHeight[i] = new wxSlider(m_Controller[i], IDS_HEIGHT, 0, 0, 727, wxDefaultPosition, wxSize(75, -1));
		m_SliderLeft[i] = new wxSlider(m_Controller[i], IDS_LEFT, 0, 100, 500, wxDefaultPosition, wxSize(75, -1));
		m_SliderTop[i] = new wxSlider(m_Controller[i], IDS_TOP, 0, 0, 500, wxDefaultPosition, wxSize(75, -1));
		m_SliderIrLevel[i] = new wxSlider(m_Controller[i], IDS_LEVEL, 0, 1, 5, wxDefaultPosition, wxSize(75, -1));

		// These are changed from the graphics plugin settings, so they are just here to show the loaded status
		m_TextAR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Aspect Ratio"));
		m_CheckAR43[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("4:3"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_CheckAR43[i]->Enable(false);
		m_CheckAR169[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("16:9"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_CheckAR169[i]->Enable(false);
		m_Crop[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Crop"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_Crop[i]->Enable(false);

		// Sizers
		m_SizeBasic[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Input Source"));
		m_SizeBasic[i]->Add(m_InputSource[i], 0, wxEXPAND | wxALL, 5);

		m_SizeEmu[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Position"));
		m_SizeEmu[i]->Add(m_SidewaysWiimote[i], 0, wxEXPAND | wxALL, 5);
		m_SizeEmu[i]->Add(m_UprightWiimote[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);

		m_SizeExtensions[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Extension"));
		m_SizeExtensions[i]->Add(m_WiiMotionPlusConnected[i], 0, wxEXPAND | wxALL, 5);
		m_SizeExtensions[i]->Add(m_Extension[i], 0, wxEXPAND | wxALL, 5);

		m_SizeUDPWii[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated UDPWii"));
		m_SizeUDPWii[i]->Add(m_UDPWiiEnable[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWiiPort[i]= new wxBoxSizer(wxHORIZONTAL);
		m_SizeUDPWiiPort[i]->Add(m_TextUDPWiiPort[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWiiPort[i]->Add(m_UDPWiiPort[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWii[i]->Add(m_SizeUDPWiiPort[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWii[i]->Add(m_UDPWiiAccel[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWii[i]->Add(m_UDPWiiButt[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWii[i]->Add(m_UDPWiiIR[i], 0, wxEXPAND | wxALL,1);
		m_SizeUDPWii[i]->Add(m_UDPWiiNun[i], 0, wxEXPAND | wxALL,1);

#ifdef _WIN32
		m_SizeRealAuto[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Automatic"));
		m_SizeRealAuto[i]->Add(m_WiiAutoReconnect[i], 0, wxEXPAND | (wxDOWN | wxTOP), 5);
		m_SizeRealAuto[i]->Add(m_WiiAutoUnpair[i], 0, wxEXPAND | (wxDOWN | wxTOP), 5);
		m_SizeRealAuto[i]->Add(m_WiiExtendedPairUp[i], 0, wxEXPAND | (wxDOWN | wxTOP), 5);
#endif

		m_SizeRealTimeout[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizeRealTimeout[i]->Add(m_TextWiimoteTimeout[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizeRealTimeout[i]->Add(m_WiimoteTimeout[i], 0, wxEXPAND | (wxRIGHT), 0);

		m_SizeRealRefreshPair[i] = new wxBoxSizer(wxHORIZONTAL);
#ifdef _WIN32
		m_SizeRealRefreshPair[i]->Add(m_PairUpRealWiimote[i], 0, wxEXPAND | wxALL, 5);
#endif
		m_SizeRealRefreshPair[i]->Add(m_ConnectRealWiimote[i], 0, wxEXPAND | wxALL, 5);

		m_SizeReal[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Real Wiimote"));
		m_SizeReal[i]->Add(m_TextFoundRealWiimote[i], 0, wxALIGN_CENTER | wxALL, 5);
		m_SizeReal[i]->Add(m_SizeRealRefreshPair[i], 0, wxALIGN_CENTER | (wxLEFT | wxDOWN | wxRIGHT), 5);
#ifdef _WIN32
		m_SizeReal[i]->Add(m_SizeRealAuto[i], 0, wxEXPAND | wxALL, 5);
#endif
		m_SizeReal[i]->Add(m_SizeRealTimeout[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);

		m_SizerIRPointerWidth[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerWidth[i]->Add(m_TextScreenLeft[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerWidth[i]->Add(m_SliderLeft[i], 0, wxEXPAND | (wxRIGHT), 0);
		m_SizerIRPointerWidth[i]->Add(m_TextScreenWidth[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerWidth[i]->Add(m_SliderWidth[i], 0, wxEXPAND | (wxLEFT), 0);

		m_SizerIRPointerHeight[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerHeight[i]->Add(m_TextScreenTop[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerHeight[i]->Add(m_SliderTop[i], 0, wxEXPAND | (wxRIGHT), 0);
		m_SizerIRPointerHeight[i]->Add(m_TextScreenHeight[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerHeight[i]->Add(m_SliderHeight[i], 0, wxEXPAND | (wxLEFT), 0);

		m_SizerIRPointerScreen[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerScreen[i]->Add(m_TextAR[i], 0, wxEXPAND | (wxTOP), 0);
		m_SizerIRPointerScreen[i]->Add(m_CheckAR43[i], 0, wxEXPAND | (wxLEFT), 5);
		m_SizerIRPointerScreen[i]->Add(m_CheckAR169[i], 0, wxEXPAND | (wxLEFT), 5);
		m_SizerIRPointerScreen[i]->Add(m_Crop[i], 0, wxEXPAND | (wxLEFT), 5);

		m_SizerIRPointerSensitivity[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerSensitivity[i]->Add(m_TextScreenIrLevel[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerSensitivity[i]->Add(m_SliderIrLevel[i], 0, wxEXPAND | (wxRIGHT), 0);

		m_SizerIRPointer[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("IR Pointer"));
		m_SizerIRPointer[i]->Add(m_SizerIRPointerWidth[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerHeight[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerSensitivity[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerScreen[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 10);

		m_SizeBasicGeneralLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeBasic[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeEmu[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeExtensions[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeUDPWii[i], 0, wxEXPAND | (wxUP), 5);

		m_SizeBasicGeneralRight[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeBasicGeneralRight[i]->Add(m_SizeReal[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralRight[i]->Add(m_SizerIRPointer[i], 0, wxEXPAND | (wxUP), 5);

		m_SizeBasicGeneral[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralLeft[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralRight[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_SizeBasicGeneral[i]);
	}

	m_ButtonMapping = new wxButton(this, ID_BUTTONMAPPING, wxT("Button Mapping"));

	m_OK = new wxButton(this, wxID_OK, wxT("OK"));
	m_OK->SetToolTip(wxT("Save changes and close"));
	m_Cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	m_Cancel->SetToolTip(wxT("Discard changes and close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_ButtonMapping, 0, (wxALL), 0);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_OK, 0, (wxALL), 0);
	sButtons->Add(m_Cancel, 0, (wxLEFT), 5);	

	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	SetSizer(m_MainSizer);
	Layout();
	Fit();
	// Center the window if there is room for it
	#ifdef _WIN32
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 600)
			Center();
	#endif

	ControlsCreated = true;
}

// Execute a delayed function
void WiimoteBasicConfigDialog::UpdateOnce(wxTimerEvent& event)
{
	switch(event.GetId())
	{
	case IDTM_UPDATE_ONCE:
		// Reenable the checkbox
		SetCursor(wxCursor(wxCURSOR_ARROW));
		UpdateGUI();
		break;
	}
}

#if HAVE_WIIUSE
void WiimoteBasicConfigDialog::DoRefreshReal()
{
	WiiMoteReal::Shutdown();
	WiiMoteReal::Initialize();
}


void WiimoteBasicConfigDialog::UpdateBasicConfigDialog(bool state) {
#ifdef _WIN32
	if (m_BasicConfigFrame != NULL) {
		if (state) {
			m_PairUpRealWiimote[m_Page]->Enable(true);
			m_BasicConfigFrame->UpdateGUI();
		}
		else 
			m_PairUpRealWiimote[m_Page]->Enable(false);
	}
#endif
}


void WiimoteBasicConfigDialog::DoUseReal()
{
	if (!g_RealWiiMotePresent)
		return;

	// Clear any eventual events in the Wiimote queue
	WiiMoteReal::ClearEvents();

	// Are we using an extension now? The report that it's removed, then reconnected.
	bool UsingExtension = false;
	if ((WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected != WiiMoteEmu::EXT_NONE) || ( WiiMoteEmu::WiiMapping[m_Page].bMotionPlusConnected))
		UsingExtension = true;

	DEBUG_LOG(WIIMOTE, "DoUseReal()  Connect extension: %i", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 0 : 1);

	UsingExtension = !UsingExtension;
	DEBUG_LOG(WIIMOTE, "DoUseReal()  Connect extension: %i", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 1 : 0);

	if (g_EmulatorState == PLUGIN_EMUSTATE_PLAY)
	{
		// Disable the checkbox for a moment
		SetCursor(wxCursor(wxCURSOR_WAIT));
		// We may not need this if there is already a message queue that allows the nessesary timeout
		//sleep(100);

		/* Start the timer to allow the approximate time it takes for the Wiimote to come online
		   it would simpler to use sleep(1000) but that doesn't work because we need the functions in main.cpp
		   to work */
		m_TimeoutOnce->Start(1000, true);
	}
}
#endif

// Generate connect/disconnect status event
void WiimoteBasicConfigDialog::DoExtensionConnectedDisconnected(int Extension)
{
	// There is no need for this if no game is running
	if (!g_EmulatorRunning || WiiMoteEmu::WiiMapping[m_Page].Source != 1)
		return;

	u8 DataFrame[8] = {0}; // make a blank report for it
	wm_request_status *rs = (wm_request_status*)DataFrame;

	// Check if a game is running, in that case change the status
	if (WiiMoteEmu::g_ReportingChannel[m_Page] > 0)
	{
		WiiMoteEmu::g_ID = m_Page;
		WiiMoteEmu::WmRequestStatus(WiiMoteEmu::g_ReportingChannel[m_Page], rs, Extension);
	}
}

// Notebook page changed
void WiimoteBasicConfigDialog::NotebookPageChanged(wxNotebookEvent& event)
{	
	// Update the global variable
	m_Page = event.GetSelection();

	// Update GUI
	if (ControlsCreated)
		UpdateGUI();
}

void WiimoteBasicConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDC_INPUT_SOURCE:
			// If the config dialog was open when the user click on Play/Pause, the GUI was not updated, so this could crash everything!
			if (g_EmulatorState == PLUGIN_EMUSTATE_PLAY)
			{
				PanicAlert("This can only be changed when the emulator is paused or stopped!");
				WiiMoteEmu::WiiMapping[m_Page].Source = 0;
			}
			else
			{
				if (m_InputSource[m_Page]->GetSelection() == 2)
				{
					WiiMoteEmu::WiiMapping[m_Page].Source = 2;
					DoUseReal();
				}
				else
					WiiMoteEmu::WiiMapping[m_Page].Source = m_InputSource[m_Page]->GetSelection();
			}
			break;
		case IDC_SIDEWAYSWIIMOTE:
			WiiMoteEmu::WiiMapping[m_Page].bSideways = m_SidewaysWiimote[m_Page]->IsChecked();
			break;
		case IDC_UPRIGHTWIIMOTE:
			WiiMoteEmu::WiiMapping[m_Page].bUpright = m_UprightWiimote[m_Page]->IsChecked();
			break;
		case IDC_MOTIONPLUSCONNECTED:
			WiiMoteEmu::WiiMapping[m_Page].bMotionPlusConnected = m_WiiMotionPlusConnected[m_Page]->IsChecked();
			DoExtensionConnectedDisconnected(WiiMoteEmu::EXT_NONE);
			if (g_EmulatorRunning)
				SLEEP(25);
			WiiMoteEmu::UpdateExtRegisterBlocks(m_Page);
			DoExtensionConnectedDisconnected();
			break;
		case IDC_WIIAUTORECONNECT:
			WiiMoteEmu::WiiMapping[m_Page].bWiiAutoReconnect = m_WiiAutoReconnect[m_Page]->IsChecked();
			break;
		case IDC_WIIAUTOUNPAIR:
			g_Config.bUnpairRealWiimote = m_WiiAutoUnpair[m_Page]->IsChecked();
			break;
#ifdef _WIN32
		case IDC_WIIAUTOPAIR:
			if (m_WiiExtendedPairUp[m_Page]->IsChecked()) 
				WiiMoteReal::g_StartAutopairThread.Set();
			g_Config.bPairRealWiimote = m_WiiExtendedPairUp[m_Page]->IsChecked();
			break;
#endif
		case IDC_EXTCONNECTED:
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected(WiiMoteEmu::EXT_NONE);
			// It doesn't seem to be needed but shouldn't it at least take 25 ms to
			// reconnect an extension after we disconnected another?
			if (g_EmulatorRunning)
				SLEEP(25);
			// Update status
			WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected = m_Extension[m_Page]->GetSelection();
			// Copy the calibration data
			WiiMoteEmu::UpdateExtRegisterBlocks(m_Page);
			// Generate connect/disconnect status event
			DoExtensionConnectedDisconnected();
			break;
	}
	UpdateGUI();
}

void WiimoteBasicConfigDialog::IRCursorChanged(wxScrollEvent& event)
{
	switch (event.GetId())
	{
	case IDS_WIDTH:
		g_Config.iIRWidth = m_SliderWidth[m_Page]->GetValue();
		break;
	case IDS_HEIGHT:
		g_Config.iIRHeight = m_SliderHeight[m_Page]->GetValue();
		break;
	case IDS_LEFT:
		g_Config.iIRLeft = m_SliderLeft[m_Page]->GetValue();
		break;
	case IDS_TOP:
		g_Config.iIRTop = m_SliderTop[m_Page]->GetValue();
		break;
	case IDS_LEVEL:
		g_Config.iIRLevel = m_SliderIrLevel[m_Page]->GetValue();
		if (g_RealWiiMotePresent) {
			for (int i = 0; i < WiiMoteReal::g_NumberOfWiiMotes; i++)
				wiiuse_set_ir_sensitivity(WiiMoteReal::g_WiiMotesFromWiiUse[i], g_Config.iIRLevel);
		}
		break;
	case IDS_TIMEOUT:
		g_Config.bWiiReadTimeout = m_WiimoteTimeout[m_Page]->GetValue();
		if (g_RealWiiMotePresent) {
			wiiuse_set_timeout(WiiMoteReal::g_WiiMotesFromWiiUse, WiiMoteReal::g_NumberOfWiiMotes, g_Config.bWiiReadTimeout);
		}
		break;
	}
	UpdateGUI();
}

void WiimoteBasicConfigDialog::UDPWiiSettingsChanged(wxCommandEvent &event)
{
	switch(event.GetId())
	{
	case IDC_UDPWIIENABLE:
		WiiMoteEmu::WiiMapping[m_Page].UDPWM.enable=m_UDPWiiEnable[m_Page]->GetValue();
		break;
	case IDC_UDPWIIACCEL:
		WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableAccel=m_UDPWiiAccel[m_Page]->GetValue();
		break;
	case IDC_UDPWIIIR:
		WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableIR=m_UDPWiiIR[m_Page]->GetValue();
		break;
	case IDC_UDPWIIBUTT:
		WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableButtons=m_UDPWiiButt[m_Page]->GetValue();
		break;
	case IDC_UDPWIINUN:
		WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableNunchuck=m_UDPWiiNun[m_Page]->GetValue();
		break;
	case IDT_UDPWIIPORT:
		strncpy(WiiMoteEmu::WiiMapping[m_Page].UDPWM.port,(const char*)(m_UDPWiiPort[m_Page]->GetValue().mb_str(wxConvUTF8)),10);
		break;
	}
	UpdateGUI();
}

void WiimoteBasicConfigDialog::UpdateGUI()
{
	/* I have disabled this option during a running game because it's enough to be able to switch
	   between using and not using then. To also use the connect option during a running game would
	   mean that the wiimote must be sent the current reporting mode and the channel ID after it
	   has been initialized. Functions for that are basically already in place so these two options
	   could possibly be simplified to one option. */
#ifdef _WIN32
	m_PairUpRealWiimote[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);
#endif
	m_TextFoundRealWiimote[m_Page]->SetLabel(wxString::Format(wxT("Connected to %i Real Wiimotes"), WiiMoteReal::g_NumberOfWiiMotes));
	m_ConnectRealWiimote[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);

	m_InputSource[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Source);
	m_InputSource[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);

	if (WiiMoteEmu::WiiMapping[m_Page].Source == 2)
	{
		m_Extension[m_Page]->Enable(false);
		m_WiiMotionPlusConnected[m_Page]->Enable(false);
		m_SidewaysWiimote[m_Page]->Enable(false);
		m_UprightWiimote[m_Page]->Enable(false);
		m_WiimoteTimeout[m_Page]->Enable(true);
		m_SliderIrLevel[m_Page]->Enable(true);
	}
	else
	{
		m_Extension[m_Page]->Enable(true);
		m_WiiMotionPlusConnected[m_Page]->Enable(true);
		m_SidewaysWiimote[m_Page]->Enable(true);
		m_UprightWiimote[m_Page]->Enable(true);
		m_WiimoteTimeout[m_Page]->Enable(false);
		m_SliderIrLevel[m_Page]->Enable(false);
	}
	
	//General settings
	m_Extension[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected);
	m_SidewaysWiimote[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bSideways);
	m_UprightWiimote[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bUpright);
	m_WiiMotionPlusConnected[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bMotionPlusConnected);
	
	//Real Wiimote related settings
#ifdef _WIN32
	//Automatic settings
	m_WiiAutoReconnect[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bWiiAutoReconnect);
	m_WiiAutoUnpair[m_Page]->SetValue(g_Config.bUnpairRealWiimote);
	m_WiiExtendedPairUp[m_Page]->SetValue(g_Config.bPairRealWiimote);
#endif
	m_TextWiimoteTimeout[m_Page]->SetLabel(wxString::Format(wxT("Timeout: %i ms"), g_Config.bWiiReadTimeout));
	m_WiimoteTimeout[m_Page]->SetValue(g_Config.bWiiReadTimeout);

	// Update the Wiimote IR pointer calibration
	m_TextScreenWidth[m_Page]->SetLabel(wxString::Format(wxT("Width: %i"), g_Config.iIRWidth));
	m_TextScreenHeight[m_Page]->SetLabel(wxString::Format(wxT("Height: %i"), g_Config.iIRHeight));
	m_TextScreenLeft[m_Page]->SetLabel(wxString::Format(wxT("Left: %i"), g_Config.iIRLeft));
	m_TextScreenTop[m_Page]->SetLabel(wxString::Format(wxT("Top: %i"), g_Config.iIRTop));
	m_TextScreenIrLevel[m_Page]->SetLabel(wxString::Format(wxT("Sensitivity: %i"), g_Config.iIRLevel));

	// Update the slider position if a configuration has been loaded
	m_SliderWidth[m_Page]->SetValue(g_Config.iIRWidth);
	m_SliderHeight[m_Page]->SetValue(g_Config.iIRHeight);
	m_SliderLeft[m_Page]->SetValue(g_Config.iIRLeft);
	m_SliderTop[m_Page]->SetValue(g_Config.iIRTop);
	m_SliderIrLevel[m_Page]->SetValue(g_Config.iIRLevel);

	m_CheckAR43[m_Page]->SetValue(g_Config.bKeepAR43);
	m_CheckAR169[m_Page]->SetValue(g_Config.bKeepAR169);
	m_Crop[m_Page]->SetValue(g_Config.bCrop);

	//Update UDPWii
	m_UDPWiiEnable[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);
	m_UDPWiiPort[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);
	m_UDPWiiEnable[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].UDPWM.enable);
	m_UDPWiiAccel[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableAccel);
	m_UDPWiiButt[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableButtons);
	m_UDPWiiIR[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableIR);
	m_UDPWiiNun[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].UDPWM.enableNunchuck);
	m_UDPWiiPort[m_Page]->ChangeValue(wxString::FromAscii(WiiMoteEmu::WiiMapping[m_Page].UDPWM.port));

}
