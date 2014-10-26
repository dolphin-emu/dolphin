// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>
#include <wx/aui/framemanager.h>

class CWatchView;
class wxWindow;

class CWatchWindow
	: public wxPanel
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
	DECLARE_EVENT_TABLE();

	wxAuiManager m_mgr;

	enum
	{
		ID_GPR = 1002
	};

	CWatchView* m_GPRGridView;
	void CreateGUIControls();
};
