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
#include "ConfigRecordingDlg.h"
#include "Config.h"
#include "EmuMain.h" // for SetDefaultExtensionRegistry
#include "EmuSubroutines.h" // for WmRequestStatus



BEGIN_EVENT_TABLE(WiimoteBasicConfigDialog,wxDialog)
	EVT_CLOSE(WiimoteBasicConfigDialog::OnClose)
	EVT_BUTTON(ID_OK, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_CANCEL, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_REFRESH_REAL, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONMAPPING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONRECORDING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONPAIRUP, WiimoteBasicConfigDialog::ButtonClick)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, WiimoteBasicConfigDialog::NotebookPageChanged)
	EVT_CHOICE(IDC_INPUT_SOURCE, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_CONNECT_REAL, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_SIDEWAYSWIIMOTE, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_UPRIGHTWIIMOTE, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_MOTIONPLUSCONNECTED, WiimoteBasicConfigDialog::GeneralSettingsChanged)	
	EVT_CHOICE(IDC_EXTCONNECTED, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	// IR cursor
	EVT_COMMAND_SCROLL(IDS_WIDTH, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_HEIGHT, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_LEFT, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_TOP, WiimoteBasicConfigDialog::IRCursorChanged)

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

	m_bEnableUseRealWiimote = true;
	// Initialize the Real WiiMotes here, so we get a count of how many were found and set everything properly
	if (!g_RealWiiMoteInitialized)
		WiiMoteReal::Initialize();

	CreateGUIControls();
	UpdateGUI();
}

void WiimoteBasicConfigDialog::OnClose(wxCloseEvent& event)
{
	// necessary as this dialog is only showed/hided
	UpdateGUI();
	g_FrameOpen = false;
	EndModal(wxID_CLOSE);
}

void WiimoteBasicConfigDialog::ButtonClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_OK:
		WiiMoteReal::Allocate();
		g_Config.Save();
		Close();
		break;
	case ID_CANCEL:
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
	case ID_BUTTONRECORDING:
		m_RecordingConfigFrame = new WiimoteRecordingConfigDialog(this);
		m_RecordingConfigFrame->ShowModal();
		m_RecordingConfigFrame->Destroy();
		m_RecordingConfigFrame = NULL;
		m_Page = g_Config.CurrentPage;
		break;
#ifdef WIN32
	case ID_BUTTONPAIRUP:
		if (g_EmulatorState != PLUGIN_EMUSTATE_PLAY) {
			m_ButtonPairUp->Enable(false);
			if (WiiMoteReal::WiimotePairUp() > 0) { //Only temporay solution TODO: 2nd step: threaded. 
				// sleep would be required (but not best way to solve that cuz 3000ms~ would be needed, which is not convenient),cuz BT device is not ready yet when calling DoRefreshReal() 

				DoRefreshReal();
			}
			m_ButtonPairUp->Enable(true);
		}
		break;
