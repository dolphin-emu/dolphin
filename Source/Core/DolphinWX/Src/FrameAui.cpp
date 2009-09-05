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


// Why doesn't it work on windows?
#ifndef _WIN32
#include "Common.h"
#endif

#include "Setup.h" // Common

#if defined(HAVE_SFML) && HAVE_SFML || defined(_WIN32)
#include "NetWindow.h"
#endif

#include "Common.h" // Common
#include "FileUtil.h"
#include "FileSearch.h"
#include "Timer.h"

#include "Globals.h" // Local
#include "Frame.h"
#include "ConfigMain.h"
#include "PluginManager.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "InfoWindow.h"
#include "AboutDolphin.h"
#include "GameListCtrl.h"
#include "BootManager.h"
#include "LogWindow.h"
#include "WxUtils.h"

#include "ConfigManager.h" // Core
#include "Core.h"
#include "OnFrame.h"
#include "HW/DVDInterface.h"
#include "State.h"
#include "VolumeHandler.h"
#include "NANDContentLoader.h"

#include <wx/datetime.h> // wxWidgets


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Aui events
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void CFrame::OnManagerResize(wxAuiManagerEvent& event)
{
	event.Skip();
	ResizeConsole();
}
void CFrame::OnResize(wxSizeEvent& event)
{
	event.Skip();
	// fit frame content, not needed right now
	//FitInside();
	DoMoveIcons();  // In FrameWiimote.cpp
}

void CFrame::OnPaneClose(wxAuiManagerEvent& event)
{
	event.Veto();

	wxAuiNotebook * nb = (wxAuiNotebook*)event.pane->window;
	if (!nb) return;
	if (! (nb->GetPageCount() == 0 || (nb->GetPageCount() == 1 && nb->GetPageText(0).IsSameAs(wxT("<>")))))
	{
		wxMessageBox(wxT("You can't close panes that have pages in them."), wxT("Notice"), wxOK, this);		
	}
	else
	{
		/*
		ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
		Console->Log(LogTypes::LNOTICE, StringFromFormat("GetNotebookCount before: %i\n", GetNotebookCount()).c_str());
		*/

		// Detach and delete the empty notebook
		event.pane->DestroyOnClose(true);
		m_Mgr->ClosePane(*event.pane);

		//Console->Log(LogTypes::LNOTICE, StringFromFormat("GetNotebookCount after: %i\n", GetNotebookCount()).c_str());
	}

	m_Mgr->Update();
}


