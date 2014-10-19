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

#include "Common/CommonTypes.h"

class CMemoryView;
class IniFile;
class wxButton;
class wxCheckBox;
class wxListBox;
class wxTextCtrl;
class wxWindow;

class CMemoryWindow : public wxPanel
{
public:
	CMemoryWindow(wxWindow* parent,
	              wxWindowID id = wxID_ANY,
	              const wxPoint& pos = wxDefaultPosition,
	              const wxSize& size = wxDefaultSize,
	              long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
	              const wxString& name = _("Memory"));

	wxCheckBox* chk8;
	wxCheckBox* chk16;
	wxCheckBox* chk32;
	wxButton*   btnSearch;
	wxCheckBox* chkAscii;
	wxCheckBox* chkHex;
	void Save(IniFile& _IniFile) const;
	void Load(IniFile& _IniFile);

	void Update() override;
	void NotifyMapLoaded();

	void JumpToAddress(u32 _Address);

private:
	DECLARE_EVENT_TABLE()

	CMemoryView* memview;
	wxListBox* symbols;

	wxButton* buttonGo;
	wxTextCtrl* addrbox;
	wxTextCtrl* valbox;

	void U8(wxCommandEvent& event);
	void U16(wxCommandEvent& event);
	void U32(wxCommandEvent& event);
	void onSearch(wxCommandEvent& event);
	void onAscii(wxCommandEvent& event);
	void onHex(wxCommandEvent& event);
	void OnSymbolListChange(wxCommandEvent& event);
	void OnCallstackListChange(wxCommandEvent& event);
	void OnAddrBoxChange(wxCommandEvent& event);
	void OnHostMessage(wxCommandEvent& event);
	void SetMemoryValue(wxCommandEvent& event);
	void OnDumpMemory(wxCommandEvent& event);
	void OnDumpMem2(wxCommandEvent& event);
	void OnDumpFakeVMEM(wxCommandEvent& event);
};
