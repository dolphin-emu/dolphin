// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Debugger.h"

#include "RegisterWindow.h"
#include "LogWindow.h"

#include "wx/button.h"
#include "wx/textctrl.h"
#include "wx/listctrl.h"
#include "wx/thread.h"
#include "wx/listctrl.h"
#include "CodeWindow.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "Host.h"

#include "Debugger/PPCDebugInterface.h"
#include "Debugger/Debugger_SymbolMap.h"

#include "Core.h"
#include "LogManager.h"

// ugly that this lib included code from the main
#include "../../DolphinWX/src/globals.h"

class SymbolList
	: public wxListCtrl
{
	wxString OnGetItemText(long item, long column)
	{
		return(_T("hello"));
	}
};

enum
{
	IDM_DEBUG_GO = 350,
	IDM_STEP,
	IDM_STEPOVER,
	IDM_SKIP,
	IDM_SETPC,
	IDM_GOTOPC,
	IDM_ADDRBOX,
	IDM_CALLSTACKLIST,
	IDM_SYMBOLLIST,
	IDM_INTERPRETER,
	IDM_DUALCORE,
	IDM_LOGWINDOW,
	IDM_REGISTERWINDOW
};

BEGIN_EVENT_TABLE(CCodeWindow, wxFrame)
EVT_BUTTON(IDM_DEBUG_GO,        CCodeWindow::OnCodeStep)
EVT_BUTTON(IDM_STEP,            CCodeWindow::OnCodeStep)
EVT_BUTTON(IDM_STEPOVER,        CCodeWindow::OnCodeStep)
EVT_BUTTON(IDM_SKIP,            CCodeWindow::OnCodeStep)
EVT_BUTTON(IDM_SETPC,           CCodeWindow::OnCodeStep)
EVT_BUTTON(IDM_GOTOPC,          CCodeWindow::OnCodeStep)
EVT_TEXT(IDM_ADDRBOX,           CCodeWindow::OnAddrBoxChange)
EVT_LISTBOX(IDM_SYMBOLLIST,     CCodeWindow::OnSymolListChange)
EVT_LISTBOX(IDM_CALLSTACKLIST,  CCodeWindow::OnCallstackListChange)
EVT_HOST_COMMAND(wxID_ANY,      CCodeWindow::OnHostMessage)
EVT_MENU(IDM_LOGWINDOW,         CCodeWindow::OnToggleLogWindow)
EVT_MENU(IDM_REGISTERWINDOW,    CCodeWindow::OnToggleRegisterWindow)
END_EVENT_TABLE()


CCodeWindow::CCodeWindow(const SCoreStartupParameter& _LocalCoreStartupParameter, wxWindow* parent, wxWindowID id,
		const wxString& title, const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(parent, id, title, pos, size, style)
	, m_RegisterWindow(NULL)
	, m_logwindow(NULL)
{
	CreateMenu(_LocalCoreStartupParameter);

	wxBoxSizer* sizerBig   = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerLeft  = new wxBoxSizer(wxVERTICAL);

	DebugInterface* di = new PPCDebugInterface();

	sizerLeft->Add(callstack = new wxListBox(this, IDM_CALLSTACKLIST, wxDefaultPosition, wxSize(90, 100)), 0, wxEXPAND);
	sizerLeft->Add(symbols = new wxListBox(this, IDM_SYMBOLLIST, wxDefaultPosition, wxSize(90, 100), 0, NULL, wxLB_SORT), 1, wxEXPAND);
	codeview = new CCodeView(di, this, wxID_ANY);
	sizerBig->Add(sizerLeft, 2, wxEXPAND);
	sizerBig->Add(codeview, 5, wxEXPAND);
	sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerRight->Add(buttonGo = new wxButton(this, IDM_DEBUG_GO, _T("&Go")));
	sizerRight->Add(buttonStep = new wxButton(this, IDM_STEP, _T("&Step")));
	sizerRight->Add(buttonStepOver = new wxButton(this, IDM_STEPOVER, _T("Step &Over")));
	sizerRight->Add(buttonSkip = new wxButton(this, IDM_SKIP, _T("Ski&p")));
	sizerRight->Add(buttonGotoPC = new wxButton(this, IDM_GOTOPC, _T("G&oto PC")));
	sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_ADDRBOX, _T("")));
	sizerRight->Add(new wxButton(this, IDM_SETPC, _T("S&et PC")));

	SetSizer(sizerBig);

	sizerLeft->SetSizeHints(this);
	sizerLeft->Fit(this);
	sizerRight->SetSizeHints(this);
	sizerRight->Fit(this);
	sizerBig->SetSizeHints(this);
	sizerBig->Fit(this);

	sync_event.Init();

	// additional dialogs
	if (IsLoggingActivated())
	{
		m_logwindow = new CLogWindow(this);
		m_logwindow->Show(true);
	}

	m_RegisterWindow = new CRegisterWindow(this);
	m_RegisterWindow->Show(true);


	UpdateButtonStates();
}