// Enable and disable the log window
void CFrame::OnToggleLogWindow(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceLogWindow = event.IsChecked();
	DoToggleWindow(event.GetId(), event.IsChecked());
}
void CFrame::ToggleLogWindow(bool _show, int i)
{	
	if (_show)
	{
		if (!m_LogWindow) m_LogWindow = new CLogWindow(this, IDM_LOGWINDOW);
		#ifdef _WIN32
		DoAddPage(m_LogWindow, i, wxT("Log"));
		#else
		m_LogWindow->Show();
		#endif
	}
	else
	{
		#ifdef _WIN32
		DoRemovePage(m_LogWindow);
		#else
		if (m_LogWindow) m_LogWindow->Show();
		#endif
	}

	// Hide pane
	if (!UseDebugger) HidePane();

	// Make sure the check is updated (if wxw isn't calling this func)
	//GetMenuBar()->FindItem(IDM_LOGWINDOW)->Check(Show);
}
// Enable and disable the console
void CFrame::OnToggleConsole(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceConsole = event.IsChecked();
	DoToggleWindow(event.GetId(), event.IsChecked());
}
void CFrame::ToggleConsole(bool _show, int i)
{
	ConsoleListener *Console = LogManager::GetInstance()->getConsoleListener();	

	if (_show)
	{
		//Console->Log(LogTypes::LNOTICE, StringFromFormat(" >>> Show\n").c_str());

		if (GetNotebookCount() == 0) return;
		if (i < 0 || i > GetNotebookCount()-1) i = 0;

		#ifdef _WIN32
		wxWindow *Win = GetWxWindowHwnd(GetConsoleWindow());
		if (Win && GetNotebookFromId(i)->GetPageIndex(Win) != wxNOT_FOUND) return;
		{
			#else
			Console->Open();
			#endif

			#ifdef _WIN32
			if(!GetConsoleWindow()) Console->Open(); else ShowWindow(GetConsoleWindow(),SW_SHOW);
		}
		Win = GetWxWindowHwnd(GetConsoleWindow());
		// Can we remove the border?
		//Win->SetWindowStyleFlag(wxNO_BORDER);
		//SetWindowLong(GetConsoleWindow(), GWL_STYLE, WS_VISIBLE);
		// Create parent window
		wxPanel * ConsoleParent = CreateEmptyPanel(IDM_CONSOLEWINDOW);
		::SetParent(GetConsoleWindow(), (HWND)ConsoleParent->GetHWND());
		//Win->SetParent(ConsoleParent);
		//if (Win) m_Mgr->GetAllPanes().Item(i)->AddPage(Win, wxT("Console"), true, aNormalFile );
		if (Win) GetNotebookFromId(i)->AddPage(ConsoleParent, wxT("Console"), true, aNormalFile );
		#endif
	}
	else // hide
	{
		//Console->Log(LogTypes::LNOTICE, StringFromFormat(" >>> Show\n").c_str());

		#ifdef _WIN32
		// Hide
		if(GetConsoleWindow()) ShowWindow(GetConsoleWindow(),SW_HIDE);
		// Release the console to Windows
		::SetParent(GetConsoleWindow(), NULL);
		// Destroy the empty parent of the console
		DoRemovePageString(wxT("Console"), true, true);

		#else
		Console->Close();
		#endif
	}

	// Hide pane
	if (!UseDebugger) HidePane();

	// Make sure the check is updated (if wxw isn't calling this func)
	//GetMenuBar()->FindItem(IDM_CONSOLEWINDOW)->Check(Show);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Notebooks
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CFrame::ClosePages()
{
	DoToggleWindow(IDM_LOGWINDOW, false);
	DoToggleWindow(IDM_CONSOLEWINDOW, false);
	DoToggleWindow(IDM_CODEWINDOW, false);
	DoToggleWindow(IDM_REGISTERWINDOW, false);
	DoToggleWindow(IDM_BREAKPOINTWINDOW, false);
	DoToggleWindow(IDM_MEMORYWINDOW, false);
	DoToggleWindow(IDM_JITWINDOW, false);
	DoToggleWindow(IDM_SOUNDWINDOW, false);
	DoToggleWindow(IDM_VIDEOWINDOW, false);
}
void CFrame::DoToggleWindow(int Id, bool _show)
{		
	switch (Id)
	{
		case IDM_LOGWINDOW: ToggleLogWindow(_show, UseDebugger ? g_pCodeWindow->iLogWindow : 0); break;
		case IDM_CONSOLEWINDOW: ToggleConsole(_show, UseDebugger ? g_pCodeWindow->iConsoleWindow : 0); break;
	}

	if (!UseDebugger) return;

	switch (Id)
	{
		case IDM_CODEWINDOW: g_pCodeWindow->OnToggleCodeWindow(_show, g_pCodeWindow->iCodeWindow); break;
		case IDM_REGISTERWINDOW: g_pCodeWindow->OnToggleRegisterWindow(_show, g_pCodeWindow->iRegisterWindow); break;
		case IDM_BREAKPOINTWINDOW: g_pCodeWindow->OnToggleBreakPointWindow(_show, g_pCodeWindow->iBreakpointWindow); break;
		case IDM_MEMORYWINDOW: g_pCodeWindow->OnToggleMemoryWindow(_show, g_pCodeWindow->iMemoryWindow); break;
		case IDM_JITWINDOW: g_pCodeWindow->OnToggleJitWindow(_show, g_pCodeWindow->iJitWindow); break;
		case IDM_SOUNDWINDOW: g_pCodeWindow->OnToggleSoundWindow(_show, g_pCodeWindow->iSoundWindow); break;
		case IDM_VIDEOWINDOW: g_pCodeWindow->OnToggleVideoWindow(_show, g_pCodeWindow->iVideoWindow); break;
	}
}
void CFrame::OnNotebookPageChanged(wxAuiNotebookEvent& event)
{
	event.Skip();
	if (!UseDebugger) return;

	// Remove the blank page if any
	AddRemoveBlankPage();

	// Update the notebook affiliation
	if(GetNootebookAffiliation(wxT("Log")) >= 0) g_pCodeWindow->iLogWindow = GetNootebookAffiliation(wxT("Log"));
	if(GetNootebookAffiliation(wxT("Console")) >= 0) g_pCodeWindow->iConsoleWindow = GetNootebookAffiliation(wxT("Console"));
	if(GetNootebookAffiliation(wxT("Code")) >= 0) g_pCodeWindow->iCodeWindow = GetNootebookAffiliation(wxT("Code"));
	if(GetNootebookAffiliation(wxT("Registers")) >= 0) g_pCodeWindow->iRegisterWindow = GetNootebookAffiliation(wxT("Registers"));
	if(GetNootebookAffiliation(wxT("Breakpoints")) >= 0) g_pCodeWindow->iBreakpointWindow = GetNootebookAffiliation(wxT("Breakpoints"));
	if(GetNootebookAffiliation(wxT("JIT")) >= 0) g_pCodeWindow->iJitWindow = GetNootebookAffiliation(wxT("JIT"));
	if(GetNootebookAffiliation(wxT("Memory")) >= 0) g_pCodeWindow->iMemoryWindow = GetNootebookAffiliation(wxT("Memory"));
	if(GetNootebookAffiliation(wxT("Sound")) >= 0) g_pCodeWindow->iSoundWindow = GetNootebookAffiliation(wxT("Sound"));
	if(GetNootebookAffiliation(wxT("Video")) >= 0) g_pCodeWindow->iVideoWindow = GetNootebookAffiliation(wxT("Video"));
}
void CFrame::OnNotebookPageClose(wxAuiNotebookEvent& event)
{
	// Override event
	event.Veto();

    wxAuiNotebook* Ctrl = (wxAuiNotebook*)event.GetEventObject();

	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Log"))) { GetMenuBar()->FindItem(IDM_LOGWINDOW)->Check(false); DoToggleWindow(IDM_LOGWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Console"))) { GetMenuBar()->FindItem(IDM_CONSOLEWINDOW)->Check(false); DoToggleWindow(IDM_CONSOLEWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Registers"))) { GetMenuBar()->FindItem(IDM_REGISTERWINDOW)->Check(false); DoToggleWindow(IDM_REGISTERWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Breakpoints"))) { GetMenuBar()->FindItem(IDM_BREAKPOINTWINDOW)->Check(false); DoToggleWindow(IDM_BREAKPOINTWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("JIT"))) { GetMenuBar()->FindItem(IDM_JITWINDOW)->Check(false); DoToggleWindow(IDM_JITWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Memory"))) { GetMenuBar()->FindItem(IDM_MEMORYWINDOW)->Check(false); DoToggleWindow(IDM_MEMORYWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Sound"))) { GetMenuBar()->FindItem(IDM_SOUNDWINDOW)->Check(false); DoToggleWindow(IDM_SOUNDWINDOW, false); }
	if (Ctrl->GetPageText(event.GetSelection()).IsSameAs(wxT("Video"))) { GetMenuBar()->FindItem(IDM_VIDEOWINDOW)->Check(false); DoToggleWindow(IDM_VIDEOWINDOW, false); }
}
void CFrame::OnFloatWindow(wxCommandEvent& event)
{
	switch(event.GetId())
	{
		case IDM_FLOAT_LOGWINDOW: if (GetNootebookPageFromId(IDM_LOGWINDOW)) { DoFloatPage(m_LogWindow); return; } break;
		case IDM_FLOAT_CONSOLEWINDOW: if (GetNootebookPageFromId(IDM_CONSOLEWINDOW)) { DoFloatPage(m_LogWindow); return; } break;
	}
	switch(event.GetId())
	{
		case IDM_FLOAT_LOGWINDOW: if (FindWindowById(IDM_LOGWINDOW)) DoUnfloatPage(IDM_LOGWINDOW); break;
		case IDM_FLOAT_CONSOLEWINDOW: if (FindWindowById(IDM_CONSOLEWINDOW)) DoUnfloatPage(IDM_CONSOLEWINDOW); break;
	}

	if (!UseDebugger) return;

	switch(event.GetId())
	{
		case IDM_FLOAT_CODEWINDOW: if (GetNootebookPageFromId(IDM_CODEWINDOW)) { DoFloatPage(g_pCodeWindow); return; } break;
		case IDM_FLOAT_REGISTERWINDOW: if (GetNootebookPageFromId(IDM_REGISTERWINDOW)) { DoFloatPage((wxWindow*)g_pCodeWindow->m_RegisterWindow); return; } break;
		case IDM_FLOAT_BREAKPOINTWINDOW: if (GetNootebookPageFromId(IDM_BREAKPOINTWINDOW)) { DoFloatPage((wxWindow*)g_pCodeWindow->m_BreakpointWindow); return; } break;
		case IDM_FLOAT_MEMORYWINDOW: if (GetNootebookPageFromId(IDM_MEMORYWINDOW)) { DoFloatPage((wxWindow*)g_pCodeWindow->m_MemoryWindow); return; } break;
		case IDM_FLOAT_JITWINDOW: if (GetNootebookPageFromId(IDM_JITWINDOW)) { DoFloatPage((wxWindow*)g_pCodeWindow->m_JitWindow); return; } break;
	}
	switch(event.GetId())
	{
		case IDM_FLOAT_CODEWINDOW: if (FindWindowById(IDM_CODEWINDOW)) DoUnfloatPage(IDM_LOGWINDOW); break;
		case IDM_FLOAT_REGISTERWINDOW: if (FindWindowById(IDM_REGISTERWINDOW)) DoUnfloatPage(IDM_REGISTERWINDOW); break;
		case IDM_FLOAT_BREAKPOINTWINDOW: if (FindWindowById(IDM_BREAKPOINTWINDOW)) DoUnfloatPage(IDM_BREAKPOINTWINDOW); break;
		case IDM_FLOAT_MEMORYWINDOW: if (FindWindowById(IDM_MEMORYWINDOW)) DoUnfloatPage(IDM_MEMORYWINDOW); break;
		case IDM_FLOAT_JITWINDOW: if (FindWindowById(IDM_JITWINDOW)) DoUnfloatPage(IDM_JITWINDOW); break;
	}
}
void CFrame::OnTab(wxAuiNotebookEvent& event)
{
	event.Skip();

	// Create the popup menu
	wxMenu MenuPopup;

	wxMenuItem* Item =  new wxMenuItem(&MenuPopup, wxID_ANY, wxT("Select floating windows"));
	MenuPopup.Append(Item);
	Item->Enable(false);
	MenuPopup.Append(new wxMenuItem(&MenuPopup));
	Item =  new wxMenuItem(&MenuPopup, IDM_FLOAT_LOGWINDOW, WindowNameFromId(IDM_LOGWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_LOGWINDOW) && !GetNootebookPageFromId(IDM_LOGWINDOW));
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_CONSOLEWINDOW, WindowNameFromId(IDM_CONSOLEWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_CONSOLEWINDOW) && !GetNootebookPageFromId(IDM_CONSOLEWINDOW));
	Item->Enable(false);
	MenuPopup.Append(new wxMenuItem(&MenuPopup));
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_REGISTERWINDOW, WindowNameFromId(IDM_REGISTERWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_REGISTERWINDOW) && !GetNootebookPageFromId(IDM_REGISTERWINDOW));
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_BREAKPOINTWINDOW, WindowNameFromId(IDM_BREAKPOINTWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_BREAKPOINTWINDOW) && !GetNootebookPageFromId(IDM_BREAKPOINTWINDOW));
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_MEMORYWINDOW, WindowNameFromId(IDM_MEMORYWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_MEMORYWINDOW) && !GetNootebookPageFromId(IDM_MEMORYWINDOW));
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_JITWINDOW, WindowNameFromId(IDM_JITWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_JITWINDOW) && !GetNootebookPageFromId(IDM_JITWINDOW));
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_SOUNDWINDOW, WindowNameFromId(IDM_SOUNDWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_SOUNDWINDOW) && !GetNootebookPageFromId(IDM_SOUNDWINDOW));
	Item->Enable(false);
	Item = new wxMenuItem(&MenuPopup, IDM_FLOAT_VIDEOWINDOW, WindowNameFromId(IDM_VIDEOWINDOW), wxT(""), wxITEM_CHECK);
	MenuPopup.Append(Item);
	Item->Check(FindWindowById(IDM_VIDEOWINDOW) && !GetNootebookPageFromId(IDM_VIDEOWINDOW));
	Item->Enable(false);

	// Line up our menu with the cursor
	wxPoint Pt = ::wxGetMousePosition();
	Pt = ScreenToClient(Pt);
	// Show
	PopupMenu(&MenuPopup, Pt);
}
void CFrame::OnAllowNotebookDnD(wxAuiNotebookEvent& event)
{
	event.Skip();
	event.Allow();
	//	wxAuiNotebook* Ctrl = (wxAuiNotebook*)event.GetEventObject();
	// If we drag away the last one the tab bar goes away and we can't add any panes to it
	//if (Ctrl->GetPageCount() == 1) Ctrl->AddPage(CreateEmptyPanel(), wxT("<>"), true);
}
void CFrame::HidePane()
{
	// Get the first notebook
	wxAuiNotebook * NB = NULL;
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook)))
			NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
	}
	if (NB) {
		if (NB->GetPageCount() == 0)
			m_Mgr->GetPane(wxT("Pane 1")).Hide();
		else
			m_Mgr->GetPane(wxT("Pane 1")).Show();
		m_Mgr->Update();
	}
	SetSimplePaneSize();
}