#endif
	case ID_REFRESH_REAL:
		DoRefreshReal();
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
		m_WiiMotionPlusConnected[i]->Enable(false);

		m_Extension[i] = new wxChoice(m_Controller[i], IDC_EXTCONNECTED, wxDefaultPosition, wxDefaultSize, arrayStringFor_extension, 0, wxDefaultValidator);

		m_ConnectRealWiimote[i] = new wxCheckBox(m_Controller[i], IDC_CONNECT_REAL, wxT("Connect Real Wiimote"));
		m_ConnectRealWiimote[i]->SetToolTip(wxT("Connected to the Real WiiMote(s).\nThis can only be changed when the emulator is paused or stopped."));
		m_FoundWiimote[i] = new wxStaticText(m_Controller[i], ID_FOUND_REAL, wxT("Found 0 WiiMotes"));
		m_RefreshRealWiiMote[i] = new wxButton(m_Controller[i], ID_REFRESH_REAL, wxT("Refresh Real WiiMotes"));
		m_RefreshRealWiiMote[i]->SetToolTip(wxT("Reconnect to all Real WiiMotes.\nThis is useful if you recently connected another one.\nThis can only be done when the emulator is paused or stopped."));

		//IR Pointer
		m_TextScreenWidth[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Width: 000"));
		m_TextScreenHeight[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Height: 000"));
		m_TextScreenLeft[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left: 000"));
		m_TextScreenTop[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Top: 000"));

		m_SliderWidth[i] = new wxSlider(m_Controller[i], IDS_WIDTH, 0, 100, 923, wxDefaultPosition, wxSize(75, -1));
		m_SliderHeight[i] = new wxSlider(m_Controller[i], IDS_HEIGHT, 0, 0, 727, wxDefaultPosition, wxSize(75, -1));
		m_SliderLeft[i] = new wxSlider(m_Controller[i], IDS_LEFT, 0, 100, 500, wxDefaultPosition, wxSize(75, -1));
		m_SliderTop[i] = new wxSlider(m_Controller[i], IDS_TOP, 0, 0, 500, wxDefaultPosition, wxSize(75, -1));

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

		m_SizeReal[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Real Wiimote"));
		m_SizeReal[i]->Add(m_ConnectRealWiimote[i], 0, wxEXPAND | wxALL, 5);
		m_SizeReal[i]->Add(m_FoundWiimote[i], 0, wxEXPAND | wxALL, 5);
		m_SizeReal[i]->Add(m_RefreshRealWiiMote[i], 0, wxEXPAND | wxALL, 5);

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

		m_SizerIRPointer[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("IR Pointer"));
		m_SizerIRPointer[i]->Add(m_SizerIRPointerWidth[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerHeight[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerScreen[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 10);

		m_SizeBasicGeneralLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeBasic[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeEmu[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeExtensions[i], 0, wxEXPAND | (wxUP), 5);

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
	m_Recording		= new wxButton(this, ID_BUTTONRECORDING, wxT("Recording"));
	m_ButtonPairUp	= new wxButton(this, ID_BUTTONPAIRUP, wxT("Pair Up Wiimote(s)"));
	
	#ifdef WIN32
	m_ButtonPairUp->SetToolTip(wxT("Pair up your Wiimote(s) with your system.\nPress the Buttons 1 and 2 on your Wiimote before pairing up.\nThis might take a few seconds.\nIt only works if you're using Microsoft's Bluetooth stack.")); // Only working with MS BT Stack.
	#else
	m_ButtonPairUp->Enable(false);
    #endif

	m_OK = new wxButton(this, ID_OK, wxT("OK"));
	m_OK->SetToolTip(wxT("Save changes and close"));
	m_Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"));
	m_Cancel->SetToolTip(wxT("Discard changes and close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_ButtonMapping, 0, (wxALL), 0);
	sButtons->Add(m_Recording, 0, (wxALL), 0);
	sButtons->Add(m_ButtonPairUp, 0, (wxALL), 0);
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
		m_bEnableUseRealWiimote = true;
		SetCursor(wxCursor(wxCURSOR_ARROW));
		UpdateGUI();
		break;
	}
}

void WiimoteBasicConfigDialog::DoConnectReal()
{
	if (g_Config.bConnectRealWiimote)
	{
		if (!g_RealWiiMoteInitialized)
		{
			WiiMoteReal::Initialize();
			UpdateGUI();
		}
	}
	else
	{
		DEBUG_LOG(WIIMOTE, "Post Message: %i", g_RealWiiMoteInitialized);

		if (g_RealWiiMoteInitialized)
			WiiMoteReal::Shutdown();
	}
}

void WiimoteBasicConfigDialog::DoRefreshReal()
{
	if (g_Config.bConnectRealWiimote && g_EmulatorState != PLUGIN_EMUSTATE_PLAY)
	{
		if (g_RealWiiMoteInitialized)
			WiiMoteReal::Shutdown();
		if (!g_RealWiiMoteInitialized)
			WiiMoteReal::Initialize();
	}
	UpdateGUI();
}

void WiimoteBasicConfigDialog::DoUseReal()
{
	if (!g_RealWiiMotePresent)
		return;

	// Clear any eventual events in the Wiimote queue
	WiiMoteReal::ClearEvents();

	// Are we using an extension now? The report that it's removed, then reconnected.
	bool UsingExtension = false;
	if (WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected != WiiMoteEmu::EXT_NONE)
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
		m_bEnableUseRealWiimote = false;
		// We may not need this if there is already a message queue that allows the nessesary timeout
		//sleep(100);

		/* Start the timer to allow the approximate time it takes for the Wiimote to come online
		   it would simpler to use sleep(1000) but that doesn't work because we need the functions in main.cpp
		   to work */
		m_TimeoutOnce->Start(1000, true);
	}
}

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
		case IDC_CONNECT_REAL:
			// If the config dialog was open when the user click on Play/Pause, the GUI was not updated, so this could crash everything!
			if (g_EmulatorState != PLUGIN_EMUSTATE_PLAY)
			{
				g_Config.bConnectRealWiimote = m_ConnectRealWiimote[m_Page]->IsChecked();
				DoConnectReal();
			}
			break;
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
			break;
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
	m_InputSource[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);
	m_ConnectRealWiimote[m_Page]->SetValue(g_Config.bConnectRealWiimote);
	m_ConnectRealWiimote[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);
	m_RefreshRealWiiMote[m_Page]->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY && g_Config.bConnectRealWiimote);
	m_ButtonPairUp->Enable(g_EmulatorState != PLUGIN_EMUSTATE_PLAY);
	wxString Found;
	if (g_Config.bConnectRealWiimote)
		Found.Printf(wxT("Connected to %i Real WiiMote(s)"), WiiMoteReal::g_NumberOfWiiMotes);
	else
		Found.Printf(wxT("Not connected to Real WiiMotes"));
	m_FoundWiimote[m_Page]->SetLabel(Found);

	m_InputSource[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Source);
	if (WiiMoteEmu::WiiMapping[m_Page].Source == 2)
	{
		m_SidewaysWiimote[m_Page]->Enable(false);
		m_UprightWiimote[m_Page]->Enable(false);
		m_Extension[m_Page]->Enable(false);
	}
	else
	{
		m_SidewaysWiimote[m_Page]->Enable(true);
		m_UprightWiimote[m_Page]->Enable(true);
		m_Extension[m_Page]->Enable(true);
	}

	m_SidewaysWiimote[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bSideways);
	m_UprightWiimote[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bUpright);
	m_WiiMotionPlusConnected[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bMotionPlusConnected);
	m_Extension[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected);

	// Update the Wiimote IR pointer calibration
	m_TextScreenWidth[m_Page]->SetLabel(wxString::Format(wxT("Width: %i"), g_Config.iIRWidth));
	m_TextScreenHeight[m_Page]->SetLabel(wxString::Format(wxT("Height: %i"), g_Config.iIRHeight));
	m_TextScreenLeft[m_Page]->SetLabel(wxString::Format(wxT("Left: %i"), g_Config.iIRLeft));
	m_TextScreenTop[m_Page]->SetLabel(wxString::Format(wxT("Top: %i"), g_Config.iIRTop));
	// Update the slider position if a configuration has been loaded
	m_SliderWidth[m_Page]->SetValue(g_Config.iIRWidth);
	m_SliderHeight[m_Page]->SetValue(g_Config.iIRHeight);
	m_SliderLeft[m_Page]->SetValue(g_Config.iIRLeft);
	m_SliderTop[m_Page]->SetValue(g_Config.iIRTop);

	m_CheckAR43[m_Page]->SetValue(g_Config.bKeepAR43);
	m_CheckAR169[m_Page]->SetValue(g_Config.bKeepAR169);
	m_Crop[m_Page]->SetValue(g_Config.bCrop);
}