void CCodeWindow::CreateMenu(const SCoreStartupParameter& _LocalCoreStartupParameter)
{
	wxMenuBar* pMenuBar = new wxMenuBar(wxMB_DOCKABLE);

	{
		wxMenu* pDebugMenu = new wxMenu;
		wxMenuItem* interpreter = pDebugMenu->Append(IDM_INTERPRETER, _T("&Interpreter"), wxEmptyString, wxITEM_CHECK);
		interpreter->Check(!_LocalCoreStartupParameter.bUseDynarec);

		wxMenuItem* dualcore = pDebugMenu->Append(IDM_DUALCORE, _T("&DualCore"), wxEmptyString, wxITEM_CHECK);
		dualcore->Check(_LocalCoreStartupParameter.bUseDualCore);

		pMenuBar->Append(pDebugMenu, _T("&Core Startup"));
	}

	{
		wxMenu* pDebugDialogs = new wxMenu;

		if (IsLoggingActivated())
		{
			wxMenuItem* pLogWindow = pDebugDialogs->Append(IDM_LOGWINDOW, _T("&LogManager"), wxEmptyString, wxITEM_CHECK);
			pLogWindow->Check(true);
		}

		wxMenuItem* pRegister = pDebugDialogs->Append(IDM_REGISTERWINDOW, _T("&Registers"), wxEmptyString, wxITEM_CHECK);
		pRegister->Check(true);

		pMenuBar->Append(pDebugDialogs, _T("&Dialogs"));
	}

	SetMenuBar(pMenuBar);
}


bool CCodeWindow::UseInterpreter()
{
	return(GetMenuBar()->IsChecked(IDM_INTERPRETER));
}


bool CCodeWindow::UseDualCore()
{
	return(GetMenuBar()->IsChecked(IDM_DUALCORE));
}


void CCodeWindow::OnCodeStep(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case IDM_DEBUG_GO:
	    {
		    // [F|RES] prolly we should disable the other buttons in go mode too ...
		    codeview->Center(PC);

		    if (CCPU::IsStepping())
		    {
			    CCPU::EnableStepping(false);
		    }
		    else
		    {
			    CCPU::EnableStepping(true);
			    Host_UpdateLogDisplay();
		    }

		    Update();
	    }
		    break;

	    case IDM_STEP:
	    {
		    CCPU::StepOpcode(&sync_event);
//            if (CCPU::IsStepping())
//	            sync_event.Wait();
		    wxThread::Sleep(20);
		    // need a short wait here
		    codeview->Center(PC);
		    Update();
		    Host_UpdateLogDisplay();
	    }
		    break;

	    case IDM_STEPOVER:
		    CCPU::EnableStepping(true);
		    break;

	    case IDM_SKIP:
		    PC += 4;
		    Update();
		    break;

	    case IDM_SETPC:
		    PC = codeview->GetSelection();
		    Update();
		    break;

	    case IDM_GOTOPC:
		    codeview->Center(PC);
		    break;
	}

	UpdateButtonStates();
}


void CCodeWindow::OnAddrBoxChange(wxCommandEvent& event)
{
	wxString txt = addrbox->GetValue();

	if (txt.size() == 8)
	{
		u32 addr;
		sscanf(txt.mb_str(), "%08x", &addr);
		codeview->Center(addr);
	}

	event.Skip(1);
}


