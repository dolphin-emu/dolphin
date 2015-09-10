// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <wx/listctrl.h>
#include <wx/panel.h>

#include "Common/CommonTypes.h"
#include "UICommon/Disassembler.h"

class wxButton;
class wxListBox;
class wxTextCtrl;

class JitBlockList : public wxListCtrl
{
public:
	JitBlockList(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
	void Init();
	void Update() override;
};

class CJitWindow : public wxPanel
{
public:
	CJitWindow(wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
		const wxString& name = _("JIT Block Viewer"));

	void ViewAddr(u32 em_address);
	void Update() override;

private:
	void OnRefresh(wxCommandEvent& /*event*/);
	void Compare(u32 em_address);

	JitBlockList* block_list;
	std::unique_ptr<HostDisassembler> m_disassembler;
	wxButton* button_refresh;
	wxTextCtrl* ppc_box;
	wxTextCtrl* x86_box;

	void OnHostMessage(wxCommandEvent& event);
};