void CFrame::DoRemovePageString(wxString Str, bool /*_hide*/, bool _destroy)
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;
		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		for (u32 j = 0; j < NB->GetPageCount(); j++)
		{
			if (NB->GetPageText(j).IsSameAs(Str))
			{
				if (!_destroy)
				{
					// Reparent to avoid destruction if the notebook is closed and destroyed
					wxWindow * Win = NB->GetPage(j);
					NB->RemovePage(j);
					Win->Reparent(this);
				}
				else
				{
					NB->DeletePage(j);
				}
				//if (_hide) Win->Hide();
				break;
			}	
		}
	}

}
void CFrame::DoAddPage(wxWindow * Win, int i, wxString Name)
{
	if (!Win) return;
	if (GetNotebookCount() == 0) return;
	if (i < 0 || i > GetNotebookCount()-1) i = 0;
	if (Win && GetNotebookFromId(i)->GetPageIndex(Win) != wxNOT_FOUND) return;
	GetNotebookFromId(i)->AddPage(Win, Name, true, aNormalFile );

	//NOTICE_LOG(CONSOLE, "DoAddPage: %i", Win->GetId());

	/*
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LNOTICE, StringFromFormat("Add: %s\n", Name.c_str()).c_str());
	*/
}

void CFrame::DoUnfloatPage(int Id)
{
	//NOTICE_LOG(CONSOLE, "DoUnfloatPage: %i", Id);

	wxFrame * Win = (wxFrame*)this->FindWindowById(Id);
	wxWindow * Child = Win->GetWindowChildren().Item(0)->GetData();
	Child->Reparent(this);
	// Return the window id
	Child->SetId(Win->GetId());
	DoAddPage(Child, 0, Win->GetTitle());
	Win->Destroy();
}
void CFrame::OnFloatingPageClosed(wxCloseEvent& event)
{
	//NOTICE_LOG(CONSOLE, "OnFloatingPageClosed: %i", event.GetId());
	DoUnfloatPage(event.GetId());
}

