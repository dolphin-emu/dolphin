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


#include <wx/wx.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include <wx/checklst.h>

#include "Core.h" // for Core::GetState()
#include "LogWindow.h"
#include "Console.h"

// Milliseconds between msgQueue flushes to wxTextCtrl
#define UPDATETIME 200

BEGIN_EVENT_TABLE(CLogWindow, wxPanel)
	EVT_CLOSE(CLogWindow::OnClose)
	EVT_TEXT_ENTER(IDM_SUBMITCMD, CLogWindow::OnSubmit)
	EVT_BUTTON(IDM_CLEARLOG, CLogWindow::OnClear)
	EVT_BUTTON(IDM_TOGGLEALL, CLogWindow::OnToggleAll)
	EVT_RADIOBOX(IDM_VERBOSITY, CLogWindow::OnOptionsCheck)
	EVT_CHOICE(IDM_FONT, CLogWindow::OnOptionsCheck)
	EVT_CHECKBOX(IDM_WRAPLINE, CLogWindow::OnOptionsCheck)
	EVT_CHECKBOX(IDM_WRITEFILE, CLogWindow::OnOptionsCheck)
	EVT_CHECKBOX(IDM_WRITECONSOLE, CLogWindow::OnOptionsCheck)
	EVT_CHECKBOX(IDM_WRITEWINDOW, CLogWindow::OnOptionsCheck)
	EVT_CHECKLISTBOX(IDM_LOGCHECKS, CLogWindow::OnLogCheck)
	EVT_TIMER(IDTM_UPDATELOG, CLogWindow::OnLogTimer)
END_EVENT_TABLE()

