// Copyright (C) 2003-2009 Dolphin Project.

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
	EVT_BUTTON(ID_CLOSE, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_APPLY, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONMAPPING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONRECORDING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_CHECKBOX(ID_CONNECT_REAL, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USE_REAL, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_SIDEWAYSDPAD, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	//EVT_CHECKBOX(ID_MOTIONPLUSCONNECTED, WiimoteConfigDialog::GeneralSettingsChanged)	
	EVT_CHOICE(ID_EXTCONNECTED, WiimoteBasicConfigDialog::GeneralSettingsChanged)
	// IR cursor
	EVT_COMMAND_SCROLL(IDS_WIDTH, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_HEIGHT, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_LEFT, WiimoteBasicConfigDialog::IRCursorChanged)
	EVT_COMMAND_SCROLL(IDS_TOP, WiimoteBasicConfigDialog::IRCursorChanged)

	EVT_TIMER(IDTM_UPDATE_ONCE, WiimoteBasicConfigDialog::UpdateOnce)
	EVT_TIMER(IDTM_SHUTDOWN, WiimoteBasicConfigDialog::ShutDown)
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
	m_bEnableUseRealWiimote = true;
	Page = 0;
	g_Config.Load();
	CreateGUIControls();
	UpdateGUI();
}

void WiimoteBasicConfigDialog::OnClose(wxCloseEvent& event)
{
	g_FrameOpen = false;
	g_Config.Save();
	if (m_PadConfigFrame)
	{
		m_PadConfigFrame->EndModal(wxID_CLOSE);
		m_PadConfigFrame = NULL;
	}
	if (m_RecordingConfigFrame)
	{
		m_RecordingConfigFrame->EndModal(wxID_CLOSE);
		m_RecordingConfigFrame = NULL;
	}
	if (!g_EmulatorRunning) Shutdown();
	// This will let the Close() function close and remove the wxDialog
	event.Skip();
}

/* Timeout the shutdown. In Windows at least the g_pReadThread execution will hang at any attempt to
   call a frame function after the main thread has entered WaitForSingleObject() or any other loop.
   We must therefore shut down the thread from here and wait for that before we can call ShutDown(). */
void WiimoteBasicConfigDialog::ShutDown(wxTimerEvent& WXUNUSED(event))
{
	// Close() is a wxWidgets function that will trigger EVT_CLOSE() and then call this->Destroy().
	if(!WiiMoteReal::g_ThreadGoing)
	{
		m_ShutDownTimer->Stop();
		Close();
	}
}

void WiimoteBasicConfigDialog::ButtonClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_CLOSE:
		// Wait for the Wiimote thread to stop, then close and shutdown
		if(!g_EmulatorRunning)
		{
			WiiMoteReal::g_Shutdown = true;
			m_ShutDownTimer->Start(10);
		}
		// Close directly
		else
		{
			Close();
		}
		break;
	case ID_APPLY:
		g_Config.Save();
		break;
	case ID_BUTTONMAPPING:
		if (m_PadConfigFrame)
			m_PadConfigFrame->EndModal(wxID_CLOSE);
		m_PadConfigFrame = new WiimotePadConfigDialog(this);
		if (!m_PadConfigFrame->IsShown())
			m_PadConfigFrame->ShowModal();
		break;
	case ID_BUTTONRECORDING:
		if (!m_RecordingConfigFrame)
			m_RecordingConfigFrame = new WiimoteRecordingConfigDialog(this);

		if (!m_RecordingConfigFrame->IsShown())
			m_RecordingConfigFrame->ShowModal();
		break;
	}
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