void CFrame::DoFloatPage(wxWindow * Win)
{
	//NOTICE_LOG(CONSOLE, "DoFloatPage: %i %s", Win->GetId(), WindowNameFromId(Win->GetId()).mb_str());

	if (Win)
	{
		for (int i = 0; i < GetNotebookCount(); i++)
		{
			if (GetNotebookFromId(i)->GetPageIndex(Win) != wxNOT_FOUND)
			{
				GetNotebookFromId(i)->RemovePage(GetNotebookFromId(i)->GetPageIndex(Win));
				// Reparent to avoid destruction if the notebook is closed and destroyed
				CreateParentFrame(Win->GetId(), WindowNameFromId(Win->GetId()), Win);
			}
		}
	}
}

void CFrame::DoRemovePage(wxWindow * Win, bool _Hide)
{
	// If m_dialog is NULL, then possibly the system didn't report the checked menu item status correctly.
	// It should be true just after the menu item was selected, if there was no modeless dialog yet.
	//wxASSERT(Win != NULL);

	if (Win)
	{
		for (int i = 0; i < GetNotebookCount(); i++)
		{
			if (GetNotebookFromId(i)->GetPageIndex(Win) != wxNOT_FOUND)
			{
				GetNotebookFromId(i)->RemovePage(GetNotebookFromId(i)->GetPageIndex(Win));
				// Reparent to avoid destruction if the notebook is closed and destroyed
				Win->Reparent(this);
				if (_Hide) Win->Hide();
			}
		}
	}
}
wxFrame * CFrame::CreateParentFrame(wxWindowID Id, const wxString& Title, wxWindow * Child)
{
	//NOTICE_LOG(CONSOLE, "CreateParentFrame: %i %s %i", Id, Title.mb_str(), Child->GetId())

	wxFrame * Frame = new wxFrame(this, Id, Title);
	
	Child->Reparent(Frame);
	Child->SetId(wxID_ANY);
	Child->Show();

	wxBoxSizer * m_MainSizer = new wxBoxSizer(wxHORIZONTAL);

	m_MainSizer->Add(Child, 1, wxEXPAND);

	Frame->Connect(wxID_ANY, wxEVT_CLOSE_WINDOW,
		wxCloseEventHandler(CFrame::OnFloatingPageClosed),
		(wxObject*)0, this);

	// Main sizer
	Frame->SetSizer( m_MainSizer );
	// Minimum frame size
	Frame->SetMinSize(wxSize(200, -1));
	Frame->Show();
	return Frame;
}