void CCodeWindow::Update()
{
	codeview->Refresh();

	callstack->Clear();

	std::vector<Debugger::SCallstackEntry>stack;

	if (Debugger::GetCallstack(stack))
	{
		for (size_t i = 0; i < stack.size(); i++)
		{
			int idx = callstack->Append(wxString::FromAscii(stack[i].Name.c_str()));
			callstack->SetClientData(idx, (void*)(u64)stack[i].vAddress);
		}
	}
	else
	{
		callstack->Append("invalid callstack");
	}

	UpdateButtonStates();

	codeview->Center(PC);

	Host_UpdateLogDisplay();
}


void CCodeWindow::NotifyMapLoaded()
{
	symbols->Show(false); // hide it for faster filling
	symbols->Clear();
#ifdef _WIN32
	const Debugger::XVectorSymbol& syms = Debugger::AccessSymbols();

	for (int i = 0; i < (int)syms.size(); i++)
	{
		int idx = symbols->Append(syms[i].GetName().c_str());
		symbols->SetClientData(idx, (void*)&syms[i]);
	}

	//
#endif

	symbols->Show(true);
	Update();
}


void CCodeWindow::UpdateButtonStates()
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
	{
		buttonGo->Enable(false);
		buttonStep->Enable(false);
		buttonStepOver->Enable(false);
		buttonSkip->Enable(false);
	}
	else
	{
		if (!CCPU::IsStepping())
		{
			buttonGo->SetLabel(_T("&Pause"));
			buttonGo->Enable(true);
			buttonStep->Enable(false);
			buttonStepOver->Enable(false);
			buttonSkip->Enable(false);
		}
		else
		{
			buttonGo->SetLabel(_T("&Go"));
			buttonGo->Enable(true);
			buttonStep->Enable(true);
			buttonStepOver->Enable(true);
			buttonSkip->Enable(true);
		}
	}
}


void CCodeWindow::OnSymolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	Debugger::CSymbol* pSymbol = static_cast<Debugger::CSymbol*>(symbols->GetClientData(index));

	if (pSymbol != NULL)
	{
		codeview->Center(pSymbol->vaddress);
	}
}


void CCodeWindow::OnCallstackListChange(wxCommandEvent& event)
{
	int index   = callstack->GetSelection();
	u32 address = (u32)(u64)(callstack->GetClientData(index));

	if (address != 0x00)
	{
		codeview->Center(address);
	}
}


void CCodeWindow::OnToggleLogWindow(wxCommandEvent& event)
{
	if (IsLoggingActivated())
	{
		bool show = GetMenuBar()->IsChecked(event.GetId());

		if (show)
		{
			if (!m_logwindow)
			{
				m_logwindow = new CLogWindow(this);
			}

			m_logwindow->Show(true);
		}
		else // hide
		{
			// If m_dialog is NULL, then possibly the system
			// didn't report the checked menu item status correctly.
			// It should be true just after the menu item was selected,
			// if there was no modeless dialog yet.
			wxASSERT(m_logwindow != NULL);

			if (m_logwindow)
			{
				m_logwindow->Hide();
			}
		}
	}
}


void CCodeWindow::OnToggleRegisterWindow(wxCommandEvent& event)
{
	bool show = GetMenuBar()->IsChecked(event.GetId());

	if (show)
	{
		if (!m_RegisterWindow)
		{
			m_RegisterWindow = new CRegisterWindow(this);
		}

		m_RegisterWindow->Show(true);
	}
	else // hide
	{
		// If m_dialog is NULL, then possibly the system
		// didn't report the checked menu item status correctly.
		// It should be true just after the menu item was selected,
		// if there was no modeless dialog yet.
		wxASSERT(m_RegisterWindow != NULL);

		if (m_RegisterWindow)
		{
			m_RegisterWindow->Hide();
		}
	}
}


void CCodeWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case IDM_NOTIFYMAPLOADED:
		    NotifyMapLoaded();
		    break;

	    case IDM_UPDATELOGDISPLAY:

		    if (m_logwindow)
		    {
			    m_logwindow->NotifyUpdate();
		    }

		    break;

	    case IDM_UPDATEDISASMDIALOG:
		    Update();

		    if (m_RegisterWindow)
		    {
			    m_RegisterWindow->NotifyUpdate();
		    }

		    break;
	}
}