void WiimoteBasicConfigDialog::CreateGUIControls()
{
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		m_Controller[i] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1 + i, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Controller[i], wxString::Format(wxT("Wiimote %d"), i+1));

		m_ConnectRealWiimote[i] = new wxCheckBox(m_Controller[i], ID_CONNECT_REAL, wxT("Connect Real Wiimote"));
		m_UseRealWiimote[i] = new wxCheckBox(m_Controller[i], ID_USE_REAL, wxT("Use Real Wiimote"));

		m_ConnectRealWiimote[0]->SetValue(g_Config.bConnectRealWiimote);
		m_UseRealWiimote[0]->SetValue(g_Config.bUseRealWiimote);

		m_ConnectRealWiimote[i]->SetToolTip(wxT("Connected to the real wiimote. This can not be changed during gameplay."));
		m_UseRealWiimote[i]->SetToolTip(wxT("Use the real Wiimote in the game. This can be changed during gameplay. This can not be selected"
			" when a recording is to be done. No status in this window will be updated when this is checked."));

		m_SizeReal[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Real Wiimote"));
		m_SizeReal[i]->Add(m_ConnectRealWiimote[i], 0, wxEXPAND | wxALL, 5);
		m_SizeReal[i]->Add(m_UseRealWiimote[i], 0, wxEXPAND | wxALL, 5);

		m_WiiMotionPlusConnected[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Wii Motion Plus Connected"));
		m_WiiMotionPlusConnected[0]->Enable(false);

		wxArrayString arrayStringFor_extension;
		arrayStringFor_extension.Add(wxT("None"));
		arrayStringFor_extension.Add(wxT("Nunchuck"));
		arrayStringFor_extension.Add(wxT("Classic Controller"));
		arrayStringFor_extension.Add(wxT("Guitar Hero 3 Guitar"));
		//arrayStringFor_extension.Add(wxT("Guitar Hero World Tour Drums Connected"));
		// Prolly needs to be a separate plugin
		//arrayStringFor_extension.Add(wxT("Balance Board"));

		extensionChoice[i] = new wxChoice(m_Controller[i], ID_EXTCONNECTED, wxDefaultPosition, wxDefaultSize, arrayStringFor_extension, 0, wxDefaultValidator);
		extensionChoice[i]->SetSelection(0);

		m_SizeExtensions[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Extension"));
		m_SizeExtensions[i]->Add(m_WiiMotionPlusConnected[i], 0, wxEXPAND | wxALL, 5);
		m_SizeExtensions[i]->Add(extensionChoice[i], 0, wxEXPAND | wxALL, 5);

		m_SizeBasicGeneralLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeReal[i], 0, wxEXPAND | wxALL, 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeExtensions[i], 0, wxEXPAND | wxALL, 5);

		// Basic Settings
		m_WiimoteOnline[i] = new wxCheckBox(m_Controller[i], IDC_WIMOTE_ON, wxT("Wiimote On"));
		m_WiimoteOnline[i]->SetValue(true);
		m_WiimoteOnline[i]->Enable(false);
		m_WiimoteOnline[i]->SetToolTip(wxString::Format(wxT("Decide if Wiimote %i shall be detected by the game"), i));

		m_SizeBasic[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("General Settings"));
		m_SizeBasic[i]->Add(m_WiimoteOnline[i], 0, wxEXPAND | wxALL, 5);

		// Emulated Wiimote
		m_SidewaysDPad[i] = new wxCheckBox(m_Controller[i], ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"));
		m_SidewaysDPad[i]->SetValue(g_Config.bSidewaysDPad);

		m_SizeEmu[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Wiimote"));
		m_SizeEmu[i]->Add(m_SidewaysDPad[i], 0, wxEXPAND | wxALL, 5);

		//IR Pointer
		m_TextScreenWidth[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Width: 000"));
		m_TextScreenHeight[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Height: 000"));
		m_TextScreenLeft[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left: 000"));
		m_TextScreenTop[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Top: 000"));

		m_SliderWidth[i] = new wxSlider(m_Controller[i], IDS_WIDTH, 0, 100, 923, wxDefaultPosition, wxSize(75, -1));
		m_SliderHeight[i] = new wxSlider(m_Controller[i], IDS_HEIGHT, 0, 0, 727, wxDefaultPosition, wxSize(75, -1));
		m_SliderLeft[i] = new wxSlider(m_Controller[i], IDS_LEFT, 0, 100, 500, wxDefaultPosition, wxSize(75, -1));
		m_SliderTop[i] = new wxSlider(m_Controller[i], IDS_TOP, 0, 0, 500, wxDefaultPosition, wxSize(75, -1));

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

		//m_ScreenSize = new wxCheckBox(m_Controller[i], IDC_SCREEN_SIZE, wxT("Adjust screen size and position"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		//m_ScreenSize[i]->SetToolTip(wxT("Use the adjusted screen size."));

		// These are changed from the graphics plugin settings, so they are just here to show the loaded status
		m_TextAR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Aspect Ratio"));
		m_CheckAR43[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("4:3"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_CheckAR169[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("16:9"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_Crop[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Crop"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);

		m_CheckAR43[i]->SetValue(g_Config.bKeepAR43);
		m_CheckAR169[i]->SetValue(g_Config.bKeepAR169);
		m_Crop[i]->SetValue(g_Config.bCrop);

		m_TextAR[i]->Enable(false);
		m_CheckAR43[i]->Enable(false);
		m_CheckAR169[i]->Enable(false);
		m_Crop[i]->Enable(false);

		m_SizerIRPointerScreen[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerScreen[i]->Add(m_TextAR[i], 0, wxEXPAND | (wxTOP), 0);
		m_SizerIRPointerScreen[i]->Add(m_CheckAR43[i], 0, wxEXPAND | (wxLEFT), 5);
		m_SizerIRPointerScreen[i]->Add(m_CheckAR169[i], 0, wxEXPAND | (wxLEFT), 5);
		m_SizerIRPointerScreen[i]->Add(m_Crop[i], 0, wxEXPAND | (wxLEFT), 5);

		m_SizerIRPointer[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("IR pointer"));
		//m_SizerIRPointer[i]->Add(m_ScreenSize[i], 0, wxEXPAND | (wxALL), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerWidth[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerHeight[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerScreen[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);

		m_SizeBasicGeneralRight[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeBasicGeneralRight[i]->Add(m_SizeBasic[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneralRight[i]->Add(m_SizeEmu[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralRight[i]->Add(m_SizerIRPointer[i], 0, wxEXPAND | (wxUP), 5);

		m_SizeBasicGeneral[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralLeft[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralRight[i], 0, wxEXPAND | (wxLEFT), 5);

		m_SizeParent[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeParent[i]->Add(m_SizeBasicGeneral[i], 0, wxBORDER_STATIC | wxEXPAND | (wxALL), 5);
		// The sizer m_sMain will be expanded inside m_Controller, m_SizeParent will not
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_SizeParent[i]);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_sMain[i]);
	}

	m_ButtonMapping = new wxButton(this, ID_BUTTONMAPPING, wxT("Button Mapping"));
	m_Recording		= new wxButton(this, ID_BUTTONRECORDING, wxT("Recording"));
	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"));
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));
	m_Close->SetToolTip(wxT("Apply and Close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_ButtonMapping, 0, (wxALL), 0);
	sButtons->Add(m_Recording, 0, (wxALL), 0);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Apply, 0, (wxALL), 0);
	sButtons->Add(m_Close, 0, (wxLEFT), 5);	

	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	this->SetSizer(m_MainSizer);
	this->Layout();
	Fit();

	// Center the window if there is room for it
	#ifdef _WIN32
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 800)
			Center();
	#endif
	ControlsCreated = true;
}

void WiimoteBasicConfigDialog::DoConnectReal()
{
	g_Config.bConnectRealWiimote = m_ConnectRealWiimote[Page]->IsChecked();

	if(g_Config.bConnectRealWiimote)
	{
		if (!g_RealWiiMoteInitialized) WiiMoteReal::Initialize();
	}
	else
	{
		INFO_LOG(CONSOLE, "Post Message: %i\n", g_RealWiiMoteInitialized);
		if (g_RealWiiMoteInitialized)
		{
			WiiMoteReal::Shutdown();
		}
	}
}

void WiimoteBasicConfigDialog::DoUseReal()
{
	// Clear any eventual events in the Wiimote queue
	WiiMoteReal::ClearEvents();

	// Are we using an extension now? The report that it's removed, then reconnected.
	bool UsingExtension = false;
	if (g_Config.iExtensionConnected != EXT_NONE)
		UsingExtension = true;

	INFO_LOG(CONSOLE, "\nDoUseReal()  Connect extension: %i\n", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 0 : 1);

	UsingExtension = !UsingExtension;
	INFO_LOG(CONSOLE, "\nDoUseReal()  Connect extension: %i\n", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 1 : 0);

	if(g_EmulatorRunning)
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
	if(!g_EmulatorRunning) return; 

	u8 DataFrame[8]; // make a blank report for it
	wm_request_status *rs = (wm_request_status*)DataFrame;

	// Check if a game is running, in that case change the status
	if(WiiMoteEmu::g_ReportingChannel > 0)
		WiiMoteEmu::WmRequestStatus(WiiMoteEmu::g_ReportingChannel, rs, Extension);
}


void WiimoteBasicConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_CONNECT_REAL:
		DoConnectReal();
		break;
	case ID_USE_REAL:
		// Enable the Wiimote thread
		g_Config.bUseRealWiimote = m_UseRealWiimote[Page]->IsChecked();
		if(g_Config.bUseRealWiimote) DoUseReal();		
		break;

	case ID_SIDEWAYSDPAD:
		g_Config.bSidewaysDPad = m_SidewaysDPad[Page]->IsChecked();
		break;

	case ID_MOTIONPLUSCONNECTED:
		break;
	case ID_EXTCONNECTED:
		g_Config.iExtensionConnected = EXT_NONE;
		// Disconnect the extension so that the game recognize the change
		DoExtensionConnectedDisconnected();
		// It doesn't seem to be needed but shouldn't it at least take 25 ms to
		// reconnect an extension after we disconnected another?
		if(g_EmulatorRunning) SLEEP(25);

		// Update status
		g_Config.iExtensionConnected = extensionChoice[Page]->GetSelection();

		// Copy the calibration data
		WiiMoteEmu::SetDefaultExtensionRegistry();

		// Generate connect/disconnect status event
		DoExtensionConnectedDisconnected();
		break;
	}
	g_Config.Save();
	UpdateGUI();
}

void WiimoteBasicConfigDialog::IRCursorChanged(wxScrollEvent& event)
{
	switch (event.GetId())
	{
	case IDS_WIDTH:
		g_Config.iIRWidth = m_SliderWidth[Page]->GetValue();
		break;
	case IDS_HEIGHT:
		g_Config.iIRHeight = m_SliderHeight[Page]->GetValue();
		break;
	case IDS_LEFT:
		g_Config.iIRLeft = m_SliderLeft[Page]->GetValue();
		break;
	case IDS_TOP:
		g_Config.iIRTop = m_SliderTop[Page]->GetValue();
		break;
	}

	UpdateGUI();
}

void WiimoteBasicConfigDialog::UpdateIRCalibration()
{
	// Update the slider position if a configuration has been loaded
	m_SliderWidth[Page]->SetValue(g_Config.iIRWidth);
	m_SliderHeight[Page]->SetValue(g_Config.iIRHeight);
	m_SliderLeft[Page]->SetValue(g_Config.iIRLeft);
	m_SliderTop[Page]->SetValue(g_Config.iIRTop);

	// Update the labels
	m_TextScreenWidth[Page]->SetLabel(wxString::Format(wxT("Width: %i"), g_Config.iIRWidth));
	m_TextScreenHeight[Page]->SetLabel(wxString::Format(wxT("Height: %i"), g_Config.iIRHeight));
	m_TextScreenLeft[Page]->SetLabel(wxString::Format(wxT("Left: %i"), g_Config.iIRLeft));
	m_TextScreenTop[Page]->SetLabel(wxString::Format(wxT("Top: %i"), g_Config.iIRTop));
}

void WiimoteBasicConfigDialog::UpdateGUI(int Slot)
{
	// Update the Wiimote IR pointer calibration
	UpdateIRCalibration();

	/* We only allow a change of extension if we are not currently using the real Wiimote, if it's in use the status will be updated
	   from the data scanning functions in main.cpp */
	bool AllowExtensionChange = !(g_RealWiiMotePresent && g_Config.bConnectRealWiimote && g_Config.bUseRealWiimote && g_EmulatorRunning);
	extensionChoice[Page]->SetSelection(g_Config.iExtensionConnected);
	extensionChoice[Page]->Enable(AllowExtensionChange);

	/* I have disabled this option during a running game because it's enough to be able to switch
	   between using and not using then. To also use the connect option during a running game would
	   mean that the wiimote must be sent the current reporting mode and the channel ID after it
	   has been initialized. Functions for that are basically already in place so these two options
	   could possibly be simplified to one option. */
	m_ConnectRealWiimote[Page]->Enable(!g_EmulatorRunning);
	m_UseRealWiimote[Page]->Enable((m_bEnableUseRealWiimote && g_RealWiiMotePresent && g_Config.bConnectRealWiimote) || (!g_EmulatorRunning && g_Config.bConnectRealWiimote));
}