// Toolbar
void CFrame::OnDropDownSettingsToolbar(wxAuiToolBarEvent& event)
{
	event.Skip();
	ClearStatusBar();

    if (event.IsDropDownClicked())
    {
		wxAuiToolBar* Tb = static_cast<wxAuiToolBar*>(event.GetEventObject());
		Tb->SetToolSticky(event.GetId(), true);

		// Create the popup menu
		wxMenu menuPopup;

		wxMenuItem* Item =  new wxMenuItem(&menuPopup, IDM_PERSPECTIVES_ADD_PANE, wxT("Add new pane"));
		menuPopup.Append(Item);
		menuPopup.Append(new wxMenuItem(&menuPopup));
		Item =  new wxMenuItem(&menuPopup, IDM_TAB_SPLIT, wxT("Tab split"), wxT(""), wxITEM_CHECK);
		menuPopup.Append(Item);
		Item->Check(m_bTabSplit);
		Item = new wxMenuItem(&menuPopup, IDM_NO_DOCKING, wxT("No docking"), wxT(""), wxITEM_CHECK);
		menuPopup.Append(Item);
		Item->Check(m_bNoDocking);

		// Line up our menu with the button
		wxRect rect = Tb->GetToolRect(event.GetId());
		wxPoint Pt = Tb->ClientToScreen(rect.GetBottomLeft());
		Pt = ScreenToClient(Pt);
		// Show
		PopupMenu(&menuPopup, Pt);
		// Make the button un-stuck again
		if (!m_bEdit) Tb->SetToolSticky(event.GetId(), false);
    }
}
void CFrame::OnDropDownToolbarItem(wxAuiToolBarEvent& event)
{
	event.Skip();
	ClearStatusBar();

    if (event.IsDropDownClicked())
    {
        wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(event.GetEventObject());
        tb->SetToolSticky(event.GetId(), true);

        // create the popup menu
        wxMenu menuPopup;
        wxMenuItem* Item = new wxMenuItem(&menuPopup, IDM_ADD_PERSPECTIVE, wxT("Create new perspective"));
        menuPopup.Append(Item);

		if (Perspectives.size() > 0)
		{
			menuPopup.Append(new wxMenuItem(&menuPopup));
			for (u32 i = 0; i < Perspectives.size(); i++)
			{
				wxMenuItem* mItem = new wxMenuItem(&menuPopup, IDM_PERSPECTIVES_0 + i, wxString::FromAscii(Perspectives.at(i).Name.c_str()), wxT(""), wxITEM_CHECK);
				menuPopup.Append(mItem);
				if (i == ActivePerspective) mItem->Check(true);
			}
		}

        // line up our menu with the button
        wxRect rect = tb->GetToolRect(event.GetId());
        wxPoint pt = tb->ClientToScreen(rect.GetBottomLeft());
        pt = ScreenToClient(pt);
		// show
        PopupMenu(&menuPopup, pt);
        // make sure the button is "un-stuck"
        tb->SetToolSticky(event.GetId(), false);
    }
}
void CFrame::OnToolBar(wxCommandEvent& event)
{	
	ClearStatusBar();

	switch (event.GetId())
	{
		case IDM_SAVE_PERSPECTIVE:
			if (Perspectives.size() == 0)
			{
				wxMessageBox(wxT("Please create a perspective before saving"), wxT("Notice"), wxOK, this);
				return;
			}
			Save();
			if (Perspectives.size() > 0 && ActivePerspective < Perspectives.size())
				this->GetStatusBar()->SetStatusText(wxString::FromAscii(StringFromFormat(
				"Saved %s", Perspectives.at(ActivePerspective).Name.c_str()).c_str()), 0);
			break;
		case IDM_PERSPECTIVES_ADD_PANE:
			AddPane();
			break;
		case IDM_EDIT_PERSPECTIVES:
			m_bEdit = !m_bEdit;
			m_ToolBarAui->SetToolSticky(IDM_EDIT_PERSPECTIVES, m_bEdit);
			TogglePaneStyle(m_bEdit, IDM_EDIT_PERSPECTIVES);			
			break;
	}
}
void CFrame::OnDropDownToolbarSelect(wxCommandEvent& event)
{
	ClearStatusBar();

	switch(event.GetId())
	{
		case IDM_ADD_PERSPECTIVE:
			{
				wxTextEntryDialog dlg(this, wxT("Enter a name for the new perspective:"), wxT("Create new perspective"));
				wxString DefaultValue = wxString::Format(wxT("Perspective %u"), unsigned(Perspectives.size() + 1));
				dlg.SetValue(DefaultValue);
				//if (dlg.ShowModal() != wxID_OK) return;
				bool DlgOk = false; int Return = 0;
				while (!DlgOk)
				{
					Return = dlg.ShowModal();
					if (Return == wxID_CANCEL)
						return;
					else if (dlg.GetValue().Find(wxT(",")) != -1)
					{
						wxMessageBox(wxT("The name can not have the letter ',' in it"), wxT("Notice"), wxOK, this);
						wxString Str = dlg.GetValue();
						Str.Replace(wxT(","), wxT(""), true);
						dlg.SetValue(Str);
					}
					else if (dlg.GetValue().IsSameAs(wxT("")))
					{
						wxMessageBox(wxT("The name can not be empty"), wxT("Notice"), wxOK, this);
						dlg.SetValue(DefaultValue);
					}
					else
						DlgOk = true;
				}
				//wxID_CANCEL

				SPerspectives Tmp;
				Tmp.Name = dlg.GetValue().mb_str();
				Perspectives.push_back(Tmp);
			}
			break;
		case IDM_TAB_SPLIT:
			 m_bTabSplit = event.IsChecked();
			ToggleNotebookStyle(wxAUI_NB_TAB_SPLIT);
			break;
		case IDM_NO_DOCKING:
			m_bNoDocking = event.IsChecked();
			TogglePaneStyle(m_bNoDocking, IDM_NO_DOCKING);
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void CFrame::ResetToolbarStyle()
{
    wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
    for (int i = 0, Count = (int)AllPanes.GetCount(); i < Count; ++i)
    {
        wxAuiPaneInfo& Pane = AllPanes.Item(i);
        if (Pane.window->IsKindOf(CLASSINFO(wxAuiToolBar)))
        {
			//Pane.BestSize(-1, -1);
			Pane.Show();
			// Show all of it
			if (Pane.rect.GetLeft() > this->GetClientSize().GetX() - 50)
				Pane.Position(this->GetClientSize().GetX() - Pane.window->GetClientSize().GetX());
        }
    }
	m_Mgr->Update();
}

void CFrame::TogglePaneStyle(bool On, int EventId)
{
    wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
    for (u32 i = 0; i < AllPanes.GetCount(); ++i)
    {
        wxAuiPaneInfo& Pane = AllPanes.Item(i);
		if (Pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			// Default
			Pane.CloseButton(true);
			Pane.MaximizeButton(true);
			Pane.MinimizeButton(true);
			Pane.PinButton(true);
			Pane.Show();

			switch(EventId)
			{
				case IDM_EDIT_PERSPECTIVES:
					Pane.CaptionVisible(On);
					Pane.Movable(On);
					Pane.Floatable(On);
					Pane.Dockable(On);					
					break;
					/*
				case IDM_NO_DOCKING:				
					Pane.Dockable(!On);
					break;
					*/
			}
			Pane.Dockable(!m_bNoDocking);
		}
    }
	m_Mgr->Update();
}

void CFrame::ToggleNotebookStyle(long Style)
{
    wxAuiPaneInfoArray& AllPanes = m_Mgr->GetAllPanes();
    for (int i = 0, Count = (int)AllPanes.GetCount(); i < Count; ++i)
    {
        wxAuiPaneInfo& Pane = AllPanes.Item(i);
        if (Pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
        {
            wxAuiNotebook* NB = (wxAuiNotebook*)Pane.window;
            NB->SetWindowStyleFlag(NB->GetWindowStyleFlag() ^ Style);
            NB->Refresh();
        }
    }
}
void CFrame::OnSelectPerspective(wxCommandEvent& event)
{
	u32 _Selection = event.GetId() - IDM_PERSPECTIVES_0;
	if (Perspectives.size() <= _Selection) _Selection = 0;
	ActivePerspective = _Selection;
	DoLoadPerspective();
}

void CFrame::ResizeConsole()
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;
		for(u32 j = 0; j <= wxDynamicCast(m_Mgr->GetAllPanes().Item(i).window, wxAuiNotebook)->GetPageCount(); j++)
		{
			if (wxDynamicCast(m_Mgr->GetAllPanes().Item(i).window, wxAuiNotebook)->GetPageText(j).IsSameAs(wxT("Console")))
			{
				#ifdef _WIN32
				// ----------------------------------------------------------
				// Get OS version
				// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
				int wxBorder, Border, LowerBorder, MenuBar, ScrollBar, WidthReduction;
				OSVERSIONINFO osvi;
				ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
				osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
				GetVersionEx(&osvi);
				if (osvi.dwMajorVersion == 6) // Vista (same as 7?)
				{
					wxBorder = 2;
					Border = 4;
					LowerBorder = 6;
					MenuBar = 30; // Including upper border				
					ScrollBar = 19;
				}
				else // XP
				{
					wxBorder = 2;
					Border = 4;
					LowerBorder = 6;
					MenuBar = 30;					
					ScrollBar = 19;
				}
				WidthReduction = 30 - Border;
				// --------------------------------
				// Get the client size
				int X = m_Mgr->GetAllPanes().Item(i).window->GetClientSize().GetX();
				int Y = m_Mgr->GetAllPanes().Item(i).window->GetClientSize().GetY();
				int InternalWidth = X - wxBorder*2 - ScrollBar;
				int InternalHeight = Y - wxBorder*2;
				int WindowWidth = InternalWidth + Border*2;
				int WindowHeight = InternalHeight;
				// Resize buffer
				ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
				//Console->Log(LogTypes::LNOTICE, StringFromFormat("Window WxH:%i %i\n", X, Y).c_str());
				Console->PixelSpace(0,0, InternalWidth,InternalHeight, false);
				// Move the window to hide the border
				MoveWindow(GetConsoleWindow(), -Border-wxBorder,-MenuBar-wxBorder, WindowWidth + 100,WindowHeight, true);
				// Move it to the bottom of the view order so that it doesn't hide the notebook tabs
				// ...
				#endif
			}
		}	
	}
}

void CFrame::SetSimplePaneSize()
{
	wxArrayInt Pane, Size;
	Pane.Add(0); Size.Add(50);
	Pane.Add(1); Size.Add(50);

	int iClientSize = this->GetSize().GetX();
	// Fix the pane sizes
	for (u32 i = 0; i < Pane.size(); i++)
	{
		// Check limits
		Size[i] = Limit(Size[i], 5, 95);
		// Produce pixel width from percentage width
		Size[i] = PercentageToPixels(Size[i], iClientSize);
		// Update size
		m_Mgr->GetPane(wxString::Format(wxT("Pane %i"), Pane[i])).BestSize(Size[i], -1).MinSize(Size[i], -1).MaxSize(Size[i], -1);
	}
	m_Mgr->Update();
	for (u32 i = 0; i < Pane.size(); i++)
	{
		// Remove the size limits
		m_Mgr->GetPane(wxString::Format(wxT("Pane %i"), Pane[i])).MinSize(-1, -1).MaxSize(-1, -1);
	}
}

void CFrame::SetPaneSize()
{
	if (Perspectives.size() <= ActivePerspective) return;
	int iClientX = this->GetSize().GetX(), iClientY = this->GetSize().GetY();

	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			if (!m_Mgr->GetAllPanes().Item(i).IsOk()) return;
			if (Perspectives.at(ActivePerspective).Width.size() <= j || Perspectives.at(ActivePerspective).Height.size() <= j) continue;
			u32 W = Perspectives.at(ActivePerspective).Width.at(j), H = Perspectives.at(ActivePerspective).Height.at(j);
			// Check limits
			W = Limit(W, 5, 95); H = Limit(H, 5, 95);
			// Produce pixel width from percentage width
			W = PercentageToPixels(W, iClientX); H = PercentageToPixels(H, iClientY);
			m_Mgr->GetAllPanes().Item(i).BestSize(W,H).MinSize(W,H).MaxSize(W,H);

			j++;
		}
	}
	m_Mgr->Update();

	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			m_Mgr->GetAllPanes().Item(i).MinSize(-1,-1).MaxSize(-1,-1);
		}
	}
}

