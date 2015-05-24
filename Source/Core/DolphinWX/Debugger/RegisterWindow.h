// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>

class CRegisterView;

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
	CRegisterView* m_GPRGridView;
	void CreateGUIControls();
};
