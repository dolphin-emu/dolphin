// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/windowid.h>
#include <wx/aui/framemanager.h>

class CBreakPointView;
class CCodeWindow;
class wxListEvent;
class wxWindow;

class CBreakPointWindow : public wxPanel
{
public:

	CBreakPointWindow(CCodeWindow* _pCodeWindow,
		wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxString& title = _("Breakpoints"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL | wxBORDER_NONE);
	~CBreakPointWindow();

	void NotifyUpdate();

	void OnDelete(wxCommandEvent& WXUNUSED(event));
	void OnClear(wxCommandEvent& WXUNUSED(event));
	void OnAddBreakPoint(wxCommandEvent& WXUNUSED(event));
	void OnAddMemoryCheck(wxCommandEvent& WXUNUSED(event));
	void Event_SaveAll(wxCommandEvent& WXUNUSED(event));
	void SaveAll();
	void Event_LoadAll(wxCommandEvent& WXUNUSED(event));
	void LoadAll();

private:
	wxAuiManager m_mgr;
	CBreakPointView* m_BreakPointListView;
	CCodeWindow* m_pCodeWindow;

	void OnClose(wxCloseEvent& event);
	void OnSelectBP(wxListEvent& event);
};