void CFrame::ReloadPanes()
{	
	// Keep settings
	bool bConsole = SConfig::GetInstance().m_InterfaceConsole;

	//ListChildren();
	//ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	//Console->Log(LogTypes::LNOTICE, StringFromFormat("ReloadPanes begin: Sound %i\n", FindWindowByName(wxT("Sound"))).c_str());

	if (ActivePerspective >= Perspectives.size()) ActivePerspective = 0;

	// Check that there is a perspective
	if (Perspectives.size() > 0)
	{
		// Check that the perspective was saved once before
		if (Perspectives.at(ActivePerspective).Width.size() == 0) return;

		// Hide to avoid flickering
		HideAllNotebooks(true);
		// Close all pages
		ClosePages();

		CloseAllNotebooks();
		//m_Mgr->Update();

		// Create new panes with notebooks
		for (u32 i = 0; i < Perspectives.at(ActivePerspective).Width.size() - 1; i++)
		{
			m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo().Hide()
			.CaptionVisible(m_bEdit).Dockable(!m_bNoDocking));
		}
		HideAllNotebooks(true);

		// Names
		NamePanes();
		// Perspectives
		m_Mgr->LoadPerspective(Perspectives.at(ActivePerspective).Perspective, false);
		// Reset toolbars
		ResetToolbarStyle();
		// Restore settings
		TogglePaneStyle(m_bNoDocking, IDM_NO_DOCKING);
		TogglePaneStyle(m_bEdit, IDM_EDIT_PERSPECTIVES);
	}
	// Create one pane by default
	else
	{
		m_Mgr->AddPane(CreateEmptyNotebook());
	}

	// Restore settings
	SConfig::GetInstance().m_InterfaceConsole = bConsole;
	// Load GUI settings
	g_pCodeWindow->Load();
	// Open notebook pages
	AddRemoveBlankPage();
	g_pCodeWindow->OpenPages();
	if (SConfig::GetInstance().m_InterfaceLogWindow) DoToggleWindow(IDM_LOGWINDOW, true);
	if (SConfig::GetInstance().m_InterfaceConsole) DoToggleWindow(IDM_CONSOLEWINDOW, true);	

	//Console->Log(LogTypes::LNOTICE, StringFromFormat("ReloadPanes end: Sound %i\n", FindWindowByName(wxT("Sound"))).c_str());
	//ListChildren();
}

