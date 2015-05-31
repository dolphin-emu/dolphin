// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <disasm.h>        // Bochs
#include <sstream>

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/JitWindow.h"
#include "UICommon/Disassembler.h"

CJitWindow::CJitWindow(wxWindow* parent, wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style, const wxString& name)
: wxPanel(parent, id, pos, size, style, name)
{
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerSplit = new wxBoxSizer(wxHORIZONTAL);
	sizerSplit->Add(ppc_box = new wxTextCtrl(this, wxID_ANY, "(ppc)",
				wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerSplit->Add(x86_box = new wxTextCtrl(this, wxID_ANY, "(x86)",
				wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerBig->Add(block_list = new JitBlockList(this, wxID_ANY,
				wxDefaultPosition, wxSize(100, 140),
				wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING),
				0, wxEXPAND);
	sizerBig->Add(sizerSplit, 2, wxEXPAND);

	sizerBig->Add(button_refresh = new wxButton(this, wxID_ANY, _("&Refresh")));
	button_refresh->Bind(wxEVT_BUTTON, &CJitWindow::OnRefresh, this);

	SetSizer(sizerBig);

	sizerSplit->Fit(this);
	sizerBig->Fit(this);

#if defined(_M_X86)
	m_disassembler.reset(GetNewDisassembler("x86"));
#elif defined(_M_ARM_64)
	m_disassembler.reset(GetNewDisassembler("aarch64"));
#elif defined(_M_ARM_32)
	m_disassembler.reset(GetNewDisassembler("armv7"));
#else
	m_disassembler.reset(GetNewDisassembler("UNK"));
#endif
}

void CJitWindow::OnRefresh(wxCommandEvent& /*event*/)
{
	block_list->Update();
}

void CJitWindow::ViewAddr(u32 em_address)
{
	Show(true);
	Compare(em_address);
	SetFocus();
}

void CJitWindow::Compare(u32 em_address)
{
	// Get host side code disassembly
	u32 host_instructions_count = 0;
	u32 host_code_size = 0;
	std::string host_instructions_disasm;
	host_instructions_disasm = DisassembleBlock(m_disassembler.get(), &em_address, &host_instructions_count, &host_code_size);

	x86_box->SetValue(host_instructions_disasm);

	// == Fill in ppc box
	u32 ppc_addr = em_address;
	PPCAnalyst::CodeBuffer code_buffer(32000);
	PPCAnalyst::BlockStats st;
	PPCAnalyst::BlockRegStats gpa;
	PPCAnalyst::BlockRegStats fpa;
	PPCAnalyst::CodeBlock code_block;
	PPCAnalyst::PPCAnalyzer analyzer;
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);

	code_block.m_stats = &st;
	code_block.m_gpa = &gpa;
	code_block.m_fpa = &fpa;

	if (analyzer.Analyze(ppc_addr, &code_block, &code_buffer, 32000) != 0xFFFFFFFF)
	{
		std::ostringstream ppc_disasm;
		for (u32 i = 0; i < code_block.m_num_instructions; i++)
		{
			const PPCAnalyst::CodeOp &op = code_buffer.codebuffer[i];
			std::string opcode = GekkoDisassembler::Disassemble(op.inst.hex, op.address);
			ppc_disasm << std::setfill('0') << std::setw(8) << std::hex << op.address;
			ppc_disasm << " " << opcode << std::endl;
		}

		// Add stats to the end of the ppc box since it's generally the shortest.
		ppc_disasm << std::dec << std::endl;

		// Add some generic analysis
		if (st.isFirstBlockOfFunction)
			ppc_disasm << "(first block of function)" << std::endl;
		if (st.isLastBlockOfFunction)
			ppc_disasm << "(last block of function)" << std::endl;

		ppc_disasm << st.numCycles << " estimated cycles" << std::endl;

		ppc_disasm << "Num instr: PPC: " << code_block.m_num_instructions
		           << " x86: " << host_instructions_count
		           << " (blowup: " << 100 * host_instructions_count / code_block.m_num_instructions - 100
		           << "%)" << std::endl;

		ppc_disasm << "Num bytes: PPC: " << code_block.m_num_instructions * 4
		           << " x86: " << host_code_size
		           << " (blowup: " << 100 * host_code_size / (4 * code_block.m_num_instructions) - 100
		           << "%)" << std::endl;

		ppc_box->SetValue(ppc_disasm.str());
	}
	else
	{
		ppc_box->SetValue(StringFromFormat("(non-code address: %08x)", em_address));
		x86_box->SetValue("---");
	}
}

void CJitWindow::Update()
{

}

void CJitWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_NOTIFY_MAP_LOADED:
			//NotifyMapLoaded();
			break;
	}
}

// JitBlockList
//================

enum
{
	COLUMN_ADDRESS,
	COLUMN_PPCSIZE,
	COLUMN_X86SIZE,
	COLUMN_NAME,
	COLUMN_FLAGS,
	COLUMN_NUMEXEC,
	COLUMN_COST,  // (estimated as x86size * numexec)
};

JitBlockList::JitBlockList(wxWindow* parent, const wxWindowID id,
		const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style) // | wxLC_VIRTUAL)
{
	Init();
}

void JitBlockList::Init()
{
	InsertColumn(COLUMN_ADDRESS, _("Address"));
	InsertColumn(COLUMN_PPCSIZE, _("PPC Size"));
	InsertColumn(COLUMN_X86SIZE, _("x86 Size"));
	InsertColumn(COLUMN_NAME, _("Symbol"));
	InsertColumn(COLUMN_FLAGS, _("Flags"));
	InsertColumn(COLUMN_NUMEXEC, _("NumExec"));
	InsertColumn(COLUMN_COST, _("Cost"));
}

void JitBlockList::Update()
{
}