CLogWindow::CLogWindow(CFrame *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
	: wxPanel(parent, id, position, size, style)
    , Parent(parent), m_LogSection(1), m_Log(NULL), m_cmdline(NULL), m_FontChoice(NULL)
	, m_LogAccess(true)
{
	m_LogManager = LogManager::GetInstance();
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		m_LogManager->addListener((LogTypes::LOG_TYPE)i, this);
	m_fileLog = m_LogManager->getFileListener();
	m_console = m_LogManager->getConsoleListener();

	CreateGUIControls();

	LoadSettings();
}

void CLogWindow::CreateGUIControls()
{
	// Verbosity
	wxArrayString wxLevels, wxLevelsUse;	
	wxLevels.Add(wxT("Notice"));
	wxLevels.Add(wxT("Error"));
	wxLevels.Add(wxT("Warning"));
	wxLevels.Add(wxT("Info"));
	wxLevels.Add(wxT("Debug"));
	for (int i = 0; i < MAX_LOGLEVEL; ++i) wxLevelsUse.Add(wxString::Format(wxT("%s"), wxLevels.Item(i).c_str()));		
	m_verbosity = new wxRadioBox(this, IDM_VERBOSITY, wxT("Verbosity"), wxDefaultPosition, wxDefaultSize, wxLevelsUse, 0, wxRA_SPECIFY_ROWS, wxDefaultValidator);
	// Don't take up so much space
	m_verbosity->SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

	// Font
	m_FontChoice = new wxChoice(this, IDM_FONT, wxDefaultPosition, wxDefaultSize, * new wxArrayString(), 0, wxDefaultValidator);
	m_FontChoice->Append(wxT("Default font"));
	m_FontChoice->Append(wxT("Monospaced font"));
	m_FontChoice->Append(wxT("Selected font"));
	m_FontChoice->SetSelection(0);
	//	wxTextCtrl *Tmp = CreateTextCtrl(this);
	//	DefaultFont = Tmp->GetFont();
	//	Tmp->Destroy();
	MonoSpaceFont.SetNativeFontInfoUserDesc(wxString::FromAscii("lucida console windows-1252"));
	Font.push_back(DefaultFont);
	Font.push_back(MonoSpaceFont);
	Font.push_back(DebuggerFont);

	// Options
	wxCheckBox * m_WrapLine = new wxCheckBox(this, IDM_WRAPLINE, wxT("Word Wrap"));
	m_writeFileCB = new wxCheckBox(this, IDM_WRITEFILE, wxT("Write to File"), wxDefaultPosition, wxDefaultSize, 0);
	m_writeConsoleCB = new wxCheckBox(this, IDM_WRITECONSOLE, wxT("Write to Console"), wxDefaultPosition, wxDefaultSize, 0);
	m_writeWindowCB = new wxCheckBox(this, IDM_WRITEWINDOW, wxT("Write to Window ->"), wxDefaultPosition, wxDefaultSize, 0);

	m_checks = new wxCheckListBox(this, IDM_LOGCHECKS, wxDefaultPosition, wxDefaultSize);

	// Log viewer and submit row
	m_Log = CreateTextCtrl(this, IDM_LOG, wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	m_cmdline = new wxTextCtrl(this, IDM_SUBMITCMD, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
	//m_cmdline->SetFont(DebuggerFont);

	// Sizers
	sUber = new wxBoxSizer(wxHORIZONTAL);			// whole plane
	sLeft = new wxBoxSizer(wxVERTICAL);				// left sizer
	sRight = new wxBoxSizer(wxVERTICAL);			// right sizer
	sRightBottom = new wxBoxSizer(wxHORIZONTAL);	// submit row
	// Left side: buttons (-submit), options, and log type selection
	wxStaticBoxSizer* sbLeftOptions = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Options"));

	wxBoxSizer* sLogCtrl = new wxBoxSizer(wxHORIZONTAL);	
	sLogCtrl->Add(new wxButton(this, IDM_TOGGLEALL, wxT("Toggle all"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 1);
	sLogCtrl->Add(new wxButton(this, IDM_CLEARLOG, wxT("Clear"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 1);

	sbLeftOptions->Add(m_FontChoice, 0, (wxDOWN), 1);
	sbLeftOptions->Add(m_WrapLine, 0, (wxDOWN), 1);	
	sbLeftOptions->Add(m_writeFileCB, 0, (wxDOWN), 1);
	sbLeftOptions->Add(m_writeConsoleCB, 0, (wxDOWN), 1);
	sbLeftOptions->Add(m_writeWindowCB, 0);

	sLeft->Add(m_verbosity, 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
	sLeft->Add(sbLeftOptions, 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
	sLeft->Add(sLogCtrl, 0, wxEXPAND);
	sLeft->Add(m_checks, 1, wxEXPAND);	

	PopulateRight();
	
	sUber->Add(sLeft, 0, wxEXPAND);
	sUber->Add(sRight, 1, wxEXPAND);
	this->SetSizer(sUber);

	// Settings
	//	SetAffirmativeId(IDM_SUBMITCMD);
	UpdateChecks();
	m_cmdline->SetFocus();

	m_LogTimer = new wxTimer(this, IDTM_UPDATELOG);
	m_LogTimer->Start(UPDATETIME);
}

CLogWindow::~CLogWindow()
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_LogManager->removeListener((LogTypes::LOG_TYPE)i, this);
	}
	m_LogTimer->Stop();
	delete m_LogTimer;

	SaveSettings();
}

void CLogWindow::OnClose(wxCloseEvent& event)
{
	wxGetApp().GetCFrame()->ToggleLogWindow(false);
	event.Skip();
}

void CLogWindow::SaveSettings()
{
	IniFile ini;
	ini.Set("LogWindow", "x", Parent->m_Mgr->GetPane(wxT("Pane 1")).rect.GetWidth());
	ini.Set("LogWindow", "y", Parent->m_Mgr->GetPane(wxT("Pane 1")).rect.GetHeight());
	ini.Set("LogWindow", "pos", Parent->m_Mgr->GetPane(wxT("Pane 1")).dock_direction);
	ini.Set("Options", "Verbosity", m_verbosity->GetSelection() + 1);
	ini.Set("Options", "Font", m_FontChoice->GetSelection());
	ini.Set("Options", "WriteToFile", m_writeFile);
	ini.Set("Options", "WriteToConsole", m_writeConsole);
	ini.Set("Options", "WriteToWindow", m_writeWindow);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		ini.Set("Logs", m_LogManager->getShortName((LogTypes::LOG_TYPE)i), m_checks->IsChecked(i));
	ini.Save(LOGGER_CONFIG_FILE);
}

void CLogWindow::LoadSettings()
{
	IniFile ini;
	ini.Load(LOGGER_CONFIG_FILE);
	int verbosity,font;
	ini.Get("Options", "Verbosity", &verbosity, 0);
	if (verbosity < 1) verbosity = 1;
	if (verbosity > MAX_LOGLEVEL) verbosity = MAX_LOGLEVEL;
	m_verbosity->SetSelection(verbosity - 1);
	ini.Get("Options", "Font", &font, 0);
	m_FontChoice->SetSelection(font);
	if (m_FontChoice->GetSelection() < Font.size())
		m_Log->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, Font.at(m_FontChoice->GetSelection())));
	ini.Get("Options", "WriteToFile", &m_writeFile, true);
	m_writeFileCB->SetValue(m_writeFile);
	ini.Get("Options", "WriteToConsole", &m_writeConsole, true);
	m_writeConsoleCB->SetValue(m_writeConsole);
	ini.Get("Options", "WriteToWindow", &m_writeWindow, true);
	m_writeWindowCB->SetValue(m_writeWindow);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		bool enable;
		ini.Get("Logs", m_LogManager->getShortName((LogTypes::LOG_TYPE)i), &enable, true);

		if (m_writeWindow && enable)
			m_LogManager->addListener((LogTypes::LOG_TYPE)i, this);
		else
			m_LogManager->removeListener((LogTypes::LOG_TYPE)i, this);

		if (m_writeFile && enable)
			m_LogManager->addListener((LogTypes::LOG_TYPE)i, m_fileLog);
		else
			m_LogManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);

		if (m_writeConsole && enable)
			m_LogManager->addListener((LogTypes::LOG_TYPE)i, m_console);
		else
			m_LogManager->removeListener((LogTypes::LOG_TYPE)i, m_console);
		m_LogManager->setLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)(verbosity));
	}
	UpdateChecks();
}

void CLogWindow::OnSubmit(wxCommandEvent& WXUNUSED (event))
{
	if (!m_cmdline) return;
	Console_Submit(m_cmdline->GetValue().To8BitData());
	m_cmdline->SetValue(wxEmptyString);
	NotifyUpdate();
}

void CLogWindow::OnClear(wxCommandEvent& WXUNUSED (event))
{
	m_Log->Clear();

	m_LogSection.Enter();
	int msgQueueSize = (int)msgQueue.size();
	for (int i = 0; i < msgQueueSize; i++) 
		msgQueue.pop();
	m_LogSection.Leave();
	
	m_console->ClearScreen();
	NOTICE_LOG(CONSOLE, "Console cleared");
	NotifyUpdate();
}

// Enable or disable all boxes for the current verbosity level and save the changes.
void CLogWindow::OnToggleAll(wxCommandEvent& WXUNUSED (event))
{
	static bool enableAll = false;
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		ToggleLog(i, enableAll);
	}

	SaveSettings();
	enableAll = !enableAll;
}

