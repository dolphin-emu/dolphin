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

	void NotifyUpdate();


private:
	DECLARE_EVENT_TABLE();

	enum
	{
		ID_GPR = 1002
	};

	CWatchView* m_GPRGridView;
	void CreateGUIControls();
};
