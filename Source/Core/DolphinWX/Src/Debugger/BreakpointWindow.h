// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __BREAKPOINTWINDOW_h__
#define __BREAKPOINTWINDOW_h__

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/aui/aui.h>

class CBreakPointView;
class CCodeWindow;

class CBreakPointWindow : public wxPanel
{
public:

	CBreakPointWindow(CCodeWindow* _pCodeWindow,
		wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxString& title = wxT("Breakpoints"),
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
	void LoadAll(wxCommandEvent& WXUNUSED(event));

private:
	DECLARE_EVENT_TABLE();

	wxAuiManager m_mgr;
	CBreakPointView* m_BreakPointListView;
	CCodeWindow* m_pCodeWindow;

	void OnClose(wxCloseEvent& event);
	void OnSelectBP(wxListEvent& event);
};

#endif