// Append checkboxes and update checked groups. 
void CLogWindow::UpdateChecks()
{
	// This is only run once to append checkboxes to the wxCheckListBox.
	if (m_checks->GetCount() == 0)
	{
		// [F|RES] hide the window while we fill it... wxwidgets gets trouble
		// if you don't do it (at least the win version)
		m_checks->Freeze();

		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		{
			m_checks->Append(wxString::FromAscii(m_LogManager->getFullName( (LogTypes::LOG_TYPE)i )));
		}
		m_checks->Thaw();
	}

	m_checks->Freeze();
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		m_checks->Check(i,
			m_LogManager->isListener((LogTypes::LOG_TYPE)i, this) ||
			m_LogManager->isListener((LogTypes::LOG_TYPE)i, m_console) ||
			m_LogManager->isListener((LogTypes::LOG_TYPE)i, m_fileLog));
	}
	m_checks->Thaw();
}

void CLogWindow::UnPopulateRight()
{
	sRight->Detach(m_Log);
	sRight->Detach(sRightBottom);
	sRightBottom = new wxBoxSizer(wxHORIZONTAL);
}
void CLogWindow::PopulateRight()
{
	sRightBottom->Add(m_cmdline, 1, wxEXPAND);
	sRight->Add(m_Log, 1, wxEXPAND | wxSHRINK);
	sRight->Add(sRightBottom, 0, wxEXPAND);
	this->Layout();
}

