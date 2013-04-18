// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __REGISTERWINDOW_h__
#define __REGISTERWINDOW_h__

class CRegisterView;
class IniFile;

class CRegisterWindow
	: public wxPanel
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
	DECLARE_EVENT_TABLE();

	enum
	{
		ID_GPR = 1002
	};

	CRegisterView* m_GPRGridView;
	void CreateGUIControls();
};

#endif