void CFrame::DoLoadPerspective()
{
	ReloadPanes();
	// Restore the exact window sizes, which LoadPerspective doesn't always do
	SetPaneSize();
	// Show
	ShowAllNotebooks(true);

	/*
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LNOTICE, StringFromFormat(
		"Loaded: %s (%i panes, %i NBs)\n",
		Perspectives.at(ActivePerspective).Name.c_str(), m_Mgr->GetAllPanes().GetCount(), GetNotebookCount()).c_str());
	*/
}

// Update the local perspectives array
void CFrame::SaveLocal()
{
	Perspectives.clear();
	std::vector<std::string> VPerspectives;
	std::string _Perspectives;

	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);
	ini.Get("Perspectives", "Perspectives", &_Perspectives, "");
	ini.Get("Perspectives", "Active", &ActivePerspective, 5);
	SplitString(_Perspectives, ",", VPerspectives);

	//
	for (u32 i = 0; i < VPerspectives.size(); i++)
	{
		SPerspectives Tmp;		
		std::string _Section, _Perspective, _Width, _Height;
		std::vector<std::string> _SWidth, _SHeight;
		Tmp.Name = VPerspectives.at(i);		
		// Don't save a blank perspective
		if (Tmp.Name == "") continue;
		//if (!ini.Exists(_Section.c_str(), "Width")) continue;

		_Section = StringFromFormat("P - %s", Tmp.Name.c_str());
		ini.Get(_Section.c_str(), "Perspective", &_Perspective, "");
		ini.Get(_Section.c_str(), "Width", &_Width, "");
		ini.Get(_Section.c_str(), "Height", &_Height, "");	

		Tmp.Perspective = wxString::FromAscii(_Perspective.c_str());

		SplitString(_Width, ",", _SWidth);
		SplitString(_Height, ",", _SHeight);
		for (u32 j = 0; j < _SWidth.size(); j++)
		{
			int _Tmp;
			if (TryParseInt(_SWidth.at(j).c_str(), &_Tmp)) Tmp.Width.push_back(_Tmp);				
		}
		for (u32 j = 0; j < _SHeight.size(); j++)
		{
			int _Tmp;
			if (TryParseInt(_SHeight.at(j).c_str(), &_Tmp)) Tmp.Height.push_back(_Tmp);
		}
		Perspectives.push_back(Tmp);	
	}
}
void CFrame::Save()
{
	if (Perspectives.size() == 0) return;
	if (ActivePerspective >= Perspectives.size()) ActivePerspective = 0;

	// Turn off edit before saving
	TogglePaneStyle(false, IDM_EDIT_PERSPECTIVES);
	// Name panes
	NamePanes();

	// Get client size
	int iClientX = this->GetSize().GetX(), iClientY = this->GetSize().GetY();

	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);

	std::string _Section = StringFromFormat("P - %s", Perspectives.at(ActivePerspective).Name.c_str());
	ini.Set(_Section.c_str(), "Perspective", m_Mgr->SavePerspective().mb_str());
	
	std::string SWidth = "", SHeight = "";
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			SWidth += StringFromFormat("%i", PixelsToPercentage(m_Mgr->GetAllPanes().Item(i).window->GetClientSize().GetX(), iClientX));
			SHeight += StringFromFormat("%i", PixelsToPercentage(m_Mgr->GetAllPanes().Item(i).window->GetClientSize().GetY(), iClientY));
			SWidth += ","; SHeight += ",";
		}
	}
	// Remove the ending ","
	SWidth = SWidth.substr(0, SWidth.length()-1); SHeight = SHeight.substr(0, SHeight.length()-1);

	ini.Set(_Section.c_str(), "Width", SWidth.c_str());
	ini.Set(_Section.c_str(), "Height", SHeight.c_str());

	// Save perspective names
	std::string STmp = "";
	for (u32 i = 0; i < Perspectives.size(); i++)
	{
		STmp += Perspectives.at(i).Name + ",";
	}
	STmp = STmp.substr(0, STmp.length()-1);
	ini.Set("Perspectives", "Perspectives", STmp.c_str());
	ini.Set("Perspectives", "Active", ActivePerspective);
	ini.Save(DEBUGGER_CONFIG_FILE);

	// Save notebook affiliations
	g_pCodeWindow->Save();

	// Update the local vector
	SaveLocal();

	/*
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	Console->Log(LogTypes::LNOTICE, StringFromFormat(
		"Saved: %s (%s, %i panes, %i NBs)\n",
		Perspectives.at(ActivePerspective).Name.c_str(), STmp.c_str(), m_Mgr->GetAllPanes().GetCount(), GetNotebookCount()).c_str());
	*/
	
	TogglePaneStyle(m_bEdit, IDM_EDIT_PERSPECTIVES); 
}

