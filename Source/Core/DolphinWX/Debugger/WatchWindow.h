// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>
#include <wx/aui/framemanager.h>

class CWatchView;

class CWatchWindow : public wxPanel
{
public:
	CWatchWindow(wxWindow* parent,
	             wxWindowID id = wxID_ANY,
	             const wxPoint& pos = wxDefaultPosition,
	             const wxSize& size = wxDefaultSize,
	             long style = wxTAB_TRAVERSAL | wxNO_BORDER,
	             const wxString& name = _("Watch"));
	~CWatchWindow();

	void NotifyUpdate();
	void Event_SaveAll(wxCommandEvent& WXUNUSED(event));
	void SaveAll();
	void Event_LoadAll(wxCommandEvent& WXUNUSED(event));
	void LoadAll();

private:
	wxAuiManager m_mgr;

	// Owned by wx. Deleted implicitly upon destruction.
	CWatchView* m_GPRGridView;
};
