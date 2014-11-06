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

class CRegisterView;
class wxWindow;

class CRegisterWindow : public wxPanel
{
public:
	CRegisterWindow(wxWindow* parent,
			wxWindowID id = wxID_ANY,
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxTAB_TRAVERSAL | wxNO_BORDER,
			const wxString& name = _("Registers"));

	void NotifyUpdate();

private:
	enum
	{
		ID_GPR = 1002
	};

	CRegisterView* m_GPRGridView;
	void CreateGUIControls();
};