wxTextCtrl* CLogWindow::CreateTextCtrl(wxPanel* parent, wxWindowID id, long Style)
{
	wxTextCtrl* TC = new wxTextCtrl(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, Style);	
	TC->SetBackgroundColour(*wxBLACK);
	if (m_FontChoice)
	{
		if (m_FontChoice->GetSelection() < Font.size())
			TC->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, Font.at(m_FontChoice->GetSelection())));
	}
	return TC;
}

// When an option is changed, save the change
void CLogWindow::OnOptionsCheck(wxCommandEvent& event)
{
	wxString Text;

 	switch (event.GetId())
	{
	case IDM_VERBOSITY:
		{
			// get selection
			int v = m_verbosity->GetSelection() + 1;
			for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
			{
				m_LogManager->setLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)v);
			}
		}
		break;

	case IDM_WRAPLINE:		
		// SetWindowStyleFlag doesn't fully work, we need to redraw the window
		//m_Log->SetWindowStyleFlag(m_Log->GetWindowStyleFlag() ^ wxTE_DONTWRAP);
		/* Notice:	To retain the colors when changing word wrapping we need to
					loop through every letter with GetStyle and then reapply them letter by letter */
		// Prevent m_Log access while it's being destroyed
		m_LogAccess = false;
		UnPopulateRight();
		Text = m_Log->GetValue();
		m_Log->Destroy();
		switch (event.IsChecked())
		{
			case 0: m_Log = CreateTextCtrl(this, IDM_LOG, wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP); break;
			case 1: m_Log = CreateTextCtrl(this, IDM_LOG, wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP); break;
		}
		m_Log->SetDefaultStyle(wxTextAttr(*wxWHITE));
		m_Log->AppendText(Text);
		PopulateRight();
		m_LogAccess = true;		
		break;

	case IDM_FONT:
		// Update selected font
		Font.at(Font.size()-1) = DebuggerFont;
		m_Log->SetStyle(0, m_Log->GetLastPosition(), wxTextAttr(wxNullColour, wxNullColour, Font.at(event.GetSelection())));
		m_Log->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, Font.at(event.GetSelection())));
		break;

	case IDM_WRITEFILE:
		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		{
			m_writeFile = event.IsChecked();
			if (m_checks->IsChecked(i))
			{
				if (m_writeFile)
					m_LogManager->addListener((LogTypes::LOG_TYPE)i, m_fileLog);
				else
					m_LogManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);
			}
		}
 		break;

	case IDM_WRITEWINDOW:
		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		{
			m_writeWindow = event.IsChecked();
			if (m_checks->IsChecked(i))
			{
				if (m_writeWindow)
					m_LogManager->addListener((LogTypes::LOG_TYPE)i, this);
				else
					m_LogManager->removeListener((LogTypes::LOG_TYPE)i, this);
			}
		}
 		break;

	case IDM_WRITECONSOLE:
		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		{
			m_writeConsole = event.IsChecked();
			if (m_checks->IsChecked(i))
			{
				if (m_writeConsole)
					m_LogManager->addListener((LogTypes::LOG_TYPE)i, m_console);
				else
					m_LogManager->removeListener((LogTypes::LOG_TYPE)i, m_console);
			}
		}
		/*
		if (m_writeConsole && !m_console->IsOpen())
			wxGetApp().GetCFrame()->ToggleConsole(true);
		else if (!m_writeConsole && m_console->IsOpen())
			wxGetApp().GetCFrame()->ToggleConsole(false);
		*/
		break;
	}
	SaveSettings();
}

