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
	EVT_BUTTON(ID_BUTTONMAPPING, WiimoteBasicConfigDialog::ButtonClick)
	EVT_BUTTON(ID_BUTTONRECORDING, WiimoteBasicConfigDialog::ButtonClick)
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
	m_Page = 0;

	m_bEnableUseRealWiimote = true;

	CreateGUIControls();
	UpdateGUI();
}

void WiimoteBasicConfigDialog::OnClose(wxCloseEvent& event)
{
	// necessary as this dialog is only showed/hided
	UpdateGUI();
	g_FrameOpen = false;
	EndModal(wxID_CLOSE);

	// Shutdown Real WiiMotes so everything is set properly before the game starts
	if (g_RealWiiMoteInitialized)	
		WiiMoteReal::Shutdown();
}

/* Timeout the shutdown. In Windows at least the g_pReadThread execution will hang at any attempt to
   call a frame function after the main thread has entered WaitForSingleObject() or any other loop.
   We must therefore shut down the thread from here and wait for that before we can call ShutDown(). */
void WiimoteBasicConfigDialog::ShutDown(wxTimerEvent& WXUNUSED(event))
{
	if (!WiiMoteReal::g_ThreadGoing)
	{
		m_ShutDownTimer->Stop();
		Close();
	}
}

void WiimoteBasicConfigDialog::ButtonClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_OK:
		g_Config.Save();
/*
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
*/
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

		// Emulated Wiimote
		m_SidewaysWiimote[i] = new wxCheckBox(m_Controller[i], IDC_SIDEWAYSWIIMOTE, wxT("Sideways Wiimote"));
		m_SidewaysWiimote[i]->SetToolTip(wxT("Treat the sideways position as neutral"));
		m_UprightWiimote[i] = new wxCheckBox(m_Controller[i], IDC_UPRIGHTWIIMOTE, wxT("Upright Wiimote"));
		m_UprightWiimote[i]->SetToolTip(wxT("Treat the upright position as neutral"));

		m_WiiMotionPlusConnected[i] = new wxCheckBox(m_Controller[i], IDC_MOTIONPLUSCONNECTED, wxT("Wii Motion Plus Connected"));
		m_WiiMotionPlusConnected[i]->Enable(false);

		m_Extension[i] = new wxChoice(m_Controller[i], IDC_EXTCONNECTED, wxDefaultPosition, wxDefaultSize, arrayStringFor_extension, 0, wxDefaultValidator);

		m_ConnectRealWiimote[i] = new wxCheckBox(m_Controller[i], IDC_CONNECT_REAL, wxT("Connect Real Wiimote"));
		m_ConnectRealWiimote[i]->SetToolTip(wxT("Connected to the real wiimote. This can not be changed during gameplay."));

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
	m_OK = new wxButton(this, ID_OK, wxT("OK"));
	m_OK->SetToolTip(wxT("Save changes and close"));
	m_Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"));
	m_Cancel->SetToolTip(wxT("Discard changes and close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_ButtonMapping, 0, (wxALL), 0);
	sButtons->Add(m_Recording, 0, (wxALL), 0);
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

	// Initialize the Real WiiMotes here, so we get a count of how many were found and set everything properly
	if (!g_RealWiiMoteInitialized && g_Config.bConnectRealWiimote)
		WiiMoteReal::Initialize();
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
	if(g_Config.bConnectRealWiimote)
	{
		if (!g_RealWiiMoteInitialized)
			WiiMoteReal::Initialize();
	}
	else
	{
		DEBUG_LOG(WIIMOTE, "Post Message: %i", g_RealWiiMoteInitialized);

		if (g_RealWiiMoteInitialized)
			WiiMoteReal::Shutdown();
	}
}

void WiimoteBasicConfigDialog::DoUseReal()
{
	if (!g_RealWiiMotePresent || !g_Config.bConnectRealWiimote)
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

	if (g_EmulatorRunning)
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
	if (!g_EmulatorRunning || WiiMoteEmu::WiiMapping[m_Page].Source <= 0)
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
			g_Config.bConnectRealWiimote = m_ConnectRealWiimote[m_Page]->IsChecked();
			DoConnectReal();
			break;
		case IDC_INPUT_SOURCE:
			if (m_InputSource[m_Page]->GetSelection() == 2)
			{
				int current_real = 0;
				for (int i = 0; i < MAX_WIIMOTES; i++)
					if (WiiMoteEmu::WiiMapping[i].Source < 0)
						current_real++;
				if (current_real >= WiiMoteReal::g_NumberOfWiiMotes)
				{
					PanicAlert("You've already assigned all your %i Real WiiMote(s) connected!", WiiMoteReal::g_NumberOfWiiMotes);
					m_InputSource[m_Page]->SetSelection(1);
				}
				else
				{
					WiiMoteEmu::WiiMapping[m_Page].Source = -1;
					DoUseReal();
				}
			}
			else
				WiiMoteEmu::WiiMapping[m_Page].Source = m_InputSource[m_Page]->GetSelection();
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
			if(g_EmulatorRunning)
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
	m_ConnectRealWiimote[m_Page]->SetValue(g_Config.bConnectRealWiimote);
	m_ConnectRealWiimote[m_Page]->Enable(!g_EmulatorRunning);
	m_InputSource[m_Page]->Enable(!g_EmulatorRunning);

	if (WiiMoteEmu::WiiMapping[m_Page].Source < 0)
		m_InputSource[m_Page]->SetSelection(2);
	else
		m_InputSource[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Source);
	
	if (m_InputSource[m_Page]->GetSelection() == 2)
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

