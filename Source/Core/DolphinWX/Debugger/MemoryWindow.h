// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>
#include "Common/CommonTypes.h"

class CMemoryView;
class IniFile;
class wxButton;
class wxCheckBox;
class wxListBox;
class wxSearchCtrl;
class wxTextCtrl;

class CMemoryWindow : public wxPanel
{
public:
	CMemoryWindow(wxWindow* parent,
	              wxWindowID id = wxID_ANY,
	              const wxPoint& pos = wxDefaultPosition,
	              const wxSize& size = wxDefaultSize,
	              long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
	              const wxString& name = _("Memory"));

	void Save(IniFile& _IniFile) const;
	void Load(IniFile& _IniFile);

	void Update() override;
	void NotifyMapLoaded();

	void JumpToAddress(u32 _Address);

private:
	DECLARE_EVENT_TABLE()

	void U8(wxCommandEvent& event);
	void U16(wxCommandEvent& event);
	void U32(wxCommandEvent& event);
	void onSearch(wxCommandEvent& event);
	void onAscii(wxCommandEvent& event);
	void onHex(wxCommandEvent& event);
	void OnSymbolListChange(wxCommandEvent& event);
	void OnAddrBoxChange(wxCommandEvent& event);
	void OnHostMessage(wxCommandEvent& event);
	void SetMemoryValueFromValBox(wxCommandEvent& event);
	void SetMemoryValue(wxCommandEvent& event);
	void OnDumpMemory(wxCommandEvent& event);
	void OnDumpMem2(wxCommandEvent& event);
	void OnDumpFakeVMEM(wxCommandEvent& event);

	wxCheckBox* chk8;
	wxCheckBox* chk16;
	wxCheckBox* chk32;
	wxButton*   btnSearch;
	wxCheckBox* chkAscii;
	wxCheckBox* chkHex;

	CMemoryView* memview;
	wxListBox* symbols;

	wxSearchCtrl* addrbox;
	wxTextCtrl* valbox;
};
