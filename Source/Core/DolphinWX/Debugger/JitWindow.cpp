// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <disasm.h>        // Bochs
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/listbase.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/window.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/JitWindow.h"

enum
{
	IDM_REFRESH_LIST = 23350,
	IDM_PPC_BOX,
	IDM_X86_BOX,
	IDM_NEXT,
	IDM_PREV,
	IDM_BLOCKLIST,
};

BEGIN_EVENT_TABLE(CJitWindow, wxPanel)
	//EVT_TEXT(IDM_ADDRBOX, CJitWindow::OnAddrBoxChange)
	//EVT_LISTBOX(IDM_SYMBOLLIST, CJitWindow::OnSymbolListChange)
	//EVT_HOST_COMMAND(wxID_ANY, CJitWindow::OnHostMessage)
	EVT_BUTTON(IDM_REFRESH_LIST, CJitWindow::OnRefresh)
END_EVENT_TABLE()

CJitWindow::CJitWindow(wxWindow* parent, wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style, const wxString& name)
: wxPanel(parent, id, pos, size, style, name)
{
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerSplit = new wxBoxSizer(wxHORIZONTAL);
	sizerSplit->Add(ppc_box = new wxTextCtrl(this, IDM_PPC_BOX, "(ppc)",
				wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerSplit->Add(x86_box = new wxTextCtrl(this, IDM_X86_BOX, "(x86)",
				wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerBig->Add(block_list = new JitBlockList(this, IDM_BLOCKLIST,
				wxDefaultPosition, wxSize(100, 140),
				wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING),
				0, wxEXPAND);
	sizerBig->Add(sizerSplit, 2, wxEXPAND);
	// sizerBig->Add(memview, 5, wxEXPAND);
	// sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerBig->Add(button_refresh = new wxButton(this, IDM_REFRESH_LIST, _("&Refresh")));
	// sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_ADDRBOX, ""));
	// sizerRight->Add(new wxButton(this, IDM_SETPC, _("S&et PC")));

	SetSizer(sizerBig);

	sizerSplit->Fit(this);
	sizerBig->Fit(this);
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
	u8 *xDis = new u8[1<<18];
	memset(xDis, 0, 1<<18);

	disassembler x64disasm;
	x64disasm.set_syntax_intel();

	int block_num = jit->GetBlockCache()->GetBlockNumberFromStartAddress(em_address);
	if (block_num < 0)
	{
		for (int i = 0; i < 500; i++)
		{
			block_num = jit->GetBlockCache()->GetBlockNumberFromStartAddress(em_address - 4 * i);
			if (block_num >= 0)
				break;
		}

		if (block_num >= 0)
		{
			JitBlock *block = jit->GetBlockCache()->GetBlock(block_num);
			if (!(block->originalAddress <= em_address &&
						block->originalSize + block->originalAddress >= em_address))
				block_num = -1;
		}

		// Do not merge this "if" with the above - block_num changes inside it.
		if (block_num < 0)
		{
			ppc_box->SetValue(_(StringFromFormat("(non-code address: %08x)", em_address)));
			x86_box->SetValue(_("(no translation)"));
			delete[] xDis;
			return;
		}
	}
	JitBlock *block = jit->GetBlockCache()->GetBlock(block_num);

	// 800031f0
	// == Fill in x86 box

	const u8 *code = (const u8 *)jit->GetBlockCache()->GetCompiledCodeFromBlock(block_num);
	u64 disasmPtr = (u64)code;
	const u8 *end = code + block->codeSize;
	char *sptr = (char*)xDis;

	int num_x86_instructions = 0;
	while ((u8*)disasmPtr < end)
	{
		disasmPtr += x64disasm.disasm64(disasmPtr, disasmPtr, (u8*)disasmPtr, sptr);
		sptr += strlen(sptr);
		*sptr++ = 13;
		*sptr++ = 10;
		num_x86_instructions++;
	}
	x86_box->SetValue(StrToWxStr((char*)xDis));

	// == Fill in ppc box
	u32 ppc_addr = block->originalAddress;
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

	if (analyzer.Analyze(ppc_addr, &code_block, &code_buffer, block->codeSize) != 0xFFFFFFFF)
	{
		sptr = (char*)xDis;
		for (u32 i = 0; i < code_block.m_num_instructions; i++)
		{
			const PPCAnalyst::CodeOp &op = code_buffer.codebuffer[i];
			std::string temp = GekkoDisassembler::Disassemble(op.inst.hex, op.address);
			sptr += sprintf(sptr, "%08x %s\n", op.address, temp.c_str());
		}

		// Add stats to the end of the ppc box since it's generally the shortest.
		sptr += sprintf(sptr, "\n");

		// Add some generic analysis
		if (st.isFirstBlockOfFunction)
			sptr += sprintf(sptr, "(first block of function)\n");
		if (st.isLastBlockOfFunction)
			sptr += sprintf(sptr, "(last block of function)\n");

		sptr += sprintf(sptr, "%i estimated cycles\n", st.numCycles);

		sptr += sprintf(sptr, "Num instr: PPC: %i  x86: %i  (blowup: %i%%)\n",
				code_block.m_num_instructions, num_x86_instructions, 100 * num_x86_instructions / code_block.m_num_instructions - 100);
		sptr += sprintf(sptr, "Num bytes: PPC: %i  x86: %i  (blowup: %i%%)\n",
				code_block.m_num_instructions * 4, block->codeSize, 100 * block->codeSize / (4 * code_block.m_num_instructions) - 100);

		ppc_box->SetValue(StrToWxStr((char*)xDis));
	}
	else
	{
		ppc_box->SetValue(StrToWxStr(StringFromFormat(
						"(non-code address: %08x)", em_address)));
		x86_box->SetValue("---");
	}

	delete[] xDis;
}

void CJitWindow::Update()
{

}

void CJitWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_NOTIFYMAPLOADED:
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