// When a checkbox is changed
void CLogWindow::OnLogCheck(wxCommandEvent& event)
{
	int i = event.GetInt();
	ToggleLog(i, m_checks->IsChecked(i));

	SaveSettings();
}

void CLogWindow::ToggleLog(int _logType, bool enable)
{
	LogTypes::LOG_TYPE logType = (LogTypes::LOG_TYPE)_logType;
	
	m_checks->Check(_logType, enable);

	m_LogManager->setEnable(logType, enable);

	if (enable)
	{
		if (m_writeWindow)
			m_LogManager->addListener(logType, this);
		if (m_writeFile)
			m_LogManager->addListener(logType, m_fileLog);
		if (m_writeConsole)
			m_LogManager->addListener(logType, m_console);
	}
	else
	{
		m_LogManager->removeListener(logType, this);
		m_LogManager->removeListener(logType, m_fileLog);
		m_LogManager->removeListener(logType, m_console);
	}
}

void CLogWindow::OnLogTimer(wxTimerEvent& WXUNUSED(event))
{
	if (!m_LogAccess) return;

	//m_Log->Freeze();
	UpdateLog();
	// Better auto scroll than wxTE_AUTO_SCROLL
	if (msgQueue.size() > 0)
	{
		m_Log->ScrollLines(1);
		m_Log->ShowPosition( m_Log->GetLastPosition() );
	}
	//m_Log->Thaw();
}

void CLogWindow::NotifyUpdate()
{
	UpdateChecks();
	//UpdateLog();
}

void CLogWindow::UpdateLog()
{
	if (!m_LogAccess) return;
	if (!m_Log) return;

	m_LogTimer->Stop();
	wxString collected_text;

	m_LogSection.Enter();
	// Rough estimate
	collected_text.reserve(100 * msgQueue.size());
	int msgQueueSize = (int)msgQueue.size();
	for (int i = 0; i < msgQueueSize; i++)
	{
 		switch (msgQueue.front().first)
 		{
 			// red
 		case ERROR_LEVEL:
 			m_Log->SetDefaultStyle(wxTextAttr(*wxRED));
 			break;
 			// yellow
 		case WARNING_LEVEL:
 			m_Log->SetDefaultStyle(wxTextAttr(wxColour(255, 255, 0)));
 			break;
 			// green
 		case NOTICE_LEVEL:
 			m_Log->SetDefaultStyle(wxTextAttr(*wxGREEN));
 			break;
 			// cyan
 		case INFO_LEVEL:
 			m_Log->SetDefaultStyle(wxTextAttr(*wxCYAN));
 			break;
 			// light gray
 		case DEBUG_LEVEL:
 			m_Log->SetDefaultStyle(wxTextAttr(wxColour(211, 211, 211)));
 			break;
 			// white
 		default:
 			m_Log->SetDefaultStyle(wxTextAttr(*wxWHITE));
 			break;
 		}
		if (msgQueue.front().second.size()) 
		{
			int j = m_Log->GetLastPosition();
			m_Log->AppendText(msgQueue.front().second);
			// White timestamp
			m_Log->SetStyle(j, j + 9, wxTextAttr(*wxWHITE));
		}
		collected_text.Append(msgQueue.front().second);
		msgQueue.pop();		
	}
	m_LogSection.Leave();
	// Write all text at once, needs multiple SetStyle, may not be better
	//if (collected_text.size()) m_Log->AppendText(collected_text);
	// Return in 0.2 seconds
	m_LogTimer->Start(UPDATETIME);
}

// Receive log messages
void CLogWindow::Log(LogTypes::LOG_LEVELS level, const char *text) 
{
	m_LogSection.Enter();
	// Remove the first message if the queue is big
	//if (msgQueue.size() >= 200) msgQueue.pop();
	msgQueue.push(std::pair<u8, wxString>((u8)level, wxString::FromAscii(text)));
	m_LogSection.Leave();
}