void CFrame::NamePanes()
{
	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiToolBar)))
		{
			m_Mgr->GetAllPanes().Item(i).Name(wxString::Format(wxT("Pane %i"), j));
			m_Mgr->GetAllPanes().Item(i).Caption(wxString::Format(wxT("Pane %i"), j));
			j++;
		}
	}
}
void CFrame::AddPane()
{
	m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo()
		.CaptionVisible(m_bEdit).Dockable(!m_bNoDocking));

	NamePanes();
	AddRemoveBlankPage();
	m_Mgr->Update();
}	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

int CFrame::Limit(int i, int Low, int High)
{	
	if (i < Low) return Low;
	if (i > High) return High;
	return i;
}
int CFrame::PercentageToPixels(int Percentage, int Total)
{
	int Pixels = (int)((float)Total * ((float)Percentage / 100.0));
	return Pixels;
}
int CFrame::PixelsToPercentage(int Pixels, int Total)
{
	int Percentage = (int)(((float)Pixels / (float)Total) * 100.0);
	return Percentage;
}

#ifdef _WIN32
wxWindow * CFrame::GetWxWindowHwnd(HWND hWnd)
{
	wxWindow * Win = new wxWindow();
	Win->SetHWND((WXHWND)hWnd);
	Win->AdoptAttributesFromHWND();
	return Win;
}
#endif
wxWindow * CFrame::GetWxWindow(wxString Name)
{
	#ifdef _WIN32
	HWND hWnd = ::FindWindow(NULL, Name.c_str());
	if (hWnd)
	{
		wxWindow * Win = new wxWindow();
		Win->SetHWND((WXHWND)hWnd);
		Win->AdoptAttributesFromHWND();
		return Win;
	}
	else
	#endif
	if (FindWindowByName(Name))
	{
		return FindWindowByName(Name);
	}
	else if (FindWindowByLabel(Name))
	{
		return FindWindowByLabel(Name);
	}
	else if (GetNootebookPage(Name))
	{
		return GetNootebookPage(Name);
	}
	else
		return NULL;
}
wxWindow * CFrame::GetFloatingPage(int Id)
{
	if (this->FindWindowById(Id))
		return this->FindWindowById(Id);
	else
		return NULL;
}
wxWindow * CFrame::GetNootebookPage(wxString Name)
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;	
		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		for(u32 j = 0; j < NB->GetPageCount(); j++)
		{

			if (NB->GetPageText(j).IsSameAs(Name)) return NB->GetPage(j);
		}	
	}
	return NULL;
}
wxWindow * CFrame::GetNootebookPageFromId(wxWindowID Id)
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;	
		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		for(u32 j = 0; j < NB->GetPageCount(); j++)
		{
			if (NB->GetPage(j)->GetId() == Id) return NB->GetPage(j);
		}	
	}
	return NULL;
}
void CFrame::AddRemoveBlankPage()
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;
		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		for(u32 j = 0; j < NB->GetPageCount(); j++)
		{
			if (NB->GetPageText(j).IsSameAs(wxT("<>")) && NB->GetPageCount() > 1) NB->DeletePage(j);
		}	
		if (NB->GetPageCount() == 0) NB->AddPage(CreateEmptyPanel(), wxT("<>"), true);
	}
}
int CFrame::GetNootebookAffiliation(wxString Name)
{
	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;
		wxAuiNotebook * NB = (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		for(u32 k = 0; k < NB->GetPageCount(); k++)
		{
			if (NB->GetPageText(k).IsSameAs(Name)) return j;
		}
		j++;
	}
	return -1;
}
wxString CFrame::WindowNameFromId(int Id)
{
	switch(Id)
	{
		case IDM_LOGWINDOW: return wxT("Log");
		case IDM_CONSOLEWINDOW: return wxT("Console");
		case IDM_CODEWINDOW: return wxT("Code");
		case IDM_REGISTERWINDOW: return wxT("Registers");
		case IDM_BREAKPOINTWINDOW: return wxT("Breakpoints");
		case IDM_MEMORYWINDOW: return wxT("Memory");
		case IDM_JITWINDOW: return wxT("JIT");
		case IDM_SOUNDWINDOW: return wxT("Sound");
		case IDM_VIDEOWINDOW: return wxT("Video");
	}
	return wxT("");
}

// Close all panes with notebooks
void CFrame::CloseAllNotebooks()
{		
	int i = 0;
	while(GetNotebookCount() > 0)
	{
		if (m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			m_Mgr->GetAllPanes().Item(i).DestroyOnClose(true);
			m_Mgr->ClosePane(m_Mgr->GetAllPanes().Item(i));
			//m_Mgr->GetAllPanes().Item(i).window->Hide();
			//m_Mgr->DetachPane(m_Mgr->GetAllPanes().Item(i).window);
			
			i = 0;
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("    %i Pane\n", i).c_str());
		}
		else
		{
			i++;
			//Console->Log(LogTypes::LNOTICE, StringFromFormat("    %i No pane\n", i).c_str());
		}
		
	}
}
int CFrame::GetNotebookCount()
{
	int Ret = 0;
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) Ret++;
	}
	return Ret;
}

wxAuiNotebook * CFrame::GetNotebookFromId(u32 NBId)
{
	for (u32 i = 0, j = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (!m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook))) continue;
		if (j == NBId) return (wxAuiNotebook*)m_Mgr->GetAllPanes().Item(i).window;
		j++;
	}
	return NULL;	
}
void CFrame::ShowAllNotebooks(bool Window)
{
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			if (Window)
				m_Mgr->GetAllPanes().Item(i).Show();
			else
				m_Mgr->GetAllPanes().Item(i).window->Hide();
		}
	}
	m_Mgr->Update();
}
void CFrame::HideAllNotebooks(bool Window)
{	
	for (u32 i = 0; i < m_Mgr->GetAllPanes().GetCount(); i++)
	{
		if (m_Mgr->GetAllPanes().Item(i).window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			if (Window)
				m_Mgr->GetAllPanes().Item(i).Hide();
			else
				m_Mgr->GetAllPanes().Item(i).window->Hide();
		}
	}
	m_Mgr->Update();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////